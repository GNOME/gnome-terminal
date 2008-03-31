/* Accelerator stuff */
/*
 * Copyright (C) 2001, 2002 Havoc Pennington, Red Hat Inc.
 *
 * This file is part of gnome-terminal.
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include "terminal-intl.h"

#include "terminal-accels.h"
#include "terminal-profile.h"
#include "terminal.h"
#include <string.h>
#include <glade/glade.h>

#define D(x)


#define ACCEL_PATH_ROOT "<Actions>/Main/"
#define ACCEL_PATH_NEW_TAB              ACCEL_PATH_ROOT "FileNewTab"
#define ACCEL_PATH_NEW_WINDOW           ACCEL_PATH_ROOT "FileNewWindow"
#define ACCEL_PATH_NEW_PROFILE          ACCEL_PATH_ROOT "FileNewProfile"
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

#define KEY_NEW_TAB CONF_KEYS_PREFIX"/new_tab"
#define KEY_NEW_WINDOW CONF_KEYS_PREFIX"/new_window"
#define KEY_NEW_PROFILE CONF_KEYS_PREFIX"/new_profile"
#define KEY_CLOSE_TAB CONF_KEYS_PREFIX"/close_tab"
#define KEY_CLOSE_WINDOW CONF_KEYS_PREFIX"/close_window"
#define KEY_COPY CONF_KEYS_PREFIX"/copy"
#define KEY_PASTE CONF_KEYS_PREFIX"/paste"
#define KEY_TOGGLE_MENUBAR CONF_KEYS_PREFIX"/toggle_menubar"
#define KEY_FULL_SCREEN CONF_KEYS_PREFIX"/full_screen"
#define KEY_RESET CONF_KEYS_PREFIX"/reset"
#define KEY_RESET_AND_CLEAR CONF_KEYS_PREFIX"/reset_and_clear"
#define KEY_PREV_TAB CONF_KEYS_PREFIX"/prev_tab"
#define KEY_NEXT_TAB CONF_KEYS_PREFIX"/next_tab"
#define KEY_MOVE_TAB_LEFT CONF_KEYS_PREFIX"/move_tab_left"
#define KEY_MOVE_TAB_RIGHT CONF_KEYS_PREFIX"/move_tab_right"
#define KEY_DETACH_TAB CONF_KEYS_PREFIX"/detach_tab"
#define KEY_SET_TERMINAL_TITLE CONF_KEYS_PREFIX"/set_window_title"
#define KEY_HELP CONF_KEYS_PREFIX"/help"
#define KEY_ZOOM_IN CONF_KEYS_PREFIX"/zoom_in"
#define KEY_ZOOM_OUT CONF_KEYS_PREFIX"/zoom_out"
#define KEY_ZOOM_NORMAL CONF_KEYS_PREFIX"/zoom_normal"

typedef struct
{
  const char *user_visible_name;
  const char *gconf_key;
  const char *accel_path;
  /* last values received from gconf */
  guint gconf_keyval;
  GdkModifierType gconf_mask;
  GClosure *closure;
  /* have gotten a notification from gtk */
  gboolean needs_gconf_sync;
} KeyEntry;

typedef struct
{
  KeyEntry *key_entry;
  gint n_elements;
  gchar *user_visible_name;
} KeyEntryList;

static KeyEntry file_entries[] =
{
  { N_("New Tab"),
    KEY_NEW_TAB, ACCEL_PATH_NEW_TAB, 0, 0, NULL, FALSE },
  { N_("New Window"),
    KEY_NEW_WINDOW, ACCEL_PATH_NEW_WINDOW, 0, 0, NULL, FALSE },
  { N_("New Profile"),
    KEY_NEW_PROFILE, ACCEL_PATH_NEW_PROFILE, 0, 0, NULL, FALSE },
  { N_("Close Tab"),
    KEY_CLOSE_TAB, ACCEL_PATH_CLOSE_TAB, 0, 0, NULL, FALSE },
  { N_("Close Window"),
    KEY_CLOSE_WINDOW, ACCEL_PATH_CLOSE_WINDOW, 0, 0, NULL, FALSE },
};

