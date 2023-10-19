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

#include "terminal-app.hh"
#include "terminal-accel-row.hh"
#include "terminal-accels.hh"
#include "terminal-preferences-list-item.hh"
#include "terminal-preferences-window.hh"
#include "terminal-schemas.hh"

struct _TerminalPreferencesWindow
{
  AdwPreferencesWindow parent_instance;

  AdwSwitchRow       *access_keys;
  AdwSwitchRow       *accelerator_key;
  AdwSwitchRow       *always_check_default;
  AdwSwitchRow       *enable_shortcuts;
  AdwNavigationPage  *profile_page;
  AdwNavigationPage  *shortcuts_page;
  AdwPreferencesPage *shortcuts_preferences;
};

G_DEFINE_FINAL_TYPE (TerminalPreferencesWindow, terminal_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

static void
terminal_preferences_window_view_shortcuts (GtkWidget  *widget,
                                            const char *action_name,
                                            GVariant   *param)
{
  TerminalPreferencesWindow *self = TERMINAL_PREFERENCES_WINDOW (widget);

  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (self),
                                       self->shortcuts_page);
}

static void
terminal_preferences_window_set_as_default (GtkWidget  *widget,
                                            const char *action_name,
                                            GVariant   *param)
{
  terminal_app_make_default_terminal (terminal_app_get ());
}

static void
notify_is_default_terminal_cb (TerminalPreferencesWindow *self,
                               GParamSpec                *pspec,
                               TerminalApp               *app)
{
  g_assert (TERMINAL_IS_PREFERENCES_WINDOW (self));
  g_assert (TERMINAL_IS_APP (app));

  gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                 "terminal.set-as-default",
                                 !terminal_app_is_default_terminal (app));
}

static void
terminal_preferences_window_constructed (GObject *object)
{
  TerminalPreferencesWindow *self = (TerminalPreferencesWindow *)object;
  TerminalApp *app = terminal_app_get ();
  GSettings *settings = terminal_app_get_global_settings (app);

  G_OBJECT_CLASS (terminal_preferences_window_parent_class)->constructed (object);

  g_signal_connect_object (app,
                           "notify::is-default-terminal",
                           G_CALLBACK (notify_is_default_terminal_cb),
                           self,
                           G_CONNECT_SWAPPED);
  notify_is_default_terminal_cb (self, nullptr, app);

  g_settings_bind (settings,
                   TERMINAL_SETTING_ALWAYS_CHECK_DEFAULT_KEY,
                   self->always_check_default,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                   self->access_keys,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                   self->accelerator_key,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY,
                   self->enable_shortcuts,
                   "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET));

  terminal_accels_populate_preferences (self->shortcuts_preferences);
}

static void
terminal_preferences_window_dispose (GObject *object)
{
  TerminalPreferencesWindow *self = (TerminalPreferencesWindow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_PREFERENCES_WINDOW);

  G_OBJECT_CLASS (terminal_preferences_window_parent_class)->dispose (object);
}

static void
terminal_preferences_window_class_init (TerminalPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = terminal_preferences_window_constructed;
  object_class->dispose = terminal_preferences_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/preferences-window.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, accelerator_key);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, access_keys);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, always_check_default);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, enable_shortcuts);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, profile_page);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, shortcuts_page);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, shortcuts_preferences);

  gtk_widget_class_install_action (widget_class,
                                   "terminal.set-as-default",
                                   nullptr,
                                   terminal_preferences_window_set_as_default);

  gtk_widget_class_install_action (widget_class,
                                   "preferences.view-shortcuts",
                                   nullptr,
                                   terminal_preferences_window_view_shortcuts);

  g_type_ensure (ADW_TYPE_PREFERENCES_PAGE);
  g_type_ensure (ADW_TYPE_SWITCH_ROW);
  g_type_ensure (TERMINAL_TYPE_ACCEL_ROW);
  g_type_ensure (TERMINAL_TYPE_PREFERENCES_LIST_ITEM);
}

static void
terminal_preferences_window_init (TerminalPreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
terminal_preferences_window_new (void)
{
  return (GtkWidget *)g_object_new (TERMINAL_TYPE_PREFERENCES_WINDOW, nullptr);
}
