/*
 * Copyright © 2002 Havoc Pennington
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
#include <math.h>

#include <glib.h>
#include <gio/gio.h>

#include "terminal-intl.h"
#include "profile-editor.h"
#include "terminal-util.h"

typedef struct _TerminalColorScheme TerminalColorScheme;

struct _TerminalColorScheme
{
  const char *name;
  const GdkColor foreground;
  const GdkColor background;
};

static const TerminalColorScheme color_schemes[] = {
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

static void profile_forgotten_cb (TerminalProfile           *profile,
                                  GtkWidget                 *editor);

static void profile_notify_sensitivity_cb (TerminalProfile *profile,
                                           GParamSpec *pspec,
                                           GtkWidget *editor);

static void profile_colors_notify_scheme_combo_cb (TerminalProfile *profile,
                                                   GParamSpec *pspec,
                                                   GtkComboBox *combo);

static void profile_palette_notify_scheme_combo_cb (TerminalProfile *profile,
                                                    GParamSpec *pspec,
                                                    GtkComboBox *combo);

static void profile_palette_notify_colorpickers_cb (TerminalProfile *profile,
                                                    GParamSpec *pspec,
                                                    GtkWidget *editor);

static GtkWidget*
profile_editor_get_widget (GtkWidget  *editor,
                           const char *widget_name)
{
  GtkBuilder *builder;

  builder = g_object_get_data (G_OBJECT (editor), "builder");
  g_assert (builder != NULL);
  
  return (GtkWidget *) gtk_builder_get_object  (builder, widget_name);
}

static void
widget_and_labels_set_sensitive (GtkWidget *widget, gboolean sensitive)
{
  GList *labels, *i;

  labels = gtk_widget_list_mnemonic_labels (widget);
  for (i = labels; i; i = i->next)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (i->data), sensitive);
    }
  g_list_free (labels);

  gtk_widget_set_sensitive (widget, sensitive);
}

static void
profile_forgotten_cb (TerminalProfile *profile,
                      GtkWidget *editor)
{
  gtk_widget_destroy (editor);
}

static void
profile_notify_sensitivity_cb (TerminalProfile *profile,
                               GParamSpec *pspec,
                               GtkWidget *editor)
{
  TerminalBackgroundType bg_type;
  const char *prop_name;

  if (pspec)
    prop_name = pspec->name;
  else
    prop_name = NULL;
  
#define SET_SENSITIVE(name, setting) widget_and_labels_set_sensitive (profile_editor_get_widget (editor, name), setting)

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_USE_CUSTOM_COMMAND) ||
      prop_name == I_(TERMINAL_PROFILE_CUSTOM_COMMAND))
    {
      gboolean use_custom_command_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND);
      SET_SENSITIVE ("use-custom-command-checkbutton", !use_custom_command_locked);
      SET_SENSITIVE ("custom-command-box",
                     terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND) &&
                     !terminal_profile_property_locked (profile, TERMINAL_PROFILE_CUSTOM_COMMAND));
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BACKGROUND_TYPE))
    {
      gboolean bg_type_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_BACKGROUND_TYPE);
      SET_SENSITIVE ("solid-radiobutton", !bg_type_locked);
      SET_SENSITIVE ("image-radiobutton", !bg_type_locked);
      SET_SENSITIVE ("transparent-radiobutton", !bg_type_locked);

      bg_type = terminal_profile_get_property_enum (profile, TERMINAL_PROFILE_BACKGROUND_TYPE);
      if (bg_type == TERMINAL_BACKGROUND_IMAGE)
        {
          SET_SENSITIVE ("background-image-filechooser", !terminal_profile_property_locked (profile, TERMINAL_PROFILE_BACKGROUND_IMAGE_FILE));
          SET_SENSITIVE ("scroll-background-checkbutton", !terminal_profile_property_locked (profile, TERMINAL_PROFILE_SCROLL_BACKGROUND));
          SET_SENSITIVE ("darken-background-vbox", !terminal_profile_property_locked (profile, TERMINAL_PROFILE_BACKGROUND_DARKNESS));
        }
      else if (bg_type == TERMINAL_BACKGROUND_TRANSPARENT)
        {
          SET_SENSITIVE ("background-image-filechooser", FALSE);
          SET_SENSITIVE ("scroll-background-checkbutton", FALSE);
          SET_SENSITIVE ("darken-background-vbox", !terminal_profile_property_locked (profile, TERMINAL_PROFILE_BACKGROUND_DARKNESS));
        }
      else
        {
          SET_SENSITIVE ("background-image-filechooser", FALSE);
          SET_SENSITIVE ("scroll-background-checkbutton", FALSE);
          SET_SENSITIVE ("darken-background-vbox", FALSE);
        }
    }

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_USE_SYSTEM_FONT) ||
      prop_name == I_(TERMINAL_PROFILE_FONT))
    {
      SET_SENSITIVE ("font-hbox",
                    !terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT) &&
                    !terminal_profile_property_locked (profile, TERMINAL_PROFILE_FONT));
      SET_SENSITIVE ("system-font-checkbutton",
                     !terminal_profile_property_locked (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT));
    }

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_FOREGROUND_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG) ||
      prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS))
    {
      gboolean bg_locked, use_theme_colors, fg_locked;

      use_theme_colors = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_THEME_COLORS);
      fg_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_FOREGROUND_COLOR);
      bg_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_BACKGROUND_COLOR);

      SET_SENSITIVE ("foreground-colorpicker", !use_theme_colors && !fg_locked);
      SET_SENSITIVE ("foreground-colorpicker-label", !use_theme_colors && !fg_locked);
      SET_SENSITIVE ("background-colorpicker", !use_theme_colors && !bg_locked);
      SET_SENSITIVE ("background-colorpicker-label", !use_theme_colors && !bg_locked);
      SET_SENSITIVE ("color-scheme-combobox", !use_theme_colors && !fg_locked && !bg_locked);
      SET_SENSITIVE ("color-scheme-combobox-label", !use_theme_colors && !fg_locked && !bg_locked);
    }

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG) ||
      prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS))
    {
      gboolean bold_locked, bold_same_as_fg_locked, bold_same_as_fg, use_theme_colors;

      use_theme_colors = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_THEME_COLORS);
      bold_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_BOLD_COLOR);
      bold_same_as_fg_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG);
      bold_same_as_fg = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG);

      SET_SENSITIVE ("bold-color-same-as-fg-checkbox", !use_theme_colors && !bold_same_as_fg_locked);
      SET_SENSITIVE ("bold-colorpicker", !use_theme_colors && !bold_locked && !bold_same_as_fg);
      SET_SENSITIVE ("bold-colorpicker-label", !use_theme_colors && ((!bold_same_as_fg && !bold_locked) || !bold_same_as_fg_locked));
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_VISIBLE_NAME))
    SET_SENSITIVE ("profile-name-entry",
                  !terminal_profile_property_locked (profile, TERMINAL_PROFILE_VISIBLE_NAME));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_DEFAULT_SHOW_MENUBAR))
    SET_SENSITIVE ("show-menubar-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_DEFAULT_SHOW_MENUBAR));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_TITLE))
    SET_SENSITIVE ("title-entry",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_TITLE));
  
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_TITLE_MODE))
    SET_SENSITIVE ("title-mode-combobox",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_TITLE_MODE));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ALLOW_BOLD))
    SET_SENSITIVE ("allow-bold-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_ALLOW_BOLD));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SILENT_BELL))
    SET_SENSITIVE ("bell-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_SILENT_BELL));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_WORD_CHARS))
    SET_SENSITIVE ("word-chars-entry",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_WORD_CHARS));

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE) ||
      prop_name == I_(TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS) ||
      prop_name == I_(TERMINAL_PROFILE_DEFAULT_SIZE_ROWS))
    {
      gboolean use_custom_default_size_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE);
      gboolean use_custom_default_size = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE);
      gboolean columns_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS);
      gboolean rows_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS);

      SET_SENSITIVE ("use-custom-default-size-checkbutton", !use_custom_default_size_locked);
      SET_SENSITIVE ("default-size-hbox", use_custom_default_size);
      SET_SENSITIVE ("default-size-label", (!columns_locked || !rows_locked));
      SET_SENSITIVE ("default-size-columns-label", !columns_locked);
      SET_SENSITIVE ("default-size-columns-spinbutton", !columns_locked);
      SET_SENSITIVE ("default-size-rows-label", !rows_locked);
      SET_SENSITIVE ("default-size-rows-spinbutton", !rows_locked);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLLBAR_POSITION))
    SET_SENSITIVE ("scrollbar-position-combobox",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_SCROLLBAR_POSITION));

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_LINES) ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_UNLIMITED))
    {
      gboolean scrollback_lines_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_SCROLLBACK_LINES);
      gboolean scrollback_unlimited_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED);
      gboolean scrollback_unlimited = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED);

      SET_SENSITIVE ("scrollback-label", !scrollback_lines_locked);
      SET_SENSITIVE ("scrollback-box", !scrollback_lines_locked && !scrollback_unlimited);
      SET_SENSITIVE ("scrollback-unlimited-checkbutton", !scrollback_lines_locked && !scrollback_unlimited_locked);
    }
  
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE))
    SET_SENSITIVE ("scroll-on-keystroke-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_OUTPUT))
    SET_SENSITIVE ("scroll-on-output-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_SCROLL_ON_OUTPUT));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_EXIT_ACTION))
    SET_SENSITIVE ("exit-action-combobox",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_EXIT_ACTION));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_LOGIN_SHELL))
    SET_SENSITIVE ("login-shell-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_LOGIN_SHELL));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_UPDATE_RECORDS))
    SET_SENSITIVE ("update-records-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_UPDATE_RECORDS));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_PALETTE))
    {
      gboolean palette_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_PALETTE);
      SET_SENSITIVE ("palette-combobox", !palette_locked);
      SET_SENSITIVE ("palette-table", !palette_locked);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BACKSPACE_BINDING))
    SET_SENSITIVE ("backspace-binding-combobox",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_BACKSPACE_BINDING));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_DELETE_BINDING))
    SET_SENSITIVE ("delete-binding-combobox",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_DELETE_BINDING));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS))
    SET_SENSITIVE ("use-theme-colors-checkbutton",
                   !terminal_profile_property_locked (profile, TERMINAL_PROFILE_USE_THEME_COLORS));

#undef SET_INSENSITIVE
}

static void
color_scheme_combo_changed_cb (GtkWidget *combo,
                               GParamSpec *pspec,
                               TerminalProfile *profile)
{
  guint i;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (i < G_N_ELEMENTS (color_schemes))
    {
      g_signal_handlers_block_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
      g_object_set (profile,
                    TERMINAL_PROFILE_FOREGROUND_COLOR, &color_schemes[i].foreground,
                    TERMINAL_PROFILE_BACKGROUND_COLOR, &color_schemes[i].background,
                    NULL);
      g_signal_handlers_unblock_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
    }
  else
    {
      /* "custom" selected, no change */
    }
}

