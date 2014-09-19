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
#include "lsa.h"
extern RcpPkt *pktout;

//***********************************************************************
// CLI
//***********************************************************************
int is_asbr = 0;

static void set_asbr_flag(void) {
	int new_asbr = (shm->config.ospf_redist_connected |
		shm->config.ospf_redist_connected_subnets |
		shm->config.ospf_redist_information |
		shm->config.ospf_redist_static)? 1:0;
		
	if (new_asbr != is_asbr) {
		// set flag
		is_asbr = new_asbr;
		
		// originate a new router lsa
		lsa_originate_rtr_update();
		
		// update summary address
		summaddr_update();
	}
}

void redistClearShm(void) {
	shm->config.ospf_redist_static = 0;
	shm->config.ospf_static_tag = 0;
	shm->config.ospf_static_metric_type = RCP_OSPF_STATIC_METRIC_TYPE_DEFAULT;
	shm->config.ospf_redist_connected = 0;
	shm->config.ospf_connected_tag = 0;
	shm->config.ospf_connected_metric_type = RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT;
	shm->config.ospf_redist_connected = 0;
	shm->config.ospf_redist_connected_subnets = 0;
	shm->config.ospf_redist_rip = 0;
	shm->config.ospf_rip_tag = 0;
	shm->config.ospf_rip_metric_type = RCP_OSPF_RIP_METRIC_TYPE_DEFAULT;
	shm->config.ospf_redist_information = 0;
	
	set_asbr_flag();
}


int cliRedistDefaultInformationCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_redist_information = 0;
	}
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		shm->config.ospf_redist_information = 1;
		if (shm->config.ospf_redist_static == 0)
			shm->config.ospf_redist_static = RCP_OSPF_STATIC_METRIC_DEFAULT;
		
	}

	// update all static routes
	redistStaticUpdate();

	// change asbr flag
	set_asbr_flag();

	return 0;
}

int cliRedistStaticCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_redist_static = 0;
		shm->config.ospf_static_tag = 0;
		shm->config.ospf_static_metric_type = RCP_OSPF_STATIC_METRIC_TYPE_DEFAULT;
	}
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		// set some defaults
		shm->config.ospf_redist_static = RCP_OSPF_STATIC_METRIC_DEFAULT;
		shm->config.ospf_static_metric_type = RCP_OSPF_STATIC_METRIC_TYPE_DEFAULT;
		shm->config.ospf_static_tag = 0;

		// parse command
		int i;
		for (i = 0; i < argc; i++) {
			if (strcmp(argv[i], "metric") == 0) {
				i++;
				uint32_t metric;
				sscanf(argv[i], "%u", &metric);
				shm->config.ospf_redist_static = metric;
			}
			else if (strcmp(argv[i], "metric-type") == 0) {
				i++;
				uint32_t type;
				sscanf(argv[i], "%u", &type);
				shm->config.ospf_static_metric_type = type;
			}
			else if (strcmp(argv[i], "tag") == 0) {
				i++;
				uint32_t tag;
				sscanf(argv[i], "%u", &tag);
				shm->config.ospf_static_tag = tag;
			}
		}
	}

	// update all static routes
	redistStaticUpdate();

	// change asbr flag
	set_asbr_flag();

	return 0;
}

// send route request message to rip
void rip_route_request(void) {
	if (muxsock) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = RCP_PROC_RIP;
		pkt.sender = RCP_PROC_OSPF;
		pkt.type = RCP_PKT_TYPE_ROUTE_RQ ;
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}
}

int cliRedistRipCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_redist_rip = 0;
		shm->config.ospf_rip_tag = 0;
		shm->config.ospf_rip_metric_type = RCP_OSPF_RIP_METRIC_TYPE_DEFAULT;
		
		// clear all rip lsas
		redistDeleteAllRip();
	}
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		// set some defaults
		shm->config.ospf_redist_rip = RCP_OSPF_RIP_METRIC_DEFAULT;
		shm->config.ospf_rip_metric_type = RCP_OSPF_RIP_METRIC_TYPE_DEFAULT;
		shm->config.ospf_rip_tag = 0;

		// parse command
		int i;
		for (i = 0; i < argc; i++) {
			if (strcmp(argv[i], "metric") == 0) {
				i++;
				uint32_t metric;
				sscanf(argv[i], "%u", &metric);
				shm->config.ospf_redist_rip = metric;
			}
			else if (strcmp(argv[i], "metric-type") == 0) {
				i++;
				uint32_t type;
				sscanf(argv[i], "%u", &type);
				shm->config.ospf_rip_metric_type = type;
			}
			else if (strcmp(argv[i], "tag") == 0) {
				i++;
				uint32_t tag;
				sscanf(argv[i], "%u", &tag);
				shm->config.ospf_rip_tag = tag;
			}
		}
		
		// clear all rip lsas
		redistDeleteAllRip();
		
		// request all rip routes
		rip_route_request();
		
	}

	// change asbr flag
	set_asbr_flag();

	return 0;
}

int cliRedistConnectedCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_redist_connected = 0;
		shm->config.ospf_connected_tag = 0;
		shm->config.ospf_connected_metric_type = RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT;
	}
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		// set some defaults
		shm->config.ospf_redist_connected = RCP_OSPF_CONNECTED_METRIC_DEFAULT;
		shm->config.ospf_connected_metric_type = RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT;
		shm->config.ospf_connected_tag = 0;

		// parse command
		int i;
		for (i = 0; i < argc; i++) {
			if (strcmp(argv[i], "metric") == 0) {
				i++;
				uint32_t metric;
				sscanf(argv[i], "%u", &metric);
				shm->config.ospf_redist_connected = metric;
			}
			else if (strcmp(argv[i], "metric-type") == 0) {
				i++;
				uint32_t type;
				sscanf(argv[i], "%u", &type);
				shm->config.ospf_connected_metric_type = type;
			}
			else if (strcmp(argv[i], "tag") == 0) {
				i++;
				uint32_t tag;
				sscanf(argv[i], "%u", &tag);
				shm->config.ospf_connected_tag = tag;
			}
		}
	}

	// update all connected routes
	redistConnectedUpdate();
	redistConnectedSubnetsUpdate();

	// change asbr flag
	set_asbr_flag();

	return 0;
}

int cliRedistConnectedSubnetsCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_redist_connected_subnets = 0;
	}
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		// set some defaults
		shm->config.ospf_redist_connected_subnets = RCP_OSPF_CONNECTED_METRIC_DEFAULT;

	}

	// update all connected routes
	redistConnectedUpdate();
	redistConnectedSubnetsUpdate();

	// change asbr flag
	set_asbr_flag();

	return 0;
}


//***********************************************************************
// LSA
//***********************************************************************
// use cases:
// 1. the interface is already enabled, redist static is already configured,
//         a new static route is added (redistStaticAdd)
// 2. the interface is already enabled, redist static is already configured,
//         an exiting static route is deleted (redistStaticDel)
// 3. enable an interface, redist static is already configured, all routes on
//         the interface will be redistributed (redistStaticUpdate)
// 4. disable an interface, redist static is already configured, all routes on
//         the interface will stop being redistributed (redistStaticUpdate)
// 5. enable redist static, all static routes on all enabled interfaces need to
//         be redistributed (redistStaticUpdate)
// 6. disable redist static, all static routes lsa will be max-aged and
//         removed (redistStaticUpdate)
// 7. redist static is already configured, new network added or deleted,
//         next hop in lsa might need to be modified as follows:
//             - if route next hop is on an ospf network, next hop in lsa should
//               equal next hop in route
//             - if route next hop is not on an ospf network, next hop in lsa should
//               equal 0 (send the packet to me)
//         (redistStaticUpdate)
// 8. metric and metric type changed (redistStaticUpdate)


uint32_t redist_del_cnt = 0;
uint32_t redist_add_cnt = 0;
uint32_t redist_modif_cnt = 0;

