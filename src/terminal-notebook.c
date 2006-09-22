/*
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
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
 */

#include "terminal-notebook.h"
#include "terminal-window.h"
#include "terminal-intl.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#include <glib/gprintf.h>

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2

#define TERMINAL_NOTEBOOK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), TERMINAL_TYPE_NOTEBOOK, TerminalNotebookPrivate))

struct _TerminalNotebookPrivate
{
  gulong   motion_notify_handler_id;
  gulong   grab_notify_handler_id;
  gulong   toplevel_grab_broken_handler_id;
  gulong   toplevel_motion_notify_handler_id;
  gulong   toplevel_button_release_handler_id;
  gint     x_start, y_start;
  gboolean drag_in_progress;
  GtkTooltips *tooltips;
  GtkRcStyle *rcstyle;
};

static void terminal_notebook_init           (TerminalNotebook *notebook);
static void terminal_notebook_class_init     (TerminalNotebookClass *klass);
static void terminal_notebook_dispose        (GObject *object);
static void terminal_notebook_finalize       (GObject *object);
static void move_tab_to_another_notebook     (TerminalNotebook *src,
                                              TerminalNotebook *dest,
                                              GdkEventMotion   *event,
                                              int               dest_position);
static void move_tab                         (TerminalNotebook *notebook,
                                              int               dest_position);
          
/* Local variables */
static GdkCursor *cursor = NULL;

enum
{
  TAB_ADDED,
  TAB_REMOVED,
  TABS_REORDERED,
  TAB_DETACHED,
  TAB_DELETE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
terminal_notebook_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo our_info =
        {
          sizeof (TerminalNotebookClass),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) terminal_notebook_class_init,
          NULL,
          NULL, /* class_data */
          sizeof (TerminalNotebook),
          0, /* n_preallocs */
          (GInstanceInitFunc) terminal_notebook_init
        };
  
      type = g_type_register_static (GTK_TYPE_NOTEBOOK,
                                     "TerminalNotebook",
                                     &our_info, 0);
  
    }

  return type;
}

