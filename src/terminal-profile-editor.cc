/*
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>
#include <vte/vte.h>

#include "terminal-app.hh"
#include "terminal-color-row.hh"
#include "terminal-profile-editor.hh"
#include "terminal-preferences-list-item.hh"
#include "terminal-schemas.hh"
#include "terminal-screen.hh"
#include "terminal-util.hh"
#include "terminal-libgsystem.hh"

#define COLOR(r, g, b) { .red = (r) / 255.0, .green = (g) / 255.0, .blue = (b) / 255.0, .alpha = 1.0 }
#define PALETTE_SIZE (16)

struct _TerminalProfileEditor
{
  AdwNavigationPage     parent_instance;

  AdwComboRow          *allow_blinking_text;
  AdwComboRow          *ambiguous_width;
  AdwSwitchRow         *audible_bell;
  AdwComboRow          *backspace_key;
  TerminalColorRow     *bold_color_set;
  TerminalColorRow     *bold_color_text;
  AdwSpinRow           *cell_height;
  AdwSpinRow           *cell_width;
  AdwComboRow          *color_palette;
  AdwComboRow          *color_schemes;
  AdwSpinRow           *columns;
  AdwComboRow          *cursor_blink;
  TerminalColorRow     *cursor_color_text;
  TerminalColorRow     *cursor_color_background;
  GtkSwitch            *cursor_colors_set;
  AdwComboRow          *cursor_shape;
  AdwEntryRow          *custom_command;
  AdwActionRow         *custom_font;
  GtkLabel             *custom_font_label;
  TerminalColorRow     *default_color_text;
  TerminalColorRow     *default_color_background;
  AdwComboRow          *delete_key;
  AdwSwitchRow         *enable_sixel;
  AdwComboRow          *encoding;
  TerminalColorRow     *highlight_color_text;
  TerminalColorRow     *highlight_color_background;
  GtkSwitch            *highlight_colors_set;
  AdwPreferencesGroup  *image_group;
  AdwSwitchRow         *limit_scrollback;
  AdwSwitchRow         *kinetic_scrolling;
  AdwSwitchRow         *login_shell;
  GtkColorDialogButton *palette_0;
  GtkColorDialogButton *palette_1;
  GtkColorDialogButton *palette_2;
  GtkColorDialogButton *palette_3;
  GtkColorDialogButton *palette_4;
  GtkColorDialogButton *palette_5;
  GtkColorDialogButton *palette_6;
  GtkColorDialogButton *palette_7;
  GtkColorDialogButton *palette_8;
  GtkColorDialogButton *palette_9;
  GtkColorDialogButton *palette_10;
  GtkColorDialogButton *palette_11;
  GtkColorDialogButton *palette_12;
  GtkColorDialogButton *palette_13;
  GtkColorDialogButton *palette_14;
  GtkColorDialogButton *palette_15;
  AdwComboRow          *preserve_working_directory;
  AdwSpinRow           *rows;
  AdwSwitchRow         *scroll_on_insert;
  AdwSwitchRow         *scroll_on_keystroke;
  AdwSwitchRow         *scroll_on_output;
  AdwSpinRow           *scrollback_lines;
  AdwSwitchRow         *show_bold_in_bright;
  AdwComboRow          *show_scrollbar;
  AdwEntryRow          *title;
  AdwSwitchRow         *use_custom_command;
  AdwSwitchRow         *use_system_colors;
  AdwSwitchRow         *use_system_font;
  GtkLabel             *uuid;
  AdwEntryRow          *visible_name;
  AdwComboRow          *when_command_exits;

  GSettings            *settings;
};

typedef struct _TerminalColorScheme
{
  const char *name;
  const GdkRGBA foreground;
  const GdkRGBA background;
} TerminalColorScheme;

enum {
  PROP_0,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalProfileEditor, terminal_profile_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties [N_PROPS];

static const TerminalColorScheme color_schemes[] = {
  { N_("Black on light yellow"),
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xff, 0xff, 0xdd)
  },
  { N_("Black on white"),
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xff, 0xff, 0xff)
  },
  { N_("Gray on black"),
    COLOR (0xaa, 0xaa, 0xaa),
    COLOR (0x00, 0x00, 0x00)
  },
  { N_("Green on black"),
    COLOR (0x00, 0xff, 0x00),
    COLOR (0x00, 0x00, 0x00)
  },
  { N_("White on black"),
    COLOR (0xff, 0xff, 0xff),
    COLOR (0x00, 0x00, 0x00)
  },
  /* Translators: "GNOME" is the name of a colour scheme, "light" can be translated */
  { N_("GNOME light"),
    COLOR (0x1e, 0x1e, 0x1e), /* Palette entry 0 */
    COLOR (0xff, 0xff, 0xff)  /* Palette entry 15 */
  },
  /* Translators: "GNOME" is the name of a colour scheme, "dark" can be translated */
  { N_("GNOME dark"),
    COLOR (0xcf, 0xcf, 0xcf), /* Palette entry 7 */
    COLOR (0x1e, 0x1e, 0x1e)  /* Palette entry 0 */
  },
  /* Translators: "Tango" is the name of a colour scheme, "light" can be translated */
  { N_("Tango light"),
    COLOR (0x2e, 0x34, 0x36),
    COLOR (0xee, 0xee, 0xec)
  },
  /* Translators: "Tango" is the name of a colour scheme, "dark" can be translated */
  { N_("Tango dark"),
    COLOR (0xd3, 0xd7, 0xcf),
    COLOR (0x2e, 0x34, 0x36)
  },
  /* Translators: "Solarized" is the name of a colour scheme, "light" can be translated */
  { N_("Solarized light"),
    COLOR (0x65, 0x7b, 0x83),  /* 11: base00 */
    COLOR (0xfd, 0xf6, 0xe3)   /* 15: base3  */
  },
  /* Translators: "Solarized" is the name of a colour scheme, "dark" can be translated */
  { N_("Solarized dark"),
    COLOR (0x83, 0x94, 0x96),  /* 12: base0  */
    COLOR (0x00, 0x2b, 0x36)   /*  8: base03 */
  },
};

