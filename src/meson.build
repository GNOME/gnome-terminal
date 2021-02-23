# Copyright © 2019, 2020, 2021 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

src_inc = include_directories('.')

# Common sources

client_util_sources = files(
  'terminal-client-utils.c',
  'terminal-client-utils.h',
)

debug_sources = files(
  'terminal-debug.c',
  'terminal-debug.h',
)

dbus_sources = gnome.gdbus_codegen(
  'terminal-gdbus-generated',
  gt_dns_name + '.xml',
  autocleanup: 'none',
  install_header: false,
  interface_prefix: gt_dns_name,
  namespace: 'Terminal',
  object_manager: true,
)

i18n_sources = files(
  'terminal-i18n.c',
  'terminal-i18n.h',
  'terminal-intl.h',
)

misc_sources = files(
  'terminal-defines.h',
  'terminal-libgsystem.h',
)

profiles_sources = files(
  'terminal-profiles-list.c',
  'terminal-profiles-list.h',
  'terminal-schemas.h',
  'terminal-settings-list.c',
  'terminal-settings-list.h',
)

regex_sources = files(
  'terminal-regex.h',
)

types_sources = files(
  'terminal-enums.h',
)

types_sources += gnome.mkenums(
  'terminal-type-builtins',
  c_template: 'terminal-type-builtins.h.template',
  h_template: 'terminal-type-builtins.c.template',
  install_header: false,
  sources: types_sources,
)

version_conf = {
  'TERMINAL_MAJOR_VERSION': gt_major_version.to_string(),
  'TERMINAL_MICRO_VERSION': gt_micro_version.to_string(),
  'TERMINAL_MINOR_VERSION': gt_minor_version.to_string(),
}

version_sources = [configure_file(
  configuration: version_conf,
  input: 'terminal-version.h.in',
  install: false,
  output: '@BASENAME@',
)]

# Server

ui_xsltproc_command = [
  xsltproc,
  '--nonet',
  '-o', '@OUTPUT@',
  '@INPUT@',
]

menubar_ui_mnemonics = custom_target(
  'terminal-menubar-with-mnemonics.ui',
  command: ui_xsltproc_command,
  input: files(
    '..' / 'data' / 'ui-filter-mnemonics.xslt',
    'terminal-menubar.ui.in',
  ),
  install: false,
  output: 'terminal-menubar-with-mnemonics.ui',
)

menubar_ui_nomnemonics = custom_target(
  'terminal-menubar-without-mnemonics.ui',
  command: ui_xsltproc_command,
  input: files(
    '..' / 'data' / 'ui-filter-no-mnemonics.xslt',
    'terminal-menubar.ui.in',
  ),
  install: false,
  output: 'terminal-menubar-without-mnemonics.ui',
)

server_sources = client_util_sources + debug_sources + dbus_sources + i18n_sources + misc_sources + profiles_sources + regex_sources + types_sources + version_sources + files(
  'eggshell.c',
  'eggshell.h',
  'profile-editor.c',
  'profile-editor.h',
  'server.c',
  'terminal-accels.c',
  'terminal-accels.h',
  'terminal-app.c',
  'terminal-app.h',
  'terminal-enums.h',
  'terminal-gdbus.c',
  'terminal-gdbus.h',
  'terminal-headerbar.c',
  'terminal-headerbar.h',
  'terminal-icon-button.c',
  'terminal-icon-button.h',
  'terminal-info-bar.c',
  'terminal-info-bar.h',
  'terminal-mdi-container.c',
  'terminal-mdi-container.h',
  'terminal-menu-button.c',
  'terminal-menu-button.h',
  'terminal-notebook.c',
  'terminal-notebook.h',
  'terminal-pcre2.h',
  'terminal-prefs.c',
  'terminal-prefs.h',
  'terminal-screen-container.c',
  'terminal-screen-container.h',
  'terminal-screen.c',
  'terminal-screen.h',
  'terminal-search-popover.c',
  'terminal-search-popover.h',
  'terminal-tab-label.c',
  'terminal-tab-label.h',
  'terminal-util.c',
  'terminal-util.h',
  'terminal-window.c',
  'terminal-window.h',
)

server_sources += gnome.compile_resources(
  'terminal-resources',
  'terminal.gresource.xml',
  c_name: 'terminal',
  dependencies: [
    menubar_ui_mnemonics,
    menubar_ui_nomnemonics,
  ],
  export: false,
)

server_sources += gnome.genmarshal(
  'terminal-marshal',
  internal: true,
  install_header: false,
  prefix: '_terminal_marshal',
  sources: files(
    'terminal-marshal.list',
  ),
  stdinc: true,
  valist_marshallers: true,
)

if get_option('search_provider')

  server_sources += files(
    'terminal-search-provider.c',
    'terminal-search-provider.h',
  )

  server_sources += gnome.gdbus_codegen(
    'terminal-search-provider-gdbus-generated',
    gt_prefix / gt_dbusinterfacedir / 'org.gnome.ShellSearchProvider2.xml',
    autocleanup: 'none',
    install_header: false,
    interface_prefix: 'org.gnome.Shell',
    namespace: 'Terminal',
  )

