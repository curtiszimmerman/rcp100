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

//************************************************************************
// route management
//************************************************************************
// find the router interface connected to this network and set the route
static OspfLsa *current_lsa = NULL;
static void update_route(uint32_t area_id, OspfLsa *lsa_router, OspfLsa *lsa_network) {
	ASSERT(lsa_router);
	ASSERT(lsa_network);
	ASSERT(current_lsa);
	if (lsa_router == NULL || lsa_network == NULL || current_lsa == NULL)
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
	while (cnt > 0) {
//		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
//			"link %d.%d.%d.%d",
//			RCP_PRINT_IP(ntohl(lnk->link_data)));

		if ( (net & net_mask) == (ntohl(lnk->link_data) & net_mask)) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"next hop %d.%d.%d.%d found",
				RCP_PRINT_IP(ntohl(lnk->link_data)));
			found = 1;
			current_lsa->h.nexthop = ntohl(lnk->link_data);
			
			// for external routes, take into consideration the forwarding address if present in lsa
			if (current_lsa->header.type == LSA_TYPE_EXTERNAL &&
			     current_lsa->h.ncost &&
			     current_lsa->h.ncost->id != 0) {
				RT *rt = route_find_lpm(ntohl(current_lsa->h.ncost->id), 0xffffffff, 0);
				if (rt != NULL && rt->del_flag == 0) {
					if (rt->type == RT_CONNECTED) {
						current_lsa->h.nexthop = ntohl(current_lsa->h.ncost->id);
					}
				}
			}

			
			break;
		}

		lnk++;
		cnt--;
	}
	
	if (found) {
		uint32_t mask = ntohl(current_lsa->u.net.mask);
		uint32_t ip = ntohl(current_lsa->header.link_state_id);
		ip &= mask;
		
		uint8_t type;
		char *type_str = "unknown";
		if (current_lsa->header.type == LSA_TYPE_EXTERNAL) {
			type_str = "external";
			type = (current_lsa->h.ncost->ext_type == 1)? RT_OSPF_E1: RT_OSPF_E2;
		}
		else if (current_lsa->header.type == LSA_TYPE_SUM_NET) {
			type_str = "inter-area";
			type = RT_OSPF_IA;
		}
		else {
			type_str = "intra-area";
			type = RT_OSPF;
		}
		
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"adding %s route %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u, output interface %d.%d.%d.%d",
			type_str,
			RCP_PRINT_IP(ip),
			mask2bits(mask),
			RCP_PRINT_IP(current_lsa->h.nexthop),
			current_lsa->h.cost,
			RCP_PRINT_IP(current_lsa->h.outhop));
		
		if (type == RT_OSPF_E2) {
			LsaCost *cost = current_lsa->h.ncost;
			ASSERT(cost);
			route_add_e2(ip, mask, 0, current_lsa->h.nexthop, current_lsa->h.cost, type, cost->cost, area_id, current_lsa->h.outhop);
		}
		else 
			route_add(ip, mask, 0, current_lsa->h.nexthop, current_lsa->h.cost, type, area_id, current_lsa->h.outhop);
			
	}
	
}

static void get_nexthop_id(uint32_t area_id, OspfLsa *lsa) {
	ASSERT(lsa);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
		"(%p) get_nexthop_id %d/%d.%d.%d.%d",
		lsa, lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	
	// not resolved by Dijkstra's algorithm
	if (!lsa->h.visited)
		return;
	if (!lsa->h.from[0])
		return;
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (lsa->h.from[i] == NULL)
			break;
		
		// see if the next hop is resolved on our own interface
		if (lsa->h.from[i]->h.outhop) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"resolved on %s link id %d.%d.%d.%d, from %s %d.%d.%d.%d",		
				(lsa->header.type == LSA_TYPE_NETWORK)? "network": "router",
				RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
				(lsa->h.from[i]->header.type == LSA_TYPE_NETWORK)? "network": "router",
				RCP_PRINT_IP(ntohl(lsa->h.from[i]->header.link_state_id)));
			update_route(area_id, lsa, lsa->h.from[i]);
		}
		else 
			get_nexthop_id(area_id, lsa->h.from[i]);
	}
}

//************************************************************************
// nexthop calculation
//************************************************************************


