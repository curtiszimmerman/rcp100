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
uint32_t seq_number = InitialSequenceNumber;
OspfLsa *extlist = NULL;

uint32_t systic_delta_lsa = 0;

const char *lsa_type(OspfLsa *lsa) {
	ASSERT(lsa != NULL);
	if (lsa->header.type > 5) {
		ASSERT(0);
		return "";
	}
	
	char *type[] = {
		"",
		"Router-LSA",
		"Network-LSA",
		"Summary-LSA",
		"Summary-LSA",
		"AS-external-LSA",
	};
	
	return type[lsa->header.type];
};



static void lsa_add_cost(OspfLsa *lsa) {
	ASSERT(lsa != NULL);

	// free the list from previous calculation
	if (lsa->h.ncost)
		free(lsa->h.ncost);
	lsa->h.ncost = NULL;
	lsa->h.ncost_cnt = 0;
	
	// create a new list based on lsa type
	if (lsa->header.type == LSA_TYPE_NETWORK) {
		// calculate the number of elements
		uint16_t len = ntohs(lsa->header.length) - sizeof(OspfLsaHeader) - sizeof(NetLsaData);
		int cnt = len / 4;
		
		if (cnt != 0) {
			// allocate memory
			lsa->h.ncost = malloc(cnt * sizeof(LsaCost));
			if (lsa->h.ncost == NULL) {
				printf("cannot allocate memory\n");
				exit(1);
			}
			memset(lsa->h.ncost, 0 , cnt * sizeof(LsaCost));
			lsa->h.ncost_cnt = cnt;
			
			// set cost elements
			uint32_t *nbip = (uint32_t *) ((uint8_t *) &lsa->u.net + sizeof(NetLsaData));
			LsaCost *ptr = lsa->h.ncost;
			int i;
			for (i = 0; i < cnt; i++, nbip++, ptr++) {
				ptr->id = *nbip;
				ptr->type = LSA_TYPE_ROUTER;
			}
		}
	}

	else if (lsa->header.type == LSA_TYPE_ROUTER) {
		// calculate the number of elements
		int cnt = ntohs(lsa->u.rtr.links);
		
		if (cnt != 0) {
			// allocate memory
			lsa->h.ncost = malloc(cnt * sizeof(LsaCost));
			if (lsa->h.ncost == NULL) {
				printf("cannot allocate memory\n");
				exit(1);
			}
			memset(lsa->h.ncost, 0 , cnt * sizeof(LsaCost));
			lsa->h.ncost_cnt = cnt;
	
			// set cost elements
			RtrLsaData *rdata = &lsa->u.rtr;
			RtrLsaLink *lnk = (RtrLsaLink *) ((uint8_t *) rdata + sizeof(RtrLsaData));
			LsaCost *ptr = lsa->h.ncost;
			int i;
			for (i = 0; i < cnt; i++, lnk++, ptr++) {
				ptr->id = lnk->link_id;
				ptr->cost = ntohs(lnk->metric);
				ptr->type = LSA_TYPE_NETWORK;
			}
		}
	}
	
	else if (lsa->header.type == LSA_TYPE_EXTERNAL) {
		// allocate memory
		lsa->h.ncost = malloc(sizeof(LsaCost));
		if (lsa->h.ncost == NULL) {
			printf("cannot allocate memory\n");
			exit(1);
		}
		memset(lsa->h.ncost, 0 , sizeof(LsaCost));
		lsa->h.ncost_cnt = 1;
		LsaCost *ptr = lsa->h.ncost;
		ptr->id = lsa->u.ext.fw_address;
		ptr->mask = lsa->u.ext.mask;
		ptr->type = LSA_TYPE_EXTERNAL;
		ptr->ext_type = (ntohl(lsa->u.ext.type_metric) & 0x80000000)? 2: 1;
		ptr->cost = ntohl(lsa->u.ext.type_metric) & 0x00ffffff;
		ptr->tag = ntohl(lsa->u.ext.tag);
	}
	
	else if (lsa->header.type == LSA_TYPE_SUM_NET ||  lsa->header.type == LSA_TYPE_SUM_ROUTER) {
		int cnt = 1;
		
		// allocate memory
		lsa->h.ncost = malloc(cnt * sizeof(LsaCost));
		if (lsa->h.ncost == NULL) {
			printf("cannot allocate memory\n");
			exit(1);
		}
		memset(lsa->h.ncost, 0 , cnt * sizeof(LsaCost));
		lsa->h.ncost_cnt = cnt;
		
		// set cost elements
		lsa->h.ncost->cost = ntohl(lsa->u.sum.metric) & 0x00ffffff;
		lsa->h.ncost->mask = lsa->u.sum.mask;
	}
}

