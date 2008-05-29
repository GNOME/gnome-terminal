/* terminal program */
/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
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

#include <locale.h>

#include "terminal.h"
#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-window.h"
#include "profile-editor.h"
#include "encoding.h"
#include <gconf/gconf-client.h>
#include <bonobo-activation/bonobo-activation-activate.h>
#include <bonobo-activation/bonobo-activation-register.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-help.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-url.h>
#include <libgnomeui/gnome-client.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <gdk/gdkx.h>


/* Settings storage works as follows:
 *   /apps/gnome-terminal/global/
 *   /apps/gnome-terminal/profiles/Foo/
 *
 * It's somewhat tricky to manage the profiles/ dir since we need to track
 * the list of profiles, but gconf doesn't have a concept of notifying that
 * a directory has appeared or disappeared.
 *
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See gnome-terminal 1.x for why.
 *
 * Don't use this code as an example of how to use GConf - it's hugely
 * overcomplicated due to the profiles stuff. Most apps should not
 * have to do scary things of this nature, and should not have
 * a profiles feature.
 *
 */

struct _TerminalAppClass {
  GObjectClass parent_class;

  void (* quit) (TerminalApp *app);
  void (* profile_list_changed) (TerminalApp *app);
  void (* default_profile_changed) (TerminalApp *app);
};

struct _TerminalApp
{
  GObject parent_instance;

  GList *windows;
  GtkWidget *edit_encodings_dialog;
  GtkWidget *new_profile_dialog;
  GtkWidget *manage_profiles_dialog;
  GtkWidget *manage_profiles_list;
  GtkWidget *manage_profiles_new_button;
  GtkWidget *manage_profiles_edit_button;
  GtkWidget *manage_profiles_delete_button;
  GtkWidget *manage_profiles_default_menu;

  GHashTable *profiles;
  char* default_profile_id;
  TerminalProfile *default_profile;
  gboolean default_profile_locked;

  guint profile_list_notify_id;
  guint default_profile_notify_id;

  gboolean use_factory;
};

enum {
  QUIT,
  PROFILES_LIST_CHANGED,
  DEFAULT_PROFILE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
  RESPONSE_CREATE = GTK_RESPONSE_ACCEPT, /* Arghhh: Glade wants a GTK_RESPONSE_* for dialog buttons */
  RESPONSE_CANCEL,
  RESPONSE_DELETE
};

enum
{
  COL_PROFILE,
  NUM_COLUMNS
};

static GConfClient *conf = NULL;
static TerminalApp *global_app = NULL;

#define TERMINAL_STOCK_EDIT "terminal-edit"
#define PROFILE_LIST_KEY CONF_GLOBAL_PREFIX "/profile_list"
#define DEFAULT_PROFILE_KEY CONF_GLOBAL_PREFIX "/default_profile"

/* Helper functions */

static int
profiles_alphabetic_cmp (gconstpointer pa,
                         gconstpointer pb)
{
  TerminalProfile *a = (TerminalProfile *) pa;
  TerminalProfile *b = (TerminalProfile *) pb;
  int result;

  result =  g_utf8_collate (terminal_profile_get_visible_name (a),
			    terminal_profile_get_visible_name (b));
  if (result == 0)
    result = strcmp (terminal_profile_get_name (a),
		     terminal_profile_get_name (b));

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

  if (strcmp (info->target, terminal_profile_get_visible_name (value)) == 0)
    info->result = value;
}

static void
terminal_window_destroyed (TerminalWindow *window,
                           TerminalApp    *app)
{
  g_return_if_fail (g_list_find (app->windows, window));
  
  app->windows = g_list_remove (app->windows, window);
  g_object_unref (G_OBJECT (window));

  /* FIXMEchpe move this to terminal.h */
  if (app->windows == NULL)
    gtk_main_quit ();
}

static void
terminal_app_profile_forgotten_cb (TerminalProfile *profile,
                                   TerminalApp *app)
{
  g_hash_table_remove (app->profiles, terminal_profile_get_name (profile));

  if (profile == app->default_profile)
    app->default_profile = NULL;
    /* FIXMEchpe update default profile! */

  /* FIXMEchpe emit profiles-list-changed signal */
}
      
static TerminalProfile *
terminal_app_create_profile (TerminalApp *app,
                             const char *name)
{
  TerminalProfile *profile;

  profile = terminal_app_get_profile_by_name (app, name);
  g_return_val_if_fail (profile == NULL, profile); /* FIXMEchpe can this happen? */

  profile = _terminal_profile_new (name);
  terminal_profile_update (profile);

  g_signal_connect (profile, "forgotten",
                    G_CALLBACK (terminal_app_profile_forgotten_cb), app);

  g_hash_table_insert (app->profiles,
                       g_strdup (terminal_profile_get_name (profile)),
                       profile /* adopts the refcount */);

  if (app->default_profile == NULL &&
      app->default_profile_id != NULL &&
      strcmp (app->default_profile_id,
              terminal_profile_get_name (profile)) == 0)
    {
      /* We are the default profile */
      app->default_profile = profile;
    }
  
  return profile;
}

