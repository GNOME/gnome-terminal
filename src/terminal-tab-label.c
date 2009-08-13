/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2007, 2008 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope tab_label it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "terminal-intl.h"
#include "terminal-tab-label.h"

#define TERMINAL_TAB_LABEL_GET_PRIVATE(tab_label)(G_TYPE_INSTANCE_GET_PRIVATE ((tab_label), TERMINAL_TYPE_TAB_LABEL, TerminalTabLabelPrivate))

#define SPACING (4)

struct _TerminalTabLabelPrivate
{
  TerminalScreen *screen;
  GtkWidget *label;
  GtkWidget *close_button;
  gboolean bold;
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

G_DEFINE_TYPE (TerminalTabLabel, terminal_tab_label, GTK_TYPE_HBOX);

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

  title = terminal_screen_get_title (screen);
  hbox = gtk_widget_get_parent (label);

  gtk_label_set_text (GTK_LABEL (label), title);
  
  gtk_widget_set_tooltip_text (hbox, title);
}

/* public functions */

/* Class implementation */

static void
terminal_tab_label_parent_set (GtkWidget *widget,
                               GtkWidget *old_parent)
{
  void (* parent_set) (GtkWidget *, GtkWidget *) = GTK_WIDGET_CLASS (terminal_tab_label_parent_class)->parent_set;

  if (parent_set)
    parent_set (widget, old_parent);
}

static void
terminal_tab_label_style_set (GtkWidget *widget,
                              GtkStyle *previous_style)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (widget);
  TerminalTabLabelPrivate *priv = tab_label->priv;
  void (* style_set) (GtkWidget *, GtkStyle *) = GTK_WIDGET_CLASS (terminal_tab_label_parent_class)->style_set;
  int h, w;

  if (style_set)
    style_set (widget, previous_style);

  gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (widget),
                                     GTK_ICON_SIZE_MENU, &w, &h);
  gtk_widget_set_size_request (priv->close_button, w + 2, h + 2);
}

static void
terminal_tab_label_init (TerminalTabLabel *tab_label)
{
  tab_label->priv = TERMINAL_TAB_LABEL_GET_PRIVATE (tab_label);
}

static GObject *
terminal_tab_label_constructor (GType type,
                                guint n_construct_properties,
                                GObjectConstructParam *construct_params)
{
  GObject *object;
  TerminalTabLabel *tab_label;
  TerminalTabLabelPrivate *priv;
  GtkWidget *hbox, *label, *close_button, *image;

  object = G_OBJECT_CLASS (terminal_tab_label_parent_class)->constructor
             (type, n_construct_properties, construct_params);

  tab_label = TERMINAL_TAB_LABEL (object);
  hbox = GTK_WIDGET (tab_label);
  priv = tab_label->priv;

  g_assert (priv->screen != NULL);
  
  gtk_box_set_spacing (GTK_BOX (hbox), SPACING);

  priv->label = label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 0, 0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);

  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  priv->close_button = close_button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_widget_set_name (close_button, "gnome-terminal-tab-close-button");
  gtk_widget_set_tooltip_text (close_button, _("Close tab"));

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (close_button), image);
  gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

  sync_tab_label (priv->screen, NULL, label);
  g_signal_connect (priv->screen, "notify::title",
                    G_CALLBACK (sync_tab_label), label);

  g_signal_connect (close_button, "clicked",
		    G_CALLBACK (close_button_clicked_cb), tab_label);

  gtk_widget_show_all (hbox);

  return object;
}

static void
terminal_tab_label_finalize (GObject *object)
{
//   TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);

  G_OBJECT_CLASS (terminal_tab_label_parent_class)->finalize (object);
}

static void
terminal_tab_label_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  TerminalTabLabel *tab_label = TERMINAL_TAB_LABEL (object);
  TerminalTabLabelPrivate *priv = tab_label->priv;

  switch (prop_id) {
    case PROP_SCREEN:
      priv->screen = g_value_get_object (value);
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

  gobject_class->constructor = terminal_tab_label_constructor;
  gobject_class->finalize = terminal_tab_label_finalize;
  gobject_class->set_property = terminal_tab_label_set_property;

  widget_class->parent_set = terminal_tab_label_parent_set;
  widget_class->style_set = terminal_tab_label_style_set;

  signals[CLOSE_BUTTON_CLICKED] =
    g_signal_new (I_("close-button-clicked"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalTabLabelClass, close_button_clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  g_object_class_install_property
    (gobject_class,
     PROP_SCREEN,
     g_param_spec_object ("screen", NULL, NULL,
                          TERMINAL_TYPE_SCREEN,
                          G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
                          G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (gobject_class, sizeof (TerminalTabLabelPrivate));
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
  return g_object_new (TERMINAL_TYPE_TAB_LABEL,
                       "screen", screen,
                       NULL);
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
  TerminalTabLabelPrivate *priv = tab_label->priv;
  PangoAttrList *attr_list;
  PangoAttribute *weight_attr;
  gboolean free_list = FALSE;

  bold = bold != FALSE;
  if (priv->bold == bold)
    return;

  priv->bold = bold;

  attr_list = gtk_label_get_attributes (GTK_LABEL (priv->label));
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

  gtk_label_set_attributes (GTK_LABEL (priv->label), attr_list);

  if (free_list)
    pango_attr_list_unref (attr_list);
}
