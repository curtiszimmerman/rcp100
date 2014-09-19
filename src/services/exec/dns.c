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
#include "services.h"
#include <errno.h>

volatile int update_hosts_file = 0;
volatile int update_resolvconf_file = 0;

static void trigger_dns_update(void) {
	// find dns proxy socket
	MuxSession *session = get_mux_process(RCP_PROC_DNS);
	if (session != NULL) {
		logDistribute("sending DNS update message to dns proxy", RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
		
		// build dns update packet
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = RCP_PROC_DNS;
		pkt.sender = RCP_PROC_SERVICES;
		pkt.type = RCP_PKT_TYPE_UPDATEDNS;
		
		errno = 0;
		int n = send(session->socket, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(session->socket);
			clean_mux(session);
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}	
}

// hostname command
int cliSetHostnameCmd(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// save data in shared memory
	strncpy(shm->config.hostname, argv[1], CLI_MAX_HOSTNAME);

	return 0;
}

// no hostname command
int cliNoSetHostnameCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// save data in shared memory
	strcpy(shm->config.hostname, "rcp");
	return 0;
}

// set domain name
int cliIpDomainNameCmd(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// save data in shared memory
	if (strcmp(argv[0], "no") == 0)
		*shm->config.domain_name = '\0';
	else {
		strcpy(shm->config.domain_name, argv[2]);
		rcpDebug("services: domain name #%s#\n", shm->config.domain_name);
	}
	
	// change in system files needed
	update_resolvconf_file = 1;	

	return 0;
}


// no ip name-server command; maximum two name servers are permitted
int cliNoIpNameServerCmd(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// extract the ip address
	uint32_t addr;
	if (atoip(argv[3], &addr)) {
		sprintf(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	// update shared memory
	if (shm->config.name_server1 == addr)
		shm->config.name_server1 = 0;
	if (shm->config.name_server2 == addr)
		shm->config.name_server2 = 0;

	// change in system files needed
	update_resolvconf_file = 1;	

	return 0;
}

// no ip name-server
int cliNoIpNameServerAll(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	shm->config.name_server1 = 0;
	shm->config.name_server2 = 0;

	// change in system files needed
	update_resolvconf_file = 1;	

	return 0;
}

// ip name-server command; maximum two name servers are permitted
int cliIpNameServerCmd(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// extract the ip address
	uint32_t addr;
	if (atoip(argv[2], &addr)) {
		sprintf(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	// nothing to do if the server is already configured
	if (shm->config.name_server1 == addr ||
	    shm->config.name_server2 == addr )
		return RCPERR;
	
	// update shared memory
	if (shm->config.name_server1 == 0) {
		shm->config.name_server1 = addr;
		rcpDebug("services: updated name_server1\n");

		// change in system files needed
		update_resolvconf_file = 1;	
		
		return 0;
	}
	
	// in case we got here, just overwrite the second name server
	shm->config.name_server2 = addr;
	rcpDebug("services: updated name_server2\n");
	
	// change in system files needed
	update_resolvconf_file = 1;	

	return 0;
}


//******************************************************************
// ip host array and command
//******************************************************************
// find a host in the array
static RcpIpHost *host_find(const char *name) {
	int i;
	RcpIpHost *ptr;
	
	for (i = 0, ptr = shm->config.ip_host; i < RCP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		if (strcmp(name, ptr->name) == 0)
			return ptr;
	}
	
	return NULL;
}

// find an empty host entry
static RcpIpHost *host_find_empty() {
	int i;
	RcpIpHost *ptr;
	
	for (i = 0, ptr = shm->config.ip_host; i < RCP_HOST_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}

// no ip host command
int cliNoIpHostNameAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	int i;
	RcpIpHost *ptr;
	
	for (i = 0, ptr = shm->config.ip_host; i < RCP_HOST_LIMIT; i++, ptr++) {
		ptr->valid = 0;
	}
	
	return 0;
}

// no ip host command
int cliNoIpHostNameCmd(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// find the host
	RcpIpHost *found = host_find(argv[3]);

	// remove the host
	if (found != NULL) {
		found->valid = 0;

		// change in system files needed
		update_hosts_file = 1;	
	}

	// send dns update message to dns proxy
	trigger_dns_update();
 	return 0;
}

// ip host command
int cliIpHostNameCmd(CliMode mode, int argc, char **argv) {
	ASSERT(strlen(argv[1]) <= CLI_MAX_HOSTNAME);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// extract the ip address
	uint32_t addr;
	if (atoip(argv[3], &addr)) {
		sprintf(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	// find the host
	RcpIpHost *found = host_find(argv[2]);
	
	// update or add a new host
	if (found) {
		// update the ip address
		found->ip = addr;
	}
	else {
		// add a new host
		RcpIpHost *newhost = host_find_empty();
		memset(newhost, 0, sizeof(RcpIpHost));
		strcpy(newhost->name, argv[2]);
		newhost->ip = addr;
		newhost->valid = 1;
	}
	
	// change in system files needed
	update_hosts_file = 1;	

	// send dns update message to dns proxy
	trigger_dns_update();
	return 0;
}
