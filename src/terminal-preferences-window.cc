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

#include <glib/gi18n.h>

#include "terminal-app.hh"
#include "terminal-preferences-list-item.hh"
#include "terminal-preferences-window.hh"
#include "terminal-profile-editor.hh"
#include "terminal-profile-row.hh"
#include "terminal-schemas.hh"
#include "terminal-shortcut-editor.hh"

struct _TerminalPreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  AdwSwitchRow         *access_keys;
  AdwSwitchRow         *accelerator_key;
  AdwSwitchRow         *always_check_default;
  AdwComboRow          *theme_variant;

  GtkListBox           *profiles_list_box;
  GtkListBoxRow        *add_profile_row;
};

static const char *const theme_variants[] = {"system", "dark", "light"};

G_DEFINE_FINAL_TYPE (TerminalPreferencesWindow, terminal_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

static void
terminal_preferences_window_add_profile (GtkWidget  *widget,
                                         const char *action_name,
                                         GVariant   *param)
{
  TerminalApp *app = terminal_app_get ();
  g_autofree char *uuid = terminal_app_new_profile (app, nullptr, _("New Profile"));

  /* Showing the new profile immediately is a bit jarring because we're
   * not able to show the action that happened on the list. Just reload
   * the list (happens externally) and let the user click the next action.
   */
#if 0
  TerminalSettingsList *profiles = terminal_app_get_profiles_list (app);
  g_autoptr(GSettings) settings = terminal_settings_list_ref_child (profiles, uuid);

  terminal_preferences_window_edit_profile (TERMINAL_PREFERENCES_WINDOW (widget), settings);
#endif
}

static void
terminal_preferences_window_view_shortcuts (GtkWidget  *widget,
                                            const char *action_name,
                                            GVariant   *param)
{
  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (widget),
                                       ADW_NAVIGATION_PAGE (terminal_shortcut_editor_new ()));
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
terminal_preferences_window_profile_row_activated_cb (TerminalPreferencesWindow *self,
                                                      TerminalProfileRow        *row)
{
  g_assert (TERMINAL_IS_PREFERENCES_WINDOW (self));
  g_assert (TERMINAL_IS_PROFILE_ROW (row));

  terminal_preferences_window_edit_profile (self,
                                            terminal_profile_row_get_settings (row));
}

static void
terminal_preferences_window_reload_profiles (TerminalPreferencesWindow *self)
{
  g_autolist(GSettings) profiles_settings = nullptr;
  TerminalSettingsList *profiles;
  TerminalApp *app;
  GtkWidget *child;

  g_assert (TERMINAL_IS_PREFERENCES_WINDOW (self));

  app = terminal_app_get ();
  profiles = terminal_app_get_profiles_list (app);
  profiles_settings = terminal_profiles_list_ref_children_sorted (profiles);

  child = gtk_widget_get_first_child (GTK_WIDGET (self->profiles_list_box));

  while (child != nullptr) {
    GtkListBoxRow *row = GTK_LIST_BOX_ROW (child);

    child = gtk_widget_get_next_sibling (child);

    if (row != self->add_profile_row)
      gtk_list_box_remove (self->profiles_list_box, GTK_WIDGET (row));
  }

  for (const GList *iter = g_list_last (profiles_settings); iter; iter = iter->prev) {
    GSettings *settings = G_SETTINGS (iter->data);
    GtkWidget *row = terminal_profile_row_new (settings);

    g_signal_connect_object (row,
                             "activated",
                             G_CALLBACK (terminal_preferences_window_profile_row_activated_cb),
                             self,
                             G_CONNECT_SWAPPED);
    gtk_list_box_prepend (self->profiles_list_box, row);
  }
}

static gboolean
theme_variant_to_index (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  const char *str = g_variant_get_string (variant, nullptr);

  for (guint i = 0; i < G_N_ELEMENTS (theme_variants); i++) {
    if (strcmp (str, theme_variants[i]) == 0) {
      g_value_set_uint (value, i);
      return TRUE;
    }
  }

  return FALSE;
}

static GVariant *
index_to_theme_variant (const GValue       *value,
                        const GVariantType *type,
                        gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  if (index < G_N_ELEMENTS (theme_variants))
    return g_variant_new_string (theme_variants[index]);
  return nullptr;
}

static void
terminal_preferences_window_constructed (GObject *object)
{
  TerminalPreferencesWindow *self = (TerminalPreferencesWindow *)object;
  TerminalSettingsList *profiles;
  TerminalApp *app;
  GSettings *settings;

  G_OBJECT_CLASS (terminal_preferences_window_parent_class)->constructed (object);

  app = terminal_app_get ();
  settings = terminal_app_get_global_settings (app);
  profiles = terminal_app_get_profiles_list (app);

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

  g_signal_connect_object (profiles,
                           "children-changed",
                           G_CALLBACK (terminal_preferences_window_reload_profiles),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (profiles,
                           "default-changed",
                           G_CALLBACK (terminal_preferences_window_reload_profiles),
                           self,
                           G_CONNECT_SWAPPED);
  terminal_preferences_window_reload_profiles (self);

  g_settings_bind_with_mapping (settings,
                                TERMINAL_SETTING_THEME_VARIANT_KEY,
                                self->theme_variant,
                                "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET),
                                theme_variant_to_index,
                                index_to_theme_variant,
                                nullptr, nullptr);
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
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, add_profile_row);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, always_check_default);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, profiles_list_box);
  gtk_widget_class_bind_template_child (widget_class, TerminalPreferencesWindow, theme_variant);

  gtk_widget_class_install_action (widget_class,
                                   "terminal.set-as-default",
                                   nullptr,
                                   terminal_preferences_window_set_as_default);

  gtk_widget_class_install_action (widget_class,
                                   "preferences.view-shortcuts",
                                   nullptr,
                                   terminal_preferences_window_view_shortcuts);

  gtk_widget_class_install_action (widget_class,
                                   "profile.add",
                                   nullptr,
                                   terminal_preferences_window_add_profile);

  g_type_ensure (TERMINAL_TYPE_PREFERENCES_LIST_ITEM);
  g_type_ensure (TERMINAL_TYPE_PROFILE_EDITOR);
  g_type_ensure (TERMINAL_TYPE_SHORTCUT_EDITOR);
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

void
terminal_preferences_window_edit_profile (TerminalPreferencesWindow *self,
                                          GSettings                 *settings)
{
  GtkWidget *editor;

  g_return_if_fail (TERMINAL_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (G_IS_SETTINGS (settings));

  editor = terminal_profile_editor_new (settings);

  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (self),
                                       ADW_NAVIGATION_PAGE (editor));
}
