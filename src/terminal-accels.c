/* Accelerator stuff */

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

#include "terminal-intl.h"
#include "terminal-accels.h"
#include "terminal-profile.h"
#include <string.h>

#define D(x) x

#define KEY_NEW_TAB CONF_KEYS_PREFIX"/new_tab"

typedef struct
{
  const char *gconf_key;
  const char *accel_path;
  /* last values received from gconf */
  guint gconf_keyval;
  GdkModifierType gconf_mask;
  /* last values received from gtk */
  guint gtk_keyval;
  GdkModifierType gtk_mask;
  GClosure *closure;
  /* have gotten a notification from gtk */
  gboolean needs_gconf_sync;
} KeyEntry;

static KeyEntry entries[] = {
  { KEY_NEW_TAB, ACCEL_PATH_NEW_TAB, 0, 0, 0, 0, NULL, FALSE }
};

/*
 * This is kind of annoying. We have two sources of keybinding change;
 * GConf and GtkAccelMap. GtkAccelMap will change if the user uses
 * the magic in-place editing mess. If accel map changes, we propagate
 * into GConf. If GConf changes we propagate into accel map.
 * To avoid infinite loop hell, we short-circuit in both directions
 * if the value is unchanged from last known.
 * The short-circuit is also required because of:
 *  http://bugzilla.gnome.org/show_bug.cgi?id=73082
 *
 *  We have to keep our own hash of the current values in order to
 *  do this short-circuit stuff.
 */

static void keys_change_notify (GConfClient *client,
                                guint        cnxn_id,
                                GConfEntry  *entry,
                                gpointer     user_data);

static void accel_changed_callback (GtkAccelGroup  *accel_group,
                                    guint           keyval,
                                    GdkModifierType modifier,
                                    GClosure       *accel_closure,
                                    gpointer        data);

static gboolean binding_from_string (const char      *str,
                                     guint           *accelerator_key,
                                     GdkModifierType *accelerator_mods);

static gboolean binding_from_value  (GConfValue       *value,
                                     guint           *accelerator_key,
                                     GdkModifierType *accelerator_mods);

static void      queue_gconf_sync (void);

static GtkAccelGroup * /* accel_group_i_need_because_gtk_accel_api_sucks */ hack_group = NULL;
static GConfClient *global_conf;

void
terminal_accels_init (GConfClient *conf)
{
  GError *err;
  int i;

  g_return_if_fail (conf != NULL);
  g_return_if_fail (global_conf == NULL);
  
  global_conf = conf;
  g_object_ref (G_OBJECT (global_conf));
  
  err = NULL;
  gconf_client_add_dir (conf, CONF_KEYS_PREFIX,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        &err);
  if (err)
    {
      g_printerr (_("There was an error loading config from %s. (%s)\n"),
                  CONF_KEYS_PREFIX, err->message);
      g_error_free (err);
    }

  err = NULL;
  gconf_client_notify_add (conf,
                           CONF_KEYS_PREFIX,
                           keys_change_notify,
                           NULL, /* user_data */
                           NULL, &err);
  
  if (err)
    {
      g_printerr (_("There was an error subscribing to notification of terminal keybinding changes. (%s)\n"),
                  err->message);
      g_error_free (err);
    }
  
  hack_group = gtk_accel_group_new ();

  i = 0;
  while (i < (int) G_N_ELEMENTS (entries))
    {
      char *str;
      guint keyval;
      GdkModifierType mask;
      
      entries[i].closure = g_closure_new_simple (sizeof (GClosure),
                                                 NULL);

      g_closure_ref (entries[i].closure);
      g_closure_sink (entries[i].closure);

      gtk_accel_group_connect_by_path (hack_group,
                                       entries[i].accel_path,
                                       entries[i].closure);
      
      /* Copy from gconf to GTK */
      
      /* FIXME handle whether the entry is writable
       *  http://bugzilla.gnome.org/show_bug.cgi?id=73207
       */

      err = NULL;
      str = gconf_client_get_string (conf, entries[i].gconf_key,
                                     &err);
      if (err != NULL)
        {
          g_printerr (_("There was an error loading a terminal keybinding. (%s)\n"),
                      err->message);
          g_error_free (err);
        }

      if (binding_from_string (str, &keyval, &mask))
        {
          entries[i].gconf_keyval = keyval;
          entries[i].gconf_mask = mask;
          entries[i].gtk_keyval = keyval;
          entries[i].gtk_mask = mask;
          
          gtk_accel_map_change_entry (entries[i].accel_path,
                                      keyval, mask,
                                      TRUE);
        }
      else
        {
          g_printerr (_("The value of configuration key %s is not valid\n"),
                      entries[i].gconf_key);
        }

      g_free (str);
      
      ++i;
    }
  
  g_signal_connect (G_OBJECT (hack_group),
                    "accel_changed",
                    G_CALLBACK (accel_changed_callback),
                    NULL);
}

