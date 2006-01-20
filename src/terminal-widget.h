/* Abstraction for which terminal widget we're using */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#ifndef TERMINAL_WIDGET_H
#define TERMINAL_WIDGET_H

#undef GDK_DISABLE_DEPRECATED
#include <gdk/gdkfont.h>
#define GDK_DISABLE_DEPRECATED

#include "terminal-profile.h"

G_BEGIN_DECLS

/*
 *  "If setting is unchanged do nothing" optimizations are encouraged,
 *  because gnome-terminal likes to call these a lot when they haven't
 *  changed.
 *
 */

GtkWidget* terminal_widget_new                        (void);
void       terminal_widget_set_size                   (GtkWidget            *widget,
                                                       int                   width_chars,
                                                       int                   height_chars);
void       terminal_widget_get_size                   (GtkWidget            *widget,
                                                       int                  *width_chars,
                                                       int                  *height_chars);
void       terminal_widget_get_cell_size              (GtkWidget            *widget,
                                                       int                  *cell_width_pixels,
                                                       int                  *cell_height_pixels);
void       terminal_widget_get_padding                (GtkWidget            *widget,
                                                       int                  *xpad,
                                                       int                  *ypad);
void       terminal_widget_match_add                  (GtkWidget            *widget,
                                                       const char           *regexp,
                                                       int                   flavor);
void       terminal_widget_skey_match_add             (GtkWidget            *widget,
                                                       const char           *regexp,
                                                       int                   flavor);
char*      terminal_widget_check_match                (GtkWidget            *widget,
                                                       int                   column,
                                                       int                   row,
                                                       int                  *flavor);
char*      terminal_widget_skey_check_match           (GtkWidget            *widget,
                                                       int                   column,
                                                       int                   row,
                                                       int                  *flavor);
void       terminal_widget_skey_match_remove          (GtkWidget            *widget);
void       terminal_widget_set_word_characters        (GtkWidget            *widget,
                                                       const char           *str);
void       terminal_widget_set_delete_binding         (GtkWidget            *widget,
                                                       TerminalEraseBinding  binding);
void       terminal_widget_set_backspace_binding      (GtkWidget            *widget,
                                                       TerminalEraseBinding  binding);
void       terminal_widget_set_cursor_blinks          (GtkWidget            *widget,
                                                       gboolean              setting);
void       terminal_widget_set_audible_bell           (GtkWidget            *widget,
                                                       gboolean              setting);
void       terminal_widget_set_scroll_on_keystroke    (GtkWidget            *widget,
                                                       gboolean              setting);
void       terminal_widget_set_scroll_on_output       (GtkWidget            *widget,
                                                       gboolean              setting);
void       terminal_widget_set_scrollback_lines       (GtkWidget            *widget,
                                                       int                   lines);
void       terminal_widget_set_background_image       (GtkWidget            *widget,
                                                       GdkPixbuf            *pixbuf);
void       terminal_widget_set_background_image_file  (GtkWidget            *widget,
                                                       const char           *fname);
void       terminal_widget_set_background_transparent (GtkWidget            *widget,
                                                       gboolean              setting);
/* 0.0 = normal bg, 1.0 = all black bg, 0.5 = half darkened */
void       terminal_widget_set_background_darkness    (GtkWidget            *widget,
                                                       double                factor);
void       terminal_widget_set_background_scrolls     (GtkWidget            *widget,
                                                       gboolean              setting);
void       terminal_widget_set_normal_gdk_font        (GtkWidget            *widget,
                                                       GdkFont              *font);
void       terminal_widget_set_bold_gdk_font          (GtkWidget            *widget,
                                                       GdkFont              *font);
void       terminal_widget_set_allow_bold             (GtkWidget            *widget,
                                                       gboolean              setting);
void       terminal_widget_set_colors                 (GtkWidget            *widget,
                                                       const GdkColor       *foreground,
                                                       const GdkColor       *background,
                                                       const GdkColor       *palette_entries);
void       terminal_widget_copy_clipboard             (GtkWidget            *widget);
void       terminal_widget_paste_clipboard            (GtkWidget            *widget);
void       terminal_widget_reset                      (GtkWidget            *widget,
                                                       gboolean              also_clear_afterward);

void terminal_widget_connect_title_changed         (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_disconnect_title_changed      (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_connect_icon_title_changed    (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_disconnect_icon_title_changed (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_connect_child_died            (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_disconnect_child_died         (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_connect_selection_changed     (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_disconnect_selection_changed  (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_connect_encoding_changed      (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);
void terminal_widget_disconnect_encoding_changed   (GtkWidget *widget,
                                                    GCallback  callback,
                                                    void      *data);

const char* terminal_widget_get_title         (GtkWidget *widget);
const char* terminal_widget_get_icon_title    (GtkWidget *widget);
gboolean    terminal_widget_get_has_selection (GtkWidget *widget);

GtkAdjustment* terminal_widget_get_scroll_adjustment (GtkWidget *widget);

gboolean terminal_widget_fork_command      (GtkWidget   *widget,
                                            gboolean     lastlog,
                                            gboolean     update_records,
                                            const char  *path,
                                            char       **argv,
                                            char       **envp,
                                            const char  *working_dir,
                                            int         *child_pid,
                                            GError     **err);

int terminal_widget_get_estimated_bytes_per_scrollback_line (void);

void terminal_widget_write_data_to_child (GtkWidget  *widget,
                                          const char *data,
                                          int         len);

void terminal_widget_set_pango_font (GtkWidget                  *widget,
				     const PangoFontDescription *font_desc,
				     gboolean 			anti_alias);

gboolean terminal_widget_supports_pango_fonts (void);

const char* terminal_widget_get_encoding (GtkWidget  *widget);
void        terminal_widget_set_encoding (GtkWidget  *widget,
                                          const char *encoding);

gboolean terminal_widget_supports_dynamic_encoding (void);

void terminal_widget_im_append_menuitems(GtkWidget    *wiget,
					 GtkMenuShell *menushell);

G_END_DECLS

#endif /* TERMINAL_WIDGET_H */
