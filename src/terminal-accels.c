/*
 * Copyright © 2001, 2002 Havoc Pennington, Red Hat Inc.
 * Copyright © 2008 Christian Persch
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

#if GTK_CHECK_VERSION (2, 90, 7)
#define GDK_KEY(symbol) GDK_KEY_##symbol
#else
#include <gdk/gdkkeysyms.h>
#define GDK_KEY(symbol) GDK_##symbol
#endif

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-profile.h"
#include "terminal-util.h"

/* NOTES
 *
 * There are two sources of keybindings changes, from GConf and from
 * the accel map (happens with in-place menu editing).
 *
 * When a keybinding gconf key changes, we propagate that into the
 * accel map.
 * When the accel map changes, we queue a sync to gconf.
 *
 * To avoid infinite loops, we short-circuit in both directions
 * if the value is unchanged from last known.
 *
 * In the keybinding editor, when editing or clearing an accel, we write
 * the change directly to gconf and rely on the gconf callback to
 * actually apply the change to the accel map.
 */

#define ACCEL_PATH_ROOT "<Actions>/Main/"
#define ACCEL_PATH_NEW_TAB              ACCEL_PATH_ROOT "FileNewTab"
#define ACCEL_PATH_NEW_WINDOW           ACCEL_PATH_ROOT "FileNewWindow"
#define ACCEL_PATH_NEW_PROFILE          ACCEL_PATH_ROOT "FileNewProfile"
#define ACCEL_PATH_SAVE_CONTENTS        ACCEL_PATH_ROOT "FileSaveContents"
#define ACCEL_PATH_CLOSE_TAB            ACCEL_PATH_ROOT "FileCloseTab"
#define ACCEL_PATH_CLOSE_WINDOW         ACCEL_PATH_ROOT "FileCloseWindow"
#define ACCEL_PATH_COPY                 ACCEL_PATH_ROOT "EditCopy"
#define ACCEL_PATH_PASTE                ACCEL_PATH_ROOT "EditPaste"
#define ACCEL_PATH_TOGGLE_MENUBAR       ACCEL_PATH_ROOT "ViewMenubar"
#define ACCEL_PATH_FULL_SCREEN          ACCEL_PATH_ROOT "ViewFullscreen"
#define ACCEL_PATH_RESET                ACCEL_PATH_ROOT "TerminalReset"
#define ACCEL_PATH_RESET_AND_CLEAR      ACCEL_PATH_ROOT "TerminalResetClear"
#define ACCEL_PATH_PREV_TAB             ACCEL_PATH_ROOT "TabsPrevious"
#define ACCEL_PATH_NEXT_TAB             ACCEL_PATH_ROOT "TabsNext"
#define ACCEL_PATH_SET_TERMINAL_TITLE   ACCEL_PATH_ROOT "TerminalSetTitle"
#define ACCEL_PATH_HELP                 ACCEL_PATH_ROOT "HelpContents"
#define ACCEL_PATH_ZOOM_IN              ACCEL_PATH_ROOT "ViewZoomIn"
#define ACCEL_PATH_ZOOM_OUT             ACCEL_PATH_ROOT "ViewZoomOut"
#define ACCEL_PATH_ZOOM_NORMAL          ACCEL_PATH_ROOT "ViewZoom100"
#define ACCEL_PATH_MOVE_TAB_LEFT        ACCEL_PATH_ROOT "TabsMoveLeft"
#define ACCEL_PATH_MOVE_TAB_RIGHT       ACCEL_PATH_ROOT "TabsMoveRight"
#define ACCEL_PATH_DETACH_TAB           ACCEL_PATH_ROOT "TabsDetach"
#define ACCEL_PATH_SWITCH_TAB_PREFIX    ACCEL_PATH_ROOT "TabsSwitch"

