/* widget for a toplevel terminal window, possibly containing multiple terminals */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2007, 2008 Christian Persch
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
#include "terminal-widget.h"
#include "terminal-window.h"
#include "terminal-tabs-menu.h"
#include "terminal.h"
#include "encoding.h"
#include <string.h>
#include <stdlib.h>
#include <libgnome/gnome-program.h>
#include <gtk/gtklabel.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <libsn/sn-launchee.h>

struct _TerminalWindowPrivate
{
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;
  guint ui_id;

  GtkActionGroup *profiles_action_group;
  guint profiles_ui_id;

  GtkActionGroup *encodings_action_group;
  guint encodings_ui_id;

  TerminalTabsMenu *tabs_menu;

  TerminalScreenPopupInfo *popup_info;
  guint remove_popup_info_idle;

  GtkActionGroup *new_terminal_action_group;
  guint new_terminal_ui_id;

  GtkWidget *menubar;
  GtkWidget *notebook;
  guint terms;
  TerminalScreen *active_term;
  GtkClipboard *clipboard;
  int old_char_width;
  int old_char_height;
  void *old_geometry_widget; /* only used for pointer value as it may be freed */
  char *startup_id;

  guint menubar_visible : 1;
  guint use_default_menubar_visibility : 1;

  /* Compositing manager integration */
  guint have_argb_visual : 1;

  guint disposed : 1;
  guint present_on_insert : 1;
};

#define PROFILE_DATA_KEY I_("Terminal::Profile")


#define FILE_NEW_TERMINAL_TAB_UI_PATH     "/menubar/File/FileNewTabProfiles"
#define FILE_NEW_TERMINAL_WINDOW_UI_PATH  "/menubar/File/FileNewWindowProfiles"
#define SET_ENCODING_ACTION_NAME_PREFIX "TerminalSetEncoding"
#define SET_ENCODING_UI_PATH "/menubar/Terminal/TerminalSetEncoding/EncodingsPH"

#define STOCK_NEW_WINDOW NULL
#define STOCK_NEW_TAB NULL
 
static void terminal_window_init        (TerminalWindow      *window);
static void terminal_window_class_init  (TerminalWindowClass *klass);
static void terminal_window_dispose     (GObject             *object);
static void terminal_window_finalize    (GObject             *object);
static gboolean terminal_window_state_event (GtkWidget            *widget,
                                             GdkEventWindowState  *event);

static gboolean terminal_window_delete_event (GtkWidget *widget,
                                              GdkEvent *event,
                                              gpointer data);

static void       screen_set_menuitem  (TerminalScreen *screen,
                                        GtkWidget      *menuitem);
static GtkWidget* screen_get_menuitem  (TerminalScreen *screen);
static TerminalScreen* find_screen (TerminalWindow *window,
                                    TerminalScreen *screen);

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

static void new_window                    (TerminalWindow  *window,
                                           TerminalScreen  *screen,
                                           TerminalProfile *profile);
static void detach_tab                    (TerminalScreen *screen,
                                           TerminalWindow *window);

/* Menu action callbacks */
static void terminal_menu_activate_callback (GtkAction *action,
                                           TerminalWindow *window);

static void file_new_window_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void file_new_tab_callback             (GtkAction *action,
                                               TerminalWindow *window);
static void file_close_window_callback        (GtkAction *action,
                                               TerminalWindow *window);
static void file_close_tab_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void edit_copy_callback                (GtkAction *action,
                                               TerminalWindow *window);
static void edit_paste_callback               (GtkAction *action,
                                               TerminalWindow *window);
static void edit_keybindings_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void edit_profiles_callback            (GtkAction *action,
                                               TerminalWindow *window);
static void change_configuration_callback     (GtkAction *action,
                                               TerminalWindow *window);
static void edit_current_profile_callback     (GtkAction *action,
                                               TerminalWindow *window);
static void file_new_profile_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void view_menubar_toggled_callback     (GtkToggleAction *action,
                                               TerminalWindow *window);
static void view_fullscreen_toggled_callback  (GtkToggleAction *action,
                                               TerminalWindow *window);
static void view_zoom_in_callback             (GtkAction *action,
                                               TerminalWindow *window);
static void view_zoom_out_callback            (GtkAction *action,
                                               TerminalWindow *window);
