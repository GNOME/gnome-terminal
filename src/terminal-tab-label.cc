/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2007, 2008 Christian Persch
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "terminal-intl.hh"
#include "terminal-tab-label.hh"
#include "terminal-icon-button.hh"
#include "terminal-window.hh"

#define SPACING (4)

struct _TerminalTabLabel
{
  GtkWidget parent_instance;
  TerminalScreen *screen;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *close_button;
  gboolean bold;
  GtkPositionType tab_pos;
};

enum
{
  PROP_0,
  PROP_SCREEN
};

enum
{
  CLOSE_BUTTON_CLICKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (TerminalTabLabel, terminal_tab_label, GTK_TYPE_WIDGET);

/* helper functions */

static void
close_button_clicked_cb (GtkWidget *widget,
                         TerminalTabLabel *tab_label)
{
  g_signal_emit (tab_label, signals[CLOSE_BUTTON_CLICKED], 0);
}

static void
sync_tab_label (TerminalScreen *screen,
                GParamSpec *pspec,
                GtkWidget *label)
{
  GtkWidget *hbox;
  const char *title;
  TerminalWindow *window;

  title = terminal_screen_get_title (screen);
  hbox = gtk_widget_get_parent (label);

  gtk_label_set_text (GTK_LABEL (label),
                      title && title[0] ? title : _("Terminal"));

  gtk_widget_set_tooltip_text (hbox, title);

  /* This call updates the window size: bug 732588.
   * FIXMEchpe: This is probably a GTK+ bug, should get them fix it.
   */
  window = TERMINAL_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (label),
                                                     TERMINAL_TYPE_WINDOW));
  if (window != nullptr)
    terminal_window_update_size (window);
}

static void
notify_tab_pos_cb (GtkNotebook *notebook,
                   GParamSpec *pspec G_GNUC_UNUSED,
                   TerminalTabLabel *label)
{
  GtkPositionType pos;

  pos = gtk_notebook_get_tab_pos (notebook);
  if (pos == label->tab_pos)
    return;

  label->tab_pos = pos;

  switch (pos) {
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      gtk_widget_hide (label->close_button);
      break;
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      gtk_widget_show (label->close_button);
      break;
  }
}

/* public functions */

/* Class implementation */

static void
terminal_tab_label_unroot (GtkWidget *widget)
{
  GtkWidget *old_parent = gtk_widget_get_parent (widget);

  if (GTK_IS_NOTEBOOK (old_parent)) {
    g_signal_handlers_disconnect_by_func (old_parent, 
                                          (void*)notify_tab_pos_cb, 
                                          widget);
  }

  GTK_WIDGET_CLASS (terminal_tab_label_parent_class)->unroot (widget);
}

static void
terminal_tab_label_root (GtkWidget *widget)
{
  GtkWidget *parent;

  GTK_WIDGET_CLASS (terminal_tab_label_parent_class)->root (widget);

  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_NOTEBOOK (parent)) {
    notify_tab_pos_cb (GTK_NOTEBOOK (parent), nullptr, TERMINAL_TAB_LABEL (widget));
    g_signal_connect (parent, "notify::tab-pos", 
                      G_CALLBACK (notify_tab_pos_cb), widget);
  }
}

static void
terminal_tab_label_measure (GtkWidget *widget,
                            GtkOrientation orientation,
                            int for_size,
                            int *minimum,
                            int *natural,
                            int *minimum_baseline,
                            int *natural_baseline)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    if (tab_label->tab_pos == GTK_POS_LEFT || tab_label->tab_pos == GTK_POS_RIGHT) {
      *natural = 160;
      *minimum = 160;
      return;
    }
  }

  GTK_WIDGET_CLASS (terminal_tab_label_parent_class)->measure (widget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
terminal_tab_label_init (TerminalTabLabel *tab_label)
{
  tab_label->tab_pos = (GtkPositionType) -1; /* invalid */

  tab_label->hbox = gtk_center_box_new ();
  gtk_widget_set_parent (tab_label->hbox, GTK_WIDGET (tab_label));
}

static void
terminal_tab_label_constructed (GObject *object)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);
  GtkWidget *label, *close_button;

  G_OBJECT_CLASS (terminal_tab_label_parent_class)->constructed (object);

  g_assert (tab_label->screen != nullptr);
  
  tab_label->label = label = gtk_label_new (nullptr);
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
  gtk_widget_set_margin_start  (label, SPACING);
  gtk_widget_set_margin_end    (label, SPACING);
  gtk_widget_set_margin_top    (label, 0);
  gtk_widget_set_margin_bottom (label, 0);

  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);

  gtk_center_box_set_center_widget (GTK_CENTER_BOX (tab_label->hbox), label);

  tab_label->close_button = close_button = terminal_close_button_new ();
  gtk_widget_set_tooltip_text (close_button, _("Close tab"));
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (tab_label->hbox), close_button);

  sync_tab_label (tab_label->screen, nullptr, label);
  g_signal_connect (tab_label->screen, "notify::title",
                    G_CALLBACK (sync_tab_label), label);

  g_signal_connect (close_button, "clicked",
		    G_CALLBACK (close_button_clicked_cb), tab_label);
}

