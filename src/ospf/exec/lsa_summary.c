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

//#define SUMDBG 1

// originate default route summary lsa for a stub area
OspfLsa *lsa_originate_sumdefault(uint32_t area_id, uint32_t cost) {
	ASSERT(is_abr);
	cost &= 0x00ffffff;
	
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return NULL;
	}
	ASSERT(area->stub);

	OspfLsa *found = lsadbFind(area_id, LSA_TYPE_SUM_NET, 0, htonl(area->router_id));
	int replace_it = 0;
	if (found && found->h.self_originated) {
		// looks like we already have an lsa, set replace flag
		replace_it = 1;
		// if the cost didn't change, do nothing
		if (ntohl(found->u.sum.metric) == cost)
			return NULL;
	}
	
	// allocate lsa
	int size = sizeof(OspfLsa) + sizeof(SumLsaData);
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);
	rv->h.size = size;
	rv->h.self_originated = 1;
	rv->h.del_flag = 0;

	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 0; //no E-bit
	lsah->type = LSA_TYPE_SUM_NET;
	lsah->link_state_id = 0;
	lsah->adv_router = htonl(area->router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	rv->u.sum.mask = 0;
	rv->u.sum.metric = htonl(cost);

	// calculate checksum
	lsah = &rv->header;
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(SumLsaData));
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);

	if (replace_it) {
		// remove existing lsa
		lsaListRemove(lsadbGetListHead(area_id, found->header.type), found);
		lsaFree(found);
	}
	else
		trap_OriginateLsa(rv, area_id);

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"area %u, originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		area_id,
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));
	
	rv->h.flood = 1;
	return rv;
}



// originate a summary lsa from a network lsa for a specific area
// if an lsa is already present and lsa data didn't change, a new one will not be created
static OspfLsa *lsa_originate_sumnet_area(uint32_t area_id, RT *rt) {
	ASSERT(rt);
#ifdef SUMDBG				
printf("generate in area %u\n", area_id);
#endif
	
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	// in stub areas check no_summary flag
	if (area->stub && area->no_summary)
		return NULL;
		
	// extract lsa data
	uint32_t link_state_id = htonl(rt->ip);
	uint32_t mask = htonl(rt->mask);
	uint32_t cost = ntohl(rt->cost);

	// find an existing lsa
	OspfLsa *found = lsadbFind(area_id, LSA_TYPE_SUM_NET, link_state_id, htonl(area->router_id));
	int replace_it = 0;
	if (found && found->h.self_originated) {
		// looks like we already have an lsa, set replace flag
		replace_it = 1;
		if (found->u.sum.mask == mask && found->u.sum.metric == cost) {
			found->h.del_flag = 0;
			return NULL;
		}
	}
	
//printf("area %u, link_state_id %d.%d.%d.%d, mask %d.%d.%d.%d, rt %p\n", 
//area_id, RCP_PRINT_IP(ntohl(link_state_id)),		
//RCP_PRINT_IP(ntohl(mask)),		
//rt);

	// allocate lsa
	int size = sizeof(OspfLsa) + sizeof(SumLsaData);
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);
	rv->h.size = size;
	rv->h.self_originated = 1;
	rv->h.del_flag = 0;

	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 2; //E bit
	lsah->type = LSA_TYPE_SUM_NET;
	lsah->link_state_id = link_state_id;
	lsah->adv_router = htonl(area->router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	rv->u.sum.mask = mask;
	rv->u.sum.metric = cost;
	
	// calculate checksum
	lsah = &rv->header;
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(SumLsaData));
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);

	if (replace_it) {
		// remove existing lsa
		lsaListRemove(lsadbGetListHead(area_id, found->header.type), found);
		lsaFree(found);
	}
	else
		trap_OriginateLsa(rv, area_id);

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"area %u, originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		area_id,
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));
	
	rv->h.flood = 1;
	return rv;
}


