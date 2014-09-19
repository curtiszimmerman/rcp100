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

int rx_open(uint16_t port) {
	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		ASSERT(0);
		return 0;
	}

	// allow reuse of local addresses in the next bind call
	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(char*)&on,sizeof(on)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	//pick up any UDP packet for port 53
	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons(port);
	s.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *)&s, sizeof(s)) < 0) {
		perror("binding DNS socket"); // it could fail if the port is in use by somebody else
		close(sock);
		return 0;
	}

	// set the socket to non-blocking
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	return sock;
		
}		


void rx_packet(int sock) {
	DnsPkt pkt;
	int len;
	struct sockaddr_in fromaddr;
	unsigned int fromlen = sizeof(fromaddr);

	if ((len = recvfrom(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&fromaddr, &fromlen)) == -1) {
		shm->stats.dns_err_rx++;
		return;
	}

	uint32_t src_ip = ntohl(fromaddr.sin_addr.s_addr);
	uint16_t src_port = ntohs(fromaddr.sin_port);

	// verify dns packet integrity
	if (dnspkt_verify(&pkt, len)) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"malformed DNS packet from %d.%d.%d.%d, length %u, packet dropped",
			RCP_PRINT_IP(src_ip), len);
		shm->stats.dns_err_malformed++;
		return;
	}
		
	// is the packet from a server?
	if ((shm->config.name_server1 != 0 && shm->config.name_server1 == src_ip) ||
	     (shm->config.name_server2 != 0 && shm->config.name_server2 == src_ip)) {
	     	// check if this is a server socket
	     	if (rq_sock(sock) == 0) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"unknown DNS server packet from %d.%d.%d.%d, length %u, packet dropped",
				RCP_PRINT_IP(src_ip), len);
			shm->stats.dns_err_unknown++;
	     		return;
	     	}
		process_server(src_ip, src_port, &pkt, len);
	}
	// is the packet from a client?
	else {
		// check if this is the client socket
	     	if (sock != client_sock) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"unknown DNS client packet from %d.%d.%d.%d, length %u, packet dropped",
				RCP_PRINT_IP(src_ip), len);
			shm->stats.dns_err_unknown++;
	     		return;
	     	}
		process_client(src_ip, src_port, &pkt, len);
	}
}
