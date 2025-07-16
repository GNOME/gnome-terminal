# Building the local test build:

```bash
meson setup build --prefix=$HOME/gnome-terminal-custom -Dsearch_provider=false
ninja -C build
ninja -C build install
```

# Recompile Gshemas
```bash
glib-compile-schemas ~/gnome-terminal-custom/share/glib-2.0/schemas
```

# ReBuilding local test build.

```bash
ninja -C build clean
meson setup build --reconfigure --prefix=$HOME/gnome-terminal-custom -Dsearch_provider=false
ninja -C build
ninja -C build install
```

# Running my local dev build - so I don't forget.

```bash
$HOME/gnome-terminal-custom/libexec/gnome-terminal-server --app-id test.Terminal &
```

# Running the client.
```bash
$HOME/gnome-terminal-custom/bin/gnome-terminal --app-id test.Terminal
```
