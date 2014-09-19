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
#include "cli.h"
#include <signal.h>



static void consume_mux_data(void) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(muxsocket,&fds);
	FD_SET(0,&fds);
	int maxfd = muxsocket + 1;

	while (1) {
		if (muxsocket == 0)
			break;

		// consume any data on the socket
		select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0, (struct timeval  *) NULL);
		
		if( FD_ISSET(muxsocket, &fds)) {
			cliReceiveMux();		  // flush any incoming mux data
		}
		
		if( FD_ISSET(0, &fds))
			break;
	}
}



void cliTimeout(void) {
	time_t s_timeout = shm->config.timeout_minutes * 60 + shm->config.timeout_seconds;
	
	// no timeout, just consume any mux data coming in
	if (s_timeout == 0)
		return consume_mux_data();

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(muxsocket,&fds);
	FD_SET(0,&fds);
	int maxfd = muxsocket + 1;

	struct timeval ts;
	ts.tv_sec = s_timeout;
	ts.tv_usec = 0;
	
	while (1) {
		if (muxsocket == 0)
			break;

		// consume any data on the socket
		int ready = select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0, &ts);
		
		if (ready == 0) { // timeout
			printf("Info: session timeout, disconnecting...\n");
			fflush(0);
			exit(0);
		}
		
		if( FD_ISSET(muxsocket, &fds)) {
			cliReceiveMux();		  // flush any incoming mux data
		}
		
		if( FD_ISSET(0, &fds))
			break;
	}
}

static void catch_alarm (int sig) {
	printf("Info: session timeout, disconnecting...\n");
	fflush(0);
	sleep(1);
	exit(0);
}

int cliTimeoutGetc(void) {
	int c;
	signal (SIGALRM, catch_alarm);
	time_t s_timeout = shm->config.timeout_minutes * 60 + shm->config.timeout_seconds;
	alarm(s_timeout);

	c = getc(stdin);
	alarm(0);	
	return c;
}
