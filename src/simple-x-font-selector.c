/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GnomeFontSelection widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
 *
 * This file is part of gnome-terminal.
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <X11/Xlib.h>

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "terminal-intl.h"
#include "simple-x-font-selector.h"
#include "terminal.h"


static void egg_xfont_selector_class_init    (EggXFontSelectorClass  *klass);
static void egg_xfont_selector_finalize       (GObject        *object);

static void egg_xfont_selector_init          (EggXFontSelector       *frame);
static void egg_xfont_selector_set_property (GObject      *object,
				    guint         param_id,
				    const GValue *value,
				    GParamSpec   *pspec);
static void egg_xfont_selector_get_property (GObject     *object,
				    guint        param_id,
				    GValue      *value,
				    GParamSpec  *pspec);

/* The maximum number of fontnames requested with XListFonts(). */
#define MAX_FONTS 32767

/* This is the largest field length we will accept. If a fontname has a field
 * larger than this we will skip it.
 */
#define XLFD_MAX_FIELD_LEN 64

/* Initial font metric & size (Remember point sizes are in decipoints).
 *  The font size should match one of those in the font_sizes array.
 */
#define INITIAL_METRIC		  EGG_XFONT_METRIC_PIXELS
#define INITIAL_FONT_SIZE	  14

/* This is the number of fields in an X Logical Font Description font name.
 * Note that we count the registry & encoding as 1.
 */
#define EGG_XLFD_NUM_FIELDS 13


typedef struct _GtkFontSelInfo GtkFontSelInfo;
typedef struct _FontInfo FontInfo;
typedef struct _FontStyle FontStyle;

/* This struct represents one family of fonts (with one foundry), e.g. adobe
 * courier or sony fixed. It stores the family name, the index of the foundry
 * name, and the index of and number of available styles.
 */

struct _FontInfo
{
  gchar   *family;
  guint16  foundry;
  gint	   style_index;
  guint16  nstyles;
};

struct _FontStyle
{
  guint16  properties[EGG_NUM_STYLE_PROPERTIES];
  gint	   pixel_sizes_index;
  guint16  npixel_sizes;
  gint	   point_sizes_index;
  guint16  npoint_sizes;
  guint8   flags;
};

struct _GtkFontSelInfo {
  
  /* This is a table with each FontInfo representing one font family+foundry */
  FontInfo *font_info;
  gint nfonts;
  
  /* This stores all the valid combinations of properties for every family.
     Each FontInfo holds an index into its own space in this one big array. */
  FontStyle *font_styles;
  gint nstyles;
  
  /* This stores all the font sizes available for every style.
     Each style holds an index into these arrays. */
  guint16 *pixel_sizes;
  guint16 *point_sizes;
  
  /* These are the arrays of strings of all possible weights, slants, 
   * set widths, spacings, charsets & foundries, and the amount of space
   * allocated for each array.
   */
  gchar **properties[EGG_NUM_FONT_PROPERTIES];
  guint16 nproperties[EGG_NUM_FONT_PROPERTIES];
  guint16 space_allocated[EGG_NUM_FONT_PROPERTIES];
};

/* These are the field numbers in the X Logical Font Description fontnames,
 * e.g. -adobe-courier-bold-o-normal--25-180-100-100-m-150-iso8859-1
 */
typedef enum
{
  XLFD_FOUNDRY		= 0,
  XLFD_FAMILY		= 1,
  XLFD_WEIGHT		= 2,
  XLFD_SLANT		= 3,
  XLFD_SET_WIDTH	= 4,
  XLFD_ADD_STYLE	= 5,
  XLFD_PIXELS		= 6,
  XLFD_POINTS		= 7,
  XLFD_RESOLUTION_X	= 8,
  XLFD_RESOLUTION_Y	= 9,
  XLFD_SPACING		= 10,
  XLFD_AVERAGE_WIDTH	= 11,
  XLFD_CHARSET		= 12
} FontField;

typedef enum
{
  WEIGHT	= 0,
  SLANT		= 1,
  SET_WIDTH	= 2,
  SPACING	= 3,
  CHARSET	= 4,
  FOUNDRY	= 5
} PropertyIndexType;

/* This is used to look up a field in a fontname given one of the above
 * property indices.
 */
static const FontField xlfd_index[EGG_NUM_FONT_PROPERTIES] = {
  XLFD_WEIGHT,
  XLFD_SLANT,
  XLFD_SET_WIDTH,
  XLFD_SPACING,
  XLFD_CHARSET,
  XLFD_FOUNDRY
};

#define XLFD_WEIGHT_BOLD "bold"

/* These are the positions of the properties in the filter table - x, y. */
static const gint filter_positions[EGG_NUM_FONT_PROPERTIES][2] = {
  { 1, 0 }, { 0, 2 }, { 1, 2 }, { 2, 2 }, { 2, 0 }, { 0, 0 }
};

/* These are what we use as the standard font sizes */
static const guint16 font_sizes[] = {
  8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 26, 28,
  32, 36, 40, 48, 56, 64, 72
};


static gchar* egg_xfont_selector_get_xlfd_field (const gchar *fontname,
						   FontField    field_num,
						   gchar       *buffer);
static gboolean egg_xfont_selector_is_xlfd_font_name (const gchar *fontname);
static void    egg_xfont_selector_get_fonts          (void);
static void    egg_xfont_selector_insert_font     (GSList         *fontnames[],
						   gint           *ntable,
						   gchar          *fontname);
static gint    egg_xfont_selector_insert_field    (gchar          *fontname,
						   gint            prop);

static gchar * egg_xfont_selector_create_xlfd     (gint            size,
						   EggXFontMetricType metric,
						   gchar          *foundry,
						   gchar          *family,
						   gchar          *weight,
						   gchar          *slant,
						   gchar          *set_width,
						   gchar          *spacing,
						   gchar	     *charset);

static gboolean egg_xfont_selector_style_visible(EggXFontSelector *fontsel,
						 FontInfo         *font,
						 gint            style_index);


static void update_family_menu (EggXFontSelector *selector);
static void update_size_menu (EggXFontSelector *selector);
static void family_changed (GtkOptionMenu *opt, EggXFontSelector *selector);
static void bold_toggled (GtkCheckButton *button, EggXFontSelector *selector);
static void size_changed (GtkOptionMenu *opt, EggXFontSelector *selector);


static GtkFontSelInfo *fontsel_info;

/* The initial size and increment of each of the arrays of property values. */
#define PROPERTY_ARRAY_INCREMENT	16

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