// return 0 if equal, 1 or -1 if not equal
int lsa_compare(OspfLsa *lsa1, OspfLsa *lsa2, int skip_seq, int skip_age) {
	if (!skip_age) {
		int age1 = ntohs(lsa1->header.age);
		int age2 = ntohs(lsa2->header.age);
		int delta = age1 - age2;
		if (delta < 0)
			delta *= -1;
		
		if (delta > MaxAgeDiff)
			return 1;
	}

	if (!skip_seq && lsa1->header.seq != lsa2->header.seq)
		return 1;

	if (lsa1->header.type != lsa2->header.type)
		return 1;

	if (lsa1->header.link_state_id != lsa2->header.link_state_id)
		return 1;

	if (lsa1->header.adv_router != lsa2->header.adv_router)
		return 1;

	if (lsa1->header.length != lsa2->header.length)
		return 1;

	return memcmp(&lsa1->u, &lsa2->u, ntohs(lsa1->header.length) - sizeof(OspfLsaHeader)) ;
}


OspfLsa *lsadbGetList(uint32_t area_id, uint8_t type) {
	if (type == LSA_TYPE_EXTERNAL)
		return extlist;

	OspfArea *area = areaFind(area_id);
	if (area == NULL)
		return NULL;
		
	if (type >=  LSA_TYPE_MAX)
		type = 0;
		
	return area->list[type];
}

OspfLsa **lsadbGetListHead(uint32_t area_id, uint8_t type) {
	if (type == LSA_TYPE_EXTERNAL)
		return &extlist;

	OspfArea *area = areaFind(area_id);
	if (area == NULL)
		return NULL;
		
	if (type >=  LSA_TYPE_MAX)
		type = 0;
	
	return &area->list[type];
}

void lsadbAdd(uint32_t area_id, OspfLsa *lsa) {
	TRACE_FUNCTION();
	ASSERT(lsa != NULL);
	
	lsa_add_cost(lsa);	
	lsaListAddHash(lsadbGetListHead(area_id, lsa->header.type), lsa);
	
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"%s %d.%d.%d.%d, from %d.%d.%d.%d, seq 0x%x added to database",
		lsa_type(lsa),
		RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
		RCP_PRINT_IP(ntohl(lsa->header.adv_router)),
		ntohl(lsa->header.seq));

	
	
	// generate summary lsa - originated during spf calculation
//	if (lsa->header.type == LSA_TYPE_NETWORK)
//		lsa_originate_sumnet(area_id, lsa);

	// generate summary lsa
//	if (lsa->header.type == LSA_TYPE_EXTERNAL)
//		lsa_originate_sumrouter(area_id, lsa);
	
	// trigger an spf calculation
	spfTrigger();
}

void lsadbRemove(uint32_t area_id, OspfLsa *lsa) {
	TRACE_FUNCTION();
	ASSERT(lsa != NULL);

	if (lsa->header.type != LSA_TYPE_EXTERNAL) {
		OspfArea *area = areaFind(area_id);
		if (!area) {
			ASSERT(0);
			return;
		}
		if (lsa == area->mylsa)
			area->mylsa = NULL;
	}

	if (lsaListRemove(lsadbGetListHead(area_id, lsa->header.type), lsa)) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
			"%s %d.%d.%d.%d, from %d.%d.%d.%d, seq 0x%x removed from database",
			lsa_type(lsa),
			RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
			RCP_PRINT_IP(ntohl(lsa->header.adv_router)),
			ntohl(lsa->header.seq));
	}

	// trigger an spf calculation
	spfTrigger();
}

