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
#include "terminal-accels.h"
#include "terminal-window.h"
#include "profile-editor.h"
#include <gconf/gconf-client.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
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
  GtkWidget *edit_keys_dialog;
  GtkWidget *new_profile_dialog;
  GtkWidget *new_profile_name_entry;
  GtkWidget *new_profile_base_menu;
  GtkWidget *manage_profiles_dialog;
  GtkWidget *manage_profiles_list;
  GtkWidget *manage_profiles_new_button;
  GtkWidget *manage_profiles_edit_button;
  GtkWidget *manage_profiles_delete_button;
  GtkWidget *manage_profiles_default_menu;
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
static void refill_profile_treeview (GtkWidget *tree_view);

static void terminal_app_get_clone_command   (TerminalApp *app,
                                              char      ***argvp);

static void spew_restart_command             (TerminalApp *app);

static GtkWidget*       profile_optionmenu_new          (void);
static void             profile_optionmenu_refill       (GtkWidget       *option_menu);
static TerminalProfile* profile_optionmenu_get_selected (GtkWidget       *option_menu);
static void             profile_optionmenu_set_selected (GtkWidget       *option_menu,
                                                         TerminalProfile *profile);



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
  OPTION_USE_FACTORY,
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
    "use-factory",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_USE_FACTORY,
    N_("Ask existing profterm process to open a new window, if possible. (Not yet implemented, http://bugzilla.gnome.org/show_bug.cgi?id=71442"),
    NULL
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
  GnomeModuleRequirement reqs[] = {
    { "1.102.0", LIBGNOMEUI_MODULE },
    { NULL, NULL }
  };
  
  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  
  gtk_init (&argc, &argv);

  module_info.requirements = reqs;
  
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

        case OPTION_USE_FACTORY:
          /* FIXME */
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
  gconf_client_add_dir (conf, CONF_GLOBAL_PREFIX,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &err);
  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  CONF_GLOBAL_PREFIX, err->message);
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

  terminal_accels_init (conf);
  
  terminal_profile_initialize (conf);
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
              profile = terminal_profile_get_for_new_term ();
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
                                 terminal_profile_get_for_new_term (),
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

  app = g_new0 (TerminalApp, 1);
  
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
      window = terminal_window_new (conf);
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
  gboolean need_new_default;
  TerminalProfile *fallback;
  
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

  fallback = NULL;
  if (terminal_profile_get_count () == 0 ||
      terminal_profile_get_count () <= g_list_length (known))
    {
      /* We are going to run out, so create the fallback
       * to be sure we always have one. Must be done
       * here before we emit "forgotten" signals so that
       * screens have a profile to fall back to.
       *
       * If the profile with the FALLBACK_ID already exists,
       * we aren't allowed to delete it, unless at least one
       * other profile will still exist. And if you delete
       * all profiles, the FALLBACK_ID profile returns as
       * the living dead.
       */
      fallback = terminal_profile_ensure_fallback (conf);
    }
  
  /* Forget no-longer-existing profiles */
  need_new_default = FALSE;
  tmp_list = known;
  while (tmp_list != NULL)
    {
      TerminalProfile *forgotten;

      forgotten = TERMINAL_PROFILE (tmp_list->data);

      /* don't allow deleting the fallback if appropriate. */
      if (forgotten != fallback)
        {
          if (terminal_profile_get_is_default (forgotten))
            need_new_default = TRUE;
          
          terminal_profile_forget (forgotten);
        }
      
      tmp_list = tmp_list->next;
    }

  g_list_free (known);
  
  if (need_new_default)
    {
      TerminalProfile *new_default;

      known = terminal_profile_get_list ();
      
      g_assert (known);
      new_default = known->data;

      g_list_free (known);

      terminal_profile_set_is_default (new_default, TRUE);
    }

  g_assert (terminal_profile_get_count () > 0);
  
  spew_restart_command (app);
  if (app->new_profile_base_menu)
    profile_optionmenu_refill (app->new_profile_base_menu);
  if (app->manage_profiles_list)
    refill_profile_treeview (app->manage_profiles_list);
  if (app->manage_profiles_default_menu)
    profile_optionmenu_refill (app->manage_profiles_default_menu);
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
                           GtkWindow       *transient_parent)
{
  terminal_profile_edit (profile, transient_parent);
}