static void view_zoom_normal_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_set_title_callback       (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_add_encoding_callback    (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_reset_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_reset_clear_callback     (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_next_tab_callback            (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_previous_tab_callback        (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_move_left_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_move_right_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_detach_tab_callback          (GtkAction *action,
                                               TerminalWindow *window);

static void change_tab_callback           (GtkAction *action,
                                           TerminalWindow *window);
                                           /* FIXME? */

static void help_contents_callback        (GtkAction *action,
                                           TerminalWindow *window);
static void help_about_callback           (GtkAction *action,
                                           TerminalWindow *window);

static gboolean find_larger_zoom_factor  (double  current,
                                          double *found);
static gboolean find_smaller_zoom_factor (double  current,
                                          double *found);

static void terminal_window_show (GtkWidget *widget);

static gboolean confirm_close_window (TerminalWindow *window);
static void
profile_set_callback (TerminalScreen *screen,
                      TerminalWindow *window);

G_DEFINE_TYPE (TerminalWindow, terminal_window, GTK_TYPE_WINDOW)

/* Menubar mnemonics settings handling */

static void
mnemonics_setting_change_notify (GConfClient *client,
                                 guint        cnxn_id,
                                 GConfEntry  *entry,
                                 gpointer     user_data)
{
  GdkScreen *screen;
  GtkSettings *settings;
  GConfValue *val;

  if (strcmp (gconf_entry_get_key (entry),
              CONF_GLOBAL_PREFIX"/use_mnemonics") != 0)
    return;
 
  val = gconf_entry_get_value (entry);
  if (!val || val->type != GCONF_VALUE_BOOL)
    return;

  screen = GDK_SCREEN (user_data);
  settings = gtk_settings_get_for_screen (screen);
  g_object_set (settings,
                "gtk-enable-mnemonics",
                gconf_value_get_bool (val),
                NULL);
}

static void
mnemonics_setting_change_destroy (GdkScreen *screen)
{
  GConfClient *client;
  guint id;

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (screen), "terminal-settings-connection"));
  g_assert (id != 0);

  client = gconf_client_get_default ();
  gconf_client_notify_remove (client, id);
  g_object_unref (client);
}

/* utility functions */

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
terminal_set_profile_toggled_callback (GtkToggleAction *action,
                                       TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalProfile *profile;

  if (!gtk_toggle_action_get_active (action))
    return;

  if (priv->active_term == NULL)
    return;
  
  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  g_assert (profile);

  if (terminal_profile_get_forgotten (profile))
    return;
      
  g_signal_handlers_block_by_func (priv->active_term, G_CALLBACK (profile_set_callback), window);
  terminal_screen_set_profile (priv->active_term, profile);    
  g_signal_handlers_unblock_by_func (priv->active_term, G_CALLBACK (profile_set_callback), window);
}

#define PROFILES_UI_PATH "/menubar/Terminal/TerminalProfiles"
#define PROFILES_POPUP_UI_PATH "/Popup/TerminalProfiles"

static void
terminal_window_update_set_profile_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalProfile *active_profile;
  GtkActionGroup *action_group;
  GtkAction *action;
  GList *profiles, *p;
  GSList *group;
  guint n;

  /* Remove the old UI */
  if (priv->profiles_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->profiles_ui_id);
      priv->profiles_ui_id = 0;
    }

  if (priv->profiles_action_group != NULL)
    {
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->profiles_action_group);
      priv->profiles_action_group = NULL;
    }

  if (priv->active_term == NULL)
    return;

  profiles = terminal_profile_get_list ();

  action = gtk_action_group_get_action (priv->action_group, "TerminalProfiles");
  gtk_action_set_sensitive (action, profiles && profiles->next != NULL /* list length >= 2 */);

  if (profiles == NULL)
    return;

  active_profile = terminal_screen_get_profile (priv->active_term);

  action_group = priv->profiles_action_group = gtk_action_group_new ("Profiles");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->profiles_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  group = NULL;
  n = 0;
  for (p = profiles; p != NULL; p = p->next)
    {
      TerminalProfile *profile = (TerminalProfile *) p->data;
      GtkRadioAction *profile_action;
      char name[32];
      char *display_name;

      g_snprintf (name, sizeof (name), "TerminalSetProfile%u", n++);
      display_name = escape_underscores (terminal_profile_get_visible_name (profile));
      profile_action = gtk_radio_action_new (name,
                                             display_name,
                                             NULL,
                                             NULL,
                                             n);
      g_free (display_name);

      /* FIXMEchpe: connect to "changed" on the profile */

      gtk_radio_action_set_group (profile_action, group);
      group = gtk_radio_action_get_group (profile_action);

      if (profile == active_profile)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (profile_action), TRUE);

      g_object_set_data_full (G_OBJECT (profile_action),
                              PROFILE_DATA_KEY,
                              g_object_ref (profile),
                              (GDestroyNotify) g_object_unref);
      g_signal_connect (profile_action, "toggled",
                        G_CALLBACK (terminal_set_profile_toggled_callback), window);

      gtk_action_group_add_action (action_group, GTK_ACTION (profile_action));
      g_object_unref (profile_action);

      gtk_ui_manager_add_ui (priv->ui_manager, priv->profiles_ui_id,
                             PROFILES_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
      gtk_ui_manager_add_ui (priv->ui_manager, priv->profiles_ui_id,
                             PROFILES_POPUP_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  g_list_free (profiles);
}

static void
terminal_window_create_new_terminal_action (TerminalWindow *window,
                                            TerminalProfile *profile,
                                            const char *name,
                                            guint num,
                                            GCallback callback)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  char *profile_name, *display_name;

  profile_name = escape_underscores (terminal_profile_get_visible_name (profile));
  if (num < 10)
    {
      display_name = g_strdup_printf (_("_%d. %s"), num, profile_name);
    }
  else if (num < 36)
    {
      display_name = g_strdup_printf (_("_%c. %s"), ('A' + num - 10), profile_name);
    }
  else
    {
      display_name = profile_name;
      profile_name = NULL;
    }

  action = gtk_action_new (name, display_name, NULL, NULL);
  g_free (profile_name);
  g_free (display_name);

  /* FIXMEchpe: connect to "changed" on the profile */
  g_object_set_data_full (G_OBJECT (action),
                          PROFILE_DATA_KEY,
                          g_object_ref (profile),
                          (GDestroyNotify) g_object_unref);
  g_signal_connect (action, "activate", callback, window);

  gtk_action_group_add_action (priv->new_terminal_action_group, action);
  g_object_unref (action);
}

static void
terminal_window_update_new_terminal_menus (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkActionGroup *action_group;
  GtkAction *action;
  GList *profiles, *p;
  guint n;
  gboolean have_single_profile;

  /* Remove the old UI */
  if (priv->new_terminal_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->new_terminal_ui_id);
      priv->new_terminal_ui_id = 0;
    }

  if (priv->new_terminal_action_group != NULL)
    {
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->new_terminal_action_group);
      priv->new_terminal_action_group = NULL;
    }

  profiles = terminal_profile_get_list ();
  have_single_profile = !profiles || !profiles->next;

  action = gtk_action_group_get_action (priv->action_group, "FileNewTab");
  gtk_action_set_visible (action, have_single_profile);
  action = gtk_action_group_get_action (priv->action_group, "FileNewWindow");
  gtk_action_set_visible (action, have_single_profile);

  if (have_single_profile)
    {
      g_list_free (profiles);
      return;
    }

  /* Now build the submenus */

  action_group = priv->new_terminal_action_group = gtk_action_group_new ("NewTerminal");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->new_terminal_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  n = 0;
  for (p = profiles; p != NULL; p = p->next)
    {
      TerminalProfile *profile = (TerminalProfile *) p->data;
      char *profile_name, *display_name;
      char name[32];

      g_snprintf (name, sizeof (name), "FileNewTab%u", n);
      terminal_window_create_new_terminal_action (window,
                                                  profile,
                                                  name,
                                                  n,
                                                  G_CALLBACK (file_new_tab_callback));

      gtk_ui_manager_add_ui (priv->ui_manager, priv->new_terminal_ui_id,
                             FILE_NEW_TERMINAL_TAB_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);

      g_snprintf (name, sizeof (name), "FileNewWindow%u", n);
      terminal_window_create_new_terminal_action (window,
                                                  profile,
                                                  name,
                                                  n,
                                                  G_CALLBACK (file_new_window_callback));

      gtk_ui_manager_add_ui (priv->ui_manager, priv->new_terminal_ui_id,
                             FILE_NEW_TERMINAL_WINDOW_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);

      ++n;
    }

  g_list_free (profiles);
}

static void
terminal_set_encoding_callback (GtkToggleAction *action,
                                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *name, *charset;
  GtkWidget *widget;
  
  if (!gtk_toggle_action_get_active (action))
    return;

  if (priv->active_term == NULL)
    return;

  name = gtk_action_get_name (GTK_ACTION (action));
  g_assert (g_str_has_prefix (name, SET_ENCODING_ACTION_NAME_PREFIX));
  charset = name + strlen (SET_ENCODING_ACTION_NAME_PREFIX);

  widget = terminal_screen_get_widget (priv->active_term);
  terminal_widget_set_encoding (widget, charset);
}

static void
terminal_window_update_encoding_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkActionGroup *action_group;
  GSList *group;
  guint n;
  GSList *encodings, *l;
  const char *charset;
  GtkWidget *widget;

  if (!terminal_widget_supports_dynamic_encoding ())
    return;
  
  /* Remove the old UI */
  if (priv->encodings_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->encodings_ui_id);
      priv->encodings_ui_id = 0;
    }

  if (priv->encodings_action_group != NULL)
    {
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->encodings_action_group);
      priv->encodings_action_group = NULL;
    }

  if (priv->active_term == NULL)
    return;

  action_group = priv->encodings_action_group = gtk_action_group_new ("Encodings");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->encodings_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  widget = terminal_screen_get_widget (priv->active_term);
  charset = terminal_widget_get_encoding (widget);
  
  encodings = terminal_get_active_encodings ();

  group = NULL;
  n = 0;
  for (l = encodings; l != NULL; l = l->next)
    {
      TerminalEncoding *e = (TerminalEncoding *) l->data;
      GtkRadioAction *encoding_action;
      char name[128];
      char *display_name;
      
      g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s", e->charset);
      display_name = g_strdup_printf ("%s (%s)", e->name, e->charset);

      g_print ("Encoding name %s encoding %s\n", e->name, e->charset);

      encoding_action = gtk_radio_action_new (name,
                                              display_name,
                                              NULL,
                                              NULL,
                                              n);
      g_free (display_name);

      gtk_radio_action_set_group (encoding_action, group);
      group = gtk_radio_action_get_group (encoding_action);

      if (strcmp (e->charset, charset) == 0)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (encoding_action), TRUE);

      g_signal_connect (encoding_action, "toggled",
                        G_CALLBACK (terminal_set_encoding_callback), window);

      gtk_action_group_add_action (action_group, GTK_ACTION (encoding_action));
      g_object_unref (encoding_action);

      gtk_ui_manager_add_ui (priv->ui_manager, priv->encodings_ui_id,
                             SET_ENCODING_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  g_slist_foreach (encodings, (GFunc) terminal_encoding_free, NULL);
  g_slist_free (encodings);
}

