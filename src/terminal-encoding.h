/*
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2017 Christian Persch
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

#ifndef TERMINAL_ENCODING_H
#define TERMINAL_ENCODING_H

#include <gtk/gtk.h>

gboolean terminal_encodings_is_known_charset (const char *charset);

void terminal_encodings_append_menu (GMenu *menu);

GtkListStore *terminal_encodings_list_store_new (int column_id,
                                                 int column_text);

#endif /* TERMINAL_ENCODING_H */
