/* Encoding stuff */

/*
 * Copyright (C) 2002 Red Hat, Inc.
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

#include "terminal-intl.h"

#include "encoding.h"

#include "terminal-profile.h"

#include <glade/glade.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktreestore.h>

#include <libgnome/gnome-help.h>

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
static GConfClient *default_client = NULL;

static TerminalEncoding encodings[] = {

  { TERMINAL_ENCODING_CURRENT_LOCALE,
    NULL, N_("Current Locale") },

  { TERMINAL_ENCODING_ISO_8859_1,
    "ISO-8859-1", N_("Western") },
  { TERMINAL_ENCODING_ISO_8859_3,
    "ISO-8859-3", N_("South European") },
  { TERMINAL_ENCODING_ISO_8859_4,
    "ISO-8859-4", N_("Baltic") },
  { TERMINAL_ENCODING_ISO_8859_5,
    "ISO-8859-5", N_("Cyrillic") },
  { TERMINAL_ENCODING_ISO_8859_6,
    "ISO-8859-6", N_("Arabic") },
  { TERMINAL_ENCODING_ISO_8859_7,
    "ISO-8859-7", N_("Greek") },
  { TERMINAL_ENCODING_ISO_8859_8,
    "ISO-8859-8", N_("Hebrew Visual") },
  { TERMINAL_ENCODING_ISO_8859_8_I,
    "ISO-8859-8-I", N_("Hebrew") },
  { TERMINAL_ENCODING_ISO_8859_9,
    "ISO-8859-9", N_("Turkish") },
  { TERMINAL_ENCODING_ISO_8859_10,
    "ISO-8859-10", N_("Nordic") },
  { TERMINAL_ENCODING_ISO_8859_13,
    "ISO-8859-13", N_("Baltic") },
  { TERMINAL_ENCODING_ISO_8859_14,
    "ISO-8859-14", N_("Celtic") },
  { TERMINAL_ENCODING_ISO_8859_15,
    "ISO-8859-15", N_("Western") },
  { TERMINAL_ENCODING_ISO_8859_16,
    "ISO-8859-16", N_("Romanian") },

  { TERMINAL_ENCODING_UTF_7,
    "UTF-7", N_("Unicode") },
  { TERMINAL_ENCODING_UTF_8,
    "UTF-8", N_("Unicode") },
  { TERMINAL_ENCODING_UTF_16,
    "UTF-16", N_("Unicode") },
  { TERMINAL_ENCODING_UCS_2,
    "UCS-2", N_("Unicode") },
  { TERMINAL_ENCODING_UCS_4,
    "UCS-4", N_("Unicode") },

  { TERMINAL_ENCODING_ARMSCII_8,
    "ARMSCII-8", N_("Armenian") },
  { TERMINAL_ENCODING_BIG5,
    "BIG5", N_("Chinese Traditional") },
  { TERMINAL_ENCODING_BIG5_HKSCS,
    "BIG5-HKSCS", N_("Chinese Traditional") },
  { TERMINAL_ENCODING_CP_866,
    "CP866", N_("Cyrillic/Russian") },

  { TERMINAL_ENCODING_EUC_JP,
    "EUC-JP", N_("Japanese") },
  { TERMINAL_ENCODING_EUC_KR,
    "EUC-KR", N_("Korean") },
  { TERMINAL_ENCODING_EUC_TW,
    "EUC-TW", N_("Chinese Traditional") },

  { TERMINAL_ENCODING_GB18030,
    "GB18030", N_("Chinese Simplified") },
  { TERMINAL_ENCODING_GB2312,
    "GB2312", N_("Chinese Simplified") },
  { TERMINAL_ENCODING_GBK,
    "GBK", N_("Chinese Simplified") },
  { TERMINAL_ENCODING_GEOSTD8,
    "GEORGIAN-ACADEMY", N_("Georgian") }, /* FIXME GEOSTD8 ? */
  { TERMINAL_ENCODING_HZ,
    "HZ", N_("Chinese Simplified") },

  { TERMINAL_ENCODING_IBM_850,
    "IBM850", N_("Western") },
  { TERMINAL_ENCODING_IBM_852,
    "IBM852", N_("Central European") },
  { TERMINAL_ENCODING_IBM_855,
    "IBM855", N_("Cyrillic") },
  { TERMINAL_ENCODING_IBM_857,
    "IBM857", N_("Turkish") },
  { TERMINAL_ENCODING_IBM_862,
    "IBM862", N_("Hebrew") },
  { TERMINAL_ENCODING_IBM_864,
    "IBM864", N_("Arabic") },

  { TERMINAL_ENCODING_ISO_2022_JP,
    "ISO2022JP", N_("Japanese") },
  { TERMINAL_ENCODING_ISO_2022_KR,
    "ISO2022KR", N_("Korean") },
  { TERMINAL_ENCODING_ISO_IR_111,
    "ISO-IR-111", N_("Cyrillic") },
  { TERMINAL_ENCODING_JOHAB,
    "JOHAB", N_("Korean") },
  { TERMINAL_ENCODING_KOI8_R,
    "KOI8R", N_("Cyrillic") },
  { TERMINAL_ENCODING_KOI8_U,
    "KOI8U", N_("Cyrillic/Ukrainian") },

