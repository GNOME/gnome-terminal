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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif

#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-options.h"

#define TERMINAL_FACTORY_SERVICE_NAME_PREFIX  "org.gnome.Terminal.Factory0.Display"
#define TERMINAL_FACTORY_SERVICE_PATH         "/org/gnome/Terminal/Factory"
#define TERMINAL_FACTORY_INTERFACE_NAME       "org.gnome.Terminal.Factory"

static GVariant *
string_to_ay (const char *string)
{
  gsize len;
  char *data;

  len = strlen (string);
  data = g_strndup (string, len);

  return g_variant_new_from_data (G_VARIANT_TYPE ("ay"), data, len, TRUE, g_free, data);
}

/**
 * options_to_variant:
 * 
 * Returns: a new floating #GVariant
 */
static GVariant *
options_to_variant (TerminalOptions *options,
                    char **argv,
                    int argc)
{
  char **envv;
  int i;
  GVariantBuilder builder;
  GString *string;
  char *s;
  gsize len;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ayayayayay)"));

  g_variant_builder_add (&builder, "@ay", string_to_ay (options->default_working_dir));
  g_variant_builder_add (&builder, "@ay", string_to_ay (options->display_name));
  g_variant_builder_add (&builder, "@ay", string_to_ay (options->startup_id));

  string = g_string_new (NULL);
  envv = g_get_environ ();
  for (i = 0; envv[i]; ++i)
    {
      if (i > 0)
        g_string_append_c (string, '\0');

      g_string_append (string, envv[i]);
    }
  g_strfreev (envv);

  len = string->len;
  s = g_string_free (string, FALSE);
  g_variant_builder_add (&builder, "@ay",
                         g_variant_new_from_data (G_VARIANT_TYPE ("ay"), s, len, TRUE, g_free, s));

  string = g_string_new (NULL);

  for (i = 0; i < argc; ++i)
    {
      if (i > 0)
        g_string_append_c (string, '\0');
      g_string_append (string, argv[i]);
    }

  len = string->len;
  s = g_string_free (string, FALSE);
  g_variant_builder_add (&builder, "@ay",
                         g_variant_new_from_data (G_VARIANT_TYPE ("ay"), s, len, TRUE, g_free, s));

  return g_variant_builder_end (&builder);
}

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
  int i;
  char **argv_copy;
  int argc_copy;
  const char *startup_id, *display_name;
  char *factory_name = NULL;
  GdkDisplay *display;
  TerminalOptions *options;
  GDBusConnection *connection;
  GError *error = NULL;
  char *working_directory;
  GVariant *server_retval;
  int exit_code = EXIT_FAILURE;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_type_init ();

  _terminal_debug_init ();

  /* Make a NULL-terminated copy since we may need it later */
  argv_copy = g_new (char *, argc + 1);
  for (i = 0; i < argc; ++i)
    argv_copy [i] = argv [i];
  argv_copy [i] = NULL;
  argc_copy = argc;

  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  working_directory = g_get_current_dir ();

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
    g_free (argv_copy);
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

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (connection == NULL) {
    g_printerr ("Error connecting to bus: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  factory_name = get_factory_name_for_display (options->display_name);
  server_retval = g_dbus_connection_call_sync (connection,
                                               factory_name,
                                               TERMINAL_FACTORY_SERVICE_PATH,
                                               TERMINAL_FACTORY_INTERFACE_NAME,
                                               "HandleArguments",
                                               options_to_variant (options, argv_copy, argc_copy),
                                               G_VARIANT_TYPE ("()"),
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);
  if (server_retval == NULL) {
    g_printerr ("Error opening terminal: %s\n", error->message);
    g_error_free (error);
  } else {
    g_variant_unref (server_retval);
    exit_code = EXIT_SUCCESS;
  }

  g_free (factory_name);
  g_object_unref (connection);

out:
  terminal_options_free (options);

  g_free (argv_copy);

  return exit_code;
}
