/* Encoding stuff */

/*
 * Copyright © 2002 Red Hat, Inc.
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

#include "terminal.h"

#include "encoding.h"

#include "terminal-profile.h"
#include "terminal-util.h"

#include <glade/glade.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktreestore.h>

#include <string.h>

/* Overview
 *
 * There's a list of character sets stored in gconf, indicating
 * which encodings to display in the encoding menu.
 * 
 * We have a pre-canned list of available encodings
 * (hardcoded in the table below) that can be added to
 * the encoding menu, and to give a human-readable name
 * to certain encodings.
 *
 * If the gconf list contains an encoding not in the
 * predetermined table, then that encoding is
 * labeled "user defined" but still appears in the menu.
 */

enum {
  TERMINAL_ENCODING_CURRENT_LOCALE,

  TERMINAL_ENCODING_ISO_8859_1,
  TERMINAL_ENCODING_ISO_8859_2,
  TERMINAL_ENCODING_ISO_8859_3,
  TERMINAL_ENCODING_ISO_8859_4,
  TERMINAL_ENCODING_ISO_8859_5,
  TERMINAL_ENCODING_ISO_8859_6,
  TERMINAL_ENCODING_ISO_8859_7,
  TERMINAL_ENCODING_ISO_8859_8,
  TERMINAL_ENCODING_ISO_8859_8_I,
  TERMINAL_ENCODING_ISO_8859_9,
  TERMINAL_ENCODING_ISO_8859_10,
  TERMINAL_ENCODING_ISO_8859_13,
  TERMINAL_ENCODING_ISO_8859_14,
  TERMINAL_ENCODING_ISO_8859_15,
  TERMINAL_ENCODING_ISO_8859_16,

  TERMINAL_ENCODING_UTF_7,
  TERMINAL_ENCODING_UTF_8,
  TERMINAL_ENCODING_UTF_16,
  TERMINAL_ENCODING_UCS_2,
  TERMINAL_ENCODING_UCS_4,

  TERMINAL_ENCODING_ARMSCII_8,
  TERMINAL_ENCODING_BIG5,
  TERMINAL_ENCODING_BIG5_HKSCS,
  TERMINAL_ENCODING_CP_866,

  TERMINAL_ENCODING_EUC_JP,
  TERMINAL_ENCODING_EUC_KR,
  TERMINAL_ENCODING_EUC_TW,

  TERMINAL_ENCODING_GB18030,
  TERMINAL_ENCODING_GB2312,
  TERMINAL_ENCODING_GBK,
  TERMINAL_ENCODING_GEOSTD8,
  TERMINAL_ENCODING_HZ,

  TERMINAL_ENCODING_IBM_850,
  TERMINAL_ENCODING_IBM_852,
  TERMINAL_ENCODING_IBM_855,
  TERMINAL_ENCODING_IBM_857,
  TERMINAL_ENCODING_IBM_862,
  TERMINAL_ENCODING_IBM_864,

  TERMINAL_ENCODING_ISO_2022_JP,
  TERMINAL_ENCODING_ISO_2022_KR,
  TERMINAL_ENCODING_ISO_IR_111,
  TERMINAL_ENCODING_JOHAB,
  TERMINAL_ENCODING_KOI8_R,
  TERMINAL_ENCODING_KOI8_U,

  TERMINAL_ENCODING_MAC_ARABIC,
  TERMINAL_ENCODING_MAC_CE,
  TERMINAL_ENCODING_MAC_CROATIAN,
  TERMINAL_ENCODING_MAC_CYRILLIC,
  TERMINAL_ENCODING_MAC_DEVANAGARI,
  TERMINAL_ENCODING_MAC_FARSI,
  TERMINAL_ENCODING_MAC_GREEK,
  TERMINAL_ENCODING_MAC_GUJARATI,
  TERMINAL_ENCODING_MAC_GURMUKHI,
  TERMINAL_ENCODING_MAC_HEBREW,
  TERMINAL_ENCODING_MAC_ICELANDIC,
  TERMINAL_ENCODING_MAC_ROMAN,
  TERMINAL_ENCODING_MAC_ROMANIAN,
  TERMINAL_ENCODING_MAC_TURKISH,
  TERMINAL_ENCODING_MAC_UKRAINIAN,
  
