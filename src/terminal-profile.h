/* object representing a profile */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef TERMINAL_PROFILE_H
#define TERMINAL_PROFILE_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#define CONF_PREFIX "/apps/profterm"
#define CONF_GLOBAL_PREFIX CONF_PREFIX"/global"
#define CONF_PROFILES_PREFIX CONF_PREFIX"/profiles"
#define DEFAULT_PROFILE "Default"

typedef enum
{
  TERMINAL_SETTING_VISIBLE_NAME         = 1 << 0,
  TERMINAL_SETTING_CURSOR_BLINK         = 1 << 1,
  TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR = 1 << 2
} TerminalSettingMask;

typedef enum
{
  TERMINAL_DELETE_CONTROL_H,
  TERMINAL_DELETE_ESCAPE_SEQUENCE,
  TERMINAL_DELETE_ASCII_DEL
} TerminalDeleteBinding;

typedef enum
{
  TERMINAL_COLORS_WHITE_ON_BLACK,
  TERMINAL_COLORS_BLACK_ON_WHITE,
  TERMINAL_COLORS_GREEN_ON_BLACK,
  TERMINAL_COLORS_BLACK_ON_LIGHT_YELLOW,
  TERMINAL_COLORS_CUSTOM
} TerminalColorScheme;

typedef enum
{
  TERMINAL_PALETTE_LINUX,
  TERMINAL_PALETTE_XTERM,
  TERMINAL_PALETTE_RXVT,
  TERMINAL_PALETTE_CUSTOM
} TerminalPaletteType;

typedef enum {
  TERMINAL_SCROLLBAR_LEFT   = 0,
  TERMINAL_SCROLLBAR_RIGHT  = 1,
  TERMINAL_SCROLLBAR_HIDDEN = 2
} TerminalScrollbarPosition;

G_BEGIN_DECLS

#define TERMINAL_TYPE_PROFILE              (terminal_profile_get_type ())
#define TERMINAL_PROFILE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_PROFILE, TerminalProfile))
#define TERMINAL_PROFILE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_PROFILE, TerminalProfileClass))
#define TERMINAL_IS_PROFILE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_PROFILE))
#define TERMINAL_IS_PROFILE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_PROFILE))
#define TERMINAL_PROFILE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_PROFILE, TerminalProfileClass))

typedef struct _TerminalProfile        TerminalProfile;
typedef struct _TerminalProfileClass   TerminalProfileClass;
typedef struct _TerminalProfilePrivate TerminalProfilePrivate;

struct _TerminalProfile
{
  GObject parent_instance;

  TerminalProfilePrivate *priv;
};

struct _TerminalProfileClass
{
  GObjectClass parent_class;

  void (* changed)   (TerminalProfile           *profile,
                      TerminalSettingMask        mask);
  void (* forgotten) (TerminalProfile           *profile);
};

GType terminal_profile_get_type (void) G_GNUC_CONST;

TerminalProfile*          terminal_profile_new                    (const char       *name,

                                                                   GConfClient *conf);
const char*               terminal_profile_get_name               (TerminalProfile  *profile);
const char*               terminal_profile_get_visible_name       (TerminalProfile  *profile);
gboolean                  terminal_profile_get_audible_bell       (TerminalProfile  *profile);
gboolean                  terminal_profile_get_cursor_blink       (TerminalProfile  *profile);
gboolean                  terminal_profile_get_scroll_on_keypress (TerminalProfile  *profile);
gboolean                  terminal_profile_get_login_shell        (TerminalProfile  *profile);
gboolean                  terminal_profile_get_scroll_background  (TerminalProfile  *profile);
gboolean                  terminal_profile_get_use_bold           (TerminalProfile  *profile);
TerminalDeleteBinding     terminal_profile_get_delete_key         (TerminalProfile  *profile);
TerminalDeleteBinding     terminal_profile_get_backspace_key      (TerminalProfile  *profile);
TerminalPaletteType       terminal_profile_get_palette_type       (TerminalProfile  *profile);
TerminalScrollbarPosition terminal_profile_get_scrollbar_position (TerminalProfile  *profile);
const char*               terminal_profile_get_font               (TerminalProfile  *profile);
int                       terminal_profile_get_scrollback_lines   (TerminalProfile  *profile);
gboolean                  terminal_profile_get_update_records     (TerminalProfile  *profile);
void                      terminal_profile_get_color_scheme       (TerminalProfile  *profile,
                                                                   GdkColor         *foreground,
                                                                   GdkColor         *background);
