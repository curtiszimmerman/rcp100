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

static void print_lsa_router(FILE *fp, OspfLsa *lsa) {
	// links
	RtrLsaData *rdata = &lsa->u.rtr;
	int cnt = ntohs(rdata->links);
	
	uint8_t flags = rdata->flags;
	char ftype[20] = "";
	// virtual link endpoint
	if (flags & 0x4) {
		strcpy(ftype, "V");
	}
	// ASBR
	if (flags & 0x2) {
		if (ftype[0] != '\0')
			strcat(ftype, ", ");
		strcat(ftype, "E");
	}
	// ABR
	if (flags & 1) {
		if (ftype[0] != '\0')
			strcat(ftype, ", ");
		strcat(ftype, "B");
	}	
	
	if (ftype[0] != '\0')
		fprintf(fp, "   Router-LSA flags 0x%02x (%s), number of links %d\n", flags, ftype, cnt);
	else
		fprintf(fp, "   Router-LSA flags 0x%02x, number of links %d\n", flags, cnt);
	
	// bring in the networks
	RtrLsaLink *lnk = (RtrLsaLink *) ((uint8_t *) rdata + sizeof(RtrLsaData));
	while (cnt > 0) {
		if (lnk->type == 1) {
			fprintf(fp, "   Link type 1 (point-to-point connection to another router)\n");
			fprintf(fp, "      Router ID %d.%d.%d.%d, ifIndex 0x%08x\n",
				RCP_PRINT_IP(ntohl(lnk->link_id)),
				ntohl(lnk->link_data));
			fprintf(fp, "      TOS %u, metric %u\n",
				lnk->tos,
				ntohs(lnk->metric));
		}
		else if (lnk->type == 2) {
			fprintf(fp, "   Link type 2 (connection to a transit network)\n");
			fprintf(fp, "      Designated router %d.%d.%d.%d, router interface %d.%d.%d.%d\n",
				RCP_PRINT_IP(ntohl(lnk->link_id)),
				RCP_PRINT_IP(ntohl(lnk->link_data)));
			fprintf(fp, "      TOS %u, metric %u\n",
				lnk->tos,
				ntohs(lnk->metric));
		}
		else if (lnk->type == 3) {
			fprintf(fp, "   Link type 3 (connection to a stub network)\n");
			fprintf(fp, "      Network %d.%d.%d.%d, mask %d.%d.%d.%d\n",
				RCP_PRINT_IP(ntohl(lnk->link_id)),
				RCP_PRINT_IP(ntohl(lnk->link_data)));
			fprintf(fp, "      TOS %u, metric %u\n",
				lnk->tos,
				ntohs(lnk->metric));
		}
		else if (lnk->type == 4) {
			fprintf(fp, "   Link type 4 (virtual link)\n");
			fprintf(fp, "      Router ID %d.%d.%d.%d, router interface %d.%d.%d.%d\n",
				RCP_PRINT_IP(ntohl(lnk->link_id)),
				RCP_PRINT_IP(ntohl(lnk->link_data)));
			fprintf(fp, "      TOS %u, metric %u\n",
				lnk->tos,
				ntohs(lnk->metric));
		}

		lnk++;
		cnt--;
	}
}

static void print_lsa_network(FILE *fp, OspfLsa *lsa) {
	// links
	NetLsaData *rdata = &lsa->u.net;
	int cnt = (ntohs(lsa->header.length) - sizeof(OspfLsaHeader) - sizeof(NetLsaData)) / 4 ;
	fprintf(fp, "   Network-LSA netmask %d.%d.%d.%d, count %d\n",
		RCP_PRINT_IP(ntohl(rdata->mask)),
		cnt);
	uint32_t *ptr = (uint32_t *) ((uint8_t *) rdata + sizeof(NetLsaData));
	while (cnt > 0) {
		fprintf(fp, "   Attached router %d.%d.%d.%d\n",
			RCP_PRINT_IP(ntohl(*ptr)));
		ptr++;
		cnt--;
	}
}

