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

#undef G_DISABLE_SINGLE_INCLUDES

#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-window.h"
#include "terminal-util.h"
#include "profile-editor.h"
#include "encoding.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <gdk/gdkx.h>

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif

#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#define TERMINAL_FACTORY_SERVICE_NAME   "org.gnome.Terminal.Factory"
#define TERMINAL_FACTORY_SERVICE_PATH   "/org/gnome/Terminal/Factory"
#define TERMINAL_FACTORY_INTERFACE_NAME "org.gnome.Terminal.Factory"

#define TERMINAL_TYPE_FACTORY             (terminal_factory_get_type ())
#define TERMINAL_FACTORY(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_FACTORY, TerminalFactory))
#define TERMINAL_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_FACTORY, TerminalFactoryClass))
#define TERMINAL_IS_FACTORY(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_FACTORY))
#define TERMINAL_IS_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_FACTORY))
#define TERMINAL_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_FACTORY, TerminalFactoryClass))

typedef struct _TerminalFactory        TerminalFactory;
typedef struct _TerminalFactoryClass   TerminalFactoryClass;
typedef struct _TerminalFactoryPrivate TerminalFactoryPrivate;

struct _TerminalFactory
{
  GObject parent_instance;
};

struct _TerminalFactoryClass
{
  GObjectClass parent_class;
};

static gboolean
terminal_factory_new_terminal (TerminalFactory *factory,
                               const char *working_directory,
                               const char *display_name,
                               const char *startup_id,
                               const char **argv,
                               GError **error);

#include "terminal-factory-client.h"
#include "terminal-factory-server.h"

static void
terminal_factory_class_init (TerminalFactoryClass *factory_class)
{
}

static void
terminal_factory_init (TerminalFactory *factory)
{
}

static GType terminal_factory_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TerminalFactory, terminal_factory, G_TYPE_OBJECT,
  dbus_g_object_type_install_info (g_define_type_id,
                                   &dbus_glib_terminal_factory_object_info)
);
 
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

static TerminalFactory *factory = NULL;

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
  double    zoom;
} OptionParsingResults;

static GOptionContext * get_goption_context (OptionParsingResults *parsing_results);

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

static gboolean
option_zoom_callback (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  OptionParsingResults *results = data;
  double zoom;
  char *end;

  /* Try reading a locale-style double first, in case it was
    * typed by a person, then fall back to ascii_strtod (we
    * always save session in C locale format)
    */
  end = NULL;
  errno = 0;
  zoom = g_strtod (value, &end);
  if (end == NULL || *end != '\0')
    {
      g_set_error (error,
                   G_OPTION_ERROR,
                   G_OPTION_ERROR_BAD_VALUE,
                   _("\"%s\" is not a valid zoom factor\n"),
                   value);
      return FALSE;
    }

  if (zoom < (TERMINAL_SCALE_MINIMUM + 1e-6))
    {
      g_printerr (_("Zoom factor \"%g\" is too small, using %g\n"),
                  zoom,
                  TERMINAL_SCALE_MINIMUM);
      zoom = TERMINAL_SCALE_MINIMUM;
    }

  if (zoom > (TERMINAL_SCALE_MAXIMUM - 1e-6))
    {
      g_printerr (_("Zoom factor \"%g\" is too large, using %g\n"),
                  zoom,
                  TERMINAL_SCALE_MAXIMUM);
      zoom = TERMINAL_SCALE_MAXIMUM;
    }

  if (results->initial_windows)
    {
      InitialTab *it = ensure_top_tab (results);
      it->zoom = zoom;
      it->zoom_set = TRUE;
    }
  else
    results->zoom = zoom;

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

  return TRUE;
}

