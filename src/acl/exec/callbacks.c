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
#include "acl.h"
#include <ctype.h>

RcpPkt *pktout = NULL;

//******************************************************************
// callbacks
//******************************************************************
int processCli(RcpPkt *pkt) {
	// execute cli command
	pktout = pkt;
	char *data = (char *) pkt + sizeof(RcpPkt);
	pkt->pkt.cli.return_value = cliExecNode(pkt->pkt.cli.mode, pkt->pkt.cli.clinode, data);

	// flip sender and destination
	unsigned tmp = pkt->destination;
	pkt->destination = pkt->sender;
	pkt->sender = tmp;
	
	// set new data length
	int len = strlen(data);
	pkt->data_len = (len == 0)? 0: len + 1;
	return 0;
}

static RcpAcl *acl_find(void *to_find, uint8_t type, uint8_t action, uint8_t list_number) {
	ASSERT(to_find);
	if (type >=  RCP_ACL_TYPE_MAX) {
		ASSERT(0);
		return NULL;
	}

	RcpAcl *ptr;
	int i;
	for (i = 0, ptr = shm->config.ip_acl; i < RCP_ACL_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ptr->type != type)
			continue;
		
		if (ptr->action != action)
			continue;
		
		if (ptr->list_number != list_number)
			continue;

		switch (ptr->type) {
			case RCP_ACL_TYPE_EXTENDED: // extended acl
				if (memcmp(&ptr->u.extended, to_find, sizeof(RcpAclExtended)) == 0)
					return ptr;
				break;

			case RCP_ACL_TYPE_SIMPLE: // simple acl
				if (memcmp(&ptr->u.simple, to_find, sizeof(RcpAclSimple)) == 0)
					return ptr;
				break;

			case RCP_ACL_TYPE_ICMP: // icmp acl
				if (memcmp(&ptr->u.icmp, to_find, sizeof(RcpAclIcmp)) == 0)
					return ptr;
				break;

			default:
				ASSERT(0);
				return NULL;
		}
	}
	
	return NULL;
}

static RcpAcl *acl_find_empty(void) {
	RcpAcl *ptr;
	int i;

	for (i = 0, ptr = shm->config.ip_acl; i < RCP_ACL_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		ptr->type = 0;
		ptr->action = 0;
		ptr->list_number = 0;
		return ptr;
	}
	
	return NULL;
}



// check if ptr is an integer number
static int isnum(const char *ptr) {
	if (ptr == NULL || *ptr == '\0')
		return 0;
	
	while (*ptr != '\0') {
		if (!isdigit((int) *ptr))
			return 0;
		ptr++;
	}
	return 1;
}

// standard ACL
int cliStandardAclCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	//
	// store incoming data in a temporary structure
	//
	RcpAclSimple in;
	memset(&in, 0, sizeof(RcpAclSimple));
	
	// no form
	int noform = 0;
	int index = 1;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
	}
		
	// list number
	uint8_t list_number = atoi(argv[index++]);
	
	// action
	uint8_t action;
	if (strcmp(argv[index++], "deny") == 0)
		action = RCPACL_ACTION_DENY;
	else
		action = RCPACL_ACTION_PERMIT;
	
	// source address
	if (strcmp(argv[index], "any") == 0)
		;
	else {
		uint32_t ip;
		uint32_t mask;
		if (atocidr(argv[index], &ip, &mask)) {
			sprintf(data, "Error: invalid IP address\n");
			return RCPERR;
		}
		uint8_t mask_bits = mask2bits(mask);
		in.ip = ip;
		in.mask_bits = mask_bits;
	}
	index++;
	
	
	//
	// process acl
	//
	
	// find if this is an existing acl
	RcpAcl *found = acl_find(&in, RCP_ACL_TYPE_SIMPLE, action, list_number);
	
	// process command
	if (noform) {
		if (found == NULL)
			return 0;
			
		// remove acl
		found->valid = 0;
	}
	else {
		// already there?
		if (found != NULL)
			return 0;

		// allocate new acl
		RcpAcl *newacl = acl_find_empty();
		if (newacl == NULL) {
			strcpy(data, "Error: cannot configure ACL entry, limit reached");
			return RCPERR;
		}

		// copy the data
		memcpy(&newacl->u.simple, &in, sizeof(RcpAclSimple));
		newacl->valid = 1;
		newacl->type = RCP_ACL_TYPE_SIMPLE;
		newacl->action = action;
		newacl->list_number = list_number;
	}		
		
	// trigger netfilter configuration update
	netfilter_update = 2; 
	
	return 0;
}