GType
egg_xfont_selector_get_type (void)
{
  static GtkType selector_type = 0;

  if (!selector_type)
    {
      static GTypeInfo selector_info =
      {
	sizeof (EggXFontSelectorClass),
	NULL,
	NULL,
	(GClassInitFunc) egg_xfont_selector_class_init,
	NULL,
	NULL,
	sizeof (EggXFontSelector),
	0,
	(GInstanceInitFunc) egg_xfont_selector_init,
	NULL
      };

      selector_type = g_type_register_static (GTK_TYPE_VBOX, 
					      "EggXFontSelector",
					      &selector_info, 0);
    }

  return selector_type;
}

static void
egg_xfont_selector_class_init (EggXFontSelectorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass*) klass;

  gobject_class->set_property = egg_xfont_selector_set_property;
  gobject_class->get_property = egg_xfont_selector_get_property;
  gobject_class->finalize = egg_xfont_selector_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,                   
                  G_STRUCT_OFFSET (EggXFontSelectorClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
		  0);

  egg_xfont_selector_get_fonts ();
}

static void
egg_xfont_selector_init (EggXFontSelector *selector)
{
  GtkWidget *size_label;
  GtkWidget *table;
  int prop;  
  
  /* Initialize the EggXFontSelection struct. We do this here in case any
   * callbacks are triggered while creating the interface.
   */

  selector->font_index = -1;
  selector->filtered_font_index = NULL;
  selector->size_options_map = NULL;
  selector->metric = INITIAL_METRIC;
  selector->size = INITIAL_FONT_SIZE;
  selector->want_bold = 0;
  selector->can_bold = 1;

  selector->filters[EGG_XFONT_FILTER_BASE].font_type = EGG_XFONT_ALL;
  selector->filters[EGG_XFONT_FILTER_USER].font_type = EGG_XFONT_BITMAP
    | EGG_XFONT_SCALABLE;

  for (prop = 0; prop < EGG_NUM_FONT_PROPERTIES; prop++)
    {
      selector->filters[EGG_XFONT_FILTER_BASE].property_filters[prop] = NULL;
      selector->filters[EGG_XFONT_FILTER_BASE].property_nfilters[prop] = 0;
      selector->filters[EGG_XFONT_FILTER_USER].property_filters[prop] = NULL;
      selector->filters[EGG_XFONT_FILTER_USER].property_nfilters[prop] = 0;
    }
  
  for (prop = 0; prop < EGG_NUM_STYLE_PROPERTIES; prop++)
    selector->property_values[prop] = 0;

  gtk_widget_push_composite_child ();
  
  table = gtk_table_new (2, 6, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 4);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  selector->family_options = gtk_option_menu_new ();
  selector->size_options = gtk_option_menu_new ();
  selector->family_label = gtk_label_new_with_mnemonic (_("_Font:"));
  gtk_misc_set_alignment (GTK_MISC (selector->family_label),
                          1.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (selector->family_label),
                        4, 0);
  size_label = gtk_label_new_with_mnemonic (_("Si_ze:"));
  selector->bold_check =
    gtk_check_button_new_with_mnemonic (_("_Use bold version of font"));

  gtk_widget_pop_composite_child ();

  gtk_label_set_mnemonic_widget (GTK_LABEL (selector->family_label),
				 selector->family_options);
  terminal_util_set_atk_name_description (selector->family_options,
                                          NULL, _("Click to choose font type"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (size_label),
				 selector->size_options);
  terminal_util_set_atk_name_description (selector->size_options,
                                          NULL, _("Click to choose font size"));

  /* FIXME: figure out what spacing looks best */

  gtk_table_attach (GTK_TABLE (table),
                    selector->family_label,
                    /* X direction */          /* Y direction */
                    0, 1,                      0, 1,
                    0,                         GTK_EXPAND | GTK_FILL,
                    0,                         0);

  gtk_table_attach (GTK_TABLE (table),
                    selector->family_options,
                    /* X direction */          /* Y direction */
                    1, 3,                      0, 1,
                    GTK_EXPAND | GTK_FILL,     GTK_EXPAND | GTK_FILL,
                    0,                         0);


  gtk_table_attach (GTK_TABLE (table),
                    size_label,
                    /* X direction */          /* Y direction */
                    3, 4,                      0, 1,
                    0,                         GTK_EXPAND | GTK_FILL,
                    0,                         0);

  gtk_table_attach (GTK_TABLE (table),
                    selector->size_options,
                    /* X direction */          /* Y direction */
                    4, 5,                      0, 1,
                    GTK_EXPAND | GTK_FILL,     GTK_EXPAND | GTK_FILL,
                    0,                         0);

  gtk_table_attach (GTK_TABLE (table),
                    selector->bold_check,
                    /* X direction */          /* Y direction */
                    1, 3,                      1, 2,
                    GTK_EXPAND | GTK_FILL,     GTK_EXPAND | GTK_FILL,
                    0,                         0);


  gtk_box_pack_start (GTK_BOX (selector), GTK_WIDGET (table),
                      FALSE, FALSE, 0);
  
  g_signal_connect (selector->family_options, "changed",
		    G_CALLBACK (family_changed), selector);
  g_signal_connect (selector->size_options, "changed",
		    G_CALLBACK (size_changed), selector);
  g_signal_connect (selector->bold_check, "toggled",
		    G_CALLBACK (bold_toggled), selector);

  update_family_menu (selector);

  gtk_widget_show_all (GTK_WIDGET (selector));
}

static void 
egg_xfont_selector_set_property (GObject         *object,
			guint            prop_id,
			const GValue    *value,
			GParamSpec      *pspec)
{
  EggXFontSelector *selector;

  selector = EGG_XFONT_SELECTOR (object);

  switch (prop_id)
    {
    default:      
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
egg_xfont_selector_get_property (GObject         *object,
			guint            prop_id,
			GValue          *value,
			GParamSpec      *pspec)
{
  EggXFontSelector *selector;

  selector = EGG_XFONT_SELECTOR (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
egg_xfont_selector_new (const gchar *label)
{
  EggXFontSelector *selector;

  selector =  g_object_new (egg_xfont_selector_get_type (), NULL);

  return GTK_WIDGET (selector);
}

/* This searches the given property table and returns the index of the given
 * string, or 0, which is the wildcard '*' index, if it's not found.
 */

static guint16
egg_xfont_selector_field_to_index (gchar **table,
				   gint    ntable,
				   gchar  *field)
{
  gint i;
  
  for (i = 0; i < ntable; i++)
    if (strcmp (field, table[i]) == 0)
      return i;
  
  return 0;
}

/* Returns the index of the given family, or -1 if not found */
static gint
egg_xfont_selector_find_font (EggXFontSelector *fontsel,
			      gchar	       *family,
			      guint16		foundry)
{
  FontInfo *font_info;
  gint lower, upper, middle = -1, cmp, nfonts;
  gint found_family = -1;
  
  font_info = fontsel_info->font_info;
  nfonts = fontsel_info->nfonts;
  if (nfonts == 0)
    return -1;
  
  /* Do a binary search to find the font family. */
  lower = 0;
  upper = nfonts;
  while (lower < upper)
    {
      middle = (lower + upper) >> 1;
      
      cmp = strcmp (family, font_info[middle].family);
      if (cmp == 0)
	{
	  found_family = middle;
	  cmp = strcmp(fontsel_info->properties[FOUNDRY][foundry],
		       fontsel_info->properties[FOUNDRY][font_info[middle].foundry]);
	}

      if (cmp == 0)
	return middle;
      else if (cmp < 0)
	upper = middle;
      else if (cmp > 0)
	lower = middle+1;
    }
  
  /* We couldn't find the family and foundry, but we may have just found the
     family, so we return that. */
  return found_family;
}

void
egg_xfont_selector_clear_filter (EggXFontSelector *fontsel)
{
  EggXFontFilter *filter;
  gint prop;
  
  /* Clear the filter data. */
  filter = &fontsel->filters[EGG_XFONT_FILTER_BASE];
  filter->font_type = EGG_XFONT_BITMAP | EGG_XFONT_SCALABLE;

  for (prop = 0; prop < EGG_NUM_FONT_PROPERTIES; prop++)
    {
      g_free(filter->property_filters[prop]);
      filter->property_filters[prop] = NULL;
      filter->property_nfilters[prop] = 0;
    }
  
  update_family_menu (fontsel);
}


gboolean
egg_xfont_selector_set_font_name (EggXFontSelector *fontsel,
				  const gchar *fontname)
{
  gchar *family, *field;
  gint index, size;
  guint16 foundry;
  gchar family_buffer[XLFD_MAX_FIELD_LEN];
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  
  g_return_val_if_fail (fontname != NULL, FALSE);

  /* Check it is a valid fontname. */
  if (!egg_xfont_selector_is_xlfd_font_name (fontname))
    return FALSE;
  
  family = egg_xfont_selector_get_xlfd_field (fontname, XLFD_FAMILY,
					      family_buffer);
  if (!family)
    return FALSE;
  
  field = egg_xfont_selector_get_xlfd_field (fontname, XLFD_FOUNDRY,
					     field_buffer);
  foundry = egg_xfont_selector_field_to_index (fontsel_info->properties[FOUNDRY],
						 fontsel_info->nproperties[FOUNDRY],
						 field);
  
  index = egg_xfont_selector_find_font(fontsel, family, foundry);

  if (index == -1) 
    return FALSE;
  
  if (fontsel->metric == EGG_XFONT_METRIC_POINTS)
    {
      field = egg_xfont_selector_get_xlfd_field (fontname, XLFD_POINTS,
						 field_buffer);
      size = atoi(field);
      if (size < 20)
	size = 20;
      fontsel->size = size;
      fontsel->metric = EGG_XFONT_METRIC_POINTS;
    }
  else
    {
      field = egg_xfont_selector_get_xlfd_field (fontname, XLFD_PIXELS,
						 field_buffer);
      size = atoi(field);
      if (size < 2)
	size = 2;
      fontsel->size = size;
      fontsel->metric = EGG_XFONT_METRIC_PIXELS;
    }

  fontsel->font_index = index;

  /* Check if the font is bold */
  field = egg_xfont_selector_get_xlfd_field (fontname, XLFD_WEIGHT,
					     field_buffer);
  fontsel->want_bold = strcmp (field, XLFD_WEIGHT_BOLD) == 0;

  /* Clear the filter and update the menus */
  egg_xfont_selector_clear_filter (fontsel);

  return TRUE;  
}

gchar*
egg_xfont_selector_get_font_name  (EggXFontSelector *fontsel)
{
  FontInfo *font;
  gchar *family_str, *foundry_str;
  gchar *property_str[EGG_NUM_STYLE_PROPERTIES];
  gint i, prop;
  EggXFontFilter *filter;
  
  /* If no family has been selected return NULL. */
  if (fontsel->font_index == -1)
    return NULL;
  
  filter = &fontsel->filters[EGG_XFONT_FILTER_BASE];

  font = &fontsel_info->font_info[fontsel->font_index];
  family_str = font->family;
  foundry_str = fontsel_info->properties[FOUNDRY][font->foundry];
  /* some fonts have a (nil) foundry */
  if (strcmp (foundry_str, "(nil)") == 0)
    foundry_str = "";

  /* Try to find a font matching the filters set. */
  for (i = 0; i < font->nstyles; i++)
    {
      if (egg_xfont_selector_style_visible (fontsel, font, i))
	{
	  FontStyle *style = &fontsel_info->font_styles[font->style_index + i];
	  memcpy (fontsel->property_values, style->properties,
		  sizeof (style->properties[0]) * EGG_NUM_STYLE_PROPERTIES);
	}
    }

  for (prop = 0; prop < EGG_NUM_STYLE_PROPERTIES; prop++)
    {
      property_str[prop] = fontsel_info->properties[prop][fontsel->property_values[prop]];
      if (strcmp (property_str[prop], "(nil)") == 0)
	property_str[prop] = "";
    }


  /* Return a bold font if sensitive (a bold style exists) and selected */
  if (fontsel->want_bold && fontsel->can_bold)
    property_str[WEIGHT] = XLFD_WEIGHT_BOLD;
  
  return egg_xfont_selector_create_xlfd (fontsel->size,
					 fontsel->metric,
					 foundry_str,
					 family_str,
					 property_str[WEIGHT],
					 property_str[SLANT],
					 property_str[SET_WIDTH],
					 property_str[SPACING],
					 property_str[CHARSET]);
}



static gchar*
egg_xfont_selector_expand_slant_code(gchar *slant)
{
  if      (!g_ascii_strcasecmp(slant, "r"))   return(_("roman"));
  else if (!g_ascii_strcasecmp(slant, "i"))   return(_("italic"));
  else if (!g_ascii_strcasecmp(slant, "o"))   return(_("oblique"));
  else if (!g_ascii_strcasecmp(slant, "ri"))  return(_("reverse italic"));
  else if (!g_ascii_strcasecmp(slant, "ro"))  return(_("reverse oblique"));
  else if (!g_ascii_strcasecmp(slant, "ot"))  return(_("other"));
  return slant;
}

static gchar*
egg_xfont_selector_expand_spacing_code(gchar *spacing)
{
  if      (!g_ascii_strcasecmp(spacing, "p")) return(_("proportional"));
  else if (!g_ascii_strcasecmp(spacing, "m")) return(_("monospaced"));
  else if (!g_ascii_strcasecmp(spacing, "c")) return(_("char cell"));
  return spacing;
}

  
void
egg_xfont_selector_set_filter	(EggXFontSelector *fontsel,
				 EggXFontFilterType filter_type,
				 EggXFontType	   font_type,
				 gchar		 **foundries,
				 gchar		 **weights,
				 gchar		 **slants,
				 gchar		 **setwidths,
				 gchar		 **spacings,
				 gchar		 **charsets)
{
  EggXFontFilter *filter;
  gchar **filter_strings [EGG_NUM_FONT_PROPERTIES];
  gchar *filter_string;
  gchar *property, *property_alt;
  gint prop, nfilters, i, j, num_found;

  /* Put them into an array so we can use a simple loop. */
  filter_strings[FOUNDRY]   = foundries;
  filter_strings[WEIGHT]    = weights;
  filter_strings[SLANT]     = slants;
  filter_strings[SET_WIDTH] = setwidths;
  filter_strings[SPACING]   = spacings;
  filter_strings[CHARSET]   = charsets;

  filter = &fontsel->filters[filter_type];
  filter->font_type = font_type;
      
  /* Free the old filter data, and insert the new. */
  for (prop = 0; prop < EGG_NUM_FONT_PROPERTIES; prop++)
    {
      g_free(filter->property_filters[prop]);
      filter->property_filters[prop] = NULL;
      filter->property_nfilters[prop] = 0;
      
      if (filter_strings[prop])
	{
	  /* Count how many items in the new array. */
	  nfilters = 0;
	  while (filter_strings[prop][nfilters])
	    nfilters++;

	  filter->property_filters[prop] = g_new(guint16, nfilters);
	  filter->property_nfilters[prop] = 0;

	  /* Now convert the strings to property indices. */
	  num_found = 0;
	  for (i = 0; i < nfilters; i++)
	    {
	      filter_string = filter_strings[prop][i];
	      for (j = 0; j < fontsel_info->nproperties[prop]; j++)
		{
		  property = _(fontsel_info->properties[prop][j]);
		  property_alt = NULL;
		  if (prop == SLANT)
		    property_alt = egg_xfont_selector_expand_slant_code(property);
		  else if (prop == SPACING)
		    property_alt = egg_xfont_selector_expand_spacing_code(property);
		  if (!strcmp (filter_string, property)
		      || (property_alt && !strcmp (filter_string, property_alt)))
		    {
		      filter->property_filters[prop][num_found] = j;
		      num_found++;
		      break;
		    }
		}
	    }
	  filter->property_nfilters[prop] = num_found;
	}
    }

  update_family_menu (fontsel);
}

static void
bold_toggled (GtkCheckButton *check, EggXFontSelector *selector)
{
  selector->want_bold =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (selector->bold_check));

  g_signal_emit (G_OBJECT (selector), signals[CHANGED], 0);
}

static void
family_changed (GtkOptionMenu *opt, EggXFontSelector *selector)
{
  FontInfo *info;
  FontStyle *style;
  char *prop;
  int i;
  
  selector->font_index = gtk_option_menu_get_history (GTK_OPTION_MENU (opt));
  selector->font_index = selector->filtered_font_index[selector->font_index];

  info = &fontsel_info->font_info[selector->font_index];

  /* Figure out if the font can be bolded and update the checkbox */
  selector->can_bold = 0;
  for (i = 0; i < info->nstyles; i++)
    {
      style = &fontsel_info->font_styles[info->style_index + i];
      prop = fontsel_info->properties[WEIGHT][style->properties[WEIGHT]];

      if (strcmp (prop, XLFD_WEIGHT_BOLD) == 0)
	selector->can_bold = 1;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (selector->bold_check),
			    selector->can_bold);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selector->bold_check),
				selector->want_bold);

  update_size_menu (selector);
}

static void
size_changed (GtkOptionMenu *opt, EggXFontSelector *selector)
{
  guint16 *pixel_sizes;
  FontInfo *info;
  FontStyle *style;
  int i;

  info = &fontsel_info->font_info[selector->font_index];
  style = &fontsel_info->font_styles[info->style_index];
  pixel_sizes = &fontsel_info->pixel_sizes[style->pixel_sizes_index];

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (opt));
  selector->size = selector->size_options_map[i];

  g_signal_emit (G_OBJECT (selector), signals[CHANGED], 0);
}

/* Returns TRUE if the style is not currently filtered out. */
static gboolean
egg_xfont_selector_style_visible(EggXFontSelector *fontsel,
				 FontInfo         *font,
				 gint              style_index)
{
  FontStyle *styles, *style;
  EggXFontFilter *filter;
  guint16 value;
  gint prop, i, j;
  gboolean matched;
  
  styles = &fontsel_info->font_styles[font->style_index];
  style = &styles[style_index];

  for (prop = 0; prop < EGG_NUM_STYLE_PROPERTIES; prop++)
    {
      value = style->properties[prop];
      
      /* Check each filter. */
      for (i = 0; i < EGG_NUM_FONT_FILTERS; i++)
	{
	  filter = &fontsel->filters[i];

	  if (filter->property_nfilters[prop] != 0)
	    {
	      matched = FALSE;
	      for (j = 0; j < filter->property_nfilters[prop]; j++)
		{
		  if (value == filter->property_filters[prop][j])
		    {
		      matched = TRUE;
		      break;
		    }
		}
	      if (!matched)
		return FALSE;
	    }
	}
    }
  return TRUE;
}

static void
update_family_menu (EggXFontSelector *selector)
{
  GtkWidget *menu, *mi;
  int i, j, k, style, matched, nfonts, filter_index, selected_font_pos;
  gchar *foundry, *name;
  FontInfo *info, *start_info;
  EggXFontFilter *filter;

  start_info = fontsel_info->font_info;
  nfonts = fontsel_info->nfonts;

  g_free (selector->filtered_font_index);
  selector->filtered_font_index = g_new (gint, nfonts);
  filter_index = 0;
  selected_font_pos = -1;

  menu = gtk_menu_new ();

  for (i = 0; i < nfonts; i++)
    {
      info = &fontsel_info->font_info[i];
      
      /* Check if the foundry passes through all filters. */
      matched = TRUE;
      for (k = 0; k < EGG_NUM_FONT_FILTERS; k++)
	{
	  filter = &selector->filters[k];

	  if (filter->property_nfilters[FOUNDRY] != 0)
	    {
	      matched = FALSE;
	      for (j = 0; j < filter->property_nfilters[FOUNDRY]; j++)
		{
		  if (info->foundry == filter->property_filters[FOUNDRY][j])
		    {
		      matched = TRUE;
		      break;
		    }
		}
	      if (!matched)
		break;
	    }
	}
      
      if (!matched)
	continue;

      /* Now check if the other properties are matched in at least one style.*/
      matched = FALSE;
      for (style = 0; style < info->nstyles; style++)
	{
	  if (egg_xfont_selector_style_visible (selector, info, style))
	    {
	      matched = TRUE;
	      break;
	    }
	}

      if (!matched)
	continue;

      if (i == selector->font_index) {
	selected_font_pos = filter_index;
      }

      selector->filtered_font_index[filter_index++] = i;

      foundry = fontsel_info->properties[FOUNDRY][info->foundry];
      name = g_strdup_printf ("%s (%s)", info->family, foundry);
      
      mi = gtk_menu_item_new_with_label (name);
      gtk_widget_show (mi);
      g_free (name);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (mi));
    }

  /* Try to keep last selected font current and prevent more than one
   * change signal being sent
   */

  if (selected_font_pos != -1) {
    g_signal_handlers_block_by_func (selector->family_options, family_changed,
				     selector);    
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (selector->family_options), menu);

  if (selected_font_pos != -1) {
    g_signal_handlers_unblock_by_func (selector->family_options,
				       family_changed, selector);
    gtk_option_menu_set_history (GTK_OPTION_MENU (selector->family_options),
				 selected_font_pos);
  }

  gtk_widget_show_all (GTK_WIDGET (selector->family_options));
}

