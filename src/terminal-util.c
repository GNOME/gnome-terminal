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

#include "config.h"
#define _GNU_SOURCE /* for strchrnul */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <langinfo.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <gdesktop-enums.h>

#include "terminal-pcre2.h"
#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-intl.h"
#include "terminal-screen.h"
#include "terminal-util.h"
#include "terminal-window.h"
#include "terminal-libgsystem.h"

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
  gs_free char *message;
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
}

static gboolean
open_url (GtkWindow *parent,
          const char *uri,
          guint32 user_time,
          GError **error)
{
  GdkScreen *screen;
  gs_free char *uri_fixed;

  if (parent)
    screen = gtk_widget_get_screen (GTK_WIDGET (parent));
  else
    screen = gdk_screen_get_default ();

  uri_fixed = terminal_util_uri_fixup (uri, error);
  if (uri_fixed == NULL)
    return FALSE;

  return gtk_show_uri (screen, uri_fixed, user_time, error);
}

void
terminal_util_show_help (const char *topic, 
                         GtkWindow  *parent)
{
  gs_free_error GError *error = NULL;
  gs_free char *uri;

  if (topic) {
    uri = g_strdup_printf ("help:gnome-terminal/%s", topic);
  } else {
    uri = g_strdup ("help:gnome-terminal");
  }

  if (!open_url (GTK_WINDOW (parent), uri, gtk_get_current_event_time (), &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (parent), NULL, error,
                                       _("There was an error displaying help"));
    }
}

#define ABOUT_GROUP "About"
#define ABOUT_URL "https://wiki.gnome.org/Apps/Terminal"
#define EMAILIFY(string) (g_strdelimit ((string), "%", '@'))

