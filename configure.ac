m4_define([terminal_version_major],[3])
m4_define([terminal_version_minor],[36])
m4_define([terminal_version_micro],[2])
m4_define([terminal_version_extra],[])
m4_define([terminal_version],[terminal_version_major().terminal_version_minor().terminal_version_micro()terminal_version_extra])

m4_define([terminal_api_version],[0])

AC_INIT([GNOME Terminal],[terminal_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-terminal],[gnome-terminal])
AC_CONFIG_SRCDIR([src/terminal.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2])

AC_USE_SYSTEM_EXTENSIONS

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

GETTEXT_PACKAGE=gnome-terminal
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

PKG_PROG_PKG_CONFIG
AC_PROG_MKDIR_P
AC_PROG_CC_C99
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

AX_PTHREAD([],[AC_MSG_ERROR([pthreads are required])])

GLIB_REQUIRED=2.50.0
GLIB_MIN_REQUIRED=2.50
GLIB_MAX_ALLOWED=2.50

GIO_REQUIRED=2.34.0
GIO_UNIX_REQUIRED=2.34.0
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=0.1.0
DCONF_REQUIRED=0.14.0
PCRE2_REQUIRED=10.00

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
       GTK_REQUIRED=3.22.27
       GTK_MIN_REQUIRED=3.18
       GTK_MAX_ALLOWED=3.22
       VTE_API_VERSION=2.91
       VTE_REQUIRED=0.60.2
       ;;
esac

#

PLATFORM_DEPS=
for target in $($PKG_CONFIG --variable targets gdk-$GTK_API_VERSION); do
  case "$target" in
    x11) PLATFORM_DEPS="$PLATFORM_DEPS x11" ;;
    broadway) ;;
    wayland) ;;
    win32|quartz) AC_MSG_WARN([unsupported GDK backend $target]) ;;
    *) AC_MSG_WARN([unknown GDK backend $target]) ;;
  esac
done

# pkg-config checks

PKG_CHECK_MODULES([TERM],
  [vte-$VTE_API_VERSION >= $VTE_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gio-2.0 >= $GIO_REQUIRED
   gio-unix-2.0 >= $GIO_UNIX_REQUIRED
   gtk+-$GTK_API_VERSION >= $GTK_REQUIRED
   gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED
   dconf >= $DCONF_REQUIRED
   uuid
   libpcre2-8 >= $PCRE2_REQUIRED
   $PLATFORM_DEPS])

# ****
# DBus
# ****

AC_ARG_WITH([dbus-interface-dir],
  [AS_HELP_STRING([--with-dbus-interface-dir=PATH],[dbus interace file directory])],
  [dbusinterfacedir="$withval"],
  [dbusinterfacedir='${datadir}/dbus-1/interfaces'])
AC_SUBST([dbusinterfacedir])

AC_ARG_WITH([dbus-service-dir],
  [AS_HELP_STRING([--with-dbus-service-dir=PATH],[dbus service file directory])],
  [dbusservicedir="$withval"],
  [dbusservicedir='${datadir}/dbus-1/services'])
AC_SUBST([dbusservicedir])

# *********
# GSettings
# *********

GLIB_GSETTINGS

# ***********
# Build tools
# ***********

AC_ARG_VAR([GLIB_GENMARSHAL],[the glib-genmarschal programme])
if test -z "$GLIB_GENMARSHAL"; then
  AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal],[])
  if test -z "$GLIB_GENMARSHAL"; then
    AC_MSG_ERROR([glib-genmarshal not found])
  fi
fi

AC_ARG_VAR([GLIB_MKENUMS],[the glib-mkenums programme])
if test -z "$GLIB_MKENUMS"; then
  AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums],[])
  if test -z "$GLIB_MKENUMS"; then
    AC_MSG_ERROR([glib-mkenums not found])
  fi
fi

AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[the glib-compile-resources programme])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
  if test -z "$GLIB_COMPILE_RESOURCES"; then
    AC_MSG_ERROR([glib-compile-resources not found])
  fi
fi

AC_ARG_VAR([GDBUS_CODEGEN],[the gdbus-codegen programme])
if test -z "$GDBUS_CODEGEN"; then
  AC_PATH_PROG([GDBUS_CODEGEN],[gdbus-codegen],[])
  if test -z "$GDBUS_CODEGEN"; then
    AC_MSG_ERROR([gdbus-codegen not found])
  fi
fi

AC_ARG_VAR([XMLLINT],[the xmllint programme])
if test -z "$XMLLINT"; then
  AC_PATH_PROG([XMLLINT],[xmllint],[])
  if test -z "$XMLLINT"; then
    AC_MSG_ERROR([xmllint not found])
  fi
fi