static void
edit_keys_destroyed_callback (GtkWidget   *new_profile_dialog,
                              TerminalApp *app)
{
  app->edit_keys_dialog = NULL;
}

void
terminal_app_edit_keybindings (TerminalApp     *app,
                               GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;

  if (app->edit_keys_dialog == NULL)
    {      
      old_transient_parent = NULL;      

      /* passing in transient_parent here purely for the
       * glade error dialog
       */
      app->edit_keys_dialog = terminal_edit_keys_dialog_new (transient_parent);

      if (app->edit_keys_dialog == NULL)
        return; /* glade file missing */
      
      g_signal_connect (G_OBJECT (app->edit_keys_dialog),
                        "destroy",
                        G_CALLBACK (edit_keys_destroyed_callback),
                        app);
    }
  else 
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->edit_keys_dialog));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->edit_keys_dialog),
                                    transient_parent);
      gtk_widget_hide (app->edit_keys_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->edit_keys_dialog);
  gtk_window_present (GTK_WINDOW (app->edit_keys_dialog));
}


enum
{
  RESPONSE_CREATE,
  RESPONSE_CANCEL,
  RESPONSE_DELETE
};

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
      GList *profiles = NULL;
      GList *tmp;
      GtkWindow *transient_parent;
      
      name = gtk_editable_get_chars (GTK_EDITABLE (app->new_profile_name_entry),
                                     0, -1);
      
      if (*name == '\0')
        bad_name_message = g_strdup (_("You have to name your profile."));
      
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

      base_profile = profile_optionmenu_get_selected (app->new_profile_base_menu);
      
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
                          GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;

  if (app->new_profile_dialog == NULL)
    {
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *entry;
      GtkWidget *label;
      GtkWidget *option_menu;
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
      app->new_profile_base_menu = profile_optionmenu_new ();
      option_menu = app->new_profile_base_menu;
      if (default_base_profile)
        profile_optionmenu_set_selected (option_menu,
                                         default_base_profile);
      
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), option_menu);
      
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), option_menu, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
      
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
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->new_profile_dialog));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
                                    transient_parent);
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
free_profiles_list (gpointer data)
{
  GList *profiles = data;
  
  g_list_foreach (profiles, (GFunc) g_object_unref, NULL);
  g_list_free (profiles);
}

static void
refill_profile_treeview (GtkWidget *tree_view)
{
  GList *profiles;
  GList *tmp;
  GtkTreeSelection *selection;
  GtkListStore *model;
  GList *selected_profiles;
  GtkTreeIter iter;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)));

  selected_profiles = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       list_selected_profiles_func,
                                       &selected_profiles);

  gtk_list_store_clear (model);
  
  profiles = terminal_profile_get_list ();
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

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
      
      if (g_list_find (selected_profiles, profile) != NULL)
        gtk_tree_selection_select_iter (selection, &iter);
    
      tmp = tmp->next;
    }

  if (selected_profiles == NULL)
    {
      /* Select first row */
      GtkTreePath *path;
      
      path = gtk_tree_path_new ();
      gtk_tree_path_append_index (path, 0);
      gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)), path);
      gtk_tree_path_free (path);
    }
  
  free_profiles_list (selected_profiles);
}

static GtkWidget*
create_profile_list (void)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkListStore *model;
  
  model = gtk_list_store_new (COLUMN_LAST,
                              G_TYPE_STRING,
                              G_TYPE_OBJECT);
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));

  g_object_unref (G_OBJECT (model));
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_MULTIPLE);

  refill_profile_treeview (tree_view);
  
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
  
  return tree_view;
}

static void
delete_confirm_response (GtkWidget   *dialog,
                         int          response_id,
                         GtkWindow   *transient_parent)
{
  GList *deleted_profiles;

  deleted_profiles = g_object_get_data (G_OBJECT (dialog),
                                        "deleted-profiles-list");
  
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      terminal_profile_delete_list (conf, deleted_profiles, transient_parent);
    }

  gtk_widget_destroy (dialog);
}

