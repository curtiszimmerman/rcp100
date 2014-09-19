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
#include <signal.h>

void rcp_monitor_procs(void) {
	int i;
	RcpProcStats *ptr;
	for (i = 0, ptr = &shm->config.pstats[i]; i < (RCP_PROC_MAX + 2); i++, ptr++) {
		char buf[1000];
	
		// all running processes
		if (!ptr->proc_type)
			continue;
		
		// but not us or cli processes or test processes
		if (ptr->proc_type == RCP_PROC_CLI ||
		     ptr->proc_type == RCP_PROC_SERVICES ||
		     ptr->proc_type == RCP_PROC_TESTPROC)
			continue;
		
		// not-monitored keep ptr->wproc at 0
		if (ptr->wproc == 0)
			continue;

		if (ptr->wproc == ptr->wmonitor) {
			// remove old process
			kill(ptr->pid, SIGKILL);
			ptr->wmonitor = 0;
			const char *proc_name = rcpProcRestartHandler(ptr->proc_type);
			ASSERT(proc_name[0] != '\0');
			if (proc_name[0] != '\0') {
				if (ptr->no_logging == 0) {
					sprintf(buf, "process %s not responding, restarting...", proc_name);
					logDistribute(buf, RLOG_ERR, RLOG_FC_IPC, RCP_PROC_SERVICES);
				}
				else
					ptr->no_logging = 0;
				
				// start the new process
				sprintf(buf, "/opt/rcp/etc/init.d/%s &", proc_name);
				int v = system(buf);
				if (v == -1)
					ASSERT(0);
			}
		}
		
		ptr->wmonitor = ptr->wproc;
	}
}


int cliRestartProcess(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	if (strcmp(argv[2], "ntpd") == 0) {
		int v = system("pkill ntpd");
		if (v == -1)
			ASSERT(0);
	}
	else {
		// find the process type
		uint32_t proc_type = 0;
		if (strcmp(argv[2], "rcpdhcp") == 0)
			proc_type = RCP_PROC_DHCP;
		else if (strcmp(argv[2], "rcpdns") == 0)
			proc_type = RCP_PROC_DNS;
		else if (strcmp(argv[2], "rcprip") == 0)
			proc_type = RCP_PROC_RIP;
		else if (strcmp(argv[2], "rcprouter") == 0)
			proc_type = RCP_PROC_ROUTER;
		else if (strcmp(argv[2], "rcpnetmon") == 0)
			proc_type = RCP_PROC_NETMON;
		else if (strcmp(argv[2], "rcpospf") == 0)
			proc_type = RCP_PROC_OSPF;
		else
			ASSERT(0);			
			
		// find the process in the process table
		RcpProcStats *proc = rcpProcStats(shm, proc_type);
		if (proc == NULL || proc->proc_type == 0) {
			strcpy(data, "Error: cannot find process\n");
			return RCPERR;
		}
		
		int rv = kill(proc->pid, SIGKILL);
		ASSERT(rv == 0);
	}	


	return 0;
}

