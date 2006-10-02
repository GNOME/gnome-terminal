/* terminal program */
/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2002 Sun Microsystems
 * Copyright (C) 2003 Mariano Suarez-Alvarez
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
#include "terminal-notebook.h"
#include "terminal-widget.h"
#include "profile-editor.h"
#include "encoding.h"
#include <gconf/gconf-client.h>
#include <bonobo-activation/bonobo-activation-activate.h>
#include <bonobo-activation/bonobo-activation-register.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-help.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>
#include <popt.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <gdk/gdkx.h>


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
  GtkWidget *edit_encodings_dialog;
  GtkWidget *new_profile_dialog;
  GtkWidget *manage_profiles_dialog;
  GtkWidget *manage_profiles_list;
  GtkWidget *manage_profiles_new_button;
  GtkWidget *manage_profiles_edit_button;
  GtkWidget *manage_profiles_delete_button;
  GtkWidget *manage_profiles_default_menu;
};

static GConfClient *conf = NULL;
static TerminalApp *app = NULL;
static gboolean terminal_factory_disabled = FALSE;
static gboolean initialization_complete = FALSE;
static GSList *pending_new_terminal_events = NULL;

#define TERMINAL_STOCK_EDIT "terminal-edit"
typedef struct
{
  char *startup_id;
  char *display_name;
  int screen_number;
  GList *initial_windows;
  gboolean default_window_menubar_forced;
  gboolean default_window_menubar_state;
  gboolean default_start_fullscreen;
  char *default_geometry;
  char *default_working_dir;
  char **post_execute_args;
} OptionParsingResults;

static void         sync_profile_list            (gboolean use_this_list,
                                                  GSList  *this_list);
static TerminalApp* terminal_app_new             (void);

static void profile_list_notify   (GConfClient *client,
                                   guint        cnxn_id,
                                   GConfEntry  *entry,
                                   gpointer     user_data);
static void refill_profile_treeview (GtkWidget *tree_view);

static void terminal_app_get_clone_command   (TerminalApp *app,
                                              int         *argc,
                                              char      ***argvp);

static GtkWidget*       profile_optionmenu_new          (void);
static void             profile_optionmenu_refill       (GtkWidget       *option_menu);
static TerminalProfile* profile_optionmenu_get_selected (GtkWidget       *option_menu);
static void             profile_optionmenu_set_selected (GtkWidget       *option_menu,
                                                         TerminalProfile *profile);


static gboolean save_yourself_callback (GnomeClient        *client,
                                        gint                phase,
                                        GnomeSaveStyle      save_style,
                                        gboolean            shutdown,
                                        GnomeInteractStyle  interact_style,
                                        gboolean            fast,
                                        void               *data);

static gboolean terminal_invoke_factory (int argc, char **argv);
static void option_parsing_results_free (OptionParsingResults *results);


static void parse_options_callback (poptContext              ctx,
                                    enum poptCallbackReason  reason,
                                    const struct poptOption *opt,
                                    const char              *arg,
                                    void                    *data);
static void client_die_cb          (GnomeClient             *client,
                                    gpointer                 data);

static void handle_new_terminal_events (void);

enum {
  OPTION_COMMAND = 1,
  OPTION_EXECUTE,
  OPTION_WINDOW,
  OPTION_WINDOW_WITH_PROFILE,
  OPTION_TAB,
  OPTION_TAB_WITH_PROFILE,
  OPTION_WINDOW_WITH_PROFILE_ID,
  OPTION_TAB_WITH_PROFILE_ID,
  OPTION_ROLE,
  OPTION_SHOW_MENUBAR,
  OPTION_HIDE_MENUBAR,
  OPTION_FULLSCREEN,
  OPTION_GEOMETRY,
  OPTION_DISABLE_FACTORY,
  OPTION_USE_FACTORY,
  OPTION_STARTUP_ID,
  OPTION_TITLE,
  OPTION_WORKING_DIRECTORY,
  OPTION_DEFAULT_WORKING_DIRECTORY,
  OPTION_ZOOM,
  OPTION_ACTIVE,
  OPTION_COMPAT,
  OPTION_LAST
};  

struct poptOption options[] = {  
  {
    NULL, 
    '\0', 
    POPT_ARG_CALLBACK | POPT_CBFLAG_POST,
    parse_options_callback, 
    0, 
    NULL, 
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
    "window",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_WINDOW,
    N_("Open a new window containing a tab with the default profile. More than one of these options can be provided."),
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
    "tab",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_TAB,
    N_("Open a new tab in the last-opened window with the default profile. More than one of these options can be provided."),
    NULL
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
    OPTION_WINDOW_WITH_PROFILE_ID,
    N_("Open a new window containing a tab with the given profile ID. Used internally to save sessions."),
    N_("PROFILEID")
  },
  {
    "tab-with-profile-internal-id",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_TAB_WITH_PROFILE_ID,
    N_("Open a new tab in the last-opened window with the given profile ID. Used internally to save sessions."),
    N_("PROFILEID")
  },
  {
    "role",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_ROLE,
    N_("Set the role for the last-specified window; applies to only one window; can be specified once for each window you create from the command line."),
    N_("ROLE")
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
    "full-screen",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_FULLSCREEN,
    N_("Set the last-specified window into fullscreen mode; applies to only one window; can be specified once for each window you create from the command line."),
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
    "disable-factory",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_DISABLE_FACTORY,
    N_("Do not register with the activation nameserver, do not re-use an active terminal"),
    NULL
  },
  {
    "use-factory",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_USE_FACTORY,
    N_("Register with the activation nameserver [default]"),
    NULL
  },
  {
    "startup-id",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_STARTUP_ID,
    N_("ID for startup notification protocol."),
    NULL
  },
  {
    "title",
    't',
    POPT_ARG_STRING,
    NULL,
    OPTION_TITLE,
    N_("Set the terminal's title"),
    N_("TITLE")
  },
  {
    "working-directory",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_WORKING_DIRECTORY,
    N_("Set the terminal's working directory"),
    N_("DIRNAME")
  },
  {
    "default-working-directory",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_DEFAULT_WORKING_DIRECTORY,
    N_("Set the default terminal's working directory. Used internally"),
    N_("DIRNAME")
  },
  {
    "zoom",
    '\0',
    POPT_ARG_STRING,
    NULL,
    OPTION_ZOOM,
    N_("Set the terminal's zoom factor (1.0 = normal size)"),
    N_("ZOOMFACTOR")
  },
  {
    "active",
    '\0',
    POPT_ARG_NONE,
    NULL,
    OPTION_ACTIVE,
    N_("Set the last specified tab as the active one in its window"),
    N_("ZOOMFACTOR")
  },
  
