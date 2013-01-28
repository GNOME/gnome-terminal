/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#include <config.h>

#include <glib.h>
#include <gio/gio.h>

#include "terminal-intl.h"

#include "terminal-debug.h"
#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-screen.h"
#include "terminal-screen-container.h"
#include "terminal-window.h"
#include "terminal-profile-utils.h"
#include "terminal-util.h"
#include "profile-editor.h"
#include "terminal-encoding.h"
#include "terminal-schemas.h"
#include "terminal-gdbus.h"
#include "terminal-defines.h"
#include "terminal-prefs.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <uuid.h>
#include <dconf.h>

#define DESKTOP_INTERFACE_SETTINGS_SCHEMA       "org.gnome.desktop.interface"

#define SYSTEM_PROXY_SETTINGS_SCHEMA            "org.gnome.system.proxy"

/*
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See gnome-terminal 1.x for why.
 */

struct _TerminalAppClass {
  GtkApplicationClass parent_class;

  void (* profile_list_changed) (TerminalApp *app);
  void (* encoding_list_changed) (TerminalApp *app);
};

struct _TerminalApp
{
  GtkApplication parent_instance;

  GDBusObjectManagerServer *object_manager;

  GtkWidget *new_profile_dialog;

  GHashTable *profiles_hash;

  GHashTable *encodings;
  gboolean encodings_locked;

  GSettings *global_settings;
  GSettings *profiles_settings;
  GSettings *desktop_interface_settings;
  GSettings *system_proxy_settings;
};