void redistTimeout(void) {
	if (redist_add_cnt || redist_del_cnt) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_SPF,
			"OSPF route redistribution: %u added, %u modified, %u removed",
			redist_add_cnt, redist_modif_cnt, redist_del_cnt);
		redist_del_cnt = 0;
		redist_add_cnt = 0;
		redist_modif_cnt = 0;
	}	
}

void redistStaticAdd(uint32_t ip, uint32_t mask, uint32_t gw, void *interface) {
//printf("redistStaticAdd %d.%d.%d.%d/%d\n",
//	RCP_PRINT_IP(ip),
//	mask2bits(mask));

	// if we have a summary route defined for ip/mask, the route is not added
	RcpOspfSummaryAddr *saddr = summaddr_match(ip, mask);
	if (saddr) {
//printf("matched\n");		
		return;
	}
	
	// is the next hop on an ospf network?
	OspfNetwork *net = networkFindAnyIp(gw);
	if (net == NULL)
		gw = 0;	// set next hop to 0
	
	// maybe we already have this lsa and we don't need to do anything
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		uint32_t metric = shm->config.ospf_redist_static & 0x00ffffff;
		if (shm->config.ospf_static_metric_type == 2)
			metric |= 0x80000000;

		// check next hop, metric, tag, interface
		if (ntohl(lsa->u.ext.fw_address) == gw &&
		     ntohl(lsa->u.ext.tag) == shm->config.ospf_static_tag &&
		     ntohl(lsa->u.ext.type_metric) == metric &&
		     lsa->h.rcpif == interface)
			return;
			
		redist_modif_cnt++;
	}
	else 
		redist_add_cnt++;

	// create a new lsa
	lsa = lsa_originate_ext(ip, mask, gw,
			shm->config.ospf_static_metric_type,
			shm->config.ospf_redist_static,
			shm->config.ospf_static_tag);
			
	if (lsa) {
		redist_add_cnt++;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
			"redistributing static route %d.%d.%d.%d/%d via %d.%d.%d.%d",
			RCP_PRINT_IP(ip),
			mask2bits(mask),
			RCP_PRINT_IP(gw));
		lsa->h.rcpif = interface;
		lsa->h.originator = OSPF_ORIG_STATIC;
		lsadbAdd(0, lsa);
	}	

}

void redistStaticDel(uint32_t ip, uint32_t mask, uint32_t gw) {
	// delete the route regardless of summarisation
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		if (gw != 0)
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
				"redistributing static route delete %d.%d.%d.%d/%d via %d.%d.%d.%d",
				RCP_PRINT_IP(ip),
				mask2bits(mask),
				RCP_PRINT_IP(gw));
		else
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
				"redistributing static route delete %d.%d.%d.%d/%d",
				RCP_PRINT_IP(ip),
				mask2bits(mask));

		// flood lsa with max age
		lsa->header.age = htons(MaxAge);
		lsadbFlood(lsa, 0);

		// remove lsa
		lsadbRemove(0, lsa);
		lsaFree(lsa);
		redist_del_cnt++;
	}
}

static void delete_all_static(void) {
	flush_flooding();
	// remove static external lsa generated by us
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		OspfLsa *next = lsa->h.next;
		
		// do not delete connected, loopbacks and summary, only static
		if (lsa->h.self_originated && lsa->h.originator == OSPF_ORIG_STATIC) {
			lsa->header.age = htons(MaxAge);
			lsadbFlood(lsa, 0);
			lsadbRemove(0, lsa);
			lsaFree(lsa);
			redist_del_cnt++;
		}
		
		lsa = next;
	}
	flush_flooding();
}


void redistStaticUpdate(void) {
	int i;
	RcpStaticRoute *ptr;
	
	if (shm->config.ospf_redist_static == 0) {
		delete_all_static();
		return;
	}

	// go trough static route array and add/delete routes to/from ospf database
	for (i = 0, ptr = shm->config.sroute; i < RCP_ROUTE_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_ROUTE_STATIC) // we don't care about blackhole routes
			continue;

		uint32_t ip = ptr->ip;
		uint32_t mask = ptr->mask;
		uint32_t gw = ptr->gw;

		// distribution of default gateway is controlled by "default-information originate" command
		if (ip == 0 && mask == 0) {
			if (shm->config.ospf_redist_information == 0) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER, "default route not redistributed");
				redistStaticDel(ip, mask, gw);
				continue;
			}
		}

		// no interface configured or interface in shutdown mode
		RcpInterface *interface = rcpFindInterfaceByLPM(shm, gw);
		if (interface == NULL || interface->link_up == 0) {
			redistStaticDel(ip, mask, gw);
			continue;
		}
		
		// add route to ospf
		redistStaticAdd(ip, mask, gw, interface);
	}
}