#if 0
  /* GLIBC iconv doesn't seem to have these mac things */
  { TERMINAL_ENCODING_MAC_ARABIC,
    "MAC_ARABIC", N_("Arabic") },
  { TERMINAL_ENCODING_MAC_CE,
    "MAC_CE", N_("Central European") },
  { TERMINAL_ENCODING_MAC_CROATIAN,
    "MAC_CROATIAN", N_("Croatian") },
  { TERMINAL_ENCODING_MAC_CYRILLIC,
    "MAC-CYRILLIC", N_("Cyrillic") },
  { TERMINAL_ENCODING_MAC_DEVANAGARI,
    "MAC_DEVANAGARI", N_("Hindi") },
  { TERMINAL_ENCODING_MAC_FARSI,
    "MAC_FARSI", N_("Farsi") },
  { TERMINAL_ENCODING_MAC_GREEK,
    "MAC_GREEK", N_("Greek") },
  { TERMINAL_ENCODING_MAC_GUJARATI,
    "MAC_GUJARATI", N_("Gujarati") },
  { TERMINAL_ENCODING_MAC_GURMUKHI,
    "MAC_GURMUKHI", N_("Gurmukhi") },
  { TERMINAL_ENCODING_MAC_HEBREW,
    "MAC_HEBREW", N_("Hebrew") },
  { TERMINAL_ENCODING_MAC_ICELANDIC,
    "MAC_ICELANDIC", N_("Icelandic") },
  { TERMINAL_ENCODING_MAC_ROMAN,
    "MAC_ROMAN", N_("Western") },
  { TERMINAL_ENCODING_MAC_ROMANIAN,
    "MAC_ROMANIAN", N_("Romanian") },
  { TERMINAL_ENCODING_MAC_TURKISH,
    "MAC_TURKISH", N_("Turkish") },
  { TERMINAL_ENCODING_MAC_UKRAINIAN,
    "MAC_UKRAINIAN", N_("Cyrillic/Ukrainian") },
#endif
  
  { TERMINAL_ENCODING_SHIFT_JIS,
    "SHIFT-JIS", N_("Japanese") },
  { TERMINAL_ENCODING_TCVN,
    "TCVN", N_("Vietnamese") },
  { TERMINAL_ENCODING_TIS_620,
    "TIS-620", N_("Thai") },
  { TERMINAL_ENCODING_UHC,
    "UHC", N_("Korean") },
  { TERMINAL_ENCODING_VISCII,
    "VISCII", N_("Vietnamese") },

  { TERMINAL_ENCODING_WINDOWS_1250,
    "WINDOWS-1250", N_("Central European") },
  { TERMINAL_ENCODING_WINDOWS_1251,
    "WINDOWS-1251", N_("Cyrillic") },
  { TERMINAL_ENCODING_WINDOWS_1252,
    "WINDOWS-1252", N_("Western") },
  { TERMINAL_ENCODING_WINDOWS_1253,
    "WINDOWS-1253", N_("Greek") },
  { TERMINAL_ENCODING_WINDOWS_1254,
    "WINDOWS-1254", N_("Turkish") },
  { TERMINAL_ENCODING_WINDOWS_1255,
    "WINDOWS-1255", N_("Hebrew") },
  { TERMINAL_ENCODING_WINDOWS_1256,
    "WINDOWS-1256", N_("Arabic") },
  { TERMINAL_ENCODING_WINDOWS_1257,
    "WINDOWS-1257", N_("Baltic") },
  { TERMINAL_ENCODING_WINDOWS_1258,
    "WINDOWS-1258", N_("Vietnamese") }
};

static GSList *active_encodings = NULL;

static void update_active_encodings_from_string_list (GSList *strings);
static void update_active_encoding_tree_models  (void);
static void register_active_encoding_tree_model (GtkListStore *store);

