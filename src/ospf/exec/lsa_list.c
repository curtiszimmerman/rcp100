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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "librcp.h"
#include "lsa.h"
#include "packet.h"


//****************************************************
// lsa hash table
//****************************************************
#define LSA_HASH_MAX 4096
#define LSA_HASH_MASK 0xfff
static OspfLsa *bucket[LSA_HASH_MAX];
static int bucket_initialized = 0;

void lsaInitHashTable(void) {
	if (!bucket_initialized) {
		memset(bucket, 0, sizeof(bucket));
		bucket_initialized = 1;
	}
}

// derived from Bob Jenkins One-at-a-Time hash
static unsigned hash(uint32_t ip) {
	uint8_t *p = (uint8_t *) &ip;
	unsigned h = 0;

	int i;
	for ( i = 0; i < 4; i++, p++) {
		h += *p;
		h += ( h << 10 );
		h ^= ( h >> 6 );
	}

	h += ( h << 3 );
	h ^= ( h >> 11 );
	h += ( h << 15 );
	return h & LSA_HASH_MASK;
}

static void lsaHashAdd(OspfLsa **head, OspfLsa *lsa) {
	ASSERT(head);
	ASSERT(lsa);
	
	unsigned index = hash(lsa->header.link_state_id);

//debug
#if 0
	// check if lsa is already present in the list
	OspfLsa *ptr = bucket[index];
	while (ptr) {
		if (ptr == lsa) {
			ASSERT(0);
			return;
		}
		ptr = ptr->h.hash_next;
	}
#endif
//end debug
	
	
	lsa->h.head = head;	// initialize head - used in searches
	lsa->h.hash_next = bucket[index];
	bucket[index] = lsa;
}

static void lsaHashRemove(OspfLsa *lsa) {
	ASSERT(lsa);

	unsigned index = hash(lsa->header.link_state_id);

	// empty
	if (bucket[index] == NULL)
		return;
	
	// first
	if (bucket[index] == lsa) {
		bucket[index] = lsa->h.hash_next;
		lsa->h.hash_next = NULL;
		return;
	}
	
	OspfLsa *ptr = bucket[index];
	while (ptr->h.hash_next) {
		if (ptr->h.hash_next == lsa) {
			ptr->h.hash_next = lsa->h.hash_next;
			lsa->h.hash_next = NULL;
			return;
		}
		ptr = ptr->h.hash_next;
	}
}


//****************************************************
// regular lsa list
//****************************************************
void lsaFree(OspfLsa *lsa) {
	ASSERT(lsa != NULL);
	if (lsa->h.ncost != NULL) {
		free(lsa->h.ncost);
	}
	lsaHashRemove(lsa);
	free(lsa);
}

// add an LSA to a list and to the hash table
void lsaListAddHash(OspfLsa **head, OspfLsa *lsa) {
	ASSERT(head != NULL);
	ASSERT(lsa != NULL);
//printf("lsa add head %p, %d.%d.%d.%d\n", head, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	// add lsa to hash table
	lsaHashAdd(head, lsa);
	
	// add lsa to area linked list
	lsa->h.next = NULL;
	lsa->h.prev = NULL;
	if (*head == NULL) {
		*head = lsa;
		return;
	}
	
	lsa->h.next = *head;
	if (lsa->h.next)
		lsa->h.next->h.prev = lsa;
	*head = lsa;

#if 0
//debug: loop trap
{
	// look for the lsa in the hash table
	unsigned index = hash(lsa->header.link_state_id);
	OspfLsa *ptr = bucket[index];
	int cnt = 0;
	while (ptr) {
printf("lsa %p add trap %p,%d.%d.%d.%d, hash_next %p\n", lsa, ptr, RCP_PRINT_IP(ntohl(ptr->header.link_state_id)), ptr->h.hash_next);		
		ptr = ptr->h.hash_next;
		cnt++;
		if (cnt > 50)
			sleep(1);
	}
}
#endif
}


OspfLsa* lsaListRemove(OspfLsa **head, OspfLsa *lsa) {
	ASSERT(lsa != NULL);
	ASSERT(head != NULL);
	lsaHashRemove(lsa);

	if (*head == NULL)
		return NULL;
		
	// head of the list
	if (*head == lsa) {
		*head = lsa->h.next;
		if (*head)
			(*head)->h.prev = NULL;
		return lsa;
	}
	
	ASSERT(lsa->h.prev != NULL);
	lsa->h.prev->h.next = lsa->h.next;
	if (lsa->h.next)
		lsa->h.next->h.prev = lsa->h.prev;
#if 0
//debug: loop trap
{
	// look for the lsa in the hash table
	unsigned index = hash(lsa->header.link_state_id);
	OspfLsa *ptr = bucket[index];
	while (ptr) {
printf("lsa %p remove trap %p,%d.%d.%d.%d, hash_next %p\n", lsa, ptr, RCP_PRINT_IP(ntohl(ptr->header.link_state_id)), ptr->h.hash_next);		
		ptr = ptr->h.hash_next;
	}
}	
#endif
	return lsa;	
}

