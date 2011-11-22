/*
 *  Copyright (C) 2004, 2005 Free Software Foundation, Inc.
 *  Copyright © 2011 Christian Persch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Christian Neumair <chris@gnome-de.org>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libnautilus-extension/nautilus-menu-provider.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h> /* for atoi */
#include <string.h> /* for strcmp */
#include <unistd.h> /* for chdir */
#include <sys/stat.h>

#define TERMINAL_TYPE_NAUTILUS         (terminal_nautilus_get_type ())
#define TERMINAL_NAUTILUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_NAUTILUS, TerminalNautilus))
#define TERMINAL_TERMINAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_NAUTILUS, TerminalNautilusClass))
#define TERMINAL_IS_NAUTILUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_NAUTILUS))
#define TERMINAL_IS_TERMINAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_NAUTILUS))
#define TERMINAL_TERMINAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_NAUTILUS, TerminalNautilusClass))

typedef struct _TerminalNautilus      TerminalNautilus;
typedef struct _TerminalNautilusClass TerminalNautilusClass;

struct _TerminalNautilus {
        GObject parent_instance;
};

struct _TerminalNautilusClass {
        GObjectClass parent_class;
};

static GType terminal_nautilus_get_type (void);

/* BEGIN gnome-desktop */

/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-desktop-utils.c - Utilities for the GNOME Desktop

   Copyright (C) 1998 Tom Tromey
   All rights reserved.

   This file is part of the Gnome Library.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/*
  @NOTATION@
 */

#include <glib.h>
#include <glib/gi18n-lib.h>

/**
 * gnome_desktop_prepend_terminal_to_vector:
 * @argc: a pointer to the vector size
 * @argv: a pointer to the vector
 *
 * Description:  Prepends a terminal (either the one configured as default in
 * the user's GNOME setup, or one of the common xterm emulators) to the passed
 * in vector, modifying it in the process.  The vector should be allocated with
 * #g_malloc, as this will #g_free the original vector.  Also all elements must
 * have been allocated separately.  That is the standard glib/GNOME way of
 * doing vectors however.  If the integer that @argc points to is negative, the
 * size will first be computed.  Also note that passing in pointers to a vector
 * that is empty, will just create a new vector for you.
 **/
static void
gnome_desktop_prepend_terminal_to_vector (int *argc, char ***argv)
{
#ifndef G_OS_WIN32
        char **real_argv;
        int real_argc;
        int i, j;
        char **term_argv = NULL;
        int term_argc = 0;
        GSettings *settings;

        gchar *terminal = NULL;

        char **the_argv;

        g_return_if_fail (argc != NULL);
        g_return_if_fail (argv != NULL);

//         _gnome_desktop_init_i18n ();

        /* sanity */
        if(*argv == NULL)
                *argc = 0;

        the_argv = *argv;

        /* compute size if not given */
        if (*argc < 0) {
                for (i = 0; the_argv[i] != NULL; i++)
                        ;
                *argc = i;
        }

        settings = g_settings_new ("org.gnome.desktop.default-applications.terminal");
        terminal = g_settings_get_string (settings, "exec");

        if (terminal) {
                gchar *command_line;
                gchar *exec_flag;

                exec_flag = g_settings_get_string (settings, "exec-arg");

                if (exec_flag == NULL)
                        command_line = g_strdup (terminal);
                else
                        command_line = g_strdup_printf ("%s %s", terminal,
                                                        exec_flag);

                g_shell_parse_argv (command_line,
                                    &term_argc,
                                    &term_argv,
                                    NULL /* error */);

                g_free (command_line);
                g_free (exec_flag);
                g_free (terminal);
        }

        g_object_unref (settings);

        if (term_argv == NULL) {
                char *check;

                term_argc = 2;
                term_argv = g_new0 (char *, 3);

                check = g_find_program_in_path ("gnome-terminal");
                if (check != NULL) {
                        term_argv[0] = check;
                        /* Note that gnome-terminal takes -x and
                         * as -e in gnome-terminal is broken we use that. */
                        term_argv[1] = g_strdup ("-x");
                } else {
                        if (check == NULL)
                                check = g_find_program_in_path ("nxterm");
                        if (check == NULL)
                                check = g_find_program_in_path ("color-xterm");
                        if (check == NULL)
                                check = g_find_program_in_path ("rxvt");
                        if (check == NULL)
                                check = g_find_program_in_path ("xterm");
                        if (check == NULL)
                                check = g_find_program_in_path ("dtterm");
                        if (check == NULL) {
                                g_warning (_("Cannot find a terminal, using "
                                             "xterm, even if it may not work"));
                                check = g_strdup ("xterm");
                        }
                        term_argv[0] = check;
                        term_argv[1] = g_strdup ("-e");
                }
        }

        real_argc = term_argc + *argc;
        real_argv = g_new (char *, real_argc + 1);

        for (i = 0; i < term_argc; i++)
                real_argv[i] = term_argv[i];

        for (j = 0; j < *argc; j++, i++)
                real_argv[i] = (char *)the_argv[j];

        real_argv[i] = NULL;

        g_free (*argv);
        *argv = real_argv;
        *argc = real_argc;

        /* we use g_free here as we sucked all the inner strings
         * out from it into real_argv */
        g_free (term_argv);
#else
        /* FIXME: Implement when needed */
        g_warning ("gnome_prepend_terminal_to_vector: Not implemented");
#endif
}

