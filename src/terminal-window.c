/* widget for a toplevel terminal window, possibly containing multiple terminals */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
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

#include "terminal-intl.h"
#include "terminal-accels.h"
#include "terminal-widget.h"
#include "terminal-window.h"
#include "terminal.h"
#include "encoding.h"
#include <string.h>
#include <stdlib.h>
#include <libgnome/gnome-program.h>
#include <gtk/gtklabel.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <libsn/sn-launchee.h>

/* Parameters for the ellipsization in the Tabs menu */
#define TAB_MENU_WIDTH_CHARS    35
#define TAB_MENU_ELLIPSIZE      PANGO_ELLIPSIZE_END

struct _TerminalWindowPrivate
{  
  GtkWidget *main_vbox;
  GtkWidget *menubar;
  GtkWidget *notebook;
  GtkWidget *file_menuitem;
  GtkWidget *edit_menuitem;
  GtkWidget *view_menuitem;
  GtkWidget *terminal_menuitem;
  GtkWidget *go_menuitem;
  GtkWidget *help_menuitem;
  GtkWidget *new_window_menuitem;
  GtkWidget *new_tab_menuitem;
  GtkWidget *close_tab_menuitem;
  GtkWidget *copy_menuitem;
  GtkWidget *paste_menuitem;
  GtkWidget *show_menubar_menuitem;
  GtkWidget *zoom_in_menuitem;
  GtkWidget *zoom_out_menuitem;
  GtkWidget *zoom_normal_menuitem;
  GtkWidget *edit_config_menuitem;
  GtkWidget *choose_config_menuitem;
  GtkWidget *next_tab_menuitem;
  GtkWidget *previous_tab_menuitem;
  GtkWidget *move_left_tab_menuitem;
  GtkWidget *move_right_tab_menuitem;
  GtkWidget *detach_tab_menuitem;
  GtkWidget *go_menu;
  GtkWidget *encoding_menuitem;
  GList *tab_menuitems;
  guint terms;
  TerminalScreen *active_term;
  GdkPixbuf *icon;
  GtkClipboard *clipboard;
  int old_char_width;
  int old_char_height;
  void *old_geometry_widget; /* only used for pointer value as it may be freed */
  GConfClient *conf;
  guint notify_id;
  char *startup_id;
  guint menubar_visible : 1;
  guint use_default_menubar_visibility : 1;
  guint use_mnemonics : 1;   /* config key value */
  guint using_mnemonics : 1; /* current menubar state */

  /* FIXME we brokenly maintain this flag here instead of
   * being event-driven, because it's too annoying to be
   * event-driven while GTK doesn't support _NET_WM_STATE_FULLSCREEN
   */
  guint fullscreen : 1;

  /* Compositing manager integration */
  guint have_argb_visual : 1;

  guint disposed : 1;
  guint present_on_insert : 1;
};

enum {
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};

static void terminal_window_init        (TerminalWindow      *window);
static void terminal_window_class_init  (TerminalWindowClass *klass);
static void terminal_window_dispose     (GObject             *object);
static void terminal_window_finalize    (GObject             *object);

static gboolean terminal_window_delete_event (GtkWidget *widget,
                                              GdkEvent *event,
                                              gpointer data);

static void       screen_set_menuitem  (TerminalScreen *screen,
                                        GtkWidget      *menuitem);
static GtkWidget* screen_get_menuitem  (TerminalScreen *screen);

static void notebook_page_selected_callback  (GtkWidget       *notebook,
                                              GtkNotebookPage *useless_crap,
                                              guint            page_num,
                                              TerminalWindow  *window);
static void notebook_page_added_callback     (GtkWidget       *notebook,
                                              TerminalScreen  *screen,
                                              guint            page_num,
                                              TerminalWindow  *window);
static void notebook_page_removed_callback   (GtkWidget       *notebook,
                                              TerminalScreen  *screen,
                                              guint            page_num,
                                              TerminalWindow  *window);
static void notebook_page_reordered_callback (GtkWidget      *notebook,
                                              GtkWidget      *child,
                                              guint           page_num,
                                              TerminalWindow *window);

static void config_change_notify            (GConfClient *client,
                                             guint        cnxn_id,
                                             GConfEntry  *entry,
                                             gpointer     user_data);

static void reset_menubar_labels          (TerminalWindow *window);
static void reset_tab_menuitems           (TerminalWindow *window);

static void new_window                    (TerminalWindow  *window,
                                           TerminalScreen  *screen,
                                           TerminalProfile *profile,
                                           char            *name,
                                           const char      *dir);
