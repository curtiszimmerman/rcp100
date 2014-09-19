//Modified for Rcp Project
/*
 * Copyright (c) 1983 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

char copyright[] =
  "@(#) Copyright (c) 1983 Regents of the University of California.\n"
  "All rights reserved.\n";

/*
 * From: @(#)tftpd.c	5.13 (Berkeley) 2/26/91
 */
char rcsid[] = 
  "$Id: tftpd.c,v 1.20 2000/07/29 18:37:21 dholland Exp $";

/*
 * Trivial file transfer protocol server.
 *
 * This version includes many modifications by Jim Guyton <guyton@rand-unix>
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
/* #include <netinet/ip.h> <--- unused? */
#include <arpa/tftp.h>
#include <netdb.h>

#include <setjmp.h>
#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "tftpsubs.h"

#include "../version.h"
//RCP
#include "../../../include/librcp.h"
//RCP
#define	TIMEOUT		5

struct formats;
static void tftp(struct tftphdr *tp, int size);
static void nak(int error);
static void sendfile(struct formats *pf);
static void recvfile(struct formats *pf);

static int validate_access(const char *, int);

static int		peer;
static int		rexmtval = TIMEOUT;
static int		maxtimeout = 5*TIMEOUT;

static char		buf[PKTSIZE];
static char		ackbuf[PKTSIZE];
static struct		sockaddr_in from;
static socklen_t	fromlen;

#define MAXARG		4
static char		*dirs[MAXARG+1];

int
main(int ac, char **av)
{
	struct sockaddr_in sn;
	socklen_t snsize;
	int dobind=1;

	register struct tftphdr *tp;
	register int n = 0;
	int on = 1;

	ac--; av++;
	if (ac==0) dirs[0] = "/tftpboot";  /* default directory */
	while (ac-- > 0 && n < MAXARG)
		dirs[n++] = *av++;
	openlog("tftpd", LOG_PID, LOG_DAEMON);
	if (ioctl(0, FIONBIO, &on) < 0) {
		syslog(LOG_ERR, "ioctl(FIONBIO): %m\n");
		exit(1);
	}
	fromlen = sizeof(from);
	n = recvfrom(0, buf, sizeof (buf), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		syslog(LOG_ERR, "recvfrom: %m\n");
		exit(1);
	}
	/*
	 * Now that we have read the message out of the UDP
	 * socket, we fork and exit.  Thus, inetd will go back
	 * to listening to the tftp port, and the next request
	 * to come in will start up a new instance of tftpd.
	 *
	 * We do this so that inetd can run tftpd in "wait" mode.
	 * The problem with tftpd running in "nowait" mode is that
	 * inetd may get one or more successful "selects" on the
	 * tftp port before we do our receive, so more than one
	 * instance of tftpd may be started up.  Worse, if tftpd
	 * were to break before doing the above "recvfrom", inetd 
	 * would spawn endless instances, clogging the system.
	 */
	{
		int pid = -1;
		int i;
		socklen_t k;

		for (i = 1; i < 20; i++) {
		    pid = fork();
		    if (pid < 0) {
				sleep(i);
				/*
				 * flush out to most recently sent request.
				 *
				 * This may drop some request, but those
				 * will be resent by the clients when
				 * they timeout.  The positive effect of
				 * this flush is to (try to) prevent more
				 * than one tftpd being started up to service
				 * a single request from a single client.
				 */
				k = sizeof(from);
				i = recvfrom(0, buf, sizeof (buf), 0,
				    (struct sockaddr *)&from, &k);
				if (i > 0) {
					n = i;
					fromlen = k;
				}
		    } else {
				break;
		    }
		}
		if (pid < 0) {
			syslog(LOG_ERR, "fork: %m\n");
			exit(1);
		} else if (pid != 0) {
			exit(0);
		}
	}
	if (!getuid() || !geteuid()) {
		struct passwd *pwd = getpwnam("nobody");
		if (pwd) {
			initgroups(pwd->pw_name, pwd->pw_gid);
			setgid(pwd->pw_gid);
			setuid(pwd->pw_uid);
		}
		seteuid(0);   /* this should fail */
		if (!getuid() || !geteuid()) {
			syslog(LOG_CRIT, "can't drop root privileges");
			exit(1);
		}
	}
	from.sin_family = AF_INET;
	alarm(0);

	/*
	 * Get the local address of the port inetd is using, so we can
	 * send from the same address. This will not make multihomed
	 * usage work *transparently*, but it at least gives a chance
	 * - you can have inetd bind a separate tftpd port for each 
	 * interface.
	 */
	snsize = sizeof(sn);
	if (getsockname(0, (struct sockaddr *)&sn, &snsize)<0 ||
	    sn.sin_addr.s_addr == INADDR_ANY) {
		dobind = 0;
	}
	sn.sin_port = 0;

	close(0);
	close(1);
	peer = socket(AF_INET, SOCK_DGRAM, 0);
	if (peer < 0) {
		syslog(LOG_ERR, "socket: %m\n");
		exit(1);
	}

	if (dobind && bind(peer, (struct sockaddr *)&sn, snsize) < 0) {
		syslog(LOG_ERR, "bind: %m\n");
		exit(1);
	}

	if (connect(peer, (struct sockaddr *)&from, sizeof(from)) < 0) {
		syslog(LOG_ERR, "connect: %m\n");
		exit(1);
	}
	tp = (struct tftphdr *)buf;
	tp->th_opcode = ntohs(tp->th_opcode);
	if (tp->th_opcode == RRQ || tp->th_opcode == WRQ)
		tftp(tp, n);
	exit(1);
}