void redistRipAdd(uint32_t ip, uint32_t mask, uint32_t gw, void *interface) {
//printf("redistStaticAdd %d.%d.%d.%d/%d\n",
//	RCP_PRINT_IP(ip),
//	mask2bits(mask));

	// if we have a summary route defined for ip/mask, the route is not added
	RcpOspfSummaryAddr *saddr = summaddr_match(ip, mask);
	if (saddr) {
//printf("matched\n");		
		return;
	}
	
	// is the next hop on an ospf network?
	OspfNetwork *net = networkFindAnyIp(gw);
	if (net == NULL)
		gw = 0;	// set next hop to 0
	
	// maybe we already have this lsa and we don't need to do anything
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		uint32_t metric = shm->config.ospf_redist_rip & 0x00ffffff;
		if (shm->config.ospf_rip_metric_type == 2)
			metric |= 0x80000000;

		// check next hop, metric, tag, interface
		if (ntohl(lsa->u.ext.fw_address) == gw &&
		     ntohl(lsa->u.ext.tag) == shm->config.ospf_rip_tag &&
		     ntohl(lsa->u.ext.type_metric) == metric &&
		     lsa->h.rcpif == interface)
			return;
			
		redist_modif_cnt++;
	}
	else 
		redist_add_cnt++;

	// create a new lsa
	lsa = lsa_originate_ext(ip, mask, gw,
			shm->config.ospf_rip_metric_type,
			shm->config.ospf_redist_rip,
			shm->config.ospf_rip_tag);
			
	if (lsa) {
		redist_add_cnt++;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
			"redistributing rip route %d.%d.%d.%d/%d via %d.%d.%d.%d",
			RCP_PRINT_IP(ip),
			mask2bits(mask),
			RCP_PRINT_IP(gw));
		lsa->h.rcpif = interface;
		lsa->h.originator = OSPF_ORIG_RIP;
		lsadbAdd(0, lsa);
	}	

}


void redistRipDel(uint32_t ip, uint32_t mask, uint32_t gw) {
	// delete the route regardless of summarisation
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		if (gw != 0)
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
				"redistributing rip route delete %d.%d.%d.%d/%d via %d.%d.%d.%d",
				RCP_PRINT_IP(ip),
				mask2bits(mask),
				RCP_PRINT_IP(gw));
		else
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
				"redistributing rip route delete %d.%d.%d.%d/%d",
				RCP_PRINT_IP(ip),
				mask2bits(mask));

		// flood lsa with max age
		lsa->header.age = htons(MaxAge);
		lsadbFlood(lsa, 0);

		// remove lsa
		lsadbRemove(0, lsa);
		lsaFree(lsa);
		redist_del_cnt++;
	}
}

void redistDeleteAllRip(void) {
	flush_flooding();
	// remove static external lsa generated by us
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		OspfLsa *next = lsa->h.next;
		
		// do not delete connected, loopbacks and summary, only static
		if (lsa->h.self_originated && lsa->h.originator == OSPF_ORIG_RIP) {
			lsa->header.age = htons(MaxAge);
			lsadbFlood(lsa, 0);
			lsadbRemove(0, lsa);
			lsaFree(lsa);
			redist_del_cnt++;
		}
		
		lsa = next;
	}
	flush_flooding();
}


