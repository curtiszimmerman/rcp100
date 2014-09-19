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

static int area_configured(void) {
	RcpOspfArea *ptr;
	int i;
	for (i = 0, ptr = shm->config.ospf_area; i < RCP_OSPF_AREA_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		return 1;
	}

	return 0;
}

static int summary_configured(void) {
	 RcpOspfSummaryAddr *ptr;
	int i;
	for (i = 0, ptr = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		return 1;
	}

	return 0;
}

static int range_configured(void) {
	 RcpOspfRange *ptr;
	int i;
	for (i = 0, ptr = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		return 1;
	}

	return 0;
}

void configOspfIf(FILE *fp, int no_passwords) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];

			if (intf->type == IF_LOOPBACK)
				continue;
				
			if (intf->ospf_cost == RCP_OSPF_IF_COST_DEFAULT &&
			     intf->ospf_priority == RCP_OSPF_IF_PRIORITY_DEFAULT &&
			     intf->ospf_hello == RCP_OSPF_IF_HELLO_DEFAULT &&
			     intf->ospf_dead == RCP_OSPF_IF_DEAD_DEFAULT &&
			     intf->ospf_rxmt == RCP_OSPF_IF_RXMT_DEFAULT &&
			     intf->ospf_auth_type == 0 && 
			     intf->ospf_mtu_ignore == 0)
			     	continue;
			
			if (intf->type == IF_BRIDGE)
				fprintf(fp, "interface bridge %s\n", intf->name);
			else if (intf->type == IF_ETHERNET)
				fprintf(fp, "interface ehternet %s\n", intf->name);
			else if (intf->type == IF_VLAN)
				fprintf(fp, "interface vlan %s\n", intf->name);
			

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

			fprintf(fp, "  exit\n");
		}
	}			
}

