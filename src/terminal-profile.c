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

#include "terminal-intl.h"
#include "terminal-profile.h"
#include <gtk/gtk.h>
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
  guint cursor_blink : 1;
  guint default_show_menubar : 1;
  guint allow_bold : 1;
  guint silent_bell : 1;
  guint scroll_on_keystroke : 1;
  guint scroll_on_output : 1;
  guint login_shell : 1;
  guint update_records : 1;
  guint use_custom_command : 1;
  guint forgotten : 1;
};

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
  { -1, NULL }
};

static GHashTable *profiles = NULL;

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

static void emit_changed (TerminalProfile    *profile,
                          TerminalSettingMask mask);


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
  profile->priv = g_new0 (TerminalProfilePrivate, 1);
  profile->priv->locked = 0;
  profile->priv->cursor_blink = FALSE;
  profile->priv->default_show_menubar = TRUE;
  profile->priv->visible_name = g_strdup ("");
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
                  /* should be VOID__ENUM but I'm lazy */
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);

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
  
  g_free (profile->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

TerminalProfile*
terminal_profile_new (const char *name,
                      GConfClient *conf)
{
  TerminalProfile *profile;
  GError *err;
  
  profile = g_object_new (TERMINAL_TYPE_PROFILE, NULL);

  profile->priv->conf = conf;
  g_object_ref (G_OBJECT (conf));
  
  profile->priv->name = g_strdup (name);
  
  profile->priv->profile_dir = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX,
                                                         profile->priv->name);

  /*   g_print ("Watching dir %s\n", profile->priv->profile_dir); */
  
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

  if (profiles == NULL)
    profiles = g_hash_table_new (g_str_hash, g_str_equal);
  
  g_hash_table_insert (profiles, profile->priv->name, profile);
  
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
  if (strcmp (profile->priv->name, DEFAULT_PROFILE) == 0)
    return _(DEFAULT_PROFILE);
  else
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
    }
  /* otherwise just leave the old name */

  return TRUE;
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
    }
  /* otherwise just leave the old name */

  return TRUE;
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
    }
  /* otherwise just leave the old chars */
  
  return TRUE;
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
    }
  /* otherwise just leave the old command */
  
  return TRUE;
}

