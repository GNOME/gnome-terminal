service_conf = {'libexecdir': gt_prefix / gt_libexecdir}

configure_file(
  input: gt_iface_name + '.service.in',
  output: '@BASENAME@',
  configuration: service_conf,
  install: true,
  install_dir: dbus_session_bus_services_dir,
)

configure_file(
  input: gt_name + '-server.service.in',
  output: '@BASENAME@',
  configuration: service_conf,
  install: true,
  install_dir: systemd_systemduserunitdir,
)

install_data(
  gt_iface_name + '.gschema.xml',
  install_dir: gio_schemasdir,
)

enums_header = files('terminal-enums.h')

enums = 'terminal-type-builtins'

enums_sources = gnome.mkenums(
  enums,
  sources: enums_header,
  c_template: enums + '.c.template',
  h_template: enums + '.h.template',
)

gdbus_sources = gnome.gdbus_codegen(
  'terminal-gdbus-generated',
  gt_iface_name + '.xml',
  interface_prefix: gt_iface_name,
  namespace: gt_namespace,
  object_manager: true,
)

common_deps = [
  dconf_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
  uuid_dep,
  vte_dep,
]

if gdk_x11_target
  common_deps += x11_dep
endif

# gnome-terminal-server
sources = files(
  'eggshell.c',
  'profile-editor.c',
  'server.c',
  'terminal-accels.c',
  'terminal-app.c',
  'terminal-debug.c',
  'terminal-encoding.c',
  'terminal-gdbus.c',
  'terminal-headerbar.c',
  'terminal-i18n.c',
  'terminal-icon-button.c',
  'terminal-info-bar.c',
  'terminal-mdi-container.c',
  'terminal-menu-button.c',
  'terminal-notebook.c',
  'terminal-prefs.c',
  'terminal-profiles-list.c',
  'terminal-screen.c',
  'terminal-screen-container.c',
  'terminal-search-popover.c',
  'terminal-settings-list.c',
  'terminal-tab-label.c',
  'terminal-util.c',
  'terminal-window.c',
)

version_conf = {
  'TERMINAL_MAJOR_VERSION': gt_major_version,
  'TERMINAL_MINOR_VERSION': gt_minor_version,
  'TERMINAL_MICRO_VERSION': gt_micro_version,
}

sources += configure_file(
  input: 'terminal-version.h.in',
  output: '@BASENAME@',
  configuration: version_conf,
)

marshal = 'terminal-marshal'

sources += gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: '_' + marshal.underscorify(),
  internal: true,
)

resource_data = files(
  'preferences.ui',
  'search-popover.ui',
  'terminal.about',
  'terminal.common.css',
  'terminal-headerbar.ui',
  'terminal-headermenu.ui',
  'terminal-notebook-menu.ui',
  'terminal-window.ui',
)

menubar = 'terminal-menubar'

mnemonics_conf = {
  'WITH_MNEMONIC_START': '',
  'WITH_MNEMONIC_END': '',
  'WITHOUT_MNEMONIC_START': '<!--',
  'WITHOUT_MNEMONIC_END': '-->',
}

resource_data += configure_file(
  input: menubar + '.ui.meson',
  output: menubar + '-with-mnemonics.ui',
  configuration: mnemonics_conf,
)

mnemonics_conf = {
  'WITH_MNEMONIC_START': '<!--',
  'WITH_MNEMONIC_END': '-->',
  'WITHOUT_MNEMONIC_START': '',
  'WITHOUT_MNEMONIC_END': '',
}

resource_data += configure_file(
  input: menubar + '.ui.meson',
  output: menubar + '-without-mnemonics.ui',
  configuration: mnemonics_conf,
)

resource_prefix = 'terminal'

sources += gnome.compile_resources(
  resource_prefix + '-resources',
  resource_prefix + '.gresource.xml',
  c_name: resource_prefix,
  dependencies: resource_data,
  export: true,
)

deps = common_deps + [
  gsettings_desktop_schemas_dep,
  libpcre_dep,
  m_dep,
  threads_dep,
]

c_flags = [
  '-DTERMINAL_COMPILATION',
  '-DVTE_DISABLE_DEPRECATION_WARNINGS',
  '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
]

# GNOME Shell search provider
if enable_search_provider
  sources += files('terminal-search-provider.c')

  sources += gnome.gdbus_codegen(
    'terminal-search-provider-gdbus-generated',
    shell_search_provider_iface,
    interface_prefix: 'org.gnome.Shell',
    namespace: gt_namespace,
  )

  install_data(
    gt_name + '-search-provider.ini',
    install_dir: gt_datadir / 'gnome-shell/search-providers',
  )
endif

executable(
  gt_name + '-server',
  sources + [enums_sources, gdbus_sources],
  include_directories: top_inc,
  dependencies: deps,
  c_args: c_flags,
  install: true,
  install_dir: gt_libexecdir,
)

# Legacy terminal client
sources = files(
  'terminal.c',
  'terminal-client-utils.c',
  'terminal-debug.c',
  'terminal-i18n.c',
  'terminal-options.c',
  'terminal-profiles-list.c',
  'terminal-settings-list.c',
)

c_flags = [
  '-DTERMINAL_COMPILATION',
  '-DTERMINAL_CLIENT',
  '-DTERM_DATADIR="@0@"'.format(gt_prefix / gt_datadir),
  '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
  '-DTERM_PKGDATADIR="@0@"'.format(gt_prefix / gt_pkgdatadir),
]

executable(
  gt_name,
  sources + [enums_sources, gdbus_sources],
  include_directories: top_inc,
  dependencies: common_deps,
  c_args: c_flags,
  install: true,
)

# Nautilus extension
if enable_nautilus_extension
  sources = files(
    'terminal-client-utils.c',
    'terminal-i18n.c',
    'terminal-nautilus.c',
  )

  deps = [
    gio_dep,
    glib_dep,
    gtk_dep,
    libnautilus_extension_dep,
  ]

  symbol_map = meson.current_source_dir() / 'nautilus.map'
  ldflags = cc.get_supported_link_arguments('-Wl,--version-script,@0@'.format(symbol_map))

  shared_module(
    'terminal-nautilus',
    sources: sources + [enums_sources, gdbus_sources],
    include_directories: top_inc,
    dependencies: deps,
    c_args: '-DTERM_LOCALEDIR="@0@"'.format(gt_prefix / gt_localedir),
    link_args: ldflags,
    link_depends: symbol_map,
    install: true,
    install_dir: libnautilus_extension_extensiondir,
  )
endif

# Checks
test_unit = 'terminal-regex'

exe = executable(
  test_unit,
  test_unit + '.c',
  include_directories: top_inc,
  c_args: '-DTERMINAL_REGEX_MAIN',
  dependencies: glib_dep,
)

test(test_unit, exe)
