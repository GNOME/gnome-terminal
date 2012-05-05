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

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-gdbus.h"
#include "terminal-intl.h"
#include "terminal-defines.h"

static char *bus_name = NULL;

static gboolean
option_bus_name_cb (const gchar *option_name,
                    const gchar *value,
                    gpointer     data,
                    GError     **error)
{
  if (!g_dbus_is_name (value)) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "%s is not a valid D-Bus name", value);
    return FALSE;
  }

  g_free (bus_name);
  bus_name = g_strdup (value);

  return TRUE;
}

static const GOptionEntry options[] = {
  { "bus-name", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_bus_name_cb, "Server D-Bus name", "NAME" },
  { NULL }
};

int
main (int argc, char **argv)
{
  GApplication *app;
  int exit_code = EXIT_FAILURE;
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

  if (!gtk_init_with_args (&argc, &argv, "", options, NULL, &error)) {
    g_printerr ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    exit (EXIT_FAILURE);
  }

  app = terminal_app_new (bus_name);
  g_free (bus_name);

  if (!g_application_register (app, NULL, &error)) {
    g_printerr ("Failed to register application: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  if (g_application_get_is_remote (app)) {
    /* How the fuck did this happen? */
    g_printerr ("Cannot be remote instance!\n");
    goto out;
  }

  exit_code = g_application_run (app, 0, NULL);

out:
  terminal_app_shutdown ();

  return exit_code;
}