void
terminal_profile_update (TerminalProfile *profile)
{
  char *key;
  gboolean bool_val;
  char *str_val;
  int int_val;
  TerminalSettingMask mask;
  TerminalSettingMask locked;
  TerminalSettingMask old_locked;
  
  mask = 0;
  locked = 0;

  /* KEY_CURSOR_BLINK */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_CURSOR_BLINK);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  /*   g_print ("cursor blink is now %d\n", bool_val); */
  if (bool_val != profile->priv->cursor_blink)
    {
      mask |= TERMINAL_SETTING_CURSOR_BLINK;
      profile->priv->cursor_blink = bool_val;
    }  

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_CURSOR_BLINK;
  
  g_free (key);

  /* KEY_DEFAULT_SHOW_MENUBAR */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_DEFAULT_SHOW_MENUBAR);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);

  if (bool_val != profile->priv->default_show_menubar)
    {
      mask |= TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR;
      profile->priv->default_show_menubar = bool_val;
    }

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR;
  
  g_free (key);

  /* KEY_VISIBLE_NAME */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_VISIBLE_NAME);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_visible_name (profile, str_val))
    mask |= TERMINAL_SETTING_VISIBLE_NAME;

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_VISIBLE_NAME;
  
  g_free (key);

  /* KEY_FOREGROUND_COLOR */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_FOREGROUND_COLOR);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_foreground_color (profile, str_val))
    mask |= TERMINAL_SETTING_FOREGROUND_COLOR;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_FOREGROUND_COLOR;
  
  g_free (key);


  /* KEY_BACKGROUND_COLOR */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_BACKGROUND_COLOR);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_background_color (profile, str_val))
    mask |= TERMINAL_SETTING_BACKGROUND_COLOR;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_BACKGROUND_COLOR;
  
  g_free (key);


  /* KEY_TITLE */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_TITLE);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_title (profile, str_val))
    mask |= TERMINAL_SETTING_TITLE;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_TITLE;
  
  g_free (key);

  /* KEY_TITLE_MODE */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_TITLE_MODE);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_title_mode (profile, str_val))
    mask |= TERMINAL_SETTING_TITLE_MODE;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_TITLE_MODE;
  
  g_free (key);


  /* KEY_ALLOW_BOLD */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_ALLOW_BOLD);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->allow_bold)
    {
      mask |= TERMINAL_SETTING_ALLOW_BOLD;
      profile->priv->allow_bold = bool_val;
    }  

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_ALLOW_BOLD;
  
  g_free (key);

  /* KEY_SILENT_BELL */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SILENT_BELL);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->silent_bell)
    {
      mask |= TERMINAL_SETTING_SILENT_BELL;
      profile->priv->silent_bell = bool_val;
    }  

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_SILENT_BELL;
  
  g_free (key);

  /* KEY_WORD_CHARS */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_WORD_CHARS);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_word_chars (profile, str_val))
    mask |= TERMINAL_SETTING_WORD_CHARS;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_WORD_CHARS;
  
  g_free (key);

  /* KEY_SCROLLBAR_POSITION */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLLBAR_POSITION);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_scrollbar_position (profile, str_val))
    mask |= TERMINAL_SETTING_SCROLLBAR_POSITION;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_SCROLLBAR_POSITION;
  
  g_free (key);

  /* KEY_SCROLLBACK_LINES */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLLBACK_LINES);
  int_val = gconf_client_get_int (profile->priv->conf,
                                  key, NULL);
  if (set_scrollback_lines (profile, int_val))
    mask |= TERMINAL_SETTING_SCROLLBACK_LINES;

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_SCROLL_ON_KEYSTROKE;
  
  g_free (key);

  
  /* KEY_SCROLL_ON_KEYSTROKE */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLL_ON_KEYSTROKE);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->scroll_on_keystroke)
    {
      mask |= TERMINAL_SETTING_SCROLL_ON_KEYSTROKE;
      profile->priv->scroll_on_keystroke = bool_val;
    }  

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_SCROLL_ON_KEYSTROKE;
  
  g_free (key);

  /* KEY_SCROLL_ON_OUTPUT */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_SCROLL_ON_OUTPUT);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->scroll_on_output)
    {
      mask |= TERMINAL_SETTING_SCROLL_ON_OUTPUT;
      profile->priv->scroll_on_output = bool_val;
    }  

  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_SCROLL_ON_OUTPUT;
  
  g_free (key);

  /* KEY_EXIT_ACTION */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_EXIT_ACTION);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_exit_action (profile, str_val))
    mask |= TERMINAL_SETTING_EXIT_ACTION;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_EXIT_ACTION;
  
  g_free (key);
  
  /* KEY_LOGIN_SHELL */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_LOGIN_SHELL);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->login_shell)
    {
      mask |= TERMINAL_SETTING_LOGIN_SHELL;
      profile->priv->login_shell = bool_val;
    }  
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_LOGIN_SHELL;
  
  g_free (key);

  /* KEY_UPDATE_RECORDS */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_UPDATE_RECORDS);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->update_records)
    {
      mask |= TERMINAL_SETTING_UPDATE_RECORDS;
      profile->priv->update_records = bool_val;
    }  
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_UPDATE_RECORDS;
  
  g_free (key);

  /* KEY_USE_CUSTOM_COMMAND */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_USE_CUSTOM_COMMAND);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  if (bool_val != profile->priv->use_custom_command)
    {
      mask |= TERMINAL_SETTING_USE_CUSTOM_COMMAND;
      profile->priv->use_custom_command = bool_val;
    }  
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_USE_CUSTOM_COMMAND;
  
  g_free (key);

  /* KEY_CUSTOM_COMMAND */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_CUSTOM_COMMAND);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  if (set_custom_command (profile, str_val))
    mask |= TERMINAL_SETTING_CUSTOM_COMMAND;
  
  if (!gconf_client_key_is_writable (profile->priv->conf, key, NULL))
    locked |= TERMINAL_SETTING_CUSTOM_COMMAND;
  
  g_free (key);

  
  /* Update state and emit signals */
  
  old_locked = profile->priv->locked;
  profile->priv->locked = locked;
  
  if (mask != 0 || locked != old_locked)
    emit_changed (profile, mask);
}


