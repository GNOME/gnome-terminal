/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-glib-extensions.c - implementation of new functions that conceptually
                                belong in glib. Perhaps some of these will be
                                actually rolled into glib someday.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "eel-glib-extensions.h"

#include "eel-debug.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include <libgnome/gnome-i18n.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>

/* Legal conversion specifiers, as specified in the C standard. */
#define C_STANDARD_STRFTIME_CHARACTERS "aAbBcdHIjmMpSUwWxXyYZ"
#define C_STANDARD_NUMERIC_STRFTIME_CHARACTERS "dHIjmMSUwWyY"

#define SAFE_SHELL_CHARACTERS "-_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

typedef struct {
	GHashTable *hash_table;
	char *display_name;
	gboolean keys_known_to_be_strings;
} HashTableToFree;

static GList *hash_tables_to_free_at_exit;

/* We will need this for eel_unsetenv if there is no unsetenv. */
#if !defined (HAVE_SETENV)
extern char **environ;
#endif

/**
 * eel_setenv:
 * 
 * Adds "*name=*value" to the environment
 *
 * Returns: see "putenv" - 
 * 
 **/
int
eel_setenv (const char *name, const char *value, gboolean overwrite)
{
#if defined (HAVE_SETENV)
	return setenv (name, value, overwrite);
#else
	char *string;
	
	if (! overwrite && g_getenv (name) != NULL) {
		return 0;
	}
	
	/* This results in a leak when you overwrite existing
	 * settings. It would be fairly easy to fix this by keeping
	 * our own parallel array or hash table.
	 */
	string = g_strconcat (name, "=", value, NULL);
	return putenv (string);
#endif
}

/**
 * eel_unsetenv:
 * 
 * Removes *name from the environment
 * 
 **/
void
eel_unsetenv (const char *name)
{
#if defined (HAVE_SETENV)
	unsetenv (name);
#else
	int i, len;

	len = strlen (name);
	
	/* Mess directly with the environ array.
	 * This seems to be the only portable way to do this.
	 */
	for (i = 0; environ[i] != NULL; i++) {
		if (strncmp (environ[i], name, len) == 0
		    && environ[i][len + 1] == '=') {
			break;
		}
	}
	while (environ[i] != NULL) {
		environ[i] = environ[i + 1];
		i++;
	}
#endif
}

/**
 * eel_g_date_new_tm:
 * 
 * Get a new GDate * for the date represented by a tm struct. 
 * The caller is responsible for g_free-ing the result.
 * @time_pieces: Pointer to a tm struct representing the date to be converted.
 * 
 * Returns: Newly allocated date.
 * 
 **/
GDate *
eel_g_date_new_tm (struct tm *time_pieces)
{
	/* tm uses 0-based months; GDate uses 1-based months.
	 * tm_year needs 1900 added to get the full year.
	 */
	return g_date_new_dmy (time_pieces->tm_mday,
			       time_pieces->tm_mon + 1,
			       time_pieces->tm_year + 1900);
}

/**
 * eel_strdup_strftime:
 *
 * Cover for standard date-and-time-formatting routine strftime that returns
 * a newly-allocated string of the correct size. The caller is responsible
 * for g_free-ing the returned string.
 *
 * Besides the buffer management, there are two differences between this
 * and the library strftime:
 *
 *   1) The modifiers "-" and "_" between a "%" and a numeric directive
 *      are defined as for the GNU version of strftime. "-" means "do not
 *      pad the field" and "_" means "pad with spaces instead of zeroes".
 *   2) Non-ANSI extensions to strftime are flagged at runtime with a
 *      warning, so it's easy to notice use of the extensions without
 *      testing with multiple versions of the library.
 *
 * @format: format string to pass to strftime. See strftime documentation
 * for details.
 * @time_pieces: date/time, in struct format.
 * 
 * Return value: Newly allocated string containing the formatted time.
 **/
