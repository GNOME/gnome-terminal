/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002, 2008-2010 Red Hat, Inc.
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

/* Code copied and adapted from glib/gio/gdbus-tool.c:
 *  * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"
#define _GNU_SOURCE

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <gtk/gtk.h>

#include "terminal-gdbus-generated.h"
#include "terminal-defines.h"
#include "terminal-client-utils.h"
#include "terminal-profiles-list.h"
#include "terminal-i18n.h"
#include "terminal-debug.h"
#include "terminal-libgsystem.h"

static gboolean quiet = FALSE;
static TerminalSettingsList *profiles_list = NULL;

static void _printerr (const char *format, ...) G_GNUC_PRINTF (1, 2);

static void 
_printerr (const char *format, ...)
{
  va_list args;

  if (quiet)
    return;

  va_start (args, format);
  g_vfprintf (stderr, format, args);
  va_end (args);
}

static void
remove_arg (gint num, gint *argc, gchar **argv[])
{
  gint n;

  g_assert (num <= (*argc));

  for (n = num; (*argv)[n] != NULL; n++)
    (*argv)[n] = (*argv)[n+1];
  (*argv)[n] = NULL;
  (*argc) = (*argc) - 1;
}

static void
usage (gint *argc, gchar **argv[], gboolean use_stdout)
{
  GOptionContext *o;
  gs_free char *program_name;
  gs_free char *summary;
  gs_free char *help;

  o = g_option_context_new (_("COMMAND"));
  g_option_context_set_help_enabled (o, FALSE);
  /* Ignore parsing result */
  g_option_context_parse (o, argc, argv, NULL);
  program_name = g_path_get_basename ((*argv)[0]);
  summary = g_strdup_printf (_("Commands:\n"
                               "  help    Shows this information\n"
                               "  run     Create a new terminal running the specified command\n"
                               "  shell   Create a new terminal running the user shell\n"
                               "\n"
                               "Use \"%s COMMAND --help\" to get help on each command.\n"),
                             program_name);
  g_option_context_set_description (o, summary);
  help = g_option_context_get_help (o, FALSE, NULL);
  if (use_stdout)
    g_print ("%s", help);
  else
    _printerr ("%s", help);

  g_option_context_free (o);
}

static void
modify_argv0_for_command (gint *argc, gchar **argv[], const gchar *command)
{
  gchar *s;
  gs_free gchar *program_name;

  /* TODO:
   *  1. get a g_set_prgname() ?; or
   *  2. save old argv[0] and restore later
   */

  g_assert (g_strcmp0 ((*argv)[1], command) == 0);
  remove_arg (1, argc, argv);

  program_name = g_path_get_basename ((*argv)[0]);
  s = g_strdup_printf ("%s %s", (*argv)[0], command);
  (*argv)[0] = s;
}

static TerminalSettingsList *
ensure_profiles_list (void)
{
  if (profiles_list == NULL)
    profiles_list = terminal_profiles_list_new ();

  return profiles_list;
}

typedef struct
{
  char       *server_app_id;

  /* Window options */
  char       *startup_id;
  const char *display_name;
  int         screen_number;
  char       *geometry;
  char       *role;

  gboolean    menubar_state;
  gboolean    start_fullscreen;
  gboolean    start_maximized;

  /* Terminal options */
  char  **exec_argv; /* not owned! */
  int     exec_argc;

  char   *working_directory;
  char   *profile;
  char   *title;
  double  zoom;

  /* Exec options */
  GUnixFDList *fd_list;
  GArray *fd_array;

  /* Processing options */
  gboolean wait;

  /* Flags */
  guint menubar_state_set : 1;
  guint zoom_set          : 1;
  guint active            : 1;
} OptionData;