static KeyEntry edit_entries[] =
{
  { N_("Copy"),
    KEY_COPY, ACCEL_PATH_COPY, 0, 0, NULL, FALSE },
  { N_("Paste"),
    KEY_PASTE, ACCEL_PATH_PASTE, 0, 0, NULL, FALSE },
};

static KeyEntry view_entries[] =
{
  { N_("Hide and Show menubar"),
    KEY_TOGGLE_MENUBAR, ACCEL_PATH_TOGGLE_MENUBAR, 0, 0, NULL, FALSE },
  { N_("Full Screen"),
    KEY_FULL_SCREEN, ACCEL_PATH_FULL_SCREEN, 0, 0, NULL, FALSE },
  { N_("Zoom In"),
    KEY_ZOOM_IN, ACCEL_PATH_ZOOM_IN, 0, 0, NULL, FALSE },
  { N_("Zoom Out"),
    KEY_ZOOM_OUT, ACCEL_PATH_ZOOM_OUT, 0, 0, NULL, FALSE },
  { N_("Normal Size"),
    KEY_ZOOM_NORMAL, ACCEL_PATH_ZOOM_NORMAL, 0, 0, NULL, FALSE }
};

static KeyEntry terminal_entries[] =
{
  { N_("Set Title"),
    KEY_SET_TERMINAL_TITLE, ACCEL_PATH_SET_TERMINAL_TITLE, 0, 0, NULL, FALSE },
  { N_("Reset"),
    KEY_RESET, ACCEL_PATH_RESET, 0, 0, NULL, FALSE },
  { N_("Reset and Clear"),
    KEY_RESET_AND_CLEAR, ACCEL_PATH_RESET_AND_CLEAR, 0, 0, NULL, FALSE },
};

static KeyEntry go_entries[] =
{
  { N_("Switch to Previous Tab"),
    KEY_PREV_TAB, ACCEL_PATH_PREV_TAB, 0, 0, NULL, FALSE },
  { N_("Switch to Next Tab"),
    KEY_NEXT_TAB, ACCEL_PATH_NEXT_TAB, 0, 0, NULL, FALSE },
  { N_("Move Tab to the Left"),
    KEY_MOVE_TAB_LEFT, ACCEL_PATH_MOVE_TAB_LEFT, 0, 0, NULL, FALSE },
  { N_("Move Tab to the Right"),
    KEY_MOVE_TAB_RIGHT, ACCEL_PATH_MOVE_TAB_RIGHT, 0, 0, NULL, FALSE },
  { N_("Detach Tab"),
    KEY_DETACH_TAB, ACCEL_PATH_DETACH_TAB, 0, 0, NULL, FALSE },
};

static KeyEntry help_entries[] = {
  { N_("Contents"), KEY_HELP, ACCEL_PATH_HELP, 0, 0, NULL, FALSE}
};

static KeyEntryList all_entries[] =
{
  { file_entries, G_N_ELEMENTS (file_entries), N_("File") },
  { edit_entries, G_N_ELEMENTS (edit_entries), N_("Edit") },
  { view_entries, G_N_ELEMENTS (view_entries), N_("View") },
  { terminal_entries, G_N_ELEMENTS (terminal_entries), N_("Terminal") },
  { go_entries, G_N_ELEMENTS (go_entries), N_("Go") },
  { help_entries, G_N_ELEMENTS (help_entries), N_("Help") }
};

enum
{
  ACTION_COLUMN,
  KEYVAL_COLUMN,
  N_COLUMNS
};

/*
 * This is kind of annoying. We have two sources of keybinding change;
 * GConf and GtkAccelMap. GtkAccelMap will change if the user uses
 * the magic in-place editing mess. If accel map changes, we propagate
 * into GConf. If GConf changes we propagate into accel map.
 * To avoid infinite loop hell, we short-circuit in both directions
 * if the value is unchanged from last known.
 * The short-circuit is also required because of:
 *  http://bugzilla.gnome.org/show_bug.cgi?id=73082
 *
 *  We have to keep our own hash of the current values in order to
 *  do this short-circuit stuff.
 */

