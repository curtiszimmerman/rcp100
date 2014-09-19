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
#include "../../cli/exec/cli.h"

static int network_count(void) {
	RcpRipPartner *ptr;
	int i;
	int cnt = 0;
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cnt++;
	}

	return cnt;
}

static int neighbor_count(void) {
	RcpRipPartner *ptr;
	int i;
	int cnt = 0;
	for (i = 0, ptr = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cnt++;
	}

	return cnt;
}


void configRip(FILE *fp, int no_passwords) {
	int to_print = 0;

	// detect existent passive interface configuration
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].rip_passive_interface) {
			to_print = 1;
			break;
		}
	}

	// detect existent rip configuration
	if (network_count() != 0 ||
	     neighbor_count() != 0 ||
	     shm->config.rip_redist_static != 0 ||
	     shm->config.rip_redist_connected != 0 ||
	     shm->config.rip_redist_connected_subnets != 0 ||
	     shm->config.rip_redist_ospf != 0 ||
	     shm->config.rip_default_information != 0 ||
	     shm->config.rip_update_timer != RCP_RIP_DEFAULT_TIMER)
		to_print = 1;
	
	// exit if no rip configuration found
	if (to_print == 0)
		return;
		
	// print the mode command if we have any rip configuration
	fprintf(fp, "router rip\n");
			
	// print network configuration
	RcpRipPartner *ptr;
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		fprintf(fp, "  network %d.%d.%d.%d/%d\n",
			RCP_PRINT_IP(ptr->ip),
			mask2bits(ptr->mask));
	}

	// print neighbor configuration
	for (i = 0, ptr = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		fprintf(fp, "  neighbor %d.%d.%d.%d\n",
			RCP_PRINT_IP(ptr->ip));
	}
	
	// print passive interface configuration
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].rip_passive_interface)
			fprintf(fp, "  passive-interface %s\n", shm->config.intf[i].name);
	}

	// print default route configuration
	if (shm->config.rip_default_information)
		fprintf(fp, "  default-information originate\n");

	// print update timer
	if (shm->config.rip_update_timer != RCP_RIP_DEFAULT_TIMER)
		fprintf(fp, "  update-timer %u\n", shm->config.rip_update_timer);
	
	// print redistribute connected
	if (shm->config.rip_redist_connected == 1)
		fprintf(fp, "  redistribute connected\n");
	else if (shm->config.rip_redist_connected > 1)
		fprintf(fp, "  redistribute connected metric %d\n", shm->config.rip_redist_connected);

	// print redistribute connected subnets
	if (shm->config.rip_redist_connected_subnets == 1)
		fprintf(fp, "  redistribute connected loopback\n");

	// print redistribute static
	if (shm->config.rip_redist_static == 1)
		fprintf(fp, "  redistribute static\n");
	else if (shm->config.rip_redist_static > 1)
		fprintf(fp, "  redistribute static metric %d\n", shm->config.rip_redist_static);

	// print redistribute ospf
	if (shm->config.rip_redist_ospf == 1)
		fprintf(fp, "  redistribute ospf\n");
	else if (shm->config.rip_redist_ospf > 1)
		fprintf(fp, "  redistribute ospf metric %d\n", shm->config.rip_redist_ospf);

	// exiting rip mode		
	fprintf(fp, "!\n");
}
