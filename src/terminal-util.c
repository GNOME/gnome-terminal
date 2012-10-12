/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2011 Christian Persch
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
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#include <gdesktop-enums.h>

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
 * @error: a #GError, or %NULL
 * @message_format: printf() style format string
 *
 * Create a #GtkMessageDialog window with the message, and present it, handling its buttons.
 * If @weap_ptr is not #NULL, only create the dialog if <literal>*weap_ptr</literal> is #NULL 
 * (and in that * case, set @weap_ptr to be a weak pointer to the new dialog), otherwise just 
 * present <literal>*weak_ptr</literal>. Note that in this last case, the message <emph>will</emph>
 * be changed.
 */
void
terminal_util_show_error_dialog (GtkWindow *transient_parent, 
                                 GtkWidget **weak_ptr,
                                 GError *error,
                                 const char *message_format, 
                                 ...) 
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

      if (error != NULL)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);

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

      /* Sucks that there's no direct accessor for "text" property */
      g_object_set (G_OBJECT (*weak_ptr), "text", message, NULL);

      gtk_window_present (GTK_WINDOW (*weak_ptr));
    }

  g_free (message);
}

static gboolean
open_url (GtkWindow *parent,
          const char *uri,
          guint32 user_time,
          GError **error)
{
  GdkScreen *screen;

  if (parent)
    screen = gtk_widget_get_screen (GTK_WIDGET (parent));
  else
    screen = gdk_screen_get_default ();

  return gtk_show_uri (screen, uri, user_time, error);
}

