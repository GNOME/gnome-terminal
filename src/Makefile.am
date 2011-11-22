NULL =

bin_PROGRAMS = gnome-terminal-client gnome-terminal
libexec_PROGRAMS = gnome-terminal-server

if WITH_NAUTILUS_EXTENSION
nautilusextension_LTLIBRARIES = libterminal-nautilus.la
endif # WITH_NAUTILUS_EXTENSION

# Terminal server

dbusservice_DATA = org.gnome.Terminal.Factory0.service

BUILT_SOURCES = \
	terminal-marshal.c \
	terminal-marshal.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	$(NULL)

gnome_terminal_server_SOURCES = \
	eggshell.c \
	eggshell.h \
	profile-editor.c \
	profile-editor.h \
	server.c \
	terminal-accels.c \
	terminal-accels.h \
	terminal-app.c \
	terminal-app.h \
	terminal-close-button.h \
	terminal-close-button.c \
	terminal-controller.c \
	terminal-controller.h \
	terminal-debug.c \
	terminal-debug.h \
	terminal-defines.h \
	terminal-encoding.c \
	terminal-encoding.h \
	terminal-info-bar.c \
	terminal-info-bar.h \
	terminal-intl.h \
	terminal-schemas.h \
	terminal-screen.c \
	terminal-screen.h \
	terminal-screen-container.c \
	terminal-screen-container.h \
	terminal-search-dialog.c \
	terminal-search-dialog.h \
	terminal-tab-label.c \
	terminal-tab-label.h \
	terminal-tabs-menu.c \
	terminal-tabs-menu.h \
	terminal-util.c \
	terminal-util.h \
	terminal-version.h \
	terminal-window.c \
	terminal-window.h \
	$(NULL)

nodist_gnome_terminal_server_SOURCES= $(BUILT_SOURCES)

gnome_terminal_server_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERMINAL_SERVER \
	-DEXECUTABLE_NAME=\"gnome-terminal\" \
	-DTERM_DATADIR="\"$(datadir)\"" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_PKGDATADIR="\"$(pkgdatadir)\"" \
	-DTERM_HELPDIR="\"$(HELP_DIR)\"" \
	-DSN_API_NOT_YET_FROZEN \
	-DGDK_MULTIHEAD_SAFE \
	$(AM_CPPFLAGS)

gnome_terminal_server_CFLAGS = \
	$(TERM_CFLAGS) \
	$(AM_CFLAGS)

gnome_terminal_server_LDFLAGS = \
	$(AM_LDFLAGS)

gnome_terminal_server_LDADD = \
	$(TERM_LIBS)

if WITH_SMCLIENT
gnome_terminal_server_SOURCES += \
	eggsmclient.c \
	eggsmclient.h \
	eggsmclient-private.h \
	$(NULL)
gnome_terminal_server_CFLAGS += $(SMCLIENT_CFLAGS)
gnome_terminal_server_LDADD += $(SMCLIENT_LIBS)

if WITH_SMCLIENT_XSMP
gnome_terminal_server_SOURCES += \
	eggdesktopfile.c \
	eggdesktopfile.h \
	eggsmclient-xsmp.c \
	$(NULL)
gnome_terminal_server_CPPFLAGS += -DEGG_SM_CLIENT_BACKEND_XSMP
endif
if WITH_SMCLIENT_WIN32
gnome_terminal_server_SOURCES += eggsmclient-win32.c
endif
if WITH_SMCLIENT_QUARTZ
gnome_terminal_server_SOURCES += eggsmclient-osx.c
endif
endif

TYPES_H_FILES = \
	terminal-enums.h \
	$(NULL)

terminal-type-builtins.h: stamp-terminal-type-builtins.h
	@true

stamp-terminal-type-builtins.h: terminal-type-builtins.h.template $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-ttbh \
	&& (cmp -s xgen-ttbh terminal-type-builtins.h || cp xgen-ttbh terminal-type-builtins.h ) \
	&& rm -f xgen-ttbh \
	&& echo timestamp > $(@F)

