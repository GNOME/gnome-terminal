/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
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

#include <gdk/gdkx.h>

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-options.h"
#include "terminal-util.h"

#define TERMINAL_FACTORY_SERVICE_NAME_PREFIX  "org.gnome.Terminal.Display"
#define TERMINAL_FACTORY_SERVICE_PATH         "/org/gnome/Terminal/Factory"

#define GAPPLICATION_INTERFACE_NAME           "org.gtk.Application"

/* The returned string is owned by @variant */
static const char *
ay_to_string (GVariant *variant,
              GError **error)
{
  const char *string;
  gsize len;

  len = g_variant_get_size (variant);
  if (len == 0) {
    g_set_error_literal (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                         "NUL strings not allowed");
    return NULL;
  }

  string = g_variant_get_bytestring (variant);
  if (strlen (string) != (len - 1)) {
    g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                         "String is shorter than claimed (claimed %" G_GSIZE_FORMAT " actual %" G_GSIZE_FORMAT ")",
                         len - 1, strlen (string));
    return NULL;
  }

  return string;
}

/* The returned strings are owned by @variant. Free the array itself with g_free(). */
static char **
aay_to_strv (GVariant *variant,
             int *argc,
             GError **error)
{
  GVariant *item;
  char **argv;
  gsize i, n;

  n = g_variant_n_children (variant);
  if (argc)
    *argc = n;
  if (n == 0)
    return NULL;

  argv = g_new (char *, n + 1);

  for (i = 0; i < n; ++i) {
    item = g_variant_get_child_value (variant, i);
    argv[i] = (char *) ay_to_string (item, error);
    g_variant_unref (item);
    if (*error != NULL)
      goto err;
  }

  argv[n] = NULL;
  return argv;

err:
  g_free (argv);
  return NULL;
}

typedef struct {
  char *factory_name;
  TerminalOptions *options;
  int exit_code;
  char **argv;
  int argc;
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
  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Remote method call sender '%s' object-path '%s' interface-name '%s' method-name '%s' parameters-type '%s'\n",
                         sender, object_path, interface_name, method_name,
                         g_variant_get_type_string (parameters));

  if (g_strcmp0 (object_path, TERMINAL_FACTORY_SERVICE_PATH) != 0 ||
      g_strcmp0 (interface_name, GAPPLICATION_INTERFACE_NAME) != 0)
    return;

  if (g_strcmp0 (method_name, "Activate") == 0) {
    TerminalOptions *options = NULL;
    GVariantIter *data_iter;
    const char *key;
    GVariant *data, *v_argv;
    const char *working_directory = NULL, *display_name = NULL, *startup_id = NULL;
    char **envv = NULL, **argv = NULL;
    int argc;
    GError *error = NULL;

    g_variant_get (parameters, "(@aaya{sv})", &v_argv, &data_iter);

    while (g_variant_iter_next (data_iter, "{&sv}", &key, &data)) {
      if (strcmp (key, "startup-notication-id") == 0) {
        working_directory = ay_to_string (data, &error);
      } else if (strcmp (key, "display-name") == 0) {
        display_name = ay_to_string (data, &error);
      } else if (strcmp (key, "cwd") == 0) {
        working_directory = ay_to_string (data, &error);
      } else if (strcmp (key, "environment") == 0) {
        envv = aay_to_strv (data, NULL, &error);
      }

      g_variant_unref (data);
      if (error)
        break;
    }
    g_variant_iter_free (data_iter);
    if (error)
      goto out;

    argv = aay_to_strv (v_argv, &argc, &error);
    if (error)
      goto out;

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
    g_free (envv);
    g_free (argv);
    g_variant_unref (v_argv);

    if (error == NULL) {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
    } else {
      g_dbus_method_invocation_return_gerror (invocation, error);
      g_error_free (error);
    }
  } else if (g_strcmp0 (method_name, "ListActions") == 0) {
    g_dbus_method_invocation_return_value (invocation,
                                            g_variant_new ("(a{s(sb)})", NULL));
    return;
  } else if (g_strcmp0 (method_name, "InvokeAction") == 0) {
    g_dbus_method_invocation_return_error (invocation,
                                            G_DBUS_ERROR,
                                            G_DBUS_ERROR_NOT_SUPPORTED,
                                            "Not supported");
    return;
  } else if (g_strcmp0 (method_name, "Quit") == 0) {
    /* Very funny. If you want to quit, use the session manager! */
    g_dbus_method_invocation_return_error (invocation,
                                            G_DBUS_ERROR,
                                            G_DBUS_ERROR_NOT_SUPPORTED,
                                            "Not supported");
    return;
  }
}

