/* terminal program */
/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2002 Sun Microsystems
 * Copyright (C) 2003 Mariano Suarez-Alvarez
 * Copyright (C) 2008 Christian Persch
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
};

enum {
  PROFILES_LIST_CHANGED,
  DEFAULT_PROFILE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GConfClient *conf = NULL;
static TerminalApp *global_app = NULL;

#define TERMINAL_STOCK_EDIT "terminal-edit"

static void         sync_profile_list            (TerminalApp *app,
    gboolean use_this_list,
                                                  GSList  *this_list);
static void profile_list_notify   (GConfClient *client,
                                   guint        cnxn_id,
                                   GConfEntry  *entry,
                                   gpointer     user_data);
static void refill_profile_treeview (GtkWidget *tree_view);

static GtkWidget*       profile_optionmenu_new          (void);
static void             profile_optionmenu_refill       (GtkWidget       *option_menu);
static TerminalProfile* profile_optionmenu_get_selected (GtkWidget       *option_menu);
static void             profile_optionmenu_set_selected (GtkWidget       *option_menu,
                                                         TerminalProfile *profile);

/* Helper functions */

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
sync_profile_list (TerminalApp *app,
                   gboolean use_this_list,
                   GSList  *this_list)
{
  GList *known;
  GSList *updated;
  GList *tmp_list;
  GSList *tmp_slist;
  GError *err;
  gboolean need_new_default;
  TerminalProfile *fallback;
  
  known = terminal_profile_get_list ();

  if (use_this_list)
    {
      updated = g_slist_copy (this_list);
    }
  else
    {
      err = NULL;
      updated = gconf_client_get_list (conf,
                                       CONF_GLOBAL_PREFIX"/profile_list",
                                       GCONF_VALUE_STRING,
                                       &err);
      if (err)
        {
          g_printerr (_("There was an error getting the list of terminal profiles. (%s)\n"),
                      err->message);
          g_error_free (err);
        }
    }

  /* Add any new ones */
  tmp_slist = updated;
  while (tmp_slist != NULL)
    {
      GList *link;
      
      link = find_profile_link (known, tmp_slist->data);
      
      if (link)
        {
          /* make known point to profiles we didn't find in the list */
          known = g_list_delete_link (known, link);
        }
      else
        {
          TerminalProfile *profile;
          
          profile = terminal_profile_new (tmp_slist->data);

          terminal_profile_update (profile);
        }

      if (!use_this_list)
        g_free (tmp_slist->data);

      tmp_slist = tmp_slist->next;
    }

  g_slist_free (updated);

  fallback = NULL;
  if (terminal_profile_get_count () == 0 ||
      terminal_profile_get_count () <= g_list_length (known))
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
      fallback = terminal_profile_ensure_fallback (conf);
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
          
          terminal_profile_forget (forgotten);
        }
      
      tmp_list = tmp_list->next;
    }

  g_list_free (known);
  
  if (need_new_default)
    {
      TerminalProfile *new_default;

      known = terminal_profile_get_list ();
      
      g_assert (known);
      new_default = known->data;

      g_list_free (known);

      terminal_profile_set_is_default (new_default, TRUE);
    }

  g_assert (terminal_profile_get_count () > 0);  

  if (app->new_profile_dialog)
    {
      GtkWidget *new_profile_base_menu;

      new_profile_base_menu = g_object_get_data (G_OBJECT (app->new_profile_dialog), "base_option_menu");
      profile_optionmenu_refill (new_profile_base_menu);
    }
  if (app->manage_profiles_list)
    refill_profile_treeview (app->manage_profiles_list);
  if (app->manage_profiles_default_menu)
    profile_optionmenu_refill (app->manage_profiles_default_menu);

  tmp_list = app->windows;
  while (tmp_list != NULL)
    {
      terminal_window_reread_profile_list (TERMINAL_WINDOW (tmp_list->data));

      tmp_list = tmp_list->next;
    }
}

