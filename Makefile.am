SUBDIRS = src po help

NULL =

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-debug \
	--disable-scrollkeeper \
	--disable-schemas-install \
	--disable-silent-rules \
	--disable-search-provider
	--without-nautilus-extension \
	$(NULL)

desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in.in
nodist_desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

DESKTOP_FILE_VALIDATE_FLAGS =
desktop-file-validate: $(nodist_desktop_DATA)
	for f in $^; do \
		$(DESKTOP_FILE_VALIDATE) $(DESKTOP_FILE_VALIDATE_FLAGS) $$f; \
	done

appdatadir = $(datadir)/appdata
appdata_in_files = gnome-terminal.appdata.xml.in
nodist_appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

APPDATA_VALIDATE_FLAGS = --nonet
appdata-validate: $(nodist_appdata_DATA)
	for f in $^; do \
		$(APPDATA_VALIDATE) $(APPDATA_VALIDATE_FLAGS) $$f; \
	done

check-local: desktop-file-validate

EXTRA_DIST = 			\
	$(appdata_in_files)	\
	ChangeLog.pre-2-23	\
	ChangeLog.README	\
	HACKING			\
	$(NULL)

CLEANFILES = \
	$(nodist_appdata_DATA) \
	$(nodist_desktop_DATA) \
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

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
