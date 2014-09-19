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

#include <utmp.h>
#include <pty.h>

#include "telnetd.h"
#include "pathnames.h"

static struct termios termbuf, termbuf2;	/* pty control structure */

/*
 * init_termbuf()
 * copy_termbuf(cp)
 * set_termbuf()
 *
 * These three routines are used to get and set the "termbuf" structure
 * to and from the kernel.  init_termbuf() gets the current settings.
 * copy_termbuf() hands in a new "termbuf" to write to the kernel, and
 * set_termbuf() writes the structure into the kernel.
 */

void
init_termbuf (void)
{
  tcgetattr (pty, &termbuf);
  termbuf2 = termbuf;
}

#if defined(LINEMODE) && defined(TIOCPKT_IOCTL)
/*
 * ?
 */
void
copy_termbuf (char *cp, int len)
{
  if (len > sizeof (termbuf))
    len = sizeof (termbuf);
  bcopy (cp, (char *) &termbuf, len);
  termbuf2 = termbuf;
}

#endif /* defined(LINEMODE) && defined(TIOCPKT_IOCTL) */

void
set_termbuf (void)
{
  if (memcmp (&termbuf, &termbuf2, sizeof (termbuf)))
    tcsetattr (pty, TCSANOW, &termbuf);
}


/*
 * spcset(func, valp, valpp)
 *
 * This function takes various special characters (func), and
 * sets *valp to the current value of that character, and
 * *valpp to point to where in the "termbuf" structure that
 * value is kept.
 *
 * It returns the SLC_ level of support for this function.
 */


int
spcset (int func, cc_t * valp, cc_t ** valpp)
{

#define	setval(a, b)	*valp = termbuf.c_cc[a]; \
			*valpp = &termbuf.c_cc[a]; \
			return(b);
#define	defval(a) *valp = ((cc_t)a); *valpp = (cc_t *)0; return(SLC_DEFAULT);

  switch (func)
    {
    case SLC_EOF:
      setval (VEOF, SLC_VARIABLE);
    case SLC_EC:
      setval (VERASE, SLC_VARIABLE);
    case SLC_EL:
      setval (VKILL, SLC_VARIABLE);
    case SLC_IP:
      setval (VINTR, SLC_VARIABLE | SLC_FLUSHIN | SLC_FLUSHOUT);
    case SLC_ABORT:
      setval (VQUIT, SLC_VARIABLE | SLC_FLUSHIN | SLC_FLUSHOUT);
    case SLC_XON:
#ifdef VSTART
      setval (VSTART, SLC_VARIABLE);
#else
      defval (0x13);
#endif
    case SLC_XOFF:
#ifdef	VSTOP
      setval (VSTOP, SLC_VARIABLE);
#else
      defval (0x11);
#endif
    case SLC_EW:
#ifdef	VWERASE
      setval (VWERASE, SLC_VARIABLE);
#else
      defval (0);
#endif
    case SLC_RP:
#ifdef	VREPRINT
      setval (VREPRINT, SLC_VARIABLE);
#else
      defval (0);
#endif
    case SLC_LNEXT:
#ifdef	VLNEXT
      setval (VLNEXT, SLC_VARIABLE);
#else
      defval (0);
#endif
    case SLC_AO:
#if	!defined(VDISCARD) && defined(VFLUSHO)
# define VDISCARD VFLUSHO
#endif
#ifdef	VDISCARD
      setval (VDISCARD, SLC_VARIABLE | SLC_FLUSHOUT);
#else
      defval (0);
#endif
    case SLC_SUSP:
#ifdef	VSUSP
      setval (VSUSP, SLC_VARIABLE | SLC_FLUSHIN);
#else
      defval (0);
#endif
#ifdef	VEOL
    case SLC_FORW1:
      setval (VEOL, SLC_VARIABLE);
#endif
#ifdef	VEOL2
    case SLC_FORW2:
      setval (VEOL2, SLC_VARIABLE);
#endif
    case SLC_AYT:
#ifdef	VSTATUS
      setval (VSTATUS, SLC_VARIABLE);
#else
      defval (0);
#endif

    case SLC_BRK:
    case SLC_SYNCH:
    case SLC_EOR:
      defval (0);

    default:
      *valp = 0;
      *valpp = 0;
      return (SLC_NOSUPPORT);
    }
}

