/*
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#include "terminal-libgsystem.h"
#include "terminal-screen-container.h"
#include "terminal-debug.h"

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define TERMINAL_SCREEN_CONTAINER_GET_PRIVATE(screen_container)(G_TYPE_INSTANCE_GET_PRIVATE ((screen_container), TERMINAL_TYPE_SCREEN_CONTAINER, TerminalScreenContainerPrivate))

struct _TerminalScreenContainerPrivate
{
  TerminalScreen *screen;
  GtkWidget *hbox;
  GtkWidget *resize_popup;
  GtkWidget *vscrollbar;
  GtkPolicyType hscrollbar_policy;
  GtkPolicyType vscrollbar_policy;
  int old_grid_height;
  int old_grid_width;
  unsigned int connect_toplevel_id;
  unsigned long motion_notify_id;
  unsigned long size_allocate_id;
};

enum
{
  PROP_0,
  PROP_SCREEN,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_WINDOW_PLACEMENT,
  PROP_WINDOW_PLACEMENT_SET
};

G_DEFINE_TYPE (TerminalScreenContainer, terminal_screen_container, GTK_TYPE_OVERLAY)

static gboolean is_wayland = FALSE;

/* helper functions */

/* Widget class implementation */

static gboolean
terminal_screen_container_key_press_event (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv = container->priv;

  gtk_widget_hide (priv->resize_popup);
  return GDK_EVENT_PROPAGATE;
}

static gboolean
terminal_screen_container_motion_notify_event (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv = container->priv;

  gtk_widget_hide (priv->resize_popup);
  return GDK_EVENT_PROPAGATE;
}

static gboolean
terminal_screen_container_size_allocate (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv = container->priv;
  gs_free char *text = NULL;
  int grid_height;
  int grid_width;

  if (priv->screen == NULL)
    goto out;

  terminal_screen_get_size (priv->screen, &grid_width, &grid_height);
  if (grid_height == priv->old_grid_height && grid_width == priv->old_grid_width)
    goto out;

  text = g_strdup_printf (_("%d x %d"), grid_width, grid_height);
  gtk_label_set_text (GTK_LABEL (priv->resize_popup), text);
  gtk_widget_show (priv->resize_popup);

  priv->old_grid_height = grid_height;
  priv->old_grid_width = grid_width;

 out:
  return GDK_EVENT_PROPAGATE;
}

static gboolean
terminal_screen_container_connect_toplevel (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv = container->priv;
  GtkWidget *toplevel;

  priv->connect_toplevel_id = 0;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
  if (gtk_widget_is_toplevel (toplevel))
    {
      priv->motion_notify_id = g_signal_connect_object (toplevel,
                                                        "motion-notify-event",
                                                        G_CALLBACK (terminal_screen_container_motion_notify_event),
                                                        container,
                                                        G_CONNECT_SWAPPED);

      priv->size_allocate_id = g_signal_connect_object (toplevel,
                                                        "size-allocate",
                                                        G_CALLBACK (terminal_screen_container_size_allocate),
                                                        container,
                                                        G_CONNECT_SWAPPED);
    }

  return G_SOURCE_REMOVE;
}

static void
terminal_screen_container_remove_idle (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv = container->priv;

  if (priv->connect_toplevel_id != 0)
    {
      g_source_remove (priv->connect_toplevel_id);
      priv->connect_toplevel_id = 0;
    }
}

static void
terminal_screen_container_parent_set (GtkWidget *widget, GtkWidget *old_parent)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (widget);
  TerminalScreenContainerPrivate *priv = container->priv;
  GtkWidget *toplevel;

  if (!is_wayland)
    return;

  toplevel = gtk_widget_get_toplevel (widget);
  if (gtk_widget_is_toplevel (toplevel))
    {
      if (priv->motion_notify_id != 0)
        {
          g_signal_handler_disconnect (old_parent, priv->motion_notify_id);
          priv->motion_notify_id = 0;
        }

      if (priv->size_allocate_id != 0)
        {
          g_signal_handler_disconnect (old_parent, priv->size_allocate_id);
          priv->size_allocate_id = 0;
        }

      terminal_screen_container_remove_idle (container);
      priv->connect_toplevel_id = g_idle_add ((GSourceFunc) terminal_screen_container_connect_toplevel,
                                              container);
    }
}