#define KEY_CLOSE_TAB           CONF_KEYS_PREFIX "/close_tab"
#define KEY_CLOSE_WINDOW        CONF_KEYS_PREFIX "/close_window"
#define KEY_COPY                CONF_KEYS_PREFIX "/copy"
#define KEY_DETACH_TAB          CONF_KEYS_PREFIX "/detach_tab"
#define KEY_FULL_SCREEN         CONF_KEYS_PREFIX "/full_screen"
#define KEY_HELP                CONF_KEYS_PREFIX "/help"
#define KEY_MOVE_TAB_LEFT       CONF_KEYS_PREFIX "/move_tab_left"
#define KEY_MOVE_TAB_RIGHT      CONF_KEYS_PREFIX "/move_tab_right"
#define KEY_NEW_PROFILE         CONF_KEYS_PREFIX "/new_profile"
#define KEY_NEW_TAB             CONF_KEYS_PREFIX "/new_tab"
#define KEY_NEW_WINDOW          CONF_KEYS_PREFIX "/new_window"
#define KEY_NEXT_TAB            CONF_KEYS_PREFIX "/next_tab"
#define KEY_PASTE               CONF_KEYS_PREFIX "/paste"
#define KEY_PREV_TAB            CONF_KEYS_PREFIX "/prev_tab"
#define KEY_RESET_AND_CLEAR     CONF_KEYS_PREFIX "/reset_and_clear"
#define KEY_RESET               CONF_KEYS_PREFIX "/reset"
#define KEY_SAVE_CONTENTS       CONF_KEYS_PREFIX "/save_contents"
#define KEY_SET_TERMINAL_TITLE  CONF_KEYS_PREFIX "/set_window_title"
#define KEY_TOGGLE_MENUBAR      CONF_KEYS_PREFIX "/toggle_menubar"
#define KEY_ZOOM_IN             CONF_KEYS_PREFIX "/zoom_in"
#define KEY_ZOOM_NORMAL         CONF_KEYS_PREFIX "/zoom_normal"
#define KEY_ZOOM_OUT            CONF_KEYS_PREFIX "/zoom_out"
#define KEY_SWITCH_TAB_PREFIX   CONF_KEYS_PREFIX "/switch_to_tab_"

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
  const char *gconf_key;
  const char *accel_path;
  /* last values received from gconf */
  GdkModifierType gconf_mask;
  guint gconf_keyval;
  GClosure *closure;
  /* have gotten a notification from gtk */
  gboolean needs_gconf_sync;
  gboolean accel_path_unlocked;
} KeyEntry;

typedef struct
{
  KeyEntry *key_entry;
  guint n_elements;
  const char *user_visible_name;
} KeyEntryList;

static KeyEntry file_entries[] =
{
  { N_("New Tab"),
    KEY_NEW_TAB, ACCEL_PATH_NEW_TAB, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (t), NULL, FALSE, TRUE },
  { N_("New Window"),
    KEY_NEW_WINDOW, ACCEL_PATH_NEW_WINDOW, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (n), NULL, FALSE, TRUE },
  { N_("New Profile"),
    KEY_NEW_PROFILE, ACCEL_PATH_NEW_PROFILE, 0, 0, NULL, FALSE, TRUE },
#ifdef ENABLE_SAVE
  { N_("Save Contents"),
    KEY_SAVE_CONTENTS, ACCEL_PATH_SAVE_CONTENTS, 0, 0, NULL, FALSE, TRUE },
#endif
  { N_("Close Tab"),
    KEY_CLOSE_TAB, ACCEL_PATH_CLOSE_TAB, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (w), NULL, FALSE, TRUE },
  { N_("Close Window"),
    KEY_CLOSE_WINDOW, ACCEL_PATH_CLOSE_WINDOW, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (q), NULL, FALSE, TRUE },
};

static KeyEntry edit_entries[] =
{
  { N_("Copy"),
    KEY_COPY, ACCEL_PATH_COPY, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (c), NULL, FALSE, TRUE },
  { N_("Paste"),
    KEY_PASTE, ACCEL_PATH_PASTE, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (v), NULL, FALSE, TRUE },
};

static KeyEntry view_entries[] =
{
  { N_("Hide and Show menubar"),
    KEY_TOGGLE_MENUBAR, ACCEL_PATH_TOGGLE_MENUBAR, 0, 0, NULL, FALSE, TRUE },
  { N_("Full Screen"),
    KEY_FULL_SCREEN, ACCEL_PATH_FULL_SCREEN, 0, GDK_KEY (F11), NULL, FALSE, TRUE },
  { N_("Zoom In"),
    KEY_ZOOM_IN, ACCEL_PATH_ZOOM_IN, GDK_CONTROL_MASK, GDK_KEY (plus), NULL, FALSE, TRUE },
  { N_("Zoom Out"),
    KEY_ZOOM_OUT, ACCEL_PATH_ZOOM_OUT, GDK_CONTROL_MASK, GDK_KEY (minus), NULL, FALSE, TRUE },
  { N_("Normal Size"),
    KEY_ZOOM_NORMAL, ACCEL_PATH_ZOOM_NORMAL, GDK_CONTROL_MASK, GDK_KEY (0), NULL, FALSE, TRUE }
};

