/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-pango-extensions.h - interface for new functions that conceptually
                            belong in pango. Perhaps some of these will be
                            actually rolled into pango someday.

   Copyright (C) 2001 Anders Carlsson

   The Eel Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Eel Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Eel Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#ifndef EEL_PANGO_EXTENSIONS_H
#define EEL_PANGO_EXTENSIONS_H

#include <pango/pango-layout.h>

typedef enum {
	EEL_ELLIPSIZE_START,
	EEL_ELLIPSIZE_MIDDLE,
	EEL_ELLIPSIZE_END
} EelEllipsizeMode;

PangoAttrList *eel_pango_attr_list_copy_or_create         (PangoAttrList    *attr_list);
PangoAttrList *eel_pango_attr_list_apply_global_attribute (PangoAttrList    *attr_list,
							   PangoAttribute   *attr);
void           eel_pango_layout_set_weight                (PangoLayout      *layout,
							   PangoWeight       weight);
void           eel_pango_layout_set_underline             (PangoLayout      *layout,
							   PangoUnderline    underline);
void           eel_pango_layout_set_font_desc_from_string (PangoLayout      *layout,
							   const char       *str);
int eel_pango_font_description_get_largest_fitting_font_size (const PangoFontDescription *font_desc,
							      PangoContext               *context,
							      const char                 *text,
							      int                         available_width,
							      int                         minimum_acceptable_font_size,
							      int                         maximum_acceptable_font_size);

PangoRectangle eel_pango_layout_fit_to_dimensions         (PangoLayout      *layout,
							   int               max_width,
							   int               max_height);
/* caution: this function is expensive. */
void           eel_pango_layout_set_text_ellipsized       (PangoLayout      *layout,
							   const char       *string,
							   int               width,
							   EelEllipsizeMode  mode);
PangoContext * eel_pango_ft2_get_context                  (void);




#endif /* EEL_PANGO_EXTENSIONS_H */
