/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010, 2011, 2015, 2017, 2022 Christian Persch
 * Copyright © 2023 Christian Hergert
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <adwaita.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "terminal-intl.hh"
#include "terminal-debug.hh"
#include "terminal-default.hh"
#include "terminal-app.hh"
#include "terminal-accels.hh"
#include "terminal-client-utils.hh"
#include "terminal-profiles-list.hh"
#include "terminal-util.hh"
#include "terminal-schemas.hh"
#include "terminal-settings-utils.hh"
#include "terminal-defines.hh"
#include "terminal-libgsystem.hh"

#ifdef TERMINAL_SERVER
#include "terminal-gdbus.hh"
#include "terminal-prefs-process.hh"
#include "terminal-tab.hh"
#include "terminal-screen.hh"
#include "terminal-window.hh"

#include <adwaita.h>
#endif

#ifdef TERMINAL_PREFERENCES
#include "terminal-preferences-window.hh"
#endif

#ifndef TERMINAL_SERVER
#undef ENABLE_SEARCH_PROVIDER
#endif

#ifdef ENABLE_SEARCH_PROVIDER
#include "terminal-search-provider.hh"
#endif /* ENABLE_SEARCH_PROVIDER */

#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkx.h>
#endif

#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define GNOME_TERMINAL_PREFERENCES_ICON_NAME    "org.gnome.Terminal.Preferences"
#define GNOME_TERMINAL_PREFERENCES_DESKTOP_NAME "org.gnome.Terminal.Preferences.desktop"

#define DESKTOP_INTERFACE_SETTINGS_SCHEMA       "org.gnome.desktop.interface"

#define SYSTEM_PROXY_SETTINGS_SCHEMA            "org.gnome.system.proxy"
#define SYSTEM_HTTP_PROXY_SETTINGS_SCHEMA       "org.gnome.system.proxy.http"
#define SYSTEM_HTTPS_PROXY_SETTINGS_SCHEMA      "org.gnome.system.proxy.https"
#define SYSTEM_FTP_PROXY_SETTINGS_SCHEMA        "org.gnome.system.proxy.ftp"
#define SYSTEM_SOCKS_PROXY_SETTINGS_SCHEMA      "org.gnome.system.proxy.socks"

#define GTK_SETTING_PREFER_DARK_THEME           "gtk-application-prefer-dark-theme"

#define GTK_DEBUG_SETTING_SCHEMA                "org.gtk.gtk4.Settings.Debug"

#ifdef DISUNIFY_NEW_TERMINAL_SECTION
#error Use a gsettings override instead
#endif

enum {
  PROP_SETTINGS_BACKEND = 1,
  PROP_IS_DEFAULT_TERMINAL,
  PROP_ASK_DEFAULT_TERMINAL,
};

/*
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See gnome-terminal 1.x for why.
 */

struct _TerminalAppClass {
  AdwApplicationClass parent_class;

  void (* clipboard_targets_changed) (TerminalApp *app,
                                      GdkClipboard *clipboard);
};

struct _TerminalApp
{
  AdwApplication parent_instance;

  TerminalSettingsList *profiles_list;

  GSettingsBackend* settings_backend;
  GSettingsSchemaSource* schema_source;
  GSettings *global_settings;
  GSettings *desktop_interface_settings;
  GSettings *system_proxy_settings;
  GSettings* system_proxy_protocol_settings[4];
  GSettings *gtk_debug_settings;

#ifdef TERMINAL_SERVER
  GDBusObjectManagerServer *object_manager;
  GHashTable *screen_map;

#ifdef ENABLE_SEARCH_PROVIDER
  TerminalSearchProvider *search_provider;
#endif /* ENABLE_SEARCH_PROVIDER */

  GMenuModel *profilemenu;
  GMenuModel* headermenu;
  GMenu *headermenu_set_profile_section;

  GMenu *set_profile_menu;

  GdkClipboard *clipboard;
  GdkContentFormats *clipboard_targets;

  GWeakRef prefs_process_ref;

#endif /* TERMINAL_SERVER */

#ifdef TERMINAL_PREFERENCES
  GtkWindow* prefs_window;
#endif

  AdwStyleManager* style_manager;

  GIcon* default_icon;
  GIcon* default_icon_symbolic;

  gboolean ask_default;
  gboolean xte_is_default;
  gboolean unified_menu;
  gboolean use_headerbar;
};

