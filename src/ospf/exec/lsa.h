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
#ifndef LSA_H
#define LSA_H
#include <stdint.h>
#include "librcp_limits.h"

// lsa header
typedef struct ospf_lsa_header_t {
	uint16_t age;
	uint8_t options;
#define LSA_TYPE_NONE 0
#define LSA_TYPE_ROUTER 1
#define LSA_TYPE_NETWORK 2
#define LSA_TYPE_SUM_NET 3
#define LSA_TYPE_SUM_ROUTER 4
#define LSA_TYPE_EXTERNAL 5
#define LSA_TYPE_MAX 6
	uint8_t type;
	uint32_t link_state_id;
	uint32_t adv_router;
	uint32_t seq;
	uint16_t checksum;
	uint16_t length;
} OspfLsaHeader;

typedef struct ospf_header_list_t {
	struct ospf_header_list_t *next;
	int ack;		// lsa was acknowledged and it can be removed from the list
	int sent;		// lsa was sent in dd description packets
			// when the packet is acknowledged, ack filed is set
	OspfLsaHeader header;
} OspfLsaHeaderList ;



// external lsa
typedef struct ext_lsa_data_t {
	uint32_t mask;
//   0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//  |E|     0       |		  metric		       |
  	uint32_t type_metric;	// type (MSB) and metric (uint24)
 	uint32_t fw_address;	// forwarding address
 	uint32_t tag;		// route tag
} ExtLsaData;

// summary lsa
typedef struct sum_lsa_data_t {
	uint32_t mask;
	uint32_t metric;
} SumLsaData;
 	
// network lsa
typedef struct net_lsa_data_t {
	uint32_t mask;
	// followed by ip address of attached routers as uint32_t
} NetLsaData;

// router lsa
typedef struct rtr_lsa_link_t {
	uint32_t link_id;
	uint32_t link_data;
	uint8_t type;
	uint8_t tos;
	uint16_t metric;
} RtrLsaLink;

typedef struct rtr_lsa_data_t {
	uint8_t flags;
	uint8_t reserved;
	uint16_t links;
	// followed by RtrLsaLink structures
} RtrLsaData;

typedef struct lsa_cost_t {
	uint32_t id;	// link id or forwarding address for external lsa; network format
	uint32_t mask;	// mask for external lsa; network format
	uint8_t type;
	uint8_t ext_type;	// 1 or 2 for external lsa
	uint32_t cost;	// in host format
	uint32_t tag;	// tag in host format
} LsaCost;

typedef struct lsa_t {
	struct lsa_h_t {
		// area linked list
		struct lsa_t *next;
		struct lsa_t *prev;
		// hash table
		struct lsa_t *hash_next;
		struct lsa_t **head;		// head pointer of the area list; it is used in the hash table to locate items
		// next pointer for various operations such as LS Update packets, Djikstra's algorithm priority queue etc.
		struct lsa_t *util_next;
		int size;	// total length of structure in bytes
		
		uint8_t self_originated;	// lsas originated by us have this flag set
		uint8_t del_flag;	// delete flag for summary lsa
		uint8_t flood;	// flood this lsa
		
		// somebody sent us our lsa with a higher seq - it could be from a previous incarnation;
		// we bump our global seq counter to the one received (old_self_originated_seq),
		// increment it, and generate a new lsa; the lsa if flooded after that
		uint8_t old_cnt;
		uint32_t old_self_originated_seq;	// network format
		
		uint32_t net_ip;	// the lsa was received on this network ip - in some cases, it will not be flooded on this network

		// cost calculation
		LsaCost *ncost;	// neighbor costs
		uint32_t cost;
		int ncost_cnt;	// neighbor count
		struct lsa_t *from[RCP_OSPF_ECMP_LIMIT];	// parent node reaching this
		int visited;	// the node was visited by djikstra's algorithm
		
		// next hop calculation
		// an lsa can be accessed by an interface (outhop and neighbor ip) or by nexthop (nexthop router id and nexthop ip)
		uint32_t outhop;	// output interface ip address
		void *rcpif;	// rcp interface for redistributed routes
		uint32_t nexthop;	// next hop router id

#define OSPF_ORIG_INCOMING 0
#define OSPF_ORIG_STATIC	1
#define OSPF_ORIG_CONNECTED 2
#define OSPF_ORIG_LOOPBACK 3
#define OSPF_ORIG_SUMMARY 4
#define OSPF_ORIG_RIP	1
		uint8_t originator;
		uint8_t maxage_candidate;
	} h;

	OspfLsaHeader header;

	union {
		RtrLsaData rtr;	// router lsa, type 1
		NetLsaData net;	// net lsa, type 2
		SumLsaData sum;	// sum lsa, type 3 and 4
		ExtLsaData ext;	// external lsa, type 5
	} u;
	
} OspfLsa;

// lsa.c
#define InitialSequenceNumber 0x80000001
#define MaxSequenceNumber 0x7fffffff
const char *lsa_type(OspfLsa *lsa);
int lsa_compare(OspfLsa *lsa1, OspfLsa *lsa2, int skip_seq, int skip_age);
int lsa_count_external(void);
int lsa_count_originated(void);
int lsa_count_area(uint32_t area_id);
extern uint32_t seq_number;
static inline void lsa_increment_seq(void) {
	if (seq_number == MaxSequenceNumber)
		seq_number = InitialSequenceNumber;
	else
		seq_number++;
}


// lsa_list.c
void lsaInitHashTable(void);
void lsaFree(OspfLsa *lsa);
void lsaListAddHash(OspfLsa **head, OspfLsa *lsa);
OspfLsa* lsaListRemove(OspfLsa **head, OspfLsa *lsa);
OspfLsa* lsaListReplace(OspfLsa **head, OspfLsa *oldlsa, OspfLsa *lsa);
OspfLsa *lsaListFind(OspfLsa **head, uint8_t type, uint32_t link_state_id, uint32_t adv_router);
OspfLsa *lsaListFind2(OspfLsa **head, uint8_t type, uint32_t link_state_id);
OspfLsa *lsaListFindRequest(OspfLsa **head, void *lsrq);
// lsa header list
void lsahListAdd(OspfLsaHeaderList **head, OspfLsaHeader *lsah);
void lsahListRemoveAll(OspfLsaHeaderList **head);
int lsahListIsAck(OspfLsaHeaderList **head);
void lsahListRemove(OspfLsaHeaderList **head, uint8_t type, uint32_t id, uint32_t adv_router);
void lsahListPurge(OspfLsaHeaderList **head);

void flush_flooding(void);



#endif
