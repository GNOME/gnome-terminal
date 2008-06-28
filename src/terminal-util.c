/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#undef G_DISABLE_SINGLE_INCLUDES

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <gconf/gconf-client.h>
#include <libgnome/gnome-help.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-intl.h"
#include "terminal-util.h"
#include "terminal-window.h"

void
terminal_util_set_unique_role (GtkWindow *window, const char *prefix)
{
  char *role;

  role = g_strdup_printf ("%s-%d-%d-%d", prefix, getpid (), g_random_int (), (int) time (NULL));
  gtk_window_set_role (window, role);
  g_free (role);
}

/**
 * terminal_util_show_error_dialog:
 * @transient_parent: parent of the future dialog window;
 * @weap_ptr: pointer to a #Widget pointer, to control the population.
 * @message_format: printf() style format string
 *
 * Create a #GtkMessageDialog window with the message, and present it, handling its buttons.
 * If @weap_ptr is not #NULL, only create the dialog if <literal>*weap_ptr</literal> is #NULL 
 * (and in that * case, set @weap_ptr to be a weak pointer to the new dialog), otherwise just 
 * present <literal>*weak_ptr</literal>. Note that in this last case, the message <emph>will</emph>
 * be changed.
 */

void
terminal_util_show_error_dialog (GtkWindow *transient_parent, GtkWidget **weak_ptr, const char *message_format, ...)
{
  char *message;
  va_list args;

  if (message_format)
    {
      va_start (args, message_format);
      message = g_strdup_vprintf (message_format, args);
      va_end (args);
    }
  else message = NULL;

  if (weak_ptr == NULL || *weak_ptr == NULL)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (transient_parent,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       message ? "%s" : NULL,
				       message);

      g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (gtk_widget_destroy), NULL);

      if (weak_ptr != NULL)
        {
        *weak_ptr = dialog;
        g_object_add_weak_pointer (G_OBJECT (dialog), (void**)weak_ptr);
        }

      gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
      
      gtk_widget_show_all (dialog);
    }
  else 
    {
      g_return_if_fail (GTK_IS_MESSAGE_DIALOG (*weak_ptr));

      gtk_label_set_text (GTK_LABEL (GTK_MESSAGE_DIALOG (*weak_ptr)->label), message);

      gtk_window_present (GTK_WINDOW (*weak_ptr));
    }
  }

void
terminal_util_show_help (const char *topic, 
                         GtkWindow  *transient_parent)
{
  GError *err;

  err = NULL;

  gnome_help_display ("gnome-terminal.xml", topic, &err);
  
  if (err)
    {
      terminal_util_show_error_dialog (GTK_WINDOW (transient_parent), NULL,
                                       _("There was an error displaying help: %s"),
                                      err->message);
      g_error_free (err);
    }
}
 
/* sets accessible name and description for the widget */

void
terminal_util_set_atk_name_description (GtkWidget  *widget,
                                        const char *name,
                                        const char *desc)
{
  AtkObject *obj;
  
  obj = gtk_widget_get_accessible (widget);

  if (obj == NULL)
    {
      g_warning ("%s: for some reason widget has no GtkAccessible",
                 G_STRFUNC);
      return;
    }

  
  if (!GTK_IS_ACCESSIBLE (obj))
    return; /* This means GAIL is not loaded so we have the NoOp accessible */
      
  g_return_if_fail (GTK_IS_ACCESSIBLE (obj));  
  if (desc)
    atk_object_set_description (obj, desc);
  if (name)
    atk_object_set_name (obj, name);
}

void
terminal_util_open_url (GtkWidget *parent,
                        const char *orig_url,
                        TerminalURLFlavour flavor,
                        guint32 user_time)
{
  GError *error = NULL;
  char *uri;
#if GTK_CHECK_VERSION (2, 13, 0)
  GdkAppLaunchContext *context;
#endif

  g_return_if_fail (orig_url != NULL);

  switch (flavor)
    {
    case FLAVOR_DEFAULT_TO_HTTP:
      uri = g_strdup_printf ("http:%s", orig_url);
      break;
    case FLAVOR_EMAIL:
      if (g_ascii_strncasecmp ("mailto:", orig_url, 7) != 0)
	uri = g_strdup_printf ("mailto:%s", orig_url);
      else
	uri = g_strdup (orig_url);
      break;
    case FLAVOR_VOIP_CALL:
    case FLAVOR_AS_IS:
      uri = g_strdup (orig_url);
      break;
    default:
      uri = NULL;
      g_assert_not_reached ();
    }

#if GTK_CHECK_VERSION (2, 13, 0)
  context = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_timestamp (context, user_time);

  if (parent)
    gdk_app_launch_context_set_screen (context, gtk_widget_get_screen (parent));
  else
    gdk_app_launch_context_set_screen (context, gdk_screen_get_default ());

  g_app_info_launch_default_for_uri (uri, G_APP_LAUNCH_CONTEXT (context), &error);
  g_object_unref (context);
#else
  g_app_info_launch_default_for_uri (uri, NULL, &error);
#endif

  if (error)
    {
      terminal_util_show_error_dialog (GTK_WINDOW (parent), NULL,
                                       _("Could not open the address “%s”:\n%s"),
                                       uri, error->message);

      g_error_free (error);
    }

  g_free (uri);
}

