
INCLUDES=@PROFTERM_CFLAGS@ -DPROFTERM_PKGDATADIR=\"$(datadir)/profterm\"	  \
	-DPROFTERM_LOCALEDIR=\"$(datadir)/locale\" -DEXECUTABLE_NAME=\"profterm\" \
	-DPROFTERM_GLADE_DIR=\"$(gladedir)\"					  \
	-DPROFTERM_GLADE_FILE=\"profterm.glade2\"

bin_PROGRAMS=profterm

## Terminal
profterm_SOURCES=				\
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
	terminal-window.h

profterm_LDADD= @PROFTERM_LIBS@


gladedir   = $(datadir)/profterm/glade
glade_DATA = profterm.glade2
glade_src = profterm.glade
glade_msgs = profterm.glade.h

profterm.glade2: profterm.glade $(LIBGLADE_CONVERT)
	$(LIBGLADE_CONVERT) $(srcdir)/profterm.glade > profterm.glade.tmp \
	&& mv profterm.glade.tmp profterm.glade2

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = profterm.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST =				\
	$(glade_DATA)			\
	$(glade_src)			\
	$(glade_msgs)			\
	$(schema_DATA)