static void new_window_callback           (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void new_tab_callback              (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static gboolean key_press_callback	  (GtkWidget      *widget,
					   GdkEventKey    *event,
					   TerminalWindow *window);
static void close_window_callback         (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void close_tab_callback            (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void copy_callback                 (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void paste_callback                (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void edit_keybindings_callback     (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void change_configuration_callback (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void edit_configuration_callback   (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void new_configuration_callback    (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void manage_configurations_callback(GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void toggle_menubar_callback         (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void fullscreen_callback           (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void zoom_in_callback              (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void zoom_out_callback             (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void zoom_normal_callback          (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void set_title_callback            (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void change_encoding_callback      (GtkWidget      *menu_item,
                                           TerminalWindow *window);
static void add_encoding_callback         (GtkWidget      *menu_item,
                                           TerminalWindow *window);
static void reset_callback                (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void reset_and_clear_callback      (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void next_tab_callback             (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void previous_tab_callback         (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void move_left_tab_callback        (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void move_right_tab_callback       (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void detach_tab                    (TerminalScreen *screen,
                                           TerminalWindow *window);
static void detach_tab_callback           (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void change_tab_callback           (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void help_callback                 (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void about_callback                (GtkWidget      *menuitem,
                                           TerminalWindow *window);

static gboolean window_state_event_callback (GtkWidget            *widget, 
                                             GdkEventWindowState  *event,
                                             gpointer              user_data);

static void set_menuitem_text (GtkWidget  *mi,
                               const char *text,
                               gboolean    strip_mnemonic);

static void terminal_menu_opened_callback (GtkWidget      *menuitem,
                                           TerminalWindow *window);

static gboolean find_larger_zoom_factor  (double  current,
                                          double *found);
static gboolean find_smaller_zoom_factor (double  current,
                                          double *found);

static void terminal_window_show (GtkWidget *widget);

static gboolean confirm_close_window (TerminalWindow *window);

G_DEFINE_TYPE (TerminalWindow, terminal_window, GTK_TYPE_WINDOW)

static GtkWidget*
append_menuitem_common (GtkWidget  *menu,
                        GtkWidget  *menu_item,
                        const char *accel_path,
                        GCallback   callback,
                        gpointer    data)
{
  if (accel_path)
    gtk_menu_item_set_accel_path (GTK_MENU_ITEM (menu_item),
                                  accel_path);
  
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         menu_item);

  if (callback)
    g_signal_connect (G_OBJECT (menu_item),
                      "activate",
                      callback, data);

  return menu_item;
}

static GtkWidget*
append_menuitem (GtkWidget  *menu,
                 const char *text,
                 const char *accel_path,
                 GCallback   callback,
                 gpointer    data)
{
  return append_menuitem_common (menu,
                                 gtk_menu_item_new_with_mnemonic (text),
                                 accel_path,
                                 callback,
                                 data);
}

static GtkWidget*
append_stock_menuitem (GtkWidget  *menu,
                       const char *stock_id,
                       const char *accel_path,
                       GCallback   callback,
                       gpointer    data)
{
  return append_menuitem_common (menu,
                                 gtk_image_menu_item_new_from_stock (stock_id, NULL),
                                 accel_path,
                                 callback,
                                 data);
}

static GtkWidget *
append_check_menuitem (GtkWidget  *menu, 
                       const char *text,
                       const char *accel_path,
                       gboolean    active,
                       GCallback   callback,
                       gpointer    user_data)
{
  GtkWidget *menu_item;

  menu_item = gtk_check_menu_item_new_with_mnemonic (text);

  if (accel_path)
    gtk_menu_item_set_accel_path (GTK_MENU_ITEM (menu_item), accel_path);

  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), active);

  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  if (callback)
    g_signal_connect (G_OBJECT (menu_item), "toggled", callback, user_data);

  return menu_item;
}
                       
static void
fill_in_config_picker_submenu (TerminalWindow *window)
{
  GtkWidget *menu;
  GtkWidget *menu_item;
  GList *profiles;
  GList *tmp;
  GSList *group;
  GtkAccelGroup *accel_group;

  profiles = terminal_profile_get_list ();

  if (window->priv->active_term == NULL || g_list_length (profiles) < 2)
    {
      gtk_widget_set_sensitive (window->priv->choose_config_menuitem, FALSE);
      gtk_menu_item_remove_submenu (GTK_MENU_ITEM (window->priv->choose_config_menuitem));
      g_list_free (profiles);

      return;
    }
  
  gtk_widget_set_sensitive (window->priv->choose_config_menuitem, TRUE);

  accel_group = terminal_accels_get_group_for_widget (GTK_WIDGET (window));  
  
  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu), accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (window->priv->choose_config_menuitem),
                             menu);
  
  group = NULL;
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile;
      
      profile = tmp->data;
      
      /* Profiles can go away while the menu is up. */
      g_object_ref (G_OBJECT (profile));

      menu_item = gtk_radio_menu_item_new_with_label (group,
                                                      terminal_profile_get_visible_name (profile));
      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                             menu_item);
      
      if (profile == terminal_screen_get_profile (window->priv->active_term))
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                        TRUE);
      
      g_signal_connect (G_OBJECT (menu_item),
                        "toggled",
                        G_CALLBACK (change_configuration_callback),
                        window);
      
      g_object_set_data_full (G_OBJECT (menu_item),
                              "profile",
                              profile,
                              (GDestroyNotify) g_object_unref);
      
      tmp = tmp->next;
    }

  g_list_free (profiles);  
}

static char *
escape_underscores (const char *name)
{
  GString *escaped_name;

  g_assert (name != NULL);

  /* Who'd use more that 4 underscores in a profile name... */
  escaped_name = g_string_sized_new (strlen (name) + 4 + 1);

  while (*name)
    {
      if (*name == '_')
        g_string_append (escaped_name, "__");
      else
        g_string_append_c (escaped_name, *name);
      name++;
    }

  return g_string_free (escaped_name, FALSE);
}

static void
fill_in_new_term_submenu_real(GtkWidget *menuitem, 
                              TerminalWindow *window,
                              const char *accel, 
                              TerminalProfile *current_profile, 
                              GCallback callback) 
{
  GList *profiles;

  gtk_widget_set_sensitive (menuitem, TRUE);
  
  /* remove older signal handler and accelerator */
  g_signal_handlers_disconnect_by_func (G_OBJECT (menuitem), G_CALLBACK (callback), window);
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (menuitem), NULL);

  profiles = terminal_profile_get_list ();

  if (current_profile == NULL)
    {
      /* Well, this shouldn't happen: it'd mean there is no default profile */
      gtk_widget_set_sensitive (window->priv->new_window_menuitem, FALSE);
      gtk_menu_item_remove_submenu (GTK_MENU_ITEM (window->priv->new_window_menuitem));
    }
  else if (g_list_length (profiles) < 2)
    {
      if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem)))
        gtk_menu_item_remove_submenu (GTK_MENU_ITEM (menuitem));

      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (callback), window);
      g_object_ref (G_OBJECT (current_profile));
      g_object_set_data_full (G_OBJECT (menuitem), "profile", current_profile, (GDestroyNotify) g_object_unref);  
      gtk_menu_item_set_accel_path (GTK_MENU_ITEM (menuitem), accel);
    }
  else
    {
      GtkWidget *new_sub_menu;
      GtkWidget *menu_item;
      GtkAccelGroup *accel_group;
      GList *tmp;
      int i;

      accel_group = terminal_accels_get_group_for_widget (GTK_WIDGET (window));  

      /* New submenu */
      new_sub_menu = gtk_menu_new ();
      gtk_menu_set_accel_group (GTK_MENU (new_sub_menu), accel_group);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), new_sub_menu);

      /* Add default menu item */
      menu_item = gtk_menu_item_new_with_label (terminal_profile_get_visible_name (current_profile));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (new_sub_menu), menu_item);
      g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (callback), window);
      g_object_ref (G_OBJECT (current_profile));
      g_object_set_data_full (G_OBJECT (menu_item), "profile", current_profile, (GDestroyNotify) g_object_unref);  
      gtk_menu_item_set_accel_path (GTK_MENU_ITEM (menu_item), accel);
      
      /* separator */
      menu_item = gtk_separator_menu_item_new ();
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (new_sub_menu), menu_item);

      for (i = 1, tmp = profiles; tmp != NULL; i++, tmp = tmp->next)
        {
          TerminalProfile *profile;
          char *escaped_name, *str;
          
          profile = tmp->data;
          
          /* Profiles can go away while the menu is up. */
          g_object_ref (G_OBJECT (profile));

          escaped_name = escape_underscores (terminal_profile_get_visible_name (profile));

          if (i < 10)
            str = g_strdup_printf (_("_%d. %s"), i, escaped_name);
          else if (i < 36)
            str = g_strdup_printf (_("_%c. %s"), ('A' + i - 10), escaped_name);
          else
            str = g_strdup (escaped_name);

          /* item for new window */
          menu_item = gtk_menu_item_new_with_mnemonic (str);
          gtk_widget_show (menu_item);
          gtk_menu_shell_append (GTK_MENU_SHELL (new_sub_menu), menu_item);      
          g_signal_connect (G_OBJECT (menu_item), "activate", G_CALLBACK (callback), window);      
          g_object_set_data_full (G_OBJECT (menu_item), "profile", profile, (GDestroyNotify) g_object_unref);

          g_free (str);
          g_free (escaped_name);
          
        }

    }
  g_list_free (profiles);
}

static void
fill_in_new_term_submenus (TerminalWindow *window)
{
  TerminalProfile *current_profile;

  current_profile = NULL;

  if (window->priv->active_term != NULL)
    current_profile = terminal_screen_get_profile (window->priv->active_term);

  /* if current_profile is still NULL, this will change it to a default */
  current_profile = terminal_profile_get_for_new_term (current_profile);
  
  fill_in_new_term_submenu_real (window->priv->new_window_menuitem, 
                                 window,
                                 ACCEL_PATH_NEW_WINDOW,
                                 current_profile,
                                 G_CALLBACK (new_window_callback));
  fill_in_new_term_submenu_real (window->priv->new_tab_menuitem, 
                                 window,
                                 ACCEL_PATH_NEW_TAB,
                                 current_profile,
                                 G_CALLBACK (new_tab_callback));
}

static void
fill_in_encoding_menu (TerminalWindow *window)
{
  GtkWidget *menu;
  GtkWidget *menu_item;
  GSList *group;
  GSList *encodings;
  GSList *tmp;
  const char *charset;
  GtkWidget *w;

  if (!terminal_widget_supports_dynamic_encoding ())
    return;
  
  if (window->priv->active_term == NULL)
    {
      gtk_widget_set_sensitive (window->priv->encoding_menuitem, FALSE);
      gtk_menu_item_remove_submenu (GTK_MENU_ITEM (window->priv->encoding_menuitem));

      return;
    }
  
  gtk_widget_set_sensitive (window->priv->encoding_menuitem, TRUE);

  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (window->priv->encoding_menuitem),
                             menu);

  w = terminal_screen_get_widget (window->priv->active_term);
  charset = terminal_widget_get_encoding (w);
  
  group = NULL;
  encodings = terminal_get_active_encodings ();
  tmp = encodings;
  while (tmp != NULL)
    {
      TerminalEncoding *e;
      char *name;
      
      e = tmp->data;

      name = g_strdup_printf ("%s (%s)", e->name, e->charset);
      
      menu_item = gtk_radio_menu_item_new_with_label (group, name);

      g_free (name);

      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                             menu_item);
      
      if (strcmp (e->charset, charset) == 0)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                        TRUE);
      
      g_signal_connect (G_OBJECT (menu_item),
                        "toggled",
                        G_CALLBACK (change_encoding_callback),
                        window);

      g_object_set_data_full (G_OBJECT (menu_item),
                              "encoding",
                              g_strdup (e->charset),
                              (GDestroyNotify) g_free);
      
      tmp = tmp->next;
    }

  g_slist_foreach (encodings,
                   (GFunc) terminal_encoding_free,
                   NULL);
  g_slist_free (encodings);

  menu_item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
  gtk_widget_show (menu_item);
  
  append_menuitem (menu, _("_Add or Remove..."), NULL,
                   G_CALLBACK (add_encoding_callback),
                   window);
}

static void
terminal_menu_opened_callback (GtkWidget      *menuitem,
                               TerminalWindow *window)
{
  fill_in_encoding_menu (window);
}

static void
update_zoom_items (TerminalWindow *window)
{
  TerminalScreen *screen;
  double ignored;
  double current_zoom;
  
  screen = window->priv->active_term;
  if (screen == NULL)
    return;

  current_zoom = terminal_screen_get_font_scale (screen);

  gtk_widget_set_sensitive (window->priv->zoom_out_menuitem,
                            find_smaller_zoom_factor (current_zoom, &ignored));
  gtk_widget_set_sensitive (window->priv->zoom_in_menuitem,
                            find_larger_zoom_factor (current_zoom, &ignored));
}

static void
update_edit_menu (GtkClipboard *clipboard,
                  const  gchar *text,
                  gpointer     *user_data)
{
  TerminalWindow *window;

  window = (TerminalWindow *) user_data;

  gtk_widget_set_sensitive (window->priv->paste_menuitem, text != NULL);
}

static void
edit_menu_activate_callback (GtkMenuItem *menuitem,
                             gpointer     user_data)
{
  TerminalWindow *window;

  window = (TerminalWindow *) user_data;

  gtk_clipboard_request_text (window->priv->clipboard, (GtkClipboardTextReceivedFunc) update_edit_menu, window);
}

static void
initialize_alpha_mode (TerminalWindow *window)
{
  GdkScreen *screen;
  GdkColormap *colormap;

  screen = gtk_widget_get_screen (GTK_WIDGET (window));
  colormap = gdk_screen_get_rgba_colormap (screen);
  if (colormap != NULL && gdk_screen_is_composited (screen))
    {
      /* Set RGBA colormap if possible so VTE can use real alpha
       * channels for transparency. */

      gtk_widget_set_colormap(GTK_WIDGET (window), colormap);
      window->priv->have_argb_visual = TRUE;
    }
  else
    {
      window->priv->have_argb_visual = FALSE;
    }
}

gboolean
terminal_window_uses_argb_visual (TerminalWindow *window)
{
  return window->priv->have_argb_visual;
}

static void
update_tab_visibility (TerminalWindow *window,
                        int             change)
{
  gboolean show_tabs;
  guint num;

  num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));

  show_tabs = (num + change) > 1;
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook), show_tabs);
}

static GtkNotebook *
handle_tab_droped_on_desktop (GtkNotebook *source_notebook,
                              GtkWidget   *child,
                              gint         x,
                              gint         y,
                              gpointer     data)
{
  TerminalScreen *screen;
  TerminalWindow *source_window;
  TerminalWindow *dest_window;
  double zoom;

  screen = TERMINAL_SCREEN (child);
  source_window = TERMINAL_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (source_notebook)));

  g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), NULL);

  zoom = terminal_screen_get_font_scale (screen);

  dest_window = terminal_app_new_window (terminal_app_get (), NULL, NULL, NULL, -1);
  dest_window->priv->present_on_insert = TRUE;

  update_tab_visibility (source_window, -1);
  update_tab_visibility (dest_window, +1);

  return GTK_NOTEBOOK (dest_window->priv->notebook);
}

