/* Zvt implementation of terminal-widget.h */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#include "terminal-widget.h"
#include "terminal-intl.h"

#include <libzvt/libzvt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>


/* hacky map from signals into something much lamer.
 * Could all be avoided if the terminal had exactly these
 * signals and the signals took no arguments ;-)
 */
typedef enum
{
  CB_TITLE_CHANGED,
  CB_ICON_TITLE_CHANGED,
  CB_SELECTION_CHANGED,
  CB_CHILD_DIED
} CbType;

typedef struct
{
  CbType    type;
  GCallback callback;
  void     *data;
  int       refcount;
  guint     removed : 1;
} Callback;

typedef struct
{
  GdkFont *normal_font;
  GdkFont *bold_font;
  char *bg_file;
  double bg_darkness;
  GSList *callbacks;
  char *title;
  char *icon_title;
  guint bg_transparent : 1;
  guint bg_scrolls : 1;
  guint allow_bold : 1;
} ZvtData;

static void
zvt_data_add_callback (ZvtData  *zd,
                       CbType    type,
                       GCallback callback,
                       void     *data)
{
  Callback *cb;

  cb = g_new (Callback, 1);
  cb->type = type;
  cb->callback = callback;
  cb->data = data;
  cb->refcount = 1;
  cb->removed = FALSE;
  
  zd->callbacks = g_slist_append (zd->callbacks, cb);
}

static void
zvt_data_remove_callback (ZvtData  *zd,
                          CbType    type,
                          GCallback callback,
                          void     *data)
{
  GSList *tmp;

  /* mmmmm, efficiency */
 again:
  tmp = zd->callbacks;
  while (tmp != NULL)
    {
      Callback *cb = tmp->data;

      if (cb->type == type &&
          cb->callback == callback &&
          cb->data == data)
        break;
      
      tmp = tmp->next;
    }

  if (tmp)
    {
      Callback *cb = tmp->data;
      zd->callbacks = g_slist_remove (zd->callbacks, tmp->data);
      cb->refcount -= 1;
      cb->removed = TRUE;
      if (cb->refcount == 0)
        g_free (cb);
      goto again;
    }
}

static void
zvt_data_invoke_callbacks (GtkWidget *widget,
                           ZvtData   *zd,
                           CbType     type)
{
  GSList *copy;
  GSList *tmp;

  /* Make some effort to be reentrant, though it isn't
   * totally robust probably
   */
  
  copy = g_slist_copy (zd->callbacks);
  tmp = copy;
  while (tmp != NULL)
    {
      Callback *cb = tmp->data;

      cb->refcount += 1;

      tmp = tmp->next;
    }

  tmp = copy;
  while (tmp != NULL)
    {
      Callback *cb = tmp->data;

      if (cb->type == type &&
          !cb->removed)
        {
          typedef void (* MyFunc) (GtkWidget *widget, void *data);
          MyFunc func;
          func = (MyFunc) cb->callback;
          
          (* func) (widget, cb->data);
        }

      tmp = tmp->next;
    }

  tmp = copy;
  while (tmp != NULL)
    {
      Callback *cb = tmp->data;

      cb->refcount -= 1;
      if (cb->refcount == 0)
        g_free (cb);
      
      tmp = tmp->next;
    }

  g_slist_free (copy);
}

static void
zvt_title_changed_callback (GtkWidget      *zvt,
                            VTTITLE_TYPE    type,
                            const char     *title,
                            ZvtData        *zd)
{
  gboolean title_changed;
  gboolean icon_title_changed;

  title_changed = FALSE;
  icon_title_changed = FALSE;
  switch (type)
    {
    case VTTITLE_WINDOW:
      g_free (zd->title);
      zd->title = g_strdup (title);
      title_changed = TRUE;
      break;
      
    case VTTITLE_WINDOWICON:
      g_free (zd->title);
      g_free (zd->icon_title);
      zd->title = g_strdup (title);
      zd->icon_title = g_strdup (title);
      title_changed = TRUE;
      icon_title_changed = TRUE;
      break;

    case VTTITLE_ICON:
      g_free (zd->icon_title);
      zd->icon_title = g_strdup (title);
      icon_title_changed = TRUE;
      break;
      
    case VTTITLE_XPROPERTY:
      /* See gnome-terminal.c - this is supposed to
       * be a "XPROPNAME=VALUE" pair to set XPROPNAME on the toplevel
       * with VALUE as an XA_STRING, or if no "=VALUE" a way to delete
       * the property. Does anything use this?
       */
      break;
  }

  if (title_changed)
    zvt_data_invoke_callbacks (zvt, zd, CB_TITLE_CHANGED);
  if (icon_title_changed)
    zvt_data_invoke_callbacks (zvt, zd, CB_ICON_TITLE_CHANGED);
}

