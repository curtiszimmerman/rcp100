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
extern RcpPkt *pktout;
static int is_configured = 0;

int range_is_configured(void) {
	return is_configured;
}

static int range_any_configured(void) {
	RcpOspfRange *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		return 1;
	}
	
	return 0;
}

static RcpOspfRange *range_find(uint32_t area, uint32_t ip, uint32_t mask) {
	RcpOspfRange *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		if (ptr->area == area && ptr->ip == ip && ptr->mask == mask)
			return ptr;
	}
	
	return NULL;
}

static RcpOspfRange *range_find_empty(void) {
	RcpOspfRange *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			return ptr;
	}
	
	return NULL;
}


RcpOspfRange *range_match(uint32_t area, uint32_t ip, uint32_t mask) {
	// summary address not configured?
	if (!is_configured)	
		return NULL;

	RcpOspfRange *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
		if (!ptr->valid || area != ptr->area)
			continue;
		
		if (ptr->mask > mask) {
			continue;
		}
					
		if ((ptr->ip & ptr->mask) == (ip & ptr->mask)) {
			return ptr;
		}
	}
	
	return NULL;
}

int cliRangeCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// maybe we need to generate a router_id
	if (!shm->config.ospf_router_id)
		generate_router_id();
	if (!shm->config.ospf_router_id) {
		strcpy(data, "Error: cannot set router id\n");
		return RCPERR;
	}

	// extract data
	int index = 0;
	int noform = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}
	
	// area id
	uint32_t area;
	sscanf(argv[index], "%u", &area);

	// cidr
	index += 2;
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[index], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// find an existing entry
	RcpOspfRange *found = range_find(area, ip, mask);
	if (noform) {
		// range configured?
		if (found == NULL)
			return 0;
		
		// clear shared memory
		memset(found, 0, sizeof(RcpOspfRange));

		is_configured = range_any_configured();
	}
	else {
		if (found) {
			// already configured
			if (argc == 5 && found->not_advertise == 0) {
				found->not_advertise = 1;
				spfTrigger();
			}
			if (argc != 5 && found->not_advertise == 1) {
				found->not_advertise = 0;
				spfTrigger();
			}

			return 0;
		}
		
		found = range_find_empty();
		if (!found) {
			strcpy(data, "Error: range not configured, limit reached\n");
			return RCPERR;
		}
		memset(found, 0, sizeof(RcpOspfRange));
		found->area = area;
		found->ip = ip;
		found->mask = mask;
		if (argc == 5)
			found->not_advertise = 1;
		found->valid = 1;
		is_configured = 1;
	}
	
	// trigger an spf calculation
	spfTrigger();

	return 0;
}

int cliDiscardRoute(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_discard_external = 0;
		shm->config.ospf_discard_internal = 0;
	}
	else {
		shm->config.ospf_discard_external = 1;
		shm->config.ospf_discard_internal = 1;
	}
	
	// trigger an spf calculation
	spfTrigger();

	return 0;	
}

int cliDiscardRouteExternal(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_discard_external = 0;
	}
	else {
		shm->config.ospf_discard_external = 1;
	}
	
	// trigger an spf calculation
	spfTrigger();

	return 0;	
}

int cliDiscardRouteInternal(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_discard_internal = 0;
	}
	else {
		shm->config.ospf_discard_internal = 1;
	}
	
	// trigger an spf calculation
	spfTrigger();

	return 0;	
}
