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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GNOME_FONTSEL_H__
#define __GNOME_FONTSEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtknotebook.h>

G_BEGIN_DECLS

#define GNOME_TYPE_FONT_SELECTION            (gnome_font_selection_get_type ())
#define GNOME_FONT_SELECTION(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_FONT_SELECTION, GnomeFontSelection))
#define GNOME_FONT_SELECTION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_FONT_SELECTION, GnomeFontSelectionClass))
#define GNOME_IS_FONT_SELECTION(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_FONT_SELECTION))
#define GNOME_IS_FONT_SELECTION_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_FONT_SELECTION))
#define GNOME_FONT_SELECTION_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GNOME_TYPE_FONT_SELECTION, GnomeFontSelectionClass))

#define GNOME_TYPE_FONT_SELECTOR            (gnome_font_selector_get_type ())
#define GNOME_FONT_SELECTOR(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_FONT_SELECTOR, GnomeFontSelector))
#define GNOME_FONT_SELECTOR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_FONT_SELECTOR, GnomeFontSelectorClass))
#define GNOME_IS_FONT_SELECTOR(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_FONT_SELECTOR))
#define GNOME_IS_FONT_SELECTOR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_FONT_SELECTOR))
#define GNOME_FONT_SELECTOR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GNOME_TYPE_FONT_SELECTOR, GnomeFontSelectorClass))

typedef struct _GnomeFontSelection	     GnomeFontSelection;
typedef struct _GnomeFontSelectionClass	     GnomeFontSelectionClass;

typedef struct _GnomeFontSelector      GnomeFontSelector;
typedef struct _GnomeFontSelectorClass GnomeFontSelectorClass;

/* This is the number of properties which we keep in the properties array,
   i.e. Weight, Slant, Set Width, Spacing, Charset & Foundry. */
#define GNOME_NUM_FONT_PROPERTIES  6

/* This is the number of properties each style has i.e. Weight, Slant,
   Set Width, Spacing & Charset. Note that Foundry is not included,
   since it is the same for all styles of the same FontInfo. */
#define GNOME_NUM_STYLE_PROPERTIES 5


/* Used to determine whether we are using point or pixel sizes. */
typedef enum
{
  GNOME_FONT_METRIC_PIXELS,
  GNOME_FONT_METRIC_POINTS
} GnomeFontMetricType;

/* Used for determining the type of a font style, and also for setting filters.
   These can be combined if a style has bitmaps and scalable fonts available.*/
typedef enum
{
  GNOME_FONT_BITMAP		= 1 << 0,
  GNOME_FONT_SCALABLE		= 1 << 1,
  GNOME_FONT_SCALABLE_BITMAP	= 1 << 2,

  GNOME_FONT_ALL			= 0x07
} GnomeFontType;

/* These are the two types of filter available - base and user. The base
   filter is set by the application and can't be changed by the user. */
#define	GNOME_NUM_FONT_FILTERS	2
typedef enum
{
  GNOME_FONT_FILTER_BASE,
  GNOME_FONT_FILTER_USER
} GnomeFontFilterType;

/* These hold the arrays of current filter settings for each property.
   If nfilters is 0 then all values of the property are OK. If not the
   filters array contains the indexes of the valid property values. */
typedef struct _GnomeFontFilter	GnomeFontFilter;
struct _GnomeFontFilter
{
  gint font_type;
  guint16 *property_filters[GNOME_NUM_FONT_PROPERTIES];
  guint16 property_nfilters[GNOME_NUM_FONT_PROPERTIES];
};


struct _GnomeFontSelection
{
  GtkNotebook notebook;
  
  /* These are on the font page. */
  GtkWidget *main_vbox;
  GtkWidget *font_label;
  GtkWidget *font_entry;
  GtkWidget *font_clist;
  GtkWidget *font_style_entry;
  GtkWidget *font_style_clist;
  GtkWidget *size_entry;
  GtkWidget *size_clist;
  GtkWidget *pixels_button;
  GtkWidget *points_button;
  GtkWidget *filter_button;
  GtkWidget *preview_entry;
  GtkWidget *message_label;
  
  /* These are on the font info page. */
  GtkWidget *info_vbox;
  GtkWidget *info_clist;
  GtkWidget *requested_font_name;
  GtkWidget *actual_font_name;
  
  /* These are on the filter page. */
  GtkWidget *filter_vbox;
  GtkWidget *type_bitmaps_button;
  GtkWidget *type_scalable_button;
  GtkWidget *type_scaled_bitmaps_button;
  GtkWidget *filter_clists[GNOME_NUM_FONT_PROPERTIES];
  
  GdkFont *font;
  gint font_index;
  gint style;
  GnomeFontMetricType metric;
  /* The size is either in pixels or deci-points, depending on the metric. */
  gint size;
  
  /* This is the last size explicitly selected. When the user selects different
     fonts we try to find the nearest size to this. */
  gint selected_size;
  
  /* These are the current property settings. They are indexes into the
     strings in the GnomeFontSelInfo properties array. */
  guint16 property_values[GNOME_NUM_STYLE_PROPERTIES];
  
  /* These are the base and user font filters. */
  GnomeFontFilter filters[GNOME_NUM_FONT_FILTERS];
};

struct _GnomeFontSelectionClass
{
  GtkNotebookClass parent_class;
};


