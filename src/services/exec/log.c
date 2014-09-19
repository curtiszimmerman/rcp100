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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
int syslog_fd = 0;

// check if ptr is an integer number
static int isnum(char *ptr) {
	if (*ptr == '\0')
		return 0;
	
	while (*ptr != '\0') {
		if (!isdigit((int) *ptr))
			return 0;
		ptr++;
	}
	return 1;
}

int cliLoggingCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	if (strcmp(argv[0], "no") == 0) {
		shm->config.log_level = RLOG_LEVEL_DEFAULT;
		shm->config.facility = RLOG_FC_DEFAULT;
		shm->config.rate_limit = RLOG_RATE_DEFAULT;
		
		// clear all hosts
		int i;
		RcpSyslogHost *ptr;
		for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
			ptr->valid = 0;
		}
		return 0;
	}

	if (isnum(argv[1])) {
		shm->config.log_level = atoi(argv[1]);
		return 0;
	}
	
	if (strcmp(argv[1], "emergencies") == 0)
		shm->config.log_level = RLOG_EMERG;
	else if (strcmp(argv[1], "alerts") == 0)
		shm->config.log_level = RLOG_ALERT;
	else if (strcmp(argv[1], "critical") == 0)
		shm->config.log_level = RLOG_CRIT;
	else if (strcmp(argv[1], "errors") == 0)
		shm->config.log_level = RLOG_ERR;
	else if (strcmp(argv[1], "warning") == 0)
		shm->config.log_level = RLOG_WARNING;
	else if (strcmp(argv[1], "notifications") == 0)
		shm->config.log_level = RLOG_NOTICE;
	else if (strcmp(argv[1], "informational") == 0)
		shm->config.log_level = RLOG_INFO;
	else if (strcmp(argv[1], "debugging") == 0)
		shm->config.log_level = RLOG_DEBUG;
	
	return 0;
}

static uint64_t parse_facilities(char **argv, int index) {
	uint64_t mask = 0;
	if (strcmp(argv[index], "all") == 0)
		mask = 0xffffffffffffffffULL;
	else if (strcmp(argv[index], "configuration") == 0)
		mask |= RLOG_FC_CONFIG;
	else if (strcmp(argv[index], "interface") == 0)
		mask |= RLOG_FC_INTERFACE;
	else if (strcmp(argv[index], "router") == 0)
		mask |= RLOG_FC_ROUTER;
	else if (strcmp(argv[index], "admin") == 0)
		mask |= RLOG_FC_ADMIN;
	else if (strcmp(argv[index], "syscfg") == 0)
		mask |= RLOG_FC_SYSCFG;
	else if (strcmp(argv[index], "ipc") == 0)
		mask |= RLOG_FC_IPC;
	else if (strcmp(argv[index], "dhcp") == 0)
		mask |= RLOG_FC_DHCP;
	else if (strcmp(argv[index], "dns") == 0)
		mask |= RLOG_FC_DNS;
	else if (strcmp(argv[index], "rip") == 0)
		mask |= RLOG_FC_RIP;
	else if (strcmp(argv[index], "ipc") == 0)
		mask |= RLOG_FC_IPC;
	else if (strcmp(argv[index], "ntp") == 0)
		mask |= RLOG_FC_NTPD;
	else if (strcmp(argv[index], "monitor") == 0)
		mask |= RLOG_FC_NETMON;
	else if (strcmp(argv[index], "http") == 0)
		mask |= RLOG_FC_HTTP;
	else if (strcmp(argv[index], "ospf") == 0) {
		if (strcmp(argv[index + 1], "packets") == 0)
			mask |= RLOG_FC_OSPF_PKT;
		else if (strcmp(argv[index + 1], "drbdr") == 0)
			mask |= RLOG_FC_OSPF_DRBDR;
		else if (strcmp(argv[index + 1], "lsa") == 0)
			mask |= RLOG_FC_OSPF_LSA;
		else if (strcmp(argv[index + 1], "spf") == 0)
			mask |= RLOG_FC_OSPF_SPF;
	}
	
	return mask;
}

int cliLoggingFacilityCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	int index = 0;
	int no = 0;
	if (strcmp(argv[0], "no") == 0) {
		no = 1;
		index = 3;
	}
	else {
		no = 0;
		index = 2;
	}
	
	if (argc == 3 && no == 1) {
		shm->config.facility = RLOG_FC_DEFAULT;
		return 0;
	}

	uint64_t mask = parse_facilities(argv, index);
	if (no)
		shm->config.facility &= ~mask;
	else
		shm->config.facility |= mask;
		
	return 0;
}

int cliLoggingSnmpCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	int index = 0;
	int no = 0;
	if (strcmp(argv[0], "no") == 0) {
		no = 1;
		index = 3;
	}
	else {
		no = 0;
		index = 2;
	}
	
	if (argc == 3 && no == 1) {
		shm->config.snmp_facility = 0;
		return 0;
	}

	uint64_t mask = parse_facilities(argv, index);
	if (no)
		shm->config.snmp_facility &= ~mask;
	else
		shm->config.snmp_facility |= mask;
		
	return 0;
}

int cliLoggingRateCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	if (strcmp(argv[0], "no") == 0)
		shm->config.rate_limit = RLOG_RATE_DEFAULT;
	else
		shm->config.rate_limit = atoi(argv[2]);
	
	return 0;
}


void logSendCli(uint32_t proc, uint64_t fc, const char *data, uint8_t level) {
	const char *str_level = rcpLogLevel(level);

	int k;
	for (k = 0; k < MAX_SESSIONS; k++) {
		if (mux[k].socket == 0 || mux[k].ttyname[0] == '\0')
			continue;
		if (mux[k].terminal_monitor == 0)
			continue;
			
		FILE *fp = fopen (mux[k].ttyname, "w");
		if (fp == NULL) {
			continue;
		}
		fprintf(fp, "%s-%s: <%s> %s\n", rcpProcName(proc), rcpLogFacility(fc), str_level, data);
		fclose(fp);
	}
}

static void remove_last_message(void) {
	if (shm->log_tail == NULL || shm->log_head == shm->log_tail)
		return;
	
	RcpLogEntry *log = shm->log_tail;
	rcpFree(shm->log_smem_handle, log->data);
	ASSERT(log->prev != NULL);
	shm->log_tail = log->prev;
	log->prev->next = NULL;
	rcpFree(shm->log_smem_handle, log);
		
	shm->config.log_stored_messages--;
}

static void free_some_space(void) {
	// remove 10 messages
	int i;
	for (i = 0; i < 10; i++)
		remove_last_message();
}

#define DEFAULT_SYSLOG_PORT 514
int logInitSyslogSocket(void) {
	int sock;
	struct sockaddr_in host_address;
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		ASSERT(0);
		return 0;
	}

	memset((void*)&host_address, 0, sizeof(host_address));
	host_address.sin_family = PF_INET;
	host_address.sin_addr.s_addr = htonl(INADDR_ANY);
	host_address.sin_port = htons(RCP_PORT_LOGGER);
	
	int optval = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	if (bind(sock, (struct sockaddr*)&host_address, sizeof(host_address)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}
	return sock;
}

void logDistribute(const char *data, uint8_t level, uint64_t fc, uint32_t proc) {
	uint8_t old_level = shm->config.log_level;
	uint64_t old_facility = shm->config.facility;
	shm->config.current_rate++;
	if (rcpLogAccept(shm, level, fc)) {
	     	// store the log
		logStore(data, level, fc, proc);
		
		// send the log to cli
		logSendCli(proc, fc, data, level);
	}
	ASSERT(old_level == shm->config.log_level);
	ASSERT(old_facility == shm->config.facility);
}