static void
profile_list_notify (GConfClient *client,
                     guint        cnxn_id,
                     GConfEntry  *entry,
                     gpointer     user_data)
{
  GConfValue *val;
  GSList *value_list;
  GSList *string_list;
  GSList *tmp;
  
  val = gconf_entry_get_value (entry);

  if (val == NULL ||
      val->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (val) != GCONF_VALUE_STRING)
    value_list = NULL;
  else
    value_list = gconf_value_get_list (val);

  string_list = NULL;
  tmp = value_list;
  while (tmp != NULL)
    {
      string_list = g_slist_prepend (string_list,
                                     g_strdup (gconf_value_get_string ((GConfValue*)tmp->data)));

      tmp = tmp->next;
    }

  string_list = g_slist_reverse (string_list);
  
  sync_profile_list (terminal_app_get (), TRUE, string_list);

  g_slist_foreach (string_list, (GFunc) g_free, NULL);
  g_slist_free (string_list);
}

enum
{
  RESPONSE_CREATE = GTK_RESPONSE_ACCEPT, /* Arghhh: Glade wants a GTK_RESPONSE_* for dialog buttons */
  RESPONSE_CANCEL,
  RESPONSE_DELETE
};


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
      
      name_entry = g_object_get_data (G_OBJECT (new_profile_dialog), "name_entry");
      name = gtk_editable_get_chars (GTK_EDITABLE (name_entry), 0, -1);
      g_strstrip (name); /* name will be non empty after stripping */
      
      profiles = terminal_profile_get_list ();
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
      base_profile = profile_optionmenu_get_selected (base_option_menu);
      
      if (base_profile == NULL)
        {
          terminal_util_show_error_dialog (GTK_WINDOW (new_profile_dialog), NULL, 
                                          _("The profile you selected as a base for your new profile no longer exists"));
          goto cleanup;
        }

      transient_parent = gtk_window_get_transient_for (GTK_WINDOW (new_profile_dialog));
      
      gtk_widget_destroy (new_profile_dialog);
      
      escaped_name = terminal_profile_create (base_profile, name, transient_parent);
      new_profile = terminal_profile_new (escaped_name);
      terminal_profile_update (new_profile);
      sync_profile_list (app, FALSE, NULL);
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
  GtkWindow *old_transient_parent;
  GtkWidget *create_button;

  if (app->new_profile_dialog == NULL)
    {
      GladeXML *xml;
      GtkWidget *w, *wl;
      GtkWidget *create_button;
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
      w = glade_xml_get_widget (xml, "new-profile-base-option-menu");
      g_object_set_data (G_OBJECT (app->new_profile_dialog), "base_option_menu", w);
      terminal_util_set_atk_name_description (w, NULL, _("Choose base profile"));
      profile_optionmenu_refill (w);
      gtk_size_group_add_widget (size_group, w);

      wl = glade_xml_get_widget (xml, "new-profile-base-label");
      gtk_label_set_mnemonic_widget (GTK_LABEL (wl), w);
      gtk_size_group_add_widget (size_group_labels, wl);

      gtk_dialog_set_default_response (GTK_DIALOG (app->new_profile_dialog), RESPONSE_CREATE);

      g_object_unref (G_OBJECT (size_group));
      g_object_unref (G_OBJECT (size_group_labels));

      g_object_unref (G_OBJECT (xml));
    }

  old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (app->new_profile_dialog));
  
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
                                    transient_parent);
      gtk_widget_hide (app->new_profile_dialog); /* re-show the window on its new parent */
    }

  create_button = g_object_get_data (G_OBJECT (app->new_profile_dialog), "create_button");
  gtk_widget_set_sensitive (create_button, FALSE);
  
  gtk_widget_show_all (app->new_profile_dialog);
  gtk_window_present (GTK_WINDOW (app->new_profile_dialog));
}


enum
{
  COLUMN_NAME,
  COLUMN_PROFILE_OBJECT,
  COLUMN_LAST
};

static void
list_selected_profiles_func (GtkTreeModel      *model,
                             GtkTreePath       *path,
                             GtkTreeIter       *iter,
                             gpointer           data)
{
  GList **list = data;
  TerminalProfile *profile = NULL;

  gtk_tree_model_get (model,
                      iter,
                      COLUMN_PROFILE_OBJECT,
                      &profile,
                      -1);

  *list = g_list_prepend (*list, profile);
}

static void
free_profiles_list (gpointer data)
{
  GList *profiles = data;
  
  g_list_foreach (profiles, (GFunc) g_object_unref, NULL);
  g_list_free (profiles);
}

