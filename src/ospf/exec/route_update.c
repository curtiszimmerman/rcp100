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

//*******************************************
// callbacks
//*******************************************
// set del_flag in every route; this is done at the beginning of SPF cycle
static int callback_init(PTN *ptn, void *arg) {
	ASSERT(ptn != NULL);
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;

		ptn->rt[i].del_flag = 1;
	}
	
	return 0;

}

// remove all routes with del_flag set; this is done at the end of SPF cycle
static int callback_clear(PTN *ptn, void *arg) {
	ASSERT(ptn != NULL);

	int i;
	int deleted = 0;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		RT *rt = &ptn->rt[i];
//printf("callback delete %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u, type %s, del_flag %u\n",	
//RCP_PRINT_IP(rt->ip),
//mask2bits(rt->mask),
//RCP_PRINT_IP(rt->gw),
//rt->cost,
//rcpRouteType(rt->type),
//rt->del_flag);
		if (rt->type == RT_NONE)
			break;
		if (!rt->del_flag)
			continue;

//printf("delete route %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u\n",	
//RCP_PRINT_IP(rt->ip),
//mask2bits(rt->mask),
//RCP_PRINT_IP(rt->gw),
//rt->cost);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
			"deleting route %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u",
			RCP_PRINT_IP(rt->ip),
			mask2bits(rt->mask),
			RCP_PRINT_IP(rt->gw),
			rt->cost);
		ecmp_del(ptn);
		deleted = 1;
		
		memset(rt, 0, sizeof(RT));
		route_del_cnt++;
		
		// shift the rest of the array so we don't leave any holes
		if (i != (RCP_OSPF_ECMP_LIMIT - 1)) {
			memcpy(&ptn->rt[i], &ptn->rt[i + 1], (RCP_OSPF_ECMP_LIMIT - 1 - i) * sizeof(RT));
			memset(&ptn->rt[RCP_OSPF_ECMP_LIMIT - 1], 0, sizeof(RT));
		}
	}
	
	if (deleted) {
		ecmp_add(ptn);
	}
	
	// check retry flag, it is possible we have some more ecmp routes here
	if (ptn->retry_ecmp) {
		// count the number of ecmp routes installed
		int cnt = 0;
		for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
			if (ptn->rt[i].type == RT_NONE)
				break;
			cnt++;
		}
		
		// if there is room to add more,
		if (cnt < RCP_OSPF_ECMP_LIMIT) {
			// trigger a new spf calculation
			spfTrigger();
		}
		ptn->retry_ecmp = 0;
	}
		
	return 0;
}



static int debug_print(PTN *ptn, void *arg) {
	ASSERT(ptn != NULL);
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type != RT_NONE)
			printf("RT(%d): %d.%d.%d.%d %d.%d.%d.%d via %d.%d.%d.%d, type %s, metric %u\n",
				i,
				RCP_PRINT_IP(ptn->rt[i].ip),
				RCP_PRINT_IP(ptn->rt[i].mask),
				RCP_PRINT_IP(ptn->rt[i].gw),
				rcpRouteType(ptn->rt[i].type),
				ptn->rt[i].cost);
	}
	
	return 0;

}

//*******************************************
// update
//*******************************************
void update_rt_start(void) {
	// set delete flag in all routes
	route_walk(callback_init, NULL);
}

void update_rt_finish(void) {
	// remove all the routes that are still marked for deletion
	route_walk(callback_clear, NULL);
}

void update_rt_print(void) {
	// remove all the routes that are still marked for deletion
	route_walk(debug_print, NULL);
}

void update_rt_connected(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (area) {
		OspfNetwork *net = area->network;
		while (net != NULL) {
			uint32_t ip = net->ip & net->mask;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"adding connected route %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u",
				RCP_PRINT_IP(ip),
				mask2bits(net->mask),
				RCP_PRINT_IP(net->ip),
				net->cost);
			route_add(net->ip & net->mask, net->mask, 0, net->ip, net->cost, RT_CONNECTED, area_id, net->ip);
			net = net->next;
		}
	}
}


#if 0
void update_rt_summary(uint32_t area_id) {

	OspfArea *area = areaFind(area_id);
	if (area == NULL)
		return;
	
	// do everything else
	OspfLsa *lsa = lsadbGetList(area_id, LSA_TYPE_SUM_NET);
	while (lsa != NULL) {
		if (lsa->h.nexthop) {
			uint32_t mask = ntohl(lsa->u.net.mask);
			uint32_t ip = ntohl(lsa->header.link_state_id);
			ip &= mask;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_SPF,
				"adding inter-area route %d.%d.%d.%d/%d via %d.%d.%d.%d, cost %u, output interface %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip),
				mask2bits(mask),
				RCP_PRINT_IP(lsa->h.nexthop),
				lsa->h.cost,
				RCP_PRINT_IP(lsa->h.outhop));

			RT *rt = route_add(ip, mask, 0, lsa->h.nexthop, lsa->h.cost, RT_OSPF_IA);
			if (rt != NULL) {
				rt->area_id = area_id;
				rt->ifip = lsa->h.outhop;
				rt->del_flag = 0;
			}
		}

		lsa = lsa->h.next;
	}
}
#endif
