/*
 * Copyright © 2001 Havoc Pennington
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TERMINAL_ACCELS_H
#define TERMINAL_ACCELS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void terminal_accels_init (void);
 
void terminal_accels_shutdown (void);

void terminal_edit_keys_dialog_show (GtkWindow *transient_parent);

G_END_DECLS

#endif /* TERMINAL_ACCELS_H */