static void
profile_list_delete_selection (GtkWidget   *profile_list,
                               GtkWindow   *transient_parent,
                               TerminalApp *app)
{
  GtkTreeSelection *selection;
  GList *deleted_profiles;
  GtkWidget *dialog;
  GString *str;
  GList *tmp;
  int count;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (profile_list));

  deleted_profiles = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       list_selected_profiles_func,
                                       &deleted_profiles);

  if (deleted_profiles == NULL)
    {
      dialog = gtk_message_dialog_new (transient_parent,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("You must select one or more profiles to delete."));

      g_signal_connect (G_OBJECT (dialog), "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      
      gtk_widget_show_all (dialog);
      
      return;
    }
  
  count = g_list_length (deleted_profiles);

  if (count == terminal_profile_get_count ())
    {
      free_profiles_list (deleted_profiles);
      
      dialog = gtk_message_dialog_new (transient_parent,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("You must have at least one profile; you can't delete all of them."));

      g_signal_connect (G_OBJECT (dialog), "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      
      gtk_widget_show_all (dialog);
      
      return;
    }
  
  if (count > 1)
    {
      str = g_string_new (NULL);
      /* for languages with separate forms for 2 vs. many */
      if (count == 2)
        g_string_printf (str, _("Delete these two profiles?\n"));
      else
        g_string_printf (str, _("Delete these %d profiles?\n"), count);

      tmp = deleted_profiles;
      while (tmp != NULL)
        {
          g_string_append (str, "    ");
          g_string_append (str,
                           terminal_profile_get_visible_name (tmp->data));
          if (tmp->next)
            g_string_append (str, "\n");
          
          tmp = tmp->next;
        }
    }
  else
    {
      str = g_string_new (NULL);
      g_string_printf (str,
                       _("Delete profile \"%s\"?"),
                       terminal_profile_get_visible_name (deleted_profiles->data));
    }
  
  dialog = gtk_message_dialog_new (transient_parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   "%s", 
                                   str->str);

  g_string_free (str, TRUE);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_No"),
                          GTK_RESPONSE_REJECT,
                          _("_Delete"),
                          GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_REJECT);
  
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  g_object_set_data_full (G_OBJECT (dialog), "deleted-profiles-list",
                          deleted_profiles, free_profiles_list);
  
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (delete_confirm_response),
                    transient_parent);
  
  gtk_widget_show_all (dialog);
}

static void
new_button_clicked (GtkWidget   *button,
                    TerminalApp *app)
{
  terminal_app_new_profile (app,
                            NULL,
                            GTK_WINDOW (app->manage_profiles_dialog));
}

static void
edit_button_clicked (GtkWidget   *button,
                     TerminalApp *app)
{
  GtkTreeSelection *selection;
  GList *profiles;
      
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->manage_profiles_list));

  profiles = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       list_selected_profiles_func,
                                       &profiles);

  if (profiles == NULL)
    return; /* edit button was supposed to be insensitive... */

  if (profiles->next == NULL)
    {
      terminal_app_edit_profile (app,
                                 profiles->data,
                                 GTK_WINDOW (app->manage_profiles_dialog));
    }
  else
    {
      /* edit button was supposed to be insensitive due to multiple
       * selection
       */
    }
  
  g_list_foreach (profiles, (GFunc) g_object_unref, NULL);
  g_list_free (profiles);
}

static void
delete_button_clicked (GtkWidget   *button,
                       TerminalApp *app)
{
  profile_list_delete_selection (app->manage_profiles_list,
                                 GTK_WINDOW (app->manage_profiles_dialog),
                                 app);
}

static void
default_menu_changed (GtkWidget   *option_menu,
                      TerminalApp *app)
{
  TerminalProfile *p;

  p = profile_optionmenu_get_selected (app->manage_profiles_default_menu);

  if (!terminal_profile_get_is_default (p))
    terminal_profile_set_is_default (p, TRUE);
}

static void
manage_profiles_destroyed_callback (GtkWidget   *manage_profiles_dialog,
                                    TerminalApp *app)
{
  app->manage_profiles_dialog = NULL;
  app->manage_profiles_list = NULL;
  app->manage_profiles_new_button = NULL;
  app->manage_profiles_edit_button = NULL;
  app->manage_profiles_delete_button = NULL;
  app->manage_profiles_default_menu = NULL;
}

