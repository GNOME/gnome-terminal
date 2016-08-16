
/*
 * Copyright © 2002 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
 * Copyright © 2008, 2011 Christian Persch
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

#include <config.h>

#include <string.h>
#include <math.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "terminal-app.h"
#include "terminal-enums.h"
#include "profile-editor.h"
#include "terminal-schemas.h"
#include "terminal-type-builtins.h"
#include "terminal-util.h"
#include "terminal-profiles-list.h"
#include "terminal-libgsystem.h"

typedef struct _TerminalColorScheme TerminalColorScheme;

struct _TerminalColorScheme
{
  const char *name;
  const GdkRGBA foreground;
  const GdkRGBA background;
};

static const TerminalColorScheme color_schemes[] = {
  { N_("Black on light yellow"),
    { 0, 0, 0, 1 },
    { 1, 1, 0.866667, 1 }
  },
  { N_("Black on white"),
    { 0, 0, 0, 1 },
    { 1, 1, 1, 1 }
  },
  { N_("Gray on black"),
    { 0.666667, 0.666667, 0.666667, 1 },
    { 0, 0, 0, 1 }
  },
  { N_("Green on black"),
    { 0, 1, 0, 1 },
    { 0, 0, 0, 1 }
  },
  { N_("White on black"),
    { 1, 1, 1, 1 },
    { 0, 0, 0, 1 }
  },
  /* Translators: "Solarized" is the name of a colour scheme, "light" can be translated */
  { N_("Solarized light"),
    { 0.396078, 0.482352, 0.513725, 1 },
    { 0.992156, 0.964705, 0.890196, 1 }
  },
  /* Translators: "Solarized" is the name of a colour scheme, "dark" can be translated */
  { N_("Solarized dark"),
    { 0.513725, 0.580392, 0.588235, 1 },
    { 0,        0.168627, 0.211764, 1 }
  },
};

#define TERMINAL_PALETTE_SIZE (16)

enum
{
  TERMINAL_PALETTE_TANGO     = 0,
  TERMINAL_PALETTE_LINUX     = 1,
  TERMINAL_PALETTE_XTERM     = 2,
  TERMINAL_PALETTE_RXVT      = 3,
  TERMINAL_PALETTE_SOLARIZED = 4,
  TERMINAL_PALETTE_N_BUILTINS
};

#define COLOR(r, g, b) { .red = (r) / 255.0, .green = (g) / 255.0, .blue = (b) / 255.0, .alpha = 1.0 }

static const GdkRGBA terminal_palettes[TERMINAL_PALETTE_N_BUILTINS][TERMINAL_PALETTE_SIZE] =
{
  /* Tango palette */
  {
    COLOR (0x00, 0x00, 0x00),
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
    COLOR (0x07, 0x36, 0x42),
    COLOR (0xdc, 0x32, 0x2f),
    COLOR (0x85, 0x99, 0x00),
    COLOR (0xb5, 0x89, 0x00),
    COLOR (0x26, 0x8b, 0xd2),
    COLOR (0xd3, 0x36, 0x82),
    COLOR (0x2a, 0xa1, 0x98),
    COLOR (0xee, 0xe8, 0xd5),
    COLOR (0x00, 0x2b, 0x36),
    COLOR (0xcb, 0x4b, 0x16),
    COLOR (0x58, 0x6e, 0x75),
    COLOR (0x65, 0x7b, 0x83),
    COLOR (0x83, 0x94, 0x96),
    COLOR (0x6c, 0x71, 0xc4),
    COLOR (0x93, 0xa1, 0xa1),
    COLOR (0xfd, 0xf6, 0xe3)
  },
};

#undef COLOR

static void profile_colors_notify_scheme_combo_cb (GSettings *profile,
                                                   const char *key,
                                                   GtkComboBox *combo);

static void profile_palette_notify_scheme_combo_cb (GSettings *profile,
                                                    const char *key,
                                                    GtkComboBox *combo);

static void profile_palette_notify_colorpickers_cb (GSettings *profile,
                                                    const char *key,
                                                    GtkWidget *editor);


/* gdk_rgba_equal is too strict! */
static gboolean
rgba_equal (const GdkRGBA *a,
            const GdkRGBA *b)
{
  gdouble dr, dg, db;

  dr = a->red - b->red;
  dg = a->green - b->green;
  db = a->blue - b->blue;

  return (dr * dr + dg * dg + db * db) < 1e-4;
}

static gboolean
palette_cmp (const GdkRGBA *ca,
             const GdkRGBA *cb)
{
  guint i;

  for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
    if (!rgba_equal (&ca[i], &cb[i]))
      return FALSE;

  return TRUE;
}

