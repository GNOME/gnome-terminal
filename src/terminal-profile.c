/* object representing a profile */

/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "terminal-profile.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>
#define _(x) gettext (x)

/* If you add a key, you need to update code:
 *  - in the function that sets the key
 *  - in the function that reads the key
 *  - in the function that copies base profiles to new profiles
 */
#define KEY_VISIBLE_NAME "visible_name"
#define KEY_CURSOR_BLINK "cursor_blink"
#define KEY_DEFAULT_SHOW_MENUBAR "default_show_menubar"

struct _TerminalProfilePrivate
{
  char *name;
  char *profile_dir;
  GConfClient *conf;
  guint notify_id;
  char *visible_name;
  guint cursor_blink : 1;
  guint default_show_menubar : 1;
  guint forgotten : 1;
};

static GHashTable *profiles = NULL;

enum {
  CHANGED,
  FORGOTTEN,
  LAST_SIGNAL
};

static void terminal_profile_init        (TerminalProfile      *profile);
static void terminal_profile_class_init  (TerminalProfileClass *klass);
static void terminal_profile_finalize    (GObject              *object);

static void profile_change_notify        (GConfClient *client,
                                          guint        cnxn_id,
                                          GConfEntry  *entry,
                                          gpointer     user_data);



static gpointer parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
terminal_profile_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();
  
  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (TerminalProfileClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) terminal_profile_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (TerminalProfile),
        0,              /* n_preallocs */
        (GInstanceInitFunc) terminal_profile_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "TerminalProfile",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
terminal_profile_init (TerminalProfile *profile)
{  
  profile->priv = g_new0 (TerminalProfilePrivate, 1);
  profile->priv->cursor_blink = FALSE;
  profile->priv->default_show_menubar = TRUE;
  profile->priv->visible_name = g_strdup ("");
}

static void
terminal_profile_class_init (TerminalProfileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = terminal_profile_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalProfileClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[FORGOTTEN] =
    g_signal_new ("forgotten",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalProfileClass, forgotten),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
terminal_profile_finalize (GObject *object)
{
  TerminalProfile *profile;

  profile = TERMINAL_PROFILE (object);

  terminal_profile_forget (profile);
  
  gconf_client_notify_remove (profile->priv->conf,
                              profile->priv->notify_id);
  profile->priv->notify_id = 0;

  g_object_unref (G_OBJECT (profile->priv->conf));

  g_free (profile->priv->visible_name);
  g_free (profile->priv->name);
  g_free (profile->priv->profile_dir);
  
  g_free (profile->priv);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

TerminalProfile*
terminal_profile_new (const char *name,
                      GConfClient *conf)
{
  TerminalProfile *profile;
  GError *err;
  
  profile = g_object_new (TERMINAL_TYPE_PROFILE, NULL);

  profile->priv->conf = conf;
  g_object_ref (G_OBJECT (conf));
  
  profile->priv->name = g_strdup (name);
  
  profile->priv->profile_dir = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX,
                                                         profile->priv->name);

  /*   g_print ("Watching dir %s\n", profile->priv->profile_dir); */
  
  err = NULL;
  profile->priv->notify_id =
    gconf_client_notify_add (conf,
                             profile->priv->profile_dir,
                             profile_change_notify,
                             profile,
                             NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal profile changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }

  if (profiles == NULL)
    profiles = g_hash_table_new (g_str_hash, g_str_equal);
  
  g_hash_table_insert (profiles, profile->priv->name, profile);
  
  return profile;
}

const char*
terminal_profile_get_name (TerminalProfile *profile)
{
  return profile->priv->name;
}

const char*
terminal_profile_get_visible_name (TerminalProfile *profile)
{
  if (strcmp (profile->priv->name, DEFAULT_PROFILE) == 0)
    return _(DEFAULT_PROFILE);
  else
    return profile->priv->visible_name;
}

gboolean
terminal_profile_get_forgotten (TerminalProfile *profile)
{
  return profile->priv->forgotten;
}

gboolean
terminal_profile_get_audible_bell       (TerminalProfile  *profile)
{
}

gboolean
terminal_profile_get_cursor_blink (TerminalProfile  *profile)
{
  return profile->priv->cursor_blink;
}

void
terminal_profile_set_cursor_blink (TerminalProfile *profile,
                                   gboolean         setting)
{
  char *key;
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_CURSOR_BLINK);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

gboolean
terminal_profile_get_scroll_on_keypress (TerminalProfile  *profile)
{
}
gboolean
terminal_profile_get_login_shell        (TerminalProfile  *profile)
{
}
gboolean
terminal_profile_get_scroll_background  (TerminalProfile  *profile)
{
}
gboolean
terminal_profile_get_use_bold           (TerminalProfile  *profile)
{
}
TerminalDeleteBinding
terminal_profile_get_delete_key         (TerminalProfile  *profile)
{
}
TerminalDeleteBinding
terminal_profile_get_backspace_key      (TerminalProfile  *profile)
{
}
TerminalPaletteType
terminal_profile_get_palette_type       (TerminalProfile  *profile)
{
}
TerminalScrollbarPosition
terminal_profile_get_scrollbar_position (TerminalProfile  *profile)
{
}
const char*
terminal_profile_get_font               (TerminalProfile  *profile)
{
}
int
terminal_profile_get_scrollback_lines   (TerminalProfile  *profile)
{
}
gboolean
terminal_profile_get_update_records     (TerminalProfile  *profile)
{
}
void
terminal_profile_get_color_scheme       (TerminalProfile  *profile,
                                         GdkColor         *foreground,
                                         GdkColor         *background)
{
}
void
terminal_profile_get_palette            (TerminalProfile  *profile,
                                         GdkColor        **colors,
                                         int               n_colors)
{
}

gboolean
terminal_profile_get_transparent        (TerminalProfile  *profile)
{
}

gboolean
terminal_profile_get_shaded             (TerminalProfile  *profile)
{
}

GdkPixmap*
terminal_profile_get_background_pixmap  (TerminalProfile  *profile)
{
}

const char*
terminal_profile_get_word_class         (TerminalProfile  *profile)
{
}

const char*
terminal_profile_get_term_variable      (TerminalProfile  *profile)
{
}

gboolean
terminal_profile_get_lock_title         (TerminalProfile  *profile)
{
}

const char*
terminal_profile_get_title              (TerminalProfile  *profile)
{
}

gboolean
terminal_profile_get_default_show_menubar (TerminalProfile *profile)
{
  return profile->priv->default_show_menubar;
}

void
terminal_profile_set_default_show_menubar (TerminalProfile *profile,
                                           gboolean         setting)
{
  char *key;
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_DEFAULT_SHOW_MENUBAR);
  
  gconf_client_set_bool (profile->priv->conf,
                         key,
                         setting,
                         NULL);

  g_free (key);
}

