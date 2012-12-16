/*
 * Copyright © 2011 Christian Persch
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gnome-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <vte/vte.h>
#include <uuid.h>

#include "terminal-schemas.h"
#include "terminal-type-builtins.h"

static gboolean dry_run = FALSE;
static gboolean verbose = FALSE;

static const GOptionEntry options[] = {
  { "dry-run", 0, 0, G_OPTION_ARG_NONE, &dry_run, NULL, NULL },
  { "verbose", 0, 0, G_OPTION_ARG_NONE, &verbose, NULL, NULL },
  { NULL }
};

#define ERROR_DOMAIN (g_intern_static_string ("gnome-terminal-migration-error"))

enum {
  ERROR_GENERIC
};

#define GCONF_PREFIX           "/apps/gnome-terminal"
#define GCONF_GLOBAL_PREFIX    GCONF_PREFIX "/global"
#define GCONF_PROFILES_PREFIX  GCONF_PREFIX "/profiles"

#define KEY_ALLOW_BOLD "allow_bold"
#define KEY_BACKGROUND_COLOR "background_color"
#define KEY_BACKGROUND_DARKNESS "background_darkness"
#define KEY_BACKGROUND_IMAGE_FILE "background_image"
#define KEY_BACKGROUND_TYPE "background_type"
#define KEY_BACKSPACE_BINDING "backspace_binding"
#define KEY_BOLD_COLOR "bold_color"
#define KEY_BOLD_COLOR_SAME_AS_FG "bold_color_same_as_fg"
#define KEY_CURSOR_BLINK_MODE "cursor_blink_mode"
#define KEY_CURSOR_SHAPE "cursor_shape"
#define KEY_CUSTOM_COMMAND "custom_command"
#define KEY_DEFAULT_SHOW_MENUBAR "default_show_menubar"
#define KEY_DEFAULT_SIZE_COLUMNS "default_size_columns"
#define KEY_DEFAULT_SIZE_ROWS "default_size_rows"
#define KEY_DELETE_BINDING "delete_binding"
#define KEY_ENCODING "encoding"
#define KEY_EXIT_ACTION "exit_action"
#define KEY_FONT "font"
#define KEY_FOREGROUND_COLOR "foreground_color"
#define KEY_LOGIN_SHELL "login_shell"
#define KEY_PALETTE "palette"
#define KEY_SCROLL_BACKGROUND "scroll_background"
#define KEY_SCROLLBACK_LINES "scrollback_lines"
#define KEY_SCROLLBACK_UNLIMITED "scrollback_unlimited"
#define KEY_SCROLLBAR_POSITION "scrollbar_position"
#define KEY_SCROLL_ON_KEYSTROKE "scroll_on_keystroke"
#define KEY_SCROLL_ON_OUTPUT "scroll_on_output"
#define KEY_SILENT_BELL "silent_bell"
#define KEY_TITLE_MODE "title_mode"
#define KEY_TITLE "title"
#define KEY_UPDATE_RECORDS "update_records"
#define KEY_USE_CUSTOM_COMMAND "use_custom_command"
#define KEY_USE_CUSTOM_DEFAULT_SIZE "use_custom_default_size"
#define KEY_USE_SYSTEM_FONT "use_system_font"
#define KEY_USE_THEME_COLORS "use_theme_colors"
#define KEY_VISIBLE_NAME "visible_name"
#define KEY_WORD_CHARS "word_chars"

static const GConfEnumStringPair erase_binding_pairs[] = {
  { VTE_ERASE_AUTO, "auto" },
  { VTE_ERASE_ASCII_BACKSPACE, "control-h" },
  { VTE_ERASE_ASCII_DELETE, "ascii-del" },
  { VTE_ERASE_DELETE_SEQUENCE, "escape-sequence" },
  { VTE_ERASE_TTY, "tty" },
  { -1, NULL }
};

static const GConfEnumStringPair scrollbar_position_pairs[] = {
  { GTK_POLICY_ALWAYS, "left" },
  { GTK_POLICY_ALWAYS, "right" },
  { GTK_POLICY_NEVER,  "hidden" },
  { -1, NULL }
};

static gboolean
string_to_enum (GType type,
                const char *s,
                int *val)
{
  GEnumClass *klass;
  GEnumValue *eval = NULL;
  guint i;

  klass = g_type_class_ref (type);
  for (i = 0; i < klass->n_values; ++i) {
    if (strcmp (klass->values[i].value_nick, s) != 0)
      continue;

    eval = &klass->values[i];
    break;
  }

  if (eval)
    *val = eval->value;

  g_type_class_unref (klass);

  return eval != NULL;
}

static void
migrate_bool (GConfClient *client,
              const char *gconf_path,
              const char *gconf_key,
              GSettings *settings,
              const char *settings_key,
              gboolean invert)
{
  GConfValue *value;
  char *key;

  key = gconf_concat_dir_and_key (gconf_path, gconf_key);
  value = gconf_client_get_without_default (client, key, NULL);
  g_free (key);

  if (value != NULL &&
      value->type == GCONF_VALUE_BOOL)
    g_settings_set_boolean (settings, settings_key,
                            invert ? !gconf_value_get_bool (value)
                                   : gconf_value_get_bool (value));

  if (value)
    gconf_value_free (value);
}

static void
migrate_int (GConfClient *client,
             const char *gconf_path,
             const char *gconf_key,
             GSettings *settings,
             const char *settings_key)
{
  GConfValue *value;
  char *key;

  key = gconf_concat_dir_and_key (gconf_path, gconf_key);
  value = gconf_client_get_without_default (client, key, NULL);
  g_free (key);

  if (value != NULL &&
      value->type == GCONF_VALUE_INT)
    g_settings_set_int (settings, settings_key, gconf_value_get_int (value));

  if (value)
    gconf_value_free (value);
}

static void
migrate_string (GConfClient *client,
                const char *gconf_path,
                const char *gconf_key,
                GSettings *settings,
                const char *settings_key)
{
  GConfValue *value;
  char *key;

  key = gconf_concat_dir_and_key (gconf_path, gconf_key);
  value = gconf_client_get_without_default (client, key, NULL);
  g_free (key);

  if (value != NULL &&
      value->type == GCONF_VALUE_STRING)
    g_settings_set_string (settings, settings_key, gconf_value_get_string (value));

  if (value)
    gconf_value_free (value);
}

static void
migrate_string_list (GConfClient *client,
                     const char *gconf_path,
                     const char *gconf_key,
                     GSettings *settings,
                     const char *settings_key)
{
  GConfValue *value;
  GVariantBuilder builder;
  GSList *l;
  char *key;

  key = gconf_concat_dir_and_key (gconf_path, gconf_key);
  value = gconf_client_get_without_default (client, key, NULL);
  g_free (key);

  if (value != NULL &&
      value->type == GCONF_VALUE_LIST &&
      gconf_value_get_list_type (value) == GCONF_VALUE_STRING) {
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

    for (l = gconf_value_get_list (value); l != NULL; l = l->next)
      g_variant_builder_add (&builder, "s", gconf_value_get_string (l->data));

    g_settings_set (settings, settings_key, "as", &builder);
  }

  if (value)
    gconf_value_free (value);
}

static void
migrate_enum (GConfClient *client,
              const char *gconf_path,
              const char *gconf_key,
              const GConfEnumStringPair *pairs,
              GSettings *settings,
              const char *settings_key)
{
  GConfValue *value;
  int val;
  char *key;

  key = gconf_concat_dir_and_key (gconf_path, gconf_key);
  value = gconf_client_get_without_default (client, key, NULL);
  g_free (key);

  if (value != NULL &&
      value->type == GCONF_VALUE_STRING &&
      gconf_string_to_enum ((GConfEnumStringPair*) pairs, gconf_value_get_string (value), &val))
    g_settings_set_enum (settings, settings_key, val);

  if (value)
    gconf_value_free (value);
}

static void
migrate_genum (GConfClient *client,
               const char *gconf_path,
               const char *gconf_key,
               GSettings *settings,
               const char *settings_key,
               GType enum_type)
{
  GConfValue *value;
  int val;
  char *key;

  key = gconf_concat_dir_and_key (gconf_path, gconf_key);
  value = gconf_client_get_without_default (client, key, NULL);
  g_free (key);

  if (value != NULL &&
      value->type == GCONF_VALUE_STRING &&
      string_to_enum (enum_type, gconf_value_get_string (value), &val))
    g_settings_set_enum (settings, settings_key, val);

  if (value)
    gconf_value_free (value);
}

static gboolean
migrate_global_prefs (GError **error)
{
  GConfClient *client;
  GSettings *settings;

  settings = g_settings_new (TERMINAL_SETTING_SCHEMA);
  client = gconf_client_get_default ();

  migrate_bool (client, GCONF_GLOBAL_PREFIX, "confirm_window_close",
                settings, TERMINAL_SETTING_CONFIRM_CLOSE_KEY,
                FALSE);
  migrate_bool (client, GCONF_GLOBAL_PREFIX, "use_mnemonics",
                settings, TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                FALSE);
  migrate_bool (client, GCONF_GLOBAL_PREFIX, "use_menu_accelerator",
                settings, TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                FALSE);
  migrate_string_list (client, GCONF_GLOBAL_PREFIX, "active_encodings",
                       settings, TERMINAL_SETTING_ENCODINGS_KEY);

  g_object_unref (settings);
  g_object_unref (client);

  return TRUE;
}

static char *
migrate_profile (GConfClient *client,
                 GSettings *global_settings,
                 const char *gconf_id,
                 gboolean is_default)
{
  GSettings *settings;
  char *path;
  const char *name;
  uuid_t u;
  char str[37];

  uuid_generate (u);
  uuid_unparse (u, str);

  path = g_strdup_printf (TERMINAL_PROFILES_PATH_PREFIX ":%s/", str);
  if (verbose)
    g_print ("Migrating profile \"%s\" to \"%s\"\n", gconf_id, path);

  settings = g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, path);
  g_free (path);

  path = gconf_concat_dir_and_key (GCONF_PROFILES_PREFIX, gconf_id);

  migrate_string (client, path, KEY_VISIBLE_NAME,
                  settings, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

  g_settings_get (settings, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &name);
  if (strlen (name) == 0)
    g_settings_set_string (settings, TERMINAL_PROFILE_VISIBLE_NAME_KEY, _("Unnamed"));

  migrate_string (client, path, KEY_FOREGROUND_COLOR,
                  settings, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY);
  migrate_string (client, path, KEY_BACKGROUND_COLOR,
                  settings, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY);
  migrate_string (client, path, KEY_BOLD_COLOR,
                 settings, TERMINAL_PROFILE_BOLD_COLOR_KEY);
  migrate_bool (client, path, KEY_BOLD_COLOR_SAME_AS_FG,
                settings, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                FALSE);
  migrate_genum (client, path, KEY_TITLE_MODE,
                 settings, TERMINAL_PROFILE_TITLE_MODE_KEY,
                 TERMINAL_TYPE_TITLE_MODE);
  migrate_string (client, path, KEY_TITLE,
                  settings, TERMINAL_PROFILE_TITLE_KEY);
  migrate_bool (client, path, KEY_ALLOW_BOLD,
                settings, TERMINAL_PROFILE_ALLOW_BOLD_KEY,
                FALSE);
  migrate_bool (client, path, KEY_SILENT_BELL,
                settings, TERMINAL_PROFILE_AUDIBLE_BELL_KEY,
                TRUE);
  migrate_string (client, path, KEY_WORD_CHARS,
                 settings, TERMINAL_PROFILE_WORD_CHARS_KEY);
  migrate_bool (client, path, KEY_USE_CUSTOM_DEFAULT_SIZE,
                settings, TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE_KEY,
                FALSE);
  migrate_int (client, path, KEY_DEFAULT_SIZE_COLUMNS,
               settings, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY);
  migrate_int (client, path, KEY_DEFAULT_SIZE_ROWS,
               settings, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY);
  migrate_enum (client, path, KEY_SCROLLBAR_POSITION,
                scrollbar_position_pairs,
                settings, TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY);
  { // FIX these as -1 == unlimited? 
  migrate_int (client, path, KEY_SCROLLBACK_LINES,
               settings, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY);
  migrate_bool (client, path, KEY_SCROLLBACK_UNLIMITED,
                settings, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                FALSE);
  }
  migrate_bool (client, path, KEY_SCROLL_ON_KEYSTROKE,
                settings, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY,
                FALSE);
  migrate_bool (client, path, KEY_SCROLL_ON_OUTPUT,
                settings, TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY,
                FALSE);
  migrate_genum (client, path, KEY_EXIT_ACTION,
                settings, TERMINAL_PROFILE_EXIT_ACTION_KEY,
                TERMINAL_TYPE_EXIT_ACTION);
  migrate_bool (client, path, KEY_LOGIN_SHELL,
                settings, TERMINAL_PROFILE_LOGIN_SHELL_KEY,
                FALSE);
  migrate_bool (client, path, KEY_UPDATE_RECORDS,
                settings, TERMINAL_PROFILE_UPDATE_RECORDS_KEY,
                FALSE);
  migrate_bool (client, path, KEY_USE_CUSTOM_COMMAND,
                settings, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                FALSE);
  migrate_string (client, path, KEY_CUSTOM_COMMAND,
                  settings, TERMINAL_PROFILE_CUSTOM_COMMAND_KEY);
  migrate_genum (client, path, KEY_CURSOR_BLINK_MODE,
                 settings, TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY,
                 VTE_TYPE_TERMINAL_CURSOR_BLINK_MODE);
  migrate_genum (client, path, KEY_CURSOR_SHAPE,
                 settings, TERMINAL_PROFILE_CURSOR_SHAPE_KEY,
                 VTE_TYPE_TERMINAL_CURSOR_SHAPE);
  migrate_string_list (client, path, KEY_PALETTE,
                       settings, TERMINAL_PROFILE_PALETTE_KEY);
  migrate_string (client, path, KEY_FONT,
                  settings, TERMINAL_PROFILE_FONT_KEY);
  migrate_enum (client, path, KEY_BACKSPACE_BINDING, erase_binding_pairs,
                settings, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY);
  migrate_enum (client, path, KEY_DELETE_BINDING, erase_binding_pairs,
                settings, TERMINAL_PROFILE_DELETE_BINDING_KEY);
  migrate_bool (client, path, KEY_USE_THEME_COLORS,
                settings, TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                FALSE);
  migrate_bool (client, path, KEY_USE_SYSTEM_FONT,
                settings, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                FALSE);
  migrate_string (client, path, KEY_ENCODING,
                  settings, TERMINAL_PROFILE_ENCODING);

  g_free (path);
  g_object_unref (settings);

  if (is_default)
    g_settings_set_string (global_settings, TERMINAL_SETTING_DEFAULT_PROFILE_KEY, str);

  return g_strdup (str);
}

static gboolean
migrate_profiles (GError **error)
{
  GSettings *global_settings;
  GConfClient *client;
  GConfValue *value, *dvalue;
  GSList *l;
  GPtrArray *profile_uuids;
  const char *profile, *default_profile;

  global_settings = g_settings_new (TERMINAL_SETTING_SCHEMA);
  client = gconf_client_get_default ();

  profile_uuids = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

  dvalue = gconf_client_get_without_default (client, GCONF_GLOBAL_PREFIX "/default_profile", NULL);
  if (dvalue != NULL &&
      dvalue->type == GCONF_VALUE_STRING)
    default_profile = gconf_value_get_string (dvalue);
  else
    default_profile = NULL;

  value = gconf_client_get (client, GCONF_GLOBAL_PREFIX "/profile_list", NULL);
  if (value != NULL &&
      value->type == GCONF_VALUE_LIST &&
      gconf_value_get_list_type (value) == GCONF_VALUE_STRING) {
    for (l = gconf_value_get_list (value); l != NULL; l = l->next) {
      profile = gconf_value_get_string (l->data);

      g_ptr_array_add (profile_uuids,
                       migrate_profile (client, 
                                        global_settings,
                                        profile, 
                                        g_strcmp0 (profile, default_profile) == 0));
    }
  }

  /* Some settings used to be per-profile but are now global;
   * take these from the default profile.
   */
  if (default_profile) {
    char *path;

    path = gconf_concat_dir_and_key (GCONF_PROFILES_PREFIX, default_profile);

    migrate_bool (client, path, KEY_DEFAULT_SHOW_MENUBAR,
                  global_settings, TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY,
                  FALSE);

    g_free (path);
  }

  /* Only write profile list if there were any profiles migrated */
  if (profile_uuids->len) {
    g_ptr_array_add (profile_uuids, NULL);
    g_settings_set_strv (global_settings, TERMINAL_SETTING_PROFILES_KEY,
                         (const char * const *) profile_uuids->pdata);
  }

  if (value)
    gconf_value_free (value);
  if (dvalue)
    gconf_value_free (dvalue);
  g_object_unref (client);
  g_object_unref (global_settings);
  g_ptr_array_free (profile_uuids, TRUE);

  return TRUE;
}

