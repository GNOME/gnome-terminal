NULL =

SUBDIRS = skey

bin_PROGRAMS = gnome-terminal

gnome_terminal_SOURCES= \
	encoding.c \
	encoding.h \
	profile-editor.c \
	profile-editor.h \
	skey-popup.c \
	skey-popup.h \
	terminal.c \
	terminal.h \
	terminal-accels.c \
	terminal-accels.h \
	terminal-app.c \
	terminal-app.h \
	terminal-intl.h \
	terminal-profile.c \
	terminal-profile.h \
	terminal-screen.c \
	terminal-screen.h \
	terminal-screen-container.c \
	terminal-screen-container.h \
	terminal-tabs-menu.c \
	terminal-tabs-menu.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	terminal-util.c \
	terminal-util.h \
	terminal-window.c \
	terminal-window.h \
	$(NULL)

gnome_terminal_CPPFLAGS = \
	-DEXECUTABLE_NAME=\"gnome-terminal\" \
	-DTERM_LOCALEDIR=\"$(datadir)/locale\" \
	-DTERM_LIBDIR=\"$(pkglibdir)\" \
	-DTERM_PREFIX=\"$(prefix)\" \
	-DTERM_SYSCONFDIR=\"$(sysconfdir)\" \
	-DTERM_PKGDATADIR=\"$(pkgdatadir)\" \
	-DSN_API_NOT_YET_FROZEN \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

gnome_terminal_CFLAGS = \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

gnome_terminal_LDFLAGS =

gnome_terminal_LDADD = \
	skey/libskey.la \
	$(TERM_LIBS)

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

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = gnome-terminal.schemas.in
schema_DATA = gnome-terminal.schemas

uidir = $(pkgdatadir)
ui_DATA = \
	terminal.ui \
	encodings-dialog.ui \
	keybinding-editor.ui \
	profile-manager.ui \
	profile-new-dialog.ui \
	profile-preferences.ui \
	skey-challenge.ui \
	$(NULL)

CLEANFILES = \
	gnome-terminal.schemas \
	stamp-terminal-type-builtins.h \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	$(icon_DATA) \
	$(schema_in_files) \
	$(server_DATA) \
	$(ui_DATA) \
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_DATA = gnome-terminal.server

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/src/$(schema_DATA)
endif
