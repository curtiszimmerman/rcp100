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

//****************************************************
// lsa header list
//****************************************************

void lsahListAdd(OspfLsaHeaderList **head, OspfLsaHeader *lsah) {
	ASSERT(head != NULL);
	ASSERT(lsah != NULL);

	// don't add it if it is already in the list
	OspfLsaHeaderList *lst = *head;
//	while (lst != NULL) {
//		OspfLsaHeader *ptr = &lst->header;
//		if (ptr->type == lsah->type &&
//		     ptr->link_state_id == lsah->link_state_id &&
//		     ptr->adv_router == lsah->adv_router)
//			return;
//
//		lst = lst->next;
//	}
	
	// lsah is not in the list; allocate memory for a new one, and copy it
	lst = malloc(sizeof(OspfLsaHeaderList));
	if (lst == NULL) {
		printf("cannot allocate memory\n");
		exit(1);
	}
	memset(lst, 0, sizeof(OspfLsaHeaderList));
	
	// put the new lsa in the list
	lst->next = *head;
	memcpy(&lst->header, lsah, sizeof(OspfLsaHeader));
	*head = lst;
}
				
void lsahListRemoveAll(OspfLsaHeaderList **head) {
	ASSERT(head != NULL);
	if (*head == NULL) {
		return;
	}

	OspfLsaHeaderList *lst = *head;
	while (lst != NULL) {
		OspfLsaHeaderList *next = lst->next;
		free(lst);
		lst = next;
	}
	*head = NULL;
}

// are all the list elements acknowledged? 1 yes, 0 no
int lsahListIsAck(OspfLsaHeaderList **head) {
	ASSERT(head != NULL);
	if (*head == NULL) {
		return 0;
	}

	OspfLsaHeaderList *lst = *head;
	while (lst != NULL) {
		if (lst->ack == 0)
			return 0;
		lst = lst->next;
	}
	return 1;
}

void lsahListPurge(OspfLsaHeaderList **head) {
	ASSERT(head != NULL);
	if (*head == NULL) {
		return;
	}

	OspfLsaHeaderList *lst = *head;
	while (lst != NULL) {
		if (lst->ack == 0)
			break;
		lst = lst->next;
	}
	
	OspfLsaHeaderList *tmp = *head;
	*head = lst;
	lst = tmp;
	while (lst != NULL) {
		OspfLsaHeaderList *next = lst->next;
		free(lst);
		lst = next;
		if (lst == *head)
			break;
	}

}


void lsahListRemove(OspfLsaHeaderList **head, uint8_t type, uint32_t id, uint32_t adv_router) {
	ASSERT(head != NULL);
	if (*head == NULL) {
		return;
	}
	
	// remove first element
	OspfLsaHeaderList *lst = *head;
	if (lst->header.type == type &&
	    lst->header.link_state_id == id &&
	    lst->header.adv_router == adv_router) {
	    	*head = lst->next;
		free(lst);
		return;
	}
	
	OspfLsaHeaderList *prev = lst;
	lst = lst->next;
	while (lst != NULL) {
		if (lst->header.type == type &&
		    lst->header.link_state_id == id &&
		    lst->header.adv_router == adv_router) {
		    	prev->next = lst->next;
		    	free(lst);
		    	return;
		}
		prev = lst;
		lst = lst->next;
	}
}
