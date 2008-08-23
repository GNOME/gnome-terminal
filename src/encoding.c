/*
 * Copyright © 2002 Red Hat, Inc.
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

#include <string.h>

#include <gtk/gtk.h>

#include "encoding.h"
#include "terminal-app.h"
#include "terminal-intl.h"
#include "terminal-profile.h"
#include "terminal-util.h"

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

static const struct {
  const char *charset;
  const char *name;
} encodings[] = {
//  { "UTF-8",	N_("Current Locale") },
  { "ISO-8859-1",	N_("Western") },
  { "ISO-8859-2",	N_("Central European") },
  { "ISO-8859-3",	N_("South European") },
  { "ISO-8859-4",	N_("Baltic") },
  { "ISO-8859-5",	N_("Cyrillic") },
  { "ISO-8859-6",	N_("Arabic") },
  { "ISO-8859-7",	N_("Greek") },
  { "ISO-8859-8",	N_("Hebrew Visual") },
  { "ISO-8859-8-I",	N_("Hebrew") },
  { "ISO-8859-9",	N_("Turkish") },
  { "ISO-8859-10",	N_("Nordic") },
  { "ISO-8859-13",	N_("Baltic") },
  { "ISO-8859-14",	N_("Celtic") },
  { "ISO-8859-15",	N_("Western") },
  { "ISO-8859-16",	N_("Romanian") },
  { "UTF-8",	N_("Unicode") },
  { "ARMSCII-8",	N_("Armenian") },
  { "BIG5",	N_("Chinese Traditional") },
  { "BIG5-HKSCS",	N_("Chinese Traditional") },
  { "CP866",	N_("Cyrillic/Russian") },
  { "EUC-JP",	N_("Japanese") },
  { "EUC-KR",	N_("Korean") },
  { "EUC-TW",	N_("Chinese Traditional") },
  { "GB18030",	N_("Chinese Simplified") },
  { "GB2312",	N_("Chinese Simplified") },
  { "GBK",	N_("Chinese Simplified") },
  { "GEORGIAN-PS",	N_("Georgian") },
  { "IBM850",	N_("Western") },
  { "IBM852",	N_("Central European") },
  { "IBM855",	N_("Cyrillic") },
  { "IBM857",	N_("Turkish") },
  { "IBM862",	N_("Hebrew") },
  { "IBM864",	N_("Arabic") },
  { "ISO-2022-JP",	N_("Japanese") },
  { "ISO-2022-KR",	N_("Korean") },
  { "ISO-IR-111",	N_("Cyrillic") },
  { "KOI8-R",	N_("Cyrillic") },
  { "KOI8-U",	N_("Cyrillic/Ukrainian") },
  { "MAC_ARABIC",	N_("Arabic") },
  { "MAC_CE",	N_("Central European") },
  { "MAC_CROATIAN",	N_("Croatian") },
  { "MAC-CYRILLIC",	N_("Cyrillic") },
  { "MAC_DEVANAGARI",	N_("Hindi") },
  { "MAC_FARSI",	N_("Persian") },
  { "MAC_GREEK",	N_("Greek") },
  { "MAC_GUJARATI",	N_("Gujarati") },
  { "MAC_GURMUKHI",	N_("Gurmukhi") },
  { "MAC_HEBREW",	N_("Hebrew") },
  { "MAC_ICELANDIC",	N_("Icelandic") },
  { "MAC_ROMAN",	N_("Western") },
  { "MAC_ROMANIAN",	N_("Romanian") },
  { "MAC_TURKISH",	N_("Turkish") },
  { "MAC_UKRAINIAN",	N_("Cyrillic/Ukrainian") },
  { "SHIFT_JIS",	N_("Japanese") },
  { "TCVN",	N_("Vietnamese") },
  { "TIS-620",	N_("Thai") },
  { "UHC",	N_("Korean") },
  { "VISCII",	N_("Vietnamese") },
  { "WINDOWS-1250",	N_("Central European") },
  { "WINDOWS-1251",	N_("Cyrillic") },
  { "WINDOWS-1252",	N_("Western") },
  { "WINDOWS-1253",	N_("Greek") },
  { "WINDOWS-1254",	N_("Turkish") },
  { "WINDOWS-1255",	N_("Hebrew") },
  { "WINDOWS-1256",	N_("Arabic") },
  { "WINDOWS-1257",	N_("Baltic") },
  { "WINDOWS-1258",	N_("Vietnamese") },
#if 0
  /* These encodings do NOT pass-through ASCII, so are always rejected.
   * FIXME: why are they in this table; or rather why do we need
   * the ASCII pass-through requirement?
   */
  { "UTF-7",  N_("Unicode") },
  { "UTF-16", N_("Unicode") },
  { "UCS-2",  N_("Unicode") },
  { "UCS-4",  N_("Unicode") },
  { "JOHAB",  N_("Korean") },