static void
terminal_window_realized_callback (GtkWidget *window,
				   gpointer   user_data)
{
  gdk_window_set_group (window->window, window->window);
  g_signal_handlers_disconnect_by_func (window, terminal_window_realized_callback, NULL);
}

static void
terminal_window_init (TerminalWindow *window)
{
  GtkWidget *mi;
  GtkWidget *menu;
  GtkAccelGroup *accel_group;
  GError *error;
  gboolean use_mnemonics;

  window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, TERMINAL_TYPE_WINDOW, TerminalWindowPrivate);

  g_signal_connect (G_OBJECT (window), "delete_event", 
                    G_CALLBACK(terminal_window_delete_event),
                    NULL);
  g_signal_connect (G_OBJECT (window), "realize",
                    G_CALLBACK (terminal_window_realized_callback),
                    NULL);

  gtk_window_set_title (GTK_WINDOW (window), _("Terminal"));

  gtk_notebook_set_window_creation_hook (handle_tab_droped_on_desktop, NULL, NULL);
  
  window->priv->terms = 0;
  window->priv->active_term = NULL;
  window->priv->menubar = gtk_menu_bar_new ();
  window->priv->menubar_visible = FALSE;
  
  window->priv->main_vbox = gtk_vbox_new (FALSE, 0);
  window->priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (window->priv->notebook), TRUE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (window->priv->notebook), FALSE);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook), FALSE);
  gtk_notebook_set_group_id (GTK_NOTEBOOK (window->priv->notebook), 1);
  
  window->priv->old_char_width = -1;
  window->priv->old_char_height = -1;
  window->priv->old_geometry_widget = NULL;
  
  window->priv->conf = gconf_client_get_default ();

  error = NULL;
  window->priv->notify_id =
    gconf_client_notify_add (window->priv->conf,
                             CONF_GLOBAL_PREFIX,
                             config_change_notify,
                             window,
                             NULL, &error);
  
  if (error)
    {
      g_printerr (_("There was an error subscribing to notification of terminal window configuration changes. (%s)\n"),
                  error->message);
      g_error_free (error);
    }

  error = NULL;
  use_mnemonics = gconf_client_get_bool (window->priv->conf,
                                         CONF_GLOBAL_PREFIX"/use_mnemonics",
                                         &error);
  if (error)
    {
      g_printerr (_("There was an error loading config value for whether to use mnemonics. (%s)\n"),
                  error->message);
      g_error_free (error);
      use_mnemonics = TRUE;
    }
  window->priv->use_mnemonics = use_mnemonics;
  window->priv->using_mnemonics = !use_mnemonics; /* force a label update in reset_menubar_labels */

  initialize_alpha_mode (window);

  /* force gtk to construct its GtkClipboard; otherwise our UI is very slow the first time we need it */
  window->priv->clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (window)), GDK_NONE);

  g_signal_connect (window->priv->menubar,
		    "can_activate_accel",
		    G_CALLBACK (gtk_true),
		    NULL);
  
  accel_group = terminal_accels_get_group_for_widget (GTK_WIDGET (window));
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (window->priv->notebook),
                               TRUE);                                      
  
  g_signal_connect_after (G_OBJECT (window->priv->notebook),
                          "switch-page",
                          G_CALLBACK (notebook_page_selected_callback),
                          window);

  g_signal_connect_after (G_OBJECT (window->priv->notebook),
                          "page-added",
                          G_CALLBACK (notebook_page_added_callback),
                          window);

  g_signal_connect_after (G_OBJECT (window->priv->notebook),
                          "page-removed",
                          G_CALLBACK (notebook_page_removed_callback),
                          window);
  
  g_signal_connect_after (G_OBJECT (window->priv->notebook),
                          "page-reordered",
                          G_CALLBACK (notebook_page_reordered_callback),
                          window);

  gtk_container_add (GTK_CONTAINER (window),
                     window->priv->main_vbox);

  gtk_box_pack_start (GTK_BOX (window->priv->main_vbox),
		      window->priv->menubar,
		      FALSE, FALSE, 0);

  gtk_box_pack_end (GTK_BOX (window->priv->main_vbox),
                    window->priv->notebook,
                    TRUE, TRUE, 0);  
  
  mi = append_menuitem (window->priv->menubar,
                        "", NULL,
                        NULL, NULL);
  window->priv->file_menuitem = mi;
  
  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu),
                            accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  window->priv->new_window_menuitem =
    append_menuitem (menu, _("Open _Terminal"), NULL, NULL, NULL);

  window->priv->new_tab_menuitem =
    append_menuitem (menu, _("Open Ta_b"), NULL, NULL, NULL);
  
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  /* This is fairly bogus to have here but I don't know
   * where else to put it really
   */
  append_menuitem (menu, _("New _Profile..."), ACCEL_PATH_NEW_PROFILE,
		   G_CALLBACK (new_configuration_callback), window);
  
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  window->priv->close_tab_menuitem = 
    append_menuitem (menu, _("C_lose Tab"), ACCEL_PATH_CLOSE_TAB,
                     G_CALLBACK (close_tab_callback),
                     window);
  
  append_menuitem (menu, _("_Close Window"), ACCEL_PATH_CLOSE_WINDOW,
                   G_CALLBACK (close_window_callback),
                   window);
  
  mi = append_menuitem (window->priv->menubar,
                        "", NULL,
                        NULL, NULL);
  window->priv->edit_menuitem = mi;

  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu),
                            accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  window->priv->copy_menuitem =
    append_stock_menuitem (menu,
                           GTK_STOCK_COPY, ACCEL_PATH_COPY,
                           G_CALLBACK (copy_callback),
                           window);

  window->priv->paste_menuitem =
    append_stock_menuitem (menu,
                           GTK_STOCK_PASTE, ACCEL_PATH_PASTE,
                           G_CALLBACK (paste_callback),
                           window);
  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (edit_menu_activate_callback), window);

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  
  append_menuitem (menu, _("P_rofiles..."), NULL,
                   G_CALLBACK (manage_configurations_callback), window);

  append_menuitem (menu, _("_Keyboard Shortcuts..."), NULL,
                   G_CALLBACK (edit_keybindings_callback), window);

  window->priv->edit_config_menuitem =
    append_menuitem (menu, _("C_urrent Profile..."), NULL,
                     G_CALLBACK (edit_configuration_callback), window);
  
  mi = append_menuitem (window->priv->menubar,
                        "", NULL,
                        NULL, NULL);
  window->priv->view_menuitem = mi;

  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu),
                            accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
  
  /* This is a check menu item so that the togglability is evident, but when it is not checked,
   * it won't be seen, of course, because the menubar will be then hidden. */
  mi = append_check_menuitem (menu, _("Show Menu_bar"), ACCEL_PATH_TOGGLE_MENUBAR, FALSE,
                              G_CALLBACK (toggle_menubar_callback), window);
  window->priv->show_menubar_menuitem = mi;

  mi = append_check_menuitem (menu, _("_Full Screen"), ACCEL_PATH_FULL_SCREEN, FALSE,
                        G_CALLBACK (fullscreen_callback), window);
  if (!gdk_net_wm_supports (gdk_atom_intern ("_NET_WM_STATE_FULLSCREEN", FALSE)))
    gtk_widget_set_sensitive (mi, FALSE);
  else
    g_signal_connect (G_OBJECT (window), "window-state-event", G_CALLBACK (window_state_event_callback), mi);

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  mi = append_stock_menuitem (menu, GTK_STOCK_ZOOM_IN, ACCEL_PATH_ZOOM_IN,
                              G_CALLBACK (zoom_in_callback), window);
  window->priv->zoom_in_menuitem = mi;

  mi = append_stock_menuitem (menu, GTK_STOCK_ZOOM_OUT, ACCEL_PATH_ZOOM_OUT,
                              G_CALLBACK (zoom_out_callback), window);
  window->priv->zoom_out_menuitem = mi;

  mi = append_stock_menuitem (menu, GTK_STOCK_ZOOM_100, ACCEL_PATH_ZOOM_NORMAL,
                              G_CALLBACK (zoom_normal_callback), window);
  window->priv->zoom_normal_menuitem = mi;

  update_zoom_items (window);
  
  mi = append_menuitem (window->priv->menubar,
                        "", NULL,
                        NULL, NULL);
  window->priv->terminal_menuitem = mi;  

  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (terminal_menu_opened_callback),
                    window);
  
  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu),
                            accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  /* submenu of this dynamically generated up above */
  window->priv->choose_config_menuitem =
    append_menuitem (menu, _("Change _Profile"), NULL,
                     NULL, NULL);
  
  append_menuitem (menu, _("_Set Title..."), ACCEL_PATH_SET_TERMINAL_TITLE,
                   G_CALLBACK (set_title_callback), window);

  if (terminal_widget_supports_dynamic_encoding ())
    {
      window->priv->encoding_menuitem =
        append_menuitem (menu,
                         _("Set _Character Encoding"), NULL, NULL, NULL);
    }
  else
    {
      window->priv->encoding_menuitem = NULL;
    }

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  append_menuitem (menu, _("_Reset"), ACCEL_PATH_RESET,
                   G_CALLBACK (reset_callback), window);

  append_menuitem (menu, _("Reset and C_lear"), ACCEL_PATH_RESET_AND_CLEAR,
                   G_CALLBACK (reset_and_clear_callback), window);

  mi = append_menuitem (window->priv->menubar,
                        "", NULL,
                        NULL, NULL);
  window->priv->go_menuitem = mi;
  
  menu = gtk_menu_new ();
  window->priv->go_menu = menu;
  gtk_menu_set_accel_group (GTK_MENU (menu),
                            accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  mi = append_menuitem (menu, _("_Previous Tab"), ACCEL_PATH_PREV_TAB,
                        G_CALLBACK (previous_tab_callback), window);
  window->priv->previous_tab_menuitem = mi;
  
  mi = append_menuitem (menu, _("_Next Tab"), ACCEL_PATH_NEXT_TAB,
                        G_CALLBACK (next_tab_callback), window);
  window->priv->next_tab_menuitem = mi;

  /* Capture the key presses */
  g_signal_connect (G_OBJECT(window), "key-press-event",
		    G_CALLBACK (key_press_callback), window);

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  mi = append_menuitem (menu, _("Move Tab to the _Left"), ACCEL_PATH_MOVE_TAB_LEFT,
                        G_CALLBACK (move_left_tab_callback), window);
  window->priv->move_left_tab_menuitem = mi;
  
  mi = append_menuitem (menu, _("Move Tab to the _Right"), ACCEL_PATH_MOVE_TAB_RIGHT,
                        G_CALLBACK (move_right_tab_callback), window);
  window->priv->move_right_tab_menuitem = mi;

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  mi = append_menuitem (menu, _("_Detach Tab"), ACCEL_PATH_DETACH_TAB,
                        G_CALLBACK (detach_tab_callback), window);
  window->priv->detach_tab_menuitem = mi;

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  
  mi = append_menuitem (window->priv->menubar,
                        "", NULL,
                        NULL, NULL);
  window->priv->help_menuitem = mi;
  
  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu),
                            accel_group);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  mi = append_stock_menuitem (menu, GTK_STOCK_HELP, NULL,
			      G_CALLBACK (help_callback), window);
  set_menuitem_text (mi, _("_Contents"), FALSE);

  gtk_accel_map_add_entry (ACCEL_PATH_HELP, GDK_F1, 0);
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (mi),
                                ACCEL_PATH_HELP);

  
  mi = append_stock_menuitem (menu, GTK_STOCK_ABOUT, NULL,
                              G_CALLBACK (about_callback), window);
  set_menuitem_text (mi, _("_About"), FALSE);
  
  terminal_window_reread_profile_list (window);
  
  terminal_window_set_menubar_visible (window, TRUE);
  window->priv->use_default_menubar_visibility = TRUE;  

  reset_menubar_labels (window);
  reset_tab_menuitems (window);

  gtk_widget_show_all (window->priv->main_vbox);
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
  object_class->finalize = terminal_window_finalize;
  object_class->dispose = terminal_window_dispose;
  widget_class->show = terminal_window_show;

  g_type_class_add_private (object_class, sizeof (TerminalWindowPrivate));

        
  gtk_rc_parse_string ("style \"gnome-terminal-tab-close-button-style\"\n"
                       "{\n"
                          "GtkWidget::focus-padding = 0\n"
                          "GtkWidget::focus-line-width = 0\n"
                          "xthickness = 0\n"
                          "ythickness = 0\n"
                       "}\n"
                       "widget \"*.gnome-terminal-tab-close-button\" style \"gnome-terminal-tab-close-button-style\"");


}

