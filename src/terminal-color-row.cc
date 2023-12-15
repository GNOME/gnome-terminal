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
 */

#include "config.h"

#include "terminal-color-row.hh"

struct _TerminalColorRow
{
  AdwActionRow parent_instance;
  GtkColorButton* button;
  GdkRGBA color;
};

enum {
  PROP_0,
  PROP_COLOR,
  N_PROPS
};

static GObject *
terminal_color_row_get_internal_child (GtkBuildable *buildable,
                                       GtkBuilder   *builder,
                                       const char   *name)
{
  if (g_str_equal(name, "button"))
    return G_OBJECT(TERMINAL_COLOR_ROW(buildable)->button);

  return nullptr;
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = terminal_color_row_get_internal_child;
}

G_DEFINE_FINAL_TYPE_WITH_CODE(TerminalColorRow, terminal_color_row, ADW_TYPE_ACTION_ROW,
                              G_IMPLEMENT_INTERFACE(GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec *properties [N_PROPS];

static void
terminal_color_row_dispose (GObject *object)
{
  TerminalColorRow *self = (TerminalColorRow *)object;

  gtk_widget_dispose_template (GTK_WIDGET (self), TERMINAL_TYPE_COLOR_ROW);

  G_OBJECT_CLASS (terminal_color_row_parent_class)->dispose (object);
}

static void
terminal_color_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TerminalColorRow *self = TERMINAL_COLOR_ROW (object);

  switch (prop_id) {
  case PROP_COLOR:
    g_value_set_boxed (value, terminal_color_row_get_color (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_color_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TerminalColorRow *self = TERMINAL_COLOR_ROW (object);

  switch (prop_id) {
  case PROP_COLOR:
    terminal_color_row_set_color (self, (const GdkRGBA *)g_value_get_boxed (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
terminal_color_row_class_init (TerminalColorRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = terminal_color_row_dispose;
  object_class->get_property = terminal_color_row_get_property;
  object_class->set_property = terminal_color_row_set_property;

  properties [PROP_COLOR] =
    g_param_spec_boxed ("color", nullptr, nullptr,
                        GDK_TYPE_RGBA,
                        GParamFlags(G_PARAM_READWRITE |
                                    G_PARAM_EXPLICIT_NOTIFY |
                                    G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/color-row.ui");

  gtk_widget_class_bind_template_child (widget_class, TerminalColorRow, button);
}

static void
terminal_color_row_init (TerminalColorRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

const GdkRGBA *
terminal_color_row_get_color (TerminalColorRow *self)
{
  g_return_val_if_fail (TERMINAL_IS_COLOR_ROW (self), nullptr);

  return &self->color;
}

void
terminal_color_row_set_color (TerminalColorRow *self,
                              const GdkRGBA    *color)
{
  static GdkRGBA empty;

  g_return_if_fail (TERMINAL_IS_COLOR_ROW (self));
  g_return_if_fail (color != nullptr);

  if (color == nullptr)
    color = &empty;

  if (!gdk_rgba_equal (&self->color, color)) {
    self->color = *color;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
  }
}
