/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-string-list.h: A collection of strings.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "eel-string-list.h"

#include <string.h>
#include "eel-glib-extensions.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"

static gboolean suppress_out_of_bounds_warning;

struct EelStringList
{
	GSList *strings;
	GCompareFunc compare_function;
};

static gboolean str_is_equal (const char *a,
			      const char *b,
			      gboolean    case_sensitive);

/**
 * eel_string_list_new:
 *
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly constructed string list.
 */
EelStringList *
eel_string_list_new (gboolean case_sensitive)
{
	EelStringList * string_list;

	string_list = g_new (EelStringList, 1);

	if (!case_sensitive) {
	}

	string_list->strings = NULL;
	string_list->compare_function = case_sensitive
		? eel_strcmp_compare_func
		: eel_strcasecmp_compare_func;
	/* FIXME: If these lists are seen by users, we want to use
	 * strcoll, not strcasecmp.
	 */

	return string_list;
}

/**
 * eel_string_list_new_from_string:
 *
 * @other_or_null: A EelStringList or NULL
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly constructed string list with one entry 'string'.
 */
EelStringList *
eel_string_list_new_from_string (const char *string,
				 gboolean case_sensitive)
{
	EelStringList * string_list;

	g_return_val_if_fail (string != NULL, NULL);

	string_list = eel_string_list_new (case_sensitive);

	eel_string_list_insert (string_list, string);

	return string_list;
}

/**
 * eel_string_list_copy:
 *
 * @string_list: A EelStringList or NULL
 *
 * Return value: A newly allocated string list that is equal to @string_list.
 */
EelStringList *
eel_string_list_copy (const EelStringList *string_list)
{
	EelStringList *copy;

	if (string_list == NULL) {
		return NULL;
	}

	copy = eel_string_list_new (eel_string_list_is_case_sensitive (string_list));
	eel_string_list_assign_from_string_list (copy, string_list);
	return copy;
}

/**
 * eel_string_list_new_from_string_array:
 *
 * @string_array: A NULL terminated string srray.
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly allocated string list populated with the contents of the
 * NULL terminated @string_array.
 */
EelStringList *
eel_string_list_new_from_string_array (const char * const string_array[],
				       gboolean case_sensitive)
{
	EelStringList *string_list;

	if (string_array == NULL) {
		return NULL;
	}

	string_list = eel_string_list_new (case_sensitive);

	eel_string_list_assign_from_string_array (string_list, string_array);

	return string_list;
}

/**
 * eel_string_list_assign_from_string_array:
 *
 * @string_list: A EelStringList.
 * @string_array: A NULL terminated string srray.
 *
 * Populate a string list with the contents of a NULL terminated string array.
 */
void
eel_string_list_assign_from_string_array (EelStringList *string_list,
					  const char * const string_array[])
{
	guint i;
	
	g_return_if_fail (string_list != NULL);

	eel_string_list_clear (string_list);

	if (string_array == NULL) {
		return;
	}

	for (i = 0; string_array[i] != NULL; i++) {
		eel_string_list_insert (string_list, string_array[i]);
	}
}

/**
 * eel_string_list_new_from_g_slist:
 *
 * @slist: A GSList of strings or NULL
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly allocated string with the same contents as @slist.
 */
EelStringList *
eel_string_list_new_from_g_slist (const GSList *gslist,
				 gboolean case_sensitive)
{
	EelStringList *string_list;
	const GSList *node;

	string_list = eel_string_list_new (case_sensitive);
	for (node = gslist; node != NULL; node = node->next) {
		eel_string_list_insert (string_list, node->data);
	}
	
	return string_list;
}

/**
 * eel_string_list_new_from_g_list:
 *
 * @slist: A GList of strings or NULL
 * @case_sensitive: Flag indicating whether the new string list is case sensitive.
 *
 * Return value: A newly allocated string with the same contents as @slist.
 */
EelStringList *
eel_string_list_new_from_g_list (const GList *glist,
				 gboolean case_sensitive)
{
	EelStringList *string_list;
	const GList *node;
	
	string_list = eel_string_list_new (case_sensitive);
	for (node = glist; node != NULL; node = node->next) {
		eel_string_list_insert (string_list, node->data);
	}
	
	return string_list;
}

/* Construct a string list from tokens delimited by the given string and delimiter */
EelStringList *
eel_string_list_new_from_tokens (const char *string,
				 const char *delimiter,
				 gboolean case_sensitive)
{
	EelStringList *string_list;
	char  **string_array;
	int i;

	g_return_val_if_fail (delimiter != NULL, NULL);

	string_list = eel_string_list_new (case_sensitive);

	if (string != NULL) {
		string_array = g_strsplit (string, delimiter, -1);
		if (string_array) {
			for (i = 0; string_array[i]; i++) {
				eel_string_list_insert (string_list, string_array[i]);
			}
			
			g_strfreev (string_array);
		}
	}

	return string_list;
}

