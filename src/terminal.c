/* terminal program */
/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "terminal-intl.h"
#include "terminal.h"
#include "terminal-window.h"
#include "profile-editor.h"
#include <gconf/gconf-client.h>
#include <libgnome/gnome-program.h>
#include <popt.h>
#include <string.h>


/* Settings storage works as follows:
 *   /apps/gnome-terminal/global/
 *   /apps/gnome-terminal/profiles/Foo/
 *
 * It's somewhat tricky to manage the profiles/ dir since we need to track
 * the list of profiles, but gconf doesn't have a concept of notifying that
 * a directory has appeared or disappeared.
 *
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See gnome-terminal 1.x for why.
 *
 * Don't use this code as an example of how to use GConf - it's hugely
 * overcomplicated due to the profiles stuff. Most apps should not
 * have to do scary things of this nature, and should not have
 * a profiles feature.
 *
 */

struct _TerminalApp
{
  GList *windows;
  GtkWidget *new_profile_dialog;
  GtkWidget *new_profile_name_entry;
  GtkWidget *new_profile_base_menu;
  GtkWidget *delete_profiles_dialog;
  GtkWidget *delete_profiles_list;
};

static GConfClient *conf = NULL;
static TerminalApp *app = NULL;

static void         sync_profile_list            (gboolean use_this_list,
                                                  GSList  *this_list);
static TerminalApp* terminal_app_new             (void);

static void profile_list_notify   (GConfClient *client,
                                   guint        cnxn_id,
                                   GConfEntry  *entry,
                                   gpointer     user_data);

static void terminal_app_get_clone_command   (TerminalApp *app,
                                              char      ***argvp);

static void spew_restart_command             (TerminalApp *app);

enum {
  OPTION_COMMAND = 1,
  OPTION_EXECUTE,
  OPTION_WINDOW_WITH_PROFILE,
  OPTION_TAB_WITH_PROFILE,
  OPTION_WINDOW_WITH_PROFILE_ID,
  OPTION_TAB_WITH_PROFILE_ID,
  OPTION_SHOW_MENUBAR,
  OPTION_HIDE_MENUBAR,
  OPTION_GEOMETRY,
  OPTION_LAST
};  

struct poptOption options[] = {
  { 
    NULL, 
    '\0', 
    POPT_ARG_INCLUDE_TABLE, 
    poptHelpOptions,
    0, 
    N_("Help options"), 
    NULL 
  },
  {
    "command",
    'e',
    POPT_ARG_STRING,
    NULL,
    OPTION_COMMAND,
    N_("Execute the argument to this option inside the terminal."),
    NULL
  },
  {
    "execute",
    'x',
    POPT_ARG_NONE,
    NULL,
    OPTION_EXECUTE,
    N_("Execute the remainder of the command line inside the terminal."),
    NULL
  },
  {
    "window-with-profile",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_WINDOW_WITH_PROFILE,
    N_("Open a new window containing a tab with the given profile. More than one of these options can be provided."),
    N_("PROFILENAME")
  },
  {
    "tab-with-profile",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_TAB_WITH_PROFILE,
    N_("Open a new tab in the last-opened window with the given profile. More than one of these options can be provided."),
    N_("PROFILENAME")
  },
  {
    "window-with-profile-internal-id",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_WINDOW_WITH_PROFILE,
    N_("Open a new window containing a tab with the given profile ID. Used internally to save sessions."),
    N_("PROFILEID")
  },
  {
    "tab-with-profile-internal-id",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_TAB_WITH_PROFILE,
    N_("Open a new tab in the last-opened window with the given profile ID. Used internally to save sessions."),
    N_("PROFILEID")
  },
  {
    "show-menubar",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_SHOW_MENUBAR,
    N_("Turn on the menubar for the last-specified window; applies to only one window; can be specified once for each window you create from the command line."),
    NULL
  },
  {
    "hide-menubar",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_HIDE_MENUBAR,
    N_("Turn off the menubar for the last-specified window; applies to only one window; can be specified once for each window you create from the command line."),
    NULL
  },
  {
    "geometry",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_GEOMETRY,
    N_("X geometry specification (see \"X\" man page), can be specified once per window to be opened."),
    N_("GEOMETRY")
  },
  {
    NULL,
    '\0',
    0,
    NULL,
    0,
    NULL,
    NULL
  }
};

typedef struct
{
  char *profile;
  gboolean profile_is_id;
  char **exec_argv;
} InitialTab;

typedef struct
{
  GList *tabs; /* list of InitialTab */

  gboolean force_menubar_state;
  gboolean menubar_state;

  char *geometry;
  
} InitialWindow;

