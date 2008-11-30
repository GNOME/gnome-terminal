SUBDIRS = src po help

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-schemas-install

desktopdir = $(datadir)/applications
desktop_in_files = gnome-terminal.desktop.in.in
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

# Build ChangeLog from GIT or SVN history
# (SVN r2446 was the first ChangeLog-less checkin).
# Only build this when in a git or svn checkout.

ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat > $@; \
	elif test -f $(top_srcdir)/.svn/entries; then \
		svn log -v --xml -r HEAD:2446 $(SVN_ROOT)/$(SVN_MODULE)/$(SVN_BRANCH) | \
			xsltproc --stringparam strip-prefix "$(SVN_MODULE)/$(SVN_BRANCH)" \
				 --stringparam include-rev "yes" $(top_srcdir)/svn2cl.xsl - > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
