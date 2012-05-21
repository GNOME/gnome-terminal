/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2007, 2008, 2010 Christian Persch
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION (2, 90, 7)
#define GDK_KEY(symbol) GDK_KEY_##symbol
#else
#include <gdk/gdkkeysyms.h>
#define GDK_KEY(symbol) GDK_##symbol
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-marshal.h"
#include "terminal-profile.h"
#include "terminal-screen-container.h"
#include "terminal-util.h"
#include "terminal-window.h"
#include "terminal-info-bar.h"

#include "eggshell.h"

#define URL_MATCH_CURSOR  (GDK_HAND2)

typedef struct
{
  int tag;
  TerminalURLFlavour flavor;
} TagData;

struct _TerminalScreenPrivate
{
  TerminalProfile *profile; /* may be NULL at times */
  guint profile_changed_id;
  guint profile_forgotten_id;
  char *raw_title, *raw_icon_title;
  char *cooked_title, *cooked_icon_title;
  char *override_title;
  gboolean icon_title_set;
  char *initial_working_directory;
  char **initial_env;
  char **override_command;
  int child_pid;
  int pty_fd;
  double font_scale;
  gboolean user_title; /* title was manually set */
  GSList *match_tags;
  guint launch_child_source_id;
};

enum
{
  PROFILE_SET,
  SHOW_POPUP_MENU,
  MATCH_CLICKED,
  CLOSE_SCREEN,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PROFILE,
  PROP_ICON_TITLE,
  PROP_ICON_TITLE_SET,
  PROP_OVERRIDE_COMMAND,
  PROP_TITLE,
  PROP_INITIAL_ENVIRONMENT
};

enum
{
  TARGET_COLOR,
  TARGET_BGIMAGE,
  TARGET_RESET_BG,
  TARGET_MOZ_URL,
  TARGET_NETSCAPE_URL,
  TARGET_TAB
};

static void terminal_screen_dispose     (GObject             *object);
static void terminal_screen_finalize    (GObject             *object);
static void terminal_screen_drag_data_received (GtkWidget        *widget,
                                                GdkDragContext   *context,
                                                gint              x,
                                                gint              y,
                                                GtkSelectionData *selection_data,
                                                guint             info,
                                                guint             time);
static void terminal_screen_system_font_notify_cb (TerminalApp *app,
                                                   GParamSpec *pspec,
                                                   TerminalScreen *screen);
static void terminal_screen_change_font (TerminalScreen *screen);
static gboolean terminal_screen_popup_menu (GtkWidget *widget);
static gboolean terminal_screen_button_press (GtkWidget *widget,
                                              GdkEventButton *event);
static void terminal_screen_launch_child_on_idle (TerminalScreen *screen);
static void terminal_screen_child_exited  (VteTerminal *terminal);

static void terminal_screen_window_title_changed      (VteTerminal *vte_terminal,
                                                       TerminalScreen *screen);
static void terminal_screen_icon_title_changed        (VteTerminal *vte_terminal,
                                                       TerminalScreen *screen);

static void update_color_scheme                      (TerminalScreen *screen);

static gboolean terminal_screen_format_title (TerminalScreen *screen, const char *raw_title, char **old_cooked_title);

static void terminal_screen_cook_title      (TerminalScreen *screen);
static void terminal_screen_cook_icon_title (TerminalScreen *screen);

static char* terminal_screen_check_match       (TerminalScreen            *screen,
                                                int                   column,
                                                int                   row,
                                                int                  *flavor);

static guint signals[LAST_SIGNAL];

#define USERCHARS "-[:alnum:]"
#define USERCHARS_CLASS "[" USERCHARS "]"
#define PASSCHARS_CLASS "[-[:alnum:]\\Q,?;.:/!%$^*&~\"#'\\E]"
#define HOSTCHARS_CLASS "[-[:alnum:]]"
#define HOST HOSTCHARS_CLASS "+(\\." HOSTCHARS_CLASS "+)*"
#define PORT "(?:\\:[[:digit:]]{1,5})?"
#define PATHCHARS_CLASS "[-[:alnum:]\\Q_$.+!*,:;@&=?/~#%\\E]"
#define PATHTERM_CLASS "[^\\Q]'.:}>) \t\r\n,\"\\E]"
#define SCHEME "(?:news:|telnet:|nntp:|file:\\/|https?:|ftps?:|sftp:|webcal:)"
#define USERPASS USERCHARS_CLASS "+(?:" PASSCHARS_CLASS "+)?"
#define URLPATH   "(?:(/"PATHCHARS_CLASS"+(?:[(]"PATHCHARS_CLASS"*[)])*"PATHCHARS_CLASS"*)*"PATHTERM_CLASS")?"

typedef struct {
  const char *pattern;
  TerminalURLFlavour flavor;
  GRegexCompileFlags flags;
} TerminalRegexPattern;

static const TerminalRegexPattern url_regex_patterns[] = {
  { SCHEME "//(?:" USERPASS "\\@)?" HOST PORT URLPATH, FLAVOR_AS_IS, G_REGEX_CASELESS },
  { "(?:www|ftp)" HOSTCHARS_CLASS "*\\." HOST PORT URLPATH , FLAVOR_DEFAULT_TO_HTTP, G_REGEX_CASELESS  },
  { "(?:callto:|h323:|sip:)" USERCHARS_CLASS "[" USERCHARS ".]*(?:" PORT "/[a-z0-9]+)?\\@" HOST, FLAVOR_VOIP_CALL, G_REGEX_CASELESS  },
  { "(?:mailto:)?" USERCHARS_CLASS "[" USERCHARS ".]*\\@" HOSTCHARS_CLASS "+\\." HOST, FLAVOR_EMAIL, G_REGEX_CASELESS  },
  { "(?:news:|man:|info:)[[:alnum:]\\Q^_{|}~!\"#$%&'()*+,./;:=?`\\E]+", FLAVOR_AS_IS, G_REGEX_CASELESS  },
};

static GRegex **url_regexes;
static TerminalURLFlavour *url_regex_flavors;
static guint n_url_regexes;

G_DEFINE_TYPE (TerminalScreen, terminal_screen, VTE_TYPE_TERMINAL)

static char *
cwd_of_pid (int pid)
{
  static const char patterns[][18] = {
    "/proc/%d/cwd",         /* Linux */
    "/proc/%d/path/cwd",    /* Solaris >= 10 */
  };
  guint i;
  
  if (pid == -1)
    return NULL;

  /* Try to get the working directory using various OS-specific mechanisms */
  for (i = 0; i < G_N_ELEMENTS (patterns); ++i)
    {
      char cwd_file[64];
      char buf[PATH_MAX + 1];
      int len;

      g_snprintf (cwd_file, sizeof (cwd_file), patterns[i], pid);
      len = readlink (cwd_file, buf, sizeof (buf) - 1);

      if (len > 0 && buf[0] == '/')
        return g_strndup (buf, len);

      /* If that didn't do it, try this hack */
      if (len <= 0)
        {
          char *cwd, *working_dir = NULL;

          cwd = g_get_current_dir ();
          if (cwd != NULL)
            {
              /* On Solaris, readlink returns an empty string, but the
               * link can be used as a directory, including as a target
               * of chdir().
               */
              if (chdir (cwd_file) == 0)
                {
                  working_dir = g_get_current_dir ();
                  (void) chdir (cwd);
                }
              g_free (cwd);
            }

          if (working_dir)
            return working_dir;
        }
    }

  return NULL;
}

static void
free_tag_data (TagData *tagdata)
{
  g_slice_free (TagData, tagdata);
}