static void
profile_colors_notify_scheme_combo_cb (TerminalProfile *profile,
                                       GParamSpec *pspec,
                                       GtkComboBox *combo)
{
  const GdkColor *fg, *bg;
  guint i;

  fg = terminal_profile_get_property_boxed (profile, TERMINAL_PROFILE_FOREGROUND_COLOR);
  bg = terminal_profile_get_property_boxed (profile, TERMINAL_PROFILE_BACKGROUND_COLOR);

  if (fg && bg)
    {
      for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
	{
	  if (gdk_color_equal (fg, &color_schemes[i].foreground) &&
	      gdk_color_equal (bg, &color_schemes[i].background))
	    break;
	}
    }
  else
    {
      i = G_N_ELEMENTS (color_schemes);
    }
  /* If we didn't find a match, then we get the last combo box item which is "custom" */

  g_signal_handlers_block_by_func (combo, G_CALLBACK (color_scheme_combo_changed_cb), profile);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), i);
  g_signal_handlers_unblock_by_func (combo, G_CALLBACK (color_scheme_combo_changed_cb), profile);
}

static void
palette_scheme_combo_changed_cb (GtkComboBox *combo,
                                 GParamSpec *pspec,
                                 TerminalProfile *profile)
{
  int i;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  g_signal_handlers_block_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
  if (i < TERMINAL_PALETTE_N_BUILTINS)
    terminal_profile_set_palette_builtin (profile, i);
  else
    {
      /* "custom" selected, no change */
    }
  g_signal_handlers_unblock_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
}

