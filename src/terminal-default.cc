/*
 * Copyright © 2008, 2011, 2025, 2026 Christian Persch
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

#include <fcntl.h>

#include "terminal-default.hh"

#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"

#define XTE_CONFIG_DIRNAME  "xdg-terminals"
#define XTE_CONFIG_FILENAME "xdg-terminals.list"

#define XIA_CONFIG_FILENAME "intentapps.list"
#define XIA_DEFAULT_APPLICATIONS_GROUP "Default Applications"
#define XIA_INTENT_TERMINAL1 "org.freedesktop.Terminal1"

// glib forward-compat
#ifndef G_KEY_FILE_DESKTOP_KEY_IMPLEMENTS
#define G_KEY_FILE_DESKTOP_KEY_IMPLEMENTS "Implements"
#endif

#define NEWLINE '\n'
#define DOT_DESKTOP ".desktop"
#define TERMINAL_DESKTOP_FILENAME TERMINAL_APPLICATION_ID DOT_DESKTOP

static bool
validate_desktop(char const* desktop)
{
  if (!desktop|| !desktop[0])
    return false;

  for (auto i = size_t{0}; desktop[i]; ++i) {
    if (desktop[i] != '-' && desktop[i] != '_' &&
        !g_ascii_isalnum (desktop[i]))
      return false;
  }

  return true;
}

static char**
validate_and_lc_desktops(char const* const* strv)
{
  if (!strv)
    return nullptr;

  auto builder = g_ptr_array_new();

  for (auto i = size_t{0}; strv[i]; ++i) {
    if (!validate_desktop(strv[i]))
      continue;

    g_ptr_array_add(builder, g_ascii_strdown(strv[i], -1));
  }

  g_ptr_array_add(builder, nullptr);
  return reinterpret_cast<char**>(g_ptr_array_free(builder, false));
}

static char**
get_desktops_lc(void)
{
  auto const desktop = g_getenv("XDG_CURRENT_DESKTOP");
  if (!desktop)
    return nullptr;

  gs_strfreev auto raw_strv = g_strsplit(desktop, G_SEARCHPATH_SEPARATOR_S, -1);
  if (!raw_strv)
    return nullptr;

  return validate_and_lc_desktops(raw_strv);
}

static bool
desktop_name_is_foreign(char const* name,
                        char const* native_name)
{
  return !g_str_equal(name, native_name);
}

typedef enum {
  CHECK_XIA = 1 << 0,
  CHECK_XTE = 1 << 1,
  CHECK_FULL = 1 << 2,
} CheckFlags;

static bool
desktop_data_check_one(char const* file,
                       CheckFlags flags)
{
  if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Desktop file \"%s\" does not exist.\n",
                          file);
    return false;
  }

  if ((flags & CHECK_FULL) == 0)
    return true;

  gs_free_error GError* error = nullptr;
  gs_unref_key_file auto keyfile = g_key_file_new();
  if (!g_key_file_load_from_file(keyfile,
                                 file,
                                 GKeyFileFlags(G_KEY_FILE_NONE),
                                 &error)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to load  \"%s\" as keyfile: %s\n",
                          file, error->message);

    return false;
  }

  if (!g_key_file_has_group(keyfile, G_KEY_FILE_DESKTOP_GROUP)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Keyfile file \"%s\" is not a desktop file.\n",
                          file);
    return false;
  }

  if (gs_free auto type = g_key_file_get_string(keyfile,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_TYPE,
                                                nullptr);
      !type ||
      !g_str_equal(type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Keyfile file \"%s\" has incorrec type \"%s\".\n",
                          file, type ? type : "(nil)");
    return false;
  }

  if (!g_key_file_has_key(keyfile,
                          G_KEY_FILE_DESKTOP_GROUP,
                          G_KEY_FILE_DESKTOP_KEY_NAME,
                          nullptr)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Keyfile file \"%s\" is missing the Name entry.\n",
                          file);
    return false;
  }

  // As per the XDG desktop entry spec, the (optional) TryExec key contains
  // the name of an executable that can be used to determine if the programme
  // is actually present.
  gs_free auto try_exec = g_key_file_get_string(keyfile,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_TRY_EXEC,
                                                nullptr);
  if (try_exec && try_exec[0]) {
    // TryExec may be an abolute path, or be searched in $PATH
    gs_free char* exec_path = nullptr;
    if (g_path_is_absolute(try_exec))
      exec_path = g_strdup(try_exec);
    else
      exec_path = g_find_program_in_path(try_exec);

    auto const exists = exec_path != nullptr &&
      g_file_test(exec_path, GFileTest(G_FILE_TEST_IS_EXECUTABLE));

    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Desktop file \"%s\" is %sinstalled (TryExec).\n",
                          file, exists ? "" : "not ");

    if (!exists)
      return false;
  } else {
    // TryExec is not present. We could fall back to parsing the Exec
    // key and look if its first argument points to an executable that
    // exists on the system, but that may also fail if the desktop file
    // is DBusActivatable=true in which case we would need to find
    // out if the D-Bus service corresponding to the name of the desktop
    // file (without the .desktop extension) is activatable.

    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Desktop file \"%s\" has no TryExec field.\n",
                          file);
  }

  if (flags & CHECK_XIA) {
    if (gs_strfreev auto interfaces = g_key_file_get_string_list(keyfile,
                                                                 G_KEY_FILE_DESKTOP_GROUP,
                                                                 G_KEY_FILE_DESKTOP_KEY_IMPLEMENTS,
                                                                 nullptr,
                                                                 nullptr);
        !interfaces ||
        !g_strv_contains(interfaces, XIA_INTENT_TERMINAL1)) {
      _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                            "Keyfile file \"%s\" does not implement intent \"%s\".\n",
                            file, XIA_INTENT_TERMINAL1);
      return false;
    }
  }

  return true;
}

static bool
desktop_data_check(char const* name,
                   CheckFlags flags)
{
  gs_strfreev auto segments = g_strsplit(name, ":", 2);

  auto const desktop_id = segments[0];
  if (!desktop_id || !desktop_id[0])
    return false;

  auto const include_xte_dirs = (flags & CHECK_XTE) != 0;

  if (include_xte_dirs) { // Legacy x-t-e spec
    gs_free auto path = g_build_filename(g_get_user_data_dir(),
                                         XTE_CONFIG_DIRNAME,
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  {
    gs_free auto path = g_build_filename(g_get_user_data_dir(),
                                         "applications",
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  {
    gs_free auto path = g_build_filename(g_get_user_data_dir(),
                                         "flatpak",
                                         "exports",
                                         "share",
                                         "applications",
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  if (include_xte_dirs && !g_str_equal(TERM_PREFIX, "/usr/local")) {
    gs_free auto path = g_build_filename(TERM_PREFIX,
                                         "local",
                                         "share",
                                         XTE_CONFIG_DIRNAME,
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  if (!g_str_equal(TERM_PREFIX, "/usr/local")) {
    gs_free auto path2 = g_build_filename(TERM_PREFIX,
                                          "local",
                                          "share",
                                          "applications",
                                          desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path2, flags))
      return true;
  }
  if (include_xte_dirs) {
    gs_free auto path = g_build_filename(TERM_DATADIR,
                                         XTE_CONFIG_DIRNAME,
                                         name,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  {
    gs_free auto path = g_build_filename(TERM_DATADIR,
                                         "applications",
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  {
    gs_free auto path = g_build_filename("/var",
                                         "lib",
                                         "flatpak",
                                         "exports",
                                         "share",
                                         "applications",
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  if (include_xte_dirs && !g_str_equal(TERM_PREFIX, "/usr")) {
    gs_free auto path = g_build_filename("/usr", "share",
                                         XTE_CONFIG_DIRNAME,
                                         desktop_id,
                                         nullptr);
    if (desktop_data_check_one(path, flags))
      return true;
  }
  if (!g_str_equal(TERM_PREFIX, "/usr")) {
    gs_free auto path2 = g_build_filename("/usr",
                                          "share",
                                          "applications",
                                          desktop_id,
                                          nullptr);
    if (desktop_data_check_one(path2, flags))
      return true;
  }

  return false;
}

// XDG Terminal Exec

static bool
xte_data_ensure(char const* desktop_id)
{
  if (desktop_data_check(desktop_id, CHECK_XTE))
    return true;

  // If we get here, there wasn't a desktop file in any of the paths. Install
  // a symlink to the system-installed desktop file into the user path.

  gs_free auto user_dir = g_build_filename(g_get_user_data_dir(),
                                           XTE_CONFIG_DIRNAME,
                                           nullptr);
  if (g_mkdir_with_parents(user_dir, 0700) != 0 &&
      errno != EEXIST) {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to create directory %s: %s\n",
                          user_dir, g_strerror(errsv));
    return false;
  }

  gs_free auto link_path = g_build_filename(user_dir,
                                            desktop_id,
                                            nullptr);
  gs_free auto target_path = g_build_filename(TERM_DATADIR,
                                              "applications",
                                              desktop_id,
                                              nullptr);

  auto const r = symlink(target_path, link_path);
  if (r != -1) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Installed symlink %s -> %s\n",
                          link_path, target_path);

  } else {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to create symlink %s: %s\n",
                          link_path, g_strerror(errsv));
  }

  return r != -1;
}

static char**
xte_config_read(char const* path,
                GError** error)
{
  _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                        "Reading x-t-e config file \"%s\"\n",
                        path);

  gs_close_fd auto fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd == -1)
    return nullptr;

  // This is a small config file, so shouldn't be any bigger than this.
  // If it is bigger, we'll discard the rest. That's why we're not using
  // g_file_get_contents() here.
  char buf[8192];
  auto r = ssize_t{};
  do {
    r = read(fd, buf, sizeof(buf) - 1); // reserve one byte in buf
  } while (r == -1 && errno == EINTR);
  if (r < 0)
    return nullptr;

  buf[r] = '\0'; // NUL terminator; note that r < sizeof(buf)

  auto lines = g_strsplit_set(buf, "\r\n", -1);
  if (!lines)
    return nullptr;

  for (auto i = size_t{0}; lines[i]; ++i)
    lines[i] = g_strstrip(lines[i]);

  return lines;
}

static bool
xte_config_rewrite(char const* path,
                   char const* desktop_id)
{
  gs_free_gstring auto str = g_string_sized_new(1024);
  g_string_append(str, desktop_id);
  g_string_append_c(str, NEWLINE);

  gs_strfreev auto lines = xte_config_read(path, nullptr);
  if (lines) {
    for (auto i = size_t{0}; lines[i]; ++i) {
      if (lines[i][0] == '\0')
        continue;
      if (strcmp(lines[i], desktop_id) == 0)
        continue;
      if (auto colon = strchr(lines[i], ':');
          colon &&
          strncmp(lines[i], desktop_id, colon - lines[i]) == 0)
          continue;

      g_string_append(str, lines[i]);
      g_string_append_c(str, NEWLINE);
    }
  }

  gs_free_error GError* error = nullptr;
  auto const r = g_file_set_contents(path, str->str, str->len, &error);
  if (!r) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to rewrite XTE config %s: %s\n",
                          path, error->message);
  }

  return r;
}

static void
xte_config_rewrite(char const* desktop_id)
{
  auto const user_dir = g_get_user_config_dir();
  if (g_mkdir_with_parents(user_dir, 0700) != 0 &&
      errno != EEXIST) {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to user config create directory %s: %s\n",
                          user_dir, g_strerror(errsv));
   // Nothing to do if we can't even create the directory
    return;
  }

  // Install as default for all current desktops
  gs_strfreev auto desktops = get_desktops_lc();
  if (desktops) {
    for (auto i = size_t{0}; desktops[i]; ++i) {
      gs_free auto name = g_strdup_printf("%s-" XTE_CONFIG_FILENAME,
                                          desktops[i]);
      gs_free auto path = g_build_filename(user_dir, name, nullptr);

      xte_config_rewrite(path, desktop_id);
    }
  }

  // Install as non-desktop specific default too
  gs_free auto path = g_build_filename(user_dir, XTE_CONFIG_FILENAME, nullptr);
  xte_config_rewrite(path, desktop_id);
}

static char*
xte_config_get_default_for_path(char const* path,
                                char const* native_name)
{
  gs_strfreev auto lines = xte_config_read(path, nullptr);
  if (!lines)
    return nullptr;

  // A terminal is the default if it's the first non-comment line in the file
  for (auto i = size_t{0}; lines[i]; ++i) {
    auto const line = lines[i];
    if (!line[0] || line[0] == '#')
      continue;

    // If a foreign terminal is default, check whether it is actually installed.
    // (We always ensure our own desktop file exists.)
    if (desktop_name_is_foreign(line, native_name) &&
        !desktop_data_check(line, CheckFlags(CHECK_XTE | CHECK_FULL))) {
      _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                            "Default XTE entry \"%s\" from config \"%s\" is not installed, skipping.\n",
                            line, path);
      return nullptr;
    }

    return g_strdup(line);
  }

  return nullptr;
}

static char*
xte_config_get_default_for_path_and_desktops(char const* base_path,
                                             char const* const* desktops,
                                             char const* native_name)
{
  if (desktops) {
    for (auto i = size_t{0}; desktops[i]; ++i) {
      gs_free auto name = g_strdup_printf("%s-" XTE_CONFIG_FILENAME,
                                          desktops[i]);
      gs_free auto path = g_build_filename(base_path, name, nullptr);
      if (auto term = xte_config_get_default_for_path(path, native_name))
        return term;
    }
  }

  gs_free auto sys_path = g_build_filename(base_path, XTE_CONFIG_FILENAME, nullptr);
  if (auto term = xte_config_get_default_for_path(sys_path, native_name))
    return term;

  return nullptr;
}

static char*
xte_config_get_default(char const* native_name)
{
  gs_strfreev auto desktops = get_desktops_lc();
  auto const user_dir = g_get_user_config_dir();
  if (auto term = xte_config_get_default_for_path_and_desktops(user_dir, desktops, native_name))
    return term;
  if (auto term = xte_config_get_default_for_path_and_desktops("/etc/xdg", desktops, native_name))
    return term;
  if (auto term = xte_config_get_default_for_path_and_desktops("/usr/etc/xdg", desktops, native_name))
    return term;

  auto data_dirs = g_get_system_data_dirs();
  for (auto i = gsize{0}; data_dirs[i]; ++i) {
    gs_free auto path = g_build_filename(data_dirs[i], "xdg-terminal-exec", nullptr);
    if (auto term = xte_config_get_default_for_path_and_desktops(path, desktops, native_name))
      return term;
  }

  return nullptr;
}

static bool
xte_config_is_default(char const* desktop_id,
                      bool* set = nullptr)
{
  gs_free auto term = xte_config_get_default(desktop_id);

  auto const is_default = term && g_str_equal(term, desktop_id);
  if (set)
    *set = term != nullptr;
  return is_default;
}

static bool
xte_config_set_default(char const* desktop_id = TERMINAL_DESKTOP_FILENAME)
{
  xte_config_rewrite(desktop_id);
  xte_data_ensure(desktop_id);

  return xte_config_is_default(desktop_id);
}

// XDG Intent Apps
// https://specifications.freedesktop.org/intent-apps/latest/

static bool
xia_config_set_default_for_path(char const* path,
                                char const* desktop_id)
{
  _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                        "Reading XIA config file \"%s\"\n",
                        path);

  gs_unref_key_file auto keyfile = g_key_file_new();

  if (gs_free_error GError* err = nullptr;
      !g_key_file_load_from_file(keyfile,
                                 path,
                                 GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS),
                                 &err) &&
      !g_error_matches(err, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to read XIA config \"%s\": %s\n",
                          path, err->message);

    // Ignore error, just clear the file
    g_key_file_unref(keyfile);
    keyfile = g_key_file_new();
  }

  auto arr = g_ptr_array_new();
  g_ptr_array_add(arr, g_strdup(desktop_id));

  if (gs_strfreev auto apps = g_key_file_get_string_list(keyfile,
                                                         XIA_DEFAULT_APPLICATIONS_GROUP,
                                                         XIA_INTENT_TERMINAL1,
                                                         nullptr,
                                                         nullptr)) {
    for (auto i = size_t{0}; apps[i]; ++i) {
      auto const app = apps[i];
      if (g_str_equal(app, desktop_id))
        continue;

      g_ptr_array_add(arr, g_strdup(app));
    }
  }

  auto const n = arr->len;

  g_ptr_array_add(arr, nullptr);
  gs_strfreev auto strv = reinterpret_cast<char**>(g_ptr_array_free(arr, false));

  g_key_file_set_string_list(keyfile,
                             XIA_DEFAULT_APPLICATIONS_GROUP,
                             XIA_INTENT_TERMINAL1,
                             strv, n);

  if (gs_free_error GError* err = nullptr;
      !g_key_file_save_to_file(keyfile, path, &err)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to write XIA config \"%s\": %s\n",
                          path, err->message);
    return false;
  }

  return true;
}

static void
xia_config_set_default(char const* desktop_id = TERMINAL_DESKTOP_FILENAME)
{
  auto const user_dir = g_get_user_config_dir();
  if (g_mkdir_with_parents(user_dir, 0700) != 0 &&
      errno != EEXIST) {
    auto const errsv = errno;
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Failed to create user config directory %s: %s\n",
                          user_dir, g_strerror(errsv));
   // Nothing to do if we can't even create the directory
    return;
  }

  // Install as default for all current desktops
  gs_strfreev auto desktops = get_desktops_lc();
  if (desktops) {
    for (auto i = size_t{0}; desktops[i]; ++i) {
      gs_free auto name = g_strdup_printf("%s-" XIA_CONFIG_FILENAME,
                                          desktops[i]);
      gs_free auto path = g_build_filename(user_dir, name, nullptr);

      xia_config_set_default_for_path(path, desktop_id);
    }
  }

  // Install as non-desktop specific default too
  gs_free auto path = g_build_filename(user_dir, XIA_CONFIG_FILENAME, nullptr);
  xia_config_set_default_for_path(path, desktop_id);
}

static char*
xia_config_get_default_for_path(char const* path,
                                char const* native_name)
{
  _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                        "Reading XIA config file \"%s\"\n",
                        path);

  gs_unref_key_file auto keyfile = g_key_file_new();

  if (gs_free_error GError* err = nullptr;
      !g_key_file_load_from_file(keyfile,
                                 path,
                                 GKeyFileFlags(G_KEY_FILE_NONE),
                                 &err)) {
    if (!g_error_matches(err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                            "Failed to load XIA config file \"%s\": %s\n",
                            path, err->message);
    return nullptr;
  }

  if (!g_key_file_has_group(keyfile, XIA_DEFAULT_APPLICATIONS_GROUP)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "\"%s\" is missing the \"%s\" group and so is not a valid XIA config file\n",
                          path, XIA_DEFAULT_APPLICATIONS_GROUP);
    return nullptr;
  }

  if (!g_key_file_has_key(keyfile,
                          XIA_DEFAULT_APPLICATIONS_GROUP,
                          XIA_INTENT_TERMINAL1,
                          nullptr)) {
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "\"%s\" has no key \"%s\"\n",
                          path, XIA_INTENT_TERMINAL1);
    return nullptr;
  }

  gs_strfreev auto apps = g_key_file_get_string_list(keyfile,
                                                     XIA_DEFAULT_APPLICATIONS_GROUP,
                                                     XIA_INTENT_TERMINAL1,
                                                     nullptr,
                                                     nullptr);
  if (!apps)
    return nullptr;

  for (auto i = size_t{0}; apps[i]; ++i) {
    auto const app = apps[i];

    // If a foreign terminal is default, check whether it is actually installed
    // and implements the terminal intent.
    // (Our own desktop file exists and implements the terminal intent.)
    if (desktop_name_is_foreign(app, native_name) &&
        !desktop_data_check(app, CheckFlags(CHECK_XTE | CHECK_FULL))) {
      _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                            "Default XIA entry \"%s\" from config \"%s\" is not installed or not intent, skipping.\n",
                            app, path);
      continue;
    }

    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "Returning XIA default \"%s\" from config \"%s\".\n",
                          app, path);
    return g_strdup(app);
  }

  return nullptr;
}

static char*
xia_config_get_default_for_path_and_desktops(char const* base_path,
                                             char const* const* desktops,
                                             char const* native_name)
{
  if (desktops) {
    for (auto i = size_t{0}; desktops[i]; ++i) {
      gs_free auto name = g_strdup_printf("%s-" XIA_CONFIG_FILENAME,
                                          desktops[i]);
      gs_free auto path = g_build_filename(base_path, name, nullptr);
      if (auto term = xia_config_get_default_for_path(path, native_name))
        return term;
    }
  }

  gs_free auto sys_path = g_build_filename(base_path, XIA_CONFIG_FILENAME, nullptr);
  if (auto term = xia_config_get_default_for_path(sys_path, native_name))
    return term;

  return nullptr;
}

static char*
xia_config_get_default(char const* native_name)
{
  gs_strfreev auto desktops = get_desktops_lc();
  auto const user_dir = g_get_user_config_dir();
  if (auto term = xia_config_get_default_for_path_and_desktops(user_dir, desktops, native_name))
    return term;

  auto config_dirs = g_get_system_config_dirs();
  for (auto i = size_t{0}; config_dirs[i]; ++i) {
    if (auto term = xia_config_get_default_for_path_and_desktops(config_dirs[i], desktops, native_name))
      return term;
  }

  auto data_dirs = g_get_system_data_dirs();
  for (auto i = size_t{0}; data_dirs[i]; ++i) {
    if (auto term = xia_config_get_default_for_path_and_desktops(data_dirs[i], desktops, native_name))
      return term;
  }

  return nullptr;
}

static bool
xia_config_is_default(char const* desktop_id,
                      bool* set = nullptr)
{
  gs_free auto term = xia_config_get_default(desktop_id);

  auto const is_default = term && g_str_equal(term, desktop_id);
  if (set)
    *set = term != nullptr;
  return is_default;
}

// public API

gboolean
terminal_is_default(void)
{
  auto const desktop_id = TERMINAL_DESKTOP_FILENAME;

  gs_free auto xia_default = xia_config_get_default(desktop_id);
  gs_free auto xte_default = xte_config_get_default(desktop_id);

  auto const xia_default_set = xia_default != nullptr;
  auto const xte_default_set = xte_default != nullptr;

  auto xia_is_default = xia_default && g_str_equal(xia_default, desktop_id);
  auto xte_is_default = xte_default && g_str_equal(xte_default, desktop_id);

  // If one of XTE/XIA is not set but the other is set to gnome-terminal,
  // copy that pref over.
  if (xte_is_default && !xia_default_set) {
    // No XIA default yet, claim XIA default
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "No XIA default terminal, claiming default.\n");
    xia_config_set_default(desktop_id);
    xia_is_default = xia_config_is_default(desktop_id);
  }
  if (xia_is_default && !xte_default_set) {
    // No XTE default yet, claim XTE default
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "No XTE default terminal, claiming default.\n");
    xte_is_default = xte_config_set_default(desktop_id);
  } else if (!xia_default_set && !xte_default_set) {
    // neither has a pref set yet, claim both
    _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                          "No XIA or XTE default terminal, claiming default.\n");
    xia_config_set_default(desktop_id);
    xia_is_default = xia_config_is_default(desktop_id);

    xte_is_default = xte_config_set_default(desktop_id);
  }

  if (xte_is_default) {
    // If we're the default terminal, ensure our desktop file is installed
    // in the right location.
    xte_data_ensure(desktop_id);
  }

  _terminal_debug_print(TERMINAL_DEBUG_DEFAULT,
                        "XIA is-default %d, XTE is-default %d\n",
                        xia_is_default, xte_is_default);

  return xte_is_default && xia_is_default;
}

void
terminal_make_default(void)
{
  auto const desktop_id = TERMINAL_DESKTOP_FILENAME;

  xte_config_set_default(desktop_id);
  xia_config_set_default(desktop_id);
}

#ifdef TERMINAL_DEFAULT_MAIN

#include <clocale>

static int
usage(char const* argv0) noexcept
{
  g_printerr("Usage: %s [--debug] [get|set] [DESKTOP_ID]\n", argv0);
  return _EXIT_FAILURE_ARGPARSE;
}

static void
show_default(char const* desktop_id,
             bool xia,
             bool xte)
{
  if (xia) {
    gs_free auto xia_term = xia_config_get_default(desktop_id);
    if (xia_term) {
      g_printerr("Default XIA terminal: %s\n", xia_term);
    } else {
      g_printerr("Default XIA terminal not set\n");
    }
  }
  if (xte) {
    gs_free auto xte_term = xte_config_get_default(desktop_id);
    if (xte_term) {
      g_printerr("Default XTE terminal: %s\n", xte_term);
    } else {
      g_printerr("Default XTE terminal not set\n");
    }
  }
}

static void
set_default(char const* desktop_id,
            bool xia,
            bool xte)
{
  if (xia) {
    g_printerr("Setting %s as default XIA terminal...\n", desktop_id);
    xia_config_set_default(desktop_id);
  }
  if (xte) {
    g_printerr("Setting %s as default XIA terminal...\n", desktop_id);
    xte_config_set_default(desktop_id);
  }

  show_default(desktop_id, xia, xte);
}

static gboolean arg_debug = false;
static gboolean arg_xia = false;
static gboolean arg_xte = false;

static const GOptionEntry goptions[] = {
  {"debug", 0, 0, G_OPTION_ARG_NONE, &arg_debug, "Debug output", nullptr},
  {"xia", 0, 0, G_OPTION_ARG_NONE, &arg_xia, "XDG intent applications", nullptr},
  {"xte", 0, 0, G_OPTION_ARG_NONE, &arg_xia, "XDG terminal exec", nullptr},
  {nullptr}
};

int
main(int argc,
     char* argv[])
{
  if (setlocale(LC_ALL, "") == nullptr) {
    g_printerr("Locale not supported.\n");
    return _EXIT_FAILURE_UNSUPPORTED_LOCALE;
  }

  char const* charset = nullptr;
  if (!g_get_charset(&charset)) {
    g_printerr("Non UTF-8 locale (%s) is not supported!\n", charset);
    return _EXIT_FAILURE_NO_UTF8;
  }

  gs_free_option_context auto context = g_option_context_new("[get|set]");
  g_option_context_set_ignore_unknown_options(context, true);
  g_option_context_add_main_entries(context, goptions, GETTEXT_PACKAGE);

  gs_free_error GError* error = nullptr;
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_printerr("Failed to parse arguments: %s\n", error ? error->message : "");
    return usage(argv[0]);
  }

  if (arg_debug) {
    g_setenv("GNOME_TERMINAL_DEBUG", "default", true);
    _terminal_debug_init();
  }

  if (argc < 1 || argc > 3)
    return usage(argv[0]);

  auto const verb = argc > 1 ? argv[1] : nullptr;
  auto const desktop_id = argc > 2 ? argv[2] : TERMINAL_DESKTOP_FILENAME;

  auto const use_xia = !arg_xte || arg_xia;
  auto const use_xte = !arg_xia || arg_xte;

  if (!verb || g_str_equal(verb, "get")) {
    show_default(desktop_id, use_xia, use_xte);
  } else if (g_str_equal(verb, "set")) {
    set_default(desktop_id, use_xia, use_xte);
  } else {
    return usage(argv[0]);
  }

  return 0;
}

#endif // TERMINAL_DEFAULT_MAIN
