/* widget for a toplevel terminal window, possibly containing multiple terminals */

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
#include "terminal-window.h"
#include "terminal.h"
#include <string.h>
#include <stdlib.h>
#include <libzvt/libzvt.h>
#include <libgnomeui/gnome-about.h>

struct _TerminalWindowPrivate
{
  GtkWidget *main_vbox;
  GtkWidget *menubar;
  GtkWidget *notebook;
  GtkWidget *copy_menuitem;
  GtkWidget *paste_menuitem;
  GtkWidget *edit_config_menuitem;
  GtkWidget *delete_config_menuitem;
  GtkWidget *choose_config_menuitem;
  GList *terms;
  TerminalScreen *active_term;
  GdkPixbuf *icon;
  int old_char_width;
  int old_char_height;
  guint menubar_visible : 1;
  guint use_default_menubar_visibility : 1;
};

enum {
  dummy, /* remove this when you add more signals */
  LAST_SIGNAL
};

static void terminal_window_init        (TerminalWindow      *window);
static void terminal_window_class_init  (TerminalWindowClass *klass);
static void terminal_window_finalize    (GObject             *object);
static void terminal_window_destroy     (GtkObject           *object);

static void       screen_set_scrollbar (TerminalScreen *screen,
                                        GtkWidget      *scrollbar);
static void       screen_set_hbox      (TerminalScreen *screen,
                                        GtkWidget      *hbox);
static void       screen_set_label     (TerminalScreen *screen,
                                        GtkWidget      *label);
static GtkWidget* screen_get_scrollbar (TerminalScreen *screen);
static GtkWidget* screen_get_hbox      (TerminalScreen *screen);
static GtkWidget* screen_get_label     (TerminalScreen *screen);

static TerminalScreen* find_screen_by_hbox (TerminalWindow *window,
                                            GtkWidget      *hbox);

static void notebook_page_switched_callback (GtkWidget *notebook,
                                             GtkNotebookPage *useless_crap,
                                             int              page_num,
                                             TerminalWindow  *window);

