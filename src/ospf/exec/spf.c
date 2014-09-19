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
#include "ospf.h"
#include "route.h"

uint32_t systic_spf_delta = 0;
uint32_t route_add_cnt = 0;
uint32_t route_del_cnt = 0;
uint32_t route_replaced_cnt = 0;

// return 1 if error, 0 if ok
static int dijkstra(uint32_t area_id) {
	OspfLsa *lsa;
	int rv = 0;
	int index;
	
	while ((lsa = lsapq_get()) != NULL) {
		if (lsa->h.from[0] != NULL)
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"Dijkstra(%p): visiting node %u/%d.%d.%d.%d, cost %u, reached from %u/%d.%d.%d.%d, cost %u",
				lsa,
				lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)), lsa->h.cost,
				lsa->h.from[0]->header.type,
				RCP_PRINT_IP(ntohl(lsa->h.from[0]->header.link_state_id)),
				lsa->h.cost);

		if (lsa->h.visited) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"Dijkstra(%p): visiting node %u/%d.%d.%d.%d, cost %u, node already visited",
				lsa, lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)), lsa->h.cost);
			
			continue;
		}
				
		// mark it as visited
		lsa->h.visited = 1;
		// find the smallest cost
		LsaCost *c = lsa->h.ncost;
		if (c == NULL) {
			rv = 1;
			continue;	
		}
		
		// push all lsa's with the smallest cost onto the queue
		int i;
		c = lsa->h.ncost;
		for (i = 0; i < lsa->h.ncost_cnt; i++, c++) {
			// find lsa
			OspfLsa *found = lsadbFind2(area_id, c->type, c->id);
			if (found == NULL) {
				rv = 1;
				continue;
			}
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"Dijkstra(%p):found %u/%d.%d.%d.%d, visited %s, cost %u, possible new cost %u",
				found, found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)),
				(found->h.visited)? "yes": "no",
				found->h.cost, lsa->h.cost + c->cost);

			if (found->h.visited == 0) {
				// set cost
				if (found->h.cost > lsa->h.cost + c->cost) {
					found->h.cost = lsa->h.cost + c->cost;
					found->h.from[0] = lsa;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
						"Dijkstra(%p): push node %u/%d.%d.%d.%d, cost %u, reached from %u/%d.%d.%d.%d, cost %u",
						found, found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)), found->h.cost,
						found->h.from[0]->header.type,
						RCP_PRINT_IP(ntohl(found->h.from[0]->header.link_state_id)),
						found->h.cost);
					// clear the remaining entries in h.from array
					for (index = 1; index < RCP_OSPF_ECMP_LIMIT; index++)
						found->h.from[index] = NULL;
				}
				// is this an ECMP route?
				else if (found->h.cost == lsa->h.cost + c->cost) {
					ASSERT(found->h.from[0] != NULL);
					// find an empty index in h.from array
					for (index = 1; index < RCP_OSPF_ECMP_LIMIT; index++) {
						if (found->h.from[index] == NULL)
							break;
					}
					if (index < RCP_OSPF_ECMP_LIMIT) {
						found->h.from[index] = lsa;
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
							"Dijkstra(%p): ECMP LSA %u/%d.%d.%d.%d, cost %u, reached from %u/%d.%d.%d.%d, cost %u",
							found, found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)), found->h.cost,
							found->h.from[index]->header.type,
							RCP_PRINT_IP(ntohl(found->h.from[index]->header.link_state_id)),
							found->h.cost);
					}
				}
				lsapq_push(found);
			}
			else {
				// already visited, just modify the cost
				if (found->h.cost > lsa->h.cost + c->cost) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
						"Dijkstra(%p): modify cost %u/%d.%d.%d.%d, cost %u, reached from %u/%d.%d.%d.%d, cost %u",
						found, found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)), found->h.cost,
						found->h.from[0]->header.type,
						RCP_PRINT_IP(ntohl(found->h.from[0]->header.link_state_id)),
						found->h.cost);
					found->h.cost = lsa->h.cost + c->cost;
					found->h.from[0] = lsa;
					// clear the remaining entries in h.from array
					for (index = 1; index < RCP_OSPF_ECMP_LIMIT; index++)
						found->h.from[index] = NULL;
				}
				// is this ECMP?
				else if (found->h.cost == lsa->h.cost + c->cost) {
					ASSERT(found->h.from != NULL);
					// find an empty index in h.from array
					for (index = 1; index < RCP_OSPF_ECMP_LIMIT; index++) {
						if (found->h.from[index] == NULL)
							break;
					}
					if (index < RCP_OSPF_ECMP_LIMIT) {
						found->h.from[index] = lsa;
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
							"Dijkstra(%p): ECMP LSA modify cost %u/%d.%d.%d.%d, cost %u, reached from %u/%d.%d.%d.%d, cost %u",
							found, found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)), found->h.cost,
							found->h.from[index]->header.type,
							RCP_PRINT_IP(ntohl(found->h.from[index]->header.link_state_id)),
							found->h.cost);
					}
				}
			}
		}
	}
	
	return rv;
}