static void
refill_profile_treeview (GtkWidget *tree_view)
{
  GList *profiles;
  GList *tmp;
  GtkTreeSelection *selection;
  GtkListStore *model;
  GList *selected_profiles;
  GtkTreeIter iter;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)));

  selected_profiles = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       list_selected_profiles_func,
                                       &selected_profiles);

  gtk_list_store_clear (model);
  
  profiles = terminal_profile_get_list ();
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

      gtk_list_store_append (model, &iter);
      
      /* We are assuming the list store will hold a reference to
       * the profile object, otherwise we would be in danger of disappearing
       * profiles.
       */
      gtk_list_store_set (model,
                          &iter,
                          COLUMN_NAME, terminal_profile_get_visible_name (profile),
                          COLUMN_PROFILE_OBJECT, profile,
                          -1);
      
      if (g_list_find (selected_profiles, profile) != NULL)
        gtk_tree_selection_select_iter (selection, &iter);
    
      tmp = tmp->next;
    }

  if (selected_profiles == NULL)
    {
      /* Select first row */
      GtkTreePath *path;
      
      path = gtk_tree_path_new ();
      gtk_tree_path_append_index (path, 0);
      gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)), path);
      gtk_tree_path_free (path);
    }
  
  free_profiles_list (selected_profiles);
}

static GtkWidget*
create_profile_list (void)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkListStore *model;
  
  model = gtk_list_store_new (COLUMN_LAST,
                              G_TYPE_STRING,
                              G_TYPE_OBJECT);
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  terminal_util_set_atk_name_description (tree_view, _("Profile list"), NULL);

  g_object_unref (G_OBJECT (model));
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
                               GTK_SELECTION_MULTIPLE);

  refill_profile_treeview (tree_view);
  
  cell = gtk_cell_renderer_text_new ();

  g_object_set (G_OBJECT (cell),
                "xpad", 2,
                NULL);
  
  column = gtk_tree_view_column_new_with_attributes (NULL,
                                                     cell,
                                                     "text", COLUMN_NAME,
                                                     NULL);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                               GTK_TREE_VIEW_COLUMN (column));

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
  
  return tree_view;
}

static void
delete_confirm_response (GtkWidget   *dialog,
                         int          response_id,
                         GtkWindow   *transient_parent)
{
  GList *deleted_profiles;

  deleted_profiles = g_object_get_data (G_OBJECT (dialog),
                                        "deleted-profiles-list");
  
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      terminal_profile_delete_list (conf, deleted_profiles, transient_parent);
    }

  gtk_widget_destroy (dialog);
}

static void
profile_list_delete_selection (GtkWidget   *profile_list,
                               GtkWindow   *transient_parent,
                               TerminalApp *app)
{
  GtkTreeSelection *selection;
  GList *deleted_profiles;
  GtkWidget *dialog;
  GString *str;
  GList *tmp;
  int count;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (profile_list));

  deleted_profiles = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       list_selected_profiles_func,
                                       &deleted_profiles);

  if (deleted_profiles == NULL)
    {
      terminal_util_show_error_dialog (transient_parent, NULL, _("You must select one or more profiles to delete."));
      return;
    }
  
  count = g_list_length (deleted_profiles);

  if (count == terminal_profile_get_count ())
    {
      free_profiles_list (deleted_profiles);

      terminal_util_show_error_dialog (transient_parent, NULL,
                                       _("You must have at least one profile; you can't delete all of them."));
      return;
    }
  
  if (count > 1)
    {
      str = g_string_new (NULL);
      g_string_printf (str,
                       ngettext ("Delete this profile?\n",
                       "Delete these %d profiles?\n",
                       count),
                       count);

      tmp = deleted_profiles;
      while (tmp != NULL)
        {
          g_string_append (str, "    ");
          g_string_append (str,
                           terminal_profile_get_visible_name (tmp->data));
          if (tmp->next)
            g_string_append (str, "\n");

          tmp = tmp->next;
        }
    }
  else
    {
      str = g_string_new (NULL);
      g_string_printf (str,
                       _("Delete profile \"%s\"?"),
                       terminal_profile_get_visible_name (deleted_profiles->data));
    }
  
  dialog = gtk_message_dialog_new (transient_parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   "%s", 
                                   str->str);
  g_string_free (str, TRUE);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL,
                          GTK_RESPONSE_REJECT,
                          GTK_STOCK_DELETE,
                          GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                   GTK_RESPONSE_ACCEPT);
 
  gtk_window_set_title (GTK_WINDOW (dialog), _("Delete Profile")); 
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  g_object_set_data_full (G_OBJECT (dialog), "deleted-profiles-list",
                          deleted_profiles, free_profiles_list);
  
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (delete_confirm_response),
                    transient_parent);
  
  gtk_widget_show_all (dialog);
}

