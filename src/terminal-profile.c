/* object representing a profile */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Mathias Hasselmann
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

#include "terminal-intl.h"
#include "terminal-profile.h"
#include <gtk/gtk.h>
#include <libgnome/gnome-program.h>
#include <string.h>
#include <stdlib.h>

/* If you add a key, you need to update code:
 * 
 *  - in the function that sets the key
 *  - in the update function that reads all keys on startup
 *  - in the profile_change_notify function
 *  - in the function that copies base profiles to new profiles
 *  - in terminal_profile_init() initial value, sometimes
 *    (only e.g. if the item is non-null by invariant)
 *
 * This sucks. ;-)
 */
#define KEY_VISIBLE_NAME "visible_name"
#define KEY_CURSOR_BLINK "cursor_blink"
#define KEY_DEFAULT_SHOW_MENUBAR "default_show_menubar"
#define KEY_FOREGROUND_COLOR "foreground_color"
#define KEY_BACKGROUND_COLOR "background_color"
#define KEY_TITLE "title"
#define KEY_TITLE_MODE "title_mode"
#define KEY_ALLOW_BOLD "allow_bold"
#define KEY_SILENT_BELL "silent_bell"
#define KEY_WORD_CHARS "word_chars"
#define KEY_SCROLLBAR_POSITION "scrollbar_position"
#define KEY_SCROLLBACK_LINES "scrollback_lines"
#define KEY_SCROLL_ON_KEYSTROKE "scroll_on_keystroke"
#define KEY_SCROLL_ON_OUTPUT "scroll_on_output"
#define KEY_EXIT_ACTION "exit_action"
#define KEY_LOGIN_SHELL "login_shell"
#define KEY_UPDATE_RECORDS "update_records"
#define KEY_USE_CUSTOM_COMMAND "use_custom_command"
#define KEY_CUSTOM_COMMAND "custom_command"
#define KEY_ICON "icon"
#define KEY_PALETTE "palette"
#define KEY_X_FONT "x_font"
#define KEY_BACKGROUND_TYPE "background_type"
#define KEY_BACKGROUND_IMAGE "background_image"
#define KEY_SCROLL_BACKGROUND "scroll_background"
#define KEY_BACKGROUND_DARKNESS "background_darkness"
#define KEY_BACKSPACE_BINDING "backspace_binding"
#define KEY_DELETE_BINDING "delete_binding"
#define KEY_USE_THEME_COLORS "use_theme_colors"
#define KEY_USE_SYSTEM_FONT "use_system_font"
#define KEY_USE_SKEY "use_skey"
#define KEY_FONT "font"

struct _TerminalProfilePrivate
{
  char *name;
  char *profile_dir;
  GConfClient *conf;
  guint notify_id;
  TerminalSettingMask locked;
  /* can't set keys when reporting a key changed,
   * avoids a bunch of pesky signal handler blocks
   * in profile-editor.c.
   *
   * As backup, we don't emit "changed" when values
   * didn't really change.
   */
  int in_notification_count;
  char *visible_name;
  GdkColor foreground;
  GdkColor background;
  char *title;
  TerminalTitleMode title_mode;
  char *word_chars;
  TerminalScrollbarPosition scrollbar_position;
  int scrollback_lines;
  TerminalExitAction exit_action;
  char *custom_command;

  char *icon_file;
  GdkPixbuf *icon;

  GdkColor palette[TERMINAL_PALETTE_SIZE];

  char *x_font;

  TerminalBackgroundType background_type;
  char *background_image_file;
  GdkPixbuf *background_image;
  double background_darkness;
  TerminalEraseBinding backspace_binding;
  TerminalEraseBinding delete_binding;

  PangoFontDescription *font;
  
  guint icon_load_failed : 1;
  guint background_load_failed : 1;
  
  guint cursor_blink : 1;
  guint default_show_menubar : 1;
  guint allow_bold : 1;
  guint silent_bell : 1;
  guint scroll_on_keystroke : 1;
  guint scroll_on_output : 1;
  guint login_shell : 1;
  guint update_records : 1;
  guint use_custom_command : 1;
  guint scroll_background : 1;
  guint use_theme_colors : 1;
  guint use_system_font : 1;
  guint use_skey : 1;
  guint forgotten : 1;
};

static gboolean
constcorrect_string_to_enum (const GConfEnumStringPair *table,
                             const char                *str,
                             int                       *outp)
{
  return gconf_string_to_enum ((GConfEnumStringPair*)table, str, outp);
}

static const char*
constcorrect_enum_to_string (const GConfEnumStringPair *table,
                             int                        val)
{
  return gconf_enum_to_string ((GConfEnumStringPair*)table, val);
}

#define gconf_string_to_enum(table, str, outp) \
   constcorrect_string_to_enum (table, str, outp)
#define gconf_enum_to_string(table, val) \
   constcorrect_enum_to_string (table, val)

static const GConfEnumStringPair title_modes[] = {
  { TERMINAL_TITLE_REPLACE, "replace" },
  { TERMINAL_TITLE_BEFORE, "before" },
  { TERMINAL_TITLE_AFTER, "after" },
  { TERMINAL_TITLE_IGNORE, "ignore" },
  { -1, NULL }
};

static const GConfEnumStringPair scrollbar_positions[] = {
  { TERMINAL_SCROLLBAR_LEFT, "left" },
  { TERMINAL_SCROLLBAR_RIGHT, "right" },
  { TERMINAL_SCROLLBAR_HIDDEN, "hidden" },  
  { -1, NULL }
};

static const GConfEnumStringPair exit_actions[] = {
  { TERMINAL_EXIT_CLOSE, "close" },
  { TERMINAL_EXIT_RESTART, "restart" },
  { TERMINAL_EXIT_HOLD, "hold" },
  { -1, NULL }
};

static const GConfEnumStringPair erase_bindings[] = {
  { TERMINAL_ERASE_CONTROL_H, "control-h" },
  { TERMINAL_ERASE_ESCAPE_SEQUENCE, "escape-sequence" },
  { TERMINAL_ERASE_ASCII_DEL, "ascii-del" },
  { -1, NULL }
};

static const GConfEnumStringPair background_types[] = {
  { TERMINAL_BACKGROUND_SOLID, "solid" },
  { TERMINAL_BACKGROUND_IMAGE, "image" },
  { TERMINAL_BACKGROUND_TRANSPARENT, "transparent" },
  { -1, NULL }
};

static GHashTable *profiles = NULL;
static char* default_profile_id = NULL;
static TerminalProfile *default_profile = NULL;
static gboolean default_profile_locked = FALSE;

#define RETURN_IF_NOTIFYING(profile) if ((profile)->priv->in_notification_count) return

enum {
  CHANGED,
  FORGOTTEN,
  LAST_SIGNAL
};

static void terminal_profile_init        (TerminalProfile      *profile);
static void terminal_profile_class_init  (TerminalProfileClass *klass);
static void terminal_profile_finalize    (GObject              *object);

static void profile_change_notify        (GConfClient *client,
                                          guint        cnxn_id,
                                          GConfEntry  *entry,
                                          gpointer     user_data);

static void default_change_notify        (GConfClient *client,
                                          guint        cnxn_id,
                                          GConfEntry  *entry,
                                          gpointer     user_data);

static void update_default_profile       (const char  *name,
                                          gboolean     locked);

static void emit_changed (TerminalProfile           *profile,
                          const TerminalSettingMask *mask);


static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
terminal_profile_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (TerminalProfileClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) terminal_profile_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (TerminalProfile),
        0,              /* n_preallocs */
        (GInstanceInitFunc) terminal_profile_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "TerminalProfile",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
terminal_profile_init (TerminalProfile *profile)
{
  g_return_if_fail (profiles != NULL);
  
  profile->priv = g_new0 (TerminalProfilePrivate, 1);
  terminal_setting_mask_clear (&profile->priv->locked);
  profile->priv->cursor_blink = FALSE;
  profile->priv->default_show_menubar = TRUE;
  profile->priv->visible_name = g_strdup ("<not named>");
  profile->priv->foreground.red = 0;
  profile->priv->foreground.green = 0;
  profile->priv->foreground.blue = 0;
  profile->priv->background.red = 0xFFFF;
  profile->priv->background.green = 0xFFFF;
  profile->priv->background.blue = 0xDDDD;
  profile->priv->in_notification_count = 0;
  profile->priv->title_mode = TERMINAL_TITLE_REPLACE;
  profile->priv->title = g_strdup (_("Terminal"));
  profile->priv->scrollbar_position = TERMINAL_SCROLLBAR_RIGHT;
  profile->priv->scrollback_lines = 1000;
  profile->priv->allow_bold = TRUE;
  profile->priv->word_chars = g_strdup ("");
  profile->priv->custom_command = g_strdup ("");
  profile->priv->icon_file = g_strdup ("gnome-terminal.png");
  memcpy (profile->priv->palette,
          terminal_palette_linux,
          TERMINAL_PALETTE_SIZE * sizeof (GdkColor));
  profile->priv->x_font = g_strdup ("fixed");
  profile->priv->background_type = TERMINAL_BACKGROUND_SOLID;
  profile->priv->background_image_file = g_strdup ("");
  profile->priv->background_darkness = 0.0;
  profile->priv->backspace_binding = TERMINAL_ERASE_ASCII_DEL;
  profile->priv->delete_binding = TERMINAL_ERASE_ESCAPE_SEQUENCE;
  profile->priv->use_theme_colors = TRUE;
  profile->priv->use_system_font = TRUE;
  profile->priv->use_skey = TRUE;
  profile->priv->font = pango_font_description_new ();
  pango_font_description_set_family (profile->priv->font,
                                     "monospace");
  pango_font_description_set_size (profile->priv->font,
                                   PANGO_SCALE * 12);
}

