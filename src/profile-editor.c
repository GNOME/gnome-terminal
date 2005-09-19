/* dialog for editing a profile */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#include "profile-editor.h"
#include "terminal-intl.h"
#include "terminal.h"
#include <glade/glade.h>
#include <libgnomeui/gnome-file-entry.h>
#include <libgnomeui/gnome-icon-entry.h>
#include <libgnomeui/gnome-thumbnail.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <string.h>
#include <math.h>
#include "simple-x-font-selector.h"
#include "terminal-widget.h"

#define BYTES_PER_LINE (terminal_widget_get_estimated_bytes_per_scrollback_line ())

typedef struct _TerminalColorScheme TerminalColorScheme;

struct _TerminalColorScheme
{
  const char *name;
  GdkColor foreground;
  GdkColor background;
};

/* alphabetized */
static TerminalColorScheme color_schemes[] = {
  { N_("Black on light yellow"),
    { 0, 0x0000, 0x0000, 0x0000 }, { 0, 0xFFFF, 0xFFFF, 0xDDDD } },
  { N_("Black on white"),
    { 0, 0x0000, 0x0000, 0x0000 }, { 0, 0xFFFF, 0xFFFF, 0xFFFF } },
  { N_("Gray on black"),
    { 0, 0xAAAA, 0xAAAA, 0xAAAA }, { 0, 0x0000, 0x0000, 0x0000 } },
  { N_("Green on black"),
    { 0, 0x0000, 0xFFFF, 0x0000 }, { 0, 0x0000, 0x0000, 0x0000 } },
  { N_("White on black"),
    { 0, 0xFFFF, 0xFFFF, 0xFFFF }, { 0, 0x0000, 0x0000, 0x0000 } }
};

typedef struct _TerminalPaletteScheme TerminalPaletteScheme;

struct _TerminalPaletteScheme
{
  const char *name;
  const GdkColor *palette;
};

static TerminalPaletteScheme palette_schemes[] = {
  { N_("Linux console"), terminal_palette_linux },
  { N_("XTerm"), terminal_palette_xterm },
  { N_("Rxvt"), terminal_palette_rxvt }
};

static GtkWidget* profile_editor_get_widget                  (GtkWidget       *editor,
                                                              const char      *widget_name);
