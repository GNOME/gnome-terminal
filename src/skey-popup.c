/*
 * Copyright (C) 2002 Jonathan Blandford <jrb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "terminal-intl.h"
#include "terminal-widget.h"
#include "skey-popup.h"
#include <glade/glade.h>
#include "skey/skey.h"
#include <stdlib.h>
#include <string.h>

#define SKEY_PREFIX "s/key "
static gboolean
extract_seq_and_seed (const gchar  *skey_match,
		      gint         *seq,
		      gchar       **seed)
{
  gchar *end_ptr = NULL;

  if (strncmp (SKEY_PREFIX, skey_match, strlen (SKEY_PREFIX)))
    return FALSE;

  *seq = strtol (skey_match + strlen (SKEY_PREFIX), &end_ptr, 0);

  if (end_ptr == NULL || *end_ptr == '\000')
    return FALSE;
  *seed = g_strdup (end_ptr + 1);
  return TRUE;
}

void
terminal_skey_do_popup (TerminalScreen *screen,
			GtkWindow      *transient_parent,
			const gchar    *skey_match)
{
  GtkWidget *dialog = NULL;
  GtkWidget *entry;
  GtkWidget *ok_button;
  gint seq;
  gchar *seed;

  if (!extract_seq_and_seed (skey_match, &seq, &seed))
    return;

  dialog = g_object_get_data (G_OBJECT (transient_parent), "skey-dialog");

  if (dialog == NULL)
    {
      GladeXML *xml;
      if (g_file_test ("./"TERM_GLADE_FILE,
                       G_FILE_TEST_EXISTS))
        {
          /* Try current dir, for debugging */
          xml = glade_xml_new ("./"TERM_GLADE_FILE,
                               "skey-dialog",
                               GETTEXT_PACKAGE);
        }
      else
        {
          xml = glade_xml_new (TERM_GLADE_DIR"/"TERM_GLADE_FILE,
                               "skey-dialog",
                               GETTEXT_PACKAGE);
        }

      if (xml == NULL)
        {
          static GtkWidget *no_glade_dialog = NULL;
          
          if (no_glade_dialog == NULL)
            {
              no_glade_dialog =
                gtk_message_dialog_new (transient_parent,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("The file \"%s\" is missing. This indicates that the application is installed incorrectly, so the profile editor can't be displayed."),
                                        TERM_GLADE_DIR"/"TERM_GLADE_FILE);
                                        
              g_signal_connect (G_OBJECT (no_glade_dialog),
                                "response",
                                G_CALLBACK (gtk_widget_destroy),
                                NULL);

              g_object_add_weak_pointer (G_OBJECT (no_glade_dialog),
                                         (void**)&no_glade_dialog);
            }

          gtk_window_present (GTK_WINDOW (no_glade_dialog));
	  g_free (seed);
          return;
        }
      
      dialog = glade_xml_get_widget (xml, "skey-dialog");
      entry = glade_xml_get_widget (xml, "skey-entry");
      ok_button = glade_xml_get_widget (xml, "skey-ok-button");
      g_object_set_data (G_OBJECT (transient_parent), "skey-dialog", dialog);      
      g_object_set_data (G_OBJECT (dialog), "skey-entry", entry);      
      g_object_set_data (G_OBJECT (dialog), "skey-ok-button", ok_button);      

    }

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
  				GTK_WINDOW (transient_parent));
  entry = g_object_get_data (G_OBJECT (dialog), "skey-entry");
  ok_button = g_object_get_data (G_OBJECT (dialog), "skey-ok-button");
  gtk_widget_grab_focus (entry);
  gtk_widget_grab_default (ok_button);
  gtk_entry_set_text (GTK_ENTRY (entry), "");

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      const gchar *password;
      gchar *response;

      
      password = gtk_entry_get_text (GTK_ENTRY (entry));
      response = skey (MD5, seq, seed, password);
      if (response)
	{
	  terminal_widget_write_data_to_child (terminal_screen_get_widget (screen),
					       response,
					       strlen (response));
	  terminal_widget_write_data_to_child (terminal_screen_get_widget (screen),
					       "\n",
					       strlen ("\n"));
	  free (response);
	}
    }

  gtk_widget_hide (dialog);
  g_free (seed);
}