/**
 * terminal_util_transform_uris_to_quoted_fuse_paths:
 * @uris:
 *
 * Transforms those URIs in @uris to shell-quoted paths that point to
 * GIO fuse paths.
 */
void
terminal_util_transform_uris_to_quoted_fuse_paths (char **uris)
{
  guint i;

  if (!uris)
    return;

  for (i = 0; uris[i]; ++i)
    {
      GFile *file;
      char *path;

      file = g_file_new_for_uri (uris[i]);

      if ((path = g_file_get_path (file)))
        {
          char *quoted;

          quoted = g_shell_quote (path);
          g_free (uris[i]);
          g_free (path);

          uris[i] = quoted;
        }

      g_object_unref (file);
    }
}

gboolean
terminal_util_load_builder_file (const char *filename,
                                 const char *object_name,
                                 ...)
{
  char *path;
  GtkBuilder *builder;
  GError *error = NULL;
  va_list args;

  path = g_build_filename (TERM_PKGDATADIR, filename, NULL);
  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, path, &error)) {
    g_warning ("Failed to load %s: %s\n", filename, error->message);
    g_error_free (error);
    g_free (path);
    g_object_unref (builder);
    return FALSE;
  }
  g_free (path);

  va_start (args, object_name);

  while (object_name) {
    GObject **objectptr;

    objectptr = va_arg (args, GObject**);
    *objectptr = gtk_builder_get_object (builder, object_name);
    if (!*objectptr) {
      g_warning ("Failed to fetch object \"%s\"\n", object_name);
      break;
    }

    object_name = va_arg (args, const char*);
  }

  va_end (args);

  g_object_unref (builder);
  return object_name == NULL;
}

gboolean
terminal_util_dialog_response_on_delete (GtkWindow *widget)
{
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_DELETE_EVENT);
  return TRUE;
}

/* Bidirectional object/widget binding */

typedef struct {
  GObject *object;
  const char *object_prop;
  GtkWidget *widget;
  gulong object_notify_id;
  gulong widget_notify_id;
  PropertyChangeFlags flags;
} PropertyChange;

static void
property_change_free (PropertyChange *change)
{
  g_signal_handler_disconnect (change->object, change->object_notify_id);

  g_slice_free (PropertyChange, change);
}

static gboolean
transform_boolean (gboolean input,
                   PropertyChangeFlags flags)
{
  if (flags & FLAG_INVERT_BOOL)
    input = !input;

  return input;
}

static void
object_change_notify_cb (PropertyChange *change)
{
  GObject *object = change->object;
  const char *object_prop = change->object_prop;
  GtkWidget *widget = change->widget;

  g_signal_handler_block (widget, change->widget_notify_id);

  if (GTK_IS_RADIO_BUTTON (widget))
    {
      int ovalue, rvalue;

      g_object_get (object, object_prop, &ovalue, NULL);
      rvalue = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "enum-value"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), ovalue == rvalue);
    }
  else if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      gboolean enabled;

      g_object_get (object, object_prop, &enabled, NULL);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
                                    transform_boolean (enabled, change->flags));
    }
  else if (GTK_IS_SPIN_BUTTON (widget))
    {
      int value;

      g_object_get (object, object_prop, &value, NULL);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
    }
  else if (GTK_IS_ENTRY (widget))
    {
      char *text;

      g_object_get (object, object_prop, &text, NULL);
      gtk_entry_set_text (GTK_ENTRY (widget), text ? text : "");
      g_free (text);
    }
  else if (GTK_IS_COMBO_BOX (widget))
    {
      int value;

      g_object_get (object, object_prop, &value, NULL);
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), value);
    }
  else if (GTK_IS_RANGE (widget))
    {
      double value;

      g_object_get (object, object_prop, &value, NULL);
      gtk_range_set_value (GTK_RANGE (widget), value);
    }
  else if (GTK_IS_COLOR_BUTTON (widget))
    {
      GdkColor *color;
      GdkColor old_color;

      g_object_get (object, object_prop, &color, NULL);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &old_color);

      if (color && !gdk_color_equal (color, &old_color))
        gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), color);
      if (color)
        gdk_color_free (color);
    }
  else if (GTK_IS_FONT_BUTTON (widget))
    {
      PangoFontDescription *font_desc;
      char *font;

      g_object_get (object, object_prop, &font_desc, NULL);
      if (!font_desc)
        goto out;

      font = pango_font_description_to_string (font_desc);
      gtk_font_button_set_font_name (GTK_FONT_BUTTON (widget), font);
      g_free (font);
      pango_font_description_free (font_desc);
    }
  else if (GTK_IS_FILE_CHOOSER (widget))
    {
      char *name = NULL, *filename = NULL;

      g_object_get (object, object_prop, &name, NULL);
      if (name)
        filename = g_filename_from_utf8 (name, -1, NULL, NULL, NULL);

      if (filename)
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
      else
        gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (widget));
      g_free (filename);
      g_free (name);
    }