static void
new_button_clicked (GtkWidget   *button,
                    TerminalApp *app)
{
  terminal_app_new_profile (app,
                            NULL,
                            GTK_WINDOW (app->manage_profiles_dialog));
}

static void
edit_button_clicked (GtkWidget   *button,
                     TerminalApp *app)
{
  GtkTreeSelection *selection;
  GList *profiles;
      
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->manage_profiles_list));

  profiles = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       list_selected_profiles_func,
                                       &profiles);

  if (profiles == NULL)
    return; /* edit button was supposed to be insensitive... */

  if (profiles->next == NULL)
    {
      terminal_app_edit_profile (app,
                                 profiles->data,
                                 GTK_WINDOW (app->manage_profiles_dialog));
    }
  else
    {
      /* edit button was supposed to be insensitive due to multiple
       * selection
       */
    }
  
  g_list_foreach (profiles, (GFunc) g_object_unref, NULL);
  g_list_free (profiles);
}

static void
delete_button_clicked (GtkWidget   *button,
                       TerminalApp *app)
{
  profile_list_delete_selection (app->manage_profiles_list,
                                 GTK_WINDOW (app->manage_profiles_dialog),
                                 app);
}

static void
default_menu_changed (GtkWidget   *option_menu,
                      TerminalApp *app)
{
  TerminalProfile *p;

  p = profile_optionmenu_get_selected (app->manage_profiles_default_menu);

  if (!terminal_profile_get_is_default (p))
    terminal_profile_set_is_default (p, TRUE);
}

static void
default_profile_changed (TerminalProfile           *profile,
                         const TerminalSettingMask *mask,
                         void                      *profile_optionmenu)
{
  if (mask->is_default)
    {
      if (terminal_profile_get_is_default (profile))
        profile_optionmenu_set_selected (GTK_WIDGET (profile_optionmenu),
                                         profile);      
    }
}

static void
monitor_profiles_for_is_default_change (GtkWidget *profile_optionmenu)
{
  GList *profiles;
  GList *tmp;
  
  profiles = terminal_profile_get_list ();

  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

      g_signal_connect_object (G_OBJECT (profile),
                               "changed",
                               G_CALLBACK (default_profile_changed),
                               G_OBJECT (profile_optionmenu),
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
profile_activated_callback (GtkTreeView       *tree_view,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            TerminalApp       *app)
{
  TerminalProfile *profile;
  GtkTreeIter iter;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (tree_view);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;
  
  profile = NULL;
  gtk_tree_model_get (model,
                      &iter,
                      COLUMN_PROFILE_OBJECT,
                      &profile,
                      -1);

  if (profile)
    terminal_app_edit_profile (app,
                               profile,
                               GTK_WINDOW (app->manage_profiles_dialog));
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

      app->manage_profiles_list = create_profile_list ();

      g_signal_connect (G_OBJECT (app->manage_profiles_list), "row_activated",
                        G_CALLBACK (profile_activated_callback), app);
      
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
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (new_button_clicked), app);
      app->manage_profiles_new_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_new_button, NULL,                             
                                              _("Click to open new profile dialog"));
      
      button = gtk_button_new_from_stock (TERMINAL_STOCK_EDIT);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (edit_button_clicked), app);
      app->manage_profiles_edit_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_edit_button, NULL,                            
                                              _("Click to open edit profile dialog"));
      
      button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
      fix_button_align (button);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (delete_button_clicked), app);
      app->manage_profiles_delete_button = button;
      terminal_util_set_atk_name_description (app->manage_profiles_delete_button, NULL,                          
                                              _("Click to delete selected profile"));
      // bottom line
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new_with_mnemonic (_("Profile _used when launching a new terminal:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
            
      app->manage_profiles_default_menu = profile_optionmenu_new ();
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), app->manage_profiles_default_menu);
      if (terminal_profile_get_default ())
        {
          profile_optionmenu_set_selected (app->manage_profiles_default_menu, terminal_profile_get_default ());
        }
      g_signal_connect (G_OBJECT (app->manage_profiles_default_menu), "changed", 
                        G_CALLBACK (default_menu_changed), app);
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