static void print_lsa_external(FILE *fp, OspfLsa *lsa) {
	ExtLsaData *rdata = &lsa->u.ext;
	uint8_t type = (ntohl(rdata->type_metric) & 0x80000000)? 2: 1;
	uint32_t metric = ntohl(rdata->type_metric) & 0x00ffffff;
	fprintf(fp, "   AS-external-LSA type E%u, metric %u, mask %d.%d.%d.%d\n",
		type, metric,
		RCP_PRINT_IP(ntohl(rdata->mask)));
	fprintf(fp, "   Forwarding address %d.%d.%d.%d, tag %u\n", 
		RCP_PRINT_IP(ntohl(rdata->fw_address)), 
		ntohl(rdata->tag));
	switch (lsa->h.originator) {
		case 0: fprintf(fp, "   Origin: external\n"); break;
		case 1: fprintf(fp, "   Origin: static route\n"); break;
		case 2: fprintf(fp, "   Origin: connected route\n"); break;
		case 3: fprintf(fp, "   Origin: loopback route\n"); break;
		case 4: fprintf(fp, "   Origin: summary address route\n"); break;
		default:
			ASSERT(0);
	}
}

static void print_lsa_summary_network(FILE *fp, OspfLsa *lsa) {
	SumLsaData *rdata = &lsa->u.sum;
	uint32_t metric = ntohl(rdata->metric) & 0x00ffffff;
	fprintf(fp, "   Summary-LSA mask %d.%d.%d.%d, metric %u\n", 
		RCP_PRINT_IP(ntohl(rdata->mask)), 
		metric);
}

static void print_lsa_summary_asbr(FILE *fp, OspfLsa *lsa) {
	SumLsaData *rdata = &lsa->u.sum;
	uint32_t metric = ntohl(rdata->metric) & 0x00ffffff;
	fprintf(fp, "   Summary-LSA (ASBR) mask %d.%d.%d.%d, metric %u\n", 
		RCP_PRINT_IP(ntohl(rdata->mask)), 
		metric);
}

static void print_lsa(FILE *fp, OspfLsa *lsa) {
	

	fprintf(fp, "LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d\n",
		RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
		lsa->header.type,
		RCP_PRINT_IP(ntohl(lsa->header.adv_router)));

	uint8_t options = lsa->header.options;
	char otype[50] = "";
	if (options & 0x80)
		strcpy(otype, "DN");
	if (options & 0x20) {
		if (otype[0] != '\0')
			strcat(otype, ", ");
		strcat(otype, "DC");
	}
	if (options & 0x10) {
		if (otype[0] != '\0')
			strcat(otype, ", ");
		strcat(otype, "L");
	}
	if (options & 0x08) {
		if (otype[0] != '\0')
			strcat(otype, ", ");
		strcat(otype, "NP");
	}
	if (options & 0x04) {
		if (otype[0] != '\0')
			strcat(otype, ", ");
		strcat(otype, "MC");
	}
	if (options & 0x02) {
		if (otype[0] != '\0')
			strcat(otype, ", ");
		strcat(otype, "E");
	}
	if (options & 0x01) {
		if (otype[0] != '\0')
			strcat(otype, ", ");
		strcat(otype, "MT");
	}
			
	if (otype[0] == '\0')
		fprintf(fp, "   Seq 0x%x, age %u, options 0x%02x\n",
			ntohl(lsa->header.seq),
			ntohs(lsa->header.age),
			options);
	else
		fprintf(fp, "   Seq 0x%x, age %u, options 0x%02x (%s)\n",
			ntohl(lsa->header.seq),
			ntohs(lsa->header.age),
			options, otype);
	
	switch (lsa->header.type) {
		case 1:
			print_lsa_router(fp, lsa);
			break;
		case 2:
			print_lsa_network(fp, lsa);
			break;
		case 3:
			print_lsa_summary_network(fp, lsa);
			break;
		case 4:
			print_lsa_summary_asbr(fp, lsa);
			break;
		case 5:
			print_lsa_external(fp, lsa);
			break;
		default:
			fprintf(fp, "   LSA type %d not implemented\n", lsa->header.type);
	
	}

	fprintf(fp, "\n");
}	


static void lsadbPrint(FILE *fp, uint32_t area_id) {
	TRACE_FUNCTION();

	if (cli_html)
		fprintf(fp, "\n\t\t<b>LSA database (Area %d)</b>\n\n", area_id);
	else
		fprintf(fp, "\n\t\tLSA database (Area %d)\n\n", area_id);

	int i;
	for (i = 0; i < LSA_TYPE_EXTERNAL; i++) {
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa != NULL) {
			print_lsa(fp, lsa);
			lsa = lsa->h.next;
		}
	}
}

