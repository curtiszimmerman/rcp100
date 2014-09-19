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
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/times.h>
#include "librcp.h"

static int myrate = 0;
static uint32_t mytic = 0;
// system time in seconds
static inline uint32_t tic(void) {
	struct tms tm;
	clock_t systick = times(&tm);
	return (uint32_t) ((uint32_t) systick / 100);
}

// send message to logger
void rcpLog(int s, uint32_t proc, uint8_t level, uint64_t facility, const char *fmt, ...) {
	// do nothing if rcp socket is not open
	if (s <= 0)
		return; // mux socket not open

	// get access to logger configuration in shared memory
	RcpShm *shm = rcpGetShm();
	
	// send the message if acceptable
	if (rcpLogAccept(shm, level, facility)) {
		// check my rate
		uint32_t newtic = tic();
		if (newtic != mytic) {
			mytic = newtic;
			myrate = 0;
		}
		if (++myrate > shm->config.rate_limit) {
			return;
		}
		
		// build the message
		RcpPkt *pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
		if (pkt == NULL) {
			ASSERT(0);
			fprintf(stderr, "Error: process %s, cannot allocate memory, attempting recovery...\n", rcpGetProcName());
			exit(1);
		}
		memset(pkt, 0, sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	
		// set packet data
		pkt->type = RCP_PKT_TYPE_LOG;
		pkt->destination = RCP_PROC_SERVICES;
		pkt->sender = proc;
		pkt->pkt.log.facility = facility;
		pkt->pkt.log.level = level;
		
		// write the data to the packet
		char *data = (char *) pkt + sizeof(RcpPkt);
		va_list args;
		va_start (args,fmt);
		vsnprintf(data, RCP_PKT_DATA_LEN, fmt, args);
		va_end (args);
		pkt->data_len = strlen(data) + 1;
	
		// send packet
		errno = 0;
		int n = send(s, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
		if(n < 1) {
			fprintf(stderr, "Error: process %s, cannot send log message, attempting recovery...\n", rcpGetProcName());
		}
		else
			ASSERT(n == sizeof(RcpPkt) + pkt->data_len);
		fflush(0);
		
		// free memory
		free(pkt);
	} // message sent
}
