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
#include "librcp.h"

RcpInterface *rcpFindInterface(RcpShm *shm, uint32_t ip) {
	ASSERT(shm != NULL);
	if (ip == 0)
		return NULL;
	
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (*shm->config.intf[i].name == '\0')
			continue;
		if (shm->config.intf[i].ip == ip)
			return &shm->config.intf[i];
	}
	
	return NULL;
}

RcpInterface *rcpFindInterfaceByName(RcpShm *shm, const char *name) {
	ASSERT(shm != NULL);
	if (name == NULL)
		return NULL;
		
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (*shm->config.intf[i].name == '\0')
			continue;
		if (strcmp(shm->config.intf[i].name, name) == 0)
			return &shm->config.intf[i];
	}
	
	return NULL;
}

 // longest prefix match
 RcpInterface *rcpFindInterfaceByLPM(RcpShm *shm, uint32_t ip) {
	ASSERT(shm != NULL);
	if (ip == 0)
		return NULL;
		
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (*shm->config.intf[i].name == '\0')
			continue;
		if (shm->config.intf[i].type == IF_LOOPBACK)
			continue;
		if (shm->config.intf[i].mask == 0)
			continue;

		if ((shm->config.intf[i].ip & shm->config.intf[i].mask) == (ip & shm->config.intf[i].mask))
			return &shm->config.intf[i];
	}
	
	return NULL;
}

RcpInterface *rcpFindInterfaceByKIndex(RcpShm *shm, int kindex) {
	ASSERT(shm != NULL);
	ASSERT(kindex != 0);

	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (*shm->config.intf[i].name == '\0')
			continue;
		if (shm->config.intf[i].kindex == kindex)
			return &shm->config.intf[i];
	}
	
	return NULL;
}

// is this a virtual interface?
int rcpInterfaceVirtual(const char *ifname) {
	if (strncmp(ifname, "lo:", 3) == 0)
		return 1;
	
	return 0;
}


#define MAXBUF 1000
RcpIfCounts *rcpGetInterfaceCounts(void) {
	RcpIfCounts *rv = NULL;
	FILE *fp = fopen("/proc/net/dev", "r");
	if (fp == NULL)
		return NULL;

	// skip the first two lines
	char line[MAXBUF];
	char *v = fgets(line, MAXBUF, fp);
	v = fgets(line, MAXBUF, fp);
	(void) v;
	while(fgets(line, MAXBUF, fp)) {
		RcpIfCounts *ptr = malloc(sizeof(RcpIfCounts));
		if (ptr == NULL)
			goto errout;
		memset(ptr, 0, sizeof(RcpIfCounts));
		
		ptr->next = rv,
		rv = ptr;
		
		// extract the name
		char *ptr1 = strchr(line, ':');
		if (!ptr1)
			goto errout;
		*ptr1 = '\0';
		char *ptr2 = line;
		while (*ptr2 == ' ' || *ptr2 == '\t')
			ptr2++;
		strcpy(ptr->name, ptr2);

		// extract packet counts
		ptr1++;
		sscanf(ptr1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			&ptr->rx_bytes,
			&ptr->rx_packets,
			&ptr->rx_errors,
			&ptr->rx_dropped,
			&ptr->rx_fifo,
			&ptr->rx_frame,
			&ptr->rx_compressed,
			&ptr->rx_multicast,
			&ptr->tx_bytes,
			&ptr->tx_packets,
			&ptr->tx_errors,
			&ptr->tx_dropped,
			&ptr->tx_fifo,
			&ptr->tx_colls,
			&ptr->tx_carrierfifo,
			&ptr->tx_compressed);
	}
	return rv;
	
errout:
	// clean allocated memory
	while (rv) {
		RcpIfCounts *ptr = rv->next;
		free(rv);
		rv = ptr;
	}
	
	return NULL;	
}