static InitialTab*
initial_tab_new (const char *profile,
                 gboolean    is_id)
{
  InitialTab *it;

  it = g_new (InitialTab, 1);

  it->profile = g_strdup (profile);
  it->profile_is_id = is_id;
  it->exec_argv = NULL;
  
  return it;
}

static void
initial_tab_free (InitialTab *it)
{
  g_free (it->profile);
  g_strfreev (it->exec_argv);
  g_free (it);
}

static InitialWindow*
initial_window_new (const char *profile,
                    gboolean    is_id)
{
  InitialWindow *iw;
  
  iw = g_new (InitialWindow, 1);
  
  iw->tabs = g_list_prepend (NULL, initial_tab_new (profile, is_id));
  iw->force_menubar_state = FALSE;
  iw->menubar_state = FALSE;
  iw->geometry = NULL;
  
  return iw;
}

static void
initial_window_free (InitialWindow *iw)
{
  g_list_foreach (iw->tabs, (GFunc) initial_tab_free, NULL);
  g_list_free (iw->tabs);
  g_free (iw->geometry);
  g_free (iw);
}

static InitialTab*
ensure_top_tab (GList **initial_windows_p)
{
  InitialWindow *iw;
  InitialTab *it;
  
  if (*initial_windows_p == NULL)
    {
      iw = initial_window_new (NULL, FALSE);
      *initial_windows_p = g_list_append (*initial_windows_p, iw);
    }
  else
    {
      iw = g_list_last (*initial_windows_p)->data;
    }

  g_assert (iw->tabs);
  
  it = g_list_last (iw->tabs)->data;

  return it;
}

