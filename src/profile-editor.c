/*
 * Copyright © 2002 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
 * Copyright © 2008, 2011 Christian Persch
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

#include "terminal-enums.h"
#include "terminal-intl.h"
#include "profile-editor.h"
#include "terminal-schemas.h"
#include "terminal-type-builtins.h"
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

#define TERMINAL_PALETTE_SIZE (16)

enum
{
  TERMINAL_PALETTE_TANGO = 0,
  TERMINAL_PALETTE_LINUX = 1,
  TERMINAL_PALETTE_XTERM = 2,
  TERMINAL_PALETTE_RXVT  = 3,
  TERMINAL_PALETTE_N_BUILTINS
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

static void profile_colors_notify_scheme_combo_cb (GSettings *profile,
                                                   const char *key,
                                                   GtkComboBox *combo);

static void profile_palette_notify_scheme_combo_cb (GSettings *profile,
                                                    const char *key,
                                                    GtkComboBox *combo);

static void profile_palette_notify_colorpickers_cb (GSettings *profile,
                                                    const char *key,
                                                    GtkWidget *editor);

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

static gboolean
palette_is_builtin (GdkColor *colors,
                    gsize n_colors,
                    guint *n)
{
  guint i;

  if (n_colors != TERMINAL_PALETTE_SIZE)
    return FALSE;

  for (i = 0; i < TERMINAL_PALETTE_N_BUILTINS; ++i)
    {
      if (palette_cmp (colors, terminal_palettes[i]))
        {
          *n = i;
          return TRUE;
        }
    }

  return FALSE;
}

static void
modify_palette_entry (GSettings       *profile,
                      guint            i,
                      const GdkColor  *color)
{
  GdkColor *colors;
  gsize n_colors;

  colors = terminal_g_settings_get_gdk_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  if (i < n_colors)
    {
      colors[i] = *color;
      terminal_g_settings_set_gdk_palette (profile, TERMINAL_PROFILE_PALETTE_KEY,
                                           colors, n_colors);
    }

  g_free (colors);
}

static void
color_scheme_combo_changed_cb (GtkWidget *combo,
                               GParamSpec *pspec,
                               GSettings *profile)
{
  guint i;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (i < G_N_ELEMENTS (color_schemes))
    {
      g_signal_handlers_block_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
      terminal_g_settings_set_gdk_color (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &color_schemes[i].foreground);
      terminal_g_settings_set_gdk_color (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &color_schemes[i].background);
      g_signal_handlers_unblock_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
    }
  else
    {
      /* "custom" selected, no change */
    }
}

static void
profile_colors_notify_scheme_combo_cb (GSettings *profile,
                                       const char *key,
                                       GtkComboBox *combo)
{
  GdkColor fg, bg;
  guint i;

  terminal_g_settings_get_gdk_color (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &fg);
  terminal_g_settings_get_gdk_color (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &bg);

  for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
    {
      if (gdk_color_equal (&fg, &color_schemes[i].foreground) &&
          gdk_color_equal (&bg, &color_schemes[i].background))
        break;
    }
  /* If we didn't find a match, then we get the last combo box item which is "custom" */

  g_signal_handlers_block_by_func (combo, G_CALLBACK (color_scheme_combo_changed_cb), profile);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), i);
  g_signal_handlers_unblock_by_func (combo, G_CALLBACK (color_scheme_combo_changed_cb), profile);
}

static void
palette_scheme_combo_changed_cb (GtkComboBox *combo,
                                 GParamSpec *pspec,
                                 GSettings *profile)
{
  int i;

  i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  g_signal_handlers_block_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
  if (i < TERMINAL_PALETTE_N_BUILTINS)
    terminal_g_settings_set_gdk_palette (profile, TERMINAL_PROFILE_PALETTE_KEY,
                                         terminal_palettes[i], TERMINAL_PALETTE_SIZE);
  else
    {
      /* "custom" selected, no change */
    }
  g_signal_handlers_unblock_by_func (profile, G_CALLBACK (profile_colors_notify_scheme_combo_cb), combo);
}

