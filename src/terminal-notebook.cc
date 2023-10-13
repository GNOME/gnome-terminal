/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2008, 2010, 2011, 2012 Christian Persch
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

#include "terminal-debug.hh"
#include "terminal-app.hh"
#include "terminal-intl.hh"
#include "terminal-screen-container.hh"
#include "terminal-tab-label.hh"
#include "terminal-schemas.hh"
#include "terminal-libgsystem.hh"

struct _TerminalNotebook
{
  GtkWidget parent_instance;
  GtkWidget *notebook;
  TerminalScreen *active_screen;
  GtkPolicyType policy;
};

enum
{
  PROP_0,
  PROP_ACTIVE_SCREEN,
  PROP_TAB_POLICY
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
update_tab_visibility (TerminalNotebook *notebook,
                       int               change)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  int new_n_pages;
  gboolean show_tabs;

  if (gtk_widget_in_destruction (GTK_WIDGET (notebook)))
    return;

  new_n_pages = gtk_notebook_get_n_pages (gtk_notebook) + change;
  /* Don't do anything if we're going to have zero pages (and thus close the window) */
  if (new_n_pages == 0)
    return;

  switch (notebook->policy) {
  case GTK_POLICY_ALWAYS:
    show_tabs = TRUE;
    break;
  case GTK_POLICY_AUTOMATIC:
    show_tabs = new_n_pages > 1;
    break;
  case GTK_POLICY_NEVER:
  case GTK_POLICY_EXTERNAL:
  default:
    show_tabs = FALSE;
    break;
  }

  gtk_notebook_set_show_tabs (gtk_notebook, show_tabs);
}

static void
close_button_clicked_cb (TerminalTabLabel *tab_label,
                         gpointer          user_data)
{
  TerminalScreen *screen;
  TerminalNotebook *notebook;

  screen = terminal_tab_label_get_screen (tab_label);

  /* notebook is not passed as user_data because it can change during DND
   * and the close button is not notified about that, see bug 731998.
   */
  notebook = TERMINAL_NOTEBOOK (gtk_widget_get_ancestor (GTK_WIDGET (screen),
                                                         TERMINAL_TYPE_NOTEBOOK));

  if (notebook != nullptr)
    g_signal_emit_by_name (notebook, "screen-close-request", screen);
}

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
  GtkWidget *screen_container, *tab_label;
  GtkNotebook *gtk_notebook;
  GtkNotebookPage *page;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (screen)) == nullptr);

  gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  screen_container = terminal_screen_container_new (screen);
  gtk_widget_show (screen_container);

  update_tab_visibility (notebook, +1);

  tab_label = terminal_tab_label_new (screen);
  g_signal_connect (tab_label, "close-button-clicked",
                    G_CALLBACK (close_button_clicked_cb), nullptr);

  gtk_notebook_insert_page (gtk_notebook, screen_container, tab_label, position);
  page = gtk_notebook_get_page (gtk_notebook, screen_container);

  g_object_set (page,
                "tab-expand", TRUE,
                "tab-fill", TRUE,
                nullptr);

  gtk_notebook_set_tab_reorderable (gtk_notebook, screen_container, TRUE);
#if 0
  gtk_notebook_set_tab_detachable (gtk_notebook, screen_container, TRUE);
#endif
}

void
terminal_notebook_remove_screen (TerminalNotebook *notebook,
                                 TerminalScreen   *screen)
{
  TerminalScreenContainer *screen_container;
  GtkNotebook *gtk_notebook;
  int page_num;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (notebook));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_is_ancestor (GTK_WIDGET (screen), GTK_WIDGET (notebook)));

  update_tab_visibility (notebook, -1);

  gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  screen_container = terminal_screen_container_get_from_screen (screen);
  page_num = gtk_notebook_page_num (gtk_notebook, GTK_WIDGET (screen_container));
  gtk_notebook_remove_page (gtk_notebook, page_num);
}

TerminalScreen *
terminal_notebook_get_active_screen (TerminalNotebook *notebook)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  GtkWidget *widget;

  widget = gtk_notebook_get_nth_page (gtk_notebook, gtk_notebook_get_current_page (gtk_notebook));
  return terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (widget));
}

void
terminal_notebook_set_active_screen (TerminalNotebook *notebook,
                                     TerminalScreen   *screen)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  TerminalScreenContainer *screen_container;
  GtkWidget *widget;

  screen_container = terminal_screen_container_get_from_screen (screen);
  widget = GTK_WIDGET (screen_container);

  gtk_notebook_set_current_page (gtk_notebook,
                                 gtk_notebook_page_num (gtk_notebook, widget));
}

