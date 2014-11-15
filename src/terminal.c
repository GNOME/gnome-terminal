/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#include "config.h"
#define _GNU_SOURCE

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "terminal-debug.h"
#include "terminal-i18n.h"
#include "terminal-options.h"
#include "terminal-gdbus-generated.h"
#include "terminal-defines.h"
#include "terminal-client-utils.h"

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
                GError **error)
{
  GList *lw;
  GError *err;

#if 0
  gdk_screen = terminal_app_get_screen_by_display_name (options->display_name,
                                                        options->screen_number);
#endif

  /* Make sure we open at least one window */
  terminal_options_ensure_window (options);

  for (lw = options->initial_windows;  lw != NULL; lw = lw->next)
    {
      InitialWindow *iw = lw->data;
      GList *lt;
      guint window_id;

      g_assert (iw->tabs);

      window_id = 0;

      /* Now add the tabs */
      for (lt = iw->tabs; lt != NULL; lt = lt->next)
        {
          InitialTab *it = lt->data;
          GVariantBuilder builder;
          char *object_path, *p;
          TerminalReceiver *receiver;
          char **argv;
          int argc;

          err = NULL;

          g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

          terminal_client_append_create_instance_options (&builder,
                                                          options->display_name,
                                                          options->startup_id,
                                                          iw->geometry,
                                                          iw->role,
                                                          it->profile ? it->profile : options->default_profile,
                                                          NULL /* title */,
                                                          iw->start_maximized,
                                                          iw->start_fullscreen);

          if (window_id)
            g_variant_builder_add (&builder, "{sv}",
                                   "window-id", g_variant_new_uint32 (window_id));

          /* Restored windows shouldn't demand attention; see bug #586308. */
          if (iw->source_tag == SOURCE_SESSION)
            g_variant_builder_add (&builder, "{sv}",
                                   "present-window", g_variant_new_boolean (FALSE));
          if (options->zoom_set || it->zoom_set)
            g_variant_builder_add (&builder, "{sv}",
                                   "zoom", g_variant_new_double (it->zoom_set ? it->zoom : options->zoom));
          if (iw->force_menubar_state)
            g_variant_builder_add (&builder, "{sv}",
                                   "show-menubar", g_variant_new_boolean (iw->menubar_state));
#if 0
          if (it->active)
            terminal_window_switch_screen (window, screen);
#endif

          if (!terminal_factory_call_create_instance_sync 
                 (factory,
                  g_variant_builder_end (&builder),
                  &object_path,
                  NULL /* cancellable */,
                  &err)) {
            g_dbus_error_strip_remote_error (err);
            g_printerr ("Error creating terminal: %s\n", err->message);
            g_error_free (err);

            /* Continue processing the remaining options! */
            continue;
          }

          p = strstr (object_path, "/window/");
          if (p) {
            char *end = NULL;
            guint64 value;

            errno = 0;
            p += strlen ("/window/");
            value = g_ascii_strtoull (p, &end, 10);
            if (errno == 0 && end != p && *end == '/')
              window_id = (guint) value;
          }

          receiver = terminal_receiver_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                               options->server_app_id ? options->server_app_id
                                                                                      : TERMINAL_APPLICATION_ID,
                                                               object_path,
                                                               NULL /* cancellable */,
                                                               &err);
          if (receiver == NULL) {
            g_dbus_error_strip_remote_error (err);
            g_printerr ("Failed to create proxy for terminal: %s\n", err->message);
            g_error_free (err);
            g_free (object_path);

            /* Continue processing the remaining options! */
            continue;
          }
          g_free (object_path);

          g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

          argv = it->exec_argv ? it->exec_argv : options->exec_argv,
          argc = argv ? g_strv_length (argv) : 0;

          terminal_client_append_exec_options (&builder,
                                               it->working_dir ? it->working_dir 
                                                               : options->default_working_dir,
                                               NULL, 0, /* FD array */
                                               argc == 0);

          if (!terminal_receiver_call_exec_sync (receiver,
                                                 g_variant_builder_end (&builder),
                                                 g_variant_new_bytestring_array ((const char * const *) argv, argc),
                                                 NULL /* infdlist */, NULL /* outfdlist */,
                                                NULL /* cancellable */,
                                                &err)) {
            g_dbus_error_strip_remote_error (err);
            g_printerr ("Error: %s\n", err->message);
            g_error_free (err);
          }

          g_object_unref (receiver);
        }
    }

  return TRUE;
}

int
main (int argc, char **argv)
{
  int i;
  char **argv_copy;
  const char *startup_id, *display_name;
  GdkDisplay *display;
  TerminalOptions *options;
  TerminalFactory *factory;
  GError *error = NULL;
  char *working_directory;
  int exit_code = EXIT_FAILURE;

  setlocale (LC_ALL, "");

  terminal_i18n_init (TRUE);

  _terminal_debug_init ();

  /* Make a NULL-terminated copy since we may need it later */
  argv_copy = g_new (char *, argc + 1);
  for (i = 0; i < argc; ++i)
    argv_copy [i] = argv [i];
  argv_copy [i] = NULL;

  startup_id = g_getenv ("DESKTOP_STARTUP_ID");

  working_directory = g_get_current_dir ();

  options = terminal_options_parse (working_directory,
                                    startup_id,
                                    &argc, &argv,
                                    &error);
  if (options == NULL) {
    g_printerr (_("Failed to parse arguments: %s\n"), error->message);
    g_error_free (error);
    g_free (working_directory);
    g_free (argv_copy);
    exit (EXIT_FAILURE);
  }

  g_set_application_name (_("Terminal"));

  /* Do this here so that gdk_display is initialized */
  if (options->startup_id == NULL)
    options->startup_id = terminal_client_get_fallback_startup_id ();

  display = gdk_display_get_default ();
  display_name = gdk_display_get_name (display);
  options->display_name = g_strdup (display_name);

  factory = terminal_factory_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                     options->server_app_id ? options->server_app_id 
                                                                            : TERMINAL_APPLICATION_ID,
                                                     TERMINAL_FACTORY_OBJECT_PATH,
                                                     NULL /* cancellable */,
                                                     &error);
  if (factory == NULL) {
    g_dbus_error_strip_remote_error (error);
    g_printerr ("Error constructing proxy for %s:%s: %s\n", 
                options->server_app_id ? options->server_app_id : TERMINAL_APPLICATION_ID,
                TERMINAL_FACTORY_OBJECT_PATH,
                error->message);
    g_error_free (error);
    goto out;
  }

  if (!handle_options (factory, options, &error)) {
    g_dbus_error_strip_remote_error (error);
    g_printerr ("Failed to handle arguments: %s\n", error->message);
  } else {
    exit_code = EXIT_SUCCESS;
  }

  g_object_unref (factory);

out:
  terminal_options_free (options);
  g_free (working_directory);
  g_free (argv_copy);

  return exit_code;
}