static void
terminal_window_dispose (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;

  if (priv->notify_id != 0)
    {
      gconf_client_notify_remove (priv->conf, priv->notify_id);
      priv->notify_id = 0;
    }

  priv->disposed = TRUE;

  G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}
   
static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window;

  window = TERMINAL_WINDOW (object);

  if (window->priv->conf)
    {
      g_object_unref (G_OBJECT (window->priv->conf));
    }
  
  if (window->priv->icon)
    {
      g_object_unref (G_OBJECT (window->priv->icon));
    }

  if (window->priv->tab_menuitems)
    {
      g_list_free (window->priv->tab_menuitems);
    }

  g_free (window->priv->startup_id);

  G_OBJECT_CLASS (terminal_window_parent_class)->finalize (object);
}

static gboolean
terminal_window_delete_event (GtkWidget *widget,
                              GdkEvent *event,
                              gpointer data)
{
   return !confirm_close_window (TERMINAL_WINDOW (widget));
}

static void
sn_error_trap_push (SnDisplay *display,
                    Display   *xdisplay)
{
  gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay *display,
                   Display   *xdisplay)
{
  gdk_error_trap_pop ();
}

static void
terminal_window_show (GtkWidget *widget)
{
  TerminalWindow *window;
  SnDisplay *sn_display;
  SnLauncheeContext *context;
  GdkScreen *screen;
  GdkDisplay *display;
  
  window = TERMINAL_WINDOW (widget);

  if (!GTK_WIDGET_REALIZED (widget))
    gtk_widget_realize (widget);
  
  context = NULL;
  sn_display = NULL;
  if (window->priv->startup_id != NULL)
    {
      /* Set up window for launch notification */
      /* FIXME In principle all transient children of this
       * window should get the same startup_id
       */
      
      screen = gtk_window_get_screen (GTK_WINDOW (window));
      display = gdk_screen_get_display (screen);
      
      sn_display = sn_display_new (gdk_x11_display_get_xdisplay (display),
                                   sn_error_trap_push,
                                   sn_error_trap_pop);
      
      context = sn_launchee_context_new (sn_display,
                                         gdk_screen_get_number (screen),
                                         window->priv->startup_id);

      /* Handle the setup for the window if the startup_id is valid; I
       * don't think it can hurt to do this even if it was invalid,
       * but why do the extra work...
       */
      if (strncmp (sn_launchee_context_get_startup_id (context), "_TIME", 5) != 0)
        sn_launchee_context_setup_window (context,
                                          GDK_WINDOW_XWINDOW (widget->window));

      /* Now, set the _NET_WM_USER_TIME for the new window to the timestamp
       * that caused the window to be launched.
       */
      if (sn_launchee_context_get_id_has_timestamp (context))
        {
          gulong timestamp;

          timestamp = sn_launchee_context_get_timestamp (context);
          gdk_x11_window_set_user_time (widget->window, timestamp);
        }
    }
  
  GTK_WIDGET_CLASS (terminal_window_parent_class)->show (widget);

  if (context != NULL)
    {
      sn_launchee_context_complete (context);
      sn_launchee_context_unref (context);
      sn_display_unref (sn_display);
    }
}

TerminalWindow*
terminal_window_new (void)
{
  return g_object_new (TERMINAL_TYPE_WINDOW, NULL);
}

static void
update_notebook (TerminalWindow *window)
{
  gboolean single;

  single = window->priv->terms == 1;
    
  gtk_notebook_set_show_border (GTK_NOTEBOOK (window->priv->notebook), !single);
}

static void
profile_set_callback (TerminalScreen *screen,
                      TerminalWindow *window)
{
  /* Redo the pick-a-profile menu */
  fill_in_config_picker_submenu (window);
  /* and the open-new-profile menu */
  fill_in_new_term_submenus (window);
}

static void
title_changed_callback (TerminalScreen *screen,
                        TerminalWindow *window)
{
  GtkWidget *menu_item;
  const char *title;
  
  title = terminal_screen_get_title (screen);

  menu_item = screen_get_menuitem (screen);
  if (menu_item)
    {
      GtkWidget *label;

      label = gtk_bin_get_child (GTK_BIN (menu_item));

      gtk_label_set_use_underline (GTK_LABEL (label), FALSE);
      gtk_label_set_ellipsize (GTK_LABEL (label), TAB_MENU_ELLIPSIZE);
      gtk_label_set_max_width_chars (GTK_LABEL (label), TAB_MENU_WIDTH_CHARS); 
      gtk_label_set_text (GTK_LABEL (label), title);
    }

  if (screen == window->priv->active_term) 
    {
      gtk_window_set_title (GTK_WINDOW (window), title);

      if (terminal_screen_get_icon_title_set (screen))
        {
          title = terminal_screen_get_icon_title (screen);
        }
      gdk_window_set_icon_name (GTK_WIDGET (window)->window, title);
    }
}

static void
icon_title_changed_callback (TerminalScreen *screen,
                             TerminalWindow *window)
{
  if (screen == window->priv->active_term)
    gdk_window_set_icon_name (GTK_WIDGET (window)->window, terminal_screen_get_icon_title (screen));
}


static void
update_copy_sensitivity (TerminalWindow *window)
{
  gboolean can_copy = FALSE;

  if (window->priv->active_term)
    can_copy = terminal_screen_get_text_selected (window->priv->active_term);
  else
    can_copy = FALSE;

  gtk_widget_set_sensitive (window->priv->copy_menuitem, can_copy);
}

static void
update_tab_sensitivity (TerminalWindow *window)
{
  GtkWidget *notebook;
  int num_pages, page_num;
  gboolean on_last_page;

  notebook = window->priv->notebook;

  if (notebook == NULL)
    {
      return;
    }

  num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  
  gtk_widget_set_sensitive (window->priv->previous_tab_menuitem,
                            page_num > 0);

  on_last_page = (page_num >= num_pages - 1);

  /* If there's only one tab, Close Tab is insensitive */
  if (page_num == 0 && on_last_page)
    {
      gtk_widget_set_sensitive (window->priv->close_tab_menuitem, FALSE);
      gtk_widget_set_sensitive (window->priv->detach_tab_menuitem, FALSE);

      gtk_widget_set_sensitive (window->priv->move_left_tab_menuitem, FALSE);
      gtk_widget_set_sensitive (window->priv->move_right_tab_menuitem, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (window->priv->close_tab_menuitem, TRUE);
      gtk_widget_set_sensitive (window->priv->detach_tab_menuitem, TRUE);

      gtk_widget_set_sensitive (window->priv->move_left_tab_menuitem, TRUE);
      gtk_widget_set_sensitive (window->priv->move_right_tab_menuitem, TRUE);
    }

  gtk_widget_set_sensitive (window->priv->next_tab_menuitem,
                            !on_last_page);
}

static void
selection_changed_callback (TerminalScreen *screen,
                            TerminalWindow *window)
{
  update_copy_sensitivity (window);
}

static void
close_button_clicked_cb (GtkWidget *widget, GtkWidget *screen)
{
  GtkWidget *notebook;
  guint page_num;

  notebook = gtk_widget_get_parent (GTK_WIDGET (screen));
  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), screen);
  gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page_num);
}

static void
sync_tab_label (TerminalScreen *screen,
                GtkWidget *label)
{
  GtkWidget *hbox;
  const char *title;

  title = terminal_screen_get_title (screen);

  hbox = gtk_widget_get_parent (label);

  gtk_label_set_text (GTK_LABEL (label), title);
  gtk_widget_set_tooltip_text (hbox, title);
}

