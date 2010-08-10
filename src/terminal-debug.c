/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <glib.h>

#include "terminal-debug.h"

TerminalDebugFlags _terminal_debug_flags;

void
_terminal_debug_init(void)
{
#ifdef GNOME_ENABLE_DEBUG
  const GDebugKey keys[] = {
    { "accels",    TERMINAL_DEBUG_ACCELS    },
    { "encodings", TERMINAL_DEBUG_ENCODINGS },
    { "factory",   TERMINAL_DEBUG_FACTORY   },
    { "geometry",  TERMINAL_DEBUG_GEOMETRY  },
    { "mdi",       TERMINAL_DEBUG_MDI       },
    { "processes", TERMINAL_DEBUG_PROCESSES },
    { "profile",   TERMINAL_DEBUG_PROFILE   }
  };

  _terminal_debug_flags = g_parse_debug_string (g_getenv ("GNOME_TERMINAL_DEBUG"),
                                                keys, G_N_ELEMENTS (keys));
#endif /* GNOME_ENABLE_DEBUG */
}