static void lsadbPrintSelfOriginated(FILE *fp, uint32_t area_id) {
	TRACE_FUNCTION();


	if (cli_html)
		fprintf(fp, "\n\t\t<b>LSA database (Area %d)</b>\n\n", area_id);
	else
		fprintf(fp, "\n\t\tLSA database (Area %d)\n\n", area_id);

	int i;
	for (i = 0; i < LSA_TYPE_EXTERNAL; i++) {
		OspfLsa *lsa = lsadbGetList(area_id, i);
		while (lsa != NULL) {
			if (lsa->h.self_originated)
				print_lsa(fp, lsa);
			lsa = lsa->h.next;
		}
	}
}

static void lsadbPrintType(FILE *fp, uint32_t area_id, uint8_t type) {
	TRACE_FUNCTION();
	char *stype = "";

	switch(type) {
		case LSA_TYPE_ROUTER:
			stype = "Router";
			break;
		case LSA_TYPE_NETWORK:
			stype = "Network";
			break;
		case LSA_TYPE_SUM_NET:
			stype = "Summary";
			break;
		case LSA_TYPE_SUM_ROUTER:
			stype = "ASBR-Summary";
			break;
		default:;
	}

	if (cli_html)
		fprintf(fp, "\n\t\t<b>%s LSA database (Area %d)</b>\n\n", stype, area_id);
	else
		fprintf(fp, "\n\t\t%s LSA database (Area %d)\n\n", stype, area_id);

	OspfLsa *lsa = lsadbGetList(area_id, type);
	while (lsa != NULL) {
		print_lsa(fp, lsa);
		lsa = lsa->h.next;
	}
}

static void lsadbPrintExternal(FILE *fp) {
	TRACE_FUNCTION();

	int printed = 0;

	// print external routes
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		if (!printed) {
			if (cli_html)
				fprintf(fp, "\n\t\t<b>ASBR LSA database</b>\n\n");
			else
				fprintf(fp, "\n\t\tASBR LSA database\n\n");
			printed = 1;
		}
		print_lsa(fp, lsa);
		lsa = lsa->h.next;
	}
}

static void lsadbPrintExternalSelfOriginated(FILE *fp) {
	TRACE_FUNCTION();

	int printed = 0;

	// print external routes
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	while (lsa != NULL) {
		if (!printed) {
			if (cli_html)
				fprintf(fp, "\n\t\t<b>ASBR LSA database</b>\n\n");
			else
				fprintf(fp, "\n\t\tASBR LSA database\n\n");
			printed = 1;
		}
		if (lsa->h.self_originated)
			print_lsa(fp, lsa);
		lsa = lsa->h.next;
	}
}

void showOspfDatabaseDetail(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		lsadbPrint(fp, area->area_id);
		area = area->next;
	}

	lsadbPrintExternal(fp);
}

void showOspfAreaDatabaseDetail(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			lsadbPrint(fp, area->area_id);
			break;
		}
		area = area->next;
	}

}

void showOspfDatabaseSelfOriginateDetail(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		lsadbPrintSelfOriginated(fp, area->area_id);
		area = area->next;
	}

	lsadbPrintExternalSelfOriginated(fp);
}

void showOspfAreaDatabaseSelfOriginateDetail(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			lsadbPrintSelfOriginated(fp, area->area_id);
			break;
		}
		area = area->next;
	}

}

void showOspfDatabaseRouterDetail(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		lsadbPrintType(fp, area->area_id, LSA_TYPE_ROUTER);
		area = area->next;
	}
}

void showOspfAreaDatabaseRouterDetail(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			lsadbPrintType(fp, area->area_id, LSA_TYPE_ROUTER);
			break;
		}
		area = area->next;
	}

}

void showOspfDatabaseNetworkDetail(FILE *fp) {
	ASSERT(fp);

	OspfArea *area = areaGetList();
	while (area) {
		lsadbPrintType(fp, area->area_id, LSA_TYPE_NETWORK);
		area = area->next;
	}
}

void showOspfAreaDatabaseNetworkDetail(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			lsadbPrintType(fp, area->area_id, LSA_TYPE_NETWORK);
			break;
		}
		area = area->next;
	}
}