enum
{
  TERMINAL_PALETTE_GNOME     = 0,
  TERMINAL_PALETTE_TANGO     = 1,
  TERMINAL_PALETTE_LINUX     = 2,
  TERMINAL_PALETTE_XTERM     = 3,
  TERMINAL_PALETTE_RXVT      = 4,
  TERMINAL_PALETTE_SOLARIZED = 5,
  TERMINAL_PALETTE_N_BUILTINS
};

static const char * const terminal_palette_names[] = {
  "GNOME",
  "Tango",
  "Linux Console",
  "XTerm",
  "Rxvt",
  "Solarized",
  nullptr,
};

static const GdkRGBA terminal_palettes[TERMINAL_PALETTE_N_BUILTINS][PALETTE_SIZE] =
{
  /* Based on GNOME 3.32 palette: https://developer.gnome.org/hig/stable/icon-design.html.en#palette */
  {
    COLOR (0x1e, 0x1e, 0x1e),  /* Suggested background for contrast: libadwaita dark mode @view_bg_color */
    COLOR (0xc0, 0x1c, 0x28),  /* Red 4 */
    COLOR (0x26, 0xa2, 0x69),  /* Green 5 */
    COLOR (0xa2, 0x73, 0x4c),  /* Blend of Brown 2 and Brown 3 */
    COLOR (0x12, 0x48, 0x8b),  /* Blend of Blue 5 and Dark 4 */
    COLOR (0xa3, 0x47, 0xba),  /* Purple 3 */
    COLOR (0x2a, 0xa1, 0xb3),  /* Linear addition Blue 5 + Green 5, darkened slightly */
    COLOR (0xcf, 0xcf, 0xcf),  /* Blend of Light 3 and Light 4, desaturated */
    COLOR (0x5d, 0x5d, 0x5d),  /* Dark 2, desaturated */
    COLOR (0xf6, 0x61, 0x51),  /* Red 1 */
    COLOR (0x33, 0xd1, 0x7a),  /* Green 3 */
    COLOR (0xe9, 0xad, 0x0c),  /* Blend of Yellow 4 and Yellow 5 */
    COLOR (0x2a, 0x7b, 0xde),  /* Blend of Blue 3 and Blue 4 */
    COLOR (0xc0, 0x61, 0xcb),  /* Purple 2 */
    COLOR (0x33, 0xc7, 0xde),  /* Linear addition Blue 4 + Green 4, darkened slightly */
    COLOR (0xff, 0xff, 0xff)   /* Light 1 */
  },

  /* Tango palette */
  {
    COLOR (0x2e, 0x34, 0x36),
    COLOR (0xcc, 0x00, 0x00),
    COLOR (0x4e, 0x9a, 0x06),
    COLOR (0xc4, 0xa0, 0x00),
    COLOR (0x34, 0x65, 0xa4),
    COLOR (0x75, 0x50, 0x7b),
    COLOR (0x06, 0x98, 0x9a),
    COLOR (0xd3, 0xd7, 0xcf),
    COLOR (0x55, 0x57, 0x53),
    COLOR (0xef, 0x29, 0x29),
    COLOR (0x8a, 0xe2, 0x34),
    COLOR (0xfc, 0xe9, 0x4f),
    COLOR (0x72, 0x9f, 0xcf),
    COLOR (0xad, 0x7f, 0xa8),
    COLOR (0x34, 0xe2, 0xe2),
    COLOR (0xee, 0xee, 0xec)
  },

  /* Linux palette */
  {
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xaa, 0x00, 0x00),
    COLOR (0x00, 0xaa, 0x00),
    COLOR (0xaa, 0x55, 0x00),
    COLOR (0x00, 0x00, 0xaa),
    COLOR (0xaa, 0x00, 0xaa),
    COLOR (0x00, 0xaa, 0xaa),
    COLOR (0xaa, 0xaa, 0xaa),
    COLOR (0x55, 0x55, 0x55),
    COLOR (0xff, 0x55, 0x55),
    COLOR (0x55, 0xff, 0x55),
    COLOR (0xff, 0xff, 0x55),
    COLOR (0x55, 0x55, 0xff),
    COLOR (0xff, 0x55, 0xff),
    COLOR (0x55, 0xff, 0xff),
    COLOR (0xff, 0xff, 0xff)
  },

  /* XTerm palette */
  {
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xcd, 0x00, 0x00),
    COLOR (0x00, 0xcd, 0x00),
    COLOR (0xcd, 0xcd, 0x00),
    COLOR (0x00, 0x00, 0xee),
    COLOR (0xcd, 0x00, 0xcd),
    COLOR (0x00, 0xcd, 0xcd),
    COLOR (0xe5, 0xe5, 0xe5),
    COLOR (0x7f, 0x7f, 0x7f),
    COLOR (0xff, 0x00, 0x00),
    COLOR (0x00, 0xff, 0x00),
    COLOR (0xff, 0xff, 0x00),
    COLOR (0x5c, 0x5c, 0xff),
    COLOR (0xff, 0x00, 0xff),
    COLOR (0x00, 0xff, 0xff),
    COLOR (0xff, 0xff, 0xff)
  },

  /* RXVT palette */
  {
    COLOR (0x00, 0x00, 0x00),
    COLOR (0xcd, 0x00, 0x00),
    COLOR (0x00, 0xcd, 0x00),
    COLOR (0xcd, 0xcd, 0x00),
    COLOR (0x00, 0x00, 0xcd),
    COLOR (0xcd, 0x00, 0xcd),
    COLOR (0x00, 0xcd, 0xcd),
    COLOR (0xfa, 0xeb, 0xd7),
    COLOR (0x40, 0x40, 0x40),
    COLOR (0xff, 0x00, 0x00),
    COLOR (0x00, 0xff, 0x00),
    COLOR (0xff, 0xff, 0x00),
    COLOR (0x00, 0x00, 0xff),
    COLOR (0xff, 0x00, 0xff),
    COLOR (0x00, 0xff, 0xff),
    COLOR (0xff, 0xff, 0xff)
  },

  /* Solarized palette (1.0.0beta2): http://ethanschoonover.com/solarized */
  {
    COLOR (0x07, 0x36, 0x42),  /*  0: base02  */
    COLOR (0xdc, 0x32, 0x2f),  /*  1: red     */
    COLOR (0x85, 0x99, 0x00),  /*  2: green   */
    COLOR (0xb5, 0x89, 0x00),  /*  3: yellow  */
    COLOR (0x26, 0x8b, 0xd2),  /*  4: blue    */
    COLOR (0xd3, 0x36, 0x82),  /*  5: magenta */
    COLOR (0x2a, 0xa1, 0x98),  /*  6: cyan    */
    COLOR (0xee, 0xe8, 0xd5),  /*  7: base2   */
    COLOR (0x00, 0x2b, 0x36),  /*  8: base03  */
    COLOR (0xcb, 0x4b, 0x16),  /*  9: orange  */
    COLOR (0x58, 0x6e, 0x75),  /* 10: base01  */
    COLOR (0x65, 0x7b, 0x83),  /* 11: base00  */
    COLOR (0x83, 0x94, 0x96),  /* 12: base0   */
    COLOR (0x6c, 0x71, 0xc4),  /* 13: violet  */
    COLOR (0x93, 0xa1, 0xa1),  /* 14: base1   */
    COLOR (0xfd, 0xf6, 0xe3)   /* 15: base3   */
  },
};

