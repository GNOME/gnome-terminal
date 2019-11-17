/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2008, 2010 Christian Persch
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

#ifndef TERMINAL_UTIL_H
#define TERMINAL_UTIL_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void terminal_util_show_error_dialog (GtkWindow *transient_parent,
                                      GtkWidget **weap_ptr,
                                      GError *error,
                                      const char *message_format, ...) G_GNUC_PRINTF(4, 5);

void terminal_util_show_help (const char *topic);

void terminal_util_show_about (void);

void terminal_util_set_labelled_by          (GtkWidget  *widget,
                                             GtkLabel   *label);
void terminal_util_set_atk_name_description (GtkWidget  *widget,
                                             const char *name,
                                             const char *desc);

void terminal_util_open_url (GtkWidget *parent,
                             const char *orig_url,
                             TerminalURLFlavor flavor,
                             guint32 user_time);

void terminal_util_transform_uris_to_quoted_fuse_paths (char **uris);

char *terminal_util_concat_uris (char **uris,
                                 gsize *length);

char *terminal_util_get_licence_text (void);

GtkBuilder *terminal_util_load_widgets_resource (const char *path,
                                                 const char *main_object_name,
                                                 const char *object_name,
                                                 ...);

void terminal_util_load_objects_resource (const char *path,
                                          const char *object_name,
                                          ...);

void terminal_util_dialog_focus_widget (GtkBuilder *builder,
                                        const char *widget_name);

gboolean terminal_util_dialog_response_on_delete (GtkWindow *widget);

void terminal_util_add_proxy_env (GHashTable *env_table);

char **terminal_util_get_etc_shells (void);

gboolean terminal_util_get_is_shell (const char *command);

GSettings *terminal_g_settings_new (const char *schema_id,
                                    const char *mandatory_key,
                                    const GVariantType *mandatory_key_type);

const GdkRGBA *terminal_g_settings_get_rgba (GSettings  *settings,
                                             const char *key,
                                             GdkRGBA    *rgba);
void terminal_g_settings_set_rgba (GSettings  *settings,
                                   const char *key,
                                   const GdkRGBA *rgba);

GdkRGBA *terminal_g_settings_get_rgba_palette (GSettings  *settings,
                                               const char *key,
                                               gsize      *n_colors);
void terminal_g_settings_set_rgba_palette (GSettings      *settings,
                                           const char     *key,
                                           const GdkRGBA  *colors,
                                           gsize           n_colors);

void terminal_util_bind_mnemonic_label_sensitivity (GtkWidget *widget);

char *terminal_util_number_info (const char *str);

char *terminal_util_uri_fixup (const char *uri,
                               GError **error);

char *terminal_util_hyperlink_uri_label (const char *str);

gchar *terminal_util_utf8_make_valid (const gchar *str,
                                      gssize       len) G_GNUC_MALLOC;

void terminal_util_load_print_settings (GtkPrintSettings **settings,
                                        GtkPageSetup **page_setup);

void terminal_util_save_print_settings (GtkPrintSettings *settings,
                                        GtkPageSetup *page_setup);

const char *terminal_util_translate_encoding (const char *encoding);

char *terminal_util_find_program_in_path (const char *path,
                                          const char *program);

G_END_DECLS

#endif /* TERMINAL_UTIL_H */
