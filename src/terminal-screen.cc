/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2007, 2008, 2010, 2011 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "terminal-pcre2.hh"
#include "terminal-regex.hh"
#include "terminal-screen.hh"
#include "terminal-client-utils.hh"
#include "terminal-notebook.hh"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <uuid.h>

#include <algorithm>

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <adwaita.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#include <adwaita.h>

#include "terminal-accels.hh"
#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-enums.hh"
#include "terminal-intl.hh"
#include "terminal-marshal.h"
#include "terminal-schemas.hh"
#include "terminal-tab.hh"
#include "terminal-util.hh"
#include "terminal-window.hh"
#include "terminal-info-bar.hh"
#include "terminal-libgsystem.hh"

#include "eggshell.hh"

#define URL_MATCH_CURSOR_NAME "pointer"
#define SIZE_DISMISS_TIMEOUT_MSEC 1000

#define DROP_REQUEST_PRIORITY               G_PRIORITY_DEFAULT
#define APPLICATION_VND_PORTAL_FILETRANSFER "application/vnd.portal.filetransfer"
#define APPLICATION_VND_PORTAL_FILES        "application/vnd.portal.files"
#define TEXT_X_MOZ_URL                      "text/x-moz-url"
#define TEXT_URI_LIST                       "text/uri-list"
#define X_SPECIAL_GNOME_RESET_BACKGROUND    "x-special/gnome-reset-background"

namespace {

typedef struct {
  volatile int refcount;
  char **argv; /* as passed */
  char **exec_argv; /* as processed */
  char **envv;
  char *cwd;
  gboolean as_shell;

  VtePtyFlags pty_flags;
  GSpawnFlags spawn_flags;

  /* FD passing */
  GUnixFDList *fd_list;
  int n_fd_map;
  int* fd_map;

  /* async exec callback */
  TerminalScreenExecCallback callback;
  gpointer callback_data;
  GDestroyNotify callback_data_destroy_notify;

  /* Cancellable */
  GCancellable *cancellable;
} ExecData;

} // anon namespace

typedef struct
{
  int tag;
  TerminalURLFlavor flavor;
} TagData;

typedef struct {
  TerminalScreen *screen;
  GdkDrop *drop;
  GList *files;
  const char *mime_type;
} TextUriList;

struct _TerminalScreenPrivate
{
  char *uuid;
  gboolean registered; /* D-Bus interface is registered */

  GSettings *profile; /* never nullptr */
  guint profile_changed_id;
  guint profile_forgotten_id;
  int child_pid;
  GSList *match_tags;
  gboolean exec_on_realize;
  guint idle_exec_source;
  ExecData *exec_data;

  GtkRevealer *size_revealer;
  GtkLabel *size_label;
  guint size_dismiss_source;

  GtkDropTargetAsync *drop_target;
  GtkWidget *drop_highlight;

  GIcon* icon_color;
  GIcon* icon_image;

  bool has_progress;
  bool icon_progress_set;
  VteProgressHint progress_hint;
  double progress_fraction;
  GIcon* icon_progress;
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
  PROP_PROFILE = 1,
  PROP_ICON,
  PROP_ICON_PROGRESS,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec* pspecs[N_PROPS];

static void terminal_screen_constructed (GObject             *object);
static void terminal_screen_dispose     (GObject             *object);
static void terminal_screen_finalize    (GObject             *object);
static void terminal_screen_profile_changed_cb (GSettings     *profile,
                                                const char    *prop_name,
                                                TerminalScreen *screen);
static gboolean terminal_screen_drop_target_drop (TerminalScreen     *screen,
                                                  GdkDrop            *drop,
                                                  double              x,
                                                  double              y,
                                                  GtkDropTargetAsync *drop_target);
static GdkDragAction terminal_screen_drop_target_drag_enter (TerminalScreen     *screen,
                                                             GdkDrop            *drop,
                                                             double              x,
                                                             double              y,
                                                             GtkDropTargetAsync *drop_target);
static void terminal_screen_drop_target_drag_leave (TerminalScreen     *screen,
                                                    GdkDrop            *drop,
                                                    GtkDropTargetAsync *drop_target);
static void terminal_screen_set_font (TerminalScreen *screen);
static void terminal_screen_system_font_changed_cb (GSettings *,
                                                    const char*,
                                                    TerminalScreen *screen);
static void terminal_screen_capture_click_pressed_cb (TerminalScreen  *screen,
                                                      int              n_press,
                                                      double           x,
                                                      double           y,
                                                      GtkGestureClick *click);
static void terminal_screen_bubble_click_pressed_cb (TerminalScreen  *screen,
                                                     int              n_press,
                                                     double           x,
                                                     double           y,
                                                     GtkGestureClick *click);
static void terminal_screen_child_exited  (VteTerminal *terminal,
                                           int status);

static void terminal_screen_window_title_changed      (VteTerminal *vte_terminal,
                                                       TerminalScreen *screen);

static void update_color_scheme                      (TerminalScreen *screen);

static void terminal_screen_check_extra (TerminalScreen *screen,
                                         double          x,
                                         double          y,
                                         char           **number_info,
                                         char           **timestamp_info);
static char* terminal_screen_check_match (TerminalScreen *screen,
                                          double          x,
                                          double          y,
                                          int            *flavor);

static void terminal_screen_show_info_bar (TerminalScreen *screen,
                                           GError *error,
                                           gboolean show_relaunch);


static char**terminal_screen_get_child_environment (TerminalScreen *screen,
                                                    char **initial_envv,
                                                    char **path,
                                                    char **shell);

static gboolean terminal_screen_get_child_command (TerminalScreen *screen,
                                                   char          **exec_argv,
                                                   const char     *path_env,
                                                   const char     *shell_env,
                                                   gboolean        shell,
                                                   gboolean       *preserve_cwd_p,
                                                   GSpawnFlags    *spawn_flags_p,
                                                   char         ***argv_p,
                                                   GError        **err);

static void terminal_screen_menu_popup_action (GtkWidget  *widget,
                                               const char *action_name,
                                               GVariant   *param);

static void terminal_screen_queue_idle_exec (TerminalScreen *screen);

static void _terminal_screen_update_scrollbar (TerminalScreen *screen);

static void _terminal_screen_update_kinetic_scrolling (TerminalScreen *screen);

static void text_uri_list_free (TextUriList *uri_list);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (TextUriList, text_uri_list_free)

static guint signals[LAST_SIGNAL];

typedef struct {
  const char *pattern;
  TerminalURLFlavor flavor;
} TerminalRegexPattern;

static const TerminalRegexPattern url_regex_patterns[] = {
  { REGEX_URL_AS_IS, FLAVOR_AS_IS },
  { REGEX_URL_HTTP,  FLAVOR_DEFAULT_TO_HTTP },
  { REGEX_URL_FILE,  FLAVOR_AS_IS },
  { REGEX_URL_VOIP,  FLAVOR_VOIP_CALL },
  { REGEX_EMAIL,     FLAVOR_EMAIL },
  { REGEX_NEWS_MAN,  FLAVOR_AS_IS },
};

static const TerminalRegexPattern extra_regex_patterns[] = {
  { "(0[Xx][[:xdigit:]]+|[[:digit:]]+)", FLAVOR_NUMBER },
};

static VteRegex **url_regexes;
static VteRegex **extra_regexes;
static TerminalURLFlavor *url_regex_flavors;
static TerminalURLFlavor *extra_regex_flavors;
static guint n_url_regexes;
static guint n_extra_regexes;

/* See bug #697024 */
#ifndef __linux__

#undef dup3
#define dup3 fake_dup3

static int
fake_dup3 (int fd, int fd2, int flags)
{
  if (dup2 (fd, fd2) == -1)
    return -1;

  return fcntl (fd2, F_SETFD, flags);
}
#endif /* !__linux__ */

static char*
strv_to_string (char **strv)
{
  return strv ? g_strjoinv (" ", strv) : g_strdup ("(null)");
}

static char*
exec_data_to_string (ExecData *data)
{
  gs_free char *str1 = nullptr;
  gs_free char *str2 = nullptr;
  return data ? g_strdup_printf ("data %p argv:[%s] exec-argv:[%s] envv:%p(%u) as-shell:%s cwd:%s",
                                 data,
                                 (str1 = strv_to_string (data->argv)),
                                 (str2 = strv_to_string (data->exec_argv)),
                                 data->envv, data->envv ? g_strv_length (data->envv) : 0,
                                 data->as_shell ? "true" : "false",
                                 data->cwd)
  : g_strdup ("(null)");
}

static ExecData*
exec_data_new (void)
{
  ExecData *data = g_new0 (ExecData, 1);
  data->refcount = 1;

  return data;
}

static ExecData *
exec_data_clone (ExecData *data)
{
  if (data == nullptr)
    return nullptr;

  ExecData *clone = exec_data_new ();
  clone->envv = g_strdupv (data->envv);
  clone->cwd = g_strdup (data->cwd);

  /* If FDs were passed, cannot repeat argv. Return data only for env and cwd */
  if (data->fd_list != nullptr) {
    clone->as_shell = TRUE;
    return clone;
  }

  clone->argv = g_strdupv (data->argv);
  clone->as_shell = data->as_shell;

  return clone;
}

static void
exec_data_callback (ExecData *data,
                    GError *error,
                    TerminalScreen *screen)
{
  if (data->callback)
    data->callback (screen, error, data->callback_data);
}

static ExecData*
exec_data_ref (ExecData *data)
{
  data->refcount++;
  return data;
}

static void
exec_data_unref (ExecData *data)
{
  if (data == nullptr)
    return;

  if (--data->refcount > 0)
    return;

  g_strfreev (data->argv);
  g_strfreev (data->exec_argv);
  g_strfreev (data->envv);
  g_free (data->cwd);
  g_clear_object (&data->fd_list);
  g_free (data->fd_map);

  if (data->callback_data_destroy_notify && data->callback_data)
    data->callback_data_destroy_notify (data->callback_data);

  g_clear_object (&data->cancellable);

  g_free (data);
}

GS_DEFINE_CLEANUP_FUNCTION0(ExecData*, _terminal_local_unref_exec_data, exec_data_unref)
#define terminal_unref_exec_data __attribute__((__cleanup__(_terminal_local_unref_exec_data)))

static void
terminal_screen_clear_exec_data (TerminalScreen *screen,
                                 gboolean cancelled)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->exec_data == nullptr)
    return;

  if (cancelled) {
    gs_free_error GError *err = nullptr;
    g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                         "Spawning was cancelled");
    exec_data_callback (priv->exec_data, err, screen);
  }

  exec_data_unref (priv->exec_data);
  priv->exec_data = nullptr;
}

G_DEFINE_TYPE_WITH_PRIVATE (TerminalScreen, terminal_screen, VTE_TYPE_TERMINAL)

static void
free_tag_data (TagData *tagdata)
{
  g_slice_free (TagData, tagdata);
}

