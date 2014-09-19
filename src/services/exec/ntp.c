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

volatile int update_ntpd_conf = 0;


//******************************************************************
// NTP external servers
//******************************************************************
static RcpNtpServer *ntp_find_hostname(const char *hostname) {
	RcpNtpServer *ptr;
	int i;

	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (*ptr->name == '\0')
			continue;
			
		if (strcmp(ptr->name, hostname) == 0)
			return ptr;
	}
	
	return NULL;
}

static RcpNtpServer *ntp_find_ip(uint32_t ip) {
	RcpNtpServer *ptr;
	int i;

	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->ip == 0)
			continue;
			
		if (ptr->ip == ip)
			return ptr;
	}
	
	return NULL;
}

static RcpNtpServer *ntp_find_empty(void) {
	RcpNtpServer *ptr;
	int i;

	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}

static void ntp_clean_preferred(void) {
	RcpNtpServer *ptr;
	int i;

	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		ptr->preferred = 0;
	}
}

int cliNtpServerHostname(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// no form?
	int no = 0;
	if (strcmp(argv[0], "no") == 0)
		no = 1;
	
	// find existing server
	RcpNtpServer *found = (no)? ntp_find_hostname(argv[3]): ntp_find_hostname(argv[2]);
	
	// add or remove
	if (no) {
		if (found == NULL)
			return 0; // nothing to do
		found->valid = 0;
	}
	else {
		if (found) {
			// preferred
			if (argc == 4) {
				ntp_clean_preferred();
				found->preferred = 1;
			}
			
			return 0;
		}

		// grab a new entry and set it
		found =  ntp_find_empty();
		if (found == NULL) {
			sprintf(data, "Error: cannot set NTP server, limit reached\n");
			return RCPERR;
		}
		strcpy(found->name, argv[2]);
		found->ip = 0;
		found->valid = 1;
		
		// preferred
		if (argc == 4) {
			ntp_clean_preferred();
			found->preferred = 1;
		}
	}
	
	// update ntpd.conf file
	update_ntpd_conf = 2;
	return 0;
}

int cliNtpServerIp(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// no form?
	int no = 0;
	if (strcmp(argv[0], "no") == 0)
		no = 1;
	
	// extract data
	uint32_t ip;
	if (atoip((no)? argv[3]: argv[2], &ip)) {
		sprintf(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	// find existing server
	RcpNtpServer *found = ntp_find_ip(ip);
	
	// add or remove
	if (no) {
		if (found == NULL)
			return 0; // nothing to do
		found->valid = 0;
	}
	else {
		if (found) {
			// preferred
			if (argc == 4) {
				ntp_clean_preferred();
				found->preferred = 1;
			}
			
			return 0;
		}
		
		// grab a new entry and set it
		found =  ntp_find_empty();
		if (found == NULL) {
			sprintf(data, "Error: cannot set NTP server, limit reached\n");
			return RCPERR;
		}
		*found->name = '\0';
		found->ip = ip;
		found->valid = 1;

		// preferred
		if (argc == 4) {
			ntp_clean_preferred();
			found->preferred = 1;
		}
	}
	
	// update ntpd.conf file
	update_ntpd_conf = 2;
	return 0;
}

int cliNoNtpServer(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	RcpNtpServer *ptr;
	int i;

	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		ptr->valid = 0;
	}

	// update ntpd.conf file
	update_ntpd_conf = 2;
	return 0;
}

//******************************************************************
// ip ntp server
//******************************************************************
int cliIpNtpServer(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// no form?
	int no = 0;
	if (strcmp(argv[0], "no") == 0)
		no = 1;

	if (no)
		shm->config.ntp_server_enabled = 0;
	else
		shm->config.ntp_server_enabled = 1;
	
	// update ntpd.conf file
	update_ntpd_conf = 2;
	return 0;
}


int cliShowNtpAssociations(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	fprintf(fp, "%-25.25s %-11.11s %-7.7s  %-10.10s  %-10.10s  %-8.8s\n",
		"Peer", "Status", "Stratum", "Offset", "Delay", "Interval");
	
	int i;
	RcpNtpServer *ptr = NULL;
	for (i = 0, ptr = &shm->config.ntp_server[0]; i < RCP_NTP_LIMIT; i++, ptr++) {
		if (ptr->valid == 0)
			continue;
		
		// ip or hostname
		char server[26];
		if (ptr->ip != 0)
			sprintf(server, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->ip));
		else
			sprintf(server, "%-25.25s", ptr->name);
		
		// server status
		char status[11];
		if (ptr->trustlevel < 2)
			strcpy(status, "inactive");
		else if (ptr->trustlevel < 5)
			strcpy(status, "pathetic");
		else if (ptr->trustlevel < 8)
			strcpy(status, "agresive");
		else
			strcpy(status, "active");

		// stratum
		char stratum[20];
		sprintf(stratum, "%d", ptr->stratum);
		if (ptr->trustlevel < 2)
			*stratum = '\0';
		
		// offset
		char offset[20];
		sprintf(offset, "%fs", ptr->offset);
		if (ptr->trustlevel < 2)
			*offset = '\0';		
		// delay
		char delay[20];
		sprintf(delay, "%fs", ptr->delay);
		if (ptr->trustlevel < 2)
			*delay = '\0';
					
		// interval
		char interval[20];
		if (ptr->interval == 0)
			*interval = '\0';
		else
			sprintf(interval, "%ds", (int) ptr->interval);

		fprintf(fp, "%-25.25s %-11.11s %-7.7s  %-10.10s  %-10.10s  %-8.8s\n",
			server, status, stratum, offset, delay, interval);
	}
	
	fclose(fp);
	return 0;

}
	
int cliShowNtpStatus(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (shm->config.ntp_synced)
		fprintf(fp, "Clock is synchronized, stratum %d\n",
			shm->config.ntp_stratum);
	else
		fprintf(fp, "Clock is not synchronized\n");
	
	if (shm->config.ntp_refid != 0 && shm->config.ntp_reftime != 0)
		fprintf(fp, "Reference is %d.%d.%d.%d, reference time is %f\n", 
			RCP_PRINT_IP(shm->config.ntp_refid), shm->config.ntp_reftime);
	
	if (shm->config.ntp_offset != 0 && shm->config.ntp_rootdelay != 0)
		fprintf(fp, "Clock offset is %fs, root delay is %fs\n",
			shm->config.ntp_offset, shm->config.ntp_rootdelay);

	if (shm->config.ntp_rootdispersion != 0)
		fprintf(fp, "Root dispersion is %f\n", shm->config.ntp_rootdispersion);
	
	fclose(fp);
	return 0;

}
	