static gboolean
palette_is_builtin (const GdkRGBA *colors,
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
                      const GdkRGBA   *color)
{
  gs_free GdkRGBA *colors;
  gsize n_colors;

  /* FIXMEchpe: this can be optimised, don't really need to parse the colours! */

  colors = terminal_g_settings_get_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  if (i < n_colors)
    {
      colors[i] = *color;
      terminal_g_settings_set_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY,
                                            colors, n_colors);
    }
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
      terminal_g_settings_set_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &color_schemes[i].foreground);
      terminal_g_settings_set_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &color_schemes[i].background);
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
  GdkRGBA fg, bg;
  guint i;

  terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &fg);
  terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &bg);

  for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
    {
      if (rgba_equal (&fg, &color_schemes[i].foreground) &&
          rgba_equal (&bg, &color_schemes[i].background))
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
    terminal_g_settings_set_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY,
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
  gs_free GdkRGBA *colors;
  gsize n_colors;
  guint i;

  colors = terminal_g_settings_get_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);
  if (!palette_is_builtin (colors, n_colors, &i))
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
                         GSettings *profile)
{
  GtkWidget *editor;
  GdkRGBA color;
  guint i;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);
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
  gs_free GdkRGBA *colors;
  gsize n_colors, i;

  g_assert (strcmp (key, TERMINAL_PROFILE_PALETTE_KEY) == 0);

  builder = g_object_get_data (G_OBJECT (editor), "builder");
  g_assert (builder != NULL);

  colors = terminal_g_settings_get_rgba_palette (profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  n_colors = MIN (n_colors, TERMINAL_PALETTE_SIZE);
  for (i = 0; i < n_colors; i++)
    {
      char name[32];

      g_snprintf (name, sizeof (name), "palette-colorpicker-%" G_GSIZE_FORMAT, i + 1);
      w = (GtkWidget *) gtk_builder_get_object  (builder, name);

      g_signal_handlers_block_by_func (w, G_CALLBACK (palette_color_notify_cb), profile);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (w), &colors[i]);
      g_signal_handlers_unblock_by_func (w, G_CALLBACK (palette_color_notify_cb), profile);
    }
}

static void
custom_command_entry_changed_cb (GtkEntry *entry)
{
  const char *command;
  gs_free_error GError *error = NULL;

  command = gtk_entry_get_text (entry);

  if (command[0] == '\0' ||
      g_shell_parse_argv (command, NULL, NULL, &error))
    {
      gtk_entry_set_icon_from_icon_name (entry, GTK_PACK_END, NULL);
    }
  else
    {
      gs_free char *tooltip;

      gtk_entry_set_icon_from_icon_name (entry, GTK_PACK_END, "dialog-warning");

      tooltip = g_strdup_printf (_("Error parsing command: %s"), error->message);
      gtk_entry_set_icon_tooltip_text (entry, GTK_PACK_END, tooltip);
    }
}

static void
default_size_reset_cb (GtkWidget *button,
                       GSettings *profile)
{
  g_settings_reset (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY);
}

static void
reset_compat_defaults_cb (GtkWidget *button,
                          GSettings *profile)
{
  g_settings_reset (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_ENCODING_KEY);
  g_settings_reset (profile, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY);
}

/*
 * initialize widgets
 */

static void
init_color_scheme_menu (GtkWidget *widget)
{
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  gs_unref_object GtkListStore *store;
  guint i;

  store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
    gtk_list_store_insert_with_values (store, &iter, -1,
                                       0, _(color_schemes[i].name),
                                       -1);
  gtk_list_store_insert_with_values (store, &iter, -1,
                                      0, _("Custom"),
                                      -1);

  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer, "text", 0, NULL);
}

enum {
  ENCODINGS_COLUMN_ID,
  ENCODINGS_COLUMN_MARKUP
};