  TERMINAL_ENCODING_SHIFT_JIS,
  TERMINAL_ENCODING_TCVN,
  TERMINAL_ENCODING_TIS_620,
  TERMINAL_ENCODING_UHC,
  TERMINAL_ENCODING_VISCII,

  TERMINAL_ENCODING_WINDOWS_1250,
  TERMINAL_ENCODING_WINDOWS_1251,
  TERMINAL_ENCODING_WINDOWS_1252,
  TERMINAL_ENCODING_WINDOWS_1253,
  TERMINAL_ENCODING_WINDOWS_1254,
  TERMINAL_ENCODING_WINDOWS_1255,
  TERMINAL_ENCODING_WINDOWS_1256,
  TERMINAL_ENCODING_WINDOWS_1257,
  TERMINAL_ENCODING_WINDOWS_1258,

  TERMINAL_ENCODING_LAST
  
};

static TerminalEncoding encodings[] = {

  { TERMINAL_ENCODING_CURRENT_LOCALE, TRUE,
    NULL, N_("Current Locale") , 1 },

  { TERMINAL_ENCODING_ISO_8859_1, FALSE,
    "ISO-8859-1", N_("Western") , 1 },
  { TERMINAL_ENCODING_ISO_8859_2, FALSE,
    "ISO-8859-2", N_("Central European") , 1 },
  { TERMINAL_ENCODING_ISO_8859_3, FALSE,
    "ISO-8859-3", N_("South European") , 1 },
  { TERMINAL_ENCODING_ISO_8859_4, FALSE,
    "ISO-8859-4", N_("Baltic") , 1 },
  { TERMINAL_ENCODING_ISO_8859_5, FALSE,
    "ISO-8859-5", N_("Cyrillic") , 1 },
  { TERMINAL_ENCODING_ISO_8859_6, FALSE,
    "ISO-8859-6", N_("Arabic") , 1 },
  { TERMINAL_ENCODING_ISO_8859_7, FALSE,
    "ISO-8859-7", N_("Greek") , 1 },
  { TERMINAL_ENCODING_ISO_8859_8, FALSE,
    "ISO-8859-8", N_("Hebrew Visual") , 1 },
  { TERMINAL_ENCODING_ISO_8859_8_I, FALSE,
    "ISO-8859-8-I", N_("Hebrew") , 1 },
  { TERMINAL_ENCODING_ISO_8859_9, FALSE,
    "ISO-8859-9", N_("Turkish") , 1 },
  { TERMINAL_ENCODING_ISO_8859_10, FALSE,
    "ISO-8859-10", N_("Nordic") , 1 },
  { TERMINAL_ENCODING_ISO_8859_13, FALSE,
    "ISO-8859-13", N_("Baltic") , 1 },
  { TERMINAL_ENCODING_ISO_8859_14, FALSE,
    "ISO-8859-14", N_("Celtic") , 1 },
  { TERMINAL_ENCODING_ISO_8859_15, FALSE,
    "ISO-8859-15", N_("Western") , 1 },
  { TERMINAL_ENCODING_ISO_8859_16, FALSE,
    "ISO-8859-16", N_("Romanian") , 1 },

  { TERMINAL_ENCODING_UTF_7, FALSE,
    "UTF-7", N_("Unicode") , 1 },
  { TERMINAL_ENCODING_UTF_8, FALSE,
    "UTF-8", N_("Unicode") , 1 },
  { TERMINAL_ENCODING_UTF_16, FALSE,
    "UTF-16", N_("Unicode") , 1 },
  { TERMINAL_ENCODING_UCS_2, FALSE,
    "UCS-2", N_("Unicode") , 1 },
  { TERMINAL_ENCODING_UCS_4, FALSE,
    "UCS-4", N_("Unicode") , 1 },

  { TERMINAL_ENCODING_ARMSCII_8, FALSE,
    "ARMSCII-8", N_("Armenian") , 1 },
  { TERMINAL_ENCODING_BIG5, FALSE,
    "BIG5", N_("Chinese Traditional") , 1 },
  { TERMINAL_ENCODING_BIG5_HKSCS, FALSE,
    "BIG5-HKSCS", N_("Chinese Traditional") , 1 },
  { TERMINAL_ENCODING_CP_866, FALSE,
    "CP866", N_("Cyrillic/Russian") , 1 },

  { TERMINAL_ENCODING_EUC_JP, FALSE,
    "EUC-JP", N_("Japanese") , 1 },
  { TERMINAL_ENCODING_EUC_KR, FALSE,
    "EUC-KR", N_("Korean") , 1 },
  { TERMINAL_ENCODING_EUC_TW, FALSE,
    "EUC-TW", N_("Chinese Traditional") , 1 },

  { TERMINAL_ENCODING_GB18030, FALSE,
    "GB18030", N_("Chinese Simplified") , 1 },
  { TERMINAL_ENCODING_GB2312, FALSE,
    "GB2312", N_("Chinese Simplified") , 1 },
  { TERMINAL_ENCODING_GBK, FALSE,
    "GBK", N_("Chinese Simplified") , 1 },
  { TERMINAL_ENCODING_GEOSTD8, FALSE,
    "GEORGIAN-PS", N_("Georgian") , 1 },
  { TERMINAL_ENCODING_HZ, FALSE,
    "HZ", N_("Chinese Simplified") , 1 },

  { TERMINAL_ENCODING_IBM_850, FALSE,
    "IBM850", N_("Western") , 1 },
  { TERMINAL_ENCODING_IBM_852, FALSE,
    "IBM852", N_("Central European") , 1 },
  { TERMINAL_ENCODING_IBM_855, FALSE,
    "IBM855", N_("Cyrillic") , 1 },
  { TERMINAL_ENCODING_IBM_857, FALSE,
    "IBM857", N_("Turkish") , 1 },
  { TERMINAL_ENCODING_IBM_862, FALSE,
    "IBM862", N_("Hebrew") , 1 },
  { TERMINAL_ENCODING_IBM_864, FALSE,
    "IBM864", N_("Arabic") , 1 },

  { TERMINAL_ENCODING_ISO_2022_JP, FALSE,
    "ISO-2022-JP", N_("Japanese") , 1 },
  { TERMINAL_ENCODING_ISO_2022_KR, FALSE,
    "ISO-2022-KR", N_("Korean") , 1 },
  { TERMINAL_ENCODING_ISO_IR_111, FALSE,
    "ISO-IR-111", N_("Cyrillic") , 1 },
  { TERMINAL_ENCODING_JOHAB, FALSE,
    "JOHAB", N_("Korean") , 1 },
  { TERMINAL_ENCODING_KOI8_R, FALSE,
    "KOI8-R", N_("Cyrillic") , 1 },
  { TERMINAL_ENCODING_KOI8_U, FALSE,
    "KOI8-U", N_("Cyrillic/Ukrainian") , 1 },

  { TERMINAL_ENCODING_MAC_ARABIC, FALSE,
    "MAC_ARABIC", N_("Arabic") , 1 },
  { TERMINAL_ENCODING_MAC_CE, FALSE,
    "MAC_CE", N_("Central European") , 1 },
  { TERMINAL_ENCODING_MAC_CROATIAN, FALSE,
    "MAC_CROATIAN", N_("Croatian") , 1 },
  { TERMINAL_ENCODING_MAC_CYRILLIC, FALSE,
    "MAC-CYRILLIC", N_("Cyrillic") , 1 },
  { TERMINAL_ENCODING_MAC_DEVANAGARI, FALSE,
    "MAC_DEVANAGARI", N_("Hindi") , 1 },
  { TERMINAL_ENCODING_MAC_FARSI, FALSE,
    "MAC_FARSI", N_("Persian") , 1 },
  { TERMINAL_ENCODING_MAC_GREEK, FALSE,
    "MAC_GREEK", N_("Greek") , 1 },
  { TERMINAL_ENCODING_MAC_GUJARATI, FALSE,
    "MAC_GUJARATI", N_("Gujarati") , 1 },
  { TERMINAL_ENCODING_MAC_GURMUKHI, FALSE,
    "MAC_GURMUKHI", N_("Gurmukhi") , 1 },
  { TERMINAL_ENCODING_MAC_HEBREW, FALSE,
    "MAC_HEBREW", N_("Hebrew") , 1 },
  { TERMINAL_ENCODING_MAC_ICELANDIC, FALSE,
    "MAC_ICELANDIC", N_("Icelandic") , 1 },
  { TERMINAL_ENCODING_MAC_ROMAN, FALSE,
    "MAC_ROMAN", N_("Western") , 1 },
  { TERMINAL_ENCODING_MAC_ROMANIAN, FALSE,
    "MAC_ROMANIAN", N_("Romanian") , 1 },
  { TERMINAL_ENCODING_MAC_TURKISH, FALSE,
    "MAC_TURKISH", N_("Turkish") , 1 },
  { TERMINAL_ENCODING_MAC_UKRAINIAN, FALSE,
    "MAC_UKRAINIAN", N_("Cyrillic/Ukrainian") , 1 },
  
  { TERMINAL_ENCODING_SHIFT_JIS, FALSE,
    "SHIFT_JIS", N_("Japanese") , 1 },
  { TERMINAL_ENCODING_TCVN, FALSE,
    "TCVN", N_("Vietnamese") , 1 },
  { TERMINAL_ENCODING_TIS_620, FALSE,
    "TIS-620", N_("Thai") , 1 },
  { TERMINAL_ENCODING_UHC, FALSE,
    "UHC", N_("Korean") , 1 },
  { TERMINAL_ENCODING_VISCII, FALSE,
    "VISCII", N_("Vietnamese") , 1 },

  { TERMINAL_ENCODING_WINDOWS_1250, FALSE,
    "WINDOWS-1250", N_("Central European") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1251, FALSE,
    "WINDOWS-1251", N_("Cyrillic") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1252, FALSE,
    "WINDOWS-1252", N_("Western") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1253, FALSE,
    "WINDOWS-1253", N_("Greek") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1254, FALSE,
    "WINDOWS-1254", N_("Turkish") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1255, FALSE,
    "WINDOWS-1255", N_("Hebrew") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1256, FALSE,
    "WINDOWS-1256", N_("Arabic") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1257, FALSE,
    "WINDOWS-1257", N_("Baltic") , 1 },
  { TERMINAL_ENCODING_WINDOWS_1258, FALSE,
    "WINDOWS-1258", N_("Vietnamese") , 1 }
};

