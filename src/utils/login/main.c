/*
 * Copyright (C) 2012-2013 RCP100 Team (rcpteam@yahoo.com)
 *
 * This file is part of RCP100 project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
//#include <utmp.h>
#include <netdb.h>
#include <ctype.h>
#include "librcp.h"

#define MAX_INPUT 512	// maximum input for user strings
struct termios t;

static void alarm_callback(int sig) {
	signal(SIGALRM, SIG_IGN);
	
	// restore terminal
	tcsetattr(0,TCSAFLUSH,&t);
	exit(1);
}


static void clean_user(char *username) {
	// remove eol
	char *ptr = username;
	while (*ptr != '\0') {
		if (*ptr == '\n' || *ptr == '\r') {
			*ptr = '\0';
			break;
		}
		ptr++;
	}

	// skip spaces at the beginning
	if (*username != ' ')
		return;
	ptr = username;
	char *ptr2 = username;
	while (*ptr2 == ' ')
		ptr2++;
	while (*ptr2 != '\0') {
		*ptr = *ptr2;
		ptr++;
		ptr2++;
	}
	*ptr = '\0';
}

static void clean_password(char *password) {
	// remove eol
	char *ptr = password;
	while (*ptr != '\0') {
		if (*ptr == '\n' || *ptr == '\r') {
			*ptr = '\0';
			break;
		}
		ptr++;
	}
}

static void get_user_data(char *username, char *password, int nouser) {
	password[0] = '\0';
	
	// prompt username
	if (nouser == 0) {
		username[0] = '\0';
		printf("User: "); fflush(0);
		// get username
		if (!fgets(username, MAX_INPUT, stdin))
			return;
	}
	// clearn user name
	clean_user(username);

	// set terminal for no echo
	struct termios tt;
	tcgetattr(0, &tt);
	tt.c_lflag &= ~(ECHO | ISIG);
	tcsetattr(0,TCSAFLUSH,&tt);

	// prompt password
	printf("Password: "); fflush(0);
	// get password and clean it
	if (!fgets(password, MAX_INPUT, stdin))
		return;
	clean_password(password);
	
	// restore terminal using the global setting
	tcsetattr(0,TCSAFLUSH,&t);
	printf("\n");
}

static RcpAdmin *admin_find(RcpShm *shm, const char *name) {
	int i;
	RcpAdmin *ptr;

	for (i = 0, ptr = shm->config.admin; i < RCP_ADMIN_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		if (strcmp(ptr->name, name) == 0)
			return ptr;
	}
	
	return NULL;
}

int main(int argc, char **argv) {
	// this program shuld be run only as root
	uid_t uid = getuid();
	uid_t euid = getuid();
	if (uid || euid) {
		sleep(5);
		exit(1);
	}
	
	// display /home/rcp/motd
	char *banner = "/home/rcp/banner";
	struct stat st;
	if (stat(banner, &st) == 0) {
		// open the file
		int fd = open(banner, O_RDONLY);
		if (fd != -1) {
#define BUFSIZE 256
			char buf[BUFSIZE];
			int len;
			while ((len = read(fd, buf, BUFSIZE)) > 0) {
				int v = write(STDOUT_FILENO, buf, len);
				(void) v;
			}
			close(fd);
		}
	}


	// save the terminal settings
	tcgetattr(0, &t);
	
	// set an alarm signal for 30s
	signal(SIGALRM, alarm_callback);
	alarm((unsigned int) 30);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	// extract hostname
	char *hostname = NULL;
	int i;
	int user_index = 0;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			hostname = argv[i + 1];
			i++;
		}
		else if (strcmp(argv[i], "-p") == 0); // this is a default for us
		else if (strcmp(argv[i], "--") == 0) { // as called by getty
			i++;
			if (i < argc)
				user_index = i;
		}
		else {
			ASSERT(0);
		}
	}

	if (hostname) {
		struct hostent *he = gethostbyname(hostname);
		unsigned char ha[4] = {0};

		// copy the part we need
		if (he && he->h_addr_list && he->h_addr_list[0]) {
			memcpy(ha, he->h_addr_list[0], 4);
		}
		
		// set the environment
		char hostaddress[30];
		sprintf(hostaddress, "%u.%u.%u.%u", ha[0], ha[1], ha[2], ha[3]);
		setenv("SSH_CONNECTION", hostaddress, 1);
	}

	// extract username and password
	char username[MAX_INPUT + 1] = {0};
	char password[MAX_INPUT + 1] = {0};
	if (user_index) {
		memset(username, 0, MAX_INPUT + 1);
		strcpy(username, argv[user_index]);
		get_user_data(username, password, 1);
	}
	else 
		get_user_data(username, password, 0);
	if (*username == '\0' || *password == '\0')
		exit(1);

	// shut down the alarm
	signal(SIGALRM, SIG_IGN);

	// initialize shared memory
	RcpShm *shm;
	if ((shm = rcpShmemInit(RCP_PROC_CLI)) == NULL) {
		exit(1);
	}
	RcpAdmin *admin = admin_find(shm, username);
	if (admin == NULL) {
		exit(1);
	}

	{ // check password
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
			solt[i] = admin->password[i];
		solt[RCP_CRYPT_SALT_LEN] = '\0';

		char *cp = rcpCrypt(password, solt);
		if (strcmp(admin->password + RCP_CRYPT_SALT_LEN + 1, cp + RCP_CRYPT_SALT_LEN + 4) != 0) {
			exit(1);
		}
	}

	// set RCPUSER
	setenv("RCPUSER", username, 1);

	// drop root privileges
//printf ("UID = %d, EUID = %d\n", getuid(), geteuid());
	struct passwd *pwd = getpwnam("rcp");
	if (pwd) {
		initgroups(pwd->pw_name, pwd->pw_gid);
		setgid(pwd->pw_gid);
		setuid(pwd->pw_uid);
	}

	seteuid(0);   /* this should fail */
	if (!getuid() || !geteuid())
		exit(1);
	if (chdir("/home/rcp") != 0)
		exit(1);
//printf ("UID = %d, EUID = %d\n", getuid(), geteuid());



	// start the cli session
	char *eargv[10];
	int eargc = 0;
	eargv[eargc++] = "/opt/rcp/bin/cli";
	eargv[eargc++] = "-cli";
	eargv[eargc++] = NULL;
	execvp(eargv[0], eargv + 1);
	return 0;
}
