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
#include <libgnomeui/gnome-color-picker.h>

typedef struct _TerminalColorScheme TerminalColorScheme;

struct _TerminalColorScheme
{
  const char *name;
  GdkColor foreground;
  GdkColor background;
};
  
static TerminalColorScheme color_schemes[] = {
  { N_("Black on light yellow"),
    { 0, 0x0000, 0x0000, 0x0000 }, { 0, 0xFFFF, 0xFFFF, 0xDDDD } },
  { N_("White on black"),
    { 0, 0xFFFF, 0xFFFF, 0xFFFF }, { 0, 0x0000, 0x0000, 0x0000 } },
  { N_("Black on white"),
    { 0, 0x0000, 0x0000, 0x0000 }, { 0, 0xFFFF, 0xFFFF, 0xFFFF } },
  { N_("Green on black"),
    { 0, 0x0000, 0xFFFF, 0x0000 }, { 0, 0x0000, 0x0000, 0x0000 } }  
};

static GtkWidget* profile_editor_get_widget                  (GtkWidget       *editor,
                                                              const char      *widget_name);
static void       profile_editor_update_sensitivity          (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_visible_name         (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_cursor_blink         (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_default_show_menubar (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_color_pickers        (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_color_scheme_menu    (GtkWidget       *editor,
                                                              TerminalProfile *profile);

static void       profile_editor_update_title                (GtkWidget       *editor,
                                                              TerminalProfile *profile);
static void       profile_editor_update_title_mode           (GtkWidget       *editor,
                                                              TerminalProfile *profile);

static void
entry_set_text_if_changed (GtkEntry   *entry,
                           const char *text)
{
  char *s;

  s = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  if (strcmp (s, text) != 0)
    gtk_entry_set_text (GTK_ENTRY (entry), text);

  g_free (s);
}

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
    profile_editor_update_visible_name (editor, profile);

  if (mask & TERMINAL_SETTING_CURSOR_BLINK)
    profile_editor_update_cursor_blink (editor, profile);

  if (mask & TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR)
    profile_editor_update_default_show_menubar (editor, profile);

  if ((mask & TERMINAL_SETTING_FOREGROUND_COLOR) ||
      (mask & TERMINAL_SETTING_BACKGROUND_COLOR))
    {
      profile_editor_update_color_scheme_menu (editor, profile);
      profile_editor_update_color_pickers (editor, profile);
    }

  if (mask & TERMINAL_SETTING_TITLE)
    profile_editor_update_title (editor, profile);
  
  if (mask & TERMINAL_SETTING_TITLE_MODE)
    profile_editor_update_title_mode (editor, profile);
  
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

static void
foreground_color_set (GtkWidget       *colorpicker,
                      guint r, guint g, guint b, guint a,
                      TerminalProfile *profile)
{
  GdkColor color;
  GdkColor bg;

  color.red = r;
  color.green = g;
  color.blue = b;

  terminal_profile_get_color_scheme (profile,
                                     NULL, &bg);
  
  terminal_profile_set_color_scheme (profile, &color, &bg);
}

static void
background_color_set (GtkWidget       *colorpicker,
                      guint r, guint g, guint b, guint a,
                      TerminalProfile *profile)
{
  GdkColor color;
  GdkColor fg;

  color.red = r;
  color.green = g;
  color.blue = b;

  terminal_profile_get_color_scheme (profile,
                                     &fg, NULL);
  
  terminal_profile_set_color_scheme (profile, &fg, &color);
}

static void
color_scheme_changed (GtkWidget       *option_menu,
                      TerminalProfile *profile)
{
  int i;
  
  i = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
  
  if (i < G_N_ELEMENTS (color_schemes))
    terminal_profile_set_color_scheme (profile,
                                       &color_schemes[i].foreground,
                                       &color_schemes[i].background);
  else
    ; /* "custom" selected, no change */
}

static void
title_changed (GtkWidget       *entry,
               TerminalProfile *profile)
{
  char *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  
  terminal_profile_set_title (profile, text);

  g_free (text);
}

static void
title_mode_changed (GtkWidget       *option_menu,
                    TerminalProfile *profile)
{
  int i;
  
  i = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));

  terminal_profile_set_title_mode (profile, i);
}

/*
 * initialize widgets
 */

static void
init_color_scheme_menu (GtkWidget *option_menu)
{
  GtkWidget *menu;
  GtkWidget *menu_item;
  int i;
  
  menu = gtk_menu_new ();

  i = 0;
  while (i < G_N_ELEMENTS (color_schemes))
    {
      menu_item = gtk_menu_item_new_with_label (_(color_schemes[i].name));

      gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                             menu_item);
      
      ++i;
    }

  menu_item = gtk_menu_item_new_with_label (_("Custom"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                         menu_item);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu),
                            menu);
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

      profile_editor_update_sensitivity (editor, profile);

      /* Autoconnect is just too scary for me. */
      w = glade_xml_get_widget (xml, "profile-name-entry");
      profile_editor_update_visible_name (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (visible_name_changed),
                        profile);

      w = glade_xml_get_widget (xml, "blink-cursor-checkbutton");
      profile_editor_update_cursor_blink (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (cursor_blink_toggled),
                        profile);

      w = glade_xml_get_widget (xml, "show-menubar-checkbutton");
      profile_editor_update_default_show_menubar (editor, profile);
      g_signal_connect (G_OBJECT (w), "toggled",
                        G_CALLBACK (show_menubar_toggled),
                        profile);

      profile_editor_update_color_pickers (editor, profile);
      
      w = glade_xml_get_widget (xml, "foreground-colorpicker");
      g_signal_connect (G_OBJECT (w), "color_set",
                        G_CALLBACK (foreground_color_set),
                        profile);

      w = glade_xml_get_widget (xml, "background-colorpicker");
      g_signal_connect (G_OBJECT (w), "color_set",
                        G_CALLBACK (background_color_set),
                        profile);

      w = glade_xml_get_widget (xml, "color-scheme-optionmenu");
      init_color_scheme_menu (w);
      profile_editor_update_color_scheme_menu (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (color_scheme_changed),
                        profile);

      w = glade_xml_get_widget (xml, "title-entry");
      profile_editor_update_title (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (title_changed),
                        profile);

      w = glade_xml_get_widget (xml, "title-mode-optionmenu");
      profile_editor_update_title_mode (editor, profile);
      g_signal_connect (G_OBJECT (w), "changed",
                        G_CALLBACK (title_mode_changed),
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
set_insensitive (GtkWidget  *editor,
                 const char *widget_name,
                 gboolean    setting)
{
  GtkWidget *w;  

  w = profile_editor_get_widget (editor, widget_name);

  gtk_widget_set_sensitive (w, !setting);
}

static void
profile_editor_update_sensitivity (GtkWidget       *editor,
                                   TerminalProfile *profile)
{
  TerminalSettingMask mask;

  /* FIXME cache last mask, to avoid doing all the work
   * if it hasn't changed
   */
  mask = terminal_profile_get_locked_settings (profile);

  set_insensitive (editor, "profile-name-entry",
                   mask & TERMINAL_SETTING_VISIBLE_NAME);
  
  set_insensitive (editor, "blink-cursor-checkbutton",
                   mask & TERMINAL_SETTING_CURSOR_BLINK);

  set_insensitive (editor, "show-menubar-checkbutton",
                   mask & TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR);
  
  set_insensitive (editor, "foreground-colorpicker",
                   mask & TERMINAL_SETTING_FOREGROUND_COLOR);

  set_insensitive (editor, "background-colorpicker",
                   mask & TERMINAL_SETTING_BACKGROUND_COLOR);

  set_insensitive (editor, "color-scheme-optionmenu",
                   (mask & (TERMINAL_SETTING_BACKGROUND_COLOR |
                            TERMINAL_SETTING_FOREGROUND_COLOR)));

  set_insensitive (editor, "title-entry",
                   mask & TERMINAL_SETTING_TITLE);
  
  set_insensitive (editor, "title-mode-optionmenu",
                   mask & TERMINAL_SETTING_TITLE_MODE);
}


static void
profile_editor_update_visible_name (GtkWidget       *editor,
                                    TerminalProfile *profile)
{
  char *s;
  GtkWidget *w;
  
  s = g_strdup_printf (_("Editing profile \"%s\""),
                       terminal_profile_get_visible_name (profile));
  
  gtk_window_set_title (GTK_WINDOW (editor), s);
  
  g_free (s);

  w = profile_editor_get_widget (editor, "profile-name-entry");

  entry_set_text_if_changed (GTK_ENTRY (w),
                             terminal_profile_get_visible_name (profile));
}

static void
profile_editor_update_cursor_blink (GtkWidget       *editor,
                                    TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "blink-cursor-checkbutton");
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w),
                               terminal_profile_get_cursor_blink (profile));
}