static GSList *active_encodings = NULL;

static void update_active_encoding_tree_models (void);
static void register_active_encoding_treeview  (GtkTreeView *tree_view);

static void encodings_notify_cb (GConfClient *client,
                                 guint        cnxn_id,
                                 GConfEntry  *entry,
                                 gpointer     user_data);

static TerminalEncoding*
find_encoding_by_charset (const char *charset)
{
  int i;

  i = 1; /* skip current locale */
  while (i < TERMINAL_ENCODING_LAST)
    {
      /* Note that the "current locale" encoding entry
       * may have the same charset as other entries
       */
      
      if (strcmp (charset, encodings[i].charset) == 0)
        return &encodings[i];
      
      ++i;
    }

  /* Fall back to current locale if the current locale charset
   * wasn't known.
   */
  if (strcmp (charset, encodings[TERMINAL_ENCODING_CURRENT_LOCALE].charset) == 0)
    return &encodings[TERMINAL_ENCODING_CURRENT_LOCALE];
  
  return NULL;
}


static TerminalEncoding *
terminal_encoding_new (int index_,
                       const char *charset,
                       const char *name,
                       gboolean valid)
{
  TerminalEncoding *encoding;


  encoding = g_slice_new (TerminalEncoding);
  encoding->refcount = 1;
  encoding->index_ = index_;
  encoding->name = g_strdup (name);
  encoding->charset = g_strdup (charset);
  encoding->valid = valid;
  encoding->validity_checked = TRUE;

  return encoding;
}