// originate a summary lsa for a intra-area route
static void lsa_originate_intra(RT *rt) {
	ASSERT(rt);
#ifdef SUMDBG				
printf("intra-area\n");
#endif

	OspfArea *area = areaGetList();
	// generate lsa in all areas but not in area_id
	while (area) {
		if (area->area_id != rt->area_id && networkCount(area->area_id)) {
			OspfLsa *newlsa = NULL;

			RcpOspfRange *range = range_match(rt->area_id, rt->ip, rt->mask);
			if (!range)
				 newlsa =  lsa_originate_sumnet_area(area->area_id, rt);
			else if (range->not_advertise)
				;
			else {
				// update the cost
				uint32_t cost = rt->cost;
				if (cost != OSPF_MAX_COST) {
					if (range->cost == OSPF_MAX_COST)
						range->cost = cost;
					else if (range->cost < cost) {
						range->cost = cost;
//printf("range %d.%d.%d.%d/%d cost set to %u\n",
//RCP_PRINT_IP(range->ip), mask2bits(range->mask), range->cost);
					}
				}
			}
				 
			if (newlsa) {
				lsadbAdd(area->area_id, newlsa);
			}
		}
		
		area = area->next;
	}
}

// originate a summary lsa for an inter-area route
static void lsa_originate_inter(PTN *ptn, RT *rt) {
	ASSERT(rt);
#ifdef SUMDBG				
printf("inter-area\n");
#endif

	OspfArea *area = areaGetList();
	// generate lsa in all areas but not in area_id
	while (area) {
		if (area->area_id != rt->area_id && networkCount(area->area_id)) {
			// looking trough all the routes to this destination, check if the network is located in this area
			int i;
			int found = 0;
			for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
				if (ptn->rt[i].type == RT_NONE)
					break;
				if (&ptn->rt[i] == rt)
					continue;			
				if (ptn->rt[i].area_id == area->area_id) {
					found = 1;
					break;
				}
			}
			
			if (!found) {
				OspfLsa *newlsa =  lsa_originate_sumnet_area(area->area_id, rt);
				if (newlsa) {
					lsadbAdd(area->area_id, newlsa);
				}
			}
		}
		area = area->next;
	}
}


static int callback_summary(PTN *ptn, void *arg) {
	ASSERT(ptn);
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;

		RT *rt = &ptn->rt[i];
#ifdef SUMDBG				
		printf("%p:%d.%d.%d.%d/%d cost %u, area %u\n",
			rt,
			RCP_PRINT_IP(rt->ip & rt->mask),
			mask2bits(rt->mask),
			rt->cost,
			rt->area_id);
#endif
		if (rt->type == RT_CONNECTED || rt->type == RT_OSPF) {
			lsa_originate_intra(rt);
		}
		else if (rt->type == RT_OSPF_IA) {
			lsa_originate_inter(ptn, rt);
		}
//		else if (rt->type == RT_OSPF_BLACKHOLE) {
//		}
//		else if (rt->type == RT_OSPF_E1) {
//		}
//		else if (rt->type == RT_OSPF_E2) {
//		}
							
	}
	return 0;
}


