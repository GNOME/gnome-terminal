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
#include <glade/glade.h>

static void profile_editor_update_title (GtkWidget       *editor,
                                         TerminalProfile *profile);

static void profile_editor_update_sensitivity (GtkWidget       *editor,
                                               TerminalProfile *profile);

static void
profile_editor_destroyed (GtkWidget       *editor,
                          TerminalProfile *profile)
{
  g_object_set_data (G_OBJECT (profile), "editor-window", NULL);
  g_object_set_data (G_OBJECT (editor), "glade-xml", NULL);
}

/*
 * Profile callbacks
 */

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
profile_changed (TerminalProfile          *profile,
                 TerminalSettingMask       mask,
                 GtkWidget                *editor)
{
  if (mask & TERMINAL_SETTING_VISIBLE_NAME)
    profile_editor_update_title (editor, profile);

  profile_editor_update_sensitivity (editor, profile);
}

/*
 * Widget callbacks
 */

static void
visible_name_changed (GtkWidget       *entry,
                      TerminalProfile *profile)
{
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  
  terminal_profile_set_visible_name (profile, text);

  g_free (text);
}

static void
cursor_blink_toggled (GtkWidget       *checkbutton,
                      TerminalProfile *profile)
{
  terminal_profile_set_cursor_blink (profile,
                                     gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

static void
show_menubar_toggled (GtkWidget       *checkbutton,
                      TerminalProfile *profile)
{
  terminal_profile_set_default_show_menubar (profile,
                                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)));
}

void
terminal_profile_edit (TerminalProfile *profile,
                       GtkWindow       *transient_parent)
{
  GtkWidget *editor;
  GtkWindow *old_transient_parent;
  
  editor = g_object_get_data (G_OBJECT (profile),
                              "editor-window");

  if (editor == NULL)
    {
      GladeXML *xml;
      GtkWidget *w;
      
      if (g_file_test ("./"PROFTERM_GLADE_FILE,
                       G_FILE_TEST_EXISTS))
        {
          /* Try current dir, for debugging */
          xml = glade_xml_new ("./"PROFTERM_GLADE_FILE,
                               "profile-editor-dialog",
                               GETTEXT_PACKAGE);
        }
      else
        {
          xml = glade_xml_new (PROFTERM_GLADE_DIR"/"PROFTERM_GLADE_FILE,
                               "profile-editor-dialog",
                               GETTEXT_PACKAGE);
        }

      if (xml == NULL)
        {
          static GtkWidget *no_glade_dialog = NULL;

          if (no_glade_dialog == NULL)
            {
              no_glade_dialog =
                gtk_message_dialog_new (transient_parent,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("The file \"%s\" is missing. This indicates that the application is installed incorrectly, so the profile editor can't be displayed."),
                                        PROFTERM_GLADE_DIR"/"PROFTERM_GLADE_FILE);
                                        
              g_signal_connect (G_OBJECT (no_glade_dialog),
                                "response",
                                G_CALLBACK (gtk_widget_destroy),
                                NULL);

              g_object_add_weak_pointer (G_OBJECT (no_glade_dialog),
                                         (void**)&no_glade_dialog);
            }

          gtk_window_present (GTK_WINDOW (no_glade_dialog));

          return;
        }
      
      old_transient_parent = NULL;
      
      editor = glade_xml_get_widget (xml, "profile-editor-dialog");

      
      /* Begin "we have no Glade 2" workarounds */
      gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
      
      gtk_dialog_add_buttons (GTK_DIALOG (editor),
                              _("_Done"), GTK_RESPONSE_ACCEPT,
                              NULL);
      /* End "we have no Glade 2" workarounds */


      
      g_object_set_data (G_OBJECT (profile),
                         "editor-window",
                         editor);      

      g_object_set_data_full (G_OBJECT (editor),
                              "glade-xml",
                              xml,
                              (GDestroyNotify) g_object_unref);
      
      g_signal_connect (G_OBJECT (editor),
                        "destroy",
                        G_CALLBACK (profile_editor_destroyed),
                        profile);

      g_signal_connect (G_OBJECT (editor),
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      
      gtk_window_set_destroy_with_parent (GTK_WINDOW (editor), TRUE);

      g_signal_connect (G_OBJECT (profile),
                        "changed",
                        G_CALLBACK (profile_changed),
                        editor);
      
      g_signal_connect (G_OBJECT (profile),
                        "forgotten",
                        G_CALLBACK (profile_forgotten),
                        editor);
      
      profile_editor_update_title (editor, profile);
      profile_editor_update_sensitivity (editor, profile);

      /* Autoconnect is just too scary for me. */
      w = glade_xml_get_widget (xml, "profile-name-entry");
      gtk_entry_set_text (GTK_ENTRY (w),
                          terminal_profile_get_visible_name (profile));
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (visible_name_changed),
                        profile);

      w = glade_xml_get_widget (xml, "blink-cursor-checkbutton");
      gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w),
                                   terminal_profile_get_cursor_blink (profile));
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (cursor_blink_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "show-menubar-checkbutton");
      gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w),
                                   terminal_profile_get_default_show_menubar (profile));
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (show_menubar_toggled),
                        profile);
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

static void
profile_editor_update_title (GtkWidget       *editor,
                             TerminalProfile *profile)
{
  char *s;
  
  s = g_strdup_printf (_("Editing profile \"%s\""),
                       terminal_profile_get_visible_name (profile));
  
  gtk_window_set_title (GTK_WINDOW (editor), s);
  
  g_free (s);
}

static void
set_insensitive (GtkWidget  *editor,
                 const char *widget_name,
                 gboolean    setting)
{
  GladeXML *xml;
  GtkWidget *w;
  
  xml = g_object_get_data (G_OBJECT (editor),
                           "glade-xml");

  g_return_if_fail (xml);

  w = glade_xml_get_widget (xml, widget_name);

  gtk_widget_set_sensitive (w, !setting);
}

static void
profile_editor_update_sensitivity (GtkWidget       *editor,
                                   TerminalProfile *profile)
{
  TerminalSettingMask mask;

  mask = terminal_profile_get_locked_settings (profile);

  set_insensitive (editor, "profile-name-entry",
                   mask & TERMINAL_SETTING_VISIBLE_NAME);
  
  set_insensitive (editor, "blink-cursor-checkbutton",
                   mask & TERMINAL_SETTING_CURSOR_BLINK);

  set_insensitive (editor, "show-menubar-checkbutton",
                   mask & TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR);
}
