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

//************************************************************************
// Router LSA
//************************************************************************
// return 0 if identical
static int cmp_rtr_lsa(OspfLsa *lsa1, OspfLsa *lsa2) {
	if (lsa1 == NULL || lsa2 == NULL) {
		ASSERT(0);
		return 1;
	}
	
	OspfLsaHeader *lsah1 = &lsa1->header;
	OspfLsaHeader *lsah2 = &lsa2->header;

	if (lsah1->options != lsah1->options ||
	    lsah1->type !=  lsah2->type ||
	    lsah1->link_state_id !=  lsah2->link_state_id ||
	    lsah1->adv_router !=  lsah2->adv_router ||
	    lsah1->length != lsah2->length)
	  	return 1;

	RtrLsaData *rdata1 = &lsa1->u.rtr;
	RtrLsaData *rdata2 = &lsa2->u.rtr;
	return memcmp(rdata1, rdata2, lsah1->length - sizeof(OspfLsaHeader));
}


// originate a router lsa
OspfLsa *lsa_originate_rtr(uint32_t area_id) {
	TRACE_FUNCTION();
	
	// try to find if we already have this lsa
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return NULL;
	}

	OspfLsa *lsa = lsadbFind(area_id, 1, htonl(area->router_id), htonl(area->router_id));
	int replace_it = 0;
	if (lsa && lsa->h.self_originated) {
		// looks like we already have an lsa, set replace flag
		replace_it = 1;
	}
	
	// calculate total length
	int size = sizeof(OspfLsa) + sizeof(RtrLsaData);

	// count networks in this area
	int cnt = 0;
	OspfNetwork *net = area->network;
	if (net == NULL) {
		if (replace_it) {
			// remove existing lsa
			lsaListRemove(lsadbGetListHead(area_id, lsa->header.type), lsa);
			lsaFree(lsa);
			area->mylsa = NULL;
		}
		return NULL;
	}

	// adding only networks with a designated router
	while (net != NULL) {
		if (net->designated_router) {
			cnt++;
		}
		net = net->next;
	}
	size += cnt * sizeof(RtrLsaLink);
	
	// allocate lsa
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);
	rv->h.size = size;
	rv->h.self_originated = 1;
	
	// fill in the header
	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 2; //E bit
	lsah->type = 1;
	lsah->link_state_id = htonl(area->router_id);
	lsah->adv_router = htonl(area->router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(RtrLsaData) + cnt * sizeof(RtrLsaLink));
	
	// set links
	RtrLsaData *rdata = &rv->u.rtr;
	rdata->flags = ((is_abr)? 1: 0) | ((is_asbr)? 2: 0);	//ABR
	rdata->links = htons(cnt);
	
	// bring in the networks
	RtrLsaLink *lnk = (RtrLsaLink *) ((uint8_t *) rdata + sizeof(RtrLsaData));
	net = area->network;
	while (net != NULL) {
		if (net->designated_router) {
			if (neighborCount(net) == 0) {
				lnk->type = 3; // Connection to a stub network
				lnk->link_id = htonl(net->ip & net->mask); // network
				lnk->link_data = htonl(net->mask);	// mask
			}
			else {
				lnk->type = 2; // Connection to a transit network
				lnk->link_id = htonl(net->designated_router); // designated router for the network
				lnk->link_data = htonl(net->ip);	// interface ip address for the network
			}
			lnk->tos = 0;
			lnk->metric = htons(net->cost);
			lnk++;
		}

		net = net->next;
	}

	// calculate checksum
	lsah = &rv->header;
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);

	if (replace_it) {
		// compare the old lsa with the new one
//printf("area %u, link_state_id %d.%d.%d.%d\n", 
//area_id, RCP_PRINT_IP(area->router_id));
		
		if (cmp_rtr_lsa(rv, lsa) == 0) {
			// same lsa different age and seq
			// free new lsa
			free(rv);
			return NULL;
		}
		
		// remove old lsa
		lsaListRemove(lsadbGetListHead(area_id, lsa->header.type), lsa);
		lsaFree(lsa);
	}
	else
		trap_OriginateLsa(rv, area_id);
		
	rv->h.flood = 1;

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"area %u, originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		area_id,
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));
	
	area->mylsa = rv;
	return rv;
}

void lsa_originate_rtr_update(void) {
	OspfArea *area = areaGetList();
	// update all rtr lsa
	while (area) {
		OspfLsa *newlsa =  lsa_originate_rtr(area->area_id);
		if (newlsa) {
			lsadbAdd(area->area_id, newlsa);
		}
		
		area = area->next;
	}
}