// originate a summary lsa from a summary range for a specific area
// if an lsa is already present and lsa data didn't change, a new one will not be created
static OspfLsa *lsa_originate_sumnet_area_range(uint32_t area_id, RcpOspfRange *range) {
	ASSERT(range);
#ifdef SUMDBG				
printf("generate range in area %u\n", area_id);
#endif
	
	OspfArea *area = areaFind(range->area);
	if (area == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	// in stub areas check no_summary flag
	if (area->stub && area->no_summary)
		return NULL;
		

	// we need to apply the mask to link_state_id
	uint32_t link_state_id = htonl(range->ip);
	
	OspfLsa *found = lsadbFind(area_id, LSA_TYPE_SUM_NET, link_state_id, htonl(area->router_id));
	int replace_it = 0;
	if (found && found->h.self_originated) {
		// looks like we already have an lsa, set replace flag
		replace_it = 1;
		if (found->u.sum.mask == htonl(range->mask) &&
		    found->u.sum.metric == htonl(range->cost)) {
			found->h.del_flag = 0;
			return NULL;
		}
	}
	
	// allocate lsa
	int size = sizeof(OspfLsa) + sizeof(SumLsaData);
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);
	rv->h.size = size;
	rv->h.self_originated = 1;
	rv->h.del_flag = 0;

	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 2; //E bit
	lsah->type = LSA_TYPE_SUM_NET;
	lsah->link_state_id = link_state_id;
	lsah->adv_router = htonl(area->router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	rv->u.sum.mask = htonl(range->mask);
	rv->u.sum.metric = htonl(range->cost);	//src/ospf/exec/lsa_originate.c

	// calculate checksum
	lsah = &rv->header;
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(SumLsaData));
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);

	if (replace_it) {
		// remove existing lsa
		lsaListRemove(lsadbGetListHead(area_id, found->header.type), found);
		lsaFree(found);
	}
	else
		trap_OriginateLsa(rv, area_id);

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"area %u, originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		area_id,
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));
	
	rv->h.flood = 1;
	rv->h.originator = OSPF_ORIG_SUMMARY;
	return rv;
}

// originate a summary lsa from a network lsa
static void lsa_originate_sumnet_range(RcpOspfRange *range) {
	ASSERT(range);
	
	OspfArea *area = areaGetList();
	// generate lsa in all areas but not in area_id
	while (area) {
		if (area->area_id != range->area && networkCount(area->area_id)) {
			OspfLsa *newlsa = lsa_originate_sumnet_area_range(area->area_id, range);
			if (newlsa)
				lsadbAdd(area->area_id, newlsa);
		}
		area = area->next;
	}
}





// originate a summary lsa from an external lsa for a specific area
// if an lsa is already present, a new one will not be created
static OspfLsa *lsa_originate_sumrouter_area(uint32_t area_id, OspfLsa *lsa) {
	ASSERT(lsa);
	ASSERT(lsa->header.type == LSA_TYPE_ROUTER);
#ifdef SUMDBG				
printf("generate in area %u\n", area_id);
#endif

	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	// sumrouter lsa not generated in stub area
	if (area->stub)
		return NULL;


	// extract cost from the routing table
	uint32_t cost = lsa->h.cost; 

	int replace_it = 0;
	OspfLsa *found = lsadbFind(area_id, LSA_TYPE_SUM_ROUTER, lsa->header.adv_router, htonl(area->router_id));
	if (found && found->h.self_originated) {
		// compare
		if (found->u.sum.metric == htonl(cost)) {
			// looks like we already have an lsa, set replace flag
			found->h.del_flag = 0;
//printf("already there\n");		
			return NULL;
		}
		replace_it = 1;
	}
	
	// allocate lsa
	int size = sizeof(OspfLsa) + sizeof(SumLsaData);
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);

	rv->h.size = size;
	rv->h.self_originated = 1;
	rv->h.del_flag = 0;
	
	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 2; //E bit
	lsah->type = LSA_TYPE_SUM_ROUTER;
	lsah->link_state_id = lsa->header.adv_router;
	lsah->adv_router = htonl(area->router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	rv->u.sum.mask = 0;
	rv->u.sum.metric = htonl(cost);

	// calculate checksum
	lsah = &rv->header;
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(SumLsaData));
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);
	
	if (replace_it) {
		// remove existing lsa
		lsaListRemove(lsadbGetListHead(area_id, found->header.type), found);
		lsaFree(found);
	}
	else
		trap_OriginateLsa(rv, area_id);

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"area %u, originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		area_id,
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));


	rv->h.flood = 1;
	return rv;
}

