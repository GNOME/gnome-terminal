/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#ifndef WITH_DCONF
#define WITH_DCONF
#endif

#include <glib.h>
#include <gio/gio.h>

#ifdef WITH_DCONF
#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>
#endif

#include "terminal-intl.h"

#include "terminal-debug.h"
#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-screen.h"
#include "terminal-screen-container.h"
#include "terminal-window.h"
#include "terminal-util.h"
#include "profile-editor.h"
#include "terminal-encoding.h"
#include "terminal-schemas.h"
#include "terminal-gdbus.h"
#include "terminal-defines.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef WITH_DCONF
#include <dconf-client.h>
#include <dconf-paths.h>
#endif

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

  void (* quit) (TerminalApp *app);
  void (* profile_list_changed) (TerminalApp *app);
  void (* encoding_list_changed) (TerminalApp *app);
};

struct _TerminalApp
{
  GtkApplication parent_instance;

  GDBusObjectManagerServer *object_manager;

  GList *windows;
  GtkWidget *new_profile_dialog;
  GtkWidget *manage_profiles_dialog;
  GtkWidget *manage_profiles_list;
  GtkWidget *manage_profiles_new_button;
  GtkWidget *manage_profiles_edit_button;
  GtkWidget *manage_profiles_delete_button;
  GtkWidget *manage_profiles_default_menu;

  GHashTable *profiles;

  GHashTable *encodings;
  gboolean encodings_locked;

  GSettings *global_settings;
  GSettings *profiles_settings;
  GSettings *desktop_interface_settings;
  GSettings *system_proxy_settings;

#ifdef WITH_DCONF
  DConfClient *dconf_client;
#endif
};

enum
{
  QUIT,
  PROFILE_LIST_CHANGED,
  ENCODING_LIST_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
  COL_PROFILE,
  NUM_COLUMNS
};

static TerminalApp *global_app = NULL;

static void terminal_app_dconf_get_profile_list (TerminalApp *app);

/* Helper functions */

#if 0
static int
profiles_alphabetic_cmp (gconstpointer pa,
                         gconstpointer pb)
{
  TerminalProfile *a = (TerminalProfile *) pa;
  TerminalProfile *b = (TerminalProfile *) pb;
  int result;

  result =  g_utf8_collate (terminal_profile_get_property_string (a, TERMINAL_PROFILE_VISIBLE_NAME_KEY),
			    terminal_profile_get_property_string (b, TERMINAL_PROFILE_VISIBLE_NAME_KEY));
  if (result == 0)
    result = strcmp (terminal_profile_get_property_string (a, TERMINAL_PROFILE_NAME_KEY),
		     terminal_profile_get_property_string (b, TERMINAL_PROFILE_NAME_KEY));

  return result;
}

typedef struct
{
  TerminalProfile *result;
  const char *target;
} LookupInfo;

static void
profiles_lookup_by_visible_name_foreach (gpointer key,
                                         gpointer value,
                                         gpointer data)
{
  LookupInfo *info = data;
  const char *name;

  name = terminal_profile_get_property_string (value, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
  if (name && strcmp (info->target, name) == 0)
    info->result = value;
}
#endif

static void
terminal_window_destroyed (TerminalWindow *window,
                           TerminalApp    *app)
{
  app->windows = g_list_remove (app->windows, window);

  if (app->windows == NULL)
    g_signal_emit (app, signals[QUIT], 0);
}

#if 0

static TerminalProfile *
terminal_app_create_profile (TerminalApp *app,
                             const char *name)
{
  TerminalProfile *profile;

  g_assert (terminal_app_get_profile_by_name (app, name) == NULL);

  profile = _terminal_profile_new (name);

  g_hash_table_insert (app->profiles,
                       g_strdup (terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME_KEY)),
                       profile /* adopts the refcount */);

  if (app->default_profile == NULL &&
      app->default_profile_id != NULL &&
      strcmp (app->default_profile_id,
              terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME_KEY)) == 0)
    {
      /* We are the default profile */
      app->default_profile = profile;
      g_object_notify (G_OBJECT (app), TERMINAL_APP_DEFAULT_PROFILE);
    }

  return profile;
}

static void
terminal_app_delete_profile (TerminalApp *app,
                             TerminalProfile *profile)
{
  GHashTableIter iter;
  GSList *name_list;
  const char *name, *profile_name;
  char *gconf_dir;
  GError *error = NULL;
  const char **nameptr = &name;

  profile_name = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME_KEY);
  gconf_dir = gconf_concat_dir_and_key (CONF_PREFIX "/profiles", profile_name);

  name_list = NULL;
  g_hash_table_iter_init (&iter, app->profiles);
  while (g_hash_table_iter_next (&iter, (gpointer *) nameptr, NULL))
    {
      if (strcmp (name, profile_name) == 0)
        continue;

      name_list = g_slist_prepend (name_list, g_strdup (name));
    }

  gconf_client_set_list (app->conf,
                         CONF_GLOBAL_PREFIX"/profile_list",
                         GCONF_VALUE_STRING,
                         name_list,
                         NULL);

  g_slist_foreach (name_list, (GFunc) g_free, NULL);
  g_slist_free (name_list);

  /* And remove the profile directory */
  if (!gconf_client_recursive_unset (app->conf, gconf_dir, GCONF_UNSET_INCLUDING_SCHEMA_NAMES, &error))
    {
      g_warning ("Failed to recursively unset %s: %s\n", gconf_dir, error->message);
      g_error_free (error);
    }

  g_free (gconf_dir);
}