static KeyEntry terminal_entries[] =
{
  { N_("Set Title"),
    KEY_SET_TERMINAL_TITLE, ACCEL_PATH_SET_TERMINAL_TITLE, 0, 0, NULL, FALSE, TRUE },
  { N_("Reset"),
    KEY_RESET, ACCEL_PATH_RESET, 0, 0, NULL, FALSE, TRUE },
  { N_("Reset and Clear"),
    KEY_RESET_AND_CLEAR, ACCEL_PATH_RESET_AND_CLEAR, 0, 0, NULL, FALSE, TRUE },
};

static KeyEntry tabs_entries[] =
{
  { N_("Switch to Previous Tab"),
    KEY_PREV_TAB, ACCEL_PATH_PREV_TAB, GDK_CONTROL_MASK, GDK_KEY (Page_Up), NULL, FALSE, TRUE },
  { N_("Switch to Next Tab"),
    KEY_NEXT_TAB, ACCEL_PATH_NEXT_TAB, GDK_CONTROL_MASK, GDK_KEY (Page_Down), NULL, FALSE, TRUE },
  { N_("Move Tab to the Left"),
    KEY_MOVE_TAB_LEFT, ACCEL_PATH_MOVE_TAB_LEFT, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (Page_Up), NULL, FALSE, TRUE },
  { N_("Move Tab to the Right"),
    KEY_MOVE_TAB_RIGHT, ACCEL_PATH_MOVE_TAB_RIGHT, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY (Page_Down), NULL, FALSE, TRUE },
  { N_("Detach Tab"),
    KEY_DETACH_TAB, ACCEL_PATH_DETACH_TAB, 0, 0, NULL, FALSE, TRUE },
  { N_("Switch to Tab 1"),
    KEY_SWITCH_TAB_PREFIX "1",
    ACCEL_PATH_SWITCH_TAB_PREFIX "1", GDK_MOD1_MASK, GDK_KEY (1), NULL, FALSE, TRUE },
  { N_("Switch to Tab 2"),
    KEY_SWITCH_TAB_PREFIX "2",
    ACCEL_PATH_SWITCH_TAB_PREFIX "2", GDK_MOD1_MASK, GDK_KEY (2), NULL, FALSE, TRUE },
  { N_("Switch to Tab 3"),
    KEY_SWITCH_TAB_PREFIX "3",
    ACCEL_PATH_SWITCH_TAB_PREFIX "3", GDK_MOD1_MASK, GDK_KEY (3), NULL, FALSE, TRUE },
  { N_("Switch to Tab 4"),
    KEY_SWITCH_TAB_PREFIX "4",
    ACCEL_PATH_SWITCH_TAB_PREFIX "4", GDK_MOD1_MASK, GDK_KEY (4), NULL, FALSE, TRUE },
  { N_("Switch to Tab 5"),
    KEY_SWITCH_TAB_PREFIX "5",
    ACCEL_PATH_SWITCH_TAB_PREFIX "5", GDK_MOD1_MASK, GDK_KEY (5), NULL, FALSE, TRUE },
  { N_("Switch to Tab 6"),
    KEY_SWITCH_TAB_PREFIX "6",
    ACCEL_PATH_SWITCH_TAB_PREFIX "6", GDK_MOD1_MASK, GDK_KEY (6), NULL, FALSE, TRUE },
  { N_("Switch to Tab 7"),
    KEY_SWITCH_TAB_PREFIX "7",
    ACCEL_PATH_SWITCH_TAB_PREFIX "7", GDK_MOD1_MASK, GDK_KEY (7), NULL, FALSE, TRUE },
  { N_("Switch to Tab 8"),
    KEY_SWITCH_TAB_PREFIX "8",
    ACCEL_PATH_SWITCH_TAB_PREFIX "8", GDK_MOD1_MASK, GDK_KEY (8), NULL, FALSE, TRUE },
  { N_("Switch to Tab 9"),
    KEY_SWITCH_TAB_PREFIX "9",
    ACCEL_PATH_SWITCH_TAB_PREFIX "9", GDK_MOD1_MASK, GDK_KEY (9), NULL, FALSE, TRUE },
  { N_("Switch to Tab 10"),
    KEY_SWITCH_TAB_PREFIX "10",
    ACCEL_PATH_SWITCH_TAB_PREFIX "10", GDK_MOD1_MASK, GDK_KEY (0), NULL, FALSE, TRUE },
  { N_("Switch to Tab 11"),
    KEY_SWITCH_TAB_PREFIX "11",
    ACCEL_PATH_SWITCH_TAB_PREFIX "11", 0, 0, NULL, FALSE, TRUE },
  { N_("Switch to Tab 12"),
    KEY_SWITCH_TAB_PREFIX "12",
    ACCEL_PATH_SWITCH_TAB_PREFIX "12", 0, 0, NULL, FALSE, TRUE }
};

