/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Gnome-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib.h>

#include "terminal-intl.h"

#undef BONOBO_DISABLE_DEPRECATED
#undef G_DISABLE_SINGLE_INCLUDES

#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-window.h"
#include "terminal-util.h"
#include "profile-editor.h"
#include "encoding.h"
#include <bonobo-activation/bonobo-activation-activate.h>
#include <bonobo-activation/bonobo-activation-register.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <gdk/gdkx.h>

#define ACT_IID "OAFIID:GNOME_Terminal_Factory"

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

static gboolean initialization_complete = FALSE;
static GSList *pending_new_terminal_events = NULL;
static BonoboListener *listener = NULL;
static gboolean factory_registered = FALSE;

typedef struct
{
  char    *startup_id;
  char    *display_name;
  int      screen_number;
  GList   *initial_windows;
  gboolean default_window_menubar_forced;
  gboolean default_window_menubar_state;
  gboolean default_fullscreen;
  gboolean default_maximize;
  char    *default_role;
  char    *default_geometry;
  char    *default_working_dir;
  char   **post_execute_args;

  gboolean  execute;
  gboolean  use_factory;
  char     *zoom;
} OptionParsingResults;

static GOptionContext * get_goption_context (OptionParsingResults *parsing_results);

static gboolean terminal_invoke_factory (int argc, char **argv);

static void handle_new_terminal_events (void);

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
  gboolean start_maximized;

  char *geometry;
  char *role;
  
} InitialWindow;

static InitialTab*
initial_tab_new (const char *profile,
                 gboolean    is_id)
{
  InitialTab *it;

  it = g_slice_new (InitialTab);

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
  g_slice_free (InitialTab, it);
}

static InitialWindow*
initial_window_new (const char *profile,
                    gboolean    is_id)
{
  InitialWindow *iw;
  
  iw = g_slice_new (InitialWindow);
  
  iw->tabs = g_list_prepend (NULL, initial_tab_new (profile, is_id));
  iw->force_menubar_state = FALSE;
  iw->menubar_state = FALSE;
  iw->start_fullscreen = FALSE;
  iw->start_maximized = FALSE;
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
  g_slice_free (InitialWindow, iw);
}

static void
apply_defaults (OptionParsingResults *results,
                InitialWindow        *iw)
{
  if (results->default_role)
    {
      iw->role = results->default_role;
      results->default_role = NULL;
    }
  
  if (iw->geometry == NULL)
    iw->geometry = g_strdup (results->default_geometry);

  if (results->default_window_menubar_forced)
    {
      iw->force_menubar_state = TRUE;
      iw->menubar_state = results->default_window_menubar_state;

      results->default_window_menubar_forced = FALSE;
    }

  iw->start_fullscreen |= results->default_fullscreen;
  iw->start_maximized |= results->default_maximize;
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

/* handle deprecated command line options */
static gboolean
unsupported_option_callback (const gchar *option_name,
                             const gchar *value,
                             gpointer     data,
                             GError     **error)
{
  g_printerr (_("Option \"%s\" is no longer supported in this version of gnome-terminal;"
               " you might want to create a profile with the desired setting, and use"
               " the new '--window-with-profile' option\n"), option_name);
  return TRUE; /* we do not want to bail out here but continue */
}


static gboolean 
option_command_callback (const gchar *option_name,
                         const gchar *value,
                         gpointer     data,
                         GError     **error)
{
  OptionParsingResults *results = data;
  InitialTab *it;
  GError *err = NULL;
  char  **exec_argv;

  if (!g_shell_parse_argv (value, NULL, &exec_argv, &err))
    {
      g_set_error(error,
                  G_OPTION_ERROR,
                  G_OPTION_ERROR_BAD_VALUE,
                  _("Argument to \"%s\" is not a valid command: %s\n"),
                   "--command/-e",
                  err->message);
      g_error_free (err);
      return FALSE;
    }

  it = ensure_top_tab (results);
  it->exec_argv = exec_argv;

  return TRUE;
}


static gboolean 
option_window_callback (const gchar *option_name,
                        const gchar *value,
                        gpointer     data,
                        GError     **error)
{
  OptionParsingResults *results = data;

  add_new_window (results, NULL, FALSE);

  return TRUE;
}


static gboolean 
option_window_with_profile_callback (const gchar *option_name,
                                     const gchar *value,
                                     gpointer     data,
                                     GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;
  const char *profile;
  profile = value;

  iw = add_new_window (results, profile, FALSE);

  return TRUE;
}


static gboolean 
option_tab_callback (const gchar *option_name,
                     const gchar *value,
                     gpointer     data,
                     GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;
  const char *profile = NULL;
                
  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;

      iw->tabs = 
      g_list_append (iw->tabs, initial_tab_new (profile, FALSE));
    }
  else
    {
      iw = add_new_window (results, profile, FALSE);
    }

  return TRUE;
}


