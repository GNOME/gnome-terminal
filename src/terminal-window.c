/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2007, 2008, 2009, 2011 Christian Persch
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
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-enums.h"
#include "terminal-encoding.h"
#include "terminal-intl.h"
#include "terminal-mdi-container.h"
#include "terminal-notebook.h"
#include "terminal-schemas.h"
#include "terminal-screen-container.h"
#include "terminal-search-dialog.h"
#include "terminal-tab-label.h"
#include "terminal-tabs-menu.h"
#include "terminal-util.h"
#include "terminal-window.h"

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
  TerminalMdiContainer *mdi_container;
  TerminalScreen *active_screen;
  int old_char_width;
  int old_char_height;
  void *old_geometry_widget; /* only used for pointer value as it may be freed */

  GtkWidget *confirm_close_dialog;
  GtkWidget *search_find_dialog;

  guint menubar_visible : 1;
  guint use_default_menubar_visibility : 1;

  /* Used to clear stray "demands attention" flashing on our window when we
   * unmap and map it to switch to an ARGB visual.
   */
  guint clear_demands_attention : 1;

  guint disposed : 1;
  guint present_on_insert : 1;

  /* Workaround until gtk+ bug #535557 is fixed */
  guint icon_title_set : 1;
};

#define PROFILE_DATA_KEY "GT::Profile"

#define FILE_NEW_TERMINAL_TAB_UI_PATH     "/menubar/File/FileNewTabProfiles"
#define FILE_NEW_TERMINAL_WINDOW_UI_PATH  "/menubar/File/FileNewWindowProfiles"
#define SET_ENCODING_UI_PATH              "/menubar/Terminal/TerminalSetEncoding/EncodingsPH"
#define SET_ENCODING_ACTION_NAME_PREFIX   "TerminalSetEncoding"

#define PROFILES_UI_PATH        "/menubar/Terminal/TerminalProfiles"
#define PROFILES_POPUP_UI_PATH  "/Popup/PopupTerminalProfiles/ProfilesPH"

#define SIZE_TO_UI_PATH            "/menubar/Terminal/TerminalSizeToPH"
#define SIZE_TO_ACTION_NAME_PREFIX "TerminalSizeTo"

#define STOCK_NEW_WINDOW  "window-new"
#define STOCK_NEW_TAB     "tab-new"

#define ENCODING_DATA_KEY "encoding"

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

static void terminal_window_dispose     (GObject             *object);
static void terminal_window_finalize    (GObject             *object);
static gboolean terminal_window_state_event (GtkWidget            *widget,
                                             GdkEventWindowState  *event);

static gboolean terminal_window_delete_event (GtkWidget *widget,
                                              GdkEvent *event,
                                              gpointer data);

static gboolean notebook_button_press_cb     (GtkWidget *notebook,
                                              GdkEventButton *event,
                                              TerminalWindow *window);
static gboolean notebook_popup_menu_cb       (GtkWidget *notebook,
                                              TerminalWindow *window);
static void mdi_screen_switched_cb (TerminalMdiContainer *container,
                                    TerminalScreen *old_active_screen,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screen_added_cb    (TerminalMdiContainer *container,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screen_removed_cb  (TerminalMdiContainer *container,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void screen_close_request_cb (TerminalMdiContainer *container,
                                     TerminalScreen *screen,
                                     TerminalWindow *window);

/* Menu action callbacks */
static void file_new_window_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void file_new_tab_callback             (GtkAction *action,
                                               TerminalWindow *window);
static void file_new_profile_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void file_close_window_callback        (GtkAction *action,
                                               TerminalWindow *window);
static void file_save_contents_callback       (GtkAction *action,
                                               TerminalWindow *window);
static void file_close_tab_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void edit_copy_callback                (GtkAction *action,
                                               TerminalWindow *window);
static void edit_paste_callback               (GtkAction *action,
                                               TerminalWindow *window);
static void edit_select_all_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void edit_preferences_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void edit_current_profile_callback     (GtkAction *action,
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
static void search_find_callback              (GtkAction *action,
                                               TerminalWindow *window);
static void search_find_next_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void search_find_prev_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void search_clear_highlight_callback   (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_set_title_callback       (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_add_encoding_callback    (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_reset_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_reset_clear_callback     (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_next_or_previous_tab_cb      (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_move_left_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_move_right_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_detach_tab_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void help_contents_callback        (GtkAction *action,
                                           TerminalWindow *window);
static void help_about_callback           (GtkAction *action,
                                           TerminalWindow *window);

static gboolean find_larger_zoom_factor  (double  current,
                                          double *found);
static gboolean find_smaller_zoom_factor (double  current,
                                          double *found);

static void terminal_window_show (GtkWidget *widget);

static gboolean confirm_close_window_or_tab (TerminalWindow *window,
                                             TerminalScreen *screen);

static void
profile_set_callback (TerminalScreen *screen,
                      GSettings *old_profile,
                      TerminalWindow *window);
static void
sync_screen_icon_title (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window);

static void terminal_window_update_size (TerminalWindow *window);

G_DEFINE_TYPE (TerminalWindow, terminal_window, GTK_TYPE_APPLICATION_WINDOW)

/* Menubar mnemonics & accel settings handling */

static void
enable_menubar_accel_changed_cb (GSettings *settings,
                                 const char *key,
                                 GtkSettings *gtk_settings)
{
  /* const */ char *saved_menubar_accel;

  /* FIXME: Once gtk+ bug 507398 is fixed, use that to reset the property instead */
  /* Now this is a bad hack on so many levels. */
  saved_menubar_accel = g_object_get_data (G_OBJECT (gtk_settings), "GT::gtk-menu-bar-accel");

  if (g_settings_get_boolean (settings, key))
    g_object_set (gtk_settings, "gtk-menu-bar-accel", saved_menubar_accel, NULL);
  else
    g_object_set (gtk_settings, "gtk-menu-bar-accel", NULL, NULL);
}

static void
app_setting_notify_destroy_cb (GtkSettings *gtk_settings)
{
  g_signal_handlers_disconnect_by_func (terminal_app_get_global_settings (terminal_app_get ()),
                                        G_CALLBACK (enable_menubar_accel_changed_cb),
                                        gtk_settings);
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

static int
find_tab_num_at_pos (GtkNotebook *notebook,
                     int screen_x, 
                     int screen_y)
{
  GtkPositionType tab_pos;
  int page_num = 0;
  GtkNotebook *nb = GTK_NOTEBOOK (notebook);
  GtkWidget *page;
  GtkAllocation tab_allocation;

  tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

  while ((page = gtk_notebook_get_nth_page (nb, page_num)))
    {
      GtkWidget *tab;
      int max_x, max_y, x_root, y_root;

      tab = gtk_notebook_get_tab_label (nb, page);
      g_return_val_if_fail (tab != NULL, -1);

      if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
        {
          page_num++;
          continue;
        }

      gdk_window_get_origin (gtk_widget_get_window (tab), &x_root, &y_root);

      gtk_widget_get_allocation (tab, &tab_allocation);
      max_x = x_root + tab_allocation.x + tab_allocation.width;
      max_y = y_root + tab_allocation.y + tab_allocation.height;

      if ((tab_pos == GTK_POS_TOP || tab_pos == GTK_POS_BOTTOM) && screen_x <= max_x)
        return page_num;

      if ((tab_pos == GTK_POS_LEFT || tab_pos == GTK_POS_RIGHT) && screen_y <= max_y)
        return page_num;

      page_num++;
    }

  return -1;
}

static void
position_menu_under_widget (GtkMenu *menu,
                            int *x,
                            int *y,
                            gboolean *push_in,
                            gpointer user_data)
{
  /* Adapted from gtktoolbar.c */
  GtkWidget *widget = GTK_WIDGET (user_data);
  GdkWindow *widget_window;
  GtkWidget *container;
  GtkRequisition req;
  GtkRequisition menu_req;
  GdkRectangle monitor;
  int monitor_num;
  GdkScreen *screen;
  GtkAllocation widget_allocation;

  widget_window = gtk_widget_get_window (widget);
  gtk_widget_get_allocation (widget, &widget_allocation);
  container = gtk_widget_get_ancestor (widget, GTK_TYPE_CONTAINER);

  gtk_widget_get_preferred_size (widget, NULL, &req);
  gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, &menu_req);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, widget_window);
  if (monitor_num < 0)
          monitor_num = 0;
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  gdk_window_get_origin (widget_window, x, y);
  if (!gtk_widget_get_has_window (widget))
    {
      *x += widget_allocation.x;
      *y += widget_allocation.y;
    }
  if (gtk_widget_get_direction (container) == GTK_TEXT_DIR_LTR) 
    *x += widget_allocation.width - req.width;
  else 
    *x += req.width - menu_req.width;

  if ((*y + widget_allocation.height + menu_req.height) <= monitor.y + monitor.height)
    *y += widget_allocation.height;
  else if ((*y - menu_req.height) >= monitor.y)
    *y -= menu_req.height;
  else if (monitor.y + monitor.height - (*y + widget_allocation.height) > *y)
    *y += widget_allocation.height;
  else
    *y -= menu_req.height;

  *push_in = FALSE;
}

static void
terminal_set_profile_toggled_callback (GtkToggleAction *action,
                                       TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *profile;

  if (!gtk_toggle_action_get_active (action))
    return;

  if (priv->active_screen == NULL)
    return;

  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  g_assert (profile);

  g_signal_handlers_block_by_func (priv->active_screen, G_CALLBACK (profile_set_callback), window);
  terminal_screen_set_profile (priv->active_screen, profile);
  g_signal_handlers_unblock_by_func (priv->active_screen, G_CALLBACK (profile_set_callback), window);
}

static void
profile_visible_name_notify_cb (GSettings  *profile,
                                const char *key,
                                GtkAction  *action)
{
  const char *visible_name;
  char *dot, *display_name;
  guint num;

  g_settings_get (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, "&s", &visible_name);
  display_name = escape_underscores (visible_name);

  dot = strchr (gtk_action_get_name (action), '.');
  if (dot != NULL)
    {
      char *free_me;

      num = g_ascii_strtoll (dot + 1, NULL, 10);

      free_me = display_name;
      if (num < 10)
        /* Translators: This is the label of a menu item to choose a profile.
         * _%d is used as the accelerator (with d between 1 and 9), and
         * the %s is the name of the terminal profile.
         */
        display_name = g_strdup_printf (_("_%d. %s"), num, display_name);
      else if (num < 36)
        /* Translators: This is the label of a menu item to choose a profile.
         * _%c is used as the accelerator (it will be a character between A and Z),
         * and the %s is the name of the terminal profile.
         */
        display_name = g_strdup_printf (_("_%c. %s"), ('A' + num - 10), display_name);
      else
        free_me = NULL;

      g_free (free_me);
    }

  g_object_set (action, "label", display_name, NULL);
  g_free (display_name);
}

static void
disconnect_profiles_from_actions_in_group (GtkActionGroup *action_group)
{
  GList *actions, *l;

  actions = gtk_action_group_list_actions (action_group);
  for (l = actions; l != NULL; l = l->next)
    {
      GObject *action = G_OBJECT (l->data);
      GSettings *profile;

      profile = g_object_get_data (action, PROFILE_DATA_KEY);
      if (!profile)
        continue;

      g_signal_handlers_disconnect_by_func (profile, G_CALLBACK (profile_visible_name_notify_cb), action);
    }
  g_list_free (actions);
}

static void
terminal_window_update_set_profile_menu_active_profile (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *new_active_profile;
  GList *actions, *l;

  if (!priv->profiles_action_group)
    return;

  if (!priv->active_screen)
    return;

  new_active_profile = terminal_screen_get_profile (priv->active_screen);

  actions = gtk_action_group_list_actions (priv->profiles_action_group);
  for (l = actions; l != NULL; l = l->next)
    {
      GObject *action = G_OBJECT (l->data);
      GSettings *profile;

      profile = g_object_get_data (action, PROFILE_DATA_KEY);
      if (profile != new_active_profile)
        continue;

      g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_set_profile_toggled_callback), window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
      g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_set_profile_toggled_callback), window);

      break;
    }
  g_list_free (actions);
}

static void
terminal_window_update_set_profile_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *active_profile;
  GtkActionGroup *action_group;
  GtkAction *action;
  TerminalSettingsList *profiles_list;
  GList *profiles, *p;
  GSList *group;
  guint n;
  gboolean single_profile;

  /* Remove the old UI */
  if (priv->profiles_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->profiles_ui_id);
      priv->profiles_ui_id = 0;
    }

  if (priv->profiles_action_group != NULL)
    {
      disconnect_profiles_from_actions_in_group (priv->profiles_action_group);
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->profiles_action_group);
      priv->profiles_action_group = NULL;
    }

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  profiles = terminal_profiles_list_ref_children (profiles_list);

  action = gtk_action_group_get_action (priv->action_group, "TerminalProfiles");
  single_profile = !profiles || profiles->next == NULL; /* list length <= 1 */
  gtk_action_set_sensitive (action, !single_profile);

  if (profiles == NULL)
    return;

  if (priv->active_screen)
    active_profile = terminal_screen_get_profile (priv->active_screen);
  else
    active_profile = NULL;

  action_group = priv->profiles_action_group = gtk_action_group_new ("Profiles");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->profiles_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  group = NULL;
  n = 0;
  for (p = profiles; p != NULL; p = p->next)
    {
      GSettings *profile = (GSettings *) p->data;
      GtkRadioAction *profile_action;
      char name[32];

      g_snprintf (name, sizeof (name), "TerminalSetProfile%u", n++);

      profile_action = gtk_radio_action_new (name,
                                             NULL,
                                             NULL,
                                             NULL,
                                             n);

      gtk_radio_action_set_group (profile_action, group);
      group = gtk_radio_action_get_group (profile_action);

      if (profile == active_profile)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (profile_action), TRUE);

      g_object_set_data_full (G_OBJECT (profile_action),
                              PROFILE_DATA_KEY,
                              g_object_ref (profile),
                              (GDestroyNotify) g_object_unref);
      profile_visible_name_notify_cb (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, GTK_ACTION (profile_action));
      g_signal_connect (profile, "changed::" TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                        G_CALLBACK (profile_visible_name_notify_cb), profile_action);
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

  g_list_free_full (profiles, (GDestroyNotify) g_object_unref);
}

static void
terminal_window_create_new_terminal_action (TerminalWindow *window,
                                            GSettings *profile,
                                            const char *name,
                                            guint num,
                                            GCallback callback)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;

  action = gtk_action_new (name, NULL, NULL, NULL);

  g_object_set_data_full (G_OBJECT (action),
                          PROFILE_DATA_KEY,
                          g_object_ref (profile),
                          (GDestroyNotify) g_object_unref);
  profile_visible_name_notify_cb (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, action);
  g_signal_connect (profile, "changed::" TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                    G_CALLBACK (profile_visible_name_notify_cb), action);
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
  TerminalSettingsList *profiles_list;
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
      disconnect_profiles_from_actions_in_group (priv->new_terminal_action_group);
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->new_terminal_action_group);
      priv->new_terminal_action_group = NULL;
    }

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  profiles = terminal_profiles_list_ref_children (profiles_list);

  have_single_profile = !profiles || !profiles->next;

  action = gtk_action_group_get_action (priv->action_group, "FileNewTab");
  gtk_action_set_visible (action, have_single_profile);
  action = gtk_action_group_get_action (priv->action_group, "FileNewWindow");
  gtk_action_set_visible (action, have_single_profile);

  if (have_single_profile)
    {
      g_list_free_full (profiles, (GDestroyNotify) g_object_unref);
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
      GSettings *profile = (GSettings *) p->data;
      char name[32];

      g_snprintf (name, sizeof (name), "FileNewTab.%u", n);
      terminal_window_create_new_terminal_action (window,
                                                  profile,
                                                  name,
                                                  n,
                                                  G_CALLBACK (file_new_tab_callback));

      gtk_ui_manager_add_ui (priv->ui_manager, priv->new_terminal_ui_id,
                             FILE_NEW_TERMINAL_TAB_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);

      g_snprintf (name, sizeof (name), "FileNewWindow.%u", n);
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

  g_list_free_full (profiles, (GDestroyNotify) g_object_unref);
}

