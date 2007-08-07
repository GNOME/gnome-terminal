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

#include "terminal-intl.h"
#include "terminal-widget.h"

#include <string.h>
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

GtkWidget *
terminal_widget_new (void)
{
  GtkWidget *terminal;
  VteData *data;
  
  terminal = vte_terminal_new ();

  vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), TRUE);
  
  data = g_new0 (VteData, 1);

  data->url_tags = NULL;
  data->skey_tags = NULL;

  g_object_set_data_full (G_OBJECT (terminal), "terminal-widget-data",
                          data, free_vte_data);
  
  return terminal;
}

void
terminal_widget_set_size (GtkWidget *widget,
			  int        width_chars,
			  int        height_chars)
{
  VteTerminal *terminal;

  terminal = VTE_TERMINAL (widget);

  vte_terminal_set_size (terminal, width_chars, height_chars);
}

void
terminal_widget_get_size (GtkWidget *widget,
			  int       *width_chars,
			  int       *height_chars)
{
  VteTerminal *terminal;

  terminal = VTE_TERMINAL (widget);

  *width_chars = terminal->column_count;
  *height_chars = terminal->row_count;
}

void
terminal_widget_get_cell_size (GtkWidget            *widget,
			       int                  *cell_width_pixels,
			       int                  *cell_height_pixels)
{
  VteTerminal *terminal;

  terminal = VTE_TERMINAL (widget);

  *cell_width_pixels = terminal->char_width;
  *cell_height_pixels = terminal->char_height;
}

void
terminal_widget_get_padding                (GtkWidget            *widget,
					    int                  *xpad,
					    int                  *ypad)
{
  vte_terminal_get_padding(VTE_TERMINAL(widget), xpad, ypad);
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
    if (GPOINTER_TO_INT(((TagData*)tags->data)->tag) == tag)
      {
        if (flavor)
          *flavor = tag;
        return match;
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
    if (GPOINTER_TO_INT(((TagData*)tags->data)->tag) == tag)
      {
        if (flavor)
          *flavor = tag;
        return match;
      }

  g_free (match);
  return NULL;
}

void
terminal_widget_set_word_characters (GtkWidget  *widget,
                                     const char *str)
{
  vte_terminal_set_word_chars(VTE_TERMINAL(widget), str);
}

void
terminal_widget_set_delete_binding (GtkWidget           *widget,
				    TerminalEraseBinding binding)
{
  switch (binding) {
    case TERMINAL_ERASE_ASCII_DEL:
      vte_terminal_set_delete_binding(VTE_TERMINAL(widget),
		      		      VTE_ERASE_ASCII_DELETE);
      break;
    case TERMINAL_ERASE_ESCAPE_SEQUENCE:
      vte_terminal_set_delete_binding(VTE_TERMINAL(widget),
		      		      VTE_ERASE_DELETE_SEQUENCE);
      break;
    case TERMINAL_ERASE_CONTROL_H:
      vte_terminal_set_delete_binding(VTE_TERMINAL(widget),
		      		      VTE_ERASE_ASCII_BACKSPACE);
      break;
    default:
      vte_terminal_set_delete_binding(VTE_TERMINAL(widget),
		      		      VTE_ERASE_AUTO);
      break;
  }
}

void
terminal_widget_set_backspace_binding (GtkWidget            *widget,
				       TerminalEraseBinding  binding)
{
  switch (binding) {
    case TERMINAL_ERASE_ASCII_DEL:
      vte_terminal_set_backspace_binding(VTE_TERMINAL(widget),
		      			 VTE_ERASE_ASCII_DELETE);
      break;
    case TERMINAL_ERASE_ESCAPE_SEQUENCE:
      vte_terminal_set_backspace_binding(VTE_TERMINAL(widget),
		      			 VTE_ERASE_DELETE_SEQUENCE);
      break;
    case TERMINAL_ERASE_CONTROL_H:
      vte_terminal_set_backspace_binding(VTE_TERMINAL(widget),
		      			 VTE_ERASE_ASCII_BACKSPACE);
      break;
    default:
      vte_terminal_set_backspace_binding(VTE_TERMINAL(widget),
		      			 VTE_ERASE_AUTO);
      break;
  }
}

void
terminal_widget_set_cursor_blinks (GtkWidget *widget,
				   gboolean   setting)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_cursor_blinks(VTE_TERMINAL(widget), setting);
}