static void keys_change_notify (GConfClient *client,
                                guint        cnxn_id,
                                GConfEntry  *entry,
                                gpointer     user_data);

static void mnemonics_change_notify (GConfClient *client,
                                    guint        cnxn_id,
                                    GConfEntry  *entry,
                                    gpointer     user_data);

static void menu_accels_change_notify (GConfClient *client,
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

static char*    binding_name        (guint            keyval,
                                     GdkModifierType  mask,
                                     gboolean         translate);

static void      queue_gconf_sync (void);

static void      update_menu_accel_state (void);

static GtkAccelGroup * /* accel_group_i_need_because_gtk_accel_api_sucks */ hack_group = NULL;
static GSList *living_treeviews = NULL;
static GSList *living_mnemonics_checkbuttons = NULL;
static GSList *living_menu_accel_checkbuttons = NULL;
static gboolean using_mnemonics = TRUE;
static gboolean using_menu_accels = TRUE;
/* never set gconf keys in response to receiving a gconf notify. */
static int inside_gconf_notify = 0;
static char *saved_menu_accel = NULL;
static GtkWidget *edit_keys_dialog = NULL;

void
terminal_accels_init (void)
{
  GConfClient *conf;
  GError *err;
  int i, j;

  conf = gconf_client_get_default ();
  
  err = NULL;
  gconf_client_add_dir (conf, CONF_KEYS_PREFIX,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &err);
  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  CONF_KEYS_PREFIX, err->message);
      g_error_free (err);
    }

  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_KEYS_PREFIX,
                           keys_change_notify,
                           NULL, /* user_data */
                           NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal keybinding changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }
  
  hack_group = gtk_accel_group_new ();
  
  i = 0;
  while (i < (int) G_N_ELEMENTS (all_entries))
    {
      j = 0;

      while (j < all_entries[i].n_elements)
	{
	  char *str;
	  guint keyval;
	  GdkModifierType mask;
	  KeyEntry *key_entry;

	  key_entry = &(all_entries[i].key_entry[j]);

	  key_entry->closure = g_closure_new_simple (sizeof (GClosure), NULL);

	  g_closure_ref (key_entry->closure);
	  g_closure_sink (key_entry->closure);
	  
	  gtk_accel_group_connect_by_path (hack_group,
					   key_entry->accel_path,
					   key_entry->closure);
      
	  /* Copy from gconf to GTK */
      
	  /* FIXME handle whether the entry is writable
	   *  http://bugzilla.gnome.org/show_bug.cgi?id=73207
	   */

	  err = NULL;
	  str = gconf_client_get_string (conf, key_entry->gconf_key, &err);

	  if (err != NULL)
	    {
	      g_printerr (_("There was an error loading a terminal keybinding. (%s)\n"),
			  err->message);
	      g_error_free (err);
	    }

	  if (binding_from_string (str, &keyval, &mask))
	    {
	      key_entry->gconf_keyval = keyval;
	      key_entry->gconf_mask = mask;
          
	      gtk_accel_map_change_entry (key_entry->accel_path,
					  keyval, mask,
					  TRUE);
	    }
	  else
	    {
	      g_printerr (_("The value of configuration key %s is not valid; value is \"%s\"\n"),
			  key_entry->gconf_key,
			  str ? str : "(null)");
	    }

	  g_free (str);
	  
	  ++j;
	}
      ++i;
    }
  
  g_signal_connect (G_OBJECT (hack_group),
                    "accel_changed",
                    G_CALLBACK (accel_changed_callback),
                    NULL);

  err = NULL;
  using_mnemonics = gconf_client_get_bool (conf,
                                           CONF_GLOBAL_PREFIX"/use_mnemonics",
                                           &err);
  if (err)
    {
      g_printerr (_("There was an error loading config value for whether to use menubar access keys. (%s)\n"),
                  err->message);
      g_error_free (err);
    }

  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/use_mnemonics",
                           mnemonics_change_notify,
                           NULL, /* user_data */
                           NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification on changes on whether to use menubar access keys (%s)\n"),
                  err->message);
      g_error_free (err);
    }

  err = NULL;
  using_menu_accels = gconf_client_get_bool (conf,
                                             CONF_GLOBAL_PREFIX"/use_menu_accelerators",
                                             &err);
  if (err)
    {
      g_printerr (_("There was an error loading config value for whether to use menu accelerators. (%s)\n"),
                  err->message);
      g_error_free (err);
    }

  update_menu_accel_state ();
  
  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/use_menu_accelerators",
                           menu_accels_change_notify,
                           NULL, /* user_data */
                           NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification for use_menu_accelerators (%s)\n"),
                  err->message);
      g_error_free (err);
    }
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

  if (key_entry == (KeyEntry *)data)
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
  GdkModifierType mask;
  guint keyval;

  /* FIXME handle whether the entry is writable
   *  http://bugzilla.gnome.org/show_bug.cgi?id=73207
   */

  D (g_print ("key %s changed\n", gconf_entry_get_key (entry)));
  
  val = gconf_entry_get_value (entry);

  D (if (val == NULL)
     g_print (" changed to be unset\n");
     else if (val->type != GCONF_VALUE_STRING)
     g_print (" changed to non-string value\n");
     else
     g_print (" changed to \"%s\"\n",
              gconf_value_get_string (val)));
  
  if (binding_from_value (val, &keyval, &mask))
    {
      int i;

      i = 0;
      while (i < (int) G_N_ELEMENTS (all_entries))
        {
	  int j;

	  j = 0;
	  while (j < all_entries[i].n_elements)
	    {
	      KeyEntry *key_entry;

	      key_entry = &(all_entries[i].key_entry[j]);
	      if (strcmp (key_entry->gconf_key, gconf_entry_get_key (entry)) == 0)
		{
		  GSList *tmp;
              
		  /* found it */
		  key_entry->gconf_keyval = keyval;
		  key_entry->gconf_mask = mask;

		  /* sync over to GTK */
		  D (g_print ("changing path %s to %s\n",
			      key_entry->accel_path,
			      binding_name (keyval, mask, FALSE))); /* memleak */
		  inside_gconf_notify += 1;
		  gtk_accel_map_change_entry (key_entry->accel_path,
					      keyval, mask,
					      TRUE);
		  inside_gconf_notify -= 1;

		  /* Notify tree views to repaint with new values */
		  tmp = living_treeviews;
		  while (tmp != NULL)
		    {
		      gtk_tree_model_foreach (gtk_tree_view_get_model (GTK_TREE_VIEW (tmp->data)),
					      update_model_foreach,
					      key_entry);
		      tmp = tmp->next;
		    }
              
		  break;
		}
	      ++j;
	    }
	  ++i;
        }
    }
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
  int i;
  
  D (g_print ("Changed accel %s closure %p\n",
              binding_name (keyval, modifier, FALSE), /* memleak */
              accel_closure));

  if (inside_gconf_notify)
    {
      D (g_print ("Ignoring change from gtk because we're inside a gconf notify\n"));
      return;
    }

  i = 0;
  while (i < (int) G_N_ELEMENTS (all_entries))
    {
      int j;

      j = 0;
      while (j < all_entries[i].n_elements)
	{
	  KeyEntry *key_entry;

	  key_entry = &(all_entries[i].key_entry[j]);

	  if (key_entry->closure == accel_closure)
	    {
	      key_entry->needs_gconf_sync = TRUE;
	      queue_gconf_sync ();
	      break;
	    }
	  j++;
	}
      ++i;
    }
}

