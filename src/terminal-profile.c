/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
 * Copyright © 2008 Christian Persch
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gnome-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-encoding.h"
#include "terminal-profile.h"
#include "terminal-screen.h"
#include "terminal-type-builtins.h"

/* To add a new key, you need to:
 *
 *  - add an entry to the enum below
 *  - add a #define with its name in terminal-profile.h
 *  - add a gobject property for it in terminal_profile_class_init
 *  - if the property's type needs special casing, add that to
 *    terminal_profile_gconf_notify_cb and
 *    terminal_profile_gconf_changeset_add
 *  - if necessary the default value cannot be handled via the paramspec,
 *    handle that in terminal_profile_reset_property_internal
 */
enum
{
  PROP_0,
  PROP_ALLOW_BOLD,
  PROP_BACKGROUND_COLOR,
  PROP_BACKGROUND_DARKNESS,
  PROP_BACKGROUND_IMAGE,
  PROP_BACKGROUND_IMAGE_FILE,
  PROP_BACKGROUND_TYPE,
  PROP_BACKSPACE_BINDING,
  PROP_BOLD_COLOR,
  PROP_BOLD_COLOR_SAME_AS_FG,
  PROP_CURSOR_BLINK_MODE,
  PROP_CURSOR_SHAPE,
  PROP_CUSTOM_COMMAND,
  PROP_DEFAULT_SIZE_COLUMNS,
  PROP_DEFAULT_SIZE_ROWS,
  PROP_DEFAULT_SHOW_MENUBAR,
  PROP_DELETE_BINDING,
  PROP_ENCODING,
  PROP_EXIT_ACTION,
  PROP_FONT,
  PROP_FOREGROUND_COLOR,
  PROP_LOGIN_SHELL,
  PROP_NAME,
  PROP_PALETTE,
  PROP_SCROLL_BACKGROUND,
  PROP_SCROLLBACK_LINES,
  PROP_SCROLLBACK_UNLIMITED,
  PROP_SCROLLBAR_POSITION,
  PROP_SCROLL_ON_KEYSTROKE,
  PROP_SCROLL_ON_OUTPUT,
  PROP_SILENT_BELL,
  PROP_TITLE,
  PROP_TITLE_MODE,
  PROP_UPDATE_RECORDS,
  PROP_USE_CUSTOM_COMMAND,
  PROP_USE_CUSTOM_DEFAULT_SIZE,
  PROP_USE_SYSTEM_FONT,
  PROP_USE_THEME_COLORS,
  PROP_VISIBLE_NAME,
  PROP_WORD_CHARS,
  LAST_PROP
};

#define KEY_ALLOW_BOLD "allow_bold"
#define KEY_BACKGROUND_COLOR "background_color"
#define KEY_BACKGROUND_DARKNESS "background_darkness"
#define KEY_BACKGROUND_IMAGE_FILE "background_image"
#define KEY_BACKGROUND_TYPE "background_type"
#define KEY_BACKSPACE_BINDING "backspace_binding"
#define KEY_BOLD_COLOR "bold_color"
#define KEY_BOLD_COLOR_SAME_AS_FG "bold_color_same_as_fg"
#define KEY_CURSOR_BLINK_MODE "cursor_blink_mode"
#define KEY_CURSOR_SHAPE "cursor_shape"
#define KEY_CUSTOM_COMMAND "custom_command"
#define KEY_DEFAULT_SHOW_MENUBAR "default_show_menubar"
#define KEY_DEFAULT_SIZE_COLUMNS "default_size_columns"
#define KEY_DEFAULT_SIZE_ROWS "default_size_rows"
#define KEY_DELETE_BINDING "delete_binding"
#define KEY_EXIT_ACTION "exit_action"
#define KEY_ENCODING "encoding"
#define KEY_FONT "font"
#define KEY_FOREGROUND_COLOR "foreground_color"
#define KEY_LOGIN_SHELL "login_shell"
#define KEY_PALETTE "palette"
#define KEY_SCROLL_BACKGROUND "scroll_background"
#define KEY_SCROLLBACK_LINES "scrollback_lines"
#define KEY_SCROLLBACK_UNLIMITED "scrollback_unlimited"
#define KEY_SCROLLBAR_POSITION "scrollbar_position"
#define KEY_SCROLL_ON_KEYSTROKE "scroll_on_keystroke"
#define KEY_SCROLL_ON_OUTPUT "scroll_on_output"
#define KEY_SILENT_BELL "silent_bell"
#define KEY_TITLE_MODE "title_mode"
#define KEY_TITLE "title"
#define KEY_UPDATE_RECORDS "update_records"
#define KEY_USE_CUSTOM_COMMAND "use_custom_command"
#define KEY_USE_CUSTOM_DEFAULT_SIZE "use_custom_default_size"
#define KEY_USE_SYSTEM_FONT "use_system_font"
#define KEY_USE_THEME_COLORS "use_theme_colors"
#define KEY_VISIBLE_NAME "visible_name"
#define KEY_WORD_CHARS "word_chars"

/* Keep these in sync with the GConf schema! */
#define DEFAULT_ALLOW_BOLD            (TRUE)
#define DEFAULT_BACKGROUND_COLOR      ("#FFFFDD")
#define DEFAULT_BOLD_COLOR_SAME_AS_FG (TRUE)
#define DEFAULT_BACKGROUND_DARKNESS   (0.5)
#define DEFAULT_BACKGROUND_IMAGE_FILE ("")
#define DEFAULT_BACKGROUND_IMAGE      (NULL)
#define DEFAULT_BACKGROUND_TYPE       (TERMINAL_BACKGROUND_SOLID)
#define DEFAULT_BACKSPACE_BINDING     (VTE_ERASE_ASCII_DELETE)
#define DEFAULT_CURSOR_BLINK_MODE     (VTE_CURSOR_BLINK_SYSTEM)
#define DEFAULT_CURSOR_SHAPE          (VTE_CURSOR_SHAPE_BLOCK)
#define DEFAULT_CUSTOM_COMMAND        ("")
#define DEFAULT_DEFAULT_SHOW_MENUBAR  (TRUE)
#define DEFAULT_DEFAULT_SIZE_COLUMNS  (80)
#define DEFAULT_DEFAULT_SIZE_ROWS     (24)
#define DEFAULT_DELETE_BINDING        (VTE_ERASE_DELETE_SEQUENCE)
#define DEFAULT_ENCODING              (NULL)
#define DEFAULT_EXIT_ACTION           (TERMINAL_EXIT_CLOSE)
#define DEFAULT_FONT                  ("Monospace 12")
#define DEFAULT_FOREGROUND_COLOR      ("#000000")
#define DEFAULT_LOGIN_SHELL           (FALSE)
#define DEFAULT_NAME                  (NULL)
#define DEFAULT_PALETTE               (terminal_palettes[TERMINAL_PALETTE_TANGO])
#define DEFAULT_SCROLL_BACKGROUND     (TRUE)
#define DEFAULT_SCROLLBACK_LINES      (512)
#define DEFAULT_SCROLLBACK_UNLIMITED  (FALSE)
#define DEFAULT_SCROLLBAR_POSITION    (TERMINAL_SCROLLBAR_RIGHT)
#define DEFAULT_SCROLL_ON_KEYSTROKE   (TRUE)
#define DEFAULT_SCROLL_ON_OUTPUT      (FALSE)
#define DEFAULT_SILENT_BELL           (FALSE)
#define DEFAULT_TITLE_MODE            (TERMINAL_TITLE_REPLACE)
#define DEFAULT_TITLE                 (N_("Terminal"))
#define DEFAULT_UPDATE_RECORDS        (TRUE)
#define DEFAULT_USE_CUSTOM_COMMAND    (FALSE)
#define DEFAULT_USE_CUSTOM_DEFAULT_SIZE (FALSE)
#define DEFAULT_USE_SYSTEM_FONT       (TRUE)
#define DEFAULT_USE_THEME_COLORS      (TRUE)
#define DEFAULT_VISIBLE_NAME          (N_("Unnamed"))
#define DEFAULT_WORD_CHARS            ("-A-Za-z0-9,./?%&#:_=+@~")