static void
terminal_tab_label_dispose (GObject *object)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);

  if (tab_label->screen != nullptr) {
    g_signal_handlers_disconnect_by_func (tab_label->screen,
                                          (void*)sync_tab_label,
                                          tab_label->label);
    g_object_unref (tab_label->screen);
    tab_label->screen = nullptr;
  }

  if (tab_label->hbox != nullptr) {
    gtk_widget_unparent (tab_label->hbox);
    tab_label->hbox = nullptr;
  }

  G_OBJECT_CLASS (terminal_tab_label_parent_class)->dispose (object);
}

static void
terminal_tab_label_finalize (GObject *object)
{
//   TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);

  G_OBJECT_CLASS (terminal_tab_label_parent_class)->finalize (object);
}

static void
terminal_tab_label_get_property (GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);

  switch (prop_id) {
    case PROP_SCREEN:
      g_value_set_object (value, terminal_tab_label_get_screen (tab_label));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_tab_label_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);

  switch (prop_id) {
    case PROP_SCREEN:
      tab_label->screen = (TerminalScreen*)g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_tab_label_class_init (TerminalTabLabelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructed = terminal_tab_label_constructed;
  gobject_class->dispose = terminal_tab_label_dispose;
  gobject_class->finalize = terminal_tab_label_finalize;
  gobject_class->get_property = terminal_tab_label_get_property;
  gobject_class->set_property = terminal_tab_label_set_property;

  widget_class->root = terminal_tab_label_root;
  widget_class->unroot = terminal_tab_label_unroot;
  widget_class->measure = terminal_tab_label_measure;

  signals[CLOSE_BUTTON_CLICKED] =
    g_signal_new (I_("close-button-clicked"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE,
                  0);

  g_object_class_install_property
    (gobject_class,
     PROP_SCREEN,
     g_param_spec_object ("screen", nullptr, nullptr,
                          TERMINAL_TYPE_SCREEN,
                          GParamFlags(G_PARAM_READWRITE |
				      G_PARAM_STATIC_NAME |
				      G_PARAM_STATIC_NICK |
				      G_PARAM_STATIC_BLURB |
				      G_PARAM_CONSTRUCT_ONLY)));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* public API */

/**
 * terminal_tab_label_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalTabLabel for @screen
 */
GtkWidget *
terminal_tab_label_new (TerminalScreen *screen)
{
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_TAB_LABEL,
		   "screen", screen,
		   nullptr));
}

/**
 * terminal_tab_label_set_bold:
 * @tab_label: a #TerminalTabLabel
 * @bold: whether to enable label bolding
 *
 * Sets the tab label text bold, or unbolds it.
 */
void
terminal_tab_label_set_bold (TerminalTabLabel *tab_label,
                             gboolean bold)
{
  PangoAttrList *attr_list;
  PangoAttribute *weight_attr;
  gboolean free_list = FALSE;

  bold = bold != FALSE;
  if (tab_label->bold == bold)
    return;

  tab_label->bold = bold;

  attr_list = gtk_label_get_attributes (GTK_LABEL (tab_label->label));
  if (!attr_list) {
    attr_list = pango_attr_list_new ();
    free_list = TRUE;
  }

  if (bold)
    weight_attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  else
    weight_attr = pango_attr_weight_new (PANGO_WEIGHT_NORMAL);

  /* gtk_label_get_attributes() returns the label's internal list,
   * which we're probably not supposed to modify directly. 
   * It seems to work ok however.
   */
  pango_attr_list_change (attr_list, weight_attr);

  gtk_label_set_attributes (GTK_LABEL (tab_label->label), attr_list);

  if (free_list)
    pango_attr_list_unref (attr_list);
}

/**
 * terminal_tab_label_get_screen:
 * @tab_label: a #TerminalTabLabel
 *
 * Returns: (transfer none): the #TerminalScreen for @tab_label
 */
TerminalScreen *
terminal_tab_label_get_screen (TerminalTabLabel *tab_label)
{
  g_return_val_if_fail (TERMINAL_IS_TAB_LABEL (tab_label), nullptr);

  return tab_label->screen;
}