static void
mnemonics_change_notify (GConfClient *client,
                         guint        cnxn_id,
                         GConfEntry  *entry,
                         gpointer     user_data)
{
  GConfValue *val;
  
  val = gconf_entry_get_value (entry);  
  
  if (strcmp (gconf_entry_get_key (entry),
              CONF_GLOBAL_PREFIX"/use_mnemonics") == 0)
    {
      if (val && val->type == GCONF_VALUE_BOOL)
        {
          if (using_mnemonics != gconf_value_get_bool (val))
            {
              GSList *tmp;
              
              using_mnemonics = !using_mnemonics;

              /* Reset the checkbuttons */
              tmp = living_mnemonics_checkbuttons;
              while (tmp != NULL)
                {
                  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tmp->data),
                                                !using_mnemonics);
                  tmp = tmp->next;
                }
            }
        }
    }
}

static void
menu_accels_change_notify (GConfClient *client,
                           guint        cnxn_id,
                           GConfEntry  *entry,
                           gpointer     user_data)
{
  GConfValue *val;
  
  val = gconf_entry_get_value (entry);  
  
  if (strcmp (gconf_entry_get_key (entry),
              CONF_GLOBAL_PREFIX"/use_menu_accelerators") == 0)
    {
      if (val && val->type == GCONF_VALUE_BOOL)
        {
          if (using_menu_accels != gconf_value_get_bool (val))
            {
              GSList *tmp;
              
              using_menu_accels = !using_menu_accels;

              /* Reset the checkbuttons */
              tmp = living_menu_accel_checkbuttons;
              while (tmp != NULL)
                {
                  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tmp->data),
                                                !using_menu_accels);
                  tmp = tmp->next;
                }

              /* Reset the actual feature; super broken hack alert */
              update_menu_accel_state ();
            }
        }
    }
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

  if (*accelerator_key == 0)
    return FALSE;
    
  return TRUE;
}

