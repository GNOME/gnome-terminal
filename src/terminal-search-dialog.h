/*
 * Copyright © 2005 Paolo Maggi
 * Copyright © 2010 Red Hat (Red Hat author: Behdad Esfahbod)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TERMINAL_SEARCH_DIALOG_H
#define TERMINAL_SEARCH_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum _TerminalSearchFlags {
  TERMINAL_SEARCH_FLAG_BACKWARDS	= 1 << 0,
  TERMINAL_SEARCH_FLAG_WRAP_AROUND	= 1 << 1
} TerminalSearchFlags;


GtkWidget	*terminal_search_dialog_new		(GtkWindow   *parent);

void		 terminal_search_dialog_present		(GtkWidget   *dialog);

void		 terminal_search_dialog_set_search_text (GtkWidget   *dialog,
							 const gchar *text);

const gchar 	*terminal_search_dialog_get_search_text	(GtkWidget   *dialog);

TerminalSearchFlags
		 terminal_search_dialog_get_search_flags(GtkWidget   *dialog);
GRegex		*terminal_search_dialog_get_regex	(GtkWidget   *dialog);

G_END_DECLS

#endif /* TERMINAL_SEARCH_DIALOG_H */