endif

server_incs = [
  top_inc,
  src_inc,
]

server_cflags = [
  '-DTERMINAL_COMPILATION',
  '-DVTE_DISABLE_DEPRECATION_WARNINGS',
  '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
]

server_deps = [
  dconf_dep,
  gio_dep,
  gio_unix_dep,
  glib_dep,
  gtk_dep,
  pcre2_dep,
  pthreads_dep,
  schemas_dep,
  uuid_dep,
  vte_dep,
  x11_dep,
]

server = executable(
  gt_name + '-server',
  c_args: server_cflags,
  dependencies: server_deps,
  include_directories: server_incs,
  install: true,
  install_dir: gt_prefix / gt_libexecdir,
  sources: server_sources,
)

# systemd and D-Bus glue

server_conf = configuration_data()
server_conf.set('gt_name', gt_name)
server_conf.set('gt_dns_name', gt_dns_name)
server_conf.set('libexecdir', gt_prefix / gt_libexecdir)

configure_file(
  input: gt_dns_name + '.service.in',
  output: gt_dns_name + '.service',
  configuration: server_conf,
  install: true,
  install_dir: gt_prefix / gt_dbusservicedir,
)

configure_file(
  input: gt_name + '-server.service.in',
  output: gt_name + '-server.service',
  configuration: server_conf,
  install: true,
  install_dir: gt_prefix / gt_systemduserdir
)

# Search provider

if get_option('search_provider')

  provider_ini = files(gt_name + '-search-provider.ini')

  install_data(
    provider_ini,
    install_dir: gt_prefix / gt_searchproviderdir,
  )
endif # option 'search_provider'

# Legacy client

client_sources = client_util_sources + debug_sources + dbus_sources + i18n_sources + profiles_sources + types_sources + files(
  'terminal-options.c',
  'terminal-options.h',
  'terminal.c',
)

client_incs = [
  top_inc,
  src_inc,
]

client_incs = [
  top_inc,
  src_inc,
]

client_cflags = [
  '-DTERMINAL_COMPILATION',
  '-DTERMINAL_CLIENT',
  '-DTERM_DATADIR="@0@"'.format(gt_prefix / gt_datadir),
  '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
  '-DTERM_PKGDATADIR="@0@"'.format(gt_prefix / gt_pkgdatadir),
]

client_deps = [
  dconf_dep,
  gio_dep,
  gio_unix_dep,
  glib_dep,
  gtk_dep,
  x11_dep,
  uuid_dep,
  vte_dep,
]

client = executable(
  gt_name,
  c_args: client_cflags,
  include_directories: client_incs,
  dependencies: client_deps,
  install: true,
  sources: client_sources,
)

# Settings schemas

install_data(
  gt_dns_name + '.gschema.xml',
  install_dir: gt_prefix / gt_schemadir,
)

meson.add_install_script(
  'meson_compileschemas.py',
  gt_prefix / gt_schemadir,
)

# Nautilus extension

if get_option('nautilus_extension')

  nautilus_sources = client_util_sources + dbus_sources + i18n_sources + misc_sources + types_sources + files(
    'terminal-nautilus.c',
  )

  nautilus_map = meson.current_source_dir() / 'nautilus.map'

  nautilus_incs = [
    top_inc,
    src_inc,
  ]

  nautilus_cflags = [
    '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
  ]

  nautilus_deps = [
    gio_dep,
    gio_unix_dep,
    glib_dep,
    libnautilus_extension_dep,
  ]

  nautilus_ldflags = [
    '-Wl,--version-script,@0@'.format(nautilus_map),
  ]

  nautilus_extension = shared_library(
     'libterminal-nautilus',
     c_args: nautilus_cflags,
     dependencies: nautilus_deps,
     include_directories: nautilus_incs,
     install: true,
     install_dir: gt_prefix / gt_nautilusextensiondir,
     link_args: nautilus_ldflags,
     link_depends: nautilus_map,
     sources: nautilus_sources,
     soversion: '',
  )

endif # option 'nautilus_extension'

# Unit tests

test_regex_sources = regex_sources + files(
  'terminal-regex.c',
)

test_regex = executable(
  'test-regex',
  c_args: [
    '-DTERMINAL_REGEX_MAIN',
  ],
  dependencies: [
    glib_dep,
    pcre2_dep,
  ],
  include_directories: [top_inc, src_inc,],
  sources: test_regex_sources,
  install: false,
)

test_env = [
  'GNOME_TERMINAL_DEBUG=0',
  'VTE_DEBUG=0',
]

test_units = [
  ['regex', test_regex],
]

foreach test: test_units
  test(
    test[0],
    test[1],
    env: test_env,
  )
endforeach
