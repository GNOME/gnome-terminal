/*
 *  Copyright © 2011 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope controller it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TERMINAL_CONTROLLER_H
#define TERMINAL_CONTROLLER_H

#include <glib-object.h>

#include "terminal-gdbus-generated.h"
#include "terminal-screen.h"

G_BEGIN_DECLS

#define TERMINAL_TYPE_CONTROLLER         (terminal_controller_get_type ())
#define TERMINAL_CONTROLLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_CONTROLLER, TerminalController))
#define TERMINAL_CONTROLLER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_CONTROLLER, TerminalControllerClass))
#define TERMINAL_IS_CONTROLLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_CONTROLLER))
#define TERMINAL_IS_CONTROLLER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_CONTROLLER))
#define TERMINAL_CONTROLLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_CONTROLLER, TerminalControllerClass))

typedef struct _TerminalController        TerminalController;
typedef struct _TerminalControllerClass   TerminalControllerClass;
typedef struct _TerminalControllerPrivate TerminalControllerPrivate;

struct _TerminalController
{
  TerminalReceiverSkeleton parent_instance;

  /*< private >*/
  TerminalControllerPrivate *priv;
};

struct _TerminalControllerClass
{
  TerminalReceiverSkeletonClass parent_class;
};

GType terminal_controller_get_type (void);

TerminalController *terminal_controller_new (TerminalScreen *screen);

TerminalScreen *terminal_controller_get_screen (TerminalController *controller);

void _terminal_controller_unset_screen (TerminalController *controller);

G_END_DECLS

#endif /* !TERMINAL_CONTROLLER_H */