static void
encodings_change_notify (GConfClient *client,
                         guint        cnxn_id,
                         GConfEntry  *entry,
                         gpointer     user_data)
{
  GConfValue *val;
  GSList *strings;
  
  /* FIXME handle whether the entry is writable
   */

  val = gconf_entry_get_value (entry);
  if (val == NULL || val->type != GCONF_VALUE_LIST ||
      gconf_value_get_list_type (val) != GCONF_VALUE_STRING)
    strings = NULL;
  else
    {
      GSList *tmp;

      strings = NULL;
      tmp = gconf_value_get_list (val);
      while (tmp != NULL)
        {
          GConfValue *v = tmp->data;
          g_assert (v->type == GCONF_VALUE_STRING);

          if (gconf_value_get_string (v))
            {
              strings = g_slist_prepend (strings,
                                         (char*) gconf_value_get_string (v));
            }
          
          tmp = tmp->next;
        }
    }

  update_active_encodings_from_string_list (strings);

  /* note we didn't copy the strings themselves, so don't free them */
  g_slist_free (strings);
}

static const TerminalEncoding*
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

void
terminal_encoding_free (TerminalEncoding *encoding)
{
  g_free (encoding->name);
  g_free (encoding->charset);
  g_free (encoding);
}

static TerminalEncoding*
terminal_encoding_copy (const TerminalEncoding *src)
{
  TerminalEncoding *c;

  c = g_new (TerminalEncoding, 1);
  c->index = src->index;
  c->name = g_strdup (src->name);
  c->charset = g_strdup (src->charset);
  
  return c;
}

static void
update_active_encodings_from_string_list (GSList *strings)
{
  GSList *tmp;

#if 1
  g_slist_foreach (active_encodings, (GFunc) terminal_encoding_free,
                   NULL);
  g_slist_free (active_encodings);
#endif
  active_encodings = NULL;
  
  tmp = strings;
  while (tmp != NULL)
    {
      const TerminalEncoding *e;
      const char *charset = tmp->data;
      TerminalEncoding *encoding;
      
      if (strcmp (charset, "current") == 0)
        g_get_charset (&charset);
      
      e = find_encoding_by_charset (charset);
      
      if (e == NULL)
        {
          encoding = g_new0 (TerminalEncoding, 1);
          
          encoding->index = -1;
          encoding->charset = g_strdup (charset);
          encoding->name = g_strdup (_("User Defined"));
        }
      else
        {
          encoding = terminal_encoding_copy (e);
        }

      active_encodings = g_slist_prepend (active_encodings, encoding);
      
      tmp = tmp->next;
    }

  /* Put it back in order, order is significant */
  active_encodings = g_slist_reverse (active_encodings);
  
  if (active_encodings == NULL)
    {
      /* Emergency fallbacks */
      active_encodings = g_slist_prepend (active_encodings,
                                          terminal_encoding_copy (&encodings[TERMINAL_ENCODING_CURRENT_LOCALE]));
      if (strcmp (encodings[TERMINAL_ENCODING_CURRENT_LOCALE].charset,
                  "UTF-8") != 0)
        active_encodings = g_slist_prepend (active_encodings,
                                            terminal_encoding_copy (&encodings[TERMINAL_ENCODING_UTF_8]));
    }
  
  update_active_encoding_tree_models ();
}

GSList*
terminal_get_active_encodings (void)
{
  GSList *copy;
  GSList *tmp;

  copy = NULL;
  tmp = active_encodings;
  while (tmp != NULL)
    {
      copy = g_slist_prepend (copy,
                              terminal_encoding_copy (tmp->data));
      
      tmp = tmp->next;
    }

  /* They should appear in order in the menus */
  copy = g_slist_reverse (copy);

  return copy;
}

