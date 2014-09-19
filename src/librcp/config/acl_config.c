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
#include "../../cli/exec/cli.h"

static int is_mac_allzero(unsigned char *mac) {
	int i;
	for (i = 0; i < 6; i++)
		if (mac[i] != 0)
			return 0;
	
	return 1;
}



static void print_standard_acl_entry(RcpAcl *ptr, FILE *fp) {
	ASSERT(ptr);
     	ASSERT(ptr->type == RCP_ACL_TYPE_SIMPLE);
     	
	// action
	char action[10];
	ASSERT(ptr->action != RCPACL_ACTION_NONE);
	if (ptr->action == RCPACL_ACTION_PERMIT)
		strcpy(action, "permit");
	else
		strcpy(action, "deny");

	
	// source ip address
	char source_ip[30];
	if (ptr->u.simple.ip == 0)
		strcpy(source_ip, "any");
	else
		sprintf(source_ip, "%u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.simple.ip),
			ptr->u.simple.mask_bits);
	

	// print all data
	fprintf(fp, "access-list %u %s %s\n",
		ptr->list_number,
		action,
		source_ip);
}

static void print_icmp_acl_entry(RcpAcl *ptr, FILE *fp) {
	ASSERT(ptr);
     	ASSERT(ptr->type == RCP_ACL_TYPE_ICMP);
     	
	// action
	char action[10];
	ASSERT(ptr->action != RCPACL_ACTION_NONE);
	if (ptr->action == RCPACL_ACTION_PERMIT)
		strcpy(action, "permit");
	else
		strcpy(action, "deny");

	char mac[30] = {0};
	if (!is_mac_allzero(ptr->u.icmp.mac))
		sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", RCP_PRINT_MAC(ptr->u.icmp.mac));
		
	// source ip address
	char source_ip[30];
	if (ptr->u.icmp.ip == 0)
		strcpy(source_ip, "any");
	else
		sprintf(source_ip, "%u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.icmp.ip),
			ptr->u.icmp.mask_bits);
	

	// destination ip address
	char dest_ip[30];
	if (ptr->u.icmp.dest_ip == 0)
		strcpy(dest_ip, "any");
	else
		sprintf(dest_ip, "%u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.icmp.dest_ip),
			ptr->u.icmp.dest_mask_bits);
	
	char type[10] = "";
	if (ptr->u.icmp.icmp_type != 0)
		sprintf(type, "%d", ptr->u.icmp.icmp_type);
		
	// print all data
	fprintf(fp, "access-list %u %s %s icmp %s %s %s\n",
		ptr->list_number,
		action,
		mac,
		source_ip,
		dest_ip,
		type);
}