/* END gnome-desktop */

/* BEGIN eel-gnome-extensions */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gnome-extensions.h - interface for new functions that operate on
                                 gnome classes. Perhaps some of these should be
                                 rolled into gnome someday.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef _NOT_EEL_GNOME_EXTENSIONS_H
#define _NOT_EEL_GNOME_EXTENSIONS_H

#include <gtk/gtk.h>

/* Return a command string containing the path to a terminal on this system. */
char *        _not_eel_gnome_make_terminal_command                         (const char               *command);

/* Open up a new terminal, optionally passing in a command to execute */
void          _not_eel_gnome_open_terminal                                 (const char               *command);
void          _not_eel_gnome_open_terminal_on_screen                       (const char               *command,
                                                                            GdkScreen                *screen);
                                                                 
#endif /* _NOT_EEL_GNOME_EXTENSIONS_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gnome-extensions.c - implementation of new functions that operate on
                            gnome classes. Perhaps some of these should be
                            rolled into gnome someday.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>

#include <X11/Xatom.h>
#include <errno.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Return a command string containing the path to a terminal on this system. */



static char *
try_terminal_command (const char *program,
                      const char *args)
{
        char *program_in_path, *quoted, *result;

        if (program == NULL) {
                return NULL;
        }

        program_in_path = g_find_program_in_path (program);
        if (program_in_path == NULL) {
                return NULL;
        }

        quoted = g_shell_quote (program_in_path);
        if (args == NULL || args[0] == '\0') {
                return quoted;
        }
        result = g_strconcat (quoted, " ", args, NULL);
        g_free (quoted);
        return result;
}

static char *
try_terminal_command_argv (int argc,
                           char **argv)
{
        GString *string;
        int i;
        char *quoted, *result;

        if (argc == 0) {
                return NULL;
        }

        if (argc == 1) {
                return try_terminal_command (argv[0], NULL);
        }
        
        string = g_string_new (argv[1]);
        for (i = 2; i < argc; i++) {
                quoted = g_shell_quote (argv[i]);
                g_string_append_c (string, ' ');
                g_string_append (string, quoted);
                g_free (quoted);
        }
        result = try_terminal_command (argv[0], string->str);
        g_string_free (string, TRUE);

        return result;
}

