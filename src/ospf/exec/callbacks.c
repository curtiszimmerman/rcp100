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
#include "route.h"
RcpPkt *pktout = NULL;
extern RcpProcStats *pstats;	// process statistics
extern int cliClearIpOspfCmd(CliMode mode, int argc, char **argv);

// cli html flag
int cli_html = 0;

// ABR/ASBR router flags
int is_abr = 0;

//******************************************************************
// callbacks
//******************************************************************
int processCli(RcpPkt *pkt) {
	// execute cli command
	pktout = pkt;
	cli_html = pktout->pkt.cli.html;

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



//******************************************************************
// configuration
//******************************************************************
int cliRouterOspfCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_OSPF;
	pktout->pkt.cli.mode_data  = NULL;

	return 0;
}

int cliRouterId(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	uint32_t ip;
	if (atoip(argv[1], &ip)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	shm->config.ospf_router_id = ip;
	
	// if any area defined
	if (areaGetList())
		strcpy(data, "Please execute 'clear ip ospf' to activate the new router ID\n");
		
	return 0;
}


OspfArea *createOspfArea(uint32_t area_id) {
	// maybe we need to generate a router_id
	if (!shm->config.ospf_router_id)
		generate_router_id();
	if (!shm->config.ospf_router_id) {
		return NULL;
	}

	// create area
	OspfArea *a = malloc(sizeof(OspfArea));
	if (!a) {
		return NULL;
	}
	memset(a, 0, sizeof(OspfArea));
	a->router_id = shm->config.ospf_router_id;
	a->area_id = area_id;

	// add area
	if (areaAdd(a)) {
		free(a);
		return NULL;
	}
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_INTERFACE,
		"area %u created", area_id);
	
	// update abr/asbr flags
	if (areaCount() > 1)
		is_abr = 1;
	else
		is_abr = 0;
	
	
	// originate a router lsa
	lsa_originate_rtr_update();

	return a;
}

int cliNoNetworkAllCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		RcpOspfArea *a = &shm->config.ospf_area[i];
		if (!a->valid)
			continue;
		
		int j;
		for (j = 0; j < RCP_OSPF_NETWORK_LIMIT; j++)
			a->network[j].valid = 0;
	}
	
	return 0;
}

int cliNetworkCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	int noform = 0;
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}

	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[index], &ip, &mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}

	// this should be a network address
	if (ip & (~mask)) {
		strcpy(data, "Error: invalid IP address\n");
		return RCPERR;
	}
	
	index += 2;
	uint32_t area_id;
	sscanf(argv[index], "%u", &area_id);
	
	RcpOspfArea *area = get_shm_area(area_id);
	RcpOspfNetwork *network = get_shm_network(area_id, ip, mask);
	RcpOspfNetwork *any_network = get_shm_any_network(ip, mask);

	if (noform) {
		// are area and network configured?
		if (area == NULL || network == NULL)
			return 0;
			
		// clear shared memory
		memset(network, 0, sizeof(RcpOspfNetwork));
	}
	else {
		if (!network && any_network) {
			strcpy(data, "Error: network already defined in a different area\n");
			return RCPERR;
		}

		if (!area) {
			area = get_shm_area_empty();
			if (!area) {
				strcpy(data, "Error: network not configured, OSPF area limit reached\n");
				return RCPERR;
			}
			memset(area, 0, sizeof(RcpOspfArea));
			rcpOspfAreaSetDefaults(area);
			area->area_id = area_id;
			area->valid = 1;

			// create ospf area
			OspfArea *a = createOspfArea(area_id);
			if (!a) {
				area->valid = 0;
				strcpy(data, "Error: cannot create OSPF area\n");
				return RCPERR;
			}
		}
		
		if (!network) {
			network = get_shm_network_empty(area_id);
			if (!network) {
				strcpy(data, "Error: network not configured, OSPF network limit reached\n");
				return RCPERR;
			}
			
			memset(network, 0, sizeof(RcpOspfNetwork));
			network->ip  = ip;
			network->mask = mask;
			network->area_id = area_id;
			network->valid = 1;
		}			
	}
	
//	// we do not attempt to enable/disable ospf on the interface at this time
//	// this is left to the interface monitoring task
	return 0;
}

int cliPassiveInterfaceCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// maybe we need to generate a router_id
	if (!shm->config.ospf_router_id)
		generate_router_id();
	if (!shm->config.ospf_router_id) {
		return RCPERR;
	}

	int noform = 0;
	int index = 1;
	if (strcmp(argv[0], "no") == 0) {
		noform = 1;
		index = 2;
	}
	
	// find the interface
	RcpInterface *intf = rcpFindInterfaceByName(shm, argv[index]);
	if (intf == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot find the interface\n");
		return RCPERR;
	}

	if (noform) {
		intf->ospf_passive_interface = 0;
	}
	else {	
		intf->ospf_passive_interface = 1;
	}
	
	return 0;
}


