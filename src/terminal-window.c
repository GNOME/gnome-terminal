
/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2007, 2008, 2009, 2011 Christian Persch
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
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <uuid.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-enums.h"
#include "terminal-encoding.h"
#include "terminal-icon-button.h"
#include "terminal-intl.h"
#include "terminal-mdi-container.h"
#include "terminal-notebook.h"
#include "terminal-schemas.h"
#include "terminal-screen-container.h"
#include "terminal-search-popover.h"
#include "terminal-tab-label.h"
#include "terminal-tabs-menu.h"
#include "terminal-util.h"
#include "terminal-window.h"
#include "terminal-libgsystem.h"

struct _TerminalWindowPrivate
{
  char *uuid;

  GtkClipboard *clipboard;

  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;
  guint ui_id;

  GtkActionGroup *profiles_action_group;
  guint profiles_ui_id;

  GtkActionGroup *encodings_action_group;
  guint encodings_ui_id;

  TerminalTabsMenu *tabs_menu;

  TerminalScreenPopupInfo *popup_info;
  guint remove_popup_info_idle;

  GtkActionGroup *new_terminal_action_group;
  guint new_terminal_ui_id;

  GtkWidget *menubar;
  TerminalMdiContainer *mdi_container;
  GtkWidget *main_vbox;
  TerminalScreen *active_screen;

  /* Size of a character cell in pixels */
  int old_char_width;
  int old_char_height;

  /* Width and height added to the actual terminal grid by "chrome" inside
   * what was traditionally the X11 window: menu bar, title bar,
   * style-provided padding. This must be included when resizing the window
   * and also included in geometry hints. */
  int old_chrome_width;
  int old_chrome_height;

  /* Width and height added to the window by client-side decorations.
   * This must be included in geometry hints but must not be included when
   * resizing the window. */
  int old_csd_width;
  int old_csd_height;

  /* Width and height of the padding around the geometry widget. */
  int old_padding_width;
  int old_padding_height;

  void *old_geometry_widget; /* only used for pointer value as it may be freed */

  GtkWidget *confirm_close_dialog;
  TerminalSearchPopover *search_popover;

  guint menubar_visible : 1;
  guint use_default_menubar_visibility : 1;

  guint disposed : 1;
  guint present_on_insert : 1;

  guint realized : 1;

  /* Workaround until gtk+ bug #535557 is fixed */
  guint icon_title_set : 1;
};

#define TERMINAL_WINDOW_CSS_NAME "terminal-window"

#define PROFILE_DATA_KEY "GT::Profile"

#define FILE_NEW_TERMINAL_UI_PATH         "/menubar/File/FileNewTerminalProfiles"
#define SET_ENCODING_UI_PATH              "/menubar/Terminal/TerminalSetEncoding/EncodingsPH"
#define SET_ENCODING_ACTION_NAME_PREFIX   "TerminalSetEncoding"

#define PROFILES_UI_PATH        "/menubar/Terminal/TerminalProfiles"
#define PROFILES_POPUP_UI_PATH  "/Popup/PopupTerminalProfiles/ProfilesPH"

#define SIZE_TO_UI_PATH            "/menubar/Terminal/TerminalSizeToPH"
#define SIZE_TO_ACTION_NAME_PREFIX "TerminalSizeTo"

#define STOCK_NEW_WINDOW  "window-new"
#define STOCK_NEW_TAB     "tab-new"

#define ENCODING_DATA_KEY "encoding"

#if 1
/*
 * We don't want to enable content saving until vte supports it async.
 * So we disable this code for stable versions.
 */
#include "terminal-version.h"

#if (TERMINAL_MINOR_VERSION & 1) != 0
#define ENABLE_SAVE
#else
#undef ENABLE_SAVE
#endif
#endif

static void terminal_window_dispose     (GObject             *object);
static void terminal_window_finalize    (GObject             *object);
static gboolean terminal_window_state_event (GtkWidget            *widget,
                                             GdkEventWindowState  *event);

static gboolean terminal_window_delete_event (GtkWidget *widget,
                                              GdkEvent *event,
                                              gpointer data);

static gboolean notebook_button_press_cb     (GtkWidget *notebook,
                                              GdkEventButton *event,
                                              TerminalWindow *window);
static gboolean notebook_popup_menu_cb       (GtkWidget *notebook,
                                              TerminalWindow *window);
static void mdi_screen_switched_cb (TerminalMdiContainer *container,
                                    TerminalScreen *old_active_screen,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screen_added_cb    (TerminalMdiContainer *container,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void mdi_screen_removed_cb  (TerminalMdiContainer *container,
                                    TerminalScreen *screen,
                                    TerminalWindow *window);
static void screen_close_request_cb (TerminalMdiContainer *container,
                                     TerminalScreen *screen,
                                     TerminalWindow *window);

/* Menu action callbacks */
static void file_new_terminal_callback        (GtkAction *action,
                                               TerminalWindow *window);
static void file_new_profile_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void file_close_window_callback        (GtkAction *action,
                                               TerminalWindow *window);
static void file_save_contents_callback       (GtkAction *action,
                                               TerminalWindow *window);
static void file_close_tab_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void edit_copy_callback                (GtkAction *action,
                                               TerminalWindow *window);
static void edit_paste_callback               (GtkAction *action,
                                               TerminalWindow *window);
static void edit_select_all_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void edit_preferences_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void edit_current_profile_callback     (GtkAction *action,
                                               TerminalWindow *window);
static void view_menubar_toggled_callback     (GtkToggleAction *action,
                                               TerminalWindow *window);
static void view_fullscreen_toggled_callback  (GtkToggleAction *action,
                                               TerminalWindow *window);
static void view_zoom_in_callback             (GtkAction *action,
                                               TerminalWindow *window);
static void view_zoom_out_callback            (GtkAction *action,
                                               TerminalWindow *window);
static void view_zoom_normal_callback         (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_add_encoding_callback    (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_reset_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_reset_clear_callback     (GtkAction *action,
                                               TerminalWindow *window);
static void terminal_readonly_toggled_callback(GtkToggleAction *action,
                                               TerminalWindow *window);
static void tabs_next_or_previous_tab_cb      (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_move_left_callback           (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_move_right_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void tabs_detach_tab_callback          (GtkAction *action,
                                               TerminalWindow *window);
static void help_contents_callback        (GtkAction *action,
                                           TerminalWindow *window);
static void help_about_callback           (GtkAction *action,
                                           TerminalWindow *window);
static void help_inspector_callback       (GtkAction *action,
                                           TerminalWindow *window);

static gboolean find_larger_zoom_factor  (double  current,
                                          double *found);
static gboolean find_smaller_zoom_factor (double  current,
                                          double *found);
static void terminal_window_update_zoom_sensitivity (TerminalWindow *window);
static void terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                                       TerminalWindow *window);

static void terminal_window_show (GtkWidget *widget);

static gboolean confirm_close_window_or_tab (TerminalWindow *window,
                                             TerminalScreen *screen);

static void
profile_set_callback (TerminalScreen *screen,
                      GSettings *old_profile,
                      TerminalWindow *window);
static void
sync_screen_icon_title (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window);

G_DEFINE_TYPE (TerminalWindow, terminal_window, GTK_TYPE_APPLICATION_WINDOW)

/* Clipboard helpers */

typedef struct {
  TerminalScreen *screen;
  gboolean uris_as_paths;
} PasteData;

static void
clipboard_uris_received_cb (GtkClipboard *clipboard,
                            /* const */ char **uris,
                            PasteData *data)
{
  gs_free char *text = NULL;
  gsize len;

  if (!uris) {
    g_object_unref (data->screen);
    g_slice_free (PasteData, data);
    return;
  }

  /* This potentially modifies the strings in |uris| but that's ok */
  if (data->uris_as_paths)
    terminal_util_transform_uris_to_quoted_fuse_paths (uris);

  text = terminal_util_concat_uris (uris, &len);
  vte_terminal_feed_child (VTE_TERMINAL (data->screen), text, len);

  g_object_unref (data->screen);
  g_slice_free (PasteData, data);
}

static void
clipboard_targets_received_cb (GtkClipboard *clipboard,
                               GdkAtom *targets,
                               int n_targets,
                               PasteData *data)
{
  if (!targets) {
    g_object_unref (data->screen);
    g_slice_free (PasteData, data);
    return;
  }

  if (gtk_targets_include_uri (targets, n_targets)) {
    gtk_clipboard_request_uris (clipboard,
                                (GtkClipboardURIReceivedFunc) clipboard_uris_received_cb,
                                data);
    return;
  } else /* if (gtk_targets_include_text (targets, n_targets)) */ {
    vte_terminal_paste_clipboard (VTE_TERMINAL (data->screen));
  }

  g_object_unref (data->screen);
  g_slice_free (PasteData, data);
}

/* Zoom helpers */

static const double zoom_factors[] = {
  TERMINAL_SCALE_MINIMUM,
  TERMINAL_SCALE_XXXXX_SMALL,
  TERMINAL_SCALE_XXXX_SMALL,
  TERMINAL_SCALE_XXX_SMALL,
  PANGO_SCALE_XX_SMALL,
  PANGO_SCALE_X_SMALL,
  PANGO_SCALE_SMALL,
  PANGO_SCALE_MEDIUM,
  PANGO_SCALE_LARGE,
  PANGO_SCALE_X_LARGE,
  PANGO_SCALE_XX_LARGE,
  TERMINAL_SCALE_XXX_LARGE,
  TERMINAL_SCALE_XXXX_LARGE,
  TERMINAL_SCALE_XXXXX_LARGE,
  TERMINAL_SCALE_MAXIMUM
};

static gboolean
find_larger_zoom_factor (double  current,
                         double *found)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (zoom_factors); ++i)
    {
      /* Find a font that's larger than this one */
      if ((zoom_factors[i] - current) > 1e-6)
        {
          *found = zoom_factors[i];
          return TRUE;
        }
    }
  
  return FALSE;
}

static gboolean
find_smaller_zoom_factor (double  current,
                          double *found)
{
  int i;
  
  i = (int) G_N_ELEMENTS (zoom_factors) - 1;
  while (i >= 0)
    {
      /* Find a font that's smaller than this one */
      if ((current - zoom_factors[i]) > 1e-6)
        {
          *found = zoom_factors[i];
          return TRUE;
        }
      
      --i;
    }

  return FALSE;
}

/* GAction callbacks */

static void
action_new_terminal_cb (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalSettingsList *profiles_list;
  gs_unref_object GSettings *profile = NULL;
  gs_free char *new_working_directory = NULL;
  const char *mode_str, *uuid_str;
  TerminalNewTerminalMode mode;
  GdkModifierType modifiers;

  g_assert (TERMINAL_IS_WINDOW (window));

  app = terminal_app_get ();

  g_variant_get (parameter, "(&s&s)", &mode_str, &uuid_str);

  if (g_str_equal (mode_str, "tab"))
    mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
  else if (g_str_equal (mode_str, "window"))
    mode = TERMINAL_NEW_TERMINAL_MODE_WINDOW;
  else {
    mode = g_settings_get_enum (terminal_app_get_global_settings (app),
                                TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY);
    if (gtk_get_current_event_state (&modifiers) &&
        (modifiers & gtk_accelerator_get_default_mod_mask () & GDK_CONTROL_MASK)) {
      /* Invert */
      if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
        mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
      else
        mode = TERMINAL_NEW_TERMINAL_MODE_WINDOW;
    }
  }

  profiles_list = terminal_app_get_profiles_list (app);
  if (g_str_equal (uuid_str, "current"))
    profile = terminal_screen_ref_profile (priv->active_screen);
  else if (g_str_equal (uuid_str, "default"))
    profile = terminal_settings_list_ref_default_child (profiles_list);
  else
    profile = terminal_settings_list_ref_child (profiles_list, uuid_str);

  if (profile == NULL)
    return;

  if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
    window = terminal_app_new_window (app, gtk_widget_get_screen (GTK_WIDGET (window)));

  new_working_directory = terminal_screen_get_current_dir (priv->active_screen);
  terminal_app_new_terminal (app, window, profile, NULL /* use profile encoding */,
                             NULL, NULL,
                             new_working_directory,
                             terminal_screen_get_initial_environment (priv->active_screen),
                             1.0);

  if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
    gtk_window_present (GTK_WINDOW (window));
}

static void
file_new_terminal_callback (GtkAction *action,
                            TerminalWindow *window)
{
  GSettings *profile;
  gs_free char *uuid;
  const char *name;
  GVariant *param;

  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  if (profile)
    uuid = terminal_settings_list_dup_uuid_from_child (terminal_app_get_profiles_list (terminal_app_get ()), profile);
  else
    uuid = g_strdup ("current");

  name = gtk_action_get_name (action);
  if (g_str_has_prefix (name, "FileNewTab"))
    param = g_variant_new ("(ss)", "tab", uuid);
  else if (g_str_has_prefix (name, "FileNewWindow"))
    param = g_variant_new ("(ss)", "window", uuid);
  else
    param = g_variant_new ("(ss)", "default", uuid);

  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "new-terminal"),
                     param);
}

static void
action_new_profile_cb (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_new_profile (terminal_app_get (),
                            terminal_screen_get_profile (priv->active_screen),
                            GTK_WINDOW (window));
}


#ifdef ENABLE_SAVE
static void
save_contents_dialog_on_response (GtkDialog *dialog, gint response_id, gpointer terminal)
{
  GtkWindow *parent;
  gs_free gchar *filename_uri = NULL;
  gs_unref_object GFile *file = NULL;
  GOutputStream *stream;
  gs_free_error GError *error = NULL;

  if (response_id != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      return;
    }

  parent = (GtkWindow*) gtk_widget_get_ancestor (GTK_WIDGET (terminal), GTK_TYPE_WINDOW);
  filename_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (filename_uri == NULL)
    return;

  file = g_file_new_for_uri (filename_uri);
  stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

  if (stream)
    {
      /* XXX
       * FIXME
       * This is a sync operation.
       * Should be replaced with the async version when vte implements that.
       */
      vte_terminal_write_contents_sync (terminal, stream,
					VTE_WRITE_DEFAULT,
					NULL, &error);
      g_object_unref (stream);
    }

  if (error)
    {
      terminal_util_show_error_dialog (parent, NULL, error,
				       "%s", _("Could not save contents"));
    }
}
#endif /* ENABLE_SAVE */

static void
action_save_contents_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
#ifdef ENABLE_SAVE
  TerminalWindow *window = user_data;
  GtkWidget *dialog = NULL;
  TerminalWindowPrivate *priv = window->priv;
  VteTerminal *terminal;

  if (!priv->active_screen)
    return;

  terminal = VTE_TERMINAL (priv->active_screen);
  g_return_if_fail (VTE_IS_TERMINAL (terminal));

  dialog = gtk_file_chooser_dialog_new (_("Save as…"),
                                        GTK_WINDOW(window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  /* XXX where should we save to? */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW(window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  g_signal_connect (dialog, "response", G_CALLBACK (save_contents_dialog_on_response), terminal);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (terminal_util_dialog_response_on_delete), NULL);

  gtk_window_present (GTK_WINDOW (dialog));
#endif /* ENABLE_SAVE */
}

static void
file_save_contents_callback (GtkAction *action,
                             TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "save-contents"),
                     NULL);
}