static void
bus_acquired_cb (GDBusConnection *connection,
                 const char *name,
                 gpointer user_data)
{
  static const char dbus_introspection_xml[] =
    "<node name='/org/gnome/Terminal'>"
      "<interface name='org.gtk.Application'>"
        "<method name='Quit'>"
          "<arg type='u' name='timestamp' direction='in'/>"
        "</method>"
        "<method name='ListActions'>"
          "<arg type='a{s(sb)}' name='actions' direction='out'/>"
        "</method>"
        "<method name='InvokeAction'>"
          "<arg type='s' name='action' direction='in'/>"
          "<arg type='u' name='timestamp' direction='in'/>"
        "</method>"
        "<method name='Activate'>"
          "<arg type='aay' name='arguments' direction='in'/>"
          "<arg type='a{sv}' name='data' direction='in'/>"
        "</method>"
        "<signal name='ActionsChanged'/>"
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
    g_printerr ("Failed to register object: %s\n", error->message);
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
  OwnData *data = (OwnData *) user_data;
  GError *error = NULL;

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Acquired the name %s on the session bus\n", name);

  if (data->options == NULL) {
    /* Name re-acquired!? */
    g_assert_not_reached ();
  }


  if (!terminal_app_handle_options (terminal_app_get (), data->options, FALSE /* no resume */, &error)) {
    g_printerr ("Failed to handle options: %s\n", error->message);
    g_error_free (error);
    data->exit_code = EXIT_FAILURE;
    gtk_main_quit ();
  }

  terminal_options_free (data->options);
  data->options = NULL;
}

static void
name_lost_cb (GDBusConnection *connection,
              const char *name,
              gpointer user_data)
{
  OwnData *data = (OwnData *) user_data;
  GError *error = NULL;
  char **envv;
  int envc, i;
  GVariantBuilder builder;
  GVariant *value;

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Lost the name %s on the session bus\n", name);

  if (data->options == NULL) {
    /* Already handled */
    data->exit_code = EXIT_SUCCESS;
    gtk_main_quit ();
    return;
  }

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                          "Forwarding arguments to existing instance\n");

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(aaya{sv})"));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("aay"));
  for (i = 0; i < data->argc; ++i)
    g_variant_builder_add (&builder, "^ay", data->argv[i]);
  g_variant_builder_close (&builder); /* aay */

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}",
                         "startup-notication-id",
                         g_variant_new_bytestring (data->options->startup_id ? data->options->startup_id : ""));
  g_variant_builder_add (&builder, "{sv}",
                         "display-name",
                         g_variant_new_bytestring (data->options->display_name ? data->options->display_name : ""));
  g_variant_builder_add (&builder, "{sv}",
                         "cwd",
                         g_variant_new_bytestring (data->options->default_working_dir ? data->options->default_working_dir : ""));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (&builder, "s", "environment");
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("aay"));
  envv = g_listenv ();
  envc = g_strv_length (envv);
  for (i = 0; i < envc; ++i)
    {
      const char *value;
      char *str;

      value = g_getenv (envv[i]);
      if (value == NULL)
        continue;

      str = g_strdup_printf ("%s=%s", envv[i], value);
      g_variant_builder_add (&builder, "^ay", str);
      g_free (str);
    }
  g_variant_builder_close (&builder); /* aay */
  g_variant_builder_close (&builder); /* v */
  g_variant_builder_close (&builder); /* {sv} */

  g_variant_builder_close (&builder); /* a{sv} */

  value = g_dbus_connection_call_sync (connection,
                                       data->factory_name,
                                       TERMINAL_FACTORY_SERVICE_PATH,
                                       GAPPLICATION_INTERFACE_NAME,
                                       "Activate",
                                       g_variant_builder_end (&builder),
                                       G_VARIANT_TYPE ("()"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);
  if (value == NULL) {
    g_printerr ("Failed to forward arguments: %s\n", error->message);
    g_error_free (error);
    data->exit_code = EXIT_FAILURE;
    gtk_main_quit ();
  } else {
    g_variant_unref (value);
    data->exit_code = EXIT_SUCCESS;
  }

  terminal_options_free (data->options);
  data->options = NULL;

  gtk_main_quit ();
}

/* Settings storage works as follows:
 *   /apps/gnome-terminal/global/
 *   /apps/gnome-terminal/profiles/Foo/
 *
 * It's somewhat tricky to manage the profiles/ dir since we need to track
 * the list of profiles, but gconf doesn't have a concept of notifying that
 * a directory has appeared or disappeared.
 *
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See gnome-terminal 1.x for why.
 *
 * Don't use this code as an example of how to use GConf - it's hugely
 * overcomplicated due to the profiles stuff. Most apps should not
 * have to do scary things of this nature, and should not have
 * a profiles feature.
 *
 */