enum
{
  PROFILE_LIST_CHANGED,
  ENCODING_LIST_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Helper functions */

static void
maybe_migrate_settings (TerminalApp *app)
{
#ifdef ENABLE_MIGRATION
  const char * const argv[] = { 
    TERM_LIBEXECDIR "/gnome-terminal-migration",
#ifdef GNOME_ENABLE_DEBUG
    "--verbose", 
#endif
    NULL 
  };
  guint version;
  int status;
  GError *error = NULL;

  version = g_settings_get_uint (terminal_app_get_global_settings (app), TERMINAL_SETTING_SCHEMA_VERSION);
  if (version >= TERMINAL_SCHEMA_VERSION) {
     _terminal_debug_print (TERMINAL_DEBUG_SERVER | TERMINAL_DEBUG_PROFILE,
                            "Schema version is %d, already migrated.\n", version);
    return;
  }

  if (!g_spawn_sync (NULL /* our home directory */,
                     (char **) argv,
                     NULL /* envv */,
                     0,
                     NULL, NULL,
                     NULL, NULL,
                     &status,
                     &error)) {
    g_printerr ("Failed to migrate settings: %s\n", error->message);
    g_error_free (error);
    return;
  }

  if (WIFEXITED (status)) {
    if (WEXITSTATUS (status) != 0)
      g_printerr ("Profile migrator exited with status %d\n", WEXITSTATUS (status));
  } else {
    g_printerr ("Profile migrator exited abnormally.\n");
  }
#else
  g_settings_set_uint (terminal_app_get_global_settings (app),
                       TERMINAL_SETTING_SCHEMA_VERSION,
                       TERMINAL_SCHEMA_VERSION);
#endif /* ENABLE_MIGRATION */
}

static char **
strv_insert (char **strv,
             char *str)
{
  guint i;

  for (i = 0; strv[i]; i++)
    if (strcmp (strv[i], str) == 0)
      return strv;

  /* Not found; append */
  strv = g_realloc_n (strv, i + 2, sizeof (char *));
  strv[i++] = str;
  strv[i] = NULL;

  return strv;
}

static char **
strv_remove (char **strv,
             char *str)
{
  char **a, **b;

  a = b = strv;
  while (*a) {
    if (strcmp (*a, str) != 0)
      *b++ = *a;
    a++;
  }
  *b = NULL;

  return strv;
}

static GSettings * /* ref */
profile_clone (TerminalApp *app,
               GSettings *base_profile)
{
  GSettings *profile;
  uuid_t u;
  char str[37];
  char *new_path;
  char **profiles;

  uuid_generate (u);
  uuid_unparse (u, str);
  new_path = g_strdup_printf (TERMINAL_PROFILES_PATH_PREFIX ":%s/", str);

  if (base_profile)
    {
      static const char * const keys[] = {
        TERMINAL_PROFILE_ALLOW_BOLD_KEY,
        TERMINAL_PROFILE_AUDIBLE_BELL_KEY,
        TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
        TERMINAL_PROFILE_BACKSPACE_BINDING_KEY,
        TERMINAL_PROFILE_BOLD_COLOR_KEY,
        TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
        TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY,
        TERMINAL_PROFILE_CURSOR_SHAPE_KEY,
        TERMINAL_PROFILE_CUSTOM_COMMAND_KEY,
        TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY,
        TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY,
        TERMINAL_PROFILE_DELETE_BINDING_KEY,
        TERMINAL_PROFILE_ENCODING,
        TERMINAL_PROFILE_EXIT_ACTION_KEY,
        TERMINAL_PROFILE_FONT_KEY,
        TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
        TERMINAL_PROFILE_LOGIN_SHELL_KEY,
        TERMINAL_PROFILE_NAME_KEY,
        TERMINAL_PROFILE_PALETTE_KEY,
        TERMINAL_PROFILE_SCROLLBACK_LINES_KEY,
        TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
        TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY,
        TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY,
        TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY,
        TERMINAL_PROFILE_TITLE_MODE_KEY,
        TERMINAL_PROFILE_TITLE_KEY,
        TERMINAL_PROFILE_UPDATE_RECORDS_KEY,
        TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
        TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE_KEY,
        TERMINAL_PROFILE_USE_SKEY_KEY,
        TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
        TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
        /* TERMINAL_PROFILE_VISIBLE_NAME_KEY, */
        TERMINAL_PROFILE_WORD_CHARS_KEY,
      };
      DConfClient *client;
#ifndef HAVE_DCONF_1_2
      DConfChangeset *changeset;
#endif
      char *base_path;
      guint i;

      g_object_get (base_profile, "path", &base_path, NULL);

#ifdef HAVE_DCONF_1_2
      client = dconf_client_new (NULL, NULL, NULL, NULL);
#else
      client = dconf_client_new ();
      changeset = dconf_changeset_new ();
#endif

      for (i = 0; i < G_N_ELEMENTS (keys); i++)
        {
          GVariant *value;
          char *p;

          p = g_strconcat (base_path, keys[i], NULL);
#ifdef HAVE_DCONF_1_2
          value = dconf_client_read_no_default (client, p);
#else
          value = dconf_client_read (client, p);
#endif
          g_free (p);

          if (value)
            {
              p = g_strconcat (new_path, keys[i], NULL);
#ifdef HAVE_DCONF_1_2
              dconf_client_write (client, p, value, NULL, NULL, NULL);
#else
              dconf_changeset_set (changeset, p, value);
#endif
              g_free (p);
              g_variant_unref (value);
            }
        }

#ifndef HAVE_DCONF_1_2
      dconf_client_change_sync (client, changeset, NULL, NULL, NULL);
      g_object_unref (changeset);
#endif
      g_object_unref (client);
      g_free (base_path);
    }

  profile = g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, new_path);
  g_free (new_path);

  if (base_profile)
    {
      const char *base_name;
      char *new_name;

      g_settings_get (base_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &base_name);
      new_name = g_strdup_printf ("%s (Cloned)", base_name);
      g_settings_set_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, new_name);
      g_free (new_name);
    }

  /* Store the new UUID in the list of profiles, and add the profile to the hash table.
   * We'll get a changed signal for the profile list key, but that will result in a no-op.
   */
  g_hash_table_insert (app->profiles_hash, g_strdup (str) /* adopted */, profile /* adopted */);
  g_signal_emit (app, signals[PROFILE_LIST_CHANGED], 0);

  g_settings_get (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, "^a&s", &profiles);
  profiles = strv_insert (profiles, str);
  g_settings_set_strv (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, (const char * const *) profiles);
  g_free (profiles);

  return g_object_ref (profile);
}