void
eel_string_list_assign_from_string_list (EelStringList *string_list,
					 const EelStringList *other_or_null)
{
	const GSList *node;

	g_return_if_fail (string_list != NULL);

	eel_string_list_clear (string_list);

	if (other_or_null == NULL) {
		return;
	}

	for (node = other_or_null->strings; node != NULL; node = node->next) {
		eel_string_list_insert (string_list, node->data);
	}
}

void
eel_string_list_free (EelStringList *string_list_or_null)
{
	if (string_list_or_null == NULL) {
		return;
	}
	
	eel_string_list_clear (string_list_or_null);
	g_free (string_list_or_null);
}

void
eel_string_list_insert (EelStringList *string_list,
			const char *string)
{
	g_return_if_fail (string_list != NULL);
	g_return_if_fail (string != NULL);

	string_list->strings = g_slist_append (string_list->strings,
					       g_strdup (string));
}

void
eel_string_list_prepend (EelStringList *string_list,
			 const char *string)
{
	g_return_if_fail (string_list != NULL);
	g_return_if_fail (string != NULL);

	string_list->strings = g_slist_prepend (string_list->strings,
						g_strdup (string));
}

void
eel_string_list_insert_string_list (EelStringList *string_list,
				    const EelStringList *other_string_list)
{
	const GSList *node;

	g_return_if_fail (string_list != NULL);

	if (other_string_list == NULL) {
		return;
	}

	for (node = other_string_list->strings; node; node = node->next) {
		eel_string_list_insert (string_list, node->data);
	}
}

char *
eel_string_list_nth (const EelStringList *string_list,
		     guint n)
{
	const char *nth_string;

	g_return_val_if_fail (string_list != NULL, NULL);

	if (n  < g_slist_length (string_list->strings)) {
		nth_string = g_slist_nth_data (string_list->strings, n);
		g_return_val_if_fail (nth_string != NULL, NULL);
		
		return g_strdup (nth_string);
	} else if (!suppress_out_of_bounds_warning) {
		g_warning ("eel_string_list_nth (n = %d) is out of bounds.", n);
	}
	
	return NULL;
}

/**
 * eel_string_list_nth_as_integer
 *
 * @string_list: A EelStringList
 * @n: Index of string to convert.
 * @integer_result: Where to store the conversion result.
 *
 * Convert the nth string to an integer and store the result in &integer_result.
 *
 * Return value: Returns TRUE if the string to integer conversion was successful,
 * FALSE otherwise.
 */
gboolean
eel_string_list_nth_as_integer (const EelStringList *string_list,
				guint n,
				int *integer_result)
{
	const char *string;

	g_return_val_if_fail (string_list != NULL, FALSE);
	g_return_val_if_fail (integer_result != NULL, FALSE);

	if (n >= g_slist_length (string_list->strings)) {
		if (!suppress_out_of_bounds_warning) {
			g_warning ("(n = %d) is out of bounds.", n);
		}
		return FALSE;
	}

	string = g_slist_nth_data (string_list->strings, n);
	return eel_str_to_int (string, integer_result);
}

/**
 * eel_string_list_modify_nth
 *
 * @string_list: A EelStringList
 * @n: Index of string to modify.
 * @string: New value for the string.
 *
 * Modify the nth value of a string in the collection.
 */
void
eel_string_list_modify_nth (EelStringList *string_list,
			    guint n,
			    const char *string)
{
	GSList *nth;

	g_return_if_fail (string_list != NULL);
	g_return_if_fail (string != NULL);

	if (n >= g_slist_length (string_list->strings)) {
		if (!suppress_out_of_bounds_warning) {
			g_warning ("eel_string_list_nth (n = %d) is out of bounds.", n);
		}

		return;
	}

	nth = g_slist_nth (string_list->strings, n);
	g_assert (nth != NULL);

	g_free (nth->data);
	nth->data = g_strdup (string);
}

/**
 * eel_string_list_remove_nth
 *
 * @string_list: A EelStringList
 * @n: Index of string to modify.
 *
 * Remove the nth string in the collection.
 */
void
eel_string_list_remove_nth (EelStringList *string_list,
			    guint n)
{
	GSList* nth;

	g_return_if_fail (string_list != NULL);

	if (n >= g_slist_length (string_list->strings)) {
		if (!suppress_out_of_bounds_warning) {
			g_warning ("eel_string_list_nth (n = %d) is out of bounds.", n);
		}

		return;
	}

	nth = g_slist_nth (string_list->strings, n);
	g_assert (nth != NULL);
	g_free (nth->data);
	string_list->strings = g_slist_remove_link (string_list->strings, nth);
}

