/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-ellipsizing-label.c: Subclass of GtkLabel that ellipsizes the text.

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

#include <config.h>
#include "eel-ellipsizing-label.h"

#include "eel-gtk-macros.h"
#include "eel-pango-extensions.h"
#include "eel-string.h"

struct EelEllipsizingLabelDetails
{
	char *full_text;
};

static void eel_ellipsizing_label_class_init (EelEllipsizingLabelClass *class);
static void eel_ellipsizing_label_init       (EelEllipsizingLabel      *label);

EEL_CLASS_BOILERPLATE (EelEllipsizingLabel, eel_ellipsizing_label, GTK_TYPE_LABEL)

static void
eel_ellipsizing_label_init (EelEllipsizingLabel *label)
{
	label->details = g_new0 (EelEllipsizingLabelDetails, 1);
}

static void
real_finalize (GObject *object)
{
	EelEllipsizingLabel *label;

	label = EEL_ELLIPSIZING_LABEL (object);

	g_free (label->details->full_text);
	g_free (label->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

GtkWidget*
eel_ellipsizing_label_new (const char *string)
{
	EelEllipsizingLabel *label;
  
	label = g_object_new (EEL_TYPE_ELLIPSIZING_LABEL, NULL);
	eel_ellipsizing_label_set_text (label, string);
  
	return GTK_WIDGET (label);
}

void
eel_ellipsizing_label_set_text (EelEllipsizingLabel *label, 
				const char          *string)
{
	g_return_if_fail (EEL_IS_ELLIPSIZING_LABEL (label));

	if (eel_str_is_equal (string, label->details->full_text)) {
		return;
	}

	g_free (label->details->full_text);
	label->details->full_text = g_strdup (string);

	/* Queues a resize as side effect */
	gtk_label_set_text (GTK_LABEL (label), label->details->full_text);
}

static void
real_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_request, (widget, requisition));

	/* Don't demand any particular width; will draw ellipsized into whatever size we're given */
	requisition->width = 0;
}

static void	
real_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EelEllipsizingLabel *label;

	label = EEL_ELLIPSIZING_LABEL (widget);
	
	/* This is the bad hack of the century, using private
	 * GtkLabel layout object. If the layout is NULL
	 * then it got blown away since size request,
	 * we just punt in that case, I don't know what to do really.
	 */

	if (GTK_LABEL (label)->layout != NULL) {
		if (label->details->full_text == NULL) {
			pango_layout_set_text (GTK_LABEL (label)->layout, "", -1);
		} else {
			EelEllipsizeMode mode;

			if (ABS (GTK_MISC (label)->xalign - 0.5) < 1e-12)
				mode = EEL_ELLIPSIZE_MIDDLE;
			else if (GTK_MISC (label)->xalign < 0.5)
				mode = EEL_ELLIPSIZE_END;
			else
				mode = EEL_ELLIPSIZE_START;
			
			eel_pango_layout_set_text_ellipsized (GTK_LABEL (label)->layout,
							      label->details->full_text,
							      allocation->width,
							      mode);
		}
	}
	
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
}

static gboolean
real_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	EelEllipsizingLabel *label;
	GtkRequisition req;
	
	label = EEL_ELLIPSIZING_LABEL (widget);

	/* push/pop the actual size so expose draws in the right
	 * place, yes this is bad hack central. Here we assume the
	 * ellipsized text has been set on the layout in size_allocate
	 */
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_request, (widget, &req));
	widget->requisition.width = req.width;
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, expose_event, (widget, event));
	widget->requisition.width = 0;

	return FALSE;
}


static void
eel_ellipsizing_label_class_init (EelEllipsizingLabelClass *klass)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = real_finalize;

	widget_class->size_request = real_size_request;
	widget_class->size_allocate = real_size_allocate;
	widget_class->expose_event = real_expose_event;
}