void
terminal_app_remove_profile (TerminalApp *app,
                             GSettings *profile)
{
  char *uuid, *path;
  char **profiles;
  DConfClient *client;

  uuid = terminal_profile_util_get_profile_uuid (profile);
  g_object_get (profile, "path", &path, NULL);

  g_settings_get (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, "^a&s", &profiles);
  profiles = strv_remove (profiles, uuid);
  g_settings_set_strv (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, (const char * const *) profiles);
  g_free (profiles);

  /* unset all keys under the profile's path */
#ifdef HAVE_DCONF_1_2
  client = dconf_client_new (NULL, NULL, NULL, NULL);
  dconf_client_write (client, path, NULL, NULL, NULL, NULL);
#else /* modern DConf */
  client = dconf_client_new ();
  dconf_client_write_sync (client, path, NULL, NULL, NULL, NULL);
#endif
  g_object_unref (client);

  g_free (uuid);
  g_free (path);
}

gboolean
terminal_app_can_remove_profile (TerminalApp *app,
                                 GSettings *profile)
{
  return g_hash_table_size (app->profiles_hash) > 1;
}

void
terminal_app_get_profiles_iter (TerminalApp *app,
                                GHashTableIter *iter)
{
  g_hash_table_iter_init (iter, app->profiles_hash);
}

static void
terminal_app_profile_list_changed_cb (GSettings *settings,
                                      const char *key,
                                      TerminalApp *app)
{
  char **profiles, *default_profile;
  guint i;
  GHashTable *new_profiles;
  gboolean changed = FALSE;

  /* Use get_mapped so we can be sure never to get valid profile names, and
   * never an empty profile list, since the schema defines one profile.
   */
  profiles = terminal_profile_util_get_profiles (app->global_settings);
  g_settings_get (app->global_settings, TERMINAL_SETTING_DEFAULT_PROFILE_KEY,
                  "&s", &default_profile);

  new_profiles = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        (GDestroyNotify) g_free,
                                        (GDestroyNotify) g_object_unref);

  for (i = 0; profiles[i] != NULL; i++) {
    const char *name = profiles[i];
    GSettings *profile;

    if (app->profiles_hash)
      profile = g_hash_table_lookup (app->profiles_hash, name);
    else
      profile = NULL;

    if (profile) {
      g_object_ref (profile);
      g_hash_table_remove (app->profiles_hash, name);
    } else {
      char *path;
      path = g_strdup_printf (TERMINAL_PROFILES_PATH_PREFIX ":%s/", name);
      profile = g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, path);
      g_free (path);
      changed = TRUE;
    }

    g_hash_table_insert (new_profiles, g_strdup (name) /* adopted */, profile /* adopted */);
  }
  g_strfreev (profiles);

  g_assert (g_hash_table_size (new_profiles) > 0);

  if (app->profiles_hash == NULL ||
      g_hash_table_size (app->profiles_hash) > 0)
    changed = TRUE;

  if (app->profiles_hash != NULL)
    g_hash_table_unref (app->profiles_hash);
  app->profiles_hash = new_profiles;

  if (changed)
    g_signal_emit (app, signals[PROFILE_LIST_CHANGED], 0);
}

static int
compare_encodings (TerminalEncoding *a,
                   TerminalEncoding *b)
{
  return g_utf8_collate (a->name, b->name);
}

static void
encoding_mark_active (gpointer key,
                      gpointer value,
                      gpointer data)
{
  TerminalEncoding *encoding = (TerminalEncoding *) value;
  guint active = GPOINTER_TO_UINT (data);

  encoding->is_active = active;
}

