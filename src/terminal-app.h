/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2008 Christian Persch
 *
 * This programme is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This programme is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this programme; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include <gtk/gtk.h>

#include "terminal-encoding.h"
#include "terminal-screen.h"

G_BEGIN_DECLS

/* Terminal conf files */

#define TERMINAL_CONFIG_VERSION             (1) /* Bump this for any changes */
#define TERMINAL_CONFIG_COMPAT_VERSION      (1) /* Bump this for incompatible changes */

#define TERMINAL_CONFIG_GROUP               "GNOME Terminal Configuration"
#define TERMINAL_CONFIG_PROP_VERSION        "Version"
#define TERMINAL_CONFIG_PROP_COMPAT_VERSION "CompatVersion"
#define TERMINAL_CONFIG_PROP_WINDOWS        "Windows"

#define TERMINAL_CONFIG_WINDOW_PROP_ACTIVE_TAB       "ActiveTerminal"
#define TERMINAL_CONFIG_WINDOW_PROP_FULLSCREEN       "Fullscreen"
#define TERMINAL_CONFIG_WINDOW_PROP_GEOMETRY         "Geometry"
#define TERMINAL_CONFIG_WINDOW_PROP_MAXIMIZED        "Maximized"
#define TERMINAL_CONFIG_WINDOW_PROP_MENUBAR_VISIBLE  "MenubarVisible"
#define TERMINAL_CONFIG_WINDOW_PROP_ROLE             "Role"
#define TERMINAL_CONFIG_WINDOW_PROP_TABS             "Terminals"

#define TERMINAL_CONFIG_TERMINAL_PROP_HEIGHT             "Height"
#define TERMINAL_CONFIG_TERMINAL_PROP_COMMAND            "Command"
#define TERMINAL_CONFIG_TERMINAL_PROP_PROFILE_ID         "ProfileID"
#define TERMINAL_CONFIG_TERMINAL_PROP_TITLE              "Title"
#define TERMINAL_CONFIG_TERMINAL_PROP_WIDTH              "Width"
#define TERMINAL_CONFIG_TERMINAL_PROP_WORKING_DIRECTORY  "WorkingDirectory"
#define TERMINAL_CONFIG_TERMINAL_PROP_ZOOM               "Zoom"

#define GNOME_TERMINAL_ICON_NAME "utilities-terminal"

#define MONOSPACE_FONT_KEY_NAME                 "monospace-font-name"

/* TerminalApp */

#define TERMINAL_TYPE_APP              (terminal_app_get_type ())
#define TERMINAL_APP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_APP, TerminalApp))
#define TERMINAL_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_APP, TerminalAppClass))
#define TERMINAL_IS_APP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_APP))
#define TERMINAL_IS_APP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_APP))
#define TERMINAL_APP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_APP, TerminalAppClass))

typedef struct _TerminalAppClass TerminalAppClass;
typedef struct _TerminalApp TerminalApp;

GType terminal_app_get_type (void);

TerminalApp* terminal_app_get (void);

void terminal_app_shutdown (void);

GDBusObjectManagerServer *terminal_app_get_object_manager (TerminalApp *app);

void terminal_app_edit_profile (TerminalApp *app,
                                GSettings   *profile,
                                GtkWindow   *transient_parent,
                                const char  *widget_name);

void terminal_app_new_profile (TerminalApp *app,
                               GSettings   *default_base_profile,
                               GtkWindow   *transient_parent);

TerminalWindow * terminal_app_new_window   (TerminalApp *app,
                                            GdkScreen *screen);

TerminalScreen *terminal_app_new_terminal (TerminalApp     *app,
                                           TerminalWindow  *window,
                                           GSettings       *profile,
                                           char           **override_command,
                                           const char      *title,
                                           const char      *working_dir,
                                           char           **child_env,
                                           double           zoom);

TerminalWindow *terminal_app_get_current_window (TerminalApp *app);

void terminal_app_manage_profiles (TerminalApp     *app,
                                   GtkWindow       *transient_parent);

void terminal_app_edit_keybindings (TerminalApp     *app,
                                    GtkWindow       *transient_parent);
void terminal_app_edit_encodings   (TerminalApp     *app,
                                    GtkWindow       *transient_parent);

GList* terminal_app_get_profile_list (TerminalApp *app);

GSettings* terminal_app_get_profile_by_name (TerminalApp *app,
                                             const char  *name);

GSettings* terminal_app_get_profile_by_visible_name (TerminalApp *app,
                                                     const char  *name);

TerminalEncoding *terminal_app_ensure_encoding (TerminalApp *app,
                                                const char *charset);

GHashTable *terminal_app_get_encodings (TerminalApp *app);

GSList* terminal_app_get_active_encodings (TerminalApp *app);

void terminal_app_save_config (TerminalApp *app,
                               GKeyFile *key_file);

gboolean terminal_app_save_config_file (TerminalApp *app,
                                        const char *file_name,
                                        GError **error);

/* GSettings */

GSettings *terminal_app_get_global_settings (TerminalApp *app);

GSettings *terminal_app_get_desktop_interface_settings (TerminalApp *app);

GSettings *terminal_app_get_proxy_settings (TerminalApp *app);

PangoFontDescription *terminal_app_get_system_font (TerminalApp *app);

G_END_DECLS

#endif /* !TERMINAL_APP_H */
