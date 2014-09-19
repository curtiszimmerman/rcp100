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

static int count_dhcp_servers(void) {
	int cnt = 0;
	int i;
	RcpDhcprServer *ptr;

	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cnt++;
	}

	return cnt;
}

static int count_dhcp_networks(void) {
	int cnt = 0;
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cnt++;
	}

	return cnt;
}

static int count_dhcp_hosts(void) {
	int cnt = 0;
	int i;
	RcpDhcpsHost *ptr;

	for (i = 0, ptr = shm->config.dhcps_host; i < RCP_DHCP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		cnt++;
	}

	return cnt;
}

void configDhcpIf(FILE *fp, int no_passwords) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];

			if (intf->type == IF_LOOPBACK)
				continue;
			if (intf->dhcp_relay == 0)
				continue;
				
			if (intf->type == IF_BRIDGE)
				fprintf(fp, "interface bridge %s\n", intf->name);
			else if (intf->type == IF_ETHERNET)
				fprintf(fp, "interface ehternet %s\n", intf->name);
			else if (intf->type == IF_VLAN)
				fprintf(fp, "interface vlan %s\n", intf->name);
			fprintf(fp, "  ip dhcp relay enable\n");
			fprintf(fp, "  exit\n");
		}
	}			
}

static void configDhcpRelay(FILE *fp, int no_passwords) {
	if (shm->config.dhcpr_max_hops == RCP_DHCPR_DEFAULT_MAX_HOPS &&
	    shm->config.dhcpr_service_delay == RCP_DHCPR_DEFAULT_SERVICE_DELAY &&
	    count_dhcp_servers() == 0)
		return;
		
	fprintf(fp, "ip dhcp relay\n");
	int i;
	RcpDhcprServer *ptr;

	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ptr->server_ip[1] == 0)
			fprintf(fp, "  server-group %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->server_ip[0]));
		else if (ptr->server_ip[2] == 0)
			fprintf(fp, "  server-group %d.%d.%d.%d %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->server_ip[0]), RCP_PRINT_IP(ptr->server_ip[1]));
		else if (ptr->server_ip[3] == 0)
			fprintf(fp, "  server-group %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->server_ip[0]), RCP_PRINT_IP(ptr->server_ip[1]),
				RCP_PRINT_IP(ptr->server_ip[2]));
		else
			fprintf(fp, "  server-group %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->server_ip[0]), RCP_PRINT_IP(ptr->server_ip[1]),
				RCP_PRINT_IP(ptr->server_ip[2]), RCP_PRINT_IP(ptr->server_ip[3]));
	}
	
	if (shm->config.dhcpr_max_hops != RCP_DHCPR_DEFAULT_MAX_HOPS)
		fprintf(fp, "  max-hops %d\n", shm->config.dhcpr_max_hops);

	if (shm->config.dhcpr_service_delay != RCP_DHCPR_DEFAULT_SERVICE_DELAY)
		fprintf(fp, "  service-delay %d\n", shm->config.dhcpr_service_delay);

	if (shm->config.dhcpr_option82)
		fprintf(fp, "  option\n");

	fprintf(fp, "!\n");
}

static void configDhcpServer(FILE *fp, int no_passwords) {
	int i;

	if (shm->config.dhcps_enable == 0 &&
	    count_dhcp_networks() == 0 &&
	    count_dhcp_hosts() == 0 &&
	    shm->config.dhcps_dns1 == 0 &&
	    shm->config.dhcps_dns2 == 0 &&
	    shm->config.dhcps_ntp1 == 0 &&
	    shm->config.dhcps_ntp2 == 0)
		return;
		
	
	if (shm->config.dhcps_enable)
		fprintf(fp, "service dhcp\n");
	
	fprintf(fp, "ip dhcp server\n");


	if (*shm->config.dhcps_domain_name != '\0')
		fprintf(fp, "  domain-name %s\n", shm->config.dhcps_domain_name);


	if (shm->config.dhcps_dns1 && !shm->config.dhcps_dns2)
		fprintf(fp, "  dns-server %d.%d.%d.%d\n",
			RCP_PRINT_IP(shm->config.dhcps_dns1));
	if (shm->config.dhcps_dns1 && shm->config.dhcps_dns2)
		fprintf(fp, "  dns-server %d.%d.%d.%d %d.%d.%d.%d\n",
			RCP_PRINT_IP(shm->config.dhcps_dns1),
			RCP_PRINT_IP(shm->config.dhcps_dns2));

	if (shm->config.dhcps_ntp1 && !shm->config.dhcps_ntp2)
		fprintf(fp, "  ntp-server %d.%d.%d.%d\n",
			RCP_PRINT_IP(shm->config.dhcps_ntp1));
	if (shm->config.dhcps_ntp1 && shm->config.dhcps_ntp2)
		fprintf(fp, "  ntp-server %d.%d.%d.%d %d.%d.%d.%d\n",
			RCP_PRINT_IP(shm->config.dhcps_ntp1),
			RCP_PRINT_IP(shm->config.dhcps_ntp2));


	// print hosts
	RcpDhcpsHost *hptr;
	for (i = 0, hptr = shm->config.dhcps_host; i < RCP_DHCP_HOST_LIMIT; i++, hptr++) {
		if (!hptr->valid)
			continue;
		fprintf(fp, "  host %d.%d.%d.%d %02x:%02x:%02x:%02x:%02x:%02x\n",
			RCP_PRINT_IP(hptr->ip), RCP_PRINT_MAC(hptr->mac));
	}

	// print networks
	RcpDhcpsNetwork *ptr;
	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		fprintf(fp, "  network %s\n", ptr->name);
		
		if (ptr->range_start && ptr->range_end)
			fprintf(fp, "    range %d.%d.%d.%d %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->range_start),
				RCP_PRINT_IP(ptr->range_end));


		if (ptr->gw1 && !ptr->gw2)
			fprintf(fp, "    default-router %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->gw1));
		else if (ptr->gw1 && ptr->gw2)
			fprintf(fp, "    default-router %d.%d.%d.%d %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->gw1),
				RCP_PRINT_IP(ptr->gw2));
		
		if (ptr->lease_days != RCP_DHCP_LEASE_DAY_DEFAULT ||
		    ptr->lease_hours != RCP_DHCP_LEASE_HOUR_DEFAULT ||
		    ptr->lease_minutes != RCP_DHCP_LEASE_MINUTE_DEFAULT)
		    	fprintf(fp, "    lease %u %u %u\n",
		    		ptr->lease_days,
		    		ptr->lease_hours,
		    		ptr->lease_minutes);
		fprintf(fp, "  !\n");
	}
	fprintf(fp, "!\n");
}

void configDhcp(FILE *fp, int no_passwords) {
	configDhcpRelay(fp, no_passwords);
	configDhcpServer(fp, no_passwords);
}