static void
terminal_app_encoding_list_notify_cb (GSettings   *settings,
                                      const char  *key,
                                      TerminalApp *app)
{
  char **encodings;
  int i;
  TerminalEncoding *encoding;

  app->encodings_locked = !g_settings_is_writable (settings, key);

  /* Mark all as non-active, then re-enable the active ones */
  g_hash_table_foreach (app->encodings, (GHFunc) encoding_mark_active, GUINT_TO_POINTER (FALSE));

  /* First add the locale's charset */
  encoding = g_hash_table_lookup (app->encodings, "current");
  g_assert (encoding);
  if (terminal_encoding_is_valid (encoding))
    encoding->is_active = TRUE;

  /* Also always make UTF-8 available */
  encoding = g_hash_table_lookup (app->encodings, "UTF-8");
  g_assert (encoding);
  if (terminal_encoding_is_valid (encoding))
    encoding->is_active = TRUE;

  g_settings_get (settings, key, "^a&s", &encodings);
  for (i = 0; encodings[i] != NULL; ++i) {
      encoding = terminal_app_ensure_encoding (app, encodings[i]);
      if (!terminal_encoding_is_valid (encoding))
        continue;

      encoding->is_active = TRUE;
    }
  g_free (encodings);

  g_signal_emit (app, signals[ENCODING_LIST_CHANGED], 0);
}

void
terminal_app_new_profile (TerminalApp *app,
                          GSettings   *base_profile,
                          GtkWindow   *transient_parent)
{
  GSettings *new_profile;

  new_profile = profile_clone (app, base_profile);
  terminal_profile_edit (new_profile, transient_parent, "profile-name-entry");
  g_object_unref (new_profile);
}

/* App menu callbacks */

static void
app_menu_preferences_cb (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  TerminalApp *app = user_data;
  GtkWindow *window;
  TerminalScreen *screen;

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (!TERMINAL_IS_WINDOW (window))
    return;

  screen = terminal_window_get_active (TERMINAL_WINDOW (window));
  if (!TERMINAL_IS_SCREEN (screen))
    return;

  terminal_app_edit_profile (app, terminal_screen_get_profile (screen), window, NULL);
}

static void
app_menu_help_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
        GtkApplication *application = user_data;

        terminal_util_show_help (NULL, gtk_application_get_active_window (application));
}

static void
app_menu_about_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GtkApplication *application = user_data;

  terminal_util_show_about (gtk_application_get_active_window (application));
}

/* Class implementation */

G_DEFINE_TYPE (TerminalApp, terminal_app, GTK_TYPE_APPLICATION)

/* GApplicationClass impl */

static void
terminal_app_activate (GApplication *application)
{
  /* No-op required because GApplication is stupid */
}

static void
terminal_app_startup (GApplication *application)
{
  const GActionEntry app_menu_actions[] = {
    { "preferences", app_menu_preferences_cb,   NULL, NULL, NULL },
    { "help",        app_menu_help_cb,          NULL, NULL, NULL },
    { "about",       app_menu_about_cb,         NULL, NULL, NULL }
  };

  GtkBuilder *builder;
  GError *error = NULL;

  G_APPLICATION_CLASS (terminal_app_parent_class)->startup (application);

  /* Need to set the WM class (bug #685742) */
  gdk_set_program_class("Gnome-terminal");

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_menu_actions, G_N_ELEMENTS (app_menu_actions),
                                   application);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder,
                                 TERMINAL_RESOURCES_PATH_PREFIX "ui/terminal-appmenu.ui",
                                 &error);
  g_assert_no_error (error);

  gtk_application_set_app_menu (GTK_APPLICATION (application),
                                G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu")));
  g_object_unref (builder);

  _terminal_debug_print (TERMINAL_DEBUG_SERVER, "Startup complete\n");
}

/* GObjectClass impl */

static void
terminal_app_init (TerminalApp *app)
{
  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);

  /* Desktop proxy settings */
  app->system_proxy_settings = g_settings_new (SYSTEM_PROXY_SETTINGS_SCHEMA);

  /* Desktop Interface settings */
  app->desktop_interface_settings = g_settings_new (DESKTOP_INTERFACE_SETTINGS_SCHEMA);

  /* Terminal global settings */
  app->global_settings = g_settings_new (TERMINAL_SETTING_SCHEMA);

  /* Check if we need to migrate from gconf to dconf */
  maybe_migrate_settings (app);

  /* Get the profiles */
  terminal_app_profile_list_changed_cb (app->global_settings, NULL, app);
  g_signal_connect (app->global_settings,
                    "changed::" TERMINAL_SETTING_PROFILES_KEY,
                    G_CALLBACK (terminal_app_profile_list_changed_cb),
                    app);

  /* Get the encodings */
  app->encodings = terminal_encodings_get_builtins ();
  terminal_app_encoding_list_notify_cb (app->global_settings, "encodings", app);
  g_signal_connect (app->global_settings,
                    "changed::encodings",
                    G_CALLBACK (terminal_app_encoding_list_notify_cb),
                    app);

  terminal_accels_init ();
}