void
terminal_util_show_help (const char *topic, 
                         GtkWindow  *parent)
{
  GError *error = NULL;
  char *url;

  if (topic) {
    url = g_strdup_printf ("help:gnome-terminal/%s", topic); /* DOC_MODULE */
  } else {
    url = g_strdup ("help:gnome-terminal"); /* DOC_MODULE */
  }

  if (!open_url (GTK_WINDOW (parent), url, gtk_get_current_event_time (), &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (parent), NULL, error,
                                       _("There was an error displaying help"));
      g_error_free (error);
    }

  g_free (url);
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

  g_return_if_fail (orig_url != NULL);

  switch (flavor)
    {
    case FLAVOR_DEFAULT_TO_HTTP:
      uri = g_strdup_printf ("http://%s", orig_url);
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

  if (!open_url (GTK_WINDOW (parent), uri, user_time, &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (parent), NULL, error,
                                       _("Could not open the address “%s”"),
                                       uri);

      g_error_free (error);
    }

  g_free (uri);
}

/**
 * terminal_util_resolve_relative_path:
 * @path:
 * @relative_path:
 *
 * Returns: a newly allocate string
 */
char *
terminal_util_resolve_relative_path (const char *path,
                                     const char *relative_path)
{
  GFile *file, *resolved_file;
  char *resolved_path = NULL;

  g_return_val_if_fail (relative_path != NULL, NULL);

  if (path == NULL)
    return g_strdup (relative_path);

  file = g_file_new_for_path (path);
  resolved_file = g_file_resolve_relative_path (file, relative_path);
  g_object_unref (file);

  if (resolved_file == NULL)
    return NULL;

  resolved_path = g_file_get_path (resolved_file);
  g_object_unref (resolved_file);

  return resolved_path;
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

char *
terminal_util_concat_uris (char **uris,
                           gsize *length)
{
  GString *string;
  gsize len;
  guint i;

  len = 0;
  for (i = 0; uris[i]; ++i)
    len += strlen (uris[i]) + 1;

  if (length)
    *length = len;

  string = g_string_sized_new (len + 1);
  for (i = 0; uris[i]; ++i)
    {
      g_string_append (string, uris[i]);
      g_string_append_c (string, ' ');
    }

  return g_string_free (string, FALSE);
}

char *
terminal_util_get_licence_text (void)
{
  const gchar *license[] = {
    N_("GNOME Terminal is free software; you can redistribute it and/or modify "
       "it under the terms of the GNU General Public License as published by "
       "the Free Software Foundation; either version 3 of the License, or "
       "(at your option) any later version."),
    N_("GNOME Terminal is distributed in the hope that it will be useful, "
       "but WITHOUT ANY WARRANTY; without even the implied warranty of "
       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
       "GNU General Public License for more details."),
    N_("You should have received a copy of the GNU General Public License "
       "along with GNOME Terminal; if not, write to the Free Software Foundation, "
       "Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA")
  };

  return g_strjoin ("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);
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

/* Like g_key_file_set_string, but escapes characters so that
 * the stored string is ASCII. Use when the input string may not
 * be UTF-8.
 */
void
terminal_util_key_file_set_string_escape (GKeyFile *key_file,
                                          const char *group,
                                          const char *key,
                                          const char *string)
{
  char *escaped;

  /* FIXMEchpe: be more intelligent and only escape characters that aren't UTF-8 */
  escaped = g_strescape (string, NULL);
  g_key_file_set_string (key_file, group, key, escaped);
  g_free (escaped);
}

char *
terminal_util_key_file_get_string_unescape (GKeyFile *key_file,
                                            const char *group,
                                            const char *key,
                                            GError **error)
{
  char *escaped, *unescaped;

  escaped = g_key_file_get_string (key_file, group, key, error);
  if (!escaped)
    return NULL;

  unescaped = g_strcompress (escaped);
  g_free (escaped);

  return unescaped;
}

void
terminal_util_key_file_set_argv (GKeyFile *key_file,
                                 const char *group,
                                 const char *key,
                                 int argc,
                                 char **argv)
{
  char **quoted_argv;
  char *flat;
  int i;

  if (argc < 0)
    argc = g_strv_length (argv);

  quoted_argv = g_new (char*, argc + 1);
  for (i = 0; i < argc; ++i)
    quoted_argv[i] = g_shell_quote (argv[i]);
  quoted_argv[argc] = NULL;

  flat = g_strjoinv (" ", quoted_argv);
  terminal_util_key_file_set_string_escape (key_file, group, key, flat);

  g_free (flat);
  g_strfreev (quoted_argv);
}

char **
terminal_util_key_file_get_argv (GKeyFile *key_file,
                                 const char *group,
                                 const char *key,
                                 int *argc,
                                 GError **error)
{
  char **argv;
  char *flat;
  gboolean retval;

  flat = terminal_util_key_file_get_string_unescape (key_file, group, key, error);
  if (!flat)
    return NULL;

  retval = g_shell_parse_argv (flat, argc, &argv, error);
  g_free (flat);

  if (retval)
    return argv;

  return NULL;
}

/* Proxy stuff */

/*
 * set_proxy_env:
 * @env_table: a #GHashTable
 * @key: the env var name
 * @value: the env var value
 *
 * Adds @value for @key to @env_table, taking care to never overwrite an
 * existing value for @key. @value is consumed.
 */
static void
set_proxy_env (GHashTable *env_table,
               const char *key,
               char *value /* consumed */)
{
  char *key1 = NULL, *key2 = NULL;
  char *value1 = NULL, *value2 = NULL;

  if (!value)
    return;

  if (g_hash_table_lookup (env_table, key) == NULL)
    key1 = g_strdup (key);

  key2 = g_ascii_strup (key, -1);
  if (g_hash_table_lookup (env_table, key) != NULL)
    {
      g_free (key2);
      key2 = NULL;
    }

  if (key1 && key2)
    {
      value1 = value;
      value2 = g_strdup (value);
    }
  else if (key1)
    value1 = value;
  else if (key2)
    value2 = value;
  else
    g_free (value);

  if (key1)
    g_hash_table_replace (env_table, key1, value1);
  if (key2)
    g_hash_table_replace (env_table, key2, value2);
}

static void
setup_proxy_env (GSettings  *proxy_settings,
                 const char *child_schema_id,
                 const char *proxy_scheme,
                 const char *env_name,
                 GHashTable *env_table)
{
  GSettings *child_settings;
  GString *buf;
  const char *host;
  int port;
  gboolean is_http;

  is_http = (strcmp (child_schema_id, "http") == 0);

  child_settings = g_settings_get_child (proxy_settings, child_schema_id);

  g_settings_get (child_settings, "host", "&s", &host);
  port = g_settings_get_int (child_settings, "port");
  if (host[0] == '\0' || port == 0)
    goto out;

  buf = g_string_sized_new (64);

  g_string_append_printf (buf, "%s://", proxy_scheme);

  if (is_http &&
      g_settings_get_boolean (child_settings, "use-authentication"))
    {
      const char *user, *password;

      g_settings_get (child_settings, "authentication-user", "&s", &user);

      if (user[0])
        {
          g_string_append_uri_escaped (buf, user, NULL, TRUE);

          g_settings_get (child_settings, "authentication-password", "&s", &password);

          if (password[0])
            {
              g_string_append_c (buf, ':');
              g_string_append_uri_escaped (buf, password, NULL, TRUE);
            }
          g_string_append_c (buf, '@');
        }
    }

  g_string_append_printf (buf, "%s:%d/", host, port);
  set_proxy_env (env_table, env_name, g_string_free (buf, FALSE));

out:
  g_object_unref (child_settings);
}

static void
setup_autoconfig_proxy_env (GSettings *proxy_settings,
                            GHashTable *env_table)
{
  /* XXX  Not sure what to do with this.  See bug #596688.
  const char *url;

  g_settings_get (proxy_settings, "autoconfig-url", "&s", &url);
  if (url[0])
    {
      char *proxy;
      proxy = g_strdup_printf ("pac+%s", url);
      set_proxy_env (env_table, "http_proxy", proxy);
    }
  */
}

static void
setup_ignore_proxy_env (GSettings *proxy_settings,
                        GHashTable *env_table)
{
  GString *buf;
  char **ignore;
  int i;

  g_settings_get (proxy_settings, "ignore-hosts", "^a&s", &ignore);
  if (ignore == NULL)
    return;

  buf = g_string_sized_new (64);
  for (i = 0; ignore[i] != NULL; ++i)
    {
      if (buf->len)
        g_string_append_c (buf, ',');
      g_string_append (buf, ignore[i]);
    }
  g_free (ignore);

  set_proxy_env (env_table, "no_proxy", g_string_free (buf, FALSE));
}

/**
 * terminal_util_add_proxy_env:
 * @env_table: a #GHashTable
 *
 * Adds the proxy env variables to @env_table.
 */
void
terminal_util_add_proxy_env (GHashTable *env_table)
{
  GSettings *proxy_settings;
  GDesktopProxyMode mode;

  proxy_settings = terminal_app_get_proxy_settings (terminal_app_get ());
  mode = g_settings_get_enum (proxy_settings, "mode");

  if (mode == G_DESKTOP_PROXY_MODE_MANUAL)
    {
      setup_proxy_env (proxy_settings, "http", "http", "http_proxy", env_table);
      /* Even though it's https, the proxy scheme is 'http'. See bug #624440. */
      setup_proxy_env (proxy_settings, "https", "http", "https_proxy", env_table);
      /* Even though it's ftp, the proxy scheme is 'http'. See bug #624440. */
      setup_proxy_env (proxy_settings, "ftp", "http", "ftp_proxy", env_table);
      setup_proxy_env (proxy_settings, "socks", "socks", "all_proxy", env_table);
      setup_ignore_proxy_env (proxy_settings, env_table);
    }
  else if (mode == G_DESKTOP_PROXY_MODE_AUTO)
    {
      setup_autoconfig_proxy_env (proxy_settings, env_table);
    }
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
  const char *signal_name;
  char notify_signal_name[64];

  change = g_slice_new0 (PropertyChange);

  change->widget = widget;
  g_assert (g_object_get_data (G_OBJECT (widget), "GT:PCD") == NULL);
  g_object_set_data_full (G_OBJECT (widget), "GT:PCD", change, (GDestroyNotify) property_change_free);

  if (GTK_IS_TOGGLE_BUTTON (widget))
    signal_name = "notify::active";
  else if (GTK_IS_SPIN_BUTTON (widget))
    signal_name = "notify::value";
  else if (GTK_IS_ENTRY (widget))
    signal_name = "notify::text";
  else if (GTK_IS_COMBO_BOX (widget))
    signal_name = "notify::active";
  else if (GTK_IS_COLOR_BUTTON (widget))
    signal_name = "notify::color";
  else if (GTK_IS_FONT_BUTTON (widget))
    signal_name = "notify::font-name";
  else if (GTK_IS_RANGE (widget))
    signal_name = "value-changed";
  else if (GTK_IS_FILE_CHOOSER_BUTTON (widget))
    signal_name = "file-set";
  else if (GTK_IS_FILE_CHOOSER (widget))
    signal_name = "selection-changed";
  else
    g_assert_not_reached ();

  change->widget_notify_id = g_signal_connect_swapped (widget, signal_name, G_CALLBACK (widget_change_notify_cb), change);

  change->object = object;
  change->flags = flags;
  change->object_prop = object_prop;

  g_snprintf (notify_signal_name, sizeof (notify_signal_name), "notify::%s", object_prop);
  object_change_notify_cb (change);
  change->object_notify_id = g_signal_connect_swapped (object, notify_signal_name, G_CALLBACK (object_change_notify_cb), change);
}

#ifdef GDK_WINDOWING_X11

/* We don't want to hop desktops when we unrealize/realize.
 * So we need to save and restore the value of NET_WM_DESKTOP. This isn't
 * exposed through GDK.
 */
gboolean
terminal_util_x11_get_net_wm_desktop (GdkWindow *window,
				      guint32   *desktop)
{
  GdkDisplay *display;
  Atom type;
  int format;
  guchar *data;
  gulong n_items, bytes_after;
  gboolean result = FALSE;

#if GTK_CHECK_VERSION (2, 90, 8)
  display = gdk_window_get_display (window);
#else
  display = gdk_drawable_get_display (window);
#endif

  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
#if GTK_CHECK_VERSION (2, 91, 6)
                          GDK_WINDOW_XID (window),
#else
			  GDK_DRAWABLE_XID (window),
#endif
			  gdk_x11_get_xatom_by_name_for_display (display,
								 "_NET_WM_DESKTOP"),
			  0, G_MAXLONG, False, AnyPropertyType,
			  &type, &format, &n_items, &bytes_after, &data) == Success &&
      type != None)
    {
      if (type == XA_CARDINAL && format == 32 && n_items == 1)
	{
	  *desktop = *(gulong *)data;
	  result = TRUE;
	}

      XFree (data);
    }

  return result;
}

void
terminal_util_x11_set_net_wm_desktop (GdkWindow *window,
				      guint32    desktop)
{
  /* We can't change the current desktop before mapping our window,
   * because GDK has the annoying habit of clearing _NET_WM_DESKTOP
   * before mapping a GdkWindow, So we we have to do it after instead.
   *
   * However, doing it after is different whether or not we have a
   * window manager (if we don't have a window manager, we have to
   * set the _NET_WM_DESKTOP property so that it picks it up when
   * it starts)
   *
   * http://bugzilla.gnome.org/show_bug.cgi?id=586311 asks for GTK+
   * to just handle everything behind the scenes including the desktop.
   */
  GdkScreen *screen;
  GdkDisplay *display;
  Display *xdisplay;
  char *wm_selection_name;
  Atom wm_selection;
  gboolean have_wm;

#if GTK_CHECK_VERSION (2, 90, 8)
  screen = gdk_window_get_screen (window);
#else
  screen = gdk_drawable_get_screen (window);
#endif
  display = gdk_screen_get_display (screen);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  wm_selection_name = g_strdup_printf ("WM_S%d", gdk_screen_get_number (screen));
  wm_selection = gdk_x11_get_xatom_by_name_for_display (display, wm_selection_name);
  g_free(wm_selection_name);

  XGrabServer (xdisplay);

  have_wm = XGetSelectionOwner (xdisplay, wm_selection) != None;

  if (have_wm)
    {
      /* code borrowed from GDK
       */
      XClientMessageEvent xclient;

      memset (&xclient, 0, sizeof (xclient));
      xclient.type = ClientMessage;
      xclient.serial = 0;
      xclient.send_event = True;
#if GTK_CHECK_VERSION (2, 91, 6)
      xclient.window = GDK_WINDOW_XID (window);
#else
      xclient.window = GDK_WINDOW_XWINDOW (window);
#endif
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP");
      xclient.format = 32;

      xclient.data.l[0] = desktop;
      xclient.data.l[1] = 0;
      xclient.data.l[2] = 0;
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;

      XSendEvent (xdisplay,
#if GTK_CHECK_VERSION (2, 91, 6)
                  GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
#else
		  GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen)),
#endif
		  False,
		  SubstructureRedirectMask | SubstructureNotifyMask,
		  (XEvent *)&xclient);
    }
  else
    {
      gulong long_desktop = desktop;

      XChangeProperty (xdisplay,
#if GTK_CHECK_VERSION (2, 91, 6)
                       GDK_WINDOW_XID (window),
#else
		       GDK_DRAWABLE_XID (window),
#endif
		       gdk_x11_get_xatom_by_name_for_display (display,
							      "_NET_WM_DESKTOP"),
		       XA_CARDINAL, 32, PropModeReplace,
		       (guchar *)&long_desktop, 1);
    }

  XUngrabServer (xdisplay);
  XFlush (xdisplay);
}

