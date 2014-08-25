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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-schemas.h"
#include "terminal-intl.h"
#include "terminal-util.h"
#include "terminal-libgsystem.h"

/* NOTES
 *
 * There are two sources of keybindings changes, from GSettings and from
 * the accel map (happens with in-place menu editing).
 *
 * When a keybinding gconf key changes, we propagate that into the
 * accel map.
 * When the accel map changes, we queue a sync to GSettings.
 *
 * To avoid infinite loops, we short-circuit in both directions
 * if the value is unchanged from last known.
 *
 * In the keybinding editor, when editing or clearing an accel, we write
 * the change directly to GSettings and rely on the callback to
 * actually apply the change to the accel map.
 */

#define KEY_CLOSE_TAB           "close-tab"
#define KEY_CLOSE_WINDOW        "close-window"
#define KEY_COPY                "copy"
#define KEY_DETACH_TAB          "detach-tab"
#define KEY_FIND                "find"
#define KEY_FIND_CLEAR          "find-clear"
#define KEY_FIND_PREV           "find-previous"
#define KEY_FIND_NEXT           "find-next"
#define KEY_FULL_SCREEN         "full-screen"
#define KEY_HELP                "help"
#define KEY_MOVE_TAB_LEFT       "move-tab-left"
#define KEY_MOVE_TAB_RIGHT      "move-tab-right"
#define KEY_NEW_PROFILE         "new-profile"
#define KEY_NEW_TAB             "new-tab"
#define KEY_NEW_WINDOW          "new-window"
#define KEY_NEXT_TAB            "next-tab"
#define KEY_PASTE               "paste"
#define KEY_PREV_TAB            "prev-tab"
#define KEY_RESET_AND_CLEAR     "reset-and-clear"
#define KEY_RESET               "reset"
#define KEY_SAVE_CONTENTS       "save-contents"
#define KEY_TOGGLE_MENUBAR      "toggle-menubar"
#define KEY_ZOOM_IN             "zoom-in"
#define KEY_ZOOM_NORMAL         "zoom-normal"
#define KEY_ZOOM_OUT            "zoom-out"
#define KEY_SWITCH_TAB_PREFIX   "switch-to-tab-"

/* Accel paths for the gtkuimanager based menus */
#define ACCEL_PATH_ROOT "<Actions>/Main/"
#define ACCEL_PATH_KEY_CLOSE_TAB            ACCEL_PATH_ROOT "FileCloseTab"
#define ACCEL_PATH_KEY_CLOSE_WINDOW         ACCEL_PATH_ROOT "FileCloseWindow"
#define ACCEL_PATH_KEY_COPY                 ACCEL_PATH_ROOT "EditCopy"
#define ACCEL_PATH_KEY_DETACH_TAB           ACCEL_PATH_ROOT "TabsDetach"
#define ACCEL_PATH_KEY_FIND                 ACCEL_PATH_ROOT "SearchFind"
#define ACCEL_PATH_KEY_FIND_CLEAR           ACCEL_PATH_ROOT "SearchClearHighlight"
#define ACCEL_PATH_KEY_FIND_PREV            ACCEL_PATH_ROOT "SearchFindPrevious"
#define ACCEL_PATH_KEY_FIND_NEXT            ACCEL_PATH_ROOT "SearchFindNext"
#define ACCEL_PATH_KEY_FULL_SCREEN          ACCEL_PATH_ROOT "ViewFullscreen"
#define ACCEL_PATH_KEY_HELP                 ACCEL_PATH_ROOT "HelpContents"
#define ACCEL_PATH_KEY_MOVE_TAB_LEFT        ACCEL_PATH_ROOT "TabsMoveLeft"
#define ACCEL_PATH_KEY_MOVE_TAB_RIGHT       ACCEL_PATH_ROOT "TabsMoveRight"
#define ACCEL_PATH_KEY_NEW_PROFILE          ACCEL_PATH_ROOT "FileNewProfile"
#define ACCEL_PATH_KEY_NEW_TAB              ACCEL_PATH_ROOT "FileNewTab"
#define ACCEL_PATH_KEY_NEW_WINDOW           ACCEL_PATH_ROOT "FileNewWindow"
#define ACCEL_PATH_KEY_NEXT_TAB             ACCEL_PATH_ROOT "TabsNext"
#define ACCEL_PATH_KEY_PASTE                ACCEL_PATH_ROOT "EditPaste"
#define ACCEL_PATH_KEY_PREV_TAB             ACCEL_PATH_ROOT "TabsPrevious"
#define ACCEL_PATH_KEY_RESET                ACCEL_PATH_ROOT "TerminalReset"
#define ACCEL_PATH_KEY_RESET_AND_CLEAR      ACCEL_PATH_ROOT "TerminalResetClear"
#define ACCEL_PATH_KEY_SAVE_CONTENTS        ACCEL_PATH_ROOT "FileSaveContents"
#define ACCEL_PATH_KEY_TOGGLE_MENUBAR       ACCEL_PATH_ROOT "ViewMenubar"
#define ACCEL_PATH_KEY_ZOOM_IN              ACCEL_PATH_ROOT "ViewZoomIn"
#define ACCEL_PATH_KEY_ZOOM_NORMAL          ACCEL_PATH_ROOT "ViewZoom100"
#define ACCEL_PATH_KEY_ZOOM_OUT             ACCEL_PATH_ROOT "ViewZoomOut"
#define ACCEL_PATH_KEY_SWITCH_TAB_PREFIX    ACCEL_PATH_ROOT "TabsSwitch"

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
  const char *settings_key;
  const char *action_name;
  const GVariantType *action_parameter_type;
  const char *action_parameter;
  GVariant *parameter;
  gboolean installed;