  /*
   * Crappy old compat args
   */
  {
    "tclass",
    '\0',
    POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "font",
    '\0',
    POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "nologin",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "login",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "foreground",
    '\0',
    POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "background",
    '\0',
    POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "solid",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "bgscroll",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "bgnoscroll",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "shaded",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "noshaded",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "transparent",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "utmp",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "noutmp",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "wtmp",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "nowtmp",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "lastlog",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "nolastlog",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "icon",
    '\0',
    POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },  
  {
    "termname",
    '\0',
    POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
  },
  {
    "start-factory-server",
    '\0',
    POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN,
    NULL,
    OPTION_COMPAT,
    NULL, NULL
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
  char *title;
  char *working_dir;
  double zoom;
  guint zoom_set : 1;
  guint active : 1;
} InitialTab;

typedef struct
{
  GList *tabs; /* list of InitialTab */

  gboolean force_menubar_state;
  gboolean menubar_state;

  gboolean start_fullscreen;

  char *geometry;
  char *role;
  
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
  it->title = NULL;
  it->working_dir = NULL;
  it->zoom = 1.0;
  it->zoom_set = FALSE;
  it->active = FALSE;
  
  return it;
}

static void
initial_tab_free (InitialTab *it)
{
  g_free (it->profile);
  g_strfreev (it->exec_argv);
  g_free (it->title);
  g_free (it->working_dir);
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
  iw->start_fullscreen = FALSE;
  iw->geometry = NULL;
  iw->role = NULL;
  
  return iw;
}

static void
initial_window_free (InitialWindow *iw)
{
  g_list_foreach (iw->tabs, (GFunc) initial_tab_free, NULL);
  g_list_free (iw->tabs);
  g_free (iw->geometry);
  g_free (iw->role);
  g_free (iw);
}

static void
apply_defaults (OptionParsingResults *results,
                InitialWindow        *iw)
{
  g_assert (iw->geometry == NULL);

  if (results->default_geometry)
    {
      iw->geometry = results->default_geometry;
      results->default_geometry = NULL;
    }

  if (results->default_window_menubar_forced)
    {
      iw->force_menubar_state = TRUE;
      iw->menubar_state = results->default_window_menubar_state;

      results->default_window_menubar_forced = FALSE;
    }

  iw->start_fullscreen = results->default_start_fullscreen;
}

static InitialWindow*
ensure_top_window (OptionParsingResults *results)
{
  InitialWindow *iw;
  
  if (results->initial_windows == NULL)
    {
      iw = initial_window_new (NULL, FALSE);
      apply_defaults (results, iw);

      results->initial_windows = g_list_append (results->initial_windows,
                                                iw);
    }
  else
    {
      iw = g_list_last (results->initial_windows)->data;
    }

  g_assert (iw->tabs);
  
  return iw;
}

static InitialTab*
ensure_top_tab (OptionParsingResults *results)
{
  InitialWindow *iw;
  InitialTab *it;

  iw = ensure_top_window (results);

  g_assert (iw->tabs);
  
  it = g_list_last (iw->tabs)->data;

  return it;
}

static void
set_default_icon (const char *filename)
{
  GdkPixbuf *pixbuf;
  GError *err;
  GList *list;
  
  err = NULL;
  pixbuf = gdk_pixbuf_new_from_file (filename, &err);

  if (pixbuf == NULL)
    {
      g_printerr (_("Could not load icon \"%s\": %s\n"),
                  filename, err->message);
      g_error_free (err);

      return;
    }

  list = NULL;
  list = g_list_prepend (list, pixbuf);

  gtk_window_set_default_icon_list (list);

  g_list_free (list);
  g_object_unref (G_OBJECT (pixbuf));
}

static InitialWindow*
add_new_window (OptionParsingResults *results,
                const char           *profile,
                gboolean              is_id)
{
  InitialWindow *iw;

  iw = initial_window_new (profile, is_id);
  
  apply_defaults (results, iw);

  results->initial_windows = g_list_append (results->initial_windows, iw);
  
  return iw;
}

static void
parse_options_callback (poptContext              ctx,
                        enum poptCallbackReason  reason,
                        const struct poptOption *opt,
                        const char              *arg,
                        void                    *data)
{
  OptionParsingResults *results;

  results = data;

  if (reason == POPT_CALLBACK_REASON_POST)
    {
      /* Make sure we have some window */
      if (results->initial_windows == NULL)
        ensure_top_window (results);

      return;
    }
  else if (reason != POPT_CALLBACK_REASON_OPTION)
    return;

  switch (opt->val & POPT_ARG_MASK)
    {
    case OPTION_COMMAND:
      {
        GError *err;
        InitialTab *it;
        char **exec_argv;
            

        if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires specifying the command to run\n"), "--command/-e");
            exit (1);
          }
            
        err = NULL;
        exec_argv = NULL;
        if (!g_shell_parse_argv (arg, NULL, &exec_argv, &err))
          {
            g_printerr (_("Argument to \"%s\" is not a valid command: %s\n"),
                        "--command/-e", err->message);
            g_error_free (err);
            exit (1);
          }

        it = ensure_top_tab (results);

        if (it->exec_argv != NULL)
          {
            g_printerr (_("\"%s\" specified more than once for the same window or tab\n"),
                        "--command/-e/--execute/-x");
            exit (1);
          }

        it->exec_argv = exec_argv;
      }
      break;

    case OPTION_EXECUTE:
      {
        InitialTab *it;
            
        if (results->post_execute_args == NULL)
          {
            g_printerr (_("Option \"%s\" requires specifying the command to run on the rest of the command line\n"),
                        "--execute/-x");
            exit (1);
          }

        it = ensure_top_tab (results);

        if (it->exec_argv != NULL)
          {
            g_printerr (_("\"%s\" specified more than once for the same window or tab\n"),
                        "--command/-e/--execute/-x");
            exit (1);
          }

        it->exec_argv = results->post_execute_args;
        results->post_execute_args = NULL;
      }
      break;
          
    case OPTION_WINDOW:
    case OPTION_WINDOW_WITH_PROFILE:
    case OPTION_WINDOW_WITH_PROFILE_ID:
      {
        InitialWindow *iw;
        const char *profile;


        if (opt->val == OPTION_WINDOW)
          profile = NULL;
        else if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument specifying what profile to use\n"),
                        "--window-with-profile");
            exit (1);
          }
        else
          profile = arg;

        iw = add_new_window (results, profile,
                             opt->val == OPTION_WINDOW_WITH_PROFILE_ID);
      }
      break;

    case OPTION_TAB:
    case OPTION_TAB_WITH_PROFILE:
    case OPTION_TAB_WITH_PROFILE_ID:
      {
        InitialWindow *iw;
        const char *profile;


        if (opt->val == OPTION_TAB)
          profile = NULL;
        else if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument specifying what profile to use\n"),
                        "--tab-with-profile");
            exit (1);
          }
        else
          profile = arg;

                
        if (results->initial_windows)
          {
            iw = g_list_last (results->initial_windows)->data;

            iw->tabs = g_list_append (iw->tabs,
                                      initial_tab_new (profile,
                                                       opt->val == OPTION_TAB_WITH_PROFILE_ID));
          }
        else
          {
            iw = add_new_window (results, profile, 
                                 opt->val == OPTION_TAB_WITH_PROFILE_ID);
          }
      }
      break;
          
    case OPTION_SHOW_MENUBAR:
      {
        InitialWindow *iw;
            
        if (results->initial_windows)
          {
            iw = g_list_last (results->initial_windows)->data;

            if (iw->force_menubar_state)
              {
                g_printerr (_("\"%s\" option given twice for the same window\n"), "--show-menubar");

                exit (1);
              }
                
            iw->force_menubar_state = TRUE;
            iw->menubar_state = TRUE;
          }
        else
          {
            results->default_window_menubar_forced = TRUE;
            results->default_window_menubar_state = TRUE;
          }
      }
      break;
          
    case OPTION_HIDE_MENUBAR:
      {
        InitialWindow *iw;
            
        if (results->initial_windows)
          {
            iw = g_list_last (results->initial_windows)->data;

            if (iw->force_menubar_state)
              {
                g_printerr (_("\"%s\" option given twice for the same window\n"), "--hide-menubar");

                exit (1);
              }
                
            iw->force_menubar_state = TRUE;
            iw->menubar_state = FALSE;                
          }
        else
          {
            results->default_window_menubar_forced = TRUE;
            results->default_window_menubar_state = FALSE;
          }
      }
      break;

    case OPTION_FULLSCREEN:
      {
        InitialWindow *iw;
            
        if (results->initial_windows)
          {
            iw = g_list_last (results->initial_windows)->data;

            iw->start_fullscreen = TRUE;
          }
        else
          {
            results->default_start_fullscreen = TRUE;
          }
      }
      break;

    case OPTION_ROLE:
      {
        InitialWindow *iw;


        if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument giving the role\n"), "--role");
            exit (1);
          }
            
        if (results->initial_windows)
          {
            iw = g_list_last (results->initial_windows)->data;
            if (iw->role)
              {
                g_printerr (_("Two roles given for one window\n"));
                exit (1);
              }

            iw->role = g_strdup (arg);
          }
      }
      break;

    case OPTION_GEOMETRY:
      {
        InitialWindow *iw;


        if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument giving the geometry\n"), "--geometry");
            exit (1);
          }
            
        if (results->initial_windows)
          {
            iw = g_list_last (results->initial_windows)->data;
            if (iw->geometry)
              {
                g_printerr (_("Two \"%s\" options given for one window\n"), "--geometry");
                exit (1);
              }

            iw->geometry = g_strdup (arg);
          }
        else
          {
            if (results->default_geometry)
              {
                g_printerr (_("Two geometries given for one window\n"));
                exit (1);
              }
            else
              {
                results->default_geometry = g_strdup (arg);
              }
          }
      }
      break;

    case OPTION_DISABLE_FACTORY:
      terminal_factory_disabled = TRUE;
      break;

    case OPTION_USE_FACTORY:
      terminal_factory_disabled = FALSE;
      break;

    case OPTION_TITLE:
      {
        InitialTab *it;
            

        if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument giving the title\n"), "--title");
            exit (1);
          }

        it = ensure_top_tab (results);

        if (it->title)
          {
            g_printerr (_("Two \"%s\" options given for one tab\n"), "--title");
            exit (1);
          }

        it->title = g_strdup (arg);
      }
      break;

    case OPTION_WORKING_DIRECTORY:
      {
        InitialTab *it;
            

        if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument giving the directory\n"), "--working-directory");
            exit (1);
          }

        it = ensure_top_tab (results);

        if (it->working_dir)
          {
            g_printerr (_("Two \"%s\" options given for one tab\n"), "--working-directory");
            exit (1);
          }

        it->working_dir = g_strdup (arg);
      }
      break;

    case OPTION_DEFAULT_WORKING_DIRECTORY:
      {
        if (arg == NULL)
          {
            g_printerr (_("Option --default-working-directory requires an argument giving the directory\n"));
            exit (1);
          }

        if (results->default_working_dir)
          {
            g_printerr (_("Two --default-working-directories given\n"));
            exit (1);
          }

        results->default_working_dir = g_strdup (arg);
      }
      break;

    case OPTION_ZOOM:
      {
        InitialTab *it;            
        double val;
        char *end;
        
        if (arg == NULL)
          {
            g_printerr (_("Option \"%s\" requires an argument giving the zoom factor\n"), "--zoom");
            exit (1);
          }

        it = ensure_top_tab (results);

        if (it->zoom_set)
          {
            g_printerr (_("Two \"%s\" options given for one tab\n"), "--zoom");
            exit (1);
          }

        /* Try reading a locale-style double first, in case it was
         * typed by a person, then fall back to ascii_strtod (we
         * always save session in C locale format)
         */
        end = NULL;
        val = g_strtod (arg, &end);
        if (end == NULL || *end != '\0')
          {
            val = g_ascii_strtod (arg, &end);
            if (end == NULL || *end != '\0')
              {
                g_printerr (_("\"%s\" is not a valid zoom factor\n"),
                            arg);
                exit (1);
              }
          }

        if (val < (TERMINAL_SCALE_MINIMUM + 1e-6))
          {
            g_printerr (_("Zoom factor \"%g\" is too small, using %g\n"),
                        val, TERMINAL_SCALE_MINIMUM);
            val = TERMINAL_SCALE_MINIMUM;
          }

        if (val > (TERMINAL_SCALE_MAXIMUM - 1e-6))
          {
            g_printerr (_("Zoom factor \"%g\" is too large, using %g\n"),
                        val, TERMINAL_SCALE_MAXIMUM);
            val = TERMINAL_SCALE_MAXIMUM;
          }
        
        it->zoom = val;
        it->zoom_set = TRUE;
      }
      break;

    case OPTION_ACTIVE:
      {
        InitialTab *it;

        it = ensure_top_tab (results);

        it->active = TRUE;
      }
      break;
      
    case OPTION_COMPAT:
      g_printerr (_("Option --%s is no longer supported in this version of gnome-terminal; you might want to create a profile with the desired setting, and use the new --window-with-profile option\n"), opt->longName);
      break;

    case OPTION_STARTUP_ID:
      if (results->startup_id != NULL)
        {
          g_printerr (_("\"%s\" option given twice\n"), "--startup-id");
          exit (1);
        }
      else if (arg == NULL)
        {
          g_printerr (_("\"%s\" option requires an argument\n"), "--startup-id");
          exit (1);
        }
      else
        {
          results->startup_id = g_strdup (arg);
        }
      break;
      
    case OPTION_LAST:
      g_assert_not_reached ();
      break;
      /* no default so we get warnings on missing items */
    }
}

