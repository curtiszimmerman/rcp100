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
#include <netdb.h>
#include "librcp.h"

int debug = 0;

// differenmce in microseconds
static inline unsigned delta_tv(struct timeval *start, struct timeval *end) {
	return (end->tv_sec - start->tv_sec)*1000000 + end->tv_usec - start->tv_usec;
}

int main(int argc, char **argv) {
	// test debug mode
	if (getenv("DBGMONTCP"))
		debug = 1;

	if (debug)
		printf("MONITOR: starting %s\n", argv[0]);

	RcpShm *shm = NULL;
	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}

	// file descriptor set
	fd_set fds;
	FD_ZERO(&fds);
	int maxfd = 0;
	
	// open sockets and try to connect
	int i;
	RcpNetmonHost *ptr;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_NETMON_TYPE_TCP)
			continue;
		if (ptr->ip_resolved == 0)
			continue;	// address not available yet
	
		// update tx counters
		ptr->pkt_sent++;
		ptr->interval_pkt_sent++;
		ptr->wait_response = 1;
		
		// open socket
		int sock;
		struct sockaddr_in server;
		sock = socket(AF_INET , SOCK_STREAM , 0);
		if (sock == -1)
			continue;
		
		// set the socket to non-blocking
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
			close(sock);
			continue;
		}	
		
		// connect
		server.sin_addr.s_addr = htonl(ptr->ip_resolved);
		server.sin_family = AF_INET;
		server.sin_port = htons(ptr->port);
		if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
			if (errno != EINPROGRESS) {
				close(sock);
				continue;
			}
		}	

		// set fd
		FD_SET(sock, &fds);
		maxfd = (sock > maxfd)? sock: maxfd;
		ptr->sock = sock;
		gettimeofday(&ptr->start_tv, NULL);
		if (debug)
			printf("MONITOR: open socket %d.%d.%d.%d:%d, %s, \n",
				RCP_PRINT_IP(ptr->ip_resolved), ptr->port, ptr->hostname);		
	}
	
	// select loop
	struct timeval ts;
	ts.tv_sec = 10; // 10 seconds
	ts.tv_usec = 0;
	while (1) {
		int nready = select(maxfd + 1, (fd_set *) 0, &fds, (fd_set *) 0, &ts);
		if (nready < 0) {
			exit(1);
		}
		else if (nready == 0) {
			if (debug)
				printf("MONITOR: %s giving up!\n", argv[0]);
			exit(0);
		}
		else {
			for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
				if (!ptr->valid || ptr->type != RCP_NETMON_TYPE_TCP)
					continue;
				if (ptr->ip_resolved == 0)
					continue;	// address not available yet
				
				if (FD_ISSET(ptr->sock, &fds)) {
					if (debug)
						printf("MONITOR: socket %d.%d.%d.%d connected\n", 
							RCP_PRINT_IP(ptr->ip_resolved));						
					FD_CLR(ptr->sock, &fds);
					close(ptr->sock);
					ptr->sock = 0;
					ptr->pkt_received++;
					ptr->interval_pkt_received++;
					ptr->wait_response = 0;

					// calculate the response time
					struct timeval end_tv;
					gettimeofday (&end_tv, NULL);
					unsigned resp_time = delta_tv(&ptr->start_tv, &end_tv);
					if (debug)
						printf("MONITOR: received tcp reply from %d.%d.%d.%d, response time %fs\n",
							RCP_PRINT_IP(ptr->ip_resolved),
							(float) resp_time / (float) 1000000);
					ptr->resptime_acc += resp_time;
					ptr->resptime_samples++;
				}
			}
			
			FD_ZERO(&fds);
			maxfd = 0;
			int found = 0;
			for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
				if (!ptr->valid || ptr->type != RCP_NETMON_TYPE_TCP)
					continue;
				if (ptr->ip_resolved == 0)
					continue;	// address not available yet
				if (ptr->sock) {
					FD_SET(ptr->sock, &fds);
					maxfd = (ptr->sock > maxfd)? ptr->sock: maxfd;
					found = 1;
				}
			}
			if (!found)
				exit(0);
		}
	}
	
	return 0;
}