static gboolean
set_visible_name (TerminalProfile *profile,
                  const char      *candidate_name)
{
  if (candidate_name &&
      strcmp (profile->priv->visible_name, candidate_name) == 0)
    return FALSE;
  
  g_free (profile->priv->visible_name);
  
  if (candidate_name == NULL)
    {
      profile->priv->visible_name = g_strdup ("");
    }
  else
    {
      profile->priv->visible_name = g_strdup (candidate_name);
    }

  return TRUE;
}

void
terminal_profile_update (TerminalProfile *profile)
{
  char *key;
  gboolean changed;
  gboolean bool_val;
  char *str_val;

  changed = FALSE;

  /* KEY_CURSOR_BLINK */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_CURSOR_BLINK);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);
  /*   g_print ("cursor blink is now %d\n", bool_val); */
  if (bool_val != profile->priv->cursor_blink)
    {
      changed = TRUE;
      profile->priv->cursor_blink = bool_val;
    }

  g_free (key);

  /* KEY_DEFAULT_SHOW_MENUBAR */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_DEFAULT_SHOW_MENUBAR);
  bool_val = gconf_client_get_bool (profile->priv->conf,
                                    key, NULL);

  if (bool_val != profile->priv->default_show_menubar)
    {
      changed = TRUE;
      profile->priv->default_show_menubar = bool_val;
    }

  g_free (key);

  /* KEY_VISIBLE_NAME */
  
  key = gconf_concat_dir_and_key (profile->priv->profile_dir,
                                  KEY_VISIBLE_NAME);
  str_val = gconf_client_get_string (profile->priv->conf,
                                     key, NULL);

  changed |= set_visible_name (profile, str_val);

  g_free (key);
  
  if (changed)
    g_signal_emit (G_OBJECT (profile), signals[CHANGED], 0);
}


static const gchar*
find_key (const gchar* key)
{
  const gchar* end;
  
  end = strrchr(key, '/');

  ++end;

  return end;
}