static void
terminal_app_profile_cell_data_func (GtkTreeViewColumn *tree_column,
                                     GtkCellRenderer *cell,
                                     GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     gpointer data)
{
  TerminalProfile *profile;
  GValue value = { 0, };

  gtk_tree_model_get (tree_model, iter, (int) COL_PROFILE, &profile, (int) -1);

  g_value_init (&value, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (profile), "visible-name", &value);
  g_object_set_property (G_OBJECT (cell), "text", &value);
  g_value_unset (&value);
}

static int
terminal_app_profile_sort_func (GtkTreeModel *model,
                                GtkTreeIter *a,
                                GtkTreeIter *b,
                                gpointer user_data)
{
  TerminalProfile *profile_a, *profile_b;
  int retval;

  gtk_tree_model_get (model, a, (int) COL_PROFILE, &profile_a, (int) -1);
  gtk_tree_model_get (model, b, (int) COL_PROFILE, &profile_b, (int) -1);

  retval = profiles_alphabetic_cmp (profile_a, profile_b);

  g_object_unref (profile_a);
  g_object_unref (profile_b);

  return retval;
}

static /* ref */ GtkTreeModel *
terminal_app_get_profile_liststore (TerminalApp *app,
                                    TerminalProfile *selected_profile,
                                    GtkTreeIter *selected_profile_iter,
                                    gboolean *selected_profile_iter_set)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *profiles, *l;
  TerminalProfile *default_profile;

  store = gtk_list_store_new (NUM_COLUMNS, TERMINAL_TYPE_PROFILE);

  *selected_profile_iter_set = FALSE;

  if (selected_profile &&
      _terminal_profile_get_forgotten (selected_profile))
    selected_profile = NULL;

  profiles = terminal_app_get_profile_list (app);
  default_profile = terminal_app_get_default_profile (app);

  for (l = profiles; l != NULL; l = l->next)
    {
      TerminalProfile *profile = TERMINAL_PROFILE (l->data);

      gtk_list_store_insert_with_values (store, &iter, 0,
                                         (int) COL_PROFILE, profile,
                                         (int) -1);

      if (selected_profile_iter && profile == selected_profile)
        {
          *selected_profile_iter = iter;
          *selected_profile_iter_set = TRUE;
        }
    }
  g_list_free (profiles);

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                   COL_PROFILE,
                                   terminal_app_profile_sort_func,
                                   NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        COL_PROFILE, GTK_SORT_ASCENDING);

  return GTK_TREE_MODEL (store);
}

static /* ref */ TerminalProfile*
profile_combo_box_get_selected (GtkWidget *widget)
{
  GtkComboBox *combo = GTK_COMBO_BOX (widget);
  TerminalProfile *profile = NULL;
  GtkTreeIter iter;

  if (gtk_combo_box_get_active_iter (combo, &iter))
    gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
                        (int) COL_PROFILE, &profile, (int) -1);

  return profile;
}

static void
profile_combo_box_refill (TerminalApp *app,
                          GtkWidget *widget)
{
  GtkComboBox *combo = GTK_COMBO_BOX (widget);
  GtkTreeIter iter;
  gboolean iter_set;
  TerminalProfile *selected_profile;
  GtkTreeModel *model;

  selected_profile = profile_combo_box_get_selected (widget);
  if (!selected_profile)
    {
      selected_profile = terminal_app_get_default_profile (app);
      if (selected_profile)
        g_object_ref (selected_profile);
    }

  model = terminal_app_get_profile_liststore (app,
                                              selected_profile,
                                              &iter,
                                              &iter_set);
  gtk_combo_box_set_model (combo, model);
  g_object_unref (model);

  if (iter_set)
    gtk_combo_box_set_active_iter (combo, &iter);

  if (selected_profile)
    g_object_unref (selected_profile);
}

