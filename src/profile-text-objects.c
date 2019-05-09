/*
 * Copyright © 2019 Rodolfo Granata
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

#include "profile-editor.h"
#include "profile-text-objects.h"
#include "terminal-libgsystem.h"
#include "terminal-prefs.h"

#include <glib/gi18n.h>

enum {
  TEXT_OBJ_NAME = 0,
  TEXT_OBJ_MATCH,
  TEXT_OBJ_REWRITE,
  TEXT_OBJ_PRIO,
  TEXT_OBJ_N_COLS,
};

/* setup the the profile editor's text-object tab */
void
profile_text_objects_init(void)
{
  GtkBuilder *builder = the_pref_data->builder;
  GtkTreeView *tree_view =
    (GtkTreeView *) gtk_builder_get_object (builder, "text-object-list");
  /* setup tree */
  GtkTreeSelection *select = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

  /* Setup cell renderers */
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _("Name"), renderer, "text", TEXT_OBJ_NAME, NULL);
  gtk_tree_view_append_column (tree_view, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _("Regex Match"), renderer, "text", TEXT_OBJ_MATCH, NULL);
  gtk_tree_view_append_column (tree_view, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _("URL Template"), renderer, "text", TEXT_OBJ_REWRITE, NULL);
  gtk_tree_view_append_column (tree_view, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (
      _("Rank"), renderer, "text", TEXT_OBJ_PRIO, NULL);
  gtk_tree_view_append_column (tree_view, column);
}

static void
profile_save_text_objects(GSettings *profile, GtkListStore *store) {
  /* setup gvariant dict */
  GVariantBuilder txt_objs;
  g_variant_builder_init (&txt_objs, G_VARIANT_TYPE ("a{s(ssi)}"));

  GtkTreeIter list_iter;
  /* iterate over the store and write values into variant */
  gboolean more_rows =
    gtk_tree_model_get_iter_first ((GtkTreeModel *) store, &list_iter);
  while (more_rows) {
    gs_free gchar *name, *match, *rewrite;
    gint prio;
    gtk_tree_model_get ((GtkTreeModel *) store, &list_iter,
        TEXT_OBJ_NAME, &name,
        TEXT_OBJ_MATCH, &match,
        TEXT_OBJ_REWRITE, &rewrite,
        TEXT_OBJ_PRIO, &prio,
        -1);
    /* encode text-object into variant dict */
    g_variant_builder_add (&txt_objs, "{s(ssi)}", name, match, rewrite, prio);
    more_rows = gtk_tree_model_iter_next ((GtkTreeModel *) store, &list_iter);
  }

  GVariant *text_objects = g_variant_builder_end (&txt_objs);
  g_settings_set_value (profile, "text-objects", text_objects);
}

/* callback triggered for opening the dialog to edit/add a text-object */
static void
edit_text_object_cb (GtkWidget *button, gpointer user_data)
{
  GtkBuilder *builder = the_pref_data->builder;
  GtkPopover *txtobj_dialog =
    (GtkPopover *) gtk_builder_get_object (builder, "txt-obj-dialog");

  gtk_popover_set_relative_to (txtobj_dialog, button);
  gtk_popover_set_position (txtobj_dialog, GTK_POS_BOTTOM);
  gtk_popover_set_default_widget (txtobj_dialog,
      GTK_WIDGET (gtk_builder_get_object (builder, "txt-obj-edit-save")));

#if GTK_CHECK_VERSION (3, 22, 0)
  gtk_popover_popup (txtobj_dialog);
#else
  gtk_widget_show (GTK_WIDGET (txtobj_dialog));
#endif
}

/* callback for removing a text-object in the profile editor */
static void
remove_text_object_cb (GtkWidget *button, GSettings *profile)
{
  GtkBuilder *builder = the_pref_data->builder;
  GtkTreeView *tree_view =
    (GtkTreeView *) gtk_builder_get_object (builder, "text-object-list");
  GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);

  GtkTreeIter list_iter;
  GtkListStore *store;
  /* check if there's an item selected */
  if (gtk_tree_selection_get_selected (selection, (GtkTreeModel **) &store, &list_iter)) {
    gtk_list_store_remove (store, &list_iter);
    profile_save_text_objects (profile, store);
  }
}