static GnomeModuleInfo module_info = {
  PACKAGE, VERSION, N_("Terminal"),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

int
main (int argc, char **argv)
{
  GError *err;
  poptContext ctx;
  int next_opt;
  GList *initial_windows = NULL;
  GList *tmp;
  gboolean default_window_menubar_forced = FALSE;
  gboolean default_window_menubar_state = FALSE;
  int i;
  char **post_execute_args;
  const char **args;
  char *default_geometry = NULL;
  
  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  
  gtk_init (&argc, &argv);
  
  gnome_program_init (PACKAGE, VERSION,
                      &module_info,
                      1, /* don't give it any args for now since option parsing doesn't go
                          * through here
                          */
                      argv,
                      GNOME_PARAM_APP_PREFIX, TERM_PREFIX,
                      GNOME_PARAM_APP_SYSCONFDIR, TERM_SYSCONFDIR,
                      GNOME_PARAM_APP_DATADIR, TERM_DATADIR,
                      GNOME_PARAM_APP_LIBDIR, TERM_LIBDIR,
                      NULL); 
  
  /* pre-scan for -x and --execute options (code from old gnome-terminal) */
  post_execute_args = NULL;
  i = 1;
  while (i < argc)
    {
      if (strcmp (argv[i], "-x") == 0 ||
          strcmp (argv[i], "--execute") == 0)
        {
          int last;
          int j;

          ++i;
          last = i;
          if (i == argc)
            break; /* we'll complain about this later. */
          
          post_execute_args = g_new0 (char*, argc - i + 1);
          j = 0;
          while (i < argc)
            {
              post_execute_args[j] = g_strdup (argv[i]);

              i++; 
              j++;
            }
          post_execute_args[j] = NULL;

          /* strip the args we used up, also ends the loop since i >= last */
          argc = last;
        }
      
      ++i;
    }
  
  ctx = poptGetContext (PACKAGE, argc, (const char **) argv, options, 0);

  poptReadDefaultConfig (ctx, TRUE);

  while ((next_opt = poptGetNextOpt (ctx)) > 0)
    {
      switch (next_opt)
        {
        case OPTION_COMMAND:
          {
            const char *arg;
            GError *err;
            InitialTab *it;
            char **exec_argv;
            
            arg = poptGetOptArg (ctx);

            if (arg == NULL)
              {
                g_printerr (_("Option --command/-e requires specifying the command to run\n"));
                return 1;
              }
            
            err = NULL;
            exec_argv = NULL;
            if (!g_shell_parse_argv (arg, NULL, &exec_argv, &err))
              {
                g_printerr (_("Argument to --command/-e is not a valid command: %s\n"),
                            err->message);
                g_error_free (err);
                return 1;
              }

            it = ensure_top_tab (&initial_windows);

            if (it->exec_argv != NULL)
              {
                g_printerr (_("--command/-e/--execute/-x specified more than once for the same window or tab\n"));
                return 1;
              }

            it->exec_argv = exec_argv;
          }
          break;

        case OPTION_EXECUTE:
          {
            InitialTab *it;
            
            if (post_execute_args == NULL)
              {
                g_printerr (_("Option --execute/-x requires specifying the command to run on the rest of the command line\n"));
                return 1;
              }

            it = ensure_top_tab (&initial_windows);

            if (it->exec_argv != NULL)
              {
                g_printerr (_("--command/-e/--execute/-x specified more than once for the same window or tab\n"));
                return 1;
              }

            it->exec_argv = post_execute_args;
            post_execute_args = NULL;
          }
          break;
          
        case OPTION_WINDOW_WITH_PROFILE:
        case OPTION_WINDOW_WITH_PROFILE_ID:
          {
            InitialWindow *iw;
            const char *prof;

            prof = poptGetOptArg (ctx);

            if (prof == NULL)
              {
                g_printerr (_("Option --window-with-profile requires an argument specifying what profile to use\n"));
                return 1;
              }
            
            iw = initial_window_new (prof,
                                     next_opt == OPTION_WINDOW_WITH_PROFILE_ID);
          
            initial_windows = g_list_append (initial_windows, iw);
          }
          break;

        case OPTION_TAB_WITH_PROFILE:
        case OPTION_TAB_WITH_PROFILE_ID:
          {
            InitialWindow *iw;
            const char *prof;

            prof = poptGetOptArg (ctx);

            if (prof == NULL)
              {
                g_printerr (_("Option --tab-with-profile requires an argument specifying what profile to use\n"));
                return 1;
              }
            
            if (initial_windows)
              {
                iw = g_list_last (initial_windows)->data;
                iw->tabs = g_list_append (iw->tabs,
                                          initial_tab_new (prof,
                                                           next_opt == OPTION_TAB_WITH_PROFILE_ID));
              }
            else
              {
                iw = initial_window_new (prof,
                                         next_opt == OPTION_TAB_WITH_PROFILE_ID);
                initial_windows = g_list_append (initial_windows, iw);
              }
          }
          break;
          
        case OPTION_SHOW_MENUBAR:
          {
            InitialWindow *iw;
            
            if (initial_windows)
              {
                iw = g_list_last (initial_windows)->data;

                if (iw->force_menubar_state)
                  {
                    g_printerr (_("--show-menubar option given twice for the same window\n"));

                    return 1;
                  }
                
                iw->force_menubar_state = TRUE;
                iw->menubar_state = TRUE;
              }
            else
              {
                default_window_menubar_forced = TRUE;
                default_window_menubar_state = TRUE;
              }
          }
          break;
          
        case OPTION_HIDE_MENUBAR:
          {
            InitialWindow *iw;
            
            if (initial_windows)
              {
                iw = g_list_last (initial_windows)->data;

                if (iw->force_menubar_state)
                  {
                    g_printerr (_("--show-menubar option given twice for the same window\n"));

                    return 1;
                  }
                
                iw->force_menubar_state = TRUE;
                iw->menubar_state = FALSE;                
              }
            else
              {
                default_window_menubar_forced = TRUE;
                default_window_menubar_state = FALSE;
              }
          }
          break;

        case OPTION_GEOMETRY:
          {
            InitialWindow *iw;
            const char *geometry;

            geometry = poptGetOptArg (ctx);

            if (geometry == NULL)
              {
                g_printerr (_("Option --geometry requires an argument giving the geometry\n"));
                return 1;
              }
            
            if (initial_windows)
              {
                iw = g_list_last (initial_windows)->data;
                if (iw->geometry)
                  {
                    g_printerr (_("Two geometries given for one window\n"));
                    return 1;
                  }

                iw->geometry = g_strdup (geometry);
              }
            else
              {
                if (default_geometry)
                  {
                    g_printerr (_("Two geometries given for one window\n"));
                    return 1;
                  }
                else
                  {
                    default_geometry = g_strdup (geometry);
                  }
              }
          }
          break;
          
        case OPTION_LAST:          
        default:
          g_assert_not_reached ();
          break;
        }
    }
  
  if (next_opt != -1)
    {
      g_printerr (_("Error on option %s: %s.\nRun '%s --help' to see a full list of available command line options.\n"),
                  poptBadOption (ctx, 0),
                  poptStrerror (next_opt),
                  argv[0]);
      return 1;
    }

  args = poptGetArgs (ctx);
  if (args)
    {
      g_printerr (_("Invalid argument: \"%s\"\n"),
                  *args);
      return 1;
    }
  
  poptFreeContext (ctx);

  g_assert (post_execute_args == NULL);

  conf = gconf_client_get_default ();

  err = NULL;  
  gconf_client_add_dir (conf, CONF_PREFIX,
                        GCONF_CLIENT_PRELOAD_RECURSIVE,
                        &err);
  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  CONF_PREFIX, err->message);
      g_error_free (err);
    }
  
  app = terminal_app_new ();

  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/profile_list",
                           profile_list_notify,
                           app,
                           NULL, &err);

  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal profile list changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }  

  terminal_profile_setup_default (conf);
  sync_profile_list (FALSE, NULL);

  tmp = initial_windows;
  while (tmp != NULL)
    {
      TerminalProfile *profile;
      GList *tmp2;
      TerminalWindow *current_window;
      
      InitialWindow *iw = tmp->data;

      g_assert (iw->tabs);

      current_window = NULL;
      tmp2 = iw->tabs;
      while (tmp2 != NULL)
        {
          InitialTab *it = tmp2->data;

          profile = NULL;
          if (it->profile)
            {
              if (it->profile_is_id)
                profile = terminal_profile_lookup (it->profile);
              else
                profile = terminal_profile_lookup_by_visible_name (it->profile);
            }
          
          if (profile == NULL)
            {
              if (it->profile)
                g_printerr (_("No such profile '%s', using default profile"),
                            it->profile);
              profile = terminal_profile_lookup (DEFAULT_PROFILE);
            }
          
          g_assert (profile);

          if (tmp2 == iw->tabs)
            {
              terminal_app_new_terminal (app,
                                         profile,
                                         NULL,
                                         iw->force_menubar_state,
                                         iw->menubar_state,
                                         it->exec_argv,
                                         iw->geometry);

              current_window = g_list_last (app->windows)->data;
            }
          else
            {
              terminal_app_new_terminal (app,
                                         profile,
                                         current_window,
                                         FALSE, FALSE,
                                         it->exec_argv,
                                         NULL);
            }
          
          tmp2 = tmp2->next;
        }
      
      tmp = tmp->next;
    }

  g_list_foreach (initial_windows, (GFunc) initial_window_free, NULL);
  g_list_free (initial_windows);

  if (app->windows == NULL)
    {
      /* Open a default terminal */      

      terminal_app_new_terminal (app,
                                 terminal_profile_lookup (DEFAULT_PROFILE),
                                 NULL,
                                 default_window_menubar_forced,
                                 default_window_menubar_state,
                                 NULL,
                                 default_geometry);

      g_free (default_geometry);
    }
  else
    {
      if (default_geometry)
        {
          g_printerr (_("--geometry given prior to options that create a new window, must be given after\n"));
          return 1;
        }
    }
  
  gtk_main ();

  spew_restart_command (app);
  
  return 0;
}

