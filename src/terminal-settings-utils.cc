/*
 *  Copyright © 2008, 2010, 2011, 2022 Christian Persch
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#define G_SETTINGS_ENABLE_BACKEND

#include "terminal-settings-utils.hh"

#include <gio/gio.h>

// BEGIN copied from glib/gio/gsettingsbackend.c

/*
 * Copyright © 2009, 2010 Codethink Limited
 * Copyright © 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Matthias Clasen <mclasen@redhat.com>
 */

static void
variant_unref0(void* data)
{
  if (data)
    g_variant_unref(reinterpret_cast<GVariant*>(data));
}

static int
compare_string(void const* a,
               void const* b,
               void* closure)
{
  return strcmp(reinterpret_cast<char const*>(a),
                reinterpret_cast<char const*>(b));
}

/*
 * terminal_g_settings_backend_create_tree:
 *
 * This is a convenience function for creating a tree that is compatible
 * with terminal_g_settings_backend_write().  It merely calls g_tree_new_full()
 * with strcmp(), g_free() and g_variant_unref().
 *
 * Returns: (transfer full): a new #GTree
 */
GTree*
terminal_g_settings_backend_create_tree(void)
{
  return g_tree_new_full(compare_string, nullptr,
                         g_free,
                         variant_unref0);
}

/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Matthias Clasen <mclasen@redhat.com>
 */

/*
 * g_settings_backend_read:
 * @backend: a #GSettingsBackend implementation
 * @key: the key to read
 * @expected_type: a #GVariantType
 * @default_value: if the default value should be returned
 *
 * Reads a key. This call will never block.
 *
 * If the key exists, the value associated with it will be returned.
 * If the key does not exist, %nullptr will be returned.
 *
 * The returned value will be of the type given in @expected_type.  If
 * the backend stored a value of a different type then %nullptr will be
 * returned.
 *
 * If @default_value is %TRUE then this gets the default value from the
 * backend (ie: the one that the backend would contain if
 * g_settings_reset() were called).
 *
 * Returns: (nullable) (transfer full): the value that was read, or %nullptr
 */
GVariant*
terminal_g_settings_backend_read(GSettingsBackend* backend,
                                 char const* key,
                                 GVariantType const* expected_type,
                                 gboolean default_value)
{
  auto value = G_SETTINGS_BACKEND_GET_CLASS(backend)->read (backend,
                                                            key,
                                                            expected_type,
                                                            default_value);

  if (value)
    value = g_variant_take_ref(value);

  if (value && !g_variant_is_of_type(value, expected_type)) [[unlikely]] {
    g_clear_pointer(&value, g_variant_unref);
  }

  return value;
}

/*
 * terminal_g_settings_backend_read_user_value:
 * @backend: a #GSettingsBackend implementation
 * @key: the key to read
 * @expected_type: a #GVariantType
 *
 * Reads the 'user value' of a key.
 *
 * This is the value of the key that the user has control over and has
 * set for themselves.  Put another way: if the user did not set the
 * value for themselves, then this will return %nullptr(even if the
 * sysadmin has provided a default value).
 *
 * Returns:(nullable)(transfer full): the value that was read, or %nullptr
 */
GVariant*
terminal_g_settings_backend_read_user_value(GSettingsBackend* backend,
                                            char const*key,
                                            GVariantType const* expected_type)
{
  auto value = G_SETTINGS_BACKEND_GET_CLASS(backend)->read_user_value(backend,
                                                                      key,
                                                                      expected_type);

  if (value)
    value = g_variant_take_ref(value);

  if (value && !g_variant_is_of_type(value, expected_type)) [[unlikely]] {
    g_clear_pointer(&value, g_variant_unref);
  }

  return value;
}

/*
 * terminal_g_settings_backend_write:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of the key
 * @value: a #GVariant value to write to this key
 * @origin_tag: the origin tag
 *
 * Writes exactly one key.
 *
 * This call does not fail.  During this call a
 * #GSettingsBackend::changed signal will be emitted if the value of the
 * key has changed.  The updated key value will be visible to any signal
 * callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal(either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 *
 * If @value has a floating reference, it will be sunk.
 *
 * Returns: %TRUE if the write succeeded, %FALSE if the key was not writable
 */
