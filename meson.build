project(
  'GNOME Terminal', 'c',
  version: '3.33.0',
  license: 'GPL3+',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.50.0',
)

gt_name = 'gnome-terminal'

gt_version = meson.project_version()
version_array = gt_version.split('.')
gt_major_version = version_array[0].to_int()
gt_minor_version = version_array[1].to_int()
gt_micro_version = version_array[2].to_int()

gt_prefix = get_option('prefix')
gt_datadir = get_option('datadir')
gt_libdir = get_option('libdir')
gt_libexecdir = get_option('libexecdir')
gt_localedir = get_option('localedir')

gt_pkgdatadir = gt_datadir / gt_name

gt_namespace = 'Terminal'
gt_iface_name = 'org.gnome.' + gt_namespace

gt_debug = get_option('buildtype').contains('debug')

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

source_root = meson.current_source_dir()

po_dir = source_root / 'po'

intltool_merge = find_program('intltool-merge')
intltool_cache = po_dir / '.intltool-merge-cache'
intltool_desktop_cmd = [intltool_merge, '-d', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@']
intltool_xml_cmd = [intltool_merge, '-x', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@']

top_inc = include_directories('.')

cc = meson.get_compiler('c')

config_h = configuration_data()

# version
config_h.set_quoted('VERSION', gt_version)

# Globally define_GNU_SOURCE and therefore enable the GNU extensions
config_h.set('_GNU_SOURCE', true)

# Debugging
config_h.set('ENABLE_DEBUG', gt_debug)

# i18n
config_h.set_quoted('GETTEXT_PACKAGE', gt_name)

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']
warn_flags = []

if gt_debug
  common_flags += cc.get_supported_arguments([
    #'-pthread',
    #'-pipe',
    '-fdiagnostics-show-option',
    '-fno-common',
    '-fno-strict-aliasing',
    '-fstack-protector',
    '-fstack-protector-strong',
    '-fvisibility=hidden',
    '-Waggregate-return',
    '-Wcast-align',
    '-Werror=format=2',
    '-Werror=format-nonliteral',
    '-Werror=format-security',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Wfloat-equal',
    '-Wformat-signedness',
    '-Wlogical-op',
    '-Wmissing-declarations',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wnested-externs',
    '-Wno-missing-field-initializers',
    '-Wno-switch-enum',
    '-Wno-unused-parameter',
    '-Wold-style-definition',
    '-Wpacked',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wundef',
    '-Wunsafe-loop-optimizations',
    '-Wwrite-strings',
  ])
endif

add_project_arguments(common_flags, language: 'c')

dconf_dep = dependency('dconf', version: '>= 0.14.0')
gio_dep = dependency('gio-2.0', version: '>= 2.34.0')
glib_dep = dependency('glib-2.0', version: '>= 2.42.0')
gsettings_desktop_schemas_dep = dependency('gsettings-desktop-schemas', version: '>= 0.1.0')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.12.0')
libpcre_dep = dependency('libpcre2-8', version: '>= 10.00')
threads_dep = dependency('threads')
uuid_dep = dependency('uuid')
vte_dep = dependency('vte-2.91', version: '>= 0.55.92')

m_dep = cc.find_library('m')

gdk_dep = dependency('gdk-3.0')
gdk_targets = gdk_dep.get_pkgconfig_variable('targets')

gdk_x11_target = gdk_targets.contains('x11')
if gdk_x11_target
  x11_dep = dependency('x11')
endif

if gdk_targets.contains('win32') or gdk_targets.contains('quartz')
  warning('unsupported GDK backend target')
endif

if not (gdk_targets.contains('broadway') or gdk_targets.contains('wayland'))
  warning('unknown GDK backend target')
endif

# GIO schemas
gio_schemasdir = gio_dep.get_pkgconfig_variable(
  'schemasdir',
  define_variable: ['datadir', gt_prefix / gt_datadir],
  default: gt_prefix / gt_datadir / 'glib-2.0/schemas',
)

# systemd user unit directory
systemd_systemduserunitdir = get_option('systemduserunitdir')
if systemd_systemduserunitdir == ''
  # FIXME: this would ideally use the systemduserunitdir pkgconfig variable, but
  # it does not depend on variables we can override to install within prefix.
  #systemd_systemduserunitdir = dependency('systemd').get_pkgconfig_variable('systemduserunitdir')
  systemd_systemduserunitdir = gt_libdir / 'systemd/user'
endif

# DBus
dbus_dep = dependency('dbus-1')
dbus_session_bus_services_dir = dbus_dep.get_pkgconfig_variable('session_bus_services_dir', define_variable: ['datadir', gt_prefix / gt_datadir])

# GNOME Shell search provider
enable_search_provider = get_option('search_provider')
if enable_search_provider
  shell_search_provider_iface = dbus_dep.get_pkgconfig_variable('interfaces_dir') / 'org.gnome.ShellSearchProvider2.xml'
  res = run_command(find_program('test'), '-e', shell_search_provider_iface)
  assert(res.returncode() == 0, 'gnome-shell search provider requested but interface definition file not found')
endif
config_h.set('ENABLE_SEARCH_PROVIDER', enable_search_provider)

# Nautilus extension
enable_nautilus_extension = get_option('nautilus_extension')
if enable_nautilus_extension
  libnautilus_extension_dep = dependency('libnautilus-extension', version: '>= 3.0.0')
  libnautilus_extension_extensiondir = libnautilus_extension_dep.get_pkgconfig_variable('extensiondir', define_variable: ['libdir', gt_prefix / gt_libdir])

  appdata = gt_iface_name + '.Nautilus.metainfo.xml'

  custom_target(
    appdata,
    input: appdata + '.in',
    output: '@BASENAME@',
    command: intltool_xml_cmd,
    install: true,
    install_dir: gt_datadir / 'metainfo',
  )
endif

subdir('data/icons')
subdir('src')
subdir('po')
subdir('help')

desktop_conf = {'VERSION': gt_version}

desktop = gt_iface_name + '.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: '@BASENAME@',
  configuration: desktop_conf,
)

custom_target(
  desktop,
  input: desktop_in,
  output: '@BASENAME@',
  command: intltool_desktop_cmd,
  install: true,
  install_dir: gt_datadir / 'applications',
)

appdata = gt_iface_name + '.appdata.xml'

custom_target(
  appdata,
  input: appdata + '.in',
  output: '@BASENAME@',
  command: intltool_xml_cmd,
  install: true,
  install_dir: gt_datadir / 'metainfo',
)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

meson.add_install_script(
  'meson_post_install.py',
  gt_datadir,
  gio_schemasdir,
)

output = '\ngnome-terminal-' + gt_version + ':\n\n'
output += '\tprefix:                 ' + gt_prefix + '\n'
output += '\tsource code location:   ' + source_root + '\n'
output += '\tcompiler:               ' + cc.get_id() + '\n'
output += '\tDBus service dir:       ' + dbus_session_bus_services_dir + '\n'
output += '\tDebug:                  ' + gt_debug.to_string() + '\n'
output += '\tSearch provider:        ' + enable_search_provider.to_string() + '\n'
if enable_search_provider
  output += '\tShell search provider:  ' + shell_search_provider_iface + '\n'
endif
output += '\tNautilus extension:     ' + enable_nautilus_extension.to_string() + '\n'
if enable_nautilus_extension
  output += '\tNautilus extension dir: ' + libnautilus_extension_extensiondir
endif
message(output)