static gboolean
migrate_accels (GError **error)
{
  static const struct { const char *key; const char *path; } data[] = {
    { "new_tab", "FileNewTab" },
    { "new_window", "FileNewWindow" },
    { "new_profile", "FileNewProfile" },
    { "close_tab", "FileCloseTab" },
    { "close_window", "FileCloseWindow"},
    { "copy", "EditCopy" },
    { "paste", "EditPaste" },
    { "toggle_menubar", "ViewMenubar" },
    { "full_screen", "ViewFullscreen" },
    { "zoom_in", "ViewZoomIn" },
    { "zoom_out", "ViewZoomOut" },
    { "zoom_normal", "ViewZoom100" },
    { "set_window_title", "TerminalSetTitle" },
    { "reset", "TerminalReset" },
    { "reset_and_clear", "TerminalResetClear" },
    { "prev_tab", "TabsPrevious" },
    { "next_tab", "TabsNext" },
    { "move_tab_left", "TabsMoveLeft" },
    { "move_tab_right", "TabsMoveRight" },
    { "detach_tab", "TabsDetach" },
    { "switch_to_tab_1", "TabsSwitch1" },
    { "switch_to_tab_2", "TabsSwitch2" },
    { "switch_to_tab_3", "TabsSwitch3" },
    { "switch_to_tab_4", "TabsSwitch4" },
    { "switch_to_tab_5", "TabsSwitch5" },
    { "switch_to_tab_6", "TabsSwitch6" },
    { "switch_to_tab_7", "TabsSwitch7" },
    { "switch_to_tab_8", "TabsSwitch8" },
    { "switch_to_tab_9", "TabsSwitch9" },
    { "switch_to_tab_10", "TabsSwitch10" },
    { "switch_to_tab_11", "TabsSwitch11" },
    { "switch_to_tab_12", "TabsSwitch12" },
    { "help", "HelpContents" }
  };
  GConfClient *client;
  guint i;
  char *key, *path;
  GConfValue *value;
  GString *str;

  client = gconf_client_get_default ();
  str = g_string_sized_new (1024);

  for (i = 0; i < G_N_ELEMENTS (data); ++i) {
    key = g_strdup_printf ("/apps/gnome-terminal/keybindings/%s", data[i].key);
    path = g_strdup_printf ("<Actions>/Main/%s", data[i].path);

    if ((value = gconf_client_get_without_default (client, key, NULL)) != NULL &&
        value->type == GCONF_VALUE_STRING)
      g_string_append_printf (str,
                              "(gtk_accel_path \"%s\" \"%s\")\n",
                              path, gconf_value_get_string (value));

    g_free (key);
    g_free (path);
    if (value)
      gconf_value_free (value);
  }

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-terminal",
                           NULL);
  g_mkdir_with_parents (path, 0700);
  g_free (path);

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-terminal",
                           "accels",
                           NULL);
  g_file_set_contents (path, str->str, str->len, NULL);
  g_free (path);

  g_string_free (str, TRUE);
  g_object_unref (client);

  return TRUE;
}

