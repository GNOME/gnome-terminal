/*
 * Copyright © 2001, 2002 Havoc Pennington, Red Hat Inc.
 * Copyright © 2008, 2011, 2012, 2013 Christian Persch
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

#include <uuid.h>
#include <dconf.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "terminal-prefs.h"
#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-schemas.h"
#include "terminal-util.h"
#include "terminal-profiles-list.h"
#include "terminal-encoding.h"
#include "terminal-libgsystem.h"

typedef struct {
  TerminalSettingsList *profiles_list;
  GtkWidget *dialog;
  GtkWindow *parent;

  GtkTreeView *manage_profiles_list;
  GtkWidget *manage_profiles_new_button;
  GtkWidget *manage_profiles_edit_button;
  GtkWidget *manage_profiles_clone_button;
  GtkWidget *manage_profiles_delete_button;
  GtkWidget *profiles_default_combo;

  GtkListStore *encoding_base_store;
  GtkTreeModel *encodings_model;
  GtkTreeView *encodings_tree_view;
} PrefData;

static GtkWidget *prefs_dialog = NULL;

static void
prefs_dialog_help_button_clicked_cb (GtkWidget *button,
                                     PrefData *data)
{
  terminal_util_show_help ("pref", GTK_WINDOW (data->dialog));
}

static void
prefs_dialog_close_button_clicked_cb (GtkWidget *button,
                                      PrefData *data)
{
  gtk_widget_destroy (data->dialog);
}

/* Profiles tab */

enum
{
  COL_PROFILE,
  NUM_PROFILE_COLUMNS
};

static void
profile_cell_data_func (GtkTreeViewColumn *tree_column,
                        GtkCellRenderer *cell,
                        GtkTreeModel *tree_model,
                        GtkTreeIter *iter,
                        PrefData *data)
{
  gs_unref_object GSettings *profile;
  gs_free char *text;
  GValue value = { 0, };

  gtk_tree_model_get (tree_model, iter, (int) COL_PROFILE, &profile, (int) -1);
  text = g_settings_get_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);

  g_value_init (&value, G_TYPE_STRING);
  if (text[0])
    g_value_set_string (&value, text);
  else
    g_value_set_static_string (&value, _("Unnamed"));

  g_object_set_property (G_OBJECT (cell), "text", &value);
  g_value_unset (&value);
}

static int
profile_sort_func (GtkTreeModel *model,
                   GtkTreeIter *a,
                   GtkTreeIter *b,
                   gpointer user_data)
{
  gs_unref_object GSettings *profile_a;
  gs_unref_object GSettings *profile_b;

  gtk_tree_model_get (model, a, (int) COL_PROFILE, &profile_a, (int) -1);
  gtk_tree_model_get (model, b, (int) COL_PROFILE, &profile_b, (int) -1);

  return terminal_profiles_compare (profile_a, profile_b);
}

static /* ref */ GtkTreeModel *
profile_liststore_new (PrefData *data,
                       GSettings *selected_profile,
                       GtkTreeIter *selected_profile_iter,
                       gboolean *selected_profile_iter_set)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *list, *l;

  G_STATIC_ASSERT (NUM_PROFILE_COLUMNS == 1);
  store = gtk_list_store_new (NUM_PROFILE_COLUMNS, G_TYPE_SETTINGS);

  if (selected_profile_iter)
    *selected_profile_iter_set = FALSE;

  list = terminal_settings_list_ref_children (data->profiles_list);
  for (l = list; l != NULL; l = l->next)
    {
      GSettings *profile = (GSettings *) l->data;

      gtk_list_store_insert_with_values (store, &iter, 0,
                                         (int) COL_PROFILE, profile,
                                         (int) -1);

      if (selected_profile_iter && profile == selected_profile)
        {
          *selected_profile_iter = iter;
          *selected_profile_iter_set = TRUE;
        }
    }

  g_list_free_full (list, (GDestroyNotify) g_object_unref);

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                   COL_PROFILE,
                                   profile_sort_func,
                                   NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        COL_PROFILE, GTK_SORT_ASCENDING);

  return GTK_TREE_MODEL (store);
}