static TerminalApp*
terminal_app_new (void)
{
  TerminalApp* app;

  app = g_new (TerminalApp, 1);

  app->windows = NULL;
  app->new_profile_dialog = NULL;
  app->new_profile_name_entry = NULL;
  app->new_profile_base_menu = NULL;

  app->delete_profiles_dialog = NULL;
  app->delete_profiles_list = NULL;
  
  return app;
}

static void
terminal_window_destroyed (TerminalWindow *window,
                           TerminalApp    *app)
{
  g_return_if_fail (g_list_find (app->windows, window));
  
  app->windows = g_list_remove (app->windows, window);
  g_object_unref (G_OBJECT (window));

  if (app->windows == NULL)
    gtk_main_quit ();

  spew_restart_command (app);
}

void
terminal_app_new_terminal (TerminalApp     *app,
                           TerminalProfile *profile,
                           TerminalWindow  *window,
                           gboolean         force_menubar_state,
                           gboolean         forced_menubar_state,
                           char           **override_command,
                           const char      *geometry)
{
  TerminalScreen *screen;

  g_return_if_fail (profile);
  
  if (window == NULL)
    {
      window = terminal_window_new ();
      g_object_ref (G_OBJECT (window));

      g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (terminal_window_destroyed),
                        app);
      
      app->windows = g_list_append (app->windows, window);
    }

  if (force_menubar_state)
    {
      terminal_window_set_menubar_visible (window, forced_menubar_state);
    }
  
  screen = terminal_screen_new ();
  
  terminal_screen_set_profile (screen, profile);

  if (override_command)    
    terminal_screen_set_override_command (screen, override_command);
  
  terminal_window_add_screen (window, screen);

  g_object_unref (G_OBJECT (screen));

  terminal_window_set_active (window, screen);

  if (geometry)
    {
      if (!gtk_window_parse_geometry (GTK_WINDOW (window),
                                      geometry))
        g_printerr (_("Invalid geometry string \"%s\"\n"),
                    geometry);
    }
  
  gtk_window_present (GTK_WINDOW (window));

  terminal_screen_launch_child (screen);

  spew_restart_command (app);
}

