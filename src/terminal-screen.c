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

#include <config.h>
#include "terminal-window.h"
#include "terminal-profile.h"
#include "terminal.h"
#include <libzvt/libzvt.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <libintl.h>
#define _(x) gettext (x)

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
};

static GList* used_ids = NULL;

enum {
  PROFILE_SET,
  TITLE_CHANGED,
  LAST_SIGNAL
};

static void terminal_screen_init        (TerminalScreen      *screen);
static void terminal_screen_class_init  (TerminalScreenClass *klass);
static void terminal_screen_finalize    (GObject             *object);
static void terminal_screen_update_on_realize (ZvtTerm        *term,
                                               TerminalScreen *screen);

static void     terminal_screen_popup_menu_keyboard (GtkWidget      *zvt,
                                                     TerminalScreen *screen);
static gboolean terminal_screen_popup_menu_mouse    (GtkWidget      *zvt,
                                                     GdkEventButton *event,
                                                     TerminalScreen *screen);


static void terminal_screen_zvt_title_changed   (GtkWidget      *zvt,
                                                 const char     *title,
                                                 VTTITLE_TYPE    type,
                                                 TerminalScreen *screen);

static void reread_profile (TerminalScreen *screen);

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

  screen->priv->zvt = zvt_term_new ();
  g_object_ref (G_OBJECT (screen->priv->zvt));
  gtk_object_sink (GTK_OBJECT (screen->priv->zvt));

  g_object_set_data (G_OBJECT (screen->priv->zvt),
                     "terminal-screen",
                     screen);
  
  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "realize",
                    G_CALLBACK (terminal_screen_update_on_realize),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "popup_menu",
                    G_CALLBACK (terminal_screen_popup_menu_keyboard),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "button_press_event",
                    G_CALLBACK (terminal_screen_popup_menu_mouse),
                    screen);

  g_signal_connect (G_OBJECT (screen->priv->zvt),
                    "title_changed",
                    G_CALLBACK (terminal_screen_zvt_title_changed),
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
set_color_scheme (ZvtTerm *term)
{
  GdkColor c;
  gushort red[18],green[18],blue[18];
  
  memcpy (red, xterm_red, sizeof (xterm_red));
  memcpy (green, xterm_green, sizeof (xterm_green));
  memcpy (blue, xterm_blue, sizeof (xterm_blue));
  
  red   [16] = 0;
  green [16] = 0;
  blue  [16] = 0;
  red   [17] = 0xffff;
  green [17] = 0xffff;
  blue  [17] = 0xdddd;
  
  zvt_term_set_color_scheme (term, red, green, blue);
  c = term->colors [17];

  gdk_window_set_background (GTK_WIDGET (term)->window, &c);
  gtk_widget_queue_draw (GTK_WIDGET (term));
}

static void
reread_profile (TerminalScreen *screen)
{
  TerminalProfile *profile;

  profile = screen->priv->profile;

  if (profile == NULL)
    return;
  
  if (GTK_WIDGET_REALIZED (screen->priv->zvt))
    terminal_screen_update_on_realize (ZVT_TERM (screen->priv->zvt),
                                       screen);

#if 0
  g_print ("Setting blink to %d\n",
           terminal_profile_get_cursor_blink (profile));
#endif
  zvt_term_set_blink (ZVT_TERM (screen->priv->zvt),
                      terminal_profile_get_cursor_blink (profile));
}

static void
profile_changed_callback (TerminalProfile *profile,
                          TerminalScreen  *screen)
{
  reread_profile (screen);
}

static void
terminal_screen_update_on_realize (ZvtTerm        *term,
                                   TerminalScreen *screen)
{
  set_color_scheme (ZVT_TERM (screen->priv->zvt));
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
                                   0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   _("There was an error creating the child process for this terminal: %s"),
                                   g_strerror (errcode));

  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);
}

void
terminal_screen_launch_child (TerminalScreen *screen)
{
  ZvtTerm *term;
  
  gdk_flush ();

  term = ZVT_TERM (screen->priv->zvt);
  
  errno = 0;
  switch (zvt_term_forkpty (term, FALSE /* FIXME update_records */))
    {
    case -1:
      show_pty_error_dialog (screen, errno);
      break;
      
    case 0:
      {
        int open_max = sysconf (_SC_OPEN_MAX);
        int i;
        char buffer[64];
        
        for (i = 3; i < open_max; i++)
          fcntl (i, F_SETFD, FD_CLOEXEC);

        /* set delayed env variables */
        g_snprintf (buffer, sizeof (buffer),
                    "WINDOWID=%ld",
                    GDK_WINDOW_XWINDOW (GTK_WIDGET (term)->window));
        putenv (buffer);

        /* FIXME from config options */
        putenv ("TERM=xterm");

        /* FIXME putenv() DISPLAY */
        
        execlp ("bash" /* FIXME configurable */, "bash", NULL);

        /* FIXME print what command failed */
        g_printerr (_("Could not execute command %s: %s\n"),
                    "bash" /* FIXME */,
                    g_strerror (errno));
        _exit (127);
      }
      break;

    default:
      /* In the parent */
      break;
    }
}

static void
not_implemented (void)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   0,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "Didn't implement this item yet, sorry");
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  
  gtk_widget_show (dialog);
}

static void
new_window_callback (GtkWidget      *menu_item,
                     TerminalScreen *screen)
{
  terminal_app_new_terminal (terminal_app_get (),
                             screen->priv->profile,
                             NULL,
                             TRUE,
                             terminal_window_get_menubar_visible (screen->priv->window));
}

static void
new_tab_callback (GtkWidget      *menu_item,
                  TerminalScreen *screen)
{
  terminal_app_new_terminal (terminal_app_get (),
                             screen->priv->profile,
                             screen->priv->window,
                             FALSE, FALSE);
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

static void
secure_keyboard_callback (GtkWidget      *menu_item,
                          TerminalScreen *screen)
{
  not_implemented ();
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

  
  append_menuitem (screen->priv->popup_menu,
                   _("Secure _Keyboard"),
                   G_CALLBACK (secure_keyboard_callback),
                   screen);  
  
  gtk_menu_popup (GTK_MENU (screen->priv->popup_menu),
                  NULL, NULL,
                  NULL, NULL, 
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());
}

static void
terminal_screen_popup_menu_keyboard (GtkWidget      *zvt,
                                     TerminalScreen *screen)
{
  terminal_screen_do_popup (screen, NULL);
}

static gboolean
terminal_screen_popup_menu_mouse (GtkWidget      *zvt,
                                  GdkEventButton *event,
                                  TerminalScreen *screen)
{
  if (event->button == 3)
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
                                   const char     *title,
                                   VTTITLE_TYPE    type,
                                   TerminalScreen *screen)
{
#if 0
  /* Um, libzvt is fucked and gives us the title/type args in the wrong order.
   * so this is commented out for now.
   */
  g_free (screen->raw_title);
  screen->raw_title = g_strdup (title);
#endif

  /* FIXME this is supposed to be a combination of the title specified
   * in the profile, plus the dynamic title appended/prepended/removed
   */
  
  screen->priv->cooked_title = g_strdup (terminal_profile_get_visible_name (screen->priv->profile));
  
  g_signal_emit (G_OBJECT (screen), signals[TITLE_CHANGED], 0);
}
