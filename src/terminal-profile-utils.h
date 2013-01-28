/*
 * Copyright © 2013 Christian Persch
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

#ifndef TERMINAL_PROFILE_UTILS_H
#define TERMINAL_PROFILE_UTILS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

char **terminal_profile_util_list_profiles (void);

char **terminal_profile_util_get_profiles (GSettings *settings);

char *terminal_profile_util_get_profile_by_uuid (const char *uuid,
                                                 GError **error);

char *terminal_profile_util_get_profile_by_uuid_or_name (const char *uuid_or_name,
                                                         GError **error);

char *terminal_profile_util_get_profile_uuid (GSettings *profile);

int terminal_profile_util_profiles_compare (gconstpointer pa,
                                            gconstpointer pb);

G_END_DECLS

#endif /* TERMINAL_UTIL_UTILS_H */