static void
fix_button_align (GtkWidget *button)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (button));

  if (GTK_IS_ALIGNMENT (child))
    g_object_set (G_OBJECT (child), "xalign", 0.0, NULL);
  else if (GTK_IS_LABEL (child))
    g_object_set (G_OBJECT (child), "xalign", 0.0, NULL);    
}

static void
count_selected_profiles_func (GtkTreeModel      *model,
                              GtkTreePath       *path,
                              GtkTreeIter       *iter,
                              gpointer           data)
{
  int *count = data;

  *count += 1;
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            TerminalApp      *app)
{
  int count;

  count = 0;
  gtk_tree_selection_selected_foreach (selection,
                                       count_selected_profiles_func,
                                       &count);

  gtk_widget_set_sensitive (app->manage_profiles_edit_button,
                            count == 1);
  gtk_widget_set_sensitive (app->manage_profiles_delete_button,
                            count > 0);
}

static void
profile_activated_callback (GtkTreeView       *tree_view,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            TerminalApp       *app)
{
  TerminalProfile *profile;
  GtkTreeIter iter;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (tree_view);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;
  
  profile = NULL;
  gtk_tree_model_get (model,
                      &iter,
                      COLUMN_PROFILE_OBJECT,
                      &profile,
                      -1);

  if (profile)
    terminal_app_edit_profile (app,
                               profile,
                               GTK_WINDOW (app->manage_profiles_dialog));
}

void
terminal_app_manage_profiles (TerminalApp     *app,
                              GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;

  if (app->manage_profiles_dialog == NULL)
    {
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *sw;
      GtkWidget *hbox;
      GtkWidget *button;
      GtkWidget *spacer;
      GtkRequisition req;
      GtkSizeGroup *size_group;
      GtkTreeSelection *selection;
      
      old_transient_parent = NULL;      
      
      app->manage_profiles_dialog =
        gtk_dialog_new_with_buttons (_("Manage terminal profiles"),
                                     NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);
      g_signal_connect (G_OBJECT (app->manage_profiles_dialog),
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        app);

      g_signal_connect (G_OBJECT (app->manage_profiles_dialog),
                        "destroy",
                        G_CALLBACK (manage_profiles_destroyed_callback),
                        app);
      
#define PADDING 5

      vbox = gtk_vbox_new (FALSE, PADDING);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), PADDING);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (app->manage_profiles_dialog)->vbox),
                          vbox, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (FALSE, PADDING);
      gtk_box_pack_end (GTK_BOX (vbox),
                        hbox, FALSE, FALSE, 0);

      app->manage_profiles_default_menu = profile_optionmenu_new ();
      g_signal_connect (G_OBJECT (app->manage_profiles_default_menu),
                        "changed", G_CALLBACK (default_menu_changed),
                        app);
      label = gtk_label_new_with_mnemonic (_("Profile _used when launching a new terminal:"));
      gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                     app->manage_profiles_default_menu);
      
      gtk_box_pack_start (GTK_BOX (hbox),
                          label, TRUE, TRUE, 0);
            
      gtk_box_pack_end (GTK_BOX (hbox),
                        app->manage_profiles_default_menu, FALSE, FALSE, 0);
      
      hbox = gtk_hbox_new (FALSE, PADDING);
      gtk_box_pack_start (GTK_BOX (vbox),
                          hbox, TRUE, TRUE, 0);
      
      vbox = gtk_vbox_new (FALSE, PADDING);
      gtk_box_pack_start (GTK_BOX (hbox),
                          vbox, TRUE, TRUE, 0);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
      
      label = gtk_label_new_with_mnemonic (_("_Profiles:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_size_group_add_widget (size_group, label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      app->manage_profiles_list = create_profile_list ();

      g_signal_connect (G_OBJECT (app->manage_profiles_list),
                        "row_activated",
                        G_CALLBACK (profile_activated_callback),
                        app);
      
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					   GTK_SHADOW_IN);
      
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

      gtk_container_add (GTK_CONTAINER (sw), app->manage_profiles_list);      
      
      gtk_dialog_set_default_response (GTK_DIALOG (app->manage_profiles_dialog),
                                       RESPONSE_CREATE);

      gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                     app->manage_profiles_list);

      vbox = gtk_vbox_new (FALSE, PADDING);
      gtk_box_pack_start (GTK_BOX (hbox),
                          vbox, FALSE, FALSE, 0);

      spacer = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      gtk_size_group_add_widget (size_group, spacer);      
      gtk_box_pack_start (GTK_BOX (vbox),
                          spacer, FALSE, FALSE, 0);
      
      button = gtk_button_new_from_stock (_("_New..."));
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox),
                          button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK (new_button_clicked), app);
      app->manage_profiles_new_button = button;
      
      button = gtk_button_new_with_mnemonic (_("_Edit..."));
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox),
                          button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK (edit_button_clicked), app);
      app->manage_profiles_edit_button = button;
      
      button = gtk_button_new_with_mnemonic (_("_Delete..."));
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox),
                          button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked",
                        G_CALLBACK (delete_button_clicked), app);
      app->manage_profiles_delete_button = button;
      
      /* Set default size of profile list */
      gtk_window_set_geometry_hints (GTK_WINDOW (app->manage_profiles_dialog),
                                     app->manage_profiles_list,
                                     NULL, 0);

      /* Incremental reflow makes this a bit useless, I guess. */
      gtk_widget_size_request (app->manage_profiles_list, &req);

      gtk_window_set_default_size (GTK_WINDOW (app->manage_profiles_dialog),
                                   MIN (req.width + 140, 450),
                                   MIN (req.height + 190, 400));

      gtk_widget_grab_focus (app->manage_profiles_list);

      g_object_unref (G_OBJECT (size_group));

      /* Monitor selection for sensitivity */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->manage_profiles_list));
      selection_changed_callback (selection, app);
      g_signal_connect (G_OBJECT (selection), "changed",
                        G_CALLBACK (selection_changed_callback),
                        app);

    }
  else 
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->manage_profiles_dialog));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->manage_profiles_dialog),
                                    transient_parent);
      gtk_widget_hide (app->manage_profiles_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->manage_profiles_dialog);
  gtk_window_present (GTK_WINDOW (app->manage_profiles_dialog));
}