static /* ref */ GSettings*
profile_combo_box_ref_selected (GtkComboBox *combo)
{
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
profile_combo_box_refill (PrefData *data)
{
  GtkComboBox *combo = GTK_COMBO_BOX (data->profiles_default_combo);
  GtkTreeIter iter;
  gboolean iter_set;
  gs_unref_object GSettings *selected_profile;
  gs_unref_object GtkTreeModel *model;

  selected_profile = profile_combo_box_ref_selected (combo);

  model = profile_liststore_new (data,
                                 selected_profile,
                                 &iter,
                                 &iter_set);
  gtk_combo_box_set_model (combo, model);

  if (iter_set)
    gtk_combo_box_set_active_iter (combo, &iter);
}

static GtkWidget*
profile_combo_box_new (PrefData *data)
{
  GtkWidget *combo_widget;
  GtkComboBox *combo;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  gboolean iter_set;
  gs_unref_object GSettings *default_profile;
  gs_unref_object GtkTreeModel *model;

  combo_widget = gtk_combo_box_new ();
  combo = GTK_COMBO_BOX (combo_widget);
  terminal_util_set_atk_name_description (combo_widget, NULL, _("Click button to choose profile"));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer,
                                      (GtkCellLayoutDataFunc) profile_cell_data_func,
                                      data, NULL);

  default_profile = terminal_settings_list_ref_default_child (data->profiles_list);
  model = profile_liststore_new (data,
                                 default_profile,
                                 &iter,
                                 &iter_set);
  gtk_combo_box_set_model (combo, model);

  if (iter_set)
    gtk_combo_box_set_active_iter (combo, &iter);

  gtk_widget_show (combo_widget);
  return combo_widget;
}

static void
profile_combo_box_changed_cb (GtkWidget *widget,
                              PrefData *data)
{
  gs_unref_object GSettings *profile;
  gs_free char *uuid = NULL;

  profile = profile_combo_box_ref_selected (GTK_COMBO_BOX (data->profiles_default_combo));
  if (!profile)
    return;

  uuid = terminal_settings_list_dup_uuid_from_child (data->profiles_list, profile);
  terminal_settings_list_set_default_child (data->profiles_list, uuid);
}

static GSettings *
profile_list_ref_selected (PrefData *data)
{
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GSettings *selected_profile;

  selection = gtk_tree_view_get_selection (data->manage_profiles_list);
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return NULL;

  model = gtk_tree_view_get_model (data->manage_profiles_list);
  gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);
  return selected_profile;
}

static void
profile_list_treeview_refill (PrefData *data)
{
  GtkTreeView *tree_view = data->manage_profiles_list;
  GtkTreeIter iter;
  gboolean iter_set;
  gs_unref_object GSettings *selected_profile;
  gs_unref_object GtkTreeModel *model;

  selected_profile = profile_list_ref_selected (data);
  model = profile_liststore_new (data,
                                 selected_profile,
                                 &iter,
                                 &iter_set);
  gtk_tree_view_set_model (tree_view, model);

  if (!iter_set)
    iter_set = gtk_tree_model_get_iter_first (model, &iter);

  if (iter_set)
    gtk_tree_selection_select_iter (gtk_tree_view_get_selection (tree_view), &iter);
}

static void
profile_list_row_activated_cb (GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GtkTreeViewColumn *column,
                               PrefData *data)
{
  gs_unref_object GSettings *selected_profile;

  selected_profile = profile_list_ref_selected (data);
  if (selected_profile == NULL)
    return;

  terminal_app_edit_profile (terminal_app_get (), selected_profile, NULL, NULL);
}

static GtkTreeView *
profile_list_treeview_new (PrefData *data)
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
                                      (GtkCellLayoutDataFunc) profile_cell_data_func,
                                      data, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                               GTK_TREE_VIEW_COLUMN (column));

  g_signal_connect (tree_view, "row-activated",
                    G_CALLBACK (profile_list_row_activated_cb), data);

  return GTK_TREE_VIEW (tree_view);
}

static void
profile_list_delete_confirm_response_cb (GtkWidget *dialog,
                                         int response,
                                         PrefData *data)
{
  GSettings *profile;

  profile = (GSettings *) g_object_get_data (G_OBJECT (dialog), "profile");
  g_assert (profile != NULL);

  if (response == GTK_RESPONSE_ACCEPT)
    terminal_app_remove_profile (terminal_app_get (), profile);

  gtk_widget_destroy (dialog);
}