/* Actions stuff */

static void
terminal_window_update_copy_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean can_copy = FALSE;

  if (priv->active_term)
    can_copy = terminal_screen_get_text_selected (priv->active_term);

  action = gtk_action_group_get_action (priv->action_group, "EditCopy");
  gtk_action_set_sensitive (action, can_copy);
}

static void
terminal_window_update_zoom_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen;
  GtkAction *action;
  double current, zoom;
  
  screen = priv->active_term;
  if (screen == NULL)
    return;

  current = terminal_screen_get_font_scale (screen);

  action = gtk_action_group_get_action (priv->action_group, "ViewZoomIn");
  gtk_action_set_sensitive (action, find_smaller_zoom_factor (current, &zoom));
  action = gtk_action_group_get_action (priv->action_group, "ViewZoomIn");
  gtk_action_set_sensitive (action, find_larger_zoom_factor (current, &zoom));
}

static void
update_edit_menu (GtkClipboard *clipboard,
                  const  gchar *text,
                  TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;

  action = gtk_action_group_get_action (priv->action_group, "EditPaste");
  gtk_action_set_sensitive (action, text != NULL);
}

static void
terminal_menu_activate_callback (GtkAction *action,
                                 TerminalWindow *window)
{
  /* FIXMEchpe why? it's already updated when the active term changes */
  terminal_window_update_encoding_menu (window);
}

static void
edit_menu_activate_callback (GtkMenuItem *menuitem,
                             gpointer     user_data)
{
  TerminalWindow *window = (TerminalWindow *) user_data;
  TerminalWindowPrivate *priv = window->priv;

  gtk_clipboard_request_text (priv->clipboard, (GtkClipboardTextReceivedFunc) update_edit_menu, window);
}

static void
terminal_window_update_tabs_menu_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (priv->notebook);
  GtkActionGroup *action_group = priv->action_group;
  GtkAction *action;
  int num_pages, page_num;
  gboolean not_first, not_last;

  if (priv->disposed)
    return;

  num_pages = gtk_notebook_get_n_pages (notebook);
  page_num = gtk_notebook_get_current_page (notebook);
  not_first = page_num > 0;
  not_last = page_num + 1 < num_pages;

  action = gtk_action_group_get_action (action_group, "TabsPrevious");
  gtk_action_set_sensitive (action, not_first);
  action = gtk_action_group_get_action (action_group, "TabsNext");
  gtk_action_set_sensitive (action, not_last);
  action = gtk_action_group_get_action (action_group, "TabsMoveLeft");
  gtk_action_set_sensitive (action, not_first);
  action = gtk_action_group_get_action (action_group, "TabsMoveRight");
  gtk_action_set_sensitive (action, not_last);
  action = gtk_action_group_get_action (action_group, "TabsDetach");
  gtk_action_set_sensitive (action, num_pages > 0);
  action = gtk_action_group_get_action (action_group, "FileCloseTab");
  gtk_action_set_sensitive (action, num_pages > 0);
}

static void
initialize_alpha_mode (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GdkScreen *screen;
  GdkColormap *colormap;

  screen = gtk_widget_get_screen (GTK_WIDGET (window));
  colormap = gdk_screen_get_rgba_colormap (screen);
  if (colormap != NULL && gdk_screen_is_composited (screen))
    {
      /* Set RGBA colormap if possible so VTE can use real alpha
       * channels for transparency. */

      gtk_widget_set_colormap(GTK_WIDGET (window), colormap);
      priv->have_argb_visual = TRUE;
    }
  else
    {
      priv->have_argb_visual = FALSE;
    }
}

gboolean
terminal_window_uses_argb_visual (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  return priv->have_argb_visual;
}

static void
update_tab_visibility (TerminalWindow *window,
                       int             change)
{
  TerminalWindowPrivate *priv = window->priv;
  gboolean show_tabs;
  guint num;

  num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));

  show_tabs = (num + change) > 1;
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), show_tabs);
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
  TerminalWindowPrivate *dest_priv;
  double zoom;

  screen = TERMINAL_SCREEN (child);
  source_window = TERMINAL_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (source_notebook)));

  g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), NULL);

  zoom = terminal_screen_get_font_scale (screen);

  dest_window = terminal_app_new_window (terminal_app_get (), NULL, NULL, NULL, -1);
  dest_priv = dest_window->priv;
  dest_priv->present_on_insert = TRUE;

  update_tab_visibility (source_window, -1);
  update_tab_visibility (dest_window, +1);

  return GTK_NOTEBOOK (dest_priv->notebook);
}

static void
terminal_window_realized_callback (GtkWidget *window,
                                   gpointer   user_data)
{
  gdk_window_set_group (window->window, window->window);
  g_signal_handlers_disconnect_by_func (window, terminal_window_realized_callback, NULL);
}

/* Terminal screen popup menu handling */

static void
popup_open_url_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;

  if (info == NULL)
    return;

  terminal_util_open_url (GTK_WIDGET (window), info->string, info->flavour);
}

static void
popup_copy_url_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;
  GdkDisplay *display;
  GtkClipboard *clipboard;

  if (info == NULL)
    return;

  if (info->string == NULL)
    return;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_NONE);
  gtk_clipboard_set_text (clipboard, info->string, -1);
}

static void
remove_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->remove_popup_info_idle != 0)
    {
      g_source_remove (priv->remove_popup_info_idle);
      priv->remove_popup_info_idle = 0;
    }

  if (priv->popup_info != NULL)
    {
      terminal_screen_popup_info_unref (priv->popup_info);
      priv->popup_info = NULL;
    }
}

static gboolean
idle_remove_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  priv->remove_popup_info_idle = 0;
  remove_popup_info (window);
  return FALSE;
}

static void
unset_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  /* Unref the event from idle since we still need it
   * from the action callbacks which will run before idle.
   */
  if (priv->remove_popup_info_idle == 0 &&
      priv->popup_info != NULL)
    {
      priv->remove_popup_info_idle =
        g_idle_add ((GSourceFunc) idle_remove_popup_info, window);
    }
}

static void
popup_menu_deactivate_callback (GtkWidget *popup,
                                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *im_menu_item;

  g_signal_handlers_disconnect_by_func
    (popup, G_CALLBACK (popup_menu_deactivate_callback), window);

  im_menu_item = gtk_ui_manager_get_widget (priv->ui_manager,
                                            "/Popup/PopupInputMethods");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (im_menu_item), NULL);

  unset_popup_info (window);
}