#endif
};
                           
static GHashTable *encodings_hashtable;
static gboolean encodings_writable;

typedef struct {
  GtkWidget *dialog;
  GtkListStore *base_store;
  GtkTreeView *available_tree_view;
  GtkTreeSelection *available_selection;
  GtkTreeModel *available_model;
  GtkTreeView *active_tree_view;
  GtkTreeSelection *active_selection;
  GtkTreeModel *active_model;
  GtkWidget *add_button;
  GtkWidget *remove_button;
} EncodingDialogData;

static GtkWidget *encoding_dialog = NULL;

static void update_active_encoding_tree_models (void);

static void encodings_notify_cb (GConfClient *client,
                                 guint        cnxn_id,
                                 GConfEntry  *entry,
                                 gpointer     user_data);

static TerminalEncoding *
terminal_encoding_new (const char *charset,
                       const char *name,
                       gboolean is_custom,
                       gboolean force_valid)
{
  TerminalEncoding *encoding;

  encoding = g_slice_new (TerminalEncoding);
  encoding->refcount = 1;
  encoding->charset = g_strdup (charset);
  encoding->name = g_strdup (name);
  encoding->valid = encoding->validity_checked = force_valid;
  encoding->is_custom = is_custom;

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
      g_print("Rejecting encoding %s as invalid:\n", encoding->charset);
      g_print(" input  \"%s\"\n", ascii_sample);
      g_print(" output \"%s\" bytes read %u written %u\n",
              converted ? converted : "(null)", bytes_read, bytes_written);
      if (error)
        g_print (" Error: %s\n", error->message);
      g_print ("\n");
    }
  else
    g_print ("Encoding %s is valid\n\n", encoding->charset);
#endif

  g_clear_error (&error);
  g_free (converted);

  encoding->validity_checked = TRUE;
  return encoding->valid;
}

#define TERMINAL_TYPE_ENCODING (terminal_encoding_get_type ())

static GType
terminal_encoding_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0)) {
    type = g_boxed_type_register_static (I_("TerminalEncoding"),
                                         (GBoxedCopyFunc) terminal_encoding_ref,
                                         (GBoxedFreeFunc) terminal_encoding_unref);
  }

  return type;
}

static void
encoding_mark_active (gpointer key,
                      gpointer value,
                      gpointer data)
{
  TerminalEncoding *encoding = (TerminalEncoding *) value;
  guint active = GPOINTER_TO_UINT (data);

  encoding->is_active = active;
}

static void
encodings_notify_cb (GConfClient *client,
                     guint        cnxn_id,
                     GConfEntry  *entry,
                     gpointer     user_data)
{
  GConfValue *val;
  GSList *strings, *tmp;
  TerminalEncoding *encoding;
  const char *charset;

  encodings_writable = gconf_entry_get_is_writable (entry);

  /* Mark all as non-active, then re-enable the active ones */
  g_hash_table_foreach (encodings_hashtable, (GHFunc) encoding_mark_active, GUINT_TO_POINTER (FALSE));

  /* First add the local encoding. */
  if (!g_get_charset (&charset))
    {
      encoding = g_hash_table_lookup (encodings_hashtable, charset);
      if (encoding)
        encoding->is_active = TRUE;
    }

  /* Always ensure that UTF-8 is available. */
  encoding = g_hash_table_lookup (encodings_hashtable, "UTF-8");
  g_assert (encoding);
  encoding->is_active = TRUE;

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
      
      charset = gconf_value_get_string (v);
      if (!charset)
        continue;

      /* We already handled the locale charset above */
      if (strcmp (charset, "current") == 0)
        continue; 

      encoding = g_hash_table_lookup (encodings_hashtable, charset);
      if (!encoding)
        {
          encoding = terminal_encoding_new (charset,
                                            _("User Defined"),
                                            TRUE,
                                            TRUE /* scary! */);
          g_hash_table_insert (encodings_hashtable, encoding->charset, encoding);
        }

      if (!terminal_encoding_is_valid (encoding))
        continue;

      encoding->is_active = TRUE;
    }

  update_active_encoding_tree_models ();
}

static void
update_active_encodings_gconf (void)
{
  GSList *list, *l;
  GSList *strings = NULL;
  GConfClient *conf;

  list = terminal_get_active_encodings ();
  for (l = list; l != NULL; l = l->next)
    {
      TerminalEncoding *encoding = (TerminalEncoding *) l->data;

      strings = g_slist_prepend (strings, encoding->charset);
    }

  conf = gconf_client_get_default ();
  gconf_client_set_list (conf,
                         CONF_GLOBAL_PREFIX"/active_encodings",
                         GCONF_VALUE_STRING,
                         strings,
                         NULL);
  g_object_unref (conf);

  g_slist_free (strings);
  g_slist_foreach (list, (GFunc) terminal_encoding_unref, NULL);
  g_slist_free (list);
}