enum
{
  CLIPBOARD_TARGETS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Debugging helper */

static void
terminal_app_init_debug (void)
{
#ifdef ENABLE_DEBUG
  const char *env = g_getenv ("GTK_TEXT_DIR");
  if (env != nullptr) {
    if (g_str_equal (env, "help")) {
      g_printerr ("Usage: GTK_TEXT_DIR=ltr|rtl\n");
    } else {
      GtkTextDirection dir;
      if (g_str_equal (env, "rtl"))
        dir = GTK_TEXT_DIR_RTL;
      else
        dir = GTK_TEXT_DIR_LTR;

      gtk_widget_set_default_direction (dir);
    }
  }

  env = g_getenv ("GTK_SETTINGS");
  if (env == nullptr)
    return;

  GObject *settings = G_OBJECT (gtk_settings_get_default ());
  GObjectClass *settings_class = G_OBJECT_GET_CLASS (settings);

  if (g_str_equal (env, "help")) {
    g_printerr ("Usage: GTK_SETTINGS=setting[,setting…] where 'setting' is one of these:\n");

    guint n_props;
    GParamSpec **props = g_object_class_list_properties (settings_class, &n_props);
    for (guint i = 0; i < n_props; i++) {
      if (G_PARAM_SPEC_VALUE_TYPE (props[i]) != G_TYPE_BOOLEAN)
        continue;

      GValue value = { 0, };
      g_value_init (&value, G_TYPE_BOOLEAN);
      g_object_get_property (settings, props[i]->name, &value);
      g_printerr ("  %s (%s)\n", props[i]->name, g_value_get_boolean (&value) ? "true" : "false");
      g_value_unset (&value);
    }
    g_printerr ("  Use 'setting' to set to true, "
                "'~setting' to set to false, "
                "and '!setting' to invert.\n");
  } else {
    gs_strfreev char **tokens = g_strsplit (env, ",", -1);
    for (guint i = 0; tokens[i] != nullptr; i++) {
      const char *prop = tokens[i];
      char c = prop[0];
      if (c == '~' || c == '!')
        prop++;

      GParamSpec *pspec = g_object_class_find_property (settings_class, prop);
      if (pspec == nullptr) {
        g_printerr ("Setting \"%s\" does not exist.\n", prop);
      } else if (G_PARAM_SPEC_VALUE_TYPE (pspec) != G_TYPE_BOOLEAN) {
        g_printerr ("Setting \"%s\" is not boolean.\n", prop);
      } else {
        GValue value = { 0, };
        g_value_init (&value, G_TYPE_BOOLEAN);
        if (c == '!') {
          g_object_get_property (settings, pspec->name, &value);
          g_value_set_boolean (&value, !g_value_get_boolean (&value));
        } else if (c == '~') {
          g_value_set_boolean (&value, FALSE);
        } else {
          g_value_set_boolean (&value, TRUE);
        }
        g_object_set_property (settings, pspec->name, &value);
        g_value_unset (&value);
      }
    }
  }
#endif
}

/* Helper functions */

static gboolean
strv_contains_gnome (char **strv)
{
  if (strv == nullptr)
    return FALSE;

  for (int i = 0; strv[i] != nullptr; i++) {
    if (g_ascii_strcasecmp (strv[i], "gnome") == 0 ||
        g_ascii_strcasecmp (strv[i], "gnome-classic") == 0)
      return TRUE;
  }

  return FALSE;
}

/*
 * terminal_app_should_use_headerbar:
 *
 * Determines if the app should use headerbars. This is determined
 * * If the pref is set, the pref value is used
 * * Otherwise, if XDG_CURRENT_DESKTOP contains GNOME or GNOME-Classic,
 *   headerbar is used
 * * Otherwise, headerbar is not used.
 */
static gboolean
terminal_app_should_use_headerbar (TerminalApp *app)
{
  gboolean set, use;
  g_settings_get (app->global_settings, TERMINAL_SETTING_HEADERBAR_KEY, "mb", &set, &use);
  if (set)
    return use;

  gs_strfreev auto desktops = terminal_util_get_desktops();
  return strv_contains_gnome(desktops);
}

static gboolean
load_css_from_resource (GApplication *application,
                        GtkCssProvider *provider,
                        gboolean theme)
{
  const char *base_path;
  gs_free char *uri;
  gs_unref_object GFile *file;

  base_path = g_application_get_resource_base_path (application);

  if (theme) {
    gs_free char *str, *theme_name;

    g_object_get (gtk_settings_get_default (), "gtk-theme-name", &str, nullptr);
    theme_name = g_ascii_strdown (str, -1);
    uri = g_strdup_printf ("resource://%s/css/%s/terminal.css", base_path, theme_name);
  } else {
    uri = g_strdup_printf ("resource://%s/css/terminal.css", base_path);
  }

  file = g_file_new_for_uri (uri);
  if (!g_file_query_exists (file, nullptr /* cancellable */))
    return FALSE;

  gtk_css_provider_load_from_file (provider, file);

  return TRUE;
}

static void
add_css_provider (GApplication *application,
                  gboolean theme)
{
  gs_unref_object GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  if (!load_css_from_resource (application, provider, theme))
    return;

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
app_load_css (GApplication *application)
{
  add_css_provider (application, FALSE);
  add_css_provider (application, TRUE);
}

char *
terminal_app_new_profile (TerminalApp *app,
                          GSettings   *base_profile,
                          const char  *name)
{
  char *uuid;

  if (base_profile) {
    gs_free char *base_uuid;

    base_uuid = terminal_settings_list_dup_uuid_from_child (app->profiles_list, base_profile);
    uuid = terminal_settings_list_clone_child (app->profiles_list, base_uuid, name);
  } else {
    uuid = terminal_settings_list_add_child (app->profiles_list, name);
  }

  return uuid;
}

void
terminal_app_remove_profile (TerminalApp *app,
                             GSettings *profile)
{
  g_return_if_fail (TERMINAL_IS_APP (app));
  g_return_if_fail (G_IS_SETTINGS (profile));

  gs_unref_object GSettings *default_profile = terminal_settings_list_ref_default_child (app->profiles_list);
  if (default_profile == profile)
    return;

#ifdef TERMINAL_SERVER
  /* First, we need to switch any screen using this profile to the default profile */
  gs_free_list GList *screens = g_hash_table_get_values (app->screen_map);
  for (GList *l = screens; l != nullptr; l = l->next) {
    TerminalScreen *screen = TERMINAL_SCREEN (l->data);
    if (terminal_screen_get_profile (screen) != profile)
      continue;

    terminal_screen_set_profile (screen, default_profile);
  }
#endif /* TERMINAL_SERVER */

  /* Now we can safely remove the profile */
  gs_free char *uuid = terminal_settings_list_dup_uuid_from_child (app->profiles_list, profile);
  terminal_settings_list_remove_child (app->profiles_list, uuid);
}

static void
terminal_app_theme_variant_changed_cb (GSettings   *settings,
                                       const char  *key,
                                       GtkSettings *gtk_settings)
{
  auto const theme = TerminalThemeVariant(g_settings_get_enum(settings, key));

  auto const app = terminal_app_get();
  if (adw_style_manager_get_system_supports_color_schemes(app->style_manager)) {
    switch (theme) {
    case TERMINAL_THEME_VARIANT_SYSTEM:
      adw_style_manager_set_color_scheme(app->style_manager,
                                         ADW_COLOR_SCHEME_PREFER_LIGHT);
      break;
    case TERMINAL_THEME_VARIANT_LIGHT:
      adw_style_manager_set_color_scheme(app->style_manager,
                                         ADW_COLOR_SCHEME_FORCE_LIGHT);
      break;
    case TERMINAL_THEME_VARIANT_DARK:
      adw_style_manager_set_color_scheme(app->style_manager,
                                         ADW_COLOR_SCHEME_FORCE_DARK);
    }
  } else {
    if (theme == TERMINAL_THEME_VARIANT_SYSTEM)
      gtk_settings_reset_property(gtk_settings, GTK_SETTING_PREFER_DARK_THEME);
    else
      g_object_set(gtk_settings,
                   GTK_SETTING_PREFER_DARK_THEME,
                   theme == TERMINAL_THEME_VARIANT_DARK,
                   nullptr);
  }
}

/* Submenus for New Terminal per profile, and to change profiles */

static void
terminal_app_check_default(TerminalApp* app)
{
#ifdef TERMINAL_SERVER
  // Only do this for the default app ID
  gs_free char* app_id = nullptr;
  g_object_get(app, "application-id", &app_id, nullptr);
  if (!_terminal_debug_on(TERMINAL_DEBUG_DEFAULT) &&
      !g_str_equal(app_id, TERMINAL_APPLICATION_ID))
    return;
#endif /* TERMINAL_SERVER */

  // Check whether gnome-terminal is the default terminal
  // as per XDG-Terminal-Exec.
  app->xte_is_default = terminal_is_default();

  gboolean ask = false;
  g_settings_get(app->global_settings, TERMINAL_SETTING_ALWAYS_CHECK_DEFAULT_KEY, "b", &ask);
  app->ask_default = (ask != false) && !app->xte_is_default;
}

#ifdef TERMINAL_SERVER

typedef struct {
  char *uuid;
  char *label;
} ProfileData;

static void
profile_data_clear (ProfileData *data)
{
  g_free (data->uuid);
  g_free (data->label);
}

typedef struct {
  GArray *array;
  TerminalApp *app;
} ProfilesForeachData;

static void
foreach_profile_cb (TerminalSettingsList *list,
                    const char *uuid,
                    GSettings *profile,
                    ProfilesForeachData *user_data)
{
  ProfileData data;
  data.uuid = g_strdup (uuid);
  data.label = g_settings_get_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

  g_array_append_val (user_data->array, data);
}

static int
compare_profile_label_cb (gconstpointer ap,
                          gconstpointer bp)
{
  const ProfileData *a = (ProfileData const*)ap;
  const ProfileData *b = (ProfileData const*)bp;

  return g_utf8_collate (a->label, b->label);
}

static void
fill_header_new_terminal_menu (GMenuModel *menu,
                               ProfileData *data,
                               guint n_profiles)
{
  gs_unref_object GMenu *section = nullptr;

  if (n_profiles <= 1)
    return;

  section = g_menu_new ();

  for (guint i = 0; i < n_profiles; i++) {
    terminal_util_menu_append_numbered (section, data[i].label, i + 1,
                                        "win.new-terminal",
                                        g_variant_new ("(ss)", "default", data[i].uuid));
  }

  g_menu_append_section (G_MENU (menu), _("New Terminal"), G_MENU_MODEL (section));
}

static GMenu *
set_profile_submenu_new (ProfileData *data,
                         guint n_profiles)
{
  /* No submenu if there's only one profile */
  if (n_profiles <= 1)
    return nullptr;

  GMenu *menu = g_menu_new ();
  for (guint i = 0; i < n_profiles; i++) {
    terminal_util_menu_append_numbered (menu, data[i].label, i + 1,
                                        "win.profile",
                                        g_variant_new_string (data[i].uuid));
  }

  return menu;
}

static void
terminal_app_update_profile_menus (TerminalApp *app)
{
  g_clear_object (&app->set_profile_menu);

  /* Get profiles list and sort by label */
  gs_unref_array GArray *array = g_array_sized_new (FALSE, TRUE, sizeof (ProfileData),
                                                    terminal_settings_list_get_n_children (app->profiles_list));
  g_array_set_clear_func (array, (GDestroyNotify) profile_data_clear);

  ProfilesForeachData data = { array, app };
  terminal_settings_list_foreach_child (app->profiles_list,
                                        (TerminalSettingsListForeachFunc) foreach_profile_cb,
                                        &data);
  g_array_sort (array, compare_profile_label_cb);

  ProfileData *profiles = (ProfileData*) array->data;
  guint n_profiles = array->len;

  app->set_profile_menu = set_profile_submenu_new (profiles, n_profiles);

  if (app->profilemenu != nullptr) {
    g_menu_remove_all (G_MENU (app->profilemenu));
    fill_header_new_terminal_menu (app->profilemenu, profiles, n_profiles);
  }

  if (app->headermenu != nullptr) {
    g_menu_remove_all (G_MENU (app->headermenu_set_profile_section));
    if (app->set_profile_menu != nullptr) {
      g_menu_append_submenu (app->headermenu_set_profile_section, _("_Profile"),
                             G_MENU_MODEL (app->set_profile_menu));
    }
  }
}

static void
terminal_app_create_headermenu (TerminalApp *app)
{
  terminal_util_load_objects_resource ("/org/gnome/terminal/ui/headerbar-menu.ui",
                                       "headermenu", &app->headermenu,
                                       "set-profile-section", &app->headermenu_set_profile_section,
                                       nullptr);

  /* Install profile sections */
  terminal_app_update_profile_menus (app);
}

static void
terminal_app_create_profilemenu (TerminalApp *app)
{
  app->profilemenu = G_MENU_MODEL (g_menu_new ());

  /* Install profile sections */
  terminal_app_update_profile_menus (app);
}

/* Clipboard */

static void
clipboard_owner_change_cb (GdkClipboard *clipboard,
                           TerminalApp *app)
{
  GdkContentFormats *formats;

  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (TERMINAL_IS_APP (app));

  _terminal_debug_print (TERMINAL_DEBUG_CLIPBOARD,
                         "Clipboard owner changed\n");

  formats = gdk_clipboard_get_formats (clipboard);

  if (app->clipboard_targets != formats) {
    if (formats)
      gdk_content_formats_ref (formats);

    g_clear_pointer (&app->clipboard_targets, gdk_content_formats_unref);
    app->clipboard_targets = formats;

    _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_CLIPBOARD) {
      GString *string = g_string_new (nullptr);
      if (formats != nullptr)
        gdk_content_formats_print (formats, string);
      g_printerr ("Clipboard has targets: %s\n", string->str);
    }

    g_signal_emit (app, signals[CLIPBOARD_TARGETS_CHANGED], 0, clipboard);
  }
}

/* Preferences */

struct PrefsLaunchData {
  GWeakRef app_ref;
  char* profile_uuid;
  char* hint;
  char* activation_token;
};

static auto
prefs_launch_data_new(TerminalApp* app,
                      char const* profile_uuid,
                      char const* hint,
                      char const* activation_token)
{
  auto data = g_new(PrefsLaunchData, 1);
  g_weak_ref_init(&data->app_ref, app);
  data->profile_uuid = g_strdup(profile_uuid);
  data->hint = g_strdup(hint);
  data->activation_token = g_strdup(activation_token);

  return data;
}

static void
prefs_launch_data_free(PrefsLaunchData* data)
{
  g_weak_ref_clear(&data->app_ref);
  g_free(data->profile_uuid);
  g_free(data->hint);
  g_free(data->activation_token);
  g_free(data);
}

static void
launch_prefs_cb(GObject* source,
                GAsyncResult* result,
                void* user_data)
{
  auto const data = reinterpret_cast<PrefsLaunchData*>(user_data);
  auto const app = reinterpret_cast<TerminalApp*>(g_weak_ref_get(&data->app_ref));

  // @process holds a ref on itself via the g_subprocess_wait_async() call,
  // so we only keep a weak ref that gets cleared when the process exits.
  gs_free_error GError* error = nullptr;
  gs_unref_object auto process = terminal_prefs_process_new_finish(result, &error);
  if (app)
    g_weak_ref_init(&app->prefs_process_ref, process);

  if (process) {
    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                          "Preferences process launched successfully.\n");

    terminal_prefs_process_show(process,
                                data->profile_uuid,
                                data->hint,
                                data->activation_token);
  } else {
    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                          "Failed to launch preferences process: %s\n", error->message);
  }

  prefs_launch_data_free(data);
}

