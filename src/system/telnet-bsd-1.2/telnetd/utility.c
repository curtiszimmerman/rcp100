/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <time.h>
#include <stdarg.h>
#include <sys/utsname.h>

/* this is ugly, but we need the declaration only in one file */
#define TELOPTS
#define TELCMDS
#define SLC_NAMES
#include <arpa/telnet.h>

#include "telnetd.h"

/*
 * utility functions performing io related tasks
 */

void
netoprintf(const char *fmt, ...)
{
   int len, maxsize;
   va_list ap;
   int done=0;

   while (!done) {
      maxsize = sizeof(netobuf) - (nfrontp - netobuf);

      if (maxsize <= 0) {
          netflush();
          maxsize = sizeof(netobuf) - (nfrontp - netobuf);
      }

      va_start(ap, fmt);
      len = vsnprintf(nfrontp, maxsize, fmt, ap);
      va_end(ap);

      if (len<0 || len>=maxsize) {
         /* didn't fit */
         netflush();
      }
      else {
         done = 1;
      }
   }
   nfrontp += len;
}

/*
 * ttloop
 *
 *	A small subroutine to flush the network output buffer, get some data
 * from the network, and pass it through the telnet state machine.  We
 * also flush the pty input buffer (by dropping its data) if it becomes
 * too full.
 */

void
ttloop(void)
{

    DIAG(TD_REPORT, netoprintf("td: ttloop\r\n"););

    if (nfrontp-nbackp) {
	netflush();
    }
    ncc = read(net, netibuf, sizeof(netibuf));
    if (ncc < 0) {
	syslog(LOG_INFO, "ttloop: read: %m\n");
	exit(1);
    } else if (ncc == 0) {
	syslog(LOG_INFO, "ttloop: peer died: EOF\n");
	exit(1);
    }
    DIAG(TD_REPORT, netoprintf("td: ttloop read %d chars\r\n", ncc););
    netip = netibuf;
    telrcv();			/* state machine */
    if (ncc > 0) {
	pfrontp = pbackp = ptyobuf;
	telrcv();
    }
}  /* end of ttloop */

/*
 * Check a descriptor to see if out of band data exists on it.
 */
int stilloob(int s)		/* socket number */
{
    static struct timeval timeout = { 0, 0 };
    fd_set	excepts;
    int value;

    do {
	FD_ZERO(&excepts);
	FD_SET(s, &excepts);
	value = select(s+1, (fd_set *)0, (fd_set *)0, &excepts, &timeout);
    } while ((value == -1) && (errno == EINTR));

    if (value < 0) {
	fatalperror(pty, "select");
    }
    if (FD_ISSET(s, &excepts)) {
	return 1;
    } else {
	return 0;
    }
}

void 	ptyflush(void)
{
	int n;

	if ((n = pfrontp - pbackp) > 0) {
		DIAG((TD_REPORT | TD_PTYDATA),
		     netoprintf("td: ptyflush %d chars\r\n", n););
		DIAG(TD_PTYDATA, printdata("pd", pbackp, n));
		n = write(pty, pbackp, n);
	}
	if (n < 0) {
		if (errno == EWOULDBLOCK || errno == EINTR)
			return;
		cleanup(0);
	}
	pbackp += n;
	if (pbackp == pfrontp)
		pbackp = pfrontp = ptyobuf;
}

/*
 * nextitem()
 *
 *	Return the address of the next "item" in the TELNET data
 * stream.  This will be the address of the next character if
 * the current address is a user data character, or it will
 * be the address of the character following the TELNET command
 * if the current address is a TELNET IAC ("I Am a Command")
 * character.
 */
static
char *
nextitem(char *current)
{
    if ((*current&0xff) != IAC) {
	return current+1;
    }
    switch (*(current+1)&0xff) {
    case DO:
    case DONT:
    case WILL:
    case WONT:
	return current+3;
    case SB:		/* loop forever looking for the SE */
	{
	    register char *look = current+2;

	    for (;;) {
		if ((*look++&0xff) == IAC) {
		    if ((*look++&0xff) == SE) {
			return look;
		    }
		}
	    }
	}
    default:
	return current+2;
    }
}  /* end of nextitem */