static void
update_size_menu (EggXFontSelector *selector)
{
  GtkWidget *menu, *mi;
  guint16 *pixel_sizes;
  FontInfo *info;
  FontStyle *style;
  gchar buffer[16];
  const guint16 *standard_sizes;
  guint16 *bitmapped_sizes;
  gint nstandard_sizes, nbitmapped_sizes;
  gfloat bitmap_size_float = 0.;
  guint16 bitmap_size = 0;
  gboolean can_match;
  gint type_filter;
  int map_pos, num;
  int selected_size_pos;

  g_return_if_fail (selector->font_index != -1);

  selected_size_pos = -1;

  menu = gtk_menu_new ();

  info = &fontsel_info->font_info[selector->font_index];
  style = &fontsel_info->font_styles[info->style_index];
  pixel_sizes = &fontsel_info->pixel_sizes[style->pixel_sizes_index];

  standard_sizes = font_sizes;
  nstandard_sizes = sizeof(font_sizes) / sizeof(font_sizes[0]);

  if (selector->metric == EGG_XFONT_METRIC_POINTS)
    {
      bitmapped_sizes = &fontsel_info->point_sizes[style->point_sizes_index];
      nbitmapped_sizes = style->npoint_sizes;
    }
  else
    {
      bitmapped_sizes = &fontsel_info->pixel_sizes[style->pixel_sizes_index];
      nbitmapped_sizes = style->npixel_sizes;
    }

  /* Only show the standard sizes if a scalable font is available. */
  type_filter = selector->filters[EGG_XFONT_FILTER_BASE].font_type
    & selector->filters[EGG_XFONT_FILTER_USER].font_type;

  if (!((style->flags & EGG_XFONT_SCALABLE_BITMAP
         && type_filter & EGG_XFONT_SCALABLE_BITMAP)
        || (style->flags & EGG_XFONT_SCALABLE
            && type_filter & EGG_XFONT_SCALABLE)))
    nstandard_sizes = 0;


  g_free (selector->size_options_map);
  selector->size_options_map = g_new (int, nstandard_sizes + nbitmapped_sizes);
  map_pos = 0;

 /* Interleave the standard sizes with the bitmapped sizes so we get a list
  * of ascending sizes. If the metric is points, we have to convert the
  * decipoints to points.
  */

  while (nstandard_sizes || nbitmapped_sizes)
    {
      can_match = TRUE;

      if (nbitmapped_sizes)
        {
          if (selector->metric == EGG_XFONT_METRIC_POINTS)
            {
              if (*bitmapped_sizes % 10 != 0)
                can_match = FALSE;
              bitmap_size = *bitmapped_sizes / 10;
              bitmap_size_float = *bitmapped_sizes / 10;
            }
          else
            {
              bitmap_size = *bitmapped_sizes;
              bitmap_size_float = *bitmapped_sizes;
            }
        }

      if (can_match && nstandard_sizes && nbitmapped_sizes
          && *standard_sizes == bitmap_size)
        {
          sprintf(buffer, "%i", *standard_sizes);
          standard_sizes++;
          nstandard_sizes--;
          bitmapped_sizes++;
          nbitmapped_sizes--;
        }
      else if (nstandard_sizes
               && (!nbitmapped_sizes
                   || (gfloat)*standard_sizes < bitmap_size_float))
        {
          sprintf(buffer, "%i", *standard_sizes);
          standard_sizes++;
          nstandard_sizes--;
        }
      else
	{
          if (selector->metric == EGG_XFONT_METRIC_POINTS)
            {
              if (*bitmapped_sizes % 10 == 0)
                sprintf(buffer, "%i", *bitmapped_sizes / 10);
              else
                sprintf(buffer, "%i.%i", *bitmapped_sizes / 10,
                        *bitmapped_sizes % 10);
            }
          else
            {
              sprintf(buffer, "%i", *bitmapped_sizes);
            }
          bitmapped_sizes++;
	  nbitmapped_sizes--;
        }

      /* FIXME: This is awful. The entire function should be rewritten. */
      sscanf (buffer, "%i", &num);

      if (selector->size == num) {
	selected_size_pos = map_pos;
      }

      selector->size_options_map[map_pos++] = num;

      mi = gtk_menu_item_new_with_label (buffer);
      gtk_widget_show (mi);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (mi));
    }

 /* Try to keep last selected size font current.
  * and if we change the size, prevent more than one change signal being sent
  */

  if (selected_size_pos != -1) {
    g_signal_handlers_block_by_func (selector->size_options, size_changed,
				     selector);    
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (selector->size_options), menu);

  if (selected_size_pos != -1) {
    g_signal_handlers_unblock_by_func (selector->size_options, size_changed,
				     selector);
    gtk_option_menu_set_history (GTK_OPTION_MENU (selector->size_options),
				 selected_size_pos);
  }

  gtk_widget_show_all (GTK_WIDGET (selector->size_options));
}