static OptionParsingResults *
option_parsing_results_new (const char *working_directory,
                            const char *display_name,
                            const char *startup_id,
                            int *argc,
                            char **argv)
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

  results->startup_id = g_strdup (startup_id);
  results->display_name = g_strdup (display_name);
  results->initial_windows = NULL;
  results->default_role = NULL;
  results->default_geometry = NULL;
  results->zoom = 1.0;

  results->screen_number = -1;
  results->default_working_dir = g_strdup (working_directory);

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

  g_strfreev (results->post_execute_args);

  g_free (results->display_name);
  g_free (results->startup_id);

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
              return; /* option parsing will die on this later, plus it shouldn't happen
                       * because normally gtk_init() parses --display
                       * when not using factory mode.
                       */
            }

          g_assert (i+1 < *argc);
          g_free (results->display_name);
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

      /* Overwrite the default, unique window role set in terminal_window_init */
      if (iw->role)
        gtk_window_set_role (GTK_WINDOW (window), iw->role);

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
                                              it->zoom_set ? it->zoom : results->zoom);

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
  char **argv_copy;
  const char *startup_id;
  const char *display_name;
  GdkDisplay *display;
  OptionParsingResults *parsing_results;
  DBusGConnection *connection;
  DBusGProxy *proxy;
  guint32 request_name_ret;
  GError *error = NULL;

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Make a NULL-terminated copy since we may need it later */
  argv_copy = g_new (char *, argc + 1);
  for (i = 0; i < argc; ++i)
    argv_copy [i] = argv [i];
  argv_copy [i] = NULL;

  parsing_results = option_parsing_results_new (NULL, NULL, NULL, &argc, argv);
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
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  g_option_context_add_group (context, egg_sm_client_get_option_group ());

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr (_("Failed to parse arguments: %s\n"), error->message);
      g_error_free (error);
      g_option_context_free (context);
      exit (1);
    }

  g_option_context_free (context);
  g_set_application_name (_("Terminal"));
  
 /* Do this here so that gdk_display is initialized */
  if (parsing_results->startup_id == NULL)
    {
      /* Create a fake one containing a timestamp that we can use */
      Time timestamp;
      timestamp = slowly_and_stupidly_obtain_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

      parsing_results->startup_id = g_strdup_printf ("_TIME%lu", timestamp);
    }

  display = gdk_display_get_default ();
  display_name = gdk_display_get_name (display);
  parsing_results->display_name = g_strdup (display_name);
  
  option_parsing_results_apply_directory_defaults (parsing_results);

  if (!parsing_results->use_factory)
    goto factory_disabled;

  /* Now try to acquire register us as the terminal factory */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (!connection)
    {
      g_printerr ("Failed to get the session bus: %s\nFalling back to non-factory mode.\n",
                  error->message);
      g_error_free (error);
      goto factory_disabled;
    }

  proxy = dbus_g_proxy_new_for_name (connection,
                                     DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_DBUS);
#if 0
  dbus_g_proxy_add_signal (proxy, "NameOwnerChanged",
                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "NameOwnerChanged",
                               G_CALLBACK (name_owner_changed), factory, NULL);
#endif

  if (!org_freedesktop_DBus_request_name (proxy,
                                          TERMINAL_FACTORY_SERVICE_NAME,
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &request_name_ret,
                                          &error))
    {
      g_printerr ("Failed name request: %s\n", error->message);
      g_error_free (error);
      goto factory_disabled;
    }

  /* Forward to the existing factory and exit */
  if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      int ret = EXIT_SUCCESS;

      proxy = dbus_g_proxy_new_for_name (connection,
                                         TERMINAL_FACTORY_SERVICE_NAME,
                                         TERMINAL_FACTORY_SERVICE_PATH,
                                         TERMINAL_FACTORY_INTERFACE_NAME);
      if (!org_gnome_Terminal_Factory_new_terminal (proxy,
                                                    g_get_current_dir (),
                                                    parsing_results->display_name,
                                                    parsing_results->startup_id,
                                                    (const char **) argv_copy,
                                                    &error))
        {
          g_printerr ("Failed to forward request to factory: %s\n", error->message);
          g_error_free (error);
          ret = EXIT_FAILURE;
        }

      g_free (argv_copy);
      option_parsing_results_free (parsing_results);

      exit (ret);
    }

  factory = g_object_new (TERMINAL_TYPE_FACTORY, NULL);
  dbus_g_connection_register_g_object (connection,
                                       TERMINAL_FACTORY_SERVICE_PATH,
                                       G_OBJECT (factory));

  /* Now we're registered as the factory. Proceed to open the terminal(s). */

factory_disabled:
  g_free (argv_copy);

  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);

  g_assert (parsing_results->post_execute_args == NULL);

  terminal_app_initialize (parsing_results->use_factory);
  g_signal_connect (terminal_app_get (), "quit", G_CALLBACK (gtk_main_quit), NULL);

  new_terminal_with_options (terminal_app_get (), parsing_results);
  option_parsing_results_free (parsing_results);

  gtk_main ();

  terminal_app_shutdown ();

  if (factory)
    g_object_unref (factory);

  return 0;
}

/* Factory stuff */

typedef struct
{
  char *working_directory;
  char *display_name;
  char *startup_id;
  int argc;
  char **argv;
} NewTerminalEvent;

