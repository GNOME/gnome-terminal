/*
 * Copyright © 2001 Havoc Pennington
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

#ifndef TERMINAL_UTIL_H
#define TERMINAL_UTIL_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "terminal-screen.h"

void terminal_util_set_unique_role (GtkWindow *window, const char *prefix);

void terminal_util_show_error_dialog (GtkWindow *transient_parent, 
                                      GtkWidget **weap_ptr, 
                                      const char *message_format, ...) G_GNUC_PRINTF(3, 4);

void terminal_util_show_help (const char *topic, GtkWindow  *transient_parent);

void terminal_util_set_labelled_by          (GtkWidget  *widget,
                                             GtkLabel   *label);
void terminal_util_set_atk_name_description (GtkWidget  *widget,
                                             const char *name,
                                             const char *desc);

void terminal_util_open_url (GtkWidget *parent,
                             const char *orig_url,
                             TerminalURLFlavour flavor,
                             guint32 user_time);


void terminal_util_transform_uris_to_quoted_fuse_paths (char **uris);

gboolean terminal_util_load_builder_file (const char *filename,
                                          const char *object_name,
                                          ...);

gboolean terminal_util_dialog_response_on_delete (GtkWindow *widget);

typedef enum {
  FLAG_INVERT_BOOL  = 1 << 0,
} PropertyChangeFlags;

void terminal_util_bind_object_property_to_widget (GObject *object,
                                                   const char *object_prop,
                                                   GtkWidget *widget,
                                                   PropertyChangeFlags flags);

#endif /* TERMINAL_UTIL_H */