static void
terminal_profile_class_init (TerminalProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = terminal_profile_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalProfileClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  signals[FORGOTTEN] =
    g_signal_new ("forgotten",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalProfileClass, forgotten),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
terminal_profile_finalize (GObject *object)
{
  TerminalProfile *profile;

  profile = TERMINAL_PROFILE (object);

  terminal_profile_forget (profile);
  
  gconf_client_notify_remove (profile->priv->conf,
                              profile->priv->notify_id);
  profile->priv->notify_id = 0;

  g_object_unref (G_OBJECT (profile->priv->conf));

  g_free (profile->priv->visible_name);
  g_free (profile->priv->name);
  g_free (profile->priv->title);
  g_free (profile->priv->profile_dir);
  g_free (profile->priv->icon_file);
  if (profile->priv->icon)
    g_object_unref (G_OBJECT (profile->priv->icon));
  g_free (profile->priv->x_font);

  g_free (profile->priv->background_image_file);
  if (profile->priv->background_image)
    g_object_unref (G_OBJECT (profile->priv->background_image));

  pango_font_description_free (profile->priv->font);
  
  g_free (profile->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

TerminalProfile*
terminal_profile_new (const char *name,
                      GConfClient *conf)
{
  TerminalProfile *profile;
  GError *err;

  g_return_val_if_fail (profiles != NULL, NULL);
  g_return_val_if_fail (terminal_profile_lookup (name) == NULL,
                        NULL);
  
  profile = g_object_new (TERMINAL_TYPE_PROFILE, NULL);

  profile->priv->conf = conf;
  g_object_ref (G_OBJECT (conf));
  
  profile->priv->name = g_strdup (name);
  
  profile->priv->profile_dir = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX,
                                                         profile->priv->name);

  err = NULL;
  gconf_client_add_dir (conf, profile->priv->profile_dir,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &err);
  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  profile->priv->profile_dir, err->message);
      g_error_free (err);
    }
  
  err = NULL;
  profile->priv->notify_id =
    gconf_client_notify_add (conf,
                             profile->priv->profile_dir,
                             profile_change_notify,
                             profile,
                             NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal profile changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }
  
  g_hash_table_insert (profiles, profile->priv->name, profile);

  if (default_profile == NULL &&
      default_profile_id &&
      strcmp (default_profile_id, profile->priv->name) == 0)
    {
      /* We are the default profile */
      default_profile = profile;
    }
  
  return profile;
}

const char*
terminal_profile_get_name (TerminalProfile *profile)
{
  return profile->priv->name;
}

const char*
terminal_profile_get_visible_name (TerminalProfile *profile)
{
  return profile->priv->visible_name;
}

void
terminal_profile_set_visible_name (TerminalProfile *profile,
                                   const char      *name)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_VISIBLE_NAME);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           name,
                           NULL);

  g_free (key);
}

gboolean
terminal_profile_get_forgotten (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);
  return profile->priv->forgotten;
}

gboolean
terminal_profile_get_silent_bell (TerminalProfile  *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->silent_bell;
}

void
terminal_profile_set_silent_bell (TerminalProfile  *profile,
                                  gboolean          setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SILENT_BELL);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_cursor_blink (TerminalProfile  *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);
  
  return profile->priv->cursor_blink;
}

void
terminal_profile_set_cursor_blink (TerminalProfile *profile,
                                   gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_CURSOR_BLINK);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_scroll_on_keystroke (TerminalProfile  *profile)
{
    g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->scroll_on_keystroke;
}


void
terminal_profile_set_scroll_on_keystroke (TerminalProfile  *profile,
                                          gboolean          setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLL_ON_KEYSTROKE);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_scroll_on_output (TerminalProfile  *profile)
{
    g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->scroll_on_output;
}


void
terminal_profile_set_scroll_on_output (TerminalProfile  *profile,
                                          gboolean          setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLL_ON_OUTPUT);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

TerminalScrollbarPosition
terminal_profile_get_scrollbar_position (TerminalProfile  *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), 0);

  return profile->priv->scrollbar_position;
}

void
terminal_profile_set_scrollbar_position (TerminalProfile *profile,
                                         TerminalScrollbarPosition pos)
{
  char *key;
  const char *pos_string;
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLLBAR_POSITION);

  pos_string = gconf_enum_to_string (scrollbar_positions, pos);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           pos_string,
                           NULL);

  g_free (key);
}

int
terminal_profile_get_scrollback_lines (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), 500);
  
  return profile->priv->scrollback_lines;  
}

void
terminal_profile_set_scrollback_lines (TerminalProfile  *profile,
                                       int               lines)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLLBACK_LINES);
  
  gconf_client_set_int (profile->priv->conf,
                        key,
                        lines,
                        NULL);

  g_free (key);
}

void
terminal_profile_get_color_scheme (TerminalProfile  *profile,
                                   GdkColor         *foreground,
                                   GdkColor         *background)
{
  g_return_if_fail (TERMINAL_IS_PROFILE (profile));
  
  if (foreground)
    *foreground = profile->priv->foreground;
  if (background)
    *background = profile->priv->background;
}

static char*
color_to_string (const GdkColor *color)
{
  char *s;
  char *ptr;
  
  s = g_strdup_printf ("#%2X%2X%2X",
                       color->red / 256,
                       color->green / 256,
                       color->blue / 256);
  
  for (ptr = s; *ptr; ptr++)
    if (*ptr == ' ')
      *ptr = '0';

  return s;
}

void
terminal_profile_set_color_scheme (TerminalProfile  *profile,
                                   const GdkColor   *foreground,
                                   const GdkColor   *background)
{
  char *fg_key;
  char *bg_key;
  char *str;

  RETURN_IF_NOTIFYING (profile);
  
  fg_key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                     KEY_FOREGROUND_COLOR);

  bg_key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                     KEY_BACKGROUND_COLOR);

  str = color_to_string (foreground);
  
  gconf_client_set_string (profile->priv->conf,
                           fg_key, str, NULL);

  g_free (str);

  str = color_to_string (background);

  gconf_client_set_string (profile->priv->conf,
                           bg_key, str, NULL);

  g_free (str);
  
  g_free (fg_key);
  g_free (bg_key);
}

const char*
terminal_profile_get_word_chars (TerminalProfile  *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);
    
  return profile->priv->word_chars;
}

void
terminal_profile_set_word_chars (TerminalProfile *profile,
                                 const char      *word_chars)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_WORD_CHARS);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           word_chars,
                           NULL);

  g_free (key);
}

const char*
terminal_profile_get_title (TerminalProfile  *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);
  
  return profile->priv->title;
}

void
terminal_profile_set_title (TerminalProfile *profile,
                            const char      *title)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_TITLE);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           title,
                           NULL);

  g_free (key);
}

TerminalTitleMode
terminal_profile_get_title_mode (TerminalProfile  *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), 0);
  
  return profile->priv->title_mode;
}

void
terminal_profile_set_title_mode (TerminalProfile *profile,
                                 TerminalTitleMode mode)
{
  char *key;
  const char *mode_string;
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_TITLE_MODE);

  mode_string = gconf_enum_to_string (title_modes, mode);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           mode_string,
                           NULL);

  g_free (key);
}


gboolean
terminal_profile_get_allow_bold (TerminalProfile  *profile)
{
    g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->allow_bold;
}

void
terminal_profile_set_allow_bold (TerminalProfile  *profile,
                                          gboolean          setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_ALLOW_BOLD);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_default_show_menubar (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), TRUE);
  
  return profile->priv->default_show_menubar;
}

void
terminal_profile_set_default_show_menubar (TerminalProfile *profile,
                                           gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_DEFAULT_SHOW_MENUBAR);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}


TerminalExitAction
terminal_profile_get_exit_action (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), 0);

  return profile->priv->exit_action;
}

void
terminal_profile_set_exit_action (TerminalProfile   *profile,
                                  TerminalExitAction action)
{
  char *key;
  const char *action_string;
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_EXIT_ACTION);

  action_string = gconf_enum_to_string (exit_actions, action);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           action_string,
                           NULL);

  g_free (key);
}

gboolean
terminal_profile_get_login_shell (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->login_shell;
}