void lsadbReplace(uint32_t area_id, OspfLsa *oldlsa, OspfLsa *newlsa) {
	TRACE_FUNCTION();
	ASSERT(oldlsa != NULL);
	ASSERT(newlsa != NULL);
	ASSERT(oldlsa->header.type == newlsa->header.type);

	lsa_add_cost(newlsa);	
	if (lsaListReplace(lsadbGetListHead(area_id, oldlsa->header.type), oldlsa, newlsa)) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
			"%s %d.%d.%d.%d, from %d.%d.%d.%d, seq 0x%x removed from database",
			lsa_type(oldlsa),
			RCP_PRINT_IP(ntohl(oldlsa->header.link_state_id)),
			RCP_PRINT_IP(ntohl(oldlsa->header.adv_router)),
			ntohl(oldlsa->header.seq));

		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
			"%s %d.%d.%d.%d, from %d.%d.%d.%d, seq 0x%x added to database",
			lsa_type(newlsa),
			RCP_PRINT_IP(ntohl(newlsa->header.link_state_id)),
			RCP_PRINT_IP(ntohl(newlsa->header.adv_router)),
			ntohl(newlsa->header.seq));
	}

	// trigger an spf calculation if the lsa changed
	if (lsa_compare(oldlsa, newlsa, 1, 1)) // skip seq, skip age
		spfTrigger();
}

// return lsa if found, NULL if not found
// link_state_id and adv_router in network format
OspfLsa *lsadbFind(uint32_t area_id, uint8_t type, uint32_t link_state_id, uint32_t adv_router) {
	TRACE_FUNCTION();
	return lsaListFind(lsadbGetListHead(area_id, type), type, link_state_id, adv_router);
}
OspfLsa *lsadbFind2(uint32_t area_id, uint8_t type, uint32_t link_state_id) {
	TRACE_FUNCTION();
	return lsaListFind2(lsadbGetListHead(area_id, type), type, link_state_id);
}

// return lsa if found, NULL if not found
OspfLsa *lsadbFindHeader(uint32_t area_id, OspfLsaHeader *lsah) {
	TRACE_FUNCTION();
	ASSERT(lsah != NULL);
	return lsaListFind(lsadbGetListHead(area_id, lsah->type), lsah->type, lsah->link_state_id, lsah->adv_router);
}

// return lsa if found, NULL if not found
OspfLsa *lsadbFindRequest(uint32_t area_id, OspfLsRequest *lsrq) {
	TRACE_FUNCTION();
	ASSERT(lsrq != NULL);
	uint8_t type = (uint8_t) ntohl(lsrq->type);
	return lsaListFindRequest(lsadbGetListHead(area_id, type), lsrq);
}


// renew a self originated lsa
static void lsa_renew(OspfLsa *lsa) {
	TRACE_FUNCTION();
	ASSERT(lsa != NULL);
	if (lsa->h.self_originated == 0) {
		ASSERT(0);
		return;
	}
	
	// reset age
	lsa->header.age = 0;
	
	// add a new sequence number
	lsa->header.seq = htonl(seq_number);
	lsa_increment_seq();
	
	// recalculate checksum
	OspfLsaHeader *lsah = &lsa->header;
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);
	
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"renewing %s %d.%d.%d.%d LSA\n",
		lsa_type(lsa), RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
}