AC_ARG_VAR([DESKTOP_FILE_VALIDATE],[the desktop-file-validate programme])
if test -z "$DESKTOP_FILE_VALIDATE"; then
  AC_PATH_PROG([DESKTOP_FILE_VALIDATE],[desktop-file-validate],[])
  if test -z "$DESKTOP_FILE_VALIDATE"; then
    AC_MSG_ERROR([desktop-file-validate not found])
  fi
fi

# ***************************
# GNOME Shell search provider
# ***************************

AC_MSG_CHECKING([whether to build the gnome-shell search provider])
AC_ARG_ENABLE([search-provider],
  [AS_HELP_STRING([--disable-search-provider],[Disable gnome-shell search provider])],
  [],[enable_search_provider=yes])
AC_MSG_RESULT([$enable_search_provider])

if test "$enable_search_provider" = "yes"; then
  AC_CHECK_FILE([$(eval echo $(eval echo $(eval echo ${dbusinterfacedir})))/org.gnome.ShellSearchProvider2.xml],[],
    [AC_MSG_ERROR([gnome-shell search provider requested but interface definition file not found])])

  AC_DEFINE([ENABLE_SEARCH_PROVIDER],[1],[Define to enable gnome-shell search provider])
fi

AM_CONDITIONAL([ENABLE_SEARCH_PROVIDER],[test "$enable_search_provider" = "yes"])

# ******************
# Nautilus extension
# ******************

AC_ARG_WITH([nautilus-extension],
  [AS_HELP_STRING([--without-nautilus-extension],[Disable nautilus extension])],
  [],
  [with_nautilus_extension=yes])

if test "$with_nautilus_extension" = "yes"; then
  NAUTILUS_REQUIRED=3.28.0

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

WARN_CFLAGS=
CC_CHECK_FLAGS_APPEND([WARN_CFLAGS],[CFLAGS],[ \
  -pipe \
  -Waggregate-return \
  -Wall \
  -Wcast-align \
  -Wendif-labels \
  "-Werror=format=2 -Werror=format-nonliteral -Werror=format-security" \
  -Werror=implicit-function-declaration \
  -Werror=init-self \
  -Werror=missing-include-dirs \
  -Werror=missing-prototypes \
  -Werror=pointer-arith \
  -Wextra \
  -Wfloat-equal \
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
  -Wvla \
  -Wwrite-strings \
  -Wformat-signedness \
  -fno-common \
  -fdiagnostics-show-option \
  -fno-strict-aliasing \
  -fvisibility=hidden \
  -fstack-protector \
  -fstack-protector-strong \
])

# *********
# Debugging
# *********

AC_MSG_CHECKING([whether to enable special debug support])
AC_ARG_ENABLE([debug],
        [AS_HELP_STRING([--enable-debug],[Enable special debug support])],
        [],[enable_debug=no])
AC_MSG_RESULT([$enable_debug])

if test "$enable_debug" = "yes"; then
   AC_DEFINE([ENABLE_DEBUG],[1],[Define to 1 to enable special debug support])
fi

# *************
# Documentation
# *************

YELP_HELP_INIT([no-lc-media-links no-lc-dist])

# ****************************

AC_SUBST([TERMINAL_MAJOR_VERSION],[terminal_version_major])
AC_SUBST([TERMINAL_MINOR_VERSION],[terminal_version_minor])
AC_SUBST([TERMINAL_MICRO_VERSION],[terminal_version_micro])
AC_SUBST([TERMINAL_API_VERSION],[terminal_api_version])
AC_SUBST([GTK_API_VERSION])
AC_SUBST([WARN_CFLAGS])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])
AC_DEFINE_UNQUOTED([GLIB_VERSION_MIN_REQUIRED],[G_ENCODE_VERSION($(echo $GLIB_MIN_REQUIRED | tr . ,))],[Define the min required glib version])
AC_DEFINE_UNQUOTED([GLIB_VERSION_MAX_ALLOWED],[G_ENCODE_VERSION($(echo $GLIB_MAX_ALLOWED | tr . ,))],[Define the max allowed glib version])
AC_DEFINE_UNQUOTED([GDK_VERSION_MIN_REQUIRED],[G_ENCODE_VERSION($(echo $GTK_MIN_REQUIRED | tr . ,))],[Define the min required gtk version])
AC_DEFINE_UNQUOTED([GDK_VERSION_MAX_ALLOWED],[G_ENCODE_VERSION($(echo $GTK_MAX_ALLOWED | tr . ,))],[Define the max allowed gtk version])

# ****************************

AC_CONFIG_FILES([
Makefile
org.gnome.Terminal.desktop.in
data/Makefile
data/icons/Makefile
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
      gterminal:              ${enable_gterminal}
      DBus interface dir:     ${dbusinterfacedir}
      DBus service dir:       ${dbusservicedir}
      Debug:                  ${enable_debug}
      Search provider:        ${enable_search_provider}
      Nautilus extension:     ${with_nautilus_extension}
      Nautilus extension dir: ${nautilusextensiondir}
"
