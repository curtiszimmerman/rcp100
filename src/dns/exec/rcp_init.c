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
#include <semaphore.h>

RcpShm *shm = NULL;
int muxsock = 0;		// RCP services sockets
RcpProcStats *pstats = NULL;	// process statistics

void rcp_init(uint32_t process) {
	rcpInitCrashHandler();

	// initialize parser
	if ((shm = rcpShmemInit(process)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}
	CliNode *head = shm->cli_head;
	if (head == NULL) {
		fprintf(stderr, "Error: process %s, memory not initialized, exiting...\n", rcpGetProcName());
		exit(1);
	}

	//***********************************************
	// add CLI commands and grab process stats pointer
	//***********************************************
	// grab semaphore
	sem_t *mutex = rcpSemOpen(RCP_SHMEM_NAME);
	if (mutex == NULL) {
		fprintf(stderr, "Error: process %s, failed to open semaphore, exiting...\n", rcpGetProcName());
		exit(1);
	}
	sem_wait(mutex);
	// init cli commands
	if (module_initialize_commands(head) != 0) {
		fprintf(stderr, "Error: process %s, cannot initialize command line interface\n", rcpGetProcName());
		exit(1);
	}
	// grab proc stats pointer
	pstats = rcpProcStats(shm, process);
	if (pstats == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize statistics\n", rcpGetProcName());
		exit(1);
	}
	if (pstats->proc_type == 0)
		pstats->proc_type = process;
	pstats->pid = getpid();
	pstats->start_cnt++;
	pstats->wproc = 1;
	
	// close semaphores
	sem_post(mutex);
	sem_close(mutex);

	// connect multiplexer
	muxsock = rcpConnectMux(process, NULL, NULL, NULL);	
	if (muxsock == 0) {
		fprintf(stderr, "Error: process %s, cannot connect service socket,  exiting...\n", rcpGetProcName());
		exit(1);
	}
}