static void
profile_palette_notify_scheme_combo_cb (TerminalProfile *profile,
                                        GParamSpec *pspec,
                                        GtkComboBox *combo)
{
  guint i;

  if (!terminal_profile_get_palette_is_builtin (profile, &i))
    /* If we didn't find a match, then we want the last combo
     * box item which is "custom"
     */
    i = TERMINAL_PALETTE_N_BUILTINS;

  g_signal_handlers_block_by_func (combo, G_CALLBACK (palette_scheme_combo_changed_cb), profile);
  gtk_combo_box_set_active (combo, i);
  g_signal_handlers_unblock_by_func (combo, G_CALLBACK (palette_scheme_combo_changed_cb), profile);
}

static void
palette_color_notify_cb (GtkColorButton *button,
                         GParamSpec *pspec,
                         TerminalProfile *profile)
{
  GtkWidget *editor;
  GdkColor color;
  guint i;

  gtk_color_button_get_color (button, &color);
  i = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "palette-entry-index"));

  editor = gtk_widget_get_toplevel (GTK_WIDGET (button));
  g_signal_handlers_block_by_func (profile, G_CALLBACK (profile_palette_notify_colorpickers_cb), editor);
  terminal_profile_modify_palette_entry (profile, i, &color);
  g_signal_handlers_unblock_by_func (profile, G_CALLBACK (profile_palette_notify_colorpickers_cb), editor);
}

