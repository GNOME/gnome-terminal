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

#ifndef EEL_STRING_LIST_H
#define EEL_STRING_LIST_H

#include <glib.h>

#define EEL_STRING_LIST_NOT_FOUND -1
#define EEL_STRING_LIST_ALL_STRINGS -1

/* Opaque type declaration. */
typedef struct EelStringList EelStringList;

/* Test function for finding strings in the list */
typedef gboolean (*EelStringListTestFunction) (const EelStringList *string_list,
					       const char *string,
					       gpointer callback_data);

/* StringList iterator */
typedef void (*EelStringListForEachCallback) (const char *string,
					      gpointer callback_data);

/* Construct an empty string list. */
EelStringList *eel_string_list_new                       (gboolean                      case_sensitive);

/* Construct a string list with a single element */
EelStringList *eel_string_list_new_from_string           (const char                   *string,
							  gboolean                      case_sensitive);

/* Construct a string list that is a copy of another string list */
EelStringList *eel_string_list_copy                      (const EelStringList          *string_list);

/* Construct a string list that is a copy of another string list */
EelStringList *eel_string_list_new_from_g_slist          (const GSList                 *gslist,
							  gboolean                      case_sensitive);

/* Construct a string list that is a copy of another string list */
EelStringList *eel_string_list_new_from_g_list           (const GList                  *glist,
							  gboolean                      case_sensitive);

/* Construct a string list from a NULL terminated string array */
EelStringList *eel_string_list_new_from_string_array     (const char * const            string_array[],
							  gboolean                      case_sensitive);
void           eel_string_list_assign_from_string_array  (EelStringList                *string_list,
							  const char * const            string_array[]);

/* Construct a string list from tokens delimited by the given string and delimiter */
EelStringList *eel_string_list_new_from_tokens           (const char                   *string,
							  const char                   *delimiter,
							  gboolean                      case_sensitive);

/* Assign the contents another string list.  The other string list can be null. */
void           eel_string_list_assign_from_string_list   (EelStringList                *string_list,
							  const EelStringList          *other_or_null);
/* Free a string list */
void           eel_string_list_free                      (EelStringList                *string_list_or_null);

/* Insert a string into the collection. */
void           eel_string_list_insert                    (EelStringList                *string_list,
							  const char                   *string);
/* Insert a string into the collection. */
void           eel_string_list_prepend                   (EelStringList                *string_list,
							  const char                   *string);

/* Insert a string list into the collection. */
void           eel_string_list_insert_string_list        (EelStringList                *string_list,
							  const EelStringList          *other_string_list);

/* Append the contents of one string list to another */
void           eel_string_list_append_string_list        (EelStringList                *string_list,
							  const EelStringList          *append_string_list);
/* Clear the collection. */
void           eel_string_list_clear                     (EelStringList                *string_list);

/* Access the nth string in the collection.  Returns an strduped string. */
char *         eel_string_list_nth                       (const EelStringList          *string_list,
							  guint                         n);
/* Access the nth string as an integer.  Return TRUE if the conversion was successful.  */
gboolean       eel_string_list_nth_as_integer            (const EelStringList          *string_list,
							  guint                         n,
							  int                          *integer_result);
/* Modify the nth string in the collection. */
void           eel_string_list_modify_nth                (EelStringList                *string_list,
							  guint                         n,
							  const char                   *string);
/* Remove the nth string in the collection. */
void           eel_string_list_remove_nth                (EelStringList                *string_list,
							  guint                         n);
/* Does the string list contain the given string ? */
gboolean       eel_string_list_contains                  (const EelStringList          *string_list,
							  const char                   *string);
/* Find a string using the given test function ? */
char *         eel_string_list_find_by_function          (const EelStringList          *string_list,
							  EelStringListTestFunction     test_function,
							  gpointer                      callback_data);
/* How many strings are currently in the collection ? */
guint          eel_string_list_get_length                (const EelStringList          *string_list);

/* Get the index for the given string.  Return EEL_STRING_LIST_NOT_FOUND if not found. */
int            eel_string_list_get_index_for_string      (const EelStringList          *string_list,
							  const char                   *string);
/* Does the string list a equal string list b ? */
gboolean       eel_string_list_equals                    (const EelStringList          *a,
							  const EelStringList          *b);
/* Return the string list in a GList.  Must deep free the result with eel_g_list_free_deep() */
GSList *       eel_string_list_as_g_slist                (const EelStringList          *string_list);

/* Return the string list as a concatenation of all the items delimited by delimiter. */
char *         eel_string_list_as_string                 (const EelStringList          *string_list,
							  const char                   *delimiter,
							  int                           num_strings);
/* Sort the string collection. */
void           eel_string_list_sort                      (EelStringList                *string_list);

/* Sort the string collection using a comparison function. */
void           eel_string_list_sort_by_function          (EelStringList                *string_list,
							  GCompareFunc                  compare_function);
/* Remove duplicate strings from the collection. */
void           eel_string_list_remove_duplicates         (EelStringList                *string_list);

/* Invoke the given callback for each string in the collection. */
void           eel_string_list_for_each                  (const EelStringList          *string_list,
							  EelStringListForEachCallback  for_each_callback,
							  gpointer                      callback_data);

/* Return the longest string in the collection. */
char *         eel_string_list_get_longest_string        (const EelStringList          *string_list);

/* Return the length of the longest string in the collection. */
int            eel_string_list_get_longest_string_length (const EelStringList          *string_list);

/* Return whether the string list is case sensitive */
gboolean       eel_string_list_is_case_sensitive         (const EelStringList          *string_list);

/* Reverse the the order of the strings */
void           eel_string_list_reverse                   (EelStringList                *string_list);

#endif /* EEL_STRING_LIST_H */

