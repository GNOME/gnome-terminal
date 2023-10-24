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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>
#include <vte/vte.h>

#include "terminal-color-row.hh"
#include "terminal-profile-editor.hh"
#include "terminal-preferences-list-item.hh"

struct _TerminalProfileEditor
{
  AdwNavigationPage    parent_instance;

  AdwSwitchRow        *audible_bell;
  TerminalColorRow    *bold_color_set;
  TerminalColorRow    *bold_color_text;
  AdwSpinRow          *cell_height;
  AdwSpinRow          *cell_width;
  AdwSpinRow          *columns;
  TerminalColorRow    *cursor_color_text;
  TerminalColorRow    *cursor_color_background;
  GtkSwitch           *cursor_colors_set;
  AdwEntryRow         *custom_command;
  AdwActionRow        *custom_font;
  TerminalColorRow    *default_color_text;
  TerminalColorRow    *default_color_background;
  AdwSwitchRow        *enable_bidi;
  AdwSwitchRow        *enable_shaping;
  TerminalColorRow    *highlight_color_text;
  TerminalColorRow    *highlight_color_background;
  GtkSwitch           *highlight_colors_set;
  AdwPreferencesGroup *image_group;
  AdwSwitchRow        *enable_sixel;
  AdwComboRow         *encoding;
  AdwSwitchRow        *limit_scrollback;
  AdwSwitchRow        *login_shell;
  AdwComboRow         *preserve_working_directory;
  AdwSwitchRow        *rewrap_on_resize;
  AdwSpinRow          *rows;
  AdwSwitchRow        *scroll_on_keystroke;
  AdwSwitchRow        *scroll_on_output;
  AdwSpinRow          *scrollback_lines;
  AdwSwitchRow        *show_bold_in_bright;
  AdwSwitchRow        *show_scrollbar;
  AdwEntryRow         *title;
  AdwSwitchRow        *use_custom_command;
  AdwSwitchRow        *use_system_colors;
  AdwSwitchRow        *use_system_font;
  GtkLabel            *uuid;
  AdwEntryRow         *visible_name;

  GSettings           *settings;
};

enum {
  PROP_0,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalProfileEditor, terminal_profile_editor, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties [N_PROPS];

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

static gboolean
scrollbar_policy_to_boolean (GValue   *value,
                             GVariant *variant,
                             gpointer  user_data)
{
  g_value_set_boolean (value,
                       9 == g_strcmp0 ("always", g_variant_get_string (variant, nullptr)));
  return TRUE;
}

static GVariant *
boolean_to_scrollbar_policy (const GValue       *value,
                             const GVariantType *type,
                             gpointer            user_data)
{
  if (g_value_get_boolean (value))
    return g_variant_new_string ("always");
  else
    return g_variant_new_string ("never");
}

static void
terminal_profile_editor_reset_size (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (widget);

  g_settings_reset (self->settings, "default-size-columns");
  g_settings_reset (self->settings, "default-size-rows");
  g_settings_reset (self->settings, "cell-height-scale");
  g_settings_reset (self->settings, "cell-width-scale");
}

static void
terminal_profile_editor_reset_compatibility (GtkWidget  *widget,
                                             const char *action_name,
                                             GVariant   *param)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (widget);

  g_settings_reset (self->settings, "cjk-utf8-ambiguous-width");
  g_settings_reset (self->settings, "encoding");
  g_settings_reset (self->settings, "delete-binding");
  g_settings_reset (self->settings, "backspace-binding");
}

