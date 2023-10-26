/*
 * Copyright © 2010 Christian Persch
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

#include "terminal-info-bar.hh"
#include "terminal-libgsystem.hh"

#include <gtk/gtk.h>

struct _TerminalInfoBar
{
  GtkWidget  parent_instance;
  GtkWidget *info_bar;
  GtkWidget *content_box;
};

G_DEFINE_FINAL_TYPE (TerminalInfoBar, terminal_info_bar, GTK_TYPE_WIDGET)

enum {
  RESPONSE,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

/* helper functions */

static void
terminal_info_bar_response_cb (TerminalInfoBar *info_bar,
                               int response_id,
                               GtkInfoBar *gtk_info_bar)
{
  g_assert (TERMINAL_IS_INFO_BAR (info_bar));
  g_assert (GTK_IS_INFO_BAR (gtk_info_bar));

  g_signal_emit (info_bar, signals[RESPONSE], 0, response_id);
}

static void
terminal_info_bar_dispose (GObject *object)
{
  TerminalInfoBar *bar = TERMINAL_INFO_BAR (object);

  g_clear_pointer (&bar->info_bar, gtk_widget_unparent);

  G_OBJECT_CLASS (terminal_info_bar_parent_class)->dispose (object);
}

static void
terminal_info_bar_init (TerminalInfoBar *bar)
{
  bar->info_bar = gtk_info_bar_new ();
  gtk_widget_set_parent (GTK_WIDGET (bar->info_bar), GTK_WIDGET (bar));

  bar->content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_info_bar_add_child (GTK_INFO_BAR (bar->info_bar), bar->content_box);
}

static void
terminal_info_bar_class_init (TerminalInfoBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = terminal_info_bar_dispose;

  signals[RESPONSE] = g_signal_new ("response",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    nullptr, nullptr,
                                    nullptr,
                                    G_TYPE_NONE, 1, G_TYPE_INT);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
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
  TerminalInfoBar *info_bar;
  va_list args;

  info_bar = reinterpret_cast<TerminalInfoBar*>
    (g_object_new (TERMINAL_TYPE_INFO_BAR,
		   nullptr));

  g_object_set (info_bar->info_bar,
                "message-type", type,
                "show-close-button", true,
                nullptr);

  g_signal_connect_object (info_bar->info_bar,
                           "response",
                           G_CALLBACK (terminal_info_bar_response_cb),
                           info_bar,
                           G_CONNECT_SWAPPED);

  va_start (args, first_button_text);
  while (first_button_text != nullptr) {
    int response_id;

    response_id = va_arg (args, int);
    gtk_info_bar_add_button (GTK_INFO_BAR (info_bar->info_bar),
                             first_button_text, response_id);

    first_button_text = va_arg (args, const char *);
  }
  va_end (args);

  return GTK_WIDGET (info_bar);
}

void
terminal_info_bar_format_text (TerminalInfoBar *bar,
                               const char *format,
                               ...)
{
  gs_free char *text = nullptr;
  GtkWidget *label;
  va_list args;

  g_return_if_fail (TERMINAL_IS_INFO_BAR (bar));

  va_start (args, format);
  text = g_strdup_vprintf (format, args);
  va_end (args);

  label = gtk_label_new (text);

  gtk_label_set_natural_wrap_mode (GTK_LABEL (label), GTK_NATURAL_WRAP_INHERIT);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0);

  gtk_box_prepend (GTK_BOX (bar->content_box), label);
}

void
terminal_info_bar_set_default_response (TerminalInfoBar *bar,
                                        int response_id)
{
  g_return_if_fail (TERMINAL_IS_INFO_BAR (bar));

  gtk_info_bar_set_default_response (GTK_INFO_BAR (bar->info_bar), response_id);
}
