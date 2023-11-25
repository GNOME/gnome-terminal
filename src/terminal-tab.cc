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

#include "terminal-tab.hh"
#include "terminal-debug.hh"

#include <gtk/gtk.h>

struct _TerminalTab
{
  GtkWidget parent_instance;

  TerminalScreen *screen;
  GtkWidget *overlay;
  GtkWidget *scrolled_window;
  GtkPolicyType hscrollbar_policy;
  GtkPolicyType vscrollbar_policy;

  bool pinned;
};

enum
{
  PROP_0,
  PROP_SCREEN,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY
};

G_DEFINE_FINAL_TYPE (TerminalTab, terminal_tab, GTK_TYPE_WIDGET)

#define TERMINAL_TAB_CSS_NAME "terminal-tab"

static void
terminal_tab_init (TerminalTab *tab)
{
  tab->hscrollbar_policy = GTK_POLICY_AUTOMATIC;
  tab->vscrollbar_policy = GTK_POLICY_AUTOMATIC;
}

static void
terminal_tab_constructed (GObject *object)
{
  TerminalTab *tab = TERMINAL_TAB (object);

  G_OBJECT_CLASS (terminal_tab_parent_class)->constructed (object);

  g_assert (tab->screen != nullptr);

  tab->overlay = gtk_overlay_new ();
  gtk_widget_set_parent (GTK_WIDGET (tab->overlay), GTK_WIDGET (tab));

  tab->scrolled_window =
    (GtkWidget *)g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                               "hscrollbar-policy", tab->hscrollbar_policy,
                               "vscrollbar-policy", tab->vscrollbar_policy,
                               "child", tab->screen,
                               "propagate-natural-width", TRUE,
                               "propagate-natural-height", TRUE,
                               nullptr);
  gtk_overlay_set_child (GTK_OVERLAY (tab->overlay),
                         tab->scrolled_window);
}

static void
terminal_tab_dispose (GObject *object)
{
  TerminalTab *tab = TERMINAL_TAB (object);

  g_clear_pointer (&tab->overlay, gtk_widget_unparent);

  G_OBJECT_CLASS (terminal_tab_parent_class)->dispose (object);
}

static void
terminal_tab_get_property (GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
  TerminalTab *tab = TERMINAL_TAB (object);

  switch (prop_id) {
    case PROP_SCREEN:
      g_value_set_object (value, tab->screen);
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, tab->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, tab->vscrollbar_policy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_tab_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  TerminalTab *tab = TERMINAL_TAB (object);

  switch (prop_id) {
    case PROP_SCREEN:
      tab->screen = (TerminalScreen*)g_value_get_object (value);
      break;
    case PROP_HSCROLLBAR_POLICY:
      terminal_tab_set_policy (tab,
                                            GtkPolicyType(g_value_get_enum (value)),
                                            tab->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      terminal_tab_set_policy (tab,
                                            tab->hscrollbar_policy,
                                            GtkPolicyType(g_value_get_enum (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_tab_class_init (TerminalTabClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructed = terminal_tab_constructed;
  gobject_class->dispose = terminal_tab_dispose;
  gobject_class->get_property = terminal_tab_get_property;
  gobject_class->set_property = terminal_tab_set_property;

  gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name(widget_class, TERMINAL_TAB_CSS_NAME);

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
 * terminal_tab_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalTab for @screen
 */
TerminalTab*
terminal_tab_new (TerminalScreen *screen)
{
  return reinterpret_cast<TerminalTab*>
    (g_object_new (TERMINAL_TYPE_TAB,
		   "screen", screen,
		   nullptr));
}

/**
 * terminal_tab_get_screen:
 * @tab: a #TerminalTab
 *
 * Returns: @tab's #TerminalScreen
 */
TerminalScreen *
terminal_tab_get_screen (TerminalTab *tab)
{
  if (tab == nullptr)
    return nullptr;

  g_return_val_if_fail (TERMINAL_IS_TAB (tab), nullptr);

  return tab->screen;
}

/**
 * terminal_tab_get_from_screen:
 * @screen: a #TerminalScreen
 *
 * Returns the #TerminalTab containing @screen.
 */
TerminalTab *
terminal_tab_get_from_screen (TerminalScreen *screen)
{
  if (screen == nullptr)
    return nullptr;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), nullptr);

  return TERMINAL_TAB (gtk_widget_get_ancestor (GTK_WIDGET (screen), TERMINAL_TYPE_TAB));
}

/**
 * terminal_tab_set_policy:
 * @tab: a #TerminalTab
 * @hpolicy: a #GtkPolicyType
 * @vpolicy: a #GtkPolicyType
 *
 * Sets @tab's scrollbar policy.
 */
void
terminal_tab_set_policy (TerminalTab *tab,
                                      GtkPolicyType hpolicy,
                                      GtkPolicyType vpolicy)
{
  GObject *object;
  GtkSettings *settings;
  gboolean overlay_scrolling;

  g_return_if_fail (TERMINAL_IS_TAB (tab));

  object = G_OBJECT (tab);

  g_object_freeze_notify (object);

  if (tab->hscrollbar_policy != hpolicy) {
    tab->hscrollbar_policy = hpolicy;
    g_object_notify (object, "hscrollbar-policy");
  }
  if (tab->vscrollbar_policy != vpolicy) {
    tab->vscrollbar_policy = vpolicy;
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
    terminal_assert_not_reached ();
  }

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tab->scrolled_window), hpolicy, vpolicy);
  gtk_scrolled_window_set_overlay_scrolling (GTK_SCROLLED_WINDOW (tab->scrolled_window),
                                             vpolicy == GTK_POLICY_AUTOMATIC);

  g_object_thaw_notify (object);
}

void
terminal_tab_add_overlay (TerminalTab *tab,
                                       GtkWidget *child)
{
  g_return_if_fail (TERMINAL_IS_TAB (tab));

  gtk_overlay_add_overlay (GTK_OVERLAY (tab->overlay), child);
}

void
terminal_tab_remove_overlay (TerminalTab *tab,
                                          GtkWidget               *child)
{
  g_return_if_fail (TERMINAL_IS_TAB (tab));

  gtk_overlay_remove_overlay (GTK_OVERLAY (tab->overlay), child);
}

void
terminal_tab_destroy (TerminalTab *tab)
{
  g_return_if_fail (TERMINAL_IS_TAB (tab));

  tab->screen = nullptr;
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (tab->scrolled_window), nullptr);
}

void
terminal_tab_set_pinned(TerminalTab* tab,
                        bool pinned)
{
  g_return_if_fail(TERMINAL_IS_TAB(tab));

  tab->pinned = pinned;
}

bool
terminal_tab_get_pinned(TerminalTab* tab)
{
  g_return_val_if_fail(TERMINAL_IS_TAB(tab), false);

  return tab->pinned;
}