static KeyEntry help_entries[] = {
  { N_("Contents"), KEY_HELP, ACCEL_PATH_HELP, 0, GDK_KEY (F1), NULL, FALSE, TRUE }
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
  KEYVAL_COLUMN,
  N_COLUMNS
};

static void keys_change_notify (GConfClient *client,
                                guint        cnxn_id,
                                GConfEntry  *entry,
                                gpointer     user_data);

static void accel_changed_callback (GtkAccelGroup  *accel_group,
                                    guint           keyval,
                                    GdkModifierType modifier,
                                    GClosure       *accel_closure,
                                    gpointer        data);

static gboolean binding_from_string (const char      *str,
                                     guint           *accelerator_key,
                                     GdkModifierType *accelerator_mods);

static gboolean binding_from_value  (GConfValue       *value,
                                     guint           *accelerator_key,
                                     GdkModifierType *accelerator_mods);

static gboolean sync_idle_cb (gpointer data);

static guint sync_idle_id = 0;
static GtkAccelGroup *notification_group = NULL;
/* never set gconf keys in response to receiving a gconf notify. */
static int inside_gconf_notify = 0;
static GtkWidget *edit_keys_dialog = NULL;
static GtkTreeStore *edit_keys_store = NULL;
static guint gconf_notify_id;
static GHashTable *gconf_key_to_entry;

static char*
binding_name (guint            keyval,
              GdkModifierType  mask)
{
  if (keyval != 0)
    return gtk_accelerator_name (keyval, mask);

  return g_strdup ("disabled");
}

static const char *
key_from_gconf_key (const char *gconf_key)
{
  const char *last_slash = strrchr (gconf_key, '/');
  if (last_slash)
    return ++last_slash;
  return NULL;
}

void
terminal_accels_init (void)
{
  GConfClient *conf;
  guint i, j;

  conf = gconf_client_get_default ();
  
  gconf_client_add_dir (conf, CONF_KEYS_PREFIX,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        NULL);
  gconf_notify_id =
  gconf_client_notify_add (conf,
                           CONF_KEYS_PREFIX,
                           keys_change_notify,
                           NULL, NULL, NULL);

  gconf_key_to_entry = g_hash_table_new (g_str_hash, g_str_equal);

  notification_group = gtk_accel_group_new ();

  for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
    {
      for (j = 0; j < all_entries[i].n_elements; ++j)
	{
	  KeyEntry *key_entry;

	  key_entry = &(all_entries[i].key_entry[j]);

          g_hash_table_insert (gconf_key_to_entry,
                               (gpointer) key_from_gconf_key (key_entry->gconf_key),
                               key_entry);

	  key_entry->closure = g_closure_new_simple (sizeof (GClosure), key_entry);

	  g_closure_ref (key_entry->closure);
	  g_closure_sink (key_entry->closure);
	  
	  gtk_accel_group_connect_by_path (notification_group,
					   I_(key_entry->accel_path),
					   key_entry->closure);

          gconf_client_notify (conf, key_entry->gconf_key);
	}
    }

  g_object_unref (conf);
  
  g_signal_connect (notification_group, "accel-changed",
                    G_CALLBACK (accel_changed_callback), NULL);
}

