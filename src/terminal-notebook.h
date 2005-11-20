/*
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2005 Tony Tsui
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef TERMINAL_NOTEBOOK_H
#define TERMINAL_NOTEBOOK_H

#include <glib.h>
#include <gtk/gtknotebook.h>

#include "terminal-screen.h"

G_BEGIN_DECLS

#define TERMINAL_TYPE_NOTEBOOK         (terminal_notebook_get_type ())
#define TERMINAL_NOTEBOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_NOTEBOOK, TerminalNotebook))
#define TERMINAL_NOTEBOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_NOTEBOOK, TerminalNotebookClass))
#define TERMINAL_IS_NOTEBOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_NOTEBOOK))
#define TERMINAL_IS_NOTEBOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_NOTEBOOK))
#define TERMINAL_NOTEBOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_NOTEBOOK, TerminalNotebookClass))

typedef struct _TerminalNotebookClass    TerminalNotebookClass;
typedef struct _TerminalNotebook         TerminalNotebook;
typedef struct _TerminalNotebookPrivate  TerminalNotebookPrivate;

struct _TerminalNotebook
{
  GtkNotebook parent;

  /*< private >*/
  TerminalNotebookPrivate *priv;
};

struct _TerminalNotebookClass
{
  GtkNotebookClass parent_class;

  /* Signals */
  void     (* tab_added)      (TerminalNotebook *notebook,
                               TerminalScreen   *screen);
  void     (* tab_removed)    (TerminalNotebook *notebook,
                               TerminalScreen   *screen);
  void     (* tab_detached)   (TerminalNotebook *notebook,
                               TerminalScreen   *screen);
  void     (* tabs_reordered) (TerminalNotebook *notebook);
  gboolean (* tab_delete)     (TerminalNotebook *notebook,
                               TerminalScreen   *screen);
};

GType           terminal_notebook_get_type       (void);

GtkWidget      *terminal_notebook_new            (void);

void            terminal_notebook_add_tab        (TerminalNotebook *nb,
                                                  TerminalScreen   *screen,
                                                  int               positionl,
                                                  gboolean          jump_to);

void            terminal_notebook_remove_tab     (TerminalNotebook *nb,
                                                  TerminalScreen   *screen);

void            terminal_notebook_move_tab       (TerminalNotebook *src,
                                                  TerminalNotebook *dest,
                                                  TerminalScreen   *screen,
                                                  int               dest_position);

void            terminal_notebook_set_show_tabs  (TerminalNotebook *nb,
                                                  gboolean          show_tabs);

G_END_DECLS

#endif /* TERMINAL_NOTEBOOK_H */