struct _TerminalProfilePrivate
{
  GValueArray *properties;
  gboolean *locked;

  GConfClient *conf;
  char *profile_dir;
  guint notify_id;

  GSList *dirty_pspecs;
  guint save_idle_id;

  GParamSpec *gconf_notification_pspec;

  gboolean background_load_failed;

  guint forgotten : 1;
};

/* We have to continue to use these since they're unfortunately different
 * from the value nicks of the vte_terminal_erase_binding_get_type() enum type.
 */
static const GConfEnumStringPair erase_bindings[] = {
  { VTE_ERASE_AUTO, "auto" },
  { VTE_ERASE_ASCII_BACKSPACE, "control-h" },
  { VTE_ERASE_ASCII_DELETE, "ascii-del" },
  { VTE_ERASE_DELETE_SEQUENCE, "escape-sequence" },
  { VTE_ERASE_TTY, "tty" },
  { -1, NULL }
};

static const GdkColor terminal_palettes[TERMINAL_PALETTE_N_BUILTINS][TERMINAL_PALETTE_SIZE] =
{
  /* Tango palette */
  {
    { 0, 0x0000, 0x0000, 0x0000 },
    { 0, 0xcccc, 0x0000, 0x0000 },
    { 0, 0x4e4e, 0x9a9a, 0x0606 },
    { 0, 0xc4c4, 0xa0a0, 0x0000 },
    { 0, 0x3434, 0x6565, 0xa4a4 },
    { 0, 0x7575, 0x5050, 0x7b7b },
    { 0, 0x0606, 0x9820, 0x9a9a },
    { 0, 0xd3d3, 0xd7d7, 0xcfcf },
    { 0, 0x5555, 0x5757, 0x5353 },
    { 0, 0xefef, 0x2929, 0x2929 },
    { 0, 0x8a8a, 0xe2e2, 0x3434 },
    { 0, 0xfcfc, 0xe9e9, 0x4f4f },
    { 0, 0x7272, 0x9f9f, 0xcfcf },
    { 0, 0xadad, 0x7f7f, 0xa8a8 },
    { 0, 0x3434, 0xe2e2, 0xe2e2 },
    { 0, 0xeeee, 0xeeee, 0xecec }
  },

  /* Linux palette */
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
  },

  /* XTerm palette */
  {
    { 0, 0x0000, 0x0000, 0x0000 },
    { 0, 0xcdcb, 0x0000, 0x0000 },
    { 0, 0x0000, 0xcdcb, 0x0000 },
    { 0, 0xcdcb, 0xcdcb, 0x0000 },
    { 0, 0x1e1a, 0x908f, 0xffff },
    { 0, 0xcdcb, 0x0000, 0xcdcb },
    { 0, 0x0000, 0xcdcb, 0xcdcb },
    { 0, 0xe5e2, 0xe5e2, 0xe5e2 },
    { 0, 0x4ccc, 0x4ccc, 0x4ccc },
    { 0, 0xffff, 0x0000, 0x0000 },
    { 0, 0x0000, 0xffff, 0x0000 },
    { 0, 0xffff, 0xffff, 0x0000 },
    { 0, 0x4645, 0x8281, 0xb4ae },
    { 0, 0xffff, 0x0000, 0xffff },
    { 0, 0x0000, 0xffff, 0xffff },
    { 0, 0xffff, 0xffff, 0xffff }
  },

  /* RXVT palette */
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
  }
};

static const GdkColor default_fg_color = { 0, 0, 0, 0 };
static const GdkColor default_bg_color = { 0, 0xffff, 0xffff, 0xdddd };

enum
{
  FORGOTTEN,
  LAST_SIGNAL
};

static void terminal_profile_finalize    (GObject              *object);
static void terminal_profile_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void ensure_pixbuf_property (TerminalProfile *profile,
                                    guint path_prop_id,
                                    guint pixbuf_prop_id,
                                    gboolean *load_failed);

static guint signals[LAST_SIGNAL];
static GQuark gconf_key_quark;

G_DEFINE_TYPE (TerminalProfile, terminal_profile, G_TYPE_OBJECT);

static gboolean
palette_cmp (const GdkColor *ca,
             const GdkColor *cb)
{
  guint i;

  for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
    if (!gdk_color_equal (&ca[i], &cb[i]))
      return FALSE;

  return TRUE;
}

static GParamSpec *
get_pspec_from_name (TerminalProfile *profile,
                     const char *prop_name)
{
  TerminalProfileClass *klass = TERMINAL_PROFILE_GET_CLASS (profile);
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), prop_name);
  if (pspec &&
      pspec->owner_type != TERMINAL_TYPE_PROFILE)
    pspec = NULL;

  return pspec;
}

static const GValue *
get_prop_value_from_prop_name (TerminalProfile *profile,
                               const char *prop_name)
{
  TerminalProfilePrivate *priv = profile->priv;
  GParamSpec *pspec;

  pspec = get_pspec_from_name (profile, prop_name);
  if (!pspec)
    return NULL;

  if (G_UNLIKELY (pspec->param_id == PROP_BACKGROUND_IMAGE))
    ensure_pixbuf_property (profile, PROP_BACKGROUND_IMAGE_FILE, PROP_BACKGROUND_IMAGE, &priv->background_load_failed);

  return g_value_array_get_nth (priv->properties, pspec->param_id);
}

static void
set_value_from_palette (GValue *ret_value,
                        const GdkColor *colors,
                        guint n_colors)
{
  GValueArray *array;
  guint i, max_n_colors;

  max_n_colors = MAX (n_colors, TERMINAL_PALETTE_SIZE);
  array = g_value_array_new (max_n_colors);
  for (i = 0; i < max_n_colors; ++i)
    g_value_array_append (array, NULL);

  for (i = 0; i < n_colors; ++i)
    {
      GValue *value = g_value_array_get_nth (array, i);

      g_value_init (value, GDK_TYPE_COLOR);
      g_value_set_boxed (value, &colors[i]);
    }

  /* If we haven't enough colours yet, fill up with the default palette */
  for (i = n_colors; i < TERMINAL_PALETTE_SIZE; ++i)
    {
      GValue *value = g_value_array_get_nth (array, i);

      g_value_init (value, GDK_TYPE_COLOR);
      g_value_set_boxed (value, &DEFAULT_PALETTE[i]);
    }

  g_value_take_boxed (ret_value, array);
}

