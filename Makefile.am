
SUBDIRS=src po

Desktopdir = $(datadir)/applications
Desktop_in_files = gnome-terminal.desktop.in
Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

Pixmapdir = $(datadir)/pixmaps

Pixmap_DATA = gnome-terminal.png

EXTRA_DIST = \
	$(Desktop_in_files) \
	$(Desktop_DATA)	\
	$(Pixmap_DATA)