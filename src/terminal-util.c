/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2011 Christian Persch
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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

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
#include "terminal-screen.h"
#include "terminal-util.h"
#include "terminal-window.h"

void
terminal_util_set_unique_role (GtkWindow *window, const char *prefix)
{
  char *role;

  role = g_strdup_printf ("%s-%d-%u-%d", prefix, getpid (), (guint) g_random_int (), (int) time (NULL));
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
  char *uri;
  GError *error = NULL;

  if (topic) {
    uri = g_strdup_printf ("help:gnome-terminal/%s", topic);
  } else {
    uri = g_strdup ("help:gnome-terminal");
  }

  if (!open_url (GTK_WINDOW (parent), uri, gtk_get_current_event_time (), &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (parent), NULL, error,
                                       _("There was an error displaying help"));
      g_error_free (error);
    }

  g_free (uri);
}

#define ABOUT_GROUP "About"
#define EMAILIFY(string) (g_strdelimit ((string), "%", '@'))

void
terminal_util_show_about (GtkWindow *transient_parent)
{
  static const char copyright[] =
    "Copyright © 2002–2004 Havoc Pennington\n"
    "Copyright © 2003–2004, 2007 Mariano Suárez-Alvarez\n"
    "Copyright © 2006 Guilherme de S. Pastore\n"
    "Copyright © 2007–2013 Christian Persch";
  char *licence_text;
  GKeyFile *key_file;
  GBytes *bytes;
  const guint8 *data;
  gsize data_len;
  GError *error = NULL;
  char **authors, **contributors, **artists, **documenters, **array_strv;
  gsize n_authors = 0, n_contributors = 0, n_artists = 0, n_documenters = 0 , i;
  GPtrArray *array;

  bytes = g_resources_lookup_data (TERMINAL_RESOURCES_PATH_PREFIX "ui/terminal.about", 
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);
  g_assert_no_error (error);

  data = g_bytes_get_data (bytes, &data_len);
  key_file = g_key_file_new ();
  g_key_file_load_from_data (key_file, (const char *) data, data_len, 0, &error);
  g_assert_no_error (error);

  authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Authors", &n_authors, NULL);
  contributors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Contributors", &n_contributors, NULL);
  artists = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", &n_artists, NULL);
  documenters = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", &n_documenters, NULL);

  g_key_file_free (key_file);
  g_bytes_unref (bytes);

  array = g_ptr_array_new ();

  for (i = 0; i < n_authors; ++i)
    g_ptr_array_add (array, EMAILIFY (authors[i]));
  g_free (authors); /* strings are now owned by the array */

  if (n_contributors > 0)
  {
    g_ptr_array_add (array, g_strdup (""));
    g_ptr_array_add (array, g_strdup (_("Contributors:")));
    for (i = 0; i < n_contributors; ++i)
      g_ptr_array_add (array, EMAILIFY (contributors[i]));
  }
  g_free (contributors); /* strings are now owned by the array */
  
  g_ptr_array_add (array, NULL);
  array_strv = (char **) g_ptr_array_free (array, FALSE);

  for (i = 0; i < n_artists; ++i)
    artists[i] = EMAILIFY (artists[i]);
  for (i = 0; i < n_documenters; ++i)
    documenters[i] = EMAILIFY (documenters[i]);

  licence_text = terminal_util_get_licence_text ();

  gtk_show_about_dialog (transient_parent,
                         "program-name", _("GNOME Terminal"),
                         "copyright", copyright,
                         "comments", _("A terminal emulator for the GNOME desktop"),
                         "version", VERSION,
                         "authors", array_strv,
                         "artists", artists,
                         "documenters", documenters,
                         "license", licence_text,
                         "wrap-license", TRUE,
                         "translator-credits", _("translator-credits"),
                         "logo-icon-name", GNOME_TERMINAL_ICON_NAME,
                         NULL);

  g_strfreev (array_strv);
  g_strfreev (artists);
  g_strfreev (documenters);
  g_free (licence_text);
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
    N_("GNOME Terminal is free software: you can redistribute it and/or modify "
       "it under the terms of the GNU General Public License as published by "
       "the Free Software Foundation, either version 3 of the License, or "
       "(at your option) any later version."),
    N_("GNOME Terminal is distributed in the hope that it will be useful, "
       "but WITHOUT ANY WARRANTY; without even the implied warranty of "
       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
       "GNU General Public License for more details."),
    N_("You should have received a copy of the GNU General Public License "
       "along with GNOME Terminal.  If not, see <http://www.gnu.org/licenses/>.")
  };

  return g_strjoin ("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);
}

static void
main_object_destroy_cb (GtkWidget *widget)
{
  g_object_set_data (G_OBJECT (widget), "builder", NULL);
}

