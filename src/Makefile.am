
EFENCE=

SUBDIRS=eel

INCLUDES=@TERM_CFLAGS@								\
	-DTERM_LOCALEDIR=\"$(datadir)/locale\" -DEXECUTABLE_NAME=\"gnome-terminal\"	\
	-DTERM_GLADE_DIR=\"$(gladedir)\"						\
	-DTERM_GLADE_FILE=\"gnome-terminal.glade2\"					\
	-DTERM_DATADIR=\"$(datadir)\"						\
	-DTERM_LIBDIR=\"$(libdir)\"							\
	-DTERM_PREFIX=\"$(prefix)\"							\
	-DTERM_SYSCONFDIR=\"$(sysconfdir)\"						\
	-DTERM_PKGDATADIR=\"$(pkgdatadir)\"						\
	-DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DG_DISABLE_DEPRECATED 	\
	-DGNOME_DISABLE_DEPRECATED

bin_PROGRAMS=gnome-terminal

EGGFILES=			\
	eggcellrendererkeys.h 	\
	eggcellrendererkeys.c

if TERMINAL_WIDGET_VTE
terminal_widget_impl=terminal-widget-vte.c
else
terminal_widget_impl=terminal-widget-zvt.c
endif

## Terminal
gnome_terminal_SOURCES=				\
	profile-editor.c			\
	profile-editor.h			\
	terminal.c				\
	terminal.h				\
	terminal-accels.c			\
	terminal-accels.h			\
	terminal-intl.h				\
	terminal-profile.c			\
	terminal-profile.h			\
	terminal-screen.c			\
	terminal-screen.h			\
	terminal-widget.h			\
	$(terminal_widget_impl)			\
	terminal-window.c			\
	terminal-window.h			\
	x-font-selector.c			\
	x-font-selector.h			\
	$(EGGFILES)

gnome_terminal_LDADD= $(EFENCE) @TERM_LIBS@ eel/libeelcnp.la

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-terminal.glade2

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = profterm.schemas

icondir = $(datadir)/pixmaps
icon_DATA = gnome-terminal.png

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST =				\
	$(icon_DATA)			\
	$(glade_DATA)			\
	$(schema_DATA)			\
	update-from-egg.sh


EGGDIR=$(srcdir)/../../libegg/libegg

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" update-from-egg.sh
