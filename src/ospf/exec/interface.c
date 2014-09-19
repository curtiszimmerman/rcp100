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
#include "ospf.h"
extern RcpPkt *pktout;

int cliIpOspfCost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}
	
	if (strcmp(argv[0], "no") == 0)
		pif->ospf_cost = RCP_OSPF_IF_COST_DEFAULT;
	else {
		uint32_t val;
		sscanf(argv[3], "%u", &val);
		pif->ospf_cost = val;
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net) {
		net->cost = pif->ospf_cost;
		
		// update router lsa
		 lsa_originate_rtr_update();
		// trigger spf
		spfTrigger();
	}
		
	return 0;
}

int cliIpOspfPriority(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0)
		pif->ospf_priority = RCP_OSPF_IF_PRIORITY_DEFAULT;
	else {
		uint32_t val;
		sscanf(argv[3], "%u", &val);
		pif->ospf_priority = val;
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net)
		net->router_priority = pif->ospf_priority;

	return 0;
}

int cliIpOspfHello(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;

	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	// modify both hello and dead interval
	if (strcmp(argv[0], "no") == 0) {
		pif->ospf_hello = RCP_OSPF_IF_HELLO_DEFAULT;
		pif->ospf_dead = RCP_OSPF_IF_DEAD_DEFAULT;
	}
	else {
		uint32_t val;
		sscanf(argv[3], "%u", &val);
		pif->ospf_hello = val;
		pif->ospf_dead = val * 4;
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net) {
		net->hello_interval = pif->ospf_hello;
		if (net->hello_timer > net->hello_interval)
			net->hello_timer = net->hello_interval;

		net->dead_interval = pif->ospf_dead;
		if (net->wait_timer > net->dead_interval)
			net->wait_timer = net->dead_interval;
		OspfNeighbor *nb = net->neighbor;
		while (nb) {
			if (nb->inactivity_timer > net->dead_interval)
				nb->inactivity_timer = net->dead_interval;
			nb = nb->next;
		}
	}

	return 0;
}

int cliIpOspfDead(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}


	if (strcmp(argv[0], "no") == 0)
		pif->ospf_dead = RCP_OSPF_IF_DEAD_DEFAULT;
	else {
		uint32_t val;
		sscanf(argv[3], "%u", &val);
		pif->ospf_dead = val;
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net) {
		net->dead_interval = pif->ospf_dead;
		if (net->wait_timer > net->dead_interval)
			net->wait_timer = net->dead_interval;
		OspfNeighbor *nb = net->neighbor;
		while (nb) {
			if (nb->inactivity_timer > net->dead_interval)
				nb->inactivity_timer = net->dead_interval;
			nb = nb->next;
		}
	}
	
	return 0;
}

int cliIpOspfRetransmit(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0)
		pif->ospf_rxmt = RCP_OSPF_IF_RXMT_DEFAULT;
	else {
		uint32_t val;
		sscanf(argv[3], "%u", &val);
		pif->ospf_rxmt = val;
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net)
		net->rxtm_interval = pif->ospf_rxmt;

	return 0;
}




int cliIpOsfpAuthenticationKey(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;

	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0)
		memset(pif->ospf_passwd, 0, 8);
	else {
		memset(pif->ospf_passwd, 0, 8);
		strcpy(pif->ospf_passwd, argv[3]);
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net) {
		memset(net->ospf_passwd, 0, 8);
		strcpy(net->ospf_passwd, pif->ospf_passwd);
	}

	return 0;
}

int cliIpOsfpMd5AuthenticationKey(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;

	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0) {
		memset(pif->ospf_md5_passwd, 0, 16);
		pif->ospf_md5_key = 0;
	}
	else {
		memset(pif->ospf_md5_passwd, 0, 16);
		strcpy(pif->ospf_md5_passwd, argv[5]);
		unsigned key;
		sscanf(argv[3], "%u", &key);
		pif->ospf_md5_key = key;
	}

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net) {
		memset(net->ospf_md5_passwd, 0, 16);
		strcpy(net->ospf_md5_passwd, pif->ospf_md5_passwd);
		net->ospf_md5_key  = pif->ospf_md5_key;
	}

	return 0;
}

