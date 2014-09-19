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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rip.h"

static int txconnect(RcpInterface *intf) {
	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		ASSERT(0);
		return 0;
	}

	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(char*)&on,sizeof(on)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}


	// bind the socket to the outgoing interface ip address
	struct sockaddr_in s;
	memset(&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(RIP_PORT);
	s.sin_addr.s_addr = htonl(intf->ip);

	if (bind(sock, (struct sockaddr *)&s, sizeof(s)) < 0) {
		perror("binding RIP socket");
		ASSERT(0);
		close(sock);
		return 0;
	}

	return sock;
}


static void txpacket(RcpInterface *intf, uint32_t ip, uint8_t *msg, unsigned len) {
	ASSERT(intf != NULL);
	ASSERT(msg != NULL);
	ASSERT(len != 0);

	int sock = txconnect(intf);
	if (sock == 0) {
		ASSERT(0);
		return;
	}
	

	// set destination address
	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	if (ip == 0)
		addr.sin_addr.s_addr = inet_addr(RIP_GROUP);
	else
		addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port=htons(RIP_PORT);

	if (sendto(sock, msg, len, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("sendto");
		ASSERT(0);
	}
		
	close(sock);
}

static void print_rip_pkt(RcpInterface *intf, RipPacket *pkt, int cnt) {
	char *cmd[] = {"", "request", "response"};
	rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
		"Sending RIP packet of size %d, on interface %d.%d.%d.%d, destination 244.0.0.9, RIP %s, protocol version %d",
		rip_pkt_len(cnt),
		RCP_PRINT_IP(intf->ip),
		cmd[(pkt->command <= 2) ? pkt->command: 0],
		pkt->version);

	if (pkt->command > 2) {
		ASSERT(0);
		return;
	}
		
#if 0
	int i;
	RipRoute *ptr = &pkt->routes[0];
	for (i = 0; i < cnt; i++, ptr++) {
		uint32_t metric = ntohl(ptr->metric);
		uint32_t mask = ntohl(ptr->mask);
		uint32_t ip = ntohl(ptr->ip);
		uint32_t gw = ntohl(ptr->gw);

//		if (trace_prefix == 0 || 
//		      (trace_prefix != 0 && trace_prefix == ip)) {
//		      	if (gw == 0)
//				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
//					"   %d.%d.%d.%d/%d metric %u",
//					RCP_PRINT_IP(ip),
//					mask2bits(mask),
//					metric);
//			else
//				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
//					"   %d.%d.%d.%d/%d metric %u next hop %d.%d.%d.%d",
//					RCP_PRINT_IP(ip),
//					mask2bits(mask),
//					metric,
//					RCP_PRINT_IP(gw));
//		}
	}
#endif
	
}

// transmit request
void txrequest(RcpInterface *intf, RcpRipPartner *net) {
	ASSERT(intf != NULL);
	
	// if net is a neighbor as opposed to a network, send the packets to the specified network ip
	uint32_t ip = 0;
	if (net->mask == 0xffffffff)
		ip = net->ip;
	
	// multicast packets are not sent when passive interface is enabled
	// passive interface allows sending unicast packets to direct neighbors
	if (ip == 0 && intf->rip_passive_interface)
		return;
	
	net->req_tx++;
	
	RipPacket pkt;
	memset(&pkt, 0, sizeof(pkt));
	pkt.command = 1;
	pkt.version = 2;
	pkt.domain = 0;
	pkt.routes[0].metric = htonl(16);
	
	int len = rip_pkt_len(1);
	txpacket(intf, ip, (uint8_t *) &pkt, len);
	print_rip_pkt(intf, &pkt, 1);
}

static void send_response(RcpInterface *intf, uint32_t ip, RipRoute *rt, int cnt) {
	int have_auth = (intf->rip_passwd[0] == '\0')? 0: 1;

	RipPacket pkt;
	memset(&pkt, 0, sizeof(pkt));
	pkt.command = 2;
	pkt.version = 2;
	pkt.domain = 0;
	
	// fill in authentication header
	if (have_auth) {
		RipAuthMd5 *auth = (RipAuthMd5 *) &pkt.routes[0];
		auth->family = 0xffff;
		auth->tag = htons(3);
		auth->offset = htons(rip_pkt_len(cnt + 1));
		auth->key_id = 1;
		auth->auth_len = 20;
		auth->seq = htonl(time(NULL));

		// add the routes
		memcpy(&pkt.routes[1], rt, sizeof(RipRoute) * cnt);
	}
	else 
		// add the routes
		memcpy(&pkt.routes[0], rt, sizeof(RipRoute) * cnt);
	
	// calculate and add md5
	if (have_auth) {
		// set the last route
		pkt.routes[cnt + 1].family = 0xffff;
		pkt.routes[cnt + 1].tag = htons(1);
		
		// calculate digest
		uint8_t secret[16];
		memset(secret, 0, 16);
		memcpy(secret, intf->rip_passwd, strlen(intf->rip_passwd));
		int size = rip_pkt_len(cnt + 2);
		
		MD5_CTX context;
		uint8_t digest[16];
		MD5Init (&context);
		MD5Update (&context, (uint8_t *) &pkt, size - 16);
		MD5Update (&context, secret, 16);
		MD5Final (digest, &context);
#if 0
{
int i;		
uint8_t *p = digest;
printf("tx digest:\n");
for (i = 0; i < 16; i++,p++)
	printf("%02x ", *p);
printf("\n");
}
#endif
		// copy digest
		memcpy((uint8_t *) &pkt.routes[0] + rip_pkt_len(cnt + 1), digest, 16);
	}
	
	int len = rip_pkt_len((have_auth)? cnt + 2: cnt);
	txpacket(intf, ip, (uint8_t *) &pkt, len);
	print_rip_pkt(intf, &pkt, cnt);
}

// transmit response
void txresponse(RcpInterface *intf, int updates_only, RcpRipPartner *net) {
	ASSERT(intf != NULL);

	// if net is a neighbor as opposed to a network, send the packets to the specified network ip
	uint32_t ip = 0;
	if (net->mask == 0xffffffff)
		ip = net->ip;
	
	// multicast packets are not sent when passive interface is enabled
	// passive interface allows sending unicast packets to direct neighbors
	if (ip == 0 && intf->rip_passive_interface)
		return;

	// calculate the number of packets to send out
	RipRoute routes[RIP_MAX_ROUTES];
	int rcnt = ripdb_build_response(intf, &routes[0], updates_only);
	int max_routes_pkt = (intf->rip_passwd[0] == '\0')? MAX_ROUTES_PKT: MAX_ROUTES_PKT - 2;
	int rpkt = rcnt / max_routes_pkt;
	
	// send the packets
	int i;
	for (i = 0; i < rpkt; i++) {
		net->resp_tx++;
		send_response(intf, ip, &routes[i * max_routes_pkt], max_routes_pkt);
	}
	send_response(intf, ip, &routes[i * max_routes_pkt], rcnt % max_routes_pkt);
	net->resp_tx++;
}
