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
#ifndef MGMT_H
#define MGMT_H

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

#define NETMON_ACTIVE_THRESHOLD 5	// number of intervals to declare a host up/down
#define NETMON_STATS_CNT 10	// check the stats interval every 10 seconds

// difference in microseconds
static inline unsigned delta_tv(struct timeval *start, struct timeval *end) {
	return (end->tv_sec - start->tv_sec)*1000000 + end->tv_usec - start->tv_usec;
}

// main.c
#define RESOLVER_TIMEOUT 60
extern int resolver_update;
extern int stats_interval;
extern int nprocs;

// rcp_init.c
extern RcpShm *shm;
extern int muxsock;
extern RcpProcStats *pstats;
void rcp_init(uint32_t process);

// callbacks.c
int processCli(RcpPkt *pkt);
extern RcpPkt *pktout;
void netmon_clear(void);

// cmds.c
int module_initialize_commands(CliNode *head);

// ping.c
extern int ping_enabled;
int open_icmp_sock(void);
void get_icmp_pkt(int sock);
void ping_all(int sock);
void ping_flag_update(void);
void ping_monitor(void);

// udp.c
extern int udp_enabled;
int open_udp_sock(void);
void get_udp_pkt(int sock);
void udp_send(int sock);
void monitor_udp(void);
void udp_flag_update(void);

// dns.c
void reset_dns_pkt(void);
void send_dns(int sock, RcpNetmonHost *ptr);
void get_dns(uint32_t port, RcpNetmonHost *ptr, unsigned char *pkt, int len);
void monitor_dns(RcpNetmonHost *ptr);

// ntp.c
void send_ntp(int sock, RcpNetmonHost *ptr);
void get_ntp(uint32_t port, RcpNetmonHost *ptr, unsigned char *pkt, int len);
void monitor_ntp(RcpNetmonHost *ptr);

// tcp.c
extern int tcp_enabled;
void monitor_tcp(void);
void tcp_flag_update(void);

// http.c
extern int http_enabled;
void monitor_http(void);
void http_flag_update(void);

// ssh.c
extern int ssh_enabled;
void monitor_ssh(void);
void ssh_flag_update(void);

// smtp.c
extern int smtp_enabled;
void monitor_smtp(void);
void smtp_flag_update(void);

// stats.c
void process_stats(void);

#endif