static int
values_equal (GParamSpec *pspec,
              const GValue *va,
              const GValue *vb)
{
  /* g_param_values_cmp isn't good enough for some types, since e.g.
   * it compares colours and font descriptions by pointer value, not
   * with the correct compare functions. Providing extra
   * PangoParamSpecFontDescription and GdkParamSpecColor wouldn't
   * have fixed this either, since it's unclear how to _order_ them.
   * Luckily we only need to check them for equality here.
   */

  if (g_param_values_cmp (pspec, va, vb) == 0)
    return TRUE;

  if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_COLOR)
    return gdk_color_equal (g_value_get_boxed (va), g_value_get_boxed (vb));

  if (G_PARAM_SPEC_VALUE_TYPE (pspec) == PANGO_TYPE_FONT_DESCRIPTION)
    return pango_font_description_equal (g_value_get_boxed (va), g_value_get_boxed (vb));

  if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec) &&
      G_PARAM_SPEC_VALUE_TYPE (G_PARAM_SPEC_VALUE_ARRAY (pspec)->element_spec) == GDK_TYPE_COLOR)
    {
      GValueArray *ara, *arb;
      guint i;

      ara = g_value_get_boxed (va);
      arb = g_value_get_boxed (vb);

      if (!ara || !arb || ara->n_values != arb->n_values)
        return FALSE;
        
      for (i = 0; i < ara->n_values; ++i)
        if (!gdk_color_equal (g_value_get_boxed (g_value_array_get_nth (ara, i)),
                              g_value_get_boxed (g_value_array_get_nth (arb, i))))
          return FALSE;

      return TRUE;
    }

  return FALSE;
}

static void
ensure_pixbuf_property (TerminalProfile *profile,
                        guint path_prop_id,
                        guint pixbuf_prop_id,
                        gboolean *load_failed)
{
  TerminalProfilePrivate *priv = profile->priv;
  GValue *path_value, *pixbuf_value;
  GdkPixbuf *pixbuf;
  const char *path_utf8;
  char *path;
  GError *error = NULL;

  pixbuf_value = g_value_array_get_nth (priv->properties, pixbuf_prop_id);

  pixbuf = g_value_get_object (pixbuf_value);
  if (pixbuf)
    return;

  if (*load_failed)
    return;

  path_value = g_value_array_get_nth (priv->properties, path_prop_id);
  path_utf8 = g_value_get_string (path_value);
  if (!path_utf8 || !path_utf8[0])
    goto failed;

  path = g_filename_from_utf8 (path_utf8, -1, NULL, NULL, NULL);
  if (!path)
    goto failed;

  pixbuf = gdk_pixbuf_new_from_file (path, &error);
  if (!pixbuf)
    {
      _terminal_debug_print (TERMINAL_DEBUG_PROFILE,
                             "Failed to load image \"%s\": %s\n",
                             path, error->message);

      g_error_free (error);
      g_free (path);
      goto failed;
    }
          
  g_value_take_object (pixbuf_value, pixbuf);
  g_free (path);
  return;

failed:
  *load_failed = TRUE;
}

static void
terminal_profile_reset_property_internal (TerminalProfile *profile,
                                          GParamSpec *pspec,
                                          gboolean notify)
{
  TerminalProfilePrivate *priv = profile->priv;
  GValue value_ = { 0, };
  GValue *value;

  if (notify)
    {
      value = &value_;
      g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));
    }
  else
    value = g_value_array_get_nth (priv->properties, pspec->param_id);
  g_assert (value != NULL);

  /* A few properties don't have defaults via the param spec; set them explicitly */
  switch (pspec->param_id)
    {
      case PROP_FOREGROUND_COLOR:
      case PROP_BOLD_COLOR:
        g_value_set_boxed (value, &DEFAULT_FOREGROUND_COLOR);
        break;

      case PROP_BACKGROUND_COLOR:
        g_value_set_boxed (value, &DEFAULT_BACKGROUND_COLOR);
        break;

      case PROP_ENCODING:
        g_value_set_boxed (value, terminal_app_ensure_encoding (terminal_app_get (), DEFAULT_ENCODING));
        break;

      case PROP_FONT:
        g_value_take_boxed (value, pango_font_description_from_string (DEFAULT_FONT));
        break;

      case PROP_PALETTE:
        set_value_from_palette (value, DEFAULT_PALETTE, TERMINAL_PALETTE_SIZE);
        break;

      default:
        g_param_value_set_default (pspec, value);
        break;
    }

  if (notify)
    {
      g_object_set_property (G_OBJECT (profile), pspec->name, value);
      g_value_unset (value);
    }
}

