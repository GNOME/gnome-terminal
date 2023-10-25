/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2007, 2008, 2009, 2011, 2017 Christian Persch
 * Copyright © 2023 Christian Hergert
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

#ifdef GDK_WINDOWING_X11
# include <gdk/x11/gdkx.h>
#endif

#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-enums.hh"
#include "terminal-headerbar.hh"
#include "terminal-icon-button.hh"
#include "terminal-intl.hh"
#include "terminal-notebook.hh"
#include "terminal-schemas.hh"
#include "terminal-screen-container.hh"
#include "terminal-search-popover.hh"
#include "terminal-tab-label.hh"
#include "terminal-util.hh"
#include "terminal-window.hh"
#include "terminal-libgsystem.hh"

struct _TerminalWindow
{
  AdwApplicationWindow parent_instance;

  char *uuid;

  GdkClipboard *clipboard;

  TerminalScreenPopupInfo *popup_info;

  GtkWidget *titlebar;
  AdwTabBar *tab_bar;
  TerminalHeaderbar *headerbar;
  TerminalNotebook *notebook;
  GtkWidget *main_vbox;
  GtkWidget* ask_default_infobar;
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

  /* For restoring hints after unmaximizing etc */
#ifdef GTK4_TODO
  GdkGeometry hints;
#endif
  GdkToplevelState window_state;

  GtkWidget *confirm_close_dialog;
  TerminalSearchPopover *search_popover;
  GtkPopoverMenu *context_menu;

  guint disposed : 1;
  guint present_on_insert : 1;

  guint realized : 1;
};

#define MIN_WIDTH_CHARS 4
#define MIN_HEIGHT_CHARS 1

#if 1
/*
 * We don't want to enable content saving until vte supports it async.
 * So we disable this code for stable versions.
 */
#include "terminal-version.hh"

#if (TERMINAL_MINOR_VERSION & 1) != 0
#define ENABLE_SAVE
#else
#undef ENABLE_SAVE
#endif
#endif

/* See bug #789356 and issue gnome-terminal#129*/
static inline constexpr auto
window_state_is_snapped(GdkToplevelState state) noexcept
{
  return (state & (GDK_TOPLEVEL_STATE_FULLSCREEN |
                   GDK_TOPLEVEL_STATE_MAXIMIZED |
                   GDK_TOPLEVEL_STATE_BOTTOM_TILED |
                   GDK_TOPLEVEL_STATE_LEFT_TILED |
                   GDK_TOPLEVEL_STATE_RIGHT_TILED |
                   GDK_TOPLEVEL_STATE_TOP_TILED |
                   GDK_TOPLEVEL_STATE_TILED)) != 0;
}

static void terminal_window_dispose  (GObject *object);
static void terminal_window_finalize (GObject *object);

static void terminal_window_state_event (GtkWidget  *widget,
                                         GParamSpec *pspec,
                                         GdkToplevel *surface);

static gboolean terminal_window_close_request (GtkWindow *window);

static void notebook_screen_switched_cb   (TerminalNotebook *notebook,
                                           TerminalScreen   *old_active_screen,
                                           TerminalScreen   *screen,
                                           TerminalWindow   *window);
static void notebook_screen_added_cb      (TerminalNotebook *notebook,
                                           TerminalScreen   *screen,
                                           TerminalWindow   *window);
static void notebook_screen_removed_cb    (TerminalNotebook *notebook,
                                           TerminalScreen   *screen,
                                           TerminalWindow   *window);
static void notebook_screens_reordered_cb (TerminalNotebook *notebook,
                                           TerminalWindow   *window);

static gboolean screen_close_request_cb (TerminalNotebook *notebook,
                                         TerminalScreen   *screen,
                                         TerminalWindow   *window);

/* Menu action callbacks */
static gboolean find_larger_zoom_factor                   (double         *zoom);
static gboolean find_smaller_zoom_factor                  (double         *zoom);
static void     terminal_window_update_zoom_sensitivity   (TerminalWindow *window);
static void     terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                                           TerminalWindow *window);
static void     terminal_window_update_paste_sensitivity  (TerminalWindow *window);
static void     terminal_window_show                      (GtkWidget      *widget);
static gboolean confirm_close_window_or_tab               (TerminalWindow *window,
                                                           TerminalScreen *screen);

G_DEFINE_FINAL_TYPE (TerminalWindow, terminal_window, ADW_TYPE_APPLICATION_WINDOW)

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
find_larger_zoom_factor (double *zoom)
{
  double current = *zoom;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (zoom_factors); ++i)
    {
      /* Find a font that's larger than this one */
      if ((zoom_factors[i] - current) > 1e-6)
        {
          *zoom = zoom_factors[i];
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
find_smaller_zoom_factor (double *zoom)
{
  double current = *zoom;
  int i;

  i = (int) G_N_ELEMENTS (zoom_factors) - 1;
  while (i >= 0)
    {
      /* Find a font that's smaller than this one */
      if ((current - zoom_factors[i]) > 1e-6)
        {
          *zoom = zoom_factors[i];
          return TRUE;
        }

      --i;
    }

  return FALSE;
}

static inline GSimpleAction *
lookup_action (TerminalWindow *window,
               const char *name)
{
  GAction *action;

  action = g_action_map_lookup_action (G_ACTION_MAP (window), name);
  g_return_val_if_fail (action != nullptr, nullptr);

  return G_SIMPLE_ACTION (action);
}

/* GAction callbacks */

static void
action_new_terminal_cb (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer      user_data)
{
  TerminalWindow *window = TERMINAL_WINDOW (user_data);
  TerminalApp *app;
  TerminalSettingsList *profiles_list;
  gs_unref_object GSettings *profile = nullptr;
  gboolean can_toggle = FALSE;

  g_assert (TERMINAL_IS_WINDOW (window));

  app = terminal_app_get ();

  const char *mode_str, *uuid_str;
  g_variant_get (parameter, "(&s&s)", &mode_str, &uuid_str);

  TerminalNewTerminalMode mode;
  if (g_str_equal (mode_str, "tab"))
    mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
  else if (g_str_equal (mode_str, "window"))
    mode = TERMINAL_NEW_TERMINAL_MODE_WINDOW;
  else if (g_str_equal (mode_str, "tab-default")) {
    mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
    can_toggle = TRUE;
  } else {
    mode = TerminalNewTerminalMode(g_settings_get_enum (terminal_app_get_global_settings (app),
						      TERMINAL_SETTING_NEW_TERMINAL_MODE_KEY));
    can_toggle = TRUE;
  }

  if (can_toggle) {
#ifdef GTK4_TODO
    GdkEvent *event = gtk_get_current_event ();
    if (event != nullptr) {
      GdkModifierType modifiers;

      if ((gdk_event_get_state (event, &modifiers) &&
           (modifiers & gtk_accelerator_get_default_mod_mask () & GDK_CONTROL_MASK))) {
        /* Invert */
        if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
          mode = TERMINAL_NEW_TERMINAL_MODE_TAB;
        else
          mode = TERMINAL_NEW_TERMINAL_MODE_WINDOW;
      }
      gdk_event_free (event);
    }
#endif
  }

  TerminalScreen *parent_screen = window->active_screen;

  profiles_list = terminal_app_get_profiles_list (app);
  if (g_str_equal (uuid_str, "current"))
    profile = terminal_screen_ref_profile (parent_screen);
  else if (g_str_equal (uuid_str, "default"))
    profile = terminal_settings_list_ref_default_child (profiles_list);
  else
    profile = terminal_settings_list_ref_child (profiles_list, uuid_str);

  if (profile == nullptr)
    return;

  if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
    window = terminal_window_new (G_APPLICATION (app));

  TerminalScreen *screen = terminal_screen_new (profile,
                                                nullptr /* title */,
                                                1.0);

  /* Now add the new screen to the window */
  terminal_window_add_screen (window, screen, -1);
  terminal_window_switch_screen (window, screen);
  gtk_widget_grab_focus (GTK_WIDGET (screen));

  /* Start child process, if possible by using the same args as the parent screen */
  terminal_screen_reexec_from_screen (screen, parent_screen, nullptr, nullptr);

  if (mode == TERMINAL_NEW_TERMINAL_MODE_WINDOW)
    gtk_window_present (GTK_WINDOW (window));
}

#ifdef ENABLE_SAVE

static void
save_contents_dialog_on_response (GtkDialog *dialog,
				  int response_id,
				  gpointer user_data)
{
  VteTerminal *terminal = (VteTerminal*)user_data;
  GtkWindow *parent;
  gs_unref_object GFile *file = nullptr;
  GOutputStream *stream;
  gs_free_error GError *error = nullptr;

  if (response_id != GTK_RESPONSE_ACCEPT)
    {
      gtk_window_destroy (GTK_WINDOW (dialog));
      return;
    }

  parent = (GtkWindow*) gtk_widget_get_ancestor (GTK_WIDGET (terminal), GTK_TYPE_WINDOW);

  gtk_window_destroy (GTK_WINDOW (dialog));

  stream = G_OUTPUT_STREAM (g_file_replace (file, nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error));

  if (stream)
    {
      /* XXX
       * FIXME
       * This is a sync operation.
       * Should be replaced with the async version when vte implements that.
       */
      vte_terminal_write_contents_sync (terminal, stream,
					VTE_WRITE_DEFAULT,
					nullptr, &error);
      g_object_unref (stream);
    }

  if (error)
    {
      terminal_util_show_error_dialog (parent, nullptr, error,
				       "%s", _("Could not save contents"));
    }
}

static void
action_save_contents_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  GtkWidget *dialog = nullptr;
  VteTerminal *terminal;
  gs_unref_object GFile *file = nullptr;

  if (window->active_screen == nullptr)
    return;

  terminal = VTE_TERMINAL (window->active_screen);
  g_return_if_fail (VTE_IS_TERMINAL (terminal));

  dialog = gtk_file_chooser_dialog_new (_("Save as…"),
                                        GTK_WINDOW(window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        nullptr);

  file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS));
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), file, nullptr);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  g_signal_connect (dialog, "response", G_CALLBACK (save_contents_dialog_on_response), terminal);
  g_signal_connect (dialog, "delete_event", G_CALLBACK (terminal_util_dialog_response_on_delete), nullptr);

  gtk_window_present (GTK_WINDOW (dialog));
}

#endif /* ENABLE_SAVE */

#ifdef ENABLE_PRINT

