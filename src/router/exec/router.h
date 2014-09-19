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
#ifndef ROUTER_INTERFACE_H
#define ROUTER_INTERFACE_H
#include "librcp.h"

// main.c
// maximum number of -r arguments supported - removed interfaces
#define MAX_INTREMOVED 4
extern char *intremoved[MAX_INTREMOVED];
extern int intremoved_cnt;

// rcp_init.c
extern RcpShm *shm ;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);

//callbacks.c
extern RcpPkt *pktout;

//interface.c
void router_detect_interfaces(void);
void router_if_timeout(void);

//arp.c
void router_update_if_arp(RcpInterface *intf);

//router.c
void router_update_if_route(RcpInterface *intf);

// kernel.c
int kernel_set_arp(uint32_t ip, unsigned char *mac);
int kernel_del_arp(uint32_t ip);
int kernel_add_route(int cmd, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t metric);
int kernel_set_if_flags(int up, const char *ifname);
int kernel_set_if_ip(uint32_t ip, uint32_t mask, const char *ifname);
int kernel_set_if_mtu(int mtu, const char *ifname);
int kernel_update_interface(const char *ifname);

// callbacks.c
int processCli(RcpPkt *pkt);

// cmds.c
int module_initialize_commands(CliNode *head);


#endif