static void
terminal_profile_gconf_notify_cb (GConfClient *client,
                                  guint        cnxn_id,
                                  GConfEntry  *entry,
                                  gpointer     user_data)
{
  TerminalProfile *profile = TERMINAL_PROFILE (user_data);
  TerminalProfilePrivate *priv = profile->priv;
  TerminalProfileClass *klass;
  const char *key;
  GConfValue *gconf_value;
  GParamSpec *pspec;
  GValue value = { 0, };
  gboolean equal;
  gboolean force_set = FALSE;

  key = gconf_entry_get_key (entry);
  if (!key || !g_str_has_prefix (key, priv->profile_dir))
    return;

  _terminal_debug_print (TERMINAL_DEBUG_PROFILE,
                         "GConf notification for key %s [%s]\n",
                         key,
                         gconf_entry_get_is_writable (entry) ? "writable" : "LOCKED");

  key += strlen (priv->profile_dir);
  if (!key[0])
    return;

  key++;
  klass = TERMINAL_PROFILE_GET_CLASS (profile);
  pspec = g_hash_table_lookup (klass->gconf_keys, key);
  if (!pspec)
    return; /* ignore unknown keys, for future extensibility */

  priv->locked[pspec->param_id] = !gconf_entry_get_is_writable (entry);

  gconf_value = gconf_entry_get_value (entry);
  if (!gconf_value)
    return;

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
    {
      if (gconf_value->type != GCONF_VALUE_BOOL)
        goto out;

      g_value_set_boolean (&value, gconf_value_get_bool (gconf_value));
    }
  else if (G_IS_PARAM_SPEC_STRING (pspec))
    {
      if (gconf_value->type != GCONF_VALUE_STRING)
        goto out;

      g_value_set_string (&value, gconf_value_get_string (gconf_value));
    }
  else if (G_IS_PARAM_SPEC_ENUM (pspec))
    {
      const GEnumValue *eval;
      int enum_value;

      if (gconf_value->type != GCONF_VALUE_STRING)
        goto out;

      eval = g_enum_get_value_by_nick (G_PARAM_SPEC_ENUM (pspec)->enum_class,
                                       gconf_value_get_string (gconf_value));
      if (eval)
        enum_value = eval->value;
      else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == vte_terminal_erase_binding_get_type ())
        {
          /* Backward compatibility */
          if (!gconf_string_to_enum ((GConfEnumStringPair*) erase_bindings,
                                     gconf_value_get_string (gconf_value),
                                     &enum_value))
            goto out;
        }
      else
        goto out;

      g_value_set_enum (&value, enum_value);
    }
  else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_COLOR)
    {
      GdkColor color;

      if (gconf_value->type != GCONF_VALUE_STRING)
        goto out;

      if (!gdk_color_parse (gconf_value_get_string (gconf_value), &color))
        goto out;

      g_value_set_boxed (&value, &color);
    }
  else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == PANGO_TYPE_FONT_DESCRIPTION)
    {
      if (gconf_value->type != GCONF_VALUE_STRING)
        goto out;

      g_value_take_boxed (&value, pango_font_description_from_string (gconf_value_get_string (gconf_value)));
    }
  else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == TERMINAL_TYPE_ENCODING)
    {
      if (gconf_value->type != GCONF_VALUE_STRING)
        goto out;

      g_value_set_boxed (&value, terminal_app_ensure_encoding (terminal_app_get (), gconf_value_get_string (gconf_value)));
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    {
      if (gconf_value->type != GCONF_VALUE_FLOAT)
        goto out;

      g_value_set_double (&value, gconf_value_get_float (gconf_value));
    }
  else if (G_IS_PARAM_SPEC_INT (pspec))
    {
      if (gconf_value->type != GCONF_VALUE_INT)
        goto out;

      g_value_set_int (&value, gconf_value_get_int (gconf_value));
    }
  else if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec) &&
           G_PARAM_SPEC_VALUE_TYPE (G_PARAM_SPEC_VALUE_ARRAY (pspec)->element_spec) == GDK_TYPE_COLOR)
    {
      char **color_strings;
      GdkColor *colors;
      int n_colors, i;

      if (gconf_value->type != GCONF_VALUE_STRING)
        goto out;

      color_strings = g_strsplit (gconf_value_get_string (gconf_value), ":", -1);
      if (!color_strings)
        goto out;

      n_colors = g_strv_length (color_strings);
      colors = g_new0 (GdkColor, n_colors);
      for (i = 0; i < n_colors; ++i)
        {
          if (!gdk_color_parse (color_strings[i], &colors[i]))
            continue; /* ignore errors */
        }
      g_strfreev (color_strings);

      /* We continue even with a palette size != TERMINAL_PALETTE_SIZE,
       * so we can change the palette size in future versions without
       * causing too many issues.
       */
      set_value_from_palette (&value, colors, n_colors);
      g_free (colors);
    }
  else
    {
      g_printerr ("Unhandled value type %s of pspec %s\n", g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)), pspec->name);
      goto out;
    }

  if (g_param_value_validate (pspec, &value))
    {
      _terminal_debug_print (TERMINAL_DEBUG_PROFILE,
                             "Invalid value in gconf for key %s was changed to comply with pspec %s\n",
                             gconf_entry_get_key (entry), pspec->name);

      force_set = TRUE;
    }

  /* Only set the property if the value is different than our current value,
   * so we don't go into an infinite loop.
   */
  equal = values_equal (pspec, &value, g_value_array_get_nth (priv->properties, pspec->param_id));
#ifdef GNOME_ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROFILE)
    {
      if (!equal)
        _terminal_debug_print (TERMINAL_DEBUG_PROFILE,
                              "Setting property %s to a different value\n"
                                "  now: %s\n"
                                "  new: %s\n",
                                pspec->name,
                                g_strdup_value_contents (g_value_array_get_nth (priv->properties, pspec->param_id)),
                                g_strdup_value_contents (&value));
    }
#endif

  if (!equal || force_set)
    {
      priv->gconf_notification_pspec = pspec;
      g_object_set_property (G_OBJECT (profile), pspec->name, &value);
      priv->gconf_notification_pspec = NULL;
    }

out:
  /* FIXME: if we arrive here through goto in the error cases,
   * should we maybe reset the property to its default value?
   */

  g_value_unset (&value);
}

static void
terminal_profile_gconf_changeset_add (TerminalProfile *profile,
                                      GConfChangeSet *changeset,
                                      GParamSpec *pspec)
{
  TerminalProfilePrivate *priv = profile->priv;
  const char *gconf_key;
  char *key;
  const GValue *value;

  /* FIXME: do this? */
#if 0
  if (priv->locked[pspec->param_id])
    return;

  if (!gconf_client_key_is_writable (priv->conf, gconf_key, NULL))
    return;
#endif
  
  gconf_key = g_param_spec_get_qdata (pspec, gconf_key_quark);
  if (!gconf_key)
    return;

  key = gconf_concat_dir_and_key (priv->profile_dir, gconf_key);
  value = g_value_array_get_nth (priv->properties, pspec->param_id);

  _terminal_debug_print (TERMINAL_DEBUG_PROFILE,
                         "Adding pspec %s with value %s to the gconf changeset\n",
                         pspec->name, g_strdup_value_contents (value));

  if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
    gconf_change_set_set_bool (changeset, key, g_value_get_boolean (value));
  else if (G_IS_PARAM_SPEC_STRING (pspec))
    {
      const char *str;

      str = g_value_get_string (value);
      gconf_change_set_set_string (changeset, key, str ? str : "");
    }
  else if (G_IS_PARAM_SPEC_ENUM (pspec))
    {
      const GEnumValue *eval;
      const char *string;

      eval = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, g_value_get_enum (value));

      if (G_PARAM_SPEC_VALUE_TYPE (pspec) == vte_terminal_erase_binding_get_type ())
        {
          /* Backward compatibility */
          string = gconf_enum_to_string ((GConfEnumStringPair*) erase_bindings, g_value_get_enum (value));
          if (!string)
            goto cleanup;
        }
      else if (eval)
        string = eval->value_nick;
      else
        goto cleanup;

      gconf_change_set_set_string (changeset, key, string);
    }
  else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_COLOR)
    {
      GdkColor *color;
      char str[16];

      color = g_value_get_boxed (value);
      if (!color)
        goto cleanup;

      g_snprintf (str, sizeof (str),
                  "#%04X%04X%04X",
                  color->red,
                  color->green,
                  color->blue);

      gconf_change_set_set_string (changeset, key, str);
    }
  else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == PANGO_TYPE_FONT_DESCRIPTION)
    {
      PangoFontDescription *font_desc;
      char *font;

      font_desc = g_value_get_boxed (value);
      if (!font_desc)
        goto cleanup;

      font = pango_font_description_to_string (font_desc);
      gconf_change_set_set_string (changeset, key, font);
      g_free (font);
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    gconf_change_set_set_float (changeset, key, (float) g_value_get_double (value));
  else if (G_IS_PARAM_SPEC_INT (pspec))
    gconf_change_set_set_int (changeset, key, g_value_get_int (value));
  else if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec) &&
           G_PARAM_SPEC_VALUE_TYPE (G_PARAM_SPEC_VALUE_ARRAY (pspec)->element_spec) == GDK_TYPE_COLOR)
    {
      GValueArray *array;
      GString *string;
      guint n_colors, i;

      /* We need to do this ourselves, because the gtk_color_selection_palette_to_string
       * does not carry all the bytes, and xterm's palette is messed up...
       */

      array = g_value_get_boxed (value);
      if (!array)
        goto cleanup;

      n_colors = array->n_values;
      string = g_string_sized_new (n_colors * (1 /* # */ + 3 * 4) + n_colors /* : separators and terminating \0 */);
      for (i = 0; i < n_colors; ++i)
        {
          GdkColor *color;

          if (i > 0)
            g_string_append_c (string, ':');

          color = g_value_get_boxed (g_value_array_get_nth (array, i));
          if (!color)
            continue;

          g_string_append_printf (string,
                                  "#%04X%04X%04X",
                                  color->red,
                                  color->green,
                                  color->blue);
        }

      gconf_change_set_set_string (changeset, key, string->str);
      g_string_free (string, TRUE);
    }
  else
    g_printerr ("Unhandled value type %s of pspec %s\n", g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)), pspec->name);