static gboolean
binding_from_value (GConfValue       *value,
                    guint            *accelerator_key,
                    GdkModifierType  *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);
  
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

static char*
binding_name (guint            keyval,
              GdkModifierType  mask,
              gboolean         translate)
{
  if (keyval != 0)
    return gtk_accelerator_name (keyval, mask);
  else
    return translate ? g_strdup (_("Disabled")) : g_strdup ("disabled");
}


static guint sync_idle = 0;

static gboolean
sync_handler (gpointer data)
{
  GConfClient *conf;
  int i, j;

  D (g_print ("gconf sync handler\n"));
  
  sync_idle = 0;

  conf = gconf_client_get_default ();

  i = 0;
  while (i < (int) G_N_ELEMENTS (all_entries))
    {
      j = 0;

      while (j < all_entries[i].n_elements)
	{
	  KeyEntry *key_entry;

	  key_entry = &(all_entries[i].key_entry[j]);

	  if (key_entry->needs_gconf_sync)
	    {
	      GtkAccelKey gtk_key;
          
	      key_entry->needs_gconf_sync = FALSE;

	      gtk_key.accel_key = 0;
	      gtk_key.accel_mods = 0;
          
	      gtk_accel_map_lookup_entry (key_entry->accel_path, &gtk_key);
          
	      if (gtk_key.accel_key != key_entry->gconf_keyval ||
		  gtk_key.accel_mods != key_entry->gconf_mask)
		{
		  GError *err;
		  char *accel_name;

		  accel_name = binding_name (gtk_key.accel_key,
					     gtk_key.accel_mods,
					     FALSE);

		  D (g_print ("Setting gconf key %s to \"%s\"\n",
			      key_entry->gconf_key, accel_name));
              
		  err = NULL;
		  gconf_client_set_string (conf,
					   key_entry->gconf_key,
					   accel_name,
					   &err);

		  g_free (accel_name);
              
		  if (err != NULL)
		    {
		      g_printerr (_("Error propagating accelerator change to configuration database: %s\n"),
				  err->message);

		      g_error_free (err);
		    }
		}
	    }
	  ++j;
	}
      ++i;
    }  
  
              
  g_object_unref (conf);

  return FALSE;
}

static void
queue_gconf_sync (void)
{
  if (sync_idle == 0)
    sync_idle = g_idle_add (sync_handler, NULL);
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
		  "accel-key", ke->gconf_keyval,
		  "accel-mods", ke->gconf_mask,
		  NULL);
}