void
terminal_profile_set_login_shell (TerminalProfile *profile,
                                  gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_LOGIN_SHELL);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_update_records (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->update_records;
}

void
terminal_profile_set_update_records (TerminalProfile *profile,
                                     gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_UPDATE_RECORDS);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_use_custom_command (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->use_custom_command;
}

void
terminal_profile_set_use_custom_command (TerminalProfile *profile,
                                         gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_USE_CUSTOM_COMMAND);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

const char*
terminal_profile_get_custom_command (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);

  return profile->priv->custom_command;
}

void
terminal_profile_set_custom_command (TerminalProfile *profile,
                                     const char      *command)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_CUSTOM_COMMAND);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           command,
                           NULL);

  g_free (key);
}

const char*
terminal_profile_get_icon_file (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);
  
  return profile->priv->icon_file;
}

GdkPixbuf*
terminal_profile_get_icon (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);
  
  if (profile->priv->icon == NULL &&
      !profile->priv->icon_load_failed)
    {
      GdkPixbuf *pixbuf;
      GError *err;
      char *filename;

      filename = gnome_program_locate_file (gnome_program_get (),
                                            /* FIXME should I use APP_PIXMAP? */
                                            GNOME_FILE_DOMAIN_PIXMAP,
                                            profile->priv->icon_file,
                                            TRUE, NULL);

      if (filename == NULL)
        {
          g_printerr (_("Could not find an icon called \"%s\" for terminal profile \"%s\"\n"),
                      profile->priv->icon_file,
                      terminal_profile_get_visible_name (profile));

          profile->priv->icon_load_failed = TRUE;
          
          goto out;
        }
      
      err = NULL;
      pixbuf = gdk_pixbuf_new_from_file (filename, &err);

      if (pixbuf == NULL)
        {
          g_printerr (_("Failed to load icon \"%s\" for terminal profile \"%s\": %s\n"),
                      filename,
                      terminal_profile_get_visible_name (profile),
                      err->message);
          g_error_free (err);

          g_free (filename);

          profile->priv->icon_load_failed = TRUE;
          
          goto out;
        }

      profile->priv->icon = pixbuf;

      g_free (filename);
    }

 out:
  return profile->priv->icon;
}

void
terminal_profile_set_icon_file (TerminalProfile *profile,
                                const char      *filename)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_ICON);

  if (filename)
    {
      gconf_client_set_string (profile->priv->conf,
                               key,
                               filename,
                               NULL);
    }
  else
    {
      gconf_client_unset (profile->priv->conf, key, NULL);
    }

  g_free (key);
}

gboolean
terminal_profile_get_is_default (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);
  
  return profile == default_profile;
}

void
terminal_profile_set_is_default (TerminalProfile *profile,
                                 gboolean         setting)
{
  RETURN_IF_NOTIFYING (profile);
  
  gconf_client_set_string (profile->priv->conf,
                           CONF_GLOBAL_PREFIX"/default_profile",
                           terminal_profile_get_name (profile),
                           NULL);
}

void
terminal_profile_get_palette (TerminalProfile *profile,
                              GdkColor        *colors)
{
  g_return_if_fail (TERMINAL_IS_PROFILE (profile));
  
  memcpy (colors, profile->priv->palette,
          TERMINAL_PALETTE_SIZE * sizeof (GdkColor));
}

void
terminal_profile_set_palette (TerminalProfile *profile,
                              const GdkColor  *colors)
{
  char *key;
  char *str;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_PALETTE);

  str = terminal_palette_to_string (colors);
  
  gconf_client_set_string (profile->priv->conf,
                           key, str,
                           NULL);

  g_free (key);
  g_free (str);
}

const char*
terminal_profile_get_x_font (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);

  return profile->priv->x_font;
}

void
terminal_profile_set_x_font (TerminalProfile *profile,
                             const char      *name)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_X_FONT);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           name,
                           NULL);

  g_free (key);
}

TerminalBackgroundType
terminal_profile_get_background_type (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), TERMINAL_BACKGROUND_SOLID);

  return profile->priv->background_type;
}

void
terminal_profile_set_background_type (TerminalProfile        *profile,
                                      TerminalBackgroundType  type)
{
  char *key;
  const char *type_string;
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_BACKGROUND_TYPE);

  type_string = gconf_enum_to_string (background_types, type);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           type_string,
                           NULL);

  g_free (key);
}

GdkPixbuf*
terminal_profile_get_background_image (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);
  
  if (profile->priv->background_image == NULL &&
      !profile->priv->background_load_failed)
    {
      GdkPixbuf *pixbuf;
      GError *err;
      char *filename;

      filename = gnome_program_locate_file (gnome_program_get (),
                                            /* FIXME should I use APP_PIXMAP? */
                                            GNOME_FILE_DOMAIN_PIXMAP,
                                            profile->priv->background_image_file,
                                            TRUE, NULL);

      if (filename == NULL)
        {
          g_printerr (_("Could not find a background image called \"%s\" for terminal profile \"%s\"\n"),
                      profile->priv->background_image_file,
                      terminal_profile_get_visible_name (profile));

          profile->priv->background_load_failed = TRUE;
          
          goto out;
        }
      
      err = NULL;
      pixbuf = gdk_pixbuf_new_from_file (filename, &err);

      if (pixbuf == NULL)
        {
          g_printerr (_("Failed to load background image \"%s\" for terminal profile \"%s\": %s\n"),
                      filename,
                      terminal_profile_get_visible_name (profile),
                      err->message);
          g_error_free (err);

          g_free (filename);

          profile->priv->background_load_failed = TRUE;
          
          goto out;
        }

      profile->priv->background_image = pixbuf;
    }

 out:
  return profile->priv->background_image;
}

const char*
terminal_profile_get_background_image_file (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);

  return profile->priv->background_image_file;
}


void
terminal_profile_set_background_image_file (TerminalProfile *profile,
                                            const char      *filename)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_BACKGROUND_IMAGE);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           filename,
                           NULL);

  g_free (key);
}

gboolean
terminal_profile_get_scroll_background (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->scroll_background;
}

void
terminal_profile_set_scroll_background (TerminalProfile *profile,
                                        gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLL_BACKGROUND);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

double
terminal_profile_get_background_darkness (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), 0.0);

  return profile->priv->background_darkness;
}

void
terminal_profile_set_background_darkness (TerminalProfile *profile,
                                          double           setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_BACKGROUND_DARKNESS);
  
  gconf_client_set_float (profile->priv->conf,
                          key,
                          setting,
                          NULL);

  g_free (key);
}

TerminalEraseBinding
terminal_profile_get_backspace_binding (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), TERMINAL_ERASE_ASCII_DEL);

  return profile->priv->backspace_binding;
}

void
terminal_profile_set_backspace_binding (TerminalProfile        *profile,
                                        TerminalEraseBinding    binding)
{
  char *key;
  const char *binding_string;
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_BACKSPACE_BINDING);

  binding_string = gconf_enum_to_string (erase_bindings, binding);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           binding_string,
                           NULL);

  g_free (key);
}

TerminalEraseBinding
terminal_profile_get_delete_binding (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), TERMINAL_ERASE_ESCAPE_SEQUENCE);

  return profile->priv->delete_binding;
}

void
terminal_profile_set_delete_binding (TerminalProfile      *profile,
                                     TerminalEraseBinding  binding)
{
  char *key;
  const char *binding_string;
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_DELETE_BINDING);

  binding_string = gconf_enum_to_string (erase_bindings, binding);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           binding_string,
                           NULL);

  g_free (key);
}

void
terminal_profile_set_palette_entry (TerminalProfile *profile,
                                    int              i,
                                    const GdkColor  *color)
{
  GdkColor colors[TERMINAL_PALETTE_SIZE];

  g_return_if_fail (TERMINAL_IS_PROFILE (profile));
  g_return_if_fail (i < TERMINAL_PALETTE_SIZE);
  g_return_if_fail (color != NULL);
  
  memcpy (colors, profile->priv->palette,
          TERMINAL_PALETTE_SIZE * sizeof (GdkColor));

  colors[i] = *color;

  terminal_profile_set_palette (profile, colors);
}

gboolean
terminal_profile_get_use_theme_colors (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->use_theme_colors;
}

void
terminal_profile_set_use_theme_colors (TerminalProfile *profile,
                                       gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_USE_THEME_COLORS);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_use_system_font (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->use_system_font;
}

void
terminal_profile_set_use_system_font (TerminalProfile *profile,
                                      gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_USE_SYSTEM_FONT);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_use_skey (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->use_skey;
}

void
terminal_profile_set_use_skey (TerminalProfile *profile,
			       gboolean         setting)
{
  char *key;

  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_USE_SKEY);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}


const PangoFontDescription*
terminal_profile_get_font (TerminalProfile *profile)
{
  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);

  return profile->priv->font;
}

