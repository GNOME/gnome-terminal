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

#define TERMINAL_TYPE_PROFILE_ROW (terminal_profile_row_get_type())

G_DECLARE_FINAL_TYPE (TerminalProfileRow, terminal_profile_row, TERMINAL, PROFILE_ROW, AdwActionRow)

GtkWidget *terminal_profile_row_new          (GSettings          *settings);
GSettings *terminal_profile_row_get_settings (TerminalProfileRow *self);

G_END_DECLS
