/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-ellipsizing-label.h: Subclass of GtkLabel that ellipsizes the text.

   Copyright (C) 2001 Eazel, Inc.

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

   Author: John Sullivan <sullivan@eazel.com>,
 */

#ifndef EEL_ELLIPSIZING_LABEL_H
#define EEL_ELLIPSIZING_LABEL_H

#include <gtk/gtklabel.h>

#define EEL_TYPE_ELLIPSIZING_LABEL            (eel_ellipsizing_label_get_type ())
#define EEL_ELLIPSIZING_LABEL(obj)            (GTK_CHECK_CAST ((obj), EEL_TYPE_ELLIPSIZING_LABEL, EelEllipsizingLabel))
#define EEL_ELLIPSIZING_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EEL_TYPE_ELLIPSIZING_LABEL, EelEllipsizingLabelClass))
#define EEL_IS_ELLIPSIZING_LABEL(obj)         (GTK_CHECK_TYPE ((obj), EEL_TYPE_ELLIPSIZING_LABEL))
#define EEL_IS_ELLIPSIZING_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EEL_TYPE_ELLIPSIZING_LABEL))

typedef struct EelEllipsizingLabel	      EelEllipsizingLabel;
typedef struct EelEllipsizingLabelClass	      EelEllipsizingLabelClass;
typedef struct EelEllipsizingLabelDetails     EelEllipsizingLabelDetails;

struct EelEllipsizingLabel {
	GtkLabel parent;
	EelEllipsizingLabelDetails *details;
};

struct EelEllipsizingLabelClass {
	GtkLabelClass parent_class;
};

GtkType    eel_ellipsizing_label_get_type (void);
GtkWidget *eel_ellipsizing_label_new      (const char          *string);
void       eel_ellipsizing_label_set_text (EelEllipsizingLabel *label,
					   const char          *string);

#endif /* EEL_ELLIPSIZING_LABEL_H */