static void
popup_clipboard_request_callback (GtkClipboard *clipboard,
                                  const char   *text,
                                  gpointer      user_data)
{
  TerminalScreenPopupInfo *info = user_data;
  TerminalWindow *window = info->window;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen = info->screen;
  GtkWidget *popup_menu, *im_menu, *im_menu_item;
  GtkAction *action;
  gboolean show_link, show_email_link, show_input_method_menu;
  
  remove_popup_info (window);

  if (!GTK_WIDGET_REALIZED (terminal_screen_get_widget (info->screen)))
    {
      terminal_screen_popup_info_unref (info);
      return;
    }

  priv->popup_info = info; /* adopt the ref added when requesting the clipboard */

  show_link = info->string != NULL && info->flavour != FLAVOR_EMAIL;
  show_email_link = info->string != NULL && info->flavour == FLAVOR_EMAIL;

  action = gtk_action_group_get_action (priv->action_group, "PopupSendEmail");
  gtk_action_set_visible (action, show_email_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyEmailAddress");
  gtk_action_set_visible (action, show_email_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupOpenLink");
  gtk_action_set_visible (action, show_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyLinkAddress");
  gtk_action_set_visible (action, show_link);

  action = gtk_action_group_get_action (priv->action_group, "PopupCloseWindow");
  gtk_action_set_visible (action, priv->terms <= 1);
  action = gtk_action_group_get_action (priv->action_group, "PopupCloseTab");
  gtk_action_set_visible (action, priv->terms > 1);

  action = gtk_action_group_get_action (priv->action_group, "PopupCopy");
  gtk_action_set_sensitive (action, terminal_screen_get_text_selected (screen));
  action = gtk_action_group_get_action (priv->action_group, "PopupPaste");
  gtk_action_set_sensitive (action, text != NULL);
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-show-input-method-menu", &show_input_method_menu,
                NULL);

  action = gtk_action_group_get_action (priv->action_group, "PopupInputMethods");
  gtk_action_set_visible (action, show_input_method_menu);

  im_menu_item = gtk_ui_manager_get_widget (priv->ui_manager,
                                            "/Popup/PopupInputMethods");
  /* FIXME: fix this when gtk+ bug #500065 is done */
  if (show_input_method_menu)
    {
      im_menu = gtk_menu_new ();
      terminal_widget_im_append_menuitems (terminal_screen_get_widget (screen),
                                           GTK_MENU_SHELL (im_menu));
      gtk_widget_show (im_menu);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (im_menu_item), im_menu);
    }
  else
    {
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (im_menu_item), NULL);
    }

  popup_menu = gtk_ui_manager_get_widget (priv->ui_manager, "/Popup");
  g_signal_connect (popup_menu, "deactivate",
                    G_CALLBACK (popup_menu_deactivate_callback), window);

  /* Pseudo activation of the popup menu's action */
  action = gtk_action_group_get_action (priv->action_group, "Popup");
  gtk_action_activate (action);

  gtk_menu_popup (GTK_MENU (popup_menu),
                  NULL, NULL,
                  NULL, NULL, 
                  info->button,
                  info->timestamp);
}

static void
screen_show_popup_menu_callback (TerminalScreen *screen,
                                 TerminalScreenPopupInfo *info,
                                 TerminalWindow *window)
{
  GtkClipboard *clipboard;

  g_return_if_fail (info->window == window);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_NONE /* FIXMEchpe ? */);
  gtk_clipboard_request_text (clipboard, popup_clipboard_request_callback,
                              terminal_screen_popup_info_ref (info));
}

/*****************************************/


static gboolean
terminal_window_state_event (GtkWidget            *widget,
                             GdkEventWindowState  *event)
{
  gboolean (* window_state_event) (GtkWidget *, GdkEventWindowState *event) =
      GTK_WIDGET_CLASS (terminal_window_parent_class)->window_state_event;

  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      TerminalWindow *window = TERMINAL_WINDOW (widget);
      TerminalWindowPrivate *priv = window->priv;
      GtkAction *action;
      gboolean is_fullscreen;

      is_fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;

      action = gtk_action_group_get_action (priv->action_group, "ViewFullscreen");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), is_fullscreen);
    }
  
  if (window_state_event)
    return window_state_event (widget, event);

  return FALSE;
}

static void
terminal_window_settings_update (GtkWidget *widget)
{
  GdkScreen *screen;
  GConfClient *client;
  gboolean use_mnemonics;
  guint id;

  if (!gtk_widget_has_screen (widget))
    return;

  screen = gtk_widget_get_screen (widget);
  if (0 != GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (screen), "terminal-settings-connection")))
    return;

  client = gconf_client_get_default ();
  id = gconf_client_notify_add (client,
                                CONF_GLOBAL_PREFIX "/use_mnemonics",
                                mnemonics_setting_change_notify,
                                screen,
                                NULL, NULL);
  g_object_set_data_full (G_OBJECT (screen), "terminal-settings-connection",
                          GUINT_TO_POINTER (id),
                          (GDestroyNotify) mnemonics_setting_change_destroy);

  use_mnemonics = gconf_client_get_bool (client,
                                          CONF_GLOBAL_PREFIX "/use_mnemonics",
                                          NULL);
  g_object_unref (client);

  g_object_set (gtk_settings_get_for_screen (screen),
                "gtk-enable-mnemonics",
                use_mnemonics,
                NULL);
}

static void
terminal_window_screen_changed (GtkWidget *widget,
                                GdkScreen *previous_screen)
{
  void (* screen_changed) (GtkWidget *, GdkScreen *) =
    GTK_WIDGET_CLASS (terminal_window_parent_class)->screen_changed;

  if (screen_changed)
    screen_changed (widget, previous_screen);

  if (previous_screen == gtk_widget_get_screen (widget))
    return;

  terminal_window_settings_update (widget);
}

