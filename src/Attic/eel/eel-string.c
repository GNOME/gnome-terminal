/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-string.c: String routines to augment <string.h>.

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

#include <config.h>
#include "eel-string.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>

#if !defined (EEL_OMIT_SELF_CHECK)
#include "eel-lib-self-check-functions.h"
#endif

size_t
eel_strlen (const char *string)
{
	return string == NULL ? 0 : strlen (string);
}

char *
eel_strchr (const char *haystack, char needle)
{
	return haystack == NULL ? NULL : strchr (haystack, needle);
}

int
eel_strcmp (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return strcmp (string_a == NULL ? "" : string_a,
		       string_b == NULL ? "" : string_b);
}

int
eel_strcasecmp (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return g_ascii_strcasecmp (string_a == NULL ? "" : string_a,
				   string_b == NULL ? "" : string_b);
}

int
eel_strcmp_case_breaks_ties (const char *string_a, const char *string_b)
{
	int casecmp_result;

	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	casecmp_result = eel_strcasecmp (string_a, string_b);
	if (casecmp_result != 0) {
		return casecmp_result;
	}
	return eel_strcmp (string_a, string_b);
}

int
eel_strcoll (const char *string_a, const char *string_b)
{
	const char *locale;
	int result;
	
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */

	locale = setlocale (LC_COLLATE, NULL);
	
	if (locale == NULL || strcmp (locale, "C") == 0 || strcmp (locale, "POSIX") == 0) {
		/* If locale is NULL or default "C" or "POSIX" use eel sorting */
		return eel_strcmp_case_breaks_ties (string_a, string_b);
	} else {
		/* Use locale-specific collated sorting */
		result = strcoll (string_a == NULL ? "" : string_a,
				  string_b == NULL ? "" : string_b);
		if (result != 0) {
			return result;
		}
		return eel_strcmp (string_a, string_b);
	}
}

gboolean
eel_str_is_empty (const char *string_or_null)
{
	return eel_strcmp (string_or_null, NULL) == 0;
}

gboolean
eel_str_is_equal (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL != ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return eel_strcmp (string_a, string_b) == 0;
}

gboolean
eel_istr_is_equal (const char *string_a, const char *string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL != ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return eel_strcasecmp (string_a, string_b) == 0;
}

int
eel_strcmp_compare_func (gconstpointer string_a, gconstpointer string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return eel_strcmp ((const char *) string_a,
				(const char *) string_b);
}

int
eel_strcoll_compare_func (gconstpointer string_a, gconstpointer string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return eel_strcoll ((const char *) string_a,
				 (const char *) string_b);
}

int
eel_strcasecmp_compare_func (gconstpointer string_a, gconstpointer string_b)
{
	/* FIXME bugzilla.eazel.com 5450: Maybe we need to make this
	 * treat 'NULL < ""', or have a flavor that does that. If we
	 * didn't have code that already relies on 'NULL == ""', I
	 * would change it right now.
	 */
	return eel_strcasecmp ((const char *) string_a,
				    (const char *) string_b);
}

gboolean
eel_str_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
	} while (*h++ == *n++);
	return FALSE;
}

gboolean
eel_str_has_suffix (const char *haystack, const char *needle)
{
	const char *h, *n;

	if (needle == NULL) {
		return TRUE;
	}
	if (haystack == NULL) {
		return needle[0] == '\0';
	}
		
	/* Eat one character at a time. */
	h = haystack + strlen(haystack);
	n = needle + strlen(needle);
	do {
		if (n == needle) {
			return TRUE;
		}
		if (h == haystack) {
			return FALSE;
		}
	} while (*--h == *--n);
	return FALSE;
}

gboolean
eel_istr_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;
	char hc, nc;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
		hc = *h++;
		nc = *n++;
		hc = g_ascii_tolower (hc);
		nc = g_ascii_tolower (nc);
	} while (hc == nc);
	return FALSE;
}