static void
action_close_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen;
  const char *mode_str;

  g_assert (parameter != NULL);
  g_variant_get (parameter, "&s", &mode_str);

  if (g_str_equal (mode_str, "tab"))
    screen = priv->active_screen;
  else if (g_str_equal (mode_str, "window"))
    screen = NULL;
  else
    g_assert_not_reached ();

  if (confirm_close_window_or_tab (window, screen))
    return;

  if (screen)
    terminal_window_remove_screen (window, screen);
  else
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
action_copy_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (!priv->active_screen)
    return;
      
  vte_terminal_copy_clipboard (VTE_TERMINAL (priv->active_screen));
}

static void
edit_copy_callback (GtkAction *action,
                    TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "copy"),
                     NULL);
}

static void
action_paste_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  GtkClipboard *clipboard;
  PasteData *data;
  const char *mode;

  g_assert (parameter != NULL);

  if (!priv->active_screen)
    return;

  g_variant_get (parameter, "&s", &mode);

  data = g_slice_new (PasteData);
  data->screen = g_object_ref (priv->active_screen);
  data->uris_as_paths = g_str_equal (mode, "uri");

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_targets (clipboard,
                                 (GtkClipboardTargetsReceivedFunc) clipboard_targets_received_cb,
                                 data);
}

static void
edit_paste_callback (GtkAction *action,
                     TerminalWindow *window)
{
  const char *name;
  GVariant *parameter;

  name = gtk_action_get_name (action);
  if ((name == I_("EditPasteURIPaths") || name == I_("PopupPasteURIPaths")))
    parameter = g_variant_new_string ("uri");
  else
    parameter = g_variant_new_string ("normal");

  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "paste"),
                     parameter);
}

static void
action_select_all_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;

  if (!priv->active_screen)
    return;

  vte_terminal_select_all (VTE_TERMINAL (priv->active_screen));
}

static void
edit_select_all_callback (GtkAction *action,
                          TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "select-all"),
                     NULL);
}

static void
action_reset_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;

  g_assert (parameter != NULL);

  if (priv->active_screen == NULL)
    return;

  vte_terminal_reset (VTE_TERMINAL (priv->active_screen),
                      TRUE,
                      g_variant_get_boolean (parameter));
}

static void
action_switch_tab_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  int value;

  g_assert (parameter != NULL);

  value = g_variant_get_int32 (parameter);

  if (value > 0)
    terminal_mdi_container_set_active_screen_num (priv->mdi_container, value - 1);
  else
    terminal_mdi_container_change_screen (priv->mdi_container, value == -2 ? -1 : 1);
}

static void
action_move_tab_cb (GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  int value;
  
  g_assert (parameter != NULL);

  value = g_variant_get_int32 (parameter);
  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         value);
}

static void
action_zoom_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  int value;
  double zoom;
  
  if (priv->active_screen == NULL)
    return;

  g_assert (parameter != NULL);

  value = g_variant_get_int32 (parameter);

  if (value == 0) {
    zoom = PANGO_SCALE_MEDIUM;
  } else if (value == 1) {
    zoom = vte_terminal_get_font_scale (VTE_TERMINAL (priv->active_screen));
    if (!find_larger_zoom_factor (zoom, &zoom))
      return;
  } else if (value == -1) {
    zoom = vte_terminal_get_font_scale (VTE_TERMINAL (priv->active_screen));
    if (!find_smaller_zoom_factor (zoom, &zoom))
      return;
  } else
    g_assert_not_reached ();

  vte_terminal_set_font_scale (VTE_TERMINAL (priv->active_screen), zoom);
  terminal_window_update_zoom_sensitivity (window);
}

static void
view_zoom_in_callback (GtkAction *action,
                       TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "zoom"),
                     g_variant_new_int32 (1));
}

static void
view_zoom_out_callback (GtkAction *action,
                        TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "zoom"),
                     g_variant_new_int32 (-1));
}

static void
view_zoom_normal_callback (GtkAction *action,
                           TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "zoom"),
                     g_variant_new_int32 (0));
}

static void
action_detach_tab_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalWindow *new_window;
  TerminalScreen *screen;
  char geometry[32];
  int width, height;

  app = terminal_app_get ();

  screen = priv->active_screen;

  /* FIXME: this seems wrong if tabs are shown in the window */
  terminal_screen_get_size (screen, &width, &height);
  g_snprintf (geometry, sizeof (geometry), "%dx%d", width, height);

  new_window = terminal_app_new_window (app, gtk_widget_get_screen (GTK_WIDGET (window)));

  terminal_window_move_screen (window, new_window, screen, -1);

  terminal_window_parse_geometry (new_window, geometry);

  gtk_window_present_with_time (GTK_WINDOW (new_window), gtk_get_current_event_time ());
}

static void
tabs_detach_tab_callback (GtkAction *action,
                          TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "detach-tab"),
                     NULL);
}

static void
action_help_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = user_data;

  terminal_util_show_help (NULL, GTK_WINDOW (window));
}

static void
action_about_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;

  terminal_util_show_about (GTK_WINDOW (window));
}

static void
action_preferences_cb (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
  TerminalWindow *window = user_data;

  terminal_app_edit_preferences (terminal_app_get (), GTK_WINDOW (window));
}

static void
action_edit_profile_cb (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;

  terminal_app_edit_profile (terminal_app_get (),
                             terminal_screen_get_profile (priv->active_screen),
                             GTK_WINDOW (window),
                             NULL);
}

static void
search_popover_search_cb (TerminalSearchPopover *popover,
                          gboolean backward,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (G_UNLIKELY (priv->active_screen == NULL))
    return;

  if (backward)
    vte_terminal_search_find_previous (VTE_TERMINAL (priv->active_screen));
  else
    vte_terminal_search_find_next (VTE_TERMINAL (priv->active_screen));
}

static void
search_popover_notify_regex_cb (TerminalSearchPopover *popover,
                                GParamSpec *pspec G_GNUC_UNUSED,
                                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  VteRegex *regex;

  if (G_UNLIKELY (priv->active_screen == NULL))
    return;

  regex = terminal_search_popover_get_regex (popover);
  vte_terminal_search_set_regex (VTE_TERMINAL (priv->active_screen), regex, 0);

  terminal_window_update_search_sensitivity (priv->active_screen, window);
}

static void
search_popover_notify_wrap_around_cb (TerminalSearchPopover *popover,
                                      GParamSpec *pspec G_GNUC_UNUSED,
                                      TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  gboolean wrap;

  if (G_UNLIKELY (priv->active_screen == NULL))
    return;

  wrap = terminal_search_popover_get_wrap_around (popover);
  vte_terminal_search_set_wrap_around (VTE_TERMINAL (priv->active_screen), wrap);
}

static void
terminal_window_ensure_search_popover (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (G_UNLIKELY(priv->active_screen == NULL))
    return;

  if (priv->search_popover != NULL) {
    search_popover_notify_regex_cb (priv->search_popover, NULL, window);
    search_popover_notify_wrap_around_cb (priv->search_popover, NULL, window);

    gtk_widget_show (GTK_WIDGET (priv->search_popover));
    return;
  }

  if (priv->active_screen == NULL)
    return;

  priv->search_popover = terminal_search_popover_new (GTK_WIDGET (priv->menubar));

  g_signal_connect (priv->search_popover, "search", G_CALLBACK (search_popover_search_cb), window);

  search_popover_notify_regex_cb (priv->search_popover, NULL, window);
  g_signal_connect (priv->search_popover, "notify::regex", G_CALLBACK (search_popover_notify_regex_cb), window);

  search_popover_notify_wrap_around_cb (priv->search_popover, NULL, window);
  g_signal_connect (priv->search_popover, "notify::wrap-around", G_CALLBACK (search_popover_notify_wrap_around_cb), window);

  g_signal_connect (priv->search_popover, "destroy", G_CALLBACK (gtk_widget_destroyed), &priv->search_popover);

  gtk_widget_show (GTK_WIDGET (priv->search_popover));
}

static void
action_find_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = user_data;
  TerminalWindowPrivate *priv = window->priv;
  const char *mode;

  if (G_UNLIKELY (!priv->active_screen))
    return;

  g_variant_get (parameter, "&s", &mode);

  if (g_str_equal (mode, "find")) {
    terminal_window_ensure_search_popover (window);
  } else if (g_str_equal (mode, "next")) {
    vte_terminal_search_find_next (VTE_TERMINAL (priv->active_screen));
  } else if (g_str_equal (mode, "previous")) {
    vte_terminal_search_find_previous (VTE_TERMINAL (priv->active_screen));
  } else if (g_str_equal (mode, "clear")) {
    vte_terminal_search_set_regex (VTE_TERMINAL (priv->active_screen), NULL, 0);
  } else
    return;
}

static void
search_find_callback (GtkAction *action,
		      TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "find"),
                     g_variant_new ("s", "find"));
}

static void
search_find_next_callback (GtkAction *action,
			   TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "find"),
                     g_variant_new ("s", "next"));
}

static void
search_find_prev_callback (GtkAction *action,
			   TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "find"),
                     g_variant_new ("s", "previous"));
}

static void
search_clear_highlight_callback (GtkAction *action,
				 TerminalWindow *window)
{
  g_action_activate (g_action_map_lookup_action (G_ACTION_MAP (window), "find"),
                     g_variant_new ("s", "clear"));
}

static void
action_toggle_state_cb (GSimpleAction *saction,
                        GVariant *parameter,
                        gpointer user_data)
{
  GAction *action = G_ACTION (saction);
  gs_unref_variant GVariant *state;

  state = g_action_get_state (action);
  g_action_change_state (action, g_variant_new_boolean (!g_variant_get_boolean (state)));
}

static void
action_show_menubar_state_cb (GSimpleAction *action,
                              GVariant *state,
                              gpointer user_data)
{
  TerminalWindow *window = user_data;

  g_simple_action_set_state (action, state);

  terminal_window_set_menubar_visible (window, g_variant_get_boolean (state));
}

static void
action_fullscreen_state_cb (GSimpleAction *action,
                            GVariant *state,
                            gpointer user_data)
{
  TerminalWindow *window = user_data;

  g_simple_action_set_state (action, state);

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (g_variant_get_boolean (state))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

/* Menubar mnemonics & accel settings handling */

static void
enable_menubar_accel_changed_cb (GSettings *settings,
                                 const char *key,
                                 GtkSettings *gtk_settings)
{
  /* const */ char *saved_menubar_accel;

  /* FIXME: Once gtk+ bug 507398 is fixed, use that to reset the property instead */
  /* Now this is a bad hack on so many levels. */
  saved_menubar_accel = g_object_get_data (G_OBJECT (gtk_settings), "GT::gtk-menu-bar-accel");

  if (g_settings_get_boolean (settings, key))
    g_object_set (gtk_settings, "gtk-menu-bar-accel", saved_menubar_accel, NULL);
  else
    g_object_set (gtk_settings, "gtk-menu-bar-accel", NULL, NULL);
}

static void
app_setting_notify_destroy_cb (GtkSettings *gtk_settings)
{
  g_signal_handlers_disconnect_by_func (terminal_app_get_global_settings (terminal_app_get ()),
                                        G_CALLBACK (enable_menubar_accel_changed_cb),
                                        gtk_settings);
}

/* utility functions */

static char *
escape_underscores (const char *name)
{
  GString *escaped_name;

  g_assert (name != NULL);

  /* Who'd use more that 4 underscores in a profile name... */
  escaped_name = g_string_sized_new (strlen (name) + 4 + 1);

  while (*name)
    {
      if (*name == '_')
        g_string_append (escaped_name, "__");
      else
        g_string_append_c (escaped_name, *name);
      name++;
    }

  return g_string_free (escaped_name, FALSE);
}

static int
find_tab_num_at_pos (GtkNotebook *notebook,
                     int screen_x, 
                     int screen_y)
{
  GtkPositionType tab_pos;
  int page_num = 0;
  GtkNotebook *nb = GTK_NOTEBOOK (notebook);
  GtkWidget *page;
  GtkAllocation tab_allocation;

  tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

  while ((page = gtk_notebook_get_nth_page (nb, page_num)))
    {
      GtkWidget *tab;
      int max_x, max_y, x_root, y_root;

      tab = gtk_notebook_get_tab_label (nb, page);
      g_return_val_if_fail (tab != NULL, -1);

      if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
        {
          page_num++;
          continue;
        }

      gdk_window_get_origin (gtk_widget_get_window (tab), &x_root, &y_root);

      gtk_widget_get_allocation (tab, &tab_allocation);
      max_x = x_root + tab_allocation.x + tab_allocation.width;
      max_y = y_root + tab_allocation.y + tab_allocation.height;

      if ((tab_pos == GTK_POS_TOP || tab_pos == GTK_POS_BOTTOM) && screen_x <= max_x)
        return page_num;

      if ((tab_pos == GTK_POS_LEFT || tab_pos == GTK_POS_RIGHT) && screen_y <= max_y)
        return page_num;

      page_num++;
    }

  return -1;
}

static void
position_menu_under_widget (GtkMenu *menu,
                            int *x,
                            int *y,
                            gboolean *push_in,
                            gpointer user_data)
{
  /* Adapted from gtktoolbar.c */
  GtkWidget *widget = GTK_WIDGET (user_data);
  GdkWindow *widget_window;
  GtkWidget *container;
  GtkRequisition req;
  GtkRequisition menu_req;
  GdkRectangle monitor;
  int monitor_num;
  GdkScreen *screen;
  GtkAllocation widget_allocation;

  widget_window = gtk_widget_get_window (widget);
  gtk_widget_get_allocation (widget, &widget_allocation);
  container = gtk_widget_get_ancestor (widget, GTK_TYPE_CONTAINER);

  gtk_widget_get_preferred_size (widget, NULL, &req);
  gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, &menu_req);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, widget_window);
  if (monitor_num < 0)
          monitor_num = 0;
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  gdk_window_get_origin (widget_window, x, y);
  if (!gtk_widget_get_has_window (widget))
    {
      *x += widget_allocation.x;
      *y += widget_allocation.y;
    }
  if (gtk_widget_get_direction (container) == GTK_TEXT_DIR_LTR) 
    *x += widget_allocation.width - req.width;
  else 
    *x += req.width - menu_req.width;

  if ((*y + widget_allocation.height + menu_req.height) <= monitor.y + monitor.height)
    *y += widget_allocation.height;
  else if ((*y - menu_req.height) >= monitor.y)
    *y -= menu_req.height;
  else if (monitor.y + monitor.height - (*y + widget_allocation.height) > *y)
    *y += widget_allocation.height;
  else
    *y -= menu_req.height;

  *push_in = FALSE;
}