void configOspf(FILE *fp, int no_passwords) {
	int to_print = 0;

	// detect existent ospf configuration
	if (area_configured() || summary_configured() || range_configured() ||
	     shm->config.ospf_spf_delay != RCP_OSPF_SPF_DELAY_DEFAULT ||
	     shm->config.ospf_spf_hold != RCP_OSPF_SPF_HOLD_DEFAULT ||
	     shm->config.ospf_log_adjacency ||
	     shm->config.ospf_log_adjacency_detail ||
	     shm->config.ospf_router_id ||
	     shm->config.ospf_redist_information != 0 ||
	     shm->config.ospf_redist_static != 0 ||
	     shm->config.ospf_redist_connected_subnets != 0 ||
	     shm->config.ospf_redist_connected != 0)
		to_print = 1;
	
	// exit if no ospf configuration found
	if (!to_print)
		return;
		
	// print the mode command 
	fprintf(fp, "router ospf\n");
	
	if (shm->config.ospf_router_id)
		fprintf(fp, "  router-id %d.%d.%d.%d\n", RCP_PRINT_IP(shm->config.ospf_router_id));
		
	RcpOspfArea *ptr;
	int i;
	for (i = 0, ptr = shm->config.ospf_area; i < RCP_OSPF_AREA_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		// print network configuration
		int j;
		RcpOspfNetwork *net;
		for (j = 0, net = shm->config.ospf_area[i].network; j < RCP_OSPF_NETWORK_LIMIT; j++, net++) {
			if (!net->valid)
				continue;
				
			fprintf(fp, "  network %d.%d.%d.%d/%d area %u\n",
				RCP_PRINT_IP(net->ip),
				mask2bits(net->mask),
				ptr->area_id);
		}
		
		// stub area
		if (ptr->stub && ptr->no_summary) 
			fprintf(fp, "  area %u stub no-summary\n", ptr->area_id);
		else if (ptr->stub)
			fprintf(fp, "  area %u stub\n", ptr->area_id);
		if (ptr->stub && ptr->default_cost != RCP_OSPF_DEFAULT_COST)
			fprintf(fp, "  area %u default-cost %u\n", ptr->area_id, ptr->default_cost);
	}
	
	// passive interface
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (*intf->name == '\0')
			continue;
		if (intf->ospf_passive_interface)
			fprintf(fp, "  passive-interface %s\n", intf->name);
	}


	if (shm->config.ospf_spf_delay != RCP_OSPF_SPF_DELAY_DEFAULT ||
	    shm->config.ospf_spf_hold != RCP_OSPF_SPF_HOLD_DEFAULT)
		fprintf(fp, "  timers spf %u %u\n",
			shm->config.ospf_spf_delay,
			shm->config.ospf_spf_hold);
	
	if (shm->config.ospf_log_adjacency && shm->config.ospf_log_adjacency_detail)
		fprintf(fp, "  ospf log-adjacency-changes detail\n");
	else if (shm->config.ospf_log_adjacency)
		fprintf(fp, "  ospf log-adjacency-changes\n");
	
	// print default-information originate
	if (shm->config.ospf_redist_information > 0) {
		fprintf(fp, "  default-information originate\n");
	}

	// print redistribute connected
	if (shm->config.ospf_redist_connected > 0) {
		char metric[30];
		*metric = '\0';
		char type[30];
		*type = '\0';
		char tag[30];
		*tag = '\0';
		
		if (shm->config.ospf_redist_connected != RCP_OSPF_CONNECTED_METRIC_DEFAULT)
			sprintf(metric, "metric %u", shm->config.ospf_redist_connected);
		if (shm->config.ospf_connected_metric_type != RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT)
			sprintf(type, "metric-type %u", shm->config.ospf_connected_metric_type);
		if (shm->config.ospf_connected_tag)
			sprintf(tag, "tag %u", shm->config.ospf_connected_tag);
		fprintf(fp, "  redistribute connected %s %s %s\n", metric, type, tag);
		
	}
	
	// print redistribute connected subnets
	if (shm->config.ospf_redist_connected_subnets > 0)
		fprintf(fp, "  redistribute connected loopback\n");

	// print redistribute static
	if (shm->config.ospf_redist_static > 0) {
		char metric[30];
		*metric = '\0';
		char type[30];
		*type = '\0';
		char tag[30];
		*tag = '\0';
		
		if (shm->config.ospf_redist_static != RCP_OSPF_STATIC_METRIC_DEFAULT)
			sprintf(metric, "metric %u", shm->config.ospf_redist_static);
		if (shm->config.ospf_static_metric_type != RCP_OSPF_STATIC_METRIC_TYPE_DEFAULT)
			sprintf(type, "metric-type %u", shm->config.ospf_static_metric_type);
		if (shm->config.ospf_static_tag)
			sprintf(tag, "tag %u", shm->config.ospf_static_tag);
		fprintf(fp, "  redistribute static %s %s %s\n", metric, type, tag);
		
	}

	// print redistribute rip
	if (shm->config.ospf_redist_rip > 0) {
		char metric[30];
		*metric = '\0';
		char type[30];
		*type = '\0';
		char tag[30];
		*tag = '\0';
		
		if (shm->config.ospf_redist_rip != RCP_OSPF_RIP_METRIC_DEFAULT)
			sprintf(metric, "metric %u", shm->config.ospf_redist_rip);
		if (shm->config.ospf_rip_metric_type != RCP_OSPF_RIP_METRIC_TYPE_DEFAULT)
			sprintf(type, "metric-type %u", shm->config.ospf_rip_metric_type);
		if (shm->config.ospf_rip_tag)
			sprintf(tag, "tag %u", shm->config.ospf_rip_tag);
		fprintf(fp, "  redistribute rip %s %s %s\n", metric, type, tag);
		
	}

	// summary address
	RcpOspfSummaryAddr *ptr1;
	for (i = 0, ptr1 = shm->config.ospf_sum_addr; i < RCP_OSPF_SUMMARY_ADDR_LIMIT; i++, ptr1++) {
		if (!ptr1->valid)
			continue;
		fprintf(fp, "  summary-address %d.%d.%d.%d/%d %s\n",
			RCP_PRINT_IP(ptr1->ip),
			mask2bits(ptr1->mask),
			(ptr1->not_advertise)? "not-advertise": "");
	}
	
	// summary range
	RcpOspfRange *ptr2;
	for (i = 0, ptr2 = shm->config.ospf_range; i < RCP_OSPF_RANGE_LIMIT; i++, ptr2++) {
		if (!ptr2->valid)
			continue;
		fprintf(fp, "  area %u range %d.%d.%d.%d/%d %s\n",
			ptr2->area,
			RCP_PRINT_IP(ptr2->ip),
			mask2bits(ptr2->mask),
			(ptr2->not_advertise)? "not-advertise": "");
	}
	
	// discard routes
	if (!shm->config.ospf_discard_external && !shm->config.ospf_discard_internal)
		fprintf(fp, "  no discard-route\n");
	else {
		if (!shm->config.ospf_discard_external)
			fprintf(fp, "  no discard-route external\n");
		if (!shm->config.ospf_discard_internal)
			fprintf(fp, "  no discard-route internal\n");
	}
			
	
		
	// exiting rip mode		
	fprintf(fp, "!\n");
}