static void
precompile_regexes (const TerminalRegexPattern *regex_patterns,
                    guint n_regexes,
                    VteRegex ***regexes,
                    TerminalURLFlavor **regex_flavors)
{
  guint i;

  *regexes = g_new0 (VteRegex*, n_regexes);
  *regex_flavors = g_new0 (TerminalURLFlavor, n_regexes);

  for (i = 0; i < n_regexes; ++i)
    {
      GError *error = nullptr;

      (*regexes)[i] = vte_regex_new_for_match (regex_patterns[i].pattern, -1,
                                               PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE,
                                               &error);
      terminal_assert_no_error (error);

      if (!vte_regex_jit ((*regexes)[i], PCRE2_JIT_COMPLETE, &error) ||
          !vte_regex_jit ((*regexes)[i], PCRE2_JIT_PARTIAL_SOFT, &error)) {
        g_printerr ("Failed to JIT regex '%s': %s\n", regex_patterns[i].pattern, error->message);
        g_clear_error (&error);
      }

      (*regex_flavors)[i] = regex_patterns[i].flavor;
    }
}

static void
terminal_screen_enable_menu_bar_accel_notify_cb (GSettings *settings,
                                                 const char *key,
                                                 TerminalScreen *screen)
{
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != nullptr);
  g_assert (TERMINAL_IS_SCREEN (screen));

  gtk_widget_action_set_enabled (GTK_WIDGET (screen),
                                 "menu.popup",
                                 g_settings_get_boolean (settings, key));
}

static GdkTexture*
texture_from_surface(cairo_surface_t* surface)
{
  gs_unref_bytes auto bytes =
    g_bytes_new_with_free_func(cairo_image_surface_get_data(surface),
                               size_t(cairo_image_surface_get_height(surface)) *
                               size_t(cairo_image_surface_get_stride(surface)),
                               GDestroyNotify(cairo_surface_destroy),
                               cairo_surface_reference(surface));

  return gdk_memory_texture_new(cairo_image_surface_get_width(surface),
                                cairo_image_surface_get_height(surface),
                                GDK_MEMORY_DEFAULT,
                                bytes,
                                cairo_image_surface_get_stride(surface));
}

static void
terminal_screen_icon_color_changed_cb(TerminalScreen* screen,
                                      char const* prop,
                                      VteTerminal* terminal)
{
  auto const priv = screen->priv;

  g_clear_object(&priv->icon_color);

  auto color = GdkRGBA{};
  if (vte_terminal_get_termprop_rgba_by_id(terminal,
                                           VTE_PROPERTY_ID_ICON_COLOR,
                                           &color)) {
    auto const scale = gtk_widget_get_scale_factor(GTK_WIDGET(screen));
    auto const w = 32 * scale, h = 32 * scale;
    auto const xc = w / 2, yc = h / 2;
    auto const radius = w / 2 - 1;

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    auto cr = cairo_create(surface);
    cairo_set_source_rgb(cr, color.red, color.green, color.blue);
    cairo_new_sub_path(cr);
    cairo_arc(cr, xc, yc, radius, 0., G_PI * 2);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_destroy(cr);

    priv->icon_color = G_ICON(texture_from_surface(surface));
    cairo_surface_destroy(surface);
  }

  g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ICON]);
}

static void
terminal_screen_icon_image_changed_cb(TerminalScreen* screen,
                                      char const* prop,
                                      VteTerminal* terminal)
{
  auto const priv = screen->priv;

  g_clear_object(&priv->icon_image);
  priv->icon_image =
    G_ICON(vte_terminal_ref_termprop_image_texture_by_id(terminal,
                                                         VTE_PROPERTY_ID_ICON_IMAGE));

  g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_ICON]);
}

static GIcon*
terminal_screen_ensure_icon_progress(TerminalScreen* screen)
{
  auto const priv = screen->priv;

  if (priv->icon_progress_set)
    return priv->icon_progress;

  if (priv->has_progress) {
    GIcon* icon = nullptr;

    switch (priv->progress_hint) {
    case VTE_PROGRESS_HINT_ERROR:
      icon = g_themed_icon_new("dialog-error-symbolic");
      break;

    case VTE_PROGRESS_HINT_INDETERMINATE:
      icon = nullptr;
      break;

    case VTE_PROGRESS_HINT_PAUSED:
    case VTE_PROGRESS_HINT_ACTIVE: {
      auto const scale = gtk_widget_get_scale_factor(GTK_WIDGET(screen));
      auto const w = 16 * scale, h = 16 * scale;
      auto const xc = w / 2, yc = h / 2;
      auto const radius = w / 2 - 1;

      auto color = GdkRGBA{};
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      auto style_context = gtk_widget_get_style_context(GTK_WIDGET(screen));
      gtk_style_context_get_color(style_context, &color);
      G_GNUC_END_IGNORE_DEPRECATIONS;

      auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
      auto cr = cairo_create(surface);

      // First draw a shadow filled circle
      cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.25);
      cairo_arc(cr, xc, yc, radius, 0., 2 * G_PI);
      cairo_close_path(cr);
      cairo_fill(cr);

      // Now draw progress filled circle
      auto const fraction = priv->progress_fraction;
      if (fraction > 0.) {
        cairo_set_line_width(cr, 1.);
        cairo_set_source_rgb(cr, color.red, color.green, color.blue);
        cairo_new_sub_path(cr);

        if (fraction < 1.) {
          cairo_move_to(cr, xc, yc);
          cairo_line_to(cr, xc + radius, yc);
          cairo_arc_negative(cr, xc, yc, radius, 0, 2 * G_PI * (1. - fraction));
          cairo_line_to(cr, xc, yc);
        } else {
          cairo_arc(cr, xc, yc, radius, 0, 2 * G_PI);
        }

        cairo_close_path(cr);
        cairo_fill(cr);
      }
      cairo_destroy(cr);

      icon = G_ICON(texture_from_surface(surface));
      cairo_surface_destroy(surface);
      break;
    }

    case VTE_PROGRESS_HINT_INACTIVE:
    default:
      icon = nullptr;
      break;
    }

    g_set_object(&priv->icon_progress, icon);

  } else {
    // Remove progress
    g_clear_object(&priv->icon_progress);
  }

  priv->icon_progress_set = true;

  return priv->icon_progress;
}

static void
terminal_screen_clear_icon_progress(TerminalScreen* screen)
{
  auto const priv = screen->priv;

  g_clear_object(&priv->icon_progress);
  priv->icon_progress_set = false;

  g_object_notify_by_pspec(G_OBJECT(screen), pspecs[PROP_ICON_PROGRESS]);
}

static void
terminal_screen_progress_value_changed_cb(TerminalScreen* screen,
                                          char const* prop,
                                          VteTerminal* terminal)
{
  auto const priv = screen->priv;

  auto fraction = 0.;
  uint64_t value = 0;
  auto const has_progress = vte_terminal_get_termprop_uint_by_id(terminal,
                                                                 VTE_PROPERTY_ID_PROGRESS_VALUE,
                                                                 &value);
  if (has_progress)
    fraction = std::clamp(double(value) / 100., 0., 1.);
  else
    fraction = 0.;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  if (priv->has_progress == has_progress &&
      priv->progress_fraction == fraction)
    return;
#pragma GCC diagnostic pop

  priv->has_progress = has_progress;
  priv->progress_fraction = fraction;

  terminal_screen_clear_icon_progress(screen);
}

static void
terminal_screen_progress_hint_changed_cb(TerminalScreen* screen,
                                         char const* prop,
                                         VteTerminal* terminal)
{
  auto const priv = screen->priv;

  VteProgressHint hint;
  int64_t value = 0;
  if (vte_terminal_get_termprop_int_by_id(terminal,
                                          VTE_PROPERTY_ID_PROGRESS_HINT,
                                          &value))
    hint = VteProgressHint(value);
  else
    hint = VTE_PROGRESS_HINT_INACTIVE;

  if (priv->progress_hint == hint)
    return;

  priv->progress_hint = hint;

  terminal_screen_clear_icon_progress(screen);
}

static TerminalWindow *
terminal_screen_get_window (TerminalScreen *screen)
{
  GtkWidget *widget = GTK_WIDGET (screen);
  GtkRoot *toplevel = gtk_widget_get_root (widget);

  return TERMINAL_WINDOW (toplevel);
}

static void
terminal_screen_realize (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->realize (widget);

  terminal_screen_set_font (screen);

  TerminalScreenPrivate *priv = screen->priv;
  if (priv->exec_on_realize)
    terminal_screen_queue_idle_exec (screen);

  priv->exec_on_realize = FALSE;

}

static void
terminal_screen_update_style (TerminalScreen *screen)
{
  update_color_scheme (screen);
  terminal_screen_set_font (screen);
}

static void
terminal_screen_css_changed (GtkWidget *widget,
                             GtkCssStyleChange *change)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->css_changed (widget, change);

  terminal_screen_update_style (screen);
}

static void
terminal_screen_root (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->root (widget);

  if (priv->profile != nullptr)
    terminal_screen_profile_changed_cb (priv->profile, nullptr, screen);
}

static void
terminal_screen_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;
  int min_revealer;
  int nat_revealer;

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->measure (widget,
                                                            orientation,
                                                            for_size,
                                                            minimum, natural,
                                                            minimum_baseline, natural_baseline);

  gtk_widget_measure (GTK_WIDGET (priv->size_revealer),
                      orientation, for_size,
                      &min_revealer, &nat_revealer, nullptr, nullptr);

  *minimum = MAX (*minimum, min_revealer);
  *natural = MAX (*natural, nat_revealer);
}

static gboolean
dismiss_size_label_cb (gpointer user_data)
{
  TerminalScreen *screen = TERMINAL_SCREEN (user_data);
  TerminalScreenPrivate *priv = screen->priv;

  gtk_revealer_set_reveal_child (priv->size_revealer, FALSE);
  priv->size_dismiss_source = 0;

  return G_SOURCE_REMOVE;
}

static void
terminal_screen_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;
  GtkRequisition min;
  GtkAllocation revealer_alloc;
  GtkRoot *root;
  int prev_column_count, column_count;
  int prev_row_count, row_count;

  prev_column_count = vte_terminal_get_column_count (VTE_TERMINAL (screen));
  prev_row_count = vte_terminal_get_row_count (VTE_TERMINAL (screen));

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->size_allocate (widget, width, height, baseline);

  column_count = vte_terminal_get_column_count (VTE_TERMINAL (screen));
  row_count = vte_terminal_get_row_count (VTE_TERMINAL (screen));

  root = gtk_widget_get_root (widget);

  if (terminal_screen_is_active (screen) &&
      GTK_IS_WINDOW (root) &&
      !gtk_window_is_maximized (GTK_WINDOW (root)) &&
      !gtk_window_is_fullscreen (GTK_WINDOW (root)) &&
      (prev_column_count != column_count || prev_row_count != row_count)) {
    char format[32];

    g_snprintf (format, sizeof format, "%ld × %ld",
                vte_terminal_get_column_count (VTE_TERMINAL (screen)),
                vte_terminal_get_row_count (VTE_TERMINAL (screen)));
    gtk_label_set_label (priv->size_label, format);

    gtk_revealer_set_reveal_child (priv->size_revealer, TRUE);

    g_clear_handle_id (&priv->size_dismiss_source, g_source_remove);
    priv->size_dismiss_source = g_timeout_add (SIZE_DISMISS_TIMEOUT_MSEC,
                                               dismiss_size_label_cb,
                                               screen);
  } else if (gtk_window_is_maximized (GTK_WINDOW (root)) ||
             gtk_window_is_fullscreen (GTK_WINDOW (root)) ||
             terminal_window_in_fullscreen_transition (TERMINAL_WINDOW (root))) {
    g_clear_handle_id (&priv->size_dismiss_source, g_source_remove);
    gtk_revealer_set_reveal_child (priv->size_revealer, FALSE);
  }

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->size_revealer), &min, nullptr);
  revealer_alloc.x = width - min.width;
  revealer_alloc.y = height - min.height;
  revealer_alloc.width = min.width;
  revealer_alloc.height = min.height;
  gtk_widget_size_allocate (GTK_WIDGET (priv->size_revealer), &revealer_alloc, -1);

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->drop_highlight), &min, nullptr);
  gtk_widget_allocate (priv->drop_highlight, width, height, baseline, nullptr);
}