static gboolean
migrate (GError **error)
{
  return migrate_global_prefs (error) &&
         migrate_profiles (error) &&
         migrate_accels (error);
}

static void
update_schema_version (void)
{
  GSettings *settings;

  if (verbose)
    g_printerr ("Updating schema version\n");

  settings = g_settings_new (TERMINAL_SETTING_SCHEMA);
  g_settings_set_uint (settings, TERMINAL_SETTING_SCHEMA_VERSION, TERMINAL_SCHEMA_VERSION);
  g_object_unref (settings);
}

int
main (int argc,
      char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;

  setlocale (LC_ALL, "");

#if !GLIB_CHECK_VERSION (2, 35, 3)
  g_type_init ();
#endif

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_option_context_free (context);
    g_printerr ("Error parsing arguments: %s\n", error->message);
    g_error_free (error);
    return EXIT_FAILURE;
  }
  g_option_context_free (context);

  if (!migrate (&error)) {
    g_printerr ("Error: %s\n", error->message);
    g_error_free (error);
    return EXIT_FAILURE;
  }

  update_schema_version ();

  if (verbose)
    g_printerr ("Syncing gsettings...\n");

  g_settings_sync ();

  if (verbose)
    g_printerr ("Migration successful!\n");

  return EXIT_SUCCESS;
}
