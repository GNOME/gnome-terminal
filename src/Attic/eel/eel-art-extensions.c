/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-extensions.c - implementation of libart extension functions.

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

#include <config.h>

#include "eel-art-extensions.h"
#include "eel-lib-self-check-functions.h"
#include <math.h>

const ArtDRect eel_art_drect_empty = { 0.0, 0.0, 0.0, 0.0 };
const ArtIRect eel_art_irect_empty = { 0, 0, 0, 0 };
const ArtPoint eel_art_point_zero = { 0, 0 };
const ArtPoint eel_art_point_max = { G_MAXDOUBLE, G_MAXDOUBLE };
const ArtPoint eel_art_point_min = { G_MINDOUBLE, G_MINDOUBLE };
const EelArtIPoint eel_art_ipoint_max = { G_MAXINT, G_MAXINT };
const EelArtIPoint eel_art_ipoint_min = { G_MININT, G_MININT };
const EelArtIPoint eel_art_ipoint_zero = { 0, 0 };
const EelDimensions eel_dimensions_empty = { 0, 0 };

gboolean
eel_art_irect_contains_irect (ArtIRect outer_rectangle,
			      ArtIRect inner_rectangle)
{
	return outer_rectangle.x0 <= inner_rectangle.x0
		&& outer_rectangle.y0 <= inner_rectangle.y0
		&& outer_rectangle.x1 >= inner_rectangle.x1
		&& outer_rectangle.y1 >= inner_rectangle.y1; 
}

/**
 * eel_art_irect_contains_point:
 * 
 * @rectangle: An ArtIRect.
 * @x: X coordinate to test.
 * @y: Y coordinate to test.
 *
 * Returns: A boolean value indicating whether the rectangle 
 *          contains the x,y coordinate.
 * 
 */
gboolean
eel_art_irect_contains_point (ArtIRect rectangle,
			      int x,
			      int y)
{
	return x >= rectangle.x0
		&& x <= rectangle.x1
		&& y >= rectangle.y0
		&& y <= rectangle.y1;
}

gboolean
eel_art_irect_hits_irect (ArtIRect rectangle_a,
			  ArtIRect rectangle_b)
{
	ArtIRect intersection;
	art_irect_intersect (&intersection, &rectangle_a, &rectangle_b);
	return !art_irect_empty (&intersection);
}

gboolean
eel_art_irect_equal (ArtIRect rectangle_a,
		     ArtIRect rectangle_b)
{
	return rectangle_a.x0 == rectangle_b.x0
		&& rectangle_a.y0 == rectangle_b.y0
		&& rectangle_a.x1 == rectangle_b.x1
		&& rectangle_a.y1 == rectangle_b.y1;
}

gboolean
eel_art_drect_equal (ArtDRect rectangle_a,
		     ArtDRect rectangle_b)
{
	return rectangle_a.x0 == rectangle_b.x0
		&& rectangle_a.y0 == rectangle_b.y0
		&& rectangle_a.x1 == rectangle_b.x1
		&& rectangle_a.y1 == rectangle_b.y1;
}

ArtIRect
eel_art_irect_assign (int x,
		      int y,
		      int width,
		      int height)
{
	ArtIRect rectangle;

	rectangle.x0 = x;
	rectangle.y0 = y;
	rectangle.x1 = rectangle.x0 + width;
	rectangle.y1 = rectangle.y0 + height;

	return rectangle;
}

/**
 * eel_art_irect_get_width:
 * 
 * @rectangle: An ArtIRect.
 *
 * Returns: The width of the rectangle.
 * 
 */
int
eel_art_irect_get_width (ArtIRect rectangle)
{
	return rectangle.x1 - rectangle.x0;
}

/**
 * eel_art_irect_get_height:
 * 
 * @rectangle: An ArtIRect.
 *
 * Returns: The height of the rectangle.
 * 
 */
int
eel_art_irect_get_height (ArtIRect rectangle)
{
	return rectangle.y1 - rectangle.y0;
}


/**
 * eel_art_drect_get_width:
 * 
 * @rectangle: An ArtDrect.
 *
 * Returns: The width of the rectangle.
 * 
 */