cleanup:
  g_free (key);
}

static void
terminal_profile_save (TerminalProfile *profile)
{
  TerminalProfilePrivate *priv = profile->priv;
  GConfChangeSet *changeset;
  GSList *l;
  GError *error = NULL;

  priv->save_idle_id = 0;

  changeset = gconf_change_set_new ();

  for (l = priv->dirty_pspecs; l != NULL; l = l->next)
    {
      GParamSpec *pspec = (GParamSpec *) l->data;

      if (pspec->owner_type != TERMINAL_TYPE_PROFILE)
        continue;

      if ((pspec->flags & G_PARAM_WRITABLE) == 0)
        continue;

      terminal_profile_gconf_changeset_add (profile, changeset, pspec);
    }

  g_slist_free (priv->dirty_pspecs);
  priv->dirty_pspecs = NULL;

  if (!gconf_client_commit_change_set (priv->conf, changeset, TRUE, &error))
    {
      g_message ("Failed to commit the changeset to gconf: %s", error->message);
      g_error_free (error);
    }

  gconf_change_set_unref (changeset);
}

static gboolean
terminal_profile_save_idle_cb (TerminalProfile *profile)
{
  terminal_profile_save (profile);

  /* don't run again */
  return FALSE; 
}

static void
terminal_profile_schedule_save (TerminalProfile *profile,
                                GParamSpec *pspec)
{
  TerminalProfilePrivate *priv = profile->priv;

  g_assert (pspec != NULL);

  if (!g_slist_find (priv->dirty_pspecs, pspec))
    priv->dirty_pspecs = g_slist_prepend (priv->dirty_pspecs, pspec);

  if (priv->save_idle_id != 0)
    return;

  priv->save_idle_id = g_idle_add ((GSourceFunc) terminal_profile_save_idle_cb, profile);
}

static void
terminal_profile_init (TerminalProfile *profile)
{
  TerminalProfilePrivate *priv;
  GObjectClass *object_class;
  GParamSpec **pspecs;
  guint n_pspecs, i;

  priv = profile->priv = G_TYPE_INSTANCE_GET_PRIVATE (profile, TERMINAL_TYPE_PROFILE, TerminalProfilePrivate);

  priv->gconf_notification_pspec = NULL;

  priv->conf = gconf_client_get_default ();

  priv->locked = g_new0 (gboolean, LAST_PROP);

  priv->properties = g_value_array_new (LAST_PROP);
  for (i = 0; i < LAST_PROP; ++i)
    g_value_array_append (priv->properties, NULL);

  pspecs = g_object_class_list_properties (G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (profile)), &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue *value;

      if (pspec->owner_type != TERMINAL_TYPE_PROFILE)
        continue;

      g_assert (pspec->param_id < LAST_PROP);
      value = g_value_array_get_nth (priv->properties, pspec->param_id);
      g_value_init (value, pspec->value_type);
      g_param_value_set_default (pspec, value);
    }

  g_free (pspecs);

  /* A few properties don't have defaults via the param spec; set them explicitly */
  object_class = G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (profile));
  terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_FOREGROUND_COLOR), FALSE);
  terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_BOLD_COLOR), FALSE);
  terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_BACKGROUND_COLOR), FALSE);
  terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_ENCODING), FALSE);
  terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_FONT), FALSE);
  terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_PALETTE), FALSE);
}

static GObject *
terminal_profile_constructor (GType type,
                              guint n_construct_properties,
                              GObjectConstructParam *construct_params)
{
  GObject *object;
  TerminalProfile *profile;
  TerminalProfilePrivate *priv;
  const char *name;
  GParamSpec **pspecs;
  guint n_pspecs, i;

  object = G_OBJECT_CLASS (terminal_profile_parent_class)->constructor
            (type, n_construct_properties, construct_params);

  profile = TERMINAL_PROFILE (object);
  priv = profile->priv;

  name = g_value_get_string (g_value_array_get_nth (priv->properties, PROP_NAME));
  g_assert (name != NULL);

  /* Now load those properties from gconf that were not set as construction params */
  pspecs = g_object_class_list_properties (G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (profile)), &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      guint j;
      gboolean is_construct = FALSE;
      const char *gconf_key;
      char *key;

      if (pspec->owner_type != TERMINAL_TYPE_PROFILE)
        continue;

      if ((pspec->flags & G_PARAM_WRITABLE) == 0 ||
          (pspec->flags & G_PARAM_CONSTRUCT_ONLY) != 0)
        continue;

      for (j = 0; j < n_construct_properties; ++j)
        if (pspec == construct_params[j].pspec)
          {
            is_construct = TRUE;
            break;
          }

      if (is_construct)
        continue;

      gconf_key = g_param_spec_get_qdata (pspec, gconf_key_quark);
      if (!gconf_key)
        continue;

      key = gconf_concat_dir_and_key (priv->profile_dir, gconf_key);
      gconf_client_notify (priv->conf, key);
      g_free (key);
    }

  g_free (pspecs);

  return object;
}

static void
terminal_profile_finalize (GObject *object)
{
  TerminalProfile *profile = TERMINAL_PROFILE (object);
  TerminalProfilePrivate *priv = profile->priv;

  gconf_client_notify_remove (priv->conf, priv->notify_id);
  priv->notify_id = 0;

  if (priv->save_idle_id)
    {
      g_source_remove (priv->save_idle_id);

      /* Save now */
      terminal_profile_save (profile); 
    }

  _terminal_profile_forget (profile);

  g_object_unref (priv->conf);

  g_free (priv->profile_dir);
  g_free (priv->locked);
  g_value_array_free (priv->properties);

  G_OBJECT_CLASS (terminal_profile_parent_class)->finalize (object);
}

static void
terminal_profile_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  TerminalProfile *profile = TERMINAL_PROFILE (object);
  TerminalProfilePrivate *priv = profile->priv;

  if (prop_id == 0 || prop_id >= LAST_PROP)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      return;
    }
    
  /* Note: When adding things here, do the same in get_prop_value_from_prop_name! */
  switch (prop_id)
    {
      case PROP_BACKGROUND_IMAGE:
        ensure_pixbuf_property (profile, PROP_BACKGROUND_IMAGE_FILE, PROP_BACKGROUND_IMAGE, &priv->background_load_failed);
        break;
      default:
        break;
    }

  g_value_copy (g_value_array_get_nth (priv->properties, prop_id), value);
}

