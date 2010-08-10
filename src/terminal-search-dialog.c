/*
 * Copyright © 2005 Paolo Maggi
 * Copyright © 2010 Red Hat (Red Hat author: Behdad Esfahbod)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include "terminal-search-dialog.h"
#include "terminal-util.h"

#define HISTORY_MIN_ITEM_LEN 3
#define HISTORY_LENGTH 10

static GQuark
get_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (!quark))
    quark = g_quark_from_static_string ("GT:data");

  return quark;
}


#define TERMINAL_SEARCH_DIALOG_GET_PRIVATE(object) \
  ((TerminalSearchDialogPrivate *) g_object_get_qdata (G_OBJECT (object), get_quark ()))

#define GET_FLAG(widget) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->widget))

typedef struct _TerminalSearchDialogPrivate
{
  GtkWidget *search_label;
  GtkWidget *search_entry;
  GtkWidget *search_text_entry;
  GtkWidget *match_case_checkbutton;
  GtkWidget *entire_word_checkbutton;
  GtkWidget *regex_checkbutton;
  GtkWidget *backwards_checkbutton;
  GtkWidget *wrap_around_checkbutton;

  GtkListStore *store;
  GtkEntryCompletion *completion;

  /* Cached regex */
  GRegex *regex;
  GRegexCompileFlags regex_compile_flags;
} TerminalSearchDialogPrivate;


static void update_sensitivity (void *unused,
				GtkWidget *dialog);
static void response_handler (GtkWidget *dialog,
			      gint       response_id,
			      gpointer   data);
static void terminal_search_dialog_private_destroy (TerminalSearchDialogPrivate *priv);


GtkWidget *
terminal_search_dialog_new (GtkWindow   *parent)
{
  GtkWidget *dialog;
  TerminalSearchDialogPrivate *priv;
  GtkListStore *store;
  GtkEntryCompletion *completion;

  priv = g_new0 (TerminalSearchDialogPrivate, 1);

  if (!terminal_util_load_builder_file ("find-dialog.ui",
					"find-dialog", &dialog,
					"search-label", &priv->search_label,
					"search-entry", &priv->search_entry,
					"match-case-checkbutton", &priv->match_case_checkbutton,
					"entire-word-checkbutton", &priv->entire_word_checkbutton,
					"regex-checkbutton", &priv->regex_checkbutton,
					"search-backwards-checkbutton", &priv->backwards_checkbutton,
					"wrap-around-checkbutton", &priv->wrap_around_checkbutton,
					NULL))
  {
    g_free (priv);
    return NULL;
  }

  g_object_set_qdata_full (G_OBJECT (dialog), get_quark (), priv,
			   (GDestroyNotify) terminal_search_dialog_private_destroy);


  priv->search_text_entry = gtk_bin_get_child (GTK_BIN (priv->search_entry));
  gtk_widget_set_size_request (priv->search_entry, 300, -1);

  priv->store = store = gtk_list_store_new (1, G_TYPE_STRING);
  g_object_set (G_OBJECT (priv->search_entry),
		"model", store,
		"text-column", 0,
		NULL);

  priv->completion = completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
  gtk_entry_completion_set_text_column (completion, 0);
  gtk_entry_completion_set_minimum_key_length (completion, HISTORY_MIN_ITEM_LEN);
  gtk_entry_completion_set_popup_completion (completion, FALSE);
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_set_completion (GTK_ENTRY (priv->search_text_entry), completion);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);

  gtk_entry_set_activates_default (GTK_ENTRY (priv->search_text_entry), TRUE);
  g_signal_connect (priv->search_text_entry, "changed", G_CALLBACK (update_sensitivity), dialog);
  g_signal_connect (priv->regex_checkbutton, "toggled", G_CALLBACK (update_sensitivity), dialog);

  g_signal_connect (dialog, "response", G_CALLBACK (response_handler), NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  return GTK_WIDGET (dialog);
}

void
terminal_search_dialog_present (GtkWidget *dialog)
{
  TerminalSearchDialogPrivate *priv;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);
  g_return_if_fail (priv);

  gtk_window_present (GTK_WINDOW (dialog));
  gtk_widget_grab_focus (priv->search_text_entry);
}

static void
terminal_search_dialog_private_destroy (TerminalSearchDialogPrivate *priv)
{

  if (priv->regex)
    g_regex_unref (priv->regex);

  g_object_unref (priv->store);
  g_object_unref (priv->completion);

  g_free (priv);
}


static void
update_sensitivity (void *unused, GtkWidget *dialog)
{
  TerminalSearchDialogPrivate *priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);
  const gchar *search_string;
  gboolean valid;

  if (priv->regex) {
    g_regex_unref (priv->regex);
    priv->regex = NULL;
  }

  search_string = gtk_entry_get_text (GTK_ENTRY (priv->search_text_entry));
  g_return_if_fail (search_string != NULL);

  valid = *search_string != '\0';

  if (valid && GET_FLAG (regex_checkbutton)) {
    /* Check that the regex is valid */
    valid = NULL != terminal_search_dialog_get_regex (dialog);
    /* TODO show the error message somewhere */
  }

  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, valid);
}