OspfLsa* lsaListReplace(OspfLsa **head, OspfLsa *oldlsa, OspfLsa *lsa) {
	ASSERT(lsa != NULL);
	ASSERT(oldlsa != NULL);
	ASSERT(head != NULL);

	if (*head == NULL)
		return NULL;
	
	// replace the lsa in hash table
	lsaHashRemove(oldlsa);
	lsaHashAdd(head, lsa);
	
	// replace the lsa in the linked list	
	lsa->h.next = oldlsa->h.next;
	lsa->h.prev = oldlsa->h.prev;
	if (oldlsa->h.next)
		oldlsa->h.next->h.prev = lsa;
	if (oldlsa->h.prev)
		oldlsa->h.prev->h.next = lsa;
	
	// head of the list
	if (*head == oldlsa) {
		*head = lsa;
	}
	
#if 0
//debug: loop trap
{
	// look for the lsa in the hash table
	unsigned index = hash(lsa->header.link_state_id);
	OspfLsa *ptr = bucket[index];
	while (ptr) {
printf("lsa %p replace trap %p,%d.%d.%d.%d, hash_next %p\n", lsa, ptr, RCP_PRINT_IP(ntohl(ptr->header.link_state_id)), ptr->h.hash_next);		
		ptr = ptr->h.hash_next;
	}
}	
#endif	
	return oldlsa;
}

// return lsa if found, NULL if not found
// link_state_id and adv_router in network format
OspfLsa *lsaListFind(OspfLsa **head, uint8_t type, uint32_t link_state_id, uint32_t adv_router) {
	ASSERT(head != NULL);
	if (*head == NULL)
		return NULL;
	
	// look for the lsa in the hash table
	unsigned index = hash(link_state_id);
	OspfLsa *lsa = bucket[index];
	while (lsa) {
#if 0
//debug
printf("find lsa %p,%d.%d.%d.%d, head %p, hash_next %p\n", lsa, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
lsa->h.head, lsa->h.hash_next);		
#endif
		if (lsa->h.head == head &&
		     lsa->header.type == type &&
		     lsa->header.link_state_id == link_state_id &&
		     lsa->header.adv_router == adv_router)
			return lsa;
		lsa = lsa->h.hash_next;
	}
	
	return NULL; // not found
}

// return lsa if found, NULL if not found
// link_state_id in network format
OspfLsa *lsaListFind2(OspfLsa **head, uint8_t type, uint32_t link_state_id) {
	ASSERT(head != NULL);

	if (*head == NULL)
		return NULL;
			
	// match type, link_state_id and adv_router in the header
	// the caller will have to look at seq numbers

	// look for the lsa in the hash table
	unsigned index = hash(link_state_id);
	OspfLsa *lsa = bucket[index];
	while (lsa) {
		if (lsa->h.head == head &&
		     lsa->header.type == type &&
		     lsa->header.link_state_id == link_state_id)
			return lsa;
		lsa = lsa->h.hash_next;
	}

	return NULL; // not found
}
	

// return lsa if found, NULL if not found
OspfLsa *lsaListFindRequest(OspfLsa **head, void *rq) {
	ASSERT(rq != NULL);
	ASSERT(head != NULL);
	OspfLsRequest *lsrq = (OspfLsRequest *) rq;
	
	if (*head == NULL)
		return NULL;
	
	// match type, link_state_id and adv_router in the header
	// the caller will have to look at seq numbers
	// look for the lsa in the hash table
	unsigned index = hash(lsrq->id);
	OspfLsa *lsa = bucket[index];
	while (lsa) {
		// lsa->header.type is uint8_t while lsrq->type is unint32_t
		int type =  ntohl(lsrq->type);
		if (lsa->h.head == head &&
		     lsa->header.type == type &&
		     lsa->header.link_state_id == lsrq->id &&
		     lsa->header.adv_router == lsrq->adv_router)
			return lsa;
		lsa = lsa->h.hash_next;
	}

	return NULL; // not found
}

