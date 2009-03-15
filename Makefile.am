SUBDIRS = src po help

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-schemas-install

desktopdir = $(datadir)/applications
desktop_in_files = @PACKAGE@.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 			\
	ChangeLog.pre-2-23	\
	po/ChangeLog.pre-2-23	\
	help/ChangeLog.pre-2-23	\
	ChangeLog.README	\
	HACKING			\
	$(desktop_DATA)		\
	gnome-doc-utils.make	\
	xmldocs.make		\
	omf.make

DISTCLEANFILES =		\
	gnome-doc-utils.make	\
	$(desktop_DATA)

# hack from sound-juicer to get around scrollkeeper's leaving a big
# mess behind.
distuninstallcheck_listfiles = 	\
        find -regex '.*/var/scrollkeeper/.*' -prune -or -type f -print

# Build ChangeLog from GIT  history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