static GtkWidget*
profile_optionmenu_new (void)
{
  GtkWidget *option_menu;
  
  option_menu = gtk_option_menu_new ();

  profile_optionmenu_refill (option_menu);
  
  return option_menu;
}

static void
profile_optionmenu_refill (GtkWidget *option_menu)
{
  GList *profiles;
  GList *tmp;
  int i;
  int history;
  GtkWidget *menu;
  GtkWidget *mi;
  TerminalProfile *selected;

  selected = profile_optionmenu_get_selected (option_menu);
  
  menu = gtk_menu_new ();
  
  profiles = terminal_profile_get_list ();

  history = 0;
  i = 0;
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;
      
      mi = gtk_menu_item_new_with_label (terminal_profile_get_visible_name (profile));

      gtk_widget_show (mi);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                             mi);

      g_object_ref (G_OBJECT (profile));
      g_object_set_data_full (G_OBJECT (mi),
                              "profile",
                              profile,
                              (GDestroyNotify) g_object_unref);
      
      if (profile == selected)
        history = i;
      
      ++i;
      tmp = tmp->next;
    }
  
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), history);
  
  g_list_free (profiles);  
}

static TerminalProfile*
profile_optionmenu_get_selected (GtkWidget *option_menu)
{
  GtkWidget *menu;
  GtkWidget *active;

  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  if (menu == NULL)
    return NULL;

  active = gtk_menu_get_active (GTK_MENU (menu));
  if (active == NULL)
    return NULL;

  return g_object_get_data (G_OBJECT (active), "profile");
}

static void
profile_optionmenu_set_selected (GtkWidget       *option_menu,
                                 TerminalProfile *profile)
{
  GtkWidget *menu;
  GList *children;
  GList *tmp;
  int i;
  
  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  if (menu == NULL)
    return;
  
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  i = 0;
  tmp = children;
  while (tmp != NULL)
    {
      if (g_object_get_data (G_OBJECT (tmp->data), "profile") == profile)
        break;
      ++i;
      tmp = tmp->next;
    }
  g_list_free (children);

  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), i);
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