static char *
get_terminal_command_prefix (gboolean for_command)
{
        int argc;
        char **argv;
        char *command;
        guint i;
        static const char *const commands[][3] = {
                { "gnome-terminal", "-x",                                      "" },
                { "dtterm",         "-e",                                      "-ls" },
                { "nxterm",         "-e",                                      "-ls" },
                { "color-xterm",    "-e",                                      "-ls" },
                { "rxvt",           "-e",                                      "-ls" },
                { "xterm",          "-e",                                      "-ls" },
        };

        /* Try the terminal from preferences. Use without any
         * arguments if we are just doing a standalone terminal.
         */
        argc = 0;
        argv = g_new0 (char *, 1);
        gnome_desktop_prepend_terminal_to_vector (&argc, &argv);

        command = NULL;
        if (argc != 0) {
                if (for_command) {
                        command = try_terminal_command_argv (argc, argv);
                } else {
                        /* Strip off the arguments in a lame attempt
                         * to make it be an interactive shell.
                         */
                        command = try_terminal_command (argv[0], NULL);
                }
        }

        while (argc != 0) {
                g_free (argv[--argc]);
        }
        g_free (argv);

        if (command != NULL) {
                return command;
        }

        /* Try well-known terminal applications in same order that gmc did. */
        for (i = 0; i < G_N_ELEMENTS (commands); i++) {
                command = try_terminal_command (commands[i][0],
                                                commands[i][for_command ? 1 : 2]);
                if (command != NULL) {
                        break;
                }
        }
        
        return command;
}

char *
_not_eel_gnome_make_terminal_command (const char *command)
{
        char *prefix, *quoted, *terminal_command;

        if (command == NULL) {
                return get_terminal_command_prefix (FALSE);
        }
        prefix = get_terminal_command_prefix (TRUE);
        quoted = g_shell_quote (command);
        terminal_command = g_strconcat (prefix, " /bin/sh -c ", quoted, NULL);
        g_free (prefix);
        g_free (quoted);
        return terminal_command;
}

void
_not_eel_gnome_open_terminal_on_screen (const char *command,
                                        GdkScreen  *screen)
{
        char *command_line;
        GAppInfo *app;
        GdkAppLaunchContext *ctx;
        GError *error = NULL;
        GdkDisplay *display;

        if (screen == NULL) {
                screen = gdk_screen_get_default ();
        }
        
        command_line = _not_eel_gnome_make_terminal_command (command);
        if (command_line == NULL) {
                g_message ("Could not start a terminal");
                return;
        }

        app = g_app_info_create_from_commandline (command_line, NULL, 0, &error);

        if (app != NULL) {
                display = gdk_screen_get_display (screen);
                ctx = gdk_display_get_app_launch_context (display);
                gdk_app_launch_context_set_screen (ctx, screen);

                g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (ctx), &error);

                g_object_unref (app);
                g_object_unref (ctx);   
        }

        if (error != NULL) {
                g_message ("Could not start application on terminal: %s", error->message);

                g_error_free (error);
        }

        g_free (command_line);
}

void
_not_eel_gnome_open_terminal (const char *command)
{
        _not_eel_gnome_open_terminal_on_screen (command, NULL);
}

/* END eel-gnome-extensions */

static void terminal_nautilus_init       (TerminalNautilus      *instance);
static void terminal_nautilus_class_init (TerminalNautilusClass *klass);

typedef enum {
	/* local files. Always open "conventionally", i.e. cd and spawn. */
	FILE_INFO_LOCAL,
	FILE_INFO_DESKTOP,
	/* SFTP: Shell terminals are opened "remote" (i.e. with ssh client),
	 * commands are executed like *_OTHER */
	FILE_INFO_SFTP,
	/* OTHER: Terminals and commands are opened by mapping the URI back
	 * to ~/.gvfs, i.e. to the GVFS FUSE bridge
	 */
	FILE_INFO_OTHER
} TerminalFileInfo;

static TerminalFileInfo
get_terminal_file_info (const char *uri)
{
	TerminalFileInfo ret;
	char *uri_scheme;

	uri_scheme = g_uri_parse_scheme (uri);

	if (uri_scheme == NULL) {
		ret = FILE_INFO_OTHER;
	} else if (strcmp (uri_scheme, "file") == 0) {
		ret = FILE_INFO_LOCAL;
	} else if (strcmp (uri_scheme, "x-nautilus-desktop") == 0) {
		ret = FILE_INFO_DESKTOP;
	} else if (strcmp (uri_scheme, "sftp") == 0 ||
		   strcmp (uri_scheme, "ssh") == 0) {
		ret = FILE_INFO_SFTP;
	} else {
		ret = FILE_INFO_OTHER;
	}

	g_free (uri_scheme);

	return ret;
}