static void
terminal_screen_class_enable_menu_bar_accel_notify_cb (TerminalApp *app,
                                                       GParamSpec *pspec,
                                                       TerminalScreenClass *klass)
{
  static gboolean is_enabled = TRUE; /* the binding is enabled by default since GtkWidgetClass installs it */
  gboolean enable;
  GtkBindingSet *binding_set;

  g_object_get (app, TERMINAL_APP_ENABLE_MENU_BAR_ACCEL, &enable, NULL);

  /* Only remove the 'skip' entry when we have added it previously! */
  if (enable == is_enabled)
    return;

  is_enabled = enable;

  binding_set = gtk_binding_set_by_class (klass);
  if (enable)
    gtk_binding_entry_remove (binding_set, GDK_KEY (F10), GDK_SHIFT_MASK);
  else
    gtk_binding_entry_skip (binding_set, GDK_KEY (F10), GDK_SHIFT_MASK);
}

static TerminalWindow *
terminal_screen_get_window (TerminalScreen *screen)
{
  GtkWidget *widget = GTK_WIDGET (screen);
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return NULL;

  return TERMINAL_WINDOW (toplevel);
}

static gboolean
window_uses_argb_visual (TerminalScreen *screen)
{
  TerminalWindow *window;

  window = terminal_screen_get_window (screen);
  if (window == NULL || !gtk_widget_get_realized (GTK_WIDGET (window)))
    return FALSE;

  return terminal_window_uses_argb_visual (window);
}

static void
terminal_screen_realize (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;
  TerminalBackgroundType bg_type;

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->realize (widget);

  /* FIXME: Don't enable this if we have a compmgr. */
  bg_type = terminal_profile_get_property_enum (priv->profile, TERMINAL_PROFILE_BACKGROUND_TYPE);
  vte_terminal_set_background_transparent (VTE_TERMINAL (screen),
                                           bg_type == TERMINAL_BACKGROUND_TRANSPARENT &&
                                           !window_uses_argb_visual (screen));
}

#if GTK_CHECK_VERSION (2, 99, 0)

static void
terminal_screen_style_updated (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->style_updated (widget);

  update_color_scheme (screen);

  if (gtk_widget_get_realized (widget))
    terminal_screen_change_font (screen);
}

#else /* GTK 2.0 */

static void
terminal_screen_style_set (GtkWidget *widget,
                           GtkStyle *previous_style)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  void (* style_set) (GtkWidget*, GtkStyle*) = GTK_WIDGET_CLASS (terminal_screen_parent_class)->style_set;

  if (style_set)
    style_set (widget, previous_style);

  update_color_scheme (screen);

  if (gtk_widget_get_realized (widget))
    terminal_screen_change_font (screen);
}

#endif /* GTK 3.0 */

#ifdef GNOME_ENABLE_DEBUG
static void
size_request (GtkWidget *widget,
              GtkRequisition *req)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[screen %p] size-request %d : %d\n",
                         widget, req->width, req->height);
}

static void
size_allocate (GtkWidget *widget,
               GtkAllocation *allocation)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[screen %p] size-alloc   %d : %d at (%d, %d)\n",
                         widget, allocation->width, allocation->height, allocation->x, allocation->y);
}
#endif