static void
profile_palette_notify_colorpickers_cb (TerminalProfile *profile,
                                        GParamSpec *pspec,
                                        GtkWidget *editor)
{
  GtkWidget *w;
  GdkColor colors[TERMINAL_PALETTE_SIZE];
  guint n_colors, i;

  n_colors = G_N_ELEMENTS (colors);
  terminal_profile_get_palette (profile, colors, &n_colors);

  n_colors = MIN (n_colors, TERMINAL_PALETTE_SIZE);
  for (i = 0; i < n_colors; i++)
    {
      char name[32];
      GdkColor old_color;

      g_snprintf (name, sizeof (name), "palette-colorpicker-%d", i + 1);
      w = profile_editor_get_widget (editor, name);

      gtk_color_button_get_color (GTK_COLOR_BUTTON (w), &old_color);
      if (!gdk_color_equal (&old_color, &colors[i]))
        {
          g_signal_handlers_block_by_func (w, G_CALLBACK (palette_color_notify_cb), profile);
          gtk_color_button_set_color (GTK_COLOR_BUTTON (w), &colors[i]);
          g_signal_handlers_unblock_by_func (w, G_CALLBACK (palette_color_notify_cb), profile);
        }
    }
}

static void
custom_command_entry_changed_cb (GtkEntry *entry)
{
#if GTK_CHECK_VERSION (2, 16, 0)
  const char *command;
  GError *error = NULL;

  command = gtk_entry_get_text (entry);

  if (g_shell_parse_argv (command, NULL, NULL, &error))
    {
      gtk_entry_set_icon_from_stock (entry, GTK_PACK_END, NULL);
    }
  else
    {
      char *tooltip;

      gtk_entry_set_icon_from_stock (entry, GTK_PACK_END, GTK_STOCK_DIALOG_WARNING);

      tooltip = g_strdup_printf (_("Error parsing command: %s"), error->message);
      gtk_entry_set_icon_tooltip_text (entry, GTK_PACK_END, tooltip);
      g_free (tooltip);

      g_error_free (error);
    }
#endif /* GTK+ >= 2.16.0 */
}

static void
visible_name_entry_changed_cb (GtkEntry *entry,
                               GtkWindow *window)
{
  const char *visible_name;
  char *text;
  
  visible_name = gtk_entry_get_text (entry);

  text = g_strdup_printf (_("Editing Profile “%s”"), visible_name);
  gtk_window_set_title (window, text);
  g_free (text);
}

static void
reset_compat_defaults_cb (GtkWidget       *button,
                          TerminalProfile *profile)
{
  terminal_profile_reset_property (profile, TERMINAL_PROFILE_DELETE_BINDING);
  terminal_profile_reset_property (profile, TERMINAL_PROFILE_BACKSPACE_BINDING);
}

/*
 * initialize widgets
 */