#undef COLOR

static GListModel *
create_color_scheme_model (void)
{
  g_autoptr(GArray) ar = g_array_new (TRUE, FALSE, sizeof (char *));
  const char *custom = _("Custom");

  for (guint i = 0; i < G_N_ELEMENTS (color_schemes); i++)
    g_array_append_val (ar, color_schemes[i].name);
  g_array_append_val (ar, custom);

  return G_LIST_MODEL (gtk_string_list_new ((const char * const *)(gpointer)ar->data));
}

static GListModel *
create_color_palette_model (void)
{
  g_autoptr(GArray) ar = g_array_new (TRUE, FALSE, sizeof (char *));
  const char *custom = _("Custom");

  for (guint i = 0; i < TERMINAL_PALETTE_N_BUILTINS; i++)
    g_array_append_val (ar, terminal_palette_names[i]);
  g_array_append_val (ar, custom);

  return G_LIST_MODEL (gtk_string_list_new ((const char * const *)(gpointer)ar->data));
}

typedef enum {
  GROUP_UTF8,
  GROUP_CJKV,
  GROUP_OBSOLETE,
  LAST_GROUP
} EncodingGroup;

typedef struct {
  const char *charset;
  const char *name;
  EncodingGroup group;
} EncodingEntry;

/* These MUST be sorted by charset so that bsearch can work! */
static const EncodingEntry encodings[] = {
  { "BIG5",           N_("Chinese Traditional"), GROUP_CJKV },
  { "BIG5-HKSCS",     N_("Chinese Traditional"), GROUP_CJKV },
  { "CP866",          N_("Cyrillic/Russian"),    GROUP_OBSOLETE },
  { "EUC-JP",         N_("Japanese"),            GROUP_CJKV },
  { "EUC-KR",         N_("Korean"),              GROUP_CJKV },
  { "EUC-TW",         N_("Chinese Traditional"), GROUP_CJKV },
  { "GB18030",        N_("Chinese Simplified"),  GROUP_CJKV },
  { "GB2312",         N_("Chinese Simplified"),  GROUP_CJKV },
  { "GBK",            N_("Chinese Simplified"),  GROUP_CJKV },
  { "IBM850",         N_("Western"),             GROUP_OBSOLETE },
  { "IBM852",         N_("Central European"),    GROUP_OBSOLETE },
  { "IBM855",         N_("Cyrillic"),            GROUP_OBSOLETE },
  { "IBM857",         N_("Turkish"),             GROUP_OBSOLETE },
  { "IBM862",         N_("Hebrew"),              GROUP_OBSOLETE },
  { "IBM864",         N_("Arabic"),              GROUP_OBSOLETE },
  { "ISO-8859-1",     N_("Western"),             GROUP_OBSOLETE },
  { "ISO-8859-10",    N_("Nordic"),              GROUP_OBSOLETE },
  { "ISO-8859-13",    N_("Baltic"),              GROUP_OBSOLETE },
  { "ISO-8859-14",    N_("Celtic"),              GROUP_OBSOLETE },
  { "ISO-8859-15",    N_("Western"),             GROUP_OBSOLETE },
  { "ISO-8859-16",    N_("Romanian"),            GROUP_OBSOLETE },
  { "ISO-8859-2",     N_("Central European"),    GROUP_OBSOLETE },
  { "ISO-8859-3",     N_("South European"),      GROUP_OBSOLETE },
  { "ISO-8859-4",     N_("Baltic"),              GROUP_OBSOLETE },
  { "ISO-8859-5",     N_("Cyrillic"),            GROUP_OBSOLETE },
  { "ISO-8859-6",     N_("Arabic"),              GROUP_OBSOLETE },
  { "ISO-8859-7",     N_("Greek"),               GROUP_OBSOLETE },
  { "ISO-8859-8",     N_("Hebrew Visual"),       GROUP_OBSOLETE },
  { "ISO-8859-8-I",   N_("Hebrew"),              GROUP_OBSOLETE },
  { "ISO-8859-9",     N_("Turkish"),             GROUP_OBSOLETE },
  { "KOI8-R",         N_("Cyrillic"),            GROUP_OBSOLETE },
  { "KOI8-U",         N_("Cyrillic/Ukrainian"),  GROUP_OBSOLETE },
  { "MAC-CYRILLIC",   N_("Cyrillic"),            GROUP_OBSOLETE },
  { "MAC_ARABIC",     N_("Arabic"),              GROUP_OBSOLETE },
  { "MAC_CE",         N_("Central European"),    GROUP_OBSOLETE },
  { "MAC_CROATIAN",   N_("Croatian"),            GROUP_OBSOLETE },
  { "MAC_GREEK",      N_("Greek"),               GROUP_OBSOLETE },
  { "MAC_HEBREW",     N_("Hebrew"),              GROUP_OBSOLETE },
  { "MAC_ROMAN",      N_("Western"),             GROUP_OBSOLETE },
  { "MAC_ROMANIAN",   N_("Romanian"),            GROUP_OBSOLETE },
  { "MAC_TURKISH",    N_("Turkish"),             GROUP_OBSOLETE },
  { "MAC_UKRAINIAN",  N_("Cyrillic/Ukrainian"),  GROUP_OBSOLETE },
  { "SHIFT_JIS",      N_("Japanese"),            GROUP_CJKV },
  { "TIS-620",        N_("Thai"),                GROUP_OBSOLETE },
  { "UHC",            N_("Korean"),              GROUP_CJKV },
  { "UTF-8",          N_("Unicode"),             GROUP_UTF8 },
  { "WINDOWS-1250",   N_("Central European"),    GROUP_OBSOLETE },
  { "WINDOWS-1251",   N_("Cyrillic"),            GROUP_OBSOLETE },
  { "WINDOWS-1252",   N_("Western"),             GROUP_OBSOLETE },
  { "WINDOWS-1253",   N_("Greek"),               GROUP_OBSOLETE },
  { "WINDOWS-1254",   N_("Turkish"),             GROUP_OBSOLETE },
  { "WINDOWS-1255",   N_("Hebrew"),              GROUP_OBSOLETE},
  { "WINDOWS-1256",   N_("Arabic"),              GROUP_OBSOLETE },
  { "WINDOWS-1257",   N_("Baltic"),              GROUP_OBSOLETE },
  { "WINDOWS-1258",   N_("Vietnamese"),          GROUP_OBSOLETE },
};