static void
terminal_window_init (TerminalWindow *window)
{
  const GtkActionEntry menu_entries[] =
    {
      /* Toplevel */
      { "File", NULL, N_("_File") },
      { "FileNewTabProfiles", NULL, N_("Open _Terminal") },
      { "FileNewWindowProfiles", NULL, N_("Open Ta_b") },
      { "Edit", NULL, N_("_Edit") },
      { "View", NULL, N_("_View") },
      { "Terminal", NULL, N_("_Terminal") },
      { "Tabs", NULL, N_("_Tabs") },
      { "Help", NULL, N_("_Help") },
      { "Popup", NULL, NULL },

      /* File menu */
      { "FileNewWindow", STOCK_NEW_WINDOW, N_("Open _Terminal"), NULL,
        NULL,
        G_CALLBACK (file_new_window_callback) },
      { "FileNewTab", STOCK_NEW_TAB, N_("Open Ta_b"), "<shift><control>T",
        NULL,
        G_CALLBACK (file_new_tab_callback) },
      { "FileNewProfile", GTK_STOCK_OPEN, N_("New _Profile…"), NULL,
        NULL,
        G_CALLBACK (file_new_profile_callback) },
      { "FileCloseTab", GTK_STOCK_CLOSE, N_("C_lose Tab"), NULL,
        NULL,
        G_CALLBACK (file_close_tab_callback) },
      { "FileCloseWindow", GTK_STOCK_CLOSE, N_("_Close Window"), NULL,
        NULL,
        G_CALLBACK (file_close_window_callback) },

      /* Edit menu */
      { "EditCopy", GTK_STOCK_COPY, NULL, NULL,
        NULL,
        G_CALLBACK (edit_copy_callback) },
      { "EditPaste", GTK_STOCK_PASTE, NULL, NULL,
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "EditProfiles", NULL, N_("P_rofiles…"), NULL,
        NULL,
        G_CALLBACK (edit_profiles_callback) },
      { "EditKeybindings", NULL, N_("_Keyboard Shortcuts…"), NULL,
        NULL,
        G_CALLBACK (edit_keybindings_callback) },
      { "EditCurrentProfile", NULL, N_("C_urrent Profile…"), NULL,
        NULL,
        G_CALLBACK (edit_current_profile_callback) },

      /* View menu */
      { "ViewZoomIn", GTK_STOCK_ZOOM_IN, NULL, NULL,
        NULL,
        G_CALLBACK (view_zoom_in_callback) },
      { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, NULL, NULL,
        NULL,
        G_CALLBACK (view_zoom_out_callback) },
      { "ViewZoom100", GTK_STOCK_ZOOM_100, NULL, NULL,
        NULL,
        G_CALLBACK (view_zoom_normal_callback) },

      /* Terminal menu */
      { "TerminalProfiles", NULL, N_("Change _Profile") },
      { "TerminalSetTitle", NULL, N_("_Set Title…"), NULL,
        NULL,
        G_CALLBACK (terminal_set_title_callback) },
      { "TerminalSetEncoding", NULL, N_("Set _Character Encoding") },
      { "TerminalReset", NULL, N_("_Reset"), NULL,
        NULL,
        G_CALLBACK (terminal_reset_callback) },
      { "TerminalResetClear", NULL, N_("Reset and C_lear"), NULL,
        NULL,
        G_CALLBACK (terminal_reset_clear_callback) },

      /* Terminal/Encodings menu */
      { "TerminalAddEncoding", NULL, N_("_Add or Remove…"), NULL,
        NULL,
        G_CALLBACK (terminal_add_encoding_callback) },

      /* Tabs menu */
      { "TabsPrevious", NULL, N_("_Previous Tab"), "<control>Page_Up",
        NULL,
        G_CALLBACK (tabs_previous_tab_callback) },
      { "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down",
        NULL,
        G_CALLBACK (tabs_next_tab_callback) },
      { "TabsMoveLeft", NULL, N_("Move Tab _Left"), "<shift><control>Page_Up",
        NULL,
        G_CALLBACK (tabs_move_left_callback) },
      { "TabsMoveRight", NULL, N_("Move Tab _Right"), "<shift><control>Page_Down",
        NULL,
        G_CALLBACK (tabs_move_right_callback) },
      { "TabsDetach", NULL, N_("_Detach tab"), NULL,
        NULL,
        G_CALLBACK (tabs_detach_tab_callback) },

      /* Help menu */
      { "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
        NULL,
        G_CALLBACK (help_contents_callback) },
      { "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
        NULL,
        G_CALLBACK (help_about_callback) },

      /* Popup menu */
      { "PopupSendEmail", NULL, N_("_Send Mail To..."), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyEmailAddress", NULL, N_("_Copy E-mail Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupOpenLink", NULL, N_("_Open Link"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupCopy", GTK_STOCK_COPY, NULL, NULL,
        NULL,
        G_CALLBACK (edit_copy_callback) },
      { "PopupPaste", GTK_STOCK_PASTE, NULL, NULL,
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "PopupNewTerminal", NULL, N_("Open _Terminal"), NULL,
        NULL,
        G_CALLBACK (file_new_window_callback) },
      { "PopupNewTab", NULL, N_("Open Ta_b"), NULL,
        NULL,
        G_CALLBACK (file_new_tab_callback) },
      { "PopupCloseWindow", NULL, N_("C_lose Window"), NULL,
        NULL,
        G_CALLBACK (file_close_window_callback) },
      { "PopupCloseTab", NULL, N_("C_lose Tab"), NULL,
        NULL,
        G_CALLBACK (file_close_tab_callback) },
      { "PopupInputMethods", NULL, N_("_Input Methods") }
    };
  
  const GtkToggleActionEntry toggle_menu_entries[] =
    {
      /* View Menu */
      { "ViewMenubar", NULL, N_("Show Menu_bar"), NULL,
        NULL,
        G_CALLBACK (view_menubar_toggled_callback),
        FALSE },
      { "ViewFullscreen", NULL, N_("_Full Screen"), NULL,
        NULL,
        G_CALLBACK (view_fullscreen_toggled_callback),
        FALSE }
    };
  TerminalWindowPrivate *priv;
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkUIManager *manager;
  GtkWidget *main_vbox;
  GtkWidget *mi;
  GtkWidget *menu;
  GtkAccelGroup *accel_group;
  GError *error;


  priv = window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, TERMINAL_TYPE_WINDOW, TerminalWindowPrivate);

  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK(terminal_window_delete_event),
                    NULL);
  g_signal_connect (G_OBJECT (window), "realize",
                    G_CALLBACK (terminal_window_realized_callback),
                    NULL);

  gtk_window_set_title (GTK_WINDOW (window), _("Terminal"));

  priv->terms = 0;
  priv->active_term = NULL;
  priv->menubar_visible = FALSE;
  
  main_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), main_vbox);
  gtk_widget_show (main_vbox);

  priv->notebook = gtk_notebook_new ();
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (priv->notebook), TRUE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
  gtk_notebook_set_group_id (GTK_NOTEBOOK (priv->notebook), 1);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (priv->notebook),
                               TRUE);                                      
  g_signal_connect_after (priv->notebook, "switch-page",
                          G_CALLBACK (notebook_page_selected_callback), window);
  g_signal_connect_after (priv->notebook, "page-added",
                          G_CALLBACK (notebook_page_added_callback), window);
  g_signal_connect_after (priv->notebook, "page-removed",
                          G_CALLBACK (notebook_page_removed_callback), window);
  g_signal_connect_data (priv->notebook, "page-reordered",
                         G_CALLBACK (terminal_window_update_tabs_menu_sensitivity),
                         window, NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  
  gtk_box_pack_end (GTK_BOX (main_vbox), priv->notebook, TRUE, TRUE, 0);
  gtk_widget_show (priv->notebook);

  priv->old_char_width = -1;
  priv->old_char_height = -1;
  priv->old_geometry_widget = NULL;
  
  initialize_alpha_mode (window);

  /* force gtk to construct its GtkClipboard; otherwise our UI is very slow the first time we need it */
  /* FIXMEchpe is that really true still ?? */
  priv->clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_NONE);

  /* Create the UI manager */
  manager = priv->ui_manager = gtk_ui_manager_new ();
  gtk_window_add_accel_group (GTK_WINDOW (window),
                              gtk_ui_manager_get_accel_group (manager));

  /* Create the actions */
  priv->action_group = action_group = gtk_action_group_new ("Main");
  gtk_action_group_set_translation_domain (action_group, NULL);
  gtk_action_group_add_actions (action_group, menu_entries,
                                G_N_ELEMENTS (menu_entries), window);
  gtk_action_group_add_toggle_actions (action_group,
                                       toggle_menu_entries,
                                       G_N_ELEMENTS (toggle_menu_entries),
                                       window);
  gtk_ui_manager_insert_action_group (manager, action_group, 0);
  g_object_unref (action_group);

  action = gtk_action_group_get_action (action_group, "Edit");
  g_signal_connect (action, "activate",
                    G_CALLBACK (edit_menu_activate_callback), window);
  action = gtk_action_group_get_action (action_group, "Terminal");
  g_signal_connect (action, "activate",
                    G_CALLBACK (terminal_menu_activate_callback), window);

  action = gtk_action_group_get_action (action_group, "ViewFullscreen");
  gtk_action_set_sensitive (action,
                            gdk_net_wm_supports (gdk_atom_intern ("_NET_WM_STATE_FULLSCREEN", FALSE)));
  action = gtk_action_group_get_action (action_group, "TerminalSetEncoding");
  gtk_action_set_sensitive (action, terminal_widget_supports_dynamic_encoding ());

  /* Load the UI */
  error = NULL;
  priv->ui_id = gtk_ui_manager_add_ui_from_file (manager,
                                                 TERM_PKGDATADIR "/terminal.ui",
                                                 &error);
  if (error)
    {
      g_printerr ("Failed to load UI: %s\n", error->message);
      g_error_free (error);
    }

  priv->menubar = gtk_ui_manager_get_widget (manager, "/menubar");
  gtk_box_pack_start (GTK_BOX (main_vbox),
		      priv->menubar,
		      FALSE, FALSE, 0);
  gtk_widget_show (priv->menubar);

  /* Add tabs menu */
  priv->tabs_menu = terminal_tabs_menu_new (window);

  terminal_window_reread_profile_list (window);
  
  terminal_window_set_menubar_visible (window, TRUE);
  priv->use_default_menubar_visibility = TRUE;

  /* We have to explicitly call this, since screen-changed is NOT
   * emitted for the toplevel the first time!
   */
  terminal_window_settings_update (GTK_WIDGET (window));
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
  object_class->dispose = terminal_window_dispose;
  object_class->finalize = terminal_window_finalize;

  widget_class->show = terminal_window_show;
  widget_class->window_state_event = terminal_window_state_event;
  widget_class->screen_changed = terminal_window_screen_changed;

  g_type_class_add_private (object_class, sizeof (TerminalWindowPrivate));

  gtk_rc_parse_string ("style \"gnome-terminal-tab-close-button-style\"\n"
                       "{\n"
                          "GtkWidget::focus-padding = 0\n"
                          "GtkWidget::focus-line-width = 0\n"
                          "xthickness = 0\n"
                          "ythickness = 0\n"
                       "}\n"
                       "widget \"*.gnome-terminal-tab-close-button\" style \"gnome-terminal-tab-close-button-style\"");


  gtk_notebook_set_window_creation_hook (handle_tab_droped_on_desktop, NULL, NULL);
}

