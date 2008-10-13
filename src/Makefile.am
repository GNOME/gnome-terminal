NULL =

SUBDIRS = skey

bin_PROGRAMS = gnome-terminal

gnome_terminal_SOURCES= \
	profile-editor.c \
	profile-editor.h \
	skey-popup.c \
	skey-popup.h \
	terminal.c \
	terminal-accels.c \
	terminal-accels.h \
	terminal-app.c \
	terminal-app.h \
	terminal-encoding.c \
	terminal-encoding.h \
	terminal-intl.h \
	terminal-marshal.c \
	terminal-marshal.h \
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
	-DTERM_DATADIR="\"$(datadir)\"" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_LIBDIR="\"$(pkglibdir)\"" \
	-DTERM_PREFIX="\"$(prefix)\"" \
	-DTERM_SYSCONFDIR="\"$(sysconfdir)\"" \
	-DTERM_PKGDATADIR="\"$(pkgdatadir)\"" \
	-DSN_API_NOT_YET_FROZEN \
	-DGDK_MULTIHEAD_SAFE \
	-DG_DISABLE_SINGLE_INCLUDES \
	-DPANGO_DISABLE_SINGLE_INCLUDES \
	-DATK_DISABLE_SINGLE_INCLUDES \
	-DGDK_DISABLE_SINGLE_INCLUDES \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES \
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
	terminal-marshal.c \
	terminal-marshal.h \
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

terminal-marshal.h: $(srcdir)/terminal-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=_terminal_marshal $(srcdir)/terminal-marshal.list \
			--header \
			--internal > terminal-marshal.h.tmp \
	&& mv terminal-marshal.h.tmp terminal-marshal.h ) \
	|| ( rm -f terminal-marshal.h.tmp && exit 1 )

terminal-marshal.c: $(srcdir)/terminal-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=_terminal_marshal $(srcdir)/terminal-marshal.list \
				--header \
				--body \
				--internal > terminal-marshal.c.tmp \
		&& mv terminal-marshal.c.tmp terminal-marshal.c ) \
	|| ( rm -f terminal-marshal.c.tmp && exit 1 )

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = gnome-terminal.schemas.in
schema_DATA = gnome-terminal.schemas

uimanagerdir = $(pkgdatadir)
uimanager_DATA = \
	terminal.xml \
	$(NULL)

builder_in_files = \
	encodings-dialog.glade \
	keybinding-editor.glade \
	profile-manager.glade \
	profile-new-dialog.glade \
	profile-preferences.glade \
	skey-challenge.glade \
	$(NULL)

builderdir = $(pkgdatadir)
builder_DATA = $(builder_in_files:.glade=.ui)

CLEANFILES = \
	stamp-terminal-type-builtins.h \
	gnome-terminal.schemas \
	stamp-terminal-type-builtins.h \
	$(builder_DATA) \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	terminal-marshal.list \
	terminal-type-builtins.c.template \
	terminal-type-builtins.h.template \
	$(icon_DATA) \
	$(schema_in_files) \
	$(server_DATA) \
	$(uimanager_DATA) \
	$(builder_in_files) \
	$(NULL)

serverdir = $(libdir)/bonobo/servers
server_DATA = gnome-terminal.server

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/src/$(schema_DATA)
endif

%.ui: %.glade
	$(GTK_BUILDER_CONVERT) $< $@