static void
terminal_notebook_class_init (TerminalNotebookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = terminal_notebook_dispose;
  object_class->finalize = terminal_notebook_finalize;

  signals[TAB_ADDED] =
    g_signal_new ("tab_added",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (TerminalNotebookClass, tab_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  TERMINAL_TYPE_SCREEN);
  signals[TAB_REMOVED] =
    g_signal_new ("tab_removed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (TerminalNotebookClass, tab_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  TERMINAL_TYPE_SCREEN);
  signals[TAB_DETACHED] =
    g_signal_new ("tab_detached",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (TerminalNotebookClass, tab_detached),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  TERMINAL_TYPE_SCREEN);
  signals[TABS_REORDERED] =
    g_signal_new ("tabs_reordered",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (TerminalNotebookClass, tabs_reordered),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  g_type_class_add_private (object_class, sizeof(TerminalNotebookPrivate));
}

static TerminalNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
  GdkWindow *win_at_pointer, *toplevel_win;
  gpointer toplevel = NULL;
  gint x, y;

  /* FIXME multi-head */
  win_at_pointer = gdk_window_at_pointer (&x, &y);
  if (win_at_pointer == NULL)
    {
      /* We are outside all windows containing a notebook */
      return NULL;
    }

  toplevel_win = gdk_window_get_toplevel (win_at_pointer);

  /* get the GtkWidget which owns the toplevel GdkWindow */
  gdk_window_get_user_data (toplevel_win, &toplevel);

  /* toplevel should be an TerminalWindow */
  if (toplevel != NULL && TERMINAL_IS_WINDOW (toplevel))
    {
      return TERMINAL_NOTEBOOK (terminal_window_get_notebook
                                (TERMINAL_WINDOW (toplevel)));
    }

  return NULL;
}

static gboolean
is_in_notebook_window (TerminalNotebook *notebook,
                       gint abs_x, gint abs_y)
{
  TerminalNotebook *nb_at_pointer;

  nb_at_pointer = find_notebook_at_pointer (abs_x, abs_y);

  return nb_at_pointer == notebook;
}

static gint
find_tab_num_at_pos (TerminalNotebook *notebook, gint abs_x, gint abs_y)
{
  GtkPositionType tab_pos;
  int page_num = 0;
  GtkNotebook *nb = GTK_NOTEBOOK (notebook);
  GtkWidget *page;

  tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

  if (GTK_NOTEBOOK (notebook)->first_tab == NULL)
    {
      return AFTER_ALL_TABS;
    }

  while ((page = gtk_notebook_get_nth_page (nb, page_num)))
    {
      GtkWidget *screen;
      gint max_x, max_y;
      gint x_root, y_root;
  
      screen = gtk_notebook_get_tab_label (nb, page);
      g_return_val_if_fail (screen != NULL, -1);
  
      if (!GTK_WIDGET_MAPPED (GTK_WIDGET (screen)))
       {
         page_num++;
         continue;
       }
  
      gdk_window_get_origin (GDK_WINDOW (screen->window),
                             &x_root, &y_root);
  
      max_x = x_root + screen->allocation.x + screen->allocation.width;
      max_y = y_root + screen->allocation.y + screen->allocation.height;
  
      if (((tab_pos == GTK_POS_TOP) || (tab_pos == GTK_POS_BOTTOM))
          &&(abs_x<=max_x))
        {
          return page_num;
        }
      else if (((tab_pos == GTK_POS_LEFT) || (tab_pos == GTK_POS_RIGHT))
               && (abs_y<=max_y))
        {
          return page_num;
        }
  
      page_num++;
    }
  return AFTER_ALL_TABS;
}

static gint find_notebook_and_tab_at_pos (gint abs_x, gint abs_y,
                                          TerminalNotebook **notebook,
                                          gint *page_num)
{
  *notebook = find_notebook_at_pointer (abs_x, abs_y);
  if (*notebook == NULL)
    {
      return NOT_IN_APP_WINDOWS;
    }
  *page_num = find_tab_num_at_pos (*notebook, abs_x, abs_y);

  if (*page_num < 0)
    {
      return *page_num;
    }
  else
    {
      return 0;
    }
}

void
terminal_notebook_move_tab (TerminalNotebook *src,
                            TerminalNotebook *dest,
                            TerminalScreen *screen,
                            int dest_position)
{
  if (dest == NULL || src == dest)
    {
      gtk_notebook_reorder_child
        (GTK_NOTEBOOK (src), GTK_WIDGET (screen), dest_position);
      
      if (src->priv->drag_in_progress == FALSE)
        {
          g_signal_emit (G_OBJECT (src), signals[TABS_REORDERED], 0);
        }
    }
  else
    {
      GtkWidget *toplevel;

      /* make sure the screen isn't destroyed while we move it */
      g_object_ref (screen);

      terminal_notebook_remove_tab (src, screen);

      /* Set new window for screen so TerminalScreen widget realize function
       * works.
       */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (dest));
      
      g_assert (GTK_WIDGET_TOPLEVEL (toplevel));
      g_assert (TERMINAL_IS_WINDOW (toplevel));

      terminal_screen_set_window (screen, TERMINAL_WINDOW(toplevel));
   
      terminal_notebook_add_tab (dest, screen, dest_position, TRUE);
      g_object_unref (screen);
    }
}

static void
drag_stop (TerminalNotebook *notebook,
           guint32 time)
{
  TerminalNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkWidget *toplevel, *child;

  if (priv->drag_in_progress)
    {
      toplevel = gtk_widget_get_toplevel (widget);
      g_return_if_fail (GTK_WIDGET_TOPLEVEL (toplevel));

      child = gtk_bin_get_child (GTK_BIN (toplevel));
      g_return_if_fail (child != NULL);

      /* disconnect the signals before ungrabbing! */
      if (priv->toplevel_grab_broken_handler_id != 0)
        {
          g_signal_handler_disconnect (toplevel,
                                       priv->toplevel_grab_broken_handler_id);
          priv->toplevel_grab_broken_handler_id = 0;
        }
      if (priv->grab_notify_handler_id != 0)
        {
          g_signal_handler_disconnect (notebook,
                                       priv->grab_notify_handler_id);
          priv->grab_notify_handler_id = 0;
        }
      if (priv->toplevel_motion_notify_handler_id != 0)
        {
          g_signal_handler_disconnect (toplevel,
                                       priv->toplevel_motion_notify_handler_id);
          priv->toplevel_motion_notify_handler_id = 0;
        }
      if (priv->toplevel_button_release_handler_id != 0)
        {
          g_signal_handler_disconnect (toplevel,
                                       priv->toplevel_button_release_handler_id);
          priv->toplevel_button_release_handler_id = 0;
        }

      /* ungrab the pointer if it's grabbed */
      /* FIXME multihead */
      if (gdk_pointer_is_grabbed ())
        {
          gdk_pointer_ungrab (time);
        }

      gtk_grab_remove (toplevel);

      g_signal_emit (G_OBJECT (notebook), signals[TABS_REORDERED], 0);
    }

  if (priv->motion_notify_handler_id != 0)
    {
      g_signal_handler_disconnect (notebook,
                                   priv->motion_notify_handler_id);
      priv->motion_notify_handler_id = 0;
    }

  priv->drag_in_progress = FALSE;
}

static gboolean
grab_broken_event_cb (GtkWidget *widget,
                      GdkEventGrabBroken *event,
                      TerminalNotebook *notebook)
{
  drag_stop (notebook, GDK_CURRENT_TIME /* FIXME? */);

  return FALSE;
}

static void
grab_notify_cb (GtkWidget *widget,
                gboolean was_grabbed,
                TerminalNotebook *notebook)
{
  drag_stop (notebook, GDK_CURRENT_TIME /* FIXME? */);
}

static gboolean
toplevel_motion_notify_cb (GtkWidget *toplevel,
                           GdkEventMotion *event,
                           TerminalNotebook *notebook)
{
  TerminalNotebook *dest = NULL;
  int page_num, result;

  result = find_notebook_and_tab_at_pos ((gint)event->x_root,
                                         (gint)event->y_root,
                                         &dest, &page_num);

  if (result != NOT_IN_APP_WINDOWS)
    {
      if (dest != notebook)
        {
          move_tab_to_another_notebook (notebook, dest,
                                        event, page_num);
        }
      else
        {
          g_assert (page_num >= -1);
          move_tab (notebook, page_num);
        }
    }

  return FALSE;
}

static gboolean
toplevel_button_release_cb (GtkWidget *toplevel,
                            GdkEventButton *event,
                            TerminalNotebook *notebook)
{
  TerminalNotebookPrivate *priv = notebook->priv;

  if (priv->drag_in_progress)
    {
      gint cur_page_num;
      GtkWidget *cur_page;

      cur_page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
      cur_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
                                            cur_page_num);

      if (!is_in_notebook_window (notebook, event->x_root, event->y_root)
          && gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 1)
        {
          /* Tab was detached */
          g_signal_emit (G_OBJECT (notebook),
                         signals[TAB_DETACHED], 0, cur_page);
        }
    }

  /* This must be called even if a drag isn't happening */
  drag_stop (notebook, event->time);

  return FALSE;
}