static GConfClient *gconf_client = NULL;

static inline gboolean
desktop_opens_home_dir (void)
{
	return gconf_client_get_bool (gconf_client,
				      "/apps/nautilus-open-terminal/desktop_opens_home_dir",
				      NULL);
}

static inline gboolean
display_mc_item (void)
{
	return gconf_client_get_bool (gconf_client,
				      "/apps/nautilus-open-terminal/display_mc_item",
				      NULL);
}

static inline gboolean
desktop_is_home_dir (void)
{
	return gconf_client_get_bool (gconf_client,
				      "/apps/nautilus/preferences/desktop_is_home_dir",
				      NULL);
}

/* a very simple URI parsing routine from Launchpad #333462, until GLib supports URI parsing (GNOME #489862) */
#define SFTP_PREFIX "sftp://"
static void
parse_sftp_uri (GFile *file,
		char **user,
		char **host,
		unsigned int *port,
		char **path)
{
	char *tmp, *save;
	char *uri;

	uri = g_file_get_uri (file);
	g_assert (uri != NULL);
	save = uri;

	*path = NULL;
	*user = NULL;
	*host = NULL;
	*port = 0;

	/* skip intial 'sftp:// prefix */
	g_assert (!strncmp (uri, SFTP_PREFIX, strlen (SFTP_PREFIX)));
	uri += strlen (SFTP_PREFIX);

	/* cut out the path */
	tmp = strchr (uri, '/');
	if (tmp != NULL) {
		*path = g_uri_unescape_string (tmp, "/");
		*tmp = '\0';
	}

	/* read the username - it ends with @ */
	tmp = strchr (uri, '@');
	if (tmp != NULL) {
		*tmp++ = '\0';

		*user = strdup (uri);
		if (strchr (*user, ':') != NULL) {
			/* chop the password */
			*(strchr (*user, ':')) = '\0'; 
		}

		uri = tmp;
	}

	/* now read the port, starts with : */
	tmp = strchr (uri, ':');
	if (tmp != NULL) {
		*tmp++ = '\0';
		*port = atoi (tmp);  /*FIXME: getservbyname*/
	}

	/* what is left is the host */
	*host = strdup (uri);
	g_free (save);
}

static char *
get_remote_ssh_command (const char *uri,
			const char *command_to_run)
{
	GFile *file;

	char *host_name, *path, *user_name;
	char *command, *user_host, *unescaped_path;
	char *quoted_path;
	char *remote_command;
	char *quoted_remote_command;
	char *port_str;
	guint host_port;

	g_assert (uri != NULL);

	file = g_file_new_for_uri (uri);
	parse_sftp_uri (file, &user_name, &host_name, &host_port, &path);
	g_object_unref (file);

	/* FIXME to we have to consider the remote file encoding? */
	unescaped_path = g_uri_unescape_string (path, NULL);
	quoted_path = g_shell_quote (unescaped_path);

	port_str = NULL;
	if (host_port != 0) {
		port_str = g_strdup_printf (" -p %d", host_port);
	} else {
		port_str = g_strdup ("");
	}

	if (user_name != NULL) {
		user_host = g_strdup_printf ("%s@%s", user_name, host_name);
	} else {
		user_host = g_strdup (host_name);
	}

	if (command_to_run != NULL) {
		remote_command = g_strdup_printf ("cd %s && exec %s", quoted_path, command_to_run);
	} else {
		/* login shell */
		remote_command = g_strdup_printf ("cd %s && exec $SHELL -", quoted_path);
	}

	quoted_remote_command = g_shell_quote (remote_command);

	command = g_strdup_printf ("ssh %s%s -t %s", user_host, port_str, quoted_remote_command);

	g_free (user_name);
	g_free (user_host);
	g_free (host_name);
	g_free (port_str);

	g_free (path);
	g_free (unescaped_path);
	g_free (quoted_path);

	g_free (remote_command);
	g_free (quoted_remote_command);

	return command;
}