static gboolean
option_zoom_cb (const gchar *option_name,
                const gchar *value,
                gpointer     user_data,
                GError     **error)
{
  OptionData *data = user_data;
  double zoom;
  char *end;

  /* Try reading a locale-style double first, in case it was
    * typed by a person, then fall back to ascii_strtod (we
    * always save session in C locale format)
    */
  end = NULL;
  errno = 0;
  zoom = g_strtod (value, &end);
  if (end == NULL || *end != '\0')
    {
      g_set_error (error,
                   G_OPTION_ERROR,
                   G_OPTION_ERROR_BAD_VALUE,
                   _("\"%s\" is not a valid zoom factor"),
                   value);
      return FALSE;
    }


  data->zoom = zoom;
  data->zoom_set = TRUE;

  return TRUE;
}

static gboolean
option_app_id_cb (const gchar *option_name,
                    const gchar *value,
                    gpointer     user_data,
                    GError     **error)
{
  OptionData *data = user_data;

  if (!g_application_id_is_valid (value)) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "\"%s\" is not a valid application ID", value);
    return FALSE;
  }

  g_free (data->server_app_id);
  data->server_app_id = g_strdup (value);

  return TRUE;
}

typedef struct {
  int index;
  int fd;
} PassFdElement;

static gboolean
option_fd_cb (const gchar *option_name,
              const gchar *value,
              gpointer     user_data,
              GError     **error)
{
  OptionData *data = user_data;
  int fd = -1;
  PassFdElement e;

  if (strcmp (option_name, "--fd") == 0) {
    char *end = NULL;
    errno = 0;
    fd = g_ascii_strtoll (value, &end, 10);
    if (errno != 0 || end == value) {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   "Invalid argument \"%s\" to --fd option", value);
      return FALSE;
    }

  } else if (strcmp (option_name, "--stdin") == 0) {
    fd = STDIN_FILENO;
  } else if (strcmp (option_name, "--stdout") == 0) {
    fd = STDOUT_FILENO;
  } else if (strcmp (option_name, "--stderr") == 0) {
    fd = STDERR_FILENO;
  } else {
    g_assert_not_reached ();
  }

#if 1
  if (fd == STDIN_FILENO ||
      fd == STDOUT_FILENO ||
      fd == STDERR_FILENO) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "FD passing of std%s is not supported",
                 fd == STDIN_FILENO ? "in" : fd == STDOUT_FILENO ? "out" : "err");
    return FALSE;
  }
#endif

  if (data->fd_list == NULL) {
    data->fd_list = g_unix_fd_list_new ();
    data->fd_array = g_array_new (FALSE, FALSE, sizeof (PassFdElement));
  } else {
    guint i, n;
    n = data->fd_array->len;
    for (i = 0; i < n; i++) {
      e = g_array_index (data->fd_array, PassFdElement, i);
      if (e.fd == fd) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "Cannot pass FD %d twice", fd);
        return FALSE;
      }
    }
  }

  e.fd = fd;
  e.index = g_unix_fd_list_append (data->fd_list, fd, error);
  if (e.index == -1)
    return FALSE;

  g_array_append_val (data->fd_array, e);

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    quiet = TRUE;
  if (fd == STDIN_FILENO)
    data->wait = TRUE;

  return TRUE;
}

static gboolean
option_profile_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     user_data,
                   GError     **error)
{
  OptionData *data = user_data;

  if (data->profile != NULL) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "May only use option %s once", option_name);
    return FALSE;
  }

  data->profile = terminal_profiles_list_dup_uuid (ensure_profiles_list (), value, error);
  return data->profile != NULL;
}