static void
profile_change_notify (GConfClient *client,
                       guint        cnxn_id,
                       GConfEntry  *entry,
                       gpointer     user_data)
{
  TerminalProfile *profile;
  const char *key;
  GConfValue *val;
  gboolean changed;
  
  profile = TERMINAL_PROFILE (user_data);

  changed = FALSE;

  val = gconf_entry_get_value (entry);
  
  key = find_key (gconf_entry_get_key (entry));

  /*   g_print ("Key '%s' changed\n", key); */
  
  if (strcmp (key, KEY_CURSOR_BLINK) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->cursor_blink)
        {
          changed = TRUE;
          profile->priv->cursor_blink = bool_val;
        }
    }
  else if (strcmp (key, KEY_DEFAULT_SHOW_MENUBAR) == 0)
    {
      gboolean bool_val;

      bool_val = FALSE;

      if (val && val->type == GCONF_VALUE_BOOL)
        bool_val = gconf_value_get_bool (val);

      if (bool_val != profile->priv->default_show_menubar)
        {
          changed = TRUE;
          profile->priv->default_show_menubar = bool_val;
        }
    }
  else if (strcmp (key, KEY_VISIBLE_NAME) == 0)
    {
      const char *str_val;

      str_val = NULL;
      if (val && val->type == GCONF_VALUE_STRING)
        str_val = gconf_value_get_string (val);
      
      changed = set_visible_name (profile, str_val);
    }
  
  if (changed)
    g_signal_emit (G_OBJECT (profile), signals[CHANGED], 0);
}

static void
listify_foreach (gpointer key,
                 gpointer value,
                 gpointer data)
{
  GList **listp = data;

  *listp = g_list_prepend (*listp, value);
}

static int
alphabetic_cmp (gconstpointer a,
                gconstpointer b)
{
  TerminalProfile *ap = (TerminalProfile*) a;
  TerminalProfile *bp = (TerminalProfile*) b;

  return g_utf8_collate (terminal_profile_get_visible_name (ap),
                         terminal_profile_get_visible_name (bp));
}

GList*
terminal_profile_get_list (void)
{
  GList *list;

  list = NULL;
  g_hash_table_foreach (profiles, listify_foreach, &list);

  list = g_list_sort (list, alphabetic_cmp);
  
  return list;
}

TerminalProfile*
terminal_profile_lookup (const char *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  if (profiles)
    return g_hash_table_lookup (profiles, name);
  else
    return NULL;
}

typedef struct
{
  TerminalProfile *result;
  const char *target;
} LookupInfo;

static void
lookup_by_visible_name_foreach (gpointer key,
                                gpointer value,
                                gpointer data)
{
  LookupInfo *info = data;

  if (strcmp (info->target, terminal_profile_get_visible_name (value)) == 0)
    info->result = value;
}

TerminalProfile*
terminal_profile_lookup_by_visible_name (const char *name)
{
  LookupInfo info;

  info.result = NULL;
  info.target = name;

  if (profiles)
    {
      g_hash_table_foreach (profiles, lookup_by_visible_name_foreach, &info);
      return info.result;
    }
  else
    return NULL;
}

void
terminal_profile_forget (TerminalProfile *profile)
{
  if (!profile->priv->forgotten)
    {
      g_hash_table_remove (profiles, profile->priv->name);
      profile->priv->forgotten = TRUE;
      g_signal_emit (G_OBJECT (profile), signals[FORGOTTEN], 0);
    }
}

void
terminal_profile_setup_default (GConfClient *conf)
{
  TerminalProfile *profile;

  profile = terminal_profile_new (DEFAULT_PROFILE, conf);

  terminal_profile_update (profile);
}

/* Function I'm cut-and-pasting everywhere, this is from msm */
void
dialog_add_details (GtkDialog  *dialog,
                    const char *details)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkRequisition req;
  
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
  
  gtk_box_pack_start (GTK_BOX (dialog->vbox),
                      hbox,
                      FALSE, FALSE, 0);

  button = gtk_button_new_with_mnemonic (_("_Details"));
  
  gtk_box_pack_end (GTK_BOX (hbox), button,
                    FALSE, FALSE, 0);
  
  label = gtk_label_new (details);

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  
  gtk_box_pack_start (GTK_BOX (hbox), label,
                      TRUE, TRUE, 0);

  /* show the label on click */
  g_signal_connect_swapped (G_OBJECT (button),
                            "clicked",
                            G_CALLBACK (gtk_widget_show),
                            label);
  
  /* second callback destroys the button (note disconnects first callback) */
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (gtk_widget_destroy),
                    NULL);

  /* Set default dialog size to size with the label,
   * and without the button, but then rehide the label
   */
  gtk_widget_show_all (hbox);

  gtk_widget_size_request (GTK_WIDGET (dialog), &req);

  gtk_window_set_default_size (GTK_WINDOW (dialog), req.width, req.height);
  
  gtk_widget_hide (label);
}