// extended ACL
int cliAclCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	//
	// store incoming data in a temporary structure
	//
	RcpAclExtended in;
	memset(&in, 0, sizeof(RcpAclExtended));
	
	// no form
	int noform = 0;
	int index = 1;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
	}
		
	// list number
	uint8_t list_number = atoi(argv[index++]);
	
	// action
	uint8_t action;
	if (strcmp(argv[index++], "deny") == 0)
		action = RCPACL_ACTION_DENY;
	else
		action = RCPACL_ACTION_PERMIT;
	

	// mac address
	uint8_t mac[6];
	if (atomac(argv[index], mac) == 0) {
		memcpy(in.mac, mac, 6);
		index++;
	}

	// protocol
	if (isnum(argv[index]))
		in.protocol = atoi(argv[index++]);
	else if (strcmp(argv[index], "tcp") == 0) {
		in.protocol = RCPACL_PROTOCOL_TCP;
		index++;
	}
	else if (strcmp(argv[index], "udp") == 0) {
		in.protocol = RCPACL_PROTOCOL_UDP;
		index++;
	}
	else
		in.protocol = RCPACL_PROTOCOL_ANY;
		
	// source address
	if (strcmp(argv[index], "any") == 0)
		;
	else {
		uint32_t ip;
		uint32_t mask;
		if (atocidr(argv[index], &ip, &mask)) {
			sprintf(data, "Error: invalid source IP address\n");
			return RCPERR;
		}
		uint8_t mask_bits = mask2bits(mask);
		in.ip = ip;
		in.mask_bits = mask_bits;
	}
	index++;
	
	// source port
	if (isnum(argv[index]))
		in.port = atoi(argv[index++]);
	
	// destination address/output interface
	if (strcmp(argv[index], "any") == 0)
		;
	else if (strcmp(argv[index], "out-interface") == 0) {
		strncpy(in.out_interface, argv[index + 1], RCP_MAX_IF_NAME);
		index++;
	}
	else {
		uint32_t ip;
		uint32_t mask;
		if (atocidr(argv[index], &ip, &mask)) {
			sprintf(data, "Error: invalid destination IP address\n");
			return RCPERR;
		}
		uint8_t mask_bits = mask2bits(mask);
		in.dest_ip = ip;
		in.dest_mask_bits = mask_bits;
	}
	index++;

	// destination port
	if (index < argc && isnum(argv[index]))
		in.dest_port = atoi(argv[index++]);
	
	// connection state
	if (index < argc) {
		// parsing
		char *str = argv[index];
		char *ptr = strtok(str, ",");
		in.constate = 0;
		while(ptr != NULL) {
			if (strcmp(ptr, "new") == 0)
				in.constate |= RCP_ACL_CONSTATE_NEW;
			else if (strcmp(ptr, "established") == 0)
				in.constate |= RCP_ACL_CONSTATE_ESTABLISHED;
			else if (strcmp(ptr, "related") == 0)
				in.constate |= RCP_ACL_CONSTATE_RELATED;
			else if (strcmp(ptr, "invalid") == 0)
				in.constate |= RCP_ACL_CONSTATE_INVALID;
			else {
				sprintf(data, "Error: invalid connecton state\n");
				return RCPERR;
			}
			
			ptr = strtok(NULL, ",");
		}
	} // this should be the last one, argv was modified by strtok


	//
	// process acl
	//
	
	// find if this is an existing acl
	RcpAcl *found = acl_find(&in, RCP_ACL_TYPE_EXTENDED, action, list_number);
	
	// process command
	if (noform) {
		if (found == NULL)
			return 0;
			
		// remove acl
		found->valid = 0;
	}
	else {
		// already there?
		if (found != NULL)
			return 0;

		// allocate new acl
		RcpAcl *newacl = acl_find_empty();
		if (newacl == NULL) {
			strcpy(data, "Error: cannot configure ACL entry, limit reached");
			return RCPERR;
		}

		// copy the data
		memcpy(&newacl->u.extended, &in, sizeof(RcpAclExtended));
		newacl->valid = 1;
		newacl->type = RCP_ACL_TYPE_EXTENDED;
		newacl->action = action;
		newacl->list_number = list_number;
	}		
		
	// trigger netfilter configuration update
	netfilter_update = 2; 
	
	return 0;
}