static GList*
find_profile_link (GList      *profiles,
                   const char *name)
{
  GList *tmp;

  tmp = profiles;
  while (tmp != NULL)
    {
      if (strcmp (terminal_profile_get_name (TERMINAL_PROFILE (tmp->data)),
                  name) == 0)
        return tmp;
      
      tmp = tmp->next;
    }

  return NULL;
}

static void
sync_profile_list (gboolean use_this_list,
                   GSList  *this_list)
{
  GList *known;
  GSList *updated;
  GList *tmp_list;
  GSList *tmp_slist;
  GError *err;
  
  known = terminal_profile_get_list ();

  if (use_this_list)
    {
      updated = g_slist_copy (this_list);
    }
  else
    {
      err = NULL;
      updated = gconf_client_get_list (conf,
                                       CONF_GLOBAL_PREFIX"/profile_list",
                                       GCONF_VALUE_STRING,
                                       &err);
      if (err)
        {
          g_printerr (_("There was an error getting the list of terminal profiles. (%s)\n"),
                      err->message);
          g_error_free (err);
        }
    }

  /* Add any new ones */
  tmp_slist = updated;
  while (tmp_slist != NULL)
    {
      GList *link;
      
      link = find_profile_link (known, tmp_slist->data);
      
      if (link)
        {
          /* make known point to profiles we didn't find in the list */
          known = g_list_delete_link (known, link);
        }
      else
        {
          TerminalProfile *profile;
          
          profile = terminal_profile_new (tmp_slist->data, conf);

          terminal_profile_update (profile);
        }

      tmp_slist = tmp_slist->next;
    }

  g_slist_free (updated);

  /* Forget no-longer-existing profiles */
  tmp_list = known;
  while (tmp_list != NULL)
    {
      TerminalProfile *forgotten;

      forgotten = TERMINAL_PROFILE (tmp_list->data);

      if (strcmp (terminal_profile_get_name (forgotten), DEFAULT_PROFILE) != 0)
        terminal_profile_forget (forgotten);
      
      tmp_list = tmp_list->next;
    }

  g_list_free (known);

  spew_restart_command (app);
}

static void
profile_list_notify (GConfClient *client,
                     guint        cnxn_id,
                     GConfEntry  *entry,
                     gpointer     user_data)
{
  GConfValue *val;
  GSList *value_list;
  GSList *string_list;
  GSList *tmp;
  
  val = gconf_entry_get_value (entry);

  if (val == NULL ||
      val->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (val) != GCONF_VALUE_STRING)
    value_list = NULL;
  else
    value_list = gconf_value_get_list (val);

  string_list = NULL;
  tmp = value_list;
  while (tmp != NULL)
    {
      string_list = g_slist_prepend (string_list,
                                     g_strdup (gconf_value_get_string ((GConfValue*)tmp->data)));

      tmp = tmp->next;
    }

  string_list = g_slist_reverse (string_list);
  
  sync_profile_list (TRUE, string_list);

  g_slist_foreach (string_list, (GFunc) g_free, NULL);
  g_slist_free (string_list);
}

TerminalApp*
terminal_app_get (void)
{
  return app;
}

void
terminal_app_edit_profile (TerminalApp     *app,
                           TerminalProfile *profile,
                           TerminalWindow  *transient_parent)
{
  terminal_profile_edit (profile, GTK_WINDOW (transient_parent));
}

enum
{
  RESPONSE_CREATE,
  RESPONSE_CANCEL,
  RESPONSE_DELETE
};

static void
profile_set_index (TerminalProfile *profile,
                   int              index)
{
  g_object_set_data (G_OBJECT (profile),
                     "new-profile-dialog-index",
                     GINT_TO_POINTER (index));
}

static int
profile_get_index (TerminalProfile *profile)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (profile),
                                             "new-profile-dialog-index"));
}

