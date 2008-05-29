NULL =

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
	terminal-screen-container.c		\
	terminal-screen-container.h		\
	terminal-tabs-menu.c			\
	terminal-tabs-menu.h			\
	terminal-type-builtins.c		\
	terminal-type-builtins.h		\
	terminal-widget.h			\
	terminal-widget-vte.c			\
	terminal-window.c			\
	terminal-window.h			\
	$(NULL)

gnome_terminal_LDADD= $(EFENCE) @TERM_LIBS@ skey/libskey.la

BUILT_SOURCES = \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	$(NULL)
	
TYPES_H_FILES = \
	terminal-profile.h \
	$(NULL)

terminal-type-builtins.h: stamp-terminal-type-builtins.h
	@true

stamp-terminal-type-builtins.h: terminal-type-builtins.h.template $(TYPES_H_FILES)
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-ttbh \
	&& (cmp -s xgen-ttbh terminal-type-builtins.h || cp xgen-ttbh terminal-type-builtins.h ) \
	&& rm -f xgen-ttbh \
	&& echo timestamp > $(@F)

terminal-type-builtins.c: terminal-type-builtins.c.template $(TYPES_H_FILES)
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-ttbc \
	&& (cmp -s xgen-ttbc terminal-type-builtins.c || cp xgen-ttbc terminal-type-builtins.c ) \
	&& rm -f xgen-ttbc

gladedir   = $(pkgdatadir)/glade
glade_DATA = gnome-terminal.glade2

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_in_files = gnome-terminal.schemas.in
schema_DATA = gnome-terminal.schemas

uidir = $(pkgdatadir)
ui_DATA = \
	terminal.ui

CLEANFILES = gnome-terminal.schemas

EXTRA_DIST =				\
	$(icon_DATA)			\
	$(glade_DATA)			\
	$(schema_in_files)		\
	$(server_DATA)			\
	$(ui_DATA)			\
	terminal-widget-vte.c


serverdir = $(libdir)/bonobo/servers
server_DATA = gnome-terminal.server

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/src/$(schema_DATA)
endif