static void
terminal_set_profile_toggled_callback (GtkToggleAction *action,
                                       TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *profile;

  if (!gtk_toggle_action_get_active (action))
    return;

  if (priv->active_screen == NULL)
    return;

  profile = g_object_get_data (G_OBJECT (action), PROFILE_DATA_KEY);
  g_assert (profile);

  g_signal_handlers_block_by_func (priv->active_screen, G_CALLBACK (profile_set_callback), window);
  terminal_screen_set_profile (priv->active_screen, profile);
  g_signal_handlers_unblock_by_func (priv->active_screen, G_CALLBACK (profile_set_callback), window);
}

static void
profile_visible_name_notify_cb (GSettings  *profile,
                                const char *key,
                                GtkAction  *action)
{
  gs_free char *visible_name;
  char *dot;
  gs_free char *display_name;
  guint num;

  visible_name = g_settings_get_string (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY);
  display_name = escape_underscores (visible_name);

  dot = strchr (gtk_action_get_name (action), '.');
  if (dot != NULL)
    {
      gs_free char *free_me;

      num = g_ascii_strtoll (dot + 1, NULL, 10);

      free_me = display_name;
      if (num < 10)
        /* Translators: This is the label of a menu item to choose a profile.
         * _%u is used as the accelerator (with u between 1 and 9), and
         * the %s is the name of the terminal profile.
         */
        display_name = g_strdup_printf (_("_%u. %s"), num, display_name);
      else if (num < 36)
        /* Translators: This is the label of a menu item to choose a profile.
         * _%c is used as the accelerator (it will be a character between A and Z),
         * and the %s is the name of the terminal profile.
         */
        display_name = g_strdup_printf (_("_%c. %s"), (guchar)('A' + num - 10), display_name);
      else
        free_me = NULL;
    }

  g_object_set (action, "label", display_name, NULL);
}

static void
disconnect_profiles_from_actions_in_group (GtkActionGroup *action_group)
{
  GList *actions, *l;

  actions = gtk_action_group_list_actions (action_group);
  for (l = actions; l != NULL; l = l->next)
    {
      GObject *action = G_OBJECT (l->data);
      GSettings *profile;

      profile = g_object_get_data (action, PROFILE_DATA_KEY);
      if (!profile)
        continue;

      g_signal_handlers_disconnect_by_func (profile, G_CALLBACK (profile_visible_name_notify_cb), action);
    }
  g_list_free (actions);
}

static void
terminal_window_update_set_profile_menu_active_profile (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *new_active_profile;
  GList *actions, *l;

  if (!priv->profiles_action_group)
    return;

  if (!priv->active_screen)
    return;

  new_active_profile = terminal_screen_get_profile (priv->active_screen);

  actions = gtk_action_group_list_actions (priv->profiles_action_group);
  for (l = actions; l != NULL; l = l->next)
    {
      GObject *action = G_OBJECT (l->data);
      GSettings *profile;

      profile = g_object_get_data (action, PROFILE_DATA_KEY);
      if (profile != new_active_profile)
        continue;

      g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_set_profile_toggled_callback), window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
      g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_set_profile_toggled_callback), window);

      break;
    }
  g_list_free (actions);
}

static void
terminal_window_update_set_profile_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GSettings *active_profile;
  GtkActionGroup *action_group;
  GtkAction *action;
  TerminalSettingsList *profiles_list;
  GList *profiles, *p;
  GSList *group;
  guint n;
  gboolean single_profile;

  /* Remove the old UI */
  if (priv->profiles_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->profiles_ui_id);
      priv->profiles_ui_id = 0;
    }

  if (priv->profiles_action_group != NULL)
    {
      disconnect_profiles_from_actions_in_group (priv->profiles_action_group);
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->profiles_action_group);
      priv->profiles_action_group = NULL;
    }

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  profiles = terminal_profiles_list_ref_children_sorted (profiles_list);

  action = gtk_action_group_get_action (priv->action_group, "TerminalProfiles");
  single_profile = !profiles || profiles->next == NULL; /* list length <= 1 */
  gtk_action_set_sensitive (action, !single_profile);

  if (profiles == NULL)
    return;

  if (priv->active_screen)
    active_profile = terminal_screen_get_profile (priv->active_screen);
  else
    active_profile = NULL;

  action_group = priv->profiles_action_group = gtk_action_group_new ("Profiles");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->profiles_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  group = NULL;
  n = 0;
  for (p = profiles; p != NULL; p = p->next)
    {
      GSettings *profile = (GSettings *) p->data;
      gs_unref_object GtkRadioAction *profile_action;
      char name[32];

      g_snprintf (name, sizeof (name), "TerminalSetProfile%u", n++);

      profile_action = gtk_radio_action_new (name,
                                             NULL,
                                             NULL,
                                             NULL,
                                             n);

      gtk_radio_action_set_group (profile_action, group);
      group = gtk_radio_action_get_group (profile_action);

      if (profile == active_profile)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (profile_action), TRUE);

      g_object_set_data_full (G_OBJECT (profile_action),
                              PROFILE_DATA_KEY,
                              g_object_ref (profile),
                              (GDestroyNotify) g_object_unref);
      profile_visible_name_notify_cb (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, GTK_ACTION (profile_action));
      g_signal_connect (profile, "changed::" TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                        G_CALLBACK (profile_visible_name_notify_cb), profile_action);
      g_signal_connect (profile_action, "toggled",
                        G_CALLBACK (terminal_set_profile_toggled_callback), window);

      gtk_action_group_add_action (action_group, GTK_ACTION (profile_action));

      gtk_ui_manager_add_ui (priv->ui_manager, priv->profiles_ui_id,
                             PROFILES_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
      gtk_ui_manager_add_ui (priv->ui_manager, priv->profiles_ui_id,
                             PROFILES_POPUP_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  g_list_free_full (profiles, (GDestroyNotify) g_object_unref);
}

static void
terminal_window_create_new_terminal_action (TerminalWindow *window,
                                            GSettings *profile,
                                            const char *name,
                                            guint num,
                                            GCallback callback)
{
  TerminalWindowPrivate *priv = window->priv;
  gs_unref_object GtkAction *action;

  action = gtk_action_new (name, NULL, NULL, NULL);

  g_object_set_data_full (G_OBJECT (action),
                          PROFILE_DATA_KEY,
                          g_object_ref (profile),
                          (GDestroyNotify) g_object_unref);
  profile_visible_name_notify_cb (profile, TERMINAL_PROFILE_VISIBLE_NAME_KEY, action);
  g_signal_connect (profile, "changed::" TERMINAL_PROFILE_VISIBLE_NAME_KEY,
                    G_CALLBACK (profile_visible_name_notify_cb), action);
  g_signal_connect (action, "activate", callback, window);

  gtk_action_group_add_action (priv->new_terminal_action_group, action);
}

static void
terminal_window_update_new_terminal_menus (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkActionGroup *action_group;
  GtkAction *action;
  TerminalSettingsList *profiles_list;
  GList *profiles, *p;
  guint n;
  gboolean have_single_profile;

  /* Remove the old UI */
  if (priv->new_terminal_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->new_terminal_ui_id);
      priv->new_terminal_ui_id = 0;
    }

  if (priv->new_terminal_action_group != NULL)
    {
      disconnect_profiles_from_actions_in_group (priv->new_terminal_action_group);
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->new_terminal_action_group);
      priv->new_terminal_action_group = NULL;
    }

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  profiles = terminal_profiles_list_ref_children_sorted (profiles_list);

  have_single_profile = !profiles || !profiles->next;

  action = gtk_action_group_get_action (priv->action_group, "FileNewTab");
  gtk_action_set_visible (action, have_single_profile);
  action = gtk_action_group_get_action (priv->action_group, "FileNewWindow");
  gtk_action_set_visible (action, have_single_profile);
  action = gtk_action_group_get_action (priv->action_group, "FileNewTerminal");
  gtk_action_set_visible (action, have_single_profile);

  if (have_single_profile)
    {
      g_list_free_full (profiles, (GDestroyNotify) g_object_unref);
      return;
    }

  /* Now build the submenus */

  action_group = priv->new_terminal_action_group = gtk_action_group_new ("NewTerminal");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->new_terminal_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  n = 0;
  for (p = profiles; p != NULL; p = p->next)
    {
      GSettings *profile = (GSettings *) p->data;
      char name[32];

      g_snprintf (name, sizeof (name), "FileNewTerminal.%u", n);
      terminal_window_create_new_terminal_action (window,
                                                  profile,
                                                  name,
                                                  n,
                                                  G_CALLBACK (file_new_terminal_callback));

      gtk_ui_manager_add_ui (priv->ui_manager, priv->new_terminal_ui_id,
                             FILE_NEW_TERMINAL_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);

      ++n;
    }

  g_list_free_full (profiles, (GDestroyNotify) g_object_unref);
}

static void
terminal_set_encoding_callback (GtkToggleAction *action,
                                TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalEncoding *encoding;
  
  if (!gtk_toggle_action_get_active (action))
    return;

  if (priv->active_screen == NULL)
    return;

  encoding = g_object_get_data (G_OBJECT (action), ENCODING_DATA_KEY);
  g_assert (encoding);

  vte_terminal_set_encoding (VTE_TERMINAL (priv->active_screen),
                             terminal_encoding_get_charset (encoding),
                             NULL);
}

static void
terminal_window_update_encoding_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  GtkActionGroup *action_group;
  GSList *group;
  guint n;
  GSList *encodings, *l;
  const char *charset;
  TerminalEncoding *active_encoding;

  /* Remove the old UI */
  if (priv->encodings_ui_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->ui_manager, priv->encodings_ui_id);
      priv->encodings_ui_id = 0;
    }

  if (priv->encodings_action_group != NULL)
    {
      gtk_ui_manager_remove_action_group (priv->ui_manager,
                                          priv->encodings_action_group);
      priv->encodings_action_group = NULL;
    }

  action_group = priv->encodings_action_group = gtk_action_group_new ("Encodings");
  gtk_ui_manager_insert_action_group (priv->ui_manager, action_group, -1);
  g_object_unref (action_group);

  priv->encodings_ui_id = gtk_ui_manager_new_merge_id (priv->ui_manager);

  if (priv->active_screen)
    charset = vte_terminal_get_encoding (VTE_TERMINAL (priv->active_screen));
  else
    charset = "UTF-8";

  app = terminal_app_get ();
  active_encoding = terminal_app_ensure_encoding (app, charset);

  encodings = terminal_app_get_active_encodings (app);

  if (g_slist_find (encodings, active_encoding) == NULL)
    encodings = g_slist_append (encodings, terminal_encoding_ref (active_encoding));

  group = NULL;
  n = 0;
  for (l = encodings; l != NULL; l = l->next)
    {
      TerminalEncoding *e = (TerminalEncoding *) l->data;
      gs_unref_object GtkRadioAction *encoding_action;
      char name[128];
      gs_free char *display_name;

      g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s", terminal_encoding_get_charset (e));
      display_name = g_strdup_printf ("%s (%s)", e->name, terminal_encoding_get_charset (e));

      encoding_action = gtk_radio_action_new (name,
                                              display_name,
                                              NULL,
                                              NULL,
                                              n);

      gtk_radio_action_set_group (encoding_action, group);
      group = gtk_radio_action_get_group (encoding_action);

      if (charset && strcmp (terminal_encoding_get_charset (e), charset) == 0)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (encoding_action), TRUE);

      g_signal_connect (encoding_action, "toggled",
                        G_CALLBACK (terminal_set_encoding_callback), window);

      g_object_set_data_full (G_OBJECT (encoding_action), ENCODING_DATA_KEY,
                              terminal_encoding_ref (e),
                              (GDestroyNotify) terminal_encoding_unref);

      gtk_action_group_add_action (action_group, GTK_ACTION (encoding_action));

      gtk_ui_manager_add_ui (priv->ui_manager, priv->encodings_ui_id,
                             SET_ENCODING_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }

  g_slist_foreach (encodings, (GFunc) terminal_encoding_unref, NULL);
  g_slist_free (encodings);
}

static void
terminal_window_update_encoding_menu_active_encoding (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  char name[128];

  if (!priv->active_screen)
    return;
  if (!priv->encodings_action_group)
    return;

  g_snprintf (name, sizeof (name), SET_ENCODING_ACTION_NAME_PREFIX "%s",
              vte_terminal_get_encoding (VTE_TERMINAL (priv->active_screen)));
  action = gtk_action_group_get_action (priv->encodings_action_group, name);
  if (!action)
    return;

  g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_set_encoding_callback), window);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
  g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_set_encoding_callback), window);
}

