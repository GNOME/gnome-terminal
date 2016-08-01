/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "terminal-options.h"
#include "terminal-screen.h"
#include "terminal-app.h"
#include "terminal-util.h"
#include "terminal-version.h"

static GOptionContext *get_goption_context (TerminalOptions *options);

static TerminalSettingsList *
terminal_options_ensure_profiles_list (TerminalOptions *options)
{
  if (options->profiles_list == NULL)
    options->profiles_list = terminal_profiles_list_new ();

  return options->profiles_list;
}

static char *
terminal_util_key_file_get_string_unescape (GKeyFile *key_file,
                                            const char *group,
                                            const char *key,
                                            GError **error)
{
  char *escaped, *unescaped;

  escaped = g_key_file_get_string (key_file, group, key, error);
  if (!escaped)
    return NULL;

  unescaped = g_strcompress (escaped);
  g_free (escaped);

  return unescaped;
}

static char **
terminal_util_key_file_get_argv (GKeyFile *key_file,
                                 const char *group,
                                 const char *key,
                                 int *argc,
                                 GError **error)
{
  char **argv;
  char *flat;
  gboolean retval;

  flat = terminal_util_key_file_get_string_unescape (key_file, group, key, error);
  if (!flat)
    return NULL;

  retval = g_shell_parse_argv (flat, argc, &argv, error);
  g_free (flat);

  if (retval)
    return argv;

  return NULL;
}

static InitialTab*
initial_tab_new (char *profile /* adopts */)
{
  InitialTab *it;

  it = g_slice_new (InitialTab);

  it->profile = profile;
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
initial_window_new (guint source_tag)
{
  InitialWindow *iw;

  iw = g_slice_new0 (InitialWindow);
  iw->source_tag = source_tag;

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
apply_defaults (TerminalOptions *options,
                InitialWindow        *iw)
{
  if (options->default_role)
    {
      iw->role = options->default_role;
      options->default_role = NULL;
    }

  if (iw->geometry == NULL)
    iw->geometry = g_strdup (options->default_geometry);

  if (options->default_window_menubar_forced)
    {
      iw->force_menubar_state = TRUE;
      iw->menubar_state = options->default_window_menubar_state;

      options->default_window_menubar_forced = FALSE;
    }

  iw->start_fullscreen |= options->default_fullscreen;
  iw->start_maximized |= options->default_maximize;
}

static InitialWindow*
ensure_top_window (TerminalOptions *options)
{
  InitialWindow *iw;

  if (options->initial_windows == NULL)
    {
      iw = initial_window_new (0);
      iw->tabs = g_list_append (NULL, initial_tab_new (NULL));
      apply_defaults (options, iw);

      options->initial_windows = g_list_append (options->initial_windows, iw);
    }
  else
    {
      iw = g_list_last (options->initial_windows)->data;
    }

  g_assert (iw->tabs);

  return iw;
}

static InitialTab*
ensure_top_tab (TerminalOptions *options)
{
  InitialWindow *iw;
  InitialTab *it;

  iw = ensure_top_window (options);

  g_assert (iw->tabs);

  it = g_list_last (iw->tabs)->data;

  return it;
}

static InitialWindow*
add_new_window (TerminalOptions *options,
                char            *profile /* adopts */)
{
  InitialWindow *iw;

  iw = initial_window_new (0);
  iw->tabs = g_list_prepend (NULL, initial_tab_new (profile));
  apply_defaults (options, iw);

  options->initial_windows = g_list_append (options->initial_windows, iw);

  return iw;
}

/* handle deprecated command line options */

static void
deprecated_option_warning (const gchar *option_name)
{
  g_printerr (_("Option \"%s\" is deprecated and might be removed in a later version of gnome-terminal."),
              option_name);
  g_printerr ("\n");
}

static gboolean
unsupported_option_callback (const gchar *option_name,
                             const gchar *value,
                             gpointer     data,
                             GError     **error)
{
  g_printerr (_("Option \"%s\" is no longer supported in this version of gnome-terminal."),
              option_name);
  g_printerr ("\n");
  return TRUE; /* we do not want to bail out here but continue */
}

static gboolean
unsupported_option_fatal_callback (const gchar *option_name,
                                   const gchar *value,
                                   gpointer     data,
                                   GError     **error)
{
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
               _("Option \"%s\" is no longer supported in this version of gnome-terminal."),
               option_name);
  return FALSE;
}


static gboolean G_GNUC_NORETURN
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", _("GNOME Terminal"), VERSION);

  exit (EXIT_SUCCESS);
}

