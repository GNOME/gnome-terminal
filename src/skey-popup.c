/*
 * Copyright © 2002 Jonathan Blandford <jrb@gnome.org>
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

#include "terminal-intl.h"

#include "terminal-util.h"
#include "terminal-screen.h"
#include "skey-popup.h"
#include "skey/skey.h"
#include <stdlib.h>
#include <string.h>

#define SKEY_PREFIX "s/key "
#define OTP_PREFIX  "otp-"

typedef struct {
  TerminalScreen *screen;
  char *seed;
  int seq;
  int hash;
} SkeyData;

static void
skey_data_free (SkeyData *data)
{
  g_free (data->seed);
  g_free (data);
}

static gboolean
extract_seq_and_seed (const gchar  *skey_match,
		      gint         *seq,
		      gchar       **seed)
{
  gchar *end_ptr = NULL;

  /* FIXME: use g_ascii_strtoll */
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
  const gchar *p = otp_match + strlen (OTP_PREFIX);
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

  /* FIXME: use g_ascii_strtoll */
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

  *seed = g_strdup (p);
  return TRUE;
}

static void
skey_challenge_response_cb (GtkWidget *dialog,
                            int response_id,
                            SkeyData *data)
{  
  if (response_id == GTK_RESPONSE_OK)
    {
      GtkWidget *entry;
      const char *password;
      char *response;

      entry = g_object_get_data (G_OBJECT (dialog), "skey-entry");
      password = gtk_entry_get_text (GTK_ENTRY (entry));

      /* FIXME: fix skey to use g_malloc */
      response = skey (data->hash, data->seq, data->seed, password);
      if (response)
	{
          VteTerminal *vte_terminal = VTE_TERMINAL (data->screen);
          static const char newline[2] = "\n";

	  vte_terminal_feed_child (vte_terminal, response, strlen (response));
          vte_terminal_feed_child (vte_terminal, newline, strlen (newline));
	  free (response);
	}
    }

  gtk_widget_destroy (dialog);
}

void
terminal_skey_do_popup (GtkWindow *window,
                        TerminalScreen *screen,
			const gchar    *skey_match)
{
  GtkWidget *dialog, *label, *entry, *ok_button;
  char *title_text;
  char *seed;
  int seq;
  int hash = MD5;
  SkeyData *data;

  if (strncmp (SKEY_PREFIX, skey_match, strlen (SKEY_PREFIX)) == 0)
    {
      if (!extract_seq_and_seed (skey_match, &seq, &seed))
	{
	  terminal_util_show_error_dialog (window, NULL, NULL,
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
	  terminal_util_show_error_dialog (window, NULL, NULL,
					   _("The text you clicked on doesn't "
					     "seem to be a valid OTP "
					     "challenge."));
	  return;
	}
    }

  if (!terminal_util_load_builder_file ("skey-challenge.ui",
                                        "skey-dialog", &dialog,
                                        "skey-entry", &entry,
                                        "text-label", &label,
                                        "skey-ok-button", &ok_button,
                                        NULL))
    {
      g_free (seed);
      return;
    }

  title_text = g_strdup_printf ("<big><b>%s</b></big>",
                                gtk_label_get_text (GTK_LABEL (label)));
  gtk_label_set_label (GTK_LABEL (label), title_text);
  g_free (title_text);

  g_object_set_data (G_OBJECT (dialog), "skey-entry", entry);

  gtk_widget_grab_focus (entry);
  gtk_widget_grab_default (ok_button);
  gtk_entry_set_text (GTK_ENTRY (entry), "");

  gtk_window_set_transient_for (GTK_WINDOW (dialog), window);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  /* FIXME: make this dialogue close if the screen closes! */

  data = g_new (SkeyData, 1);
  data->hash = hash;
  data->seq = seq;
  data->seed = seed;
  data->screen = screen;

  g_signal_connect_data (dialog, "response",
                         G_CALLBACK (skey_challenge_response_cb),
                         data, (GClosureNotify) skey_data_free, 0);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}