static void
print_begin_cb (GtkPrintOperation *op,
                GtkPrintContext *context,
                TerminalApp *app)
{
  GtkPrintSettings *settings;
  GtkPageSetup *page_setup;

  /* Don't save if the print dialogue was cancelled */
  if (gtk_print_operation_get_status(op) == GTK_PRINT_STATUS_FINISHED_ABORTED)
    return;

  settings = gtk_print_operation_get_print_settings (op);
  page_setup = gtk_print_operation_get_default_page_setup (op);
  terminal_util_save_print_settings (settings, page_setup);
}

static void
print_done_cb (GtkPrintOperation *op,
               GtkPrintOperationResult result,
               TerminalWindow *window)
{
  if (result != GTK_PRINT_OPERATION_RESULT_ERROR)
    return;

  /* FIXME: show error */
}

static void
action_print_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  gs_unref_object GtkPrintSettings *settings = nullptr;
  gs_unref_object GtkPageSetup *page_setup = nullptr;
  gs_unref_object GtkPrintOperation *op = nullptr;
  gs_free_error GError *error = nullptr;
  GtkPrintOperationResult result;

  if (window->active_screen == nullptr)
    return;

  op = vte_print_operation_new (VTE_TERMINAL (window->active_screen),
                                VTE_PRINT_OPERATION_DEFAULT /* flags */);
  if (op == nullptr)
    return;

  terminal_util_load_print_settings (&settings, &page_setup);
  if (settings != nullptr)
    gtk_print_operation_set_print_settings (op, settings);
  if (page_setup != nullptr)
    gtk_print_operation_set_default_page_setup (op, page_setup);

  g_signal_connect (op, "begin-print", G_CALLBACK (print_begin_cb), window);
  g_signal_connect (op, "done", G_CALLBACK (print_done_cb), window);

  /* FIXME: show progress better */

  result = gtk_print_operation_run (op,
                                    /* this is the only supported one: */
                                    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                    GTK_WINDOW (window),
                                    &error);
  /* VtePrintOperation always runs async */
  g_assert_cmpint (result, ==, GTK_PRINT_OPERATION_RESULT_IN_PROGRESS);
}

#endif /* ENABLE_PRINT */

#ifdef ENABLE_EXPORT

static void
action_export_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = user_data;
  gs_unref_object VteExportOperation *op = nullptr;
  gs_free_error GError *error = nullptr;

  if (window->active_screen == nullptr)
    return;

  op = vte_export_operation_new (VTE_TERMINAL (window->active_screen),
                                 TRUE /* interactive */,
                                 VTE_EXPORT_FORMAT_ASK /* allow user to choose export format */,
                                 nullptr, nullptr /* GSettings & key to load/store default directory from, FIXME */,
                                 nullptr, nullptr /* progress callback & user data, FIXME */);
  if (op == nullptr)
    return;

  /* FIXME: show progress better */

  vte_export_operation_run_async (op, GTK_WINDOW (window), nullptr /* cancellable */);
}

#endif /* ENABLE_EXPORT */

static void
action_close_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalScreen *screen;
  const char *mode_str;

  g_assert_nonnull (parameter);
  g_variant_get (parameter, "&s", &mode_str);

  if (g_str_equal (mode_str, "tab"))
    screen = window->active_screen;
  else if (g_str_equal (mode_str, "window"))
    screen = nullptr;
  else
    return;

  if (confirm_close_window_or_tab (window, screen))
    return;

  if (screen)
    terminal_window_remove_screen (window, screen);
  else
    gtk_window_destroy (GTK_WINDOW (window));
}

static void
action_copy_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  const char *format_str;
  VteFormat format;

  if (window->active_screen == nullptr)
    return;

  g_assert_nonnull (parameter);
  g_variant_get (parameter, "&s", &format_str);

  if (g_str_equal (format_str, "text"))
    format = VTE_FORMAT_TEXT;
  else if (g_str_equal (format_str, "html"))
    format = VTE_FORMAT_HTML;
  else
    return;

  vte_terminal_copy_clipboard_format (VTE_TERMINAL (window->active_screen), format);
}

/* Clipboard helpers */

typedef struct {
  GWeakRef screen_weak_ref;
} PasteData;

static void
clipboard_uris_received_cb (GObject *object,
                            GAsyncResult *result,
                            gpointer user_data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (object);
  PasteData *data = (PasteData *)user_data;
  gs_unref_object TerminalScreen *screen = nullptr;
  const GValue *value;

  if ((value = gdk_clipboard_read_value_finish (clipboard, result, nullptr)) &&
      G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST) &&
      (screen = (TerminalScreen*)g_weak_ref_get (&data->screen_weak_ref))) {
    const GList *uris = (const GList *)g_value_get_boxed (value);
    GString *string = g_string_new (NULL);

    for (const GList *iter = uris; iter; iter = iter->next) {
      GFile *file = G_FILE (iter->data);

      if (g_file_is_native (file)) {
        g_autofree char *quoted = g_shell_quote (g_file_peek_path (file));

        g_string_append (string, quoted);
        g_string_append_c (string, ' ');
      } else {
        g_autofree char *uri = g_file_get_uri (file);
        g_autofree char *quoted = g_shell_quote (uri);

        g_string_append (string, quoted);
        g_string_append_c (string, ' ');
      }
    }

    terminal_screen_paste_text (screen, string->str, string->len);
    g_string_free (string, TRUE);
  }

  g_weak_ref_clear (&data->screen_weak_ref);
  g_slice_free (PasteData, data);
}

static void
request_clipboard_contents_for_paste (TerminalWindow *window,
                                      gboolean paste_as_uris)
{
  GdkContentFormats *targets;

  if (window->active_screen == nullptr)
    return;

  targets = terminal_app_get_clipboard_targets (terminal_app_get (),
                                                window->clipboard);
  if (targets == nullptr)
    return;

  if (paste_as_uris && gdk_content_formats_contain_gtype (targets, GDK_TYPE_FILE_LIST)) {
    PasteData *data = g_slice_new (PasteData);
    g_weak_ref_init (&data->screen_weak_ref, window->active_screen);

    gdk_clipboard_read_value_async (window->clipboard,
                                    GDK_TYPE_FILE_LIST,
                                    G_PRIORITY_DEFAULT,
                                    nullptr,
                                    clipboard_uris_received_cb,
                                    data);
    return;
  } else if (gdk_content_formats_contain_gtype (targets, G_TYPE_STRING)) {
    vte_terminal_paste_clipboard (VTE_TERMINAL (window->active_screen));
  }
}

static void
action_paste_text_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  request_clipboard_contents_for_paste (window, FALSE);
}

static void
action_paste_uris_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  request_clipboard_contents_for_paste (window, TRUE);
}

static void
action_select_all_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (window->active_screen == nullptr)
    return;

  vte_terminal_select_all (VTE_TERMINAL (window->active_screen));
}

static void
action_reset_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  g_assert_nonnull (parameter);

  if (window->active_screen == nullptr)
    return;

  vte_terminal_reset (VTE_TERMINAL (window->active_screen),
                      TRUE,
                      g_variant_get_boolean (parameter));
}

static void
tab_switch_relative (TerminalWindow *window,
                     int change)
{
  int n_screens, value;

  n_screens = terminal_notebook_get_n_screens (window->notebook);
  value = terminal_notebook_get_active_screen_num (window->notebook) + change;

  gboolean keynav_wrap_around = true;
#if 0
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-keynav-wrap-around", &keynav_wrap_around,
                nullptr);
#endif
  if (keynav_wrap_around) {
    if (value < 0)
      value += n_screens;
    else if (value >= n_screens)
      value -= n_screens;
  }

  if (value < 0 || value >= n_screens)
    return;

  g_action_change_state (G_ACTION (lookup_action (window, "active-tab")),
                         g_variant_new_int32 (value));
}

static void
action_tab_switch_left_cb (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  tab_switch_relative (window, -1);
}

static void
action_tab_switch_right_cb (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  tab_switch_relative (window, 1);
}

static void
action_tab_move_left_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  int change;

  if (window->active_screen == nullptr)
    return;

  change = gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL ? 1 : -1;
  terminal_notebook_reorder_screen (window->notebook,
                                         terminal_notebook_get_active_screen (window->notebook),
                                         change);
}

static void
action_tab_move_right_cb (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  int change;

  if (window->active_screen == nullptr)
    return;

  change = gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL ? -1 : 1;
  terminal_notebook_reorder_screen (window->notebook,
                                         terminal_notebook_get_active_screen (window->notebook),
                                         change);
}

static void
action_zoom_in_cb (GSimpleAction *action,
                   GVariant *parameter,
                   gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  double zoom;

  if (window->active_screen == nullptr)
    return;

  zoom = vte_terminal_get_font_scale (VTE_TERMINAL (window->active_screen));
  if (!find_larger_zoom_factor (&zoom))
    return;

  gtk_window_set_default_size (GTK_WINDOW (window), -1, -1);
  vte_terminal_set_font_scale (VTE_TERMINAL (window->active_screen), zoom);
  terminal_window_update_zoom_sensitivity (window);
}

static void
action_zoom_out_cb (GSimpleAction *action,
                    GVariant *parameter,
                    gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  double zoom;

  if (window->active_screen == nullptr)
    return;

  zoom = vte_terminal_get_font_scale (VTE_TERMINAL (window->active_screen));
  if (!find_smaller_zoom_factor (&zoom))
    return;

  gtk_window_set_default_size (GTK_WINDOW (window), -1, -1);
  vte_terminal_set_font_scale (VTE_TERMINAL (window->active_screen), zoom);
  terminal_window_update_zoom_sensitivity (window);
}

static void
action_zoom_normal_cb (GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (window->active_screen == nullptr)
    return;

  vte_terminal_set_font_scale (VTE_TERMINAL (window->active_screen), PANGO_SCALE_MEDIUM);
  terminal_window_update_zoom_sensitivity (window);
}

static void
action_tab_detach_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalApp *app;
  TerminalWindow *new_window;
  TerminalScreen *screen;
  char geometry[32];
  int width, height;

  app = terminal_app_get ();

  screen = window->active_screen;

  terminal_screen_get_size (screen, &width, &height);
  g_snprintf (geometry, sizeof (geometry), "%dx%d", width, height);

  new_window = terminal_window_new (G_APPLICATION (app));

  terminal_window_move_screen (window, new_window, screen, -1);

  terminal_window_parse_geometry (new_window, geometry);

  gtk_window_present (GTK_WINDOW (new_window));
}

static void
action_help_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  terminal_util_show_help (nullptr);
}

static void
action_about_cb (GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
  terminal_util_show_about ();
}