void logStore(const char *data, uint8_t level, uint64_t fc, uint32_t proc) {
	RcpLogEntry *log = rcpAlloc(shm->log_smem_handle, sizeof(RcpLogEntry));
	if (log == NULL) {
		free_some_space();
		log = rcpAlloc(shm->log_smem_handle, sizeof(RcpLogEntry));
		if (log == NULL) {
			ASSERT(0);
			return;
		}
	}
	memset(log, 0, sizeof(RcpLogEntry));
	
	int dsize = strlen(data);
	char *shmdata = rcpAlloc(shm->log_smem_handle, dsize + 1);
	if (shmdata == NULL) {
		free_some_space();
		free_some_space();
		free_some_space();
		shmdata = rcpAlloc(shm->log_smem_handle, dsize + 1);
		if (shmdata == NULL) {
			ASSERT(0);
			rcpFree(shm->log_smem_handle, log);
			return;
		}
	}
	
	shm->config.log_stored_messages++;
	
	// fill in the structure
	log->ts = time(NULL);
	log->facility = fc;
	log->level = level;
	log->proc = proc;
	strcpy(shmdata, data);
	log->data = (unsigned char *) shmdata;
	
	// link in the structure
	if (shm->log_head == NULL) {
		ASSERT(shm->log_tail == NULL);
		shm->log_head = log;
		shm->log_tail = log;
		return;
	}
	log->next = shm->log_head;
	shm->log_head->prev = log;
	shm->log_head = log;
	
	// send log message to all syslog servers
	if (syslog_fd != 0) {
		// build the buffer
		const char *slevel = rcpLogLevel(level);
		const char *sfc = rcpLogFacility(fc);
		const char *sproc = rcpProcName(proc);
		char *buffer = malloc(strlen(shm->config.hostname) + strlen(sproc) + strlen(sfc) + strlen(slevel) + strlen(data) + 16);
		sprintf(buffer, "%s %s-%s: <%s> %s", shm->config.hostname, sproc, sfc, slevel, data);
		int len = strlen(buffer);					
	
	
		// send the message to all syslog hosts
		int i;
		RcpSyslogHost *ptr;
		for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
			if (!ptr->valid)
				continue;
	
			struct sockaddr_in host_address;
			memset((void*)&host_address, 0, sizeof(host_address));
			host_address.sin_family = PF_INET;
			host_address.sin_addr.s_addr = htonl(ptr->ip); //INADDR_ANY;
			host_address.sin_port=htons((ptr->port == 0)? DEFAULT_SYSLOG_PORT: ptr->port);
	
			if (sendto(syslog_fd, (unsigned char *) buffer, len, 0, (struct sockaddr*)&host_address, sizeof(struct sockaddr)) <= 0) {
				ASSERT(0);
				return;
			}
		}
		free(buffer);
	}
	
	// send snmp trap
	if (shm->config.snmp.traps_enabled && (fc & shm->config.snmp_facility)) {
		// build snmp timestamp
		struct tm *tm_log = localtime(&log->ts);
		if (tm_log) {
			char tstamp[20];
			int year = tm_log->tm_year % 100;
			sprintf(tstamp, "%04x%02x%02x%02x%02x%02x",
				year, tm_log->tm_mon, tm_log->tm_mday,
				tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec);

			char msg[1000];
			// restrict the message to the first 300 chars
			sprintf(msg, ".1.3.6.1.2.1.192.0.1 " //syslogMsgNotification
				".1.3.6.1.2.1.192.1.2.1.2 i 16 " //syslogMsgFacility - this is UNIX syslog facility
				".1.3.6.1.2.1.192.1.2.1.3 i %d " //syslogMsgSeverity
				".1.3.6.1.2.1.192.1.2.1.5 x %s " //syslogMsgTimeStamp
				".1.3.6.1.2.1.192.1.2.1.6 s \"%s\" " //syslogMsgHostName
				" .1.3.6.1.2.1.192.1.2.1.7 s \"%s\" " //syslogMsgAppName
				".1.3.6.1.2.1.192.1.2.1.9 s \"%s\" " //syslogMsgMsgID - this is RCP facility
				".1.3.6.1.2.1.192.1.2.1.11 s \"%-300s\"", //syslogMsgMsg
				level,
				tstamp,
				shm->config.hostname,
				rcpProcName(proc),
				rcpLogFacility(fc),
				data);
		
			// send trap
			rcpSnmpSendTrap(msg);
		}
		else
			ASSERT(0);
	}
}
			



int cliTerminalMonitorCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	if (strcmp(argv[0], "no") == 0)
		mux[muxsession].terminal_monitor = 0;
	else
		mux[muxsession].terminal_monitor = 1;
	return 0;
}

int cliClearLoggingCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	

	RcpLogEntry *log = shm->log_head;
	
	while (log != NULL) {
		RcpLogEntry *next = log->next;
		rcpFree(shm->log_smem_handle, log->data);
		rcpFree(shm->log_smem_handle, log);
		shm->config.log_stored_messages--;
		log = next;
	}
	shm->log_head = NULL;
	shm->log_tail = NULL;
	ASSERT(shm->config.log_stored_messages == 0);
	
	return 0;
}


//******************************************************************
// syslog hosts
//******************************************************************
static RcpSyslogHost *host_find(uint32_t ip) {
	int i;
	RcpSyslogHost *ptr;
	for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ip == ptr->ip)
			return ptr;
	}
	
	return NULL;
}

static RcpSyslogHost *host_find_empty() {
	int i;
	RcpSyslogHost *ptr;
	for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}

int cliNoLoggingHostAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	int i;
	RcpSyslogHost *ptr;
	for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++)
		ptr->valid = 0;
	return 0;
}

int cliLoggingHostCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	
	// no command
	if (strcmp("no", argv[0]) == 0) {
		// extract address
		uint32_t ip;
		if (atoip(argv[3], &ip)) {
			strcpy(data, "Error: invalid IP address");
			return RCPERR;
		}
		
		RcpSyslogHost *found = host_find(ip);
		if (found == NULL)
			return 0;
		found->valid = 0;
	}
	else {
		// extract address and port
		uint32_t ip;
		if (atoip(argv[2], &ip)) {
			strcpy(data, "Error: invalid IP address");
			return RCPERR;
		}
		
		uint16_t port = 0;
		if (argc == 5) {
			int p = atoi(argv[4]);
			port = (uint16_t) p;
		}
		
		RcpSyslogHost *found = host_find(ip);
		if (found != NULL) {
			// modify existing host
			found->port = port;
			return 0;
		}
		
		// set a new host
		RcpSyslogHost *newhost = host_find_empty();
		if (newhost == NULL) {
			strcpy(data, "Error: cannot configure host, limit reached\n");
			return RCPERR;
		}
		memset(newhost, 0, sizeof(RcpSyslogHost));
		newhost->ip = ip;
		newhost->port = port;
		newhost->valid = 1;
	}

	return 0;
}

int cliShowLoggingBufferCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	// grab a temporary transport file
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	// set facility
	uint64_t facility = 0xffffffffffffffffULL;
	if (argc == 4) {
		if (strcmp(argv[3], "monitor") == 0)
			facility = RLOG_FC_NETMON;
	}
	
	// write the data
	RcpLogEntry *ptr = shm->log_head;
	char *month[] = {
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec"
	};
	
	while (ptr != NULL) {
		ASSERT(ptr->ts != 0);
		if (ptr->ts != 0) {
			if (ptr->facility & facility) {
				struct tm *tm_log = localtime(&ptr->ts);
				const char *level = rcpLogLevel(ptr->level);
				const char *fc = rcpLogFacility(ptr->facility);
				const char *proc = rcpProcName(ptr->proc);
				
				int len = strlen((char *) ptr->data);
				if (len != 0) {
					fprintf(fp, "%s %d %d:%02d:%02d %s-%s (%s):\n    %s\n",
						month[tm_log->tm_mon], 
						tm_log->tm_mday,
						tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec,
						proc, fc, level, ptr->data);					
				}
				else {
					ASSERT(0);
					fprintf(stderr, "level #%s#, facility #%s#, proc #%s#, data #%s#\n",
						level, fc, proc, ptr->data);
				}
			}
		}

		ptr = ptr->next;	
	}

	// close file
	fclose(fp);

	return 0;
}


int cliShowLoggingCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	// grab a temporary transport file
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	// logging level
	fprintf(fp, "Logging level %d (%s)\n", shm->config.log_level, rcpLogLevel(shm->config.log_level));
	
	// debug facilities
	if (shm->config.log_level == RLOG_DEBUG)
		fprintf(fp, "Logging facilities: all\n");
	else if (shm->config.facility) {
		fprintf(fp, "Debug logging facilities:\n");
		int i;
		uint64_t mask = 1;
		for (i = 0; i < 64; i++, mask <<= 1) {
			if (mask & shm->config.facility && strlen(rcpLogFacility(mask)) != 0)
				fprintf(fp, "   %s\n", rcpLogFacility(mask));
		}
	}
	
	// rate limit
	fprintf(fp, "Logging rate limit %d\n", shm->config.rate_limit);
	
	// logging hosts
	int i;
	RcpSyslogHost *ptr;
	int found = 0;
	for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		found = 1;
		break;
	}
	if (found) {
		fprintf(fp, "Logging hosts:\n");
		for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
			if (!ptr->valid)
				continue;
	
			if (ptr->port == 0)
				fprintf(fp, "   %d.%d.%d.%d\n", RCP_PRINT_IP(ptr->ip));
			else
				fprintf(fp, "   %d.%d.%d.%d port %u\n", RCP_PRINT_IP(ptr->ip), ptr->port);
		}
	}
	
	if (shm->config.snmp_facility) {
		fprintf(fp, "SNMP logging facilities:\n");
		int i;
		uint64_t mask = 1;
		for (i = 0; i < 64; i++, mask <<= 1) {
			if (mask & shm->config.snmp_facility && strlen(rcpLogFacility(mask)) != 0)
				fprintf(fp, "   %s\n", rcpLogFacility(mask));
		}
	}
	
	// close file
	fclose(fp);
	
	return 0;
}
