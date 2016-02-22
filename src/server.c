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

#include <config.h>

#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-gdbus.h"
#include "terminal-i18n.h"
#include "terminal-defines.h"
#include "terminal-libgsystem.h"

static char *app_id = NULL;

#define INACTIVITY_TIMEOUT (100 /* ms */)

static gboolean
option_app_id_cb (const gchar *option_name,
                    const gchar *value,
                    gpointer     data,
                    GError     **error)
{
  if (!g_application_id_is_valid (value)) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "\"%s\" is not a valid application ID", value);
    return FALSE;
  }

  g_free (app_id);
  app_id = g_strdup (value);

  return TRUE;
}

static const GOptionEntry options[] = {
  { "app-id", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_app_id_cb, "Application ID", "ID" },
  { NULL }
};

/* We use up to 8 FDs per terminal, so let's bump the limit way up.
 * However we need to restore the original limit for the child processes.
 */

static struct rlimit sv_rlimit_nofile;

static void
atfork_child_restore_rlimit_nofile (void)
{
  if (setrlimit (RLIMIT_NOFILE, &sv_rlimit_nofile) < 0)
    _exit (127);
}

static gboolean
increase_rlimit_nofile (void)
{
  struct rlimit l;

  if (getrlimit (RLIMIT_NOFILE, &sv_rlimit_nofile) < 0)
    return FALSE;

  if (pthread_atfork (NULL, NULL, atfork_child_restore_rlimit_nofile) != 0)
    return FALSE;

  l.rlim_cur = l.rlim_max = sv_rlimit_nofile.rlim_max;
  if (setrlimit (RLIMIT_NOFILE, &l) < 0)
    return FALSE;

  return TRUE;
}

int
main (int argc, char **argv)
{
  gs_unref_object GApplication *app = NULL;
  const char *home_dir, *charset;
  GError *error = NULL;

  if (G_UNLIKELY ((getuid () != geteuid () ||
                  getgid () != getegid ()) &&
                  geteuid () == 0 &&
                  getegid () == 0)) {
    g_printerr ("Wrong euid/egid, exiting.\n");
    return _EXIT_FAILURE_WRONG_ID;
  }

  if (setlocale (LC_ALL, "") == NULL) {
    g_printerr ("Locale not supported.\n");
    return _EXIT_FAILURE_UNSUPPORTED_LOCALE;
  }

  terminal_i18n_init (TRUE);

  if (!g_get_charset (&charset)) {
    g_printerr ("Non UTF-8 locale (%s) is not supported!\n", charset);
    return _EXIT_FAILURE_NO_UTF8;
  }

  /* Sanitise environment */
  g_unsetenv ("DBUS_STARTER_BUS_TYPE");

  /* Not interested in silly debug spew polluting the journal, bug #749195 */
  if (g_getenv ("G_ENABLE_DIAGNOSTIC") == NULL)
    g_setenv ("G_ENABLE_DIAGNOSTIC", "0", TRUE);

#ifndef ENABLE_DISTRO_PACKAGING
#ifdef HAVE_UBUNTU
  /* Set some env vars to disable the ubuntu modules. Their package will be 
   * built using --enable-distro-packaging, but anyone running from git will
   * get the right behaviour.
   */
  g_setenv ("LIBOVERLAY_SCROLLBAR", "0", TRUE);
  g_setenv ("UBUNTU_MENUPROXY", "0", TRUE);
  g_setenv ("NO_UNITY_GTK_MODULE", "1", TRUE);
#endif
#endif

  _terminal_debug_init ();

  // FIXMEchpe: just use / here but make sure #565328 doesn't regress
  /* Change directory to $HOME so we don't prevent unmounting, e.g. if the
   * factory is started by nautilus-open-terminal. See bug #565328.
   * On failure back to /.
   */
  home_dir = g_get_home_dir ();
  if (home_dir == NULL || chdir (home_dir) < 0)
    (void) chdir ("/");

  g_set_prgname ("gnome-terminal-server");
  g_set_application_name (_("Terminal"));

  if (!gtk_init_with_args (&argc, &argv, NULL, options, NULL, &error)) {
    g_printerr ("Failed to parse arguments: %s\n", error->message);
    g_error_free (error);
    exit (_EXIT_FAILURE_GTK_INIT);
  }

  if (!increase_rlimit_nofile ()) {
    g_printerr ("Failed to increase RLIMIT_NOFILE: %m\n");
  }

  /* Now we can create the app */
  app = terminal_app_new (app_id);
  g_free (app_id);

  /* We stay around a bit after the last window closed */
  g_application_set_inactivity_timeout (app, INACTIVITY_TIMEOUT);

  return g_application_run (app, 0, NULL);
}