static void
action_edit_preferences_cb (GSimpleAction *action,
                            GVariant *parameter,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  terminal_app_edit_preferences (terminal_app_get (),
                                 terminal_screen_get_profile (window->active_screen),
                                 nullptr,
                                 GDK_CURRENT_TIME);
}

static void
action_size_to_cb (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  guint width, height;

  g_assert_nonnull (parameter);

  if (window->active_screen == nullptr)
    return;

  g_variant_get (parameter, "(uu)", &width, &height);
  if (width < MIN_WIDTH_CHARS || height < MIN_HEIGHT_CHARS ||
      width > 256 || height > 256)
    return;

  vte_terminal_set_size (VTE_TERMINAL (window->active_screen), width, height);
  terminal_window_update_size (window);
}

static void
action_open_match_cb (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalScreenPopupInfo *info = window->popup_info;

  if (info == nullptr)
    return;
  if (info->url == nullptr)
    return;

  terminal_util_open_url (GTK_WIDGET (window), info->url, info->url_flavor,
                          GDK_CURRENT_TIME);
}

static void
action_copy_match_cb (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalScreenPopupInfo *info = window->popup_info;

  if (info == nullptr)
    return;
  if (info->url == nullptr)
    return;

  gdk_clipboard_set_text (window->clipboard, info->url);
}

static void
action_open_hyperlink_cb (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalScreenPopupInfo *info = window->popup_info;

  if (info == nullptr)
    return;
  if (info->hyperlink == nullptr)
    return;

  terminal_util_open_url (GTK_WIDGET (window), info->hyperlink, FLAVOR_AS_IS,
                          GDK_CURRENT_TIME);
}

static void
action_copy_hyperlink_cb (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalScreenPopupInfo *info = window->popup_info;

  if (info == nullptr)
    return;
  if (info->hyperlink == nullptr)
    return;

  gdk_clipboard_set_text (window->clipboard, info->hyperlink);
}

static void
action_toggle_fullscreen_cb (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  g_action_group_change_action_state (G_ACTION_GROUP (window), "fullscreen",
                                      g_variant_new_boolean (!gtk_window_is_fullscreen (GTK_WINDOW (window))));
}

static void
action_inspector_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
search_popover_search_cb (TerminalSearchPopover *popover,
                          gboolean backward,
                          TerminalWindow *window)
{
  if (G_UNLIKELY (window->active_screen == nullptr))
    return;

  if (backward)
    vte_terminal_search_find_previous (VTE_TERMINAL (window->active_screen));
  else
    vte_terminal_search_find_next (VTE_TERMINAL (window->active_screen));
}

static void
search_popover_notify_regex_cb (TerminalSearchPopover *popover,
                                GParamSpec *pspec G_GNUC_UNUSED,
                                TerminalWindow *window)
{
  VteRegex *regex;

  if (G_UNLIKELY (window->active_screen == nullptr))
    return;

  regex = terminal_search_popover_get_regex (popover);
  vte_terminal_search_set_regex (VTE_TERMINAL (window->active_screen), regex, 0);

  terminal_window_update_search_sensitivity (window->active_screen, window);
}

static void
search_popover_notify_wrap_around_cb (TerminalSearchPopover *popover,
                                      GParamSpec *pspec G_GNUC_UNUSED,
                                      TerminalWindow *window)
{
  gboolean wrap;

  if (G_UNLIKELY (window->active_screen == nullptr))
    return;

  wrap = terminal_search_popover_get_wrap_around (popover);
  vte_terminal_search_set_wrap_around (VTE_TERMINAL (window->active_screen), wrap);
}

static void
action_find_cb (GSimpleAction *action,
                GVariant *parameter,
                gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (G_UNLIKELY(window->active_screen == nullptr))
    return;

  if (window->search_popover != nullptr) {
    search_popover_notify_regex_cb (window->search_popover, nullptr, window);
    search_popover_notify_wrap_around_cb (window->search_popover, nullptr, window);

    gtk_window_present (GTK_WINDOW (window->search_popover));
    gtk_widget_grab_focus (GTK_WIDGET (window->search_popover));
    return;
  }

  if (window->active_screen == nullptr)
    return;

  window->search_popover = terminal_search_popover_new (GTK_WIDGET (window));

  g_signal_connect (window->search_popover, "search", G_CALLBACK (search_popover_search_cb), window);

  search_popover_notify_regex_cb (window->search_popover, nullptr, window);
  g_signal_connect (window->search_popover, "notify::regex", G_CALLBACK (search_popover_notify_regex_cb), window);

  search_popover_notify_wrap_around_cb (window->search_popover, nullptr, window);
  g_signal_connect (window->search_popover, "notify::wrap-around", G_CALLBACK (search_popover_notify_wrap_around_cb), window);

  g_signal_connect_swapped (window->search_popover, "destroy", G_CALLBACK (g_nullify_pointer), &window->search_popover);

  gtk_window_present (GTK_WINDOW (window->search_popover));
  gtk_widget_grab_focus (GTK_WIDGET (window->search_popover));
}

static void
action_find_forward_cb (GSimpleAction *action,
                        GVariant *parameter,
                        gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (window->active_screen == nullptr)
    return;

  vte_terminal_search_find_next (VTE_TERMINAL (window->active_screen));
}

static void
action_find_backward_cb (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (window->active_screen == nullptr)
    return;

  vte_terminal_search_find_previous (VTE_TERMINAL (window->active_screen));
}

static void
action_find_clear_cb (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (window->active_screen == nullptr)
    return;

  vte_terminal_search_set_regex (VTE_TERMINAL (window->active_screen), nullptr, 0);
  vte_terminal_unselect_all (VTE_TERMINAL (window->active_screen));
}

static void
action_shadow_activate_cb (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  gs_free char *param = g_variant_print(parameter, TRUE);

  _terminal_debug_print (TERMINAL_DEBUG_ACCELS,
                         "Window %p shadow action activated for %s\n",
                         window, param);

  /* We make sure in terminal-accels to always install the keybinding
   * for the real action first, so that it's first in line for activation.
   * That means we can make this here a NOP, instead of forwarding the
   * activation to the shadowed action.
   */
}

static void
action_fullscreen_state_cb (GSimpleAction *action,
                            GVariant *state,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (g_variant_get_boolean (state))
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));

  /* The window-state-changed callback will update the action's actual state */
}

static void
action_read_only_state_cb (GSimpleAction *action,
                           GVariant *state,
                           gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  g_assert_nonnull (state);

  g_simple_action_set_state (action, state);

  terminal_window_update_paste_sensitivity (window);

  if (window->active_screen == nullptr)
    return;

  vte_terminal_set_input_enabled (VTE_TERMINAL (window->active_screen),
                                  !g_variant_get_boolean (state));
}

static void
action_profile_state_cb (GSimpleAction *action,
                         GVariant *state,
                         gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  TerminalSettingsList *profiles_list;
  const gchar *uuid;
  gs_unref_object GSettings *profile;

  g_assert_nonnull (state);

  uuid = g_variant_get_string (state, nullptr);
  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  profile = terminal_settings_list_ref_child (profiles_list, uuid);
  if (profile == nullptr)
    return;

  g_simple_action_set_state (action, state);

  terminal_screen_set_profile (window->active_screen, profile);
}

static void
action_active_tab_set_cb (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;
  int value, n_screens;

  g_assert_nonnull (parameter);

  n_screens = terminal_notebook_get_n_screens (window->notebook);

  value = g_variant_get_int32 (parameter);
  if (value < 0)
    value += n_screens;
  if (value < 0 || value >= n_screens)
    return;

  g_action_change_state (G_ACTION (action), g_variant_new_int32 (value));
}

static void
action_active_tab_state_cb (GSimpleAction *action,
                            GVariant *state,
                            gpointer user_data)
{
  TerminalWindow *window = (TerminalWindow*)user_data;

  g_assert_nonnull (state);

  g_simple_action_set_state (action, state);

  terminal_notebook_set_active_screen_num (window->notebook, g_variant_get_int32 (state));
}

/* utility functions */

static void
terminal_window_update_set_profile_menu_active_profile (TerminalWindow *window)
{
  GSettings *new_active_profile;
  TerminalSettingsList *profiles_list;
  char *uuid;

  if (window->active_screen == nullptr)
    return;

  new_active_profile = terminal_screen_get_profile (window->active_screen);

  profiles_list = terminal_app_get_profiles_list (terminal_app_get ());
  uuid = terminal_settings_list_dup_uuid_from_child (profiles_list, new_active_profile);

  g_simple_action_set_state (lookup_action (window, "profile"),
                             g_variant_new_take_string (uuid));
}

static void
terminal_window_update_terminal_menu (TerminalWindow *window)
{
  if (window->active_screen == nullptr)
    return;

  gboolean read_only = !vte_terminal_get_input_enabled (VTE_TERMINAL (window->active_screen));
  g_simple_action_set_state (lookup_action (window, "read-only"),
                             g_variant_new_boolean (read_only));
}

/* Actions stuff */

static void
terminal_window_update_copy_sensitivity (TerminalScreen *screen,
                                         TerminalWindow *window)
{
  gboolean can_copy;

  if (screen != window->active_screen)
    return;

  can_copy = vte_terminal_get_has_selection (VTE_TERMINAL (screen));
  g_simple_action_set_enabled (lookup_action (window, "copy"), can_copy);
}

static void
terminal_window_update_zoom_sensitivity (TerminalWindow *window)
{
  TerminalScreen *screen;

  screen = window->active_screen;
  if (screen == nullptr)
    return;

  double v;
  double zoom = v = vte_terminal_get_font_scale (VTE_TERMINAL (screen));
  g_simple_action_set_enabled (lookup_action (window, "zoom-in"),
                               find_larger_zoom_factor (&v));

  v = zoom;
  g_simple_action_set_enabled (lookup_action (window, "zoom-out"),
                               find_smaller_zoom_factor (&v));
}

static void
terminal_window_update_search_sensitivity (TerminalScreen *screen,
                                           TerminalWindow *window)
{
  if (screen != window->active_screen)
    return;

  gboolean can_search = vte_terminal_search_get_regex (VTE_TERMINAL (screen)) != nullptr;

  g_simple_action_set_enabled (lookup_action (window, "find-forward"), can_search);
  g_simple_action_set_enabled (lookup_action (window, "find-backward"), can_search);
  g_simple_action_set_enabled (lookup_action (window, "find-clear"), can_search);
}