static void
terminal_screen_snapshot (GtkWidget   *widget,
                          GtkSnapshot *snapshot)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->snapshot (widget, snapshot);

  gtk_widget_snapshot_child (widget, GTK_WIDGET (priv->drop_highlight), snapshot);
  gtk_widget_snapshot_child (widget, GTK_WIDGET (priv->size_revealer), snapshot);
}

static void
terminal_screen_init (TerminalScreen *screen)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);
  TerminalScreenPrivate *priv;
  TerminalApp *app;
  guint i;
  uuid_t u;
  char uuidstr[37];

  priv = screen->priv = (TerminalScreenPrivate*)terminal_screen_get_instance_private (screen);

  uuid_generate (u);
  uuid_unparse (u, uuidstr);
  priv->uuid = g_strdup (uuidstr);

  priv->child_pid = -1;

  priv->has_progress = false;
  priv->progress_hint = VTE_PROGRESS_HINT_INACTIVE;
  priv->progress_fraction = 0.;

  gtk_widget_init_template (GTK_WIDGET (screen));

  vte_terminal_set_mouse_autohide (terminal, TRUE);
  vte_terminal_set_allow_hyperlink (terminal, TRUE);
  vte_terminal_set_scroll_unit_is_pixels (terminal, TRUE);
  vte_terminal_set_enable_fallback_scrolling (terminal, FALSE);

  for (i = 0; i < n_url_regexes; ++i)
    {
      TagData *tag_data;

      tag_data = g_slice_new (TagData);
      tag_data->flavor = url_regex_flavors[i];
      tag_data->tag = vte_terminal_match_add_regex (terminal, url_regexes[i], 0);
      vte_terminal_match_set_cursor_name (terminal, tag_data->tag, URL_MATCH_CURSOR_NAME);

      priv->match_tags = g_slist_prepend (priv->match_tags, tag_data);
    }

  GdkContentFormatsBuilder *builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_gtype (builder, G_TYPE_STRING);
  gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_FILE_LIST);
  gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_RGBA);
  gdk_content_formats_builder_add_mime_type (builder, APPLICATION_VND_PORTAL_FILES);
  gdk_content_formats_builder_add_mime_type (builder, APPLICATION_VND_PORTAL_FILETRANSFER);
  gdk_content_formats_builder_add_mime_type (builder, TEXT_URI_LIST);
  gdk_content_formats_builder_add_mime_type (builder, TEXT_X_MOZ_URL);
  gdk_content_formats_builder_add_mime_type (builder, X_SPECIAL_GNOME_RESET_BACKGROUND);
  g_autoptr(GdkContentFormats) formats = gdk_content_formats_builder_free_to_formats (builder);

  gtk_drop_target_async_set_actions (priv->drop_target,
                                     GdkDragAction(GDK_ACTION_COPY|GDK_ACTION_MOVE));
  gtk_drop_target_async_set_formats (priv->drop_target, formats);

  g_signal_connect (screen, "window-title-changed",
                    G_CALLBACK (terminal_screen_window_title_changed),
                    screen);

  g_signal_connect(screen, "termprop-changed::" VTE_TERMPROP_ICON_COLOR,
                   G_CALLBACK(terminal_screen_icon_color_changed_cb), screen);
  g_signal_connect(screen, "termprop-changed::" VTE_TERMPROP_ICON_IMAGE,
                   G_CALLBACK(terminal_screen_icon_image_changed_cb), screen);

  g_signal_connect(screen, "termprop-changed::" VTE_TERMPROP_PROGRESS_VALUE,
                   G_CALLBACK(terminal_screen_progress_value_changed_cb), screen);
  g_signal_connect(screen, "termprop-changed::" VTE_TERMPROP_PROGRESS_HINT,
                   G_CALLBACK(terminal_screen_progress_hint_changed_cb), screen);

  app = terminal_app_get ();
  g_signal_connect (terminal_app_get_desktop_interface_settings (app), "changed::" MONOSPACE_FONT_KEY_NAME,
                    G_CALLBACK (terminal_screen_system_font_changed_cb), screen);

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
      case PROP_ICON:
        g_value_set_object(value, terminal_screen_get_icon(screen));
        break;
      case PROP_ICON_PROGRESS:
        g_value_set_object(value, terminal_screen_get_icon_progress(screen));
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
      case PROP_PROFILE:
        terminal_screen_set_profile (screen, (GSettings*)g_value_get_object (value));
        break;
      case PROP_ICON:
      case PROP_ICON_PROGRESS:
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

  object_class->constructed = terminal_screen_constructed;
  object_class->dispose = terminal_screen_dispose;
  object_class->finalize = terminal_screen_finalize;
  object_class->get_property = terminal_screen_get_property;
  object_class->set_property = terminal_screen_set_property;

  widget_class->css_changed = terminal_screen_css_changed;
  widget_class->measure = terminal_screen_measure;
  widget_class->realize = terminal_screen_realize;
  widget_class->root = terminal_screen_root;
  widget_class->size_allocate = terminal_screen_size_allocate;
  widget_class->snapshot = terminal_screen_snapshot;

  terminal_class->child_exited = terminal_screen_child_exited;

  signals[PROFILE_SET] =
    g_signal_new (I_("profile-set"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, profile_set),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, G_TYPE_SETTINGS);
  
  signals[SHOW_POPUP_MENU] =
    g_signal_new (I_("show-popup-menu"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, show_popup_menu),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  signals[MATCH_CLICKED] =
    g_signal_new (I_("match-clicked"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, match_clicked),
                  g_signal_accumulator_true_handled, nullptr,
                  _terminal_marshal_BOOLEAN__STRING_INT_UINT,
                  G_TYPE_BOOLEAN,
                  3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_UINT);
  
  signals[CLOSE_SCREEN] =
    g_signal_new (I_("close-screen"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, close_screen),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  pspecs[PROP_PROFILE] =
     g_param_spec_object ("profile", nullptr, nullptr,
                          G_TYPE_SETTINGS,
                          GParamFlags(G_PARAM_READWRITE |
				      G_PARAM_STATIC_NAME |
				      G_PARAM_STATIC_NICK |
				      G_PARAM_STATIC_BLURB));

  pspecs[PROP_ICON] =
     g_param_spec_object ("icon", nullptr, nullptr,
                          G_TYPE_ICON,
                          GParamFlags(G_PARAM_READABLE |
				      G_PARAM_STATIC_STRINGS |
                                      G_PARAM_EXPLICIT_NOTIFY));

  pspecs[PROP_ICON_PROGRESS] =
     g_param_spec_object ("icon-progress", nullptr, nullptr,
                          G_TYPE_ICON,
                          GParamFlags(G_PARAM_READABLE |
				      G_PARAM_STATIC_STRINGS |
                                      G_PARAM_EXPLICIT_NOTIFY));

  pspecs[PROP_TITLE] =
     g_param_spec_string ("title", nullptr, nullptr,
                          nullptr,
                          GParamFlags(G_PARAM_READABLE |
				      G_PARAM_STATIC_STRINGS |
                                      G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties(object_class, N_PROPS, pspecs);

  gtk_widget_class_install_action (widget_class,
                                   "menu.popup",
                                   nullptr,
                                   terminal_screen_menu_popup_action);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/screen.ui");

  gtk_widget_class_bind_template_child_private (widget_class, TerminalScreen, drop_target);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalScreen, drop_highlight);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalScreen, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalScreen, size_revealer);

  gtk_widget_class_bind_template_callback (widget_class, terminal_screen_bubble_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, terminal_screen_capture_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, terminal_screen_drop_target_drag_enter);
  gtk_widget_class_bind_template_callback (widget_class, terminal_screen_drop_target_drag_leave);
  gtk_widget_class_bind_template_callback (widget_class, terminal_screen_drop_target_drop);

  n_url_regexes = G_N_ELEMENTS (url_regex_patterns);
  precompile_regexes (url_regex_patterns, n_url_regexes, &url_regexes, &url_regex_flavors);
  n_extra_regexes = G_N_ELEMENTS (extra_regex_patterns);
  precompile_regexes (extra_regex_patterns, n_extra_regexes, &extra_regexes, &extra_regex_flavors);

  g_type_ensure (ADW_TYPE_BIN);
}

static void
terminal_screen_constructed (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *settings;

  G_OBJECT_CLASS (terminal_screen_parent_class)->constructed (object);

  terminal_app_register_screen (terminal_app_get (), screen);
  priv->registered = TRUE;

  /* This fixes bug #329827 */
  settings = terminal_app_get_global_settings (terminal_app_get ());
  g_signal_connect_object (settings,
                           "changed::" TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                           G_CALLBACK (terminal_screen_enable_menu_bar_accel_notify_cb),
                           screen,
                           GConnectFlags(0));
  terminal_screen_enable_menu_bar_accel_notify_cb (settings, TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY, screen);
}

static void
terminal_screen_dispose (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;
  GtkSettings *settings;

  g_clear_handle_id (&priv->size_dismiss_source, g_source_remove);

  gtk_widget_dispose_template (GTK_WIDGET (object), TERMINAL_TYPE_SCREEN);

  /* Unset child PID so that when an eventual child-exited signal arrives,
   * we don't emit "close".
   */
  priv->child_pid = -1;

  settings = gtk_widget_get_settings (GTK_WIDGET (screen));
  g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                        0, 0, nullptr, nullptr,
                                        screen);

  if (priv->idle_exec_source != 0)
    {
      g_source_remove (priv->idle_exec_source);
      priv->idle_exec_source = 0;
    }

  terminal_screen_clear_exec_data (screen, TRUE);

  g_clear_object(&screen->priv->icon_color);
  g_clear_object(&screen->priv->icon_image);

  G_OBJECT_CLASS (terminal_screen_parent_class)->dispose (object);

  /* Unregister *after* chaining up to the parent's dispose,
   * since that will terminate the child process if there still
   * is any, and we need to get the dbus signal out
   * from the TerminalReceiver.
   */
  if (priv->registered) {
    terminal_app_unregister_screen (terminal_app_get (), screen);
    priv->registered = FALSE;
  }
}