static void
profile_palette_notify_scheme_combo_cb (GSettings *profile,
                                        const char *key,
                                        GtkComboBox *combo)
{
  GdkColor *colors;
  gsize n_colors;
  guint i;

  colors = terminal_g_settings_get_gdk_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);
  if (!palette_is_builtin (colors, n_colors, &i))
    /* If we didn't find a match, then we want the last combo
     * box item which is "custom"
     */
    i = TERMINAL_PALETTE_N_BUILTINS;

  g_signal_handlers_block_by_func (combo, G_CALLBACK (palette_scheme_combo_changed_cb), profile);
  gtk_combo_box_set_active (combo, i);
  g_signal_handlers_unblock_by_func (combo, G_CALLBACK (palette_scheme_combo_changed_cb), profile);

  g_free (colors);
}

static void
palette_color_notify_cb (GtkColorButton *button,
                         GParamSpec *pspec,
                         GSettings *profile)
{
  GtkWidget *editor;
  GdkColor color;
  guint i;

  gtk_color_button_get_color (button, &color);
  i = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "palette-entry-index"));

  editor = gtk_widget_get_toplevel (GTK_WIDGET (button));
  g_signal_handlers_block_by_func (profile, G_CALLBACK (profile_palette_notify_colorpickers_cb), editor);
  modify_palette_entry (profile, i, &color);
  g_signal_handlers_unblock_by_func (profile, G_CALLBACK (profile_palette_notify_colorpickers_cb), editor);
}

static void
profile_palette_notify_colorpickers_cb (GSettings *profile,
                                        const char *key,
                                        GtkWidget *editor)
{
  GtkWidget *w;
  GtkBuilder *builder;
  GdkColor *colors;
  gsize n_colors, i;

  g_assert (strcmp (key, TERMINAL_PROFILE_PALETTE_KEY) == 0);

  builder = g_object_get_data (G_OBJECT (editor), "builder");
  g_assert (builder != NULL);

  colors = terminal_g_settings_get_gdk_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  n_colors = MIN (n_colors, TERMINAL_PALETTE_SIZE);
  for (i = 0; i < n_colors; i++)
    {
      char name[32];
      GdkColor old_color;

      g_snprintf (name, sizeof (name), "palette-colorpicker-%" G_GSIZE_FORMAT, i + 1);
      w = (GtkWidget *) gtk_builder_get_object  (builder, name);

      gtk_color_button_get_color (GTK_COLOR_BUTTON (w), &old_color);
      if (!gdk_color_equal (&old_color, &colors[i]))
        {
          g_signal_handlers_block_by_func (w, G_CALLBACK (palette_color_notify_cb), profile);
          gtk_color_button_set_color (GTK_COLOR_BUTTON (w), &colors[i]);
          g_signal_handlers_unblock_by_func (w, G_CALLBACK (palette_color_notify_cb), profile);
        }
    }

  g_free (colors);
}

static void
custom_command_entry_changed_cb (GtkEntry *entry)
{
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
}

