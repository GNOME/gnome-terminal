
SUBDIRS=eel

INCLUDES=@TERM_CFLAGS@								\
	-DTERM_LOCALEDIR=\"$(datadir)/locale\" -DEXECUTABLE_NAME=\"gnome-terminal\"	\
	-DTERM_GLADE_DIR=\"$(gladedir)\"						\
	-DTERM_GLADE_FILE=\"profterm.glade2\"					\
	-DTERM_DATADIR=\"$(datadir)\"						\
	-DTERM_LIBDIR=\"$(libdir)\"							\
	-DTERM_PREFIX=\"$(prefix)\"							\
	-DTERM_SYSCONFDIR=\"$(sysconfdir)\"						\
	-DTERM_PKGDATADIR=\"$(pkgdatadir)\"

bin_PROGRAMS=gnome-terminal

## Terminal
gnome_terminal_SOURCES=				\
	profile-editor.c			\
	profile-editor.h			\
	terminal.c				\
	terminal.h				\
	terminal-intl.h				\
	terminal-profile.c			\
	terminal-profile.h			\
	terminal-screen.c			\
	terminal-screen.h			\
	terminal-window.c			\
	terminal-window.h			\
	eggcellrendererkeys.c			\
	eggcellrendererkeys.h

gnome_terminal_LDADD= @TERM_LIBS@ eel/libeelcnp.la

gladedir   = $(pkgdatadir)/glade
glade_DATA = profterm.glade2
glade_src = profterm.glade
glade_msgs = profterm.glade.h

profterm.glade2: profterm.glade $(LIBGLADE_CONVERT)
	$(LIBGLADE_CONVERT) $(srcdir)/profterm.glade > profterm.glade.tmp \
	&& mv profterm.glade.tmp profterm.glade2

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = profterm.schemas

icondir = $(datadir)/pixmaps
icon_DATA = gnome-terminal.png

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST =				\
	$(icon_DATA)			\
	$(glade_DATA)			\
	$(glade_src)			\
	$(glade_msgs)			\
	$(schema_DATA)




