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

G_BEGIN_DECLS

#define TERMINAL_TYPE_NOTEBOOK (terminal_notebook_get_type ())

G_DECLARE_FINAL_TYPE (TerminalNotebook, terminal_notebook, TERMINAL, NOTEBOOK, GtkWidget)

GtkWidget      *terminal_notebook_new                    (void);
void            terminal_notebook_set_tab_policy         (TerminalNotebook *notebook,
                                                          GtkPolicyType     policy);
GtkPolicyType   terminal_notebook_get_tab_policy         (TerminalNotebook *notebook);
void            terminal_notebook_add_screen             (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen,
                                                          int               position);
void            terminal_notebook_remove_screen          (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen);
TerminalScreen *terminal_notebook_get_active_screen      (TerminalNotebook *notebook);
void            terminal_notebook_set_active_screen      (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen);
void            terminal_notebook_set_active_screen_num  (TerminalNotebook *notebook,
                                                          int               position);
GList          *terminal_notebook_list_screens           (TerminalNotebook *notebook);
GList          *terminal_notebook_list_screen_containers (TerminalNotebook *notebook);
int             terminal_notebook_get_n_screens          (TerminalNotebook *notebook);
int             terminal_notebook_get_active_screen_num  (TerminalNotebook *notebook);
void            terminal_notebook_reorder_screen         (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen,
                                                          int               new_position);
void            terminal_notebook_change_screen          (TerminalNotebook *notebook,
                                                          int               change);
void            terminal_notebook_confirm_close          (TerminalNotebook *notebook,
                                                          TerminalScreen   *screen,
                                                          gboolean          confirm);
AdwTabView     *terminal_notebook_get_tab_view           (TerminalNotebook *notebook);

G_END_DECLS