static void redistConnectedAdd(uint32_t ip, uint32_t mask, uint32_t gw, void *interface,
	uint32_t in_metric, uint32_t in_type, uint8_t orig) {

	// if we have a summary route defined for ip/mask, the route is not added
	RcpOspfSummaryAddr *saddr = summaddr_match(ip, mask);
	if (saddr) {
//printf("matched\n");		
		return;
	}

	// is the next hop on an ospf network?
	OspfNetwork *net = networkFindAnyIp(gw);
	if (net == NULL)
		gw = 0;	// set next hop to 0
	
	// maybe we already have this lsa and we don't need to do anything
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));

	if (lsa && lsa->h.self_originated) {
		uint32_t metric = in_metric  & 0x00ffffff;
		if (in_type == 2)
			metric |= 0x80000000;

		// check next hop, metric, tag, interface
		if (ntohl(lsa->u.ext.fw_address) == gw &&
		     ntohl(lsa->u.ext.tag) == shm->config.ospf_connected_tag &&
		     ntohl(lsa->u.ext.type_metric) == metric &&
		     lsa->h.rcpif == interface)
			return;
			
		redist_modif_cnt++;
	}
	else 
		redist_add_cnt++;

	// create a new lsa
	lsa = lsa_originate_ext(ip, mask, gw,
			in_type,
			in_metric,
			shm->config.ospf_connected_tag);
			
	if (lsa) {
		redist_add_cnt++;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER, "redistributing connected route %d.%d.%d.%d/%d via %d.%d.%d.%d",
			RCP_PRINT_IP(ip),
			mask2bits(mask),
			RCP_PRINT_IP(gw));
		lsa->h.rcpif = interface;
		lsa->h.originator = orig;
		lsadbAdd(0, lsa);
	}	

}



void redistConnectedDel(uint32_t ip, uint32_t mask, uint32_t gw) {
	// delete the route regardless summarisation
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER, "redistributing connected route delete %d.%d.%d.%d/%d via %d.%d.%d.%d",
			RCP_PRINT_IP(ip),
			mask2bits(mask),
			RCP_PRINT_IP(gw));

		// flood lsa with max age
		lsa->header.age = htons(MaxAge);
		lsadbFlood(lsa, 0);

		// remove lsa
		lsadbRemove(0, lsa);
		lsaFree(lsa);
		redist_del_cnt++;
	}
}



void redistConnectedUpdate(void) {
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type == IF_LOOPBACK || intf->ip == 0)
			continue;
		
		// skip ospf interfaces
		if (shm_match_network(intf->ip, intf->mask))
			continue;

		// add/remove lsa
		if (shm->config.ospf_redist_connected && intf->admin_up && intf->link_up)
			redistConnectedAdd(intf->ip & intf->mask, intf->mask, 0, intf,
				shm->config.ospf_redist_connected,
				shm->config.ospf_connected_metric_type,
				OSPF_ORIG_CONNECTED);
		else
			redistConnectedDel(intf->ip & intf->mask, intf->mask, 0);
	}
}

void redistConnectedSubnetsUpdate(void) {
	int i;
	RcpInterface *intf = &shm->config.intf[0];
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++, intf++) {
		// skip unconfigured and loopback interfaces
		if (intf->name[0] == '\0' || intf->type != IF_LOOPBACK || intf->ip == 0)
			continue;
		
		if (!rcpInterfaceVirtual(intf->name))
			continue;
		
		// skip ospf interfaces
		if (shm_match_network(intf->ip, intf->mask))
			continue;

		// add/remove lsa
		if (shm->config.ospf_redist_connected_subnets)
			redistConnectedAdd(intf->ip & intf->mask, intf->mask, 0, intf,
				RCP_OSPF_CONNECTED_METRIC_DEFAULT,
				RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT,
				OSPF_ORIG_LOOPBACK);
		else
			redistConnectedDel(intf->ip & intf->mask, intf->mask, 0);
	}
}