static void
tab_label_style_set_cb (GtkWidget *hbox,
                        GtkStyle *previous_style,
                        GtkWidget *button)
{
        int h, w;

        gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
                                           GTK_ICON_SIZE_MENU, &w, &h);
        gtk_widget_set_size_request (button, w + 2, h + 2);
}

static GtkWidget *
contruct_tab_label (TerminalWindow *window, TerminalScreen *screen)
{
  GtkWidget *hbox, *label, *close_button, *image;

  hbox = gtk_hbox_new (FALSE, 4);

  label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  close_button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_widget_set_name (close_button, "gnome-terminal-tab-close-button");
  gtk_widget_set_tooltip_text (close_button, _("Close tab"));

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (close_button), image);
  gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (close_button), "clicked",
		    G_CALLBACK (close_button_clicked_cb),
		    screen);

  sync_tab_label (screen, label);
  g_signal_connect (G_OBJECT (screen),
                    "title-changed",
                    G_CALLBACK (sync_tab_label),
                    label);

  g_signal_connect (G_OBJECT (hbox), "style-set",
                    G_CALLBACK (tab_label_style_set_cb), close_button);

  gtk_widget_show_all (hbox);

  return hbox;
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            gint            position)
{
  TerminalWindow *old;
  GtkWidget *tab_label;
 
  old = terminal_screen_get_window (screen);

  if (old == window)
    return;  

  g_object_ref (G_OBJECT (screen)); /* make our own new refcount */

  if (old)
    terminal_window_remove_screen (old, screen);

  terminal_screen_set_window (screen, window);

  gtk_widget_show_all (GTK_WIDGET (screen));

  update_tab_visibility (window, +1);

  tab_label = contruct_tab_label (window, screen);

  gtk_notebook_insert_page (GTK_NOTEBOOK (window->priv->notebook),
                            GTK_WIDGET (screen),
                            tab_label,
                            position);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (window->priv->notebook),
                                      GTK_WIDGET (screen),
                                      TRUE, TRUE, GTK_PACK_START);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->priv->notebook),
                                    GTK_WIDGET (screen),
                                    TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (window->priv->notebook),
                                   GTK_WIDGET (screen),
                                   TRUE);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  guint num_page;

  g_return_if_fail (terminal_screen_get_window (screen) == window);

  update_tab_visibility (window, -1);

  num_page = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook), GTK_WIDGET (screen));
  gtk_notebook_remove_page (GTK_NOTEBOOK (window->priv->notebook), num_page);
}

GList*
terminal_window_list_screens (TerminalWindow *window)
{
  /* We are trusting that GtkNotebook will return pages in order */
  return gtk_container_get_children (GTK_CONTAINER (window->priv->notebook));
}

int    
terminal_window_get_screen_count (TerminalWindow *window)
{
  return window->priv->terms;
}

void
terminal_window_set_menubar_visible (TerminalWindow *window,
                                     gboolean        setting)
{
  /* it's been set now, so don't override when adding a screen.
   * this side effect must happen before we short-circuit below.
   */
  window->priv->use_default_menubar_visibility = FALSE;
  
  if (setting == window->priv->menubar_visible)
    return;

  window->priv->menubar_visible = setting;
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (window->priv->show_menubar_menuitem), setting);
  
  if (window->priv->menubar_visible)
    {      
      gtk_widget_show (window->priv->menubar);
    }
  else
    {
      gtk_widget_hide (window->priv->menubar);
    }
  reset_menubar_labels (window);

  if (window->priv->active_term)
    {
#ifdef DEBUG_GEOMETRY
      g_fprintf (stderr,"setting size after toggling menubar visibility\n");
#endif
      terminal_window_set_size (window, window->priv->active_term, TRUE);
    }
}

gboolean
terminal_window_get_menubar_visible (TerminalWindow *window)
{
  return window->priv->menubar_visible;
}

GtkWidget *
terminal_window_get_notebook (TerminalWindow *window)
{
	g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

	return GTK_WIDGET (window->priv->notebook);
}

void
terminal_window_set_size (TerminalWindow *window,
                          TerminalScreen *screen,
                          gboolean        even_if_mapped)
{
  terminal_window_set_size_force_grid (window, screen, even_if_mapped, -1, -1);
}

void
terminal_window_set_size_force_grid (TerminalWindow *window,
                                     TerminalScreen *screen,
                                     gboolean        even_if_mapped,
                                     int             force_grid_width,
                                     int             force_grid_height)
{
  /* Owen's hack from gnome-terminal */
  GtkWidget *widget;
  GtkWidget *app;
  GtkRequisition toplevel_request;
  GtkRequisition widget_request;
  int w, h;
  int char_width;
  int char_height;
  int grid_width;
  int grid_height;
  int xpad;
  int ypad;

  /* be sure our geometry is up-to-date */
  terminal_window_update_geometry (window);
  widget = terminal_screen_get_widget (screen);
  
  app = gtk_widget_get_toplevel (widget);
  g_assert (app != NULL);

  gtk_widget_size_request (app, &toplevel_request);
  gtk_widget_size_request (widget, &widget_request);

#ifdef DEBUG_GEOMETRY
  g_fprintf (stderr,"set size: toplevel %dx%d widget %dx%d\n",
           toplevel_request.width, toplevel_request.height,
           widget_request.width, widget_request.height);
#endif
  
  w = toplevel_request.width - widget_request.width;
  h = toplevel_request.height - widget_request.height;

  terminal_widget_get_cell_size (widget, &char_width, &char_height);
  terminal_widget_get_size (widget, &grid_width, &grid_height);

  if (force_grid_width >= 0)
    grid_width = force_grid_width;
  if (force_grid_height >= 0)
    grid_height = force_grid_height;
  
  terminal_widget_get_padding (widget, &xpad, &ypad);
  
  w += xpad + char_width * grid_width;
  h += ypad + char_height * grid_height;

#ifdef DEBUG_GEOMETRY
  g_fprintf (stderr,"set size: grid %dx%d force %dx%d setting %dx%d pixels\n",
           grid_width, grid_height, force_grid_width, force_grid_height, w, h);
#endif

  if (even_if_mapped && GTK_WIDGET_MAPPED (app)) {
    gtk_window_resize (GTK_WINDOW (app), w, h);
  }
  else {
    gtk_window_set_default_size (GTK_WINDOW (app), w, h);
  }
}

void
terminal_window_set_active (TerminalWindow *window,
                            TerminalScreen *screen)
{
  GtkWidget *widget;
  TerminalProfile *profile;
  guint page_num;
  
  if (window->priv->active_term == screen)
    return;
  
  /* Workaround to remove gtknotebook's feature of computing its size based on
   * all pages. When the widget is hidden, its size will not be taken into
   * account.
   */
  if (window->priv->active_term)
  {
    GtkWidget *old_widget;
    old_widget = terminal_screen_get_widget (window->priv->active_term);
    gtk_widget_hide (old_widget);
  }
  
  widget = terminal_screen_get_widget (screen);
  
  /* Make sure that the widget is no longer hidden due to the workaround */
  gtk_widget_show (widget);

  profile = terminal_screen_get_profile (screen);

  if (!GTK_WIDGET_REALIZED (widget))
    gtk_widget_realize (widget); /* we need this for the char width */

  window->priv->active_term = screen;

  terminal_window_update_geometry (window);
  terminal_window_update_icon (window);
  
  /* Override menubar setting if it wasn't restored from session */
  if (window->priv->use_default_menubar_visibility)
    {
      gboolean setting =
        terminal_profile_get_default_show_menubar (terminal_screen_get_profile (screen));

      terminal_window_set_menubar_visible (window, setting);
    }

  gdk_window_set_icon_name (GTK_WIDGET (window)->window, terminal_screen_get_icon_title (screen));
  gtk_window_set_title (GTK_WINDOW (window), terminal_screen_get_title (screen));

  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook), GTK_WIDGET (screen));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), page_num);

  /* set size of window to current grid size */
#ifdef DEBUG_GEOMETRY
  g_fprintf (stderr,"setting size after flipping notebook pages\n");
#endif
  terminal_window_set_size (window, screen, TRUE);
  
  update_copy_sensitivity (window);
  
  fill_in_config_picker_submenu (window);
  fill_in_new_term_submenus (window);
  fill_in_encoding_menu (window);
  update_zoom_items (window);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{

  return window->priv->active_term;
}

static void
screen_set_menuitem  (TerminalScreen *screen,
                      GtkWidget      *menuitem)
{
  g_object_set_data (G_OBJECT (screen),
                     "menuitem",
                     menuitem);
}

static GtkWidget*
screen_get_menuitem (TerminalScreen *screen)
{
  return g_object_get_data (G_OBJECT (screen), "menuitem");
}

static void
notebook_page_selected_callback (GtkWidget       *notebook,
                                 GtkNotebookPage *useless_crap,
                                 guint            page_num,
                                 TerminalWindow  *window)
{
  GtkWidget* page_widget;
  TerminalScreen *screen;
  GtkWidget *menu_item;
  int old_grid_width, old_grid_height;
  GtkWidget *old_widget, *new_widget;

  if (window->priv->active_term == NULL || window->priv->disposed)
    return;

  old_widget = terminal_screen_get_widget (window->priv->active_term);
  terminal_widget_get_size (old_widget, &old_grid_width, &old_grid_height);
  
  page_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
                                           page_num);

  g_assert (page_widget);

  screen = TERMINAL_SCREEN (page_widget);

  g_assert (screen);
  
  /* This is so that we maintain the same grid */
  new_widget = terminal_screen_get_widget (screen);
  terminal_widget_set_size (new_widget, old_grid_width, old_grid_height);

  terminal_window_set_active (window, screen);

  update_tab_sensitivity (window);
  
  menu_item = screen_get_menuitem (screen);
  if (menu_item &&
      screen == window->priv->active_term)
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                    TRUE);
}