double
eel_art_drect_get_width (ArtDRect rectangle)
{
	return rectangle.x1 - rectangle.x0;
}

/**
 * eel_art_drect_get_height:
 * 
 * @rectangle: An ArtDRect.
 *
 * Returns: The height of the rectangle.
 * 
 */
double
eel_art_drect_get_height (ArtDRect rectangle)
{
	return rectangle.y1 - rectangle.y0;
}

/**
 * eel_art_irect_align:
 * 
 * @container: The rectangle that is to contain the aligned rectangle.
 * @aligned_width: Width of rectangle being algined.
 * @aligned_height: Height of rectangle being algined.
 * @x_alignment: X alignment.
 * @y_alignment: Y alignment.
 *
 * Returns: A rectangle aligned within a container rectangle
 *          using the given alignment parameters.
 */
ArtIRect
eel_art_irect_align (ArtIRect container,
		     int aligned_width,
		     int aligned_height,
		     float x_alignment,
		     float y_alignment)
{
	ArtIRect aligned;
	int available_width;
	int available_height;

	if (art_irect_empty (&container)) {
		return eel_art_irect_empty;
	}

	if (aligned_width == 0 || aligned_height == 0) {
		return eel_art_irect_empty;
	}

	/* Make sure the aligment parameters are within range */
	x_alignment = MAX (0, x_alignment);
	x_alignment = MIN (1.0, x_alignment);
	y_alignment = MAX (0, y_alignment);
	y_alignment = MIN (1.0, y_alignment);

	available_width = eel_art_irect_get_width (container) - aligned_width;
	available_height = eel_art_irect_get_height (container) - aligned_height;

	aligned.x0 = floor (container.x0 + (available_width * x_alignment) + 0.5);
	aligned.y0 = floor (container.y0 + (available_height * y_alignment) + 0.5);
	aligned.x1 = aligned.x0 + aligned_width;
	aligned.y1 = aligned.y0 + aligned_height;

	return aligned;
}

gboolean
eel_art_irect_is_empty (ArtIRect rectangle)
{
	return art_irect_empty (&rectangle);
}

/**
 * eel_dimensions_are_empty:
 * 
 * @dimensions: A EelDimensions structure.
 *
 * Returns: Whether the dimensions are empty.
 */
gboolean
eel_dimensions_are_empty (EelDimensions dimensions)
{
	return dimensions.width <= 0 || dimensions.height <= 0;
}

/**
 * eel_art_irect_assign_dimensions:
 * 
 * @x: X coodinate for resulting rectangle.
 * @y: Y coodinate for resulting rectangle.
 * @dimensions: A EelDimensions structure for the rect's width and height.
 *
 * Returns: An ArtIRect with the given coordinates and dimensions.
 */
ArtIRect
eel_art_irect_assign_dimensions (int x,
				 int y,
				 EelDimensions dimensions)
{
	ArtIRect rectangle;

	rectangle.x0 = x;
	rectangle.y0 = y;
	rectangle.x1 = rectangle.x0 + dimensions.width;
	rectangle.y1 = rectangle.y0 + dimensions.height;

	return rectangle;
}

/**
 * eel_art_irect_assign_end_points:
 * 
 * @top_left_point: Top left point.
 * @bottom_right_point: Bottom right point.
 *
 * Returns: An ArtIRect that bounds the given end points.
 */
ArtIRect
eel_art_irect_assign_end_points (EelArtIPoint top_left_point,
				 EelArtIPoint bottom_right_point)
{
	ArtIRect rectangle;

	rectangle.x0 = top_left_point.x;
	rectangle.y0 = top_left_point.y;
	rectangle.x1 = bottom_right_point.x;
	rectangle.y1 = bottom_right_point.y;

	return rectangle;
}

ArtIRect 
eel_art_irect_offset_by (ArtIRect rectangle, int x, int y)
{
	rectangle.x0 += x;
	rectangle.x1 += x;
	rectangle.y0 += y;
	rectangle.y1 += y;
	
	return rectangle;
}

ArtIRect 
eel_art_irect_offset_to (ArtIRect rectangle, int x, int y)
{
	rectangle.x1 = rectangle.x1 - rectangle.x0 + x;
	rectangle.x0 = x;
	rectangle.y1 = rectangle.y1 - rectangle.y0 + y;
	rectangle.y0 = y;
	
	return rectangle;
}

