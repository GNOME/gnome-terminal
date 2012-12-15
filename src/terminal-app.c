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

#include <glib.h>
#include <gio/gio.h>

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

#include <uuid.h>

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
  GtkWidget *manage_profiles_dialog;
  GtkWidget *manage_profiles_list;
  GtkWidget *manage_profiles_new_button;
  GtkWidget *manage_profiles_edit_button;
  GtkWidget *manage_profiles_clone_button;
  GtkWidget *manage_profiles_delete_button;
  GtkWidget *manage_profiles_default_menu;

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

enum
{
  COL_PROFILE,
  NUM_COLUMNS
};

/* Helper functions */

static void
maybe_migrate_settings (TerminalApp *app)
{
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
     _terminal_debug_print (TERMINAL_DEBUG_FACTORY | TERMINAL_DEBUG_PROFILE,
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
  guint i;

  for (i = 0; strv[i]; i++) {
    if (strcmp (strv[i], str) != 0)
      continue;

    for ( ; strv[i]; i++)
      strv[i] = strv[i+1];
    strv[i-1] = NULL;
  }

  return strv;
}

static char *
profile_get_uuid (GSettings *profile)
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

static int
profiles_alphabetic_cmp (gconstpointer pa,
                         gconstpointer pb)
{
  GSettings *a = (GSettings *) pa;
  GSettings *b = (GSettings *) pb;
  const char *na, *nb;
  char *patha, *pathb;
  int result;

  if (pa == pb)
    return 0;
  if (pa == NULL)
    return 1;
  if (pb == NULL)
    return -1;

  g_settings_get (a, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &na);
  g_settings_get (b, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &nb);
  result =  g_utf8_collate (na, nb);
  if (result != 0)
    return result;

  g_object_get (a, "path", &patha, NULL);
  g_object_get (b, "path", &pathb, NULL);
  result = strcmp (patha, pathb);
  g_free (patha);
  g_free (pathb);

  return result;
}

static GSettings * /* ref */
profile_clone (TerminalApp *app,
               GSettings *base_profile G_GNUC_UNUSED,//FIXME
               char *visible_name)
{
  GSettings *profile;
  uuid_t u;
  char str[37];
  char *path;
  char **profiles;

  uuid_generate (u);
  uuid_unparse (u, str);
  path = g_strdup_printf (TERMINAL_PROFILES_PATH_PREFIX ":%s/", str);
  profile = g_settings_new_with_path (TERMINAL_PROFILE_SCHEMA, path);
  g_free (path);

  g_settings_set_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, visible_name);

  /* Store the new UUID in the list of profiles, and add the profile to the hash table.
   * We'll get a changed signal for the profile list key, but that will result in a no-op.
   */
  g_hash_table_insert (app->profiles_hash, g_strdup (str) /* adopted */, profile /* adopted */);

  g_settings_get (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, "^a&s", &profiles);
  profiles = strv_insert (profiles, str);
  g_settings_set_strv (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, (const char * const *) profiles);
  g_free (profiles);

  return g_object_ref (profile);
}

static void
profile_remove (TerminalApp *app,
                GSettings *profile)
{
  char *uuid;
  char **profiles;

  uuid = profile_get_uuid (profile);

  g_settings_get (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, "^a&s", &profiles);
  profiles = strv_remove (profiles, uuid);
  g_settings_set_strv (app->global_settings, TERMINAL_SETTING_PROFILES_KEY, (const char * const *) profiles);
  g_free (profiles);

  g_free (uuid);

  /* FIXME: recursively unset all keys under the profile's path? */
}

static void
terminal_app_profile_cell_data_func (GtkTreeViewColumn *tree_column,
                                     GtkCellRenderer *cell,
                                     GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     gpointer data)
{
  GSettings *profile;
  const char *text;
  char *uuid;
  GValue value = { 0, };

  gtk_tree_model_get (tree_model, iter, (int) COL_PROFILE, &profile, (int) -1);
  g_settings_get (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &text);
  uuid = profile_get_uuid (profile);

  g_value_init (&value, G_TYPE_STRING);
  g_value_take_string (&value,
                       g_markup_printf_escaped ("%s\n<span size=\"small\" font_family=\"monospace\">%s</span>",
                                                strlen (text) > 0 ? text : _("Unnamed"), 
                                                uuid));
  g_free (uuid);
  g_object_set_property (G_OBJECT (cell), "markup", &value);
  g_value_unset (&value);

  g_object_unref (profile);
}

