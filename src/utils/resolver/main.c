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
#include <netdb.h>
#include "librcp.h"


int main(int argc, char **argv) {
	RcpShm *shm = NULL;
	
	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}

	int i;
	RcpNetmonHost *ptr;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		uint32_t ip;
		if (atoip(ptr->hostname, &ip) == 0)
			ptr->ip_resolved = ip;
		else {
			// resolve hostname
			struct hostent *h;
			if (!(h = gethostbyname(ptr->hostname))) {
				continue;
			}
			uint32_t tmp;
			memcpy(&tmp, h->h_addr, 4);
			ptr->ip_resolved = ntohl(tmp);
		}
//printf("host %s ip %d.%d.%d.%d\n", ptr->hostname, RCP_PRINT_IP(ptr->ip_resolved));			
	}
	
		
	return 0;
}