static void
option_parsing_results_free (OptionParsingResults *results)
{
  g_list_foreach (results->initial_windows, (GFunc) initial_window_free, NULL);
  g_list_free (results->initial_windows);

  g_free (results->default_geometry);
  g_free (results->default_working_dir);

  if (results->post_execute_args)
    g_strfreev (results->post_execute_args);

  g_free (results->display_name);
  g_free (results->startup_id);
  
  g_free (results);
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

static OptionParsingResults *
option_parsing_results_init (int *argc, char **argv)
{
  int i;
  OptionParsingResults *results;

  results = g_new0 (OptionParsingResults, 1);
  results->screen_number = -1;
  results->default_working_dir = NULL;
  
  /* pre-scan for -x and --execute options (code from old gnome-terminal) */
  results->post_execute_args = NULL;
  i = 1;
  while (i < *argc)
    {
      if (strcmp (argv[i], "-x") == 0 ||
          strcmp (argv[i], "--execute") == 0)
        {
          int last;
          int j;

          ++i;
          last = i;
          if (i == *argc)
            break; /* we'll complain about this later. */
          
          results->post_execute_args = g_new0 (char*, *argc - i + 1);
          j = 0;
          while (i < *argc)
            {
              results->post_execute_args[j] = g_strdup (argv[i]);

              i++; 
              j++;
            }
          results->post_execute_args[j] = NULL;

          /* strip the args we used up, also ends the loop since i >= last */
          *argc = last;
        }
      
      ++i;
    }

  return results;
}

static void
option_parsing_results_check_for_display_name (OptionParsingResults *results,
                                               int *argc, char **argv)
{
  int i;
  
  /* The point here is to strip --display, in the case where we
   * aren't going via gtk_init()
   */
  i = 1;
  while (i < *argc)
    {
      gboolean remove_two = FALSE;
      
      if (strcmp (argv[i], "-x") == 0 ||
          strcmp (argv[i], "--execute") == 0)
        {
          return; /* We can't have --display or --screen past here,
                   * unless intended for the child process.
                   */
        }
      else if (strcmp (argv[i], "--display") == 0)
        {          
          if ((i + 1) >= *argc)
            {
              g_printerr (_("No argument given to \"%s\" option\n"), "--display");
              return; /* popt will die on this later, plus it shouldn't happen
                       * because normally gtk_init() parses --display
                       * when not using factory mode.
                       */
            }
          
          if (results->display_name)
            g_free (results->display_name);

          g_assert (i+1 < *argc);
          results->display_name = g_strdup (argv[i+1]);
          
          remove_two = TRUE;
        }
      else if (strcmp (argv[i], "--screen") == 0)
        {
          int n;
          char *end;
          
          if ((i + 1) >= *argc)
            {
              g_printerr (_("\"%s\" option requires an argument\n"), "--screen");
              return; /* popt will die on this later, plus it shouldn't happen
                       * because normally gtk_init() parses --display
                       * when not using factory mode.
                       */
            }

          g_assert (i+1 < *argc);
          
          errno = 0;
          end = argv[i+1];
          n = strtoul (argv[i+1], &end, 0);
          if (errno == 0 && argv[i+1] != end)
            results->screen_number = n;
          
          remove_two = TRUE;
        }

      if (remove_two)
        {
          int n_to_move;
          
          n_to_move = *argc - i - 2;
          g_assert (n_to_move >= 0);
          
          if (n_to_move > 0)
            {
              g_memmove (&argv[i], &argv[i+2],
                         sizeof (argv[0]) * n_to_move);
              argv[*argc-1] = NULL;
              argv[*argc-2] = NULL;
            }
          else
            {
              argv[i] = NULL;
            }

          *argc -= 2;
        }
      else
        {
          ++i;
        }
    }
}

static void
option_parsing_results_apply_directory_defaults (OptionParsingResults *results)
{
  GList *w, *t;

  if (results->default_working_dir == NULL)
    return;

  for (w = results->initial_windows; w; w = w ->next)
    {
      InitialWindow *window;

      window = (InitialWindow*) w->data;
      
      for (t = window->tabs; t ; t = t->next)
        {
          InitialTab *tab;

          tab = (InitialTab*) t->data;

          if (tab->working_dir == NULL)
            tab->working_dir = g_strdup (results->default_working_dir);
        }
    }
}

static int
new_terminal_with_options (OptionParsingResults *results)
{
  GList *tmp;
  
  tmp = results->initial_windows;
  while (tmp != NULL)
    {
      TerminalProfile *profile;
      GList *tmp2;
      TerminalWindow *current_window;
      TerminalScreen *active_screen;
      
      InitialWindow *iw = tmp->data;

      g_assert (iw->tabs);

      current_window = NULL;
      active_screen = NULL;
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
          else if (it->profile == NULL)
            {
              profile = terminal_profile_get_for_new_term (NULL);
            }
          
          if (profile == NULL)
            {
              if (it->profile)
                g_printerr (_("No such profile '%s', using default profile\n"),
                            it->profile);
              profile = terminal_profile_get_for_new_term (NULL);
            }
          
          g_assert (profile);

          if (tmp2 == iw->tabs)
            {
              terminal_app_new_terminal (app,
                                         profile,
                                         NULL,
                                         NULL,
                                         iw->force_menubar_state,
                                         iw->menubar_state,
                                         iw->start_fullscreen,
                                         it->exec_argv,
                                         iw->geometry,
                                         it->title,
                                         it->working_dir,
                                         iw->role,
                                         it->zoom_set ?
                                         it->zoom : 1.0,
                                         results->startup_id,
                                         results->display_name,
                                         results->screen_number);

              current_window = g_list_last (app->windows)->data;
            }
          else
            {
              terminal_app_new_terminal (app,
                                         profile,
                                         current_window,
                                         NULL,
                                         FALSE, FALSE,
                                         FALSE/*not fullscreen*/,
                                         it->exec_argv,
                                         NULL,
                                         it->title,
                                         it->working_dir,
                                         NULL,
                                         it->zoom_set ?
                                         it->zoom : 1.0,
                                         NULL, NULL, -1);
            }
          
          if (it->active)
            {
              /* TerminalWindow's interface does not expose the list of TerminalScreens,
               * so we use the fact that terminal_app_new_terminal() sets the new terminal
               * to be the active one. Not nice.
               */
              active_screen = terminal_window_get_active (current_window);
             }

          tmp2 = tmp2->next;
        }
      
      if (active_screen)
        terminal_window_set_active (current_window, active_screen);

      tmp = tmp->next;
    }

  return 0;
}