TerminalEncoding*
terminal_encoding_ref (TerminalEncoding *encoding)
{
  g_return_val_if_fail (encoding != NULL, NULL);

  encoding->refcount++;
  return encoding;
}

void
terminal_encoding_unref (TerminalEncoding *encoding)
{
  if (--encoding->refcount > 0)
    return;

  g_free (encoding->name);
  g_free (encoding->charset);
  g_slice_free (TerminalEncoding, encoding);
}

static gboolean
terminal_encoding_is_valid (TerminalEncoding *encoding)
{
  /* All of the printing ASCII characters from space (32) to the tilde (126) */
  static const char ascii_sample[] =
      " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
  char *converted;
  gsize bytes_read = 0, bytes_written = 0;
  GError *error = NULL;

  if (encoding->validity_checked)
    return encoding->valid;

  /* Test that the encoding is a proper superset of ASCII (which naive
   * apps are going to use anyway) by attempting to validate the text
   * using the current encoding.  This also flushes out any encodings
   * which the underlying GIConv implementation can't support.
   */
  converted = g_convert (ascii_sample, sizeof (ascii_sample) - 1,
                         encoding->charset, "ASCII",
                         &bytes_read, &bytes_written, &error);

  /* The encoding is only valid if ASCII passes through cleanly. */
  encoding->valid = (bytes_read == (sizeof (ascii_sample) - 1)) &&
                    (converted != NULL) &&
                    (strcmp (converted, ascii_sample) == 0);

#ifdef DEBUG_ENCODINGS
  if (!encoding->valid)
    {
      g_print("Rejecting encoding %s as invalid: %s\n", encoding->charset, error ? error->message : "");
      g_print(" input  \"%s\"\n", ascii_sample);
      g_print(" output \"%s\"\n\n", converted ? converted : "(null)");
      g_clear_error (&error);
    }
  else
    g_print ("Encoding %s is valid\n\n", encoding->charset);
#endif

  g_free (converted);

  encoding->validity_checked = TRUE;
  return encoding->valid;
}