static int
accel_compare_func (GtkTreeModel *model,
                    GtkTreeIter  *a,
                    GtkTreeIter  *b,
                    gpointer      user_data)
{
  KeyEntry *ke_a;
  KeyEntry *ke_b;
  char *name_a;
  char *name_b;
  int result;
  
  gtk_tree_model_get (model, a,
                      KEYVAL_COLUMN, &ke_a,
                      -1);
  if (ke_a == NULL)
    {
      gtk_tree_model_get (model, a,
			  ACTION_COLUMN, &name_a,
			  -1);
    }
  else
    {
      name_a = binding_name (ke_a->gconf_keyval,
			     ke_a->gconf_mask,
			     TRUE);
    }

  gtk_tree_model_get (model, b,
                      KEYVAL_COLUMN, &ke_b,
                      -1);
  if (ke_b == NULL)
    {
      gtk_tree_model_get (model, b,
                          ACTION_COLUMN, &name_b,
                          -1);
    }
  else
    {
      name_b = binding_name (ke_b->gconf_keyval,
			     ke_b->gconf_mask,
			     TRUE);
    }
  
  result = g_utf8_collate (name_a, name_b);

  g_free (name_a);
  g_free (name_b);

  return result;
}

static void
remove_from_list_callback (GtkObject *object, gpointer data)
{
  GSList **listp = data;
  
  *listp = g_slist_remove (*listp, object);
}

static gboolean
cb_check_for_uniqueness (GtkTreeModel *model,
                         GtkTreePath  *path,
                         GtkTreeIter  *iter,
                         gpointer      user_data)
{
  KeyEntry *key_entry = (KeyEntry *) user_data;
  KeyEntry *tmp_key_entry;
 
  gtk_tree_model_get (model, iter,
                      KEYVAL_COLUMN, &tmp_key_entry,
                      -1);
 
  if (tmp_key_entry != NULL &&
      key_entry->gconf_keyval == tmp_key_entry->gconf_keyval &&
      key_entry->gconf_mask   == tmp_key_entry->gconf_mask &&
      /* be sure we don't claim a key is a dup of itself */
      strcmp (key_entry->gconf_key, tmp_key_entry->gconf_key) != 0)
    {
      key_entry->needs_gconf_sync = FALSE;
      key_entry->gconf_key = tmp_key_entry->gconf_key;
      key_entry->user_visible_name = tmp_key_entry->user_visible_name;
      return TRUE;
    }

  return FALSE;
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
  KeyEntry *ke, tmp_key;
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

  tmp_key.gconf_keyval = keyval;
  tmp_key.gconf_mask = mask;
  tmp_key.gconf_key = ke->gconf_key;
  tmp_key.user_visible_name = NULL;
  tmp_key.needs_gconf_sync = TRUE; /* kludge: we'll use this as return flag in the _foreach call */

  if (keyval != 0) 
    {
      gtk_tree_model_foreach (model, cb_check_for_uniqueness, &tmp_key);

      if (!tmp_key.needs_gconf_sync)
        {
          GtkWidget *dialog;
          char *name;

          name = gtk_accelerator_get_label (keyval, mask);

          dialog =
            gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                    GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_WARNING,
                                    GTK_BUTTONS_OK,
                                    _("The shortcut key \"%s\" is already bound to the \"%s\" action"),
                                    name,
                                    tmp_key.user_visible_name ? _(tmp_key.user_visible_name) : tmp_key.gconf_key);
          g_free (name);

          g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_window_present (GTK_WINDOW (dialog));

          return;
        }
    }

  str = binding_name (keyval, mask, FALSE);

  D (g_print ("Edited keyval %s, setting gconf to %s\n",
              gdk_keyval_name (keyval) ? gdk_keyval_name (keyval) : "null",
              str));

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
  KeyEntry *ke, tmp_key;
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

  str = binding_name (0, 0, FALSE);
  D (g_print ("Cleared keybinding for gconf %s", ke->gconf_key));

  conf = gconf_client_get_default ();
  gconf_client_set_string (conf,
                           ke->gconf_key,
                           str,
                           NULL);
  g_object_unref (conf);
  g_free (str);
}