static void
new_profile_response_callback (GtkWidget *new_profile_dialog,
                               int        response_id,
                               TerminalApp *app)
{
  if (response_id == RESPONSE_CREATE)
    {
      char *name = NULL;
      TerminalProfile *base_profile = NULL;
      char *bad_name_message = NULL;
      int index;
      GList *profiles = NULL;
      GList *tmp;
      GtkWindow *transient_parent;
      
      name = gtk_editable_get_chars (GTK_EDITABLE (app->new_profile_name_entry),
                                     0, -1);
      
      if (*name == '\0')
        bad_name_message = g_strdup (_("You have to name your profile."));

      index = gtk_option_menu_get_history (GTK_OPTION_MENU (app->new_profile_base_menu));
      
      profiles = terminal_profile_get_list ();
      tmp = profiles;
      while (tmp != NULL)
        {
          TerminalProfile *profile = tmp->data;

          if (strcmp (name, terminal_profile_get_visible_name (profile)) == 0)
            {
              bad_name_message = g_strdup_printf (_("You already have a profile called \"%s\""),
                                                  name);
            }
          
          if (index == profile_get_index (profile))
            {
              base_profile = profile;
              break;
            }
          
          tmp = tmp->next;
        }

      if (bad_name_message)
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (new_profile_dialog),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           bad_name_message);
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show_all (dialog);

          goto cleanup;
        }

      if (base_profile == NULL)
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (new_profile_dialog),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("The profile you selected as a base for your new profile no longer exists"));
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show_all (dialog);
          
          goto cleanup;
        }

      transient_parent =
        gtk_window_get_transient_for (GTK_WINDOW (new_profile_dialog));
      
      gtk_widget_destroy (new_profile_dialog);
      
      terminal_profile_create (base_profile, name,
                               transient_parent);

    cleanup:
      g_list_free (profiles);
      g_free (bad_name_message);
      g_free (name);
    }
  else
    {
      gtk_widget_destroy (new_profile_dialog);
    }
}

static void
new_profile_destroyed_callback (GtkWidget   *new_profile_dialog,
                                TerminalApp *app)
{
  app->new_profile_dialog = NULL;
  app->new_profile_name_entry = NULL;
  app->new_profile_base_menu = NULL;
}

void
terminal_app_new_profile (TerminalApp     *app,
                          TerminalProfile *default_base_profile,
                          TerminalWindow  *transient_parent)
{
  TerminalWindow *old_transient_parent;

  if (app->new_profile_dialog == NULL)
    {
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *entry;
      GtkWidget *label;
      GtkWidget *option_menu;
      GtkWidget *menu;
      GtkWidget *mi;
      GList *profiles;
      GList *tmp;
      int i;
      int default_history;
      GtkSizeGroup *size_group;
      
      old_transient_parent = NULL;      
      
      app->new_profile_dialog =
        gtk_dialog_new_with_buttons (_("New terminal profile"),
                                     NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_CANCEL,
                                     RESPONSE_CANCEL,
                                     _("C_reate"),
                                     RESPONSE_CREATE,
                                     NULL);
      g_signal_connect (G_OBJECT (app->new_profile_dialog),
                        "response",
                        G_CALLBACK (new_profile_response_callback),
                        app);

      g_signal_connect (G_OBJECT (app->new_profile_dialog),
                        "destroy",
                        G_CALLBACK (new_profile_destroyed_callback),
                        app);

      gtk_window_set_resizable (GTK_WINDOW (app->new_profile_dialog),
                                FALSE);
      
#define PADDING 5
      
      vbox = gtk_vbox_new (FALSE, PADDING);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), PADDING);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (app->new_profile_dialog)->vbox),
                          vbox, TRUE, TRUE, 0);
      
      hbox = gtk_hbox_new (FALSE, PADDING);

      label = gtk_label_new_with_mnemonic (_("Profile _Name:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      entry = gtk_entry_new ();
      app->new_profile_name_entry = entry;

      gtk_entry_set_width_chars (GTK_ENTRY (entry), 14);
      gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
      
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
      
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);      
      
      hbox = gtk_hbox_new (FALSE, PADDING);

      label = gtk_label_new_with_mnemonic (_("_Base new profile on:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      option_menu = gtk_option_menu_new ();
      app->new_profile_base_menu = option_menu;

      gtk_label_set_mnemonic_widget (GTK_LABEL (label), option_menu);
      
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), option_menu, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      menu = gtk_menu_new ();

      profiles = terminal_profile_get_list ();

      default_history = 0;
      i = 0;
      tmp = profiles;
      while (tmp != NULL)
        {
          TerminalProfile *profile = tmp->data;

          mi = gtk_menu_item_new_with_label (terminal_profile_get_visible_name (profile));

          gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                                 mi);

          profile_set_index (profile, i);

          if (profile == default_base_profile)
            default_history = i;
          
          ++i;
          tmp = tmp->next;
        }

      gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
      gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), default_history);
      
      g_list_free (profiles);

      gtk_widget_grab_focus (app->new_profile_name_entry);
      gtk_dialog_set_default_response (GTK_DIALOG (app->new_profile_dialog),
                                       RESPONSE_CREATE);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      gtk_size_group_add_widget (size_group, entry);
      gtk_size_group_add_widget (size_group, option_menu);
      g_object_unref (G_OBJECT (size_group));
    }
  else 
    {
      old_transient_parent = TERMINAL_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (app->new_profile_dialog)));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
                                    GTK_WINDOW (transient_parent));
      gtk_widget_hide (app->new_profile_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->new_profile_dialog);
  gtk_window_present (GTK_WINDOW (app->new_profile_dialog));
}


