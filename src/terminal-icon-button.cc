/*
 * terminal-icon-button.c
 *
 * Copyright © 2010 - Paolo Borelli
 * Copyright © 2011 - Ignacio Casal Quinteiro
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

#include "terminal-icon-button.hh"
#include "terminal-libgsystem.hh"

GtkWidget *
terminal_icon_button_new (const char *gicon_name)
{
  static const char * const css_classes[] = {"flat", nullptr};

  return (GtkWidget *) g_object_new (GTK_TYPE_BUTTON,
                                     "css-classes", css_classes,
                                     "focus-on-click", FALSE,
                                     "icon-name", gicon_name,
                                     nullptr);
}

GtkWidget *
terminal_close_button_new (void)
{
  return terminal_icon_button_new ("window-close-symbolic");
}