/*
 * netclear()
 *
 *	We are about to do a TELNET SYNCH operation.  Clear
 * the path to the network.
 *
 *	Things are a bit tricky since we may have sent the first
 * byte or so of a previous TELNET command into the network.
 * So, we have to scan the network buffer from the beginning
 * until we are up to where we want to be.
 *
 *	A side effect of what we do, just to keep things
 * simple, is to clear the urgent data pointer.  The principal
 * caller should be setting the urgent data pointer AFTER calling
 * us in any case.
 */
void netclear(void)
{
    register char *thisitem, *next;
    char *good;
#define	wewant(p)	((nfrontp > p) && ((*p&0xff) == IAC) && \
				((*(p+1)&0xff) != EC) && ((*(p+1)&0xff) != EL))

#if	defined(ENCRYPT)
    thisitem = nclearto > netobuf ? nclearto : netobuf;
#else
    thisitem = netobuf;
#endif

    while ((next = nextitem(thisitem)) <= nbackp) {
	thisitem = next;
    }

    /* Now, thisitem is first before/at boundary. */

#if	defined(ENCRYPT)
    good = nclearto > netobuf ? nclearto : netobuf;
#else
    good = netobuf;	/* where the good bytes go */
#endif

    while (nfrontp > thisitem) {
	if (wewant(thisitem)) {
	    int length;

	    next = thisitem;
	    do {
		next = nextitem(next);
	    } while (wewant(next) && (nfrontp > next));
	    length = next-thisitem;
	    bcopy(thisitem, good, length);
	    good += length;
	    thisitem = next;
	} else {
	    thisitem = nextitem(thisitem);
	}
    }

    nbackp = netobuf;
    nfrontp = good;		/* next byte to be sent */
    neturg = 0;
}  /* end of netclear */

/*
 *  netflush
 *		Send as much data as possible to the network,
 *	handling requests for urgent data.
 */
extern int not42;
void
netflush(void)
{
    int n;

    if ((n = nfrontp - nbackp) > 0) {
	DIAG(TD_REPORT,
	    { netoprintf("td: netflush %d chars\r\n", n);
	      n = nfrontp - nbackp;  /* update count */
	    });
#if	defined(ENCRYPT)
	if (encrypt_output) {
		char *s = nclearto ? nclearto : nbackp;
		if (nfrontp - s > 0) {
			(*encrypt_output)((unsigned char *)s, nfrontp-s);
			nclearto = nfrontp;
		}
	}
#endif
	/*
	 * if no urgent data, or if the other side appears to be an
	 * old 4.2 client (and thus unable to survive TCP urgent data),
	 * write the entire buffer in non-OOB mode.
	 */
	if ((neturg == 0) || (not42 == 0)) {
	    n = write(net, nbackp, n);	/* normal write */
	} else {
	    n = neturg - nbackp;
	    /*
	     * In 4.2 (and 4.3) systems, there is some question about
	     * what byte in a sendOOB operation is the "OOB" data.
	     * To make ourselves compatible, we only send ONE byte
	     * out of band, the one WE THINK should be OOB (though
	     * we really have more the TCP philosophy of urgent data
	     * rather than the Unix philosophy of OOB data).
	     */
	    if (n > 1) {
		n = send(net, nbackp, n-1, 0);	/* send URGENT all by itself */
	    } else {
		n = send(net, nbackp, n, MSG_OOB);	/* URGENT data */
	    }
	}
    }
    if (n < 0) {
	if (errno == EWOULDBLOCK || errno == EINTR)
		return;
	cleanup(0);
    }
    nbackp += n;
#if	defined(ENCRYPT)
    if (nbackp > nclearto)
	nclearto = 0;
#endif
    if (nbackp >= neturg) {
	neturg = 0;
    }
    if (nbackp == nfrontp) {
	nbackp = nfrontp = netobuf;
#if	defined(ENCRYPT)
	nclearto = 0;
#endif
    }
    return;
}  /* end of netflush */