void showOspfDatabaseSummaryDetail(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		lsadbPrintType(fp, area->area_id, LSA_TYPE_SUM_NET);
		area = area->next;
	}
}

void showOspfAreaDatabaseSummaryDetail(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			lsadbPrintType(fp, area->area_id, LSA_TYPE_SUM_NET);
			break;
		}
		area = area->next;
	}

}

void showOspfDatabaseAsbrSummaryDetail(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		lsadbPrintType(fp, area->area_id, LSA_TYPE_SUM_ROUTER);
		area = area->next;
	}
}

void showOspfAreaDatabaseAsbrSummaryDetail(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			lsadbPrintType(fp, area->area_id, LSA_TYPE_SUM_ROUTER);
			break;
		}
		area = area->next;
	}

}

static void print_area_router(FILE *fp, OspfArea *area) {
	ASSERT(area != NULL);
	
	// print router lsa
	OspfLsa *lsa = lsadbGetList(area->area_id, LSA_TYPE_ROUTER);
	if (lsa) {
		if (cli_html)
			fprintf(fp, "<b>");
		fprintf(fp, "\n\t\t\tRouter Link States (Area %d)\n\n", area->area_id);
		fprintf(fp, "%-17s%-17s%-8s%-12s%-12s%s\n",
			"Link ID", "ADV Router", "Age", "Seq#", "Checksum", "Link count");
		if (cli_html)
			fprintf(fp, "</b>");

		while (lsa) {
			char lid[20];
			sprintf(lid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	
			char advr[20];
			sprintf(advr, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
			
			char age[20];
			sprintf(age, "%d", ntohs(lsa->header.age));
	
			char seq[20];
			sprintf(seq, "0x%08x", ntohl(lsa->header.seq));
	
			char cks[20];
			sprintf(cks, "0x%04x", ntohs(lsa->header.checksum));
	
			fprintf(fp, "%-17s%-17s%-8s%-12s%-12s%d\n",
				lid, advr, age, seq, cks, lsa->h.ncost_cnt);
			lsa = lsa->h.next;
		}
	}
}

static void print_area_network(FILE *fp, OspfArea *area) {
	ASSERT(area != NULL);
	
	// print network lsa
	OspfLsa *lsa = lsadbGetList(area->area_id, LSA_TYPE_NETWORK);
	if (lsa) {
		if (cli_html)
			fprintf(fp, "<b>");
		fprintf(fp, "\n\t\t\tNetwork Link States (Area %d)\n\n", area->area_id);
		fprintf(fp, "%-17s%-17s%-8s%-12s%-12s\n",
			"Link ID", "ADV Router", "Age", "Seq#", "Checksum");
		if (cli_html)
			fprintf(fp, "</b>");

		while (lsa) {
			char lid[20];
			sprintf(lid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	
			char advr[20];
			sprintf(advr, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
			
			char age[20];
			sprintf(age, "%d", ntohs(lsa->header.age));
	
			char seq[20];
			sprintf(seq, "0x%08x", ntohl(lsa->header.seq));
	
			char cks[20];
			sprintf(cks, "0x%04x", ntohs(lsa->header.checksum));
	
			fprintf(fp, "%-17s%-17s%-8s%-12s%-12s\n",
				lid, advr, age, seq, cks);
			lsa = lsa->h.next;
		}
	}
}

static void print_area_summary_network(FILE *fp, OspfArea *area) {
	ASSERT(area != NULL);
	
	// print summary network lsa
	OspfLsa *lsa = lsadbGetList(area->area_id, LSA_TYPE_SUM_NET);
	if (lsa) {
		if (cli_html)
			fprintf(fp, "<b>");
		fprintf(fp, "\n\t\t\tSummary Link States (Area %d)\n\n", area->area_id);
		fprintf(fp, "%-17s%-17s%-8s%-12s%-12s\n",
			"Link ID", "ADV Router", "Age", "Seq#", "Checksum");
		if (cli_html)
			fprintf(fp, "</b>");

		while (lsa) {
			char lid[20];
			sprintf(lid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	
			char advr[20];
			sprintf(advr, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
			
			char age[20];
			sprintf(age, "%d", ntohs(lsa->header.age));
	
			char seq[20];
			sprintf(seq, "0x%08x", ntohl(lsa->header.seq));
	
			char cks[20];
			sprintf(cks, "0x%04x", ntohs(lsa->header.checksum));
	
			fprintf(fp, "%-17s%-17s%-8s%-12s%-12s\n",
				lid, advr, age, seq, cks);
			lsa = lsa->h.next;
		}
	}
}

static void print_area_summary_router(FILE *fp, OspfArea *area) {
	ASSERT(area != NULL);
	
	// print summary router lsa
	OspfLsa *lsa = lsadbGetList(area->area_id, LSA_TYPE_SUM_ROUTER);
	if (lsa) {
		if (cli_html)
			fprintf(fp, "<b>");
		fprintf(fp, "\n\t\t\tASBR-Summary Link States (Area %d)\n\n", area->area_id);
		fprintf(fp, "%-17s%-17s%-8s%-12s%-12s\n",
			"Link ID", "ADV Router", "Age", "Seq#", "Checksum");
		if (cli_html)
			fprintf(fp, "</b>");

		while (lsa) {
			char lid[20];
			sprintf(lid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	
			char advr[20];
			sprintf(advr, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
			
			char age[20];
			sprintf(age, "%d", ntohs(lsa->header.age));
	
			char seq[20];
			sprintf(seq, "0x%08x", ntohl(lsa->header.seq));
	
			char cks[20];
			sprintf(cks, "0x%04x", ntohs(lsa->header.checksum));
	
			fprintf(fp, "%-17s%-17s%-8s%-12s%-12s\n",
				lid, advr, age, seq, cks);
			lsa = lsa->h.next;
		}
	}
}

static void print_area(FILE *fp, OspfArea *area) {
	ASSERT(area != NULL);
	
	// print router lsa
	print_area_router(fp, area);

	// print network lsa
	print_area_network(fp, area);
	
	// print summary network lsa
	print_area_summary_network(fp, area);

	// print summary router lsa
	print_area_summary_router(fp, area);
}
	
void showOspfDatabaseNetwork(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		print_area_network(fp, area);
		area = area->next;
	}
}

void showOspfAreaDatabaseNetwork(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			print_area_network(fp, area);
			break;
		}
		area = area->next;
	}
}

void showOspfDatabaseRouter(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		print_area_router(fp, area);
		area = area->next;
	}
}

void showOspfAreaDatabaseRouter(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			print_area_router(fp, area);
			break;
		}
		area = area->next;
	}
}

void showOspfDatabaseSummary(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		print_area_summary_network(fp, area);
		area = area->next;
	}
}

void showOspfAreaDatabaseSummary(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			print_area_summary_network(fp, area);
			break;
		}
		area = area->next;
	}
}

