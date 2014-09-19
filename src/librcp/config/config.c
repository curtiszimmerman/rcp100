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

void configInterfaces(FILE *fp, int no_passwords) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->type == IF_LOOPBACK) {
				if (rcpInterfaceVirtual(intf->name)) {
					int lb = 0;
					sscanf(intf->name, "lo:%d", &lb);
					fprintf(fp, "interface loopback %d\n", lb);
				}
				else
					fprintf(fp, "interface loopback %s\n", intf->name);
			}
			else if (intf->type == IF_BRIDGE)
				fprintf(fp, "interface bridge %s\n", intf->name);
			else if (intf->type == IF_ETHERNET)
				fprintf(fp, "interface ethernet %s\n", intf->name);
			else if (intf->type == IF_VLAN)
				fprintf(fp, "interface vlan %s id %u %s\n",
					intf->name,
					intf->vlan_id,
					shm->config.intf[intf->vlan_parent].name);
			else
				ASSERT(0);
				
			if (intf->ip != 0)
				fprintf(fp, "  ip address %d.%d.%d.%d/%d\n",
					RCP_PRINT_IP(intf->ip),
					mask2bits(intf->mask));
			if (intf->mtu != 0)
				fprintf(fp, "  ip mtu %u\n", intf->mtu);

			// only for ethernet/bridge interfaces
			if (intf->type == IF_ETHERNET || intf->type == IF_BRIDGE || intf->type == IF_VLAN) {
				if (intf->proxy_arp && intf->type != IF_VLAN )
					fprintf(fp, "  ip proxy-arp\n");
				if (intf->dhcp_relay)
					fprintf(fp, "  ip dhcp relay enable\n");
				if (intf->admin_up)
					fprintf(fp, "  no shutdown\n");
				else
					fprintf(fp, "  shutdown\n");
			}
			
			// acl
			if (intf->acl_in)
				fprintf(fp, "  ip access-group %u in\n", intf->acl_in);
			if (intf->acl_out)
				fprintf(fp, "  ip access-group %u out\n", intf->acl_out);
			if (intf->acl_forward)
				fprintf(fp, "  ip access-group %u forward\n", intf->acl_forward);
			
			// rip
			if (intf->type == IF_ETHERNET || intf->type == IF_BRIDGE || intf->type == IF_VLAN) {
				if (intf->rip_passwd[0] != '\0')
					fprintf(fp, "  ip rip md5 secret %s\n", (no_passwords)? "<removed>": intf->rip_passwd);
			}
			
			// ospf
			if (intf->type == IF_ETHERNET || intf->type == IF_BRIDGE || intf->type == IF_VLAN) {
				if (intf->ospf_cost != RCP_OSPF_IF_COST_DEFAULT)
					fprintf(fp, "  ip ospf cost %u\n", intf->ospf_cost);
				if (intf->ospf_priority != RCP_OSPF_IF_PRIORITY_DEFAULT)
					fprintf(fp, "  ip ospf priority %u\n", intf->ospf_priority);
				if (intf->ospf_hello != RCP_OSPF_IF_HELLO_DEFAULT)
					fprintf(fp, "  ip ospf hello-interval %u\n", intf->ospf_hello);
				if (intf->ospf_dead != RCP_OSPF_IF_DEAD_DEFAULT)
					fprintf(fp, "  ip ospf dead-interval %u\n", intf->ospf_dead);
				if (intf->ospf_rxmt != RCP_OSPF_IF_RXMT_DEFAULT)
					fprintf(fp, "  ip ospf retransmit-interval %u\n", intf->ospf_rxmt);
				
				// authentication
				if (*intf->ospf_passwd != '\0')
					fprintf(fp, "  ip ospf authentication-key %s\n", (no_passwords)? "<removed>": intf->ospf_passwd);
				if (*intf->ospf_md5_passwd != '\0')
					fprintf(fp, "  ip ospf message-digest-key %u md5 %s\n",
						intf->ospf_md5_key, (no_passwords)? "<removed>": intf->ospf_md5_passwd);

				if (intf->ospf_auth_type == 1)
					fprintf(fp, "  ip ospf authentication\n");
				else if (intf->ospf_auth_type == 2)
					fprintf(fp, "  ip ospf authentication message-digest\n");
				
				if (intf->ospf_mtu_ignore)
					fprintf(fp, "  ip ospf mtu-ignore\n");
			}
						
			fprintf(fp, "!\n");
		}
	}
}

void configArp(FILE *fp, int no_passwords) {
	int i;
	RcpStaticArp *ptr;
	int printed = 0;
	
	// print static arp
	for (i = 0, ptr = shm->config.sarp; i < RCP_ARP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		fprintf(fp, "arp %d.%d.%d.%d %02X:%02X:%02X:%02X:%02X:%02X\n",
			RCP_PRINT_IP(ptr->ip),
			RCP_PRINT_MAC(ptr->mac));
		printed = 1;
	}

	if (printed)
		fprintf(fp, "!\n");
}

void configRoute(FILE *fp, int no_passwords) {
	int i;
	RcpStaticRoute *ptr;
	int printed = 0;
	
	// print static routes
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		
		printed = 1;
		if (ptr->type == RCP_ROUTE_STATIC) {
			if (ptr->metric == 1)
				fprintf(fp, "ip route %d.%d.%d.%d/%d  %d.%d.%d.%d\n",
					RCP_PRINT_IP(ptr->ip),
					mask2bits(ptr->mask),
					RCP_PRINT_IP(ptr->gw));
			else
				fprintf(fp, "ip route %d.%d.%d.%d/%d  %d.%d.%d.%d %u\n",
					RCP_PRINT_IP(ptr->ip),
					mask2bits(ptr->mask),
					RCP_PRINT_IP(ptr->gw),
					ptr->metric);
		}
		else if (ptr->type == RCP_ROUTE_BLACKHOLE)
			fprintf(fp, "ip route %d.%d.%d.%d/%d blackhole\n",
				RCP_PRINT_IP(ptr->ip),
				mask2bits(ptr->mask));
		else
			ASSERT(0);
	}

	if (printed)
		fprintf(fp, "!\n");
}