static const struct {
  EncodingGroup group;
  const char *name;
} encodings_group_names[] = {
  { GROUP_UTF8,     N_("Unicode") },
  { GROUP_CJKV,     N_("Legacy CJK Encodings") },
  { GROUP_OBSOLETE, N_("Obsolete Encodings") },
};

static GListModel *
create_encodings_model (void)
{
  GListStore *model = g_list_store_new (TERMINAL_TYPE_PREFERENCES_LIST_ITEM);

  for (guint i = 0; i < G_N_ELEMENTS (encodings_group_names); i++) {
    for (guint j = 0; j < G_N_ELEMENTS (encodings); j++) {
      if (encodings[j].group == encodings_group_names[i].group) {
        g_autofree char *title = g_strdup_printf ("%s (%s)",
                                                  encodings[j].name,
                                                  encodings[j].charset);
        g_autoptr(GVariant) value = g_variant_ref_sink (g_variant_new_string (encodings[j].charset));
        g_autoptr(TerminalPreferencesListItem) item =
          (TerminalPreferencesListItem*)g_object_new (TERMINAL_TYPE_PREFERENCES_LIST_ITEM,
                                                      "title", title,
                                                      "value", value,
                                                      nullptr);

        g_list_store_append (model, G_OBJECT (item));
      }
    }
  }

  return G_LIST_MODEL (model);
}

static void
recurse_remove_emoji_hint (GtkWidget *widget)
{
  if (GTK_IS_TEXT (widget)) {
    gtk_text_set_input_hints (GTK_TEXT (widget), GTK_INPUT_HINT_NO_EMOJI);
    return;
  }

  for (auto child = gtk_widget_get_first_child (widget);
       child != nullptr;
       child = gtk_widget_get_next_sibling (child)) {
    recurse_remove_emoji_hint (child);
  }
}

