/* object representing one terminal window/tab with settings */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2007, 2008 Christian Persch
 *
 * This file is part of gnome-terminal.
 *
 * Gnome-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
#include "terminal-intl.h"

/* grab gtk_style_get_font and gdk_x11_get_font_name */
#undef GTK_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED
#include <gtk/gtkstyle.h>
#include <gdk/gdkx.h>
#define GTK_DISABLE_DEPRECATED
#define GDK_DISABLE_DEPRECATED

#include <X11/extensions/Xrender.h>

#include <gdk/gdkx.h>

#include "terminal-accels.h"
#include "terminal-window.h"
#include "terminal-widget.h"
#include "terminal-profile.h"
#include "terminal-screen-container.h"
#include "terminal.h"
#include "skey-popup.h"
#include <libgnome/gnome-util.h> /* gnome_util_user_shell */
#include <libgnome/gnome-url.h> /* gnome_url_show */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MONOSPACE_FONT_DIR "/desktop/gnome/interface"
#define MONOSPACE_FONT_KEY MONOSPACE_FONT_DIR "/monospace_font_name"
#define HTTP_PROXY_DIR "/system/http_proxy"

typedef struct
{
  int tag;
  int flavor;
} TagData;

struct _TerminalScreenPrivate
{
  GtkWidget *term;
  TerminalWindow *window;
  TerminalProfile *profile; /* may be NULL at times */
  guint profile_changed_id;
  guint profile_forgotten_id;
  char *raw_title, *raw_icon_title;
  char *cooked_title, *cooked_icon_title;
  char *title_from_arg;
  gboolean icon_title_set;
  char **override_command;
  GtkWidget *title_editor;
  GtkWidget *title_entry;
  char *working_dir;
  int child_pid;
  double font_scale;
  guint recheck_working_dir_idle;
  guint gconf_connection_id;
  gboolean user_title; /* title was manually set */
  GSList *url_tags;
  GSList *skey_tags;
};

enum {
  PROFILE_SET,
  SHOW_POPUP_MENU,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TITLE
};

static void terminal_screen_init        (TerminalScreen      *screen);
static void terminal_screen_class_init  (TerminalScreenClass *klass);
static void terminal_screen_dispose     (GObject             *object);
static void terminal_screen_finalize    (GObject             *object);
static void terminal_screen_update_on_realize (VteTerminal *vte_terminal,
                                               TerminalScreen *screen);
static void terminal_screen_change_font (TerminalScreen *screen);
static gboolean terminal_screen_popup_menu         (GtkWidget      *term,
                                                    TerminalScreen *screen);
static gboolean terminal_screen_button_press_event (GtkWidget      *term,
                                                    GdkEventButton *event,
                                                    TerminalScreen *screen);

static void terminal_screen_window_title_changed      (VteTerminal *vte_terminal,
                                                       TerminalScreen *screen);
static void terminal_screen_icon_title_changed        (VteTerminal *vte_terminal,
                                                       TerminalScreen *screen);

static void terminal_screen_widget_child_died        (GtkWidget      *term,
                                                      TerminalScreen *screen);

static void terminal_screen_setup_dnd                (TerminalScreen *screen);

static void update_color_scheme                      (TerminalScreen *screen);

static gboolean cook_title  (TerminalScreen *screen, const char *raw_title, char **old_cooked_title);

static void terminal_screen_cook_title      (TerminalScreen *screen);
static void terminal_screen_cook_icon_title (TerminalScreen *screen);

static void queue_recheck_working_dir (TerminalScreen *screen);

static void monospace_font_change_notify (GConfClient *client,
					  guint        cnxn_id,
					  GConfEntry  *entry,
					  gpointer     user_data);

static void  terminal_screen_match_add         (TerminalScreen            *screen,
                                                const char           *regexp,
                                                int                   flavor);
static void  terminal_screen_skey_match_add    (TerminalScreen            *screen,
                                                const char           *regexp,
                                                int                   flavor);
static char* terminal_screen_check_match       (TerminalScreen            *screen,
                                                int                   column,
                                                int                   row,
                                                int                  *flavor);
static char* terminal_screen_skey_check_match  (TerminalScreen            *screen,
                                                int                   column,
                                                int                   row,
                                                int                  *flavor);
static void  terminal_screen_skey_match_remove (TerminalScreen            *screen);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (TerminalScreen, terminal_screen, VTE_TYPE_TERMINAL)

static void
style_set_callback (GtkWidget *widget,
                    GtkStyle  *previous_style,
                    void      *data)
{
  if (GTK_WIDGET_REALIZED (widget))
    terminal_screen_change_font (TERMINAL_SCREEN (data));
}

#ifdef DEBUG_GEOMETRY
static void
parent_size_request (GtkWidget *scrolled_window, GtkRequisition *req, GtkWidget *screen)
{
  g_print ("screen %p scrolled-window size req %d : %d\n", screen, req->width, req->height);
}
#endif

static void
parent_set_callback (TerminalScreen *screen,
                     GtkWidget      *old_parent)
{
  TerminalScreenPrivate *priv = screen->priv;

#ifdef DEBUG_GEOMETRY
  if (old_parent)
    g_signal_handlers_disconnect_by_func (old_parent, G_CALLBACK (parent_size_request), screen);
  if (GTK_WIDGET (screen)->parent)
    g_signal_connect (GTK_WIDGET (screen)->parent, "size-request", G_CALLBACK (parent_size_request), screen);
#endif

  if (GTK_WIDGET (screen)->parent == NULL)
    {
      priv->window = NULL;
    }
  else
    {
      GtkWidget *parent;
      GtkWidget *window;

      window = gtk_widget_get_toplevel (GTK_WIDGET (screen)->parent);
      if (!TERMINAL_IS_WINDOW (window))
        return; /* FIXMEchpe */

      priv->window = TERMINAL_WINDOW (window);
    }
}

static void
set_background_image_file (VteTerminal *terminal,
                           const char *fname)
{
  if (fname && fname[0])
    vte_terminal_set_background_image_file (terminal,fname);
  else
    vte_terminal_set_background_image (terminal, NULL);
}

static void
connect_monospace_font_change (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GError *err;
  GConfClient *conf;
  guint connection_id;

  conf = gconf_client_get_default ();
  
  err = NULL;
  gconf_client_add_dir (conf, MONOSPACE_FONT_DIR,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &err);

  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  MONOSPACE_FONT_DIR, err->message);
      g_error_free (err);
    }

  err = NULL;
  connection_id = gconf_client_notify_add (conf,
					   MONOSPACE_FONT_DIR,
					   monospace_font_change_notify,
					   screen, 
					   NULL, &err);

  g_object_unref (conf);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of monospace font changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }
  else
    priv->gconf_connection_id = connection_id;
}

static void
terminal_screen_sync_settings (GtkSettings *settings,
                               GParamSpec *pspec,
                               TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  gboolean blink;

  g_object_get (G_OBJECT (settings),
                "gtk-cursor-blink", &blink,
                NULL);

  vte_terminal_set_cursor_blinks (VTE_TERMINAL (screen), blink);
}

