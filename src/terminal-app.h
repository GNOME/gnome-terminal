/* application-wide commands */

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

#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include <gtk/gtk.h>

#include "terminal-screen.h"

G_BEGIN_DECLS

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
                                            const char *role,
                                            const char *startup_id,
                                            const char *display_name,
                                            int screen_number);

void terminal_app_new_terminal (TerminalApp     *app,
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
                                int              screen_number);

TerminalWindow *terminal_app_get_current_window (TerminalApp *app);

void terminal_app_manage_profiles (TerminalApp     *app,
                                   GtkWindow       *transient_parent);

void terminal_app_edit_keybindings (TerminalApp     *app,
                                    GtkWindow       *transient_parent);
void terminal_app_edit_encodings   (TerminalApp     *app,
                                    GtkWindow       *transient_parent);


GList* terminal_app_get_profile_list (TerminalApp *app);

guint  terminal_app_get_profile_count (TerminalApp *app);

TerminalProfile* terminal_app_ensure_profile_fallback (TerminalApp *app);

TerminalProfile* terminal_app_get_profile_by_name         (TerminalApp *app,
                                                           const char      *name);

TerminalProfile* terminal_app_get_profile_by_visible_name (TerminalApp *app,
                                                           const char      *name);

/* may return NULL */
TerminalProfile* terminal_app_get_default_profile (TerminalApp *app);

/* never returns NULL if any profiles exist, one is always supposed to */
TerminalProfile* terminal_app_get_profile_for_new_term (TerminalApp *app,
                                                        TerminalProfile *current);

G_END_DECLS

#endif /* !TERMINAL_APP_H */
