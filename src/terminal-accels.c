/*
 * Copyright © 2001, 2002 Havoc Pennington, Red Hat Inc.
 * Copyright © 2008, 2011 Christian Persch
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

#include <string.h>

#include <gtk/gtk.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-schemas.h"
#include "terminal-util.h"

#define ACCEL_PATH_ROOT "<Actions>/Main/"
#define ACCEL_NEW_TAB              "FileNewTab"
#define ACCEL_NEW_WINDOW           "FileNewWindow"
#define ACCEL_NEW_PROFILE          "FileNewProfile"
#define ACCEL_SAVE_CONTENTS        "FileSaveContents"
#define ACCEL_CLOSE_TAB            "FileCloseTab"
#define ACCEL_CLOSE_WINDOW         "FileCloseWindow"
#define ACCEL_COPY                 "EditCopy"
#define ACCEL_PASTE                "EditPaste"
#define ACCEL_TOGGLE_MENUBAR       "ViewMenubar"
#define ACCEL_FULL_SCREEN          "ViewFullscreen"
#define ACCEL_RESET                "TerminalReset"
#define ACCEL_RESET_AND_CLEAR      "TerminalResetClear"
#define ACCEL_PREV_TAB             "TabsPrevious"
#define ACCEL_NEXT_TAB             "TabsNext"
#define ACCEL_SET_TERMINAL_TITLE   "TerminalSetTitle"
#define ACCEL_HELP                 "HelpContents"
#define ACCEL_ZOOM_IN              "ViewZoomIn"
#define ACCEL_ZOOM_OUT             "ViewZoomOut"
#define ACCEL_ZOOM_NORMAL          "ViewZoom100"
#define ACCEL_MOVE_TAB_LEFT        "TabsMoveLeft"
#define ACCEL_MOVE_TAB_RIGHT       "TabsMoveRight"
#define ACCEL_DETACH_TAB           "TabsDetach"
#define ACCEL_SWITCH_TAB_PREFIX    "TabsSwitch"

#if 1
/*
* We don't want to enable content saving until vte supports it async.
* So we disable this code for stable versions.
*/
#include "terminal-version.h"

#if (TERMINAL_MINOR_VERSION & 1) != 0
#define ENABLE_SAVE
#else
#undef ENABLE_SAVE
#endif
#endif

typedef struct
{
  const char *user_visible_name;
  const char *accel_path_suffix;
} KeyEntry;

typedef struct
{
  KeyEntry *key_entry;
  guint n_elements;
  const char *user_visible_name;
} KeyEntryList;

static KeyEntry file_entries[] =
{
  { N_("New Tab"), ACCEL_NEW_TAB },
  { N_("New Window"), ACCEL_NEW_WINDOW },
  { N_("New Profile"), ACCEL_NEW_PROFILE },
#ifdef ENABLE_SAVE
  { N_("Save Contents"), ACCEL_SAVE_CONTENTS },
#endif
  { N_("Close Tab"), ACCEL_CLOSE_TAB },
  { N_("Close Window"), ACCEL_CLOSE_WINDOW }
};

static KeyEntry edit_entries[] =
{
  { N_("Copy"), ACCEL_COPY },
  { N_("Paste"), ACCEL_PASTE }
};

static KeyEntry view_entries[] =
{
  { N_("Hide and Show menubar"), ACCEL_TOGGLE_MENUBAR },
  { N_("Full Screen"), ACCEL_FULL_SCREEN },
  { N_("Zoom In"), ACCEL_ZOOM_IN },
  { N_("Zoom Out"), ACCEL_ZOOM_OUT },
  { N_("Normal Size"), ACCEL_ZOOM_NORMAL },
};

static KeyEntry terminal_entries[] =
{
  { N_("Set Title"), ACCEL_SET_TERMINAL_TITLE },
  { N_("Reset"), ACCEL_RESET },
  { N_("Reset and Clear"), ACCEL_RESET_AND_CLEAR }
};

static KeyEntry tabs_entries[] =
{
  { N_("Switch to Previous Tab"), ACCEL_PREV_TAB },
  { N_("Switch to Next Tab"), ACCEL_NEXT_TAB },
  { N_("Move Tab to the Left"), ACCEL_MOVE_TAB_LEFT },
  { N_("Move Tab to the Right"), ACCEL_MOVE_TAB_RIGHT },
  { N_("Detach Tab"), ACCEL_DETACH_TAB },
  { N_("Switch to Tab 1"), ACCEL_SWITCH_TAB_PREFIX "1" },
  { N_("Switch to Tab 2"), ACCEL_SWITCH_TAB_PREFIX "2" },
  { N_("Switch to Tab 3"), ACCEL_SWITCH_TAB_PREFIX "3" },
  { N_("Switch to Tab 4"), ACCEL_SWITCH_TAB_PREFIX "4" },
  { N_("Switch to Tab 5"), ACCEL_SWITCH_TAB_PREFIX "5" },
  { N_("Switch to Tab 6"), ACCEL_SWITCH_TAB_PREFIX "6" },
  { N_("Switch to Tab 7"), ACCEL_SWITCH_TAB_PREFIX "7" },
  { N_("Switch to Tab 8"), ACCEL_SWITCH_TAB_PREFIX "8" },
  { N_("Switch to Tab 9"), ACCEL_SWITCH_TAB_PREFIX "9" },
  { N_("Switch to Tab 10"), ACCEL_SWITCH_TAB_PREFIX "10" },
  { N_("Switch to Tab 11"), ACCEL_SWITCH_TAB_PREFIX "11" },
  { N_("Switch to Tab 12"), ACCEL_SWITCH_TAB_PREFIX "12" }
};

