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
#include "dhcp.h"
#include "lease.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static RcpDhcpsNetwork *find_network(uint32_t ip, uint32_t mask) {
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip == ptr->ip && mask == ptr->mask)
			return ptr;
	}
	
	return NULL;
}

static RcpDhcpsNetwork *find_network_name(const char *name) {
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (strcmp(name, ptr->name) == 0)
			return ptr;
	}
	
	return NULL;
}

static RcpDhcpsNetwork *find_network_empty() {
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}


static RcpDhcpsHost *find_host(uint32_t ip) {
	int i;
	RcpDhcpsHost *ptr;

	for (i = 0, ptr = shm->config.dhcps_host; i < RCP_DHCP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip == ptr->ip)
			return ptr;
	}
	
	return NULL;
}

static RcpDhcpsHost *find_host_empty() {
	int i;
	RcpDhcpsHost *ptr;

	for (i = 0, ptr = shm->config.dhcps_host; i < RCP_DHCP_HOST_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}




//******************************************************************
// config mode
//******************************************************************
int cliNoIpDhcpServer(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	shm->config.dhcps_enable = 0;

	int i;
	RcpDhcpsNetwork *ptr;
	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		memset(ptr, 0, sizeof(RcpDhcpsNetwork));
	}

	shm->config.dhcps_dns1 = 0;
	shm->config.dhcps_dns2 = 0;
	shm->config.dhcps_ntp1 = 0;
	shm->config.dhcps_ntp2 = 0;
	shm->config.dhcps_domain_name[0] = '\0';

	return 0;
}

int cliIpDhcpServerCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_DHCP_SERVER;
	pktout->pkt.cli.mode_data  = NULL;

	return 0;
}

//******************************************************************
// server mode
//******************************************************************
int cliDhcpServerEnableCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		shm->config.dhcps_enable = 0;
	else
		shm->config.dhcps_enable = 1;
	return 0;
}

int cliDhcpServerNoNetwork(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	int i;
	RcpDhcpsNetwork *ptr;
	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		memset(ptr, 0, sizeof(RcpDhcpsNetwork));
	}

	return 0;
}

int cliDhcpServerNetwork(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	int noform = 0;
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}

	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[index], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// this should be a network address
	if (ip & (~mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// find an existing network
	RcpDhcpsNetwork *found = find_network(ip, mask);
	
	// process network
	if (noform) {
		if (found)
			memset(found, 0, sizeof(RcpDhcpsNetwork));
	}
	else {
		if (!found) {
			// create a new network
			found = find_network_empty();
			if (!found) {
				strcpy(data, "Error: network not configured, DHCP server network limit reached\n");
				return RCPERR;
			}
			memset(found, 0, sizeof(RcpDhcpsNetwork));
			found->valid = 1;
			found->ip = ip;
			found->mask = mask;
			sprintf(found->name, "%d.%d.%d.%d/%d",
				RCP_PRINT_IP(found->ip), mask2bits(found->mask));
			found->lease_days = RCP_DHCP_LEASE_DAY_DEFAULT ;
			found->lease_hours = RCP_DHCP_LEASE_HOUR_DEFAULT ;
			found->lease_minutes = RCP_DHCP_LEASE_MINUTE_DEFAULT ;
		}
		// set the new cli mode
		pktout->pkt.cli.mode  = CLIMODE_DHCP_NETWORK;
		pktout->pkt.cli.mode_data  = found->name;
	}
	
	return 0;
}

int cliDhcpServerDns(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.dhcps_dns1 = 0;
		shm->config.dhcps_dns2 = 0;
	}
	else {
		uint32_t dns1 = 0;
		uint32_t dns2 = 0;
		
		if (atoip(argv[1], &dns1)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		
		if (argc == 3) {
			if (atoip(argv[2], &dns2)) {
				strcpy(data, "Error: invalid IP address\n");
				return RCPERR;
			}
		}
		
		shm->config.dhcps_dns1 = dns1;
		shm->config.dhcps_dns2 = dns2;
	}
	
	return 0;
}

int cliDhcpServerNtp(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.dhcps_ntp1 = 0;
		shm->config.dhcps_ntp2 = 0;
	}
	else {
		uint32_t ntp1 = 0;
		uint32_t ntp2 = 0;
		
		if (atoip(argv[1], &ntp1)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		
		if (argc == 3) {
			if (atoip(argv[2], &ntp2)) {
				strcpy(data, "Error: invalid IP address\n");
				return RCPERR;
			}
		}
		
		shm->config.dhcps_ntp1 = ntp1;
		shm->config.dhcps_ntp2 = ntp2;
	}
	
	return 0;
}

int cliDhcpServerDomainName(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		shm->config.dhcps_domain_name[0] = '\0';
	else {
		strncpy((char *) shm->config.dhcps_domain_name, argv[1], CLI_MAX_DOMAIN_NAME);
		shm->config.dhcps_domain_name[CLI_MAX_DOMAIN_NAME] = '\0';
	}
	
	return 0;
}

int cliDhcpServerHost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	int noform = 0;
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}

	// extract ip
	uint32_t ip;
	if (atoip(argv[index], &ip)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	// extract mac
	uint8_t mac[6];
	if (noform == 0) {
		if (atomac(argv[index + 1], mac)) {
			strcpy(data, "Error: invalid MAC address\n");
			return RCPERR;
		}
	}

	// find an existing host
	RcpDhcpsHost *found = find_host(ip);
	
	// process host
	if (noform) {
		if (found)
			memset(found, 0, sizeof(RcpDhcpsHost));
	}
	else {
		if (!found) {
			// create a new host
			found = find_host_empty();
			if (!found) {
				strcpy(data, "Error: DHCP host not configured, limit reached\n");
				return RCPERR;
			}
		}
		
		memset(found, 0, sizeof(RcpDhcpsHost));
		found->valid = 1;
		found->ip = ip;
		memcpy(found->mac, mac,  6);
	}
	
	return 0;
}