static void
terminal_screen_init (TerminalScreen *screen)
{
  const GtkTargetEntry target_table[] = {
    { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, TARGET_TAB },
    { "application/x-color", 0, TARGET_COLOR },
    { "property/bgimage",    0, TARGET_BGIMAGE },
    { "x-special/gnome-reset-background", 0, TARGET_RESET_BG },
    { "text/x-moz-url",  0, TARGET_MOZ_URL },
    { "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL }
  };
  VteTerminal *terminal = VTE_TERMINAL (screen);
  TerminalScreenPrivate *priv;
  GtkTargetList *target_list;
  GtkTargetEntry *targets;
  int n_targets;
  guint i;

  priv = screen->priv = G_TYPE_INSTANCE_GET_PRIVATE (screen, TERMINAL_TYPE_SCREEN, TerminalScreenPrivate);

  vte_terminal_set_mouse_autohide (VTE_TERMINAL (screen), TRUE);

  priv->child_pid = -1;
  priv->pty_fd = -1;

  priv->font_scale = PANGO_SCALE_MEDIUM;

  for (i = 0; i < n_url_regexes; ++i)
    {
      TagData *tag_data;

      tag_data = g_slice_new (TagData);
      tag_data->flavor = url_regex_flavors[i];
      tag_data->tag = vte_terminal_match_add_gregex (terminal, url_regexes[i], 0);
      vte_terminal_match_set_cursor_type (terminal, tag_data->tag, URL_MATCH_CURSOR);

      priv->match_tags = g_slist_prepend (priv->match_tags, tag_data);
    }

  /* Setup DND */
  target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_target_list_add_table (target_list, target_table, G_N_ELEMENTS (target_table));

  targets = gtk_target_table_new_from_list (target_list, &n_targets);

  gtk_drag_dest_set (GTK_WIDGET (screen),
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     targets, n_targets,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  gtk_target_table_free (targets, n_targets);
  gtk_target_list_unref (target_list);

  priv->override_title = NULL;
  priv->user_title = FALSE;
  
  g_signal_connect (screen, "window-title-changed",
                    G_CALLBACK (terminal_screen_window_title_changed),
                    screen);
  g_signal_connect (screen, "icon-title-changed",
                    G_CALLBACK (terminal_screen_icon_title_changed),
                    screen);

  g_signal_connect (terminal_app_get (), "notify::system-font",
                    G_CALLBACK (terminal_screen_system_font_notify_cb), screen);

#ifdef GNOME_ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_GEOMETRY)
    {
      g_signal_connect_after (screen, "size-request", G_CALLBACK (size_request), NULL);
      g_signal_connect_after (screen, "size-allocate", G_CALLBACK (size_allocate), NULL);
    }
#endif
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
      case PROP_PROFILE:
        g_value_set_object (value, terminal_screen_get_profile (screen));
        break;
      case PROP_ICON_TITLE:
        g_value_set_string (value, terminal_screen_get_icon_title (screen));
        break;
      case PROP_ICON_TITLE_SET:
        g_value_set_boolean (value, terminal_screen_get_icon_title_set (screen));
        break;
      case PROP_OVERRIDE_COMMAND:
        g_value_set_boxed (value, terminal_screen_get_override_command (screen));
        break;
      case PROP_INITIAL_ENVIRONMENT:
        g_value_set_boxed (value, terminal_screen_get_initial_environment (screen));
        break;
      case PROP_TITLE:
        g_value_set_string (value, terminal_screen_get_title (screen));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
terminal_screen_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);

  switch (prop_id)
    {
      case PROP_PROFILE: {
        TerminalProfile *profile;

        profile = g_value_get_object (value);
        g_assert (profile != NULL);
        terminal_screen_set_profile (screen, profile);
        break;
      }
      case PROP_OVERRIDE_COMMAND:
        terminal_screen_set_override_command (screen, g_value_get_boxed (value));
        break;
      case PROP_INITIAL_ENVIRONMENT:
        terminal_screen_set_initial_environment (screen, g_value_get_boxed (value));
        break;
      case PROP_ICON_TITLE:
      case PROP_ICON_TITLE_SET:
      case PROP_TITLE:
        /* not writable */
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
  VteTerminalClass *terminal_class = VTE_TERMINAL_CLASS (klass);
  TerminalApp *app;
  guint i;

  object_class->dispose = terminal_screen_dispose;
  object_class->finalize = terminal_screen_finalize;
  object_class->get_property = terminal_screen_get_property;
  object_class->set_property = terminal_screen_set_property;

  widget_class->realize = terminal_screen_realize;
#if GTK_CHECK_VERSION (2, 99, 0)
  widget_class->style_updated = terminal_screen_style_updated;
#else
  widget_class->style_set = terminal_screen_style_set;
#endif
  widget_class->drag_data_received = terminal_screen_drag_data_received;
  widget_class->button_press_event = terminal_screen_button_press;
  widget_class->popup_menu = terminal_screen_popup_menu;

  terminal_class->child_exited = terminal_screen_child_exited;

  signals[PROFILE_SET] =
    g_signal_new (I_("profile-set"),
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

  signals[MATCH_CLICKED] =
    g_signal_new (I_("match-clicked"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, match_clicked),
                  g_signal_accumulator_true_handled, NULL,
                  _terminal_marshal_BOOLEAN__STRING_INT_UINT,
                  G_TYPE_BOOLEAN,
                  3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_UINT);
  
  signals[CLOSE_SCREEN] =
    g_signal_new (I_("close-screen"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, close_screen),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  g_object_class_install_property
    (object_class,
     PROP_PROFILE,
     g_param_spec_string ("profile", NULL, NULL,
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property
    (object_class,
     PROP_ICON_TITLE,
     g_param_spec_string ("icon-title", NULL, NULL,
                          NULL,
                          G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property
    (object_class,
     PROP_ICON_TITLE_SET,
     g_param_spec_boolean ("icon-title-set", NULL, NULL,
                           FALSE,
                           G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property
    (object_class,
     PROP_OVERRIDE_COMMAND,
     g_param_spec_boxed ("override-command", NULL, NULL,
                         G_TYPE_STRV,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property
    (object_class,
     PROP_TITLE,
     g_param_spec_string ("title", NULL, NULL,
                          NULL,
                          G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property
    (object_class,
     PROP_INITIAL_ENVIRONMENT,
     g_param_spec_boxed ("initial-environment", NULL, NULL,
                         G_TYPE_STRV,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (TerminalScreenPrivate));

  /* Precompile the regexes */
  n_url_regexes = G_N_ELEMENTS (url_regex_patterns);
  url_regexes = g_new0 (GRegex*, n_url_regexes);
  url_regex_flavors = g_new0 (TerminalURLFlavour, n_url_regexes);

  for (i = 0; i < n_url_regexes; ++i)
    {
      GError *error = NULL;

      url_regexes[i] = g_regex_new (url_regex_patterns[i].pattern,
                                    url_regex_patterns[i].flags | G_REGEX_OPTIMIZE,
                                    0, &error);
      if (error)
        {
          g_message ("%s", error->message);
          g_error_free (error);
        }

      url_regex_flavors[i] = url_regex_patterns[i].flavor;
    }

  /* This fixes bug #329827 */
  app = terminal_app_get ();
  terminal_screen_class_enable_menu_bar_accel_notify_cb (app, NULL, klass);
  g_signal_connect (app, "notify::" TERMINAL_APP_ENABLE_MENU_BAR_ACCEL,
                    G_CALLBACK (terminal_screen_class_enable_menu_bar_accel_notify_cb), klass);
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

  if (priv->launch_child_source_id != 0)
    {
      g_source_remove (priv->launch_child_source_id);
      priv->launch_child_source_id = 0;
    }

  G_OBJECT_CLASS (terminal_screen_parent_class)->dispose (object);
}

static void
terminal_screen_finalize (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;

  g_signal_handlers_disconnect_by_func (terminal_app_get (),
                                        G_CALLBACK (terminal_screen_system_font_notify_cb),
                                        screen);

  terminal_screen_set_profile (screen, NULL);

  g_free (priv->raw_title);
  g_free (priv->cooked_title);
  g_free (priv->override_title);
  g_free (priv->raw_icon_title);
  g_free (priv->cooked_icon_title);
  g_free (priv->initial_working_directory);
  g_strfreev (priv->override_command);
  g_strfreev (priv->initial_env);

  g_slist_foreach (priv->match_tags, (GFunc) free_tag_data, NULL);
  g_slist_free (priv->match_tags);

  G_OBJECT_CLASS (terminal_screen_parent_class)->finalize (object);
}

TerminalScreen *
terminal_screen_new (TerminalProfile *profile,
                     char           **override_command,
                     const char      *title,
                     const char      *working_dir,
                     char           **child_env,
                     double           zoom)
{
  TerminalScreen *screen;
  TerminalScreenPrivate *priv;

  g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), NULL);

  screen = g_object_new (TERMINAL_TYPE_SCREEN, NULL);
  priv = screen->priv;

  terminal_screen_set_profile (screen, profile);

  if (terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_CUSTOM_DEFAULT_SIZE)) {
    vte_terminal_set_size (VTE_TERMINAL (screen),
			   terminal_profile_get_property_int (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS),
			   terminal_profile_get_property_int (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS));
  }

  if (title)
    terminal_screen_set_override_title (screen, title);

  priv->initial_working_directory = g_strdup (working_dir);

  if (override_command)
    terminal_screen_set_override_command (screen, override_command);

  if (child_env)
    terminal_screen_set_initial_environment (screen, child_env);

  terminal_screen_set_font_scale (screen, zoom);
  terminal_screen_set_font (screen);

  /* Launch the child on idle */
  terminal_screen_launch_child_on_idle (screen);

  return screen;
}

const char*
terminal_screen_get_raw_title (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  if (priv->raw_title)
    return priv->raw_title;

  return "";
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

/* Supported format specifiers:
 * %S = static title
 * %D = dynamic title
 * %A = dynamic title, falling back to static title if empty
 * %- = separator, if not at start or end of string (excluding whitespace)
 */
static const char *
terminal_screen_get_title_format (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  static const char *formats[] = {
    "%A"      /* TERMINAL_TITLE_REPLACE */,
    "%D%-%S"  /* TERMINAL_TITLE_BEFORE  */,
    "%S%-%D"  /* TERMINAL_TITLE_AFTER   */,
    "%S"      /* TERMINAL_TITLE_IGNORE  */
  };

  return formats[terminal_profile_get_property_enum (priv->profile, TERMINAL_PROFILE_TITLE_MODE)];
}

/**
 * terminal_screen_format_title::
 * @screen:
 * @raw_title: main ingredient
 * @titleptr <inout>: pointer of the current title string
 * 
 * Format title according @format, and stores it in <literal>*titleptr</literal>.
 * Always ensures that *titleptr will be non-NULL.
 *
 * Returns: %TRUE iff the title changed
 */
static gboolean
terminal_screen_format_title (TerminalScreen *screen,
                              const char *raw_title,
                              char **titleptr)
{
  TerminalScreenPrivate *priv = screen->priv;
  const char *format, *arg;
  const char *static_title = NULL;
  GString *title;
  gboolean add_sep = FALSE;

  g_assert (titleptr);

  /* use --title argument if one was supplied, otherwise ask the profile */
  if (priv->override_title)
    static_title = priv->override_title;
  else
    static_title = terminal_profile_get_property_string (priv->profile, TERMINAL_PROFILE_TITLE);

  //title = g_string_sized_new (strlen (static_title) + strlen (raw_title) + 3 + 1);
  title = g_string_sized_new (128);

  format = terminal_screen_get_title_format (screen);
  for (arg = format; *arg; arg += 2)
    {
      const char *text_to_append = NULL;

      g_assert (arg[0] == '%');

      switch (arg[1])
        {
          case 'A':
            text_to_append = raw_title ? raw_title : static_title;
            break;
          case 'D':
            text_to_append = raw_title;
            break;
          case 'S':
            text_to_append = static_title;
            break;
          case '-':
            text_to_append = NULL;
            add_sep = TRUE;
            break;
          default:
            g_assert_not_reached ();
        }

      if (!text_to_append || !text_to_append[0])
        continue;

      if (add_sep && title->len > 0)
        g_string_append (title, " - ");

      g_string_append (title, text_to_append);
      add_sep = FALSE;
    }

  if (*titleptr == NULL || strcmp (title->str, *titleptr) != 0)
    {
      g_free (*titleptr);
      *titleptr = g_string_free (title, FALSE);
      return TRUE;
    }

  g_string_free (title, TRUE);
  return FALSE;
}

static void 
terminal_screen_cook_title (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  
  if (terminal_screen_format_title (screen, priv->raw_title, &priv->cooked_title))
    g_object_notify (G_OBJECT (screen), "title");
}

static void 
terminal_screen_cook_icon_title (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (terminal_screen_format_title (screen, priv->raw_icon_title, &priv->cooked_icon_title))
    g_object_notify (G_OBJECT (screen), "icon-title");
}

static void
terminal_screen_profile_notify_cb (TerminalProfile *profile,
                                   GParamSpec *pspec,
                                   TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GObject *object = G_OBJECT (screen);
  VteTerminal *vte_terminal = VTE_TERMINAL (screen);
  const char *prop_name;
  TerminalBackgroundType bg_type;
  TerminalWindow *window;

  if (pspec)
    prop_name = pspec->name;
  else
    prop_name = NULL;

  g_object_freeze_notify (object);

  if ((window = terminal_screen_get_window (screen)))
    {
      /* We need these in line for the set_size in
       * update_on_realize
       */
      terminal_window_update_geometry (window);
    }
  
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLLBAR_POSITION))
    _terminal_screen_update_scrollbar (screen);

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_ENCODING))
    {
      TerminalEncoding *encoding;

      encoding = terminal_profile_get_property_boxed (profile, TERMINAL_PROFILE_ENCODING);
      vte_terminal_set_encoding (vte_terminal, terminal_encoding_get_charset (encoding));
    }

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_TITLE_MODE) ||
      prop_name == I_(TERMINAL_PROFILE_TITLE))
    {
      terminal_screen_cook_title (screen);
      terminal_screen_cook_icon_title (screen);
    }

  if (gtk_widget_get_realized (GTK_WIDGET (screen)) &&
      (!prop_name ||
       prop_name == I_(TERMINAL_PROFILE_USE_SYSTEM_FONT) ||
       prop_name == I_(TERMINAL_PROFILE_FONT)))
    terminal_screen_change_font (screen);

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS) ||
      prop_name == I_(TERMINAL_PROFILE_FOREGROUND_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR) ||
      prop_name == I_(TERMINAL_PROFILE_PALETTE))
    update_color_scheme (screen);

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SILENT_BELL))
      vte_terminal_set_audible_bell (vte_terminal, !terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_SILENT_BELL));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_WORD_CHARS))
    vte_terminal_set_word_chars (vte_terminal,
                                 terminal_profile_get_property_string (profile, TERMINAL_PROFILE_WORD_CHARS));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE))
    vte_terminal_set_scroll_on_keystroke (vte_terminal,
                                          terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_OUTPUT))
    vte_terminal_set_scroll_on_output (vte_terminal,
                                       terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_SCROLL_ON_OUTPUT));
  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_LINES) ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_UNLIMITED))
    {
      glong lines = terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED) ?
		    -1 : terminal_profile_get_property_int (profile, TERMINAL_PROFILE_SCROLLBACK_LINES);
      vte_terminal_set_scrollback_lines (vte_terminal, lines);
    }

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_TYPE) ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_IMAGE) ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_DARKNESS) ||
      prop_name == I_(TERMINAL_PROFILE_SCROLL_BACKGROUND))
    {
      bg_type = terminal_profile_get_property_enum (profile, TERMINAL_PROFILE_BACKGROUND_TYPE);

      if (bg_type == TERMINAL_BACKGROUND_IMAGE)
        {
          vte_terminal_set_background_image (vte_terminal,
                                             terminal_profile_get_property_object (profile, TERMINAL_PROFILE_BACKGROUND_IMAGE));
          vte_terminal_set_scroll_background (vte_terminal,
                                              terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_SCROLL_BACKGROUND));
        }
      else
        {
          vte_terminal_set_background_image (vte_terminal, NULL);
          vte_terminal_set_scroll_background (vte_terminal, FALSE);
        }

      if (bg_type == TERMINAL_BACKGROUND_IMAGE ||
          bg_type == TERMINAL_BACKGROUND_TRANSPARENT)
        {
          vte_terminal_set_background_saturation (vte_terminal,
                                                  1.0 - terminal_profile_get_property_double (profile, TERMINAL_PROFILE_BACKGROUND_DARKNESS));
          vte_terminal_set_opacity (vte_terminal,
                                    0xffff * terminal_profile_get_property_double (profile, TERMINAL_PROFILE_BACKGROUND_DARKNESS));
        }
      else
        {
          vte_terminal_set_background_saturation (vte_terminal, 1.0); /* normal color */
          vte_terminal_set_opacity (vte_terminal, 0xffff);
        }
      
      /* FIXME: Don't enable this if we have a compmgr. */
      vte_terminal_set_background_transparent (vte_terminal,
                                               bg_type == TERMINAL_BACKGROUND_TRANSPARENT &&
                                               !window_uses_argb_visual (screen));
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BACKSPACE_BINDING))
  vte_terminal_set_backspace_binding (vte_terminal,
                                      terminal_profile_get_property_enum (profile, TERMINAL_PROFILE_BACKSPACE_BINDING));
  
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_DELETE_BINDING))
  vte_terminal_set_delete_binding (vte_terminal,
                                   terminal_profile_get_property_enum (profile, TERMINAL_PROFILE_DELETE_BINDING));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ALLOW_BOLD))
    vte_terminal_set_allow_bold (vte_terminal,
                                 terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_ALLOW_BOLD));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CURSOR_BLINK_MODE))
    vte_terminal_set_cursor_blink_mode (vte_terminal,
                                        terminal_profile_get_property_enum (priv->profile, TERMINAL_PROFILE_CURSOR_BLINK_MODE));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CURSOR_SHAPE))
    vte_terminal_set_cursor_shape (vte_terminal,
                                   terminal_profile_get_property_enum (priv->profile, TERMINAL_PROFILE_CURSOR_SHAPE));

  g_object_thaw_notify (object);
}

