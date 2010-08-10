/*
 * Copyright © 2010 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope info_bar it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "terminal-info-bar.h"

#include <gtk/gtk.h>

#define TERMINAL_INFO_BAR_GET_PRIVATE(info_bar)(G_TYPE_INSTANCE_GET_PRIVATE ((info_bar), TERMINAL_TYPE_INFO_BAR, TerminalInfoBarPrivate))

struct _TerminalInfoBarPrivate
{
  GtkWidget *content_box;
};

G_DEFINE_TYPE (TerminalInfoBar, terminal_info_bar, GTK_TYPE_INFO_BAR)

/* helper functions */

static void
terminal_info_bar_init (TerminalInfoBar *bar)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (bar);
  TerminalInfoBarPrivate *priv;

  priv = bar->priv = TERMINAL_INFO_BAR_GET_PRIVATE (bar);

  priv->content_box = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (info_bar)),
                      priv->content_box, TRUE, TRUE, 0);
}

static void
terminal_info_bar_class_init (TerminalInfoBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (gobject_class, sizeof (TerminalInfoBarPrivate));
}

/* public API */

/**
 * terminal_info_bar_new:
 * @type: a #GtkMessageType
 *
 * Returns: a new #TerminalInfoBar for @screen
 */
GtkWidget *
terminal_info_bar_new (GtkMessageType type,
                       const char *first_button_text,
                       ...)
{
  GtkWidget *info_bar;
  va_list args;

  info_bar = g_object_new (TERMINAL_TYPE_INFO_BAR,
                           "message-type", type,
                           NULL);

  va_start (args, first_button_text);
  while (first_button_text != NULL) {
    int response_id;

    response_id = va_arg (args, int);
    gtk_info_bar_add_button (GTK_INFO_BAR (info_bar),
                             first_button_text, response_id);

    first_button_text = va_arg (args, const char *);
  }
  va_end (args);

  return info_bar;
}

void
terminal_info_bar_format_text (TerminalInfoBar *bar,
                               const char *format,
                               ...)
{
  TerminalInfoBarPrivate *priv;
  char *text;
  GtkWidget *label;
  va_list args;

  g_return_if_fail (TERMINAL_IS_INFO_BAR (bar));

  priv = bar->priv;

  va_start (args, format);
  text = g_strdup_vprintf (format, args);
  va_end (args);

  label = gtk_label_new (text);
  g_free (text);
  
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

  gtk_box_pack_start (GTK_BOX (priv->content_box), label, FALSE, FALSE, 0);
  gtk_widget_show_all (priv->content_box);
}