static void
profile_editor_update_default_show_menubar (GtkWidget       *editor,
                                            TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "show-menubar-checkbutton");
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (w),
                               terminal_profile_get_default_show_menubar (profile));
}

static void
profile_editor_update_color_pickers (GtkWidget       *editor,
                                     TerminalProfile *profile)
{
  GtkWidget *w;
  GdkColor color1, color2;
  
  terminal_profile_get_color_scheme (profile, &color1, &color2);
  
  w = profile_editor_get_widget (editor, "foreground-colorpicker");
  gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (w),
                              color1.red, color1.green, color1.blue, 0);
  
  w = profile_editor_get_widget (editor, "background-colorpicker");
  gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (w),
                              color2.red, color2.green, color2.blue, 0);
}

static void
profile_editor_update_color_scheme_menu (GtkWidget       *editor,
                                         TerminalProfile *profile)
{
  GdkColor fg, bg;
  int i;
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "color-scheme-optionmenu");
  
  terminal_profile_get_color_scheme (profile, &fg, &bg);

  i = 0;
  while (i < G_N_ELEMENTS (color_schemes))
    {
      if (gdk_color_equal (&color_schemes[i].foreground,
                           &fg) &&
          gdk_color_equal (&color_schemes[i].background,
                           &bg))
        break;
      ++i;
    }

  /* If we didn't find a match, then we want the last option
   * menu item which is "custom"
   */
  gtk_option_menu_set_history (GTK_OPTION_MENU (w), i);
}

static void
profile_editor_update_title (GtkWidget       *editor,
                             TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "title-entry");

  entry_set_text_if_changed (GTK_ENTRY (w),
                             terminal_profile_get_title (profile));
}

static void
profile_editor_update_title_mode (GtkWidget       *editor,
                                  TerminalProfile *profile)
{
  GtkWidget *w;

  w = profile_editor_get_widget (editor, "title-mode-optionmenu");
  
  gtk_option_menu_set_history (GTK_OPTION_MENU (w),
                               terminal_profile_get_title_mode (profile));
}

static GtkWidget*
profile_editor_get_widget (GtkWidget  *editor,
                           const char *widget_name)
{
  GladeXML *xml;
  GtkWidget *w;
  
  xml = g_object_get_data (G_OBJECT (editor),
                           "glade-xml");

  g_return_val_if_fail (xml, NULL);

  w = glade_xml_get_widget (xml, widget_name);

  if (w == NULL)
    g_warning ("No such widget %s", widget_name);
  
  return w;
}
