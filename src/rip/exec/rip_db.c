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
#include "rip.h"
int ripdb_update_available = 0;
extern int static_update_trigger;

// a simple hash table with a 256 index
typedef struct ripdb_elem_t {
	struct ripdb_elem_t *next;
	
	uint32_t ip;
	uint32_t mask;
	uint32_t gw;
	uint32_t metric;
	uint32_t partner;	// who advertised the route
	RcpInterface *interface;
	uint8_t type;	//RcpRouteType 
	uint8_t timeout;	// route countdown timer
	uint8_t gc;	// garbage collection
	uint8_t updated;	// update flag
} RipDbElem;

static int initialized = 0;
static RipDbElem *ripdb_index[256];

static void inline initialize(void) {
	if (initialized == 0) {
		memset(ripdb_index, 0, sizeof(ripdb_index));
	}
	initialized = 1;
}

static inline uint8_t hash(uint32_t ip) {
	uint32_t rv =  ((ip & 0xff000000) >> 24) ^
	             ((ip & 0xff0000) >> 16) ^
	             ((ip & 0xff00) >> 8) ^
	             (ip & 0xff);
	return (uint8_t) rv;
}

static RipDbElem *find_exact(RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t partner) {
//printf("find type %d, %d.%d.%d.%d/%d, %d.%d.%d.%d, partner %d.%d.%d.%d\n",
//type,
//RCP_PRINT_IP(ip), mask2bits(mask),
//RCP_PRINT_IP(gw),
//RCP_PRINT_IP(partner));

	uint8_t hval = hash(ip);
	RipDbElem *ptr = ripdb_index[hval];
	while (ptr != NULL) {
//printf("searching type %d, %d.%d.%d.%d/%d, %d.%d.%d.%d, partner %d.%d.%d.%d\n",
//type,
//RCP_PRINT_IP(ip), mask2bits(mask),
//RCP_PRINT_IP(gw),
//RCP_PRINT_IP(partner));
		if (ptr->type == type &&
		     ptr->ip == ip &&
		     ptr->mask == mask &&
		     ptr->gw == gw &&
		     ptr->partner == partner)
		     	return ptr;
		ptr = ptr->next;
	}
	
	return NULL;
}

static RipDbElem *find(uint32_t ip, uint32_t mask) {
	uint8_t hval = hash(ip);
	RipDbElem *ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->ip == ip &&
		     ptr->mask == mask)
		     	return ptr;
		ptr = ptr->next;
	}
	
	return NULL;
}



// redistribute route into RIP
void ripdb_redistribute_route(int add, uint32_t ip, uint32_t mask, uint32_t gw) {
	uint32_t destination = 0;
	
	// send rcp route message to rip
	if (rcpOspfRedistRip())
		destination |= RCP_PROC_OSPF;

	// send rcp route message to clients
	if (muxsock && destination) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = destination;
		
		if (!add)
			pkt.type = RCP_PKT_TYPE_DELROUTE;
		else
			pkt.type = RCP_PKT_TYPE_ADDROUTE;

		pkt.pkt.route.type = RCP_ROUTE_RIP;
		pkt.pkt.route.ip = ip;
		pkt.pkt.route.mask = mask;
		pkt.pkt.route.gw = gw;
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}	
}


static void ecmp_del(uint32_t ip, uint32_t mask) {
	uint8_t hval = hash(ip);

	// count ecmp routes
	int cnt = 0;
	RipDbElem *found = NULL;
	RipDbElem *ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->ip == ip && ptr->mask == mask && ptr->metric < 16 && ptr->type == RCP_ROUTE_RIP) {
		     	cnt++;
		     	found = ptr;
		}
		ptr = ptr->next;
	}

	// nothing to delete
	if (cnt == 0)
		return;
	
	// single route
	else if (cnt == 1) {
		ASSERT(found);
		rcpDelRoute(muxsock, found->ip, found->mask, found->gw);
		ripdb_redistribute_route(0, found->ip, found->mask, found->gw);
		return;
	}
	
	// ecmp routes
	uint32_t gw[cnt];
	int gw_cnt = 0;
	ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->ip == ip && ptr->mask == mask && ptr->metric < 16 && ptr->type == RCP_ROUTE_RIP) {
		     	gw[gw_cnt] = ptr->gw;
		     	gw_cnt++;
		}
		ripdb_redistribute_route(0, ip, mask, ptr->gw);
		ptr = ptr->next;
	}
	rcpDelECMPRoute(muxsock, RCP_ROUTE_RIP, ip, mask, gw_cnt, gw);
}

