/* Copyright 2023 Christian Hergert
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

#include "terminal-preferences-list-item.hh"

struct _TerminalPreferencesListItem
{
  GObject parent_instance;
  char *title;
  GVariant *value;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalPreferencesListItem, terminal_preferences_list_item, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
terminal_preferences_list_item_dispose (GObject *object)
{
  TerminalPreferencesListItem *self = (TerminalPreferencesListItem *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->value, g_variant_unref);

  G_OBJECT_CLASS (terminal_preferences_list_item_parent_class)->dispose (object);
}

static void
terminal_preferences_list_item_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  TerminalPreferencesListItem *self = TERMINAL_PREFERENCES_LIST_ITEM (object);

  switch (prop_id) {
  case PROP_TITLE:
    g_value_set_string (value, self->title);
    break;
  case PROP_VALUE:
    g_value_set_variant (value, self->value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_preferences_list_item_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  TerminalPreferencesListItem *self = TERMINAL_PREFERENCES_LIST_ITEM (object);

  switch (prop_id) {
  case PROP_TITLE:
    self->title = g_value_dup_string (value);
    break;
  case PROP_VALUE:
    self->value = g_value_dup_variant (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_preferences_list_item_class_init (TerminalPreferencesListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = terminal_preferences_list_item_dispose;
  object_class->get_property = terminal_preferences_list_item_get_property;
  object_class->set_property = terminal_preferences_list_item_set_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", nullptr, nullptr,
                         nullptr,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_VALUE] =
    g_param_spec_variant ("value", nullptr, nullptr,
                          G_VARIANT_TYPE_ANY,
                          nullptr,
                          GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
terminal_preferences_list_item_init (TerminalPreferencesListItem *self)
{
}

GVariant *
terminal_preferences_list_item_get_value (TerminalPreferencesListItem const* self)
{
  g_return_val_if_fail (TERMINAL_IS_PREFERENCES_LIST_ITEM((void*)self), nullptr);

  return self->value;
}
