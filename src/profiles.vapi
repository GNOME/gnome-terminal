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

[CCode (cprefix = "Terminal", lower_case_cprefix = "terminal_")]
namespace Terminal {

  [CCode (cheader_filename = "terminal-settings-list.h", type_id = "terminal_settings_list_get_type")]
  public class SettingsList : GLib.Object {

    [Flags, CCode (cprefix = "TERMINAL_SETTINGS_LIST_FLAG_", has_type_id = false)]
    public enum Flags {
      NONE,
      HAS_DEFAULT,
      ALLOW_EMPTY
    }

    [CCode (has_construct_function = false)]
    public SettingsList (string path, string schema_id, string child_schema_id, Flags flags = Flags.NONE);

    [CCode (array_length = false, array_null_terminated = true)]
    public string[]? dupv_children ();
    public GLib.List<GLib.Settings> ref_children ();

    public bool has_child (string uuid);
    public GLib.Settings? ref_child (string uuid);

    public string add_child ();
    public string clone_child (string uuid);
    public void remove_child (string uuid);

    public string dup_uuid_from_child (GLib.Settings child);
    public GLib.Settings? ref_default_child ();
    public string dup_default_child ();
    public void set_default_child (string uuid);

    public static bool valid_uuid (string uuid);
  }

  [CCode (cname = "TerminalSettingsList", cprefix = "terminal_profiles_list_", cheader_filename = "terminal-profiles-list.h", type_id = "terminal_settings_list_get_type")]
  public class ProfilesList : SettingsList {

    public ProfilesList ();

    public GLib.List<GLib.Settings> ref_children_sorted ();

    public string dup_uuid (string uuid) throws GLib.Error;
    public string dup_uuid_or_name (string uuid_or_name) throws GLib.Error;

    public GLib.Settings ref_profile_by_uuid (string uuid) throws GLib.Error;
    public GLib.Settings ref_profile_by_uuid_or_name (string uuid_or_name) throws GLib.Error;
  }

}