static void dijkstra_result_print_lsa(uint32_t area_id, OspfLsa *lsa) {
	if (!lsa->h.visited) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"Dijkstra(%p): LSA type %u, id %d.%d.%d.%d not visited",
			lsa, lsa->header.type,
			RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
		return;
	}
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {	
		if (lsa->h.from[i] == NULL)
			break;
			
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"Dijkstra(%p): LSA type %u, id %d.%d.%d.%d, cost %u, from %u/%d.%d.%d.%d",
			lsa, lsa->header.type,
			RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
			lsa->h.cost,
			lsa->h.from[i]->header.type,
			RCP_PRINT_IP(ntohl(lsa->h.from[i]->header.link_state_id)));
	}
}

static void dijkstra_result(uint32_t area_id) {
	int i;
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
		"Dijkstra results for area %d", area_id);
		
	for (i = 0; i <= LSA_TYPE_NETWORK; i++) {
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa != NULL) {
			dijkstra_result_print_lsa(area_id, lsa);
			lsa = lsa->h.next;
		}
	}
}

void init_costs(uint32_t area_id) {
	int i;
	for (i = 0; i < LSA_TYPE_MAX; i++) {
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa != NULL) {
			lsa->h.cost = OSPF_MAX_COST;
			lsa->h.visited = 0;
			int i;
			for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++)
				lsa->h.from[i] = NULL;
			lsa->h.util_next = NULL;
			lsa->h.nexthop = 0;
			lsa->h.outhop = 0;
			
			lsa = lsa->h.next;
		}
	}
}

static void spf(uint32_t area_id) {
	init_costs(area_id);
	
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}
	
	if (area->mylsa == NULL)
		return;
		
	// push my router lsa on the queue
	area->mylsa->h.cost = 0;	// head of spf tree
	lsapq_push(area->mylsa);
	
	// run Dijkstra's algorithm
	dijkstra(area_id);
	
	// print results
	dijkstra_result(area_id);
}


static void update_stub_route(uint32_t area_id, OspfLsa *lsa_router, OspfLsa *lsa_network, uint32_t ip, uint32_t mask, uint32_t cost) {
	ASSERT(lsa_router);
	ASSERT(lsa_network);
	if (lsa_router == NULL || lsa_network == NULL)
		return;
	
	// extract network prefix
	uint32_t net = ntohl(lsa_network->header.link_state_id);
	uint32_t net_mask = ntohl(lsa_network->u.net.mask);
//	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
//		"network prefix %d.%d.%d.%d",
//		RCP_PRINT_IP(net & net_mask));
	
	// look for this prefix in router interfaces
	RtrLsaData *rdata = &lsa_router->u.rtr;
	int cnt = ntohs(rdata->links);
	RtrLsaLink *lnk = (RtrLsaLink *) ((uint8_t *) rdata + sizeof(RtrLsaData));
	int found = 0;
	uint32_t nexthop;
	while (cnt > 0) {
//		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
//			"link %d.%d.%d.%d",
//			RCP_PRINT_IP(ntohl(lnk->link_data)));

		if ( (net & net_mask) == (ntohl(lnk->link_data) & net_mask)) {
//			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
//				"next hop %d.%d.%d.%d found",
//				RCP_PRINT_IP(ntohl(lnk->link_data)));
			found = 1;
			nexthop = ntohl(lnk->link_data);
			break;
		}

		lnk++;
		cnt--;
	}
	
	if (found) {
		ip &= mask;
		
		char *type_str = "intra-area";
		uint8_t type = RT_OSPF;
		
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"adding %s route %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u\n",
			type_str,
			RCP_PRINT_IP(ip),
			mask2bits(mask),
			RCP_PRINT_IP(nexthop),
			cost);
		
			route_add(ip, mask, 0, nexthop, cost, type, area_id, 0);//current_lsa->h.outhop);
			
	}
	
}

