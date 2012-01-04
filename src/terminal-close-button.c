/*
 * terminal-close-button.c
 *
 * Copyright © 2010 - Paolo Borelli
 * Copyright © 2011 - Ignacio Casal Quinteiro
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

#include "terminal-close-button.h"

#if GTK_CHECK_VERSION (3, 0, 0)
struct _TerminalCloseButtonClassPrivate {
	GtkCssProvider *css;
};

G_DEFINE_TYPE_WITH_CODE (TerminalCloseButton, terminal_close_button, GTK_TYPE_BUTTON,
                         g_type_add_class_private (g_define_type_id, sizeof (TerminalCloseButtonClassPrivate)))
#else
G_DEFINE_TYPE (TerminalCloseButton, terminal_close_button, GTK_TYPE_BUTTON)

static void
terminal_close_button_style_set (GtkWidget *button,
				 GtkStyle *previous_style)
{
	gint h, w;

	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
					   GTK_ICON_SIZE_MENU, &w, &h);

	gtk_widget_set_size_request (button, w + 2, h + 2);

	GTK_WIDGET_CLASS (terminal_close_button_parent_class)->style_set (button, previous_style);
}
#endif

static void
terminal_close_button_class_init (TerminalCloseButtonClass *klass)
{
#if GTK_CHECK_VERSION (3, 0, 0)
	static const gchar button_style[] =
		"* {\n"
		  "-GtkButton-default-border : 0;\n"
		  "-GtkButton-default-outside-border : 0;\n"
		  "-GtkButton-inner-border: 0;\n"
		  "-GtkWidget-focus-line-width : 0;\n"
		  "-GtkWidget-focus-padding : 0;\n"
		  "padding: 0;\n"
		"}";

	klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButtonClassPrivate);

	klass->priv->css = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (klass->priv->css, button_style, -1, NULL);
#else
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->style_set = terminal_close_button_style_set;

	gtk_rc_parse_string ("style \"gnome-terminal-tab-close-button-style\"\n"
                       "{\n"
                          "GtkWidget::focus-padding = 0\n"
                          "GtkWidget::focus-line-width = 0\n"
                          "xthickness = 0\n"
                          "ythickness = 0\n"
                       "}\n"
                       "widget \"*.gnome-terminal-tab-close-button\" style \"gnome-terminal-tab-close-button-style\"");
#endif
}

static void
terminal_close_button_init (TerminalCloseButton *button)
{
	GtkWidget *image;
	GIcon *icon;

	icon = g_themed_icon_new_with_default_fallbacks ("window-close-symbolic");
	image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
	g_object_unref (icon);

	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (button), image);

#if GTK_CHECK_VERSION (3, 0, 0)
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (button));
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (TERMINAL_CLOSE_BUTTON_GET_CLASS (button)->priv->css),
		                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#else
	gtk_widget_set_name (GTK_WIDGET (button), "gnome-terminal-tab-close-button");
#endif
}

GtkWidget *
terminal_close_button_new ()
{
	return GTK_WIDGET (g_object_new (TERMINAL_TYPE_CLOSE_BUTTON,
	                                 "relief", GTK_RELIEF_NONE,
	                                 "focus-on-click", FALSE,
	                                 NULL));
}

/* ex:set ts=8 noet: */