static gboolean
drag_start (TerminalNotebook *notebook,
            guint32 time)
{
  TerminalNotebookPrivate *priv = notebook->priv;
  GtkWidget *widget = GTK_WIDGET (notebook);
  GtkWidget *toplevel, *child;

  /* FIXME multihead */
  if (priv->drag_in_progress || gdk_pointer_is_grabbed ()) return FALSE;

  priv->drag_in_progress = TRUE;

  /* get a new cursor, if necessary */
  /* FIXME multi-head */
  if (!cursor) cursor = gdk_cursor_new (GDK_FLEUR);

  toplevel = gtk_widget_get_toplevel (widget);
  g_return_val_if_fail (GTK_WIDGET_TOPLEVEL (toplevel), FALSE);

  child = gtk_bin_get_child (GTK_BIN (toplevel));
  g_return_val_if_fail (child != NULL, FALSE);

  /* grab the pointer */
  gtk_grab_add (toplevel);

  /* FIXME multi-head */
  if (gdk_pointer_grab (toplevel->window,
                        FALSE,
                        GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                        NULL, cursor, time) != GDK_GRAB_SUCCESS)
    {
      drag_stop (notebook, time);
      return FALSE;
    }

  g_return_val_if_fail (priv->toplevel_grab_broken_handler_id == 0, FALSE);
  g_return_val_if_fail (priv->toplevel_motion_notify_handler_id == 0, FALSE);
  g_return_val_if_fail (priv->toplevel_button_release_handler_id == 0, FALSE);
  g_return_val_if_fail (priv->grab_notify_handler_id == 0, FALSE);

  priv->toplevel_grab_broken_handler_id =
    g_signal_connect (toplevel, "grab-broken-event",
                      G_CALLBACK (grab_broken_event_cb), notebook);
  priv->toplevel_motion_notify_handler_id =
    g_signal_connect (toplevel, "motion-notify-event",
                      G_CALLBACK (toplevel_motion_notify_cb), notebook);
  priv->toplevel_button_release_handler_id =
    g_signal_connect (toplevel, "button-release-event",
                      G_CALLBACK (toplevel_button_release_cb), notebook);
  priv->grab_notify_handler_id =
    g_signal_connect (notebook, "grab-notify",
                      G_CALLBACK (grab_notify_cb), notebook);

  return TRUE;
}



