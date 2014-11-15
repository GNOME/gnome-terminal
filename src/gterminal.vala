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

namespace GTerminal
{
  /* Output handling */

  public struct Output
  {
    public static bool quiet = false;
    public static bool verbose = false;

    public static const OptionEntry[] entries = {
      { "quiet",   0,   OptionFlags.HIDDEN, OptionArg.NONE, ref quiet,
        N_("Suppress output"), null },
      { "verbose", 'v', OptionFlags.HIDDEN, OptionArg.NONE, ref verbose,
        N_("Verbose output"), null },
      { null, 0, 0, 0, null, null, null }
    };

    public static void set_quiet (bool value)
    {
      quiet = value;
    }

    public GLib.OptionGroup get_option_group ()
    {
      var group = new GLib.OptionGroup ("output",
                                        N_("Output options:"),
                                        N_("Show output options"),
                                        null, null);
      group.add_entries (entries);
      group.set_translation_domain(Config.GETTEXT_PACKAGE);
      return group;
    }

    [PrintfFormat]
    public void print(string format,
                      ...)
    {
      if (!quiet)
        stdout.vprintf (format, va_list());
    }

    [PrintfFormat]
    public void printerr(string format, ...)
    {
      if (!quiet)
        stderr.vprintf (format, va_list());
    }

    [PrintfFormat]
    public void info(string format, ...)
    {
      if (verbose)
        stderr.vprintf (format, va_list());
    }
  }

  /* Global options */

  public struct GlobalOptions {
    public static string? app_id = null;

    private static bool option_app_id (string option_name,
                                       string value,
                                       void *unused_user_data) throws OptionError
    {
      if (!GLib.Application.id_is_valid (value))
        throw new OptionError.BAD_VALUE (_("\"%s\" is not a valid application ID"), value);
      app_id = value;
      return true;
    }

    public static string get_app_id ()
    {
      return app_id != null ? app_id : "org.gnome.Terminal";
    }

    private static const OptionEntry[] entries = {
      { "app-id", 0, OptionFlags.HIDDEN, OptionArg.CALLBACK, (void*) option_app_id,
        N_("Server application ID"), N_("ID") },
      { null, 0, 0, 0, null, null, null }
    };

    public static GLib.OptionGroup get_option_group ()
    {
      var group = new GLib.OptionGroup ("global",
                                        N_("Global options:"),
                                        N_("Show global options"),
                                        null, null);
      group.add_entries (entries);
      group.set_translation_domain(Config.GETTEXT_PACKAGE);
      return group;
    }
  }

  public struct OpenOptions {

    [CCode (array_length = false, array_null_terminated = true)]
    private static string[]? pass_fds = null;
    private static bool pass_stdin = false;
    private static bool pass_stdout = false;
    private static bool pass_stderr = false;
    private static Terminal.Client.PassFdElement[]? fd_array = null;
    public static GLib.UnixFDList? fd_list = null;

    private static bool post_parse (OptionContext context,
                                    OptionGroup group,
                                    void *unused_user_data) throws Error
    {
      if (pass_stdin || pass_stdout || pass_stderr)
        throw new OptionError.BAD_VALUE (pass_stdin  ? _("FD passing of stdin is not supported") :
                                         pass_stdout ? _("FD passing of stdout is not supported") :
                                                       _("FD passing of stderr is not supported"));

      if (pass_fds == null)
        return true;

      fd_list = new GLib.UnixFDList ();
      Terminal.Client.PassFdElement[] arr = {};

      for (uint i = 0; i < pass_fds.length; i++) {
        int64 v;
        if (!int64.try_parse (pass_fds[i], out v) ||
            v == -1 || v < int.MIN || v > int.MAX)
          throw new OptionError.BAD_VALUE (_("Invalid argument \"%s\" to --fd option"), pass_fds[i]);

        int fd = (int) v;

        if (fd == Posix.STDIN_FILENO ||
            fd == Posix.STDOUT_FILENO ||
            fd == Posix.STDERR_FILENO)
          throw new OptionError.BAD_VALUE (fd == Posix.STDIN_FILENO  ? _("FD passing of stdin is not supported") :
                                           fd == Posix.STDOUT_FILENO ? _("FD passing of stdout is not supported") :
                                                                       _("FD passing of stderr is not supported"));

        for (uint j = 0; j < arr.length; j++) {
          if (arr[j].fd == fd)
            throw new OptionError.BAD_VALUE (_("Cannot pass FD %d twice"), fd);
        }

        var idx = fd_list.append (fd);
        Terminal.Client.PassFdElement e = { idx, fd };
        arr += e;

        if (fd == Posix.STDOUT_FILENO ||
            fd == Posix.STDERR_FILENO) {
          GTerminal.Output.set_quiet (true);
        }
#if 0
        if (fd == Posix.STDIN_FILENO)
          data->wait = TRUE;
#endif
      }

      fd_array = arr;
      return true;
    }

