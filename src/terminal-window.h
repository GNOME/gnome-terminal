/* widget for a toplevel terminal window, possibly containing multiple terminals */

/*
 * Copyright (C) 2001 Havoc Pennington
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

#ifndef TERMINAL_WINDOW_H
#define TERMINAL_WINDOW_H

#include <gtk/gtkwindow.h>
#include "terminal-screen.h"

G_BEGIN_DECLS

#define TERMINAL_TYPE_WINDOW              (terminal_window_get_type ())
#define TERMINAL_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_WINDOW, TerminalWindow))
#define TERMINAL_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_WINDOW, TerminalWindowClass))
#define TERMINAL_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_WINDOW))
#define TERMINAL_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_WINDOW))
#define TERMINAL_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_WINDOW, TerminalWindowClass))

typedef struct _TerminalWindowClass   TerminalWindowClass;
typedef struct _TerminalWindowPrivate TerminalWindowPrivate;

struct _TerminalWindow
{
  GtkWindow parent_instance;

  TerminalWindowPrivate *priv;
};

struct _TerminalWindowClass
{
  GtkWindowClass parent_class;

};

GType terminal_window_get_type (void) G_GNUC_CONST;

TerminalWindow* terminal_window_new (GConfClient *conf);

void terminal_window_add_screen (TerminalWindow *window,
                                 TerminalScreen *screen);

void terminal_window_remove_screen (TerminalWindow *window,
                                    TerminalScreen *screen);

/* Menubar visibility is part of session state, except that
 * if it isn't restored from session, the window gets the setting
 * from the profile of the first screen added to the window
 */
void terminal_window_set_menubar_visible     (TerminalWindow *window,
                                              gboolean        setting);
gboolean terminal_window_get_menubar_visible (TerminalWindow *window);

void            terminal_window_set_active (TerminalWindow *window,
                                            TerminalScreen *screen);
TerminalScreen* terminal_window_get_active (TerminalWindow *window);

/* In order of their tabs in the notebook */
GList* terminal_window_list_screens (TerminalWindow *window);
int    terminal_window_get_screen_count (TerminalWindow *window);

void terminal_window_update_icon      (TerminalWindow *window);
void terminal_window_update_geometry  (TerminalWindow *window);
void terminal_window_set_size         (TerminalWindow *window,
                                       TerminalScreen *screen,
                                       gboolean        even_if_mapped);
void terminal_window_set_size_force_grid (TerminalWindow *window,
                                          TerminalScreen *screen,
                                          gboolean        even_if_mapped,
                                          int             force_grid_width,
                                          int             force_grid_height);

void     terminal_window_set_fullscreen (TerminalWindow *window,
                                         gboolean        setting);
gboolean terminal_window_get_fullscreen (TerminalWindow *window);

GtkWidget* terminal_window_get_notebook (TerminalWindow *window);

void terminal_window_reread_profile_list (TerminalWindow *window);

void terminal_window_set_startup_id (TerminalWindow *window,
                                     const char     *startup_id);

gboolean terminal_window_uses_argb_visual (TerminalWindow *window);

G_END_DECLS

#endif /* TERMINAL_WINDOW_H */
