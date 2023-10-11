/*
 * Copyright © 2008, 2010, 2011 Christian Persch
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

#include "config.h"

#include "terminal-screen-container.hh"
#include "terminal-debug.hh"

#include <gtk/gtk.h>

struct _TerminalScreenContainer
{
  GtkWidget parent_instance;

  TerminalScreen *screen;
  GtkWidget *overlay;
  GtkWidget *scrolled_window;
  GtkPolicyType hscrollbar_policy;
  GtkPolicyType vscrollbar_policy;
};

enum
{
  PROP_0,
  PROP_SCREEN,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY
};

G_DEFINE_FINAL_TYPE (TerminalScreenContainer, terminal_screen_container, GTK_TYPE_WIDGET)

#define TERMINAL_SCREEN_CONTAINER_CSS_NAME "terminal-screen-container"

static void
terminal_screen_container_init (TerminalScreenContainer *container)
{
  container->hscrollbar_policy = GTK_POLICY_AUTOMATIC;
  container->vscrollbar_policy = GTK_POLICY_AUTOMATIC;
}

static void
terminal_screen_container_constructed (GObject *object)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);

  G_OBJECT_CLASS (terminal_screen_container_parent_class)->constructed (object);

  g_assert (container->screen != nullptr);

  container->overlay = gtk_overlay_new ();
  gtk_widget_set_parent (GTK_WIDGET (container->overlay), GTK_WIDGET (container));

  container->scrolled_window =
    (GtkWidget *)g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                               "hscrollbar-policy", container->hscrollbar_policy,
                               "vscrollbar-policy", container->vscrollbar_policy,
                               "child", container->screen,
                               "propagate-natural-width", TRUE,
                               "propagate-natural-height", TRUE,
                               nullptr);
  gtk_overlay_set_child (GTK_OVERLAY (container->overlay),
                         container->scrolled_window);
}

static void
terminal_screen_container_dispose (GObject *object)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);

  g_clear_pointer (&container->overlay, gtk_widget_unparent);

  G_OBJECT_CLASS (terminal_screen_container_parent_class)->dispose (object);
}

static void
terminal_screen_container_get_property (GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);

  switch (prop_id) {
    case PROP_SCREEN:
      g_value_set_object (value, container->screen);
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, container->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, container->vscrollbar_policy);
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

  switch (prop_id) {
    case PROP_SCREEN:
      container->screen = (TerminalScreen*)g_value_get_object (value);
      break;
    case PROP_HSCROLLBAR_POLICY:
      terminal_screen_container_set_policy (container,
                                            GtkPolicyType(g_value_get_enum (value)),
                                            container->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      terminal_screen_container_set_policy (container,
                                            container->hscrollbar_policy,
                                            GtkPolicyType(g_value_get_enum (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_screen_container_class_init (TerminalScreenContainerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructed = terminal_screen_container_constructed;
  gobject_class->dispose = terminal_screen_container_dispose;
  gobject_class->get_property = terminal_screen_container_get_property;
  gobject_class->set_property = terminal_screen_container_set_property;

  gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name(widget_class, TERMINAL_SCREEN_CONTAINER_CSS_NAME);

  g_object_class_install_property
    (gobject_class,
     PROP_SCREEN,
     g_param_spec_object ("screen", nullptr, nullptr,
                          TERMINAL_TYPE_SCREEN,
                          GParamFlags(G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_STRINGS)));
     
  g_object_class_install_property
    (gobject_class,
     PROP_HSCROLLBAR_POLICY,
     g_param_spec_enum ("hscrollbar-policy", nullptr, nullptr,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        GParamFlags(G_PARAM_READWRITE |
                                    G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property
    (gobject_class,
     PROP_VSCROLLBAR_POLICY,
     g_param_spec_enum ("vscrollbar-policy", nullptr, nullptr,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        GParamFlags(G_PARAM_READWRITE |
                                    G_PARAM_STATIC_STRINGS)));
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
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_SCREEN_CONTAINER,
		   "screen", screen,
		   nullptr));
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
  if (container == nullptr)
    return nullptr;

  g_return_val_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container), nullptr);

  return container->screen;
}

/**
 * terminal_screen_container_get_from_screen:
 * @screen: a #TerminalScreen
 *
 * Returns the #TerminalScreenContainer containing @screen.
 */
TerminalScreenContainer *
terminal_screen_container_get_from_screen (TerminalScreen *screen)
{
  if (screen == nullptr)
    return nullptr;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), nullptr);

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
  GObject *object;
  GtkSettings *settings;
  gboolean overlay_scrolling;

  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  object = G_OBJECT (container);

  g_object_freeze_notify (object);

  if (container->hscrollbar_policy != hpolicy) {
    container->hscrollbar_policy = hpolicy;
    g_object_notify (object, "hscrollbar-policy");
  }
  if (container->vscrollbar_policy != vpolicy) {
    container->vscrollbar_policy = vpolicy;
    g_object_notify (object, "vscrollbar-policy");
  }

  settings = gtk_settings_get_default ();
  g_object_get (settings,
                "gtk-overlay-scrolling", &overlay_scrolling,
                nullptr);

  switch (vpolicy) {
  case GTK_POLICY_NEVER:
    if (overlay_scrolling)
      vpolicy = GTK_POLICY_AUTOMATIC;
    else
      vpolicy = GTK_POLICY_EXTERNAL;
    break;
  case GTK_POLICY_AUTOMATIC:
  case GTK_POLICY_ALWAYS:
    vpolicy = GTK_POLICY_ALWAYS;
    break;
  default:
    g_assert_not_reached ();
  }

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (container->scrolled_window), hpolicy, vpolicy);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (container->scrolled_window),
                                             vpolicy == GTK_POLICY_AUTOMATIC);

  g_object_thaw_notify (object);
}

void
terminal_screen_container_add_overlay (TerminalScreenContainer *container,
                                       GtkWidget *child)
{
  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  gtk_overlay_add_overlay (GTK_OVERLAY (container->overlay), child);
}

void
terminal_screen_container_remove_overlay (TerminalScreenContainer *container,
                                          GtkWidget               *child)
{
  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  gtk_overlay_remove_overlay (GTK_OVERLAY (container->overlay), child);
}

void
terminal_screen_container_destroy (TerminalScreenContainer *container)
{
  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  container->screen = NULL;
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (container->scrolled_window), NULL);
}