static void
encodings_notify_cb (GConfClient *client,
                     guint        cnxn_id,
                     GConfEntry  *entry,
                     gpointer     user_data)
{
  GConfValue *val;
  GSList *strings, *tmp;
  GHashTable *table;
  const char *charset;

#if 1
  g_slist_foreach (active_encodings, (GFunc) terminal_encoding_unref,
                   NULL);
  g_slist_free (active_encodings);
#endif
  active_encodings = NULL;

  table = g_hash_table_new (g_direct_hash, g_direct_equal);
  
  /* First add the local encoding. */
  charset = encodings[TERMINAL_ENCODING_CURRENT_LOCALE].charset;
  if (g_hash_table_lookup (table, GINT_TO_POINTER (g_quark_from_string (charset))) == NULL)
    {
      active_encodings = g_slist_prepend (active_encodings,
                                          terminal_encoding_ref (&encodings[TERMINAL_ENCODING_CURRENT_LOCALE]));
      g_hash_table_insert (table,
		           GINT_TO_POINTER (g_quark_from_string (charset)),
		           GINT_TO_POINTER (g_quark_from_string (charset)));
    }

  /* Always ensure that UTF-8 is available. */
  charset = encodings[TERMINAL_ENCODING_UTF_8].charset;
  if (g_hash_table_lookup (table, GINT_TO_POINTER (g_quark_from_string (charset))) == NULL)
    {
      active_encodings = g_slist_prepend (active_encodings,
                                          terminal_encoding_ref (&encodings[TERMINAL_ENCODING_UTF_8]));
      g_hash_table_insert (table,
		           GINT_TO_POINTER (g_quark_from_string (charset)),
		           GINT_TO_POINTER (g_quark_from_string (charset)));
    }

  val = gconf_entry_get_value (entry);
  if (val != NULL &&
      val->type == GCONF_VALUE_LIST &&
      gconf_value_get_list_type (val) == GCONF_VALUE_STRING)
    strings = gconf_value_get_list (val);
  else
    strings = NULL;

  for (tmp = strings; tmp != NULL; tmp = tmp->next)
    {
      GConfValue *v = (GConfValue *) tmp->data;
      TerminalEncoding *e;
      TerminalEncoding *encoding;
      charset = tmp->data;
      
      charset = gconf_value_get_string (v);
      if (!charset)
        continue;

      if (strcmp (charset, "current") == 0)
        g_get_charset (&charset);
      
      e = find_encoding_by_charset (charset);

      if (g_hash_table_lookup (table, GINT_TO_POINTER (g_quark_from_string (charset))) != NULL)
        {
	  continue;
        }

      g_hash_table_insert (table,
		           GINT_TO_POINTER (g_quark_from_string (charset)),
		           GINT_TO_POINTER (g_quark_from_string (charset)));
      
      if (e == NULL)
        {
          encoding = terminal_encoding_new (-1,
                                            charset,
                                            _("User Defined"),
                                            TRUE /* scary! */);
        }
      else
        {
          encoding = terminal_encoding_is_valid (e) ? terminal_encoding_ref (e) : NULL;
        }

      if (encoding != NULL)
        {
          active_encodings = g_slist_prepend (active_encodings, encoding);
        }
    }

  /* Put it back in order, order is significant */
  active_encodings = g_slist_reverse (active_encodings);
  
  g_hash_table_destroy (table);
  
  update_active_encoding_tree_models ();
}