static GOptionContext *
get_goption_context (OptionData *data)
{
  const GOptionEntry global_goptions[] = {
    { "quiet", 0, 0, G_OPTION_ARG_NONE, &quiet, N_("Be quiet"), NULL },
    { NULL }
  };

  const GOptionEntry server_goptions[] = {
    { "app-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_app_id_cb, "Server application ID", "ID" },
    { NULL }
  };

  const GOptionEntry window_goptions[] = {
    { "maximize", 0, 0, G_OPTION_ARG_NONE, &data->start_maximized,
      N_("Maximize the window"), NULL },
    { "fullscreen", 0, 0, G_OPTION_ARG_NONE, &data->start_fullscreen,
      N_("Full-screen the window"), NULL },
    { "geometry", 0, 0, G_OPTION_ARG_STRING, &data->geometry,
      N_("Set the window size; for example: 80x24, or 80x24+200+200 (COLSxROWS+X+Y)"),
      N_("GEOMETRY") },
    { "role", 0, 0, G_OPTION_ARG_STRING, &data->role,
      N_("Set the window role"), N_("ROLE") },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry terminal_goptions[] = {
    { "profile", 0, 0, G_OPTION_ARG_CALLBACK, option_profile_cb,
      N_("Use the given profile instead of the default profile"),
      N_("UUID") },
    { "title", 0, 0, G_OPTION_ARG_STRING, &data->title,
      N_("Set the terminal title"), N_("TITLE") },
    { "cwd", 0, 0, G_OPTION_ARG_FILENAME, &data->working_directory,
      N_("Set the working directory"), N_("DIRNAME") },
    { "zoom", 0, 0, G_OPTION_ARG_CALLBACK, option_zoom_cb,
      N_("Set the terminal's zoom factor (1.0 = normal size)"),
      N_("ZOOM") },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry exec_goptions[] = {
    { "stdin", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_fd_cb,
      N_("Forward stdin"), NULL },
    { "stdout", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_fd_cb,
      N_("Forward stdout"), NULL },
    { "stderr", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, option_fd_cb,
      N_("Forward stderr"), NULL },
    { "fd", 0, 0, G_OPTION_ARG_CALLBACK, option_fd_cb,
      N_("Forward file descriptor"), N_("FD") },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  const GOptionEntry processing_goptions[] = {
    { "wait", 0, 0, G_OPTION_ARG_NONE, &data->wait,
      N_("Wait until the child exits"), NULL },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };


  GOptionContext *context;
  GOptionGroup *group;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_description (context, N_("GNOME Terminal Client"));
  g_option_context_set_ignore_unknown_options (context, FALSE);

  group = g_option_group_new ("global-options",
                              N_("Global options:"),
                              N_("Show global options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, global_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("server-options",
                              N_("Server options:"),
                              N_("Show server options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, server_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("window-options",
                              N_("Window options:"),
                              N_("Show window options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, window_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("terminal-options",
                              N_("Terminal options:"),
                              N_("Show terminal options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, terminal_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("exec-goptions",
                              N_("Exec options:"),
                              N_("Show exec options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, exec_goptions);
  g_option_context_add_group (context, group);

  group = g_option_group_new ("processing-goptions",
                              N_("Processing options:"),
                              N_("Show processing options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, processing_goptions);
  g_option_context_add_group (context, group);

  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  return context;
}

static void
option_data_free (OptionData *data)
{
  if (data == NULL)
    return;

  g_free (data->server_app_id);
  g_free (data->startup_id);
  g_free (data->geometry);
  g_free (data->role);

  g_free (data->working_directory);
  g_free (data->profile);
  g_free (data->title);

  if (data->fd_list)
    g_object_unref (data->fd_list);
  if (data->fd_array)
    g_array_free (data->fd_array, TRUE);

  g_free (data);
}

GS_DEFINE_CLEANUP_FUNCTION0(GOptionContext*, _terminal_local_option_context_free, g_option_context_free)
#define terminal_free_option_context __attribute__ ((__cleanup__(_terminal_local_option_context_free)))

GS_DEFINE_CLEANUP_FUNCTION0(OptionData*, _terminal_local_option_data_free, option_data_free)
#define terminal_free_option_data __attribute__((__cleanup__(_terminal_local_option_data_free)))

static OptionData *
parse_arguments (int *argcp,
                 char ***argvp,
                 GError **error)
{
  OptionData *data;
  terminal_free_option_context GOptionContext *context;
  int argc = *argcp;
  char **argv = *argvp;
  int i;

  data = g_new0 (OptionData, 1);

  /* If there's a '--' argument with other arguments after it, 
   * strip them off. Need to do this before parsing the options!
   */
  data->exec_argv = NULL;
  data->exec_argc = 0;
  for (i = 1; i < argc - 1; i++) {
    if (strcmp (argv[i], "--") == 0) {
      data->exec_argv = &argv[i + 1];
      data->exec_argc = argc - (i + 1);

      /* Truncate argv */
      *argcp = argc = i;
      break;
    }
  }

  /* Need to save this here before calling gtk_init! */
  data->startup_id = g_strdup (g_getenv ("DESKTOP_STARTUP_ID"));

  context = get_goption_context (data);
  if (!g_option_context_parse (context, argcp, argvp, error)) {
    option_data_free (data);
    return NULL;
  }

  if (data->working_directory == NULL)
    data->working_directory = g_get_current_dir ();

  /* Do this here so that gdk_display is initialized */
  if (data->startup_id == NULL)
    terminal_client_get_fallback_startup_id (&data->startup_id);

  data->display_name = gdk_display_get_name (gdk_display_get_default ());

  return data;
}

static GVariant *
build_create_options_variant (OptionData *data)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  terminal_client_append_create_instance_options (&builder,
                                                  data->display_name,
                                                  data->startup_id,
                                                  data->geometry,
                                                  data->role,
                                                  data->profile,
                                                  data->title,
                                                  data->start_maximized,
                                                  data->start_fullscreen);

  return g_variant_builder_end (&builder);
}

/**
 * build_exec_options_variant:
 * 
 * Returns: a floating #GVariant
 */
static GVariant *
build_exec_options_variant (OptionData *data,
                            gboolean shell,
                            GUnixFDList **fd_list)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  terminal_client_append_exec_options (&builder,
                                       data->working_directory,
                                       shell);

  if (data->fd_array != NULL) {
    int i, n_fds;

    g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
    g_variant_builder_add (&builder, "s", "fd-set");

    g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));
    g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(ih)"));
    n_fds = (int) data->fd_array->len;
    for (i = 0; i < n_fds; i++) {
      PassFdElement e =  g_array_index (data->fd_array, PassFdElement, i);

      g_variant_builder_add (&builder, "(ih)", e.fd, e.index);
    }
    g_variant_builder_close (&builder); /* a(ih) */
    g_variant_builder_close (&builder); /* v */

    g_variant_builder_close (&builder); /* {sv} */

    *fd_list = data->fd_list;
    data->fd_list = NULL;
  } else {
    *fd_list = NULL;
  }

  return g_variant_builder_end (&builder);
}

typedef struct {
  GMainLoop *loop;
  int exit_code;
} WaitData;

static void
receiver_child_exited_cb (TerminalReceiver *receiver,
                          int exit_code,
                          WaitData *data)
{
  data->exit_code = exit_code;

  if (g_main_loop_is_running (data->loop))
    g_main_loop_quit (data->loop);
}

static const char *
get_app_id (OptionData *data)
{
  return data->server_app_id ? data->server_app_id : TERMINAL_APPLICATION_ID;
}

static gboolean
handle_open (int *argc,
             char ***argv,
             const char *command,
             int *exit_code)
{
  terminal_free_option_data OptionData *data;
  gs_unref_object TerminalFactory *factory = NULL;
  gs_unref_object TerminalReceiver *receiver = NULL;
  gs_free_error GError *error = NULL;
  gs_free char *object_path = NULL;
  gs_unref_object GUnixFDList *fd_list = NULL;
  GVariant *arguments;

  modify_argv0_for_command (argc, argv, command);

  data = parse_arguments (argc, argv, &error);
  if (data == NULL) {
    _printerr ("Error parsing arguments: %s\n", error->message);
    return FALSE;
  }

  if (g_strcmp0 (command, "run") == 0 && data->exec_argc == 0) {
     _printerr ("\"run\" needs the command to run as arguments after --\n");
    return FALSE;
  }

  factory = terminal_factory_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                     get_app_id (data),
                                                     TERMINAL_FACTORY_OBJECT_PATH,
                                                     NULL /* cancellable */,
                                                     &error);
  if (factory == NULL) {
    g_dbus_error_strip_remote_error (error);
    _printerr ("Error constructing proxy for %s:%s: %s\n",
               get_app_id (data), TERMINAL_FACTORY_OBJECT_PATH,
               error->message);
    return FALSE;
  }

  if (!terminal_factory_call_create_instance_sync
         (factory,
          build_create_options_variant (data),
          &object_path,
          NULL /* cancellable */,
          &error)) {
    g_dbus_error_strip_remote_error (error);
    _printerr ("Error creating terminal: %s\n", error->message);
    return FALSE;
  }

  receiver = terminal_receiver_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                       get_app_id (data),
                                                       object_path,
                                                       NULL /* cancellable */,
                                                       &error);
  if (receiver == NULL) {
    g_dbus_error_strip_remote_error (error);
    _printerr ("Failed to create proxy for terminal: %s\n", error->message);
    return FALSE;
  }

  arguments = build_exec_options_variant (data, g_strcmp0 (command, "shell") == 0, &fd_list);
  if (!terminal_receiver_call_exec_sync (receiver,
                                         arguments,
                                         g_variant_new_bytestring_array ((const char * const *) data->exec_argv, data->exec_argc),
                                         fd_list,
                                         NULL, /* outfdlist */
                                         NULL /* cancellable */,
                                         &error)) {
    g_dbus_error_strip_remote_error (error);
    _printerr ("Error: %s\n", error->message);
    return FALSE;
  }

  if (data->wait) {
    WaitData wait_data;

    wait_data.loop = g_main_loop_new (NULL, FALSE);
    wait_data.exit_code = 255;

    g_signal_connect (receiver, "child-exited", 
                      G_CALLBACK (receiver_child_exited_cb), 
                      &wait_data);
    g_main_loop_run (wait_data.loop);
    g_signal_handlers_disconnect_by_func (receiver,
                                          G_CALLBACK (receiver_child_exited_cb),
                                          &wait_data);

    g_main_loop_unref (wait_data.loop);

    *exit_code = wait_data.exit_code;
  }

  return TRUE;
}

static gboolean
complete (int *argcp,
          char ***argvp)
{
  char *thing;

  modify_argv0_for_command (argcp, argvp, "complete");

  if (*argcp != 2)
    {
      _printerr ("Usage: %s THING\n", (*argvp)[0]);
      return FALSE;
    }

  thing = (*argvp)[1];
  if (g_str_equal (thing, "profiles"))
    {
      gs_strfreev char **profiles;
      char **p;

      profiles = terminal_settings_list_dupv_children (ensure_profiles_list ());
      if (profiles == NULL)
        return TRUE;

      for (p = profiles; *p; p++)
        g_print ("%s\n", *p);
      return TRUE;
    }

  _printerr ("Unknown completion requested\n");
  return FALSE;
}

static int
mangle_exit_code (int status)
{
  if (WIFEXITED (status))
    return WEXITSTATUS (status);
  else if (WIFSIGNALED (status))
    return 128 + (WTERMSIG (status));
  else
    return 127;
}

gint
main (gint argc, gchar *argv[])
{
  int ret;
  int exit_code = 0;
  const gchar *command;

  setlocale (LC_ALL, "");

  terminal_i18n_init (TRUE);

  _terminal_debug_init ();

  ret = EXIT_FAILURE;

  if (argc < 2)
    {
      usage (&argc, &argv, FALSE);
      goto out;
    }

  command = argv[1];
  if (g_strcmp0 (command, "help") == 0)
    {
      usage (&argc, &argv, TRUE);
      ret = EXIT_SUCCESS;
    }
  else if (g_strcmp0 (command, "run") == 0 ||
           g_strcmp0 (command, "shell") == 0)
    {
      if (handle_open (&argc,
                       &argv,
                       command,
                       &exit_code))
        ret = mangle_exit_code (exit_code);
      else
        ret = 127;
    }
  else if (g_strcmp0 (command, "complete") == 0)
    {
      if (complete (&argc, &argv))
        ret = EXIT_SUCCESS;
    }
  else
    {
      _printerr ("Unknown command `%s'\n", command);
      usage (&argc, &argv, FALSE);
    }

 out:

  g_clear_object (&profiles_list);

  return ret;
}