/* Callbacks from former app menu.
 * The preferences one is still used with the "--preferences" cmdline option. */

static void
app_menu_preferences_cb (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  TerminalApp *app = (TerminalApp*)user_data;

  terminal_app_edit_preferences (app, nullptr, nullptr);
}

static void
app_menu_help_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  terminal_util_show_help (nullptr);
}

static void
app_menu_about_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  terminal_util_show_about ();
}

static void
app_menu_quit_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  GtkApplication *application = (GtkApplication*)user_data;
  GtkWindow *window;

  window = gtk_application_get_active_window (application);
  if (TERMINAL_IS_WINDOW (window))
    terminal_window_request_close (TERMINAL_WINDOW (window));
  else /* a dialogue */
    gtk_window_destroy (GTK_WINDOW (window));
}

#endif /* TERMINAL_SERVER */

/* Class implementation */

G_DEFINE_TYPE (TerminalApp, terminal_app, ADW_TYPE_APPLICATION)

/* GApplicationClass impl */

static void
terminal_app_activate (GApplication *application)
{
  /* No-op required because GApplication is stupid */
}

static void
terminal_app_startup (GApplication *application)
{
  auto const app = TERMINAL_APP(application);

  g_application_set_resource_base_path (application, TERMINAL_RESOURCES_PATH_PREFIX);

  G_APPLICATION_CLASS (terminal_app_parent_class)->startup (application);

#ifdef GDK_WINDOWING_X11
  auto const display = gdk_display_get_default ();
  if (GDK_IS_X11_DISPLAY (display)) {
    /* Need to set the WM class (bug #685742) */
# if defined(TERMINAL_SERVER)
    gdk_x11_display_set_program_class (display, "Gnome-terminal");
# elif defined(TERMINAL_PREFERENCES)
    gdk_x11_display_set_program_class (display, "Gnome-terminal-preferences");
# else
#  error
# endif
  }
#endif

  app_load_css (application);

#ifdef TERMINAL_SERVER
  GActionEntry const action_entries[] = {
    { "preferences", app_menu_preferences_cb,   nullptr, nullptr, nullptr },
    { "help",        app_menu_help_cb,          nullptr, nullptr, nullptr },
    { "about",       app_menu_about_cb,         nullptr, nullptr, nullptr },
    { "quit",        app_menu_quit_cb,          nullptr, nullptr, nullptr }
  };

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   action_entries, G_N_ELEMENTS (action_entries),
                                   application);

  /* Keep dynamic menus updated */
  g_signal_connect_swapped (app->profiles_list, "children-changed",
                            G_CALLBACK (terminal_app_update_profile_menus), app);
  g_signal_connect_swapped (app->profiles_list, "child-changed::" TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                            G_CALLBACK (terminal_app_update_profile_menus), app);