    private static const OptionEntry[] exec_entries = {
      { "stdin", 0, OptionFlags.HIDDEN, OptionArg.NONE, ref pass_stdin,
        N_("Forward stdin"), null },
      { "stdout", 0, OptionFlags.HIDDEN, OptionArg.NONE, ref pass_stdout,
        N_("Forward stdout"), null },
      { "stderr", 0, OptionFlags.HIDDEN, OptionArg.NONE, ref pass_stderr,
        N_("Forward stderr"), null },
      { "fd", 0, 0, OptionArg.STRING_ARRAY, ref pass_fds,
        N_("Forward file descriptor"), N_("FD") },
      { null, 0, 0, 0, null, null, null }
    };

    public static GLib.OptionGroup get_exec_option_group ()
    {
      var group = new GLib.OptionGroup ("exec",
                                        N_("Exec options:"),
                                        N_("Show exec options"),
                                        null, null);
      group.add_entries (exec_entries);
      group.set_translation_domain(Config.GETTEXT_PACKAGE);
      group.set_parse_hooks (null, (OptionParseFunc)post_parse);
      return group;
    }

    /* Window options */

    public static string? geometry = null;
    public static string? role = null;
    public static bool show_menubar = true;
    public static bool show_menubar_set = false;
    public static bool maximise = false;
    public static bool fullscreen = false;

    private static const OptionEntry[] window_entries = {
      { "maximise", 0, 0, OptionArg.NONE, ref maximise,
        N_("Maximise the window"), null },
      { "fullscreen", 0, 0, OptionArg.NONE, ref fullscreen,
        N_("Full-screen the window"), null },
      { "geometry", 0, 0, OptionArg.STRING, ref geometry,
        N_("Set the window size; for example: 80x24, or 80x24+200+200 (COLSxROWS+X+Y)"),
        N_("GEOMETRY") },
      { "role", 0, 0, OptionArg.STRING, ref role,
        N_("Set the window role"), N_("ROLE") },
      { null, 0, 0, 0, null, null, null }
    };

    public static GLib.OptionGroup get_window_option_group()
    {
      var group = new GLib.OptionGroup ("window",
                                        N_("Window options:"),
                                        N_("Show window options"),
                                        null, null);
      group.add_entries (window_entries);
      group.set_translation_domain(Config.GETTEXT_PACKAGE);
      return group;
    }

    /* Terminal options */

    public static string? working_directory = null;
    public static string? profile = null;
    public static double zoom = 1.0;

    private static bool option_profile (string option_name,
                                        string? value,
                                        void *unused_user_data) throws Error
    {
      if (profile != null)
        throw new OptionError.BAD_VALUE (_("May only use option %s once"), option_name);

        var profiles = new Terminal.ProfilesList ();
        profile = profiles.dup_uuid (value);
        return true;
    }

    private static bool option_zoom (string option_name,
                                     string? value,
                                     void *unused_user_data) throws Error
    {
      double v;
      if (!double.try_parse (value, out v))
        throw new OptionError.BAD_VALUE (_("\"%s\" is not a valid zoom factor"),
                                         value);

      if (v < 0.25 || v > 4.0)
        throw new OptionError.BAD_VALUE (_("Zoom value \"%s\" is outside allowed range"),
                                         value);

      zoom = v;
      return true;
    }

    private static const OptionEntry[] terminal_entries = {
      { "profile", 0, 0, OptionArg.CALLBACK, (void*) option_profile,
        N_("Use the given profile instead of the default profile"),
        N_("UUID") },
      { "cwd", 0, 0, OptionArg.FILENAME, ref working_directory,
        N_("Set the working directory"), N_("DIRNAME") },
      { "zoom", 0, 0, OptionArg.CALLBACK, (void*) option_zoom,
        N_("Set the terminal's zoom factor (1.0 = normal size)"),
        N_("ZOOM") },
      { null, 0, 0, 0, null, null, null }
    };

    public static GLib.OptionGroup get_terminal_option_group ()
    {
      var group = new GLib.OptionGroup ("terminal",
                                        N_("Terminal options:"),
                                        N_("Show terminal options"),
                                        null, null);
      group.add_entries (terminal_entries);
      group.set_translation_domain(Config.GETTEXT_PACKAGE);
      return group;
    }