/*
 * getpty()
 *
 * Allocate a pty.  As a side effect, the external character
 * array "line" contains the name of the slave side.
 *
 * Returns the file descriptor of the opened pty.
 */
char line[16];

static int ptyslavefd = -1;

int
getpty (void)
{
  int masterfd;

  if (openpty (&masterfd, &ptyslavefd, line, NULL, NULL))
    return -1;
  return masterfd;
}

#ifdef	LINEMODE
/*
 * tty_flowmode()	Find out if flow control is enabled or disabled.
 * tty_linemode()	Find out if linemode (external processing) is enabled.
 * tty_setlinemod(on)	Turn on/off linemode.
 * tty_isecho()		Find out if echoing is turned on.
 * tty_setecho(on)	Enable/disable character echoing.
 * tty_israw()		Find out if terminal is in RAW mode.
 * tty_binaryin(on)	Turn on/off BINARY on input.
 * tty_binaryout(on)	Turn on/off BINARY on output.
 * tty_isediting()	Find out if line editing is enabled.
 * tty_istrapsig()	Find out if signal trapping is enabled.
 * tty_setedit(on)	Turn on/off line editing.
 * tty_setsig(on)	Turn on/off signal trapping.
 * tty_issofttab()	Find out if tab expansion is enabled.
 * tty_setsofttab(on)	Turn on/off soft tab expansion.
 * tty_islitecho()	Find out if typed control chars are echoed literally
 * tty_setlitecho()	Turn on/off literal echo of control chars
 * tty_tspeed(val)	Set transmit speed to val.
 * tty_rspeed(val)	Set receive speed to val.
 */

int
tty_flowmode (void)
{
  return (termbuf.c_iflag & IXON ? 1 : 0);
}

int
tty_linemode (void)
{
  return (termbuf.c_lflag & EXTPROC);
}

void
tty_setlinemode (int on)
{
#ifdef TIOCEXT
  set_termbuf ();
  ioctl (pty, TIOCEXT, (char *) &on);
  init_termbuf ();
#else /* !TIOCEXT */
# ifdef	EXTPROC
  if (on)
    termbuf.c_lflag |= EXTPROC;
  else
    termbuf.c_lflag &= ~EXTPROC;
# endif
#endif /* TIOCEXT */
}

int
tty_isecho (void)
{
  return (termbuf.c_lflag & ECHO);
}

#endif /* LINEMODE */

void
tty_setecho (int on)
{
  if (on)
    termbuf.c_lflag |= ECHO;
  else
    termbuf.c_lflag &= ~ECHO;
}

#if defined(LINEMODE) && defined(KLUDGELINEMODE)
int
tty_israw (void)
{
  return (!(termbuf.c_lflag & ICANON));
}

#endif /* defined(LINEMODE) && defined(KLUDGELINEMODE) */

void
tty_binaryin (int on)
{
  if (on)
    termbuf.c_iflag &= ~ISTRIP;
  else
    termbuf.c_iflag |= ISTRIP;
}

void
tty_binaryout (int on)
{
  if (on)
    {
      termbuf.c_cflag &= ~(CSIZE | PARENB);
      termbuf.c_cflag |= CS8;
      termbuf.c_oflag &= ~OPOST;
    }
  else
    {
      termbuf.c_cflag &= ~CSIZE;
      termbuf.c_cflag |= CS7 | PARENB;
      termbuf.c_oflag |= OPOST;
    }
}

int
tty_isbinaryin (void)
{
  return (!(termbuf.c_iflag & ISTRIP));
}

int
tty_isbinaryout (void)
{
  return (!(termbuf.c_oflag & OPOST));
}

