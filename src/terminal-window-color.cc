/*
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <math.h>

#include "terminal-schemas.hh"
#include "terminal-window-color.hh"
#include "terminal-util.hh"

struct _TerminalWindowColor
{
  GObject parent_instance;

  GWeakRef screen_wr;
  GWeakRef window_wr;

  GtkCssProvider *css_provider;

  GSettings *profile;

  char *css_class;

  gulong notify_profile_handler;
  gulong changed_handler;

  guint queued_update;

  double opacity;
  GdkRGBA background;
  GdkRGBA foreground;
  guint style_window : 1;
};

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_WINDOW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (TerminalWindowColor, terminal_window_color, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];
static guint last_sequence;

static gboolean
rgba_is_dark (const GdkRGBA *rgba)
{
  /* http://alienryderflex.com/hsp.html */
  double r = rgba->red * 255.0;
  double g = rgba->green * 255.0;
  double b = rgba->blue * 255.0;
  double hsp = sqrt (0.299 * (r * r) +
                     0.587 * (g * g) +
                     0.114 * (b * b));

  return hsp <= 127.5;
}

static void
terminal_window_color_update (TerminalWindowColor *self)
{
  g_autoptr(GString) string = nullptr;

  g_assert (TERMINAL_IS_WINDOW_COLOR (self));

  string = g_string_new (nullptr);

  if (self->style_window) {
    g_autoptr(TerminalScreen) screen = nullptr;
    g_autoptr(GString) gstring = g_string_new (nullptr);
    g_autofree char *bg = nullptr;
    g_autofree char *fg = nullptr;
    char header_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
    char window_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
    char popover_alpha_str[G_ASCII_DTOSTR_BUF_SIZE];
    double window_alpha;
    double header_alpha;
    double popover_alpha;
    GdkRGBA bg_rgba;
    GdkRGBA fg_rgba;

    screen = terminal_window_color_dup_screen (self);

    if (screen != nullptr) {
      vte_terminal_get_color_background_for_draw (VTE_TERMINAL (screen), &bg_rgba);
      terminal_screen_get_foreground (screen, &fg_rgba);
    } else {
      bg_rgba = self->background;
      fg_rgba = self->foreground;
    }

    bg = gdk_rgba_to_string (&bg_rgba);
    fg = gdk_rgba_to_string (&fg_rgba);

    window_alpha = self->opacity;
    header_alpha = .25;
    popover_alpha = MAX (window_alpha, 0.95);

    g_ascii_dtostr (header_alpha_str, sizeof header_alpha_str, header_alpha);
    g_ascii_dtostr (popover_alpha_str, sizeof popover_alpha_str, popover_alpha);
    g_ascii_dtostr (window_alpha_str, sizeof window_alpha_str, window_alpha);

    g_string_append_printf (string,
                            "window.%s { color: %s; background: alpha(%s, %s); }\n",
                            self->css_class, fg, bg, window_alpha_str);
    g_string_append_printf (string,
                            "window.%s popover > contents { color: %s; background: alpha(%s, %s); }\n",
                            self->css_class, fg, bg, popover_alpha_str);
    g_string_append_printf (string,
                            "window.%s popover > arrow { background: alpha(%s, %s); }\n",
                            self->css_class, bg, popover_alpha_str);
    g_string_append_printf (string,
                            "window.%s vte-terminal > revealer.size label { color: %s; background-color: alpha(%s, %s); }\n",
                            self->css_class, fg, bg, popover_alpha_str);

    if (rgba_is_dark (&bg_rgba))
      g_string_append_printf (string,
                              "window.%s toolbarview > revealer > windowhandle { color: %s; background: alpha(#fff, .05); }\n",
                              self->css_class, fg);
    else
      g_string_append_printf (string,
                              "window.%s toolbarview > revealer > windowhandle { color: %s; background: alpha(#000, .1); }\n",
                              self->css_class, fg);
  }

  gtk_css_provider_load_from_string (self->css_provider, string->str);
}

static gboolean
terminal_window_color_update_idle (gpointer user_data)
{
  TerminalWindowColor *self = TERMINAL_WINDOW_COLOR (user_data);

  self->queued_update = 0;

  terminal_window_color_update (self);

  return G_SOURCE_REMOVE;
}

static void
terminal_window_color_queue_update (TerminalWindowColor *self)
{
  g_assert (TERMINAL_IS_WINDOW_COLOR (self));

  if (self->queued_update == 0)
    self->queued_update = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                           terminal_window_color_update_idle,
                                           self, nullptr);
}

