
EFENCE=

SUBDIRS=skey

INCLUDES=@TERM_CFLAGS@ \
	-DTERM_LOCALEDIR=\"$(datadir)/locale\" -DEXECUTABLE_NAME=\"gnome-terminal\" \
	-DTERM_GLADE_DIR=\"$(gladedir)\" \
	-DTERM_GLADE_FILE=\"gnome-terminal.glade2\" \
	-DTERM_DATADIR=\"$(datadir)\" \
	-DTERM_LIBDIR=\"$(libdir)\" \
	-DTERM_PREFIX=\"$(prefix)\" \
	-DTERM_SYSCONFDIR=\"$(sysconfdir)\" \
	-DTERM_PKGDATADIR=\"$(pkgdatadir)\" \
	-DGDK_DISABLE_DEPRECATED -DG_DISABLE_DEPRECATED	\
	-DSN_API_NOT_YET_FROZEN
#	-DGTK_DISABLE_DEPRECATED 
#	-DGNOME_DISABLE_DEPRECATED 

bin_PROGRAMS=gnome-terminal

EGGFILES=			\
	eggaccelerators.c	\
	eggaccelerators.h	\
	eggcellrendererkeys.h 	\
	eggcellrendererkeys.c

## Terminal
gnome_terminal_SOURCES=				\
	encoding.c				\
	encoding.h				\
	profile-editor.c			\
	profile-editor.h			\
	skey-popup.c				\
	skey-popup.h				\
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
	terminal-widget-vte.c			\
	terminal-window.c			\
	terminal-window.h			\
	terminal-notebook.c			\
	terminal-notebook.h			\
	simple-x-font-selector.c		\
	simple-x-font-selector.h		\
	$(EGGFILES)

gnome_terminal_LDADD= $(EFENCE) @TERM_LIBS@ skey/libskey.la

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-terminal.glade2

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gnome-terminal.schemas.in
schema_DATA = gnome-terminal.schemas

icondir = $(datadir)/pixmaps
icon_DATA = gnome-terminal.png

CLEANFILES = gnome-terminal.schemas

EXTRA_DIST =				\
	$(icon_DATA)			\
	$(glade_DATA)			\
	$(schema_DATA)			\
	$(schema_in_files)		\
	$(server_DATA)			\
	update-from-egg.sh		\
	terminal-widget-vte.c


EGGDIR=$(srcdir)/../../libegg/libegg

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh

serverdir = $(libdir)/bonobo/servers
server_DATA = gnome-terminal.server

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/src/$(schema_DATA)
endif
