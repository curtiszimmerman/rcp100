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
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include "librcp.h"

int main(int argc, char **argv) {
	RcpShm *shm = NULL;
	
	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}

	int i;
	RcpProcStats *ptr;

	int cycle = 0;
	
	while (cycle < 10) {
		int allset = 1;
		for (i = 0, ptr = &shm->config.pstats[i]; i < (RCP_PROC_MAX + 2); i++, ptr++) {
			// all running processes
			if (!ptr->proc_type)
				continue;
			
			// but not us or cli processes or test processes
			if (ptr->proc_type == RCP_PROC_CLI ||
			     ptr->proc_type == RCP_PROC_SERVICES ||
			     ptr->proc_type == RCP_PROC_TESTPROC)
				continue;
				
			if (ptr->wproc < 3)
				allset = 0;

		}

		if (allset)
			break;
		
		cycle++;
		sleep(1);
	}
	
	for (i = 0, ptr = &shm->config.pstats[i]; i < (RCP_PROC_MAX + 2); i++, ptr++) {
		// all running processes
		if (!ptr->proc_type)
			continue;
		
		// but not us or cli processes or test processes
		if (ptr->proc_type == RCP_PROC_CLI ||
		     ptr->proc_type == RCP_PROC_SERVICES ||
		     ptr->proc_type == RCP_PROC_TESTPROC)
			continue;
			
		printf("Process %s active (%u/%u)\n",
			rcpProcExecutable(ptr->proc_type),
			ptr->wproc,
			ptr->wmonitor);
	}

	return 0;
}