ArtIRect 
eel_art_irect_scale_by (ArtIRect rectangle, double scale)
{
	rectangle.x0 *= scale;
	rectangle.x1 *= scale;
	rectangle.y0 *= scale;
	rectangle.y1 *= scale;
	
	return rectangle;
}

ArtIRect 
eel_art_irect_inset (ArtIRect rectangle, int horizontal_inset, int vertical_inset)
{
	rectangle.x0 += horizontal_inset;
	rectangle.x1 -= horizontal_inset;
	rectangle.y0 += vertical_inset;
	rectangle.y1 -= vertical_inset;
	
	return rectangle;
}


ArtDRect 
eel_art_drect_offset_by (ArtDRect rectangle, double x, double y)
{
	rectangle.x0 += x;
	rectangle.x1 += x;
	rectangle.y0 += y;
	rectangle.y1 += y;
	
	return rectangle;
}

ArtDRect 
eel_art_drect_offset_to (ArtDRect rectangle, double x, double y)
{
	rectangle.x1 = rectangle.x1 - rectangle.x0 + x;
	rectangle.x0 = x;
	rectangle.y1 = rectangle.y1 - rectangle.y0 + y;
	rectangle.y0 = y;
	
	return rectangle;
}

ArtIRect 
eel_art_irect_offset_by_point (ArtIRect rectangle, EelArtIPoint point)
{
	rectangle.x0 += point.x;
	rectangle.x1 += point.x;
	rectangle.y0 += point.y;
	rectangle.y1 += point.y;
	
	return rectangle;
}

ArtIRect 
eel_art_irect_offset_to_point (ArtIRect rectangle, EelArtIPoint point)
{
	rectangle.x1 = rectangle.x1 - rectangle.x0 + point.x;
	rectangle.x0 = point.x;
	rectangle.y1 = rectangle.y1 - rectangle.y0 + point.y;
	rectangle.y0 = point.y;
	
	return rectangle;
}

ArtDRect 
eel_art_drect_scale_by (ArtDRect rectangle, double scale)
{
	rectangle.x0 *= scale;
	rectangle.x1 *= scale;
	rectangle.y0 *= scale;
	rectangle.y1 *= scale;
	
	return rectangle;
}

ArtDRect 
eel_art_drect_inset (ArtDRect rectangle, double horizontal_inset, double vertical_inset)
{
	rectangle.x0 += horizontal_inset;
	rectangle.x1 -= horizontal_inset;
	rectangle.y0 += vertical_inset;
	rectangle.y1 -= vertical_inset;
	
	return rectangle;
}

ArtDRect
eel_art_drect_assign_end_points (ArtPoint top_left_point,
				 ArtPoint bottom_right_point)
{
	ArtDRect rectangle;

	rectangle.x0 = top_left_point.x;
	rectangle.y0 = top_left_point.y;
	rectangle.x1 = bottom_right_point.x;
	rectangle.y1 = bottom_right_point.y;

	return rectangle;
}

/**
 * eel_art_irect_intersect:
 * 
 * @rectangle_a: A rectangle.
 * @rectangle_b: Another rectangle.
 *
 * Returns: The intersection of the 2 rectangles.
 *
 * This function has 2 advantages over plain art_irect_intersect():
 *
 * 1) Rectangles are passed in and returned by value
 * 2) The empty result can always be counted on being exactly eel_art_irect_empty.
 */
ArtIRect
eel_art_irect_intersect (ArtIRect rectangle_a,
			 ArtIRect rectangle_b)
{
	ArtIRect ab_intersection;

	art_irect_intersect (&ab_intersection, &rectangle_a, &rectangle_b);

	if (art_irect_empty (&ab_intersection)) {
		return eel_art_irect_empty;
	} else {
		return ab_intersection;
	}
}

/**
 * eel_art_irect_union:
 * 
 * @rectangle_a: A rectangle.
 * @rectangle_b: Another rectangle.
 *
 * Returns: The union of the 2 rectangles.
 *
 * This function has 2 advantages over plain art_irect_union():
 *
 * 1) Rectangles are passed in and returned by value
 * 2) The empty result can always be counted on being exactly eel_art_irect_empty.
 */
