/* Accelerator stuff */

/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "terminal-intl.h"
#include "terminal-accels.h"
#include "terminal-profile.h"
#include <string.h>
#include <glade/glade.h>
#include "eggcellrendererkeys.h"

#define D(x)

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
#define PREFIX_KEY_SWITCH_TO_TAB CONF_KEYS_PREFIX"/switch_to_tab_"

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
  { N_("New tab"),
    KEY_NEW_TAB, ACCEL_PATH_NEW_TAB, 0, 0, NULL, FALSE },
  { N_("New window"),
    KEY_NEW_WINDOW, ACCEL_PATH_NEW_WINDOW, 0, 0, NULL, FALSE },
  { N_("New profile"),
    KEY_NEW_PROFILE, ACCEL_PATH_NEW_PROFILE, 0, 0, NULL, FALSE },
  { N_("Close tab"),
    KEY_CLOSE_TAB, ACCEL_PATH_CLOSE_TAB, 0, 0, NULL, FALSE },
  { N_("Close window"),
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
  { N_("Hide and show menubar"),
    KEY_TOGGLE_MENUBAR, ACCEL_PATH_TOGGLE_MENUBAR, 0, 0, NULL, FALSE },
  { N_("Full screen"),
    KEY_FULL_SCREEN, ACCEL_PATH_FULL_SCREEN, 0, 0, NULL, FALSE },
};

static KeyEntry terminal_entries[] =
{
  { N_("Reset"),
    KEY_RESET, ACCEL_PATH_RESET, 0, 0, NULL, FALSE },
  { N_("Reset and clear"),
    KEY_RESET_AND_CLEAR, ACCEL_PATH_RESET_AND_CLEAR, 0, 0, NULL, FALSE },
};

static KeyEntry go_entries[] =
{
  { N_("Switch to previous tab"),
    KEY_PREV_TAB, ACCEL_PATH_PREV_TAB, 0, 0, NULL, FALSE },
  { N_("Switch to next tab"),
    KEY_NEXT_TAB, ACCEL_PATH_NEXT_TAB, 0, 0, NULL, FALSE },
  { N_("Switch to tab 1"),
    PREFIX_KEY_SWITCH_TO_TAB"1",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"1", 0, 0, NULL, FALSE },
  { N_("Switch to tab 2"),
    PREFIX_KEY_SWITCH_TO_TAB"2",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"2", 0, 0, NULL, FALSE },
  { N_("Switch to tab 3"),
    PREFIX_KEY_SWITCH_TO_TAB"3",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"3", 0, 0, NULL, FALSE },
  { N_("Switch to tab 4"),
    PREFIX_KEY_SWITCH_TO_TAB"4",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"4", 0, 0, NULL, FALSE },
  { N_("Switch to tab 5"),
    PREFIX_KEY_SWITCH_TO_TAB"5",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"5", 0, 0, NULL, FALSE },
  { N_("Switch to tab 6"),
    PREFIX_KEY_SWITCH_TO_TAB"6",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"6", 0, 0, NULL, FALSE },
  { N_("Switch to tab 7"),
    PREFIX_KEY_SWITCH_TO_TAB"7",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"7", 0, 0, NULL, FALSE },
  { N_("Switch to tab 8"),
    PREFIX_KEY_SWITCH_TO_TAB"8",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"8", 0, 0, NULL, FALSE },
  { N_("Switch to tab 9"),
    PREFIX_KEY_SWITCH_TO_TAB"9",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"9", 0, 0, NULL, FALSE },
  { N_("Switch to tab 10"),
    PREFIX_KEY_SWITCH_TO_TAB"10",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"10", 0, 0, NULL, FALSE },
  { N_("Switch to tab 11"),
    PREFIX_KEY_SWITCH_TO_TAB"11",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"11", 0, 0, NULL, FALSE },
  { N_("Switch to tab 12"),
    PREFIX_KEY_SWITCH_TO_TAB"12",
    PREFIX_ACCEL_PATH_SWITCH_TO_TAB"12", 0, 0, NULL, FALSE }
};