void showOspfDatabaseAsbrSummary(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		print_area_summary_router(fp, area);
		area = area->next;
	}
}

void showOspfAreaDatabaseAsbrSummary(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			print_area_summary_router(fp, area);
			break;
		}
		area = area->next;
	}
}
		
void showOspfDatabaseExternal(FILE *fp) {
	// print external lsa
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	if (lsa) {
		if (cli_html)
			fprintf(fp, "<b>");
		fprintf(fp, "\n\t\t\tAS External Link States\n\n");
		fprintf(fp, "%-17s%-17s%-8s%-12s%-12s%-5s%s\n",
			"Link ID", "ADV Router", "Age", "Seq#", "Checksum", "Type", "Tag");
		if (cli_html)
			fprintf(fp, "</b>");

		while (lsa) {
			char lid[20];
			sprintf(lid, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.link_state_id)));
	
			char advr[20];
			sprintf(advr, "%d.%d.%d.%d", RCP_PRINT_IP(ntohl(lsa->header.adv_router)));
			
			char age[20];
			sprintf(age, "%d", ntohs(lsa->header.age));
	
			char seq[20];
			sprintf(seq, "0x%08x", ntohl(lsa->header.seq));
	
			char cks[20];
			sprintf(cks, "0x%04x", ntohs(lsa->header.checksum));
			
			uint8_t type = 2;
			if (lsa->h.ncost)
				type = lsa->h.ncost->ext_type;
			else
				ASSERT(0);
			char type_s[20];
			sprintf(type_s, "E%d", type);
	
			uint32_t tag = 0;
			if (lsa->h.ncost)
				tag = lsa->h.ncost->tag;
			else
				ASSERT(0);
				
			fprintf(fp, "%-17s%-17s%-8s%-12s%-12s%-5s%d\n",
				lid, advr, age, seq, cks, type_s, tag);
			lsa = lsa->h.next;
		}
	}
}