static void
profile_list_delete_button_clicked_cb (GtkWidget *button,
                                       PrefData *data)
{
  GtkWidget *dialog;
  gs_unref_object GSettings *selected_profile;
  gs_free char *name = NULL;

  selected_profile = profile_list_ref_selected (data);
  if (selected_profile == NULL)
    return;

  name = g_settings_get_string (selected_profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
  dialog = gtk_message_dialog_new (GTK_WINDOW (data->dialog),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Delete profile “%s”?"),
                                   name);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"),
                          GTK_RESPONSE_REJECT,
                          _("_Delete"),
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

  g_object_set_data_full (G_OBJECT (dialog), "profile", g_object_ref (selected_profile), g_object_unref);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (profile_list_delete_confirm_response_cb),
                    data);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
profile_list_new_button_clicked_cb (GtkWidget *button,
                                    PrefData *data)
{
  terminal_app_new_profile (terminal_app_get (), NULL, GTK_WINDOW (data->parent));
}

static void
profile_list_clone_button_clicked_cb (GtkWidget *button,
                                      PrefData *data)
{
  gs_unref_object GSettings *selected_profile;

  selected_profile = profile_list_ref_selected (data);
  if (selected_profile == NULL)
    return;

  terminal_app_new_profile (terminal_app_get (), selected_profile, GTK_WINDOW (data->parent));
}

static void
profile_list_edit_button_clicked_cb (GtkWidget *button,
                                     PrefData *data)
{
  gs_unref_object GSettings *selected_profile;

  selected_profile = profile_list_ref_selected (data);
  if (selected_profile == NULL)
    return;

  terminal_app_edit_profile (terminal_app_get (), selected_profile, NULL, NULL);
}

static void
profile_list_selection_changed_cb (GtkTreeSelection *selection,
                                   PrefData *data)
{
  gboolean selected;
  gs_unref_object GSettings *selected_profile;

  selected = gtk_tree_selection_get_selected (selection, NULL, NULL);
  selected_profile = profile_list_ref_selected (data);

  gtk_widget_set_sensitive (data->manage_profiles_edit_button, selected);
  gtk_widget_set_sensitive (data->manage_profiles_clone_button, selected);
  gtk_widget_set_sensitive (data->manage_profiles_delete_button,
                            selected && 
                            terminal_app_can_remove_profile (terminal_app_get (), selected_profile));
}

/* Keybindings tab */

/* Encodings tab */

static void
update_active_encodings_setting (void)
{
  TerminalApp *app;
  GSList *list, *l;
  GVariantBuilder builder;
  GSettings *settings;

  app = terminal_app_get ();

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  list = terminal_app_get_active_encodings (app);
  for (l = list; l != NULL; l = l->next)
    {
      TerminalEncoding *encoding = (TerminalEncoding *) l->data;

      g_variant_builder_add (&builder, "s", terminal_encoding_get_charset (encoding));
    }
  g_slist_foreach (list, (GFunc) terminal_encoding_unref, NULL);
  g_slist_free (list);

  settings = terminal_app_get_global_settings (app);
  g_settings_set (settings, TERMINAL_SETTING_ENCODINGS_KEY, "as", &builder);
}

enum
{
  COLUMN_NAME,
  COLUMN_CHARSET,
  COLUMN_DATA,
  N_ENCODING_COLUMNS
};

static void
encoding_active_toggled_cb (GtkCellRendererToggle *cell,
                            gchar *path_as_string,
                            PrefData *data)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  TerminalEncoding *encoding;

  path = gtk_tree_path_new_from_string (path_as_string);
  if (!gtk_tree_model_get_iter (data->encodings_model, &iter, path))
    goto out;

  gtk_tree_model_get (data->encodings_model, &iter, COLUMN_DATA, &encoding, -1);
  g_assert (encoding != NULL);

  encoding->is_active = !encoding->is_active;
  terminal_encoding_unref (encoding);

  gtk_tree_model_row_changed (GTK_TREE_MODEL (data->encodings_model), path, &iter);

  /* Persist the change */
  update_active_encodings_setting ();

 out:
  gtk_tree_path_free (path);
}

static void
encoding_active_cell_data_func (GtkTreeViewColumn *tree_column,
                                GtkCellRenderer *cell,
                                GtkTreeModel *tree_model,
                                GtkTreeIter *iter,
                                PrefData *data)
{
  TerminalEncoding *encoding;

  gtk_tree_model_get (tree_model, iter, (int) COLUMN_DATA, &encoding, -1);
  g_object_set (G_OBJECT (cell), "active", encoding->is_active, NULL);
  terminal_encoding_unref (encoding);
}

static void
encodings_list_changed_cb (PrefData *data)
{
  /* We just queue a redraw here which will take care of everything */
  gtk_widget_queue_draw (GTK_WIDGET (data->encodings_tree_view));
}

