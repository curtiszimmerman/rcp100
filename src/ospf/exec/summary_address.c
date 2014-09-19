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
extern RcpPkt *pktout;
extern uint32_t redist_del_cnt;

static int is_configured = 0;

static int summaddr_any_configured(void) {
	RcpOspfSummaryAddr *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		return 1;
	}
	
	return 0;
}

static RcpOspfSummaryAddr *summaddr_find(uint32_t ip, uint32_t mask) {
	RcpOspfSummaryAddr *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		if (ptr->ip == ip && ptr->mask == mask)
			return ptr;
	}
	
	return NULL;
}

static RcpOspfSummaryAddr *summaddr_find_empty(void) {
	RcpOspfSummaryAddr *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			return ptr;
	}
	
	return NULL;
}


RcpOspfSummaryAddr *summaddr_match(uint32_t ip, uint32_t mask) {
	// summary address not configured?
	if (!is_configured)	
		return NULL;

	RcpOspfSummaryAddr *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		if (ptr->mask > mask) {
			continue;
		}
					
		if ((ptr->ip & ptr->mask) == (ip & ptr->mask)) {
			return ptr;
		}
	}
	
	return NULL;
}

int cliSummaryAddressCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// maybe we need to generate a router_id
	if (!shm->config.ospf_router_id)
		generate_router_id();
	if (!shm->config.ospf_router_id) {
		strcpy(data, "Error: cannot set router id\n");
		return RCPERR;
	}

	// extract data
	int index = 0;
	int noform = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}

	// cidr
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[index], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// find an existing entry
	RcpOspfSummaryAddr *found = summaddr_find(ip, mask);
	if (noform) {
		// summary address configured?
		if (found == NULL)
			return 0;
		
		// clear shared memory
		memset(found, 0, sizeof(RcpOspfSummaryAddr));

		is_configured = summaddr_any_configured();
		
		// remove summary address lsa
		redistSummaryDel(ip, mask);

		// run a static/connected/loopback route update to bring in any configured routes
		// previously replaced by the summary
		redistStaticUpdate();
		redistConnectedUpdate();
		redistConnectedSubnetsUpdate();
	}
	else {
		if (found) {
			// already configured
			if (argc == 3) {
				found->not_advertise = 1;
				redistSummaryNotAdvertise(ip, mask);
			}
			else
				redistSummaryAdd(ip, mask);

			return 0;
		}
		
		found = summaddr_find_empty();
		if (!found) {
			strcpy(data, "Error: summary address not configured, limit reached\n");
			return RCPERR;
		}
		memset(found, 0, sizeof(RcpOspfSummaryAddr));
		found->ip = ip;
		found->mask = mask;
		if (argc == 3)
			found->not_advertise = 1;
		found->valid = 1;
		is_configured = 1;

		// add summary address lsa
		if (argc == 3)
			redistSummaryNotAdvertise(ip, mask);
		else
			redistSummaryAdd(ip, mask);
		
	}

	return 0;
}

void summaddr_update(void) {
	// walk summaddr array and update lsa
	RcpOspfSummaryAddr *ptr;
	int i;

	// add summaddr lsa
	if (is_asbr) {
		for (i = 0, ptr = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr++) {
			if (!ptr->valid)
				continue;
		
			if (ptr->not_advertise == 1)
				redistSummaryNotAdvertise(ptr->ip, ptr->mask);
			else
				redistSummaryAdd(ptr->ip, ptr->mask);
		}
	}
	else {
		// remove all self originated external lsa
		flush_flooding();
		OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
		while (lsa != NULL) {
			OspfLsa *next = lsa->h.next;
			
			if (lsa->h.self_originated) {
				lsa->header.age = htons(MaxAge);
				lsadbFlood(lsa, 0);
				lsadbRemove(0, lsa);
				lsaFree(lsa);
				redist_del_cnt++;
			}
			
			lsa = next;
		}
		flush_flooding();
	}
}



	