static void
init_encodings_combo (GtkWidget *widget)
{
  GtkCellRenderer *renderer;
  GHashTableIter ht_iter;
  gpointer key, value;
  gs_unref_object GtkListStore *store;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  g_hash_table_iter_init (&ht_iter, terminal_app_get_encodings (terminal_app_get ()));
  while (g_hash_table_iter_next (&ht_iter, &key, &value)) {
    TerminalEncoding *encoding = value;
    GtkTreeIter iter;
    gs_free char *name;

    name = g_markup_printf_escaped ("%s <span size=\"small\">%s</span>",
                                    terminal_encoding_get_charset (encoding),
                                    encoding->name);
    gtk_list_store_insert_with_values (store, &iter, -1,
                                       ENCODINGS_COLUMN_MARKUP, name,
                                       ENCODINGS_COLUMN_ID, terminal_encoding_get_charset (encoding),
                                       -1);
  }

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        ENCODINGS_COLUMN_MARKUP,
                                        GTK_SORT_ASCENDING);

  gtk_combo_box_set_id_column (GTK_COMBO_BOX (widget), ENCODINGS_COLUMN_ID);
  gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));

  /* Cell renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
                                  "markup", ENCODINGS_COLUMN_MARKUP, NULL);
}

static void
editor_help_button_clicked_cb (GtkWidget *button,
                               GtkWidget *editor)
{
  terminal_util_show_help ("profile", GTK_WINDOW (editor));
}

static void
editor_close_button_clicked_cb (GtkWidget *button,
                                GtkWidget *editor)
{
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


/* Tab scrolling was removed from GtkNotebook in gtk 3, so reimplement it here */
static gboolean
scroll_event_cb (GtkWidget      *widget,
                 GdkEventScroll *event,
                 gpointer        user_data)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkWidget *child, *event_widget, *action_widget;

  if ((event->state & gtk_accelerator_get_default_mod_mask ()) != 0)
    return FALSE;

  child = gtk_notebook_get_nth_page (notebook, gtk_notebook_get_current_page (notebook));
  if (child == NULL)
    return FALSE;

  event_widget = gtk_get_event_widget ((GdkEvent *) event);

  /* Ignore scroll events from the content of the page */
  if (event_widget == NULL ||
      event_widget == child ||
      gtk_widget_is_ancestor (event_widget, child))
    return FALSE;

  /* And also from the action widgets */
  action_widget = gtk_notebook_get_action_widget (notebook, GTK_PACK_START);
  if (event_widget == action_widget ||
      (action_widget != NULL && gtk_widget_is_ancestor (event_widget, action_widget)))
    return FALSE;
  action_widget = gtk_notebook_get_action_widget (notebook, GTK_PACK_END);
  if (event_widget == action_widget ||
      (action_widget != NULL && gtk_widget_is_ancestor (event_widget, action_widget)))
    return FALSE;

  switch (event->direction) {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
      gtk_notebook_next_page (notebook);
      return TRUE;
    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
      gtk_notebook_prev_page (notebook);
      return TRUE;
    case GDK_SCROLL_SMOOTH:
      switch (gtk_notebook_get_tab_pos (notebook)) {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
          if (event->delta_y > 0)
            gtk_notebook_next_page (notebook);
          else if (event->delta_y < 0)
            gtk_notebook_prev_page (notebook);
          break;
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
          if (event->delta_x > 0)
            gtk_notebook_next_page (notebook);
          else if (event->delta_x < 0)
            gtk_notebook_prev_page (notebook);
          break;
      }
      return TRUE;
  }

  return FALSE;
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
s_to_rgba (GValue *value,
           GVariant *variant,
           gpointer user_data)
{
  const char *s;
  GdkRGBA color;

  g_variant_get (variant, "&s", &s);
  if (!gdk_rgba_parse (&color, s))
    return FALSE;

  color.alpha = 1.0;
  g_value_set_boxed (value, &color);
  return TRUE;
}