int cliTimersSpf(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		shm->config.ospf_spf_delay = RCP_OSPF_SPF_DELAY_DEFAULT;
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		uint32_t tmp;
		sscanf(argv[2], "%u", &tmp);
		shm->config.ospf_spf_delay = tmp;
		
		
		if (argc == 4) {
			sscanf(argv[3], "%u", &tmp);
			shm->config.ospf_spf_hold = tmp;
		}

		
	}
	
	return 0;
}

int cliOspfLogAdjacency(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.ospf_log_adjacency = 0;
		shm->config.ospf_log_adjacency_detail = 0;
	}
	else {
		// maybe we need to generate a router_id
		if (!shm->config.ospf_router_id)
			generate_router_id();
		if (!shm->config.ospf_router_id) {
			strcpy(data, "Error: Please configure a router ID\n");
			return RCPERR;
		}

		shm->config.ospf_log_adjacency = 1;
		shm->config.ospf_log_adjacency_detail = 0;
		if (argc == 3)
			shm->config.ospf_log_adjacency_detail = 1;
	}
	
	return 0;
}


int cliAreaStub(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	int noform = 0;
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}
	uint32_t area_id;
	sscanf(argv[index], "%u", &area_id);

	int changed = 0;
	
	// update shm and OspfArea
	RcpOspfArea *area = get_shm_area(area_id);
	if (noform) {
		// is area configured?
		if (area == NULL)
			return 0;
		
		// clear shared memory data
		area->stub = 0;
		area->no_summary = 0;
		area->default_cost = RCP_OSPF_DEFAULT_COST;
		
		// clear stub flag in  ospf area
		OspfArea *a = areaFind(area_id);
		if (a) {
			if (a->stub != 0)
				changed = 1;
			a->stub = 0;
			a->no_summary = 0;
		}
	}
	else {
		if (!area) {
			area = get_shm_area_empty();
			if (!area) {
				strcpy(data, "Error: stub area not configured, OSPF area limit reached\n");
				return RCPERR;
			}
			memset(area, 0, sizeof(RcpOspfArea));
			rcpOspfAreaSetDefaults(area);
			area->area_id = area_id;
			area->valid = 1;
			area->stub = 1;
			if (argc == 4)
				area->no_summary = 1;
			else
				area->no_summary = 0;

			// create ospf area
			OspfArea *a = createOspfArea(area_id);
			if (!a) {
				area->valid = 0;
				strcpy(data, "Error: cannot create OSPF area\n");
				return RCPERR;
			}
			a->stub = 1;
			if (argc == 4)
				a->no_summary = 1;
			else
				a->no_summary = 0;
		}
		else {
			area->stub = 1;
			if (argc == 4)
				area->no_summary = 1;
			else
				area->no_summary = 0;
			// default cost remains unchanged
			
			OspfArea *a = areaFind(area_id);
			if (a) {
				if (a->stub != 1)
					changed = 1;
					
				a->stub = 1;
				if (argc == 4)
					a->no_summary = 1;
				else
					a->no_summary = 0;
			}
		}
	}

	// force a clear ip ospf command
	if (changed)
		cliClearIpOspfCmd(mode, argc, argv);

	// update abr/asbr flags
	if (areaCount() > 1)
		is_abr = 1;
	else
		is_abr = 0;
		
	// delete default summary lsa
	ASSERT(shm->config.ospf_router_id);
	OspfLsa *found = lsadbFind(area_id, LSA_TYPE_SUM_NET, 0, htonl(shm->config.ospf_router_id));
	if (found && found->h.self_originated) {
		if (!is_abr || noform) {
			// remove existing lsa
			lsaListRemove(lsadbGetListHead(area_id, found->header.type), found);
			lsaFree(found);
		}
	}
	else if (is_abr && !noform) {
		// originate a new lsa
		OspfLsa *newlsa =  lsa_originate_sumdefault(area_id, area->default_cost);
		if (newlsa)
			lsadbAdd(area->area_id, newlsa);
	}
	
	// trigger spf - this will also update all summary lsa for the area
	spfTrigger();

	return 0;
}

