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
#include <sys/stat.h>

static int is_mac_allzero(unsigned char *mac) {
	int i;
	for (i = 0; i < 6; i++)
		if (mac[i] != 0)
			return 0;
	
	return 1;
}


// return 1 if the list exists in shared memory
static int list_exists(uint8_t list_number) {
	if (list_number == 0) {
		ASSERT(0);
		return 0;
	}

	RcpAcl *ptr;
	int i;

	for (i = 0, ptr = shm->config.ip_acl; i < RCP_ACL_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (ptr->list_number == list_number)
			return 1;

		return 1;
	}

	return 0;
}


int cliAclGroupCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract interface data
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	
	// extract list number
	int aclno = atoi(argv[2]);
	if (list_exists(aclno) == 0)
		sprintf(data, "Warning: list %d not defined yet", aclno);
		
	// save data in shared memory
	if (strcmp(argv[3], "in") == 0)
		pif->acl_in = aclno;
	else if (strcmp(argv[3], "out") == 0)
		pif->acl_out = aclno;
	else if (strcmp(argv[3], "forward") == 0)
		pif->acl_forward = aclno;
	else
		ASSERT(0);
	
	// trigger netfilter configuration
	netfilter_update = 1;
	
	return 0;
}

int cliNoAclGroupCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract interface data
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	
	// extract list number and save data in shared memory
	if (strcmp(argv[3], "in") == 0)
		pif->acl_in = 0;
	else if (strcmp(argv[3], "out") == 0)
		pif->acl_out = 0;
	else if (strcmp(argv[3], "forward") == 0)
		pif->acl_forward = 0;
	else
		ASSERT(0);
	
	// trigger netfilter configuration
	netfilter_update = 1;
	
	return 0;
}

// generate an empty netfilter table
/*
# iptables -F
# iptables -X
# iptables -t nat -F
# iptables -t nat -X
# iptables -t mangle -F
# iptables -t mangle -X
# iptables -P INPUT ACCEPT
# iptables -P OUTPUT ACCEPT
# iptables -P FORWARD ACCEPT
-F : Deleting (flushing) all the rules.
-X : Delete chain.
-P : Set the default policy
*/
void netfilter_generate_empty(FILE *fp) {
	ASSERT(fp != NULL);
	// flush rules
	fprintf(fp, "iptables --flush\n");
	// flush nat table
	fprintf(fp, "iptables -t nat -F\n");
	// delete all chains
	fprintf(fp, "iptables -X\n");
	// reset all counters
	fprintf(fp, "iptables -Z\n");
	// set policy accept
	fprintf(fp, "iptables -P INPUT ACCEPT\n");
	fprintf(fp, "iptables -P OUTPUT ACCEPT\n");
	fprintf(fp, "iptables -P FORWARD ACCEPT\n");
}

// new table command
static void netfilter_new_table(FILE *fp, RcpAcl *ptr) {
	ASSERT(fp != NULL);
	ASSERT(ptr != NULL);
	
	fprintf(fp, "# new table\n");
	fprintf(fp, "iptables -N rcp%d\n", ptr->list_number);
}



static void netfilter_add_rule_simple(FILE *fp, RcpAcl *ptr) {
	// action
	char action[30];
	ASSERT(ptr->action != RCPACL_ACTION_NONE);
	if (ptr->action == RCPACL_ACTION_PERMIT)
		strcpy(action, "-j ACCEPT");
	else
		strcpy(action, "-j DROP");


	// source ip address
	char source_ip[30];
	if (ptr->u.simple.ip == 0)
		source_ip[0] = '\0';
	else
		sprintf(source_ip, "-s %u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.simple.ip),
			ptr->u.simple.mask_bits);

	
	fprintf(fp, "iptables -A rcp%d %s %s\n",
		ptr->list_number,
		source_ip,
		action);
}

static void netfilter_add_rule_icmp(FILE *fp, RcpAcl *ptr) {
	// action
	char action[30];
	ASSERT(ptr->action != RCPACL_ACTION_NONE);
	if (ptr->action == RCPACL_ACTION_PERMIT)
		strcpy(action, "-j ACCEPT");
	else
		strcpy(action, "-j DROP");

	char mac[50] = {0};
	if (!is_mac_allzero(ptr->u.icmp.mac)) {
		sprintf(mac, "-m mac  --mac-source %02x:%02x:%02x:%02x:%02x:%02x", RCP_PRINT_MAC(ptr->u.icmp.mac));
	}
	
	// source ip address
	char source_ip[30];
	if (ptr->u.icmp.ip == 0)
		source_ip[0] = '\0';
	else
		sprintf(source_ip, "-s %u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.icmp.ip),
			ptr->u.icmp.mask_bits);

	
	// destination ip address
	char dest_ip[30];
	if (ptr->u.icmp.dest_ip == 0)
		dest_ip[0] = '\0';
	else
		sprintf(dest_ip, "-d %u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.icmp.dest_ip),
			ptr->u.icmp.dest_mask_bits);
	
	char icmp_type[30];
	if (ptr->u.icmp.icmp_type == 0)
		icmp_type[0] = '\0';
	else
		sprintf(icmp_type, "--icmp-type %d", ptr->u.icmp.icmp_type);
	
	fprintf(fp, "iptables -A rcp%d %s -p icmp %s %s %s %s\n",
		ptr->list_number,
		mac,
		icmp_type,
		source_ip,
		dest_ip,
		action);
}