#if 1
  /* Legacy gtkuimanager menu accelerator */
  const char *legacy_accel_path;
#endif
} KeyEntry;

typedef struct
{
  KeyEntry *key_entry;
  guint n_elements;
  const char *user_visible_name;
} KeyEntryList;

#define ENTRY(name, key, action, type, parameter) \
  { name, key, "win." action, (const GVariantType *) type, parameter, NULL, FALSE, ACCEL_PATH_##key }

static KeyEntry file_entries[] = {
  ENTRY (N_("New Terminal in New Tab"),    KEY_NEW_TAB,       "new-terminal",  "(ss)",  "('tab','current')"   ),
  ENTRY (N_("New Terminal in New Window"), KEY_NEW_WINDOW,    "new-terminal",  "(ss)",  "('window','current')"),
  ENTRY (N_("New Profile"),                KEY_NEW_PROFILE,   "new-profile",   NULL,    NULL                  ),
#ifdef ENABLE_SAVE
  ENTRY (N_("Save Contents"),              KEY_SAVE_CONTENTS, "save-contents", NULL,    NULL                  ),
#endif
  ENTRY (N_("Close Terminal"),             KEY_CLOSE_TAB,     "close",         "s",     "'tab'"               ),
  ENTRY (N_("Close All Terminals"),        KEY_CLOSE_WINDOW,  "close",         "s",     "'window'"            ),
};

static KeyEntry edit_entries[] = {
  ENTRY (N_("Copy"),  KEY_COPY,  "copy",  NULL, NULL      ),
  ENTRY (N_("Paste"), KEY_PASTE, "paste", "s",  "'normal'"),
};

static KeyEntry find_entries[] = {
  ENTRY (N_("Find"),                 KEY_FIND,       "find", "s", "'find'"    ),
  ENTRY (N_("Find Next"),            KEY_FIND_NEXT,  "find", "s", "'next'"    ),
  ENTRY (N_("Find Previous"),        KEY_FIND_PREV,  "find", "s", "'previous'"),
  ENTRY (N_("Clear Find Highlight"), KEY_FIND_CLEAR, "find", "s", "'clear'"   )
};

static KeyEntry view_entries[] = {
  ENTRY (N_("Hide and Show toolbar"), KEY_TOGGLE_MENUBAR, "show-menubar", NULL, NULL),
  ENTRY (N_("Full Screen"),           KEY_FULL_SCREEN,    "fullscreen",   NULL, NULL),
  ENTRY (N_("Zoom In"),               KEY_ZOOM_IN,        "zoom",         "i",  "1" ),
  ENTRY (N_("Zoom Out"),              KEY_ZOOM_OUT,       "zoom",         "i",  "-1"),
  ENTRY (N_("Normal Size"),           KEY_ZOOM_NORMAL,    "zoom",         "i",  "0" )
};

static KeyEntry terminal_entries[] = {
  ENTRY (N_("Reset"),           KEY_RESET,              "reset",     "b",  "false"),
  ENTRY (N_("Reset and Clear"), KEY_RESET_AND_CLEAR,    "reset",     "b",  "true" ),
};

static KeyEntry tabs_entries[] = {
  ENTRY (N_("Switch to Previous Terminal"), KEY_PREV_TAB,       "switch-tab", "i",  "-2"),
  ENTRY (N_("Switch to Next Terminal"),     KEY_NEXT_TAB,       "switch-tab", "i",  "-1"),
  ENTRY (N_("Move Terminal to the Left"),   KEY_MOVE_TAB_LEFT,  "move-tab",   "i",  "-1"),
  ENTRY (N_("Move Terminal to the Right"),  KEY_MOVE_TAB_RIGHT, "move-tab",   "i",  "1" ),
  ENTRY (N_("Detach Terminal"),             KEY_DETACH_TAB,     "detach-tab", NULL, NULL),

#define SWITCH_TAB_ENTRY(num) \
  ENTRY (NULL, \
         KEY_SWITCH_TAB_PREFIX # num, \
         "switch-tab", \
         "i", \
         # num)

  SWITCH_TAB_ENTRY (1),  SWITCH_TAB_ENTRY (2),  SWITCH_TAB_ENTRY (3),  SWITCH_TAB_ENTRY (4),
  SWITCH_TAB_ENTRY (5),  SWITCH_TAB_ENTRY (6),  SWITCH_TAB_ENTRY (7),  SWITCH_TAB_ENTRY (8),
  SWITCH_TAB_ENTRY (9),  SWITCH_TAB_ENTRY (10), SWITCH_TAB_ENTRY (11), SWITCH_TAB_ENTRY (12),
  SWITCH_TAB_ENTRY (13), SWITCH_TAB_ENTRY (14), SWITCH_TAB_ENTRY (15), SWITCH_TAB_ENTRY (16),
  SWITCH_TAB_ENTRY (17), SWITCH_TAB_ENTRY (18), SWITCH_TAB_ENTRY (19), SWITCH_TAB_ENTRY (20),
  SWITCH_TAB_ENTRY (21), SWITCH_TAB_ENTRY (22), SWITCH_TAB_ENTRY (23), SWITCH_TAB_ENTRY (24),
  SWITCH_TAB_ENTRY (25), SWITCH_TAB_ENTRY (26), SWITCH_TAB_ENTRY (27), SWITCH_TAB_ENTRY (28),
  SWITCH_TAB_ENTRY (29), SWITCH_TAB_ENTRY (30), SWITCH_TAB_ENTRY (31), SWITCH_TAB_ENTRY (32),
  SWITCH_TAB_ENTRY (33), SWITCH_TAB_ENTRY (34), SWITCH_TAB_ENTRY (35)

#undef SWITCH_TAB_ENTRY
};

static KeyEntry help_entries[] = {
  ENTRY (N_("Contents"), KEY_HELP, "help", NULL, NULL)
};

#undef ENTRY

static KeyEntryList all_entries[] =
{
  { file_entries, G_N_ELEMENTS (file_entries), N_("File") },
  { edit_entries, G_N_ELEMENTS (edit_entries), N_("Edit") },
  { view_entries, G_N_ELEMENTS (view_entries), N_("View") },
  { find_entries, G_N_ELEMENTS (find_entries), N_("Find") },
  { terminal_entries, G_N_ELEMENTS (terminal_entries), N_("Terminal") },
  { tabs_entries, G_N_ELEMENTS (tabs_entries), N_("Tabs") },
  { help_entries, G_N_ELEMENTS (help_entries), N_("Help") }
};

enum
{
  ACTION_COLUMN,
  KEYVAL_COLUMN,
  N_COLUMNS
};

static GHashTable *settings_key_to_entry;
static GSettings *keybinding_settings = NULL;

GS_DEFINE_CLEANUP_FUNCTION(GtkTreePath*, _terminal_local_free_tree_path, gtk_tree_path_free)
#define terminal_free_tree_path __attribute__((__cleanup__(_terminal_local_free_tree_path)))

static char*
binding_name (guint            keyval,
              GdkModifierType  mask)
{
  if (keyval != 0)
    return gtk_accelerator_name (keyval, mask);

  return g_strdup ("disabled");
}

static void
key_changed_cb (GSettings *settings,
                const char *settings_key,
                gpointer user_data)
{
  GtkApplication *application = user_data;
  KeyEntry *key_entry;
  gs_free char *value = NULL;

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "key %s changed\n",
                         settings_key);

  key_entry = g_hash_table_lookup (settings_key_to_entry, settings_key);
  if (!key_entry)
    {
      /* shouldn't really happen, but let's be safe */
      _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                             "  WARNING: KeyEntry for changed key not found, bailing out\n");
      return;
    }

  value = g_settings_get_string (settings, settings_key);

  if (g_str_equal (value, "disabled")) {
    if (key_entry->installed)
      gtk_application_remove_accelerator (application,
                                          key_entry->action_name,
                                          key_entry->parameter);
    key_entry->installed = FALSE;
  } else {
    gtk_application_add_accelerator (application,
                                     value,
                                     key_entry->action_name,
                                     key_entry->parameter);
    key_entry->installed = TRUE;
  }

#if 1
  /* Legacy gtkuimanager menu accelerator */
  {
    GdkModifierType mods = 0;
    guint key = 0;

    if (!g_str_equal (value, "disabled"))
      gtk_accelerator_parse (value, &key, &mods);

    gtk_accel_map_change_entry (key_entry->legacy_accel_path, key, mods, TRUE);
  }
#endif
}