static void
terminal_app_delete_profile (TerminalApp *app,
                             TerminalProfile *profile,
                             GtkWindow   *transient_parent)
{
  GList *current_profiles, *tmp;
  GSList *name_list;
  GError *err = NULL;
  char *dir;

  current_profiles = terminal_app_get_profile_list (app);

  /* remove profile from list */
  dir = g_strdup_printf (CONF_PREFIX "/profiles/%s",
                         terminal_profile_get_name (profile));
  gconf_client_recursive_unset (conf, dir,
                                GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
                                &err);
  g_free (dir);

  current_profiles = g_list_remove (current_profiles, profile);

  if (!err)
    {
      /* make list of profile names */
      name_list = NULL;
      tmp = current_profiles;
      while (tmp != NULL)
	{
	  name_list = g_slist_prepend (name_list,
				       g_strdup (terminal_profile_get_name (tmp->data)));
	  tmp = tmp->next;
	}

      g_list_free (current_profiles);

      gconf_client_set_list (conf,
			     CONF_GLOBAL_PREFIX"/profile_list",
			     GCONF_VALUE_STRING,
			     name_list,
			     &err);

      g_slist_foreach (name_list, (GFunc) g_free, NULL);
      g_slist_free (name_list);
    }
  else
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("There was an error deleting the profiles"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s", err->message);
      g_error_free (err);

      g_signal_connect (G_OBJECT (dialog), "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

      gtk_window_present (GTK_WINDOW (dialog));

      g_error_free (err);
    }
}

static GdkScreen*
find_screen_by_display_name (const char *display_name,
                             int         screen_number)
{
  GdkScreen *screen;
  
  /* --screen=screen_number overrides --display */
  
  screen = NULL;
  
  if (display_name == NULL)
    {
      if (screen_number >= 0)
        screen = gdk_display_get_screen (gdk_display_get_default (), screen_number);

      if (screen == NULL)
        screen = gdk_screen_get_default ();

      g_object_ref (G_OBJECT (screen));
    }
  else
    {
      GSList *displays;
      GSList *tmp;
      const char *period;
      GdkDisplay *display;
        
      period = strrchr (display_name, '.');
      if (period)
        {
          unsigned long n;
          char *end;
          
          errno = 0;
          end = (char*) period + 1;
          n = strtoul (period + 1, &end, 0);
          if (errno == 0 && (period + 1) != end)
            screen_number = n;
        }
      
      displays = gdk_display_manager_list_displays (gdk_display_manager_get ());

      display = NULL;
      tmp = displays;
      while (tmp != NULL)
        {
          const char *this_name;

          display = tmp->data;
          this_name = gdk_display_get_name (display);
          
          /* compare without the screen number part */
          if (strncmp (this_name, display_name, period - display_name) == 0)
            break;

          tmp = tmp->next;
        }
      
      g_slist_free (displays);

      if (display == NULL)
        display = gdk_display_open (display_name); /* FIXME we never close displays */
      
      if (display != NULL)
        {
          if (screen_number >= 0)
            screen = gdk_display_get_screen (display, screen_number);
          
          if (screen == NULL)
            screen = gdk_display_get_default_screen (display);

          if (screen)
            g_object_ref (G_OBJECT (screen));
        }
    }

  if (screen == NULL)
    {
      screen = gdk_screen_get_default ();
      g_object_ref (G_OBJECT (screen));
    }
  
  return screen;
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

static GtkTreeModel *
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
      terminal_profile_get_forgotten (selected_profile))
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
profile_combo_box_refill (GtkWidget *widget)
{
  GtkComboBox *combo = GTK_COMBO_BOX (widget);
  GtkTreeIter iter;
  gboolean iter_set;
  TerminalProfile *selected_profile;
  GtkTreeModel *model;
  TerminalApp *app;

  app = terminal_app_get ();

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
profile_combo_box_new (void)
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

  profile_combo_box_refill (combo);
  
  return combo;
}

static void
profile_combo_box_set_selected (GtkWidget *widget,
                                TerminalProfile *selected_profile)
{
  GtkComboBox *combo = GTK_COMBO_BOX (widget);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean found = FALSE;

  model = gtk_combo_box_get_model (combo);
  if (!model)
    return;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  do
    {
      TerminalProfile *profile;

      gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &profile, (int) -1);
      found = (profile == selected_profile);
      g_object_unref (profile);
    } while (!found && gtk_tree_model_iter_next (model, &iter));

  if (found)
    gtk_combo_box_set_active_iter (combo, &iter);
}

static void
profile_combo_box_changed_cb (GtkWidget *widget,
                              TerminalApp *app)
{
  TerminalProfile *profile;

  profile = profile_combo_box_get_selected (widget);
  if (!profile)
    return;

  if (!terminal_profile_get_is_default (profile))
    terminal_profile_set_is_default (profile, TRUE);

  g_object_unref (profile);

  /* FIXMEchpe */
}

static void
profile_list_treeview_refill (GtkWidget *widget)
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
  
  profile_list_treeview_refill (tree_view);

  return tree_view;
}

static void
profile_list_delete_confirm_response_cb (GtkWidget *dialog,
                                         int response,
                                         gpointer data)
{
  TerminalProfile *profile;

  profile = TERMINAL_PROFILE (g_object_get_data (G_OBJECT (dialog), "profile"));
  
  if (response == GTK_RESPONSE_ACCEPT)
    terminal_app_delete_profile (terminal_app_get (), profile,
                                 gtk_window_get_transient_for (GTK_WINDOW (dialog)));

  gtk_widget_destroy (dialog);
}

static void
profile_list_delete_button_clicked_cb (GtkWidget *button,
                                       GtkWidget *widget)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
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

  if (terminal_app_get_profile_count (terminal_app_get ()) == 1)
    {
      g_object_unref (selected_profile);

      terminal_util_show_error_dialog (GTK_WINDOW (transient_parent), NULL,
                                       _("You must have at least one profile; you can't delete all of them."));
      return;
    }

  dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Delete profile \"%s\"?"),
                                   terminal_profile_get_visible_name (selected_profile));

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
                    NULL);
  
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
                             GTK_WINDOW (app->manage_profiles_dialog));
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
                             GTK_WINDOW (app->manage_profiles_dialog));
  g_object_unref (selected_profile);
}

static GList*
find_profile_link (GList      *profiles,
                   const char *name)
{
  GList *tmp;

  tmp = profiles;
  while (tmp != NULL)
    {
      if (strcmp (terminal_profile_get_name (TERMINAL_PROFILE (tmp->data)),
                  name) == 0)
        return tmp;
      
      tmp = tmp->next;
    }

  return NULL;
}