void
terminal_profile_set_font (TerminalProfile            *profile,
                           const PangoFontDescription *font_desc)
{
  char *key;
  char *str;

  g_return_if_fail (font_desc != NULL);
  
  RETURN_IF_NOTIFYING (profile);
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_FONT);

  str = pango_font_description_to_string (font_desc);
  g_return_if_fail (str);
  
  gconf_client_set_string (profile->priv->conf,
                           key,
                           str,
                           NULL);

  g_free (str);
  g_free (key);
}

static gboolean
set_visible_name (TerminalProfile *profile,
                  const char      *candidate_name)
{
  if (candidate_name &&
      strcmp (profile->priv->visible_name, candidate_name) == 0)
    return FALSE;
  
  if (candidate_name != NULL)
    {
      g_free (profile->priv->visible_name);
      profile->priv->visible_name = g_strdup (candidate_name);
      return TRUE;
    }
  /* otherwise just leave the old name */

  return FALSE;
}

static gboolean
set_foreground_color (TerminalProfile *profile,
                      const char      *str_val)
{
  GdkColor color;
  
  if (str_val && gdk_color_parse (str_val, &color) &&
      !gdk_color_equal (&color, &profile->priv->foreground))
    {
      profile->priv->foreground = color;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_background_color (TerminalProfile *profile,
                      const char      *str_val)
{
  GdkColor color;
  
  if (str_val && gdk_color_parse (str_val, &color) &&
      !gdk_color_equal (&color, &profile->priv->background))
    {
      profile->priv->background = color;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_title (TerminalProfile *profile,
           const char      *candidate_name)
{
  if (candidate_name &&
      strcmp (profile->priv->title, candidate_name) == 0)
    return FALSE;
  
  if (candidate_name != NULL)
    {
      g_free (profile->priv->title);
      profile->priv->title = g_strdup (candidate_name);
      return TRUE;
    }
  /* otherwise just leave the old name */

  return FALSE;
}

static gboolean
set_title_mode (TerminalProfile *profile,
                const char      *str_val)
{
  int mode; /* TerminalTitleMode */
  
  if (str_val &&
      gconf_string_to_enum (title_modes, str_val, &mode) &&
      mode != profile->priv->title_mode)
    {
      profile->priv->title_mode = mode;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_word_chars (TerminalProfile *profile,
                const char      *candidate_chars)
{
  if (candidate_chars &&
      strcmp (profile->priv->word_chars, candidate_chars) == 0)
    return FALSE;
  
  if (candidate_chars != NULL)
    {
      g_free (profile->priv->word_chars);
      profile->priv->word_chars = g_strdup (candidate_chars);
      return TRUE;
    }
  /* otherwise just leave the old chars */
  
  return FALSE;
}

static gboolean
set_scrollbar_position (TerminalProfile *profile,
                        const char      *str_val)
{
  int pos; /* TerminalScrollbarPosition */
  
  if (str_val &&
      gconf_string_to_enum (scrollbar_positions, str_val, &pos) &&
      pos != profile->priv->scrollbar_position)
    {
      profile->priv->scrollbar_position = pos;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_scrollback_lines (TerminalProfile *profile,
                      int              int_val)
{
  if (int_val >= 1 &&
      int_val != profile->priv->scrollback_lines)
    {
      profile->priv->scrollback_lines = int_val;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_exit_action (TerminalProfile *profile,
                 const char      *str_val)
{
  int action; /* TerminalExitAction */
  
  if (str_val &&
      gconf_string_to_enum (exit_actions, str_val, &action) &&
      action != profile->priv->exit_action)
    {
      profile->priv->exit_action = action;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_custom_command (TerminalProfile *profile,
                    const char      *candidate_command)
{
  if (candidate_command &&
      strcmp (profile->priv->custom_command, candidate_command) == 0)
    return FALSE;
  
  if (candidate_command != NULL)
    {
      g_free (profile->priv->custom_command);
      profile->priv->custom_command = g_strdup (candidate_command);
      return TRUE;
    }
  /* otherwise just leave the old command */
  
  return FALSE;
}

static gboolean
set_icon_file (TerminalProfile *profile,
               const char      *candidate_file)
{
  if (candidate_file &&
      strcmp (profile->priv->icon_file, candidate_file) == 0)
    return FALSE;
  
  if (candidate_file != NULL)
    {
      
      g_free (profile->priv->icon_file);
      profile->priv->icon_file = g_strdup (candidate_file);

      if (profile->priv->icon != NULL)
        {
          g_object_unref (G_OBJECT (profile->priv->icon));
          profile->priv->icon = NULL;
        }

      profile->priv->icon_load_failed = FALSE; /* try again */

      return TRUE;
    }
  /* otherwise just leave the old filename */
  
  return FALSE;
}

static gboolean
set_palette (TerminalProfile *profile,
             const char      *candidate_str)
{  
  if (candidate_str != NULL)
    {
      GdkColor new_palette[TERMINAL_PALETTE_SIZE];

      if (!terminal_palette_from_string (candidate_str,
                                         new_palette,
                                         TRUE))
        return FALSE;

      if (memcmp (profile->priv->palette, new_palette,
                  TERMINAL_PALETTE_SIZE * sizeof (GdkColor)) == 0)
        return FALSE;

      memcpy (profile->priv->palette, new_palette,
              TERMINAL_PALETTE_SIZE * sizeof (GdkColor));

      return TRUE;
    }
  
  return FALSE;
}

static gboolean
set_x_font (TerminalProfile *profile,
            const char      *candidate_font)
{
  if (candidate_font &&
      strcmp (profile->priv->x_font, candidate_font) == 0)
    return FALSE;
  
  if (candidate_font != NULL)
    {
      g_free (profile->priv->x_font);
      profile->priv->x_font = g_strdup (candidate_font);
      return TRUE;
    }
  /* otherwise just leave the old font */
  
  return FALSE;
}

static gboolean
set_background_type (TerminalProfile *profile,
                     const char      *str_val)
{
  int type; /* TerminalBackgroundType */
  
  if (str_val &&
      gconf_string_to_enum (background_types, str_val, &type) &&
      type != profile->priv->background_type)
    {
      profile->priv->background_type = type;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_background_image_file (TerminalProfile *profile,
                           const char      *candidate_file)
{
  if (candidate_file &&
      strcmp (profile->priv->background_image_file, candidate_file) == 0)
    return FALSE;
  
  if (candidate_file != NULL)
    {
      g_free (profile->priv->background_image_file);
      profile->priv->background_image_file = g_strdup (candidate_file);

      if (profile->priv->background_image != NULL)
        {
          g_object_unref (G_OBJECT (profile->priv->background_image));
          profile->priv->background_image = NULL;
        }

      profile->priv->background_load_failed = FALSE; /* try again */

      return TRUE;
    }
  /* otherwise just leave the old filename */
  
  return FALSE;
}

static gboolean
set_backspace_binding (TerminalProfile *profile,
                       const char      *str_val)
{
  int binding; /* TerminalEraseBinding */
  
  if (str_val &&
      gconf_string_to_enum (erase_bindings, str_val, &binding) &&
      binding != profile->priv->backspace_binding)
    {
      profile->priv->backspace_binding = binding;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_delete_binding (TerminalProfile *profile,
                    const char      *str_val)
{
  int binding; /* TerminalEraseBinding */
  
  if (str_val &&
      gconf_string_to_enum (erase_bindings, str_val, &binding) &&
      binding != profile->priv->delete_binding)
    {
      profile->priv->delete_binding = binding;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
set_font (TerminalProfile *profile,
          const char      *candidate_font_name)
{
  PangoFontDescription *desc;
  PangoFontDescription *tmp;
  
  if (candidate_font_name == NULL)
    return FALSE; /* leave old font */

  desc = pango_font_description_from_string (candidate_font_name);
  if (desc == NULL)
    {
      g_printerr (_("GNOME Terminal: font name \"%s\" set in configuration database is not valid\n"),
                  candidate_font_name);
      return FALSE; /* leave the old font */
    }

  /* Merge in case the new string isn't complete enough to
   * load a font
   */
  tmp = pango_font_description_copy (profile->priv->font);
  pango_font_description_merge (tmp, desc, TRUE);
  pango_font_description_free (desc);
  desc = tmp;
  
  if (pango_font_description_equal (profile->priv->font, desc))
    {
      pango_font_description_free (desc);
      return FALSE;
    }
  else
    {
      pango_font_description_free (profile->priv->font);
      profile->priv->font = desc;
      return TRUE;
    }
}

void
terminal_profile_update (TerminalProfile *profile)
{
  TerminalSettingMask locked;
  TerminalSettingMask old_locked;
  TerminalSettingMask mask;
  
  terminal_setting_mask_clear (&mask);
  terminal_setting_mask_clear (&locked);

#define UPDATE_BOOLEAN(KName, FName)                                            \
  {                                                                             \
    char *key = gconf_concat_dir_and_key (profile->priv->profile_dir, KName);   \
    gboolean val = gconf_client_get_bool (profile->priv->conf, key, NULL);      \
                                                                                \
    if (val != profile->priv->FName)                                            \
      {                                                                         \
        mask.FName = TRUE;                                                      \
        profile->priv->FName = val;                                             \
      }                                                                         \
                                                                                \
    locked.FName =                                                              \
      !gconf_client_key_is_writable (profile->priv->conf, key, NULL);           \
                                                                                \
    g_free (key);                                                               \
  }

#define UPDATE_INTEGER(KName, FName)                                            \
  {                                                                             \
    char *key = gconf_concat_dir_and_key (profile->priv->profile_dir, KName);   \
    int val = gconf_client_get_int (profile->priv->conf, key, NULL);            \
                                                                                \
    mask.FName = set_##FName (profile, val);                                    \
                                                                                \
    locked.FName =                                                              \
      !gconf_client_key_is_writable (profile->priv->conf, key, NULL);           \
                                                                                \
    g_free (key);                                                               \
  }

#define UPDATE_FLOAT(KName, FName)                                              \
  {                                                                             \
    char *key = gconf_concat_dir_and_key (profile->priv->profile_dir, KName);   \
    double val = gconf_client_get_float (profile->priv->conf, key, NULL);       \
                                                                                \
    if (val != profile->priv->FName)                                            \
      {                                                                         \
        mask.FName = TRUE;                                                      \
        profile->priv->FName = val;                                             \
      }                                                                         \
                                                                                \
    g_free (key);                                                               \
  }

#define UPDATE_STRING(KName, FName)                                             \
  {                                                                             \
    char *key = gconf_concat_dir_and_key (profile->priv->profile_dir, KName);   \
    char *val = gconf_client_get_string (profile->priv->conf, key, NULL);       \
                                                                                \
    mask.FName = set_##FName (profile, val);                                    \
                                                                                \
    locked.FName =                                                              \
      !gconf_client_key_is_writable (profile->priv->conf, key, NULL);           \
                                                                                \
    g_free (val);                                                               \
    g_free (key);                                                               \
  }

  UPDATE_BOOLEAN (KEY_CURSOR_BLINK,         cursor_blink);
  UPDATE_BOOLEAN (KEY_DEFAULT_SHOW_MENUBAR, default_show_menubar);
  UPDATE_STRING  (KEY_VISIBLE_NAME,         visible_name);
  UPDATE_STRING  (KEY_FOREGROUND_COLOR,     foreground_color);
  UPDATE_STRING  (KEY_BACKGROUND_COLOR,     background_color);
  UPDATE_STRING  (KEY_TITLE,                title);
  UPDATE_STRING  (KEY_TITLE_MODE,           title_mode);
  UPDATE_BOOLEAN (KEY_ALLOW_BOLD,           allow_bold);
  UPDATE_BOOLEAN (KEY_SILENT_BELL,          silent_bell);
  UPDATE_STRING  (KEY_WORD_CHARS,           word_chars);
  UPDATE_STRING  (KEY_SCROLLBAR_POSITION,   scrollbar_position);
  UPDATE_INTEGER (KEY_SCROLLBACK_LINES,     scrollback_lines);
  UPDATE_BOOLEAN (KEY_SCROLL_ON_KEYSTROKE,  scroll_on_keystroke);
  UPDATE_BOOLEAN (KEY_SCROLL_ON_OUTPUT,     scroll_on_output);
  UPDATE_STRING  (KEY_EXIT_ACTION,          exit_action);
  UPDATE_BOOLEAN (KEY_LOGIN_SHELL,          login_shell);
  UPDATE_BOOLEAN (KEY_UPDATE_RECORDS,       update_records);
  UPDATE_BOOLEAN (KEY_USE_CUSTOM_COMMAND,   use_custom_command);
  UPDATE_STRING  (KEY_CUSTOM_COMMAND,       custom_command);
  UPDATE_STRING  (KEY_ICON,                 icon_file);
  UPDATE_STRING  (KEY_PALETTE,              palette);
  UPDATE_STRING  (KEY_X_FONT,               x_font);
  UPDATE_STRING  (KEY_BACKGROUND_TYPE,      background_type);
  UPDATE_STRING  (KEY_BACKGROUND_IMAGE,     background_image_file);
  UPDATE_BOOLEAN (KEY_SCROLL_BACKGROUND,    scroll_background);
  UPDATE_FLOAT   (KEY_BACKGROUND_DARKNESS,  background_darkness);
  UPDATE_STRING  (KEY_BACKSPACE_BINDING,    backspace_binding);
  UPDATE_STRING  (KEY_DELETE_BINDING,       delete_binding);
  UPDATE_BOOLEAN (KEY_USE_THEME_COLORS,     use_theme_colors);
  UPDATE_BOOLEAN (KEY_USE_SYSTEM_FONT,      use_system_font);
  UPDATE_STRING  (KEY_FONT,                 font);
  
#undef UPDATE_BOOLEAN
#undef UPDATE_INTEGER
#undef UPDATE_FLOAT
#undef UPDATE_STRING

  /* Update state and emit signals */
  
  old_locked = profile->priv->locked;
  profile->priv->locked = locked;
  
  if (!(terminal_setting_mask_is_empty (&mask) &&
        terminal_setting_mask_equal (&locked, &old_locked)))
    emit_changed (profile, &mask);
}


static const gchar*
find_key (const gchar* key)
{
  const gchar* end;
  
  end = strrchr (key, '/');

  ++end;

  return end;
}

static void
profile_change_notify (GConfClient *client,
                       guint        cnxn_id,
                       GConfEntry  *entry,
                       gpointer     user_data)
{
  TerminalProfile *profile;
  const char *key;
  GConfValue *val;
  TerminalSettingMask old_locked;
  TerminalSettingMask mask;
  
  profile = TERMINAL_PROFILE (user_data);

  terminal_setting_mask_clear (&mask);
  old_locked = profile->priv->locked;

  val = gconf_entry_get_value (entry);
  
  key = find_key (gconf_entry_get_key (entry));
  
#define UPDATE_BOOLEAN(KName, FName, Preset)                            \
  }                                                                     \
else if (strcmp (key, KName) == 0)                                      \
  {                                                                     \
    gboolean setting = (Preset);                                        \
                                                                        \
    if (val && val->type == GCONF_VALUE_BOOL)                           \
      setting = gconf_value_get_bool (val);                             \
                                                                        \
    if (setting != profile->priv->FName)                                \
      {                                                                 \
        mask.FName = TRUE;                                              \
        profile->priv->FName = setting;                                 \
      }                                                                 \
                                                                        \
    profile->priv->locked.FName = !gconf_entry_get_is_writable (entry);

#define UPDATE_INTEGER(KName, FName, Preset)                            \
  }                                                                     \
else if (strcmp (key, KName) == 0)                                      \
  {                                                                     \
    int setting = (Preset);                                             \
                                                                        \
    if (val && val->type == GCONF_VALUE_INT)                            \
      setting = gconf_value_get_int (val);                              \
                                                                        \
    mask.FName = set_##FName (profile, setting);                        \
                                                                        \
    profile->priv->locked.FName = !gconf_entry_get_is_writable (entry);

#define UPDATE_FLOAT(KName, FName, Preset)                              \
  }                                                                     \
else if (strcmp (key, KName) == 0)                                      \
  {                                                                     \
    float setting = (Preset);                                           \
                                                                        \
    if (val && val->type == GCONF_VALUE_FLOAT)                          \
      setting = gconf_value_get_float (val);                            \
                                                                        \
    if (setting != profile->priv->FName)                                \
      {                                                                 \
        mask.FName = TRUE;                                              \
        profile->priv->FName = setting;                                 \
      }                                                                 \
                                                                        \
    profile->priv->locked.FName = !gconf_entry_get_is_writable (entry);


#define UPDATE_STRING(KName, FName, Preset)                             \
  }                                                                     \
else if (strcmp (key, KName) == 0)                                      \
  {                                                                     \
    const char * setting = (Preset);                                    \
                                                                        \
    if (val && val->type == GCONF_VALUE_STRING)                         \
      setting = gconf_value_get_string (val);                           \
                                                                        \
    mask.FName = set_##FName (profile, setting);                        \
                                                                        \
    profile->priv->locked.FName = !gconf_entry_get_is_writable (entry);


 if (0)
   {
     ;
     UPDATE_BOOLEAN (KEY_CURSOR_BLINK,           cursor_blink,           FALSE);
     UPDATE_BOOLEAN (KEY_DEFAULT_SHOW_MENUBAR,   default_show_menubar,   FALSE);
     UPDATE_STRING  (KEY_VISIBLE_NAME,           visible_name,           NULL);
     UPDATE_STRING  (KEY_FOREGROUND_COLOR,       foreground_color,       NULL);
     UPDATE_STRING  (KEY_BACKGROUND_COLOR,       background_color,       NULL);
     UPDATE_STRING  (KEY_TITLE,                  title,                  NULL);
     UPDATE_STRING  (KEY_TITLE_MODE,             title_mode,             NULL);
     UPDATE_BOOLEAN (KEY_ALLOW_BOLD,             allow_bold,             FALSE);
     UPDATE_BOOLEAN (KEY_SILENT_BELL,            silent_bell,            FALSE);
     UPDATE_STRING  (KEY_WORD_CHARS,             word_chars,             NULL);
     UPDATE_STRING  (KEY_SCROLLBAR_POSITION,     scrollbar_position,     NULL);
     UPDATE_INTEGER (KEY_SCROLLBACK_LINES,       scrollback_lines,       profile->priv->scrollback_lines);
     UPDATE_BOOLEAN (KEY_SCROLL_ON_KEYSTROKE, 	 scroll_on_keystroke,    FALSE);
     UPDATE_BOOLEAN (KEY_SCROLL_ON_OUTPUT,       scroll_on_output,       FALSE);
     UPDATE_STRING  (KEY_EXIT_ACTION,            exit_action,            NULL);
     UPDATE_BOOLEAN (KEY_LOGIN_SHELL,            login_shell,            FALSE);
     UPDATE_BOOLEAN (KEY_UPDATE_RECORDS,         update_records,         FALSE);
     UPDATE_BOOLEAN (KEY_USE_CUSTOM_COMMAND,     use_custom_command,     FALSE);
     UPDATE_STRING  (KEY_CUSTOM_COMMAND,         custom_command,         NULL);
     UPDATE_STRING  (KEY_ICON,                   icon_file,              NULL);
     UPDATE_STRING  (KEY_PALETTE,                palette,                NULL);
     UPDATE_STRING  (KEY_X_FONT,                 x_font,                 NULL);
     UPDATE_STRING  (KEY_BACKGROUND_TYPE,        background_type,        NULL);
     UPDATE_STRING  (KEY_BACKGROUND_IMAGE,       background_image_file,  NULL);
     UPDATE_BOOLEAN (KEY_SCROLL_BACKGROUND,      scroll_background,      FALSE);
     UPDATE_FLOAT   (KEY_BACKGROUND_DARKNESS,    background_darkness,    0.5);
     UPDATE_STRING  (KEY_BACKSPACE_BINDING,      backspace_binding,      NULL);
     UPDATE_STRING  (KEY_DELETE_BINDING,         delete_binding,         NULL);
     UPDATE_BOOLEAN (KEY_USE_THEME_COLORS,       use_theme_colors,       TRUE);
     UPDATE_BOOLEAN (KEY_USE_SYSTEM_FONT,        use_system_font,        TRUE);
     UPDATE_STRING  (KEY_FONT,                   font,                   NULL);
   }
  
#undef UPDATE_BOOLEAN
#undef UPDATE_INTEGER
#undef UPDATE_FLOAT
#undef UPDATE_STRING

  if (!(terminal_setting_mask_is_empty (&mask) &&
        terminal_setting_mask_equal (&old_locked, &profile->priv->locked)))
    emit_changed (profile, &mask);
}

static void
set_key_to_default (TerminalProfile *profile,
                    const char      *relative_key)
{
  char *key;
  char *default_key;
  GError *err;
  GConfValue *val;

  default_key = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX"/"FALLBACK_PROFILE_ID,
                                          relative_key);

  err = NULL;
  val = gconf_client_get_default_from_schema (profile->priv->conf,
                                              default_key,
                                              &err);
  if (err != NULL)
    {
      g_printerr (_("Error getting default value of %s: %s\n"),
                  default_key, err->message);
      g_error_free (err);
    }

  if (val == NULL)
    g_printerr (_("There wasn't a default value for %s\n"),
                default_key);
  
  g_free (default_key);
  
  if (val)
    {
      key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                      relative_key);
      
      err = NULL;      
      gconf_client_set (profile->priv->conf,
                        key, val, &err);
      if (err != NULL)
        {
          g_printerr (_("Error setting key %s back to default: %s\n"),
                      key, err->message);
          g_error_free (err);
        }

      gconf_value_free (val);
      g_free (key);
    }
}

void
terminal_profile_reset_compat_defaults (TerminalProfile *profile)
{
  set_key_to_default (profile, KEY_DELETE_BINDING);
  set_key_to_default (profile, KEY_BACKSPACE_BINDING);
}

static void
update_default_profile (const char *name,
                        gboolean    locked)
{
  TerminalProfile *profile;
  gboolean changed;
  TerminalProfile *old_default;
  
  changed = FALSE;
  
  g_free (default_profile_id);
  default_profile_id = g_strdup (name);

  old_default = default_profile;
  profile = terminal_profile_lookup (name);
  
  if (profile)
    {
      if (profile != default_profile)
        {
          default_profile = profile;
          changed = TRUE;
        }
    }

  if (locked != default_profile_locked)
    {
      /* Need to emit changed on all profiles */
      GList *all_profiles;
      GList *tmp;
      TerminalSettingMask mask;

      terminal_setting_mask_clear (&mask);
      mask.is_default = TRUE;
      
      default_profile_locked = locked;
      
      all_profiles = terminal_profile_get_list ();
      tmp = all_profiles;
      while (tmp != NULL)
        {
          TerminalProfile *p = tmp->data;
          
          emit_changed (p, &mask);
          tmp = tmp->next;
        }

      g_list_free (all_profiles);
    }
  else if (changed)
    {
      TerminalSettingMask mask;
      
      terminal_setting_mask_clear (&mask);
      mask.is_default = TRUE;

      if (old_default)
        emit_changed (old_default, &mask);

      emit_changed (profile, &mask);
    }
}

static void
default_change_notify (GConfClient *client,
                       guint        cnxn_id,
                       GConfEntry  *entry,
                       gpointer     user_data)
{
  GConfValue *val;
  gboolean locked;
  const char *name;
  
  val = gconf_entry_get_value (entry);  

  if (val == NULL || val->type != GCONF_VALUE_STRING)
    return;
  
  if (gconf_entry_get_is_writable (entry))  
    locked = FALSE;
  else
    locked = TRUE;
  
  name = gconf_value_get_string (val);

  update_default_profile (name, locked);
}

static void
listify_foreach (gpointer key,
                 gpointer value,
                 gpointer data)
{
  GList **listp = data;

  *listp = g_list_prepend (*listp, value);
}

static int
alphabetic_cmp (gconstpointer a,
                gconstpointer b)
{
  TerminalProfile *ap = (TerminalProfile*) a;
  TerminalProfile *bp = (TerminalProfile*) b;

  return g_utf8_collate (terminal_profile_get_visible_name (ap),
                         terminal_profile_get_visible_name (bp));
}

GList*
terminal_profile_get_list (void)
{
  GList *list;

  list = NULL;
  g_hash_table_foreach (profiles, listify_foreach, &list);

  list = g_list_sort (list, alphabetic_cmp);
  
  return list;
}

int
terminal_profile_get_count (void)
{
  return g_hash_table_size (profiles);
}

TerminalProfile*
terminal_profile_lookup (const char *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  if (profiles)
    return g_hash_table_lookup (profiles, name);
  else
    return NULL;
}

typedef struct
{
  TerminalProfile *result;
  const char *target;
} LookupInfo;

static void
lookup_by_visible_name_foreach (gpointer key,
                                gpointer value,
                                gpointer data)
{
  LookupInfo *info = data;

  if (strcmp (info->target, terminal_profile_get_visible_name (value)) == 0)
    info->result = value;
}

TerminalProfile*
terminal_profile_lookup_by_visible_name (const char *name)
{
  LookupInfo info;

  info.result = NULL;
  info.target = name;

  if (profiles)
    {
      g_hash_table_foreach (profiles, lookup_by_visible_name_foreach, &info);
      return info.result;
    }
  else
    return NULL;
}

void
terminal_profile_forget (TerminalProfile *profile)
{
  if (!profile->priv->forgotten)
    {
      GError *err;
      
      err = NULL;
      gconf_client_remove_dir (profile->priv->conf,
                               profile->priv->profile_dir,
                               &err);
      if (err)
        {
          g_printerr (_("There was an error while removing the configuration directory %s. (%s)\n"),
                      profile->priv->profile_dir, err->message);
          g_error_free (err);
        }

      g_hash_table_remove (profiles, profile->priv->name);
      profile->priv->forgotten = TRUE;

      if (profile == default_profile)          
        default_profile = NULL;
      
      g_signal_emit (G_OBJECT (profile), signals[FORGOTTEN], 0);
    }
}

const TerminalSettingMask*
terminal_profile_get_locked_settings (TerminalProfile *profile)
{
  return &profile->priv->locked;
}

TerminalProfile*
terminal_profile_ensure_fallback (GConfClient *conf)
{
  TerminalProfile *profile;

  profile = terminal_profile_lookup (FALLBACK_PROFILE_ID);

  if (profile == NULL)
    {
      profile = terminal_profile_new (FALLBACK_PROFILE_ID, conf);  
      
      terminal_profile_update (profile);
    }
  
  return profile;
}

void
terminal_profile_initialize (GConfClient *conf)
{
  GError *err;
  char *str;

  g_return_if_fail (profiles == NULL);
  
  profiles = g_hash_table_new (g_str_hash, g_str_equal);
  
  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/default_profile",
                           default_change_notify,
                           NULL,
                           NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of changes to default profile. (%s)\n"),
                  err->message);
      g_error_free (err);
    }

  str = gconf_client_get_string (conf,
                                 CONF_GLOBAL_PREFIX"/default_profile",
                                 NULL);
  if (str)
    {
      update_default_profile (str,
                              !gconf_client_key_is_writable (conf,
                                                             CONF_GLOBAL_PREFIX"/default_profile",
                                                             NULL));
      g_free (str);
    }
}

static void
emit_changed (TerminalProfile           *profile,
              const TerminalSettingMask *mask)
{
  profile->priv->in_notification_count += 1;
  g_signal_emit (G_OBJECT (profile), signals[CHANGED], 0, mask);  
  profile->priv->in_notification_count -= 1;
}

/* Function I'm cut-and-pasting everywhere, this is from msm */
void
dialog_add_details (GtkDialog  *dialog,
                    const char *details)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkRequisition req;
  
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
  
  gtk_box_pack_start (GTK_BOX (dialog->vbox),
                      hbox,
                      FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic (_("_Details"));
  
  gtk_box_pack_end (GTK_BOX (hbox), button,
                    FALSE, FALSE, 0);
  
  label = gtk_label_new (details);

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  
  gtk_box_pack_start (GTK_BOX (hbox), label,
                      TRUE, TRUE, 0);

  /* show the label on click */
  g_signal_connect_swapped (G_OBJECT (button),
                            "clicked",
                            G_CALLBACK (gtk_widget_show),
                            label);
  
  /* second callback destroys the button (note disconnects first callback) */
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  /* Set default dialog size to size with the label,
   * and without the button, but then rehide the label
   */
  gtk_widget_show_all (hbox);

  gtk_widget_size_request (GTK_WIDGET (dialog), &req);

  gtk_window_set_default_size (GTK_WINDOW (dialog), req.width, req.height);
  
  gtk_widget_hide (label);
}

void
terminal_profile_create (TerminalProfile *base_profile,
                         const char      *visible_name,
                         GtkWindow       *transient_parent)
{
  char *profile_name = NULL;
  char *profile_dir = NULL;
  int i;
  char *s;
  const char *cs;
  char *key = NULL;
  GError *err = NULL;
  GList *profiles = NULL;
  GSList *name_list = NULL;
  GList *tmp;  

  /* This is for extra bonus paranoia against CORBA reentrancy */
  g_object_ref (G_OBJECT (base_profile));
  g_object_ref (G_OBJECT (transient_parent));

#define BAIL_OUT_CHECK() do {                           \
    if (!GTK_WIDGET_VISIBLE (transient_parent) ||       \
        base_profile->priv->forgotten ||                \
        err != NULL)                                    \
       goto cleanup;                                    \
  } while (0) 
  
  /* Pick a unique name for storing in gconf (based on visible name) */
  profile_name = gconf_escape_key (visible_name, -1);

  s = g_strdup (profile_name);
  i = 0;
  while (terminal_profile_lookup (s))
    {
      g_free (s);
      
      s = g_strdup_printf ("%s-%d", profile_name, i);

      ++i;
    }

  g_free (profile_name);
  profile_name = s;

  profile_dir = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX, 
                                          profile_name);
  
  /* Store a copy of base profile values at under that directory */

  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_VISIBLE_NAME);

  gconf_client_set_string (base_profile->priv->conf,
                           key,
                           visible_name,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_CURSOR_BLINK);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->cursor_blink,
                         &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_DEFAULT_SHOW_MENUBAR);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->default_show_menubar,
                         &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_FOREGROUND_COLOR);
  s = color_to_string (&base_profile->priv->foreground);
  gconf_client_set_string (base_profile->priv->conf,
                           key, s,                           
                           &err);
  g_free (s);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_BACKGROUND_COLOR);
  s = color_to_string (&base_profile->priv->background);
  gconf_client_set_string (base_profile->priv->conf,
                           key, s,                           
                           &err);
  g_free (s);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_TITLE);
  /* default title is profile name, not copied from base */
  gconf_client_set_string (base_profile->priv->conf,
                           key, visible_name,
                           &err);
  BAIL_OUT_CHECK ();
  
  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_TITLE_MODE);
  cs = gconf_enum_to_string (title_modes, base_profile->priv->title_mode);
  gconf_client_set_string (base_profile->priv->conf,
                           key, cs,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_ALLOW_BOLD);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->allow_bold,
                         &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_SILENT_BELL);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->silent_bell,
                         &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_WORD_CHARS);
  /* default title is profile name, not copied from base */
  gconf_client_set_string (base_profile->priv->conf,
                           key, base_profile->priv->word_chars,
                           &err);
  BAIL_OUT_CHECK ();
  
  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_SCROLLBAR_POSITION);
  cs = gconf_enum_to_string (scrollbar_positions,
                             base_profile->priv->scrollbar_position);
  gconf_client_set_string (base_profile->priv->conf,
                           key, cs,
                           &err);
  BAIL_OUT_CHECK ();
  
  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_SCROLLBACK_LINES);
  gconf_client_set_int (base_profile->priv->conf,
                        key, base_profile->priv->scrollback_lines,
                        &err);
  BAIL_OUT_CHECK ();

  
  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_SCROLL_ON_KEYSTROKE);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->scroll_on_keystroke,
                         &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_SCROLL_ON_OUTPUT);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->scroll_on_output,
                         &err);

  BAIL_OUT_CHECK ();


  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_EXIT_ACTION);

  cs = gconf_enum_to_string (scrollbar_positions,
                             base_profile->priv->exit_action);
  
  gconf_client_set_string (base_profile->priv->conf,
                           key, cs,
                           &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_LOGIN_SHELL);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->login_shell,
                         &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_UPDATE_RECORDS);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->update_records,
                         &err);

  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_USE_CUSTOM_COMMAND);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->use_custom_command,
                         &err);

  BAIL_OUT_CHECK ();

  
  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_CUSTOM_COMMAND);
  gconf_client_set_string (base_profile->priv->conf,
                           key, base_profile->priv->custom_command,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_ICON);
  gconf_client_set_string (base_profile->priv->conf,
                           key, base_profile->priv->icon_file,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_PALETTE);
  s = terminal_palette_to_string (base_profile->priv->palette);
  gconf_client_set_string (base_profile->priv->conf,
                           key, s,
                           &err);
  g_free (s);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_X_FONT);
  gconf_client_set_string (base_profile->priv->conf,
                           key, base_profile->priv->x_font,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_BACKGROUND_TYPE);
  cs = gconf_enum_to_string (background_types, base_profile->priv->background_type);
  gconf_client_set_string (base_profile->priv->conf,
                           key, cs,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_BACKGROUND_IMAGE);
  gconf_client_set_string (base_profile->priv->conf,
                           key, base_profile->priv->background_image_file,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_SCROLL_BACKGROUND);
  gconf_client_set_bool (base_profile->priv->conf,
                         key, base_profile->priv->scroll_background,
                         &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_BACKGROUND_DARKNESS);
  gconf_client_set_float (base_profile->priv->conf,
                          key, base_profile->priv->background_darkness,
                          &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_BACKSPACE_BINDING);
  cs = gconf_enum_to_string (erase_bindings, base_profile->priv->backspace_binding);
  gconf_client_set_string (base_profile->priv->conf,
                           key, cs,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_DELETE_BINDING);
  cs = gconf_enum_to_string (erase_bindings, base_profile->priv->delete_binding);
  gconf_client_set_string (base_profile->priv->conf,
                           key, cs,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_USE_THEME_COLORS);
  gconf_client_set_bool (base_profile->priv->conf,
                         key, base_profile->priv->use_theme_colors,
                         &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_USE_SYSTEM_FONT);
  gconf_client_set_bool (base_profile->priv->conf,
                         key, base_profile->priv->use_system_font,
                         &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_FONT);
  s = pango_font_description_to_string (base_profile->priv->font);
  gconf_client_set_string (base_profile->priv->conf,
                           key, s,
                           &err);
  g_free (s);
  
  BAIL_OUT_CHECK ();
  
  
  /* Add new profile to the profile list; the method for doing this has
   * a race condition where we and someone else set at the same time,
   * but I am just going to punt on this issue.
   */
  profiles = terminal_profile_get_list ();
  tmp = profiles;
  while (tmp != NULL)
    {
      name_list = g_slist_prepend (name_list,
                                   g_strdup (terminal_profile_get_name (tmp->data)));
      
      tmp = tmp->next;
    }

  name_list = g_slist_prepend (name_list, g_strdup (profile_name));
  
  gconf_client_set_list (base_profile->priv->conf,
                         CONF_GLOBAL_PREFIX"/profile_list",
                         GCONF_VALUE_STRING,
                         name_list,
                         &err);

  BAIL_OUT_CHECK ();
  
 cleanup:
  g_free (profile_name);
  g_free (profile_dir);
  g_free (key);

  g_list_free (profiles);

  if (name_list)
    {
      g_slist_foreach (name_list, (GFunc) g_free, NULL);
      g_slist_free (name_list);
    }
  
  if (err)
    {
      if (GTK_WIDGET_VISIBLE (transient_parent))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("There was an error creating the profile \"%s\""),
                                           visible_name);
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          dialog_add_details (GTK_DIALOG (dialog),
                              err->message);
          
          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show (dialog);
        }

      g_error_free (err);
    }
  
  g_object_unref (G_OBJECT (base_profile));
  g_object_unref (G_OBJECT (transient_parent));
}

