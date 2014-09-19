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
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include "router.h"

char *intremoved[MAX_INTREMOVED];	// interface not to be take into account
int intremoved_cnt = 0;


static void help(void) {
	printf("router - RCP router process\n");
	printf("Usage: router [-r ifname] [-h]\n");
	printf("where:\n");
	printf("\t-h - this help screen\n");
	printf("\t-r ifname - interfaces  with the name starting with ifname will not be taken into account\n");
	printf("\t\t(up to 4 -r arguments can be supplied)\n");
	printf("\n");
}

int main(int argc, char **argv) {
	// parse arguments
	int i;
	for (i = 0; i < MAX_INTREMOVED; i++)
		intremoved[i] = NULL;
		
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			help();
			return 0;
		}
		else if (strcmp(argv[i], "-r") == 0) {
			if (argc == (i + 1)) {
				help();
				return 1;
			}

			if (intremoved_cnt >= MAX_INTREMOVED) {
				help();
				return 1;
			}

			intremoved[intremoved_cnt++] = argv[++i];
		}
		else {
			fprintf(stderr, "Error: process %s, unrecognized argument\n", rcpGetProcName());
			help();
			return 1;
		}
	}
	
	// initialize rcp
	rcp_init(RCP_PROC_ROUTER);
	
	// set interface configuration in shared memory
	router_detect_interfaces();

	RcpPkt *pkt;
	pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory, exiting...\n", rcpGetProcName());
		return 1;
	}
	
	// delete all static routes - default routes are not deleted
	int v = system("/opt/rcp/bin/rtclean static >> /opt/rcp/var/log/restart");
	if (v == -1) {
		fprintf(stderr, "Error: process %s, cannot delete existing static routes, exiting...\n", rcpGetProcName());
		return 1;
	}

	// receive loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	// forever
	while (1) {
		// reconnect mux socket if connection failed
		if (reconnect_timer >= 5) {
			ASSERT(muxsock == 0);
			muxsock = rcpConnectMux(RCP_PROC_ROUTER, NULL, NULL, NULL);
			if (muxsock == 0)
				reconnect_timer = 1; // start reconnect timer
			else {
				reconnect_timer = 0;
				pstats->mux_reconnect++;
			}
		}

		fd_set fds;
		FD_ZERO(&fds);
		if (muxsock != 0)
			FD_SET(muxsock, &fds);
		int maxfd = muxsock;

		// wait for data
		errno = 0;
		int nready = select(maxfd + 1, &fds, (fd_set *) 0, (fd_set *) 0, &ts);
		if (nready < 0) {
			fprintf(stderr, "Error: process %s, select nready %d, errno %d\n",
				rcpGetProcName(), nready, errno);
		}
		else if (nready == 0) {
			// watchdog
			pstats->wproc++;

			// one second timeout
			router_if_timeout();
				
			// muxsocket reconnect timeout
			if (muxsock == 0)
				reconnect_timer++;
			
			// reload ts
			rcptic++;
			if (rcpTic() > rcptic) {
				// speed up the clock
				ts.tv_sec = 0;
				ts.tv_usec = 800000;	// 0.8 seconds
				pstats->select_speedup++;
			}
			else {
				ts.tv_sec = 1; // 1 second
				ts.tv_usec = 0;
			}
		}
		else if (muxsock != 0 && FD_ISSET(muxsock, &fds)) {
			errno = 0;
			int nread = recv(muxsock, pkt, sizeof(RcpPkt), 0);
			if(nread < sizeof(RcpPkt) || errno != 0) {
//				printf("Error: process %s, muxsocket nread %d, errno %d, disconnecting...\n",
//					rcpGetProcName(), nread, errno);
				close(muxsock);
				muxsock = 0;
				continue;
			}

			// read the packet data
			if (pkt->data_len != 0) {
				nread += recv(muxsock, (unsigned char *) pkt + sizeof(RcpPkt), pkt->data_len, 0);
			}
			ASSERT(nread == sizeof(RcpPkt) + pkt->data_len);

			// process cli packet
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_ROUTER) {
				processCli(pkt);
				// forward the response back
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			else
				ASSERT(0);
			
			
		}
		
	}
	return 0;
}