static void       profile_editor_update_sensitivity          (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_visible_name         (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_icon                 (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_cursor_blink         (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_default_show_menubar (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_color_pickers        (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_color_scheme_menu    (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_title                (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_title_mode           (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_allow_bold           (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_silent_bell          (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_word_chars           (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_scrollbar_position   (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_scrollback_lines     (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_scroll_on_keystroke  (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_scroll_on_output     (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_exit_action          (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_login_shell          (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_update_records       (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_use_custom_command   (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_custom_command       (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_palette              (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_x_font               (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_background_type      (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_background_image     (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_scroll_background    (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_background_darkness  (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_backspace_binding    (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_delete_binding       (GtkWidget       *widget,
                                                              TerminalProfile *profile);

static void       profile_editor_update_use_theme_colors     (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_use_system_font      (GtkWidget       *widget,
                                                              TerminalProfile *profile);
static void       profile_editor_update_font                 (GtkWidget       *widget,
                                                              TerminalProfile *profile);


static void profile_forgotten (TerminalProfile           *profile,
                               GtkWidget                 *editor);
static void profile_changed   (TerminalProfile           *profile,
                               const TerminalSettingMask *mask,
                               GtkWidget                 *editor);



static void
entry_set_text_if_changed (GtkEntry   *entry,
                           const char *text)
{
  char *s;

  s = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  if (strcmp (s, text) != 0)
    gtk_entry_set_text (GTK_ENTRY (entry), text);

  g_free (s);
}

static void
colorpicker_set_if_changed (GtkWidget      *colorpicker,
                            const GdkColor *color)
{
  GdkColor old_color;

  gtk_color_button_get_color (GTK_COLOR_BUTTON (colorpicker), &old_color);

  if (!gdk_color_equal (color, &old_color))
    gtk_color_button_set_color (GTK_COLOR_BUTTON (colorpicker), color);
}

static void
profile_editor_destroyed (GtkWidget       *editor,
                          TerminalProfile *profile)
{
  g_signal_handlers_disconnect_by_func (G_OBJECT (profile),
                                        G_CALLBACK (profile_forgotten),
                                        editor);

  g_signal_handlers_disconnect_by_func (G_OBJECT (profile),
                                        G_CALLBACK (profile_changed),
                                        editor);
  
  g_object_set_data (G_OBJECT (profile), "editor-window", NULL);
  g_object_set_data (G_OBJECT (editor), "glade-xml", NULL);
  profile_name_entry_notify (profile);
}

static PangoFontDescription*
fontpicker_get_desc (GtkWidget *font_picker)
{
  const char *current_name;
  PangoFontDescription *current_desc;

  current_name = gtk_font_button_get_font_name (GTK_FONT_BUTTON (font_picker));
  if (current_name)
    current_desc = pango_font_description_from_string (current_name);
  else
    current_desc = NULL;

  return current_desc;
}

static void
fontpicker_set_if_changed (GtkWidget                  *font_picker,
                           const PangoFontDescription *font_desc)
{
  PangoFontDescription *current_desc;

  current_desc = fontpicker_get_desc (font_picker);

  if (current_desc == NULL || !pango_font_description_equal (font_desc, current_desc))
    {
      char *str;

      str = pango_font_description_to_string (font_desc);
      gtk_font_button_set_font_name (GTK_FONT_BUTTON (font_picker),
                                       str);

      g_free (str);
    }

  if (current_desc)
    pango_font_description_free (current_desc);
}

/*
 * Profile callbacks
 */

static void
profile_forgotten (TerminalProfile *profile,
                   GtkWidget       *editor)
{
  /* FIXME this might confuse users a bit, if it ever happens (unlikely
   * to actually happen much)
   */
  gtk_widget_destroy (editor);
}

static void
profile_changed (TerminalProfile           *profile,
                 const TerminalSettingMask *mask,
                 GtkWidget                 *editor)
{
  if (mask->visible_name)
    profile_editor_update_visible_name (editor, profile);

  if (mask->icon_file)
    profile_editor_update_icon (editor, profile);
  
  if (mask->cursor_blink)
    profile_editor_update_cursor_blink (editor, profile);

  if (mask->default_show_menubar)
    profile_editor_update_default_show_menubar (editor, profile);

  if ((mask->foreground_color) ||
      (mask->background_color))
    {
      profile_editor_update_color_scheme_menu (editor, profile);
      profile_editor_update_color_pickers (editor, profile);
    }

  if (mask->title)
    profile_editor_update_title (editor, profile);
  
  if (mask->title_mode)
    profile_editor_update_title_mode (editor, profile);

  if (mask->allow_bold)
    profile_editor_update_allow_bold (editor, profile);

  if (mask->silent_bell)
    profile_editor_update_silent_bell (editor, profile);

  if (mask->word_chars)
    profile_editor_update_word_chars (editor, profile);

  if (mask->scrollbar_position)
    profile_editor_update_scrollbar_position (editor, profile);

  if (mask->scrollback_lines)
    profile_editor_update_scrollback_lines (editor, profile);

  if (mask->scroll_on_keystroke)
    profile_editor_update_scroll_on_keystroke (editor, profile);

  if (mask->scroll_on_output)
    profile_editor_update_scroll_on_output (editor, profile);

  if (mask->exit_action)
    profile_editor_update_exit_action (editor, profile);

  if (mask->login_shell)
    profile_editor_update_login_shell (editor, profile);

  if (mask->update_records)
    profile_editor_update_update_records (editor, profile);

  if (mask->use_custom_command)
    profile_editor_update_use_custom_command (editor, profile);

  if (mask->custom_command)
    profile_editor_update_custom_command (editor, profile);

  if (mask->palette)
    profile_editor_update_palette (editor, profile);

  if (mask->x_font)
    profile_editor_update_x_font (editor, profile);

  if (mask->background_type)
    profile_editor_update_background_type (editor, profile);
  
  if (mask->background_image_file)
    profile_editor_update_background_image (editor, profile);

  if (mask->scroll_background)
    profile_editor_update_scroll_background (editor, profile);

  if (mask->background_darkness)
    profile_editor_update_background_darkness (editor, profile);

  if (mask->backspace_binding)
    profile_editor_update_backspace_binding (editor, profile);

  if (mask->delete_binding)
    profile_editor_update_delete_binding (editor, profile);

  if (mask->use_theme_colors)
    profile_editor_update_use_theme_colors (editor, profile);
    
  if (mask->use_system_font)
    profile_editor_update_use_system_font (editor, profile);

  if (mask->font)
    profile_editor_update_font (editor, profile);
  
  profile_editor_update_sensitivity (editor, profile);
}

/*
 * Widget callbacks
 */

static void
visible_name_changed (GtkWidget       *entry,
                      TerminalProfile *profile)
{
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  
  terminal_profile_set_visible_name (profile, text);

  g_free (text);
}

static void
icon_changed (GtkWidget       *icon_entry,
              TerminalProfile *profile)
{
  char *filename;

  filename = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (icon_entry));

  /* NULL filename happens here to unset */
  terminal_profile_set_icon_file (profile, filename);
  
  g_free (filename);
}

static void
cursor_blink_toggled (GtkWidget       *checkbutton,
                      TerminalProfile *profile)
{
  terminal_profile_set_cursor_blink (profile,
                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
show_menubar_toggled (GtkWidget       *checkbutton,
                      TerminalProfile *profile)
{
  terminal_profile_set_default_show_menubar (profile,
                                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
foreground_color_set (GtkWidget       *colorpicker,
                      TerminalProfile *profile)
{
  GdkColor color;
  GdkColor bg;

  gtk_color_button_get_color (GTK_COLOR_BUTTON (colorpicker), &color);

  terminal_profile_get_color_scheme (profile, NULL, &bg);
  
  terminal_profile_set_color_scheme (profile, &color, &bg);
}

static void
background_color_set (GtkWidget       *colorpicker,
                      TerminalProfile *profile)
{
  GdkColor color;
  GdkColor fg;

  gtk_color_button_get_color (GTK_COLOR_BUTTON (colorpicker), &color);

  terminal_profile_get_color_scheme (profile, &fg, NULL);
  
  terminal_profile_set_color_scheme (profile, &fg, &color);
}

static void
color_scheme_changed (GtkWidget       *combo_box,
                      TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
  
  if (i < G_N_ELEMENTS (color_schemes))
    terminal_profile_set_color_scheme (profile,
                                       &color_schemes[i].foreground,
                                       &color_schemes[i].background);
  else
    ; /* "custom" selected, no change */
}

static void
title_changed (GtkWidget       *entry,
               TerminalProfile *profile)
{
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  
  terminal_profile_set_title (profile, text);

  g_free (text);
}

static void
title_mode_changed (GtkWidget       *combo_box,
                    TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));  

  terminal_profile_set_title_mode (profile, i);
}

static void
allow_bold_toggled (GtkWidget       *checkbutton,
                    TerminalProfile *profile)
{
  terminal_profile_set_allow_bold (profile,
                                   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
bell_toggled (GtkWidget       *checkbutton,
              TerminalProfile *profile)
{
  terminal_profile_set_silent_bell (profile,
                                    !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
word_chars_changed (GtkWidget       *entry,
                    TerminalProfile *profile)
{
  char *text;
  
  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  
  terminal_profile_set_word_chars (profile, text);

  g_free (text);
}

static void 
font_changed (EggXFontSelector *selector, TerminalProfile *profile)
{
  gchar *name = egg_xfont_selector_get_font_name (selector);
  if (name)
    terminal_profile_set_x_font (profile, name);
  g_free (name);
}

static void
scrollbar_position_changed (GtkWidget       *combo_box,
                            TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

  terminal_profile_set_scrollbar_position (profile, i);
}

static void
scrollback_lines_value_changed (GtkWidget       *spin,
                                TerminalProfile *profile)
{
  double val;
  
  val = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

  terminal_profile_set_scrollback_lines (profile, val);
}

static void
scrollback_kilobytes_value_changed (GtkWidget       *spin,
                                    TerminalProfile *profile)
{
  double val;
  
  val = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

  terminal_profile_set_scrollback_lines (profile,
                                         (val * 1024) / BYTES_PER_LINE);
}

static void
scroll_on_keystroke_toggled (GtkWidget       *checkbutton,
                             TerminalProfile *profile)
{
  terminal_profile_set_scroll_on_keystroke (profile,
                                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
scroll_on_output_toggled (GtkWidget       *checkbutton,
                          TerminalProfile *profile)
{
  terminal_profile_set_scroll_on_output (profile,
                                         gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
exit_action_changed (GtkWidget       *combo_box,
                     TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

  terminal_profile_set_exit_action (profile, i);
}

static void
login_shell_toggled (GtkWidget       *checkbutton,
                     TerminalProfile *profile)
{
  terminal_profile_set_login_shell (profile,
                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
update_records_toggled (GtkWidget       *checkbutton,
                        TerminalProfile *profile)
{
  terminal_profile_set_update_records (profile,
                                       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
use_custom_command_toggled (GtkWidget       *checkbutton,
                            TerminalProfile *profile)
{
  terminal_profile_set_use_custom_command (profile,
                                           gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
custom_command_changed (GtkWidget       *entry,
                        TerminalProfile *profile)
{
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  
  terminal_profile_set_custom_command (profile, text);

  g_free (text);
}

static void
palette_scheme_changed (GtkWidget       *combo_box,
                      TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
  
  if (i < G_N_ELEMENTS (palette_schemes))
    terminal_profile_set_palette (profile,
                                  palette_schemes[i].palette);
  else
    ; /* "custom" selected, no change */
}

static void
palette_color_set (GtkWidget       *colorpicker,
                   TerminalProfile *profile)
{
  int i;
  GdkColor color;

  gtk_color_button_get_color (GTK_COLOR_BUTTON (colorpicker), &color);

  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (colorpicker), "palette-entry-index"));

  terminal_profile_set_palette_entry (profile, i, &color);
}

static void
solid_radio_toggled (GtkWidget       *radiobutton,
                     TerminalProfile *profile)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radiobutton)))
    terminal_profile_set_background_type (profile,
                                          TERMINAL_BACKGROUND_SOLID);
}

static void
image_radio_toggled (GtkWidget       *radiobutton,
                     TerminalProfile *profile)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radiobutton)))
    terminal_profile_set_background_type (profile,
                                          TERMINAL_BACKGROUND_IMAGE);
}

static void
transparent_radio_toggled (GtkWidget       *radiobutton,
                           TerminalProfile *profile)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radiobutton)))
    terminal_profile_set_background_type (profile,
                                          TERMINAL_BACKGROUND_TRANSPARENT);
}

static void
background_image_changed (GtkWidget       *entry,
                          TerminalProfile *profile)
{
  char *text;
  
  text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (entry));
  
  terminal_profile_set_background_image_file (profile, text);

  g_free (text);
}

static void
scroll_background_toggled (GtkWidget       *checkbutton,
                           TerminalProfile *profile)
{
  terminal_profile_set_scroll_background (profile,
                                          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
darken_background_value_changed (GtkWidget       *scale,
                                 TerminalProfile *profile)
{
  double new_val;
  double old_val;

  new_val = gtk_range_get_value (GTK_RANGE (scale));
  old_val = terminal_profile_get_background_darkness (profile);

  /* The epsilon here is some anti-infinite-loop paranoia */
  if (fabs (new_val - old_val) > 1e-6)
    terminal_profile_set_background_darkness (profile, new_val);
}

static void
backspace_binding_changed (GtkWidget       *combo_box,
                           TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

  terminal_profile_set_backspace_binding (profile, i);
}

static void
delete_binding_changed (GtkWidget       *combo_box,
                        TerminalProfile *profile)
{
  int i;
  
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

  terminal_profile_set_delete_binding (profile, i);
}

static void
reset_compat_defaults_clicked (GtkWidget       *button,
                               TerminalProfile *profile)
{
  terminal_profile_reset_compat_defaults (profile);
}

static void
use_theme_colors_toggled (GtkWidget       *checkbutton,
                          TerminalProfile *profile)
{
  terminal_profile_set_use_theme_colors (profile,
                                         gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
use_system_font_toggled (GtkWidget       *checkbutton,
                         TerminalProfile *profile)
{
  terminal_profile_set_use_system_font (profile,
                                        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
font_set (GtkWidget       *fontpicker,
          TerminalProfile *profile)
{
  PangoFontDescription *desc;
  PangoFontDescription *tmp;
  const char *font_name;
  
  font_name = gtk_font_button_get_font_name (GTK_FONT_BUTTON (fontpicker));
  desc = pango_font_description_from_string (font_name);
  if (desc == NULL)
    {
      g_warning ("Font name \"%s\" from font picker can't be parsed", font_name);
      return;
    }

  /* Merge as paranoia against fontpicker giving us some junk */
  tmp = pango_font_description_copy (terminal_profile_get_font (profile));
  pango_font_description_merge (tmp, desc, TRUE);
  pango_font_description_free (desc);
  desc = tmp;
  
  terminal_profile_set_font (profile, desc);

  pango_font_description_free (desc);
}

/*
 * initialize widgets
 */

static void
init_color_scheme_menu (GtkWidget *combo_box)
{
  int i;

  i = G_N_ELEMENTS (color_schemes);
  while (i > 0)
    {
      gtk_combo_box_prepend_text (GTK_COMBO_BOX (combo_box), 
                                  _(color_schemes[--i].name));
    }
}

static void
init_palette_scheme_menu (GtkWidget *combo_box)
{
  int i;

  i = G_N_ELEMENTS (palette_schemes);
  while (i > 0)
    {
      gtk_combo_box_prepend_text (GTK_COMBO_BOX (combo_box), 
                                  _(palette_schemes[--i].name));
    }
}

static char*
format_percent_value (GtkScale *scale,
                      double    val,
                      void     *data)
{
  return g_strdup_printf ("%d%%", (int) rint (val * 100.0));
}

static void
init_background_darkness_scale (GtkWidget *scale)
{
  g_signal_connect (G_OBJECT (scale), "format_value",
                    G_CALLBACK (format_percent_value),
                    NULL);
}


static void
editor_response_cb (GtkDialog *editor,
                    int        id,
                    void      *data)
{  
  if (id == GTK_RESPONSE_HELP)
    terminal_util_show_help ("gnome-terminal-prefs", GTK_WINDOW (editor));
  else
    gtk_widget_destroy (GTK_WIDGET (editor));
}

static GdkPixbuf *
create_preview_pixbuf (const gchar *file) 
{
  GdkPixbuf *pixbuf = NULL;

  if (file != NULL) {

    if (g_file_test (file, G_FILE_TEST_EXISTS) == TRUE) {

      GnomeThumbnailFactory *thumbs;
      gchar *mime_type;

      mime_type = (gchar *)gnome_vfs_get_mime_type (file);
      thumbs = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);

      pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbs,
                                                           file,
                                                           mime_type);
      g_free (mime_type);
    }
  }				
  return pixbuf;
}

static void 
update_image_preview (GtkFileChooser *chooser) 
{
  GtkWidget *image;
  gchar *file;

  image = gtk_file_chooser_get_preview_widget (GTK_FILE_CHOOSER (chooser));
  file = gtk_file_chooser_get_preview_filename (chooser);
  
  if (file != NULL) {

    GdkPixbuf *pixbuf = NULL;
    
    pixbuf = create_preview_pixbuf (file);
    g_free (file);

    if (pixbuf != NULL) {
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
      g_object_unref (pixbuf);
    }
    else {
      gtk_image_set_from_stock (GTK_IMAGE (image),
                                "gtk-dialog-question",
      	                        GTK_ICON_SIZE_DIALOG);
    }
  }				
  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static void
setup_background_filechooser (GtkWidget *filechooser, 
                              TerminalProfile *profile)
{
  GtkFileFilter *filter;
  GtkWidget *image_preview;
  GdkPixbuf *pixbuf = NULL;
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_filter_set_name (filter, _("Images"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filechooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (filechooser), filter);

  image_preview = gtk_image_new ();
  pixbuf = create_preview_pixbuf (terminal_profile_get_background_image_file (profile));
  if (pixbuf != NULL) {
    gtk_image_set_from_pixbuf (GTK_IMAGE (image_preview), pixbuf);
  }
  else {
    gtk_image_set_from_stock (GTK_IMAGE (image_preview),
                              "gtk-dialog-question",
                              GTK_ICON_SIZE_DIALOG);
  }
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (filechooser),
                                       image_preview);
  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (filechooser),
                                          FALSE);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (filechooser), TRUE);
  gtk_widget_set_size_request (image_preview, 128, -1);  
  gtk_widget_show (image_preview); 

  g_signal_connect (filechooser, "update-preview",
                    G_CALLBACK (update_image_preview), NULL);
}

void
terminal_profile_edit (TerminalProfile *profile,
                       GtkWindow       *transient_parent)
{
  GtkWidget *editor, *fontsel;
  GtkWindow *old_transient_parent;

  editor = g_object_get_data (G_OBJECT (profile),
                              "editor-window");

  if (editor == NULL)
    {
      GladeXML *xml;
      GtkWidget *w;
      double num1, num2;
      gint i;
      GtkSizeGroup *size_group;

      xml = terminal_util_load_glade_file (TERM_GLADE_FILE,
                                           "profile-editor-dialog",
                                           transient_parent);
      if (xml == NULL)
        return;
      
      old_transient_parent = NULL;
      
      editor = glade_xml_get_widget (xml, "profile-editor-dialog");
	         
      g_object_set_data (G_OBJECT (profile),
                         "editor-window",
                         editor);      

      g_object_set_data_full (G_OBJECT (editor),
                              "glade-xml",
                              xml,
                              (GDestroyNotify) g_object_unref);
      
      g_signal_connect (G_OBJECT (editor),
                        "destroy",
                        G_CALLBACK (profile_editor_destroyed),
                        profile);

      g_signal_connect (G_OBJECT (editor),
                        "response",
                        G_CALLBACK (editor_response_cb),
                        NULL);
      
      gtk_window_set_destroy_with_parent (GTK_WINDOW (editor), TRUE);

      g_signal_connect (G_OBJECT (profile),
                        "changed",
                        G_CALLBACK (profile_changed),
                        editor);
      
      g_signal_connect (G_OBJECT (profile),
                        "forgotten",
                        G_CALLBACK (profile_forgotten),
                        editor);      

      profile_editor_update_sensitivity (editor, profile);

      /* Autoconnect is just too scary for me. */
      w = glade_xml_get_widget (xml, "profile-name-entry");
      profile_editor_update_visible_name (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (visible_name_changed),
                        profile);

      w = glade_xml_get_widget (xml, "profile-icon-entry");
      profile_editor_update_icon (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (icon_changed),
                        profile);
      
      w = glade_xml_get_widget (xml, "blink-cursor-checkbutton");
      profile_editor_update_cursor_blink (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (cursor_blink_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "show-menubar-checkbutton");
      profile_editor_update_default_show_menubar (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (show_menubar_toggled),
                        profile);

      profile_editor_update_color_pickers (editor, profile);
      
      w = glade_xml_get_widget (xml, "foreground-colorpicker");
      g_signal_connect (G_OBJECT (w), "color_set",
                        G_CALLBACK (foreground_color_set),
                        profile);

      w = glade_xml_get_widget (xml, "background-colorpicker");
      g_signal_connect (G_OBJECT (w), "color_set",
                        G_CALLBACK (background_color_set),
                        profile);

      w = glade_xml_get_widget (xml, "color-scheme-combobox");
      init_color_scheme_menu (w);
      profile_editor_update_color_scheme_menu (editor, profile);
      profile_editor_update_palette (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (color_scheme_changed),
                        profile);

      w = glade_xml_get_widget (xml, "title-entry");
      profile_editor_update_title (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (title_changed),
                        profile);

      w = glade_xml_get_widget (xml, "title-mode-combobox");
      profile_editor_update_title_mode (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (title_mode_changed),
                        profile);

      w = glade_xml_get_widget (xml, "allow-bold-checkbutton");
      profile_editor_update_allow_bold (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (allow_bold_toggled),
                        profile);
      
      w = glade_xml_get_widget (xml, "bell-checkbutton");
      profile_editor_update_silent_bell (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (bell_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "word-chars-entry");
      profile_editor_update_word_chars (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (word_chars_changed),
                        profile);
      
      w = glade_xml_get_widget (xml, "scrollbar-position-combobox");
      profile_editor_update_scrollbar_position (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (scrollbar_position_changed),
                        profile);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      
      w = glade_xml_get_widget (xml, "scrollback-lines-spinbutton");
      profile_editor_update_scrollback_lines (editor, profile);
      g_signal_connect (G_OBJECT (w), "value_changed",
                        G_CALLBACK (scrollback_lines_value_changed),
                        profile);
      gtk_size_group_add_widget (size_group, w);
      
      gtk_spin_button_get_range (GTK_SPIN_BUTTON (w), &num1, &num2);

      w = glade_xml_get_widget (xml, "scrollback-kilobytes-spinbutton");

      /* Sync kilobytes spinbutton range with the lines spinbutton */
      gtk_spin_button_set_range (GTK_SPIN_BUTTON (w),
                                 (num1 * BYTES_PER_LINE) / 1024,
                                 (num2 * BYTES_PER_LINE) / 1024);
      
      profile_editor_update_scrollback_lines (editor, profile);      
      g_signal_connect (G_OBJECT (w), "value_changed",
                        G_CALLBACK (scrollback_kilobytes_value_changed),
                        profile);
      gtk_size_group_add_widget (size_group, w);

      g_object_unref (G_OBJECT (size_group));
      
      w = glade_xml_get_widget (xml, "scroll-on-keystroke-checkbutton");
      profile_editor_update_scroll_on_keystroke (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (scroll_on_keystroke_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "scroll-on-output-checkbutton");
      profile_editor_update_scroll_on_output (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (scroll_on_output_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "exit-action-combobox");
      profile_editor_update_exit_action (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (exit_action_changed),
                        profile);
      
      w = glade_xml_get_widget (xml, "login-shell-checkbutton");
      profile_editor_update_login_shell (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (login_shell_toggled),
                        profile);
      
      w = glade_xml_get_widget (xml, "update-records-checkbutton");
      profile_editor_update_update_records (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (update_records_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "use-custom-command-checkbutton");
      profile_editor_update_use_custom_command (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (use_custom_command_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "custom-command-entry");
      profile_editor_update_custom_command (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (custom_command_changed),
                        profile);

      w = glade_xml_get_widget (xml, "palette-combobox");
      g_assert (w);
      init_palette_scheme_menu (w);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (palette_scheme_changed),
                        profile);

      profile_editor_update_background_type (editor, profile);
      w = glade_xml_get_widget (xml, "solid-radiobutton");
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (solid_radio_toggled),
                        profile);
      w = glade_xml_get_widget (xml, "image-radiobutton");
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (image_radio_toggled),
                        profile);
      w = glade_xml_get_widget (xml, "transparent-radiobutton");
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (transparent_radio_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "background-image-filechooser");
      profile_editor_update_background_image (editor, profile);
      g_signal_connect (G_OBJECT (w), "selection-changed",
                        G_CALLBACK (background_image_changed),
                        profile);
      setup_background_filechooser (w, profile);

      w = glade_xml_get_widget (xml, "scroll-background-checkbutton");
      profile_editor_update_scroll_background (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (scroll_background_toggled),
                        profile);
      
      w = glade_xml_get_widget (xml, "darken-background-scale");
      init_background_darkness_scale (w);
      profile_editor_update_background_darkness (editor, profile);
      g_signal_connect (G_OBJECT (w), "value_changed",
                        G_CALLBACK (darken_background_value_changed),
                        profile);

      w = glade_xml_get_widget (xml, "backspace-binding-combobox");
      profile_editor_update_backspace_binding (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (backspace_binding_changed),
                        profile);

      w = glade_xml_get_widget (xml, "delete-binding-combobox");
      profile_editor_update_delete_binding (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (delete_binding_changed),
                        profile);

      w = glade_xml_get_widget (xml, "use-theme-colors-checkbutton");
      profile_editor_update_use_theme_colors (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (use_theme_colors_toggled),
                        profile);
      
      w = glade_xml_get_widget (xml, "system-font-checkbutton");
      profile_editor_update_use_system_font (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (use_system_font_toggled),
                        profile);

      
      i = 0;
      while (i < TERMINAL_PALETTE_SIZE)
        {
          char *s = g_strdup_printf ("palette-colorpicker-%d", i+1);
          
          w = glade_xml_get_widget (xml, s);
          g_assert (w);
          
          g_object_set_data (G_OBJECT (w),
                             "palette-entry-index",
                             GINT_TO_POINTER (i));

          g_signal_connect (G_OBJECT (w), "color_set",
                            G_CALLBACK (palette_color_set),
                            profile);
          
          g_free (s);

          ++i;
        }

      profile_editor_update_palette (editor, profile);

      w = glade_xml_get_widget (xml, "font-hbox");

      if (terminal_widget_supports_pango_fonts ())
        {
          GtkWidget *font_label;
          
          fontsel = gtk_font_button_new ();
          g_object_set_data (G_OBJECT (editor), "font-selector", fontsel);

          gtk_font_button_set_title (GTK_FONT_BUTTON (fontsel), _("Choose A Terminal Font"));
          gtk_font_button_set_show_size (GTK_FONT_BUTTON (fontsel), TRUE);
          gtk_font_button_set_show_style (GTK_FONT_BUTTON (fontsel), FALSE);
          gtk_font_button_set_use_font (GTK_FONT_BUTTON (fontsel), TRUE);
          gtk_font_button_set_use_size (GTK_FONT_BUTTON (fontsel), FALSE);

          profile_editor_update_font (editor, profile);
          g_signal_connect (G_OBJECT (fontsel), "font_set",
                            G_CALLBACK (font_set),
                            profile);

          font_label = gtk_label_new_with_mnemonic (_("_Font:"));
          gtk_misc_set_alignment (GTK_MISC (font_label), 0.0, 0.5);
          gtk_label_set_mnemonic_widget (GTK_LABEL (font_label), fontsel);

          gtk_box_set_spacing (GTK_BOX (w), 12);
          
          gtk_box_pack_start (GTK_BOX (w), GTK_WIDGET (font_label), FALSE, FALSE, 0);
          gtk_box_pack_start (GTK_BOX (w), GTK_WIDGET (fontsel), FALSE, FALSE, 0);

          size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
          gtk_size_group_add_widget (size_group,
                                     font_label);
          gtk_size_group_add_widget (size_group,
                                     glade_xml_get_widget (xml,
                                                           "profile-name-label"));
          gtk_size_group_add_widget (size_group,
                                     glade_xml_get_widget (xml,
                                                           "profile-icon-label"));
          g_object_unref (G_OBJECT (size_group));
        }
      else
        {
          fontsel = egg_xfont_selector_new (_("Choose A Terminal Font"));
          g_object_set_data (G_OBJECT (editor), "font-selector", fontsel);
 
          profile_editor_update_x_font (editor, profile);
      
          g_signal_connect (fontsel, "changed",
                            G_CALLBACK (font_changed), profile);

          size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
          gtk_size_group_add_widget (size_group,
                                     EGG_XFONT_SELECTOR (fontsel)->family_label);
          gtk_size_group_add_widget (size_group,
                                     glade_xml_get_widget (xml,
                                                           "profile-name-label"));
          gtk_size_group_add_widget (size_group,
                                     glade_xml_get_widget (xml,
                                                           "profile-icon-label"));
          g_object_unref (G_OBJECT (size_group));

          gtk_box_pack_start (GTK_BOX (w), GTK_WIDGET (fontsel), TRUE, TRUE, 0);
        }
      
      gtk_widget_show_all (w);
      
      w = glade_xml_get_widget (xml, "reset-compat-defaults-button");
      g_signal_connect (G_OBJECT (w), "clicked",
			G_CALLBACK (reset_compat_defaults_clicked),
			profile);

      terminal_util_set_unique_role (GTK_WINDOW (editor), "gnome-terminal-profile-editor");
    }
  else
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (editor));
    }

  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (editor),
                                    GTK_WINDOW (transient_parent));
      gtk_widget_hide (editor); /* re-show the window on its new parent */
    }
  
  /*   gtk_widget_show_all (editor);*/
  gtk_window_present (GTK_WINDOW (editor));
}

static void
set_insensitive (GtkWidget  *editor,
                 const char *widget_name,
                 gboolean    setting)
{
  GtkWidget *w;  

  w = profile_editor_get_widget (editor, widget_name);

  gtk_widget_set_sensitive (w, !setting);
}

static void
profile_editor_update_sensitivity (GtkWidget       *editor,
                                   TerminalProfile *profile)
{
  const TerminalSettingMask *mask;
  GtkWidget *w;
  
  /* the first time in this function the object data is unset
   * thus the last mask is 0, which means everything is sensitive,
   * which is what we want.
   */
  /* disabled for now for testing */
#if 0
  TerminalSettingMask last_mask;
  last_mask = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (editor),
                                                  "cached-lock-mask"));
#endif
  
  mask = terminal_profile_get_locked_settings (profile);

  /* This one can't be short-circuited by the cache, since
   * it depends on settings
   */
  w = profile_editor_get_widget (editor, "custom-command-entry");
  gtk_widget_set_sensitive (w,
                            !((mask->custom_command) ||
                              !terminal_profile_get_use_custom_command (profile))); 

  w = profile_editor_get_widget (editor, "custom-command-entry-label");
  gtk_widget_set_sensitive (w,
                            !((mask->custom_command) ||
                              !terminal_profile_get_use_custom_command (profile))); 
  if (terminal_profile_get_background_type (profile) == TERMINAL_BACKGROUND_IMAGE)
    {
      w = profile_editor_get_widget (editor, "background-image-filechooser");
      gtk_widget_set_sensitive (w, !(mask->background_image_file));
      w = profile_editor_get_widget (editor, "background-image-filechooser-label");
      gtk_widget_set_sensitive (w, !(mask->background_image_file));
      w = profile_editor_get_widget (editor, "scroll-background-checkbutton");
      gtk_widget_set_sensitive (w, !(mask->scroll_background));
      w = profile_editor_get_widget (editor, "darken-background-vbox");
      gtk_widget_set_sensitive (w, !(mask->background_darkness));
    }
  else if (terminal_profile_get_background_type (profile) == TERMINAL_BACKGROUND_TRANSPARENT) 
  {
      w = profile_editor_get_widget (editor, "darken-background-vbox");
      gtk_widget_set_sensitive (w, !(mask->background_darkness));
      w = profile_editor_get_widget (editor, "background-image-filechooser");
      gtk_widget_set_sensitive (w, FALSE);
      w = profile_editor_get_widget (editor, "background-image-filechooser-label");
      gtk_widget_set_sensitive (w, FALSE);
      w = profile_editor_get_widget (editor, "scroll-background-checkbutton");
      gtk_widget_set_sensitive (w, FALSE);
      
    }
  else
    {
      w = profile_editor_get_widget (editor, "background-image-filechooser");
      gtk_widget_set_sensitive (w, FALSE);
      w = profile_editor_get_widget (editor, "background-image-filechooser-label");
      gtk_widget_set_sensitive (w, FALSE);
      w = profile_editor_get_widget (editor, "scroll-background-checkbutton");
      gtk_widget_set_sensitive (w, FALSE);
      w = profile_editor_get_widget (editor, "darken-background-vbox");
      gtk_widget_set_sensitive (w, FALSE);
    }

  if (!terminal_profile_get_use_system_font (profile))
    {
      if (terminal_widget_supports_pango_fonts ())
        set_insensitive (editor, "font-hbox", mask->font);
      else
        set_insensitive (editor, "font-hbox", mask->x_font);
    }
  else
    {
      w = profile_editor_get_widget (editor, "font-hbox");
      gtk_widget_set_sensitive (w, FALSE);
    }


  if (terminal_profile_get_use_theme_colors (profile))
    {
      set_insensitive (editor, "foreground-colorpicker", TRUE);
      set_insensitive (editor, "foreground-colorpicker-label", TRUE);
      set_insensitive (editor, "background-colorpicker", TRUE);
      set_insensitive (editor, "background-colorpicker-label", TRUE);
      set_insensitive (editor, "color-scheme-combobox", TRUE);
      set_insensitive (editor, "color-scheme-combobox-label", TRUE);
    }
  else
    {      
      set_insensitive (editor, "foreground-colorpicker",
                       mask->foreground_color);
      
      set_insensitive (editor, "foreground-colorpicker-label",
                       mask->foreground_color);
		             
      set_insensitive (editor, "background-colorpicker",
                       mask->background_color);
      
      set_insensitive (editor, "background-colorpicker-label",
                       mask->background_color);      

      set_insensitive (editor, "color-scheme-combobox",
                       (mask->background_color) ||
                       (mask->foreground_color));

      set_insensitive (editor, "color-scheme-combobox-label",
                       (mask->background_color) ||
                       (mask->foreground_color));		       
		       
    }
  
#if 0 /* uncomment once we've tested the sensitivity code */
  if (mask == last_mask)
    return;
  g_object_set_data (G_OBJECT (editor), "cached-lock-mask",
                     GINT_TO_POINTER (mask));
#endif
  
  set_insensitive (editor, "profile-name-entry",
                   mask->visible_name);

  set_insensitive (editor, "profile-icon-entry",
                   mask->icon_file);
  
  set_insensitive (editor, "blink-cursor-checkbutton",
                   mask->cursor_blink);

  set_insensitive (editor, "show-menubar-checkbutton",
                   mask->default_show_menubar);

  set_insensitive (editor, "title-entry",
                   mask->title);
  
  set_insensitive (editor, "title-mode-combobox",
                   mask->title_mode);

  set_insensitive (editor, "allow-bold-checkbutton",
                   mask->allow_bold);

  set_insensitive (editor, "bell-checkbutton",
                   mask->silent_bell);

  set_insensitive (editor, "word-chars-entry",
                   mask->word_chars);

  set_insensitive (editor, "scrollbar-position-combobox",
                   mask->scrollbar_position);

  set_insensitive (editor, "scrollback-lines-spinbutton",
                   mask->scrollback_lines);

  set_insensitive (editor, "scrollback-kilobytes-spinbutton",
                   mask->scrollback_lines);
  
  set_insensitive (editor, "scroll-on-keystroke-checkbutton",
                   mask->scroll_on_keystroke);

  set_insensitive (editor, "scroll-on-output-checkbutton",
                   mask->scroll_on_output);

  set_insensitive (editor, "exit-action-combobox",
                   mask->exit_action);

  set_insensitive (editor, "login-shell-checkbutton",
                   mask->login_shell);

  set_insensitive (editor, "update-records-checkbutton",
                   mask->update_records);

  set_insensitive (editor, "use-custom-command-checkbutton",
                   mask->use_custom_command);

  set_insensitive (editor, "palette-combobox",
                   mask->palette);

  set_insensitive (editor, "solid-radiobutton",
                   mask->background_type);
  set_insensitive (editor, "image-radiobutton",
                   mask->background_type);
  set_insensitive (editor, "transparent-radiobutton",
                   mask->background_type);

  set_insensitive (editor, "backspace-binding-combobox",
                   mask->backspace_binding);
  set_insensitive (editor, "delete-binding-combobox",
                   mask->delete_binding);

  set_insensitive (editor, "use-theme-colors-checkbutton",
                   mask->use_theme_colors);

  set_insensitive (editor, "system-font-checkbutton",
                   mask->use_system_font);

  
  {
    int i;

    i = 0;
    while (i < TERMINAL_PALETTE_SIZE)
      {
        char *s = g_strdup_printf ("palette-colorpicker-%d", i+1);

        set_insensitive (editor, s, mask->palette);

        g_free (s);

        ++i;
      }
  }
}


static void
profile_editor_update_visible_name (GtkWidget       *editor,
                                    TerminalProfile *profile)
{
  char *s;
  GtkWidget *w;
  
  s = g_strdup_printf (_("Editing Profile \"%s\""),
                       terminal_profile_get_visible_name (profile));
  
  gtk_window_set_title (GTK_WINDOW (editor), s);
  
  g_free (s);

  w = profile_editor_get_widget (editor, "profile-name-entry");

  entry_set_text_if_changed (GTK_ENTRY (w),
                             terminal_profile_get_visible_name (profile));
}

static void
profile_editor_update_icon (GtkWidget       *editor,
                            TerminalProfile *profile)
{
  GtkWidget *w;
  char *current_filename;
  const char *profile_filename;
  
  w = profile_editor_get_widget (editor, "profile-icon-entry");

  current_filename = gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (w));

  profile_filename = terminal_profile_get_icon_file (profile);
  
  if (current_filename && profile_filename &&
      strcmp (current_filename, profile_filename) == 0)
    return;

  g_free (current_filename);
  
  gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (w), profile_filename);
}

static void
profile_editor_update_cursor_blink (GtkWidget       *editor,
                                    TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "blink-cursor-checkbutton");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_cursor_blink (profile));
}


static void
profile_editor_update_default_show_menubar (GtkWidget       *editor,
                                            TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "show-menubar-checkbutton");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_default_show_menubar (profile));
}

static void
profile_editor_update_color_pickers (GtkWidget       *editor,
                                     TerminalProfile *profile)
{
  GtkWidget *w;
  GdkColor color1, color2;
  
  terminal_profile_get_color_scheme (profile, &color1, &color2);
  
  w = profile_editor_get_widget (editor, "foreground-colorpicker");
  colorpicker_set_if_changed (w, &color1);
  
  w = profile_editor_get_widget (editor, "background-colorpicker");
  colorpicker_set_if_changed (w, &color2);
}

static void
profile_editor_update_color_scheme_menu (GtkWidget       *editor,
                                         TerminalProfile *profile)
{
  GdkColor fg, bg;
  int i;
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "color-scheme-combobox");
  
  terminal_profile_get_color_scheme (profile, &fg, &bg);

  i = 0;
  while (i < G_N_ELEMENTS (color_schemes))
    {
      if (gdk_color_equal (&color_schemes[i].foreground,
                           &fg) &&
          gdk_color_equal (&color_schemes[i].background,
                           &bg))
        break;
      ++i;
    }

  /* If we didn't find a match, then we want the last combo
   * box item which is "custom"
   */
  gtk_combo_box_set_active (GTK_COMBO_BOX (w), i);
}

static void
profile_editor_update_title (GtkWidget       *editor,
                             TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "title-entry");

  entry_set_text_if_changed (GTK_ENTRY (w),
                             terminal_profile_get_title (profile));
}

static void
profile_editor_update_title_mode (GtkWidget       *editor,
                                  TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "title-mode-combobox");
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (w),
                            terminal_profile_get_title_mode (profile));
}

static void
profile_editor_update_allow_bold (GtkWidget       *editor,
                                  TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "allow-bold-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_allow_bold (profile));
}

static void
profile_editor_update_silent_bell (GtkWidget       *editor,
                                   TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "bell-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                !terminal_profile_get_silent_bell (profile));
}

static void
profile_editor_update_word_chars (GtkWidget       *editor,
                                  TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "word-chars-entry");

  entry_set_text_if_changed (GTK_ENTRY (w),
                             terminal_profile_get_word_chars (profile));
}

static void
profile_editor_update_scrollbar_position   (GtkWidget       *editor,
                                            TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "scrollbar-position-combobox");
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (w),
                            terminal_profile_get_scrollbar_position (profile));
}

static void
profile_editor_update_scrollback_lines (GtkWidget       *editor,
                                        TerminalProfile *profile)
{
  GtkWidget *w;
  int lines;

  lines = terminal_profile_get_scrollback_lines (profile);
  
  w = profile_editor_get_widget (editor, "scrollback-lines-spinbutton");

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), lines);

  w = profile_editor_get_widget (editor, "scrollback-kilobytes-spinbutton");
  
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), (BYTES_PER_LINE * lines) / 1024);
}

static void
profile_editor_update_scroll_on_keystroke  (GtkWidget       *editor,
                                            TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "scroll-on-keystroke-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_scroll_on_keystroke (profile));
}

static void
profile_editor_update_scroll_on_output (GtkWidget       *editor,
                                        TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "scroll-on-output-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_scroll_on_output (profile));
}

static void
profile_editor_update_exit_action (GtkWidget       *editor,
                                   TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "exit-action-combobox");
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (w),
                            terminal_profile_get_exit_action (profile));
}

static void
profile_editor_update_login_shell (GtkWidget       *editor,
                                   TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "login-shell-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_login_shell (profile));
}

static void
profile_editor_update_update_records (GtkWidget       *editor,
                                      TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "update-records-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                               terminal_profile_get_update_records (profile));

}

static void
profile_editor_update_use_custom_command (GtkWidget       *editor,
                                          TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "use-custom-command-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                               terminal_profile_get_use_custom_command (profile));

}

static void
profile_editor_update_custom_command (GtkWidget       *editor,
                                      TerminalProfile *profile)
{
  GtkWidget *w;
  const char *command;

  w = profile_editor_get_widget (editor, "custom-command-entry");

  command = terminal_profile_get_custom_command (profile);
  
  entry_set_text_if_changed (GTK_ENTRY (w), command);

  /* FIXME get error from this and display it in a tooltip
   * or label
   */
  if (g_shell_parse_argv (command, NULL, NULL, NULL))
    {
      GtkRcStyle *mod;

      mod = gtk_widget_get_modifier_style (w);
      if (mod)
        mod->color_flags[GTK_STATE_NORMAL] &= ~GTK_RC_TEXT;

      gtk_widget_modify_style (w, mod);
      /* caution, mod destroyed at this point */
    }
  else
    {
      GdkColor color;
      gdk_color_parse ("red", &color);
      gtk_widget_modify_text (w, GTK_STATE_NORMAL, &color);
    }            
}

static void
profile_editor_update_palette (GtkWidget       *editor,
                               TerminalProfile *profile)
{
  GtkWidget *w;
  int i;
  GdkColor palette[TERMINAL_PALETTE_SIZE];

  terminal_profile_get_palette (profile, palette);
  
  i = 0;
  while (i < TERMINAL_PALETTE_SIZE)
    {
      char *s = g_strdup_printf ("palette-colorpicker-%d", i+1);
      w = profile_editor_get_widget (editor, s);
      g_free (s);
      
      colorpicker_set_if_changed (w, &palette[i]);
      
      ++i;
    }

  w = profile_editor_get_widget (editor, "palette-combobox");

  i = 0;
  while (i < G_N_ELEMENTS (palette_schemes))
    {
      int j;
      gboolean match;

      match = TRUE;
      j = 0;
      while (j < TERMINAL_PALETTE_SIZE)
        {
          if (!gdk_color_equal (&palette_schemes[i].palette[j],
                                &palette[j]))
            {
              match = FALSE;
              break;
            }
          
          ++j;
        }

      if (match)
        break;
      
      ++i;
    }

  /* If we didn't find a match, then we want the last combo
   * box item which is "custom"
   */
  gtk_combo_box_set_active (GTK_COMBO_BOX (w), i);
}

static void
profile_editor_update_x_font (GtkWidget       *editor,
                              TerminalProfile *profile)
{
  GtkWidget *fontsel;
  char *spacings[] = { "m", "c", NULL };
  char *slants[] = { "r", "ot", NULL };
  char *weights[] = { "medium", "regular", "demibold", NULL };
  gchar *name;

  if (terminal_widget_supports_pango_fonts ())
    return;
  
  fontsel = g_object_get_data (G_OBJECT (editor), "font-selector");

  /* If the current selector font is the same as the new font,
   * don't do any work.
   */
  name = egg_xfont_selector_get_font_name (EGG_XFONT_SELECTOR (fontsel));
  
  if (name == NULL)
    return;

  if (strcmp (name, terminal_profile_get_x_font (profile)) == 0)
    {
      g_free (name);
      return;
    }
  
  g_free (name);
  
  egg_xfont_selector_set_font_name (EGG_XFONT_SELECTOR (fontsel),
				    terminal_profile_get_x_font (profile));

  egg_xfont_selector_set_filter (EGG_XFONT_SELECTOR (fontsel),
				 EGG_XFONT_FILTER_BASE,
				 EGG_XFONT_ALL,
				 NULL,
				 weights,
				 slants,
				 NULL,
				 spacings,
				 NULL);
}

static void
profile_editor_update_background_type (GtkWidget       *editor,
                                       TerminalProfile *profile)
{
  GtkWidget *w;

  switch (terminal_profile_get_background_type (profile))
    {
    case TERMINAL_BACKGROUND_SOLID:
      w = profile_editor_get_widget (editor, "solid-radiobutton");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
      break;
    case TERMINAL_BACKGROUND_IMAGE:
      w = profile_editor_get_widget (editor, "image-radiobutton");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
      break;
    case TERMINAL_BACKGROUND_TRANSPARENT:
      w = profile_editor_get_widget (editor, "transparent-radiobutton");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
      break;
    }
}

static void
profile_editor_update_background_image (GtkWidget       *editor,
                                        TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "background-image-filechooser");

  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (w),
                                 terminal_profile_get_background_image_file (profile));
}

static void
profile_editor_update_scroll_background (GtkWidget       *editor,
                                         TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "scroll-background-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                               terminal_profile_get_scroll_background (profile));
}

static void
profile_editor_update_background_darkness (GtkWidget       *editor,
                                           TerminalProfile *profile)
{
  GtkWidget *w;
  double v;

  w = profile_editor_get_widget (editor, "darken-background-scale");

  v = terminal_profile_get_background_darkness (profile);
  
  gtk_range_set_value (GTK_RANGE (w), v);
}

static void
profile_editor_update_backspace_binding (GtkWidget       *editor,
                                         TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "backspace-binding-combobox");
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (w),
                            terminal_profile_get_backspace_binding (profile));
}

static void
profile_editor_update_delete_binding (GtkWidget       *editor,
                                      TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "delete-binding-combobox");
  
  gtk_combo_box_set_active (GTK_COMBO_BOX (w),
                            terminal_profile_get_delete_binding (profile));
}

static void
profile_editor_update_use_theme_colors (GtkWidget       *editor,
                                        TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "use-theme-colors-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_use_theme_colors (profile));
}

static void
profile_editor_update_use_system_font (GtkWidget       *editor,
                                       TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "system-font-checkbutton");
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
                                terminal_profile_get_use_system_font (profile));
}

static void
profile_editor_update_font (GtkWidget       *editor,
                            TerminalProfile *profile)
{
  GtkWidget *w;

  if (!terminal_widget_supports_pango_fonts ())
    return;
  
  w = g_object_get_data (G_OBJECT (editor), "font-selector");

  fontpicker_set_if_changed (w,
                             terminal_profile_get_font (profile));
}

static GtkWidget*
profile_editor_get_widget (GtkWidget  *editor,
                           const char *widget_name)
{
  GladeXML *xml;
  GtkWidget *w;
  
  xml = g_object_get_data (G_OBJECT (editor),
                           "glade-xml");

  g_return_val_if_fail (xml, NULL);
  
  w = glade_xml_get_widget (xml, widget_name);

  if (w == NULL)
    g_error ("No such widget %s", widget_name);
  
  return w;
}