#endif /* TERMINAL_SERVER */

  terminal_app_check_default(app);

  _terminal_debug_print (TERMINAL_DEBUG_SERVER, "Startup complete\n");
}

static void
terminal_app_shutdown (GApplication *application)
{
  G_APPLICATION_CLASS (terminal_app_parent_class)->shutdown (application);

#ifdef TERMINAL_PREFERENCES
  auto const app = TERMINAL_APP(application);

  if (app->prefs_window)
    gtk_window_destroy(app->prefs_window);
#endif
}

#ifdef TERMINAL_PREFERENCES

static void
terminal_app_prefs_window_destroyed_cb(GtkWidget* window,
                                       TerminalApp* app)
{
  g_signal_handlers_disconnect_by_func(window,
                                       (void*)terminal_app_prefs_window_destroyed_cb,
                                       app);

  app->prefs_window = nullptr;
}

#endif /* TERMINAL_PREFERENCES */

/* GObjectClass impl */

static void
terminal_app_init (TerminalApp* app)
{
#ifdef TERMINAL_SERVER
  adw_init ();

  g_weak_ref_init(&app->prefs_process_ref, nullptr);

  app->screen_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, nullptr);
#endif
}

static void
terminal_app_constructed(GObject *object)
{
  auto app = TERMINAL_APP(object);

  G_OBJECT_CLASS(terminal_app_parent_class)->constructed(object);

  terminal_app_init_debug ();

#if defined(TERMINAL_SERVER)
  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);
#elif defined(TERMINAL_PREFERENCES)
  gtk_window_set_default_icon_name(GNOME_TERMINAL_PREFERENCES_ICON_NAME);
