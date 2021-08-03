/*
 * Copyright © 2011, 2012, 2021 Christian Persch
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

#include "terminal-gdbus.hh"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>

#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-mdi-container.hh"
#include "terminal-util.hh"
#include "terminal-window.hh"
#include "terminal-libgsystem.hh"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

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
  g_return_if_fail (screen == nullptr || TERMINAL_IS_SCREEN (screen));

  priv = impl->priv;
  if (priv->screen == screen)
    return;

  if (priv->screen) {
    g_signal_handlers_disconnect_matched (priv->screen,
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, nullptr, nullptr, impl);
  }

  priv->screen = screen;
  if (screen) {
    g_signal_connect (screen, "child-exited",
                      G_CALLBACK (child_exited_cb),
                      impl);
  }

  g_object_notify (G_OBJECT (impl), "screen");
}

/* Class implementation */

typedef struct {
  void* object;
  GDBusMethodInvocation* invocation;
} ExecData;

static void
exec_data_free (ExecData *data)
{
  g_object_unref (data->object);
  g_object_unref (data->invocation);
  g_free (data);
}

static void
exec_cb (TerminalScreen *screen, /* unused, may be %nullptr */
         GError *error, /* set on error, %nullptr on success */
         ExecData *data)
{
  /* Note: these calls transfer the ref */
  g_object_ref (data->invocation);
  if (error) {
    g_dbus_method_invocation_return_gerror (data->invocation, error);
  } else {
    terminal_receiver_complete_exec(TERMINAL_RECEIVER(data->object),
                                    data->invocation, nullptr /* outfdlist */);
  }
}

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
  gsize exec_argc;
  gs_free char **exec_argv = nullptr; /* container needs to be freed, strings not owned */
  gs_free char **envv = nullptr; /* container needs to be freed, strings not owned */
  gs_unref_variant GVariant *fd_array = nullptr;

  if (priv->screen == nullptr) {
    g_dbus_method_invocation_return_error_literal (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_FAILED,
                                                   "Terminal already closed");
    return TRUE; /* handled */
  }

  if (!g_variant_lookup (options, "cwd", "^&ay", &working_directory))
    working_directory = nullptr;
  if (!g_variant_lookup (options, "shell", "b", &shell))
    shell = FALSE;
  if (!g_variant_lookup (options, "environ", "^a&ay", &envv))
    envv = nullptr;
  if (!g_variant_lookup (options, "fd-set", "@a(ih)", &fd_array))
    fd_array = nullptr;

  /* Check environment */
  if (!terminal_util_check_envv((const char * const*)envv)) {
    g_dbus_method_invocation_return_error_literal (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_INVALID_ARGS,
                                                   "Malformed environment");
    return TRUE; /* handled */
  }

  /* Check FD passing */
  if ((fd_list != nullptr) ^ (fd_array != nullptr)) {
    g_dbus_method_invocation_return_error_literal (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_INVALID_ARGS,
                                                   "Must pass both fd-set options and a FD list");
    return TRUE; /* handled */
  }
  if (fd_list != nullptr && fd_array != nullptr) {
    const int *fd_array_data;
    gsize fd_array_data_len, i;
    int n_fds;

    fd_array_data = reinterpret_cast<int const*>
      (g_variant_get_fixed_array (fd_array, &fd_array_data_len, 2 * sizeof (int)));
    n_fds = g_unix_fd_list_get_length (fd_list);
    for (i = 0; i < fd_array_data_len; i++) {
      const int fd = fd_array_data[2 * i];
      const int idx = fd_array_data[2 * i + 1];

      if (fd == -1) {
        g_dbus_method_invocation_return_error (invocation,
                                               G_DBUS_ERROR,
                                               G_DBUS_ERROR_INVALID_ARGS,
                                               "Passing of invalid FD %d not supported", fd);
	return TRUE; /* handled */
      }
      if (fd == STDIN_FILENO ||
          fd == STDOUT_FILENO ||
          fd == STDERR_FILENO) {
        g_dbus_method_invocation_return_error (invocation,
                                               G_DBUS_ERROR,
                                               G_DBUS_ERROR_INVALID_ARGS,
                                               "Passing of std%s not supported",
                                               fd == STDIN_FILENO ? "in" : fd == STDOUT_FILENO ? "out" : "err");
	return TRUE; /* handled */
      }
      if (idx < 0 || idx >= n_fds) {
        g_dbus_method_invocation_return_error_literal (invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_INVALID_ARGS,
                                                       "Handle out of range");
        return TRUE; /* handled */
      }
    }
  }

  if (working_directory != nullptr)
    _terminal_debug_print (TERMINAL_DEBUG_SERVER,
                           "CWD is '%s'\n", working_directory);

  exec_argv = (char **) g_variant_get_bytestring_array (arguments, &exec_argc);

  ExecData *exec_data = g_new (ExecData, 1);
  exec_data->object = g_object_ref(receiver);
  /* We want to transfer the ownership of @invocation to ExecData here, but
   * we have to temporarily ref it so that in the error case below (where
   * terminal_screen_exec() frees the exec data via the supplied callback,
   * the g_dbus_method_invocation_take_error() calll still can take ownership
   * of the invocation's ref passed to this function (terminal_receiver_impl_exec()).
   */
  exec_data->invocation = (GDBusMethodInvocation*)g_object_ref (invocation);

  GError *err = nullptr;
  if (!terminal_screen_exec (priv->screen,
                             exec_argc > 0 ? exec_argv : nullptr,
                             envv,
                             shell,
                             working_directory,
                             fd_list, fd_array,
                             (TerminalScreenExecCallback) exec_cb,
                             exec_data /* adopted */,
                             (GDestroyNotify) exec_data_free,
                             nullptr /* cancellable */,
                             &err)) {
    /* Transfers ownership of @invocation */
    g_dbus_method_invocation_take_error (invocation, err);
  }

  /* Now we can remove that extra ref again. */
  g_object_unref (invocation);

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

  terminal_receiver_impl_set_screen (impl, nullptr);

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
      terminal_receiver_impl_set_screen (impl, (TerminalScreen*)g_value_get_object (value));
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
      g_param_spec_object ("screen", nullptr, nullptr,
                          TERMINAL_TYPE_SCREEN,
			   GParamFlags(G_PARAM_READWRITE |
				       G_PARAM_CONSTRUCT_ONLY |
				       G_PARAM_STATIC_STRINGS)));

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
  return reinterpret_cast<TerminalReceiverImpl*>
    (g_object_new (TERMINAL_TYPE_RECEIVER_IMPL,
		   "screen", screen,
		   nullptr));
}