void redistSummaryAdd(uint32_t ip, uint32_t mask) {
	// don't bother if redist static and redist connected are not enabled
	if (shm->config.ospf_redist_static == 0 &&
	    shm->config.ospf_redist_connected == 0 &&
	    shm->config.ospf_redist_connected_subnets == 0)
		return;

	uint32_t gw = 0;

	// take the metric from redist static configuration
	uint32_t metric = shm->config.ospf_redist_static & 0x00ffffff;
	if (metric == 0)
		metric = RCP_OSPF_STATIC_METRIC_DEFAULT;
	if (shm->config.ospf_static_metric_type == 2)
		metric |= 0x80000000;
		
	// maybe we already have this lsa and we don't need to do anything
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		// check next hop, metric, tag, interface
		if (ntohl(lsa->u.ext.fw_address) == gw &&
		     ntohl(lsa->u.ext.tag) == shm->config.ospf_static_tag &&
		     ntohl(lsa->u.ext.type_metric) == metric &&
		     lsa->h.rcpif == NULL)
			return;
			
		redist_modif_cnt++;
	}
	else 
		redist_add_cnt++;

	// create a new lsa
	lsa = lsa_originate_ext(ip, mask, gw,
			shm->config.ospf_static_metric_type,
			metric,
			shm->config.ospf_static_tag);
			
	if (lsa) {
		redist_add_cnt++;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
			"redistributing summary route %d.%d.%d.%d/%d",
			RCP_PRINT_IP(ip),
			mask2bits(mask));
		lsa->h.rcpif = NULL;
		lsa->h.originator = OSPF_ORIG_SUMMARY;
		lsadbAdd(0, lsa);
		
		// remove any static lsa replaced by this
		flush_flooding();
		// remove static external lsa generated by us
		OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
		while (lsa != NULL) {
			OspfLsa *next = lsa->h.next;
			
			// do not delete summary
			if (lsa->h.self_originated && lsa->h.originator != OSPF_ORIG_SUMMARY) {
				ExtLsaData *rdata = &lsa->u.ext;
				uint32_t lsa_mask = ntohl(rdata->mask);
				uint32_t lsa_ip = ntohl(lsa->header.link_state_id);

				// if we have a summary route defined for ip/mask, the route is not deleted
				RcpOspfSummaryAddr *saddr = summaddr_match(lsa_ip, lsa_mask);
				if (saddr) {
					lsa->header.age = htons(MaxAge);
					lsadbFlood(lsa, 0);
					lsadbRemove(0, lsa);
					lsaFree(lsa);
					redist_del_cnt++;
				}
			}
			
			lsa = next;
		}
		flush_flooding();
		
	}	

}

void redistSummaryNotAdvertise(uint32_t ip, uint32_t mask) {
	// don't bother is redist static is not enabled
	if (shm->config.ospf_redist_static == 0)
		return;

	// delete the advertisment in case we have any
	redistSummaryDel(ip, mask);
	
	// delete all static routes in the range if we have any
	flush_flooding();
	// remove static external lsa generated by us
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		OspfLsa *next = lsa->h.next;
		
		// do not delete summary
		if (lsa->h.self_originated && lsa->h.originator  != OSPF_ORIG_SUMMARY) {
			ExtLsaData *rdata = &lsa->u.ext;
			uint32_t lsa_mask = ntohl(rdata->mask);
			uint32_t lsa_ip = ntohl(lsa->header.link_state_id);

			// if we have a summary route defined for ip/mask, the route is not deleted
			RcpOspfSummaryAddr *saddr = summaddr_match(lsa_ip, lsa_mask);
			if (saddr) {
				lsa->header.age = htons(MaxAge);
				lsadbFlood(lsa, 0);
				lsadbRemove(0, lsa);
				lsaFree(lsa);
				redist_del_cnt++;
			}
		}
		
		lsa = next;
	}
	flush_flooding();
	

}

void redistSummaryDel(uint32_t ip, uint32_t mask) {
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	if (lsa && lsa->h.self_originated) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER,
			"redistributing summary route delete %d.%d.%d.%d/%d",
			RCP_PRINT_IP(ip),
			mask2bits(mask));

		// flood lsa with max age
		lsa->header.age = htons(MaxAge);
		lsadbFlood(lsa, 0);

		// remove lsa
		lsadbRemove(0, lsa);
		lsaFree(lsa);
		redist_del_cnt++;
	}
}