    /* Processing options */

    public static bool wait_for_remote = false;

    private static const OptionEntry[] processing_entries = {
    { "wait", 0, 0, OptionArg.NONE, ref wait_for_remote,
      N_("Wait until the child exits"), null },
      { null, 0, 0, 0, null, null, null }
    };

    public static GLib.OptionGroup get_processing_option_group ()
    {
      var group = new GLib.OptionGroup ("processing",
                                        N_("Processing options:"),
                                        N_("Show processing options"),
                                        null, null);
      group.add_entries (processing_entries);
      group.set_translation_domain(Config.GETTEXT_PACKAGE);
      return group;
    }

    /* Argument parsing */

    [CCode (array_length = false, array_null_terminated = true)]
    public static string[]? argv_pre = null;
    [CCode (array_length = false, array_null_terminated = true)]
    public static string[]? argv_post = null;
    public static string? display_name = null;
    public static string? startup_id = null;

    public static void parse_argv (string[] argv) throws Error
    {
      /* Need to save this before gtk_init is being called! */
      startup_id = Environment.get_variable ("DESKTOP_STARTUP_ID");

      /* If there's a '--' argument with other arguments after it,
       * strip them off. Need to do this before parsing the options!
       */

      bool found_dashdash = false;
      for (uint i = 0; i < argv.length; i++) {
        if (argv[i] != "--")
          continue;

        if (i > 0)
          argv_pre = argv[0:i];
        else
          argv_pre = null;

        if (i + 1 < argv.length)
          argv_post = argv[i+1:argv.length];
        else
          argv_post = null;

        found_dashdash = true;
        break;
      }

      if (!found_dashdash) {
        argv_pre = argv;
        argv_post = null;
      }

      var context = new GLib.OptionContext ("— terminal client");
      context.set_translation_domain (Config.GETTEXT_PACKAGE);
      context.add_group (Gtk.get_option_group (true));
      context.add_group (GlobalOptions.get_option_group ());
      context.add_group (get_window_option_group ());
      context.add_group (get_terminal_option_group ());
      context.add_group (get_exec_option_group ());
      context.add_group (get_processing_option_group ());

      context.parse_strv (ref argv_pre);

      if (working_directory == null)
        working_directory = Environment.get_current_dir ();

      /* Do this here so that gdk_display is initialized */
      if (startup_id == null)
        startup_id = Terminal.Client.get_fallback_startup_id ();

      display_name = Gdk.Display.get_default ().get_name ();
    }

  } /* struct OpenOptions */

  /* DBUS Interfaces */

  [DBus (name = "org.gnome.Terminal.Factory0")]
  interface Server : DBusProxy {
    public const string SERVICE_NAME = "org.gnome.Terminal";
    public const string INTERFACE_NAME = "org.gnome.Terminal.Factory0";
    public const string OBJECT_PATH = "/org/gnome/Terminal/Factory0";

    /* public abstract GLib.ObjectPath CreateInstance (HashTable<string, Variant> dict) throws IOError; */
  }

  [DBus (name = "org.gnome.Terminal.Terminal0")]
  interface Receiver : DBusProxy {
    public const string INTERFACE_NAME = "org.gnome.Terminal.Terminal0";

    /* public abstract void Exec (HashTable<string, Variant> options,
       [DBus (signature = "aay")] string[] arguments) throws IOError; */
    public signal void ChildExited (int exit_code);
  }

  /* DBus helper functions */

  private Server get_server () throws IOError
  {
    return Bus.get_proxy_sync (BusType.SESSION,
                               GlobalOptions.get_app_id (),
                               Server.OBJECT_PATH,
                               DBusProxyFlags.DO_NOT_LOAD_PROPERTIES |
                               DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);
  }

  private Receiver create_terminal () throws Error
  {
    var server = get_server ();

    var builder = new GLib.VariantBuilder (VariantType.VARDICT);
    Terminal.Client.append_create_instance_options (builder,
                                                    OpenOptions.display_name,
                                                    OpenOptions.startup_id,
                                                    OpenOptions.geometry,
                                                    OpenOptions.role,
                                                    OpenOptions.profile,
                                                    null /* title */,
                                                    OpenOptions.maximise,
                                                    OpenOptions.fullscreen);
    if (OpenOptions.show_menubar_set)
      builder.add ("{sv}", "show-menubar", new Variant.boolean (OpenOptions.show_menubar));

    /* FIXME: Not using the proxy method since the generated code seems broken… */
    var path = server.call_sync ("CreateInstance" /* (a{sv}) */,
                                 new Variant ("(a{sv})", builder),
                                 DBusCallFlags.NO_AUTO_START, -1,
                                 null);

    string obj_path;
    path.get ("(o)", out obj_path);

    return Bus.get_proxy_sync (BusType.SESSION,
                               GlobalOptions.get_app_id (),
                               obj_path,
                               DBusProxyFlags.DO_NOT_LOAD_PROPERTIES);
  }