void
terminal_util_show_about (GtkWindow *transient_parent G_GNUC_UNUSED)
{
  static const char copyright[] =
    "Copyright © 2002–2004 Havoc Pennington\n"
    "Copyright © 2003–2004, 2007 Mariano Suárez-Alvarez\n"
    "Copyright © 2006 Guilherme de S. Pastore\n"
    "Copyright © 2007–2016 Christian Persch";
  char *licence_text;
  GKeyFile *key_file;
  GBytes *bytes;
  const guint8 *data;
  gsize data_len;
  GError *error = NULL;
  char **authors, **contributors, **artists, **documenters, **array_strv;
  gsize n_authors = 0, n_contributors = 0, n_artists = 0, n_documenters = 0 , i;
  GPtrArray *array;
  gs_free char *comment;
  gs_free char *vte_version;
  GtkWindow *dialog;

  bytes = g_resources_lookup_data (TERMINAL_RESOURCES_PATH_PREFIX "/ui/terminal.about",
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

  vte_version = g_strdup_printf (_("Using VTE version %u.%u.%u"),
                                 vte_get_major_version (),
                                 vte_get_minor_version (),
                                 vte_get_micro_version ());

  comment = g_strdup_printf("%s\n%s %s",
                            _("A terminal emulator for the GNOME desktop"),
                            vte_version,
                            vte_get_features ());

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         /* Hold the application while the window is shown */
                         "application", terminal_app_get (),
                         "program-name", _("GNOME Terminal"),
                         "copyright", copyright,
                         "comments", comment,
                         "version", VERSION,
                         "authors", array_strv,
                         "artists", artists,
                         "documenters", documenters,
                         "license", licence_text,
                         "wrap-license", TRUE,
                         "website", ABOUT_URL,
                         "translator-credits", _("translator-credits"),
                         "logo-icon-name", GNOME_TERMINAL_ICON_NAME,
                         NULL);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (dialog);

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
                        TerminalURLFlavor flavor,
                        guint32 user_time)
{
  gs_free_error GError *error = NULL;
  gs_free char *uri = NULL;

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
    }
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
      gs_unref_object GFile *file;
      gs_free char *path;

      file = g_file_new_for_uri (uris[i]);

      path = g_file_get_path (file);
      if (path)
        {
          char *quoted;

          quoted = g_shell_quote (path);
          g_free (uris[i]);

          uris[i] = quoted;
        }
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
  gs_unref_object GtkBuilder *builder;
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
    GtkWidget *action_area;

    main_object = gtk_builder_get_object (builder, main_object_name);
    g_object_set_data_full (main_object, "builder", g_object_ref (builder), (GDestroyNotify) g_object_unref);
    g_signal_connect (main_object, "destroy", G_CALLBACK (main_object_destroy_cb), NULL);

    /* Fixup dialogue padding, #735242 */
    if (GTK_IS_DIALOG (main_object) &&
        (action_area = (GtkWidget *) gtk_builder_get_object (builder, "dialog-action-area"))) {
      gtk_widget_set_margin_left   (action_area, 5);
      gtk_widget_set_margin_right  (action_area, 5);
      gtk_widget_set_margin_top    (action_area, 5);
      gtk_widget_set_margin_bottom (action_area, 5);
    }
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
  gs_unref_object GSettings *child_settings;
  GString *buf;
  gs_free char *host;
  int port;
  gboolean is_http;

  is_http = (strcmp (child_schema_id, "http") == 0);

  child_settings = g_settings_get_child (proxy_settings, child_schema_id);

  host = g_settings_get_string (child_settings, "host");
  port = g_settings_get_int (child_settings, "port");
  if (host[0] == '\0' || port == 0)
    return;

  buf = g_string_sized_new (64);

  g_string_append_printf (buf, "%s://", proxy_scheme);

  if (is_http &&
      g_settings_get_boolean (child_settings, "use-authentication"))
    {
      gs_free char *user;

      user = g_settings_get_string (child_settings, "authentication-user");
      if (user[0])
        {
          gs_free char *password;

          g_string_append_uri_escaped (buf, user, NULL, TRUE);

          password = g_settings_get_string (child_settings, "authentication-password");
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
}

static void
setup_autoconfig_proxy_env (GSettings *proxy_settings,
                            GHashTable *env_table)
{
  /* XXX  Not sure what to do with this.  See bug #596688.
  gs_free char *url;

  url = g_settings_get_string (proxy_settings, "autoconfig-url");
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
  gs_strfreev char **ignore;
  int i;

  g_settings_get (proxy_settings, "ignore-hosts", "^as", &ignore);
  if (ignore == NULL)
    return;

  buf = g_string_sized_new (64);
  for (i = 0; ignore[i] != NULL; ++i)
    {
      if (buf->len)
        g_string_append_c (buf, ',');
      g_string_append (buf, ignore[i]);
    }

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

static void
free_url_handler (UrlHandler *uh)
{
  if (uh) {
    if (uh->match) g_free(uh->match);
    if (uh->rewrite) g_free(uh->rewrite);
    g_free(uh);
  }
}

/**
 * terminal_util_load_url_handlers:
 *
 * Returns: loads custom-url handling patterns
 */
GHashTable *
terminal_util_load_url_handlers (void)
{
  GError *error = NULL;
  GKeyFile *key_file = NULL;
  gsize n_groups;
  char **groups = NULL;
  GHashTable *handlers = NULL;
  gs_free char* rcpath = NULL;

  const char *home = g_getenv("HOME");
  if (home == NULL)
    return NULL;
  key_file = g_key_file_new ();
  rcpath = g_strdup_printf("%s/.gnome-terminal-customurls.rc", home);
  if (!g_key_file_load_from_file (key_file, rcpath, G_KEY_FILE_NONE, &error)) {
    g_key_file_free (key_file);
    return NULL;
  }
  groups = g_key_file_get_groups (key_file, &n_groups);
  handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    NULL, (GDestroyNotify) free_url_handler);
  for (gsize i = 0; i < n_groups; ++i) {
    UrlHandler *uh = g_new0(UrlHandler, 1);
    if (!uh) {
      g_printerr ("Failed to alloc UrlHandler\n");
      g_hash_table_destroy(handlers);
      handlers = NULL;
      break;
    }
    uh->match = g_key_file_get_string (key_file, groups[i], "Match", NULL);
    uh->rewrite = g_key_file_get_string (key_file, groups[i], "Rewrite", NULL);
    if (!uh->match && !uh->rewrite) {
      g_printerr ("UrlHandler: couldn't read Match/Rewrite\n");
      free_url_handler(uh);
      continue;
    }
    uh->_regex = vte_regex_new_for_match (uh->match, -1,
                                          PCRE2_UTF | PCRE2_NO_UTF_CHECK |
                                          PCRE2_MULTILINE, &error);
    g_assert_no_error (error);
    if (!vte_regex_jit (uh->_regex, PCRE2_JIT_COMPLETE, &error) ||
        !vte_regex_jit (uh->_regex, PCRE2_JIT_PARTIAL_SOFT, &error)) {
      g_printerr ("Failed to JIT regex '%s': %s\n",
                  (char*)uh->match, error->message);
      g_clear_error (&error);
      free_url_handler(uh);
      continue;
    }
    g_hash_table_replace (handlers, uh->match, uh);
  }

  g_free (groups);
  g_key_file_free (key_file);
  return handlers;
}

/**
 * terminal_util_get_etc_shells:
 *
 * Returns: (transfer full) the contents of /etc/shells
 */
char **
terminal_util_get_etc_shells (void)
{
  GError *err = NULL;
  gsize len;
  gs_free char *contents = NULL;
  char *str, *nl, *end;
  GPtrArray *arr;

  if (!g_file_get_contents ("/etc/shells", &contents, &len, &err) || len == 0)
    return NULL;

  arr = g_ptr_array_new ();
  str = contents;
  end = contents + len;
  while (str < end && (nl = strchr (str, '\n')) != NULL) {
    if (str != nl) /* non-empty? */
      g_ptr_array_add (arr, g_strndup (str, nl - str));
    str = nl + 1;
  }
  /* Anything non-empty left? */
  if (str < end && str[0])
    g_ptr_array_add (arr, g_strdup (str));

  g_ptr_array_add (arr, NULL);
  return (char **) g_ptr_array_free (arr, FALSE);
}

/**
 * terminal_util_get_is_shell:
 * @command: a string
 *
 * Returns wether @command is a valid shell as defined by the contents of /etc/shells.
 *
 * Returns: whether @command is a shell
 */
gboolean
terminal_util_get_is_shell (const char *command)
{
  gs_strfreev char **shells;
  guint i;

  shells = terminal_util_get_etc_shells ();
  if (shells == NULL)
    return FALSE;

  for (i = 0; shells[i]; i++)
    if (g_str_equal (command, shells[i]))
      return TRUE;

  return FALSE;
}

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

  color->alpha = 1.0;
  *result = color;
  return TRUE;
}