static void
terminal_window_update_terminal_menu (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;

  if (!priv->active_screen)
    return;

  action = gtk_action_group_get_action(priv->action_group, "TerminalReadOnly");
  g_signal_handlers_block_by_func (action, G_CALLBACK (terminal_readonly_toggled_callback), window);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                !vte_terminal_get_input_enabled (VTE_TERMINAL (priv->active_screen)));
  g_signal_handlers_unblock_by_func (action, G_CALLBACK (terminal_readonly_toggled_callback), window);
}

static void
terminal_size_to_cb (GtkAction *action,
                     TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *name;
  char *end = NULL;
  guint width, height;

  if (priv->active_screen == NULL)
    return;

  name = gtk_action_get_name (action) + strlen (SIZE_TO_ACTION_NAME_PREFIX);
  width = g_ascii_strtoull (name, &end, 10);
  g_assert (end && *end == 'x');
  height = g_ascii_strtoull (end + 1, &end, 10);
  g_assert (end && *end == '\0');

  vte_terminal_set_size (VTE_TERMINAL (priv->active_screen), width, height);

  terminal_window_update_size (window);
}

static void
terminal_window_update_size_to_menu (TerminalWindow *window)
{
  static const struct {
    guint grid_width;
    guint grid_height;
  } predefined_sizes[] = {
    { 80, 24 },
    { 80, 43 },
    { 132, 24 },
    { 132, 43 }
  };
  TerminalWindowPrivate *priv = window->priv;
  guint i;

  /* We only install this once, so there's no need for a separate action group
   * and any cleanup + build-new-one action here.
   */

  for (i = 0; i < G_N_ELEMENTS (predefined_sizes); ++i)
    {
      guint grid_width = predefined_sizes[i].grid_width;
      guint grid_height = predefined_sizes[i].grid_height;
      gs_unref_object GtkAction *action;
      char name[40];
      gs_free char *display_name;

      g_snprintf (name, sizeof (name), SIZE_TO_ACTION_NAME_PREFIX "%ux%u",
                  grid_width, grid_height);

      /* If there are ever more than 9 of these, extend this to use A..Z as mnemonics,
       * like we do for the profiles menu.
       */
      display_name = g_strdup_printf ("_%u. %u×%u", i + 1, grid_width, grid_height);

      action = gtk_action_new (name, display_name, NULL, NULL);
      g_signal_connect (action, "activate",
                        G_CALLBACK (terminal_size_to_cb), window);

      gtk_action_group_add_action (priv->action_group, action);

      gtk_ui_manager_add_ui (priv->ui_manager, priv->ui_id,
                             SIZE_TO_UI_PATH,
                             name, name,
                             GTK_UI_MANAGER_MENUITEM, FALSE);
    }
}

/* Actions stuff */

static void
terminal_window_update_copy_sensitivity (TerminalScreen *screen,
                                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean can_copy;

  if (screen != priv->active_screen)
    return;

  can_copy = vte_terminal_get_has_selection (VTE_TERMINAL (screen));

  action = gtk_action_group_get_action (priv->action_group, "EditCopy");
  gtk_action_set_sensitive (action, can_copy);
}

static void
terminal_window_update_zoom_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *screen;
  GtkAction *action;
  double current, zoom;
  
  screen = priv->active_screen;
  if (screen == NULL)
    return;

  current = vte_terminal_get_font_scale (VTE_TERMINAL (screen));

  action = gtk_action_group_get_action (priv->action_group, "ViewZoomOut");
  gtk_action_set_sensitive (action, find_smaller_zoom_factor (current, &zoom));
  action = gtk_action_group_get_action (priv->action_group, "ViewZoomIn");
  gtk_action_set_sensitive (action, find_larger_zoom_factor (current, &zoom));
}

static void
terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;
  gboolean can_search;

  if (screen != priv->active_screen)
    return;

  can_search = vte_terminal_search_get_regex (VTE_TERMINAL (screen)) != NULL;

  action = gtk_action_group_get_action (priv->action_group, "SearchFindNext");
  gtk_action_set_sensitive (action, can_search);
  action = gtk_action_group_get_action (priv->action_group, "SearchFindPrevious");
  gtk_action_set_sensitive (action, can_search);
  action = gtk_action_group_get_action (priv->action_group, "SearchClearHighlight");
  gtk_action_set_sensitive (action, can_search);
}

static void
update_edit_menu_cb (GtkClipboard *clipboard,
                     GdkAtom *targets,
                     int n_targets,
                     GWeakRef *ref)
{
  TerminalWindow *window;
  TerminalWindowPrivate *priv;
  GtkAction *action;
  gboolean can_paste, can_paste_uris;

  window = g_weak_ref_get (ref);
  if (window == NULL)
    goto out;

  /* Now we know the window is still alive */
  priv = window->priv;

  can_paste = targets != NULL && gtk_targets_include_text (targets, n_targets);
  can_paste_uris = targets != NULL && gtk_targets_include_uri (targets, n_targets);

  action = gtk_action_group_get_action (priv->action_group, "EditPaste");
  gtk_action_set_sensitive (action, can_paste);
  action = gtk_action_group_get_action (priv->action_group, "EditPasteURIPaths");
  gtk_action_set_visible (action, can_paste_uris);
  gtk_action_set_sensitive (action, can_paste_uris);

  g_object_unref (window);
 out:
  g_weak_ref_clear (ref);
  g_slice_free (GWeakRef, ref);
}

static void
update_edit_menu (GtkClipboard *clipboard,
                  GdkEvent *event G_GNUC_UNUSED,
                  TerminalWindow *window)
{
  GWeakRef *ref;

  ref = g_slice_new0 (GWeakRef);
  g_weak_ref_init (ref, window);
  gtk_clipboard_request_targets (clipboard,
                                 (GtkClipboardTargetsReceivedFunc) update_edit_menu_cb,
                                 ref);
}

static void
screen_resize_window_cb (TerminalScreen *screen,
                         guint columns,
                         guint rows,
                         TerminalWindow* window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget = GTK_WIDGET (screen);

  if (gtk_widget_get_realized (widget) &&
      (gdk_window_get_state (gtk_widget_get_window (widget)) & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) != 0)
    return;

  vte_terminal_set_size (VTE_TERMINAL (priv->active_screen), columns, rows);

  if (screen == priv->active_screen)
    terminal_window_update_size (window);
}

static void
terminal_window_update_tabs_menu_sensitivity (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GAction *gaction;
  GtkActionGroup *action_group = priv->action_group;
  GtkAction *action;
  int num_pages, page_num;
  gboolean not_first, not_last;

  if (priv->disposed)
    return;

  num_pages = terminal_mdi_container_get_n_screens (priv->mdi_container);
  page_num = terminal_mdi_container_get_active_screen_num (priv->mdi_container);
  not_first = page_num > 0;
  not_last = page_num + 1 < num_pages;

  /* Hide the tabs menu in single-tab windows */
  action = gtk_action_group_get_action (action_group, "Tabs");
  gtk_action_set_visible (action, num_pages > 1);
  
#if 1
  /* NOTE: We always make next/prev actions sensitive except in
   * single-tab windows, so the corresponding shortcut key escape code
   * isn't sent to the terminal. See bug #453193 and bug #138609.
   * This also makes tab cycling work, bug #92139.
   * FIXME: Find a better way to do this.
   */
  action = gtk_action_group_get_action (action_group, "TabsPrevious");
  gtk_action_set_sensitive (action, num_pages > 1);
  action = gtk_action_group_get_action (action_group, "TabsNext");
  gtk_action_set_sensitive (action, num_pages > 1);
#else
  /* This would be correct, but see the comment above. */
  action = gtk_action_group_get_action (action_group, "TabsPrevious");
  gtk_action_set_sensitive (action, not_first);
  action = gtk_action_group_get_action (action_group, "TabsNext");
  gtk_action_set_sensitive (action, not_last);
#endif

  gaction = g_action_map_lookup_action (G_ACTION_MAP (window), "switch-tab");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (gaction), num_pages > 1);

  action = gtk_action_group_get_action (action_group, "TabsMoveLeft");
  gtk_action_set_sensitive (action, not_first);
  action = gtk_action_group_get_action (action_group, "TabsMoveRight");
  gtk_action_set_sensitive (action, not_last);
  action = gtk_action_group_get_action (action_group, "TabsDetach");
  gtk_action_set_sensitive (action, num_pages > 1);
}

static GtkNotebook *
handle_tab_droped_on_desktop (GtkNotebook *source_notebook,
                              GtkWidget   *container,
                              gint         x,
                              gint         y,
                              gpointer     data G_GNUC_UNUSED)
{
  TerminalWindow *source_window;
  TerminalWindow *new_window;
  TerminalWindowPrivate *new_priv;

  source_window = TERMINAL_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (source_notebook)));
  g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), NULL);

  new_window = terminal_app_new_window (terminal_app_get (),
                                        gtk_widget_get_screen (GTK_WIDGET (source_window)));
  new_priv = new_window->priv;
  new_priv->present_on_insert = TRUE;

//   update_tab_visibility (source_window, -1);
//   update_tab_visibility (new_window, +1);

  return GTK_NOTEBOOK (new_priv->mdi_container);
}

/* Terminal screen popup menu handling */

static void
popup_open_url_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;

  if (info == NULL)
    return;

  terminal_util_open_url (GTK_WIDGET (window), info->url, info->url_flavor,
                          gtk_get_current_event_time ());
}

static void
popup_copy_url_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreenPopupInfo *info = priv->popup_info;
  GtkClipboard *clipboard;

  if (info == NULL)
    return;

  if (info->url == NULL)
    return;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, info->url, -1);
}

static void
popup_leave_fullscreen_callback (GtkAction *action,
                                 TerminalWindow *window)
{
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
remove_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->remove_popup_info_idle != 0)
    {
      g_source_remove (priv->remove_popup_info_idle);
      priv->remove_popup_info_idle = 0;
    }

  if (priv->popup_info != NULL)
    {
      terminal_screen_popup_info_unref (priv->popup_info);
      priv->popup_info = NULL;
    }
}

static gboolean
idle_remove_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  priv->remove_popup_info_idle = 0;
  remove_popup_info (window);
  return FALSE;
}

static void
unset_popup_info (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  /* Unref the event from idle since we still need it
   * from the action callbacks which will run before idle.
   */
  if (priv->remove_popup_info_idle == 0 &&
      priv->popup_info != NULL)
    {
      priv->remove_popup_info_idle =
        g_idle_add ((GSourceFunc) idle_remove_popup_info, window);
    }
}

static void
popup_menu_deactivate_callback (GtkWidget *popup,
                                TerminalWindow *window)
{
  g_signal_handlers_disconnect_by_func
    (popup, G_CALLBACK (popup_menu_deactivate_callback), window);

  unset_popup_info (window);
}