char *
eel_strdup_strftime (const char *format, struct tm *time_pieces)
{
	GString *string;
	const char *remainder, *percent;
	char code[3], buffer[512];
	char *piece, *result, *converted;
	size_t string_length;
	gboolean strip_leading_zeros, turn_leading_zeros_to_spaces;

	/* Format could be translated, and contain UTF-8 chars,
	 * so convert to locale encoding which strftime uses */
	converted = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	g_return_val_if_fail (converted != NULL, NULL);
	
	string = g_string_new ("");
	remainder = converted;

	/* Walk from % character to % character. */
	for (;;) {
		percent = strchr (remainder, '%');
		if (percent == NULL) {
			g_string_append (string, remainder);
			break;
		}
		g_string_append_len (string, remainder,
				     percent - remainder);

		/* Handle the "%" character. */
		remainder = percent + 1;
		switch (*remainder) {
		case '-':
			strip_leading_zeros = TRUE;
			turn_leading_zeros_to_spaces = FALSE;
			remainder++;
			break;
		case '_':
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = TRUE;
			remainder++;
			break;
		case '%':
			g_string_append_c (string, '%');
			remainder++;
			continue;
		case '\0':
			g_warning ("Trailing %% passed to eel_strdup_strftime");
			g_string_append_c (string, '%');
			continue;
		default:
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = FALSE;
			break;
		}
		
		if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL) {
			g_warning ("eel_strdup_strftime does not support "
				   "non-standard escape code %%%c",
				   *remainder);
		}

		/* Convert code to strftime format. We have a fixed
		 * limit here that each code can expand to a maximum
		 * of 512 bytes, which is probably OK. There's no
		 * limit on the total size of the result string.
		 */
		code[0] = '%';
		code[1] = *remainder;
		code[2] = '\0';
		string_length = strftime (buffer, sizeof (buffer),
					  code, time_pieces);
		if (string_length == 0) {
			/* We could put a warning here, but there's no
			 * way to tell a successful conversion to
			 * empty string from a failure.
			 */
			buffer[0] = '\0';
		}

		/* Strip leading zeros if requested. */
		piece = buffer;
		if (strip_leading_zeros || turn_leading_zeros_to_spaces) {
			if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL) {
				g_warning ("eel_strdup_strftime does not support "
					   "modifier for non-numeric escape code %%%c%c",
					   remainder[-1],
					   *remainder);
			}
			if (*piece == '0') {
				do {
					piece++;
				} while (*piece == '0');
				if (!g_ascii_isdigit (*piece)) {
				    piece--;
				}
			}
			if (turn_leading_zeros_to_spaces) {
				memset (buffer, ' ', piece - buffer);
				piece = buffer;
			}
		}
		remainder++;

		/* Add this piece. */
		g_string_append (string, piece);
	}
	
	/* Convert the string back into utf-8. */
	result = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);

	g_string_free (string, TRUE);
	g_free (converted);

	return result;
}

/**
 * eel_g_list_exactly_one_item
 *
 * Like g_list_length (list) == 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has exactly one item.
 **/
gboolean
eel_g_list_exactly_one_item (GList *list)
{
	return list != NULL && list->next == NULL;
}

/**
 * eel_g_list_more_than_one_item
 *
 * Like g_list_length (list) > 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has more than one item.
 **/
gboolean
eel_g_list_more_than_one_item (GList *list)
{
	return list != NULL && list->next != NULL;
}

/**
 * eel_g_list_equal
 *
 * Compares two lists to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists are the same length with the same elements.
 **/