/* This assumes that argv already has room for the args,
 * and inserts them just after argv[0]
 */
static void
insert_args (int        *argc,
             char      **argv,
             const char *arg1,
             const char *arg2)
{
  int i;

  i = *argc;
  while (i >= 1)
    {
      argv[i+2] = argv[i];
      --i;
    }

  /* fill in 1 and 2 */
  argv[1] = g_strdup (arg1);
  argv[2] = g_strdup (arg2);
  *argc += 2;
  argv[*argc] = NULL;
}

/* Copied from libnautilus/nautilus-program-choosing.c; Needed in case
 * we have no DESKTOP_STARTUP_ID (with its accompanying timestamp).
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
  Window xwindow;
  XEvent event;

  {
    XSetWindowAttributes attrs;
    Atom atom_name;
    Atom atom_type;
    char *name;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
		     RootWindow (xdisplay, 0),
		     -100, -100, 1, 1,
		     0,
		     CopyFromParent,
		     CopyFromParent,
		     CopyFromParent,
		     CWOverrideRedirect | CWEventMask,
		     &attrs);

    atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
    g_assert (atom_name != None);
    atom_type = XInternAtom (xdisplay, "STRING", TRUE);
    g_assert (atom_type != None);

    name = "Fake Window";
    XChangeProperty (xdisplay, 
		     xwindow, atom_name,
		     atom_type,
		     8, PropModeReplace, (unsigned char *)name, strlen (name));
  }

  XWindowEvent (xdisplay,
		xwindow,
		PropertyChangeMask,
		&event);

  XDestroyWindow(xdisplay, xwindow);

  return event.xproperty.time;
}

int
main (int argc, char **argv)
{
  GError *err;
  poptContext ctx;
  int i;
  int argc_copy;
  char **argv_copy;
  const char **args;
  const char *startup_id;
  const char *display_name;
  GdkDisplay *display;
  GnomeModuleRequirement reqs[] = {
    { "2.0.0", NULL },
    { NULL, NULL }
  };
  GnomeClient *sm_client;
  OptionParsingResults *results;
  GnomeProgram *program;

  reqs[0].module_info = LIBGNOMEUI_MODULE;

  if (setlocale (LC_ALL, "") == NULL)
    g_printerr ("GNOME Terminal: locale not understood by C library, internationalization will not work\n");

#if 0
  {
    const char *charset;
    g_get_charset (&charset);
    
    g_print ("Running in locale \"%s\" with encoding \"%s\"\n",
             setlocale (LC_ALL, NULL), charset);
  }
#endif
  
  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  argc_copy = argc;
  /* we leave empty slots, for --startup-id, --display and --default-working-directory */
  argv_copy = g_new0 (char *, argc_copy + 7);
  for (i = 0; i < argc_copy; i++)
    argv_copy [i] = g_strdup (argv [i]);
  argv_copy [i] = NULL;

  results = option_parsing_results_init (&argc, argv);
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  if (startup_id != NULL && *startup_id != '\0')
    {
      results->startup_id = g_strdup (startup_id);
      putenv ("DESKTOP_STARTUP_ID=");
    }
  
  gtk_window_set_auto_startup_notification (FALSE); /* we'll do it ourselves due
                                                     * to complicated factory setup
                                                     */
  
  gtk_init (&argc, &argv);

  /* Do this here so that gdk_display is initialized */
  if (results->startup_id == NULL)
    {
      /* Create a fake one containing a timestamp that we can use */
      Time timestamp;
      timestamp = slowly_and_stupidly_obtain_timestamp (gdk_display);
      results->startup_id = g_strdup_printf ("_TIME%lu",
                                             timestamp);
    }

  g_set_application_name (_("Terminal"));
  
  display = gdk_display_get_default ();
  display_name = gdk_display_get_name (display);
  results->display_name = g_strdup (display_name);
  
  module_info.requirements = reqs;

  options[0].descrip = (void*) results; /* I hate GnomeProgram, popt, and their
                                         * mutant spawn
                                         */
  program = gnome_program_init (PACKAGE, VERSION,
                                &module_info,
                                argc,
                                argv,
                                GNOME_PARAM_POPT_TABLE, options,
                                GNOME_PARAM_APP_PREFIX, TERM_PREFIX,
                                GNOME_PARAM_APP_SYSCONFDIR, TERM_SYSCONFDIR,
                                GNOME_PARAM_APP_DATADIR, TERM_DATADIR,
                                GNOME_PARAM_APP_LIBDIR, TERM_LIBDIR,
                                NULL); 
  
  g_object_get (G_OBJECT (program),
                GNOME_PARAM_POPT_CONTEXT, &ctx,
                NULL);
  
  args = poptGetArgs (ctx);
  if (args)
    {
      g_printerr (_("Invalid argument: \"%s\"\n"), *args);
      g_strfreev (argv_copy);
      return 1;
    }
  poptFreeContext (ctx);

  option_parsing_results_apply_directory_defaults (results);
  
  gtk_rc_parse_string ("style \"hig-dialog\" {\n"
                         "GtkDialog::action-area-border = 0\n"
                         "GtkDialog::button-spacing = 12\n"
                         "}\n");

  if (!terminal_factory_disabled)
    {
      char *cwd;
      
      if (results->startup_id != NULL)
        {
          /* we allocated argv_copy with extra space so we could do this */
          insert_args (&argc_copy, argv_copy,
                       "--startup-id", results->startup_id);
        }

      /* Forward our display to the child */
      insert_args (&argc_copy, argv_copy,
                   "--display", results->display_name);
      
      /* Forward out working directory */
      cwd = g_get_current_dir ();
      insert_args (&argc_copy, argv_copy,
                   "--default-working-directory", cwd);
      g_free (cwd);

      if (terminal_invoke_factory (argc_copy, argv_copy))
        {
          g_strfreev (argv_copy);
          option_parsing_results_free (results);
          return 0;
        }
    }

  g_strfreev (argv_copy);
  argv_copy = NULL;

  set_default_icon (TERM_DATADIR"/pixmaps/gnome-terminal.png");
 
  g_assert (results->post_execute_args == NULL);
  
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
  terminal_encoding_init (conf);
  
  terminal_profile_initialize (conf);
  sync_profile_list (FALSE, NULL);

  sm_client = gnome_master_client ();
  g_signal_connect (G_OBJECT (sm_client),
                    "save_yourself",
                    G_CALLBACK (save_yourself_callback),
                    app);

  g_signal_connect (G_OBJECT (sm_client), "die",
                    G_CALLBACK (client_die_cb),
                    NULL);

  if (new_terminal_with_options (results))
    {
      option_parsing_results_free (results);
      return 1;
    }

  option_parsing_results_free (results);
  results = NULL;

  initialization_complete = TRUE;
  handle_new_terminal_events ();
  
  gtk_main ();
  
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
}

static GdkScreen*
find_screen_by_display_name (const char *display_name,
                             int         screen_number)
{
  GdkScreen *screen;
  
  /* --screen=screen_number overrides --display */
  
  screen = NULL;
  
  if (display_name == NULL)
    {
      if (screen_number >= 0)
        screen = gdk_display_get_screen (gdk_display_get_default (), screen_number);

      if (screen == NULL)
        screen = gdk_screen_get_default ();

      g_object_ref (G_OBJECT (screen));
    }
  else
    {
      GSList *displays;
      GSList *tmp;
      const char *period;
      GdkDisplay *display;
        
      period = strrchr (display_name, '.');
      if (period)
        {
          unsigned long n;
          char *end;
          
          errno = 0;
          end = (char*) period + 1;
          n = strtoul (period + 1, &end, 0);
          if (errno == 0 && (period + 1) != end)
            screen_number = n;
        }
      
      displays = gdk_display_manager_list_displays (gdk_display_manager_get ());

      display = NULL;
      tmp = displays;
      while (tmp != NULL)
        {
          const char *this_name;

          display = tmp->data;
          this_name = gdk_display_get_name (display);
          
          /* compare without the screen number part */
          if (strncmp (this_name, display_name, period - display_name) == 0)
            break;

          tmp = tmp->next;
        }
      
      g_slist_free (displays);

      if (display == NULL)
        display = gdk_display_open (display_name); /* FIXME we never close displays */
      
      if (display != NULL)
        {
          if (screen_number >= 0)
            screen = gdk_display_get_screen (display, screen_number);
          
          if (screen == NULL)
            screen = gdk_display_get_default_screen (display);

          if (screen)
            g_object_ref (G_OBJECT (screen));
        }
    }

  if (screen == NULL)
    {
      screen = gdk_screen_get_default ();
      g_object_ref (G_OBJECT (screen));
    }
  
  return screen;
}