// originate a summary lsa from a summary router lsa for a specific area
// if an lsa is already present, a new one will not be created
static OspfLsa *lsa_originate_sumrouter_transfer_area(uint32_t area_id, OspfLsa *lsa) {
	ASSERT(lsa);
	ASSERT(lsa->header.type == LSA_TYPE_SUM_ROUTER);
#ifdef SUMDBG				
printf("generate in area %u\n", area_id);
#endif

	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	// sumrouter lsa not generated in stub area
	if (area->stub)
		return NULL;


	// extract cost from the routing table
	uint32_t cost = 10; // cost in network format
	// find the advertising router in area 0
	OspfLsa *advr = lsadbFind(0, LSA_TYPE_ROUTER, lsa->header.adv_router,  lsa->header.adv_router);
	if (advr) {
		cost = ntohl(lsa->u.sum.metric) + advr->h.cost;
	}		
//printf("area %u, summary lsa link_state_id %d.%d.%d.%d, adv router %d.%d.%d.%d\n",
//area_id, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
//RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
//printf("cost set to %u\n", cost);

	int replace_it = 0;
	OspfLsa *found = lsadbFind(area_id, LSA_TYPE_SUM_ROUTER, lsa->header.link_state_id, htonl(area->router_id));
	if (found && found->h.self_originated) {
		// compare
		if (found->u.sum.metric == htonl(cost)) {
			// looks like we already have an lsa, set replace flag
			found->h.del_flag = 0;
//printf("already there\n");			
			return NULL;
		}
		replace_it = 1;
	}
	
	// allocate lsa
	int size = sizeof(OspfLsa) + sizeof(SumLsaData);
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);

	rv->h.size = size;
	rv->h.self_originated = 1;
	rv->h.del_flag = 0;
	
	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 2; //E bit
	lsah->type = LSA_TYPE_SUM_ROUTER;
	lsah->link_state_id = lsa->header.link_state_id;
	lsah->adv_router = htonl(area->router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	rv->u.sum.mask = 0;
	rv->u.sum.metric = htonl(cost);

	// calculate checksum
	lsah = &rv->header;
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(SumLsaData));
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);
	
	if (replace_it) {
		// remove existing lsa
		lsaListRemove(lsadbGetListHead(area_id, found->header.type), found);
		lsaFree(found);
	}
	else
		trap_OriginateLsa(rv, area_id);

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"area %u, originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		area_id,
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));


	rv->h.flood = 1;
	return rv;
}

// originate a summary lsa from a router lsa
static void lsa_originate_sumrouter(uint32_t area_id, OspfLsa *lsa) {
	ASSERT(lsa);
	ASSERT(lsa->header.type == LSA_TYPE_ROUTER);

	// only if ABR
	if (!is_abr)
		return;

	OspfArea *area = areaGetList();
	// generate lsa in all areas but not in area_id
	while (area) {
		if (area->area_id != area_id && networkCount(area->area_id)) {
			// check if the router is located in this area
			OspfLsa *found = lsadbFind(area->area_id, LSA_TYPE_ROUTER, lsa->header.link_state_id, lsa->header.link_state_id);
			if (!found) {
				OspfLsa *newlsa =  lsa_originate_sumrouter_area(area->area_id, lsa);
				if (newlsa) {
					lsadbAdd(area->area_id, newlsa);
				}
			}
		}
		area = area->next;
	}
}

// originate a summary router lsa from a summary router lsa
static void lsa_originate_sumrouter_transfer(uint32_t area_id, OspfLsa *lsa) {
	ASSERT(lsa);
	ASSERT(lsa->header.type == LSA_TYPE_SUM_ROUTER);

	// only if ABR
	if (!is_abr)
		return;

	OspfArea *area = areaGetList();
	// generate lsa in all areas but not in area_id
	while (area) {
		if (area->area_id != area_id && networkCount(area->area_id)) {
			// check if the router is located in this area
			OspfLsa *found = lsadbFind(area->area_id, LSA_TYPE_ROUTER, lsa->header.link_state_id, lsa->header.link_state_id);
			if (!found) {
				OspfLsa *newlsa =  lsa_originate_sumrouter_transfer_area(area->area_id, lsa);
				if (newlsa) {
					lsadbAdd(area->area_id, newlsa);
				}
			}
		}
		area = area->next;
	}
}




