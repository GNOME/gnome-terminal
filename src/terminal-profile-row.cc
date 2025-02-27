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

#include "terminal-app.hh"
#include "terminal-profile-row.hh"
#include "terminal-profiles-list.hh"
#include "terminal-preferences-window.hh"

struct _TerminalProfileRow
{
  AdwActionRow  parent_instance;

  GtkLabel     *default_label;

  char         *uuid;
  GSettings    *settings;
};

enum {
  PROP_0,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalProfileRow, terminal_profile_row, ADW_TYPE_ACTION_ROW)

static GParamSpec *properties [N_PROPS];

static void
terminal_profile_row_clone (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  GtkWidget *window = gtk_widget_get_ancestor (widget, TERMINAL_TYPE_PREFERENCES_WINDOW);
  TerminalProfileRow *self = TERMINAL_PROFILE_ROW (widget);
  TerminalApp *app = terminal_app_get ();
  g_autofree char *name = g_settings_get_string (self->settings, "visible-name");
  g_autofree char *new_name = g_strdup_printf ("%s (%s)", name, _("Copy"));
  g_autofree char *uuid = terminal_app_new_profile (app, self->settings, new_name);
  TerminalSettingsList *profiles_list = terminal_app_get_profiles_list (app);
  g_autoptr(GSettings) settings = terminal_profiles_list_ref_profile_by_uuid (profiles_list, uuid, nullptr);

  if (settings != nullptr && window != nullptr) {
    terminal_preferences_window_edit_profile (TERMINAL_PREFERENCES_WINDOW (window), settings);
  }
}

static void
terminal_profile_row_edit (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *param)
{
  adw_action_row_activate (ADW_ACTION_ROW (widget));
}

static void
terminal_profile_row_delete (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  TerminalProfileRow *self = TERMINAL_PROFILE_ROW (widget);

  terminal_app_remove_profile (terminal_app_get (), self->settings);
}

static void
terminal_profile_row_make_default (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  TerminalProfileRow *self = TERMINAL_PROFILE_ROW (widget);
  TerminalSettingsList *list;
  TerminalApp *app;

  g_assert (TERMINAL_IS_PROFILE_ROW (self));

  app = terminal_app_get ();
  list = terminal_app_get_profiles_list (app);
  terminal_settings_list_set_default_child (list, self->uuid);
}

static void
terminal_profile_row_constructed (GObject *object)
{
  TerminalProfileRow *self = (TerminalProfileRow *)object;
  TerminalApp *app = terminal_app_get ();
  TerminalSettingsList *list = terminal_app_get_profiles_list (app);
  g_autofree char *default_uuid = nullptr;
  gboolean is_default;

  G_OBJECT_CLASS (terminal_profile_row_parent_class)->constructed (object);

  self->uuid = terminal_settings_list_dup_uuid_from_child (list, self->settings);
  default_uuid = terminal_settings_list_dup_default_child (list);
  is_default = g_strcmp0 (self->uuid, default_uuid) == 0;

  gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                 "profile.set-as-default",
                                 !is_default);
  gtk_widget_set_visible (GTK_WIDGET (self->default_label), is_default);

  /* This handles both the "is-default" as well as a single profile
   * (which is going to be the default) so you cannot remove the
   * last item from the list.
   */
  gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                 "profile.delete",
                                 !is_default);

  g_settings_bind (self->settings, "visible-name", self, "title",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET));
}

static void
terminal_profile_row_dispose (GObject *object)
{
  TerminalProfileRow *self = (TerminalProfileRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_PROFILE_ROW);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (terminal_profile_row_parent_class)->dispose (object);
}

static void
terminal_profile_row_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  TerminalProfileRow *self = TERMINAL_PROFILE_ROW (object);

  switch (prop_id) {
  case PROP_SETTINGS:
    g_value_set_object (value, self->settings);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_profile_row_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  TerminalProfileRow *self = TERMINAL_PROFILE_ROW (object);

  switch (prop_id) {
  case PROP_SETTINGS:
    self->settings = G_SETTINGS (g_value_dup_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_profile_row_class_init (TerminalProfileRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = terminal_profile_row_constructed;
  object_class->dispose = terminal_profile_row_dispose;
  object_class->get_property = terminal_profile_row_get_property;
  object_class->set_property = terminal_profile_row_set_property;

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", nullptr, nullptr,
                         G_TYPE_SETTINGS,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class,
                                   "profile.clone",
                                   nullptr,
                                   terminal_profile_row_clone);
  gtk_widget_class_install_action (widget_class,
                                   "profile.edit",
                                   nullptr,
                                   terminal_profile_row_edit);
  gtk_widget_class_install_action (widget_class,
                                   "profile.delete",
                                   nullptr,
                                   terminal_profile_row_delete);
  gtk_widget_class_install_action (widget_class,
                                   "profile.set-as-default",
                                   nullptr,
                                   terminal_profile_row_make_default);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalProfileRow, default_label);
}

static void
terminal_profile_row_init (TerminalProfileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
terminal_profile_row_new (GSettings *settings)
{
  g_return_val_if_fail (G_IS_SETTINGS (settings), nullptr);

  return (GtkWidget *)g_object_new (TERMINAL_TYPE_PROFILE_ROW,
                                    "settings", settings,
                                    nullptr);
}

GSettings *
terminal_profile_row_get_settings (TerminalProfileRow *self)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE_ROW (self), nullptr);

  return self->settings;
}