/**
 * terminal_g_settings_new:
 * @schema_id: a settings schema ID
 * @mandatory_key: the name of a key that must exist in the schema
 * @mandatory_key_type: the expected value type of @mandatory_key
 *
 * Creates a #GSettings for @schema_id, if this schema exists and
 * has a key named @mandatory_key (if non-%NULL) with the value type
 * @mandatory_key_type.
 *
 * Returns: (transfer full): a new #GSettings, or %NULL
 */
GSettings *
terminal_g_settings_new (const char *schema_id,
                         const char *mandatory_key,
                         const GVariantType *mandatory_key_type)
{
  gs_unref_settings_schema GSettingsSchema *schema;

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            schema_id,
                                            TRUE);
  if (schema == NULL)
    return NULL;

  if (mandatory_key) {
    gs_unref_settings_schema_key GSettingsSchemaKey *key;

    key = g_settings_schema_get_key (schema, mandatory_key);
    if (key == NULL)
      return NULL;

    if (!g_variant_type_equal (g_settings_schema_key_get_value_type (key),
                               mandatory_key_type))
      return NULL;
  }

  return g_settings_new_full (schema, NULL, NULL);
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
  gs_free char *str;

  str = gdk_rgba_to_string (color);
  g_settings_set_string (settings, key, str);
}