/*
 * writenet
 *
 * Just a handy little function to write a bit of raw data to the net.
 * It will force a transmit of the buffer if necessary
 *
 * arguments
 *    ptr - A pointer to a character string to write
 *    len - How many bytes to write
 */
void writenet(register unsigned char *ptr, register int len)
{
	/* flush buffer if no room for new data) */
	if ((&netobuf[BUFSIZ] - nfrontp) < len) {
		/* if this fails, don't worry, buffer is a little big */
		netflush();
	}

	bcopy(ptr, nfrontp, len);
	nfrontp += len;

}  /* end of writenet */


/*
 * miscellaneous functions doing a variety of little jobs follow ...
 */


void
fatal(int f, const char *msg)
{
	char buf[BUFSIZ];

	(void) snprintf(buf, sizeof(buf), "telnetd: %s.\r\n", msg);
#if	defined(ENCRYPT)
	if (encrypt_output) {
		/*
		 * Better turn off encryption first....
		 * Hope it flushes...
		 */
		encrypt_send_end();
		netflush();
	}
#endif
	(void) write(f, buf, (int)strlen(buf));
	sleep(1);	/*XXX*/
	exit(1);
}

void
fatalperror(int f, const char *msg)
{
	char buf[BUFSIZ];
	snprintf(buf, sizeof(buf), "%s: %s\r\n", msg, strerror(errno));
	fatal(f, buf);
}

char editedhost[32];
struct utsname kerninfo;

void
edithost(const char *pat, const char *host)
{
	char *res = editedhost;

	uname(&kerninfo);

	if (!pat)
		pat = "";
	while (*pat) {
		switch (*pat) {

		case '#':
			if (*host)
				host++;
			break;

		case '@':
			if (*host)
				*res++ = *host++;
			break;

		default:
			*res++ = *pat;
			break;
		}
		if (res == &editedhost[sizeof editedhost - 1]) {
			*res = '\0';
			return;
		}
		pat++;
	}
	if (*host)
		(void) strncpy(res, host,
				sizeof editedhost - (res - editedhost) -1);
	else
		*res = '\0';
	editedhost[sizeof editedhost - 1] = '\0';
}

static char *putlocation;

static
void
putstr(const char *s)
{
    while (*s) putchr(*s++);
}

void putchr(int cc)
{
	*putlocation++ = cc;
}

static char fmtstr[] = { "%H:%M on %A, %d %B %Y" };

void putf(const char *cp, char *where)
{
	char *slash;
	time_t t;
	char db[100];

	if (where)
	putlocation = where;

	while (*cp) {
		if (*cp != '%') {
			putchr(*cp++);
			continue;
		}
		switch (*++cp) {

		case 't':
			slash = strrchr(line, '/');
			if (slash == NULL)
				putstr(line);
			else
				putstr(slash+1);
			break;

		case 'h':
			putstr(editedhost);
			break;

		case 'd':
			(void)time(&t);
			(void)strftime(db, sizeof(db), fmtstr, localtime(&t));
			putstr(db);
			break;

		case '%':
			putchr('%');
			break;

		case 'D':
			{
				char	buff[128];

				if (getdomainname(buff,sizeof(buff)) < 0
					|| buff[0] == '\0'
					|| strcmp(buff, "(none)") == 0)
					break;
				putstr(buff);
			}
			break;

		case 'i':
			{
				char buff[3];
				FILE *fp;
				int p, c;

				if ((fp = fopen(ISSUE_FILE, "r")) == NULL)
					break;
				p = '\n';
				while ((c = fgetc(fp)) != EOF) {
					if (p == '\n' && c == '#') {
						do {
							c = fgetc(fp);
						} while (c != EOF && c != '\n');
						continue;
					} else if (c == '%') {
						buff[0] = c;
						c = fgetc(fp);
						if (c == EOF) break;
						buff[1] = c;
						buff[2] = '\0';
						putf(buff, NULL);
					} else {
						if (c == '\n') putchr('\r');
						putchr(c);
						p = c;
					}
				};
				(void) fclose(fp);
			}
			return; /* ignore remainder of the banner string */
			/*NOTREACHED*/

		case 's':
			putstr(kerninfo.sysname);
			break;

		case 'm':
			putstr(kerninfo.machine);
			break;

		case 'r':
			putstr(kerninfo.release);
			break;

		case 'v':
			putstr(kerninfo.version);
			break;
		}
		cp++;
	}
}