static void
init_color_scheme_menu (GtkWidget *widget)
{
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GtkListStore *store;
  int i;

  store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
    gtk_list_store_insert_with_values (store, &iter, -1,
                                       0, _(color_schemes[i].name),
                                       -1);
  gtk_list_store_insert_with_values (store, &iter, -1,
                                      0, _("Custom"),
                                      -1);

  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer, "text", 0, NULL);
}

static char*
format_percent_value (GtkScale *scale,
                      double    val,
                      void     *data)
{
  return g_strdup_printf ("%d%%", (int) (val * 100.0 + 0.5));
}

static void
init_background_darkness_scale (GtkWidget *scale)
{
  g_signal_connect (scale, "format-value",
                    G_CALLBACK (format_percent_value),
                    NULL);
}

#if GTK_CHECK_VERSION (3, 1, 19)
static gboolean
font_family_is_monospace (const PangoFontFamily *family,
                          const PangoFontFace   *face,
                          gpointer               data)
{
  return pango_font_family_is_monospace ((PangoFontFamily *) family);
}

#endif

static void
editor_response_cb (GtkWidget *editor,
                    int response,
                    gpointer use_data)
{  
  if (response == GTK_RESPONSE_HELP)
    {
      terminal_util_show_help ("gnome-terminal-prefs", GTK_WINDOW (editor));
      return;
    }
    
  gtk_widget_destroy (editor);
}

#if 0
static GdkPixbuf *
create_preview_pixbuf (const gchar *filename)
{
  GdkPixbuf *pixbuf = NULL;
  GnomeThumbnailFactory *thumbs;
  const char *mime_type = NULL;
  GFile *gfile;
  GFileInfo *file_info;

  if (filename == NULL)
    return NULL;

  gfile = g_file_new_for_uri (filename);
  file_info = g_file_query_info (gfile,
                                  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                  0, NULL, NULL);
  if (file_info != NULL)
    mime_type = g_file_info_get_content_type (file_info);

  g_object_unref (gfile);

  if (mime_type != NULL)
    {
      thumbs = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);

      pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbs,
                                                           filename,
                                                           mime_type);
      g_object_unref (thumbs);
    }

  if (file_info != NULL)
    g_object_unref (file_info);

  return pixbuf;
}

static void 
update_image_preview (GtkFileChooser *chooser) 
{
  GtkWidget *image;
  gchar *file;

  image = gtk_file_chooser_get_preview_widget (GTK_FILE_CHOOSER (chooser));
  file = gtk_file_chooser_get_preview_uri (chooser);
  
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
  gtk_file_chooser_set_preview_widget_active (chooser, file != NULL);
}
#endif

static void
setup_background_filechooser (GtkWidget *filechooser, 
                              TerminalProfile *profile)
{
  GtkFileFilter *filter;
  const char *home_dir;

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_filter_set_name (filter, _("Images"));
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (filechooser), filter);

  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (filechooser), TRUE);

  /* Start filechooser in $HOME instead of the current dir of the factory which is "/" */
  home_dir = g_get_home_dir ();
  if (home_dir)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser), home_dir);

#if 0
  GtkWidget *image_preview;
  GdkPixbuf *pixbuf = NULL;

  image_preview = gtk_image_new ();
  /* FIXMchpe this is bogus */
  pixbuf = create_preview_pixbuf (terminal_profile_get_property_string (profile, TERMINAL_PROFILE_BACKGROUND_IMAGE_FILE));
  if (pixbuf != NULL)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (image_preview), pixbuf);
      g_object_unref (pixbuf);
    }
  else
    {
      gtk_image_set_from_stock (GTK_IMAGE (image_preview),
                                "gtk-dialog-question",
                                GTK_ICON_SIZE_DIALOG);
    }

  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (filechooser),
                                       image_preview);
  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (filechooser),
                                          FALSE);
  gtk_widget_set_size_request (image_preview, 128, -1);
  gtk_widget_show (image_preview); 

  g_signal_connect (filechooser, "update-preview",
                    G_CALLBACK (update_image_preview), NULL);
#endif
}