static void
zvt_child_died_callback (GtkWidget      *zvt,
                         ZvtData        *zd)
{
  zvt_data_invoke_callbacks (zvt, zd, CB_CHILD_DIED);
}

static void
zvt_selection_changed_callback (GtkWidget      *zvt,
                                ZvtData        *zd)
{
  zvt_data_invoke_callbacks (zvt, zd, CB_SELECTION_CHANGED);
}

static void
free_zvt_data (void *data)
{
  ZvtData *zd;
  GSList *tmp;
  
  zd = data;

  tmp = zd->callbacks;
  while (tmp != NULL)
    {
      Callback *cb = tmp->data;
      cb->refcount -= 1;
      if (cb->refcount == 0)
        g_free (cb);

      tmp = tmp->next;
    }
  g_slist_free (zd->callbacks);
  
  if (zd->normal_font)
    gdk_font_unref (zd->normal_font);
  if (zd->bold_font)
    gdk_font_unref (zd->bold_font);
  g_free (zd->bg_file);

  g_free (zd->title);
  g_free (zd->icon_title);
  
  g_free (zd);
}

GtkWidget*
terminal_widget_new (void)
{
  GtkWidget *widget;
  ZvtData *zd;
  
  widget = zvt_term_new_with_size (80, 24);

  zd = g_new0 (ZvtData, 1);
  zd->allow_bold = TRUE;
  g_object_set_data_full (G_OBJECT (widget), "terminal-widget-data",
                          zd, free_zvt_data);

  zvt_term_set_auto_window_hint (ZVT_TERM (widget), FALSE);

  /* Fix defaults */
  zvt_term_set_del_key_swap (ZVT_TERM (widget), TRUE);
  zvt_term_set_del_is_del (ZVT_TERM (widget), FALSE);

  g_signal_connect (G_OBJECT (widget),
                    "title_changed",
                    G_CALLBACK (zvt_title_changed_callback),
                    zd);

  g_signal_connect (G_OBJECT (widget),
                    "child_died",
                    G_CALLBACK (zvt_child_died_callback),
                    zd);

  g_signal_connect (G_OBJECT (widget),
                    "selection_changed",
                    G_CALLBACK (zvt_selection_changed_callback),
                    zd);
  
  return widget;
}

void
terminal_widget_set_size (GtkWidget            *widget,
                          int                   width_chars,
                          int                   height_chars)
{
  zvt_term_set_size (ZVT_TERM (widget), width_chars, height_chars);
}

void
terminal_widget_get_size (GtkWidget            *widget,
                          int                  *width_chars,
                          int                  *height_chars)
{
  if (width_chars)
    *width_chars = ZVT_TERM (widget)->grid_width;
  if (height_chars)
    *height_chars = ZVT_TERM (widget)->grid_height;
}

void
terminal_widget_get_cell_size (GtkWidget            *widget,
                               int                  *cell_width_pixels,
                               int                  *cell_height_pixels)
{
  if (cell_width_pixels)
    *cell_width_pixels = ZVT_TERM (widget)->charwidth;
  if (cell_height_pixels)
    *cell_height_pixels = ZVT_TERM (widget)->charheight;
}

#define PADDING 0 /* from zvtterm.c */
void
terminal_widget_get_padding (GtkWidget *widget,
                             int       *xpad,
                             int       *ypad)
{
  if (xpad)
    *xpad = widget->style->xthickness * 2 + PADDING;
  
  if (ypad)
    *ypad = widget->style->ythickness * 2;
}

void
terminal_widget_match_add (GtkWidget            *widget,
                           const char           *regexp)
{
  zvt_term_match_add (ZVT_TERM (widget),
                      (char*) regexp,
                      VTATTR_UNDERLINE, NULL);  
}

void
terminal_widget_skey_match_add (GtkWidget            *widget,
				const char           *regexp)
{
}

char*
terminal_widget_check_match (GtkWidget            *widget,
                             int                   column,
                             int                   row)
{
  return g_strdup (zvt_term_match_check (ZVT_TERM (widget),
                                         column, row, 0));
}

char*
terminal_widget_skey_check_match (GtkWidget            *widget,
				  int                   column,
				  int                   row)
{
	return NULL;
}

