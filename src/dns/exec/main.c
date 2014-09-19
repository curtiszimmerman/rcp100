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
#include "dns.h"
#include <sys/types.h>
#include <sys/stat.h>

int client_sock = 0;		// dns messages from clients
volatile int force_restart = 0;	// exit the process
volatile int force_shutdown = 0; // exit the process
int proxy_disabled = 0;

int main(int argc, char **argv) {
//rcpDebugEnable();
	
	// initialize shared memory
	rcp_init(RCP_PROC_DNS);

	RcpPkt *pkt;
	pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory, exiting...\n", rcpGetProcName());
		exit(1);
	}



	struct stat s;
	if (stat("/opt/rcp/var/log/dnsproxy_at_startup", &s) == 0)
		proxy_disabled = 1;


	// open sockets if necessary
	if (shm->config.dns_server) {
		if (proxy_disabled) {		
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_WARNING, RLOG_FC_DNS,
				"an external DNS proxy is already running on the system, RCP DNS proxy will be disabled");
		}
		else {
			client_sock = rx_open(DNS_SERVER_PORT); // the socket open to clients listens on the server socket
	
			if (client_sock == 0) {
				fprintf(stderr, "Error: process %s, cannot open sockets, exiting...\n", rcpGetProcName());
				exit(1);
			}
		}
	}
	
	// set the static cache entries
	cache_update_static();
	
	
	// drop privileges
	rcpDropPriv();
	
	// initialize request list
	rq_list_init();
	
	// receive loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	while (1) {
		// reconnect mux socket if connection failed
		if (reconnect_timer >= 10) {
			// a regular reconnect will fail, this process runs with dropped privileges
			// end the process and restart it again
			break;
		}

		// set descriptors
		fd_set fds;
		FD_ZERO(&fds);
		int maxfd = 0;
		if (muxsock != 0) {
			FD_SET(muxsock, &fds);
			maxfd = (muxsock > maxfd)? muxsock: maxfd;
		}
		if (client_sock != 0) {
			FD_SET(client_sock, &fds);
			maxfd = (client_sock > maxfd)? client_sock: maxfd;
		}
		
		// set all server sockets
		DnsReq *rq = rq_active();
		while (rq) {
			if (rq->sock != 0) {
				FD_SET(rq->sock, &fds);
				maxfd = (rq->sock > maxfd)? rq->sock: maxfd;
			}

			rq = rq->next;
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
			
			// age cache entries
			cache_timer();
			
			// age request queue entries
			rq_timer();
			
			// reset rate-limit counter
			rate_limit = 0;
			
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
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_DNS) {
				processCli(pkt);
				// forward the packet back to rcp
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			// dns updates packet
			else if (pkt->type == RCP_PKT_TYPE_UPDATEDNS) {
				rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_IPC,
					"processing DNS updates packet");
				cache_update_static();
			}
			
			else
				ASSERT(0);
			
			
		}
		// DNS packets from clients
		else if (client_sock != 0 && FD_ISSET(client_sock, &fds)) {
			rx_packet(client_sock);
		}
		// DNS packets from servers
		else {
			DnsReq *rq = rq_active();
			while (rq) {
				if (rq->sock != 0 && FD_ISSET(rq->sock, &fds)) {
					rx_packet(rq->sock);
					break;
				}
				rq = rq->next;
			}
		}

		if (force_restart) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_NOTICE, RLOG_FC_IPC,
				"process %s exiting for configuration purposes", rcpGetProcName());
			if (pstats->wmonitor != 0)
				pstats->wproc = pstats->wmonitor;	// tigger a restart in the next monitoring cycle
			pstats->no_logging = 1;
			break; // exit from while(1)
		}
		
		if (force_shutdown)
			break;

	}
	
	fflush(0);
	sleep(1);
	
	// close sockets
	if (muxsock != 0) {
		close(muxsock);
	}
	if (client_sock != 0) {
		close(client_sock);
	}
	
	// remove cli memory
	cliRemoveFunctions();

	// clear request memory
	rq_clear_inactive();
	// remove cache memory	
	cache_clear();
	
	// remove packet memory
	if (pkt)
		free(pkt);
	
	return 0;
}
