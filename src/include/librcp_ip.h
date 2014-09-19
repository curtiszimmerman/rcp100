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
#ifndef LIBRCP_IP_H
#define LIBRCP_IP_H
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

// udp port for sending logs
#define RCP_PORT_LOGGER 9998
// udp port for monitoring udp-based services
#define RCP_PORT_MONITOR_UDP 9999
// dns proxy port range for outgoing connections
#define RCP_PORT_DNS_START  10000
#define RCP_PORT_DNS_RANGE 0x7fff // 32k range
#define RCP_PORT_DNS_END (RCP_PORT_DNS_START + RCP_PORT_DNS_RANGE)

#define RCP_PRINT_MAC(A) \
((unsigned) (*(A)) & 0xff), ((unsigned) (*((A) + 1) & 0xff)), ((unsigned) (*((A) + 2) & 0xff)), \
((unsigned) (*((A) + 3)) & 0xff), ((unsigned) (*((A) + 4) & 0xff)), ((unsigned) (*((A) + 5)) & 0xff)

#define RCP_PRINT_IP(A) \
((int) (((A) >> 24) & 0xFF)),  ((int) (((A) >> 16) & 0xFF)), ((int) (((A) >> 8) & 0xFF)), ((int) ( (A) & 0xFF))

// return 1 if error
static inline int atocidr(const char *str, uint32_t *ip, uint32_t *mask) {
	unsigned a, b, c, d, e;

	// extract ip
	int rv = sscanf(str, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &e);
	if (rv != 5 || a > 255 || b > 255 || c > 255 || d > 255 || e > 32)
		return 1;
	*ip = a * 0x1000000 + b * 0x10000 + c * 0x100 + d;

	// extract mask
	uint32_t tmp;
	unsigned i;
	for (i = 0, *mask = 0, tmp = 0x80000000; i < e; i++, tmp >>= 1) {
		*mask |= tmp;
	}
	return 0;
}


static inline uint8_t mask2bits(uint32_t mask) {
	uint32_t tmp = 0x80000000;
	int i;
	uint8_t rv = 0;

	for (i = 0; i < 32; i++, tmp >>= 1) {
		if (tmp & mask)
			rv++;
		else
			break;
	}
	return rv;
}


static inline int atoip(const char *str, uint32_t *ip) {
	unsigned a, b, c, d;

	if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
		return 1;
		
	*ip = a * 0x1000000 + b * 0x10000 + c * 0x100 + d;
	return 0;
}


static inline int atomac(char *str, unsigned char macAddr[6]) {
	unsigned mac[6];

	if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6)
		return 1;

	int i;
	for (i = 0; i < 6; i++) {
		if (mac[i] > 0xff)
			return 1;
			
		macAddr[i] = (unsigned char) mac[i];
	}

	return 0;
}


// return 1 if the prefix is a loopback address
static inline int isLoopback(uint32_t prefix) {
	return ((((long int) (prefix)) & 0xff000000) == 0x7f000000);
}

// return 1 if the prefix is a multicast address
static inline int isMulticast(uint32_t prefix) {
	return IN_CLASSD(prefix);
}


// return 1 if the prefix is a broadcast address
static inline int isBroadcast(uint32_t prefix) {
	if (IN_CLASSA(prefix))
		return (prefix & 0x00ffffff) == 0x00ffffff;
	if (IN_CLASSB(prefix))
		return (prefix & 0x0000ffff) == 0x0000ffff;
	if (IN_CLASSC(prefix))
		return (prefix & 0x000000ff) == 0x000000ff;
	return 0;
}

// return 1 if the prefix is a broadcast address
static inline uint32_t classMask(uint32_t prefix) {
	if (IN_CLASSA(prefix))
		return IN_CLASSA_NET;
	if (IN_CLASSB(prefix))
		return IN_CLASSB_NET;
	if (IN_CLASSC(prefix))
		return IN_CLASSC_NET;
	if (IN_CLASSD(prefix))
		return 0xF0000000;
	return 0;
}

static inline int maskContiguous(uint32_t mask) {
	uint32_t m = 0x80000000;
	int zero;
	int i;
	for (i = 0, zero = 0; i < 32; i++, m >>= 1) {
		if (zero == 1 && (m & mask) == 0)
			return 0;
		if ((m & mask) == 1)
			zero = 1;
	}
	
	return 1;
}

#ifdef __cplusplus
}
#endif
#endif