int cliIpOspfAuthentication(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;

	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (strcmp(argv[0], "no") == 0)
		pif->ospf_auth_type = 0;
	else if (argc == 4)
		pif->ospf_auth_type = 2;
	else
		pif->ospf_auth_type = 1;

	// find an active network associated with this interface
	OspfNetwork *net = networkFindAnyIp(pif->ip);	
	if (net) {
		net->auth_type  = pif->ospf_auth_type;
	}
	return 0;
}


int cliIpOsfpMtuIgnore(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}
	
	if (strcmp(argv[0], "no") == 0)
		pif->ospf_mtu_ignore = 0;
	else {
		pif->ospf_mtu_ignore = 1;
	}

	return 0;
}


void delete_vlan_network(uint32_t ip, uint32_t mask) {
	ASSERT(ip);
	ASSERT(mask);

	// delete the network
	OspfNetwork *net = networkFindAnyIp(ip);
	if (net) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(ip));
		uint32_t area_id = net->area_id;
		OspfArea *area = areaFind(area_id);
		ASSERT(area);
		uint32_t ip = net->ip;
		
		// age out my lsa if the last area network is just
		// about to disappear
		if (networkCount(area_id) == 1) {
			lsadbAgeArea(area_id);
		}

		// network down
		netfsmInterfaceDown(net);
		rxMultiDropNetwork(rxsock, net);
		networkRemove(net->area_id, net);
		lsa_originate_rtr_update();
		lsa_originate_net_update();
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
			"network %d.%d.%d.%d deleted from area %u",
			RCP_PRINT_IP(ip), area_id);

		if (networkCount(area_id) == 0) {
			// remove lsa
			lsadbRemoveArea(area_id);
			// remove area
			areaRemove(area);
			// remove area form shared memory
			RcpOspfArea *a = get_shm_area(area_id);
			if (a)
				a->valid = 0;
				
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
				"area %u deleted", area_id);
		}

		// update static routes
		redistStaticUpdate();

		// trigger spf
		spfTrigger();
	}
}

