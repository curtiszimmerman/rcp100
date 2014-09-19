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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include "librcp.h"


static void usage(void) {
	printf("rcplog - utility to log messages to RCP\n");
	printf("Usage: rcplog error-level \"your message here\"\n");
	printf("where:\n");
	printf("\terror-level: number between 0 and 7\n");
	printf("\n");
}


int main(int argc, char **argv) {
	int muxsock = 0;
	RcpShm *shm = NULL;
	
	// parse arguments
	if (argc != 3) {
		fprintf(stderr, "Error: exactly 3 arguments expected\n");
		usage();
		return 1;
	}
	int level = atoi(argv[1]);

	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}


	// connect multiplexer
	muxsock = rcpConnectMux(RCP_PROC_TESTPROC, NULL, NULL, NULL);	
	if (muxsock == 0) {
		fprintf(stderr, "Error: process %s, cannot connect services, exiting...\n", rcpGetProcName());
		exit(1);
	}

	// send message
	rcpLog(muxsock, RCP_PROC_SERVICES, level, RLOG_FC_ADMIN, argv[2]);

	// close multiplexer
	close(muxsock);	
	
	return 0;
}
