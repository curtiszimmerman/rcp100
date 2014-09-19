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

// transmit dhcp packet to a server
void txpacket(int sock, Packet *pkt, uint32_t ip) {
	ASSERT(sock != 0);
	ASSERT(pkt != NULL);
	ASSERT(ip != 0);
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(DHCP_SERVER_PORT);

	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
                                  "forwarding packet to server %d.%d.%d.%d", RCP_PRINT_IP(ip));

	errno = 0;
	sendto(sock, &pkt->dhcp, pkt->pkt_len, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr));
}

// transmit dhcp packet to a client
void txpacket_client(int sock, Packet *pkt, uint32_t ip) {
	ASSERT(sock != 0);
	ASSERT(pkt != NULL);
	ASSERT(ip != 0);
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(DHCP_CLIENT_PORT);

	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
                                  "forwarding packet to server %d.%d.%d.%d", RCP_PRINT_IP(ip));

	errno = 0;
	sendto(sock, &pkt->dhcp, pkt->pkt_len, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr));
}

// check if this is one of our servers
int check_server(uint32_t ip) {
	if (ip == 0)
		return 0;

	int i;
	RcpDhcprServer *ptr;
	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		int j;
		for (j = 0; j < RCP_DHCP_SERVER_LIMIT; j++) {
			if (ptr->server_ip[j] == 0)
				continue;
			if (ptr->server_ip[j] == ip)
				return 1;
		}
	}

	return 0;
}

void update_lease_count(uint32_t ip) {
	if (ip == 0)
		return;

	int i;
	RcpDhcprServer *ptr;
	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		int j;
		for (j = 0; j < RCP_DHCP_SERVER_LIMIT; j++) {
			if (ptr->server_ip[j] == 0)
				continue;
			if (ptr->server_ip[j] == ip) {
				ptr->leases[j]++;
				return;	
			}
		}
	}

}

void txpacket_all(int sock, Packet *pkt) {
	int i;
	RcpDhcprServer *ptr;
	
	// send the packet to all servers in all groups
	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		int j;
		for (j = 0; j < RCP_DHCP_SERVER_LIMIT; j++) {
			if (ptr->server_ip[j] == 0)
				continue;
			txpacket(sock, pkt, ptr->server_ip[j]);
		}
	}
}

// send the packet to a random group
void txpacket_random_balancing(int sock, Packet *pkt) {
	int cnt = 0;
	int i;
	RcpDhcprServer *ptr;

	// count the server goups
	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cnt++;
	}
	if (cnt == 0) {
		return;
	}
	
	// jump to a random server group
	int index = rand() % cnt;
	for (i = 0, cnt = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, cnt++, ptr++) {
		if (!ptr->valid)
			continue;

		if (cnt == index) {
			// send the packet to all servers in this group
			int j;
			for (j = 0; j < RCP_DHCP_SERVER_LIMIT; j++) {
				if (ptr->server_ip[j] == 0)
					continue;
				txpacket(sock, pkt, ptr->server_ip[j]);
			}

			return;
		}
	}
}	
