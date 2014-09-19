/*
 * Copyright (c) 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include "telnet_locl.h"

/* These values need to be the same as defined in libtelnet/kerberos5.c */
/* Either define them in both places, or put in some common header file. */
#define OPTS_FORWARD_CREDS	0x00000002
#define OPTS_FORWARDABLE_CREDS	0x00000001

/*
 * Initialize variables.
 */
void
tninit (void)
{
  init_terminal ();

  init_network ();

  init_telnet ();

  init_sys ();

#if defined(TN3270)
  init_3270 ();
#endif
}

void
usage (void)
{
  fprintf (stderr, "Usage: %s %s%s%s%s\n", prompt,
	   "[-8] [-E] [-L] [-S tos] [-a] [-c] [-d] [-e char] [-l user]",
	   "\n\t[-n tracefile] [-b hostalias ] ",
#if defined(TN3270) && defined(__unix__)
	   "[-noasynch] [-noasynctty] [-noasyncnet] [-r]\n\t[-t transcom]",
#else
	   "[-r] ",
#endif
           "\n\t"
	   "[host-name [port]]");
  exit (1);
}

/*
 * main.  Parse arguments, invoke the protocol or command parser.
 */
int
main (int argc, char *argv[])
{
  int ch;
  char *user, *alias;
#ifdef	FORWARD
  extern int forward_flags;
#endif /* FORWARD */

  tninit ();			/* Clear out things */

  TerminalSaveState ();

  if ((prompt = strrchr (argv[0], '/')))
    ++prompt;
  else
    prompt = argv[0];

  user = alias = NULL;

  rlogin = (strncmp (prompt, "rlog", 4) == 0) ? '~' : _POSIX_VDISABLE;
  autologin = -1;

  while ((ch = getopt (argc, argv, "78DEKLS:X:ab:cde:fFk:l:n:rt:x")) != -1)
    {
      switch (ch)
	{
	case '8':
	  eight = 3;		/* binary output and input */
	  break;
	case '7':
	  eight = 0;
	  break;
	case 'D':
	  {
	    /* sometimes we don't want a mangled display */
	    char *p;
	    if ((p = getenv ("DISPLAY")))
	      env_define ("DISPLAY", (unsigned char *) p);
	    break;
	  }

	case 'E':
	  rlogin = escape = _POSIX_VDISABLE;
	  break;
	case 'K':
	  /* autologin = 0; */
	  break;
	case 'L':
	  eight |= 2;		/* binary output only */
	  break;
	case 'S':
	  {
#ifdef	HAS_GETTOS
	    extern int tos;

	    if ((tos = parsetos (optarg, "tcp")) < 0)
	      fprintf (stderr, "%s%s%s%s\n",
		       prompt, ": Bad TOS argument '",
		       optarg, "; will try to use default TOS");
#else
	    fprintf (stderr,
		     "%s: Warning: -S ignored, no parsetos() support.\n",
		     prompt);
#endif
	  }
	  break;
	case 'X':
#ifdef	AUTHENTICATION
	  auth_disable_name (optarg);
#endif
	  break;
	case 'a':
	  autologin = 1;
	  break;
	case 'c':
	  skiprc = 1;
	  break;
	case 'd':
	  debug = 1;
	  break;
	case 'e':
	  set_escape_char (optarg);
	  break;
	case 'f':
	  fprintf (stderr,
		   "%s: Warning: -f ignored, no Kerberos V5 support.\n",
		   prompt);
	  break;
	case 'F':
	  fprintf (stderr,
		   "%s: Warning: -F ignored, no Kerberos V5 support.\n",
		   prompt);
	  break;
	case 'k':
	  fprintf (stderr,
		   "%s: Warning: -k ignored, no Kerberos V4 support.\n",
		   prompt);
	  break;
	case 'l':
	  autologin = -1;
	  user = optarg;
	  break;
	case 'b':
	  alias = optarg;
	  break;
	case 'n':
#if defined(TN3270) && defined(__unix__)
	  /* distinguish between "-n oasynch" and "-noasynch" */
	  if (argv[optind - 1][0] == '-' && argv[optind - 1][1]
	      == 'n' && argv[optind - 1][2] == 'o')
	    {
	      if (!strcmp (optarg, "oasynch"))
		{
		  noasynchtty = 1;
		  noasynchnet = 1;
		}
	      else if (!strcmp (optarg, "oasynchtty"))
		noasynchtty = 1;
	      else if (!strcmp (optarg, "oasynchnet"))
		noasynchnet = 1;
	    }
	  else
#endif /* defined(TN3270) && defined(__unix__) */
	    SetNetTrace (optarg);
	  break;
	case 'r':
	  rlogin = '~';
	  break;
	case 't':
#if defined(TN3270) && defined(__unix__)
	  transcom = tline;
	  strncpy (transcom, optarg, sizeof (tline));
#else
	  fprintf (stderr,
		   "%s: Warning: -t ignored, no TN3270 support.\n", prompt);
#endif
	  break;
	case 'x':
	  fprintf (stderr,
		   "%s: Warning: -x ignored, no ENCRYPT support.\n", prompt);
	  break;
	case '?':
	default:
	  usage ();
	  /* NOTREACHED */
	}
    }

  if (autologin == -1)
    autologin = (rlogin == _POSIX_VDISABLE) ? 0 : 1;

  argc -= optind;
  argv += optind;

  if (argc)
    {
      char *args[7], **argp = args;

      if (argc > 2)
	usage ();
      *argp++ = prompt;
      if (user)
	{
	  *argp++ = "-l";
	  *argp++ = user;
	}
      if (alias)
	{
	  *argp++ = "-b";
	  *argp++ = alias;
	}
      *argp++ = argv[0];	/* host */
      if (argc > 1)
	*argp++ = argv[1];	/* port */
      *argp = 0;

      if (sigsetjmp (toplevel, 1) != 0)
	Exit (0);
      if (tn (argp - args, args) == 1)
	return (0);
      else
	return (1);
    }
  sigsetjmp (toplevel, 1);
  for (;;)
    {
#ifdef TN3270
      if (shell_active)
	shell_continue ();
      else
#endif
	command (1, 0, 0);
    }
  return 0;
}
