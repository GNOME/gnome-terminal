
### FIXME
INCLUDES=@TERM_CFLAGS@ -I$(srcdir)/..

SKEYFILES =		\
	btoe.c		\
	config.h	\
	skey.h		\
	donkey.h	\
	donkey.c	\
	md.h		\
	md.c		\
	md2.c		\
	md4.c		\
	md5.c		\
	skey.c		\
	skey.h		\
	tailor.h

noinst_LTLIBRARIES=libskey.la

libskey_la_SOURCES=$(SKEYFILES)

libskey_la_LDFLAGS= @TERM_LIBS@
