/*
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2008, 2017 Christian Persch
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

#include <string.h>
#include <search.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-encoding.h"
#include "terminal-schemas.h"
#include "terminal-util.h"
#include "terminal-libgsystem.h"

/* Overview
 *
 * There's a list of character sets stored in gsettings, indicating
 * which encodings to display in the encoding menu.
 * 
 * We have a pre-canned list of available encodings
 * (hardcoded in the table below) that can be added to
 * the encoding menu, and to give a human-readable name
 * to certain encodings.
 *
 * If the setting list contains an encoding not in the
 * predetermined table, then that encoding is
 * labeled "user defined" but still appears in the menu.
 */

typedef enum {
  GROUP_UNICODE,
  GROUP_ASIAN,
  GROUP_EUROPEAN,
  LAST_GROUP
} EncodingGroup;

typedef struct {
  const char *charset;
  const char *name;
  EncodingGroup group;
} EncodingEntry;

/* These MUST be sorted by charset so that bsearch can work! */
static const EncodingEntry const encodings[] = {
  { "ARMSCII-8",      N_("Armenian"),            GROUP_ASIAN },
  { "BIG5",           N_("Chinese Traditional"), GROUP_ASIAN },
  { "BIG5-HKSCS",     N_("Chinese Traditional"), GROUP_ASIAN },
  { "CP866",          N_("Cyrillic/Russian"),    GROUP_EUROPEAN },
  { "EUC-JP",         N_("Japanese"),            GROUP_ASIAN },
  { "EUC-KR",         N_("Korean"),              GROUP_ASIAN },
  { "EUC-TW",         N_("Chinese Traditional"), GROUP_ASIAN },
  { "GB18030",        N_("Chinese Simplified"),  GROUP_ASIAN },
  { "GB2312",         N_("Chinese Simplified"),  GROUP_ASIAN },
  { "GBK",            N_("Chinese Simplified"),  GROUP_ASIAN },
  { "GEORGIAN-PS",    N_("Georgian"),            GROUP_ASIAN },
  { "IBM850",         N_("Western"),             GROUP_EUROPEAN },
  { "IBM852",         N_("Central European"),    GROUP_EUROPEAN },
  { "IBM855",         N_("Cyrillic"),            GROUP_EUROPEAN },
  { "IBM857",         N_("Turkish"),             GROUP_ASIAN },
  { "IBM862",         N_("Hebrew"),              GROUP_ASIAN },
  { "IBM864",         N_("Arabic"),              GROUP_ASIAN },
  { "ISO-2022-JP",    N_("Japanese"),            GROUP_ASIAN },
  { "ISO-2022-KR",    N_("Korean"),              GROUP_ASIAN },
  { "ISO-8859-1",     N_("Western"),             GROUP_EUROPEAN },
  { "ISO-8859-10",    N_("Nordic"),              GROUP_EUROPEAN },
  { "ISO-8859-13",    N_("Baltic"),              GROUP_EUROPEAN },
  { "ISO-8859-14",    N_("Celtic"),              GROUP_EUROPEAN },
  { "ISO-8859-15",    N_("Western"),             GROUP_EUROPEAN },
  { "ISO-8859-16",    N_("Romanian"),            GROUP_EUROPEAN },
  { "ISO-8859-2",     N_("Central European"),    GROUP_EUROPEAN },
  { "ISO-8859-3",     N_("South European"),      GROUP_EUROPEAN },
  { "ISO-8859-4",     N_("Baltic"),              GROUP_EUROPEAN },
  { "ISO-8859-5",     N_("Cyrillic"),            GROUP_EUROPEAN },
  { "ISO-8859-6",     N_("Arabic"),              GROUP_ASIAN },
  { "ISO-8859-7",     N_("Greek"),               GROUP_EUROPEAN },
  { "ISO-8859-8",     N_("Hebrew Visual"),       GROUP_ASIAN },
  { "ISO-8859-8-I",   N_("Hebrew"),              GROUP_ASIAN },
  { "ISO-8859-9",     N_("Turkish"),             GROUP_ASIAN },
  { "ISO-IR-111",     N_("Cyrillic"),            GROUP_EUROPEAN },
   /* { "JOHAB",      N_("Korean"),              GROUP_ASIAN }, */
  { "KOI8-R",         N_("Cyrillic"),            GROUP_EUROPEAN },
  { "KOI8-U",         N_("Cyrillic/Ukrainian"),  GROUP_EUROPEAN },
  { "MAC-CYRILLIC",   N_("Cyrillic"),            GROUP_EUROPEAN },
  { "MAC_ARABIC",     N_("Arabic"),              GROUP_ASIAN },
  { "MAC_CE",         N_("Central European"),    GROUP_EUROPEAN },
  { "MAC_CROATIAN",   N_("Croatian"),            GROUP_EUROPEAN },
  { "MAC_DEVANAGARI", N_("Hindi"),               GROUP_ASIAN },
  { "MAC_FARSI",      N_("Persian"),             GROUP_ASIAN },
  { "MAC_GREEK",      N_("Greek"),               GROUP_EUROPEAN },
  { "MAC_GUJARATI",   N_("Gujarati"),            GROUP_ASIAN },
  { "MAC_GURMUKHI",   N_("Gurmukhi"),            GROUP_ASIAN },
  { "MAC_HEBREW",     N_("Hebrew"),              GROUP_ASIAN },
  { "MAC_ICELANDIC",  N_("Icelandic"),           GROUP_EUROPEAN },
  { "MAC_ROMAN",      N_("Western"),             GROUP_EUROPEAN },
  { "MAC_ROMANIAN",   N_("Romanian"),            GROUP_EUROPEAN },
  { "MAC_TURKISH",    N_("Turkish"),             GROUP_ASIAN },
  { "MAC_UKRAINIAN",  N_("Cyrillic/Ukrainian"),  GROUP_EUROPEAN },
  { "SHIFT_JIS",      N_("Japanese"),            GROUP_ASIAN },
  { "TCVN",           N_("Vietnamese"),          GROUP_ASIAN },
  { "TIS-620",        N_("Thai"),                GROUP_ASIAN },
  /* { "UCS-4",       N_("Unicode"),             GROUP_UNICODE }, */
  { "UHC",            N_("Korean"),              GROUP_ASIAN },
  /* { "UTF-16",      N_("Unicode"),             GROUP_UNICODE }, */
  /* { "UTF-32",      N_("Unicode"),             GROUP_UNICODE }, */
  /* { "UTF-7",       N_("Unicode"),             GROUP_UNICODE }, */
  { "UTF-8",          N_("Unicode"),             GROUP_UNICODE },
  { "VISCII",         N_("Vietnamese"),          GROUP_ASIAN },
  { "WINDOWS-1250",   N_("Central European"),    GROUP_EUROPEAN },
  { "WINDOWS-1251",   N_("Cyrillic"),            GROUP_EUROPEAN },
  { "WINDOWS-1252",   N_("Western"),             GROUP_EUROPEAN },
  { "WINDOWS-1253",   N_("Greek"),               GROUP_EUROPEAN },
  { "WINDOWS-1254",   N_("Turkish"),             GROUP_ASIAN },
  { "WINDOWS-1255",   N_("Hebrew"),              GROUP_ASIAN },
  { "WINDOWS-1256",   N_("Arabic"),              GROUP_ASIAN },
  { "WINDOWS-1257",   N_("Baltic"),              GROUP_EUROPEAN },
  { "WINDOWS-1258",   N_("Vietnamese"),          GROUP_ASIAN },
};

