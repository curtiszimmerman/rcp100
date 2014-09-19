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
#include <pwd.h>
#include <grp.h>
#include "librcp.h"

static char rcp_proc_name[1000] = {'\0'};

static void set_proc_name(void) {
	// if already set, do nothing
	if (*rcp_proc_name != '\0')
		return;
	
	// set the process name	
	memset(rcp_proc_name, 0, 1000);
	int plen = readlink("/proc/self/exe", rcp_proc_name, 1000 - 1);
	if (plen == -1)
		*rcp_proc_name = '\0';
}

const char *rcpGetProcName(void) {
	set_proc_name();
	return rcp_proc_name;
}

// drop privileges
void rcpDropPriv(void) {
	if (!getuid() || !geteuid()) {
		struct passwd *pwd = getpwnam("rcp");
		if (pwd == NULL) {
			fprintf(stderr, "Error: process %s, cannot access user rcp, exiting...\n", rcpGetProcName());
			exit(1);
		}

		// chroot into /opt/rcp/var/transport
		if (chdir("/opt/rcp/var/transport") == -1) {
			fprintf(stderr, "Error: process %s, cannot access transport directory, exiting...\n", rcpGetProcName());
			exit(1);
		}
		if (chroot("/opt/rcp/var/transport") == -1) {
			fprintf(stderr, "Error: process %s, cannot chroot into transport directory, exiting...\n", rcpGetProcName());
			exit(1);
		}

		// change user
		initgroups(pwd->pw_name, pwd->pw_gid);
		setgid(pwd->pw_gid);
		setuid(pwd->pw_uid);
		seteuid(0);   /* this should fail */
		if (!getuid() || !geteuid()) {
			fprintf(stderr, "Error: process %s, cannot drop privileges,  exiting...\n", rcpGetProcName());
			exit(1);
		}
	}
	else {
		fprintf(stderr, "Error: process %s, process not running as root, exiting...\n", rcpGetProcName());
		exit(1);
	}
}

// access process statistics in shared memory
RcpProcStats *rcpProcStats(RcpShm *shm, uint32_t process) {
	ASSERT(shm != NULL);
	ASSERT(process != 0);
	int i;

	// find an existing process
	for (i = 0; i < (RCP_PROC_MAX + 2); i++) {
		if (process == shm->config.pstats[i].proc_type)
			return &shm->config.pstats[i];
	}
	
	// return first empty structure available
	for (i = 0; i < (RCP_PROC_MAX + 2); i++) {
		if (0 == shm->config.pstats[i].proc_type)
			return &shm->config.pstats[i];
	}
	
	return NULL;
}

RcpModule *rcpSetModule(RcpShm *shm, const char *name, const char *version) {
	ASSERT(shm);
	ASSERT(name);
	ASSERT(version);
	
	int i;
	for (i = 0; i < RCP_MODULE_LIMIT; i++) {
		if (shm->config.module[i].name[0] == '\0') {
			strncpy(shm->config.module[i].name, name,  RCP_MODULE_NAME_LEN); 
			strncpy(shm->config.module[i].version, version,  RCP_MODULE_VERSION_LEN);
			return &shm->config.module[i];
		}
	}
	
	return NULL;
}

int rcpGetStatsInterval(void) {
	// current interval
	time_t t = time(NULL);
	struct tm *local = localtime(&t);
	int interval = local->tm_hour * 4 + local->tm_min / 15;

	int prev_interval = interval - 1;
	if (prev_interval < 0)
		prev_interval =  RCP_MAX_STATS - 1;

	return prev_interval;
}

void rcpSnmpSendTrap(const char *msg) {
	if (msg == NULL) {
		ASSERT(0);
		return;
	}
	
	int len = strlen(msg);
	len += 200;
	char cmd[len];
	RcpShm *shm = rcpGetShm();
	if (!shm->config.snmp.traps_enabled)
		return;
		
	int i;
	RcpSnmpHost *ptr;
	for (i = 0, ptr = shm->config.snmp.host; i <  RCP_SNMP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		if (ptr->inform)
			sprintf(cmd, "/opt/rcp/bin/snmpinform -v 2c -c %s %d.%d.%d.%d \"\" %s &",
				ptr->com_passwd, RCP_PRINT_IP(ptr->ip), msg);
		else
			sprintf(cmd, "/opt/rcp/bin/snmptrap -v 2c -c %s %d.%d.%d.%d \"\" %s &",
				ptr->com_passwd, RCP_PRINT_IP(ptr->ip), msg);
		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
		shm->stats.snmp_traps_sent++;
	}
}

int rcpSnmpAgentConfigured(void) {
	RcpShm *shm = rcpGetShm();
	ASSERT(!(shm->config.snmp.com_public && shm->config.snmp.com_passwd[0] != '\0'));
	
	// SNMP v1 and v2c
	if (shm->config.snmp.com_public)
		return 1;
	else if (shm->config.snmp.com_passwd[0] != '\0')
		return 1;

	// SNMP v3
	int i;
	for (i = 0; i < RCP_SNMP_USER_LIMIT; i++) {
		if (shm->config.snmp.user[i].name[0] != '\0')
			return 1;
	}
	
	return 0;	
}