/*****************************************************************************
 * These functions all deal with creating the main class arrays containing
 * the data about all available fonts.
 *****************************************************************************/

/* This inserts the given fontname into the FontInfo table.
   If a FontInfo already exists with the same family and foundry, then the
   fontname is added to the FontInfos list of fontnames, else a new FontInfo
   is created and inserted in alphabetical order in the table. */
static void
egg_xfont_selector_insert_font (GSList		      *fontnames[],
				gint		      *ntable,
				gchar		      *fontname)
{
  FontInfo *table;
  FontInfo temp_info;
  GSList *temp_fontname;
  gchar *family;
  gboolean family_exists = FALSE;
  gint foundry;
  gint lower, upper;
  gint middle, cmp;
  gchar family_buffer[XLFD_MAX_FIELD_LEN];
  
  table = fontsel_info->font_info;
  
  /* insert a fontname into a table */
  family = egg_xfont_selector_get_xlfd_field (fontname, XLFD_FAMILY,
					      family_buffer);
  if (!family)
    return;
  
  foundry = egg_xfont_selector_insert_field (fontname, FOUNDRY);
  
  lower = 0;
  if (*ntable > 0)
    {
      /* Do a binary search to determine if we have already encountered
       *  a font with this family & foundry. */
      upper = *ntable;
      while (lower < upper)
	{
	  middle = (lower + upper) >> 1;
	  
	  cmp = strcmp (family, table[middle].family);
	  /* If the family matches we sort by the foundry. */
	  if (cmp == 0)
	    {
	      family_exists = TRUE;
	      family = table[middle].family;
	      cmp = strcmp(fontsel_info->properties[FOUNDRY][foundry],
			   fontsel_info->properties[FOUNDRY][table[middle].foundry]);
	    }
	  
	  if (cmp == 0)
	    {
	      fontnames[middle] = g_slist_prepend (fontnames[middle],
						   fontname);
	      return;
	    }
	  else if (cmp < 0)
	    upper = middle;
	  else
	    lower = middle+1;
	}
    }
  
  /* Add another entry to the table for this new font family */
  temp_info.family = family_exists ? family : g_strdup(family);
  temp_info.foundry = foundry;
  temp_fontname = g_slist_prepend (NULL, fontname);
  
  (*ntable)++;
  
  /* Quickly insert the entry into the table in sorted order
   *  using a modification of insertion sort and the knowledge
   *  that the entries proper position in the table was determined
   *  above in the binary search and is contained in the "lower"
   *  variable. */
  if (*ntable > 1)
    {
      upper = *ntable - 1;
      while (lower != upper)
	{
	  table[upper] = table[upper-1];
	  fontnames[upper] = fontnames[upper-1];
	  upper--;
	}
    }
  table[lower] = temp_info;
  fontnames[lower] = temp_fontname;
}