#ifdef	LINEMODE
int
tty_isediting (void)
{
  return (termbuf.c_lflag & ICANON);
}

int
tty_istrapsig (void)
{
  return (termbuf.c_lflag & ISIG);
}

void
tty_setedit (int on)
{
  if (on)
    termbuf.c_lflag |= ICANON;
  else
    termbuf.c_lflag &= ~ICANON;
}

void
tty_setsig (int on)
{
  if (on)
    termbuf.c_lflag |= ISIG;
  else
    termbuf.c_lflag &= ~ISIG;
}

#endif /* LINEMODE */

int
tty_issofttab (void)
{
#ifdef OXTABS
  return (termbuf.c_oflag & OXTABS);
#endif
#ifdef TABDLY
  return ((termbuf.c_oflag & TABDLY) == TAB3);
#endif
}

void
tty_setsofttab (int on)
{
  if (on)
    {
#ifdef	OXTABS
      termbuf.c_oflag |= OXTABS;
#endif
#ifdef	TABDLY
      termbuf.c_oflag &= ~TABDLY;
      termbuf.c_oflag |= TAB3;
#endif
    }
  else
    {
#ifdef	OXTABS
      termbuf.c_oflag &= ~OXTABS;
#endif
#ifdef	TABDLY
      termbuf.c_oflag &= ~TABDLY;
      termbuf.c_oflag |= TAB0;
#endif
    }
}

int
tty_islitecho (void)
{
  return (!(termbuf.c_lflag & ECHOCTL));
}

void
tty_setlitecho (int on)
{
  if (on)
    termbuf.c_lflag &= ~ECHOCTL;
  else
    termbuf.c_lflag |= ECHOCTL;
}

int
tty_iscrnl (void)
{
  return (termbuf.c_iflag & ICRNL);
}

/*
 * A table of available terminal speeds
 */
struct termspeeds
{
  int speed;
  int value;
}
termspeeds[] =
{
  {0, B0},
  {50, B50},
  {75, B75},
  {
  110, B110}
  ,
  {
  134, B134}
  ,
  {
  150, B150}
  ,
  {
  200, B200}
  ,
  {
  300, B300}
  ,
  {
  600, B600}
  ,
  {
  1200, B1200}
  ,
  {
  1800, B1800}
  ,
  {
  2400, B2400}
  ,
  {
  4800, B4800}
  ,
  {
  9600, B9600}
  ,
  {
  19200, B9600}
  ,
  {
  38400, B9600}
  ,
  {
  -1, B9600}
};

void
tty_tspeed (int val)
{
  struct termspeeds *tp;
  for (tp = termspeeds; (tp->speed != -1) && (val > tp->speed); tp++);
  cfsetospeed (&termbuf, tp->value);
}

void
tty_rspeed (int val)
{
  struct termspeeds *tp;
  for (tp = termspeeds; (tp->speed != -1) && (val > tp->speed); tp++);
  cfsetispeed (&termbuf, tp->value);
}

/*
 * getptyslave()
 *
 * Open the slave side of the pty, and do any initialization
 * that is necessary.  The return value is a file descriptor
 * for the slave side.
 */
#ifdef	TIOCGWINSZ
extern int def_row, def_col;
#endif
extern int def_tspeed, def_rspeed;

