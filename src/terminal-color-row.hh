/*
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

#include <adwaita.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_COLOR_ROW (terminal_color_row_get_type())

G_DECLARE_FINAL_TYPE (TerminalColorRow, terminal_color_row, TERMINAL, COLOR_ROW, AdwActionRow)

const GdkRGBA *terminal_color_row_get_color (TerminalColorRow *self);
void           terminal_color_row_set_color (TerminalColorRow *self,
                                             const GdkRGBA    *color);

G_END_DECLS
