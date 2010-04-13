/*
 * Copyright © 2009 Richard Russon (flatcap)
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

#include "config.h"

#include <string.h>
#include <gconf/gconf-client.h>
#include <glib.h>

#include "terminal-app.h"
#include "terminal-find.h"
#include "terminal-window.h"
#include "terminal-util.h"
//#include "vteint.h"
//#include "vte-private.h"

/* Our config info lives under: /apps/gnome-terminal/find */
#define CONF_FIND_PREFIX        CONF_PREFIX "/find"

#define CONF_FIND_MATCH_CASE    CONF_FIND_PREFIX "/match_case"
#define CONF_FIND_MATCH_REGEX   CONF_FIND_PREFIX "/match_regex"
#define CONF_FIND_MATCH_WHOLE   CONF_FIND_PREFIX "/match_whole"


#define TERMINAL_FIND_FLAG_CASE   (1 << 0)
#define TERMINAL_FIND_FLAG_REGEX  (1 << 1)
#define TERMINAL_FIND_FLAG_WHOLE  (1 << 2)

typedef struct
{
  char *find_string;
  char *regex_string;
  int   row;
  int   column;
  int   length;
  int   flags;
  void *screen;
} FindParams;

/* Keep track of where we are */
static FindParams     *params;


/* GConf stuff */
static GConfClient    *gconf;
static guint           nid_case;                /* Notify IDs */
static guint           nid_regex;
static guint           nid_whole;

/* Find dialog and children */
static GtkDialog      *dialog;
static GtkCheckButton *check_case;
static GtkCheckButton *check_regex;
static GtkCheckButton *check_whole;
static GtkEntry       *entry;
static GtkTreeModel   *model;
static gint            entry_max;
static GtkWindow      *parent;


static void terminal_find_set_parent (GtkWindow *new_parent);

/**
 * terminal_find_history_add
 * @str: Find string to be added
 *
 * Add a new item to the history of find strings.
 * If the item already exists, it's moved to the top of the list.
 */
static void
terminal_find_history_add (const char *str)
{
  GtkTreeIter iter;
  int i;
  int items;

  g_assert (model);

  if (!str)
    return;

  items = gtk_tree_model_iter_n_children (model, NULL);

  /* First remove any existing matches */
  gtk_tree_model_get_iter_first (model, &iter);
  for (i = 0; i < items; i++)
    {
      GValue value = { 0 };
      gtk_tree_model_get_value (model, &iter, 0, &value);
      if (!g_strcmp0 (str, g_value_get_string (&value)))
        {
          gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
          g_value_unset (&value);
          break;
        }
      gtk_tree_model_iter_next (model, &iter);
      g_value_unset (&value);
    }

  /* Add the new item to the top of the list */
  gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, str, -1);
  items = gtk_tree_model_iter_n_children (model, NULL);

  /* Truncate the list if it's too long */
  while (items > entry_max)
    {
      gtk_tree_model_iter_nth_child (model, &iter, NULL, entry_max);
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      items = gtk_tree_model_iter_n_children (model, NULL);
    }
}

/**
 * terminal_find_history_init
 *
 * Add a history of previous find strings,
 * by adding a GtkEntryCompletion to the GtkEntry.
 */
static void
terminal_find_history_init (void)
{
  GtkEntryCompletion *comp = NULL;

  g_assert (entry);

  comp = gtk_entry_completion_new ();

  /* A history with a drop-down list of partial matches */
  gtk_entry_completion_set_inline_selection (comp, TRUE);
  gtk_entry_completion_set_text_column (comp, 0);
  gtk_entry_set_completion (entry, comp);

  model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
  gtk_entry_completion_set_model (comp, model);

  /* The GtkEntry's holding the references now. */
  g_object_unref (comp);
  g_object_unref (model);
}


/**
 * terminal_find_aways_selected
 *
 * Helper function for vte_terminal_get_text_range.
 */