void
terminal_app_new_terminal (TerminalApp     *app,
                           TerminalProfile *profile,
                           TerminalWindow  *window,
                           TerminalScreen  *screen,
                           gboolean         force_menubar_state,
                           gboolean         forced_menubar_state,
                           gboolean         start_fullscreen,
                           char           **override_command,
                           const char      *geometry,
                           const char      *title,
                           const char      *working_dir,
                           const char      *role,
                           double           zoom,
                           const char      *startup_id,
                           const char      *display_name,
                           int              screen_number)
{
  gboolean window_created;
  gboolean screen_created;
  
  g_return_if_fail (profile);

  window_created = FALSE;
  if (window == NULL)
    {
      GdkScreen *gdk_screen;
      
      window_created = TRUE;
      window = terminal_window_new (conf);
      g_object_ref (G_OBJECT (window));
      
      g_signal_connect (G_OBJECT (window), "destroy",
                        G_CALLBACK (terminal_window_destroyed),
                        app);
      
      app->windows = g_list_append (app->windows, window);

      gdk_screen = find_screen_by_display_name (display_name, screen_number);
      if (gdk_screen != NULL)
        {
          gtk_window_set_screen (GTK_WINDOW (window), gdk_screen);
          g_object_unref (G_OBJECT (gdk_screen));
        }

      if (startup_id != NULL)
        terminal_window_set_startup_id (window, startup_id);
      
      if (role == NULL)
        terminal_util_set_unique_role (GTK_WINDOW (window), "gnome-terminal");
      else
        gtk_window_set_role (GTK_WINDOW (window), role);
    }

  if (force_menubar_state)
    {
      terminal_window_set_menubar_visible (window, forced_menubar_state);
    }

  screen_created = FALSE;
  if (screen == NULL)
    {  
      screen_created = TRUE;
      screen = terminal_screen_new ();
      
      terminal_screen_set_profile (screen, profile);
    
      if (title)
	{
          terminal_screen_set_title (screen, title);
	  terminal_screen_set_dynamic_title (screen, title, FALSE);
	  terminal_screen_set_dynamic_icon_title (screen, title, FALSE);
	}

      if (working_dir)
        terminal_screen_set_working_dir (screen, working_dir);
      
      if (override_command)    
        terminal_screen_set_override_command (screen, override_command);
    
      terminal_screen_set_font_scale (screen, zoom);
      terminal_screen_set_font (screen);
    
      terminal_window_add_screen (window, screen);

      terminal_screen_reread_profile (screen);

      g_object_unref (G_OBJECT (screen));
    
      terminal_window_set_active (window, screen);
    }
  else
   {
      TerminalWindow *src_win = terminal_screen_get_window (screen);
      TerminalNotebook *src_notebook = TERMINAL_NOTEBOOK (terminal_window_get_notebook (src_win));
      TerminalNotebook *dest_notebook = TERMINAL_NOTEBOOK (terminal_window_get_notebook (window));
      
      terminal_notebook_move_tab (src_notebook, dest_notebook, screen, 0);
    }

  if (geometry)
    {
      if (!gtk_window_parse_geometry (GTK_WINDOW (window),
                                      geometry))
        g_printerr (_("Invalid geometry string \"%s\"\n"),
                    geometry);
    }

  if (start_fullscreen)
    {
      terminal_window_set_fullscreen (window, TRUE);
    }

  /* don't present on new tab, or we can accidentally make the
   * terminal jump workspaces.
   * http://bugzilla.gnome.org/show_bug.cgi?id=78253
   */
  if (window_created)
    gtk_window_present (GTK_WINDOW (window));

  if (screen_created)
    terminal_screen_launch_child (screen);
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

      if (!use_this_list)
        g_free (tmp_slist->data);

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

  if (app->new_profile_dialog)
    {
      GtkWidget *new_profile_base_menu;

      new_profile_base_menu = g_object_get_data (G_OBJECT (app->new_profile_dialog), "base_option_menu");
      profile_optionmenu_refill (new_profile_base_menu);
    }
  if (app->manage_profiles_list)
    refill_profile_treeview (app->manage_profiles_list);
  if (app->manage_profiles_default_menu)
    profile_optionmenu_refill (app->manage_profiles_default_menu);

  tmp_list = app->windows;
  while (tmp_list != NULL)
    {
      terminal_window_reread_profile_list (TERMINAL_WINDOW (tmp_list->data));

      tmp_list = tmp_list->next;
    }
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

static void
edit_encodings_destroyed_callback (GtkWidget   *new_profile_dialog,
                                   TerminalApp *app)
{
  app->edit_encodings_dialog = NULL;
}

void
terminal_app_edit_encodings (TerminalApp     *app,
                             GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;

  if (app->edit_encodings_dialog == NULL)
    {      
      old_transient_parent = NULL;      

      /* passing in transient_parent here purely for the
       * glade error dialog
       */
      app->edit_encodings_dialog =
        terminal_encoding_dialog_new (transient_parent);

      if (app->edit_encodings_dialog == NULL)
        return; /* glade file missing */
      
      g_signal_connect (G_OBJECT (app->edit_encodings_dialog),
                        "destroy",
                        G_CALLBACK (edit_encodings_destroyed_callback),
                        app);
    }
  else 
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->edit_encodings_dialog));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->edit_encodings_dialog),
                                    transient_parent);
      gtk_widget_hide (app->edit_encodings_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->edit_encodings_dialog);
  gtk_window_present (GTK_WINDOW (app->edit_encodings_dialog));
}

enum
{
  RESPONSE_CREATE = GTK_RESPONSE_ACCEPT, /* Arghhh: Glade wants a GTK_RESPONSE_* for dialog buttons */
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
      GtkWidget *name_entry;
      char *name;
      char *escaped_name;
      GtkWidget *base_option_menu;
      TerminalProfile *base_profile = NULL;
      TerminalProfile *new_profile;
      GList *profiles;
      GList *tmp;
      GtkWindow *transient_parent;
      GtkWidget *confirm_dialog;
      gint retval;
      
      name_entry = g_object_get_data (G_OBJECT (new_profile_dialog), "name_entry");
      name = gtk_editable_get_chars (GTK_EDITABLE (name_entry), 0, -1);
      g_strstrip (name); /* name will be non empty after stripping */
      
      profiles = terminal_profile_get_list ();
      for (tmp = profiles; tmp != NULL; tmp = tmp->next)
        {
          TerminalProfile *profile = tmp->data;

          if (strcmp (name, terminal_profile_get_visible_name (profile)) == 0)
            break;
        }
      if (tmp)
        {
          confirm_dialog = gtk_message_dialog_new (GTK_WINDOW (new_profile_dialog), 
						   GTK_DIALOG_DESTROY_WITH_PARENT,
                	 			   GTK_MESSAGE_QUESTION, 
						   GTK_BUTTONS_YES_NO, 
			 			   _("You already have a profile called \"%s\". Do you want to create another profile with the same name?"), name);
          retval = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
          gtk_widget_destroy (confirm_dialog);
          if (retval == GTK_RESPONSE_NO)   
            goto cleanup;
        }
      g_list_free (profiles);

      base_option_menu = g_object_get_data (G_OBJECT (new_profile_dialog), "base_option_menu");
      base_profile = profile_optionmenu_get_selected (base_option_menu);
      
      if (base_profile == NULL)
        {
          terminal_util_show_error_dialog (GTK_WINDOW (new_profile_dialog), NULL, 
                                          _("The profile you selected as a base for your new profile no longer exists"));
          goto cleanup;
        }

      transient_parent = gtk_window_get_transient_for (GTK_WINDOW (new_profile_dialog));
      
      gtk_widget_destroy (new_profile_dialog);
      
      terminal_profile_create (base_profile, name, transient_parent);

