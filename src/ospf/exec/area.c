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

static OspfArea *area_list = NULL;

// get the head of area linked list
OspfArea *areaGetList(void) {
	return area_list;
}

void neighborFree(OspfNeighbor *nb) {
	ASSERT(nb != NULL);
	free(nb);
}

static inline void networkFree(OspfNetwork *net) {
	ASSERT(net != NULL);
	
	OspfNeighbor *nb = net->neighbor;
	while (nb != NULL) {
		OspfNeighbor *tmp = nb->next;
		neighborFree(nb);
		nb = tmp;
	}
	
	free(net);
}

static inline void areaFree(OspfArea *area) {
	ASSERT(area != NULL);

	// free networks
	OspfNetwork *net = area->network;
	while (net != NULL) {
		OspfNetwork *tmp = net->next;
		networkFree(net);
		net = tmp;
	}
	
	// free area
	free(area);
}



//**********************************************
// area
//**********************************************
// count the number of areas in the list
int areaCount(void) {
	OspfArea *area = areaGetList();
	int rv = 0;
	while (area != NULL) {
		rv++;
		area = area->next;
	}
	
	return rv;
}

// find an area; returns NULL if not found
OspfArea *areaFind(uint32_t area_id) {
	OspfArea *ptr = area_list;
	
	while (ptr != NULL) {
		if (area_id == ptr->area_id)
			return ptr;
		ptr = ptr->next;
	}
	
	return NULL;

}

// find the area where this IP is located; returns NULL if error
OspfArea *areaFindAnyIp(uint32_t ip) {
	OspfArea *area = areaGetList();
	if (area == NULL)
		return NULL;

	while (area) {
		OspfNetwork *net = area->network;
		while (net != NULL) {
			if ((ip & net->mask) == (net->ip & net->mask))
				return area;
			net = net->next;
		}
		area = area->next;
	}
	
	return NULL;
}

// add an area; returns 1 if error, 0 if ok
int areaAdd(OspfArea *area) {
	TRACE_FUNCTION();
	ASSERT(area != NULL);
	
	if (areaFind(area->area_id))
		return 1;
	
	area->next = area_list;
	area_list = area;
	return 0;
}

// returns 1 if error, 0 if ok
int areaRemove(OspfArea *area) {
	TRACE_FUNCTION();
	ASSERT(area != NULL);
	if (area_list == NULL)
		return 1;
	if (areaFind(area->area_id) == NULL)
		return 1;

	// remove first element
	if (area_list == area) {
		area_list = area->next;
		areaFree(area);
		return 0;
	}
	
	OspfArea *ptr = area_list;
	while (ptr != NULL) {
		if (ptr->next == area) {
			ptr->next = area->next;
			areaFree(area);
			return 0;
		}
		ptr = ptr->next;
	}
	return 1;
}

//**********************************************
// network
//**********************************************
// count the number of networks in an area
int networkCount(uint32_t area_id) {
	OspfArea *area = areaFind(area_id);
	if (!area)
		return 0;
		
	OspfNetwork *net = area->network;
	int rv = 0;
	while (net) {
		rv++;
		net = net->next;
	}
	
	return rv;
}

// find a network; returns NULL if not found
OspfNetwork *networkFind(uint32_t area_id, uint32_t ip, uint32_t mask) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL)
		return NULL;

	OspfNetwork *net = area->network;
	while (net != NULL) {
		if (ip == net->ip && mask == net->mask)
			return net;
		net = net->next;
	}
	
	return NULL;
}

// find the network where this IP is located; returns NULL if error
OspfNetwork *networkFindIp(uint32_t area_id, uint32_t ip) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL)
		return NULL;

	OspfNetwork *net = area->network;
	while (net != NULL) {
		if ((ip & net->mask) == (net->ip & net->mask))
			return net;
		net = net->next;
	}
	
	return NULL;
}

// find the network where this IP is located; returns NULL if error
OspfNetwork *networkFindAnyIp(uint32_t ip) {
	OspfArea *area = areaGetList();
	if (area == NULL)
		return NULL;

	while (area) {
		OspfNetwork *net = area->network;
		while (net != NULL) {
			if ((ip & net->mask) == (net->ip & net->mask))
				return net;
			net = net->next;
		}
		area = area->next;
	}
	
	return NULL;
}

// add a network; returns 1 if error, 0 if ok
int networkAdd(uint32_t area_id, OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		rcpDebug("cannot find area\n");		
		return 1;
	}

	if (networkFind(area_id, net->ip, net->mask)) {
		rcpDebug("network already there\n");		
		return 1;
	}
	
	net->next = area->network;
	area->network = net;
	return 0;
}

// returns 1 if error, 0 if ok
int networkRemove(uint32_t area_id, OspfNetwork *net) {
	TRACE_FUNCTION();
	if (networkFind(area_id, net->ip, net->mask) == NULL)
		return 1;
	
	OspfArea *area = areaFind(area_id);
	if (area == NULL)
		return 1;

	// remove first element
	if (area->network == net) {
		area->network = net->next;
		networkFree(net);
		return 0;
	}
	
	OspfNetwork *ptr = area->network;
	while (ptr != NULL) {
		if (ptr->next == net) {
			ptr->next = net->next;
			networkFree(net);
			return 0;
		}
		ptr = ptr->next;
	}
	return 1;
}
	
//**********************************************
// neighbor
//**********************************************
int neighborCount(OspfNetwork *net) {
	int rv = 0;
	OspfNeighbor *nb = net->neighbor;

	while (nb) {
		rv++;
		nb = nb->next;
	}
	return rv;
}