static void
terminal_screen_container_style_updated (GtkWidget *widget)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (widget);
  TerminalScreenContainerPrivate *priv = container->priv;
  GtkCornerType corner;
  gboolean set;  

  GTK_WIDGET_CLASS (terminal_screen_container_parent_class)->style_updated (widget);

  gtk_widget_style_get (widget,
                        "window-placement", &corner,
                        "window-placement-set", &set,
                        NULL);

  if (!set) {
    g_object_get (gtk_widget_get_settings (widget),
                  "gtk-scrolled-window-placement", &corner,
                  NULL);
  }

  switch (corner) {
    case GTK_CORNER_TOP_LEFT:
    case GTK_CORNER_BOTTOM_LEFT:
      gtk_box_reorder_child (GTK_BOX (priv->hbox), priv->vscrollbar, -1);
      break;
    case GTK_CORNER_TOP_RIGHT:
    case GTK_CORNER_BOTTOM_RIGHT:
      gtk_box_reorder_child (GTK_BOX (priv->hbox), priv->vscrollbar, 0);
      break;
    default:
      g_assert_not_reached ();
  }
}

/* Class implementation */

static void
terminal_screen_container_init (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv;

  priv = container->priv = TERMINAL_SCREEN_CONTAINER_GET_PRIVATE (container);

  priv->resize_popup = gtk_label_new (NULL);
  gtk_widget_set_halign (priv->resize_popup, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->resize_popup, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (priv->resize_popup, TRUE);
  gtk_overlay_add_overlay (GTK_OVERLAY (container), priv->resize_popup);

  priv->hscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->vscrollbar_policy = GTK_POLICY_AUTOMATIC;
}

static void
terminal_screen_container_constructed (GObject *object)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
  TerminalScreenContainerPrivate *priv = container->priv;

  G_OBJECT_CLASS (terminal_screen_container_parent_class)->constructed (object);

  g_assert (priv->screen != NULL);

  g_signal_connect_swapped (priv->screen,
                            "key-press-event",
                            G_CALLBACK (terminal_screen_container_key_press_event),
                            container);

  priv->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  priv->vscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL,
                                        gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->screen)));

  gtk_box_pack_start (GTK_BOX (priv->hbox), GTK_WIDGET (priv->screen), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox), priv->vscrollbar, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (container), priv->hbox);
  gtk_widget_show_all (priv->hbox);

  _terminal_screen_update_scrollbar (priv->screen);
}

