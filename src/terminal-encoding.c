/*
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2008 Christian Persch
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-encoding.h"
#include "terminal-schemas.h"
#include "terminal-util.h"

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

static const struct {
  const char *charset;
  const char *name;
} encodings[] = {
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

TerminalEncoding *
terminal_encoding_new (const char *charset,
                       const char *display_name,
                       gboolean is_custom,
                       gboolean force_valid)
{
  TerminalEncoding *encoding;

  encoding = g_slice_new (TerminalEncoding);
  encoding->refcount = 1;
  encoding->charset = g_intern_string (charset);
  encoding->name = g_strdup (display_name);
  encoding->valid = encoding->validity_checked = force_valid || g_str_equal (charset, "UTF-8");
  encoding->is_custom = is_custom;
  encoding->is_active = FALSE;

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
  g_slice_free (TerminalEncoding, encoding);
}

const char *
terminal_encoding_get_charset (TerminalEncoding *encoding)
{
  g_return_val_if_fail (encoding != NULL, NULL);

  return encoding->charset;
}

gboolean
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
                         terminal_encoding_get_charset (encoding), "UTF-8",
                         &bytes_read, &bytes_written, &error);

  /* The encoding is only valid if ASCII passes through cleanly. */
  encoding->valid = (bytes_read == (sizeof (ascii_sample) - 1)) &&
                    (converted != NULL) &&
                    (strcmp (converted, ascii_sample) == 0);

#ifdef ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ENCODINGS)
  {
    if (!encoding->valid)
      {
        _terminal_debug_print (TERMINAL_DEBUG_ENCODINGS,
                               "Rejecting encoding %s as invalid:\n",
                               terminal_encoding_get_charset (encoding));
        _terminal_debug_print (TERMINAL_DEBUG_ENCODINGS,
                               " input  \"%s\"\n",
                               ascii_sample);
        _terminal_debug_print (TERMINAL_DEBUG_ENCODINGS,
                               " output \"%s\" bytes read %" G_GSIZE_FORMAT " written %" G_GSIZE_FORMAT "\n",
                               converted ? converted : "(null)", bytes_read, bytes_written);
        if (error)
          _terminal_debug_print (TERMINAL_DEBUG_ENCODINGS,
                                 " Error: %s\n",
                                 error->message);
      }
    else
        _terminal_debug_print (TERMINAL_DEBUG_ENCODINGS,
                               "Encoding %s is valid\n\n",
                               terminal_encoding_get_charset (encoding));
  }
#endif

  g_clear_error (&error);
  g_free (converted);

  encoding->validity_checked = TRUE;
  return encoding->valid;
}

G_DEFINE_BOXED_TYPE (TerminalEncoding, terminal_encoding,
                     terminal_encoding_ref,
                     terminal_encoding_unref);

GHashTable *
terminal_encodings_get_builtins (void)
{
  GHashTable *encodings_hashtable;
  guint i;
  TerminalEncoding *encoding;

  encodings_hashtable = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               NULL,
                                               (GDestroyNotify) terminal_encoding_unref);

  for (i = 0; i < G_N_ELEMENTS (encodings); ++i)
    {
      encoding = terminal_encoding_new (encodings[i].charset,
                                        _(encodings[i].name),
                                        FALSE,
                                        FALSE);
      g_hash_table_insert (encodings_hashtable,
                           (gpointer) terminal_encoding_get_charset (encoding),
                           encoding);
    }

  return encodings_hashtable;
}