// external routes next hop calculation 
void nexthop_external(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}

	if (area->mylsa == NULL)
		return;

	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {

		if (!lsa->h.visited) {
			lsa = lsa->h.next;
			continue;
		}

		// external summaddress routes are placed in the route table as blackhole routes
		if (lsa->h.originator == OSPF_ORIG_SUMMARY) {
			if (shm->config.ospf_discard_external) {
				uint32_t mask = ntohl(lsa->u.net.mask);
				uint32_t ip = ntohl(lsa->header.link_state_id);
				ip &= mask;
	
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF, "add summary blackhole");
				route_add(ip, mask, 0, 0, OSPF_MAX_COST, RT_OSPF_BLACKHOLE, area_id, 0);
			}
		}
		else {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF, "search next hop");
			current_lsa = lsa;
			get_nexthop_id(area->area_id, lsa);
		}

		lsa = lsa->h.next;
	}
}

// summary network routes next hop calculation 
void nexthop_summary_net(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}
	

	if (area->mylsa == NULL)
		return;

	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_SUM_NET);
	while (lsa != NULL) {
		if (!lsa->h.visited) {
			lsa = lsa->h.next;
			continue;
		}
		
		// external summaddress routes are placed in the route table as blackhole routes
		if (lsa->h.originator != OSPF_ORIG_SUMMARY) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF, "search next hop");
			current_lsa = lsa;
			get_nexthop_id(area->area_id, lsa);
		}

		lsa = lsa->h.next;
	}
}

// summary asbr routes next hop calculation 
void nexthop_summary_router(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}
	

	if (area->mylsa == NULL)
		return;
 
	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_SUM_ROUTER);
	while (lsa != NULL) {
		if (!lsa->h.visited) {
			lsa = lsa->h.next;
			continue;
		}
		OspfLsa *from = lsa->h.from[0];
		ASSERT(from);
		
		lsa->h.outhop = from->h.outhop;
		lsa->h.nexthop = from->h.nexthop;
		lsa = lsa->h.next;
	}
}



// next hop calculation for an area
void nexthop(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return;
	}
	
	if (area->mylsa == NULL)
		return;

	// skip mylsa
	
	// deal with my own interfaces
	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_NETWORK);
	while (lsa != NULL) {
		if (!lsa->h.visited || lsa->h.outhop) {
			lsa = lsa->h.next;
			continue;
		}
		
		if (lsa->h.from[0] == area->mylsa) {
			OspfNetwork *net = networkFindIp(area->area_id, ntohl(lsa->header.link_state_id));
			if (net == NULL) {
				lsa = lsa->h.next;
				continue;
			}
			lsa->h.outhop = net->ip;

			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"(%p): connected network %d.%d.%d.%d/%d via %d.%d.%d.%d, output interface %d.%d.%d.%d",
				lsa, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
				mask2bits(ntohl(lsa->u.net.mask)),
				RCP_PRINT_IP(lsa->h.nexthop),
				RCP_PRINT_IP(lsa->h.outhop));
		}

		lsa = lsa->h.next;
	}

	// do everything else
	lsa = lsadbGetList(area_id, LSA_TYPE_NETWORK);
	while (lsa != NULL) {
		if (!lsa->h.visited || lsa->h.nexthop || lsa->h.outhop) {
			lsa = lsa->h.next;
			continue;
		}

		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF, "search next hop");
		current_lsa = lsa;
		get_nexthop_id(area->area_id, lsa);
		lsa = lsa->h.next;
	}

#if 0
	lsa = lsadbGetList(area_id, LSA_TYPE_ROUTER);
	while (lsa != NULL) {
		if (!lsa->h.visited || lsa->h.nexthop || lsa->h.outhop) {
			lsa = lsa->h.next;
			continue;
		}

		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF, "search next hop");
		lsa->h.nexthop = 0;get_nexthop_id(area->area_id, lsa);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"(%p): remote network %d.%d.%d.%d/%d via %d.%d.%d.%d, output interface %d.%d.%d.%d",
			lsa, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
			mask2bits(ntohl(lsa->u.net.mask)),
			RCP_PRINT_IP(lsa->h.nexthop),
			RCP_PRINT_IP(lsa->h.outhop));

		lsa = lsa->h.next;
	}
#endif
}

