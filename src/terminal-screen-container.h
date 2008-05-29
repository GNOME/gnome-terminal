/*
 * Copyright © 2008 Christian Persch
*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#ifndef TERMINAL_SCREEN_CONTAINER_H
#define TERMINAL_SCREEN_CONTAINER_H

#include <gtk/gtk.h>
#include "terminal-screen.h"

G_BEGIN_DECLS

GtkWidget *terminal_screen_container_new (TerminalScreen *screen);

TerminalScreen *terminal_screen_container_get_screen (GtkWidget *container);

void terminal_screen_container_set_policy (GtkWidget *container,
                                           GtkPolicyType hpolicy,
                                           GtkPolicyType vpolicy);

void terminal_screen_container_set_placement (GtkWidget *container,
                                              GtkCornerType corner);

G_END_DECLS

#endif /* TERMINAL_SCREEN_CONTAINER_H */
