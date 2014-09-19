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
RcpPkt *pktout = NULL;

// send route request message to ospf
void route_request(void) {
	if (muxsock) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = RCP_PROC_OSPF;
		pkt.sender = RCP_PROC_RIP;
		pkt.type = RCP_PKT_TYPE_ROUTE_RQ ;
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}
}




//******************************************************************
// rip network/neighbor linked list
//******************************************************************
static RcpRipPartner *find_network(uint32_t ip, uint32_t mask) {
	RcpRipPartner *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip == ptr->ip && mask == ptr->mask)
			return ptr;
	}
	
	return NULL;
}

static RcpRipPartner *find_network_empty() {
	RcpRipPartner *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}

static RcpRipPartner *find_neighbor(uint32_t ip, uint32_t mask) {
	RcpRipPartner *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip == ptr->ip && mask == ptr->mask)
			return ptr;
	}
	
	return NULL;
}

static RcpRipPartner *find_neighbor_empty() {
	RcpRipPartner *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}



//******************************************************************
// callbacks
//******************************************************************
// cli html flag
int cli_html = 0;
int processCli(RcpPkt *pkt) {
	// execute cli command
	pktout = pkt;
	cli_html = pktout->pkt.cli.html;
	char *data = (char *) pkt + sizeof(RcpPkt);
	pkt->pkt.cli.return_value = cliExecNode(pkt->pkt.cli.mode, pkt->pkt.cli.clinode, data);

	// flip sender and destination
	unsigned tmp = pkt->destination;
	pkt->destination = pkt->sender;
	pkt->sender = tmp;
	
	// set new data length
	int len = strlen(data);
	pkt->data_len = (len == 0)? 0: len + 1;
	return 0;
}

int cliNoNetworkCidrAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	RcpRipPartner *ptr;
	int i;
	
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) 
		ptr->valid = 0;
	
	return 0;
}

int cliNetworkCidrCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
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
	
	// we do not attempt to enable rip on the interface at this time
	// this is left to the interface monitoring task
	RcpRipPartner *found = find_network(ip, mask);
	if (strcmp(argv[0], "no") == 0) {
		if (found == NULL)
			return 0;
		found->valid = 0;
	}
	else {
		if (found != NULL)
			return 0;
		RcpRipPartner *newnet = find_network_empty();
		if (newnet == NULL) {
			strcpy(data, "Error: network not configured, limit reached\n");
			return RCPERR;
		}
		memset(newnet, 0, sizeof(RcpRipPartner));
		newnet->ip = ip;
		newnet->mask = mask;
		newnet->valid = 1;
	}

	return 0;
}

int cliShowIpRipDatabaseCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	ripdb_print(fp);

	fclose(fp);
	return 0;

}

static void print_net(FILE *fp, RcpRipPartner *net) {
	char network[20];
	if (net->mask == 0xffffffff)
		sprintf(network, "%d.%d.%d.%d",
			RCP_PRINT_IP(net->ip));
	else
		sprintf(network, "%d.%d.%d.%d/%d",
			RCP_PRINT_IP(net->ip), mask2bits(net->mask));

	char req[24];
	sprintf(req, "%u/%u", net->req_rx, net->req_tx);
	char resp[24];
	sprintf(resp, "%u/%u", net->resp_rx, net->resp_tx);
		
	fprintf(fp, "%-19s%-20.20s%-20.20s\n",
		network, req, resp);
}

int cliShowIpRipCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	fprintf(fp, "%-19s%-20.20s%-20.20s\n",
		"Network", "Request (Rx/Tx)", "Response (Rx/Tx)");

	// print configured networks
	RcpRipPartner *net;
	int i;
	for (i = 0, net = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, net++) {
		if (!net->valid)
			continue;
		print_net(fp, net);
	}
	
	// print neighbor list
	fprintf(fp, "\n");
	fprintf(fp, "%-19s%-19s%s\n", "Neighbor", "Last Update", "Errors (packet/route/MD5)");
	RipNeighbor *neigh = rxneighbors();
	while (neigh != NULL) {
		char ip[21];
		sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(neigh->ip));

		char lup[21];
		sprintf(lup, "%u", neigh->rx_time);
		
		char perr[21];
		sprintf(perr, "%u", neigh->errors);

		char rerr[21];
		sprintf(rerr, "%u", neigh->route_errors);

		fprintf(fp, "%-19s%-19s%u/%u/%u\n",
			ip,
			lup,
			neigh->errors,
			neigh->route_errors,
			neigh->md5_errors);
		neigh = neigh->next;
	}
	
	// print interface list
	fprintf(fp, "\n");
	fprintf(fp, "%-19s%s\n", "Interface", "Update");
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK)
			continue;
		
		if (intf->rip_multicast_ip)
			fprintf(fp, "%-19s%d\n", intf->name, intf->rip_timeout);
	}
	
	fclose(fp);
	return 0;

}


int cliTracePrefixCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		trace_prefix = 0;
	else
		atoip(argv[2], &trace_prefix);

	return 0;
}

int cliNeighborCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
	}
	else {
		index = 1;
	}

	uint32_t ip;
	if (atoip(argv[index], &ip)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	RcpRipPartner *found = find_neighbor(ip, 0xffffffff);
	if (strcmp(argv[0], "no") == 0) {
		if (found == NULL)
			return 0;

		rcpLog(muxsock, RCP_PROC_RIP, RLOG_NOTICE, RLOG_FC_INTERFACE,
			"RIP neighbor %d.%d.%d.%d deleted", 
			RCP_PRINT_IP(found->ip));

		found->valid = 0;
	}
	else {
		if (found != NULL)
			return 0;
		RcpRipPartner *newnet = find_neighbor_empty();
		if (newnet == NULL) {
			strcpy(data, "Error: cannot configure neighbor, limit reached\n");
			return RCPERR;
		}
		memset(newnet, 0, sizeof(RcpRipPartner));
		newnet->ip = ip;
		newnet->mask = 0xffffffff;
		newnet->valid = 1;
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_NOTICE, RLOG_FC_INTERFACE,
			"RIP neighbor %d.%d.%d.%d created", 
			RCP_PRINT_IP(newnet->ip));
		
		// send a rip request to the neighbor
		RcpInterface *intf = rcpFindInterfaceByLPM(shm, newnet->ip);
		if (intf != NULL && intf->link_up)
		 	txrequest(intf, newnet);
	}

	return 0;
}

int cliPassiveInterfaceCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
	}
	else {
		index = 1;
	}
	RcpInterface *intf = rcpFindInterfaceByName(shm, argv[index]);
	if (intf == NULL) {
		strcpy(data, "Error: cannot find interface\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0) {
		intf->rip_passive_interface = 0;
	}
	else {
		intf->rip_passive_interface = 1;
	}

	return 0;
}

int cliRedistStaticCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.rip_redist_static = 0;
		ripdb_delete_type(RCP_ROUTE_STATIC);
	}
	else {
		uint32_t newmetric = 1;
		if (argc == 4)
			newmetric = atoi(argv[3]);
		else
			newmetric = 1;
		
		if (shm->config.rip_redist_static != newmetric)
			ripdb_delete_type(RCP_ROUTE_STATIC);
		shm->config.rip_redist_static = newmetric;
	}

	// update all static routes
	rip_update_static();

	return 0;
}

int cliRedistConnectedCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.rip_redist_connected = 0;
		ripdb_delete_type(RCP_ROUTE_CONNECTED);
	}
	else {
		uint32_t newmetric = 1;
		if (argc == 4)
			newmetric = atoi(argv[3]);
		else
			newmetric = 1;
		
		if (shm->config.rip_redist_connected != newmetric)
			ripdb_delete_type(RCP_ROUTE_CONNECTED);
		shm->config.rip_redist_connected = newmetric;
		
		// push connected routes into the database
		rip_update_connected();
	}
	return 0;
}

int cliRedistConnectedSubnetsCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.rip_redist_connected_subnets = 0;
		ripdb_delete_type(RCP_ROUTE_CONNECTED_SUBNETS);
	}
	else {
		shm->config.rip_redist_connected_subnets = 1;
		
		// push connected routes into the database
		rip_update_connected_subnets();
	}
	return 0;
}

int cliRedistOspfCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.rip_redist_ospf = 0;
		ripdb_delete_type(RCP_ROUTE_OSPF);
	}
	else {
		uint32_t newmetric = 1;
		if (argc == 4)
			newmetric = atoi(argv[3]);
		else
			newmetric = 1;
		
		if (shm->config.rip_redist_ospf != newmetric)
			ripdb_delete_type(RCP_ROUTE_OSPF);
		shm->config.rip_redist_ospf = newmetric;
		route_request();
	}

	return 0;
}

int cliDefaultInformationOriginateCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.rip_default_information = 0;
		ripdb_delete_type(RCP_ROUTE_STATIC);	// this will also remove the default routes
	}
	else {
		shm->config.rip_default_information = 1;
	}
		
	// adding back the static routes
	rip_update_static();
	return 0;
}

int cliUpdateTimerCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0)
		shm->config.rip_update_timer = RCP_RIP_DEFAULT_TIMER;
	else
		shm->config.rip_update_timer = atoi(argv[1]);
	return 0;
}


int cliNoRouterRipCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// clear all networks
	RcpRipPartner *ptr;
	int i;
	for (i = 0, ptr = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		ptr->valid = 0;
	}
	
	// clear all neighbors
	for (i = 0, ptr = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		ptr->valid = 0;
	}
	
	// clear passive interface configuration
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++)
		shm->config.intf[i].rip_passive_interface = 0;
	
	// clear redistribution
	shm->config.rip_redist_connected = 0;
	shm->config.rip_redist_static = 0;
	
	// clear default route
	shm->config.rip_default_information = 0;
	
	// clear update timer
	shm->config.rip_update_timer = RCP_RIP_DEFAULT_TIMER;
	
	// clear all routes from the database
	ripdb_delete_all();
	
	return 0;
}

int cliRouterRipCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_RIP;
	pktout->pkt.cli.mode_data  = NULL;

	return 0;
}

//****************************************************************
// Interface Commands
//****************************************************************
int cliIpRipMd5(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0) {
		pif->rip_passwd[0] = '\0';
	}
	else {
		strcpy(pif->rip_passwd, argv[4]);
	}

	return 0;
}
		

//******************************************************************
// debug callbacks
//******************************************************************
int cliDebugRipPkt1(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	uint32_t ip = 0;
	uint32_t mask = 0;
	uint32_t gw = 0;
	uint32_t metric = 4;
	RcpInterface *rxif = rcpFindInterfaceByName(shm, "br0");
	atocidr("10.77.88.0/24", &ip, &mask);
	atoip("1.2.3.99", &gw);
	ripdb_add(RCP_ROUTE_RIP, ip, mask, gw, metric, gw, rxif);
	return 0;
}

int cliDebugRipPkt2(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	uint32_t ip = 0;
	uint32_t mask = 0;
	uint32_t gw = 0;
	uint32_t metric = 6;
	RcpInterface *rxif = rcpFindInterfaceByName(shm, "br0");
	atocidr("10.77.88.0/24", &ip, &mask);
	atoip("1.2.3.99", &gw);
	ripdb_add(RCP_ROUTE_RIP, ip, mask, gw, metric, gw, rxif);
	return 0;
}

int cliDebugRipPkt3(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	uint32_t ip = 0;
	uint32_t mask = 0;
	uint32_t gw = 0;
	rcpFindInterfaceByName(shm, "br0");
	atocidr("10.77.88.0/24", &ip, &mask);
	atoip("1.2.3.99", &gw);
	ripdb_delete(RCP_ROUTE_RIP, ip, mask, gw, gw);
	return 0;
}

int cliDebugRipPkt4(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	uint32_t ip = 0;
	uint32_t mask = 0;
	uint32_t gw = 0;
	uint32_t metric = 2;
	RcpInterface *rxif = rcpFindInterfaceByName(shm, "br0");
	atocidr("10.77.88.0/24", &ip, &mask);
	atoip("1.2.3.20", &gw);
	ripdb_add(RCP_ROUTE_RIP, ip, mask, gw, metric, gw, rxif);
	return 0;
}

int cliDebugRipPkt5(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	uint32_t gw = 0;
	uint32_t metric = 1;
	atoip("1.2.3.20", &gw);
	RcpInterface *rxif = rcpFindInterfaceByName(shm, "br0");
	ripdb_add(RCP_ROUTE_RIP, 0, 0, 0, metric, gw, rxif);
	return 0;
}


int cliDebugRipAdd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract data
	uint32_t ip = 0;
	uint32_t mask = 0;
	if (atocidr(argv[3], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	uint32_t gw;
	if (atoip(argv[4], &gw)) {
		strcpy(data, "Error: invalid next hop address\n");
		return RCPERR;
	}

	uint32_t metric = atoi(argv[5]);
	RcpInterface *rxif = rcpFindInterfaceByName(shm, "br0");
	ripdb_add(RCP_ROUTE_RIP, ip, mask, gw, metric, gw, rxif);
	return 0;
}

int cliDebugRipDel(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract data
	uint32_t ip = 0;
	uint32_t mask = 0;
	if (atocidr(argv[3], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	uint32_t gw = 0;
	if (atoip(argv[4], &gw)) {
		strcpy(data, "Error: invalid next hop address\n");
		return RCPERR;
	}

	ripdb_delete(RCP_ROUTE_RIP, ip, mask, gw, gw);
	return 0;
}