gboolean
eel_string_list_contains (const EelStringList *string_list,
			  const char *string)
{
	const GSList *node;

	if (string_list == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (string != NULL, FALSE);

	node = g_slist_find_custom (string_list->strings,
				    (gpointer) string,
				    string_list->compare_function);

	return node != NULL;
}

/**
 * eel_string_list_get_longest_string:
 *
 * @string_list: A EelStringList
 * @test_function: Function to use for testing the strings.
 * @callback_data: Data to pass to test function.
 *
 * Return value: Returns the first string in the collection for 
 * which the test function returns TRUE.  If the no string matches, the 
 * result is NULL.
 */
char *
eel_string_list_find_by_function (const EelStringList *string_list,
				  EelStringListTestFunction test_function,
				  gpointer callback_data)
{
	const GSList *node;

	if (string_list == NULL) {
		return NULL;
	}

	g_return_val_if_fail (test_function != NULL, FALSE);
	
	for (node = string_list->strings; node; node = node->next) {
		if ((* test_function) (string_list, node->data, callback_data)) {
			return g_strdup (node->data);
		}
	}

	return NULL;
}

guint
eel_string_list_get_length (const EelStringList *string_list)
{
	if (string_list == NULL) {
		return 0;
	}

	return g_slist_length (string_list->strings);
}

void
eel_string_list_clear (EelStringList *string_list)
{
	g_return_if_fail (string_list != NULL);

	eel_g_slist_free_deep (string_list->strings);
	string_list->strings = NULL;
}

gboolean
eel_string_list_equals (const EelStringList *a,
			const EelStringList *b)
{
	const GSList *a_node;
	const GSList *b_node;
	gboolean case_sensitive;

	if (a == NULL && b == NULL) {
		return TRUE;
	}

	if (a == NULL || b == NULL) {
		return FALSE;
	}

	if (eel_string_list_get_length (a) != eel_string_list_get_length (b)) {
		return FALSE;
	}
	
	case_sensitive = eel_string_list_is_case_sensitive (a)
		&& eel_string_list_is_case_sensitive (b);

	for (a_node = a->strings, b_node = b->strings; 
	     a_node != NULL && b_node != NULL;
	     a_node = a_node->next, b_node = b_node->next) {
		g_assert (a_node->data != NULL);
		g_assert (b_node->data != NULL);
		if (!str_is_equal (a_node->data, b_node->data, case_sensitive)) {
			return FALSE;
		}
	}
	
	return TRUE;
}

/**
 * eel_string_list_as_g_slist:
 *
 * @string_list: A EelStringList
 *
 * Return value: A GSList of strings that must deep free the result with
 * eel_g_slist_free_deep()
 */
GSList *
eel_string_list_as_g_slist (const EelStringList *string_list)
{
	guint i;
	GSList *gslist;
	
	if (string_list == NULL) {
		return NULL;
	}

	gslist = NULL;
	for (i = 0; i < eel_string_list_get_length (string_list); i++) {
		gslist = g_slist_append (gslist, eel_string_list_nth (string_list, i));
	}

	return gslist;
}

/**
 * eel_string_list_get_index_for_string:
 *
 * @string_list: A EelStringList
 * @string: The string to look for
 *
 * Return value: An int with the index of the given string or 
 * EEL_STRING_LIST_NOT_FOUND if the string aint found.
 */
int
eel_string_list_get_index_for_string (const EelStringList *string_list,
				      const char *string)
{
	int n;
	const GSList *node;
	
	g_return_val_if_fail (string_list != NULL, EEL_STRING_LIST_NOT_FOUND);
	g_return_val_if_fail (string != NULL, EEL_STRING_LIST_NOT_FOUND);
	
	for (node = string_list->strings, n = 0; node != NULL; node = node->next, n++) {
		if (str_is_equal ((const char *) node->data, string,
				  string_list->compare_function == eel_strcmp_compare_func)) {
			return n;
		}
	}

	return EEL_STRING_LIST_NOT_FOUND;
}

/**
 * eel_string_list_as_string
 *
 * @string_list: A EelStringList
 * @delimiter: The string to use a delimiter, can be NULL.
 * @num_strings: Number of strings to concatenate.  Must be between 0 and length.
 *
 * Return value: An newly allocated string concatenation of all the items in the list.
 * The string is delimited by 'delimiter'.
 */
char *
eel_string_list_as_string (const EelStringList *string_list,
			   const char *delimiter,
			   int num_strings)
{
	char *result;
	int length;
	const char *current;
	int n;
	const GSList *node;
	GString	*tokens;
	
	g_return_val_if_fail (string_list != NULL, NULL);

	result = NULL;
	length = eel_string_list_get_length (string_list);

	if (num_strings == EEL_STRING_LIST_ALL_STRINGS) {
		num_strings = length;
	}

	if (num_strings == 0) {
		return g_strdup ("");
	}

	g_return_val_if_fail (num_strings >= 1, NULL);
	g_return_val_if_fail (num_strings <= num_strings, NULL);

	tokens = g_string_new (NULL);
	for (node = string_list->strings, n = 1; node != NULL && n <= num_strings; node = node->next, n++) {
		g_assert (node->data != NULL);
		current = node->data;
		
		g_string_append (tokens, current);
		
		if (delimiter && (n != num_strings)) {
			g_string_append (tokens, delimiter);
		}
	}
	
	result = tokens->str;
	g_string_free (tokens, FALSE);

	return result;
}

void
eel_string_list_sort (EelStringList *string_list)
{
	g_return_if_fail (string_list != NULL);

	string_list->strings = g_slist_sort (string_list->strings, string_list->compare_function);
}

/**
 * eel_string_list_sort_by_function
 *
 * @string_list: A EelStringList
 * @compare_function: Function to use for comparing the strings.
 *
 * Sort the strings using the given compare function.
 */
void
eel_string_list_sort_by_function (EelStringList *string_list,
				  GCompareFunc compare_function)
{
	g_return_if_fail (string_list != NULL);

	string_list->strings = g_slist_sort (string_list->strings, compare_function);
}

void
eel_string_list_remove_duplicates (EelStringList *string_list)
{
	GSList *new_strings;
	const GSList *node;
	const char *string;

	g_return_if_fail (string_list != NULL);
	
	new_strings = NULL;
	for (node = string_list->strings; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		string = node->data;

		if (g_slist_find_custom (new_strings,
					 (gpointer) string,
					 string_list->compare_function) == NULL) {
			new_strings = g_slist_append (new_strings, g_strdup (string));
		}
	}

	eel_string_list_clear (string_list);
	string_list->strings = new_strings;
}

/**
 * eel_string_list_get_longest_string:
 *
 * @string_list: A EelStringList
 *
 * Return value: A copy of the longest string in the collection.  Need to g_free() it.
 */
char *
eel_string_list_get_longest_string (const EelStringList *string_list)
{
	int longest_length;
	int longest_index;
	const GSList *node;
	int i;
	int current_length;

	g_return_val_if_fail (string_list != NULL, NULL);

	if (string_list->strings == NULL) {
		return NULL;
	}
	
	longest_length = 0;
	longest_index = 0;
	for (node = string_list->strings, i = 0; node != NULL; node = node->next, i++) {
		g_assert (node->data != NULL);
		current_length = eel_strlen (node->data);

		if (current_length > longest_length) {
			longest_index = i;
			longest_length = current_length;
		}
	}

	return eel_string_list_nth (string_list, longest_index);
}

/**
 * eel_string_list_get_longest_string_length:
 *
 * @string_list: A EelStringList
 *
 * Return value: The length of the longest string in the collection.
 */
int
eel_string_list_get_longest_string_length (const EelStringList *string_list)
{
	int longest_length;
	const GSList *node;
	int i;
	int current_length;

	g_return_val_if_fail (string_list != NULL, 0);

	if (string_list->strings == NULL) {
		return 0;
	}

	longest_length = 0;
	for (node = string_list->strings, i = 0; node != NULL; node = node->next, i++) {
		g_assert (node->data != NULL);
		current_length = eel_strlen (node->data);
		
		if (current_length > longest_length) {
			longest_length = current_length;
		}
	}

	return longest_length;
}

gboolean
eel_string_list_is_case_sensitive (const EelStringList *string_list)
{
	g_return_val_if_fail (string_list != NULL, FALSE);
	
	return string_list->compare_function == eel_strcmp_compare_func;
}

/* Invoke the given callback for each string in the collection. */
void
eel_string_list_for_each (const EelStringList *string_list,
			  EelStringListForEachCallback for_each_callback,
			  gpointer callback_data)
{
	const GSList *node;

	g_return_if_fail (for_each_callback != NULL);
	
	if (string_list == NULL) {
		return;
	}

	for (node = string_list->strings; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		(* for_each_callback) (node->data, callback_data);
	}
}

void
eel_string_list_append_string_list (EelStringList *string_list,
				    const EelStringList *append_string_list)
{
	const GSList *node;

	g_return_if_fail (string_list != NULL);

	if (append_string_list == NULL) {
		return;
	}

	for (node = append_string_list->strings; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		eel_string_list_insert (string_list, node->data);
	}
}

static gboolean
str_is_equal (const char *a,
	      const char *b,
	      gboolean case_sensitive)
{
	return case_sensitive ? eel_str_is_equal (a, b) : eel_istr_is_equal (a, b);
}

void
eel_string_list_reverse (EelStringList *string_list)
{
	g_return_if_fail (string_list != NULL);
	
	string_list->strings = g_slist_reverse (string_list->strings);
}

#if !defined (EEL_OMIT_SELF_CHECK)

static gboolean
test_dog (const EelStringList *string_list,
	     const char *string,
	     gpointer callback_data)
{
	return eel_str_is_equal (string, "dog");
}

static gboolean
test_data (const EelStringList *string_list,
	   const char *string,
	   gpointer callback_data)
{
	return eel_str_is_equal (string, callback_data);
}

static gboolean
test_true (const EelStringList *string_list,
	   const char *string,
	   gpointer callback_data)
{
	return TRUE;
}

static gboolean
test_false (const EelStringList *string_list,
	    const char *string,
	    gpointer callback_data)
{
	return FALSE;
}

static int
compare_number (gconstpointer string_a,
		gconstpointer string_b)
{
	int a;
	int b;
	
	g_return_val_if_fail (string_a != NULL, 0);
	g_return_val_if_fail (string_b != NULL, 0);

	g_return_val_if_fail (eel_str_to_int (string_a, &a), 0);
	g_return_val_if_fail (eel_str_to_int (string_b, &b), 0);

 	if (a < b) {
 		return -1;
	}

 	if (a == b) {
 		return 0;
 	}

	return 1;
}

static EelStringList *
test_string_list_reverse (const char *tokens,
			  const char *delimiter)
{
	EelStringList *result;

	result = eel_string_list_new_from_tokens (tokens, delimiter, TRUE);

	eel_string_list_reverse (result);

	return result;
}

static EelStringList *
test_new_from_string_array (const char *strings,
			    const char *delimiter)
{
	EelStringList *result;
	char **string_array;
	
 	g_return_val_if_fail (delimiter != NULL, NULL);

	if (strings == NULL) {
		return eel_string_list_new (TRUE);
	}

	string_array = g_strsplit (strings, delimiter, -1);
	result = eel_string_list_new_from_string_array ((const char * const *) string_array,
							TRUE);
	g_strfreev (string_array);
	
	return result;
}

void
eel_self_check_string_list (void)
{
	EelStringList *fruits;
	EelStringList *cities;
	EelStringList *cities_copy;
	EelStringList *empty;
	EelStringList *single;

	/*
	 * eel_string_list_contains
	 */
	empty = eel_string_list_new (TRUE);

	EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (empty), 0);
	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (empty, "something"), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (NULL, "something"), FALSE);

	/*
	 * eel_string_list_new
	 */
	cities = eel_string_list_new (TRUE);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (cities, empty), TRUE);
	
	eel_string_list_insert (cities, "london");
	eel_string_list_insert (cities, "paris");
	eel_string_list_insert (cities, "rome");

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (cities, empty), FALSE);

	/*
	 * eel_string_list_copy
	 */
	cities_copy = eel_string_list_copy (cities);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (cities, cities_copy), TRUE);

	eel_string_list_free (cities_copy);
	eel_string_list_free (cities);

	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_copy (NULL) == NULL, TRUE);

	/*
	 * eel_string_list_insert,
	 * eel_string_list_nth,
	 * eel_string_list_contains,
	 * eel_string_list_get_length
	 */
	fruits = eel_string_list_new (TRUE);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (fruits, empty), TRUE);

	eel_string_list_insert (fruits, "orange");
	eel_string_list_insert (fruits, "apple");
	eel_string_list_insert (fruits, "strawberry");
	eel_string_list_insert (fruits, "cherry");
	eel_string_list_insert (fruits, "bananna");
	eel_string_list_insert (fruits, "watermelon");

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (fruits, empty), FALSE);

 	EEL_CHECK_STRING_RESULT (eel_string_list_nth (fruits, 0), "orange");
 	EEL_CHECK_STRING_RESULT (eel_string_list_nth (fruits, 2), "strawberry");
 	EEL_CHECK_STRING_RESULT (eel_string_list_nth (fruits, 3), "cherry");
 	EEL_CHECK_STRING_RESULT (eel_string_list_nth (fruits, 5), "watermelon");
	suppress_out_of_bounds_warning = TRUE;
 	EEL_CHECK_STRING_RESULT (eel_string_list_nth (fruits, 6), NULL);
	suppress_out_of_bounds_warning = FALSE;

	EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (fruits), 6);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (fruits, "orange"), TRUE);
 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (fruits, "apple"), TRUE);
 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (fruits, "watermelon"), TRUE);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (fruits, "pineapple"), FALSE);

 	eel_string_list_clear (fruits);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (fruits, "orange"), FALSE);
	EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (fruits), 0);
	
	eel_string_list_free (fruits);
	eel_string_list_free (empty);

	/*
	 * eel_string_list_new_from_string
	 */
	single = eel_string_list_new_from_string ("something", TRUE);

 	EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (single, "something"), TRUE);
	EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (single), 1);

	eel_string_list_free (single);


	/*
	 * eel_string_list_as_g_slist
	 */
	{
		guint i;
		GSList *slist;
		const GSList *node;
		EelStringList *string_list;

		string_list = eel_string_list_new (TRUE);
		
		eel_string_list_insert (string_list, "orange");
		eel_string_list_insert (string_list, "apple");
		eel_string_list_insert (string_list, "strawberry");
		eel_string_list_insert (string_list, "cherry");
		eel_string_list_insert (string_list, "bananna");
		eel_string_list_insert (string_list, "watermelon");
		
		slist = eel_string_list_as_g_slist (string_list);
		
		EEL_CHECK_BOOLEAN_RESULT (g_slist_length (slist) == eel_string_list_get_length (string_list),
					  TRUE);

		for (i = 0, node = slist;
		     i < eel_string_list_get_length (string_list); 
		     i++, node = node->next) {
			char *s1 = eel_string_list_nth (string_list, i);
			const char *s2 = (const char *) node->data;

			EEL_CHECK_INTEGER_RESULT (eel_strcmp (s1, s2), 0);

			g_free (s1);
		}

		eel_string_list_free (string_list);

		eel_g_slist_free_deep (slist);
	}
	
	/*
	 * eel_string_list_get_index_for_string
	 *
	 */

	{
		EelStringList *fruits;

		fruits = eel_string_list_new (TRUE);
		
		eel_string_list_insert (fruits, "orange");
		eel_string_list_insert (fruits, "apple");
		eel_string_list_insert (fruits, "strawberry");
		eel_string_list_insert (fruits, "cherry");
		eel_string_list_insert (fruits, "bananna");
		eel_string_list_insert (fruits, "watermelon");

		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "orange"), 0);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "apple"), 1);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "strawberry"), 2);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "cherry"), 3);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "bananna"), 4);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "watermelon"), 5);

		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_index_for_string (fruits, "papaya"), EEL_STRING_LIST_NOT_FOUND);

		eel_string_list_free (fruits);
	}

	/*
	 * eel_string_list_as_string
	 *
	 */
	{
		EelStringList *l;

		l = eel_string_list_new (TRUE);

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, EEL_STRING_LIST_ALL_STRINGS), "");
		
		eel_string_list_insert (l, "x");

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, EEL_STRING_LIST_ALL_STRINGS), "x");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, ":", EEL_STRING_LIST_ALL_STRINGS), "x");

		eel_string_list_insert (l, "y");
		eel_string_list_insert (l, "z");

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, EEL_STRING_LIST_ALL_STRINGS), "xyz");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "", EEL_STRING_LIST_ALL_STRINGS), "xyz");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, ":", EEL_STRING_LIST_ALL_STRINGS), "x:y:z");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "abc", EEL_STRING_LIST_ALL_STRINGS), "xabcyabcz");

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, 0), "");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "", 0), "");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, ":", 0), "");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "abc", 0), "");

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, 1), "x");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "", 1), "x");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, ":", 1), "x");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "abc", 1), "x");

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, 2), "xy");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "", 2), "xy");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, ":", 2), "x:y");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "abc", 2), "xabcy");

		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, NULL, 3), "xyz");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "", 3), "xyz");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, ":", 3), "x:y:z");
		EEL_CHECK_STRING_RESULT (eel_string_list_as_string (l, "abc", 3), "xabcyabcz");

		eel_string_list_free (l);
	}


	/*
	 * eel_string_list_sort
	 *
	 */
	{
		EelStringList *l;

		l = eel_string_list_new (TRUE);
		
		eel_string_list_insert (l, "dog");
		eel_string_list_insert (l, "cat");
		eel_string_list_insert (l, "bird");

		eel_string_list_sort (l);

		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 0), "bird");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 1), "cat");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 2), "dog");

		eel_string_list_free (l);
	}

	/*
	 * eel_string_list_remove_duplicates
	 *
	 */
	{
		EelStringList *l;

		l = eel_string_list_new (TRUE);

		eel_string_list_remove_duplicates (l);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 0);
		
		eel_string_list_insert (l, "foo");
		eel_string_list_insert (l, "bar");
		eel_string_list_insert (l, "bar");
		eel_string_list_insert (l, "foo");
		eel_string_list_insert (l, "foo");

		eel_string_list_remove_duplicates (l);

		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 2);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 0), "foo");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 1), "bar");

		eel_string_list_clear (l);

		eel_string_list_remove_duplicates (l);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 0);

		eel_string_list_insert (l, "single");
		eel_string_list_remove_duplicates (l);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 1);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 0), "single");

		eel_string_list_clear (l);

		eel_string_list_free (l);
	}

	/*
	 * eel_string_list_assign_from_string_list
	 *
	 */
	{
		EelStringList *l;
		EelStringList *other;

		l = eel_string_list_new (TRUE);
		other = eel_string_list_new (TRUE);

		/* assign an other with some items */
		eel_string_list_insert (other, "dog");
		eel_string_list_insert (other, "cat");
		eel_string_list_insert (other, "mouse");
		eel_string_list_assign_from_string_list (l, other);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 3);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 0), "dog");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 1), "cat");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 2), "mouse");

		/* assign an other with 1 item */
		eel_string_list_clear (other);
		eel_string_list_insert (other, "something");
		eel_string_list_assign_from_string_list (l, other);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 1);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (l, 0), "something");
		
		/* assign an empty other */
		eel_string_list_clear (other);
		eel_string_list_assign_from_string_list (l, other);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 0);

		eel_string_list_free (l);
		eel_string_list_free (other);
	}


	/*
	 * eel_string_list_get_longest_string
	 *
	 */
	{
		EelStringList *l;

		l = eel_string_list_new (TRUE);

		EEL_CHECK_STRING_RESULT (eel_string_list_get_longest_string (l), NULL);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_longest_string_length (l), 0);

		eel_string_list_insert (l, "a");
		eel_string_list_insert (l, "bb");
		eel_string_list_insert (l, "ccc");
		eel_string_list_insert (l, "dddd");

		EEL_CHECK_STRING_RESULT (eel_string_list_get_longest_string (l), "dddd");
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_longest_string_length (l), strlen ("dddd"));

		eel_string_list_clear (l);

		eel_string_list_insert (l, "foo");
		EEL_CHECK_STRING_RESULT (eel_string_list_get_longest_string (l), "foo");
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_longest_string_length (l), strlen ("foo"));

		eel_string_list_free (l);

	}

	/*
	 * case insensitive tests
	 *
	 */
	{
		EelStringList *l;

		l = eel_string_list_new (FALSE);

		eel_string_list_insert (l, "foo");
		eel_string_list_insert (l, "bar");

		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (l, "Foo"), TRUE);
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (l, "foO"), TRUE);
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (l, "fOo"), TRUE);
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_contains (l, "foo"), TRUE);

		eel_string_list_clear (l);

		eel_string_list_insert (l, "Foo");
		eel_string_list_insert (l, "Foo");
		eel_string_list_insert (l, "fOo");
		eel_string_list_insert (l, "foO");
		eel_string_list_remove_duplicates (l);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (l), 1);

		eel_string_list_free (l);
	}

	/*
	 * eel_string_list_new_from_tokens
	 */
	{
		EelStringList *lines;
		EelStringList *thick_lines;

		const char lines_string[] = "This\nAre\nSome\n\nLines";
		const char thick_lines_string[] = "This####Are####Some########Lines";
		const int num_lines = eel_str_count_characters (lines_string, '\n') + 1;

		lines = eel_string_list_new_from_tokens (lines_string, "\n", TRUE);
		thick_lines = eel_string_list_new_from_tokens (thick_lines_string, "####", TRUE);

		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (lines, thick_lines), TRUE);

		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (lines), num_lines);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (lines, 0), "This");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (lines, 1), "Are");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (lines, 2), "Some");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (lines, 3), "");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (lines, 4), "Lines");
		
		eel_string_list_free (lines);
		eel_string_list_free (thick_lines);

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_tokens (NULL, ",", TRUE), "", ",");
		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_tokens ("", ",", TRUE), "", ",");
		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_tokens ("foo", ",", TRUE), "foo", ",");
		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_tokens ("foo,bar", ",", TRUE), "foo,bar", ",");
	}

	/*
	 * eel_string_list_modify_nth
	 */
	{
		EelStringList *list;

		list = eel_string_list_new (TRUE);
		eel_string_list_insert (list, "dog");
		eel_string_list_insert (list, "cat");
		eel_string_list_insert (list, "mouse");

		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 0), "dog");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 2), "mouse");
		eel_string_list_modify_nth (list, 2, "rat");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 2), "rat");
		eel_string_list_modify_nth (list, 0, "pig");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 0), "pig");
		
		eel_string_list_free (list);
	}

	/*
	 * eel_string_list_remove_nth
	 */
	{
		EelStringList *list;

		list = eel_string_list_new (TRUE);
		eel_string_list_insert (list, "dog");
		eel_string_list_insert (list, "cat");
		eel_string_list_insert (list, "mouse");
		eel_string_list_insert (list, "bird");
		eel_string_list_insert (list, "pig");
		
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (list), 5);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 0), "dog");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 4), "pig");

		eel_string_list_remove_nth (list, 2);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (list), 4);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 2), "bird");

		eel_string_list_remove_nth (list, 3);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (list), 3);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 0), "dog");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 2), "bird");

		eel_string_list_remove_nth (list, 0);
		EEL_CHECK_INTEGER_RESULT (eel_string_list_get_length (list), 2);
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 0), "cat");
		EEL_CHECK_STRING_RESULT (eel_string_list_nth (list, 1), "bird");
		
		eel_string_list_free (list);
	}

	/*
	 * eel_string_list_find_by_function
	 */
	{
		EelStringList *list;

		list = eel_string_list_new (TRUE);
		eel_string_list_insert (list, "house");
		eel_string_list_insert (list, "street");
		eel_string_list_insert (list, "car");
		eel_string_list_insert (list, "dog");
		
		EEL_CHECK_STRING_RESULT (eel_string_list_find_by_function (NULL, test_dog, NULL), NULL);
		EEL_CHECK_STRING_RESULT (eel_string_list_find_by_function (list, test_dog, NULL), "dog");
		EEL_CHECK_STRING_RESULT (eel_string_list_find_by_function (list, test_false, NULL), NULL);
		EEL_CHECK_STRING_RESULT (eel_string_list_find_by_function (list, test_true, NULL), "house");
		EEL_CHECK_STRING_RESULT (eel_string_list_find_by_function (list, test_data, "car"), "car");

		eel_string_list_free (list);
	}

	/*
	 * eel_string_list_sort_by_function
	 */
	{
		EelStringList *sorted_list;
		EelStringList *list;

		sorted_list = eel_string_list_new (TRUE);
		eel_string_list_insert (sorted_list, "0");
		eel_string_list_insert (sorted_list, "1");
		eel_string_list_insert (sorted_list, "2");
		eel_string_list_insert (sorted_list, "3");
		eel_string_list_insert (sorted_list, "4");

		list = eel_string_list_new (TRUE);
		eel_string_list_insert (list, "4");
		eel_string_list_insert (list, "2");
		eel_string_list_insert (list, "1");
		eel_string_list_insert (list, "0");
		eel_string_list_insert (list, "3");

		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (list, sorted_list), FALSE);

		eel_string_list_sort_by_function (list, compare_number);
		
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_equals (list, sorted_list), TRUE);

		eel_string_list_free (list);
		eel_string_list_free (sorted_list);
	}

	/*
	 * eel_string_list_nth_as_integer
	 */
	{
		EelStringList *list;
		const int untouched = 666;
		int result;

		list = eel_string_list_new_from_tokens ("word,c,0,1,20,xxx,foo,bar,-1", ",", TRUE);
		
		result = untouched;
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 0, &result), FALSE);
		EEL_CHECK_INTEGER_RESULT (result, untouched);

		result = untouched;
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 1, &result), FALSE);
		EEL_CHECK_INTEGER_RESULT (result, untouched);

		result = untouched;
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 5, &result), FALSE);
		EEL_CHECK_INTEGER_RESULT (result, untouched);

		result = untouched;
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 6, &result), FALSE);
		EEL_CHECK_INTEGER_RESULT (result, untouched);

		result = untouched;
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 7, &result), FALSE);
		EEL_CHECK_INTEGER_RESULT (result, untouched);
		
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 2, &result), TRUE);
		EEL_CHECK_INTEGER_RESULT (result, 0);
		
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 3, &result), TRUE);
		EEL_CHECK_INTEGER_RESULT (result, 1);

		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 4, &result), TRUE);
		EEL_CHECK_INTEGER_RESULT (result, 20);
		
		EEL_CHECK_BOOLEAN_RESULT (eel_string_list_nth_as_integer (list, 8, &result), TRUE);
		EEL_CHECK_INTEGER_RESULT (result, -1);
		
		eel_string_list_free (list);
	}

	/* eel_string_list_new_from_string_gslist */
	{
		GSList *gslist;

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_slist (NULL, TRUE), "", ",");
		
		gslist = NULL;
		gslist = g_slist_append (gslist, g_strdup ("foo"));

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_slist (gslist, TRUE), "foo", ",");

		gslist = g_slist_append (gslist, g_strdup ("bar"));

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_slist (gslist, TRUE), "foo,bar", ",");

		gslist = g_slist_append (gslist, g_strdup ("baz"));
		
		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_slist (gslist, TRUE), "foo,bar,baz", ",");
		
		eel_g_slist_free_deep (gslist);
	}

	/* eel_string_list_new_from_string_glist */
	{
		GList *glist;

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_list (NULL, TRUE), "", ",");
		
		glist = NULL;
		glist = g_list_append (glist, g_strdup ("foo"));

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_list (glist, TRUE), "foo", ",");

		glist = g_list_append (glist, g_strdup ("bar"));

		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_list (glist, TRUE), "foo,bar", ",");

		glist = g_list_append (glist, g_strdup ("baz"));
		
		EEL_CHECK_STRING_LIST_RESULT (eel_string_list_new_from_g_list (glist, TRUE), "foo,bar,baz", ",");
		
		eel_g_list_free_deep (glist);
	}

	/* eel_string_list_reverse */
	EEL_CHECK_STRING_LIST_RESULT (test_string_list_reverse (NULL, ","), "", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_string_list_reverse ("", ","), "", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_string_list_reverse ("foo", ","), "foo", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_string_list_reverse ("foo,bar", ","), "bar,foo", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_string_list_reverse ("1,2,3,4,5", ","), "5,4,3,2,1", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_string_list_reverse ("1", ","), "1", ",");

	/* eel_string_list_new_from_string_array */
	EEL_CHECK_STRING_LIST_RESULT (test_new_from_string_array (NULL, ","), "", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_new_from_string_array ("", ","), "", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_new_from_string_array ("1", ","), "1", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_new_from_string_array ("1,2", ","), "1,2", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_new_from_string_array ("1,2,3,4,5", ","), "1,2,3,4,5", ",");
	EEL_CHECK_STRING_LIST_RESULT (test_new_from_string_array ("foo,bar,baz", ","), "foo,bar,baz", ",");
}

#endif /* !EEL_OMIT_SELF_CHECK */
