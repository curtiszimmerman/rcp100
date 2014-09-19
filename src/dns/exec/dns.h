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

#define DNS_SERVER_PORT 53	// receive messages from clients

#define DNS_MAX_REQ 512

// TTL
#define DNS_MIN_TTL 10 // values smaller than this will not be cached
#define DNS_MAX_TTL 900 // 15 minutes - values bigger than this will be truncated
#define DNS_STATIC_TTL (60 * 60) // one hour
#define DNS_STATIC_FLAGS 0x8180

typedef struct dnspkt_t {
	//*******************************************
	// dns packet format starting here
	//*******************************************
	uint16_t tid;
	uint16_t flags;
	uint16_t queries;
	uint16_t answers;
	uint16_t authority;
	uint16_t additional;
	unsigned char padding[1500 - 12];	// padding up to 1500 bytes
	
	//*******************************************
	// data extracted from the packet
	//*******************************************
	int len;	// packet length
	uint8_t dns_pkt_type;	// 0 -query, 1 - response
	uint8_t dns_error_code;	// 0 - no error
	uint16_t dns_flags;		// flags
	// query
	unsigned char dns_query_name[CLI_MAX_DOMAIN_NAME + 1];
	uint8_t dns_query_type;
	// answer
	uint16_t dns_type;
	uint16_t dns_class;
	int dns_ttl;
	uint32_t dns_address;
	uint8_t dns_address_v6[16];
	
} DnsPkt;


typedef struct dnsreq_t {
	struct dnsreq_t *next;	// linked list
	
#define MAX_REQ_TTL 10	// wait not more than 10 seconds for a response from the server
	int16_t ttl;
	uint16_t query_type;
	
	// client data
	uint16_t client_port;	
	uint32_t client_ip;
	uint16_t client_tid;		// original transaction id
	uint16_t new_tid;		// new transaction id
	
	// server data
	uint32_t server_ip;	// request forwarded to this server
	int sock;	// server socket listening for responses
	uint16_t port;		// random port
	
	// request data
	char query_name[CLI_MAX_DOMAIN_NAME + 1];
} DnsReq;

typedef struct dnscache_t {
	struct dnscache_t *next;	// linked list
	uint32_t dns_ttl;
	uint32_t dns_ip;
	uint16_t dns_flags;		// flags
	uint8_t dns_static;		// static entry flag
	uint8_t marker;		// for removing unwanted entries
	uint8_t type;	// dns type: 1 - ipv4 address, 5 - cname, 28 - ipv6 address
	uint8_t dns_ipv6[16];
	DnsPkt *dns_cname;
	int dns_cname_len;
	char dns_name[CLI_MAX_DOMAIN_NAME +1];
} DnsCache;


// main.c
extern int client_sock;
volatile int force_restart;
volatile int force_shutdown;
extern int proxy_disabled;

// rcp_init.c
extern RcpShm *shm;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);

// callbacks.c
int processCli(RcpPkt *pkt);

// cmds.c
int module_initialize_commands(CliNode *head);

// dns_rx.c
int rx_open(uint16_t port);
void rx_packet(int sock);

// dns_client.c
extern uint16_t rate_limit;
void process_client(uint32_t ip, uint16_t port, DnsPkt *pkt, int len);

// dns_server.c
void process_server(uint32_t ip, uint16_t port, DnsPkt *pkt, int len);

// dns_rq.c
void rq_list_init(void);
void rq_clear_inactive(void);
void rq_clear_all(void);
DnsReq *rq_active(void);
DnsReq *rq_malloc(void);
void rq_free(DnsReq *rq);
int rq_count(void);
void rq_add(DnsReq *ptr);
DnsReq *rq_find(uint16_t tid);
DnsReq *rq_del(DnsReq *ptr);
void rq_clear_all(void);
void rq_timer(void);
int rq_sock(int sock);

// dns_pkt.c
int dnspkt_verify(DnsPkt *pkt, int len);
void dnspkt_print(DnsPkt *pkt);
const char *dnspkt_get_real_name(unsigned char *ptr);
const char *dnspkt_build_name(char *ptr);
int dnspkt_build(DnsPkt *pkt, DnsCache *dc, uint32_t tid);

// dns_cache.h
DnsCache *cache_find(const char *name, uint16_t type);
void cache_add(DnsCache *ptr);
void cache_update_static(void);
void cache_remove(DnsCache *cptr);
void cache_print(FILE *fp);
void cache_timer(void);
void cache_clear(void);

#endif