  /* Helper functions */

  private int mangle_exit_code (int status)
  {
    if (Process.if_exited (status))
      return Process.exit_status (status).clamp (0, 127);
    else if (Process.if_signaled (status))
      return 128 + (int) Process.term_sig (status);
    else
      return 127;
  }

  /* Verbs */

  private int run (Receiver receiver)
  {
    int status = 0;
    var loop = new GLib.MainLoop ();
    var id = receiver.ChildExited.connect((s) => {
      if (loop.is_running ())
        loop.quit ();
      status = s;
    });

    loop.run ();
    receiver.disconnect(id);

    return mangle_exit_code (status);
  }

  private int open (string[] argv) throws Error
  {
    OpenOptions.parse_argv (argv);

    if (argv[0] == "run" && OpenOptions.argv_post == null)
      throw new OptionError.BAD_VALUE (_("'%s' needs the command to run as arguments after '--'"),
                                       argv[0]);

    var receiver = create_terminal ();

    var builder = new GLib.VariantBuilder (VariantType.TUPLE);
    builder.open (VariantType.VARDICT); {
      Terminal.Client.append_exec_options (builder,
                                           OpenOptions.working_directory,
                                           OpenOptions.fd_array,
                                           argv[0] == "shell");
    } builder.close ();
    builder.add_value (new Variant.bytestring_array (OpenOptions.argv_post));

    receiver.call_with_unix_fd_list_sync ("Exec" /* (a{sv}aay) */,
                                          builder.end (),
                                          DBusCallFlags.NO_AUTO_START, -1,
                                          OpenOptions.fd_list);

    if (!OpenOptions.wait_for_remote)
      return Posix.EXIT_SUCCESS;

    return run (receiver);
  }

  private int help (string[] argv) throws Error
  {
    /* FIXME: launch man pager for gterminal(1) */
    return Posix.EXIT_SUCCESS;
  }

  private int complete (string[] argv) throws Error
  {
    if (argv.length < 2)
      throw new OptionError.UNKNOWN_OPTION (_("Missing argument"));

    if (argv[1] == "commands") {
      string? prefix = argv.length > 2 ? argv[2] : null;
      for (uint i = 0; i < commands.length; i++) {
        if (commands[i].verb.has_prefix ("_"))
          continue;
        if (prefix == null || commands[i].verb.has_prefix (prefix))
          print ("%s\n", commands[i].verb);
      }

      return Posix.EXIT_SUCCESS;
    } else if (argv[1] == "profiles") {
      var service = new Terminal.ProfilesList ();
      var profiles = service.dupv_children ();
      string? prefix = argv.length > 2 ? argv[2] : null;
      for (uint i = 0; i < profiles.length; i++) {
        if (prefix == null || profiles[i].has_prefix (prefix))
          print ("%s\n", profiles[i]);
      }

      return Posix.EXIT_SUCCESS;
    }

    throw new OptionError.UNKNOWN_OPTION (_("Unknown completion request for \"%s\""), argv[0]);
  }

  private delegate int CommandFunc (string[] args) throws Error;

  private struct CommandMap {
    unowned string verb;
    unowned CommandFunc func;
  }

  private static const CommandMap[] commands = {
    { "help", help },
    { "open", open },
    { "shell", open },
    { "_complete", complete },
  };

  public static int main (string[] argv)
  {
    Environment.set_prgname ("gterminal");

    Intl.setlocale (LocaleCategory.ALL, "");
    Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
    Intl.textdomain (Config.GETTEXT_PACKAGE);
    Environment.set_application_name (_("GTerminal"));

    try {
      if (argv.length == 1) {
        throw new OptionError.FAILED (_("Missing command"));
      }

      for (uint i = 0; i < commands.length; i++) {
        if (commands[i].verb == argv[1]) {
          return commands[i].func (argv[1:argv.length]);
        }
      }

      throw new OptionError.FAILED (_("Unknown command \"%s\""), argv[1]);
    } catch (Error e) {
      DBusError.strip_remote_error (e);

      printerr (_("Error processing arguments: %s\n"), e.message);
      return Posix.EXIT_FAILURE;
    }
  }

} /* namespace GTerminal */
