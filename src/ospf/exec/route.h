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
#ifndef OSPF_ROUTE_H
#define OSPF_ROUTE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "librcp_route.h"
#include "librcp_limits.h"

// route structure
typedef struct rt_t {
	uint32_t ip;
	uint32_t mask;
	uint32_t gw;
	uint32_t cost;
	uint32_t e2_cost;
	
	// ospf specific route data
	uint32_t area_id;		// the route was installed on this area
	uint32_t ifip;		// interface ip address
#define RT_NONE		0
#define RT_CONNECTED	RCP_ROUTE_CONNECTED
#define RT_STATIC		RCP_ROUTE_STATIC
#define RT_OSPF		RCP_ROUTE_OSPF		// intra-area
#define RT_OSPF_IA	RCP_ROUTE_OSPF_IA	// inter-area
#define RT_OSPF_E1	RCP_ROUTE_OSPF_E1
#define RT_OSPF_E2	RCP_ROUTE_OSPF_E2
#define RT_OSPF_BLACKHOLE RCP_ROUTE_OSPF_BLACKHOLE
#define RT_TYPE_MAX	RCP_ROUTE_MAX
	uint8_t type;
	uint8_t del_flag;	// delete flag - set at SPF start, the route gets deleted
			// at the end, if during SPF the flag was not reseted
	uint8_t to_add;
} RT;

// Patricia tree structure
typedef struct ptree_node_t {
	struct ptree_node_t *parent;
	struct ptree_node_t *link[2];

	uint32_t key;
	uint32_t mask;
	int len;
	
	RT rt[RCP_OSPF_ECMP_LIMIT];	// if not configured, RT.type is RT_NONE
				// unconfigured entries are at the end of the array
				// configured entries are at the beginning of the array
	uint8_t retry_ecmp;		// we have some more ecmp routes for this prefix,
				// a restart of spf calculation might be necessary
} PTN;

// add a route
RT* route_add(uint32_t ip, uint32_t mask, int len, uint32_t gw, uint32_t cost, uint8_t type, uint32_t area_id, uint32_t ifip);
RT* route_add_e2(uint32_t ip, uint32_t mask, int len, uint32_t gw, uint32_t cost, uint8_t type, uint32_t e2_cost, uint32_t area_id, uint32_t ifip);
// find route
RT* route_find(uint32_t ip, uint32_t mask, int len);
// longest prefix match search
RT* route_find_lpm(uint32_t ip, uint32_t mask, int len);
// delete route
void route_delete(uint32_t ip, uint32_t mask, int len);
// delete all routes
void route_delete_all(void);
// debug print
void route_print_debug(FILE *fp);
// walk route table
// return 1 if error, 0 if ok
int route_walk(int (*f)(PTN *, void *), void *arg);

// kernel add/del route interface
void ecmp_del(PTN *ptn);
void ecmp_add(PTN *ptn);

// redistribute route into RIP
void redistribute_route(int add, uint32_t ip, uint32_t mask, uint32_t gw);

#endif