      escaped_name = gconf_escape_key (name, -1);
      new_profile = terminal_profile_new (escaped_name, conf);
      terminal_profile_update (new_profile);
      sync_profile_list (FALSE, NULL);
      g_free (escaped_name);
      
      new_profile = terminal_profile_lookup_by_visible_name (name);

      if (new_profile == NULL)
        {
          terminal_util_show_error_dialog (transient_parent, NULL, 
                                           _("There was an error creating the profile \"%s\""), name);
        }
      else 
        {
          terminal_profile_edit (new_profile, transient_parent);
        }

    cleanup:
      g_free (name);
    }
  else
    {
      gtk_widget_destroy (new_profile_dialog);
    }
}

static void
new_profile_name_entry_changed_callback (GtkEditable *editable, gpointer data)
{
  char *name, *saved_name;
  GtkWidget *create_button;

  create_button = (GtkWidget*) data;

  saved_name = name = gtk_editable_get_chars (editable, 0, -1);

  /* make the create button sensitive only if something other than space has been set */
  while (*name != '\0' && g_ascii_isspace (*name))
    name++;
 
  gtk_widget_set_sensitive (create_button, *name != '\0' ? TRUE : FALSE);

  g_free (saved_name);
}

void
terminal_app_new_profile (TerminalApp     *app,
                          TerminalProfile *default_base_profile,
                          GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;
  GtkWidget *create_button;

  if (app->new_profile_dialog == NULL)
    {
      GladeXML *xml;
      GtkWidget *w, *wl;
      GtkWidget *create_button;
      GtkSizeGroup *size_group, *size_group_labels;

      xml = terminal_util_load_glade_file (TERM_GLADE_FILE, "new-profile-dialog", transient_parent);

      if (xml == NULL)
        return;

      app->new_profile_dialog = glade_xml_get_widget (xml, "new-profile-dialog");
      g_signal_connect (G_OBJECT (app->new_profile_dialog), "response", G_CALLBACK (new_profile_response_callback), app);

      terminal_util_set_unique_role (GTK_WINDOW (app->new_profile_dialog), "gnome-terminal-new-profile");
  
      g_object_add_weak_pointer (G_OBJECT (app->new_profile_dialog), (void**) &app->new_profile_dialog);

      create_button = glade_xml_get_widget (xml, "new-profile-create-button");
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "create_button", create_button);
      gtk_widget_set_sensitive (create_button, FALSE);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      size_group_labels = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

      /* the name entry */
      w = glade_xml_get_widget (xml, "new-profile-name-entry");
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "name_entry", w);
      g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (new_profile_name_entry_changed_callback), create_button);
      gtk_entry_set_activates_default (GTK_ENTRY (w), TRUE);
      gtk_widget_grab_focus (w);
      terminal_util_set_atk_name_description (w, NULL, _("Enter profile name"));
      gtk_size_group_add_widget (size_group, w);

      wl = glade_xml_get_widget (xml, "new-profile-name-label");
      gtk_label_set_mnemonic_widget (GTK_LABEL (wl), w);
      gtk_size_group_add_widget (size_group_labels, wl);
 
      /* the base profile option menu */
      w = glade_xml_get_widget (xml, "new-profile-base-option-menu");
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "base_option_menu", w);
      terminal_util_set_atk_name_description (w, NULL, _("Choose base profile"));
      profile_optionmenu_refill (w);
      gtk_size_group_add_widget (size_group, w);

      wl = glade_xml_get_widget (xml, "new-profile-base-label");
      gtk_label_set_mnemonic_widget (GTK_LABEL (wl), w);
      gtk_size_group_add_widget (size_group_labels, wl);

      gtk_dialog_set_default_response (GTK_DIALOG (app->new_profile_dialog), RESPONSE_CREATE);

      g_object_unref (G_OBJECT (size_group));
      g_object_unref (G_OBJECT (size_group_labels));

      g_object_unref (G_OBJECT (xml));
    }

  old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->new_profile_dialog));
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
                                    transient_parent);
      gtk_widget_hide (app->new_profile_dialog); /* re-show the window on its new parent */
    }

  create_button = g_object_get_data (G_OBJECT (app->new_profile_dialog), "create_button");
  gtk_widget_set_sensitive (create_button, FALSE);
  
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
  terminal_util_set_atk_name_description (tree_view, _("Profile list"), NULL);

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
      terminal_util_show_error_dialog (transient_parent, NULL, _("You must select one or more profiles to delete."));
      return;
    }
  
  count = g_list_length (deleted_profiles);

  if (count == terminal_profile_get_count ())
    {
      free_profiles_list (deleted_profiles);

      terminal_util_show_error_dialog (transient_parent, NULL,
                                       _("You must have at least one profile; you can't delete all of them."));
      return;
    }
  
  if (count > 1)
    {
      str = g_string_new (NULL);
      g_string_printf (str,
		       ngettext ("Delete this profile?\n",
				 "Delete these %d profiles?\n",
				 count),
		       count);

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
                          GTK_STOCK_CANCEL,
                          GTK_RESPONSE_REJECT,
			  GTK_STOCK_DELETE,
                          GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);
 
  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete Profile")); 
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
default_profile_changed (TerminalProfile           *profile,
                         const TerminalSettingMask *mask,
                         void                      *profile_optionmenu)
{
  if (mask->is_default)
    {
      if (terminal_profile_get_is_default (profile))
        profile_optionmenu_set_selected (GTK_WIDGET (profile_optionmenu),
                                         profile);      
    }
}

static void
monitor_profiles_for_is_default_change (GtkWidget *profile_optionmenu)
{
  GList *profiles;
  GList *tmp;
  
  profiles = terminal_profile_get_list ();

  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

      g_signal_connect_object (G_OBJECT (profile),
                               "changed",
                               G_CALLBACK (default_profile_changed),
                               G_OBJECT (profile_optionmenu),
                               0);
      
      tmp = tmp->next;
    }

  g_list_free (profiles);
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
  GtkWidget *alignment;

  child = gtk_bin_get_child (GTK_BIN (button));

  g_object_ref (child);
  gtk_container_remove (GTK_CONTAINER (button), child);

  alignment = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 12);

  gtk_container_add (GTK_CONTAINER (alignment), child);
  g_object_unref (child);

  gtk_container_add (GTK_CONTAINER (button), alignment);

  if (GTK_IS_ALIGNMENT (child) || GTK_IS_LABEL (child))
    {
      g_object_set (G_OBJECT (child), "xalign", 0.0, NULL);
    }
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