gboolean
eel_istr_has_suffix (const char *haystack, const char *needle)
{
	const char *h, *n;
	char hc, nc;

	if (needle == NULL) {
		return TRUE;
	}
	if (haystack == NULL) {
		return needle[0] == '\0';
	}
		
	/* Eat one character at a time. */
	h = haystack + strlen (haystack);
	n = needle + strlen (needle);
	do {
		if (n == needle) {
			return TRUE;
		}
		if (h == haystack) {
			return FALSE;
		}
		hc = *--h;
		nc = *--n;
		hc = g_ascii_tolower (hc);
		nc = g_ascii_tolower (nc);
	} while (hc == nc);
	return FALSE;
}

/**
 * eel_str_get_prefix:
 * Get a new string containing the first part of an existing string.
 * 
 * @source: The string whose prefix should be extracted.
 * @delimiter: The string that marks the end of the prefix.
 * 
 * Return value: A newly-allocated string that that matches the first part
 * of @source, up to but not including the first occurrence of
 * @delimiter. If @source is NULL, returns NULL. If 
 * @delimiter is NULL, returns a copy of @source.
 * If @delimiter does not occur in @source, returns
 * a copy of @source.
 **/
char *
eel_str_get_prefix (const char *source, 
			 const char *delimiter)
{
	char *prefix_start;

	if (source == NULL) {
		return NULL;
	}

	if (delimiter == NULL) {
		return g_strdup (source);
	}

	prefix_start = strstr (source, delimiter);

	if (prefix_start == NULL) {
		return g_strdup ("");
	}

	return g_strndup (source, prefix_start - source);
}


/**
 * eel_str_get_after_prefix:
 * Get a new string containing the part of the string
 * after the prefix
 * @source: The string whose prefix should be extracted.
 * @delimiter: The string that marks the end of the prefix.
 * 
 * Return value: A newly-allocated string that that matches the end
 * of @source, starting right after the first occurr
 * @delimiter. If @source is NULL, returns NULL. If 
 * @delimiter is NULL, returns a copy of @source.
 * If @delimiter does not occur in @source, returns
 * NULL
 **/
char *
eel_str_get_after_prefix (const char *source,
			       const char *delimiter)
{
	char *prefix_start;
	
	if (source == NULL) {
		return NULL;
	}
	
	if (delimiter == NULL) {
		return g_strdup (source);
	}
	
	prefix_start = strstr (source, delimiter);
	
	if (prefix_start == NULL) {
		return NULL;
	}
	
	return g_strdup (prefix_start);
}

gboolean
eel_str_to_int (const char *string, int *integer)
{
	long result;
	char *parse_end;

	/* Check for the case of an empty string. */
	if (string == NULL || *string == '\0') {
		return FALSE;
	}
	
	/* Call the standard library routine to do the conversion. */
	errno = 0;
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == G_MINLONG || result == G_MAXLONG) && errno == ERANGE) {
		return FALSE;
	}
	if (result < G_MININT || result > G_MAXINT) {
		return FALSE;
	}

	/* Check that all the trailing characters are spaces. */
	while (*parse_end != '\0') {
		if (!g_ascii_isspace (*parse_end++)) {
			return FALSE;
		}
	}

	/* Return the result. */
	*integer = result;
	return TRUE;
}

/**
 * eel_str_strip_chr:
 * Remove all occurrences of a character from a string.
 * 
 * @source: The string to be stripped.
 * @remove_this: The char to remove from @source
 * 
 * Return value: A copy of @source, after removing all occurrences
 * of @remove_this.
 */
char *
eel_str_strip_chr (const char *source, char remove_this)
{
	char *result, *out;
	const char *in;
	
        if (source == NULL) {
		return NULL;
	}
	
	result = g_new (char, strlen (source) + 1);
	in = source;
	out = result;
	do {
		if (*in != remove_this) {
			*out++ = *in;
		}
	} while (*in++ != '\0');

        return result;
}

/**
 * eel_str_strip_trailing_chr:
 * Remove trailing occurrences of a character from a string.
 * 
 * @source: The string to be stripped.
 * @remove_this: The char to remove from @source
 * 
 * Return value: @source, after removing trailing occurrences
 * of @remove_this.
 */
char *
eel_str_strip_trailing_chr (const char *source, char remove_this)
{
	const char *end;
	
        if (source == NULL) {
		return NULL;
	}

	for (end = source + strlen (source); end != source; end--) {
		if (end[-1] != remove_this) {
			break;
		}
	}
	
        return g_strndup (source, end - source);
}

