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
#include "dhcp.h"
#include "lease.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int icmp_sock = 0;
	
	// initialize shared memory
	rcp_init(RCP_PROC_DHCP);

	RcpPkt *pkt;
	pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory,  exiting...\n", rcpGetProcName());
		exit(1);
	}

	struct stat s;
	if (stat("/opt/rcp/var/log/dhcp_at_startup", &s) == 0) {		
		rcpLog(muxsock, RCP_PROC_DHCP, RLOG_WARNING, RLOG_FC_DHCP,
			"an external DHCP server or relay is already running on the system");
	}	

	int rxsock = rxconnect();
	if (rxsock == 0) {
		fprintf(stderr, "Error: process %s, cannot connect DHCP socket, exiting...\n", rcpGetProcName());
		return 1;
	}

	// drop privileges
//	rcpDropPriv();
	
	// select loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// initialize lease database
	lease_init();
	
	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	while (1) {
		if (reconnect_timer >= 5) {
			ASSERT(muxsock == 0);
			muxsock = rcpConnectMux(RCP_PROC_DHCP, NULL, NULL, NULL);
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
		FD_SET(rxsock, &fds);
		int maxfd = (muxsock > rxsock)? muxsock: rxsock;

		if (icmp_sock != 0) {
			FD_SET(icmp_sock, &fds);
			maxfd = (icmp_sock > maxfd)? icmp_sock: maxfd;
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

			// connect or disconnect icmp socket
			if (icmp_sock == 0 && shm->config.dhcps_enable) {
				icmp_sock = open_icmp_sock();
			}
			else if (icmp_sock != 0 && !shm->config.dhcps_enable) {
				close(icmp_sock);
				icmp_sock = 0;
			}

			// timout icmp queue
			icmp_timeout();

			// expire leases
			lease_timeout();
			
			// update the lease file every hour
			if ((rcptic % 3600) == 0)
				lease_file_update();
				
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
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_DHCP) {
				processCli(pkt);
				// forward the packet back to rcp
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			else
				ASSERT(0);
			
			
		}
		// dhcp data
		else if (rxsock != 0 && FD_ISSET(rxsock, &fds)) {
			rxpacket(rxsock, icmp_sock);
		}
		else if (icmp_sock != 0 && FD_ISSET(icmp_sock, &fds)) {
			get_icmp_pkt(icmp_sock);
		}
		
	}
	return 0;
}