/**
 * terminal_receiver_impl_get_screen:
 * @impl: a #TerminalReceiverImpl
 *
 * Returns: (transfer none): the impl's #TerminalScreen, or %nullptr
 */
TerminalScreen *
terminal_receiver_impl_get_screen (TerminalReceiverImpl *impl)
{
  g_return_val_if_fail (TERMINAL_IS_RECEIVER_IMPL (impl), nullptr);

  return impl->priv->screen;
}

/**
 * terminal_receiver_impl_unget_screen:
 * @impl: a #TerminalReceiverImpl
 *
 * Unsets the impls #TerminalScreen.
 */
void
terminal_receiver_impl_unset_screen (TerminalReceiverImpl *impl)
{
  g_return_if_fail (TERMINAL_IS_RECEIVER_IMPL (impl));

  terminal_receiver_impl_set_screen (impl, nullptr);
}

/* ---------------------------------------------------------------------------
 * TerminalFactoryImpl
 * ---------------------------------------------------------------------------
 */

struct _TerminalFactoryImplPrivate {
  gpointer dummy;
};

static gboolean
terminal_factory_impl_create_instance (TerminalFactory *factory,
                                       GDBusMethodInvocation *invocation,
                                       GVariant *options)
{
  TerminalApp *app = terminal_app_get ();

  /* If a parent screen is specified, use that to fill in missing information */
  TerminalScreen *parent_screen = nullptr;
  const char *parent_screen_object_path;
  if (g_variant_lookup (options, "parent-screen", "&o", &parent_screen_object_path)) {
    parent_screen = terminal_app_get_screen_by_object_path (app, parent_screen_object_path);
    if (parent_screen == nullptr) {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Failed to get screen from object path %s",
                                             parent_screen_object_path);
      return TRUE;
    }
  }

  /* Try getting a parent window, first by parent screen then by window ID;
   * if that fails, create a new window.
   */
  TerminalWindow *window = nullptr;
  gboolean have_new_window = FALSE;
  const char *window_from_screen_object_path;
  if (g_variant_lookup (options, "window-from-screen", "&o", &window_from_screen_object_path)) {
    TerminalScreen *window_screen =
      terminal_app_get_screen_by_object_path (app, window_from_screen_object_path);
    if (window_screen == nullptr) {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Failed to get screen from object path %s",
                                             parent_screen_object_path);
      return TRUE;
    }

    GtkWidget *win = gtk_widget_get_toplevel (GTK_WIDGET (window_screen));
    if (TERMINAL_IS_WINDOW (win))
      window = TERMINAL_WINDOW (win);
  }

  /* Support old client */
  guint window_id;
  if (window == nullptr && g_variant_lookup (options, "window-id", "u", &window_id)) {
    GtkWindow *win = gtk_application_get_window_by_id (GTK_APPLICATION (app), window_id);

    if (!TERMINAL_IS_WINDOW (win)) {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Nonexisting window %u referenced",
                                             window_id);
      return TRUE;
    }

    window = TERMINAL_WINDOW (win);
  }

  /* Still no parent window? Create a new one */
  if (window == nullptr) {
    const char *startup_id, *role;
    gboolean start_maximized, start_fullscreen;

    window = terminal_window_new (G_APPLICATION (app));
    have_new_window = TRUE;

    if (g_variant_lookup (options, "desktop-startup-id", "^&ay", &startup_id))
      gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);

    /* Overwrite the default, unique window role set in terminal_window_init */
    if (g_variant_lookup (options, "role", "&s", &role))
      gtk_window_set_role (GTK_WINDOW (window), role);

    gboolean show_menubar;
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

  g_assert_nonnull (window);

  const char *title;
  if (!g_variant_lookup (options, "title", "&s", &title))
    title = nullptr;

  double zoom;
  if (!g_variant_lookup (options, "zoom", "d", &zoom)) {
    if (parent_screen != nullptr)
      zoom = vte_terminal_get_font_scale (VTE_TERMINAL (parent_screen));
    else
      zoom = 1.0;
  }

  /* Look up the profile */
  gs_unref_object GSettings *profile = nullptr;
  const char *profile_uuid;
  if (!g_variant_lookup (options, "profile", "&s", &profile_uuid))
    profile_uuid = nullptr;

  if (profile_uuid == nullptr && parent_screen != nullptr) {
    profile = terminal_screen_ref_profile (parent_screen);
  } else {
    GError *err = nullptr;
    profile = terminal_profiles_list_ref_profile_by_uuid (terminal_app_get_profiles_list (app),
                                                          profile_uuid /* default if nullptr */,
                                                          &err);
    if (profile == nullptr) {
      g_dbus_method_invocation_return_gerror (invocation, err);
      g_error_free (err);
      return TRUE;
    }
  }

  g_assert_nonnull (profile);

  /* Now we can create the new screen */
  TerminalScreen *screen = terminal_screen_new (profile, title, zoom);
  terminal_window_add_screen (window, screen, -1);

  /* Apply window properties */
  gboolean active;
  if (g_variant_lookup (options, "active", "b", &active) &&
      active) {
    terminal_window_switch_screen (window, screen);
    gtk_widget_grab_focus (GTK_WIDGET (screen));
  }

  if (have_new_window) {
    const char *geometry;

    if (g_variant_lookup (options, "geometry", "&s", &geometry) &&
        !terminal_window_parse_geometry (window, geometry))
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "Invalid geometry string \"%s\"", geometry);
  }

  gboolean present_window;
  gboolean present_window_set = g_variant_lookup (options, "present-window", "b", &present_window);

  if (have_new_window || (present_window_set && present_window))
    gtk_window_present (GTK_WINDOW (window));

  gs_free char *object_path = terminal_app_dup_screen_object_path (app, screen);
  terminal_factory_complete_create_instance (factory, invocation, object_path);

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
  /* g_type_class_add_private (klass, sizeof (TerminalFactoryImplPrivate)); */
}