char *   
eel_str_strip_trailing_str (const char *source, const char *remove_this)
{
	const char *end;
	if (source == NULL) {
		return NULL;
	}
	if (remove_this == NULL) {
		return g_strdup (source);
	}
	end = source + strlen (source);
	if (strcmp (end - strlen (remove_this), remove_this) != 0) {
		return g_strdup (source);
	}
	else {
		return g_strndup (source, strlen (source) - strlen(remove_this));
	}
	
}

char *
eel_str_double_underscores (const char *string)
{
	int underscores;
	const char *p;
	char *q;
	char *escaped;
	
	if (string == NULL) {
		return NULL;
	}
	
	underscores = 0;
	for (p = string; *p != '\0'; p++) {
		underscores += (*p == '_');
	}
	
	if (underscores == 0) {
		return g_strdup (string);
	}

	escaped = g_new (char, strlen (string) + underscores + 1);
	for (p = string, q = escaped; *p != '\0'; p++, q++) {
		/* Add an extra underscore. */
		if (*p == '_') {
			*q++ = '_';
		}
		*q = *p;
	}
	*q = '\0';
	
	return escaped;
}

char *
eel_str_capitalize (const char *string)
{
	char *capitalized;

	if (string == NULL) {
		return NULL;
	}

	capitalized = g_strdup (string);

	capitalized[0] = g_ascii_toupper (capitalized[0]);

	return capitalized;
}

/* Note: eel_string_ellipsize_* that use a length in pixels
 * rather than characters can be found in eel_gdk_extensions.h
 * 
 * FIXME bugzilla.eazel.com 5089: 
 * we should coordinate the names of eel_string_ellipsize_*
 * and eel_str_*_truncate so that they match better and reflect
 * their different behavior.
 */
char *
eel_str_middle_truncate (const char *string,
			      guint truncate_length)
{
	char *truncated;
	guint length;
	guint num_left_chars;
	guint num_right_chars;

	const char delimter[] = "...";
	const guint delimter_length = strlen (delimter);
	const guint min_truncate_length = delimter_length + 2;

	if (string == NULL) {
		return NULL;
	}

	/* It doesnt make sense to truncate strings to less than
	 * the size of the delimiter plus 2 characters (one on each
	 * side)
	 */
	if (truncate_length < min_truncate_length) {
		return g_strdup (string);
	}

	length = strlen (string);

	/* Make sure the string is not already small enough. */
	if (length <= truncate_length) {
		return g_strdup (string);
	}

	/* Find the 'middle' where the truncation will occur. */
	num_left_chars = (truncate_length - delimter_length) / 2;
	num_right_chars = truncate_length - num_left_chars - delimter_length + 1;

	truncated = g_new (char, truncate_length + 1);

	strncpy (truncated, string, num_left_chars);
	strncpy (truncated + num_left_chars, delimter, delimter_length);
	strncpy (truncated + num_left_chars + delimter_length, string + length - num_right_chars + 1, num_right_chars);
	
	return truncated;
}

/**
 * eel_str_count_characters:
 * Count the number of 'c' characters that occur in 'string.
 * 
 * @string: The string to be scanned.
 * @c: The char to count.
 * 
 * Return value: @count, the 'c' occurance count.
 */
guint
eel_str_count_characters (const char	*string,
			       char		c)
{
	guint count = 0;

	while (string && *string != '\0') {
		if (*string == c) {
			count++;
		}

		string++;
	}

	return count;
}

char *
eel_str_strip_substring_and_after (const char *string,
					const char *substring)
{
	const char *substring_position;

	g_return_val_if_fail (substring != NULL, g_strdup (string));
	g_return_val_if_fail (substring[0] != '\0', g_strdup (string));

	if (string == NULL) {
		return NULL;
	}

	substring_position = strstr (string, substring);
	if (substring_position == NULL) {
		return g_strdup (string);
	}

	return g_strndup (string,
			  substring_position - string);
}

