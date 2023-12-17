/*
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>

#include "terminal-accel-dialog.hh"
#include "terminal-accel-row.hh"
#include "terminal-accels.hh"
#include "terminal-app.hh"
#include "terminal-util.hh"

struct _TerminalAccelRow
{
  AdwActionRow parent_instance;
  char *key;
  char *accelerator;
};

enum {
  PROP_0,
  PROP_KEY,
  PROP_ACCELERATOR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalAccelRow, terminal_accel_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static char *
accelerator_to_label (G_GNUC_UNUSED gpointer  unused,
                      const char             *accelerator)
{
  GdkModifierType modifier;
  guint key;

  if (!accelerator || !gtk_accelerator_parse (accelerator, &key, &modifier))
    return g_strdup (_("disabled"));

  return gtk_accelerator_get_label (key, modifier);
}

static void
terminal_accel_row_shortcut_set_cb (TerminalAccelRow    *self,
                                    const char          *accelerator,
                                    TerminalAccelDialog *dialog)
{
  GSettings *settings;

  g_assert (TERMINAL_IS_ACCEL_ROW (self));
  g_assert (TERMINAL_IS_ACCEL_DIALOG (dialog));

  settings = terminal_accels_get_settings ();

  g_settings_set_string (settings, self->key, accelerator ? accelerator : "disabled");
}

static void
terminal_accel_row_activate (AdwActionRow *row)
{
  TerminalAccelRow *self = TERMINAL_ACCEL_ROW (row);
  TerminalAccelDialog *dialog;
  const char *title;
  GtkRoot *root;

  root = gtk_widget_get_root (GTK_WIDGET (row));
  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self));
  dialog = (TerminalAccelDialog *)g_object_new (TERMINAL_TYPE_ACCEL_DIALOG,
                                                "transient-for", root,
                                                "shortcut-title", title,
                                                "accelerator", self->accelerator,
                                                "title", _("Set Shortcut"),
                                                nullptr);
  g_signal_connect_object (dialog,
                           "shortcut-set",
                           G_CALLBACK (terminal_accel_row_shortcut_set_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
terminal_accel_row_constructed (GObject *object)
{
  TerminalAccelRow *self = (TerminalAccelRow *)object;
  GSettings *settings;

  G_OBJECT_CLASS (terminal_accel_row_parent_class)->constructed (object);

  g_return_if_fail (self->key != nullptr);

  settings = terminal_accels_get_settings ();

  g_settings_bind (settings, self->key, self, "accelerator", G_SETTINGS_BIND_DEFAULT);
  terminal_util_set_settings_and_key_for_widget(GTK_WIDGET(self),
                                                settings,
                                                self->key);
}

static void
terminal_accel_row_dispose (GObject *object)
{
  TerminalAccelRow *self = (TerminalAccelRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_ACCEL_ROW);

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->accelerator, g_free);

  G_OBJECT_CLASS (terminal_accel_row_parent_class)->dispose (object);
}

static void
terminal_accel_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TerminalAccelRow *self = TERMINAL_ACCEL_ROW (object);

  switch (prop_id) {
  case PROP_KEY:
    g_value_set_string (value, self->key);
    break;
  case PROP_ACCELERATOR:
    g_value_set_string (value, self->accelerator);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_accel_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TerminalAccelRow *self = TERMINAL_ACCEL_ROW (object);

  switch (prop_id) {
  case PROP_KEY:
    g_set_str (&self->key, g_value_get_string (value));
    break;
  case PROP_ACCELERATOR:
    g_set_str (&self->accelerator, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_accel_row_class_init (TerminalAccelRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  AdwActionRowClass *action_row_class = ADW_ACTION_ROW_CLASS (klass);

  object_class->constructed = terminal_accel_row_constructed;
  object_class->dispose = terminal_accel_row_dispose;
  object_class->get_property = terminal_accel_row_get_property;
  object_class->set_property = terminal_accel_row_set_property;

  action_row_class->activate = terminal_accel_row_activate;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/accel-row.ui");

  gtk_widget_class_bind_template_callback (widget_class, accelerator_to_label);

  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", nullptr, nullptr,
                         nullptr,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_KEY] =
    g_param_spec_string ("key", nullptr, nullptr,
                         nullptr,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
terminal_accel_row_init (TerminalAccelRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