static gboolean
option_app_id_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  TerminalOptions *options = data;

  if (!g_application_id_is_valid (value)) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "\"%s\" is not a valid application ID", value);
    return FALSE;
  }

  g_free (options->server_app_id);
  options->server_app_id = g_strdup (value);

  return TRUE;
}

static gboolean
option_command_callback (const gchar *option_name,
                         const gchar *value,
                         gpointer     data,
                         GError     **error)
{
  TerminalOptions *options = data;
  GError *err = NULL;
  char  **exec_argv;

  if (!g_shell_parse_argv (value, NULL, &exec_argv, &err))
    {
      g_set_error(error,
                  G_OPTION_ERROR,
                  G_OPTION_ERROR_BAD_VALUE,
                  _("Argument to \"%s\" is not a valid command: %s"),
                   "--command/-e",
                  err->message);
      g_error_free (err);
      return FALSE;
    }

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_strfreev (it->exec_argv);
      it->exec_argv = exec_argv;
    }
  else
    {
      g_strfreev (options->exec_argv);
      options->exec_argv = exec_argv;
    }

  return TRUE;
}

static gboolean
option_profile_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  TerminalOptions *options = data;
  char *profile;

  profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                     value, error);
  if (profile == NULL)
  {
      g_printerr ("Profile '%s' specified but not found. Attempting to fall back "
                  "to the default profile.\n", value);
      g_clear_error (error);
      profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                         NULL, error);
  }

  if (profile == NULL)
      return FALSE;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->profile);
      it->profile = profile;
    }
  else
    {
      g_free (options->default_profile);
      options->default_profile = profile;
    }

  return TRUE;
}

static gboolean
option_profile_id_cb (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  TerminalOptions *options = data;
  char *profile;

  profile = terminal_profiles_list_dup_uuid (terminal_options_ensure_profiles_list (options),
                                             value, error);
  if (profile == NULL)
    return FALSE;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->profile);
      it->profile = profile;
    }
  else
    {
      g_free (options->default_profile);
      options->default_profile = profile;
    }

  return TRUE;
}


static gboolean
option_window_callback (const gchar *option_name,
                        const gchar *value,
                        gpointer     data,
                        GError     **error)
{
  TerminalOptions *options = data;
  char *profile;

  profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                     value, error);

  if (value && profile == NULL)
  {
      g_printerr ("Profile '%s' specified but not found. Attempting to fall back "
                  "to the default profile.\n", value);
      g_clear_error (error);
      profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                         NULL, error);
  }

  if (profile == NULL)
    return FALSE;

  add_new_window (options, profile /* adopts */);

  return TRUE;
}

static gboolean
option_tab_callback (const gchar *option_name,
                     const gchar *value,
                     gpointer     data,
                     GError     **error)
{
  TerminalOptions *options = data;
  char *profile;

  profile = terminal_profiles_list_dup_uuid_or_name (terminal_options_ensure_profiles_list (options),
                                                     value, error);
  if (profile == NULL)
    return FALSE;

  if (options->initial_windows)
    {
      InitialWindow *iw;

      iw = g_list_last (options->initial_windows)->data;
      iw->tabs = g_list_append (iw->tabs, initial_tab_new (profile /* adopts */));
    }
  else
    add_new_window (options, profile /* adopts */);

  return TRUE;
}

static gboolean
option_role_callback (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  TerminalOptions *options = data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = g_list_last (options->initial_windows)->data;
      iw->role = g_strdup (value);
    }
  else if (!options->default_role)
    options->default_role = g_strdup (value);
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
  TerminalOptions *options = data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = g_list_last (options->initial_windows)->data;
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
      options->default_window_menubar_forced = TRUE;
      options->default_window_menubar_state = TRUE;
    }

  return TRUE;
}