void
terminal_accels_init (GApplication *application,
                      GSettings *settings)
{
  guint i, j;

  keybinding_settings = g_object_ref (settings);

  settings_key_to_entry = g_hash_table_new (g_str_hash, g_str_equal);

  /* Initialise names of tab_switch_entries */
  j = 1;
  for (i = 0; i < G_N_ELEMENTS (tabs_entries); i++)
    {
      gs_free char *name = NULL;

      if (tabs_entries[i].user_visible_name != NULL)
        continue;

      name = g_strdup_printf (_("Switch to Tab %d"), j++);
      tabs_entries[i].user_visible_name = g_intern_string (name);
    }

  for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
    {
      for (j = 0; j < all_entries[i].n_elements; ++j)
	{
	  KeyEntry *key_entry;

	  key_entry = &(all_entries[i].key_entry[j]);
          if (key_entry->action_parameter) {
            GError *err = NULL;
            key_entry->parameter = g_variant_parse (key_entry->action_parameter_type,
                                                    key_entry->action_parameter,
                                                    NULL, NULL, &err);
            g_assert_no_error (err);

            g_assert (key_entry->parameter != NULL);
          }

          g_hash_table_insert (settings_key_to_entry,
                               (gpointer) key_entry->settings_key,
                               key_entry);

          key_changed_cb (keybinding_settings, key_entry->settings_key, application);
	}
    }

  g_signal_connect (keybinding_settings, "changed", G_CALLBACK (key_changed_cb), application);
}

