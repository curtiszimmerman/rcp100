#
# Copyright (c) 2004 Thorsten Kukuk <kukuk@suse.de>
#
AUTOMAKE_OPTIONS = 1.8 gnits

localedir = $(datadir)/locale

AM_CFLAGS = @EXTRA_CFLAGS@ -DENV_HACK -DUSE_TERMIO -DKLUDGELINEMODE -D_GNU_SOURCE @PIE_CFLAGS@
DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
LDADD = @PIE_LDFLAGS@ -lncurses
INCLUDES = -I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/intl\
		-I$(srcdir)

CLEANFILES = *~

EXTRA_DIST = ${MANS}

noinst_HEADERS = defines.h externs.h ring.h telnet_locl.h types.h\
		misc-proto.h

man_MANS = telnet.1

bin_PROGRAMS = telnet

telnet_SOURCES = main.c ring.c telnet.c tn3270.c commands.c\
		network.c sys_bsd.c terminal.c utilities.c genget.c