static void
terminal_set_encoding_callback (GtkToggleAction *action,
                                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalEncoding *encoding;
  
  if (!gtk_toggle_action_get_active (action))
    return;

  if (priv->active_screen == NULL)
    return;

  encoding = g_object_get_data (G_OBJECT (action), ENCODING_DATA_KEY);
  g_assert (encoding);

  vte_terminal_set_encoding (VTE_TERMINAL (priv->active_screen),
                             terminal_encoding_get_charset (encoding));
}

static void
terminal_window_update_encoding_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  GtkActionGroup *action_group;
  GSList *group;
  guint n;
  GSList *encodings, *l;
  const char *charset;
  TerminalEncoding *active_encoding;

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

  action_group = priv->encodings_action_group = gtk_action_group_new ("Encodings");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->encodings_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  if (priv->active_screen)
    charset = vte_terminal_get_encoding (VTE_TERMINAL (priv->active_screen));
  else
    charset = "current";

  app = terminal_app_get ();
  active_encoding = terminal_app_ensure_encoding (app, charset);

  encodings = terminal_app_get_active_encodings (app);

  if (g_slist_find (encodings, active_encoding) == NULL)
    encodings = g_slist_append (encodings, terminal_encoding_ref (active_encoding));

  group = NULL;
  n = 0;
  for (l = encodings; l != NULL; l = l->next)
    {
      TerminalEncoding *e = (TerminalEncoding *) l->data;
      GtkRadioAction *encoding_action;
      char name[128];
      char *display_name;
      
      g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s", terminal_encoding_get_id (e));
      display_name = g_strdup_printf ("%s (%s)", e->name, terminal_encoding_get_charset (e));

      encoding_action = gtk_radio_action_new (name,
                                              display_name,
                                              NULL,
                                              NULL,
                                              n);
      g_free (display_name);

      gtk_radio_action_set_group (encoding_action, group);
      group = gtk_radio_action_get_group (encoding_action);

      if (charset && strcmp (terminal_encoding_get_id (e), charset) == 0)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (encoding_action), TRUE);

      g_signal_connect (encoding_action, "toggled",
                        G_CALLBACK (terminal_set_encoding_callback), window);

      g_object_set_data_full (G_OBJECT (encoding_action), ENCODING_DATA_KEY,
                              terminal_encoding_ref (e),
                              (GDestroyNotify) terminal_encoding_unref);

      gtk_action_group_add_action (action_group, GTK_ACTION (encoding_action));
      g_object_unref (encoding_action);

      gtk_ui_manager_add_ui (priv->ui_manager, priv->encodings_ui_id,
                             SET_ENCODING_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  g_slist_foreach (encodings, (GFunc) terminal_encoding_unref, NULL);
  g_slist_free (encodings);
}

static void
terminal_window_update_encoding_menu_active_encoding (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  char name[128];

  if (!priv->active_screen)
    return;
  if (!priv->encodings_action_group)
    return;

  g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s",
              vte_terminal_get_encoding (VTE_TERMINAL (priv->active_screen)));
  action = gtk_action_group_get_action (priv->encodings_action_group, name);
  if (!action)
    return;

  g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_set_encoding_callback), window);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
  g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_set_encoding_callback), window);
}

static void
terminal_size_to_cb (GtkAction *action,
                     TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *name;
  char *end = NULL;
  guint width, height;

  if (priv->active_screen == NULL)
    return;

  name = gtk_action_get_name (action) + strlen (SIZE_TO_ACTION_NAME_PREFIX);
  width = g_ascii_strtoull (name, &end, 10);
  g_assert (end && *end == 'x');
  height = g_ascii_strtoull (end + 1, &end, 10);
  g_assert (end && *end == '\0');

  vte_terminal_set_size (VTE_TERMINAL (priv->active_screen), width, height);

  terminal_window_update_size (window);
}