static void ecmp_add(uint32_t ip, uint32_t mask) {
	uint8_t hval = hash(ip);

	// count ecmp routes
	int cnt = 0;
	RipDbElem *found = NULL;
	RipDbElem *ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->ip == ip && ptr->mask == mask && ptr->metric < 16 && ptr->type == RCP_ROUTE_RIP) {
		     	cnt++;
		     	found = ptr;
		}
		ptr = ptr->next;
	}

	// nothing to add
	if (cnt == 0)
		return;
	
	// single route
	else if (cnt == 1) {
		ASSERT(found);
		rcpAddRoute(muxsock, RCP_ROUTE_RIP, found->ip, found->mask, found->gw, found->metric);
		ripdb_redistribute_route(1, found->ip, found->mask, found->gw);
		return;
	}
	
	// ecmp routes
	uint32_t gw[cnt];
	int gw_cnt = 0;
	ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->ip == ip && ptr->mask == mask && ptr->metric < 16 && ptr->type == RCP_ROUTE_RIP) {
		     	gw[gw_cnt] = ptr->gw;
		     	gw_cnt++;
		}
		ripdb_redistribute_route(1, ip, mask, ptr->gw);
		ptr = ptr->next;
	}
	rcpAddECMPRoute(muxsock, RCP_ROUTE_RIP, ip, mask, found->metric, gw_cnt, gw);
}

static void clean_route(RipDbElem *ptr) {
	// delete all ecmp routes
	if (ptr->type == RCP_ROUTE_RIP) {
		// delete the route from kernel
		ecmp_del(ptr->ip, ptr->mask);
		
		// schedule a static update in 10 seconds if redist static enabled
		if (shm->config.rip_redist_static)
			static_update_trigger = 10;
	}

	// mark the route as deleted
	ptr->metric = 16;
	ptr->timeout = 0;
		
	// add back the remaining ecmp routes
	if (ptr->type == RCP_ROUTE_RIP) {
		// add the route to kernel
		ecmp_add(ptr->ip, ptr->mask);
	}

	// start the garbage collection
	if (ptr->gc == 0) {
		ptr->gc = RIP_ROUTE_CLEANUP;
		ptr->updated = 1;
		ripdb_update_available = 1;
	}
		
}

// return 1 if type/metric is better, 0 if equal, -1 if worst
static inline int compare_type_metric(RcpRouteType type, uint32_t metric, RipDbElem *rt) {
	if (metric < rt->metric)
		return 1;
	else if (metric == rt->metric) {
		// check the type
		if (type < rt->type)
			return 1;
		else if (type == rt->type)
			return 0;
		else
			return -1;
	}
	else
		return -1;
}

static void clean_metric(uint32_t ip, uint32_t mask) {
	uint8_t hval = hash(ip);
	
	// walk the list and record the best metric
	uint32_t best_metric = 16;
	RipDbElem *ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->type == RCP_ROUTE_RIP && ptr->ip == ip && ptr->mask == mask && ptr->metric < best_metric)
			best_metric = ptr->metric;
		ptr = ptr->next;
	}
	
	if (best_metric == 16) // nothing to do
		return;
	
	// clean all the routes with a bigger metric
	ptr = ripdb_index[hval];
	while (ptr != NULL) {
		if (ptr->type == RCP_ROUTE_RIP && ptr->ip == ip && ptr->mask == mask && ptr->metric > best_metric) {
			// mark the route as deleted
			ptr->metric = 16;
			ptr->timeout = 0;

			// start the garbage collection if not started alerady
			if (ptr->gc == 0) {
				ptr->gc = RIP_ROUTE_CLEANUP;
				ptr->updated = 1;
				ripdb_update_available = 1;
			}
		}

		ptr = ptr->next;
	}
}

