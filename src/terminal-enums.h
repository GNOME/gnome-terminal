/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
 * Copyright © 2008, 2010 Christian Persch
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

#ifndef TERMINAL_ENUMS_H
#define TERMINAL_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  TERMINAL_TITLE_REPLACE,
  TERMINAL_TITLE_BEFORE,
  TERMINAL_TITLE_AFTER,
  TERMINAL_TITLE_IGNORE
} TerminalTitleMode;

typedef enum 
{
  TERMINAL_EXIT_CLOSE,
  TERMINAL_EXIT_RESTART,
  TERMINAL_EXIT_HOLD
} TerminalExitAction;

G_END_DECLS

#endif /* TERMINAL_ENUMS_H */