static void
popup_clipboard_targets_received_cb (GtkClipboard *clipboard,
                                     GdkAtom *targets,
                                     int n_targets,
                                     TerminalScreenPopupInfo *info)
{
  TerminalWindow *window;
  TerminalWindowPrivate *priv;
  TerminalScreen *screen = info->screen;
  GtkWidget *popup_menu;
  GtkAction *action;
  gboolean can_paste, can_paste_uris, show_link, show_email_link, show_call_link, show_number_info;

  window = terminal_screen_popup_info_ref_window (info);
  if (window == NULL ||
      !gtk_widget_get_realized (GTK_WIDGET (screen)))
    {
      terminal_screen_popup_info_unref (info);
      return;
    }

  /* Now we know that the window is still alive */
  priv = window->priv;

  remove_popup_info (window);
  priv->popup_info = info; /* adopt the ref added when requesting the clipboard */

  can_paste = targets != NULL && gtk_targets_include_text (targets, n_targets);
  can_paste_uris = targets != NULL && gtk_targets_include_uri (targets, n_targets);
  show_link = info->url != NULL && (info->url_flavor == FLAVOR_AS_IS || info->url_flavor == FLAVOR_DEFAULT_TO_HTTP);
  show_email_link = info->url != NULL && info->url_flavor == FLAVOR_EMAIL;
  show_call_link = info->url != NULL && info->url_flavor == FLAVOR_VOIP_CALL;
  show_number_info = info->number_info != NULL;

  action = gtk_action_group_get_action (priv->action_group, "PopupSendEmail");
  gtk_action_set_visible (action, show_email_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyEmailAddress");
  gtk_action_set_visible (action, show_email_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCall");
  gtk_action_set_visible (action, show_call_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyCallAddress");
  gtk_action_set_visible (action, show_call_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupOpenLink");
  gtk_action_set_visible (action, show_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupCopyLinkAddress");
  gtk_action_set_visible (action, show_link);
  action = gtk_action_group_get_action (priv->action_group, "PopupNumberInfo");
  gtk_action_set_label (action, info->number_info);
  gtk_action_set_sensitive (action, FALSE);
  gtk_action_set_visible (action, show_number_info);

  action = gtk_action_group_get_action (priv->action_group, "PopupCopy");
  gtk_action_set_sensitive (action, vte_terminal_get_has_selection (VTE_TERMINAL (screen)));
  action = gtk_action_group_get_action (priv->action_group, "PopupPaste");
  gtk_action_set_sensitive (action, can_paste);
  action = gtk_action_group_get_action (priv->action_group, "PopupPasteURIPaths");
  gtk_action_set_visible (action, can_paste_uris);

  popup_menu = gtk_ui_manager_get_widget (priv->ui_manager, "/Popup");
  g_signal_connect (popup_menu, "deactivate",
                    G_CALLBACK (popup_menu_deactivate_callback), window);

  /* Pseudo activation of the popup menu's action */
  action = gtk_action_group_get_action (priv->action_group, "Popup");
  gtk_action_activate (action);

  if (info->button == 0)
    gtk_menu_shell_select_first (GTK_MENU_SHELL (popup_menu), FALSE);

  if (gtk_menu_get_attach_widget (GTK_MENU (popup_menu)))
    gtk_menu_detach (GTK_MENU (popup_menu));
  gtk_menu_attach_to_widget (GTK_MENU (popup_menu), GTK_WIDGET (screen), NULL);
  gtk_menu_popup (GTK_MENU (popup_menu),
                  NULL, NULL,
                  NULL, NULL, 
                  info->button,
                  info->timestamp);

  g_object_unref (window);
}

static void
screen_show_popup_menu_callback (TerminalScreen *screen,
                                 TerminalScreenPopupInfo *info,
                                 TerminalWindow *window)
{
  GtkClipboard *clipboard;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_targets (clipboard,
                                  (GtkClipboardTargetsReceivedFunc) popup_clipboard_targets_received_cb,
                                  terminal_screen_popup_info_ref (info));
}

static gboolean
screen_match_clicked_cb (TerminalScreen *screen,
                         const char *url,
                         int url_flavor,
                         guint state,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (screen != priv->active_screen)
    return FALSE;

  gtk_widget_grab_focus (GTK_WIDGET (screen));
  terminal_util_open_url (GTK_WIDGET (window), url, url_flavor,
                          gtk_get_current_event_time ());

  return TRUE;
}

static void
screen_close_cb (TerminalScreen *screen,
                 TerminalWindow *window)
{
  terminal_window_remove_screen (window, screen);
}

static gboolean
terminal_window_accel_activate_cb (GtkAccelGroup  *accel_group,
                                   GObject        *acceleratable,
                                   guint           keyval,
                                   GdkModifierType modifier,
                                   TerminalWindow *window)
{
  GtkAccelGroupEntry *entries;
  guint n_entries;
  gboolean retval = FALSE;

  entries = gtk_accel_group_query (accel_group, keyval, modifier, &n_entries);
  if (n_entries > 0)
    {
      const char *accel_path;

      accel_path = g_quark_to_string (entries[0].accel_path_quark);

      if (g_str_has_prefix (accel_path, "<Actions>/Main/"))
        {
          const char *action_name;

          /* We want to always consume these accelerators, even if the corresponding
           * action is insensitive, so the corresponding shortcut key escape code
           * isn't sent to the terminal. See bug #453193, bug #138609 and bug #559728.
           * This also makes tab cycling work, bug #92139. (NOT!)
           */

          action_name = I_(accel_path + strlen ("<Actions>/Main/"));

#if 0
          if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook)) > 1 &&
              (action_name == I_("TabsPrevious") || action_name == I_("TabsNext")))
            retval = TRUE;
          else
#endif
               if (action_name == I_("EditCopy") ||
                   action_name == I_("PopupCopy") ||
                   action_name == I_("EditPaste") ||
                   action_name == I_("PopupPaste"))
            retval = TRUE;
        }
    }

  return retval;
}

static void
terminal_window_fill_notebook_action_box (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *box, *button;
  GtkAction *action;
  GtkWidget *menu;

  box = terminal_notebook_get_action_box (TERMINAL_NOTEBOOK (priv->mdi_container), GTK_PACK_END);

  /* Create the NewTerminal button */
  action = gtk_action_group_get_action (priv->action_group, "FileNewTab");

  button = terminal_icon_button_new ("tab-new-symbolic");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* Create Tabs menu button */
  menu = gtk_ui_manager_get_widget (priv->ui_manager, "/TabsPopup");
  gtk_widget_set_halign (menu, GTK_ALIGN_END);

  button = gtk_menu_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu);

  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_menu_button_set_align_widget (GTK_MENU_BUTTON (button), box);
  gtk_widget_show (button);
}

/*****************************************/

#ifdef ENABLE_DEBUG
static void
terminal_window_size_request_cb (GtkWidget *widget,
                                 GtkRequisition *req)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size-request result %d : %d\n",
                         widget, req->width, req->height);
}

static void
terminal_window_size_allocate_cb (GtkWidget *widget,
                                  GtkAllocation *allocation)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size-alloc result %d : %d at (%d, %d)\n",
                         widget,
                         allocation->width, allocation->height,
                         allocation->x, allocation->y);
}
#endif /* ENABLE_DEBUG */

static void
terminal_window_realize (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] realize, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->realize (widget);

  /* Need to do this now since this requires the window to be realized */
  if (priv->active_screen != NULL)
    sync_screen_icon_title (priv->active_screen, NULL, window);

  /* Now that we've been realized, we should know precisely how large the
   * client-side decorations are going to be. Recalculate the geometry hints,
   * export them to the windowing system, and resize the window accordingly. */
  priv->realized = TRUE;
  terminal_window_update_size (window);
}

static gboolean
terminal_window_state_event (GtkWidget            *widget,
                             GdkEventWindowState  *event)
{
  gboolean (* window_state_event) (GtkWidget *, GdkEventWindowState *event) =
      GTK_WIDGET_CLASS (terminal_window_parent_class)->window_state_event;

  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      TerminalWindow *window = TERMINAL_WINDOW (widget);
      TerminalWindowPrivate *priv = window->priv;
      GtkAction *action;
      gboolean is_fullscreen;

      is_fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;

      action = gtk_action_group_get_action (priv->action_group, "ViewFullscreen");
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), is_fullscreen);
  
      action = gtk_action_group_get_action (priv->action_group, "PopupLeaveFullscreen");
      gtk_action_set_visible (action, is_fullscreen);
    }
  
  if (window_state_event)
    return window_state_event (widget, event);

  return FALSE;
}

static void
terminal_window_screen_update (TerminalWindow *window,
                               GdkScreen *screen)
{
  GSettings *settings;
  GtkSettings *gtk_settings;
  char *value;

  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (screen), "GT::HasSettingsConnection")))
    return;

  settings = terminal_app_get_global_settings (terminal_app_get ());
  gtk_settings = gtk_settings_get_for_screen (screen);

  g_object_set_data_full (G_OBJECT (screen), "GT::HasSettingsConnection",
                          gtk_settings,
                          (GDestroyNotify) app_setting_notify_destroy_cb);

  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_MNEMONICS_KEY,
                   gtk_settings,
                   "gtk-enable-mnemonics",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (settings,
                   TERMINAL_SETTING_ENABLE_SHORTCUTS_KEY,
                   gtk_settings,
                   "gtk-enable-accels",
                   G_SETTINGS_BIND_GET);

  g_object_get (gtk_settings, "gtk-menu-bar-accel", &value, NULL);
  g_object_set_data_full (G_OBJECT (gtk_settings), "GT::gtk-menu-bar-accel",
                          value, (GDestroyNotify) g_free);
  enable_menubar_accel_changed_cb (settings, 
                                   TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                                   gtk_settings);
  g_signal_connect (settings, "changed::" TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                    G_CALLBACK (enable_menubar_accel_changed_cb),
                    gtk_settings);
}

static void
terminal_window_screen_changed (GtkWidget *widget,
                                GdkScreen *previous_screen)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  void (* screen_changed) (GtkWidget *, GdkScreen *) =
    GTK_WIDGET_CLASS (terminal_window_parent_class)->screen_changed;
  GdkScreen *screen;

  if (screen_changed)
    screen_changed (widget, previous_screen);

  screen = gtk_widget_get_screen (widget);
  if (previous_screen == screen)
    return;

  if (!screen)
    return;

  terminal_window_screen_update (window, screen);
}

static void
terminal_window_profile_list_changed_cb (TerminalSettingsList *profiles_list,
                                         TerminalWindow *window)
{
  terminal_window_update_set_profile_menu (window);
  terminal_window_update_new_terminal_menus (window);
}

static void
terminal_window_encoding_list_changed_cb (TerminalApp *app,
                                          TerminalWindow *window)
{
  terminal_window_update_encoding_menu (window);
}