static const gchar*
find_key (const gchar* key)
{
  const gchar* end;
  
  end = strrchr(key, '/');

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
  TerminalSettingMask mask;  
  TerminalSettingMask old_locked;
  
  profile = TERMINAL_PROFILE (user_data);

  mask = 0;
  old_locked = profile->priv->locked;

  val = gconf_entry_get_value (entry);
  
  key = find_key (gconf_entry_get_key (entry));
  
  /*   g_print ("Key '%s' changed\n", key); */

#define UPDATE_LOCKED(flag)                     \
      if (gconf_entry_get_is_writable (entry))  \
        profile->priv->locked &= ~(flag);        \
      else                                      \
        profile->priv->locked |= (flag);
  
  if (strcmp (key, KEY_CURSOR_BLINK) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->cursor_blink)
        {
          mask |= TERMINAL_SETTING_CURSOR_BLINK;
          profile->priv->cursor_blink = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_CURSOR_BLINK);
    }
  else if (strcmp (key, KEY_DEFAULT_SHOW_MENUBAR) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->default_show_menubar)
        {
          mask |= TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR;
          profile->priv->default_show_menubar = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR);
    }
  else if (strcmp (key, KEY_VISIBLE_NAME) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_visible_name (profile, str_val))
        mask |= TERMINAL_SETTING_VISIBLE_NAME;

      UPDATE_LOCKED (TERMINAL_SETTING_VISIBLE_NAME);
    }
  else if (strcmp (key, KEY_FOREGROUND_COLOR) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_foreground_color (profile, str_val))
        mask |= TERMINAL_SETTING_FOREGROUND_COLOR;

      UPDATE_LOCKED (TERMINAL_SETTING_FOREGROUND_COLOR);
    }
  else if (strcmp (key, KEY_BACKGROUND_COLOR) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_background_color (profile, str_val))
        mask |= TERMINAL_SETTING_BACKGROUND_COLOR;

      UPDATE_LOCKED (TERMINAL_SETTING_BACKGROUND_COLOR);
    }
  else if (strcmp (key, KEY_TITLE) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_title (profile, str_val))
        mask |= TERMINAL_SETTING_TITLE;

      UPDATE_LOCKED (TERMINAL_SETTING_TITLE);
    }
  else if (strcmp (key, KEY_TITLE_MODE) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_title_mode (profile, str_val))
        mask |= TERMINAL_SETTING_TITLE_MODE;

      UPDATE_LOCKED (TERMINAL_SETTING_TITLE_MODE);
    }
  else if (strcmp (key, KEY_ALLOW_BOLD) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->allow_bold)
        {
          mask |= TERMINAL_SETTING_ALLOW_BOLD;
          profile->priv->allow_bold = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_ALLOW_BOLD);
    }
  else if (strcmp (key, KEY_SILENT_BELL) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->silent_bell)
        {
          mask |= TERMINAL_SETTING_SILENT_BELL;
          profile->priv->silent_bell = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_SILENT_BELL);
    }
  else if (strcmp (key, KEY_WORD_CHARS) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_word_chars (profile, str_val))
        mask |= TERMINAL_SETTING_WORD_CHARS;

      UPDATE_LOCKED (TERMINAL_SETTING_WORD_CHARS);
    }
  else if (strcmp (key, KEY_SCROLLBAR_POSITION) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_scrollbar_position (profile, str_val))
        mask |= TERMINAL_SETTING_SCROLLBAR_POSITION;

      UPDATE_LOCKED (TERMINAL_SETTING_SCROLLBAR_POSITION);
    }
  else if (strcmp (key, KEY_SCROLLBACK_LINES) == 0)
    {
      int int_val;

      int_val = profile->priv->scrollback_lines;
      if (val && val->type == GCONF_VALUE_INT)
        int_val = gconf_value_get_int (val);
      
      if (set_scrollback_lines (profile, int_val))
        mask |= TERMINAL_SETTING_SCROLLBACK_LINES;

      UPDATE_LOCKED (TERMINAL_SETTING_SCROLLBACK_LINES);
    }
  else if (strcmp (key, KEY_SCROLL_ON_KEYSTROKE) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->scroll_on_keystroke)
        {
          mask |= TERMINAL_SETTING_SCROLL_ON_KEYSTROKE;
          profile->priv->scroll_on_keystroke = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_SCROLL_ON_KEYSTROKE);
    }
  else if (strcmp (key, KEY_SCROLL_ON_OUTPUT) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->scroll_on_output)
        {
          mask |= TERMINAL_SETTING_SCROLL_ON_OUTPUT;
          profile->priv->scroll_on_output = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_SCROLL_ON_OUTPUT);
    }  
  else if (strcmp (key, KEY_EXIT_ACTION) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_exit_action (profile, str_val))
        mask |= TERMINAL_SETTING_EXIT_ACTION;

      UPDATE_LOCKED (TERMINAL_SETTING_EXIT_ACTION);
    }
  else if (strcmp (key, KEY_LOGIN_SHELL) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->login_shell)
        {
          mask |= TERMINAL_SETTING_LOGIN_SHELL;
          profile->priv->login_shell = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_LOGIN_SHELL);
    }
  else if (strcmp (key, KEY_UPDATE_RECORDS) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->update_records)
        {
          mask |= TERMINAL_SETTING_UPDATE_RECORDS;
          profile->priv->update_records = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_UPDATE_RECORDS);
    }
  else if (strcmp (key, KEY_USE_CUSTOM_COMMAND) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->use_custom_command)
        {
          mask |= TERMINAL_SETTING_USE_CUSTOM_COMMAND;
          profile->priv->use_custom_command = bool_val;
        }

      UPDATE_LOCKED (TERMINAL_SETTING_USE_CUSTOM_COMMAND);
    }
  else if (strcmp (key, KEY_CUSTOM_COMMAND) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      if (set_custom_command (profile, str_val))
        mask |= TERMINAL_SETTING_CUSTOM_COMMAND;

      UPDATE_LOCKED (TERMINAL_SETTING_CUSTOM_COMMAND);
    }

  
  if (mask != 0 || old_locked != profile->priv->locked)
    emit_changed (profile, mask);
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
      g_hash_table_remove (profiles, profile->priv->name);
      profile->priv->forgotten = TRUE;
      g_signal_emit (G_OBJECT (profile), signals[FORGOTTEN], 0);
    }
}

TerminalSettingMask
terminal_profile_get_locked_settings (TerminalProfile *profile)
{
  if (strcmp (profile->priv->name, DEFAULT_PROFILE) == 0)
    return profile->priv->locked | TERMINAL_SETTING_VISIBLE_NAME;
  else
    return profile->priv->locked;
}

void
terminal_profile_setup_default (GConfClient *conf)
{
  TerminalProfile *profile;

  profile = terminal_profile_new (DEFAULT_PROFILE, conf);

  terminal_profile_update (profile);
}

static void
emit_changed (TerminalProfile    *profile,
              TerminalSettingMask mask)
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
  /* default title is profile name, not copied from base */
  gconf_client_set_string (base_profile->priv->conf,
                           key, base_profile->priv->custom_command,
                           &err);
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

      g_print ("Deleting profile '%s'\n",
               terminal_profile_get_visible_name (profile));
      
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