char *
eel_str_replace_substring (const char *string,
				const char *substring,
				const char *replacement)
{
	int substring_length, replacement_length, result_length, remaining_length;
	const char *p, *substring_position;
	char *result, *result_position;

	g_return_val_if_fail (substring != NULL, g_strdup (string));
	g_return_val_if_fail (substring[0] != '\0', g_strdup (string));

	if (string == NULL) {
		return NULL;
	}

	substring_length = strlen (substring);
	replacement_length = eel_strlen (replacement);

	result_length = strlen (string);
	for (p = string; ; p = substring_position + substring_length) {
		substring_position = strstr (p, substring);
		if (substring_position == NULL) {
			break;
		}
		result_length += replacement_length - substring_length;
	}

	result = g_malloc (result_length + 1);

	result_position = result;
	for (p = string; ; p = substring_position + substring_length) {
		substring_position = strstr (p, substring);
		if (substring_position == NULL) {
			remaining_length = strlen (p);
			memcpy (result_position, p, remaining_length);
			result_position += remaining_length;
			break;
		}
		memcpy (result_position, p, substring_position - p);
		result_position += substring_position - p;
		memcpy (result_position, replacement, replacement_length);
		result_position += replacement_length;
	}
	g_assert (result_position - result == result_length);
	result_position[0] = '\0';

	return result;
}


/* Removes strings enclosed by the '[' and ']' characters.  Strings
   that have unbalanced open and closed brackets will return the
   string itself. */
char *
eel_str_remove_bracketed_text (const char *text)
{
	char *unbracketed_text;
	char *unbracketed_segment, *new_unbracketed_text;
	const char *current_text_location;
	const char *next_open_bracket, *next_close_bracket;
	int bracket_depth;
	

	g_return_val_if_fail (text != NULL, NULL);

	current_text_location = text;
	bracket_depth = 0;
	unbracketed_text = g_strdup ("");
	while (TRUE) {
		next_open_bracket = strchr (current_text_location, '[');
		next_close_bracket = strchr (current_text_location, ']');
		/* No more brackets */
		if (next_open_bracket == NULL &&
		    next_close_bracket == NULL) {
			if (bracket_depth == 0) {
				new_unbracketed_text = g_strconcat (unbracketed_text, 
								    current_text_location, NULL);
				g_free (unbracketed_text);
				return new_unbracketed_text;
			}
			else {
				g_free (unbracketed_text);
				return g_strdup (text);
			}
		}
		/* Close bracket but no open bracket */
		else if (next_open_bracket == NULL) {
			if (bracket_depth == 0) {
				g_free (unbracketed_text);
				return g_strdup (text);
			}
			else {
				current_text_location = next_close_bracket + 1;
				bracket_depth--;
			}
		}
		/* Open bracket but no close bracket */
		else if (next_close_bracket == NULL) {
			g_free (unbracketed_text);
			return g_strdup (text);
		}
		/* Deal with the open bracket, that's first */
		else if (next_open_bracket < next_close_bracket) {
			if (bracket_depth == 0) {
				/* We're out of brackets. Copy until the next bracket */
				unbracketed_segment = g_strndup (current_text_location,
								 next_open_bracket - current_text_location);
				new_unbracketed_text = g_strconcat (unbracketed_text, unbracketed_segment, NULL);
				g_free (unbracketed_text);
				g_free (unbracketed_segment);
				unbracketed_text = new_unbracketed_text;
			}
			current_text_location = next_open_bracket + 1;
			bracket_depth++;
		}
		/* Deal with the close bracket, that's first */
		else {
			if (bracket_depth > 0) {
				bracket_depth--;
				current_text_location = next_close_bracket + 1;
			}
			else {
				g_free (unbracketed_text);
				return g_strdup (text);
			}
		}
	}
	
	     

}

#if !defined (EEL_OMIT_SELF_CHECK)

static int
call_str_to_int (const char *string)
{
	int integer;

	integer = 9999;
	eel_str_to_int (string, &integer);
	return integer;
}

