NULL =

BUILT_SOURCES =

bin_PROGRAMS = gnome-terminal
libexec_PROGRAMS = gnome-terminal-server
noinst_PROGRAMS =

check_PROGRAMS = terminal-regex

if WITH_NAUTILUS_EXTENSION
nautilusextension_LTLIBRARIES = libterminal-nautilus.la
endif # WITH_NAUTILUS_EXTENSION

# Terminal server

dbusservice_DATA = org.gnome.Terminal.service

# It is correct for this to be in ${prefix}/lib, even on systems where that
# does not match ${libdir}. This is what systemd uses on such platforms.
systemduserdir = $(prefix)/lib/systemd/user
nodist_systemduser_DATA = gnome-terminal-server.service

gnome_terminal_server_built = \
	terminal-marshal.c \
	terminal-marshal.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	terminal-resources.c \
	terminal-resources.h \
	$(NULL)
BUILT_SOURCES += $(gnome_terminal_server_built)

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
	terminal-debug.c \
	terminal-debug.h \
	terminal-defines.h \
	terminal-enums.h \
	terminal-encoding.c \
	terminal-encoding.h \
	terminal-gdbus.c \
	terminal-gdbus.h \
	terminal-icon-button.h \
	terminal-icon-button.c \
	terminal-info-bar.c \
	terminal-info-bar.h \
	terminal-intl.h \
	terminal-i18n.c \
	terminal-i18n.h \
	terminal-libgsystem.h \
	terminal-mdi-container.c \
	terminal-mdi-container.h \
	terminal-notebook.c \
	terminal-notebook.h \
	terminal-pcre2.h \
	terminal-prefs.c \
	terminal-prefs.h \
	terminal-profiles-list.c \
	terminal-profiles-list.h \
	terminal-regex.h \
	terminal-schemas.h \
	terminal-settings-list.c \
	terminal-settings-list.h \
	terminal-screen.c \
	terminal-screen.h \
	terminal-screen-container.c \
	terminal-screen-container.h \
	terminal-search-popover.c \
	terminal-search-popover.h \
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

nodist_gnome_terminal_server_SOURCES = $(gnome_terminal_server_built)

gnome_terminal_server_CPPFLAGS = \
	-DTERMINAL_COMPILATION \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	-DTERM_HELPDIR="\"$(HELP_DIR)\"" \
	-DTERM_LIBEXECDIR="\"$(libexecdir)\"" \
	$(AM_CPPFLAGS)

gnome_terminal_server_CFLAGS = \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
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
		      echo "SystemdService=gnome-terminal-server.service"; \
		      echo "Exec=${libexecdir}/gnome-terminal-server") > $@

gnome-terminal-server.service: Makefile
	$(AM_V_GEN) ( echo "[Unit]"; \
		      echo "Description=GNOME Terminal Server"; \
		      echo "[Service]"; \
		      echo "KillMode=process"; \
		      echo "Type=dbus"; \
		      echo "BusName=org.gnome.Terminal"; \
		      echo "ExecStart=${libexecdir}/gnome-terminal-server") > $@

terminal-gdbus-generated.c terminal-gdbus-generated.h: org.gnome.Terminal.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
			--interface-prefix=org.gnome.Terminal \
			--c-namespace=Terminal \
			--c-generate-object-manager \
			--generate-c-code terminal-gdbus-generated \
			$<

terminal-resources.h terminal-resources.c: terminal.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/terminal.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name terminal $<

# Checks

TESTS = \
	terminal-regex \
	$(NULL)

# Check programmes

terminal_regex_CPPFLAGS = \
	$(AM_CPPFLAGS)
terminal_regex_SOURCES = \
	terminal-regex.c \
	terminal-regex.h \
	$(NULL)
terminal_regex_CFLAGS = \
	-DTERMINAL_REGEX_MAIN \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)
terminal_regex_LDFLAGS = \
	$(AM_LDFLAGS)
terminal_regex_LDADD = \
	$(TERM_LIBS)

# Terminal client

if ENABLE_GTERMINAL

noinst_PROGRAMS += gterminal

gterminal_SOURCES = \
	gterminal.vala \
	client.vapi \
	config.vapi \
	profiles.vapi \
	terminal-client-utils.c \
	terminal-client-utils.h \
	terminal-debug.c \
	terminal-debug.h \
	terminal-profiles-list.c \
	terminal-profiles-list.h \
	terminal-settings-list.c \
	terminal-settings-list.h \
	$(NULL)

