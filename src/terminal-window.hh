/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2023 Christian Hergert
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

#pragma once

#include <adwaita.h>

#include "terminal-screen.hh"
#include "terminal-tab.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_WINDOW (terminal_window_get_type ())

G_DECLARE_FINAL_TYPE (TerminalWindow, terminal_window, TERMINAL, WINDOW, AdwApplicationWindow)

TerminalWindow *terminal_window_new                      (GApplication   *app);
int             terminal_window_get_active_screen_num    (TerminalWindow *window);
void            terminal_window_set_titlebar             (TerminalWindow *window,
                                                          GtkWidget      *titlebar);
void            terminal_window_add_tab(TerminalWindow *window,
                                        TerminalTab* tab,
                                        TerminalTab* parent_tab);
void            terminal_window_remove_screen            (TerminalWindow *window,
                                                          TerminalScreen *screen);
void            terminal_window_update_size              (TerminalWindow *window);
void            terminal_window_switch_screen            (TerminalWindow *window,
                                                          TerminalScreen *screen);
TerminalScreen *terminal_window_get_active               (TerminalWindow *window);
TerminalTab*    terminal_window_get_active_tab           (TerminalWindow *window);
GList          *terminal_window_list_tabs   (TerminalWindow *window);
gboolean        terminal_window_parse_geometry           (TerminalWindow *window,
                                                          const char     *geometry);
void            terminal_window_update_geometry          (TerminalWindow *window);
void            terminal_window_request_close            (TerminalWindow *window);
const char     *terminal_window_get_uuid                 (TerminalWindow *window);
gboolean        terminal_window_in_fullscreen_transition (TerminalWindow *window);
bool terminal_window_is_animating (TerminalWindow* window);

G_END_DECLS