static void
terminal_screen_screen_changed (GtkWidget *widget, GdkScreen *previous_screen)
{
  GdkScreen *screen;
  GtkSettings *settings;

  screen = gtk_widget_get_screen (widget);
  if (previous_screen != NULL &&
      (screen != previous_screen || screen == NULL)) {
    settings = gtk_settings_get_for_screen (previous_screen);
    g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL,
                                          widget);
  }

  if (GTK_WIDGET_CLASS (terminal_screen_parent_class)->screen_changed) {
    GTK_WIDGET_CLASS (terminal_screen_parent_class)->screen_changed (widget, previous_screen);
  }

  if (screen == previous_screen)
    return;

  settings = gtk_widget_get_settings (widget);
  terminal_screen_sync_settings (settings, NULL, TERMINAL_SCREEN (widget));
  g_signal_connect (settings, "notify::gtk-cursor-blink",
                    G_CALLBACK (terminal_screen_sync_settings), widget);
}

#ifdef DEBUG_GEOMETRY

static void size_request (GtkWidget *widget, GtkRequisition *req)
{
  g_print ("Screen %p size-request %d : %d\n", widget, req->width, req->height);
}

static void size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  g_print ("Screen %p size-alloc   %d : %d at (%d, %d)\n", widget, allocation->width, allocation->height, allocation->x, allocation->y);
}

#endif

static void
terminal_screen_init (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv;

  priv = screen->priv = G_TYPE_INSTANCE_GET_PRIVATE (screen, TERMINAL_TYPE_SCREEN, TerminalScreenPrivate);

  vte_terminal_set_mouse_autohide (VTE_TERMINAL (screen), TRUE);

  priv->working_dir = g_get_current_dir ();
  if (priv->working_dir == NULL) /* shouldn't ever happen */
    priv->working_dir = g_strdup (g_get_home_dir ());
  priv->child_pid = -1;

  priv->recheck_working_dir_idle = 0;

  priv->term = GTK_WIDGET (screen);

  priv->font_scale = PANGO_SCALE_MEDIUM;

#define USERCHARS "-A-Za-z0-9"
#define PASSCHARS "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS "-A-Za-z0-9"
#define PATHCHARS "-A-Za-z0-9_$.+!*(),;:@&=?/~#%"
#define SCHEME    "(news:|telnet:|nntp:|file:/|https?:|ftps?:|webcal:)"
#define USER      "[" USERCHARS "]+(:["PASSCHARS "]+)?"
#define URLPATH   "/[" PATHCHARS "]*[^]'.}>) \t\r\n,\\\"]"

  terminal_screen_match_add (screen,
			     "\\<" SCHEME "//(" USER "@)?[" HOSTCHARS ".]+"
			     "(:[0-9]+)?(" URLPATH ")?\\>/?", FLAVOR_AS_IS);

  terminal_screen_match_add (screen,
			     "\\<(www|ftp)[" HOSTCHARS "]*\\.[" HOSTCHARS ".]+"
			     "(:[0-9]+)?(" URLPATH ")?\\>/?",
			     FLAVOR_DEFAULT_TO_HTTP);

  terminal_screen_match_add (screen,
			     "\\<(mailto:)?[a-z0-9][a-z0-9.-]*@[a-z0-9]"
			     "[a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+\\>",
			     FLAVOR_EMAIL);

  terminal_screen_match_add (screen,
			     "\\<news:[-A-Z\\^_a-z{|}~!\"#$%&'()*+,./0-9;:=?`]+"
			     "@[" HOSTCHARS ".]+(:[0-9]+)?\\>", FLAVOR_AS_IS);

  terminal_screen_setup_dnd (screen);

  g_object_set_data (G_OBJECT (priv->term),
                     "terminal-screen",
                     screen);

  g_signal_connect (screen,
                    "realize",
                    G_CALLBACK (terminal_screen_update_on_realize),
                    screen);

  g_signal_connect (G_OBJECT (priv->term),
                    "style_set",
                    G_CALLBACK (style_set_callback),
                    screen);

  g_signal_connect (G_OBJECT (priv->term),
                    "popup_menu",
                    G_CALLBACK (terminal_screen_popup_menu),
                    screen);

  g_signal_connect (G_OBJECT (priv->term),
                    "button_press_event",
                    G_CALLBACK (terminal_screen_button_press_event),
                    screen);

  priv->title_from_arg = NULL;
  priv->user_title = FALSE;
  
  g_signal_connect (screen, "window-title-changed",
                    G_CALLBACK (terminal_screen_window_title_changed),
                    screen);
  g_signal_connect (screen, "icon-title-changed",
                    G_CALLBACK (terminal_screen_icon_title_changed),
                    screen);

  g_signal_connect (screen, "child-exited",
                    G_CALLBACK (terminal_screen_widget_child_died),
                    screen);

  connect_monospace_font_change (screen);

  g_signal_connect (G_OBJECT (screen), "parent-set",
                    G_CALLBACK (parent_set_callback), 
                    NULL);

#ifdef DEBUG_GEOMETRY
  g_signal_connect_after (screen, "size-request", G_CALLBACK (size_request), NULL);
  g_signal_connect_after (screen, "size-allocate", G_CALLBACK (size_allocate), NULL);
#endif

  gtk_widget_show (GTK_WIDGET (screen)); /* FIXMEchpe remove this */
}

static void
terminal_screen_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);

  switch (prop_id)
    {
      case PROP_TITLE:
        g_value_set_string (value, terminal_screen_get_title (screen));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
terminal_screen_class_init (TerminalScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose = terminal_screen_dispose;
  object_class->finalize = terminal_screen_finalize;
  object_class->get_property = terminal_screen_get_property;

  widget_class->screen_changed = terminal_screen_screen_changed;

  signals[PROFILE_SET] =
    g_signal_new (I_("profile_set"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, profile_set),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_PROFILE);
  
  signals[SHOW_POPUP_MENU] =
    g_signal_new (I_("show-popup-menu"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, show_popup_menu),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));


  g_type_class_add_private (object_class, sizeof (TerminalScreenPrivate));
}

static void
terminal_screen_dispose (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;
  GtkSettings *settings;

  settings = gtk_widget_get_settings (GTK_WIDGET (screen));
  g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL,
                                        screen);

  G_OBJECT_CLASS (terminal_screen_parent_class)->dispose (object);
}

static void
terminal_screen_finalize (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;
  GConfClient *conf;

  conf = gconf_client_get_default ();
  
  if (priv->gconf_connection_id)
    gconf_client_notify_remove (conf, priv->gconf_connection_id);
  
  gconf_client_remove_dir (conf, MONOSPACE_FONT_DIR,
			   NULL);

  g_object_unref (conf);

  if (priv->title_editor)
    gtk_widget_destroy (priv->title_editor);
  
  terminal_screen_set_profile (screen, NULL);

  if (priv->recheck_working_dir_idle)
    g_source_remove (priv->recheck_working_dir_idle);
  
  g_free (priv->raw_title);
  g_free (priv->cooked_title);
  g_free (priv->title_from_arg);
  g_free (priv->raw_icon_title);
  g_free (priv->cooked_icon_title);
  g_strfreev (priv->override_command);
  g_free (priv->working_dir);

  g_slist_foreach (priv->url_tags, (GFunc) g_free, NULL);
  g_slist_free (priv->url_tags);

  g_slist_foreach (priv->skey_tags, (GFunc) g_free, NULL);
  g_slist_free (priv->skey_tags);

  G_OBJECT_CLASS (terminal_screen_parent_class)->finalize (object);
}

TerminalScreen*
terminal_screen_new (void)
{
  return g_object_new (TERMINAL_TYPE_SCREEN, NULL);
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
  TerminalScreenPrivate *priv = screen->priv;
  
  if (priv->cooked_title == NULL)
    terminal_screen_cook_title (screen);

  /* cooked_title may still be NULL */
  if (priv->cooked_title != NULL)
    return priv->cooked_title;
  else
    return "";
}

