/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-string.h: String routines to augment <string.h>.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef EEL_STRING_H
#define EEL_STRING_H

#include <glib.h>
#include <string.h>

/* We use the "str" abbrevation to mean char * string, since
 * "string" usually means g_string instead. We use the "istr"
 * abbreviation to mean a case-insensitive char *.
 */

/* NULL is allowed for all the str parameters to these functions. */

/* Versions of basic string functions that allow NULL, and handle
 * cases that the standard ones get a bit wrong for our purposes.
 */
size_t   eel_strlen                        (const char    *str);
char *   eel_strchr                        (const char    *haystack,
					    char           needle);
int      eel_strcmp                        (const char    *str_a,
					    const char    *str_b);
int      eel_strcoll                       (const char    *str_a,
					    const char    *str_b);
int      eel_strcasecmp                    (const char    *str_a,
					    const char    *str_b);
int      eel_strcmp_case_breaks_ties       (const char    *str_a,
					    const char    *str_b);

/* GCompareFunc versions. */
int      eel_strcmp_compare_func           (gconstpointer  str_a,
					    gconstpointer  str_b);
int      eel_strcoll_compare_func          (gconstpointer  str_a,
					    gconstpointer  str_b);
int      eel_strcasecmp_compare_func       (gconstpointer  str_a,
					    gconstpointer  str_b);

/* Other basic string operations. */
gboolean eel_str_is_empty                  (const char    *str_or_null);
gboolean eel_str_is_equal                  (const char    *str_a,
					    const char    *str_b);
gboolean eel_istr_is_equal                 (const char    *str_a,
					    const char    *str_b);
gboolean eel_str_has_prefix                (const char    *target,
					    const char    *prefix);
char *   eel_str_get_prefix                (const char    *source,
					    const char    *delimiter);
char *   eel_str_get_after_prefix          (const char    *source,
					    const char    *delimiter);
gboolean eel_istr_has_prefix               (const char    *target,
					    const char    *prefix);
gboolean eel_str_has_suffix                (const char    *target,
					    const char    *suffix);
gboolean eel_istr_has_suffix               (const char    *target,
					    const char    *suffix);
char *   eel_str_strip_chr                 (const char    *str,
					    char           remove_this);
char *   eel_str_strip_trailing_chr        (const char    *str,
					    char           remove_this);
char *   eel_str_strip_trailing_str        (const char    *str,
					    const char    *remove_this);

/* Conversions to and from strings. */
gboolean eel_str_to_int                    (const char    *str,
					    int           *integer);

/* Escape function for '_' character. */
char *   eel_str_double_underscores        (const char    *str);

/* Capitalize a string */
char *   eel_str_capitalize                (const char    *str);

/* Middle truncate a string to a maximum of truncate_length characters.
 * The resulting string will be truncated in the middle with a "..."
 * delimiter.
 */
char *   eel_str_middle_truncate           (const char    *str,
					    guint          truncate_length);


/* Count the number of 'c' characters that occur in 'string'. */
guint    eel_str_count_characters          (const char    *str,
					    char           c);

/* Remove all characters after the passed-in substring. */
char *   eel_str_strip_substring_and_after (const char    *str,
					    const char    *substring);

/* Replace all occurrences of substring with replacement. */
char *   eel_str_replace_substring         (const char    *str,
					    const char    *substring,
					    const char    *replacement);

/* Remove all text in brackets.  Used where context is included in strings to 
 * be internationalized, to help translators, and to make sure that strings
 * that may be used in different places with a different meaning may be 
 * translated separately.  If brackets are not even, it will just return a 
 * copy of the original string. 
 */
char *   eel_str_remove_bracketed_text     (const char    *text);

#endif /* EEL_STRING_H */
