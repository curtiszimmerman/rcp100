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

static RcpMasq *masq_find(uint32_t ip, uint32_t mask) {
	RcpMasq *ptr;
	int i;

	for (i = 0, ptr = shm->config.ip_masq; i < RCP_MASQ_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip == ptr->ip && mask == ptr->mask)
			return ptr;

	}
	 
	return NULL;
}


static RcpMasq *masq_find_empty(void) {
	RcpMasq *ptr;
	int i;

	for (i = 0, ptr = shm->config.ip_masq; i < RCP_MASQ_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}



int cliMasqCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int noform = 0;
	int index = 3;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
	}
	
	// extract data
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[index], &ip, &mask)) {
		sprintf(data, "Error: invalid IP address range");
		return RCPERR;
	}
	
	// find an existing entry in masq table	
	RcpMasq *found = masq_find(ip, mask);
	
	if (noform) {
		if (!found)	
			return 0; // nothing to do
		found->valid = 0;
	}
	else {
		if (found) {
			// just copy over the interface name
			strcpy(found->out_interface, argv[4]);
		}
		else {
			found = masq_find_empty();
			if (!found) {
				sprintf(data, "Error: cannot create NAT entry, limit reached");
				return RCPERR;
			}
			memset(found, 0, sizeof(RcpMasq));
			found->ip = ip;
			found->mask = mask;
			strcpy(found->out_interface, argv[4]);
			found->valid = 1;
		}
	}			
	
	// trigger netfilter configuration update
	netfilter_update = 2; 

	return 0;
}		

int cliShowNat(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	// grab a temporary transport file - the file name is stored in data
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	// close the file
	fclose(fp);
	
	// run iptables command
	char cmd[strlen(data) + 200];
	sprintf(cmd, "iptables-save -t nat -c | grep [:-] | grep -v iptables > %s", data);
	int v = system(cmd);
	if (v == -1)
		ASSERT(0);
	return 0;
}