/**
 * terminal_factory_impl_new:
 *
 * Returns: (transfer full): a new #TerminalFactoryImpl
 */
TerminalFactory *
terminal_factory_impl_new (void)
{
  return reinterpret_cast<TerminalFactory*>
    (g_object_new (TERMINAL_TYPE_FACTORY_IMPL, nullptr));
}

/* ---------------------------------------------------------------------------
 * org.freedesktop.Terminal1 intent implementation
 * ---------------------------------------------------------------------------
 */

static void
exec_intent_cb(TerminalScreen *screen, /* unused, may be %nullptr */
               GError *error, /* set on error, %nullptr on success */
               ExecData *data)
{
  /* Note: these calls transfer the ref */
  g_object_ref(data->invocation);
  if (error) {
    g_dbus_method_invocation_return_gerror(data->invocation, error);
  } else {
    terminal_intent_complete_launch_command(TERMINAL_INTENT(data->object),
					    data->invocation);
  }
}

static gboolean
terminal_intent_impl_launch_command(TerminalIntent* intent,
				    GDBusMethodInvocation* invocation,
				    char const* const* argv,
				    char const* cwd,
				    char const* desktop_entry,
				    char const* const* envv,
				    GVariant* options,
				    GVariant* platform_data)
{
  TerminalApp *app = terminal_app_get ();

  if (!terminal_util_check_envv(envv)) {
    /* Transfers ownership of @invocation */
    g_dbus_method_invocation_return_error_literal(invocation,
						  G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
						  "Invalid envv array passed");
      return true;
  }

  /* Try to get some info from the passed desktop entry */
  gs_free char* desktop_cwd = nullptr;
  gs_free char* desktop_icon = nullptr;
  gs_free char* desktop_title = nullptr;
  gs_free char* desktop_wm_class = nullptr;
  gs_free char* desktop_profile_uuid = nullptr;

  if (desktop_entry[0]) {
    gs_free char* desktop_entry_utf8 = g_utf8_make_valid(desktop_entry, -1);
    gs_unref_key_file GKeyFile* key_file = g_key_file_new();

    gs_free_error GError* err = nullptr;
    if (!g_key_file_load_from_file(key_file,
                                   desktop_entry,
                                   GKeyFileFlags(G_KEY_FILE_NONE),
                                   &err)) {
      /* Transfers ownership of @invocation */
      g_dbus_method_invocation_return_error(invocation,
                                            G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                            "Failed to load desktop entry \"%s\": %s",
                                            desktop_entry_utf8, err->message);
      return true;
    }

    gs_free char* version = g_key_file_get_string(key_file,
                                                  G_KEY_FILE_DESKTOP_GROUP,
                                                  G_KEY_FILE_DESKTOP_KEY_VERSION,
                                                  nullptr);
    gs_free char* type = g_key_file_get_string(key_file,
                                               G_KEY_FILE_DESKTOP_GROUP,
                                               G_KEY_FILE_DESKTOP_KEY_TYPE,
                                               nullptr);
    auto const is_terminal = g_key_file_get_boolean(key_file,
                                                    G_KEY_FILE_DESKTOP_GROUP,
                                                    G_KEY_FILE_DESKTOP_KEY_TERMINAL,
                                                    nullptr);
    if (!version ||
        !g_str_equal(version, "1.0") ||
        !type ||
        !g_str_equal(type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) ||
        !is_terminal) {
      /* Transfers ownership of @invocation */
      g_dbus_method_invocation_return_error(invocation,
                                            G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                            "\"%s\" is not a valid desktop file of a terminal application",
                                            desktop_entry_utf8);
      return true;
    }


    desktop_cwd = g_key_file_get_string(key_file,
                                        G_KEY_FILE_DESKTOP_GROUP,
                                        G_KEY_FILE_DESKTOP_KEY_PATH,
                                        nullptr);

    desktop_icon = g_key_file_get_locale_string(key_file,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                G_KEY_FILE_DESKTOP_KEY_ICON,
                                                nullptr, // default locale
                                                nullptr);

    desktop_title = g_key_file_get_locale_string(key_file,
                                                 G_KEY_FILE_DESKTOP_GROUP,
                                                 G_KEY_FILE_DESKTOP_KEY_NAME,
                                                 nullptr, // default locale
                                                 nullptr);
    if (!desktop_title || !desktop_title[0]) {
      g_clear_pointer(&desktop_title, GDestroyNotify(g_free));
      desktop_title = g_key_file_get_locale_string(key_file,
                                                   G_KEY_FILE_DESKTOP_GROUP,
                                                   G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME,
                                                   nullptr, // default locale
                                                   nullptr);
    }

    desktop_wm_class = g_key_file_get_string(key_file,
                                             G_KEY_FILE_DESKTOP_GROUP,
                                             G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS,
                                             nullptr);

    desktop_profile_uuid = g_key_file_get_string(key_file,
                                                 G_KEY_FILE_DESKTOP_GROUP,
                                                 "X-GNOME-Profile-UUID",
                                                 nullptr);
  }

  auto option_keep = gboolean{false};
  (void)g_variant_lookup(options, "keep-terminal-open", "b", &option_keep);

  char const* option_profile_uuid = nullptr;
  (void)g_variant_lookup(options, "x-gnome-profile", "&s", &option_profile_uuid);

  GError* err = nullptr;
  gs_unref_object GSettings* profile =
    terminal_profiles_list_ref_profile_by_uuid(terminal_app_get_profiles_list (app),
                                               option_profile_uuid ? option_profile_uuid : desktop_profile_uuid, /* nullptr if default */
                                               &err);
  if (profile == nullptr) {
    g_dbus_method_invocation_take_error(invocation, err);
    return true;
  }

  /* Now open new window and terminal */
  auto window = terminal_window_new(G_APPLICATION(app));

  char const* startup_id = nullptr;
  if (g_variant_lookup(platform_data, "desktop-startup-id", "&s", &startup_id))
    gtk_window_set_startup_id(GTK_WINDOW(window), startup_id);

  if (desktop_wm_class) {
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
      gtk_window_set_wmclass(GTK_WINDOW(window), desktop_wm_class, desktop_wm_class);
#endif
  }

  auto const zoom = 1.0;

  auto const screen = terminal_screen_new(profile,
                                          desktop_title ? desktop_title : _("Terminal"),
                                          zoom);

  if (option_keep)
    terminal_screen_set_exit_action(screen, TERMINAL_EXIT_HOLD);

  terminal_window_add_screen(window, screen, -1);

  gtk_window_present(GTK_WINDOW(window));

  auto exec_data = g_new(ExecData, 1);
  exec_data->object = g_object_ref(intent);
  /* We want to transfer the ownership of @invocation to ExecData here, but
   * we have to temporarily ref it so that in the error case below (where
   * terminal_screen_exec() frees the exec data via the supplied callback,
   * the g_dbus_method_invocation_take_error() calll still can take ownership
   * of the invocation's ref passed to this function (terminal_receiver_impl_exec()).
   */
  exec_data->invocation = (GDBusMethodInvocation*)g_object_ref (invocation);

  if (!terminal_screen_exec(screen,
                            argv,
                            envv,
                            false /* shell */,
                            cwd[0] ? cwd : desktop_cwd ? desktop_cwd : nullptr,
                            nullptr, nullptr,
                            TerminalScreenExecCallback(exec_intent_cb),
                            exec_data /* adopted */,
                            GDestroyNotify(exec_data_free),
                            nullptr /* cancellable */,
                            &err)) {
    /* Transfers ownership of @invocation */
    g_dbus_method_invocation_take_error(invocation, err);
  }

  /* Now we can remove that extra ref again. */
  g_object_unref (invocation);

  return true;
}

static void
terminal_intent_impl_iface_init (TerminalIntentIface *iface)
{
  iface->handle_launch_command = terminal_intent_impl_launch_command;
}

G_DEFINE_TYPE_WITH_CODE (TerminalIntentImpl, terminal_intent_impl, TERMINAL_TYPE_INTENT_SKELETON,
                         G_IMPLEMENT_INTERFACE (TERMINAL_TYPE_INTENT, terminal_intent_impl_iface_init))

static void
terminal_intent_impl_init (TerminalIntentImpl *impl)
{
}

static void
terminal_intent_impl_class_init (TerminalIntentImplClass *klass)
{
}

/**
 * terminal_intent_impl_new:
 *
 * Returns: (transfer full): a new #TerminalIntentImpl
 */
TerminalIntent *
terminal_intent_impl_new (void)
{
  return reinterpret_cast<TerminalIntent*>
    (g_object_new (TERMINAL_TYPE_INTENT_IMPL, nullptr));
}