void
terminal_widget_skey_match_remove (GtkWidget            *widget)
{
}

void
terminal_widget_set_word_characters (GtkWidget  *widget,
                                     const char *str)
{
  zvt_term_set_wordclass (ZVT_TERM (widget), (char*) str);
}

void
terminal_widget_set_delete_binding (GtkWidget            *widget,
                                    TerminalEraseBinding  binding)
{
  ZvtTerm *term;

  term = ZVT_TERM (widget);
  
  switch (binding)
    {
    case TERMINAL_ERASE_CONTROL_H:
      zvt_term_set_delete_binding (term, ZVT_ERASE_CONTROL_H);
      break;
    case TERMINAL_ERASE_ESCAPE_SEQUENCE:
      zvt_term_set_delete_binding (term, ZVT_ERASE_ESCAPE_SEQUENCE);
      break;
    case TERMINAL_ERASE_ASCII_DEL:
      zvt_term_set_delete_binding (term, ZVT_ERASE_ASCII_DEL);
      break;
    }
}

void
terminal_widget_set_backspace_binding (GtkWidget            *widget,
                                       TerminalEraseBinding  binding)
{
  ZvtTerm *term;

  term = ZVT_TERM (widget);
  
  switch (binding)
    {
    case TERMINAL_ERASE_CONTROL_H:
      zvt_term_set_backspace_binding (term, ZVT_ERASE_CONTROL_H);
      break;
    case TERMINAL_ERASE_ESCAPE_SEQUENCE:
      zvt_term_set_backspace_binding (term, ZVT_ERASE_ESCAPE_SEQUENCE);
      break;
    case TERMINAL_ERASE_ASCII_DEL:
      zvt_term_set_backspace_binding (term, ZVT_ERASE_ASCII_DEL);
      break;
    }
}

void
terminal_widget_set_cursor_blinks (GtkWidget            *widget,
                                   gboolean              setting)
{
  zvt_term_set_blink (ZVT_TERM (widget), setting);
}

void
terminal_widget_set_audible_bell (GtkWidget            *widget,
                                  gboolean              setting)
{
  zvt_term_set_bell (ZVT_TERM (widget), setting);
}

void
terminal_widget_set_scroll_on_keystroke (GtkWidget            *widget,
                                         gboolean              setting)
{
  zvt_term_set_scroll_on_keystroke (ZVT_TERM (widget), setting);
}

void
terminal_widget_set_scroll_on_output (GtkWidget            *widget,
                                      gboolean              setting)
{
  zvt_term_set_scroll_on_output (ZVT_TERM (widget), setting);
}

void
terminal_widget_set_scrollback_lines (GtkWidget            *widget,
                                      int                   lines)
{
  zvt_term_set_scrollback (ZVT_TERM (widget), lines);
                           
}

static void
reset_bg (ZvtTerm *zvt,
          ZvtData *zd)
{
  int bgflags;
  
  bgflags = 0;

  if (zd->bg_scrolls)
    bgflags |= ZVT_BACKGROUND_SCROLL;  

  /* avoid enabling shading if the shading is invisibly small */
  if (zd->bg_darkness >= 0.02) 
    bgflags |= ZVT_BACKGROUND_SHADED;

  zvt_term_set_background_with_shading (zvt,
                                        zd->bg_file,
                                        zd->bg_transparent,
                                        bgflags,
                                        0, 0, 0,
                                        zd->bg_darkness * 65535);
}

void
terminal_widget_set_background_image (GtkWidget *widget,
                                      GdkPixbuf *pixbuf)
{
  /* DOES NOT WORK with ZvtTerm */
}

void
terminal_widget_set_background_image_file (GtkWidget  *widget,
                                           const char *fname)
{
  ZvtData *zd;

  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (zd->bg_file == NULL && fname == NULL)
    return;

  if (zd->bg_file && fname && strcmp (zd->bg_file, fname) == 0)
    return;
  
  g_free (zd->bg_file);
  zd->bg_file = g_strdup (fname);

  reset_bg (ZVT_TERM (widget), zd);
}

void
terminal_widget_set_background_transparent (GtkWidget            *widget,
                                            gboolean              setting)
{
  ZvtData *zd;

  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (setting != zd->bg_transparent)
    {
      zd->bg_transparent = setting;

      reset_bg (ZVT_TERM (widget), zd);
    }
}

