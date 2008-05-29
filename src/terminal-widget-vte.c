/* VTE implementation of terminal-widget.h */

/*
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This file is part of gnome-terminal.
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
#include "terminal-intl.h"

#include "terminal-widget.h"

#include <string.h>
#include <X11/extensions/Xrender.h>

#include <vte/vte.h>
#include <gdk/gdkx.h>

#define UNIMPLEMENTED /* g_warning (G_STRLOC": unimplemented") */

void
terminal_widget_set_colors (GtkWidget      *widget,
			    const GdkColor *foreground,
			    const GdkColor *background,
			    const GdkColor *palette_entries)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_colors(VTE_TERMINAL(widget), foreground, background,
			  palette_entries, TERMINAL_PALETTE_SIZE);
  vte_terminal_set_background_tint_color(VTE_TERMINAL(widget), background);
}

void
terminal_widget_connect_child_died (GtkWidget *widget,
				    GCallback  callback,
				    void      *data)
{
  g_signal_connect (widget, "child-exited",
		    G_CALLBACK (callback), data);
}

void
terminal_widget_disconnect_child_died (GtkWidget *widget,
				       GCallback  callback,
				       void      *data)
{
  g_signal_handlers_disconnect_by_func (widget, callback, data);
}

gboolean
terminal_widget_fork_command (GtkWidget   *widget,
                              gboolean     lastlog,
			      gboolean     update_records,
			      const char  *path,
			      char       **argv,
			      char       **envp,
                              const char  *working_dir,
                              int         *child_pid,
			      GError     **err)
{
  *child_pid = vte_terminal_fork_command (VTE_TERMINAL (widget),
		 			  path, argv, envp, working_dir,
					  lastlog, update_records, update_records);

  if (*child_pid == -1)
    {
      g_set_error (err,
                   G_SPAWN_ERROR,
                   G_SPAWN_ERROR_FAILED,
                   _("There was an error creating the child process for this terminal")
                   );
      return FALSE;
    }

  return TRUE;
}

void
terminal_widget_write_data_to_child (GtkWidget  *widget,
                                     const char *data,
                                     int         len)
{
  vte_terminal_feed_child(VTE_TERMINAL(widget), data, len);
}