static void
disable_mnemonics_toggled (GtkWidget *button,
                           gpointer   data)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* I confused myself by making the visible button opposite
   * the gconf key
   */
  if (active != (!using_mnemonics))
    {
      GConfClient *conf;
      GError *err;
      
      err = NULL;
      conf = gconf_client_get_default ();
      gconf_client_set_bool (conf,
                             CONF_GLOBAL_PREFIX"/use_mnemonics",
                             !active,
                             &err);
      g_object_unref (conf);
      if (err != NULL)
        {
          g_printerr (_("Error setting %s config key: %s\n"), CONF_GLOBAL_PREFIX"/use_mnemonics", err->message);
          g_error_free (err);
        }
    }
}

static void
disable_menu_accels_toggled (GtkWidget *button,
                             gpointer   data)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  /* I confused myself by making the visible button opposite
   * the gconf key
   */
  if (active != (!using_menu_accels))
    {
      GConfClient *conf;
      GError *err;
      
      err = NULL;
      conf = gconf_client_get_default ();
      gconf_client_set_bool (conf,
                             CONF_GLOBAL_PREFIX"/use_menu_accelerators",
                             !active,
                             &err);
      g_object_unref (conf);
      if (err != NULL)
        {
          g_printerr (_("Error setting use_menu_accelerators key: %s\n"),
                      err->message);
          
          g_error_free (err);
        }
    }
}

typedef struct
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
} IdleData;

static gboolean
real_start_editing_cb (IdleData *idle_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (idle_data->tree_view));
  gtk_tree_view_set_cursor (idle_data->tree_view,
                            idle_data->path,
			    gtk_tree_view_get_column (idle_data->tree_view, 1),
			    TRUE);

  gtk_tree_path_free (idle_data->path);
  g_free (idle_data);

  return FALSE;
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
                  GdkEventButton *event,
		  gpointer        data)
{
  GtkTreePath *path;

  if (event->window != gtk_tree_view_get_bin_window (tree_view))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view,
                                     (gint) event->x,
				     (gint) event->y,
				     &path, NULL,
				     NULL, NULL))
    {
      IdleData *idle_data;

      if (gtk_tree_path_get_depth (path) == 1)
        {
	  gtk_tree_path_free (path);
	  return FALSE;
	}

      idle_data = g_new (IdleData, 1);
      idle_data->tree_view = tree_view;
      idle_data->path = path;
      g_signal_stop_emission_by_name (G_OBJECT (tree_view), "button_press_event");
      g_idle_add ((GSourceFunc) real_start_editing_cb, idle_data);
    }

  return TRUE;
}

