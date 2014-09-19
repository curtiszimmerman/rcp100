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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/signal.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <stdint.h>

#define PING_ID 0x9cd0
#define PING_SEQ 0x34d1
int ping_enabled = 0;

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

//static void printmem(void *mem, int size) {
//	int i;
//	unsigned char *ptr = mem;
//	
//	for (i = 0; i < size; i++, ptr++) {
//		printf("%02x ", *ptr & 0xff);
//	}
//	printf("\n");
//}

void get_icmp_pkt(int sock) {
	unsigned char packet[1500];
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	int c;

	if ((c = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *) &from, &fromlen)) < 0) {
		return;
	}
	
	
	struct iphdr *iphdr = (struct iphdr *) packet;
//	printmem(iphdr, sizeof (struct iphdr));
	
	struct icmp *pkt;
	pkt = (struct icmp *) (packet + (sizeof(struct iphdr)));
	if (pkt->icmp_type == ICMP_ECHOREPLY) {
		if (pkt->icmp_hun.ih_idseq.icd_id == htons(PING_ID) &&
		     pkt->icmp_hun.ih_idseq.icd_seq == htons(PING_SEQ)) {
		
			uint32_t ip = ntohl(iphdr->saddr);
			uint8_t index = packet[sizeof(struct iphdr) + sizeof(struct icmp)];
			if (index < RCP_NETMON_LIMIT) {
				RcpNetmonHost *nm = &shm->config.netmon_host[index];
				if (nm->valid && nm->ip_sent == ip) {
					nm->pkt_received++;
					nm->interval_pkt_received++;
					if (nm->pkt_received > nm->pkt_sent) {
						nm->pkt_received = nm->pkt_sent;
						nm->interval_pkt_received = nm->interval_pkt_sent;
					}
					
					nm->wait_response = 0;
					
					// calculate the response time
					struct timeval end_tv;
					gettimeofday (&end_tv, NULL);
					unsigned resp_time = delta_tv(&nm->start_tv, &end_tv);
//printf("received ping reply from %d.%d.%d.%d, response time %fs\n",
//	RCP_PRINT_IP(ip), (float) resp_time / (float) 1000000);
					nm->resptime_acc += resp_time;
					nm->resptime_samples++;
				}
			}
		}
	}
}

// return 0 if OK, 1 if error
static int send_ping_pkt(int sock, RcpNetmonHost *nm, int index) {
	if (nm->ip_resolved == 0)
		return 1;	// address not available yet

#define PINGSIZE 64
	char packet[PINGSIZE];
	struct sockaddr_in addr; 
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;

	// set address
	addr.sin_addr.s_addr = htonl(nm->ip_resolved);
	
	// build icmp
	struct icmp *pkt = (struct icmp *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_hun.ih_idseq.icd_id = htons(PING_ID);
	pkt->icmp_hun.ih_idseq.icd_seq = htons(PING_SEQ);
	// place the index in the body of the message
	packet[sizeof (struct icmp)] = (unsigned char) index;
	pkt->icmp_cksum = icmp_cksum((unsigned short *) pkt, sizeof(packet));

	// store the destination ip in nm structure
	nm->ip_sent = ntohl(addr.sin_addr.s_addr);	

	sendto(sock, packet, sizeof(packet), 0,
		(struct sockaddr *) &addr, sizeof(struct sockaddr_in));

	gettimeofday(&nm->start_tv, NULL);
	return 0;
}

void ping_all(int sock) {
	RcpNetmonHost *ptr;
	int i;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_NETMON_TYPE_ICMP)
			continue;
			
		int rv = send_ping_pkt(sock, ptr, i);
		ptr->wait_response = 1;

		if (rv == 0) {
			ptr->pkt_sent++;
			ptr->interval_pkt_sent++;
		}
	}
}

void ping_monitor(void) {
	RcpNetmonHost *ptr;
	int i;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_NETMON_TYPE_ICMP)
			continue;
			
		if (ptr->wait_response == 0) {
			if (ptr->status == 0) {
				ptr->status = 1;
				rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
					"Host %s UP", ptr->hostname);
			}
		}
		else if (ptr->wait_response) {
			if (ptr->status == 1) {
				ptr->status = 0;
				rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
					"Host %s DOWN", ptr->hostname);
			}
		}
		ptr->wait_response = 0;
	}
}

void ping_flag_update(void) {
	RcpNetmonHost *ptr;
	int i;

	// update netmon status
	int newflag = 0;

	// clean hosts
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (ptr->type == RCP_NETMON_TYPE_ICMP) {
			if (ptr->valid)
				newflag = 1;
		}
	}
	
	// update run flag and start time
	if (newflag != ping_enabled)
		ping_enabled = newflag;
}