static void
manage_profiles_response_cb (GtkDialog *dialog,
                             int        id,
                             void      *data)
{
  TerminalApp *app;

  app = data;

  g_assert (app->manage_profiles_dialog == GTK_WIDGET (dialog));
  
  if (id == GTK_RESPONSE_HELP)
    terminal_util_show_help ("gnome-terminal-manage-profiles", GTK_WINDOW (dialog));
  else
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
terminal_app_register_stock (void)
{
  static gboolean registered = FALSE;

  if (!registered)
    {
      GtkIconFactory *factory;
      GtkIconSet     *icons;

      static GtkStockItem edit_item [] = {
	{ TERMINAL_STOCK_EDIT, N_("_Edit"), 0, 0, GETTEXT_PACKAGE },
      };

      icons = gtk_icon_factory_lookup_default (GTK_STOCK_PREFERENCES);
      factory = gtk_icon_factory_new ();
      gtk_icon_factory_add (factory, TERMINAL_STOCK_EDIT, icons);
      gtk_icon_factory_add_default (factory);
      gtk_stock_add_static (edit_item, 1);
      registered = TRUE;
    }
}

void
terminal_app_manage_profiles (TerminalApp     *app,
                              GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;

  if (app->manage_profiles_dialog == NULL)
    {
      GtkWidget *main_vbox;
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *sw;
      GtkWidget *hbox;
      GtkWidget *button;
      GtkWidget *spacer;
      GtkRequisition req;
      GtkSizeGroup *size_group;
      GtkTreeSelection *selection;
      
      terminal_app_register_stock ();

      old_transient_parent = NULL;      
      
      app->manage_profiles_dialog =
        gtk_dialog_new_with_buttons (_("Profiles"),
                                     NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_HELP,
                                     GTK_RESPONSE_HELP,
                                     GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (app->manage_profiles_dialog),
                                       GTK_RESPONSE_ACCEPT);
      g_signal_connect (GTK_DIALOG (app->manage_profiles_dialog),
                        "response",
                        G_CALLBACK (manage_profiles_response_cb),
                        app);

      g_signal_connect (G_OBJECT (app->manage_profiles_dialog),
                        "destroy",
                        G_CALLBACK (manage_profiles_destroyed_callback),
                        app);

      terminal_util_set_unique_role (GTK_WINDOW (app->manage_profiles_dialog), "gnome-terminal-profile-manager");

      gtk_widget_set_name (app->manage_profiles_dialog, "profile-manager-dialog");
      gtk_rc_parse_string ("widget \"profile-manager-dialog\" style \"hig-dialog\"\n");

      gtk_dialog_set_has_separator (GTK_DIALOG (app->manage_profiles_dialog), FALSE);
      gtk_container_set_border_width (GTK_CONTAINER (app->manage_profiles_dialog), 10);
      gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (app->manage_profiles_dialog)->vbox), 12);

      main_vbox = gtk_vbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (app->manage_profiles_dialog)->vbox), main_vbox, TRUE, TRUE, 0);
     
      // the top thing
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, TRUE, TRUE, 0);
      
      vbox = gtk_vbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
      
      label = gtk_label_new_with_mnemonic (_("_Profiles:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_size_group_add_widget (size_group, label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      app->manage_profiles_list = create_profile_list ();

      g_signal_connect (G_OBJECT (app->manage_profiles_list), "row_activated",
                        G_CALLBACK (profile_activated_callback), app);
      
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (sw), app->manage_profiles_list);      
      
      gtk_dialog_set_default_response (GTK_DIALOG (app->manage_profiles_dialog), RESPONSE_CREATE);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), app->manage_profiles_list);

      vbox = gtk_vbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

      spacer = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      gtk_size_group_add_widget (size_group, spacer);      
      gtk_box_pack_start (GTK_BOX (vbox), spacer, FALSE, FALSE, 0);
      
      button = gtk_button_new_from_stock (GTK_STOCK_NEW);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (new_button_clicked), app);
      app->manage_profiles_new_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_new_button, NULL,                             
                                              _("Click to open new profile dialog"));
      
      button = gtk_button_new_from_stock (TERMINAL_STOCK_EDIT);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (edit_button_clicked), app);
      app->manage_profiles_edit_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_edit_button, NULL,                            
                                              _("Click to open edit profile dialog"));
      
      button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (delete_button_clicked), app);
      app->manage_profiles_delete_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_delete_button, NULL,                          
                                              _("Click to delete selected profile"));
      // bottom line
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new_with_mnemonic (_("Profile _used when launching a new terminal:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
            
      app->manage_profiles_default_menu = profile_optionmenu_new ();
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), app->manage_profiles_default_menu);
      if (terminal_profile_get_default ())
        {
          profile_optionmenu_set_selected (app->manage_profiles_default_menu, terminal_profile_get_default ());
        }
      g_signal_connect (G_OBJECT (app->manage_profiles_default_menu), "changed", 
                        G_CALLBACK (default_menu_changed), app);
      monitor_profiles_for_is_default_change (app->manage_profiles_default_menu);
      gtk_box_pack_start (GTK_BOX (hbox), app->manage_profiles_default_menu, TRUE, TRUE, 0);
 
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
      g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed_callback), app);
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
  terminal_util_set_atk_name_description (option_menu, NULL, _("Click button to choose profile"));

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
                                int         *argcp,
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

  if (terminal_factory_disabled)
    argc += 1; /* --disable-factory */
  
  argc += n_windows; /* one --with-window-profile-internal-id per window,
                      * for the first tab in that window
                      */
  argc += n_windows; /* one --show-menubar or --hide-menubar per window */

  argc += n_windows; /* one --role per window */

  argc += n_windows; /* one --active per window */

  argc += n_tabs - n_windows; /* one --with-tab-profile-internal-id
                               * per extra tab
                               */

  argc += n_tabs * 2; /* one "--command foo" per tab */

  argc += n_tabs * 2; /* one "--title foo" per tab */

  argc += n_tabs * 2; /* one "--working-directory foo" per tab */

  argc += n_tabs * 2; /* one "--zoom" per tab */
  
  argc += 2; /* one "--geometry" per active tab */
  
  argv = g_new0 (char*, argc + 1);

  i = 0;
  argv[i] = g_strdup (EXECUTABLE_NAME);
  ++i;

  if (terminal_factory_disabled)
    {
      argv[i] = g_strdup ("--disable-factory");
      ++i;
    }
  
  tmp = app->windows;
  while (tmp != NULL)
    {
      GList *tabs;
      GList *tmp2;
      TerminalWindow *window = tmp->data;
      TerminalScreen *active_screen;

      active_screen = terminal_window_get_active (window);
      
      tabs = terminal_window_list_screens (window);

      tmp2 = tabs;
      while (tmp2 != NULL)
        {
          TerminalScreen *screen = tmp2->data;
          const char *profile_id;
          const char **override_command;
          const char *title;
          double zoom;
          
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
              argv[i] = g_strdup_printf ("--role=%s",
                                         gtk_window_get_role (GTK_WINDOW (window)));
              ++i;
            }
          else
            {
              argv[i] = g_strdup_printf ("--tab-with-profile-internal-id=%s",
                                         profile_id);
              ++i;
            }

          if (screen == active_screen)
            {
              int w, h, x, y;

              /* FIXME saving the geometry is not great :-/ */
              argv[i] = g_strdup ("--active");
              ++i;
              argv[i] = g_strdup ("--geometry");
              ++i;
              terminal_widget_get_size (terminal_screen_get_widget (screen), &w, &h);
              gtk_window_get_position (GTK_WINDOW (window), &x, &y);
              argv[i] = g_strdup_printf ("%dx%d+%d+%d", w, h, x, y);
              ++i;
            }

          override_command = terminal_screen_get_override_command (screen);
          if (override_command)
            {
              char *flattened;

              argv[i] = g_strdup ("--command");
              ++i;
              
              flattened = g_strjoinv (" ", (char**) override_command);
              argv[i] = flattened;
              ++i;
            }

          title = terminal_screen_get_dynamic_title (screen);
          if (title)
            {
              argv[i] = g_strdup ("--title");
              ++i;
              argv[i] = g_strdup (title);
              ++i;
            }

          {
            const char *dir;

            dir = terminal_screen_get_working_dir (screen);

            if (dir != NULL && *dir != '\0') /* should always be TRUE anyhow */
              {
                argv[i] = g_strdup ("--working-directory");
                ++i;
                argv[i] = g_strdup (dir);
                ++i;
              }
          }

          zoom = terminal_screen_get_font_scale (screen);
          if (zoom < -1e-6 || zoom > 1e-6) /* if not 1.0 */
            {
              char buf[G_ASCII_DTOSTR_BUF_SIZE];

              g_ascii_dtostr (buf, sizeof (buf), zoom);
              
              argv[i] = g_strdup ("--zoom");
              ++i;
              argv[i] = g_strdup (buf);
              ++i;
            }
          
          tmp2 = tmp2->next;
        }
      
      g_list_free (tabs);
      
      tmp = tmp->next;
    }

  *argvp = argv;
  *argcp = i;
}

static gboolean
save_yourself_callback (GnomeClient        *client,
                        gint                phase,
                        GnomeSaveStyle      save_style,
                        gboolean            shutdown,
                        GnomeInteractStyle  interact_style,
                        gboolean            fast,
                        void               *data)
{
  char **clone_command;
  TerminalApp *app;
  int argc;
  int i;
  
  app = data;
  
  terminal_app_get_clone_command (app, &argc, &clone_command);

  /* GnomeClient builds the clone command from the restart command */
  gnome_client_set_restart_command (client, argc, clone_command);

  /* Debug spew */
  g_print ("Saving session: ");
  i = 0;
  while (clone_command[i])
    {
      g_print ("%s ", clone_command[i]);
      ++i;
    }
  g_print ("\n");

  g_strfreev (clone_command);
  
  /* success */
  return TRUE;
}

static void
client_die_cb (GnomeClient        *client,
               gpointer            data)
{
  gtk_main_quit ();
}

/*
 * Utility stuff
 */

void
terminal_util_set_unique_role (GtkWindow *window, const char *prefix)
{
  char *role;

  role = g_strdup_printf ("%s-%d-%d-%d", prefix, getpid (), g_random_int (), (int) time (NULL));
  gtk_window_set_role (window, role);
  g_free (role);
}

/**
 * terminal_util_show_error_dialog:
 * @transient_parent: parent of the future dialog window;
 * @weap_ptr: pointer to a #Widget pointer, to control the population.
 * @message_format: printf() style format string
 *
 * Create a #GtkMessageDialog window with the message, and present it, handling its buttons.
 * If @weap_ptr is not #NULL, only create the dialog if <literal>*weap_ptr</literal> is #NULL 
 * (and in that * case, set @weap_ptr to be a weak pointer to the new dialog), otherwise just 
 * present <literal>*weak_ptr</literal>. Note that in this last case, the message <emph>will</emph>
 * be changed.
 */