static void
clipboard_targets_changed_cb (TerminalApp *app,
                              GdkClipboard *clipboard,
                              TerminalWindow *window)
{
  if (clipboard != window->clipboard)
    return;

  terminal_window_update_paste_sensitivity (window);
}

static void
terminal_window_update_paste_sensitivity (TerminalWindow *window)
{
#ifdef GTK4_TODO
  GdkAtom *targets;
  int n_targets;
  targets = terminal_app_get_clipboard_targets (terminal_app_get(), window->clipboard, &n_targets);

  gboolean can_paste;
  gboolean can_paste_uris;
  if (n_targets) {
    can_paste = gtk_targets_include_text (targets, n_targets);
    can_paste_uris = gtk_targets_include_uri (targets, n_targets);
  } else {
    can_paste = can_paste_uris = FALSE;
  }

  gs_unref_variant GVariant *ro_state = g_action_get_state (g_action_map_lookup_action (G_ACTION_MAP (window), "read-only"));
  gboolean read_only = g_variant_get_boolean (ro_state);

  g_simple_action_set_enabled (lookup_action (window, "paste-text"), can_paste && !read_only);
  g_simple_action_set_enabled (lookup_action (window, "paste-uris"), can_paste_uris && !read_only);
#endif
}

static void
screen_resize_window_cb (TerminalScreen *screen,
                         guint columns,
                         guint rows,
                         TerminalWindow* window)
{
  if (window->realized &&
      window_state_is_snapped(window->window_state))
    return;

  vte_terminal_set_size (VTE_TERMINAL (window->active_screen), columns, rows);

  if (screen == window->active_screen)
    terminal_window_update_size (window);
}

static void
terminal_window_update_tabs_actions_sensitivity (TerminalWindow *window)
{
  g_assert (TERMINAL_IS_WINDOW (window));
  g_assert (TERMINAL_IS_NOTEBOOK (window->notebook));

  if (window->disposed)
    return;

  int num_pages = terminal_notebook_get_n_screens (window->notebook);
  int page_num = terminal_notebook_get_active_screen_num (window->notebook);

  gboolean not_only = num_pages > 1;
  gboolean not_first = page_num > 0;
  gboolean not_last = page_num + 1 < num_pages;

  gboolean not_first_lr, not_last_lr;
  if (gtk_widget_get_direction (GTK_WIDGET (window)) == GTK_TEXT_DIR_RTL) {
    not_first_lr = not_last;
    not_last_lr = not_first;
  } else {
    not_first_lr = not_first;
    not_last_lr = not_last;
  }

  /* Hide the tabs menu in single-tab windows */
  g_simple_action_set_enabled (lookup_action (window, "tabs-menu"), not_only);

  /* Disable shadowing of MDI actions in SDI windows */
  g_simple_action_set_enabled (lookup_action (window, "shadow-mdi"), not_only);

  /* Disable tab switching (and all its shortcuts) in SDI windows */
  g_simple_action_set_enabled (lookup_action (window, "active-tab"), not_only);

  /* Set the active tab */
  g_simple_action_set_state (lookup_action (window, "active-tab"),
                             g_variant_new_int32 (page_num));

  /* Keynav wraps around? See bug #92139 */
  gboolean keynav_wrap_around = TRUE;
#ifdef GTK4_TODO
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (window)),
                "gtk-keynav-wrap-around", &keynav_wrap_around,
                nullptr);
#endif

  gboolean wrap = keynav_wrap_around && not_only;
  g_simple_action_set_enabled (lookup_action (window, "tab-switch-left"), not_first || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-switch-right"), not_last || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-move-left"), not_first_lr || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-move-right"), not_last_lr || wrap);
  g_simple_action_set_enabled (lookup_action (window, "tab-detach"), not_only);
}

static AdwTabView *
handle_tab_dropped_on_desktop (AdwTabView     *tab_view,
                               TerminalWindow *window)
{
  TerminalWindow *source_window;
  TerminalWindow *new_window;

  g_assert (ADW_IS_TAB_VIEW (tab_view));
  g_assert (TERMINAL_IS_WINDOW (window));

  source_window = TERMINAL_WINDOW (gtk_widget_get_root (GTK_WIDGET (tab_view)));
  g_return_val_if_fail (TERMINAL_IS_WINDOW (source_window), nullptr);

  new_window = terminal_window_new (G_APPLICATION (terminal_app_get ()));
  new_window->present_on_insert = TRUE;

  return terminal_notebook_get_tab_view (new_window->notebook);
}

/* Terminal screen popup menu handling */

static void
remove_popup_info (TerminalWindow *window)
{
  if (window->popup_info != nullptr)
    {
      terminal_screen_popup_info_unref (window->popup_info);
      window->popup_info = nullptr;
    }
}

static void
screen_show_popup_menu_cb (TerminalScreen *screen,
                           TerminalScreenPopupInfo *info,
                           TerminalWindow *window)
{
  TerminalApp *app = terminal_app_get ();

  if (screen != window->active_screen)
    return;

  remove_popup_info (window);
  window->popup_info = terminal_screen_popup_info_ref (info);

  gs_unref_object GMenu *menu = g_menu_new ();

  /* Hyperlink section */
  if (info->hyperlink != nullptr) {
    gs_unref_object GMenu *section1 = g_menu_new ();

    g_menu_append (section1, _("Open _Hyperlink"), "win.open-hyperlink");
    g_menu_append (section1, _("Copy Hyperlink _Address"), "win.copy-hyperlink");
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section1));
  }
  /* Matched link section */
  else if (info->url != nullptr) {
    gs_unref_object GMenu *section2 = g_menu_new ();

    const char *open_label = nullptr, *copy_label = nullptr;
    switch (info->url_flavor) {
    case FLAVOR_EMAIL:
      open_label = _("Send Mail _To…");
      copy_label = _("Copy Mail _Address");
      break;
    case FLAVOR_VOIP_CALL:
      open_label = _("Call _To…");
      copy_label = _("Copy Call _Address ");
      break;
    case FLAVOR_AS_IS:
    case FLAVOR_DEFAULT_TO_HTTP:
    default:
      open_label = _("_Open Link");
      copy_label = _("Copy _Link");
      break;
    }

    g_menu_append (section2, open_label, "win.open-match");
    g_menu_append (section2, copy_label, "win.copy-match");
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section2));
  }

  /* Info section */
  gs_strfreev char** citems = g_settings_get_strv (terminal_app_get_global_settings (terminal_app_get ()),
                                                   TERMINAL_SETTING_CONTEXT_INFO_KEY);

  gs_unref_object GMenu *section3 = g_menu_new ();

  for (int i = 0; citems[i] != nullptr; ++i) {
    const char *citem = citems[i];

    if (g_str_equal (citem, "numbers") &&
        info->number_info != nullptr) {
      /* Non-existent action will make this item insensitive */
      gs_unref_object GMenuItem *item3 = g_menu_item_new (info->number_info, "win.notexist");
      g_menu_append_item (section3, item3);
    }

    if (g_str_equal (citem, "timestamps") &&
        info->timestamp_info != nullptr) {
      /* Non-existent action will make this item insensitive */
      gs_unref_object GMenuItem *item3 = g_menu_item_new (info->timestamp_info, "win.notexist");
      g_menu_append_item (section3, item3);
    }
  }

  if (g_menu_model_get_n_items(G_MENU_MODEL (section3)) > 0)
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (section3));

  /* Clipboard section */
  gs_unref_object GMenu *section4 = g_menu_new ();

  g_menu_append (section4, _("_Copy"), "win.copy::text");
  g_menu_append (section4, _("Copy as _HTML"), "win.copy::html");
  g_menu_append (section4, _("_Paste"), "win.paste-text");
  if (g_action_get_enabled (G_ACTION (lookup_action (window, "paste-uris"))))
    g_menu_append (section4, _("Paste as _Filenames"), "win.paste-uris");

  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section4));

  /* Profile and property section */
  gs_unref_object GMenu *section5 = g_menu_new ();
  g_menu_append (section5, _("Read-_Only"), "win.read-only");

  GMenuModel *profiles_menu = terminal_app_get_profile_section (app);
  if (profiles_menu != nullptr && g_menu_model_get_n_items (profiles_menu) > 1) {
    gs_unref_object GMenu *submenu5 = g_menu_new ();
    g_menu_append_section (submenu5, nullptr, profiles_menu);

    gs_unref_object GMenuItem *item5 = g_menu_item_new (_("P_rofiles"), nullptr);
    g_menu_item_set_submenu (item5, G_MENU_MODEL (submenu5));
    g_menu_append_item (section5, item5);
  }

  g_menu_append (section5, _("_Preferences"), "win.edit-preferences");

  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section5));

  /* New Terminal section */
  gs_unref_object GMenu *section6 = g_menu_new ();
  if (terminal_app_get_menu_unified (app)) {
    gs_unref_object GMenuItem *item6 = g_menu_item_new (_("New _Terminal"), nullptr);
    g_menu_item_set_action_and_target (item6, "win.new-terminal",
                                       "(ss)", "default", "current");
    g_menu_append_item (section6, item6);
  } else {
    gs_unref_object GMenuItem *item61 = g_menu_item_new (_("New _Window"), nullptr);
    g_menu_item_set_action_and_target (item61, "win.new-terminal",
                                       "(ss)", "window", "current");
    g_menu_append_item (section6, item61);
    gs_unref_object GMenuItem *item62 = g_menu_item_new (_("New _Tab"), nullptr);
    g_menu_item_set_action_and_target (item62, "win.new-terminal",
                                       "(ss)", "tab", "current");
    g_menu_append_item (section6, item62);
  }
  g_menu_append_section (menu, nullptr, G_MENU_MODEL (section6));

  if (window->context_menu == nullptr) {
    window->context_menu = GTK_POPOVER_MENU (gtk_popover_menu_new_from_model (nullptr));
    gtk_popover_set_has_arrow (GTK_POPOVER (window->context_menu), FALSE);
    gtk_popover_set_position (GTK_POPOVER (window->context_menu), GTK_POS_BOTTOM);
    gtk_widget_set_halign (GTK_WIDGET (window->context_menu), GTK_ALIGN_START);
    gtk_widget_set_parent (GTK_WIDGET (window->context_menu), GTK_WIDGET (window));
  }

  graphene_point_t point = {float(info->x), float(info->y)};
  if (gtk_widget_compute_point (GTK_WIDGET (screen), GTK_WIDGET (window), &point, &point)) {
    cairo_rectangle_int_t rect = {int(point.x), int(point.y), 1, 1};
    gtk_popover_menu_set_menu_model (window->context_menu, G_MENU_MODEL (menu));
    gtk_popover_set_pointing_to (GTK_POPOVER (window->context_menu), &rect);
    gtk_popover_popup (GTK_POPOVER (window->context_menu));
  }
}