static gboolean
option_hide_menubar_callback (const gchar *option_name,
                              const gchar *value,
                              gpointer     data,
                              GError     **error)
{
  TerminalOptions *options = data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = g_list_last (options->initial_windows)->data;

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
      options->default_window_menubar_forced = TRUE;
      options->default_window_menubar_state = FALSE;
    }

  return TRUE;
}

static gboolean
option_maximize_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  TerminalOptions *options = data;
  InitialWindow *iw;

  if (options->initial_windows)
    {
      iw = g_list_last (options->initial_windows)->data;
      iw->start_maximized = TRUE;
    }
  else
    options->default_maximize = TRUE;

  return TRUE;
}

static gboolean
option_fullscreen_callback (const gchar *option_name,
                            const gchar *value,
                            gpointer     data,
                            GError     **error)
{
  TerminalOptions *options = data;

  if (options->initial_windows)
    {
      InitialWindow *iw;

      iw = g_list_last (options->initial_windows)->data;
      iw->start_fullscreen = TRUE;
    }
  else
    options->default_fullscreen = TRUE;

  return TRUE;
}

static gboolean
option_geometry_callback (const gchar *option_name,
                          const gchar *value,
                          gpointer     data,
                          GError     **error)
{
  TerminalOptions *options = data;

  if (options->initial_windows)
    {
      InitialWindow *iw;

      iw = g_list_last (options->initial_windows)->data;
      iw->geometry = g_strdup (value);
    }
  else
    options->default_geometry = g_strdup (value);

  return TRUE;
}

static gboolean
option_load_config_cb (const gchar *option_name,
                       const gchar *value,
                       gpointer     data,
                       GError     **error)
{
  TerminalOptions *options = data;
  GFile *file;
  char *config_file;
  GKeyFile *key_file;
  gboolean result;

  file = g_file_new_for_commandline_arg (value);
  config_file = g_file_get_path (file);
  g_object_unref (file);

  key_file = g_key_file_new ();
  result = g_key_file_load_from_file (key_file, config_file, 0, error) &&
           terminal_options_merge_config (options, key_file,
                                          strcmp (option_name, "load-config") == 0 ? SOURCE_DEFAULT : SOURCE_SESSION,
                                          error);
  g_key_file_free (key_file);
  g_free (config_file);

  return result;
}

static gboolean
option_title_callback (const gchar *option_name,
                       const gchar *value,
                       gpointer     data,
                       GError     **error)
{
  TerminalOptions *options = data;

  deprecated_option_warning (option_name);

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->title);
      it->title = g_strdup (value);
    }
  else
    {
      g_free (options->default_title);
      options->default_title = g_strdup (value);
    }

  return TRUE;
}

static gboolean
option_working_directory_callback (const gchar *option_name,
                                   const gchar *value,
                                   gpointer     data,
                                   GError     **error)
{
  TerminalOptions *options = data;

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);

      g_free (it->working_dir);
      it->working_dir = g_strdup (value);
    }
  else
    {
      g_free (options->default_working_dir);
      options->default_working_dir = g_strdup (value);
    }

  return TRUE;
}

static gboolean
option_active_callback (const gchar *option_name,
                        const gchar *value,
                        gpointer     data,
                        GError     **error)
{
  TerminalOptions *options = data;
  InitialTab *it;

  it = ensure_top_tab (options);
  it->active = TRUE;

  return TRUE;
}

static gboolean
option_zoom_callback (const gchar *option_name,
                      const gchar *value,
                      gpointer     data,
                      GError     **error)
{
  TerminalOptions *options = data;
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
                   _("\"%s\" is not a valid zoom factor"),
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

  if (options->initial_windows)
    {
      InitialTab *it = ensure_top_tab (options);
      it->zoom = zoom;
      it->zoom_set = TRUE;
    }
  else
    {
      options->zoom = zoom;
      options->zoom_set = TRUE;
    }

  return TRUE;
}