static void new_window_callback           (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void new_tab_callback              (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void close_window_callback         (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void close_tab_callback            (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void copy_callback                 (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void paste_callback                (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void change_configuration_callback (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void edit_configuration_callback   (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void new_configuration_callback    (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void manage_configurations_callback(GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void hide_menubar_callback         (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void reset_callback                (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void reset_and_clear_callback      (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void help_callback                 (GtkWidget      *menuitem,
                                           TerminalWindow *window);
static void about_callback                (GtkWidget      *menuitem,
                                           TerminalWindow *window);

static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
terminal_window_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (TerminalWindowClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) terminal_window_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (TerminalWindow),
        0,              /* n_preallocs */
        (GInstanceInitFunc) terminal_window_init,
      };
      
      object_type = g_type_register_static (GTK_TYPE_WINDOW,
                                            "TerminalWindow",
                                            &object_info, 0);
    }
  
  return object_type;
}

static GtkWidget*
append_menuitem (GtkWidget  *menu,
                 const char *text,
                 GCallback   callback,
                 gpointer    data)
{
  GtkWidget *menu_item;
  
  menu_item = gtk_menu_item_new_with_mnemonic (text);
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
append_stock_menuitem (GtkWidget  *menu,
                       const char *text,
                       GCallback   callback,
                       gpointer    data)
{
  GtkWidget *menu_item;
  
  menu_item = gtk_image_menu_item_new_from_stock (text, NULL);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         menu_item);

  if (callback)
    g_signal_connect (G_OBJECT (menu_item),
                      "activate",
                      callback, data);

  return menu_item;
}

static void
set_menu_item_text (GtkWidget *menuitem,
                    const char *text)
{
  gtk_label_set_text_with_mnemonic (GTK_LABEL (GTK_BIN (menuitem)->child),
                                    text);
}

static void
fill_in_config_picker_submenu (TerminalWindow *window)
{
  GtkWidget *menu;
  GtkWidget *menu_item;
  GList *profiles;
  GList *tmp;
  GSList *group;

  if (window->priv->active_term == NULL)
    {
      gtk_widget_set_sensitive (window->priv->choose_config_menuitem, FALSE);
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (window->priv->choose_config_menuitem),
                                 NULL);
      return;
    }
  
  gtk_widget_set_sensitive (window->priv->choose_config_menuitem, TRUE);

  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (window->priv->choose_config_menuitem),
                             menu);
  
  group = NULL;
  profiles = terminal_profile_get_list ();
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

static void
terminal_menu_activated (GtkMenuItem    *menu_item,
                         TerminalWindow *window)
{
  fill_in_config_picker_submenu (window);
}

static void
terminal_window_init (TerminalWindow *window)
{
  GtkWidget *mi;
  GtkWidget *menu;

  gtk_window_set_title (GTK_WINDOW (window), _("Terminal"));
  
  window->priv = g_new0 (TerminalWindowPrivate, 1);
  window->priv->terms = NULL;
  window->priv->active_term = NULL;
  window->priv->menubar = gtk_menu_bar_new ();
  window->priv->menubar_visible = FALSE;
  g_object_ref (G_OBJECT (window->priv->menubar)); /* so we can add/remove */
  
  window->priv->main_vbox = gtk_vbox_new (FALSE, 0);
  window->priv->notebook = gtk_notebook_new ();

  window->priv->old_char_width = -1;
  window->priv->old_char_height = -1;
  
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (window->priv->notebook),
                               TRUE);
  
  g_signal_connect_after (G_OBJECT (window->priv->notebook),
                          "switch_page",
                          G_CALLBACK (notebook_page_switched_callback),
                          window);
  
  gtk_container_add (GTK_CONTAINER (window),
                     window->priv->main_vbox);

  gtk_box_pack_end (GTK_BOX (window->priv->main_vbox),
                    window->priv->notebook,
                    TRUE, TRUE, 0);
  
  mi = append_menuitem (window->priv->menubar,
                        _("_File"),
                        NULL, NULL);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  append_menuitem (menu, _("_New window"),
                   G_CALLBACK (new_window_callback),
                   window);
  append_menuitem (menu, _("New _tab"),
                   G_CALLBACK (new_tab_callback),
                   window);

  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  
  append_menuitem (menu, _("_Close window"),
                   G_CALLBACK (close_window_callback),
                   window);

  append_menuitem (menu, _("C_lose tab"),
                   G_CALLBACK (close_tab_callback),
                   window);
  
  mi = append_menuitem (window->priv->menubar,
                        _("_Edit"),
                        NULL, NULL);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  window->priv->copy_menuitem = append_stock_menuitem (menu,
                                                       GTK_STOCK_COPY,
                                                       G_CALLBACK (copy_callback),
                                                       window);
  window->priv->paste_menuitem = append_stock_menuitem (menu,
                                                        GTK_STOCK_PASTE,
                                                        G_CALLBACK (paste_callback),
                                                        window);

  mi = append_menuitem (window->priv->menubar,
                        _("_Terminal"),
                        NULL, NULL);

  /* Set up a callback to demand-create the Configuration submenu */
  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (terminal_menu_activated),
                    window);  
  
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  /* submenu of this dynamically generated up above */
  window->priv->choose_config_menuitem =
    append_menuitem (menu, _("_Profile"),
                     NULL, NULL);

  window->priv->edit_config_menuitem =
    append_menuitem (menu, _("_Edit current profile..."),
                     G_CALLBACK (edit_configuration_callback), window);
  
  append_menuitem (menu, _("_New profile..."),
                   G_CALLBACK (new_configuration_callback), window);

  append_menuitem (menu, _("_Manage profiles..."),
                   G_CALLBACK (manage_configurations_callback), window);
  
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  
  append_menuitem (menu, _("Hide menu_bar"),
                   G_CALLBACK (hide_menubar_callback), window);
  
  append_menuitem (menu, _("_Reset terminal"),
                   G_CALLBACK (reset_callback), window);

  append_menuitem (menu, _("Reset and C_lear"),
                   G_CALLBACK (reset_and_clear_callback), window);

  mi = append_menuitem (window->priv->menubar,
                        _("_Help"),
                        NULL, NULL);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);

  append_menuitem (menu, _("_Help on GNOME terminal"),
                   G_CALLBACK (help_callback), window);

  /* FIXME use stock item */
  append_menuitem (menu, _("_About"),
                   G_CALLBACK (about_callback), window);

  terminal_window_set_menubar_visible (window, TRUE);
  window->priv->use_default_menubar_visibility = TRUE;  
  
  gtk_widget_show_all (window->priv->main_vbox);
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = terminal_window_finalize;
  gtk_object_class->destroy = terminal_window_destroy;
}

static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window;

  window = TERMINAL_WINDOW (object);

  if (window->priv->icon)
    g_object_unref (G_OBJECT (window->priv->icon));
  
  g_free (window->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
terminal_window_destroy (GtkObject *object)
{
  TerminalWindow *window;

  window = TERMINAL_WINDOW (object);

  while (window->priv->terms)
    terminal_window_remove_screen (window, window->priv->terms->data);

  if (window->priv->menubar)
    g_object_unref (G_OBJECT (window->priv->menubar));
  
  window->priv->menubar = NULL;
  window->priv->notebook = NULL;
  window->priv->main_vbox = NULL;
  window->priv->copy_menuitem = NULL;
  window->priv->paste_menuitem = NULL;
  window->priv->edit_config_menuitem = NULL;
  window->priv->choose_config_menuitem = NULL;
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);  
}

TerminalWindow*
terminal_window_new (void)
{
  TerminalWindow *window;

  window = g_object_new (TERMINAL_TYPE_WINDOW, NULL);

  return window;
}

static void
update_notebook (TerminalWindow *window)
{
  if (g_list_length (window->priv->terms) > 1)
    {
      gtk_notebook_set_show_border (GTK_NOTEBOOK (window->priv->notebook),
                                    TRUE);
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook),
                                  TRUE);
    }
  else
    {
      gtk_notebook_set_show_border (GTK_NOTEBOOK (window->priv->notebook),
                                    FALSE);
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook),
                                  FALSE);
    }
}

static void
profile_set_callback (TerminalScreen *screen,
                      TerminalWindow *window)
{
  /* nothing anymore */
}

static void
title_changed_callback (TerminalScreen *screen,
                        TerminalWindow *window)
{
  GtkWidget *label;
  
  if (screen == window->priv->active_term)
    gtk_window_set_title (GTK_WINDOW (window),
                          terminal_screen_get_title (screen));

  label = screen_get_label (screen);
  gtk_label_set_text (GTK_LABEL (label), terminal_screen_get_title (screen));
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
selection_changed_callback (TerminalScreen *screen,
                            TerminalWindow *window)
{
  update_copy_sensitivity (window);
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen)
{
  TerminalWindow *old;
  ZvtTerm *term;
  GtkWidget *hbox;
  GtkWidget *scrollbar;
  GtkWidget *label;
  
  old = terminal_screen_get_window (screen);

  if (old == window)
    return;  

  g_object_ref (G_OBJECT (screen)); /* make our own new refcount */

  if (old)
    terminal_window_remove_screen (old, screen);

  /* keep this list in same order as tabs in notebook,
   * so that terminal_window_list_screens() returns screens
   * in tab order
   */
  window->priv->terms = g_list_append (window->priv->terms, screen);

  terminal_screen_set_window (screen, window);

  hbox = gtk_hbox_new (FALSE, 0);
  scrollbar = gtk_vscrollbar_new (NULL);
  label = gtk_label_new (terminal_screen_get_title (screen));
  
  screen_set_hbox (screen, hbox);
  screen_set_scrollbar (screen, scrollbar);
  screen_set_label (screen, label);

  g_signal_connect (G_OBJECT (screen),
                    "profile_set",
                    G_CALLBACK (profile_set_callback),
                    window);

  g_signal_connect (G_OBJECT (screen),
                    "title_changed",
                    G_CALLBACK (title_changed_callback),
                    window);

  g_signal_connect (G_OBJECT (screen),
                    "selection_changed",
                    G_CALLBACK (selection_changed_callback),
                    window);
  
  term = ZVT_TERM (terminal_screen_get_widget (screen));

  gtk_box_pack_start (GTK_BOX (hbox),
                      GTK_WIDGET (term), TRUE, TRUE, 0);
  
  gtk_range_set_adjustment (GTK_RANGE (scrollbar),
                            term->adjustment);  

  update_notebook (window);
  
  gtk_notebook_append_page (GTK_NOTEBOOK (window->priv->notebook),
                            hbox,
                            label);
  
  /* ZvtTerm is a broken POS and requires this realize to get
   * the size request right.
   */
  gtk_widget_realize (GTK_WIDGET (term));

  gtk_widget_show_all (hbox);

  terminal_window_update_scrollbar (window, screen);
  
  /* Make the first-added screen the active one */
  if (window->priv->terms == NULL)
    terminal_window_set_active (window, screen);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  g_return_if_fail (terminal_screen_get_window (screen) == window);
  
  if (window->priv->active_term == screen)
    window->priv->active_term = NULL;

  window->priv->terms = g_list_remove (window->priv->terms, screen);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (profile_set_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (title_changed_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (selection_changed_callback),
                                        window);
  
  terminal_screen_set_window (screen, NULL);
  
  gtk_container_remove (GTK_CONTAINER (window->priv->notebook),
                        screen_get_hbox (screen));

  screen_set_hbox (screen, NULL);
  screen_set_label (screen, NULL);
  screen_set_scrollbar (screen, NULL);
  
  g_object_unref (G_OBJECT (screen));

  /* Put ourselves back in a sane state */
  update_notebook (window);

  if (window->priv->active_term == NULL &&
      window->priv->terms)
    terminal_window_set_active (window, window->priv->terms->data);

  /* Close window if no more terminals */
  if (window->priv->terms == NULL)
    gtk_widget_destroy (GTK_WIDGET (window));
}

GList*
terminal_window_list_screens (TerminalWindow *window)
{
  return g_list_copy (window->priv->terms);
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
  
  if (window->priv->menubar_visible)
    {      
      gtk_box_pack_start (GTK_BOX (window->priv->main_vbox),
                          window->priv->menubar,
                          FALSE, FALSE, 0);
      gtk_widget_show (window->priv->menubar);
    }
  else
    {      
      gtk_container_remove (GTK_CONTAINER (window->priv->main_vbox),
                            window->priv->menubar);
    }
}

gboolean
terminal_window_get_menubar_visible (TerminalWindow *window)
{
  return window->priv->menubar_visible;
}

#define PADDING 2
static void
set_size (GtkWidget *widget)
{
  /* Owen's hack from gnome-terminal */
  ZvtTerm *term;
  GtkWidget *app;
  GtkRequisition toplevel_request;
  GtkRequisition widget_request;
  gint w, h;
  
  g_assert (widget != NULL);
  term = ZVT_TERM (widget);
  
  app = gtk_widget_get_toplevel (widget);
  g_assert (app != NULL);

#if 0  
  gtk_widget_size_request (app, &toplevel_request);
  gtk_widget_size_request (widget, &widget_request);
  
  w = toplevel_request.width - widget_request.width;
  h = toplevel_request.height - widget_request.height;
  
  w += widget->style->xthickness * 2 + PADDING;
  h += widget->style->ythickness * 2;
  
  w += term->charwidth * term->grid_width;
  h += term->charheight * term->grid_height;
  
  gtk_window_resize (GTK_WINDOW (app), w, h);
#else
  gtk_window_set_default_size (GTK_WINDOW (app),
                               term->charwidth * term->grid_width,
                               term->charheight * term->grid_height);
#endif
}

void
terminal_window_set_active (TerminalWindow *window,
                            TerminalScreen *screen)
{
  ZvtTerm *term;
  GtkWidget *widget;
  TerminalProfile *profile;
  
  if (window->priv->active_term == screen)
    return;

  widget = terminal_screen_get_widget (screen);
  term = ZVT_TERM (widget);

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

  gtk_window_set_title (GTK_WINDOW (window),
                        terminal_screen_get_title (screen));

  update_copy_sensitivity (window);
  
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
                                 gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
                                                        screen_get_hbox (screen)));

  /* set initial size of window if window isn't onscreen */
  if (!GTK_WIDGET_MAPPED (window))
    set_size (widget);

  gtk_widget_grab_focus (terminal_screen_get_widget (window->priv->active_term));
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{

  return window->priv->active_term;
}

static void
screen_set_scrollbar (TerminalScreen *screen,
                      GtkWidget      *scrollbar)
{
  g_object_set_data (G_OBJECT (screen),
                     "scrollbar",
                     scrollbar);
}

static void
screen_set_hbox (TerminalScreen *screen,
                 GtkWidget      *hbox)
{
  g_object_set_data (G_OBJECT (screen),
                     "hbox",
                     hbox);
}

static void
screen_set_label (TerminalScreen *screen,
                  GtkWidget      *label)
{
  g_object_set_data (G_OBJECT (screen),
                     "label",
                     label);
}

static GtkWidget*
screen_get_scrollbar (TerminalScreen *screen)
{
  return g_object_get_data (G_OBJECT (screen), "scrollbar");
}

static GtkWidget*
screen_get_hbox (TerminalScreen *screen)
{
  return g_object_get_data (G_OBJECT (screen), "hbox");
}

static GtkWidget*
screen_get_label (TerminalScreen *screen)
{
  return g_object_get_data (G_OBJECT (screen), "label");
}

static TerminalScreen*
find_screen_by_hbox (TerminalWindow *window,
                     GtkWidget      *hbox)
{
  GList *tmp;

  tmp = window->priv->terms;
  while (tmp != NULL)
    {
      if (screen_get_hbox (tmp->data) == hbox)
        return tmp->data;

      tmp = tmp->next;
    }

  return NULL;
}

static void
notebook_page_switched_callback (GtkWidget       *notebook,
                                 GtkNotebookPage *useless_crap,
                                 int              page_num,
                                 TerminalWindow  *window)
{
  GtkWidget* page_widget;
  TerminalScreen *screen;
  
  page_widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
                                           page_num);

  g_assert (page_widget);

  screen = find_screen_by_hbox (window, page_widget);

  g_assert (screen);

  terminal_window_set_active (window, screen);
}

void
terminal_window_update_scrollbar (TerminalWindow *window,
                                  TerminalScreen *screen)
{
  GtkWidget *scrollbar;
  GtkWidget *hbox;
  TerminalProfile *profile;

  profile = terminal_screen_get_profile (screen);

  if (profile == NULL)
    return;
  
  scrollbar = screen_get_scrollbar (screen);
  hbox = screen_get_hbox (screen);
  
  g_object_ref (G_OBJECT (scrollbar));

  if (scrollbar->parent)
    gtk_container_remove (GTK_CONTAINER (hbox), scrollbar);
  
  switch (terminal_profile_get_scrollbar_position (profile))
    {
    case TERMINAL_SCROLLBAR_HIDDEN:
      gtk_widget_hide (scrollbar);
      /* pack just to hold refcount */
      gtk_box_pack_end (GTK_BOX (hbox),
                        scrollbar, FALSE, FALSE, 0);
      break;
    case TERMINAL_SCROLLBAR_RIGHT:
      gtk_box_pack_end (GTK_BOX (hbox),
                        scrollbar, FALSE, FALSE, 0);
      gtk_box_reorder_child (GTK_BOX (hbox), scrollbar, -1);
      gtk_widget_show (scrollbar);
      break;
    case TERMINAL_SCROLLBAR_LEFT:
      gtk_box_pack_start (GTK_BOX (hbox),
                          scrollbar, FALSE, FALSE, 0);
      gtk_box_reorder_child (GTK_BOX (hbox), scrollbar, 0);
      gtk_widget_show (scrollbar);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  g_object_unref (G_OBJECT (scrollbar));
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
  ZvtTerm *term;
  GtkWidget *widget;  

  if (window->priv->active_term == NULL)
    return;
  
  widget = terminal_screen_get_widget (window->priv->active_term);
  term = ZVT_TERM (widget);
  
  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */

  if (term->charwidth != window->priv->old_char_width ||
      term->charheight != window->priv->old_char_height)
    {
      /* padding copied from zvt size request. */
      
      /* FIXME Since we're using xthickness/ythickness we need to change
       * the hints when the theme changes.
       */  
      hints.base_width = (GTK_WIDGET (term)->style->xthickness * 2) + PADDING;
      hints.base_height =  (GTK_WIDGET (term)->style->ythickness * 2);
      
      hints.width_inc = term->charwidth;
      hints.height_inc = term->charheight;
      hints.min_width = hints.base_width + hints.width_inc;
      hints.min_height = hints.base_height + hints.height_inc;
      
      gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                     GTK_WIDGET (term),
                                     &hints,
                                     GDK_HINT_RESIZE_INC |
                                     GDK_HINT_MIN_SIZE |
                                     GDK_HINT_BASE_SIZE);

      window->priv->old_char_width = hints.width_inc;
      window->priv->old_char_height = hints.height_inc;
    }
}

/*
 * Callbacks for the menus
 */

static void
not_implemented (void)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "Didn't implement this item yet, sorry");
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  
  gtk_widget_show (dialog);
}