gboolean
terminal_g_settings_backend_write(GSettingsBackend* backend,
                                  char const* key,
                                  GVariant* value,
                                  void* origin_tag)
{
  g_variant_ref_sink(value);
  auto const success = G_SETTINGS_BACKEND_GET_CLASS(backend)->write(backend,
                                                                    key,
                                                                    value,
                                                                    origin_tag);
  g_variant_unref(value);

  return success;
}

/*
 * terminal_g_settings_backend_write_tree:
 * @backend: a #GSettingsBackend implementation
 * @tree: a #GTree containing key-value pairs to write
 * @origin_tag: the origin tag
 *
 * Writes one or more keys.  This call will never block.
 *
 * The key of each item in the tree is the key name to write to and the
 * value is a #GVariant to write.  The proper type of #GTree for this
 * call can be created with terminal_g_settings_backend_create_tree().  This call
 * might take a reference to the tree; you must not modified the #GTree
 * after passing it to this call.
 *
 * This call does not fail.  During this call a #GSettingsBackend::changed
 * signal will be emitted if any keys have been changed.  The new values of
 * all updated keys will be visible to any signal callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal(either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 */
gboolean
terminal_g_settings_backend_write_tree(GSettingsBackend* backend,
                                       GTree* tree,
                                       void* origin_tag)
{
  return G_SETTINGS_BACKEND_GET_CLASS(backend)->write_tree(backend,
                                                           tree,
                                                           origin_tag);
}

/*
 * terminal_g_settings_backend_reset:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of a key
 * @origin_tag: the origin tag
 *
 * "Resets" the named key to its "default" value(ie: after system-wide
 * defaults, mandatory keys, etc. have been taken into account) or possibly
 * unsets it.
 */
void
terminal_g_settings_backend_reset(GSettingsBackend*backend,
                                  char const* key,
                                  void* origin_tag)
{
  G_SETTINGS_BACKEND_GET_CLASS(backend)->reset(backend, key, origin_tag);
}

/*
 * terminal_g_settings_backend_get_writable:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of a key
 *
 * Finds out if a key is available for writing to.  This is the
 * interface through which 'lockdown' is implemented.  Locked down
 * keys will have %FALSE returned by this call.
 *
 * You should not write to locked-down keys, but if you do, the
 * implementation will deal with it.
 *
 * Returns: %TRUE if the key is writable
 */
gboolean
terminal_g_settings_backend_get_writable(GSettingsBackend* backend,
                                         char const* key)
{
  return G_SETTINGS_BACKEND_GET_CLASS(backend)->get_writable(backend, key);
}

/*
 * terminal_g_settings_backend_unsubscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Reverses the effect of a previous call to
 * terminal_g_settings_backend_subscribe().
 */
void
terminal_g_settings_backend_unsubscribe(GSettingsBackend* backend,
                                        const char* name)
{
  G_SETTINGS_BACKEND_GET_CLASS(backend)->unsubscribe(backend, name);
}

/*
 * terminal_g_settings_backend_subscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Requests that change signals be emitted for events on @name.
 */
void
terminal_g_settings_backend_subscribe(GSettingsBackend* backend,
                                      char const* name)
{
  G_SETTINGS_BACKEND_GET_CLASS(backend)->subscribe(backend, name);
}

/*
 * terminal_g_settings_backend_get_permission:
 * @backend: a #GSettingsBackend
 * @path: a path
 *
 * Gets the permission object associated with writing to keys below
 * @path on @backend.
 *
 * If this is not implemented in the backend, then a %TRUE
 * #GSimplePermission is returned.
 *
 * Returns:(not nullable)(transfer full): a non-%nullptr #GPermission.
 *     Free with g_object_unref()
 */
GPermission*
terminal_g_settings_backend_get_permission(GSettingsBackend* backend,
                                           char const* path)
{
  auto const klass = G_SETTINGS_BACKEND_GET_CLASS(backend);
  if (klass->get_permission)
    return klass->get_permission(backend, path);

  return g_simple_permission_new(TRUE);
}

/*
 * terminal_g_settings_backend_sync_default:
 *
 * Syncs.
 */
void
terminal_g_settings_backend_sync(GSettingsBackend* backend)
{
  auto const klass = G_SETTINGS_BACKEND_GET_CLASS(backend);
  if (klass->sync)
    klass->sync(backend);
}

// END copied from glib
