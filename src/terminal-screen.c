/* object representing one terminal window/tab with settings */

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

#include "terminal-intl.h"
#include "terminal-window.h"
#include "terminal-profile.h"
#include "terminal.h"
#include <libzvt/libzvt.h>
#include <libgnome/gnome-util.h> /* gnome_util_user_shell */
#include <libgnome/gnome-url.h> /* gnome_url_show */
#include <gdk/gdkx.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

struct _TerminalScreenPrivate
{
  GtkWidget *zvt;
  TerminalWindow *window;
  TerminalProfile *profile; /* may be NULL at times */
  guint profile_changed_id;
  guint profile_forgotten_id;
  int id;
  GtkWidget *popup_menu;
  char *raw_title;
  char *cooked_title;
  char *matched_string;
  char **override_command;
};

static GList* used_ids = NULL;

enum {
  PROFILE_SET,
  TITLE_CHANGED,
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static void terminal_screen_init        (TerminalScreen      *screen);
static void terminal_screen_class_init  (TerminalScreenClass *klass);
static void terminal_screen_finalize    (GObject             *object);
static void terminal_screen_update_on_realize (ZvtTerm        *term,
                                               TerminalScreen *screen);

static void     terminal_screen_popup_menu         (GtkWidget      *zvt,
                                                    TerminalScreen *screen);
static gboolean terminal_screen_button_press_event (GtkWidget      *zvt,
                                                    GdkEventButton *event,
                                                    TerminalScreen *screen);


static void terminal_screen_zvt_title_changed   (GtkWidget      *zvt,
                                                 VTTITLE_TYPE    type,
                                                 const char     *title,
                                                 TerminalScreen *screen);

static void terminal_screen_zvt_child_died      (GtkWidget      *zvt,
                                                 TerminalScreen *screen);

static void terminal_screen_zvt_selection_changed (GtkWidget      *zvt,
                                                   TerminalScreen *screen);

static void reread_profile (TerminalScreen *screen);

static void rebuild_title  (TerminalScreen *screen);

static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
terminal_screen_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (TerminalScreenClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) terminal_screen_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (TerminalScreen),
        0,              /* n_preallocs */
        (GInstanceInitFunc) terminal_screen_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "TerminalScreen",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
terminal_screen_init (TerminalScreen *screen)
{  
  screen->priv = g_new0 (TerminalScreenPrivate, 1);

  screen->priv->zvt = zvt_term_new_with_size (80, 24);
  g_object_ref (G_OBJECT (screen->priv->zvt));
  gtk_object_sink (GTK_OBJECT (screen->priv->zvt));

  zvt_term_set_auto_window_hint (ZVT_TERM (screen->priv->zvt), FALSE);

  zvt_term_match_add (ZVT_TERM (screen->priv->zvt),
                     "(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
                     VTATTR_UNDERLINE, "host only url");
  zvt_term_match_add (ZVT_TERM (screen->priv->zvt),
                      "(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
                      VTATTR_UNDERLINE, "full url");
  
  g_object_set_data (G_OBJECT (screen->priv->zvt),
                     "terminal-screen",
                     screen);
  
  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "realize",
                    G_CALLBACK (terminal_screen_update_on_realize),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "popup_menu",
                    G_CALLBACK (terminal_screen_popup_menu),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "button_press_event",
                    G_CALLBACK (terminal_screen_button_press_event),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "title_changed",
                    G_CALLBACK (terminal_screen_zvt_title_changed),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "child_died",
                    G_CALLBACK (terminal_screen_zvt_child_died),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "selection_changed",
                    G_CALLBACK (terminal_screen_zvt_selection_changed),
                    screen);
}