// new table rule
static void netfilter_add_rule_extended(FILE *fp, RcpAcl *ptr) {
	ASSERT(fp != NULL);
	ASSERT(ptr != NULL);

	// action
	char action[30];
	ASSERT(ptr->action != RCPACL_ACTION_NONE);
	if (ptr->action == RCPACL_ACTION_PERMIT)
		strcpy(action, "-j ACCEPT");
	else
		strcpy(action, "-j DROP");


	char mac[60] = {0};
	if (!is_mac_allzero(ptr->u.extended.mac)) {
		sprintf(mac, "-m mac --mac-source %02x:%02x:%02x:%02x:%02x:%02x", RCP_PRINT_MAC(ptr->u.extended.mac));
	}

	// protocol number
	char protocol[30];
	if (ptr->u.extended.protocol == RCPACL_PROTOCOL_ANY)
		protocol[0] = '\0';
	else if (ptr->u.extended.protocol == RCPACL_PROTOCOL_TCP)
		strcpy(protocol, "-p tcp");
	else if (ptr->u.extended.protocol == RCPACL_PROTOCOL_UDP)
		strcpy(protocol, "-p udp");
	else
		sprintf(protocol, "-p %u", ptr->u.extended.protocol);

	// source ip address
	char source_ip[30];
	if (ptr->u.extended.ip == 0)
		source_ip[0] = '\0';
	else
		sprintf(source_ip, "-s %u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.extended.ip),
			ptr->u.extended.mask_bits);

	// source port
	char source_port[30];
	if (ptr->u.extended.port == 0)
		source_port[0] = '\0';
	else
		sprintf(source_port, "--sport %u", ptr->u.extended.port);
	
	// destination ip address
	char dest_ip[30];
	if (ptr->u.extended.out_interface[0] != '\0')
		sprintf(dest_ip, "-o %s", ptr->u.extended.out_interface);
	else if (ptr->u.extended.dest_ip == 0)
		dest_ip[0] = '\0';
	else
		sprintf(dest_ip, "-d %u.%u.%u.%u/%u",
			RCP_PRINT_IP(ptr->u.extended.dest_ip),
			ptr->u.extended.dest_mask_bits);

	// destination port
	char dest_port[30];
	if (ptr->u.extended.dest_port == 0)
		dest_port[0] = '\0';
	else
		sprintf(dest_port, "--dport %u", ptr->u.extended.dest_port);

	// state
	char constring[128];
	constring[0] = '\0';
	if (ptr->u.extended.constate) {
		int added = 0;
		strcpy(constring, "-m state --state ");
		if (ptr->u.extended.constate & RCP_ACL_CONSTATE_NEW) {
			strcat(constring, "NEW");
			added = 1;
		}
		if (ptr->u.extended.constate & RCP_ACL_CONSTATE_ESTABLISHED) {
			if (added)
				strcat(constring, ",ESTABLISHED");
			else
				strcat(constring, "ESTABLISHED");
			added = 1;
		}
		if (ptr->u.extended.constate & RCP_ACL_CONSTATE_RELATED) {
			if (added)
				strcat(constring, ",RELATED");
			else
				strcat(constring, "RELATED");
			added = 1;
		}
		if (ptr->u.extended.constate & RCP_ACL_CONSTATE_INVALID) {
			if (added)
				strcat(constring, ",INVALID");
			else
				strcat(constring, "INVALID");
			added = 1;
		}
		
		
	}

	fprintf(fp, "iptables -A rcp%d %s %s %s %s %s %s %s %s\n",
		ptr->list_number,
		mac,
		protocol,
		source_ip,
		source_port,
		dest_ip,
		dest_port,
		constring,
		action);
}




// new table rule
static void netfilter_add_rule(FILE *fp, RcpAcl *ptr) {
	ASSERT(fp != NULL);
	ASSERT(ptr != NULL);

	if (ptr->type ==  RCP_ACL_TYPE_EXTENDED)
		 netfilter_add_rule_extended(fp, ptr);
	else if (ptr->type ==  RCP_ACL_TYPE_SIMPLE)
		 netfilter_add_rule_simple(fp, ptr);
	else if (ptr->type ==  RCP_ACL_TYPE_ICMP)
		 netfilter_add_rule_icmp(fp, ptr);
	else
		ASSERT(0);

}

// drop all
static void netfilter_add_drop_all(FILE *fp, uint8_t list_number) {
	ASSERT(fp != NULL);
	
	fprintf(fp, "iptables -A rcp%d -j DROP\n", list_number);
}