static inline char *
get_gvfs_path_for_uri (const char *uri)
{
	GFile *file;
	char *path;

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);
	g_object_unref (file);

	return path;
}

static char *
get_terminal_command_for_file_info (NautilusFileInfo *file_info,
				    const char *command_to_run,
				    gboolean remote_terminal)
{
	char *uri, *path, *quoted_path;
	char *command;

	uri = nautilus_file_info_get_activation_uri (file_info);

	path = NULL;
	command = NULL;

	switch (get_terminal_file_info (uri)) {
		case FILE_INFO_LOCAL:
			if (uri != NULL) {
				path = g_filename_from_uri (uri, NULL, NULL);
			}
			break;

		case FILE_INFO_DESKTOP:
			if (desktop_is_home_dir () || desktop_opens_home_dir ()) {
				path = g_strdup (g_get_home_dir ());
			} else {
				path = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
			}
			break;

		case FILE_INFO_SFTP:
			if (remote_terminal && uri != NULL) {
				command = get_remote_ssh_command (uri, command_to_run);
				break;
			}

			/* fall through */
		case FILE_INFO_OTHER:
			if (uri != NULL) {
				/* map back remote URI to local path */
				path = get_gvfs_path_for_uri (uri);
			}
			break;

		default:
			g_assert_not_reached ();
	}

	if (command == NULL && path != NULL) {
		quoted_path = g_shell_quote (path);

		if (command_to_run != NULL) {
			command = g_strdup_printf ("cd %s && exec %s", quoted_path, command_to_run);
		} else {
			/* interactive shell */
			command = g_strdup_printf ("cd %s && exec $SHELL", quoted_path);
		}

		g_free (quoted_path);
	}

	g_free (path);
	g_free (uri);

	return command;
}


static void
open_terminal (NautilusMenuItem *item,
	       NautilusFileInfo *file_info)
{
	char *terminal_command, *command_to_run;
	GdkScreen *screen;
	gboolean remote_terminal;

	screen = g_object_get_data (G_OBJECT (item), "TerminalNautilus::screen");
	command_to_run = g_object_get_data (G_OBJECT (item), "TerminalNautilus::command-to-run");
	remote_terminal = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "TerminalNautilus::remote-terminal"));

	terminal_command = get_terminal_command_for_file_info (file_info, command_to_run, remote_terminal);
	if (terminal_command != NULL) {
		_not_eel_gnome_open_terminal_on_screen (terminal_command, screen);
	}
	g_free (terminal_command);
}

static void
open_terminal_callback (NautilusMenuItem *item,
			NautilusFileInfo *file_info)
{
	open_terminal (item, file_info);
}