static void
terminal_app_finalize (GObject *object)
{
  TerminalApp *app = TERMINAL_APP (object);

  g_signal_handlers_disconnect_by_func (app->global_settings,
                                        G_CALLBACK (terminal_app_encoding_list_notify_cb),
                                        app);
  g_hash_table_destroy (app->encodings);

  g_signal_handlers_disconnect_by_func (app->global_settings,
                                        G_CALLBACK (terminal_app_profile_list_changed_cb),
                                        app);
  g_hash_table_unref (app->profiles_hash);

  g_object_unref (app->global_settings);
  g_object_unref (app->desktop_interface_settings);
  g_object_unref (app->system_proxy_settings);

  terminal_accels_shutdown ();

  G_OBJECT_CLASS (terminal_app_parent_class)->finalize (object);
}

static gboolean
terminal_app_dbus_register (GApplication    *application,
                            GDBusConnection *connection,
                            const gchar     *object_path,
                            GError         **error)
{
  TerminalApp *app = TERMINAL_APP (application);
  TerminalObjectSkeleton *object;
  TerminalFactory *factory;

  if (!G_APPLICATION_CLASS (terminal_app_parent_class)->dbus_register (application,
                                                                       connection,
                                                                       object_path,
                                                                       error))
    return FALSE;

  object = terminal_object_skeleton_new (TERMINAL_FACTORY_OBJECT_PATH);
  factory = terminal_factory_impl_new ();
  terminal_object_skeleton_set_factory (object, factory);
  g_object_unref (factory);

  app->object_manager = g_dbus_object_manager_server_new (TERMINAL_OBJECT_PATH_PREFIX);
  g_dbus_object_manager_server_export (app->object_manager, G_DBUS_OBJECT_SKELETON (object));
  g_object_unref (object);

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
    app->object_manager = NULL;
  }

  G_APPLICATION_CLASS (terminal_app_parent_class)->dbus_unregister (application,
                                                                    connection,
                                                                    object_path);
}

