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

//A.3.4 The Link State Request packet
//
//    Link State Request packets are OSPF	packet type 3.	After exchanging
//    Database Description packets with a	neighboring router, a router may
//    find that parts of its link-state database are out-of-date.	 The
//    Link State Request packet is used to request the pieces of the
//    neighbor's database	that are more up-to-date.  Multiple Link State
//    Request packets may	need to	be used.
//
//    A router that sends	a Link State Request packet has	in mind	the
//    precise instance of	the database pieces it is requesting. Each
//    instance is	defined	by its LS sequence number, LS checksum,	and LS
//    age, although these	fields are not specified in the	Link State
//    Request Packet itself.  The	router may receive even	more recent
//    instances in response.
//
//    The	sending	of Link	State Request packets is documented in Section
//    10.9.  The reception of Link State Request packets is documented in
//    Section 10.7.
//
//	0		    1			2		    3
//	0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |   Version #   |       3       |	 Packet	length	       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			  Router ID			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			   Area	ID			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |	   Checksum	       |	     AuType	       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		       Authentication			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		       Authentication			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			  LS type			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		       Link State ID			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		     Advertising Router			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			      ...			       |
//
//    Each LSA requested is specified by its LS type, Link State ID, and
//    Advertising	Router.	 This uniquely identifies the LSA, but not its
//    instance.  Link State Request packets are understood to be requests
//    for	the most recent	instance (whatever that	might be).
//

int lsrequestRx(OspfPacket *pkt, OspfPacketDesc *desc) {
	int sz = ntohs(pkt->header.length) - sizeof(OspfHeader);
	if (sz % sizeof(OspfLsRequest)) {
		trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, 
			"invalid length");
		return 1;
	}


	// find area, network and neighbor
	OspfArea *area = areaFind(desc->area_id);
	if (area == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	OspfNetwork *net = networkFindIp(desc->area_id, desc->ip_source);
	if (net == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	OspfNeighbor *nb = neighborFind(desc->area_id, desc->ip_source);
	if (nb == NULL) {
		return 1;
	}
	
	if (nb->state < NSTATE_EXCHANGE) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, 
			"invalid negihbor state, packet ignored\n");
		return 0;
	}
	
//    10.7.  Receiving Link State Request Packets
//
//        This section explains the detailed processing of received Link
//        State Request packets.  Received Link State Request Packets
//        specify a list of LSAs that the neighbor wishes to receive.
//        Link State Request Packets should be accepted when the neighbor
//        is in states Exchange, Loading, or Full.  In all other states
//        Link State Request Packets should be ignored.
//
//        Each LSA specified in the Link State Request packet should be
//        located in the router's database, and copied into Link State
//        Update packets for transmission to the neighbor.  These LSAs
//        should NOT be placed on the Link state retransmission list for
//        the neighbor.  If an LSA cannot be found in the database,
//        something has gone wrong with the Database Exchange process, and
//        neighbor event BadLSReq should be generated.

	
	int cnt = sz / sizeof(OspfLsRequest);

	// lsa accumulation
	OspfLsa *outlist = NULL;
	int i;
	OspfLsRequest *lsrq = &pkt->u.req;
	for (i = 0; i < cnt; i++, lsrq++) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, 
			"requesting type %u, id %d.%d.%d.%d, from %d.%d.%d.%d\n",
			ntohl(lsrq->type),
			RCP_PRINT_IP(ntohl(lsrq->id)),
			RCP_PRINT_IP(ntohl(lsrq->adv_router)));
		
		OspfLsa *lsa = lsadbFindRequest(desc->area_id, lsrq);
		if (lsa == NULL) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"requested type %u, id %d.%d.%d.%d, from %d.%d.%d.%d, not found in LSA database",
				ntohl(lsrq->type),
				RCP_PRINT_IP(ntohl(lsrq->id)),
				RCP_PRINT_IP(ntohl(lsrq->adv_router)));
			
			nfsmBadLSReq(net, nb);
			return 0;
		}
		else {
			lsa->h.util_next = outlist;
			outlist = lsa;
		}
	}
	
	if (outlist != NULL) {
		OspfPacket out;
		lsupdateBuildMultiplePkt(net, &out, outlist, 0);
		txpacket_unicast(&out, net, nb->ip);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "LS Update sent to neighbor %d.%d.%d.%d",
			RCP_PRINT_IP(nb->ip));
		outlist = NULL;
	}
	
	return 0;
}

void lsrequestBuildPkt(OspfNetwork *net, OspfNeighbor *nb, OspfPacket *pkt) {
 //   10.9.  Sending Link	State Request Packets
//
//	In neighbor states Exchange or Loading,	the Link state request
//	list contains a	list of	those LSAs that	need to	be obtained from
//	the neighbor.  To request these	LSAs, a	router sends the
//	neighbor the beginning of the Link state request list, packaged
//	in a Link State	Request	packet.
//
//	When the neighbor responds to these requests with the proper
//	Link State Update packet(s), the Link state request list is
//	truncated and a	new Link State Request packet is sent.	This
//	process	continues until	the Link state request list becomes
//	empty. LSAs on the Link	state request list that	have been
//	requested, but not yet received, are packaged into Link	State
//	Request	packets	for retransmission at intervals	of RxmtInterval.
//	There should be	at most	one Link State Request packet
//	outstanding at any one time.

//
//	When the Link state request list becomes empty,	and the	neighbor
//	state is Loading (i.e.,	a complete sequence of Database
//	Description packets has	been sent to and received from the
//	neighbor), the Loading Done neighbor event is generated.
//(implemented in  lsupdateRx()) 
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(pkt != NULL);
	ASSERT(nb != NULL);
	
	if (nb->lsreq == NULL) {
		ASSERT(0);
		return;
	}
	memset(pkt, 0, OSPF_PKT_HDR_LEN);

	// add maximum OSPF_LS_REQ_MAX requests
	int size = sizeof(OspfHeader);
	OspfLsaHeaderList *lst = nb->lsreq;
	OspfLsRequest *ptr = &pkt->u.req;
	int cnt = 0;
	while (lst != NULL) {
		if (lst->ack == 0) {
			OspfLsaHeader *lsa = &lst->header;
			ptr->type = htonl(lsa->type);
			ptr->id = lsa->link_state_id;
			ptr->adv_router = lsa->adv_router;
			ptr++;
			size += sizeof(OspfLsRequest);
			if (++cnt >= OSPF_LS_REQ_MAX)
				break;
		}
		lst = lst->next;
	}

	// add header
	headerBuild(pkt, net, 3, size);
	
	// store a copy of the packet for retransmission
	OspfPacket *last = malloc(sizeof(OspfPacket));
	if (last == NULL) {
		printf("cannot allocate memory\n");
		exit(1);
	}
	memcpy(last, pkt, sizeof(OspfPacket));
	if (nb->last_lsreq_pkt != NULL) {
		free(nb->last_lsreq_pkt);
	}
	nb->last_lsreq_pkt = last;

	// start rxmt timer
	nb->lsreq_rxmt_timer = net->rxtm_interval;	
}
