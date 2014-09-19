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
#include "dns.h"


// we keep two linked lists, one with active requests and one with inactive requests
// a maximum of 512 request limit will be imposed, see DNS_MAX_REQ in dns.h
static DnsReq *active = NULL;
static DnsReq *inactive = NULL;
static int initialized = 0;

// initialize inactive request list
void rq_list_init(void) {
	if (initialized)
		return;
	
	// allocate DNS_MAX_REQ in inactive list	
	int i;
	for (i = 0; i < DNS_MAX_REQ; i++) {
		DnsReq *rq = malloc(sizeof(DnsReq));
		if (rq == NULL) {
			fprintf(stderr, "Error: process %s, failed to allocate memory, exiting...\n", rcpGetProcName());
			exit(1);
		}
		memset(rq, 0, sizeof(DnsReq));
		rq->next = inactive;
		inactive = rq;
	}
	
	initialized = 1;
}

// get a request from inactive list
DnsReq *rq_malloc(void) {
	if (inactive == NULL)
		return NULL;
		
	DnsReq *rq = inactive;
	inactive = inactive->next;
	memset(rq, 0, sizeof(DnsReq));
	return rq;
}

// return a request to the inactive list
void rq_free(DnsReq *rq) {
	ASSERT(rq != NULL);
	// close socket
	if (rq->sock)
		close(rq->sock);
	rq->sock = 0;
	
	rq->next = inactive;
	inactive = rq;
}

// any pending requests?
DnsReq *rq_active(void) {
	return active;
}

// count active requests
int rq_count(void) {
	int rv = 0;
	DnsReq *ptr = active;
	while (ptr != NULL) {
		rv++;
		ptr = ptr->next;
	}
	return rv;
}

// add request to active list
void rq_add(DnsReq *ptr) {
	ptr->next = active;
	active = ptr;
	ptr->ttl = MAX_REQ_TTL;
	
	// open a socket
	ptr->port = random() & RCP_PORT_DNS_RANGE;
	ptr->port += RCP_PORT_DNS_START;
	ptr->sock = rx_open(ptr->port);
}

// find request in active list based on transaction id
DnsReq *rq_find(uint16_t tid) {
	DnsReq *ptr = active;
	while (ptr != NULL) {
		if (ptr->new_tid == tid)
			return ptr;
		ptr = ptr->next;
	}
	
	return NULL;
}

// remove request from active list
DnsReq *rq_del(DnsReq *req) {
	if (active == req) {
		active = req->next;
		return req;
	}
	
	DnsReq *ptr = active;
	while (ptr != NULL) {
		if (ptr->next == req) {
			ptr->next = ptr->next->next;
			return req;
		}
		ptr = ptr->next;
	}
	
	return NULL;
}

// remove all request from active list
void rq_clear_all(void) {
	if (!active)
		return;
		
	DnsReq *ptr = active;
	while (ptr != NULL) {
		DnsReq *next = ptr->next;
		
		// move the request to inactive list
		rq_free(ptr);

		ptr = next;
	}
	active = NULL;
}

void rq_clear_inactive(void) {
	rq_clear_all();
	
	DnsReq *ptr = inactive;
	while (ptr != NULL) {
		DnsReq *next = ptr->next;
		free(ptr);
		ptr = next;
	}
	
	initialized = 0;
}

// is this a request socket?
int rq_sock(int sock) {
	DnsReq *ptr = active;
	while (ptr != NULL) {
		if (ptr->sock == sock)
			return 1;
		ptr = ptr->next;
	}
	
	return 0;
}

// remove expired entries from active list
void rq_timer(void) {
	DnsReq *ptr = active;
	
	// walk the list
	while (ptr != NULL) {
		DnsReq *next = ptr->next;
		if (--ptr->ttl <= 0) {
			rq_del(ptr);
			rq_free(ptr);
			shm->stats.dns_err_rq_timeout++;
		}
		ptr = next;
	}
}