static gboolean
screen_match_clicked_cb (TerminalScreen *screen,
                         const char *url,
                         int url_flavor,
                         guint state,
                         TerminalWindow *window)
{
  if (screen != window->active_screen)
    return FALSE;

  gtk_widget_grab_focus (GTK_WIDGET (screen));
  terminal_util_open_url (GTK_WIDGET (window), url, TerminalURLFlavor(url_flavor),
                          GDK_CURRENT_TIME);

  return TRUE;
}

static void
screen_close_cb (TerminalScreen *screen,
                 TerminalWindow *window)
{
  terminal_window_remove_screen (window, screen);
}

static void
notebook_update_tabs_menu_cb (GtkMenuButton *button,
                              gpointer user_data)
{
  TerminalWindow *window = TERMINAL_WINDOW (user_data);
  gs_unref_object GMenu *menu;
  gs_free_list GList *tabs;
  GList *t;
  int i;

  g_assert (GTK_IS_MENU_BUTTON (button));
  g_assert (TERMINAL_IS_WINDOW (window));

  menu = g_menu_new ();
  tabs = terminal_window_list_screen_containers (window);

  for (t = tabs, i = 0; t != nullptr; t = t->next, i++) {
    TerminalScreenContainer *container = (TerminalScreenContainer*)t->data;
    TerminalScreen *screen = terminal_screen_container_get_screen (container);
    gs_unref_object GMenuItem *item;
    const char *title;

    if (t->next == nullptr) {
      /* Last entry. If it has no dedicated shortcut "Switch to Tab N",
       * display the accel of "Switch to Last Tab". */
      GtkApplication *app = GTK_APPLICATION (g_application_get_default ());
      gs_free gchar *detailed_action = g_strdup_printf("win.active-tab(%d)", i);
      gs_strfreev gchar **accels = gtk_application_get_accels_for_action (app, detailed_action);
      if (accels[0] == nullptr)
        i = -1;
    }

    title = terminal_screen_get_title (screen);

    item = g_menu_item_new (title && title[0] ? title : _("Terminal"), nullptr);
    g_menu_item_set_action_and_target (item, "win.active-tab", "i", i);
    g_menu_append_item (menu, item);
  }

  gtk_menu_button_set_menu_model (button, G_MENU_MODEL (menu));
}

static void
terminal_window_fill_notebook_action_box (TerminalWindow *window,
                                          gboolean add_new_tab_button)
{
  GtkWidget *box, *new_tab_button, *tabs_menu_button;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  adw_tab_bar_set_end_action_widget (window->tab_bar, box);

  /* Create the NewTerminal button */
  if (add_new_tab_button)
    {
      new_tab_button = terminal_icon_button_new ("tab-new-symbolic");
      gtk_actionable_set_action_name (GTK_ACTIONABLE (new_tab_button), "win.new-terminal");
      gtk_actionable_set_action_target (GTK_ACTIONABLE (new_tab_button), "(ss)", "tab", "current");
      gtk_box_prepend (GTK_BOX (box), new_tab_button);
      gtk_widget_show (new_tab_button);
    }

  /* Create Tabs menu button */
  tabs_menu_button = gtk_menu_button_new ();
  gtk_widget_add_css_class (tabs_menu_button, "flat");
  gtk_menu_button_set_create_popup_func (GTK_MENU_BUTTON (tabs_menu_button),
                                         notebook_update_tabs_menu_cb,
                                         window, nullptr);
  gtk_box_prepend (GTK_BOX (box), tabs_menu_button);
}

static void
window_hide_ask_default_terminal(TerminalWindow* window)
{
  if (!window->ask_default_infobar ||
      !gtk_widget_get_visible(window->ask_default_infobar))
    return;

  gtk_widget_hide(window->ask_default_infobar);
  terminal_window_update_size(window);
}

static void
window_sync_ask_default_terminal_cb(TerminalApp* app,
                                    GParamSpec* pspect,
                                    TerminalWindow* window)
{
  window_hide_ask_default_terminal(window);
}

static void
default_infobar_response_cb(GtkInfoBar* infobar,
                            int response,
                            TerminalWindow* window)
{
  auto const app = terminal_app_get();

  if (response == GTK_RESPONSE_YES) {
    terminal_app_make_default_terminal(app);
    terminal_app_unset_ask_default_terminal(app);
  } else if (response == GTK_RESPONSE_NO) {
    terminal_app_unset_ask_default_terminal(app);
  } else { // GTK_RESPONSE_CLOSE
    window_hide_ask_default_terminal(window);
  }
}

/*****************************************/

static void
terminal_window_realize (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  GtkAllocation widget_allocation;
  GdkSurface *surface;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] realize, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->realize (widget);

  surface = gtk_native_get_surface (GTK_NATIVE (window));

  g_signal_connect_object (surface,
                           "notify::state",
                           G_CALLBACK (terminal_window_state_event),
                           window,
                           G_CONNECT_SWAPPED);

  /* Now that we've been realized, we should know precisely how large the
   * client-side decorations are going to be. Recalculate the geometry hints,
   * export them to the windowing system, and resize the window accordingly. */
  window->realized = TRUE;
  terminal_window_update_size (window);
}

static void
terminal_window_state_event (GtkWidget *widget,
                             GParamSpec *pspec,
                             GdkToplevel *surface)
{
  auto const window = TERMINAL_WINDOW(widget);
  auto window_state = gdk_toplevel_get_state (surface);
  auto changed_mask = GdkToplevelState(unsigned(window->window_state) ^ unsigned(window_state));

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "Window state changed mask %x old state %x new state %x\n",
                         unsigned(changed_mask),
                         unsigned(window->window_state),
                         unsigned(window_state));

  window->window_state = window_state;

  if (changed_mask & GDK_TOPLEVEL_STATE_FULLSCREEN)
    {
      auto const is_fullscreen = (window->window_state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;

      g_simple_action_set_state (lookup_action (window, "fullscreen"),
                                 g_variant_new_boolean (is_fullscreen));
      g_simple_action_set_enabled (lookup_action (window, "size-to"),
                                   !is_fullscreen);
    }

  if (window_state_is_snapped(changed_mask))
    {
      if (window_state_is_snapped(window_state))
        {
          _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                                 "Disapplying geometry hints entering snapped state\n");
#ifdef GTK4_TODO
          gtk_window_set_geometry_hints(GTK_WINDOW(widget), nullptr, nullptr,
                                        GdkWindowHints(0));
#endif
        }
      else
        {
          _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                                 "Reapplying geometry hints after leaving snapped state\n");
#ifdef GTK4_TODO
          gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                         nullptr,
                                         &window->hints,
                                         GdkWindowHints(GDK_HINT_RESIZE_INC |
                                                        GDK_HINT_MIN_SIZE |
                                                        GDK_HINT_BASE_SIZE));
#endif
        }
    }
}

