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

#ifndef TERMINAL_H
#define TERMINAL_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include "terminal-screen.h"

typedef struct _TerminalApp TerminalApp;

TerminalApp* terminal_app_get (void);

void terminal_app_edit_profile (TerminalApp     *app,
                                TerminalProfile *profile,
                                GtkWindow       *transient_parent);

void terminal_app_new_profile (TerminalApp     *app,
                               TerminalProfile *default_base_profile,
                               GtkWindow       *transient_parent);

void terminal_app_new_terminal (TerminalApp     *app,
                                TerminalProfile *profile,
                                TerminalWindow  *window,
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

void terminal_app_manage_profiles (TerminalApp     *app,
                                   GtkWindow       *transient_parent);

void terminal_app_edit_keybindings (TerminalApp     *app,
                                    GtkWindow       *transient_parent);
void terminal_app_edit_encodings   (TerminalApp     *app,
                                    GtkWindow       *transient_parent);

void terminal_util_set_unique_role (GtkWindow *window, const char *prefix);

void terminal_util_show_error_dialog (GtkWindow *transient_parent, 
                                      GtkWidget **weap_ptr, 
                                      const char *message_format, ...) G_GNUC_PRINTF(3, 4);

void terminal_util_show_help (const char *topic, GtkWindow  *transient_parent);

void terminal_util_set_labelled_by          (GtkWidget  *widget,
                                             GtkLabel   *label);
void terminal_util_set_atk_name_description (GtkWidget  *widget,
                                             const char *name,
                                             const char *desc);
GladeXML* terminal_util_load_glade_file (const char *filename,
                                         const char *widget_root,
                                         GtkWindow  *error_dialog_parent);


#endif /* TERMINAL_H */