static GtkWidget*
profile_optionmenu_new (void)
{
  GtkWidget *option_menu;
  
  option_menu = gtk_option_menu_new ();
  terminal_util_set_atk_name_description (option_menu, NULL, _("Click button to choose profile"));

  profile_optionmenu_refill (option_menu);
  
  return option_menu;
}

static void
profile_optionmenu_refill (GtkWidget *option_menu)
{
  GList *profiles;
  GList *tmp;
  int i;
  int history;
  GtkWidget *menu;
  GtkWidget *mi;
  TerminalProfile *selected;

  selected = profile_optionmenu_get_selected (option_menu);
  
  menu = gtk_menu_new ();
  
  profiles = terminal_profile_get_list ();

  history = 0;
  i = 0;
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;
      
      mi = gtk_menu_item_new_with_label (terminal_profile_get_visible_name (profile));

      gtk_widget_show (mi);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                             mi);

      g_object_ref (G_OBJECT (profile));
      g_object_set_data_full (G_OBJECT (mi),
                              "profile",
                              profile,
                              (GDestroyNotify) g_object_unref);
      
      if (profile == selected)
        history = i;
      
      ++i;
      tmp = tmp->next;
    }
  
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), history);
  
  g_list_free (profiles);  
}

static TerminalProfile*
profile_optionmenu_get_selected (GtkWidget *option_menu)
{
  GtkWidget *menu;
  GtkWidget *active;

  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  if (menu == NULL)
    return NULL;

  active = gtk_menu_get_active (GTK_MENU (menu));
  if (active == NULL)
    return NULL;

  return g_object_get_data (G_OBJECT (active), "profile");
}

static void
profile_optionmenu_set_selected (GtkWidget       *option_menu,
                                 TerminalProfile *profile)
{
  GtkWidget *menu;
  GList *children;
  GList *tmp;
  int i;
  
  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  if (menu == NULL)
    return;
  
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  i = 0;
  tmp = children;
  while (tmp != NULL)
    {
      if (g_object_get_data (G_OBJECT (tmp->data), "profile") == profile)
        break;
      ++i;
      tmp = tmp->next;
    }
  g_list_free (children);

  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), i);
}

/* Class implementation */

G_DEFINE_TYPE (TerminalApp, terminal_app, G_TYPE_OBJECT)

static void
terminal_app_init (TerminalApp *app)
{
  GError *err;
//   GConfClient *conf;

  global_app = app;

  conf = gconf_client_get_default ();

  err = NULL;
  gconf_client_add_dir (conf, CONF_GLOBAL_PREFIX,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &err);
  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  CONF_GLOBAL_PREFIX, err->message);
      g_error_free (err);
    }
  
  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/profile_list",
                           profile_list_notify,
                           app,
                           NULL, &err);

  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal profile list changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }  

  terminal_accels_init ();
  terminal_encoding_init ();
  
  terminal_profile_initialize (conf);
  sync_profile_list (app, FALSE, NULL);

  g_object_unref (conf);
}

static void
terminal_app_finalize (GObject *object)
{
  TerminalApp *app = TERMINAL_APP (object);
  GConfClient *conf;

  conf = gconf_client_get_default ();

  gconf_client_remove_dir (conf, CONF_GLOBAL_PREFIX, NULL);
//   gconf_client_notify_remove (app->profile_list_notify_id);

  g_object_unref (conf);

  G_OBJECT_CLASS (terminal_app_parent_class)->finalize (object);

  global_app = NULL;
}

static void
terminal_app_class_init (TerminalAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = terminal_app_finalize;
}

/* Public API */

void
terminal_app_initialize (void)
{
  g_assert (global_app == NULL);
  g_object_new (TERMINAL_TYPE_APP, NULL);
  g_assert (global_app != NULL);
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
