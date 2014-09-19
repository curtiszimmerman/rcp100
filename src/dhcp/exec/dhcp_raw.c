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

//*********************************************************************************
// raw socket - socket used for sending packets to clients
//*********************************************************************************
static int rsocket = 0;
static int rawconnect(void) {
	int s;

	// open a raw socket
	s = socket(AF_PACKET, SOCK_DGRAM, htons(ETHERTYPE_IP));
	if (s == -1) {
		ASSERT(0);
		return 0;
	}
	rsocket = s;
	return s;
}

static void set_ip(Packet *pkt, uint32_t ifip) {
	int len = pkt->pkt_len;
	uint32_t sum;
	unsigned int i;

	pkt->ip.ip_p = IPPROTO_UDP;
	pkt->ip.ip_src.s_addr = htonl(ifip);
	pkt->ip.ip_dst.s_addr = INADDR_BROADCAST;
	pkt->ip.ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + len) ;
	pkt->ip.ip_hl = sizeof(struct ip) / 4;
	pkt->ip.ip_v = IPVERSION;
	pkt->ip.ip_tos = 0;
	pkt->ip.ip_id = htons(0);
	pkt->ip.ip_off = htons(0x4000); // no fragmentation
	pkt->ip.ip_ttl = IPDEFTTL;
	pkt->ip.ip_sum = 0;
	for (sum = 0, i = 0; i < sizeof(struct ip) / 2; i++)
		sum += ((uint16_t *)&pkt->ip)[i];
	while (sum>>16)
		sum = (sum & 0xffff) + (sum >> 16);
	pkt->ip.ip_sum = (sum == 0xffff) ? sum : ~sum;
}


static void set_udp(Packet *pkt) {
	int len = pkt->pkt_len;
	uint32_t sum;
	unsigned int i;

	pkt->udp.source = htons(DHCP_SERVER_PORT);
	pkt->udp.dest = htons(DHCP_CLIENT_PORT);

	((uint8_t *)&pkt->dhcp)[len] = 0;
	pkt->udp.check = 0;
	pkt->udp.len = sum = htons(sizeof(struct udphdr) + len);
	sum += htons(IPPROTO_UDP);
	for (i = 0; i < 4; i++)
		sum += ((uint16_t *)&pkt->ip.ip_src)[i];
	for (i = 0; i < (sizeof(struct udphdr) + len + 1) / 2; i++)
		sum += ((uint16_t *)&pkt->udp)[i];
	while (sum>>16)
		sum = (sum & 0xffff) + (sum >> 16);
	pkt->udp.check = (sum == 0xffff) ? sum : ~sum;
}

void txrawpacket(Packet *pkt, const char *ifname, uint32_t ifip, int kindex) {
	// bring up rsocket
	if (rsocket == 0) {
		if (rawconnect() == 0) {
			ASSERT(0);
			return;
		}
	}
	ASSERT(rsocket != 0);
	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
                                  "forwarding dhcp client packet on interface %s", ifname);
	
//	// bind rsocket to the outgoing interface ifname
//	struct sockaddr_ll sll;
//	sll.sll_family = AF_PACKET;
//	sll.sll_ifindex = kindex;
//	sll.sll_protocol =  htons(ETHERTYPE_IP);
//	if ((bind(rsocket, (struct sockaddr *)&sll, sizeof(sll)))== -1) {
//		ASSERT(0);
//		close(rsocket);
//		rsocket = 0;
//		return;
//	}


	// set ethernet header
	struct sockaddr_ll dest_addr;
	dest_addr.sll_family = AF_PACKET;
	dest_addr.sll_halen =  ETHER_ADDR_LEN;
	dest_addr.sll_ifindex = kindex;
	dest_addr.sll_protocol = htons(ETHERTYPE_IP);
	memcpy(dest_addr.sll_addr, pkt->dhcp.chaddr, ETHER_ADDR_LEN);

	// set ip header
	set_ip(pkt, ifip);
	
	// udp setup
	set_udp(pkt);

	// send packet
	errno = 0;
	while (sendto(rsocket, pkt, ntohs(pkt->ip.ip_len), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
		if (errno != EINTR) {
			ASSERT(0);
		}
	}
}