#if GTK_CHECK_VERSION (2, 99, 0)
static void
rgba_to_color (GdkColor *color,
               const GdkRGBA *rgba)
{
        color->red = rgba->red * 65535.;
        color->green = rgba->green * 65535.;
        color->blue = rgba->blue * 65535.;
        color->pixel = 0;
}
#endif

static void
update_color_scheme (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile = priv->profile;
  GdkColor colors[TERMINAL_PALETTE_SIZE];
  const GdkColor *fg_color, *bg_color, *bold_color;
  GdkColor fg, bg;
  guint n_colors;
#if GTK_CHECK_VERSION (2, 99, 0)
  GtkStyleContext *context;
  GdkRGBA rgba;

  context = gtk_widget_get_style_context (GTK_WIDGET (screen));
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &rgba);
  rgba_to_color (&fg, &rgba);

  gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &rgba);
  rgba_to_color (&bg, &rgba);

#else /* GTK 2.0 */
  GtkStyle *style;

  style = gtk_widget_get_style (GTK_WIDGET (screen));
  if (!style)
    return;

  fg = style->text[GTK_STATE_NORMAL];
  bg = style->base[GTK_STATE_NORMAL];
#endif /* GTK 3.0 */

  bold_color = NULL;

  if (!terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_THEME_COLORS))
    {
      fg_color = terminal_profile_get_property_boxed (profile, TERMINAL_PROFILE_FOREGROUND_COLOR);
      bg_color = terminal_profile_get_property_boxed (profile, TERMINAL_PROFILE_BACKGROUND_COLOR);

      if (!terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG))
	bold_color = terminal_profile_get_property_boxed (profile, TERMINAL_PROFILE_BOLD_COLOR);

      if (fg_color)
        fg = *fg_color;
      if (bg_color)
        bg = *bg_color;
    }

  n_colors = G_N_ELEMENTS (colors);
  terminal_profile_get_palette (priv->profile, colors, &n_colors);
  vte_terminal_set_colors (VTE_TERMINAL (screen), &fg, &bg,
                           colors, n_colors);
  if (bold_color)
    vte_terminal_set_color_bold (VTE_TERMINAL (screen), bold_color);
  vte_terminal_set_background_tint_color (VTE_TERMINAL (screen), &bg);
}

