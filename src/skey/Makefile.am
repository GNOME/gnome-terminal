### FIXME
INCLUDES=@TERM_CFLAGS@ -I$(srcdir)/..

SKEYFILES =		\
	btoe.c		\
	btoe.h		\
	skey.h		\
	skey.c		\
	skeyutil.h	\
	skeyutil.c	\
	md4.c		\
	md4.h		\
	md5.c		\
	md5.h		\
	sha1.h		\
	sha1.c

noinst_LTLIBRARIES=libskey.la

libskey_la_SOURCES=$(SKEYFILES)

libskey_la_LDFLAGS= @TERM_LIBS@

noinst_PROGRAMS=testskey

testskey_CFLAGS = @CFLAGS@ @TERM_CFLAGS@ -DSKEY_MAIN
testskey_LDFLAGS= @TERM_LIBS@
testskey_SOURCES = \
	btoe.c		\
	btoe.h		\
	skey.h		\
	skey.c		\
	skeyutil.h	\
	skeyutil.c	\
	md4.c		\
	md4.h		\
	md5.c		\
	md5.h		\
	sha1.h		\
	sha1.c