void                      terminal_profile_get_palette            (TerminalProfile  *profile,
                                                                   GdkColor        **colors,
                                                                   int               n_colors);
gboolean                  terminal_profile_get_transparent        (TerminalProfile  *profile);
gboolean                  terminal_profile_get_shaded             (TerminalProfile  *profile);
GdkPixmap*                terminal_profile_get_background_pixmap  (TerminalProfile  *profile);
const char*               terminal_profile_get_word_class         (TerminalProfile  *profile);
const char*               terminal_profile_get_term_variable      (TerminalProfile  *profile);
gboolean                  terminal_profile_get_lock_title         (TerminalProfile  *profile);
const char*               terminal_profile_get_title              (TerminalProfile  *profile);
gboolean                  terminal_profile_get_forgotten          (TerminalProfile  *profile);
gboolean                  terminal_profile_get_default_show_menubar (TerminalProfile *profile);

void            terminal_profile_set_keyboard_secured   (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_audible_bell       (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_cursor_blink       (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_visible_name       (TerminalProfile *profile,
                                                         const char      *name);
void            terminal_profile_set_scroll_on_keypress (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_login_shell        (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_scroll_background  (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_use_bold           (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_delete_key         (TerminalProfile            *profile,
                                                        TerminalDeleteBinding      binding);
void            terminal_profile_set_backspace_key      (TerminalProfile            *profile,
                                                        TerminalDeleteBinding      binding);
void            terminal_profile_set_scrollbar_position (TerminalProfile            *profile,
                                                        TerminalScrollbarPosition  pos);
void            terminal_profile_set_font               (TerminalProfile            *profile,
                                                        const char                *str);
void            terminal_profile_set_scrollback_lines   (TerminalProfile            *profile,
                                                        int                        lines);
void            terminal_profile_set_update_records     (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_color_scheme       (TerminalProfile            *profile,
                                                        const GdkColor            *foreground,
                                                        const GdkColor            *background);
void            terminal_profile_set_transparent        (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_shaded             (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_background_pixmap  (TerminalProfile            *profile,
                                                        GdkPixmap                 *pixmap);
void            terminal_profile_set_title              (TerminalProfile            *profile,
                                                        const char                *title);
void            terminal_profile_set_lock_title         (TerminalProfile            *profile,
                                                        gboolean                   setting);
void            terminal_profile_set_word_class         (TerminalProfile            *profile,
                                                        const char                *word_class);
void            terminal_profile_set_term_variable      (TerminalProfile            *profile,
                                                        const char                *term_variable);
void            terminal_profile_set_palette            (TerminalProfile            *profile,
                                                        const GdkColor            *palette,
                                                        int                        n_colors);
void            terminal_profile_set_default_show_menubar (TerminalProfile *profile,
                                                           gboolean         setting);


void             terminal_profile_setup_default (GConfClient *conf);
GList*           terminal_profile_get_list (void);
TerminalProfile* terminal_profile_lookup                 (const char      *name);
TerminalProfile* terminal_profile_lookup_by_visible_name (const char      *name);
void             terminal_profile_forget   (TerminalProfile *profile);

TerminalSettingMask terminal_profile_get_locked_settings (TerminalProfile *profile);

void terminal_profile_update (TerminalProfile *profile);

void terminal_profile_create (TerminalProfile *base_profile,
                              const char      *visible_name,
                              GtkWindow       *transient_parent);

void terminal_profile_delete_list (GConfClient *conf,
                                   GList      *list,
                                   GtkWindow  *transient_parent);

G_END_DECLS

#endif /* TERMINAL_PROFILE_H */
