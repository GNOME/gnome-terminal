/*
 * terminal-close-button.h
 *
 * Copyright © 2010 - Paolo Borelli
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gnome-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TERMINAL_CLOSE_BUTTON_H__
#define __TERMINAL_CLOSE_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_CLOSE_BUTTON			(terminal_close_button_get_type ())
#define TERMINAL_CLOSE_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButton))
#define TERMINAL_CLOSE_BUTTON_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButton const))
#define TERMINAL_CLOSE_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButtonClass))
#define TERMINAL_IS_CLOSE_BUTTON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TERMINAL_TYPE_CLOSE_BUTTON))
#define TERMINAL_IS_CLOSE_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_CLOSE_BUTTON))
#define TERMINAL_CLOSE_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButtonClass))

typedef struct _TerminalCloseButton		TerminalCloseButton;
typedef struct _TerminalCloseButtonPrivate	TerminalCloseButtonPrivate;
typedef struct _TerminalCloseButtonClass	TerminalCloseButtonClass;
typedef struct _TerminalCloseButtonClassPrivate	TerminalCloseButtonClassPrivate;

struct _TerminalCloseButton
{
	GtkButton parent;
};

struct _TerminalCloseButtonClass
{
	GtkButtonClass parent_class;

	TerminalCloseButtonClassPrivate *priv;
};

GType		  terminal_close_button_get_type (void) G_GNUC_CONST;

GtkWidget	 *terminal_close_button_new      (void);

G_END_DECLS

#endif /* __TERMINAL_CLOSE_BUTTON_H__ */
/* ex:set ts=8 noet: */