void
terminal_screen_set_font (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;
  PangoFontDescription *desc;

  profile = priv->profile;
  
  if (terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT))
    g_object_get (terminal_app_get (), "system-font", &desc, NULL);
  else
    g_object_get (profile, TERMINAL_PROFILE_FONT, &desc, NULL);
  g_assert (desc);

  if (pango_font_description_get_size_is_absolute (desc))
    pango_font_description_set_absolute_size (desc,
                                              priv->font_scale *
                                              pango_font_description_get_size (desc));
  else
    pango_font_description_set_size (desc,
                                     priv->font_scale *
                                     pango_font_description_get_size (desc));

  vte_terminal_set_font (VTE_TERMINAL (screen), desc);

  pango_font_description_free (desc);
}

static void
terminal_screen_system_font_notify_cb (TerminalApp *app,
                                       GParamSpec *pspec,
                                       TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen)))
    return;

  if (!terminal_profile_get_property_boolean (priv->profile, TERMINAL_PROFILE_USE_SYSTEM_FONT))
    return;

  terminal_screen_change_font (screen);
}

static void
terminal_screen_change_font (TerminalScreen *screen)
{
  TerminalWindow *window;

  terminal_screen_set_font (screen);

  window = terminal_screen_get_window (screen);
  terminal_window_set_size (window, screen);
}

static void
profile_forgotten_callback (TerminalProfile *profile,
                            TerminalScreen  *screen)
{
  TerminalProfile *new_profile;

  new_profile = terminal_app_get_profile_for_new_term (terminal_app_get ());
  g_assert (new_profile != NULL);
  terminal_screen_set_profile (screen, new_profile);
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
  
  priv->profile = profile;
  if (profile)
    {
      g_object_ref (profile);
      priv->profile_changed_id =
        g_signal_connect (profile, "notify",
                          G_CALLBACK (terminal_screen_profile_notify_cb),
                          screen);
      priv->profile_forgotten_id =
        g_signal_connect (G_OBJECT (profile),
                          "forgotten",
                          G_CALLBACK (profile_forgotten_callback),
                          screen);

      terminal_screen_profile_notify_cb (profile, NULL, screen);

      g_signal_emit (G_OBJECT (screen), signals[PROFILE_SET], 0, old_profile);
    }

  if (old_profile)
    g_object_unref (old_profile);

  g_object_notify (G_OBJECT (screen), "profile");
}

TerminalProfile*
terminal_screen_get_profile (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  g_assert (priv->profile != NULL);
  return priv->profile;
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

void
terminal_screen_set_initial_environment (TerminalScreen *screen,
                                         char          **argv)
{
  TerminalScreenPrivate *priv;

  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  priv = screen->priv;
  g_assert (priv->initial_env == NULL);
  priv->initial_env = g_strdupv (argv);
}

char**
terminal_screen_get_initial_environment (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  return screen->priv->initial_env;
}

static gboolean
get_child_command (TerminalScreen *screen,
                   const char     *shell_env,
                   GSpawnFlags    *spawn_flags_p,
                   char         ***argv_p,
                   GError        **err)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalProfile *profile;
  char **argv;

  g_assert (spawn_flags_p != NULL && argv_p != NULL);

  profile = priv->profile;

  *argv_p = argv = NULL;

  if (priv->override_command)
    {
      argv = g_strdupv (priv->override_command);

      *spawn_flags_p |= G_SPAWN_SEARCH_PATH;
    }
  else if (terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND))
    {
      if (!g_shell_parse_argv (terminal_profile_get_property_string (profile, TERMINAL_PROFILE_CUSTOM_COMMAND),
                               NULL, &argv,
                               err))
        return FALSE;

      *spawn_flags_p |= G_SPAWN_SEARCH_PATH;
    }
  else
    {
      const char *only_name;
      char *shell;
      int argc = 0;

      shell = egg_shell (shell_env);

      only_name = strrchr (shell, '/');
      if (only_name != NULL)
        only_name++;
      else
        only_name = shell;

      argv = g_new (char*, 3);

      argv[argc++] = shell;

      if (terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_LOGIN_SHELL))
        argv[argc++] = g_strconcat ("-", only_name, NULL);
      else
        argv[argc++] = g_strdup (only_name);

      argv[argc++] = NULL;

      *spawn_flags_p |= G_SPAWN_FILE_AND_ARGV_ZERO;
    }

  *argv_p = argv;

  return TRUE;
}

static char**
get_child_environment (TerminalScreen *screen,
                       char **shell)
{
  TerminalScreenPrivate *priv = screen->priv;
  GtkWidget *term = GTK_WIDGET (screen);
  GtkWidget *window;
  char **env;
  char *e, *v;
  GHashTable *env_table;
  GHashTableIter iter;
  GPtrArray *retval;
  guint i;

  window = gtk_widget_get_toplevel (term);
  g_assert (window != NULL);
  g_assert (gtk_widget_is_toplevel (window));

  env_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* First take the factory's environment */
  env = g_listenv ();
  for (i = 0; env[i]; ++i)
    g_hash_table_insert (env_table, env[i], g_strdup (g_getenv (env[i])));
  g_free (env); /* the strings themselves are now owned by the hash table */

  /* and then merge the child environment, if any */
  env = priv->initial_env;
  if (env)
    {
      for (i = 0; env[i]; ++i)
        {
          v = strchr (env[i], '=');
          if (v)
             g_hash_table_replace (env_table, g_strndup (env[i], v - env[i]), g_strdup (v + 1));
           else
             g_hash_table_replace (env_table, g_strdup (env[i]), NULL);
        }
    }

  g_hash_table_remove (env_table, "COLUMNS");
  g_hash_table_remove (env_table, "LINES");
  g_hash_table_remove (env_table, "GNOME_DESKTOP_ICON");
  
  g_hash_table_replace (env_table, g_strdup ("COLORTERM"), g_strdup (EXECUTABLE_NAME));
  
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_SCREEN (gtk_widget_get_screen (window)))
    {
      /* FIXME: moving the tab between windows, or the window between displays will make the next two invalid... */
      g_hash_table_replace (env_table, g_strdup ("WINDOWID"),
			    g_strdup_printf ("%ld",
					     GDK_WINDOW_XID (gtk_widget_get_window (window))));
      g_hash_table_replace (env_table, g_strdup ("DISPLAY"), g_strdup (gdk_display_get_name (gtk_widget_get_display (window))));
    }