void
terminal_util_load_builder_resource (const char *path,
                                     const char *main_object_name,
                                     const char *object_name,
                                     ...)
{
  GtkBuilder *builder;
  GError *error = NULL;
  va_list args;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, path, &error);
  g_assert_no_error (error);

  va_start (args, object_name);

  while (object_name) {
    GObject **objectptr;

    objectptr = va_arg (args, GObject**);
    *objectptr = gtk_builder_get_object (builder, object_name);
    if (!*objectptr)
      g_error ("Failed to fetch object \"%s\" from resource \"%s\"\n", object_name, path);

    object_name = va_arg (args, const char*);
  }

  va_end (args);

  if (main_object_name) {
    GObject *main_object;

    main_object = gtk_builder_get_object (builder, main_object_name);
    g_object_set_data_full (main_object, "builder", builder, (GDestroyNotify) g_object_unref);
    g_signal_connect (main_object, "destroy", G_CALLBACK (main_object_destroy_cb), NULL);
  } else {
    g_object_unref (builder);
  }
}

gboolean
terminal_util_dialog_response_on_delete (GtkWindow *widget)
{
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_DELETE_EVENT);
  return TRUE;
}

void
terminal_util_dialog_focus_widget (GtkWidget *dialog,
                                   const char *widget_name)
{
  GtkBuilder *builder;
  GtkWidget *widget, *page, *page_parent;

  if (widget_name == NULL)
    return;

  builder = g_object_get_data (G_OBJECT (dialog), "builder");
  widget = GTK_WIDGET (gtk_builder_get_object (builder, widget_name));
  if (widget == NULL)
    return;

  page = widget;
  while (page != NULL &&
         (page_parent = gtk_widget_get_parent (page)) != NULL &&
         !GTK_IS_NOTEBOOK (page_parent))
    page = page_parent;

  page_parent = gtk_widget_get_parent (page);
  if (page != NULL && GTK_IS_NOTEBOOK (page_parent)) {
    GtkNotebook *notebook;

    notebook = GTK_NOTEBOOK (page_parent);
    gtk_notebook_set_current_page (notebook, gtk_notebook_page_num (notebook, page));
  }

  if (gtk_widget_is_sensitive (widget))
    gtk_widget_grab_focus (widget);
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

GdkScreen*
terminal_util_get_screen_by_display_name (const char *display_name,
                                          int screen_number)
{
  GdkDisplay *display = NULL;
  GdkScreen *screen = NULL;

  /* --screen=screen_number overrides --display */

  if (display_name == NULL)
    display = gdk_display_get_default ();
  else
    {
      GSList *displays, *l;
      const char *period;

      period = strrchr (display_name, '.');
      if (period)
        {
          gulong n;
          char *end;

          errno = 0;
          end = NULL;
          n = g_ascii_strtoull (period + 1, &end, 0);
          if (errno == 0 && (period + 1) != end)
            screen_number = n;
        }

      displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
      for (l = displays; l != NULL; l = l->next)
        {
          GdkDisplay *disp = l->data;

          /* compare without the screen number part, if present */
          if ((period && strncmp (gdk_display_get_name (disp), display_name, period - display_name) == 0) ||
              (period == NULL && strcmp (gdk_display_get_name (disp), display_name) == 0))
            {
              display = disp;
              break;
            }
        }
      g_slist_free (displays);

      if (display == NULL)
        display = gdk_display_open (display_name); /* FIXME we never close displays */
    }

  if (display == NULL)
    return NULL;
  if (screen_number >= 0)
    screen = gdk_display_get_screen (display, screen_number);
  if (screen == NULL)
    screen = gdk_display_get_default_screen (display);

  return screen;
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

  display = gdk_window_get_display (window);

  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
                          GDK_WINDOW_XID (window),
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

  screen = gdk_window_get_screen (window);
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
      xclient.window = GDK_WINDOW_XID (window);
      xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP");
      xclient.format = 32;

      xclient.data.l[0] = desktop;
      xclient.data.l[1] = 0;
      xclient.data.l[2] = 0;
      xclient.data.l[3] = 0;
      xclient.data.l[4] = 0;

      XSendEvent (xdisplay,
                  GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
		  False,
		  SubstructureRedirectMask | SubstructureNotifyMask,
		  (XEvent *)&xclient);
    }
  else
    {
      gulong long_desktop = desktop;

      XChangeProperty (xdisplay,
                       GDK_WINDOW_XID (window),
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

  screen = gdk_window_get_screen (window);
  display = gdk_screen_get_display (screen);

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.serial = 0;
  xclient.send_event = True;
  xclient.window = GDK_WINDOW_XID (window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE");
  xclient.format = 32;

  xclient.data.l[0] = 0; /* _NET_WM_STATE_REMOVE */
  xclient.data.l[1] = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_DEMANDS_ATTENTION");
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;

  XSendEvent (GDK_DISPLAY_XDISPLAY (display),
              GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
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
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Atom *atoms = NULL;
  gulong i;
  gboolean minimized = FALSE;

  display = gdk_window_get_display (window);

  type = None;
  gdk_error_trap_push ();
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
                      0, G_MAXLONG, False, XA_ATOM, &type, &format, &nitems,
                      &bytes_after, &data);
  gdk_error_trap_pop_ignored ();

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

static gboolean
s_to_rgba (GVariant *variant,
           gpointer *result,
           gpointer  user_data)
{
  GdkRGBA *color = user_data;
  const char *str;

  if (variant == NULL) {
    /* Fallback */
    *result = NULL;
    return TRUE;
  }

  g_variant_get (variant, "&s", &str);
  if (!gdk_rgba_parse (color, str))
    return FALSE;

  *result = color;
  return TRUE;
}

/**
 * terminal_g_settings_get_rgba:
 * @settings: a #GSettings
 * @key: a valid key in @settings of type "s"
 * @color: location to store the parsed color
 *
 * Gets a color from @key in @settings.
 *
 * Returns: @color if parsing succeeded, or %NULL otherwise
 */
const GdkRGBA *
terminal_g_settings_get_rgba (GSettings  *settings,
                              const char *key,
                              GdkRGBA    *color)
{
  g_return_val_if_fail (color != NULL, FALSE);

  return g_settings_get_mapped (settings, key,
                                (GSettingsGetMapping) s_to_rgba,
                                color);
}

/**
 * terminal_g_settings_set_rgba:
 * @settings: a #GSettings
 * @key: a valid key in @settings of type "s"
 * @color: a #GdkRGBA
 *
 * Sets a color in @key in @settings.
 */
void
terminal_g_settings_set_rgba (GSettings  *settings,
                              const char *key,
                              const GdkRGBA *color)
{
  char *str;

  str = gdk_rgba_to_string (color);
  g_settings_set_string (settings, key, str);
  g_free (str);
}

static gboolean
as_to_rgba_palette (GVariant *variant,
                    gpointer *result,
                    gpointer user_data)
{
  gsize *n_colors = user_data;
  gsize n, i;
  GdkRGBA *colors;
  GVariantIter iter;
  const char *str;

  /* Fallback */
  if (variant == NULL) {
    *result = NULL;
    if (n_colors)
      *n_colors = 0;
    return TRUE;
  }

  g_variant_iter_init (&iter, variant);
  n = g_variant_iter_n_children (&iter);
  colors = g_new (GdkRGBA, n);

  i = 0;
  while (g_variant_iter_next (&iter, "&s", &str)) {
    if (!gdk_rgba_parse (&colors[i++], str)) {
      g_free (colors);
      return FALSE;
    }
  }

  *result = colors;
  if (n_colors)
    *n_colors = n;

  return TRUE;
}

/**
 * terminal_g_settings_get_rgba_palette:
 * @settings: a #GSettings
 * @key: a valid key in @settings or type "s"
 * @n_colors: (allow-none): location to store the number of palette entries, or %NULL
 *
 * Returns: (transfer full):
 */
GdkRGBA *
terminal_g_settings_get_rgba_palette (GSettings  *settings,
                                      const char *key,
                                      gsize      *n_colors)
{
  return g_settings_get_mapped (settings, key,
                                (GSettingsGetMapping) as_to_rgba_palette,
                                n_colors);
}

void
terminal_g_settings_set_rgba_palette (GSettings      *settings,
                                      const char     *key,
                                      const GdkRGBA  *colors,
                                      gsize           n_colors)
{
  char **strv;
  gsize i;

  strv = g_new (char *, n_colors + 1);
  for (i = 0; i < n_colors; ++i)
    strv[i] = gdk_rgba_to_string (&colors[i]);
  strv[n_colors] = NULL;

  g_settings_set (settings, key, "^as", strv);
  g_strfreev (strv);
}

static void
mnemonic_label_set_sensitive_cb (GtkWidget *widget,
                                 GParamSpec *pspec,
                                 GtkWidget *label)
{
  gtk_widget_set_sensitive (label, gtk_widget_get_sensitive (widget));
}

/**
 * terminal_util_bind_mnemonic_label_sensitivity:
 * @container: a #GtkContainer
 */
void
terminal_util_bind_mnemonic_label_sensitivity (GtkWidget *widget)
{
  GList *list, *l;

  list = gtk_widget_list_mnemonic_labels (widget);
  for (l = list; l != NULL; l = l->next) {
    GtkWidget *label = l->data;

    if (gtk_widget_is_ancestor (label, widget))
      continue;

#if 0
    g_print ("Widget %s has mnemonic label %s\n",
             gtk_buildable_get_name (GTK_BUILDABLE (widget)),
             gtk_buildable_get_name (GTK_BUILDABLE (label)));
#endif

    mnemonic_label_set_sensitive_cb (widget, NULL, label);
    g_signal_connect (widget, "notify::sensitive",
                      G_CALLBACK (mnemonic_label_set_sensitive_cb),
                      label);
  }
  g_list_free (list);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
                           (GtkCallback) terminal_util_bind_mnemonic_label_sensitivity,
                           NULL);
}