/* Copied from libnautilus/nautilus-program-choosing.c; Needed in case
 * we have no DESKTOP_STARTUP_ID (with its accompanying timestamp).
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
  Window xwindow;
  XEvent event;

  {
    XSetWindowAttributes attrs;
    Atom atom_name;
    Atom atom_type;
    const char *name;

    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask | StructureNotifyMask;

    xwindow =
      XCreateWindow (xdisplay,
                     RootWindow (xdisplay, 0),
                     -100, -100, 1, 1,
                     0,
                     CopyFromParent,
                     CopyFromParent,
                     (Visual *)CopyFromParent,
                     CWOverrideRedirect | CWEventMask,
                     &attrs);

    atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
    g_assert (atom_name != None);
    atom_type = XInternAtom (xdisplay, "STRING", TRUE);
    g_assert (atom_type != None);

    name = "Fake Window";
    XChangeProperty (xdisplay,
                     xwindow, atom_name,
                     atom_type,
                     8, PropModeReplace, (unsigned char *)name, strlen (name));
  }

  XWindowEvent (xdisplay,
                xwindow,
                PropertyChangeMask,
                &event);

  XDestroyWindow(xdisplay, xwindow);

  return event.xproperty.time;
}

static char *
get_factory_name_for_display (const char *display_name)
{
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
}

int
main (int argc, char **argv)
{
  int i;
  char **argv_copy;
  int argc_copy;
  const char *startup_id, *display_name, *home_dir;
  GdkDisplay *display;
  TerminalOptions *options;
  GError *error = NULL;
  char *working_directory;
  int ret = EXIT_SUCCESS;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* GConf uses ORBit2 which need GThread. See bug #565516 */
  g_thread_init (NULL);

  _terminal_debug_init ();

  /* Make a NULL-terminated copy since we may need it later */
  argv_copy = g_new (char *, argc + 1);
  for (i = 0; i < argc; ++i)
    argv_copy [i] = argv [i];
  argv_copy [i] = NULL;
  argc_copy = argc;

  startup_id = g_getenv ("DESKTOP_STARTUP_ID");

  working_directory = g_get_current_dir ();

  /* Now change directory to $HOME so we don't prevent unmounting, e.g. if the
   * factory is started by nautilus-open-terminal. See bug #565328.
   * On failure back to /.
   */
  home_dir = g_get_home_dir ();
  if (home_dir == NULL || chdir (home_dir) < 0)
    (void) chdir ("/");

  options = terminal_options_parse (working_directory,
                                    NULL,
                                    startup_id,
                                    NULL,
                                    FALSE,
                                    FALSE,
                                    &argc, &argv,
                                    &error,
                                    gtk_get_option_group (TRUE),
#ifdef WITH_SMCLIENT
                                    egg_sm_client_get_option_group (),
#endif
                                    NULL);

  g_free (working_directory);

  if (options == NULL) {
    g_printerr (_("Failed to parse arguments: %s\n"), error->message);
    g_error_free (error);
    exit (EXIT_FAILURE);
  }

  g_set_application_name (_("Terminal"));
  
  /* Unset the these env variables, so they doesn't end up
   * in the factory's env and thus in the terminals' envs.
   */
  g_unsetenv ("DESKTOP_STARTUP_ID");
  g_unsetenv ("GIO_LAUNCHED_DESKTOP_FILE_PID");
  g_unsetenv ("GIO_LAUNCHED_DESKTOP_FILE");

 /* Do this here so that gdk_display is initialized */
  if (options->startup_id == NULL)
    {
      /* Create a fake one containing a timestamp that we can use */
      Time timestamp;

      timestamp = slowly_and_stupidly_obtain_timestamp (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

      options->startup_id = g_strdup_printf ("_TIME%lu", timestamp);
    }

  display = gdk_display_get_default ();
  display_name = gdk_display_get_name (display);
  options->display_name = g_strdup (display_name);
  
  if (options->use_factory) {
    OwnData *data;
    guint owner_id;

    data = g_new (OwnData, 1);
    data->factory_name = get_factory_name_for_display (display_name);
    data->options = options;
    data->exit_code = -1;
    data->argv = argv_copy;
    data->argc = argc_copy;

    owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                               data->factory_name,
                               G_BUS_NAME_OWNER_FLAGS_NONE,
                               bus_acquired_cb,
                               name_acquired_cb,
                               name_lost_cb,
                               data, NULL);

    gtk_main ();

    ret = data->exit_code;
    g_bus_unown_name (owner_id);

    g_free (data->factory_name);
    g_free (data);

  } else {

    terminal_app_handle_options (terminal_app_get (), options, TRUE /* allow resume */, &error);
    terminal_options_free (options);

    if (error == NULL) {
      gtk_main ();
    } else {
      g_printerr ("Error handling options: %s\n", error->message);
      g_error_free (error);
      ret = EXIT_FAILURE;
    }
  }

  terminal_app_shutdown ();

  g_free (argv_copy);

  return ret;
}
