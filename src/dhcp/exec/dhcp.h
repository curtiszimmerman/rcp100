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
#ifndef DHCP_H
#define DHCP_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "librcp.h"

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

typedef struct {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t tid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t chaddr[16];
	uint8_t sname[64];
	uint8_t file[128];
	uint32_t cookie;
	uint8_t options[308 + 824];

	// as of RFC 2131, the maximum DHCP packet size is 576 including IP and UDP header;
	// the packet size can be increased using Maximum Message Size option;
	// we leave enough room for a packet of 1400 bytes size
#define MAX_DHCP_PKT_SIZE (1400 - sizeof(struct ip) - sizeof(struct udphdr))
} DhcpPacket;

typedef struct packet_t {
	struct ip ip;		
	struct udphdr udp;		
	DhcpPacket dhcp;  		
	// real packet space ends here
	
	// protocol information - starts at offset 1400 in the structure
	uint32_t ip_source;
	uint32_t ip_dest;
	uint32_t if_address;
	unsigned if_index;
	
	unsigned char *pkt_type;
	uint32_t server_id;
	uint32_t lease_time;
	uint32_t subnet_mask;
	uint32_t requested_ip;
	unsigned char *pkt_82;
	unsigned char *pkt_end;
	unsigned pkt_len; // length of dhcp packet without ip and udp headers
	
	// icmp queue
	struct packet_t *icmp_next;
	int icmp_ttl;
	unsigned icmp_response;
	uint32_t icmp_giaddr;
	uint32_t icmp_lease_ip;
	unsigned icmp_udp_sock;
	RcpInterface *icmp_pif;
} Packet;

// rcp_init.c
extern RcpShm *shm;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);


// dhcp_rx.c
int rxconnect(void);
void rxpacket(int sock, int icmp_sock);

// dhcp_rx_server.c
void dhcp_rx_server(int sock, int icmp_sock, Packet *pkt, RcpInterface *pif);

// dhcp_tx.c
void txpacket(int sock, Packet *pkt, uint32_t ip);
void txpacket_client(int sock, Packet *pkt, uint32_t ip);
void txpacket_all(int sock, Packet *pkt);
void txpacket_random_balancing(int sock, Packet *pkt);
int check_server(uint32_t ip);
void update_lease_count(uint32_t ip);

// dhcp_raw.c
void txrawpacket(Packet *pkt, const char *ifname, uint32_t ifip, int kindex);


// callbacks.c
extern RcpPkt *pktout;
int processCli(RcpPkt *pkt);

// cmds.c
int module_initialize_commands(CliNode *head);

// icmp.c
int open_icmp_sock(void);
void get_icmp_pkt(int sock);
int send_icmp_pkt(int sock, uint32_t ip, Packet *pkt);
void icmp_timeout(void);

#endif
