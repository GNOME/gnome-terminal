
INCLUDES=@PROFTERM_CFLAGS@ -DPROFTERM_PKGDATADIR=\"$(datadir)/profterm\" \
	-DPROFTERM_LOCALEDIR=\"$(datadir)/locale\" -DEXECUTABLE_NAME=\"profterm\"

noinst_PROGRAMS=profterm

## Terminal
profterm_SOURCES=				\
	terminal.c				\
	terminal.h				\
	terminal-profile.c			\
	terminal-profile.h			\
	terminal-screen.c			\
	terminal-screen.h			\
	terminal-window.c			\
	terminal-window.h

profterm_LDADD= @PROFTERM_LIBS@

