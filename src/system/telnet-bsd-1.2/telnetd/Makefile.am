#
# Copyright (c) 2004 Thorsten Kukuk <kukuk@suse.de>
#
AUTOMAKE_OPTIONS = 1.8 gnits

localedir = $(datadir)/locale

AM_CFLAGS = @EXTRA_CFLAGS@ -DKLUDGELINEMODE \
	'-DISSUE_FILE="/etc/issue.net"' -DPARANOID_TTYS @PIE_CFLAGS@

DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\"
LDADD = -lutil @PIE_LDFLAGS@ -lncurses
INCLUDES = -I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/intl

CLEANFILES = *~

EXTRA_DIST = ${MANS}

noinst_HEADERS = pathnames.h defs.h ext.h telnetd.h setproctitle.h

man_MANS = telnetd.8 in.telnetd.8 issue.net.5

sbin_PROGRAMS = in.telnetd

in_telnetd_SOURCES = global.c state.c sys_term.c telnetd.c\
		termstat.c utility.c setproctitle.c