enum
{
  COLUMN_NAME,
  COLUMN_PROFILE_OBJECT,
  COLUMN_LAST
};

static GtkWidget*
create_profile_list (void)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkListStore *model;
  GtkTreeIter iter;
  GList *tmp;
  GList *profiles;
  GtkTreePath *path;
  
  model = gtk_list_store_new (COLUMN_LAST,
                              G_TYPE_STRING,
                              G_TYPE_OBJECT);
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));

  g_object_unref (G_OBJECT (model));
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_MULTIPLE);

  profiles = terminal_profile_get_list ();
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

      if (strcmp (terminal_profile_get_name (profile), DEFAULT_PROFILE) != 0)
        {
          gtk_list_store_append (model, &iter);
          
          /* We are assuming the list store will hold a reference to
           * the profile object, otherwise we would be in danger of disappearing
           * profiles.
           */
          gtk_list_store_set (model,
                              &iter,
                              COLUMN_NAME, terminal_profile_get_visible_name (profile),
                              COLUMN_PROFILE_OBJECT, profile,
                              -1);
        }
      
      tmp = tmp->next;
    }
  
  cell = gtk_cell_renderer_text_new ();

  g_object_set (G_OBJECT (cell),
                "xpad", 2,
                NULL);
  
  column = gtk_tree_view_column_new_with_attributes (NULL,
						     cell,
						     "text", COLUMN_NAME,
						     NULL);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
			       GTK_TREE_VIEW_COLUMN (column));

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

  /* Select first row */
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);
  gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)), path);
  gtk_tree_path_free (path);
  
  return tree_view;
}

static void
list_selected_profiles_func (GtkTreeModel      *model,
                             GtkTreePath       *path,
                             GtkTreeIter       *iter,
                             gpointer           data)
{
  GList **list = data;
  TerminalProfile *profile = NULL;

  gtk_tree_model_get (model,
                      iter,
                      COLUMN_PROFILE_OBJECT,
                      &profile,
                      -1);

  *list = g_list_prepend (*list, profile);
}


static void
delete_profiles_response_callback (GtkWidget   *delete_profiles_dialog,
                                   int          response_id,
                                   TerminalApp *app)
{
  if (response_id == RESPONSE_DELETE)
    {
      GtkTreeSelection *selection;
      GList *deleted_profiles;
      GtkWindow *transient_parent;
      
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->delete_profiles_list));

      deleted_profiles = NULL;
      gtk_tree_selection_selected_foreach (selection,
                                           list_selected_profiles_func,
                                           &deleted_profiles);

      if (deleted_profiles == NULL)
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (delete_profiles_dialog),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("You must select one or more profiles to delete"));
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show_all (dialog);

          return;
        }
      
      transient_parent =
        gtk_window_get_transient_for (GTK_WINDOW (delete_profiles_dialog));

      gtk_widget_destroy (delete_profiles_dialog);      

      terminal_profile_delete_list (conf, deleted_profiles, transient_parent);

      g_list_foreach (deleted_profiles, (GFunc) g_object_unref, NULL);
      g_list_free (deleted_profiles);
    }
  else
    {
      gtk_widget_destroy (delete_profiles_dialog);
    }
}

static void
delete_profiles_destroyed_callback (GtkWidget   *delete_profiles_dialog,
                                    TerminalApp *app)
{
  app->delete_profiles_dialog = NULL;
  app->delete_profiles_list = NULL;
}