static void
terminal_profile_editor_copy_uuid (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  auto const self = TERMINAL_PROFILE_EDITOR (widget);
  auto const app = terminal_app_get();
  auto const profiles = terminal_app_get_profiles_list(app);
  g_autofree auto uuid = terminal_settings_list_dup_uuid_from_child(profiles, self->settings);
  if (!uuid)
    return;

  gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), uuid);
}

static void
terminal_profile_editor_reset_size (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (widget);

  g_settings_reset (self->settings, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY);
  g_settings_reset (self->settings, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY);
  g_settings_reset (self->settings, TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY);
  g_settings_reset (self->settings, TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY);
}

static void
terminal_profile_editor_reset_compatibility (GtkWidget  *widget,
                                             const char *action_name,
                                             GVariant   *param)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (widget);

  g_settings_reset (self->settings, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY);
  g_settings_reset (self->settings, TERMINAL_PROFILE_ENCODING_KEY);
  g_settings_reset (self->settings, TERMINAL_PROFILE_DELETE_BINDING_KEY);
  g_settings_reset (self->settings, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY);
}

static void
terminal_profile_editor_scheme_changed_from_selection (TerminalProfileEditor *self,
                                                       GParamSpec            *pspec,
                                                       AdwComboRow           *row)
{
  GtkStringObject *item;
  const char *name;

  g_assert (TERMINAL_IS_PROFILE_EDITOR (self));
  g_assert (ADW_IS_COMBO_ROW (row));

  item = GTK_STRING_OBJECT (adw_combo_row_get_selected_item (row));
  if (item == nullptr)
    return;

  name = gtk_string_object_get_string (item);

  for (guint i = 0; i < G_N_ELEMENTS (color_schemes); i++) {
    if (strcmp (name, color_schemes[i].name) == 0) {
      terminal_g_settings_set_rgba (self->settings,
                                    TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                                    &color_schemes[i].foreground);
      terminal_g_settings_set_rgba (self->settings,
                                    TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                    &color_schemes[i].background);
      break;
    }
  }
}

static void
terminal_profile_editor_scheme_changed_from_settings (TerminalProfileEditor *self,
                                                      const char            *key,
                                                      GSettings             *settings)
{
  GdkRGBA foreground;
  GdkRGBA background;

  g_assert (TERMINAL_IS_PROFILE_EDITOR (self));
  g_assert (G_IS_SETTINGS (settings));

  terminal_g_settings_get_rgba (self->settings,
                                TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                                &foreground);
  terminal_g_settings_get_rgba (self->settings,
                                TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                &background);

  for (guint i = 0; i < G_N_ELEMENTS (color_schemes); i++) {
    if (gdk_rgba_equal (&foreground, &color_schemes[i].foreground) &&
        gdk_rgba_equal (&background, &color_schemes[i].background)) {
      adw_combo_row_set_selected (self->color_schemes, i);
      break;
    }
  }
}

static void
terminal_profile_editor_palette_index_changed (TerminalProfileEditor *self,
                                               GParamSpec            *pspec,
                                               GtkColorDialogButton  *button)
{
  g_auto(GStrv) palette = nullptr;
  GdkRGBA color;
  const char *child_name;
  guint pos;

  g_assert (TERMINAL_IS_PROFILE_EDITOR (self));
  g_assert (pspec != nullptr);
  g_assert (g_str_equal (pspec->name, "rgba"));
  g_assert (GTK_IS_COLOR_BUTTON (button));

  child_name = gtk_buildable_get_buildable_id (GTK_BUILDABLE (button));
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);

  g_assert (child_name != nullptr);
  g_assert (g_str_has_prefix (child_name, "palette_"));

  if (1 != sscanf (child_name, "palette_%u", &pos) || pos >= PALETTE_SIZE)
    return;

  /* If we come across an invalid palette, just reset the key and
   * let the user try again.
   */
  palette = g_settings_get_strv (self->settings, TERMINAL_PROFILE_PALETTE_KEY);
  if (g_strv_length (palette) != PALETTE_SIZE) {
    g_settings_reset (self->settings, TERMINAL_PROFILE_PALETTE_KEY);
    return;
  }

  g_free (palette[pos]);
  palette[pos] = gdk_rgba_to_string (&color);
  g_settings_set_strv (self->settings, TERMINAL_PROFILE_PALETTE_KEY, palette);
}

static void
terminal_profile_editor_palette_changed_from_selection (TerminalProfileEditor *self,
                                                        GParamSpec            *pspec,
                                                        AdwComboRow           *row)
{
  GtkStringObject *item;
  const char *name;

  g_assert (TERMINAL_IS_PROFILE_EDITOR (self));
  g_assert (ADW_IS_COMBO_ROW (row));

  item = GTK_STRING_OBJECT (adw_combo_row_get_selected_item (row));
  if (item == nullptr)
    return;

  name = gtk_string_object_get_string (item);

  for (guint i = 0; i < TERMINAL_PALETTE_N_BUILTINS; i++) {
    if (strcmp (name, terminal_palette_names[i]) == 0) {
      terminal_g_settings_set_rgba_palette (self->settings,
                                            TERMINAL_PROFILE_PALETTE_KEY,
                                            terminal_palettes[i],
                                            PALETTE_SIZE);
      break;
    }
  }
}

