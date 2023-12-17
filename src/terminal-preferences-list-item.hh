/* Copyright 2023 Christian Hergert
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

#include <glib-object.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_PREFERENCES_LIST_ITEM (terminal_preferences_list_item_get_type())

G_DECLARE_FINAL_TYPE (TerminalPreferencesListItem, terminal_preferences_list_item, TERMINAL, PREFERENCES_LIST_ITEM, GObject)

GVariant *terminal_preferences_list_item_get_value (TerminalPreferencesListItem const* self);

G_END_DECLS
