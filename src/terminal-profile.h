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
  TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR = 1 << 2,
  TERMINAL_SETTING_FOREGROUND_COLOR     = 1 << 3,
  TERMINAL_SETTING_BACKGROUND_COLOR     = 1 << 4,
  TERMINAL_SETTING_TITLE                = 1 << 5,
  TERMINAL_SETTING_TITLE_MODE           = 1 << 6,
  TERMINAL_SETTING_ALLOW_BOLD           = 1 << 7,
  TERMINAL_SETTING_SILENT_BELL          = 1 << 8,
  TERMINAL_SETTING_WORD_CHARS           = 1 << 9,
  TERMINAL_SETTING_SCROLLBAR_POSITION   = 1 << 10,
  TERMINAL_SETTING_SCROLLBACK_LINES     = 1 << 11,
  TERMINAL_SETTING_SCROLL_ON_KEYSTROKE  = 1 << 12,
  TERMINAL_SETTING_SCROLL_ON_OUTPUT     = 1 << 13,
  TERMINAL_SETTING_EXIT_ACTION          = 1 << 14,
  TERMINAL_SETTING_LOGIN_SHELL          = 1 << 15,
  TERMINAL_SETTING_UPDATE_RECORDS       = 1 << 16,
  TERMINAL_SETTING_USE_CUSTOM_COMMAND   = 1 << 17,
  TERMINAL_SETTING_CUSTOM_COMMAND       = 1 << 18,
  TERMINAL_SETTING_ICON                 = 1 << 19
} TerminalSettingMask;

typedef enum
{
  /* this has to be kept in sync with the option menu in the
   * glade file
   */
  TERMINAL_TITLE_REPLACE,
  TERMINAL_TITLE_BEFORE,
  TERMINAL_TITLE_AFTER,
  TERMINAL_TITLE_IGNORE
} TerminalTitleMode;

typedef enum
{
  TERMINAL_DELETE_CONTROL_H,
  TERMINAL_DELETE_ESCAPE_SEQUENCE,
  TERMINAL_DELETE_ASCII_DEL
} TerminalDeleteBinding;

typedef enum
{
  TERMINAL_PALETTE_LINUX,
  TERMINAL_PALETTE_XTERM,
  TERMINAL_PALETTE_RXVT,
  TERMINAL_PALETTE_CUSTOM
} TerminalPaletteType;

typedef enum
{
  TERMINAL_SCROLLBAR_LEFT,
  TERMINAL_SCROLLBAR_RIGHT,
  TERMINAL_SCROLLBAR_HIDDEN
} TerminalScrollbarPosition;

typedef enum 
{
  TERMINAL_EXIT_CLOSE,
  TERMINAL_EXIT_RESTART
} TerminalExitAction;

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

TerminalProfile* terminal_profile_new (const char  *name,
                                       GConfClient *conf);

const char*               terminal_profile_get_name                 (TerminalProfile *profile);
const char*               terminal_profile_get_visible_name         (TerminalProfile *profile);
gboolean                  terminal_profile_get_cursor_blink         (TerminalProfile *profile);
gboolean                  terminal_profile_get_allow_bold           (TerminalProfile *profile);
gboolean                  terminal_profile_get_silent_bell          (TerminalProfile *profile);
TerminalScrollbarPosition terminal_profile_get_scrollbar_position   (TerminalProfile *profile);
int                       terminal_profile_get_scrollback_lines     (TerminalProfile *profile);
void                      terminal_profile_get_color_scheme         (TerminalProfile *profile,
                                                                     GdkColor        *foreground,
                                                                     GdkColor        *background);
const char*               terminal_profile_get_word_chars           (TerminalProfile *profile);
const char*               terminal_profile_get_title                (TerminalProfile *profile);
TerminalTitleMode         terminal_profile_get_title_mode           (TerminalProfile *profile);
gboolean                  terminal_profile_get_forgotten            (TerminalProfile *profile);
gboolean                  terminal_profile_get_default_show_menubar (TerminalProfile *profile);
gboolean                  terminal_profile_get_scroll_on_keystroke  (TerminalProfile *profile);
gboolean                  terminal_profile_get_scroll_on_output     (TerminalProfile *profile);

TerminalExitAction        terminal_profile_get_exit_action          (TerminalProfile *profile);
gboolean                  terminal_profile_get_login_shell          (TerminalProfile *profile);
gboolean                  terminal_profile_get_update_records       (TerminalProfile *profile);
gboolean                  terminal_profile_get_use_custom_command   (TerminalProfile *profile);
const char*               terminal_profile_get_custom_command       (TerminalProfile *profile);

const char*               terminal_profile_get_icon_file            (TerminalProfile *profile);
GdkPixbuf*                terminal_profile_get_icon                 (TerminalProfile *profile);

void terminal_profile_set_cursor_blink         (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_visible_name         (TerminalProfile           *profile,
                                                const char                *name);
void terminal_profile_set_allow_bold           (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_silent_bell          (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_scrollbar_position   (TerminalProfile           *profile,
                                                TerminalScrollbarPosition  pos);
void terminal_profile_set_scrollback_lines     (TerminalProfile           *profile,
                                                int                        lines);
void terminal_profile_set_color_scheme         (TerminalProfile           *profile,
                                                const GdkColor            *foreground,
                                                const GdkColor            *background);
void terminal_profile_set_title                (TerminalProfile           *profile,
                                                const char                *title);
void terminal_profile_set_title_mode           (TerminalProfile           *profile,
                                                TerminalTitleMode          mode);
void terminal_profile_set_word_chars           (TerminalProfile           *profile,
                                                const char                *word_class);
void terminal_profile_set_default_show_menubar (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_scroll_on_keystroke  (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_scroll_on_output     (TerminalProfile           *profile,
                                                gboolean                   setting);

void terminal_profile_set_exit_action          (TerminalProfile           *profile,
                                                TerminalExitAction         action);
void terminal_profile_set_login_shell          (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_update_records       (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_use_custom_command   (TerminalProfile           *profile,
                                                gboolean                   setting);
void terminal_profile_set_custom_command       (TerminalProfile          *profile,
                                                const char               *command);

void terminal_profile_set_icon_file            (TerminalProfile          *profile,
                                                const char               *filename);

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