// standard ACL
int cliAclIcmpCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	//
	// store incoming data in a temporary structure
	//
	RcpAclIcmp in;
	memset(&in, 0, sizeof(RcpAclIcmp));
	
	// no form
	int noform = 0;
	int index = 1;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index++;
	}
		
	// list number
	uint8_t list_number = atoi(argv[index++]);
	
	// action
	uint8_t action;
	if (strcmp(argv[index++], "deny") == 0)
		action = RCPACL_ACTION_DENY;
	else
		action = RCPACL_ACTION_PERMIT;
	
	// mac address
	uint8_t mac[6];
	if (atomac(argv[index], mac) == 0) {
		memcpy(in.mac, mac, 6);
		index++;
	}
	else

	// skip "icmp"
	index++;

	// source address
	if (strcmp(argv[index], "any") == 0)
		;
	else {
		uint32_t ip;
		uint32_t mask;
		if (atocidr(argv[index], &ip, &mask)) {
			sprintf(data, "Error: invalid source IP address\n");
			return RCPERR;
		}
		uint8_t mask_bits = mask2bits(mask);
		in.ip = ip;
		in.mask_bits = mask_bits;
	}
	index++;
	
	// destination address/output interface
	if (strcmp(argv[index], "any") == 0)
		;
	else {
		uint32_t ip;
		uint32_t mask;
		if (atocidr(argv[index], &ip, &mask)) {
			sprintf(data, "Error: invalid destination IP address\n");
			return RCPERR;
		}
		uint8_t mask_bits = mask2bits(mask);
		in.dest_ip = ip;
		in.dest_mask_bits = mask_bits;
	}
	index++;

	// icmp type
	if ((argc - 1) == index)
		in.icmp_type = atoi(argv[index]);
	
	//
	// process acl
	//
	
	// find if this is an existing acl
	RcpAcl *found = acl_find(&in, RCP_ACL_TYPE_ICMP, action, list_number);
	
	// process command
	if (noform) {
		if (found == NULL)
			return 0;
			
		// remove acl
		found->valid = 0;
	}
	else {
		// already there?
		if (found != NULL)
			return 0;

		// allocate new acl
		RcpAcl *newacl = acl_find_empty();
		if (newacl == NULL) {
			strcpy(data, "Error: cannot configure ACL entry, limit reached");
			return RCPERR;
		}

		// copy the data
		memcpy(&newacl->u.icmp, &in, sizeof(RcpAclIcmp));
		newacl->valid = 1;
		newacl->type = RCP_ACL_TYPE_ICMP;
		newacl->action = action;
		newacl->list_number = list_number;
	}		
		
	// trigger netfilter configuration update
	netfilter_update = 2; 
	
	return 0;
}

// clear a list
int cliNoAclCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int list_number = atoi(argv[2]);

	// walk the array and remove all elements matching list_number
	int i;
	RcpAcl *ptr;
	
	for (i = 0, ptr = shm->config.ip_acl; i < RCP_ACL_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ptr->list_number == list_number)
			ptr->valid = 0;
	}

	// trigger netfilter configuration update
	netfilter_update = 2; 
	return 0;
}

int cliShowAclStatistics(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	// grab a temporary transport file - the file name is stored in data
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	// close the file
	fclose(fp);
	
	// run iptables command
	char cmd[strlen(data) + 200];
	sprintf(cmd, "iptables-save -t filter -c | grep [-:] | grep -v iptables > %s", data);
	int v = system(cmd);
	if (v == -1)
		ASSERT(0);
	return 0;
}
	

	