static void
terminal_screen_class_init (TerminalScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = terminal_screen_finalize;

  signals[PROFILE_SET] =
    g_signal_new ("profile_set",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, profile_set),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[TITLE_CHANGED] =
    g_signal_new ("title_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, title_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[SELECTION_CHANGED] =
    g_signal_new ("selection_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, selection_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);  
}

static void
terminal_screen_finalize (GObject *object)
{
  TerminalScreen *screen;

  screen = TERMINAL_SCREEN (object);
  
  used_ids = g_list_remove (used_ids, GINT_TO_POINTER (screen->priv->id));  
  
  terminal_screen_set_profile (screen, NULL);
  
  g_object_unref (G_OBJECT (screen->priv->zvt));

  g_free (screen->priv->raw_title);
  g_free (screen->priv->cooked_title);
  g_free (screen->priv->matched_string);
  g_strfreev (screen->priv->override_command);
  
  g_free (screen->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static int
next_unused_id (void)
{
  int i = 0;

  while (g_list_find (used_ids, GINT_TO_POINTER (i)))
    ++i;

  return i;
}

TerminalScreen*
terminal_screen_new (void)
{
  TerminalScreen *screen;

  screen = g_object_new (TERMINAL_TYPE_SCREEN, NULL);
  
  screen->priv->id = next_unused_id ();

  used_ids = g_list_prepend (used_ids, GINT_TO_POINTER (screen->priv->id));
  
  return screen;
}

int
terminal_screen_get_id (TerminalScreen *screen)
{
  return screen->priv->id;
}

TerminalWindow*
terminal_screen_get_window (TerminalScreen *screen)
{
  return screen->priv->window;
}

void
terminal_screen_set_window (TerminalScreen *screen,
                            TerminalWindow *window)
{
  screen->priv->window = window;
}

const char*
terminal_screen_get_title (TerminalScreen *screen)
{
  if (screen->priv->cooked_title)
    return screen->priv->cooked_title;
  else if (screen->priv->profile)
    return terminal_profile_get_visible_name (screen->priv->profile);
  else
    return "";
}

static void
reread_profile (TerminalScreen *screen)
{
  TerminalProfile *profile;
  ZvtTerm *term;
  
  profile = screen->priv->profile;  
  
  if (profile == NULL)
    return;

  term = ZVT_TERM (screen->priv->zvt);
  
  if (GTK_WIDGET_REALIZED (screen->priv->zvt))
    terminal_screen_update_on_realize (term, screen);

  rebuild_title (screen);

  /* FIXME For now, just hardwire the backspace and delete keys correctly
   * Needs to be wired up for people that expect brokenness later.
   */
  zvt_term_set_del_key_swap (term, TRUE);
  zvt_term_set_del_is_del (term, FALSE);
  
  zvt_term_set_blink (term,
                      terminal_profile_get_cursor_blink (profile));

  zvt_term_set_bell (term,
                     !terminal_profile_get_silent_bell (profile));

  zvt_term_set_wordclass (term,
                          terminal_profile_get_word_chars (profile));

  zvt_term_set_scroll_on_keystroke (term,
                                    terminal_profile_get_scroll_on_keystroke (profile));

  zvt_term_set_scroll_on_output (term,
                                 terminal_profile_get_scroll_on_output (profile));

  zvt_term_set_scrollback (term,
                           terminal_profile_get_scrollback_lines (profile));

  if (screen->priv->window)
    {
      terminal_window_update_scrollbar (screen->priv->window, screen);
      terminal_window_update_icon (screen->priv->window);
      terminal_window_update_geometry (screen->priv->window);
    }
}

static void
rebuild_title  (TerminalScreen *screen)
{
  TerminalTitleMode mode;

  if (screen->priv->profile)
    mode = terminal_profile_get_title_mode (screen->priv->profile);
  else
    mode = TERMINAL_TITLE_REPLACE;

  g_free (screen->priv->cooked_title);
  
  switch (mode)
    {
    case TERMINAL_TITLE_AFTER:
      screen->priv->cooked_title =
        g_strconcat (terminal_profile_get_title (screen->priv->profile),
                     (screen->priv->raw_title && *(screen->priv->raw_title)) ?
                     " - " : "",
                     screen->priv->raw_title,
                     NULL);
      break;
    case TERMINAL_TITLE_BEFORE:
      screen->priv->cooked_title =
        g_strconcat (screen->priv->raw_title ?
                     screen->priv->raw_title : "",
                     (screen->priv->raw_title && *(screen->priv->raw_title)) ?
                     " - " : "",
                     terminal_profile_get_title (screen->priv->profile),
                     NULL);
      break;
    case TERMINAL_TITLE_REPLACE:
      screen->priv->cooked_title = g_strdup (screen->priv->raw_title);
      break;
    case TERMINAL_TITLE_IGNORE:
      screen->priv->cooked_title = g_strdup (terminal_profile_get_title (screen->priv->profile));
      break;
    default:
      g_assert_not_reached ();
      break;
    }
      
  g_signal_emit (G_OBJECT (screen), signals[TITLE_CHANGED], 0);
}

static void
profile_changed_callback (TerminalProfile          *profile,
                          TerminalSettingMask       mask,
                          TerminalScreen           *screen)
{
  reread_profile (screen);
}


/* FIXME temporary hack */
static gushort xterm_red[] = { 0x0000, 0x6767, 0x0000, 0x6767, 0x0000, 0x6767, 0x0000, 0x6868,
                               0x2a2a, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff,
                               0x0,    0x0 };

static gushort xterm_green[] = { 0x0000, 0x0000, 0x6767, 0x6767, 0x0000, 0x0000, 0x6767, 0x6868,
                                 0x2a2a, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff,
                                 0x0,    0x0 };
static gushort xterm_blue[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x6767, 0x6767, 0x6767, 0x6868,
                                0x2a2a, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff,
                                0x0,    0x0 };

static void
update_color_scheme (TerminalScreen *screen)
{
  GdkColor c;
  gushort red[18],green[18],blue[18];
  ZvtTerm *term;
  GdkColor fg, bg;
  
  if (screen->priv->zvt == NULL ||
      !GTK_WIDGET_REALIZED (screen->priv->zvt))
    return;
  
  term = ZVT_TERM (screen->priv->zvt);
  
  memcpy (red, xterm_red, sizeof (xterm_red));
  memcpy (green, xterm_green, sizeof (xterm_green));
  memcpy (blue, xterm_blue, sizeof (xterm_blue));

  terminal_profile_get_color_scheme (screen->priv->profile,
                                     &fg, &bg);

  /* fg is at pos 16, bg at 17, zvt should have #defines for this crap */
  red[16] = fg.red;
  green[16] = fg.green;
  blue[16] = fg.blue;
  red[17] = bg.red;
  green[17] = bg.green;
  blue[17] = bg.blue;
  
  zvt_term_set_color_scheme (term, red, green, blue);
  c = term->colors [17];

  gdk_window_set_background (GTK_WIDGET (term)->window, &c);
  gtk_widget_queue_draw (GTK_WIDGET (term));
}

static void
terminal_screen_update_on_realize (ZvtTerm        *term,
                                   TerminalScreen *screen)
{
  TerminalProfile *profile;
  GdkFont *libzvt_workaround_hack;
  
  profile = screen->priv->profile;
  
  update_color_scheme (screen);

  /* I fixed this zvt bug but working around it for a couple weeks.
   * FIXME take this out.
   */
  libzvt_workaround_hack = term->font;
  gdk_font_ref (libzvt_workaround_hack);

  zvt_term_set_fonts (term, term->font,
                      terminal_profile_get_allow_bold (profile) ?
                      term->font : NULL);

  gdk_font_unref (libzvt_workaround_hack);
}

static void
profile_forgotten_callback (TerminalProfile *profile,
                            TerminalScreen  *screen)
{
  TerminalProfile *default_profile;

  /* Revert to the default profile */
  default_profile = terminal_profile_lookup (DEFAULT_PROFILE);
  g_assert (default_profile);

  terminal_screen_set_profile (screen, default_profile);
}

void
terminal_screen_set_profile (TerminalScreen *screen,
                             TerminalProfile *profile)
{
  if (profile == screen->priv->profile)
    return;
  
  if (screen->priv->profile_changed_id)
    {
      g_signal_handler_disconnect (G_OBJECT (screen->priv->profile),
                                   screen->priv->profile_changed_id);
      screen->priv->profile_changed_id = 0;
    }

  if (screen->priv->profile_forgotten_id)
    {
      g_signal_handler_disconnect (G_OBJECT (screen->priv->profile),
                                   screen->priv->profile_forgotten_id);
      screen->priv->profile_forgotten_id = 0;
    }
  
  if (profile)
    {
      g_object_ref (G_OBJECT (profile));
      screen->priv->profile_changed_id =
        g_signal_connect (G_OBJECT (profile),
                          "changed",
                          G_CALLBACK (profile_changed_callback),
                          screen);
      screen->priv->profile_forgotten_id =
        g_signal_connect (G_OBJECT (profile),
                          "forgotten",
                          G_CALLBACK (profile_forgotten_callback),
                          screen);
    }

#if 0
  g_print ("Switching profile from '%s' to '%s'\n",
           screen->priv->profile ?
           terminal_profile_get_visible_name (screen->priv->profile) : "none",
           profile ? terminal_profile_get_visible_name (profile) : "none");
#endif
  
  if (screen->priv->profile)
    {
      g_object_unref (G_OBJECT (screen->priv->profile));
    }

  screen->priv->profile = profile;

  reread_profile (screen);

  if (screen->priv->profile)
    g_signal_emit (G_OBJECT (screen), signals[PROFILE_SET], 0);
}

TerminalProfile*
terminal_screen_get_profile (TerminalScreen *screen)
{
  return screen->priv->profile;
}

void
terminal_screen_set_override_command (TerminalScreen *screen,
                                      char          **argv)
{
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  g_strfreev (screen->priv->override_command);
  screen->priv->override_command = g_strdupv (argv);
}

const char**
terminal_screen_get_override_command (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  return (const char**) screen->priv->override_command;
}


GtkWidget*
terminal_screen_get_widget (TerminalScreen *screen)
{
  return screen->priv->zvt;
}

static void
show_pty_error_dialog (TerminalScreen *screen,
                       int             errcode)
{
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new ((GtkWindow*)
                                   gtk_widget_get_ancestor (screen->priv->zvt,
                                                            GTK_TYPE_WINDOW),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   _("There was an error creating the child process for this terminal: %s"),
                                   g_strerror (errcode));

  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  gtk_widget_show (dialog);
}

static void
show_command_error_dialog (TerminalScreen *screen,
                           GError         *error)
{
  GtkWidget *dialog;

  g_return_if_fail (error != NULL);
  
  dialog = gtk_message_dialog_new ((GtkWindow*)
                                   gtk_widget_get_ancestor (screen->priv->zvt,
                                                            GTK_TYPE_WINDOW),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   _("There was a problem with the command for this terminal: %s"),
                                   error->message);

  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  gtk_widget_show (dialog);
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


static gboolean
get_child_command (TerminalScreen *screen,
                   char          **file_p,
                   char         ***argv_p,
                   GError        **err)
{
  /* code from gnome-terminal */
  ZvtTerm *term;
  TerminalProfile *profile;
  char  *file;
  char **argv;
  
  term = ZVT_TERM (screen->priv->zvt);
  profile = screen->priv->profile;

  file = NULL;
  argv = NULL;
  
  if (file_p)
    *file_p = NULL;
  if (argv_p)
    *argv_p = NULL;

  if (screen->priv->override_command)
    {
      file = g_strdup (screen->priv->override_command[0]);
      argv = g_strdupv (screen->priv->override_command);
    }
  else if (terminal_profile_get_use_custom_command (profile))
    {
      if (!g_shell_parse_argv (terminal_profile_get_custom_command (profile),
                               NULL, &argv,
                               err))
        return FALSE;

      file = g_strdup (argv[0]);
    }
  else
    {
      const char *only_name;
      const char *shell;

      shell = gnome_util_user_shell ();

      file = g_strdup (shell);
      
      only_name = strrchr (shell, '/');
      if (only_name != NULL)
        only_name++;
      else
        only_name = shell;

      argv = g_new (char*, 2);

      if (terminal_profile_get_login_shell (profile))
        argv[0] = g_strconcat ("-", only_name, NULL);
      else
        argv[0] = g_strdup (only_name);

      argv[1] = NULL;
    }

  if (file_p)
    *file_p = file;
  else
    g_free (file);

  if (argv_p)
    *argv_p = argv;
  else
    g_strfreev (argv);

  return TRUE;
}

extern char **environ;

static char**
get_child_environment (TerminalScreen *screen)
{
  /* code from gnome-terminal, sort of. */
  ZvtTerm *term;
  TerminalProfile *profile;
  char **p;
  int i;
  char **retval;
#define EXTRA_ENV_VARS 6
  
  term = ZVT_TERM (screen->priv->zvt);
  profile = screen->priv->profile;

  /* count env vars that are set */
  for (p = environ; *p; p++)
    ;
  
  i = p - environ;
  retval = g_new (char *, i + 1 + EXTRA_ENV_VARS);

  for (i = 0, p = environ; *p; p++)
    {
      /* Strip all these out, we'll replace some of them */
      if ((strncmp (*p, "COLUMNS=", 8) == 0) ||
          (strncmp (*p, "LINES=", 6) == 0)   ||
          (strncmp (*p, "WINDOWID=", 9) == 0) ||
          (strncmp (*p, "TERM=", 5) == 0)    ||
          (strncmp (*p, "GNOME_DESKTOP_ICON=", 19) == 0) ||
          (strncmp (*p, "COLORTERM=", 10) == 0))
        {
          /* nothing: do not copy */
        }
      else
        {
          retval[i] = g_strdup (*p);
          ++i;
        }
    }

  retval[i] = g_strdup ("COLORTERM="EXECUTABLE_NAME);
  ++i;
  retval[i] = g_strdup ("TERM=xterm"); /* FIXME configurable later? */
  ++i;
  retval[i] = g_strdup_printf ("WINDOWID=%ld",
                               GDK_WINDOW_XWINDOW (GTK_WIDGET (term)->window));
  ++i;
  
  retval[i] = NULL;
  
  return retval;
}

void
terminal_screen_launch_child (TerminalScreen *screen)
{
  ZvtTerm *term;
  TerminalProfile *profile;
  char **env;
  char  *path;
  char **argv;
  GError *err;
  
  term = ZVT_TERM (screen->priv->zvt);
  profile = screen->priv->profile;

  err = NULL;
  if (!get_child_command (screen, &path, &argv, &err))
    {
      show_command_error_dialog (screen, err);
      g_error_free (err);
      return;
    }
  
  env = get_child_environment (screen);  
  
  gdk_flush ();
  errno = 0;
  switch (zvt_term_forkpty (term,
                            terminal_profile_get_update_records (profile)))
    {
    case -1:
      show_pty_error_dialog (screen, errno);
      break;
      
    case 0:
      {
        int open_max = sysconf (_SC_OPEN_MAX);
        int i;
        
        for (i = 3; i < open_max; i++)
          fcntl (i, F_SETFD, FD_CLOEXEC);

        cnp_execute (path, argv, env, TRUE);
        
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

  g_free (path);
  g_strfreev (argv);
  g_strfreev (env);
}

void
terminal_screen_close (TerminalScreen *screen)
{
  g_return_if_fail (screen->priv->window);

  terminal_window_remove_screen (screen->priv->window, screen);
  /* screen should be finalized here, do not touch it past this point */
}

gboolean
terminal_screen_get_text_selected (TerminalScreen *screen)
{
  if (GTK_WIDGET_REALIZED (screen->priv->zvt))
    return ZVT_TERM (screen->priv->zvt)->vx->selected != FALSE;
  else
    return FALSE;
}

static void
new_window_callback (GtkWidget      *menu_item,
                     TerminalScreen *screen)
{
  terminal_app_new_terminal (terminal_app_get (),
                             screen->priv->profile,
                             NULL,
                             FALSE, FALSE, NULL);
}

static void
new_tab_callback (GtkWidget      *menu_item,
                  TerminalScreen *screen)
{
  terminal_app_new_terminal (terminal_app_get (),
                             screen->priv->profile,
                             screen->priv->window,
                             FALSE, FALSE, NULL);
}

static void
copy_callback (GtkWidget      *menu_item,
               TerminalScreen *screen)
{
  zvt_term_copy_clipboard (ZVT_TERM (screen->priv->zvt));
}

static void
paste_callback (GtkWidget      *menu_item,
                TerminalScreen *screen)
{
  zvt_term_paste_clipboard (ZVT_TERM (screen->priv->zvt));
}

static void
configuration_callback (GtkWidget      *menu_item,
                        TerminalScreen *screen)
{
  g_return_if_fail (screen->priv->profile);
  
  terminal_app_edit_profile (terminal_app_get (),
                             screen->priv->profile, 
                             screen->priv->window);
}

static void
choose_profile_callback (GtkWidget      *menu_item,
                         TerminalScreen *screen)
{
  TerminalProfile *profile;

  if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item)))
    return;
  
  profile = g_object_get_data (G_OBJECT (menu_item),
                               "profile");

  g_assert (profile);

  if (!terminal_profile_get_forgotten (profile))
    {
      terminal_screen_set_profile (screen, profile);
    }
}

static void
show_menubar_callback (GtkWidget      *menu_item,
                       TerminalScreen *screen)
{
  if (terminal_window_get_menubar_visible (screen->priv->window))
    terminal_window_set_menubar_visible (screen->priv->window,
                                         FALSE);
  else
    terminal_window_set_menubar_visible (screen->priv->window,
                                         TRUE);
}

#if 0
static void
secure_keyboard_callback (GtkWidget      *menu_item,
                          TerminalScreen *screen)
{
  not_implemented ();
}
#endif

static void
open_url (TerminalScreen *screen,
          const char     *orig_url)
{
  GError *err;
  GtkWidget *dialog;
  char *url;
  
  g_return_if_fail (orig_url != NULL);

  /* this is to handle gnome_url_show reentrancy */
  url = g_strdup (orig_url);
  g_object_ref (G_OBJECT (screen));
  
  err = NULL;
  gnome_url_show (url, &err);

  if (err)
    {
      GtkWidget *window;

      if (screen->priv->zvt)
        window = gtk_widget_get_ancestor (screen->priv->zvt,
                                          GTK_TYPE_WINDOW);
      else
        window = NULL;
      
      dialog = gtk_message_dialog_new (window ? GTK_WINDOW (window) : NULL,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Could not open the address \"%s\":\n%s"),
                                       url, err->message);
      
      g_error_free (err);
      
      g_signal_connect (G_OBJECT (dialog),
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_widget_show (dialog);
    }

  g_free (url);
  g_object_unref (G_OBJECT (screen));
}

static void
open_url_callback (GtkWidget      *menu_item,
                   TerminalScreen *screen)
{
  if (screen->priv->matched_string)
    {
      open_url (screen, screen->priv->matched_string);
      g_free (screen->priv->matched_string);
      screen->priv->matched_string = NULL;
    }
}

static void
copy_url_callback (GtkWidget      *menu_item,
                   TerminalScreen *screen)
{
  if (screen->priv->matched_string)
    {
      gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE), 
                              screen->priv->matched_string, -1);
      g_free (screen->priv->matched_string);
      screen->priv->matched_string = NULL;
    }
}


static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  TerminalScreen *screen;

  screen = g_object_get_data (G_OBJECT (attach_widget), "terminal-screen");

  g_assert (screen);

  screen->priv->popup_menu = NULL;
}

static GtkWidget*
append_menuitem (GtkWidget  *menu,
                 const char *text,
                 GCallback   callback,
                 gpointer    data)
{
  GtkWidget *menu_item;
  
  menu_item = gtk_menu_item_new_with_mnemonic (text);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         menu_item);

  g_signal_connect (G_OBJECT (menu_item),
                    "activate",
                    callback, data);

  return menu_item;
}

