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
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include "dhcp.h"

#define PING_ID 0x4df6
#define PING_SEQ 0x53c7

// icmp queue
static Packet *icmp_queue = NULL;

static int icmp_cksum(unsigned short *buf, int sz) {
	int nleft = sz;
	int sum = 0;
	unsigned short *w = buf;
	unsigned short ans = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *) (&ans) = *(unsigned char *) w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return (ans);
}

int open_icmp_sock(void) {
	int sock;
	
	// protocol 1 - ICMP
	if ((sock = socket(AF_INET, SOCK_RAW, 1)) < 0) {
		ASSERT(0);
		perror("cannot open raw socket");
		exit(1);
	}

	// set the socket to non-blocking
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		ASSERT(0);
		perror("cannot make the socket nonblocking");
		exit(1);
	}

	return sock;
}

void get_icmp_pkt(int sock) {
	unsigned char packet[1500];
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	int c;

	if ((c = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *) &from, &fromlen)) < 0) {
		return;
	}
	
	struct iphdr *iphdr = (struct iphdr *) packet;
	
	struct icmp *pkt;
	pkt = (struct icmp *) (packet + (sizeof(struct iphdr)));
	if (pkt->icmp_type == ICMP_ECHOREPLY) {
		if (pkt->icmp_hun.ih_idseq.icd_id == htons(PING_ID) &&
		     pkt->icmp_hun.ih_idseq.icd_seq == htons(PING_SEQ)) {
			uint32_t ip = ntohl(iphdr->saddr);

			// mark the packet in icmp queue
			Packet *pkt = icmp_queue;
			while (pkt) {
				if (pkt->icmp_lease_ip == ip) {
					rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
						"received ICMP echo response from %d.%d.%d.%d\n", RCP_PRINT_IP(ip));
					rcpLog(muxsock, RCP_PROC_DHCP, RLOG_INFO, RLOG_FC_DHCP,
						"unknown network client %d.%d.%d.%d\n", RCP_PRINT_IP(ip));
					pkt->icmp_response = 1;
					pkt->icmp_ttl = 0; // the packet will be removed in the next queue cycle
					if (pkt->icmp_pif) {
						pkt->icmp_pif->stats.dhcp_err_rx++;
						pkt->icmp_pif->stats.dhcp_err_icmp++;
					}
					break;
				}
				pkt = pkt->icmp_next;
			}
		}
	}
}

#if 0
static void print_queue(void) {
	Packet *pkt = icmp_queue;
	while (pkt) {
		printf("%d.%d.%d.%d %d\n", RCP_PRINT_IP(pkt->icmp_lease_ip), pkt->icmp_ttl);
		pkt = pkt->icmp_next;
	}
}
#endif

// return 0 if OK, 1 if error
int send_icmp_pkt(int sock, uint32_t ip, Packet *dhcp_pkt) {
	ASSERT(dhcp_pkt);
#define PINGSIZE 64
	char packet[PINGSIZE];
	struct sockaddr_in addr; 
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;

	// set address
	addr.sin_addr.s_addr = htonl(ip);
	
	// build icmp
	struct icmp *pkt = (struct icmp *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_hun.ih_idseq.icd_id = htons(PING_ID);
	pkt->icmp_hun.ih_idseq.icd_seq = htons(PING_SEQ);
	pkt->icmp_cksum = icmp_cksum((unsigned short *) pkt, sizeof(packet));

	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
		"sending ICMP echo request to %d.%d.%d.%d\n", RCP_PRINT_IP(ip));
	sendto(sock, packet, sizeof(packet), 0,
		(struct sockaddr *) &addr, sizeof(struct sockaddr_in));

	// put dhcp packet into the queue
	dhcp_pkt->icmp_lease_ip = ip;
	dhcp_pkt->icmp_next = icmp_queue;
	dhcp_pkt->icmp_ttl = 2;
	icmp_queue = dhcp_pkt;
	return 0;
}

static inline void send_dhcp_pkt(Packet *pkt) {
	ASSERT(pkt);

	// transmit raw packet
	if (pkt->icmp_pif) {
		pkt->icmp_pif->stats.dhcp_tx++;
		pkt->icmp_pif->stats.dhcp_tx_offer++;
	}
	if (pkt->icmp_giaddr == 0)
		txrawpacket(pkt, pkt->icmp_pif->name, pkt->icmp_pif->ip, pkt->icmp_pif->kindex);
	else
		txpacket(pkt->icmp_udp_sock, pkt, pkt->icmp_giaddr);

}

void icmp_timeout(void) {
//print_queue();
	Packet *ptr = icmp_queue;
	if (!ptr)
		return;
		
	Packet *prev = NULL;
	while (ptr) {
		Packet *next = ptr->icmp_next;
		if (--ptr->icmp_ttl <= 0) {
			if (ptr->icmp_response == 0)
				// we didn't receive any response to our ping, forward the  dhcp packet
				send_dhcp_pkt(ptr);
			
			// unlink packet
			if (prev)
				prev->icmp_next = next;
			else
				icmp_queue = next;				
		
			// free the packet
			free(ptr);
		}
		else
			prev = ptr;
		ptr = next;
	}
}
		