static void
terminal_profile_editor_palette_changed_from_settings (TerminalProfileEditor *self,
                                                       const char            *key,
                                                       GSettings             *settings)
{
  g_auto(GStrv) palette = nullptr;
  GdkRGBA colors[PALETTE_SIZE];

  g_assert (TERMINAL_IS_PROFILE_EDITOR (self));
  g_assert (g_str_equal (key, TERMINAL_PROFILE_PALETTE_KEY));
  g_assert (G_IS_SETTINGS (settings));

  palette = g_settings_get_strv (settings, key);

  if (g_strv_length (palette) != PALETTE_SIZE) {
    g_warning_once ("Palette must contain exactly 16 colors");
    return;
  }

  for (guint i = 0; i < PALETTE_SIZE; i++) {
    GObject *button;
    char child_name[32];

    if (!gdk_rgba_parse (&colors[i], palette[i])) {
      g_warning ("'%s' cannot be parsed into a color", palette[i]);
      return;
    }

    g_snprintf (child_name, sizeof child_name, "palette_%u", i);
    button = gtk_widget_get_template_child (GTK_WIDGET (self),
                                            TERMINAL_TYPE_PROFILE_EDITOR,
                                            child_name);

    g_assert (button != nullptr);
    g_assert (GTK_IS_COLOR_BUTTON (button));

    g_signal_handlers_block_by_func (button, (gpointer)G_CALLBACK (terminal_profile_editor_palette_index_changed), self);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), &colors[i]);
    g_signal_handlers_unblock_by_func (button, (gpointer)G_CALLBACK (terminal_profile_editor_palette_index_changed), self);
  }

  for (guint i = 0; i < TERMINAL_PALETTE_N_BUILTINS; i++) {
    if (memcmp (colors, terminal_palettes[i], sizeof (GdkRGBA) * PALETTE_SIZE) == 0) {
      adw_combo_row_set_selected (self->color_palette, i);
      return;
    }
  }

  /* Set to "Custom" */
  adw_combo_row_set_selected (self->color_palette, TERMINAL_PALETTE_N_BUILTINS);
}

static gboolean
monospace_filter (void* item,
                  void* user_data)
{
  PangoFontFamily* family = nullptr;
  if (PANGO_IS_FONT_FAMILY(item)) {
    family = reinterpret_cast<PangoFontFamily*>(item);
  } else if (PANGO_IS_FONT_FACE(item)) {
    auto const face = reinterpret_cast<PangoFontFace*>(item);
    family = pango_font_face_get_family(face);
  }

  return family ? pango_font_family_is_monospace (family) : false;
}

static void
terminal_profile_editor_select_custom_font_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  GtkFontDialog *dialog = GTK_FONT_DIALOG (object);
  g_autoptr(TerminalProfileEditor) self = TERMINAL_PROFILE_EDITOR (user_data);
  g_autoptr(PangoFontDescription) font_desc = nullptr;
  g_autoptr(GError) error = nullptr;

  if ((font_desc = gtk_font_dialog_choose_font_finish (dialog, result, &error))) {
    g_autofree char *font_string = pango_font_description_to_string (font_desc);

    if (font_string != nullptr)
      g_settings_set (self->settings, TERMINAL_PROFILE_FONT_KEY, "s", font_string);
  }
}

static void
terminal_profile_editor_select_custon_font (GtkWidget  *widget,
                                            const char *action_name,
                                            GVariant   *param)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (widget);
  g_autoptr(PangoFontDescription) font_desc = nullptr;
  g_autoptr(GtkFontDialog) dialog = nullptr;
  g_autofree char *font_string = nullptr;

  if ((font_string = g_settings_get_string (self->settings, TERMINAL_PROFILE_FONT_KEY)))
    font_desc = pango_font_description_from_string (font_string);


  g_autoptr(GtkCustomFilter) filter = gtk_custom_filter_new(GtkCustomFilterFunc(monospace_filter),
                                                            nullptr, nullptr);

  dialog = (GtkFontDialog *)g_object_new (GTK_TYPE_FONT_DIALOG,
                                          "title", _("Select Font"),
                                          "filter", filter,
                                          nullptr);

  gtk_font_dialog_choose_font (dialog,
                               GTK_WINDOW (gtk_widget_get_root (widget)),
                               font_desc,
                               nullptr,
                               terminal_profile_editor_select_custom_font_cb,
                               g_object_ref (self));
}

static gboolean
string_to_rgba (GValue   *value,
                GVariant *variant,
                gpointer  user_data)
{
  const char *str = g_variant_get_string (variant, nullptr);
  GdkRGBA rgba;
  if (str && str[0] && gdk_rgba_parse (&rgba, str))
    g_value_set_boxed (value, &rgba);
  return TRUE;
}

static GVariant *
rgba_to_string (const GValue       *value,
                const GVariantType *type,
                gpointer            user_data)
{
  const GdkRGBA *rgba = (const GdkRGBA *)g_value_get_boxed (value);
  if (rgba != nullptr)
    return g_variant_new_take_string (gdk_rgba_to_string (rgba));
  return nullptr;
}

static gboolean
sanitize_font_string (GValue   *value,
                      GVariant *variant,
                      gpointer  user_data)
{
  const char *str = g_variant_get_string (variant, nullptr);

  if (str && str[0]) {
    g_autoptr(PangoFontDescription) font_desc = pango_font_description_from_string (str);

    if (font_desc)
      g_value_take_string (value, pango_font_description_to_string (font_desc));
  }

  return TRUE;
}