static void
terminal_window_color_set_window (TerminalWindowColor *self,
                                  TerminalWindow       *window)
{
  g_assert (TERMINAL_IS_WINDOW_COLOR (self));
  g_assert (TERMINAL_IS_WINDOW (window));

  g_weak_ref_set (&self->window_wr, window);

  if (self->opacity < 1.0)
    gtk_widget_add_css_class (GTK_WIDGET (window), self->css_class);
}

static void
terminal_window_color_constructed (GObject *object)
{
  TerminalWindowColor *self = (TerminalWindowColor *)object;

  G_OBJECT_CLASS (terminal_window_color_parent_class)->constructed (object);

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (self->css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);

  terminal_window_color_queue_update (self);
}

static void
terminal_window_color_dispose (GObject *object)
{
  TerminalWindowColor *self = (TerminalWindowColor *)object;

  terminal_window_color_set_screen (self, nullptr);

  if (self->css_provider != nullptr) {
    gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                   GTK_STYLE_PROVIDER (self->css_provider));
    g_clear_object (&self->css_provider);
  }

  g_weak_ref_set (&self->screen_wr, nullptr);
  g_weak_ref_set (&self->window_wr, nullptr);

  g_assert (self->profile == nullptr);
  g_assert (self->notify_profile_handler == 0);
  g_assert (self->changed_handler == 0);

  g_clear_handle_id (&self->queued_update, g_source_remove);

  G_OBJECT_CLASS (terminal_window_color_parent_class)->dispose (object);
}

static void
terminal_window_color_finalize (GObject *object)
{
  TerminalWindowColor *self = (TerminalWindowColor *)object;

  g_weak_ref_clear (&self->screen_wr);
  g_weak_ref_clear (&self->window_wr);

  g_clear_pointer (&self->css_class, g_free);

  G_OBJECT_CLASS (terminal_window_color_parent_class)->finalize (object);
}

