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
#ifndef LIBRCP_PROC_H
#define LIBRCP_PROC_H
#include <stdint.h>
#include <sys/times.h>

#ifdef __cplusplus
extern "C" {
#endif


//*************************************************************************************
// processes
//*************************************************************************************
#define RCP_PROC_CLI 1
#define RCP_PROC_SERVICES (1 << 1)

#define RCP_PROC_ROUTER (1 << 2)
#define RCP_PROC_DHCP (1 << 3)
#define RCP_PROC_RIP (1 << 4)
#define RCP_PROC_TESTPROC (1 << 5)
#define RCP_PROC_SEC (1 << 6)
#define RCP_PROC_DNS (1 << 7)
#define RCP_PROC_NTPD (1 << 8)
#define RCP_PROC_ACL (1 << 9)
#define RCP_PROC_NETMON (1 << 10)
#define RCP_PROC_OSPF (1 << 11)
#define RCP_PROC_MAX 10	// maximum number of concurrent processes supported excluding cli sessions and services process

static inline const char *rcpProcName(uint32_t proc) {
	switch (proc) {
		case RCP_PROC_CLI:
			return "CLI";
		case RCP_PROC_SERVICES:
			return "SERVICES";
		case RCP_PROC_ROUTER:
			return "ROUTER";
		case RCP_PROC_DHCP:
			return "DHCP";
		case RCP_PROC_RIP: 
			return "RIP";
		case RCP_PROC_SEC:
			return "SECURITY";
		case RCP_PROC_TESTPROC:
			return "*";
		case RCP_PROC_DNS:
			return "DNS";
		case RCP_PROC_NTPD:
			return "NTPD";
		case RCP_PROC_ACL:
			return "ACL";
		case RCP_PROC_NETMON:
			return "NETMON";
		case RCP_PROC_OSPF:
			return "OSPF";
	}
	return "";
}

static inline const char *rcpProcExecutable(uint32_t proc) {
	switch (proc) {
		case RCP_PROC_CLI:
			return "cli";
		case RCP_PROC_SERVICES:
			return "rcpservicesd";
		case RCP_PROC_ROUTER:
			return "rcprouterd";
		case RCP_PROC_DHCP:
			return "rcpdhcpd";
		case RCP_PROC_RIP:
			return "rcpripd";
		case RCP_PROC_SEC:
			return "rcpsecd";
		case RCP_PROC_TESTPROC:
			return "TESTPROC";
		case RCP_PROC_DNS:
			return "rcpdnsd";
		case RCP_PROC_NTPD:
			return "ntpd";
		case RCP_PROC_ACL:
			return "rcpacld";
		case RCP_PROC_NETMON:
			return "rcpnetmond";
		case RCP_PROC_OSPF:
			return "rcpospfd";
	}
	return "";
}

static inline const char *rcpProcRestartHandler(uint32_t proc) {
	switch (proc) {
		case RCP_PROC_SERVICES:
			return "rcpservices";
		case RCP_PROC_ROUTER:
			return "rcprouter";
		case RCP_PROC_DHCP:
			return "rcpdhcp";
		case RCP_PROC_RIP:
			return "rcprip";
		case RCP_PROC_SEC:
			return "rcpsec";
		case RCP_PROC_DNS:
			return "rcpdns";
		case RCP_PROC_ACL:
			return "rcpacl";
		case RCP_PROC_NETMON:
			return "rcpnetmon";
		case RCP_PROC_OSPF:
			return "rcpospf";
	}
	return "";
}

// get current process name
extern const char *rcpGetProcName(void);
// get current process ID
extern unsigned rcpGetProcId(void);

// drop process privileges - set the user as rcp, chroot it in /home/rcp directory
extern void rcpDropPriv(void);

// initialize process crash handler
void rcpInitCrashHandler(void);

// process debug
extern void rcpDebug(const char *fmt, ...);
extern void rcpDebugEnable(void);
extern void rcpDebugDisable(void);

// send snmp trap
void rcpSnmpSendTrap(const char *msg);

// get current stats interval
int rcpGetStatsInterval(void);

// system time in seconds
static inline uint32_t rcpTic(void) {
	struct tms tm;
	clock_t systick = times(&tm);
	return (uint32_t) ((uint32_t) systick / 100);
}

#ifdef __cplusplus
}
#endif

#endif