static GtkWidget*
profile_combo_box_new (TerminalApp *app)
{
  GtkWidget *combo;
  GtkCellRenderer *renderer;

  combo = gtk_combo_box_new ();
  terminal_util_set_atk_name_description (combo, NULL, _("Click button to choose profile"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer,
                                      (GtkCellLayoutDataFunc) terminal_app_profile_cell_data_func,
                                      NULL, NULL);

  profile_combo_box_refill (app, combo);
  g_signal_connect (app, "profile-list-changed",
                    G_CALLBACK (profile_combo_box_refill), combo);

  gtk_widget_show (combo);
  return combo;
}

static void
profile_combo_box_changed_cb (GtkWidget *widget,
                              TerminalApp *app)
{
  TerminalProfile *profile;

  profile = profile_combo_box_get_selected (widget);
  if (!profile)
    return;

  gconf_client_set_string (app->conf,
                           CONF_GLOBAL_PREFIX "/default_profile",
                           terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME_KEY),
                           NULL);

  /* Even though the gconf change notification does this, it happens too late.
   * In some cases, the default profile changes twice in quick succession,
   * and update_default_profile must be called in sync with those changes.
   */
  app->default_profile = profile;

  g_object_notify (G_OBJECT (app), TERMINAL_APP_DEFAULT_PROFILE);

  g_object_unref (profile);
}

static void
profile_list_treeview_refill (TerminalApp *app,
                              GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeIter iter;
  gboolean iter_set;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  TerminalProfile *selected_profile = NULL;

  model = gtk_tree_view_get_model (tree_view);

  selection = gtk_tree_view_get_selection (tree_view);
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

  model = terminal_app_get_profile_liststore (terminal_app_get (),
                                              selected_profile,
                                              &iter,
                                              &iter_set);
  gtk_tree_view_set_model (tree_view, model);
  g_object_unref (model);

  if (!iter_set)
    iter_set = gtk_tree_model_get_iter_first (model, &iter);

  if (iter_set)
    gtk_tree_selection_select_iter (selection, &iter);

  if (selected_profile)
    g_object_unref (selected_profile);
}

static GtkWidget*
profile_list_treeview_create (TerminalApp *app)
{
  GtkWidget *tree_view;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  tree_view = gtk_tree_view_new ();
  terminal_util_set_atk_name_description (tree_view, _("Profile list"), NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
                               GTK_SELECTION_BROWSE);

  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer,
                                      (GtkCellLayoutDataFunc) terminal_app_profile_cell_data_func,
                                      NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                               GTK_TREE_VIEW_COLUMN (column));

  return tree_view;
}

static void
profile_list_delete_confirm_response_cb (GtkWidget *dialog,
                                         int response,
                                         TerminalApp *app)
{
  TerminalProfile *profile;

  profile = TERMINAL_PROFILE (g_object_get_data (G_OBJECT (dialog), "profile"));
  g_assert (profile != NULL);

  if (response == GTK_RESPONSE_ACCEPT)
    terminal_app_delete_profile (app, profile);

  gtk_widget_destroy (dialog);
}

static void
profile_list_delete_button_clicked_cb (GtkWidget *button,
                                       GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  TerminalApp *app = terminal_app_get ();
  GtkTreeSelection *selection;
  GtkWidget *dialog;
  GtkTreeIter iter;
  GtkTreeModel *model;
  TerminalProfile *selected_profile;
  GtkWidget *transient_parent;

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

  transient_parent = gtk_widget_get_toplevel (widget);
  dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Delete profile “%s”?"),
                                   terminal_profile_get_property_string (selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY));

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL,
                          GTK_RESPONSE_REJECT,
                          GTK_STOCK_DELETE,
                          GTK_RESPONSE_ACCEPT,
                          NULL);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_REJECT,
                                           -1);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);

  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete Profile"));
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* Transfer refcount of |selected_profile|, so no unref below */
  g_object_set_data_full (G_OBJECT (dialog), "profile", selected_profile, g_object_unref);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (profile_list_delete_confirm_response_cb),
                    app);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
profile_list_new_button_clicked_cb (GtkWidget   *button,
                                    gpointer data)
{
  TerminalApp *app;

  app = terminal_app_get ();
  terminal_app_new_profile (app, NULL, GTK_WINDOW (app->manage_profiles_dialog));
}

static void
profile_list_edit_button_clicked_cb (GtkWidget *button,
                                     GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreeModel *model;
  TerminalProfile *selected_profile;
  TerminalApp *app;

  app = terminal_app_get ();

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

  terminal_app_edit_profile (app, selected_profile,
                             GTK_WINDOW (app->manage_profiles_dialog),
                             NULL);
  g_object_unref (selected_profile);
}

static void
profile_list_row_activated_cb (GtkTreeView       *tree_view,
                               GtkTreePath       *path,
                               GtkTreeViewColumn *column,
                               gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  TerminalProfile *selected_profile;
  TerminalApp *app;

  app = terminal_app_get ();

  model = gtk_tree_view_get_model (tree_view);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

  terminal_app_edit_profile (app, selected_profile,
                             GTK_WINDOW (app->manage_profiles_dialog),
                             NULL);
  g_object_unref (selected_profile);
}