static void spf_stubs(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}
	
	if (area->mylsa == NULL)
		return;

	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_ROUTER);
	if (lsa == NULL)
		return;

	while (lsa) {
		if (lsa == area->mylsa) {
			lsa = lsa->h.next;
			continue;
		}

		// links
		RtrLsaData *rdata = &lsa->u.rtr;
		int cnt = ntohs(rdata->links);
		RtrLsaLink *lnk = (RtrLsaLink *) ((uint8_t *) rdata + sizeof(RtrLsaData));
		while (cnt > 0 && lsa->h.cost < OSPF_MAX_COST) {
			if (lnk->type == 3) { // stub network
				uint32_t mask = ntohl(lnk->link_data);
				uint32_t ip = ntohl(lnk->link_id);
				ip &= mask;
				uint32_t cost = lsa->h.cost + ntohs(lnk->metric);
				
				// calculate next hop
				OspfLsa *nh = lsa;
				int found = 0;
				while (nh) {
					if (nh->h.from[0]->h.outhop) {
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
							"(stub link) %d.%d.%d.%d/%d resolved on %s link id %d.%d.%d.%d, from %s %d.%d.%d.%d",		
							RCP_PRINT_IP(ip),
							mask2bits(mask),
							(nh->header.type == LSA_TYPE_NETWORK)? "network": "router",
							RCP_PRINT_IP(ntohl(nh->header.link_state_id)),
							(nh->h.from[0]->header.type == LSA_TYPE_NETWORK)? "network": "router",
							RCP_PRINT_IP(ntohl(nh->h.from[0]->header.link_state_id)));
						found = 1;
						break;
					}
					else
						nh = nh->h.from[0];
				}
				
				// add route
				if (found)
					update_stub_route(area_id, nh, nh->h.from[0], ip, mask, cost);

			}
			lnk++;
			cnt--;
		}
		
		lsa = lsa->h.next;
	}
}

static void spf_external(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}

	if (area->mylsa == NULL) {
		return;
	}
	
	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_EXTERNAL);
	if (lsa == NULL) {
		return;
	}
		
	while (lsa) {
		ASSERT(lsa->h.ncost);
		ASSERT(lsa->h.ncost_cnt == 1);
		
		// if a forwarding address is specified, find the longest prefix match in the routing table
		uint32_t fw_cost = 0;
		if (lsa->u.ext.fw_address != 0) {
			RT *rt = route_find_lpm(ntohl(lsa->u.ext.fw_address), 0, 32);
			if (rt) {
				fw_cost = rt->cost;
			}
		}
		
//TODO: over here we can find multiple lsas - pick the one with the smallest cost
		// if still not solved, use the path to the advertising router in the calculation
		OspfLsa *found = lsadbFind2(area_id, LSA_TYPE_ROUTER, lsa->header.adv_router);
		if (found == NULL)
			found = lsadbFind2(area_id, LSA_TYPE_SUM_ROUTER, lsa->header.adv_router);
			
		if (found != NULL && found->h.cost < OSPF_MAX_COST) {
			lsa->h.visited = 1;
			lsa->h.from[0] = found;
			LsaCost *cost = lsa->h.ncost;
			if (fw_cost)
				lsa->h.cost = cost->cost + fw_cost;
			else
				lsa->h.cost = cost->cost + found->h.cost;

			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"(%p) external: node %u/%d.%d.%d.%d, cost %u, from %u/%d.%d.%d.%d",
				lsa, 
				lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)), lsa->h.cost,
				found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)));
		}


		lsa = lsa->h.next;	
	}

}