#endif

  terminal_util_add_proxy_env (env_table);

  retval = g_ptr_array_sized_new (g_hash_table_size (env_table));
  g_hash_table_iter_init (&iter, env_table);
  while (g_hash_table_iter_next (&iter, (gpointer *) &e, (gpointer *) &v))
    g_ptr_array_add (retval, g_strdup_printf ("%s=%s", e, v ? v : ""));
  g_ptr_array_add (retval, NULL);

  *shell = g_strdup (g_hash_table_lookup (env_table, "SHELL"));

  g_hash_table_destroy (env_table);
  return (char **) g_ptr_array_free (retval, FALSE);
}

enum {
  RESPONSE_RELAUNCH,
  RESPONSE_EDIT_PROFILE
};

static void
info_bar_response_cb (GtkWidget *info_bar,
                      int response,
                      TerminalScreen *screen)
{
  gtk_widget_grab_focus (GTK_WIDGET (screen));

  switch (response) {
    case GTK_RESPONSE_CANCEL:
      gtk_widget_destroy (info_bar);
      g_signal_emit (screen, signals[CLOSE_SCREEN], 0);
      break;
    case RESPONSE_RELAUNCH:
      gtk_widget_destroy (info_bar);
      terminal_screen_launch_child_on_idle (screen);
      break;
    case RESPONSE_EDIT_PROFILE:
      terminal_app_edit_profile (terminal_app_get (),
                                 terminal_screen_get_profile (screen),
                                 GTK_WINDOW (terminal_screen_get_window (screen)),
                                 "custom-command-entry");
      break;
    default:
      gtk_widget_destroy (info_bar);
      break;
  }
}

static gboolean
terminal_screen_launch_child_cb (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  VteTerminal *terminal = VTE_TERMINAL (screen);
  TerminalProfile *profile;
  char **env, **argv;
  char *shell = NULL;
  GError *err = NULL;
  const char *working_dir;
  VtePtyFlags pty_flags = VTE_PTY_DEFAULT;
  GSpawnFlags spawn_flags = 0;
  GPid pid;

  priv->launch_child_source_id = 0;

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] now launching the child process\n",
                         screen);

  profile = priv->profile;

  env = get_child_environment (screen, &shell);

  if (priv->initial_working_directory)
    working_dir = priv->initial_working_directory;
  else
    working_dir = g_get_home_dir ();

  if (!terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_LOGIN_SHELL))
    pty_flags |= VTE_PTY_NO_LASTLOG;
  if (!terminal_profile_get_property_boolean (profile, TERMINAL_PROFILE_UPDATE_RECORDS))
    pty_flags |= VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP;

  if (!get_child_command (screen, shell, &spawn_flags, &argv, &err) ||
      !vte_terminal_fork_command_full (terminal,
                                       pty_flags,
                                       working_dir,
                                       argv,
                                       env,
                                       spawn_flags,
                                       NULL, NULL,
                                       &pid,
                                       &err)) {
    GtkWidget *info_bar;

    info_bar = terminal_info_bar_new (GTK_MESSAGE_ERROR,
                                      _("_Profile Preferences"), RESPONSE_EDIT_PROFILE,
                                      _("_Relaunch"), RESPONSE_RELAUNCH,
                                      NULL);
    terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                   _("There was an error creating the child process for this terminal"));
    terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                   "%s", err->message);
    g_signal_connect (info_bar, "response",
                      G_CALLBACK (info_bar_response_cb), screen);

    gtk_box_pack_start (GTK_BOX (terminal_screen_container_get_from_screen (screen)),
                        info_bar, FALSE, FALSE, 0);
    gtk_info_bar_set_default_response (GTK_INFO_BAR (info_bar), GTK_RESPONSE_CANCEL);
    gtk_widget_show (info_bar);

    g_error_free (err);
    g_strfreev (env);
    g_free (shell);

    return FALSE;
  }

  priv->child_pid = pid;
  priv->pty_fd = vte_terminal_get_pty (terminal);

  g_free (shell);
  g_strfreev (argv);
  g_strfreev (env);

  return FALSE; /* don't run again */
}

static void
terminal_screen_launch_child_on_idle (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->launch_child_source_id != 0)
    return;

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] scheduling launching the child process on idle\n",
                         screen);

  priv->launch_child_source_id = g_idle_add ((GSourceFunc) terminal_screen_launch_child_cb, screen);
}

static TerminalScreenPopupInfo *
terminal_screen_popup_info_new (TerminalScreen *screen)
{
  TerminalScreenPopupInfo *info;

  info = g_slice_new0 (TerminalScreenPopupInfo);
  info->ref_count = 1;
  info->screen = g_object_ref (screen);
  info->window = terminal_screen_get_window (screen);

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
terminal_screen_popup_menu (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPopupInfo *info;

  info = terminal_screen_popup_info_new (screen);
  info->button = 0;
  info->timestamp = gtk_get_current_event_time ();

  g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
  terminal_screen_popup_info_unref (info);

  return TRUE;
}

static gboolean
terminal_screen_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  gboolean (* button_press_event) (GtkWidget*, GdkEventButton*) =
    GTK_WIDGET_CLASS (terminal_screen_parent_class)->button_press_event;
  int char_width, char_height, row, col;
  char *matched_string;
  int matched_flavor = 0;
  guint state;
  GtkBorder *inner_border = NULL;

  state = event->state & gtk_accelerator_get_default_mod_mask ();

  terminal_screen_get_cell_size (screen, &char_width, &char_height);

  gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);
  row = (event->x - (inner_border ? inner_border->left : 0)) / char_width;
  col = (event->y - (inner_border ? inner_border->top : 0)) / char_height;
  gtk_border_free (inner_border);

  /* FIXMEchpe: add vte API to do this check by widget coords instead of grid coords */
  matched_string = terminal_screen_check_match (screen, row, col, &matched_flavor);

  if (matched_string != NULL &&
      (event->button == 1 || event->button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      gboolean handled = FALSE;

      g_signal_emit (screen, signals[MATCH_CLICKED], 0,
                     matched_string,
                     matched_flavor,
                     state,
                     &handled);
      if (handled)
        {
          g_free (matched_string);
          return TRUE; /* don't do anything else such as select with the click */
        }
    }

  if (event->button == 3 &&
      (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0)
    {
      TerminalScreenPopupInfo *info;

      info = terminal_screen_popup_info_new (screen);
      info->button = event->button;
      info->state = state;
      info->timestamp = event->time;
      info->string = matched_string; /* adopted */
      info->flavour = matched_flavor;

      g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
      terminal_screen_popup_info_unref (info);

      return TRUE;
    }

  g_free (matched_string);

  /* default behavior is to let the terminal widget deal with it */
  if (button_press_event)
    return button_press_event (widget, event);

  return FALSE;
}

static void
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
}

static void
terminal_screen_set_dynamic_icon_title (TerminalScreen *screen,
                                        const char     *icon_title,
					gboolean       userset)
{
  TerminalScreenPrivate *priv = screen->priv;
  GObject *object = G_OBJECT (screen);
  
  g_assert (TERMINAL_IS_SCREEN (screen));

  if ((priv->user_title && !userset) ||  
      (priv->icon_title_set &&
       priv->raw_icon_title &&
       icon_title &&
       strcmp (priv->raw_icon_title, icon_title) == 0))
    return;

  g_object_freeze_notify (object);

  g_free (priv->raw_icon_title);
  priv->raw_icon_title = g_strdup (icon_title);
  priv->icon_title_set = TRUE;

  g_object_notify (object, "icon-title-set");
  terminal_screen_cook_icon_title (screen);

  g_object_thaw_notify (object);
}