static void
profile_editor_destroyed (GtkWidget       *editor,
                          TerminalProfile *profile)
{
  g_signal_handlers_disconnect_by_func (profile, G_CALLBACK (profile_forgotten_cb), editor);
  g_signal_handlers_disconnect_by_func (profile, G_CALLBACK (profile_notify_sensitivity_cb), editor);
  g_signal_handlers_disconnect_matched (profile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                        G_CALLBACK (profile_colors_notify_scheme_combo_cb), NULL);
  g_signal_handlers_disconnect_matched (profile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                        G_CALLBACK (profile_palette_notify_scheme_combo_cb), NULL);
  g_signal_handlers_disconnect_matched (profile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                        G_CALLBACK (profile_palette_notify_colorpickers_cb), NULL);
  
  g_object_set_data (G_OBJECT (profile), "editor-window", NULL);
  g_object_set_data (G_OBJECT (editor), "builder", NULL);
}

static void
terminal_profile_editor_focus_widget (GtkWidget *editor,
                                      const char *widget_name)
{
  GtkBuilder *builder;
  GtkWidget *widget, *page, *page_parent;

  if (widget_name == NULL)
    return;

  builder = g_object_get_data (G_OBJECT (editor), "builder");
  widget = GTK_WIDGET (gtk_builder_get_object (builder, widget_name));
  if (widget == NULL)
    return;

  page = widget;
  while (page != NULL &&
         (page_parent = gtk_widget_get_parent (page)) != NULL &&
         !GTK_IS_NOTEBOOK (page_parent))
    page = page_parent;

  page_parent = gtk_widget_get_parent (page);
  if (page != NULL && GTK_IS_NOTEBOOK (page_parent)) {
    GtkNotebook *notebook;

    notebook = GTK_NOTEBOOK (page_parent);
    gtk_notebook_set_current_page (notebook, gtk_notebook_page_num (notebook, page));
  }

  if (gtk_widget_is_sensitive (widget))
    gtk_widget_grab_focus (widget);
}

/**
 * terminal_profile_edit:
 * @profile: a #TerminalProfile
 * @transient_parent: a #GtkWindow, or %NULL
 * @widget_name: a widget name in the profile editor's UI, or %NULL
 *
 * Shows the profile editor with @profile, anchored to @transient_parent.
 * If @widget_name is non-%NULL, focuses the corresponding widget and
 * switches the notebook to its containing page.
 */