static gboolean 
option_tab_with_profile_callback (const gchar *option_name,
				  const gchar *value,
				  gpointer     data,
				  GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;
  const gchar *profile = value;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;
      iw->tabs = 
      g_list_append (iw->tabs, initial_tab_new (profile, FALSE));
    }
  else
    iw = add_new_window (results, profile, FALSE);

  return TRUE;
}


static gboolean 
option_window_with_profile_internal_id_callback (const gchar *option_name,
                                                 const gchar *value,
                                                 gpointer     data,
                                                 GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;
  const char *profile = value;

  iw = add_new_window (results, profile, TRUE);

  return TRUE;
}


static gboolean 
option_tab_with_profile_internal_id_callback (const gchar *option_name,
					      const gchar *value,
					      gpointer     data,
					      GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;
  const char *profile = value;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;

      iw->tabs = 
      g_list_append (iw->tabs, initial_tab_new (profile, TRUE));
    }
  else
    iw = add_new_window (results, profile, TRUE);

  return TRUE;
}


static gboolean 
option_role_callback (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;
      iw->role = g_strdup (value);
    }
  else if (!results->default_role)
    results->default_role = g_strdup (value);
  else
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "%s", _("Two roles given for one window"));
      return FALSE;
    }

  return TRUE;
}


static gboolean 
option_show_menubar_callback (const gchar *option_name,
                         const gchar *value,
                         gpointer     data,
                         GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;
      if (iw->force_menubar_state && iw->menubar_state == TRUE)
        {
          g_printerr (_("\"%s\" option given twice for the same window\n"),
                        "--show-menubar");

          return TRUE;
        }

      iw->force_menubar_state = TRUE;
      iw->menubar_state = TRUE;
    }
  else
    {
      results->default_window_menubar_forced = TRUE;
      results->default_window_menubar_state = TRUE;
    }

  return TRUE;
}


static gboolean 
option_hide_menubar_callback (const gchar *option_name,
                              const gchar *value,
                              gpointer     data,
                              GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;
 
      if (iw->force_menubar_state && iw->menubar_state == FALSE)
        {
          g_printerr (_("\"%s\" option given twice for the same window\n"),
                        "--hide-menubar");
          return TRUE;
        }

      iw->force_menubar_state = TRUE;
      iw->menubar_state = FALSE;                
    }
  else
    {
      results->default_window_menubar_forced = TRUE;
      results->default_window_menubar_state = FALSE;
    }

  return TRUE;
}

static gboolean
option_maximize_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;
      iw->start_maximized = TRUE;
    }
  else
    results->default_maximize = TRUE;

  return TRUE;
}

static gboolean
option_fullscreen_callback (const gchar *option_name,
                            const gchar *value,
                            gpointer     data,
                            GError     **error)
{
  OptionParsingResults *results = data;
  InitialWindow *iw;

  if (results->initial_windows)
    {
      iw = g_list_last (results->initial_windows)->data;
      iw->start_fullscreen = TRUE;
    }
  else
    results->default_fullscreen = TRUE;

  return TRUE;
}

static gboolean
option_geometry_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  OptionParsingResults *results = data;

  if (results->initial_windows)
    {
      InitialWindow *iw;

      iw = g_list_last (results->initial_windows)->data;
      iw->geometry = g_strdup (value);
    }
  else
    results->default_geometry = g_strdup (value);

  return TRUE;
}

static gboolean
option_disable_factory_callback (const gchar *option_name,
                                 const gchar *value,
                                 gpointer     data,
                                 GError     **error)
{
  OptionParsingResults *results = data;

  results->use_factory = FALSE;

  return TRUE;
}


