NULL =

bin_PROGRAMS = gnome-terminal
libexec_PROGRAMS = gnome-terminal-server
noinst_PROGRAMS = gnome-terminal-client

if WITH_NAUTILUS_EXTENSION
nautilusextension_LTLIBRARIES = libterminal-nautilus.la
endif # WITH_NAUTILUS_EXTENSION

# Terminal server

dbusservice_DATA = org.gnome.Terminal.service

BUILT_SOURCES = \
	terminal-marshal.c \
	terminal-marshal.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	terminal-resources.c \
	terminal-resources.h \
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
	terminal-debug.c \
	terminal-debug.h \
	terminal-defines.h \
	terminal-enums.h \
	terminal-encoding.c \
	terminal-encoding.h \
	terminal-gdbus.c \
	terminal-gdbus.h \
	terminal-info-bar.c \
	terminal-info-bar.h \
	terminal-intl.h \
	terminal-i18n.c \
	terminal-i18n.h \
	terminal-mdi-container.c \
	terminal-mdi-container.h \
	terminal-notebook.c \
	terminal-notebook.h \
	terminal-prefs.c \
	terminal-prefs.h \
	terminal-profiles-list.c \
	terminal-profiles-list.h \
	terminal-schemas.h \
	terminal-settings-list.c \
	terminal-settings-list.h \
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

nodist_gnome_terminal_server_SOURCES = $(BUILT_SOURCES)

gnome_terminal_server_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DEXECUTABLE_NAME=\"gnome-terminal\" \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_HELPDIR="\"$(HELP_DIR)\"" \
	-DTERM_LIBEXECDIR="\"$(libexecdir)\"" \
	-DSN_API_NOT_YET_FROZEN \
	-DGDK_MULTIHEAD_SAFE \
	-DGDK_DISABLE_DEPRECATION_WARNINGS \
	$(AM_CPPFLAGS)

gnome_terminal_server_CFLAGS = \
	$(TERM_CFLAGS) \
	$(AM_CFLAGS)

gnome_terminal_server_LDFLAGS = \
	$(AM_LDFLAGS)

gnome_terminal_server_LDADD = \
	$(TERM_LIBS)

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

org.gnome.Terminal.service: Makefile
	$(AM_V_GEN) ( echo "[D-BUS Service]"; \
		      echo "Name=org.gnome.Terminal"; \
		      echo "Exec=${libexecdir}/gnome-terminal-server") > $@

terminal-gdbus-generated.c terminal-gdbus-generated.h: org.gnome.Terminal.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
			--interface-prefix=org.gnome.Terminal \
			--c-namespace=Terminal \
			--c-generate-object-manager \
			--generate-c-code terminal-gdbus-generated \
			$<

terminal-resources.h terminal-resources.c: terminal.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/terminal.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name terminal $<

# Terminal client

gnome_terminal_client_SOURCES = \
	client.c \
	terminal-client-utils.c \
	terminal-client-utils.h \
	terminal-debug.c \
	terminal-debug.h \
	terminal-defines.h \
	terminal-i18n.c \
	terminal-i18n.h \
	terminal-profiles-list.c \
	terminal-profiles-list.h \
	terminal-schemas.h \
	terminal-settings-list.c \
	terminal-settings-list.h \
	$(NULL)

nodist_gnome_terminal_client_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
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
	terminal-client-utils.c \
	terminal-client-utils.h \
	terminal-debug.c \
	terminal-debug.h \
	terminal-defines.h \
	terminal-i18n.c \
	terminal-i18n.h \
	terminal-options.c \
	terminal-options.h \
	terminal-profiles-list.c \
	terminal-profiles-list.h \
	terminal-schemas.h \
	terminal-settings-list.c \
	terminal-settings-list.h \
	$(NULL)

nodist_gnome_terminal_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
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

# Nautilus extension

libterminal_nautilus_la_SOURCES = \
	terminal-client-utils.c \
	terminal-client-utils.h \
	terminal-defines.h \
	terminal-i18n.c \
	terminal-i18n.h \
	terminal-nautilus.c \
	$(NULL)

nodist_libterminal_nautilus_la_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	$(NULL)

libterminal_nautilus_la_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
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

if ENABLE_MIGRATION
libexec_PROGRAMS += gnome-terminal-migration

gnome_terminal_migration_SOURCES = \
	migration.c \
	terminal-debug.c \
	terminal-debug.h \
	terminal-profiles-list.c \
	terminal-profiles-list.h \
	terminal-schemas.h \
	terminal-settings-list.c \
	terminal-settings-list.h \
	$(NULL)

nodist_gnome_terminal_migration_SOURCES = \
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

endif # ENABLE_MIGRATION

gsettingsschema_in_files = org.gnome.Terminal.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.gschema.xml.in=.gschema.xml)

CLEANFILES = \
	stamp-terminal-type-builtins.h \
	gnome-terminal.schemas \
	stamp-terminal-type-builtins.h \
	org.gnome.Terminal.service \
	$(gsettings_SCHEMAS) \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	terminal.about \
	terminal.xml \
	terminal-appmenu.ui \
	terminal.gresource.xml \
	terminal-marshal.list \
	terminal-type-builtins.c.template \
	terminal-type-builtins.h.template \
	org.gnome.Terminal.xml \
	nautilus.symbols \
	find-dialog.ui \
	preferences.ui \
	profile-preferences.ui \
	$(about_DATA) \
	$(uimanager_DATA) \
	$(builder_DATA) \
	$(gsettingsschema_in_files) \
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

-include $(top_srcdir)/git.mk
