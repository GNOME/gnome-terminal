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

#define CONF_PREFIX "/apps/gnome-terminal"
#define CONF_GLOBAL_PREFIX CONF_PREFIX"/global"
#define CONF_PROFILES_PREFIX CONF_PREFIX"/profiles"
#define CONF_KEYS_PREFIX CONF_PREFIX"/keybindings"
#define FALLBACK_PROFILE_ID "Default"

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
  TERMINAL_SETTING_ICON                 = 1 << 19,
  TERMINAL_SETTING_IS_DEFAULT           = 1 << 20,
  TERMINAL_SETTING_PALETTE              = 1 << 21,
  TERMINAL_SETTING_X_FONT               = 1 << 22,
  TERMINAL_SETTING_BACKGROUND_TYPE      = 1 << 23,
  TERMINAL_SETTING_BACKGROUND_IMAGE     = 1 << 24,
  TERMINAL_SETTING_SCROLL_BACKGROUND    = 1 << 25,
  TERMINAL_SETTING_BACKGROUND_DARKNESS  = 1 << 26,
  TERMINAL_SETTING_BACKSPACE_BINDING    = 1 << 27,
  TERMINAL_SETTING_DELETE_BINDING       = 1 << 28

  /* When we get to 31 we need to do something else ;-) */
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
  TERMINAL_ERASE_ASCII_DEL,
  TERMINAL_ERASE_ESCAPE_SEQUENCE,
  TERMINAL_ERASE_CONTROL_H
} TerminalEraseBinding;

#define TERMINAL_PALETTE_SIZE 16

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

typedef enum
{
  TERMINAL_BACKGROUND_SOLID,
  TERMINAL_BACKGROUND_IMAGE,
  TERMINAL_BACKGROUND_TRANSPARENT
} TerminalBackgroundType;

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
gboolean                  terminal_profile_get_is_default           (TerminalProfile *profile);
void                      terminal_profile_get_palette              (TerminalProfile *profile,
                                                                     GdkColor        *colors);
const char*               terminal_profile_get_x_font               (TerminalProfile *profile);

TerminalBackgroundType terminal_profile_get_background_type       (TerminalProfile *profile);
GdkPixbuf*             terminal_profile_get_background_image      (TerminalProfile *profile);
const char*            terminal_profile_get_background_image_file (TerminalProfile *profile);
gboolean               terminal_profile_get_scroll_background     (TerminalProfile *profile);
double                 terminal_profile_get_background_darkness   (TerminalProfile *profile);
TerminalEraseBinding   terminal_profile_get_backspace_binding     (TerminalProfile *profile);
TerminalEraseBinding   terminal_profile_get_delete_binding        (TerminalProfile *profile);


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
void terminal_profile_set_is_default           (TerminalProfile          *profile,
                                                gboolean                  setting);
void terminal_profile_set_palette              (TerminalProfile *profile,
                                                const GdkColor  *colors);
void terminal_profile_set_palette_entry        (TerminalProfile *profile,
                                                int              i,
                                                const GdkColor  *color);
void terminal_profile_set_x_font               (TerminalProfile *profile,
                                                const char      *name);

void terminal_profile_set_background_type       (TerminalProfile        *profile,
                                                 TerminalBackgroundType  type);
void terminal_profile_set_background_image_file (TerminalProfile        *profile,
                                                 const char             *filename);
void terminal_profile_set_scroll_background     (TerminalProfile        *profile,
                                                 gboolean                setting);
void terminal_profile_set_background_darkness   (TerminalProfile        *profile,
                                                 double                  setting);
void terminal_profile_set_backspace_binding     (TerminalProfile        *profile,
                                                 TerminalEraseBinding    binding);
void terminal_profile_set_delete_binding        (TerminalProfile        *profile,
                                                 TerminalEraseBinding    binding);

void terminal_profile_reset_compat_defaults     (TerminalProfile        *profile);

TerminalProfile* terminal_profile_ensure_fallback        (GConfClient     *conf);
void             terminal_profile_initialize             (GConfClient     *conf);
GList*           terminal_profile_get_list               (void);
int              terminal_profile_get_count              (void);
/* may return NULL */
TerminalProfile* terminal_profile_get_default            (void);
/* never returns NULL if any profiles exist, one is always supposed to */
TerminalProfile* terminal_profile_get_for_new_term       (void);
TerminalProfile* terminal_profile_lookup                 (const char      *name);
TerminalProfile* terminal_profile_lookup_by_visible_name (const char      *name);
void             terminal_profile_forget                 (TerminalProfile *profile);

TerminalSettingMask terminal_profile_get_locked_settings (TerminalProfile *profile);

void terminal_profile_update (TerminalProfile *profile);

void terminal_profile_create (TerminalProfile *base_profile,
                              const char      *visible_name,
                              GtkWindow       *transient_parent);

void terminal_profile_delete_list (GConfClient *conf,
                                   GList      *list,
                                   GtkWindow  *transient_parent);

extern const GdkColor terminal_palette_linux[TERMINAL_PALETTE_SIZE];
extern const GdkColor terminal_palette_xterm[TERMINAL_PALETTE_SIZE];
extern const GdkColor terminal_palette_rxvt[TERMINAL_PALETTE_SIZE];

char*    terminal_palette_to_string   (const GdkColor *palette);
gboolean terminal_palette_from_string (const char     *str,
                                       GdkColor       *palette,
                                       gboolean        warn);


/* Used to convert the double "darkness" setting into a bool.
 * Temporary hack.
 */
#define DARKNESS_THRESHOLD 0.25
#define DARKNESS_TRUE  0.5
#define DARKNESS_FALSE 0.0

G_END_DECLS

#endif /* TERMINAL_PROFILE_H */