struct formats {
	const char *f_mode;
	int (*f_validate)(const char *, int);
	void (*f_send)(struct formats *);
	void (*f_recv)(struct formats *);
	int f_convert;
} formats[] = {
	{ "netascii",	validate_access,	sendfile, recvfile, 1 },
	{ "octet",	validate_access,	sendfile, recvfile, 0 },
     /* { "mail",	validate_user,		sendmail, recvmail, 1 }, */
	{ 0,0,0,0,0 }
};

/*
 * Handle initial connection protocol.
 */
static void
tftp(struct tftphdr *tp, int size)
{
	register char *cp;
	int first = 1, ecode;
	register struct formats *pf;
	char *filename, *mode = NULL;

	filename = cp = tp->th_stuff;
again:
	while (cp < buf + size) {
		if (*cp == '\0')
			break;
		cp++;
	}
	if (*cp != '\0') {
		nak(EBADOP);
		exit(1);
	}
	if (first) {
		mode = ++cp;
		first = 0;
		goto again;
	}
	for (cp = mode; *cp; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	for (pf = formats; pf->f_mode; pf++)
		if (strcmp(pf->f_mode, mode) == 0)
			break;
	if (pf->f_mode == 0) {
		nak(EBADOP);
		exit(1);
	}
	
//RCP
	// initialize shared memory
	RcpShm *shm;
	if ((shm = rcpShmemInit(RCP_PROC_CLI)) == NULL) {
		fprintf(stderr, "%%Error: cannot initialize memory, exiting...\n");
		exit(1);
	}
	
	// check if tftpd service is enabled
	if (!(shm->config.services & RCP_SERVICE_TFTP))
		exit(1);
		
//RCP
	ecode = (*pf->f_validate)(filename, tp->th_opcode);
	if (ecode) {
		nak(ecode);
		exit(1);
	}
	if (tp->th_opcode == WRQ)
;//RCP - write operation disabled		
//		(*pf->f_recv)(pf);
//RCP
	else
		(*pf->f_send)(pf);
	exit(0);
}


FILE *file;

/*
 * Validate file access.  Since we
 * have no uid or gid, for now require
 * file to exist and be publicly
 * readable/writable.
 * If we were invoked with arguments
 * from inetd then the file must also be
 * in one of the given directory prefixes.
 * Note also, full path name must be
 * given as we have no login directory.
 */
static
int
validate_access(const char *filename, int mode)
{
	struct stat stbuf;
	int	fd;
	const char *cp;
	char **dirp;

	syslog(LOG_NOTICE, "tftpd: trying to get file: %s\n", filename);

	if (*filename != '/') {
		syslog(LOG_NOTICE, "tftpd: serving file from %s\n", dirs[0]);
		chdir(dirs[0]);
	} else {
		for (dirp = dirs; *dirp; dirp++)
			if (strncmp(filename, *dirp, strlen(*dirp)) == 0)
				break;
		if (*dirp==0 && dirp!=dirs)
			return (EACCESS);
	}
	/*
	 * prevent tricksters from getting around the directory restrictions
	 */
	if (!strncmp(filename, "../", 3)) {
		syslog(LOG_WARNING, "tftpd: Blocked illegal request for %s\n",
		       filename);
		return EACCESS;
	}
	for (cp = filename + 1; *cp; cp++) {
		if (*cp == '.' && strncmp(cp-1, "/../", 4) == 0) {
			syslog(LOG_WARNING, 
			       "tftpd: Blocked illegal request for %s\n",
			       filename);
			return(EACCESS);
		}
	}
	if (stat(filename, &stbuf) < 0)
		return (errno == ENOENT ? ENOTFOUND : EACCESS);
#if 0
	/*
	 * The idea is that symlinks are dangerous. However, a symlink
	 * in the tftp area has to have been put there by root, and it's
	 * not part of the philosophy of Unix to keep root from shooting
	 * itself in the foot if it tries to. So basically we assume if
	 * there are symlinks they're there on purpose and not pointing
	 * to /etc/passwd or /tmp or other dangerous places.
	 *
	 * Note if this gets turned on the stat above needs to be made
	 * an lstat, or the check is useless.
	 */
	/* symlinks prohibited */
	if (S_ISLNK(stbuf.st_mode)) {
		return (EACCESS);
	}
#endif
	if (mode == RRQ) {
		if ((stbuf.st_mode & S_IROTH) == 0)
			return (EACCESS);
	} else {
		if ((stbuf.st_mode & S_IWOTH) == 0)
			return (EACCESS);
	}
	fd = open(filename, mode == RRQ ? O_RDONLY : O_WRONLY|O_TRUNC);
	if (fd < 0)
		return (errno + 100);
	file = fdopen(fd, (mode == RRQ)? "r":"w");
	if (file == NULL) {
		return errno+100;
	}
	return (0);
}

int	timeout;
sigjmp_buf	timeoutbuf;

static
void
timer(int signum)
{
	(void)signum;

	timeout += rexmtval;
	if (timeout >= maxtimeout)
		exit(1);
	siglongjmp(timeoutbuf, 1);
}

/*
 * Send the requested file.
 */
static void
sendfile(struct formats *pf)
{
	struct tftphdr *dp;
	register struct tftphdr *ap;    /* ack packet */
	volatile u_int16_t block = 1;
	int size, n;

	mysignal(SIGALRM, timer);
	dp = r_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		size = readit(file, &dp, pf->f_convert);
		if (size < 0) {
			nak(errno + 100);
			goto abort;
		}
		dp->th_opcode = htons((u_short)DATA);
		dp->th_block = htons((u_short)block);
		timeout = 0;
		(void) sigsetjmp(timeoutbuf, 1);

send_data:
		if (send(peer, dp, size + 4, 0) != size + 4) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		read_ahead(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);        /* read the ack */
			n = recv(peer, ackbuf, sizeof (ackbuf), 0);
			alarm(0);
			if (n < 0) {
				syslog(LOG_ERR, "tftpd: read: %m\n");
				goto abort;
			}
			ap->th_opcode = ntohs((u_short)ap->th_opcode);
			ap->th_block = ntohs((u_short)ap->th_block);

			if (ap->th_opcode == ERROR)
				goto abort;
			
			if (ap->th_opcode == ACK) {
				if (ap->th_block == block) {
					break;
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (ap->th_block == (block -1)) {
					goto send_data;
				}
			}

		}
		block++;
	} while (size == SEGSIZE);
abort:
	(void) fclose(file);
}