static gboolean
as_to_rgba_palette (GVariant *variant,
                    gpointer *result,
                    gpointer user_data)
{
  gsize *n_colors = user_data;
  gs_free GdkRGBA *colors = NULL;
  gsize n = 0;
  GVariantIter iter;
  const char *str;
  gsize i;

  /* Fallback */
  if (variant == NULL)
    goto out;

  g_variant_iter_init (&iter, variant);
  n = g_variant_iter_n_children (&iter);
  colors = g_new (GdkRGBA, n);

  i = 0;
  while (g_variant_iter_next (&iter, "&s", &str)) {
    if (!gdk_rgba_parse (&colors[i++], str)) {
      return FALSE;
    }
  }

 out:
  gs_transfer_out_value (result, &colors);
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
  gs_strfreev char **strv;
  gsize i;

  strv = g_new (char *, n_colors + 1);
  for (i = 0; i < n_colors; ++i)
    strv[i] = gdk_rgba_to_string (&colors[i]);
  strv[n_colors] = NULL;

  g_settings_set (settings, key, "^as", strv);
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

/*
 * "1234567", "'", 3 -> "1'234'567"
 */
static char *
add_separators (const char *in, const char *sep, int groupby)
{
  int inlen, outlen, seplen, firstgrouplen;
  char *out, *ret;

  if (in[0] == '\0')
    return g_strdup("");

  inlen = strlen(in);
  seplen = strlen(sep);
  outlen = inlen + (inlen - 1) / groupby * seplen;
  ret = out = g_malloc(outlen + 1);

  firstgrouplen = (inlen - 1) % groupby + 1;
  strncpy(out, in, firstgrouplen);
  in += firstgrouplen;
  out += firstgrouplen;

  while (*in != '\0') {
    strncpy(out, sep, seplen);
    out += seplen;
    strncpy(out, in, groupby);
    in += groupby;
    out += groupby;
  }

  g_assert(out - ret == outlen);
  *out = '\0';
  return ret;
}

/**
 * terminal_util_number_info:
 * @str: a dec or hex number as string
 *
 * Returns: (transfer full): Useful info about @str, or %NULL if it's too large
 */
char *
terminal_util_number_info (const char *str)
{
  gs_free char *decstr = NULL;
  gs_free char *hextmp = NULL;
  gs_free char *hexstr = NULL;
  gs_free char *magnitudestr = NULL;
  unsigned long long num;
  gboolean exact = TRUE;
  gboolean hex = FALSE;
  const char *thousep;

  errno = 0;
  /* Deliberately not handle octal */
  if (str[1] == 'x' || str[1] == 'X') {
    num = strtoull(str + 2, NULL, 16);
    hex = TRUE;
  } else {
    num = strtoull(str, NULL, 10);
  }
  if (errno) {
    return NULL;
  }

  /* No use in dec-hex conversion for so small numbers */
  if (num < 10) {
    return NULL;
  }

  /* Group the decimal digits */
  thousep = nl_langinfo(THOUSEP);
  if (thousep[0] != '\0') {
    /* If thousep is nonempty, use printf's magic which can handle
       more complex separating logics, e.g. 2+2+2+3 for some locales */
    decstr = g_strdup_printf("%'llu", num);
  } else {
    /* If, however, thousep is empty, override it with a space so that we
       do always group the digits (that's the whole point of this feature;
       the choice of space guarantees not conflicting with the decimal separator) */
    gs_free char *tmp = g_strdup_printf("%llu", num);
    thousep = " ";
    decstr = add_separators(tmp, thousep, 3);
  }

  /* Group the hex digits by 4 using the same nonempty separator */
  hextmp = g_strdup_printf("%llx", num);
  hexstr = add_separators(hextmp, thousep, 4);

  /* Find out the human-readable magnitude, e.g. 15.99 Mi */
  if (num >= 1024) {
    int power = 0;
    while (num >= 1024 * 1024) {
      power++;
      if (num % 1024 != 0)
        exact = FALSE;
      num /= 1024;
    }
    /* Show 2 fraction digits, always rounding downwards. Printf rounds floats to the nearest representable value,
       so do the calculation with integers until we get 100-fold the desired value, and then switch to float. */
    if (100 * num % 1024 != 0)
      exact = FALSE;
    num = 100 * num / 1024;
    magnitudestr = g_strdup_printf(" %s %.2f %ci", exact ? "=" : "≈", (double) num / 100, "KMGTPE"[power]);
  } else {
    magnitudestr = g_strdup("");
  }

  return g_strdup_printf(hex ? "0x%2$s = %1$s%3$s" : "%s = 0x%s%s", decstr, hexstr, magnitudestr);
}

/**
 * terminal_util_uri_fixup:
 * @uri: The URI to verify and maybe fixup
 * @error: a #GError that is returned in case of errors
 *
 * Checks if gnome-terminal should attempt to handle the given URI,
 * and rewrites if necessary.
 *
 * Currently URIs of "file://some-other-host/..." are refused because
 * GIO (e.g. gtk_show_uri()) silently strips off the remote hostname
 * and opens the local counterpart which is incorrect and misleading.
 *
 * Furthermore, once the hostname is verified, it is stripped off to
 * avoid potential confusion around short hostname vs. fqdn, and to
 * work around bug 781800 (LibreOffice bug 107461).
 *
 * Returns: The possibly rewritten URI if gnome-terminal should attempt
 *   to handle it, NULL if it should refuse to handle.
 */
char *
terminal_util_uri_fixup (const char *uri,
                         GError **error)
{
  gs_free char *filename;
  gs_free char *hostname;

  filename = g_filename_from_uri (uri, &hostname, NULL);
  if (filename != NULL &&
      hostname != NULL &&
      hostname[0] != '\0') {
    /* "file" scheme and nonempty hostname */
    if (g_ascii_strcasecmp (hostname, "localhost") == 0 ||
        g_ascii_strcasecmp (hostname, g_get_host_name()) == 0) {
      /* hostname corresponds to localhost */
      char *slash1, *slash2, *slash3;

      /* We shouldn't enter this branch in case of URIs like
       * "file:/etc/passwd", but just in case we do, or encounter
       * something else unexpected, leave the URI unchanged. */
      slash1 = strchr(uri, '/');
      if (slash1 == NULL)
        return g_strdup (uri);

      slash2 = slash1 + 1;
      if (*slash2 != '/')
        return g_strdup (uri);

      slash3 = strchr(slash2 + 1, '/');
      if (slash3 == NULL)
        return g_strdup (uri);

      return g_strdup_printf("%.*s%s",
                             (int) (slash2 + 1 - uri),
                             uri,
                             slash3);
    } else {
      /* hostname refers to another host (e.g. the OSC 8 escape sequence
       * was correctly emitted by a utility inside an ssh session) */
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                         _("“file” scheme with remote hostname not supported"));
      return NULL;
    }
  } else {
    /* "file" scheme without hostname, or some other scheme */
    return g_strdup (uri);
  }
}

