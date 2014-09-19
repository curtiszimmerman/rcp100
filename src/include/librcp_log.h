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
#ifndef LIBRCP_LOG_H
#define LIBRCP_LOG_H
#include <stdint.h>
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif

// log level
#define	RLOG_EMERG	0	// system is unusable
#define	RLOG_ALERT	1	// action must be taken immediately 
#define	RLOG_CRIT	2	// critical condition
#define	RLOG_ERR	3	// error condition
#define	RLOG_WARNING	4	// warning condition
#define	RLOG_NOTICE	5	// normal but significant condition 
#define	RLOG_INFO	6	// informational 
#define	RLOG_DEBUG	7	// debug
// default startup value
#define RLOG_LEVEL_DEFAULT RLOG_NOTICE

static inline const char *rcpLogLevel(uint8_t level) {
	switch (level) {
		case RLOG_EMERG:
			return "Emergency";
		case RLOG_ALERT:
			return "Alert";
		case RLOG_CRIT:
			return "Critical";
		case RLOG_ERR:
			return "Error";
		case RLOG_WARNING:
			return "Warning";
		case RLOG_NOTICE:
			return "Notice";
		case RLOG_INFO:
			return "Information";
		case RLOG_DEBUG:
			return "Debug";
	
	}
	return "";
}

// log facility, as a uint64_t bitmap
#define RLOG_FC_CONFIG		1ULL
#define RLOG_FC_INTERFACE	(1ULL << 1)
#define RLOG_FC_ROUTER		(1ULL << 2)
#define RLOG_FC_ADMIN		(1ULL << 3)
#define RLOG_FC_SYSCFG		(1ULL << 4)
#define RLOG_FC_IPC		(1ULL << 5)
#define RLOG_FC_DHCP		(1ULL << 6)
#define RLOG_FC_RIP		(1ULL << 7)
#define RLOG_FC_DNS		(1ULL << 8)
#define RLOG_FC_NTPD		(1ULL << 9)
#define RLOG_FC_OSPF_SPF		(1ULL << 10)
#define RLOG_FC_OSPF_DRBDR	(1ULL << 11)
#define RLOG_FC_OSPF_LSA		(1ULL << 12)
#define RLOG_FC_OSPF_PKT		(1ULL << 13)
#define RLOG_FC_NETMON		(1ULL << 14)
#define RLOG_FC_HTTP			(1ULL << 15)
// when adding new facilities, add a new cli command in services, implement it in
//   services/src/log.c in parse_facilities(), modify rcpLogFacility() below



// default startup value
#define RLOG_FC_DEFAULT	     (0ULL)

static inline const char *rcpLogFacility(uint64_t fc) {
	if (fc &  RLOG_FC_ROUTER)
		return "ROUTER";
		
	if (fc &  RLOG_FC_OSPF_SPF)
	     	return "OSPF-SPF";
	if (fc &  RLOG_FC_OSPF_LSA)
		return "OSPF-LSA";
	if (fc &  RLOG_FC_OSPF_DRBDR)
		return "OSPF-DRBDR";
	if (fc &  RLOG_FC_OSPF_PKT)
		return "OSPF-PKT";

	switch (fc) {
		case RLOG_FC_CONFIG:
			return "CONFIG";
		case RLOG_FC_INTERFACE:
			return "INTERFACE";
		case RLOG_FC_ADMIN:
			return "ADMIN";
		case RLOG_FC_SYSCFG:
			return "SYSCFG";
		case RLOG_FC_IPC:
			return "IPC";
		case RLOG_FC_DHCP:
			return "DHCP";
		case RLOG_FC_RIP:
			return "RIP";
		case RLOG_FC_DNS:
			return "DNS";
		case RLOG_FC_NTPD:
			return "NTPD";
		case RLOG_FC_NETMON:
			return "MONITOR";
		case RLOG_FC_HTTP:
			return "HTTP";
	}
	return "";
}

// default rate-limit value (messages per second accepted by logging facility)
#define RLOG_RATE_DEFAULT 100

typedef struct logentry_t {
	struct logentry_t *next;
	struct logentry_t *prev;
	time_t ts;			// timestamp - seconds since the Epoch (00:00:00 UTC, January 1, 1970)
	uint64_t facility;		// facility for this message
	uint32_t proc;		// logging process
	uint8_t level;		// log level
	unsigned char *data;	// the message
} RcpLogEntry;

void rcpLog(int s, uint32_t proc, uint8_t level, uint64_t facility, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