static gboolean
terminal_find_aways_selected (VteTerminal *terminal,
                              glong column,
                              glong row,
                              gpointer data)
{
  return TRUE;
}

/**
 * terminal_find_replace_string
 *
 * A helper to manage string pointers.
 * Free an old strings (if it exists) and use the new pointer.
 */
static void
terminal_find_replace_string (char **oldstr,
                              char  *newstr)
{
  if (!oldstr)
    return;

  g_free (*oldstr);
  *oldstr = newstr;
}

/**
 * terminal_find_new_search
 *
 * Create a new FindParams object to keep track of a search.
 */
static FindParams *
terminal_find_new_search (void)
{
  return g_new0 (FindParams, 1);
}

/**
 * terminal_find_free_search
 *
 * Free a FindParam object used when searching.
 */
static void
terminal_find_free_search (FindParams *fp)
{
  if (!fp)
    return;

  g_free (fp->find_string);
  g_free (fp->regex_string);
  g_free (fp);
}

/**
 * terminal_find_build_search
 *
 * Create the search strings and set the flags.
 * If the search string has changed we reset the row and column.
 *
 * This function know about the FindDialog.
 */
static gboolean
terminal_find_build_search (FindParams *fp)
{
  char    *new_str   = NULL;
  int      new_flags = 0;
  gboolean changed   = FALSE;

  if (!fp)
    return FALSE;

  g_assert (check_case);
  g_assert (check_regex);
  g_assert (check_whole);
  g_assert (entry);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_case)))
    new_flags |= TERMINAL_FIND_FLAG_CASE;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_regex)))
    new_flags |= TERMINAL_FIND_FLAG_REGEX;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_whole)))
    new_flags |= TERMINAL_FIND_FLAG_WHOLE;

  if (fp->flags != new_flags)
    {
      fp->flags = new_flags;
      changed = TRUE;
    }

  new_str = g_strdup (gtk_entry_get_text (entry));
  if (g_strcmp0 (fp->find_string, new_str) != 0)
    {
      terminal_find_replace_string (&fp->find_string, g_strdup (new_str));
      changed = TRUE;
    }

  if (changed)
    {
      fp->row    = -1;
      fp->column = -1;
    }

  if ((fp->flags & TERMINAL_FIND_FLAG_REGEX) == 0)
    terminal_find_replace_string (&new_str, g_regex_escape_string (new_str, -1));

  /* Perl Regular Expression (RE) */
  if (fp->flags & TERMINAL_FIND_FLAG_WHOLE)
    terminal_find_replace_string (&new_str, g_strdup_printf ("\\b%s\\b", new_str));

  terminal_find_replace_string (&fp->regex_string, new_str);

  return changed;
}

/**
 * terminal_find_perform_search
 * @fp: Search criteria
 *
 * Search through the buffer for a match.  The search will begin at row,column
 * @fp->row,  @fp->column.  If they are set to -1, the search will begin at
 * the first visible line of the buffer.
 *
 * This dialog knows about the GnomeTerminal window.
 *
 * Returns: %TRUE a match was found, or %FALSE, no match found
 */
