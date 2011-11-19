/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002, 2008-2010 Red Hat, Inc.
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

/* Code copied and adapted from glib/gio/gdbus-tool.c:
 *  * Author: David Zeuthen <davidz@redhat.com>
 */

#include <config.h>

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gio.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "terminal-intl.h"
#include "terminal-gdbus-generated.h"
#include "terminal-defines.h"

/* ---------------------------------------------------------------------------------------------------- */

G_GNUC_UNUSED static void completion_debug (const gchar *format, ...);

/* Uncomment to get debug traces in /tmp/gdbus-completion-debug.txt (nice
 * to not have it interfere with stdout/stderr)
 */
#if 0
G_GNUC_UNUSED static void
completion_debug (const gchar *format, ...)
{
  va_list var_args;
  gchar *s;
  static FILE *f = NULL;

  va_start (var_args, format);
  s = g_strdup_vprintf (format, var_args);
  if (f == NULL)
    {
      f = fopen ("/tmp/gdbus-completion-debug.txt", "a+");
    }
  fprintf (f, "%s\n", s);
  g_free (s);
}
#else
static void
completion_debug (const gchar *format, ...)
{
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

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
  gchar *s;
  gchar *program_name;

  o = g_option_context_new (_("COMMAND"));
  g_option_context_set_help_enabled (o, FALSE);
  /* Ignore parsing result */
  g_option_context_parse (o, argc, argv, NULL);
  program_name = g_path_get_basename ((*argv)[0]);
  s = g_strdup_printf (_("Commands:\n"
                         "  help    Shows this information\n"
                         "  open    Create a new terminal\n"
                         "\n"
                         "Use \"%s COMMAND --help\" to get help on each command.\n"),
                       program_name);
  g_free (program_name);
  g_option_context_set_description (o, s);
  g_free (s);
  s = g_option_context_get_help (o, FALSE, NULL);
  if (use_stdout)
    g_print ("%s", s);
  else
    g_printerr ("%s", s);
  g_free (s);
  g_option_context_free (o);
}

static void
modify_argv0_for_command (gint *argc, gchar **argv[], const gchar *command)
{
  gchar *s;
  gchar *program_name;

  /* TODO:
   *  1. get a g_set_prgname() ?; or
   *  2. save old argv[0] and restore later
   */

  g_assert (g_strcmp0 ((*argv)[1], command) == 0);
  remove_arg (1, argc, argv);

  program_name = g_path_get_basename ((*argv)[0]);
  s = g_strdup_printf ("%s %s", (*argv)[0], command);
  (*argv)[0] = s;
  g_free (program_name);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Copied from libnautilus/nautilus-program-choosing.c; Needed in case
 * we have no DESKTOP_STARTUP_ID (with its accompanying timestamp).
 */
static char *
slowly_and_stupidly_obtain_timestamp (void)
{
  Display *xdisplay;
  Window xwindow;
  XEvent event;

  xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

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

  return g_strdup_printf ("_TIME%lu", event.xproperty.time);
}

typedef struct
{
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

static GOptionContext *
get_goption_context (OptionData *data)
{
  const GOptionEntry window_goptions[] = {
    { "maximize", 0, 0, G_OPTION_ARG_NONE, &data->start_maximized,
      N_("Maximise the window"), NULL },
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
    { "profile", 0, 0, G_OPTION_ARG_STRING, &data->profile,
      N_("Use the given profile instead of the default profile"),
      N_("PROFILE-NAME") },
    { "title", 0, 0, G_OPTION_ARG_STRING, &data->title,
      N_("Set the terminal title"), N_("TITLE") },
    { "cwd", 0, 0, G_OPTION_ARG_STRING, &data->working_directory,
      N_("Set the working directory"), N_("DIRNAME") },
    { "zoom", 0, 0, G_OPTION_ARG_CALLBACK, option_zoom_cb,
      N_("Set the terminal's zoom factor (1.0 = normal size)"),
      N_("ZOOM") },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

  GOptionContext *context;
  GOptionGroup *group;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_description (context, N_("GNOME Terminal Client"));
  g_option_context_set_ignore_unknown_options (context, FALSE);

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
                              N_("Show per-terminal options"),
                              data,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, terminal_goptions);
  g_option_context_add_group (context, group);

  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  return context;
}

static void
option_data_free (OptionData *data)
{
  if (data == NULL)
    return;

  g_free (data->startup_id);
  g_free (data->geometry);
  g_free (data->role);

  g_free (data->working_directory);
  g_free (data->profile);
  g_free (data->title);
}

static OptionData *
parse_arguments (int *argcp,
                 char ***argvp,
                 GError **error)
{
  OptionData *data;
  GOptionContext *context;
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
      *argcp = argc = i - 1;
      break;
    }
  }

  data->working_directory = g_get_current_dir ();

  /* Need to save this here before calling gtk_init! */
  data->startup_id = g_strdup (g_getenv ("DESKTOP_STARTUP_ID"));

  context = get_goption_context (data);
  if (!g_option_context_parse (context, argcp, argvp, error)) {
    option_data_free (data);
    g_option_context_free (context);
    return NULL;
  }
  g_option_context_free (context);

  /* Do this here so that gdk_display is initialized */
  if (data->startup_id == NULL)
    data->startup_id = slowly_and_stupidly_obtain_timestamp ();

  data->display_name = gdk_display_get_name (gdk_display_get_default ());

  return data;
}

