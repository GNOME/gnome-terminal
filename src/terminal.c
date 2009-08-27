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

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gdk/gdkx.h>

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#ifdef GDK_WINDOWING_X11
#include "eggdesktopfile.h"
#endif
#endif

#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-options.h"
#include "terminal-util.h"

#define TERMINAL_FACTORY_SERVICE_NAME_PREFIX  "org.gnome.Terminal.Display"
#define TERMINAL_FACTORY_SERVICE_PATH         "/org/gnome/Terminal/Factory"
#define TERMINAL_FACTORY_INTERFACE_NAME       "org.gnome.Terminal.Factory"

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
terminal_factory_handle_arguments (TerminalFactory *factory,
                                   const GArray *working_directory_array,
                                   const GArray *display_name_array,
                                   const GArray *startup_id_array,
                                   const GArray *argv_array,
                                   const GArray *env_array,
                                   GError **error);

#include "terminal-factory-client.h"
#include "terminal-factory-server.h"

static GType terminal_factory_get_type (void);

G_DEFINE_TYPE_WITH_CODE (TerminalFactory, terminal_factory, G_TYPE_OBJECT,
  dbus_g_object_type_install_info (g_define_type_id,
                                   &dbus_glib_terminal_factory_object_info)
);
 
static void
terminal_factory_class_init (TerminalFactoryClass *factory_class)
{
}

static void
terminal_factory_init (TerminalFactory *factory)
{
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
	        const char *uri,
	        gpointer user_data)
{
  GError *error = NULL;

  if (!gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (about)),
                      uri,
                      gtk_get_current_event_time (),
                      &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (about), NULL, error,
                                       "%s", _("Could not open link"));
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

/* Evil hack alert: this is exported from libgconf-2 but not in a public header */
extern gboolean gconf_spawn_daemon(GError** err);