static GVariant *
rgba_to_s (const GValue *value,
           const GVariantType *expected_type,
           gpointer user_data)
{
  GdkRGBA *color;
  gs_free char *s = NULL;

  color = g_value_get_boxed (value);
  if (color == NULL)
    return NULL;

  s = gdk_rgba_to_string (color);
  return g_variant_new_string (s);
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

static gboolean
scrollbar_policy_to_bool (GValue *value,
                          GVariant *variant,
                          gpointer user_data)
{
  const char *str;

  g_variant_get (variant, "&s", &str);
  g_value_set_boolean (value, g_str_equal (str, "always"));

  return TRUE;
}

static GVariant *
bool_to_scrollbar_policy (const GValue *value,
                          const GVariantType *expected_type,
                          gpointer user_data)
{
  return g_variant_new_string (g_value_get_boolean (value) ? "always" : "never");
}

#if !GTK_CHECK_VERSION (3, 19, 8)

/* ATTENTION: HACK HACK HACK!
 * GtkColorButton usability is broken. It always pops up the
 * GtkColorChooserDialog with show-editor=FALSE, which brings
 * up the dialogue in palette mode, when all we want is pick
 * a colour. Since there is no way to get to the colour
 * dialogue of the button, and the dialogue always sets
 * show-editor=FALSE in its map anyway, we need to override
 * the map implementation, set show-editor=TRUE and chain to
 * the parent's map. This is reasonably safe to do since that's
 * all the map functiondoes, and we can change this for _all_
 * colour chooser buttons, since they are used only in our
 * profile preferences dialogue.
 */

static void
fixup_color_chooser_dialog_map (GtkWidget *widget)
{
  g_object_set (GTK_COLOR_CHOOSER_DIALOG (widget), "show-editor", TRUE, NULL);

  GTK_WIDGET_CLASS (g_type_class_peek_parent (GTK_COLOR_CHOOSER_DIALOG_GET_CLASS (widget)))->map (widget);
}

static void
fixup_color_chooser_button (void)
{
  static gboolean done = FALSE;

  if (!done) {
    GtkColorChooserDialogClass *klass;
    klass = g_type_class_ref (GTK_TYPE_COLOR_CHOOSER_DIALOG);
    g_assert (klass != NULL);
    GTK_WIDGET_CLASS (klass)->map = fixup_color_chooser_dialog_map;
    g_type_class_unref (klass);
    done = TRUE;
  }
}

#endif /* GTK+ < 3.19.8 HACK */

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
  TerminalSettingsList *profiles_list;
  GtkBuilder *builder;
  GError *error = NULL;
  GtkWidget *editor, *w;
  gs_free char *uuid = NULL;
  guint i;

  editor = g_object_get_data (G_OBJECT (profile), "editor-window");
  if (editor)
    {
      terminal_util_dialog_focus_widget (editor, widget_name);

      gtk_window_set_transient_for (GTK_WINDOW (editor),
                                    GTK_WINDOW (transient_parent));
      gtk_window_present (GTK_WINDOW (editor));
      return;
    }

#if !GTK_CHECK_VERSION (3, 19, 8)
  fixup_color_chooser_button ();
#endif

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/gnome/terminal/ui/profile-preferences.ui", &error);
  g_assert_no_error (error);

  editor = (GtkWidget *) gtk_builder_get_object  (builder, "profile-editor-dialog");
  g_object_set_data_full (G_OBJECT (editor), "builder",
                          builder, (GDestroyNotify) g_object_unref);

  gtk_window_set_application (GTK_WINDOW (editor), GTK_APPLICATION (terminal_app_get ()));

  /* Store the dialogue on the profile, so we can acccess it above to check if
   * there's already a profile editor for this profile.
   */
  g_object_set_data (G_OBJECT (profile), "editor-window", editor);

  g_signal_connect (editor, "destroy",
                    G_CALLBACK (profile_editor_destroyed),
                    profile);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "close-button");
  g_signal_connect (w, "clicked", G_CALLBACK (editor_close_button_clicked_cb), editor);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "help-button");
  g_signal_connect (w, "clicked", G_CALLBACK (editor_help_button_clicked_cb), editor);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "profile-editor-notebook");
  gtk_widget_add_events (w, GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK);
  g_signal_connect (w, "scroll-event", G_CALLBACK (scroll_event_cb), NULL);

  uuid = terminal_settings_list_dup_uuid_from_child (profiles_list, profile);
  gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "profile-uuid")),
                      uuid);

  g_signal_connect (gtk_builder_get_object  (builder, "default-size-reset-button"),
                    "clicked",
                    G_CALLBACK (default_size_reset_cb),
                    profile);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "color-scheme-combobox");
  init_color_scheme_menu (w);

  /* Hook up the palette colorpickers and combo box */

  for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
    {
      char name[32];
      char *text;

      g_snprintf (name, sizeof (name), "palette-colorpicker-%u", i + 1);
      w = (GtkWidget *) gtk_builder_get_object  (builder, name);

#if GTK_CHECK_VERSION (3, 19, 8)
      g_object_set (w, "show-editor", TRUE, NULL);
#endif

      g_object_set_data (G_OBJECT (w), "palette-entry-index", GUINT_TO_POINTER (i));

      text = g_strdup_printf (_("Choose Palette Color %u"), i + 1);
      gtk_color_button_set_title (GTK_COLOR_BUTTON (w), text);
      g_free (text);

      text = g_strdup_printf (_("Palette entry %u"), i + 1);
      gtk_widget_set_tooltip_text (w, text);
      g_free (text);

      g_signal_connect (w, "notify::rgba",
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
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
                                NULL, NULL);
  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_BACKSPACE_BINDING_KEY,
                                gtk_builder_get_object (builder,
                                                        "backspace-binding-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                vte_erase_binding_get_type, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                   gtk_builder_get_object (builder,
                                           "bold-color-checkbutton"),
                   "active",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY,
                   gtk_builder_get_object (builder,
                                           "bold-colorpicker"),
                   "sensitive",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_BOLD_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "bold-colorpicker"),
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_NO_SENSITIVITY,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
                                NULL, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                   gtk_builder_get_object (builder,
                                           "cursor-colors-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                   gtk_builder_get_object (builder,
                                           "cursor-foreground-colorpicker"),
                   "sensitive",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY,
                   gtk_builder_get_object (builder,
                                           "cursor-background-colorpicker"),
                   "sensitive",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "cursor-foreground-colorpicker"),
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_NO_SENSITIVITY,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
                                NULL, NULL);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "cursor-background-colorpicker"),
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_NO_SENSITIVITY,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
                                NULL, NULL);
  g_settings_bind (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                   gtk_builder_get_object (builder,
                                           "highlight-colors-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                   gtk_builder_get_object (builder,
                                           "highlight-foreground-colorpicker"),
                   "sensitive",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY,
                   gtk_builder_get_object (builder,
                                           "highlight-background-colorpicker"),
                   "sensitive",
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "highlight-foreground-colorpicker"),
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_NO_SENSITIVITY,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
                                NULL, NULL);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY,
                                gtk_builder_get_object (builder,
                                                        "highlight-background-colorpicker"),
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_NO_SENSITIVITY,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
                                NULL, NULL);
  g_settings_bind_with_mapping (profile, TERMINAL_PROFILE_CURSOR_SHAPE_KEY,
                                gtk_builder_get_object (builder,
                                                        "cursor-shape-combobox"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) string_to_enum,
                                (GSettingsBindSetMapping) enum_to_string,
                                vte_cursor_shape_get_type, NULL);
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
                                vte_erase_binding_get_type, NULL);
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
                                "rgba",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) s_to_rgba,
                                (GSettingsBindSetMapping) rgba_to_s,
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
                                           "scrollback-limited-checkbutton"),
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY,
                   gtk_builder_get_object (builder,
                                           "scrollback-box"),
                   "sensitive", 
                   G_SETTINGS_BIND_GET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind_with_mapping (profile,
                                TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY,
                                gtk_builder_get_object (builder,
                                                        "scrollbar-checkbutton"),
                                "active",
                                G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
                                (GSettingsBindGetMapping) scrollbar_policy_to_bool,
                                (GSettingsBindSetMapping) bool_to_scrollbar_policy,
                                NULL, NULL);
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
                                           "custom-font-checkbutton"),
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET |
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                   gtk_builder_get_object (builder,
                                           "use-custom-command-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                   gtk_builder_get_object (builder,
                                           "use-theme-colors-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (profile, TERMINAL_PROFILE_AUDIBLE_BELL_KEY,
                   gtk_builder_get_object (builder, "bell-checkbutton"),
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  g_settings_bind (profile,
                   TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY,
                   gtk_builder_get_object (builder, "custom-command-box"),
                   "sensitive",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind (profile,
                   TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY,
                   gtk_builder_get_object (builder, "font-selector"),
                   "sensitive",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind (profile,
                   TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
                   gtk_builder_get_object (builder, "colors-box"),
                   "sensitive",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind_writable (profile,
                            TERMINAL_PROFILE_PALETTE_KEY,
                            gtk_builder_get_object (builder, "palette-box"),
                            "sensitive",
                            FALSE);
  g_settings_bind (profile,
                   TERMINAL_PROFILE_REWRAP_ON_RESIZE_KEY,
                   gtk_builder_get_object (builder, "rewrap-on-resize-checkbutton"),
                   "active", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  /* Compatibility options */
  w = (GtkWidget *) gtk_builder_get_object  (builder, "encoding-combobox");
  init_encodings_combo (w);
  g_settings_bind (profile,
                   TERMINAL_PROFILE_ENCODING_KEY,
                   w,
                   "active-id", G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  w = (GtkWidget *) gtk_builder_get_object  (builder, "cjk-ambiguous-width-combobox");
  g_settings_bind (profile, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY,
                   w,
                   "active-id",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  /* Finished! */
  terminal_util_bind_mnemonic_label_sensitivity (editor);

  terminal_util_dialog_focus_widget (editor, widget_name);

  gtk_window_set_transient_for (GTK_WINDOW (editor),
                                GTK_WINDOW (transient_parent));
  gtk_window_present (GTK_WINDOW (editor));
}