static void
notebook_page_added_callback (GtkWidget       *notebook,
                              TerminalScreen  *screen,
                              guint            page_num,
                              TerminalWindow  *window)
{
  GtkWidget *term;

  terminal_screen_set_window (screen, window);
  window->priv->terms++;

  g_signal_connect (G_OBJECT (screen),
                    "profile-set",
                    G_CALLBACK (profile_set_callback),
                    window);

  g_signal_connect (G_OBJECT (screen),
                    "title-changed",
                    G_CALLBACK (title_changed_callback),
                    window);

  g_signal_connect (G_OBJECT (screen),
                    "icon-title-changed",
                    G_CALLBACK (icon_title_changed_callback),
                    window);

  g_signal_connect (G_OBJECT (screen),
                    "selection-changed",
                    G_CALLBACK (selection_changed_callback),
                    window);

  terminal_screen_update_scrollbar (screen);

  update_notebook (window);

  reset_tab_menuitems (window);
  update_tab_sensitivity (window);
  update_tab_visibility (window, 0);

  term = terminal_screen_get_widget (screen);

  /* ZvtTerm is a broken POS and requires this realize to get
   * the size request right.
   */
  gtk_widget_realize (GTK_WIDGET (term));

  /* If we have an active screen, match its size and zoom */
  if (window->priv->active_term)
    {
      GtkWidget *widget;
      int current_width, current_height;
      double scale;

      widget = terminal_screen_get_widget (window->priv->active_term);
      terminal_widget_get_size (widget, &current_width, &current_height);
      terminal_widget_set_size (term, current_width, current_height);

      scale = terminal_screen_get_font_scale (window->priv->active_term);
      terminal_screen_set_font_scale (screen, scale);
    }
  
  /* Make the first-added screen the active one */
  if (window->priv->active_term == NULL)
    terminal_window_set_active (window, screen);

  if (window->priv->present_on_insert)
    {
      gtk_widget_show (GTK_WIDGET (window));
      window->priv->present_on_insert = FALSE;
    }
}

static void
notebook_page_removed_callback (GtkWidget       *notebook,
                                TerminalScreen  *screen,
                                guint            page_num,
                                TerminalWindow  *window)
{
  int pages;

  if (window->priv->disposed)
    return;

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (profile_set_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (title_changed_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (icon_title_changed_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (selection_changed_callback),
                                        window);

  terminal_screen_set_window (screen, NULL);
  window->priv->terms--;

  screen_set_menuitem (screen, NULL);
  
  update_notebook (window);

  reset_tab_menuitems (window);
  update_tab_sensitivity (window);
  update_tab_visibility (window, 0);

  pages = terminal_window_get_screen_count (window);
  if (pages == 1)
    {
      terminal_window_set_size (window, window->priv->active_term, TRUE);
    }
  else if (pages == 0)
    {
      gtk_widget_destroy (GTK_WIDGET (window));
    }
}

static void
notebook_page_reordered_callback (GtkWidget       *notebook,
                                  GtkWidget       *child,
                                  guint            page_num,
                                  TerminalWindow  *window)
{
  if (window->priv->disposed)
    return;

  reset_tab_menuitems(window);
}

void
terminal_window_update_icon (TerminalWindow *window)
{
  GdkPixbuf *new_icon;
  TerminalProfile *profile;

  if (window->priv->active_term == NULL)
    {
      gtk_window_set_icon (GTK_WINDOW (window), NULL);
      return;
    }

  profile = terminal_screen_get_profile (window->priv->active_term);
  if (profile == NULL)
    {
      gtk_window_set_icon (GTK_WINDOW (window), NULL);
      return;
    }
  
  new_icon = terminal_profile_get_icon (profile);
  
  if (window->priv->icon != new_icon)
    {
      if (new_icon)
        g_object_ref (G_OBJECT (new_icon));

      if (window->priv->icon)
        g_object_unref (G_OBJECT (window->priv->icon));

      window->priv->icon = new_icon;

      gtk_window_set_icon (GTK_WINDOW (window), window->priv->icon);
    }
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
  GdkGeometry hints;
  GtkWidget *widget;  
  int char_width;
  int char_height;
  
  if (window->priv->active_term == NULL)
    return;
  
  widget = terminal_screen_get_widget (window->priv->active_term);
  
  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */
  terminal_widget_get_cell_size (widget, &char_width, &char_height);
  
  if (char_width != window->priv->old_char_width ||
      char_height != window->priv->old_char_height ||
      widget != (GtkWidget*) window->priv->old_geometry_widget)
    {
      int xpad, ypad;
      
      /* FIXME Since we're using xthickness/ythickness to compute
       * padding we need to change the hints when the theme changes.
       */
      terminal_widget_get_padding (widget, &xpad, &ypad);
      
      hints.base_width = xpad;
      hints.base_height = ypad;

#define MIN_WIDTH_CHARS 4
#define MIN_HEIGHT_CHARS 2
      
      hints.width_inc = char_width;
      hints.height_inc = char_height;

      /* min size is min size of just the geometry widget, remember. */
      hints.min_width = hints.base_width + hints.width_inc * MIN_WIDTH_CHARS;
      hints.min_height = hints.base_height + hints.height_inc * MIN_HEIGHT_CHARS;
      
      gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                     widget,
                                     &hints,
                                     GDK_HINT_RESIZE_INC |
                                     GDK_HINT_MIN_SIZE |
                                     GDK_HINT_BASE_SIZE);

#ifdef DEBUG_GEOMETRY
      g_fprintf (stderr,"hints: base %dx%d min %dx%d inc %d %d\n",
               hints.base_width,
               hints.base_height,
               hints.min_width,
               hints.min_height,
               hints.width_inc,
               hints.height_inc);
#endif
      
      window->priv->old_char_width = hints.width_inc;
      window->priv->old_char_height = hints.height_inc;
      window->priv->old_geometry_widget = widget;
    }
#ifdef DEBUG_GEOMETRY
  else
    {
      g_fprintf (stderr,"hints: increment unchanged, not setting\n");
    }
#endif
}

/*
 * Config updates
 */

static void
config_change_notify (GConfClient *client,
                      guint        cnxn_id,
                      GConfEntry  *entry,
                      gpointer     user_data)
{
  GConfValue *val;
  TerminalWindow *window;

  window = TERMINAL_WINDOW (user_data);

  val = gconf_entry_get_value (entry);
  
  if (strcmp (gconf_entry_get_key (entry),
              CONF_GLOBAL_PREFIX"/use_mnemonics") == 0)
    {      
      if (val && val->type == GCONF_VALUE_BOOL)
        {
          window->priv->use_mnemonics = gconf_value_get_bool (val);
          reset_menubar_labels (window);
        }
    }
}

static void
set_menuitem_text (GtkWidget  *mi,
                   const char *text,
                   gboolean    strip_mnemonic)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (mi));

  if (child && GTK_IS_LABEL (child))
    {
      const char *label;
      char *no_mnemonic;
      
      label = NULL;
      no_mnemonic = NULL;
      
      if (strip_mnemonic)
        {
          const char *src;
          char *dest;

          no_mnemonic = g_strdup (text);
          dest = no_mnemonic;
          src = text;

          while (*src)
            {
              if (*src != '_')
                {
                  *dest = *src;
                  ++dest;
                }
              
              ++src;
            }
          *dest = '\0';

          label = no_mnemonic;
        }
      else
        {
          label = text;
        }

      if (strip_mnemonic)
        gtk_label_set_text (GTK_LABEL (child), label);
      else
        gtk_label_set_text_with_mnemonic (GTK_LABEL (child), label);
      
      if (no_mnemonic)
        g_free (no_mnemonic);
    }
}

static void
reset_menubar_labels (TerminalWindow *window)
{
  gboolean want_mnemonics =
	  window->priv->use_mnemonics && window->priv->menubar_visible;

  if (want_mnemonics == window->priv->using_mnemonics)
    return;

  window->priv->using_mnemonics = want_mnemonics;

  set_menuitem_text (window->priv->file_menuitem,
                     _("_File"), !window->priv->using_mnemonics);
  set_menuitem_text (window->priv->edit_menuitem,
                     _("_Edit"), !window->priv->using_mnemonics);
  set_menuitem_text (window->priv->view_menuitem,
                     _("_View"), !window->priv->using_mnemonics);
  set_menuitem_text (window->priv->terminal_menuitem,
                     _("_Terminal"), !window->priv->using_mnemonics);
  set_menuitem_text (window->priv->go_menuitem,
                     _("Ta_bs"), !window->priv->using_mnemonics);
  set_menuitem_text (window->priv->help_menuitem,
                     _("_Help"), !window->priv->using_mnemonics);
}

static void
reset_tab_menuitems (TerminalWindow *window)
{
  GList *tmp;
  GtkWidget *menu_item;
  int i;
  TerminalScreen *screen;
  GSList *group;
  gboolean single_page;
  
  tmp = window->priv->tab_menuitems;
  while (tmp != NULL)
    {
      gtk_widget_destroy (tmp->data);
      tmp = tmp->next;
    }

  g_list_free (window->priv->tab_menuitems);
  window->priv->tab_menuitems = NULL;

  single_page = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook)) == 1;
  
  group = NULL;
  i = 0;
  while (TRUE) /* should probably make us somewhat nervous */
    {
      GtkWidget *page;
      GtkWidget *label;
      char *accel_path;
      
      page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->priv->notebook),
                                        i);

      if (page == NULL)
        break;
      
      screen = TERMINAL_SCREEN (page);

      menu_item = gtk_radio_menu_item_new_with_label (group,
                                                      terminal_screen_get_title (screen));
      label = gtk_bin_get_child (GTK_BIN (menu_item));
      gtk_label_set_use_underline (GTK_LABEL (label), FALSE);
      gtk_label_set_ellipsize (GTK_LABEL (label), TAB_MENU_ELLIPSIZE);
      gtk_label_set_max_width_chars (GTK_LABEL (label), TAB_MENU_WIDTH_CHARS); 

      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));
      
      if (i < N_TABS_WITH_ACCEL && !single_page)
        accel_path = g_strdup_printf (FORMAT_ACCEL_PATH_SWITCH_TO_TAB,
                                      i + 1);
      else
        accel_path = NULL;
      
      if (accel_path)
        {
          gtk_menu_item_set_accel_path (GTK_MENU_ITEM (menu_item),
					accel_path);
	  g_free (accel_path);
	}
      
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (window->priv->go_menu),
                             menu_item);

      if (screen == window->priv->active_term)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                        TRUE);
      
      g_object_set_data (G_OBJECT (menu_item),
                         "notebook-page",
                         GINT_TO_POINTER (i));
      
      g_signal_connect (G_OBJECT (menu_item),
                        "activate",
                        G_CALLBACK (change_tab_callback),
                        window);      

      window->priv->tab_menuitems =
        g_list_prepend (window->priv->tab_menuitems,
                        menu_item);

      /* so we can keep the title updated */
      screen_set_menuitem (screen, menu_item);
      
      ++i;
    }
}

