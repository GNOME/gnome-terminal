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
#include "terminal-gdbus-generated.h"
#include "terminal-defines.h"

/**
 * handle_options:
 * @app:
 * @options: a #TerminalOptions
 * @allow_resume: whether to merge the terminal configuration from the
 *   saved session on resume
 * @error: a #GError to fill in
 *
 * Processes @options. It loads or saves the terminal configuration, or
 * opens the specified windows and tabs.
 *
 * Returns: %TRUE if @options could be successfully handled, or %FALSE on
 *   error
 */
static gboolean
handle_options (TerminalFactory *factory,
                TerminalOptions *options,
                char **envv,
                gboolean allow_resume,
                GError **error)
{
#if 0
  GList *lw;
  GdkScreen *gdk_screen;

  gdk_screen = terminal_app_get_screen_by_display_name (options->display_name,
                                                        options->screen_number);

  if (options->save_config)
    {
#if 0
      if (options->remote_arguments)
        return terminal_app_save_config_file (app, options->config_file, error);
      
      g_set_error_literal (error, TERMINAL_OPTION_ERROR, TERMINAL_OPTION_ERROR_NOT_IN_FACTORY,
                            "Cannot use \"--save-config\" when starting the factory process");
      return FALSE;
#endif
      g_set_error (error, TERMINAL_OPTION_ERROR, TERMINAL_OPTION_ERROR_NOT_SUPPORTED,
                   "Not supported anymore");
      return FALSE;
    }

  if (options->load_config)
    {
      GKeyFile *key_file;
      gboolean result;

      key_file = g_key_file_new ();
      result = g_key_file_load_from_file (key_file, options->config_file, 0, error) &&
               terminal_options_merge_config (options, key_file, SOURCE_DEFAULT, error);
      g_key_file_free (key_file);

      if (!result)
        return FALSE;

      /* fall-through on success */
    }

#ifdef WITH_SMCLIENT
{
  EggSMClient *sm_client;

  sm_client = egg_sm_client_get ();

  if (allow_resume && egg_sm_client_is_resumed (sm_client))
    {
      GKeyFile *key_file;

      key_file = egg_sm_client_get_state_file (sm_client);
      if (key_file != NULL &&
          !terminal_options_merge_config (options, key_file, SOURCE_SESSION, error))
        return FALSE;
    }
}
#endif

  /* Make sure we open at least one window */
  terminal_options_ensure_window (options);

  if (options->startup_id != NULL)
    _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                           "Startup ID is '%s'\n",
                           options->startup_id);

  for (lw = options->initial_windows;  lw != NULL; lw = lw->next)
    {
      InitialWindow *iw = lw->data;
      GList *lt;
#if 0
      TerminalWindow *window;

      g_assert (iw->tabs);

      /* Create & setup new window */
      window = terminal_app_new_window (app, gdk_screen);

      /* Restored windows shouldn't demand attention; see bug #586308. */
      if (iw->source_tag == SOURCE_SESSION)
        terminal_window_set_is_restored (window);

      if (options->startup_id != NULL)
        gtk_window_set_startup_id (GTK_WINDOW (window), options->startup_id);

      /* Overwrite the default, unique window role set in terminal_window_init */
      if (iw->role)
        gtk_window_set_role (GTK_WINDOW (window), iw->role);

      if (iw->force_menubar_state)
        terminal_window_set_menubar_visible (window, iw->menubar_state);

      if (iw->start_fullscreen)
        gtk_window_fullscreen (GTK_WINDOW (window));
      if (iw->start_maximized)
        gtk_window_maximize (GTK_WINDOW (window));
#endif

      /* Now add the tabs */
      for (lt = iw->tabs; lt != NULL; lt = lt->next)
        {
          InitialTab *it = lt->data;
#if 0
          GSettings *profile = NULL;
          TerminalScreen *screen;

          if (it->profile)
            {
              if (it->profile_is_id)
                profile = terminal_app_get_profile_by_name (app, it->profile);
              else
                profile = terminal_app_get_profile_by_visible_name (app, it->profile);

              if (profile == NULL)
                g_printerr (_("No such profile \"%s\", using default profile\n"), it->profile);
            }
          if (profile == NULL)
            profile = g_object_ref (g_hash_table_lookup (app->profiles, "profile0"));
          g_assert (profile);

          screen = terminal_app_new_terminal (app, window, profile,
                                              it->exec_argv ? it->exec_argv : options->exec_argv,
                                              it->title ? it->title : options->default_title,
                                              it->working_dir ? it->working_dir : options->default_working_dir,
                                              options->env,
                                              it->zoom_set ? it->zoom : options->zoom);
          g_object_unref (profile);

          if (it->active)
            terminal_window_switch_screen (window, screen);
#endif
        }

#if 0
      if (iw->geometry)
        {
          _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                                "[window %p] applying geometry %s\n",
                                window, iw->geometry);

          if (!terminal_window_parse_geometry (window, iw->geometry))
            g_printerr (_("Invalid geometry string \"%s\"\n"), iw->geometry);
        }

      gtk_window_present (GTK_WINDOW (window));
#endif
    }

  return TRUE;
#endif
  return FALSE;
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
#if 0
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
#endif
  if (!handle_options (factory, options, envv, TRUE /* allow resume */, &error)) {
    g_printerr ("Failed to handle arguments: %s\n", error->message);
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