static KeyEntryList all_entries[] =
{
  { file_entries, G_N_ELEMENTS (file_entries), N_("File") },
  { edit_entries, G_N_ELEMENTS (edit_entries), N_("Edit") },
  { view_entries, G_N_ELEMENTS (view_entries), N_("View") },
  { terminal_entries, G_N_ELEMENTS (terminal_entries), N_("Terminal") },
  { go_entries, G_N_ELEMENTS (go_entries), N_("Go") }
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

static GtkAccelGroup * /* accel_group_i_need_because_gtk_accel_api_sucks */ hack_group = NULL;
static GConfClient *global_conf;
static GSList *living_treeviews = NULL;
static GSList *living_mnemonics_checkbuttons = NULL;
static gboolean using_mnemonics = TRUE;
/* never set gconf keys in response to receiving a gconf notify. */
static int inside_gconf_notify = 0;

void
terminal_accels_init (GConfClient *conf)
{
  GError *err;
  int i, j;
 
  g_return_if_fail (conf != NULL);
  g_return_if_fail (global_conf == NULL);
  
  global_conf = conf;
  g_object_ref (G_OBJECT (global_conf));
  
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
  using_mnemonics = gconf_client_get_bool (global_conf,
                                           CONF_GLOBAL_PREFIX"/use_mnemonics",
                                           &err);
  if (err)
    {
      g_printerr (_("There was an error loading config value for whether to use mnemonics. (%s)\n"),
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
      g_printerr (_("There was an error subscribing to notification for use_mnemonics (%s)\n"),
                  err->message);
      g_error_free (err);
    }
}

GtkAccelGroup*
terminal_accels_get_accel_group (void)
{
  return hack_group;
}

GtkAccelGroup*
terminal_accels_get_group_for_widget (GtkWidget *widget)
{
  GtkAccelGroup *group;

  group = g_object_get_data (G_OBJECT (widget), "terminal-accel-group");

  if (group == NULL)
    {
      group = gtk_accel_group_new ();
      g_object_set_data_full (G_OBJECT (widget),
                              "terminal-accel-group",
                              group,
                              (GDestroyNotify) g_object_unref);
    }

  return group;
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

static gboolean
binding_from_string (const char      *str,
                     guint           *accelerator_key,
                     GdkModifierType *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);
  
  if (str == NULL || (str && strcmp (str, "disabled") == 0))
    {
      *accelerator_key = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  gtk_accelerator_parse (str, accelerator_key, accelerator_mods);

  if (*accelerator_key == 0)
    return FALSE;
  else
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
  int i, j;

  D (g_print ("gconf sync handler\n"));
  
  sync_idle = 0;

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
		  gconf_client_set_string (global_conf,
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
    g_object_set (G_OBJECT (cell),
		  "visible", FALSE,
		  NULL);
  else
    g_object_set (G_OBJECT (cell),
		  "visible", TRUE,
		  "accel_key", ke->gconf_keyval,
		  "accel_mask", ke->gconf_mask,
		  NULL);
}

int
name_compare_func (GtkTreeModel *model,
                   GtkTreeIter  *a,
                   GtkTreeIter  *b,
                   gpointer      user_data)
{
  KeyEntry *ke_a;
  KeyEntry *ke_b;
  
  gtk_tree_model_get (model, a,
                      ACTION_COLUMN, &ke_a,
                      -1);

  gtk_tree_model_get (model, b,
                      ACTION_COLUMN, &ke_b,
                      -1);

  return g_utf8_collate (_(ke_a->user_visible_name),
                         _(ke_b->user_visible_name));
}

int
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

static void
accel_edited_callback (GtkCellRendererText *cell,
                       const char          *path_string,
                       guint                keyval,
                       GdkModifierType      mask,
                       gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  KeyEntry *ke;
  GError *err;
  char *str;
  
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, KEYVAL_COLUMN, &ke, -1);

  /* sanity check */
  if (ke == NULL)
    return;

  str = binding_name (keyval, mask, FALSE);

  D (g_print ("Edited keyval %s, setting gconf to %s\n",
              gdk_keyval_name (keyval) ? gdk_keyval_name (keyval) : "null",
              str));
  
  err = NULL;
  gconf_client_set_string (global_conf,
                           ke->gconf_key,
                           str,
                           &err);
  g_free (str);
  
  if (err != NULL)
    {
      g_printerr (_("Error setting new accelerator in configuration database: %s\n"),
                  err->message);
      
      g_error_free (err);
    }
  
  gtk_tree_path_free (path);
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
      GError *err;
      
      err = NULL;
      gconf_client_set_bool (global_conf,
                             CONF_GLOBAL_PREFIX"/use_mnemonics",
                             !active,
                             &err);
      if (err != NULL)
        {
          g_printerr (_("Error setting use_mnemonics key: %s\n"),
                      err->message);
          
          g_error_free (err);
        }
    }
}

GtkWidget*
terminal_edit_keys_dialog_new (GtkWindow *transient_parent)
{
  GladeXML *xml;
  GtkWidget *w;
  GtkCellRenderer *cell_renderer;
  int i;
  GtkTreeModel *sort_model;
  GtkTreeStore *tree;
  GtkTreeViewColumn *column;
  GtkTreeIter parent_iter;

  if (g_file_test ("./"TERM_GLADE_FILE,
                   G_FILE_TEST_EXISTS))
    {
      /* Try current dir, for debugging */
      xml = glade_xml_new ("./"TERM_GLADE_FILE,
                           "keybindings-dialog",
                           GETTEXT_PACKAGE);
    }
  else
    {
      xml = glade_xml_new (TERM_GLADE_DIR"/"TERM_GLADE_FILE,
                           "keybindings-dialog",
                           GETTEXT_PACKAGE);
    }

  if (xml == NULL)
    {
      static GtkWidget *no_glade_dialog = NULL;
          
      if (no_glade_dialog == NULL)
        {
          no_glade_dialog =
            gtk_message_dialog_new (transient_parent,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE,
                                    _("The file \"%s\" is missing. This indicates that the application is installed incorrectly, so the keybindings dialog can't be displayed."),
                                    TERM_GLADE_DIR"/"TERM_GLADE_FILE);
                                        
          g_signal_connect (G_OBJECT (no_glade_dialog),
                            "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          g_object_add_weak_pointer (G_OBJECT (no_glade_dialog),
                                     (void**)&no_glade_dialog);
        }

      gtk_window_present (GTK_WINDOW (no_glade_dialog));

      return NULL;
    }

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
  
  w = glade_xml_get_widget (xml, "accelerators-treeview");

  living_treeviews = g_slist_prepend (living_treeviews, w);

  g_signal_connect (G_OBJECT (w), "destroy",
                    G_CALLBACK (remove_from_list_callback),
                    &living_treeviews);
  
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (w)),
                               GTK_SELECTION_NONE);


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
  cell_renderer = g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
				"editable", TRUE,
				NULL);
  g_signal_connect (G_OBJECT (cell_renderer), "keys_edited",
                    G_CALLBACK (accel_edited_callback),
                    tree);
  
  g_object_set (G_OBJECT (cell_renderer),
                "editable", TRUE,
                NULL);
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Accelerator _Key"));
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
			  ACTION_COLUMN, all_entries[i].user_visible_name,
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
  
  return w;
}
