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
#include "acl.h"

static RcpForwarding *port_find(uint16_t port) {
	RcpForwarding *ptr;
	int i;

	for (i = 0, ptr = shm->config.port_forwarding; i < RCP_FORWARD_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (port == ptr->port )
			return ptr;

	}
	 
	return NULL;
}


static RcpForwarding *port_find_empty(void) {
	RcpForwarding *ptr;
	int i;

	for (i = 0, ptr = shm->config.port_forwarding; i < RCP_FORWARD_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}



int cliForwardingCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int noform = 0;
	int index = 4;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
	}
	
	// extract port
	uint16_t port;
	uint32_t tmp;
	sscanf(argv[index], "%u", &tmp);
	port = tmp;
	ASSERT(port >= 1 && port <= 65535);

	// destination	
	uint32_t dest_ip = 0;
	uint16_t dest_port = 0;
	if (!noform) {
		index += 2;
		if (atoip(argv[index], &dest_ip)) {
			sprintf(data, "Error: invalid IP address");
			return RCPERR;
		}
		index++;
		sscanf(argv[index], "%u", &tmp);
		dest_port = tmp;
		ASSERT(dest_port >= 1 && dest_port <= 65535);
	}
	
	// find an existing entry in masq table	
	RcpForwarding *found = port_find(port);
	
	if (noform) {
		if (!found)	
			return 0; // nothing to do
		found->valid = 0;
	}
	else {
		if (found) {
			// just copy over the destination
			found->dest_ip = dest_ip;
			found->dest_port = dest_port;
		}
		else {
			found = port_find_empty();
			if (!found) {
				sprintf(data, "Error: cannot create port forwarding entry, limit reached");
				return RCPERR;
			}
			memset(found, 0, sizeof(RcpForwarding));
			found->port = port;
			found->dest_ip = dest_ip;
			found->dest_port = dest_port;
			found->valid = 1;
		}
	}			
	
	// trigger netfilter configuration update
	netfilter_update = 2; 

	return 0;
}		

