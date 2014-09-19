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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "dhcp.h"
#include "lease.h"

//********************************************************
// hash tables, for each ip, mac and timestamp
//********************************************************
static Lease *ht_mac[LEASE_MAC_HASH_MAX];
static Lease *ht_ip[LEASE_IP_HASH_MAX];
static Lease *ht_tstamp[LEASE_TSTAMP_HASH_MAX];

static inline void add_mac(Lease *lease) {
	ASSERT(lease);
	unsigned hash = hash_mac(lease->mac);
	lease->next_mac = ht_mac[hash];
	ht_mac[hash] = lease;
}

static inline void add_ip(Lease *lease) {
	ASSERT(lease);
	unsigned hash = hash_ip(lease->ip);
	lease->next_ip = ht_ip[hash];
	ht_ip[hash] = lease;
}

static inline void add_tstamp(Lease *lease) {
	ASSERT(lease);
	unsigned hash = hash_tstamp(lease->tstamp);
	lease->next_tstamp = ht_tstamp[hash];
	ht_tstamp[hash] = lease;
}

static void remove_mac(Lease *lease) {
	ASSERT(lease);
	unsigned hash = hash_mac(lease->mac);
	if (ht_mac[hash] == NULL) {
		ASSERT(0);
		return;
	}
	
	// remove first element
	if (ht_mac[hash] == lease) {
		ht_mac[hash] = lease->next_mac;
		return;
	}
	
	// remove from the list
	Lease *prev = ht_mac[hash];
	Lease *ptr = ht_mac[hash]->next_mac;
	while (ptr) {
		if (ptr == lease) {
			prev->next_mac = lease->next_mac;
			return;
		}
		
		prev = ptr;
		ptr = ptr->next_mac;
	}	
	
	ASSERT(0);
}

static void remove_ip(Lease *lease) {
	ASSERT(lease);
	unsigned hash = hash_ip(lease->ip);
	if (ht_ip[hash] == NULL) {
		ASSERT(0);
		return;
	}
	
	// remove first element
	if (ht_ip[hash] == lease) {
		ht_ip[hash] = lease->next_ip;
		return;
	}
	
	// remove from the list
	Lease *prev = ht_ip[hash];
	Lease *ptr = ht_ip[hash]->next_ip;
	while (ptr) {
		if (ptr == lease) {
			prev->next_ip = lease->next_ip;
			return;
		}
		
		prev = ptr;
		ptr = ptr->next_ip;
	}	
	
	ASSERT(0);
}

static void remove_tstamp(Lease *lease) {
	ASSERT(lease);
	unsigned hash = hash_tstamp(lease->tstamp);
	if (ht_tstamp[hash] == NULL) {
		ASSERT(0);
		return;
	}
	
	// remove first element
	if (ht_tstamp[hash] == lease) {
		ht_tstamp[hash] = lease->next_tstamp;
		return;
	}
	
	// remove from the list
	Lease *prev = ht_tstamp[hash];
	Lease *ptr = ht_tstamp[hash]->next_tstamp;
	while (ptr) {
		if (ptr == lease) {
			prev->next_tstamp = lease->next_tstamp;
			return;
		}
		
		prev = ptr;
		ptr = ptr->next_tstamp;
	}	
	
	ASSERT(0);
}


//********************************************************
// public interface
//********************************************************


Lease *lease_find_mac(const unsigned char *mac) {
	unsigned hash = hash_mac(mac);
	if (ht_mac[hash] == NULL)
		return NULL;
	
	Lease *ptr = ht_mac[hash];
	while (ptr) {
		if (!compare_mac(ptr->mac, mac))
			return ptr;
		ptr = ptr->next_mac;
		
	}
	
	return NULL;
}

Lease *lease_find_ip(const uint32_t ip) {
	unsigned hash = hash_ip(ip);
	if (ht_ip[hash] == NULL)
		return NULL;
	
	Lease *ptr = ht_ip[hash];
	while (ptr) {
		if (ptr->ip == ip)
			return ptr;
		ptr = ptr->next_ip;
		
	}
	
	return NULL;
}


// tstamp is the expiring timestamp
Lease *lease_add(const unsigned char *mac, const uint32_t ip, const time_t tstamp) {
	ASSERT(mac);
	
	// check if the lease is already in the  database
	if (lease_find_mac(mac))
		return NULL;
	if (lease_find_ip(ip))
		return NULL;

	time_t now = time(NULL);
	if (tstamp < now) {
		ASSERT(0);
		return NULL;
	}

	// create the new lease
	Lease *lease = malloc(sizeof(Lease));
	if (lease == NULL)
		return NULL;
	
	memset(lease, 0, sizeof(Lease));
	memcpy(lease->mac, mac, 6);
	lease->ip = ip;
	lease->tstamp = tstamp;
	
	// add the lease to the database
	add_mac(lease);
	add_ip(lease);
	add_tstamp(lease);
	
	return lease;
}