static void
terminal_screen_finalize (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;

  g_signal_handlers_disconnect_by_func (terminal_app_get_desktop_interface_settings (terminal_app_get ()),
                                        (void*)terminal_screen_system_font_changed_cb,
                                        screen);

  terminal_screen_set_profile (screen, nullptr);

  g_slist_free_full (priv->match_tags, (GDestroyNotify) free_tag_data);

  g_free (priv->uuid);

  G_OBJECT_CLASS (terminal_screen_parent_class)->finalize (object);
}

TerminalScreen *
terminal_screen_new (GSettings       *profile,
                     const char      *title,
                     double           zoom)
{
  g_return_val_if_fail (G_IS_SETTINGS (profile), nullptr);

  TerminalScreen *screen = (TerminalScreen*)g_object_new (TERMINAL_TYPE_SCREEN, nullptr);

  terminal_screen_set_profile (screen, profile);

  vte_terminal_set_size (VTE_TERMINAL (screen),
                         g_settings_get_int (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY),
                         g_settings_get_int (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY));

  /* If given an initial title, strip it of control characters and
   * feed it to the terminal.
   */
  if (title != nullptr) {
    GString *seq;
    const char *p;

    seq = g_string_new ("\033]0;");
    for (p = title; *p; p = g_utf8_next_char (p)) {
      gunichar c = g_utf8_get_char (p);
      if (c < 0x20 || (c >= 0x7f && c <= 0x9f))
        continue;
      else if (c == ';')
        break;

      g_string_append_unichar (seq, c);
    }
    g_string_append (seq, "\033\\");

    vte_terminal_feed (VTE_TERMINAL (screen), seq->str, seq->len);
    g_string_free (seq, TRUE);
  }

  vte_terminal_set_font_scale (VTE_TERMINAL (screen), zoom);
  terminal_screen_set_font (screen);

  return screen;
}

static gboolean
terminal_screen_reexec_from_exec_data (TerminalScreen *screen,
                                       ExecData *data,
                                       char **envv,
                                       const char *cwd,
                                       GCancellable *cancellable,
                                       GError **error)
{
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROCESSES) {
    gs_free char *str = exec_data_to_string (data);
    _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                           "[screen %p] reexec_from_data: envv:%p(%u) cwd:%s data:[%s]\n",
                           screen,
                           envv, envv ? g_strv_length (envv) : 0,
                           cwd,
                           str);
  }

  return terminal_screen_exec (screen,
                               data ? data->argv : nullptr,
                               envv ? envv : data ? data->envv : nullptr,
                               data ? data->as_shell : TRUE,
                               /* If we have command line args, must always pass the cwd from the command line, too */
                               data && data->argv ? data->cwd : cwd ? cwd : data ? data->cwd : nullptr,
                               nullptr /* fd list */, nullptr /* fd array */,
                               nullptr, nullptr, nullptr, /* callback + data + destroy notify */
                               cancellable,
                               error);
}

gboolean
terminal_screen_reexec_from_screen (TerminalScreen *screen,
                                    TerminalScreen *parent_screen,
                                    GCancellable *cancellable,
                                    GError **error)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);

  if (parent_screen == nullptr)
    return TRUE;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (parent_screen), FALSE);

  gs_free char* cwd = terminal_screen_get_current_dir (parent_screen);

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] reexec_from_screen: parent:%p cwd:%s\n",
                         screen,
                         parent_screen,
                         cwd);

  return terminal_screen_reexec_from_exec_data (screen,
                                                nullptr /* exec data */,
                                                nullptr /* envv */,
                                                cwd,
                                                cancellable,
                                                error);
}

gboolean
terminal_screen_reexec (TerminalScreen *screen,
                        char **envv,
                        const char *cwd,
                        GCancellable *cancellable,
                        GError **error)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);


  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] reexec: envv:%p(%u) cwd:%s\n",
                         screen,
                         envv, envv ? g_strv_length (envv) : 0,
                         cwd);

  return terminal_screen_reexec_from_exec_data (screen,
                                                screen->priv->exec_data,
                                                envv,
                                                cwd,
                                                cancellable,
                                                error);
}

gboolean
terminal_screen_exec (TerminalScreen *screen,
                      char **argv,
                      char **initial_envv,
                      gboolean as_shell,
                      const char *cwd,
                      GUnixFDList *fd_list,
                      GVariant *fd_array,
                      TerminalScreenExecCallback callback,
                      gpointer user_data,
                      GDestroyNotify destroy_notify,
                      GCancellable *cancellable,
                      GError **error)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (cancellable == nullptr || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == nullptr || *error == nullptr, FALSE);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (screen)) != nullptr, FALSE);

  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROCESSES) {
    gs_free char *argv_str = nullptr;
    _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                           "[screen %p] exec: argv:[%s] envv:%p(%u) as-shell:%s cwd:%s\n",
                           screen,
                           (argv_str = strv_to_string(argv)),
                           initial_envv, initial_envv ? g_strv_length (initial_envv) : 0,
                           as_shell ? "true":"false",
                           cwd);
  }

  TerminalScreenPrivate *priv = screen->priv;

  ExecData *data = exec_data_new ();
  data->callback = callback;
  data->callback_data = user_data;
  data->callback_data_destroy_notify = destroy_notify;

  GError *err = nullptr;
  if (priv->child_pid != -1) {
    g_set_error_literal (&err, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                         "Cannot launch a new child process while the terminal is still running another child process");

    terminal_screen_show_info_bar (screen, err, FALSE);
    g_propagate_error (error, err);
    exec_data_unref (data); /* frees the callback data */
    return FALSE;
  }

  gs_free char *path = nullptr;
  gs_free char *shell = nullptr;
  gs_strfreev char **envv = terminal_screen_get_child_environment (screen,
                                                                  initial_envv,
                                                                  &path,
                                                                  &shell);

  gboolean preserve_cwd = FALSE;
  GSpawnFlags spawn_flags = GSpawnFlags(G_SPAWN_SEARCH_PATH_FROM_ENVP |
					VTE_SPAWN_NO_PARENT_ENVV);
  gs_strfreev char **exec_argv = nullptr;
  if (!terminal_screen_get_child_command (screen,
                                          argv,
                                          path,
                                          shell,
                                          as_shell,
                                          &preserve_cwd,
                                          &spawn_flags,
                                          &exec_argv,
                                          &err)) {
    terminal_screen_show_info_bar (screen, err, FALSE);
    g_propagate_error (error, err);
    exec_data_unref (data); /* frees the callback data */
    return FALSE;
  }

  if (!preserve_cwd) {
    cwd = g_get_home_dir ();
    envv = g_environ_unsetenv (envv, "PWD");
  }

  data->fd_list = (GUnixFDList*)(fd_list ? g_object_ref(fd_list) : nullptr);

  if (fd_array) {
    terminal_assert_nonnull(fd_list);
    G_GNUC_UNUSED int n_fds = g_unix_fd_list_get_length(fd_list);

    gsize fd_array_data_len;
    const int *fd_array_data = (int const*)g_variant_get_fixed_array (fd_array, &fd_array_data_len, 2 * sizeof (int));

    data->n_fd_map = fd_array_data_len;
    data->fd_map = g_new (int, data->n_fd_map);
    for (gsize i = 0; i < fd_array_data_len; i++) {
      const int fd = fd_array_data[2 * i];
      const int idx = fd_array_data[2 * i + 1];
      terminal_assert_cmpint(idx, >=, 0);
      terminal_assert_cmpuint(idx, <, n_fds);

      data->fd_map[idx] = fd;
    }
  } else {
    data->n_fd_map = 0;
    data->fd_map = nullptr;
  }

  data->argv = g_strdupv (argv);
  data->exec_argv = g_strdupv (exec_argv);
  data->cwd = g_strdup (cwd);
  data->envv = g_strdupv (envv);
  data->as_shell = as_shell;
  data->pty_flags = VTE_PTY_DEFAULT;
  data->spawn_flags = spawn_flags;
  data->cancellable = (GCancellable*)(cancellable ? g_object_ref (cancellable) : nullptr);

  terminal_screen_clear_exec_data (screen, TRUE);
  priv->exec_data = data;

  terminal_screen_queue_idle_exec (screen);

  return TRUE;
}

const char*
terminal_screen_get_title (TerminalScreen *screen)
{
  return vte_terminal_get_window_title (VTE_TERMINAL (screen));
}

static void
terminal_screen_profile_changed_cb (GSettings     *profile,
                                    const char    *prop_name,
                                    TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GObject *object = G_OBJECT (screen);
  VteTerminal *vte_terminal = VTE_TERMINAL (screen);
  TerminalWindow *window;

  g_object_freeze_notify (object);

  if ((window = terminal_screen_get_window (screen)))
    {
      /* We need these in line for the set_size in
       * update_on_realize
       */
      terminal_window_update_geometry (window);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY))
    _terminal_screen_update_scrollbar (screen);

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_KINETIC_SCROLLING_KEY))
    _terminal_screen_update_kinetic_scrolling (screen);

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENCODING_KEY))
    {
      gs_free char *charset = g_settings_get_string (profile, TERMINAL_PROFILE_ENCODING_KEY);
      const char *encoding = terminal_util_translate_encoding (charset);
      if (encoding != nullptr)
        vte_terminal_set_encoding (vte_terminal, encoding, nullptr);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY))
    {
      int width;

      width = g_settings_get_enum (profile, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY);
      vte_terminal_set_cjk_ambiguous_width (vte_terminal, width);
    }

  if (gtk_widget_get_realized (GTK_WIDGET (screen)) &&
      (!prop_name ||
       prop_name == I_(TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY) ||
       prop_name == I_(TERMINAL_PROFILE_FONT_KEY) ||
       prop_name == I_(TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY) ||
       prop_name == I_(TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY)))
    terminal_screen_set_font (screen);

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_FOREGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_PALETTE_KEY))
    update_color_scheme (screen);

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_AUDIBLE_BELL_KEY))
      vte_terminal_set_audible_bell (vte_terminal, g_settings_get_boolean (profile, TERMINAL_PROFILE_AUDIBLE_BELL_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY))
    vte_terminal_set_scroll_on_insert(vte_terminal,
                                      g_settings_get_boolean(profile, TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY))
    vte_terminal_set_scroll_on_keystroke (vte_terminal,
                                          g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY))
    vte_terminal_set_scroll_on_output (vte_terminal,
                                       g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY));
  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_LINES_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY))
    {
      glong lines = g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY) ?
		    -1 : g_settings_get_int (profile, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY);
      vte_terminal_set_scrollback_lines (vte_terminal, lines);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BACKSPACE_BINDING_KEY))
  vte_terminal_set_backspace_binding (vte_terminal,
                                      VteEraseBinding(g_settings_get_enum (profile, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY)));
  
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_DELETE_BINDING_KEY))
  vte_terminal_set_delete_binding (vte_terminal,
                                   VteEraseBinding(g_settings_get_enum (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENABLE_BIDI_KEY))
    vte_terminal_set_enable_bidi (vte_terminal,
                                  g_settings_get_boolean (profile, TERMINAL_PROFILE_ENABLE_BIDI_KEY));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENABLE_SHAPING_KEY))
    vte_terminal_set_enable_shaping (vte_terminal,
                                     g_settings_get_boolean (profile, TERMINAL_PROFILE_ENABLE_SHAPING_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENABLE_SIXEL_KEY))
    vte_terminal_set_enable_sixel (vte_terminal,
                                   g_settings_get_boolean (profile, TERMINAL_PROFILE_ENABLE_SIXEL_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY))
    vte_terminal_set_bold_is_bright (vte_terminal,
                                     g_settings_get_boolean (profile, TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY))
    vte_terminal_set_cursor_blink_mode (vte_terminal,
                                        VteCursorBlinkMode(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CURSOR_SHAPE_KEY))
    vte_terminal_set_cursor_shape (vte_terminal,
                                   VteCursorShape(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_CURSOR_SHAPE_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_REWRAP_ON_RESIZE_KEY))
    vte_terminal_set_rewrap_on_resize (vte_terminal,
                                       g_settings_get_boolean (profile, TERMINAL_PROFILE_REWRAP_ON_RESIZE_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY))
    vte_terminal_set_text_blink_mode (vte_terminal,
                                      VteTextBlinkMode(g_settings_get_enum (profile, TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_WORD_CHAR_EXCEPTIONS_KEY))
    {
      gs_free char *word_char_exceptions;
      g_settings_get (profile, TERMINAL_PROFILE_WORD_CHAR_EXCEPTIONS_KEY, "ms", &word_char_exceptions);
      vte_terminal_set_word_char_exceptions (vte_terminal, word_char_exceptions);
    }

  g_object_thaw_notify (object);
}