/* This checks that the specified field of the given fontname is in the
   appropriate properties array. If not it is added. Thus eventually we get
   arrays of all possible weights/slants etc. It returns the array index. */
static gint
egg_xfont_selector_insert_field (gchar		       *fontname,
				 gint			prop)
{
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  guint16 index;
  
  field = egg_xfont_selector_get_xlfd_field (fontname, xlfd_index[prop],
					     field_buffer);
  if (!field)
    return 0;
  
  /* If the field is already in the array just return its index. */
  for (index = 0; index < fontsel_info->nproperties[prop]; index++)
    if (!strcmp(field, fontsel_info->properties[prop][index]))
      return index;
  
  /* Make sure we have enough space to add the field. */
  if (fontsel_info->nproperties[prop] == fontsel_info->space_allocated[prop])
    {
      fontsel_info->space_allocated[prop] += PROPERTY_ARRAY_INCREMENT;
      fontsel_info->properties[prop] = g_realloc(fontsel_info->properties[prop],
						 sizeof(gchar*)
						 * fontsel_info->space_allocated[prop]);
    }
  
  /* Add the new field. */
  index = fontsel_info->nproperties[prop];
  fontsel_info->properties[prop][index] = g_strdup(field);
  fontsel_info->nproperties[prop]++;
  return index;
}


