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
#include "acl.h"

int netfilter_update = 0;

int main(int argc, char **argv) {
//rcpDebugEnable();
	
	// initialize rcplib
	rcp_init(RCP_PROC_ACL);

	RcpPkt *pkt;
	pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory, exiting...\n", rcpGetProcName());
		exit(1);
	}
	
	// clear firewall table
	netfilter_generate();

	// receive loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	while (1) {
		// reconnect mux socket if connection failed
		if (reconnect_timer >= 5) {
			ASSERT(muxsock == 0);
			muxsock = rcpConnectMux(RCP_PROC_ACL, NULL, NULL, NULL);
			if (muxsock == 0)
				reconnect_timer = 1; // start reconnect timer
			else {
				reconnect_timer = 0;
				pstats->mux_reconnect++;
			}
		}

		// set descriptors
		fd_set fds;
		FD_ZERO(&fds);
		int maxfd = 0;
		if (muxsock != 0) {
			FD_SET(muxsock, &fds);
			maxfd = (muxsock > maxfd)? muxsock: maxfd;
		}

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

			// muxsocket reconnect timeout
			if (reconnect_timer > 0)
				reconnect_timer++;
			
			// netfilter update
			if (netfilter_update > 0) {
				if (--netfilter_update == 0)
					netfilter_generate();
			}
			
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
		// cli data
		else if (muxsock != 0 && FD_ISSET(muxsock, &fds)) {
			errno = 0;
			int nread = recv(muxsock, pkt, sizeof(RcpPkt), 0);
			if(nread < sizeof(RcpPkt) || errno != 0) {
//				fprintf(stderr, "Error: process %s, muxsocket nread %d, errno %d, disconnecting...\n",
//					rcpGetProcName(), nread, errno);
				close(muxsock);
				reconnect_timer = 1;
				muxsock = 0;
				continue;
			}
			// read the packet data
			if (pkt->data_len != 0) {
				nread += recv(muxsock, (unsigned char *) pkt + sizeof(RcpPkt), pkt->data_len, 0);
			}
			ASSERT(nread == sizeof(RcpPkt) + pkt->data_len);

			// process the cli packet
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_ACL) {
				processCli(pkt);
				// forward the packet back to rcp
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			else
				ASSERT(0);
			
			
		}
		else
			ASSERT(0);

	}

	return 0;
}