static gboolean
index_from_list_value(GValue* value,
                      GVariant *variant,
                      void* user_data)
{
  auto const model = reinterpret_cast<GListModel*>(user_data);
  if (g_list_model_get_item_type(model) != TERMINAL_TYPE_PREFERENCES_LIST_ITEM)
    return false;

  auto const str = g_variant_get_string(variant, nullptr);
  auto const n_items = g_list_model_get_n_items(model);
  for (auto i = 0u; i < n_items; ++i) {
    gs_unref_object auto item = reinterpret_cast<TerminalPreferencesListItem const*>
      (g_list_model_get_item(model, i));
    if (!item)
      continue;

    auto const ivariant = terminal_preferences_list_item_get_value(item);
    if (!ivariant)
      continue;

    if (!g_variant_is_of_type(ivariant, G_VARIANT_TYPE("s")))
      continue;

    auto const istr = g_variant_get_string(ivariant, nullptr);
    if (!g_str_equal(istr, str))
      continue;

    g_value_set_uint(value, i);
    return true;
  }

  return false;
}

static GVariant *
list_value_from_index(const GValue* value,
                      const GVariantType* type,
                      void* user_data)
{
  auto const model = reinterpret_cast<GListModel*>(user_data);
  if (g_list_model_get_item_type(model) != TERMINAL_TYPE_PREFERENCES_LIST_ITEM)
    return nullptr;

  auto const i = g_value_get_uint(value);
  gs_unref_object auto item = reinterpret_cast<TerminalPreferencesListItem const*>
    (g_list_model_get_item(model, i));
  if (!item)
    return nullptr;

  return g_variant_ref(terminal_preferences_list_item_get_value(item));
}

static void
terminal_profile_editor_constructed (GObject *object)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);
  g_autoptr(GListModel) color_palette_model = nullptr;
  g_autoptr(GListModel) color_schemes_model = nullptr;
  g_autoptr(GListModel) encodings_model = nullptr;

  G_OBJECT_CLASS (terminal_profile_editor_parent_class)->constructed (object);

  auto const app = terminal_app_get();
  auto const profiles = terminal_app_get_profiles_list(app);
  g_autofree auto uuid = terminal_settings_list_dup_uuid_from_child(profiles, self->settings);

  gtk_label_set_label (self->uuid, uuid ? uuid : "");

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                   self->visible_name, "text",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_FONT_KEY,
                                self->custom_font_label, "label",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                sanitize_font_string, nullptr, nullptr, nullptr);

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   self->use_system_font, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   self->custom_font, "sensitive",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET|G_SETTINGS_BIND_INVERT_BOOLEAN));
  g_settings_bind (self->settings, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   self->custom_font_label, "sensitive",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET|G_SETTINGS_BIND_INVERT_BOOLEAN));
  g_settings_bind (self->settings, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   self->custom_font, "activatable",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET|G_SETTINGS_BIND_INVERT_BOOLEAN));
  terminal_util_set_settings_and_key_for_widget(GTK_WIDGET(self->custom_font),
                                                self->settings,
                                                TERMINAL_PROFILE_FONT_KEY);

  // Hide sixel pref when vte does not support images
  gtk_widget_set_visible(GTK_WIDGET(self->image_group),
                         (vte_get_feature_flags() & VTE_FEATURE_FLAG_SIXEL) != 0);

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_ENABLE_SIXEL_KEY,
                   self->enable_sixel, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_AUDIBLE_BELL_KEY,
                   self->audible_bell, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY,
                   self->columns, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY,
                   self->rows, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY,
                   self->cell_height, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY,
                   self->cell_width, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY,
                                self->show_scrollbar, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->show_scrollbar),
                                nullptr);

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_KINETIC_SCROLLING_KEY,
                   self->kinetic_scrolling, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY,
                   self->scroll_on_insert, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY,
                   self->scroll_on_keystroke, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY,
                   self->scroll_on_output, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                   self->limit_scrollback, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT|G_SETTINGS_BIND_INVERT_BOOLEAN));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY,
                   self->scrollback_lines, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_LOGIN_SHELL_KEY,
                   self->login_shell, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                   self->use_custom_command, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_CUSTOM_COMMAND_KEY,
                   self->custom_command, "text",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY,
                   self->show_bold_in_bright, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                   self->use_system_colors, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                   self->bold_color_set, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT|G_SETTINGS_BIND_INVERT_BOOLEAN));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                   self->cursor_colors_set, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  terminal_util_g_settings_bind (self->settings, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                   self->highlight_colors_set, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                                self->default_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                self->default_color_background, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_BOLD_COLOR_KEY,
                                self->bold_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY,
                                self->cursor_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY,
                                self->cursor_color_background, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY,
                                self->highlight_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY,
                                self->highlight_color_background, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);

  encodings_model = create_encodings_model ();
  adw_combo_row_set_enable_search (self->encoding, TRUE);
  adw_combo_row_set_model (self->encoding, encodings_model);

  color_schemes_model = create_color_scheme_model ();
  adw_combo_row_set_model (self->color_schemes, color_schemes_model);
  terminal_profile_editor_scheme_changed_from_settings (self, nullptr, self->settings);
  g_signal_connect_object (self->color_schemes,
                           "notify::selected-item",
                           G_CALLBACK (terminal_profile_editor_scheme_changed_from_selection),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings,
                           "changed::" TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                           G_CALLBACK (terminal_profile_editor_scheme_changed_from_settings),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings,
                           "changed::" TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                           G_CALLBACK (terminal_profile_editor_scheme_changed_from_settings),
                           self,
                           G_CONNECT_SWAPPED);

  color_palette_model = create_color_palette_model ();
  adw_combo_row_set_model (self->color_palette, color_palette_model);
  terminal_profile_editor_palette_changed_from_settings (self, TERMINAL_PROFILE_PALETTE_KEY, self->settings);
  g_signal_connect_object (self->color_palette,
                           "notify::selected-item",
                           G_CALLBACK (terminal_profile_editor_palette_changed_from_selection),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings,
                           "changed::" TERMINAL_PROFILE_PALETTE_KEY,
                           G_CALLBACK (terminal_profile_editor_palette_changed_from_settings),
                           self,
                           G_CONNECT_SWAPPED);
  terminal_util_set_settings_and_key_for_widget(GTK_WIDGET(self->color_palette),
                                                self->settings,
                                                TERMINAL_PROFILE_PALETTE_KEY);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY,
                                self->allow_blinking_text, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->allow_blinking_text),
                                nullptr);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY,
                                self->cursor_blink, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->cursor_blink),
                                nullptr);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_CURSOR_SHAPE_KEY,
                                self->cursor_shape, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->cursor_shape),
                                nullptr);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_EXIT_ACTION_KEY,
                                self->when_command_exits, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->when_command_exits),
                                nullptr);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_PRESERVE_WORKING_DIRECTORY_KEY,
                                self->preserve_working_directory, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->preserve_working_directory),
                                nullptr);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY,
                                self->backspace_key, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->backspace_key),
                                nullptr);
  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_DELETE_BINDING_KEY,
                                self->delete_key, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->delete_key),
                                nullptr);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_ENCODING_KEY,
                                self->encoding, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                g_object_ref (encodings_model),
                                g_object_unref);

  terminal_util_g_settings_bind_with_mapping (self->settings, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY,
                                self->ambiguous_width, "selected",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                index_from_list_value, list_value_from_index,
                                adw_combo_row_get_model(self->ambiguous_width),
                                nullptr);

  recurse_remove_emoji_hint (GTK_WIDGET (self->cell_height));
  recurse_remove_emoji_hint (GTK_WIDGET (self->cell_width));
  recurse_remove_emoji_hint (GTK_WIDGET (self->columns));
  recurse_remove_emoji_hint (GTK_WIDGET (self->rows));
  recurse_remove_emoji_hint (GTK_WIDGET (self->scrollback_lines));
}

