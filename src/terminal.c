/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include <glib.h>

#include "terminal-intl.h"

#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-options.h"
#include "terminal-util.h"
#include <stdlib.h>
#include <time.h>
#include <gdk/gdkx.h>

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif

#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#define TERMINAL_FACTORY_SERVICE_NAME   "org.gnome.Terminal.Factory"
#define TERMINAL_FACTORY_SERVICE_PATH   "/org/gnome/Terminal/Factory"
#define TERMINAL_FACTORY_INTERFACE_NAME "org.gnome.Terminal.Factory"

#define TERMINAL_TYPE_FACTORY             (terminal_factory_get_type ())
#define TERMINAL_FACTORY(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_FACTORY, TerminalFactory))
#define TERMINAL_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_FACTORY, TerminalFactoryClass))
#define TERMINAL_IS_FACTORY(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_FACTORY))
#define TERMINAL_IS_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_FACTORY))
#define TERMINAL_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_FACTORY, TerminalFactoryClass))

typedef struct _TerminalFactory        TerminalFactory;
typedef struct _TerminalFactoryClass   TerminalFactoryClass;
typedef struct _TerminalFactoryPrivate TerminalFactoryPrivate;

struct _TerminalFactory
{
  GObject parent_instance;
};

struct _TerminalFactoryClass
{
  GObjectClass parent_class;
};

static gboolean
terminal_factory_new_terminal (TerminalFactory *factory,
                               const char *working_directory,
                               const char *display_name,
                               const char *startup_id,
                               const char **argv,
                               const char **env,
                               GError **error);

#include "terminal-factory-client.h"
#include "terminal-factory-server.h"

static void
terminal_factory_class_init (TerminalFactoryClass *factory_class)
{
}

static void
terminal_factory_init (TerminalFactory *factory)
{
}

static GType terminal_factory_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TerminalFactory, terminal_factory, G_TYPE_OBJECT,
  dbus_g_object_type_install_info (g_define_type_id,
                                   &dbus_glib_terminal_factory_object_info)
);
 
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

static TerminalFactory *factory = NULL;

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

static void
about_url_hook (GtkAboutDialog *about,
	        const char *link,
	        gpointer user_data)
{
  GError *error = NULL;

  if (!gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (about)),
                      link,
                      gtk_get_current_event_time (),
                      &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (about), NULL,
                                       _("Could not open link: %s"),
                                       error->message);
      g_error_free (error);
    }
}

static void
about_email_hook (GtkAboutDialog *about,
		  const char *email_address,
		  gpointer user_data)
{
  char *escaped, *uri;

  escaped = g_uri_escape_string (email_address, NULL, FALSE);
  uri = g_strdup_printf ("mailto:%s", escaped);
  g_free (escaped);

  about_url_hook (about, uri, user_data);
  g_free (uri);
}