void
eel_self_check_string (void)
{
	int integer;

	EEL_CHECK_INTEGER_RESULT (eel_strlen (NULL), 0);
	EEL_CHECK_INTEGER_RESULT (eel_strlen (""), 0);
	EEL_CHECK_INTEGER_RESULT (eel_strlen ("abc"), 3);

	EEL_CHECK_INTEGER_RESULT (eel_strcmp (NULL, NULL), 0);
	EEL_CHECK_INTEGER_RESULT (eel_strcmp (NULL, ""), 0);
	EEL_CHECK_INTEGER_RESULT (eel_strcmp ("", NULL), 0);
	EEL_CHECK_INTEGER_RESULT (eel_strcmp ("a", "a"), 0);
	EEL_CHECK_INTEGER_RESULT (eel_strcmp ("aaab", "aaab"), 0);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp (NULL, "a") < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("a", NULL) > 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("", "a") < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("a", "") > 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("a", "b") < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("a", "ab") < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("ab", "a") > 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("aaa", "aaab") < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_strcmp ("aaab", "aaa") > 0, TRUE);

	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix (NULL, NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix (NULL, ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("", NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("a", "a"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("aaab", "aaab"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix (NULL, "a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("a", NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("", "a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("a", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("a", "b"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("a", "ab"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("ab", "a"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("aaa", "aaab"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_prefix ("aaab", "aaa"), TRUE);

	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix (NULL, NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix (NULL, ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("", NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("a", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("", "a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("a", "a"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("aaab", "aaab"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix (NULL, "a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("a", NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("", "a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("a", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("a", "b"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("a", "ab"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("ab", "a"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("ab", "b"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("aaa", "baaa"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_has_suffix ("baaa", "aaa"), TRUE);

	EEL_CHECK_STRING_RESULT (eel_str_get_prefix (NULL, NULL), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix (NULL, "foo"), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("foo", NULL), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("", "foo"), "");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("foo", ""), "");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("foo", "foo"), "");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("foo:", ":"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("foo:bar", ":"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_get_prefix ("footle:bar", "tle:"), "foo");	

	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix (NULL, NULL), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix (NULL, "foo"), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("foo", NULL), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("", "foo"), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("foo", ""), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("foo", "foo"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("foo:", ":"), ":");
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("foo:bar", ":"), ":bar");
	EEL_CHECK_STRING_RESULT (eel_str_get_after_prefix ("footle:bar", "tle:"), "tle:bar");	

	EEL_CHECK_STRING_RESULT (eel_str_strip_chr (NULL, '_'), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_strip_chr ("", '_'), "");
	EEL_CHECK_STRING_RESULT (eel_str_strip_chr ("foo", '_'), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_strip_chr ("_foo", '_'), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_strip_chr ("foo_", '_'), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_strip_chr ("_foo__", '_'), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_strip_chr ("_f_o__o_", '_'), "foo");
        
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr (NULL, '_'), NULL);	
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr ("", '_'), "");	
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr ("foo", '_'), "foo");	
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr ("_foo", '_'), "_foo");	
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr ("foo_", '_'), "foo");	
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr ("_foo__", '_'), "_foo");	
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_chr ("_f_o__o_", '_'), "_f_o__o");	

	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str (NULL, NULL), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str (NULL, "bar"), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("bar", NULL), "bar");
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("", ""), "");
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("", "bar"), "");
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("bar", ""), "bar");
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("foo", "bar"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("foo bar", "bar"), "foo ");
	EEL_CHECK_STRING_RESULT (eel_str_strip_trailing_str ("bar", "bar"), "");
	
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores (NULL), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores (""), "");
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("_"), "__");
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo_bar"), "foo__bar");
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo_bar_2"), "foo__bar__2");
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("_foo"), "__foo");
	EEL_CHECK_STRING_RESULT (eel_str_double_underscores ("foo_"), "foo__");

	EEL_CHECK_STRING_RESULT (eel_str_capitalize (NULL), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_capitalize (""), "");
	EEL_CHECK_STRING_RESULT (eel_str_capitalize ("foo"), "Foo");
	EEL_CHECK_STRING_RESULT (eel_str_capitalize ("Foo"), "Foo");

	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 0), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 1), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 3), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 4), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 5), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 6), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("foo", 7), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 0), "a_much_longer_foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 1), "a_much_longer_foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 2), "a_much_longer_foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 3), "a_much_longer_foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 4), "a_much_longer_foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 5), "a...o");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 6), "a...oo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 7), "a_...oo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 8), "a_...foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("a_much_longer_foo", 9), "a_m...foo");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 8), "so...ven");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 8), "so...odd");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 9), "som...ven");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 9), "som...odd");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 10), "som...even");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 10), "som..._odd");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 11), "some...even");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 11), "some..._odd");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 12), "some..._even");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 12), "some...g_odd");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 13), "somet..._even");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 13), "something_odd");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_even", 14), "something_even");
	EEL_CHECK_STRING_RESULT (eel_str_middle_truncate ("something_odd", 13), "something_odd");

	#define TEST_INTEGER_CONVERSION_FUNCTIONS(string, boolean_result, integer_result) \
		EEL_CHECK_BOOLEAN_RESULT (eel_str_to_int (string, &integer), boolean_result); \
		EEL_CHECK_INTEGER_RESULT (call_str_to_int (string), integer_result);

	TEST_INTEGER_CONVERSION_FUNCTIONS (NULL, FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("a", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS (".", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("0", TRUE, 0)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("1", TRUE, 1)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+1", TRUE, 1)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-1", TRUE, -1)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("2147483647", TRUE, 2147483647)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("2147483648", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+2147483647", TRUE, 2147483647)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+2147483648", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-2147483648", TRUE, INT_MIN)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-2147483649", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("1a", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("0.0", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("1e1", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("21474836470", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("+21474836470", FALSE, 9999)
	TEST_INTEGER_CONVERSION_FUNCTIONS ("-21474836480", FALSE, 9999)

	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal (NULL, NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal (NULL, ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal ("", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal ("", NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal ("", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal ("foo", "foo"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_str_is_equal ("foo", "bar"), FALSE);

	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal (NULL, NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal (NULL, ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("", NULL), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("", ""), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("foo", "foo"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("foo", "bar"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("Foo", "foo"), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_istr_is_equal ("foo", "Foo"), TRUE);

	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters (NULL, 'x'), 0);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters ("", 'x'), 0);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters (NULL, '\0'), 0);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters ("", '\0'), 0);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters ("foo", 'x'), 0);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters ("foo", 'f'), 1);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters ("foo", 'o'), 2);
	EEL_CHECK_INTEGER_RESULT (eel_str_count_characters ("xxxx", 'x'), 4);

	EEL_CHECK_STRING_RESULT (eel_str_strip_substring_and_after (NULL, "bar"), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_strip_substring_and_after ("", "bar"), "");
	EEL_CHECK_STRING_RESULT (eel_str_strip_substring_and_after ("foo", "bar"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_strip_substring_and_after ("foo bar", "bar"), "foo ");
	EEL_CHECK_STRING_RESULT (eel_str_strip_substring_and_after ("foo bar xxx", "bar"), "foo ");
	EEL_CHECK_STRING_RESULT (eel_str_strip_substring_and_after ("bar", "bar"), "");

	EEL_CHECK_STRING_RESULT (eel_str_replace_substring (NULL, "foo", NULL), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring (NULL, "foo", "bar"), NULL);
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("bar", "foo", NULL), "bar");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("", "foo", ""), "");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("", "foo", "bar"), "");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("bar", "foo", ""), "bar");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("xxx", "x", "foo"), "foofoofoo");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("fff", "f", "foo"), "foofoofoo");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("foofoofoo", "foo", "f"), "fff");
	EEL_CHECK_STRING_RESULT (eel_str_replace_substring ("foofoofoo", "f", ""), "oooooo");

	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text (""), "");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("[]"), "");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("["), "[");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("]"), "]");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("[[]"), "[[]");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo [bar]"), "foo ");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo[ bar]"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo[ bar] baz"), "foo baz");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo[ [b]ar] baz"), "foo baz");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo[ bar] baz[ bat]"), "foo baz");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo[ bar[ baz] bat]"), "foo");
	EEL_CHECK_STRING_RESULT (eel_str_remove_bracketed_text ("foo[ bar] baz] bat]"), "foo[ bar] baz] bat]");
}

#endif /* !EEL_OMIT_SELF_CHECK */