static NautilusMenuItem *
open_terminal_menu_item_new (NautilusFileInfo *file_info,
			     TerminalFileInfo  terminal_file_info,
			     GdkScreen        *screen,
			     const char       *command_to_run,
			     gboolean          remote_terminal,
			     gboolean          is_file_item)
{
	NautilusMenuItem *ret;
	char *action_name;
	const char *name;
	const char *tooltip;

	if (command_to_run == NULL) {
		switch (terminal_file_info) {
			case FILE_INFO_SFTP:
				if (remote_terminal) {
					name = _("Open in _Remote Terminal");
				} else {
					name = _("Open in _Local Terminal");
				}

				if (is_file_item) {
					tooltip = _("Open the currently selected folder in a terminal");
				} else {
					tooltip = _("Open the currently open folder in a terminal");
				}
				break;

			case FILE_INFO_LOCAL:
			case FILE_INFO_OTHER:
				name = _("Open in T_erminal");

				if (is_file_item) {
					tooltip = _("Open the currently selected folder in a terminal");
				} else {
					tooltip = _("Open the currently open folder in a terminal");
				}
				break;

			case FILE_INFO_DESKTOP:
				if (desktop_opens_home_dir ()) {
					name = _("Open T_erminal");
					tooltip = _("Open a terminal");
				} else {
					name = _("Open in T_erminal");
					tooltip = _("Open the currently open folder in a terminal");
				}
				break;

			default:
				g_assert_not_reached ();
		}
	} else if (!strcmp (command_to_run, "mc")) {
		switch (terminal_file_info) {
			case FILE_INFO_LOCAL:
			case FILE_INFO_SFTP:
			case FILE_INFO_OTHER:
				name = _("Open in _Midnight Commander");
				if (is_file_item) {
					tooltip = _("Open the currently selected folder in the terminal file manager Midnight Commander");
				} else {
					tooltip = _("Open the currently open folder in the terminal file manager Midnight Commander");
				}
				break;

			case FILE_INFO_DESKTOP:
				if (desktop_opens_home_dir ()) {
					name = _("Open _Midnight Commander");
					tooltip = _("Open the terminal file manager Midnight Commander");
				} else {
					name = _("Open in _Midnight Commander");
					tooltip = _("Open the currently open folder in the terminal file manager Midnight Commander");
				}
				break;

			default:
				g_assert_not_reached ();
		}
	} else {
		g_assert_not_reached ();
	}

	if (command_to_run != NULL) {
		action_name = g_strdup_printf (remote_terminal ?
					       "TerminalNautilus::open_remote_terminal_%s" :
					       "TerminalNautilus::open_terminal_%s",
					       command_to_run);
	} else {
		action_name = g_strdup (remote_terminal ? 
					"TerminalNautilus::open_remote_terminal" :
					"TerminalNautilus::open_terminal");
	}
	ret = nautilus_menu_item_new (action_name, name, tooltip, "gnome-terminal");
	g_free (action_name);

	g_object_set_data (G_OBJECT (ret),
			   "TerminalNautilus::screen",
			   screen);
	g_object_set_data_full (G_OBJECT (ret), "TerminalNautilus::command-to-run",
				g_strdup (command_to_run),
				(GDestroyNotify) g_free);
	g_object_set_data (G_OBJECT (ret), "TerminalNautilus::remote-terminal",
			   GUINT_TO_POINTER (remote_terminal));


	g_object_set_data_full (G_OBJECT (ret), "file-info",
				g_object_ref (file_info),
				(GDestroyNotify) g_object_unref);


	g_signal_connect (ret, "activate",
			  G_CALLBACK (open_terminal_callback),
			  file_info);


	return ret;
}

static gboolean
terminal_locked_down (void)
{
	return gconf_client_get_bool (gconf_client,
                                      "/desktop/gnome/lockdown/disable_command_line",
                                      NULL);
}

/* used to determine for remote URIs whether GVFS is capable of mapping them to ~/.gvfs */
static gboolean
uri_has_local_path (const char *uri)
{
	GFile *file;
	char *path;
	gboolean ret;

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);

	ret = (path != NULL);

	g_free (path);
	g_object_unref (file);

	return ret;
}

static GList *
terminal_nautilus_get_background_items (NautilusMenuProvider *provider,
					     GtkWidget		  *window,
					     NautilusFileInfo	  *file_info)
{
	gchar *uri;
	GList *items;
	NautilusMenuItem *item;
	TerminalFileInfo  terminal_file_info;

        if (terminal_locked_down ()) {
            return NULL;
        }

	items = NULL;

	uri = nautilus_file_info_get_activation_uri (file_info);
	terminal_file_info = get_terminal_file_info (uri);

	if (terminal_file_info == FILE_INFO_SFTP ||
	    terminal_file_info == FILE_INFO_DESKTOP ||
	    uri_has_local_path (uri)) {
		/* local locations or SSH */
		item = open_terminal_menu_item_new (file_info, terminal_file_info, gtk_widget_get_screen (window),
						    NULL, terminal_file_info == FILE_INFO_SFTP, FALSE);
		items = g_list_append (items, item);
	}

	if ((terminal_file_info == FILE_INFO_SFTP ||
	     terminal_file_info == FILE_INFO_OTHER) &&
	    uri_has_local_path (uri)) {
		/* remote locations that offer local back-mapping */
		item = open_terminal_menu_item_new (file_info, terminal_file_info, gtk_widget_get_screen (window),
						    NULL, FALSE, FALSE);
		items = g_list_append (items, item);
	}

	if (display_mc_item () &&
	    g_find_program_in_path ("mc") &&
	    ((terminal_file_info == FILE_INFO_DESKTOP &&
	      (desktop_is_home_dir () || desktop_opens_home_dir ())) ||
	     uri_has_local_path (uri))) {
		item = open_terminal_menu_item_new (file_info, terminal_file_info, gtk_widget_get_screen (window), "mc", FALSE, FALSE);
		items = g_list_append (items, item);
	}

	g_free (uri);

	return items;
}

