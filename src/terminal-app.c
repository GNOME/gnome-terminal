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
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "terminal-intl.h"
#include "terminal-debug.h"
#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-screen.h"
#include "terminal-screen-container.h"
#include "terminal-window.h"
#include "terminal-profiles-list.h"
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

  void (* encoding_list_changed) (TerminalApp *app);
};

struct _TerminalApp
{
  GtkApplication parent_instance;

  GDBusObjectManagerServer *object_manager;

  TerminalSettingsList *profiles_list;

  GHashTable *encodings;
  gboolean encodings_locked;

  GSettings *global_settings;
  GSettings *desktop_interface_settings;
  GSettings *system_proxy_settings;
};

enum
{
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
  int status;
  GError *error = NULL;
#endif /* ENABLE_MIGRATION */
  guint version;

  version = g_settings_get_uint (terminal_app_get_global_settings (app), TERMINAL_SETTING_SCHEMA_VERSION);
  if (version >= TERMINAL_SCHEMA_VERSION) {
     _terminal_debug_print (TERMINAL_DEBUG_SERVER | TERMINAL_DEBUG_PROFILE,
                            "Schema version is %d, already migrated.\n", version);
    return;
  }

#ifdef ENABLE_MIGRATION
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

void
terminal_app_new_profile (TerminalApp *app,
                          GSettings   *base_profile,
                          GtkWindow   *transient_parent)
{
  GSettings *profile;
  char *base_uuid, *uuid;

  if (base_profile) {
    base_uuid = terminal_settings_list_dup_uuid_from_child (app->profiles_list, base_profile);
    uuid = terminal_settings_list_clone_child (app->profiles_list, base_uuid);
    g_free (base_uuid);
  } else {
    uuid = terminal_settings_list_add_child (app->profiles_list);
  }

  if (uuid == NULL)
    return;

  profile = terminal_settings_list_ref_child (app->profiles_list, uuid);
  g_free (uuid);
  if (profile == NULL)
    return;

  terminal_profile_edit (profile, transient_parent, "profile-name-entry");
  g_object_unref (profile);
}

void
terminal_app_remove_profile (TerminalApp *app,
                             GSettings *profile)
{
  char *uuid;

  uuid = terminal_settings_list_dup_uuid_from_child (app->profiles_list, profile);
  terminal_settings_list_remove_child (app->profiles_list, uuid);
  g_free (uuid);
}

gboolean
terminal_app_can_remove_profile (TerminalApp *app,
                                 GSettings *profile)
{
  return TRUE;
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

/* App menu callbacks */

static void
app_menu_preferences_cb (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  TerminalApp *app = user_data;
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (!TERMINAL_IS_WINDOW (window))
    return;

  terminal_app_edit_preferences (app, window);
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
  GSettings *settings;

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
  app->profiles_list = terminal_profiles_list_new ();

  /* Get the encodings */
  app->encodings = terminal_encodings_get_builtins ();
  terminal_app_encoding_list_notify_cb (app->global_settings, "encodings", app);
  g_signal_connect (app->global_settings,
                    "changed::encodings",
                    G_CALLBACK (terminal_app_encoding_list_notify_cb),
                    app);

  settings = g_settings_get_child (app->global_settings, "keybindings");
  terminal_accels_init (settings);
  g_object_unref (settings);
}

static void
terminal_app_finalize (GObject *object)
{
  TerminalApp *app = TERMINAL_APP (object);

  g_signal_handlers_disconnect_by_func (app->global_settings,
                                        G_CALLBACK (terminal_app_encoding_list_notify_cb),
                                        app);
  g_hash_table_destroy (app->encodings);

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

  signals[ENCODING_LIST_CHANGED] =
    g_signal_new (I_("encoding-list-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalAppClass, encoding_list_changed),
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
 * terminal_app_get_profiles_list:
 *
 * Returns: (transfer none): returns the singleton profiles list #TerminalSettingsList
 */
TerminalSettingsList *
terminal_app_get_profiles_list (TerminalApp *app)
{
  return app->profiles_list;
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