static gboolean 
option_title_callback (const gchar *option_name,
                       const gchar *value,
                       gpointer     data,
                       GError     **error)
{
  OptionParsingResults *results = data;
  InitialTab *it = ensure_top_tab (results);

  it->title = g_strdup (value);

  return TRUE;
}


static gboolean 
option_working_directory_callback (const gchar *option_name,
                                   const gchar *value,
                                   gpointer     data,
                                   GError     **error)
{
  OptionParsingResults *results = data;
  InitialTab *it = ensure_top_tab (results);

  it->working_dir = g_strdup (value);

  return TRUE;
}


static gboolean 
option_active_callback (const gchar *option_name,
                        const gchar *value,
                        gpointer     data,
                        GError     **error)
{
  OptionParsingResults *results = data;
  InitialTab *it;

  it = ensure_top_tab (results);
  it->active = TRUE;

  return TRUE;
}


/* Evaluation of the arguments given to the command line options */
static gboolean
digest_options_callback (GOptionContext *context,
                         GOptionGroup *group,
                         gpointer      data,
                         GError      **error)
{
  OptionParsingResults *results = data;
  InitialTab    *it;

  /* make sure we have some window in case no options were given */
  if (results->initial_windows == NULL)
    it = ensure_top_tab (results);

  if (results->execute)
    {         
      if (results->post_execute_args == NULL)
        {
          g_set_error (error,
                       G_OPTION_ERROR,
                       G_OPTION_ERROR_BAD_VALUE,
                       _("Option \"%s\" requires specifying the command to run"
                       " on the rest of the command line\n"),
                       "--execute/-x");
          return FALSE;
        }

      it = ensure_top_tab (results);
      it->exec_argv = results->post_execute_args;
      results->post_execute_args = NULL;
    } 

  if (results->zoom)
    {
      double val;
      char *end;

      it = ensure_top_tab (results);

      /* Try reading a locale-style double first, in case it was
       * typed by a person, then fall back to ascii_strtod (we
       * always save session in C locale format)
       */
      end = NULL;
      errno = 0;
      val = g_strtod (results->zoom, &end);
      if (end == NULL || *end != '\0')
        {
          g_set_error (error,
                        G_OPTION_ERROR,
                        G_OPTION_ERROR_BAD_VALUE,
                        _("\"%s\" is not a valid zoom factor\n"),
                        results->zoom);
          return FALSE;
        }

      if (val < (TERMINAL_SCALE_MINIMUM + 1e-6))
        {
          g_printerr (_("Zoom factor \"%g\" is too small, using %g\n"),
                      val,
                      TERMINAL_SCALE_MINIMUM);
          val = TERMINAL_SCALE_MINIMUM;
        }

      if (val > (TERMINAL_SCALE_MAXIMUM - 1e-6))
        {
          g_printerr (_("Zoom factor \"%g\" is too large, using %g\n"),
                      val, 
                      TERMINAL_SCALE_MAXIMUM);
          val = TERMINAL_SCALE_MAXIMUM;
        }
        
      it->zoom = val;
      it->zoom_set = TRUE;
    }

  return TRUE;
}