/* callback to hide the text-object editor dialog */
static void
edit_text_object_hide_cb (GtkButton *button, gpointer user_data)
{
  GtkBuilder *builder = the_pref_data->builder;
  GtkPopover *txtobj_dialog =
    (GtkPopover *) gtk_builder_get_object (builder, "txt-obj-dialog");

#if GTK_CHECK_VERSION (3, 22, 0)
  gtk_popover_popdown (txtobj_dialog);
#else
  gtk_widget_hide (GTK_WIDGET (txtobj_dialog));
#endif
}

/* callback for saving contents to text-object editor dialog */
static void
edit_text_object_save_cb (GtkButton *button, GSettings *profile)
{
  GtkBuilder *builder = the_pref_data->builder;

  /* get contents of the dialog text fields */
  const char *name = gtk_entry_get_text (
      GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-name")));
  const char *match = gtk_entry_get_text (
      GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-match")));
  const char *rewrite = gtk_entry_get_text (
      GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-rewrite")));
  const char *sprio = gtk_entry_get_text (
      GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-prio")));
  gint64 prio = g_ascii_strtoll (sprio, NULL, 10);

  GtkTreeView *tree_view =
    (GtkTreeView *) gtk_builder_get_object (builder, "text-object-list");
  GtkListStore *store = (GtkListStore *) gtk_tree_view_get_model(tree_view);

  /* search the profile's text-objects by rule-name to check if we're
   * adding a new text-object or we're replacing an existing one */
  GtkTreeIter list_iter;
  gboolean more_rows =
    gtk_tree_model_get_iter_first ((GtkTreeModel *) store, &list_iter);
  gboolean existing = FALSE;
  while (more_rows && !existing) {
    gs_free gchar *_name;
    gtk_tree_model_get ((GtkTreeModel *) store, &list_iter,
                        TEXT_OBJ_NAME, &_name, -1);
    existing = (g_strcmp0 (name, _name) == 0);
    /* found an existing text-object with the same rule-name, we'll replace it */
    if (existing) {
      break;
    }
    more_rows = gtk_tree_model_iter_next ((GtkTreeModel *) store, &list_iter);
  }

  /* if no existing rule found we'll append a new one */
  if (!existing) {
    gtk_list_store_append (store, &list_iter);
  }

  /* update/add the text-object */
  gtk_list_store_set (store, &list_iter,
      TEXT_OBJ_NAME, name,
      TEXT_OBJ_MATCH, match,
      TEXT_OBJ_REWRITE, rewrite,
      TEXT_OBJ_PRIO, prio,
      -1);

  profile_save_text_objects (profile, store);

  /* Hide the popover */
  edit_text_object_hide_cb (button, NULL);
}

/* callback to pre-populate text-object edition dialog with current selection */
static void
toggle_text_object_buttons_cb (GtkTreeSelection *selection, gpointer user_data)
{
  GtkBuilder *builder = the_pref_data->builder;
  GtkTreeIter list_iter;
  GtkListStore *store;
  /* get active selection */
  gboolean selected = gtk_tree_selection_get_selected (
      selection, (GtkTreeModel **) &store, &list_iter);

  /* disable the 'Remove' button if there's no active selection */
  gtk_widget_set_sensitive (
      GTK_WIDGET (gtk_builder_get_object (builder, "txt-obj-remove-button")), selected);

  /* copy selected values into edit dialog for later use */
  if (selected) {
    gs_free gchar *name, *match, *rewrite, *sprio;
    gint prio;
    gtk_tree_model_get ((GtkTreeModel *) store, &list_iter,
        TEXT_OBJ_NAME, &name,
        TEXT_OBJ_MATCH, &match,
        TEXT_OBJ_REWRITE, &rewrite,
        TEXT_OBJ_PRIO, &prio,
        -1);
    sprio = g_strdup_printf("%d", prio);
    gtk_entry_set_text (
        GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-name")), name);
    gtk_entry_set_text (
        GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-match")), match);
    gtk_entry_set_text (
        GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-rewrite")), rewrite);
    gtk_entry_set_text (
        GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-prio")), sprio);
  }
}

static void
validate_text_object_cb (GtkEntry *entry, gpointer user_data)
{
  GtkBuilder *builder = the_pref_data->builder;
  gboolean valid = TRUE;

  /* check that all fields have some text */
  const char *text = gtk_entry_get_text (entry);
  valid &= (strlen(text) > 0);

  /* Check that Priority/Rank is an integer */
  GtkEntry *prio = GTK_ENTRY (gtk_builder_get_object (builder, "txt-obj-prio"));
  if (entry == prio) {
    gchar *end_ptr = NULL;
    g_ascii_strtoll (text, &end_ptr, 10);
    /* check that end_ptr actually advanced */
    valid &= (end_ptr == text + strlen(text));
  }

  /* react to input being valid: set warning icon and toggle Save button */
  gtk_entry_set_icon_from_icon_name (
      entry,
      GTK_ENTRY_ICON_PRIMARY, valid ? NULL : "dialog-warning");
  gtk_widget_set_sensitive (
      (GtkWidget *) gtk_builder_get_object (builder, "txt-obj-edit-save"), valid);
}

/* bind the text-object GUI elements to callbacks */
static void
profile_text_objects_bind(GSettings *profile)
{
  GtkBuilder *builder = the_pref_data->builder;
  profile_prefs_signal_connect (
      gtk_builder_get_object (builder, "txt-obj-remove-button"),
      "clicked", G_CALLBACK (remove_text_object_cb), profile);

  profile_prefs_signal_connect (
      gtk_builder_get_object (builder, "txt-obj-edit-button"),
      "clicked", G_CALLBACK (edit_text_object_cb), NULL);

  profile_prefs_signal_connect (
      gtk_builder_get_object (builder, "txt-obj-edit-cancel"),
      "clicked", G_CALLBACK (edit_text_object_hide_cb), NULL);

  profile_prefs_signal_connect (
      gtk_builder_get_object (builder, "txt-obj-edit-save"),
      "clicked", G_CALLBACK (edit_text_object_save_cb), profile);

  profile_prefs_signal_connect (
      (GtkEntry *) gtk_builder_get_object (builder, "txt-obj-match"),
      "changed", G_CALLBACK (validate_text_object_cb), NULL);

  profile_prefs_signal_connect (
      (GtkEntry *) gtk_builder_get_object (builder, "txt-obj-rewrite"),
      "changed", G_CALLBACK (validate_text_object_cb), NULL);

  profile_prefs_signal_connect (
      (GtkEntry *) gtk_builder_get_object (builder, "txt-obj-prio"),
      "changed", G_CALLBACK (validate_text_object_cb), NULL);

  /* disable edit buttons if we can't write settings */
  if (g_settings_is_writable (profile, "text-objects")) {
    /* react to selection changes in the text-object list */
    GtkTreeSelection *selection = gtk_tree_view_get_selection (
        (GtkTreeView *) gtk_builder_get_object (builder, "text-object-list"));
    profile_prefs_signal_connect (
        selection,
        "changed", G_CALLBACK (toggle_text_object_buttons_cb), NULL);
  } else {
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "txt-obj-remove-button")), FALSE);
    gtk_widget_set_sensitive (
        GTK_WIDGET (gtk_builder_get_object (builder, "txt-obj-edit-button")), FALSE);
  }
}

void
profile_text_objects_load(GSettings *profile)
{
  /* Create the model */
  GtkListStore *store = gtk_list_store_new (
      TEXT_OBJ_N_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

  /* Load text-objects config from profile gsettings */
  gs_unref_variant GVariant *text_objects =
    g_settings_get_value(profile, "text-objects");

  GVariantIter viter;
  const gchar *name, *match, *rewrite;
  gint prio;

  /* populate the text-object table */
  g_variant_iter_init (&viter, text_objects);
  while (g_variant_iter_next (&viter, "{&s(&s&si)}", &name, &match, &rewrite, &prio)) {
    GtkTreeIter list_iter;
    gtk_list_store_append (store, &list_iter);
    gtk_list_store_set (store, &list_iter,
        TEXT_OBJ_NAME, name,
        TEXT_OBJ_MATCH, match,
        TEXT_OBJ_REWRITE, rewrite,
        TEXT_OBJ_PRIO, prio,
        -1);
  }

  /* link the data to the view */
  GtkBuilder *builder = the_pref_data->builder;
  GtkTreeView *tree_view =
    (GtkTreeView *) gtk_builder_get_object (builder, "text-object-list");
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL(store));

  /* bind buttons and actions */
  profile_text_objects_bind (profile);
}