void showOspfDatabaseExternalDetail(FILE *fp) {
	lsadbPrintExternal(fp);
}

void showOspfDatabase(FILE *fp) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {

		print_area(fp, area);
		area = area->next;
	}


	// print external lsa
	showOspfDatabaseExternal(fp);
}


void showOspfAreaDatabase(FILE *fp, uint32_t area_id) {
	ASSERT(fp);
	
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id == area_id) {
			print_area(fp, area);
			break;
		}
		
		area = area->next;
	}
}




void showNeighbors(FILE *fp) {
	ASSERT(fp);

	OspfArea *area = areaGetList();
	while (area != NULL) {
		OspfNetwork *net = area->network;
		while (net != NULL) {
			// find interface name
			char *intf = "";
			RcpInterface *rcpif = rcpFindInterface(shm, net->ip);
			if (rcpif && *rcpif->name != '\0')
				intf = rcpif->name;
			
			
			fprintf(fp, "\n");
			if (cli_html)
				fprintf(fp, "<b>");
			fprintf(fp, "\t\tArea %u, network %d.%d.%d.%d/%d, network state %s\n\n",
				area->area_id, RCP_PRINT_IP(net->ip), mask2bits(net->mask),
				netfsmState2String(net->state));
			fprintf(fp, "%-17s%-9s%-16s%-10s%-17s%-10.10s\n",
				"Router ID", "Priority", "State", "Dead Time", "Address", "Interface"); 
			if (cli_html)
				fprintf(fp, "</b>");
			OspfNeighbor *nb = net->neighbor;
			while (nb != NULL) {
				char rid[20];
				sprintf(rid, "%d.%d.%d.%d", RCP_PRINT_IP(nb->router_id));
				
				char pri[20];
				sprintf(pri, "%d", nb->priority);
				
				char state[20];
				sprintf(state, "%s", nfsmState2String(nb->state));
				if (nb->ip == net->designated_router)
					strcat(state, "/DR");
				else if (nb->ip == net->backup_router)
					strcat(state, "/BDR");
				else
					strcat(state, "/DROther");
				
				char deadt[20];
				sprintf(deadt, "%d", nb->inactivity_timer);
				
				char ip[20];
				sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
				
				fprintf(fp, "%-17s%-9s%-16s%-10s%-17s%-10.10s\n",
					rid, pri, state, deadt, ip, intf);

				// print dd summary list
				OspfLsaHeaderList *lst = nb->ddsum;
				if (lst != NULL) {
					fprintf(fp, "   DD summary list:\n");
					while (lst != NULL) {
						if (lst->ack == 0) {
							OspfLsaHeader *lsah = &lst->header;
							fprintf(fp, "      LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x\n",
								RCP_PRINT_IP(ntohl(lsah->link_state_id)),
								lsah->type,
								RCP_PRINT_IP(ntohl(lsah->adv_router)),
								ntohl(lsah->seq));
						}
						lst = lst->next;
					}
				}

				// print request list
				lst = nb->lsreq;
				if (lst != NULL) {
					fprintf(fp, "   Request list:\n");
					while (lst != NULL) {
						if (lst->ack == 0) {
							OspfLsaHeader *lsah = &lst->header;
							fprintf(fp, "      LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x\n",
								RCP_PRINT_IP(ntohl(lsah->link_state_id)),
								lsah->type,
								RCP_PRINT_IP(ntohl(lsah->adv_router)),
								ntohl(lsah->seq));
						}
						lst = lst->next;
					}
				}
				
				// database description packet retransmission
				if (nb->last_dd_pkt && (nb->state == NSTATE_EXCHANGE || nb->state == NSTATE_EXSTART))
					fprintf(fp, "   One database description packet set for retransmission in %d seconds\n",
						nb->dd_rxmt_timer);

				// print ls update list
				if (nb->rxmt_list != NULL) {
					fprintf(fp, "   Update retransmit list:\n");
					OspfLsa *lsa = nb->rxmt_list;
					while (lsa != NULL) {
						OspfLsaHeader *lsah = &lsa->header;
						fprintf(fp, "      LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x\n",
							RCP_PRINT_IP(ntohl(lsah->link_state_id)),
							lsah->type,
							RCP_PRINT_IP(ntohl(lsah->adv_router)),
							ntohl(lsah->seq));
						
						lsa = lsa->h.next;
					}
				}
				nb = nb->next;
			}
			
			net = net->next;
		}
		
		area = area->next;
	}
	fprintf(fp, "\n");
}