void
terminal_widget_set_background_darkness (GtkWidget            *widget,
                                         double                factor)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (factor != zd->bg_darkness)
    {
      zd->bg_darkness = factor;

      reset_bg (ZVT_TERM (widget), zd);
    }
}

void
terminal_widget_set_background_scrolls (GtkWidget *widget,
                                        gboolean   setting)
{
  ZvtData *zd;

  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (setting != zd->bg_scrolls)
    {
      zd->bg_scrolls = setting;

      reset_bg (ZVT_TERM (widget), zd);
    }
}

static void
reset_fonts (ZvtTerm *zvt,
             ZvtData *zd)
{
  if (zd->normal_font == NULL)
    return; /* not font to set */
  
  zvt_term_set_fonts (zvt, zd->normal_font,
                      zd->allow_bold ? NULL : zd->bold_font);
}

void
terminal_widget_set_normal_gdk_font (GtkWidget            *widget,
                                     GdkFont              *font)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (font == zd->normal_font)
    return;
  
  if (font)
    gdk_font_ref (font);

  if (zd->normal_font)
    gdk_font_unref (zd->normal_font);

  zd->normal_font = font;

  reset_fonts (ZVT_TERM (widget), zd);
}

void
terminal_widget_set_bold_gdk_font (GtkWidget            *widget,
                                   GdkFont              *font)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (font == zd->bold_font)
    return;
  
  if (font)
    gdk_font_ref (font);

  if (zd->bold_font)
    gdk_font_unref (zd->bold_font);

  zd->bold_font = font;

  reset_fonts (ZVT_TERM (widget), zd);
}

void
terminal_widget_set_allow_bold (GtkWidget            *widget,
                                gboolean              setting)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  if (setting != zd->allow_bold)
    {
      zd->allow_bold = setting;

      reset_fonts (ZVT_TERM (widget), zd);
    }
}

void
terminal_widget_set_colors (GtkWidget            *widget,
                            const GdkColor       *fg,
                            const GdkColor       *bg,
                            const GdkColor       *palette_entries)
{
  gushort red[18], green[18], blue[18];
  ZvtTerm *term;
  int i;
  GdkColor c;
  
  term = ZVT_TERM (widget);
  
  i = 0;
  while (i < 16)
    {
      red[i] = palette_entries[i].red;
      green[i] = palette_entries[i].green;
      blue[i] = palette_entries[i].blue;
      ++i;
    }

  /* fg is at pos 16, bg at 17, zvt should have #defines for this crap */
  red[16] = fg->red;
  green[16] = fg->green;
  blue[16] = fg->blue;
  red[17] = bg->red;
  green[17] = bg->green;
  blue[17] = bg->blue;
  
  zvt_term_set_color_scheme (term, red, green, blue);
  c = term->colors[17];

  gdk_window_set_background (GTK_WIDGET (term)->window, &c);
  gtk_widget_queue_draw (GTK_WIDGET (term));
}

void
terminal_widget_copy_clipboard (GtkWidget            *widget)
{
  zvt_term_copy_clipboard (ZVT_TERM (widget));
}

void
terminal_widget_paste_clipboard (GtkWidget            *widget)
{
  zvt_term_paste_clipboard (ZVT_TERM (widget));
}

void
terminal_widget_reset (GtkWidget            *widget,
                       gboolean              also_clear_afterward)
{
  zvt_term_reset (ZVT_TERM (widget), also_clear_afterward);
}


void
terminal_widget_connect_title_changed (GtkWidget *widget,
                                       GCallback  callback,
                                       void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_add_callback (zd, CB_TITLE_CHANGED, callback, data);
}

void
terminal_widget_disconnect_title_changed (GtkWidget *widget,
                                          GCallback  callback,
                                          void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_remove_callback (zd, CB_TITLE_CHANGED, callback, data);
}

void
terminal_widget_connect_icon_title_changed (GtkWidget *widget,
                                            GCallback  callback,
                                            void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_add_callback (zd, CB_ICON_TITLE_CHANGED, callback, data);
}

void
terminal_widget_disconnect_icon_title_changed (GtkWidget *widget,
                                               GCallback  callback,
                                               void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_remove_callback (zd, CB_ICON_TITLE_CHANGED, callback, data);
}

void
terminal_widget_connect_child_died (GtkWidget *widget,
                                    GCallback  callback,
                                    void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_add_callback (zd, CB_CHILD_DIED, callback, data);
}

void
terminal_widget_disconnect_child_died (GtkWidget *widget,
                                       GCallback  callback,
                                       void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_remove_callback (zd, CB_CHILD_DIED, callback, data);
}

