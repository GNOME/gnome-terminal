
SUBDIRS=src po help

desktopdir = $(datadir)/applications
desktop_in_files = gnome-terminal.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 			\
	$(desktop_in_files) 	\
	$(desktop_DATA)		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in