static void spf_sum_net(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}

	if (area->mylsa == NULL)
		return;
	
	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_SUM_NET);
	if (lsa == NULL)
		return;
		
	while (lsa) {
		if(lsa->h.ncost) {
			OspfLsa *found = lsadbFind2(area_id, LSA_TYPE_ROUTER, lsa->header.adv_router);
			if (found != NULL && found->h.cost < OSPF_MAX_COST) {
				lsa->h.visited = 1;
				lsa->h.from[0] = found;
				LsaCost *cost = lsa->h.ncost;
				lsa->h.cost = cost->cost + found->h.cost;
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
					"(%p) summary network: node %u/%d.%d.%d.%d, cost %u,  from %u/%d.%d.%d.%d",
					lsa,
					lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)), lsa->h.cost, 
					found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)));
			}
		}


		lsa = lsa->h.next;	
	}

}
static void spf_sum_router(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}

	if (area->mylsa == NULL)
		return;
	
	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_SUM_ROUTER);
	if (lsa == NULL)
		return;
		
	while (lsa) {
		if(lsa->h.ncost) {
			OspfLsa *found = lsadbFind2(area_id, LSA_TYPE_ROUTER, lsa->header.adv_router);
			if (found != NULL && found->h.cost < OSPF_MAX_COST) {
				lsa->h.visited = 1;
				lsa->h.from[0] = found;
				LsaCost *cost = lsa->h.ncost;
				lsa->h.cost = cost->cost + found->h.cost;
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
					"(%p) summary router: node %u/%d.%d.%d.%d, cost %u, from %u/%d.%d.%d.%d",
					lsa,
					lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)), lsa->h.cost,
					found->header.type, RCP_PRINT_IP(ntohl(found->header.link_state_id)));
			}
		}

		lsa = lsa->h.next;	
	}

}


void spf_range_backup(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}
	

	if (area->mylsa == NULL)
		return;

	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_SUM_NET);
	while (lsa != NULL) {
		// external summaddress routes are placed in the route table as blackhole routes
		if (lsa->h.originator == OSPF_ORIG_SUMMARY) {
			if (shm->config.ospf_discard_internal) {
				uint32_t mask = ntohl(lsa->u.net.mask);
				uint32_t ip = ntohl(lsa->header.link_state_id);
				ip &= mask;
	
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF, "add summary blackhole");
				route_add(ip, mask, 0, 0, OSPF_MAX_COST, RT_OSPF_BLACKHOLE, area_id, 0);
			}
		}

		lsa = lsa->h.next;
	}
}

//******************************************************************************
// Routing table calculation
//******************************************************************************

