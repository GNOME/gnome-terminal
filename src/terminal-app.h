/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2008 Christian Persch
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

#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include <gtk/gtk.h>

#include "terminal-screen.h"

G_BEGIN_DECLS

#define CONF_PREFIX           "/apps/gnome-terminal"
#define CONF_GLOBAL_PREFIX    CONF_PREFIX "/global"
#define CONF_PROFILES_PREFIX  CONF_PREFIX "/profiles"
#define CONF_KEYS_PREFIX      CONF_PREFIX "/keybindings"

#define GNOME_TERMINAL_ICON_NAME "utilities-terminal"

#define TERMINAL_APP_DEFAULT_PROFILE        "default-profile"
#define TERMINAL_APP_ENABLE_MENU_BAR_ACCEL  "enable-menu-accels"
#define TERMINAL_APP_ENABLE_MNEMONICS       "enable-mnemonics"
#define TERMINAL_APP_SYSTEM_FONT            "system-font"

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

void terminal_app_initialize (gboolean use_factory);

void terminal_app_shutdown (void);

TerminalApp* terminal_app_get (void);

void terminal_app_edit_profile (TerminalApp     *app,
                                TerminalProfile *profile,
                                GtkWindow       *transient_parent);

void terminal_app_new_profile (TerminalApp     *app,
                               TerminalProfile *default_base_profile,
                               GtkWindow       *transient_parent);

TerminalWindow * terminal_app_new_window   (TerminalApp *app,
                                            GdkScreen *screen);

TerminalScreen *terminal_app_new_terminal (TerminalApp     *app,
                                           TerminalWindow  *window,
                                           TerminalProfile *profile,
                                           char           **override_command,
                                           const char      *title,
                                           const char      *working_dir,
                                           double           zoom);

TerminalWindow *terminal_app_get_current_window (TerminalApp *app);

void terminal_app_manage_profiles (TerminalApp     *app,
                                   GtkWindow       *transient_parent);

void terminal_app_edit_keybindings (TerminalApp     *app,
                                    GtkWindow       *transient_parent);
void terminal_app_edit_encodings   (TerminalApp     *app,
                                    GtkWindow       *transient_parent);


GList* terminal_app_get_profile_list (TerminalApp *app);

TerminalProfile* terminal_app_ensure_profile_fallback (TerminalApp *app);

TerminalProfile* terminal_app_get_profile_by_name         (TerminalApp *app,
                                                           const char      *name);

TerminalProfile* terminal_app_get_profile_by_visible_name (TerminalApp *app,
                                                           const char      *name);

/* may return NULL */
TerminalProfile* terminal_app_get_default_profile (TerminalApp *app);

/* never returns NULL if any profiles exist, one is always supposed to */
TerminalProfile* terminal_app_get_profile_for_new_term (TerminalApp *app);

GHashTable *terminal_app_get_encodings (TerminalApp *app);

GSList* terminal_app_get_active_encodings (TerminalApp *app);

G_END_DECLS

#endif /* !TERMINAL_APP_H */