ArtIRect
eel_art_irect_union (ArtIRect rectangle_a,
			 ArtIRect rectangle_b)
{
	ArtIRect ab_union;

	art_irect_union (&ab_union, &rectangle_a, &rectangle_b);

	if (art_irect_empty (&ab_union)) {
		return eel_art_irect_empty;
	} else {
		return ab_union;
	}
}

EelDimensions
eel_dimensions_assign (int width,
		       int height)
{
	EelDimensions dimensions;

	dimensions.width = width;
	dimensions.height = height;

	return dimensions;
}

gboolean
eel_dimensions_equal (EelDimensions dimensions_a,
		      EelDimensions dimensions_b)
{
	return dimensions_a.width == dimensions_b.width
		&& dimensions_a.height == dimensions_b.height;
}

EelDimensions
eel_dimensions_clamp (EelDimensions dimensions,
		      EelDimensions min,
		      EelDimensions max)
{
	EelDimensions clamped;

	clamped.width = CLAMP (dimensions.width, min.width, max.width);
	clamped.height = CLAMP (dimensions.height, min.height, max.height);

	return clamped;
}

EelArtIPoint
eel_art_ipoint_assign (int x,
		       int y)
{
	EelArtIPoint point;

	point.x = x;
	point.y = y;

	return point;
}

gboolean
eel_art_ipoint_equal (EelArtIPoint point_a,
		      EelArtIPoint point_b)
{
	return point_a.x == point_b.x
		&& point_a.y == point_b.y;
}

EelArtIPoint
eel_art_ipoint_clamp (EelArtIPoint point,
		      EelArtIPoint min,
		      EelArtIPoint max)
{
	return eel_art_ipoint_assign (CLAMP (point.x, min.x, max.x),
				      CLAMP (point.y, min.y, max.y));
}

EelArtIPoint
eel_art_ipoint_offset_by (EelArtIPoint point,
			  int x,
			  int y)
{
	return eel_art_ipoint_assign (point.x + x, point.y + y);
}

gboolean
eel_art_point_equal (ArtPoint point_a,
		     ArtPoint point_b)
{
	return point_a.x == point_b.x
		&& point_a.y == point_b.y;
}

ArtPoint
eel_art_point_assign (double x,
		      double y)
{
	ArtPoint point;

	point.x = x;
	point.y = y;

	return point;
}

ArtPoint
eel_art_point_clamp (ArtPoint point,
		     ArtPoint min,
		     ArtPoint max)
{
	return eel_art_point_assign (CLAMP (point.x, min.x, max.x),
				     CLAMP (point.y, min.y, max.y));
}

ArtPoint
eel_art_point_offset_by (ArtPoint point,
			 double x,
			 double y)
{
	return eel_art_point_assign (point.x + x, point.y + y);
}

#if !defined (EEL_OMIT_SELF_CHECK)

static ArtIRect
test_irect_intersect (int a_x0, int a_y0, int a_x1, int a_y1,
		      int b_x0, int b_y0, int b_x1, int b_y1)
{
	ArtIRect a;
	ArtIRect b;

	a.x0 = a_x0;
	a.y0 = a_y0;
	a.x1 = a_x1;
	a.y1 = a_y1;

	b.x0 = b_x0;
	b.y0 = b_y0;
	b.x1 = b_x1;
	b.y1 = b_y1;

	return eel_art_irect_intersect (a, b);
}

static ArtIRect
test_irect_union (int a_x0, int a_y0, int a_x1, int a_y1,
		  int b_x0, int b_y0, int b_x1, int b_y1)
{
	ArtIRect a;
	ArtIRect b;

	a.x0 = a_x0;
	a.y0 = a_y0;
	a.x1 = a_x1;
	a.y1 = a_y1;

	b.x0 = b_x0;
	b.y0 = b_y0;
	b.x1 = b_x1;
	b.y1 = b_y1;

	return eel_art_irect_union (a, b);
}