void
terminal_widget_connect_selection_changed (GtkWidget *widget,
                                           GCallback  callback,
                                           void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_add_callback (zd, CB_SELECTION_CHANGED, callback, data);
}

void
terminal_widget_disconnect_selection_changed (GtkWidget *widget,
                                              GCallback  callback,
                                              void      *data)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  zvt_data_remove_callback (zd, CB_SELECTION_CHANGED, callback, data);
}

void
terminal_widget_connect_encoding_changed      (GtkWidget *widget,
                                               GCallback  callback,
                                               void      *data)
{
  ; /* does nothing */
}

void
terminal_widget_disconnect_encoding_changed   (GtkWidget *widget,
                                               GCallback  callback,
                                               void      *data)
{
  ; /* does nothing */
}

const char*
terminal_widget_get_title (GtkWidget *widget)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  return zd->title;
}

const char*
terminal_widget_get_icon_title (GtkWidget *widget)
{
  ZvtData *zd;
  
  zd = g_object_get_data (G_OBJECT (widget), "terminal-widget-data");
  g_assert (zd);

  return zd->icon_title;
}

gboolean
terminal_widget_get_has_selection (GtkWidget *widget)
{
  return ZVT_TERM (widget)->vx->selected != FALSE;
}

GtkAdjustment*
terminal_widget_get_scroll_adjustment (GtkWidget *widget)
{
  return ZVT_TERM (widget)->adjustment;
}

/* Cut-and-paste from gspawn.c in GLib */
/* Based on execvp from GNU C Library */

static void
script_execute (const gchar *file,
                gchar      **argv,
                gchar      **envp,
                gboolean     search_path)
{
  /* Count the arguments.  */
  int argc = 0;
  while (argv[argc])
    ++argc;
  
  /* Construct an argument list for the shell.  */
  {
    gchar **new_argv;

    new_argv = g_new0 (gchar*, argc + 2); /* /bin/sh and NULL */
    
    new_argv[0] = (char *) "/bin/sh";
    new_argv[1] = (char *) file;
    while (argc > 0)
      {
	new_argv[argc + 1] = argv[argc];
	--argc;
      }

    /* Execute the shell. */
    if (envp)
      execve (new_argv[0], new_argv, envp);
    else
      execv (new_argv[0], new_argv);
    
    g_free (new_argv);
  }
}

static gchar*
my_strchrnul (const gchar *str, gchar c)
{
  gchar *p = (gchar*) str;
  while (*p && (*p != c))
    ++p;

  return p;
}

static gint
cnp_execute (const gchar *file,
             gchar      **argv,
             gchar      **envp,
             gboolean     search_path)
{
  if (*file == '\0')
    {
      /* We check the simple case first. */
      errno = ENOENT;
      return -1;
    }

  if (!search_path || strchr (file, '/') != NULL)
    {
      /* Don't search when it contains a slash. */
      if (envp)
        execve (file, argv, envp);
      else
        execv (file, argv);
      
      if (errno == ENOEXEC)
	script_execute (file, argv, envp, FALSE);
    }
  else
    {
      gboolean got_eacces = 0;
      const gchar *path, *p;
      gchar *name, *freeme;
      size_t len;
      size_t pathlen;

      path = g_getenv ("PATH");
      if (path == NULL)
	{
	  /* There is no `PATH' in the environment.  The default
	   * search path in libc is the current directory followed by
	   * the path `confstr' returns for `_CS_PATH'.
           */

          /* In GLib we put . last, for security, and don't use the
           * unportable confstr(); UNIX98 does not actually specify
           * what to search if PATH is unset. POSIX may, dunno.
           */
          
          path = "/bin:/usr/bin:.";
	}

      len = strlen (file) + 1;
      pathlen = strlen (path);
      freeme = name = g_malloc (pathlen + len + 1);
      
      /* Copy the file name at the top, including '\0'  */
      memcpy (name + pathlen + 1, file, len);
      name = name + pathlen;
      /* And add the slash before the filename  */
      *name = '/';

      p = path;
      do
	{
	  char *startp;

	  path = p;
	  p = my_strchrnul (path, ':');

	  if (p == path)
	    /* Two adjacent colons, or a colon at the beginning or the end
             * of `PATH' means to search the current directory.
             */
	    startp = name + 1;
	  else
	    startp = memcpy (name - (p - path), path, p - path);

	  /* Try to execute this name.  If it works, execv will not return.  */
          if (envp)
            execve (startp, argv, envp);
          else
            execv (startp, argv);
          
	  if (errno == ENOEXEC)
	    script_execute (startp, argv, envp, search_path);

	  switch (errno)
	    {
	    case EACCES:
	      /* Record the we got a `Permission denied' error.  If we end
               * up finding no executable we can use, we want to diagnose
               * that we did find one but were denied access.
               */
	      got_eacces = TRUE;

              /* FALL THRU */
              
	    case ENOENT:
#ifdef ESTALE
	    case ESTALE:
#endif
#ifdef ENOTDIR
	    case ENOTDIR:
#endif
	      /* Those errors indicate the file is missing or not executable
               * by us, in which case we want to just try the next path
               * directory.
               */
	      break;

	    default:
	      /* Some other error means we found an executable file, but
               * something went wrong executing it; return the error to our
               * caller.
               */
              g_free (freeme);
	      return -1;
	    }
	}
      while (*p++ != '\0');

      /* We tried every element and none of them worked.  */
      if (got_eacces)
	/* At least one failure was due to permissions, so report that
         * error.
         */
        errno = EACCES;

      g_free (freeme);
    }

  /* Return the error from the last attempt (probably ENOENT).  */
  return -1;
}

