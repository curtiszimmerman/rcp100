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

RcpOspfArea *get_shm_area(uint32_t area_id) {
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		if (shm->config.ospf_area[i].valid && shm->config.ospf_area[i].area_id == area_id)
			return &shm->config.ospf_area[i];
	}
	
	return NULL;
}

RcpOspfArea *get_shm_area_empty(void) {
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		if (!shm->config.ospf_area[i].valid)
			return &shm->config.ospf_area[i];
	}
	
	return NULL;
}

RcpOspfNetwork *get_shm_network(uint32_t area_id, uint32_t ip, uint32_t mask) {
	RcpOspfArea *area = get_shm_area(area_id);
	if (area == NULL)
		return NULL;
	
	int i;
	for (i = 0; i < RCP_OSPF_NETWORK_LIMIT; i++) {
		RcpOspfNetwork *net = &area->network[i];
		if (net->valid && net->ip == ip && net->mask == mask)
			return net;
	}
	
	return NULL;
}

RcpOspfNetwork *get_shm_any_network(uint32_t ip, uint32_t mask) {
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		if (!shm->config.ospf_area[i].valid)
			continue;
			
		int j;
		for (j = 0; j < RCP_OSPF_NETWORK_LIMIT; j++) {
			RcpOspfNetwork *net = &shm->config.ospf_area[i].network[j];
			if (net->valid && net->ip == ip && net->mask == mask) {
				return net;
			}
		}
	}
	
	return NULL;
}

// test if the input subnet (ip,mask) can be matched with any
// network configured in ospf cli mode
RcpOspfNetwork *shm_match_network(uint32_t ip, uint32_t mask) {
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		if (!shm->config.ospf_area[i].valid)
			continue;
			
		int j;
		for (j = 0; j < RCP_OSPF_NETWORK_LIMIT; j++) {
			RcpOspfNetwork *net = &shm->config.ospf_area[i].network[j];

			if (!net->valid)
				continue;
			
			if (net->mask > mask) {
				continue;
			}
					
			if ((net->ip & net->mask) == (ip & net->mask)) {
				return net;
			}
		}
	}

	return NULL;	// no match
}

RcpOspfNetwork *get_shm_network_empty(uint32_t area_id) {
	RcpOspfArea *area = get_shm_area(area_id);
	if (area == NULL)
		return NULL;

	int i;
	for (i = 0; i < RCP_OSPF_NETWORK_LIMIT; i++) {
		RcpOspfNetwork *net = &area->network[i];
		if (!net->valid)
			return net;
	}
	
	return NULL;
}

void generate_router_id(void) {
	shm->config.ospf_router_id = 0;

	// walk through all interfaces and pick up the higher ip address
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK)
			continue;

		if (intf->ip > shm->config.ospf_router_id)
			shm->config.ospf_router_id = intf->ip;
	}
}