static void
terminal_app_profile_list_notify_cb (GConfClient *conf,
                                     guint        cnxn_id,
                                     GConfEntry  *entry,
                                     gpointer     user_data)
{
  TerminalApp *app = TERMINAL_APP (user_data);
  GConfValue *val;
  GSList *value_list, *tmp_slist;
  GList *known, *tmp_list;
  gboolean need_new_default;
  TerminalProfile *fallback;
  
  known = terminal_app_get_profile_list (app);

  val = gconf_entry_get_value (entry);

  if (val == NULL ||
      val->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (val) != GCONF_VALUE_STRING)
    goto ensure_one_profile;
    
  value_list = gconf_value_get_list (val);

  /* Add any new ones */
  for (tmp_slist = value_list; tmp_slist != NULL; tmp_slist = tmp_slist->next)
    {
      GConfValue *listvalue = (GConfValue *) (tmp_slist->data);
      const char *profile_name;
      GList *link;

      profile_name = gconf_value_get_string (listvalue);
      if (!profile_name)
        continue;

      link = find_profile_link (known, profile_name);
      
      if (link)
        {
          /* make known point to profiles we didn't find in the list */
          known = g_list_delete_link (known, link);
        }
      else
        {
          terminal_app_create_profile (app, profile_name);
        }
    }

ensure_one_profile:

  fallback = NULL;
  if (terminal_app_get_profile_count (app) == 0 ||
      terminal_app_get_profile_count (app) <= g_list_length (known))
    {
      /* We are going to run out, so create the fallback
       * to be sure we always have one. Must be done
       * here before we emit "forgotten" signals so that
       * screens have a profile to fall back to.
       *
       * If the profile with the FALLBACK_ID already exists,
       * we aren't allowed to delete it, unless at least one
       * other profile will still exist. And if you delete
       * all profiles, the FALLBACK_ID profile returns as
       * the living dead.
       */
      fallback = terminal_app_ensure_profile_fallback (app);
    }
  
  /* Forget no-longer-existing profiles */
  need_new_default = FALSE;
  tmp_list = known;
  while (tmp_list != NULL)
    {
      TerminalProfile *forgotten;

      forgotten = TERMINAL_PROFILE (tmp_list->data);

      /* don't allow deleting the fallback if appropriate. */
      if (forgotten != fallback)
        {
          if (terminal_profile_get_is_default (forgotten))
            need_new_default = TRUE;

          /* FIXMEchpe make this out of this loop! */
          terminal_profile_forget (forgotten);
        }
      
      tmp_list = tmp_list->next;
    }

  g_list_free (known);
  
  if (need_new_default)
    {
      TerminalProfile *new_default;

      known = terminal_app_get_profile_list (app);
      
      g_assert (known);
      new_default = known->data;

      g_list_free (known);

      terminal_profile_set_is_default (new_default, TRUE);

      /* FIXMEchpe emit default-profile-changed signal */
    }

  g_assert (terminal_app_get_profile_count (app) > 0);

  if (app->new_profile_dialog)
    {
      GtkWidget *new_profile_base_menu;

      new_profile_base_menu = g_object_get_data (G_OBJECT (app->new_profile_dialog), "base_option_menu");
      profile_combo_box_refill (new_profile_base_menu);
    }
  if (app->manage_profiles_list)
    profile_list_treeview_refill (app->manage_profiles_list);
  if (app->manage_profiles_default_menu)
    profile_combo_box_refill (app->manage_profiles_default_menu);

  tmp_list = app->windows;
  while (tmp_list != NULL)
    {
      terminal_window_reread_profile_list (TERMINAL_WINDOW (tmp_list->data));

      tmp_list = tmp_list->next;
    }
}

static void
terminal_app_default_profile_notify_cb (GConfClient *client,
                                        guint        cnxn_id,
                                        GConfEntry  *entry,
                                        gpointer     user_data)
{
  TerminalApp *app = TERMINAL_APP (user_data);
  TerminalProfile *profile;
  TerminalProfile *old_default;
  GConfValue *val;
  gboolean changed = FALSE;
  gboolean locked;
  const char *name;
  
  val = gconf_entry_get_value (entry);  
  if (val == NULL ||
      val->type != GCONF_VALUE_STRING)
    return;
  
  name = gconf_value_get_string (val);
  if (!name)
    return; /* FIXMEchpe? */

  locked = !gconf_entry_get_is_writable (entry);

  g_free (app->default_profile_id);
  app->default_profile_id = g_strdup (name);

  old_default = app->default_profile;

  profile = terminal_app_get_profile_by_name (app, name);
  /* FIXMEchpe: what if |profile| is NULL here? */

  if (profile != NULL &&
      profile != app->default_profile)
    {
      app->default_profile = profile;
      changed = TRUE;
    }

  if (locked != app->default_profile_locked)
    {
      /* Need to emit changed on all profiles */
      GList *all_profiles;
      GList *tmp;
      TerminalSettingMask mask;

      terminal_setting_mask_clear (&mask);
      mask.is_default = TRUE;
      
      app->default_profile_locked = locked;
      
      all_profiles = terminal_app_get_profile_list (app);
      for (tmp = all_profiles; tmp != NULL; tmp = tmp->next)
        {
//           TerminalProfile *p = tmp->data;
          
//           emit_changed (p, &mask);
        }

      g_list_free (all_profiles);
    }
  else if (changed)
    {
      TerminalSettingMask mask;
      
      terminal_setting_mask_clear (&mask);
      mask.is_default = TRUE;

//       if (old_default)
//         emit_changed (old_default, &mask);

//       emit_changed (profile, &mask);
    }

  /* FIXMEchpe: emit default-profile-changed signal */
}

