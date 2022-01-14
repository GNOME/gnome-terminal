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

metainfodir = gt_datadir / 'metainfo'

i18n.merge_file(
  input: gt_dns_name + '.metainfo.xml.in',
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: gt_prefix / metainfodir,
)

i18n.merge_file(
  input: gt_dns_name + '.Nautilus.metainfo.xml.in',
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: gt_prefix / metainfodir,
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
)

meson.add_install_script(
  'meson_desktopfile.py',
  desktopdatadir,
  gt_dns_name + '.desktop',
)

# Subdirs

subdir('icons')