static GOptionContext *
get_goption_context (OptionParsingResults *parsing_results)
{
  const GOptionEntry global_unique_goptions[] = {
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
      "execute",
      'x',
      0,
      G_OPTION_ARG_NONE,
      &parsing_results->execute,
      N_("Execute the remainder of the command line inside the terminal"),
      NULL
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry global_multiple_goptions[] = {
    {
      "window",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_window_callback,
      N_("Open a new window containing a tab with the default profile"),
      NULL
    },
    {
      "tab",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_tab_callback,
      N_("Open a new tab in the last-opened window with the default profile"),
      NULL
    },
    {
      "window-with-profile",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_window_with_profile_callback,
      N_("Open a new window containing a tab with the given profile"),
      N_("PROFILE-NAME")
    },
    {
      "tab-with-profile",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_tab_with_profile_callback,
      N_("Open a new tab in the last-opened window with the given profile"),
      N_("PROFILE-NAME")
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry window_goptions[] = {
    {
      "show-menubar",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_show_menubar_callback,
      N_("Turn on the menubar"),
      NULL
    },
    {
      "hide-menubar",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_hide_menubar_callback,
      N_("Turn off the menubar"),
      NULL
    },
    {
      "maximize",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_maximize_callback,
      N_("Maximise the window"),
      NULL
    },
    {
      "full-screen",
      0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK,
      option_fullscreen_callback,
      N_("Full-screen the window"),
      NULL
    },
    {
      "geometry",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_geometry_callback,
      N_("Set the window geometry from the provided X geometry specification; see the \"X\" man page for more information"),
      N_("GEOMETRY")
    },
    {
      "role",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_role_callback,
      N_("Set the window role"),
      N_("ROLE")
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry terminal_goptions[] = {
    {
      "command",
      'e',
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      option_command_callback,
      N_("Execute the argument to this option inside the terminal"),
      NULL
    },
    {
      "title",
      't',
      0,
      G_OPTION_ARG_CALLBACK,
      option_title_callback,
      N_("Set the terminal title"),
      N_("TITLE")
    },
    {
      "working-directory",
      0,
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      option_working_directory_callback,
      N_("Set the working directory"),
      N_("DIRNAME")
    },
    {
      "zoom",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_zoom_callback,
      N_("Set the terminalx's zoom factor (1.0 = normal size)"),
      N_("ZOOM")
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
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry internal_goptions[] = {  
    {
      "window-with-profile-internal-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_window_with_profile_internal_id_callback,
      NULL, NULL
    },
    {
      "tab-with-profile-internal-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_tab_with_profile_internal_id_callback,
      NULL, NULL
    },
    {
      "default-working-directory",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_FILENAME,
      &parsing_results->default_working_dir,
      NULL, NULL,
    },
    {
      "use-factory",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_NONE,
      &parsing_results->use_factory,
      NULL, NULL
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
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  GOptionContext *context;
  GOptionGroup *group;

  context = g_option_context_new (N_("GNOME Terminal Emulator"));
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);

  group = g_option_group_new ("gnome-terminal",
                              N_("GNOME Terminal Emulator"),
                              N_("Show GNOME Terminal options"),
                              parsing_results,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_unique_goptions);
  g_option_group_add_entries (group, internal_goptions);
  g_option_group_set_parse_hooks (group, NULL, digest_options_callback);
  g_option_context_set_main_group (context, group);

  group = g_option_group_new ("terminal",
                              N_("Options to open new windows or terminal tabs; more than one of these may be specified:"),
                              N_("Show terminal options"),
                              parsing_results,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_multiple_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("window-options",
                              N_("Window options; if used before the first --window or --tab argument, sets the default for all windows:"),
                              N_("Show per-window options"),
                              parsing_results,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, window_goptions);
  g_option_context_add_group (context, group);
  
  group = g_option_group_new ("terminal-options",
                              N_("Terminal options; if used before the first --window or --tab argument, sets the default for all terminals:"),
                              N_("Show per-terminal options"),
                              parsing_results,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, terminal_goptions);
  g_option_context_add_group (context, group);
  
  return context;
}

static gboolean
handle_new_terminal_event (NewTerminalEvent *event)
{
  GOptionContext *context;
  OptionParsingResults *parsing_results;
  GError *error = NULL;
  int argc = event->argc;
  char **argv = event->argv;

  parsing_results = option_parsing_results_new (event->working_directory,
                                                event->display_name,
                                                event->startup_id,
                                                &argc, argv);

  /* Find and parse --display */
  option_parsing_results_check_for_display_name (parsing_results, &argc, argv);

  context = get_goption_context (parsing_results);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_warning ("Error parsing options: %s, passed from terminal child",
                 error->message);
      g_error_free (error);
      g_option_context_free (context);
      option_parsing_results_free (parsing_results);

      return FALSE;
    }
  g_option_context_free (context);

  option_parsing_results_apply_directory_defaults (parsing_results);

  new_terminal_with_options (terminal_app_get (), parsing_results);
  option_parsing_results_free (parsing_results);

  return FALSE;
}

static void
new_terminal_event_free (NewTerminalEvent *event)
{
  g_free (event->working_directory);
  g_free (event->display_name);
  g_free (event->startup_id);
  g_strfreev (event->argv);
  g_slice_free (NewTerminalEvent, event);
}

static gboolean
terminal_factory_new_terminal (TerminalFactory *factory,
                               const char *working_directory,
                               const char *display_name,
                               const char *startup_id,
                               const char **argv,
                               GError **error)
{
  NewTerminalEvent *event;

  event = g_slice_new0 (NewTerminalEvent);
  event->working_directory = g_strdup (working_directory);
  event->display_name = g_strdup (display_name);
  event->startup_id = g_strdup (startup_id);
  event->argc = g_strv_length ((char **) argv);
  event->argv = g_strdupv ((char **) argv);

  g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                   (GSourceFunc) handle_new_terminal_event,
                   event,
                   (GDestroyNotify) new_terminal_event_free);

  return TRUE;
}
