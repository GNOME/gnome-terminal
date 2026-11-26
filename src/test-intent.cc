/*
 * Copyright © 2008, 2010, 2011, 2021, 2022, 2026 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
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

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"
#include "terminal-gdbus-generated.h"

static char* arg_app_id = nullptr;
static char* arg_desktop_file = nullptr;
static char* arg_profile_uuid = nullptr;
static gboolean arg_keep = false;
static gboolean arg_no_cmd = false;
static gboolean arg_no_cwd = false;
static gboolean arg_no_env = false;

static const GOptionEntry goptions[] = {
  {"app-id", 0, 0, G_OPTION_ARG_STRING, &arg_app_id, "Application ID", "ID"},
  {"desktop-file", 0, 0, G_OPTION_ARG_FILENAME, &arg_desktop_file, "Desktop file", "FILE"},
  {"keep", 0, 0, G_OPTION_ARG_NONE, &arg_keep, "Keep terminal open after command exits", nullptr},
  {"no-cmd", 0, 0, G_OPTION_ARG_NONE, &arg_no_cmd, "Don't pass commands", nullptr},
  {"no-cwd", 0, 0, G_OPTION_ARG_NONE, &arg_no_cwd, "Don't pass working directory", nullptr},
  {"no-env", 0, 0, G_OPTION_ARG_NONE, &arg_no_env, "Don't pass environment", nullptr},
  {"profile", 0, 0, G_OPTION_ARG_STRING, &arg_profile_uuid, "Profile", "UUID"},
  {nullptr}
};

static char*
getenv_utf8(char const* env)
{
  auto const value = g_getenv(env);
  if (!value ||
      !value[0] ||
      !g_utf8_validate(value, -1, nullptr))
    return nullptr;

  return g_strdup(value);
}

static char**
strndupv(char const* const* strv,
         int n)
{
  auto arr = g_ptr_array_new();
  for (auto i = 0; i < n; ++i)
    g_ptr_array_add(arr, g_strdup(strv[i]));
  g_ptr_array_add(arr, nullptr);
  return reinterpret_cast<char**>(g_ptr_array_free(arr, false));
}

static char**
expand_desktop_argv(char const* const* eargv,
                    int eargc,
                    char const* const* args,
                    int argc)
{
  gs_unref_ptrarray auto arr = g_ptr_array_new_null_terminated(0, GDestroyNotify(g_free), true);
  for (auto i = 0; eargv[i]; ++i) {
    auto const arg = eargv[i];
    if (arg[0] == '%' && arg[1]) {
      switch (arg[1]) {
      case 'd': case 'D': case 'n': case 'N': case 'v': case 'm':
        // deprecated
        break;
      case 'i': case 'c': case 'k':
        // not supported here
        break;
      case 'f': case 'u':
        if (argc != 1)
          return nullptr;
        g_ptr_array_add(arr, g_strdup(args[0]));
        break;
      case 'F': case 'U':
        for (auto j = 0; j < argc; ++j)
          g_ptr_array_add(arr, g_strdup(args[j]));
        break;
      }
    } else
      g_ptr_array_add(arr, g_strdup(arg));
  }

  return reinterpret_cast<char**>(g_ptr_array_free(g_steal_pointer(&arr), false));
}

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

  // Find and save any args after a "--"
  gs_strfreev char **args = nullptr;
  auto n_args = 0;
  for (auto i = 0; i < argc; ++i) {
    if (!g_str_equal(argv[i], "--"))
      continue;

    if (i + 1 < argc) {
      n_args = argc - i - 1;
      args = strndupv(argv + i + 1, n_args);
    }

    argc = i;
    break;
  }

  gs_free_option_context auto context = g_option_context_new("[-- COMMAND]");
  g_option_context_set_ignore_unknown_options(context, false);
  g_option_context_add_main_entries(context, goptions, GETTEXT_PACKAGE);

  gs_free_error GError* error = nullptr;
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_printerr("Failed to parse arguments: %s\n", error ? error->message : "");
    return _EXIT_FAILURE_ARGPARSE;
  }

  if (arg_app_id && !g_application_id_is_valid(arg_app_id)) {
    g_printerr("\"%s\" is not a valid application ID\n", arg_app_id);
    return _EXIT_FAILURE_ARGPARSE;
  }

  if (arg_desktop_file) {
    gs_free char* desktop_entry_utf8 = g_utf8_make_valid(arg_desktop_file, -1);
    gs_unref_key_file GKeyFile* key_file = g_key_file_new();

    gs_free_error GError* err = nullptr;
    if (!g_key_file_load_from_file(key_file,
                                   arg_desktop_file,
                                   GKeyFileFlags(G_KEY_FILE_NONE),
                                   &err)) {
      g_printerr("Failed to load desktop file \"%s\": %s\n",
                 desktop_entry_utf8, err->message);
      return _EXIT_FAILURE_DESKTOP_FILE_PARSE;
    }

    gs_free auto version = g_key_file_get_string(key_file,
                                                 G_KEY_FILE_DESKTOP_GROUP,
                                                  G_KEY_FILE_DESKTOP_KEY_VERSION,
                                                 nullptr);
    gs_free auto type = g_key_file_get_string(key_file,
                                              G_KEY_FILE_DESKTOP_GROUP,
                                              G_KEY_FILE_DESKTOP_KEY_TYPE,
                                              nullptr);
    auto const is_terminal = g_key_file_get_boolean(key_file,
                                                    G_KEY_FILE_DESKTOP_GROUP,
                                                    G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                                    nullptr);
    if (!version ||
        !g_str_equal(version, "1.0") ||
        !type ||
        !g_str_equal(type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) ||
        !is_terminal) {
      g_printerr("\"%s\" is not a valid desktop file of a terminal application\n",
                 desktop_entry_utf8);
      return _EXIT_FAILURE_DESKTOP_FILE_PARSE;
    }

    gs_free auto exec = g_key_file_get_string(key_file,
                                              G_KEY_FILE_DESKTOP_GROUP,
                                              G_KEY_FILE_DESKTOP_KEY_EXEC,
                                              nullptr);
    if (!exec) {
      g_printerr("\"%s\" has no Exec key\n", desktop_entry_utf8);
      return _EXIT_FAILURE_DESKTOP_FILE_PARSE;
    }

    int exec_argc = 0;
    gs_strfreev char** exec_argv = nullptr;
    if (!g_shell_parse_argv(exec, &exec_argc, &exec_argv, &err)) {
      g_printerr("Failed to parse Exec key: %s\n", err->message);
      return _EXIT_FAILURE_DESKTOP_FILE_PARSE;
    }

    gs_free auto eargv = expand_desktop_argv(exec_argv, exec_argc,
                                             args, n_args);
    if (!eargv) {
      g_printerr("Failed to expand arguments\n");
      return _EXIT_FAILURE_DESKTOP_FILE_PARSE;
    }

    g_strfreev(args);
    args = g_steal_pointer(&eargv);
  }

  gs_free auto object_path =
    g_strdelimit(g_strdup_printf(".%s", arg_app_id ? arg_app_id : TERMINAL_APPLICATION_ID), ".", '/');

  gs_unref_object auto proxy =
    terminal_intent_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           arg_app_id ? arg_app_id : TERMINAL_APPLICATION_ID,
                                           object_path,
                                           nullptr, // cancellable
                                           &error);
  if (!proxy) {
    g_printerr("Error creating proxy for intent: %s\n", error->message);
    return EXIT_FAILURE;
  }

  auto aargv = GVariantBuilder{};
  g_variant_builder_init(&aargv, G_VARIANT_TYPE("aa{sv}"));
  if (!arg_no_cmd) {
    g_variant_builder_open(&aargv, G_VARIANT_TYPE("a{sv}"));
    if (args)
      g_variant_builder_add(&aargv, "{sv}", "exec", g_variant_new_bytestring_array(args, n_args));
    if (!arg_no_env) {
      gs_free auto envv = g_get_environ();
      g_variant_builder_add(&aargv, "{sv}", "env", g_variant_new_bytestring_array(envv, -1));
    }
    if (!arg_no_cwd) {
      gs_free auto cwd = g_get_current_dir();
      g_variant_builder_add(&aargv, "{sv}", "working-directory", g_variant_new_bytestring(cwd));
    }
    g_variant_builder_close(&aargv); // a{sv}
  }

  auto options = GVariantBuilder{};
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  if (arg_keep)
    g_variant_builder_add(&options, "{sv}", "keep-terminal-open", g_variant_new_boolean(true));
  if (arg_profile_uuid)
    g_variant_builder_add(&options, "{sv}", "x-gnome-terminal-profile-uuid", g_variant_new_string(arg_profile_uuid));

  auto platform_data = GVariantBuilder{};
  g_variant_builder_init(&platform_data, G_VARIANT_TYPE("a{sv}"));

  gs_free auto xdg_token = getenv_utf8("XDG_ACTIVATION_TOKEN");
  if (xdg_token)
    g_variant_builder_add(&platform_data, "{sv}", "activation-token", g_variant_new_string(xdg_token));
  gs_free auto startup_id = getenv_utf8("DESKTOP_STARTUP_ID");
  if (startup_id)
    g_variant_builder_add(&platform_data, "{sv}", "desktop-startup-id", g_variant_new_string(startup_id));

  gs_free char* desktop_entry = arg_desktop_file && arg_desktop_file[0]
    ? g_canonicalize_filename(arg_desktop_file, nullptr) : nullptr;
  if (!terminal_intent_call_launch_command_sync(TERMINAL_INTENT(proxy),
                                                g_variant_builder_end(&aargv),
                                                desktop_entry ? desktop_entry : "",
                                                g_variant_builder_end(&options),
                                                g_variant_builder_end(&platform_data),
                                                nullptr, // cancellable
                                                &error)) {
    g_printerr("Error calling LaunchCommand: %s\n", error->message);
    return EXIT_FAILURE;
  }

  g_printerr("Success!\n");
  return EXIT_SUCCESS;
}