static void
update_color_scheme (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *profile = priv->profile;
  gs_free GdkRGBA *colors;
  gsize n_colors = 0;
  GdkRGBA fg, bg, bold;
  GdkRGBA cursor_bg, cursor_fg;
  GdkRGBA highlight_bg, highlight_fg;
  GdkRGBA *boldp;
  GdkRGBA *cursor_bgp = nullptr, *cursor_fgp = nullptr;
  GdkRGBA *highlight_bgp = nullptr, *highlight_fgp = nullptr;
  gboolean use_theme_colors;

  colors = terminal_g_settings_get_rgba_palette (priv->profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);

  use_theme_colors = g_settings_get_boolean (profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY);
  if (!terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &fg) ||
      !terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &bg)) {
    use_theme_colors = true;
  }

  if (use_theme_colors) {
    auto const app = terminal_app_get();
    auto const style_manager = reinterpret_cast<AdwStyleManager*>(terminal_app_get_adw_style_manager(app));

    switch (adw_style_manager_get_color_scheme(style_manager)) {
    default:
    case ADW_COLOR_SCHEME_DEFAULT:
    case ADW_COLOR_SCHEME_FORCE_LIGHT:
    case ADW_COLOR_SCHEME_PREFER_LIGHT:
      if (n_colors >= 16) {
        fg = colors[0];
        bg = colors[15];
      } else if (n_colors >= 8) {
        fg = colors[0];
        bg = colors[7];
      } else {
        fg = {0, 0, 0, 1};
        bg = {1, 1, 1, 1};
      }
      break;

    case ADW_COLOR_SCHEME_PREFER_DARK:
    case ADW_COLOR_SCHEME_FORCE_DARK:
      if (n_colors >= 16) {
        fg = colors[15];
        bg = colors[0];
      } else if (n_colors >= 8) {
        fg = colors[7];
        bg = colors[0];
      } else {
        fg = {1, 1, 1, 1};
        bg = {0, 0, 0, 1};
      }
      break;
    }
  }

  if (!g_settings_get_boolean (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY) &&
      !use_theme_colors &&
      terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BOLD_COLOR_KEY, &bold))
    boldp = &bold;
  else
    boldp = nullptr;

  if (g_settings_get_boolean (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY) &&
      !use_theme_colors)
    {
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY, &cursor_bg))
        cursor_bgp = &cursor_bg;
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY, &cursor_fg))
        cursor_fgp = &cursor_fg;
    }

  if (g_settings_get_boolean (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY) &&
      !use_theme_colors)
    {
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY, &highlight_bg))
        highlight_bgp = &highlight_bg;
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY, &highlight_fg))
        highlight_fgp = &highlight_fg;
    }

  vte_terminal_set_colors (VTE_TERMINAL (screen), &fg, &bg,
                           colors, n_colors);
  vte_terminal_set_color_bold (VTE_TERMINAL (screen), boldp);
  vte_terminal_set_color_cursor (VTE_TERMINAL (screen), cursor_bgp);
  vte_terminal_set_color_cursor_foreground (VTE_TERMINAL (screen), cursor_fgp);
  vte_terminal_set_color_highlight (VTE_TERMINAL (screen), highlight_bgp);
  vte_terminal_set_color_highlight_foreground (VTE_TERMINAL (screen), highlight_fgp);
}

static void
terminal_screen_set_font (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *profile = priv->profile;
  PangoFontDescription *desc;
  int size;

  if (g_settings_get_boolean (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY))
    {
      desc = terminal_app_get_system_font (terminal_app_get ());
    }
  else
    {
      gs_free char *font;
      font = g_settings_get_string (profile, TERMINAL_PROFILE_FONT_KEY);
      desc = pango_font_description_from_string (font);
    }

  size = pango_font_description_get_size (desc);
  /* Sanity check */
  if (size == 0) {
    if (pango_font_description_get_size_is_absolute (desc))
      pango_font_description_set_absolute_size (desc, 10);
    else
      pango_font_description_set_size (desc, 10);
  }

  vte_terminal_set_font (VTE_TERMINAL (screen), desc);

  pango_font_description_free (desc);

  vte_terminal_set_cell_width_scale (VTE_TERMINAL (screen),
                                     g_settings_get_double (profile, TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY));
  vte_terminal_set_cell_height_scale (VTE_TERMINAL (screen),
                                      g_settings_get_double (profile, TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY));
}

static void
terminal_screen_system_font_changed_cb (GSettings      *settings,
                                        const char     *key,
                                        TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen)))
    return;

  if (!g_settings_get_boolean (priv->profile, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY))
    return;

  terminal_screen_set_font (screen);
}

void
terminal_screen_set_profile (TerminalScreen *screen,
                             GSettings *profile)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings*old_profile;

  old_profile = priv->profile;
  if (profile == old_profile)
    return;

  if (priv->profile_changed_id)
    {
      g_signal_handler_disconnect (G_OBJECT (priv->profile),
                                   priv->profile_changed_id);
      priv->profile_changed_id = 0;
    }

  priv->profile = profile;
  if (profile)
    {
      g_object_ref (profile);
      priv->profile_changed_id =
        g_signal_connect (profile, "changed",
                          G_CALLBACK (terminal_screen_profile_changed_cb),
                          screen);
      terminal_screen_profile_changed_cb (profile, nullptr, screen);

      g_signal_emit (G_OBJECT (screen), signals[PROFILE_SET], 0, old_profile);
    }

  if (old_profile)
    g_object_unref (old_profile);

  g_object_notify_by_pspec (G_OBJECT (screen), pspecs[PROP_PROFILE]);
}

GSettings*
terminal_screen_get_profile (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  return priv->profile;
}

GSettings*
terminal_screen_ref_profile (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->profile != nullptr)
    return (GSettings*)g_object_ref (priv->profile);
  return nullptr;
}

static gboolean
should_preserve_cwd (TerminalPreserveWorkingDirectory preserve_cwd,
                     const char *path,
                     const char *arg0)
{
  switch (preserve_cwd) {
  case TERMINAL_PRESERVE_WORKING_DIRECTORY_SAFE: {
    gs_free char *resolved_arg0 = terminal_util_find_program_in_path (path, arg0);
    return resolved_arg0 != nullptr &&
      terminal_util_get_is_shell (resolved_arg0);
  }

  case TERMINAL_PRESERVE_WORKING_DIRECTORY_ALWAYS:
    return TRUE;

  case TERMINAL_PRESERVE_WORKING_DIRECTORY_NEVER:
  default:
    return FALSE;
  }
}

static gboolean
terminal_screen_get_child_command (TerminalScreen *screen,
                                   char          **argv,
                                   const char     *path_env,
                                   const char     *shell_env,
                                   gboolean        as_shell,
                                   gboolean       *preserve_cwd_p,
                                   GSpawnFlags    *spawn_flags_p,
                                   char         ***exec_argv_p,
                                   GError        **err)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *profile = priv->profile;
  TerminalPreserveWorkingDirectory preserve_cwd;
  char **exec_argv;

  g_assert (spawn_flags_p != nullptr && exec_argv_p != nullptr && preserve_cwd_p != nullptr);

  *exec_argv_p = exec_argv = nullptr;

  preserve_cwd = TerminalPreserveWorkingDirectory
    (g_settings_get_enum (profile, TERMINAL_PROFILE_PRESERVE_WORKING_DIRECTORY_KEY));

  if (argv)
    {
      exec_argv = g_strdupv (argv);

      /* argv and cwd come from the command line client, so it must always be used */
      *preserve_cwd_p = TRUE;
      *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_SEARCH_PATH_FROM_ENVP);
    }
  else if (g_settings_get_boolean (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY))
    {
      gs_free char *exec_argv_str;

      exec_argv_str = g_settings_get_string (profile, TERMINAL_PROFILE_CUSTOM_COMMAND_KEY);
      if (!g_shell_parse_argv (exec_argv_str, nullptr, &exec_argv, err))
        return FALSE;

      *preserve_cwd_p = should_preserve_cwd (preserve_cwd, path_env, exec_argv[0]);
      *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_SEARCH_PATH_FROM_ENVP);
    }
  else if (as_shell)
    {
      const char *only_name;
      char *shell;
      int argc = 0;

      shell = egg_shell (shell_env);

      only_name = strrchr (shell, '/');
      if (only_name != nullptr)
        only_name++;
      else {
        only_name = shell;
        *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_SEARCH_PATH_FROM_ENVP);
      }

      exec_argv = g_new (char*, 3);

      exec_argv[argc++] = shell;

      if (g_settings_get_boolean (profile, TERMINAL_PROFILE_LOGIN_SHELL_KEY))
        exec_argv[argc++] = g_strconcat ("-", only_name, nullptr);
      else
        exec_argv[argc++] = g_strdup (only_name);

      exec_argv[argc++] = nullptr;

      *preserve_cwd_p = should_preserve_cwd (preserve_cwd, path_env, shell);
      *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_FILE_AND_ARGV_ZERO);
    }

  else
    {
      g_set_error_literal (err, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                           _("No command supplied nor shell requested"));
      return FALSE;
    }

  *exec_argv_p = exec_argv;

  return TRUE;
}

static gboolean
remove_prefixed_cb(void* key,
                   void* value,
                   void* user_data)
{
  auto const env = reinterpret_cast<char const*>(key);
  auto const prefix = reinterpret_cast<char const*>(user_data);

  if (terminal_client_get_environment_prefix_filters_is_excluded(env))
    return false;

  return g_str_has_prefix(env, prefix);
}

