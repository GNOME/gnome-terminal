
SUBDIRS=src po

desktopdir = $(datadir)/applications
desktop_in_files = gnome-terminal.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(Desktop_in_files) \
	$(Desktop_DATA)	\
	$(Pixmap_DATA)