out:
  g_signal_handler_unblock (widget, change->widget_notify_id);
}

static void
widget_change_notify_cb (PropertyChange *change)
{
  GObject *object = change->object;
  const char *object_prop = change->object_prop;
  GtkWidget *widget = change->widget;

  g_signal_handler_block (change->object, change->object_notify_id);

  if (GTK_IS_RADIO_BUTTON (widget))
    {
      gboolean active;
      int value;

      active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
      if (!active)
        goto out;

      value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "enum-value"));
      g_object_set (object, object_prop, value, NULL);
    }
  else if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      gboolean enabled;

      enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
      g_object_set (object, object_prop, transform_boolean (enabled, change->flags), NULL);
    }
  else if (GTK_IS_SPIN_BUTTON (widget))
    {
      int value;

      value = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
      g_object_set (object, object_prop, value, NULL);
    }
  else if (GTK_IS_ENTRY (widget))
    {
      const char *text;

      text = gtk_entry_get_text (GTK_ENTRY (widget));
      g_object_set (object, object_prop, text, NULL);
    }
  else if (GTK_IS_COMBO_BOX (widget))
    {
      int value;

      value = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
      g_object_set (object, object_prop, value, NULL);
    }
  else if (GTK_IS_COLOR_BUTTON (widget))
    {
      GdkColor color;

      gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &color);
      g_object_set (object, object_prop, &color, NULL);
    }
  else if (GTK_IS_FONT_BUTTON (widget))
    {
      PangoFontDescription *font_desc;
      const char *font;

      font = gtk_font_button_get_font_name (GTK_FONT_BUTTON (widget));
      font_desc = pango_font_description_from_string (font);
      g_object_set (object, object_prop, font_desc, NULL);
      pango_font_description_free (font_desc);
    }
  else if (GTK_IS_RANGE (widget))
    {
      double value;

      value = gtk_range_get_value (GTK_RANGE (widget));
      g_object_set (object, object_prop, value, NULL);
    }
  else if (GTK_IS_FILE_CHOOSER (widget))
    {
      char *filename, *name = NULL;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
      if (filename)
        name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

      g_object_set (object, object_prop, name, NULL);
      g_free (filename);
      g_free (name);
    }

out:
  g_signal_handler_unblock (change->object, change->object_notify_id);
}

void
terminal_util_bind_object_property_to_widget (GObject *object,
                                              const char *object_prop,
                                              GtkWidget *widget,
                                              PropertyChangeFlags flags)
{
  PropertyChange *change;
  const char *signal;
  char notify_signal[64];

  change = g_slice_new0 (PropertyChange);

  change->widget = widget;
  g_assert (g_object_get_data (G_OBJECT (widget), "GT:PCD") == NULL);
  g_object_set_data_full (G_OBJECT (widget), "GT:PCD", change, (GDestroyNotify) property_change_free);

  if (GTK_IS_TOGGLE_BUTTON (widget))
    signal = "notify::active";
  else if (GTK_IS_SPIN_BUTTON (widget))
    signal = "notify::value";
  else if (GTK_IS_ENTRY (widget))
    signal = "notify::text";
  else if (GTK_IS_COMBO_BOX (widget))
    signal = "notify::active";
  else if (GTK_IS_COLOR_BUTTON (widget))
    signal = "notify::color";
  else if (GTK_IS_FONT_BUTTON (widget))
    signal = "notify::font-name";
  else if (GTK_IS_RANGE (widget))
    signal = "value-changed";
  else if (GTK_IS_FILE_CHOOSER_BUTTON (widget))
    signal = "file-set";
  else if (GTK_IS_FILE_CHOOSER (widget))
    signal = "selection-changed";
  else
    g_assert_not_reached ();

  change->widget_notify_id = g_signal_connect_swapped (widget, signal, G_CALLBACK (widget_change_notify_cb), change);

  change->object = object;
  change->flags = flags;
  change->object_prop = object_prop;

  g_snprintf (notify_signal, sizeof (notify_signal), "notify::%s", object_prop);
  object_change_notify_cb (change);
  change->object_notify_id = g_signal_connect_swapped (object, notify_signal, G_CALLBACK (object_change_notify_cb), change);
}