static void
terminal_window_dispose (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;

  remove_popup_info (window);

  priv->disposed = TRUE;

  if (priv->tabs_menu)
    {
      g_object_unref (priv->tabs_menu);
      priv->tabs_menu = NULL;
    }

  G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}
   
static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;

  g_free (priv->startup_id);

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
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  SnDisplay *sn_display;
  SnLauncheeContext *context;
  GdkScreen *screen;
  GdkDisplay *display;

  if (!GTK_WIDGET_REALIZED (widget))
    gtk_widget_realize (widget);
  
  context = NULL;
  sn_display = NULL;
  if (priv->startup_id != NULL)
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
                                         priv->startup_id);

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
  TerminalWindowPrivate *priv = window->priv;
  gboolean single;

  single = priv->terms == 1;
    
  gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), !single);
}

static void
profile_set_callback (TerminalScreen *screen,
                      TerminalWindow *window)
{
  terminal_window_update_set_profile_menu (window);
  terminal_window_update_new_terminal_menus (window);
}

static void
title_changed_callback (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *menu_item;
  const char *title;
  
  if (screen == priv->active_term)
    {
      title = terminal_screen_get_title (screen);

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
  TerminalWindowPrivate *priv = window->priv;

  if (screen == priv->active_term)
    gdk_window_set_icon_name (GTK_WIDGET (window)->window, terminal_screen_get_icon_title (screen));
}

/* Notebook callbacks */

static void
close_button_clicked_cb (GtkWidget *widget,
                         GtkWidget *screen)
{
  GtkWidget *notebook;
  guint page_num;

  notebook = gtk_widget_get_parent (GTK_WIDGET (screen));
  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), screen);
  gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page_num);
}

static void
sync_tab_label (TerminalScreen *screen,
                GParamSpec *pspec,
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
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 0, 0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);

  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  close_button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_widget_set_name (close_button, "gnome-terminal-tab-close-button");
  gtk_widget_set_tooltip_text (close_button, _("Close tab"));

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (close_button), image);
  gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

  sync_tab_label (screen, NULL, label);
  g_signal_connect (screen, "notify::title",
                    G_CALLBACK (sync_tab_label), label);

  g_signal_connect (close_button, "clicked",
		    G_CALLBACK (close_button_clicked_cb), screen);

  g_signal_connect (hbox, "style-set",
                    G_CALLBACK (tab_label_style_set_cb), close_button);

  gtk_widget_show_all (hbox);

  return hbox;
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            gint            position)
{
  TerminalWindowPrivate *priv = window->priv;
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

  gtk_notebook_insert_page (GTK_NOTEBOOK (priv->notebook),
                            GTK_WIDGET (screen),
                            tab_label,
                            position);
  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (priv->notebook),
                                      GTK_WIDGET (screen),
                                      TRUE, TRUE, GTK_PACK_START);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (priv->notebook),
                                    GTK_WIDGET (screen),
                                    TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (priv->notebook),
                                   GTK_WIDGET (screen),
                                   TRUE);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;
  guint num_page;

  g_return_if_fail (terminal_screen_get_window (screen) == window);

  update_tab_visibility (window, -1);

  num_page = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), GTK_WIDGET (screen));
  gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), num_page);
}

GList*
terminal_window_list_screens (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  /* We are trusting that GtkNotebook will return pages in order */
  return gtk_container_get_children (GTK_CONTAINER (priv->notebook));
}

void
terminal_window_set_menubar_visible (TerminalWindow *window,
                                     gboolean        setting)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;

  /* it's been set now, so don't override when adding a screen.
   * this side effect must happen before we short-circuit below.
   */
  priv->use_default_menubar_visibility = FALSE;
  
  if (setting == priv->menubar_visible)
    return;

  priv->menubar_visible = setting;

  action = gtk_action_group_get_action (priv->action_group, "ViewMenubar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), setting);
  
  g_object_set (priv->menubar, "visible", setting, NULL);

  if (priv->active_term)
    {
#ifdef DEBUG_GEOMETRY
      g_fprintf (stderr,"setting size after toggling menubar visibility\n");
#endif
      terminal_window_set_size (window, priv->active_term, TRUE);
    }
}

gboolean
terminal_window_get_menubar_visible (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  return priv->menubar_visible;
}

GtkWidget *
terminal_window_get_notebook (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
	
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

  return GTK_WIDGET (priv->notebook);
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
  TerminalWindowPrivate *priv = window->priv;
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

/* FIXMEchpe make this also switch tabs! */
void
terminal_window_set_active (TerminalWindow *window,
                            TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  TerminalProfile *profile;
  guint page_num;
  
  if (priv->active_term == screen)
    return;
  
  /* Workaround to remove gtknotebook's feature of computing its size based on
   * all pages. When the widget is hidden, its size will not be taken into
   * account.
   */
  if (priv->active_term)
  {
    GtkWidget *old_widget;
    old_widget = terminal_screen_get_widget (priv->active_term);
    gtk_widget_hide (old_widget);
  }
  
  widget = terminal_screen_get_widget (screen);
  
  /* Make sure that the widget is no longer hidden due to the workaround */
  gtk_widget_show (widget);

  profile = terminal_screen_get_profile (screen);

  if (!GTK_WIDGET_REALIZED (widget))
    gtk_widget_realize (widget); /* we need this for the char width */

  priv->active_term = screen;

  terminal_window_update_geometry (window);
  terminal_window_update_icon (window);
  
  /* Override menubar setting if it wasn't restored from session */
  if (priv->use_default_menubar_visibility)
    {
      gboolean setting =
        terminal_profile_get_default_show_menubar (terminal_screen_get_profile (screen));

      terminal_window_set_menubar_visible (window, setting);
    }

  gdk_window_set_icon_name (GTK_WIDGET (window)->window, terminal_screen_get_icon_title (screen));
  gtk_window_set_title (GTK_WINDOW (window), terminal_screen_get_title (screen));

  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook), GTK_WIDGET (screen));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), page_num);

  /* set size of window to current grid size */