/**
 * build_create_options_variant:
 * 
 * Returns: a floating #GVariant
 */
static GVariant *
build_create_options_variant (OptionData *data)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}",
                         "display", g_variant_new_bytestring (data->display_name));

  if (data->startup_id)
    g_variant_builder_add (&builder, "{sv}", 
                           "desktop-startup-id", g_variant_new_bytestring (data->startup_id));
  if (data->geometry)
    g_variant_builder_add (&builder, "{sv}", 
                           "geometry", g_variant_new_string (data->geometry));
  if (data->role)
    g_variant_builder_add (&builder, "{sv}", 
                           "role", g_variant_new_string (data->role));
  if (data->start_maximized)
    g_variant_builder_add (&builder, "{sv}", 
                           "maximize-window", g_variant_new_boolean (TRUE));
  if (data->start_fullscreen)
    g_variant_builder_add (&builder, "{sv}", 
                           "fullscreen-window", g_variant_new_boolean (TRUE));

  return g_variant_builder_end (&builder);
}

/**
 * build_exec_options_variant:
 * 
 * Returns: a floating #GVariant
 */
static GVariant *
build_exec_options_variant (OptionData *data)
{
  GVariantBuilder builder;
  char **envv;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  if (data->working_directory)
    g_variant_builder_add (&builder, "{sv}", 
                           "cwd", g_variant_new_bytestring (data->working_directory));

  envv = g_get_environ ();
  if (envv) {
    envv = g_environ_unsetenv (envv, "DESKTOP_STARTUP_ID");
    envv = g_environ_unsetenv (envv, "GIO_LAUNCHED_DESKTOP_FILE_PID");
    envv = g_environ_unsetenv (envv, "GIO_LAUNCHED_DESKTOP_FILE");

    g_variant_builder_add (&builder, "{sv}",
                           "environ",
                           g_variant_new_bytestring_array ((const char * const *) envv, -1));
    g_strfreev (envv);
  }

  return g_variant_builder_end (&builder);
}

