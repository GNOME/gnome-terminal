/*
 * Copyright © 2011 Christian Persch
 *
 * This programme is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This programme is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this programme; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TERMINAL_CLIENT_UTILS_H
#define TERMINAL_CLIENT_UTILS_H

#include <gio/gio.h>

G_BEGIN_DECLS

void terminal_client_append_create_instance_options (GVariantBuilder *builder,
                                                     const char      *display_name,
                                                     const char      *startup_id,
                                                     const char      *geometry,
                                                     const char      *role,
                                                     const char      *profile,
                                                     const char      *title,
                                                     gboolean         maximise_window,
                                                     gboolean         fullscreen_window);

void terminal_client_append_exec_options            (GVariantBuilder *builder,
                                                     const char      *working_directory,
                                                     gboolean         shell);

void terminal_client_get_fallback_startup_id        (char           **startup_id);

G_END_DECLS

#endif /* TERMINAL_UTIL_UTILS_H */
