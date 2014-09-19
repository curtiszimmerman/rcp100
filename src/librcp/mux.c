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
#include <sys/types.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "librcp.h"

// returns the socket or 0 if error
int rcpConnectMux(unsigned sender_type, const char *ttyname, const char *admin, const char *ip) {
	int len;
	struct sockaddr_un name;
	int s; // socket
	
	// create the socket
	if( (s = socket(AF_UNIX, SOCK_STREAM, 0) ) < 0) {
		ASSERT(0);
		return 0;
	}

	// create the server address
	memset(&name, 0, sizeof(struct sockaddr_un));
	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, RCP_MUX_SOCKET);
	len = sizeof(name.sun_family) + strlen(name.sun_path);

	// connect to the server
	if (connect(s, (struct sockaddr *) &name, len) < 0) {
		printf("Error: process %s, cannot reconnect muxsocket\n", rcpGetProcName());
		return 0;
	}

	// set socket non-blocking
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
		ASSERT(0);
		return 0;
	}

	// build the registration message
	RcpPkt pkt;
	char buf[sizeof(RcpPkt) + RCP_PKT_DATA_LEN];
	memset(&pkt, 0, sizeof(RcpPkt));
	pkt.type = RCP_PKT_TYPE_REGISTRATION;
	pkt.destination = RCP_PROC_SERVICES;
	pkt.sender = sender_type;
	char *ptr = buf + sizeof(RcpPkt);
	*ptr = 0;
	ptr++;	// skip the first byte
	int datalen = 1; // skip the first byte
	
	// add session data to registration message
	if (ttyname != NULL) {
		strncpy(ptr, ttyname, CLI_MAX_TTY_NAME);
		pkt.pkt.registration.offset_tty = datalen;
		datalen += strlen(ptr) + 1;
		ptr += strlen(ptr) + 1;
	}
	if (admin != NULL) {
		strncpy(ptr, admin, CLI_MAX_ADMIN_NAME);
		pkt.pkt.registration.offset_admin = datalen;
		datalen += strlen(ptr) + 1;
		ptr += strlen(ptr) + 1;
	}
	if (ip != NULL) {
		strncpy(ptr, ip, CLI_MAX_IP_NAME);
		pkt.pkt.registration.offset_ip = datalen;
		datalen += strlen(ptr) + 1;
		ptr += strlen(ptr) + 1;
	}
	pkt.data_len = datalen;
	memcpy(buf, &pkt, sizeof(RcpPkt));		

	// send the registration message to rcpservices
	errno = 0;
	int rv = send(s, buf, sizeof(RcpPkt) + datalen, 0);
	if (errno != 0 && rv != (sizeof(RcpPkt) + datalen)) {
		ASSERT(0);
		close(s);
		return 0;
	}
	
	return s;
}

// returns a file descriptor, stores the file name in data string provided by the user
FILE *rcpTransportFile(char *data) {
	ASSERT(data != NULL);
	*data = '\0';
	
	// get rcp user data
	struct passwd *p = getpwnam("rcp");
	if (p == NULL) {
		ASSERT(0);
		return NULL;
	}

	// generate a temporary file
	char tmpl[50] = "/opt/rcp/var/transport/tXXXXXX";
	int fd = mkstemp(tmpl);
	if (fd == -1) {
		ASSERT(0)
		return NULL;
	}
	close(fd);

	if (chown(tmpl, p->pw_uid, p->pw_gid)) {
		ASSERT(0);
		unlink(tmpl);
		return NULL;
	}

	// open the file
	FILE *fp = fopen(tmpl, "w");
	if (fp == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	strcpy(data, tmpl);
	return fp;
}

// returns a file descriptor, stores the file name in data string provided by the user
FILE *rcpJailTransportFile(char *data) {
	ASSERT(data != NULL);
	*data = '\0';
	
	// generate a temporary file
	char tmpl[50] = "tXXXXXX";
	int fd = mkstemp(tmpl);
	if (fd == -1) {
		ASSERT(0)
		return NULL;
	}
	close(fd);


	// open the file
	FILE *fp = fopen(tmpl, "w");
	if (fp == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	strcpy(data, "/opt/rcp/var/transport/");
	strcat(data, tmpl);
	return fp;
}

// returns malloc memory
char *rcpGetTempFile(void) {
	char *fname = malloc(10);
	if (fname == NULL)
		return NULL;

	strcpy(fname, "trfXXXXXX");
	int fd = mkstemp(fname);
	if (fd == -1) {
		free(fname);
		return NULL;
	}

	FILE *fp = fopen(fname, "w");
	if (fp == NULL) {
		free(fname);
		return NULL;
	}
	fclose(fp);

	chmod(fname, S_IRWXU | S_IRWXG | S_IRWXO);
	return fname;
}