// return 1 if error
int ripdb_add(RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t metric, uint32_t partner, RcpInterface *interface) {
	ASSERT(metric != 0);
	ASSERT(metric < 16);
	initialize();

	RipDbElem *elem = find_exact(type, ip, mask, gw, partner);
	if (elem != NULL) {
//printf("found exact %d.%d.%d.%d/%d, gw %d.%d.%d.%d, metric %d\n",
//RCP_PRINT_IP(elem->ip), mask2bits(elem->mask), RCP_PRINT_IP(elem->gw), elem->metric);
		int update = 0;	
		// update existing route
		if (elem->metric != metric) {
			// postponed elem->metric = metric; 
			update = 1;
		}
		elem->timeout = RIP_ROUTE_TIMEOUT;
		if (elem->gc != 0) {
			elem->gc = 0;
			update = 1;
		}
		if (elem->interface != interface) {
			elem->interface = interface;
			update = 1;
		}
		if (elem->type == RCP_ROUTE_RIP && update) {
//printf("update %d.%d.%d.%d/%d, old metric %u, new metric %u\n",
//RCP_PRINT_IP(ip), mask2bits(mask), elem->metric, metric);
			ecmp_del(ip, mask);
			if (elem->metric != metric)
				elem->metric = metric; 
			clean_metric(ip, mask);
			ecmp_add(ip, mask);
		}
		else if (elem->metric != metric)
			elem->metric = metric; 
		
		if (update) {
			elem->updated = 1;
			ripdb_update_available = 1;
		}
		
		return 0;
	}

	// we have a different route coming in
	elem = find(ip, mask);
	int ecmp  = 0; // ecmp flag
	if (elem != NULL) {
//printf("found generic %d.%d.%d.%d/%d, gw %d.%d.%d.%d, metric %d\n",
//RCP_PRINT_IP(elem->ip), mask2bits(elem->mask), RCP_PRINT_IP(elem->gw), elem->metric);		
		int cmp = compare_type_metric(type, metric, elem);
//printf("cmp returned %d\n", cmp);
		if (cmp == -1)
			return 0;

		// better route
		else if (cmp == 1) {
			// delete the old route from the kernel
			ecmp_del(ip, mask);

			// replace the route
			elem->type = type;
			elem->metric = metric; 
			elem->gw = gw;
			elem->partner = partner;
			elem->timeout = RIP_ROUTE_TIMEOUT;
			elem->gc = 0;
			elem->interface = interface;
			elem->updated = 1;
			ripdb_update_available = 1;

			// add the route back
			clean_metric(ip, mask);
			ecmp_add(ip, mask);
			return 0;
		}
		
		// ecmp
		else if (cmp == 0)
			ecmp  = 1;
	}

	// create the new route
	elem = malloc(sizeof(RipDbElem));
	if (elem == NULL)
		return 1;
	memset(elem, 0, sizeof(RipDbElem));
	elem->type = type;
	elem->ip = ip;
	elem->mask = mask;
	elem->gw = gw;
	elem->metric = metric;
	elem->partner = partner;
	elem->timeout = RIP_ROUTE_TIMEOUT;
	elem->gc = 0;
	elem->interface = interface;
	elem->updated = 1;
	ripdb_update_available = 1;

	if (elem->type == RCP_ROUTE_RIP && ecmp) {
		// clear ecmp route
		ecmp_del(ip, mask);
	}
	
	uint8_t hval = hash(elem->ip);
	RipDbElem *ptr = ripdb_index[hval];
	if (ptr == NULL)
		ripdb_index[hval] = elem;
	else {
		elem->next = ptr;
		ripdb_index[hval] = elem;
	}

	if (elem->type == RCP_ROUTE_RIP) {
		if (ecmp)
			ecmp_add(ip, mask);
		else {
			// add single route
			rcpAddRoute(muxsock, RCP_ROUTE_RIP, elem->ip, elem->mask, elem->gw, elem->metric);
			ripdb_redistribute_route(1, elem->ip, elem->mask, elem->gw);
		}
	}
	return 0;
}

