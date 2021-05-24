GNOME TErminal
==============

Source & Releases
-----------------

To get the source code, use
```
$ git clone https://gitlab.gnome.org/GNOME/gnome-terminal
```

To get the source for a release version, use the corresponding git tag, or
download a tarball at
https://gitlab.gnome.org/GNOME/gnome-terminal/-/archive/TAG/gnome-terminal-TAG.tar.bz2
replacing `TAG` with the desired tag's name (e.g. `3.40.0`). Older releases are also
available at https://download.gnome.org/sources/gnome-terminal .

Building from source
--------------------

You will most likely need to also build `vte` from source; see https://gitlab.gnome.org/GNOME/vte/-/blob/master/README.md .

Start by installing the build dependencies, and a C++ compiler.

For fedora and related distributions, use
```
sudo dnf build-dep vte291 gnome-terminal
sudo dnf install g++
```
while for debian and related distributions, use
```
sudo apt-get build-dep libvte-2.91-0 gnome-terminal
sudo apt-get install g++
```

First build `vte` according to its own instructions. Then:
```
$ # Get the source code
$ git clone https://gitlab.gnome.org/GNOME/gnome-terminal
$
$ # Change to the toplevel directory
$ cd gnome-terminal
$
$ # Run the configure script (choose an appropriate path instead of "/some/where"!)
$ # Don't forget to make sure that pkg-config can find your self-build vte!
$ # e.g. by doing:
$ # export PKG_CONFIG_PATH=/some/where/lib64/pkg-config:$PKG_CONFIG_PATH
$ #
$ # If you compiled gnome-shell into the same prefix, you can omit disabling
$ # the search provider.
$ #
$ meson _build --prefix=/some/where -Dsearch_provider=false
$
$ # Build
$ ninja -C _build
$
$ # Install
$ ninja -C _build install
```

* By default, GNOME Terminal will install under `/usr/local`, which is not usually
the right choice. You can customize the prefix directory by `--prefix` option, e.g.
if you want to install GNOME-TERMINAL under `~/foobar`, you should run
`meson _build --prefix=$HOME/foobar`. If you already run the configure script before,
you should also pass `--reconfigure` option to it.

* You may need to execute `ninja -C _build install` as root
(i.e. `sudo ninja -C _build install`) if installing to system directories. Use a
user-writable directory as `--prefix` instead to avoid that.

* You may need to set the gsettings schema path so that gnome-terminal can find
its schemas. E.g. use
```
$ export GSETTINGS_SCHEMA_DIR=/some/where/share/glib-2.0/schemas
```
and if you skipped the `ninja install` step, you need to create the
gsettings schema cache yourself.

* Since GNOME Terminal uses a D-Bus activated server, you cannot simply run
the self-built gnome-terminal directly. Instead, you need to start the new `gnome-terminal-server` directly using
```
$ ./_build/src/gnome-terminal-server --app-id test.Terminal &
```
and then you have 10s time to open a window in that server using
```
$ ./_build/src/gnome-terminal --app-id test.Terminal
```

Also see https://wiki.gnome.org/Apps/Terminal/Debugging for more information.

Debugging
---------

After installing GNOME-TERMINAL with `-Ddebugg=true` flag, you can use `GNOME-TERMINAL_DEBUG` variable to control
GNOME-TERMINAL to print out the debug information

```
# You should change gnome-terminal-[2.91] to the version you build
$ GNOME-TERMINAL_DEBUG=selection ./_build/src/app/gnome-terminal-2.91

# Or, you can mixup with multiple logging level
$ GNOME-TERMINAL_DEBUG=selection,draw,cell ./_build/src/app/gnome-terminal-2.91

$ Or, you can use `all` to print out all logging message
$ GNOME-TERMINAL_DEBUG=all ./_build/src/app/gnome-terminal-2.91
```

For logging level information, please refer to enum [Gnome-TerminalDebugFlags](src/debug.h).

Contributing
------------

Bugs should be filed here: https://gitlab.gnome.org/GNOME/gnome-terminal/issues/
Please note that this is a bug tracker to be used for developers of GNOME Terminal,
and contributors of code, documentation, and translations to GNOME Terminal,
and *not a support forum*.

If you are an end user, always file bugs in your distribution's bug tracker, or use their
support forums.

If you want to provide a patch, please attach them to an issue in GNOME
GitLab, in the format output by the `git format-patch` command.