/**
 * terminal_get_active_encodings:
 *
 * Returns: a newly allocated list of newly referenced #TerminalEncoding objects.
 */
GSList*
terminal_get_active_encodings (void)
{
  GSList *copy;

  copy = g_slist_copy (active_encodings);
  g_slist_foreach (copy, (GFunc) terminal_encoding_ref, NULL);
  return copy;
}

static void
response_callback (GtkWidget *window,
                   int        id,
                   void      *data)
{
  if (id == GTK_RESPONSE_HELP)
    terminal_util_show_help ("gnome-terminal-encoding-add", GTK_WINDOW (window));
  else
    gtk_widget_destroy (GTK_WIDGET (window));
}

enum
{
  COLUMN_NAME,
  COLUMN_CHARSET,
  N_COLUMNS
};

static void
count_selected_items_func (GtkTreeModel      *model,
                           GtkTreePath       *path,
                           GtkTreeIter       *iter,
                           gpointer           data)
{
  int *count = data;

  *count += 1;
}

static void
available_selection_changed_callback (GtkTreeSelection *selection,
                                      void             *data)
{
  int count;
  GtkWidget *add_button;
  GtkWidget *dialog;

  dialog = data;
  
  count = 0;
  gtk_tree_selection_selected_foreach (selection,
                                       count_selected_items_func,
                                       &count);

  add_button = g_object_get_data (G_OBJECT (dialog), "encoding-dialog-add");
  
  gtk_widget_set_sensitive (add_button, count > 0);
}

static void
displayed_selection_changed_callback (GtkTreeSelection *selection,
                                      void             *data)
{
  int count;
  GtkWidget *remove_button;
  GtkWidget *dialog;

  dialog = data;
  
  count = 0;
  gtk_tree_selection_selected_foreach (selection,
                                       count_selected_items_func,
                                       &count);

  remove_button = g_object_get_data (G_OBJECT (dialog), "encoding-dialog-remove");
  
  gtk_widget_set_sensitive (remove_button, count > 0);
}

static void
get_selected_encodings_func (GtkTreeModel      *model,
                             GtkTreePath       *path,
                             GtkTreeIter       *iter,
                             gpointer           data)
{
  GSList **list = data;
  char *charset;

  charset = NULL;
  gtk_tree_model_get (model,
                      iter,
                      COLUMN_CHARSET,
                      &charset,
                      -1);

  *list = g_slist_prepend (*list, charset);
}

static gboolean
charset_in_encoding_list (GSList     *list,
                          const char *str)
{
  GSList *tmp;

  tmp = list;
  while (tmp != NULL)
    {
      const TerminalEncoding *enc = tmp->data;
      
      if (strcmp (enc->charset, str) == 0)
        return TRUE;

      tmp = tmp->next;
    }

  return FALSE;
}

static GSList*
remove_string_from_list (GSList     *list,
                         const char *str)
{
  GSList *tmp;

  tmp = list;
  while (tmp != NULL)
    {
      if (strcmp (tmp->data, str) == 0)
        break;

      tmp = tmp->next;
    }

  if (tmp != NULL)
    {
      g_free (tmp->data);
      list = g_slist_remove (list, tmp->data);
    }
  
  return list;
}

static GSList*
encoding_list_to_charset_list (GSList *src)
{
  GSList *list;
  GSList *tmp;
  
  list = NULL;
  tmp = src;
  while (tmp != NULL)
    {
      const TerminalEncoding *enc = tmp->data;
      
      list = g_slist_prepend (list, g_strdup (enc->charset));
      tmp = tmp->next;
    }
  list = g_slist_reverse (list);

  return list;
}