// return 1 if error
int ripdb_delete(RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t partner) {
	initialize();

	RipDbElem *ptr = find_exact(type, ip, mask, gw, partner);
	if (ptr == NULL)
		return 1;
	
	clean_route(ptr);
	return 0;
}

// return 1 if error
int ripdb_delete_rip(uint32_t ip, uint32_t mask) {
	initialize();

	RipDbElem *ptr = find(ip, mask);
	if (ptr == NULL)
		return 1;
	
	if (ptr->type != RCP_ROUTE_RIP)
		return 1;	// not a rip route
		
	clean_route(ptr);
	return 0;
}

void ripdb_delete_interface(RcpInterface *intf) {
	initialize();

	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL) {
//printf("route %d.%d.%d.%d/%d, gw %d.%d.%d.%d, interface %p\n",
//RCP_PRINT_IP(ptr->ip),
//mask2bits(ptr->mask),
//RCP_PRINT_IP(ptr->gw),
//ptr->interface);
			
			if (ptr->interface == intf) {
				clean_route(ptr);
			}
			ptr = ptr->next;
		}
	}
}

void ripdb_delete_type(RcpRouteType type) {
	initialize();
	
	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL) {
			if (ptr->type == type) {
				clean_route(ptr);
			}
			ptr = ptr->next;
		}
	}
}

void ripdb_delete_all(void) {
	initialize();
	
	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL) {
			clean_route(ptr);
			ptr = ptr->next;
		}
	}
}

void ripdb_clean_updates(void) {
	initialize();
	
	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL) {
			ptr->updated = 0;
			ptr = ptr->next;
		}
	}
	ripdb_update_available = 0;
}



static void list_timeout(RipDbElem **ptr) {
	ASSERT(ptr != NULL);
	
	if (*ptr == NULL)
		return;
	RipDbElem *elem = *ptr;
	
//printf("list timeout %p: %d.%d.%d.%d/%d, gw %d.%d.%d.%d, metric %d, %d/%d, next %p\n",
//elem, RCP_PRINT_IP(elem->ip), mask2bits(elem->mask), RCP_PRINT_IP(elem->gw), elem->metric, elem->timeout, elem->gc, elem->next);

	if (elem->timeout > 0) {
		if (elem->type == RCP_ROUTE_RIP) {
			elem->timeout--;
			if (elem->timeout == 0) {
				elem->gc = 0;
				clean_route(elem);
			}
		}
		list_timeout(&elem->next);
	}
	else { // timeout == 0
		if (elem->gc > 0) {
			elem->gc--;
			if (elem->gc == 0) {
				// remove the element
				*ptr = elem->next;
				free(elem);
				list_timeout(ptr);
			}
			else
				list_timeout(&elem->next);
		}
		else
			ASSERT(0);
	}
}

void ripdb_timeout(void) {
	initialize();
	
	int i;
	for (i = 0; i < 256; i++) {
		list_timeout(&ripdb_index[i]);
	}
}

void ripdb_print(FILE *fp) {
	initialize();

	if (cli_html)
		fprintf(fp, "<b>\n");
	fprintf(fp, "Codes: Rn - RIP network, R - RIP, C - connected, S - static, O - OSPF\n  * - cleanup timer\n\n");
	fprintf(fp, "   %-19s%-17s%-7s%-17s%-10.10sTime\n",
		"Network", "Next Hop", "Metric", "From", "Interface");
	if (cli_html)
		fprintf(fp, "</b>\n");

	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL) {
			char net[24];
			sprintf(net, "%d.%d.%d.%d/%d", RCP_PRINT_IP(ptr->ip), mask2bits(ptr->mask));
	
			char nexthop[24];
			if (ptr->gw == 0)
				*nexthop = '\0';
			else
				sprintf(nexthop, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->gw));
	
			char metric[24];
			sprintf(metric, "%d", ptr->metric);
	
			char from[24];
			if (ptr->partner == 0)
				*from = '\0';
			else
				sprintf(from, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->partner));

			char timeout[24];			
			if (ptr->type != RCP_ROUTE_STATIC &&
			     ptr->type != RCP_ROUTE_CONNECTED &&
			     ptr->type != RCP_ROUTE_CONNECTED_SUBNETS &&
			     ptr->type != RCP_ROUTE_RIP_NETWORK) {
				if (ptr->timeout > 0)
					sprintf(timeout, "%d", ptr->timeout);
				else
					sprintf(timeout, "%d*", ptr->gc);
			}
			else {
				if (ptr->timeout > 0)
					sprintf(timeout, " ");
				else
					sprintf(timeout, "%d*", ptr->gc);
			}

			fprintf(fp, "%-3s%-19s%-17s%-7s%-17s%-10.10s%s\n",
				rcpRouteType(ptr->type),
				net,
				nexthop,
				metric,
				from,
				ptr->interface->name,
				timeout);
			
			ptr = ptr->next;
		}
	}
}

