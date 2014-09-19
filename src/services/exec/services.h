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
#ifndef SERVICES_H
#define SERVICES_H

#include "librcp.h"

// main.c
extern CliNode *head;
extern RcpShm *shm;

typedef struct {
	int socket;
	unsigned connected_process;
	int terminal_monitor;
	char ttyname[CLI_MAX_TTY_NAME + 1];
	char admin[CLI_MAX_ADMIN_NAME + 1];
	char ip[CLI_MAX_IP_NAME + 1];
	time_t start_time;
} MuxSession;

static inline void clean_mux(MuxSession *pmux) {
	pmux->socket = 0;
	pmux->connected_process = 0;
	pmux->ttyname[0] = '\0';
	pmux->admin[0] = '\0';
	pmux->ip[0] = '\0';
	pmux->terminal_monitor = 0;
}

#define MAX_SESSIONS (RCP_PROC_MAX + CLI_MAX_SESSION)
extern MuxSession mux[MAX_SESSIONS];

// return NULL if not found
static inline MuxSession *get_mux_process(unsigned process) {
	int i;
	for (i = 0; i  < MAX_SESSIONS; i++) {
		if (process == mux[i].connected_process)
			return &mux[i];
	}
	return NULL;
}

// callbacks.c
extern RcpPkt *pktout;
extern int muxsession;
int processCli(RcpPkt *pkt, int session);

// log.c
extern int syslog_fd;
void logSendCli(uint32_t proc, uint64_t fc, const char *data, uint8_t level);
void logStore(const char *data, uint8_t level, uint64_t fc, uint32_t proc);
void logDistribute(const char *data, uint8_t level, uint64_t fc, uint32_t proc);
int logInitSyslogSocket(void);

// system_files.c
void setSystemDefaults(void);
void updateXinetdFiles(void);
void updateSshKeyFile(void);
void updateHostsFile(void);
void updateResolvconfFile(void);
void updateNtpdFile(void);

// services.c
extern volatile int update_xinetd_files;

// dns.c
extern volatile int update_hosts_file;
extern volatile int update_resolvconf_file;

// ntp.c
extern volatile int update_ntpd_conf;

// cmds.c
int module_initialize_commands(CliNode *head);

// proc_mon.c
void rcp_monitor_procs(void);

// snmpd.c
extern int restart_snmpd;
#define SNMPD_TIMER_INIT 3
#define SNMPD_TIMER_KILL 2
#define SNMPD_TIMER_START 1
void kill_snmpd(void);
void start_snmpd(void);

// canary.c
void check_canary(void);

// http_auth.c
void http_auth(uint32_t ip);
void http_auth_timeout(void);
void http_auth_init(void);

#endif