static void
new_profile_response_callback (GtkWidget *new_profile_dialog,
                               int        response_id,
                               TerminalApp *app)
{
  if (response_id == RESPONSE_CREATE)
    {
      GtkWidget *name_entry;
      char *name;
      char *escaped_name;
      GtkWidget *base_option_menu;
      TerminalProfile *base_profile = NULL;
      TerminalProfile *new_profile;
      GList *profiles;
      GList *tmp;
      GtkWindow *transient_parent;
      GtkWidget *confirm_dialog;
      gint retval;
      GError *err;
      
      name_entry = g_object_get_data (G_OBJECT (new_profile_dialog), "name_entry");
      name = gtk_editable_get_chars (GTK_EDITABLE (name_entry), 0, -1);
      g_strstrip (name); /* name will be non empty after stripping */
      
      profiles = terminal_app_get_profile_list (app);
      for (tmp = profiles; tmp != NULL; tmp = tmp->next)
        {
          TerminalProfile *profile = tmp->data;

          if (strcmp (name, terminal_profile_get_visible_name (profile)) == 0)
            break;
        }
      if (tmp)
        {
          confirm_dialog = gtk_message_dialog_new (GTK_WINDOW (new_profile_dialog), 
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_QUESTION, 
                                                   GTK_BUTTONS_YES_NO, 
                                                   _("You already have a profile called \"%s\". Do you want to create another profile with the same name?"), name);
          retval = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
          gtk_widget_destroy (confirm_dialog);
          if (retval == GTK_RESPONSE_NO)   
            goto cleanup;
        }
      g_list_free (profiles);

      base_option_menu = g_object_get_data (G_OBJECT (new_profile_dialog), "base_option_menu");
      base_profile = profile_combo_box_get_selected (base_option_menu);
      
      if (base_profile == NULL)
        {
          terminal_util_show_error_dialog (GTK_WINDOW (new_profile_dialog), NULL, 
                                          _("The profile you selected as a base for your new profile no longer exists"));
          goto cleanup;
        }

      transient_parent = gtk_window_get_transient_for (GTK_WINDOW (new_profile_dialog));
      
      gtk_widget_destroy (new_profile_dialog);
      
      err = NULL;
      escaped_name = terminal_profile_clone (base_profile, name, &err);
      g_object_unref (base_profile);
      if (err)
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("There was an error creating the profile \"%s\""),
                                           name);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s", err->message);
          g_error_free (err);

          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_window_present (GTK_WINDOW (dialog));
        }
      
      /* FIXMEchpe: we should have the profile in our list by now! */
      new_profile = terminal_app_create_profile (app, escaped_name);

      /* FIXMEchpe: should be obsolete due to gconf notification */
      gconf_client_notify (conf, PROFILE_LIST_KEY);

      g_free (escaped_name);
      
      if (new_profile == NULL)
        {
          terminal_util_show_error_dialog (transient_parent, NULL, 
                                           _("There was an error creating the profile \"%s\""), name);
        }
      else 
        {
          terminal_profile_edit (new_profile, transient_parent);
        }

    cleanup:
      g_free (name);
    }
  else
    {
      gtk_widget_destroy (new_profile_dialog);
    }
}

static void
new_profile_name_entry_changed_callback (GtkEditable *editable, gpointer data)
{
  char *name, *saved_name;
  GtkWidget *create_button;

  create_button = (GtkWidget*) data;

  saved_name = name = gtk_editable_get_chars (editable, 0, -1);

  /* make the create button sensitive only if something other than space has been set */
  while (*name != '\0' && g_ascii_isspace (*name))
    name++;
 
  gtk_widget_set_sensitive (create_button, *name != '\0' ? TRUE : FALSE);

  g_free (saved_name);
}

void
terminal_app_new_profile (TerminalApp     *app,
                          TerminalProfile *default_base_profile,
                          GtkWindow       *transient_parent)
{
  GtkWidget *create_button;

  if (app->new_profile_dialog == NULL)
    {
      GladeXML *xml;
      GtkWidget *w, *wl;
      GtkWidget *create_button, *combo;
      GtkSizeGroup *size_group, *size_group_labels;

      xml = terminal_util_load_glade_file (TERM_GLADE_FILE, "new-profile-dialog", transient_parent);

      if (xml == NULL)
        return;

      app->new_profile_dialog = glade_xml_get_widget (xml, "new-profile-dialog");
      g_signal_connect (G_OBJECT (app->new_profile_dialog), "response", G_CALLBACK (new_profile_response_callback), app);

      terminal_util_set_unique_role (GTK_WINDOW (app->new_profile_dialog), "gnome-terminal-new-profile");
  
      g_object_add_weak_pointer (G_OBJECT (app->new_profile_dialog), (void**) &app->new_profile_dialog);

      create_button = glade_xml_get_widget (xml, "new-profile-create-button");
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "create_button", create_button);
      gtk_widget_set_sensitive (create_button, FALSE);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      size_group_labels = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

      /* the name entry */
      w = glade_xml_get_widget (xml, "new-profile-name-entry");
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "name_entry", w);
      g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (new_profile_name_entry_changed_callback), create_button);
      gtk_entry_set_activates_default (GTK_ENTRY (w), TRUE);
      gtk_widget_grab_focus (w);
      terminal_util_set_atk_name_description (w, NULL, _("Enter profile name"));
      gtk_size_group_add_widget (size_group, w);

      wl = glade_xml_get_widget (xml, "new-profile-name-label");
      gtk_label_set_mnemonic_widget (GTK_LABEL (wl), w);
      gtk_size_group_add_widget (size_group_labels, wl);
 
      /* the base profile option menu */
      w = glade_xml_get_widget (xml, "new-profile-table");
      combo = profile_combo_box_new ();
      gtk_table_attach_defaults (GTK_TABLE (w), combo, 1, 2, 1, 2);
      w = combo;
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "base_option_menu", w);
      terminal_util_set_atk_name_description (w, NULL, _("Choose base profile"));
      gtk_size_group_add_widget (size_group, w);

      wl = glade_xml_get_widget (xml, "new-profile-base-label");
      gtk_label_set_mnemonic_widget (GTK_LABEL (wl), w);
      gtk_size_group_add_widget (size_group_labels, wl);

      gtk_dialog_set_default_response (GTK_DIALOG (app->new_profile_dialog), RESPONSE_CREATE);

      g_object_unref (G_OBJECT (size_group));
      g_object_unref (G_OBJECT (size_group_labels));

      g_object_unref (G_OBJECT (xml));
    }

  gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
                                transient_parent);

  create_button = g_object_get_data (G_OBJECT (app->new_profile_dialog), "create_button");
  gtk_widget_set_sensitive (create_button, FALSE);
  
  gtk_widget_show_all (app->new_profile_dialog);
  gtk_window_present (GTK_WINDOW (app->new_profile_dialog));
}

