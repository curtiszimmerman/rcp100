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

OspfLsa *queue = NULL;

#if 0
static void print_queue(void) {
	printf("****\n");
	OspfLsa *lsa = queue;
	while (lsa != NULL) {
		printf("%u/%d.%d.%d.%d, cost %u\n",
			lsa->header.type,
			RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
			lsa->h.cost);
		
		lsa = lsa->h.util_next;	
	}
	printf("****\n");
}
#endif

// insert lsa in the queue
void lsapq_push(OspfLsa *lsa) {
	ASSERT(lsa);
//printf("Djikstra algo: push %u/%d.%d.%d.%d at cost %u\n",
//	lsa->header.type, RCP_PRINT_IP(ntohl(lsa->header.link_state_id)), lsa->h.cost);		
	
	
	// if the lsa is already in the queue, don't add it
	OspfLsa *ptr = queue;
	while (ptr) {
		if (ptr == lsa)
			return;
		ptr = ptr->h.util_next;
	}
		
	// inserting on top
	lsa->h.util_next = queue;
	queue = lsa;
}

// get the smallest element
OspfLsa *lsapq_get(void) {
//print_queue();
//printf("Djikstra algo: pop\n");

	// empty head
	if (queue == NULL)
		return NULL;

	// one element
	if (queue->h.util_next == NULL) {
		// return head
		OspfLsa *tmp = queue;
		queue = NULL;
		return tmp;
	}

	OspfLsa *ptr = queue;
	OspfLsa *parent = NULL;
	OspfLsa *sptr = queue;
	OspfLsa *sparent = NULL;

	while (ptr != NULL) {
		if (ptr->h.cost < sptr->h.cost) {
			sptr = ptr;
			sparent = parent;
		}
		
		parent = ptr;
		ptr = ptr->h.util_next;
	}	
	
	if (sparent == NULL) {
		// return head
		OspfLsa *tmp = queue;
		queue = queue->h.util_next;
		return tmp;
	}
	
	OspfLsa *tmp = sptr;
	sparent->h.util_next = tmp->h.util_next;
	return tmp;
}