static void
terminal_profile_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  TerminalProfile *profile = TERMINAL_PROFILE (object);
  TerminalProfilePrivate *priv = profile->priv;
  GValue *prop_value;

  if (prop_id == 0 || prop_id >= LAST_PROP)
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      return;
    }

  prop_value = g_value_array_get_nth (priv->properties, prop_id);

  /* Preprocessing */
  switch (prop_id)
    {
#if 0
      case PROP_FONT: {
        PangoFontDescription *font_desc, *new_font_desc;

        font_desc = g_value_get_boxed (prop_value);
        new_font_desc = g_value_get_boxed (value);

        if (font_desc && new_font_desc)
          {
            /* Merge in case the new string isn't complete enough to load a font */
            pango_font_description_merge (font_desc, new_font_desc, TRUE);
            pango_font_description_free (new_font_desc);
            break;
          }

        /* fall-through */
      }
#endif
      default:
        g_value_copy (value, prop_value);
        break;
    }

  /* Postprocessing */
  switch (prop_id)
    {
      case PROP_NAME: {
        const char *name = g_value_get_string (value);

        g_assert (name != NULL);
        priv->profile_dir = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX, name);

        gconf_client_add_dir (priv->conf, priv->profile_dir,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);
        priv->notify_id =
          gconf_client_notify_add (priv->conf,
                                   priv->profile_dir,
                                   terminal_profile_gconf_notify_cb,
                                   profile, NULL,
                                   NULL);

        break;
      }

      case PROP_BACKGROUND_IMAGE_FILE:
        /* Clear the cached image */
        g_value_set_object (g_value_array_get_nth (priv->properties, PROP_BACKGROUND_IMAGE), NULL);
        priv->background_load_failed = FALSE;
        g_object_notify (object, TERMINAL_PROFILE_BACKGROUND_IMAGE);
        break;

      default:
        break;
    }
}

static void
terminal_profile_notify (GObject *object,
                         GParamSpec *pspec)
{
  TerminalProfilePrivate *priv = TERMINAL_PROFILE (object)->priv;
  void (* notify) (GObject *, GParamSpec *) = G_OBJECT_CLASS (terminal_profile_parent_class)->notify;

  _terminal_debug_print (TERMINAL_DEBUG_PROFILE,
                         "Property notification for prop %s\n",
                         pspec->name);

  if (notify)
    notify (object, pspec);

  if (pspec->owner_type == TERMINAL_TYPE_PROFILE &&
      (pspec->flags & G_PARAM_WRITABLE) &&
      g_param_spec_get_qdata (pspec, gconf_key_quark) != NULL &&
      pspec != priv->gconf_notification_pspec)
    terminal_profile_schedule_save (TERMINAL_PROFILE (object), pspec);
}