/* misc */

static void
prefs_dialog_destroy_cb (GtkWidget *widget,
                         PrefData *data)
{
  TerminalApp *app = terminal_app_get ();

  g_signal_handlers_disconnect_by_func (data->profiles_list, G_CALLBACK (profile_combo_box_refill), data);
  g_signal_handlers_disconnect_by_func (data->profiles_list, G_CALLBACK (profile_list_treeview_refill), data);

  g_signal_handlers_disconnect_by_func (app, G_CALLBACK (encodings_list_changed_cb), data);

  /* Don't run this handler again */
  g_signal_handlers_disconnect_by_func (widget, G_CALLBACK (prefs_dialog_destroy_cb), data);
  g_free (data);
}

void
terminal_prefs_show_preferences (GtkWindow *transient_parent,
                                 const char *page)
{
  TerminalApp *app = terminal_app_get ();
  PrefData *data;
  GtkWidget *dialog, *tree_view;
  GtkWidget *show_menubar_button, *disable_mnemonics_button, *disable_menu_accel_button;
  GtkWidget *disable_shortcuts_button;
  GtkWidget *tree_view_container, *new_button, *edit_button, *clone_button, *remove_button;
  GtkWidget *theme_variant_label, *theme_variant_combo, *new_terminal_mode_combo;
  GtkWidget *default_hbox, *default_label;
  GtkWidget *close_button, *help_button;
  GtkTreeSelection *selection;
  GSettings *settings;
  GtkCellRenderer *cell_renderer;
  GtkTreeViewColumn *column;
  GtkListStore *list_store;
  GHashTableIter ht_iter;
  gpointer key, value;

  if (prefs_dialog != NULL)
    goto done;

  data = g_new0 (PrefData, 1);
  data->parent = transient_parent;
  data->profiles_list = terminal_app_get_profiles_list (app);

  terminal_util_load_builder_resource ("/org/gnome/terminal/ui/preferences.ui",
                                       "preferences-dialog",
                                       "preferences-dialog", &dialog,
                                       "close-button", &close_button,
                                       "help-button", &help_button,
                                       "default-show-menubar-checkbutton", &show_menubar_button,
                                       "theme-variant-label", &theme_variant_label,
                                       "theme-variant-combobox", &theme_variant_combo,
                                       "new-terminal-mode-combobox", &new_terminal_mode_combo,
                                       "disable-mnemonics-checkbutton", &disable_mnemonics_button,
                                       "disable-shortcuts-checkbutton", &disable_shortcuts_button,
                                       "disable-menu-accel-checkbutton", &disable_menu_accel_button,
                                       "accelerators-treeview", &tree_view,
                                       "profiles-treeview-container", &tree_view_container,
                                       "new-profile-button", &new_button,
                                       "edit-profile-button", &edit_button,
                                       "clone-profile-button", &clone_button,
                                       "delete-profile-button", &remove_button,
                                       "default-profile-hbox", &default_hbox,
                                       "default-profile-label", &default_label,
                                       "encodings-treeview", &data->encodings_tree_view,
                                       NULL);

  data->dialog = dialog;

  terminal_util_bind_mnemonic_label_sensitivity (dialog);

  settings = terminal_app_get_global_settings (app);

  /* General tab */

  g_settings_bind (settings,
                   TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY,
                   show_menubar_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

#if GTK_CHECK_VERSION (3, 19, 0)
  g_settings_bind (settings,
                   TERMINAL_SETTING_THEME_VARIANT_KEY,
                   theme_variant_combo,
                   "active-id",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
#else
  gtk_widget_set_visible (theme_variant_label, FALSE);
  gtk_widget_set_visible (theme_variant_combo, FALSE);
#endif /* GTK+ 3.19 */

  g_settings_bind (settings,
                   TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY,
                   new_terminal_mode_combo,
                   "active-id",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  /* Keybindings tab */

  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                   disable_mnemonics_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY,
                   disable_shortcuts_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                   disable_menu_accel_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  terminal_accels_fill_treeview (tree_view, disable_shortcuts_button);

  /* Profiles tab */

  data->manage_profiles_new_button = GTK_WIDGET (new_button);
  data->manage_profiles_edit_button = GTK_WIDGET (edit_button);
  data->manage_profiles_clone_button = GTK_WIDGET (clone_button);
  data->manage_profiles_delete_button  = GTK_WIDGET (remove_button);

  data->manage_profiles_list = profile_list_treeview_new (data);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->manage_profiles_list));
  g_signal_connect (selection, "changed", G_CALLBACK (profile_list_selection_changed_cb), data);

  profile_list_treeview_refill (data);
  g_signal_connect_swapped (data->profiles_list, "children-changed",
                            G_CALLBACK (profile_list_treeview_refill), data);

  gtk_container_add (GTK_CONTAINER (tree_view_container), GTK_WIDGET (data->manage_profiles_list));
  gtk_widget_show (GTK_WIDGET (data->manage_profiles_list));

  g_signal_connect (new_button, "clicked",
                    G_CALLBACK (profile_list_new_button_clicked_cb),
                    data);
  g_signal_connect (edit_button, "clicked",
                    G_CALLBACK (profile_list_edit_button_clicked_cb),
                    data);
  g_signal_connect (clone_button, "clicked",
                    G_CALLBACK (profile_list_clone_button_clicked_cb),
                    data);
  g_signal_connect (remove_button, "clicked",
                    G_CALLBACK (profile_list_delete_button_clicked_cb),
                    data);

  data->profiles_default_combo = profile_combo_box_new (data);
  g_signal_connect_swapped (data->profiles_list, "children-changed",
                            G_CALLBACK (profile_combo_box_refill), data);
  g_signal_connect (data->profiles_default_combo, "changed",
                    G_CALLBACK (profile_combo_box_changed_cb), data);

  gtk_box_pack_start (GTK_BOX (default_hbox), data->profiles_default_combo, FALSE, FALSE, 0);
  gtk_widget_show (data->profiles_default_combo);

  // FIXMEchpe
  gtk_label_set_mnemonic_widget (GTK_LABEL (default_label), data->profiles_default_combo);

  /* Encodings tab */

  selection = gtk_tree_view_get_selection (data->encodings_tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  /* Column 1 */
  cell_renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (cell_renderer, "xalign", 0.0, NULL);
  g_signal_connect (cell_renderer, "toggled",
                    G_CALLBACK (encoding_active_toggled_cb), data);
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Show"));
  gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column),
                                      cell_renderer,
                                      (GtkCellLayoutDataFunc) encoding_active_cell_data_func,
                                      data, NULL);

  gtk_tree_view_append_column (data->encodings_tree_view, column);

  /* Column 2 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Encoding"),
						     cell_renderer,
						     "markup", COLUMN_NAME,
						     NULL);
  gtk_tree_view_append_column (data->encodings_tree_view, column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);

  /* Add the data */

  list_store = gtk_list_store_new (N_ENCODING_COLUMNS,
                                   G_TYPE_STRING,
                                   G_TYPE_STRING,
                                   TERMINAL_TYPE_ENCODING);
  data->encodings_model = GTK_TREE_MODEL (list_store);

  g_hash_table_iter_init (&ht_iter, terminal_app_get_encodings (app));
  while (g_hash_table_iter_next (&ht_iter, &key, &value)) {
    TerminalEncoding *encoding = value;
    GtkTreeIter iter;
    gs_free char *name;

    name = g_markup_printf_escaped ("%s <span size=\"small\">%s</span>",
                                    terminal_encoding_get_charset (encoding),
                                    encoding->name);
    gtk_list_store_insert_with_values (list_store, &iter, -1,
                                       COLUMN_NAME, name,
                                       COLUMN_CHARSET, terminal_encoding_get_charset (encoding),
                                       COLUMN_DATA, encoding,
                                       -1);
  }

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (data->encodings_model),
                                        COLUMN_CHARSET,
                                        GTK_SORT_ASCENDING);

  gtk_tree_view_set_model (data->encodings_tree_view, data->encodings_model);
  g_object_unref (data->encodings_model);

  g_signal_connect_swapped (app, "encoding-list-changed",
                            G_CALLBACK (encodings_list_changed_cb), data);

  /* misc */

  g_signal_connect (close_button, "clicked", G_CALLBACK (prefs_dialog_close_button_clicked_cb), data);
  g_signal_connect (help_button, "clicked", G_CALLBACK (prefs_dialog_help_button_clicked_cb), data);
  g_signal_connect (dialog, "destroy", G_CALLBACK (prefs_dialog_destroy_cb), data);
  gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 350);

  prefs_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &prefs_dialog);

done:
  gtk_window_set_transient_for (GTK_WINDOW (prefs_dialog), transient_parent);

  terminal_util_dialog_focus_widget (prefs_dialog, page);

  gtk_window_present (GTK_WINDOW (prefs_dialog));
}