static const struct {
  EncodingGroup group;
  const char *name;
} group_names[] = {
  { GROUP_UNICODE,  N_("Unicode") },
  { GROUP_ASIAN,    N_("Legacy Asian Encodings") },
  { GROUP_EUROPEAN, N_("Legacy European Encodings") },
};

#define EM_DASH "—"

static int
compare_encoding_entry_cb (const void *ap,
                           const void *bp)
{
  const EncodingEntry *a = ap;
  const EncodingEntry *b = bp;

  int r = a->group - b->group;
  if (r != 0)
    return r;

  r = g_utf8_collate (a->name, b->name);
  if (r != 0)
    return r;

  return strcmp (a->charset, b->charset);
}

/**
 * terminal_encodings_append_menu:
 *
 * Appends to known encodings to a #GMenu, sorted in groups and
 * alphabetically by name inside the groups. The action name
 * used when activating the menu items is "win.encoding".
 */
void
terminal_encodings_append_menu (GMenu *menu)
{
  /* First, sort the encodings */
  gs_free EncodingEntry *array = g_memdup (encodings, sizeof encodings);
  for (guint i = 0; i < G_N_ELEMENTS (encodings); i++)
    array[i].name = _(array[i].name); /* translate */

  qsort (array, G_N_ELEMENTS (encodings), sizeof array[0],
         compare_encoding_entry_cb);

  for (guint group = 0 ; group < LAST_GROUP; group++) {
    gs_unref_object GMenu *section = g_menu_new ();

    for (guint i = 0; i < G_N_ELEMENTS (encodings); i++) {
      if (array[i].group != group)
        continue;

      gs_free_gstring GString *str = g_string_sized_new (128);
      g_string_append (str, array[i].name);
      g_string_append (str, " " EM_DASH " ");
      for (const char *p = array[i].charset; *p; p++) {
        if (*p == '_')
          g_string_append (str, "__");
        else
          g_string_append_c (str, *p);
      }

      gs_unref_object GMenuItem *item = g_menu_item_new (str->str, NULL);
      g_menu_item_set_action_and_target (item, "win.encoding", "s", array[i].charset);

      g_menu_append_item (section, item);
    }

    g_menu_append_section (menu, _(group_names[group].name), G_MENU_MODEL (section));
  }
}

/**
 * terminal_encodings_list_store_new:
 *
 * Creates a #GtkListStore containing the known encodings.
 * The model containing 2 columns, the 0th one with the
 * charset name, and the 1st one with the label.
 * The model is unsorted.
 *
 * Returns: (transfer full): a new #GtkTreeModel
 */
GtkListStore *
terminal_encodings_list_store_new (int column_id,
                                   int column_text)
{
  GtkListStore *store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  for (guint i = 0; i < G_N_ELEMENTS (encodings); i++) {
    gs_free char *name = g_strdup_printf ("%s " EM_DASH " %s",
                                          encodings[i].name, encodings[i].charset);

    GtkTreeIter iter;
    gtk_list_store_insert_with_values (store, &iter, -1,
                                       column_id, encodings[i].charset,
                                       column_text, name,
                                       -1);
  }

  return store;
}

static int
compare_charset_cb (const void *ap,
                    const void *bp)
{
  const EncodingEntry *a = ap;
  const EncodingEntry *b = bp;

  return strcmp (a->charset, b->charset);
}

gboolean
terminal_encodings_is_known_charset (const char *charset)
{
  EncodingEntry key = { charset, NULL, 0 };
  return bsearch (&key,
                  encodings, G_N_ELEMENTS (encodings),
                  sizeof (encodings[0]),
                  compare_charset_cb) != NULL;
}
