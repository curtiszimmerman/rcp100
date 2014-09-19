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

void http_auth(uint32_t ip) {
	int i;

	// find an existing one
	for (i = 0; i < RCP_HTTP_AUTH_LIMIT; i++) {
		if (shm->stats.http_auth[i].ip == ip) {
			char tmp[200];
			sprintf(tmp, "reset http auth token ttl for %d.%d.%d.%d",
				RCP_PRINT_IP(ip));
			logDistribute(tmp, RLOG_DEBUG, RLOG_FC_HTTP, RCP_PROC_SERVICES);
			// reset ttl
			shm->stats.http_auth[i].ttl = RCP_HTTP_AUTH_TTL;
			return;
		}
	}
	
	// find an empty entry
	int found = -1;
	for (i = 0; i < RCP_HTTP_AUTH_LIMIT; i++) {
		if (shm->stats.http_auth[i].ip == 0) {
			found = i;
			break;
		}
	}

	// add the ip address in shared memory
	if (found != -1) {
		char tmp[200];
		sprintf(tmp, "create http auth token for %d.%d.%d.%d, ttl %u",
			RCP_PRINT_IP(ip), RCP_HTTP_AUTH_TTL);
		logDistribute(tmp, RLOG_DEBUG, RLOG_FC_HTTP, RCP_PROC_SERVICES);
		shm->stats.http_auth[found].ip = ip;
		shm->stats.http_auth[found].ttl = RCP_HTTP_AUTH_TTL;
//				printf("http IP %d.%d.%d.%d authenticated\n", RCP_PRINT_IP(pkt->pkt.http_auth.ip));
		// generate a security token for configuration
		shm->stats.http_auth[found].token = (unsigned) rand();
	}
}

void http_auth_timeout(void) {
	int i;
	for (i = 0; i < RCP_HTTP_AUTH_LIMIT; i++) {
		if (shm->stats.http_auth[i].ip == 0)
			continue;
		if (--shm->stats.http_auth[i].ttl <= 0) {
			char tmp[200];
			sprintf(tmp, "remove http auth token for %d.%d.%d.%d",
				RCP_PRINT_IP(shm->stats.http_auth[i].ip));
			logDistribute(tmp, RLOG_DEBUG, RLOG_FC_HTTP, RCP_PROC_SERVICES);
			shm->stats.http_auth[i].ip = 0;
			shm->stats.http_auth[i].ttl = 0;
		}
	}

	// update session
	unsigned t = (unsigned) time(NULL);
	int index = (t  + 2) % RCP_HTTP_AUTH_TTL;
	shm->stats.http_session[index] = (unsigned) rand();
}

void http_auth_init(void) {
	int i;
	for (i = 0; i < RCP_HTTP_AUTH_TTL; i++)
		shm->stats.http_session[i] = (unsigned) rand();
}

