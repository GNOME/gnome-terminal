/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib.h>

#include "terminal-debug.hh"

TerminalDebugFlags _terminal_debug_flags;

void
_terminal_debug_init(void)
{
#ifdef ENABLE_DEBUG
  const GDebugKey keys[] = {
    { "accels",        TERMINAL_DEBUG_ACCELS        },
    { "clipboard",     TERMINAL_DEBUG_CLIPBOARD     },
    { "encodings",     TERMINAL_DEBUG_ENCODINGS     },
    { "server",        TERMINAL_DEBUG_SERVER        },
    { "geometry",      TERMINAL_DEBUG_GEOMETRY      },
    { "mdi",           TERMINAL_DEBUG_MDI           },
    { "processes",     TERMINAL_DEBUG_PROCESSES     },
    { "profile",       TERMINAL_DEBUG_PROFILE       },
    { "settings-list", TERMINAL_DEBUG_SETTINGS_LIST },
    { "search",        TERMINAL_DEBUG_SEARCH        },
    { "bridge",        TERMINAL_DEBUG_BRIDGE        },
    { "default",       TERMINAL_DEBUG_DEFAULT       },
    { "focus",         TERMINAL_DEBUG_FOCUS         },
  };

  _terminal_debug_flags = TerminalDebugFlags(g_parse_debug_string (g_getenv ("GNOME_TERMINAL_DEBUG"),
								   keys, G_N_ELEMENTS (keys)));

#endif /* ENABLE_DEBUG */
}

#ifdef ENABLE_DEBUG

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

#include <gtk/gtk.h>
#include "terminal-libgsystem.hh"

static char*
object_to_string(void* object)
{
  if (!object)
    return g_strdup("(nil)");

  return g_strdup_printf("%s(%p)", G_OBJECT_TYPE_NAME(object), object);
}

static void
focus_notify_cb(GtkWindow* window,
                GParamSpec* pspec,
                void* user_data)
{
  gs_free auto window_str = object_to_string(window);
  gs_free auto focus_str = object_to_string(gtk_window_get_focus(window));
  g_printerr("Focus %s focus-widget %s\n", window_str, focus_str);
}

void
_terminal_debug_attach_focus_listener(void* widget)
{
  g_return_if_fail(GTK_IS_WINDOW(widget));

  g_signal_connect(widget, "notify::focus-widget", G_CALLBACK(focus_notify_cb), nullptr);
}

#endif // TERMINAL_SERVER || TERMINAL_PREFERENCES

#endif // ENABLE_DEBUG
