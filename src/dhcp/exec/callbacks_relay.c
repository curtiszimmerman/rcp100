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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
RcpPkt *pktout = NULL;


static RcpDhcprServer *find_server(uint32_t ip1, uint32_t ip2, uint32_t ip3, uint32_t ip4) {
	int i;
	RcpDhcprServer *ptr;

	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip1 == ptr->server_ip[0] &&
		    ip2 == ptr->server_ip[1] &&
		    ip3 == ptr->server_ip[2] &&
		    ip4 == ptr->server_ip[3])
			return ptr;
	}
	
	return NULL;
}

static RcpDhcprServer *find_server_empty() {
	int i;
	RcpDhcprServer *ptr;

	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}



//******************************************************************
// callbacks
//******************************************************************
int processCli(RcpPkt *pkt) {
	// execute cli command
	pktout = pkt;
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

//******************************************************************
// interface mode
//******************************************************************
int cliDhcpRelayCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *intf = rcpFindInterfaceByName(shm, ifname);
	if (intf == NULL) {
		sprintf(data, "Error: cannot find the current interface");
		return RCPERR;
	}
	
	if (strcmp(argv[0], "no") == 0) {
		if (intf->dhcp_relay)
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_NOTICE, RLOG_FC_INTERFACE,
				"DHCP disabled for interface %s", intf->name);
		intf->dhcp_relay = 0;
	}
	else {
		if (intf->dhcp_relay == 0)
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_NOTICE, RLOG_FC_INTERFACE,
				"DHCP enabled for interface %s", intf->name);
		intf->dhcp_relay = 1;
	}

	return 0;
}

//******************************************************************
// dhcp mode
//******************************************************************
int cliNoServerAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int i;
	for (i = 0; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++)
		shm->config.dhcpr_server[i].valid = 0;
		
	return 0;
}

int cliServerCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract data
	int ipindex = 0;
	if (strcmp(argv[0], "no") == 0) {
		ipindex = 2;
	}
	else {
		ipindex = 1;
	}
	
	// extract server list
	uint32_t ip1 = 0;
	uint32_t ip2 = 0;
	uint32_t ip3 = 0;
	uint32_t ip4 = 0;
	if (atoip(argv[ipindex], &ip1)) {
		sprintf(data, "Error: invalid IP address");
		return RCPERR;
	}
	if (++ipindex < argc) {
		if (atoip(argv[ipindex], &ip2)) {
			sprintf(data, "Error: invalid IP address");
			return RCPERR;
		}
		if (++ipindex < argc) {
			if (atoip(argv[ipindex], &ip3)) {
				sprintf(data, "Error: invalid IP address");
				return RCPERR;
			}
			if (++ipindex < argc) {
				if (atoip(argv[ipindex], &ip4)) {
					sprintf(data, "Error: invalid IP address");
					return RCPERR;
				}
			}
		}
	}
	
	// add/delete servers
	RcpDhcprServer *found = find_server(ip1, ip2, ip3, ip4);
	if (strcmp(argv[0], "no") == 0) {
		// delete server group
		if (found == NULL)
			return 0;
		found->valid = 0;
	}
	else {
		// modify an existing server group
		if (found != NULL) {
			// update the values
			found->server_ip[0] = ip1;
			found->server_ip[1] = ip2;
			found->server_ip[2] = ip3;
			found->server_ip[3] = ip4;
			return 0;
		}
			
		RcpDhcprServer *newsrv = find_server_empty();
		if (newsrv == NULL) {
			strcpy(data, "Error: cannot add DHCP server group, limit reached\n");
			return RCPERR;
		}
		memset(newsrv, 0, sizeof(RcpDhcprServer));
		newsrv->server_ip[0] = ip1;
		newsrv->server_ip[1] = ip2;
		newsrv->server_ip[2] = ip3;
		newsrv->server_ip[3] = ip4;
		newsrv->valid = 1;
	}

	return 0;
}

int cliServiceDelayCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.dhcpr_service_delay = RCP_DHCPR_DEFAULT_SERVICE_DELAY;
	}
	else {
		shm->config.dhcpr_service_delay = atoi(argv[1]);
	}
			
	return 0;
}

int cliMaxHopsCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		shm->config.dhcpr_max_hops = RCP_DHCPR_DEFAULT_MAX_HOPS;
	else
		shm->config.dhcpr_max_hops = atoi(argv[1]);
		
	
	return 0;
}

int cliClearDhcpStatsCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0')
			continue;

		RcpInterface *ptr = &shm->config.intf[i];
		ptr->stats.dhcp_rx = 0;
		ptr->stats.dhcp_rx_discover = 0;
		ptr->stats.dhcp_rx_request = 0;
		ptr->stats.dhcp_rx_decline = 0;
		ptr->stats.dhcp_rx_release = 0;
		ptr->stats.dhcp_rx_inform = 0;
		ptr->stats.dhcp_rx_offer = 0;
		ptr->stats.dhcp_rx_ack = 0;
		ptr->stats.dhcp_rx_nack = 0;
	
		// dhcp errors	
		ptr->stats.dhcp_err_rx = 0;
		ptr->stats.dhcp_err_hops = 0;
		ptr->stats.dhcp_err_pkt = 0;
		ptr->stats.dhcp_err_not_configured = 0;
		ptr->stats.dhcp_err_server_notconf = 0;
		ptr->stats.dhcp_warn_server_unknown = 0;
		ptr->stats.dhcp_err_outgoing_intf = 0;
		ptr->stats.dhcp_err_giaddr = 0;
		ptr->stats.dhcp_err_lease = 0;
		ptr->stats.dhcp_err_icmp = 0;
		
		// dhcp transmit stats - packets sent to clients
		ptr->stats.dhcp_tx = 0;
		ptr->stats.dhcp_tx_offer = 0;
		ptr->stats.dhcp_tx_ack = 0;
		ptr->stats.dhcp_tx_nack = 0;
		ptr->stats.dhcp_tx_discover = 0;
		ptr->stats.dhcp_tx_request = 0;
		ptr->stats.dhcp_tx_decline = 0;
		ptr->stats.dhcp_tx_release = 0;
		ptr->stats.dhcp_tx_inform = 0;
		
		// stats
		shm->stats.dhcp_last_rx = 0;
	}

	// clear lease count
	RcpDhcprServer *ptr;
	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		int j;
		for (j = 0; j < RCP_DHCP_SERVER_LIMIT; j++) {
			ptr->leases[j] = 0;
		}
	}
	
	return 0;
}

int cliOption(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		shm->config.dhcpr_option82 = 0;
	else
		shm->config.dhcpr_option82 = 1;
		
	return 0;
}

	
//******************************************************************
// config mode
//******************************************************************
int cliNoIpDhcpRelay(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// clear dhcp relay configuration
	shm->config.dhcpr_max_hops = RCP_DHCPR_DEFAULT_MAX_HOPS;
	shm->config.dhcpr_service_delay = RCP_DHCPR_DEFAULT_SERVICE_DELAY;
	
	// clear server groups
	int i;
	RcpDhcprServer *ptr;

	for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
		memset(ptr, 0, sizeof(RcpDhcprServer));
	}

	return 0;
}

int cliIpDhcpRelayCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_DHCP_RELAY;
	pktout->pkt.cli.mode_data  = NULL;

	return 0;
}

