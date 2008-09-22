m4_define([gt_version_major],[2])
m4_define([gt_version_minor],[24])
m4_define([gt_version_micro],[0])
m4_define([gt_version_extra],[])
m4_define([gt_version],[gt_version_major().gt_version_minor().gt_version_micro()gt_version_extra])

# This can be either "trunk" or "branches/gnome-x-y"
m4_define([gt_branch],["branches/gnome-2-24"])

AC_INIT([GNOME Terminal],[gt_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-terminal],[gnome-terminal])
AC_CONFIG_SRCDIR([src/terminal.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2])

GETTEXT_PACKAGE=gnome-terminal
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

AC_C_BIGENDIAN

AM_MAINTAINER_MODE

PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.40.0])

GNOME_COMMON_INIT
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

AM_GLIB_GNU_GETTEXT

GLIB_REQUIRED=2.16.0
GTK_REQUIRED=2.13.6
GCONF_REQUIRED=2.14.0
STARTUP_NOTIFICATION_REQUIRED=0.8
VTE_REQUIRED=0.17.0
LIBGNOME_REQUIRED=2.14.0

## here we get the flags we'll actually use
PKG_CHECK_MODULES(TERM, dnl
 glib-2.0 >= $GLIB_REQUIRED dnl
 gtk+-2.0 >= $GTK_REQUIRED dnl
 gconf-2.0 >= $GCONF_REQUIRED dnl
 gio-2.0 dnl
 libgnomeui-2.0 dnl
 libgnome-2.0 >= $LIBGNOME_REQUIRED dnl
 libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_REQUIRED dnl
 vte >= $VTE_REQUIRED)

AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

if $PKG_CONFIG 'xft >= 2.0.0 pangoxft >= 1.1.1'; then
   using_xft2=yes
   CPPFLAGS="`$PKG_CONFIG --cflags xft pangoxft` $CPPFLAGS"
   LIBS="`$PKG_CONFIG --libs xft pangoxft` `$PKG_CONFIG --variable=xrenderlibs xft` $LIBS"
else
   using_xft2=no
fi

GLIB_GENMARSHAL="$($PKG_CONFIG --variable=glib_genmarshal glib-2.0)"
AC_SUBST([GLIB_GENMARSHAL])
GLIB_MKENUMS="$($PKG_CONFIG --variable=glib_mkenums glib-2.0)"
AC_SUBST([GLIB_MKENUMS])

AC_PATH_PROG([GTK_BUILDER_CONVERT],[gtk-builder-convert],[false])
if test "$GTK_BUILDER_CONVERT" = "false"; then
  AC_MSG_ERROR([gtk-builder-convert not found])
fi

GNOME_DOC_INIT

# ****************************
# SVN for ChangeLog generation
# ****************************

AC_SUBST([SVN_ROOT],[http://svn.gnome.org/svn])
AC_SUBST([SVN_MODULE],[gnome-terminal])
AC_SUBST([SVN_BRANCH],[gt_branch])

# ****************************

AC_CONFIG_FILES([
Makefile
gnome-terminal.desktop.in
src/Makefile
src/skey/Makefile
help/Makefile
help/bg/Makefile
help/ro/Makefile
help/ja/Makefile
help/zh_CN/Makefile
help/zh_TW/Makefile
po/Makefile.in
])
AC_OUTPUT

# ****************************

if test x$using_xft2 = xno; then
        echo "*** WARNING WARNING WARNING WARNING WARNING"
        echo "*** Building with VTE widget but not Xft2"
        echo "*** This won't be as optimal as fontconfig/Xft2"
fi

echo "
gnome-terminal-$VERSION:

	prefix:                 ${prefix}
	source code location:	${srcdir}
	compiler:		${CC}

        using fontconfig/Xft2:  ${using_xft2}
"