static gboolean
extract_uuid (const char  *path,
              char       **uuid)
{
  g_autoptr(GString) str = g_string_new (path);

  if (str->len > 0) {
    if (str->str[str->len-1] == '/')
      g_string_truncate (str, str->len-1);

    const char *slash = strrchr (str->str, '/');

    if (slash && slash[1] == ':') {
      *uuid = g_strdup (slash+2);
      return TRUE;
    }
  }

  return FALSE;
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

static void
terminal_profile_editor_constructed (GObject *object)
{
  TerminalProfileEditor *self = TERMINAL_PROFILE_EDITOR (object);
  g_autoptr(GListModel) encodings_model = nullptr;
  g_autofree char *path = nullptr;
  g_autofree char *uuid = nullptr;

  G_OBJECT_CLASS (terminal_profile_editor_parent_class)->constructed (object);

  g_object_get (self->settings, "path", &path, nullptr);

  if (extract_uuid (path, &uuid)) {
    gtk_label_set_label (self->uuid, uuid);
  }

  g_settings_bind (self->settings, "visible-name",
                   self->visible_name, "text",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "use-system-font",
                   self->use_system_font, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "use-system-font",
                   self->custom_font, "sensitive",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET|G_SETTINGS_BIND_INVERT_BOOLEAN));
  g_settings_bind (self->settings, "enable-bidi",
                   self->enable_bidi, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "enable-shaping",
                   self->enable_shaping, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  // Hide sixel pref when vte does not support images
  gtk_widget_set_visible(GTK_WIDGET(self->image_group),
                         (vte_get_feature_flags() & VTE_FEATURE_FLAG_SIXEL) != 0);

  g_settings_bind (self->settings, "enable-sixel",
                   self->enable_sixel, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "audible-bell",
                   self->audible_bell, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "default-size-columns",
                   self->columns, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "default-size-rows",
                   self->rows, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "cell-height-scale",
                   self->cell_height, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "cell-width-scale",
                   self->cell_width, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind_with_mapping (self->settings, "scrollbar-policy",
                                self->show_scrollbar, "active",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                scrollbar_policy_to_boolean,
                                boolean_to_scrollbar_policy,
                                nullptr, nullptr);

  g_settings_bind (self->settings, "scroll-on-keystroke",
                   self->scroll_on_keystroke, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "scroll-on-output",
                   self->scroll_on_output, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "scrollback-unlimited",
                   self->limit_scrollback, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT|G_SETTINGS_BIND_INVERT_BOOLEAN));
  g_settings_bind (self->settings, "scrollback-lines",
                   self->scrollback_lines, "value",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "login-shell",
                   self->login_shell, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "use-custom-command",
                   self->use_custom_command, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "custom-command",
                   self->custom_command, "text",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "bold-is-bright",
                   self->show_bold_in_bright, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "use-theme-colors",
                   self->use_system_colors, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "rewrap-on-resize",
                   self->rewrap_on_resize, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind (self->settings, "bold-color-same-as-fg",
                   self->bold_color_set, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT|G_SETTINGS_BIND_INVERT_BOOLEAN));
  g_settings_bind (self->settings, "cursor-colors-set",
                   self->cursor_colors_set, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));
  g_settings_bind (self->settings, "highlight-colors-set",
                   self->cursor_colors_set, "active",
                   GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT));

  g_settings_bind_with_mapping (self->settings, "foreground-color",
                                self->default_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  g_settings_bind_with_mapping (self->settings, "background-color",
                                self->default_color_background, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  g_settings_bind_with_mapping (self->settings, "bold-color",
                                self->bold_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  g_settings_bind_with_mapping (self->settings, "cursor-foreground-color",
                                self->cursor_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  g_settings_bind_with_mapping (self->settings, "cursor-background-color",
                                self->cursor_color_background, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  g_settings_bind_with_mapping (self->settings, "highlight-foreground-color",
                                self->highlight_color_text, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);
  g_settings_bind_with_mapping (self->settings, "highlight-background-color",
                                self->highlight_color_background, "color",
                                GSettingsBindFlags(G_SETTINGS_BIND_DEFAULT),
                                string_to_rgba, rgba_to_string,
                                nullptr, nullptr);

  encodings_model = create_encodings_model ();
  adw_combo_row_set_enable_search (self->encoding, TRUE);
  adw_combo_row_set_model (self->encoding, encodings_model);
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

  gtk_widget_class_install_action (widget_class, "size.reset", nullptr, terminal_profile_editor_reset_size);
  gtk_widget_class_install_action (widget_class, "compatibility.reset", nullptr, terminal_profile_editor_reset_compatibility);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/profile-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, audible_bell);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, bold_color_set);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, bold_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cell_height);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cell_width);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, columns);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_color_background);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, cursor_colors_set);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, custom_command);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, custom_font);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, default_color_background);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, default_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, enable_bidi);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, enable_shaping);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, enable_sixel);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, encoding);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, highlight_color_background);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, highlight_color_text);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, highlight_colors_set);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, image_group);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, limit_scrollback);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, login_shell);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, preserve_working_directory);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, rewrap_on_resize);
  gtk_widget_class_bind_template_child (widget_class, TerminalProfileEditor, rows);
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
