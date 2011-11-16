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
#include "terminal-factory.h"

#define TERMINAL_UNIQUE_NAME                  "org.gnome.Terminal"

#define TERMINAL_OBJECT_PATH_PREFIX           "/org/gnome/Terminal"
#define TERMINAL_OBJECT_INTERFACE_PREFIX      "org.gnome.Terminal"

#define TERMINAL_FACTORY_OBJECT_PATH          TERMINAL_OBJECT_PATH_PREFIX "/Factory0"
#define TERMINAL_FACTORY_INTERFACE_NAME       TERMINAL_OBJECT_INTERFACE_PREFIX ".Factory0"

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

int
main (int argc, char **argv)
{
  int i;
  char **argv_copy, **envv;
  const char *startup_id, *display_name;
  GdkDisplay *display;
  TerminalOptions *options;
  TerminalFactory *factory;
  GError *error = NULL;
  char *working_directory;
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

  if (options == NULL) {
    g_printerr (_("Failed to parse arguments: %s\n"), error->message);
    g_error_free (error);
    g_free (working_directory);
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

  factory = terminal_factory_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                     TERMINAL_UNIQUE_NAME,
                                                     TERMINAL_FACTORY_OBJECT_PATH,
                                                     NULL /* cancellable */,
                                                     &error);
  if (factory == NULL) {
    g_printerr ("Error constructing proxy for %s:%s: %s\n", 
                TERMINAL_UNIQUE_NAME, TERMINAL_FACTORY_OBJECT_PATH,
                error->message);
    g_error_free (error);
    goto out;
  }

  envv = g_get_environ ();
  if (!terminal_factory_call_handle_arguments_sync (factory,
                                                    working_directory ? working_directory : "",
                                                    display_name ? display_name : "",
                                                    startup_id ? startup_id : "",
                                                    (const char * const *) envv,
                                                    (const char * const *) argv_copy,
                                                    NULL /* cancellable */,
                                                    &error)) {
    g_printerr ("Error opening terminal: %s\n", error->message);
    g_error_free (error);
  } else {
    exit_code = EXIT_SUCCESS;
  }

  g_strfreev (envv);
  g_object_unref (factory);

out:
  terminal_options_free (options);
  g_free (working_directory);
  g_free (argv_copy);

  return exit_code;
}