static int
getptyslave (void)
{
#if 0
  register int t = -1;

# ifdef	LINEMODE
  int waslm;
# endif
# ifdef	TIOCGWINSZ
  struct winsize ws;
# endif
  /*
   * Opening the slave side may cause initilization of the
   * kernel tty structure.  We need remember the state of
   *  if linemode was turned on
   *  terminal window size
   *  terminal speed
   * so that we can re-set them if we need to.
   */
# ifdef	LINEMODE
  waslm = tty_linemode ();
# endif


  /*
   * Make sure that we don't have a controlling tty, and
   * that we are the session (process group) leader.
   */
  t = open (_PATH_TTY, O_RDWR);
  if (t >= 0)
    {
      ioctl (t, TIOCNOTTY, (char *) 0);
      close (t);
    }

  t = cleanopen (line);
  if (t < 0)
    fatalperror (net, line);
#endif /* 0 */

  struct winsize ws;
  int t = ptyslavefd;

  /*
   * set up the tty modes as we like them to be.
   */
  init_termbuf ();
# ifdef	TIOCGWINSZ
  if (def_row || def_col)
    {
      bzero ((char *) &ws, sizeof (ws));
      ws.ws_col = def_col;
      ws.ws_row = def_row;
      ioctl (t, TIOCSWINSZ, (char *) &ws);
    }
# endif

  /*
   * Settings for all other termios/termio based
   * systems, other than 4.4BSD.  In 4.4BSD the
   * kernel does the initial terminal setup.
   *
   * XXX what about linux?
   */
#  ifndef	OXTABS
#   define OXTABS	0
#  endif
  termbuf.c_lflag |= ECHO;
  termbuf.c_oflag |= OPOST | ONLCR | OXTABS;
  termbuf.c_iflag |= ICRNL;
  termbuf.c_iflag &= ~IXOFF;

  tty_rspeed ((def_rspeed > 0) ? def_rspeed : 9600);
  tty_tspeed ((def_tspeed > 0) ? def_tspeed : 9600);
# ifdef	LINEMODE
  if (waslm)
    tty_setlinemode (1);
# endif				/* LINEMODE */

  /*
   * Set the tty modes, and make this our controlling tty.
   */
  set_termbuf ();
  if (login_tty (t) == -1)
    fatalperror (net, "login_tty");

  if (net > 2)
    close (net);
  if (pty > 2)
    close (pty);
  return t;
}

#if 0
#ifndef	O_NOCTTY
#define	O_NOCTTY	0
#endif
/*
 * Open the specified slave side of the pty,
 * making sure that we have a clean tty.
 */
static int
cleanopen (char *lyne)
{
  register int t;

  /*
   * Make sure that other people can't open the
   * slave side of the connection.
   */
  chown (lyne, 0, 0);
  chmod (lyne, 0600);

#ifndef NO_REVOKE
  revoke (lyne);
#endif

  t = open (lyne, O_RDWR | O_NOCTTY);
  if (t < 0)
    return (-1);

  /*
   * Hangup anybody else using this ttyp, then reopen it for
   * ourselves.
   */
# if !defined(__linux__)
  /* this looks buggy to me, our ctty is really a pty at this point */
  signal (SIGHUP, SIG_IGN);
  vhangup ();
  signal (SIGHUP, SIG_DFL);
  t = open (lyne, O_RDWR | O_NOCTTY);
  if (t < 0)
    return (-1);
# endif
  return (t);
}

#endif /* 0 */

int
login_tty (int t)
{
  if (setsid () < 0)
    fatalperror (net, "setsid()");
  if (ioctl (t, TIOCSCTTY, (char *) 0) < 0)
    {
      fatalperror (net, "ioctl(sctty)");
    }
  if (t != 0)
    dup2 (t, 0);
  if (t != 1)
    dup2 (t, 1);
  if (t != 2)
    dup2 (t, 2);
  if (t > 2)
    close (t);
  return 0;
}

/*
 * startslave(host)
 *
 * Given a hostname, do whatever
 * is necessary to startup the login process on the slave side of the pty.
 */

void
startslave (const char *host)
{
  int i;

  i = fork ();
  if (i < 0)
    fatalperror (net, "fork");
  if (i)
    {
      /* parent */
      signal (SIGHUP, SIG_IGN);
      close (ptyslavefd);
    }
  else
    {
      /* child */
      signal (SIGHUP, SIG_IGN);
      getptyslave ();
      start_login (host);
     /*NOTREACHED*/}
}

char *envinit[3];

void
init_env (void)
{
  char **envp;
  envp = envinit;
  if ((*envp = getenv ("TZ")) != NULL)
    *envp++ -= 3;
  *envp = 0;
  environ = envinit;
}

