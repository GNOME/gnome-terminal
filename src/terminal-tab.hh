/*
 * Copyright © 2008, 2010 Christian Persch
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

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_TAB (terminal_tab_get_type())

G_DECLARE_FINAL_TYPE (TerminalTab, terminal_tab, TERMINAL, TAB, GtkWidget)

GtkWidget *terminal_tab_new (TerminalScreen *screen);

TerminalScreen *terminal_tab_get_screen (TerminalTab *tab);

void terminal_tab_destroy (TerminalTab *tab);

TerminalTab *terminal_tab_get_from_screen (TerminalScreen *screen);

void terminal_tab_set_policy (TerminalTab *tab,
                              GtkPolicyType hpolicy,
                              GtkPolicyType vpolicy);

void terminal_tab_add_overlay (TerminalTab *tab,
                               GtkWidget *child);
void terminal_tab_remove_overlay (TerminalTab *tab,
                                  GtkWidget *child);

G_END_DECLS