static char**
terminal_screen_get_child_environment (TerminalScreen *screen,
                                       char **initial_envv,
                                       char **path,
                                       char **shell)
{
  TerminalApp *app = terminal_app_get ();
  char **env;
  gs_strfreev char** current_environ = nullptr;
  char *e, *v;
  GHashTable *env_table;
  GHashTableIter iter;
  GPtrArray *retval;
  guint i;

  env_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (initial_envv)
    env = initial_envv;
  else {
    env = current_environ = g_get_environ ();
    /* Remove this variable which we set in server.c:main() */
    env = g_environ_unsetenv (env, "G_ENABLE_DIAGNOSTIC");
  }

  for (i = 0; env[i]; ++i)
    {
      v = strchr (env[i], '=');
      if (v)
          g_hash_table_replace (env_table, g_strndup (env[i], v - env[i]), g_strdup (v + 1));
        else
          g_hash_table_replace (env_table, g_strdup (env[i]), nullptr);
    }

  /* Remove unwanted env variables */
  auto const filters = terminal_client_get_environment_filters ();
  for (i = 0; filters[i]; ++i)
    g_hash_table_remove (env_table, filters[i]);

  auto const pfilters = terminal_client_get_environment_prefix_filters ();
  for (i = 0; pfilters[i]; ++i) {
    g_hash_table_foreach_remove (env_table,
                                 GHRFunc(remove_prefixed_cb),
                                 (void*)pfilters[i]);
  }

  terminal_util_add_proxy_env (env_table);

  /* Add gnome-terminal private env vars used to communicate back to g-t-server */
  GDBusConnection *connection = g_application_get_dbus_connection (G_APPLICATION (app));
  g_hash_table_replace (env_table, g_strdup (TERMINAL_ENV_SERVICE_NAME),
                        g_strdup (g_dbus_connection_get_unique_name (connection)));

  g_hash_table_replace (env_table, g_strdup (TERMINAL_ENV_SCREEN),
                        terminal_app_dup_screen_object_path (app, screen));

  /* Convert to strv */
  retval = g_ptr_array_sized_new (g_hash_table_size (env_table));
  g_hash_table_iter_init (&iter, env_table);
  while (g_hash_table_iter_next (&iter, (gpointer *) &e, (gpointer *) &v))
    g_ptr_array_add (retval, g_strdup_printf ("%s=%s", e, v ? v : ""));
  g_ptr_array_add (retval, nullptr);

  *path = g_strdup ((char const*)g_hash_table_lookup (env_table, "PATH"));
  *shell = g_strdup ((char const*)g_hash_table_lookup (env_table, "SHELL"));

  g_hash_table_destroy (env_table);
  return (char **) g_ptr_array_free (retval, FALSE);
}

enum {
  RESPONSE_RELAUNCH,
  RESPONSE_EDIT_PREFERENCES
};

static void
info_bar_response_cb (GtkWidget *info_bar,
                      int response,
                      TerminalScreen *screen)
{
  TerminalTab *tab;

  gtk_widget_grab_focus (GTK_WIDGET (screen));

  tab = TERMINAL_TAB (gtk_widget_get_ancestor (info_bar, TERMINAL_TYPE_TAB));

  switch (response) {
    case GTK_RESPONSE_CANCEL:
      terminal_tab_remove_overlay (tab, info_bar);
      g_signal_emit (screen, signals[CLOSE_SCREEN], 0);
      break;
    case RESPONSE_RELAUNCH:
      terminal_tab_remove_overlay (tab, info_bar);
      terminal_screen_reexec (screen, nullptr, nullptr, nullptr, nullptr);
      break;
    case RESPONSE_EDIT_PREFERENCES:
      terminal_app_edit_preferences (terminal_app_get (),
                                     terminal_screen_get_profile (screen),
                                     "custom-command-entry");
      break;
    default:
      terminal_tab_remove_overlay (tab, info_bar);
      break;
  }
}

static void
terminal_screen_show_info_bar (TerminalScreen *screen,
                               GError *error,
                               gboolean show_relaunch)
{
  GtkWidget *info_bar;

  if (!gtk_widget_get_parent (GTK_WIDGET (screen)))
    return;

  info_bar = terminal_info_bar_new (GTK_MESSAGE_ERROR,
                                    _("_Preferences"), RESPONSE_EDIT_PREFERENCES,
                                    !show_relaunch ? nullptr : _("_Relaunch"), RESPONSE_RELAUNCH,
                                    nullptr);
  terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                 _("There was an error creating the child process for this terminal"));
  terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                 "%s", error->message);
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (info_bar_response_cb), screen);

  gtk_widget_set_halign (info_bar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (info_bar, GTK_ALIGN_START);
  terminal_tab_add_overlay(terminal_tab_get_from_screen (screen),
                                        info_bar);
  terminal_info_bar_set_default_response (TERMINAL_INFO_BAR (info_bar), GTK_RESPONSE_CANCEL);
  gtk_widget_show (info_bar);
}

static void
spawn_result_cb (VteTerminal *terminal,
                 GPid pid,
                 GError *error,
                 gpointer user_data)
{
  TerminalScreen *screen = TERMINAL_SCREEN (terminal);
  ExecData *exec_data = (ExecData*)user_data;

  /* Terminal was destroyed while the spawn operation was in progress; nothing to do. */
  if (terminal == nullptr)
    goto out;

  {
  TerminalScreenPrivate *priv = screen->priv;

  priv->child_pid = pid;

  if (error) {
     // FIXMEchpe should be unnecessary, vte already does this internally
    vte_terminal_set_pty (terminal, nullptr);

    gboolean can_reexec = TRUE; /* FIXME */
    terminal_screen_show_info_bar (screen, error, can_reexec);
  }

  /* Retain info for reexec, if possible */
  ExecData *new_exec_data = exec_data_clone (exec_data);
  terminal_screen_clear_exec_data (screen, FALSE);
  priv->exec_data = new_exec_data;
  }

out:

  /* Must do this even if the terminal was destroyed */
  exec_data_callback (exec_data, error, screen);

  exec_data_unref (exec_data);
}

static gboolean
idle_exec_cb (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  priv->idle_exec_source = 0;

  ExecData *data = priv->exec_data;
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROCESSES) {
    gs_free char *str = exec_data_to_string (data);
    _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                           "[screen %p] now launching the child process: %s\n",
                           screen, str);
  }

  int n_fds;
  int *fds;
  if (data->fd_list) {
    fds = g_unix_fd_list_steal_fds(data->fd_list, &n_fds);
  } else {
    fds = nullptr;
    n_fds = 0;
  }

  VteTerminal *terminal = VTE_TERMINAL (screen);
  vte_terminal_spawn_with_fds_async (terminal,
                                     data->pty_flags,
                                     data->cwd,
                                     (char const* const*)data->exec_argv,
                                     (char const* const*)data->envv,
                                     fds, n_fds,
                                     data->fd_map, data->n_fd_map,
                                     data->spawn_flags,
                                     nullptr, nullptr, nullptr, /* child setup, data, destroy */
                                     -1,
                                     data->cancellable,
                                     spawn_result_cb,
                                     exec_data_ref (data));

  return FALSE; /* don't run again */
}

static void
terminal_screen_queue_idle_exec (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->idle_exec_source != 0)
    return;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen))) {
    priv->exec_on_realize = TRUE;
    return;
  }

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] scheduling launching the child process on idle\n",
                         screen);

  priv->idle_exec_source = g_idle_add ((GSourceFunc) idle_exec_cb, screen);
}

static TerminalScreenPopupInfo *
terminal_screen_popup_info_new (TerminalScreen *screen)
{
  TerminalScreenPopupInfo *info;

  info = g_slice_new0 (TerminalScreenPopupInfo);
  info->ref_count = 1;

  return info;
}

TerminalScreenPopupInfo *
terminal_screen_popup_info_ref (TerminalScreenPopupInfo *info)
{
  g_return_val_if_fail (info != nullptr, nullptr);

  info->ref_count++;
  return info;
}

void
terminal_screen_popup_info_unref (TerminalScreenPopupInfo *info)
{
  g_return_if_fail (info != nullptr);

  if (--info->ref_count > 0)
    return;

  g_free (info->hyperlink);
  g_free (info->url);
  g_free (info->number_info);
  g_free (info->timestamp_info);
  g_slice_free (TerminalScreenPopupInfo, info);
}

static void
terminal_screen_do_popup (TerminalScreen *screen,
                          int state,
                          guint button,
                          guint time,
                          double x,
                          double y,
                          char *hyperlink,
                          char *url,
                          int url_flavor,
                          char *number_info,
                          char *timestamp_info)
{
  TerminalScreenPopupInfo *info;

  info = terminal_screen_popup_info_new (screen);
  info->button = button;
  info->state = state;
  info->timestamp = time;
  info->x = x;
  info->y = y;
  info->hyperlink = hyperlink; /* adopted */
  info->url = url; /* adopted */
  info->url_flavor = TerminalURLFlavor(url_flavor);
  info->number_info = number_info; /* adopted */
  info->timestamp_info = timestamp_info; /* adopted */

  g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
  terminal_screen_popup_info_unref (info);
}

static void
terminal_screen_menu_popup_action (GtkWidget *widget,
                                   const char *action_name,
                                   GVariant *param)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);

  terminal_screen_do_popup (screen, 0, 0, GDK_CURRENT_TIME, 0, 0,
                            nullptr, nullptr, 0, nullptr, nullptr);
}