static void
terminal_window_init (TerminalWindow *window)
{
  const GActionEntry gaction_entries[] = {
    { "new-terminal",        action_new_terminal_cb,   "(ss)", NULL, NULL },
    { "new-profile",         action_new_profile_cb,    NULL,   NULL, NULL },
    { "save-contents",       action_save_contents_cb,  NULL,   NULL, NULL },
    { "close",               action_close_cb,          "s",    NULL, NULL },
    { "copy",                action_copy_cb,           NULL,   NULL, NULL },
    { "paste",               action_paste_cb,          "s",    NULL, NULL },
    { "select-all",          action_select_all_cb,     NULL,   NULL, NULL },
    { "reset",               action_reset_cb,          "b",    NULL, NULL },
    { "switch-tab",          action_switch_tab_cb,     "i",    NULL, NULL },
    { "move-tab",            action_move_tab_cb,       "i",    NULL, NULL },
    { "zoom",                action_zoom_cb,           "i",    NULL, NULL },
    { "detach-tab",          action_detach_tab_cb,     NULL,   NULL, NULL },
    { "find",                action_find_cb,           "s",    NULL, NULL },
    { "help",                action_help_cb,           NULL,   NULL, NULL },
    { "about",               action_about_cb,          NULL,   NULL, NULL },
    { "preferences",         action_preferences_cb,    NULL,   NULL, NULL },
    { "edit-profile",        action_edit_profile_cb,   "s",    NULL, NULL },

    { "show-menubar",        action_toggle_state_cb,   NULL, "true",  action_show_menubar_state_cb },
    { "fullscreen",          action_toggle_state_cb,   NULL, "false", action_fullscreen_state_cb   },
  };

  const GtkActionEntry menu_entries[] =
    {
      /* Toplevel */
      { "File", NULL, N_("_File") },
      { "FileNewTerminalProfiles", STOCK_NEW_WINDOW, N_("Open _Terminal")},
      { "Edit", NULL, N_("_Edit") },
      { "View", NULL, N_("_View") },
      { "Search", NULL, N_("_Search") },
      { "Terminal", NULL, N_("_Terminal") },
      { "Tabs", NULL, N_("Ta_bs") },
      { "Help", NULL, N_("_Help") },
      { "Popup", NULL, NULL },
      { "NotebookPopup", NULL, "" },
      { "TabsPopup", NULL, "" },

      /* File menu */
      { "FileNewWindow", STOCK_NEW_WINDOW, N_("Open _Terminal"), "<shift><control>N",
        NULL,
        G_CALLBACK (file_new_terminal_callback) },
      { "FileNewTab", STOCK_NEW_TAB, N_("Open Ta_b"), "<shift><control>T",
        NULL,
        G_CALLBACK (file_new_terminal_callback) },
      { "FileNewTerminal", STOCK_NEW_TAB, N_("Open _Terminal"), NULL,
        NULL,
        G_CALLBACK (file_new_terminal_callback) },
      { "FileNewProfile", "document-open", N_("New _Profile"), "",
        NULL,
        G_CALLBACK (file_new_profile_callback) },
      { "FileSaveContents", "document-save", N_("_Save Contents"), "",
        NULL,
        G_CALLBACK (file_save_contents_callback) },
      { "FileCloseTab", "window-close", N_("C_lose Terminal"), "<shift><control>W",
        NULL,
        G_CALLBACK (file_close_tab_callback) },
      { "FileCloseWindow", "window-close", N_("_Close All Terminals"), "<shift><control>Q",
        NULL,
        G_CALLBACK (file_close_window_callback) },

      /* Edit menu */
      { "EditCopy", "edit-copy", N_("Copy"), "<shift><control>C",
        NULL,
        G_CALLBACK (edit_copy_callback) },
      { "EditPaste", "edit-paste", N_("Paste"), "<shift><control>V",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "EditPasteURIPaths", "edit-paste", N_("Paste _Filenames"), "",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "EditSelectAll", "edit-select-all", N_("Select All"), NULL,
        NULL,
        G_CALLBACK (edit_select_all_callback) },
      { "EditPreferences", NULL, N_("Pre_ferences"), NULL,
        NULL,
        G_CALLBACK (edit_preferences_callback) },
      { "EditCurrentProfile", "preferences-system", N_("_Profile Preferences"), NULL,
        NULL,
        G_CALLBACK (edit_current_profile_callback) },

      /* View menu */
      { "ViewZoomIn", "zoom-in", N_("Zoom In"), "<control>plus",
        NULL,
        G_CALLBACK (view_zoom_in_callback) },
      { "ViewZoomOut", "zoom-out", N_("Zoom Out"), "<control>minus",
        NULL,
        G_CALLBACK (view_zoom_out_callback) },
      { "ViewZoom100", "zoom-original", N_("Normal Size"), "<control>0",
        NULL,
        G_CALLBACK (view_zoom_normal_callback) },

      /* Search menu */
      { "SearchFind", "edit-find", N_("_Find…"), "<shift><control>F",
	NULL,
	G_CALLBACK (search_find_callback) },
      { "SearchFindNext", NULL, N_("Find Ne_xt"), "<shift><control>G",
	NULL,
	G_CALLBACK (search_find_next_callback) },
      { "SearchFindPrevious", NULL, N_("Find Pre_vious"), "<shift><control>H",
	NULL,
	G_CALLBACK (search_find_prev_callback) },
      { "SearchClearHighlight", NULL, N_("_Clear Highlight"), "<shift><control>J",
	NULL,
	G_CALLBACK (search_clear_highlight_callback) },
#if 0
      { "SearchGoToLine", "go-jump", N_("Go to _Line..."), "<shift><control>I",
	NULL,
	G_CALLBACK (search_goto_line_callback) },
      { "SearchIncrementalSearch", "edit-find", N_("_Incremental Search..."), "<shift><control>K",
	NULL,
	G_CALLBACK (search_incremental_search_callback) },
#endif

      /* Terminal menu */
      { "TerminalProfiles", NULL, N_("Change _Profile") },
      { "TerminalSetEncoding", NULL, N_("Set _Character Encoding") },
      { "TerminalReset", NULL, N_("_Reset"), NULL,
        NULL,
        G_CALLBACK (terminal_reset_callback) },
      { "TerminalResetClear", NULL, N_("Reset and C_lear"), NULL,
        NULL,
        G_CALLBACK (terminal_reset_clear_callback) },

      /* Terminal/Encodings menu */
      { "TerminalAddEncoding", NULL, N_("_Add or Remove…"), NULL,
        NULL,
        G_CALLBACK (terminal_add_encoding_callback) },

      /* Tabs menu */
      { "TabsPrevious", NULL, N_("_Previous Terminal"), "<control>Page_Up",
        NULL,
        G_CALLBACK (tabs_next_or_previous_tab_cb) },
      { "TabsNext", NULL, N_("_Next Terminal"), "<control>Page_Down",
        NULL,
        G_CALLBACK (tabs_next_or_previous_tab_cb) },
      { "TabsMoveLeft", NULL, N_("Move Terminal _Left"), "<shift><control>Page_Up",
        NULL,
        G_CALLBACK (tabs_move_left_callback) },
      { "TabsMoveRight", NULL, N_("Move Terminal _Right"), "<shift><control>Page_Down",
        NULL,
        G_CALLBACK (tabs_move_right_callback) },
      { "TabsDetach", NULL, N_("_Detach Terminal"), NULL,
        NULL,
        G_CALLBACK (tabs_detach_tab_callback) },

      /* Help menu */
      { "HelpContents", "help-browser", N_("_Contents"), "F1",
        NULL,
        G_CALLBACK (help_contents_callback) },
      { "HelpAbout", "help-about", N_("_About"), NULL,
        NULL,
        G_CALLBACK (help_about_callback) },
      { "HelpInspector", NULL, N_("_Inspector"), NULL,
        NULL,
        G_CALLBACK (help_inspector_callback) },

      /* Popup menu */
      { "PopupSendEmail", NULL, N_("_Send Mail To…"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyEmailAddress", NULL, N_("_Copy E-mail Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupCall", NULL, N_("C_all To…"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyCallAddress", NULL, N_("_Copy Call Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupOpenLink", NULL, N_("_Open Link"), NULL,
        NULL,
        G_CALLBACK (popup_open_url_callback) },
      { "PopupCopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
        NULL,
        G_CALLBACK (popup_copy_url_callback) },
      { "PopupNumberInfo", NULL, "", NULL,
        NULL,
        NULL },
      { "PopupTerminalProfiles", NULL, N_("P_rofiles") },
      { "PopupCopy", "edit-copy", N_("Copy"), "",
        NULL,
        G_CALLBACK (edit_copy_callback) },
      { "PopupPaste", "edit-paste", N_("Paste"), "",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "PopupPasteURIPaths", "edit-paste", N_("Paste _Filenames"), "",
        NULL,
        G_CALLBACK (edit_paste_callback) },
      { "PopupNewTerminal", NULL, N_("Open _Terminal"), NULL,
        NULL,
        G_CALLBACK (file_new_terminal_callback) },
      { "PopupLeaveFullscreen", NULL, N_("L_eave Full Screen"), NULL,
        NULL,
        G_CALLBACK (popup_leave_fullscreen_callback) },
    };
  
  const GtkToggleActionEntry toggle_menu_entries[] =
    {
      /* View Menu */
      { "ViewMenubar", NULL, N_("Show _Menubar"), NULL,
        NULL,
        G_CALLBACK (view_menubar_toggled_callback),
        FALSE },
      { "ViewFullscreen", NULL, N_("_Full Screen"), NULL,
        NULL,
        G_CALLBACK (view_fullscreen_toggled_callback),
        FALSE },
      /* Terminal menu */
      { "TerminalReadOnly", NULL, N_("Read-_Only"), NULL,
        NULL,
        G_CALLBACK (terminal_readonly_toggled_callback),
        FALSE }
    };
  TerminalWindowPrivate *priv;
  TerminalApp *app;
  TerminalSettingsList *profiles_list;
  GSettings *gtk_debug_settings;
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkUIManager *manager;
  GError *error;
  GtkWindowGroup *window_group;
  GtkAccelGroup *accel_group;
  uuid_t u;
  char uuidstr[37], role[64];

  app = terminal_app_get ();

  priv = window->priv = G_TYPE_INSTANCE_GET_PRIVATE (window, TERMINAL_TYPE_WINDOW, TerminalWindowPrivate);

  gtk_widget_init_template (GTK_WIDGET (window));

  uuid_generate (u);
  uuid_unparse (u, uuidstr);
  priv->uuid = g_strdup (uuidstr);

  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK(terminal_window_delete_event),
                    NULL);
#ifdef ENABLE_DEBUG
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_GEOMETRY)
    {
      g_signal_connect_after (window, "size-request", G_CALLBACK (terminal_window_size_request_cb), NULL);
      g_signal_connect_after (window, "size-allocate", G_CALLBACK (terminal_window_size_allocate_cb), NULL);
    }
#endif

  gtk_window_set_title (GTK_WINDOW (window), _("Terminal"));

  priv->active_screen = NULL;

  priv->main_vbox = gtk_bin_get_child (GTK_BIN (window));

  priv->mdi_container = TERMINAL_MDI_CONTAINER (terminal_notebook_new ());

  g_signal_connect (priv->mdi_container, "screen-close-request",
                    G_CALLBACK (screen_close_request_cb), window);

  g_signal_connect_after (priv->mdi_container, "screen-switched",
                          G_CALLBACK (mdi_screen_switched_cb), window);
  g_signal_connect_after (priv->mdi_container, "screen-added",
                          G_CALLBACK (mdi_screen_added_cb), window);
  g_signal_connect_after (priv->mdi_container, "screen-removed",
                          G_CALLBACK (mdi_screen_removed_cb), window);
  g_signal_connect_data (priv->mdi_container, "screens-reordered",
                         G_CALLBACK (terminal_window_update_tabs_menu_sensitivity),
                         window, NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_swapped (priv->mdi_container, "notify::tab-pos",
                            G_CALLBACK (terminal_window_update_geometry), window);
  g_signal_connect_swapped (priv->mdi_container, "notify::show-tabs",
                            G_CALLBACK (terminal_window_update_geometry), window);

  /* FIXME hack hack! */
  if (GTK_IS_NOTEBOOK (priv->mdi_container)) {
  g_signal_connect (priv->mdi_container, "button-press-event",
                    G_CALLBACK (notebook_button_press_cb), window);	
  g_signal_connect (priv->mdi_container, "popup-menu",
                    G_CALLBACK (notebook_popup_menu_cb), window);
  }

  /* FIXME hack */
  if (GTK_IS_NOTEBOOK (priv->mdi_container))
    g_signal_connect (priv->mdi_container, "create-window",
                      G_CALLBACK (handle_tab_droped_on_desktop), window);

  gtk_box_pack_end (GTK_BOX (priv->main_vbox), GTK_WIDGET (priv->mdi_container), TRUE, TRUE, 0);
  gtk_widget_show (GTK_WIDGET (priv->mdi_container));

  priv->old_char_width = -1;
  priv->old_char_height = -1;

  priv->old_chrome_width = -1;
  priv->old_chrome_height = -1;
  priv->old_csd_width = -1;
  priv->old_csd_height = -1;
  priv->old_padding_width = -1;
  priv->old_padding_height = -1;

  priv->old_geometry_widget = NULL;

  /* GAction setup */
  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   gaction_entries, G_N_ELEMENTS (gaction_entries),
                                   window);

  /* Create the UI manager */
  manager = priv->ui_manager = gtk_ui_manager_new ();

  accel_group = gtk_ui_manager_get_accel_group (manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  /* Workaround for bug #453193, bug #138609 and bug #559728 */
  g_signal_connect_after (accel_group, "accel-activate",
                          G_CALLBACK (terminal_window_accel_activate_cb), window);

  /* Create the actions */
  /* Note that this action group name is used in terminal-accels.c; do not change it */
  priv->action_group = action_group = gtk_action_group_new ("Main");
  gtk_action_group_set_translation_domain (action_group, NULL);
  gtk_action_group_add_actions (action_group, menu_entries,
                                G_N_ELEMENTS (menu_entries), window);
  gtk_action_group_add_toggle_actions (action_group,
                                       toggle_menu_entries,
                                       G_N_ELEMENTS (toggle_menu_entries),
                                       window);
  gtk_ui_manager_insert_action_group (manager, action_group, 0);
  g_object_unref (action_group);

  priv->clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  update_edit_menu (priv->clipboard, NULL, window);
  g_signal_connect (priv->clipboard, "owner-change",
                    G_CALLBACK (update_edit_menu), window);

  /* Idem for this action, since the window is not fullscreen. */
  action = gtk_action_group_get_action (priv->action_group, "PopupLeaveFullscreen");
  gtk_action_set_visible (action, FALSE);

#ifndef ENABLE_SAVE
  action = gtk_action_group_get_action (priv->action_group, "FileSaveContents");
  gtk_action_set_visible (action, FALSE);
#endif
  
  /* Load the UI */
  error = NULL;
  priv->ui_id = gtk_ui_manager_add_ui_from_resource (manager,
                                                     TERMINAL_RESOURCES_PATH_PREFIX "/ui/terminal.xml",
                                                     &error);
  g_assert_no_error (error);

  priv->menubar = gtk_ui_manager_get_widget (manager, "/menubar");
  gtk_box_pack_start (GTK_BOX (priv->main_vbox),
		      priv->menubar,
		      FALSE, FALSE, 0);


  /* Maybe make Inspector available */
  action = gtk_action_group_get_action (priv->action_group, "HelpInspector");
  gtk_debug_settings = terminal_app_get_gtk_debug_settings (app);
  if (gtk_debug_settings != NULL)
    g_settings_bind (gtk_debug_settings,
                     "enable-inspector-keybinding",
                     action,
                     "visible",
                     G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);
  else
    gtk_action_set_visible (action, FALSE);

  /* Add tabs menu */
  priv->tabs_menu = terminal_tabs_menu_new (window);

  profiles_list = terminal_app_get_profiles_list (app);
  terminal_window_profile_list_changed_cb (profiles_list, window);
  g_signal_connect (profiles_list, "children-changed",
                    G_CALLBACK (terminal_window_profile_list_changed_cb), window);
  
  terminal_window_encoding_list_changed_cb (app, window);
  g_signal_connect (app, "encoding-list-changed",
                    G_CALLBACK (terminal_window_encoding_list_changed_cb), window);

  terminal_window_set_menubar_visible (window, TRUE);
  priv->use_default_menubar_visibility = TRUE;

  terminal_window_update_size_to_menu (window);

  terminal_window_fill_notebook_action_box (window);

  /* We have to explicitly call this, since screen-changed is NOT
   * emitted for the toplevel the first time!
   */
  terminal_window_screen_update (window, gtk_widget_get_screen (GTK_WIDGET (window)));

  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

  g_snprintf (role, sizeof (role), "gnome-terminal-window-%s", uuidstr);
  gtk_window_set_role (GTK_WINDOW (window), role);
}

static void
terminal_window_style_updated (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->style_updated (widget);

  terminal_window_update_size (window);
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = terminal_window_dispose;
  object_class->finalize = terminal_window_finalize;

  widget_class->show = terminal_window_show;
  widget_class->realize = terminal_window_realize;
  widget_class->window_state_event = terminal_window_state_event;
  widget_class->screen_changed = terminal_window_screen_changed;
  widget_class->style_updated = terminal_window_style_updated;

#if GTK_CHECK_VERSION (3, 14, 0)
{
  GtkWindowClass *window_klass;
  GtkBindingSet *binding_set;

  window_klass = g_type_class_ref (GTK_TYPE_WINDOW);
  binding_set = gtk_binding_set_by_class (window_klass);
  gtk_binding_entry_skip (binding_set, GDK_KEY_I, GDK_CONTROL_MASK|GDK_SHIFT_MASK);
  gtk_binding_entry_skip (binding_set, GDK_KEY_D, GDK_CONTROL_MASK|GDK_SHIFT_MASK);
  g_type_class_unref (window_klass);
}
#endif

  g_type_class_add_private (object_class, sizeof (TerminalWindowPrivate));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/window.ui");

#if GTK_CHECK_VERSION(3, 19, 5)
  gtk_widget_class_set_css_name(widget_class, TERMINAL_WINDOW_CSS_NAME);
#else
  if (gtk_check_version(3, 19, 5) == NULL)
    g_printerr("gnome-terminal needs to be recompiled against a newer gtk+ version.\n");
#endif
}

static void
terminal_window_dispose (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;
  TerminalApp *app;
  TerminalSettingsList *profiles_list;
  GSList *list, *l;

  priv->disposed = TRUE;

  /* Deactivate open popup menus. This fixes a crash if the window is closed
   * while the context menu is open.
   */
  list = gtk_ui_manager_get_toplevels (priv->ui_manager, GTK_UI_MANAGER_POPUP);
  for (l = list; l != NULL; l = l->next)
    if (GTK_IS_MENU (l->data))
      gtk_menu_popdown (GTK_MENU (l->data));
  g_slist_free (list);

  remove_popup_info (window);

  if (priv->search_popover != NULL)
    {
      g_signal_handlers_disconnect_matched (priv->search_popover, G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, window);
      gtk_widget_destroy (GTK_WIDGET (priv->search_popover));
      priv->search_popover = NULL;
    }

  if (priv->tabs_menu)
    {
      g_object_unref (priv->tabs_menu);
      priv->tabs_menu = NULL;
    }

  if (priv->profiles_action_group != NULL)
    disconnect_profiles_from_actions_in_group (priv->profiles_action_group);
  if (priv->new_terminal_action_group != NULL)
    disconnect_profiles_from_actions_in_group (priv->new_terminal_action_group);

  app = terminal_app_get ();
  profiles_list = terminal_app_get_profiles_list (app);
  g_signal_handlers_disconnect_by_func (profiles_list,
                                        G_CALLBACK (terminal_window_profile_list_changed_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (app,
                                        G_CALLBACK (terminal_window_encoding_list_changed_cb),
                                        window);

  g_signal_handlers_disconnect_by_func (priv->clipboard,
                                        G_CALLBACK (update_edit_menu),
                                        window);

  G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}

static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalWindowPrivate *priv = window->priv;

  g_object_unref (priv->ui_manager);

  if (priv->confirm_close_dialog)
    gtk_dialog_response (GTK_DIALOG (priv->confirm_close_dialog),
                         GTK_RESPONSE_DELETE_EVENT);

  g_free (priv->uuid);

  G_OBJECT_CLASS (terminal_window_parent_class)->finalize (object);
}

static gboolean
terminal_window_delete_event (GtkWidget *widget,
                              GdkEvent *event,
                              gpointer data)
{
   return confirm_close_window_or_tab (TERMINAL_WINDOW (widget), NULL);
}

static void
terminal_window_show (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  TerminalWindowPrivate *priv = window->priv;
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] show, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  /* Because of the unexpected reentrancy caused by notebook_page_added_callback()
   * showing the TerminalWindow, we can get here when the first page has been
   * added but not yet set current. By setting the page current, we get the
   * right size when we first show the window */
  if (GTK_IS_NOTEBOOK (priv->mdi_container) &&
      gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->mdi_container)) == -1)
    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->mdi_container), 0);

  if (priv->active_screen != NULL)
    {
      /* At this point, we have our GdkScreen, and hence the right
       * font size, so we can go ahead and size the window. */
      terminal_window_update_size (window);
    }

  GTK_WIDGET_CLASS (terminal_window_parent_class)->show (widget);
}