const char*
terminal_screen_get_icon_title (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  if (priv->cooked_icon_title == NULL)
    terminal_screen_cook_icon_title (screen);

  /* cooked_icon_title may still be NULL */
  if (priv->cooked_icon_title != NULL)
    return priv->cooked_icon_title;
  else
    return "";
}

gboolean
terminal_screen_get_icon_title_set (TerminalScreen *screen)
{
  return screen->priv->icon_title_set;
}

void
terminal_screen_reread_profile (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  VteTerminal *vte_terminal = VTE_TERMINAL (screen);
  TerminalProfile *profile;
  GtkWidget *term;
  TerminalBackgroundType bg_type;
  TerminalWindow *window;
  
  profile = priv->profile;  
  
  if (profile == NULL)
    return;

  term = priv->term;

  terminal_screen_cook_title (screen);
  terminal_screen_cook_icon_title (screen);
  
  if (priv->window)
    {
      /* We need these in line for the set_size in
       * update_on_realize
       */
      terminal_screen_update_scrollbar (screen);
      terminal_window_update_icon (priv->window);
      terminal_window_update_geometry (priv->window);
    }
  
  if (GTK_WIDGET_REALIZED (priv->term))
    terminal_screen_change_font (screen);

  update_color_scheme (screen);

  vte_terminal_set_audible_bell (vte_terminal,
                                 !terminal_profile_get_silent_bell (profile));
  vte_terminal_set_word_chars (vte_terminal,
                               terminal_profile_get_word_chars (profile));
  vte_terminal_set_scroll_on_keystroke (vte_terminal,
                                        terminal_profile_get_scroll_on_keystroke (profile));
  vte_terminal_set_scroll_on_output (vte_terminal,
                                     terminal_profile_get_scroll_on_output (profile));
  vte_terminal_set_scrollback_lines (vte_terminal,
                                     terminal_profile_get_scrollback_lines (profile));

  if (terminal_profile_get_use_skey (priv->profile))
    {
      terminal_screen_skey_match_add (screen,
				      "s/key [0-9]* [-A-Za-z0-9]*",
				      FLAVOR_AS_IS);

      terminal_screen_skey_match_add (screen,
				      "otp-[a-z0-9]* [0-9]* [-A-Za-z0-9]*",
				      FLAVOR_AS_IS);
    }
  else
    {
      terminal_screen_skey_match_remove (screen);
    }
  bg_type = terminal_profile_get_background_type (profile);
  
  if (bg_type == TERMINAL_BACKGROUND_IMAGE)
    {
      set_background_image_file (vte_terminal,
                                 terminal_profile_get_background_image_file (profile));
      vte_terminal_set_scroll_background (vte_terminal,
                                          terminal_profile_get_scroll_background (profile));
    }
  else
    {
      set_background_image_file (vte_terminal, NULL);
      vte_terminal_set_scroll_background (vte_terminal, FALSE);
    }

  if (bg_type == TERMINAL_BACKGROUND_IMAGE ||
      bg_type == TERMINAL_BACKGROUND_TRANSPARENT)
    {
      vte_terminal_set_background_saturation (vte_terminal,
                                              1.0 - terminal_profile_get_background_darkness (profile));
      vte_terminal_set_opacity (vte_terminal,
                                terminal_profile_get_background_darkness (profile) * 0xffff);
    }      
  else
    {
      vte_terminal_set_background_saturation (vte_terminal, 1.0); /* normal color */
      vte_terminal_set_opacity (vte_terminal, 0xffff);
    }
  
  window = terminal_screen_get_window (screen);
  /* FIXME: Don't enable this if we have a compmgr. */
  vte_terminal_set_background_transparent (vte_terminal,
                                           bg_type == TERMINAL_BACKGROUND_TRANSPARENT &&
                                           (!window || !terminal_window_uses_argb_visual (window)));

  vte_terminal_set_backspace_binding (vte_terminal,
                                      terminal_profile_get_backspace_binding (profile));
  
  vte_terminal_set_delete_binding (vte_terminal,
                                   terminal_profile_get_delete_binding (profile));
}

/**
 * cook_title:
 * @screen:
 * @raw_title: main ingredient
 * @old_cooked_title: pointer of the current cooked_title
 * 
 * Cook title according to the profile of @screen. If the result is different from
 * <literal>*old_cooked_title</literal>, store it there.
 * Returns: %TRUE or %FALSE according to whether the cooked_title was changed.
 */

static gboolean
cook_title  (TerminalScreen *screen, const char *raw_title, char **old_cooked_title)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalTitleMode mode;
  char *new_cooked_title = NULL;
  const char *static_title_piece = NULL;

  g_return_val_if_fail (old_cooked_title != NULL, FALSE);

  if (priv->profile)
    mode = terminal_profile_get_title_mode (priv->profile);
  else
    mode = TERMINAL_TITLE_REPLACE;

  /* use --title argument if one was supplied, otherwise ask the profile */
  if (priv->title_from_arg)
    static_title_piece = priv->title_from_arg;
  else
    static_title_piece = terminal_profile_get_title (priv->profile);

  switch (mode)
    {
    case TERMINAL_TITLE_AFTER:
      new_cooked_title =
        g_strconcat (static_title_piece,
                     (raw_title && *raw_title) ? " - " : "",
                     raw_title,
                     NULL);
      break;
    case TERMINAL_TITLE_BEFORE:
      new_cooked_title =
        g_strconcat (raw_title ? raw_title : "",
                     (raw_title && *raw_title) ? " - " : "",
                     static_title_piece,
                     NULL);
      break;
    case TERMINAL_TITLE_REPLACE:
      if (raw_title)
        new_cooked_title = g_strdup (raw_title);
      else
        new_cooked_title = g_strdup (static_title_piece);
      break;
    case TERMINAL_TITLE_IGNORE:
      new_cooked_title = g_strdup (static_title_piece);
    /* no default so we get missing cases statically */
    }

  if (*old_cooked_title == NULL || strcmp (new_cooked_title, *old_cooked_title) != 0)
    {
      g_free (*old_cooked_title);
      *old_cooked_title = new_cooked_title;
      return TRUE;
    }
  else
    {
      g_free (new_cooked_title);
      return FALSE;
    }
}

static void 
terminal_screen_cook_title (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  if (cook_title (screen, priv->raw_title, &priv->cooked_title))
    g_object_notify (G_OBJECT (screen), "title");
}

static void 
terminal_screen_cook_icon_title (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  cook_title (screen, priv->raw_icon_title, &priv->cooked_icon_title);
#if 0
  /* FIXMEchpe */
  if (cook_title (screen, priv->raw_icon_title, &priv->cooked_icon_title))
    g_signal_emit (G_OBJECT (screen), signals[ICON_TITLE_CHANGED], 0);
#endif
}

static void
profile_changed_callback (TerminalProfile           *profile,
                          const TerminalSettingMask *mask,
                          TerminalScreen            *screen)
{
  terminal_screen_reread_profile (screen);
}

