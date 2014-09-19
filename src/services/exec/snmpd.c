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
#include <sys/stat.h>
#include <fcntl.h>

int restart_snmpd = 0;

void kill_snmpd(void) {
	int v = system("pkill snmpd");
	if (v == -1)
		ASSERT(0);
}

static int cfg_empty(void) {
	if (shm->config.snmp.com_public)
		return 0;
	
	if (shm->config.snmp.com_passwd[0] != '\0')
		return 0;

	int i;
	for (i = 0; i < RCP_SNMP_USER_LIMIT; i++) {
		if (shm->config.snmp.user[i].name[0] != '\0')
			return 0;
	}
	
	return 1;
}


void start_snmpd(void) {
	// if no snmp configuration, do nothing
	if (cfg_empty())
		return;

	// build the config file
	FILE *fp = fopen("/opt/rcp/var/transport/snmpd.conf", "w");
	if (fp == NULL) {
		logDistribute("cannot open /opt/rcp/var/transport/snmpd.conf", RLOG_ERR, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
		return;		
	}

	// SNMP v1 and v2 
	ASSERT(!(shm->config.snmp.com_public && shm->config.snmp.com_passwd[0] != '\0'));
	if (shm->config.snmp.com_public) {
		fprintf(fp, "com2sec secname1 default public\n");
		fprintf(fp, "group MyROGroup	v1 secname1\n");
		fprintf(fp, "group MyROGroup	v2c secname1\n");
		fprintf(fp, "view all    included  .1 80\n");
		fprintf(fp, "access MyROGroup \"\"      any       noauth    exact  all    none   none\n");
	}
	else if (shm->config.snmp.com_passwd[0] != '\0') {
		fprintf(fp, "com2sec secname1 default %s\n", shm->config.snmp.com_passwd);
		fprintf(fp, "group MyROGroup	v1 secname1\n");
		fprintf(fp, "group MyROGroup	v2c secname1\n");
		fprintf(fp, "view all    included  .1 80\n");
		fprintf(fp, "access MyROGroup \"\"      any       noauth    exact  all    none   none\n");
	}
	
	// SNMP v3
	int i;
	for (i = 0; i < RCP_SNMP_USER_LIMIT; i++) {
		if (shm->config.snmp.user[i].name[0] != '\0') {
			fprintf(fp, "rouser %s authnopriv .1\n", shm->config.snmp.user[i].name);
			fprintf(fp, "createUser %s MD5 %s\n", 
				shm->config.snmp.user[i].name, 
				shm->config.snmp.user[i].passwd);
		}
	}
	
	// contact and location
	if (shm->config.snmp.contact[0] != '\0')
		fprintf(fp, "sysContact %s\n", shm->config.snmp.contact);
	else
		fprintf(fp, "sysContact RCP administrator\n");
	
	if (shm->config.snmp.location[0] != '\0')
		fprintf(fp, "sysLocation %s\n", shm->config.snmp.location);
	else
		fprintf(fp, "sysLocation Earth\n");
	
	
	fclose(fp);
	
	// start snmpd
	int v = system("/opt/rcp/sbin/snmpd -u rcp -Lf /opt/rcp/var/log/snmpd.log -c /opt/rcp/var/transport/snmpd.conf");
	if (v == -1)
		ASSERT(0);
}


//******************************************************************
// SNMP
//******************************************************************
int cliSnmpServerCommunity(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.snmp.com_public = 0;
		memset(shm->config.snmp.com_passwd, 0, CLI_SNMP_MAX_COMMUNITY + 1);
	}
	else {
		if (strcmp(argv[2], "public") == 0) {
			shm->config.snmp.com_public = 1;
			memset(shm->config.snmp.com_passwd, 0, CLI_SNMP_MAX_COMMUNITY + 1);
		}
		else {
			shm->config.snmp.com_public = 0;
			memset(shm->config.snmp.com_passwd, 0, CLI_SNMP_MAX_COMMUNITY + 1);
			strncpy(shm->config.snmp.com_passwd, argv[2], CLI_SNMP_MAX_COMMUNITY);
		}		
	}

	// restart snmpd
	restart_snmpd = SNMPD_TIMER_INIT;

	return 0;
}

