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
#ifndef HDCP_LEASE_H
#define HDCP_LEASE_H
#include <time.h>
#include <stdint.h>

#define DHCP_OFFER_LEASE_TIME  20 // time for a temporary offer lease
#define DHCP_LEASE_FILE "/home/rcp/leases"
#define DHCP_LEASE_FILE_TMP "/home/rcp/leases-tmp"

typedef struct lease_t {
	struct lease_t *next_mac;		// linked list based on mac addresses
	struct lease_t *next_ip;		// linked list based on ip addresses
	struct lease_t *next_tstamp;	// linked list based on time stamps

	uint32_t ip;		// ip address
	uint32_t mask;		// network mask for this ip address
	unsigned char mac[6];	// mac address
	time_t tstamp;		// system time when the lease expires
	uint32_t lease;		// lease time in seconds

	// status variables
	uint32_t tid;	// the ip address was offered, waiting for a request with this tid
			// a value of 0 here means the lease was successfully assigned
			// the value is stored in network format
	int declined;	// the lease was declined by the client, or the lease was released


} Lease;


#define LEASE_MAC_HASH_MAX 4096
#define LEASE_MAC_HASH_MASK 0xfff
static inline unsigned hash_mac(const unsigned char *mac) {
	uint8_t msb = mac[0] ^ mac[2] ^ mac[4];
	uint8_t lsb = mac[1] ^ mac[3] ^ mac[5];
	uint16_t hash = (((uint16_t) msb) << 8) + (uint16_t) lsb;
	return (unsigned) (hash & LEASE_MAC_HASH_MASK);
}

// return 0 if mac1 and mac2 are equal
static inline int compare_mac(const unsigned char *mac1, const unsigned char *mac2) {
	if (mac1[0] == mac2[0] &&
	    mac1[1] == mac2[1] &&
	    mac1[2] == mac2[2] &&
	    mac1[3] == mac2[3] &&
	    mac1[4] == mac2[4] &&
	    mac1[5] == mac2[5])
	    	return 0;

	return 1;
}

#define LEASE_IP_HASH_MAX 4096
#define LEASE_IP_HASH_MASK 0xfff
// derived from Bob Jenkins One-at-a-Time hash
static inline unsigned hash_ip(uint32_t ip) {
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
	return h & LEASE_IP_HASH_MASK;
}

#define LEASE_TSTAMP_HASH_MAX 256
#define LEASE_TSTAMP_HASH_MASK 0xff
static inline unsigned hash_tstamp(time_t tstamp) {
	return (unsigned) (tstamp & LEASE_TSTAMP_HASH_MASK);
}

void lease_init(void);
Lease *lease_find_mac(const unsigned char *mac);
Lease *lease_find_ip(const uint32_t ip);
Lease *lease_add(const unsigned char *mac, const uint32_t ip, const time_t tstamp);
void lease_renew(Lease *lease, time_t tstamp);
void lease_delete(Lease *lease);
void lease_walk(int (*f)(Lease *, void *), void *arg);
void lease_timeout(void);
void lease_file_update(void);
void lease_store(Lease *lease);
void lease_clear_all();
#endif