static void
terminal_window_color_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  TerminalWindowColor *self = TERMINAL_WINDOW_COLOR (object);

  switch (prop_id) {
  case PROP_SCREEN:
    g_value_take_object (value, terminal_window_color_dup_screen (self));
    break;

  case PROP_WINDOW:
    g_value_take_object (value, terminal_window_color_dup_window (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_window_color_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  TerminalWindowColor *self = TERMINAL_WINDOW_COLOR (object);

  switch (prop_id) {
  case PROP_SCREEN:
    terminal_window_color_set_screen (self, TERMINAL_SCREEN (g_value_get_object (value)));
    break;

  case PROP_WINDOW:
    terminal_window_color_set_window (self, TERMINAL_WINDOW (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_window_color_class_init (TerminalWindowColorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = terminal_window_color_constructed;
  object_class->dispose = terminal_window_color_dispose;
  object_class->finalize = terminal_window_color_finalize;
  object_class->get_property = terminal_window_color_get_property;
  object_class->set_property = terminal_window_color_set_property;

  properties [PROP_SCREEN] =
    g_param_spec_object ("screen", nullptr, nullptr,
                         TERMINAL_TYPE_SCREEN,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_EXPLICIT_NOTIFY |
                                     G_PARAM_STATIC_STRINGS));

  properties [PROP_WINDOW] =
    g_param_spec_object ("window", nullptr, nullptr,
                         TERMINAL_TYPE_WINDOW,
                         GParamFlags(G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
terminal_window_color_init (TerminalWindowColor *self)
{
  self->css_provider = gtk_css_provider_new ();
  self->css_class = g_strdup_printf ("window-color-%u", ++last_sequence);
  self->opacity = 1.0;

  g_weak_ref_init (&self->window_wr, nullptr);
  g_weak_ref_init (&self->screen_wr, nullptr);
}

TerminalWindow *
terminal_window_color_dup_window (TerminalWindowColor *self)
{
  g_return_val_if_fail (TERMINAL_IS_WINDOW_COLOR (self), nullptr);

  return TERMINAL_WINDOW (g_weak_ref_get (&self->window_wr));
}

TerminalScreen *
terminal_window_color_dup_screen (TerminalWindowColor *self)
{
  g_return_val_if_fail (TERMINAL_IS_WINDOW_COLOR (self), nullptr);

  return TERMINAL_SCREEN (g_weak_ref_get (&self->screen_wr));
}

TerminalWindowColor *
terminal_window_color_new (TerminalWindow *window)
{
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), nullptr);

  return (TerminalWindowColor *)g_object_new (TERMINAL_TYPE_WINDOW_COLOR,
                                               "window", window,
                                               nullptr);
}

static void
terminal_window_color_profile_changed_cb (TerminalWindowColor *self,
                                          const char          *key,
                                          GSettings           *profile)
{
  static const char *useful_keys[] = {
    TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
    TERMINAL_PROFILE_FOREGROUND_COLOR_KEY,
    TERMINAL_PROFILE_STYLE_WINDOW_KEY,
    TERMINAL_PROFILE_OPACITY_KEY,
    TERMINAL_PROFILE_USE_THEME_COLORS_KEY,
    TERMINAL_PROFILE_PALETTE_KEY,
    nullptr
  };

  g_autoptr(TerminalScreen) screen = nullptr;
  g_autoptr(TerminalWindow) window = nullptr;

  g_assert (TERMINAL_IS_WINDOW_COLOR (self));
  g_assert (G_IS_SETTINGS (profile));

  if (key && !g_strv_contains (useful_keys, key))
    return;

  window = terminal_window_color_dup_window (self);
  screen = terminal_window_color_dup_screen (self);

  if (!key || g_str_equal (key, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY))
    terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &self->background);

  if (!key || g_str_equal (key, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY))
    terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &self->foreground);

  if (!key || g_str_equal (key, TERMINAL_PROFILE_STYLE_WINDOW_KEY)) {
    self->style_window = g_settings_get_boolean (profile, TERMINAL_PROFILE_STYLE_WINDOW_KEY);

    if (window != nullptr) {
      if (self->style_window)
        gtk_widget_add_css_class (GTK_WIDGET (window), self->css_class);
      else
        gtk_widget_remove_css_class (GTK_WIDGET (window), self->css_class);
    }

    if (screen != nullptr)
      vte_terminal_set_clear_background (VTE_TERMINAL (screen), !self->style_window);
  }

  if (!key || g_str_equal (key, TERMINAL_PROFILE_OPACITY_KEY))
    self->opacity = g_settings_get_double (profile, TERMINAL_PROFILE_OPACITY_KEY);

  terminal_window_color_queue_update (self);
}

static void
terminal_window_color_set_profile (TerminalWindowColor *self,
                                   GSettings            *profile)
{

  g_assert (TERMINAL_IS_WINDOW_COLOR (self));
  g_assert (!profile || G_IS_SETTINGS (profile));

  if (profile == self->profile)
    return;

  if (self->profile != nullptr) {
    g_autoptr(TerminalScreen) screen = terminal_window_color_dup_screen (self);

    if (screen != nullptr)
      vte_terminal_set_clear_background (VTE_TERMINAL (screen), TRUE);

    g_clear_signal_handler (&self->changed_handler, self->profile);

    g_clear_object (&self->profile);
  }

  if (profile != nullptr) {
    self->profile = g_object_ref (profile);
    self->changed_handler =
      g_signal_connect_object (self->profile,
                               "changed",
                               G_CALLBACK (terminal_window_color_profile_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    terminal_window_color_profile_changed_cb (self, nullptr, profile);
  }
}

static void
terminal_window_color_notify_profile (TerminalWindowColor *self,
                                      GParamSpec           *pspec,
                                      TerminalScreen       *screen)
{
  g_assert (TERMINAL_IS_WINDOW_COLOR (self));
  g_assert (TERMINAL_IS_SCREEN (screen));

  terminal_window_color_set_profile (self, terminal_screen_get_profile (screen));
}

void
terminal_window_color_set_screen (TerminalWindowColor *self,
                                  TerminalScreen       *screen)
{
  g_autoptr(TerminalScreen) old_screen = nullptr;

  g_return_if_fail (TERMINAL_IS_WINDOW_COLOR (self));
  g_return_if_fail (!screen || TERMINAL_IS_SCREEN (screen));

  old_screen = terminal_window_color_dup_screen (self);

  if (old_screen == screen)
    return;

  if (old_screen != nullptr) {
    g_clear_signal_handler (&self->notify_profile_handler, old_screen);
    terminal_window_color_set_profile (self, nullptr);
  }

  g_weak_ref_set (&self->screen_wr, screen);

  if (screen != nullptr) {
    g_signal_connect_object (screen,
                             "notify::profile",
                             G_CALLBACK (terminal_window_color_notify_profile),
                             self,
                             G_CONNECT_SWAPPED);
    terminal_window_color_set_profile (self, terminal_screen_get_profile (screen));
  }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCREEN]);
}

