
SUBDIRS=src po help

desktopdir = $(datadir)/applications
desktop_in_files = gnome-terminal.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
applicationsdir = $(datadir)/application-registry
applications_DATA = gnome-terminal.applications
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 			\
	HACKING			\
	$(desktop_in_files) 	\
	$(desktop_DATA)		\
	$(applications_DATA)    \
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	xmldocs.make		\
	omf.make

DISTCLEANFILES =		\
	intltool-extract	\
	intltool-merge		\
	intltool-update		\
	$(desktop_DATA)		

# hack from sound-juicer to get around scrollkeeper's leaving a big
# mess behind.
distuninstallcheck_listfiles = 	\
        find -path ./var/scrollkeeper/\* -prune -or -type f -print