/* this function is only called during dnd, we don't need to emit TABS_REORDERED
 * here, instead we do it on drag_stop
 */
static void
move_tab (TerminalNotebook *notebook,
          int dest_position)
{
  gint cur_page_num;

  cur_page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

  if (dest_position != cur_page_num)
    {
      GtkWidget *cur_tab;
      
      cur_tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook),
                                           cur_page_num);
      terminal_notebook_move_tab (TERMINAL_NOTEBOOK (notebook), NULL,
                                  TERMINAL_SCREEN (cur_tab),
                                  dest_position);
    }
}

static gboolean
motion_notify_cb (TerminalNotebook *notebook,
                  GdkEventMotion *event,
                  gpointer data)
{
  TerminalNotebookPrivate *priv = notebook->priv;

  if (priv->drag_in_progress == FALSE)
    {
      if (gtk_drag_check_threshold (GTK_WIDGET (notebook),
                                    notebook->priv->x_start,
                                    notebook->priv->y_start,
                                    event->x_root, event->y_root))
        {
          return drag_start (notebook, event->time);
        }

      return FALSE;
    }

  return FALSE;
}


static void
move_tab_to_another_notebook (TerminalNotebook *src,
                              TerminalNotebook *dest,
                              GdkEventMotion *event,
                              int dest_position)
{
  GtkWidget *screen;
  int cur_page;

  /* This is getting tricky, the screen was dragged in a notebook
   * in another window of the same app, we move the screen
   * to that new notebook, and let this notebook handle the
   * drag
  */
  g_assert (TERMINAL_IS_NOTEBOOK (dest));
  g_assert (dest != src);

  cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (src));
  screen = gtk_notebook_get_nth_page (GTK_NOTEBOOK (src), cur_page);

  /* stop drag in origin window */
  /* ungrab the pointer if it's grabbed */
  drag_stop (src, event->time);
  if (gdk_pointer_is_grabbed ())
    {
      gdk_pointer_ungrab (event->time);
    }
  gtk_grab_remove (GTK_WIDGET (src));

  terminal_notebook_move_tab (src, dest, TERMINAL_SCREEN (screen),
                              dest_position);

  /* start drag handling in dest notebook */

  dest->priv->motion_notify_handler_id =
    g_signal_connect (G_OBJECT (dest),
                      "motion-notify-event",
                      G_CALLBACK (motion_notify_cb),
                      NULL);

  drag_start (dest, event->time);
}

