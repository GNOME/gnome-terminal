/*
 * Copyright © 2014 Christian Persch
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

[CCode (lower_case_cprefix = "terminal_client_", cheader_filename = "terminal-client-utils.h")]
namespace Terminal.Client {

  public void append_create_instance_options (GLib.VariantBuilder builder,
                                              string display_name,
                                              string? startup_id,
                                              string? geometry,
                                              string? role,
                                              string? profile,
                                              string? title,
                                              bool maximise_window,
                                              bool fullscreen_window);

  [CCode (cname = "PassFdElement", has_type_id = false)]
  [SimpleType]
  public struct PassFdElement {
    public int index;
    public int fd;
  }

  public void append_exec_options (GLib.VariantBuilder builder,
                                   string? working_directory,
                                   PassFdElement[]? fd_array,
                                   bool shell);

  public string? get_fallback_startup_id ();
}