void
terminal_profile_edit (TerminalProfile *profile,
                       GtkWindow       *transient_parent,
                       const char      *widget_name)
{
  char *path;
  GtkBuilder *builder;
  GError *error = NULL;
  GtkWidget *editor, *w;
  guint i;

  editor = g_object_get_data (G_OBJECT (profile), "editor-window");
  if (editor)
    {
      terminal_profile_editor_focus_widget (editor, widget_name);

      gtk_window_set_transient_for (GTK_WINDOW (editor),
                                    GTK_WINDOW (transient_parent));
      gtk_window_present (GTK_WINDOW (editor));
      return;
    }

  path = g_build_filename (TERM_PKGDATADIR, "profile-preferences.ui", NULL);
  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, path, &error)) {
    g_warning ("Failed to load %s: %s\n", path, error->message);
    g_error_free (error);
    g_free (path);
    g_object_unref (builder);
    return;
  }
  g_free (path);

  editor = (GtkWidget *) gtk_builder_get_object  (builder, "profile-editor-dialog");
  g_object_set_data_full (G_OBJECT (editor), "builder",
                          builder, (GDestroyNotify) g_object_unref);

  /* Store the dialogue on the profile, so we can acccess it above to check if
   * there's already a profile editor for this profile.
   */
  g_object_set_data (G_OBJECT (profile), "editor-window", editor);

  g_signal_connect (editor, "destroy",
                    G_CALLBACK (profile_editor_destroyed),
                    profile);

  g_signal_connect (editor, "response",
                    G_CALLBACK (editor_response_cb),
                    NULL);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "color-scheme-combobox");
  init_color_scheme_menu (w);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "darken-background-scale");
  init_background_darkness_scale (w);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "background-image-filechooser");
  setup_background_filechooser (w, profile);

  /* Hook up the palette colorpickers and combo box */

  for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
    {
      char name[32];
      char *text;

      g_snprintf (name, sizeof (name), "palette-colorpicker-%u", i + 1);
      w = (GtkWidget *) gtk_builder_get_object  (builder, name);

      g_object_set_data (G_OBJECT (w), "palette-entry-index", GUINT_TO_POINTER (i));

      text = g_strdup_printf (_("Choose Palette Color %d"), i + 1);
      gtk_color_button_set_title (GTK_COLOR_BUTTON (w), text);
      g_free (text);

      text = g_strdup_printf (_("Palette entry %d"), i + 1);
      gtk_widget_set_tooltip_text (w, text);
      g_free (text);

      g_signal_connect (w, "notify::color",
                        G_CALLBACK (palette_color_notify_cb),
                        profile);
    }

  profile_palette_notify_colorpickers_cb (profile, NULL, editor);
  g_signal_connect (profile, "notify::" TERMINAL_PROFILE_PALETTE,
                    G_CALLBACK (profile_palette_notify_colorpickers_cb),
                    editor);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "palette-combobox");
  g_signal_connect (w, "notify::active",
                    G_CALLBACK (palette_scheme_combo_changed_cb),
                    profile);

  profile_palette_notify_scheme_combo_cb (profile, NULL, GTK_COMBO_BOX (w));
  g_signal_connect (profile, "notify::" TERMINAL_PROFILE_PALETTE,
                    G_CALLBACK (profile_palette_notify_scheme_combo_cb),
                    w);

  /* Hook up the color scheme pickers and combo box */
  w = (GtkWidget *) gtk_builder_get_object  (builder, "color-scheme-combobox");
  g_signal_connect (w, "notify::active",
                    G_CALLBACK (color_scheme_combo_changed_cb),
                    profile);

  profile_colors_notify_scheme_combo_cb (profile, NULL, GTK_COMBO_BOX (w));
  g_signal_connect (profile, "notify::" TERMINAL_PROFILE_FOREGROUND_COLOR,
                    G_CALLBACK (profile_colors_notify_scheme_combo_cb),
                    w);
  g_signal_connect (profile, "notify::" TERMINAL_PROFILE_BACKGROUND_COLOR,
                    G_CALLBACK (profile_colors_notify_scheme_combo_cb),
                    w);

#define CONNECT_WITH_FLAGS(name, prop, flags) terminal_util_bind_object_property_to_widget (G_OBJECT (profile), prop, (GtkWidget *) gtk_builder_get_object (builder, name), flags)
#define CONNECT(name, prop) CONNECT_WITH_FLAGS (name, prop, 0)
#define SET_ENUM_VALUE(name, value) g_object_set_data (gtk_builder_get_object (builder, name), "enum-value", GINT_TO_POINTER (value))

  w = GTK_WIDGET (gtk_builder_get_object (builder, "custom-command-entry"));
  custom_command_entry_changed_cb (GTK_ENTRY (w));
  g_signal_connect (w, "changed",
                    G_CALLBACK (custom_command_entry_changed_cb), NULL);
  w = GTK_WIDGET (gtk_builder_get_object (builder, "profile-name-entry"));
  g_signal_connect (w, "changed",
                    G_CALLBACK (visible_name_entry_changed_cb), editor);

  g_signal_connect (gtk_builder_get_object  (builder, "reset-compat-defaults-button"),
                    "clicked",
                    G_CALLBACK (reset_compat_defaults_cb),
                    profile);

#if GTK_CHECK_VERSION (3, 1, 19)
  gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (gtk_builder_get_object (builder, "font-selector")),
                                    font_family_is_monospace, NULL, NULL);