static void
add_button_clicked_callback (GtkWidget *button,
                             void      *data)
{
  GtkWidget *dialog;
  GtkWidget *treeview;
  GtkTreeSelection *selection;
  GSList *encodings;
  GSList *tmp;
  GSList *new_active_list;
  GConfClient *conf;
  
  dialog = data;

  treeview = g_object_get_data (G_OBJECT (dialog),
                                "encoding-dialog-available-treeview");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  encodings = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       get_selected_encodings_func,
                                       &encodings);

  new_active_list = encoding_list_to_charset_list (active_encodings);
  tmp = encodings;
  while (tmp != NULL)
    {
      /* appending is less efficient but produces user-expected
       * result
       */
      if (!charset_in_encoding_list (active_encodings, tmp->data))
        new_active_list = g_slist_append (new_active_list,
                                          g_strdup (tmp->data));
      
      tmp = tmp->next;
    }

  /* this is reentrant, but only after it's done using the list
   * values, so should be safe
   */
  conf = gconf_client_get_default ();
  gconf_client_set_list (conf,
                         CONF_GLOBAL_PREFIX"/active_encodings",
                         GCONF_VALUE_STRING,
                         new_active_list,
                         NULL);
  g_object_unref (conf);

  g_slist_foreach (new_active_list, (GFunc) g_free, NULL);
  g_slist_free (new_active_list);
  
  g_slist_foreach (encodings, (GFunc) g_free, NULL);
  g_slist_free (encodings);
}

static void
remove_button_clicked_callback (GtkWidget *button,
                                void      *data)
{
  GtkWidget *dialog;
  GtkWidget *treeview;
  GtkTreeSelection *selection;
  GSList *encodings;
  GSList *tmp;
  GSList *new_active_list;
  GConfClient *conf;
  
  dialog = data;

  treeview = g_object_get_data (G_OBJECT (dialog),
                                "encoding-dialog-displayed-treeview");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  encodings = NULL;
  gtk_tree_selection_selected_foreach (selection,
                                       get_selected_encodings_func,
                                       &encodings);

  new_active_list = encoding_list_to_charset_list (active_encodings);
  tmp = encodings;
  while (tmp != NULL)
    {
      /* appending is less efficient but produces user-expected
       * result
       */
      new_active_list =
        remove_string_from_list (new_active_list, tmp->data);
      
      tmp = tmp->next;
    }

  /* this is reentrant, but only after it's done using the list
   * values, so should be safe
   */
  conf = gconf_client_get_default ();
  gconf_client_set_list (conf,
                         CONF_GLOBAL_PREFIX"/active_encodings",
                         GCONF_VALUE_STRING,
                         new_active_list,
                         NULL);
  g_object_unref (conf);

  g_slist_foreach (new_active_list, (GFunc) g_free, NULL);
  g_slist_free (new_active_list);  
  
  g_slist_foreach (encodings, (GFunc) g_free, NULL);
  g_slist_free (encodings);
}