// add interface jump in input chain
static void netfilter_pif_input_jump(FILE *fp, const char *name, uint8_t list_number) {
	ASSERT(fp != NULL);
	ASSERT(name != NULL);

	fprintf(fp, "iptables -A INPUT -i %s -j rcp%d\n", name, list_number);
} 

// add interface jump in output chain
static void netfilter_pif_output_jump(FILE *fp, const char *name, uint8_t list_number) {
	ASSERT(fp != NULL);
	ASSERT(name != NULL);

	fprintf(fp, "iptables -A OUTPUT -o %s -j rcp%d\n", name, list_number);
} 

// add interface jump in forwarding chain
static void netfilter_pif_forwarding_jump(FILE *fp, const char *name, uint8_t list_number) {
	ASSERT(fp != NULL);
	ASSERT(name != NULL);

	fprintf(fp, "iptables -A FORWARD -i %s -j rcp%d\n", name, list_number);
} 

// add masq nat entry
static void netfilter_add_masq(FILE *fp, RcpMasq *ptr) {
	ASSERT(fp != NULL);
	ASSERT(ptr != NULL);

	fprintf(fp, "iptables -t nat -A POSTROUTING -o %s -s %d.%d.%d.%d/%d  -j MASQUERADE\n",
		ptr->out_interface,
		RCP_PRINT_IP(ptr->ip), mask2bits(ptr->mask));
} 

// add port forwarding entry
static void netfilter_add_forwarding(FILE *fp, RcpForwarding *ptr) {
	ASSERT(fp != NULL);
	ASSERT(ptr != NULL);

	fprintf(fp, "iptables -t nat -A PREROUTING -p tcp --dport %u -j DNAT --to %d.%d.%d.%d:%u\n",
		ptr->port, RCP_PRINT_IP(ptr->dest_ip), ptr->dest_port);
} 

#define NETFILTER_FILE "/opt/rcp/var/transport/netfilter.sh"
// generate a new netfilter table
void netfilter_generate(void) {
	int i;

	// open a script file
	FILE *fp = fopen(NETFILTER_FILE, "w");
	if (fp == NULL) {
		rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_ERR, RLOG_FC_SYSCFG,
			"cannot open firewall file");
		return;
	}
	
	// clear the existing table
	netfilter_generate_empty(fp);
	
	// generate all the lists in order
	for (i = 1; i < 199; i++) {
		int have_list = 0;
		int have_permit = 0;
		
		// print deny
		int j;
		RcpAcl *ptr;
		
		// print deny
		for (j = 0, ptr = shm->config.ip_acl; j < RCP_ACL_LIMIT; j++, ptr++) {
			if (!ptr->valid)
				continue;

			if (ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_DENY) {
			     	// create a new table if necessary
			     	if (have_list == 0)
			     		netfilter_new_table(fp, ptr);
			     	have_list = 1;
			     	
			     	// add the rule to the table
			     	netfilter_add_rule(fp, ptr);
			}
		}

		// print permit
		for (j = 0, ptr = shm->config.ip_acl; j < RCP_ACL_LIMIT; j++, ptr++) {
			if (!ptr->valid)
				continue;

			if (ptr->list_number == i && 
			     ptr->action == RCPACL_ACTION_PERMIT) {
			     	have_permit = 1;	// flag adding drop all at the end of the list
			     	
			     	// create a new table if necessary
			     	if (have_list == 0)
			     		netfilter_new_table(fp, ptr);
			     	have_list = 1;
			     	
			     	// add the rule to the table
			     	netfilter_add_rule(fp, ptr);
			}
		}
		
		if (have_permit)
			netfilter_add_drop_all(fp, i);
	}

	// interface jump targets
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0')
			continue;
			
		RcpInterface *pif = &shm->config.intf[i];
		if (pif->acl_in != 0)
			netfilter_pif_input_jump(fp, pif->name, pif->acl_in);
		if (pif->acl_out != 0)
			netfilter_pif_output_jump(fp, pif->name, pif->acl_out);
		if (pif->acl_forward != 0)
			netfilter_pif_forwarding_jump(fp, pif->name, pif->acl_forward);
	}
	
	// masquerade nat
	for (i = 0; i < RCP_MASQ_LIMIT; i++) {
		if (!shm->config.ip_masq[i].valid)
			continue;
			
		netfilter_add_masq(fp, &shm->config.ip_masq[i]);
	}
	
	// port forwarding
	for (i = 0; i < RCP_FORWARD_LIMIT; i++) {
		if (!shm->config.port_forwarding[i].valid)
			continue;
			
		netfilter_add_forwarding(fp, &shm->config.port_forwarding[i]);
	}
			
	// close file and run it
	fclose(fp);
	chmod(NETFILTER_FILE, S_IRUSR | S_IXUSR);
	int v = system(NETFILTER_FILE);
	if (v == -1)
		ASSERT(0);
//	unlink(NETFILTER_FILE);
}

