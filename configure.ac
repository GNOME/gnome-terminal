m4_define([gt_version_major],[3])
m4_define([gt_version_minor],[3])
m4_define([gt_version_micro],[0])
m4_define([gt_version_extra],[])
m4_define([gt_version],[gt_version_major().gt_version_minor().gt_version_micro()gt_version_extra])

m4_define([gt_api_version],[0])

AC_INIT([GNOME Terminal],[gt_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-terminal],[gnome-terminal])
AC_CONFIG_SRCDIR([src/terminal.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

GETTEXT_PACKAGE=gnome-terminal
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

AC_C_BIGENDIAN

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.40.0])

GNOME_COMMON_INIT
GNOME_DEBUG_CHECK

AM_GLIB_GNU_GETTEXT

GLIB_REQUIRED=2.31.0
GIO_REQUIRED=2.27.0
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=0.1.0

AC_MSG_CHECKING([which gtk+ version to compile against])
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=3.0],[which gtk+ version to compile against (default: 3.0)])],
  [case "$with_gtk" in
     3.0) ;;
     2.0) AC_MSG_ERROR([unsupported gtk version $with_gtk specified]) ;;
     *) AC_MSG_ERROR([invalid gtk version $with_gtk specified]) ;;
   esac],
  [with_gtk=3.0])
AC_MSG_RESULT([$with_gtk])

case "$with_gtk" in
  3.0) GTK_API_VERSION=3.0
       GTK_REQUIRED=3.0.0
       VTE_PC_VERSION=-2.90
       VTE_REQUIRED=0.27.3
       ;;
esac

AC_MSG_CHECKING([for GDK target])
case "$with_gtk" in
  3.0) for target in $($PKG_CONFIG --variable targets gdk-$GTK_API_VERSION); do
         GDK_TARGET="$target"
         break
       done
       ;;
esac
AC_MSG_RESULT([$GDK_TARGET])

case "$GDK_TARGET" in
  x11) PLATFORM_DEPS="x11" ;;
  win32|quartz) PLATFORM_DEPS="" ;;
  *) AC_MSG_ERROR([unknown gdk target]) ;;
esac

PKG_CHECK_MODULES([TERM],
  [vte$VTE_PC_VERSION >= $VTE_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gthread-2.0
   gio-2.0 >= $GIO_REQUIRED
   gtk+-$GTK_API_VERSION >= $GTK_REQUIRED
   gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED
   $PLATFORM_DEPS])

# ****
# DBus
# ****

AC_ARG_WITH([dbus-service-dir],
  [AS_HELP_STRING([--with-dbus-service-dir=PATH],[dbus service file directory])],
  [dbusservicedir="$withval"],
  [dbusservicedir='${datadir}/dbus-1/services'])
AC_SUBST([dbusservicedir])

AC_ARG_WITH
# *********
# GSettings
# *********

GLIB_GSETTINGS

# ********
# smclient
# ********

SMCLIENT_PKGS=
AC_MSG_CHECKING([which smclient backend to use])
AC_ARG_WITH([smclient],
  [AS_HELP_STRING([--with-smclient-backend],[which smclient backend to use (xsmp/win32/quartz)])],
  [],
  [case "$GDK_TARGET" in
    x11) with_smclient=xsmp SMCLIENT_PKGS="sm ice" ;;
    win32|quartz) with_smclient=$GDK_TARGET ;;
    *) with_smclient=no ;;
   esac])
AC_MSG_RESULT([$with_smclient])

if test "$with_smclient" != "no"; then
  AC_DEFINE([WITH_SMCLIENT],[1],[Define if smclient is enabled])
fi

if test -n "$SMCLIENT_PKGS"; then
  PKG_CHECK_MODULES([SMCLIENT],[$SMCLIENT_PKGS])
  AC_SUBST([SMCLIENT_CFLAGS])
  AC_SUBST([SMCLIENT_LIBS])
fi

AM_CONDITIONAL([WITH_SMCLIENT],[test "$with_smclient" != "no"])
AM_CONDITIONAL([WITH_SMCLIENT_XSMP],[test "$with_smclient" = "xsmp"])
AM_CONDITIONAL([WITH_SMCLIENT_WIN32],[test "$with_smclient" = "win32"])
AM_CONDITIONAL([WITH_SMCLIENT_QUARTZ],[test "$with_smclient" = "quartz"])

# *************************
# GConf-GSettings Migration
# *************************

GCONF_REQUIRED=2.31.3

PKG_CHECK_MODULES([MIGRATOR],[
  gio-2.0 >= $GIO_REQUIRED
  vte$VTE_PC_VERSION >= $VTE_REQUIRED
  gconf-2.0 >= $GCONF_REQUIRED])

# ***********
# Build tools
# ***********

AC_ARG_VAR([GLIB_GENMARSHAL],[the glib-genmarschal programme])
AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal],[])
if test -z "$GLIB_GENMARSHAL"; then
  AC_MSG_ERROR([glib-genmarshal not found])
fi

AC_ARG_VAR([GLIB_MKENUMS],[the glib-mkenums programme])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums],[])
if test -z "$GLIB_MKENUMS"; then
  AC_MSG_ERROR([glib-mkenums not found])
fi

AC_ARG_VAR([GDBUS_CODEGEN],[the gdbus-codegen programme])
AC_PATH_PROG([GDBUS_CODEGEN],[gdbus-codegen],[])
if test -z "$GDBUS_CODEGEN"; then
  AC_MSG_ERROR([gdbus-codegen not found])
fi

AC_ARG_VAR([GTK_BUILDER_CONVERT],[the gtk-builder-convert programme])
AC_PATH_PROG([GTK_BUILDER_CONVERT],[gtk-builder-convert],[])
if test -z "$GTK_BUILDER_CONVERT"; then
  AC_MSG_ERROR([gtk-builder-convert not found])
fi

# ***********
# Compilation
# ***********

CC_CHECK_CFLAGS_APPEND([ \
  -Wall -Wextra \
  -Wsign-compare -Werror-implicit-function-declaration \
  -Wpointer-arith -Wwrite-strings -Wstrict-prototypes \
  -Waggregate-return -Wcast-align -Wimplicit -Wuninitialised \
  -Wmissing-prototypes -Wmissing-declarations -Wnested-externs \
  -Wpacked -Wmissing-format-attribute -Wshadow \
  -Wstrict-aliasing=2 -Winit-self -Wunsafe-loop-optimizations \
  -Wdeclaration-after-statement -Wold-style-definition \
  -Wno-missing-field-initializers -Wno-unused-parameter \
  -fno-common -Wno-switch-enum])

# *************
# Documentation
# *************

GNOME_DOC_INIT

# ****************************

AC_SUBST([TERMINAL_MAJOR_VERSION],[gt_version_major])
AC_SUBST([TERMINAL_MINOR_VERSION],[gt_version_minor])
AC_SUBST([TERMINAL_MICRO_VERSION],[gt_version_micro])
AC_SUBST([TERMINAL_API_VERSION],[gt_api_version])

# ****************************

AC_CONFIG_FILES([
Makefile
gnome-terminal.desktop.in
src/Makefile
src/terminal-version.h
help/Makefile
help/bg/Makefile
help/ro/Makefile
help/ja/Makefile
help/zh_TW/Makefile
po/Makefile.in
])
AC_OUTPUT

# ****************************

echo "
gnome-terminal-$VERSION:

	prefix:                 ${prefix}
        DBus service dir:       ${dbusservicedir}
	source code location:	${srcdir}
	compiler:		${CC}

        using smclient:         ${with_smclient}
"