/* Evaluation of the arguments given to the command line options */
static gboolean
digest_options_callback (GOptionContext *context,
                         GOptionGroup *group,
                         gpointer      data,
                         GError      **error)
{
  TerminalOptions *options = data;
  InitialTab    *it;

  if (options->execute)
    {
      if (options->exec_argv == NULL)
        {
          g_set_error (error,
                       G_OPTION_ERROR,
                       G_OPTION_ERROR_BAD_VALUE,
                       _("Option \"%s\" requires specifying the command to run"
                         " on the rest of the command line"),
                       "--execute/-x");
          return FALSE;
        }

      /* Apply -x/--execute command only to the first tab */
      it = ensure_top_tab (options);
      it->exec_argv = options->exec_argv;
      options->exec_argv = NULL;
    }

  return TRUE;
}

/**
 * terminal_options_parse:
 * @working_directory: the default working directory
 * @startup_id: the startup notification ID
 * @argcp: (inout) address of the argument count. Changed if any arguments were handled
 * @argvp: (inout) address of the argument vector. Any parameters understood by
 *   the terminal #GOptionContext are removed
 * @error: a #GError to fill in
 *
 * Parses the argument vector *@argvp.
 *
 * Returns: a new #TerminalOptions containing the windows and tabs to open,
 *   or %NULL on error.
 */
TerminalOptions *
terminal_options_parse (const char *working_directory,
                        const char *startup_id,
                        int *argcp,
                        char ***argvp,
                        GError **error)
{
  TerminalOptions *options;
  GOptionContext *context;
  gboolean retval;
  int i;
  char **argv = *argvp;

  options = g_slice_new0 (TerminalOptions);

  options->remote_arguments = FALSE;
  options->default_window_menubar_forced = FALSE;
  options->default_window_menubar_state = TRUE;
  options->default_fullscreen = FALSE;
  options->default_maximize = FALSE;
  options->execute = FALSE;

  options->startup_id = g_strdup (startup_id && startup_id[0] ? startup_id : NULL);
  options->display_name = NULL;
  options->initial_windows = NULL;
  options->default_role = NULL;
  options->default_geometry = NULL;
  options->default_title = NULL;
  options->zoom = 1.0;
  options->zoom_set = FALSE;

  options->screen_number = -1;
  options->default_working_dir = g_strdup (working_directory);

  /* The old -x/--execute option is broken, so we need to pre-scan for it. */
  /* We now also support passing the command after the -- switch. */
  options->exec_argv = NULL;
  for (i = 1 ; i < *argcp; ++i)
    {
      gboolean is_execute;
      gboolean is_dashdash;
      int j, last;

      is_execute = strcmp (argv[i], "-x") == 0 || strcmp (argv[i], "--execute") == 0;
      is_dashdash = strcmp (argv[i], "--") == 0;

      if (!is_execute && !is_dashdash)
        continue;

      options->execute = is_execute;

      /* Skip the switch */
      last = i;
      ++i;
      if (i == *argcp)
        break; /* we'll complain about this later for -x/--execute; it's fine for -- */

      /* Collect the args, and remove them from argv */
      options->exec_argv = g_new0 (char*, *argcp - i + 1);
      for (j = 0; i < *argcp; ++i, ++j)
        options->exec_argv[j] = g_strdup (argv[i]);
      options->exec_argv[j] = NULL;

      *argcp = last;
      break;
    }

  context = get_goption_context (options);
  retval = g_option_context_parse (context, argcp, argvp, error);
  g_option_context_free (context);

  if (retval)
    return options;

  terminal_options_free (options);
  return NULL;
}

/**
 * terminal_options_merge_config:
 * @options:
 * @key_file: a #GKeyFile containing to merge the options from
 * @source_tag: a source_tag to use in new #InitialWindow<!-- -->s
 * @error: a #GError to fill in
 *
 * Merges the saved options from @key_file into @options.
 *
 * Returns: %TRUE if @key_file was a valid key file containing a stored
 *   terminal configuration, or %FALSE on error
 */