static void
terminal_profile_class_init (TerminalProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  gconf_key_quark = g_quark_from_static_string ("GT::GConfKey");

  g_type_class_add_private (object_class, sizeof (TerminalProfilePrivate));

  object_class->constructor = terminal_profile_constructor;
  object_class->finalize = terminal_profile_finalize;
  object_class->get_property = terminal_profile_get_property;
  object_class->set_property = terminal_profile_set_property;
  object_class->notify = terminal_profile_notify;

  signals[FORGOTTEN] =
    g_signal_new ("forgotten",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalProfileClass, forgotten),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* gconf_key -> pspec hash */
  klass->gconf_keys = g_hash_table_new (g_str_hash, g_str_equal);

#define TERMINAL_PROFILE_PSPEC_STATIC (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define TERMINAL_PROFILE_PROPERTY(propId, propSpec, propGConf) \
{\
  GParamSpec *pspec = propSpec;\
  g_object_class_install_property (object_class, propId, pspec);\
\
  if (propGConf)\
    {\
      g_param_spec_set_qdata (pspec, gconf_key_quark, (gpointer) propGConf);\
      g_hash_table_insert (klass->gconf_keys, (gpointer) propGConf, pspec);\
    }\
}

#define TERMINAL_PROFILE_PROPERTY_BOOLEAN(prop, propDefault, propGConf) \
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_boolean (TERMINAL_PROFILE_##prop, NULL, NULL,\
                          propDefault,\
                          G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_BOXED(prop, propType, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_boxed (TERMINAL_PROFILE_##prop, NULL, NULL,\
                        propType,\
                        G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_DOUBLE(prop, propMin, propMax, propDefault, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_double (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propMin, propMax, propDefault,\
                         G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_ENUM(prop, propType, propDefault, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_enum (TERMINAL_PROFILE_##prop, NULL, NULL,\
                       propType, propDefault,\
                       G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_INT(prop, propMin, propMax, propDefault, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_int (TERMINAL_PROFILE_##prop, NULL, NULL,\
                      propMin, propMax, propDefault,\
                      G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

/* these are all read-only */
#define TERMINAL_PROFILE_PROPERTY_OBJECT(prop, propType, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_object (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propType,\
                         G_PARAM_READABLE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_STRING(prop, propDefault, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_string (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propDefault,\
                         G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_STRING_CO(prop, propDefault, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_string (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propDefault,\
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

#define TERMINAL_PROFILE_PROPERTY_VALUE_ARRAY_BOXED(prop, propElementName, propElementType, propGConf)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_value_array (TERMINAL_PROFILE_##prop, NULL, NULL,\
                              g_param_spec_boxed (propElementName, NULL, NULL,\
                                                  propElementType, \
                                                  G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
                              G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGConf)

  TERMINAL_PROFILE_PROPERTY_BOOLEAN (ALLOW_BOLD, DEFAULT_ALLOW_BOLD, KEY_ALLOW_BOLD);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (BOLD_COLOR_SAME_AS_FG, DEFAULT_BOLD_COLOR_SAME_AS_FG, KEY_BOLD_COLOR_SAME_AS_FG);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (DEFAULT_SHOW_MENUBAR, DEFAULT_DEFAULT_SHOW_MENUBAR, KEY_DEFAULT_SHOW_MENUBAR);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (LOGIN_SHELL, DEFAULT_LOGIN_SHELL, KEY_LOGIN_SHELL);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLL_BACKGROUND, DEFAULT_SCROLL_BACKGROUND, KEY_SCROLL_BACKGROUND);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLLBACK_UNLIMITED, DEFAULT_SCROLLBACK_UNLIMITED, KEY_SCROLLBACK_UNLIMITED);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLL_ON_KEYSTROKE, DEFAULT_SCROLL_ON_KEYSTROKE, KEY_SCROLL_ON_KEYSTROKE);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLL_ON_OUTPUT, DEFAULT_SCROLL_ON_OUTPUT, KEY_SCROLL_ON_OUTPUT);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (SILENT_BELL, DEFAULT_SILENT_BELL, KEY_SILENT_BELL);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (UPDATE_RECORDS, DEFAULT_UPDATE_RECORDS, KEY_UPDATE_RECORDS);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_CUSTOM_COMMAND, DEFAULT_USE_CUSTOM_COMMAND, KEY_USE_CUSTOM_COMMAND);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_CUSTOM_DEFAULT_SIZE, DEFAULT_USE_CUSTOM_DEFAULT_SIZE, KEY_USE_CUSTOM_DEFAULT_SIZE);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_SYSTEM_FONT, DEFAULT_USE_SYSTEM_FONT, KEY_USE_SYSTEM_FONT);
  TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_THEME_COLORS, DEFAULT_USE_THEME_COLORS, KEY_USE_THEME_COLORS);

  TERMINAL_PROFILE_PROPERTY_BOXED (BACKGROUND_COLOR, GDK_TYPE_COLOR, KEY_BACKGROUND_COLOR);
  TERMINAL_PROFILE_PROPERTY_BOXED (BOLD_COLOR, GDK_TYPE_COLOR, KEY_BOLD_COLOR);
  TERMINAL_PROFILE_PROPERTY_BOXED (ENCODING, TERMINAL_TYPE_ENCODING, KEY_ENCODING);
  TERMINAL_PROFILE_PROPERTY_BOXED (FONT, PANGO_TYPE_FONT_DESCRIPTION, KEY_FONT);
  TERMINAL_PROFILE_PROPERTY_BOXED (FOREGROUND_COLOR, GDK_TYPE_COLOR, KEY_FOREGROUND_COLOR);

  /* 0.0 = normal bg, 1.0 = all black bg, 0.5 = half darkened */
  TERMINAL_PROFILE_PROPERTY_DOUBLE (BACKGROUND_DARKNESS, 0.0, 1.0, DEFAULT_BACKGROUND_DARKNESS, KEY_BACKGROUND_DARKNESS);

  TERMINAL_PROFILE_PROPERTY_ENUM (BACKGROUND_TYPE, TERMINAL_TYPE_BACKGROUND_TYPE, DEFAULT_BACKGROUND_TYPE, KEY_BACKGROUND_TYPE);
  TERMINAL_PROFILE_PROPERTY_ENUM (BACKSPACE_BINDING,  VTE_TYPE_TERMINAL_ERASE_BINDING, DEFAULT_BACKSPACE_BINDING, KEY_BACKSPACE_BINDING);
  TERMINAL_PROFILE_PROPERTY_ENUM (CURSOR_BLINK_MODE, VTE_TYPE_TERMINAL_CURSOR_BLINK_MODE, DEFAULT_CURSOR_BLINK_MODE, KEY_CURSOR_BLINK_MODE);
  TERMINAL_PROFILE_PROPERTY_ENUM (CURSOR_SHAPE, VTE_TYPE_TERMINAL_CURSOR_SHAPE, DEFAULT_CURSOR_SHAPE, KEY_CURSOR_SHAPE);
  TERMINAL_PROFILE_PROPERTY_ENUM (DELETE_BINDING, VTE_TYPE_TERMINAL_ERASE_BINDING, DEFAULT_DELETE_BINDING, KEY_DELETE_BINDING);
  TERMINAL_PROFILE_PROPERTY_ENUM (EXIT_ACTION, TERMINAL_TYPE_EXIT_ACTION, DEFAULT_EXIT_ACTION, KEY_EXIT_ACTION);
  TERMINAL_PROFILE_PROPERTY_ENUM (SCROLLBAR_POSITION, TERMINAL_TYPE_SCROLLBAR_POSITION, DEFAULT_SCROLLBAR_POSITION, KEY_SCROLLBAR_POSITION);
  TERMINAL_PROFILE_PROPERTY_ENUM (TITLE_MODE, TERMINAL_TYPE_TITLE_MODE, DEFAULT_TITLE_MODE, KEY_TITLE_MODE);

  TERMINAL_PROFILE_PROPERTY_INT (DEFAULT_SIZE_COLUMNS, 1, 1024, DEFAULT_DEFAULT_SIZE_COLUMNS, KEY_DEFAULT_SIZE_COLUMNS);
  TERMINAL_PROFILE_PROPERTY_INT (DEFAULT_SIZE_ROWS, 1, 1024, DEFAULT_DEFAULT_SIZE_ROWS, KEY_DEFAULT_SIZE_ROWS);
  TERMINAL_PROFILE_PROPERTY_INT (SCROLLBACK_LINES, 1, G_MAXINT, DEFAULT_SCROLLBACK_LINES, KEY_SCROLLBACK_LINES);

  TERMINAL_PROFILE_PROPERTY_OBJECT (BACKGROUND_IMAGE, GDK_TYPE_PIXBUF, NULL);

  TERMINAL_PROFILE_PROPERTY_STRING_CO (NAME, DEFAULT_NAME, NULL);
  TERMINAL_PROFILE_PROPERTY_STRING (BACKGROUND_IMAGE_FILE, DEFAULT_BACKGROUND_IMAGE_FILE, KEY_BACKGROUND_IMAGE_FILE);
  TERMINAL_PROFILE_PROPERTY_STRING (CUSTOM_COMMAND, DEFAULT_CUSTOM_COMMAND, KEY_CUSTOM_COMMAND);
  TERMINAL_PROFILE_PROPERTY_STRING (TITLE, _(DEFAULT_TITLE), KEY_TITLE);
  TERMINAL_PROFILE_PROPERTY_STRING (VISIBLE_NAME, _(DEFAULT_VISIBLE_NAME), KEY_VISIBLE_NAME);
  TERMINAL_PROFILE_PROPERTY_STRING (WORD_CHARS, DEFAULT_WORD_CHARS, KEY_WORD_CHARS);

  TERMINAL_PROFILE_PROPERTY_VALUE_ARRAY_BOXED (PALETTE, "palette-color", GDK_TYPE_COLOR, KEY_PALETTE);
}

/* Semi-Public API */

TerminalProfile*
_terminal_profile_new (const char *name)
{
  return g_object_new (TERMINAL_TYPE_PROFILE,
                       "name", name,
                       NULL);
}

void
_terminal_profile_forget (TerminalProfile *profile)
{
  TerminalProfilePrivate *priv = profile->priv;
  
  if (!priv->forgotten)
    {
      gconf_client_remove_dir (priv->conf,
                               priv->profile_dir,
                               NULL);

      priv->forgotten = TRUE;

      g_signal_emit (G_OBJECT (profile), signals[FORGOTTEN], 0);
    }
}

gboolean
_terminal_profile_get_forgotten (TerminalProfile *profile)
{
  return profile->priv->forgotten;
}

TerminalProfile *
_terminal_profile_clone (TerminalProfile *base_profile,
                         const char      *visible_name)
{
  TerminalApp *app = terminal_app_get ();
  GObject *base_object = G_OBJECT (base_profile);
  TerminalProfilePrivate *new_priv;
  char profile_name[32];
  GParameter *params;
  GParamSpec **pspecs;
  guint n_pspecs, i, n_params, profile_num;
  TerminalProfile *new_profile;

  g_object_ref (base_profile);

  profile_num = 0;
  do
    {
      g_snprintf (profile_name, sizeof (profile_name), "Profile%u", profile_num++);
    }
  while (terminal_app_get_profile_by_name (app, profile_name) != NULL);
 
  /* Now we have an unused profile name */
  pspecs = g_object_class_list_properties (G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (base_profile)), &n_pspecs);
  
  params = g_newa (GParameter, n_pspecs);
  n_params = 0;

  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue *value;

      if (pspec->owner_type != TERMINAL_TYPE_PROFILE ||
          (pspec->flags & G_PARAM_WRITABLE) == 0)
        continue;

      params[n_params].name = pspec->name;

      value = &params[n_params].value;
      G_VALUE_TYPE (value) = 0;
      g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

      if (pspec->name == I_(TERMINAL_PROFILE_NAME))
        g_value_set_static_string (value, profile_name);
      else if (pspec->name == I_(TERMINAL_PROFILE_VISIBLE_NAME))
        g_value_set_static_string (value, visible_name);
      else
        g_object_get_property (base_object, pspec->name, value);

      ++n_params;
    }

  new_profile = g_object_newv (TERMINAL_TYPE_PROFILE, n_params, params);

  g_object_unref (base_profile);

  for (i = 0; i < n_params; ++i)
    g_value_unset (&params[i].value);

  /* Flush the new profile to gconf */
  new_priv = new_profile->priv;

  g_slist_free (new_priv->dirty_pspecs);
  new_priv->dirty_pspecs = NULL;
  if (new_priv->save_idle_id != 0)
    {
      g_source_remove (new_priv->save_idle_id);
      new_priv->save_idle_id = 0;
    }

  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];

      if (pspec->owner_type != TERMINAL_TYPE_PROFILE ||
          (pspec->flags & G_PARAM_WRITABLE) == 0)
        continue;

      new_priv->dirty_pspecs = g_slist_prepend (new_priv->dirty_pspecs, pspec);
    }
  g_free (pspecs);

  terminal_profile_save (new_profile);

  return new_profile;
}

/* Public API */

gboolean
terminal_profile_get_property_boolean (TerminalProfile *profile,
                                       const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_BOOLEAN (value), FALSE);
  if (!value || !G_VALUE_HOLDS_BOOLEAN (value))
    return FALSE;

  return g_value_get_boolean (value);
}

