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
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "cli.h"
#include "librcp_interface.h"

static int is_mac_allzero(unsigned char *mac) {
	int i;
	for (i = 0; i < 6; i++)
		if (mac[i] != 0)
			return 0;
	
	return 1;
}


static void print_if_detail(int index, RcpIfCounts *ifcounts) {
	RcpInterface *intf = &shm->config.intf[index];
	ASSERT(intf->name[0] != '\0');
	
	cliPrintLine("Interface %s admin %s, link %s\n",
		intf->name,
		(intf->admin_up)? "UP": "DOWN",
		(intf->link_up)? "UP": "DOWN");

	if (!is_mac_allzero(intf->mac))
		cliPrintLine("\tMAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
			RCP_PRINT_MAC(intf->mac));

	if (intf->type == IF_VLAN) 
		cliPrintLine("\tVLAN ID %u, parent interface %s\n",
			intf->vlan_id,  shm->config.intf[intf->vlan_parent].name);
		
	if (intf->ip != 0 && intf->mask != 0)
		cliPrintLine("\tIP address %d.%d.%d.%d, mask %d.%d.%d.%d\n",
			RCP_PRINT_IP(intf->ip),
			RCP_PRINT_IP(intf->mask));
	
	if (intf->mtu)
		cliPrintLine("\tMTU %d\n", intf->mtu);

	if (!rcpInterfaceVirtual(intf->name)) {
		// find current counts
		RcpIfCounts *ptr = ifcounts;
		RcpIfCounts *found = NULL;
		while (ptr) {
			if (strcmp(intf->name, ptr->name) == 0) {
				found = ptr;
				break;
			}
			ptr = ptr->next;
		}
		
		if (found) {
			cliPrintLine("\tTX: %u packets, %u bytes, %u errors, %u dropped\n",
				found->tx_packets, found->tx_bytes, found->tx_errors, found->tx_dropped);
			cliPrintLine("\tRX: %u packets, %u bytes, %u errors, %u dropped\n",
				found->rx_packets, found->rx_bytes, found->rx_errors, found->rx_dropped);
		}
	}
	cliPrintLine("\n");
}

int cliShowInterfaceCmd(CliMode mode, int argc, char **argv) {
	int i;

	if (argc == 3) {
		// update statistics
		RcpIfCounts *ifcounts = rcpGetInterfaceCounts();

		// print details
		for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->name[0] == '\0')
				continue;
			print_if_detail(i, ifcounts);
		}
	}
	else {
		// list the interfaces
		cliPrintLine("%-16.16s %-8.8s     %-20.20s    %s\n",
			"Interface", "Type", "IP", "Status (admin/link)");
		
		for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->name[0] == '\0')
				continue;
				
			char *type = "";
			if (intf->type == IF_ETHERNET)
				type = "ethernet";
			else if (intf->type == IF_LOOPBACK)
				type = "loopback";
			if (intf->type == IF_BRIDGE)
				type = "bridge";
			if (intf->type == IF_VLAN)
				type = "VLAN";
			
			char ip[30];
			sprintf(ip, "%d.%d.%d.%d/%d",
				RCP_PRINT_IP(intf->ip), mask2bits(intf->mask));
			
			
			if (intf->type == IF_LOOPBACK)			
				cliPrintLine("%-16.16s %-8.8s     %-20.20s    %s/%s\n",
					intf->name,
					type,
					ip,
					"UP", "UP");
			else
				cliPrintLine("%-16.16s %-8.8s     %-20.20s    %s/%s\n",
					intf->name,
					type,
					ip,
					(intf->admin_up)? "UP": "DOWN",
					(intf->link_up)? "UP": "DOWN");
		}
	}
	
	return 0;
}