int cliAreaDefaultCost(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	int noform = 0;
	
	// extract data
	int index = 0;
	if (strcmp(argv[0], "no") == 0) {
		index = 2;
		noform = 1;
	}
	else {
		index = 1;
	}
	uint32_t area_id;
	sscanf(argv[index], "%u", &area_id);
	uint32_t cost = 0;
	if (!noform)
		sscanf(argv[index + 2], "%u", &cost);

	// update shm and OspfArea
	RcpOspfArea *area = get_shm_area(area_id);
	if (noform) {
		// is area configured?
		if (area == NULL)
			return 0;
		area->default_cost = RCP_OSPF_DEFAULT_COST;
	}
	else {
		if (!area || !area->stub) {
			strcpy(data, "Error: please define the stub area first\n");
			return RCPERR;
		}
		else
			area->default_cost = cost;
	}
			
	// update abr/asbr flags
	if (areaCount() > 1)
		is_abr = 1;
	else
		is_abr = 0;

	// originate a new lsa
	OspfLsa *newlsa =  lsa_originate_sumdefault(area_id, cost);
	if (newlsa)
		lsadbAdd(area->area_id, newlsa);

	// trigger spf - this will also update all summary lsa for the area
	spfTrigger();

	return 0;
}

//int cliDefaultInformationOriginateCmd(CliMode mode, int argc, char **argv) {
//	char *data = (char *) pktout + sizeof(RcpPkt);
//	*data = '\0';
//	
//	if (strcmp(argv[0], "no") == 0) {
//		shm->config.ospf_default_information = 0;
//		ospfdb_delete_type(RCP_ROUTE_STATIC);	// this will also remove the default routes
//	}
//	else {
//		shm->config.ospf_default_information = 1;
//	}
//		
//	// adding back the static routes
//	ospf_update_static();
//	return 0;
//}

//int cliUpdateTimerCmd(CliMode mode, int argc, char **argv) {
//	char *data = (char *) pktout + sizeof(RcpPkt);
//	*data = '\0';
//	
//	if (strcmp(argv[0], "no") == 0)
//		shm->config.ospf_update_timer = RCP_OSPF_DEFAULT_TIMER;
//	else
//		shm->config.ospf_update_timer = atoi(argv[1]);
//	return 0;
//}