static gboolean
terminal_find_perform_search (FindParams *fp)
{
  /* We don't need to free these pointers */
  VteTerminal       *vterm  = NULL;
  GtkAdjustment     *adj    = NULL;
  TerminalScreen    *screen = NULL;
  VteCharAttributes *ca     = NULL;

  /* We must free these pointers */
  char       *row        = NULL;
  GRegex     *regex      = NULL;
  GMatchInfo *match_info = NULL;
  GError     *error      = NULL;
  GArray     *attrs      = NULL;
  gchar      *word       = NULL;

  int b_first;
  int b_cursor;
  int b_last;
  int b_page;
  int b_range;
  int i;
  int rownum;
  gboolean result = FALSE;
  GRegexCompileFlags regex_flags = 0;
  int colnum = 0;
  int start = 0;
  int end = 0;

  if (!fp)
    return FALSE;

  g_assert (parent);

  /* XXX There's a small window when the user could close the tab
   * while we're searching.
   */
  screen = terminal_window_get_active (TERMINAL_WINDOW (parent));
  vterm = VTE_TERMINAL (screen);
  adj = vterm->adjustment;

  /* Some measures of the screen */
  b_first  = gtk_adjustment_get_lower (adj);       /* First line of the buffer */
  b_cursor = gtk_adjustment_get_value (adj);       /* First visible line of the buffer */
  b_last   = gtk_adjustment_get_upper (adj);       /* Last line of the buffer */
  b_page   = gtk_adjustment_get_page_size (adj);   /* Number of lines on screen */
  b_range  = b_last - b_first + 1;                 /* Number of lines to search */

  if ((fp->row == -1) ||                        /* Something in the build is different */
      ((fp->row + b_page) >= b_last) ||         /* Result on screen */
      (fp->screen != screen))                   /* Tab has changed */
    {
      fp->row    = b_cursor;
      fp->column = 0;
      fp->screen = screen;
    }

  fp->length = -1;

  /* Generate a regex for searching */
  if ((fp->flags & TERMINAL_FIND_FLAG_CASE) == 0)
    regex_flags |= G_REGEX_CASELESS;

  attrs = g_array_new (FALSE, TRUE, sizeof (VteCharAttributes));
  colnum = fp->column;
  regex = g_regex_new (fp->find_string, regex_flags, 0, NULL);

  for (i = 0; i < b_range; i++)
    {
      if (i == 1)               /* After the first row, colnum = 0 */
        colnum = 0;

      /* search from find_row, not b_cursor, just in case the previous match is on screen */
      /* search from fp->row...b_last, b_first...b_cursor-1 */
      rownum = ((i + fp->row - b_first) % b_range) + b_first;

      row = vte_terminal_get_text_range (vterm, rownum, colnum, rownum, 1000, terminal_find_aways_selected, NULL, attrs);

      g_regex_match_full (regex, row, -1, 0, 0, &match_info, &error);
      if (error)
        {
          g_printerr ("Error while matching: %s\n", error->message);
          g_error_free (error);
          g_match_info_free (match_info);
          g_free (row);
          break;
        }

      if (!g_match_info_matches (match_info))
        {
          g_match_info_free (match_info);
          g_free (row);
          continue;
        }

      word = g_match_info_fetch (match_info, 0);
      g_match_info_fetch_pos (match_info, 0, &start, &end);

      /* This gives us the offset in the buffer */
      ca = &g_array_index (attrs, VteCharAttributes, start);
      /*g_print ("Found: %s at (%ld,%ld)\n", word, ca->row, ca->column);*/

      fp->length = strlen (word);
      fp->row    = ca->row;
      fp->column = ca->column;
      g_free (word);
      g_free (row);
      g_match_info_free (match_info);

      result = TRUE;
      break;
    }

  g_array_free (attrs, TRUE);
  g_regex_unref (regex);
  return result;
}

/**
 * terminal_find_show_search
 * @fp: Match details
 *
 * Highlight a match.  The buffer is scrolled to put the match on the top line
 * of the screen (if possible).
 *
 * XXX Currently, VTE won't allow us to highlight the matching text.
 */
static void
terminal_find_show_search (FindParams *fp)
{
  VteTerminal   *vterm = NULL;
  GtkAdjustment *adj   = NULL;

  if (!fp || !fp->screen)
    return;

  vterm = VTE_TERMINAL (fp->screen);
  adj = vterm->adjustment;
  gtk_adjustment_set_value (adj, fp->row);

  //_vte_terminal_select_text (vterm, fp->column, fp->row, fp->column + fp->length - 1, fp->row, 0, 0);
  //_vte_invalidate_all (vterm);
}

/**
 * terminal_find_window_cb
 *
 * Our parent has died.  We need to close immediately.
 */
