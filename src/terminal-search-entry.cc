/*
 * Copyright © 2023 Christian Hergert
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

#include "config.h"

#include "terminal-search-entry.hh"

#define SEARCH_CHANGED_TIMEOUT_MSEC 150

struct _TerminalSearchEntry
{
  GtkEntry parent_instance;

  guint search_changed_source;
};

G_DEFINE_FINAL_TYPE (TerminalSearchEntry, terminal_search_entry, GTK_TYPE_ENTRY)

enum {
  SEARCH_CHANGED,
  PREVIOUS_MATCH,
  NEXT_MATCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean
terminal_search_entry_search_changed_cb (gpointer user_data)
{
  TerminalSearchEntry *self = TERMINAL_SEARCH_ENTRY (user_data);

  self->search_changed_source = 0;

  g_signal_emit (self, signals[SEARCH_CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
terminal_search_entry_changed_cb (TerminalSearchEntry *self)
{
  g_assert (TERMINAL_IS_SEARCH_ENTRY (self));

  g_clear_handle_id (&self->search_changed_source, g_source_remove);
  self->search_changed_source = g_timeout_add (SEARCH_CHANGED_TIMEOUT_MSEC,
                                               terminal_search_entry_search_changed_cb,
                                               self);
}

static void
terminal_search_entry_finalize (GObject *object)
{
  TerminalSearchEntry *self = TERMINAL_SEARCH_ENTRY (object);

  g_clear_handle_id (&self->search_changed_source, g_source_remove);

  G_OBJECT_CLASS (terminal_search_entry_parent_class)->finalize (object);
}

static void
terminal_search_entry_class_init (TerminalSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = terminal_search_entry_finalize;

  signals[SEARCH_CHANGED] =
    g_signal_new ("search-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE, 0);

  signals[PREVIOUS_MATCH] =
    g_signal_new ("previous-match",
                  G_TYPE_FROM_CLASS (klass),
                  GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                  0,
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE, 0);

  signals[NEXT_MATCH] =
    g_signal_new ("next-match",
                  G_TYPE_FROM_CLASS (klass),
                  GSignalFlags(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                  0,
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE, 0);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_g,
                                       GDK_CONTROL_MASK,
                                       "next-match",
                                       nullptr);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_g,
                                       GdkModifierType(GDK_SHIFT_MASK | GDK_CONTROL_MASK),
                                       "previous-match",
                                       nullptr);
}

static void
terminal_search_entry_init (TerminalSearchEntry *self)
{
  g_signal_connect (self,
                    "changed",
                    G_CALLBACK (terminal_search_entry_changed_cb),
                    nullptr);
}

GtkWidget *
terminal_search_entry_new (void)
{
  return (GtkWidget *)g_object_new (TERMINAL_TYPE_SEARCH_ENTRY, nullptr);
}