static
void
justquit(int signum)
{
	(void)signum;
	exit(0);
}


/*
 * Receive a file.
 */
static void
recvfile(struct formats *pf)
{
	struct tftphdr *dp;
	struct tftphdr *ap;    /* ack buffer */
	volatile u_int16_t block = 0;
	int n, size;

	mysignal(SIGALRM, timer);
	dp = w_init();
	ap = (struct tftphdr *)ackbuf;
	do {
		timeout = 0;
		ap->th_opcode = htons((u_short)ACK);
		ap->th_block = htons((u_short)block);
		block++;
		(void) sigsetjmp(timeoutbuf, 1);
send_ack:
		if (send(peer, ackbuf, 4, 0) != 4) {
			syslog(LOG_ERR, "tftpd: write: %m\n");
			goto abort;
		}
		write_behind(file, pf->f_convert);
		for ( ; ; ) {
			alarm(rexmtval);
			n = recv(peer, dp, PKTSIZE, 0);
			alarm(0);
			if (n < 0) {            /* really? */
				syslog(LOG_ERR, "tftpd: read: %m\n");
				goto abort;
			}
			dp->th_opcode = ntohs((u_short)dp->th_opcode);
			dp->th_block = ntohs((u_short)dp->th_block);
			if (dp->th_opcode == ERROR)
				goto abort;
			if (dp->th_opcode == DATA) {
				if (dp->th_block == block) {
					break;   /* normal */
				}
				/* Re-synchronize with the other side */
				(void) synchnet(peer);
				if (dp->th_block == (block-1))
					goto send_ack;          /* rexmit */
			}
		}
		/*  size = write(file, dp->th_data, n - 4); */
		size = writeit(file, &dp, n - 4, pf->f_convert);
		if (size != (n-4)) {                    /* ahem */
			if (size < 0) nak(errno + 100);
			else nak(ENOSPACE);
			goto abort;
		}
	} while (size == SEGSIZE);
	write_behind(file, pf->f_convert);
	(void) fclose(file);            /* close data file */

	ap->th_opcode = htons((u_short)ACK);    /* send the "final" ack */
	ap->th_block = htons((u_short)(block));
	(void) send(peer, ackbuf, 4, 0);

	mysignal(SIGALRM, justquit);      /* just quit on timeout */
	alarm(rexmtval);
	n = recv(peer, buf, sizeof (buf), 0); /* normally times out and quits */
	alarm(0);
	if (n >= 4 &&                   /* if read some data */
	    dp->th_opcode == DATA &&    /* and got a data block */
	    block == dp->th_block) {	/* then my last ack was lost */
		(void) send(peer, ackbuf, 4, 0);     /* resend final ack */
	}
abort:
	return;
}