static OptionParsingResults *
option_parsing_results_new (int *argc, char **argv)
{
  OptionParsingResults *results;
  int i;

  results = g_slice_new0 (OptionParsingResults);

  results->default_window_menubar_forced = FALSE;
  results->default_window_menubar_state = TRUE;
  results->default_fullscreen = FALSE;
  results->default_maximize = FALSE;
  results->execute = FALSE;
  results->use_factory = TRUE;

  results->startup_id = NULL;
  results->display_name = NULL;
  results->initial_windows = NULL;
  results->default_role = NULL;
  results->default_geometry = NULL;
  results->zoom = NULL;

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
option_parsing_results_free (OptionParsingResults *results)
{
  g_list_foreach (results->initial_windows, (GFunc) initial_window_free, NULL);
  g_list_free (results->initial_windows);

  g_free (results->default_role);
  g_free (results->default_geometry);
  g_free (results->default_working_dir);

  if (results->post_execute_args)
    g_strfreev (results->post_execute_args);

  g_free (results->display_name);
  g_free (results->startup_id);
  g_free (results->zoom);

  g_slice_free (OptionParsingResults, results);
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
              return; /* popt (GOption?) will die on this later, plus it shouldn't happen
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
          end = NULL;
          n = g_ascii_strtoll (argv[i+1], &end, 0);
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

static GdkScreen*
find_screen_by_display_name (const char *display_name,
                             int         screen_number)
{
  GdkDisplay *display = NULL;
  GdkScreen *screen;
  
  /* --screen=screen_number overrides --display */
  
  screen = NULL;
  
  if (display_name == NULL)
    display = gdk_display_get_default ();
  else
    {
      GSList *displays, *l;
      const char *period;
        
      period = strrchr (display_name, '.');
      if (period)
        {
          gulong n;
          char *end;
          
          errno = 0;
          end = NULL;
          n = g_ascii_strtoull (period + 1, &end, 0);
          if (errno == 0 && (period + 1) != end)
            screen_number = n;
        }
      
      displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
      for (l = displays; l != NULL; l = l->next)
        {
          GdkDisplay *disp = l->data;

          /* compare without the screen number part */
          if (strncmp (gdk_display_get_name (disp), display_name, period - display_name) == 0)
            {
              display = disp;
              break;
            }
        }
      g_slist_free (displays);

      if (display == NULL)
        display = gdk_display_open (display_name); /* FIXME we never close displays */
    }

  if (display == NULL)
    return NULL;
  if (screen_number >= 0)
    screen = gdk_display_get_screen (display, screen_number);
  if (screen == NULL)
    screen = gdk_display_get_default_screen (display);

  return screen;
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

static void
new_terminal_with_options (TerminalApp *app,
                           OptionParsingResults *results)
{
  GList *lw;
  GdkScreen *screen;

  screen = find_screen_by_display_name (results->display_name,
                                        results->screen_number);

  for (lw = results->initial_windows;  lw != NULL; lw = lw->next)
    {
      InitialWindow *iw = lw->data;
      TerminalWindow *window;
      GList *lt;

      g_assert (iw->tabs);

      /* Create & setup new window */
      window = terminal_app_new_window (app, screen);

      if (results->startup_id)
        terminal_window_set_startup_id (window, results->startup_id);

      if (iw->role)
        gtk_window_set_role (GTK_WINDOW (window), iw->role);
      else
        terminal_util_set_unique_role (GTK_WINDOW (window), "gnome-terminal");

      if (iw->force_menubar_state)
        terminal_window_set_menubar_visible (window, iw->menubar_state);

      if (iw->start_fullscreen)
        gtk_window_fullscreen (GTK_WINDOW (window));
      if (iw->start_maximized)
        gtk_window_maximize (GTK_WINDOW (window));

      /* Now add the tabs */
      for (lt = iw->tabs; lt != NULL; lt = lt->next)
        {
          InitialTab *it = lt->data;
          TerminalProfile *profile = NULL;
          TerminalScreen *screen;

          if (it->profile)
            {
              if (it->profile_is_id)
                profile = terminal_app_get_profile_by_name (app, it->profile);
              else                
                profile = terminal_app_get_profile_by_visible_name (app, it->profile);

              if (profile == NULL)
                g_printerr (_("No such profile \"%s\", using default profile\n"), it->profile);
            }
          if (profile == NULL)
            profile = terminal_app_get_profile_for_new_term (app);
          g_assert (profile);

          screen = terminal_app_new_terminal (app, window, profile,
                                              it->exec_argv,
                                              it->title,
                                              it->working_dir,
                                              it->zoom_set ? it->zoom : 1.0);
          
          if (it->active)
            terminal_window_switch_screen (window, screen);
        }

      if (iw->geometry)
        {
          if (!gtk_window_parse_geometry (GTK_WINDOW (window), iw->geometry))
            g_printerr (_("Invalid geometry string \"%s\"\n"), iw->geometry);
        }

      gtk_window_present (GTK_WINDOW (window));
    }
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
    const char *name;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
                     RootWindow (xdisplay, 0),
                     -100, -100, 1, 1,
                     0,
                     CopyFromParent,
                     CopyFromParent,
                     (Visual *)CopyFromParent,
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
  GOptionContext *context;
  int i;
  int argc_copy;
  char **argv_copy;
  const char *startup_id;
  const char *display_name;
  GdkDisplay *display;
  GnomeProgram *program;
  OptionParsingResults *parsing_results;

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  argc_copy = argc;
  /* we leave empty slots, for --startup-id, --display and --default-working-directory */
  argv_copy = g_new0 (char *, argc_copy + 7);
  for (i = 0; i < argc_copy; i++)
    argv_copy [i] = g_strdup (argv [i]);
  argv_copy [i] = NULL;

  parsing_results = option_parsing_results_new (&argc, argv);
  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  if (startup_id != NULL && startup_id[0] != '\0')
    {
      parsing_results->startup_id = g_strdup (startup_id);
      g_unsetenv ("DESKTOP_STARTUP_ID");
    }
  
  gtk_window_set_auto_startup_notification (FALSE); /* we'll do it ourselves due
                                                     * to complicated factory setup
                                                     */

  context = get_goption_context (parsing_results);

  /* This automagically makes GOption parse */
  program = gnome_program_init (PACKAGE, VERSION,
                                LIBGNOMEUI_MODULE,
                                argc,
                                argv,
                                GNOME_PARAM_GOPTION_CONTEXT, context,
                                GNOME_PARAM_APP_PREFIX, TERM_PREFIX,
                                GNOME_PARAM_APP_SYSCONFDIR, TERM_SYSCONFDIR,
                                GNOME_PARAM_APP_DATADIR, TERM_PKGDATADIR,
                                GNOME_PARAM_APP_LIBDIR, TERM_LIBDIR,
                                NULL); 

 /* Do this here so that gdk_display is initialized */
  if (parsing_results->startup_id == NULL)
    {
      /* Create a fake one containing a timestamp that we can use */
      Time timestamp;
      timestamp = slowly_and_stupidly_obtain_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

      parsing_results->startup_id = g_strdup_printf ("_TIME%lu", timestamp);
    }

  g_set_application_name (_("Terminal"));
  
  display = gdk_display_get_default ();
  display_name = gdk_display_get_name (display);
  parsing_results->display_name = g_strdup (display_name);
  
  option_parsing_results_apply_directory_defaults (parsing_results);

  if (parsing_results->use_factory)
    {
      char *cwd;
      
      if (parsing_results->startup_id != NULL)
        {
          /* we allocated argv_copy with extra space so we could do this */
          insert_args (&argc_copy, argv_copy,
                       "--startup-id", parsing_results->startup_id);
        }

      /* Forward our display to the child */
      insert_args (&argc_copy, argv_copy,
                   "--display", parsing_results->display_name);
      
      /* Forward out working directory */
      cwd = g_get_current_dir ();
      insert_args (&argc_copy, argv_copy,
                   "--default-working-directory", cwd);
      g_free (cwd);

      if (terminal_invoke_factory (argc_copy, argv_copy))
        {
          g_strfreev (argv_copy);
          option_parsing_results_free (parsing_results);
          return 0;
        }
      /* FIXME: else return 1; ? */
    }

  g_strfreev (argv_copy);
  argv_copy = NULL;

  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);
 
  g_assert (parsing_results->post_execute_args == NULL);

  terminal_app_initialize (parsing_results->use_factory);
  g_signal_connect (terminal_app_get (), "quit", G_CALLBACK (gtk_main_quit), NULL);

  new_terminal_with_options (terminal_app_get (), parsing_results);
  option_parsing_results_free (parsing_results);
  parsing_results = NULL;

  initialization_complete = TRUE;
  handle_new_terminal_events ();
  
  gtk_main ();

  terminal_app_shutdown ();

  if (factory_registered)
    bonobo_activation_active_server_unregister (ACT_IID, BONOBO_OBJREF (listener));
  if (listener)
    bonobo_object_unref (BONOBO_OBJECT (listener));

  g_object_unref (program);

  return 0;
}

/* Factory stuff */

typedef struct
{
  int argc;
  char **argv;
} NewTerminalEvent;

static GOptionContext *
get_goption_context (OptionParsingResults *parsing_results)
{
  GOptionContext *context;
  GOptionGroup *option_group;

  const GOptionEntry goptions[] = {  
    {
      "command",
      'e',
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      option_command_callback,
      N_("Execute the argument to this option inside the terminal."),
      NULL
    },
    {
      "execute",
      'x',
      0,
      G_OPTION_ARG_NONE,
      &parsing_results->execute,
      N_("Execute the remainder of the command line inside the terminal."),
      NULL
    },
    {
      "window",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_window_callback,
      N_("Open a new window containing a tab with the default profile. More than one of these options can be provided."),
      NULL
    },
    {
      "window-with-profile",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_window_with_profile_callback,
      N_("Open a new window containing a tab with the given profile. More than one of these options can be provided."),
      N_("PROFILENAME")
    },
    {
      "tab",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_tab_callback,
      N_("Open a new tab in the last-opened window with the default profile. More than one of these options can be provided."),
      NULL
    },
    {
      "tab-with-profile",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_tab_with_profile_callback,
      N_("Open a new tab in the last-opened window with the given profile. More than one of these options can be provided."),
      N_("PROFILENAME")
    },
    {
      "window-with-profile-internal-id",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_window_with_profile_internal_id_callback,
      N_("Open a new window containing a tab with the given profile ID. Used internally to save sessions."),
      N_("PROFILEID")
    },
    {
      "tab-with-profile-internal-id",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_tab_with_profile_internal_id_callback,
      N_("Open a new tab in the last-opened window with the given profile ID. Used internally to save sessions."),
      N_("PROFILEID")
    },
    {
      "role",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_role_callback,
      N_("Set the role for the last-specified window; applies to only one window; can be specified once for each window you create from the command line."),
      N_("ROLE")
    },
    {
      "show-menubar",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_show_menubar_callback,
      N_("Turn on the menubar for the last-specified window; applies to only one window; can be specified once for each window you create from the command line."),
      NULL
    },
    {
      "hide-menubar",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_hide_menubar_callback,
      N_("Turn off the menubar for the last-specified window; applies to only one window; can be specified once for each window you create from the command line."),
      NULL
    },
    {
      "maximize",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_maximize_callback,
      N_("Set the last-specified window into maximized mode; applies to only one window; can be specified once for each window you create from the command line."),
      NULL
    },
    {
      "full-screen",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_fullscreen_callback,
      N_("Set the last-specified window into fullscreen mode; applies to only one window; can be specified once for each window you create from the command line."),
      NULL
    },
    {
      "geometry",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_geometry_callback,
      N_("X geometry specification (see \"X\" man page), can be specified once per window to be opened."),
      N_("GEOMETRY")
    },
    {
      "disable-factory",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_disable_factory_callback,
      N_("Do not register with the activation nameserver, do not re-use an active terminal"),
      NULL
    },
    {
      "use-factory",
      0,
      0,
      G_OPTION_ARG_NONE,
      &parsing_results->use_factory,
      N_("Register with the activation nameserver [default]"),
      NULL
    },
    {
      "startup-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &parsing_results->startup_id,
      NULL,
      NULL
    },
    {
      "title",
      't',
      0,
      G_OPTION_ARG_CALLBACK,
      option_title_callback,
      N_("Set the terminal's title"),
      N_("TITLE")
    },
    {
      "working-directory",
      0,
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      option_working_directory_callback,
      N_("Set the terminal's working directory"),
      N_("DIRNAME")
    },
    {
      "default-working-directory",
      0,
      0,
      G_OPTION_ARG_FILENAME,
      &parsing_results->default_working_dir,
      N_("Set the default terminal's working directory. Used internally"),
      N_("DIRNAME")
    },
    {
      "zoom",
      0,
      0,
      G_OPTION_ARG_STRING,
      &parsing_results->zoom,
      N_("Set the terminal's zoom factor (1.0 = normal size)"),
      N_("ZOOMFACTOR")
    },
    {
      "active",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_active_callback,
      N_("Set the last specified tab as the active one in its window"),
      NULL
    },
    
    /*
     * Crappy old compat args
     */
    {
      "tclass",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "font",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "nologin",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "login",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "foreground",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "background",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "solid",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "bgscroll",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "bgnoscroll",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "shaded",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "noshaded",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "transparent",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "utmp",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "noutmp",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "wtmp",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "nowtmp",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "lastlog",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "nolastlog",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "icon",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },  
    {
      "termname",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "start-factory-server",
      0,
      G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      NULL
    }
  };

  context = g_option_context_new (N_("GNOME Terminal Emulator"));
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  option_group = g_option_group_new ("gnome-terminal",
                                     N_("GNOME Terminal Emulator"),
                                     N_("Show GNOME Terminal options"),
                                     parsing_results,
                                     NULL);
  g_option_group_add_entries (option_group, goptions);
  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
  g_option_group_set_parse_hooks (option_group, NULL, digest_options_callback);
  g_option_context_set_main_group (context, option_group);

  return context;
}

static void
handle_new_terminal_event (int          argc,
                           char       **argv)
{
  GOptionContext *context;
  OptionParsingResults *parsing_results;
  GError *error = NULL;

  g_assert (initialization_complete);

  parsing_results = option_parsing_results_new (&argc, argv);
  
  /* Find and parse --display */
  option_parsing_results_check_for_display_name (parsing_results,
                                                 &argc,
                                                 argv);
  
  context = get_goption_context (parsing_results);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  if(!g_option_context_parse (context, &argc, &argv, &error))
  {
      g_warning ("Error parsing options: %s, passed from terminal child",
                 error->message);
      g_error_free (error);
      g_option_context_free (context);
      option_parsing_results_free (parsing_results);
      return;
  }
  g_option_context_free (context);

  option_parsing_results_apply_directory_defaults (parsing_results);

  new_terminal_with_options (terminal_app_get (), parsing_results);
  option_parsing_results_free (parsing_results);
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
      g_slice_free (NewTerminalEvent, event);
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
  NewTerminalEvent *event;
  
  if (strcmp (event_name, "new_terminal"))
    {
      g_warning ("Unknown event \"%s\" on terminal",
		 event_name);
      return;
    }

  args = any->_value;
  
  tmp_argv = g_new0 (char*, args->_length + 1);
  for (tmp_argc = 0; tmp_argc < args->_length; ++tmp_argc)
    tmp_argv[tmp_argc] = g_strdup (((const char**)args->_buffer)[tmp_argc]);
  tmp_argv[tmp_argc] = NULL;

  event = g_slice_new0 (NewTerminalEvent);
  event->argc = tmp_argc;
  event->argv = tmp_argv;

  pending_new_terminal_events = g_slist_append (pending_new_terminal_events,
                                                event);

  if (initialization_complete)
    handle_new_terminal_events ();
}

static Bonobo_RegistrationResult
terminal_register_as_factory (void)
{
  char *per_display_iid;
  Bonobo_RegistrationResult result;

  listener = bonobo_listener_new (terminal_new_event, NULL);

  per_display_iid = bonobo_activation_make_registration_id (
    ACT_IID, DisplayString (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ())));

  result = bonobo_activation_active_server_register (
    per_display_iid, BONOBO_OBJREF (listener));

  factory_registered = (result == Bonobo_ACTIVATION_REG_SUCCESS);

  if (!factory_registered)
    {
      bonobo_object_unref (BONOBO_OBJECT (listener));
      listener = NULL;
    }


#ifdef DEBUG_FACTORY
  if (result == Bonobo_ACTIVATION_REG_SUCCESS)
    g_print ("Successfully registered factory \"%s\"\n", per_display_iid);
#endif

  g_free (per_display_iid);

  return result;
}

static gboolean
terminal_invoke_factory (int argc, char **argv)
{
  Bonobo_Listener receiver;

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
#ifdef DEBUG_FACTORY
        g_print ("Factory found; forwarding request\n");
#endif
        /* lets use it then */
        break;
    }

  receiver = bonobo_activation_activate_from_id (
    ACT_IID, Bonobo_ACTIVATION_FLAG_EXISTING_ONLY, NULL, NULL);

  if (receiver != CORBA_OBJECT_NIL)
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
      
      Bonobo_Listener_event (receiver, "new_terminal", &any, &ev);
      CORBA_Object_release (receiver, &ev);
      if (!BONOBO_EX (&ev))
        return TRUE;

      CORBA_exception_free (&ev);
    }
  else
    g_printerr (_("Failed to retrieve terminal server from activation server\n"));

  return FALSE;
}