gboolean
eel_g_list_equal (GList *list_a, GList *list_b)
{
	GList *p, *q;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		if (p->data != q->data) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

/**
 * eel_g_str_list_equal
 *
 * Compares two lists of C strings to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists contain the same strings.
 **/
gboolean
eel_g_str_list_equal (GList *list_a, GList *list_b)
{
	GList *p, *q;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		if (eel_strcmp (p->data, q->data) != 0) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

/**
 * eel_g_str_list_copy
 *
 * @list: List of strings and/or NULLs to copy.
 * Return value: Deep copy of @list.
 **/
GList *
eel_g_str_list_copy (GList *list)
{
	GList *node, *result;

	result = NULL;
	
	for (node = g_list_last (list); node != NULL; node = node->prev) {
		result = g_list_prepend (result, g_strdup (node->data));
	}
	return result;
}

/**
 * eel_g_str_list_alphabetize
 *
 * Sort a list of strings using locale-sensitive rules.
 *
 * @list: List of strings and/or NULLs.
 * 
 * Return value: @list, sorted.
 **/
GList *
eel_g_str_list_alphabetize (GList *list)
{
	return g_list_sort (list, (GCompareFunc) eel_strcoll);
}

/**
 * eel_g_list_free_deep_custom
 *
 * Frees the elements of a list and then the list, using a custom free function.
 *
 * @list: List of elements that can be freed with the provided free function.
 * @element_free_func: function to call with the data pointer and user_data to free it.
 * @user_data: User data to pass to element_free_func
 **/
void
eel_g_list_free_deep_custom (GList *list, GFunc element_free_func, gpointer user_data)
{
	g_list_foreach (list, element_free_func, user_data);
	g_list_free (list);
}

/**
 * eel_g_list_free_deep
 *
 * Frees the elements of a list and then the list.
 * @list: List of elements that can be freed with g_free.
 **/
void
eel_g_list_free_deep (GList *list)
{
	eel_g_list_free_deep_custom (list, (GFunc) g_free, NULL);
}

/**
 * eel_g_list_free_deep_custom
 *
 * Frees the elements of a list and then the list, using a custom free function.
 *
 * @list: List of elements that can be freed with the provided free function.
 * @element_free_func: function to call with the data pointer and user_data to free it.
 * @user_data: User data to pass to element_free_func
 **/
void
eel_g_slist_free_deep_custom (GSList *list, GFunc element_free_func, gpointer user_data)
{
	g_slist_foreach (list, element_free_func, user_data);
	g_slist_free (list);
}

/**
 * eel_g_slist_free_deep
 *
 * Frees the elements of a list and then the list.
 * @list: List of elements that can be freed with g_free.
 **/
void
eel_g_slist_free_deep (GSList *list)
{
	eel_g_slist_free_deep_custom (list, (GFunc) g_free, NULL);
}


/**
 * eel_g_strv_find
 * 
 * Get index of string in array of strings.
 * 
 * @strv: NULL-terminated array of strings.
 * @find_me: string to search for.
 * 
 * Return value: index of array entry in @strv that
 * matches @find_me, or -1 if no matching entry.
 */
int
eel_g_strv_find (char **strv, const char *find_me)
{
	int index;

	g_return_val_if_fail (find_me != NULL, -1);
	
	for (index = 0; strv[index] != NULL; ++index) {
		if (strcmp (strv[index], find_me) == 0) {
			return index;
		}
	}

	return -1;
}

static int
compare_pointers (gconstpointer pointer_1, gconstpointer pointer_2)
{
	if ((const char *) pointer_1 < (const char *) pointer_2) {
		return -1;
	}
	if ((const char *) pointer_1 > (const char *) pointer_2) {
		return +1;
	}
	return 0;
}

gboolean
eel_g_lists_sort_and_check_for_intersection (GList **list_1,
					     GList **list_2) 

{
	GList *node_1, *node_2;
	int compare_result;
	
	*list_1 = g_list_sort (*list_1, compare_pointers);
	*list_2 = g_list_sort (*list_2, compare_pointers);

	node_1 = *list_1;
	node_2 = *list_2;

	while (node_1 != NULL && node_2 != NULL) {
		compare_result = compare_pointers (node_1->data, node_2->data);
		if (compare_result == 0) {
			return TRUE;
		}
		if (compare_result <= 0) {
			node_1 = node_1->next;
		}
		if (compare_result >= 0) {
			node_2 = node_2->next;
		}
	}

	return FALSE;
}


/**
 * eel_g_list_partition
 * 
 * Parition a list into two parts depending on whether the data
 * elements satisfy a provided predicate. Order is preserved in both
 * of the resulting lists, and the original list is consumed. A list
 * of the items that satisfy the predicate is returned, and the list
 * of items not satisfying the predicate is returned via the failed
 * out argument.
 * 
 * @list: List to partition.
 * @predicate: Function to call on each element.
 * @user_data: Data to pass to function.  
 * @failed: The GList * variable pointed to by this argument will be
 * set to the list of elements for which the predicate returned
 * false. */

GList *
eel_g_list_partition (GList *list,
			   EelPredicateFunction  predicate,
			   gpointer user_data,
			   GList **failed)
{
	GList *predicate_true;
	GList *predicate_false;
	GList *reverse;
	GList *p;
	GList *next;

	predicate_true = NULL;
	predicate_false = NULL;

	reverse = g_list_reverse (list);

	for (p = reverse; p != NULL; p = next) {
		next = p->next;
		
		if (next != NULL) {
			next->prev = NULL;
		}

		if (predicate (p->data, user_data)) {
			p->next = predicate_true;
 			if (predicate_true != NULL) {
				predicate_true->prev = p;
			}
			predicate_true = p;
		} else {
			p->next = predicate_false;
 			if (predicate_false != NULL) {
				predicate_false->prev = p;
			}
			predicate_false = p;
		}
	}

	*failed = predicate_false;
	return predicate_true;
}



/**
 * eel_g_ptr_array_new_from_list
 * 
 * Copies (shallow) a list of pointers into an array of pointers.
 * 
 * @list: List to copy.
 * 
 * Return value: GPtrArray containing a copy of all the pointers
 * from @list
 */
GPtrArray *
eel_g_ptr_array_new_from_list (GList *list)
{
	GPtrArray *array;
	int size;
	int index;
	GList *p;

	size = g_list_length (list);
	array = g_ptr_array_sized_new (size);

	for (p = list, index = 0; p != NULL; p = p->next, index++) {
		g_ptr_array_index (array, index) = p->data;
	}

	return array;
}

/**
 * eel_g_ptr_array_search
 * 
 * Does a binary search through @array looking for an item
 * that matches a predicate consisting of a @search_function and
 * @context. May be used to find an insertion point 
 * 
 * @array: pointer array to search
 * @search_function: search function called on elements
 * @context: search context passed to the @search_function containing
 * the key or other data we are matching
 * @match_only: if TRUE, only returns a match if the match is exact,
 * if FALSE, returns either an index of a match or an index of the
 * slot a new item should be inserted in
 * 
 * Return value: index of the match or -1 if not found
 */
int
eel_g_ptr_array_search (GPtrArray *array, 
			     EelSearchFunction search_function,
			     void *context,
			     gboolean match_only)
{
	int r, l;
	int resulting_index;
	int compare_result;
	void *item;

	r = array->len - 1;
	item = NULL;
	resulting_index = 0;
	compare_result = 0;
	
	for (l = 0; l <= r; ) {
		resulting_index = (l + r) / 2;

		item = g_ptr_array_index (array, resulting_index);

		compare_result = (search_function) (item, context);
		
		if (compare_result > 0) {
			r = resulting_index - 1;
		} else if (compare_result < 0) {
			l = resulting_index + 1;
		} else {
			return resulting_index;
		}
	}

	if (compare_result < 0) {
		resulting_index++;
	}

	if (match_only && compare_result != 0) {
		return -1;
	}

	return resulting_index;
}

/**
 * eel_get_system_time
 * 
 * Return value: number of microseconds since the machine was turned on
 */
gint64
eel_get_system_time (void)
{
	struct timeval tmp;

	gettimeofday (&tmp, NULL);
	return (gint64)tmp.tv_usec + (gint64)tmp.tv_sec * G_GINT64_CONSTANT (1000000);
}

static void
print_key_string (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (callback_data == NULL);

	g_print ("--> %s\n", (char *) key);
}

static void
free_hash_tables_at_exit (void)
{
	GList *p;
	HashTableToFree *hash_table_to_free;
	guint size;

	for (p = hash_tables_to_free_at_exit; p != NULL; p = p->next) {
		hash_table_to_free = p->data;

		size = g_hash_table_size (hash_table_to_free->hash_table);
		if (size != 0) {
			if (hash_table_to_free->keys_known_to_be_strings) {
				g_print ("\n--- Hash table keys for warning below:\n");
				g_hash_table_foreach (hash_table_to_free->hash_table,
						      print_key_string,
						      NULL);
			}
			g_warning ("\"%s\" hash table still has %u element%s at quit time%s",
				   hash_table_to_free->display_name, size,
				   size == 1 ? "" : "s",
				   hash_table_to_free->keys_known_to_be_strings
				   ? " (keys above)" : "");
		}

		g_hash_table_destroy (hash_table_to_free->hash_table);
		g_free (hash_table_to_free->display_name);
		g_free (hash_table_to_free);
	}
	g_list_free (hash_tables_to_free_at_exit);
	hash_tables_to_free_at_exit = NULL;
}

GHashTable *
eel_g_hash_table_new_free_at_exit (GHashFunc hash_func,
				   GCompareFunc key_compare_func,
				   const char *display_name)
{
	GHashTable *hash_table;
	HashTableToFree *hash_table_to_free;

	/* FIXME: We can take out the NAUTILUS_DEBUG check once we
	 * have fixed more of the leaks. For now, it's a bit too noisy
	 * for the general public.
	 */
	if (hash_tables_to_free_at_exit == NULL) {
		eel_debug_call_at_shutdown (free_hash_tables_at_exit);
	}

	hash_table = g_hash_table_new (hash_func, key_compare_func);

	hash_table_to_free = g_new (HashTableToFree, 1);
	hash_table_to_free->hash_table = hash_table;
	hash_table_to_free->display_name = g_strdup (display_name);
	hash_table_to_free->keys_known_to_be_strings =
		hash_func == g_str_hash;

	hash_tables_to_free_at_exit = g_list_prepend
		(hash_tables_to_free_at_exit, hash_table_to_free);

	return hash_table;
}

typedef struct {
	GList *keys;
	GList *values;
} FlattenedHashTable;

static void
flatten_hash_table_element (gpointer key, gpointer value, gpointer callback_data)
{
	FlattenedHashTable *flattened_table;

	flattened_table = callback_data;
	flattened_table->keys = g_list_prepend
		(flattened_table->keys, key);
	flattened_table->values = g_list_prepend
		(flattened_table->values, value);
}

void
eel_g_hash_table_safe_for_each (GHashTable *hash_table,
				GHFunc callback,
				gpointer callback_data)
{
	FlattenedHashTable flattened;
	GList *p, *q;

	flattened.keys = NULL;
	flattened.values = NULL;

	g_hash_table_foreach (hash_table,
			      flatten_hash_table_element,
			      &flattened);

	for (p = flattened.keys, q = flattened.values;
	     p != NULL;
	     p = p->next, q = q->next) {
		(* callback) (p->data, q->data, callback_data);
	}

	g_list_free (flattened.keys);
	g_list_free (flattened.values);
}

int
eel_round (double d)
{
	double val;

	val = floor (d + .5);

	/* The tests are needed because the result of floating-point to integral
	 * conversion is undefined if the floating point value is not representable
	 * in the new type. E.g. the magnititude is too large or a negative
	 * floating-point value being converted to an unsigned.
	 */
	g_return_val_if_fail (val <= INT_MAX, INT_MAX);
	g_return_val_if_fail (val >= INT_MIN, INT_MIN);

	return val;
}

GList *
eel_g_list_from_g_slist (GSList *slist)
{
	GList *list;
	GSList *node;

	list = NULL;
	for (node = slist; node != NULL; node = node->next) {
		list = g_list_prepend (list, node->data);
	}
	return g_list_reverse (list);
}

GSList *
eel_g_slist_from_g_list (GList *list)
{
	GSList *slist;
	GList *node;

	slist = NULL;
	for (node = list; node != NULL; node = node->next) {
		slist = g_slist_prepend (slist, node->data);
	}
	return g_slist_reverse (slist);
}

/**
 * eel_dumb_down_for_multi_byte_locale_hack
 * 
 * Return value: A boolean value indicating whether the current locale
 *               is multi byte so that we can dumb down some operations.
 *               to work on those locales.  This is a temporary workaround
 *               with will be properly fixed in a future version of 
 *               eel.
 *
 */
gboolean
eel_dumb_down_for_multi_byte_locale_hack (void)
{
	static gboolean is_multi_byte_locale = FALSE;
	static gboolean is_multi_byte_locale_known = FALSE;
	guint i;
	const char *variable = NULL;

	/*
	 * List of environment variables that effect the locale.
	 * This list was provided by John Harper (out of Sawfish)
	 * where he uses it for similar purposes.
	 */
	static const char *locale_variables[] ={
		"LANGUAGE",
		"LC_ALL",
		"LC_MESSAGES",
		"LANG",
		"GDM_LANG"
	};

	/*
	 * List of locales (prefixes) known to be multi byte.
	 */
	static const char *multi_byte_locales[] ={
		"ja",
		"ko",
		"zh"
	};

	/* Find out if the locale is multi byte only once */
	if (is_multi_byte_locale_known) {
		return is_multi_byte_locale;
	}
	is_multi_byte_locale_known = TRUE;

	/* Find the first language variable that is set */
	for (i = 0; i < G_N_ELEMENTS (locale_variables) && variable == NULL; i++) {
		variable = g_getenv (locale_variables[i]);
	}

	/* If a language variable was found, check it agains the known multi byte locales */
	if (variable != NULL) {
		for (i = 0; i < G_N_ELEMENTS (multi_byte_locales); i++) {
			if (eel_istr_has_prefix (variable, multi_byte_locales[i])) {
				is_multi_byte_locale = TRUE;
			}
		}
	}

	return is_multi_byte_locale;
}

/* Return the operating system name: Linux, Solaris, etc. */
char *
eel_get_operating_system_name (void)
{
	struct utsname buffer;

	if (uname (&buffer) != -1) {
		/* Check for special sysnames for which there is 
		 * more accepted names.
		 */
		if (eel_str_is_equal (buffer.sysname, "SunOS")) {
			return g_strdup ("Solaris");
		}

		return g_strdup (buffer.sysname);
	}

	return g_strdup ("Unix");
}

int
eel_compare_integer (gconstpointer a,
			  gconstpointer b)
{
	int int_a;
	int int_b;

	int_a = GPOINTER_TO_INT (a);
	int_b = GPOINTER_TO_INT (b);

	if (int_a == int_b) {
		return 0;
	}

	return int_a < int_b ? -1 : 1;
}

#if !defined (EEL_OMIT_SELF_CHECK)

static void 
check_tm_to_g_date (time_t time)
{
	struct tm *before_conversion;
	struct tm after_conversion;
	GDate *date;

	before_conversion = localtime (&time);
	date = eel_g_date_new_tm (before_conversion);

	g_date_to_struct_tm (date, &after_conversion);

	g_date_free (date);

	EEL_CHECK_INTEGER_RESULT (after_conversion.tm_mday,
				       before_conversion->tm_mday);
	EEL_CHECK_INTEGER_RESULT (after_conversion.tm_mon,
				       before_conversion->tm_mon);
	EEL_CHECK_INTEGER_RESULT (after_conversion.tm_year,
				       before_conversion->tm_year);
}

static gboolean
eel_test_predicate (gpointer data,
		    gpointer callback_data)
{
	return g_ascii_strcasecmp (data, callback_data) <= 0;
}

static char *
test_strftime (const char *format,
	       int year,
	       int month,
	       int day,
	       int hour,
	       int minute,
	       int second)
{
	struct tm time_pieces;

	time_pieces.tm_sec = second;
	time_pieces.tm_min = minute;
	time_pieces.tm_hour = hour;
	time_pieces.tm_mday = day;
	time_pieces.tm_mon = month - 1;
	time_pieces.tm_year = year - 1900;
	time_pieces.tm_isdst = -1;
	mktime (&time_pieces);

	return eel_strdup_strftime (format, &time_pieces);
}

void
eel_self_check_glib_extensions (void)
{
	char **strv;
	GList *compare_list_1;
	GList *compare_list_2;
	GList *compare_list_3;
	GList *compare_list_4;
	GList *compare_list_5;
	gint64 time1, time2;
	GList *list_to_partition;
	GList *expected_passed;
	GList *expected_failed;
	GList *actual_passed;
	GList *actual_failed;
	char *huge_string;
	
	check_tm_to_g_date (0);			/* lower limit */
	check_tm_to_g_date ((time_t) -1);	/* upper limit */
	check_tm_to_g_date (time (NULL));	/* current time */

	strv = g_strsplit ("zero|one|two|three|four", "|", 0);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "zero"), 0);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "one"), 1);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "four"), 4);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "five"), -1);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, ""), -1);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "o"), -1);
	g_strfreev (strv);

	/* eel_get_system_time */
	time1 = eel_get_system_time ();
	time2 = eel_get_system_time ();
	EEL_CHECK_BOOLEAN_RESULT (time1 - time2 > -1000, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (time1 - time2 <= 0, TRUE);

	/* eel_g_str_list_equal */

	/* We g_strdup because identical string constants can be shared. */

	compare_list_1 = NULL;
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("Apple"));
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("zebra"));
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("!@#!@$#@$!"));

	compare_list_2 = NULL;
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("Apple"));
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("zebra"));
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("!@#!@$#@$!"));

	compare_list_3 = NULL;
	compare_list_3 = g_list_append (compare_list_3, g_strdup ("Apple"));
	compare_list_3 = g_list_append (compare_list_3, g_strdup ("zebra"));

	compare_list_4 = NULL;
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("Apple"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("zebra"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("!@#!@$#@$!"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("foobar"));

	compare_list_5 = NULL;
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("Apple"));
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("zzzzzebraaaaaa"));
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("!@#!@$#@$!"));

	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_2), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_3), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_4), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_5), FALSE);

	eel_g_list_free_deep (compare_list_1);
	eel_g_list_free_deep (compare_list_2);
	eel_g_list_free_deep (compare_list_3);
	eel_g_list_free_deep (compare_list_4);
	eel_g_list_free_deep (compare_list_5);

	/* eel_g_list_partition */

	list_to_partition = NULL;
	list_to_partition = g_list_append (list_to_partition, "Cadillac");
	list_to_partition = g_list_append (list_to_partition, "Pontiac");
	list_to_partition = g_list_append (list_to_partition, "Ford");
	list_to_partition = g_list_append (list_to_partition, "Range Rover");
	
	expected_passed = NULL;
	expected_passed = g_list_append (expected_passed, "Cadillac");
	expected_passed = g_list_append (expected_passed, "Ford");
	
	expected_failed = NULL;
	expected_failed = g_list_append (expected_failed, "Pontiac");
	expected_failed = g_list_append (expected_failed, "Range Rover");
	
	actual_passed = eel_g_list_partition (list_to_partition, 
						   eel_test_predicate,
						   "m",
						   &actual_failed);
	
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (expected_passed, actual_passed), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (expected_failed, actual_failed), TRUE);
	
	/* Don't free "list_to_partition", since it is consumed
	 * by eel_g_list_partition.
	 */
	
	g_list_free (expected_passed);
	g_list_free (actual_passed);
	g_list_free (expected_failed);
	g_list_free (actual_failed);

	/* eel_strdup_strftime */
	huge_string = g_new (char, 10000+1);
	memset (huge_string, 'a', 10000);
	huge_string[10000] = '\0';

	EEL_CHECK_STRING_RESULT (test_strftime ("", 2000, 1, 1, 0, 0, 0), "");
	EEL_CHECK_STRING_RESULT (test_strftime (huge_string, 2000, 1, 1, 0, 0, 0), huge_string);
	EEL_CHECK_STRING_RESULT (test_strftime ("%%", 2000, 1, 1, 1, 0, 0), "%");
	EEL_CHECK_STRING_RESULT (test_strftime ("%%%%", 2000, 1, 1, 1, 0, 0), "%%");
	/* localizers: These strings are part of the strftime
	 * self-check code and must be changed to match what strtfime
	 * yields -- usually just omitting the AM part is all that's
	 * needed.
	 */
	EEL_CHECK_STRING_RESULT (test_strftime ("%m/%d/%y, %I:%M %p", 2000, 1, 1, 1, 0, 0), _("01/01/00, 01:00 AM"));
	EEL_CHECK_STRING_RESULT (test_strftime ("%-m/%-d/%y, %-I:%M %p", 2000, 1, 1, 1, 0, 0), _("1/1/00, 1:00 AM"));
	EEL_CHECK_STRING_RESULT (test_strftime ("%_m/%_d/%y, %_I:%M %p", 2000, 1, 1, 1, 0, 0), _(" 1/ 1/00,  1:00 AM"));

	g_free (huge_string);

	/* eel_shell_quote */
	EEL_CHECK_STRING_RESULT (g_shell_quote (""), "''");
	EEL_CHECK_STRING_RESULT (g_shell_quote ("a"), "'a'");
	EEL_CHECK_STRING_RESULT (g_shell_quote ("("), "'('");
	EEL_CHECK_STRING_RESULT (g_shell_quote ("'"), "''\\'''");
	EEL_CHECK_STRING_RESULT (g_shell_quote ("'a"), "''\\''a'");
	EEL_CHECK_STRING_RESULT (g_shell_quote ("a'"), "'a'\\'''");
	EEL_CHECK_STRING_RESULT (g_shell_quote ("a'a"), "'a'\\''a'");

	/* eel_compare_integer */
	EEL_CHECK_INTEGER_RESULT (eel_compare_integer (GINT_TO_POINTER (0), GINT_TO_POINTER (0)), 0);
	EEL_CHECK_INTEGER_RESULT (eel_compare_integer (GINT_TO_POINTER (0), GINT_TO_POINTER (1)), -1);
	EEL_CHECK_INTEGER_RESULT (eel_compare_integer (GINT_TO_POINTER (1), GINT_TO_POINTER (0)), 1);
	EEL_CHECK_INTEGER_RESULT (eel_compare_integer (GINT_TO_POINTER (-1), GINT_TO_POINTER (0)), -1);
	EEL_CHECK_INTEGER_RESULT (eel_compare_integer (GINT_TO_POINTER (0), GINT_TO_POINTER (-1)), 1);
	EEL_CHECK_INTEGER_RESULT (eel_compare_integer (GINT_TO_POINTER (-1), GINT_TO_POINTER (-1)), 0);

#ifdef __linux__
	EEL_CHECK_STRING_RESULT (eel_get_operating_system_name (), "Linux");
#endif
}

#endif /* !EEL_OMIT_SELF_CHECK */