static void
update_color_scheme (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GdkColor fg, bg;
  GdkColor palette[TERMINAL_PALETTE_SIZE];
  
  if (priv->term == NULL)
    return;

  terminal_profile_get_palette (priv->profile,
                                palette);

  if (terminal_profile_get_use_theme_colors (priv->profile))
    {
      fg = priv->term->style->text[GTK_STATE_NORMAL];
      bg = priv->term->style->base[GTK_STATE_NORMAL];
    }
  else
    {
      terminal_profile_get_color_scheme (priv->profile,
                                         &fg, &bg);
    }

  vte_terminal_set_colors (VTE_TERMINAL (screen), &fg, &bg,
                           palette, TERMINAL_PALETTE_SIZE);
  vte_terminal_set_background_tint_color (VTE_TERMINAL (screen), &bg);
}

/* Note this can be called on style_set, on prefs changing,
 * and on setting the font scale factor
 */
static void
monospace_font_change_notify (GConfClient *client,
			      guint        cnxn_id,
			      GConfEntry  *entry,
			      gpointer     user_data)
{
  TerminalScreen *screen = TERMINAL_SCREEN (user_data);
  TerminalScreenPrivate *priv = screen->priv;
  GtkWidget *widget = priv->term;
  
  if (strcmp (entry->key, MONOSPACE_FONT_KEY) == 0 &&
      GTK_WIDGET_REALIZED (widget))
    terminal_screen_change_font (screen);
}

static PangoFontDescription *
get_system_monospace_font (void)
{
  GConfClient *conf;
  char *name;
  PangoFontDescription *desc = NULL;

  conf = gconf_client_get_default ();
  name = gconf_client_get_string (conf, MONOSPACE_FONT_KEY, NULL);

  if (name)
    {
      desc = pango_font_description_from_string (name);
      g_free (name);
    }
  
  if (!desc)
    desc = pango_font_description_from_string ("Monospace 10");

  return desc;
}

void
terminal_screen_set_font (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;
  PangoFontDescription *desc;
  gboolean no_aa_without_render;

  profile = priv->profile;
  
  /* FIXMEchpe make this use a get_font_desc on TerminalProfile */
  if (terminal_profile_get_use_system_font (profile))
    desc = get_system_monospace_font ();
  else
    desc = pango_font_description_copy (terminal_profile_get_font (profile));

  pango_font_description_set_size (desc,
				   priv->font_scale *
				   pango_font_description_get_size (desc));

  no_aa_without_render = terminal_profile_get_no_aa_without_render (profile);
  if (!no_aa_without_render)
    {
      vte_terminal_set_font (VTE_TERMINAL (screen), desc);
    }
  else
    {
      Display *dpy;
      gboolean has_render;
      gint event_base, error_base;

      /* FIXME multi-head/mult-screen! */
      dpy = gdk_x11_display_get_xdisplay (gtk_widget_get_display (GTK_WIDGET (screen)));
      has_render = (XRenderQueryExtension (dpy, &event_base, &error_base) &&
		    (XRenderFindVisualFormat (dpy, DefaultVisual (dpy, DefaultScreen (dpy))) != NULL));

      if (has_render)
	vte_terminal_set_font (VTE_TERMINAL (screen), desc);
      else 
	vte_terminal_set_font_full (VTE_TERMINAL (screen),
				    desc,
				    VTE_ANTI_ALIAS_FORCE_DISABLE);
    }

  pango_font_description_free (desc);
}

static void
terminal_screen_update_on_realize (VteTerminal *vte_terminal,
                                   TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;

  profile = priv->profile;

  update_color_scheme (screen);

  /* FIXMEchpe: why do this on realize? */
  vte_terminal_set_allow_bold (vte_terminal,
                               terminal_profile_get_allow_bold (profile));
  terminal_window_set_size (priv->window, screen, TRUE);
}

static void
terminal_screen_change_font (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  terminal_screen_set_font (screen);
  terminal_screen_update_on_realize (VTE_TERMINAL (screen), screen);
}

static void
profile_forgotten_callback (TerminalProfile *profile,
                            TerminalScreen  *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *new_profile;

  /* Revert to the new term profile if any */
  new_profile = terminal_profile_get_for_new_term (NULL);

  if (new_profile)
    terminal_screen_set_profile (screen, new_profile);
  else
    g_assert_not_reached (); /* FIXME ? */
}

void
terminal_screen_set_profile (TerminalScreen *screen,
                             TerminalProfile *profile)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *old_profile;

  old_profile = priv->profile;
  if (profile == old_profile)
    return;

  if (priv->profile_changed_id)
    {
      g_signal_handler_disconnect (G_OBJECT (priv->profile),
                                   priv->profile_changed_id);
      priv->profile_changed_id = 0;
    }

  if (priv->profile_forgotten_id)
    {
      g_signal_handler_disconnect (G_OBJECT (priv->profile),
                                   priv->profile_forgotten_id);
      priv->profile_forgotten_id = 0;
    }
  
  if (profile)
    {
      g_object_ref (G_OBJECT (profile));
      priv->profile_changed_id =
        g_signal_connect (G_OBJECT (profile),
                          "changed",
                          G_CALLBACK (profile_changed_callback),
                          screen);
      priv->profile_forgotten_id =
        g_signal_connect (G_OBJECT (profile),
                          "forgotten",
                          G_CALLBACK (profile_forgotten_callback),
                          screen);
    }

#if 0
  g_print ("Switching profile from '%s' to '%s'\n",
           priv->profile ?
           terminal_profile_get_visible_name (priv->profile) : "none",
           profile ? terminal_profile_get_visible_name (profile) : "none");
#endif
  
  priv->profile = profile;

  terminal_screen_reread_profile (screen);

  if (priv->profile)
    g_signal_emit (G_OBJECT (screen), signals[PROFILE_SET], 0, old_profile);

  if (old_profile)
    g_object_unref (old_profile);
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
  TerminalScreenPrivate *priv;

  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  priv = screen->priv;
  g_strfreev (priv->override_command);
  priv->override_command = g_strdupv (argv);
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
  return screen->priv->term;
}

static void
show_command_error_dialog (TerminalScreen *screen,
                           GError         *error)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  g_assert (error != NULL);
  
  terminal_util_show_error_dialog ((GtkWindow*) gtk_widget_get_ancestor (priv->term, GTK_TYPE_WINDOW), NULL,
                                   _("There was a problem with the command for this terminal: %s"), error->message);
}