static GtkWidget*
append_stock_menuitem (GtkWidget  *menu,
                       const char *text,
                       GCallback   callback,
                       gpointer    data)
{
  GtkWidget *menu_item;
  
  menu_item = gtk_image_menu_item_new_from_stock (text, NULL);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         menu_item);

  if (callback)
    g_signal_connect (G_OBJECT (menu_item),
                      "activate",
                      callback, data);

  return menu_item;
}

static void
terminal_screen_do_popup (TerminalScreen *screen,
                          GdkEventButton *event)
{
  GtkWidget *profile_menu;
  GtkWidget *menu_item;
  GList *profiles;
  GList *tmp;
  GSList *group;
  
  if (screen->priv->popup_menu)
    gtk_widget_destroy (screen->priv->popup_menu);

  g_assert (screen->priv->popup_menu == NULL);
  
  screen->priv->popup_menu = gtk_menu_new ();
  
  gtk_menu_attach_to_widget (GTK_MENU (screen->priv->popup_menu),
                             GTK_WIDGET (screen->priv->zvt),
                             popup_menu_detach);

  append_menuitem (screen->priv->popup_menu,
                   _("_New window"),
                   G_CALLBACK (new_window_callback),
                   screen);

  append_menuitem (screen->priv->popup_menu,
                   _("New _tab"),
                   G_CALLBACK (new_tab_callback),
                   screen);

  menu_item = append_stock_menuitem (screen->priv->popup_menu,
                                     GTK_STOCK_COPY,
                                     G_CALLBACK (copy_callback),
                                     screen);
  if (!terminal_screen_get_text_selected (screen))
    gtk_widget_set_sensitive (menu_item, FALSE);

  menu_item = append_stock_menuitem (screen->priv->popup_menu,
                                     GTK_STOCK_PASTE,
                                     G_CALLBACK (paste_callback),
                                     screen);
  
  profile_menu = gtk_menu_new ();
  menu_item = gtk_menu_item_new_with_mnemonic (_("_Profile"));

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                             profile_menu);

  gtk_widget_show (profile_menu);
  gtk_widget_show (menu_item);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (screen->priv->popup_menu),
                         menu_item);

  group = NULL;
  profiles = terminal_profile_get_list ();
  tmp = profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile;
      
      profile = tmp->data;
      
      /* Profiles can go away while the menu is up. */
      g_object_ref (G_OBJECT (profile));

      menu_item = gtk_radio_menu_item_new_with_label (group,
                                                      terminal_profile_get_visible_name (profile));
      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (profile_menu),
                             menu_item);

      if (profile == screen->priv->profile)
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                        TRUE);
      
      g_signal_connect (G_OBJECT (menu_item),
                        "toggled",
                        G_CALLBACK (choose_profile_callback),
                        screen);
      
      g_object_set_data_full (G_OBJECT (menu_item),
                              "profile",
                              profile,
                              (GDestroyNotify) g_object_unref);
      
      tmp = tmp->next;
    }

  g_list_free (profiles);

  append_menuitem (screen->priv->popup_menu,
                   _("_Edit current profile..."),
                   G_CALLBACK (configuration_callback),
                   screen);
  
  if (terminal_window_get_menubar_visible (screen->priv->window))
    append_menuitem (screen->priv->popup_menu,
                     _("Hide _Menubar"),
                     G_CALLBACK (show_menubar_callback),
                     screen);
  else
    append_menuitem (screen->priv->popup_menu,
                     _("Show _Menubar"),
                     G_CALLBACK (show_menubar_callback),
                     screen);