/*
 * Print telnet options and commands in plain text, if possible.
 */
void
printoption(const char *fmt, int option)
{
	if (TELOPT_OK(option))
		netoprintf("%s %s\r\n", fmt, TELOPT(option));
	else if (TELCMD_OK(option))
		netoprintf("%s %s\r\n", fmt, TELCMD(option));
	else
		netoprintf("%s %d\r\n", fmt, option);
}

/* direction: '<' or '>' */
/* pointer: where suboption data sits */
/* length: length of suboption data */
void
printsub(char direction, unsigned char *pointer, int length)
{
    register int i = -1;

        if (!(diagnostic & TD_OPTIONS))
		return;

	if (direction) {
	    netoprintf("td: %s suboption ",
		       direction == '<' ? "recv" : "send");
	    if (length >= 3) {
		register int j;

		i = pointer[length-2];
		j = pointer[length-1];

		if (i != IAC || j != SE) {
		    netoprintf("(terminated by ");
		    if (TELOPT_OK(i))
			netoprintf("%s ", TELOPT(i));
		    else if (TELCMD_OK(i))
			netoprintf("%s ", TELCMD(i));
		    else
			netoprintf("%d ", i);
		    if (TELOPT_OK(j))
			netoprintf("%s", TELOPT(j));
		    else if (TELCMD_OK(j))
			netoprintf("%s", TELCMD(j));
		    else
			netoprintf("%d", j);
		    netoprintf(", not IAC SE!) ");
		}
	    }
	    length -= 2;
	}
	if (length < 1) {
	    netoprintf("(Empty suboption?)");
	    return;
	}
	switch (pointer[0]) {
	case TELOPT_TTYPE:
	    netoprintf("TERMINAL-TYPE ");
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		netoprintf("IS \"%.*s\"", length-2, (char *)pointer+2);
		break;
	    case TELQUAL_SEND:
		netoprintf("SEND");
		break;
	    default:
		netoprintf("- unknown qualifier %d (0x%x).",
				pointer[1], pointer[1]);
	    }
	    break;
	case TELOPT_TSPEED:
	    netoprintf("TERMINAL-SPEED");
	    if (length < 2) {
		netoprintf(" (empty suboption?)");
		break;
	    }
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		netoprintf(" IS %.*s", length-2, (char *)pointer+2);
		break;
	    default:
		if (pointer[1] == 1)
		    netoprintf(" SEND");
		else
		    netoprintf(" %d (unknown)", pointer[1]);
		for (i = 2; i < length; i++) {
		    netoprintf(" ?%d?", pointer[i]);
		}
		break;
	    }
	    break;

	case TELOPT_LFLOW:
	    netoprintf("TOGGLE-FLOW-CONTROL");
	    if (length < 2) {
		netoprintf(" (empty suboption?)");
		break;
	    }
	    switch (pointer[1]) {
	    case 0:
		netoprintf(" OFF"); break;
	    case 1:
		netoprintf(" ON"); break;
	    default:
		netoprintf(" %d (unknown)", pointer[1]);
	    }
	    for (i = 2; i < length; i++) {
		netoprintf(" ?%d?", pointer[i]);
	    }
	    break;

	case TELOPT_NAWS:
	    netoprintf("NAWS");
	    if (length < 2) {
		netoprintf(" (empty suboption?)");
		break;
	    }
	    if (length == 2) {
		netoprintf(" ?%d?", pointer[1]);
		break;
	    }
	    netoprintf(" %d %d (%d)",
		pointer[1], pointer[2],
		(int)((((unsigned int)pointer[1])<<8)|((unsigned int)pointer[2])));
	    if (length == 4) {
		netoprintf(" ?%d?", pointer[3]);
		break;
	    }
	    netoprintf(" %d %d (%d)",
		pointer[3], pointer[4],
		(int)((((unsigned int)pointer[3])<<8)|((unsigned int)pointer[4])));
	    for (i = 5; i < length; i++) {
		netoprintf(" ?%d?", pointer[i]);
	    }
	    break;

	case TELOPT_LINEMODE:
	    netoprintf("LINEMODE ");
	    if (length < 2) {
		netoprintf(" (empty suboption?)");
		break;
	    }
	    switch (pointer[1]) {
	    case WILL:
		netoprintf("WILL ");
		goto common;
	    case WONT:
		netoprintf("WONT ");
		goto common;
	    case DO:
		netoprintf("DO ");
		goto common;
	    case DONT:
		netoprintf("DONT ");
	    common:
		if (length < 3) {
		    netoprintf("(no option?)");
		    break;
		}
		switch (pointer[2]) {
		case LM_FORWARDMASK:
		    netoprintf("Forward Mask");
		    for (i = 3; i < length; i++) {
			netoprintf(" %x", pointer[i]);
		    }
		    break;
		default:
		    netoprintf("%d (unknown)", pointer[2]);
		    for (i = 3; i < length; i++) {
			netoprintf(" %d", pointer[i]);
		    }
		    break;
		}
		break;

	    case LM_SLC:
		netoprintf("SLC");
		for (i = 2; i < length - 2; i += 3) {
		    if (SLC_NAME_OK(pointer[i+SLC_FUNC]))
			netoprintf(" %s", SLC_NAME(pointer[i+SLC_FUNC]));
		    else
			netoprintf(" %d", pointer[i+SLC_FUNC]);
		    switch (pointer[i+SLC_FLAGS]&SLC_LEVELBITS) {
		    case SLC_NOSUPPORT:
			netoprintf(" NOSUPPORT"); break;
		    case SLC_CANTCHANGE:
			netoprintf(" CANTCHANGE"); break;
		    case SLC_VARIABLE:
			netoprintf(" VARIABLE"); break;
		    case SLC_DEFAULT:
			netoprintf(" DEFAULT"); break;
		    }
		    netoprintf("%s%s%s",
			pointer[i+SLC_FLAGS]&SLC_ACK ? "|ACK" : "",
			pointer[i+SLC_FLAGS]&SLC_FLUSHIN ? "|FLUSHIN" : "",
			pointer[i+SLC_FLAGS]&SLC_FLUSHOUT ? "|FLUSHOUT" : "");
		    if (pointer[i+SLC_FLAGS]& ~(SLC_ACK|SLC_FLUSHIN|
						SLC_FLUSHOUT| SLC_LEVELBITS)) {
			netoprintf("(0x%x)", pointer[i+SLC_FLAGS]);
		    }
		    netoprintf(" %d;", pointer[i+SLC_VALUE]);
		    if ((pointer[i+SLC_VALUE] == IAC) &&
			(pointer[i+SLC_VALUE+1] == IAC))
				i++;
		}
		for (; i < length; i++) {
		    netoprintf(" ?%d?", pointer[i]);
		}
		break;

	    case LM_MODE:
		netoprintf("MODE ");
		if (length < 3) {
		    netoprintf ("(no mode?)");
		    break;
		}
		{
		    char tbuf[32];
		    snprintf(tbuf, sizeof(tbuf), "%s%s%s%s%s",
			pointer[2]&MODE_EDIT ? "|EDIT" : "",
			pointer[2]&MODE_TRAPSIG ? "|TRAPSIG" : "",
			pointer[2]&MODE_SOFT_TAB ? "|SOFT_TAB" : "",
			pointer[2]&MODE_LIT_ECHO ? "|LIT_ECHO" : "",
			pointer[2]&MODE_ACK ? "|ACK" : "");
		    netoprintf("%s", tbuf[1] ? &tbuf[1] : "0");
		}
		if (pointer[2]&~(MODE_EDIT|MODE_TRAPSIG|MODE_ACK)) {
		    netoprintf(" (0x%x)", pointer[2]);
		}
		for (i = 3; i < length; i++) {
		    netoprintf(" ?0x%x?", pointer[i]);
		}
		break;
	    default:
		netoprintf("%d (unknown)", pointer[1]);
		for (i = 2; i < length; i++) {
		    netoprintf(" %d", pointer[i]);
		}
	    }
	    break;

	case TELOPT_STATUS: {
	    const char *cp;
	    register int j, k;

	    netoprintf("STATUS");

	    switch (pointer[1]) {
	    default:
		if (pointer[1] == TELQUAL_SEND)
		    netoprintf(" SEND");
		else
		    netoprintf(" %d (unknown)", pointer[1]);
		for (i = 2; i < length; i++) {
		    netoprintf(" ?%d?", pointer[i]);
		}
		break;
	    case TELQUAL_IS:
		netoprintf(" IS\r\n");

		for (i = 2; i < length; i++) {
		    switch(pointer[i]) {
		    case DO:	cp = "DO"; goto common2;
		    case DONT:	cp = "DONT"; goto common2;
		    case WILL:	cp = "WILL"; goto common2;
		    case WONT:	cp = "WONT"; goto common2;
		    common2:
			i++;
			if (TELOPT_OK((int)pointer[i]))
			    netoprintf(" %s %s", cp, TELOPT(pointer[i]));
			else
			    netoprintf(" %s %d", cp, pointer[i]);

			netoprintf("\r\n");
			break;

		    case SB:
			netoprintf(" SB ");
			i++;
			j = k = i;
			while (j < length) {
			    if (pointer[j] == SE) {
				if (j+1 == length)
				    break;
				if (pointer[j+1] == SE)
				    j++;
				else
				    break;
			    }
			    pointer[k++] = pointer[j++];
			}
			printsub(0, &pointer[i], k - i);
			if (i < length) {
			    netoprintf(" SE");
			    i = j;
			} else
			    i = j - 1;

			netoprintf("\r\n");

			break;

		    default:
			netoprintf(" %d", pointer[i]);
			break;
		    }
		}
		break;
	    }
	    break;
	  }

	case TELOPT_XDISPLOC:
	    netoprintf("X-DISPLAY-LOCATION ");
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		netoprintf("IS \"%.*s\"", length-2, (char *)pointer+2);
		break;
	    case TELQUAL_SEND:
		netoprintf("SEND");
		break;
	    default:
		netoprintf("- unknown qualifier %d (0x%x).",
				pointer[1], pointer[1]);
	    }
	    break;

	case TELOPT_ENVIRON:
	    netoprintf("ENVIRON ");
	    switch (pointer[1]) {
	    case TELQUAL_IS:
		netoprintf("IS ");
		goto env_common;
	    case TELQUAL_SEND:
		netoprintf("SEND ");
		goto env_common;
	    case TELQUAL_INFO:
		netoprintf("INFO ");
	    env_common:
		{
		    register int noquote = 2;
		    for (i = 2; i < length; i++ ) {
			switch (pointer[i]) {
			case ENV_VAR:
			    if (pointer[1] == TELQUAL_SEND)
				goto def_case;
			    netoprintf("\" VAR " + noquote);
			    noquote = 2;
			    break;

			case ENV_VALUE:
			    netoprintf("\" VALUE " + noquote);
			    noquote = 2;
			    break;

			case ENV_ESC:
			    netoprintf("\" ESC " + noquote);
			    noquote = 2;
			    break;

			default:
			def_case:
			    if (isprint(pointer[i]) && pointer[i] != '"') {
				if (noquote) {
				    netoprintf("\"");
				    noquote = 0;
				}
				netoprintf("%c", pointer[i]);
			    } else {
				netoprintf("\" %03o " + noquote,
							pointer[i]);
				noquote = 2;
			    }
			    break;
			}
		    }
		    if (!noquote)
			netoprintf("\"");
		    break;
		}
	    }
	    break;