/*
 * start_login(host)
 *
 * Assuming that we are now running as a child processes, this
 * function will turn us into the login process.
 */

struct argv_stuff
{
  const char **argv;
  int argc;
  int argmax;
};

static void addarg (struct argv_stuff *, const char *);
static void initarg (struct argv_stuff *);

void
start_login (const char *host)
{
  struct argv_stuff avs;
  char *const *argvfoo;

  initarg (&avs);

  /*
   * -h : pass on name of host.
   *            WARNING:  -h is accepted by login if and only if
   *                    getuid() == 0.
   * -p : don't clobber the environment (so terminal type stays set).
   *
   * -f : force this login, he has already been authenticated
   */
  addarg (&avs, loginprg);
  addarg (&avs, "-h");
  addarg (&avs, host);
#if !defined(NO_LOGIN_P)
  addarg (&avs, "-p");
#endif
#ifdef BFTPDAEMON
  /*
   * Are we working as the bftp daemon?  If so, then ask login
   * to start bftp instead of shell.
   */
  if (bftpd)
    {
      addarg (&avs, "-e");
      addarg (&avs, BFTPPATH);
    }
  else
#endif
    {
#if defined (SecurID)
      /*
       * don't worry about the -f that might get sent.
       * A -s is supposed to override it anyhow.
       */
      if (require_SecurID)
	addarg (&avs, "-s");
#endif
      if (getenv ("USER"))
	{
	  addarg (&avs, getenv ("USER"));
	  if (*getenv ("USER") == '-')
	    {
	      write (1, "I don't hear you!\r\n", 19);
	      syslog (LOG_ERR, "Attempt to login with an option!");
	      exit (1);
	    }
	}
    }
  closelog ();
  /* execv() should really take char const* const *, but it can't */
  /*argvfoo = argv */ ;
  memcpy (&argvfoo, &avs.argv, sizeof (argvfoo));
  execv (loginprg, argvfoo);

  syslog (LOG_ERR, "%s: %m\n", loginprg);
  fatalperror (net, loginprg);
}

static void
initarg (struct argv_stuff *avs)
{
  /*
   * 10 entries and a null
   */
  avs->argmax = 11;
  avs->argv = malloc (sizeof (avs->argv[0]) * avs->argmax);
  if (avs->argv == NULL)
    {
      fprintf (stderr, "Out of memory\n");
      exit (1);
    }
  avs->argc = 0;
  avs->argv[0] = NULL;
}

static void
addarg (struct argv_stuff *avs, const char *val)
{
  if (avs->argc >= avs->argmax - 1)
    {
      avs->argmax += 10;
      avs->argv = realloc (avs->argv, sizeof (avs->argv[0]) * avs->argmax);
      if (avs->argv == NULL)
	{
	  fprintf (stderr, "Out of memory\n");
	  exit (1);
	}
    }

  avs->argv[avs->argc++] = val;
  avs->argv[avs->argc] = NULL;
}

/*
 * cleanup()
 *
 * This is the routine to call when we are all through, to
 * clean up anything that needs to be cleaned up.
 */
void
cleanup (int sig)
{
  sigset_t sigset;
  char *p;
  (void) sig;

  p = line + sizeof ("/dev/") - 1;

  /* logout() is not thread safe, so make sure we don't
   * receive another signal while we're in that function. */
  sigfillset(&sigset);
  sigprocmask(SIG_SETMASK, &sigset, &sigset);
  if (logout (p))
    logwtmp (p, "", "");
  sigprocmask(SIG_SETMASK, &sigset, NULL);
#ifdef PARANOID_TTYS
  /*
   * dholland 16-Aug-96 chmod the tty when not in use
   * This will make it harder to attach unwanted stuff to it
   * (which is a security risk) but will break some programs.
   */
  chmod (line, 0600);
#else
  chmod (line, 0666);
#endif
  chown (line, 0, 0);
  *p = 'p';
  chmod (line, 0666);
  chown (line, 0, 0);
  shutdown (net, 2);
  exit (1);
}
