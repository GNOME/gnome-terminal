/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GnomeFontSelection widget for Gnome, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __EGG_XFONT_SELECTOR_H__
#define __EGG_XFONT_SELECTOR_H__

#include <gdk/gdk.h>
#include <gtk/gtkbin.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EGG_TYPE_XFONT_SELECTOR                (egg_xfont_selector_get_type ())
#define EGG_XFONT_SELECTOR(obj)                  (GTK_CHECK_CAST ((obj), EGG_TYPE_XFONT_SELECTOR, EggXFontSelector))
#define EGG_XFONT_SELECTOR_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), EGG_TYPE_XFONT_SELECTOR, EggXFontSelectorClass))
#define GTK_IS_XFONT_SELECTOR(obj)               (GTK_CHECK_TYPE ((obj), EGG_TYPE_XFONT_SELECTOR))
#define GTK_IS_XFONT_SELECTOR_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), EGG_TYPE_XFONT_SELECTOR))
#define EGG_XFONT_SELECTOR_GET_CLASS(obj)        (GTK_CHECK_GET_CLASS ((obj), EGG_TYPE_XFONT_SELECTOR, EggXFontSelectorClass))


/* This is the number of properties which we keep in the properties array,
 * i.e. Weight, Slant, Set Width, Spacing, Charset & Foundry.
 */
#define EGG_NUM_FONT_PROPERTIES  6

/* This is the number of properties each style has i.e. Weight, Slant,
 * Set Width, Spacing & Charset. Note that Foundry is not included,
 *  since it is the same for all styles of the same FontInfo.
 */
#define EGG_NUM_STYLE_PROPERTIES 5


/* Used to determine whether we are using point or pixel sizes. */
typedef enum
{
  EGG_XFONT_METRIC_PIXELS,
  EGG_XFONT_METRIC_POINTS
} EggXFontMetricType;

/* Used for determining the type of a font style, and also for setting filters.
 *  These can be combined if a style has bitmaps and scalable fonts available.
 */
typedef enum
{
  EGG_XFONT_BITMAP		= 1 << 0,
  EGG_XFONT_SCALABLE		= 1 << 1,
  EGG_XFONT_SCALABLE_BITMAP	= 1 << 2,
  EGG_XFONT_ALL			= 0x07
} EggXFontType;

/* These are the two types of filter available - base and user. The base
 * filter is set by the application and can't be changed by the user.
 */
#define	EGG_NUM_FONT_FILTERS	2
typedef enum
{
  EGG_XFONT_FILTER_BASE,
  EGG_XFONT_FILTER_USER
} EggXFontFilterType;

/* These hold the arrays of current filter settings for each property.
 * If nfilters is 0 then all values of the property are OK. If not the
 * filters array contains the indexes of the valid property values.
 */
typedef struct _EggXFontFilter	EggXFontFilter;
struct _EggXFontFilter
{
  gint font_type;
  guint16 *property_filters[EGG_NUM_FONT_PROPERTIES];
  guint16 property_nfilters[EGG_NUM_FONT_PROPERTIES];
};

typedef struct _EggXFontSelector       EggXFontSelector;
typedef struct _EggXFontSelectorClass  EggXFontSelectorClass;

struct _EggXFontSelector
{
  GtkVBox vbox;

  GtkWidget *family_label;
  GtkWidget *family_options;
  GtkWidget *size_options;
  GtkWidget *bold_check;

  gint font_index;

  /* Map from the position of font in the family menu to its font_index */
  gint *filtered_font_index;

  /* Map from position of size in option menu to actual size */
  gint *size_options_map;

  EggXFontMetricType metric;

  /* The size is either in pixels or deci-points, depending on the metric. */
  gint size;
  
  /* These are the current property settings. They are indexes into the
   * strings in the EggXFontSelInfo properties array.
   */

  guint16 property_values[EGG_NUM_STYLE_PROPERTIES];
  
  /* These are the base and user font filters. */
  EggXFontFilter filters[EGG_NUM_FONT_FILTERS];

  guint want_bold : 1;
  guint can_bold : 1;
};

struct _EggXFontSelectorClass
{
  GtkBinClass parent_class;

  void (* changed)         (EggXFontSelector *selector);
};


GType    egg_xfont_selector_get_type         (void) G_GNUC_CONST;
GtkWidget* egg_xfont_selector_new              (const gchar *label);

/* Setting the selected font also clears the filter. */

gboolean egg_xfont_selector_set_font_name (EggXFontSelector *selector,
						const gchar *xlfd);

/* The result must be freed */
gchar* egg_xfont_selector_get_font_name (EggXFontSelector *selector);

/* Restricts what can be selected by the user and the values returned by
 * egg_xfont_selector_get_font_name.
 */

void egg_xfont_selector_set_filter	(EggXFontSelector *fsd,
					 EggXFontFilterType filter_type,
					 EggXFontType	   font_type,
					 gchar		 **foundries,
					 gchar		 **weights,
					 gchar		 **slants,
					 gchar		 **setwidths,
					 gchar		 **spacings,
					 gchar		 **charsets);

void egg_xfont_selector_clear_filter (EggXFontSelector *selector);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EGG_XFONT_SELECTOR_H__ */