static RcpSnmpUser *find_user(const char *user) {
	RcpSnmpUser *ptr;
	int i;

	for (i = 0, ptr = shm->config.snmp.user; i <  RCP_SNMP_USER_LIMIT; i++, ptr++) {
		if (ptr->name[0] == '\0')
			continue;
			
		if (strcmp(ptr->name, user) == 0)
			return ptr;
	}
	
	return NULL;
}

static RcpSnmpUser *find_empty_user(void) {
	RcpSnmpUser *ptr;
	int i;

	for (i = 0, ptr = shm->config.snmp.user; i <  RCP_SNMP_USER_LIMIT; i++, ptr++) {
		if (ptr->name[0] == '\0')
			return ptr;
	}
	
	return NULL;
}

int cliNoSnmpServerUserAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	RcpSnmpUser *ptr;
	int i;

	for (i = 0, ptr = shm->config.snmp.user; i <  RCP_SNMP_USER_LIMIT; i++, ptr++) {
		memset(ptr->name, 0, CLI_MAX_ADMIN_NAME + 1);
		memset(ptr->passwd, 0, CLI_SNMP_MAX_PASSWD + 1);
	}
	return 0;
}

int cliSnmpServerUser(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int noform = 0;
	char *user = argv[2];
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		user = argv[3];
	}
	
	// find an existing user
	RcpSnmpUser *found = find_user(user);
	
	// modify shared memory
	if (noform) {
		if (found)
			memset(found, 0, sizeof(RcpSnmpUser));
	}
	else {
		if (found) {
			memset(found->passwd, 0, CLI_SNMP_MAX_PASSWD + 1);
			strncpy(found->passwd, argv[4], CLI_SNMP_MAX_PASSWD);
		}
		else {
			// create a new user
			RcpSnmpUser *newuser = find_empty_user();
			if (newuser == NULL) {
				sprintf(data, "Error: cannot set SNMP user, limit reached\n");
				return RCPERR;
			}
			memset(newuser, 0, sizeof(RcpSnmpUser));
			strncpy(newuser->name, argv[2], CLI_MAX_ADMIN_NAME);
			strncpy(newuser->passwd, argv[4], CLI_SNMP_MAX_PASSWD);
		}
	}
	
	// restart snmpd
	restart_snmpd = SNMPD_TIMER_INIT;
	
	return 0;
}

int cliSnmpServerContact(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		memset(shm->config.snmp.contact, 0, RCP_SNMP_MAX_CONTACT + 1);
	}
	else {
		memset(shm->config.snmp.contact, 0, RCP_SNMP_MAX_CONTACT + 1);
		strncpy(shm->config.snmp.contact, argv[2], RCP_SNMP_MAX_CONTACT);
	}

	// restart snmpd
	restart_snmpd = SNMPD_TIMER_INIT;

	return 0;
}


int cliSnmpServerLocation(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		memset(shm->config.snmp.location, 0, RCP_SNMP_MAX_LOCATION + 1);
	}
	else {
		memset(shm->config.snmp.location, 0, RCP_SNMP_MAX_LOCATION + 1);
		strncpy(shm->config.snmp.location, argv[2], RCP_SNMP_MAX_LOCATION);
	}

	// restart snmpd
	restart_snmpd = SNMPD_TIMER_INIT;

	return 0;
}

// traps
int cliSnmpServerEnableTraps(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.snmp.traps_enabled = 0;
	}
	else {
		shm->config.snmp.traps_enabled = 1;

		// send coldstart trap
		if (!shm->config.snmp.coldstart_trap_sent) {
			rcpSnmpSendTrap(".1.3.6.1.6.3.1.1.5.1");
			shm->config.snmp.coldstart_trap_sent = 1;
		}
	}

	return 0;
}

static RcpSnmpHost *host_find_ip(uint32_t ip) {
	RcpSnmpHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.snmp.host; i < RCP_SNMP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->ip == 0)
			continue;
			
		if (ptr->ip == ip)
			return ptr;
	}
	
	return NULL;
}