static int lsa_flooded = 0;
static void lsa_tx_network(OspfLsa *lsa, OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(lsa != NULL);
	ASSERT(net != NULL);
	
	if (net->state != NETSTATE_DROTHER && 
	     net->state != NETSTATE_BACKUP &&
	     net->state != NETSTATE_DR)
		return;

	// if nobody on the network, do nothing
	if (!net->neighbor)
		return;

	// if only one neighbor, and the neighbor is not full
	if (!net->neighbor->next && net->neighbor->state != NSTATE_FULL)
		return;
	
	
	// if there is no dr/bdr yet
	if (net->designated_router == 0 && net->backup_router == 0)
		return;

// section 13.3
// (4) If the new LSA was received on this interface, and the
//            interface state is Backup (i.e., the router itself is the
//            Backup Designated Router), examine the next interface.
	if (net->state == NETSTATE_BACKUP && lsa->h.net_ip == net->ip)
		return;

	// if state is DROther
	//	send to AllDRouters_GROUP
	// else if state is DR or BDR
	//	send to AllSPFRouters_GROUP
	if (net->state == NETSTATE_DROTHER) {
		net->flooding_cnt++;
		if (++lsa_flooded < OSPF_FLOOD_MAX) {
			// build the packet
			OspfPacket pktout;
			lsupdateBuildPkt(net, &pktout, lsa);
			txpacket_unicast(&pktout, net, AllDRouters_GROUP_IP);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA | RLOG_FC_OSPF_PKT,
				"LS Update sent to AllDRouters on network %d.%d.%d.%d",
					RCP_PRINT_IP(net->ip));
		}
		else {
			// add a copy of the lsa in flooding_alldrouters list
			ASSERT(lsa->h.size != 0);
			OspfLsa *newlsa = malloc(lsa->h.size);
			if (newlsa == NULL) {
				printf("cannot allocate memory\n");
				exit(1);
			}

			memcpy(newlsa, lsa, lsa->h.size);
			// clean h structure, it contains pointers to allocated memory
			memset(&newlsa->h, 0, sizeof(struct lsa_h_t));
			newlsa->h.util_next = NULL;

			// add it to the list
			if (net->flooding_alldrouters == NULL)
				net->flooding_alldrouters = newlsa;
			else {
				newlsa->h.util_next = net->flooding_alldrouters;
				net->flooding_alldrouters = newlsa;
			}
		}
	}				
	else {
		net->flooding_cnt++;
		if (++lsa_flooded < OSPF_FLOOD_MAX) {
			// build the packet
			OspfPacket pktout;
			lsupdateBuildPkt(net, &pktout, lsa);
			txpacket(&pktout, net);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA | RLOG_FC_OSPF_PKT,
				"LS Update sent to AllSPFRouters on network %d.%d.%d.%d",
				RCP_PRINT_IP(net->ip));
		}
		else {
			// add a copy of the lsa in flooding_allspfrouters list
			ASSERT(lsa->h.size != 0);
			OspfLsa *newlsa = malloc(lsa->h.size);
			if (newlsa == NULL) {
				printf("cannot allocate memory\n");
				exit(1);
			}

			memcpy(newlsa, lsa, lsa->h.size);
			// clean h structure, it contains pointers to allocated memory
			memset(&newlsa->h, 0, sizeof(struct lsa_h_t));
			newlsa->h.util_next = NULL;
			
			// add it to the list
			if (net->flooding_allspfrouters == NULL)
				net->flooding_allspfrouters = newlsa;
			else {
				newlsa->h.util_next = net->flooding_allspfrouters;
				net->flooding_allspfrouters = newlsa;
			}
		}
	}
		
	// put the lsa in the retransmit list for each neighbor
	OspfNeighbor *nb = net->neighbor;
	while (nb != NULL) {
		if (nb->state >= NSTATE_EXSTART) {
			if (net->state == NETSTATE_DROTHER) {
				// send the packet only to dr and bdr	
				if (nb->ip != net->designated_router &&
				     nb->ip != net->backup_router) {
					nb = nb->next;
					continue;
				}
			}
			
			ASSERT(lsa->h.size != 0);
			OspfLsa *newlsa = malloc(lsa->h.size);
			if (newlsa == NULL) {
				printf("cannot allocate memory\n");
				exit(1);
			}

			memcpy(newlsa, lsa, lsa->h.size);
			// clean h structure, it contains pointers to allocated memory
			memset(&newlsa->h, 0, sizeof(struct lsa_h_t));
			newlsa->h.next = NULL;
			
			// replace an old one if any, or add the new one
			OspfLsa *found = lsaListFind(&nb->rxmt_list, lsa->header.type, lsa->header.link_state_id, lsa->header.adv_router);
			if (found) {
				lsaListRemove(&nb->rxmt_list, found);
				lsaFree(found);
			}
			lsaListAddHash(&nb->rxmt_list, newlsa);

			// restart rxmt timer if necessary
			if (nb->rxmt_list_timer <= 0)
				nb->rxmt_list_timer = net->rxtm_interval;
		}
		nb = nb->next;
	}
}