int cliShowIpDhcp(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// open a transport file
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	struct stat s;
	if (stat("/opt/rcp/var/log/dhcp_at_startup", &s) == 0) {		
		fprintf(fp, "Warning: an external DHCP server or relay is already running on the system.\n");
		fprintf(fp, "It might interfere with RCP DHCP functionality.\n\n");
	}	

	// parse arguments
	int interfaces = 0;
	int errors = 1;
	if (argc == 4) {
		if (strcmp(argv[3], "interfaces") == 0) {
			interfaces = 1;
			errors = 0;
		}
		else
			ASSERT(0);
	}

	// print data
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0')
			continue;

		RcpInterface *ptr = &shm->config.intf[i];
		if (ptr->stats.dhcp_rx == 0 && ptr->stats.dhcp_tx == 0)
			continue;
		
		fprintf(fp, "Interface %s RX/TX packets:\n", ptr->name);
		if (ptr->stats.dhcp_err_rx == 0)
			fprintf(fp, "\tRX %u, TX %u\n", ptr->stats.dhcp_rx, ptr->stats.dhcp_tx);
		else
			fprintf(fp, "\tRX %u, TX %u, RX errors %u\n", ptr->stats.dhcp_rx, ptr->stats.dhcp_tx, ptr->stats.dhcp_err_rx);

		if (errors && ptr->stats.dhcp_err_rx) {
			// print error counts
			if (ptr->stats.dhcp_err_hops)
				fprintf(fp, "\tMax-hops exceeded: %u\n", ptr->stats.dhcp_err_hops);
			if (ptr->stats.dhcp_err_pkt)
				fprintf(fp, "\tMalforemd packets: %u\n", ptr->stats.dhcp_err_pkt);
			if (ptr->stats.dhcp_err_not_configured)
				fprintf(fp, "\tInterface not configured: %u\n", ptr->stats.dhcp_err_not_configured);
			if (ptr->stats.dhcp_err_server_notconf)
				fprintf(fp, "\tNo DHCP server configured: %u\n", ptr->stats.dhcp_err_server_notconf);
			if (ptr->stats.dhcp_err_outgoing_intf)
				fprintf(fp, "\tUnknown outgoing interface: %u\n", ptr->stats.dhcp_err_outgoing_intf);
			if (ptr->stats.dhcp_err_lease)
				fprintf(fp, "\tLease errors: %u\n", ptr->stats.dhcp_err_lease);
			if (ptr->stats.dhcp_err_icmp)
				fprintf(fp, "\tClient IP address already present in the network: %u\n", ptr->stats.dhcp_err_icmp);
		}
		if (errors && ptr->stats.dhcp_warn_server_unknown) {
			fprintf(fp, "\tWarning: unknown DHCP server: %u\n", ptr->stats.dhcp_warn_server_unknown);
		}

		if (interfaces) {
			if (ptr->stats.dhcp_rx) {
				fprintf(fp, "RX stats:\n");
				char str[50];
				sprintf(str, "DHCPDISCOVER %u", ptr->stats.dhcp_rx_discover);
				fprintf(fp, "\t%-30sDHCPOFFER %u\n",
					str, ptr->stats.dhcp_rx_offer);
				sprintf(str, "DHCPREQUEST %u", ptr->stats.dhcp_rx_request);
				fprintf(fp, "\t%-30sDHCPACK %u\n",
					str, ptr->stats.dhcp_rx_ack);
				sprintf(str, "DHCPDECLINE %u", ptr->stats.dhcp_rx_decline);
				fprintf(fp, "\t%-30sDHCPNACK %u\n",
					str, ptr->stats.dhcp_rx_nack);
				sprintf(str, "DHCPRELEASE %u", ptr->stats.dhcp_rx_release);
				fprintf(fp, "\t%-30sDHCPINFORM %u\n",
					str, ptr->stats.dhcp_rx_inform);
				fprintf(fp, "\n");
			}

			if (ptr->stats.dhcp_tx) {
				fprintf(fp, "TX stats:\n");
				char str[50];
				sprintf(str, "DHCPDISCOVER %u", ptr->stats.dhcp_tx_discover);
				fprintf(fp, "\t%-30sDHCPOFFER %u\n",
					str, ptr->stats.dhcp_tx_offer);
				sprintf(str, "DHCPREQUEST %u", ptr->stats.dhcp_tx_request);
				fprintf(fp, "\t%-30sDHCPACK %u\n",
					str, ptr->stats.dhcp_tx_ack);
				sprintf(str, "DHCPDECLINE %u", ptr->stats.dhcp_tx_decline);
				fprintf(fp, "\t%-30sDHCPNACK %u\n",
					str, ptr->stats.dhcp_tx_nack);
				sprintf(str, "DHCPRELEASE %u", ptr->stats.dhcp_tx_release);
				fprintf(fp, "\t%-30sDHCPINFORM %u\n",
					str, ptr->stats.dhcp_tx_inform);
				fprintf(fp, "\n");
			}
		}
		
		fprintf(fp, "\n");
	}

	fclose(fp);
	return 0;
}


int cliShowIpDhcpServers(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// open a transport file
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	struct stat s;
	if (stat("/opt/rcp/var/log/dhcp_at_startup", &s) == 0) {		
		fprintf(fp, "Warning: an external DHCP server or relay is already running on the system.\n");
		fprintf(fp, "It might interfere with RCP DHCP functionality.\n\n");
	}	

	// print data
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0')
			continue;

		if (shm->config.intf[i].dhcp_relay) {
			found =1;
			break;
		}
	}
	
	if (!found) {
		fclose(fp);
		return 0;
	}

	fprintf(fp, "DHCP relay is configured for the following interfaces:\n");
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0')
			continue;

		if (shm->config.intf[i].dhcp_relay)
			fprintf(fp, "\t%s\n", shm->config.intf[i].name);
	}
	fprintf(fp, "\n");

	fprintf(fp, "External DHCP servers:\n");
	int n1;
	for (n1 = 0; n1 < RCP_DHCP_SERVER_GROUPS_LIMIT; n1++) {
		RcpDhcprServer *srv = &shm->config.dhcpr_server[n1];
		if (!srv->valid)
			continue;
			
		int n2;
		for (n2 = 0; n2 < RCP_DHCP_SERVER_LIMIT; n2++) {
			if (srv->server_ip[n2] == 0)
				continue;
				
			fprintf(fp, "\t%d.%d.%d.%d: %u leases\n",
				RCP_PRINT_IP(srv->server_ip[n2]),
				srv->leases[n2]);
		}
	}

	fclose(fp);
	return 0;
}

	
