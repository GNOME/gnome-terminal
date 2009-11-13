include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_MODULE = gnome-terminal
DOC_ENTITIES = legal.xml
DOC_INCLUDES = 
DOC_FIGURES = figures/gnome-terminal-default.png \
              figures/gnome-terminal-tabbed.png 
#	      figures/terminal-window.png   This image is not used.

DOC_LINGUAS = da de ca cs el en_GB es fr it ko oc pt_BR ru sv uk zh_CN

# Move to DOC_LINGUAS in transition to gnome-doc-utils

SUBDIRS = bg ro ja zh_TW

-include $(top_srcdir)/git.mk