void
terminal_profile_create (TerminalProfile *base_profile,
                         const char      *visible_name,
                         GtkWindow       *transient_parent)
{
  char *profile_name = NULL;
  char *profile_dir = NULL;
  int i;
  char *s;
  char *key = NULL;
  GError *err = NULL;
  GList *profiles = NULL;
  GSList *name_list = NULL;
  GList *tmp;  

  /* This is for extra bonus paranoia against CORBA reentrancy */
  g_object_ref (G_OBJECT (base_profile));
  g_object_ref (G_OBJECT (transient_parent));

#define BAIL_OUT_CHECK() do {                           \
    if (!GTK_WIDGET_VISIBLE (transient_parent) ||       \
        base_profile->priv->forgotten ||                \
        err != NULL)                                    \
       goto cleanup;                                    \
  } while (0) 
  
  /* Pick a unique name for storing in gconf (based on visible name) */
  profile_name = gconf_escape_key (visible_name, -1);

  s = g_strdup (profile_name);
  i = 0;
  while (terminal_profile_lookup (s))
    {
      g_free (s);
      
      s = g_strdup_printf ("%s-%d", profile_name, i);

      ++i;
    }

  g_free (profile_name);
  profile_name = s;

  profile_dir = gconf_concat_dir_and_key (CONF_PROFILES_PREFIX, 
                                          profile_name);
  
  /* Store a copy of base profile values at under that directory */

  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_VISIBLE_NAME);

  gconf_client_set_string (base_profile->priv->conf,
                           key,
                           visible_name,
                           &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_CURSOR_BLINK);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->cursor_blink,
                         &err);
  BAIL_OUT_CHECK ();

  g_free (key);
  key = gconf_concat_dir_and_key (profile_dir,
                                  KEY_DEFAULT_SHOW_MENUBAR);

  gconf_client_set_bool (base_profile->priv->conf,
                         key,
                         base_profile->priv->default_show_menubar,
                         &err);

  BAIL_OUT_CHECK ();
  
  /* Add new profile to the profile list; the method for doing this has
   * a race condition where we and someone else set at the same time,
   * but I am just going to punt on this issue.
   */
  profiles = terminal_profile_get_list ();
  tmp = profiles;
  while (tmp != NULL)
    {
      name_list = g_slist_prepend (name_list,
                                   g_strdup (terminal_profile_get_name (tmp->data)));
      
      tmp = tmp->next;
    }

  name_list = g_slist_prepend (name_list, g_strdup (profile_name));
  
  gconf_client_set_list (base_profile->priv->conf,
                         CONF_GLOBAL_PREFIX"/profile_list",
                         GCONF_VALUE_STRING,
                         name_list,
                         &err);

  BAIL_OUT_CHECK ();
  
 cleanup:
  g_free (profile_name);
  g_free (profile_dir);
  g_free (key);

  g_list_free (profiles);

  if (name_list)
    {
      g_slist_foreach (name_list, (GFunc) g_free, NULL);
      g_slist_free (name_list);
    }
  
  if (err)
    {
      if (GTK_WIDGET_VISIBLE (transient_parent))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("There was an error creating the profile \"%s\""),
                                           visible_name);
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          dialog_add_details (GTK_DIALOG (dialog),
                              err->message);
          
          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show (dialog);
        }

      g_error_free (err);
    }
  
  g_object_unref (G_OBJECT (base_profile));
  g_object_unref (G_OBJECT (transient_parent));
}

void
terminal_profile_delete_list (GConfClient *conf,
                              GList       *deleted_profiles,
                              GtkWindow   *transient_parent)
{
  GList *current_profiles;
  GList *tmp;
  GSList *name_list;
  GError *err;

  /* reentrancy paranoia */
  g_object_ref (G_OBJECT (transient_parent));
  
  current_profiles = terminal_profile_get_list ();  

  /* remove deleted profiles from list */
  tmp = deleted_profiles;
  while (tmp != NULL)
    {
      TerminalProfile *profile = tmp->data;
      
      current_profiles = g_list_remove (current_profiles, profile);

      g_print ("Deleting profile '%s'\n",
               terminal_profile_get_visible_name (profile));
      
      tmp = tmp->next;
    }

  /* make list of profile names */
  name_list = NULL;
  tmp = current_profiles;
  while (tmp != NULL)
    {
      name_list = g_slist_prepend (name_list,
                                   g_strdup (terminal_profile_get_name (tmp->data)));
      
      tmp = tmp->next;
    }

  g_list_free (current_profiles);

  err = NULL;
  gconf_client_set_list (conf,
                         CONF_GLOBAL_PREFIX"/profile_list",
                         GCONF_VALUE_STRING,
                         name_list,
                         &err);

  g_slist_foreach (name_list, (GFunc) g_free, NULL);
  g_slist_free (name_list);

  if (err)
    {
      if (GTK_WIDGET_VISIBLE (transient_parent))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("There was an error deleting the profiles"));
          g_signal_connect (G_OBJECT (dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);

          dialog_add_details (GTK_DIALOG (dialog),
                              err->message);
          
          gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
          
          gtk_widget_show (dialog);
        }

      g_error_free (err);
    }

  g_object_unref (G_OBJECT (transient_parent));
}