// call this function periodically to update multicast group and rip networks
void ospf_update_interfaces(void) {
	// walk through all interfaces and join or drop the multicast group
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK)
			continue;
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));

		// network not started
		if (!intf->ospf_multicast_ip) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
			// look for a shm network
			RcpOspfNetwork *shm_net = shm_match_network(intf->ip & intf->mask, intf->mask);
			if (shm_net && intf->link_up == 1 && intf->admin_up == 1) {

				// add a ospf network only if the interface is up
				OspfNetwork *net = malloc(sizeof(OspfNetwork));
				if (net == NULL) {
					printf("Cannot allocate memory\n");
					exit(1);
				}
				
				memset(net, 0, sizeof(OspfNetwork));
				// add interface data
				net->ip = intf->ip;
				net->mask = intf->mask;
				net->area_id = shm_net->area_id;
				net->router_id = shm->config.ospf_router_id;
				net->cost = intf->ospf_cost;
				net->router_priority = intf->ospf_priority;
				net->hello_interval = intf->ospf_hello;
				net->dead_interval = intf->ospf_dead;
				net->rxtm_interval = intf->ospf_rxmt;
				net->auth_type = intf->ospf_auth_type;
				memset(net->ospf_passwd, 0, 8);
				strcpy(net->ospf_passwd, intf->ospf_passwd);
				memset(net->ospf_md5_passwd, 0, 16);
				strcpy(net->ospf_md5_passwd, intf->ospf_md5_passwd);
				net->ospf_md5_key = intf->ospf_md5_key;
				if (networkAdd(net->area_id, net)) {
					free(net);
					rcpDebug("Cannot add network\n");
					continue;
				}
				netfsmInterfaceUp(net);
				
				// set interface addresses
				intf->ospf_multicast_ip = intf->ip;
				intf->ospf_multicast_mask = intf->mask;
				rxMultiJoinNetwork(rxsock, net);
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
					"network %d.%d.%d.%d added to area %u",
					RCP_PRINT_IP(net->ip), shm_net->area_id);
				
				// update static routes
				redistStaticUpdate();

				// trigger spf
				spfTrigger();
			}
			
			continue;
		}

		// existing network
		if (intf->ospf_multicast_ip) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
			// detect a network deleted on an existing ospf interface
			RcpOspfNetwork *shm_net;
			if (intf->ip)
				shm_net = shm_match_network(intf->ip, intf->mask);
			else // just in case the interface changed to 0
				shm_net = shm_match_network(intf->ospf_multicast_ip, intf->ospf_multicast_mask);

			if (!shm_net) {
				// delete the network
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
				OspfNetwork *net = networkFindAnyIp(intf->ip);
				if (net) {
					uint32_t area_id = net->area_id;
					OspfArea *area = areaFind(area_id);
					ASSERT(area);
					uint32_t ip = net->ip;
					
					// age out my lsa if the last area network is just
					// about to disappear
					if (networkCount(area_id) == 1) {
						lsadbAgeArea(area_id);
					}

					// network down
					netfsmInterfaceDown(net);
					rxMultiDropNetwork(rxsock, net);
					networkRemove(net->area_id, net);
					lsa_originate_rtr_update();
					lsa_originate_net_update();
					intf->ospf_multicast_ip = 0;
					intf->ospf_multicast_mask = 0;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
						"network %d.%d.%d.%d deleted from area %u",
						RCP_PRINT_IP(ip), area_id);

					if (networkCount(area_id) == 0) {
						// remove lsa
						lsadbRemoveArea(area_id);
						// remove area
						areaRemove(area);
						// remove area form shared memory
						RcpOspfArea *a = get_shm_area(area_id);
						if (a)
							a->valid = 0;
							
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
							"area %u deleted", area_id);
					}

					// update static routes
					redistStaticUpdate();

					// trigger spf
					spfTrigger();
				}
				continue;
			}




			// detect an interface address change on an existing ospf interface
			if (shm_net && (intf->ip != intf->ospf_multicast_ip || intf->mask != intf->ospf_multicast_mask)) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
				// delete the old network
				OspfNetwork *net = networkFind(shm_net->area_id, intf->ospf_multicast_ip, intf->ospf_multicast_mask);
				if (net) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
					uint32_t ip = net->ip;
					netfsmInterfaceDown(net);
					rxMultiDropNetwork(rxsock, net);
					networkRemove(net->area_id, net);
					lsa_originate_rtr_update();
					lsa_originate_net_update();
					intf->ospf_multicast_ip = 0;
					intf->ospf_multicast_mask = 0;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
						"network %d.%d.%d.%d deleted from area %u",
						RCP_PRINT_IP(ip), shm_net->area_id);

					// update static routes
					redistStaticUpdate();
					spfTrigger();
				}
				// a new network will be created again by this function in one second
				continue;
			}
			
			// interface down on an existing ospf interface
			if (shm_net && (intf->link_up == 0 || intf->admin_up == 0)) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
				// if we have a network configured and the network running
				RcpOspfNetwork *shm_net = shm_match_network(intf->ip, intf->mask);
				if (shm_net) {
//printf("%d intf ip %d.%d.%d.%d\n", __LINE__, RCP_PRINT_IP(intf->ip));
					// delete the network
					OspfNetwork *net = networkFindAnyIp(intf->ip);
					if (net) {
						uint32_t ip = net->ip;
						netfsmInterfaceDown(net);
						rxMultiDropNetwork(rxsock, net);
						networkRemove(net->area_id, net);
						lsa_originate_rtr_update();
						lsa_originate_net_update();
						intf->ospf_multicast_ip = 0;
						intf->ospf_multicast_mask = 0;
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
							"network %d.%d.%d.%d deleted from area %u",
							RCP_PRINT_IP(ip), shm_net->area_id);
	
						// update static routes
						redistStaticUpdate();
						spfTriggerNow();
					}
				}
			}
		}
	}		
}
