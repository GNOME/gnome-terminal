
INCLUDES=@TERM_CFLAGS@ -I$(srcdir)/..

## I can't believe we need all this just to get the ellipsizing code :-/
## but I don't want to modify the files I'm cut-and-pasting
EELFILES=eel-ellipsizing-label.h		\
	eel-ellipsizing-label.c			\
	eel-glib-extensions.h			\
	eel-glib-extensions.c			\
	eel-art-extensions.h			\
	eel-art-extensions.c			\
        eel-pango-extensions.h			\
        eel-pango-extensions.c			\
        eel-lib-self-check-functions.h		\
        eel-self-checks.h			\
        eel-self-checks.c			\
        eel-string-list.h			\
        eel-string-list.c			\
	eel-gtk-macros.h			\
	eel-string.h				\
	eel-string.c				\
	eel-debug.h				\
	eel-debug.c

EELDIR=$(srcdir)/../../../eel/eel

noinst_LTLIBRARIES=libeelcnp.la

libeelcnp_la_SOURCES=$(EELFILES)

libeelcnp_la_LDFLAGS= @TERM_LIBS@

regenerate-built-sources:
	EELFILES="$(EELFILES)" EELDIR="$(EELDIR)" update-from-eel.sh

EXTRA_DIST=update-from-eel.sh