static void
egg_xfont_selector_get_fonts (void)
{
  gchar **xfontnames;
  GSList **fontnames;
  gchar *fontname;
  GSList * temp_list;
  gint num_fonts;
  gint i, prop, style, size;
  gint npixel_sizes = 0, npoint_sizes = 0;
  FontInfo *font;
  FontStyle *current_style, *prev_style, *tmp_style;
  gboolean matched_style, found_size;
  gint pixels, points, res_x, res_y;
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  guint8 flags;
  guint16 *pixel_sizes, *point_sizes, *tmp_sizes;
  
  fontsel_info = g_new (GtkFontSelInfo, 1);
  
  /* Get a maximum of MAX_FONTS fontnames from the X server.
   * Use "-*" as the pattern rather than "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" since
   * the latter may result in fonts being returned which don't actually exist.
   * xlsfonts also uses "*" so I think it's OK. "-*" gets rid of aliases.
  */

  xfontnames = XListFonts (GDK_DISPLAY(), "-*", MAX_FONTS, &num_fonts);

  /* Output a warning if we actually get MAX_FONTS fonts. */
  if (num_fonts == MAX_FONTS)
    g_warning(_("MAX_FONTS exceeded. Some fonts may be missing."));
  
  /* The maximum size of all these tables is the number of font names
     returned. We realloc them later when we know exactly how many
     unique entries there are. */
  fontsel_info->font_info = g_new (FontInfo, num_fonts);
  fontsel_info->font_styles = g_new (FontStyle, num_fonts);
  fontsel_info->pixel_sizes = g_new (guint16, num_fonts);
  fontsel_info->point_sizes = g_new (guint16, num_fonts);
  
  fontnames = g_new (GSList*, num_fonts);
  
  /* Create the initial arrays for the property value strings, though they
     may be realloc'ed later. Put the wildcard '*' in the first elements. */
  for (prop = 0; prop < EGG_NUM_FONT_PROPERTIES; prop++)
    {
      fontsel_info->properties[prop] = g_new(gchar*, PROPERTY_ARRAY_INCREMENT);
      fontsel_info->space_allocated[prop] = PROPERTY_ARRAY_INCREMENT;
      fontsel_info->nproperties[prop] = 1;
      fontsel_info->properties[prop][0] = "*";
    }
  
  
  /* Insert the font families into the main table, sorted by family and
   * foundry (fonts with different foundries are placed in seaparate FontInfos.
   * All fontnames in each family + foundry are placed into the fontnames
   * array of lists.
   */

  fontsel_info->nfonts = 0;
  for (i = 0; i < num_fonts; i++)
    {
#ifdef FONTSEL_DEBUG
      g_message("%s\n", xfontnames[i]);
#endif
      if (egg_xfont_selector_is_xlfd_font_name (xfontnames[i]))
	egg_xfont_selector_insert_font (fontnames, &fontsel_info->nfonts, xfontnames[i]);
      else
	{
#ifdef FONTSEL_DEBUG
	  g_warning("Skipping invalid font: %s", xfontnames[i]);
#endif
	}
    }
  
  
  /* Since many font names will be in the same FontInfo not all of the
     allocated FontInfo table will be used, so we will now reallocate it
     with the real size. */
  fontsel_info->font_info = g_realloc(fontsel_info->font_info,
				      sizeof(FontInfo) * fontsel_info->nfonts);
  
  
  /* Now we work out which choices of weight/slant etc. are valid for each
     font. */
  fontsel_info->nstyles = 0;
  current_style = fontsel_info->font_styles;
  for (i = 0; i < fontsel_info->nfonts; i++)
    {
      font = &fontsel_info->font_info[i];
      
      /* Use the next free position in the styles array. */
      font->style_index = fontsel_info->nstyles;
      
      /* Now step through each of the fontnames with this family, and create
       * style for each fontname. Each style contains the index into the
       * weights/slants etc. arrays, and a number of pixel/point sizes.
       */

      style = 0;
      temp_list = fontnames[i];
      while (temp_list)
	{
	  fontname = temp_list->data;
	  temp_list = temp_list->next;
	  
	  for (prop = 0; prop < EGG_NUM_STYLE_PROPERTIES; prop++)
	    {
	      current_style->properties[prop]
		= egg_xfont_selector_insert_field (fontname, prop);
	    }
	  current_style->pixel_sizes_index = npixel_sizes;
	  current_style->npixel_sizes = 0;
	  current_style->point_sizes_index = npoint_sizes;
	  current_style->npoint_sizes = 0;
	  current_style->flags = 0;
	  
	  
	  field = egg_xfont_selector_get_xlfd_field (fontname, XLFD_PIXELS,
						     field_buffer);
	  pixels = atoi(field);
	  
	  field = egg_xfont_selector_get_xlfd_field (fontname, XLFD_POINTS,
						     field_buffer);
	  points = atoi(field);
	  
	  field = egg_xfont_selector_get_xlfd_field (fontname,
						     XLFD_RESOLUTION_X,
						     field_buffer);
	  res_x = atoi(field);
	  
	  field = egg_xfont_selector_get_xlfd_field (fontname,
						     XLFD_RESOLUTION_Y,
						     field_buffer);
	  res_y = atoi(field);
	  
	  if (pixels == 0 && points == 0)
	    {
	      if (res_x == 0 && res_y == 0)
		flags = EGG_XFONT_SCALABLE;
	      else
		flags = EGG_XFONT_SCALABLE_BITMAP;
	    }
	  else
	    flags = EGG_XFONT_BITMAP;
	  
	  /* Now we check to make sure that the style is unique. If it isn't
	     we forget it. */
	  prev_style = fontsel_info->font_styles + font->style_index;
	  matched_style = FALSE;
	  while (prev_style < current_style)
	    {
	      matched_style = TRUE;
	      for (prop = 0; prop < EGG_NUM_STYLE_PROPERTIES; prop++)
		{
		  if (prev_style->properties[prop]
		      != current_style->properties[prop])
		    {
		      matched_style = FALSE;
		      break;
		    }
		}
	      if (matched_style)
		break;
	      prev_style++;
	    }
	  
	  /* If we matched an existing style, we need to add the pixels &
	     point sizes to the style. If not, we insert the pixel & point
	     sizes into our new style. Note that we don't add sizes for
	     scalable fonts. */
	  if (matched_style)
	    {
	      prev_style->flags |= flags;
	      if (flags == EGG_XFONT_BITMAP)
		{
		  pixel_sizes = fontsel_info->pixel_sizes
		    + prev_style->pixel_sizes_index;
		  found_size = FALSE;
		  for (size = 0; size < prev_style->npixel_sizes; size++)
		    {
		      if (pixels == *pixel_sizes)
			{
			  found_size = TRUE;
			  break;
			}
		      else if (pixels < *pixel_sizes)
			break;
		      pixel_sizes++;
		    }
		  /* We need to move all the following pixel sizes up, and also
		     update the indexes of any following styles. */
		  if (!found_size)
		    {
		      for (tmp_sizes = fontsel_info->pixel_sizes + npixel_sizes;
			   tmp_sizes > pixel_sizes; tmp_sizes--)
			*tmp_sizes = *(tmp_sizes - 1);
		      
		      *pixel_sizes = pixels;
		      npixel_sizes++;
		      prev_style->npixel_sizes++;
		      
		      tmp_style = prev_style + 1;
		      while (tmp_style < current_style)
			{
			  tmp_style->pixel_sizes_index++;
			  tmp_style++;
			}
		    }
		  
		  point_sizes = fontsel_info->point_sizes
		    + prev_style->point_sizes_index;
		  found_size = FALSE;
		  for (size = 0; size < prev_style->npoint_sizes; size++)
		    {
		      if (points == *point_sizes)
			{
			  found_size = TRUE;
			  break;
			}
		      else if (points < *point_sizes)
			break;
		      point_sizes++;
		    }
		  /* We need to move all the following point sizes up, and also
		     update the indexes of any following styles. */
		  if (!found_size)
		    {
		      for (tmp_sizes = fontsel_info->point_sizes + npoint_sizes;
			   tmp_sizes > point_sizes; tmp_sizes--)
			*tmp_sizes = *(tmp_sizes - 1);
		      
		      *point_sizes = points;
		      npoint_sizes++;
		      prev_style->npoint_sizes++;
		      
		      tmp_style = prev_style + 1;
		      while (tmp_style < current_style)
			{
			  tmp_style->point_sizes_index++;
			  tmp_style++;
			}
		    }
		}
	    }
	  else
	    {
	      current_style->flags = flags;
	      if (flags == EGG_XFONT_BITMAP)
		{
		  fontsel_info->pixel_sizes[npixel_sizes++] = pixels;
		  current_style->npixel_sizes = 1;
		  fontsel_info->point_sizes[npoint_sizes++] = points;
		  current_style->npoint_sizes = 1;
		}
	      style++;
	      fontsel_info->nstyles++;
	      current_style++;
	    }
	}
      g_slist_free(fontnames[i]);
      
      /* Set nstyles to the real value, minus duplicated fontnames.
	 Note that we aren't using all the allocated memory if fontnames are
	 duplicated. */
      font->nstyles = style;
    }
  
  /* Since some repeated styles may be skipped we won't have used all the
     allocated space, so we will now reallocate it with the real size. */
  fontsel_info->font_styles = g_realloc(fontsel_info->font_styles,
					sizeof(FontStyle) * fontsel_info->nstyles);
  fontsel_info->pixel_sizes = g_realloc(fontsel_info->pixel_sizes,
					sizeof(guint16) * npixel_sizes);
  fontsel_info->point_sizes = g_realloc(fontsel_info->point_sizes,
					sizeof(guint16) * npoint_sizes);
  g_free(fontnames);
  XFreeFontNames (xfontnames);
  
  
  /* Debugging Output */
  /* This outputs all FontInfos. */
#ifdef FONTSEL_DEBUG
  g_message("\n\n Font Family           Weight    Slant     Set Width Spacing   Charset\n\n");
  for (i = 0; i < fontsel_info->nfonts; i++)
    {
      FontInfo *font = &fontsel_info->font_info[i];
      FontStyle *styles = fontsel_info->font_styles + font->style_index;
      for (style = 0; style < font->nstyles; style++)
	{
	  g_message("%5i %-16.16s ", i, font->family);
	  for (prop = 0; prop < EGG_NUM_STYLE_PROPERTIES; prop++)
	    g_message("%-9.9s ",
		      fontsel_info->properties[prop][styles->properties[prop]]);
	  g_message("\n      ");
	  
	  if (styles->flags & EGG_XFONT_BITMAP)
	    g_message("Bitmapped font  ");
	  if (styles->flags & EGG_XFONT_SCALABLE)
	    g_message("Scalable font  ");
	  if (styles->flags & EGG_XFONT_SCALABLE_BITMAP)
	    g_message("Scalable-Bitmapped font  ");
	  g_message("\n");
	  
	  if (styles->npixel_sizes)
	    {
	      g_message("      Pixel sizes: ");
	      tmp_sizes = fontsel_info->pixel_sizes + styles->pixel_sizes_index;
	      for (size = 0; size < styles->npixel_sizes; size++)
		g_message("%i ", *tmp_sizes++);
	      g_message("\n");
	    }
	  
	  if (styles->npoint_sizes)
	    {
	      g_message("      Point sizes: ");
	      tmp_sizes = fontsel_info->point_sizes + styles->point_sizes_index;
	      for (size = 0; size < styles->npoint_sizes; size++)
		g_message("%i ", *tmp_sizes++);
	      g_message("\n");
	    }
	  
	  g_message("\n");
	  styles++;
	}
    }
  /* This outputs all available properties. */
  for (prop = 0; prop < EGG_NUM_FONT_PROPERTIES; prop++)
    {
      g_message("Property: %s\n", xlfd_field_names[xlfd_index[prop]]);
      for (i = 0; i < fontsel_info->nproperties[prop]; i++)
        g_message("  %s\n", fontsel_info->properties[prop][i]);
    }
#endif
}