static void
terminal_profile_editor_dispose (GObject *object)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_PROFILE_EDITOR);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (terminal_profile_editor_parent_class)->dispose (object);
}

static void
terminal_profile_editor_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  switch (prop_id) {
  case PROP_SETTINGS:
    g_value_set_object (value, self->settings);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_profile_editor_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);

  switch (prop_id) {
  case PROP_SETTINGS:
    self->settings = G_SETTINGS (g_value_dup_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_profile_editor_class_init (TerminalProfileEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = terminal_profile_editor_constructed;
  object_class->dispose = terminal_profile_editor_dispose;
  object_class->get_property = terminal_profile_editor_get_property;
  object_class->set_property = terminal_profile_editor_set_property;

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", nullptr, nullptr,
                         G_TYPE_SETTINGS,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_action (widget_class, "uuid.copy", nullptr, terminal_profile_editor_copy_uuid);
  gtk_widget_class_install_action (widget_class, "size.reset", nullptr, terminal_profile_editor_reset_size);
  gtk_widget_class_install_action (widget_class, "compatibility.reset", nullptr, terminal_profile_editor_reset_compatibility);
  gtk_widget_class_install_action (widget_class, "profile.select-custom-font", nullptr, terminal_profile_editor_select_custon_font);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/profile-editor.ui");

  gtk_widget_class_bind_template_callback (widget_class, terminal_profile_editor_palette_index_changed);

  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, allow_blinking_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, ambiguous_width);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, audible_bell);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, backspace_key);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, bold_color_set);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, bold_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cell_height);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cell_width);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, color_palette);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, color_schemes);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, columns);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_blink);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_color_background);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_colors_set);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_shape);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, custom_command);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, custom_font);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, custom_font_label);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, default_color_background);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, default_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, delete_key);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, enable_sixel);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, encoding);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, highlight_color_background);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, highlight_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, highlight_colors_set);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, image_group);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, kinetic_scrolling);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, limit_scrollback);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, login_shell);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_0);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_1);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_10);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_11);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_12);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_13);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_14);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_15);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_2);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_3);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_4);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_5);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_6);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_7);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_8);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, palette_9);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, preserve_working_directory);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, rows);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, scroll_on_insert);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, scroll_on_keystroke);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, scroll_on_output);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, scrollback_lines);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, show_bold_in_bright);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, show_scrollbar);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, use_custom_command);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, use_system_colors);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, use_system_font);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, uuid);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, visible_name);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, when_command_exits);

  g_type_ensure (TERMINAL_TYPE_COLOR_ROW);
}

static void
terminal_profile_editor_init (TerminalProfileEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
terminal_profile_editor_new (GSettings *settings)
{
  g_return_val_if_fail (G_IS_SETTINGS (settings), nullptr);

  return (GtkWidget *)g_object_new (TERMINAL_TYPE_PROFILE_EDITOR,
                                    "settings", settings,
                                    nullptr);
}