void
terminal_accels_shutdown (void)
{
  GConfClient *conf;

  conf = gconf_client_get_default ();
  gconf_client_notify_remove (conf, gconf_notify_id);
  gconf_client_remove_dir (conf, CONF_KEYS_PREFIX, NULL);
  g_object_unref (conf);

  if (sync_idle_id != 0)
    {
      g_source_remove (sync_idle_id);
      sync_idle_id = 0;

      sync_idle_cb (NULL);
    }

  g_hash_table_destroy (gconf_key_to_entry);
  gconf_key_to_entry = NULL;

  g_object_unref (notification_group);
  notification_group = NULL;
}

static gboolean
update_model_foreach (GtkTreeModel *model,
		      GtkTreePath  *path,
		      GtkTreeIter  *iter,
		      gpointer      data)
{
  KeyEntry *key_entry = NULL;

  gtk_tree_model_get (model, iter,
		      KEYVAL_COLUMN, &key_entry,
		      -1);

  if (key_entry == (KeyEntry *) data)
    {
      gtk_tree_model_row_changed (model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
keys_change_notify (GConfClient *client,
                    guint        cnxn_id,
                    GConfEntry  *entry,
                    gpointer     user_data)
{
  GConfValue *val;
  KeyEntry *key_entry;
  GdkModifierType mask;
  guint keyval;

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "key %s changed\n",
                         gconf_entry_get_key (entry));
  
  val = gconf_entry_get_value (entry);

#ifdef GNOME_ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
    {
      if (val == NULL)
        _terminal_debug_print (TERMINAL_DEBUG_ACCELS, " changed to be unset\n");
      else if (val->type != GCONF_VALUE_STRING)
        _terminal_debug_print (TERMINAL_DEBUG_ACCELS, " changed to non-string value\n");
      else
        _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                               " changed to \"%s\"\n",
                               gconf_value_get_string (val));
    }
#endif

  key_entry = g_hash_table_lookup (gconf_key_to_entry, key_from_gconf_key (gconf_entry_get_key (entry)));
  if (!key_entry)
    {
      /* shouldn't really happen, but let's be safe */
      _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                             "  WARNING: KeyEntry for changed key not found, bailing out\n");
      return;
    }

  if (!binding_from_value (val, &keyval, &mask))
    {
      const char *str = val->type == GCONF_VALUE_STRING ? gconf_value_get_string (val) : NULL;
      g_printerr ("The value \"%s\" of configuration key %s is not a valid accelerator\n",
                  str ? str : "(null)",
                  key_entry->gconf_key);
      return;
    }

  key_entry->gconf_keyval = keyval;
  key_entry->gconf_mask = mask;

  /* Unlock the path, so we can change its accel */
  if (!key_entry->accel_path_unlocked)
    gtk_accel_map_unlock_path (key_entry->accel_path);

  /* sync over to GTK */
  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "changing path %s to %s\n",
                         key_entry->accel_path,
                         binding_name (keyval, mask)); /* memleak */
  inside_gconf_notify += 1;
  /* Note that this may return FALSE, e.g. when the entry was already set correctly. */
  gtk_accel_map_change_entry (key_entry->accel_path,
                              keyval, mask,
                              TRUE);
  inside_gconf_notify -= 1;

  /* Lock the path if the gconf key isn't writable */
  key_entry->accel_path_unlocked = gconf_entry_get_is_writable (entry);
  if (!key_entry->accel_path_unlocked)
    gtk_accel_map_lock_path (key_entry->accel_path);

  /* This seems necessary to update the tree model, since sometimes the
   * notification on the notification_group seems not to be emitted correctly.
   * Without this change, when trying to set an accel to e.g. Alt-T (while the main
   * menu in the terminal windows is _Terminal with Alt-T mnemonic) only displays
   * the accel change after a re-expose of the row.
   * FIXME: Find out *why* the accel-changed signal is wrong here!
   */
  if (edit_keys_store)
    gtk_tree_model_foreach (GTK_TREE_MODEL (edit_keys_store), update_model_foreach, key_entry);
}