static void
terminal_screen_bubble_click_pressed_cb (TerminalScreen  *screen,
                                         int              n_press,
                                         double           x,
                                         double           y,
                                         GtkGestureClick *click)
{
  if (n_press == 1) {
    gs_free char *hyperlink = nullptr;
    gs_free char *url = nullptr;
    int url_flavor = 0;
    gs_free char *number_info = nullptr;
    gs_free char *timestamp_info = nullptr;

    auto event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));
    auto state = gdk_event_get_modifier_state (event) & gtk_accelerator_get_default_mod_mask ();
    auto button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));
    auto time = gdk_event_get_time (event);

    hyperlink = vte_terminal_check_hyperlink_at (VTE_TERMINAL (screen), x, y);
    url = terminal_screen_check_match (screen, x, y, &url_flavor);
    terminal_screen_check_extra (screen, x, y, &number_info, &timestamp_info);

    if (button == 3)
      {
        if (!(state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK)) ||
            !(state & (GDK_CONTROL_MASK | GDK_ALT_MASK)))
          {
            terminal_screen_do_popup (screen, state, button, time, x, y,
                                      g_steal_pointer (&hyperlink),
                                      g_steal_pointer (&url),
                                      url_flavor,
                                      g_steal_pointer (&number_info),
                                      g_steal_pointer (&timestamp_info));
            gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
            return;
          }
      }
  }

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
terminal_screen_capture_click_pressed_cb (TerminalScreen  *screen,
                                          int              n_press,
                                          double           x,
                                          double           y,
                                          GtkGestureClick *click)
{
  gs_free char *hyperlink = nullptr;
  gs_free char *url = nullptr;
  int url_flavor = 0;
  gs_free char *number_info = nullptr;
  gs_free char *timestamp_info = nullptr;
  gboolean handled = FALSE;

  auto event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));
  auto state = gdk_event_get_modifier_state (event) & gtk_accelerator_get_default_mod_mask ();
  auto button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

  hyperlink = vte_terminal_check_hyperlink_at (VTE_TERMINAL (screen), x, y);
  url = terminal_screen_check_match (screen, x, y, &url_flavor);
  terminal_screen_check_extra (screen, x, y, &number_info, &timestamp_info);

  if (n_press == 1 &&
      !handled &&
      hyperlink != nullptr &&
      (button == 1 || button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      g_signal_emit (screen, signals[MATCH_CLICKED], 0,
                     hyperlink,
                     FLAVOR_AS_IS,
                     state,
                     &handled);
    }

  if (n_press == 1 &&
      !handled &&
      url != nullptr &&
      (button == 1 || button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      g_signal_emit (screen, signals[MATCH_CLICKED], 0,
                     url,
                     url_flavor,
                     state,
                     &handled);
    }

  if (handled)
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  else
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

/**
 * terminal_screen_get_current_dir:
 * @screen:
 *
 * Tries to determine the current working directory of the foreground process
 * in @screen's PTY.
 *
 * Returns: a newly allocated string containing the current working directory,
 *   or %nullptr on failure
 */
char *
terminal_screen_get_current_dir (TerminalScreen *screen)
{
  const char *uri;

  uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (screen));
  if (uri != nullptr)
    return g_filename_from_uri (uri, nullptr, nullptr);

  ExecData *data = screen->priv->exec_data;
  if (data && data->cwd)
    return g_strdup (data->cwd);

  return nullptr;
}

static void
terminal_screen_window_title_changed (VteTerminal *vte_terminal,
                                      TerminalScreen *screen)
{
  g_object_notify_by_pspec (G_OBJECT (screen), pspecs[PROP_TITLE]);
}

static void
terminal_screen_child_exited (VteTerminal *terminal,
                              int status)
{
  TerminalScreen *screen = TERMINAL_SCREEN (terminal);
  TerminalScreenPrivate *priv = screen->priv;
  TerminalExitAction action;

  /* Don't do anything if we don't have a child */
  if (priv->child_pid == -1)
    return;

  /* No need to chain up to VteTerminalClass::child_exited since it's nullptr */

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] child process exited\n",
                         screen);

  priv->child_pid = -1;

  action = TerminalExitAction(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_EXIT_ACTION_KEY));
  auto const tab = terminal_tab_get_from_screen(screen);
  if (terminal_tab_get_pinned(tab))
    action = TERMINAL_EXIT_HOLD;

  switch (action)
    {
    case TERMINAL_EXIT_CLOSE:
      g_signal_emit (screen, signals[CLOSE_SCREEN], 0);
      break;
    case TERMINAL_EXIT_RESTART:
      terminal_screen_reexec (screen, nullptr, nullptr, nullptr, nullptr);
      break;
    case TERMINAL_EXIT_HOLD: {
      GtkWidget *info_bar;

      info_bar = terminal_info_bar_new (GTK_MESSAGE_INFO,
                                        _("_Relaunch"), RESPONSE_RELAUNCH,
                                        nullptr);
      if (WIFEXITED (status)) {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process exited normally with status %d."), WEXITSTATUS (status));
      } else if (WIFSIGNALED (status)) {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process was aborted by signal %d."), WTERMSIG (status));
      } else {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process was aborted."));
      }
      g_signal_connect (info_bar, "response",
                        G_CALLBACK (info_bar_response_cb), screen);

      gtk_widget_set_halign (info_bar, GTK_ALIGN_FILL);
      gtk_widget_set_valign (info_bar, GTK_ALIGN_START);
      terminal_tab_add_overlay(terminal_tab_get_from_screen (screen),
                                            info_bar);
      terminal_info_bar_set_default_response (TERMINAL_INFO_BAR (info_bar), RESPONSE_RELAUNCH);
      gtk_widget_show (info_bar);
      break;
    }

    default:
      break;
    }
}

static void
text_uri_list_free (TextUriList *uri_list)
{
  g_clear_object (&uri_list->screen);
  g_clear_object (&uri_list->drop);
  g_clear_list (&uri_list->files, g_object_unref);
  uri_list->mime_type = nullptr;
  g_free (uri_list);
}

static void
terminal_screen_drop_file_list (TerminalScreen *screen,
                                const GList    *files)
{
  g_assert (TERMINAL_IS_SCREEN (screen));
  g_assert (files == nullptr || G_IS_FILE (files->data));

  g_autoptr(GString) string = g_string_new (nullptr);

  for (const GList *iter = files; iter; iter = iter->next) {
    GFile *file = G_FILE (iter->data);

    if (g_file_is_native (file)) {
      g_autofree char *quoted = g_shell_quote (g_file_peek_path (file));

      g_string_append (string, quoted);
      g_string_append_c (string, ' ');
    } else {
      g_autofree char *uri = g_file_get_uri (file);
      g_autofree char *quoted = g_shell_quote (uri);

      g_string_append (string, quoted);
      g_string_append_c (string, ' ');
    }
  }

  if (string->len > 0)
    terminal_screen_paste_text (screen, string->str, string->len);
}

static void
terminal_screen_drop_uri_list_line_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GDataInputStream *line_reader = G_DATA_INPUT_STREAM (object);
  g_autoptr(TextUriList) state = (TextUriList*)user_data;
  g_autoptr(GError) error = nullptr;
  g_autofree char *line = nullptr;
  gsize len = 0;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != nullptr);
  g_assert (TERMINAL_IS_SCREEN (state->screen));
  g_assert (GDK_IS_DROP (state->drop));

  line = g_data_input_stream_read_line_finish_utf8 (line_reader, result, &len, &error);

  if (error != nullptr) {
    g_debug ("Failed to receive '%s': %s", state->mime_type, error->message);
    gdk_drop_finish (state->drop, GdkDragAction(0));
    return;
  }

  if (line != nullptr && line[0] != 0 && line[0] != '#') {
    GFile *file = g_file_new_for_uri (line);

    if (file != nullptr) {
      state->files = g_list_append (state->files, file);
    }
  }

  if (line == nullptr || g_strcmp0 (state->mime_type, TEXT_X_MOZ_URL) == 0) {
    terminal_screen_drop_file_list (state->screen, state->files);
    gdk_drop_finish (state->drop, GDK_ACTION_COPY);
    return;
  }

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       nullptr,
                                       terminal_screen_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static void
terminal_screen_drop_uri_list_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(TerminalScreen) screen = TERMINAL_SCREEN (user_data);
  g_autoptr(GError) error = nullptr;
  g_autoptr(GInputStream) stream = nullptr;
  g_autoptr(GDataInputStream) line_reader = nullptr;
  g_autoptr(TextUriList) state = nullptr;
  const char *mime_type = nullptr;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (TERMINAL_IS_SCREEN (screen));

  if (!(stream = gdk_drop_read_finish (drop, result, &mime_type, &error))) {
    g_debug ("Failed to receive text/uri-list offer: %s", error->message);
    gdk_drop_finish (drop, GdkDragAction(0));
    return;
  }

  g_assert (g_strcmp0 (mime_type, TEXT_URI_LIST) == 0);
  g_assert (G_IS_INPUT_STREAM (stream));

  line_reader = g_data_input_stream_new (stream);
  g_data_input_stream_set_newline_type (line_reader,
                                        G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

  state = g_new0 (TextUriList, 1);
  state->screen = g_object_ref (screen);
  state->drop = g_object_ref (drop);
  state->mime_type = g_intern_string (mime_type);

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       nullptr,
                                       terminal_screen_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static void
terminal_screen_drop_file_list_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(TerminalScreen) screen = TERMINAL_SCREEN (user_data);
  g_autoptr(GError) error = nullptr;
  const GValue *value;
  const GList *file_list;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (TERMINAL_IS_SCREEN (screen));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error))) {
    g_debug ("Failed to receive file-list offer: %s", error->message);

    /* If the user dragged a directory from Nautilus or another
     * new-style application, a portal request would be made. But
     * GTK won't be able to open the directory so the request for
     * APPLICATION_VND_PORTAL_FILETRANSFER will fail. Fallback to
     * opening the request via TEXT_URI_LIST gracefully.
     */
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
        g_error_matches (error, G_IO_ERROR, G_DBUS_ERROR_ACCESS_DENIED))
      gdk_drop_read_async (drop,
                           (const char **)(const char * const[]){TEXT_URI_LIST, nullptr},
                           DROP_REQUEST_PRIORITY,
                           nullptr,
                           terminal_screen_drop_uri_list_cb,
                           g_object_ref (screen));
    else
      gdk_drop_finish (drop, GdkDragAction(0));

    return;
  }

  g_assert (value != nullptr);
  g_assert (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST));

  file_list = (const GList *)g_value_get_boxed (value);
  terminal_screen_drop_file_list (screen, file_list);
  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
terminal_screen_drop_string_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(TerminalScreen) screen = TERMINAL_SCREEN (user_data);
  g_autoptr(GError) error = nullptr;
  const GValue *value;
  const char *string;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (TERMINAL_IS_SCREEN (screen));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error))) {
    gdk_drop_finish (drop, GdkDragAction(0));
    return;
  }

  g_assert (value != nullptr);
  g_assert (G_VALUE_HOLDS_STRING (value));

  string = g_value_get_string (value);

  if (string != nullptr && string[0] != 0)
    terminal_screen_paste_text (screen, string, -1);

  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
terminal_screen_drop_color_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(TerminalScreen) screen = TERMINAL_SCREEN (user_data);
  TerminalScreenPrivate *priv = screen->priv;
  g_autoptr(GError) error = nullptr;
  const GValue *value;
  const GdkRGBA *color;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (TERMINAL_IS_SCREEN (screen));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error))) {
    gdk_drop_finish (drop, GdkDragAction(0));
    return;
  }

  g_assert (value != nullptr);
  g_assert (G_VALUE_HOLDS (value, GDK_TYPE_RGBA));

  color = (const GdkRGBA *)g_value_get_boxed (value);
  terminal_g_settings_set_rgba (priv->profile,
                                TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                color);
  g_settings_set_boolean (priv->profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY, FALSE);

  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
terminal_screen_drop_moz_url_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(TerminalScreen) screen = TERMINAL_SCREEN (user_data);
  g_autoptr(GCharsetConverter) converter = nullptr;
  g_autoptr(GDataInputStream) line_reader = nullptr;
  g_autoptr(GInputStream) converter_stream = nullptr;
  g_autoptr(GInputStream) stream = nullptr;
  g_autoptr(GError) error = nullptr;
  const char *mime_type = nullptr;
  TextUriList *state;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (TERMINAL_IS_SCREEN (screen));

  if (!(stream = gdk_drop_read_finish (drop, result, &mime_type, &error))) {
    gdk_drop_finish (drop, GdkDragAction(0));
    return;
  }

  g_assert (G_IS_INPUT_STREAM (stream));

  if (!(converter = g_charset_converter_new ("UTF-8", "UCS-2", &error))) {
    g_debug ("Failed to create UTF-8 decoder: %s", error->message);
    gdk_drop_finish (drop, GdkDragAction(0));
    return;
  }

  /* TEXT_X_MOZ_URL is in UCS-2 so convert it to UTF-8.
   *
   * The data is expected to be URL, a \n, then the title of the web page.
   *
   * However, some applications (e.g. dolphin) delimit with a \r\n (see
   * issue#293) so handle that generically with the line reader.
   */
  converter_stream = g_converter_input_stream_new (stream, G_CONVERTER (converter));
  line_reader = g_data_input_stream_new (converter_stream);
  g_data_input_stream_set_newline_type (line_reader,
                                        G_DATA_STREAM_NEWLINE_TYPE_ANY);

  state = g_new0 (TextUriList, 1);
  state->screen = g_object_ref (screen);
  state->drop = g_object_ref (drop);
  state->mime_type = g_intern_string (TEXT_X_MOZ_URL);

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       nullptr,
                                       terminal_screen_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static gboolean
