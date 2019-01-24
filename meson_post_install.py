#!/usr/bin/env python3

import os
import subprocess
import sys

if not os.environ.get('DESTDIR'):
    prefix = os.environ['MESON_INSTALL_PREFIX']

    icondir = os.path.join(prefix, sys.argv[1], 'icons', 'hicolor')
    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', sys.argv[2]])