/**
 * terminal_util_hyperlink_uri_label:
 * @uri: a URI
 *
 * Formats @uri to be displayed in a tooltip.
 * Performs URI-decoding and converts IDN hostname to UTF-8.
 *
 * Returns: (transfer full): The human readable URI as plain text
 */
char *terminal_util_hyperlink_uri_label (const char *uri)
{
  gs_free char *unesc = NULL;
  gboolean replace_hostname;

  if (uri == NULL)
    return NULL;

  unesc = g_uri_unescape_string(uri, NULL);
  if (unesc == NULL)
    unesc = g_strdup(uri);

  if (g_ascii_strncasecmp(unesc, "ftp://", 6) == 0 ||
      g_ascii_strncasecmp(unesc, "http://", 7) == 0 ||
      g_ascii_strncasecmp(unesc, "https://", 8) == 0) {
    gs_free char *unidn = NULL;
    char *hostname = strchr(unesc, '/') + 2;
    char *hostname_end = strchrnul(hostname, '/');
    char save = *hostname_end;
    *hostname_end = '\0';
    unidn = g_hostname_to_unicode(hostname);
    replace_hostname = unidn != NULL && g_ascii_strcasecmp(unidn, hostname) != 0;
    *hostname_end = save;
    if (replace_hostname) {
      char *new_unesc = g_strdup_printf("%.*s%s%s",
                                        (int) (hostname - unesc),
                                        unesc,
                                        unidn,
                                        hostname_end);
      g_free(unesc);
      unesc = new_unesc;
    }
  }

  return terminal_util_utf8_make_valid (unesc, -1);
}

/**
 * terminal_util_utf8_make_valid:
 *
 * Just as g_utf8_make_valid().
 *
 * FIXME: Use g_utf8_make_valid() instead once we require glib >= 2.52.
 */
gchar *
terminal_util_utf8_make_valid (const gchar *str,
                               gssize       len)
{
  /* copied from glib's g_utf8_make_valid() implementation */
  GString *string;
  const gchar *remainder, *invalid;
  gsize remaining_bytes, valid_bytes;

  g_return_val_if_fail (str != NULL, NULL);

  if (len < 0)
    len = strlen (str);

  string = NULL;
  remainder = str;
  remaining_bytes = len;

  while (remaining_bytes != 0)
    {
      if (g_utf8_validate (remainder, remaining_bytes, &invalid))
	break;
      valid_bytes = invalid - remainder;

      if (string == NULL)
	string = g_string_sized_new (remaining_bytes);

      g_string_append_len (string, remainder, valid_bytes);
      /* append U+FFFD REPLACEMENT CHARACTER */
      g_string_append (string, "\357\277\275");

      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
    }

  if (string == NULL)
    return g_strndup (str, len);

  g_string_append (string, remainder);
  g_string_append_c (string, '\0');

  g_assert (g_utf8_validate (string->str, -1, NULL));

  return g_string_free (string, FALSE);
}