static KeyEntry help_entries[] = {
  { N_("Contents"), ACCEL_HELP }
};

static KeyEntryList all_entries[] =
{
  { file_entries, G_N_ELEMENTS (file_entries), N_("File") },
  { edit_entries, G_N_ELEMENTS (edit_entries), N_("Edit") },
  { view_entries, G_N_ELEMENTS (view_entries), N_("View") },
  { terminal_entries, G_N_ELEMENTS (terminal_entries), N_("Terminal") },
  { tabs_entries, G_N_ELEMENTS (tabs_entries), N_("Tabs") },
  { help_entries, G_N_ELEMENTS (help_entries), N_("Help") }
};

enum
{
  ACTION_COLUMN,
  ACCEL_PATH_COLUMN,
  N_COLUMNS
};

static void accel_map_changed_callback (GtkAccelMap    *accel_map,
                                        const char     *accel_path,
                                        guint           keyval,
                                        GdkModifierType modifier,
                                        gpointer        data);

static guint save_id = 0;
static GtkWidget *edit_keys_dialog = NULL;
static GtkTreeStore *edit_keys_store = NULL;
static gboolean dirty = FALSE;

static char *
binding_name (guint            keyval,
              GdkModifierType  mask)
{
  if (keyval != 0)
    return gtk_accelerator_name (keyval, mask);

  return g_strdup ("disabled");
}

static char *
get_accel_map_filename (void)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-terminal",
                           "accels",
                           NULL);
}

static gboolean
save_cb (gpointer data)
{
  char *path;

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "saving accel map");

  save_id = 0;

  if (!dirty)
    return FALSE;

  dirty = FALSE;

  path = get_accel_map_filename ();
  gtk_accel_map_save (path);
  g_free (path);

  return FALSE; /* don't run again */
}

static void
schedule_save (void)
{
  if (save_id != 0)
    return;

  dirty = TRUE;

  save_id = g_timeout_add_seconds (5, save_cb, NULL);
}

void
terminal_accels_init (void)
{
  char *path;

  g_signal_connect (gtk_accel_map_get (), "changed",
                    G_CALLBACK (accel_map_changed_callback), NULL);

  path = get_accel_map_filename ();
  gtk_accel_map_load (path);
  g_free (path);
}

void
terminal_accels_shutdown (void)
{
  if (save_id != 0)
    {
      g_source_remove (save_id);
      save_id = 0;

      save_cb (NULL);
    }
}

static gboolean
update_model_foreach (GtkTreeModel *model,
		      GtkTreePath  *path,
		      GtkTreeIter  *iter,
		      gpointer      data)
{
  guint accel_path_quark;

  gtk_tree_model_get (model, iter, ACCEL_PATH_COLUMN, &accel_path_quark, -1);
  if (accel_path_quark == GPOINTER_TO_UINT (data))
    {
      gtk_tree_model_row_changed (model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
accel_map_changed_callback (GtkAccelMap    *accel_map,
                            const char     *accel_path,
                            guint           keyval,
                            GdkModifierType modifier,
                            gpointer        data)
{
  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "Changed accel path %s to %s\n",
                         accel_path,
                         binding_name (keyval, modifier) /* memleak */);

  schedule_save ();
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  guint accel_path_quark;
  GtkAccelKey key;

  gtk_tree_model_get (model, iter, ACCEL_PATH_COLUMN, &accel_path_quark, -1);
  if (accel_path_quark == 0) {
    /* This is a title row */
    g_object_set (cell,
                  "visible", FALSE,
		  NULL);
    return;
  }

  if (gtk_accel_map_lookup_entry (g_quark_to_string (accel_path_quark), &key))
    g_object_set (cell,
                  "visible", TRUE,
                  "sensitive", (key.accel_flags & GTK_ACCEL_LOCKED) == 0,
                  "editable", (key.accel_flags & GTK_ACCEL_LOCKED) == 0,
                  "accel-key", key.accel_key,
                  "accel-mods", (guint) key.accel_mods,
                  NULL);
  else
    g_object_set (cell,
                  "visible", TRUE,
                  "sensitive", TRUE,
                  "editable", TRUE,
                  "accel-key", (guint) 0,
                  "accel-mods", (guint) 0,
                  NULL);
}

static void
treeview_accel_map_changed_cb (GtkAccelMap *accel_map,
                               const char *accel_path,
                               guint keyval,
                               GdkModifierType modifier,
                               GtkTreeModel *model)
{
  gtk_tree_model_foreach (model,
                          update_model_foreach,
                          GUINT_TO_POINTER (g_quark_from_string (accel_path)));
}

static void
accel_edited_callback (GtkCellRendererAccel *cell,
                       gchar                *path_string,
                       guint                 keyval,
                       GdkModifierType       mask,
                       guint                 hardware_keycode,
                       GtkTreeView          *view)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  guint accel_path_quark;

  model = gtk_tree_view_get_model (view);

  path = gtk_tree_path_new_from_string (path_string);
  if (!path)
    return;

  if (!gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, ACCEL_PATH_COLUMN, &accel_path_quark, -1);
  /* sanity check */
  if (accel_path_quark == 0)
    return;

  gtk_accel_map_change_entry (g_quark_to_string (accel_path_quark), keyval, mask, TRUE);
}

static void
accel_cleared_callback (GtkCellRendererAccel *cell,
                        gchar                *path_string,
                        GtkTreeView          *view)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  guint accel_path_quark;

  model = gtk_tree_view_get_model (view);

  path = gtk_tree_path_new_from_string (path_string);
  if (!path)
    return;

  if (!gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, ACCEL_PATH_COLUMN, &accel_path_quark, -1);
  /* sanity check */
  if (accel_path_quark == 0)
    return;

  gtk_accel_map_change_entry (g_quark_to_string (accel_path_quark), 0, 0, TRUE);
}