GtkWidget*
terminal_encoding_dialog_new (GtkWindow *transient_parent)
{
  GladeXML *xml;
  GtkWidget *w;
  GtkCellRenderer *cell_renderer;
  int i;
  GtkListStore *store;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkWidget *dialog;

  xml = terminal_util_load_glade_file (TERM_GLADE_FILE,
                                       "encodings-dialog",
                                       transient_parent);
  if (xml == NULL)
    return NULL;

  /* The dialog itself */
  dialog = glade_xml_get_widget (xml, "encodings-dialog");

  terminal_util_set_unique_role (GTK_WINDOW (dialog), "gnome-terminal-encodings");

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (response_callback),
                    NULL);

  /* buttons */
  w = glade_xml_get_widget (xml, "add-button");
  g_object_set_data (G_OBJECT (dialog),
                     "encoding-dialog-add",
                     w);

  g_signal_connect (G_OBJECT (w), "clicked",
                    G_CALLBACK (add_button_clicked_callback),
                    dialog);

  w = glade_xml_get_widget (xml, "remove-button");
  g_object_set_data (G_OBJECT (dialog),
                     "encoding-dialog-remove",
                     w);

  g_signal_connect (G_OBJECT (w), "clicked",
                    G_CALLBACK (remove_button_clicked_callback),
                    dialog);
  
  /* Tree view of available encodings */
  
  w = glade_xml_get_widget (xml, "available-treeview");
  g_object_set_data (G_OBJECT (dialog),
                     "encoding-dialog-available-treeview",
                     w);
  
  /* Column 1 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Description"),
						     cell_renderer,
						     "text", COLUMN_NAME,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  
  /* Column 2 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Encoding"),
						     cell_renderer,
						     "text", COLUMN_CHARSET,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);  

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_MULTIPLE);

  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < (int) G_N_ELEMENTS (encodings); ++i)
    {
      if (!terminal_encoding_is_valid (&encodings[i]))
        continue;

      gtk_list_store_insert_with_values (store, &iter, -1,
                                         COLUMN_CHARSET,
                                         encodings[i].charset,
                                         COLUMN_NAME,
                                         encodings[i].name,
                                         -1);
    }

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  
  gtk_tree_view_set_model (GTK_TREE_VIEW (w), GTK_TREE_MODEL (store));
  g_object_unref (store);

  available_selection_changed_callback (selection, dialog);
  g_signal_connect (G_OBJECT (selection), "changed",                    
                    G_CALLBACK (available_selection_changed_callback),
                    dialog);

  /* Tree view of selected encodings */
  
  w = glade_xml_get_widget (xml, "displayed-treeview");
  g_object_set_data (G_OBJECT (dialog),
                     "encoding-dialog-displayed-treeview",
                     w);

  /* Column 1 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Description"),
						     cell_renderer,
						     "text", COLUMN_NAME,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  
  /* Column 2 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Encoding"),
						     cell_renderer,
						     "text", COLUMN_CHARSET,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);  

  /* Add the data */
  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (w), GTK_TREE_MODEL (store));
  g_object_unref (store);

  register_active_encoding_treeview (GTK_TREE_VIEW (w));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));    
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_MULTIPLE);

  displayed_selection_changed_callback (selection, dialog);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (displayed_selection_changed_callback),
                    dialog);

  g_object_unref (G_OBJECT (xml));
  
  return dialog;
}

static void
update_single_treeview (GtkTreeView *tree_view)
{
  GtkListStore *store;
  GSList *tmp;
  GtkTreeIter iter;

  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

  for (tmp = active_encodings; tmp != NULL; tmp = tmp->next)
    {
      TerminalEncoding *e = tmp->data;
      
      gtk_list_store_insert_with_values (store, &iter, -1,
                                         COLUMN_CHARSET,
                                         e->charset,
                                         COLUMN_NAME,
                                         e->name,
                                         -1);
    }

  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
  g_object_unref (store);
}

static GSList *treeviews = NULL;

static void
unregister_treeview (void    *data,
                     GObject *where_object_was)
{
  treeviews = g_slist_remove (treeviews, where_object_was);
}

static void
update_active_encoding_tree_models (void)
{
  GSList *tmp;
  tmp = treeviews;
  while (tmp != NULL)
    {
      update_single_treeview (tmp->data);
      tmp = tmp->next;
    }
}

static void
register_active_encoding_treeview (GtkTreeView *tree_view)
{
  update_single_treeview (tree_view);
  treeviews = g_slist_prepend (treeviews, tree_view);
  g_object_weak_ref (G_OBJECT (tree_view), unregister_treeview, NULL);
}

void
terminal_encoding_init (void)
{
  GConfClient *conf;
  guint i;

  conf = gconf_client_get_default ();

  g_get_charset ((const char**)
                 &encodings[TERMINAL_ENCODING_CURRENT_LOCALE].charset);

  g_assert (G_N_ELEMENTS (encodings) == TERMINAL_ENCODING_LAST);

  for (i = 0; i < TERMINAL_ENCODING_LAST; ++i)
    {
      g_assert (encodings[i].index_ == i);

      /* Translate the names */
      encodings[i].name = _(encodings[i].name);

      if (i == TERMINAL_ENCODING_CURRENT_LOCALE)
        encodings[i].valid = encodings[i].validity_checked = TRUE;
    }

  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/active_encodings",
                           encodings_notify_cb,
                           NULL /* user_data */, NULL,
                           NULL);

  gconf_client_notify (conf, CONF_GLOBAL_PREFIX"/active_encodings");

  g_object_unref (conf);
}