static gboolean
button_release_cb (TerminalNotebook *notebook,
                   GdkEventButton *event,
                   gpointer data)
{
  /* This must be called even if a drag isn't happening */
  drag_stop (notebook, event->time);

  return FALSE;
}

static gboolean
button_press_cb (TerminalNotebook *notebook,
                 GdkEventButton *event,
                 gpointer data)
{
  gint tab_clicked = find_tab_num_at_pos (notebook,
                                          event->x_root,
                                          event->y_root);

  if (notebook->priv->drag_in_progress)
    {
      return TRUE;
    }

  if ((event->button == 1) && (event->type == GDK_BUTTON_PRESS)
      && (tab_clicked >= 0))
    {
      notebook->priv->x_start = event->x_root;
      notebook->priv->y_start = event->y_root;
      notebook->priv->motion_notify_handler_id =
        g_signal_connect (G_OBJECT (notebook),
                          "motion-notify-event",
                          G_CALLBACK (motion_notify_cb), NULL);
    }
  else if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
    {
      if (tab_clicked == -1)
        {
          /* consume event, so that we don't pop up the context menu when
           * the mouse if not over a screen label
           */
          return TRUE;
        }
      else
        {
          /* switch to the page the mouse is over, but don't consume the event
           */
          gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_clicked);
        }
    }

  return FALSE;
}

GtkWidget *
terminal_notebook_new (void)
{
  return GTK_WIDGET (g_object_new (TERMINAL_TYPE_NOTEBOOK, NULL));
}

/*
 * update_tabs_visibility: Hide tabs if there is only one screen
 * and the pref is not set.
 */
static void
update_tabs_visibility (TerminalNotebook *nb, gboolean before_inserting)
{
  gboolean show_tabs;
  guint num;

  num = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb));

  if (before_inserting) num++;

  show_tabs = (num > 1);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), show_tabs);
}

static void
terminal_notebook_init (TerminalNotebook *notebook)
{
  notebook->priv = TERMINAL_NOTEBOOK_GET_PRIVATE (notebook);

  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);

  notebook->priv->tooltips = gtk_tooltips_new ();
  g_object_ref (notebook->priv->tooltips);
  gtk_object_sink (GTK_OBJECT (notebook->priv->tooltips));

  notebook->priv->rcstyle = gtk_rc_style_new ();
  notebook->priv->rcstyle->xthickness = 0;
  notebook->priv->rcstyle->ythickness = 0;

  g_signal_connect (notebook, "button-press-event",
                    (GCallback)button_press_cb, NULL);
  g_signal_connect (notebook, "button-release-event",
                    (GCallback)button_release_cb, NULL);
  gtk_widget_add_events (GTK_WIDGET (notebook), GDK_BUTTON1_MOTION_MASK);

}

static void
terminal_notebook_dispose (GObject *object)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (object);

  if (notebook->priv->tooltips)
    {
      g_object_unref (notebook->priv->tooltips);
      notebook->priv->tooltips = NULL;
    }

  if (notebook->priv->rcstyle)
    {
      g_object_unref (notebook->priv->rcstyle);
      notebook->priv->rcstyle = NULL;
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
terminal_notebook_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
sync_label (TerminalScreen *screen, TerminalNotebook *nb)
{
  GtkWidget *hbox, *ebox, *label;
  GList *children;
  const char *title;

  hbox = gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb), GTK_WIDGET (screen));
  children = gtk_container_get_children (GTK_CONTAINER (hbox));
  ebox = g_list_first (children)->data;
  g_list_free (children);
  children = gtk_container_get_children (GTK_CONTAINER (ebox));
  label = g_list_first (children)->data;
  g_list_free (children);

  title = terminal_screen_get_title (screen);

  gtk_label_set_text (GTK_LABEL (label), title);

  if (G_LIKELY (nb->priv->tooltips))
    {
      gtk_tooltips_set_tip (nb->priv->tooltips, ebox, title, NULL);
    }
}