static void calculate_spf(void) {
	shm->stats.ospf_spf_calculations++;
	shm->stats.ospf_last_spf = time(NULL);
	route_add_cnt = 0;
	route_del_cnt = 0;
	route_replaced_cnt = 0;
	 
	

//   (1)	The present routing table is invalidated.  The routing table is
//	built again from scratch.  The old routing table is saved so
//	that changes in	routing	table entries can be identified.
	update_rt_start();
	OspfArea *area = areaGetList();
	if (area == NULL) {
		// this will remove all routes in ospf routing table
		update_rt_finish();
		return;
	}
	while (area != NULL) {
		update_rt_connected(area->area_id);
		area = area->next;
	}
	
//    (2)	The intra-area routes are calculated by	building the shortest-
//	path tree for each attached area.  In particular, all routing
//	table entries whose Destination	Type is	"area border router" are
//	calculated in this step.  This step is described in two	parts.
//	At first the tree is constructed by only considering those links
//	between	routers	and transit networks.  Then the	stub (done) networks
//	are incorporated into the tree.	During the area's shortest-path
//	tree calculation, the area's TransitCapability is also
//	calculated for later use in Step 4.

	area = areaGetList();
	while (area != NULL) {
		// spf calculation for all network and router lsa in each area
		spf(area->area_id);
		// calculate next hop
		nexthop(area->area_id);
		
		// look trough router lsa list for advertised stub networks
		// - calculate nexthop
		// - add routes
		spf_stubs(area->area_id);
		
		area =area->next;
	}

//    (3)	The inter-area routes are calculated, through examination of
//	summary-LSAs.  If the router is	attached to multiple areas
//	(i.e., it is an	area border router), only backbone summary-LSAs
//	are examined.
	if (is_abr)
		area = areaFind(0);
	else
		area = areaGetList();

	while (area != NULL) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"calculating summary router routes for area %u", area->area_id);
		// routers
		spf_sum_router(area->area_id);
		nexthop_summary_router(area->area_id);

		// networks
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"calculating summary network routes for area %u", area->area_id);
		spf_sum_net(area->area_id);
		nexthop_summary_net(area->area_id);
//		update_rt_summary(area->area_id);
		
		if (is_abr)
			break;
		else
			area =area->next;
	}
	
	// install blackhole routes for area ranges
	area = areaGetList();
	while (area != NULL) {
		spf_range_backup(area->area_id);
		area = area->next;
	}
	
//    (4)	In area	border routers connecting to one or more transit areas
//	(i.e, non-backbone areas whose TransitCapability is found to be
//	TRUE), the transit areas' summary-LSAs are examined to see
//	whether	better paths exist using the transit areas than	were
//	found in Steps 2-3 above.
//TODO

//    (5)	Routes to external destinations	are calculated,	through
//	examination of AS-external-LSAs.  The locations	of the AS
//	boundary routers (which	originate the AS-external-LSAs)	have
//	been determined	in steps 2-4.
	// calculate next hop

	area = areaGetList();
	while (area != NULL) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"calculating external routes for area %u", area->area_id);
		// spf calculation for external lsa in each area
		spf_external(area->area_id);

		// calculate next hop
		nexthop_external(area->area_id);

		// print results
//		dijkstra_result(area->area_id);
		
		area =area->next;
	}
	
	update_rt_finish();
	
	if (route_add_cnt || route_del_cnt || route_replaced_cnt)
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_SPF,
			"OSPF routes: %u added, %u deleted, %u replaced", route_add_cnt, route_del_cnt, route_replaced_cnt);

// debug print
//update_rt_print();

	// update summary lsa - if any new lsa is generated, spf is triggered again in lsadbAdd function
	lsa_originate_summary_update();

}

static int timeout = 0;	// spf wait time
static int timeout_hold = 0;	// spf hold time

void spfTrigger(void) {
	// init timeout
	if (timeout == 0) {
		timeout = shm->config.ospf_spf_delay;
	}		
	
	// else just let it expire as is
}

void spfTriggerNow(void) {
	// init timeout to expire in the next cycle
	timeout = 1;
	timeout_hold = 0;
}

void spfTimeout(void) {
	if (timeout_hold > 0) {
		if (--timeout_hold > 0) {
			return;
		}
	}

	if (timeout > 0) {
		if (--timeout == 0) {
			struct tms tm;
			uint32_t tic1 = times(&tm);

 			// calculate spf
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT | RLOG_FC_ROUTER, "running SPF alorithm");
			calculate_spf();
			
			uint32_t tic2 = times(&tm);
			uint32_t delta;
			if (tic2 > tic1)
				delta = tic2 - tic1;
			else
				delta = tic1 - tic2;
			
			if (delta > systic_spf_delta) {
				systic_spf_delta = delta;
				rcpDebug("spf delta %u\n", systic_spf_delta);
			}
			
			// start hold timer
			timeout_hold = shm->config.ospf_spf_hold;
			// update http stats
			active_neighbor_update();
			
		}
	}
}
