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

project(
  'gnome-terminal',
  ['c',],
  default_options: [
    'buildtype=release',
    'c_std=gnu11',
    'warning_level=' + (meson.version().version_compare('>= 0.50.0') ? '0' : '1'),
    'b_ndebug=false',
  ],
  license: [
    'GPL-3.0-or-later',             # the programme
    'GPL-3.0-only', 'CC-BY-SA-3.0', # the documentation
    'GFDL-1.3-only',                # the appstream data
  ],
  meson_version: '>= 0.49.0',
  version: '3.41.0',
)

# Naming

gt_name     = 'gnome-terminal'
gt_dns_name = 'org.gnome.Terminal'

# Compiler requirements

c_req_std = 'gnu11'

# Requirements

vte_req                      = 'vte-2.91'
vte_req_version              = '0.63.0'

glib_req_version             = '2.52.0'
glib_min_req_version         = '2.52'
glib_max_allowed_version     = '2.52'

gtk_req_version              = '3.22.27'
gtk_min_req_version          = '3.18'
gtk_max_allowed_version      = '3.24'

dconf_req_version            = '0.14.0'
libnautilus_ext_req_version  = '3.28.0'
pcre2_req_version            = '10.00'
schemas_req_version          = '0.1.0'

# Versioning

gt_version = meson.project_version()

version_split = meson.project_version().split('.')
gt_major_version = version_split[0].to_int()
gt_minor_version = version_split[1].to_int()
gt_micro_version = version_split[2].to_int()

# Directories

gt_datadir    = get_option('datadir')
gt_includedir = get_option('includedir')
gt_libdir     = get_option('libdir')
gt_libexecdir = get_option('libexecdir')
gt_localedir  = get_option('localedir')
gt_prefix     = get_option('prefix')

gt_pkgdatadir = gt_datadir / gt_name

gt_schemadir = gt_datadir / 'glib-2.0' / 'schemas'

gt_searchproviderdir = gt_datadir / 'gnome-shell' / 'search-providers'

gt_nautilusextensiondir = gt_libdir / 'nautilus' / 'extensions-3.0'

gt_dbusinterfacedir = gt_datadir / 'dbus-1' / 'interfaces'
gt_dbusservicedir   = gt_datadir / 'dbus-1' / 'services'

# It is correct for this to be in ${prefix}/lib, even on systems where that
# does not match ${libdir}. This is what systemd uses on such platforms.
gt_systemduserdir = 'lib' / 'systemd' / 'user'

# Debug

enable_debug = get_option('dbg') or get_option('debug') or get_option('buildtype').contains('debug')

# Meson modules

gnome = import('gnome')
i18n  = import('i18n')
pkg   = import('pkgconfig')

# i18n

gt_gettext_domain = gt_name
po_dir = meson.current_source_dir() / 'po'

# Compilers

cc = meson.get_compiler('c')

c_std_opt = '-std=' + c_req_std
assert(cc.has_argument(c_std_opt), 'option ' + c_std_opt + ' not supported by ' + cc.get_id())
add_project_arguments(c_std_opt, language: 'c')

# Asserts must not be disabled

assert(get_option('b_ndebug') == 'false', 'assertions may not be disabled')

# Start config.h

config_h = configuration_data()

config_h.set_quoted('GETTEXT_PACKAGE', gt_gettext_domain)
config_h.set_quoted('VERSION', gt_version)
config_h.set('ENABLE_DEBUG', enable_debug)
config_h.set('ENABLE_SEARCH_PROVIDER', get_option('search_provider'))

# FIXME AC_USE_SYSTEM_EXTENSIONS also supported non-gnu systems
config_h.set10('_GNU_SOURCE', true)


ver = glib_min_req_version.split('.')
config_h.set('GLIB_VERSION_MIN_REQUIRED', '(G_ENCODE_VERSION(' + ver[0] + ',' + ver[1] + '))')
ver = glib_max_allowed_version.split('.')
config_h.set('GLIB_VERSION_MAX_ALLOWED', '(G_ENCODE_VERSION(' + ver[0] + ',' + ver[1] + '))')

ver = gtk_min_req_version.split('.')
config_h.set('GDK_VERSION_MIN_REQUIRED', '(G_ENCODE_VERSION(' + ver[0] + ',' + ver[1] + '))')

ver = gtk_max_allowed_version.split('.')
config_h.set('GDK_VERSION_MAX_ALLOWED', '(G_ENCODE_VERSION(' + ver[0] + ',' + ver[1] + '))')

# Compiler flags

