/* dialog for editing a profile */

/*
 * Copyright (C) 2002 Havoc Pennington
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

#include "profile-editor.h"
#include "terminal-intl.h"

static void
profile_editor_destroyed (GtkWidget       *editor,
                          TerminalProfile *profile)
{
  g_object_set_data (G_OBJECT (profile), "editor-window", NULL);
}

static void
profile_forgotten (TerminalProfile *profile,
                   GtkWidget       *editor)
{
  /* FIXME this might confuse users a bit, if it ever happens (unlikely
   * to actually happen much)
   */
  gtk_widget_destroy (editor);
}

static void
cursor_blink_toggled (GtkWidget       *checkbutton,
                      TerminalProfile *profile)
{
  terminal_profile_set_cursor_blink (profile,
                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

void
terminal_profile_edit (TerminalProfile *profile,
                       GtkWindow       *transient_parent)
{
  GtkWidget *editor;
  GtkWidget *checkbutton;
  GtkWindow *old_transient_parent;
  
  editor = g_object_get_data (G_OBJECT (profile),
                              "editor-window");

  if (editor == NULL)
    {
      /* FIXME this is all just placeholder cruft, need to get glade
       * file from gnome-terminal
       */
      char *s;

      old_transient_parent = NULL;
      
      editor = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      s = g_strdup_printf (_("Editing profile \"%s\""),
                           terminal_profile_get_visible_name (profile));
      
      gtk_window_set_title (GTK_WINDOW (editor), s);

      g_free (s);
      
      g_object_set_data (G_OBJECT (profile),
                         "editor-window",
                         editor);
      
      checkbutton = gtk_check_button_new_with_mnemonic (_("_Cursor blinks"));
      gtk_container_add (GTK_CONTAINER (editor), checkbutton);
  
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton),
                                    terminal_profile_get_cursor_blink (profile));

      g_signal_connect (G_OBJECT (checkbutton),
                        "toggled",
                        G_CALLBACK (cursor_blink_toggled),
                        profile);
  
      g_signal_connect (G_OBJECT (editor),
                        "destroy",
                        G_CALLBACK (profile_editor_destroyed), profile);

      g_signal_connect (G_OBJECT (profile),
                        "forgotten",
                        G_CALLBACK (profile_forgotten), editor);

      gtk_window_set_type_hint (GTK_WINDOW (editor),
                                GDK_WINDOW_TYPE_HINT_DIALOG);

      gtk_window_set_destroy_with_parent (GTK_WINDOW (editor), TRUE);
    }
  else
    {
      old_transient_parent = gtk_window_get_transient_for (GTK_WINDOW (editor));
    }

  if (old_transient_parent != transient_parent)
    {
      gtk_window_set_transient_for (GTK_WINDOW (editor),
                                    GTK_WINDOW (transient_parent));
      gtk_widget_hide (editor); /* re-show the window on its new parent */
    }
  
  gtk_widget_show_all (editor);
  gtk_window_present (GTK_WINDOW (editor));
}

