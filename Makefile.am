SUBDIRS = src po help

NULL =

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-scrollkeeper \
	--disable-schemas-install \
	--disable-silent-rules \
	--with-gtk=3.0

desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 			\
	ChangeLog.pre-2-23	\
	ChangeLog.README	\
	HACKING			\
	$(NULL)

CLEANFILES = \
	$(desktop_DATA)	\
	$(NULL)

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(NULL)

# hack from sound-juicer to get around scrollkeeper's leaving a big
# mess behind.
distuninstallcheck_listfiles = 	\
        find -regex '.*/var/scrollkeeper/.*' -prune -or -type f -print

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