terminal_screen_drop_target_drop (TerminalScreen     *screen,
                                  GdkDrop            *drop,
                                  double              x,
                                  double              y,
                                  GtkDropTargetAsync *drop_target)
{
  TerminalScreenPrivate *priv = screen->priv;
  GdkContentFormats *formats;

  g_assert (TERMINAL_IS_SCREEN (screen));
  g_assert (GDK_IS_DROP (drop));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_hide (priv->drop_highlight);

  formats = gdk_drop_get_formats (drop);

  if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_FILE_LIST) ||
      gdk_content_formats_contain_gtype (formats, G_TYPE_FILE) ||
      gdk_content_formats_contain_mime_type (formats, TEXT_URI_LIST) ||
      gdk_content_formats_contain_mime_type (formats, APPLICATION_VND_PORTAL_FILETRANSFER) ||
      gdk_content_formats_contain_mime_type (formats, APPLICATION_VND_PORTAL_FILES)) {
    gdk_drop_read_value_async (drop,
                               GDK_TYPE_FILE_LIST,
                               DROP_REQUEST_PRIORITY,
                               nullptr,
                               terminal_screen_drop_file_list_cb,
                               g_object_ref (screen));
    return TRUE;
  } else if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_RGBA)) {
    gdk_drop_read_value_async (drop,
                               GDK_TYPE_RGBA,
                               DROP_REQUEST_PRIORITY,
                               nullptr,
                               terminal_screen_drop_color_cb,
                               g_object_ref (screen));
    return TRUE;
  } else if (gdk_content_formats_contain_mime_type (formats, TEXT_X_MOZ_URL)) {
    gdk_drop_read_async (drop,
                         (const char **)((const char * const []){TEXT_X_MOZ_URL, nullptr}),
                         DROP_REQUEST_PRIORITY,
                         nullptr,
                         terminal_screen_drop_moz_url_cb,
                         g_object_ref (screen));
    return TRUE;
  } else if (gdk_content_formats_contain_mime_type (formats, X_SPECIAL_GNOME_RESET_BACKGROUND)) {
      g_settings_reset (priv->profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY);
      gdk_drop_finish (drop, GDK_ACTION_COPY);
      return TRUE;
  } else if (gdk_content_formats_contain_gtype (formats, G_TYPE_STRING)) {
    gdk_drop_read_value_async (drop,
                               G_TYPE_STRING,
                               DROP_REQUEST_PRIORITY,
                               nullptr,
                               terminal_screen_drop_string_cb,
                               g_object_ref (screen));
    return TRUE;
  }

  return FALSE;
}

static GdkDragAction
terminal_screen_drop_target_drag_enter (TerminalScreen     *screen,
                                        GdkDrop            *drop,
                                        double              x,
                                        double              y,
                                        GtkDropTargetAsync *drop_target)
{
  TerminalScreenPrivate *priv = screen->priv;

  g_assert (TERMINAL_IS_SCREEN (screen));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_show (priv->drop_highlight);

  return GDK_ACTION_COPY;
}

static void
terminal_screen_drop_target_drag_leave (TerminalScreen     *screen,
                                        GdkDrop            *drop,
                                        GtkDropTargetAsync *drop_target)
{
  TerminalScreenPrivate *priv = screen->priv;

  g_assert (TERMINAL_IS_SCREEN (screen));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_hide (priv->drop_highlight);
}

static void
_terminal_screen_update_scrollbar (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  auto const tab = terminal_tab_get_from_screen (screen);
  if (tab == nullptr)
    return;

  auto const vpolicy = TerminalScrollbarPolicy(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY));

  terminal_tab_set_policy (tab, TERMINAL_SCROLLBAR_POLICY_NEVER, vpolicy);
}

void
_terminal_screen_update_kinetic_scrolling(TerminalScreen* screen)
{
  auto const tab = terminal_tab_get_from_screen (screen);
  if (tab == nullptr)
    return;

  auto const priv = screen->priv;
  auto const value = g_settings_get_boolean(priv->profile, TERMINAL_PROFILE_KINETIC_SCROLLING_KEY);
  terminal_tab_set_kinetic_scrolling(tab, value);
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
                             double          x,
                             double          y,
                             int            *flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSList *tags;
  int tag;
  char *match;

  match = vte_terminal_check_match_at (VTE_TERMINAL (screen), x, y, &tag);
  for (tags = priv->match_tags; tags != nullptr; tags = tags->next)
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
  return nullptr;
}

static void
terminal_screen_check_extra (TerminalScreen  *screen,
                             double           x,
                             double           y,
                             char           **number_info,
                             char           **timestamp_info)
{
  guint i;
  char **matches;
  gboolean flavor_number_found = FALSE;

  matches = g_newa (char *, n_extra_regexes);
  memset(matches, 0, sizeof(char*) * n_extra_regexes);

  if (
      vte_terminal_check_regex_simple_at (VTE_TERMINAL (screen),
                                          x, y,
                                          extra_regexes,
                                          n_extra_regexes,
                                          0,
                                          matches))
    {
      for (i = 0; i < n_extra_regexes; i++)
        {
          if (matches[i] != nullptr)
            {
              /* Store the first match for each flavor, free all the others */
              switch (extra_regex_flavors[i])
                {
                case FLAVOR_NUMBER:
                  if (!flavor_number_found)
                    {
                      *number_info = terminal_util_number_info (matches[i]);
                      *timestamp_info = terminal_util_timestamp_info (matches[i]);
                      flavor_number_found = TRUE;
                    }
                  g_free (matches[i]);
                  break;
                default:
                  g_free (matches[i]);
                }
            }
        }
    }
}

/**
 * terminal_screen_has_foreground_process:
 * @screen:
 * @process_name: (out) (allow-none): the basename of the program, or %nullptr
 * @cmdline: (out) (allow-none): the full command line, or %nullptr
 *
 * Checks whether there's a foreground process running in
 * this terminal.
 * 
 * Returns: %TRUE iff there's a foreground process running in @screen
 */
gboolean
terminal_screen_has_foreground_process (TerminalScreen *screen,
                                        char           **process_name,
                                        char           **cmdline)
{
  TerminalScreenPrivate *priv = screen->priv;
  gs_free char *command = nullptr;
  gs_free char *data_buf = nullptr;
  gs_free char *basename = nullptr;
  gs_free char *name = nullptr;
  VtePty *pty;
  int fd;
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
  int mib[4];
#else
  char filename[64];
#endif
  char *data;
  gsize i;
  gsize len;
  int fgpid;

  if (priv->child_pid == -1)
    return FALSE;

  pty = vte_terminal_get_pty (VTE_TERMINAL (screen));
  if (pty == nullptr)
    return FALSE;

  fd = vte_pty_get_fd (pty);
  if (fd == -1)
    return FALSE;

  fgpid = tcgetpgrp (fd);
  if (fgpid == -1 || fgpid == priv->child_pid)
    return FALSE;

#if defined(__FreeBSD__) || defined(__DragonFly__)
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_ARGS;
  mib[3] = fgpid;
  if (sysctl (mib, G_N_ELEMENTS (mib), nullptr, &len, nullptr, 0) == -1)
      return TRUE;

  data_buf = (char*)g_malloc0 (len);
  if (sysctl (mib, G_N_ELEMENTS (mib), data_buf, &len, nullptr, 0) == -1)
      return TRUE;
  data = data_buf;
#elif defined(__OpenBSD__)
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC_ARGS;
  mib[2] = fgpid;
  mib[3] = KERN_PROC_ARGV;
  if (sysctl (mib, G_N_ELEMENTS (mib), nullptr, &len, nullptr, 0) == -1)
      return TRUE;

  data_buf = (char*)g_malloc0 (len);
  if (sysctl (mib, G_N_ELEMENTS (mib), data_buf, &len, nullptr, 0) == -1)
      return TRUE;
  data = ((char**)data_buf)[0];
#else
  g_snprintf (filename, sizeof (filename), "/proc/%d/cmdline", fgpid);
  if (!g_file_get_contents (filename, &data_buf, &len, nullptr))
    return TRUE;
  data = data_buf;
#endif

  basename = g_path_get_basename (data);
  if (!basename)
    return TRUE;

  name = g_filename_to_utf8 (basename, -1, nullptr, nullptr, nullptr);
  if (!name)
    return TRUE;

  if (!process_name && !cmdline)
    return TRUE;

  gs_transfer_out_value (process_name, &name);

  if (len > 0 && data[len - 1] == '\0')
    len--;
  for (i = 0; i < len; i++)
    {
      if (data[i] == '\0')
        data[i] = ' ';
    }

  command = g_filename_to_utf8 (data, -1, nullptr, nullptr, nullptr);
  if (!command)
    return TRUE;

  gs_transfer_out_value (cmdline, &command);

  return TRUE;
}

const char *
terminal_screen_get_uuid (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), nullptr);

  return screen->priv->uuid;
}

/**
 * terminal_screen_paste_text:
 * @screen:
 * @text: a NUL-terminated string
 * @len: length of @text, or -1
 *
 * Inserts @text to @terminal as if pasted.
 */
void
terminal_screen_paste_text (TerminalScreen* screen,
                            char const* text,
                            gssize len)
{
  g_return_if_fail (text != nullptr);
  g_return_if_fail (len >= -1);

  /* This is just an API hack until vte 0.69 adds vte_terminal_paste_text_len() */
  /* Note that @text MUST be NUL-terminated */

  vte_terminal_paste_text (VTE_TERMINAL (screen), text);
}

gboolean
terminal_screen_is_active (TerminalScreen *screen)
{
  TerminalNotebook *notebook;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);

  notebook = TERMINAL_NOTEBOOK (gtk_widget_get_ancestor (GTK_WIDGET (screen),
                                                         TERMINAL_TYPE_NOTEBOOK));
  if (notebook == nullptr)
    return FALSE;

  return terminal_notebook_get_active_screen (notebook) == screen;
}

GIcon*
terminal_screen_get_icon(TerminalScreen* screen)
{
  g_return_val_if_fail(TERMINAL_IS_SCREEN(screen), nullptr);

  if (screen->priv->icon_image)
    return screen->priv->icon_image;
  else if (screen->priv->icon_color)
    return screen->priv->icon_color;
  else
    return nullptr;
}

GIcon*
terminal_screen_get_icon_progress(TerminalScreen* screen)
{
  g_return_val_if_fail(TERMINAL_IS_SCREEN(screen), nullptr);

  return terminal_screen_ensure_icon_progress(screen);
}
