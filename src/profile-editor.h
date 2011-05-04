/*
 * Copyright © 2002 Havoc Pennington
 *
 * This programme is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This programme is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this programme; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TERMINAL_PROFILE_EDITOR_H
#define TERMINAL_PROFILE_EDITOR_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void terminal_profile_edit (GSettings  *profile,
                            GtkWindow  *transient_parent,
                            const char *widget_name);

G_END_DECLS

#endif /* TERMINAL_PROFILE_EDITOR_H */