compiler_flags_common = [
  '-Waggregate-return',
  '-Wall',
  '-Wcast-align',
  '-Wendif-labels',
  '-Werror=implicit-function-declaration',
  '-Werror=init-self',
  '-Werror=missing-include-dirs',
  '-Werror=missing-prototypes',
  '-Werror=pointer-arith',
  '-Wextra',
  '-Wfloat-equal',
  '-Wformat-signedness',
  '-Wimplicit',
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
  '-Wsign-compare',
  '-Wstrict-aliasing=2',
  '-Wstrict-prototypes',
  '-Wundef',
  '-Wuninitialized',
  '-Wunsafe-loop-optimizations',
  '-Wvla',
  '-Wwrite-strings',
  '-fdiagnostics-show-option',
  '-fno-common',
  '-fstack-protector',
  '-fstack-protector-strong',
  '-fvisibility=hidden',
]

if enable_debug
  compiler_flags_common += [
    '-ggdb3',
  ]
endif

# These are currently needed but the code should be fixed instead
compiler_flags_common_undesirable = [
  '-fno-strict-aliasing'
]

compiler_flags_cc_required = [
  '-fvisibility=hidden',
]

global_cflags = cc.get_supported_arguments(compiler_flags_common +
                                           compiler_flags_common_undesirable +
                                           compiler_flags_cc_required)

foreach flag: compiler_flags_cc_required
  assert(cc.has_argument(flag), flag + ' is required but not supported')
endforeach

# These flags have to be tested together

compiler_flags_common_multi = [
  [
    '-Werror=format=2',
    '-Werror=format-nonliteral',
    '-Werror=format-security',
  ],
]

foreach flags : compiler_flags_common_multi
  if cc.has_multi_arguments(flags)
    global_cflags += flags
  endif
endforeach

# ... and now make these flags the default

add_project_arguments(global_cflags, language: 'c',)

# Dependencies

vte_dep       = dependency(vte_req,        version: '>=' + vte_req_version)

dconf_dep     = dependency('dconf',        version: '>=' + dconf_req_version)
gio_dep       = dependency('gio-2.0',      version: '>=' + glib_req_version)
gio_unix_dep  = dependency('gio-unix-2.0', version: '>=' + glib_req_version)
glib_dep      = dependency('glib-2.0',     version: '>=' + glib_req_version)
gtk_dep       = dependency('gtk+-3.0',     version: '>=' + gtk_req_version)
pcre2_dep     = dependency('libpcre2-8',   version: '>=' + pcre2_req_version)
pthreads_dep  = dependency('threads')
schemas_dep   = dependency('gsettings-desktop-schemas',
                                           version: '>=' + schemas_req_version)
uuid_dep      = dependency('uuid')

if get_option('nautilus_extension')
  libnautilus_extension_dep = dependency('libnautilus-extension', version: '>=' + libnautilus_ext_req_version,)
else
  libnautilus_extension_dep = dependency('', required: false,)
endif

if gtk_dep.get_pkgconfig_variable('targets').contains('x11')
  x11_dep = dependency('x11')
else
  x11_dep = dependency('', required: false)
endif

# Write config.h

configure_file(
  output: 'config.h',
  configuration: config_h,
)

top_inc = include_directories('.')

# Utilities

xsltproc = find_program('xsltproc')

# Subdirs

subdir('src')
subdir('data')
subdir('po')

if get_option('docs')
  subdir('help')
  subdir('man')
endif

# Dist

meson.add_dist_script(
  find_program('meson_changelog.sh'),
)

# Simple compat Makefile

makefile_conf = configuration_data()
makefile_conf.set('srcdir', meson.current_source_dir())
makefile_conf.set('builddir', meson.current_build_dir())

configure_file(
  input: 'Makefile.meson',
  output: '@BASENAME@',
  configuration: makefile_conf,
)

# .gitignore everything in the build directory

configure_file(
  output: '.gitignore',
  command: ['echo', '**/**',],
  capture: true,
  install: false,
)

# Summary

output = '\n'
output += 'Configuration for gnome-terminal:\n\n'
output += '  Version:                ' + gt_version + '\n'
output += '\n'
output += '  C compiler:             ' + cc.get_id() + '\n\n'
output += '  Coverage:               ' + get_option('b_coverage').to_string() + '\n'
output += '  Documentation:          ' + get_option('docs').to_string() + '\n'
output += '  Debug:                  ' + enable_debug.to_string() + '\n'
output += '\n'
output += '  Prefix:                 ' + get_option('prefix') + '\n'
output += '\n'
output += '  Nautilus extension:     ' + get_option('nautilus_extension').to_string() + '\n'
output += '  Search provider:        ' + get_option('search_provider').to_string() + '\n'
message(output)

# Done