TerminalWindow*
terminal_window_new (GApplication *app)
{
  return g_object_new (TERMINAL_TYPE_WINDOW,
                       "application", app,
#ifdef ENABLE_DEBUG
                       "show-menubar", _terminal_debug_on (TERMINAL_DEBUG_APPMENU),
#else
                       "show-menubar", FALSE,
#endif
                       NULL);
}

static void
profile_set_callback (TerminalScreen *screen,
                      GSettings *old_profile,
                      TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  terminal_window_update_set_profile_menu_active_profile (window);
}

static void
sync_screen_title (TerminalScreen *screen,
                   GParamSpec *psepc,
                   TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *title;
  
  if (screen != priv->active_screen)
    return;

  title = terminal_screen_get_title (screen);
  gtk_window_set_title (GTK_WINDOW (window), 
                        title && title[0] ? title : _("Terminal"));
}

static void
sync_screen_icon_title (TerminalScreen *screen,
                        GParamSpec *psepc,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  if (!terminal_screen_get_icon_title_set (screen))
    return;

  gdk_window_set_icon_name (gtk_widget_get_window (GTK_WIDGET (window)), terminal_screen_get_icon_title (screen));

  priv->icon_title_set = TRUE;
}

static void
sync_screen_icon_title_set (TerminalScreen *screen,
                            GParamSpec *psepc,
                            TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  /* No need to restore the title if we never set an icon title */
  if (!priv->icon_title_set)
    return;

  if (screen != priv->active_screen)
    return;

  if (terminal_screen_get_icon_title_set (screen))
    return;

  /* Need to reset the icon name */
  /* FIXME: Once gtk+ bug 535557 is fixed, use that to unset the icon title. */

  g_object_set_qdata (G_OBJECT (gtk_widget_get_window (GTK_WIDGET (window))),
                      g_quark_from_static_string ("gdk-icon-name-set"),
                      GUINT_TO_POINTER (FALSE));
  priv->icon_title_set = FALSE;

  /* Re-setting the right title will be done by the notify::title handler which comes after this one */
}

static void
screen_font_any_changed_cb (TerminalScreen *screen,
                            GParamSpec *psepc,
                            TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != priv->active_screen)
    return;

  terminal_window_update_size (window);
}

/* MDI container callbacks */

static void
screen_close_request_cb (TerminalMdiContainer *container,
                         TerminalScreen *screen,
                         TerminalWindow *window)
{
  if (confirm_close_window_or_tab (window, screen))
    return;

  terminal_window_remove_screen (window, screen);
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            int            position)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *old_window;

  old_window = gtk_widget_get_toplevel (GTK_WIDGET (screen));
  if (gtk_widget_is_toplevel (old_window) &&
      TERMINAL_IS_WINDOW (old_window) &&
      TERMINAL_WINDOW (old_window)== window)
    return;  

  if (TERMINAL_IS_WINDOW (old_window))
    terminal_window_remove_screen (TERMINAL_WINDOW (old_window), screen);

  terminal_mdi_container_add_screen (priv->mdi_container, screen);
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_remove_screen (priv->mdi_container, screen);
}

void
terminal_window_move_screen (TerminalWindow *source_window,
                             TerminalWindow *dest_window,
                             TerminalScreen *screen,
                             int dest_position)
{
  TerminalScreenContainer *screen_container;

  g_return_if_fail (TERMINAL_IS_WINDOW (source_window));
  g_return_if_fail (TERMINAL_IS_WINDOW (dest_window));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));
  g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (screen)) == GTK_WIDGET (source_window));
  g_return_if_fail (dest_position >= -1);

  screen_container = terminal_screen_container_get_from_screen (screen);
  g_assert (TERMINAL_IS_SCREEN_CONTAINER (screen_container));

  /* We have to ref the screen container as well as the screen,
   * because otherwise removing the screen container from the source
   * window's notebook will cause the container and its containing
   * screen to be gtk_widget_destroy()ed!
   */
  g_object_ref_sink (screen_container);
  g_object_ref_sink (screen);
  terminal_window_remove_screen (source_window, screen);

  /* Now we can safely remove the screen from the container and let the container die */
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (screen))), GTK_WIDGET (screen));
  g_object_unref (screen_container);

  terminal_window_add_screen (dest_window, screen, dest_position);
  terminal_mdi_container_set_active_screen (dest_window->priv->mdi_container, screen);
  g_object_unref (screen);
}

GList*
terminal_window_list_screen_containers (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return terminal_mdi_container_list_screen_containers (priv->mdi_container);
}

void
terminal_window_set_menubar_visible (TerminalWindow *window,
                                     gboolean        setting)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkAction *action;

  /* it's been set now, so don't override when adding a screen.
   * this side effect must happen before we short-circuit below.
   */
  priv->use_default_menubar_visibility = FALSE;
  
  if (setting == priv->menubar_visible)
    return;

  priv->menubar_visible = setting;

  action = gtk_action_group_get_action (priv->action_group, "ViewMenubar");
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), setting);
  
  g_object_set (priv->menubar, "visible", setting, NULL);

  /* FIXMEchpe: use gtk_widget_get_realized instead? */
  if (priv->active_screen)
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] setting size after toggling menubar visibility\n",
                             window);

      terminal_window_update_size (window);
    }
}

gboolean
terminal_window_get_menubar_visible (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  return priv->menubar_visible;
}

GtkWidget *
terminal_window_get_mdi_container (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
	
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

  return GTK_WIDGET (priv->mdi_container);
}

void
terminal_window_update_size (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int grid_width, grid_height;
  int pixel_width, pixel_height;
  GdkWindow *gdk_window;

  gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

  if (gdk_window != NULL &&
      (gdk_window_get_state (gdk_window) &
       (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_TILED)))
    {
      /* Don't adjust the size of maximized or tiled (snapped, half-maximized)
       * windows: if we do, there will be ugly gaps of up to 1 character cell
       * around otherwise tiled windows. */
      return;
    }

  /* be sure our geometry is up-to-date */
  terminal_window_update_geometry (window);

  terminal_screen_get_size (priv->active_screen, &grid_width, &grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size is %dx%d cells of %dx%d px\n",
                         window, grid_width, grid_height,
                         priv->old_char_width, priv->old_char_height);

  /* the "old" struct members were updated by update_geometry */
  pixel_width = priv->old_chrome_width + grid_width * priv->old_char_width;
  pixel_height = priv->old_chrome_height + grid_height * priv->old_char_height;
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] %dx%d + %dx%d = %dx%d\n",
                         window, grid_width * priv->old_char_width,
                         grid_height * priv->old_char_height,
                         priv->old_chrome_width, priv->old_chrome_height,
                         pixel_width, pixel_height);

  gtk_window_resize (GTK_WINDOW (window), pixel_width, pixel_height);
}

void
terminal_window_switch_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_set_active_screen (priv->mdi_container, screen);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return terminal_mdi_container_get_active_screen (priv->mdi_container);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
                          GdkEventButton *event,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (widget);
  GtkWidget *tab;
  GtkWidget *menu;
  GtkAction *action;
  int tab_clicked;

  if (event->type != GDK_BUTTON_PRESS ||
      event->button != 3 ||
      (event->state & gtk_accelerator_get_default_mod_mask ()) != 0)
    return FALSE;

  tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);
  if (tab_clicked < 0)
    return FALSE;

  /* switch to the page the mouse is over */
  gtk_notebook_set_current_page (notebook, tab_clicked);

  action = gtk_action_group_get_action (priv->action_group, "NotebookPopup");
  gtk_action_activate (action);

  menu = gtk_ui_manager_get_widget (priv->ui_manager, "/NotebookPopup");
  if (gtk_menu_get_attach_widget (GTK_MENU (menu)))
    gtk_menu_detach (GTK_MENU (menu));
  tab = gtk_notebook_get_nth_page (notebook, tab_clicked);
  gtk_menu_attach_to_widget (GTK_MENU (menu), tab, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 
                  NULL, NULL, 
                  event->button, event->time);

  return TRUE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
                        TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkNotebook *notebook = GTK_NOTEBOOK (priv->mdi_container);
  GtkWidget *focus_widget, *tab, *tab_label, *menu;
  GtkAction *action;
  int page_num;

  focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
  /* Only respond if the notebook is the actual focus */
  if (focus_widget != GTK_WIDGET (priv->mdi_container))
    return FALSE;

  page_num = gtk_notebook_get_current_page (notebook);
  tab = gtk_notebook_get_nth_page (notebook, page_num);
  tab_label = gtk_notebook_get_tab_label (notebook, tab);

  action = gtk_action_group_get_action (priv->action_group, "NotebookPopup");
  gtk_action_activate (action);

  menu = gtk_ui_manager_get_widget (priv->ui_manager, "/NotebookPopup");
  if (gtk_menu_get_attach_widget (GTK_MENU (menu)))
    gtk_menu_detach (GTK_MENU (menu));
  gtk_menu_attach_to_widget (GTK_MENU (menu), tab_label, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, 
                  position_menu_under_widget, tab_label,
                  0, gtk_get_current_event_time ());
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);

  return TRUE;
}

static void
mdi_screen_switched_cb (TerminalMdiContainer *container,
                        TerminalScreen *old_active_screen,
                        TerminalScreen *screen,
                        TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int old_grid_width, old_grid_height;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen-switched old %p new %p\n",
                         window, old_active_screen, screen);

  if (priv->disposed)
    return;

  if (screen == NULL || old_active_screen == screen)
    return;

  if (priv->search_popover != NULL)
    gtk_widget_hide (GTK_WIDGET (priv->search_popover));

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: setting active tab to screen %p (old active screen %p)\n",
                         window, screen, priv->active_screen);

  if (old_active_screen != NULL && screen != NULL) {
    terminal_screen_get_size (old_active_screen, &old_grid_width, &old_grid_height);

    /* This is so that we maintain the same grid */
    vte_terminal_set_size (VTE_TERMINAL (screen), old_grid_width, old_grid_height);
  }

  priv->active_screen = screen;

  /* Override menubar setting if it wasn't restored from session */
  if (priv->use_default_menubar_visibility)
    {
      gboolean setting =
        g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                TERMINAL_SETTING_DEFAULT_SHOW_MENUBAR_KEY);

      terminal_window_set_menubar_visible (window, setting);
    }

  sync_screen_icon_title_set (screen, NULL, window);
  sync_screen_icon_title (screen, NULL, window);
  sync_screen_title (screen, NULL, window);

  /* set size of window to current grid size */
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] setting size after flipping notebook pages\n",
                         window);
  terminal_window_update_size (window);

  terminal_window_update_tabs_menu_sensitivity (window);
  terminal_window_update_encoding_menu_active_encoding (window);
  terminal_window_update_terminal_menu (window);
  terminal_window_update_set_profile_menu_active_profile (window);
  terminal_window_update_copy_sensitivity (screen, window);
  terminal_window_update_zoom_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);
}

static void
mdi_screen_added_cb (TerminalMdiContainer *container,
                     TerminalScreen *screen,
                     TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int pages;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p inserted\n",
                         window, screen);

  g_signal_connect (G_OBJECT (screen),
                    "profile-set",
                    G_CALLBACK (profile_set_callback),
                    window);

  /* FIXME: only connect on the active screen, not all screens! */
  g_signal_connect (screen, "notify::title",
                    G_CALLBACK (sync_screen_title), window);
  g_signal_connect (screen, "notify::icon-title",
                    G_CALLBACK (sync_screen_icon_title), window);
  g_signal_connect (screen, "notify::icon-title-set",
                    G_CALLBACK (sync_screen_icon_title_set), window);
  g_signal_connect (screen, "notify::font-desc",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::font-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "selection-changed",
                    G_CALLBACK (terminal_window_update_copy_sensitivity), window);

  g_signal_connect (screen, "show-popup-menu",
                    G_CALLBACK (screen_show_popup_menu_callback), window);
  g_signal_connect (screen, "match-clicked",
                    G_CALLBACK (screen_match_clicked_cb), window);
  g_signal_connect (screen, "resize-window",
                    G_CALLBACK (screen_resize_window_cb), window);

  g_signal_connect (screen, "close-screen",
                    G_CALLBACK (screen_close_cb), window);

  terminal_window_update_tabs_menu_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);

#if 0
  /* FIXMEchpe: wtf is this doing? */

  /* If we have an active screen, match its size and zoom */
  if (priv->active_screen)
    {
      int current_width, current_height;
      double scale;

      terminal_screen_get_size (priv->active_screen, &current_width, &current_height);
      vte_terminal_set_size (VTE_TERMINAL (screen), current_width, current_height);

      scale = terminal_screen_get_font_scale (priv->active_screen);
      terminal_screen_set_font_scale (screen, scale);
    }
#endif

  if (priv->present_on_insert)
    {
      gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
      priv->present_on_insert = FALSE;
    }

  pages = terminal_mdi_container_get_n_screens (container);
  if (pages == 2)
    {
      terminal_window_update_size (window);
    }
}