static gboolean
remove_item (GtkListStore *store,
	     const gchar  *text)
{
  GtkTreeIter iter;

  g_return_val_if_fail (text != NULL, FALSE);

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    return FALSE;

  do {
    gchar *item_text;

    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &item_text, -1);

    if (item_text != NULL && strcmp (item_text, text) == 0) {
      gtk_list_store_remove (store, &iter);
      g_free (item_text);
      return TRUE;
    }

    g_free (item_text);
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  return FALSE;
}

static void
clamp_list_store (GtkListStore *store,
		  guint         max)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  /* -1 because TreePath counts from 0 */
  path = gtk_tree_path_new_from_indices (max - 1, -1);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
    while (1)
      if (!gtk_list_store_remove (store, &iter))
	break;

  gtk_tree_path_free (path);
}

static void
history_entry_insert (GtkListStore *store,
		      const gchar  *text)
{
  GtkTreeIter iter;

  g_return_if_fail (text != NULL);

  if (g_utf8_strlen (text, -1) <= HISTORY_MIN_ITEM_LEN)
    return;

  /* remove the text from the store if it was already
   * present. If it wasn't, clamp to max history - 1
   * before inserting the new row, otherwise appending
   * would not work */

  if (!remove_item (store, text))
    clamp_list_store (store, HISTORY_LENGTH - 1);

  gtk_list_store_insert (store, &iter, 0);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

static void
response_handler (GtkWidget *dialog,
		  gint       response_id,
		  gpointer   data)
{
  TerminalSearchDialogPrivate *priv;
  const gchar *str;

  if (response_id != GTK_RESPONSE_ACCEPT) {
    gtk_widget_hide (dialog);
    return;
  }

  priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);

  str = gtk_entry_get_text (GTK_ENTRY (priv->search_text_entry));
  if (*str != '\0')
    history_entry_insert (priv->store, str);
}


void
terminal_search_dialog_set_search_text (GtkWidget   *dialog,
					const gchar *text)
{
  TerminalSearchDialogPrivate *priv;

  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (text != NULL);

  priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);
  g_return_if_fail (priv);

  gtk_entry_set_text (GTK_ENTRY (priv->search_text_entry), text);

  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
				     GTK_RESPONSE_ACCEPT,
				     (*text != '\0'));
}

const gchar *
terminal_search_dialog_get_search_text (GtkWidget *dialog)
{
  TerminalSearchDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);
  g_return_val_if_fail (priv, NULL);

  return gtk_entry_get_text (GTK_ENTRY (priv->search_text_entry));
}

TerminalSearchFlags
terminal_search_dialog_get_search_flags (GtkWidget *dialog)
{
  TerminalSearchDialogPrivate *priv;
  TerminalSearchFlags flags = 0;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), flags);

  priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);
  g_return_val_if_fail (priv, flags);

  if (GET_FLAG (backwards_checkbutton))
    flags |= TERMINAL_SEARCH_FLAG_BACKWARDS;

  if (GET_FLAG (wrap_around_checkbutton))
    flags |= TERMINAL_SEARCH_FLAG_WRAP_AROUND;

  return flags;
}

GRegex *
terminal_search_dialog_get_regex (GtkWidget *dialog)
{
  TerminalSearchDialogPrivate *priv;
  GRegexCompileFlags compile_flags;
  const char *text, *pattern;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  priv = TERMINAL_SEARCH_DIALOG_GET_PRIVATE (dialog);
  g_return_val_if_fail (priv, NULL);

  pattern = text = terminal_search_dialog_get_search_text (dialog);

  compile_flags = G_REGEX_OPTIMIZE;

  if (!GET_FLAG (match_case_checkbutton))
    compile_flags |= G_REGEX_CASELESS;

  if (GET_FLAG (regex_checkbutton))
    compile_flags |= G_REGEX_MULTILINE;
  else
    pattern = g_regex_escape_string (text, -1);

  if (GET_FLAG (entire_word_checkbutton)) {
    const char *old_pattern = pattern;
    pattern = g_strdup_printf ("\\b%s\\b", pattern);
    if (old_pattern != text)
      g_free ((char *) old_pattern);
  }

  if (!priv->regex || priv->regex_compile_flags != compile_flags) {
    priv->regex_compile_flags = compile_flags;
    if (priv->regex)
      g_regex_unref (priv->regex);

    /* TODO Error handling */
    priv->regex = g_regex_new (pattern, compile_flags, 0, NULL);
  }

  if (pattern != text)
    g_free ((char *) pattern);

  return priv->regex;
}