static void
terminal_window_update_size_to_menu (TerminalWindow *window)
{
  static const struct {
    guint grid_width;
    guint grid_height;
  } predefined_sizes[] = {
    { 80, 24 },
    { 80, 43 },
    { 132, 24 },
    { 132, 43 }
  };
  TerminalWindowPrivate *priv = window->priv;
  guint i;

  /* We only install this once, so there's no need for a separate action group
   * and any cleanup + build-new-one action here.
   */

  for (i = 0; i < G_N_ELEMENTS (predefined_sizes); ++i)
    {
      guint grid_width = predefined_sizes[i].grid_width;
      guint grid_height = predefined_sizes[i].grid_height;
      GtkAction *action;
      char name[40];
      char *display_name;
      
      g_snprintf (name, sizeof (name), SIZE_TO_ACTION_NAME_PREFIX "%ux%u",
                  grid_width, grid_height);

      /* If there are ever more than 9 of these, extend this to use A..Z as mnemonics,
       * like we do for the profiles menu.
       */
      display_name = g_strdup_printf ("_%u. %ux%u", i + 1, grid_width, grid_height);

      action = gtk_action_new (name, display_name, NULL, NULL);
      g_free (display_name);

      g_signal_connect (action, "activate",
                        G_CALLBACK (terminal_size_to_cb), window);

      gtk_action_group_add_action (priv->action_group, action);
      g_object_unref (action);

      gtk_ui_manager_add_ui (priv->ui_manager, priv->ui_id,
                             SIZE_TO_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }
}

/* Actions stuff */

static void
terminal_window_update_copy_sensitivity (TerminalScreen *screen,
                                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean can_copy;

  if (screen != priv->active_screen)
    return;

  can_copy = vte_terminal_get_has_selection (VTE_TERMINAL (screen));

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
  
  screen = priv->active_screen;
  if (screen == NULL)
    return;

  current = terminal_screen_get_font_scale (screen);

  action = gtk_action_group_get_action (priv->action_group, "ViewZoomOut");
  gtk_action_set_sensitive (action, find_smaller_zoom_factor (current, &zoom));
  action = gtk_action_group_get_action (priv->action_group, "ViewZoomIn");
  gtk_action_set_sensitive (action, find_larger_zoom_factor (current, &zoom));
}

static void
terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean can_search;

  if (screen != priv->active_screen)
    return;

  can_search = vte_terminal_search_get_gregex (VTE_TERMINAL (screen)) != NULL;

  action = gtk_action_group_get_action (priv->action_group, "SearchFindNext");
  gtk_action_set_sensitive (action, can_search);
  action = gtk_action_group_get_action (priv->action_group, "SearchFindPrevious");
  gtk_action_set_sensitive (action, can_search);
  action = gtk_action_group_get_action (priv->action_group, "SearchClearHighlight");
  gtk_action_set_sensitive (action, can_search);
}

static void
update_edit_menu_cb (GtkClipboard *clipboard,
                     GdkAtom *targets,
                     int n_targets,
                     TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean can_paste, can_paste_uris;

  can_paste = targets != NULL && gtk_targets_include_text (targets, n_targets);
  can_paste_uris = targets != NULL && gtk_targets_include_uri (targets, n_targets);

  action = gtk_action_group_get_action (priv->action_group, "EditPaste");
  gtk_action_set_sensitive (action, can_paste);
  action = gtk_action_group_get_action (priv->action_group, "EditPasteURIPaths");
  gtk_action_set_visible (action, can_paste_uris);
  gtk_action_set_sensitive (action, can_paste_uris);

  /* Ref was added in gtk_clipboard_request_targets below */
  g_object_unref (window);
}

static void
update_edit_menu (GtkClipboard *clipboard,
                  GdkEvent *event G_GNUC_UNUSED,
                  TerminalWindow *window)
{
  gtk_clipboard_request_targets (clipboard,
                                 (GtkClipboardTargetsReceivedFunc) update_edit_menu_cb,
                                 g_object_ref (window));
}

static void
screen_resize_window_cb (TerminalScreen *screen,
                         guint width,
                         guint height,
                         TerminalWindow* window)
{
  TerminalWindowPrivate *priv = window->priv;
  VteTerminal *terminal = VTE_TERMINAL (screen);
  GtkWidget *widget = GTK_WIDGET (screen);
  guint grid_width, grid_height;
  int char_width, char_height;
  GtkBorder *inner_border = NULL;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);
  /* Don't do anything if we're maximised or fullscreened */
  // FIXME: realized && ... instead? 
  if (!gtk_widget_get_realized (widget) ||
      (gdk_window_get_state (gtk_widget_get_window (widget)) & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) != 0)
    return;

  /* NOTE: width and height already include the VteTerminal's padding! */

  /* Short-circuit */
  if (((int) width) == widget_allocation.width &&
      ((int) height) == widget_allocation.height)
    return;

  /* The resize-window signal sucks. Re-compute grid widths */

  char_width = vte_terminal_get_char_width (terminal);
  char_height = vte_terminal_get_char_height (terminal);

  gtk_widget_style_get (GTK_WIDGET (terminal), "inner-border", &inner_border, NULL);
  grid_width = (width - (inner_border ? (inner_border->left + inner_border->right) : 0)) / char_width;
  grid_height = (height - (inner_border ? (inner_border->top + inner_border->bottom) : 0)) / char_height;
  gtk_border_free (inner_border);

  vte_terminal_set_size (terminal, grid_width, grid_height);

  if (screen == priv->active_screen)
    terminal_window_update_size (window);
}

static void
terminal_window_update_tabs_menu_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkActionGroup *action_group = priv->action_group;
  GtkAction *action;
  int num_pages, page_num;
  gboolean not_first, not_last;

  if (priv->disposed)
    return;

  num_pages = terminal_mdi_container_get_n_screens (priv->mdi_container);
  page_num = terminal_mdi_container_get_active_screen_num (priv->mdi_container);
  not_first = page_num > 0;
  not_last = page_num + 1 < num_pages;

  /* Hide the tabs menu in single-tab windows */
  action = gtk_action_group_get_action (action_group, "Tabs");
  gtk_action_set_visible (action, num_pages > 1);
  
#if 1
  /* NOTE: We always make next/prev actions sensitive except in
   * single-tab windows, so the corresponding shortcut key escape code
   * isn't sent to the terminal. See bug #453193 and bug #138609.
   * This also makes tab cycling work, bug #92139.
   * FIXME: Find a better way to do this.
   */
  action = gtk_action_group_get_action (action_group, "TabsPrevious");
  gtk_action_set_sensitive (action, num_pages > 1);
  action = gtk_action_group_get_action (action_group, "TabsNext");
  gtk_action_set_sensitive (action, num_pages > 1);
#else
  /* This would be correct, but see the comment above. */
  action = gtk_action_group_get_action (action_group, "TabsPrevious");
  gtk_action_set_sensitive (action, not_first);
  action = gtk_action_group_get_action (action_group, "TabsNext");
  gtk_action_set_sensitive (action, not_last);
#endif

  action = gtk_action_group_get_action (action_group, "TabsMoveLeft");
  gtk_action_set_sensitive (action, not_first);
  action = gtk_action_group_get_action (action_group, "TabsMoveRight");
  gtk_action_set_sensitive (action, not_last);
  action = gtk_action_group_get_action (action_group, "TabsDetach");
  gtk_action_set_sensitive (action, num_pages > 1);
  action = gtk_action_group_get_action (action_group, "FileCloseTab");
  gtk_action_set_sensitive (action, num_pages > 1);
}

static GtkNotebook *
handle_tab_droped_on_desktop (GtkNotebook *source_notebook,
                              GtkWidget   *container,
                              gint         x,
                              gint         y,
                              gpointer     data G_GNUC_UNUSED)
{
  TerminalWindow *source_window;
  TerminalWindow *new_window;
  TerminalWindowPrivate *new_priv;

  source_window = TERMINAL_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (source_notebook)));
  g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), NULL);

  new_window = terminal_app_new_window (terminal_app_get (),
                                        gtk_widget_get_screen (GTK_WIDGET (source_window)));
  new_priv = new_window->priv;
  new_priv->present_on_insert = TRUE;

//   update_tab_visibility (source_window, -1);
//   update_tab_visibility (new_window, +1);

  return GTK_NOTEBOOK (new_priv->mdi_container);
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

  terminal_util_open_url (GTK_WIDGET (window), info->string, info->flavour,
                          gtk_get_current_event_time ());
}

static void
popup_copy_url_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;
  GtkClipboard *clipboard;

  if (info == NULL)
    return;

  if (info->string == NULL)
    return;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, info->string, -1);
}

static void
popup_leave_fullscreen_callback (GtkAction *action,
                                 TerminalWindow *window)
{
    gtk_window_unfullscreen (GTK_WINDOW (window));
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
popup_clipboard_targets_received_cb (GtkClipboard *clipboard,
                                     GdkAtom *targets,
                                     int n_targets,
                                     TerminalScreenPopupInfo *info)
{
  TerminalWindow *window = info->window;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen = info->screen;
  GtkWidget *popup_menu, *im_menu, *im_menu_item;
  GtkAction *action;
  gboolean can_paste, can_paste_uris, show_link, show_email_link, show_call_link, show_input_method_menu;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen)))
    {
      terminal_screen_popup_info_unref (info);
      return;
    }

  /* Now we know that the screen is realized, we know that the window is still alive */
  remove_popup_info (window);

  priv->popup_info = info; /* adopt the ref added when requesting the clipboard */

  can_paste = targets != NULL && gtk_targets_include_text (targets, n_targets);
  can_paste_uris = targets != NULL && gtk_targets_include_uri (targets, n_targets);
  show_link = info->string != NULL && (info->flavour == FLAVOR_AS_IS || info->flavour == FLAVOR_DEFAULT_TO_HTTP);
  show_email_link = info->string != NULL && info->flavour == FLAVOR_EMAIL;
  show_call_link = info->string != NULL && info->flavour == FLAVOR_VOIP_CALL;

  action = gtk_action_group_get_action (priv->action_group, "PopupSendEmail");
  gtk_action_set_visible (action, show_email_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyEmailAddress");
  gtk_action_set_visible (action, show_email_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCall");
  gtk_action_set_visible (action, show_call_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyCallAddress");
  gtk_action_set_visible (action, show_call_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupOpenLink");
  gtk_action_set_visible (action, show_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyLinkAddress");
  gtk_action_set_visible (action, show_link);

  action = gtk_action_group_get_action (priv->action_group, "PopupCopy");
  gtk_action_set_sensitive (action, vte_terminal_get_has_selection (VTE_TERMINAL (screen)));
  action = gtk_action_group_get_action (priv->action_group, "PopupPaste");
  gtk_action_set_sensitive (action, can_paste);
  action = gtk_action_group_get_action (priv->action_group, "PopupPasteURIPaths");
  gtk_action_set_visible (action, can_paste_uris);
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-show-input-method-menu", &show_input_method_menu,
                NULL);

  action = gtk_action_group_get_action (priv->action_group, "PopupInputMethods");
  gtk_action_set_visible (action, show_input_method_menu);

  im_menu_item = gtk_ui_manager_get_widget (priv->ui_manager,
                                            "/Popup/PopupInputMethods");
  /* FIXME: fix this when gtk+ bug #500065 is done, use vte_terminal_im_merge_ui */
  if (show_input_method_menu)
    {
      im_menu = gtk_menu_new ();
      vte_terminal_im_append_menuitems (VTE_TERMINAL (screen),
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

  if (info->button == 0)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (popup_menu), FALSE);

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

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_targets (clipboard,
                                  (GtkClipboardTargetsReceivedFunc) popup_clipboard_targets_received_cb,
                                  terminal_screen_popup_info_ref (info));
}

static gboolean
screen_match_clicked_cb (TerminalScreen *screen,
                         const char *match,
                         int flavour,
                         guint state,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (screen != priv->active_screen)
    return FALSE;

  gtk_widget_grab_focus (GTK_WIDGET (screen));
  terminal_util_open_url (GTK_WIDGET (window), match, flavour,
                          gtk_get_current_event_time ());

  return TRUE;
}

static void
screen_close_cb (TerminalScreen *screen,
                 TerminalWindow *window)
{
  terminal_window_remove_screen (window, screen);
}

static gboolean
terminal_window_accel_activate_cb (GtkAccelGroup  *accel_group,
                                   GObject        *acceleratable,
                                   guint           keyval,
                                   GdkModifierType modifier,
                                   TerminalWindow *window)
{
  GtkAccelGroupEntry *entries;
  guint n_entries;
  gboolean retval = FALSE;

  entries = gtk_accel_group_query (accel_group, keyval, modifier, &n_entries);
  if (n_entries > 0)
    {
      const char *accel_path;

      accel_path = g_quark_to_string (entries[0].accel_path_quark);

      if (g_str_has_prefix (accel_path, "<Actions>/Main/"))
        {
          const char *action_name;

          /* We want to always consume these accelerators, even if the corresponding
           * action is insensitive, so the corresponding shortcut key escape code
           * isn't sent to the terminal. See bug #453193, bug #138609 and bug #559728.
           * This also makes tab cycling work, bug #92139. (NOT!)
           */

          action_name = I_(accel_path + strlen ("<Actions>/Main/"));

#if 0
          if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook)) > 1 &&
              (action_name == I_("TabsPrevious") || action_name == I_("TabsNext")))
            retval = TRUE;
          else
#endif
               if (action_name == I_("EditCopy") ||
                   action_name == I_("PopupCopy") ||
                   action_name == I_("EditPaste") ||
                   action_name == I_("PopupPaste"))
            retval = TRUE;
        }
    }

  return retval;
}

/*****************************************/

#ifdef GNOME_ENABLE_DEBUG
static void
terminal_window_size_request_cb (GtkWidget *widget,
                                 GtkRequisition *req)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size-request result %d : %d\n",
                         widget, req->width, req->height);
}