void
terminal_widget_set_audible_bell (GtkWidget *widget,
				  gboolean   setting)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_audible_bell(VTE_TERMINAL(widget), setting);
}

void
terminal_widget_set_scroll_on_keystroke (GtkWidget *widget,
					 gboolean   setting)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(widget), setting);
}

void
terminal_widget_set_scroll_on_output (GtkWidget *widget,
				      gboolean   setting)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_scroll_on_output(VTE_TERMINAL(widget), setting);
}

void
terminal_widget_set_scrollback_lines (GtkWidget *widget,
				      int        lines)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_scrollback_lines(VTE_TERMINAL(widget), lines);
}

void
terminal_widget_set_background_image (GtkWidget *widget,
				      GdkPixbuf *pixbuf)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_background_image(VTE_TERMINAL(widget), pixbuf);
}

void
terminal_widget_set_background_image_file (GtkWidget  *widget,
					   const char *fname)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));

  if ((fname != NULL) && (strlen(fname) > 0))
    vte_terminal_set_background_image_file(VTE_TERMINAL(widget), fname);
  else
    vte_terminal_set_background_image(VTE_TERMINAL(widget), NULL);
}

void
terminal_widget_set_background_transparent (GtkWidget *widget,
					    gboolean   setting)
{
    /* FIXME: Don't enable this if we have a compmgr. */
  vte_terminal_set_background_transparent(VTE_TERMINAL(widget), setting);
}

/* 0.0 = normal bg, 1.0 = all black bg, 0.5 = half darkened */
void
terminal_widget_set_background_darkness (GtkWidget *widget,
					 double     factor)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_background_saturation(VTE_TERMINAL(widget), 1.0 - factor);
}

void
terminal_widget_set_background_opacity (GtkWidget *widget,
					double     factor)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_opacity(VTE_TERMINAL(widget), factor * 0xffff);
}

void
terminal_widget_set_background_scrolls (GtkWidget *widget,
					gboolean   setting)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_scroll_background(VTE_TERMINAL(widget), setting);
}

void
terminal_widget_set_normal_gdk_font (GtkWidget *widget,
				     GdkFont   *font)
{
  UNIMPLEMENTED;
}

void
terminal_widget_set_bold_gdk_font (GtkWidget *widget,
				   GdkFont   *font)
{
  UNIMPLEMENTED;
}

void
terminal_widget_set_allow_bold (GtkWidget *widget,
				gboolean   setting)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_set_allow_bold(VTE_TERMINAL(widget), setting);
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
terminal_widget_copy_clipboard (GtkWidget *widget)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_copy_clipboard(VTE_TERMINAL(widget));
}

void
terminal_widget_paste_clipboard (GtkWidget *widget)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_paste_clipboard(VTE_TERMINAL(widget));
}

void
terminal_widget_reset (GtkWidget *widget,
		       gboolean   also_clear_afterward)
{
  g_return_if_fail(VTE_IS_TERMINAL(widget));
  vte_terminal_reset (VTE_TERMINAL(widget), TRUE, also_clear_afterward);
}


void
terminal_widget_connect_title_changed (GtkWidget *widget,
				       GCallback  callback,
				       void      *data)
{
  g_signal_connect (widget, "window_title_changed",
		    G_CALLBACK (callback), data);
}

void
terminal_widget_disconnect_title_changed (GtkWidget *widget,
					  GCallback  callback,
					  void      *data)
{
  g_signal_handlers_disconnect_by_func (widget, callback, data);
}

void
terminal_widget_connect_icon_title_changed (GtkWidget *widget,
					    GCallback  callback,
					    void      *data)
{
  g_signal_connect (widget, "icon_title_changed",
		    G_CALLBACK (callback), data);
}