static GList*
find_profile_link (GList      *profiles,
                   const char *name)
{
  GList *l;

  for (l = profiles; l != NULL; l = l->next)
    {
      const char *profile_name;

      profile_name = terminal_profile_get_property_string (TERMINAL_PROFILE (l->data), TERMINAL_PROFILE_NAME_KEY);
      if (profile_name && strcmp (profile_name, name) == 0)
        break;
    }

  return l;
}

#endif /* 0 */

static void
terminal_app_ensure_any_profiles (TerminalApp *app)
{
  /* Make sure we do have at least one profile */
  if (g_hash_table_size (app->profiles) != 0)
    return;

  g_hash_table_insert (app->profiles, 
                       g_strdup (TERMINAL_DEFAULT_PROFILE_ID),
                       g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, TERMINAL_DEFAULT_PROFILE_PATH));
}

#ifdef WITH_DCONF

static void
terminal_app_dconf_get_profile_list (TerminalApp *app)
{
  char **keys;
  int n_keys, i;

  keys = dconf_client_list (app->dconf_client, TERMINAL_PROFILES_PATH_PREFIX, &n_keys);
  for (i = 0; i < n_keys; i++) {
    const char *key = keys[i];
    char *path, *id;
    GSettings *profile;

    //g_print ("key %s\n", key);
    if (!dconf_is_rel_dir (key, NULL))
      continue;
    /* For future-compat with GSettingsList */
    if (key[0] != ':')
      continue;

    path = g_strconcat (TERMINAL_PROFILES_PATH_PREFIX, key, NULL);
    profile = g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, path);
    //g_print ("new profile %p id %s with path %s\n", profile, key, path);
    g_free (path);

    id = g_strdup (key);
    id[strlen (id) - 1] = '\0';
    g_hash_table_insert (app->profiles, id /* adopts */, profile /* adopts */);
  }
  g_strfreev (keys);

  terminal_app_ensure_any_profiles (app);
}

#endif /* WITH_DCONF */

#if 0
static void
terminal_app_profiles_children_changed_cb (GSettings   *settings,
                                           TerminalApp *app)
{
  GObject *object = G_OBJECT (app);
  char **profile_names;
  gpointer *new_profiles;
  guint i, n_profile_names;

  g_object_freeze_notify (object);

  profile_names = g_settings_list_children (settings);
  n_profile_names = g_strv_length (profile_names);
  /* There's always going to at least the the 'default' child */
  g_assert (g_strv_length (profile_names) >= 1);

  new_profiles = g_newa (gpointer, n_profile_names);
  for (i = 0; i < n_profile_names; ++i)
    {
      const char *profile_name = profile_names[i];
      GSettings *profile;

      profile = g_hash_table_lookup (app->profiles, profile_name);
      if (profile != NULL)
        new_profiles[i] = g_object_ref (profile);
      else
        new_profiles[i] = g_settings_get_child (settings, profile_name);
    }

  g_hash_table_remove_all (app->profiles);
  for (i = 0; i < n_profile_names; ++i)
    if (new_profiles[i] != NULL)
      g_hash_table_insert (app->profiles,
                           g_strdup (profile_names[i]),
                           new_profiles[i] /* adopted */);

  g_strfreev (profile_names);

  g_assert (g_hash_table_size (app->profiles) >= 1);

  // FIXME: re-set profile on any tabs having a profile NOT in the new list?
  // or just continue to use the now-defunct ones?

  g_signal_emit (app, signals[PROFILE_LIST_CHANGED], 0);

  g_object_thaw_notify (object);
}

#endif /* 0 */

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

#if 0
static void
new_profile_response_cb (GtkWidget *new_profile_dialog,
                         int        response_id,
                         TerminalApp *app)
{
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkWidget *name_entry;
      char *name;
      const char *new_profile_name;
      GtkWidget *base_option_menu;
      TerminalProfile *base_profile = NULL;
      TerminalProfile *new_profile;
      GList *profiles;
      GList *tmp;
      GtkWindow *transient_parent;
      GtkWidget *confirm_dialog;
      gint retval;
      GSList *list;

      base_option_menu = g_object_get_data (G_OBJECT (new_profile_dialog), "base_option_menu");
      base_profile = profile_combo_box_get_selected (base_option_menu);
      if (!base_profile)
        base_profile = terminal_app_get_default_profile (app);
      if (!base_profile)
        return; /* shouldn't happen ever though */

      name_entry = g_object_get_data (G_OBJECT (new_profile_dialog), "name_entry");
      name = gtk_editable_get_chars (GTK_EDITABLE (name_entry), 0, -1);
      g_strstrip (name); /* name will be non empty after stripping */

      profiles = terminal_app_get_profile_list (app);
      for (tmp = profiles; tmp != NULL; tmp = tmp->next)
        {
          TerminalProfile *profile = tmp->data;
          const char *visible_name;

          visible_name = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

          if (visible_name && strcmp (name, visible_name) == 0)
            break;
        }
      if (tmp)
        {
          confirm_dialog = gtk_message_dialog_new (GTK_WINDOW (new_profile_dialog),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_QUESTION,
                                                   GTK_BUTTONS_YES_NO,
                                                   _("You already have a profile called “%s”. Do you want to create another profile with the same name?"), name);
          /* Alternative button order was set automatically by GtkMessageDialog */
          retval = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
          gtk_widget_destroy (confirm_dialog);
          if (retval == GTK_RESPONSE_NO)
            goto cleanup;
        }
      g_list_free (profiles);

      transient_parent = gtk_window_get_transient_for (GTK_WINDOW (new_profile_dialog));

      new_profile = _terminal_profile_clone (base_profile, name);
      new_profile_name = terminal_profile_get_property_string (new_profile, TERMINAL_PROFILE_NAME_KEY);
      g_hash_table_insert (app->profiles,
                           g_strdup (new_profile_name),
                           new_profile /* adopts the refcount */);

      /* And now save the list to gconf */
      list = gconf_client_get_list (app->conf,
                                    CONF_GLOBAL_PREFIX"/profile_list",
                                    GCONF_VALUE_STRING,
                                    NULL);
      list = g_slist_append (list, g_strdup (new_profile_name));
      gconf_client_set_list (app->conf,
                             CONF_GLOBAL_PREFIX"/profile_list",
                             GCONF_VALUE_STRING,
                             list,
                             NULL);

      terminal_profile_edit (new_profile, transient_parent, NULL);

    cleanup:
      g_free (name);
    }

  gtk_widget_destroy (new_profile_dialog);
}