static void
terminal_app_class_init (TerminalAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *g_application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = terminal_app_finalize;

  g_application_class->activate = terminal_app_activate;
  g_application_class->startup = terminal_app_startup;
  g_application_class->dbus_register = terminal_app_dbus_register;
  g_application_class->dbus_unregister = terminal_app_dbus_unregister;

  signals[PROFILE_LIST_CHANGED] =
    g_signal_new (I_("profile-list-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalAppClass, profile_list_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[ENCODING_LIST_CHANGED] =
    g_signal_new (I_("encoding-list-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalAppClass, profile_list_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/* Public API */

GApplication *
terminal_app_new (const char *app_id)
{
  const GApplicationFlags flags = G_APPLICATION_IS_SERVICE;

  return g_object_new (TERMINAL_TYPE_APP,
                       "application-id", app_id ? app_id : TERMINAL_APPLICATION_ID,
                       "flags", flags,
                       NULL);
}

TerminalWindow *
terminal_app_new_window (TerminalApp *app,
                         GdkScreen *screen)
{
  TerminalWindow *window;

  window = terminal_window_new (G_APPLICATION (app));

  if (screen)
    gtk_window_set_screen (GTK_WINDOW (window), screen);

  return window;
}

TerminalScreen *
terminal_app_new_terminal (TerminalApp     *app,
                           TerminalWindow  *window,
                           GSettings       *profile,
                           char           **override_command,
                           const char      *title,
                           const char      *working_dir,
                           char           **child_env,
                           double           zoom)
{
  TerminalScreen *screen;

  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

  screen = terminal_screen_new (profile, override_command, title,
                                working_dir, child_env, zoom);

  terminal_window_add_screen (window, screen, -1);
  terminal_window_switch_screen (window, screen);
  gtk_widget_grab_focus (GTK_WIDGET (screen));

  /* Launch the child on idle */
  _terminal_screen_launch_child_on_idle (screen);

  return screen;
}

void
terminal_app_edit_profile (TerminalApp     *app,
                           GSettings       *profile,
                           GtkWindow       *transient_parent,
                           const char      *widget_name)
{
  terminal_profile_edit (profile, transient_parent, widget_name);
}

void
terminal_app_edit_preferences (TerminalApp     *app,
                               GtkWindow       *transient_parent)
{
  terminal_prefs_show_preferences (transient_parent, "general");
}

void
terminal_app_edit_encodings (TerminalApp     *app,
                             GtkWindow       *transient_parent)
{
  terminal_prefs_show_preferences (transient_parent, "encodings");
}

/**
 * terminal_profile_get_list:
 *
 * Returns: a #GList containing all profile #GSettings objects.
 *   The content of the list is owned by the backend and
 *   should not be modified or freed. Use g_list_free() when done
 *   using the list.
 */
GList*
terminal_app_get_profile_list (TerminalApp *app)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  return g_list_sort (g_hash_table_get_values (app->profiles_hash), terminal_profile_util_profiles_compare);
}

/**
 * terminal_app_get_profile_by_uuid:
 * @app:
 * @uuid:
 * @error:
 *
 * Returns: (transfer none): the #GSettings for the profile identified by @uuid
 */
GSettings *
terminal_app_ref_profile_by_uuid (TerminalApp *app,
                                  const char  *uuid,
                                  GError **error)
{
  GSettings *profile = NULL;

  if (uuid == NULL)
    g_settings_get (app->global_settings, TERMINAL_SETTING_DEFAULT_PROFILE_KEY, "&s", &uuid);

  profile = g_hash_table_lookup (app->profiles_hash, uuid);

  if (profile == NULL)
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "Profile \"%s\" does not exist", uuid);
      return NULL;
    }

  return g_object_ref (profile);
}

GHashTable *
terminal_app_get_encodings (TerminalApp *app)
{
  return app->encodings;
}

/**
 * terminal_app_ensure_encoding:
 * @app:
 * @charset: (allow-none): a charset, or %NULL
 *
 * Ensures there's a #TerminalEncoding for @charset available. If @charset
 * is %NULL, returns the #TerminalEncoding for the locale's charset. If
 * @charset is not a known charset, returns a #TerminalEncoding for a
 * custom charset.
 *
 * Returns: (transfer none): a #TerminalEncoding
 */
TerminalEncoding *
terminal_app_ensure_encoding (TerminalApp *app,
                              const char *charset)
{
  TerminalEncoding *encoding;

  encoding = g_hash_table_lookup (app->encodings, charset ? charset : "current");
  if (encoding == NULL)
    {
      encoding = terminal_encoding_new (charset,
                                        _("User Defined"),
                                        TRUE,
                                        TRUE /* scary! */);
      g_hash_table_insert (app->encodings,
                          (gpointer) terminal_encoding_get_id (encoding),
                          encoding);
    }

  return encoding;
}

/**
 * terminal_app_get_active_encodings:
 *
 * Returns: a newly allocated list of newly referenced #TerminalEncoding objects.
 */
GSList*
terminal_app_get_active_encodings (TerminalApp *app)
{
  GSList *list = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, app->encodings);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      TerminalEncoding *encoding = (TerminalEncoding *) value;

      if (!encoding->is_active)
        continue;

      list = g_slist_prepend (list, terminal_encoding_ref (encoding));
    }

  return g_slist_sort (list, (GCompareFunc) compare_encodings);
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
  const char *font;

  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  g_settings_get (app->desktop_interface_settings, MONOSPACE_FONT_KEY_NAME, "&s", &font);

  return pango_font_description_from_string (font);
}

/**
 * FIXME
 */
GDBusObjectManagerServer *
terminal_app_get_object_manager (TerminalApp *app)
{
  g_warn_if_fail (app->object_manager != NULL);
  return app->object_manager;
}
