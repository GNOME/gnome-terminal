/*
 * Copyright (C) 2016 Endless, Inc
 * Copyright (C) 2017-2023 Christian Hergert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *          Christian Hergert <chergert@redhat.com>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "terminal-accel-dialog.hh"

struct _TerminalAccelDialog
{
  AdwWindow             parent_instance;

  GtkStack             *stack;
  GtkLabel             *display_label;
  GtkShortcutLabel     *display_shortcut;
  GtkLabel             *selection_label;

  char                 *shortcut_title;

  guint                 keyval;
  GdkModifierType       modifier;

  guint                 first_modifier;

  guint                 editing : 1;
};

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_SHORTCUT_TITLE,
  N_PROPS
};

enum {
  SHORTCUT_SET,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (TerminalAccelDialog, terminal_accel_dialog, ADW_TYPE_WINDOW)

static gboolean
terminal_accel_dialog_is_editing (TerminalAccelDialog *self)
{
  g_assert (TERMINAL_IS_ACCEL_DIALOG (self));

  return self->editing;
}

static void
terminal_accel_dialog_apply_state (TerminalAccelDialog *self)
{
  g_assert (TERMINAL_IS_ACCEL_DIALOG (self));

  if (self->editing)
    {
      gtk_stack_set_visible_child_name (self->stack, "selection");
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "shortcut.set", FALSE);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "display");
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "shortcut.set", TRUE);
    }
}

static GdkModifierType
sanitize_modifier_mask (GdkModifierType mods)
{
  return GdkModifierType(mods &
                         gtk_accelerator_get_default_mod_mask () &
                         ~GDK_LOCK_MASK);
}

static bool
should_drop_shift(unsigned keyval_was,
                  unsigned keyval_is)
{
  if (keyval_was == keyval_is) {
    // Allow use of shift+arrow. See prompt#55
    if (keyval_was == GDK_KEY_Left ||
        keyval_was == GDK_KEY_Right ||
        keyval_was == GDK_KEY_Up ||
        keyval_was == GDK_KEY_Down)
      return false;

    return true;
  }

  return false;
}

static gboolean
terminal_accel_dialog_key_pressed (GtkWidget             *widget,
                                   guint                  keyval,
                                   guint                  keycode,
                                   GdkModifierType        state,
                                   GtkEventControllerKey *controller)
{
  TerminalAccelDialog *self = (TerminalAccelDialog *)widget;

  g_assert (TERMINAL_IS_ACCEL_DIALOG (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

  if (terminal_accel_dialog_is_editing (self))
    {
      GdkEvent *key = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
      GdkModifierType real_mask;
      guint keyval_lower;

      if (gdk_key_event_is_modifier (key))
        {
          if (self->keyval == 0 && self->modifier == 0)
            self->first_modifier = keyval;
          return GDK_EVENT_PROPAGATE;
        }

      real_mask = GdkModifierType(state & gtk_accelerator_get_default_mod_mask ());
      keyval_lower = gdk_keyval_to_lower (keyval);

      /* Normalize <Tab> */
      if (keyval_lower == GDK_KEY_ISO_Left_Tab)
        keyval_lower = GDK_KEY_Tab;

      /* Put shift back if it changed the case of the key */
      if (keyval_lower != keyval)
        real_mask = GdkModifierType(real_mask | GDK_SHIFT_MASK);

      /* We don't want to use SysRq as a keybinding but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq
       */
      if (keyval_lower == GDK_KEY_Sys_Req && (real_mask & GDK_ALT_MASK) != 0)
        keyval_lower = GDK_KEY_Print;

      /* A single Escape press cancels the editing */
      if (!gdk_key_event_is_modifier (key) &&
          real_mask == 0 &&
          keyval_lower == GDK_KEY_Escape)
        {
          gtk_window_close (GTK_WINDOW (self));
          return GDK_EVENT_STOP;
        }

      /* Backspace disables the current shortcut */
      if (real_mask == 0 && keyval_lower == GDK_KEY_BackSpace)
        {
          terminal_accel_dialog_set_accelerator (self, nullptr);
          gtk_widget_activate_action (GTK_WIDGET (self), "shortcut.set", nullptr);
          return GDK_EVENT_STOP;
        }

      self->keyval = gdk_keyval_to_lower (keyval);
      self->modifier = sanitize_modifier_mask (state);

      if ((state & GDK_SHIFT_MASK) != 0 && should_drop_shift (self->keyval, keyval))
        self->modifier = GdkModifierType(self->modifier & ~GDK_SHIFT_MASK);

      if ((state & GDK_LOCK_MASK) == 0 &&
          self->keyval != keyval)
        self->modifier = GdkModifierType(self->modifier | GDK_SHIFT_MASK);

      self->editing = FALSE;

      terminal_accel_dialog_apply_state (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
terminal_accel_dialog_key_released (GtkWidget             *widget,
                                    guint                  keyval,
                                    guint                  keycode,
                                    GdkModifierType        state,
                                    GtkEventControllerKey *controller)
{
  TerminalAccelDialog *self = (TerminalAccelDialog *)widget;

  g_assert (TERMINAL_IS_ACCEL_DIALOG (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

  if (self->editing)
    {
      GdkEvent *key = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));
      /*
       * If we have a chord defined and there was no modifier,
       * then any key release should be enough for us to cancel
       * our grab.
       */
      if (self->modifier == 0)
        {
          self->editing = FALSE;
          terminal_accel_dialog_apply_state (self);
          return;
        }

      /*
       * If we started our sequence with a modifier, we want to
       * release our grab when that modifier has been released.
       */
      if (gdk_key_event_is_modifier (key) &&
          self->keyval != 0 &&
          self->first_modifier != 0 &&
          self->first_modifier == keyval)
        {
          self->editing = FALSE;
          self->first_modifier = 0;
          terminal_accel_dialog_apply_state (self);
          return;
        }
    }
}

static void
shortcut_set_cb (GtkWidget  *widget,
                 const char *action_name,
                 GVariant   *param)
{
  TerminalAccelDialog *self = TERMINAL_ACCEL_DIALOG (widget);

  g_signal_emit (self, signals [SHORTCUT_SET], 0,
                 terminal_accel_dialog_get_accelerator (self));

  gtk_window_close (GTK_WINDOW (self));
}

static void
terminal_accel_dialog_constructed (GObject *object)
{
  TerminalAccelDialog *self = (TerminalAccelDialog *)object;

  G_OBJECT_CLASS (terminal_accel_dialog_parent_class)->constructed (object);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "shortcut.set", FALSE);
}