static void
new_profile_dialog_destroy_cb (GtkWidget *new_profile_dialog,
                               TerminalApp *app)
{
  GtkWidget *combo;

  combo = g_object_get_data (G_OBJECT (new_profile_dialog), "base_option_menu");
  g_signal_handlers_disconnect_by_func (app, G_CALLBACK (profile_combo_box_refill), combo);

  app->new_profile_dialog = NULL;
}

static void
new_profile_name_entry_changed_cb (GtkEntry *entry,
                                   GtkDialog *dialog)
{
  const char *name;

  name = gtk_entry_get_text (entry);

  /* make the create button sensitive only if something other than space has been set */
  while (*name != '\0' && g_ascii_isspace (*name))
    ++name;

  gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT, name[0] != '\0');
}

#endif

void
terminal_app_new_profile (TerminalApp *app,
                          GSettings   *default_base_profile,
                          GtkWindow   *transient_parent)
{
#if 0
  if (app->new_profile_dialog == NULL)
    {
      GtkWidget *create_button, *table, *name_label, *name_entry, *base_label, *combo;

      if (!terminal_util_load_builder_file ("profile-new-dialog.ui",
                                            "new-profile-dialog", &app->new_profile_dialog,
                                            "new-profile-create-button", &create_button,
                                            "new-profile-table", &table,
                                            "new-profile-name-label", &name_label,
                                            "new-profile-name-entry", &name_entry,
                                            "new-profile-base-label", &base_label,
                                            NULL))
        return;

      g_signal_connect (G_OBJECT (app->new_profile_dialog), "response", G_CALLBACK (new_profile_response_cb), app);
      g_signal_connect (app->new_profile_dialog, "destroy", G_CALLBACK (new_profile_dialog_destroy_cb), app);

      g_object_set_data (G_OBJECT (app->new_profile_dialog), "create_button", create_button);
      gtk_widget_set_sensitive (create_button, FALSE);

      /* the name entry */
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "name_entry", name_entry);
      g_signal_connect (name_entry, "changed", G_CALLBACK (new_profile_name_entry_changed_cb), app->new_profile_dialog);
      gtk_entry_set_activates_default (GTK_ENTRY (name_entry), TRUE);
      gtk_widget_grab_focus (name_entry);

      gtk_label_set_mnemonic_widget (GTK_LABEL (name_label), name_entry);

      /* the base profile option menu */
      combo = profile_combo_box_new (app);
      gtk_table_attach_defaults (GTK_TABLE (table), combo, 1, 2, 1, 2);
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "base_option_menu", combo);
      terminal_util_set_atk_name_description (combo, NULL, _("Choose base profile"));

      gtk_label_set_mnemonic_widget (GTK_LABEL (base_label), combo);

      gtk_dialog_set_alternative_button_order (GTK_DIALOG (app->new_profile_dialog),
                                               GTK_RESPONSE_ACCEPT,
                                               GTK_RESPONSE_CANCEL,
                                               -1);
      gtk_dialog_set_default_response (GTK_DIALOG (app->new_profile_dialog), GTK_RESPONSE_ACCEPT);
      gtk_dialog_set_response_sensitive (GTK_DIALOG (app->new_profile_dialog), GTK_RESPONSE_ACCEPT, FALSE);
    }

  gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
                                transient_parent);

  gtk_window_present (GTK_WINDOW (app->new_profile_dialog));
#endif
}

