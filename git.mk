# git.mk
#
# Copyright 2009, Red Hat, Inc.
# Written by Behdad Esfahbod
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
#
# The canonical source for this file is pango/git.mk, or whereever the
# header of pango/git.mk suggests in the future.
#
# To use in your project, import this file in your git repo's toplevel,
# then do "make -f git.mk".  This modifies all Makefile.am files in
# your project to include git.mk.
#
# This enables automatic .gitignore generation.  If you need to ignore
# more files, add them to the GITIGNOREFILES variable in your Makefile.am.
# But think twice before doing that.  If a file has to be in .gitignore,
# chances are very high that it's a generated file and should be in one
# of MOSTLYCLEANFILES, CLEANFILES, DISTCLEANFILES, or MAINTAINERCLEANFILES.
#
# The only case that you need to manually add a file to GITIGNOREFILES is
# when remove files in one of mostlyclean-local, clean-local, distclean-local,
# or maintainer-clean-local.
#
# Note that for files like editor backup, etc, there are better places to
# ignore them.  See "man gitignore".
#
# If "make maintainer-clean" removes the files but they are not recognized
# by this script (that is, if "git stat" shows untracked files still), send
# me the output of "git stat" as well as your Makefile.am and Makefile for
# the directories involved.
#
# For a list of toplevel files that should be in MAINTAINERCLEANFILES, see
# pango/Makefile.am.
#
# Don't EXTRA_DIST this file.  It is supposed to only live in git clones,
# not tarballs.  It serves no useful purpose in tarballs and clutters the
# build dir.
#
# This file knows how to handle autoconf, automake, libtool, gtk-doc,
# gnome-doc-utils, intltool.
#
#
# KNOWN ISSUES:
#
# - Recursive configure doesn't work as $(top_srcdir)/git.mk inside the
#   submodule doesn't find us.  If you have configure.{in,ac} files in
#   subdirs, add a proxy git.mk file in those dirs that simply does:
#   "include $(top_srcdir)/../git.mk".  Add more ..'s to your taste.
#   And add those files to git.  See vte/gnome-pty-helper/git.mk for
#   example.
#

git-all: git-mk-install

git-mk-install:
	@echo Installing git makefile
	@any_failed=; find $(top_srcdir) -name Makefile.am | while read x; do \
		if grep 'include .*/git.mk' $$x >/dev/null; then \
			echo $$x already includes git.mk; \
		else \
			failed=; \
			echo "Updating $$x"; \
			{ cat $$x; \
			  echo ''; \
			  echo '-include $$(top_srcdir)/git.mk'; \
			} > $$x.tmp || failed=1; \
			if test x$$failed = x; then \
				mv $$x.tmp $$x || failed=1; \
			fi; \
			if test x$$failed = x; then : else \
				echo Failed updating $$x; >&2 \
				any_failed=1; \
			fi; \
	fi; done; test -z "$$any_failed"

.PHONY: git-all git-mk-install


### .gitignore generation

$(srcdir)/.gitignore: Makefile.am $(top_srcdir)/git.mk
	@echo Generating $@; \
	GTKDOCGITIGNOREFILES=; \
	test "x$(DOC_MODULE)" = x -o "x$(DOC_MAIN_SGML_FILE)" = x || \
	GTKDOCGITIGNOREFILES=" \
		$(DOC_MODULE)-decl-list.txt \
		$(DOC_MODULE)-decl.txt \
		tmpl/$(DOC_MODULE)-unused.sgml \
		tmpl/*.bak \
		xml html \
		"; \
	GNOMEDOCUTILSGITIGNOREFILES=; \
	test "x$(DOC_MODULE)" = x -o "x$(DOC_LINGUAS)" = x || \
	GNOMEDOCUTILSGITIGNOREFILES=" \
		$(_DOC_C_DOCS) \
		$(_DOC_LC_DOCS) \
		$(_DOC_OMF_ALL) \
		$(_DOC_DSK_ALL) \
		$(_DOC_HTML_ALL) \
		$(_DOC_POFILES) \
		*/.xml2po.mo \
		*/*.omf.out \
		"; \
	INTLTOOLGITIGNOREFILES=; test -f $(srcdir)/po/Makefile.in.in && \
	INTLTOOLGITIGNOREFILES=" \
		po/Makefile.in.in \
		po/Makefile.in \
		po/Makefile \
		po/*.gmo \
		po/*.mo \
		po/POTFILES \
		po/stamp-it \
		po/.intltool-merge-cache \
		intltool-extract.in \
		intltool-merge.in \
		intltool-update.in \
		"; \
	AUTOCONFGITIGNOREFILES=; test -f $(srcdir)/configure && \
	AUTOCONFGITIGNOREFILES=" \
		autom4te.cache \
		configure \
		config.h \
		stamp-h1 \
		libtool \
		config.lt \
		"; \
	for x in \
		.gitignore \
		$$GTKDOCGITIGNOREFILES \
		$$GNOMEDOCUTILSGITIGNOREFILES \
		$$INTLTOOLGITIGNOREFILES \
		$$AUTOCONFGITIGNOREFILES \
		$(GITIGNOREFILES) \
		$(CLEANFILES) \
		$(PROGRAMS) \
		$(EXTRA_PROGRAMS) \
		$(LTLIBRARIES) \
		so_locations \
		.libs _libs \
		$(MOSTLYCLEANFILES) \
		"*.$(OBJEXT)" \
		"*.lo" \
		$(DISTCLEANFILES) \
		$(am__CONFIG_DISTCLEAN_FILES) \
		$(CONFIG_CLEAN_FILES) \
		TAGS ID GTAGS GRTAGS GSYMS GPATH tags \
		"*.tab.c" \
		$(MAINTAINERCLEANFILES) \
		$(BUILT_SOURCES) \
		$(DEPDIR) \
		Makefile \
		Makefile.in \
		"*.orig" \
		"*.rej" \
		"*.bak" \
		"*~" \
		".*.swp" \
	; do echo /$$x; done | \
	sed "s@^/`echo "$(srcdir)" | sed 's/\(.\)/[\1]/g'`/@/@" | \
	sed 's@/[.]/@/@g' | \
	LANG=C sort | uniq > $@.tmp && \
	mv $@.tmp $@;

all: $(srcdir)/.gitignore gitignore-recurse
gitignore-recurse:
	@if test "x$(SUBDIRS)" = "x$(DIST_SUBDIRS)"; then :; else \
		list='$(DIST_SUBDIRS)'; for subdir in $$list; do \
		  test "$$subdir" = . || (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) .gitignore); \
		done; \
	fi;
maintainer-clean-local: gitignore-clean
gitignore-clean:
	rm -f $(srcdir)/.gitignore
.PHONY: gitignore-clean gitignore-recurse