static void
default_profile_changed (TerminalProfile           *profile,
                         const TerminalSettingMask *mask,
                         void                      *profile_combo_box)
{
  if (mask->is_default)
    {
      if (terminal_profile_get_is_default (profile))
        profile_combo_box_set_selected (GTK_WIDGET (profile_combo_box),
                                         profile);      
    }
}

static void
monitor_profiles_for_is_default_change (GtkWidget *profile_combo_box)
{
  GList *profiles;
  GList *tmp;
  
  profiles = terminal_app_get_profile_list (terminal_app_get ());

  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

      g_signal_connect_object (G_OBJECT (profile),
                               "changed",
                               G_CALLBACK (default_profile_changed),
                               G_OBJECT (profile_combo_box),
                               0);
      
      tmp = tmp->next;
    }

  g_list_free (profiles);
}

static void
manage_profiles_destroyed_callback (GtkWidget   *manage_profiles_dialog,
                                    TerminalApp *app)
{
  app->manage_profiles_dialog = NULL;
  app->manage_profiles_list = NULL;
  app->manage_profiles_new_button = NULL;
  app->manage_profiles_edit_button = NULL;
  app->manage_profiles_delete_button = NULL;
  app->manage_profiles_default_menu = NULL;
}

static void
fix_button_align (GtkWidget *button)
{
  GtkWidget *child;
  GtkWidget *alignment;

  child = gtk_bin_get_child (GTK_BIN (button));

  g_object_ref (child);
  gtk_container_remove (GTK_CONTAINER (button), child);

  alignment = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 12);

  gtk_container_add (GTK_CONTAINER (alignment), child);
  g_object_unref (child);

  gtk_container_add (GTK_CONTAINER (button), alignment);

  if (GTK_IS_ALIGNMENT (child) || GTK_IS_LABEL (child))
    {
      g_object_set (G_OBJECT (child), "xalign", 0.0, NULL);
    }
}

static void
count_selected_profiles_func (GtkTreeModel      *model,
                              GtkTreePath       *path,
                              GtkTreeIter       *iter,
                              gpointer           data)
{
  int *count = data;

  *count += 1;
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            TerminalApp      *app)
{
  int count;

  count = 0;
  gtk_tree_selection_selected_foreach (selection,
                                       count_selected_profiles_func,
                                       &count);

  gtk_widget_set_sensitive (app->manage_profiles_edit_button,
                            count == 1);
  gtk_widget_set_sensitive (app->manage_profiles_delete_button,
                            count > 0);
}