static EelArtIPoint
test_ipoint_clamp (int x, int y, int min_x, int min_y, int max_x, int max_y)
{
	return eel_art_ipoint_clamp (eel_art_ipoint_assign (x, y),
				     eel_art_ipoint_assign (min_x, min_y),
				     eel_art_ipoint_assign (max_x, max_y));
}

static EelDimensions
test_dimensions_clamp (int width, int height, int min_width, int min_height, int max_width, int max_height)
{
	return eel_dimensions_clamp (eel_dimensions_assign (width, height),
				     eel_dimensions_assign (min_width, min_height),
				     eel_dimensions_assign (max_width, max_height));
}

void
eel_self_check_art_extensions (void)
{
	ArtIRect one;
	ArtIRect two;
	ArtIRect empty_rect = eel_art_irect_empty;
	ArtIRect inside;
	ArtIRect outside;
	ArtIRect container;
	EelDimensions empty_dimensions = eel_dimensions_empty;
	EelDimensions dim1;

	one = eel_art_irect_assign (10, 10, 20, 20);
	two = eel_art_irect_assign (10, 10, 20, 20);
	inside = eel_art_irect_assign (11, 11, 18, 18);
	outside = eel_art_irect_assign (31, 31, 10, 10);
	container = eel_art_irect_assign (0, 0, 100, 100);

	/* eel_art_irect_equal */
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_equal (one, two), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_equal (one, empty_rect), FALSE);

	/* eel_art_irect_hits_irect */
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_hits_irect (one, two), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_hits_irect (one, inside), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_hits_irect (one, outside), FALSE);

	/* eel_art_irect_contains_point */
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 9, 9), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 9, 10), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 10, 9), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 10, 10), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 11, 10), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 10, 11), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 11, 11), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 30, 30), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 29, 30), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 30, 29), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_art_irect_contains_point (one, 31, 31), FALSE);

	/* eel_art_irect_get_width */
	EEL_CHECK_INTEGER_RESULT (eel_art_irect_get_width (one), 20);
	EEL_CHECK_INTEGER_RESULT (eel_art_irect_get_width (empty_rect), 0);

	/* eel_art_irect_get_height */
	EEL_CHECK_INTEGER_RESULT (eel_art_irect_get_height (one), 20);
	EEL_CHECK_INTEGER_RESULT (eel_art_irect_get_height (empty_rect), 0);

	/* eel_art_irect_align */
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (empty_rect, 1, 1, 0.0, 0.0), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 0, 0, 0.0, 0.0), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 0, 0.0, 0.0), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 0, 9, 0.0, 0.0), 0, 0, 0, 0);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 0.0, 0.0), 0, 0, 10, 10);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 1.0, 0.0), 90, 0, 100, 10);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 0.0, 1.0), 0, 90, 10, 100);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 1.0, 1.0), 90, 90, 100, 100);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 0.0, 0.0), 0, 0, 9, 9);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 1.0, 0.0), 91, 0, 100, 9);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 0.0, 1.0), 0, 91, 9, 100);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 1.0, 1.0), 91, 91, 100, 100);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 0.5, 0.0), 45, 0, 55, 10);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 0.5, 0.0), 45, 0, 55, 10);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 0.0, 0.5), 0, 45, 10, 55);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 10, 10, 0.5, 0.5), 45, 45, 55, 55);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 0.5, 0.0), 46, 0, 55, 9);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 0.5, 0.0), 46, 0, 55, 9);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 0.0, 0.5), 0, 46, 9, 55);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 9, 9, 0.5, 0.5), 46, 46, 55, 55);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 120, 120, 0.0, 0.0), 0, 0, 120, 120);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_align (container, 120, 120, 0.5, 0.5), -10, -10, 110, 110);

	EEL_CHECK_BOOLEAN_RESULT (eel_dimensions_are_empty (empty_dimensions), TRUE);

	dim1.width = 10; dim1.height = 10;
	EEL_CHECK_BOOLEAN_RESULT (eel_dimensions_are_empty (dim1), FALSE);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (0, 0, dim1), 0, 0, 10, 10);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (1, 1, dim1), 1, 1, 11, 11);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (-1, 1, dim1), -1, 1, 9, 11);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (1, -1, dim1), 1, -1, 11, 9);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (-1, -1, dim1), -1, -1, 9, 9);

	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (2, 2, dim1), 2, 2, 12, 12);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (-2, 2, dim1), -2, 2, 8, 12);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (2, -2, dim1), 2, -2, 12, 8);
	EEL_CHECK_RECTANGLE_RESULT (eel_art_irect_assign_dimensions (-2, -2, dim1), -2, -2, 8, 8);

	EEL_CHECK_DIMENSIONS_RESULT (eel_dimensions_assign (0, 0), 0, 0);
	EEL_CHECK_DIMENSIONS_RESULT (eel_dimensions_assign (-1, -1), -1, -1);
	EEL_CHECK_DIMENSIONS_RESULT (eel_dimensions_assign (0, -1), 0, -1);
	EEL_CHECK_DIMENSIONS_RESULT (eel_dimensions_assign (-1, 0), -1, 0);

	EEL_CHECK_POINT_RESULT (eel_art_ipoint_assign (0, 0), 0, 0);
	EEL_CHECK_POINT_RESULT (eel_art_ipoint_assign (-1, -1), -1, -1);
	EEL_CHECK_POINT_RESULT (eel_art_ipoint_assign (0, -1), 0, -1);
	EEL_CHECK_POINT_RESULT (eel_art_ipoint_assign (-1, 0), -1, 0);

	/* test_irect_intersect */
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (-1, -1, -1, -1, -1, -1, -1, -1), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (-2, -2, -2, -2, -2, -2, -2, -2), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (0, 0, 10, 10, 0, 0, 0, 0), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (0, 0, 10, 10, 0, 0, 10, 10), 0, 0, 10, 10);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (0, 0, 10, 10, 0, 0, 5, 5), 0, 0, 5, 5);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (-5, -5, 5, 5, 5, 5, 6, 6), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (-5, -5, 5, 5, 4, 4, 6, 6), 4, 4, 5, 5);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_intersect (10, 10, 100, 100, 10, 10, 11, 11), 10, 10, 11, 11);

	/* test_irect_union */
	EEL_CHECK_RECTANGLE_RESULT (test_irect_union (0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_union (-1, -1, -1, -1, -1, -1, -1, -1), 0, 0, 0, 0);
	EEL_CHECK_RECTANGLE_RESULT (test_irect_union (-2, -2, -2, -2, -2, -2, -2, -2), 0, 0, 0, 0);

	/* test_ipoint_clamp */
	EEL_CHECK_POINT_RESULT (test_ipoint_clamp (0, 0, 0, 0, 0, 0), 0, 0);
	EEL_CHECK_POINT_RESULT (test_ipoint_clamp (5, 5, 0, 0, 10, 10), 5, 5);
	EEL_CHECK_POINT_RESULT (test_ipoint_clamp (0, 0, 0, 0, 10, 10), 0, 0);
	EEL_CHECK_POINT_RESULT (test_ipoint_clamp (10, 10, 0, 0, 10, 10), 10, 10);
	EEL_CHECK_POINT_RESULT (test_ipoint_clamp (11, 11, 0, 0, 10, 10), 10, 10);
	EEL_CHECK_POINT_RESULT (test_ipoint_clamp (-1, -1, 0, 0, 10, 10), 0, 0);

	/* test_dimensions_clamp */
	EEL_CHECK_DIMENSIONS_RESULT (test_dimensions_clamp (0, 0, 0, 0, 0, 0), 0, 0);
	EEL_CHECK_DIMENSIONS_RESULT (test_dimensions_clamp (5, 5, 0, 0, 10, 10), 5, 5);
	EEL_CHECK_DIMENSIONS_RESULT (test_dimensions_clamp (0, 0, 0, 0, 10, 10), 0, 0);
	EEL_CHECK_DIMENSIONS_RESULT (test_dimensions_clamp (10, 10, 0, 0, 10, 10), 10, 10);
	EEL_CHECK_DIMENSIONS_RESULT (test_dimensions_clamp (11, 11, 0, 0, 10, 10), 10, 10);
	EEL_CHECK_DIMENSIONS_RESULT (test_dimensions_clamp (-1, -1, 0, 0, 10, 10), 0, 0);
}

#endif /* !EEL_OMIT_SELF_CHECK */
