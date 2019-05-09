/*
 * Copyright © 2019 Rodolfo Granata
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

#ifndef TEXT_OBJECTS_H
#define TEXT_OBJECTS_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void profile_text_objects_init(void);
void profile_text_objects_load(GSettings *profile);

G_END_DECLS

#endif /* TEXT_OBJECTS_H */