int ripdb_build_response(RcpInterface *intf, RipRoute *rt, int updates_only) {
	ASSERT(intf != NULL);
	ASSERT(rt != NULL);
	
	int cnt = 0;
	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL && cnt < RIP_MAX_ROUTES) {
			if (updates_only && ptr->updated == 0) {
				ptr = ptr->next;
				continue;
			}
				
			if (ptr->type == RCP_ROUTE_RIP_NETWORK) {
				rt->family = htons(2);
				rt->tag = 0;
				rt->ip = htonl(ptr->ip);
				rt->mask = htonl(ptr->mask);
				rt->gw = 0;	// send the packets to me
				// poisoned reversed
				if ((ptr->ip & intf->mask) == (intf->ip & intf->mask))
					rt->metric = htonl(16);
				else
					rt->metric = htonl(ptr->metric);
			}
			else if (ptr->type == RCP_ROUTE_CONNECTED) {
				rt->family = htons(2);
				rt->tag = 0;
				rt->ip = htonl(ptr->ip);
				rt->mask = htonl(ptr->mask);
				rt->gw = 0;	// send the packets to me
				rt->metric = htonl(ptr->metric);
			}
			else if (ptr->type == RCP_ROUTE_CONNECTED_SUBNETS) {
				rt->family = htons(2);
				rt->tag = 0;
				rt->ip = htonl(ptr->ip);
				rt->mask = htonl(ptr->mask);
				rt->gw = 0;	// send the packets to me
				rt->metric = htonl(1);
			}
			else if (ptr->type == RCP_ROUTE_STATIC || ptr->type == RCP_ROUTE_OSPF) {
				rt->family = htons(2);
				rt->tag = 0;
				rt->ip = htonl(ptr->ip);
				rt->mask = htonl(ptr->mask);
				if ((ptr->gw & intf->mask) == (intf->ip & intf->mask))
					rt->gw = htonl(ptr->gw);	// send the packets directly
				else
					rt->gw = 0;	// send the packets to me
				rt->metric = htonl(ptr->metric);
			}
			else if (ptr->type == RCP_ROUTE_RIP) {
				rt->family = htons(2);
				rt->tag = 0;
				rt->ip = htonl(ptr->ip);
				rt->mask = htonl(ptr->mask);
				rt->gw = 0; // send the packets to me
				// poisoned reversed
				if ((ptr->partner & intf->mask) == (intf->ip & intf->mask))
					rt->metric = htonl(16);
				else
					rt->metric = htonl(ptr->metric);
			}

			cnt++;
			rt++;
			ptr = ptr->next;
		}
	}
	return cnt;
}

void ripdb_route_request(void) {
	int cnt = 0;
	int i;
	for (i = 0; i < 256; i++) {
		RipDbElem *ptr = ripdb_index[i];
		while (ptr != NULL && cnt < RIP_MAX_ROUTES) {
			if (ptr->type == RCP_ROUTE_RIP)
				ripdb_redistribute_route(1, ptr->ip, ptr->mask, ptr->gw);
			cnt++;
			ptr = ptr->next;
		}
	}
}
