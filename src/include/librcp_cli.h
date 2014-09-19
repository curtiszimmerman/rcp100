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
#ifndef LIBRCP_CLI_H
#define LIBRCP_CLI_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//*************************************************************************************
// CLI Definitions
//*************************************************************************************
// the maximum command length accepted by cli
#define CLI_MAX_LINE 1024			  // used also to limit the output
#define CLI_MAX_ARGS 128			 // maximum 128 arguments allowed in the command
#define CLI_MAX_HOSTNAME 16		  //maxmimum length of a hostname
#define CLI_MAX_TUNNEL_NAME 15		  //maxmimum length of a tunnel name
#define CLI_MAX_DOMAIN_NAME 63		  //maxmimum length of a DNS domain name
#define CLI_MAX_PASSWD 128		  // maximum length of a password (text or digest)
#define CLI_TUN_PASSWD 24		  // maximum length of a ipsec tunnel password
#define CLI_RIP_PASSWD 16		  	// maximum length of a rip md5 auth password
#define CLI_OSPF_PASSWD 8		  	// maximum length of osfp auth password
#define CLI_OSPF_MD5_PASSWD 16		  // maximum length of a ospf md5 auth password
#define CLI_SNMP_MIN_PASSWD 8		  // minimum length of an snmp v3 password
#define CLI_SNMP_MAX_PASSWD 64		  // maximum length of an snmp v3 password
#define CLI_SNMP_MAX_COMMUNITY 16	  // maximum length of an snmp (v1 and v2c) community string
#define CLI_MAX_PUBKEY CLI_MAX_LINE	  // maximum length of a public key
#define CLI_MAX_SESSION 32 		// maximum number of cli sessions
#define CLI_MAX_ADMIN_NAME 31		// maximum length of an admin name
	// as stored in shm configuration, it can have any length
	// however, only the first 32 characters will be used for show commands
#define CLI_MAX_TTY_NAME 20		// maximum length of a tty terminal name
#define CLI_MAX_IP_NAME	16		// maximum length of a IP address string
#define CLI_DEFAULT_TIMEOUT 10		// 10 minutes default for exec-timeout
	
//*************
// Parsing
//*************
// cli mode
typedef unsigned long long CliMode;
#define CLIMODE_MAX 64				  // kept as bitmapped in CliMode

#define CLIMODE_NONE 0
#define CLIMODE_LOGIN 1
#define CLIMODE_ENABLE (1ULL << 1)
#define CLIMODE_CONFIG (1ULL << 2)
#define CLIMODE_INTERFACE (1ULL << 3)
#define CLIMODE_INTERFACE_LOOPBACK (1ULL << 4)
#define CLIMODE_INTERFACE_BRIDGE (1ULL << 5)
#define CLIMODE_INTERFACE_VLAN (1ULL << 6)
#define CLIMODE_RIP (1ULL << 7)
#define CLIMODE_DHCP_RELAY (1ULL << 8)
#define CLIMODE_TUNNEL_IPSEC (1ULL << 9)
#define CLIMODE_OSPF (1ULL << 10)
#define CLIMODE_DHCP_SERVER (1ULL << 12)
#define CLIMODE_DHCP_NETWORK (1ULL << 13)

static inline int cliPreviousMode(CliMode mode) {
	switch (mode) {
		case CLIMODE_LOGIN: return CLIMODE_NONE;
		case CLIMODE_ENABLE: return CLIMODE_LOGIN;
		case CLIMODE_CONFIG: return CLIMODE_ENABLE;
		case CLIMODE_INTERFACE: return CLIMODE_CONFIG;
		case CLIMODE_INTERFACE_LOOPBACK: return CLIMODE_CONFIG;
		case CLIMODE_INTERFACE_BRIDGE: return CLIMODE_CONFIG;
		case CLIMODE_INTERFACE_VLAN: return CLIMODE_CONFIG;
		case CLIMODE_RIP: return CLIMODE_CONFIG;
		case CLIMODE_DHCP_RELAY: return CLIMODE_CONFIG;
		case CLIMODE_TUNNEL_IPSEC: return CLIMODE_CONFIG;
		case CLIMODE_OSPF: return CLIMODE_CONFIG;
		case CLIMODE_DHCP_SERVER: return CLIMODE_CONFIG;
		case CLIMODE_DHCP_NETWORK: return CLIMODE_DHCP_SERVER;
	}
 	return CLIMODE_NONE;
 }
		