#if	defined(ENCRYPT)
	case TELOPT_ENCRYPT:
	    netoprintf("ENCRYPT");
	    if (length < 2) {
		netoprintf(" (empty suboption?)");
		break;
	    }
	    switch (pointer[1]) {
	    case ENCRYPT_START:
		netoprintf(" START");
		break;

	    case ENCRYPT_END:
		netoprintf(" END");
		break;

	    case ENCRYPT_REQSTART:
		netoprintf(" REQUEST-START");
		break;

	    case ENCRYPT_REQEND:
		netoprintf(" REQUEST-END");
		break;

	    case ENCRYPT_IS:
	    case ENCRYPT_REPLY:
		netoprintf(" %s ", (pointer[1] == ENCRYPT_IS) ?
							"IS" : "REPLY");
		if (length < 3) {
		    netoprintf(" (partial suboption?)");
		    break;
		}
		if (ENCTYPE_NAME_OK(pointer[2]))
		    netoprintf("%s ", ENCTYPE_NAME(pointer[2]));
		else
		    netoprintf(" %d (unknown)", pointer[2]);

		encrypt_printsub(&pointer[1], length - 1, buf, sizeof(buf));
		netoprintf("%s", buf);
		break;

	    case ENCRYPT_SUPPORT:
		i = 2;
		netoprintf(" SUPPORT ");
		while (i < length) {
		    if (ENCTYPE_NAME_OK(pointer[i]))
			netoprintf("%s ", ENCTYPE_NAME(pointer[i]));
		    else
			netoprintf("%d ", pointer[i]);
		    i++;
		}
		break;

	    case ENCRYPT_ENC_KEYID:
		netoprintf(" ENC_KEYID", pointer[1]);
		goto encommon;

	    case ENCRYPT_DEC_KEYID:
		netoprintf(" DEC_KEYID", pointer[1]);
		goto encommon;

	    default:
		netoprintf(" %d (unknown)", pointer[1]);
	    encommon:
		for (i = 2; i < length; i++) {
		    netoprintf(" %d", pointer[i]);
		}
		break;
	    }
	    break;
#endif

	default:
	    if (TELOPT_OK(pointer[0]))
	        netoprintf("%s (unknown)", TELOPT(pointer[0]));
	    else
	        netoprintf("%d (unknown)", pointer[i]);
	    for (i = 1; i < length; i++) {
		netoprintf(" %d", pointer[i]);
	    }
	    break;
	}
	netoprintf("\r\n");
}

/*
 * Dump a data buffer in hex and ascii to the output data stream.
 */
void
printdata(const char *tag, const char *ptr, int cnt)
{
	register int i;
	char xbuf[30];

	while (cnt) {
		/* flush net output buffer if no room for new data) */
		if ((&netobuf[BUFSIZ] - nfrontp) < 80) {
			netflush();
		}

		/* add a line of output */
		netoprintf("%s: ", tag);
		for (i = 0; i < 20 && cnt; i++) {
			netoprintf("%02x", *ptr);
			if (isprint(*ptr)) {
				xbuf[i] = *ptr;
			} else {
				xbuf[i] = '.';
			}
			if (i % 2) {
				netoprintf(" ");
			}
			cnt--;
			ptr++;
		}
		xbuf[i] = '\0';
		netoprintf(" %s\r\n", xbuf );
	}
}