void lsa_originate_summary_update(void) {
	// only if ABR
	if (!is_abr)
		return;

	OspfArea *area = areaGetList();
	// set the delete flag in all summary lsa generated by us
	// with the exception of default route in stub areas
	while (area) {
		int i;
		for (i = 3; i < 5; i++) {
			OspfLsa *lsa = lsadbGetList(area->area_id, i);
			while (lsa != NULL) {
				if (lsa->h.self_originated) {
					// in stub areas, do not delete the default lsa
					if (lsa->header.link_state_id == 0 && area->stub)
						lsa->h.del_flag = 0;
					else
						lsa->h.del_flag = 1;
				}
				lsa = lsa->h.next;
			}
		}
		
		area = area->next;
	}

	// reset range cost
	if (range_is_configured()) {
		// set cost to infinite
		RcpOspfRange *ptr;
		int i;
		
		for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
			if (!ptr->valid)
				continue;
			
			ptr->cost = OSPF_MAX_COST;
		}
	}

	
	// walk the routing table and generate summary lsas as necessary
	route_walk(callback_summary, NULL);


	// add ranges
	if (range_is_configured()) {
		// set cost to infinite
		RcpOspfRange *ptr;
		int i;
		
		for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
			if (!ptr->valid)
				continue;
			if (ptr->cost == OSPF_MAX_COST)
				continue;
			
			lsa_originate_sumnet_range(ptr);
		}
	}

	// go trough router lsas inside each area and generate asbr summary lsas
	area = areaGetList();
	while (area) {
		if (networkCount(area->area_id)) {
			// in each area, generate summary network lsa in all other areas
			OspfLsa *lsa = lsadbGetList(area->area_id, LSA_TYPE_ROUTER);
			while (lsa) {
				if (lsa->h.self_originated == 0) {
#ifdef SUMDBG				
					printf("1: area %u, router %d.%d.%d.%d from %d.%d.%d.%d, flags %u, cost %u\n",
						area->area_id,
						RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
						RCP_PRINT_IP(ntohl(lsa->header.adv_router)),
						lsa->u.rtr.flags,
						lsa->h.cost);
#endif
					
					// look only at AS boundary routers
					if (lsa->h.cost != OSPF_MAX_COST &&
					     lsa->u.rtr.flags & 0x02)
						lsa_originate_sumrouter(area->area_id, lsa);	
					
				}
				lsa = lsa->h.next;
			}
		}
		
		area = area->next;
	}


	// look trough asbr summary lsas generated by other routers and push them forward
	area = areaFind(0);
	if (area && networkCount(area->area_id)) {
		// in each area, generate summary network lsa in all other areas
		OspfLsa *lsa = lsadbGetList(area->area_id,  LSA_TYPE_SUM_ROUTER);
		while (lsa) {
			if (lsa->h.self_originated == 0) {
#ifdef SUMDBG				
				printf("2: area %u, router %d.%d.%d.%d from %d.%d.%d.%d\n",
					area->area_id,
					RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
					RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
#endif					
				lsa_originate_sumrouter_transfer(area->area_id, lsa);
			}
			lsa = lsa->h.next;
		}
	}
	
	// age lsa with a del_flag set
	area = areaGetList();
	while (area) {
		int i;
		for (i = 3; i < 5; i++) {
			OspfLsa *lsa = lsadbGetList(area->area_id, i);
			while (lsa != NULL) {
				OspfLsa *next = lsa->h.next;

				if (lsa->h.del_flag == 1) {
					lsa->header.age = htons(MaxAge);
					lsadbFlood(lsa, area->area_id);
					lsadbRemove(area->area_id, lsa);
					lsaFree(lsa);
				}
				lsa = next;
			}
		}
		
		area = area->next;
	}
}