gconstpointer
terminal_profile_get_property_boxed (TerminalProfile *profile,
                                     const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_BOXED (value), NULL);
  if (!value || !G_VALUE_HOLDS_BOXED (value))
    return NULL;

  return g_value_get_boxed (value);
}

double
terminal_profile_get_property_double (TerminalProfile *profile,
                                      const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_DOUBLE (value), 0.0);
  if (!value || !G_VALUE_HOLDS_DOUBLE (value))
    return 0.0;

  return g_value_get_double (value);
}

int
terminal_profile_get_property_enum (TerminalProfile *profile,
                                    const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_ENUM (value), 0);
  if (!value || !G_VALUE_HOLDS_ENUM (value))
    return 0;

  return g_value_get_enum (value);
}

int
terminal_profile_get_property_int (TerminalProfile *profile,
                                   const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_INT (value), 0);
  if (!value || !G_VALUE_HOLDS_INT (value))
    return 0;

  return g_value_get_int (value);
}

gpointer
terminal_profile_get_property_object (TerminalProfile *profile,
                                      const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_OBJECT (value), NULL);
  if (!value || !G_VALUE_HOLDS_OBJECT (value))
    return NULL;

  return g_value_get_object (value);
}

const char*
terminal_profile_get_property_string (TerminalProfile *profile,
                                      const char *prop_name)
{
  const GValue *value;

  value = get_prop_value_from_prop_name (profile, prop_name);
  g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_STRING (value), NULL);
  if (!value || !G_VALUE_HOLDS_STRING (value))
    return NULL;

  return g_value_get_string (value);
}

gboolean
terminal_profile_property_locked (TerminalProfile *profile,
                                  const char *prop_name)
{
  TerminalProfilePrivate *priv = profile->priv;
  GParamSpec *pspec;

  pspec = get_pspec_from_name (profile, prop_name);
  g_return_val_if_fail (pspec != NULL, FALSE);
  if (!pspec)
    return FALSE;

  return priv->locked[pspec->param_id];
}

void
terminal_profile_reset_property (TerminalProfile *profile,
                                 const char *prop_name)
{
  GParamSpec *pspec;

  pspec = get_pspec_from_name (profile, prop_name);
  g_return_if_fail (pspec != NULL);
  if (!pspec ||
      (pspec->flags & G_PARAM_WRITABLE) == 0)
    return;

  terminal_profile_reset_property_internal (profile, pspec, TRUE);
}

gboolean
terminal_profile_get_palette (TerminalProfile *profile,
                              GdkColor *colors,
                              guint *n_colors)
{
  TerminalProfilePrivate *priv;
  GValueArray *array;
  guint i, n;

  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);
  g_return_val_if_fail (colors != NULL && n_colors != NULL, FALSE);

  priv = profile->priv;
  array = g_value_get_boxed (g_value_array_get_nth (priv->properties, PROP_PALETTE));
  if (!array)
    return FALSE;

  n = MIN (array->n_values, *n_colors);
  for (i = 0; i < n; ++i)
    {
      GdkColor *color = g_value_get_boxed (g_value_array_get_nth (array, i));
      if (!color)
        continue; /* shouldn't happen!! */

      colors[i] = *color;
    }

  *n_colors = n;
  return TRUE;
}

gboolean
terminal_profile_get_palette_is_builtin (TerminalProfile *profile,
                                         guint *n)
{
  GdkColor colors[TERMINAL_PALETTE_SIZE];
  guint n_colors;
  guint i;

  n_colors = G_N_ELEMENTS (colors);
  if (!terminal_profile_get_palette (profile, colors, &n_colors) ||
      n_colors != TERMINAL_PALETTE_SIZE)
    return FALSE;

  for (i = 0; i < TERMINAL_PALETTE_N_BUILTINS; ++i)
    if (palette_cmp (colors, terminal_palettes[i]))
      {
        *n = i;
        return TRUE;
      }

  return FALSE;
}

void
terminal_profile_set_palette_builtin (TerminalProfile *profile,
                                      guint n)
{
  GValue value = { 0, };

  g_return_if_fail (n < TERMINAL_PALETTE_N_BUILTINS);

  g_value_init (&value, G_TYPE_VALUE_ARRAY);
  set_value_from_palette (&value, terminal_palettes[n], TERMINAL_PALETTE_SIZE);
  g_object_set_property (G_OBJECT (profile), TERMINAL_PROFILE_PALETTE, &value);
  g_value_unset (&value);
}

gboolean
terminal_profile_modify_palette_entry (TerminalProfile *profile,
                                       guint            i,
                                       const GdkColor  *color)
{
  TerminalProfilePrivate *priv = profile->priv;
  GValueArray *array;
  GValue *value;
  GdkColor *old_color;

  array = g_value_get_boxed (g_value_array_get_nth (priv->properties, PROP_PALETTE));
  if (!array ||
      i >= array->n_values)
    return FALSE;

  value = g_value_array_get_nth (array, i);
  old_color = g_value_get_boxed (value);
  if (!old_color ||
      !gdk_color_equal (old_color, color))
    {
      g_value_set_boxed (value, color);
      g_object_notify (G_OBJECT (profile), TERMINAL_PROFILE_PALETTE);
    }

  return TRUE;
}
