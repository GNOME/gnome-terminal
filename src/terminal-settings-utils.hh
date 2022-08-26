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

#pragma once

#include <gio/gsettingsbackend.h>

GTree* terminal_g_settings_backend_create_tree(void);

GPermission* terminal_g_settings_backend_get_permission(GSettingsBackend* backend,
                                                        char const*path);

gboolean terminal_g_settings_backend_get_writable(GSettingsBackend* backend,
                                                  const char* key);

GVariant* terminal_g_settings_backend_read(GSettingsBackend* backend,
                                           char const* key,
                                           GVariantType const* expected_type,
                                           gboolean default_value);

GVariant* terminal_g_settings_backend_read_user_value(GSettingsBackend* backend,
                                                      char const* key,
                                                      GVariantType const* expected_type);

void terminal_g_settings_backend_reset(GSettingsBackend* backend,
                                       char const* key,
                                       void* origin_tag);

void terminal_g_settings_backend_subscribe(GSettingsBackend* backend,
                                           const char* name);

void terminal_g_settings_backend_sync(GSettingsBackend* backend);

void terminal_g_settings_backend_unsubscribe(GSettingsBackend* backend,
                                             const char* name);

gboolean terminal_g_settings_backend_write(GSettingsBackend* backend,
                                           char const* key,
                                           GVariant* value,
                                           void* origin_tag);

gboolean terminal_g_settings_backend_write_tree(GSettingsBackend* backend,
                                                      GTree*tree,
                                                      void* origin_tag);
