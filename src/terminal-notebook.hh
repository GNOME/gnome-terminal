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

#ifndef TERMINAL_NOTEBOOK_H
#define TERMINAL_NOTEBOOK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_NOTEBOOK (terminal_notebook_get_type ())

G_DECLARE_FINAL_TYPE (TerminalNotebook, terminal_notebook, TERMINAL, NOTEBOOK, GtkWidget)

GtkWidget *terminal_notebook_new (void);

GtkNotebook *terminal_notebook_get_notebook (TerminalNotebook *self);

void terminal_notebook_set_tab_policy (TerminalNotebook *notebook,
                                       GtkPolicyType policy);
GtkPolicyType terminal_notebook_get_tab_policy (TerminalNotebook *notebook);

GtkWidget *terminal_notebook_get_action_box (TerminalNotebook *notebook,
                                             GtkPackType pack_type);

G_END_DECLS

#endif /* TERMINAL_NOTEBOOK_H */