static gboolean
get_child_command (TerminalScreen *screen,
                   char          **file_p,
                   char         ***argv_p,
                   GError        **err)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;
  char  *file;
  char **argv;

  profile = priv->profile;

  file = NULL;
  argv = NULL;
  
  if (file_p)
    *file_p = NULL;
  if (argv_p)
    *argv_p = NULL;

  if (priv->override_command)
    {
      file = g_strdup (priv->override_command[0]);
      argv = g_strdupv (priv->override_command);
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
      char *shell;

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

      g_free (shell);
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
get_child_environment (GtkWidget      *term,
                       TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  gchar **p, **retval;
  gint i;
  GConfClient *conf;
#define EXTRA_ENV_VARS 8

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
          (strncmp (*p, "COLORTERM=", 10) == 0) ||
	  (strncmp (*p, "DISPLAY=", 8) == 0))
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
                               GDK_WINDOW_XWINDOW (term->window));
  ++i;

  retval[i] = g_strdup_printf ("DISPLAY=%s", 
			       gdk_display_get_name(gtk_widget_get_display(term)));
  ++i;
  
  conf = gconf_client_get_default ();

  if (!getenv ("http_proxy") &&
      gconf_client_get_bool (conf, HTTP_PROXY_DIR "/use_http_proxy", NULL))
    {
      gint port;
      GSList *ignore;
      gchar *host, *auth = NULL;

      host = gconf_client_get_string (conf, HTTP_PROXY_DIR "/host", NULL);
      port = gconf_client_get_int (conf, HTTP_PROXY_DIR "/port", NULL);
      ignore = gconf_client_get_list (conf, HTTP_PROXY_DIR "/ignore_hosts",
				      GCONF_VALUE_STRING, NULL);

      if (gconf_client_get_bool (conf, HTTP_PROXY_DIR "/use_authentication", NULL))
	{
	  gchar *user, *password;

	  user = gconf_client_get_string (conf,
					  HTTP_PROXY_DIR "/authentication_user",
					  NULL);

	  password = gconf_client_get_string (conf,
					      HTTP_PROXY_DIR
					      "/authentication_password",
					      NULL);

	  if (user != '\0')
	    auth = g_strdup_printf ("%s:%s", user, password);

	  g_free (user);
	  g_free (password);
	}

      g_object_unref (conf);

      if (port && host && host != '\0')
	{
	  if (auth)
	    retval[i] = g_strdup_printf ("http_proxy=http://%s@%s:%d/",
					 auth, host, port);
	  else
	    retval[i] = g_strdup_printf ("http_proxy=http://%s:%d/",
					 host, port);

	  ++i;
	}

      if (auth)
	g_free (auth);

      if (host)
	g_free (host);

      if (ignore)
	{
	  /* code distantly based on gconf's */
	  gchar *buf = NULL;
	  guint bufsize = 64;
	  guint cur = 0;

	  buf = g_malloc (bufsize + 3);

	  while (ignore != NULL)
	    {
	      guint len = strlen (ignore->data);

	      if ((cur + len + 2) >= bufsize) /* +2 for '\0' and comma */
		{
		  bufsize = MAX(bufsize * 2, bufsize + len + 4); 
		  buf = g_realloc (buf, bufsize + 3);
		}

	      g_assert (cur < bufsize);

	      strcpy (&buf[cur], ignore->data);
	      cur += len;

	      g_assert(cur < bufsize);

	      buf[cur] = ',';
	      ++cur;

	      g_assert(cur < bufsize);

	      ignore = g_slist_next (ignore);
	    }

	  buf[cur-1] = '\0'; /* overwrites last comma */

	  retval[i] = g_strdup_printf ("no_proxy=%s", buf);
	  g_free (buf);
	  ++i;
	}
    }

  retval[i] = NULL;

  return retval;
}

void
terminal_screen_launch_child (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;
  char **env;
  char  *path;
  char **argv;
  GError *err;
  gboolean update_records;
  
  
  profile = priv->profile;

  err = NULL;
  if (!get_child_command (screen, &path, &argv, &err))
    {
      show_command_error_dialog (screen, err);
      g_error_free (err);
      return;
    }
  
  env = get_child_environment (priv->term, screen);  

  update_records = terminal_profile_get_update_records (profile);

  priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL (screen),
                                               path,
                                               argv,
                                               env,
                                               terminal_screen_get_working_dir (screen),
                                               terminal_profile_get_login_shell (profile),
                                               update_records,
                                               update_records);

  if (priv->child_pid == -1)
    {

      terminal_util_show_error_dialog ((GtkWindow*) gtk_widget_get_ancestor (priv->term, GTK_TYPE_WINDOW), NULL,
                                       "%s", _("There was an error creating the child process for this terminal"));
    }
  
  g_free (path);
  g_strfreev (argv);
  g_strfreev (env);
}

void
terminal_screen_close (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  g_return_if_fail (priv->window);

  terminal_window_remove_screen (priv->window, screen);
  /* screen should be finalized here, do not touch it past this point */
}

TerminalScreenPopupInfo *
terminal_screen_popup_info_new (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalScreenPopupInfo *info;

  info = g_slice_new0 (TerminalScreenPopupInfo);
  info->ref_count = 1;
  info->screen = g_object_ref (screen);
  info->window = priv->window;

  return info;
}

TerminalScreenPopupInfo *
terminal_screen_popup_info_ref (TerminalScreenPopupInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);

  info->ref_count++;
  return info;
}

void
terminal_screen_popup_info_unref (TerminalScreenPopupInfo *info)
{
  g_return_if_fail (info != NULL);

  if (--info->ref_count > 0)
    return;

  g_object_unref (info->screen);
  g_free (info->string);
  g_slice_free (TerminalScreenPopupInfo, info);
}

static gboolean
terminal_screen_popup_menu (GtkWidget      *term,
                            TerminalScreen *screen)
{
  TerminalScreenPopupInfo *info;

  info = terminal_screen_popup_info_new (screen);
  info->button = 0;
  info->timestamp = gtk_get_current_event_time ();

  g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
  terminal_screen_popup_info_unref (info);

  return TRUE;
}

static gboolean
terminal_screen_button_press_event (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GtkWidget *term;
  int char_width, char_height;
  gboolean dingus_button;
  char *matched_string;
  int matched_flavor;
  guint state;

  state = event->state & gtk_accelerator_get_default_mod_mask ();

  term = priv->term;

  terminal_screen_get_cell_size (screen, &char_width, &char_height);

  matched_string =
    terminal_screen_check_match (screen,
                                 event->x / char_width,
                                 event->y / char_height,
                                 &matched_flavor);
  dingus_button = ((event->button == 1) || (event->button == 2));

  if (dingus_button &&
      (state & GDK_CONTROL_MASK) &&
      terminal_profile_get_use_skey (priv->profile))
    {
      gchar *skey_match;

      skey_match = terminal_screen_skey_check_match (screen,
						     event->x / char_width,
						     event->y / char_height,
                                                     NULL);
      if (skey_match != NULL)
	{
	  terminal_skey_do_popup (screen, GTK_WINDOW (terminal_screen_get_window (screen)), skey_match);
	  g_free (skey_match);
          g_free (matched_string);

	  return TRUE;
	}
    }

  if (dingus_button &&
      (state & GDK_CONTROL_MASK) != 0 &&
      matched_string != NULL)
    {
      gtk_widget_grab_focus (widget);
      
      terminal_util_open_url (GTK_WIDGET (priv->window), matched_string, matched_flavor);
      g_free (matched_string);

      return TRUE; /* don't do anything else such as select with the click */
    }
      
  if (event->button == 3 &&
      (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0)
    {
      TerminalScreenPopupInfo *info;

      info = terminal_screen_popup_info_new (screen);
      info->button = event->button;
      info->timestamp = event->time;
      info->string = matched_string; /* adopted */
      info->flavour = matched_flavor;

      g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
      terminal_screen_popup_info_unref (info);

      return TRUE;
    }

  /* default behavior is to let the terminal widget deal with it */
  return FALSE;
}

void
terminal_screen_set_dynamic_title (TerminalScreen *screen,
                                   const char     *title,
				   gboolean	  userset)
{
  TerminalScreenPrivate *priv = screen->priv;

  g_assert (TERMINAL_IS_SCREEN (screen));
  
  if ((priv->user_title && !userset) ||
      (priv->raw_title && title &&
       strcmp (priv->raw_title, title) == 0))
    return;

  g_free (priv->raw_title);
  priv->raw_title = g_strdup (title);
  terminal_screen_cook_title (screen);

  if (priv->title_entry &&
      priv->raw_title)
    {
      char *text;
      
      text = gtk_editable_get_chars (GTK_EDITABLE (priv->title_entry),
                                     0, -1);

      if (strcmp (text, priv->raw_title) != 0)
        gtk_entry_set_text (GTK_ENTRY (priv->title_entry),
                            priv->raw_title);
      
      g_free (text);
    }
}

void
terminal_screen_set_dynamic_icon_title (TerminalScreen *screen,
                                        const char     *icon_title,
					gboolean       userset)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  g_assert (TERMINAL_IS_SCREEN (screen));

  if ((priv->user_title && !userset) ||  
      (priv->icon_title_set &&
       priv->raw_icon_title &&
       icon_title &&
       strcmp (priv->raw_icon_title, icon_title) == 0))
    return;

  g_free (priv->raw_icon_title);
  priv->raw_icon_title = g_strdup (icon_title);
  priv->icon_title_set = TRUE;
  terminal_screen_cook_icon_title (screen);
}

