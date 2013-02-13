#!/bin/sh
# Run this to generate all the initial makefiles, etc.

set -e

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

pushd "$srcdir"
mkdir -p m4 &>/dev/null || true
autoreconf --verbose --force --install
intltoolize --force
popd

test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
