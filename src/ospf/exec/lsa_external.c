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

// originate an external lsa
OspfLsa *lsa_originate_ext(uint32_t ip, uint32_t mask, uint32_t gw, int type, uint32_t metric, uint32_t tag) {
	ASSERT(type == 1 || type == 2);
		
	OspfLsa *lsa = lsadbFind(0, LSA_TYPE_EXTERNAL, htonl(ip), htonl(shm->config.ospf_router_id));
	int replace_it = 0;
	if (lsa && lsa->h.self_originated) {
		// looks like we already have an lsa, set replace flag
		// TODO: no support for ECMP yet
		replace_it = 1;
	}
	
	// calculate total length
	int size = sizeof(OspfLsa) + sizeof(ExtLsaData);
	
	// allocate lsa
	OspfLsa *rv = malloc(size);
	if (rv == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(rv, 0, size);
	rv->h.size = size;
	rv->h.self_originated = 1;
	
	// fill in the header
	OspfLsaHeader *lsah = &rv->header;
	lsah->age = 0;
	lsah->options = 2; //E bit
	lsah->type = LSA_TYPE_EXTERNAL;
	lsah->link_state_id = htonl(ip);
	lsah->adv_router = htonl(shm->config.ospf_router_id);
	lsah->seq = htonl(seq_number);
	lsa_increment_seq();
	lsah->length = htons(sizeof(OspfLsaHeader) + sizeof(ExtLsaData));
	
	// set route data
	rv->u.ext.mask = htonl(mask);
	metric &= 0x00ffffff;
	if (type == 2)
		metric |= 0x80000000;
	rv->u.ext.type_metric = htonl(metric);
	rv->u.ext.fw_address = htonl(gw);
	rv->u.ext.tag = htonl(tag);
	
	// calculate checksum
	lsah = &rv->header;
	uint8_t *msg = (uint8_t *) lsah + 2;
	int sz = ntohs(lsah->length) - 2;
	fletcher16(msg, sz, 15);

	if (replace_it) {
		// remove existing lsa
		lsaListRemove(lsadbGetListHead(0, lsa->header.type), lsa);
		lsaFree(lsa);
	}
	else
		trap_OriginateLsa(rv, 0);
	rv->h.flood = 1;

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_LSA,
		"originated LSA %d.%d.%d.%d, type %d, seq 0x%x",
		RCP_PRINT_IP(ntohl(lsah->link_state_id)),
		lsah->type,
		ntohl(lsah->seq));
	
	return rv;
}
