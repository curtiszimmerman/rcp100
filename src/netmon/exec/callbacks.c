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
#include "mgmt.h"
#include <ctype.h>

RcpPkt *pktout = NULL;
volatile int netmon_enabled = 0;

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
// network monitor
//******************************************************************
void netmon_clear(void) {
	RcpNetmonHost *ptr;
	int i;

	// clean hosts
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		ptr->ip_resolved = 0;
		ptr->ip_sent = 0;
		ptr->pkt_sent = 0;
		ptr->pkt_received = 0;
		ptr->wait_response = 0;
		ptr->interval_pkt_sent = 0;
		ptr->interval_pkt_received = 0;
		
	}
}


int cliShowNetworkMonitor(CliMode mode, int argc, char **argv) {
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

	RcpNetmonHost *ptr;
	int i;

	fprintf(fp, "Monitoring interval %d seconds\n", shm->config.monitor_interval);
	
	fprintf(fp, "%-24.24s %-15.15s %-8.8s %-10.10s Response time (ms)\n", "Host", "Type", "Status", "Uptime (%)");
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->pkt_sent == 0)
			continue;
			
		float uptime = 100 * ((float) ptr->pkt_received / (float) ptr->pkt_sent);
		if (uptime > 100)
			uptime = 100;
		char upstr[20];
		sprintf(upstr, "%.02f", uptime);
		char type[20];
		if (ptr->type == RCP_NETMON_TYPE_ICMP)
			strcpy(type, "ICMP");
		else if (ptr->type == RCP_NETMON_TYPE_TCP)
			sprintf(type, "TCP (%u)", ptr->port);
		else if (ptr->type == RCP_NETMON_TYPE_DNS)
			sprintf(type, "DNS");
		else if (ptr->type == RCP_NETMON_TYPE_NTP)
			sprintf(type, "NTP");
		else if (ptr->type == RCP_NETMON_TYPE_SSH) {
			int port = 22;
			if (ptr->port)
				port = ptr->port;
			sprintf(type, "SSH (%u)", port);
		}
		else if (ptr->type == RCP_NETMON_TYPE_SMTP) {
			int port = 25;
			if (ptr->port)
				port = ptr->port;
			sprintf(type, "SMTP (%u)", port);
		}
		else if (ptr->type == RCP_NETMON_TYPE_HTTP) {
			int port = 80;
			if (ptr->port)
				port = ptr->port;
			sprintf(type, "HTTP (%u)", port);
		}
		
		char resp[20];
		if (ptr->resptime_samples == 0)
			*resp = '\0';
		else {
			unsigned r = ptr->resptime_acc / ptr->resptime_samples;
			r /= 1000;
			sprintf(resp, "%u", r);
		}
		
		fprintf(fp, "%-24.24s %-15.15s %-8.8s %-10.10s %s\n",
			ptr->hostname,
			type,
			(ptr->status)? "UP": "DOWN",
			upstr,
			resp);
	}

	// close the file
	fclose(fp);
	
	return 0;
}
	
int cliClearNetworkMonitor(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	netmon_clear();
	resolver_update = 2;
	
	return 0;
}

int cliDebugMonitorInterval(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	shm->config.monitor_interval = atoi(argv[3]);

	netmon_clear();
	return 0;
}

//**************************************************************
// netmon configuration
//**************************************************************
static RcpNetmonHost *monitor_find(const char *hostname, unsigned char type) {
	RcpNetmonHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (strcmp(ptr->hostname, hostname) == 0 && ptr->type == type)
			return ptr;

	}
	
	return NULL;
}

static RcpNetmonHost *monitor_find_empty(void) {
	RcpNetmonHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}

int cliMonitorHost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// no form
	int noform = 0;
	int index = 2;	// hostname
	int index_type = 1;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
		index_type++;
	}
	
	unsigned char type = RCP_NETMON_TYPE_ICMP; // defaulting to ping
	if (strcmp(argv[index_type], "dns") == 0)
		type = RCP_NETMON_TYPE_DNS;
	else if (strcmp(argv[index_type], "ntp") == 0)
		type = RCP_NETMON_TYPE_NTP;
		

	// find if this is an existing monitor
	RcpNetmonHost *found = monitor_find(argv[index], type);
	
	// process command
	if (noform) {
		if (found == NULL)
			return 0;
			
		// remove monitor
		found->valid = 0;
	}
	else {
		// already there?
		if (found != NULL)
			return 0;

		// allocate new monitor
		RcpNetmonHost *newmonitor = monitor_find_empty();
		if (newmonitor == NULL) {
			strcpy(data, "Error: cannot configure MONITOR entry, limit reached");
			return RCPERR;
		}
		rcpNetmonHostClean(newmonitor);

		// copy the data
		strcpy(newmonitor->hostname, argv[index]);
		newmonitor->type = type;
		newmonitor->valid = 1;
	}		
	
	// start the resolver
	resolver_update = 2;
	if (type == RCP_NETMON_TYPE_ICMP)
		ping_flag_update();
	else if (type == RCP_NETMON_TYPE_NTP || type == RCP_NETMON_TYPE_DNS)
		udp_flag_update();
			
	return 0;
}

int cliMonitorPort(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
		// no form
	int noform = 0;
	int index = 2;	// hostname
	int index_type = 1;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
		index_type++;
	}
		
	// host type
	unsigned char type = RCP_NETMON_TYPE_TCP; // default to tcp
	if (strcmp(argv[index_type], "ssh") == 0)
		type = RCP_NETMON_TYPE_SSH;
	else if (strcmp(argv[index_type], "http") == 0)
		type = RCP_NETMON_TYPE_HTTP;
	else if (strcmp(argv[index_type], "smtp") == 0)
		type = RCP_NETMON_TYPE_SMTP;

	// find if this is an existing monitor
	RcpNetmonHost *found = monitor_find(argv[index], type);
	
	// process command
	if (noform) {
		if (found == NULL)
			return 0;
			
		// remove monitor
		found->valid = 0;
	}
	else {
		// already there?
		if (found != NULL) {
			if (argc == 4) {
				int port = atoi(argv[3]);
				found->port = (uint16_t) port;
			}
		}
		else {			
			// allocate new monitor
			RcpNetmonHost *newmonitor = monitor_find_empty();
			if (newmonitor == NULL) {
				strcpy(data, "Error: cannot configure MONITOR entry, limit reached");
				return RCPERR;
			}
			rcpNetmonHostClean(newmonitor);
	
			// copy the data
			strcpy(newmonitor->hostname, argv[index]);
			newmonitor->type = type;
			if (argc == 4) {
				int port = atoi(argv[3]);
				newmonitor->port = (uint16_t) port;
			}
			newmonitor->valid = 1;
		}
	}		
	
	// start the resolver
	resolver_update = 2;

	// update worker flags
	switch (type) {
		case RCP_NETMON_TYPE_TCP:
			tcp_flag_update();
			break;
		case RCP_NETMON_TYPE_SSH:
			ssh_flag_update();
			break;
		case RCP_NETMON_TYPE_HTTP:
			http_flag_update();
			break;
		case RCP_NETMON_TYPE_SMTP:
			smtp_flag_update();
			break;
		default:
			ASSERT(0);
	}
		
	return 0;
}

