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
#include "rip.h"

// find the corresponding network for a given interface, NULL if not found
RcpRipPartner *find_network_for_interface(RcpInterface *intf) {
	ASSERT(intf != NULL);
	if (intf->ip == 0)
		return NULL;

	// walk through configured network
	RcpRipPartner *net;
	int i;
	
	for (i = 0, net = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, net++) {
		if (!net->valid)
			continue;

		// first check the length of the two mask
		if (intf->mask >= net->mask) {
			// if the prefix matches, we've found it
			if ((intf->ip & net->mask) == (net->ip & net->mask))
				return net;
		}
	}

	// we have no rip network for this interface
	return NULL;
}

// call this function periodically to update multicast group and rip networks
void rip_update_interfaces(void) {
	// walk through all interfaces and join or drop the multicast group
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK)
			continue;

		// if the address changed, maybe we need to re-join the multicast group
		if (intf->ip != intf->rip_multicast_ip || intf->mask != intf->rip_multicast_mask) {
			RcpRipPartner *net;

			// drop the multicast group
			if (intf->rip_multicast_ip) {
				rxmultidrop(rxsock, intf->rip_multicast_ip);
			 	intf->rip_multicast_ip = 0;
			 	intf->rip_multicast_mask = 0;
			 	intf->rip_timeout = 0;
				ripdb_delete_interface(intf);
		 	}
			
			 // if the address was deleted do nothing
			 if (intf->ip == 0)
			 	continue;
			 
			 // add the multicast group
			 else if (intf->link_up && (net = find_network_for_interface(intf)) != NULL) {
			 	rxmultijoin(rxsock, intf->ip);
			 	intf->rip_multicast_ip = intf->ip; 
			 	intf->rip_multicast_mask = intf->mask;
			 	intf->rip_timeout = 1;	// force an update in one second
				// add route
				if (ripdb_add(RCP_ROUTE_RIP_NETWORK,
					    intf->ip & intf->mask,
					    intf->mask,
					    0,
					    1,
					    0,
					    intf))
					   ASSERT(0);
				// send a rip request		 	
			 	txrequest(intf, net);
			 } 
		}
		// interface down for configured rip network
		else if (intf->link_up == 0 && intf->rip_multicast_ip) {
			rxmultidrop(rxsock, intf->rip_multicast_ip);
		 	intf->rip_multicast_ip = 0;
		 	intf->rip_multicast_mask = 0;
	 		intf->rip_timeout = 0;
			ripdb_delete_interface(intf);
		}
		// a configured rip network disappeared
		else if (intf->link_up && intf->rip_multicast_ip) {
			RcpRipPartner *net =  find_network_for_interface(intf);
			if (net == NULL) {
				rxmultidrop(rxsock, intf->rip_multicast_ip);
			 	intf->rip_multicast_ip = 0;
			 	intf->rip_multicast_mask = 0;
		 		intf->rip_timeout = 0;
				ripdb_delete_interface(intf);
			}
		}
	}		
}


void rip_send_timeout_updates(void) {
	// walk through all interfaces and send updates
	RcpRipPartner *net;
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK)
			continue;
			
		net = find_network_for_interface(intf);
		if (net == NULL)
			continue;

		if (intf->rip_multicast_ip != 0) {
			if (--intf->rip_timeout <= 0) {
				if (intf->link_up)
					txresponse(intf, 0, net); // sending the full database
				if (shm->config.rip_update_timer > 15)
					intf->rip_timeout = shm->config.rip_update_timer - 5 + (rand() % 10);
				else
					intf->rip_timeout = shm->config.rip_update_timer;
			
			}
		}
	}

	// send responses for all neighbors
	for (i = 0, net = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, net++) {
		if (!net->valid)
			continue;

		if (--net->timeout <= 0) {
			RcpInterface *intf = rcpFindInterfaceByLPM(shm, net->ip);
			if (intf != NULL && intf->link_up)
				txresponse(intf, 0, net); // sending the full database

			// reconfigure the timeout
			if (shm->config.rip_update_timer > 15)
				net->timeout = shm->config.rip_update_timer - 5 + (rand() % 10);
			else
				net->timeout = shm->config.rip_update_timer;
			
		}
	}

	ripdb_clean_updates();
}

