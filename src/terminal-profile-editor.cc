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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "terminal-profile-editor.hh"

struct _TerminalProfileEditor
{
  AdwNavigationPage   parent_instance;

  AdwPreferencesPage *page;

  GSettings          *settings;
};

enum {
  PROP_0,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalProfileEditor, terminal_profile_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties [N_PROPS];

static void
terminal_profile_editor_constructed (GObject *object)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  G_OBJECT_CLASS (terminal_profile_editor_parent_class)->constructed (object);
}

static void
terminal_profile_editor_dispose (GObject *object)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_PROFILE_EDITOR);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (terminal_profile_editor_parent_class)->dispose (object);
}

static void
terminal_profile_editor_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  switch (prop_id) {
  case PROP_SETTINGS:
    g_value_set_object (value, self->settings);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_profile_editor_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  switch (prop_id) {
  case PROP_SETTINGS:
    self->settings = G_SETTINGS (g_value_dup_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_profile_editor_class_init (TerminalProfileEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = terminal_profile_editor_constructed;
  object_class->dispose = terminal_profile_editor_dispose;
  object_class->get_property = terminal_profile_editor_get_property;
  object_class->set_property = terminal_profile_editor_set_property;

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", nullptr, nullptr,
                         G_TYPE_SETTINGS,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/profile-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, page);
}

static void
terminal_profile_editor_init (TerminalProfileEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
terminal_profile_editor_new (GSettings *settings)
{
  g_return_val_if_fail (G_IS_SETTINGS (settings), nullptr);

  return (GtkWidget *)g_object_new (TERMINAL_TYPE_PROFILE_EDITOR,
                                    "settings", settings,
                                    nullptr);
}
