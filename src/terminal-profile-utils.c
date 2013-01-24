/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2011, 2013 Christian Persch
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

#include "terminal-profile-utils.h"
#include "terminal-schemas.h"

#include <string.h>
#include <uuid.h>

static gboolean
validate_profile_name (const char *name)
{
  uuid_t u;

  return uuid_parse ((char *) name, u) == 0;
}

static gboolean
validate_profile_list (char **profiles)
{
  guint i;

  if (profiles == NULL)
    return FALSE;

  for (i = 0; profiles[i]; i++) {
    if (!validate_profile_name (profiles[i]))
      return FALSE;
  }

  return i > 0;
}

static gboolean
map_profiles_list (GVariant *value,
                   gpointer *result,
                   gpointer user_data G_GNUC_UNUSED)
{
  char **profiles;

  profiles = g_variant_dup_strv (value, NULL);
  if (validate_profile_list (profiles)) {
    *result = profiles;
    return TRUE;
  }

  g_strfreev (profiles);
  return FALSE;
}

/**
 * terminal_profile_util_get_profiles:
 * @settings: a #GSettings for the TERMINAL_SETTINGS_SCHEMA schema
 *
 * Returns: (transfer full): the list of profile UUIDs, or %NULL
 */
char **
terminal_profile_util_get_profiles (GSettings *settings)
{
  /* Use get_mapped so we can be sure never to get valid profile names, and
   * never an empty profile list, since the schema defines one profile.
   */
  return g_settings_get_mapped (settings,
                                TERMINAL_SETTING_PROFILES_KEY,
                                map_profiles_list, NULL);
}

/**
 * terminal_profile_util_list_profiles:
 *
 * Returns: (transfer full): the list of profile UUIDs, or %NULL
 */
char **
terminal_profile_util_list_profiles (void)
{
  GSettings *settings;
  char **profiles;

  settings = g_settings_new (TERMINAL_SETTING_SCHEMA);
  profiles = terminal_profile_util_get_profiles (settings);
  g_object_unref (settings);

  return profiles;
}

static char **
get_profile_names (char **profiles)
{
  GSettings *profile;
  char **names, *path;
  guint i, n;

  n = g_strv_length (profiles);
  names = g_new0 (char *, n + 1);
  for (i = 0; i < n; i++) {
    path = g_strdup_printf (TERMINAL_PROFILES_PATH_PREFIX ":%s/", profiles[i]);
    profile = g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, path);
    g_free (path);

    names[i] = g_settings_get_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
    g_object_unref (profile);
  }

  names[n] = NULL;
  return names;
}

/* Counts occurrences of @str in @strv */
static guint
strv_contains (char **strv,
               const char *str)
{
  guint n;

  if (strv == NULL)
    return 0;

  n = 0;
  for ( ; *strv; strv++)
    if (strcmp (*strv, str) == 0)
      n++;

  return n;
}

/**
 * terminal_profile_util_get_profile_by_uuid:
 * @uuid:
 * @error:
 *
 * Returns: (transfer full): the UUID of the profile specified by @uuid, or %NULL
 */
char *
terminal_profile_util_get_profile_by_uuid (const char *uuid,
                                           GError **error)
{
  char **profiles;
  guint n;

  profiles = terminal_profile_util_list_profiles ();
  n = strv_contains (profiles, uuid);
  g_strfreev (profiles);

  if (n != 0)
    return g_strdup (uuid);

  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               "No profile with UUID \"%s\" exists", uuid);
  return NULL;
}

/**
 * terminal_profile_util_get_profile_by_uuid:
 * @uuid:
 * @error:
 *
 * Returns: (transfer full): the UUID of the profile specified by @uuid, or %NULL
 */
char *
terminal_profile_util_get_profile_by_uuid_or_name (const char *uuid_or_name,
                                                   GError **error)
{
  char **profiles, **profile_names;
  guint n;

  profiles = terminal_profile_util_list_profiles ();
  n = strv_contains (profiles, uuid_or_name);

  if (n != 0) {
    g_strfreev (profiles);
    return g_strdup (uuid_or_name);
  }

  /* Not found as UUID; try finding a profile with this string as 'visible-name' */
  profile_names = get_profile_names (profiles);
  g_strfreev (profiles);

  n = strv_contains (profile_names, uuid_or_name);
  g_strfreev (profile_names);

  if (n == 0) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "No profile with UUID or name \"%s\" exists", uuid_or_name);
    return NULL;
  } else if (n != 1) {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "No profile with UUID \"%s\" found and name is ambiguous", uuid_or_name);
    return NULL;
  }

  return g_strdup (uuid_or_name);
}

/**
 * terminal_profile_util_get_profile_uuid:
 * @profile: a #GSettings for the %TERMINAL_PROFILE_SCHEMA schema
 *
 * Returns: (transfer full): the profile's UUID
 */
char *
terminal_profile_util_get_profile_uuid (GSettings *profile)
{
  char *path, *uuid;

  g_object_get (profile, "path", &path, NULL);
  g_assert (g_str_has_prefix (path, TERMINAL_PROFILES_PATH_PREFIX ":"));
  uuid = g_strdup (path + strlen (TERMINAL_PROFILES_PATH_PREFIX ":"));
  g_free (path);

  g_assert (strlen (uuid) == 37);
  uuid[36] = '\0';
  return uuid;
}