static void
terminal_window_size_allocate_cb (GtkWidget *widget,
                                  GtkAllocation *allocation)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size-alloc result %d : %d at (%d, %d)\n",
                         widget,
                         allocation->width, allocation->height,
                         allocation->x, allocation->y);
}
#endif /* GNOME_ENABLE_DEBUG */

static void
terminal_window_realize (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] realize, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->realize (widget);

  /* Need to do this now since this requires the window to be realized */
  if (priv->active_screen != NULL)
    sync_screen_icon_title (priv->active_screen, NULL, window);
}

static gboolean
terminal_window_map_event (GtkWidget    *widget,
			   GdkEventAny  *event)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  gboolean (* map_event) (GtkWidget *, GdkEventAny *) =
      GTK_WIDGET_CLASS (terminal_window_parent_class)->map_event;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] map-event, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  if (priv->clear_demands_attention)
    {
#ifdef GDK_WINDOWING_X11
      GdkWindow *gdk_window = gtk_widget_get_window (widget);

      if (GDK_IS_X11_WINDOW (gdk_window))
	terminal_util_x11_clear_demands_attention (gdk_window);
#endif

      priv->clear_demands_attention = FALSE;
    }

  if (map_event)
    return map_event (widget, event);

  return FALSE;
}

    
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
  
      action = gtk_action_group_get_action (priv->action_group, "PopupLeaveFullscreen");
      gtk_action_set_visible (action, is_fullscreen);
    }
  
  if (window_state_event)
    return window_state_event (widget, event);

  return FALSE;
}

#ifdef GDK_WINDOWING_X11
static void
terminal_window_window_manager_changed_cb (GdkScreen *screen,
                                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean supports_fs;

  if (GDK_IS_X11_SCREEN (screen))
    supports_fs = gdk_x11_screen_supports_net_wm_hint (screen, gdk_atom_intern ("_NET_WM_STATE_FULLSCREEN", FALSE));
  else
    supports_fs = FALSE;

  action = gtk_action_group_get_action (priv->action_group, "ViewFullscreen");
  gtk_action_set_sensitive (action, supports_fs);
}
#endif /* GDK_WINDOWING_X11 */

static void
terminal_window_screen_update (TerminalWindow *window,
                               GdkScreen *screen)
{
  GSettings *settings;
  GtkSettings *gtk_settings;
  char *value;

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_SCREEN (screen)) {
    terminal_window_window_manager_changed_cb (screen, window);
    g_signal_connect (screen, "window-manager-changed",
                      G_CALLBACK (terminal_window_window_manager_changed_cb), window);
  }
#endif

  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (screen), "GT::HasSettingsConnection")))
    return;

  settings = terminal_app_get_global_settings (terminal_app_get ());
  gtk_settings = gtk_settings_get_for_screen (screen);

  g_object_set_data_full (G_OBJECT (screen), "GT::HasSettingsConnection",
                          gtk_settings,
                          (GDestroyNotify) app_setting_notify_destroy_cb);

  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                   gtk_settings,
                   "gtk-enable-mnemonics",
                   G_SETTINGS_BIND_GET);

  g_object_get (gtk_settings, "gtk-menu-bar-accel", &value, NULL);
  g_object_set_data_full (G_OBJECT (gtk_settings), "GT::gtk-menu-bar-accel",
                          value, (GDestroyNotify) g_free);
  enable_menubar_accel_changed_cb (settings, 
                                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                                   gtk_settings);
  g_signal_connect (settings, "changed::" TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                    G_CALLBACK (enable_menubar_accel_changed_cb),
                    gtk_settings);
}

static void
terminal_window_screen_changed (GtkWidget *widget,
                                GdkScreen *previous_screen)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  void (* screen_changed) (GtkWidget *, GdkScreen *) =
    GTK_WIDGET_CLASS (terminal_window_parent_class)->screen_changed;
  GdkScreen *screen;

  if (screen_changed)
    screen_changed (widget, previous_screen);

  screen = gtk_widget_get_screen (widget);
  if (previous_screen == screen)
    return;

#ifdef GDK_WINDOWING_X11
  if (previous_screen && GDK_IS_X11_SCREEN (previous_screen))
    {
      g_signal_handlers_disconnect_by_func (previous_screen,
                                            G_CALLBACK (terminal_window_window_manager_changed_cb),
                                            window);
    }
#endif

  if (!screen)
    return;

  terminal_window_screen_update (window, screen);
}

static void
terminal_window_profile_list_changed_cb (TerminalSettingsList *profiles_list,
                                         TerminalWindow *window)
{
  terminal_window_update_set_profile_menu (window);
  terminal_window_update_new_terminal_menus (window);
}

static void
terminal_window_encoding_list_changed_cb (TerminalApp *app,
                                          TerminalWindow *window)
{
  terminal_window_update_encoding_menu (window);
}