void
terminal_profile_delete_list (GConfClient *conf,
                              GList       *deleted_profiles,
                              GtkWindow   *transient_parent)
{
  GList *current_profiles;
  GList *tmp;
  GSList *name_list;
  GError *err;

  /* reentrancy paranoia */
  g_object_ref (G_OBJECT (transient_parent));
  
  current_profiles = terminal_profile_get_list ();  

  /* remove deleted profiles from list */
  tmp = deleted_profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;
      
      current_profiles = g_list_remove (current_profiles, profile);
      
      tmp = tmp->next;
    }

  /* make list of profile names */
  name_list = NULL;
  tmp = current_profiles;
  while (tmp != NULL)
    {
      name_list = g_slist_prepend (name_list,
                                   g_strdup (terminal_profile_get_name (tmp->data)));
      
      tmp = tmp->next;
    }

  g_list_free (current_profiles);

  err = NULL;
  gconf_client_set_list (conf,
                         CONF_GLOBAL_PREFIX"/profile_list",
                         GCONF_VALUE_STRING,
                         name_list,
                         &err);

  g_slist_foreach (name_list, (GFunc) g_free, NULL);
  g_slist_free (name_list);

  if (err)
    {
      if (GTK_WIDGET_VISIBLE (transient_parent))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("There was an error deleting the profiles"));
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          dialog_add_details (GTK_DIALOG (dialog),
                              err->message);
          
          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show (dialog);
        }

      g_error_free (err);
    }

  g_object_unref (G_OBJECT (transient_parent));
}