static void
accel_changed_callback (GtkAccelGroup  *accel_group,
                        guint           keyval,
                        GdkModifierType modifier,
                        GClosure       *accel_closure,
                        gpointer        data)
{
  /* FIXME because GTK accel API is so nonsensical, we get
   * a notify for each closure, on both the added and the removed
   * accelerator. We just use the accel closure to find our
   * accel entry, then update the value of that entry.
   * We use an idle function to avoid setting the entry
   * in gconf when the accelerator gets removed and then
   * setting it again when it gets added.
   */
  KeyEntry *key_entry;
  
  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "Changed accel %s closure %p\n",
                         binding_name (keyval, modifier), /* memleak */
                         accel_closure);

  if (inside_gconf_notify)
    {
      _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                             "Ignoring change from gtk because we're inside a gconf notify\n");
      return;
    }

  key_entry = accel_closure->data;
  g_assert (key_entry);

  key_entry->needs_gconf_sync = TRUE;

  if (sync_idle_id == 0)
    sync_idle_id = g_idle_add (sync_idle_cb, NULL);
}

static gboolean
binding_from_string (const char      *str,
                     guint           *accelerator_key,
                     GdkModifierType *accelerator_mods)
{
  if (str == NULL ||
      strcmp (str, "disabled") == 0)
    {
      *accelerator_key = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  gtk_accelerator_parse (str, accelerator_key, accelerator_mods);
  if (*accelerator_key == 0 &&
      *accelerator_mods == 0)
    return FALSE;

  return TRUE;
}

static gboolean
binding_from_value (GConfValue       *value,
                    guint            *accelerator_key,
                    GdkModifierType  *accelerator_mods)
{
  if (value == NULL)
    {
      /* unset */
      *accelerator_key = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  if (value->type != GCONF_VALUE_STRING)
    return FALSE;

  return binding_from_string (gconf_value_get_string (value),
                              accelerator_key,
                              accelerator_mods);
}

static void
add_key_entry_to_changeset (gpointer key,
                            KeyEntry *key_entry,
                            GConfChangeSet *changeset)
{
  GtkAccelKey gtk_key;

  if (!key_entry->needs_gconf_sync)
    return;

  key_entry->needs_gconf_sync = FALSE;

  if (gtk_accel_map_lookup_entry (key_entry->accel_path, &gtk_key) &&
      (gtk_key.accel_key != key_entry->gconf_keyval ||
       gtk_key.accel_mods != key_entry->gconf_mask))
    {
      char *accel_name;

      accel_name = binding_name (gtk_key.accel_key, gtk_key.accel_mods);
      gconf_change_set_set_string (changeset,  key_entry->gconf_key, accel_name);
      g_free (accel_name);
    }
}
              
static gboolean
sync_idle_cb (gpointer data)
{
  GConfClient *conf;
  GConfChangeSet *changeset;
  GError *error = NULL;

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "gconf sync handler\n");
  
  sync_idle_id = 0;

  conf = gconf_client_get_default ();

  changeset = gconf_change_set_new ();
  g_hash_table_foreach (gconf_key_to_entry, (GHFunc) add_key_entry_to_changeset, changeset);
  if (!gconf_client_commit_change_set (conf, changeset, TRUE, &error))
    {
      g_printerr ("Error committing the accelerator changeset: %s\n", error->message);
      g_error_free (error);
    }
              
  gconf_change_set_unref (changeset);
  g_object_unref (conf);

  return FALSE;
}

/* We have the same KeyEntry* in both columns;
 * we only have two columns because we want to be able
 * to sort by either one of them.
 */

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

  if (ke == NULL)
    /* This is a title row */
    g_object_set (cell,
                  "visible", FALSE,
		  NULL);
  else
    g_object_set (cell,
                  "visible", TRUE,
                  "sensitive", ke->accel_path_unlocked,
                  "editable", ke->accel_path_unlocked,
                  "accel-key", ke->gconf_keyval,
                  "accel-mods", ke->gconf_mask,
		  NULL);
}