#else
#error
#endif

  if (app->settings_backend == nullptr)
    app->settings_backend = g_settings_backend_get_default ();

  app->schema_source = terminal_g_settings_schema_source_get_default();

  /* Desktop proxy settings */
  app->system_proxy_settings = terminal_g_settings_new(app->settings_backend,
                                                       app->schema_source,
                                                       SYSTEM_PROXY_SETTINGS_SCHEMA);

  /* Since there is no way to get the schema ID of a child schema, we cannot
   * verify that the installed schemas are correct. Also, due to a glib bug
   * (https://gitlab.gnome.org/GNOME/glib/-/issues/1884) g_settings_get_child()
   * doesn't work with non-default schema sources.
   * So instead of using g_settings_get_child() on the SYSTEM_PROXY_SETTINGS_SCHEMA,
   * we construct the child GSettings directly.
   */
  app->system_proxy_protocol_settings[TERMINAL_PROXY_HTTP] =
    terminal_g_settings_new(app->settings_backend,
                            app->schema_source,
                            SYSTEM_HTTP_PROXY_SETTINGS_SCHEMA);
  app->system_proxy_protocol_settings[TERMINAL_PROXY_HTTPS] =
    terminal_g_settings_new(app->settings_backend,
                            app->schema_source,
                            SYSTEM_HTTPS_PROXY_SETTINGS_SCHEMA);
  app->system_proxy_protocol_settings[TERMINAL_PROXY_FTP] =
    terminal_g_settings_new(app->settings_backend,
                            app->schema_source,
                            SYSTEM_FTP_PROXY_SETTINGS_SCHEMA);
  app->system_proxy_protocol_settings[TERMINAL_PROXY_SOCKS] =
    terminal_g_settings_new(app->settings_backend,
                            app->schema_source,
                            SYSTEM_SOCKS_PROXY_SETTINGS_SCHEMA);

  /* Desktop Interface settings */
  app->desktop_interface_settings = terminal_g_settings_new(app->settings_backend,
                                                            app->schema_source,
                                                            DESKTOP_INTERFACE_SETTINGS_SCHEMA);

  /* Terminal global settings */
  app->global_settings = terminal_g_settings_new(app->settings_backend,
                                                 app->schema_source,
                                                 TERMINAL_SETTING_SCHEMA);

  /* Gtk debug settings */
  app->gtk_debug_settings = terminal_g_settings_new(app->settings_backend,
                                                    app->schema_source,
                                                    GTK_DEBUG_SETTING_SCHEMA);

  /* These are internal settings that exists only for distributions
   * to override, so we cache them on startup and don't react to changes.
   */
  app->use_headerbar = terminal_app_should_use_headerbar (app);

#ifdef TERMINAL_SERVER

  /* These are internal settings that exists only for distributions
   * to override, so we cache them on startup and don't react to changes.
   */
  app->unified_menu = g_settings_get_boolean (app->global_settings, TERMINAL_SETTING_UNIFIED_MENU_KEY);

#endif /* TERMINAL_SERVER */

  app->style_manager = adw_style_manager_get_default();

  auto const gtk_settings = gtk_settings_get_default ();
  terminal_app_theme_variant_changed_cb (app->global_settings,
                                         TERMINAL_SETTING_THEME_VARIANT_KEY, gtk_settings);

  g_signal_connect (app->global_settings,
                    "changed::" TERMINAL_SETTING_THEME_VARIANT_KEY,
                    G_CALLBACK (terminal_app_theme_variant_changed_cb),
                    gtk_settings);
  g_signal_connect(app->style_manager,
                   "notify::system-supports-color-schemes",
                   G_CALLBACK(terminal_app_theme_variant_changed_cb),
                   gtk_settings);

#ifdef TERMINAL_SERVER

  /* Clipboard targets */
  GdkDisplay *display = gdk_display_get_default ();
  app->clipboard = gdk_display_get_clipboard (display);
  clipboard_owner_change_cb (app->clipboard, app);
  g_signal_connect (app->clipboard, "changed",
                    G_CALLBACK (clipboard_owner_change_cb), app);
#endif /* TERMINAL_SERVER */

  /* Get the profiles */
  app->profiles_list = terminal_profiles_list_new(app->settings_backend,
                                                  app->schema_source);

  gs_unref_object auto settings =
    terminal_g_settings_new_with_path(app->settings_backend,
                                      app->schema_source,
                                      TERMINAL_KEYBINDINGS_SCHEMA,
                                      TERMINAL_KEYBINDINGS_SCHEMA_PATH);
  terminal_accels_init (G_APPLICATION (app), settings, app->use_headerbar);
}

static void
terminal_app_finalize (GObject *object)
{
  auto app = TERMINAL_APP(object);

#ifdef TERMINAL_SERVER
  g_signal_handlers_disconnect_by_func (app->clipboard,
                                        (void*)clipboard_owner_change_cb,
                                        app);
  g_clear_pointer (&app->clipboard_targets, gdk_content_formats_unref);

  g_signal_handlers_disconnect_by_func (app->profiles_list,
                                        (void*)terminal_app_update_profile_menus,
                                        app);
  g_hash_table_destroy (app->screen_map);
#endif

#ifdef TERMINAL_PREFERENCES
  if (app->prefs_window) {
    g_signal_handlers_disconnect_by_func(app->prefs_window,
                                         (void*)terminal_app_prefs_window_destroyed_cb,
                                         app);
    gtk_window_destroy(app->prefs_window);
    app->prefs_window = nullptr;
  }
#endif

  g_object_unref (app->global_settings);
  g_object_unref (app->desktop_interface_settings);
  g_object_unref (app->system_proxy_settings);
  for (int i = 0; i < 4; ++i)
    g_object_unref(app->system_proxy_protocol_settings[i]);
  g_clear_object (&app->gtk_debug_settings);
  g_settings_schema_source_unref(app->schema_source);
  g_clear_object (&app->settings_backend);

#ifdef TERMINAL_SERVER
  g_clear_object (&app->profilemenu);
  g_clear_object (&app->headermenu);
  g_clear_object (&app->headermenu_set_profile_section);
  g_clear_object (&app->set_profile_menu);

  {
    gs_unref_object auto process = reinterpret_cast<TerminalPrefsProcess*>(g_weak_ref_get(&app->prefs_process_ref));
    if (process)
      terminal_prefs_process_abort(process);
  }

  g_weak_ref_clear(&app->prefs_process_ref);
#endif /* TERMINAL_SERVER */

  g_clear_object (&app->default_icon);
  g_clear_object (&app->default_icon_symbolic);

  terminal_accels_shutdown ();

  G_OBJECT_CLASS (terminal_app_parent_class)->finalize (object);
}

