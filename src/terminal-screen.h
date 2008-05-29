/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2008 Christian Persch
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

#ifndef TERMINAL_SCREEN_H
#define TERMINAL_SCREEN_H

#include <vte/vte.h>

#include "terminal-profile.h"

G_BEGIN_DECLS

typedef enum {
  FLAVOR_AS_IS,
  FLAVOR_DEFAULT_TO_HTTP,
  FLAVOR_VOIP_CALL,
  FLAVOR_EMAIL
} TerminalURLFlavour;

/* Forward decls */
typedef struct _TerminalScreenPopupInfo TerminalScreenPopupInfo;
typedef struct _TerminalWindow        TerminalWindow;

#define TERMINAL_TYPE_SCREEN              (terminal_screen_get_type ())
#define TERMINAL_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_SCREEN, TerminalScreen))
#define TERMINAL_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_SCREEN, TerminalScreenClass))
#define TERMINAL_IS_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_SCREEN))
#define TERMINAL_IS_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_SCREEN))
#define TERMINAL_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_SCREEN, TerminalScreenClass))

typedef struct _TerminalScreen        TerminalScreen;
typedef struct _TerminalScreenClass   TerminalScreenClass;
typedef struct _TerminalScreenPrivate TerminalScreenPrivate;

struct _TerminalScreen
{
  VteTerminal parent_instance;

  TerminalScreenPrivate *priv;
};

struct _TerminalScreenClass
{
  VteTerminalClass parent_class;

  void (* profile_set)        (TerminalScreen *screen,
                               TerminalProfile *old_profile);
  void (* show_popup_menu)    (TerminalScreen *screen,
                               TerminalScreenPopupInfo *info);
  void (* skey_clicked)       (TerminalScreen *screen,
                               const char *skey_challenge);
  void (* url_clicked)        (TerminalScreen *screen,
                               const char *url,
                               int flavour);
  void (* close_screen)       (TerminalScreen *screen);
};

GType terminal_screen_get_type (void) G_GNUC_CONST;

TerminalScreen* terminal_screen_new                    (void);

void terminal_screen_set_profile (TerminalScreen *screen,
                                  TerminalProfile *profile);
TerminalProfile* terminal_screen_get_profile (TerminalScreen *screen);

void         terminal_screen_set_override_command (TerminalScreen  *screen,
                                                   char           **argv);
const char** terminal_screen_get_override_command (TerminalScreen  *screen);

void terminal_screen_launch_child (TerminalScreen *screen);

const char* terminal_screen_get_raw_title      (TerminalScreen *screen);
const char* terminal_screen_get_title          (TerminalScreen *screen);
const char* terminal_screen_get_icon_title     (TerminalScreen *screen);
gboolean    terminal_screen_get_icon_title_set (TerminalScreen *screen);

void terminal_screen_set_user_title (TerminalScreen *screen,
                                     const char *text);

void        terminal_screen_set_dynamic_title      (TerminalScreen *screen,
                                                    const char     *title,
						    gboolean	   userset);
void        terminal_screen_set_dynamic_icon_title (TerminalScreen *screen,
                                                    const char     *title,
						    gboolean	   userset);
void        terminal_screen_set_title              (TerminalScreen *screen,
                                                    const char     *title);

const char *terminal_screen_get_dynamic_title      (TerminalScreen *screen);
const char *terminal_screen_get_dynamic_icon_title (TerminalScreen *screen);

void        terminal_screen_set_working_dir   (TerminalScreen *screen,
                                               const char     *dirname);
const char *terminal_screen_get_working_dir   (TerminalScreen *screen);

void        terminal_screen_set_font (TerminalScreen *screen);
void        terminal_screen_set_font_scale    (TerminalScreen *screen,
                                               double          factor);
double      terminal_screen_get_font_scale    (TerminalScreen *screen);

void       terminal_screen_get_size (TerminalScreen *screen,
                                     int *width_chars,
                                     int *height_chars);
void       terminal_screen_get_cell_size (TerminalScreen *screen,
                                          int *width_chars,
                                          int *height_chars);

void _terminal_screen_update_scrollbar (TerminalScreen *screen);

/* Allow scales a bit smaller and a bit larger than the usual pango ranges */
#define TERMINAL_SCALE_XXX_SMALL   (PANGO_SCALE_XX_SMALL/1.2)
#define TERMINAL_SCALE_XXXX_SMALL  (TERMINAL_SCALE_XXX_SMALL/1.2)
#define TERMINAL_SCALE_XXXXX_SMALL (TERMINAL_SCALE_XXXX_SMALL/1.2)
#define TERMINAL_SCALE_XXX_LARGE   (PANGO_SCALE_XX_LARGE*1.2)
#define TERMINAL_SCALE_XXXX_LARGE  (TERMINAL_SCALE_XXX_LARGE*1.2)
#define TERMINAL_SCALE_XXXXX_LARGE (TERMINAL_SCALE_XXXX_LARGE*1.2)
#define TERMINAL_SCALE_MINIMUM     (TERMINAL_SCALE_XXXXX_SMALL/1.2)
#define TERMINAL_SCALE_MAXIMUM     (TERMINAL_SCALE_XXXXX_LARGE*1.2)

struct _TerminalScreenPopupInfo {
  int ref_count;
  TerminalWindow *window;
  TerminalScreen *screen;
  char *string;
  TerminalURLFlavour flavour;
  guint button;
  guint32 timestamp;
};

TerminalScreenPopupInfo *terminal_screen_popup_info_ref (TerminalScreenPopupInfo *info);

void terminal_screen_popup_info_unref (TerminalScreenPopupInfo *info);

G_END_DECLS

#endif /* TERMINAL_SCREEN_H */
