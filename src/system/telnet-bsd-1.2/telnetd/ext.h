/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *
 */

/*
 * Telnet server variable declarations
 */
extern char options[256];
extern char do_dont_resp[256];
extern char will_wont_resp[256];
extern int linemode;	/* linemode on/off */
extern int utmp_len;

#ifdef LINEMODE
extern int uselinemode;	/* what linemode to use (on/off) */
extern int editmode;	/* edit modes in use */
extern int useeditmode;	/* edit modes to use */
extern int alwayslinemode;	/* command line option */
#ifdef KLUDGELINEMODE
extern int lmodetype;	/* Client support for linemode */
#endif	/* KLUDGELINEMODE */
#endif	/* LINEMODE */

extern int flowmode;	/* current flow control state */
extern int diagnostic;	/* telnet diagnostic capabilities */

#ifdef BFTPDAEMON
extern int bftpd;		/* behave as bftp daemon */
#endif /* BFTPDAEMON */

#if defined(SecurID)
extern int require_SecurID;
#endif

extern slcfun slctab[NSLC + 1];	/* slc mapping table */

extern char *terminaltype;

extern char *loginprg;

/*
 * I/O data buffers, pointers, and counters.
 */
extern char ptyobuf[BUFSIZ+NETSLOP], *pfrontp, *pbackp;
extern char netibuf[BUFSIZ], *netip;
extern char netobuf[BUFSIZ+NETSLOP], *nfrontp, *nbackp;
extern char *neturg;		/* one past last byte of urgent data */
extern int pcc, ncc;

/* printf into netobuf */
void netoprintf(const char *fmt, ...) __attribute((format (printf, 1, 2)));

extern int pty, net;
extern char line[16];
extern int SYNCHing;		/* we are in TELNET SYNCH mode */

void _termstat(void);
void add_slc(int, int, int);
void check_slc(void);
void change_slc(int, int, int);
void cleanup(int);
void clientstat(int, int, int);
void copy_termbuf(char *, int);
void deferslc(void);
void defer_terminit(void);
void do_opt_slc(unsigned char *, int);
void doeof(void);
void dooption(int);
void dontoption(int);
void edithost(const char *, const char *);
void fatal(int, const char *);
void fatalperror(int, const char *);
void get_slc_defaults(void);
void init_env(void);
void init_termbuf(void);
void interrupt(void);
void localstat(void);
void netclear(void);
void netflush(void);
void printoption(const char *, int);
void printdata(const char *, const char *, int);
void printsub(char, unsigned char *, int);
void ptyflush(void);
void putchr(int);
void putf(const char *, char *);
void recv_ayt(void);
void send_do(int, int);
void send_dont(int, int);
void send_slc(void);
void send_status(void);
void send_will(int, int);
void send_wont(int, int);
void sendbrk(void);
void sendsusp(void);
void set_termbuf(void);
void start_login(const char *);
void start_slc(int);
void startslave(const char *host);
void suboption(void);
void telrcv(void);
void ttloop(void);
void tty_binaryin(int);
void tty_binaryout(int);

int end_slc(unsigned char **);
int getnpty(void);
int getpty(void);
int login_tty(int);
int spcset(int, cc_t *, cc_t **);
int stilloob(int);
int terminit(void);
int termstat(void);
int tty_flowmode(void);
int tty_isbinaryin(void);
int tty_isbinaryout(void);
int tty_iscrnl(void);
int tty_isecho(void);
int tty_isediting(void);
int tty_islitecho(void);
int tty_isnewmap(void);
int tty_israw(void);
int tty_issofttab(void);
int tty_istrapsig(void);
int tty_linemode(void);

void tty_rspeed(int);
void tty_setecho(int);
void tty_setedit(int);
void tty_setlinemode(int);
void tty_setlitecho(int);
void tty_setsig(int);
void tty_setsofttab(int);
void tty_tspeed(int);
void willoption(int);
void wontoption(int);
void writenet(unsigned char *, int);

#if defined(ENCRYPT)
extern void (*encrypt_output)(unsigned char *, int);
extern int (*decrypt_input)(int);
extern char *nclearto;
#endif


/*
 * The following are some clocks used to decide how to interpret
 * the relationship between various variables.
 */

extern struct _clocks {
    int system;			/* what the current time is */
    int echotoggle;		/* last time user entered echo character */
    int modenegotiated;		/* last time operating mode negotiated */
    int didnetreceive;		/* last time we read data from network */
    int ttypesubopt;		/* ttype subopt is received */
    int tspeedsubopt;		/* tspeed subopt is received */
    int environsubopt;		/* environ subopt is received */
    int xdisplocsubopt;		/* xdisploc subopt is received */
    int baseline;		/* time started to do timed action */
    int gotDM;			/* when did we last see a data mark */
} clocks;

#define DEFAULT_IM	"%i\r\n%s %r (%h) (%t)\r\n\r\n"
