/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-extensions.h - interface of libart extension functions.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_ART_EXTENSIONS_H
#define EEL_ART_EXTENSIONS_H

#include <glib.h>
#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_point.h>

G_BEGIN_DECLS

typedef struct {
	int x;
	int y;
} EelArtIPoint;

typedef struct {
	int width;
	int height;
} EelDimensions;

extern const ArtDRect eel_art_drect_empty;
extern const ArtIRect eel_art_irect_empty;
extern const ArtPoint eel_art_point_zero;
extern const ArtPoint eel_art_point_min;
extern const ArtPoint eel_art_point_max;
extern const EelArtIPoint eel_art_ipoint_max;
extern const EelArtIPoint eel_art_ipoint_min;
extern const EelArtIPoint eel_art_ipoint_zero;
extern const EelDimensions eel_dimensions_empty;

/* More functions for ArtIRect and ArtDRect. */
gboolean      eel_art_irect_equal             (ArtIRect      rectangle_a,
					       ArtIRect      rectangle_b);
gboolean      eel_art_drect_equal             (ArtDRect      rectangle_a,
					       ArtDRect      rectangle_b);
gboolean      eel_art_irect_hits_irect        (ArtIRect      rectangle_a,
					       ArtIRect      rectangle_b);
gboolean      eel_art_irect_contains_irect    (ArtIRect      outer_rectangle,
					       ArtIRect      inner_rectangle);
gboolean      eel_art_irect_contains_point    (ArtIRect      outer_rectangle,
					       int           x,
					       int           y);
ArtIRect      eel_art_irect_assign            (int           x,
					       int           y,
					       int           width,
					       int           height);
int           eel_art_irect_get_width         (ArtIRect      rectangle);
int           eel_art_irect_get_height        (ArtIRect      rectangle);
double        eel_art_drect_get_width         (ArtDRect      rectangle);
double        eel_art_drect_get_height        (ArtDRect      rectangle);
ArtIRect      eel_art_irect_align             (ArtIRect      container,
					       int           aligned_width,
					       int           aligned_height,
					       float         x_alignment,
					       float         y_alignment);
ArtIRect      eel_art_irect_assign_dimensions (int           x,
					       int           y,
					       EelDimensions dimensions);
ArtIRect      eel_art_irect_assign_end_points (EelArtIPoint  top_left_point,
					       EelArtIPoint  bottom_right_point);
ArtIRect      eel_art_irect_offset_by         (ArtIRect      rectangle,
					       int           x,
					       int           y);
ArtIRect      eel_art_irect_offset_to         (ArtIRect      rectangle,
					       int           x,
					       int           y);
ArtIRect      eel_art_irect_scale_by          (ArtIRect      rectangle,
					       double        scale);
ArtIRect      eel_art_irect_inset             (ArtIRect      rectangle,
					       int           horizontal_inset,
					       int           vertical_inset);
ArtDRect      eel_art_drect_offset_by         (ArtDRect      rectangle,
					       double        x,
					       double        y);
ArtDRect      eel_art_drect_offset_to         (ArtDRect      rectangle,
					       double        x,
					       double        y);
ArtDRect      eel_art_drect_scale_by          (ArtDRect      rectangle,
					       double        scale);
ArtDRect      eel_art_drect_inset             (ArtDRect      rectangle,
					       double        horizontal_inset,
					       double        vertical_inset);
ArtDRect      eel_art_drect_assign_end_points (ArtPoint      top_left_point,
					       ArtPoint      bottom_right_point);
ArtIRect      eel_art_irect_offset_by_point   (ArtIRect      rectangle,
					       EelArtIPoint  point);
ArtIRect      eel_art_irect_offset_to_point   (ArtIRect      rectangle,
					       EelArtIPoint  point);
ArtIRect      eel_art_irect_intersect         (ArtIRect      rectangle_a,
					       ArtIRect      rectangle_b);
ArtIRect      eel_art_irect_union             (ArtIRect      rectangle_a,
					       ArtIRect      rectangle_b);
gboolean      eel_art_irect_is_empty          (ArtIRect      rectangle);

/* EelDimensions functions. */
gboolean      eel_dimensions_are_empty        (EelDimensions dimensions);
EelDimensions eel_dimensions_assign           (int           width,
					       int           height);
gboolean      eel_dimensions_equal            (EelDimensions dimensions_a,
					       EelDimensions dimensions_b);
EelDimensions eel_dimensions_clamp            (EelDimensions dimensions,
					       EelDimensions min,
					       EelDimensions max);

/* EelArtIPoint functions. */
EelArtIPoint  eel_art_ipoint_assign           (int           x,
					       int           y);
gboolean      eel_art_ipoint_equal            (EelArtIPoint  point_a,
					       EelArtIPoint  point_b);
EelArtIPoint  eel_art_ipoint_clamp            (EelArtIPoint  point,
					       EelArtIPoint  min,
					       EelArtIPoint  max);
EelArtIPoint  eel_art_ipoint_offset_by        (EelArtIPoint  point,
					       int           x,
					       int           y);

/* ArtPoint functions. */
gboolean      eel_art_point_equal             (ArtPoint      point_a,
					       ArtPoint      point_b);
ArtPoint      eel_art_point_assign            (double        x,
					       double        y);
ArtPoint      eel_art_point_clamp             (ArtPoint      point,
					       ArtPoint      min,
					       ArtPoint      max);
ArtPoint      eel_art_point_offset_by         (ArtPoint      point,
					       double        x,
					       double        y);

G_END_DECLS

#endif /* EEL_ART_EXTENSIONS_H */
