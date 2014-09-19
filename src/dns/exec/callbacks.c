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
#include "dns.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static RcpPkt *pktout = NULL;

//******************************************************************
// message to services
//******************************************************************
static void trigger_dns_update(void) {
	// send as system update message to services
	if (muxsock) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_IPC,
			"sending %s packet", rcpPktType(RCP_PKT_TYPE_UPDATEDNS));

		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = RCP_PROC_SERVICES;
		pkt.type = RCP_PKT_TYPE_UPDATEDNS;
		
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

int cliIpDnsServer(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// save data in shared memory
	if (strcmp(argv[0], "no") == 0) {
		shm->config.dns_server = 0;
		shm->config.dns_rate_limit = 0;
	}
	else {
		shm->config.dns_server = 1;
		if (argc == 5) {
			unsigned rl;
			sscanf(argv[4], "%u", &rl);
			shm->config.dns_rate_limit = rl;
		}
	}

	// open/close sockets
	if (client_sock == 0 && rq_active() == NULL && shm->config.dns_server == 1) {
		// force a process restart - sockets can only be opened before privileges are dropped
		force_restart = 1;
	}
	else if (client_sock != 0 && rq_active() && shm->config.dns_server == 0) {
		// close
		close(client_sock);
		client_sock = 0;
		rq_clear_all();
	}
	
	// trigger an update of /etc/resolv.conf
	trigger_dns_update();
	
	return 0;
}

int cliShowIpHost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// grab a temporary transport file
	FILE *fp = rcpJailTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	// write the data
	cache_print(fp);
	
	// close file
	fclose(fp);
	return 0;
}

int cliClearIpHost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// clear all cached entries
	cache_clear();
	
	// bring back the static entries
	cache_update_static();

	return 0;
}

int cliShowIpDnsStats(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// grab a temporary transport file
	FILE *fp = rcpJailTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (proxy_disabled) {		
		fprintf(fp, "An external DNS proxy is already running on the system\nRCP DNS proxy has been disabled\n");
	}
	else {
		fprintf(fp, "DNS queries: %u packets\n", shm->stats.dns_queries);
		fprintf(fp, "DNS answers: %u packets\n", shm->stats.dns_answers);
		fprintf(fp, "DNS cached answers: %u packets\n", shm->stats.dns_cached_answers);
		fprintf(fp, "RR types:\n");
		fprintf(fp, "\tA: %u packets\n", shm->stats.dns_a_answers);
		fprintf(fp, "\tCNAME: %u packets\n", shm->stats.dns_cname_answers);
		fprintf(fp, "\tPTR: %u packets\n", shm->stats.dns_ptr_answers);
		fprintf(fp, "\tMX: %u packets\n", shm->stats.dns_mx_answers);
		fprintf(fp, "\tAAAA (IPv6): %u packets\n", shm->stats.dns_aaaa_answers);
		fprintf(fp, "Request queue %d\n", rq_count());
		
		if (shm->stats.dns_err_rate_limit ||
		     shm->stats.dns_err_rx ||
		     shm->stats.dns_err_tx ||
		     shm->stats.dns_err_malformed ||
		     shm->stats.dns_err_unknown ||
		     shm->stats.dns_err_no_server ||
		     shm->stats.dns_err_rq_timeout) {
			fprintf(fp, "Errors:\n");
			if (shm->stats.dns_err_rate_limit)
				fprintf(fp, "\tRate limit exceeded: %u packets\n", shm->stats.dns_err_rate_limit);
			if (shm->stats.dns_err_rx)
				fprintf(fp, "\tReceive error: %u packets\n", shm->stats.dns_err_rx);
			if (shm->stats.dns_err_tx)
				fprintf(fp, "\tTransmit error: %u packets\n", shm->stats.dns_err_tx);
			if (shm->stats.dns_err_malformed)
				fprintf(fp, "\tMalformed: %u packets\n", shm->stats.dns_err_malformed);
			if (shm->stats.dns_err_unknown)
				fprintf(fp, "\tUnknown partner: %u packets\n", shm->stats.dns_err_unknown);
			if (shm->stats.dns_err_no_server)
				fprintf(fp, "\tNo server configured: %u packets\n", shm->stats.dns_err_no_server);
			if (shm->stats.dns_err_rq_timeout)
				fprintf(fp, "\tRequest timeout: %u packets\n", shm->stats.dns_err_rq_timeout);
		}
	}
	
	// close file
	fclose(fp);
	return 0;
}

int cliClearIpDnsStats(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	shm->stats.dns_queries = 0;
	shm->stats.dns_answers = 0;
	shm->stats.dns_cached_answers = 0;
	shm->stats.dns_cname_answers = 0;
	shm->stats.dns_a_answers = 0;
	shm->stats.dns_aaaa_answers = 0;
	shm->stats.dns_ptr_answers = 0;
	shm->stats.dns_mx_answers = 0;
	// errors
	shm->stats.dns_err_rate_limit = 0;
	shm->stats.dns_err_rx = 0;
	shm->stats.dns_err_tx = 0;
	shm->stats.dns_err_malformed = 0;
	shm->stats.dns_err_unknown = 0;
	shm->stats.dns_err_no_server = 0;
	shm->stats.dns_err_rq_timeout = 0;

	// mark lats stats query as 0 
	shm->stats.dns_last_q = 0;
	return 0;
}

int cliDebugDnsShutdown(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	force_shutdown = 1;	

	return 0;
}

	
