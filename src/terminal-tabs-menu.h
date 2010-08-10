/*
 *  Copyright © 2003 David Bordoley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef TERMINAL_TABS_MENU_H
#define TERMINAL_TABS_MENU_H

#include "terminal-window.h"

G_BEGIN_DECLS

#define TERMINAL_TYPE_TABS_MENU		(terminal_tabs_menu_get_type ())
#define TERMINAL_TABS_MENU(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_TABS_MENU, TerminalTabsMenu))
#define TERMINAL_TABS_MENU_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_TABS_MENU, TerminalTabsMenuClass))
#define TERMINAL_IS_TABS_MENU(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_TABS_MENU))
#define TERMINAL_IS_TABS_MENU_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_TABS_MENU))
#define TERMINAL_TABS_MENU_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_TABS_MENU, TerminalTabsMenuClass))

typedef struct _TerminalTabsMenu TerminalTabsMenu;
typedef struct _TerminalTabsMenuClass TerminalTabsMenuClass;
typedef struct _TerminalTabsMenuPrivate TerminalTabsMenuPrivate;

struct _TerminalTabsMenuClass
{
	GObjectClass parent_class;
};

struct _TerminalTabsMenu
{
	GObject parent_object;

	/*< private >*/
	TerminalTabsMenuPrivate *priv;
};

GType		terminal_tabs_menu_get_type		(void);

TerminalTabsMenu   *terminal_tabs_menu_new		(TerminalWindow *window);

G_END_DECLS

#endif