TerminalProfile*
terminal_profile_get_default (void)
{
  return default_profile;
}

TerminalProfile*
terminal_profile_get_for_new_term (TerminalProfile *current)
{
  GList *list;
  TerminalProfile *profile;

  if (current)
    return current;
  
  if (default_profile)
    return default_profile;	

  list = terminal_profile_get_list ();
  if (list)
    profile = list->data;
  else
    profile = NULL;

  g_list_free (list);

  return profile;
}

/* We need to do this ourselves, because the gtk_color_selection_palette_to_string 
 * does not carry all the bytes, and xterm's palette is messed up...
 */

gchar*
color_selection_palette_to_string (const GdkColor *colors, gint n_colors)
{
  gint i;
  gchar **strs = NULL;
  gchar *retval;
  
  if (n_colors == 0)
    return g_strdup ("");

  strs = g_new0 (gchar*, n_colors + 1);

  for (i = 0; i < n_colors; ++i)
    {
      gchar *ptr;
      
      strs[i] = g_strdup_printf ("#%4X%4X%4X", colors[i].red, colors[i].green, colors[i].blue);

      for (ptr = strs[i]; *ptr; ptr++)
        if (*ptr == ' ')
          *ptr = '0';
    }

  retval = g_strjoinv (":", strs);

  g_strfreev (strs);

  return retval;
}