static void
manage_profiles_response_cb (GtkDialog *dialog,
                             int        id,
                             void      *data)
{
  TerminalApp *app;

  app = data;

  g_assert (app->manage_profiles_dialog == GTK_WIDGET (dialog));
  
  if (id == GTK_RESPONSE_HELP)
    terminal_util_show_help ("gnome-terminal-manage-profiles", GTK_WINDOW (dialog));
  else
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
terminal_app_register_stock (void)
{
  static gboolean registered = FALSE;

  if (!registered)
    {
      GtkIconFactory *factory;
      GtkIconSet     *icons;

      static GtkStockItem edit_item [] = {
	{ TERMINAL_STOCK_EDIT, N_("_Edit"), 0, 0, GETTEXT_PACKAGE },
      };

      icons = gtk_icon_factory_lookup_default (GTK_STOCK_PREFERENCES);
      factory = gtk_icon_factory_new ();
      gtk_icon_factory_add (factory, TERMINAL_STOCK_EDIT, icons);
      gtk_icon_factory_add_default (factory);
      gtk_stock_add_static (edit_item, 1);
      registered = TRUE;
    }
}

void
terminal_app_manage_profiles (TerminalApp     *app,
                              GtkWindow       *transient_parent)
{
  GtkWindow *old_transient_parent;

  if (app->manage_profiles_dialog == NULL)
    {
      GtkWidget *main_vbox;
      GtkWidget *vbox;
      GtkWidget *label;
      GtkWidget *sw;
      GtkWidget *hbox;
      GtkWidget *button;
      GtkWidget *spacer;
      GtkRequisition req;
      GtkSizeGroup *size_group;
      GtkTreeSelection *selection;
      
      terminal_app_register_stock ();

      old_transient_parent = NULL;      
      
      app->manage_profiles_dialog =
        gtk_dialog_new_with_buttons (_("Profiles"),
                                     NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_HELP,
                                     GTK_RESPONSE_HELP,
                                     GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (app->manage_profiles_dialog),
                                       GTK_RESPONSE_ACCEPT);
      g_signal_connect (GTK_DIALOG (app->manage_profiles_dialog),
                        "response",
                        G_CALLBACK (manage_profiles_response_cb),
                        app);

      g_signal_connect (G_OBJECT (app->manage_profiles_dialog),
                        "destroy",
                        G_CALLBACK (manage_profiles_destroyed_callback),
                        app);

      terminal_util_set_unique_role (GTK_WINDOW (app->manage_profiles_dialog), "gnome-terminal-profile-manager");

      gtk_widget_set_name (app->manage_profiles_dialog, "profile-manager-dialog");
      gtk_rc_parse_string ("widget \"profile-manager-dialog\" style \"hig-dialog\"\n");

      gtk_dialog_set_has_separator (GTK_DIALOG (app->manage_profiles_dialog), FALSE);
      gtk_container_set_border_width (GTK_CONTAINER (app->manage_profiles_dialog), 10);
      gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (app->manage_profiles_dialog)->vbox), 12);

      main_vbox = gtk_vbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (app->manage_profiles_dialog)->vbox), main_vbox, TRUE, TRUE, 0);
     
      // the top thing
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, TRUE, TRUE, 0);
      
      vbox = gtk_vbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
      
      label = gtk_label_new_with_mnemonic (_("_Profiles:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_size_group_add_widget (size_group, label);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      app->manage_profiles_list = profile_list_treeview_create (app);
      g_signal_connect (app->manage_profiles_list, "row-activated",
                        G_CALLBACK (profile_list_row_activated_cb), NULL);
      
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (sw), app->manage_profiles_list);      
      
      gtk_dialog_set_default_response (GTK_DIALOG (app->manage_profiles_dialog), RESPONSE_CREATE);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), app->manage_profiles_list);

      vbox = gtk_vbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

      spacer = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      gtk_size_group_add_widget (size_group, spacer);      
      gtk_box_pack_start (GTK_BOX (vbox), spacer, FALSE, FALSE, 0);
      
      button = gtk_button_new_from_stock (GTK_STOCK_NEW);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (profile_list_new_button_clicked_cb),
                        app->manage_profiles_list);
      app->manage_profiles_new_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_new_button, NULL,                             
                                              _("Click to open new profile dialog"));
      
      button = gtk_button_new_from_stock (TERMINAL_STOCK_EDIT);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (profile_list_edit_button_clicked_cb),
                        app->manage_profiles_list);
      app->manage_profiles_edit_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_edit_button, NULL,                            
                                              _("Click to open edit profile dialog"));
      
      button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (profile_list_delete_button_clicked_cb),
                        app->manage_profiles_list);
      app->manage_profiles_delete_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_delete_button, NULL,                          
                                              _("Click to delete selected profile"));
      // bottom line
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new_with_mnemonic (_("Profile _used when launching a new terminal:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
            
      app->manage_profiles_default_menu = profile_combo_box_new ();
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), app->manage_profiles_default_menu);
      g_signal_connect (app->manage_profiles_default_menu, "changed",
                        G_CALLBACK (profile_combo_box_changed_cb), app);
      monitor_profiles_for_is_default_change (app->manage_profiles_default_menu);
      gtk_box_pack_start (GTK_BOX (hbox), app->manage_profiles_default_menu, TRUE, TRUE, 0);
 
      /* Set default size of profile list */
      gtk_window_set_geometry_hints (GTK_WINDOW (app->manage_profiles_dialog),
                                     app->manage_profiles_list,
                                     NULL, 0);

      /* Incremental reflow makes this a bit useless, I guess. */
      gtk_widget_size_request (app->manage_profiles_list, &req);
      gtk_window_set_default_size (GTK_WINDOW (app->manage_profiles_dialog),
                                   MIN (req.width + 140, 450),
                                   MIN (req.height + 190, 400));

      gtk_widget_grab_focus (app->manage_profiles_list);

      g_object_unref (G_OBJECT (size_group));

      /* Monitor selection for sensitivity */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->manage_profiles_list));
      selection_changed_callback (selection, app);
      g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (selection_changed_callback), app);
    }
  else 
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->manage_profiles_dialog));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->manage_profiles_dialog),
                                    transient_parent);
      gtk_widget_hide (app->manage_profiles_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->manage_profiles_dialog);
  gtk_window_present (GTK_WINDOW (app->manage_profiles_dialog));
}

static void
terminal_app_get_clone_command (TerminalApp *app,
                                int         *argcp,
                                char      ***argvp)
{
  GList *tmp;
  GPtrArray* args;
  
  args = g_ptr_array_new ();

   g_ptr_array_add (args, g_strdup (EXECUTABLE_NAME));

  if (!app->use_factory)
    {
       g_ptr_array_add (args, g_strdup ("--disable-factory"));
    }

  tmp = app->windows;
  while (tmp != NULL)
    {
      GList *tabs;
      GList *tmp2;
      TerminalWindow *window = tmp->data;
      TerminalScreen *active_screen;

      active_screen = terminal_window_get_active (window);
      
      tabs = terminal_window_list_screens (window);

      tmp2 = tabs;
      while (tmp2 != NULL)
        {
          TerminalScreen *screen = tmp2->data;
          const char *profile_id;
          const char **override_command;
          const char *title;
          double zoom;
          
          profile_id = terminal_profile_get_name (terminal_screen_get_profile (screen));
          
          if (tmp2 == tabs)
            {
               g_ptr_array_add (args, g_strdup_printf ("--window-with-profile-internal-id=%s",
                                                     profile_id));
              if (terminal_window_get_menubar_visible (window))
                 g_ptr_array_add (args, g_strdup ("--show-menubar"));
              else
                 g_ptr_array_add (args, g_strdup ("--hide-menubar"));

               g_ptr_array_add (args, g_strdup_printf ("--role=%s",
                                                     gtk_window_get_role (GTK_WINDOW (window))));
            }
          else
            {
               g_ptr_array_add (args, g_strdup_printf ("--tab-with-profile-internal-id=%s",
                                                     profile_id));
            }

          if (screen == active_screen)
            {
              int w, h, x, y;

              /* FIXME saving the geometry is not great :-/ */
               g_ptr_array_add (args, g_strdup ("--active"));

               g_ptr_array_add (args, g_strdup ("--geometry"));

              terminal_screen_get_size (screen, &w, &h);
              gtk_window_get_position (GTK_WINDOW (window), &x, &y);
              g_ptr_array_add (args, g_strdup_printf ("%dx%d+%d+%d", w, h, x, y));
            }

          override_command = terminal_screen_get_override_command (screen);
          if (override_command)
            {
              char *flattened;

               g_ptr_array_add (args, g_strdup ("--command"));
              
              flattened = g_strjoinv (" ", (char**) override_command);
               g_ptr_array_add (args, flattened);
            }

          title = terminal_screen_get_dynamic_title (screen);
          if (title)
            {
               g_ptr_array_add (args, g_strdup ("--title"));
               g_ptr_array_add (args, g_strdup (title));
            }

          {
            const char *dir;

            dir = terminal_screen_get_working_dir (screen);

            if (dir != NULL && *dir != '\0') /* should always be TRUE anyhow */
              {
                 g_ptr_array_add (args, g_strdup ("--working-directory"));
                g_ptr_array_add (args, g_strdup (dir));
              }
          }

          zoom = terminal_screen_get_font_scale (screen);
          if (zoom < -1e-6 || zoom > 1e-6) /* if not 1.0 */
            {
              char buf[G_ASCII_DTOSTR_BUF_SIZE];

              g_ascii_dtostr (buf, sizeof (buf), zoom);
              
              g_ptr_array_add (args, g_strdup ("--zoom"));
              g_ptr_array_add (args, g_strdup (buf));
            }
          
          tmp2 = tmp2->next;
        }
      
      g_list_free (tabs);
      
      tmp = tmp->next;
    }

  /* final NULL */
  g_ptr_array_add (args, NULL);

  *argcp = args->len;
  *argvp = (char**) g_ptr_array_free (args, FALSE);
}