static void
treeview_accel_changed_cb (GtkAccelGroup  *accel_group,
                           guint keyval,
                           GdkModifierType modifier,
                           GClosure *accel_closure,
                           GtkTreeModel *model)
{
  gtk_tree_model_foreach (model, update_model_foreach, accel_closure->data);
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
  KeyEntry *ke;
  GtkAccelGroupEntry *entries;
  guint n_entries;
  char *str;
  GConfClient *conf;

  model = gtk_tree_view_get_model (view);

  path = gtk_tree_path_new_from_string (path_string);
  if (!path)
    return;

  if (!gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, KEYVAL_COLUMN, &ke, -1);

  /* sanity check */
  if (ke == NULL)
    return;

  /* Check if we already have an entry using this accel */
  entries = gtk_accel_group_query (notification_group, keyval, mask, &n_entries);
  if (n_entries > 0)
    {
      if (entries[0].accel_path_quark != g_quark_from_string (ke->accel_path))
        {
          GtkWidget *dialog;
          char *name;
          KeyEntry *other_key;

          name = gtk_accelerator_get_label (keyval, mask);
          other_key = entries[0].closure->data;
          g_assert (other_key);

          dialog =
            gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                    GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_WARNING,
                                    GTK_BUTTONS_OK,
                                    _("The shortcut key “%s” is already bound to the “%s” action"),
                                    name,
                                    other_key->user_visible_name ? _(other_key->user_visible_name) : other_key->gconf_key);
          g_free (name);

          g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_window_present (GTK_WINDOW (dialog));
        }

      return;
    }

  str = binding_name (keyval, mask);

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "Edited path %s keyval %s, setting gconf to %s\n",
                         ke->accel_path,
                         gdk_keyval_name (keyval) ? gdk_keyval_name (keyval) : "null",
                         str);
#ifdef GNOME_ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
    {
      GtkAccelKey old_key;

      if (gtk_accel_map_lookup_entry (ke->accel_path, &old_key)) {
        _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                               "  Old entry of path %s is keyval %s mask %x\n",
                               ke->accel_path, gdk_keyval_name (old_key.accel_key), old_key.accel_mods);
      } else {
        _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                               "  Failed to look up the old entry of path %s\n",
                               ke->accel_path);
      }
    }
#endif

  conf = gconf_client_get_default ();
  gconf_client_set_string (conf,
                           ke->gconf_key,
                           str,
                           NULL);
  g_object_unref (conf);
  g_free (str);
}

static void
accel_cleared_callback (GtkCellRendererAccel *cell,
                        gchar                *path_string,
                        GtkTreeView          *view)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  KeyEntry *ke;
  char *str;
  GConfClient *conf;

  model = gtk_tree_view_get_model (view);

  path = gtk_tree_path_new_from_string (path_string);
  if (!path)
    return;

  if (!gtk_tree_model_get_iter (model, &iter, path)) {
    gtk_tree_path_free (path);
    return;
  }
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, KEYVAL_COLUMN, &ke, -1);

  /* sanity check */
  if (ke == NULL)
    return;

  ke->gconf_keyval = 0;
  ke->gconf_mask = 0;
  ke->needs_gconf_sync = TRUE;

  str = binding_name (0, 0);

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "Cleared keybinding for gconf %s",
                         ke->gconf_key);

  conf = gconf_client_get_default ();
  gconf_client_set_string (conf,
                           ke->gconf_key,
                           str,
                           NULL);
  g_object_unref (conf);
  g_free (str);
}

static void
edit_keys_dialog_destroy_cb (GtkWidget *widget,
                             gpointer user_data)
{
  g_signal_handlers_disconnect_by_func (notification_group, G_CALLBACK (treeview_accel_changed_cb), user_data);
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

#ifdef GNOME_ENABLE_DEBUG
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
terminal_edit_keys_dialog_show (GtkWindow *transient_parent)
{
  TerminalApp *app;
  GtkWidget *dialog, *tree_view, *disable_mnemonics_button, *disable_menu_accel_button;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell_renderer;
  GtkTreeStore *tree;
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

  app = terminal_app_get ();
  terminal_util_bind_object_property_to_widget (G_OBJECT (app), TERMINAL_APP_ENABLE_MNEMONICS,
                                                disable_mnemonics_button, 0);
  terminal_util_bind_object_property_to_widget (G_OBJECT (app), TERMINAL_APP_ENABLE_MENU_BAR_ACCEL,
                                                disable_menu_accel_button, 0);

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

  tree = edit_keys_store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

#ifdef GNOME_ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
    g_signal_connect (tree, "row-changed", G_CALLBACK (row_changed), NULL);
#endif

  for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
    {
      GtkTreeIter parent_iter;
      guint j;

      gtk_tree_store_append (tree, &parent_iter, NULL);
      gtk_tree_store_set (tree, &parent_iter,
			  ACTION_COLUMN, _(all_entries[i].user_visible_name),
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
  g_object_unref (tree);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  g_signal_connect (notification_group, "accel-changed",
                    G_CALLBACK (treeview_accel_changed_cb), tree);

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