static int
terminal_app_profile_sort_func (GtkTreeModel *model,
                                GtkTreeIter *a,
                                GtkTreeIter *b,
                                gpointer user_data)
{
  GSettings *profile_a, *profile_b;
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
                                    GSettings *selected_profile,
                                    GtkTreeIter *selected_profile_iter,
                                    gboolean *selected_profile_iter_set)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GHashTableIter ht_iter;
  gpointer value;

  G_STATIC_ASSERT (NUM_COLUMNS == 1);
  store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_SETTINGS);

  if (selected_profile_iter)
    *selected_profile_iter_set = FALSE;

  g_hash_table_iter_init (&ht_iter, app->profiles_hash);
  while (g_hash_table_iter_next (&ht_iter, NULL, &value))
    {
      GSettings *profile = (GSettings *) value;

      gtk_list_store_insert_with_values (store, &iter, 0,
                                         (int) COL_PROFILE, profile,
                                         (int) -1);

      if (selected_profile_iter && profile == selected_profile)
        {
          *selected_profile_iter = iter;
          *selected_profile_iter_set = TRUE;
        }
    }

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                   COL_PROFILE,
                                   terminal_app_profile_sort_func,
                                   NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        COL_PROFILE, GTK_SORT_ASCENDING);

  return GTK_TREE_MODEL (store);
}

static /* ref */ GSettings*
profile_combo_box_ref_selected (GtkWidget *widget)
{
  GtkComboBox *combo = GTK_COMBO_BOX (widget);
  GSettings *profile;
  GtkTreeIter iter;

  if (gtk_combo_box_get_active_iter (combo, &iter))
    gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
                        (int) COL_PROFILE, &profile, (int) -1);
  else
    profile = NULL;

  return profile;
}