nodist_gterminal_SOURCES = \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	$(NULL)

gterminal_VALAFLAGS = \
	--vapidir=. \
	--pkg=glib-2.0 \
	--pkg=gio-2.0 \
	--pkg=gio-unix-2.0 \
	--pkg=posix \
	--pkg=gtk+-$(GTK_API_VERSION) \
	$(GTERMINAL_VALAFLAGS) \
	$(AM_VALAFLAGS)

gterminal_CPPFLAGS = \
	-I$(top_builddir) \
	-DTERMINAL_COMPILATION \
	-DTERMINAL_CLIENT \
	-DLOCALEDIR="\"$(datadir)/locale\"" \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\" \
	$(AM_CPPFLAGS)

# See bug #710862 about -Wsuggest-attribute=format
gterminal_CFLAGS = \
	--include config.h \
	$(GTERMINAL_CFLAGS) \
	$(AM_CFLAGS) \
	-Wno-cast-qual \
	-Wno-format-nonliteral \
	-Wno-suggest-attribute=format \
	-Wno-unused-but-set-variable \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-write-strings \
	$(NULL)
gterminal_LDFLAGS = \
	$(AM_LDFLAGS)
gterminal_LDADD = \
	$(GTERMINAL_LIBS) \
	$(NULL)

endif # ENABLE_GTERMINAL

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
	terminal-libgsystem.h \
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
	$(AM_CPPFLAGS)

gnome_terminal_CFLAGS = \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
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
	terminal-libgsystem.h \
	terminal-nautilus.c \
	$(NULL)

nodist_libterminal_nautilus_la_SOURCES = \
	terminal-gdbus-generated.c \
	terminal-gdbus-generated.h \
	terminal-type-builtins.c \
	terminal-type-builtins.h \
	$(NULL)

libterminal_nautilus_la_CPPFLAGS = \
	-DTERM_LOCALEDIR="\"$(datadir)/locale\"" \
	$(AM_CPPFLAGS)

libterminal_nautilus_la_CFLAGS = \
	$(NAUTILUS_CFLAGS) \
	$(WARN_CFLAGS) \
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
	terminal-libgsystem.h \
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
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)
gnome_terminal_migration_LDFLAGS = \
	$(AM_LDFLAGS)
gnome_terminal_migration_LDADD = \
	$(MIGRATOR_LIBS) \
	$(INTLLIBS)

endif # ENABLE_MIGRATION

# GNOME Shell search provider

if ENABLE_SEARCH_PROVIDER

searchproviderdir = $(datadir)/gnome-shell/search-providers
dist_searchprovider_DATA = gnome-terminal-search-provider.ini

gnome_terminal_server_built += \
	terminal-search-provider-gdbus-generated.c \
	terminal-search-provider-gdbus-generated.h \
	$(NULL)

gnome_terminal_server_SOURCES += \
	terminal-search-provider.c \
	terminal-search-provider.h \
	$(NULL)

terminal-search-provider-gdbus-generated.c terminal-search-provider-gdbus-generated.h: $(dbusinterfacedir)/org.gnome.ShellSearchProvider2.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
			--interface-prefix=org.gnome.Shell \
			--c-namespace=Terminal \
			--generate-c-code terminal-search-provider-gdbus-generated \
			$<

endif # ENABLE_SEARCH_PROVIDER

gsettings_SCHEMAS = \
	org.gnome.Terminal.gschema.xml \
	$(NULL)

CLEANFILES = \
	stamp-terminal-type-builtins.h \
	gnome-terminal.schemas \
	stamp-terminal-type-builtins.h \
	org.gnome.Terminal.service \
	gnome-terminal-server.service \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	terminal.about \
	terminal.xml \
	terminal-menus.ui \
	terminal-window.ui \
	terminal.gresource.xml \
	terminal-marshal.list \
	terminal-type-builtins.c.template \
	terminal-type-builtins.h.template \
	org.gnome.Terminal.xml \
	nautilus.symbols \
	search-popover.ui \
	preferences.ui \
	profile-preferences.ui \
	terminal.common.css \
	$(about_DATA) \
	$(uimanager_DATA) \
	$(builder_DATA) \
	$(gsettings_SCHEMAS) \
	$(NULL)

@GSETTINGS_RULES@

-include $(top_srcdir)/git.mk
