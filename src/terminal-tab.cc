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
#include "terminal-type-builtins.hh"

#include <gtk/gtk.h>

struct _TerminalTab
{
  GtkWidget parent_instance;

  TerminalScreen *screen;
  GtkWidget *overlay;
  GtkWidget *scrolled_window;
  TerminalScrollbarPolicy hscrollbar_policy;
  TerminalScrollbarPolicy vscrollbar_policy;

  bool pinned;
  bool kinetic_scrolling;
};

enum
{
  PROP_0,
  PROP_SCREEN,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_KINETIC_SCROLLING,
  N_PROPS
};

static GParamSpec* pspecs[N_PROPS];

G_DEFINE_FINAL_TYPE (TerminalTab, terminal_tab, GTK_TYPE_WIDGET)

#define TERMINAL_TAB_CSS_NAME "terminal-tab"

static void
terminal_tab_init (TerminalTab *tab)
{
  tab->hscrollbar_policy = TERMINAL_SCROLLBAR_POLICY_ALWAYS;
  tab->vscrollbar_policy = TERMINAL_SCROLLBAR_POLICY_NEVER;
  tab->pinned = false;
  tab->kinetic_scrolling = false;
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
                               "child", tab->screen,
                               "propagate-natural-width", TRUE,
                               "propagate-natural-height", TRUE,
                               nullptr);
  gtk_overlay_set_child (GTK_OVERLAY (tab->overlay),
                         tab->scrolled_window);

  // Apply the scrollbar policy
  terminal_tab_set_policy(tab, tab->hscrollbar_policy, tab->vscrollbar_policy);
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
    case PROP_KINETIC_SCROLLING:
      g_value_set_boolean(value, tab->kinetic_scrolling);
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
      terminal_tab_set_policy(tab,
                              TerminalScrollbarPolicy(g_value_get_enum(value)),
                              tab->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      terminal_tab_set_policy(tab,
                              tab->hscrollbar_policy,
                              TerminalScrollbarPolicy(g_value_get_enum(value)));
      break;
    case PROP_KINETIC_SCROLLING:
      terminal_tab_set_kinetic_scrolling(tab, g_value_get_boolean(value));
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

  pspecs[PROP_SCREEN] =
    g_param_spec_object("screen", nullptr, nullptr,
                        TERMINAL_TYPE_SCREEN,
                        GParamFlags(G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_STRINGS));

  pspecs[PROP_HSCROLLBAR_POLICY] =
    g_param_spec_enum("hscrollbar-policy", nullptr, nullptr,
                      TERMINAL_TYPE_SCROLLBAR_POLICY,
                      TERMINAL_SCROLLBAR_POLICY_NEVER,
                      GParamFlags(G_PARAM_READWRITE |
                                  G_PARAM_STATIC_STRINGS |
                                  G_PARAM_EXPLICIT_NOTIFY));
  pspecs[PROP_VSCROLLBAR_POLICY] =
    g_param_spec_enum("vscrollbar-policy", nullptr, nullptr,
                      TERMINAL_TYPE_SCROLLBAR_POLICY,
                      TERMINAL_SCROLLBAR_POLICY_ALWAYS,
                      GParamFlags(G_PARAM_READWRITE |
                                  G_PARAM_STATIC_STRINGS |
                                  G_PARAM_EXPLICIT_NOTIFY));

  pspecs[PROP_KINETIC_SCROLLING] =
    g_param_spec_boolean("kinetic-scrolling", nullptr, nullptr,
                         false,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS |
                                     G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties(gobject_class, G_N_ELEMENTS(pspecs), pspecs);
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
 * @hpolicy: a #TerminalScrollbarPolicy
 * @vpolicy: a #TerminalScrollbarPolicy
 *
 * Sets @tab's scrollbar policy.
 */
void
terminal_tab_set_policy (TerminalTab *tab,
                         TerminalScrollbarPolicy hpolicy,
                         TerminalScrollbarPolicy vpolicy)
{
  g_return_if_fail (TERMINAL_IS_TAB (tab));

  auto const object = G_OBJECT (tab);
  g_object_freeze_notify (object);

  if (tab->hscrollbar_policy != hpolicy) {
    tab->hscrollbar_policy = hpolicy;
    g_object_notify_by_pspec(object, pspecs[PROP_HSCROLLBAR_POLICY]);
  }
  if (tab->vscrollbar_policy != vpolicy) {
    tab->vscrollbar_policy = vpolicy;
    g_object_notify_by_pspec(object, pspecs[PROP_VSCROLLBAR_POLICY]);
  }


  auto const hpolicy_gtk = GTK_POLICY_NEVER; // regardless of hscrollbar_policy

  auto vpolicy_to_gtk = [](TerminalScrollbarPolicy policy) constexpr noexcept -> auto
  {
    switch (policy) {
    case TERMINAL_SCROLLBAR_POLICY_NEVER: return GTK_POLICY_EXTERNAL;
    case TERMINAL_SCROLLBAR_POLICY_OVERLAY: return GTK_POLICY_AUTOMATIC;
    default: terminal_assert_not_reached();
      [[fallthrough]];
    case TERMINAL_SCROLLBAR_POLICY_ALWAYS: return GTK_POLICY_ALWAYS;
    }
  };

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (tab->scrolled_window),
                                 hpolicy_gtk,
                                 vpolicy_to_gtk(tab->vscrollbar_policy));
  gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(tab->scrolled_window),
                                            tab->vscrollbar_policy == TERMINAL_SCROLLBAR_POLICY_OVERLAY);

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

void
terminal_tab_set_kinetic_scrolling(TerminalTab* tab,
                                   bool enable)
{
  g_return_if_fail(TERMINAL_IS_TAB(tab));

  tab->kinetic_scrolling = enable;
  gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(tab->scrolled_window),
                                            enable);
  g_object_notify_by_pspec(G_OBJECT(tab), pspecs[PROP_KINETIC_SCROLLING]);
}

bool
terminal_tab_get_kinetic_scrolling(TerminalTab* tab)
{
  g_return_val_if_fail(TERMINAL_IS_TAB(tab), false);

  return tab->kinetic_scrolling;
}