static void print_extended_acl_entry(RcpAcl *ptr, FILE *fp) {
	ASSERT(ptr);
     	ASSERT(ptr->type == RCP_ACL_TYPE_EXTENDED);
     	
	// action
	char action[10];
	ASSERT(ptr->action != RCPACL_ACTION_NONE);
	if (ptr->action == RCPACL_ACTION_PERMIT)
		strcpy(action, "permit");
	else
		strcpy(action, "deny");

	char mac[30] = {0};
	if (!is_mac_allzero(ptr->u.extended.mac))
		sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", RCP_PRINT_MAC(ptr->u.extended.mac));

	// protocol number
	char protocol[10];
	if (ptr->u.extended.protocol == RCPACL_PROTOCOL_ANY)
		protocol[0] = '\0';
	else if (ptr->u.extended.protocol == RCPACL_PROTOCOL_TCP)
		strcpy(protocol, "tcp");
	else if (ptr->u.extended.protocol == RCPACL_PROTOCOL_UDP)
		strcpy(protocol, "udp");
	else
		sprintf(protocol, "%u", ptr->u.extended.protocol);
	
	// source ip address
	char source_ip[30];
	if (ptr->u.extended.ip == 0)
		strcpy(source_ip, "any");
	else
		sprintf(source_ip, "%u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.extended.ip),
			ptr->u.extended.mask_bits);
	
	// source port
	char source_port[10];
	if (ptr->u.extended.port == 0)
		source_port[0] = '\0';
	else
		sprintf(source_port, "%u", ptr->u.extended.port);
	
	// destination ip address
	char dest_ip[30];
	if (ptr->u.extended.out_interface[0] != '\0')
		sprintf(dest_ip, "out-interface %s", ptr->u.extended.out_interface);
	else if (ptr->u.extended.dest_ip == 0)
		strcpy(dest_ip, "any");
	else
		sprintf(dest_ip, "%u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.extended.dest_ip),
			ptr->u.extended.dest_mask_bits);
	
	// destination port
	char dest_port[10];
	if (ptr->u.extended.dest_port == 0)
		dest_port[0] = '\0';
	else
		sprintf(dest_port, "%u", ptr->u.extended.dest_port);
	
	// connection state
	char constring[64];
	constring[0] = '\0';
	int added = 0;
	if (ptr->u.extended.constate & RCP_ACL_CONSTATE_NEW) {
		strcat(constring, "new");
		added = 1;
	}
	if (ptr->u.extended.constate & RCP_ACL_CONSTATE_ESTABLISHED) {
		if (added)
			strcat(constring, ",established");
		else
			strcat(constring, "established");
		added = 1;
	}
	if (ptr->u.extended.constate & RCP_ACL_CONSTATE_RELATED) {
		if (added)
			strcat(constring, ",related");
		else
			strcat(constring, "related");
		added = 1;
	}
	if (ptr->u.extended.constate & RCP_ACL_CONSTATE_INVALID) {
		if (added)
			strcat(constring, ",invalid");
		else
			strcat(constring, "invalid");
		added = 1;
	}
	

	// print all data
	fprintf(fp, "access-list %u %s %s %s %s %s %s %s %s\n",
		ptr->list_number,
		action,
		mac,
		protocol,
		source_ip,
		source_port,
		dest_ip,
		dest_port,
		constring);
}

void configAcl(FILE *fp, int no_passwords) {
	int i;
	// print standard ACLs
	for (i = 1; i < 99; i++) {
		int j;
		RcpAcl *ptr;
		
		// print deny
		for (j = 0, ptr = shm->config.ip_acl; j < RCP_ACL_LIMIT; j++, ptr++) {
			if (!ptr->valid)
				continue;

			if (ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_DENY)
			     	print_standard_acl_entry(ptr, fp);
		}

		// print permit
		for (j = 0, ptr = shm->config.ip_acl; j < RCP_ACL_LIMIT; j++, ptr++) {
			if (!ptr->valid)
				continue;

			if (ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_PERMIT)
			     	print_standard_acl_entry(ptr, fp);
		}
	}


	// print extended ACLs
	for (i = 100; i < 199; i++) {
		int j;
		RcpAcl *ptr;
		
		// print deny
		for (j = 0, ptr = shm->config.ip_acl; j < RCP_ACL_LIMIT; j++, ptr++) {
			if (!ptr->valid)
				continue;

			if (ptr->type == RCP_ACL_TYPE_EXTENDED && ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_DENY)
			     	print_extended_acl_entry(ptr, fp);
			     	
			else if (ptr->type == RCP_ACL_TYPE_ICMP && ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_DENY)
			     	print_icmp_acl_entry(ptr, fp);
		}

		// print permit
		for (j = 0, ptr = shm->config.ip_acl; j < RCP_ACL_LIMIT; j++, ptr++) {
			if (!ptr->valid)
				continue;

			if (ptr->type == RCP_ACL_TYPE_EXTENDED && ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_PERMIT)
			     	print_extended_acl_entry(ptr, fp);

			else if (ptr->type == RCP_ACL_TYPE_ICMP && ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_PERMIT)
			     	print_icmp_acl_entry(ptr, fp);
		}
	}
	
	fprintf(fp, "!\n");
}