void lease_delete(Lease *lease) {
	ASSERT(lease);
	
	remove_mac(lease);
	remove_ip(lease);
	remove_tstamp(lease);
}

void lease_renew(Lease *lease, time_t tstamp) {
	ASSERT(lease);
	
	// remove the lease from the timestamp list
	remove_tstamp(lease);
	// update tstamp
	lease->tstamp = tstamp;
	// add the lease back in the timestamp list
	add_tstamp(lease);
}

// walk the lease; if f returns 1 exit the function
void lease_walk(int (*f)(Lease *, void *), void *arg) {
	int i;
	for (i = 0; i < LEASE_MAC_HASH_MAX; i++) {
		Lease *ptr = ht_mac[i];
		if (ptr == NULL)
			continue;
		
		while (ptr) {
			if (f(ptr, arg))
				return;
			ptr = ptr->next_mac;
		}
	}
}

void lease_timeout(void) {
	time_t now = time(NULL);

	unsigned hash = hash_tstamp(now);
	Lease *ptr = ht_tstamp[hash];
	
	// walk the list and delete the all leases with a timestamp equal or smaller than  our timestamp
	while (ptr) {
		Lease *next = ptr->next_tstamp;
		
		if (ptr->tstamp <= now) {
			lease_delete(ptr);
			free(ptr);
		}
		
		ptr = next;
	}
	
}

void lease_clear_all(void) {
	int i;
	for (i = 0; i < LEASE_TSTAMP_HASH_MAX; i++) {
		Lease *ptr = ht_tstamp[i];
		while (ptr) {
			lease_delete(ptr);
			free(ptr);
			Lease *next = ptr->next_tstamp;
			
			ptr = next;
		}
	}
	
	lease_file_update();
}

typedef struct {
	uint32_t ip;		// ip address
	uint32_t mask;		// network mask for this ip address
	unsigned char mac[6];	// mac address
	time_t tstamp;		// system time when the lease expires
	uint32_t lease;		// lease time in seconds
} LeaseStore;

// store lease in the lease file
void lease_store(Lease *lease) {
	ASSERT(lease);
	
	int f = open(DHCP_LEASE_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IWUSR | S_IRUSR);
	if (f == -1) {
		// error recovery is not critical in this case
		return;
	}

	LeaseStore ls;
	ls.ip = lease->ip;
	ls.mask = lease->mask;
	memcpy(ls.mac, lease->mac, 6);
	ls.tstamp = lease->tstamp;
	ls.lease = lease->lease;
	
	ssize_t v = write(f, &ls, sizeof(ls));
	if (v != sizeof(ls))
		ASSERT(0);
	
	close(f);
}

static int lupdate(Lease *lease, void *arg) {
	int f = *((int *) arg);
	LeaseStore ls;
	ls.ip = lease->ip;
	ls.mask = lease->mask;
	memcpy(ls.mac, lease->mac, 6);
	ls.tstamp = lease->tstamp;
	ls.lease = lease->lease;
	
	ssize_t v = write(f, &ls, sizeof(ls));
	if (v != sizeof(ls))
		ASSERT(0);
	return 0;
}

void lease_file_update(void) {
	int f = open(DHCP_LEASE_FILE_TMP, O_CREAT | O_APPEND | O_WRONLY, S_IWUSR | S_IRUSR);
	if (f == -1) {
		// error recovery is not critical in this case
		return;
	}

	lease_walk(lupdate, &f);
	close(f);

	char cmd[200];
	sprintf(cmd, "mv %s %s", DHCP_LEASE_FILE_TMP, DHCP_LEASE_FILE);
	int v = system(cmd);
	if (v == -1)
		ASSERT(0);
}

// initialize hash tables and read current leases
void lease_init(void) {
	// init hash tables
	memset(ht_mac, 0, sizeof(ht_mac));
	memset(ht_ip, 0, sizeof(ht_ip));
	memset(ht_tstamp, 0, sizeof(ht_tstamp));
	
	// read the lease file
	int f = open(DHCP_LEASE_FILE, O_RDONLY);
	if (f == -1) {
		return;
	}

	time_t now = time(NULL);
	LeaseStore ls;
	while (read(f, &ls, sizeof(ls)) == sizeof(ls)) {
		if (ls.tstamp <= now)
			continue;
		
		// delete an existing lease
		Lease *lease = lease_find_ip(ls.ip);
		if (lease)	
			lease_delete(lease);
		
		// add the new lease
		lease = lease_add(ls.mac, ls.ip, ls.tstamp);
		if (lease) {
			lease->mask = ls.mask;
			lease->lease = ls.lease;
		}
		printf("DHCP: set DHCP lease %d.%d.%d.%d\n", RCP_PRINT_IP(lease->ip)); 		
	}
	close(f);	
	
	lease_file_update();
}
