/* VTE implementation of terminal-widget.h */

/*
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "terminal-widget.h"

#include <string.h>
#include <vte/vte.h>

#define UNIMPLEMENTED /* g_warning (G_STRLOC": unimplemented") */

typedef struct
{
  int foo;
} VteData;

static void
free_vte_data (gpointer data)
{
  VteData *vte;

  vte = data;
  
  g_free (vte);
}

GtkWidget *
terminal_widget_new (void)
{
  GtkWidget *terminal;
  VteData *data;
  
  terminal = vte_terminal_new ();
  
  data = g_new0 (VteData, 1);
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
  UNIMPLEMENTED;

  *xpad = 0;
  *ypad = 0;
}

void
terminal_widget_match_add                  (GtkWidget            *widget,
					    const char           *regexp)
{
  UNIMPLEMENTED;
}

char*
terminal_widget_check_match (GtkWidget *widget,
			     int        column,
			     int        row)
{
  return NULL;
}

void
terminal_widget_set_word_characters (GtkWidget  *widget,
                                     const char *str)
{
  UNIMPLEMENTED;
}

void
terminal_widget_set_delete_binding (GtkWidget           *widget,
				    TerminalEraseBinding binding)
{
  UNIMPLEMENTED;
}

void
terminal_widget_set_backspace_binding (GtkWidget            *widget,
				       TerminalEraseBinding  binding)
{
  UNIMPLEMENTED;
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
terminal_widget_set_background_scrolls (GtkWidget *widget,
					gboolean   setting)
{
  UNIMPLEMENTED;
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
  UNIMPLEMENTED;
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
  UNIMPLEMENTED;
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
  g_signal_connect (widget, "eof",
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
  UNIMPLEMENTED;
}

void
terminal_widget_disconnect_selection_changed (GtkWidget *widget,
					      GCallback  callback,
					      void      *data)
{
  UNIMPLEMENTED;
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
			      gboolean     update_records,
			      const char  *path,
			      char       **argv,
			      char       **envp,
                              const char  *working_dir,
                              int         *child_pid,
			      GError     **err)
{
  /* FIXME: Not sure if this is correct */
  vte_terminal_fork_command (VTE_TERMINAL (widget), path, argv);

  *child_pid = 0; /* FIXME */
  
  return TRUE;
}



int
terminal_widget_get_estimated_bytes_per_scrollback_line (void)
{
  UNIMPLEMENTED;

  return sizeof (void*) * 6 + (80.0 * 4);
}

void
terminal_widget_write_data_to_child (GtkWidget  *widget,
                                     const char *data,
                                     int         len)
{
  UNIMPLEMENTED;
}

void
terminal_widget_set_pango_font (GtkWidget                  *widget,
                                const PangoFontDescription *font_desc)
{
  VteTerminal *terminal;

  g_return_if_fail (font_desc != NULL);
  
  terminal = VTE_TERMINAL (widget);

  vte_terminal_set_font (terminal, font_desc);
}

gboolean
terminal_widget_supports_pango_fonts (void)
{
  return TRUE;
}

