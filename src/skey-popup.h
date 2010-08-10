/*
 * Copyright © 2002 Jonathan Blandford <jrb@redhat.com>
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

#ifndef SKEY_POPUP_H
#define SKEY_POPUP_H

#include <gtk/gtk.h>

#include "terminal-screen.h"

G_BEGIN_DECLS

void terminal_skey_do_popup (GtkWindow *window,
                             TerminalScreen *screen,
			     const gchar    *skey_match);

G_END_DECLS

#endif /* SKEY_POPUP_H */