void
terminal_window_set_fullscreen (TerminalWindow *window,
                                gboolean        setting)
{
  g_return_if_fail (GTK_WIDGET_REALIZED (window));

  window->priv->fullscreen = setting;
  
  if (setting)
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

gboolean
terminal_window_get_fullscreen (TerminalWindow *window)
{
  return window->priv->fullscreen;
}

/*
 * Callbacks for the menus
 */

static void
new_window_callback (GtkWidget      *menuitem,
                     TerminalWindow *window)
{
  TerminalProfile *profile;
  
  profile = g_object_get_data (G_OBJECT (menuitem),
                               "profile");

  g_assert (profile);

  if (!terminal_profile_get_forgotten (profile))
    {
      char *name;
      const char *dir;
      
      name = gdk_screen_make_display_name (gtk_widget_get_screen (menuitem));
      dir = terminal_screen_get_working_dir (window->priv->active_term);

      new_window (window, NULL, profile, name, dir);

      g_free (name);
    }
}

static void
new_window (TerminalWindow *window,
            TerminalScreen *screen,
            TerminalProfile *profile,
            char *name,
            const char *dir)
{
  char *geometry;

  if (screen)
    {
      GtkWidget *term;
      int width, height;

      term = terminal_screen_get_widget (screen);
      terminal_widget_get_size (term, &width, &height);
      geometry = g_strdup_printf("%dx%d", width, height);
    }
  else
    {
      geometry = NULL;
    }

  terminal_app_new_terminal (terminal_app_get (),
                             profile,
                             NULL,
                             screen,
                             FALSE, FALSE, FALSE,
                             NULL, geometry, NULL, dir, NULL, 1.0,
                             NULL, name, -1);

  g_free (geometry);
}

static void
new_tab_callback (GtkWidget      *menuitem,
                  TerminalWindow *window)
{
  TerminalProfile *profile;
  
  profile = g_object_get_data (G_OBJECT (menuitem),
                               "profile");

  g_assert (profile);

  if (!terminal_profile_get_forgotten (profile))
    {
      const char *dir;

      dir = terminal_screen_get_working_dir (window->priv->active_term);

      terminal_app_new_terminal (terminal_app_get (),
                                 profile,
                                 window,
                                 NULL,
                                 FALSE, FALSE, FALSE,
                                 NULL, NULL, NULL, dir, NULL, 1.0,
                                 NULL, NULL, -1);
    }
}

static gboolean
confirm_close_window (TerminalWindow *window)
{
  GtkWidget *dialog;
  GError *error;
  gboolean result;
  int n;

  n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook));

  if (n <= 1)
    return TRUE;

  error = NULL;
  if (!gconf_client_get_bool (window->priv->conf, CONF_GLOBAL_PREFIX "/confirm_window_close", &error))
    return TRUE;

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   "%s", _("Close all tabs?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
			  		    ngettext ("This window has one tab open. Closing "
						      "the window will close it.",
						      "This window has %d tabs open. Closing "
						      "the window will also close all tabs.",
						      n),
					    n);

  gtk_window_set_title (GTK_WINDOW(dialog), ""); 

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Close All _Tabs"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  result = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT;
  gtk_widget_destroy (dialog);

  return result;
}

static void
close_window_callback (GtkWidget      *menuitem,
                       TerminalWindow *window)
{
  if (confirm_close_window (window))
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
close_tab_callback (GtkWidget      *menuitem,
                    TerminalWindow *window)
{
  if (window->priv->active_term)
    terminal_screen_close (window->priv->active_term);
}

static void
copy_callback (GtkWidget      *menuitem,
               TerminalWindow *window)
{
  GtkWidget *widget;

  if (window->priv->active_term)
    {
      widget = terminal_screen_get_widget (window->priv->active_term);
      
      terminal_widget_copy_clipboard (widget);
    }
}

static void
paste_callback (GtkWidget      *menuitem,
                TerminalWindow *window)
{
  GtkWidget *widget;

  if (window->priv->active_term)
    {
      widget = terminal_screen_get_widget (window->priv->active_term);

      terminal_widget_paste_clipboard (widget);
    }  
}

static void
edit_keybindings_callback (GtkWidget      *menuitem,
                           TerminalWindow *window)
{
  terminal_app_edit_keybindings (terminal_app_get (),
                                 GTK_WINDOW (window));
}

static void
change_configuration_callback (GtkWidget      *menu_item,
                               TerminalWindow *window)
{
  TerminalProfile *profile;

  if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item)))
    return;

  if (window->priv->active_term == NULL)
    return;
  
  profile = g_object_get_data (G_OBJECT (menu_item),
                               "profile");

  g_assert (profile);

  if (!terminal_profile_get_forgotten (profile))
    {
      g_signal_handlers_block_by_func (G_OBJECT (window->priv->active_term), G_CALLBACK (profile_set_callback), window);
      terminal_screen_set_profile (window->priv->active_term, profile);
      g_signal_handlers_unblock_by_func (G_OBJECT (window->priv->active_term), G_CALLBACK (profile_set_callback), window);
    }
}

static void
edit_configuration_callback (GtkWidget      *menuitem,
                             TerminalWindow *window)
{
  terminal_app_edit_profile (terminal_app_get (),
                             terminal_screen_get_profile (window->priv->active_term),
                             GTK_WINDOW (window));
}

static void
new_configuration_callback (GtkWidget      *menuitem,
                            TerminalWindow *window)
{
  terminal_app_new_profile (terminal_app_get (),
                            terminal_screen_get_profile (window->priv->active_term),
                            GTK_WINDOW (window));
}

static void
manage_configurations_callback (GtkWidget      *menuitem,
                                TerminalWindow *window)
{
  terminal_app_manage_profiles (terminal_app_get (),
                                GTK_WINDOW (window));
}

static void
toggle_menubar_callback (GtkWidget      *menuitem,
                         TerminalWindow *window)
{
  terminal_window_set_menubar_visible (window, gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem)));
}

static void
fullscreen_callback (GtkWidget      *menuitem,
                     TerminalWindow *window)
{
  terminal_window_set_fullscreen (window, gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem)));
}

static gboolean
window_state_event_callback (GtkWidget            *widget, 
                             GdkEventWindowState  *event,
                             gpointer              user_data)
{
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
    TerminalWindow *window;
    GtkCheckMenuItem *menu_item;
    gboolean new_state;

    window = TERMINAL_WINDOW (widget);
    menu_item = GTK_CHECK_MENU_ITEM (user_data);

    new_state = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

    window->priv->fullscreen = new_state;
    gtk_check_menu_item_set_active (menu_item, new_state);
    }
  
  /* Call any other handlers there may be */
  return FALSE;
}

static double zoom_factors[] = {
  TERMINAL_SCALE_MINIMUM,
  TERMINAL_SCALE_XXXXX_SMALL,
  TERMINAL_SCALE_XXXX_SMALL,
  TERMINAL_SCALE_XXX_SMALL,
  PANGO_SCALE_XX_SMALL,
  PANGO_SCALE_X_SMALL,
  PANGO_SCALE_SMALL,
  PANGO_SCALE_MEDIUM,
  PANGO_SCALE_LARGE,
  PANGO_SCALE_X_LARGE,
  PANGO_SCALE_XX_LARGE,
  TERMINAL_SCALE_XXX_LARGE,
  TERMINAL_SCALE_XXXX_LARGE,
  TERMINAL_SCALE_XXXXX_LARGE,
  TERMINAL_SCALE_MAXIMUM
};

static gboolean
find_larger_zoom_factor (double  current,
                         double *found)
{
  int i;
  
  i = 0;
  while (i < (int) G_N_ELEMENTS (zoom_factors))
    {
      /* Find a font that's larger than this one */
      if ((zoom_factors[i] - current) > 1e-6)
        {
          *found = zoom_factors[i];
          return TRUE;
        }
      
      ++i;
    }
  
  return FALSE;
}

static gboolean
find_smaller_zoom_factor (double  current,
                          double *found)
{
  int i;
  
  i = (int) G_N_ELEMENTS (zoom_factors) - 1;
  while (i >= 0)
    {
      /* Find a font that's smaller than this one */
      if ((current - zoom_factors[i]) > 1e-6)
        {
          *found = zoom_factors[i];
          return TRUE;
        }
      
      --i;
    }

  return FALSE;
}

static void
zoom_in_callback (GtkWidget      *menuitem,
                  TerminalWindow *window)
{
  double current;
  TerminalScreen *screen;
  
  screen = window->priv->active_term;

  if (screen == NULL)
    return;
  
  current = terminal_screen_get_font_scale (screen);

  if (find_larger_zoom_factor (current, &current))
    {
      terminal_screen_set_font_scale (screen, current);
      update_zoom_items (window);
    }
}

static void
zoom_out_callback (GtkWidget      *menuitem,
                   TerminalWindow *window)
{
  double current;
  TerminalScreen *screen;
  
  screen = window->priv->active_term;

  if (screen == NULL)
    return;
  
  current = terminal_screen_get_font_scale (screen);

  if (find_smaller_zoom_factor (current, &current))
    {
      terminal_screen_set_font_scale (screen, current);
      update_zoom_items (window);
    }
}