#endif

  SET_ENUM_VALUE ("image-radiobutton", TERMINAL_BACKGROUND_IMAGE);
  SET_ENUM_VALUE ("solid-radiobutton", TERMINAL_BACKGROUND_SOLID);
  SET_ENUM_VALUE ("transparent-radiobutton", TERMINAL_BACKGROUND_TRANSPARENT);
  CONNECT ("allow-bold-checkbutton", TERMINAL_PROFILE_ALLOW_BOLD);
  CONNECT ("background-colorpicker", TERMINAL_PROFILE_BACKGROUND_COLOR);
  CONNECT ("background-image-filechooser", TERMINAL_PROFILE_BACKGROUND_IMAGE_FILE);
  CONNECT ("backspace-binding-combobox", TERMINAL_PROFILE_BACKSPACE_BINDING);
  CONNECT ("bold-color-same-as-fg-checkbox", TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG);
  CONNECT ("bold-colorpicker", TERMINAL_PROFILE_BOLD_COLOR);
  CONNECT ("cursor-shape-combobox", TERMINAL_PROFILE_CURSOR_SHAPE);
  CONNECT ("custom-command-entry", TERMINAL_PROFILE_CUSTOM_COMMAND);
  CONNECT ("darken-background-scale", TERMINAL_PROFILE_BACKGROUND_DARKNESS);
  CONNECT ("default-size-columns-spinbutton", TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS);
  CONNECT ("default-size-rows-spinbutton", TERMINAL_PROFILE_DEFAULT_SIZE_ROWS);
  CONNECT ("delete-binding-combobox", TERMINAL_PROFILE_DELETE_BINDING);
  CONNECT ("exit-action-combobox", TERMINAL_PROFILE_EXIT_ACTION);
  CONNECT ("font-selector", TERMINAL_PROFILE_FONT);
  CONNECT ("foreground-colorpicker", TERMINAL_PROFILE_FOREGROUND_COLOR);
  CONNECT ("image-radiobutton", TERMINAL_PROFILE_BACKGROUND_TYPE);
  CONNECT ("login-shell-checkbutton", TERMINAL_PROFILE_LOGIN_SHELL);
  CONNECT ("profile-name-entry", TERMINAL_PROFILE_VISIBLE_NAME);
  CONNECT ("scrollback-lines-spinbutton", TERMINAL_PROFILE_SCROLLBACK_LINES);
  CONNECT ("scrollback-unlimited-checkbutton", TERMINAL_PROFILE_SCROLLBACK_UNLIMITED);
  CONNECT ("scroll-background-checkbutton", TERMINAL_PROFILE_SCROLL_BACKGROUND);
  CONNECT ("scrollbar-position-combobox", TERMINAL_PROFILE_SCROLLBAR_POSITION);
  CONNECT ("scroll-on-keystroke-checkbutton", TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE);
  CONNECT ("scroll-on-output-checkbutton", TERMINAL_PROFILE_SCROLL_ON_OUTPUT);
  CONNECT ("show-menubar-checkbutton", TERMINAL_PROFILE_DEFAULT_SHOW_MENUBAR);
  CONNECT ("solid-radiobutton", TERMINAL_PROFILE_BACKGROUND_TYPE);
  CONNECT ("system-font-checkbutton", TERMINAL_PROFILE_USE_SYSTEM_FONT);
  CONNECT ("title-entry", TERMINAL_PROFILE_TITLE);
  CONNECT ("title-mode-combobox", TERMINAL_PROFILE_TITLE_MODE);
  CONNECT ("transparent-radiobutton", TERMINAL_PROFILE_BACKGROUND_TYPE);
  CONNECT ("update-records-checkbutton", TERMINAL_PROFILE_UPDATE_RECORDS);
  CONNECT ("use-custom-command-checkbutton", TERMINAL_PROFILE_USE_CUSTOM_COMMAND);
  CONNECT ("use-custom-default-size-checkbutton", TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE);
  CONNECT ("use-theme-colors-checkbutton", TERMINAL_PROFILE_USE_THEME_COLORS);
  CONNECT ("word-chars-entry", TERMINAL_PROFILE_WORD_CHARS);
  CONNECT_WITH_FLAGS ("bell-checkbutton", TERMINAL_PROFILE_SILENT_BELL, FLAG_INVERT_BOOL);

#undef CONNECT
#undef CONNECT_WITH_FLAGS
#undef SET_ENUM_VALUE

  profile_notify_sensitivity_cb (profile, NULL, editor);
  g_signal_connect (profile, "notify",
                    G_CALLBACK (profile_notify_sensitivity_cb),
                    editor);
  g_signal_connect (profile,
                    "forgotten",
                    G_CALLBACK (profile_forgotten_cb),
                    editor);

  terminal_profile_editor_focus_widget (editor, widget_name);

  gtk_window_set_transient_for (GTK_WINDOW (editor),
                                GTK_WINDOW (transient_parent));
  gtk_window_present (GTK_WINDOW (editor));
}