#ifdef DEBUG_GEOMETRY
  g_fprintf (stderr,"setting size after flipping notebook pages\n");
#endif
  terminal_window_set_size (window, screen, TRUE);
  
  terminal_window_update_set_profile_menu (window); /* FIXMEchpe no need to do this, just update the current profile action's active state! */
  terminal_window_update_new_terminal_menus (window);
  terminal_window_update_encoding_menu (window);
  terminal_window_update_copy_sensitivity (window);
  terminal_window_update_zoom_sensitivity (window);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return priv->active_term;
}

static void
notebook_page_selected_callback (GtkWidget       *notebook,
                                 GtkNotebookPage *useless_crap,
                                 guint            page_num,
                                 TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget* page_widget;
  TerminalScreen *screen;
  GtkWidget *menu_item;
  int old_grid_width, old_grid_height;
  GtkWidget *old_widget, *new_widget;

  if (priv->active_term == NULL || priv->disposed)
    return;

  old_widget = terminal_screen_get_widget (priv->active_term);
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
  terminal_window_update_tabs_menu_sensitivity (window);
}

static void
notebook_page_added_callback (GtkWidget       *notebook,
                              TerminalScreen  *screen,
                              guint            page_num,
                              TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *term;

  terminal_screen_set_window (screen, window);
  priv->terms++;

  g_signal_connect (G_OBJECT (screen),
                    "profile-set",
                    G_CALLBACK (profile_set_callback),
                    window);

  /* FIXMEchpe: only connect on the active screen, not all screens! */
  g_signal_connect (G_OBJECT (screen),
                    "notify::title",
                    G_CALLBACK (title_changed_callback),
                    window);

  g_signal_connect (G_OBJECT (screen),
                    "icon-title-changed",
                    G_CALLBACK (icon_title_changed_callback),
                    window);

  g_signal_connect_swapped (G_OBJECT (screen),
                            "selection-changed",
                            G_CALLBACK (terminal_window_update_copy_sensitivity),
                            window);

  g_signal_connect (screen, "show-popup-menu",
                    G_CALLBACK (screen_show_popup_menu_callback), window);

  terminal_screen_update_scrollbar (screen);

  update_notebook (window);

  update_tab_visibility (window, 0);

  term = terminal_screen_get_widget (screen);

  /* ZvtTerm is a broken POS and requires this realize to get
   * the size request right.
   */
  gtk_widget_realize (GTK_WIDGET (term));

  /* If we have an active screen, match its size and zoom */
  if (priv->active_term)
    {
      GtkWidget *widget;
      int current_width, current_height;
      double scale;

      widget = terminal_screen_get_widget (priv->active_term);
      terminal_widget_get_size (widget, &current_width, &current_height);
      terminal_widget_set_size (term, current_width, current_height);

      scale = terminal_screen_get_font_scale (priv->active_term);
      terminal_screen_set_font_scale (screen, scale);
    }
  
  /* Make the first-added screen the active one */
  if (priv->active_term == NULL)
    terminal_window_set_active (window, screen);

  if (priv->present_on_insert)
    {
      gtk_widget_show_all (GTK_WIDGET (window));
      priv->present_on_insert = FALSE;
    }

  terminal_window_update_tabs_menu_sensitivity (window);
}

static void
notebook_page_removed_callback (GtkWidget       *notebook,
                                TerminalScreen  *screen,
                                guint            page_num,
                                TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int pages;

  if (priv->disposed)
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
                                        G_CALLBACK (terminal_window_update_copy_sensitivity),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_show_popup_menu_callback),
                                        window);

  /* FIXMEchpe this should have been done by the parent-set handler already! */
  terminal_screen_set_window (screen, NULL);
  priv->terms--;

  update_notebook (window);

  terminal_window_update_tabs_menu_sensitivity (window);
  update_tab_visibility (window, 0);

  pages = priv->terms;
  if (pages == 1)
    {
      terminal_window_set_size (window, priv->active_term, TRUE);
    }
  else if (pages == 0)
    {
      /* FIXMEchpe!!! DO NOT DO THIS FROM THIS CALLBACK !!!!!!! */
      gtk_widget_destroy (GTK_WIDGET (window));
    }
}

void
terminal_window_update_icon (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalProfile *profile;

  if (priv->active_term == NULL ||
      !(profile = terminal_screen_get_profile (priv->active_term)))
    {
      gtk_window_set_icon (GTK_WINDOW (window), NULL);
      return;
    }

  gtk_window_set_icon (GTK_WINDOW (window),
                       terminal_profile_get_icon (profile));
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GdkGeometry hints;
  GtkWidget *widget;  
  int char_width;
  int char_height;
  
  if (priv->active_term == NULL)
    return;
  
  widget = terminal_screen_get_widget (priv->active_term);
  
  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */
  terminal_widget_get_cell_size (widget, &char_width, &char_height);
  
  if (char_width != priv->old_char_width ||
      char_height != priv->old_char_height ||
      widget != (GtkWidget*) priv->old_geometry_widget)
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
      
      priv->old_char_width = hints.width_inc;
      priv->old_char_height = hints.height_inc;
      priv->old_geometry_widget = widget;
    }
#ifdef DEBUG_GEOMETRY
  else
    {
      g_fprintf (stderr,"hints: increment unchanged, not setting\n");
    }
#endif
}

static void
file_new_window_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalProfile *profile;

  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  if (!profile)
    profile = terminal_profile_get_default ();
  if (!profile)
    return;

  if (terminal_profile_get_forgotten (profile))
    return;

  new_window (window, NULL, profile);
}

static void
new_window (TerminalWindow *window,
            TerminalScreen *screen,
            TerminalProfile *profile)
{
  TerminalWindowPrivate *priv = window->priv;
  char *display_name, *geometry;
  const char *dir;

  display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (window)));

  dir = terminal_screen_get_working_dir (priv->active_term);

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
                             NULL, display_name, -1);

  g_free (geometry);
  g_free (display_name);
}

static void
file_new_tab_callback (GtkAction *action,
                       TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalProfile *profile;
  const char *dir;

  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  if (!profile)
    profile = terminal_profile_get_default ();
  if (!profile)
    return;

  if (terminal_profile_get_forgotten (profile))
    return;
      
  dir = terminal_screen_get_working_dir (priv->active_term);

  terminal_app_new_terminal (terminal_app_get (),
                             profile,
                             window,
                             NULL,
                             FALSE, FALSE, FALSE,
                             NULL, NULL, NULL, dir, NULL, 1.0,
                             NULL, NULL, -1);
}

static gboolean
confirm_close_window (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *dialog;
  GConfClient *client;
  gboolean result, do_confirm;
  int n;

  n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));

  if (n <= 1)
    return TRUE;

  client = gconf_client_get_default ();
  do_confirm = gconf_client_get_bool (client, CONF_GLOBAL_PREFIX "/confirm_window_close", NULL);
  g_object_unref (client);
  if (!do_confirm)
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
file_close_window_callback (GtkAction *action,
                            TerminalWindow *window)
{
  if (confirm_close_window (window))
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
file_close_tab_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  if (priv->active_term)
    terminal_screen_close (priv->active_term);
}

