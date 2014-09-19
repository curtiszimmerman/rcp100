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
#ifndef LIBRCP_INTERFACE_H
#define LIBRCP_INTERFACE_H
#ifndef LIBRCP_SHM_H
#include "librcp_shm.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// interface packet stats
typedef struct rcp_ifcount_t {
	struct rcp_ifcount_t *next;
	char name[RCP_MAX_IF_NAME + 10];
	
	// rx
	long long unsigned rx_bytes;
	long long unsigned rx_packets;
	long long unsigned rx_errors;
	long long unsigned rx_dropped;
	long long unsigned rx_fifo;
	long long unsigned rx_frame;
	long long unsigned rx_compressed;
	long long unsigned rx_multicast;
	
	// tx
	long long unsigned tx_bytes;
	long long unsigned tx_packets;
	long long unsigned tx_errors;
	long long unsigned tx_dropped;
	long long unsigned tx_fifo;
	long long unsigned tx_colls;
	long long unsigned tx_carrierfifo;
	long long unsigned tx_compressed;
} RcpIfCounts;
RcpIfCounts *rcpGetInterfaceCounts(void);

// interface find functions
RcpInterface *rcpFindInterface(RcpShm *shm, uint32_t ip);
RcpInterface *rcpFindInterfaceByName(RcpShm *shm, const char *name);
RcpInterface *rcpFindInterfaceByLPM(RcpShm *shm, uint32_t ip);
RcpInterface *rcpFindInterfaceByKIndex(RcpShm *shm, int kindex);

// interface type
int rcpInterfaceVirtual(const char *ifname);
#ifdef __cplusplus
}
#endif
#endif