/*****************************************************************************
 * These functions all deal with X Logical Font Description (XLFD) fontnames.
 * See the freely available documentation about this.
 *****************************************************************************/


/*
 * Returns TRUE if the fontname is a valid XLFD.
 * (It just checks if the number of dashes is 14, and that each
 * field < XLFD_MAX_FIELD_LEN  characters long - that's not in the XLFD but it
 * makes it easier for me).
 */
static gboolean
egg_xfont_selector_is_xlfd_font_name (const gchar *fontname)
{
  gint i = 0;
  gint field_len = 0;
  
  while (*fontname)
    {
      if (*fontname++ == '-')
	{
	  if (field_len > XLFD_MAX_FIELD_LEN) return FALSE;
	  field_len = 0;
	  i++;
	}
      else
	field_len++;
    }
  
  return (i == 14) ? TRUE : FALSE;
}


/*
 * This returns a X Logical Font Description font name, given all the pieces.
 * Note: this retval must be freed by the caller.
 */
static gchar *
egg_xfont_selector_create_xlfd (gint		  size,
				EggXFontMetricType metric,
				gchar		  *foundry,
				gchar		  *family,
				gchar		  *weight,
				gchar		  *slant,
				gchar		  *set_width,
				gchar		  *spacing,
				gchar		  *charset)
{
  gchar buffer[16];
  gchar *pixel_size = "*", *point_size = "*", *fontname;
  
  if (size <= 0)
    return NULL;
  
  sprintf (buffer, "%d", (int) size);
  if (metric == EGG_XFONT_METRIC_PIXELS)
    pixel_size = buffer;
  else
    point_size = buffer;
  
  fontname = g_strdup_printf("-%s-%s-%s-%s-%s-*-%s-%s-*-*-%s-*-%s",
			     foundry, family, weight, slant, set_width,
			     pixel_size, point_size, spacing, charset);

  return fontname;
}