GtkAccelGroup*
terminal_accels_get_accel_group (void)
{
  return hack_group;
}

static void
keys_change_notify (GConfClient *client,
                    guint        cnxn_id,
                    GConfEntry  *entry,
                    gpointer     user_data)
{
  GConfValue *val;
  GdkModifierType mask;
  guint keyval;

  /* FIXME handle whether the entry is writable
   *  http://bugzilla.gnome.org/show_bug.cgi?id=73207
   */

  D (g_print ("key %s changed\n", gconf_entry_get_key (entry)));
  
  val = gconf_entry_get_value (entry);

  if (binding_from_value (val, &keyval, &mask))
    {
      int i;

      i = 0;
      while (i < (int) G_N_ELEMENTS (entries))
        {
          if (strcmp (entries[i].gconf_key, gconf_entry_get_key (entry)) == 0)
            {
              /* found it */
              entries[i].gconf_keyval = keyval;
              entries[i].gconf_mask = mask;

              /* sync over to GTK */
              gtk_accel_map_change_entry (entries[i].accel_path,
                                          keyval, mask,
                                          TRUE);
              
              break;
            }
          
          ++i;
        }
    }
}

static void
accel_changed_callback (GtkAccelGroup  *accel_group,
                        guint           keyval,
                        GdkModifierType modifier,
                        GClosure       *accel_closure,
                        gpointer        data)
{
  /* FIXME because GTK accel API is so nonsensical, we get
   * a notify for each closure, on both the added and the removed
   * accelerator. We just use the accel closure to find our
   * accel entry, then update the value of that entry.
   * We use an idle function to avoid setting the entry
   * in gconf when the accelerator gets removed and then
   * setting it again when it gets added.
   */
  int i;
  
  D (g_print ("Changed accel %s closure %p\n",
              gtk_accelerator_name (keyval, modifier),
              accel_closure));


  i = 0;
  while (i < (int) G_N_ELEMENTS (entries))
    {
      if (entries[i].closure == accel_closure)
        {
          entries[i].needs_gconf_sync = TRUE;
          entries[i].gtk_keyval = keyval;
          entries[i].gtk_mask = modifier;
          queue_gconf_sync ();
          break;
        }

      ++i;
    }
}

static gboolean
binding_from_string (const char      *str,
                     guint           *accelerator_key,
                     GdkModifierType *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);

  if (str == NULL)
    {
      *accelerator_key = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  gtk_accelerator_parse (str, accelerator_key, accelerator_mods);

  if (*accelerator_key == 0)
    return FALSE;
  else
    return TRUE;
}

static gboolean
binding_from_value (GConfValue       *value,
                    guint            *accelerator_key,
                    GdkModifierType  *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);
  
  if (value == NULL)
    {
      /* unset */
      *accelerator_key = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  if (value->type != GCONF_VALUE_STRING)
    return FALSE;

  return binding_from_string (gconf_value_get_string (value),
                              accelerator_key,
                              accelerator_mods);
}

static guint sync_idle = 0;

static gboolean
sync_handler (gpointer data)
{
  int i;

  D (g_print ("gconf sync handler\n"));
  
  sync_idle = 0;

  i = 0;
  while (i < (int) G_N_ELEMENTS (entries))
    {
      if (entries[i].needs_gconf_sync)
        {
          entries[i].needs_gconf_sync = FALSE;
          
          if (entries[i].gtk_keyval != entries[i].gconf_keyval ||
              entries[i].gtk_mask != entries[i].gconf_mask)
            {
              GError *err;

              err = NULL;
              gconf_client_set_string (global_conf,
                                       entries[i].gconf_key,
                                       gtk_accelerator_name (entries[i].gtk_keyval,
                                                             entries[i].gtk_mask),
                                       &err);
              if (err != NULL)
                {
                  g_printerr (_("Error propagating accelerator change to configuration database: %s\n"),
                              err->message);

                  g_error_free (err);
                }
            }
        }
      
      ++i;
    }  
  
  return FALSE;
}

static void
queue_gconf_sync (void)
{
  if (sync_idle == 0)
    sync_idle = g_idle_add (sync_handler, NULL);
}
