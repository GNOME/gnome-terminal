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
#include "terminal-libgsystem.h"

GS_DEFINE_CLEANUP_FUNCTION0(TerminalOptions*, gs_local_options_free, terminal_options_free)
#define gs_free_options __attribute__ ((cleanup(gs_local_options_free)))

static gboolean
get_factory_exit_status (const char *message,
                         const char *service_name,
                         int *exit_status)
{
  gs_free char *pattern = NULL, *number = NULL;
  gs_unref_regex GRegex *regex = NULL;
  gs_free_match_info GMatchInfo *match_info = NULL;
  gint64 v;
  char *end;
  GError *err = NULL;

  pattern = g_strdup_printf ("org.freedesktop.DBus.Error.Spawn.ChildExited: Process %s exited with status (\\d+)$", service_name);
  regex = g_regex_new (pattern, 0, 0, &err);
  g_assert_no_error (err);

  if (!g_regex_match (regex, message, 0, &match_info))
    return FALSE;

  number = g_match_info_fetch (match_info, 1);
  g_assert_true (number != NULL);

  errno = 0;
  v = g_ascii_strtoll (number, &end, 10);
  if (errno || end == number || *end != '\0' || v < 0 || v > G_MAXINT)
    return FALSE;

  *exit_status = (int)v;
  return TRUE;
}

static gboolean
handle_factory_error (GError *error,
                      const char *service_name)
{
  int exit_status;

  if (!g_dbus_error_is_remote_error (error) ||
      !g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SPAWN_CHILD_EXITED) ||
      !get_factory_exit_status (error->message, service_name, &exit_status))
    return FALSE;

  g_dbus_error_strip_remote_error (error);
  g_printerr ("%s\n\n", error->message);

  switch (exit_status) {
  case _EXIT_FAILURE_WRONG_ID:
    g_printerr ("You tried to run gnome-terminal-server with elevated privileged. This is not supported.\n");
    break;
  case _EXIT_FAILURE_NO_UTF8:
    g_printerr ("The environment that gnome-terminal-server was launched with specified a non-UTF-8 locale. This is not supported.\n");
    break;
  case _EXIT_FAILURE_UNSUPPORTED_LOCALE:
    g_printerr ("The environment that gnome-terminal-server was launched with specified an unsupported locale.\n");
    break;
  case _EXIT_FAILURE_GTK_INIT:
    g_printerr ("The environment that gnome-terminal-server was launched with most likely contained an incorrect or unset \"DISPLAY\" variable.\n");
    break;
  default:
    break;
  }
  g_printerr ("See https://wiki.gnome.org/Apps/Terminal/FAQ#Exit_status_%d for more information.\n", exit_status);

  return TRUE;
}

static gboolean
handle_create_instance_error (GError *error,
                              const char *service_name)
{
  if (handle_factory_error (error, service_name))
    return TRUE;

  g_dbus_error_strip_remote_error (error);
  g_printerr ("Error creating terminal: %s\n", error->message);
  return FALSE; /* don't abort */
}

static gboolean
handle_create_receiver_proxy_error (GError *error,
                                    const char *service_name,
                                    const char *object_path)
{
  if (handle_factory_error (error, service_name))
    return TRUE;

  g_dbus_error_strip_remote_error (error);
  g_printerr ("Failed to create proxy for terminal: %s\n", error->message);
  return FALSE; /* don't abort */
}

static gboolean
handle_exec_error (GError *error,
                   const char *service_name)
{
  if (handle_factory_error (error, service_name))
    return TRUE;

  g_dbus_error_strip_remote_error (error);
  g_printerr ("Error: %s\n", error->message);
  return FALSE; /* don't abort */
}

