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

#include "terminal-screen.h"

G_BEGIN_DECLS

void terminal_util_show_error_dialog (GtkWindow *transient_parent, 
                                      GtkWidget **weap_ptr, 
                                      GError *error,
                                      const char *message_format, ...) G_GNUC_PRINTF(4, 5);

void terminal_util_show_help (const char *topic, GtkWindow  *transient_parent);

void terminal_util_show_about (GtkWindow *transient_parent);

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

char *terminal_util_concat_uris (char **uris,
                                 gsize *length);

char *terminal_util_get_licence_text (void);

void terminal_util_load_builder_resource (const char *path,
                                          const char *main_object_name,
                                          const char *object_name,
                                          ...);

void terminal_util_dialog_focus_widget (GtkWidget *dialog,
                                        const char *widget_name);

gboolean terminal_util_dialog_response_on_delete (GtkWindow *widget);

void terminal_util_add_proxy_env (GHashTable *env_table);

GdkScreen *terminal_util_get_screen_by_display_name (const char *display_name,
                                                     int screen_number);

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

void terminal_util_object_class_undeprecate_property (GObjectClass *klass,
                                                      const char *prop);

#define TERMINAL_UTIL_OBJECT_CLASS_UNDEPRECATE_PROPERTY(klass, prop) \
  { \
    static volatile gsize once = 0; \
    \
    if (g_once_init_enter (&once)) { \
      GParamSpec *pspec; \
      \
      pspec = g_object_class_find_property (klass, prop); \
      g_warn_if_fail (pspec != NULL); \
      if (pspec) { \
        g_warn_if_fail (pspec->flags & G_PARAM_DEPRECATED); \
        pspec->flags &= ~G_PARAM_DEPRECATED; \
      } \
      g_once_init_leave (&once, 1); \
    } \
  }

#define TERMINAL_UTIL_OBJECT_TYPE_UNDEPRECATE_PROPERTY(type, prop) \
  { \
    GObjectClass *klass; \
    \
    klass = g_type_class_ref (type); \
    TERMINAL_UTIL_OBJECT_CLASS_UNDEPRECATE_PROPERTY (klass, prop); \
    g_type_class_unref (klass); \
  }

G_END_DECLS

#endif /* TERMINAL_UTIL_H */
