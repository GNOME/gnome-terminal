/*
 * Copyright © 2011, 2012 Christian Persch
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

#include "terminal-gdbus.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-defines.h"
#include "terminal-mdi-container.h"
#include "terminal-util.h"
#include "terminal-window.h"

/* ------------------------------------------------------------------------- */

#define TERMINAL_RECEIVER_IMPL_GET_PRIVATE(impl)(G_TYPE_INSTANCE_GET_PRIVATE ((impl), TERMINAL_TYPE_RECEIVER_IMPL, TerminalReceiverImplPrivate))

struct _TerminalReceiverImplPrivate {
  TerminalScreen *screen; /* unowned! */
};

enum {
  PROP_0,
  PROP_SCREEN
};

/* helper functions */

static char *
get_object_path_for_screen (TerminalWindow *window,
                            TerminalScreen *screen)
{
  return g_strdelimit (g_strdup_printf (TERMINAL_RECEIVER_OBJECT_PATH_FORMAT,
                                        gtk_application_window_get_id (GTK_APPLICATION_WINDOW (window)),
                                        terminal_screen_get_uuid (screen)),
                       "-", '_');

}

static void
child_exited_cb (VteTerminal *terminal,
                 int exit_code,
                 TerminalReceiver *receiver)
{
  terminal_receiver_emit_child_exited (receiver, exit_code);
}

static void
terminal_receiver_impl_set_screen (TerminalReceiverImpl *impl,
                                TerminalScreen *screen)
{
  TerminalReceiverImplPrivate *priv;

  g_return_if_fail (TERMINAL_IS_RECEIVER_IMPL (impl));
  g_return_if_fail (screen == NULL || TERMINAL_IS_SCREEN (screen));

  priv = impl->priv;
  if (priv->screen == screen)
    return;

  if (priv->screen) {
    g_signal_handlers_disconnect_matched (priv->screen,
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, impl);
  }

  priv->screen = screen;
  if (screen) {
    g_signal_connect (screen, "child-exited",
                      G_CALLBACK (child_exited_cb), 
                      impl);
    g_signal_connect_swapped (screen, "destroy",
                              G_CALLBACK (_terminal_receiver_impl_unset_screen), 
                              impl);
  }

  g_object_notify (G_OBJECT (impl), "screen");
}

/* Class implementation */

static gboolean 
terminal_receiver_impl_exec (TerminalReceiver *receiver,
                             GDBusMethodInvocation *invocation,
                             GUnixFDList *fd_list,
                             GVariant *options,
                             GVariant *arguments)
{
  TerminalReceiverImpl *impl = TERMINAL_RECEIVER_IMPL (receiver);
  TerminalReceiverImplPrivate *priv = impl->priv;
  const char *working_directory;
  gboolean shell;
  char **exec_argv, **envv;
  gsize exec_argc;
  GVariant *fd_array;
  GError *error;

  if (priv->screen == NULL) {
    g_dbus_method_invocation_return_error_literal (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_FAILED,
                                                   "Terminal already closed");
    goto out;
  }

  if (!g_variant_lookup (options, "cwd", "^&ay", &working_directory))
    working_directory = NULL;
  if (!g_variant_lookup (options, "shell", "b", &shell))
    shell = FALSE;
  if (!g_variant_lookup (options, "environ", "^a&ay", &envv))
    envv = NULL;

  if (!g_variant_lookup (options, "fd-set", "@a(ih)", &fd_array))
    fd_array = NULL;

  /* Check FD passing */
  if ((fd_list != NULL) ^ (fd_array != NULL)) {
    g_dbus_method_invocation_return_error_literal (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_INVALID_ARGS,
                                                   "Must pass both fd-set options and a FD list");
    goto out;
  }
  if (fd_list != NULL && fd_array != NULL) {
    const int *fd_array_data;
    gsize fd_array_data_len, i;
    int n_fds;

    fd_array_data = g_variant_get_fixed_array (fd_array, &fd_array_data_len, 2 * sizeof (int));
    n_fds = g_unix_fd_list_get_length (fd_list);
    for (i = 0; i < fd_array_data_len; i++) {
      const int fd = fd_array_data[2 * i];
      const int idx = fd_array_data[2 * i + 1];

      if (fd == STDIN_FILENO ||
          fd == STDOUT_FILENO ||
          fd == STDERR_FILENO) {
        g_dbus_method_invocation_return_error (invocation,
                                               G_DBUS_ERROR,
                                               G_DBUS_ERROR_INVALID_ARGS,
                                               "Passing of std%s not supported",
                                               fd == STDIN_FILENO ? "in" : fd == STDOUT_FILENO ? "out" : "err");
        goto out;
      }
      if (idx < 0 || idx >= n_fds) {
        g_dbus_method_invocation_return_error_literal (invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_INVALID_ARGS,
                                                       "Handle out of range");
        goto out;
      }
    }
  }

  if (working_directory != NULL)
    _terminal_debug_print (TERMINAL_DEBUG_SERVER,
                           "CWD is '%s'\n", working_directory);

  exec_argv = (char **) g_variant_get_bytestring_array (arguments, &exec_argc);

  error = NULL;
  if (!terminal_screen_exec (priv->screen,
                             exec_argc > 0 ? exec_argv : NULL,
                             envv,
                             shell,
                             working_directory,
                             fd_list, fd_array,
                             &error)) {
    g_dbus_method_invocation_take_error (invocation, error);
  } else {
    terminal_receiver_complete_exec (receiver, invocation, NULL /* outfdlist */);
  }

  g_free (exec_argv);
  g_free (envv);
  if (fd_array)
    g_variant_unref (fd_array);

out:

  return TRUE; /* handled */
}

