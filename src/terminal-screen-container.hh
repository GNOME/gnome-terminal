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

#define TERMINAL_TYPE_SCREEN_CONTAINER (terminal_screen_container_get_type())

G_DECLARE_FINAL_TYPE (TerminalScreenContainer, terminal_screen_container, TERMINAL, SCREEN_CONTAINER, GtkWidget)

GtkWidget *terminal_screen_container_new (TerminalScreen *screen);

TerminalScreen *terminal_screen_container_get_screen (TerminalScreenContainer *container);
void terminal_screen_container_destroy (TerminalScreenContainer *container);

TerminalScreenContainer *terminal_screen_container_get_from_screen (TerminalScreen *screen);

void terminal_screen_container_set_policy (TerminalScreenContainer *container,
                                           GtkPolicyType hpolicy,
                                           GtkPolicyType vpolicy);

void terminal_screen_container_add_overlay (TerminalScreenContainer *container,
                                            GtkWidget *child);
void terminal_screen_container_remove_overlay (TerminalScreenContainer *container,
                                               GtkWidget *child);

G_END_DECLS