struct _GnomeFontSelector
{
  GtkDialog  parent;
  
  GtkWidget *fontsel;
  
  GtkWidget *main_vbox;
  GtkWidget *action_area;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;
  
  /* If the user changes the width of the dialog, we turn auto-shrink off. */
  gint dialog_width;
  gboolean auto_resize;
};

struct _GnomeFontSelectorClass
{
  GtkDialogClass parent_class;
};


/*****************************************************************************
 * GnomeFontSelection functions.
 *   see the comments in the GnomeFontSelection functions.
 *****************************************************************************/

GtkType	   gnome_font_selection_get_type        (void);
GtkWidget* gnome_font_selection_new		(void);
gchar*	   gnome_font_selection_get_font_name	(GnomeFontSelection *fontsel);
GdkFont*   gnome_font_selection_get_font		(GnomeFontSelection *fontsel);
gboolean   gnome_font_selection_set_font_name	(GnomeFontSelection *fontsel,
						 const gchar	  *fontname);
void	   gnome_font_selection_set_filter	(GnomeFontSelection *fontsel,
						 GnomeFontFilterType filter_type,
						 GnomeFontType	   font_type,
						 gchar		 **foundries,
						 gchar		 **weights,
						 gchar		 **slants,
						 gchar		 **setwidths,
						 gchar		 **spacings,
						 gchar		 **charsets);
const gchar* gnome_font_selection_get_preview_text	(GnomeFontSelection *fontsel);
void	   gnome_font_selection_set_preview_text	(GnomeFontSelection *fontsel,
							 const gchar	  *text);



/*****************************************************************************
 * GnomeFontSelector functions.
 *   most of these functions simply call the corresponding function in the
 *   GnomeFontSelector.
 *****************************************************************************/

/* The only API we really need for compat */
GtkType	     gnome_font_selector_get_type     (void);
GtkWidget   *gnome_font_selector_new	      (const gchar       *title);
gchar       *gnome_font_selector_get_selected (GnomeFontSelector *text_tool);
/* Basically runs a modal-dialog version of this, and returns
   the string that id's the selected font. */
gchar       *gnome_font_select                (void);
gchar       *gnome_font_select_with_default   (const gchar *);



/* This returns the X Logical Font Description fontname, or NULL if no font
   is selected. Note that there is a slight possibility that the font might not
   have been loaded OK. You should call gnome_font_selector_get_font()
   to see if it has been loaded OK.
   You should g_free() the returned font name after you're done with it. */
gchar*	 gnome_font_selector_get_font_name    (GnomeFontSelector *fsd);

/* This will return the current GdkFont, or NULL if none is selected or there
   was a problem loading it. Remember to use gdk_font_ref/unref() if you want
   to use the font (in a style, for example). */
GdkFont* gnome_font_selector_get_font	    (GnomeFontSelector *fsd);

/* This sets the currently displayed font. It should be a valid X Logical
   Font Description font name (anything else will be ignored), e.g.
   "-adobe-courier-bold-o-normal--25-*-*-*-*-*-*-*" 
   It returns TRUE on success. */
gboolean gnome_font_selector_set_font_name    (GnomeFontSelector *fsd,
						       const gchar	*fontname);

/* This sets one of the font filters, to limit the fonts shown. The filter_type
   is GNOME_FONT_FILTER_BASE or GNOME_FONT_FILTER_USER. The font type is a
   combination of the bit flags GNOME_FONT_BITMAP, GNOME_FONT_SCALABLE and
   GNOME_FONT_SCALABLE_BITMAP (or GNOME_FONT_ALL for all font types).
   The foundries, weights etc. are arrays of strings containing property
   values, e.g. 'bold', 'demibold', and *MUST* finish with a NULL.
   Standard long names are also accepted, e.g. 'italic' instead of 'i'.

   e.g. to allow only fixed-width fonts ('char cell' or 'monospaced') to be
   selected use:

  gchar *spacings[] = { "c", "m", NULL };
  gnome_font_selector_set_filter (GNOME_FONT_SELECTOR (fontsel),
				       GNOME_FONT_FILTER_BASE, GNOME_FONT_ALL,
				       NULL, NULL, NULL, NULL, spacings, NULL);

  to allow only true scalable fonts to be selected use:

  gnome_font_selector_set_filter (GNOME_FONT_SELECTOR (fontsel),
				       GNOME_FONT_FILTER_BASE, GNOME_FONT_SCALABLE,
				       NULL, NULL, NULL, NULL, NULL, NULL);
*/
void	   gnome_font_selector_set_filter	(GnomeFontSelector *fsd,
						 GnomeFontFilterType filter_type,
						 GnomeFontType	   font_type,
						 gchar		 **foundries,
						 gchar		 **weights,
						 gchar		 **slants,
						 gchar		 **setwidths,
						 gchar		 **spacings,
						 gchar		 **charsets);

/* This returns the text in the preview entry. You should copy the returned
   text if you need it. */
const gchar* gnome_font_selector_get_preview_text (GnomeFontSelector *fsd);

/* This sets the text in the preview entry. It will be copied by the entry,
   so there's no need to g_strdup() it first. */
void	 gnome_font_selector_set_preview_text (GnomeFontSelector *fsd,
					       const gchar	    *text);

G_END_DECLS

#endif /* __GNOME_FONTSEL_H__ */