#if 0
static void
profile_list_selection_changed_cb (GtkTreeSelection *selection,
                                   TerminalApp *app)
{
  gboolean selected;

  selected = gtk_tree_selection_get_selected (selection, NULL, NULL);

  gtk_widget_set_sensitive (app->manage_profiles_edit_button, selected);
  gtk_widget_set_sensitive (app->manage_profiles_delete_button,
                            selected &&
                            g_hash_table_size (app->profiles) > 1);
}

static void
profile_list_response_cb (GtkWidget *dialog,
                          int        id,
                          TerminalApp *app)
{
  g_assert (app->manage_profiles_dialog == dialog);

  if (id == GTK_RESPONSE_HELP)
    {
      terminal_util_show_help ("gnome-terminal-manage-profiles", GTK_WINDOW (dialog));
      return;
    }

  gtk_widget_destroy (dialog);
}

static void
profile_list_destroyed_cb (GtkWidget   *manage_profiles_dialog,
                           TerminalApp *app)
{
  g_signal_handlers_disconnect_by_func (app, G_CALLBACK (profile_list_treeview_refill), app->manage_profiles_list);
  g_signal_handlers_disconnect_by_func (app, G_CALLBACK (profile_combo_box_refill), app->manage_profiles_default_menu);

  app->manage_profiles_dialog = NULL;
  app->manage_profiles_list = NULL;
  app->manage_profiles_new_button = NULL;
  app->manage_profiles_edit_button = NULL;
  app->manage_profiles_delete_button = NULL;
  app->manage_profiles_default_menu = NULL;
}
#endif

void
terminal_app_manage_profiles (TerminalApp     *app,
                              GtkWindow       *transient_parent)
{
#if 0
  GObject *dialog;
  GObject *tree_view_container, *new_button, *edit_button, *remove_button;
  GObject *default_hbox, *default_label;
  GtkTreeSelection *selection;

  if (app->manage_profiles_dialog)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->manage_profiles_dialog), transient_parent);
      gtk_window_present (GTK_WINDOW (app->manage_profiles_dialog));
      return;
    }

  if (!terminal_util_load_builder_file ("profile-manager.ui",
                                        "profile-manager", &dialog,
                                        "profiles-treeview-container", &tree_view_container,
                                        "new-profile-button", &new_button,
                                        "edit-profile-button", &edit_button,
                                        "delete-profile-button", &remove_button,
                                        "default-profile-hbox", &default_hbox,
                                        "default-profile-label", &default_label,
                                        NULL))
    return;

  app->manage_profiles_dialog = GTK_WIDGET (dialog);
  app->manage_profiles_new_button = GTK_WIDGET (new_button);
  app->manage_profiles_edit_button = GTK_WIDGET (edit_button);
  app->manage_profiles_delete_button  = GTK_WIDGET (remove_button);

  g_signal_connect (dialog, "response", G_CALLBACK (profile_list_response_cb), app);
  g_signal_connect (dialog, "destroy", G_CALLBACK (profile_list_destroyed_cb), app);

  app->manage_profiles_list = profile_list_treeview_create (app);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->manage_profiles_list));
  g_signal_connect (selection, "changed", G_CALLBACK (profile_list_selection_changed_cb), app);

  profile_list_treeview_refill (app, app->manage_profiles_list);
  g_signal_connect (app, "profile-list-changed",
                    G_CALLBACK (profile_list_treeview_refill), app->manage_profiles_list);

  g_signal_connect (app->manage_profiles_list, "row-activated",
                    G_CALLBACK (profile_list_row_activated_cb), app);

  gtk_container_add (GTK_CONTAINER (tree_view_container), app->manage_profiles_list);
  gtk_widget_show (app->manage_profiles_list);

  g_signal_connect (new_button, "clicked",
                    G_CALLBACK (profile_list_new_button_clicked_cb),
                    app->manage_profiles_list);
  g_signal_connect (edit_button, "clicked",
                    G_CALLBACK (profile_list_edit_button_clicked_cb),
                    app->manage_profiles_list);
  g_signal_connect (remove_button, "clicked",
                    G_CALLBACK (profile_list_delete_button_clicked_cb),
                    app->manage_profiles_list);

  app->manage_profiles_default_menu = profile_combo_box_new (app);
  g_signal_connect (app->manage_profiles_default_menu, "changed",
                    G_CALLBACK (profile_combo_box_changed_cb), app);

  gtk_box_pack_start (GTK_BOX (default_hbox), app->manage_profiles_default_menu, FALSE, FALSE, 0);
  gtk_widget_show (app->manage_profiles_default_menu);

  gtk_label_set_mnemonic_widget (GTK_LABEL (default_label), app->manage_profiles_default_menu);

  gtk_widget_grab_focus (app->manage_profiles_list);

  gtk_window_set_transient_for (GTK_WINDOW (app->manage_profiles_dialog),
                                transient_parent);

  gtk_window_present (GTK_WINDOW (app->manage_profiles_dialog));
#endif
}

/* App menu callbacks */