void
terminal_screen_set_override_title (TerminalScreen *screen,
                                    const char     *title)
{
  TerminalScreenPrivate *priv = screen->priv;
  char *old_title;

  old_title = priv->override_title;
  priv->override_title = g_strdup (title);
  g_free (old_title);

  terminal_screen_set_dynamic_title (screen, title, FALSE);
  terminal_screen_set_dynamic_icon_title (screen, title, FALSE);
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

/**
 * terminal_screen_get_current_dir:
 * @screen:
 *
 * Tries to determine the current working directory of the foreground process
 * in @screen's PTY, falling back to the current working directory of the
 * primary child.
 * 
 * Returns: a newly allocated string containing the current working directory,
 *   or %NULL on failure
 */
char*
terminal_screen_get_current_dir (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  char *cwd;

  if (priv->pty_fd != -1) {
#if 0
    /* Get the foreground process ID */
    cwd = cwd_of_pid (tcgetpgrp (priv->pty_fd));
    if (cwd != NULL)
      return cwd;
#endif

    /* If that didn't work, try falling back to the primary child. See bug #575184. */
    cwd = cwd_of_pid (priv->child_pid);
    if (cwd != NULL)
      return cwd;
  }

  return NULL;
}

/**
 * terminal_screen_get_current_dir_with_fallback:
 * @screen:
 *
 * Like terminal_screen_get_current_dir(), but falls back to returning
 * @screen's initial working directory, with a further fallback to the
 * user's home directory.
 * 
 * Returns: a newly allocated string containing the current working directory,
 *   or %NULL on failure
 */
char*
terminal_screen_get_current_dir_with_fallback (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->pty_fd == -1)
    return g_strdup (priv->initial_working_directory);

  return terminal_screen_get_current_dir (screen);
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
  
  if (gtk_widget_get_realized (GTK_WIDGET (screen)))
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
}

static void
terminal_screen_icon_title_changed (VteTerminal *vte_terminal,
                                    TerminalScreen *screen)
{
  terminal_screen_set_dynamic_icon_title (screen,
                                          vte_terminal_get_icon_title (vte_terminal),
					  FALSE);
}

static void
terminal_screen_child_exited (VteTerminal *terminal)
{
  TerminalScreen *screen = TERMINAL_SCREEN (terminal);
  TerminalScreenPrivate *priv = screen->priv;
  TerminalExitAction action;

  /* No need to chain up to VteTerminalClass::child_exited since it's NULL */

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] child process exited\n",
                         screen);

  priv->child_pid = -1;
  priv->pty_fd = -1;
  
  action = terminal_profile_get_property_enum (priv->profile, TERMINAL_PROFILE_EXIT_ACTION);
  
  switch (action)
    {
    case TERMINAL_EXIT_CLOSE:
      g_signal_emit (screen, signals[CLOSE_SCREEN], 0);
      break;
    case TERMINAL_EXIT_RESTART:
      terminal_screen_launch_child_on_idle (screen);
      break;
    case TERMINAL_EXIT_HOLD: {
      GtkWidget *info_bar;
      int status;

      status = vte_terminal_get_child_exit_status (terminal);

      info_bar = terminal_info_bar_new (GTK_MESSAGE_INFO,
                                        _("_Relaunch"), RESPONSE_RELAUNCH,
                                        NULL);
      if (WIFEXITED (status)) {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process exited normally with status %d."), WEXITSTATUS (status));
      } else if (WIFSIGNALED (status)) {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process was terminated by signal %d."), WTERMSIG (status));
      } else {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process was terminated."));
      }
      g_signal_connect (info_bar, "response",
                        G_CALLBACK (info_bar_response_cb), screen);

      gtk_box_pack_start (GTK_BOX (terminal_screen_container_get_from_screen (screen)),
                          info_bar, FALSE, FALSE, 0);
      gtk_info_bar_set_default_response (GTK_INFO_BAR (info_bar), RESPONSE_RELAUNCH);
      gtk_widget_show (info_bar);
      break;
    }

    default:
      break;
    }
}

void
terminal_screen_set_user_title (TerminalScreen *screen,
                                const char *text)
{
  TerminalScreenPrivate *priv = screen->priv;

  /* The user set the title to nothing, let's understand that as a
     request to revert to dynamically setting the title again. */
  if (!text || !text[0])
    priv->user_title = FALSE;
  else
    {
      priv->user_title = TRUE;
      terminal_screen_set_dynamic_title (screen, text, TRUE);
      terminal_screen_set_dynamic_icon_title (screen, text, TRUE);
    }
}

static void
terminal_screen_drag_data_received (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             timestamp)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;
  const guchar *selection_data_data;
  GdkAtom selection_data_target;
  gint selection_data_length, selection_data_format;

  selection_data_data = gtk_selection_data_get_data (selection_data);
  selection_data_target = gtk_selection_data_get_target (selection_data);
  selection_data_length = gtk_selection_data_get_length (selection_data);
  selection_data_format = gtk_selection_data_get_format (selection_data);

#if 0
  {
    GList *tmp;

    g_print ("info: %d\n", info);
    tmp = context->targets;
    while (tmp != NULL)
      {
        GdkAtom atom = GDK_POINTER_TO_ATOM (tmp->data);

        g_print ("Target: %s\n", gdk_atom_name (atom));        
        
        tmp = tmp->next;
      }

    g_print ("Chosen target: %s\n", gdk_atom_name (selection_data->target));
  }
