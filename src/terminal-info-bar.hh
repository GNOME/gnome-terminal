/*
 *  Copyright © 2010 Christian Persch
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

#ifndef TERMINAL_INFO_BAR_H
#define TERMINAL_INFO_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_INFO_BAR (terminal_info_bar_get_type ())

G_DECLARE_FINAL_TYPE (TerminalInfoBar, terminal_info_bar, TERMINAL, INFO_BAR, GtkWidget)

GtkWidget *terminal_info_bar_new (GtkMessageType type,
                                  const char *first_button_text,
                                  ...) G_GNUC_NULL_TERMINATED;

void terminal_info_bar_format_text (TerminalInfoBar *bar,
                                    const char *format,
                                    ...) G_GNUC_PRINTF (2, 3);
void terminal_info_bar_set_default_response (TerminalInfoBar *bar,
                                             int response_id);

G_END_DECLS

#endif /* !TERMINAL_INFO_BAR_H */