static int callback_show(PTN *ptn, void *arg) {
	ASSERT(ptn);
	ASSERT(arg);
	FILE *fp = arg;
	
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;

		RT *rt = &ptn->rt[i];
		
		// find interface name
		char *intf = NULL;
		if (rt->ifip) {
			RcpInterface *rcpif = rcpFindInterface(shm, rt->ifip);
			if (rcpif && *rcpif->name != '\0')
				intf = rcpif->name;
		}
	
		char *type;
		switch (rt->type) {
			case RT_CONNECTED:
				type = "C    ";
				break;
			case RT_STATIC:
				type = "S    ";
				break;
			case RT_OSPF:
				type = "O    ";
				break;
			case RT_OSPF_IA:
				type = "O IA ";
				break;
			case RT_OSPF_E1:
				type = "O E1 ";
				break;
			case RT_OSPF_E2:
				type = "O E2 ";
				break;
			case RT_OSPF_BLACKHOLE:
				type = "O B  ";
				break;
			default:
				ASSERT(0);
		}

		if (intf && rt->type == RT_CONNECTED) {
			fprintf(fp, "%s%d.%d.%d.%d/%d [%d] is directly connected, %s, area %d\n",
				type,
				RCP_PRINT_IP(rt->ip & rt->mask),
				mask2bits(rt->mask),
				rt->cost,
				intf,
				rt->area_id);
			
			return 0;	
		}
		else if (rt->type == RT_OSPF_BLACKHOLE) {
			fprintf(fp, "%s%d.%d.%d.%d/%d is a summary blackhole\n",
				type,
				RCP_PRINT_IP(rt->ip & rt->mask),
				mask2bits(rt->mask));
			
			return 0;	
		}
		
		else if (intf)
			fprintf(fp, "%s%d.%d.%d.%d/%d [%d] via %d.%d.%d.%d, %s, area %d\n",
				type,
				RCP_PRINT_IP(rt->ip & rt->mask),
				mask2bits(rt->mask),
				rt->cost,
				RCP_PRINT_IP(rt->gw),
				intf,
				rt->area_id);
		else
			fprintf(fp, "%s%d.%d.%d.%d/%d [%d] via %d.%d.%d.%d, area %d\n",
				type,
				RCP_PRINT_IP(rt->ip & rt->mask),
				mask2bits(rt->mask),
				rt->cost,
				RCP_PRINT_IP(rt->gw),
				rt->area_id);
	}
	return 0;
}

static int callback_show_ecmp(PTN *ptn, void *arg) {
	ASSERT(ptn);
	ASSERT(arg);
	FILE *fp = arg;
	
	int cnt = 0;
	int i;
	for (i = 0; i < RCP_OSPF_ECMP_LIMIT; i++) {
		if (ptn->rt[i].type == RT_NONE)
			break;
		cnt++;
	}
	
	if (cnt > 1)
		fprintf(fp, "%d.%d.%d.%d/%d %d ECMP routes\n",
			RCP_PRINT_IP(ptn->rt[0].ip),
			mask2bits(ptn->rt[0].mask),
			cnt);
	return 0;
}

void showRoutes(FILE *fp) {
	ASSERT(fp);

	route_walk(callback_show, fp);
}

void showRoutesEcmp(FILE *fp) {
	ASSERT(fp);

	route_walk(callback_show_ecmp, fp);
}

//**************************************************
// show ip ospf external summary
//**************************************************
typedef struct advrouter_t {
	struct advrouter_t *next;
	uint32_t ip;
	uint32_t count_e1;
	uint32_t count_e2;
} AdvRouter;
static AdvRouter *alist = NULL;

static AdvRouter *arouter_find(uint32_t ip) {
	AdvRouter *ptr = alist;
	while (ptr) {
		if (ptr->ip == ip)
			return ptr;
		ptr = ptr->next;
	}
	
	return NULL;
}