gboolean
terminal_options_merge_config (TerminalOptions *options,
                               GKeyFile *key_file,
                               guint source_tag,
                               GError **error)
{
  int version, compat_version;
  char **groups;
  guint i;
  gboolean have_error = FALSE;
  GList *initial_windows = NULL;

  if (!g_key_file_has_group (key_file, TERMINAL_CONFIG_GROUP))
    {
      g_set_error_literal (error, TERMINAL_OPTION_ERROR,
                           TERMINAL_OPTION_ERROR_INVALID_CONFIG_FILE,
                           _("Not a valid terminal config file."));
      return FALSE;
    }
  
  version = g_key_file_get_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_VERSION, NULL);
  compat_version = g_key_file_get_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_COMPAT_VERSION, NULL);

  if (version <= 0 ||
      compat_version <= 0 ||
      compat_version > TERMINAL_CONFIG_COMPAT_VERSION)
    {
      g_set_error_literal (error, TERMINAL_OPTION_ERROR,
                           TERMINAL_OPTION_ERROR_INCOMPATIBLE_CONFIG_FILE,
                           _("Incompatible terminal config file version."));
      return FALSE;
    }

  groups = g_key_file_get_string_list (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_WINDOWS, NULL, error);
  if (!groups)
    return FALSE;

  for (i = 0; groups[i]; ++i)
    {
      const char *window_group = groups[i];
      char *active_terminal;
      char **tab_groups;
      InitialWindow *iw;
      guint j;

      tab_groups = g_key_file_get_string_list (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_TABS, NULL, error);
      if (!tab_groups)
        continue; /* no tabs in this window, skip it */

      iw = initial_window_new (source_tag);
      initial_windows = g_list_append (initial_windows, iw);
      apply_defaults (options, iw);

      active_terminal = g_key_file_get_string (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_ACTIVE_TAB, NULL);
      iw->role = g_key_file_get_string (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_ROLE, NULL);
      iw->geometry = g_key_file_get_string (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_GEOMETRY, NULL);
      iw->start_fullscreen = g_key_file_get_boolean (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_FULLSCREEN, NULL);
      iw->start_maximized = g_key_file_get_boolean (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_MAXIMIZED, NULL);
      if (g_key_file_has_key (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_MENUBAR_VISIBLE, NULL))
        {
          iw->force_menubar_state = TRUE;
          iw->menubar_state = g_key_file_get_boolean (key_file, window_group, TERMINAL_CONFIG_WINDOW_PROP_MENUBAR_VISIBLE, NULL);
        }

      for (j = 0; tab_groups[j]; ++j)
        {
          const char *tab_group = tab_groups[j];
          InitialTab *it;
          char *profile;

          profile = g_key_file_get_string (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_PROFILE_ID, NULL);
          it = initial_tab_new (profile /* adopts */);

          iw->tabs = g_list_append (iw->tabs, it);

          if (g_strcmp0 (active_terminal, tab_group) == 0)
            it->active = TRUE;

/*          it->width = g_key_file_get_integer (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_WIDTH, NULL);
          it->height = g_key_file_get_integer (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_HEIGHT, NULL);*/
          it->working_dir = terminal_util_key_file_get_string_unescape (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_WORKING_DIRECTORY, NULL);
          it->title = g_key_file_get_string (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_TITLE, NULL);

          if (g_key_file_has_key (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_COMMAND, NULL) &&
              !(it->exec_argv = terminal_util_key_file_get_argv (key_file, tab_group, TERMINAL_CONFIG_TERMINAL_PROP_COMMAND, NULL, error)))
            {
              have_error = TRUE;
              break;
            }
        }

      g_free (active_terminal);
      g_strfreev (tab_groups);

      if (have_error)
        break;
    }

  g_strfreev (groups);

  if (have_error)
    {
      g_list_foreach (initial_windows, (GFunc) initial_window_free, NULL);
      g_list_free (initial_windows);
      return FALSE;
    }

  options->initial_windows = g_list_concat (options->initial_windows, initial_windows);

  return TRUE;
}

/**
 * terminal_options_ensure_window:
 * @options:
 *
 * Ensure that @options will contain at least one window to open.
 */
void
terminal_options_ensure_window (TerminalOptions *options)
{
  ensure_top_window (options);
}

/**
 * terminal_options_free:
 * @options:
 *
 * Frees @options.
 */