void
terminal_screen_set_title (TerminalScreen *screen,
			   const char     *title)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  if (priv->title_from_arg)
    g_free (priv->title_from_arg);
  priv->title_from_arg = g_strdup (title);
}

const char*
terminal_screen_get_dynamic_title (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);
  
  return screen->priv->raw_title;
}

const char*
terminal_screen_get_dynamic_icon_title (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);
  
  return screen->priv->raw_icon_title;
}

void
terminal_screen_set_working_dir (TerminalScreen *screen,
                                 const char     *dirname)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  g_free (priv->working_dir);
  priv->working_dir = g_strdup (dirname);
}

const char*
terminal_screen_get_working_dir (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  /* Try to update the working dir using various OS-specific mechanisms */
  if (priv->child_pid >= 0)
    {
      char *file;
      char buf[PATH_MAX+1];
      int len;

      /* readlink (/proc/pid/cwd) will work on Linux */
      file = g_strdup_printf ("/proc/%d/cwd", priv->child_pid);

      /* Silently ignore failure here, since we may not be on Linux */
      len = readlink (file, buf, sizeof (buf) - 1);

      if (len > 0 && buf[0] == '/')
        {
          buf[len] = '\0';
          
          g_free (priv->working_dir);
          priv->working_dir = g_strdup (buf);
        }
      else if (len == 0)
        {
          /* On Solaris, readlink returns an empty string, but the
           * link can be used as a directory, including as a target
           * of chdir().
           */
          char *cwd;

          cwd = g_get_current_dir ();
          if (cwd != NULL)
            {
              if (chdir (file) == 0)
                {
                  g_free (priv->working_dir);
                  priv->working_dir = g_get_current_dir ();
                  chdir (cwd);
                }
              g_free (cwd);
            }
        }
      
      g_free (file);
    }
  
  return priv->working_dir;
}

static gboolean
recheck_dir (void *data)
{
  TerminalScreen *screen = data;
  TerminalScreenPrivate *priv = screen->priv;

  priv->recheck_working_dir_idle = 0;
  
  /* called just for side effect */
  terminal_screen_get_working_dir (screen);

  /* remove idle */
  return FALSE;
}

static void
queue_recheck_working_dir (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  if (priv->recheck_working_dir_idle == 0)
    {
      priv->recheck_working_dir_idle =
        g_idle_add_full (G_PRIORITY_LOW + 50,
                         recheck_dir,
                         screen,
                         NULL);
    }
}


void
terminal_screen_set_font_scale (TerminalScreen *screen,
                                double          factor)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  if (factor < TERMINAL_SCALE_MINIMUM)
    factor = TERMINAL_SCALE_MINIMUM;
  if (factor > TERMINAL_SCALE_MAXIMUM)
    factor = TERMINAL_SCALE_MAXIMUM;
  
  priv->font_scale = factor;
  
  if (priv->term &&
      GTK_WIDGET_REALIZED (priv->term))
    {
      /* Update the font */
      terminal_screen_change_font (screen);
    }
}

double
terminal_screen_get_font_scale (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), 1.0);
  
  return screen->priv->font_scale;
}

static void
terminal_screen_window_title_changed (VteTerminal *vte_terminal,
                                      TerminalScreen *screen)
{
  terminal_screen_set_dynamic_title (screen,
                                     vte_terminal_get_window_title (vte_terminal),
				     FALSE);

  queue_recheck_working_dir (screen);
}

static void
terminal_screen_icon_title_changed (VteTerminal *vte_terminal,
                                    TerminalScreen *screen)
{
  terminal_screen_set_dynamic_icon_title (screen,
                                          vte_terminal_get_icon_title (vte_terminal),
					  FALSE);  

  queue_recheck_working_dir (screen);
}


static void
terminal_screen_widget_child_died (GtkWidget      *term,
                                   TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalExitAction action;

  priv->child_pid = -1;
  
  action = TERMINAL_EXIT_CLOSE;
  if (priv->profile)
    action = terminal_profile_get_exit_action (priv->profile);
  
  switch (action)
    {
    case TERMINAL_EXIT_CLOSE:
      terminal_screen_close (screen);
      break;
    case TERMINAL_EXIT_RESTART:
      terminal_screen_launch_child (screen);
      break;
    case TERMINAL_EXIT_HOLD:
      break;
    }
}

static void
title_entry_changed (GtkWidget      *entry,
                     TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

  /* The user set the title to nothing, let's understand that as a
     request to revert to dynamically setting the title again. */
  if (G_UNLIKELY (*text == '\0'))
    priv->user_title = FALSE;
  else
    {
      priv->user_title = TRUE;
      terminal_screen_set_dynamic_title (screen, text, TRUE);
      terminal_screen_set_dynamic_icon_title (screen, text, TRUE);
    }

  g_free (text);
}

void
terminal_screen_edit_title (TerminalScreen *screen,
                            GtkWindow      *transient_parent)
{
  TerminalScreenPrivate *priv = screen->priv;
  GtkWindow *old_transient_parent;
  
  if (priv->title_editor == NULL)
    {
      GtkWidget *hbox;
      GtkWidget *entry;
      GtkWidget *label;
      
      old_transient_parent = NULL;      
      
      priv->title_editor =
        gtk_dialog_new_with_buttons (_("Set Title"),
                                     NULL,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);
      
      g_signal_connect (G_OBJECT (priv->title_editor),
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);

      g_object_add_weak_pointer (G_OBJECT (priv->title_editor),
                                 (void**) &priv->title_editor);

      gtk_window_set_resizable (GTK_WINDOW (priv->title_editor), FALSE);
      
      terminal_util_set_unique_role (GTK_WINDOW (priv->title_editor), "gnome-terminal-change-title");

      gtk_widget_set_name (priv->title_editor, "set-title-dialog");
      gtk_rc_parse_string ("widget \"set-title-dialog\" style \"hig-dialog\"\n");

      gtk_dialog_set_has_separator (GTK_DIALOG (priv->title_editor), FALSE);
      gtk_container_set_border_width (GTK_CONTAINER (priv->title_editor), 10);
      gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (priv->title_editor)->vbox), 12);

      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (priv->title_editor)->vbox), hbox, FALSE, FALSE, 0);      

      label = gtk_label_new_with_mnemonic (_("_Title:"));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      entry = gtk_entry_new ();
      gtk_entry_set_width_chars (GTK_ENTRY (entry), 30);
      gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
      
      gtk_widget_grab_focus (entry);
      gtk_dialog_set_default_response (GTK_DIALOG (priv->title_editor), GTK_RESPONSE_ACCEPT);
      
      if (priv->raw_title)
        gtk_entry_set_text (GTK_ENTRY (entry), priv->raw_title);
      
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

      g_signal_connect (G_OBJECT (entry), "changed",
                        G_CALLBACK (title_entry_changed),
                        screen);

      priv->title_entry = entry;
      g_object_add_weak_pointer (G_OBJECT (priv->title_entry),
                                 (void**) &priv->title_entry);

    }
  else
    {
      old_transient_parent =
        gtk_window_get_transient_for (GTK_WINDOW (priv->title_editor));
    }
    
  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (priv->title_editor),
                                    transient_parent);
      gtk_widget_hide (priv->title_editor); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (priv->title_editor);
  gtk_window_present (GTK_WINDOW (priv->title_editor));
}