void
terminal_accels_shutdown (void)
{
  guint i, j;

  for (i = 0; i < G_N_ELEMENTS (all_entries); ++i) {
    for (j = 0; j < all_entries[i].n_elements; ++j) {
      KeyEntry *key_entry;

      key_entry = &(all_entries[i].key_entry[j]);
      if (key_entry->parameter)
        g_variant_unref (key_entry->parameter);
    }
  }

  g_signal_handlers_disconnect_by_func (keybinding_settings,
                                        G_CALLBACK (key_changed_cb),
                                        g_application_get_default ());

  g_clear_pointer (&settings_key_to_entry, (GDestroyNotify) g_hash_table_unref);
  g_clear_object (&keybinding_settings);
}

static gboolean
foreach_row_cb (GtkTreeModel *model,
                GtkTreePath  *path,
                GtkTreeIter  *iter,
                gpointer      data)
{
  const char *key = data;
  KeyEntry *key_entry;

  gtk_tree_model_get (model, iter,
		      KEYVAL_COLUMN, &key_entry,
		      -1);

  if (key_entry == NULL ||
      !g_str_equal (key_entry->settings_key, key))
    return FALSE;

  gtk_tree_model_row_changed (model, path, iter);
  return TRUE;
}

static void
treeview_key_changed_cb (GSettings *settings,
                         const char *key,
                         GtkTreeView *tree_view)
{
  gtk_tree_model_foreach (gtk_tree_view_get_model (tree_view), foreach_row_cb, (gpointer) key);
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  KeyEntry *ke;

  gtk_tree_model_get (model, iter,
                      KEYVAL_COLUMN, &ke,
                      -1);

  if (ke == NULL) {
    /* This is a title row */
    g_object_set (cell,
                  "visible", FALSE,
		  NULL);
  } else {
    gs_free char *value;
    guint key;
    GdkModifierType mods;
    gboolean writable;
    GtkWidget *button = data;

    value = g_settings_get_string (keybinding_settings, ke->settings_key);
    gtk_accelerator_parse (value, &key, &mods);

    writable = g_settings_is_writable (keybinding_settings, ke->settings_key) &&
               gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    g_object_set (cell,
                  "visible", TRUE,
                  "sensitive", writable,
                  "editable", writable,
                  "accel-key", key,
                  "accel-mods", mods,
		  NULL);
  }
}

