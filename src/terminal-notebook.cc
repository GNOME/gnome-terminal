/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2008, 2010, 2011, 2012 Christian Persch
 * Copyright © 2023 Christian Hergert
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

#include <config.h>

#include "terminal-notebook.hh"

#include <gtk/gtk.h>

#include <adwaita.h>

#include "terminal-debug.hh"
#include "terminal-app.hh"
#include "terminal-intl.hh"
#include "terminal-tab.hh"
#include "terminal-tab-label.hh"
#include "terminal-schemas.hh"
#include "terminal-libgsystem.hh"
#include "terminal-util.hh"

struct _TerminalNotebook
{
  GtkWidget       parent_instance;
  AdwTabView     *tab_view;
  GMenu          *page_menu;
  TerminalScreen *active_screen;
};

enum
{
  PROP_0,
  PROP_ACTIVE_SCREEN,
  PROP_ACTIVE_SCREEN_NUM,
};

enum {
  SCREEN_ADDED,
  SCREEN_REMOVED,
  SCREEN_SWITCHED,
  SCREENS_REORDERED,
  SCREEN_CLOSE_REQUEST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

#define ACTION_AREA_BORDER_WIDTH (2)
#define ACTION_BUTTON_SPACING (6)

/* helper functions */

static void
remove_binding (GtkWidgetClass  *widget_class,
                guint            keysym,
                GdkModifierType  modifier)
{
  GtkShortcut *shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (keysym, modifier),
                                            g_object_ref (gtk_nothing_action_get ()));
  gtk_widget_class_add_shortcut (widget_class, shortcut);
  g_object_unref (shortcut);
}

static void
remove_reorder_bindings (GtkWidgetClass *widget_class,
                         guint           keysym)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  remove_binding (widget_class, keysym, GDK_ALT_MASK);
  remove_binding (widget_class, keypad_keysym, GDK_ALT_MASK);
}

void
terminal_notebook_add_screen (TerminalNotebook *notebook,
                              TerminalScreen   *screen,
                              int               position)
{
  AdwTabPage *page;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (screen)) == nullptr);

  if (position < 0)
    position = adw_tab_view_get_n_pages (notebook->tab_view);

  page = adw_tab_view_insert (notebook->tab_view,
                              terminal_tab_new (screen),
                              position);

  g_object_bind_property (screen, "title",
                          page, "title",
                          G_BINDING_SYNC_CREATE);
}

void
terminal_notebook_confirm_close (TerminalNotebook *notebook,
                                 TerminalScreen   *screen,
                                 gboolean          confirm)
{
  TerminalTab *tab;
  AdwTabPage *page;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  tab = terminal_tab_get_from_screen (screen);
  page = adw_tab_view_get_page (notebook->tab_view, GTK_WIDGET (tab));

  if (page != nullptr)
    adw_tab_view_close_page_finish (notebook->tab_view, page, confirm);
}

void
terminal_notebook_remove_screen (TerminalNotebook *notebook,
                                 TerminalScreen   *screen)
{
  TerminalTab *tab;
  AdwTabPage *page;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_is_ancestor (GTK_WIDGET (screen), GTK_WIDGET (notebook)));

  tab = terminal_tab_get_from_screen (screen);
  page = adw_tab_view_get_page (notebook->tab_view, GTK_WIDGET (tab));

  if (page != nullptr)
    adw_tab_view_close_page (notebook->tab_view, page);
}

TerminalScreen *
terminal_notebook_get_active_screen (TerminalNotebook *notebook)
{
  AdwTabPage *page;
  GtkWidget *child;

  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), nullptr);

  page = adw_tab_view_get_selected_page (notebook->tab_view);
  if (page == nullptr)
    return nullptr;

  child = adw_tab_page_get_child (page);
  if (child == nullptr)
    return nullptr;

  return terminal_tab_get_screen (TERMINAL_TAB (child));
}

void
terminal_notebook_set_active_screen (TerminalNotebook *notebook,
                                     TerminalScreen   *screen)
{
  TerminalTab *tab;
  AdwTabPage *page;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_is_ancestor (GTK_WIDGET (screen), GTK_WIDGET (notebook)));

  tab = terminal_tab_get_from_screen (screen);
  page = adw_tab_view_get_page (notebook->tab_view, GTK_WIDGET (tab));
  adw_tab_view_set_selected_page (notebook->tab_view, page);
}