// find a neighbor; returns NULL if error
OspfNeighbor *neighborFind(uint32_t area_id, uint32_t ip) {
	// find the network for this neighbor
	OspfNetwork *net = networkFindIp(area_id, ip);
	if (net == NULL)
		return NULL;
	
	OspfNeighbor *ptr = net->neighbor;
	while (ptr != NULL) {
		if (ip == ptr->ip)
			return ptr;
		ptr = ptr->next;
	}
	
	return NULL;
}

// add a neighbor; returns 1 if error, 0 if ok
int neighborAdd(uint32_t area_id, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(nb != NULL);
	
	if (neighborFind(area_id, nb->ip))
		return 1;
	
	// find the network for this neighbor
	OspfNetwork *net = networkFindIp(area_id, nb->ip);
	if (net == NULL)
		return 1;
	
	nb->next = net->neighbor;
	net->neighbor = nb;
	return 0;
}

//returns 1 if error, 0 if ok
int neighborRemove(uint32_t area_id, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(nb != NULL);
	
	OspfNeighbor *ptr = neighborFind(area_id, nb->ip);
	if (ptr == NULL)
		return 1;

	// find the network for this neighbor
	OspfNetwork *net = networkFindIp(area_id, nb->ip);
	if (net == NULL)
		return 1;

	// remove first element
	if (net->neighbor == nb) {
		net->neighbor = nb->next;
		neighborFree(nb);
		return 0;
	}
	
	ptr = net->neighbor;
	while (ptr != NULL) {
		if (ptr->next == nb) {
			ptr->next = nb->next;
			neighborFree(nb);
			return 0;
		}
		ptr = ptr->next;
	}
	
	return 0;
}

//returns 1 if error, 0 if ok
// the function is similar with neighborRemove, the only difference is neighborFree is not called
int neighborUnlink(uint32_t area_id, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(nb != NULL);
	
	OspfNeighbor *ptr = neighborFind(area_id, nb->ip);
	if (ptr == NULL)
		return 1;

	// find the network for this neighbor
	OspfNetwork *net = networkFindIp(area_id, nb->ip);
	if (net == NULL)
		return 1;

	// remove first element
	if (net->neighbor == nb) {
		net->neighbor = nb->next;
		return 0;
	}
	
	ptr = net->neighbor;
	while (ptr != NULL) {
		if (ptr->next == nb) {
			ptr->next = nb->next;
			return 0;
		}
		ptr = ptr->next;
	}
	
	return 0;
}

// router id to ip translation for neighbors
uint32_t neighborRid2Ip(uint32_t area_id, uint32_t rid) {
	OspfArea *area = areaFind(area_id);
	if (area == NULL) {
		ASSERT(0);
		return 0;
	}
	
	OspfNetwork *net = area->network;
	while (net != NULL) {
		OspfNeighbor *nb = net->neighbor;
		while (nb != NULL) {
			if (nb->router_id == rid)
				return nb->ip;
			nb = nb->next;
		}
		
		net = net->next;
	}
	return 0;
}
		
void  active_neighbor_update(void) {
	RcpActiveNeighbor *an = shm->stats.ospf_active;
	int ancnt = 0;
	RcpActiveNeighbor value;

	// lock data in shared memory
	shm->stats.ospf_active_locked = 1;
	
	OspfArea *area = areaGetList();
	while (area && ancnt < RCP_ACTIVE_NEIGHBOR_LIMIT) {
		memset(&value, 0, sizeof(RcpActiveNeighbor));
		value.valid = 1;
		value.area = area->area_id;
		
		OspfNetwork *net = area->network;
		while (net && ancnt < RCP_ACTIVE_NEIGHBOR_LIMIT) {
			value.network = net->ip & net->mask;
			value.netmask_cnt = mask2bits(net->mask);
			value.if_ip = net->ip;

			OspfNeighbor *nb = net->neighbor;
			while (nb && ancnt < RCP_ACTIVE_NEIGHBOR_LIMIT) {
				value.ip = nb->ip;
				value.router_id = nb->router_id;
				// apply new data
				memcpy(an, &value, sizeof(RcpActiveNeighbor));
				ancnt++;
				an++;
				
				nb = nb->next;
			}
			
			net = net->next;
		}
		
		area = area->next;
	}
	
	while (ancnt < RCP_ACTIVE_NEIGHBOR_LIMIT) {
		an->valid = 0;
		an++;
		ancnt++;	 
	}
	
	// update lsa counts
	shm->stats.ospf_external_lsa = lsa_count_external();
	shm->stats.ospf_originated_lsa = lsa_count_originated();
	shm->stats.ospf_is_asbr = is_asbr;
	shm->stats.ospf_is_abr = is_abr;

	// update interface stats
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0') {
			intf->stats.ospf_state[0] = '\0';
			intf->stats.ospf_dr = 0;
			intf->stats.ospf_bdr = 0;
			continue;
		}

		// find the network
		OspfNetwork *net = networkFindAnyIp(intf->ip);
		if (net == NULL) {
			intf->stats.ospf_state[0] = '\0';
			intf->stats.ospf_dr = 0;
			intf->stats.ospf_bdr = 0;
		}
		else {
			const char *nstate = netfsmState2String(net->state);
			if (nstate == NULL)
				intf->stats.ospf_state[0] = '\0';
			else
				strncpy(intf->stats.ospf_state, nstate, RCP_OSPF_NETSTATE_SIZE);
			intf->stats.ospf_dr = net->designated_router;
			intf->stats.ospf_bdr = net->backup_router;
		}
	}
	
	// unlock shared memory
	shm->stats.ospf_active_locked = 0;
}