static void
mdi_screen_removed_cb (TerminalMdiContainer *container,
                       TerminalScreen *screen,
                       TerminalWindow  *window)
{
  TerminalWindowPrivate *priv = window->priv;
  int pages;

  if (priv->disposed)
    return;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p removed\n",
                         window, screen);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (profile_set_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (sync_screen_title),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (sync_screen_icon_title),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (sync_screen_icon_title_set),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (screen_font_any_changed_cb),
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        G_CALLBACK (terminal_window_update_copy_sensitivity),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_show_popup_menu_callback),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_match_clicked_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_resize_window_cb),
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        G_CALLBACK (screen_close_cb),
                                        window);

  /* We already got a switch-page signal whose handler sets the active tab to the
   * new active tab, unless this screen was the only one in the notebook, so
   * priv->active_tab is valid here.
   */

  pages = terminal_mdi_container_get_n_screens (container);
  if (pages == 0)
    {
      priv->active_screen = NULL;

      /* That was the last tab in the window; close it. */
      gtk_widget_destroy (GTK_WIDGET (window));
      return;
    }

  terminal_window_update_tabs_menu_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);

  if (pages == 1)
    {
      TerminalScreen *active_screen = terminal_mdi_container_get_active_screen (container);
      gtk_widget_grab_focus (GTK_WIDGET(active_screen));  /* bug 742422 */

      terminal_window_update_size (window);
    }
}

gboolean
terminal_window_parse_geometry (TerminalWindow *window,
				const char     *geometry)
{
  TerminalWindowPrivate *priv = window->priv;

  /* gtk_window_parse_geometry() needs to have the right base size
   * and width/height increment to compute the window size from
   * the geometry.
   */
  terminal_window_update_geometry (window);

  if (!gtk_window_parse_geometry (GTK_WINDOW (window), geometry))
    return FALSE;

  /* We won't actually get allocated at the size parsed out of the
   * geometry until the window is shown. If terminal_window_update_size()
   * is called between now and then, that could result in us getting
   * snapped back to the old grid size. So we need to immediately
   * update the size of the active terminal to grid size from the
   * geometry.
   */
  if (priv->active_screen)
    {
      int grid_width, grid_height;

      /* After parse_geometry(), the default size is in units of the
       * width/height increment, not a pixel size */
      gtk_window_get_default_size (GTK_WINDOW (window), &grid_width, &grid_height);

      vte_terminal_set_size (VTE_TERMINAL (priv->active_screen),
			     grid_width, grid_height);
    }

  return TRUE;
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *widget;
  GdkGeometry hints;
  GtkBorder padding;
  GtkRequisition vbox_request, widget_request;
  int grid_width, grid_height;
  int char_width, char_height;
  int chrome_width, chrome_height;
  int csd_width = 0, csd_height = 0;
  
  if (priv->active_screen == NULL)
    return;

  widget = GTK_WIDGET (priv->active_screen);

  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */
  terminal_screen_get_cell_size (priv->active_screen, &char_width, &char_height);

  terminal_screen_get_size (priv->active_screen, &grid_width, &grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "%dx%d cells of %dx%d px = %dx%d px\n",
                         grid_width, grid_height, char_width, char_height,
                         char_width * grid_width, char_height * grid_height);

  gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                gtk_widget_get_state_flags(widget),
                                &padding);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "padding = %dx%d px\n",
                         padding.left + padding.right,
                         padding.top + padding.bottom);

  gtk_widget_get_preferred_size (priv->main_vbox, NULL, &vbox_request);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "content area requests %dx%d px\n",
                         vbox_request.width, vbox_request.height);


  chrome_width = vbox_request.width - (char_width * grid_width);
  chrome_height = vbox_request.height - (char_height * grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "chrome: %dx%d px\n",
                         chrome_width, chrome_height);

  if (priv->realized)
    {
      /* Only when having been realize the CSD can be calculated. Do this by
       * using the actual allocation rather then the preferred size as the
       * the preferred size takes the natural size of e.g. the title bar into
       * account which can be far wider then the contents size when using a
       * very long title */
      GtkAllocation toplevel_allocation, vbox_allocation;

      gtk_widget_get_allocation (GTK_WIDGET (priv->main_vbox), &vbox_allocation);
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "terminal widget allocation %dx%d px\n",
                         vbox_allocation.width, vbox_allocation.height);

      gtk_widget_get_allocation (GTK_WIDGET (window), &toplevel_allocation);
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "window allocation %dx%d px\n",
                         toplevel_allocation.width, toplevel_allocation.height);

      csd_width =  toplevel_allocation.width - vbox_allocation.width;
      csd_height = toplevel_allocation.height - vbox_allocation.height;
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "CSDs: %dx%d px\n",
                             csd_width, csd_height);
    }

  gtk_widget_get_preferred_size (widget, NULL, &widget_request);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "terminal widget requests %dx%d px\n",
                         widget_request.width, widget_request.height);

  if (!priv->realized)
    {
      /* Don't actually set the geometry hints until we have been realized,
       * because we don't know how large the client-side decorations are going
       * to be. We also avoid setting priv->old_csd_width or
       * priv->old_csd_height, so that next time through this function we'll
       * definitely recalculate the hints.
       *
       * Similarly, the size request doesn't seem to include the padding
       * until we've been redrawn at least once. Don't resize the window
       * until we've done that. */
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "not realized yet\n");
    }
  else if (char_width != priv->old_char_width ||
      char_height != priv->old_char_height ||
      padding.left + padding.right != priv->old_padding_width ||
      padding.top + padding.bottom != priv->old_padding_height ||
      chrome_width != priv->old_chrome_width ||
      chrome_height != priv->old_chrome_height ||
      csd_width != priv->old_csd_width ||
      csd_height != priv->old_csd_height ||
      widget != (GtkWidget*) priv->old_geometry_widget)
    {
      hints.base_width = chrome_width + csd_width;
      hints.base_height = chrome_height + csd_height;

#define MIN_WIDTH_CHARS 4
#define MIN_HEIGHT_CHARS 1
      
      hints.width_inc = char_width;
      hints.height_inc = char_height;

      /* min size is min size of the whole window, remember. */
      hints.min_width = hints.base_width + hints.width_inc * MIN_WIDTH_CHARS;
      hints.min_height = hints.base_height + hints.height_inc * MIN_HEIGHT_CHARS;
      
      gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                     NULL,
                                     &hints,
                                     GDK_HINT_RESIZE_INC |
                                     GDK_HINT_MIN_SIZE |
                                     GDK_HINT_BASE_SIZE);

      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: base %dx%d min %dx%d inc %d %d\n",
                             window,
                             hints.base_width,
                             hints.base_height,
                             hints.min_width,
                             hints.min_height,
                             hints.width_inc,
                             hints.height_inc);

      priv->old_csd_width = csd_width;
      priv->old_csd_height = csd_height;
      priv->old_geometry_widget = widget;
    }
  else
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: increment unchanged, not setting\n",
                             window);
    }

  /* We need these for the size calculation in terminal_window_update_size()
   * (at least under GTK >= 3.19), so we set them unconditionally. */
  priv->old_char_width = char_width;
  priv->old_char_height = char_height;
  priv->old_chrome_width = chrome_width;
  priv->old_chrome_height = chrome_height;
  priv->old_padding_width = padding.left + padding.right;
  priv->old_padding_height = padding.top + padding.bottom;
}

static void
confirm_close_response_cb (GtkWidget *dialog,
                           int response,
                           TerminalWindow *window)
{
  TerminalScreen *screen;

  screen = g_object_get_data (G_OBJECT (dialog), "close-screen");

  gtk_widget_destroy (dialog);

  if (response != GTK_RESPONSE_ACCEPT)
    return;
    
  if (screen)
    terminal_window_remove_screen (window, screen);
  else
    gtk_widget_destroy (GTK_WIDGET (window));
}

/* Returns: TRUE if closing needs to wait until user confirmation;
 * FALSE if the terminal or window can close immediately.
 */
static gboolean
confirm_close_window_or_tab (TerminalWindow *window,
                             TerminalScreen *screen)
{
  TerminalWindowPrivate *priv = window->priv;
  GtkWidget *dialog;
  gboolean do_confirm;
  int n_tabs;

  if (priv->confirm_close_dialog)
    {
      /* WTF, already have one? It's modal, so how did that happen? */
      gtk_dialog_response (GTK_DIALOG (priv->confirm_close_dialog),
                           GTK_RESPONSE_DELETE_EVENT);
    }

  do_confirm = g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                       TERMINAL_SETTING_CONFIRM_CLOSE_KEY);
  if (!do_confirm)
    return FALSE;

  if (screen)
    {
      do_confirm = terminal_screen_has_foreground_process (screen, NULL, NULL);
      n_tabs = 1;
    }
  else
    {
      GList *tabs, *t;

      do_confirm = FALSE;

      tabs = terminal_window_list_screen_containers (window);
      n_tabs = g_list_length (tabs);

      for (t = tabs; t != NULL; t = t->next)
        {
          TerminalScreen *terminal_screen;

          terminal_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (t->data));
          if (terminal_screen_has_foreground_process (terminal_screen, NULL, NULL))
            {
              do_confirm = TRUE;
              break;
            }
        }
      g_list_free (tabs);
    }

  if (!do_confirm)
    return FALSE;

  dialog = priv->confirm_close_dialog =
    gtk_message_dialog_new (GTK_WINDOW (window),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_MESSAGE_WARNING,
                            GTK_BUTTONS_CANCEL,
                            "%s", n_tabs > 1 ? _("Close this window?") : _("Close this terminal?"));

  if (n_tabs > 1)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", _("There are still processes running in some terminals in this window. "
                                                      "Closing the window will kill all of them."));
  else
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", _("There is still a process running in this terminal. "
                                                      "Closing the terminal will kill it."));

  gtk_window_set_title (GTK_WINDOW (dialog), ""); 

  gtk_dialog_add_button (GTK_DIALOG (dialog), n_tabs > 1 ? _("C_lose Window") : _("C_lose Terminal"), GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  g_object_set_data (G_OBJECT (dialog), "close-screen", screen);

  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed), &priv->confirm_close_dialog);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (confirm_close_response_cb), window);

  gtk_window_present (GTK_WINDOW (dialog));

  return TRUE;
}

static void
file_close_window_callback (GtkAction *action,
                            TerminalWindow *window)
{
  terminal_window_request_close (window);
}

static void
file_close_tab_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  TerminalScreen *active_screen = priv->active_screen;
  
  if (!active_screen)
    return;

  if (confirm_close_window_or_tab (window, active_screen))
    return;

  terminal_window_remove_screen (window, active_screen);
}

static void
edit_preferences_callback (GtkAction *action,
                           TerminalWindow *window)
{
  terminal_app_edit_preferences (terminal_app_get (), GTK_WINDOW (window));
}

static void
edit_current_profile_callback (GtkAction *action,
                               TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_edit_profile (terminal_app_get (),
                             terminal_screen_get_profile (priv->active_screen),
                             GTK_WINDOW (window),
                             NULL);
}

static void
file_new_profile_callback (GtkAction *action,
                           TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  
  terminal_app_new_profile (terminal_app_get (),
                            terminal_screen_get_profile (priv->active_screen),
                            GTK_WINDOW (window));
}

static void
view_menubar_toggled_callback (GtkToggleAction *action,
                               TerminalWindow *window)
{
  terminal_window_set_menubar_visible (window, gtk_toggle_action_get_active (action));
}

static void
view_fullscreen_toggled_callback (GtkToggleAction *action,
                                  TerminalWindow *window)
{
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (window)));

  if (gtk_toggle_action_get_active (action))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
terminal_add_encoding_callback (GtkAction *action,
                                TerminalWindow *window)
{
  terminal_app_edit_encodings (terminal_app_get (),
                               GTK_WINDOW (window));
}

static void
terminal_reset_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == NULL)
    return;
      
  vte_terminal_reset (VTE_TERMINAL (priv->active_screen), TRUE, FALSE);
}

static void
terminal_reset_clear_callback (GtkAction *action,
                               TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == NULL)
    return;
      
  vte_terminal_reset (VTE_TERMINAL (priv->active_screen), TRUE, TRUE);
}

static void
terminal_readonly_toggled_callback (GtkToggleAction *action,
                                    TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  if (priv->active_screen == NULL)
    return;

  vte_terminal_set_input_enabled(VTE_TERMINAL(priv->active_screen),
                                 !gtk_toggle_action_get_active (action));
}

static void
tabs_next_or_previous_tab_cb (GtkAction *action,
                              TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;
  const char *name;
  guint keyval = 0;

  name = gtk_action_get_name (action);
  if (strcmp (name, "TabsNext") == 0) {
    keyval = GDK_KEY_Page_Down;
  } else if (strcmp (name, "TabsPrevious") == 0) {
    keyval = GDK_KEY_Page_Up;
  }

  /* FIXMEchpe this is GtkNotebook specific */
  gtk_bindings_activate (G_OBJECT (priv->mdi_container),
                         keyval,
                         GDK_CONTROL_MASK);
}

static void
tabs_move_left_callback (GtkAction *action,
                         TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         -1);
}

static void
tabs_move_right_callback (GtkAction *action,
                          TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  terminal_mdi_container_reorder_screen (priv->mdi_container,
                                         terminal_mdi_container_get_active_screen (priv->mdi_container),
                                         +1);
}

static void
help_contents_callback (GtkAction *action,
                        TerminalWindow *window)
{
  terminal_util_show_help (NULL, GTK_WINDOW (window));
}

static void
help_about_callback (GtkAction *action,
                     TerminalWindow *window)
{
  terminal_util_show_about (NULL);
}

static void
help_inspector_callback (GtkAction *action,
                         TerminalWindow *window)
{
  gtk_window_set_interactive_debugging (TRUE);
}

GtkUIManager *
terminal_window_get_ui_manager (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return priv->ui_manager;
}

void
terminal_window_request_close (TerminalWindow *window)
{
  g_return_if_fail (TERMINAL_IS_WINDOW (window));

  if (confirm_close_window_or_tab (window, NULL))
    return;

  gtk_widget_destroy (GTK_WIDGET (window));
}

GtkActionGroup *
terminal_window_get_main_action_group (TerminalWindow *window)
{
  TerminalWindowPrivate *priv = window->priv;

  return priv->action_group;
}

const char *
terminal_window_get_uuid (TerminalWindow *window)
{
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

  return window->priv->uuid;
}