void
terminal_options_free (TerminalOptions *options)
{
  g_list_foreach (options->initial_windows, (GFunc) initial_window_free, NULL);
  g_list_free (options->initial_windows);

  g_free (options->default_role);
  g_free (options->default_geometry);
  g_free (options->default_working_dir);
  g_free (options->default_title);
  g_free (options->default_profile);

  g_strfreev (options->exec_argv);

  g_free (options->display_name);
  g_free (options->startup_id);
  g_free (options->server_app_id);

  g_free (options->sm_client_id);
  g_free (options->sm_config_prefix);

  g_clear_object (&options->profiles_list);

  g_slice_free (TerminalOptions, options);
}

static GOptionContext *
get_goption_context (TerminalOptions *options)
{
  const GOptionEntry global_unique_goptions[] = {
    {
      "app-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_app_id_callback,
      "Server application ID",
      "ID"
    },
    {
      "disable-factory",
      0,
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_fatal_callback,
      N_("Do not register with the activation nameserver, do not re-use an active terminal"),
      NULL
    },
    {
      "load-config",
      0,
      G_OPTION_FLAG_FILENAME,
      G_OPTION_ARG_CALLBACK,
      option_load_config_cb,
      N_("Load a terminal configuration file"),
      N_("FILE")
    },
    {
      "save-config",
      0,
      G_OPTION_FLAG_FILENAME | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    { "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
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
      N_("Maximize the window"),
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
      N_("Set the window size; for example: 80x24, or 80x24+200+200 (COLSxROWS+X+Y)"),
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
      "profile",
      0,
      0,
      G_OPTION_ARG_CALLBACK,
      option_profile_cb,
      N_("Use the given profile instead of the default profile"),
      N_("PROFILE-NAME")
    },
    {
      "title",
      't',
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_title_callback,
      N_("Set the initial terminal title"),
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
      N_("Set the terminal's zoom factor (1.0 = normal size)"),
      N_("ZOOM")
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry internal_goptions[] = {  
    {
      "profile-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_profile_id_cb,
      NULL, NULL
    },
    {
      "window-with-profile",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_window_callback,
      NULL, NULL
    },
    {
      "tab-with-profile",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_tab_callback,
      NULL, NULL
    },
    {
      "window-with-profile-internal-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_window_callback,
      NULL, NULL
    },
    {
      "tab-with-profile-internal-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      option_tab_callback,
      NULL, NULL
    },
    {
      "default-working-directory",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_FILENAME,
      &options->default_working_dir,
      NULL, NULL,
    },
    {
      "use-factory",
      0,
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      unsupported_option_callback,
      NULL, NULL
    },
    {
      "startup-id",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &options->startup_id,
      NULL,
      NULL
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry smclient_goptions[] = {
    { "sm-client-disable",    0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &options->sm_client_disable,    NULL, NULL },
    { "sm-client-state-file", 0, G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, option_load_config_cb, NULL, NULL },
    { "sm-client-id",         0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,   &options->sm_client_id,         NULL, NULL },
    { "sm-disable",           0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,     &options->sm_client_disable,    NULL, NULL },
    { "sm-config-prefix",     0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,   &options->sm_config_prefix,     NULL, NULL },
    { NULL }
  };

  GOptionContext *context;
  GOptionGroup *group;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_description (context, N_("GNOME Terminal Emulator"));
  g_option_context_set_ignore_unknown_options (context, FALSE);

  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  group = g_option_group_new ("gnome-terminal",
                              N_("GNOME Terminal Emulator"),
                              N_("Show GNOME Terminal options"),
                              options,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_unique_goptions);
  g_option_group_add_entries (group, internal_goptions);
  g_option_group_set_parse_hooks (group, NULL, digest_options_callback);
  g_option_context_set_main_group (context, group);

  group = g_option_group_new ("terminal",
                              N_("Options to open new windows or terminal tabs; more than one of these may be specified:"),
                              N_("Show terminal options"),
                              options,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_multiple_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("window-options",
                              N_("Window options; if used before the first --window or --tab argument, sets the default for all windows:"),
                              N_("Show per-window options"),
                              options,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, window_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("terminal-options",
                              N_("Terminal options; if used before the first --window or --tab argument, sets the default for all terminals:"),
                              N_("Show per-terminal options"),
                              options,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, terminal_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("sm-client", "", "", options, NULL);
  g_option_group_add_entries (group, smclient_goptions);
  g_option_context_add_group (context, group);

  return context;
}