static void
edit_keys_dialog_destroy_cb (GtkWidget *widget,
                             gpointer user_data)
{
  g_signal_handlers_disconnect_by_func (gtk_accel_map_get (),
                                        G_CALLBACK (treeview_accel_map_changed_cb),
                                        user_data);
  edit_keys_dialog = NULL;
  edit_keys_store = NULL;
}

static void
edit_keys_dialog_response_cb (GtkWidget *editor,
                              int response,
                              gpointer use_data)
{
  if (response == GTK_RESPONSE_HELP)
    {
      terminal_util_show_help ("gnome-terminal-shortcuts", GTK_WINDOW (editor));
      return;
    }

  gtk_widget_destroy (editor);
}

void
terminal_edit_keys_dialog_show (GtkWindow *transient_parent)
{
  GtkWidget *dialog, *tree_view, *disable_mnemonics_button, *disable_menu_accel_button;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell_renderer;
  GtkTreeStore *tree;
  GSettings *settings;
  guint i;

  if (edit_keys_dialog != NULL)
    goto done;

  if (!terminal_util_load_builder_file ("keybinding-editor.ui",
                                        "keybindings-dialog", &dialog,
                                        "disable-mnemonics-checkbutton", &disable_mnemonics_button,
                                        "disable-menu-accel-checkbutton", &disable_menu_accel_button,
                                        "accelerators-treeview", &tree_view,
                                        NULL))
    return;

  terminal_util_bind_mnemonic_label_sensitivity (dialog);

  settings = terminal_app_get_global_settings (terminal_app_get ());
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                   disable_mnemonics_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                   disable_menu_accel_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  /* Column 1 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Action"),
						     cell_renderer,
						     "text", ACTION_COLUMN,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* Column 2 */
  cell_renderer = gtk_cell_renderer_accel_new ();
  g_object_set (cell_renderer,
                "editable", TRUE,
                "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                NULL);
  g_signal_connect (cell_renderer, "accel-edited",
                    G_CALLBACK (accel_edited_callback), tree_view);
  g_signal_connect (cell_renderer, "accel-cleared",
                    G_CALLBACK (accel_cleared_callback), tree_view);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Shortcut _Key"));
  gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, cell_renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* Add the data */

  tree = edit_keys_store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT);

  for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
    {
      GtkTreeIter parent_iter;
      guint j;

      gtk_tree_store_insert_with_values (tree, &parent_iter, NULL, -1,
                                         ACTION_COLUMN, _(all_entries[i].user_visible_name),
                                         ACCEL_PATH_COLUMN, 0,
                                         -1);

      for (j = 0; j < all_entries[i].n_elements; ++j)
	{
	  KeyEntry *key_entry = &(all_entries[i].key_entry[j]);
	  GtkTreeIter iter;
          char *accel_path;

          accel_path = g_strconcat (ACCEL_PATH_ROOT, key_entry->accel_path_suffix, NULL);
          gtk_tree_store_insert_with_values (tree, &iter, &parent_iter, -1,
                                             ACTION_COLUMN, _(key_entry->user_visible_name),
                                             ACCEL_PATH_COLUMN, g_quark_from_string (accel_path),
                                             -1);
          g_free (accel_path);
	}
    }

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (tree), ACTION_COLUMN,
                                        GTK_SORT_ASCENDING);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (tree));
  g_object_unref (tree);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  g_signal_connect (gtk_accel_map_get (), "changed",
                    G_CALLBACK (treeview_accel_map_changed_cb), tree);

  edit_keys_dialog = dialog;
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (edit_keys_dialog_destroy_cb), tree);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (edit_keys_dialog_response_cb),
                    NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 350);

done:
  gtk_window_set_transient_for (GTK_WINDOW (edit_keys_dialog), transient_parent);
  gtk_window_present (GTK_WINDOW (edit_keys_dialog));
}
