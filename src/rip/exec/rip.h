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
#ifndef RIP_H
#define RIP_H

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
#include "librcp.h"

#define RIP_GROUP "224.0.0.9"
#define RIP_PORT 520
#define RIP_MAX_ROUTES 32000	// maximum route capacity
#define RIP_ROUTE_TIMEOUT (shm->config.rip_update_timer * 6)
#define RIP_ROUTE_REPLACE_TIMEOUT (shm->config.rip_update_timer * 2)  // replacing routes with equal routes
#define RIP_ROUTE_CLEANUP (shm->config.rip_update_timer * 4)

typedef struct {
	uint16_t family;	// address family
	uint16_t tag;	// route tag
	uint32_t ip;	// ip address
	uint32_t mask;	// subnet mask
	uint32_t gw;	// next hop
	uint32_t metric;	// metric
} RipRoute;

typedef struct {
	uint16_t family;	// 0xffff
	uint16_t tag;	// auth type - 0x0001
	uint8_t passwd[16];// password
} RipAuthSimple;

typedef struct {
	uint16_t family;	// 0xffff
	uint16_t tag;	// auth type - 0x0003
	uint16_t offset;	// offset
	uint8_t key_id;	// key id
	uint8_t auth_len;	// length of auth field - 20 bytes for md5
	uint32_t seq;	// sequence number
	uint8_t pad[8];	// zero padding
} RipAuthMd5;

// Note: sizeof(RipAuthMd5) == sizeof(RipAuthSimple) == sizeof(RipRoute)

static inline int nullRipRoute(RipRoute *ptr) {
	if (ptr->family == 0 &&
	     ptr->tag == 0 &&
	     ptr->ip == 0 &&
	     ptr->mask == 0 &&
	     ptr->gw == 0 &&
	     ptr->metric == 0)
	     	return 1;
	return 0;
}


#define MAX_ROUTES_PKT 25 // maximum number of routes in a packet
typedef struct rip_pkt {
	uint8_t command;		// command
	uint8_t version;		// protocol version
	uint16_t domain;		// routing domain
	RipRoute routes[MAX_ROUTES_PKT]; 	// maximum 25 allowed
} RipPacket;

static inline int rip_pkt_len(int routes) {
	ASSERT(routes <= MAX_ROUTES_PKT);
	return 4 + sizeof(RipRoute) * routes;
}

typedef struct packet_t {
	RipPacket data;
	uint32_t ip_source;
	uint32_t ip_dest;
	unsigned if_index;
} Packet;

typedef struct rip_neighbor_t {
	struct rip_neighbor_t *next;
	
	uint32_t ip;
	uint32_t rx_time;
	uint32_t auth_seq;
	uint32_t errors;
	uint32_t route_errors;
	uint32_t md5_errors;
} RipNeighbor;

// main.c
extern int rxsock;
extern int update_redist;

// rcp_init.c
extern RcpShm *shm;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);

// rip_rx.c
extern uint32_t trace_prefix;
RipNeighbor *rxneighbors(void);
int rxconnect(void);
int rxmultijoin(int sock, uint32_t ifaddress);
int rxmultidrop(int sock, uint32_t ifaddress);
void rxpacket(int sock);
void rxneigh_update(void);

// rip_tx.c
void txrequest(RcpInterface *intf, RcpRipPartner *net);
void txresponse(RcpInterface *intf, int updates_only, RcpRipPartner *net);

// rip_db.c
extern int ripdb_update_available;
int ripdb_add(RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t metric, uint32_t partner, RcpInterface *interface);
int ripdb_delete(RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t partner);
void ripdb_delete_interface(RcpInterface *intf);
void ripdb_delete_type(RcpRouteType type);
void ripdb_timeout(void);
void ripdb_print(FILE *fp);
// returns the number of routes filled
int ripdb_build_response(RcpInterface *intf, RipRoute *rt, int updates_only);
void ripdb_clean_updates(void);
void ripdb_delete_all(void);
int ripdb_delete_rip(uint32_t ip, uint32_t mask);
void ripdb_route_request(void);

// rip_interface.c
RcpRipPartner *find_network_for_interface(RcpInterface *intf);
void rip_update_interfaces(void);
void rip_send_timeout_updates(void);
void rip_send_delta_updates(void);
void rip_update_connected(void);
void rip_update_connected_subnets(void);
void rip_update_static(void);
void ripdb_redistribute_route(int add, uint32_t ip, uint32_t mask, uint32_t gw);

// callbacks.c
extern int cli_html;
int processCli(RcpPkt *pkt);
void route_request(void);

// cmds.c
int module_initialize_commands(CliNode *head);

// restart.c
void restart();
#endif
