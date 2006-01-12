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
#include "terminal.h"
#include "terminal-widget.h"
#include "skey-popup.h"
#include <glade/glade.h>
#include "skey/skey.h"
#include <stdlib.h>
#include <string.h>

#define SKEY_PREFIX "s/key "
#define OTP_PREFIX  "otp-"

static gboolean
extract_seq_and_seed (const gchar  *skey_match,
		      gint         *seq,
		      gchar       **seed)
{
  gchar *end_ptr = NULL;

  *seq = strtol (skey_match + strlen (SKEY_PREFIX), &end_ptr, 0);

  if (end_ptr == NULL || *end_ptr == '\000')
    return FALSE;

  *seed = g_strdup (end_ptr + 1);

  return TRUE;
}

static gboolean
extract_hash_seq_and_seed (const gchar  *otp_match,
			   gint         *hash,
	  	           gint         *seq,
		           gchar       **seed)
{
  gchar *end_ptr = NULL;
  gchar *p = otp_match + strlen (OTP_PREFIX);
  gint len = 3;

  if (strncmp (p, "md4", 3) == 0)
    *hash = MD4;

  else if (strncmp (p, "md5", 3) == 0)
    *hash = MD5;

  else if (strncmp (p, "sha1", 4) == 0)
    {
      *hash = SHA1;
      len++;
    }

  else
    return FALSE;

  p += len;

  /* RFC mandates the following skipping */
  while (*p == ' ' || *p == '\t')
    {
      if (*p == '\0')
	return FALSE;

      p++;
    }

  *seq = strtol (p, &end_ptr, 0);

  if (end_ptr == NULL || *end_ptr == '\000')
    return FALSE;

  p = end_ptr;

  while (*p == ' ' || *p == '\t')
    {
      if (*p == '\0')
	return FALSE;
      p++;
    }

  *seed = strdup (p);

  return TRUE;
}

void
terminal_skey_do_popup (TerminalScreen *screen,
			GtkWindow      *transient_parent,
			const gchar    *skey_match)
{
  static GtkWidget *dialog = NULL;
  GtkWidget *entry;
  GtkWidget *ok_button;
  gint seq;
  gchar *seed;
  gint hash = MD5;

  if (strncmp (SKEY_PREFIX, skey_match, strlen (SKEY_PREFIX)) == 0)
    {
      if (!extract_seq_and_seed (skey_match, &seq, &seed))
	{
	  terminal_util_show_error_dialog (GTK_WINDOW (transient_parent), NULL,
					   _("The text you clicked on doesn't "
					     "seem to be a valid S/Key "
					     "challenge."));
	  return;
	}
    }
  else
    {
      if (!extract_hash_seq_and_seed (skey_match, &hash, &seq, &seed))
	{
	  terminal_util_show_error_dialog (GTK_WINDOW (transient_parent), NULL,
					   _("The text you clicked on doesn't "
					     "seem to be a valid OTP "
					     "challenge."));
	  return;
	}
    }

  if (dialog == NULL)
    {
      GladeXML *xml;
      GtkWidget *title;
      gchar *title_text;

      xml = terminal_util_load_glade_file (TERM_GLADE_FILE,
                                           "skey-dialog",
                                           transient_parent);

      if (xml == NULL)
        return;

      dialog = glade_xml_get_widget (xml, "skey-dialog");
      title = glade_xml_get_widget (xml, "label61");
      entry = glade_xml_get_widget (xml, "skey-entry");
      ok_button = glade_xml_get_widget (xml, "skey-ok-button");

      title_text = g_strdup_printf ("<big><b>%s</b></big>",
				    gtk_label_get_text (GTK_LABEL (title)));
      gtk_label_set_label (GTK_LABEL (title), title_text);
      g_free (title_text);

      g_object_set_data (G_OBJECT (dialog), "skey-entry", entry);      
      g_object_set_data (G_OBJECT (dialog), "skey-ok-button", ok_button);

      g_object_add_weak_pointer (G_OBJECT (dialog), (void**) &dialog);

      g_object_unref (G_OBJECT (xml));
    }

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
  				GTK_WINDOW (transient_parent));
  entry = g_object_get_data (G_OBJECT (dialog), "skey-entry");
  ok_button = g_object_get_data (G_OBJECT (dialog), "skey-ok-button");
  gtk_widget_grab_focus (entry);
  gtk_widget_grab_default (ok_button);
  gtk_entry_set_text (GTK_ENTRY (entry), "");

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_present (GTK_WINDOW (dialog));
  
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      const gchar *password;
      gchar *response;
      
      password = gtk_entry_get_text (GTK_ENTRY (entry));
      response = skey (hash, seq, seed, password);
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

  gtk_widget_destroy (dialog);
  g_free (seed);
}