static void
terminal_window_init (TerminalWindow *window)
{
  const GtkActionEntry menu_entries[] =
    {
      /* Toplevel */
      { "File", NULL, N_("_File") },
      { "FileNewWindowProfiles", STOCK_NEW_WINDOW, N_("Open _Terminal")},
      { "FileNewTabProfiles", STOCK_NEW_TAB, N_("Open Ta_b") },
      { "Edit", NULL, N_("_Edit") },
      { "View", NULL, N_("_View") },
      { "Search", NULL, N_("_Search") },
      { "Terminal", NULL, N_("_Terminal") },
      { "Tabs", NULL, N_("Ta_bs") },
      { "Help", NULL, N_("_Help") },
      { "Popup", NULL, NULL },
      { "NotebookPopup", NULL, "" },

      /* File menu */
      { "FileNewWindow", STOCK_NEW_WINDOW, N_("Open _Terminal"), "<shift><control>N",
        NULL,
        G_CALLBACK (file_new_window_callback) },
      { "FileNewTab", STOCK_NEW_TAB, N_("Open Ta_b"), "<shift><control>T",
        NULL,
        G_CALLBACK (file_new_tab_callback) },
      { "FileNewProfile", GTK_STOCK_OPEN, N_("New _Profile"), "",
        NULL,
        G_CALLBACK (file_new_profile_callback) },
      { "FileSaveContents", GTK_STOCK_SAVE, N_("_Save Contents"), "",
        NULL,
        G_CALLBACK (file_save_contents_callback) },
      { "FileCloseTab", GTK_STOCK_CLOSE, N_("C_lose Tab"), "<shift><control>W",
        NULL,
        G_CALLBACK (file_close_tab_callback) },
      { "FileCloseWindow", GTK_STOCK_CLOSE, N_("_Close Window"), "<shift><control>Q",
        NULL,
        G_CALLBACK (file_close_window_callback) },

      /* Edit menu */
      { "EditCopy", GTK_STOCK_COPY, NULL, "<shift><control>C",
        NULL,
        G_CALLBACK (edit_copy_callback) },
      { "EditPaste", GTK_STOCK_PASTE, NULL, "<shift><control>V",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "EditPasteURIPaths", GTK_STOCK_PASTE, N_("Paste _Filenames"), "",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "EditSelectAll", GTK_STOCK_SELECT_ALL, NULL, NULL,
        NULL,
        G_CALLBACK (edit_select_all_callback) },
      { "EditPreferences", NULL, N_("Pre_ferences"), NULL,
        NULL,
        G_CALLBACK (edit_preferences_callback) },
      { "EditCurrentProfile", GTK_STOCK_PREFERENCES, N_("_Profile Preferences"), NULL,
        NULL,
        G_CALLBACK (edit_current_profile_callback) },

      /* View menu */
      { "ViewZoomIn", GTK_STOCK_ZOOM_IN, NULL, "<control>plus",
        NULL,
        G_CALLBACK (view_zoom_in_callback) },
      { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, NULL, "<control>minus",
        NULL,
        G_CALLBACK (view_zoom_out_callback) },
      { "ViewZoom100", GTK_STOCK_ZOOM_100, NULL, "<control>0",
        NULL,
        G_CALLBACK (view_zoom_normal_callback) },

      /* Search menu */
      { "SearchFind", GTK_STOCK_FIND, N_("_Find…"), "<shift><control>F",
	NULL,
	G_CALLBACK (search_find_callback) },
      { "SearchFindNext", NULL, N_("Find Ne_xt"), "<shift><control>H",
	NULL,
	G_CALLBACK (search_find_next_callback) },
      { "SearchFindPrevious", NULL, N_("Find Pre_vious"), "<shift><control>G",
	NULL,
	G_CALLBACK (search_find_prev_callback) },
      { "SearchClearHighlight", NULL, N_("_Clear Highlight"), "<shift><control>J",
	NULL,
	G_CALLBACK (search_clear_highlight_callback) },
#if 0
      { "SearchGoToLine", GTK_STOCK_JUMP_TO, N_("Go to _Line..."), "<shift><control>I",
	NULL,
	G_CALLBACK (search_goto_line_callback) },
      { "SearchIncrementalSearch", GTK_STOCK_FIND, N_("_Incremental Search..."), "<shift><control>K",
	NULL,
	G_CALLBACK (search_incremental_search_callback) },
#endif

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
        G_CALLBACK (tabs_next_or_previous_tab_cb) },
      { "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down",
        NULL,
        G_CALLBACK (tabs_next_or_previous_tab_cb) },
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
      { "PopupSendEmail", NULL, N_("_Send Mail To…"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyEmailAddress", NULL, N_("_Copy E-mail Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupCall", NULL, N_("C_all To…"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyCallAddress", NULL, N_("_Copy Call Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupOpenLink", NULL, N_("_Open Link"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupTerminalProfiles", NULL, N_("P_rofiles") },
      { "PopupCopy", GTK_STOCK_COPY, NULL, "",
        NULL,
        G_CALLBACK (edit_copy_callback) },
      { "PopupPaste", GTK_STOCK_PASTE, NULL, "",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "PopupPasteURIPaths", GTK_STOCK_PASTE, N_("Paste _Filenames"), "",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "PopupNewTerminal", NULL, N_("Open _Terminal"), NULL,
        NULL,
        G_CALLBACK (file_new_window_callback) },
      { "PopupNewTab", NULL, N_("Open Ta_b"), NULL,
        NULL,
        G_CALLBACK (file_new_tab_callback) },
      { "PopupLeaveFullscreen", NULL, N_("L_eave Full Screen"), NULL,
        NULL,
        G_CALLBACK (popup_leave_fullscreen_callback) },
      { "PopupInputMethods", NULL, N_("_Input Methods") }
    };
  
  const GtkToggleActionEntry toggle_menu_entries[] =
    {
      /* View Menu */
      { "ViewMenubar", NULL, N_("Show _Menubar"), NULL,
        NULL,
        G_CALLBACK (view_menubar_toggled_callback),
        FALSE },
      { "ViewFullscreen", NULL, N_("_Full Screen"), NULL,
        NULL,
        G_CALLBACK (view_fullscreen_toggled_callback),
        FALSE }
    };
  TerminalWindowPrivate *priv;
  TerminalApp *app;
  TerminalSettingsList *profiles_list;
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkUIManager *manager;
  GtkWidget *main_vbox;
  GError *error;
  GtkWindowGroup *window_group;
  GtkAccelGroup *accel_group;
  GtkClipboard *clipboard;

  priv = window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, TERMINAL_TYPE_WINDOW, TerminalWindowPrivate);

  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK(terminal_window_delete_event),
                    NULL);
#ifdef GNOME_ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_GEOMETRY)
    {
      g_signal_connect_after (window, "size-request", G_CALLBACK (terminal_window_size_request_cb), NULL);
      g_signal_connect_after (window, "size-allocate", G_CALLBACK (terminal_window_size_allocate_cb), NULL);
    }
#endif

  gtk_window_set_title (GTK_WINDOW (window), _("Terminal"));

  priv->active_screen = NULL;

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), main_vbox);
  gtk_widget_show (main_vbox);

  priv->mdi_container = TERMINAL_MDI_CONTAINER (terminal_notebook_new ());

  g_signal_connect (priv->mdi_container, "screen-close-request",
                    G_CALLBACK (screen_close_request_cb), window);

  g_signal_connect_after (priv->mdi_container, "screen-switched",
                          G_CALLBACK (mdi_screen_switched_cb), window);
  g_signal_connect_after (priv->mdi_container, "screen-added",
                          G_CALLBACK (mdi_screen_added_cb), window);
  g_signal_connect_after (priv->mdi_container, "screen-removed",
                          G_CALLBACK (mdi_screen_removed_cb), window);
  g_signal_connect_data (priv->mdi_container, "screens-reordered",
                         G_CALLBACK (terminal_window_update_tabs_menu_sensitivity),
                         window, NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  /* FIXME hack hack! */
  if (GTK_IS_NOTEBOOK (priv->mdi_container)) {
  g_signal_connect (priv->mdi_container, "button-press-event",
                    G_CALLBACK (notebook_button_press_cb), window);	
  g_signal_connect (priv->mdi_container, "popup-menu",
                    G_CALLBACK (notebook_popup_menu_cb), window);
  }

  /* FIXME hack */
  if (GTK_IS_NOTEBOOK (priv->mdi_container))
    g_signal_connect (priv->mdi_container, "create-window",
                      G_CALLBACK (handle_tab_droped_on_desktop), window);

  gtk_box_pack_end (GTK_BOX (main_vbox), GTK_WIDGET (priv->mdi_container), TRUE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (priv->mdi_container));

  priv->old_char_width = -1;
  priv->old_char_height = -1;
  priv->old_geometry_widget = NULL;
  
  /* Create the UI manager */
  manager = priv->ui_manager = gtk_ui_manager_new ();

  accel_group = gtk_ui_manager_get_accel_group (manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  /* Workaround for bug #453193, bug #138609 and bug #559728 */
  g_signal_connect_after (accel_group, "accel-activate",
                          G_CALLBACK (terminal_window_accel_activate_cb), window);

  /* Create the actions */
  /* Note that this action group name is used in terminal-accels.c; do not change it */
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

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  update_edit_menu (clipboard, NULL, window);
  g_signal_connect (clipboard, "owner-change",
                    G_CALLBACK (update_edit_menu), window);

  /* Idem for this action, since the window is not fullscreen. */
  action = gtk_action_group_get_action (priv->action_group, "PopupLeaveFullscreen");
  gtk_action_set_visible (action, FALSE);

#ifndef ENABLE_SAVE
  action = gtk_action_group_get_action (priv->action_group, "FileSaveContents");
  gtk_action_set_visible (action, FALSE);
#endif
  
  /* Load the UI */
  error = NULL;
  priv->ui_id = gtk_ui_manager_add_ui_from_resource (manager,
                                                     TERMINAL_RESOURCES_PATH_PREFIX "ui/terminal.xml",
                                                     &error);
  g_assert_no_error (error);

  priv->menubar = gtk_ui_manager_get_widget (manager, "/menubar");
  gtk_box_pack_start (GTK_BOX (main_vbox),
		      priv->menubar,
		      FALSE, FALSE, 0);

  /* Add tabs menu */
  priv->tabs_menu = terminal_tabs_menu_new (window);

  app = terminal_app_get ();
  profiles_list = terminal_app_get_profiles_list (app);
  terminal_window_profile_list_changed_cb (profiles_list, window);
  g_signal_connect (profiles_list, "children-changed",
                    G_CALLBACK (terminal_window_profile_list_changed_cb), window);
  
  terminal_window_encoding_list_changed_cb (app, window);
  g_signal_connect (app, "encoding-list-changed",
                    G_CALLBACK (terminal_window_encoding_list_changed_cb), window);

  terminal_window_set_menubar_visible (window, TRUE);
  priv->use_default_menubar_visibility = TRUE;

  terminal_window_update_size_to_menu (window);

  /* We have to explicitly call this, since screen-changed is NOT
   * emitted for the toplevel the first time!
   */
  terminal_window_screen_update (window, gtk_widget_get_screen (GTK_WIDGET (window)));

  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

  terminal_util_set_unique_role (GTK_WINDOW (window), "gnome-terminal-window");
}

static void
terminal_window_style_updated (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;

  GTK_WIDGET_CLASS (terminal_window_parent_class)->style_updated (widget);

  if (priv->active_screen != NULL)
    terminal_screen_update_style (priv->active_screen);

  terminal_window_update_size (window);
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
  object_class->dispose = terminal_window_dispose;
  object_class->finalize = terminal_window_finalize;

  widget_class->show = terminal_window_show;
  widget_class->realize = terminal_window_realize;
  widget_class->map_event = terminal_window_map_event;
  widget_class->window_state_event = terminal_window_state_event;
  widget_class->screen_changed = terminal_window_screen_changed;
  widget_class->style_updated = terminal_window_style_updated;

  g_type_class_add_private (object_class, sizeof (TerminalWindowPrivate));
}

static void
terminal_window_dispose (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  GdkScreen *screen;
  GtkClipboard *clipboard;
  GSList *list, *l;

  /* Deactivate open popup menus. This fixes a crash if the window is closed
   * while the context menu is open.
   */
  list = gtk_ui_manager_get_toplevels (priv->ui_manager, GTK_UI_MANAGER_POPUP);
  for (l = list; l != NULL; l = l->next)
    if (GTK_IS_MENU (l->data))
      gtk_menu_popdown (GTK_MENU (l->data));

  remove_popup_info (window);

  priv->disposed = TRUE;

  if (priv->tabs_menu)
    {
      g_object_unref (priv->tabs_menu);
      priv->tabs_menu = NULL;
    }

  if (priv->profiles_action_group != NULL)
    disconnect_profiles_from_actions_in_group (priv->profiles_action_group);
  if (priv->new_terminal_action_group != NULL)
    disconnect_profiles_from_actions_in_group (priv->new_terminal_action_group);

  app = terminal_app_get ();
  g_signal_handlers_disconnect_by_func (app,
                                        G_CALLBACK (terminal_window_profile_list_changed_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (app,
                                        G_CALLBACK (terminal_window_encoding_list_changed_cb),
                                        window);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  g_signal_handlers_disconnect_by_func (clipboard,
                                        G_CALLBACK (update_edit_menu),
                                        window);

  screen = gtk_widget_get_screen (GTK_WIDGET (object));
#ifdef GDK_WINDOWING_X11
  if (screen && GDK_IS_X11_SCREEN (screen))
    {
      g_signal_handlers_disconnect_by_func (screen,
                                            G_CALLBACK (terminal_window_window_manager_changed_cb),
                                            window);
    }
#endif

  G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}

static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;

  g_object_unref (priv->ui_manager);

  if (priv->confirm_close_dialog)
    gtk_dialog_response (GTK_DIALOG (priv->confirm_close_dialog),
                         GTK_RESPONSE_DELETE_EVENT);

  if (priv->search_find_dialog)
    gtk_dialog_response (GTK_DIALOG (priv->search_find_dialog),
                         GTK_RESPONSE_DELETE_EVENT);

  G_OBJECT_CLASS (terminal_window_parent_class)->finalize (object);
}

static gboolean
terminal_window_delete_event (GtkWidget *widget,
                              GdkEvent *event,
                              gpointer data)
{
   return confirm_close_window_or_tab (TERMINAL_WINDOW (widget), NULL);
}

static void
terminal_window_show (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] show, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  /* Because of the unexpected reentrancy caused by notebook_page_added_callback()
   * showing the TerminalWindow, we can get here when the first page has been
   * added but not yet set current. By setting the page current, we get the
   * right size when we first show the window */
  if (GTK_IS_NOTEBOOK (priv->mdi_container) &&
      gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->mdi_container)) == -1)
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->mdi_container), 0);

  if (priv->active_screen != NULL)
    {
      /* At this point, we have our GdkScreen, and hence the right
       * font size, so we can go ahead and size the window. */
      terminal_window_update_size (window);
    }

  GTK_WIDGET_CLASS (terminal_window_parent_class)->show (widget);
}

