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
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "router.h"

//**************************************************************
// Shared memory handling
//**************************************************************
static RcpStaticRoute *shm_find_route_static(uint32_t ip, uint32_t mask, uint32_t gw) {
	int i;
	RcpStaticRoute *ptr;
	
	if (gw != 0) {
		for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
			if (!ptr->valid || ptr->type != RCP_ROUTE_STATIC)
				continue;
	
			if (ptr->ip == ip && ptr->mask == mask && ptr->gw == gw)
				return ptr;
		}
	}
	else {
		// if gw is 0 we don't look at it
		for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
			if (!ptr->valid || ptr->type != RCP_ROUTE_STATIC)
				continue;
	
			if (ptr->ip == ip && ptr->mask == mask)
				return ptr;
		}
	}
	
	return NULL;
}

static RcpStaticRoute *shm_find_route_blackhole(uint32_t ip, uint32_t mask) {
	int i;
	RcpStaticRoute *ptr;
	
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_ROUTE_BLACKHOLE)
			continue;

		if (ptr->ip == ip && ptr->mask == mask)
			return ptr;
	}
	
	return NULL;
}

static RcpStaticRoute *shm_find_route_empty(void) {
	int i;
	RcpStaticRoute *ptr;
	
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;

		return ptr;
	}
	
	return NULL;
}


static void redistribute_route(int cmd, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t metric) {
	uint32_t destination = 0;
	
	// send rcp route message to rip
	if (rcpRipRedistStatic())
		destination |= RCP_PROC_RIP;
	// send rcp route message to ospf
	if (rcpOspfRedistStatic())
		destination |= RCP_PROC_OSPF;

	// send rcp route message to clients
	if (muxsock && destination) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = destination;
		
		if (cmd == SIOCDELRT)
			pkt.type = RCP_PKT_TYPE_DELROUTE;
		else
			pkt.type = RCP_PKT_TYPE_ADDROUTE;

		pkt.pkt.route.type = RCP_ROUTE_STATIC;
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

static void add_blackhole(uint32_t ip, uint32_t mask) {
	char cmd[100];
	sprintf(cmd, "ip route add blackhole %d.%d.%d.%d/%d",
		RCP_PRINT_IP(ip), mask2bits(mask));
	int v = system(cmd);
	if (v == -1)
		ASSERT(0);
}

static void del_blackhole(uint32_t ip, uint32_t mask) {
	char cmd[100];
	sprintf(cmd, "ip route del blackhole %d.%d.%d.%d/%d",
		RCP_PRINT_IP(ip), mask2bits(mask));
	int v = system(cmd);
	if (v == -1)
		ASSERT(0);
}

//**************************************************************
// CLI commands
//**************************************************************

int cliIpRouteCidrCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// extract data
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[2], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// this should be a network address
	if (ip & (~mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	uint32_t gw;
	if (atoip(argv[3], &gw)) {
		strcpy(data, "Error: invalid next hop address\n");
		return RCPERR;
	}
	
	uint32_t metric = 1;
	if (argc == 5)
		metric = atoi(argv[4]);

	// test RIP distance
	if (metric == RCP_ROUTE_DISTANCE_RIP) {
		sprintf(data, "Error: Administrative distance %d is reserved for RIP\n", RCP_ROUTE_DISTANCE_RIP);
		return RCPERR;
	}

	// test OSPF distance
	if (metric == RCP_ROUTE_DISTANCE_OSPF) {
		sprintf(data, "Error: Administrative distance %d is reserved for OSPF\n", RCP_ROUTE_DISTANCE_OSPF);
		return RCPERR;
	}

	// add the route
	RcpStaticRoute *rt;
	if ((rt  = shm_find_route_static(ip, mask, gw)) != NULL) {
		// a route is already present in the kernel with a different metric
		// delete the old route first
		rcpDelRoute(muxsock, ip, mask, gw);
		rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_INFO, RLOG_FC_ROUTER,
			"static route %d.%d.%d.%d/%d gateway %d.%d.%d.%d deleted",
			RCP_PRINT_IP(ip), mask2bits(mask), RCP_PRINT_IP(gw));
		redistribute_route(SIOCDELRT, ip, mask, gw, rt->metric);
		rt->metric = metric;
		strcpy(data, "Warning: route replaced\n");
	}
	else if ((rt  = shm_find_route_static(ip, mask, 0)) != NULL && rt->metric == metric) {
		// we are in an equal cost multipath (ECMP) case - not supported yet!
		// delete the old route first
		rcpDelRoute(muxsock, rt->ip, rt->mask, rt->gw);
		rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_INFO, RLOG_FC_ROUTER,
			"static route %d.%d.%d.%d/%d gateway %d.%d.%d.%d deleted",
			RCP_PRINT_IP(ip), mask2bits(mask), RCP_PRINT_IP(gw));
		redistribute_route(SIOCDELRT, ip, mask, gw, rt->metric);
		rt->gw = gw;
		strcpy(data, "Warning: existing route with a different destination deleted\n");
	}
	else {
		// create a new route
		rt = shm_find_route_empty();
		if (rt == NULL) {
			sprintf(data, "Error: cannot add route, limit reached\n");
			return RCPERR;
		}
		memset(rt, 0, sizeof(RcpStaticRoute));
		rt->ip = ip;
		rt->mask = mask;
		rt->gw = gw;
		rt->metric = metric;
		rt->valid = 1;
		rt->type = RCP_ROUTE_STATIC;
	}


	rcpAddRoute(muxsock, RCP_ROUTE_STATIC, ip, mask, gw, metric);
	redistribute_route(SIOCADDRT, ip, mask, gw, metric);

	return 0;
}

int cliNoIpRouteCidrCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// extract data
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[3], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	// this should be a network address
	if (ip & (~mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	uint32_t gw;
	if (atoip(argv[4], &gw)) {
		strcpy(data, "Error: invalid next hop address\n");
		return RCPERR;
	}
	
	// delete route
	rcpDelRoute(muxsock, ip, mask, gw);
	redistribute_route(SIOCDELRT, ip, mask, gw, 0); // cli warning intentionally omitted

	RcpStaticRoute *rt = shm_find_route_static(ip, mask, gw);
	if (rt != NULL)
		rt->valid = 0;

	return 0;
}

int cliNoIpRouteAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	int i;
	RcpStaticRoute *ptr;
	
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->type == RCP_ROUTE_STATIC) {
			uint32_t ip = ptr->ip;
			uint32_t mask = ptr->mask;
			uint32_t gw = ptr->gw;
	
			// delete route from kernel
			rcpDelRoute(muxsock, ip, mask, gw);
			redistribute_route(SIOCDELRT, ip, mask, gw, 0);
			// delete route from shared memory
			ptr->valid = 0;
		}
		else if (ptr->type == RCP_ROUTE_BLACKHOLE) {
			uint32_t ip = ptr->ip;
			uint32_t mask = ptr->mask;
			del_blackhole(ip, mask);
			ptr->valid = 0;
		}
	}

	return 0;
}

int cliIpDefaultGwCmd(CliMode mode, int argc, char **argv) {
	char *newarg[5] = {"ip", "route", "0.0.0.0/0", argv[2], NULL};
	return cliIpRouteCidrCmd(mode, 4, newarg);
}
int cliNoIpDefaultGwCmd(CliMode mode, int argc, char **argv) {
	char *newarg[6] = {"no", "ip", "route", "0.0.0.0/0", argv[3], NULL};
	return cliNoIpRouteCidrCmd(mode, 5, newarg);
}


int cliIpRouteBlackhole(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// extract data
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[2], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// this should be a network address
	if (ip & (~mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	RcpStaticRoute *rt = shm_find_route_blackhole(ip, mask);
	if (rt != NULL)
		return 0;
	
	// create a new route
	rt = shm_find_route_empty();
	if (rt == NULL) {
		sprintf(data, "Error: cannot add route, limit reached\n");
		return RCPERR;
	}
	memset(rt, 0, sizeof(RcpStaticRoute));
	rt->ip = ip;
	rt->mask = mask;
	rt->valid = 1;
	rt->type = RCP_ROUTE_BLACKHOLE;
	add_blackhole(ip, mask);
	
	return 0;
}

int cliNoIpRouteBlackhole(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// extract data
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[3], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// this should be a network address
	if (ip & (~mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	RcpStaticRoute *rt = shm_find_route_blackhole(ip, mask);
	if (rt == NULL)
		return 0;
	
	// remove the route
	del_blackhole(ip, mask);
	rt->valid = 0;
	
	return 0;
}


//******************************************************************************************
// Route interface update
//******************************************************************************************
void router_update_if_route(RcpInterface *intf) {
	ASSERT(intf != NULL);

	int i;
	RcpStaticRoute *ptr;
	
	// walk trough static route configuration and set every single route located on this interface
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_ROUTE_STATIC)
			continue;

		if ((intf->ip & intf->mask) == (ptr->gw & intf->mask))
			rcpAddRoute(muxsock, RCP_ROUTE_STATIC, ptr->ip, ptr->mask, ptr->gw, ptr->metric);
	}
}