static void
app_menu_preferences_cb (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  TerminalApp *app = user_data;

  terminal_app_edit_profile (app,
                             terminal_app_get_profile_by_name (app, TERMINAL_DEFAULT_PROFILE_ID) /* FIXME */,
                             NULL /* FIXME use last active window? */,
                             NULL);
}

static void
app_menu_help_cb (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  terminal_util_show_help (NULL, NULL /* FIXME use last active window? */);
}

static void
app_menu_about_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  terminal_util_show_about (NULL /* FIXME use last active window? */);
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

  TerminalApp *app = TERMINAL_APP (application);
  GtkBuilder *builder;
  GError *error = NULL;

  G_APPLICATION_CLASS (terminal_app_parent_class)->startup (application);

  /* FIXME: Is this the right place to do prefs migration from gconf->dconf? */

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

  g_print ("Done startup!\n");
}

/* GObjectClass impl */

static void
terminal_app_init (TerminalApp *app)
{
  global_app = app;

  gtk_window_set_default_icon_name (GNOME_TERMINAL_ICON_NAME);

  /* Desktop proxy settings */
  app->system_proxy_settings = g_settings_new (SYSTEM_PROXY_SETTINGS_SCHEMA);

  /* Desktop Interface settings */
  app->desktop_interface_settings = g_settings_new (DESKTOP_INTERFACE_SETTINGS_SCHEMA);

  /* Terminal global settings */
  app->global_settings = g_settings_new (TERMINAL_SETTING_SCHEMA);

  app->encodings = terminal_encodings_get_builtins ();
  terminal_app_encoding_list_notify_cb (app->global_settings, "encodings", app);
  g_signal_connect (app->global_settings,
                    "changed::encodings",
                    G_CALLBACK (terminal_app_encoding_list_notify_cb),
                    app);

  app->profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

#ifdef WITH_DCONF
{
  GSettingsBackend *backend;

  /* FIXME HACK! */
  backend = g_settings_backend_get_default ();
  if (strcmp (G_OBJECT_TYPE_NAME (backend), "DConfSettingsBackend") == 0) {
    app->dconf_client = dconf_client_new (NULL, NULL, NULL, NULL);
    terminal_app_dconf_get_profile_list (app);
  }
  g_object_unref (backend);
}
#endif
#if 0
  app->profiles_settings = g_settings_new (PROFILES_SETTINGS_SCHEMA_ID);
  terminal_app_profiles_children_changed_cb (app->profiles_settings, app);
  g_signal_connect (app->profiles_settings,
                    "children-changed",
                    G_CALLBACK (terminal_app_profiles_children_changed_cb),
                    app);
#endif

  terminal_app_ensure_any_profiles (app);

  terminal_accels_init ();

  /* FIXMEchpe: find out why this is necessary... */
  g_application_hold (G_APPLICATION (app));
}

static void
terminal_app_finalize (GObject *object)
{
  TerminalApp *app = TERMINAL_APP (object);

#ifdef WITH_DCONF
  g_clear_object (&app->dconf_client);
#endif

  g_hash_table_destroy (app->encodings);
  g_signal_handlers_disconnect_by_func (app->global_settings,
                                        G_CALLBACK (terminal_app_encoding_list_notify_cb),
                                        app);

  g_hash_table_destroy (app->profiles);
#if 0
  g_signal_handlers_disconnect_by_func (app->profiles_settings,
                                        G_CALLBACK (terminal_app_profiles_children_changed_cb),
                                        app);
  g_object_unref (app->profiles_settings);
#endif

  g_object_unref (app->global_settings);
  g_object_unref (app->desktop_interface_settings);
  g_object_unref (app->system_proxy_settings);

  terminal_accels_shutdown ();

  if (app->object_manager) {
    g_dbus_object_manager_server_unexport (app->object_manager, TERMINAL_FACTORY_OBJECT_PATH);
    g_object_unref (app->object_manager);
  }

  G_OBJECT_CLASS (terminal_app_parent_class)->finalize (object);

  global_app = NULL;
}

static void
terminal_app_real_quit (TerminalApp *app)
{
  /* Release the hold added when creating the app  */
  g_application_release (G_APPLICATION (app));
}