static void
edit_copy_callback (GtkAction *action,
                    TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;

  if (!priv->active_term)
    return;
      
  widget = terminal_screen_get_widget (priv->active_term);
      
  terminal_widget_copy_clipboard (widget);
}

static void
edit_paste_callback (GtkAction *action,
                     TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;

  if (!priv->active_term)
    return;
      
  widget = terminal_screen_get_widget (priv->active_term);

  terminal_widget_paste_clipboard (widget);
}

static void
edit_keybindings_callback (GtkAction *action,
                           TerminalWindow *window)
{
  terminal_app_edit_keybindings (terminal_app_get (),
                                 GTK_WINDOW (window));
}

static void
edit_current_profile_callback (GtkAction *action,
                               TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_edit_profile (terminal_app_get (),
                             terminal_screen_get_profile (priv->active_term),
                             GTK_WINDOW (window));
}

static void
file_new_profile_callback (GtkAction *action,
                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_new_profile (terminal_app_get (),
                            terminal_screen_get_profile (priv->active_term),
                            GTK_WINDOW (window));
}

static void
edit_profiles_callback (GtkAction *action,
                        TerminalWindow *window)
{
  terminal_app_manage_profiles (terminal_app_get (),
                                GTK_WINDOW (window));
}

static void
view_menubar_toggled_callback (GtkToggleAction *action,
                               TerminalWindow *window)
{
  terminal_window_set_menubar_visible (window, gtk_toggle_action_get_active (action));
}

static void
view_fullscreen_toggled_callback (GtkToggleAction *action,
                                 TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  g_return_if_fail (GTK_WIDGET_REALIZED (window));

  if (gtk_toggle_action_get_active (action))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

static const double zoom_factors[] = {
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
view_zoom_in_callback (GtkAction *action,
                       TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  double current;
  
  if (priv->active_term == NULL)
    return;
  
  current = terminal_screen_get_font_scale (priv->active_term);

  /* FIXMEchpe! this should be unnecessary! */
  if (find_larger_zoom_factor (current, &current))
    {
      terminal_screen_set_font_scale (priv->active_term, current);
      terminal_window_update_zoom_sensitivity (window);
    }
}

static void
view_zoom_out_callback (GtkAction *action,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  double current;

  if (priv->active_term == NULL)
    return;
  
  current = terminal_screen_get_font_scale (priv->active_term);

  /* FIXMEchpe! this should be unnecessary! */
  if (find_smaller_zoom_factor (current, &current))
    {
      terminal_screen_set_font_scale (priv->active_term, current);
      terminal_window_update_zoom_sensitivity (window);
    }
}

static void
view_zoom_normal_callback (GtkAction *action,
                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  if (priv->active_term == NULL)
    return;

  terminal_screen_set_font_scale (priv->active_term, PANGO_SCALE_MEDIUM);
  terminal_window_update_zoom_sensitivity (window);
}

static void
terminal_set_title_callback (GtkAction *action,
                             TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  if (priv->active_term == NULL)
    return;
    
  terminal_screen_edit_title (priv->active_term,
                              GTK_WINDOW (window));
}

static void
terminal_add_encoding_callback (GtkAction *action,
                                TerminalWindow *window)
{
  terminal_app_edit_encodings (terminal_app_get (),
                               GTK_WINDOW (window));
}

static void
terminal_reset_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;

  if (priv->active_term == NULL)
    return;
      
      
  widget = terminal_screen_get_widget (priv->active_term);

  terminal_widget_reset (widget, FALSE);
}

static void
terminal_reset_clear_callback (GtkAction *action,
                               TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;

  if (priv->active_term == NULL)
    return;
      
  widget = terminal_screen_get_widget (priv->active_term);

  terminal_widget_reset (widget, TRUE);
}

static void
tabs_next_tab_callback (GtkAction *action,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  gtk_notebook_next_page (GTK_NOTEBOOK (priv->notebook));
}

static void
tabs_previous_tab_callback (GtkAction *action,
                            TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  gtk_notebook_prev_page (GTK_NOTEBOOK (priv->notebook));
}

static void
tabs_move_left_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (priv->notebook);
  gint page_num,last_page;
  GtkWidget *page; 

  page_num = gtk_notebook_get_current_page (notebook);
  last_page = gtk_notebook_get_n_pages (notebook) - 1;
  page = gtk_notebook_get_nth_page (notebook, page_num);

  gtk_notebook_reorder_child (notebook, page, page_num == 0 ? last_page : page_num - 1);
}

static void
tabs_move_right_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (priv->notebook);
  gint page_num,last_page;
  GtkWidget *page; 

  page_num = gtk_notebook_get_current_page (notebook);
  last_page = gtk_notebook_get_n_pages (notebook) - 1;
  page = gtk_notebook_get_nth_page (notebook, page_num);
  
  gtk_notebook_reorder_child (notebook, page, page_num == last_page ? 0 : page_num + 1);
}

/* FIXMEchpe this is bogus bogus! */
static void
detach_tab (TerminalScreen  *screen,
            TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalProfile *profile;

  profile = terminal_screen_get_profile (screen);

  g_assert (profile);

  if (terminal_profile_get_forgotten (profile))
    return;
      
  new_window (window, screen, profile);
}

static void
tabs_detach_tab_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (priv->notebook);
  gint page_num;
  GtkWidget *page; 

  page_num = gtk_notebook_get_current_page (notebook);
  page = gtk_notebook_get_nth_page (notebook, page_num);
  
  detach_tab (TERMINAL_SCREEN (page), window);
}

static void
help_contents_callback (GtkAction *action,
                        TerminalWindow *window)
{
  terminal_util_show_help (NULL, GTK_WINDOW (window));
}

static void
help_about_callback (GtkAction *action,
                     TerminalWindow *window)
{
  static const char copyright[] =
    "Copyright © 2002, 2003, 2004 Havoc Pennington\n"
    "Copyright © 2003, 2004, 2007 Mariano Suárez-Alvarez\n"
    "Copyright © 2006 Guilherme de S. Pastore\n"
    "Copyright © 2007, 2008 Christian Persch";
  const char *authors[] = {
    "Guilherme de S. Pastore <gpastore@gnome.org> (maintainer)",
    "Havoc Pennington <hp@redhat.com>",
    "Mariano Suárez-Alvarez <mariano@gnome.org>",
    "Christian Persch <chpe" "\100" "gnome" "." "org" ">",
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
			 "program-name", _("GNOME Terminal"),
			 "copyright", copyright,
			 "comments", _("A terminal emulator for the GNOME desktop"),
			 "version", VERSION,
			 "authors", authors,
			 "license", license_text,
			 "wrap-license", TRUE,
			 "translator-credits", _("translator-credits"),
			 "logo-icon-name", "gnome-terminal",
			 NULL);
  g_free (license_text);
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
  TerminalWindowPrivate *priv = window->priv;
  
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
  TerminalWindowPrivate *priv = window->priv;
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
  TerminalWindowPrivate *priv = window->priv;
  
  monitor_profiles_for_is_default_change (window);
  
  terminal_window_update_set_profile_menu (window);
  terminal_window_update_new_terminal_menus (window);
}

void
terminal_window_set_startup_id (TerminalWindow *window,
                                const char     *startup_id)
{
  TerminalWindowPrivate *priv = window->priv;

  g_free (priv->startup_id);
  priv->startup_id = g_strdup (startup_id);
}

GtkUIManager *
terminal_window_get_ui_manager (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return priv->ui_manager;
}
