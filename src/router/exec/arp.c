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
#include <ifaddrs.h>
#include <net/if_arp.h>
#include "router.h"

//****************************************************************
// Shared memory handling
//****************************************************************
// check only the IP address
static int shm_is_arp_configured_ip(uint32_t ip) {
	int i;
	RcpStaticArp *ptr;
	
	// find the entry	
	for (i = 0, ptr = shm->config.sarp; i < RCP_ARP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		// enforcing unique IP and MAC addresses
		if (ptr->ip == ip)
			return 1;
	}
	
	return 0;
}

// return 1 if error
static int shm_add_arp(uint32_t ip, uint8_t *mac) {
	int i;
	RcpStaticArp *ptr;

	// find an empty entry
	for (i = 0, ptr = shm->config.sarp; i < RCP_ARP_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		break;
	}
	if (i == RCP_ARP_LIMIT)
		return 1;	// static arp table is full

	// set new arp
	memset(ptr, 0, sizeof(RcpStaticArp));
	ptr->ip = ip;
	memcpy(ptr->mac, mac, 6);
	ptr->valid = 1;
		
	return 0;
}


static void shm_del_arp(uint32_t ip) {
	// find the specific entry
	int i;

	// find the entry
	RcpStaticArp *ptr;
	for (i = 0, ptr = shm->config.sarp; i < RCP_ARP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ptr->ip == ip)
			break;
	}
	if (i == RCP_ARP_LIMIT) {
		ASSERT(0);
		return;
	}

	// delete the entry
	ptr->valid = 0;
}



//****************************************************************
// CLI commands
//****************************************************************
int cliArpCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// convert input data
	uint32_t ip;
	if (atoip(argv[1], &ip)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	uint8_t mac[6];
	if (atomac(argv[2], mac)) {
		strcpy(data, "Error: invalid MAC address\n");
		return RCPERR;
	}
	
	// arp already configured?
	if (shm_is_arp_configured_ip(ip))
		return 0; 

	// store arp entry in shared memory regardless if it was set inside the kernel
	if (shm_add_arp(ip, mac)) {
		strcpy(data, "Error: cannot add ARP entry, ARP limit reached\n");
		return RCPERR;
	}
	
	// if the interface is down or if no address configured, the arp entry will not be set by the kernel
	if (kernel_set_arp(ip, mac)) {
		strcpy(data, "Warning: ARP entry not active\n");
	}

	return 0;
}

int cliNoArpCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// convert input data
	uint32_t ip;
	if (atoip(argv[2], &ip)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// do nothing if arp is not configured
	if (shm_is_arp_configured_ip(ip) == 0)
		return 0;
	
	// delete entry from kernel
	kernel_del_arp(ip);

	// remove entry from shared memory
	shm_del_arp(ip);
	
	return 0;
}

int cliNoArpAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	int i;
	RcpStaticArp *ptr;
	
	// delete all entries
	for (i = 0, ptr = shm->config.sarp; i < RCP_ARP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		// remove entry from kernel
		uint32_t ip = ptr->ip;
		kernel_del_arp(ip);

		// delete entry
		ptr->valid = 0;
	}

	return 0;
}


#define LINE_BUF 1024
int cliArpClearCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp == NULL) {
		strcpy(data, "Error: no ARP information available\n");
		return RCPERR;
	}
	
	char line[LINE_BUF + 1];
	if (fgets(line, LINE_BUF, fp) == NULL) {
		fclose(fp);
		strcpy(data, "Error: no ARP information available\n");
		return RCPERR;
	}
	while (fgets(line, LINE_BUF, fp)) {
		char ipstr[32];
		unsigned type;
		unsigned flags;
		int rv = sscanf(line, "%s 0x%x 0x%x", ipstr, &type, &flags);
		if (rv != 3) {
			ASSERT(0);
			break;
		}
		
		if (flags & ATF_PERM) // will not delete static entries
			continue;
			
		uint32_t ip = 0;
		if (atoip(ipstr, &ip)) {
			ASSERT(0);
			// continue, don't break!
		}
	
		if (ip && kernel_del_arp(ip) ) {
			ASSERT(0);
			// continue, don't break!
		}
		
	}
	fclose(fp);	
	
	return 0;
}

static int kernel_set_proxyarp(const char *interface, uint8_t value) {
	ASSERT(interface != NULL);
	ASSERT(value == 0 || value == 1);

	char filename[100];
	sprintf(filename, "/proc/sys/net/ipv4/conf/%s/proxy_arp", interface);
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_ERR, RLOG_FC_SYSCFG,
			"cannot open kernel proxy-arp file");
		return 1;
	}
		
	fprintf(fp, "%d\n", value);
	fclose(fp);

	rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_DEBUG, RLOG_FC_SYSCFG,
			"write kernel file %s", filename);
	return 0;
}

int cliIpProxyArp(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	uint8_t new_value = (argc == 3)? 0: 1;
	if (kernel_set_proxyarp(ifname, new_value)) {
		strcpy(data, "Error: cannot set proxy-arp, attempting recovery...\n");
		return RCPERR;
	}

	pif->proxy_arp = new_value;
	return 0;
}


//******************************************************************************************
// ARP interface update
//******************************************************************************************
void router_update_if_arp(RcpInterface *intf) {
	ASSERT(intf != NULL);
	int i;
	RcpStaticArp *ptr;
	
	// walk trough arp configuration and set every single arp located on this interface
	for (i = 0, ptr = shm->config.sarp; i < RCP_ARP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if ((intf->ip & intf->mask) == (ptr->ip & intf->mask))
			kernel_set_arp(ptr->ip, ptr->mac);
	}
}