void
terminal_app_delete_profiles (TerminalApp     *app,
                              TerminalWindow  *transient_parent)
{
  TerminalWindow *old_transient_parent;

  if (app->delete_profiles_dialog == NULL)
    {
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *sw;
      GtkRequisition req;
      
      old_transient_parent = NULL;      
      
      app->delete_profiles_dialog =
        gtk_dialog_new_with_buttons (_("Deleting terminal profiles"),
                                     NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_CANCEL,
                                     RESPONSE_CANCEL,
                                     _("_Delete"),
                                     RESPONSE_DELETE,
                                     NULL);
      g_signal_connect (G_OBJECT (app->delete_profiles_dialog),
                        "response",
                        G_CALLBACK (delete_profiles_response_callback),
                        app);

      g_signal_connect (G_OBJECT (app->delete_profiles_dialog),
                        "destroy",
                        G_CALLBACK (delete_profiles_destroyed_callback),
                        app);
      
#define PADDING 5
      
      vbox = gtk_vbox_new (FALSE, PADDING);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), PADDING);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (app->delete_profiles_dialog)->vbox),
                          vbox, TRUE, TRUE, 0);

      label = gtk_label_new_with_mnemonic (_("_Select a profile or profiles to delete:"));
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      app->delete_profiles_list = create_profile_list ();

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					   GTK_SHADOW_IN);
      
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

      gtk_container_add (GTK_CONTAINER (sw), app->delete_profiles_list);      
      
      gtk_dialog_set_default_response (GTK_DIALOG (app->delete_profiles_dialog),
                                       RESPONSE_CREATE);

      gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                     app->delete_profiles_list);
      
      /* Set default size of profile list */
      gtk_window_set_geometry_hints (GTK_WINDOW (app->delete_profiles_dialog),
                                     app->delete_profiles_list,
                                     NULL, 0);

      /* Incremental reflow makes this a bit useless, I guess. */
      gtk_widget_size_request (app->delete_profiles_list, &req);

      gtk_window_set_default_size (GTK_WINDOW (app->delete_profiles_dialog),
                                   MIN (req.width + 40, 450),
                                   MIN (req.height + 40, 400));

      gtk_widget_grab_focus (app->delete_profiles_list);
    }
  else 
    {
      old_transient_parent = TERMINAL_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (app->delete_profiles_dialog)));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->delete_profiles_dialog),
                                    GTK_WINDOW (transient_parent));
      gtk_widget_hide (app->delete_profiles_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->delete_profiles_dialog);
  gtk_window_present (GTK_WINDOW (app->delete_profiles_dialog));
}

static void
terminal_app_get_clone_command (TerminalApp *app,
                                char      ***argvp)
{
  int n_windows;
  int n_tabs;
  GList *tmp;
  int argc;
  char **argv;
  int i;
  
  n_windows = g_list_length (app->windows);

  n_tabs = 0;
  tmp = app->windows;
  while (tmp != NULL)
    {
      GList *tabs = terminal_window_list_screens (tmp->data);

      n_tabs += g_list_length (tabs);

      g_list_free (tabs);
      
      tmp = tmp->next;
    }

  argc = 1; /* argv[0] */
  
  argc += n_windows; /* one --with-window-profile-internal-id per window,
                      * for the first tab in that window
                      */
  argc += n_windows; /* one --show-menubar or --hide-menubar per window */


  argc += n_tabs - n_windows; /* one --with-tab-profile-internal-id
                               * per extra tab
                               */

  argc += n_tabs * 2; /* one "--command foo" per tab */
  
  argv = g_new0 (char*, argc + 1);

  i = 0;
  argv[i] = g_strdup (EXECUTABLE_NAME);
  ++i;
  
  tmp = app->windows;
  while (tmp != NULL)
    {
      GList *tabs;
      GList *tmp2;
      TerminalWindow *window = tmp->data;
      
      tabs = terminal_window_list_screens (window);

      tmp2 = tabs;
      while (tmp2 != NULL)
        {
          TerminalScreen *screen = tmp2->data;
          const char *profile_id;
          const char **override_command;
          
          profile_id = terminal_profile_get_name (terminal_screen_get_profile (screen));
          
          if (tmp2 == tabs)
            {
              argv[i] = g_strdup_printf ("--window-with-profile-internal-id=%s",
                                         profile_id);
              ++i;
              if (terminal_window_get_menubar_visible (window))
                argv[i] = g_strdup ("--show-menubar");
              else
                argv[i] = g_strdup ("--hide-menubar");
              ++i;
            }
          else
            {
              argv[i] = g_strdup_printf ("--tab-with-profile-internal-id=%s",
                                         profile_id);
              ++i;
            }

          override_command = terminal_screen_get_override_command (screen);
          if (override_command)
            {
              char *flattened;

              argv[i] = g_strdup ("--command");
              ++i;
              
              flattened = g_strjoinv (" ", (char**) override_command);
              argv[i] = g_shell_quote (flattened);
              ++i;

              g_free (flattened);
            }
          
          tmp2 = tmp2->next;
        }
      
      g_list_free (tabs);
      
      tmp = tmp->next;
    }

  *argvp = argv;
}

static void
spew_restart_command (TerminalApp *app)
{
  char **argv;
  int i;
  
  terminal_app_get_clone_command (app, &argv);

  i = 0;
  while (argv[i])
    {
      g_print ("%s ", argv[i]);
      ++i;
    }
  g_print ("\n");
}