void rip_send_delta_updates(void) {
	// walk through all interfaces and send updates
	RcpRipPartner *net;
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK)
			continue;

		net = find_network_for_interface(intf);
		if (net == NULL)
			continue;
		
		if (intf->rip_multicast_ip != 0 && intf->link_up)
			txresponse(intf, 1, net); // sending the full database
	}


	
	// send updates for all neighbors
	for (i = 0, net = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, net++) {
		if (!net->valid)
			continue;

		RcpInterface *intf = rcpFindInterfaceByLPM(shm, net->ip);
		if (intf != NULL && intf->link_up)
			txresponse(intf, 1, net);
	}
}

void rip_update_connected(void) {
	if (shm->config.rip_redist_connected == 0)
		return;
		
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// unconfigured interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK || intf->ip == 0)
			continue;
		// link down
		if (intf->link_up == 0)
			continue;
		
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER, "redistributing connected route %d.%d.%d.%d/%d",
			RCP_PRINT_IP(intf->ip & intf->mask), mask2bits(intf->mask));
		// add the connected route
		ripdb_add(RCP_ROUTE_CONNECTED,
			    intf->ip & intf->mask,
			    intf->mask,
			    0,
			    shm->config.rip_redist_connected,
			    0,
			    intf);
	}		
}

void rip_update_connected_subnets(void) {
	if (shm->config.rip_redist_connected_subnets == 0) {
		ripdb_delete_type(RCP_ROUTE_CONNECTED_SUBNETS);
		return;
	}
		
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// unconfigured interfaces
		if (intf->name[0] == '\0' || intf->type != IF_LOOPBACK || intf->ip == 0)
			continue;
		
		if (!rcpInterfaceVirtual(intf->name))
			continue;
		
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER, "redistributing connected route %d.%d.%d.%d/%d",
			RCP_PRINT_IP(intf->ip & intf->mask), mask2bits(intf->mask));
		// add the connected route
		ripdb_add(RCP_ROUTE_CONNECTED_SUBNETS,
			    intf->ip & intf->mask,
			    intf->mask,
			    0,
			    1,
			    0,
			    intf);
	}		
}

void rip_update_static(void) {
	if (shm->config.rip_redist_static == 0) {
		ripdb_delete_type(RCP_ROUTE_STATIC);
		return;
	}

	int i;
	RcpStaticRoute *ptr;
	
	// go trough static route array and add/delete routes to/from rip database
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_ROUTE_STATIC) // we don't care about blackhole routes
			continue;
		
		uint32_t ip = ptr->ip;
		uint32_t mask = ptr->mask;
		uint32_t gw = ptr->gw;

		// distribution of default gateway is controlled by "default-information originate" command
		if (ip == 0 && mask == 0) {
			if (shm->config.rip_default_information == 0) {
				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER, "default route not redistributed");
				continue;
			}
		}

		// no interface configured or interface in shutdown mode
		RcpInterface *interface = rcpFindInterfaceByLPM(shm, gw);
		if (interface == NULL || interface->link_up == 0) {
			ripdb_delete(RCP_ROUTE_STATIC, ip, mask, gw, 0);
			continue;
		}
		ripdb_add(RCP_ROUTE_STATIC, ip, mask, gw,
			    shm->config.rip_redist_static, // metric, default 1
			    0,
			    interface);
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER, "redistributing %d.%d.%d.%d/%d via %d.%d.%d.%d",
			RCP_PRINT_IP(ip), mask2bits(mask), RCP_PRINT_IP(gw));
	}
}