char*
terminal_palette_to_string (const GdkColor *palette)
{
  return color_selection_palette_to_string (palette,
                                                TERMINAL_PALETTE_SIZE);
}

gboolean
terminal_palette_from_string (const char     *str,
                              GdkColor       *palette,
                              gboolean        warn)
{
  GdkColor *colors;
  int n_colors;

  colors = NULL;
  n_colors = 0;
  if (!gtk_color_selection_palette_from_string (str,
                                                &colors, &n_colors))
    {
      if (warn)
        g_printerr (_("Could not parse string \"%s\" as a color palette\n"),
                    str);

      return FALSE;
    }

  if (n_colors < TERMINAL_PALETTE_SIZE)
    {
      if (warn)
        g_printerr (ngettext ("Palette had %d entry instead of %d\n",
                              "Palette had %d entries instead of %d\n",
			      n_colors),
                    n_colors, TERMINAL_PALETTE_SIZE);
    }
                                                
  /* We continue even with a funky palette size, so we can change the
   * palette size in future versions without causing too many issues.
   */
  if (TERMINAL_PALETTE_SIZE > n_colors)
    memcpy (palette, terminal_palette_linux, TERMINAL_PALETTE_SIZE * sizeof (GdkColor));
  
  memcpy (palette, colors, MIN (TERMINAL_PALETTE_SIZE, n_colors) * sizeof (GdkColor));

  g_free (colors);

  return TRUE;
}

