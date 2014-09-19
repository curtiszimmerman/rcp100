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

#define HMASK 0x3ff
static DnsCache *cache[HMASK + 1]; 	// 1024 entries
static int initialized = 0;

static void cache_init(void) {
	memset(cache, 0, sizeof(cache));
	initialized = 1;
}


static inline uint32_t hash(const char *str) {
	uint32_t hash = 5381;
	int c;

	while ((c = *str++) != '\0')
		hash = ((hash << 5) + hash) ^ c; // hash * 33 ^ c

	return hash & HMASK;
}

DnsCache *cache_find(const char *name, uint16_t type) {
	if (initialized == 0)
		cache_init();
	ASSERT(initialized == 1);

	// hash the name	
	unsigned h = hash(name);
	
	// walk the linked list
	DnsCache *ptr = cache[h];
	while (ptr != NULL) {
		if (ptr->type == type && strcmp(name, ptr->dns_name) == 0)
			return ptr;
			
		ptr = ptr->next;
	}
	
	return NULL;
}

void cache_add(DnsCache *ptr) {
	ASSERT(ptr != NULL);
	ASSERT(*ptr->dns_name != '\0');
	
	if (initialized == 0)
		cache_init();
	ASSERT(initialized == 1);

	// hash the name	
	unsigned h = hash(ptr->dns_name);

	// add the entry
	ptr->next = cache[h];
	cache[h] = ptr;
}

void cache_remove(DnsCache *cptr) {
	ASSERT(cptr != NULL);
	rcpDebug("cache_remove: %p\n", cptr);
	
	if (initialized == 0)
		cache_init();
	ASSERT(initialized == 1);

	// hash the name	
	unsigned h = hash(cptr->dns_name);

	// is this the first entry in the list?
	if (cache[h] == cptr) {
		// remove entry
		cache[h] = cptr->next;
		return;
	}
	
	// walk the list
	DnsCache *ptr = cache[h];
	while (ptr->next != NULL) {
		if (ptr->next == cptr) {
			// remove entry
			ptr->next = cptr->next;
			return;
		}
		
		ptr = ptr->next;
	}
	
	ASSERT(0);
}

void cache_print(FILE *fp) {
	int i;
	
	// print header
	fprintf(fp, "%-45.45s   %-16s   TTL\n", "Name", "Address");
	
	for (i = 0; i < HMASK + 1; i++) {
		DnsCache *ptr = cache[i];
		if (ptr == NULL)
			continue;
		
		// walk the list
		while (ptr != NULL) {
			char *name = (char *) dnspkt_get_real_name((unsigned char *) ptr->dns_name);
			if (name == NULL)
				continue;
				
			char ip[20];
			if (ptr->dns_ttl == DNS_STATIC_TTL) {
				sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->dns_ip));
				fprintf(fp, "%-45.45s   %-16s   static\n",
					name,
					ip);
			}
			else if (ptr->dns_cname) {
				sprintf(ip, "* CNAME %s *", (ptr->type == 1)? "IPv4": "IPv6");
				fprintf(fp, "%-45.45s   %-16s   %u\n",
					name,
					ip,
					ptr->dns_ttl);
			}
			else if (ptr->type == 1) {
				sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->dns_ip));
				fprintf(fp, "%-45.45s   %-16s   %u\n",
					name,
					ip,
					ptr->dns_ttl);
			}
			else if (ptr->type == 28) {
				sprintf(ip, "* IPv6 *");
				fprintf(fp, "%-45.45s   %-16s   %u\n",
					name,
					ip,
					ptr->dns_ttl);
			}
			
			ptr = ptr->next;
		}
	}
}

// this function is called every second to age the entries
void cache_timer(void) {
	int i;
	
	for (i = 0; i < HMASK + 1; i++) {
		DnsCache *ptr = cache[i];
		if (ptr == NULL)
			continue;
		
		// walk the list
		while (ptr != NULL) {
			DnsCache *next = ptr->next;
			rcpDebug("cache_timer: hash %d, ptr %p, next %p\n", i, ptr, next);
			
			// remove static entries if not marked
			if (ptr->dns_static) {
				if (ptr->marker == 0) {
					cache_remove(ptr);
					if (ptr->dns_cname) {
						free(ptr->dns_cname);
					}
					free(ptr);
				}
			}
			else {
				if (--ptr->dns_ttl <= 0) {
					cache_remove(ptr);
					if (ptr->dns_cname) {
						free(ptr->dns_cname);
					}
					free(ptr);
				}
			}
			ptr = next;
		}
	}
}

// clear all cached entries
void cache_clear(void) {
	int i;
	
	for (i = 0; i < HMASK + 1; i++) {
		DnsCache *ptr = cache[i];
		if (ptr == NULL)
			continue;
		
		// walk the list
		while (ptr != NULL) {
			DnsCache *next = ptr->next;
			if (ptr->dns_cname) {
				free(ptr->dns_cname);
			}
			free(ptr);			
			ptr = next;
		}
		cache[i] = NULL;
	}
}

static void cache_add_static(const char *name, uint32_t ip) {
	rcpDebug("adding static cache entry %s, %d.%d.%d.%d\n",
		name, RCP_PRINT_IP(ip));
	if (*name == '\0') {
		ASSERT(0);
		return;
	}
	
	// build the real name
	char rn[CLI_MAX_DOMAIN_NAME + 1];
	const char *ptrin = name;
	char *start = rn;
	char *ptrout = rn + 1;
	int cnt = 0;
	while (*ptrin != '\0') {
		if (*ptrin != '.') {
			// copy the character and update the counter
			cnt++;
			*ptrout = *ptrin;
			ptrout++;
			ptrin++;
		}
		else {
			// set start position and reset counter
			*start = cnt;
			cnt = 0;
			start = ptrout;
			ptrin++;
			ptrout++;
		}
	}
	*start = cnt;
	*ptrout = '\0';
			
	// if an entry is already present, make the entry static
	DnsCache *dc = cache_find(rn, 1);
	if (dc != NULL) {
		dc->dns_flags = DNS_STATIC_FLAGS;
		dc->dns_ttl = DNS_STATIC_TTL;
		dc->dns_ip = ip;
		dc->dns_static = 1;
		dc->marker = 1;
		dc->type = 1;
		if (dc->dns_cname) {
			free(dc->dns_cname);
			dc->dns_cname_len = 0;
		}

		return;
	}
	
	// allocate a new cache entry
	dc = malloc(sizeof(DnsCache));
	if (dc == NULL) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_ERR, RLOG_FC_DNS,
			"cannot allocate memory to store the cache entry");
		return;
	}
	memset(dc, 0, sizeof(DnsCache));

	// fill in the data
	strcpy(dc->dns_name, rn);
	dc->dns_flags = DNS_STATIC_FLAGS;
	dc->dns_ttl = DNS_STATIC_TTL;
	dc->dns_ip = ip;
	dc->dns_static = 1;
	dc->marker = 1;
	dc->type = 1;
	
	cache_add(dc);
}

void cache_update_static(void) {
	int i;
	
	// clear the markers
	for (i = 0; i < HMASK + 1; i++) {
		DnsCache *dc = cache[i];
		if (dc == NULL)
			continue;
		
		// walk the list
		while (dc != NULL) {
			dc->marker = 0;
			dc = dc->next;
		}
	}


	// add static entries found in shared memory, mark existing and new added entries
	RcpIpHost *ptr;
	for (i = 0, ptr = &shm->config.ip_host[0]; i < RCP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cache_add_static(ptr->name, ptr->ip);
	}
	
	// unmarked entries will be removed on the next one second walk in cache_timer() function
}