TerminalWindow*
terminal_window_new (GApplication *app)
{
  return g_object_new (TERMINAL_TYPE_WINDOW,
                       "application", app,
                       "show-menubar", FALSE,
                       NULL);
}

/**
 * terminal_window_set_is_restored:
 * @window:
 *
 * Marks the window as restored from session.
 */
void
terminal_window_set_is_restored (TerminalWindow *window)
{
  g_return_if_fail (TERMINAL_IS_WINDOW (window));
  g_return_if_fail (!gtk_widget_get_mapped (GTK_WIDGET (window)));

  window->priv->clear_demands_attention = TRUE;
}

static void
profile_set_callback (TerminalScreen *screen,
                      GSettings *old_profile,
                      TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  terminal_window_update_set_profile_menu_active_profile (window);
}

static void
sync_screen_title (TerminalScreen *screen,
                   GParamSpec *psepc,
                   TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  if (screen != priv->active_screen)
    return;

  gtk_window_set_title (GTK_WINDOW (window), terminal_screen_get_title (screen));
}

static void
sync_screen_icon_title (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  if (!terminal_screen_get_icon_title_set (screen))
    return;

  gdk_window_set_icon_name (gtk_widget_get_window (GTK_WIDGET (window)), terminal_screen_get_icon_title (screen));

  priv->icon_title_set = TRUE;
}

static void
sync_screen_icon_title_set (TerminalScreen *screen,
                            GParamSpec *psepc,
                            TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  /* No need to restore the title if we never set an icon title */
  if (!priv->icon_title_set)
    return;

  if (screen != priv->active_screen)
    return;

  if (terminal_screen_get_icon_title_set (screen))
    return;

  /* Need to reset the icon name */
  /* FIXME: Once gtk+ bug 535557 is fixed, use that to unset the icon title. */

  g_object_set_qdata (G_OBJECT (gtk_widget_get_window (GTK_WIDGET (window))),
                      g_quark_from_static_string ("gdk-icon-name-set"),
                      GUINT_TO_POINTER (FALSE));
  priv->icon_title_set = FALSE;

  /* Re-setting the right title will be done by the notify::title handler which comes after this one */
}

static void
screen_font_desc_changed_cb (TerminalScreen *screen,
                             GParamSpec *psepc,
                             TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  terminal_window_update_size (window);
}

/* MDI container callbacks */

static void
screen_close_request_cb (TerminalMdiContainer *container,
                         TerminalScreen *screen,
                         TerminalWindow *window)
{
  if (confirm_close_window_or_tab (window, screen))
    return;

  terminal_window_remove_screen (window, screen);
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            int            position)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *old_window;

  old_window = gtk_widget_get_toplevel (GTK_WIDGET (screen));
  if (gtk_widget_is_toplevel (old_window) &&
      TERMINAL_IS_WINDOW (old_window) &&
      TERMINAL_WINDOW (old_window)== window)
    return;  

  if (TERMINAL_IS_WINDOW (old_window))
    terminal_window_remove_screen (TERMINAL_WINDOW (old_window), screen);

  terminal_mdi_container_add_screen (priv->mdi_container, screen);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_remove_screen (priv->mdi_container, screen);
}

void
terminal_window_move_screen (TerminalWindow *source_window,
                             TerminalWindow *dest_window,
                             TerminalScreen *screen,
                             int dest_position)
{
  TerminalScreenContainer *screen_container;

  g_return_if_fail (TERMINAL_IS_WINDOW (source_window));
  g_return_if_fail (TERMINAL_IS_WINDOW (dest_window));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (screen)) == GTK_WIDGET (source_window));
  g_return_if_fail (dest_position >= -1);

  screen_container = terminal_screen_container_get_from_screen (screen);
  g_assert (TERMINAL_IS_SCREEN_CONTAINER (screen_container));

  /* We have to ref the screen container as well as the screen,
   * because otherwise removing the screen container from the source
   * window's notebook will cause the container and its containing
   * screen to be gtk_widget_destroy()ed!
   */
  g_object_ref_sink (screen_container);
  g_object_ref_sink (screen);
  terminal_window_remove_screen (source_window, screen);

  /* Now we can safely remove the screen from the container and let the container die */
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (screen))), GTK_WIDGET (screen));
  g_object_unref (screen_container);

  terminal_window_add_screen (dest_window, screen, dest_position);
  g_object_unref (screen);
}

GList*
terminal_window_list_screen_containers (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return terminal_mdi_container_list_screen_containers (priv->mdi_container);
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

  /* FIXMEchpe: use gtk_widget_get_realized instead? */
  if (priv->active_screen)
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] setting size after toggling menubar visibility\n",
                             window);

      terminal_window_update_size (window);
    }
}

gboolean
terminal_window_get_menubar_visible (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  return priv->menubar_visible;
}

GtkWidget *
terminal_window_get_mdi_container (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
	
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

  return GTK_WIDGET (priv->mdi_container);
}

static void
terminal_window_update_size (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int grid_width, grid_height;

  /* be sure our geometry is up-to-date */
  terminal_window_update_geometry (window);

  terminal_screen_get_size (priv->active_screen, &grid_width, &grid_height);

  gtk_window_resize_to_geometry (GTK_WINDOW (window), grid_width, grid_height);
}

void
terminal_window_switch_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_set_active_screen (priv->mdi_container, screen);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return terminal_mdi_container_get_active_screen (priv->mdi_container);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
                          GdkEventButton *event,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkWidget *menu;
  GtkAction *action;
  int tab_clicked;

  if (event->type != GDK_BUTTON_PRESS ||
      event->button != 3 ||
      (event->state & gtk_accelerator_get_default_mod_mask ()) != 0)
    return FALSE;

  tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);
  if (tab_clicked < 0)
    return FALSE;

  /* switch to the page the mouse is over */
  gtk_notebook_set_current_page (notebook, tab_clicked);

  action = gtk_action_group_get_action (priv->action_group, "NotebookPopup");
  gtk_action_activate (action);

  menu = gtk_ui_manager_get_widget (priv->ui_manager, "/NotebookPopup");
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 
                  NULL, NULL, 
                  event->button, event->time);

  return TRUE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (priv->mdi_container);
  GtkWidget *focus_widget, *tab, *tab_label, *menu;
  GtkAction *action;
  int page_num;

  focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
  /* Only respond if the notebook is the actual focus */
  if (focus_widget != GTK_WIDGET (priv->mdi_container))
    return FALSE;

  page_num = gtk_notebook_get_current_page (notebook);
  tab = gtk_notebook_get_nth_page (notebook, page_num);
  tab_label = gtk_notebook_get_tab_label (notebook, tab);

  action = gtk_action_group_get_action (priv->action_group, "NotebookPopup");
  gtk_action_activate (action);

  menu = gtk_ui_manager_get_widget (priv->ui_manager, "/NotebookPopup");
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 
                  position_menu_under_widget, tab_label,
                  0, gtk_get_current_event_time ());
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);

  return TRUE;
}

static void
mdi_screen_switched_cb (TerminalMdiContainer *container,
                        TerminalScreen *old_active_screen,
                        TerminalScreen *screen,
                        TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int old_grid_width, old_grid_height;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen-switched old %p new %p\n",
                         window, old_active_screen, screen);

  if (priv->disposed)
    return;

  if (screen == NULL || old_active_screen == screen)
    return;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: setting active tab to screen %p (old active screen %p)\n",
                         window, screen, priv->active_screen);

  if (old_active_screen != NULL && screen != NULL) {
    terminal_screen_get_size (old_active_screen, &old_grid_width, &old_grid_height);

    /* This is so that we maintain the same grid */
    vte_terminal_set_size (VTE_TERMINAL (screen), old_grid_width, old_grid_height);
  }

  priv->active_screen = screen;

  /* Override menubar setting if it wasn't restored from session */
  if (priv->use_default_menubar_visibility)
    {
      gboolean setting =
        g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY);

      terminal_window_set_menubar_visible (window, setting);
    }

  sync_screen_icon_title_set (screen, NULL, window);
  sync_screen_icon_title (screen, NULL, window);
  sync_screen_title (screen, NULL, window);

  /* set size of window to current grid size */
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] setting size after flipping notebook pages\n",
                         window);
  terminal_window_update_size (window);

  terminal_window_update_tabs_menu_sensitivity (window);
  terminal_window_update_encoding_menu_active_encoding (window);
  terminal_window_update_set_profile_menu_active_profile (window);
  terminal_window_update_copy_sensitivity (screen, window);
  terminal_window_update_zoom_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);
}

static void
mdi_screen_added_cb (TerminalMdiContainer *container,
                     TerminalScreen *screen,
                     TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p inserted\n",
                         window, screen);

  g_signal_connect (G_OBJECT (screen),
                    "profile-set",
                    G_CALLBACK (profile_set_callback),
                    window);

  /* FIXME: only connect on the active screen, not all screens! */
  g_signal_connect (screen, "notify::title",
                    G_CALLBACK (sync_screen_title), window);
  g_signal_connect (screen, "notify::icon-title",
                    G_CALLBACK (sync_screen_icon_title), window);
  g_signal_connect (screen, "notify::icon-title-set",
                    G_CALLBACK (sync_screen_icon_title_set), window);
  g_signal_connect (screen, "notify::font-desc",
                    G_CALLBACK (screen_font_desc_changed_cb), window);
  g_signal_connect (screen, "selection-changed",
                    G_CALLBACK (terminal_window_update_copy_sensitivity), window);

  g_signal_connect (screen, "show-popup-menu",
                    G_CALLBACK (screen_show_popup_menu_callback), window);
  g_signal_connect (screen, "match-clicked",
                    G_CALLBACK (screen_match_clicked_cb), window);
  g_signal_connect (screen, "resize-window",
                    G_CALLBACK (screen_resize_window_cb), window);

  g_signal_connect (screen, "close-screen",
                    G_CALLBACK (screen_close_cb), window);

  terminal_window_update_tabs_menu_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);