GList *
terminal_notebook_list_tabs (TerminalNotebook *notebook)
{
  g_autoptr(GtkSelectionModel) pages = nullptr;
  GQueue queue = G_QUEUE_INIT;
  guint n_items;

  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  pages = adw_tab_view_get_pages (notebook->tab_view);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (pages));

  for (guint i = 0;i < n_items; i++) {
    g_autoptr(AdwTabPage) page = ADW_TAB_PAGE (g_list_model_get_item (G_LIST_MODEL (pages), i));

    g_queue_push_tail (&queue, adw_tab_page_get_child (page));
  }

  return queue.head;
}

GList *
terminal_notebook_list_screens (TerminalNotebook *notebook)
{
  GList *list, *l;

  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), nullptr);

  list = terminal_notebook_list_tabs (notebook);
  for (l = list; l != nullptr; l = l->next)
    l->data = terminal_tab_get_screen ((TerminalTab *) l->data);

  return list;
}

int
terminal_notebook_get_n_screens (TerminalNotebook *notebook)
{
  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), -1);

  return adw_tab_view_get_n_pages (notebook->tab_view);
}

int
terminal_notebook_get_active_screen_num (TerminalNotebook *notebook)
{
  AdwTabPage *page;

  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), -1);

  page = adw_tab_view_get_selected_page (notebook->tab_view);
  if (page == nullptr)
    return -1;

  return adw_tab_view_get_page_position (notebook->tab_view, page);
}

void
terminal_notebook_set_active_screen_num (TerminalNotebook *notebook,
                                         int               position)
{
  g_autoptr(GtkSelectionModel) pages = nullptr;
  g_autoptr(AdwTabPage) page = nullptr;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));

  pages = adw_tab_view_get_pages (notebook->tab_view);
  page = ADW_TAB_PAGE (g_list_model_get_item (G_LIST_MODEL (pages), unsigned(position)));
  if (page == nullptr)
    return;

  adw_tab_view_set_selected_page (notebook->tab_view, page);
}

void
terminal_notebook_reorder_screen (TerminalNotebook *notebook,
                                  TerminalScreen   *screen,
                                  int               new_position)
{
  TerminalTab *tab;
  AdwTabPage *page;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (new_position == 1 || new_position == -1);

  tab = terminal_tab_get_from_screen (screen);
  page = adw_tab_view_get_page (notebook->tab_view, GTK_WIDGET (tab));
  new_position += adw_tab_view_get_page_position (notebook->tab_view, page);

  if (new_position < adw_tab_view_get_n_pages (notebook->tab_view))
    adw_tab_view_reorder_page (notebook->tab_view, page, new_position);
}