static void
terminal_find_window_cb (GtkWidget *window, gpointer user_data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

/**
 * terminal_find_response_cb
 * @response: ID of the button that was pressed
 *
 * The user pressed a button.
 * If they pressed "Find" (GTK_RESPONSE_APPLY), perform a search,
 * otherwise close the dialog.
 */
static void
terminal_find_response_cb (GtkWidget *dialog,
                           int        response,
                           gpointer   user_data)
{
  /* Might get GTK_RESPONSE_CLOSE or GTK_RESPONSE_DELETE_EVENT */
  if (response != GTK_RESPONSE_APPLY)
    {
      gtk_widget_destroy (dialog);
      return;
    }

  if (!params)
      params = terminal_find_new_search ();

  terminal_find_build_search (params);
  terminal_find_history_add (params->find_string);
  terminal_find_perform_search (params);
  terminal_find_show_search (params);

  /* When we next search, make sure we don't get the same match */
  params->column++;
}

/**
 * terminal_find_clear_cb
 * @entry: Text Box to be cleared
 *
 * The user has hit the sweep icon -- clear the find string.
 */
static void
terminal_find_clear_cb (GtkEntry *find,
                        GtkEntryIconPosition icon_pos,
                        GdkEvent *event,
                        gpointer user_data)
{
  gtk_entry_set_text (find, "");
}

/**
 * terminal_find_toggled_cb
 *
 * User changed an option in the find dialog.
 * Store the setting in GConf.
 */
static void
terminal_find_toggled_cb (GtkToggleButton *togglebutton,
                          gpointer        *data)
{
  gboolean b;

  g_assert (check_case);
  g_assert (check_regex);
  g_assert (check_whole);

  b = gtk_toggle_button_get_active (togglebutton);

  if (togglebutton == GTK_TOGGLE_BUTTON (check_case))
    gconf_client_set_bool (gconf, CONF_FIND_MATCH_CASE, b, NULL);
  else if (togglebutton == GTK_TOGGLE_BUTTON (check_regex))
    gconf_client_set_bool (gconf, CONF_FIND_MATCH_REGEX, b, NULL);
  else if (togglebutton == GTK_TOGGLE_BUTTON (check_whole))
    gconf_client_set_bool (gconf, CONF_FIND_MATCH_WHOLE, b, NULL);
}

/**
 * terminal_find_text_cb
 *
 * The text in the entry box has changed.
 * If there's some text, enable the clear button.
 */
static void
terminal_find_text_cb (GtkEntry   *text,
                       GParamSpec *pspec,
                       GtkWidget  *button)
{
  gboolean has_text;

  has_text = gtk_entry_get_text_length (text) > 0;
  gtk_entry_set_icon_sensitive (text, GTK_ENTRY_ICON_SECONDARY, has_text);
  gtk_widget_set_sensitive (button, has_text);
}

/**
 * terminal_find_check_cb
 *
 * An option in GConf changed.  Update the find dialog.
 */
static void
terminal_find_check_cb (GConfClient *client,
                        guint cnxn_id,
                        GConfEntry *conf_entry,
                        gpointer button)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), gconf_value_get_bool (conf_entry->value));
}

/**
 * terminal_find_destroyed_cb
 *
 * The Find Dialog has been closed.
 * Clear up any resources we were using.
 */
static void
terminal_find_destroyed_cb (GtkWidget *widget,
                            gpointer   user_data)
{
  g_assert (gconf);

  gconf_client_notify_remove (gconf, nid_case);
  gconf_client_notify_remove (gconf, nid_regex);
  gconf_client_notify_remove (gconf, nid_whole);
  nid_case  = 0;
  nid_regex = 0;
  nid_whole = 0;

  gconf_client_remove_dir (gconf, CONF_FIND_PREFIX, NULL);

  g_object_unref (gconf);
  gconf = NULL;

  terminal_find_free_search (params);
  params = NULL;

  dialog      = NULL;
  check_case  = NULL;
  check_regex = NULL;
  check_whole = NULL;
  model       = NULL;
  entry       = NULL;
  entry_max   = 0;

  terminal_find_set_parent (NULL);
}


