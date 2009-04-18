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
	-DG_DISABLE_SINGLE_INCLUDES \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

libskey_la_CFLAGS = \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLASG)

libskey_la_LDFLAGS =

libskey_la_LIBADD = \
	$(TERM_LIBS)

check_PROGRAMS = testskey

testskey_SOURCES = \
	test.c \
	$(NULL)

testskey_CPPFLAGS = \
	-I$(srcdir)/.. \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

testskey_CFLAGS = \
	$(TERM_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

testskey_LDFLAGS =

testskey_LDADD = \
	libskey.la \
	$(TERM_LIBS)

TESTS = testskey

-include $(top_srcdir)/git.mk