static GObject *
terminal_notebook_get_internal_child (GtkBuildable *buildable,
                                      GtkBuilder   *builder,
                                      const char   *name)
{
  if (g_strcmp0 (name, "tab_view") == 0)
    return G_OBJECT (TERMINAL_NOTEBOOK (buildable)->tab_view);

  return nullptr;
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = terminal_notebook_get_internal_child;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (TerminalNotebook, terminal_notebook, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

/* GtkNotebookClass impl */

static void
terminal_notebook_switch_page (AdwTabView       *tab_view,
                               GParamSpec       *pspec,
                               TerminalNotebook *notebook)
{
  TerminalScreen *old_active_screen;
  TerminalScreen *screen = nullptr;
  AdwTabPage *page;

  g_assert (ADW_TAB_VIEW (tab_view));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  page = adw_tab_view_get_selected_page (tab_view);
  if (page != nullptr) {
    GtkWidget *child = adw_tab_page_get_child (page);
    screen = terminal_tab_get_screen (TERMINAL_TAB (child));
  }

  old_active_screen = notebook->active_screen;
  if (screen == old_active_screen)
    return;

  notebook->active_screen = screen;

  g_signal_emit (notebook, signals[SCREEN_SWITCHED], 0, old_active_screen, screen);
  g_object_notify (G_OBJECT (notebook), "active-screen");

  if (screen != nullptr)
    gtk_widget_grab_focus (GTK_WIDGET (screen));
}

static void
terminal_notebook_page_added (AdwTabView       *tab_view,
                              AdwTabPage       *page,
                              int               position,
                              TerminalNotebook *notebook)
{
  GtkWidget *child;

  g_assert (ADW_IS_TAB_VIEW (tab_view));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  child = adw_tab_page_get_child (page);
  g_signal_emit (notebook, signals[SCREEN_ADDED], 0,
                 terminal_tab_get_screen (TERMINAL_TAB (child)));
}

static void
terminal_notebook_page_removed (AdwTabView       *tab_view,
                                AdwTabPage       *page,
                                int               position,
                                TerminalNotebook *notebook)
{
  GtkWidget *child;

  g_assert (ADW_IS_TAB_VIEW (tab_view));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  child = adw_tab_page_get_child (page);
  g_signal_emit (notebook, signals[SCREEN_REMOVED], 0,
                 terminal_tab_get_screen (TERMINAL_TAB (child)));
}

static void
terminal_notebook_page_reordered (AdwTabView       *tab_view,
                                  AdwTabPage       *page,
                                  int               position,
                                  TerminalNotebook *notebook)
{
  g_assert (ADW_IS_TAB_VIEW (tab_view));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  g_signal_emit (notebook, signals[SCREENS_REORDERED], 0);
}

static gboolean
terminal_notebook_close_page (AdwTabView       *tab_view,
                              AdwTabPage       *page,
                              TerminalNotebook *notebook)
{
  TerminalTab *tab;
  TerminalScreen *screen;
  gboolean ret = GDK_EVENT_PROPAGATE;

  g_assert (ADW_IS_TAB_VIEW (tab_view));
  g_assert (ADW_IS_TAB_PAGE (page));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  tab = TERMINAL_TAB (adw_tab_page_get_child (page));
  screen = terminal_tab_get_screen (tab);

  g_signal_emit (notebook, signals[SCREEN_CLOSE_REQUEST], 0, screen, &ret);

  return ret;
}

/* GtkWidgetClass impl */

static gboolean
terminal_notebook_grab_focus (GtkWidget *widget)
{
  TerminalScreen *screen;

  screen = terminal_notebook_get_active_screen (TERMINAL_NOTEBOOK (widget));
  return gtk_widget_grab_focus (GTK_WIDGET (screen));
}

static void
terminal_notebook_setup_menu (AdwTabView       *tab_view,
                              AdwTabPage       *page,
                              TerminalNotebook *notebook)
{
  TerminalScreen *screen;
  GtkWidget *child;

  g_assert (ADW_IS_TAB_VIEW (tab_view));
  g_assert (!page || ADW_IS_TAB_PAGE (page));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  if (page != nullptr &&
      (child = adw_tab_page_get_child (page)) &&
      (screen = terminal_tab_get_screen (TERMINAL_TAB (child))) &&
      !terminal_screen_is_active (screen)) {
    terminal_notebook_set_active_screen (notebook, screen);
  }
}

/* GObjectClass impl */

static void
terminal_notebook_init (TerminalNotebook *notebook)
{
  gtk_widget_init_template (GTK_WIDGET (notebook));

  adw_tab_view_set_default_icon(notebook->tab_view,
                                terminal_app_get_default_icon_symbolic(terminal_app_get()));
}

static void
terminal_notebook_dispose (GObject *object)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (object);

  gtk_widget_dispose_template (GTK_WIDGET (notebook), TERMINAL_TYPE_NOTEBOOK);

  notebook->active_screen = nullptr;

  G_OBJECT_CLASS (terminal_notebook_parent_class)->dispose (object);
}

static void
terminal_notebook_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (object);

  switch (prop_id) {
    case PROP_ACTIVE_SCREEN:
      g_value_set_object (value, terminal_notebook_get_active_screen (notebook));
      break;
    case PROP_ACTIVE_SCREEN_NUM:
      g_value_set_int (value, terminal_notebook_get_active_screen_num (notebook));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_notebook_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (object);

  switch (prop_id) {
    case PROP_ACTIVE_SCREEN:
      terminal_notebook_set_active_screen (notebook, (TerminalScreen*)g_value_get_object (value));
      break;
    case PROP_ACTIVE_SCREEN_NUM:
      terminal_notebook_set_active_screen_num (notebook, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_notebook_class_init (TerminalNotebookClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = terminal_notebook_dispose;
  gobject_class->get_property = terminal_notebook_get_property;
  gobject_class->set_property = terminal_notebook_set_property;

  widget_class->grab_focus = terminal_notebook_grab_focus;

  g_object_class_install_property
    (gobject_class,
     PROP_ACTIVE_SCREEN,
     g_param_spec_object ("active-screen", nullptr, nullptr,
                          TERMINAL_TYPE_SCREEN,
                          GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property
    (gobject_class,
     PROP_ACTIVE_SCREEN_NUM,
     g_param_spec_int ("active-screen-num", nullptr, nullptr,
                       -1, G_MAXINT, -1,
                       GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  signals[SCREEN_ADDED] =
    g_signal_new (I_("screen-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_SCREEN);

  signals[SCREEN_REMOVED] =
    g_signal_new (I_("screen-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_SCREEN);

  signals[SCREEN_SWITCHED] =
    g_signal_new (I_("screen-switched"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE,
                  2, TERMINAL_TYPE_SCREEN, TERMINAL_TYPE_SCREEN);

  signals[SCREENS_REORDERED] =
    g_signal_new (I_("screens-reordered"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  signals[SCREEN_CLOSE_REQUEST] =
    g_signal_new (I_("screen-close-request"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled, nullptr,
                  nullptr,
                  G_TYPE_BOOLEAN,
                  1, TERMINAL_TYPE_SCREEN);


  /* Remove unwanted and interfering keybindings */
  remove_binding (widget_class, GDK_KEY_Page_Up, GdkModifierType(GDK_CONTROL_MASK));
  remove_binding (widget_class, GDK_KEY_Page_Up, GdkModifierType(GDK_CONTROL_MASK | GDK_ALT_MASK));
  remove_binding (widget_class, GDK_KEY_Page_Down, GdkModifierType(GDK_CONTROL_MASK));
  remove_binding (widget_class, GDK_KEY_Page_Down, GdkModifierType(GDK_CONTROL_MASK | GDK_ALT_MASK));
  remove_reorder_bindings (widget_class, GDK_KEY_Up);
  remove_reorder_bindings (widget_class, GDK_KEY_Down);
  remove_reorder_bindings (widget_class, GDK_KEY_Left);
  remove_reorder_bindings (widget_class, GDK_KEY_Right);
  remove_reorder_bindings (widget_class, GDK_KEY_Home);
  remove_reorder_bindings (widget_class, GDK_KEY_Home);
  remove_reorder_bindings (widget_class, GDK_KEY_End);
  remove_reorder_bindings (widget_class, GDK_KEY_End);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/notebook.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalNotebook, tab_view);
  gtk_widget_class_bind_template_child (widget_class, TerminalNotebook, page_menu);

  gtk_widget_class_bind_template_callback (widget_class, terminal_notebook_setup_menu);
  gtk_widget_class_bind_template_callback (widget_class, terminal_notebook_switch_page);
  gtk_widget_class_bind_template_callback (widget_class, terminal_notebook_page_added);
  gtk_widget_class_bind_template_callback (widget_class, terminal_notebook_page_removed);
  gtk_widget_class_bind_template_callback (widget_class, terminal_notebook_page_reordered);
  gtk_widget_class_bind_template_callback (widget_class, terminal_notebook_close_page);
}

/* public API */

/**
 * terminal_notebook_new:
 *
 * Returns: (transfer full): a new #TerminalNotebook
 */
GtkWidget *
terminal_notebook_new (void)
{
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_NOTEBOOK, nullptr));
}

void
terminal_notebook_change_screen (TerminalNotebook *notebook,
                                 int               change)
{
  int active, n;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (change == -1 || change == 1);

  n = terminal_notebook_get_n_screens (notebook);
  active = terminal_notebook_get_active_screen_num (notebook);

  active += change;
  if (active < 0)
    active = n - 1;
  else if (active >= n)
    active = 0;

  terminal_notebook_set_active_screen_num (notebook, active);
}

AdwTabView *
terminal_notebook_get_tab_view (TerminalNotebook *notebook)
{
  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), nullptr);

  return notebook->tab_view;
}