static void tx_area(OspfLsa *lsa, uint32_t area_id) {
	TRACE_FUNCTION();
	ASSERT(lsa != NULL);
	
	// area_id should be a valid area
	OspfArea *area = areaFind(area_id);
	if (!area) {
		ASSERT(0);
		return;
	}

	OspfNetwork *net = area->network;
	while (net) {
		if (net->state >= NETSTATE_DROTHER) {
			// self originated lsas are flooded regardless the network
			if (lsa->h.self_originated)
				lsa_tx_network(lsa, net);
			// maxage lsas from other routers are sent only if we are DR or BDR
			else if (ntohs(lsa->header.age) ==  MaxAge && net->state >= NETSTATE_BACKUP)
				lsa_tx_network(lsa, net);
			// if the network is in the state DROther, we don't flood lsas we learned about on this network
			else if (net->state == NETSTATE_DROTHER && net->ip == lsa->h.net_ip)
				;
			// probably more to come...
			else
				lsa_tx_network(lsa, net);
		}
		net = net->next;
	}
}
	

void lsadbFlood(OspfLsa *lsa, uint32_t area_id) {
	TRACE_FUNCTION();
	ASSERT(lsa != NULL);

	if (lsa->header.type != LSA_TYPE_EXTERNAL) {
		return tx_area(lsa, area_id);
	}
	
	
	// external routes are sent in all non-stub areas
	OspfArea *area = areaGetList();
	while (area != NULL) {
		if (!area->stub)
			tx_area(lsa, area->area_id);
		area = area->next;
	}
}

// self originated lsa will have to be specifically deleted; this function does not remove them
static void lsadbAgeAreaType(uint32_t area_id, uint8_t type) {
	OspfLsa *lsa = lsadbGetList(area_id,  type);

	while (lsa != NULL) {
		// clear utility next pointer
		lsa->h.util_next = NULL;
		OspfLsa *next = lsa->h.next;
		
		// aging
		uint16_t age = ntohs(lsa->header.age);
		++age;
		
		// mark the lsa in order to issue a trap at MaxAge
		if (age > (MaxAge - 5) && age < MaxAge)
			lsa->h.maxage_candidate = 1;
			
		if (age >= MaxAge) {
			if (lsa->h.self_originated) {
				lsa_renew(lsa);
				lsadbFlood(lsa, area_id);
			}
			else {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
					"%s %d.%d.%d.%d MaxAge reached\n",
					lsa_type(lsa), RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
				lsa->header.age = htons(MaxAge);
				
				// reflood it with max age
				lsadbFlood(lsa, area_id);
	
				if (lsa->h.maxage_candidate)
					trap_MaxAgeLsa(lsa, area_id);
 				// remove lsa
				lsadbRemove(area_id, lsa);
				lsaFree(lsa);
			}
			lsa = next;
			continue;
		}
		else 
			lsa->header.age = htons(age);
		
		// dealing with old instances of a self-originated lsa reported by
		// other router, but with a different seq number, possibly from other reincarnation
		if (lsa->h.self_originated && lsa->h.old_cnt > 0) {
			if (--lsa->h.old_cnt == 0) {
				// update global seq number if necessary
				uint32_t seq = ntohl(lsa->h.old_self_originated_seq);
				if (seq > seq_number) {
					seq_number = seq;
					lsa_increment_seq();
				}

				lsa_renew(lsa);
				lsadbFlood(lsa, area_id);
			}
		}
		
// flooding in two seconds - 
//MinLSArrival
//	For any	particular LSA,	the minimum time that must elapse
//	between	reception of new LSA instances during flooding.	LSA
//	instances received at higher frequencies are discarded.	The
//	value of MinLSArrival is set to	1 second.
		else if (lsa->h.flood == 1)
			lsa->h.flood = 2;
		else if (lsa->h.flood >= 2) {
			if (lsa_flooded < OSPF_TIMEOUT_FLOOD_MAX) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
					"Flooding %s %d.%d.%d.%d",
					lsa_type(lsa), RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
				
				lsadbFlood(lsa, area_id);
				lsa->h.flood = 0;
			}
		}

		// check refresh time for self->originated LSAs
		else if (lsa->h.self_originated && ntohs(lsa->header.age) > LSRefreshTime) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
				"%s %d.%d.%d.%d refreshed",
				lsa_type(lsa), RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
			
			lsa_renew(lsa);
			lsadbFlood(lsa, area_id);
		}
		
		// looking for our lsas from other incarnation
		// they will show with our router_id, however they don't have the self-generated flag set
		// the lsa needs to be flashed from the network
		else if (lsa->h.self_originated == 0 && ntohl(lsa->header.adv_router) == shm->config.ospf_router_id) {
			lsa->header.age = htons(MaxAge);
			lsadbFlood(lsa, area_id);

			// remove lsa
			lsadbRemove(area_id, lsa);
			lsaFree(lsa);
		}
				
		lsa = next;
	}
}