static void
close_button_clicked_cb (GtkWidget *widget, TerminalScreen *screen)
{
  GtkWidget *notebook;

  notebook = gtk_widget_get_parent (GTK_WIDGET (screen));
  terminal_notebook_remove_tab (TERMINAL_NOTEBOOK (notebook), screen);
}

void
terminal_notebook_add_tab (TerminalNotebook *nb,
                           TerminalScreen *screen,
                           int position,
                           gboolean jump_to)
{
  const char *title;
  GtkWidget *hbox, *label, *label_ebox, *close_button, *image;
  GtkSettings *settings;
  gint w, h;

  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  hbox = gtk_hbox_new (FALSE, 4);

  title = terminal_screen_get_title (screen);

  label_ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (label_ebox), FALSE);
  if (G_LIKELY (nb->priv->tooltips))
    {
      gtk_tooltips_set_tip (nb->priv->tooltips, label_ebox, title, NULL);
    }

  label = gtk_label_new (title);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (label_ebox), label);
  gtk_box_pack_start (GTK_BOX (hbox), label_ebox, TRUE, TRUE, 0);

  close_button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);

  if (G_LIKELY (nb->priv->rcstyle))
    {
      gtk_widget_modify_style (close_button, nb->priv->rcstyle);
    }

  settings = gtk_widget_get_settings (GTK_WIDGET (label));
  gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &w, &h);
  gtk_widget_set_size_request (close_button, w + 2, h + 2);

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (close_button), image);
  gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

  if (G_LIKELY (nb->priv->tooltips))
    {
      gtk_tooltips_set_tip (nb->priv->tooltips, close_button, _("Close tab"), NULL);
    }

  g_signal_connect (G_OBJECT (close_button), "clicked",
		    G_CALLBACK (close_button_clicked_cb),
		    screen);

  gtk_widget_show (label);
  gtk_widget_show (label_ebox);
  gtk_widget_show (image);
  gtk_widget_show (close_button);
  gtk_widget_show (hbox);

  update_tabs_visibility (nb, TRUE);

  gtk_notebook_insert_page (GTK_NOTEBOOK (nb), GTK_WIDGET (screen),
			    hbox, position);

  gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (nb),
                                      GTK_WIDGET (screen),
                                      TRUE, TRUE, GTK_PACK_START);

  g_signal_connect (G_OBJECT (screen),
                    "title-changed",
                    G_CALLBACK (sync_label),
                    nb);

  g_signal_emit (G_OBJECT (nb), signals[TAB_ADDED], 0, screen);

  /* The signal handler may have reordered the tabs */
  position = gtk_notebook_page_num (GTK_NOTEBOOK (nb), GTK_WIDGET (screen));

  if (jump_to)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), position);
      g_object_set_data (G_OBJECT (screen), "jump_to",
                         GINT_TO_POINTER (jump_to));

    }
}

void
terminal_notebook_remove_tab (TerminalNotebook *nb,
                              TerminalScreen *screen)
{
  int position, curr;
  GtkWidget *label;

  g_return_if_fail (TERMINAL_IS_NOTEBOOK (nb));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  position = gtk_notebook_page_num (GTK_NOTEBOOK (nb), GTK_WIDGET (screen));
  curr = gtk_notebook_get_current_page (GTK_NOTEBOOK (nb));

  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (nb), GTK_WIDGET (screen));

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (sync_label), nb);

  /**
   * we ref the screen so that it's still alive while the tabs_removed
   * signal is processed.
   */
  g_object_ref (screen);

  gtk_notebook_remove_page (GTK_NOTEBOOK (nb), position);

  update_tabs_visibility (nb, FALSE);

  g_signal_emit (G_OBJECT (nb), signals[TAB_REMOVED], 0, screen);

  g_object_unref (screen);
}
