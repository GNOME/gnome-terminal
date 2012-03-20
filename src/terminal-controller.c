/*
 * Copyright © 2011 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope controller it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "terminal-controller.h"
#include "terminal-debug.h"

#define TERMINAL_CONTROLLER_GET_PRIVATE(controller)(G_TYPE_INSTANCE_GET_PRIVATE ((controller), TERMINAL_TYPE_CONTROLLER, TerminalControllerPrivate))

struct _TerminalControllerPrivate {
  TerminalScreen *screen; /* unowned! */
};

enum {
  PROP_0,
  PROP_SCREEN
};

/* helper functions */

static void
child_exited_cb (VteTerminal *terminal,
                 TerminalReceiver *receiver)
{
  int exit_code;

  exit_code = vte_terminal_get_child_exit_status (terminal);;

  terminal_receiver_emit_child_exited (receiver, exit_code);
}

static void
terminal_controller_set_screen (TerminalController *controller,
                                TerminalScreen *screen)
{
  TerminalControllerPrivate *priv;

  g_return_if_fail (TERMINAL_IS_CONTROLLER (controller));
  g_return_if_fail (screen == NULL || TERMINAL_IS_SCREEN (screen));

  priv = controller->priv;
  if (priv->screen == screen)
    return;

  if (priv->screen) {
    g_signal_handlers_disconnect_matched (priv->screen,
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, controller);
  }

  priv->screen = screen;
  if (screen) {
    g_signal_connect (screen, "child-exited",
                      G_CALLBACK (child_exited_cb), 
                      controller);
    g_signal_connect_swapped (screen, "destroy",
                              G_CALLBACK (_terminal_controller_unset_screen), 
                              controller);
  }

  g_object_notify (G_OBJECT (controller), "screen");
}

/* Class implementation */

static gboolean 
terminal_controller_exec (TerminalReceiver *receiver,
                          GDBusMethodInvocation *invocation,
                          GUnixFDList *fd_list,
                          GVariant *options,
                          GVariant *arguments)
{
  TerminalController *controller = TERMINAL_CONTROLLER (receiver);
  TerminalControllerPrivate *priv = controller->priv;
  const char *working_directory;
  char **exec_argv, **envv;
  gsize exec_argc;
  GVariantIter *fd_iter;
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
  if (!g_variant_lookup (options, "environ", "^a&ay", &envv))
    envv = NULL;
  if (!g_variant_lookup (options, "fd-set", "a(ih)", &fd_iter))
    fd_iter = NULL;

  if (working_directory != NULL)
    _terminal_debug_print (TERMINAL_DEBUG_FACTORY,
                           "CWD is '%s'\n", working_directory);

  exec_argv = (char **) g_variant_get_bytestring_array (arguments, &exec_argc);

  error = NULL;
  if (!terminal_screen_exec (priv->screen,
                             exec_argc > 0 ? exec_argv : NULL,
                             envv,
                             working_directory,
                             &error)) {
    g_dbus_method_invocation_take_error (invocation, error);
  } else {
    terminal_receiver_complete_exec (receiver, invocation, NULL /* outfdlist */);
  }

  g_free (exec_argv);
  g_free (envv);
  if (fd_iter)
    g_variant_iter_free (fd_iter);

out:

  return TRUE; /* handled */
}

static void
terminal_controller_iface_init (TerminalReceiverIface *iface)
{
  iface->handle_exec = terminal_controller_exec;
}

G_DEFINE_TYPE_WITH_CODE (TerminalController, terminal_controller, TERMINAL_TYPE_RECEIVER_SKELETON,
                         G_IMPLEMENT_INTERFACE (TERMINAL_TYPE_RECEIVER, terminal_controller_iface_init))

static void
terminal_controller_init (TerminalController *controller)
{
  controller->priv = TERMINAL_CONTROLLER_GET_PRIVATE (controller);
}

static void
terminal_controller_dispose (GObject *object)
{
  TerminalController *controller = TERMINAL_CONTROLLER (object);

  terminal_controller_set_screen (controller, NULL);

  G_OBJECT_CLASS (terminal_controller_parent_class)->dispose (object);
}

static void
terminal_controller_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  TerminalController *controller = TERMINAL_CONTROLLER (object);

  switch (prop_id) {
    case PROP_SCREEN:
      g_value_set_object (value, terminal_controller_get_screen (controller));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_controller_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  TerminalController *controller = TERMINAL_CONTROLLER (object);

  switch (prop_id) {
    case PROP_SCREEN:
      terminal_controller_set_screen (controller, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_controller_class_init (TerminalControllerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = terminal_controller_dispose;
  gobject_class->get_property = terminal_controller_get_property;
  gobject_class->set_property = terminal_controller_set_property;

  g_object_class_install_property
     (gobject_class,
      PROP_SCREEN,
      g_param_spec_object ("screen", NULL, NULL,
                          TERMINAL_TYPE_SCREEN,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (gobject_class, sizeof (TerminalControllerPrivate));
}

/* public API */

/**
 * terminal_controller_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalController for @screen
 */
TerminalController *
terminal_controller_new (TerminalScreen *screen)
{
  return g_object_new (TERMINAL_TYPE_CONTROLLER, 
                       "screen", screen, 
                       NULL);
}

/**
 * terminal_controller_get_screen:
 * @controller: a #TerminalController
 * 
 * Returns: (transfer none): the controller's #TerminalScreen, or %NULL
 */
TerminalScreen *
terminal_controller_get_screen (TerminalController *controller)
{
  g_return_val_if_fail (TERMINAL_IS_CONTROLLER (controller), NULL);

  return controller->priv->screen;
}

/**
 * terminal_controller_get_screen:
 * @controller: a #TerminalController
 * 
 * Unsets the controllers #TerminalScreen.
 */
void
_terminal_controller_unset_screen (TerminalController *controller)
{
  g_return_if_fail (TERMINAL_IS_CONTROLLER (controller));

  terminal_controller_set_screen (controller, NULL);
}