GList *
terminal_notebook_list_screen_containers (TerminalNotebook *notebook)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  GQueue queue = G_QUEUE_INIT;
  int n_pages;

  /* We are trusting that GtkNotebook will return pages in order */

  n_pages = gtk_notebook_get_n_pages (gtk_notebook);

  for (int i = 0; i < n_pages; i++) {
    GtkWidget *child = gtk_notebook_get_nth_page (gtk_notebook, i);

    g_queue_push_tail (&queue, child);
  }

  return queue.head;
}

GList *
terminal_notebook_list_screens (TerminalNotebook *notebook)
{
  GList *list, *l;

  list = terminal_notebook_list_screen_containers (notebook);
  for (l = list; l != nullptr; l = l->next)
    l->data = terminal_screen_container_get_screen ((TerminalScreenContainer *) l->data);

  return list;
}

int
terminal_notebook_get_n_screens (TerminalNotebook *notebook)
{
  return gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook->notebook));
}

int
terminal_notebook_get_active_screen_num (TerminalNotebook *notebook)
{
  return gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook->notebook));
}

void
terminal_notebook_set_active_screen_num (TerminalNotebook *notebook,
                                         int               position)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);

  gtk_notebook_set_current_page (gtk_notebook, position);
}

void
terminal_notebook_reorder_screen (TerminalNotebook *notebook,
                                  TerminalScreen   *screen,
                                  int               new_position)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  GtkWidget *child;
  int n, pos;

  g_return_if_fail (new_position == 1 || new_position == -1);

  child = GTK_WIDGET (terminal_screen_container_get_from_screen (screen));
  n = gtk_notebook_get_n_pages (gtk_notebook);
  pos = gtk_notebook_page_num (gtk_notebook, child);

  pos += new_position;
  gtk_notebook_reorder_child (gtk_notebook, child,
                              pos < 0 ? n - 1 : pos < n ? pos : 0);
}

G_DEFINE_FINAL_TYPE (TerminalNotebook, terminal_notebook, GTK_TYPE_WIDGET)

/* GtkNotebookClass impl */

static void
terminal_notebook_switch_page (GtkNotebook      *gtk_notebook,
                               GtkWidget        *child,
                               guint             page_num,
                               TerminalNotebook *notebook)
{
  TerminalScreen *screen, *old_active_screen;

  screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (child));

  old_active_screen = notebook->active_screen;
  if (screen == old_active_screen)
    return;

  /* Workaround to remove gtknotebook's feature of computing its size based on
   * all pages. When the widget is hidden, its size will not be taken into
   * account.
   * FIXME!
   */
//   if (old_active_screen)
//     gtk_widget_hide (GTK_WIDGET (terminal_screen_container_get_from_screen (old_active_screen)));
  /* Make sure that the widget is no longer hidden due to the workaround */
//   if (child)
//     gtk_widget_show (child);
  if (old_active_screen)
    gtk_widget_hide (GTK_WIDGET (old_active_screen));
  if (screen)
    gtk_widget_show (GTK_WIDGET (screen));

  notebook->active_screen = screen;

  g_signal_emit_by_name (notebook, "screen-switched", old_active_screen, screen);
  g_object_notify (G_OBJECT (notebook), "active-screen");

  if (screen != nullptr)
    gtk_widget_grab_focus (GTK_WIDGET (screen));
}

static void
terminal_notebook_page_added (GtkNotebook      *gtk_notebook,
                              GtkWidget        *child,
                              guint             page_num,
                              TerminalNotebook *notebook)
{
  update_tab_visibility (notebook, 0);
  g_signal_emit_by_name (notebook, "screen-added",
                         terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (child)));
}

static void
terminal_notebook_page_removed (GtkNotebook      *gtk_notebook,
                                GtkWidget        *child,
                                guint             page_num,
                                TerminalNotebook *notebook)
{
  update_tab_visibility (notebook, 0);
 g_signal_emit_by_name (notebook, "screen-removed", terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (child)));
}

static void
terminal_notebook_page_reordered (GtkNotebook      *gtk_notebook,
                                  GtkWidget        *child,
                                  guint             page_num,
                                  TerminalNotebook *notebook)
{
  g_signal_emit_by_name (notebook, "screens-reordered");
}

/* GtkWidgetClass impl */

static gboolean
terminal_notebook_grab_focus (GtkWidget *widget)
{
  TerminalScreen *screen;

  screen = terminal_notebook_get_active_screen (TERMINAL_NOTEBOOK (widget));
  return gtk_widget_grab_focus (GTK_WIDGET (screen));
}

/* GObjectClass impl */