static void
add_active_encoding_to_list (gpointer key,
                             gpointer value,
                             gpointer data)
{
  TerminalEncoding *encoding = (TerminalEncoding *) value;
  GSList **list = (GSList **) data;

  if (!encoding->is_active)
    return;

  *list = g_slist_prepend (*list, terminal_encoding_ref (encoding));
}

/**
 * terminal_get_active_encodings:
 *
 * Returns: a newly allocated list of newly referenced #TerminalEncoding objects.
 */
GSList*
terminal_get_active_encodings (void)
{
  GSList *list = NULL;

  g_hash_table_foreach (encodings_hashtable, (GHFunc) add_active_encoding_to_list, &list);
  return g_slist_reverse (list); /* FIXME sort ! */
}

static void
response_callback (GtkWidget *window,
                   int        id,
                   EncodingDialogData *data)
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
  COLUMN_DATA,
  N_COLUMNS
};

static void
selection_changed_cb (GtkTreeSelection *selection,
                      EncodingDialogData *data)
{
  GtkWidget *button;
  gboolean have_selection;

  if (selection == data->available_selection)
    button = data->add_button;
  else if (selection == data->active_selection)
    button = data->remove_button;
  else
    g_assert_not_reached ();

  have_selection = gtk_tree_selection_get_selected (selection, NULL, NULL);
  gtk_widget_set_sensitive (button, have_selection);
}

static void
button_clicked_cb (GtkWidget *button,
                   EncodingDialogData *data)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter filter_iter, iter;
  TerminalEncoding *encoding;

  if (button == data->add_button)
    selection = data->available_selection;
  else if (button == data->remove_button)
    selection = data->active_selection;
  else
    g_assert_not_reached ();

  if (!gtk_tree_selection_get_selected (selection, &model, &filter_iter))
    return;

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                    &iter,
                                                    &filter_iter);

  model = GTK_TREE_MODEL (data->base_store);
  gtk_tree_model_get (model, &iter, COLUMN_DATA, &encoding, -1);
  g_assert (encoding != NULL);

  if (button == data->add_button)
    encoding->is_active = TRUE;
  else if (button == data->remove_button)
    encoding->is_active = FALSE;
  else
    g_assert_not_reached ();

  terminal_encoding_unref (encoding);

  /* We don't need to emit row-changed here, since updating the gconf pref
   * will update the models.
   */
  
  update_active_encodings_gconf ();
}

static void
liststore_insert_encoding (gpointer key,
                           TerminalEncoding *encoding,
                           GtkListStore *store)
{
  GtkTreeIter iter;

  if (!terminal_encoding_is_valid (encoding))
    return;

  gtk_list_store_insert_with_values (store, &iter, -1,
                                     COLUMN_CHARSET, encoding->charset,
                                     COLUMN_NAME, encoding->name,
                                     COLUMN_DATA, encoding,
                                     -1);
}

static gboolean
filter_active_encodings (GtkTreeModel *child_model,
                         GtkTreeIter *child_iter,
                         gpointer data)
{
  TerminalEncoding *encoding;
  gboolean active = GPOINTER_TO_UINT (data);
  gboolean visible;

  gtk_tree_model_get (child_model, child_iter, COLUMN_DATA, &encoding, -1);
  visible = active ? encoding->is_active : !encoding->is_active;
  terminal_encoding_unref (encoding);

  return visible;
}