static void
terminal_app_get_property(GObject* object,
                          guint prop_id,
                          GValue* value,
                          GParamSpec* pspec)
{
  auto app = TERMINAL_APP(object);

  switch (prop_id) {
  case PROP_SETTINGS_BACKEND:
    g_value_set_object(value, app->settings_backend);
    break;
  case PROP_IS_DEFAULT_TERMINAL:
    g_value_set_boolean(value, app->xte_is_default);
    break;
  case PROP_ASK_DEFAULT_TERMINAL:
    g_value_set_boolean(value, app->ask_default);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
terminal_app_set_property(GObject* object,
                          guint prop_id,
                          GValue const* value,
                          GParamSpec* pspec)
{
  auto app = TERMINAL_APP(object);

  switch (prop_id) {
  case PROP_SETTINGS_BACKEND:
    app->settings_backend = G_SETTINGS_BACKEND(g_value_dup_object(value));
    break;
  case PROP_ASK_DEFAULT_TERMINAL:
    app->ask_default = g_value_get_boolean(value);
    break;
  case PROP_IS_DEFAULT_TERMINAL: // not writable
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

#ifdef TERMINAL_SERVER

static gboolean
terminal_app_dbus_register (GApplication    *application,
                            GDBusConnection *connection,
                            const gchar     *object_path,
                            GError         **error)
{
  TerminalApp *app = TERMINAL_APP (application);
  gs_unref_object TerminalObjectSkeleton *object = nullptr;
  gs_unref_object TerminalFactory *factory = nullptr;

  if (!G_APPLICATION_CLASS (terminal_app_parent_class)->dbus_register (application,
                                                                       connection,
                                                                       object_path,
                                                                       error))
    return FALSE;

#ifdef ENABLE_SEARCH_PROVIDER
  if (g_settings_get_boolean (app->global_settings, TERMINAL_SETTING_SHELL_INTEGRATION_KEY)) {
    gs_unref_object TerminalSearchProvider *search_provider;

    search_provider = terminal_search_provider_new ();

    if (!terminal_search_provider_dbus_register (search_provider,
                                                 connection,
                                                 TERMINAL_SEARCH_PROVIDER_PATH,
                                                 error))
      return FALSE;

    gs_transfer_out_value (&app->search_provider, &search_provider);
  }
#endif /* ENABLE_SEARCH_PROVIDER */

  object = terminal_object_skeleton_new (TERMINAL_FACTORY_OBJECT_PATH);
  factory = terminal_factory_impl_new ();
  terminal_object_skeleton_set_factory (object, factory);

  app->object_manager = g_dbus_object_manager_server_new (TERMINAL_OBJECT_PATH_PREFIX);
  g_dbus_object_manager_server_export (app->object_manager, G_DBUS_OBJECT_SKELETON (object));

  /* And export the object */
  g_dbus_object_manager_server_set_connection (app->object_manager, connection);
  return TRUE;
}

static void
terminal_app_dbus_unregister (GApplication    *application,
                              GDBusConnection *connection,
                              const gchar     *object_path)
{
  TerminalApp *app = TERMINAL_APP (application);

  if (app->object_manager) {
    g_dbus_object_manager_server_unexport (app->object_manager, TERMINAL_FACTORY_OBJECT_PATH);
    g_object_unref (app->object_manager);
    app->object_manager = nullptr;
  }

#ifdef ENABLE_SEARCH_PROVIDER
  if (app->search_provider) {
    terminal_search_provider_dbus_unregister (app->search_provider, connection, TERMINAL_SEARCH_PROVIDER_PATH);
    g_object_unref (app->search_provider);
    app->search_provider = nullptr;
  }
#endif /* ENABLE_SEARCH_PROVIDER */

  G_APPLICATION_CLASS (terminal_app_parent_class)->dbus_unregister (application,
                                                                    connection,
                                                                    object_path);
}

#endif /* TERMINAL_SERVER */

static void
terminal_app_class_init (TerminalAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *g_application_class = G_APPLICATION_CLASS (klass);

  object_class->constructed = terminal_app_constructed;
  object_class->finalize = terminal_app_finalize;
  object_class->get_property = terminal_app_get_property;
  object_class->set_property = terminal_app_set_property;

  g_object_class_install_property
    (object_class,
     PROP_SETTINGS_BACKEND,
     g_param_spec_object("settings-backend", nullptr, nullptr,
                         G_TYPE_SETTINGS_BACKEND,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property
    (object_class,
     PROP_IS_DEFAULT_TERMINAL,
     g_param_spec_boolean("is-default-terminal", nullptr, nullptr,
                          false,
                          GParamFlags(G_PARAM_READABLE |
                                      G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property
    (object_class,
     PROP_ASK_DEFAULT_TERMINAL,
     g_param_spec_boolean("ask-default-terminal", nullptr, nullptr,
                          false,
                          GParamFlags(G_PARAM_READWRITE |
                                      G_PARAM_STATIC_STRINGS)));

  g_application_class->activate = terminal_app_activate;
  g_application_class->startup = terminal_app_startup;
  g_application_class->shutdown = terminal_app_shutdown;
#ifdef TERMINAL_SERVER
  g_application_class->dbus_register = terminal_app_dbus_register;
  g_application_class->dbus_unregister = terminal_app_dbus_unregister;
#endif

  signals[CLIPBOARD_TARGETS_CHANGED] =
    g_signal_new (I_("clipboard-targets-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalAppClass, clipboard_targets_changed),
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE, 1, GDK_TYPE_CLIPBOARD);
}

/* Public API */

GApplication*
terminal_app_new(char const* app_id,
                 GApplicationFlags flags,
                 GSettingsBackend* backend)
{
  return reinterpret_cast<GApplication*>
    (g_object_new (TERMINAL_TYPE_APP,
		   "application-id", app_id ? app_id : TERMINAL_APPLICATION_ID,
		   "flags", flags,
                   "settings-backend", backend,
		   nullptr));
}

#ifdef TERMINAL_SERVER

TerminalScreen *
terminal_app_get_screen_by_uuid (TerminalApp *app,
                                 const char  *uuid)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), nullptr);

  return reinterpret_cast<TerminalScreen*>(g_hash_table_lookup (app->screen_map, uuid));
}

char *
terminal_app_dup_screen_object_path (TerminalApp *app,
                                     TerminalScreen *screen)
{
  char *object_path = g_strdup_printf (TERMINAL_RECEIVER_OBJECT_PATH_FORMAT,
                                       terminal_screen_get_uuid (screen));
  object_path = g_strdelimit (object_path,  "-", '_');
  g_assert (g_variant_is_object_path (object_path));
  return object_path;
}

/**
 * terminal_app_get_receiver_impl_by_object_path:
 * @app:
 * @object_path:
 *
 * Returns: (transfer full): the #TerminalReceiverImpl for @object_path, or %nullptr
 */
static TerminalReceiverImpl *
terminal_app_get_receiver_impl_by_object_path (TerminalApp *app,
                                               const char *object_path)
{
  gs_unref_object GDBusObject *skeleton =
    g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (app->object_manager),
                                      object_path);
  if (skeleton == nullptr || !TERMINAL_IS_OBJECT_SKELETON (skeleton))
    return nullptr;

  TerminalReceiverImpl *impl = nullptr;
  g_object_get (skeleton, "receiver", &impl, nullptr);
  if (impl == nullptr)
    return nullptr;

  g_assert (TERMINAL_IS_RECEIVER_IMPL (impl));
  return impl;
}

/**
 * terminal_app_get_screen_by_object_path:
 * @app:
 * @object_path:
 *
 * Returns: (transfer full): the #TerminalScreen for @object_path, or %nullptr
 */
TerminalScreen *
terminal_app_get_screen_by_object_path (TerminalApp *app,
                                        const char *object_path)
{
  gs_unref_object TerminalReceiverImpl *impl =
    terminal_app_get_receiver_impl_by_object_path (app, object_path);
  if (impl == nullptr)
    return nullptr;

  return terminal_receiver_impl_get_screen (impl);
}

void
terminal_app_register_screen (TerminalApp *app,
                              TerminalScreen *screen)
{
  const char *uuid = terminal_screen_get_uuid (screen);
  g_hash_table_insert (app->screen_map, g_strdup (uuid), screen);

  gs_free char *object_path = terminal_app_dup_screen_object_path (app, screen);
  TerminalObjectSkeleton *skeleton = terminal_object_skeleton_new (object_path);

  TerminalReceiverImpl *impl = terminal_receiver_impl_new (screen);
  terminal_object_skeleton_set_receiver (skeleton, TERMINAL_RECEIVER (impl));
  g_object_unref (impl);

  g_dbus_object_manager_server_export (app->object_manager,
                                       G_DBUS_OBJECT_SKELETON (skeleton));
}

void
terminal_app_unregister_screen (TerminalApp *app,
                                TerminalScreen *screen)
{
  const char *uuid = terminal_screen_get_uuid (screen);
  gboolean found = g_hash_table_remove (app->screen_map, uuid);
  g_warn_if_fail (found);
  if (!found)
    return; /* repeat unregistering */

  gs_free char *object_path = terminal_app_dup_screen_object_path (app, screen);
  gs_unref_object TerminalReceiverImpl *impl =
    terminal_app_get_receiver_impl_by_object_path (app, object_path);
  g_warn_if_fail (impl != nullptr);

  if (impl != nullptr)
    terminal_receiver_impl_unset_screen (impl);

  g_dbus_object_manager_server_unexport (app->object_manager, object_path);
}

GdkContentFormats *
terminal_app_get_clipboard_targets (TerminalApp *app,
                                    GdkClipboard *clipboard)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), nullptr);

  if (clipboard != app->clipboard)
    return nullptr;

  return app->clipboard_targets;
}

void
terminal_app_edit_preferences(TerminalApp* app,
                              GSettings* profile,
                              char const* hint)
{
  gs_free char* uuid = nullptr;
  if (profile)
    uuid = terminal_settings_list_dup_uuid_from_child (app->profiles_list, profile);

  gs_unref_object auto appinfo =
    g_desktop_app_info_new(GNOME_TERMINAL_PREFERENCES_DESKTOP_NAME);
  gs_free char* token = nullptr;
  if (appinfo) {
    auto const display = gdk_display_get_default();
    gs_unref_object auto context = gdk_display_get_app_launch_context(display);

    token = G_APP_LAUNCH_CONTEXT_GET_CLASS(context)->get_startup_notify_id
      (G_APP_LAUNCH_CONTEXT(context),
       G_APP_INFO(appinfo),
       nullptr); // no files
  }
  if (!token)
    token = terminal_client_get_fallback_startup_id();

  gs_unref_object auto process = reinterpret_cast<TerminalPrefsProcess*>(g_weak_ref_get(&app->prefs_process_ref));
  if (process) {
    terminal_prefs_process_show(process,
                                uuid,
                                hint,
                                token);
  } else {
    terminal_prefs_process_new_async(nullptr, // cancellable,
                                     GAsyncReadyCallback(launch_prefs_cb),
                                     prefs_launch_data_new(app, uuid, hint, token));
  }
}

#endif /* TERMINAL_SERVER */

#ifdef TERMINAL_PREFERENCES

static bool
timestamp_from_activation_token(char const* token,
                                guint32* timestamp)
{
  if (!token)
    return false;

  if (auto needle = g_strrstr(token, "_TIME")) {
    needle += strlen("_TIME");

    char* end = nullptr;
    errno = 0;
    auto const v = g_ascii_strtoull(needle, &end, 0);
    if (errno || end == needle ||
        v > G_MAXUINT32)
      return false;

    *timestamp = guint32(v);
    return true;
  }

  return false;
}

void
terminal_app_edit_preferences(TerminalApp* app,
                              GSettings* profile,
                              char const* hint,
                              char const* activation_token)
{
  if (!app->prefs_window) {
    app->prefs_window = terminal_preferences_window_new(GTK_APPLICATION(app));
    if (activation_token)
      gtk_window_set_startup_id(app->prefs_window, activation_token);

    g_signal_connect(app->prefs_window, "destroy",
                     G_CALLBACK(terminal_app_prefs_window_destroyed_cb),
                     app);
  }

  if (profile)
    terminal_preferences_window_edit_profile(TERMINAL_PREFERENCES_WINDOW(app->prefs_window),
                                             profile);

  guint32 timestamp;
  if (timestamp_from_activation_token(activation_token, &timestamp))
    gtk_window_present_with_time(GTK_WINDOW(app->prefs_window), timestamp);
  else
    gtk_window_present(GTK_WINDOW(app->prefs_window));
}

#endif /* TERMINAL_PREFERENCES */

/**
 * terminal_app_get_profiles_list:
 *
 * Returns: (transfer none): returns the singleton profiles list #TerminalSettingsList
 */
TerminalSettingsList *
terminal_app_get_profiles_list (TerminalApp *app)
{
  return app->profiles_list;
}

#ifdef TERMINAL_SERVER

/**
 * terminal_app_get_headermenu:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the main window headerbar menu bar as a #GMenuModel
 */
GMenuModel *
terminal_app_get_headermenu (TerminalApp *app)
{
  if (app->headermenu == nullptr)
    terminal_app_create_headermenu (app);

  return app->headermenu;
}

/**
 * terminal_app_get_profilemenu:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the main window headerbar profile menu as a #GMenuModel
 */
GMenuModel *
terminal_app_get_profilemenu (TerminalApp *app)
{
  if (app->profilemenu == nullptr)
    terminal_app_create_profilemenu (app);

  return app->profilemenu;
}

#endif /* TERMINAL_SERVER */

/**
 * terminal_app_get_settings_backend:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the #GSettingsBackend to use for all #GSettings instances
 */
GSettingsBackend*
terminal_app_get_settings_backend(TerminalApp *app)
{
  return app->settings_backend;
}

/**
 * terminal_app_get_schema_source:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the #GSettingsSchemaSource to use for all #GSettings instances
 */
GSettingsSchemaSource*
terminal_app_get_schema_source(TerminalApp *app)
{
  return app->schema_source;
}

/**
 * terminal_app_get_global_settings:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the cached #GSettings object for the org.gnome.Terminal.Preferences schema
 */
GSettings *
terminal_app_get_global_settings (TerminalApp *app)
{
  return app->global_settings;
}

/**
 * terminal_app_get_desktop_interface_settings:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the cached #GSettings object for the org.gnome.interface schema
 */
GSettings *
terminal_app_get_desktop_interface_settings (TerminalApp *app)
{
  return app->desktop_interface_settings;
}

/**
 * terminal_app_get_proxy_settings:
 * @app: a #TerminalApp
 *
 * Returns: (tranfer none): the cached #GSettings object for the org.gnome.system.proxy schema
 */
GSettings *
terminal_app_get_proxy_settings (TerminalApp *app)
{
  return app->system_proxy_settings;
}

/**
 * terminal_app_get_proxy_settings_for_protocol:
 * @app: a #TerminalApp
 * @protocol: a #TerminalProxyProtocol
 *
 * Returns: (tranfer none): the cached #GSettings object for the org.gnome.system.proxy.@protocol schema
 */
GSettings*
terminal_app_get_proxy_settings_for_protocol(TerminalApp *app,
                                             TerminalProxyProtocol protocol)
{
  return app->system_proxy_protocol_settings[(int)protocol];
}

GSettings *
terminal_app_get_gtk_debug_settings (TerminalApp *app)
{
  return app->gtk_debug_settings;
}

/**
 * terminal_app_get_system_font:
 * @app:
 *
 * Creates a #PangoFontDescription for the system monospace font.
 *
 * Returns: (transfer full): a new #PangoFontDescription
 */
PangoFontDescription *
terminal_app_get_system_font (TerminalApp *app)
{
  gs_free char *font = nullptr;

  g_return_val_if_fail (TERMINAL_IS_APP (app), nullptr);

  font = g_settings_get_string (app->desktop_interface_settings, MONOSPACE_FONT_KEY_NAME);

  return pango_font_description_from_string (font);
}

#ifdef TERMINAL_SERVER

GDBusObjectManagerServer *
terminal_app_get_object_manager (TerminalApp *app)
{
  g_warn_if_fail (app->object_manager != nullptr);
  return app->object_manager;
}

void*
terminal_app_get_adw_style_manager(TerminalApp* app)
{
  g_return_val_if_fail(TERMINAL_IS_APP(app), nullptr);
  return app->style_manager;
}

#endif /* TERMINAL_SERVER */

gboolean
terminal_app_get_menu_unified (TerminalApp *app)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), TRUE);

  return app->unified_menu;
}

