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
#include "mgmt.h"
#include <sys/types.h>
#include <sys/stat.h>

int resolver_update = 2;
int stats_interval = 0;
int nprocs = 0;	// number of CPUs

static int have_hostnames(void) {
	int rv = 0;
	
	int i;
	RcpNetmonHost *ptr;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		uint32_t ip;
		if (atoip(ptr->hostname, &ip) == 0)
			ptr->ip_resolved = ip;
		else {
			rv = 1;
		}
	}
	
	return rv;
}

static int have_xmlstats(void) {
	struct stat s;
	if (stat("/opt/rcp/bin/rcpxmlstats", &s) == 0)
		return 1;
	return 0;
}

int main(int argc, char **argv) {
	int icmp_sock = 0;
	int udp_sock = 0;
	
//rcpDebugEnable();
	
	// initialize rcplib
	rcp_init(RCP_PROC_NETMON);

	RcpPkt *pkt;
	pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory, exiting...\n", rcpGetProcName());
		exit(1);
	}
	
	// initialize running status
	netmon_clear();
	ping_flag_update();
	tcp_flag_update();
	
	// receive loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	stats_interval = rcpGetStatsInterval();
	int stats_cnt = NETMON_STATS_CNT;
	
	nprocs = (int) sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs == -1) {
		fprintf(stderr, "Error: process %s, cannot read the number of processors, assuming one processor only\n", rcpGetProcName());
		nprocs = 1;
	}

	while (1) {
		// reconnect mux socket if connection failed
		if (reconnect_timer >= 5) {
			ASSERT(muxsock == 0);
			muxsock = rcpConnectMux(RCP_PROC_NETMON, NULL, NULL, NULL);
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
		if (icmp_sock != 0) {
			FD_SET(icmp_sock, &fds);
			maxfd = (icmp_sock > maxfd)? icmp_sock: maxfd;
		}
		if (udp_sock != 0) {
			FD_SET(udp_sock, &fds);
			maxfd = (udp_sock > maxfd)? udp_sock: maxfd;
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
			if (icmp_sock == 0 && ping_enabled) {
				icmp_sock = open_icmp_sock();
			}
			else if (icmp_sock != 0 && !ping_enabled) {
				close(icmp_sock);
				icmp_sock = 0;
			}

			// connect or disconnect udp socket
			if (udp_sock == 0 && udp_enabled) {
				udp_sock = open_udp_sock();
			}
			else if (udp_sock != 0 && !udp_enabled) {
				close(udp_sock);
				udp_sock = 0;
			}

			// resolver
			if (--resolver_update == 0) {
				if (have_hostnames()) {
					int v = system("/opt/rcp/bin/rcpresolver &");
					if (v == -1)
						ASSERT(0);
				}
				resolver_update = RESOLVER_TIMEOUT;
			}
			
			// stats
			if (--stats_cnt <= 0) {
				stats_cnt = NETMON_STATS_CNT;
				int newinterval = rcpGetStatsInterval();
				if (newinterval != stats_interval) {
					stats_interval = newinterval;
					process_stats();
					if (have_xmlstats()) {
						int v = system("/opt/rcp/bin/rcpxmlstats --save &");
						if (v == -1)
							ASSERT(0);
					}
				}
			}
			
			
			// netmon
			if ((rcptic % shm->config.monitor_interval) == 0 && ping_enabled) {
				// ICMP
				if (icmp_sock != 0)
					ping_all(icmp_sock);
			}

			if ((rcptic % shm->config.monitor_interval) == 1) {
				// TCP
				if (tcp_enabled) {
					int v = system("/opt/rcp/bin/rcpmon-tcp &");
					if (v == -1)
						ASSERT(0);
				}
				// TCP service
				if (http_enabled || ssh_enabled || smtp_enabled) {
					int v = system("/opt/rcp/bin/rcpmon-http &");
					if (v == -1)
						ASSERT(0);
				}
			}

			if ((rcptic % shm->config.monitor_interval) == 2 && udp_enabled) {
				// UDP service
				if (udp_sock != 0 )
					udp_send(udp_sock);
			}

			if ((rcptic % shm->config.monitor_interval) == 15 && ping_enabled) {
				// ICMP
				if (icmp_sock != 0)
					ping_monitor();
			}

			if ((rcptic % shm->config.monitor_interval) == 16) {
				// check results
				if (tcp_enabled)
					monitor_tcp();
				if (http_enabled)
					monitor_http();
				if (smtp_enabled)
					monitor_smtp();
				if (ssh_enabled)
					monitor_ssh();
			}
			if ((rcptic % shm->config.monitor_interval) == 17) {
				// check results
				if (udp_enabled)
					monitor_udp();
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
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_NETMON) {
				processCli(pkt);
				// forward the packet back to rcp
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			else
				ASSERT(0);
			
			
		}
		else if (icmp_sock != 0 && FD_ISSET(icmp_sock, &fds)) {
			get_icmp_pkt(icmp_sock);
		}
		else if (udp_sock != 0 && FD_ISSET(udp_sock, &fds)) {
			get_udp_pkt(udp_sock);
		}
		else
			ASSERT(0);

	}

	return 0;
}
