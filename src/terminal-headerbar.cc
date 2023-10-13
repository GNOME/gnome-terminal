/*
 * Copyright © 2018 Christian Persch
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

#include <glib/gi18n.h>

#include <adwaita.h>

#include "terminal-headerbar.hh"
#include "terminal-app.hh"
#include "terminal-libgsystem.hh"

struct _TerminalHeaderbar
{
  GtkWidget parent_instance;
  AdwHeaderBar *headerbar;
  GtkWidget *profilebutton;
  GtkWidget *new_tab_button;
  GtkWidget *menubutton;
  GtkPopoverMenu *profiles_popover_menu;
  gulong items_changed_handler;
};

G_DEFINE_FINAL_TYPE (TerminalHeaderbar, terminal_headerbar, GTK_TYPE_WIDGET)

static void
profilemenu_items_changed_cb (GMenuModel *menu,
                              int position G_GNUC_UNUSED,
                              int removed G_GNUC_UNUSED,
                              int added G_GNUC_UNUSED,
                              TerminalHeaderbar *headerbar)
{
  gtk_widget_set_visible (headerbar->new_tab_button,
                          g_menu_model_get_n_items (menu) == 0);
  gtk_widget_set_visible (headerbar->profilebutton,
                          g_menu_model_get_n_items (menu) > 0);
}

/* Class implementation */

static void
terminal_headerbar_init (TerminalHeaderbar *headerbar)
{

  GtkWidget *widget = GTK_WIDGET (headerbar);
  TerminalApp *app = terminal_app_get ();
  GMenuModel *profilemenu;

  gtk_widget_init_template (widget);

  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (headerbar->menubutton),
                                  terminal_app_get_headermenu (app));

  profilemenu = terminal_app_get_profilemenu (app);
  adw_split_button_set_menu_model (ADW_SPLIT_BUTTON (headerbar->profilebutton),
                                  profilemenu);

  headerbar->items_changed_handler =
    g_signal_connect_object (profilemenu,
                             "items-changed",
                             G_CALLBACK (profilemenu_items_changed_cb),
                             headerbar,
                             G_CONNECT_DEFAULT);
  profilemenu_items_changed_cb (profilemenu, 0, 0, 0, headerbar);
}

static void
terminal_headerbar_dispose (GObject *object)
{
  TerminalHeaderbar *headerbar = TERMINAL_HEADERBAR (object);
  TerminalApp *app = terminal_app_get ();

  gtk_widget_dispose_template (GTK_WIDGET (headerbar), TERMINAL_TYPE_HEADERBAR);

  GMenuModel *profilemenu = terminal_app_get_profilemenu (app);
  g_clear_signal_handler (&headerbar->items_changed_handler, profilemenu);

  G_OBJECT_CLASS (terminal_headerbar_parent_class)->dispose (object);
}

static void
terminal_headerbar_class_init (TerminalHeaderbarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = terminal_headerbar_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/headerbar.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, TerminalHeaderbar, headerbar);
  gtk_widget_class_bind_template_child (widget_class, TerminalHeaderbar, menubutton);
  gtk_widget_class_bind_template_child (widget_class, TerminalHeaderbar, profilebutton);
  gtk_widget_class_bind_template_child (widget_class, TerminalHeaderbar, new_tab_button);
}

/**
 * terminal_headerbar_new:
 *
 * Returns: a new #TerminalHeaderbar
 */
GtkWidget *
terminal_headerbar_new (void)
{
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_HEADERBAR, nullptr));
}