static void
terminal_notebook_init (TerminalNotebook *notebook)
{
  notebook->active_screen = nullptr;
  notebook->policy = GTK_POLICY_AUTOMATIC;

  notebook->notebook = gtk_notebook_new ();
  gtk_widget_set_parent (notebook->notebook, GTK_WIDGET (notebook));

  g_signal_connect_object (notebook->notebook,
                           "switch-page",
                           G_CALLBACK (terminal_notebook_switch_page),
                           notebook,
                           G_CONNECT_AFTER);
  g_signal_connect_object (notebook->notebook,
                           "page-added",
                           G_CALLBACK (terminal_notebook_page_added),
                           notebook,
                           G_CONNECT_AFTER);
  g_signal_connect_object (notebook->notebook,
                           "page-removed",
                           G_CALLBACK (terminal_notebook_page_removed),
                           notebook,
                           G_CONNECT_AFTER);
  g_signal_connect_object (notebook->notebook,
                           "page-reordered",
                           G_CALLBACK (terminal_notebook_page_reordered),
                           notebook,
                           G_CONNECT_AFTER);
}

static void
terminal_notebook_dispose (GObject *object)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (object);

  if (notebook->notebook != nullptr) {
    gtk_widget_unparent (GTK_WIDGET (notebook->notebook));
    notebook->notebook = nullptr;
  }

  G_OBJECT_CLASS (terminal_notebook_parent_class)->dispose (object);
}

static void
terminal_notebook_constructed (GObject *object)
{
  GSettings *settings;
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (object);
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook->notebook);

  G_OBJECT_CLASS (terminal_notebook_parent_class)->constructed (object);

  settings = terminal_app_get_global_settings (terminal_app_get ());

  update_tab_visibility (notebook, 0);
  g_settings_bind (settings,
                   TERMINAL_SETTING_TAB_POLICY_KEY,
                   object,
                   "tab-policy",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET |
				      G_SETTINGS_BIND_NO_SENSITIVITY));

  g_settings_bind (settings,
                   TERMINAL_SETTING_TAB_POSITION_KEY,
                   gtk_notebook,
                   "tab-pos",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET |
				      G_SETTINGS_BIND_NO_SENSITIVITY));

  gtk_notebook_set_scrollable (gtk_notebook, TRUE);
  gtk_notebook_set_show_border (gtk_notebook, FALSE);
  gtk_notebook_set_group_name (gtk_notebook, I_("gnome-terminal-window"));
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
    case PROP_TAB_POLICY:
      g_value_set_enum (value, terminal_notebook_get_tab_policy (notebook));
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
    case PROP_TAB_POLICY:
      terminal_notebook_set_tab_policy (notebook, GtkPolicyType(g_value_get_enum (value)));
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

  gobject_class->constructed = terminal_notebook_constructed;
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
     PROP_TAB_POLICY,
     g_param_spec_enum ("tab-policy", nullptr, nullptr,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
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

  signals[SCREEN_ADDED] =
    g_signal_new (I_("screen-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_SCREEN);

  signals[SCREEN_ADDED] =
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
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
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
terminal_notebook_set_tab_policy (TerminalNotebook *notebook,
                                  GtkPolicyType     policy)
{
  if (notebook->policy == policy)
    return;

  notebook->policy = policy;
  update_tab_visibility (notebook, 0);

  g_object_notify (G_OBJECT (notebook), "tab-policy");
}

GtkPolicyType
terminal_notebook_get_tab_policy (TerminalNotebook *notebook)
{
  return notebook->policy;
}

GtkWidget *
terminal_notebook_get_action_box (TerminalNotebook *notebook,
                                  GtkPackType       pack_type)
{
  GtkNotebook *gtk_notebook;
  GtkWidget *box, *inner_box;

  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), nullptr);

  gtk_notebook = GTK_NOTEBOOK (notebook->notebook);
  box = gtk_notebook_get_action_widget (gtk_notebook, pack_type);
  if (box != nullptr)
    return gtk_widget_get_first_child (box);

  /* Create container for the buttons */
  box = (GtkWidget *)g_object_new (GTK_TYPE_BOX,
                                   "orientation", GTK_ORIENTATION_VERTICAL,
                                   "margin-top", ACTION_AREA_BORDER_WIDTH,
                                   "margin-bottom", ACTION_AREA_BORDER_WIDTH,
                                   "margin-start", ACTION_AREA_BORDER_WIDTH,
                                   "margin-end", ACTION_AREA_BORDER_WIDTH,
                                   nullptr);

  inner_box = (GtkWidget *)g_object_new (GTK_TYPE_BOX,
                                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                                         "spacing", ACTION_BUTTON_SPACING,
                                         nullptr);
  gtk_box_prepend (GTK_BOX (box), inner_box);

  gtk_notebook_set_action_widget (gtk_notebook, box, pack_type);

  return inner_box;
}

GtkNotebook *
terminal_notebook_get_notebook (TerminalNotebook *notebook)
{
  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), NULL);

  return GTK_NOTEBOOK (notebook->notebook);
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