#if 0
  /* FIXMEchpe: wtf is this doing? */

  /* If we have an active screen, match its size and zoom */
  if (priv->active_screen)
    {
      int current_width, current_height;
      double scale;

      terminal_screen_get_size (priv->active_screen, &current_width, &current_height);
      vte_terminal_set_size (VTE_TERMINAL (screen), current_width, current_height);

      scale = terminal_screen_get_font_scale (priv->active_screen);
      terminal_screen_set_font_scale (screen, scale);
    }
#endif

  if (priv->present_on_insert)
    {
      gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
      priv->present_on_insert = FALSE;
    }
}

static void
mdi_screen_removed_cb (TerminalMdiContainer *container,
                       TerminalScreen *screen,
                       TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int pages;

  if (priv->disposed)
    return;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p removed\n",
                         window, screen);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (profile_set_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (sync_screen_title),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (sync_screen_icon_title),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (sync_screen_icon_title_set),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (screen_font_desc_changed_cb),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (terminal_window_update_copy_sensitivity),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_show_popup_menu_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_match_clicked_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_resize_window_cb),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_close_cb),
                                        window);

  terminal_window_update_tabs_menu_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);

  pages = terminal_mdi_container_get_n_screens (container);
  if (pages == 1)
    {
      terminal_window_update_size (window);
    }
  else if (pages == 0)
    {
      gtk_widget_destroy (GTK_WIDGET (window));
    }
}

gboolean
terminal_window_parse_geometry (TerminalWindow *window,
				const char     *geometry)
{
  TerminalWindowPrivate *priv = window->priv;

  /* gtk_window_parse_geometry() needs to have the right base size
   * and width/height increment to compute the window size from
   * the geometry.
   */
  terminal_window_update_geometry (window);

  if (!gtk_window_parse_geometry (GTK_WINDOW (window), geometry))
    return FALSE;

  /* We won't actually get allocated at the size parsed out of the
   * geometry until the window is shown. If terminal_window_update_size()
   * is called between now and then, that could result in us getting
   * snapped back to the old grid size. So we need to immediately
   * update the size of the active terminal to grid size from the
   * geometry.
   */
  if (priv->active_screen)
    {
      int grid_width, grid_height;

      /* After parse_geometry(), the default size is in units of the
       * width/height increment, not a pixel size */
      gtk_window_get_default_size (GTK_WINDOW (window), &grid_width, &grid_height);

      vte_terminal_set_size (VTE_TERMINAL (priv->active_screen),
			     grid_width, grid_height);
    }

  return TRUE;
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  GdkGeometry hints;
  int char_width;
  int char_height;
  
  if (priv->active_screen == NULL)
    return;

  widget = GTK_WIDGET (priv->active_screen);

  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */
  terminal_screen_get_cell_size (priv->active_screen, &char_width, &char_height);
  
  if (char_width != priv->old_char_width ||
      char_height != priv->old_char_height ||
      widget != (GtkWidget*) priv->old_geometry_widget)
    {
      GtkBorder *inner_border = NULL;
      
      /* FIXME Since we're using xthickness/ythickness to compute
       * padding we need to change the hints when the theme changes.
       */

      gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);

      hints.base_width = (inner_border ? (inner_border->left + inner_border->right) : 0);
      hints.base_height = (inner_border ? (inner_border->top + inner_border->bottom) : 0);

      gtk_border_free (inner_border);

#define MIN_WIDTH_CHARS 4
#define MIN_HEIGHT_CHARS 1
      
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

      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: base %dx%d min %dx%d inc %d %d\n",
                             window,
                             hints.base_width,
                             hints.base_height,
                             hints.min_width,
                             hints.min_height,
                             hints.width_inc,
                             hints.height_inc);
      
      priv->old_char_width = hints.width_inc;
      priv->old_char_height = hints.height_inc;
      priv->old_geometry_widget = widget;
    }
  else
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: increment unchanged, not setting\n",
                             window);
    }
}

static void
file_new_window_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalWindow *new_window;
  GSettings *profile;
  char *new_working_directory;

  app = terminal_app_get ();

  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  if (!profile)
    profile = terminal_screen_get_profile (priv->active_screen);
  if (!profile)
    return;

  new_window = terminal_app_new_window (app, gtk_widget_get_screen (GTK_WIDGET (window)));

  new_working_directory = terminal_screen_get_current_dir (priv->active_screen);
  terminal_app_new_terminal (app, new_window, profile,
                             NULL, NULL,
                             new_working_directory,
                             terminal_screen_get_initial_environment (priv->active_screen),
                             1.0);
  g_free (new_working_directory);

  gtk_window_present (GTK_WINDOW (new_window));
}

static void
file_new_tab_callback (GtkAction *action,
                       TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  GSettings *profile;
  char *new_working_directory;

  app = terminal_app_get ();
  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  if (!profile)
    profile = terminal_screen_get_profile (priv->active_screen);
  if (!profile)
    return;

  new_working_directory = terminal_screen_get_current_dir (priv->active_screen);
  terminal_app_new_terminal (app, window, profile,
                             NULL, NULL,
                             new_working_directory,
                             terminal_screen_get_initial_environment (priv->active_screen),
                             1.0);
  g_free (new_working_directory);
}

static void
confirm_close_response_cb (GtkWidget *dialog,
                           int response,
                           TerminalWindow *window)
{
  TerminalScreen *screen;

  screen = g_object_get_data (G_OBJECT (dialog), "close-screen");

  gtk_widget_destroy (dialog);

  if (response != GTK_RESPONSE_ACCEPT)
    return;
    
  if (screen)
    terminal_window_remove_screen (window, screen);
  else
    gtk_widget_destroy (GTK_WIDGET (window));
}

/* Returns: TRUE if closing needs to wait until user confirmation;
 * FALSE if the terminal or window can close immediately.
 */
static gboolean
confirm_close_window_or_tab (TerminalWindow *window,
                             TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *dialog;
  gboolean do_confirm;
  int n_tabs;

  if (priv->confirm_close_dialog)
    {
      /* WTF, already have one? It's modal, so how did that happen? */
      gtk_dialog_response (GTK_DIALOG (priv->confirm_close_dialog),
                           GTK_RESPONSE_DELETE_EVENT);
    }

  do_confirm = g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                       TERMINAL_SETTING_CONFIRM_CLOSE_KEY);
  if (!do_confirm)
    return FALSE;

  if (screen)
    {
      do_confirm = terminal_screen_has_foreground_process (screen);
      n_tabs = 1;
    }
  else
    {
      GList *tabs, *t;

      do_confirm = FALSE;

      tabs = terminal_window_list_screen_containers (window);
      n_tabs = g_list_length (tabs);

      for (t = tabs; t != NULL; t = t->next)
        {
          TerminalScreen *terminal_screen;

          terminal_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (t->data));
          if (terminal_screen_has_foreground_process (terminal_screen))
            {
              do_confirm = TRUE;
              break;
            }
        }
      g_list_free (tabs);
    }

  if (!do_confirm)
    return FALSE;

  dialog = priv->confirm_close_dialog =
    gtk_message_dialog_new (GTK_WINDOW (window),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_MESSAGE_WARNING,
                            GTK_BUTTONS_CANCEL,
                            "%s", n_tabs > 1 ? _("Close this window?") : _("Close this terminal?"));

  if (n_tabs > 1)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", _("There are still processes running in some terminals in this window. "
                                                      "Closing the window will kill all of them."));
  else
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", _("There is still a process running in this terminal. "
                                                      "Closing the terminal will kill it."));

  gtk_window_set_title (GTK_WINDOW (dialog), ""); 

  gtk_dialog_add_button (GTK_DIALOG (dialog), n_tabs > 1 ? _("C_lose Window") : _("C_lose Terminal"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  g_object_set_data (G_OBJECT (dialog), "close-screen", screen);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed), &priv->confirm_close_dialog);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (confirm_close_response_cb), window);

  gtk_window_present (GTK_WINDOW (dialog));

  return TRUE;
}

static void
file_close_window_callback (GtkAction *action,
                            TerminalWindow *window)
{
  if (confirm_close_window_or_tab (window, NULL))
    return;

  gtk_widget_destroy (GTK_WIDGET (window));
}

#ifdef ENABLE_SAVE
static void
save_contents_dialog_on_response (GtkDialog *dialog, gint response_id, gpointer terminal)
{
  GtkWindow *parent;
  gchar *filename_uri = NULL;
  GFile *file;
  GOutputStream *stream;
  GError *error = NULL;

  if (response_id != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      return;
    }

  parent = (GtkWindow*) gtk_widget_get_ancestor (GTK_WIDGET (terminal), GTK_TYPE_WINDOW);
  filename_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (filename_uri == NULL)
    return;

  file = g_file_new_for_uri (filename_uri);
  stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

  if (stream)
    {
      /* XXX
       * FIXME
       * This is a sync operation.
       * Should be replaced with the async version when vte implements that.
       */
      vte_terminal_write_contents (terminal, stream,
				   VTE_TERMINAL_WRITE_DEFAULT,
				   NULL, &error);
      g_object_unref (stream);
    }

  if (error)
    {
      terminal_util_show_error_dialog (parent, NULL, error,
				       "%s", _("Could not save contents"));
      g_error_free (error);
    }

  g_object_unref(file);
  g_free(filename_uri);
}
#endif /* ENABLE_SAVE */