//***************************************************************
// show commands
//***************************************************************
int cliShowIpOspfProcessCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	// process time
	fprintf(fp, "OSPF process ID %u\n", pstats->pid);
	fprintf(fp, "\tMaximum incoming packet processing time %ums\n", systic_delta_pkt * 10);
	fprintf(fp, "\tMaximum SPF calculation time %ums\n", systic_spf_delta * 10);
	fprintf(fp, "\tMaximum LSA flooding time %ums\n", systic_delta_lsa * 10);
	fprintf(fp, "\tMaximum retransmit time %ums\n", systic_delta_network * 10);
	fprintf(fp, "\tMaximum integrity check time %ums\n", systic_delta_integrity * 10);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	// general data
	fprintf(fp, "OSPF router id %d.%d.%d.%d\n", RCP_PRINT_IP(shm->config.ospf_router_id));
	fprintf(fp, "Conforms to RFC2328, RFC1583 Compatibility flag is disabled\n");
	fprintf(fp, "Supports only single TOS(TOS0) routes\n");
	if (is_abr)
		fprintf(fp, "This is an Area Border Router\n");
	if (is_asbr)
		fprintf(fp, "This is an AS Border Router\n");

	// spf
	fprintf(fp, "SPF schedule delay %u seconds, hold time %u seconds\n",
		shm->config.ospf_spf_delay, shm->config.ospf_spf_hold);
	fprintf(fp, "Number of SPF calculations %u\n",
		shm->stats.ospf_spf_calculations);
	if (shm->stats.ospf_last_spf)
		fprintf(fp, "Last SPF calculation %s", ctime(&shm->stats.ospf_last_spf));

	// lsa
	int cnt = lsa_count_external();
	fprintf(fp, "Number of external LSA %d\n", cnt);
	cnt = lsa_count_originated();
	fprintf(fp, "Number of LSA originated %d\n", cnt);

	// print area information
	OspfArea *area = areaGetList();
	while (area) {
		fprintf(fp, "Area %u:\n", area->area_id);
		cnt = lsa_count_area(area->area_id);
		fprintf(fp, "   Number of LSA %d\n", cnt);
		
		// print network information
		OspfNetwork *net = area->network;
		while (net) {
			// count neighbors
			int total = 0;
			int down = 0;
			int twoway = 0;
			int full = 0;
			OspfNeighbor *nb = net->neighbor;
			while (nb) {
				total++;
				if (nb->state < NSTATE_2WAY)
					down++;
				else if (nb->state < NSTATE_FULL)
					twoway++;
				else if (nb->state == NSTATE_FULL)
					full++;

				nb = nb->next;
			}			
			
			fprintf(fp, "   Network %d.%d.%d.%d/%d, %d neighbors (%d down, %d 2way, %d full)\n",
				RCP_PRINT_IP(net->ip & net->mask),
				mask2bits(net->mask),
				total, down, twoway, full);
			net = net->next;
		}
		
		area = area->next;
	}
	
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfInterfaceCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	OspfArea *area = areaGetList();
	while (area) {
		OspfNetwork *net = area->network;
		while (net) {
			RcpInterface *intf = rcpFindInterface(shm, net->ip);
			if (intf && *intf->name != '\0') {
				fprintf(fp, "Interface %s, admin state %s, link state %s\n",
					intf->name,
					(intf->admin_up)? "UP": "DOWN",
					(intf->link_up)? "UP": "DOWN");
				fprintf(fp, "   IP address %d.%d.%d.%d/%d, MTU %u\n",
					RCP_PRINT_IP(intf->ip),
					mask2bits(intf->mask),
					intf->mtu);
				fprintf(fp, "   Router ID %d.%d.%d.%d, network type BROADCAST, cost %u\n",
					RCP_PRINT_IP(shm->config.ospf_router_id),
					intf->ospf_cost);
				fprintf(fp, "   Transmit delay is 0, network state %s, priority %u\n",
					netfsmState2String(net->state),
					intf->ospf_priority);
				fprintf(fp, "   Hello interval %u, dead interval %u, wait time %u, retransmit interval %u\n",
					intf->ospf_hello,
					intf->ospf_dead,
					intf->ospf_dead,
					intf->ospf_rxmt);
				fprintf(fp, "   Designated router %d.%d.%d.%d, backup designated router %d.%d.%d.%d\n",
					RCP_PRINT_IP(net->designated_router),	
					RCP_PRINT_IP(net->backup_router));

				// packet counts
				fprintf(fp, "   Hello packets received %u, sent %u, errors %u\n",
					intf->stats.ospf_rx_hello, intf->stats.ospf_tx_hello, intf->stats.ospf_rx_hello_errors);
				fprintf(fp, "   Database Description packets received %u, sent %u, rxmt %u, errors %u\n",
					intf->stats.ospf_rx_db, intf->stats.ospf_tx_db, intf->stats.ospf_tx_db_rxmt, intf->stats.ospf_rx_db_errors);
				fprintf(fp, "   LS Request packets received %u, sent %u, rxmt %u, errors %u\n",
					intf->stats.ospf_rx_lsrq, intf->stats.ospf_tx_lsrq, intf->stats.ospf_tx_lsrq_rxmt, intf->stats.ospf_rx_lsrq_errors);
				fprintf(fp, "   LS Update packets received %u, sent %u, rxmt %u, errors %u\n",
					intf->stats.ospf_rx_lsup, intf->stats.ospf_tx_lsup, intf->stats.ospf_tx_lsup_rxmt, intf->stats.ospf_rx_lsup_errors);
				fprintf(fp, "   LS Acknowledgment packets received %u, sent %u, errors %u\n",
					intf->stats.ospf_rx_lsack, intf->stats.ospf_tx_lsack, intf->stats.ospf_rx_lsack_errors);
				
				// neighbor counts
				int total = 0;
				int full = 0;
				OspfNeighbor *nb = net->neighbor;
				while (nb) {
					total++;
					if (nb->state == NSTATE_FULL)
						full++;
	
					nb = nb->next;
				}			
				
				fprintf(fp, "   Neighbor count is %d, adjacent neighbor count is %d\n",
					total, full);

			}
			
			net = net->next;
		}
		
		area = area->next;
	}

	fclose(fp);
	return 0;
}

int cliShowIpOspfNeighborCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

 	showNeighbors(fp);
 	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (argc == 4)
		showOspfDatabase(fp);
	else if (argc == 5) {
		if (strcmp(argv[4], "detail") == 0)
			showOspfDatabaseDetail(fp);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfAreaDatabaseCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	uint32_t area;
	sscanf(argv[3], "%u", &area);

	if (argc == 5)
		showOspfAreaDatabase(fp, area);
	else if (argc == 6) {
		if (strcmp(argv[5], "detail") == 0)
			showOspfAreaDatabaseDetail(fp, area);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}


int cliShowIpOspfDatabaseNetworkCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (argc == 5) {
		showOspfDatabaseNetwork(fp);
	}
	else if (argc == 6) {
		if (strcmp(argv[5], "detail") == 0)
			showOspfDatabaseNetworkDetail(fp);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfAreaDatabaseNetworkCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	uint32_t area;
	sscanf(argv[3], "%u", &area);

	if (argc == 6) {
		showOspfAreaDatabaseNetwork(fp, area);
	}
	else if (argc == 7) {
		if (strcmp(argv[6], "detail") == 0)
			showOspfAreaDatabaseNetworkDetail(fp, area);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseRouterCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (argc == 5)
		showOspfDatabaseRouter(fp);
	else if (argc == 6) {
		if (strcmp(argv[5], "detail") == 0)
			showOspfDatabaseRouterDetail(fp);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfAreaDatabaseRouterCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	uint32_t area;
	sscanf(argv[3], "%u", &area);

	if (argc == 6)
		showOspfAreaDatabaseRouter(fp, area);
	else if (argc == 7) {
		if (strcmp(argv[6], "detail") == 0)
			showOspfAreaDatabaseRouterDetail(fp, area);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseSummaryCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (argc == 5)
		showOspfDatabaseSummary(fp);
	else if (argc == 6) {
		if (strcmp(argv[5], "detail") == 0)
			showOspfDatabaseSummaryDetail(fp);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfAreaDatabaseSummaryCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	uint32_t area;
	sscanf(argv[3], "%u", &area);

	if (argc == 6)
		showOspfAreaDatabaseSummary(fp, area);
	else if (argc == 7) {
		if (strcmp(argv[6], "detail") == 0)
			showOspfAreaDatabaseSummaryDetail(fp, area);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseAsbrSummaryCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (argc == 5)
		showOspfDatabaseAsbrSummary(fp);
	else if (argc == 6) {
		if (strcmp(argv[5], "detail") == 0)
			showOspfDatabaseAsbrSummaryDetail(fp);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfAreaDatabaseAsbrSummaryCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	uint32_t area;
	sscanf(argv[3], "%u", &area);

	if (argc == 6)
		showOspfAreaDatabaseAsbrSummary(fp, area);
	else if (argc == 7) {
		if (strcmp(argv[6], "detail") == 0)
			showOspfAreaDatabaseAsbrSummaryDetail(fp, area);
	}
	else
		ASSERT(0);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseSelfOriginateCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	showOspfDatabaseSelfOriginateDetail(fp);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfAreaDatabaseSelfOriginateCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;
	
	uint32_t area;
	sscanf(argv[3], "%u", &area);

	showOspfAreaDatabaseSelfOriginateDetail(fp, area);
	
	fclose(fp);
	return 0;
}


int cliShowIpOspfDatabaseExternalCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	showOspfDatabaseExternal(fp);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseExternalDetailCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	showOspfDatabaseExternalDetail(fp);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfDatabaseExternalBriefCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	showOspfDatabaseExternalBrief(fp);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfRouteCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	if (argc == 5)
		showRoutesEcmp(fp);
	else
		showRoutes(fp);
	
	fclose(fp);
	return 0;
}

int cliShowIpOspfBorderRoutersCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	showBorder(fp);
	
	fclose(fp);
	return 0;
}

//***************************************************************
// operation
//***************************************************************
int cliClearIpOspfForceSpf(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	spfTrigger();
	return 0;
}

int cliClearIpOspfStatistics(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	shm->stats.ospf_spf_calculations = 0;
	shm->stats.ospf_last_spf = 0;
	
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		intf->stats.ospf_rx_hello = 0;
		intf->stats.ospf_rx_hello_errors = 0;
		intf->stats.ospf_tx_hello = 0;
		intf->stats.ospf_rx_db = 0;
		intf->stats.ospf_rx_db_errors = 0;
		intf->stats.ospf_tx_db = 0;
		intf->stats.ospf_tx_db_rxmt = 0;
		intf->stats.ospf_rx_lsrq = 0;
		intf->stats.ospf_rx_lsrq_errors = 0;
		intf->stats.ospf_tx_lsrq = 0;
		intf->stats.ospf_tx_lsrq_rxmt = 0;
		intf->stats.ospf_rx_lsup = 0;
		intf->stats.ospf_rx_lsup_errors = 0;
		intf->stats.ospf_tx_lsup = 0;
		intf->stats.ospf_tx_lsup_rxmt = 0;
		intf->stats.ospf_rx_lsack = 0;
		intf->stats.ospf_rx_lsack_errors = 0;
		intf->stats.ospf_tx_lsack = 0;
	}
				
	return 0;
}



int cliDebugRoutePrint(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// transport file
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	route_print_debug(fp);
	fclose(fp);
	return 0;
}

