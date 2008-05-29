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

typedef struct
{
  int tag;
  int flavor;
} TagData;

typedef struct
{
  GSList *url_tags;
  GSList *skey_tags;
} VteData;

static void
free_vte_data (gpointer data)
{
  VteData *vte;

  vte = data;

  g_slist_foreach (vte->url_tags, (GFunc) g_free, NULL);
  g_slist_free(vte->url_tags);

  g_slist_foreach (vte->skey_tags, (GFunc) g_free, NULL);
  g_slist_free(vte->skey_tags);
  
  g_free (vte);
}

/* FIXMEchpe: to be removed later */
void
terminal_widget_set_implementation (GtkWidget *terminal)
{
  VteData *data;
  
  vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), TRUE);
  
  data = g_new0 (VteData, 1);

  data->url_tags = NULL;
  data->skey_tags = NULL;

  g_object_set_data_full (G_OBJECT (terminal), "terminal-widget-data",
                          data, free_vte_data);
}

void
terminal_widget_match_add                  (GtkWidget            *widget,
					    const char           *regexp,
                                            int                   flavor)
{
  TagData *tag_data;
  VteData *data;
  int tag;
  
  data = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");

  tag = vte_terminal_match_add (VTE_TERMINAL (widget), regexp);

  tag_data = g_new0 (TagData, 1);
  tag_data->tag = tag;
  tag_data->flavor = flavor;

  data->url_tags = g_slist_append (data->url_tags, tag_data);
}

void
terminal_widget_skey_match_add             (GtkWidget            *widget,
					    const char           *regexp,
                                            int                   flavor)
{
  TagData *tag_data;
  VteData *data;
  int tag;
  
  data = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");

  tag = vte_terminal_match_add(VTE_TERMINAL(widget), regexp);

  tag_data = g_new0 (TagData, 1);
  tag_data->tag = tag;
  tag_data->flavor = flavor;

  data->skey_tags = g_slist_append (data->skey_tags, tag_data);
}

void
terminal_widget_skey_match_remove          (GtkWidget            *widget)
{
  VteData *data;
  GSList *tags;
  
  data = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");

  for (tags = data->skey_tags; tags != NULL; tags = g_slist_next(tags))
    vte_terminal_match_remove(VTE_TERMINAL(widget),
   			      GPOINTER_TO_INT(((TagData*)tags->data)->tag));

  g_slist_foreach (data->skey_tags, (GFunc) g_free, NULL);
  g_slist_free(data->skey_tags);
  data->skey_tags = NULL;
}

char*
terminal_widget_check_match (GtkWidget *widget,
			     int        column,
			     int        row,
                             int       *flavor)
{
  VteData *data;
  GSList *tags;
  gint tag;
  char *match;
   
  data = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");

  match = vte_terminal_match_check(VTE_TERMINAL(widget), column, row, &tag);
  for (tags = data->url_tags; tags != NULL; tags = g_slist_next(tags))
    {
      TagData *tag_data = (TagData*) tags->data;
      if (GPOINTER_TO_INT(tag_data->tag) == tag)
	{
	  if (flavor)
	    *flavor = tag_data->flavor;
	  return match;
	}
    }

  g_free (match);
  return NULL;
}

char*
terminal_widget_skey_check_match (GtkWidget *widget,
				  int        column,
				  int        row,
                                  int       *flavor)
{
  VteData *data;
  GSList *tags;
  gint tag;
  char *match;
   
  data = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");

  match = vte_terminal_match_check(VTE_TERMINAL(widget), column, row, &tag);
  for (tags = data->skey_tags; tags != NULL; tags = g_slist_next(tags))
    {
      TagData *tag_data = (TagData*) tags->data;
      if (GPOINTER_TO_INT(tag_data->tag) == tag)
	{
	  if (flavor)
	    *flavor = tag_data->flavor;
	  return match;
	}
    }

  g_free (match);
  return NULL;
}

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
