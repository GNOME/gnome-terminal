/* Accelerator stuff */

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

#ifndef TERMINAL_ACCELS_H
#define TERMINAL_ACCELS_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

void terminal_accels_init (GConfClient *conf);
GtkAccelGroup* terminal_accels_get_group_for_widget (GtkWidget *widget);

#define ACCEL_PATH_ROOT "<terminal-accels>/menu"
#define ACCEL_PATH_NEW_TAB ACCEL_PATH_ROOT"/new_tab"
#define ACCEL_PATH_NEW_WINDOW ACCEL_PATH_ROOT"/new_window"
#define ACCEL_PATH_NEW_PROFILE ACCEL_PATH_ROOT"/new_profile"
#define ACCEL_PATH_CLOSE_TAB ACCEL_PATH_ROOT"/close_tab"
#define ACCEL_PATH_CLOSE_WINDOW ACCEL_PATH_ROOT"/close_window"
#define ACCEL_PATH_COPY ACCEL_PATH_ROOT"/copy"
#define ACCEL_PATH_PASTE ACCEL_PATH_ROOT"/paste"
#define ACCEL_PATH_TOGGLE_MENUBAR ACCEL_PATH_ROOT"/toggle_menubar"
#define ACCEL_PATH_FULL_SCREEN ACCEL_PATH_ROOT"/full_screen"
#define ACCEL_PATH_RESET ACCEL_PATH_ROOT"/reset"
#define ACCEL_PATH_RESET_AND_CLEAR ACCEL_PATH_ROOT"/reset_and_clear"
#define ACCEL_PATH_PREV_TAB ACCEL_PATH_ROOT"/prev_tab"
#define ACCEL_PATH_NEXT_TAB ACCEL_PATH_ROOT"/next_tab"
#define ACCEL_PATH_SET_TERMINAL_TITLE ACCEL_PATH_ROOT"/set_terminal_title"
#define ACCEL_PATH_HELP ACCEL_PATH_ROOT"/help"
#define ACCEL_PATH_ZOOM_IN ACCEL_PATH_ROOT"/zoom_in"
#define ACCEL_PATH_ZOOM_OUT ACCEL_PATH_ROOT"/zoom_out"
#define ACCEL_PATH_ZOOM_NORMAL ACCEL_PATH_ROOT"/zoom_normal"

#define FORMAT_ACCEL_PATH_SWITCH_TO_TAB ACCEL_PATH_ROOT"/switch_to_tab_%d"
#define PREFIX_ACCEL_PATH_SWITCH_TO_TAB ACCEL_PATH_ROOT"/switch_to_tab_"
#define N_TABS_WITH_ACCEL 11

GtkWidget* terminal_edit_keys_dialog_new (GtkWindow *transient_parent);

#endif /* TERMINAL_ACCELS_H */