static void
terminal_receiver_impl_iface_init (TerminalReceiverIface *iface)
{
  iface->handle_exec = terminal_receiver_impl_exec;
}

G_DEFINE_TYPE_WITH_CODE (TerminalReceiverImpl, terminal_receiver_impl, TERMINAL_TYPE_RECEIVER_SKELETON,
                         G_IMPLEMENT_INTERFACE (TERMINAL_TYPE_RECEIVER, terminal_receiver_impl_iface_init))

static void
terminal_receiver_impl_init (TerminalReceiverImpl *impl)
{
  impl->priv = TERMINAL_RECEIVER_IMPL_GET_PRIVATE (impl);
}

static void
terminal_receiver_impl_dispose (GObject *object)
{
  TerminalReceiverImpl *impl = TERMINAL_RECEIVER_IMPL (object);

  terminal_receiver_impl_set_screen (impl, NULL);

  G_OBJECT_CLASS (terminal_receiver_impl_parent_class)->dispose (object);
}

static void
terminal_receiver_impl_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  TerminalReceiverImpl *impl = TERMINAL_RECEIVER_IMPL (object);

  switch (prop_id) {
    case PROP_SCREEN:
      g_value_set_object (value, terminal_receiver_impl_get_screen (impl));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_receiver_impl_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  TerminalReceiverImpl *impl = TERMINAL_RECEIVER_IMPL (object);

  switch (prop_id) {
    case PROP_SCREEN:
      terminal_receiver_impl_set_screen (impl, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_receiver_impl_class_init (TerminalReceiverImplClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = terminal_receiver_impl_dispose;
  gobject_class->get_property = terminal_receiver_impl_get_property;
  gobject_class->set_property = terminal_receiver_impl_set_property;

  g_object_class_install_property
     (gobject_class,
      PROP_SCREEN,
      g_param_spec_object ("screen", NULL, NULL,
                          TERMINAL_TYPE_SCREEN,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (gobject_class, sizeof (TerminalReceiverImplPrivate));
}

/* public API */

/**
 * terminal_receiver_impl_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalReceiverImpl for @screen
 */
TerminalReceiverImpl *
terminal_receiver_impl_new (TerminalScreen *screen)
{
  return g_object_new (TERMINAL_TYPE_RECEIVER_IMPL, 
                       "screen", screen, 
                       NULL);
}

/**
 * terminal_receiver_impl_get_screen:
 * @impl: a #TerminalReceiverImpl
 * 
 * Returns: (transfer none): the impl's #TerminalScreen, or %NULL
 */
TerminalScreen *
terminal_receiver_impl_get_screen (TerminalReceiverImpl *impl)
{
  g_return_val_if_fail (TERMINAL_IS_RECEIVER_IMPL (impl), NULL);

  return impl->priv->screen;
}

/**
 * terminal_receiver_impl_get_screen:
 * @impl: a #TerminalReceiverImpl
 * 
 * Unsets the impls #TerminalScreen.
 */
void
_terminal_receiver_impl_unset_screen (TerminalReceiverImpl *impl)
{
  g_return_if_fail (TERMINAL_IS_RECEIVER_IMPL (impl));

  terminal_receiver_impl_set_screen (impl, NULL);
}

/* ---------------------------------------------------------------------------
 * TerminalFactoryImpl
 * ---------------------------------------------------------------------------
 */

struct _TerminalFactoryImplPrivate {
  gpointer dummy;
};

#define RECEIVER_IMPL_SKELETON_DATA_KEY  "terminal-object-skeleton"

static void
screen_destroy_cb (GObject *screen,
                   gpointer user_data)
{
  GDBusObjectManagerServer *object_manager;
  GDBusObjectSkeleton *skeleton;
  const char *object_path;

  skeleton = g_object_get_data (screen, RECEIVER_IMPL_SKELETON_DATA_KEY);
  if (skeleton == NULL)
    return;

  object_manager = terminal_app_get_object_manager (terminal_app_get ());
  object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (skeleton));
  g_dbus_object_manager_server_unexport (object_manager, object_path);
  g_object_set_data (screen, RECEIVER_IMPL_SKELETON_DATA_KEY, NULL);
}

static gboolean
terminal_factory_impl_create_instance (TerminalFactory *factory,
                                       GDBusMethodInvocation *invocation,
                                       GVariant *options)
{
  TerminalApp *app = terminal_app_get ();
  TerminalSettingsList *profiles_list;
  GDBusObjectManagerServer *object_manager;
  TerminalWindow *window;
  TerminalScreen *screen;
  TerminalReceiverImpl *impl;
  TerminalObjectSkeleton *skeleton;
  char *object_path;
  GSettings *profile = NULL;
  const char *profile_uuid;
  gboolean zoom_set = FALSE;
  gdouble zoom = 1.0;
  guint window_id;
  gboolean show_menubar;
  gboolean active = TRUE;
  gboolean have_new_window, present_window, present_window_set;
  GError *err = NULL;

  /* Look up the profile */
  if (!g_variant_lookup (options, "profile", "&s", &profile_uuid))
    profile_uuid = NULL;

  profiles_list = terminal_app_get_profiles_list (app);
  profile = terminal_profiles_list_ref_profile_by_uuid (profiles_list, profile_uuid, &err);
  if (profile == NULL)
    {
      g_dbus_method_invocation_return_gerror (invocation, err);
      g_error_free (err);
      goto out;
    }

  if (g_variant_lookup (options, "window-id", "u", &window_id)) {
    GtkWindow *win;

    win = gtk_application_get_window_by_id (GTK_APPLICATION (app), window_id);

    if (!TERMINAL_IS_WINDOW (win)) {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Nonexisting window %u referenced",
                                             window_id);
      goto out;
    }

    window = TERMINAL_WINDOW (win);
    have_new_window = FALSE;
  } else {
    const char *startup_id, *display_name, *role;
    gboolean start_maximized, start_fullscreen;
    int screen_number;
    GdkScreen *gdk_screen;

    /* Create a new window */

    if (!g_variant_lookup (options, "display", "^&ay", &display_name)) {
      g_dbus_method_invocation_return_error_literal (invocation, 
                                                     G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                                     "No display specified");
      goto out;
    }

    screen_number = 0;
    gdk_screen = terminal_util_get_screen_by_display_name (display_name, screen_number);
    if (gdk_screen == NULL) {
      g_dbus_method_invocation_return_error (invocation, 
                                             G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "No screen %d on display \"%s\"",
                                             screen_number, display_name);
      goto out;
    }

    window = terminal_app_new_window (app, gdk_screen);

    if (g_variant_lookup (options, "desktop-startup-id", "^&ay", &startup_id))
      gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);

    /* Overwrite the default, unique window role set in terminal_window_init */
    if (g_variant_lookup (options, "role", "&s", &role))
      gtk_window_set_role (GTK_WINDOW (window), role);

    if (g_variant_lookup (options, "show-menubar", "b", &show_menubar))
      terminal_window_set_menubar_visible (window, show_menubar);

    if (g_variant_lookup (options, "fullscreen-window", "b", &start_fullscreen) &&
        start_fullscreen) {
      gtk_window_fullscreen (GTK_WINDOW (window));
    }
    if (g_variant_lookup (options, "maximize-window", "b", &start_maximized) &&
        start_maximized) {
      gtk_window_maximize (GTK_WINDOW (window));
    }

    have_new_window = TRUE;
  }

  g_assert (window != NULL);

  if (g_variant_lookup (options, "zoom", "d", &zoom))
    zoom_set = TRUE;

  screen = terminal_screen_new (profile, NULL, NULL, NULL,
                                zoom_set ? zoom : 1.0);
  terminal_window_add_screen (window, screen, -1);
  terminal_window_switch_screen (window, screen);
  gtk_widget_grab_focus (GTK_WIDGET (screen));

  object_path = get_object_path_for_screen (window, screen);
  g_assert (g_variant_is_object_path (object_path));

  skeleton = terminal_object_skeleton_new (object_path);
  impl = terminal_receiver_impl_new (screen);
  terminal_object_skeleton_set_receiver (skeleton, TERMINAL_RECEIVER (impl));
  g_object_unref (impl);

  object_manager = terminal_app_get_object_manager (app);
  g_dbus_object_manager_server_export (object_manager, G_DBUS_OBJECT_SKELETON (skeleton));
  g_object_set_data_full (G_OBJECT (screen), RECEIVER_IMPL_SKELETON_DATA_KEY,
                          skeleton, (GDestroyNotify) g_object_unref);
  g_signal_connect (screen, "destroy",
                    G_CALLBACK (screen_destroy_cb), app);

  if (active)
    terminal_window_switch_screen (window, screen);

  if (g_variant_lookup (options, "present-window", "b", &present_window))
    present_window_set = TRUE;
  else
    present_window_set = FALSE;

  if (have_new_window) {
    const char *geometry;

    if (g_variant_lookup (options, "geometry", "&s", &geometry) &&
        !terminal_window_parse_geometry (window, geometry))
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "Invalid geometry string \"%s\"", geometry);
  }

  if (have_new_window || (present_window_set && present_window))
    gtk_window_present (GTK_WINDOW (window));

  terminal_factory_complete_create_instance (factory, invocation, object_path);

  g_free (object_path);

out:
  if (profile)
    g_object_unref (profile);

  return TRUE; /* handled */
}

static void
terminal_factory_impl_iface_init (TerminalFactoryIface *iface)
{
  iface->handle_create_instance = terminal_factory_impl_create_instance;
}

G_DEFINE_TYPE_WITH_CODE (TerminalFactoryImpl, terminal_factory_impl, TERMINAL_TYPE_FACTORY_SKELETON,
                         G_IMPLEMENT_INTERFACE (TERMINAL_TYPE_FACTORY, terminal_factory_impl_iface_init))

static void
terminal_factory_impl_init (TerminalFactoryImpl *impl)
{
  impl->priv = G_TYPE_INSTANCE_GET_PRIVATE (impl, TERMINAL_TYPE_FACTORY_IMPL, TerminalFactoryImplPrivate);
}

static void
terminal_factory_impl_class_init (TerminalFactoryImplClass *klass)
{
  g_type_class_add_private (klass, sizeof (TerminalFactoryImplPrivate));
}

/**
 * terminal_factory_impl_new:
 *
 * Returns: (transfer full): a new #TerminalFactoryImpl
 */
TerminalFactory *
terminal_factory_impl_new (void)
{
  return g_object_new (TERMINAL_TYPE_FACTORY_IMPL, NULL);
}