static gboolean
handle_open (int *argc,
             char ***argv,
             gboolean request_completion,
             const gchar *completion_cur,
             const gchar *completion_prev)
{
  OptionData *data;
  TerminalFactory *factory;
  TerminalReceiver *receiver;
  GError *error = NULL;
  char *object_path;

  modify_argv0_for_command (argc, argv, "open");

  data = parse_arguments (argc, argv, &error);
  if (data == NULL) {
    g_printerr ("Error parsing arguments: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }

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
    option_data_free (data);
    return FALSE;
  }

  if (!terminal_factory_call_create_instance_sync 
         (factory,
          build_create_options_variant (data),
          &object_path,
          NULL /* cancellable */,
          &error)) {
    g_printerr ("Error creating terminal: %s\n", error->message);
    g_error_free (error);
    g_object_unref (factory);
    option_data_free (data);
    return FALSE;
  }

  g_object_unref (factory);

  receiver = terminal_receiver_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                       TERMINAL_UNIQUE_NAME,
                                                       object_path,
                                                       NULL /* cancellable */,
                                                       &error);
  if (receiver == NULL) {
    g_printerr ("Failed to create proxy for terminal: %s\n", error->message);
    g_error_free (error);
    g_free (object_path);
    option_data_free (data);
    return FALSE;
  }

  g_free (object_path);

  if (!terminal_receiver_call_exec_sync (receiver,
                                         build_exec_options_variant (data),
                                         g_variant_new_bytestring_array ((const char * const *) data->exec_argv, data->exec_argc),
                                         NULL /* cancellable */,
                                         &error)) {
    g_printerr ("Error: %s\n", error->message);
    g_error_free (error);
    g_object_unref (receiver);
    option_data_free (data);
    return FALSE;
  }

  option_data_free (data);

  g_object_unref (receiver);

  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static gchar *
pick_word_at (const gchar  *s,
              gint          cursor,
              gint         *out_word_begins_at)
{
  gint begin;
  gint end;

  if (s[0] == '\0')
    {
      if (out_word_begins_at != NULL)
        *out_word_begins_at = -1;
      return NULL;
    }

  if (g_ascii_isspace (s[cursor]) && ((cursor > 0 && g_ascii_isspace(s[cursor-1])) || cursor == 0))
    {
      if (out_word_begins_at != NULL)
        *out_word_begins_at = cursor;
      return g_strdup ("");
    }

  while (!g_ascii_isspace (s[cursor - 1]) && cursor > 0)
    cursor--;
  begin = cursor;

  end = begin;
  while (!g_ascii_isspace (s[end]) && s[end] != '\0')
    end++;

  if (out_word_begins_at != NULL)
    *out_word_begins_at = begin;

  return g_strndup (s + begin, end - begin);
}

gint
main (gint argc, gchar *argv[])
{
  int ret;
  const gchar *command;
  gboolean request_completion;
  gchar *completion_cur;
  gchar *completion_prev;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_type_init ();

  ret = EXIT_FAILURE;
  completion_cur = NULL;
  completion_prev = NULL;

  g_type_init ();

  if (argc < 2)
    {
      usage (&argc, &argv, FALSE);
      goto out;
    }

  request_completion = FALSE;

  //completion_debug ("---- argc=%d --------------------------------------------------------", argc);

 again:
  command = argv[1];
  if (g_strcmp0 (command, "help") == 0)
    {
      if (request_completion)
        {
          /* do nothing */
        }
      else
        {
          usage (&argc, &argv, TRUE);
          ret = EXIT_SUCCESS;
        }
      goto out;
    }
  else if (g_strcmp0 (command, "open") == 0)
    {
      if (handle_open (&argc,
                       &argv,
                       request_completion,
                       completion_cur,
                       completion_prev))
        ret = EXIT_SUCCESS;
      goto out;
    }
  else if (g_strcmp0 (command, "complete") == 0 && argc == 4 && !request_completion)
    {
      const gchar *completion_line;
      gchar **completion_argv;
      gint completion_argc;
      gint completion_point;
      gchar *endp;
      gint cur_begin;

      request_completion = TRUE;

      completion_line = argv[2];
      completion_point = strtol (argv[3], &endp, 10);
      if (endp == argv[3] || *endp != '\0')
        goto out;

#if 0
      completion_debug ("completion_point=%d", completion_point);
      completion_debug ("----");
      completion_debug (" 0123456789012345678901234567890123456789012345678901234567890123456789");
      completion_debug ("`%s'", completion_line);
      completion_debug (" %*s^",
                         completion_point, "");
      completion_debug ("----");
#endif

      if (!g_shell_parse_argv (completion_line,
                               &completion_argc,
                               &completion_argv,
                               NULL))
        {
          /* it's very possible the command line can't be parsed (for
           * example, missing quotes etc) - in that case, we just
           * don't autocomplete at all
           */
          goto out;
        }

      /* compute cur and prev */
      completion_prev = NULL;
      completion_cur = pick_word_at (completion_line, completion_point, &cur_begin);
      if (cur_begin > 0)
        {
          gint prev_end;
          for (prev_end = cur_begin - 1; prev_end >= 0; prev_end--)
            {
              if (!g_ascii_isspace (completion_line[prev_end]))
                {
                  completion_prev = pick_word_at (completion_line, prev_end, NULL);
                  break;
                }
            }
        }
#if 0
      completion_debug (" cur=`%s'", completion_cur);
      completion_debug ("prev=`%s'", completion_prev);
#endif

      argc = completion_argc;
      argv = completion_argv;

      ret = EXIT_SUCCESS;

      goto again;
    }
  else
    {
      if (request_completion)
        {
          g_print ("help \nopen \n");
          ret = EXIT_SUCCESS;
          goto out;
        }
      else
        {
          g_printerr ("Unknown command `%s'\n", command);
          usage (&argc, &argv, FALSE);
          goto out;
        }
    }

 out:
  g_free (completion_cur);
  g_free (completion_prev);

  return ret;
}