void
terminal_widget_disconnect_icon_title_changed (GtkWidget *widget,
					       GCallback  callback,
					       void      *data)
{
  g_signal_handlers_disconnect_by_func (widget, callback, data);
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

void
terminal_widget_connect_selection_changed (GtkWidget *widget,
					   GCallback  callback,
					   void      *data)
{
  g_signal_connect (widget, "selection-changed",
		    G_CALLBACK (callback), data);
}

void
terminal_widget_disconnect_selection_changed (GtkWidget *widget,
					      GCallback  callback,
					      void      *data)
{
  g_signal_handlers_disconnect_by_func (widget, callback, data);
}

void
terminal_widget_connect_encoding_changed      (GtkWidget *widget,
                                               GCallback  callback,
                                               void      *data)
{
  g_signal_connect (widget, "encoding-changed",
		    G_CALLBACK (callback), data);
}

void
terminal_widget_disconnect_encoding_changed   (GtkWidget *widget,
                                               GCallback  callback,
                                               void      *data)
{
  g_signal_handlers_disconnect_by_func (widget, callback, data);
}

const char*
terminal_widget_get_title (GtkWidget *widget)
{
  VteTerminal *terminal;

  terminal = VTE_TERMINAL (widget);

  return terminal->window_title;
}

const char*
terminal_widget_get_icon_title (GtkWidget *widget)
{
  VteTerminal *terminal;
  
  terminal = VTE_TERMINAL (widget);

  return terminal->icon_title;
}

gboolean
terminal_widget_get_has_selection (GtkWidget *widget)
{
  g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
  return vte_terminal_get_has_selection(VTE_TERMINAL(widget));
}


GtkAdjustment*
terminal_widget_get_scroll_adjustment (GtkWidget *widget)
{
  VteTerminal *terminal;

  terminal = VTE_TERMINAL (widget);

  return terminal->adjustment;
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



int
terminal_widget_get_estimated_bytes_per_scrollback_line (void)
{
  /* One slot in the ring buffer, plus the array which holds the data for
   * the line, plus about 80 vte_charcell structures. */
  return sizeof(gpointer) + sizeof(GArray) + (80 * (sizeof(gunichar) + 4));
}

void
terminal_widget_write_data_to_child (GtkWidget  *widget,
                                     const char *data,
                                     int         len)
{
  vte_terminal_feed_child(VTE_TERMINAL(widget), data, len);
}

void
terminal_widget_set_pango_font (GtkWidget                  *widget,
				const PangoFontDescription *font_desc,
				gboolean no_aa_without_render)
{
  g_return_if_fail (font_desc != NULL);

  if (!no_aa_without_render)
    vte_terminal_set_font (VTE_TERMINAL (widget), font_desc);

  else
    {
      Display *dpy;
      gboolean has_render;
      gint event_base, error_base;

      dpy = gdk_x11_display_get_xdisplay (gdk_display_get_default ());
      has_render = (XRenderQueryExtension (dpy, &event_base, &error_base) &&
		    (XRenderFindVisualFormat (dpy, DefaultVisual (dpy, DefaultScreen (dpy))) != 0));

      if (has_render)
	vte_terminal_set_font (VTE_TERMINAL (widget), font_desc);
      else 
	vte_terminal_set_font_full (VTE_TERMINAL (widget),
				    font_desc,
				    VTE_ANTI_ALIAS_FORCE_DISABLE);
    }
}

gboolean
terminal_widget_supports_pango_fonts (void)
{
  return TRUE;
}

const char*
terminal_widget_get_encoding (GtkWidget *widget)
{
  return vte_terminal_get_encoding (VTE_TERMINAL (widget));
}

void
terminal_widget_set_encoding (GtkWidget  *widget,
                              const char *encoding)
{
  const char *old;

  /* Short-circuit setting the same encoding twice. */
  old = vte_terminal_get_encoding (VTE_TERMINAL (widget));
  if ((old && encoding &&
       strcmp (old, encoding) == 0) ||
      (old == NULL && encoding == NULL))
    return;
  
  vte_terminal_set_encoding (VTE_TERMINAL (widget),
                             encoding);
}

gboolean
terminal_widget_supports_dynamic_encoding (void)
{
  return TRUE;
}

void
terminal_widget_im_append_menuitems(GtkWidget *widget, GtkMenuShell *menushell)
{
  vte_terminal_im_append_menuitems(VTE_TERMINAL(widget), menushell);
}