static void
terminal_app_class_init (TerminalAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *g_application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = terminal_app_finalize;

  g_application_class->activate = terminal_app_activate;
  g_application_class->startup = terminal_app_startup;

  klass->quit = terminal_app_real_quit;

  signals[QUIT] =
    g_signal_new (I_("quit"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalAppClass, quit),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

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
terminal_app_new (const char *id)
{
  return g_object_new (TERMINAL_TYPE_APP,
                       "application-id", id,
                       "flags", (glong) (G_APPLICATION_NON_UNIQUE | G_APPLICATION_IS_SERVICE),
                       NULL);
}

TerminalApp*
terminal_app_get (void)
{
  g_assert (global_app != NULL);
  g_assert (global_app != NULL);
  return global_app;
}

void
terminal_app_shutdown (void)
{
  if (global_app == NULL)
    return;

  g_object_unref (global_app);
  g_assert (global_app == NULL);
}

TerminalWindow *
terminal_app_new_window (TerminalApp *app,
                         GdkScreen *screen)
{
  TerminalWindow *window;

  window = terminal_window_new (G_APPLICATION (app));

  app->windows = g_list_append (app->windows, window);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (terminal_window_destroyed), app);

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
terminal_app_edit_keybindings (TerminalApp     *app,
                               GtkWindow       *transient_parent)
{
  terminal_edit_keys_dialog_show (transient_parent);
}

void
terminal_app_edit_encodings (TerminalApp     *app,
                             GtkWindow       *transient_parent)
{
  terminal_encoding_dialog_show (transient_parent);
}

TerminalWindow *
terminal_app_get_current_window (TerminalApp *app)
{
  if (app->windows == NULL)
    return NULL;

  return g_list_last (app->windows)->data;
}

/**
 * terminal_profile_get_list:
 *
 * Returns: a #GList containing all #TerminalProfile objects.
 *   The content of the list is owned by the backend and
 *   should not be modified or freed. Use g_list_free() when done
 *   using the list.
 */
GList*
terminal_app_get_profile_list (TerminalApp *app)
{
#if 0
  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  return g_list_sort (g_hash_table_get_values (app->profiles), profiles_alphabetic_cmp);
#endif
return NULL;
}

/**
 * terminal_app_get_profile_by_name:
 * @app:
 * @name:
 *
 * Returns: (transfer full): a new #GSettings for the profile schema, or %NULL
 */
GSettings *
terminal_app_get_profile_by_name (TerminalApp *app,
                                  const char *name)
{
  GSettings *profile;

  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  profile = g_hash_table_lookup (app->profiles, name);
  if (profile != NULL)
    return g_object_ref (profile);

  profile = g_settings_get_child (app->profiles_settings, name);
  if (profile != NULL)
    g_hash_table_insert (app->profiles, g_strdup (name), g_object_ref (profile));

  return profile;
}

GSettings*
terminal_app_get_profile_by_visible_name (TerminalApp *app,
                                          const char *name)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  // FIXMEchpe re-implement, or drop?
  return NULL;
}

/**
 * FIXME
 */
GSettings* terminal_app_get_profile (TerminalApp *app,
                                     const char  *profile_name)
{
  GSettings *profile = NULL;

  if (profile_name)
    {
      if (TRUE /* profile_is_id */)
        profile = terminal_app_get_profile_by_name (app, profile_name);
      else
        profile = terminal_app_get_profile_by_visible_name (app, profile_name);

      if (profile == NULL)
        _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                               "No such profile \"%s\", using default profile", 
                               profile_name);
    }
  if (profile == NULL)
    profile = g_object_ref (g_hash_table_lookup (app->profiles, TERMINAL_DEFAULT_PROFILE_ID));

  g_assert (profile != NULL);
  return profile;
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

#include "terminal-options.h"

void
terminal_app_save_config (TerminalApp *app,
                          GKeyFile *key_file)
{
  GList *lw;
  guint n = 0;
  GPtrArray *window_names_array;
  char **window_names;
  gsize len;

  g_key_file_set_comment (key_file, NULL, NULL, "Written by " PACKAGE_STRING, NULL);

  g_key_file_set_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_VERSION, TERMINAL_CONFIG_VERSION);
  g_key_file_set_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_COMPAT_VERSION, TERMINAL_CONFIG_COMPAT_VERSION);

  window_names_array = g_ptr_array_sized_new (g_list_length (app->windows) + 1);

  for (lw = app->windows; lw != NULL; lw = lw->next)
    {
      TerminalWindow *window = TERMINAL_WINDOW (lw->data);
      char *group;

      group = g_strdup_printf ("Window%u", n++);
      g_ptr_array_add (window_names_array, group);

      terminal_window_save_state (window, key_file, group);
    }

  len = window_names_array->len;
  g_ptr_array_add (window_names_array, NULL);
  window_names = (char **) g_ptr_array_free (window_names_array, FALSE);
  g_key_file_set_string_list (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_WINDOWS, (const char * const *) window_names, len);
  g_strfreev (window_names);
}

gboolean
terminal_app_save_config_file (TerminalApp *app,
                               const char *file_name,
                               GError **error)
{
  GKeyFile *key_file;
  char *data;
  gsize len;
  gboolean result;

  key_file = g_key_file_new ();
  terminal_app_save_config (app, key_file);

  data = g_key_file_to_data (key_file, &len, NULL);
  result = g_file_set_contents (file_name, data, len, error);
  g_free (data);

  return result;
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
  if (app->object_manager == NULL)
    app->object_manager = g_dbus_object_manager_server_new (TERMINAL_OBJECT_PATH_PREFIX);
  return app->object_manager;
}