/* Asks the window manager to turn off the "demands attention" state on the window.
 *
 * This only works for windows that are currently window managed; if the window
 * is unmapped (in the withdrawn state) it would be necessary to change _NET_WM_STATE
 * directly.
 */
void
terminal_util_x11_clear_demands_attention (GdkWindow *window)
{
  GdkScreen *screen;
  GdkDisplay *display;
  XClientMessageEvent xclient;

#if GTK_CHECK_VERSION (2, 90, 8)
  screen = gdk_window_get_screen (window);
#else
  screen = gdk_drawable_get_screen (window);
#endif
  display = gdk_screen_get_display (screen);

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.serial = 0;
  xclient.send_event = True;
#if GTK_CHECK_VERSION (2, 91, 6)
  xclient.window = GDK_WINDOW_XID (window);
#else
  xclient.window = GDK_WINDOW_XWINDOW (window);
#endif
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE");
  xclient.format = 32;

  xclient.data.l[0] = 0; /* _NET_WM_STATE_REMOVE */
  xclient.data.l[1] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_DEMANDS_ATTENTION");
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (display),
#if GTK_CHECK_VERSION (2, 91, 6)
              GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
#else
	      GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen)),
#endif
	      False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      (XEvent *)&xclient);
}

/* Check if a GdkWindow is minimized. This is a workaround for a
 * GDK bug/misfeature. gdk_window_get_state (window) has the
 * GDK_WINDOW_STATE_ICONIFIED bit for all unmapped windows,
 * even windows on another desktop.
 *
 * http://bugzilla.gnome.org/show_bug.cgi?id=586664
 *
 * Code to read _NET_WM_STATE adapted from GDK
 */
gboolean
terminal_util_x11_window_is_minimized (GdkWindow *window)
{
  GdkDisplay *display;

#if GTK_CHECK_VERSION (2, 90, 8)
  display = gdk_window_get_display (window);
#else
  display = gdk_drawable_get_display (window);
#endif

  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Atom *atoms = NULL;
  gulong i;

  gboolean minimized = FALSE;

  type = None;
  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
                      0, G_MAXLONG, False, XA_ATOM, &type, &format, &nitems,
                      &bytes_after, &data);
#if GTK_CHECK_VERSION (2, 90, 8)
  gdk_error_trap_pop_ignored ();
#else
  gdk_error_trap_pop ();
#endif

  if (type != None)
    {
      Atom hidden_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_HIDDEN");

      atoms = (Atom *)data;

      for (i = 0; i < nitems; i++)
        {
          if (atoms[i] == hidden_atom)
            minimized = TRUE;

          ++i;
        }

      XFree (atoms);
    }

  return minimized;
}

#endif /* GDK_WINDOWING_X11 */