static void
handle_show_preferences (const char *service_name)
{
  gs_free_error GError *error = NULL;
  gs_unref_object GDBusConnection *bus = NULL;
  gs_free char *object_path = NULL;
  GVariantBuilder builder;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (bus == NULL) {
    g_printerr ("Failed to get session bus: %s\n", error->message);
    return;
  }

  /* For reasons (!?), the org.gtk.Actions interface's object path
   * is derived from the service name, i.e. for service name
   * "foo.bar.baz" the object path is "/foo/bar/baz".
   */
  object_path = g_strdelimit (g_strdup_printf (".%s", service_name), ".", '/');

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(sava{sv})"));
  g_variant_builder_add (&builder, "s", "preferences");
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("av"));
  g_variant_builder_close (&builder);
  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_close (&builder);

  if (!g_dbus_connection_call_sync (bus,
                                    service_name,
                                    object_path,
                                    "org.gtk.Actions",
                                    "Activate",
                                    g_variant_builder_end (&builder),
                                    G_VARIANT_TYPE ("()"),
                                    G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                    30 * 1000 /* ms timeout */,
                                    NULL /* cancelleable */,
                                    &error)) {
    g_printerr ("Activate call failed: %s\n", error->message);
    return;
  }
}

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
                const char *service_name,
                TerminalOptions *options)
{
  GList *lw;
  const char *encoding;

  /* We need to forward the locale encoding to the server, see bug #732128 */
  g_get_charset (&encoding);

  if (options->show_preferences) {
    handle_show_preferences (service_name);
  } else {
    /* Make sure we open at least one window */
    terminal_options_ensure_window (options);
  }

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
          char **argv;
          int argc;
          char *p;
          gs_free_error GError *err = NULL;
          gs_free char *object_path = NULL;
          gs_unref_object TerminalReceiver *receiver = NULL;

          g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

          terminal_client_append_create_instance_options (&builder,
                                                          options->display_name,
                                                          options->startup_id,
                                                          iw->geometry,
                                                          iw->role,
                                                          it->profile ? it->profile : options->default_profile,
                                                          encoding,
                                                          it->title ? it->title : options->default_title,
                                                          it->active,
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

          if (!terminal_factory_call_create_instance_sync 
                 (factory,
                  g_variant_builder_end (&builder),
                  &object_path,
                  NULL /* cancellable */,
                  &err)) {
            if (handle_create_instance_error (err, service_name))
              return FALSE;
            else
              continue; /* Continue processing the remaining options! */
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
            if (handle_create_receiver_proxy_error (err, service_name, object_path))
              return FALSE;
            else
              continue; /* Continue processing the remaining options! */
          }

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
            if (handle_exec_error (err, service_name))
              return FALSE;
            else
              continue; /* Continue processing the remaining options! */
          }
        }
    }

  return TRUE;
}

int
main (int argc, char **argv)
{
  int i;
  gs_free char **argv_copy = NULL;
  const char *startup_id, *display_name;
  GdkDisplay *display;
  gs_free_options TerminalOptions *options = NULL;
  gs_unref_object TerminalFactory *factory = NULL;
  gs_free_error GError *error = NULL;
  gs_free char *working_directory = NULL;
  int exit_code = EXIT_FAILURE;
  const char *service_name;

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
    goto out;
  }

  g_set_application_name (_("Terminal"));

  /* Do this here so that gdk_display is initialized */
  if (options->startup_id == NULL)
    options->startup_id = terminal_client_get_fallback_startup_id ();

  display = gdk_display_get_default ();
  display_name = gdk_display_get_name (display);
  options->display_name = g_strdup (display_name);

  service_name = options->server_app_id ? options->server_app_id : TERMINAL_APPLICATION_ID;

  factory = terminal_factory_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                     service_name,
                                                     TERMINAL_FACTORY_OBJECT_PATH,
                                                     NULL /* cancellable */,
                                                     &error);
  if (factory == NULL) {
    if (!handle_factory_error (error, service_name))
      g_printerr ("Error constructing proxy for %s:%s: %s\n",
                  service_name, TERMINAL_FACTORY_OBJECT_PATH, error->message);

    goto out;
  }

  if (handle_options (factory, service_name, options))
    exit_code = EXIT_SUCCESS;

 out:
  return exit_code;
}