static void
terminal_window_init (TerminalWindow *window)
{
  const GActionEntry action_entries[] = {
    /* Actions without state */
    { "about",               action_about_cb,            nullptr,   nullptr, nullptr },
    { "close",               action_close_cb,            "s",    nullptr, nullptr },
    { "copy",                action_copy_cb,             "s",    nullptr, nullptr },
    { "copy-hyperlink",      action_copy_hyperlink_cb,   nullptr,   nullptr, nullptr },
    { "copy-match",          action_copy_match_cb,       nullptr,   nullptr, nullptr },
    { "edit-preferences",    action_edit_preferences_cb, nullptr,   nullptr, nullptr },
    { "toggle-fullscreen",   action_toggle_fullscreen_cb,nullptr,   nullptr, nullptr },
    { "find",                action_find_cb,             nullptr,   nullptr, nullptr },
    { "find-backward",       action_find_backward_cb,    nullptr,   nullptr, nullptr },
    { "find-clear",          action_find_clear_cb,       nullptr,   nullptr, nullptr },
    { "find-forward",        action_find_forward_cb,     nullptr,   nullptr, nullptr },
    { "help",                action_help_cb,             nullptr,   nullptr, nullptr },
    { "inspector",           action_inspector_cb,        nullptr,   nullptr, nullptr },
    { "new-terminal",        action_new_terminal_cb,     "(ss)", nullptr, nullptr },
    { "open-match",          action_open_match_cb,       nullptr,   nullptr, nullptr },
    { "open-hyperlink",      action_open_hyperlink_cb,   nullptr,   nullptr, nullptr },
    { "paste-text",          action_paste_text_cb,       nullptr,   nullptr, nullptr },
    { "paste-uris",          action_paste_uris_cb,       nullptr,   nullptr, nullptr },
    { "reset",               action_reset_cb,            "b",    nullptr, nullptr },
    { "select-all",          action_select_all_cb,       nullptr,   nullptr, nullptr },
    { "size-to",             action_size_to_cb,          "(uu)", nullptr, nullptr },
    { "tab-detach",          action_tab_detach_cb,       nullptr,   nullptr, nullptr },
    { "tab-move-left",       action_tab_move_left_cb,    nullptr,   nullptr, nullptr },
    { "tab-move-right",      action_tab_move_right_cb,   nullptr,   nullptr, nullptr },
    { "tab-switch-left",     action_tab_switch_left_cb,  nullptr,   nullptr, nullptr },
    { "tab-switch-right",    action_tab_switch_right_cb, nullptr,   nullptr, nullptr },
    { "tabs-menu",           nullptr,                       nullptr,   nullptr, nullptr },
    { "zoom-in",             action_zoom_in_cb,          nullptr,   nullptr, nullptr },
    { "zoom-normal",         action_zoom_normal_cb,      nullptr,   nullptr, nullptr },
    { "zoom-out",            action_zoom_out_cb,         nullptr,   nullptr, nullptr },
#ifdef ENABLE_EXPORT
    { "export",              action_export_cb,           nullptr,   nullptr, nullptr },
#endif
#ifdef ENABLE_PRINT
    { "print",               action_print_cb,            nullptr,   nullptr, nullptr },
#endif
#ifdef ENABLE_SAVE
    { "save-contents",       action_save_contents_cb,    nullptr,   nullptr, nullptr },
#endif

    /* Shadow actions for keybinding comsumption, see comment in terminal-accels.c */
    { "shadow",              action_shadow_activate_cb,  "s",    nullptr, nullptr },
    { "shadow-mdi",          action_shadow_activate_cb,  "s",    nullptr, nullptr },

    /* Actions with state */
    { "active-tab",          action_active_tab_set_cb,   "i",  "@i 0",    action_active_tab_state_cb      },
    { "header-menu",         nullptr /* toggles state */,   nullptr, "false",   nullptr },
    { "fullscreen",          nullptr /* toggles state */,   nullptr, "false",   action_fullscreen_state_cb      },
    { "profile",             nullptr /* changes state */,   "s",  "''",      action_profile_state_cb         },
    { "read-only",           nullptr /* toggles state */,   nullptr, "false",   action_read_only_state_cb       },
  };
  TerminalApp *app;
  GSettings *gtk_debug_settings;
  GtkWindowGroup *window_group;
  //  GtkAccelGroup *accel_group;
  uuid_t u;
  char uuidstr[37];
  GSimpleAction *action;

  app = terminal_app_get ();

  window->active_screen = nullptr;

  window->old_char_width = -1;
  window->old_char_height = -1;

  window->old_chrome_width = -1;
  window->old_chrome_height = -1;
  window->old_csd_width = -1;
  window->old_csd_height = -1;
  window->old_padding_width = -1;
  window->old_padding_height = -1;

  window->old_geometry_widget = nullptr;

  uuid_generate (u);
  uuid_unparse (u, uuidstr);
  window->uuid = g_strdup (uuidstr);

  gtk_widget_init_template (GTK_WIDGET (window));

  /* GAction setup */
  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   action_entries, G_N_ELEMENTS (action_entries),
                                   window);

  /* Add "Set as default terminal" infobar */
  if (terminal_app_get_ask_default_terminal(app)) {
    auto const infobar = window->ask_default_infobar = gtk_info_bar_new();
    gtk_info_bar_set_show_close_button(GTK_INFO_BAR(infobar), true);
    gtk_info_bar_set_message_type(GTK_INFO_BAR(infobar), GTK_MESSAGE_QUESTION);

    auto const question = gtk_label_new (_("Set GNOME Terminal as your default terminal?"));
    gtk_label_set_wrap(GTK_LABEL(question), true);
    gtk_info_bar_add_child(GTK_INFO_BAR(infobar), question);
    gtk_widget_show(question);

    gtk_info_bar_add_button(GTK_INFO_BAR(infobar), _("_Yes"), GTK_RESPONSE_YES);
    gtk_info_bar_add_button(GTK_INFO_BAR(infobar), _("_No"), GTK_RESPONSE_NO);

    g_signal_connect (infobar, "response",
                      G_CALLBACK(default_infobar_response_cb), window);

    gtk_box_prepend(GTK_BOX(window->main_vbox), infobar);

    gtk_widget_show(infobar);
    g_signal_connect(app, "notify::ask-default-terminal",
                     G_CALLBACK(window_sync_ask_default_terminal_cb), window);
  }

  /* Maybe make Inspector available */
  action = lookup_action (window, "inspector");
  gtk_debug_settings = terminal_app_get_gtk_debug_settings (app);
  if (gtk_debug_settings != nullptr)
    g_settings_bind (gtk_debug_settings,
                     "enable-inspector-keybinding",
                     action,
                     "enabled",
                     GSettingsBindFlags(G_SETTINGS_BIND_GET |
					G_SETTINGS_BIND_NO_SENSITIVITY));
  else
    g_simple_action_set_enabled (action, FALSE);

  window->clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window));
  clipboard_targets_changed_cb (app, window->clipboard, window);
  g_signal_connect (app, "clipboard-targets-changed",
                    G_CALLBACK (clipboard_targets_changed_cb), window);

  terminal_window_fill_notebook_action_box (window, FALSE);

  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

#ifdef GDK_WINDOWING_X11
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
  if (GDK_IS_X11_SURFACE (surface)) {
    char role[64];
    g_snprintf (role, sizeof (role), "gnome-terminal-window-%s", uuidstr);
    gdk_x11_surface_set_utf8_property (surface, "WM_WINDOW_ROLE", role);
  }
#endif
}

static void
terminal_window_css_changed (GtkWidget *widget,
                             GtkCssStyleChange *change)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->css_changed (widget, change);

  terminal_window_update_size (window);
}

static gboolean
policy_type_to_autohide (GValue   *value,
                         GVariant *variant,
                         gpointer  user_data)
{
  auto policy_type = g_variant_get_string (variant, nullptr);
  gboolean autohide = g_strcmp0 (policy_type, "automatic") == 0;
  g_value_set_boolean (value, autohide);
  return TRUE;
}

static gboolean
policy_type_to_visible (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  auto policy_type = g_variant_get_string (variant, nullptr);
  gboolean visible = g_strcmp0 (policy_type, "never") != 0;
  g_value_set_boolean (value, visible);
  return TRUE;
}

static void
terminal_window_size_allocate (GtkWidget *widget,
                               int width,
                               int height,
                               int baseline)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);

  GTK_WIDGET_CLASS (terminal_window_parent_class)->size_allocate (widget, width, height, baseline);

  if (window->context_menu != nullptr) {
    gtk_popover_present (GTK_POPOVER (window->context_menu));
  }

}

static void
terminal_window_constructed (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  GSettings *settings;

  G_OBJECT_CLASS (terminal_window_parent_class)->constructed (object);

  settings = terminal_app_get_global_settings (terminal_app_get ());

  g_settings_bind_with_mapping (settings,
                                TERMINAL_SETTING_TAB_POLICY_KEY,
                                window->tab_bar,
                                "autohide",
                                GSettingsBindFlags(G_SETTINGS_BIND_GET |
                                                   G_SETTINGS_BIND_NO_SENSITIVITY),
                                policy_type_to_autohide, nullptr, nullptr, nullptr);
  g_settings_bind_with_mapping (settings,
                                TERMINAL_SETTING_TAB_POLICY_KEY,
                                window->tab_bar,
                                "visible",
                                GSettingsBindFlags(G_SETTINGS_BIND_GET |
                                                   G_SETTINGS_BIND_NO_SENSITIVITY),
                                policy_type_to_visible, nullptr, nullptr, nullptr);
}

static void
terminal_window_class_init (TerminalWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = terminal_window_constructed;
  object_class->dispose = terminal_window_dispose;
  object_class->finalize = terminal_window_finalize;

  widget_class->show = terminal_window_show;
  widget_class->realize = terminal_window_realize;
  widget_class->css_changed = terminal_window_css_changed;
  widget_class->size_allocate = terminal_window_size_allocate;

  window_class->close_request = terminal_window_close_request;

#ifdef GTK4_TODO
  GtkWindowClass *window_klass;
  GtkBindingSet *binding_set;

  window_klass = (GtkWindowClass*)g_type_class_ref (GTK_TYPE_WINDOW);

  binding_set = gtk_binding_set_by_class (window_klass);
  gtk_binding_entry_skip (binding_set, GDK_KEY_I, GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK));
  gtk_binding_entry_skip (binding_set, GDK_KEY_D, GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK));
  g_type_class_unref (window_klass);
#endif

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/window.ui");

  gtk_widget_class_bind_template_callback (widget_class, screen_close_request_cb);
  gtk_widget_class_bind_template_callback (widget_class, notebook_screen_switched_cb);
  gtk_widget_class_bind_template_callback (widget_class, notebook_screen_added_cb);
  gtk_widget_class_bind_template_callback (widget_class, notebook_screen_removed_cb);
  gtk_widget_class_bind_template_callback (widget_class, notebook_screens_reordered_cb);
  gtk_widget_class_bind_template_callback (widget_class, terminal_window_update_geometry);
  gtk_widget_class_bind_template_callback (widget_class, handle_tab_dropped_on_desktop);

  gtk_widget_class_bind_template_child (widget_class, TerminalWindow, headerbar);
  gtk_widget_class_bind_template_child (widget_class, TerminalWindow, main_vbox);
  gtk_widget_class_bind_template_child (widget_class, TerminalWindow, notebook);
  gtk_widget_class_bind_template_child (widget_class, TerminalWindow, tab_bar);

  g_type_ensure (TERMINAL_TYPE_HEADERBAR);
  g_type_ensure (TERMINAL_TYPE_NOTEBOOK);
}

static void
terminal_window_dispose (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);
  TerminalApp *app = terminal_app_get ();

  window->disposed = TRUE;

  gtk_widget_dispose_template (GTK_WIDGET (window), TERMINAL_TYPE_WINDOW);

  g_clear_pointer ((GtkWidget **)&window->context_menu, gtk_widget_unparent);

  if (window->clipboard != nullptr) {
    g_signal_handlers_disconnect_by_func (app,
                                          (void*)clipboard_targets_changed_cb,
                                          window);
    window->clipboard = nullptr;
  }

  if (window->ask_default_infobar) {
    g_signal_handlers_disconnect_by_func(app,
                                         (void*)window_sync_ask_default_terminal_cb,
                                         window);
    window->ask_default_infobar = nullptr;
  }

  remove_popup_info (window);

  if (window->search_popover != nullptr)
    {
      g_signal_handlers_disconnect_matched (window->search_popover, G_SIGNAL_MATCH_DATA,
                                            0, 0, nullptr, nullptr, window);
      g_signal_handlers_disconnect_matched (window->search_popover, G_SIGNAL_MATCH_DATA,
                                            0, 0, nullptr, nullptr, &window->search_popover);
      gtk_window_destroy (GTK_WINDOW (window->search_popover));
      window->search_popover = nullptr;
    }

  G_OBJECT_CLASS (terminal_window_parent_class)->dispose (object);
}

static void
terminal_window_finalize (GObject *object)
{
  TerminalWindow *window = TERMINAL_WINDOW (object);

  if (window->confirm_close_dialog)
    gtk_dialog_response (GTK_DIALOG (window->confirm_close_dialog),
                         GTK_RESPONSE_DELETE_EVENT);

  g_free (window->uuid);

  G_OBJECT_CLASS (terminal_window_parent_class)->finalize (object);
}

static gboolean
terminal_window_close_request (GtkWindow *window)
{
   return confirm_close_window_or_tab (TERMINAL_WINDOW (window), nullptr);
}

