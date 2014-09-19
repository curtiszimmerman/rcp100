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
#ifndef ACL_H
#define ACL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "librcp.h"

// main.c
extern int netfilter_update;

// rcp_init.c
extern RcpShm *shm;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);


// netfilter.c
// generate a new netfilter table
void netfilter_generate(void);

// ipsec.c
// generate a new setkey table
void setkey_generate(void);

// callbacks.c
int processCli(RcpPkt *pkt);
extern RcpPkt *pktout;

// cmds.c
int module_initialize_commands(CliNode *head);


#endif