static GtkTreeModel *
encodings_create_treemodel (GtkListStore *base_store,
                            gboolean active)
{
  GtkTreeModel *model;

  model = gtk_tree_model_filter_new (GTK_TREE_MODEL (base_store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (model),
                                          filter_active_encodings,
                                          GUINT_TO_POINTER (active), NULL);

  return model;
}

static void
update_single_liststore (EncodingDialogData *data)
{
  gtk_list_store_clear (data->base_store);
  g_hash_table_foreach (encodings_hashtable, (GHFunc) liststore_insert_encoding, data->base_store);
}

static GSList *encoding_dialogs_data = NULL;

static void
unregister_liststore (void    *data,
                      GObject *where_object_was)
{
  encoding_dialogs_data = g_slist_remove (encoding_dialogs_data, data);
}

static void
update_active_encoding_tree_models (void)
{
  g_slist_foreach (encoding_dialogs_data, (GFunc) update_single_liststore, NULL);
}

static void
register_liststore (EncodingDialogData *data)
{
  update_single_liststore (data);
  encoding_dialogs_data = g_slist_prepend (encoding_dialogs_data, data);
  g_object_weak_ref (G_OBJECT (data->dialog), unregister_liststore, data);
}

void
terminal_encoding_dialog_show (GtkWindow *transient_parent)
{
  GtkCellRenderer *cell_renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *model;
  EncodingDialogData *data;

  if (encoding_dialog)
    {
      gtk_window_set_transient_for (GTK_WINDOW (encoding_dialog), transient_parent);
      gtk_window_present (GTK_WINDOW (encoding_dialog));
      return;
    }

  data = g_new (EncodingDialogData, 1);

  if (!terminal_util_load_builder_file ("encodings-dialog.ui",
                                        "encodings-dialog", &data->dialog,
                                        "add-button", &data->add_button,
                                        "remove-button", &data->remove_button,
                                        "available-treeview", &data->available_tree_view,
                                        "displayed-treeview", &data->active_tree_view,
                                        NULL))
    {
      g_free (data);
      return;
    }

  g_object_set_data_full (G_OBJECT (data->dialog), "GT::Data", data, (GDestroyNotify) g_free);

  gtk_window_set_transient_for (GTK_WINDOW (data->dialog), transient_parent);
  gtk_window_set_role (GTK_WINDOW (data->dialog), "gnome-terminal-encodings");
  g_signal_connect (data->dialog, "response",
                    G_CALLBACK (response_callback), data);

  /* buttons */
  g_signal_connect (data->add_button, "clicked",
                    G_CALLBACK (button_clicked_cb), data);

  g_signal_connect (data->remove_button, "clicked",
                    G_CALLBACK (button_clicked_cb), data);
  
  /* Tree view of available encodings */
  /* Column 1 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Description"),
						     cell_renderer,
						     "text", COLUMN_NAME,
						     NULL);
  gtk_tree_view_append_column (data->available_tree_view, column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  
  /* Column 2 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Encoding"),
						     cell_renderer,
						     "text", COLUMN_CHARSET,
						     NULL);
  gtk_tree_view_append_column (data->available_tree_view, column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);  

  data->available_selection = gtk_tree_view_get_selection (data->available_tree_view);
  gtk_tree_selection_set_mode (data->available_selection, GTK_SELECTION_BROWSE);

  g_signal_connect (data->available_selection, "changed",
                    G_CALLBACK (selection_changed_cb), data);

  /* Tree view of selected encodings */
  /* Column 1 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Description"),
						     cell_renderer,
						     "text", COLUMN_NAME,
						     NULL);
  gtk_tree_view_append_column (data->active_tree_view, column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
  
  /* Column 2 */
  cell_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("_Encoding"),
						     cell_renderer,
						     "text", COLUMN_CHARSET,
						     NULL);
  gtk_tree_view_append_column (data->active_tree_view, column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CHARSET);  

  /* Add the data */

  data->active_selection = gtk_tree_view_get_selection (data->active_tree_view);
  gtk_tree_selection_set_mode (data->active_selection, GTK_SELECTION_BROWSE);

  g_signal_connect (data->active_selection, "changed",
                    G_CALLBACK (selection_changed_cb), data);

  data->base_store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, TERMINAL_TYPE_ENCODING);
  register_liststore (data);
  /* Now turn on sorting */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (data->base_store),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  
  model = encodings_create_treemodel (data->base_store, FALSE);
  gtk_tree_view_set_model (data->available_tree_view, model);
  g_object_unref (model);

  model = encodings_create_treemodel (data->base_store, TRUE);
  gtk_tree_view_set_model (data->active_tree_view, model);
  g_object_unref (model);

  g_object_unref (data->base_store);

  gtk_window_present (GTK_WINDOW (data->dialog));

  encoding_dialog = data->dialog;
  g_signal_connect (data->dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed), &encoding_dialog);
}

void
terminal_encoding_init (void)
{
  GConfClient *conf;
  guint i;
  const char *locale_charset = NULL;

  conf = gconf_client_get_default ();

  encodings_hashtable = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               NULL,
                                               (GDestroyNotify) terminal_encoding_unref);

  if (!g_get_charset (&locale_charset))
    {
      TerminalEncoding *encoding;

      encoding = terminal_encoding_new (locale_charset,
                                        _("Current Locale"),
                                        FALSE,
                                        TRUE);
      g_hash_table_insert (encodings_hashtable, encoding->charset, encoding);
    }

  for (i = 0; i < G_N_ELEMENTS (encodings); ++i)
    {
      TerminalEncoding *encoding;

      encoding = terminal_encoding_new (encodings[i].charset,
                                        _(encodings[i].name),
                                        FALSE,
                                        FALSE);
      g_hash_table_insert (encodings_hashtable, encoding->charset, encoding);
    }

  gconf_client_notify_add (conf,
                           CONF_GLOBAL_PREFIX"/active_encodings",
                           encodings_notify_cb,
                           NULL /* user_data */, NULL,
                           NULL);

  gconf_client_notify (conf, CONF_GLOBAL_PREFIX"/active_encodings");

  g_object_unref (conf);
}
