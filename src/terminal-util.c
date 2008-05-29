/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
 *
 * This file is part of gnome-terminal.
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

#include "terminal-intl.h"

#include <locale.h>

#include "terminal.h"
#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-window.h"
#include "terminal-util.h"
#include "profile-editor.h"
#include "encoding.h"
#include <gconf/gconf-client.h>
#include <libgnome/gnome-program.h>
#include <libgnome/gnome-help.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-url.h>
#include <libgnomeui/gnome-client.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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

  gnome_help_display ("gnome-terminal", topic, &err);
  
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

GladeXML*
terminal_util_load_glade_file (const char *filename,
                               const char *widget_root,
                               GtkWindow  *error_dialog_parent)
{
  char *path;
  GladeXML *xml;

  xml = NULL;
  path = g_strconcat ("./", filename, NULL);
  
  if (g_file_test (path,
                   G_FILE_TEST_EXISTS))
    {
      /* Try current dir, for debugging */
      xml = glade_xml_new (path,
                           widget_root,
                           GETTEXT_PACKAGE);
    }
  
  if (xml == NULL)
    {
      g_free (path);
      
      path = g_build_filename (TERM_GLADE_DIR, filename, NULL);

      xml = glade_xml_new (path,
                           widget_root,
                           GETTEXT_PACKAGE);
    }

  if (xml == NULL)
    {
      static GtkWidget *no_glade_dialog = NULL;

      terminal_util_show_error_dialog (error_dialog_parent, &no_glade_dialog, 
                                       _("The file \"%s\" is missing. This indicates that the application is installed incorrectly."), path);
    }

  g_free (path);

  return xml;
}

void
terminal_util_open_url (GtkWidget *parent,
                        const char *orig_url,
                        TerminalURLFlavour flavor)
{
  GError *error = NULL;
  char *url;
  
  g_return_if_fail (orig_url != NULL);

  switch (flavor)
    {
    case FLAVOR_DEFAULT_TO_HTTP:
      url = g_strdup_printf ("http:%s", orig_url);
      break;
    case FLAVOR_EMAIL:
      if (strncmp ("mailto:", orig_url, 7))
	url = g_strdup_printf ("mailto:%s", orig_url);
      else
	url = g_strdup (orig_url);
      break;
    case FLAVOR_AS_IS:
      url = g_strdup (orig_url);
      break;
    default:
      url = NULL;
      g_assert_not_reached ();
    }

  if (!gnome_url_show_on_screen (url, gtk_widget_get_screen (parent), &error))
    {
      terminal_util_show_error_dialog (GTK_WINDOW (parent), NULL,
                                       _("Could not open the address \"%s\":\n%s"),
                                       url, error->message);
      
      g_error_free (error);
    }

  g_free (url);
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
