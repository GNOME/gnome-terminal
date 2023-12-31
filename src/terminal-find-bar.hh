/*
 * terminal-find-bar.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define TERMINAL_TYPE_FIND_BAR (terminal_find_bar_get_type())

G_DECLARE_FINAL_TYPE (TerminalFindBar, terminal_find_bar, TERMINAL, FIND_BAR, GtkWidget)

TerminalScreen *terminal_find_bar_get_screen (TerminalFindBar  *self);
void            terminal_find_bar_set_screen (TerminalFindBar  *self,
                                              TerminalScreen *screen);

G_END_DECLS