static inline int cliModeIndex(CliMode mode) {
	switch (mode) {
		case CLIMODE_LOGIN: return 1;
		case CLIMODE_ENABLE: return 2;
		case CLIMODE_CONFIG: return 3;
		case CLIMODE_INTERFACE: return 4;
		case CLIMODE_INTERFACE_LOOPBACK: return 5;
		case CLIMODE_INTERFACE_BRIDGE: return 6;
		case CLIMODE_INTERFACE_VLAN: return 7;
		case CLIMODE_RIP: return 8;
		case CLIMODE_DHCP_RELAY: return 9;
		case CLIMODE_TUNNEL_IPSEC: return 10;
		case CLIMODE_OSPF: return 11;
		case CLIMODE_DHCP_SERVER: return 12;
		case CLIMODE_DHCP_NETWORK: return 13;
	}
	return 0;
}

#define CLIMODE_ALL ((CliMode) -1)
#define CLIMODE_ALL_ENABLE  (CLIMODE_ALL ^ CLIMODE_LOGIN)
#define CLIMODE_ALL_CONFIG  (CLIMODE_ALL ^ (CLIMODE_LOGIN | CLIMODE_ENABLE))

// parser node structure
typedef struct CliNode_t
{
	uint32_t owner;				  // process owner of the node
	struct CliNode_t *rule;
	struct CliNode_t *next;
	char *token;				  // the token in the command
	char *help;				  // help string
	const char *help_special;		  // left side of the help in case of a special command
	int exe;				  // is the node executable or not
	CliMode exe_mode;			  // bitmapped mode
	CliMode mode;				  // bitmapped mode
	char *question;			// ask the user before running the command
} CliNode;

// information regarding the last parser operation
typedef struct
{
	char word[CLI_MAX_LINE + 1];
	char word_multimatch[CLI_MAX_LINE + 1];
	char word_completed[CLI_MAX_LINE + 1];
	CliNode *last_node_match;
	int depth;
	const char *error_string;
} CliParserOp;

// initialize the parser
// returns the head node
CliNode *cliParserInit(unsigned owner);

// find a command in the tree
// returns termination node for the command, or NULL
CliNode *cliParserFindCmd(CliMode mode, CliNode *current, const char *str);

// add a command to the parsing tree
// returns 0 if ok, RCPERR_... if failed
int cliParserAddCmd(CliMode mode, CliNode *current, const char *str);
int cliParserAddHelp(CliNode *current, const char *token, const char *str);
int cliAddQuestion(CliNode *node, const char *str);

// print node in parser tree format
void cliParserPrintNodeTree(int level, CliNode *node);
void cliParserPrintModeTree(int level, CliNode *node);
void cliParserPrintMem(void);

// get the result of the last parsing command
CliParserOp *cliParserGetLastOp(void);

// enable output modifiers
int cliParserAddOM(void);

//*************
// Executing the command
//*************
// return 0 if OK
int cliAddFunction(CliMode mode, CliNode *node,  int (*f)(CliMode mode, int argc, char **argv));
void cliRemoveFunctions(void);
// return 0 if ok
int cliAddExecutable(CliNode *node,  const char *exec_string);
// execute a command
// arguments: the node and the command itself
// returns 0 if ok, RCPERR if the node is not found
int cliExecNode(CliMode mode, CliNode *node, const char *cmd);

// run a program as a separate process, read the result trough a pipe
// returns malloced memory or NULL if failed
char *cliRunProgram(const char *prog);

// check for unsanitized input
int rcpInputNotClean(const char *str);

#ifdef __cplusplus
}
#endif
#endif