static gboolean
terminal_app_save_yourself_cb (GnomeClient        *client,
                               gint                phase,
                               GnomeSaveStyle      save_style,
                               gboolean            shutdown,
                               GnomeInteractStyle  interact_style,
                               gboolean            fast,
                               void               *data)
{
  char **clone_command;
  TerminalApp *app;
  int argc;
#ifdef GNOME_ENABLE_DEBUG
  int i;
#endif

  app = data;
  
  terminal_app_get_clone_command (app, &argc, &clone_command);

  /* GnomeClient builds the clone command from the restart command */
  gnome_client_set_restart_command (client, argc, clone_command);

#ifdef GNOME_ENABLE_DEBUG
  /* Debug spew */
  g_print ("Saving session: ");
  i = 0;
  while (clone_command[i])
    {
      g_print ("%s ", clone_command[i]);
      ++i;
    }
  g_print ("\n");
#endif

  g_strfreev (clone_command);
  
  /* success */
  return TRUE;
}

static void
terminal_app_client_die_cb (GnomeClient *client,
                            TerminalApp *app)
{
  g_signal_emit (app, signals[QUIT], 0);
}

/* Class implementation */

G_DEFINE_TYPE (TerminalApp, terminal_app, G_TYPE_OBJECT)

static void
terminal_app_init (TerminalApp *app)
{
  GnomeClient *sm_client;
//   GConfClient *conf;

  global_app = app;

  /* FIXMEchpe leaks */
  app->profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  
  conf = gconf_client_get_default ();

  gconf_client_add_dir (conf, CONF_GLOBAL_PREFIX,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        NULL);
  
  app->profile_list_notify_id =
    gconf_client_notify_add (conf, PROFILE_LIST_KEY,
                             terminal_app_profile_list_notify_cb,
                             app,
                             NULL, NULL);

  app->default_profile_notify_id =
    gconf_client_notify_add (conf,
                             DEFAULT_PROFILE_KEY,
                             terminal_app_default_profile_notify_cb,
                             app,
                             NULL, NULL);
  
  terminal_accels_init ();
  terminal_encoding_init ();
  
  /* And now read the profile list */
  gconf_client_notify (conf, PROFILE_LIST_KEY);
  gconf_client_notify (conf, DEFAULT_PROFILE_KEY);

  g_object_unref (conf);

  sm_client = gnome_master_client ();
  g_signal_connect (sm_client,
                    "save_yourself",
                    G_CALLBACK (terminal_app_save_yourself_cb),
                    terminal_app_get ());

  g_signal_connect (sm_client, "die",
                    G_CALLBACK (terminal_app_client_die_cb),
                    NULL);
}

static void
terminal_app_finalize (GObject *object)
{
  TerminalApp *app = TERMINAL_APP (object);
  GConfClient *conf;

  conf = gconf_client_get_default ();

  if (app->profile_list_notify_id != 0)
    gconf_client_notify_remove (conf, app->profile_list_notify_id);

  gconf_client_remove_dir (conf, CONF_GLOBAL_PREFIX, NULL);

  g_object_unref (conf);

  g_free (app->default_profile_id);

  g_hash_table_destroy (app->profiles);

  G_OBJECT_CLASS (terminal_app_parent_class)->finalize (object);

  global_app = NULL;
}

