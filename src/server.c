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

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-options.h"
#include "terminal-util.h"

#define TERMINAL_FACTORY_SERVICE_NAME_PREFIX  "org.gnome.Terminal.Factory0.Display"
#define TERMINAL_FACTORY_SERVICE_PATH         "/org/gnome/Terminal/Factory"
#define TERMINAL_FACTORY_INTERFACE_NAME       "org.gnome.Terminal.Factory"

static char *
ay_to_string (GVariant *variant,
              GError **error)
{
  gsize len;
  const char *data;

  data = g_variant_get_fixed_array (variant, &len, sizeof (char));
  if (len == 0)
    return NULL;

  /* Make sure there are no embedded NULs */
  if (memchr (data, '\0', len) != NULL) {
    g_set_error_literal (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                         "String is shorter than claimed");
    return NULL;
  }

  return g_strndup (data, len);
}

static char **
ay_to_strv (GVariant *variant,
            int *argc)
{
  GPtrArray *argv;
  const char *data, *nullbyte;
  gsize data_len;
  gssize len;

  data = g_variant_get_fixed_array (variant, &data_len, sizeof (char));
  if (data_len == 0 || data_len > G_MAXSSIZE) {
    if (argc)
      *argc = 0;

    return NULL;
  }

  argv = g_ptr_array_new ();

  len = data_len;
  do {
    gssize string_len;

    nullbyte = memchr (data, '\0', len);

    string_len = nullbyte ? (gssize) (nullbyte - data) : len;
    g_ptr_array_add (argv, g_strndup (data, string_len));

    len -= string_len + 1;
    data += string_len + 1;
  } while (len > 0);

  if (argc)
    *argc = argv->len;

  /* NULL terminate */
  g_ptr_array_add (argv, NULL);
  return (char **) g_ptr_array_free (argv, FALSE);
}

typedef struct {
  char *factory_name;
  int exit_code;
} OwnData;

static void
method_call_cb (GDBusConnection *connection,
                const char *sender,
                const char *object_path,
                const char *interface_name,
                const char *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                gpointer user_data)
{
  if (g_strcmp0 (method_name, "HandleArguments") == 0) {
    TerminalOptions *options = NULL;
    GVariant *v_wd, *v_display, *v_sid, *v_envv, *v_argv;
    char *working_directory = NULL, *display_name = NULL, *startup_id = NULL;
    char **envv = NULL, **argv = NULL;
    int argc;
    GError *error = NULL;

    g_variant_get (parameters, "(@ay@ay@ay@ay@ay)",
                   &v_wd, &v_display, &v_sid, &v_envv, &v_argv);

    working_directory = ay_to_string (v_wd, &error);
    if (error)
      goto out;
    display_name = ay_to_string (v_display, &error);
    if (error)
      goto out;
    startup_id = ay_to_string (v_sid, &error);
    if (error)
      goto out;
    envv = ay_to_strv (v_envv, NULL);
    argv = ay_to_strv (v_argv, &argc);

    _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                          "Factory invoked with working-dir='%s' display='%s' startup-id='%s'\n",
                          working_directory ? working_directory : "(null)",
                          display_name ? display_name : "(null)",
                          startup_id ? startup_id : "(null)");

    options = terminal_options_parse (working_directory,
                                      display_name,
                                      startup_id,
                                      envv,
                                      TRUE,
                                      TRUE,
                                      &argc, &argv,
                                      &error,
                                      NULL);

    if (options != NULL) {
      terminal_app_handle_options (terminal_app_get (), options, FALSE /* no resume */, &error);
      terminal_options_free (options);
    }

  out:
    g_variant_unref (v_wd);
    g_free (working_directory);
    g_variant_unref (v_display);
    g_free (display_name);
    g_variant_unref (v_sid);
    g_free (startup_id);
    g_variant_unref (v_envv);
    g_strfreev (envv);
    g_variant_unref (v_argv);
    g_strfreev (argv);

    if (error == NULL) {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
    } else {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
    }
  }
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char *name,
                 gpointer user_data)
{
  static const char dbus_introspection_xml[] =
    "<node name='/org/gnome/Terminal'>"
      "<interface name='org.gnome.Terminal.Factory'>"
        "<method name='HandleArguments'>"
          "<arg type='ay' name='working_directory' direction='in' />"
          "<arg type='ay' name='display_name' direction='in' />"
          "<arg type='ay' name='startup_id' direction='in' />"
          "<arg type='ay' name='environment' direction='in' />"
          "<arg type='ay' name='arguments' direction='in' />"
        "</method>"
      "</interface>"
    "</node>";

  static const GDBusInterfaceVTable interface_vtable = {
    method_call_cb,
    NULL,
    NULL,
  };

  OwnData *data = (OwnData *) user_data;
  GDBusNodeInfo *introspection_data;
  guint registration_id;
  GError *error = NULL;

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Bus %s acquired\n", name);

  introspection_data = g_dbus_node_info_new_for_xml (dbus_introspection_xml, NULL);
  g_assert (introspection_data != NULL);

  registration_id = g_dbus_connection_register_object (connection,
                                                       TERMINAL_FACTORY_SERVICE_PATH,
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL, NULL,
                                                       &error);
  g_dbus_node_info_unref (introspection_data);

  if (registration_id == 0) {
    _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                           "Failed to register object: %s\n", error->message);
    g_error_free (error);
    data->exit_code = EXIT_FAILURE;
    gtk_main_quit ();
  }
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

static char *
get_factory_name_for_display (const char *display_name)
{
#if 0
  GString *name;
  const char *p;

  name = g_string_sized_new (strlen (TERMINAL_FACTORY_SERVICE_NAME_PREFIX) + strlen (display_name) + 1 /* NUL */);
  g_string_append (name, TERMINAL_FACTORY_SERVICE_NAME_PREFIX);

  for (p = display_name; *p; ++p)
    {
      if (g_ascii_isalnum (*p))
        g_string_append_c (name, *p);
      else
        g_string_append_c (name, '_');
    }

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Factory name is \"%s\"\n", name->str);

  return g_string_free (name, FALSE);
#endif
  return g_strdup ("org.gnome.Terminal.Factory0");
}

int
main (int argc, char **argv)
{
  OwnData data;
  guint owner_id;
  const char *home_dir;
  GdkDisplay *display;
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

  /* Unset the these env variables, so they doesn't end up
   * in the factory's env and thus in the terminals' envs.
   */
//   g_unsetenv ("DESKTOP_STARTUP_ID");
//   g_unsetenv ("GIO_LAUNCHED_DESKTOP_FILE_PID");
//   g_unsetenv ("GIO_LAUNCHED_DESKTOP_FILE");

  display = gdk_display_get_default ();
  data.factory_name = get_factory_name_for_display (gdk_display_get_name (display));
  data.exit_code = EXIT_FAILURE;

  owner_id = g_bus_own_name (G_BUS_TYPE_STARTER,
                             data.factory_name,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             bus_acquired_cb,
                             name_acquired_cb,
                             name_lost_cb,
                             &data, NULL);

  gtk_main ();

  g_bus_unown_name (owner_id);

  g_free (data.factory_name);

  terminal_app_shutdown ();

  return data.exit_code;
}
