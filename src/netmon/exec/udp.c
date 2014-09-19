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

int udp_enabled = 0;

void udp_flag_update(void) {
	RcpNetmonHost *ptr;
	int i;

	// update netmon status
	int newflag = 0;

	// clean hosts
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (ptr->type == RCP_NETMON_TYPE_NTP || ptr->type == RCP_NETMON_TYPE_DNS) {
			if (ptr->valid)
				newflag = 1;
		}
	}
	
	// update run flag and start time
	if (newflag != udp_enabled)
		udp_enabled = newflag;
}


int open_udp_sock(void) {
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

	//pick up any UDP packet for port RCP_PORT_MONITOR_UDP
	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons(RCP_PORT_MONITOR_UDP);
	s.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *)&s, sizeof(s)) < 0) {
		perror("binding netmon socket");
		ASSERT(0);
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

void get_udp_pkt(int sock) {
	unsigned char packet[1500];
	int len;
	struct sockaddr_in fromaddr;
	unsigned int fromlen = sizeof(fromaddr);

	if ((len = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&fromaddr, &fromlen)) == -1) {
		return;
	}
	
	uint32_t src_ip = ntohl(fromaddr.sin_addr.s_addr);
	uint16_t src_port = ntohs(fromaddr.sin_port);

	// find the host
	RcpNetmonHost *ptr;
	int i;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->ip_sent != src_ip)
			continue;
		
		if (ptr->type == RCP_NETMON_TYPE_NTP)
			get_ntp(src_port, ptr, packet, len);
		if (ptr->type == RCP_NETMON_TYPE_DNS)
			get_dns(src_port, ptr, packet, len);
	}
}

void udp_send(int sock) {
	if (sock == 0)
		return;
		
	RcpNetmonHost *ptr;
	int i;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->ip_resolved == 0)
			continue;
		
		if (ptr->type == RCP_NETMON_TYPE_NTP)
			send_ntp(sock, ptr);
		else if (ptr->type == RCP_NETMON_TYPE_DNS)
			send_dns(sock, ptr);
	}
}

void monitor_udp(void) {
	RcpNetmonHost *ptr;
	int i;

	int dns = 0;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->type == RCP_NETMON_TYPE_DNS) {
			monitor_dns(ptr);
			dns = 1;
			continue;
		}
		else if (ptr->type == RCP_NETMON_TYPE_NTP) {
			monitor_ntp(ptr);
			continue;
		}
		
	}
	
	if (dns)
		reset_dns_pkt();
}