int
main (int argc, char **argv)
{
  int i;
  char **argv_copy;
  int argc_copy;
  const char *startup_id, *display_name;
  GdkDisplay *display;
  TerminalOptions *options;
  DBusGConnection *connection;
  char *factory_name = NULL;
  DBusGProxy *proxy;
  guint32 request_name_ret;
  GError *error = NULL;
  const char *home_dir;
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
      g_clear_error (&error);
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

  factory_name = get_factory_name_for_display (display_name);
  if (!org_freedesktop_DBus_request_name (proxy,
                                          factory_name,
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &request_name_ret,
                                          &error))
    {
      g_printerr ("Failed name request: %s\n", error->message);
      g_clear_error (&error);
      goto factory_disabled;
    }

  /* Forward to the existing factory and exit */
  if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      char **env;
      const char *evalue;
      GPtrArray *env_ptr_array;
      int envc;
      GArray *working_directory_array, *display_name_array, *startup_id_array;
      GArray *env_array, *argv_array;
      gboolean retval;

      _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                             "Forwarding arguments to existing instance\n");

      env = g_listenv ();
      envc = g_strv_length (env);
      env_ptr_array = g_ptr_array_sized_new (envc);
      for (i = 0; i < envc; ++i)
        {
          evalue = g_getenv (env[i]);
          if (evalue)
            g_ptr_array_add (env_ptr_array, g_strdup_printf ("%s=%s", env[i], evalue));
        }
      g_ptr_array_add (env_ptr_array, NULL);

      g_strfreev (env);
      env = (char **) g_ptr_array_free (env_ptr_array, FALSE);

      working_directory = g_get_current_dir ();
      working_directory_array = terminal_util_string_to_array (working_directory);
      display_name_array = terminal_util_string_to_array (options->display_name);
      startup_id_array = terminal_util_string_to_array (options->startup_id);
      env_array = terminal_util_strv_to_array (envc, env);
      argv_array = terminal_util_strv_to_array (argc_copy, argv_copy);

      proxy = dbus_g_proxy_new_for_name (connection,
                                         factory_name,
                                         TERMINAL_FACTORY_SERVICE_PATH,
                                         TERMINAL_FACTORY_INTERFACE_NAME);
      retval = org_gnome_Terminal_Factory_handle_arguments (proxy,
                                                            working_directory_array,
                                                            display_name_array,
                                                            startup_id_array,
                                                            env_array,
                                                            argv_array,
                                                            &error);
      g_free (working_directory);
      g_array_free (working_directory_array, TRUE);
      g_array_free (display_name_array, TRUE);
      g_array_free (startup_id_array, TRUE);
      g_array_free (env_array, TRUE);
      g_array_free (argv_array, TRUE);
      g_strfreev (env);

      if (!retval)
        {
          if (g_error_matches (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD))
            {
              /* Incompatible factory version, fall back, to new instance */
              g_printerr (_("Incompatible factory version; creating a new instance.\n"));
              g_clear_error (&error);

              goto factory_disabled;
            }

          g_printerr (_("Factory error: %s\n"), error->message);
          g_error_free (error);
          ret = EXIT_FAILURE;
        }

      g_free (argv_copy);
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
  g_free (factory_name);

  /* If the gconf daemon isn't available (e.g. because there's no dbus
   * session bus running), we'd crash later on. Tell the user about it
   * now, and exit. See bug #561663.
   * Don't use gconf_ping_daemon() here since the server may just not
   * be running yet, but able to be started. See comments on bug #564649.
   */
  if (!gconf_spawn_daemon (&error))
    {
      g_printerr ("Failed to summon the GConf demon; exiting.  %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);

  gtk_about_dialog_set_url_hook (about_url_hook, NULL, NULL);
  gtk_about_dialog_set_email_hook (about_email_hook, NULL, NULL);

#if defined(WITH_SMCLIENT) && defined(GDK_WINDOWING_X11)
  {
    char *desktop_file;

    desktop_file = g_build_filename (TERM_DATADIR,
                                     "applications",
                                     PACKAGE ".desktop",
                                     NULL);
    egg_set_desktop_file_without_defaults (desktop_file);
    g_free (desktop_file);
  }
#endif

  terminal_app_initialize (options->use_factory);
  g_signal_connect (terminal_app_get (), "quit", G_CALLBACK (gtk_main_quit), NULL);

  terminal_app_handle_options (terminal_app_get (), options, TRUE /* allow resume */, &error);
  terminal_options_free (options);

  if (error)
    {
      g_printerr ("Error handling options: %s\n", error->message);
      g_clear_error (&error);

      ret = EXIT_FAILURE;
      goto shutdown;
    }

  /* Now change directory to $HOME so we don't prevent unmounting, e.g. if the
   * factory is started by nautilus-open-terminal. See bug #565328.
   * On failure back to /.
   */
  home_dir = g_get_home_dir ();
  if (home_dir == NULL || chdir (home_dir) < 0)
    (void) chdir ("/");

  gtk_main ();

shutdown:

  terminal_app_shutdown ();

  if (factory)
    g_object_unref (factory);

  return ret;
}

/* Factory stuff */

static gboolean
terminal_factory_handle_arguments (TerminalFactory *terminal_factory,
                                   const GArray *working_directory_array,
                                   const GArray *display_name_array,
                                   const GArray *startup_id_array,
                                   const GArray *env_array,
                                   const GArray *argv_array,
                                   GError **error)
{
  TerminalOptions *options = NULL;
  char *working_directory = NULL, *display_name = NULL, *startup_id = NULL;
  char **env = NULL, **argv = NULL, **argv_copy = NULL;
  int argc;
  GError *arg_error = NULL;
  gboolean retval;

  working_directory = terminal_util_array_to_string (working_directory_array, &arg_error);
  if (arg_error)
    goto out;
  display_name = terminal_util_array_to_string (display_name_array, &arg_error);
  if (arg_error)
    goto out;
  startup_id = terminal_util_array_to_string (startup_id_array, &arg_error);
  if (arg_error)
    goto out;
  env = terminal_util_array_to_strv (env_array, NULL, &arg_error);
  if (arg_error)
    goto out;
  argv = terminal_util_array_to_strv (argv_array, &argc, &arg_error);
  if (arg_error)
    goto out;

  _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                         "Factory invoked with working-dir='%s' display='%s' startup-id='%s'\n",
                         working_directory ? working_directory : "(null)",
                         display_name ? display_name : "(null)",
                         startup_id ? startup_id : "(null)");

  /* Copy the arguments since terminal_options_parse potentially modifies the array */
  argv_copy = (char **) g_memdup (argv, (argc + 1) * sizeof (char *));

  options = terminal_options_parse (working_directory,
                                    display_name,
                                    startup_id,
                                    env,
                                    TRUE,
                                    TRUE,
                                    &argc, &argv_copy,
                                    error,
                                    NULL);

out:
  g_free (working_directory);
  g_free (display_name);
  g_free (startup_id);
  g_strfreev (env);
  g_strfreev (argv);
  g_free (argv_copy);

  if (arg_error)
    {
      g_propagate_error (error, arg_error);
      return FALSE;
    }

  if (!options)
    return FALSE;

  retval = terminal_app_handle_options (terminal_app_get (), options, FALSE /* no resume */, error);

  terminal_options_free (options);
  return retval;
}