enum
{
  TARGET_URI_LIST,
  TARGET_UTF8_STRING,
  TARGET_TEXT,
  TARGET_COMPOUND_TEXT,
  TARGET_STRING,
  TARGET_COLOR,
  TARGET_BGIMAGE,
  TARGET_RESET_BG,
  TARGET_TEXT_PLAIN,
  TARGET_MOZ_URL,
  TARGET_TAB
};

static void        
drag_data_received (TerminalScreen   *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint             time,
                    gpointer          data)
{
  TerminalScreen *screen = TERMINAL_SCREEN (data);
  TerminalScreenPrivate *priv = screen->priv;

#if 0
  {
    GList *tmp;
    char *str;
    
    tmp = context->targets;
    while (tmp != NULL)
      {
        GdkAtom atom = GPOINTER_TO_UINT (tmp->data);

        g_print ("Target: %s\n", gdk_atom_name (atom));        
        
        tmp = tmp->next;
      }
  }
#endif
  
  switch (info)
    {
    case TARGET_STRING:
    case TARGET_UTF8_STRING:
    case TARGET_COMPOUND_TEXT:
    case TARGET_TEXT:
      {
        char *str;
        
        str = gtk_selection_data_get_text (selection_data);

        /*
	 * pass UTF-8 to the terminal widget. The terminal widget
	 * should know which encoding mode it's in and be able
	 * to perform the correct conversion.
         */
        if (str && *str)
          vte_terminal_feed_child (VTE_TERMINAL (screen), str, strlen (str));
        g_free (str);
      }
      break;

    case TARGET_TEXT_PLAIN:
      {
        if (selection_data->format != 8 ||
            selection_data->length == 0)
          {
            g_printerr (_("text/plain dropped on terminal had wrong format (%d) or length (%d)\n"),
                        selection_data->format,
                        selection_data->length);
            return;
          }
        
        /* FIXME just brazenly ignoring encoding issues... */
        /* FIXMEchpe: just use the text conversion routines in gtk! */
        vte_terminal_feed_child (VTE_TERMINAL (screen),
                                 selection_data->data,
                                 selection_data->length);
      }
      break;
      
    case TARGET_COLOR:
      {
        guint16 *data = (guint16 *)selection_data->data;
        GdkColor color;
        GdkColor fg;
        TerminalProfile *profile;

        if (selection_data->format != 16 ||
            selection_data->length != 8)
          {
            g_printerr (_("Color dropped on terminal had wrong format (%d) or length (%d)\n"),
                        selection_data->format,
                        selection_data->length);
            return;
          }

        color.red = data[0];
        color.green = data[1];
        color.blue = data[2];

        profile = terminal_screen_get_profile (screen);

        if (profile)
          {
            terminal_profile_set_background_type (profile,
                                                  TERMINAL_BACKGROUND_SOLID);
            terminal_profile_set_use_theme_colors (profile, FALSE);
            
            terminal_profile_get_color_scheme (profile,
                                               &fg, NULL);
            
            terminal_profile_set_color_scheme (profile, &fg, &color);
          }
      }
      break;

    case TARGET_MOZ_URL:
      {
        GString *str;
        int i;
        const guint16 *char_data;
        int char_len;
        char *filename;
        
        /* MOZ_URL is in UCS-2 but in format 8. BROKEN!
         *
         * The data contains the URL, a \n, then the
         * title of the web page.
         */
        if (selection_data->format != 8 ||
            selection_data->length == 0 ||
            (selection_data->length % 2) != 0)
          {
            g_printerr (_("Mozilla url dropped on terminal had wrong format (%d) or length (%d)\n"),
                        selection_data->format,
                        selection_data->length);
            return;
          }

        str = g_string_new (NULL);
        
        char_len = selection_data->length / 2;
        char_data = (const guint16*) selection_data->data;
        i = 0;
        while (i < char_len)
          {
            if (char_data[i] == '\n')
              break;
            
            g_string_append_unichar (str, (gunichar) char_data[i]);
            
            ++i;
          }

        /* drop file:///, else paste in URI literally */
        filename = g_filename_from_uri (str->str, NULL, NULL);
        
        /* FIXME just brazenly ignoring encoding issues, sending
         * child some UTF-8
         */
        if (filename)
          vte_terminal_feed_child (VTE_TERMINAL (screen),
                                   filename, strlen (filename));
        else
          vte_terminal_feed_child (VTE_TERMINAL (screen), str->str, str->len);

        g_free (filename);        
        g_string_free (str, TRUE);
      }
      break;
      
    case TARGET_URI_LIST:
      {
        char *uri_list;
        char **uris;
        int i;
        
        if (selection_data->format != 8 ||
            selection_data->length == 0)
          {
            g_printerr (_("URI list dropped on terminal had wrong format (%d) or length (%d)\n"),
                        selection_data->format,
                        selection_data->length);
            return;
          }
        
        uri_list = g_strndup (selection_data->data,
                              selection_data->length);

	uris = g_strsplit (uri_list, "\r\n", 0);

        i = 0;
	while (uris && uris[i])
          {
            char *old;
            
            old = uris[i];
	    /* First, treat the dropped URI like it's a filename */
            uris[i] = g_filename_from_uri (old, NULL, NULL);
	    /* if it's NULL, that means it wasn't a filename.
	     * Pass it as a plain URI, then.
	     */
	    if (uris[i] == NULL)
	      uris[i] = old;
            else 
	      {
		/* OK, it's a file. Quote the shell characters. */
		g_free(old);
		old = uris[i];
		uris[i] = g_shell_quote(uris[i]);
		g_free(old);
	      }
            ++i;
          }

        if (uris)
          {
            char *flat;
            
            flat = g_strjoinv (" ", uris);
            vte_terminal_feed_child (VTE_TERMINAL (screen), flat, strlen (flat));
            g_free (flat);
          }

        g_strfreev (uris);
        g_free (uri_list);
      }
      break;
      
    case TARGET_BGIMAGE:
      {
        char *uri_list;
        char **uris;
        gboolean exactly_one;
        
        if (selection_data->format != 8 ||
            selection_data->length == 0)
          {
            g_printerr (_("Image filename dropped on terminal had wrong format (%d) or length (%d)\n"),
                        selection_data->format,
                        selection_data->length);
            return;
          }
        
        uri_list = g_strndup (selection_data->data,
                              selection_data->length);

	uris = g_strsplit (uri_list, "\r\n", 0);

	exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

        if (exactly_one)
          {
            TerminalProfile *profile;
            char *filename;
            GError *err;

            err = NULL;
            filename = g_filename_from_uri (uris[0],
                                            NULL,
                                            &err);

            if (err)
              {
                g_printerr (_("Error converting URI \"%s\" into filename: %s\n"),
                            uris[0], err->message);

                g_error_free (err);
              }

            profile = terminal_screen_get_profile (screen);
            
            if (filename && profile)
              {
                terminal_profile_set_background_type (profile,
                                                      TERMINAL_BACKGROUND_IMAGE);
                
                terminal_profile_set_background_image_file (profile,
                                                            filename);
              }

            g_free (filename);
          }

        g_strfreev (uris);
        g_free (uri_list);
      }
      break;

    case TARGET_RESET_BG:
      {
        TerminalProfile *profile;

        profile = terminal_screen_get_profile (screen);
        
        if (profile)
          {
            terminal_profile_set_background_type (profile,
                                                  TERMINAL_BACKGROUND_SOLID);
          }
      }
      break;

    case TARGET_TAB:
      {
        TerminalScreen *moving_screen;
        TerminalWindow *source_window;
        TerminalWindow *dest_window;
        GtkWidget *source_notebook;
        GtkWidget *dest_notebook;
        gint page_num;

        moving_screen = *(TerminalScreen**) selection_data->data;

        g_return_if_fail (TERMINAL_IS_SCREEN (moving_screen));

        source_window = terminal_screen_get_window (moving_screen);
        source_notebook = terminal_window_get_notebook (source_window);
        dest_window = terminal_screen_get_window (screen);
        dest_notebook = terminal_window_get_notebook (dest_window);
        page_num = gtk_notebook_page_num (GTK_NOTEBOOK (dest_notebook), 
                                          GTK_WIDGET (screen));

        g_object_ref (G_OBJECT (moving_screen));
        terminal_window_add_screen (dest_window, moving_screen, page_num);
        g_object_unref (G_OBJECT (moving_screen));

        gtk_drag_finish (context, TRUE, TRUE, time);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
terminal_screen_setup_dnd (TerminalScreen *screen)
{
  static GtkTargetEntry target_table[] = {
    { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, TARGET_TAB },
    { "application/x-color", 0, TARGET_COLOR },
    { "property/bgimage",    0, TARGET_BGIMAGE },
    { "x-special/gnome-reset-background", 0, TARGET_RESET_BG },
    { "text/uri-list",  0, TARGET_URI_LIST },
    { "text/x-moz-url",  0, TARGET_MOZ_URL },
    { "UTF8_STRING", 0, TARGET_UTF8_STRING },
    { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
    { "TEXT", 0, TARGET_TEXT },
    { "STRING",     0, TARGET_STRING },
    /* text/plain problematic, we don't know its encoding */
    { "text/plain", 0, TARGET_TEXT_PLAIN }
    /* add when gtk supports it perhaps */
    /* { "text/unicode", 0, TARGET_TEXT_UNICODE } */
  };
  TerminalScreenPrivate *priv = screen->priv;
  
  g_signal_connect (G_OBJECT (priv->term), "drag_data_received",
                    G_CALLBACK (drag_data_received), screen);
  
  gtk_drag_dest_set (GTK_WIDGET (priv->term),
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     target_table, G_N_ELEMENTS (target_table),
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

/* FIXMEchpe move this to TerminalWindow! */
void
terminal_screen_update_scrollbar (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;
  GtkWidget *parent;
  GtkScrolledWindow *scrolled_window;
  GtkPolicyType policy = GTK_POLICY_ALWAYS;
  GtkCornerType corner = GTK_CORNER_TOP_LEFT;

  profile = terminal_screen_get_profile (screen);

  if (profile == NULL)
    return;

  parent = GTK_WIDGET (screen)->parent;
  if (!parent)
    return;

  switch (terminal_profile_get_scrollbar_position (profile))
    {
    case TERMINAL_SCROLLBAR_HIDDEN:
      policy = GTK_POLICY_NEVER;
      break;
    case TERMINAL_SCROLLBAR_RIGHT:
      policy = GTK_POLICY_ALWAYS;
      corner = GTK_CORNER_TOP_LEFT;
      break;
    case TERMINAL_SCROLLBAR_LEFT:
      policy = GTK_POLICY_ALWAYS;
      corner = GTK_CORNER_TOP_RIGHT;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  terminal_screen_container_set_placement (parent, corner);
  terminal_screen_container_set_policy (parent, GTK_POLICY_NEVER, policy);
}

void
terminal_screen_get_size (TerminalScreen *screen,
			  int       *width_chars,
			  int       *height_chars)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);

  *width_chars = terminal->column_count;
  *height_chars = terminal->row_count;
}

void
terminal_screen_get_cell_size (TerminalScreen *screen,
			       int                  *cell_width_pixels,
			       int                  *cell_height_pixels)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);

  *cell_width_pixels = terminal->char_width;
  *cell_height_pixels = terminal->char_height;
}

static void
terminal_screen_match_add (TerminalScreen            *screen,
                           const char           *regexp,
                           int                   flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  TagData *tag_data;
  int tag;
  
  tag = vte_terminal_match_add (VTE_TERMINAL (screen), regexp);

  tag_data = g_new0 (TagData, 1);
  tag_data->tag = tag;
  tag_data->flavor = flavor;

  priv->url_tags = g_slist_append (priv->url_tags, tag_data);
}

static void
terminal_screen_skey_match_add (TerminalScreen            *screen,
                                const char           *regexp,
                                int                   flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  TagData *tag_data;
  int tag;
  
  tag = vte_terminal_match_add ( VTE_TERMINAL (screen), regexp);

  tag_data = g_new0 (TagData, 1);
  tag_data->tag = tag;
  tag_data->flavor = flavor;

  priv->skey_tags = g_slist_append (priv->skey_tags, tag_data);
}

static void
terminal_screen_skey_match_remove (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSList *tags;
  
  for (tags = priv->skey_tags; tags != NULL; tags = tags->next)
    vte_terminal_match_remove (VTE_TERMINAL (screen),
                               GPOINTER_TO_INT(((TagData*)tags->data)->tag));

  g_slist_foreach (priv->skey_tags, (GFunc) g_free, NULL);
  g_slist_free (priv->skey_tags);
  priv->skey_tags = NULL;
}

static char*
terminal_screen_check_match (TerminalScreen *screen,
			     int        column,
			     int        row,
                             int       *flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSList *tags;
  gint tag;
  char *match;

  match = vte_terminal_match_check (VTE_TERMINAL (screen), column, row, &tag);
  for (tags = priv->url_tags; tags != NULL; tags = tags->next)
    {
      TagData *tag_data = (TagData*) tags->data;
      if (GPOINTER_TO_INT(tag_data->tag) == tag)
	{
	  if (flavor)
	    *flavor = tag_data->flavor;
	  return match;
	}
    }

  g_free (match);
  return NULL;
}

static char*
terminal_screen_skey_check_match (TerminalScreen *screen,
				  int        column,
				  int        row,
                                  int       *flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSList *tags;
  gint tag;
  char *match;

  match = vte_terminal_match_check (VTE_TERMINAL (screen), column, row, &tag);
  for (tags = priv->skey_tags; tags != NULL; tags = tags->next)
    {
      TagData *tag_data = (TagData*) tags->data;
      if (GPOINTER_TO_INT(tag_data->tag) == tag)
	{
	  if (flavor)
	    *flavor = tag_data->flavor;
	  return match;
	}
    }

  g_free (match);
  return NULL;
}