//******************************************************************
// network mode
//******************************************************************
int cliDhcpServerLease(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	ASSERT(pktout->pkt.cli.mode_data != NULL);
	RcpDhcpsNetwork *found = find_network_name(pktout->pkt.cli.mode_data);
	if (!found) {
		strcpy(data, "Error: invalid network\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0) {
		found->lease_days = RCP_DHCP_LEASE_DAY_DEFAULT ;
		found->lease_hours = RCP_DHCP_LEASE_HOUR_DEFAULT ;
		found->lease_minutes = RCP_DHCP_LEASE_MINUTE_DEFAULT ;
	}
	else {
		int val;
		val = atoi(argv[1]);
		found->lease_days = (uint8_t) val;
		val = atoi(argv[2]);
		found->lease_hours = (uint8_t) val;
		val = atoi(argv[3]);
		found->lease_minutes = (uint8_t) val;
		
		// replace lease 0 0 0 with default
		if (found->lease_days == 0 &&
		    found->lease_hours == 0 &&
		    found->lease_minutes == 0) {
			found->lease_days = RCP_DHCP_LEASE_DAY_DEFAULT ;
			found->lease_hours = RCP_DHCP_LEASE_HOUR_DEFAULT ;
			found->lease_minutes = RCP_DHCP_LEASE_MINUTE_DEFAULT ;
		}
	}

	return 0;
}

int cliDhcpServerRouter(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	ASSERT(pktout->pkt.cli.mode_data != NULL);
	RcpDhcpsNetwork *found = find_network_name(pktout->pkt.cli.mode_data);
	if (!found) {
		strcpy(data, "Error: invalid network\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0) {
		found->gw1 = 0;
		found->gw2 = 0;
	}
	else {
		uint32_t gw1 = 0;
		uint32_t gw2 = 0;
		
		if (atoip(argv[1], &gw1)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		
		if (argc == 3) {
			if (atoip(argv[2], &gw2)) {
				strcpy(data, "Error: invalid IP address\n");
				return RCPERR;
			}
		}
		
		found->gw1 = gw1;
		found->gw2 = gw2;
	}
	
	return 0;
}

int cliDhcpServerRange(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	ASSERT(pktout->pkt.cli.mode_data != NULL);
	RcpDhcpsNetwork *found = find_network_name(pktout->pkt.cli.mode_data);
	if (!found) {
		strcpy(data, "Error: invalid network\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0) {
		found->range_start = 0;
		found->range_end = 0;
	}
	else {
		uint32_t r1;
		uint32_t r2;
		
		if (atoip(argv[1], &r1)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		if (atoip(argv[2], &r2)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		
		// check the ranges
		if ((r1 & found->mask) != (found->ip & found->mask)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		if ((r2 & found->mask) != (found->ip & found->mask)) {
			strcpy(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		if (r1 >= r2) {
			strcpy(data, "Error: invalid range\n");
			return RCPERR;
		}
		
		// store the data
		found->range_start = r1;
		found->range_end = r2;
	}
	
	return 0;
}

//******************************************************************
// all modes
//******************************************************************

static int print_lease(Lease *lease, void *arg) {
	ASSERT(lease);
	ASSERT(arg);
	FILE *fp = arg;
	// count the lease
	shm->config.dhcps_leases++;
	
	// calculate the remaining lease time
	int expired = 0;
	time_t now = time(NULL);
	int delta = (int) lease->tstamp - now;
	if (delta < 0)
		expired = 1;

	char tmp[50 + 1];
	sprintf(tmp, "%02x:%02x:%02x:%02x:%02x:%02x   %d.%d.%d.%d",
		RCP_PRINT_MAC(lease->mac),
		RCP_PRINT_IP(lease->ip));

	if (expired)
		fprintf(fp, "%-50s  expired\n", tmp);
	else {
		// lease time in hours:minutes:seconds
		unsigned hours = 0;
		unsigned minutes = 0;
		unsigned seconds;

		if (delta >= 60) {
			seconds = delta % 60;
			delta -= seconds;
		}
		else
			seconds = delta;
			
		delta = delta / 60;
		if (delta >= 60) {
			minutes = delta % 60;
			delta -= minutes;
		}
		else
			minutes = delta;
			
		delta = delta / 60;
		hours = delta;

		fprintf(fp, "%-50s  %02u:%02u:%02u\n",
			tmp, hours, minutes, seconds);
	}

	return 0;
}

int cliShowIpDhcpLeases(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	fprintf(fp, "%-50s  %s\n",
		"MAC Address         IP Address",
		"Lease (hh:mm:ss)");
	shm->config.dhcps_leases = 0;
	lease_walk(print_lease, fp);

	fclose(fp);
	return 0;
}

int cliDebugDhcpLeaseTest(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	Lease *lease;
	
	time_t now = time(NULL);
	
	uint32_t ip = inet_addr("10.0.0.50");
	ip = ntohl(ip);
	uint8_t mac[6] = {0};
	mac[5] = 1;
	lease = lease_find_ip(ip);
	if (lease == NULL) {
		// add a lease
		lease = lease_add(mac, ip, now + 60);
		lease->lease = 60;
	}
	else {
		// renew a lease
		lease_renew(lease, now + 60);
	}
	
	ip++;
	mac[5]++;
	lease = lease_find_ip(ip);
	if (lease == NULL) {
		// add a lease
		lease = lease_add(mac, ip, now + 130);
		lease->lease = 130;
	}
	else {
		// renew a lease
		lease_renew(lease, now + 130);
	}

	ip++;
	mac[5]++;
	lease = lease_find_ip(ip);
	if (lease == NULL) {
		// add a lease
		lease = lease_add(mac, ip, now + 100);
		lease->lease = 100;
	}
	else {
		// renew a lease
		lease_renew(lease, now + 100);
	}
	
	ip++;
	mac[5]++;
	lease = lease_find_ip(ip);
	if (lease == NULL) {
		// add a lease
		lease = lease_add(mac, ip, now + 100);
		lease->lease = 100;
	}
	else {
		// renew a lease
		lease_renew(lease, now + 100);
	}
	
	ip++;
	mac[5]++;
	lease = lease_find_ip(ip);
	if (lease == NULL) {
		// add a lease
		lease = lease_add(mac, ip, now + 100);
		lease->lease = 100;
	}
	else {
		// renew a lease
		lease_renew(lease, now + 100);
	}

	return 0;
}

int cliClearDhcpLeaseCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	lease_clear_all();
	return 0;
}

int cliClearDhcpLeaseIpCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	uint32_t ip;
	if (atoip(argv[4], &ip)) {
		strcpy(data, "Error: invalid next hop address\n");
		return RCPERR;
	}
	
	Lease *lease = lease_find_ip(ip);
	if (lease)
		lease_delete(lease);

	return 0;
}
