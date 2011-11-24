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

#include "config.h"

#include "terminal-client-utils.h"

#include <gio/gio.h>

/**
 * terminal_client_append_create_instance_options:
 * @builder: a #GVariantBuilder of #GVariantType "a{sv}"
 * @display: (array element-type=guint8):
 * @startup_id: (array element-type=guint8):
 * @geometry:
 * @role:
 * @profile:
 * @title:
 * @maximise_window:
 * @fullscreen_window:
 *
 * Appends common options to @builder.
 */
void 
terminal_client_append_create_instance_options (GVariantBuilder *builder,
                                                const char      *display_name,
                                                const char      *startup_id,
                                                const char      *geometry,
                                                const char      *role,
                                                const char      *profile,
                                                const char      *title,
                                                gboolean         maximise_window,
                                                gboolean         fullscreen_window)
{
  /* Bytestring options */
  g_variant_builder_add (builder, "{sv}",
                         "display", g_variant_new_bytestring (display_name));
  if (startup_id)
    g_variant_builder_add (builder, "{sv}", 
                           "desktop-startup-id", g_variant_new_bytestring (startup_id));

  /* String options */
  if (profile)
    g_variant_builder_add (builder, "{sv}", 
                           "profile", g_variant_new_string (profile));
  if (title)
    g_variant_builder_add (builder, "{sv}", 
                           "title", g_variant_new_string (title));
  if (geometry)
    g_variant_builder_add (builder, "{sv}", 
                           "geometry", g_variant_new_string (geometry));
  if (role)
    g_variant_builder_add (builder, "{sv}", 
                           "role", g_variant_new_string (role));

  /* Boolean options */
  if (maximise_window)
    g_variant_builder_add (builder, "{sv}", 
                           "maximize-window", g_variant_new_boolean (TRUE));
  if (fullscreen_window)
    g_variant_builder_add (builder, "{sv}", 
                           "fullscreen-window", g_variant_new_boolean (TRUE));
}

/**
 * terminal_client_append_exec_options:
 * @builder: a #GVariantBuilder of #GVariantType "a{sv}"
 * @working_directory: (allow-none): the cwd, or %NULL
 *
 * Appends the environment and the working directory to @builder.
 */
void 
terminal_client_append_exec_options (GVariantBuilder *builder,
                                     const char      *working_directory)
{
  char **envv;

  envv = g_get_environ ();
  envv = g_environ_unsetenv (envv, "DESKTOP_STARTUP_ID");
  envv = g_environ_unsetenv (envv, "GIO_LAUNCHED_DESKTOP_FILE_PID");
  envv = g_environ_unsetenv (envv, "GIO_LAUNCHED_DESKTOP_FILE");

  g_variant_builder_add (builder, "{sv}",
                         "environ",
                         g_variant_new_bytestring_array ((const char * const *) envv, -1));

  if (working_directory)
    g_variant_builder_add (builder, "{sv}", 
                           "cwd", g_variant_new_bytestring (working_directory));

  g_strfreev (envv);
}
