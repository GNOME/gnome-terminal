SUBDIRS = data src po help

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
desktop_in_files = org.gnome.Terminal.desktop.in.in
nodist_desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

$(nodist_desktop_DATA): $(desktop_in_files:.desktop.in.in=.desktop.in)
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

DESKTOP_FILE_VALIDATE_FLAGS =
desktop-file-validate: $(nodist_desktop_DATA)
	for f in $^; do \
		$(DESKTOP_FILE_VALIDATE) $(DESKTOP_FILE_VALIDATE_FLAGS) $$f; \
	done

appdata_in_files = \
	org.gnome.Terminal.appdata.xml.in \
	org.gnome.Terminal.Nautilus.metainfo.xml.in \
	$(NULL)

appdatadir = $(datadir)/metainfo
nodist_appdata_DATA = \
	org.gnome.Terminal.appdata.xml \
	$(NULL)

if WITH_NAUTILUS_EXTENSION
nodist_appdata_DATA += org.gnome.Terminal.Nautilus.metainfo.xml
endif

$(nodist_appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

check-local: desktop-file-validate

EXTRA_DIST = 			\
	COPYING                 \
	COPYING.GFDL            \
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
	$(AM_V_GEN) if test -e $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
