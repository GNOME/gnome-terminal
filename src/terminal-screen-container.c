/*
 * Copyright © 2008 Christian Persch
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


#include <config.h>

#include "terminal-screen-container.h"

#define SCREEN_DATA_KEY    "TSC::Screen"
#define SCROLLBAR_DATA_KEY "TSC::Scrollbar"

GtkWidget *
terminal_screen_container_new (TerminalScreen *screen)
{
#ifdef USE_SCROLLED_WINDOW
  GtkWidget *scrolled_window;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  scrolled_window = gtk_scrolled_window_new (NULL, vte_terminal_get_adjustment (VTE_TERMINAL (screen)));
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (screen));
  gtk_widget_show (GTK_WIDGET (screen));

  _terminal_screen_update_scrollbar (screen);
  return scrolled_window;
#else
  GtkWidget *hbox, *scrollbar;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  hbox = gtk_hbox_new (FALSE, 0);

  scrollbar = gtk_vscrollbar_new (vte_terminal_get_adjustment (VTE_TERMINAL (screen)));

  gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (screen), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), scrollbar, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (hbox), SCREEN_DATA_KEY, screen);
  g_object_set_data (G_OBJECT (hbox), SCROLLBAR_DATA_KEY, scrollbar);
  
  gtk_widget_show_all (hbox);

  _terminal_screen_update_scrollbar (screen);
  return hbox;
#endif
}

TerminalScreen *
terminal_screen_container_get_screen (GtkWidget *container)
{
#ifdef USE_SCROLLED_WINDOW
  g_return_val_if_fail (GTK_IS_SCROLLED_WINDOW (container), NULL);

  return TERMINAL_SCREEN (gtk_bin_get_child (GTK_BIN (container)));
#else
  g_return_val_if_fail (GTK_IS_HBOX (container), NULL);

  return TERMINAL_SCREEN (g_object_get_data (G_OBJECT (container), SCREEN_DATA_KEY));
#endif
}

#ifndef USE_SCROLLED_WINDOW
static GtkWidget *
terminal_screen_container_get_scrollbar (GtkWidget *container)
{
  return g_object_get_data (G_OBJECT (container), SCROLLBAR_DATA_KEY);
}
#endif

void
terminal_screen_container_set_policy (GtkWidget *container,
                                      GtkPolicyType hpolicy G_GNUC_UNUSED,
                                      GtkPolicyType vpolicy)
{
#ifdef USE_SCROLLED_WINDOW
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (container), hpolicy, vpolicy);
#else
  GtkWidget *scrollbar;

  g_return_if_fail (GTK_IS_HBOX (container));

  scrollbar = terminal_screen_container_get_scrollbar (container);
  switch (vpolicy) {
    case GTK_POLICY_NEVER:
      gtk_widget_hide (scrollbar);
      break;
    case GTK_POLICY_AUTOMATIC:
    case GTK_POLICY_ALWAYS:
      gtk_widget_show (scrollbar);
      break;
    default:
      g_assert_not_reached ();
  }
#endif
}

void
terminal_screen_container_set_placement (GtkWidget *container,
                                         GtkCornerType corner)
{
#ifdef USE_SCROLLED_WINDOW
  g_return_if_fail (GTK_IS_SCROLLED_WINDOW (container));

  gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (container), corner);
#else
  GtkWidget *scrollbar;

  g_return_if_fail (GTK_IS_HBOX (container));

  scrollbar = terminal_screen_container_get_scrollbar (container);
  switch (corner) {
    case GTK_CORNER_TOP_LEFT:
    case GTK_CORNER_BOTTOM_LEFT:
      gtk_box_reorder_child (GTK_BOX (container), scrollbar, 1);
      break;
    case GTK_CORNER_TOP_RIGHT:
    case GTK_CORNER_BOTTOM_RIGHT:
      gtk_box_reorder_child (GTK_BOX (container), scrollbar, 0);
      break;
    default:
      g_assert_not_reached ();
  }
#endif
}