static RcpSnmpHost *host_find_empty(void) {
	RcpSnmpHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.snmp.host; i <  RCP_SNMP_HOST_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;
	}
	
	return NULL;
}

int cliNoSnmpServerHostAll(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	RcpSnmpHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.snmp.host; i <  RCP_SNMP_HOST_LIMIT; i++, ptr++) {
		ptr->valid = 0;
	}
	return 0;
}	

int cliSnmpServerHost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int no = 0;
	if (strcmp(argv[0], "no") == 0)
		no = 1;
	
	// extract data
	uint32_t ip;
	if (atoip((no)? argv[3]: argv[2], &ip)) {
		sprintf(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// find an existing host
	RcpSnmpHost *found = host_find_ip(ip);
	
	// store value in shared memory
	if (strcmp(argv[0], "no") == 0) {
		if (!found)
			return 0;	// nothing to do
		found->valid = 0;
	}
	else {
		if (found) {
			// copy the new community string
			memset(found->com_passwd, 0, CLI_SNMP_MAX_COMMUNITY + 1);
			strncpy(found->com_passwd, argv[6], CLI_SNMP_MAX_COMMUNITY);
			if (strcmp(argv[3], "informs") == 0)
				found->inform = 1;
			else
				found->inform = 0;
			return 0;
		}
		
		// create a new host
		found = host_find_empty();
		if (!found) {
			sprintf(data, "Error: cannot create host, limit reached\n");
			return RCPERR;
		}
		
		found->valid = 1;
		found->ip = ip;
		memset(found->com_passwd, 0, CLI_SNMP_MAX_COMMUNITY + 1);
		strncpy(found->com_passwd, argv[6], CLI_SNMP_MAX_COMMUNITY);
		if (strcmp(argv[3], "informs") == 0)
			found->inform = 1;
		else
			found->inform = 0;
	}

	return 0;
}


int cliShowSnmp(CliMode mode, int argc, char **argv) {
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

	// open snmpstats file
	RcpSnmpStats rstats;
	memset(&rstats, 0, sizeof(rstats));
	
	int fd = open(RCPSNMP_STATS_FILE, O_RDONLY);
	if (fd != -1) {
		int v = read(fd, &rstats, sizeof(rstats));
		if (v == -1)
			ASSERT(0);
		close(fd);
	}
	
	fprintf(fp, "%u input packets\n", rstats.inpkts);
	fprintf(fp, "\tGet-Request PDU: %u\n", rstats.ingetrq);
	fprintf(fp, "\tGet-Next PDU: %u\n", rstats.innextrq);
	fprintf(fp, "\tSet-Request PDU: %u\n", rstats.insetrq);

	if (rstats.inerrbadver || rstats.inerrbadcomname || rstats.inerrbadcomuses || rstats.inerrasn) {
		fprintf(fp, "\nInput Errors:\n");
		fprintf(fp, "\tBad version: %u\n", rstats.inerrbadver);
		fprintf(fp, "\tBad community name: %u\n", rstats.inerrbadcomname);
		fprintf(fp, "\tBad community uses: %u\n", rstats.inerrbadcomuses);
		fprintf(fp, "\tASN.1 parse errors: %u\n", rstats.inerrasn);
	}
	
	fprintf(fp, "\n%u output packets\n", rstats.outpkts);
	fprintf(fp, "\tGet-Response PDU: %u\n", rstats.outgetresp);
	fprintf(fp, "\tTraps: %u\n", shm->stats.snmp_traps_sent);
	
	if (rstats.outerrtoobig || rstats.outerrnosuchname || rstats.outerrbadvalue || rstats.outerrgenerr) {
		fprintf(fp, "\t'tooBig' errors: %u\n", rstats.outerrtoobig);
		fprintf(fp, "\t'noSuchName' errors: %u\n", rstats.outerrnosuchname);
		fprintf(fp, "\t'badValue' errors: %u\n", rstats.outerrbadvalue);
		fprintf(fp, "\t'getErr' errors: %u\n", rstats.outerrgenerr);
	}
		
		
	// close file
	fclose(fp);

	return 0;
}
