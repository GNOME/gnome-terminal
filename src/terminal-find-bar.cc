/*
 * terminal-find-bar.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "terminal-find-bar.hh"

#include "terminal-pcre2.hh"
#include "terminal-util.hh"

#include <glib/gi18n.h>

struct _TerminalFindBar
{
  GtkWidget        parent_instance;

  TerminalScreen  *screen;

  GtkEntry        *entry;
  GtkCheckButton  *use_regex;
  GtkCheckButton  *whole_words;
  GtkCheckButton  *match_case;
};

enum {
  PROP_0,
  PROP_SCREEN,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalFindBar, terminal_find_bar, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
terminal_find_bar_dismiss (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  TerminalFindBar *self = (TerminalFindBar *)widget;
  GtkWidget *revealer;

  g_assert (TERMINAL_IS_FIND_BAR (self));

  if ((revealer = gtk_widget_get_ancestor (widget, GTK_TYPE_REVEALER)))
    gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);

  if (self->screen != nullptr)
    gtk_widget_grab_focus (GTK_WIDGET (self->screen));
}

static gboolean
terminal_find_bar_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (TERMINAL_FIND_BAR (widget)->entry));
}

static void
terminal_find_bar_update_regex(TerminalFindBar* self)
{
  g_assert (TERMINAL_IS_FIND_BAR (self));

  auto text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  g_autoptr(VteRegex) regex = nullptr;
  g_autoptr(GError) error = nullptr;
  gsize regex_error_offset = 0;
  if (!terminal_str_empty0 (text)) {
    uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE;
    uint32_t extra_flags = 0;

    if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->match_case)))
      flags |= PCRE2_CASELESS;

    g_autofree char *escaped = nullptr;
    if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->use_regex)))
      text = escaped = g_regex_escape_string (text, -1);

    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->whole_words)))
      extra_flags |= PCRE2_EXTRA_MATCH_WORD;

    regex = vte_regex_new_for_search_full(text, -1, flags, extra_flags,
                                          &regex_error_offset, &error);

    if (regex) {
      vte_regex_jit(regex, PCRE2_JIT_COMPLETE, nullptr);
      vte_regex_jit(regex, PCRE2_JIT_PARTIAL_SOFT, nullptr);
    }
  }

  if (error) {
    gtk_widget_add_css_class(GTK_WIDGET(self->entry), "error");

    auto const tooltip = error->message;
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->entry), tooltip);
  } else {
    vte_terminal_search_set_regex (VTE_TERMINAL (self->screen), regex, 0);
    gtk_widget_remove_css_class(GTK_WIDGET(self->entry), "error");
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->entry), nullptr);
  }

  vte_terminal_search_set_wrap_around (VTE_TERMINAL (self->screen), true);
}

static void
terminal_find_bar_next (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  TerminalFindBar *self = (TerminalFindBar *)widget;

  g_assert (TERMINAL_IS_FIND_BAR (self));

  if (self->screen != nullptr)
    vte_terminal_search_find_next (VTE_TERMINAL (self->screen));
}

static void
terminal_find_bar_previous (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  TerminalFindBar *self = (TerminalFindBar *)widget;

  g_assert (TERMINAL_IS_FIND_BAR (self));

  if (self->screen != nullptr)
    vte_terminal_search_find_previous (VTE_TERMINAL (self->screen));
}

static void
terminal_find_bar_entry_changed_cb (TerminalFindBar *self,
                                    GtkEntry      *entry)
{
  terminal_find_bar_update_regex(self);
}

static void
terminal_find_bar_dispose (GObject *object)
{
  TerminalFindBar *self = (TerminalFindBar *)object;
  GtkWidget *child;

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_FIND_BAR);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->screen);

  G_OBJECT_CLASS (terminal_find_bar_parent_class)->dispose (object);
}

static void
terminal_find_bar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  TerminalFindBar *self = TERMINAL_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, self->screen);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
terminal_find_bar_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  TerminalFindBar *self = TERMINAL_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      self->screen = reinterpret_cast<TerminalScreen*>(g_value_dup_object(value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
terminal_find_bar_class_init (TerminalFindBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = terminal_find_bar_dispose;
  object_class->get_property = terminal_find_bar_get_property;
  object_class->set_property = terminal_find_bar_set_property;

  widget_class->grab_focus = terminal_find_bar_grab_focus;

  properties[PROP_SCREEN] =
    g_param_spec_object ("screen", nullptr, nullptr,
                         TERMINAL_TYPE_SCREEN,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_EXPLICIT_NOTIFY |
                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/find-bar.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "findbar");

  gtk_widget_class_bind_template_child (widget_class, TerminalFindBar, entry);
  gtk_widget_class_bind_template_child (widget_class, TerminalFindBar, use_regex);
  gtk_widget_class_bind_template_child (widget_class, TerminalFindBar, whole_words);
  gtk_widget_class_bind_template_child (widget_class, TerminalFindBar, match_case);

  gtk_widget_class_bind_template_callback (widget_class, terminal_find_bar_entry_changed_cb);

  gtk_widget_class_install_action (widget_class, "search.dismiss", nullptr, terminal_find_bar_dismiss);
  gtk_widget_class_install_action (widget_class, "search.down", nullptr, terminal_find_bar_next);
  gtk_widget_class_install_action (widget_class, "search.up", nullptr, terminal_find_bar_previous);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, GdkModifierType(0), "search.dismiss", nullptr);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GDK_CONTROL_MASK, "search.up", nullptr);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_g, GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK), "search.down", nullptr);
}

static void
terminal_find_bar_init (TerminalFindBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

TerminalScreen *
terminal_find_bar_get_screen (TerminalFindBar *self)
{
  g_return_val_if_fail (TERMINAL_IS_FIND_BAR (self), nullptr);

  return self->screen;
}

void
terminal_find_bar_set_screen (TerminalFindBar  *self,
                              TerminalScreen* screen)
{
  g_return_if_fail (TERMINAL_IS_FIND_BAR (self));
  g_return_if_fail (!screen || TERMINAL_IS_SCREEN (screen));

  if (g_set_object (&self->screen, screen))
    {
      gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCREEN]);
    }
}