static void
zoom_normal_callback (GtkWidget      *menuitem,
                      TerminalWindow *window)
{
  TerminalScreen *screen;
  
  screen = window->priv->active_term;

  if (screen == NULL)
    return;

  terminal_screen_set_font_scale (screen,
                                  PANGO_SCALE_MEDIUM);
  update_zoom_items (window);
}

static void
set_title_callback (GtkWidget      *menuitem,
                    TerminalWindow *window)
{
  if (window->priv->active_term)
    terminal_screen_edit_title (window->priv->active_term,
                                GTK_WINDOW (window));
}

static void
change_encoding_callback (GtkWidget      *menu_item,
                          TerminalWindow *window)
{
  const char *charset;
  GtkWidget *widget;
  
  if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item)))
    return;

  if (window->priv->active_term == NULL)
    return;

  charset = g_object_get_data (G_OBJECT (menu_item),
                               "encoding");

  g_assert (charset);

  widget = terminal_screen_get_widget (window->priv->active_term);
  terminal_widget_set_encoding (widget, charset);
}

static void
add_encoding_callback (GtkWidget      *menu_item,
                       TerminalWindow *window)
{
  terminal_app_edit_encodings (terminal_app_get (),
                               GTK_WINDOW (window));
}

static void
reset_callback (GtkWidget      *menuitem,
                TerminalWindow *window)
{
  GtkWidget *widget;

  if (window->priv->active_term)
    {
      widget = terminal_screen_get_widget (window->priv->active_term);

      terminal_widget_reset (widget, FALSE);
    }
}

static void
reset_and_clear_callback (GtkWidget      *menuitem,
                          TerminalWindow *window)
{
  GtkWidget *widget;

  if (window->priv->active_term)
    {
      widget = terminal_screen_get_widget (window->priv->active_term);

      terminal_widget_reset (widget, TRUE);
    }
}

static gboolean
accel_event_key_match (GdkEventKey *event, GtkAccelKey *key)
{
  GdkModifierType modifiers;

  /* Compare the keyval */
  if (event->keyval != key->accel_key)
    return FALSE;

  /* Compare the modifier keys */
  modifiers = GDK_MODIFIER_MASK & event->state;

  if (modifiers & GDK_LOCK_MASK)
    modifiers -= GDK_LOCK_MASK;
  if (modifiers & GDK_MOD2_MASK)
    modifiers -= GDK_MOD2_MASK;

  if (modifiers != key->accel_mods)
    return FALSE;

  return TRUE;
}

static gboolean
key_press_callback (GtkWidget *widget,
		    GdkEventKey *event,
		    TerminalWindow *window)
{
  GtkAccelKey key;

  /* We just pass the keys when there's no tabs */
  if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->priv->notebook)) == 1)
    return FALSE;

  /* On first page? */
  if (!GTK_WIDGET_IS_SENSITIVE (window->priv->previous_tab_menuitem))
    {
      if (gtk_accel_map_lookup_entry (ACCEL_PATH_PREV_TAB, &key))
        {
          if (accel_event_key_match (event, &key))
	    return TRUE;
	}
    }

  /* On last page? */
  if (!GTK_WIDGET_IS_SENSITIVE (window->priv->next_tab_menuitem))
    {
      if (gtk_accel_map_lookup_entry (ACCEL_PATH_NEXT_TAB, &key))
        {
          if (accel_event_key_match (event, &key))
	    return TRUE;
	}
    }

  return FALSE;
}

static void
next_tab_callback(GtkWidget      *menuitem,
                  TerminalWindow *window)
{
  gtk_notebook_next_page (GTK_NOTEBOOK (window->priv->notebook));
}

static void
previous_tab_callback (GtkWidget      *menuitem,
                       TerminalWindow *window)
{
  gtk_notebook_prev_page (GTK_NOTEBOOK (window->priv->notebook));
}

static void
move_left_tab_callback(GtkWidget      *menuitem,
                       TerminalWindow *window)
{
  GtkNotebook *notebook;
  gint page_num,last_page;
  GtkWidget *page; 

  notebook = GTK_NOTEBOOK (window->priv->notebook);
  page_num = gtk_notebook_get_current_page (notebook);
  last_page = gtk_notebook_get_n_pages (notebook) - 1;
  page = gtk_notebook_get_nth_page (notebook, page_num);
  

  gtk_notebook_reorder_child (notebook, page, page_num == 0 ? last_page : page_num - 1);

  update_tab_sensitivity (window);
  reset_tab_menuitems (window);
}

static void
move_right_tab_callback(GtkWidget      *menuitem,
                        TerminalWindow *window)
{
  GtkNotebook *notebook;
  gint page_num,last_page;
  GtkWidget *page; 

  notebook = GTK_NOTEBOOK (window->priv->notebook);
  page_num = gtk_notebook_get_current_page (notebook);
  last_page = gtk_notebook_get_n_pages (notebook) - 1;
  page = gtk_notebook_get_nth_page (notebook, page_num);
  
  gtk_notebook_reorder_child (notebook, page, page_num == last_page ? 0 : page_num + 1);

  update_tab_sensitivity (window);
  reset_tab_menuitems (window);
}

static void
detach_tab (TerminalScreen  *screen,
            TerminalWindow  *window)
{
  TerminalProfile *profile;

  profile = terminal_screen_get_profile (screen);

  g_assert (profile);

  if (!terminal_profile_get_forgotten (profile))
    {
      char *name;
      const char *dir;

      name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (window)));
      dir = terminal_screen_get_working_dir (window->priv->active_term);

      new_window (window, screen, profile, name, dir);

      g_free (name);
    }
}

static void
detach_tab_callback(GtkWidget      *menuitem,
                    TerminalWindow *window)
{
  GtkNotebook *notebook;
  gint page_num;
  GtkWidget *page; 

  notebook = GTK_NOTEBOOK (window->priv->notebook);
  page_num = gtk_notebook_get_current_page (notebook);
  page = gtk_notebook_get_nth_page (notebook, page_num);
  
  detach_tab (TERMINAL_SCREEN (page), window);
}

static void
change_tab_callback (GtkWidget      *menuitem,
                     TerminalWindow *window)
{
  if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem)))
    {
      int page_num;
      
      page_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                     "notebook-page"));

  
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
                                     page_num);
    }
}

static void
help_callback (GtkWidget      *menuitem,
               TerminalWindow *window)
{
  terminal_util_show_help (NULL, GTK_WINDOW (window));
}

static void
about_callback (GtkWidget      *menuitem,
                TerminalWindow *window)
{
  const char *copyright =
    "Copyright \xc2\xa9 2002,2003,2004 Havoc Pennington\n"
    "Copyright \xc2\xa9 2003,2004,2007 Mariano Su\303\241rez-Alvarez\n"
    "Copyright \xc2\xa9 2006 Guilherme de S. Pastore";
  const char *authors[] = {
    "Guilherme de S. Pastore <gpastore@gnome.org> (maintainer)",
    "Havoc Pennington <hp@redhat.com>",
    "Mariano Su\303\241rez-Alvarez <mariano@gnome.org>",
    NULL
  };
  const gchar *license[] = {
    N_("GNOME Terminal is free software; you can redistribute it and/or modify "
       "it under the terms of the GNU General Public License as published by "
       "the Free Software Foundation; either version 2 of the License, or "
       "(at your option) any later version."),
    N_("GNOME Terminal is distributed in the hope that it will be useful, "
       "but WITHOUT ANY WARRANTY; without even the implied warranty of "
       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
       "GNU General Public License for more details."),
    N_("You should have received a copy of the GNU General Public License "
       "along with GNOME Terminal; if not, write to the Free Software Foundation, "
       "Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA")
  };
  gchar *license_text;

  license_text = g_strjoin ("\n\n",
			    _(license[0]), _(license[1]), _(license[2]), NULL);

  gtk_show_about_dialog (GTK_WINDOW (window),
			 "name", _("GNOME Terminal"),
			 "copyright", copyright,
			 "comments", _("A terminal emulator for the GNOME desktop"),
			 "version", VERSION,
			 "authors", authors,
			 "license", license_text,
			 "wrap-license", TRUE,
			 "translator-credits", _("translator-credits"),
			 "logo-icon-name", "gnome-terminal",
			 NULL);
}


static void
default_profile_changed (TerminalProfile           *profile,
                         const TerminalSettingMask *mask,
                         void                      *data)
{
  /* This no longer applies, since our "new window" item
   * is based on the current profile, not the default profile
   */
#if 0
  if (mask & TERMINAL_SETTING_IS_DEFAULT)
    {
      TerminalWindow *window;

      window = TERMINAL_WINDOW (data);
      
      /* When the default changes, we get a settings change
       * on the old default and the new. We only rebuild
       * the menu on the notify for the new default.
       */
      if (terminal_profile_get_is_default (profile))
        fill_in_new_term_submenus (window);
    }
#endif
}

static void
monitor_profiles_for_is_default_change (TerminalWindow *window)
{
  GList *profiles;
  GList *tmp;
  
  profiles = terminal_profile_get_list ();

  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;

      g_signal_handlers_disconnect_by_func (G_OBJECT (profile),
                                            G_CALLBACK (default_profile_changed),
                                            window);
      
      g_signal_connect_object (G_OBJECT (profile),
                               "changed",
                               G_CALLBACK (default_profile_changed),
                               G_OBJECT (window),
                               0);
      
      tmp = tmp->next;
    }

  g_list_free (profiles);
}

void
terminal_window_reread_profile_list (TerminalWindow *window)
{
  monitor_profiles_for_is_default_change (window);
  
  fill_in_config_picker_submenu (window);
  fill_in_new_term_submenus (window);
}

void
terminal_window_set_startup_id (TerminalWindow *window,
                                const char     *startup_id)
{
  g_free (window->priv->startup_id);
  window->priv->startup_id = g_strdup (startup_id);
}