static void
reset_compat_defaults_cb (GtkWidget *button,
                          GSettings *profile)
{
  g_settings_reset (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY);
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

static void
profile_editor_destroyed (GtkWidget *editor,
                          GSettings *profile)
{
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

static gboolean
string_to_window_title (GValue *value,
                        GVariant *variant,
                        gpointer user_data)
{
  const char *visible_name;

  g_variant_get (variant, "&s", &visible_name);
  g_value_take_string (value, g_strdup_printf (_("Editing Profile “%s”"), visible_name));

  return TRUE;
}


static gboolean
string_to_color (GValue *value,
                 GVariant *variant,
                 gpointer user_data)
{
  const char *s;
  GdkColor color;
  gboolean retval;

  g_variant_get (variant, "&s", &s);
  retval = gdk_color_parse (s, &color);
  if (retval)
    g_value_set_boxed (value, &color);

  return retval;
}

static GVariant *
color_to_string (const GValue *value,
                 const GVariantType *expected_type,
                 gpointer user_data)
{
  GdkColor *color;
  char *s;
  GVariant *variant;

  color = g_value_get_boxed (value);
  if (color == NULL)
    return NULL;

  s = gdk_color_to_string (color);
  variant = g_variant_new_string (s);
  g_free (s);

  return variant;
}

static gboolean
string_to_enum (GValue *value,
                GVariant *variant,
                gpointer user_data)
{
  GType (* get_type) (void) = user_data;
  GEnumClass *klass;
  GEnumValue *eval = NULL;
  const char *s;
  guint i;

  g_variant_get (variant, "&s", &s);

  klass = g_type_class_ref (get_type ());
  for (i = 0; i < klass->n_values; ++i) {
    if (strcmp (klass->values[i].value_nick, s) != 0)
      continue;

    eval = &klass->values[i];
    break;
  }

  if (eval)
    g_value_set_int (value, eval->value);

  g_type_class_unref (klass);

  return eval != NULL;
}

static GVariant *
enum_to_string (const GValue *value,
                const GVariantType *expected_type,
                gpointer user_data)
{
  GType (* get_type) (void) = user_data;
  GEnumClass *klass;
  GEnumValue *eval = NULL;
  int val;
  guint i;
  GVariant *variant = NULL;

  val = g_value_get_int (value);

  klass = g_type_class_ref (get_type ());
  for (i = 0; i < klass->n_values; ++i) {
    if (klass->values[i].value != val)
      continue;

    eval = &klass->values[i];
    break;
  }

  if (eval)
    variant = g_variant_new_string (eval->value_nick);

  g_type_class_unref (klass);

  return variant;
}

/**
 * terminal_profile_edit:
 * @profile: a #GSettings
 * @transient_parent: a #GtkWindow, or %NULL
 * @widget_name: a widget name in the profile editor's UI, or %NULL
 *
 * Shows the profile editor with @profile, anchored to @transient_parent.
 * If @widget_name is non-%NULL, focuses the corresponding widget and
 * switches the notebook to its containing page.
 */
void
terminal_profile_edit (GSettings  *profile,
                       GtkWindow  *transient_parent,
                       const char *widget_name)
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

  profile_palette_notify_colorpickers_cb (profile, TERMINAL_PROFILE_PALETTE_KEY, editor);
  g_signal_connect (profile, "changed::" TERMINAL_PROFILE_PALETTE_KEY,
                    G_CALLBACK (profile_palette_notify_colorpickers_cb),
                    editor);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "palette-combobox");
  g_signal_connect (w, "notify::active",
                    G_CALLBACK (palette_scheme_combo_changed_cb),
                    profile);

  profile_palette_notify_scheme_combo_cb (profile, TERMINAL_PROFILE_PALETTE_KEY, GTK_COMBO_BOX (w));
  g_signal_connect (profile, "changed::" TERMINAL_PROFILE_PALETTE_KEY,
                    G_CALLBACK (profile_palette_notify_scheme_combo_cb),
                    w);

  /* Hook up the color scheme pickers and combo box */
  w = (GtkWidget *) gtk_builder_get_object  (builder, "color-scheme-combobox");
  g_signal_connect (w, "notify::active",
                    G_CALLBACK (color_scheme_combo_changed_cb),
                    profile);

  profile_colors_notify_scheme_combo_cb (profile, NULL, GTK_COMBO_BOX (w));
  g_signal_connect (profile, "changed::" TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                    G_CALLBACK (profile_colors_notify_scheme_combo_cb),
                    w);
  g_signal_connect (profile, "changed::" TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                    G_CALLBACK (profile_colors_notify_scheme_combo_cb),
                    w);

  w = GTK_WIDGET (gtk_builder_get_object (builder, "custom-command-entry"));
  custom_command_entry_changed_cb (GTK_ENTRY (w));
  g_signal_connect (w, "changed",
                    G_CALLBACK (custom_command_entry_changed_cb), NULL);

  g_signal_connect (gtk_builder_get_object  (builder, "reset-compat-defaults-button"),
                    "clicked",
                    G_CALLBACK (reset_compat_defaults_cb),
                    profile);

  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                                editor,
                                "title",
                                G_SETTINGS_BIND_GET |
                                G_SETTINGS_BIND_NO_SENSITIVITY,
                                (GSettingsBindGetMapping)
                                string_to_window_title, NULL, NULL, NULL);

  g_settings_bind (profile,
                   TERMINAL_PROFILE_ALLOW_BOLD_KEY,
                   gtk_builder_get_object (builder, "allow-bold-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "background-colorpicker"),
                                "color",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_color,
                                (GSettingsBindSetMapping) color_to_string,
                                NULL, NULL);
  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_BACKSPACE_BINDING_KEY,
                                gtk_builder_get_object (builder,
                                                        "backspace-binding-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                vte_terminal_erase_binding_get_type, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                   gtk_builder_get_object (builder,
                                           "bold-color-same-as-fg-checkbox"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_BOLD_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "bold-colorpicker"),
                                "color",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_color,
                                (GSettingsBindSetMapping) color_to_string,
                                NULL, NULL);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_SHAPE_KEY,
                                gtk_builder_get_object (builder,
                                                        "cursor-shape-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                vte_terminal_cursor_shape_get_type, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_CUSTOM_COMMAND_KEY,
                   gtk_builder_get_object (builder, "custom-command-entry"),
                   "text", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY,
                   gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON
                                                   (gtk_builder_get_object
                                                    (builder,
                                                     "default-size-columns-spinbutton"))),
                   "value", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY,
                   gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON
                                                   (gtk_builder_get_object
                                                    (builder,
                                                     "default-size-rows-spinbutton"))),
                   "value", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY,
                                gtk_builder_get_object (builder,
                                                        "delete-binding-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                vte_terminal_erase_binding_get_type, NULL);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_EXIT_ACTION_KEY,
                                gtk_builder_get_object (builder,
                                                        "exit-action-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                terminal_exit_action_get_type, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_FONT_KEY,
                   gtk_builder_get_object (builder, "font-selector"),
                   "font-name", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "foreground-colorpicker"),
                                "color",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_color,
                                (GSettingsBindSetMapping) color_to_string,
                                NULL, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_LOGIN_SHELL_KEY,
                   gtk_builder_get_object (builder,
                                           "login-shell-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                   gtk_builder_get_object (builder, "profile-name-entry"),
                   "text", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY,
                   gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON
                                                   (gtk_builder_get_object
                                                    (builder,
                                                     "scrollback-lines-spinbutton"))),
                   "value", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                   gtk_builder_get_object (builder,
                                           "scrollback-unlimited-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY,
                                gtk_builder_get_object (builder,
                                                        "scrollbar-policy-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                gtk_policy_type_get_type, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY,
                   gtk_builder_get_object (builder,
                                           "scroll-on-keystroke-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY,
                   gtk_builder_get_object (builder,
                                           "scroll-on-output-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   gtk_builder_get_object (builder,
                                           "system-font-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_TITLE_KEY,
                   gtk_builder_get_object (builder, "title-entry"), "text",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_TITLE_MODE_KEY,
                                gtk_builder_get_object (builder,
                                                        "title-mode-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                terminal_title_mode_get_type, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_UPDATE_RECORDS_KEY,
                   gtk_builder_get_object (builder,
                                           "update-records-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                   gtk_builder_get_object (builder,
                                           "use-custom-command-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE_KEY,
                   gtk_builder_get_object (builder,
                                           "use-custom-default-size-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                   gtk_builder_get_object (builder,
                                           "use-theme-colors-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_WORD_CHARS_KEY,
                   gtk_builder_get_object (builder, "word-chars-entry"),
                   "text", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_AUDIBLE_BELL_KEY,
                   gtk_builder_get_object (builder, "bell-checkbutton"),
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  g_settings_bind (profile,
                   TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                   gtk_builder_get_object (builder, "custom-command-box"),
                   "sensitive", G_SETTINGS_BIND_GET);
  g_settings_bind (profile,
                   TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   gtk_builder_get_object (builder, "font-hbox"),
                   "sensitive",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (profile,
                   TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE_KEY,
                   gtk_builder_get_object (builder, "default-size-hbox"),
                   "sensitive", G_SETTINGS_BIND_GET);
  g_settings_bind_writable (profile,
                            TERMINAL_PROFILE_PALETTE_KEY,
                            gtk_builder_get_object (builder, "palette-box"),
                            "sensitive",
                            FALSE);

#if 0
  if (!prop_name ||
    prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_KEY) ||
    prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY) ||
    prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS_KEY))
  {
    gboolean bold_locked, bold_same_as_fg_locked, bold_same_as_fg, use_theme_colors;
    
    bold_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_BOLD_COLOR_KEY);
    bold_same_as_fg_locked = terminal_profile_property_locked (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY);
    bold_same_as_fg = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY);
    
    SET_SENSITIVE ("bold-color-same-as-fg-checkbox", !bold_same_as_fg_locked);
    SET_SENSITIVE ("bold-colorpicker", !bold_locked && !bold_same_as_fg);
    SET_SENSITIVE ("bold-colorpicker-label", ((!bold_same_as_fg && !bold_locked) || !bold_same_as_fg_locked));
  }
#endif

  terminal_util_bind_mnemonic_label_sensitivity (editor);

  terminal_profile_editor_focus_widget (editor, widget_name);

  gtk_window_set_transient_for (GTK_WINDOW (editor),
                                GTK_WINDOW (transient_parent));
  gtk_window_present (GTK_WINDOW (editor));
}