/**
 * terminal_find_set_parent
 * @new_parent:  Dialog's new parent
 *
 * Keep track of the dialog's parent.  If it dies, we must die.
 * If find is called from another window, the dialog's parent might change.
 */
static void
terminal_find_set_parent (GtkWindow *new_parent)
{
  if (parent)
    g_signal_handlers_disconnect_by_func (parent, G_CALLBACK (terminal_find_window_cb), NULL);

  if (new_parent)
    g_signal_connect (new_parent, "destroy", G_CALLBACK (terminal_find_window_cb), NULL);

  parent = new_parent;
}


/**
 * terminal_find_dialog_display
 * @terminal_window: Parent to the Find Dialog
 *
 * Create and initialise the find dialog.  If the dialog already exists, we
 * simply make it visible.
 *
 * We listen for changes to the options in the dialog, changes to our settings
 * in GConf and we listen for the death of our parent.
 */
void
terminal_find_dialog_display (GtkWindow *terminal_window)
{
  GtkButton *button_close = NULL;
  GtkButton *button_find  = NULL;
  gboolean find_case;
  gboolean find_regex;
  gboolean find_whole;

  if (!terminal_window)
    return;

  if (dialog)
    {
      gtk_window_set_transient_for (GTK_WINDOW (dialog), terminal_window);
      gtk_window_present (GTK_WINDOW (dialog));
      terminal_find_set_parent (terminal_window);
      return;
    }

  gconf = gconf_client_get_default ();
  gconf_client_add_dir (gconf, CONF_FIND_PREFIX, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  if (!terminal_util_load_builder_file ("terminal-find.ui",
                                        "dialog-find",     &dialog,
                                        "check-case",      &check_case,
                                        "check-whole",     &check_whole,
                                        "check-regex",     &check_regex,
                                        "entry-find",      &entry,
                                        "button-close",    &button_close,
                                        "button-find",     &button_find,
                                        NULL))
    return;

  find_case  = gconf_client_get_bool (gconf, CONF_FIND_MATCH_CASE,  NULL);
  find_regex = gconf_client_get_bool (gconf, CONF_FIND_MATCH_REGEX, NULL);
  find_whole = gconf_client_get_bool (gconf, CONF_FIND_MATCH_WHOLE, NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_case),  find_case);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_regex), find_regex);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_whole), find_whole);

  terminal_find_history_init ();

  gtk_entry_set_icon_from_stock (entry, GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
  gtk_entry_set_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY, FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (button_find), FALSE);

  nid_case  = gconf_client_notify_add (gconf, CONF_FIND_MATCH_CASE,  terminal_find_check_cb, check_case,  NULL, NULL);
  nid_regex = gconf_client_notify_add (gconf, CONF_FIND_MATCH_REGEX, terminal_find_check_cb, check_regex, NULL, NULL);
  nid_whole = gconf_client_notify_add (gconf, CONF_FIND_MATCH_WHOLE, terminal_find_check_cb, check_whole, NULL, NULL);

  g_signal_connect (dialog,      "destroy",      G_CALLBACK (terminal_find_destroyed_cb), NULL);
  g_signal_connect (dialog,      "response",     G_CALLBACK (terminal_find_response_cb),  NULL);
  g_signal_connect (entry,       "icon-press",   G_CALLBACK (terminal_find_clear_cb),     NULL);
  g_signal_connect (check_case,  "toggled",      G_CALLBACK (terminal_find_toggled_cb),   NULL);
  g_signal_connect (check_regex, "toggled",      G_CALLBACK (terminal_find_toggled_cb),   NULL);
  g_signal_connect (check_whole, "toggled",      G_CALLBACK (terminal_find_toggled_cb),   NULL);
  g_signal_connect (entry,       "notify::text", G_CALLBACK (terminal_find_text_cb),      button_find);

  terminal_find_set_parent (terminal_window);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), terminal_window);
  gtk_window_present (GTK_WINDOW (dialog));
}