static void
terminal_app_class_init (TerminalAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = terminal_app_finalize;

  signals[QUIT] =
    g_signal_new (I_("quit"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalAppClass, quit),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/* Public API */

void
terminal_app_initialize (gboolean use_factory)
{
  g_assert (global_app == NULL);
  g_object_new (TERMINAL_TYPE_APP, NULL);
  g_assert (global_app != NULL);

  global_app->use_factory = use_factory;
}

void
terminal_app_shutdown (void)
{
  g_assert (global_app != NULL);
  g_object_unref (global_app);
}

TerminalApp*
terminal_app_get (void)
{
  g_assert (global_app != NULL);
  return global_app;
}

TerminalWindow *
terminal_app_new_window (TerminalApp *app,
                         const char *role,
                         const char *startup_id,
                         const char *display_name,
                         int screen_number)
{
  GdkScreen *gdk_screen;
  TerminalWindow *window;
  
  window = terminal_window_new ();
  g_object_ref (G_OBJECT (window));
  
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (terminal_window_destroyed),
                    app);
  
  app->windows = g_list_append (app->windows, window);

  gdk_screen = find_screen_by_display_name (display_name, screen_number);
  if (gdk_screen != NULL)
    {
      gtk_window_set_screen (GTK_WINDOW (window), gdk_screen);
      g_object_unref (G_OBJECT (gdk_screen));
    }

  if (startup_id != NULL)
    terminal_window_set_startup_id (window, startup_id);
  
  if (role == NULL)
    terminal_util_set_unique_role (GTK_WINDOW (window), "gnome-terminal");
  else
    gtk_window_set_role (GTK_WINDOW (window), role);

  return window;
}

void
terminal_app_new_terminal (TerminalApp     *app,
                           TerminalProfile *profile,
                           TerminalWindow  *window,
                           TerminalScreen  *screen,
                           gboolean         force_menubar_state,
                           gboolean         forced_menubar_state,
                           gboolean         start_fullscreen,
                           char           **override_command,
                           const char      *geometry,
                           const char      *title,
                           const char      *working_dir,
                           const char      *role,
                           double           zoom,
                           const char      *startup_id,
                           const char      *display_name,
                           int              screen_number)
{
  gboolean window_created;
  gboolean screen_created;
  
  g_return_if_fail (profile);

  window_created = FALSE;
  if (window == NULL)
    {
      window = terminal_app_new_window (app, role, startup_id, display_name, screen_number);
      window_created = TRUE;
    }

  if (force_menubar_state)
    {
      terminal_window_set_menubar_visible (window, forced_menubar_state);
    }

  screen_created = FALSE;
  if (screen == NULL)
    {  
      screen_created = TRUE;
      screen = terminal_screen_new ();
      
      terminal_screen_set_profile (screen, profile);
    
      if (title)
        {
          terminal_screen_set_title (screen, title);
          terminal_screen_set_dynamic_title (screen, title, FALSE);
          terminal_screen_set_dynamic_icon_title (screen, title, FALSE);
        }

      if (working_dir)
        terminal_screen_set_working_dir (screen, working_dir);
      
      if (override_command)    
        terminal_screen_set_override_command (screen, override_command);
    
      terminal_screen_set_font_scale (screen, zoom);
      terminal_screen_set_font (screen);
    
      terminal_window_add_screen (window, screen, -1);

      terminal_screen_reread_profile (screen);
    
      terminal_window_set_active (window, screen);
      gtk_widget_grab_focus (GTK_WIDGET (screen));
    }
  else
    {
      TerminalWindow *source_window;

      source_window = terminal_screen_get_window (screen);
      if (source_window)
        {
          g_object_ref_sink (screen);
          terminal_window_remove_screen (source_window, screen);
          terminal_window_add_screen (window, screen, -1);
          g_object_unref (screen);

          terminal_window_set_active (window, screen);
          gtk_widget_grab_focus (GTK_WIDGET (screen));
        }
    }

  if (geometry)
    {
      if (!gtk_window_parse_geometry (GTK_WINDOW (window),
                                      geometry))
        g_printerr (_("Invalid geometry string \"%s\"\n"),
                    geometry);
    }

  if (start_fullscreen)
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
    }

  /* don't present on new tab, or we can accidentally make the
   * terminal jump workspaces.
   * http://bugzilla.gnome.org/show_bug.cgi?id=78253
   */
  if (window_created)
    gtk_window_present (GTK_WINDOW (window));

  if (screen_created)
    terminal_screen_launch_child (screen);
}


void
terminal_app_edit_profile (TerminalApp     *app,
                           TerminalProfile *profile,
                           GtkWindow       *transient_parent)
{
  terminal_profile_edit (profile, transient_parent);
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
  GtkWindow *old_transient_parent;

  if (app->edit_encodings_dialog == NULL)
    {      
      old_transient_parent = NULL;      

      /* passing in transient_parent here purely for the
       * glade error dialog
       */
      app->edit_encodings_dialog =
        terminal_encoding_dialog_new (transient_parent);

      if (app->edit_encodings_dialog == NULL)
        return; /* glade file missing */
      
      g_signal_connect (G_OBJECT (app->edit_encodings_dialog),
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &(app->edit_encodings_dialog));
    }
  else 
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->edit_encodings_dialog));
    }
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->edit_encodings_dialog),
                                    transient_parent);
      gtk_widget_hide (app->edit_encodings_dialog); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (app->edit_encodings_dialog);
  gtk_window_present (GTK_WINDOW (app->edit_encodings_dialog));
}

TerminalWindow *
terminal_app_get_current_window (TerminalApp *app)
{
  return g_list_last (app->windows)->data;
}

/* FIXMEchpe: make this list contain ref'd objects */
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
  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  return g_list_sort (g_hash_table_get_values (app->profiles), profiles_alphabetic_cmp);
}

guint
terminal_app_get_profile_count (TerminalApp *app)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), 0);

  return g_hash_table_size (app->profiles);
}

TerminalProfile*
terminal_app_get_profile_by_name (TerminalApp *app,
                                  const char *name)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (app->profiles, name);
}

TerminalProfile*
terminal_app_get_profile_by_visible_name (TerminalApp *app,
                                          const char *name)
{
  LookupInfo info;

  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  info.result = NULL;
  info.target = name;

  g_hash_table_foreach (app->profiles,
                        profiles_lookup_by_visible_name_foreach,
                        &info);
  return info.result;
}


TerminalProfile*
terminal_app_ensure_profile_fallback (TerminalApp *app)
{
  TerminalProfile *profile;

  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  profile = terminal_app_get_profile_by_name (app, FALLBACK_PROFILE_ID);
  if (profile == NULL)
    profile = terminal_app_create_profile (app, FALLBACK_PROFILE_ID);
  
  return profile;
}

TerminalProfile*
terminal_app_get_default_profile (TerminalApp *app)
{
  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  return app->default_profile;
}

TerminalProfile*
terminal_app_get_profile_for_new_term (TerminalApp *app,
                                       TerminalProfile *current)
{
  GList *list;
  TerminalProfile *profile;

  g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

  if (current)
    return current;
  
  if (app->default_profile)
    return app->default_profile;	

  list = terminal_app_get_profile_list (app);
  if (list)
    profile = list->data;
  else
    profile = NULL;

  g_list_free (list);

  return profile;
}
