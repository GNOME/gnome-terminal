/* terminal-accel-dialog.hh
 *
 * Copyright 2017-2023 Christian Hergert
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_ACCEL_DIALOG (terminal_accel_dialog_get_type())

G_DECLARE_FINAL_TYPE (TerminalAccelDialog, terminal_accel_dialog, TERMINAL, ACCEL_DIALOG, AdwWindow)

GtkWidget  *terminal_accel_dialog_new                (void);
char       *terminal_accel_dialog_get_accelerator    (TerminalAccelDialog *self);
void        terminal_accel_dialog_set_accelerator    (TerminalAccelDialog *self,
                                                      const char          *accelerator);
const char *terminal_accel_dialog_get_shortcut_title (TerminalAccelDialog *self);
void        terminal_accel_dialog_set_shortcut_title (TerminalAccelDialog *self,
                                                      const char          *title);

G_END_DECLS