static void
response_callback (GtkWidget *window,
                   int        id,
                   void      *data)
{
  if (id == GTK_RESPONSE_HELP)
    {
      GError *err;
      err = NULL;
      gnome_help_display ("gnome-terminal", "gnome-terminal-encodings",
                          &err);
      
      if (err)
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("There was an error displaying help: %s"),
                                           err->message);
          
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);
          
          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show (dialog);
          
          g_error_free (err);
        }
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (window));
    }
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
  gconf_client_set_list (default_client,
                         CONF_GLOBAL_PREFIX"/active_encodings",
                         GCONF_VALUE_STRING,
                         new_active_list,
                         NULL);

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
  gconf_client_set_list (default_client,
                         CONF_GLOBAL_PREFIX"/active_encodings",
                         GCONF_VALUE_STRING,
                         new_active_list,
                         NULL);

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
  GtkTreeModel *sort_model;
  GtkListStore *tree;
  GtkTreeViewColumn *column;
  GtkTreeIter parent_iter;
  GtkTreeSelection *selection;
  GtkWidget *dialog;
  
  if (g_file_test ("./"TERM_GLADE_FILE,
                   G_FILE_TEST_EXISTS))
    {
      /* Try current dir, for debugging */
      xml = glade_xml_new ("./"TERM_GLADE_FILE,
                           "encodings-dialog",
                           GETTEXT_PACKAGE);
    }
  else
    {
      xml = glade_xml_new (TERM_GLADE_DIR"/"TERM_GLADE_FILE,
                           "encodings-dialog",
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
                                    _("The file \"%s\" is missing. This indicates that the application is installed incorrectly, so the encodings dialog can't be displayed."),
                                    TERM_GLADE_DIR"/"TERM_GLADE_FILE);
                                        
          g_signal_connect (G_OBJECT (no_glade_dialog),
                            "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);
          
          g_object_add_weak_pointer (G_OBJECT (no_glade_dialog),
                                     (void**)&no_glade_dialog);
        }

      gtk_window_present (GTK_WINDOW (no_glade_dialog));

      return NULL;
    }  

  /* The dialog itself */
  dialog = glade_xml_get_widget (xml, "encodings-dialog");

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
  
  tree = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

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

  i = 0;
  while (i < (int) G_N_ELEMENTS (encodings))
    {
      gtk_list_store_append (tree, &parent_iter);
      gtk_list_store_set (tree, &parent_iter,
                          COLUMN_CHARSET,
                          encodings[i].charset,
                          COLUMN_NAME,
                          encodings[i].name,
                          -1);

      ++i;
    }

  /* Sort model */
  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree));
  
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  
  gtk_tree_view_set_model (GTK_TREE_VIEW (w), sort_model);
  g_object_unref (G_OBJECT (tree));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));    
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_MULTIPLE);

  available_selection_changed_callback (selection, dialog);
  g_signal_connect (G_OBJECT (selection), "changed",                    
                    G_CALLBACK (available_selection_changed_callback),
                    dialog);

  /* Tree view of selected encodings */
  
  w = glade_xml_get_widget (xml, "displayed-treeview");
  g_object_set_data (G_OBJECT (dialog),
                     "encoding-dialog-displayed-treeview",
                     w);
  
  tree = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

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
  register_active_encoding_tree_model (tree);

  /* Sort model */
  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree));
  
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  
  gtk_tree_view_set_model (GTK_TREE_VIEW (w), sort_model);
  g_object_unref (G_OBJECT (tree));  

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));    
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_MULTIPLE);

  displayed_selection_changed_callback (selection, dialog);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (displayed_selection_changed_callback),
                    dialog);
  
  return dialog;
}

static void
update_single_tree_model (GtkListStore *store)
{
  GSList *tmp;
  GtkTreeIter parent_iter;

  gtk_list_store_clear (store);
  
  tmp = active_encodings;
  while (tmp != NULL)
    {
      TerminalEncoding *e = tmp->data;
      
      gtk_list_store_append (store, &parent_iter);
      gtk_list_store_set (store, &parent_iter,
                          COLUMN_CHARSET,
                          e->charset,
                          COLUMN_NAME,
                          e->name,
                          -1);

      tmp = tmp->next;
    }
}

static GSList *stores = NULL;

static void
unregister_store (void    *data,
                  GObject *where_object_was)
{
  stores = g_slist_remove (stores, where_object_was);
}

static void
update_active_encoding_tree_models (void)
{
  GSList *tmp;
  tmp = stores;
  while (tmp != NULL)
    {
      update_single_tree_model (tmp->data);
      tmp = tmp->next;
    }
}

static void
register_active_encoding_tree_model (GtkListStore *store)
{
  update_single_tree_model (store);
  stores = g_slist_prepend (stores, store);
  g_object_weak_ref (G_OBJECT (store), unregister_store, NULL);
}

void
terminal_encoding_init (GConfClient *conf)
{
  int i;
  GError *err;
  GSList *strings;
  
  g_return_if_fail (GCONF_IS_CLIENT (conf));

  default_client = conf;
  g_object_ref (G_OBJECT (default_client));
  
  g_get_charset ((const char**)
                 &encodings[TERMINAL_ENCODING_CURRENT_LOCALE].charset);

  g_assert (G_N_ELEMENTS (encodings) == TERMINAL_ENCODING_LAST);
  
  i = 0;
  while (i < TERMINAL_ENCODING_LAST)
    {
      g_assert (encodings[i].index == i);

      /* Translate the names */
      encodings[i].name = _(encodings[i].name);
      
      ++i;
    }

  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/active_encodings",
                           encodings_change_notify,
                           NULL, /* user_data */
                           NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal encoding list changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }

  strings = gconf_client_get_list (conf,
                                   CONF_GLOBAL_PREFIX"/active_encodings",
                                   GCONF_VALUE_STRING, NULL);

  update_active_encodings_from_string_list (strings);

  g_slist_foreach (strings, (GFunc) g_free, NULL);
  g_slist_free (strings);                                   
}

char*
terminal_encoding_get_name (const char *charset)
{
  const TerminalEncoding *e;

  e = find_encoding_by_charset (charset);
  if (e != NULL)
    return g_strdup_printf ("%s (%s)", e->name, e->charset);
  else
    return g_strdup (charset);
}
