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

#include "terminal-accel-dialog.hh"
#include "terminal-accel-row.hh"
#include "terminal-accels.hh"
#include "terminal-app.hh"
#include "terminal-schemas.hh"
#include "terminal-shortcut-editor.hh"
#include "terminal-util.hh"

struct _TerminalShortcutEditor
{
  AdwNavigationPage   parent_instance;
  AdwPreferencesPage *page;
  AdwSwitchRow       *enable_shortcuts;
};

G_DEFINE_FINAL_TYPE (TerminalShortcutEditor, terminal_shortcut_editor, ADW_TYPE_NAVIGATION_PAGE)

static void
terminal_shortcut_editor_constructed (GObject *object)
{
  TerminalShortcutEditor *self = (TerminalShortcutEditor *)object;
  TerminalApp *app = terminal_app_get ();
  GSettings *settings = terminal_app_get_global_settings (app);

  G_OBJECT_CLASS (terminal_shortcut_editor_parent_class)->constructed (object);

  terminal_accels_populate_preferences (self->page);

  terminal_util_g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY,
                   self->enable_shortcuts,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
}

static void
terminal_shortcut_editor_dispose (GObject *object)
{
  TerminalShortcutEditor *self = (TerminalShortcutEditor *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_SHORTCUT_EDITOR);

  G_OBJECT_CLASS (terminal_shortcut_editor_parent_class)->dispose (object);
}

static void
terminal_shortcut_editor_class_init (TerminalShortcutEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = terminal_shortcut_editor_constructed;
  object_class->dispose = terminal_shortcut_editor_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/shortcut-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalShortcutEditor, enable_shortcuts);
  gtk_widget_class_bind_template_child (widget_class, TerminalShortcutEditor, page);

  g_type_ensure (TERMINAL_TYPE_ACCEL_ROW);
}

static void
terminal_shortcut_editor_init (TerminalShortcutEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
terminal_shortcut_editor_new (void)
{
  return GTK_WIDGET (g_object_new (TERMINAL_TYPE_SHORTCUT_EDITOR, nullptr));
}