static void
accel_update (GtkTreeView          *view,
              GtkCellRendererAccel *cell,
              gchar                *path_string,
              guint                 keyval,
              GdkModifierType       mask)
{
  GtkTreeModel *model;
  terminal_free_tree_path GtkTreePath *path = NULL;
  GtkTreeIter iter;
  KeyEntry *ke;
  gs_free char *str = NULL;

  model = gtk_tree_view_get_model (view);

  path = gtk_tree_path_new_from_string (path_string);
  if (!path)
    return;

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, KEYVAL_COLUMN, &ke, -1);

  /* sanity check */
  if (ke == NULL)
    return;

  str = binding_name (keyval, mask);
  g_settings_set_string (keybinding_settings, ke->settings_key, str);
}

static void
accel_edited_callback (GtkCellRendererAccel *cell,
                       gchar                *path_string,
                       guint                 keyval,
                       GdkModifierType       mask,
                       guint                 hardware_keycode,
                       GtkTreeView          *view)
{
  accel_update (view, cell, path_string, keyval, mask);
}

static void
accel_cleared_callback (GtkCellRendererAccel *cell,
                        gchar                *path_string,
                        GtkTreeView          *view)
{
  accel_update (view, cell, path_string, 0, 0);
}

static void
treeview_destroy_cb (GtkWidget *tree_view,
                     gpointer user_data)
{
  g_signal_handlers_disconnect_by_func (keybinding_settings,
                                        G_CALLBACK (treeview_key_changed_cb),
                                        tree_view);
}

#ifdef ENABLE_DEBUG
static void
row_changed (GtkTreeModel *tree_model,
             GtkTreePath  *path,
             GtkTreeIter  *iter,
             gpointer      user_data)
{
  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "ROW-CHANGED [%s]\n", gtk_tree_path_to_string (path) /* leak */);
}
#endif

void
terminal_accels_fill_treeview (GtkWidget *tree_view,
                               GtkWidget *disable_shortcuts_button)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell_renderer;
  gs_unref_object GtkTreeStore *tree = NULL;
  guint i;

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
  gtk_tree_view_column_set_cell_data_func (column, cell_renderer, accel_set_func,
                                           disable_shortcuts_button, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  /* Add the data */

  tree = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

#ifdef ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
    g_signal_connect (tree, "row-changed", G_CALLBACK (row_changed), NULL);
#endif

  for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
    {
      GtkTreeIter parent_iter;
      guint j;

      gtk_tree_store_insert_with_values (tree, &parent_iter, NULL, -1,
                                         ACTION_COLUMN, _(all_entries[i].user_visible_name),
                                         KEYVAL_COLUMN, NULL,
                                         -1);

      for (j = 0; j < all_entries[i].n_elements; ++j)
	{
	  KeyEntry *key_entry = &(all_entries[i].key_entry[j]);
	  GtkTreeIter iter;

          gtk_tree_store_insert_with_values (tree, &iter, &parent_iter, -1,
                                             ACTION_COLUMN, _(key_entry->user_visible_name),
                                             KEYVAL_COLUMN, key_entry,
                                             -1);
	}
    }

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (tree));

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  g_signal_connect (keybinding_settings, "changed",
                    G_CALLBACK (treeview_key_changed_cb), tree_view);
  g_signal_connect (tree_view, "destroy",
                    G_CALLBACK (treeview_destroy_cb), tree);
}