void flush_flooding(void) {
	OspfArea *area = areaGetList();

	// for each area
	while (area != NULL) {
		OspfNetwork *net = area->network;
		while (net) {
			if (net->flooding_alldrouters) {
				OspfLsa *nextlsa = net->flooding_allspfrouters;
				while (nextlsa) {
					OspfPacket pktout;
					nextlsa = lsupdateBuildMultiplePkt(net, &pktout, nextlsa, 0);
					txpacket_unicast(&pktout, net, AllDRouters_GROUP_IP);
				}
				
				// remove the list
				OspfLsa *lsa = net->flooding_alldrouters;
				while (lsa) {
					OspfLsa *next = lsa->h.util_next;
					free(lsa);
					lsa = next;
				}
				net->flooding_alldrouters = NULL;
			}

			if (net->flooding_allspfrouters) {
				OspfLsa *nextlsa = net->flooding_allspfrouters;
				while (nextlsa) {
					OspfPacket pktout;
					nextlsa = lsupdateBuildMultiplePkt(net, &pktout, nextlsa, 0);
					txpacket(&pktout, net);
				}
				
				// remove the list
				OspfLsa *lsa = net->flooding_allspfrouters;
				while (lsa) {
					OspfLsa *next = lsa->h.util_next;
					free(lsa);
					lsa = next;
				}
				net->flooding_allspfrouters = NULL;
			}
		
			if (net->flooding_cnt) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_LSA,
					"%d LSAs flooded on network %d.%d.%d.%d",
					net->flooding_cnt, RCP_PRINT_IP(net->ip));
				net->flooding_cnt = 0;
			}
			
			net = net->next;
		}
		area = area->next;
	}


}


void lsadbTimeout(void) {
	TRACE_FUNCTION();
	struct tms tm;
	uint32_t tic1 = times(&tm);

	flush_flooding();
	lsa_flooded = 0;	// limit the number of lsas flooded in one timeout cycle to OSPF_FLOOD_MAX
	OspfArea *area = areaGetList();

	// for each area
	while (area != NULL) {
		if (area->network != NULL) {
			int i;
			for (i = 0; i < LSA_TYPE_EXTERNAL; i++) {
				lsadbAgeAreaType(area->area_id, i);
			}
		}
		area = area->next;
	}
	
	// external lsa
	lsadbAgeAreaType(0, LSA_TYPE_EXTERNAL);
	flush_flooding();
	
	uint32_t tic2 = times(&tm);
	uint32_t delta;
	if (tic2 > tic1)
		delta = tic2 - tic1;
	else
		delta = tic1 - tic2;
	
	if (delta > systic_delta_lsa) {
		systic_delta_lsa = delta;
		rcpDebug("lsa timeout delta %u\n", systic_delta_lsa);
	}
	
}

// remove all lsa from this area id with the exception of external lsa
void lsadbRemoveArea(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (!area)
		return;

	int i;
	for (i = 1; i < LSA_TYPE_EXTERNAL; i++) {	// external lsa are not deleted
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa != NULL) {
			OspfLsa *next = lsa->h.next;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
				"%s %d.%d.%d.%d removed from database\n",
				lsa_type(lsa), RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
			lsaFree(lsa);
			lsa = next;
		}
		
		// set the head of the list to NULL
		area->list[i] = NULL;
		area->mylsa = NULL;
	}
}

// age all self-originated lsa in this area
void lsadbAgeArea(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (!area)
		return;
	
	flush_flooding();
	int i;
	for (i = 1; i < LSA_TYPE_EXTERNAL; i++) {	// external lsa are not aged
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa != NULL) {
			if (lsa->h.self_originated) {
				lsa->header.age = htons(MaxAge);
				lsadbFlood(lsa, area->area_id);
			}
			lsa = lsa->h.next;
		}
	}
	flush_flooding();
}


