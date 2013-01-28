/*
 * Copyright © 2001, 2002 Havoc Pennington, Red Hat Inc.
 * Copyright © 2008, 2011, 2012, 2013 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "terminal-prefs.h"
#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-schemas.h"
#include "terminal-util.h"

static GtkWidget *prefs_dialog = NULL;

static void
prefs_dialog_response_cb (GtkWidget *editor,
                          int response,
                          gpointer use_data)
{
  if (response == GTK_RESPONSE_HELP)
    {
      terminal_util_show_help ("gnome-terminal-shortcuts", GTK_WINDOW (editor));
      return;
    }

  gtk_widget_destroy (editor);
}

void
terminal_prefs_show_preferences (GtkWindow *transient_parent)
{
  GtkWidget *dialog, *tree_view, *disable_mnemonics_button, *disable_menu_accel_button;
  GSettings *settings;

  if (prefs_dialog != NULL)
    goto done;

  terminal_util_load_builder_resource ("/org/gnome/terminal/ui/preferences.ui",
                                       "preferences-dialog", &dialog,
                                       "disable-mnemonics-checkbutton", &disable_mnemonics_button,
                                       "disable-menu-accel-checkbutton", &disable_menu_accel_button,
                                       "accelerators-treeview", &tree_view,
                                       NULL);

  terminal_util_bind_mnemonic_label_sensitivity (dialog);

  settings = terminal_app_get_global_settings (terminal_app_get ());
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                   disable_mnemonics_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                   disable_menu_accel_button,
                   "active",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

  terminal_accels_fill_treeview (tree_view);


  g_signal_connect (dialog, "response",
                    G_CALLBACK (prefs_dialog_response_cb),
                    NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 350);

  prefs_dialog = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &prefs_dialog);

done:
  gtk_window_set_transient_for (GTK_WINDOW (prefs_dialog), transient_parent);
  gtk_window_present (GTK_WINDOW (prefs_dialog));
}
