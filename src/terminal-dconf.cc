/*
 * Copyright © 2013, 2022 Christian Persch
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

#include "terminal-dconf.hh"
#include "terminal-libgsystem.hh"

// See https://gitlab.gnome.org/GNOME/dconf/-/issues/23
extern "C" {
#include <dconf.h>
}

gboolean
terminal_dconf_backend_is_dconf(GSettingsBackend* backend)
{
  return g_str_equal(G_OBJECT_TYPE_NAME(backend), "DConfSettingsBackend");
}

static bool
clone_schema(GSettingsSchemaSource* schema_source,
             char const* schema_id,
             char const* path,
             char const* new_path,
             DConfClient** client,
             DConfChangeset** changeset)
{
  gs_unref_settings_schema auto schema =
    g_settings_schema_source_lookup(schema_source, schema_id, true);
   /* shouldn't really happen ever */
  if (schema == nullptr)
    return false;

  *client = dconf_client_new();
  *changeset = dconf_changeset_new();

  gs_strfreev auto keys = g_settings_schema_list_keys(schema);

  for (auto i = 0; keys[i]; i++) {
    gs_free auto rkey = g_strconcat(path, keys[i], nullptr);
    gs_unref_variant auto value = dconf_client_read(*client, rkey);
    if (value) {
      gs_free auto wkey = g_strconcat(new_path, keys[i], nullptr);
      dconf_changeset_set(*changeset, wkey, value);
    }
  }

  return true;
}

void
terminal_dconf_clone_schema(GSettingsSchemaSource* schema_source,
                            char const* schema_id,
                            char const* path,
                            char const* new_path,
                            char const* first_key,
                            ...)
{
  gs_unref_object DConfClient* client = nullptr;
  DConfChangeset* changeset = nullptr;
  if (!clone_schema(schema_source, schema_id, path, new_path, &client, &changeset))
    return;

  va_list args;
  va_start(args, first_key);
  while (first_key != nullptr) {
    auto const type = va_arg(args, char const*);
    auto const value = g_variant_new_va(type, nullptr, &args);
    gs_free auto wkey = g_strconcat(new_path, first_key, nullptr);

    dconf_changeset_set(changeset, wkey, value);
    first_key = va_arg(args, char const*);
  }
  va_end(args);

  dconf_client_change_sync(client, changeset, nullptr, nullptr, nullptr);
  dconf_changeset_unref(changeset);
}

void
terminal_dconf_clone_schemav(GSettingsSchemaSource*schema_source,
                             char const* schema_id,
                             char const* path,
                             char const* new_path,
                             GVariant* asv)
{
  gs_unref_object DConfClient* client = nullptr;
  DConfChangeset* changeset = nullptr;
  if (!clone_schema(schema_source, schema_id, path, new_path, &client, &changeset))
    return;

  auto iter = GVariantIter{};
  g_variant_iter_init(&iter, asv);
  char* key = nullptr;
  GVariant* value = nullptr;
  while (g_variant_iter_loop(&iter, "(&sv)", &key, &value)) {
    gs_free auto wkey = g_strconcat(new_path, key, nullptr);
    dconf_changeset_set(changeset, wkey, value);
  }

  dconf_client_change_sync(client, changeset, nullptr, nullptr, nullptr);
  dconf_changeset_unref(changeset);
}

void
terminal_dconf_erase_path(char const* path)
{
  gs_unref_object DConfClient* client = dconf_client_new();
  dconf_client_write_sync(client, path, nullptr, nullptr, nullptr, nullptr);
}
