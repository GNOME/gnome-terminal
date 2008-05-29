NULL =

noinst_LTLIBRARIES = libskey.la

libskey_la_SOURCES = \
	btoe.c \
	btoe.h \
	skey.h \
	skey.c \
	skeyutil.h\
	skeyutil.c\
	md4.c \
	md4.h \
	md5.c \
	md5.h \
	sha1.h \
	sha1.c \
	$(NULL)

libskey_la_CPPFLAGS = \
	-I$(srcdir)/.. \
	$(AM_CPPFLAGS)

libskey_la_CFLAGS = \
	$(TERM_CFLAGS) \
	$(AM_CFLASG)

# 	$(WARN_CFLAGS) \
# 	$(DISABLE_DEPRECATED) \
#

libskey_la_LDFLAGS =

libskey_la_LIBADD = \
	$(TERM_LIBS)

noinst_PROGRAMS = testskey

testskey_SOURCES = \
	test.c \
	$(NULL)

testskey_CPPFLAGS = \
	-I$(srcdir)/.. \
	-DSKEY_MAIN \
	$(AM_CPPFLAGS)

testskey_CFLAGS = \
	$(TERM_CFLAGS) \
	$(AM_CFLAGS)

testskey_LDFLAGS =

testskey_LDADD = \
	libskey.la \
	$(TERM_LIBS)