static void
terminal_window_show (GtkWidget *widget)
{
  TerminalWindow *window = TERMINAL_WINDOW (widget);
  GtkAllocation widget_allocation;

  gtk_widget_get_allocation (widget, &widget_allocation);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] show, size %d : %d at (%d, %d)\n",
                         widget,
                         widget_allocation.width, widget_allocation.height,
                         widget_allocation.x, widget_allocation.y);

  if (window->active_screen != nullptr)
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
  return reinterpret_cast<TerminalWindow*>
    (g_object_new (TERMINAL_TYPE_WINDOW,
		   "application", app,
		   nullptr));
}

static void
profile_set_cb (TerminalScreen *screen,
                GSettings *old_profile,
                TerminalWindow *window)
{
  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != window->active_screen)
    return;

  terminal_window_update_set_profile_menu_active_profile (window);
}

static void
sync_screen_title (TerminalScreen *screen,
                   GParamSpec *psepc,
                   TerminalWindow *window)
{
  const char *title;

  if (screen != window->active_screen)
    return;

  title = terminal_screen_get_title (screen);
  gtk_window_set_title (GTK_WINDOW (window),
                        title && title[0] ? title : _("Terminal"));
}

static void
screen_font_any_changed_cb (TerminalScreen *screen,
                            GParamSpec *psepc,
                            TerminalWindow *window)
{
  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;

  if (screen != window->active_screen)
    return;

  terminal_window_update_size (window);
}

static void
screen_hyperlink_hover_uri_changed (TerminalScreen *screen,
                                    const char *uri,
                                    const GdkRectangle *bbox G_GNUC_UNUSED,
                                    TerminalWindow *window G_GNUC_UNUSED)
{
  gs_free char *label = nullptr;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen)))
    return;

  label = terminal_util_hyperlink_uri_label (uri);

  gtk_widget_set_tooltip_text (GTK_WIDGET (screen), label);
}

/* MDI container callbacks */

static gboolean
screen_close_request_cb (TerminalNotebook *container,
                         TerminalScreen   *screen,
                         TerminalWindow   *window)
{
  if (confirm_close_window_or_tab (window, screen))
    return GDK_EVENT_STOP;
  return GDK_EVENT_PROPAGATE;
}

int
terminal_window_get_active_screen_num (TerminalWindow *window)
{
  return terminal_notebook_get_active_screen_num (window->notebook);
}

void
terminal_window_add_screen (TerminalWindow *window,
                            TerminalScreen *screen,
                            int            position)
{
  GtkWidget *old_window;

  old_window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (screen)));
  if (GTK_IS_ROOT (old_window) &&
      TERMINAL_IS_WINDOW (old_window) &&
      TERMINAL_WINDOW (old_window)== window)
    return;

  if (TERMINAL_IS_WINDOW (old_window))
    terminal_window_remove_screen (TERMINAL_WINDOW (old_window), screen);

  if (position == -1) {
    GSettings *global_settings = terminal_app_get_global_settings (terminal_app_get ());
    TerminalNewTabPosition position_pref = TerminalNewTabPosition
      (g_settings_get_enum (global_settings,
			    TERMINAL_SETTING_NEW_TAB_POSITION_KEY));
    switch (position_pref) {
    case TERMINAL_NEW_TAB_POSITION_NEXT:
      position = terminal_window_get_active_screen_num (window) + 1;
      break;

    default:
    case TERMINAL_NEW_TAB_POSITION_LAST:
      position = -1;
      break;
    }
  }

  terminal_notebook_add_screen (window->notebook, screen, position);

  gtk_widget_grab_focus (GTK_WIDGET (screen));
}

void
terminal_window_remove_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  terminal_notebook_remove_screen (window->notebook, screen);
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
  g_return_if_fail (gtk_widget_get_root (GTK_WIDGET (screen)) == GTK_ROOT (source_window));
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
  terminal_screen_container_destroy (screen_container);
  g_object_unref (screen_container);

  terminal_window_add_screen (dest_window, screen, dest_position);
  terminal_notebook_set_active_screen (dest_window->notebook, screen);
  g_object_unref (screen);
}

GList*
terminal_window_list_screen_containers (TerminalWindow *window)
{
  return terminal_notebook_list_screen_containers (window->notebook);
}

void
terminal_window_update_size (TerminalWindow *window)
{
  int grid_width, grid_height;
  int pixel_width, pixel_height;

  if (window->realized &&
      window_state_is_snapped(window->window_state))
    {
      /* Don't adjust the size of maximized or tiled (snapped, half-maximized)
       * windows: if we do, there will be ugly gaps of up to 1 character cell
       * around otherwise tiled windows. */
      return;
    }

  if (!window->active_screen)
    return;

  /* be sure our geometry is up-to-date */
  terminal_window_update_geometry (window);

  terminal_screen_get_size (window->active_screen, &grid_width, &grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] size is %dx%d cells of %dx%d px\n",
                         window, grid_width, grid_height,
                         window->old_char_width, window->old_char_height);

  /* the "old" struct members were updated by update_geometry */
  pixel_width = window->old_chrome_width + grid_width * window->old_char_width;
  pixel_height = window->old_chrome_height + grid_height * window->old_char_height;
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] %dx%d + %dx%d = %dx%d\n",
                         window, grid_width * window->old_char_width,
                         grid_height * window->old_char_height,
                         window->old_chrome_width, window->old_chrome_height,
                         pixel_width, pixel_height);

#ifdef GTK4_TODO
  gtk_window_resize (GTK_WINDOW (window), pixel_width, pixel_height);
#endif
}

void
terminal_window_switch_screen (TerminalWindow *window,
                               TerminalScreen *screen)
{
  terminal_notebook_set_active_screen (window->notebook, screen);
}

TerminalScreen*
terminal_window_get_active (TerminalWindow *window)
{
  return terminal_notebook_get_active_screen (window->notebook);
}

static void
notebook_screen_switched_cb (TerminalNotebook *container,
                        TerminalScreen   *old_active_screen,
                        TerminalScreen   *screen,
                        TerminalWindow   *window)
{
  int old_grid_width, old_grid_height;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen-switched old %p new %p\n",
                         window, old_active_screen, screen);

  if (window->disposed)
    return;

  if (screen == nullptr || old_active_screen == screen)
    return;

  if (window->search_popover != nullptr)
    gtk_widget_hide (GTK_WIDGET (window->search_popover));

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: setting active tab to screen %p (old active screen %p)\n",
                         window, screen, window->active_screen);

  if (old_active_screen != nullptr && screen != nullptr) {
    terminal_screen_get_size (old_active_screen, &old_grid_width, &old_grid_height);

    /* This is so that we maintain the same grid */
    vte_terminal_set_size (VTE_TERMINAL (screen), old_grid_width, old_grid_height);
  }

  window->active_screen = screen;

  sync_screen_title (screen, nullptr, window);

  /* set size of window to current grid size */
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[window %p] setting size after flipping notebook pages\n",
                         window);
  terminal_window_update_size (window);

  terminal_window_update_tabs_actions_sensitivity (window);
  terminal_window_update_terminal_menu (window);
  terminal_window_update_set_profile_menu_active_profile (window);
  terminal_window_update_copy_sensitivity (screen, window);
  terminal_window_update_zoom_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);
  terminal_window_update_paste_sensitivity (window);
}

static void
notebook_screen_added_cb (TerminalNotebook *notebook,
                          TerminalScreen   *screen,
                          TerminalWindow   *window)
{
  int pages;

  g_assert (TERMINAL_IS_NOTEBOOK (notebook));
  g_assert (TERMINAL_IS_SCREEN (screen));
  g_assert (TERMINAL_IS_WINDOW (window));

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p inserted\n",
                         window, screen);

  g_signal_connect (G_OBJECT (screen),
                    "profile-set",
                    G_CALLBACK (profile_set_cb),
                    window);

  /* FIXME: only connect on the active screen, not all screens! */
  g_signal_connect (screen, "notify::title",
                    G_CALLBACK (sync_screen_title), window);
  g_signal_connect (screen, "notify::font-desc",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::font-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::cell-height-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "notify::cell-width-scale",
                    G_CALLBACK (screen_font_any_changed_cb), window);
  g_signal_connect (screen, "selection-changed",
                    G_CALLBACK (terminal_window_update_copy_sensitivity), window);
  g_signal_connect (screen, "hyperlink-hover-uri-changed",
                    G_CALLBACK (screen_hyperlink_hover_uri_changed), window);

  g_signal_connect (screen, "show-popup-menu",
                    G_CALLBACK (screen_show_popup_menu_cb), window);
  g_signal_connect (screen, "match-clicked",
                    G_CALLBACK (screen_match_clicked_cb), window);
  g_signal_connect (screen, "resize-window",
                    G_CALLBACK (screen_resize_window_cb), window);

  g_signal_connect (screen, "close-screen",
                    G_CALLBACK (screen_close_cb), window);

  terminal_window_update_tabs_actions_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);
  terminal_window_update_paste_sensitivity (window);

#if 0
  /* FIXMEchpe: wtf is this doing? */

  /* If we have an active screen, match its size and zoom */
  if (window->active_screen)
    {
      int current_width, current_height;
      double scale;

      terminal_screen_get_size (window->active_screen, &current_width, &current_height);
      vte_terminal_set_size (VTE_TERMINAL (screen), current_width, current_height);

      scale = terminal_screen_get_font_scale (window->active_screen);
      terminal_screen_set_font_scale (screen, scale);
    }
#endif

  if (window->present_on_insert)
    {
      gtk_window_present (GTK_WINDOW (window));
      window->present_on_insert = FALSE;
    }

  pages = terminal_notebook_get_n_screens (notebook);
  if (pages == 2)
    {
      terminal_window_update_size (window);
    }
}

static void
notebook_screen_removed_cb (TerminalNotebook *notebook,
                            TerminalScreen   *screen,
                            TerminalWindow   *window)
{
  int pages;

  g_assert (TERMINAL_IS_NOTEBOOK (notebook));
  g_assert (TERMINAL_IS_SCREEN (screen));
  g_assert (TERMINAL_IS_WINDOW (window));

  if (window->disposed)
    return;

  _terminal_debug_print (TERMINAL_DEBUG_MDI,
                         "[window %p] MDI: screen %p removed\n",
                         window, screen);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)profile_set_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)sync_screen_title,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)screen_font_any_changed_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)terminal_window_update_copy_sensitivity,
                                        window);

  g_signal_handlers_disconnect_by_func (G_OBJECT (screen),
                                        (void*)screen_hyperlink_hover_uri_changed,
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_show_popup_menu_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_match_clicked_cb,
                                        window);
  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_resize_window_cb,
                                        window);

  g_signal_handlers_disconnect_by_func (screen,
                                        (void*)screen_close_cb,
                                        window);

  /* We already got a switch-page signal whose handler sets the active tab to the
   * new active tab, unless this screen was the only one in the notebook, so
   * window->active_tab is valid here.
   */

  pages = terminal_notebook_get_n_screens (notebook);
  if (pages == 0)
    {
      window->active_screen = nullptr;

      /* That was the last tab in the window; close it. */
      gtk_window_destroy (GTK_WINDOW (window));
      return;
    }

  terminal_window_update_tabs_actions_sensitivity (window);
  terminal_window_update_search_sensitivity (screen, window);

  if (pages == 1)
    {
      TerminalScreen *active_screen = terminal_notebook_get_active_screen (notebook);

      if (active_screen != nullptr)
        gtk_widget_grab_focus (GTK_WIDGET (active_screen));  /* bug 742422 */

      terminal_window_update_size (window);
    }
}