#if 0
  append_menuitem (screen->priv->popup_menu,
                   _("Secure _Keyboard"),
                   G_CALLBACK (secure_keyboard_callback),
                   screen);
#endif

  if (screen->priv->matched_string)
    {
      append_menuitem (screen->priv->popup_menu,
                       _("_Open Link"),
                       G_CALLBACK (open_url_callback),
                       screen);

      append_menuitem (screen->priv->popup_menu,
                       _("_Copy Link Address"),
                       G_CALLBACK (copy_url_callback),
                       screen);
    }
      
  gtk_menu_popup (GTK_MENU (screen->priv->popup_menu),
                  NULL, NULL,
                  NULL, NULL, 
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());
}

static void
terminal_screen_popup_menu (GtkWidget      *zvt,
                            TerminalScreen *screen)
{
  terminal_screen_do_popup (screen, NULL);
}

static gboolean
terminal_screen_button_press_event (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    TerminalScreen *screen)
{
  ZvtTerm *term;
  
  term = ZVT_TERM (widget);

  g_free (screen->priv->matched_string);
  screen->priv->matched_string =
    g_strdup (zvt_term_match_check (term,
                                    event->x / term->charwidth,
                                    event->y / term->charheight,
                                    0));
  
  if (event->button == 1 || event->button == 2)
    {
      gtk_widget_grab_focus (widget);
      
      if (screen->priv->matched_string &&
          (event->state & GDK_CONTROL_MASK))
        {
          open_url (screen, screen->priv->matched_string);
          g_free (screen->priv->matched_string);
          screen->priv->matched_string = NULL;
          return TRUE; /* don't do anything else such as select with the click */
        }
      
#if 0
      /* old gnome-terminal had this code, but I'm not sure if
       * it should be here. We always return FALSE, but
       * sometimes old gnome-terminal didn't, so maybe that's
       * why it had this.
       */
      if (event->button != 3
          || (!(event->state & GDK_CONTROL_MASK) && term->vx->selected)
          || (term->vx->vt.mode & VTMODE_SEND_MOUSE))
        return FALSE;
#endif      

      return FALSE; /* pass thru the click */
    }
  else if (event->button == 3)
    {
      terminal_screen_do_popup (screen, event);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
terminal_screen_zvt_title_changed (GtkWidget      *zvt,
                                   VTTITLE_TYPE    type,
                                   const char     *title,
                                   TerminalScreen *screen)
{
  switch (type)
    {
    case VTTITLE_WINDOW:
    case VTTITLE_WINDOWICON:
      
      g_free (screen->priv->raw_title);
      screen->priv->raw_title = g_strdup (title);
      
      rebuild_title (screen);
      break;

    case VTTITLE_ICON:
      /* FIXME set WM_ICON_NAME? who cares really... */
      break;
      
    case VTTITLE_XPROPERTY:
      /* See gnome-terminal.c - this is supposed to
       * be a "XPROPNAME=VALUE" pair to set XPROPNAME on the toplevel
       * with VALUE as an XA_STRING, or if no "=VALUE" a way to delete
       * the property. Does anything use this?
       */
      /* title is in UTF-8 but came from the terminal app
       * as Latin-1 and was converted to UTF-8 by libzvt,
       * here we convert back to Latin-1 to set it as an XA_STRING
       */
      break;
  }    
}

static void
terminal_screen_zvt_child_died (GtkWidget      *zvt,
                                TerminalScreen *screen)
{
  TerminalExitAction action;

  action = TERMINAL_EXIT_CLOSE;
  if (screen->priv->profile)
    action = terminal_profile_get_exit_action (screen->priv->profile);
  
  switch (action)
    {
    case TERMINAL_EXIT_CLOSE:
      terminal_screen_close (screen);
      break;
    case TERMINAL_EXIT_RESTART:
      terminal_screen_launch_child (screen);
      break;
    }
}

static void
terminal_screen_zvt_selection_changed (GtkWidget      *zvt,
                                       TerminalScreen *screen)
{
  g_signal_emit (G_OBJECT (screen), signals[SELECTION_CHANGED], 0);
}
