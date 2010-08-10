/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2008 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
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

G_BEGIN_DECLS

#define CONF_PROXY_PREFIX      "/system/proxy"
#define CONF_HTTP_PROXY_PREFIX "/system/http_proxy"

void terminal_util_set_unique_role (GtkWindow *window, const char *prefix);

void terminal_util_show_error_dialog (GtkWindow *transient_parent, 
                                      GtkWidget **weap_ptr, 
                                      GError *error,
                                      const char *message_format, ...) G_GNUC_PRINTF(4, 5);

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

char *terminal_util_resolve_relative_path (const char *path,
                                           const char *relative_path);

void terminal_util_transform_uris_to_quoted_fuse_paths (char **uris);

char *terminal_util_concat_uris (char **uris,
                                 gsize *length);

char *terminal_util_get_licence_text (void);

gboolean terminal_util_load_builder_file (const char *filename,
                                          const char *object_name,
                                          ...);

gboolean terminal_util_dialog_response_on_delete (GtkWindow *widget);

void terminal_util_key_file_set_string_escape    (GKeyFile *key_file,
                                                  const char *group,
                                                  const char *key,
                                                  const char *string);
char *terminal_util_key_file_get_string_unescape (GKeyFile *key_file,
                                                  const char *group,
                                                  const char *key,
                                                  GError **error);

void terminal_util_key_file_set_argv      (GKeyFile *key_file,
                                           const char *group,
                                           const char *key,
                                           int argc,
                                           char **argv);
char **terminal_util_key_file_get_argv    (GKeyFile *key_file,
                                           const char *group,
                                           const char *key,
                                           int *argc,
                                           GError **error);

void terminal_util_add_proxy_env (GHashTable *env_table);

typedef enum {
  FLAG_INVERT_BOOL  = 1 << 0,
} PropertyChangeFlags;

void terminal_util_bind_object_property_to_widget (GObject *object,
                                                   const char *object_prop,
                                                   GtkWidget *widget,
                                                   PropertyChangeFlags flags);

gboolean terminal_util_x11_get_net_wm_desktop (GdkWindow *window,
					       guint32   *desktop);
void     terminal_util_x11_set_net_wm_desktop (GdkWindow *window,
					       guint32    desktop);

void terminal_util_x11_clear_demands_attention (GdkWindow *window);

gboolean terminal_util_x11_window_is_minimized (GdkWindow *window);

G_END_DECLS

#endif /* TERMINAL_UTIL_H */
