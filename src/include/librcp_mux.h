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
#ifndef RCPLIB_MUX_H
#define RCPLIB_MUX_H

#ifndef LIBRCP_CLI_H
#include "librcp_cli.h"
#endif
#ifndef LIBRCP_PROC_H
#include "librcp_proc.h"
#endif
#ifndef LIBRCP_SHM_H
#include "librcp_shm.h"
#endif
#ifndef LIBRCP_ROUTE_H
#include "librcp_route.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//*************************************************************************************
// MUX support
//*************************************************************************************
#define RCP_MUX_SOCKET "/var/tmp/rcpsocket"

typedef struct {
	CliNode *clinode; // specific to cli messages
	CliMode mode;	// cli mode
	void *mode_data;	// data associated with the mode. such as an interface name; it should point in shared memory
	int return_path;	// in use by services mux to route back the cli response to the right cli session
	int  return_value;	// 0 if cli command run fine
	uint8_t file_transport;	// the response is contained in the file specified in the data portion of the packet
	uint8_t run_cmd;	// the response in the file is actually a command to be run by cli
	uint8_t run_cmd_term;	// the response in the file is actually a command to be run by cli in a terminal
	uint8_t html;	// the response is used by the http server
} RcpPktCli;

typedef struct {
	uint16_t offset_tty;	// pointer to a \0 terminated string into the data portion
	uint16_t offset_admin;	// pointer to a \0 terminated string into the data portion
	uint16_t offset_ip;	// pointer to a \0 terminated string into the data portion
} RcpPktRegistration;

typedef struct {
	uint64_t facility;		// facility for this message
	uint8_t level;		// log level
} RcpPktLog;

#define RCP_PROC_INTERFACE_NOTIFICATION_GROUP (RCP_PROC_RIP | RCP_PROC_OSPF)
typedef struct {
	RcpInterface *interface;
	uint32_t old_ip;
	uint32_t old_mask;
} RcpPktInterface;

typedef struct {
	RcpRouteType type;
	uint32_t  ip;
	uint32_t mask;
	uint32_t gw;
} RcpPktRoute;

typedef struct {
	uint32_t ip;
	uint32_t mask;
	uint8_t state;	// 1 up, 0 down
} RcpPktLoopback;

typedef struct {
	RcpInterface *interface;
} RcpPktVlan;

typedef struct {
	uint32_t ip;
} RcpPktHttpAuth;

// Packet format. RcpPkt is followed by a data section
typedef struct {
#define RCP_PKT_TYPE_CLI 1
#define RCP_PKT_TYPE_REGISTRATION 2	// no data shipped
#define RCP_PKT_TYPE_LOG 3
#define RCP_PKT_TYPE_IFUP 4
#define RCP_PKT_TYPE_IFDOWN 5
#define RCP_PKT_TYPE_ADDROUTE 6
#define RCP_PKT_TYPE_DELROUTE 7
#define RCP_PKT_TYPE_UPDATESYS 8	// update and restart xinetd
#define RCP_PKT_TYPE_UPDATEDNS 9
#define RCP_PKT_TYPE_IFCHANGED 10
#define RCP_PKT_TYPE_LOOPBACK 11
#define RCP_PKT_TYPE_ROUTE_RQ 12	// route request
#define RCP_PKT_TYPE_HTTP_AUTH 13	// http authentication
#define RCP_PKT_TYPE_VLAN_DELETED 14
	unsigned type;
	uint32_t destination;	// destination process
	uint32_t sender;		// sender process
	union {
		RcpPktCli cli;
		RcpPktRegistration registration;
		RcpPktLog log;
		RcpPktInterface interface;
		RcpPktRoute route;
		RcpPktLoopback loopback;
		RcpPktVlan vlan;
		RcpPktHttpAuth http_auth;
	} pkt;

// the receiver should allocate the maximum of the following data lengths
#define RCP_PKT_DATA_LEN  CLI_MAX_LINE + 1 // leave room for \0 
	int data_len; // data length to follow this structure
} RcpPkt;

static inline const char *rcpPktType(unsigned type) {
	switch (type) {
		case RCP_PKT_TYPE_CLI:
			return "CLI";
		case RCP_PKT_TYPE_REGISTRATION:
			return "REGISTRATION";
		case RCP_PKT_TYPE_LOG:
			return "LOG";
		case RCP_PKT_TYPE_IFUP:
			return "INTERFACE UP";
		case RCP_PKT_TYPE_IFDOWN:
			return "INTERFACE DOWN";
		case RCP_PKT_TYPE_ADDROUTE:
			return "ADD ROUTE";
		case RCP_PKT_TYPE_DELROUTE:
			return "DELETE ROUTE";
		case RCP_PKT_TYPE_UPDATESYS:
			return "UPDATE SYSTEM FILES";
		case RCP_PKT_TYPE_UPDATEDNS:
			return "UPDATE DNS";
		case RCP_PKT_TYPE_IFCHANGED:
			return "INTERFACE ADDRESS CHANGED";
		case RCP_PKT_TYPE_LOOPBACK:
			return "LOOPBACK";
		case RCP_PKT_TYPE_VLAN_DELETED:
			return "VLAN_DELETED";
		case RCP_PKT_TYPE_ROUTE_RQ:
			return "ROUTE REQUEST";
		case RCP_PKT_TYPE_HTTP_AUTH:
			return "HTTP AUTHNETICATION";
	}
	return "UNKNOWN";
}
// connect current process to rcp mux
int rcpConnectMux(unsigned sender_type, const char *ttyname, const char *admin, const char *ip);

// create an empty temporary file, close it and return a file descriptor;
// the name of the file is stored in data argument
FILE *rcpTransportFile(char *data);
FILE *rcpJailTransportFile(char *data);

// create an empty temporary file, close it and return the name of the function in malloc memory
char *rcpGetTempFile(void);

#ifdef __cplusplus
}
#endif

#endif