gboolean 
terminal_setting_mask_is_empty (const TerminalSettingMask *mask)
{
  const unsigned int *p = (const unsigned int *) mask;
  const unsigned int *end = p + (sizeof (TerminalSettingMask) / 
                                 sizeof (unsigned int));

  while (p < end)
    {
      if (*p != 0)
        return FALSE;
      ++p;
    }

  return TRUE;
}

void
terminal_setting_mask_clear (TerminalSettingMask *mask)
{
  memset (mask, '\0', sizeof (*mask));
}

gboolean
terminal_setting_mask_equal (const TerminalSettingMask *a,
                             const TerminalSettingMask *b)
{
  /* This assumes the padding in the TerminalSettingMask
   * struct is initialized to 0
   */
  
  return memcmp (a, b, sizeof (*a)) == 0;
}

const GdkColor
terminal_palette_linux[TERMINAL_PALETTE_SIZE] =
{
  { 0, 0x0000, 0x0000, 0x0000 },
  { 0, 0xaaaa, 0x0000, 0x0000 },
  { 0, 0x0000, 0xaaaa, 0x0000 },
  { 0, 0xaaaa, 0x5555, 0x0000 },
  { 0, 0x0000, 0x0000, 0xaaaa },
  { 0, 0xaaaa, 0x0000, 0xaaaa },
  { 0, 0x0000, 0xaaaa, 0xaaaa },
  { 0, 0xaaaa, 0xaaaa, 0xaaaa },
  { 0, 0x5555, 0x5555, 0x5555 },
  { 0, 0xffff, 0x5555, 0x5555 },
  { 0, 0x5555, 0xffff, 0x5555 },
  { 0, 0xffff, 0xffff, 0x5555 },
  { 0, 0x5555, 0x5555, 0xffff },
  { 0, 0xffff, 0x5555, 0xffff },
  { 0, 0x5555, 0xffff, 0xffff },
  { 0, 0xffff, 0xffff, 0xffff }
};

const GdkColor
terminal_palette_xterm[TERMINAL_PALETTE_SIZE] =
{
    {0, 0x0000, 0x0000, 0x0000 },
    {0, 0xcdcb, 0x0000, 0x0000 },
    {0, 0x0000, 0xcdcb, 0x0000 },
    {0, 0xcdcb, 0xcdcb, 0x0000 },
    {0, 0x1e1a, 0x908f, 0xffff },
    {0, 0xcdcb, 0x0000, 0xcdcb },
    {0, 0x0000, 0xcdcb, 0xcdcb },
    {0, 0xe5e2, 0xe5e2, 0xe5e2 },
    {0, 0x4ccc, 0x4ccc, 0x4ccc },
    {0, 0xffff, 0x0000, 0x0000 },
    {0, 0x0000, 0xffff, 0x0000 },
    {0, 0xffff, 0xffff, 0x0000 },
    {0, 0x4645, 0x8281, 0xb4ae },
    {0, 0xffff, 0x0000, 0xffff },
    {0, 0x0000, 0xffff, 0xffff },
    {0, 0xffff, 0xffff, 0xffff }
};

const GdkColor
terminal_palette_rxvt[TERMINAL_PALETTE_SIZE] =
{
  { 0, 0x0000, 0x0000, 0x0000 },
  { 0, 0xcdcd, 0x0000, 0x0000 },
  { 0, 0x0000, 0xcdcd, 0x0000 },
  { 0, 0xcdcd, 0xcdcd, 0x0000 },
  { 0, 0x0000, 0x0000, 0xcdcd },
  { 0, 0xcdcd, 0x0000, 0xcdcd },
  { 0, 0x0000, 0xcdcd, 0xcdcd },
  { 0, 0xfafa, 0xebeb, 0xd7d7 },
  { 0, 0x4040, 0x4040, 0x4040 },
  { 0, 0xffff, 0x0000, 0x0000 },
  { 0, 0x0000, 0xffff, 0x0000 },
  { 0, 0xffff, 0xffff, 0x0000 },
  { 0, 0x0000, 0x0000, 0xffff },
  { 0, 0xffff, 0x0000, 0xffff },
  { 0, 0x0000, 0xffff, 0xffff },
  { 0, 0xffff, 0xffff, 0xffff }
};