static void
terminal_screen_container_get_property (GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
  TerminalScreenContainerPrivate *priv = container->priv;

  switch (prop_id) {
    case PROP_SCREEN:
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->vscrollbar_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_screen_container_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
  TerminalScreenContainerPrivate *priv = container->priv;

  switch (prop_id) {
    case PROP_SCREEN:
      priv->screen = g_value_get_object (value);
      break;
    case PROP_HSCROLLBAR_POLICY:
      terminal_screen_container_set_policy (container,
                                            g_value_get_enum (value),
                                            priv->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      terminal_screen_container_set_policy (container,
                                            priv->hscrollbar_policy,
                                            g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_screen_container_dispose (GObject *object)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);

  terminal_screen_container_remove_idle (container);

  G_OBJECT_CLASS (terminal_screen_container_parent_class)->dispose (object);
}

static void
terminal_screen_container_class_init (TerminalScreenContainerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (gobject_class, sizeof (TerminalScreenContainerPrivate));

  gobject_class->constructed = terminal_screen_container_constructed;
  gobject_class->dispose = terminal_screen_container_dispose;
  gobject_class->get_property = terminal_screen_container_get_property;
  gobject_class->set_property = terminal_screen_container_set_property;

  widget_class->parent_set = terminal_screen_container_parent_set;
  widget_class->style_updated = terminal_screen_container_style_updated;

  g_object_class_install_property
    (gobject_class,
     PROP_SCREEN,
     g_param_spec_object ("screen", NULL, NULL,
                          TERMINAL_TYPE_SCREEN,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

   g_object_class_install_property
    (gobject_class,
     PROP_HSCROLLBAR_POLICY,
     g_param_spec_enum ("hscrollbar-policy", NULL, NULL,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));
   g_object_class_install_property
    (gobject_class,
     PROP_VSCROLLBAR_POLICY,
     g_param_spec_enum ("vscrollbar-policy", NULL, NULL,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));

   gtk_widget_class_install_style_property (widget_class,
                                            g_param_spec_enum ("window-placement", NULL, NULL,
                                                               GTK_TYPE_CORNER_TYPE,
                                                               GTK_CORNER_BOTTOM_RIGHT,
                                                               G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("window-placement-set", NULL, NULL,
                                                                 FALSE,
                                                                 G_PARAM_READWRITE |
                                                                 G_PARAM_STATIC_STRINGS));

#ifdef GDK_WINDOWING_WAYLAND
  {
    GdkDisplay *display;

    display = gdk_display_get_default ();
    if (GDK_IS_WAYLAND_DISPLAY (display))
      is_wayland = TRUE;
  }
#endif
}

/* public API */

/**
 * terminal_screen_container_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalScreenContainer for @screen
 */
GtkWidget *
terminal_screen_container_new (TerminalScreen *screen)
{
  return g_object_new (TERMINAL_TYPE_SCREEN_CONTAINER,
                       "screen", screen,
                       NULL);
}

/**
 * terminal_screen_container_get_screen:
 * @container: a #TerminalScreenContainer
 *
 * Returns: @container's #TerminalScreen
 */
TerminalScreen *
terminal_screen_container_get_screen (TerminalScreenContainer *container)
{
  if (container == NULL)
    return NULL;

  g_return_val_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container), NULL);

  return container->priv->screen;
}

/**
 * terminal_screen_container_get_from_screen:
 * @screen: a #TerminalScreenContainerPrivate
 *
 * Returns the #TerminalScreenContainer containing @screen.
 */
TerminalScreenContainer *
terminal_screen_container_get_from_screen (TerminalScreen *screen)
{
  if (screen == NULL)
    return NULL;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  return TERMINAL_SCREEN_CONTAINER (gtk_widget_get_ancestor (GTK_WIDGET (screen), TERMINAL_TYPE_SCREEN_CONTAINER));
}

/**
 * terminal_screen_container_set_policy:
 * @container: a #TerminalScreenContainer
 * @hpolicy: a #GtkPolicyType
 * @vpolicy: a #GtkPolicyType
 *
 * Sets @container's scrollbar policy.
 */
void
terminal_screen_container_set_policy (TerminalScreenContainer *container,
                                      GtkPolicyType hpolicy,
                                      GtkPolicyType vpolicy)
{
  TerminalScreenContainerPrivate *priv;
  GObject *object;

  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  object = G_OBJECT (container);
  priv = container->priv;

  g_object_freeze_notify (object);

  if (priv->hscrollbar_policy != hpolicy) {
    priv->hscrollbar_policy = hpolicy;
    g_object_notify (object, "hscrollbar-policy");
  }
  if (priv->vscrollbar_policy != vpolicy) {
    priv->vscrollbar_policy = vpolicy;
    g_object_notify (object, "vscrollbar-policy");
  }

  switch (vpolicy) {
    case GTK_POLICY_NEVER:
      gtk_widget_hide (priv->vscrollbar);
      break;
    case GTK_POLICY_AUTOMATIC:
    case GTK_POLICY_ALWAYS:
      gtk_widget_show (priv->vscrollbar);
      break;
    default:
      g_assert_not_reached ();
  }

  g_object_thaw_notify (object);
}