void
terminal_util_show_error_dialog (GtkWindow *transient_parent, GtkWidget **weak_ptr, const char *message_format, ...)
{
  char *message;
  va_list args;

  if (message_format)
    {
      va_start (args, message_format);
      message = g_strdup_vprintf (message_format, args);
      va_end (args);
    }
  else message = NULL;

  if (weak_ptr == NULL || *weak_ptr == NULL)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (transient_parent,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       message);

      g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), NULL);

      if (weak_ptr != NULL)
        {
        *weak_ptr = dialog;
        g_object_add_weak_pointer (G_OBJECT (dialog), (void**)weak_ptr);
        }

      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      
      gtk_widget_show_all (dialog);
    }
  else 
    {
      g_return_if_fail (GTK_IS_MESSAGE_DIALOG (*weak_ptr));

      gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (*weak_ptr)->label), message);

      gtk_window_present (GTK_WINDOW (*weak_ptr));
    }
  }

void
terminal_util_show_help (const char *topic, 
                         GtkWindow  *transient_parent)
{
  GError *err;

  err = NULL;

  gnome_help_display ("gnome-terminal", topic, &err);
  
  if (err)
    {
      terminal_util_show_error_dialog (GTK_WINDOW (transient_parent), NULL,
                                       _("There was an error displaying help: %s"),
                                      err->message);
      g_error_free (err);
    }
}
 
/* sets accessible name and description for the widget */

void
terminal_util_set_atk_name_description (GtkWidget  *widget,
                                        const char *name,
                                        const char *desc)
{
  AtkObject *obj;
  
  obj = gtk_widget_get_accessible (widget);

  if (obj == NULL)
    {
      g_warning ("%s: for some reason widget has no GtkAccessible",
                 G_GNUC_FUNCTION);
      return;
    }

  
  if (!GTK_IS_ACCESSIBLE (obj))
    return; /* This means GAIL is not loaded so we have the NoOp accessible */
      
  g_return_if_fail (GTK_IS_ACCESSIBLE (obj));  
  if (desc)
    atk_object_set_description (obj, desc);
  if (name)
    atk_object_set_name (obj, name);
}

GladeXML*
terminal_util_load_glade_file (const char *filename,
                               const char *widget_root,
                               GtkWindow  *error_dialog_parent)
{
  char *path;
  GladeXML *xml;

  xml = NULL;
  path = g_strconcat ("./", filename, NULL);
  
  if (g_file_test (path,
                   G_FILE_TEST_EXISTS))
    {
      /* Try current dir, for debugging */
      xml = glade_xml_new (path,
                           widget_root,
                           GETTEXT_PACKAGE);
    }
  
  if (xml == NULL)
    {
      g_free (path);
      
      path = g_build_filename (TERM_GLADE_DIR, filename, NULL);

      xml = glade_xml_new (path,
                           widget_root,
                           GETTEXT_PACKAGE);
    }

  if (xml == NULL)
    {
      static GtkWidget *no_glade_dialog = NULL;

      terminal_util_show_error_dialog (error_dialog_parent, &no_glade_dialog, 
                                       _("The file \"%s\" is missing. This indicates that the application is installed incorrectly."), path);
    }

  g_free (path);

  return xml;
}

/* Factory stuff */

typedef struct
{
  int argc;
  char **argv;
} NewTerminalEvent;

static void
handle_new_terminal_event (int          argc,
                           char       **argv)
{
  int nextopt;
  poptContext ctx;
  const void *store;
  OptionParsingResults *results;

  g_assert (initialization_complete);
  
  results = option_parsing_results_init (&argc, argv);
  
  /* Find and parse --display */
  option_parsing_results_check_for_display_name (results,
                                                 &argc,
                                                 argv);
  
  store = options[0].descrip;
  options[0].descrip = (void*) results; /* I hate GnomeProgram, popt, and their
                                         * mutant spawn
                                         */
  ctx = poptGetContext (PACKAGE,
                        argc,
                        (const char**)argv,
			options, 0);
  
  g_return_if_fail (app != NULL);

  while ((nextopt = poptGetNextOpt (ctx)) > 0 ||
	 nextopt == POPT_ERROR_BADOPT)
    /* do nothing */ ;

  if (nextopt != -1)
    g_warning ("Error on option %s: %s, passed from terminal child",
	       poptBadOption (ctx, 0),
	       poptStrerror (nextopt));

  poptFreeContext (ctx);
  options[0].descrip = store;

  option_parsing_results_apply_directory_defaults (results);

  new_terminal_with_options (results);

  option_parsing_results_free (results);
}

static void
handle_new_terminal_events (void)
{
  static gboolean currently_handling_events = FALSE;

  if (currently_handling_events)
    return;
  currently_handling_events = TRUE;

  while (pending_new_terminal_events != NULL)
    {
      GSList *next;
      NewTerminalEvent *event = pending_new_terminal_events->data;

      handle_new_terminal_event (event->argc, event->argv);

      next = pending_new_terminal_events->next;
      g_strfreev (event->argv);
      g_free (event);
      g_slist_free_1 (pending_new_terminal_events);
      pending_new_terminal_events = next;
    }
  currently_handling_events = FALSE;
}

/*
 *   Invoked remotely to instantiate a terminal with the
 * given arguments.
 */
static void
terminal_new_event (BonoboListener    *listener,
		    const char        *event_name, 
		    const CORBA_any   *any,
		    CORBA_Environment *ev,
		    gpointer           user_data)
{
  CORBA_sequence_CORBA_string *args;
  char **tmp_argv;
  int tmp_argc;
  int i;
  NewTerminalEvent *event;
  
  if (strcmp (event_name, "new_terminal"))
    {
      g_warning ("Unknown event '%s' on terminal",
		 event_name);
      return;
    }

  args = any->_value;
  
  tmp_argv = g_new0 (char*, args->_length + 1);
  i = 0;
  while (i < args->_length)
    {
      tmp_argv[i] = g_strdup (((const char**)args->_buffer)[i]);
      ++i;
    }
  tmp_argv[i] = NULL;
  tmp_argc = i;

  event = g_new0 (NewTerminalEvent, 1);
  event->argc = tmp_argc;
  event->argv = tmp_argv;

  pending_new_terminal_events = g_slist_append (pending_new_terminal_events,
                                                event);

  if (initialization_complete)
    handle_new_terminal_events ();
}

#define ACT_IID "OAFIID:GNOME_Terminal_Factory"

static Bonobo_RegistrationResult
terminal_register_as_factory (void)
{
  char *per_display_iid;
  BonoboListener *listener;
  Bonobo_RegistrationResult result;

  listener = bonobo_listener_new (terminal_new_event, NULL);

  per_display_iid = bonobo_activation_make_registration_id (
    ACT_IID, DisplayString (gdk_display));

  result = bonobo_activation_active_server_register (
    per_display_iid, BONOBO_OBJREF (listener));

  if (result != Bonobo_ACTIVATION_REG_SUCCESS)
    bonobo_object_unref (BONOBO_OBJECT (listener));

  g_free (per_display_iid);

  return result;
}

static gboolean
terminal_invoke_factory (int argc, char **argv)
{
  Bonobo_Listener listener;

  switch (terminal_register_as_factory ())
    {
      case Bonobo_ACTIVATION_REG_SUCCESS:
	/* we were the first terminal to register */
	return FALSE;
      case Bonobo_ACTIVATION_REG_NOT_LISTED:
	g_printerr (_("It appears that you do not have gnome-terminal.server installed in a valid location. Factory mode disabled.\n"));
        return FALSE;
      case Bonobo_ACTIVATION_REG_ERROR:
        g_printerr (_("Error registering terminal with the activation service; factory mode disabled.\n"));
        return FALSE;
      case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
        /* lets use it then */
        break;
    }

  listener = bonobo_activation_activate_from_id (
    ACT_IID, Bonobo_ACTIVATION_FLAG_EXISTING_ONLY, NULL, NULL);

  if (listener != CORBA_OBJECT_NIL)
    {
      int i;
      CORBA_any any;
      CORBA_sequence_CORBA_string args;
      CORBA_Environment ev;

      CORBA_exception_init (&ev);

      any._type = TC_CORBA_sequence_CORBA_string;
      any._value = &args;

      args._length = argc;
      args._buffer = g_newa (CORBA_char *, args._length);

      for (i = 0; i < args._length; i++)
        args._buffer [i] = argv [i];
      
      Bonobo_Listener_event (listener, "new_terminal", &any, &ev);
      CORBA_Object_release (listener, &ev);
      if (!BONOBO_EX (&ev))
        return TRUE;

      CORBA_exception_free (&ev);
    }
  else
    g_printerr (_("Failed to retrieve terminal server from activation server\n"));

  return FALSE;
}