static GList *
terminal_nautilus_get_file_items (NautilusMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	gchar *uri;
	GList *items;
	NautilusMenuItem *item;
	TerminalFileInfo  terminal_file_info;

        if (terminal_locked_down ()) {
            return NULL;
        }

	if (g_list_length (files) != 1 ||
	    (!nautilus_file_info_is_directory (files->data) &&
	     nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_SHORTCUT &&
	     nautilus_file_info_get_file_type (files->data) != G_FILE_TYPE_MOUNTABLE)) {
		return NULL;
	}

	items = NULL;

	uri = nautilus_file_info_get_activation_uri (files->data);
	terminal_file_info = get_terminal_file_info (uri);

	switch (terminal_file_info) {
		case FILE_INFO_LOCAL:
		case FILE_INFO_SFTP:
		case FILE_INFO_OTHER:
			if (terminal_file_info == FILE_INFO_SFTP || uri_has_local_path (uri)) {
				item = open_terminal_menu_item_new (files->data, terminal_file_info, gtk_widget_get_screen (window),
								    NULL, terminal_file_info == FILE_INFO_SFTP, TRUE);
				items = g_list_append (items, item);
			}

			if (terminal_file_info == FILE_INFO_SFTP &&
			    uri_has_local_path (uri)) {
				item = open_terminal_menu_item_new (files->data, terminal_file_info, gtk_widget_get_screen (window), NULL, FALSE, TRUE);
				items = g_list_append (items, item);
			}

			if (display_mc_item () &&
			    g_find_program_in_path ("mc") &&
			     uri_has_local_path (uri)) {
				item = open_terminal_menu_item_new (files->data, terminal_file_info, gtk_widget_get_screen (window), "mc", TRUE, FALSE);
				items = g_list_append (items, item);
			}
			break;

		case FILE_INFO_DESKTOP:
			break;

		default:
			g_assert_not_reached ();
	}

	g_free (uri);

	return items;
}

static void
terminal_nautilus_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
	iface->get_background_items = terminal_nautilus_get_background_items;
	iface->get_file_items = terminal_nautilus_get_file_items;
}

static void 
terminal_nautilus_init (TerminalNautilus *instance)
{
}

static void
terminal_nautilus_class_init (TerminalNautilusClass *class)
{
        bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        gconf_client_add_dir(gconf_client_get_default(), 
                             "/desktop/gnome/lockdown",
                             0,
                             NULL);

	g_assert (gconf_client == NULL);
	gconf_client = gconf_client_get_default ();
}

static void
terminal_nautilus_class_finalize (TerminalNautilusClass *class)
{
	g_assert (gconf_client != NULL);
	g_object_unref (gconf_client);
	gconf_client = NULL;
}

G_DEFINE_DYNAMIC_TYPE_EXTENDED (TerminalNautilus, terminal_nautilus, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_MENU_PROVIDER,
                                                               terminal_nautilus_menu_provider_iface_init))

/* Nautilus extension */

void nautilus_module_initialize (GTypeModule *module);
void nautilus_module_shutdown   (void);
void nautilus_module_list_types (const GType **types, 
                                 int *num_types);

static GType type_list[1];

void
nautilus_module_initialize (GTypeModule *module)
{
        terminal_nautilus_register_type (module);
        type_list[0] = TERMINAL_TYPE_NAUTILUS;
}

void
nautilus_module_shutdown (void)
{
}

void 
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
        *types = type_list;
        *num_types = G_N_ELEMENTS (type_list);
}