terminal-type-builtins.c: terminal-type-builtins.c.template $(TYPES_H_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-ttbc \
	&& (cmp -s xgen-ttbc terminal-type-builtins.c || cp xgen-ttbc terminal-type-builtins.c ) \
	&& rm -f xgen-ttbc

terminal-marshal.h: $(srcdir)/terminal-marshal.list
	$(AM_V_GEN) ( $(GLIB_GENMARSHAL) --prefix=_terminal_marshal $(srcdir)/terminal-marshal.list \
			--header \
			--internal > terminal-marshal.h.tmp \
	&& mv terminal-marshal.h.tmp terminal-marshal.h ) \
	|| ( rm -f terminal-marshal.h.tmp && exit 1 )

terminal-marshal.c: $(srcdir)/terminal-marshal.list
	$(AM_V_GEN) ( $(GLIB_GENMARSHAL) --prefix=_terminal_marshal $(srcdir)/terminal-marshal.list \
				--header \
				--body \
				--internal > terminal-marshal.c.tmp \
		&& mv terminal-marshal.c.tmp terminal-marshal.c ) \
	|| ( rm -f terminal-marshal.c.tmp && exit 1 )

org.gnome.Terminal.Factory0.service: Makefile
	$(AM_V_GEN) ( echo "[D-BUS Service]"; \
		      echo "Name=org.gnome.Terminal.Factory0"; \
		      echo "${libexecdir}/gnome-terminal-server") > $@

terminal-gdbus-generated.c terminal-gdbus-generated.h: org.gnome.Terminal.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
			--interface-prefix=org.gnome.Terminal \
			--c-namespace=Terminal \
			--c-generate-object-manager \
			--generate-c-code terminal-gdbus-generated \
			$<

# Terminal client

gnome_terminal_client_SOURCES = \
	client.c \
	terminal-defines.h \
	terminal-intl.h \
	$(NULL)

nodist_gnome_terminal_client_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	$(NULL)

gnome_terminal_client_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERMINAL_CLIENT \
	-DTERM_DATADIR="\"$(datadir)\"" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_PKGDATADIR="\"$(pkgdatadir)\"" \
	-DSN_API_NOT_YET_FROZEN \
	-DGDK_MULTIHEAD_SAFE \
	$(AM_CPPFLAGS)

gnome_terminal_client_CFLAGS = \
	$(TERM_CFLAGS) \
	$(AM_CFLAGS)

gnome_terminal_client_LDFLAGS = \
	$(AM_LDFLAGS)

gnome_terminal_client_LDADD = \
	$(TERM_LIBS)

# Legacy terminal client

gnome_terminal_SOURCES = \
	terminal.c \
	terminal-debug.c \
	terminal-debug.h \
	terminal-defines.h \
	terminal-intl.h \
	terminal-options.c \
	terminal-options.h \
	$(NULL)

nodist_gnome_terminal_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	$(NULL)

gnome_terminal_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERMINAL_CLIENT \
	-DTERM_DATADIR="\"$(datadir)\"" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_PKGDATADIR="\"$(pkgdatadir)\"" \
	-DSN_API_NOT_YET_FROZEN \
	-DGDK_MULTIHEAD_SAFE \
	$(AM_CPPFLAGS)

gnome_terminal_CFLAGS = \
	$(TERM_CFLAGS) \
	$(AM_CFLAGS)

gnome_terminal_LDFLAGS = \
	$(AM_LDFLAGS)

gnome_terminal_LDADD = \
	$(TERM_LIBS)


if WITH_SMCLIENT
gnome_terminal_SOURCES += \
	eggsmclient.c \
	eggsmclient.h \
	eggsmclient-private.h \
	$(NULL)
gnome_terminal_CFLAGS += $(SMCLIENT_CFLAGS)
gnome_terminal_LDADD += $(SMCLIENT_LIBS)

if WITH_SMCLIENT_XSMP
gnome_terminal_SOURCES += \
	eggdesktopfile.c \
	eggdesktopfile.h \
	eggsmclient-xsmp.c \
	$(NULL)
gnome_terminal_CPPFLAGS += -DEGG_SM_CLIENT_BACKEND_XSMP
endif
if WITH_SMCLIENT_WIN32
gnome_terminal_SOURCES += eggsmclient-win32.c
endif
if WITH_SMCLIENT_QUARTZ
gnome_terminal_SOURCES += eggsmclient-osx.c
endif
endif

# Nautilus extension

libterminal_nautilus_la_SOURCES = \
	nautilus.c \
	terminal-defines.h \
	terminal-intl.h \
	terminal-nautilus.c \
	terminal-nautilus.h \
	$(NULL)

nodist_libterminal_nautilus_la_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	$(NULL)

libterminal_nautilus_la_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERM_DATADIR="\"$(datadir)\"" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_PKGDATADIR="\"$(pkgdatadir)\"" \
	-DGDK_MULTIHEAD_SAFE \
	$(AM_CPPFLAGS)

libterminal_nautilus_la_CFLAGS = \
	$(NAUTILUS_CFLAGS) \
	$(AM_CFLAGS)

libterminal_nautilus_la_LDFLAGS = \
	-module -avoid-version \
	-export-symbols $(srcdir)/nautilus.symbols \
	$(AM_LDFLAGS)

libterminal_nautilus_la_LIBADD = \
	$(NAUTILUS_LIBS)

# Pref migrator

migrationdir = $(libexecdir)/GConf/gsettings
migration_PROGRAMS = gnome-terminal-migration

gnome_terminal_migration_SOURCES = \
	migration.c \
	terminal-schemas.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	$(NULL)

gnome_terminal_migration_CPPFLAGS = \
	$(AM_CPPFLAGS)
gnome_terminal_migration_CFLAGS = \
	$(MIGRATOR_CFLAGS) \
	$(AM_CFLAGS)
gnome_terminal_migration_LDFLAGS = \
	$(AM_LDFLAGS)
gnome_terminal_migration_LDADD = \
	$(MIGRATOR_LIBS) \
	$(INTLLIBS)

aboutdir = $(pkgdatadir)
about_DATA = \
	terminal.about \
	$(NULL)

uimanagerdir = $(pkgdatadir)
uimanager_DATA = \
	terminal.xml \
	$(NULL)

builder_in_files = \
	encodings-dialog.glade \
	find-dialog.glade \
	keybinding-editor.glade \
	profile-manager.glade \
	profile-new-dialog.glade \
	profile-preferences.glade \
	$(NULL)

builderdir = $(pkgdatadir)
builder_DATA =  $(builder_in_files:.glade=.ui)

gsettingsschema_in_files = org.gnome.Terminal.Preferences.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.gschema.xml.in=.gschema.xml)

CLEANFILES = \
	stamp-terminal-type-builtins.h \
	gnome-terminal.schemas \
	stamp-terminal-type-builtins.h \
	$(builder_DATA) \
	$(gsettings_SCHEMAS) \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	terminal-marshal.list \
	terminal-type-builtins.c.template \
	terminal-type-builtins.h.template \
	extra-strings.c \
	nautilus.symbols \
	$(about_DATA) \
	$(uimanager_DATA) \
	$(builder_in_files) \
	$(gsettingsschema_in_files) \
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

%.ui: %.glade
	$(AM_V_GEN) $(GTK_BUILDER_CONVERT) $< $@

-include $(top_srcdir)/git.mk