gboolean
terminal_widget_fork_command (GtkWidget   *widget,
                              gboolean     update_records,
                              const char  *path,
                              char       **argv,
                              char       **envp,
                              const char  *working_dir,
                              int         *child_pid,
                              GError     **err)
{
  ZvtTerm *term;

  term = ZVT_TERM (widget);
  
  gdk_flush ();
  errno = 0;
  switch ((*child_pid = zvt_term_forkpty (term, update_records ?
                                          (ZVT_TERM_DO_UTMP_LOG |
                                           ZVT_TERM_DO_WTMP_LOG |
                                           ZVT_TERM_DO_LASTLOG) :
                                          0)))
    {
    case -1:
      g_set_error (err,
                   G_SPAWN_ERROR,
                   G_SPAWN_ERROR_FAILED,
                   _("There was an error creating the child process for this terminal: %s"),
                   g_strerror (errno));
      return FALSE;
      break;
      
    case 0:
      {
        int open_max = sysconf (_SC_OPEN_MAX);
        int i;
        
        for (i = 3; i < open_max; i++)
          fcntl (i, F_SETFD, FD_CLOEXEC);

        if (working_dir)
          {
            if (chdir (working_dir) < 0)
              g_printerr (_("Could not set working directory to \"%s\": %s\n"),
                          working_dir, strerror (errno));
          }
        
        cnp_execute (path, argv, envp, TRUE);
        
        g_printerr (_("Could not execute command %s: %s\n"),
                    path,
                    g_strerror (errno));

        /* so the error can be seen briefly, and infinite respawn
         * loops don't totally hose the system.
         */
        sleep (3);
        
        _exit (127);
      }
      break;

    default:
      /* In the parent */
      break;
    }

  return TRUE;
}

int
terminal_widget_get_estimated_bytes_per_scrollback_line (void)
{
  /* Bytes in a line of scrollback, rough estimate, including
   * data structure to hold the line. Based on reading
   * vt_newline in vt.c in libzvt. Each char in 80 columns
   * is a 32-bit int.
   */
  return sizeof (void*) * 6 + (80.0 * 4);
}

void
terminal_widget_write_data_to_child (GtkWidget  *widget,
                                     const char *data,
                                     int         len)
{
  zvt_term_writechild (ZVT_TERM (widget), (char*) data, len);
}

void
terminal_widget_set_pango_font (GtkWidget                  *widget,
                                const PangoFontDescription *font_desc)
{
  g_return_if_fail (font_desc != NULL);
  zvt_term_set_pango_font (ZVT_TERM (widget), font_desc);
}

gboolean
terminal_widget_supports_pango_fonts (void)
{
  return TRUE;
}

const char*
terminal_widget_get_encoding (GtkWidget *widget)
{
  const char *charset = NULL;
  g_get_charset (&charset);
  return charset;
}

void
terminal_widget_set_encoding (GtkWidget  *widget,
                              const char *encoding)
{
  ; /* does nothing */
}

gboolean
terminal_widget_supports_dynamic_encoding (void)
{
  return FALSE;
}

void
terminal_widget_im_append_menuitems(GtkWidget *widget, GtkMenuShell *menushell)
{
  ; /* does nothing */
}
