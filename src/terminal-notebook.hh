/*
 * Copyright © 2008, 2010, 2012 Christian Persch
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

#define TERMINAL_TYPE_NOTEBOOK (terminal_notebook_get_type ())

G_DECLARE_FINAL_TYPE (TerminalNotebook, terminal_notebook, TERMINAL, NOTEBOOK, GtkWidget)

GtkWidget      *terminal_notebook_new                    (void);
void            terminal_notebook_insert_tab(TerminalNotebook *notebook,
                                             TerminalTab* tab,
                                             TerminalTab* parent_tab,
                                             bool pinned);
void            terminal_notebook_append_tab(TerminalNotebook *notebook,
                                             TerminalTab* tab,
                                             bool pinned);
void            terminal_notebook_remove_screen          (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen);
TerminalScreen *terminal_notebook_get_active_screen      (TerminalNotebook *notebook);
TerminalTab*    terminal_notebook_get_active_tab         (TerminalNotebook *notebook);
void            terminal_notebook_set_active_screen      (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen);
void            terminal_notebook_set_active_screen_num  (TerminalNotebook *notebook,
                                                          int               position);
GList          *terminal_notebook_list_screens           (TerminalNotebook *notebook);
GList          *terminal_notebook_list_tabs (TerminalNotebook *notebook);
int             terminal_notebook_get_n_screens          (TerminalNotebook *notebook);
int             terminal_notebook_get_active_screen_num  (TerminalNotebook *notebook);
void            terminal_notebook_reorder_screen         (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen,
                                                          int               direction);
void            terminal_notebook_reorder_screen_limits  (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen,
                                                          int               direction);
void            terminal_notebook_reorder_tab            (TerminalNotebook* notebook,
                                                          TerminalTab* tab,
                                                          int direction);
void            terminal_notebook_reorder_tab_limits     (TerminalNotebook* notebook,
                                                          TerminalTab*      tab,
                                                          int direction);
void            terminal_notebook_change_screen          (TerminalNotebook *notebook,
                                                          int               change);
void            terminal_notebook_close_tab(TerminalNotebook* notebook,
                                            TerminalTab* tab);
void            terminal_notebook_confirm_close          (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen,
                                                          gboolean          confirm);
AdwTabView     *terminal_notebook_get_tab_view           (TerminalNotebook *notebook);

void terminal_notebook_transfer_screen(TerminalNotebook* notebook,
                                       TerminalScreen* screen,
                                       TerminalNotebook* new_notebook,
                                       int position);

void terminal_notebook_set_tab_pinned(TerminalNotebook* notebook,
                                      TerminalTab* tab,
                                      bool pinned);

void terminal_notebook_get_tab_actions(TerminalNotebook* notebook,
                                       TerminalTab* tab,
                                       bool* can_switch_left,
                                       bool* can_switch_right,
                                       bool* can_reorder_left,
                                       bool* can_reorder_right,
                                       bool* can_reorder_start,
                                       bool* can_reorder_end,
                                       bool* can_close,
                                       bool* can_detach);