gboolean
terminal_app_get_use_headerbar (TerminalApp *app)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), FALSE);

  return app->use_headerbar;
}

gboolean
terminal_app_get_dialog_use_headerbar (TerminalApp *app)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), FALSE);

  gboolean dialog_use_header;
  g_object_get (gtk_settings_get_default (),
                "gtk-dialogs-use-header", &dialog_use_header,
                nullptr);

  return dialog_use_header && app->use_headerbar;
}

gboolean
terminal_app_is_default_terminal(TerminalApp* app)
{
  g_return_val_if_fail(TERMINAL_IS_APP(app), false);
  return app->xte_is_default;
}

gboolean
terminal_app_get_ask_default_terminal(TerminalApp* app)
{
  g_return_val_if_fail(TERMINAL_IS_APP(app), false);
  return app->ask_default;
}

void
terminal_app_unset_ask_default_terminal(TerminalApp* app)
{
  g_return_if_fail(TERMINAL_IS_APP(app));
  app->ask_default = false;
  g_object_notify(G_OBJECT(app), "ask-default-terminal");
}

void
terminal_app_make_default_terminal(TerminalApp* app)
{
  g_return_if_fail(TERMINAL_IS_APP(app));
  terminal_make_default();
  app->xte_is_default = terminal_is_default();
  g_object_notify(G_OBJECT(app), "is-default-terminal");
}

GIcon*
terminal_app_get_default_icon(TerminalApp* app)
{
  g_return_val_if_fail(TERMINAL_IS_APP(app), nullptr);

  if (!app->default_icon)
    app->default_icon = g_themed_icon_new(GNOME_TERMINAL_ICON_NAME);

  return app->default_icon;
}

GIcon*
terminal_app_get_default_icon_symbolic(TerminalApp* app)
{
  g_return_val_if_fail(TERMINAL_IS_APP(app), nullptr);

  if (!app->default_icon_symbolic)
    app->default_icon_symbolic = g_themed_icon_new(GNOME_TERMINAL_ICON_SYMBOLIC_NAME);

  return app->default_icon_symbolic;
}
