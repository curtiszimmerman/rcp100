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
extern RcpPkt *pktout;

static void clear_rip_stats(void) {
	int i;
	RcpRipPartner *ptr;
	
	// clear network
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) {
		ptr->req_rx = 0;
		ptr->req_tx = 0;
		ptr->resp_rx = 0;
		ptr->resp_tx = 0;
	}
	
	// clear neighbors
	for (i = 0, ptr = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, ptr++) {
		ptr->req_rx = 0;
		ptr->req_tx = 0;
		ptr->resp_rx = 0;
		ptr->resp_tx = 0;
	}


}

// at startup, call this function before rip socket is created
void restart() {
	int i;
	
	// delete all rip routes from the routing table
	int v = system("/opt/rcp/bin/rtclean rip > /dev/null");
	if (v == -1)
		ASSERT(0);
		
	// clean rip database
	ripdb_delete_all();

	// clear multicast membership
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		
		// leave the multicast group if socket already present
		if (rxsock && intf->rip_multicast_ip)
			rxmultidrop(rxsock, intf->rip_multicast_ip);
			
		intf->rip_multicast_ip = 0;
		intf->rip_multicast_mask = 0;
	}
	
	// clean statistics
	clear_rip_stats();
	
	// update redistibution
	rip_update_connected();
	rip_update_static();

}

//****************************************************************
// Clear Commands
//****************************************************************
// delete dynamic routes
int cliClearIpRipCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	restart();
	
	// request redistributed routes in 5 seconds
	update_redist = 5;
	
	return 0;
}
