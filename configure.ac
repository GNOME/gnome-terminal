m4_define([gt_version_major],[3])
m4_define([gt_version_minor],[8])
m4_define([gt_version_micro],[0])
m4_define([gt_version_extra],[.1])
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

GLIB_REQUIRED=2.32.0
GIO_REQUIRED=2.33.2
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=0.1.0
DCONF_REQUIRED=0.12.0

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
       GTK_REQUIRED=3.6.0
       VTE_PC_VERSION=-2.90
       VTE_REQUIRED=0.34.0
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
   gio-2.0 >= $GIO_REQUIRED
   gtk+-$GTK_API_VERSION >= $GTK_REQUIRED
   gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED
   dconf >= $DCONF_REQUIRED
   uuid
   $PLATFORM_DEPS])

# DConf broke API without changing the pkgconfig name
AC_MSG_CHECKING([old DConf])
PKG_CHECK_EXISTS([dconf >= 0.14],[have_old_dconf=no],[have_old_dconf=yes])
AC_MSG_RESULT([$have_old_dconf])

if test "$have_old_dconf" = "yes"; then
  AC_DEFINE([HAVE_DCONF_1_2],[1],[Define to 1 if DConf is version 1.2])
fi

# ****
# DBus
# ****

AC_ARG_WITH([dbus-service-dir],
  [AS_HELP_STRING([--with-dbus-service-dir=PATH],[dbus service file directory])],
  [dbusservicedir="$withval"],
  [dbusservicedir='${datadir}/dbus-1/services'])
AC_SUBST([dbusservicedir])

# *********
# GSettings
# *********

GLIB_GSETTINGS

# *************************
# GConf-GSettings Migration
# *************************

GCONF_REQUIRED=2.31.3

AC_MSG_CHECKING([whether to build the GConf to GSettings migrator])
AC_ARG_ENABLE([migration],
        [AS_HELP_STRING([--disable-migration],[Disable prefs migration from GConf to GSettings])],
        [],[enable_migration=yes])
AC_MSG_RESULT([$enable_migration])

if test "$enable_migration" = "yes"; then
 PKG_CHECK_MODULES([MIGRATOR],[
   gio-2.0 >= $GIO_REQUIRED
   vte$VTE_PC_VERSION >= $VTE_REQUIRED
   gconf-2.0 >= $GCONF_REQUIRED
   dconf >= $DCONF_REQUIRED
   uuid])
  AC_DEFINE([ENABLE_MIGRATION],[1],[Define to 1 to enable prefs migration from GConf to GSettings])
fi

AM_CONDITIONAL([ENABLE_MIGRATION],[test "$enable_migration" = "yes"])

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

AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[the glib-compile-resources programme])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

AC_ARG_VAR([GDBUS_CODEGEN],[the gdbus-codegen programme])
AC_PATH_PROG([GDBUS_CODEGEN],[gdbus-codegen],[])
if test -z "$GDBUS_CODEGEN"; then
  AC_MSG_ERROR([gdbus-codegen not found])
fi

AC_ARG_VAR([XMLLINT],[the xmllint programme])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

AC_ARG_VAR([DESKTOP_FILE_VALIDATE],[the desktop-file-validate programme])
AC_PATH_PROG([DESKTOP_FILE_VALIDATE],[desktop-file-validate],[])
if test -z "$DESKTOP_FILE_VALIDATE"; then
  AC_MSG_ERROR([desktop-file-validate not found])
fi

# ******************
# Nautilus extension
# ******************

AC_ARG_WITH([nautilus-extension],
  [AS_HELP_STRING([--without-nautilus-extension],[Disable nautilus extension])],
  [],
  [with_nautilus_extension=no])

if test "$with_nautilus_extension" = "yes"; then
  NAUTILUS_REQUIRED=3.0.0

  PKG_CHECK_MODULES([NAUTILUS],[
    libnautilus-extension >= $NAUTILUS_REQUIRED
    gio-2.0
    gio-unix-2.0])

  AC_ARG_WITH([nautilus-dir],
    [AS_HELP_STRING([--with-nautilus-dir=PATH],[Nautilus extensions directory])],
    [nautilusextensiondir="$withval"],
    [nautilusextensiondir='${libdir}/nautilus/extensions-3.0'])
  AC_SUBST([nautilusextensiondir])

fi

AM_CONDITIONAL([WITH_NAUTILUS_EXTENSION],[test "$with_nautilus_extension" = "yes"])

# ***********
# Compilation
# ***********

CC_CHECK_FLAGS_APPEND([AM_CFLAGS],[CFLAGS],[ \
  -pipe \
  -Waggregate-return \
  -Wall \
  -Wcast-align \
  -Wendif-labels \
  -Werror=declaration-after-statement \
  -Werror=format=2 \
  -Werror=format-nonliteral \
  -Werror=format-security \
  -Werror=implicit-function-declaration \
  -Werror=init-self \
  -Werror=missing-include-dirs \
  -Werror=missing-prototypes \
  -Werror=pointer-arith \
  -Wextra \
  -Wfloat-equal \
  -Wformat-nonliteral \
  -Wimplicit \
  -Wlogical-op \
  -Wmissing-declarations \
  -Wmissing-include-dirs \
  -Wmissing-format-attribute \
  -Wmissing-noreturn \
  -Wnested-externs \
  -Wno-missing-field-initializers \
  -Wno-switch-enum \
  -Wno-unused-parameter \
  -Wold-style-definition \
  -Wpacked \
  -Wshadow \
  -Wsign-compare \
  -Wstrict-aliasing=2 \
  -Wstrict-prototypes \
  -Wundef \
  -Wuninitialized \
  -Wunsafe-loop-optimizations \
  -Wwrite-strings \
  -fno-common \
  -fdiagnostics-show-option \
  -fno-strict-aliasing \
  -fvisibility=hidden \
  -fstack-protector \
])

# *************
# Documentation
# *************

YELP_HELP_INIT

# ****************************

AC_SUBST([TERMINAL_MAJOR_VERSION],[gt_version_major])
AC_SUBST([TERMINAL_MINOR_VERSION],[gt_version_minor])
AC_SUBST([TERMINAL_MICRO_VERSION],[gt_version_micro])
AC_SUBST([TERMINAL_API_VERSION],[gt_api_version])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

# ****************************

AC_CONFIG_FILES([
Makefile
gnome-terminal.desktop.in
src/Makefile
src/terminal-version.h
help/Makefile
po/Makefile.in
])
AC_OUTPUT

# ****************************

echo "
gnome-terminal-$VERSION:

      prefix:                 ${prefix}
      source code location:   ${srcdir}
      compiler:               ${CC}
      DBus service dir:       ${dbusservicedir}
      Prefs migration:        ${enable_migration}
      Nautilus extension:     ${with_nautilus_extension}
      Nautilus extension dir: ${nautilusextensiondir}
"