struct errmsg {
	int e_code;
	const char *e_msg;
} errmsgs[] = {
	{ EUNDEF,	"Undefined error code" },
	{ ENOTFOUND,	"File not found" },
	{ EACCESS,	"Access violation" },
	{ ENOSPACE,	"Disk full or allocation exceeded" },
	{ EBADOP,	"Illegal TFTP operation" },
	{ EBADID,	"Unknown transfer ID" },
	{ EEXISTS,	"File already exists" },
	{ ENOUSER,	"No such user" },
	{ -1,		0 }
};

/*
 * Send a nak packet (error message).
 * Error code passed in is one of the
 * standard TFTP codes, or a UNIX errno
 * offset by 100.
 */
static void
nak(int error)
{
	register struct tftphdr *tp;
	int length;
	register struct errmsg *pe;

	tp = (struct tftphdr *)buf;
	tp->th_opcode = htons((u_short)ERROR);
	tp->th_code = htons((u_short)error);
	for (pe = errmsgs; pe->e_code >= 0; pe++)
		if (pe->e_code == error)
			break;
	if (pe->e_code < 0) {
		pe->e_msg = strerror(error - 100);
		tp->th_code = EUNDEF;   /* set 'undef' errorcode */
	}
	strcpy(tp->th_msg, pe->e_msg);
	length = strlen(pe->e_msg);
	tp->th_msg[length] = '\0';
	length += 5;
	if (send(peer, buf, length, 0) != length)
		syslog(LOG_ERR, "nak: %m\n");
}
