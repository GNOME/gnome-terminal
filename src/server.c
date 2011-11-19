/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#include <config.h>

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-gdbus-generated.h"
#include "terminal-intl.h"
#include "terminal-util.h"
#include "terminal-defines.h"

GDBusObjectManagerServer *object_manager;

typedef struct {
  int exit_code;
} OwnData;

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char *name,
                 gpointer user_data)
{
  TerminalObjectSkeleton *object;

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Bus %s acquired\n", name);

  object = terminal_object_skeleton_new (TERMINAL_FACTORY_OBJECT_PATH);
  terminal_object_skeleton_set_factory (object, TERMINAL_FACTORY (terminal_app_get ()));
  g_dbus_object_manager_server_export (object_manager, G_DBUS_OBJECT_SKELETON (object));
  g_object_unref (object);

  /* And export the object */
  g_dbus_object_manager_server_set_connection (object_manager, connection);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const char *name,
                  gpointer user_data)
{
  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Acquired the name %s on the starter bus\n", name);
}

static void
name_lost_cb (GDBusConnection *connection,
              const char *name,
              gpointer user_data)
{
  OwnData *data = (OwnData *) user_data;

  if (connection) {
    _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                          "Lost the name %s on the starter bus\n", name);
  } else {
    g_printerr ("Failed to connect to starter bus\n");
  }

  data->exit_code = EXIT_FAILURE;

  gtk_main_quit ();
}

int
main (int argc, char **argv)
{
  OwnData data;
  guint owner_id;
  const char *home_dir;
  GError *error = NULL;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_type_init ();

  _terminal_debug_init ();

  // FIXMEchpe: just use / here but make sure #565328 doesn't regress
  /* Change directory to $HOME so we don't prevent unmounting, e.g. if the
   * factory is started by nautilus-open-terminal. See bug #565328.
   * On failure back to /.
   */
  home_dir = g_get_home_dir ();
  if (home_dir == NULL || chdir (home_dir) < 0)
    (void) chdir ("/");

  g_set_application_name (_("Terminal"));

  if (!gtk_init_with_args (&argc, &argv, "", NULL, NULL, &error)) {
    g_printerr ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    exit (EXIT_FAILURE);
  }

  object_manager = g_dbus_object_manager_server_new (TERMINAL_OBJECT_PATH_PREFIX);

  data.exit_code = EXIT_FAILURE;
  owner_id = g_bus_own_name (G_BUS_TYPE_STARTER,
                             TERMINAL_UNIQUE_NAME,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             bus_acquired_cb,
                             name_acquired_cb,
                             name_lost_cb,
                             &data, NULL);

  gtk_main ();

  g_bus_unown_name (owner_id);

  g_dbus_object_manager_server_unexport (object_manager, TERMINAL_FACTORY_OBJECT_PATH);
  if (data.exit_code == EXIT_SUCCESS)
    g_dbus_connection_flush_sync (g_dbus_object_manager_server_get_connection (object_manager),
                                  NULL /* cancellable */, NULL /* error */);
  g_object_unref (object_manager);
  object_manager = NULL;

  terminal_app_shutdown ();

  return data.exit_code;
}