// age and remove router lsa from this neighbor
// the lsa is flooded only if the router is DR or BDR on the specific network
void lsadbAgeRemoveNeighbor(uint32_t nb_router_id, uint32_t area_id) {
	spfTrigger();

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_LSA,
		"removing neighbor %d.%d.%d.%d", RCP_PRINT_IP(nb_router_id));
		
	// remove router lsa for this neighbor - a new one will be generated anyway
	flush_flooding();
	OspfLsa *lsa = lsadbFind(area_id, LSA_TYPE_ROUTER, htonl(nb_router_id), htonl(nb_router_id));
	if (lsa) {
		lsa->header.age = htons(MaxAge);
		lsadbFlood(lsa, area_id);
		lsadbRemove(area_id, lsa);
		lsaFree(lsa);
	}
}

// age and remove network lsa from this neighbor
void lsadbAgeRemoveNeighborNetwork(uint32_t nb_router_id, OspfNetwork *net) {
	// remove router lsa for this neighbor - a new one will be generated anyway
	flush_flooding();
	OspfLsa *lsa = lsadbGetList(net->area_id, LSA_TYPE_NETWORK);
	while (lsa) {
		// if this lsa was originated by the neighbor
		if (lsa->header.adv_router == htonl(nb_router_id)) {
			// if this lsa describes this network
			uint32_t ip = ntohl(lsa->header.link_state_id);
			uint32_t mask = ntohl(lsa->u.net.mask);
			if ((ip & mask) == (net->ip & net->mask)) {
				// remove lsa
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_LSA,
					"removing network LSA from neighbor %d.%d.%d.%d",
					RCP_PRINT_IP(nb_router_id));
				lsa->header.age = htons(MaxAge);
				lsadbFlood(lsa, net->area_id);
				lsadbRemove(net->area_id, lsa);
				lsaFree(lsa);
				break;
			}
		}
		lsa = lsa->h.next;
	}
}

// count external lsa
int lsa_count_external(void) {
	int rv = 0;
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		rv++;
		lsa = lsa->h.next;
	}
	return rv;
}

// cont lsa in an area
int lsa_count_area(uint32_t area_id) {
	int rv = 0;
	int i;
	for (i = LSA_TYPE_ROUTER; i < LSA_TYPE_EXTERNAL; i++) {	// all lsa but not external
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa) {
			rv++;
			lsa = lsa->h.next;
		}
	}
	return rv;
}

// cont originated lsa
int lsa_count_originated(void) {
	int rv = 0;
	
	OspfArea *area = areaGetList();
	while (area) {
		int i;
		for (i = LSA_TYPE_ROUTER; i < LSA_TYPE_EXTERNAL; i++) {	// all lsa but not external
			OspfLsa *lsa = lsadbGetList(area->area_id, i);
			while (lsa) {
				if (lsa->h.self_originated)
					rv++;
				lsa = lsa->h.next;
			}
		}
		
		area = area->next;
	}
	
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		if (lsa->h.self_originated)
			rv++;
		lsa = lsa->h.next;
	}
	return rv;
}


static void validate_lsa(OspfLsa *lsa) {
	ASSERT(lsa != NULL);
	
	OspfLsaHeader *lsah = &lsa->header;
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	uint16_t fl = fletcher16(msg, sz, 0);
	if (fl != 0) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_ERR, RLOG_FC_OSPF_LSA,
			"invalid checksum for LSA %d.%d.%d.%d from %d.%d.%d.%d",
			RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
			RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
	}
}

void lsa_integrity(void) {
	OspfArea *area = areaGetList();
	while (area) {
		int i;
		for (i = LSA_TYPE_ROUTER; i < LSA_TYPE_EXTERNAL; i++) {	// all lsa but not external
			OspfLsa *lsa = lsadbGetList(area->area_id, i);
			while (lsa) {
				validate_lsa(lsa);
				lsa = lsa->h.next;
			}
		}
		
		area = area->next;
	}
	
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		validate_lsa(lsa);
		lsa = lsa->h.next;
	}
}
