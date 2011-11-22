/*
 *  Copyright (C) 2004, 2005 Free Software Foundation, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Christian Neumair <chris@gnome-de.org>
 */

#ifndef TERMINAL_NAUTILUS_H
#define TERMINAL_NAUTILUS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_NAUTILUS         (terminal_nautilus_get_type ())
#define TERMINAL_NAUTILUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_NAUTILUS, TerminalNautilus))
#define TERMINAL_TERMINAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_NAUTILUS, TerminalNautilusClass))
#define TERMINAL_IS_NAUTILUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_NAUTILUS))
#define TERMINAL_IS_TERMINAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_NAUTILUS))
#define TERMINAL_TERMINAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_NAUTILUS, TerminalNautilusClass))

typedef struct _TerminalNautilus      TerminalNautilus;
typedef struct _TerminalNautilusClass TerminalNautilusClass;

struct _TerminalNautilus {
	GObject parent_instance;
};

struct _TerminalNautilusClass {
	GObjectClass parent_class;
};

GType terminal_nautilus_get_type      (void);

void  terminal_nautilus_register_type (GTypeModule *module);

G_END_DECLS

#endif /* TERMINAL_NAUTILUS_H */