static void
profile_combo_box_refill (TerminalApp *app,
                          GtkWidget *widget)
{
  GtkComboBox *combo = GTK_COMBO_BOX (widget);
  GtkTreeIter iter;
  gboolean iter_set;
  GSettings *selected_profile;
  GtkTreeModel *model;

  selected_profile = profile_combo_box_ref_selected (widget);

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
  GtkWidget *combo_widget;
  GtkComboBox *combo;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  gboolean iter_set;
  GSettings *default_profile;
  GtkTreeModel *model;

  combo_widget = gtk_combo_box_new ();
  combo = GTK_COMBO_BOX (combo_widget);
  terminal_util_set_atk_name_description (combo_widget, NULL, _("Click button to choose profile"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer,
                                      (GtkCellLayoutDataFunc) terminal_app_profile_cell_data_func,
                                      NULL, NULL);

  default_profile = terminal_app_ref_profile_by_uuid (app, NULL, NULL);
  model = terminal_app_get_profile_liststore (app,
                                              default_profile,
                                              &iter,
                                              &iter_set);
  gtk_combo_box_set_model (combo, model);
  g_object_unref (model);

  if (iter_set)
    gtk_combo_box_set_active_iter (combo, &iter);

  if (default_profile)
    g_object_unref (default_profile);

  g_signal_connect (app, "profile-list-changed",
                    G_CALLBACK (profile_combo_box_refill), combo);

  gtk_widget_show (combo_widget);
  return combo_widget;
}

static void
profile_combo_box_changed_cb (GtkWidget *widget,
                              TerminalApp *app)
{
  GSettings *profile;
  char *uuid;

  profile = profile_combo_box_ref_selected (widget);
  if (!profile)
    return;

  uuid = profile_get_uuid (profile);
  g_settings_set_string (app->global_settings, TERMINAL_SETTING_DEFAULT_PROFILE_KEY, uuid);

  g_free (uuid);
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
  GSettings *selected_profile = NULL;

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
  GSettings *profile;

  profile = (GSettings *) g_object_get_data (G_OBJECT (dialog), "profile");
  g_assert (profile != NULL);

  if (response == GTK_RESPONSE_ACCEPT)
    profile_remove (app, profile);

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
  GSettings *selected_profile;
  GtkWidget *transient_parent;
  const char *name;

  model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

  transient_parent = gtk_widget_get_toplevel (widget);
  g_settings_get (selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &name);
  dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Delete profile “%s”?"),
                                   name);

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
profile_list_clone_button_clicked_cb (GtkWidget   *button,
                                      gpointer data)
{
  TerminalApp *app;

  app = terminal_app_get ();
  terminal_app_new_profile (app, NULL /* FIXME! */, GTK_WINDOW (app->manage_profiles_dialog));
}

static void
profile_list_edit_button_clicked_cb (GtkWidget *button,
                                     GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GSettings *selected_profile;
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
  GSettings *selected_profile;
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

  g_assert (profiles != NULL);

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

  g_variant_get (value, "^a&s", &profiles);
  if (validate_profile_list (profiles)) {
    *result = profiles;
    return TRUE;
  }

  g_free (profiles);
  return FALSE;
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
  profiles = g_settings_get_mapped (app->global_settings, TERMINAL_SETTING_PROFILES_KEY,
                                    map_profiles_list, NULL);
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
  g_free (profiles);

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

  new_profile = profile_clone (app, base_profile, _("Unnamed"));
  terminal_profile_edit (new_profile, transient_parent, "profile-name-entry");
  g_object_unref (new_profile);
}

static void
profile_list_selection_changed_cb (GtkTreeSelection *selection,
                                   TerminalApp *app)
{
  gboolean selected;

  selected = gtk_tree_selection_get_selected (selection, NULL, NULL);

  gtk_widget_set_sensitive (app->manage_profiles_edit_button, selected);
  gtk_widget_set_sensitive (app->manage_profiles_clone_button, /* selected */ FALSE);
  gtk_widget_set_sensitive (app->manage_profiles_delete_button,
                            selected &&
                            g_hash_table_size (app->profiles_hash) > 1);
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
  app->manage_profiles_clone_button = NULL;
  app->manage_profiles_delete_button = NULL;
  app->manage_profiles_default_menu = NULL;
}

void
terminal_app_manage_profiles (TerminalApp     *app,
                              GtkWindow       *transient_parent)
{
  GObject *dialog;
  GObject *tree_view_container, *new_button, *edit_button, *clone_button, *remove_button;
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
                                        "clone-profile-button", &clone_button,
                                        "delete-profile-button", &remove_button,
                                        "default-profile-hbox", &default_hbox,
                                        "default-profile-label", &default_label,
                                        NULL))
    return;

  app->manage_profiles_dialog = GTK_WIDGET (dialog);
  app->manage_profiles_new_button = GTK_WIDGET (new_button);
  app->manage_profiles_edit_button = GTK_WIDGET (edit_button);
  app->manage_profiles_clone_button = GTK_WIDGET (clone_button);
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
  g_signal_connect (clone_button, "clicked",
                    G_CALLBACK (profile_list_clone_button_clicked_cb),
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
  gboolean shell_shows_app_menu;

  G_APPLICATION_CLASS (terminal_app_parent_class)->startup (application);

  /* Need to set the WM class (bug #685742) */
  gdk_set_program_class("Gnome-terminal");

  /* Only install the app menu if it's going to be shown */
  g_object_get (gtk_settings_get_for_screen (gdk_screen_get_default ()), "gtk-shell-shows-app-menu", &shell_shows_app_menu, NULL);
  if (!shell_shows_app_menu)
    return;

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

  return g_list_sort (g_hash_table_get_values (app->profiles_hash), profiles_alphabetic_cmp);
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
    uuid = g_settings_get_string (app->global_settings, TERMINAL_SETTING_DEFAULT_PROFILE_KEY);

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
