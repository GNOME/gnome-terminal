/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#ifndef TERMINAL_OPTIONS_H
#define TERMINAL_OPTIONS_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  gboolean remote_arguments;
  char   **env;
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
  char    *default_title;
  char   **exec_argv;
  char    *default_profile;
  gboolean default_profile_is_id;

  gboolean  execute;
  gboolean  use_factory;
  double    zoom;

  char    *config_file;
  gboolean load_config;
  gboolean save_config;
} TerminalOptions;

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
  guint source_tag;

  GList *tabs; /* list of InitialTab */

  gboolean force_menubar_state;
  gboolean menubar_state;

  gboolean start_fullscreen;
  gboolean start_maximized;

  char *geometry;
  char *role;

} InitialWindow;

#define TERMINAL_OPTION_ERROR (g_quark_from_static_string ("terminal-option-error"))

typedef enum {
  TERMINAL_OPTION_ERROR_NOT_IN_FACTORY,
  TERMINAL_OPTION_ERROR_EXCLUSIVE_OPTIONS,
  TERMINAL_OPTION_ERROR_INVALID_CONFIG_FILE,
  TERMINAL_OPTION_ERROR_INCOMPATIBLE_CONFIG_FILE
} TerminalOptionError;

TerminalOptions *terminal_options_parse (const char *working_directory,
                                         const char *display_name,
                                         const char *startup_id,
                                         char **env,
                                         gboolean remote_arguments,
                                         gboolean ignore_unknown_options,
                                         int *argcp,
                                         char ***argvp,
                                         GError **error,
                                         ...) G_GNUC_NULL_TERMINATED;

gboolean terminal_options_merge_config (TerminalOptions *options,
                                        GKeyFile *key_file,
                                        guint source_tag,
                                        GError **error);

void terminal_options_ensure_window (TerminalOptions *options);

void terminal_options_free (TerminalOptions *options);

G_END_DECLS

#endif /* !TERMINAL_OPTIONS_H */