static void
notebook_screens_reordered_cb (TerminalNotebook *notebook,
                               TerminalWindow   *window)
{
  g_assert (TERMINAL_IS_WINDOW (window));
  g_assert (TERMINAL_IS_NOTEBOOK (notebook));

  terminal_window_update_tabs_actions_sensitivity (window);
}

gboolean
terminal_window_parse_geometry (TerminalWindow *window,
                                const char     *geometry)
{
  g_assert (TERMINAL_IS_WINDOW (window));

#ifdef GTK4_TODO
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
  if (window->active_screen)
    {
      int grid_width, grid_height;

      /* After parse_geometry(), the default size is in units of the
       * width/height increment, not a pixel size */
      gtk_window_get_default_size (GTK_WINDOW (window), &grid_width, &grid_height);

      vte_terminal_set_size (VTE_TERMINAL (window->active_screen),
			     grid_width, grid_height);
    }
#endif

  return TRUE;
}

void
terminal_window_update_geometry (TerminalWindow *window)
{
  GtkWidget *widget;
#ifdef GTK4_TODO
  GdkGeometry *hints = &window->hints;
#endif
  GtkBorder padding;
  GtkRequisition vbox_request, widget_request;
  int grid_width, grid_height;
  int char_width, char_height;
  int chrome_width, chrome_height;
  int csd_width = 0, csd_height = 0;

  if (gtk_widget_in_destruction (GTK_WIDGET (window)))
    return;

  if (window->active_screen == nullptr)
    return;

  widget = GTK_WIDGET (window->active_screen);

  /* We set geometry hints from the active term; best thing
   * I can think of to do. Other option would be to try to
   * get some kind of union of all hints from all terms in the
   * window, but that doesn't make too much sense.
   */
  terminal_screen_get_cell_size (window->active_screen, &char_width, &char_height);

  terminal_screen_get_size (window->active_screen, &grid_width, &grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "%dx%d cells of %dx%d px = %dx%d px\n",
                         grid_width, grid_height, char_width, char_height,
                         char_width * grid_width, char_height * grid_height);

  gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                &padding);

  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "padding = %dx%d px\n",
                         padding.left + padding.right,
                         padding.top + padding.bottom);

  gtk_widget_get_preferred_size (window->main_vbox, nullptr, &vbox_request);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "content area requests %dx%d px\n",
                         vbox_request.width, vbox_request.height);


  chrome_width = vbox_request.width - (char_width * grid_width);
  chrome_height = vbox_request.height - (char_height * grid_height);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "chrome: %dx%d px\n",
                         chrome_width, chrome_height);

  if (window->realized)
    {
      /* Only when having been realize the CSD can be calculated. Do this by
       * using the actual allocation rather then the preferred size as the
       * the preferred size takes the natural size of e.g. the title bar into
       * account which can be far wider then the contents size when using a
       * very long title */
      GtkAllocation toplevel_allocation, vbox_allocation;

      gtk_widget_get_allocation (GTK_WIDGET (window->main_vbox), &vbox_allocation);
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

  gtk_widget_get_preferred_size (widget, nullptr, &widget_request);
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "terminal widget requests %dx%d px\n",
                         widget_request.width, widget_request.height);

  if (!window->realized)
    {
      /* Don't actually set the geometry hints until we have been realized,
       * because we don't know how large the client-side decorations are going
       * to be. We also avoid setting window->old_csd_width or
       * window->old_csd_height, so that next time through this function we'll
       * definitely recalculate the hints.
       *
       * Similarly, the size request doesn't seem to include the padding
       * until we've been redrawn at least once. Don't resize the window
       * until we've done that. */
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY, "not realized yet\n");
    }
  else if (window_state_is_snapped(window->window_state))
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "Not applying geometry in snapped state\n");
    }
  else if (char_width != window->old_char_width ||
      char_height != window->old_char_height ||
      padding.left + padding.right != window->old_padding_width ||
      padding.top + padding.bottom != window->old_padding_height ||
      chrome_width != window->old_chrome_width ||
      chrome_height != window->old_chrome_height ||
      csd_width != window->old_csd_width ||
      csd_height != window->old_csd_height ||
      widget != (GtkWidget*) window->old_geometry_widget)
    {
#ifdef GTK4_TODO
      hints->base_width = chrome_width + csd_width;
      hints->base_height = chrome_height + csd_height;

      hints->width_inc = char_width;
      hints->height_inc = char_height;

      /* min size is min size of the whole window, remember. */
      hints->min_width = hints->base_width + hints->width_inc * MIN_WIDTH_CHARS;
      hints->min_height = hints->base_height + hints->height_inc * MIN_HEIGHT_CHARS;

      gtk_window_set_geometry_hints (GTK_WINDOW (window),
                                     nullptr,
                                     hints,
                                     GdkWindowHints(GDK_HINT_RESIZE_INC |
						    GDK_HINT_MIN_SIZE |
						    GDK_HINT_BASE_SIZE));

      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: base %dx%d min %dx%d inc %d %d\n",
                             window,
                             hints->base_width,
                             hints->base_height,
                             hints->min_width,
                             hints->min_height,
                             hints->width_inc,
                             hints->height_inc);
#endif

      window->old_csd_width = csd_width;
      window->old_csd_height = csd_height;
      window->old_geometry_widget = widget;
    }
  else
    {
      _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                             "[window %p] hints: increment unchanged, not setting\n",
                             window);
    }

  /* We need these for the size calculation in terminal_window_update_size()
   * (at least under GTK >= 3.19), so we set them unconditionally. */
  window->old_char_width = char_width;
  window->old_char_height = char_height;
  window->old_chrome_width = chrome_width;
  window->old_chrome_height = chrome_height;
  window->old_padding_width = padding.left + padding.right;
  window->old_padding_height = padding.top + padding.bottom;
}

static gboolean
destroy_window_in_main (gpointer user_data)
{
  TerminalWindow *window = TERMINAL_WINDOW (user_data);
  gtk_window_destroy (GTK_WINDOW (window));
  return G_SOURCE_REMOVE;
}

static void
confirm_close_response_cb (GtkWidget      *dialog,
                           int             response,
                           TerminalWindow *window)
{
  TerminalScreen *screen;

  g_assert (GTK_IS_WINDOW (dialog));
  g_assert (TERMINAL_IS_WINDOW (window));

  screen = (TerminalScreen*)g_object_get_data (G_OBJECT (dialog), "close-screen");

  terminal_notebook_confirm_close (window->notebook,
                                   screen,
                                   response == GTK_RESPONSE_ACCEPT);

  gtk_window_destroy (GTK_WINDOW (dialog));

  if (screen == nullptr)
    g_idle_add_full (G_PRIORITY_DEFAULT,
                     destroy_window_in_main,
                     g_object_ref (window),
                     g_object_unref);
}

/* Returns: TRUE if closing needs to wait until user confirmation;
 * FALSE if the terminal or window can close immediately.
 */
static gboolean
confirm_close_window_or_tab (TerminalWindow *window,
                             TerminalScreen *screen)
{
  GtkWidget *dialog;
  gboolean do_confirm;
  int n_tabs;

  if (window->confirm_close_dialog)
    {
      /* WTF, already have one? It's modal, so how did that happen? */
      gtk_dialog_response (GTK_DIALOG (window->confirm_close_dialog),
                           GTK_RESPONSE_DELETE_EVENT);
    }

  do_confirm = g_settings_get_boolean (terminal_app_get_global_settings (terminal_app_get ()),
                                       TERMINAL_SETTING_CONFIRM_CLOSE_KEY);
  if (!do_confirm)
    return FALSE;

  if (screen)
    {
      do_confirm = terminal_screen_has_foreground_process (screen, nullptr, nullptr);
      n_tabs = 1;
    }
  else
    {
      GList *tabs, *t;

      do_confirm = FALSE;

      tabs = terminal_window_list_screen_containers (window);
      n_tabs = g_list_length (tabs);

      for (t = tabs; t != nullptr; t = t->next)
        {
          TerminalScreen *terminal_screen;

          terminal_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (t->data));
          if (terminal_screen_has_foreground_process (terminal_screen, nullptr, nullptr))
            {
              do_confirm = TRUE;
              break;
            }
        }
      g_list_free (tabs);
    }

  if (!do_confirm)
    return FALSE;

  dialog = window->confirm_close_dialog =
    gtk_message_dialog_new (GTK_WINDOW (window),
                            GtkDialogFlags(GTK_DIALOG_MODAL |
                                           GTK_DIALOG_DESTROY_WITH_PARENT),
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

  GtkWidget *remove_button = gtk_dialog_add_button (GTK_DIALOG (dialog), n_tabs > 1 ? _("C_lose Window") : _("C_lose Terminal"), GTK_RESPONSE_ACCEPT);
  gtk_style_context_add_class (gtk_widget_get_style_context (remove_button), "destructive-action");
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  g_object_set_data (G_OBJECT (dialog), "close-screen", screen);

  g_signal_connect_swapped (dialog, "destroy",
                            G_CALLBACK (g_nullify_pointer),
                            &window->confirm_close_dialog);
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (confirm_close_response_cb),
                    window);

  gtk_window_present (GTK_WINDOW (dialog));

  return TRUE;
}

void
terminal_window_request_close (TerminalWindow *window)
{
  g_return_if_fail (TERMINAL_IS_WINDOW (window));

  if (confirm_close_window_or_tab (window, nullptr))
    return;

  gtk_window_destroy (GTK_WINDOW (window));
}

const char *
terminal_window_get_uuid (TerminalWindow *window)
{
  g_return_val_if_fail (TERMINAL_IS_WINDOW (window), nullptr);

  return window->uuid;
}