void
terminal_edit_keys_dialog_show (GtkWindow *transient_parent)
{
  GladeXML *xml;
  GtkWidget *w;
  GtkCellRenderer *cell_renderer;
  int i;
  GtkTreeModel *sort_model;
  GtkTreeStore *tree;
  GtkTreeViewColumn *column;
  GtkTreeIter parent_iter;

  if (edit_keys_dialog != NULL)
    {
      gtk_window_set_transient_for (GTK_WINDOW (edit_keys_dialog), transient_parent);
      gtk_window_present (GTK_WINDOW (edit_keys_dialog));
      return;
    }

  /* No keybindings editor yet, create one */
  xml = terminal_util_load_glade_file (TERM_GLADE_FILE,
                                       "keybindings-dialog",
                                       transient_parent);
  if (xml == NULL)
    return;
  
  w = glade_xml_get_widget (xml, "disable-mnemonics-checkbutton");
  living_mnemonics_checkbuttons = g_slist_prepend (living_mnemonics_checkbuttons,
                                                   w);
  g_signal_connect (G_OBJECT (w), "destroy",
                    G_CALLBACK (remove_from_list_callback),
                    &living_mnemonics_checkbuttons);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), !using_mnemonics);
  g_signal_connect (G_OBJECT (w), "toggled",
                    G_CALLBACK (disable_mnemonics_toggled),
                    NULL);

  w = glade_xml_get_widget (xml, "disable-menu-accel-checkbutton");
  living_mnemonics_checkbuttons = g_slist_prepend (living_menu_accel_checkbuttons,
                                                   w);
  g_signal_connect (G_OBJECT (w), "destroy",
                    G_CALLBACK (remove_from_list_callback),
                    &living_menu_accel_checkbuttons);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), !using_menu_accels);
  g_signal_connect (G_OBJECT (w), "toggled",
                    G_CALLBACK (disable_menu_accels_toggled),
                    NULL);
  
  w = glade_xml_get_widget (xml, "accelerators-treeview");

  living_treeviews = g_slist_prepend (living_treeviews, w);

  g_signal_connect (G_OBJECT (w), "button_press_event",
		    G_CALLBACK (start_editing_cb), NULL);
  g_signal_connect (G_OBJECT (w), "destroy",
                    G_CALLBACK (remove_from_list_callback),
                    &living_treeviews);

  tree = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
  
  /* Column 1 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Action"),
						     cell_renderer,
						     "text", ACTION_COLUMN,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);
  gtk_tree_view_column_set_sort_column_id (column, ACTION_COLUMN);

  /* Column 2 */
  cell_renderer = gtk_cell_renderer_accel_new ();
  g_object_set (cell_renderer,
                "editable", TRUE,
                "accel_mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                NULL);
  g_signal_connect (cell_renderer, "accel-edited",
                    G_CALLBACK (accel_edited_callback), w);
  g_signal_connect (cell_renderer, "accel-cleared",
                    G_CALLBACK (accel_cleared_callback), w);
  
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Shortcut _Key"));
  gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, cell_renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_sort_column_id (column, KEYVAL_COLUMN);  
  gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);

  /* Add the data */

  i = 0;
  while (i < (gint) G_N_ELEMENTS (all_entries))
    {
      int j;
      gtk_tree_store_append (tree, &parent_iter, NULL);
      gtk_tree_store_set (tree, &parent_iter,
			  ACTION_COLUMN, _(all_entries[i].user_visible_name),
			  -1);
      j = 0;

      while (j < all_entries[i].n_elements)
	{
	  GtkTreeIter iter;
	  KeyEntry *key_entry;

	  key_entry = &(all_entries[i].key_entry[j]);
	  gtk_tree_store_append (tree, &iter, &parent_iter);
	  gtk_tree_store_set (tree, &iter,
			      ACTION_COLUMN, _(key_entry->user_visible_name),
			      KEYVAL_COLUMN, key_entry,
			      -1);
	  ++j;
	}
      ++i;
    }


  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree));
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model),
                                   KEYVAL_COLUMN, accel_compare_func,
                                   NULL, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (w), sort_model);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (w));
  g_object_unref (G_OBJECT (tree));
  
  w = glade_xml_get_widget (xml, "keybindings-dialog");

  g_signal_connect (G_OBJECT (w), "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
  gtk_window_set_default_size (GTK_WINDOW (w),
                               -1, 350);

  terminal_util_set_unique_role (GTK_WINDOW (w), "gnome-terminal-accels");

  g_object_unref (xml);

  edit_keys_dialog = w;

  gtk_window_set_transient_for (GTK_WINDOW (edit_keys_dialog), transient_parent);
  g_signal_connect (edit_keys_dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed), &edit_keys_dialog);

  gtk_window_present (GTK_WINDOW (edit_keys_dialog));
}

static void
update_menu_accel_state (void)
{
  /* Now this is a bad hack on so many levels. */
  
  if (saved_menu_accel == NULL)
    {
      g_object_get (G_OBJECT (gtk_settings_get_default ()),
                    "gtk-menu-bar-accel",
                    &saved_menu_accel,
                    NULL);
      /* FIXME if gtkrc is reparsed we don't catch on,
       * I guess.
       */
    }
  
  if (using_menu_accels)
    {
      gtk_settings_set_string_property (gtk_settings_get_default (),
                                        "gtk-menu-bar-accel",
                                        saved_menu_accel,
                                        "gnome-terminal");
    }
  else
    {
      gtk_settings_set_string_property (gtk_settings_get_default (),
                                        "gtk-menu-bar-accel",
                                        /* no one will ever press this ;-) */
                                        "<Shift><Control><Mod1><Mod2><Mod3><Mod4><Mod5>F10",
                                        "gnome-terminal");
    }
}
