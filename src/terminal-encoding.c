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
  GROUP_CJKV,
  GROUP_OBSOLETE,
  LAST_GROUP
} EncodingGroup;

typedef struct {
  const char *charset;
  const char *name;
  EncodingGroup group;
} EncodingEntry;

/* These MUST be sorted by charset so that bsearch can work! */
static const EncodingEntry const encodings[] = {
  { "ARMSCII-8",      N_("Armenian"),            GROUP_OBSOLETE },
  { "BIG5",           N_("Chinese Traditional"), GROUP_CJKV },
  { "BIG5-HKSCS",     N_("Chinese Traditional"), GROUP_CJKV },
  { "CP866",          N_("Cyrillic/Russian"),    GROUP_OBSOLETE },
  { "EUC-JP",         N_("Japanese"),            GROUP_CJKV },
  { "EUC-KR",         N_("Korean"),              GROUP_CJKV },
  { "EUC-TW",         N_("Chinese Traditional"), GROUP_CJKV },
  { "GB18030",        N_("Chinese Simplified"),  GROUP_CJKV },
  { "GB2312",         N_("Chinese Simplified"),  GROUP_CJKV },
  { "GBK",            N_("Chinese Simplified"),  GROUP_CJKV },
  { "GEORGIAN-PS",    N_("Georgian"),            GROUP_OBSOLETE },
  { "IBM850",         N_("Western"),             GROUP_OBSOLETE },
  { "IBM852",         N_("Central European"),    GROUP_OBSOLETE },
  { "IBM855",         N_("Cyrillic"),            GROUP_OBSOLETE },
  { "IBM857",         N_("Turkish"),             GROUP_OBSOLETE },
  { "IBM862",         N_("Hebrew"),              GROUP_OBSOLETE },
  { "IBM864",         N_("Arabic"),              GROUP_OBSOLETE },
  { "ISO-2022-JP",    N_("Japanese"),            GROUP_CJKV },
  { "ISO-2022-KR",    N_("Korean"),              GROUP_CJKV },
  { "ISO-8859-1",     N_("Western"),             GROUP_OBSOLETE },
  { "ISO-8859-10",    N_("Nordic"),              GROUP_OBSOLETE },
  { "ISO-8859-13",    N_("Baltic"),              GROUP_OBSOLETE },
  { "ISO-8859-14",    N_("Celtic"),              GROUP_OBSOLETE },
  { "ISO-8859-15",    N_("Western"),             GROUP_OBSOLETE },
  { "ISO-8859-16",    N_("Romanian"),            GROUP_OBSOLETE },
  { "ISO-8859-2",     N_("Central European"),    GROUP_OBSOLETE },
  { "ISO-8859-3",     N_("South European"),      GROUP_OBSOLETE },
  { "ISO-8859-4",     N_("Baltic"),              GROUP_OBSOLETE },
  { "ISO-8859-5",     N_("Cyrillic"),            GROUP_OBSOLETE },
  { "ISO-8859-6",     N_("Arabic"),              GROUP_OBSOLETE },
  { "ISO-8859-7",     N_("Greek"),               GROUP_OBSOLETE },
  { "ISO-8859-8",     N_("Hebrew Visual"),       GROUP_OBSOLETE },
  { "ISO-8859-8-I",   N_("Hebrew"),              GROUP_OBSOLETE },
  { "ISO-8859-9",     N_("Turkish"),             GROUP_OBSOLETE },
  { "ISO-IR-111",     N_("Cyrillic"),            GROUP_OBSOLETE },
   /* { "JOHAB",      N_("Korean"),              GROUP_CJKV }, */
  { "KOI8-R",         N_("Cyrillic"),            GROUP_OBSOLETE },
  { "KOI8-U",         N_("Cyrillic/Ukrainian"),  GROUP_OBSOLETE },
  { "MAC-CYRILLIC",   N_("Cyrillic"),            GROUP_OBSOLETE },
  { "MAC_ARABIC",     N_("Arabic"),              GROUP_OBSOLETE },
  { "MAC_CE",         N_("Central European"),    GROUP_OBSOLETE },
  { "MAC_CROATIAN",   N_("Croatian"),            GROUP_OBSOLETE },
  { "MAC_DEVANAGARI", N_("Hindi"),               GROUP_OBSOLETE },
  { "MAC_FARSI",      N_("Persian"),             GROUP_OBSOLETE },
  { "MAC_GREEK",      N_("Greek"),               GROUP_OBSOLETE },
  { "MAC_GUJARATI",   N_("Gujarati"),            GROUP_OBSOLETE },
  { "MAC_GURMUKHI",   N_("Gurmukhi"),            GROUP_OBSOLETE },
  { "MAC_HEBREW",     N_("Hebrew"),              GROUP_OBSOLETE },
  { "MAC_ICELANDIC",  N_("Icelandic"),           GROUP_OBSOLETE },
  { "MAC_ROMAN",      N_("Western"),             GROUP_OBSOLETE },
  { "MAC_ROMANIAN",   N_("Romanian"),            GROUP_OBSOLETE },
  { "MAC_TURKISH",    N_("Turkish"),             GROUP_OBSOLETE },
  { "MAC_UKRAINIAN",  N_("Cyrillic/Ukrainian"),  GROUP_OBSOLETE },
  { "SHIFT_JIS",      N_("Japanese"),            GROUP_CJKV },
  /* This is TCVN-5712-1, not TCVN-5773:1993 which would be CJKV */
  { "TCVN",           N_("Vietnamese"),          GROUP_OBSOLETE },
  { "TIS-620",        N_("Thai"),                GROUP_OBSOLETE },
  /* { "UCS-4",       N_("Unicode"),             GROUP_UNICODE }, */
  { "UHC",            N_("Korean"),              GROUP_CJKV },
  /* { "UTF-16",      N_("Unicode"),             GROUP_UNICODE }, */
  /* { "UTF-32",      N_("Unicode"),             GROUP_UNICODE }, */
  /* { "UTF-7",       N_("Unicode"),             GROUP_UNICODE }, */
  { "UTF-8",          N_("Unicode"),             GROUP_UNICODE },
  { "VISCII",         N_("Vietnamese"),          GROUP_OBSOLETE },
  { "WINDOWS-1250",   N_("Central European"),    GROUP_OBSOLETE },
  { "WINDOWS-1251",   N_("Cyrillic"),            GROUP_OBSOLETE },
  { "WINDOWS-1252",   N_("Western"),             GROUP_OBSOLETE },
  { "WINDOWS-1253",   N_("Greek"),               GROUP_OBSOLETE },
  { "WINDOWS-1254",   N_("Turkish"),             GROUP_OBSOLETE },
  { "WINDOWS-1255",   N_("Hebrew"),              GROUP_OBSOLETE},
  { "WINDOWS-1256",   N_("Arabic"),              GROUP_OBSOLETE },
  { "WINDOWS-1257",   N_("Baltic"),              GROUP_OBSOLETE },
  { "WINDOWS-1258",   N_("Vietnamese"),          GROUP_OBSOLETE },
};

static const struct {
  EncodingGroup group;
  const char *name;
} group_names[] = {
  { GROUP_UNICODE,  N_("Unicode") },
  { GROUP_CJKV,     N_("Legacy CJK Encodings") },
  { GROUP_OBSOLETE, N_("Obsolete Encodings") },
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
                                          _(encodings[i].name), encodings[i].charset);

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