#endif

  if (gtk_targets_include_uri (&selection_data_target, 1))
    {
      char **uris;
      char *text;
      gsize len;

      uris = gtk_selection_data_get_uris (selection_data);
      if (!uris)
        return;

      terminal_util_transform_uris_to_quoted_fuse_paths (uris);

      text = terminal_util_concat_uris (uris, &len);
      vte_terminal_feed_child (VTE_TERMINAL (screen), text, len);
      g_free (text);

      g_strfreev (uris);
    }
  else if (gtk_targets_include_text (&selection_data_target, 1))
    {
      char *text;

      text = (char *) gtk_selection_data_get_text (selection_data);
      if (text && text[0])
        vte_terminal_feed_child (VTE_TERMINAL (screen), text, strlen (text));
      g_free (text);
    }
  else switch (info)
    {
    case TARGET_COLOR:
      {
        guint16 *data = (guint16 *)selection_data_data;
        GdkColor color;

        /* We accept drops with the wrong format, since the KDE color
         * chooser incorrectly drops application/x-color with format 8.
         * So just check for the data length.
         */
        if (selection_data_length != 8)
          return;

        color.red = data[0];
        color.green = data[1];
        color.blue = data[2];
        /* FIXME: use opacity from data[3] */

        g_object_set (priv->profile,
                      TERMINAL_PROFILE_BACKGROUND_TYPE, TERMINAL_BACKGROUND_SOLID,
                      TERMINAL_PROFILE_USE_THEME_COLORS, FALSE,
                      TERMINAL_PROFILE_BACKGROUND_COLOR, &color,
                      NULL);
      }
      break;

    case TARGET_MOZ_URL:
      {
        char *utf8_data, *newline, *text;
        char *uris[2];
        gsize len;
        
        /* MOZ_URL is in UCS-2 but in format 8. BROKEN!
         *
         * The data contains the URL, a \n, then the
         * title of the web page.
         */
        if (selection_data_format != 8 ||
            selection_data_length == 0 ||
            (selection_data_length % 2) != 0)
          return;

        utf8_data = g_utf16_to_utf8 ((const gunichar2*) selection_data_data,
                                     selection_data_length / 2,
                                     NULL, NULL, NULL);
        if (!utf8_data)
          return;

        newline = strchr (utf8_data, '\n');
        if (newline)
          *newline = '\0';

        uris[0] = utf8_data;
        uris[1] = NULL;
        terminal_util_transform_uris_to_quoted_fuse_paths (uris); /* This may replace uris[0] */

        text = terminal_util_concat_uris (uris, &len);
        vte_terminal_feed_child (VTE_TERMINAL (screen), text, len);
        g_free (text);
        g_free (uris[0]);
      }
      break;

    case TARGET_NETSCAPE_URL:
      {
        char *utf8_data, *newline, *text;
        char *uris[2];
        gsize len;
        
        /* The data contains the URL, a \n, then the
         * title of the web page.
         */
        if (selection_data_length < 0 || selection_data_format != 8)
          return;

        utf8_data = g_strndup ((char *) selection_data_data, selection_data_length);
        newline = strchr (utf8_data, '\n');
        if (newline)
          *newline = '\0';

        uris[0] = utf8_data;
        uris[1] = NULL;
        terminal_util_transform_uris_to_quoted_fuse_paths (uris); /* This may replace uris[0] */

        text = terminal_util_concat_uris (uris, &len);
        vte_terminal_feed_child (VTE_TERMINAL (screen), text, len);
        g_free (text);
        g_free (uris[0]);
      }
      break;
       
    case TARGET_BGIMAGE:
      {
        char *utf8_data;
        char **uris;
        
        if (selection_data_length < 0 || selection_data_format != 8)
          return;
        
        utf8_data = g_strndup ((char *) selection_data_data, selection_data_length);
        uris = g_uri_list_extract_uris (utf8_data);
        g_free (utf8_data);

        /* FIXME: use terminal_util_transform_uris_to_quoted_fuse_paths? */

        if (uris && uris[0])
          {
            char *filename;

            filename = g_filename_from_uri (uris[0], NULL, NULL);
            if (filename)
              {
                g_object_set (priv->profile,
                              TERMINAL_PROFILE_BACKGROUND_TYPE, TERMINAL_BACKGROUND_IMAGE,
                              TERMINAL_PROFILE_BACKGROUND_IMAGE_FILE, filename,
                              NULL);
              }

            g_free (filename);
          }

        g_strfreev (uris);
      }
      break;

    case TARGET_RESET_BG:
      g_object_set (priv->profile,
                    TERMINAL_PROFILE_BACKGROUND_TYPE, TERMINAL_BACKGROUND_SOLID,
                    NULL);
      break;

    case TARGET_TAB:
      {
        GtkWidget *container;
        TerminalScreen *moving_screen;
        TerminalWindow *source_window;
        TerminalWindow *dest_window;
        GtkWidget *dest_notebook;
        int page_num;

        container = *(GtkWidget**) selection_data_data;
        if (!GTK_IS_WIDGET (container))
          return;

        moving_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (container));
        g_warn_if_fail (TERMINAL_IS_SCREEN (moving_screen));
        if (!TERMINAL_IS_SCREEN (moving_screen))
          return;

        source_window = terminal_screen_get_window (moving_screen);
        dest_window = terminal_screen_get_window (screen);
        dest_notebook = terminal_window_get_notebook (dest_window);
        page_num = gtk_notebook_page_num (GTK_NOTEBOOK (dest_notebook), 
                                          GTK_WIDGET (screen));
        terminal_window_move_screen (source_window, dest_window, moving_screen, page_num + 1);

        gtk_drag_finish (context, TRUE, TRUE, timestamp);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

void
_terminal_screen_update_scrollbar (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalScreenContainer *container;
  GtkPolicyType policy = GTK_POLICY_ALWAYS;
  GtkCornerType corner = GTK_CORNER_TOP_LEFT;

  container = terminal_screen_container_get_from_screen (screen);
  if (container == NULL)
    return;

  switch (terminal_profile_get_property_enum (priv->profile, TERMINAL_PROFILE_SCROLLBAR_POSITION))
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

  terminal_screen_container_set_placement (container, corner);
  terminal_screen_container_set_policy (container, GTK_POLICY_NEVER, policy);
}

void
terminal_screen_get_size (TerminalScreen *screen,
			  int       *width_chars,
			  int       *height_chars)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);

  *width_chars = vte_terminal_get_column_count (terminal);
  *height_chars = vte_terminal_get_row_count (terminal);
}

void
terminal_screen_get_cell_size (TerminalScreen *screen,
			       int                  *cell_width_pixels,
			       int                  *cell_height_pixels)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);

  *cell_width_pixels = vte_terminal_get_char_width (terminal);
  *cell_height_pixels = vte_terminal_get_char_height (terminal);
}

static char*
terminal_screen_check_match (TerminalScreen *screen,
			     int        column,
			     int        row,
                             int       *flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSList *tags;
  int tag;
  char *match;

  match = vte_terminal_match_check (VTE_TERMINAL (screen), column, row, &tag);
  for (tags = priv->match_tags; tags != NULL; tags = tags->next)
    {
      TagData *tag_data = (TagData*) tags->data;
      if (tag_data->tag == tag)
	{
	  if (flavor)
	    *flavor = tag_data->flavor;
	  return match;
	}
    }

  g_free (match);
  return NULL;
}

void
terminal_screen_save_config (TerminalScreen *screen,
                             GKeyFile *key_file,
                             const char *group)
{
  TerminalScreenPrivate *priv = screen->priv;
  VteTerminal *terminal = VTE_TERMINAL (screen);
  TerminalProfile *profile = priv->profile;
  const char *profile_id;
  char *working_directory;

  profile_id = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME);
  g_key_file_set_string (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_PROFILE_ID, profile_id);

  if (priv->override_command)
    terminal_util_key_file_set_argv (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_COMMAND,
                                     -1, priv->override_command);

  if (priv->override_title)
    g_key_file_set_string (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_TITLE, priv->override_title);

  /* FIXMEchpe: use the initial_working_directory instead?? */
  working_directory = terminal_screen_get_current_dir (screen);
  if (working_directory)
    terminal_util_key_file_set_string_escape (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_WORKING_DIRECTORY, working_directory);
  g_free (working_directory);

  g_key_file_set_double (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_ZOOM, priv->font_scale);

  g_key_file_set_integer (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_WIDTH,
                          vte_terminal_get_column_count (terminal));
  g_key_file_set_integer (key_file, group, TERMINAL_CONFIG_TERMINAL_PROP_HEIGHT,
                          vte_terminal_get_row_count (terminal));
}

/**
 * terminal_screen_has_foreground_process:
 * @screen:
 *
 * Checks whether there's a foreground process running in
 * this terminal.
 * 
 * Returns: %TRUE iff there's a foreground process running in @screen
 */
gboolean
terminal_screen_has_foreground_process (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  int fgpid;

  if (priv->pty_fd == -1)
    return FALSE;

  fgpid = tcgetpgrp (priv->pty_fd);
  if (fgpid == -1 || fgpid == priv->child_pid)
    return FALSE;

  return TRUE;

#if 0
  char *cmdline, *basename, *name;
  gsize len;
  char filename[64];

  g_snprintf (filename, sizeof (filename), "/proc/%d/cmdline", fgpid);
  if (!g_file_get_contents (filename, &cmdline, &len, NULL))
    return TRUE;

  basename = g_path_get_basename (cmdline);
  g_free (cmdline);
  if (!basename)
    return TRUE;

  name = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
  g_free (basename);
  if (!name)
    return TRUE;

  if (process_name)
    *process_name = name;

  return TRUE;
#endif
}
