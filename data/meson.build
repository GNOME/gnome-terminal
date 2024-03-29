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

# Appstream data

metainfo_its = [
  'metainfo.its',
]

metainfodir = gt_datadir / 'metainfo'

i18n.itstool_join(
  input: gt_dns_name + '.metainfo.xml.in',
  install: true,
  install_dir: gt_prefix / metainfodir,
  install_tag: 'metadata',
  its_files: metainfo_its,
  mo_targets: gettext_targets[0],
  output: '@BASENAME@',
)

i18n.itstool_join(
  input: gt_dns_name + '.Nautilus.metainfo.xml.in',
  install: true,
  install_dir: gt_prefix / metainfodir,
  install_tag: 'metadata',
  its_files: metainfo_its,
  mo_targets: gettext_targets[0],
  output: '@BASENAME@',
)

meson.add_install_script(
  'meson_metainfofile.py',
  metainfodir,
  gt_dns_name + '.metainfo.xml',
  gt_dns_name + '.Nautilus.metainfo.xml',
)

# Desktop file

desktopdatadir = gt_datadir / 'applications'

i18n.merge_file(
  input: gt_dns_name + '.desktop.in',
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: po_dir,
  install: true,
  install_dir: desktopdatadir,
  install_tag: 'runtime',
)

i18n.merge_file(
  input: gt_dns_name + '.Preferences.desktop.in',
  install: true,
  install_dir: desktopdatadir,
  install_tag: 'runtime',
  output: '@BASENAME@',
  po_dir: po_dir,
  type: 'desktop',
)

meson.add_install_script(
  'meson_desktopfile.py',
  desktopdatadir,
  gt_dns_name + '.desktop',
  gt_dns_name + '.Preferences.desktop',
)

# Install a symlink for xdg-terminal-exec
install_symlink(
  gt_dns_name + '.desktop',
  install_dir: gt_datadir / 'xdg-terminals',
  install_tag: 'runtime',
  pointing_to: '..' / 'applications' / (gt_dns_name + '.desktop'),
)

# Subdirs

subdir('icons')