static void
new_window_callback (GtkWidget      *menuitem,
                     TerminalWindow *window)
{
  terminal_app_new_terminal (terminal_app_get (),
                             terminal_screen_get_profile (window->priv->active_term),
                             NULL,
                             FALSE, FALSE, NULL, NULL);
}

static void
new_tab_callback (GtkWidget      *menuitem,
                  TerminalWindow *window)
{
  terminal_app_new_terminal (terminal_app_get (),
                             terminal_screen_get_profile (window->priv->active_term),
                             window,
                             FALSE, FALSE, NULL, NULL);
}

static void
close_window_callback (GtkWidget      *menuitem,
                       TerminalWindow *window)
{
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
      
      zvt_term_copy_clipboard (ZVT_TERM (widget));
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
      
      zvt_term_paste_clipboard (ZVT_TERM (widget));
    }  
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
    terminal_screen_set_profile (window->priv->active_term, profile);
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
hide_menubar_callback (GtkWidget      *menuitem,
                       TerminalWindow *window)
{
  terminal_window_set_menubar_visible (window, FALSE);
}

static void
reset_callback (GtkWidget      *menuitem,
                TerminalWindow *window)
{
  GtkWidget *widget;

  if (window->priv->active_term)
    {
      widget = terminal_screen_get_widget (window->priv->active_term);
        
      zvt_term_reset (ZVT_TERM (widget), FALSE);
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
        
      zvt_term_reset (ZVT_TERM (widget), TRUE);
    }
}

static void
help_callback (GtkWidget      *menuitem,
               TerminalWindow *window)
{
  not_implemented ();

}

static void
about_callback (GtkWidget      *menuitem,
                TerminalWindow *window)
{
  static GtkWidget *about;
  const char *authors[] = {
    "Havoc Pennington <hp@redhat.com>",
    NULL
  };
  const char *documenters [] = {
    NULL
  };
  const char *translator_credits = "";

  if (about)
    {
      gtk_window_present (GTK_WINDOW (about));
      return;
    }
  
  about = gnome_about_new (PACKAGE, VERSION,
                           _("Copyright 2002 Havoc Pennington"),
                           _("GNOME Terminal"),
                           (const char **)authors,
                           (const char **)documenters,
                           (const char *)translator_credits,
                           NULL);
  g_signal_connect (G_OBJECT (about), "destroy",
		    G_CALLBACK (g_object_add_weak_pointer), &about);
  gtk_widget_show (about);
}