void showOspfDatabaseExternalBrief(FILE *fp) {
	ASSERT(fp);
	
	// count external lsas from each advertising router
	OspfLsa *lsa = lsadbGetList(0, LSA_TYPE_EXTERNAL);
	if (lsa) {
		while (lsa) {
			AdvRouter *art = arouter_find(ntohl(lsa->header.adv_router));
			if (art == NULL) {
				art = malloc(sizeof(AdvRouter));
				if (art == NULL) {
					ASSERT(0);
					return;
				}
				memset(art, 0, sizeof(AdvRouter));
				art->ip = ntohl(lsa->header.adv_router);
				art->next = alist;
				alist = art;
			}
			
			// count each type separately
			uint8_t type = 2;
			if (lsa->h.ncost)
				type = lsa->h.ncost->ext_type;
			else
				ASSERT(0);
			
			if (type == 1)
				art->count_e1++;
			else if (type == 2)
				art->count_e2++;
			else
				ASSERT(0);
				
			lsa = lsa->h.next;
		}
	}

	AdvRouter *ptr = alist;
	if (cli_html)
		fprintf(fp, "<b>");
	fprintf(fp, "%-25.25s%-15.15s%-15.15s\n",
		"Router ID", "E1 LSA Count", "E2 LSA Count"); 
	if (cli_html)
		fprintf(fp, "</b>");
	while (ptr != NULL) {
		char r[30];
		sprintf(r, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->ip));
		char e1[30];
		sprintf(e1, "%d", ptr->count_e1);
		char e2[30];
		sprintf(e2, "%d", ptr->count_e2);
		fprintf(fp, "%-25.25s%-15.15s%-15.15s\n", r, e1, e2);
		
		ptr = ptr->next;
	}
	
	// free memory
	ptr = alist;
	while (ptr) {
		AdvRouter *next = ptr->next;
		free(ptr);
		ptr = next;
	}
	alist = NULL;
}

// ip in network format
static int router_in_any_area(uint32_t ip) {
	OspfArea *area = areaGetList();
	while (area) {
		OspfLsa *found = lsadbFind(area->area_id, LSA_TYPE_ROUTER, ip, ip);
		if (found)
			return 1;
	
		area = area->next;
	}
	
	return 0;
}

// ip in network format
static OspfLsa *router_smaller_cost(uint32_t ip, uint32_t cost, uint32_t area_id) {
	OspfArea *area = areaGetList();
	while (area) {
		if (area->area_id != area_id) {
			OspfLsa *found = lsadbFind(area->area_id, LSA_TYPE_ROUTER, ip, ip);
			if (found && found->h.cost < cost)
				return found;
		}
	
		area = area->next;
	}
	
	return NULL;
}


void showBorder(FILE *fp) {
	// go trough all router lsa

	OspfArea *area = areaGetList();
	// generate lsa in all areas but not in area_id
	while (area) {
		OspfLsa *lsa = lsadbGetList(area->area_id,  LSA_TYPE_ROUTER);
		while (lsa) {
			if (area->router_id != ntohl(lsa->header.link_state_id) && lsa->h.cost != OSPF_MAX_COST) {
				if (router_smaller_cost(lsa->header.link_state_id, lsa->h.cost, area->area_id) == NULL) {
					char *type = "";
					if ((lsa->u.rtr.flags & 3) == 1)
						type = "ABR";
					if ((lsa->u.rtr.flags & 3) == 2)
						type = "ASBR";
					if ((lsa->u.rtr.flags & 3) == 3)
						type = "ABR, ASBR";
					
					fprintf(fp, "i %d.%d.%d.%d [%u], %s, area %u\n",
						RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
						lsa->h.cost,
						type,
						area->area_id);
				}
			}
			lsa = lsa->h.next;
		}			

		lsa = lsadbGetList(area->area_id,  LSA_TYPE_SUM_ROUTER);
		while (lsa) {
			if (area->router_id != ntohl(lsa->header.link_state_id) && lsa->h.cost != OSPF_MAX_COST) {
				if (router_in_any_area(lsa->header.link_state_id) == 0) {
					fprintf(fp, "I %d.%d.%d.%d [%u], ASBR, area %u\n",
						RCP_PRINT_IP(ntohl(lsa->header.link_state_id)),
						lsa->h.cost,
						area->area_id);
				}
			}
			lsa = lsa->h.next;
		}			

		area = area->next;
	}
}