static void
file_save_contents_callback (GtkAction *action,
                             TerminalWindow *window)
{
#ifdef ENABLE_SAVE
  GtkWidget *dialog = NULL;
  TerminalWindowPrivate *priv = window->priv;
  VteTerminal *terminal;

  if (!priv->active_screen)
    return;

  terminal = VTE_TERMINAL (priv->active_screen);
  g_return_if_fail (VTE_IS_TERMINAL (terminal));

  dialog = gtk_file_chooser_dialog_new (_("Save as…"),
                                        GTK_WINDOW(window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  /* XXX where should we save to? */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW(window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  g_signal_connect (dialog, "response", G_CALLBACK (save_contents_dialog_on_response), terminal);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
#endif /* ENABLE_SAVE */
}

static void
file_close_tab_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *active_screen = priv->active_screen;
  
  if (!active_screen)
    return;

  if (confirm_close_window_or_tab (window, active_screen))
    return;

  terminal_window_remove_screen (window, active_screen);
}

static void
edit_copy_callback (GtkAction *action,
                    TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!priv->active_screen)
    return;
      
  vte_terminal_copy_clipboard (VTE_TERMINAL (priv->active_screen));
}

typedef struct {
  TerminalScreen *screen;
  gboolean uris_as_paths;
} PasteData;

static void
clipboard_uris_received_cb (GtkClipboard *clipboard,
                            /* const */ char **uris,
                            PasteData *data)
{
  char *text;
  gsize len;

  if (!uris) {
    g_object_unref (data->screen);
    g_slice_free (PasteData, data);
    return;
  }

  /* This potentially modifies the strings in |uris| but that's ok */
  if (data->uris_as_paths)
    terminal_util_transform_uris_to_quoted_fuse_paths (uris);

  text = terminal_util_concat_uris (uris, &len);
  vte_terminal_feed_child (VTE_TERMINAL (data->screen), text, len);
  g_free (text);

  g_object_unref (data->screen);
  g_slice_free (PasteData, data);
}

static void
clipboard_targets_received_cb (GtkClipboard *clipboard,
                               GdkAtom *targets,
                               int n_targets,
                               PasteData *data)
{
  if (!targets) {
    g_object_unref (data->screen);
    g_slice_free (PasteData, data);
    return;
  }

  if (gtk_targets_include_uri (targets, n_targets)) {
    gtk_clipboard_request_uris (clipboard,
                                (GtkClipboardURIReceivedFunc) clipboard_uris_received_cb,
                                data);
    return;
  } else /* if (gtk_targets_include_text (targets, n_targets)) */ {
    vte_terminal_paste_clipboard (VTE_TERMINAL (data->screen));
  }

  g_object_unref (data->screen);
  g_slice_free (PasteData, data);
}

static void
edit_paste_callback (GtkAction *action,
                     TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkClipboard *clipboard;
  PasteData *data;
  const char *name;

  if (!priv->active_screen)
    return;
      
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  name = gtk_action_get_name (action);

  data = g_slice_new (PasteData);
  data->screen = g_object_ref (priv->active_screen);
  data->uris_as_paths = (name == I_("EditPasteURIPaths") || name == I_("PopupPasteURIPaths"));

  gtk_clipboard_request_targets (clipboard,
                                 (GtkClipboardTargetsReceivedFunc) clipboard_targets_received_cb,
                                 data);
}

static void
edit_select_all_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!priv->active_screen)
    return;

  vte_terminal_select_all (VTE_TERMINAL (priv->active_screen));
}
      
static void
edit_preferences_callback (GtkAction *action,
                           TerminalWindow *window)
{
  terminal_app_edit_preferences (terminal_app_get (), GTK_WINDOW (window));
}

static void
edit_current_profile_callback (GtkAction *action,
                               TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_edit_profile (terminal_app_get (),
                             terminal_screen_get_profile (priv->active_screen),
                             GTK_WINDOW (window),
                             NULL);
}

static void
file_new_profile_callback (GtkAction *action,
                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_new_profile (terminal_app_get (),
                            terminal_screen_get_profile (priv->active_screen),
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
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (window)));

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
  guint i;

  for (i = 0; i < G_N_ELEMENTS (zoom_factors); ++i)
    {
      /* Find a font that's larger than this one */
      if ((zoom_factors[i] - current) > 1e-6)
        {
          *found = zoom_factors[i];
          return TRUE;
        }
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
  
  if (priv->active_screen == NULL)
    return;
  
  current = terminal_screen_get_font_scale (priv->active_screen);
  if (!find_larger_zoom_factor (current, &current))
    return;
      
  terminal_screen_set_font_scale (priv->active_screen, current);
  terminal_window_update_zoom_sensitivity (window);
}

static void
view_zoom_out_callback (GtkAction *action,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  double current;

  if (priv->active_screen == NULL)
    return;
  
  current = terminal_screen_get_font_scale (priv->active_screen);
  if (!find_smaller_zoom_factor (current, &current))
    return;
      
  terminal_screen_set_font_scale (priv->active_screen, current);
  terminal_window_update_zoom_sensitivity (window);
}

static void
view_zoom_normal_callback (GtkAction *action,
                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  if (priv->active_screen == NULL)
    return;

  terminal_screen_set_font_scale (priv->active_screen, PANGO_SCALE_MEDIUM);
  terminal_window_update_zoom_sensitivity (window);
}


static void
search_find_response_callback (GtkWidget *dialog,
			       int        response,
			       gpointer   user_data)
{
  TerminalWindow *window = TERMINAL_WINDOW (user_data);
  TerminalWindowPrivate *priv = window->priv;
  TerminalSearchFlags flags;
  GRegex *regex;

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  if (G_UNLIKELY (!priv->active_screen))
    return;

  regex = terminal_search_dialog_get_regex (dialog);
  g_return_if_fail (regex != NULL);

  flags = terminal_search_dialog_get_search_flags (dialog);

  vte_terminal_search_set_gregex (VTE_TERMINAL (priv->active_screen), regex);
  vte_terminal_search_set_wrap_around (VTE_TERMINAL (priv->active_screen),
				       (flags & TERMINAL_SEARCH_FLAG_WRAP_AROUND));

  if (flags & TERMINAL_SEARCH_FLAG_BACKWARDS)
    vte_terminal_search_find_previous (VTE_TERMINAL (priv->active_screen));
  else
    vte_terminal_search_find_next (VTE_TERMINAL (priv->active_screen));

  terminal_window_update_search_sensitivity (priv->active_screen, window);
}

static gboolean
search_dialog_delete_event_cb (GtkWidget   *widget,
			       GdkEventAny *event,
			       gpointer     user_data)
{
	/* prevent destruction */
	return TRUE;
}

static void
search_find_callback (GtkAction *action,
		      TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!priv->search_find_dialog) {
    GtkWidget *dialog;

    dialog = priv->search_find_dialog = terminal_search_dialog_new (GTK_WINDOW (window));

    g_signal_connect (dialog, "destroy",
		      G_CALLBACK (gtk_widget_destroyed), &priv->search_find_dialog);
    g_signal_connect (dialog, "response",
		      G_CALLBACK (search_find_response_callback), window);
    g_signal_connect (dialog, "delete-event",
		     G_CALLBACK (search_dialog_delete_event_cb), NULL);
  }

  terminal_search_dialog_present (priv->search_find_dialog);
}

static void
search_find_next_callback (GtkAction *action,
			   TerminalWindow *window)
{
  if (G_UNLIKELY (!window->priv->active_screen))
    return;

  vte_terminal_search_find_next (VTE_TERMINAL (window->priv->active_screen));
}

static void
search_find_prev_callback (GtkAction *action,
			   TerminalWindow *window)
{
  if (G_UNLIKELY (!window->priv->active_screen))
    return;

  vte_terminal_search_find_previous (VTE_TERMINAL (window->priv->active_screen));
}

static void
search_clear_highlight_callback (GtkAction *action,
				 TerminalWindow *window)
{
  if (G_UNLIKELY (!window->priv->active_screen))
    return;

  vte_terminal_search_set_gregex (VTE_TERMINAL (window->priv->active_screen), NULL);
}

static void
terminal_set_title_dialog_response_cb (GtkWidget *dialog,
                                       int response,
                                       TerminalScreen *screen)
{
  if (response == GTK_RESPONSE_OK)
    {
      GtkEntry *entry;
      const char *text;

      entry = GTK_ENTRY (g_object_get_data (G_OBJECT (dialog), "title-entry"));
      text = gtk_entry_get_text (entry);
      terminal_screen_set_user_title (screen, text);
    }

  gtk_widget_destroy (dialog);
}

static void
terminal_set_title_callback (GtkAction *action,
                             TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *dialog, *message_area, *hbox, *label, *entry;

  if (priv->active_screen == NULL)
    return;

  /* FIXME: hook the screen up so this dialogue closes if the terminal screen closes */

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_OTHER,
                                   GTK_BUTTONS_OK_CANCEL,
                                   "%s", "");

  gtk_window_set_title (GTK_WINDOW (dialog), _("Set Title"));
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_role (GTK_WINDOW (dialog), "gnome-terminal-change-title");
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  /* Alternative button order was set automatically by GtkMessageDialog */

  g_signal_connect (dialog, "response",
                    G_CALLBACK (terminal_set_title_dialog_response_cb), priv->active_screen);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

  message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
  gtk_container_foreach (GTK_CONTAINER (message_area), (GtkCallback) gtk_widget_hide, NULL);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (message_area), hbox, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("_Title:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (entry), 32);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show_all (hbox);

  gtk_widget_grab_focus (entry);
  gtk_entry_set_text (GTK_ENTRY (entry), terminal_screen_get_raw_title (priv->active_screen));
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
  g_object_set_data (G_OBJECT (dialog), "title-entry", entry);

  gtk_window_present (GTK_WINDOW (dialog));
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

  if (priv->active_screen == NULL)
    return;
      
  vte_terminal_reset (VTE_TERMINAL (priv->active_screen), TRUE, FALSE);
}

static void
terminal_reset_clear_callback (GtkAction *action,
                               TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == NULL)
    return;
      
  vte_terminal_reset (VTE_TERMINAL (priv->active_screen), TRUE, TRUE);
}

static void
tabs_next_or_previous_tab_cb (GtkAction *action,
                              TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *name;
  guint keyval = 0;

  name = gtk_action_get_name (action);
  if (strcmp (name, "TabsNext") == 0) {
    keyval = GDK_KEY_Page_Down;
  } else if (strcmp (name, "TabsPrevious") == 0) {
    keyval = GDK_KEY_Page_Up;
  }

  /* FIXMEchpe this is GtkNotebook specific */
  gtk_bindings_activate (G_OBJECT (priv->mdi_container),
                         keyval,
                         GDK_CONTROL_MASK);
}

static void
tabs_move_left_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         -1);
}

static void
tabs_move_right_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         +1);
}

static void
tabs_detach_tab_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalWindow *new_window;
  TerminalScreen *screen;
  char *geometry;
  int width, height;

  app = terminal_app_get ();

  screen = priv->active_screen;

  /* FIXME: this seems wrong if tabs are shown in the window */
  terminal_screen_get_size (screen, &width, &height);
  geometry = g_strdup_printf ("%dx%d", width, height);

  new_window = terminal_app_new_window (app, gtk_widget_get_screen (GTK_WIDGET (window)));

  terminal_window_move_screen (window, new_window, screen, -1);

  terminal_window_parse_geometry (new_window, geometry);
  g_free (geometry);

  gtk_window_present_with_time (GTK_WINDOW (new_window), gtk_get_current_event_time ());
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
  terminal_util_show_about (GTK_WINDOW (window));
}

GtkUIManager *
terminal_window_get_ui_manager (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return priv->ui_manager;
}