int
main (int argc, char **argv)
{
  int i;
  char **argv_copy;
  const char *startup_id;
  const char *display_name;
  GdkDisplay *display;
  TerminalOptions *options;
  DBusGConnection *connection;
  DBusGProxy *proxy;
  guint32 request_name_ret;
  GError *error = NULL;

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Make a NULL-terminated copy since we may need it later */
  argv_copy = g_new (char *, argc + 1);
  for (i = 0; i < argc; ++i)
    argv_copy [i] = argv [i];
  argv_copy [i] = NULL;

  startup_id = g_getenv ("DESKTOP_STARTUP_ID");

  options = terminal_options_parse (NULL,
                                    NULL,
                                    startup_id,
                                    NULL,
                                    FALSE,
                                    &argc, &argv,
                                    &error,
                                    gtk_get_option_group (TRUE),
#ifdef WITH_SMCLIENT
                                    egg_sm_client_get_option_group (),
#endif
                                    NULL);
  if (!options)
    {
      g_printerr (_("Failed to parse arguments: %s\n"), error->message);
      g_error_free (error);
      exit (1);
    }

  g_set_application_name (_("Terminal"));
  
  /* Unset the startup ID, so it doesn't end up in the factory's env
   * and thus in the terminals' envs.
   */
  if (startup_id)
    g_unsetenv ("DESKTOP_STARTUP_ID");

  gtk_window_set_auto_startup_notification (FALSE); /* we'll do it ourselves due
                                                     * to complicated factory setup
                                                     */

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
  
  if (!options->use_factory)
    goto factory_disabled;

  /* Now try to acquire register us as the terminal factory */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (!connection)
    {
      g_printerr ("Failed to get the session bus: %s\nFalling back to non-factory mode.\n",
                  error->message);
      g_error_free (error);
      goto factory_disabled;
    }

  proxy = dbus_g_proxy_new_for_name (connection,
                                     DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_DBUS);
#if 0
  dbus_g_proxy_add_signal (proxy, "NameOwnerChanged",
                           G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (proxy, "NameOwnerChanged",
                               G_CALLBACK (name_owner_changed), factory, NULL);
#endif

  if (!org_freedesktop_DBus_request_name (proxy,
                                          TERMINAL_FACTORY_SERVICE_NAME,
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &request_name_ret,
                                          &error))
    {
      g_printerr ("Failed name request: %s\n", error->message);
      g_error_free (error);
      goto factory_disabled;
    }

  /* Forward to the existing factory and exit */
  if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      char **env;
      const char *evalue;
      guint i, n;
      GPtrArray *env_array;
      int ret = EXIT_SUCCESS;

      env = g_listenv ();
      n = g_strv_length (env);
      env_array = g_ptr_array_sized_new (n);
      for (i = 0; i < n; ++i)
        {
          evalue = g_getenv (env[i]);
          if (evalue)
            g_ptr_array_add (env_array, g_strdup_printf ("%s=%s", env[i], evalue));
        }
      g_ptr_array_add (env_array, NULL);

      g_strfreev (env);
      env = (char **) g_ptr_array_free (env_array, FALSE);

      proxy = dbus_g_proxy_new_for_name (connection,
                                         TERMINAL_FACTORY_SERVICE_NAME,
                                         TERMINAL_FACTORY_SERVICE_PATH,
                                         TERMINAL_FACTORY_INTERFACE_NAME);
      if (!org_gnome_Terminal_Factory_new_terminal (proxy,
                                                    g_get_current_dir (),
                                                    options->display_name,
                                                    options->startup_id,
                                                    (const char **) env,
                                                    (const char **) argv_copy,
                                                    &error))
        {
          if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD))
            {
              /* Incompatible factory version, fall back, to new instance */
              g_printerr (_("Incompatible factory version; creating a new instance.\n"));
              g_strfreev (env);
              g_error_free (error);

              goto factory_disabled;
            }

          g_printerr (_("Factory error: %s\n"), error->message);
          g_error_free (error);
          ret = EXIT_FAILURE;
        }

      g_free (argv_copy);
      g_strfreev (env);
      terminal_options_free (options);

      exit (ret);
    }

  factory = g_object_new (TERMINAL_TYPE_FACTORY, NULL);
  dbus_g_connection_register_g_object (connection,
                                       TERMINAL_FACTORY_SERVICE_PATH,
                                       G_OBJECT (factory));

  /* Now we're registered as the factory. Proceed to open the terminal(s). */

factory_disabled:
  g_free (argv_copy);

  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);

  gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);
  gtk_about_dialog_set_email_hook (about_email_hook, NULL, NULL);

  terminal_app_initialize (options->use_factory);
  g_signal_connect (terminal_app_get (), "quit", G_CALLBACK (gtk_main_quit), NULL);

  terminal_app_handle_options (terminal_app_get (), options);
  terminal_options_free (options);

  gtk_main ();

  terminal_app_shutdown ();

  if (factory)
    g_object_unref (factory);

  return 0;
}

/* Factory stuff */

static gboolean
handle_new_terminal_event (TerminalOptions *options)
{
  terminal_app_handle_options (terminal_app_get (), options);

  return FALSE;
}

static gboolean
terminal_factory_new_terminal (TerminalFactory *factory,
                               const char *working_directory,
                               const char *display_name,
                               const char *startup_id,
                               const char **env,
                               const char **arguments,
                               GError **error)
{
  TerminalOptions *options;
  char **argv;
  int argc;

  /* Copy the arguments since terminal_options_parse potentially modifies the array */
  argc = g_strv_length ((char **) arguments);
  argv = (char **) g_memdup (arguments, (argc + 1) * sizeof (char *));

  options = terminal_options_parse (working_directory,
                                    display_name,
                                    startup_id,
                                    env,
                                    TRUE,
                                    &argc, &argv,
                                    error,
                                    NULL);
  g_free (argv);

  if (!options)
    return FALSE;

  g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                   (GSourceFunc) handle_new_terminal_event,
                   options,
                   (GDestroyNotify) terminal_options_free);

  return TRUE;
}