/*
 * This fills the buffer with the specified field from the X Logical Font
 * Description name, and returns it. If fontname is NULL or the field is
 * longer than XFLD_MAX_FIELD_LEN it returns NULL.
 * Note: For the charset field, we also return the encoding, e.g. 'iso8859-1'.
 */
static gchar*
egg_xfont_selector_get_xlfd_field (const gchar *fontname,
				   FontField    field_num,
				   gchar       *buffer)
{
  const gchar *t1, *t2;
  gint countdown, len, num_dashes;
  
  if (!fontname)
    return NULL;
  
  /* we assume this is a valid fontname...that is, it has 14 fields */
  
  countdown = field_num;
  t1 = fontname;
  while (*t1 && (countdown >= 0))
    if (*t1++ == '-')
      countdown--;
  
  num_dashes = (field_num == XLFD_CHARSET) ? 2 : 1;
  for (t2 = t1; *t2; t2++)
    { 
      if (*t2 == '-' && --num_dashes == 0)
	break;
    }
  
  if (t1 != t2)
    {
      char *p;
      
      /* Check we don't overflow the buffer */
      len = (long) t2 - (long) t1;
      if (len > XLFD_MAX_FIELD_LEN - 1)
	return NULL;
      strncpy (buffer, t1, len);
      buffer[len] = 0;

      /* Convert to lower case. */
      p = buffer;
      while (*p)
        {
          *p = g_ascii_tolower (*p);
          ++p;
        }
    }
  else
    strcpy(buffer, "(nil)");
  
  return buffer;
}

static void
egg_xfont_selector_finalize (GObject *object)
{
  GObjectClass *parent_class;
  EggXFontSelector *sel;
  EggXFontFilter *filter;
  gint prop;

  sel = EGG_XFONT_SELECTOR (object);
  parent_class = g_type_class_ref (g_type_parent (G_OBJECT_TYPE (object)));

  /* Clear the filter data. */
  filter = &sel->filters[EGG_XFONT_FILTER_BASE];
  
  for (prop = 0; prop < EGG_NUM_FONT_PROPERTIES; prop++)
    {
      g_free(filter->property_filters[prop]);
      filter->property_filters[prop] = NULL;
      filter->property_nfilters[prop] = 0;
    }
  
  g_free (sel->filtered_font_index);
  g_free (sel->size_options_map);

  /* FIXME: can finalize be called multiple times? */

  sel->filtered_font_index = NULL;
  sel->size_options_map = NULL;

  if (parent_class->finalize) {
    parent_class->finalize (object);
  }

  g_type_class_unref (parent_class);
}
