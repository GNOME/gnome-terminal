/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
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

#ifndef TERMINAL_PROFILE_H
#define TERMINAL_PROFILE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  /* this has to be kept in sync with the option menu in the profile editor UI file */
  TERMINAL_TITLE_REPLACE,
  TERMINAL_TITLE_BEFORE,
  TERMINAL_TITLE_AFTER,
  TERMINAL_TITLE_IGNORE
} TerminalTitleMode;

typedef enum
{
  TERMINAL_SCROLLBAR_LEFT,
  TERMINAL_SCROLLBAR_RIGHT,
  TERMINAL_SCROLLBAR_HIDDEN
} TerminalScrollbarPosition;

typedef enum 
{
  TERMINAL_EXIT_CLOSE,
  TERMINAL_EXIT_RESTART,
  TERMINAL_EXIT_HOLD
} TerminalExitAction;

typedef enum
{
  TERMINAL_BACKGROUND_SOLID,
  TERMINAL_BACKGROUND_IMAGE,
  TERMINAL_BACKGROUND_TRANSPARENT
} TerminalBackgroundType;

#define TERMINAL_PALETTE_SIZE 16

#define TERMINAL_PALETTE_TANGO 0
#define TERMINAL_PALETTE_LINUX 1
#define TERMINAL_PALETTE_XTERM 2
#define TERMINAL_PALETTE_RXVT 3
#define TERMINAL_PALETTE_N_BUILTINS 4

/* Property names */
#define TERMINAL_PROFILE_ALLOW_BOLD             "allow-bold"
#define TERMINAL_PROFILE_BACKGROUND_COLOR       "background-color"
#define TERMINAL_PROFILE_BACKGROUND_DARKNESS    "background-darkness"
#define TERMINAL_PROFILE_BACKGROUND_IMAGE       "background-image"
#define TERMINAL_PROFILE_BACKGROUND_IMAGE_FILE  "background-image-file"
#define TERMINAL_PROFILE_BACKGROUND_TYPE        "background-type"
#define TERMINAL_PROFILE_BACKSPACE_BINDING      "backspace-binding"
#define TERMINAL_PROFILE_BOLD_COLOR             "bold-color"
#define TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG  "bold-color-same-as-fg"
#define TERMINAL_PROFILE_CURSOR_BLINK_MODE      "cursor-blink-mode"
#define TERMINAL_PROFILE_CURSOR_SHAPE           "cursor-shape"
#define TERMINAL_PROFILE_CUSTOM_COMMAND         "custom-command"
#define TERMINAL_PROFILE_DEFAULT_SHOW_MENUBAR   "default-show-menubar"
#define TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS   "default-size-columns"
#define TERMINAL_PROFILE_DEFAULT_SIZE_ROWS      "default-size-rows"
#define TERMINAL_PROFILE_DELETE_BINDING         "delete-binding"
#define TERMINAL_PROFILE_ENCODING               "encoding"
#define TERMINAL_PROFILE_EXIT_ACTION            "exit-action"
#define TERMINAL_PROFILE_FONT                   "font"
#define TERMINAL_PROFILE_FOREGROUND_COLOR       "foreground-color"
#define TERMINAL_PROFILE_LOGIN_SHELL            "login-shell"
#define TERMINAL_PROFILE_NAME                   "name"
#define TERMINAL_PROFILE_PALETTE                "palette"
#define TERMINAL_PROFILE_SCROLL_BACKGROUND      "scroll-background"
#define TERMINAL_PROFILE_SCROLLBACK_LINES       "scrollback-lines"
#define TERMINAL_PROFILE_SCROLLBACK_UNLIMITED   "scrollback-unlimited"
#define TERMINAL_PROFILE_SCROLLBAR_POSITION     "scrollbar-position"
#define TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE    "scroll-on-keystroke"
#define TERMINAL_PROFILE_SCROLL_ON_OUTPUT       "scroll-on-output"
#define TERMINAL_PROFILE_SILENT_BELL            "silent-bell"
#define TERMINAL_PROFILE_TITLE_MODE             "title-mode"
#define TERMINAL_PROFILE_TITLE                  "title"
#define TERMINAL_PROFILE_UPDATE_RECORDS         "update-records"
#define TERMINAL_PROFILE_USE_CUSTOM_COMMAND     "use-custom-command"
#define TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE "use-custom-default-size"
#define TERMINAL_PROFILE_USE_SYSTEM_FONT        "use-system-font"
#define TERMINAL_PROFILE_USE_THEME_COLORS       "use-theme-colors"
#define TERMINAL_PROFILE_VISIBLE_NAME           "visible-name"
#define TERMINAL_PROFILE_WORD_CHARS             "word-chars"

/* TerminalProfile object */

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

  void (* forgotten) (TerminalProfile           *profile);

  GHashTable *gconf_keys;
};

GType             terminal_profile_get_type               (void);

TerminalProfile* _terminal_profile_new                    (const char *name);

void             _terminal_profile_forget                 (TerminalProfile *profile);

gboolean         _terminal_profile_get_forgotten          (TerminalProfile *profile);

TerminalProfile* _terminal_profile_clone                  (TerminalProfile *base_profile,
                                                           const char *visible_name);

gboolean          terminal_profile_property_locked        (TerminalProfile *profile,
                                                           const char *prop_name);

void              terminal_profile_reset_property         (TerminalProfile *profile,
                                                           const char *prop_name);

gboolean          terminal_profile_get_property_boolean   (TerminalProfile *profile,
                                                           const char *prop_name);

gconstpointer     terminal_profile_get_property_boxed     (TerminalProfile *profile,
                                                           const char *prop_name);

double            terminal_profile_get_property_double    (TerminalProfile *profile,
                                                           const char *prop_name);

int               terminal_profile_get_property_enum      (TerminalProfile *profile,
                                                           const char *prop_name);

int               terminal_profile_get_property_int       (TerminalProfile *profile,
                                                           const char *prop_name);

gpointer          terminal_profile_get_property_object    (TerminalProfile *profile,
                                                           const char *prop_name);

const char*       terminal_profile_get_property_string    (TerminalProfile *profile,
                                                           const char *prop_name);

gboolean          terminal_profile_get_palette            (TerminalProfile *profile,
                                                           GdkColor *colors,
                                                           guint *n_colors);

gboolean          terminal_profile_get_palette_is_builtin (TerminalProfile *profile,
                                                           guint *n);

void              terminal_profile_set_palette_builtin    (TerminalProfile *profile,
                                                           guint n);

gboolean          terminal_profile_modify_palette_entry   (TerminalProfile *profile,
                                                           guint            i,
                                                           const GdkColor  *color);

G_END_DECLS

#endif /* TERMINAL_PROFILE_H */