static void
terminal_accel_dialog_finalize (GObject *object)
{
  TerminalAccelDialog *self = (TerminalAccelDialog *)object;

  g_clear_pointer (&self->shortcut_title, g_free);

  G_OBJECT_CLASS (terminal_accel_dialog_parent_class)->finalize (object);
}

static void
terminal_accel_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  TerminalAccelDialog *self = TERMINAL_ACCEL_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_take_string (value, terminal_accel_dialog_get_accelerator (self));
      break;

    case PROP_SHORTCUT_TITLE:
      g_value_set_string (value, terminal_accel_dialog_get_shortcut_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
terminal_accel_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  TerminalAccelDialog *self = TERMINAL_ACCEL_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      terminal_accel_dialog_set_accelerator (self, g_value_get_string (value));
      break;

    case PROP_SHORTCUT_TITLE:
      terminal_accel_dialog_set_shortcut_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
terminal_accel_dialog_class_init (TerminalAccelDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = terminal_accel_dialog_constructed;
  object_class->finalize = terminal_accel_dialog_finalize;
  object_class->get_property = terminal_accel_dialog_get_property;
  object_class->set_property = terminal_accel_dialog_set_property;

  properties [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator",
                         "Accelerator",
                         "Accelerator",
                         nullptr,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHORTCUT_TITLE] =
    g_param_spec_string ("shortcut-title",
                         "Title",
                         "Title",
                         nullptr,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  signals[SHORTCUT_SET] =
    g_signal_new ("shortcut-set",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  nullptr, nullptr, nullptr,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/accel-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalAccelDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, TerminalAccelDialog, selection_label);
  gtk_widget_class_bind_template_child (widget_class, TerminalAccelDialog, display_label);
  gtk_widget_class_bind_template_child (widget_class, TerminalAccelDialog, display_shortcut);
  gtk_widget_class_bind_template_callback (widget_class, terminal_accel_dialog_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, terminal_accel_dialog_key_released);

  gtk_widget_class_install_action (widget_class, "shortcut.set", nullptr, shortcut_set_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, GdkModifierType(0), "window.close", nullptr);
}

static void
terminal_accel_dialog_init (TerminalAccelDialog *self)
{
  self->editing = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property (self, "accelerator",
                          self->display_shortcut, "accelerator",
                          G_BINDING_SYNC_CREATE);

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif
}

gchar *
terminal_accel_dialog_get_accelerator (TerminalAccelDialog *self)
{
  g_return_val_if_fail (TERMINAL_IS_ACCEL_DIALOG (self), nullptr);

  if (self->keyval == 0)
    return nullptr;

  return gtk_accelerator_name (self->keyval, self->modifier);
}

void
terminal_accel_dialog_set_accelerator (TerminalAccelDialog *self,
                                       const gchar         *accelerator)
{
  guint keyval;
  GdkModifierType state;

  g_return_if_fail (TERMINAL_IS_ACCEL_DIALOG (self));

  if (accelerator == nullptr)
    {
      if (self->keyval != 0 || self->modifier != 0)
        {
          self->keyval = 0;
          self->modifier = GdkModifierType(0);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
        }
    }
  else if (gtk_accelerator_parse (accelerator, &keyval, &state))
    {
      if (keyval != self->keyval || state != self->modifier)
        {
          self->keyval = keyval;
          self->modifier = state;
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACCELERATOR]);
        }
    }
}

void
terminal_accel_dialog_set_shortcut_title (TerminalAccelDialog *self,
                                          const gchar         *shortcut_title)
{
  g_autofree gchar *label = nullptr;

  g_return_if_fail (TERMINAL_IS_ACCEL_DIALOG (self));

  if (shortcut_title != nullptr)
    {
      /* Translators: <b>%s</b> is used to show the provided text in bold */
      label = g_strdup_printf (_("Enter new shortcut to change <b>%s</b>."), shortcut_title);
    }

  if (g_set_str (&self->shortcut_title, shortcut_title))
    {
      gtk_label_set_label (self->selection_label, label);
      gtk_label_set_label (self->display_label, label);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHORTCUT_TITLE]);
    }
}

const gchar *
terminal_accel_dialog_get_shortcut_title (TerminalAccelDialog *self)
{
  g_return_val_if_fail (TERMINAL_IS_ACCEL_DIALOG (self), nullptr);

  return self->shortcut_title;
}

GtkWidget *
terminal_accel_dialog_new (void)
{
  return (GtkWidget*)g_object_new (TERMINAL_TYPE_ACCEL_DIALOG, nullptr);
}

