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

//A.3.6 The Link State Acknowledgment packet
//
//    Link State Acknowledgment Packets are OSPF packet type 5.  To make
//    the	flooding of LSAs reliable, flooded LSAs	are explicitly
//    acknowledged.  This	acknowledgment is accomplished through the
//    sending and	receiving of Link State	Acknowledgment packets.
//    Multiple LSAs can be acknowledged in a single Link State
//    Acknowledgment packet.
//
//    Depending on the state of the sending interface and	the sender of
//    the	corresponding Link State Update	packet,	a Link State
//    Acknowledgment packet is sent either to the	multicast address
//    AllSPFRouters, to the multicast address AllDRouters, or as a
//    unicast.  The sending of Link State	Acknowledgment	packets	is
//    documented in Section 13.5.	 The reception of Link State
//    Acknowledgment packets is documented in Section 13.7.
//
//    The	format of this packet is similar to that of the	Data Description
//    packet.  The body of both packets is simply	a list of LSA headers.
//
//
//	0		    1			2		    3
//	0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |   Version #   |       5       |	 Packet	length	       |
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
//       |							       |
//       +-							      -+
//       |							       |
//       +-			  An LSA Header			      -+
//       |							       |
//       +-							      -+
//       |							       |
//       +-							      -+
//       |							       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			      ...			       |
//
//
//    Each acknowledged LSA is described by its LSA header.  The LSA
//    header is documented in Section A.4.1.  It contains	all the
//    information	required to uniquely identify both the LSA and the LSA's
//    current instance.
//

int lsackRx(OspfPacket *pkt, OspfPacketDesc *desc) {
	TRACE_FUNCTION();
	ASSERT(pkt != NULL);
	int i;

	int size = ntohs(pkt->header.length);
	size -= sizeof(OspfHeader);
	
	int cnt = size / sizeof(OspfLsaHeader);
	if (cnt == 0) {
		trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
		return 1;
	}

	// verify length
	if (size % sizeof(OspfLsaHeader)) {
		trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "invalid length");
		return 1;
	}

	OspfArea *area = areaFind(desc->area_id);
	if (area == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	
	// check the mask in hello packet
	OspfNetwork *net = networkFindIp(desc->area_id, desc->ip_source);	// using the source of the packet
	if (net == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}

	// find the neighbor
	OspfNeighbor *nb = neighborFind(desc->area_id, desc->ip_source);
	if (nb == NULL) {
		return 1;
	}

	
	uint8_t *ptr = ((uint8_t *) &pkt->u);
	for (i = 0; i < cnt; i++) {
		OspfLsaHeader *lsah = (OspfLsaHeader *) ptr;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, 
			"LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x",
			RCP_PRINT_IP(ntohl(lsah->link_state_id)),
			lsah->type,
			RCP_PRINT_IP(ntohl(lsah->adv_router)),
			ntohl(lsah->seq));
		if (nb->state == NSTATE_FULL) {
			// remove lsa from rxmt list
//			OspfLsa *found = lsaListFindRxmt(&nb->rxmt_list, lsah);
			OspfLsa *found = lsaListFind(&nb->rxmt_list, lsah->type, lsah->link_state_id, lsah->adv_router);
			if (found) {
				lsaListRemove(&nb->rxmt_list, found);
				lsaFree(found);
			}
		}
		ptr += sizeof(OspfLsaHeader);
	}
	return 0;
}

void lsackBuildPkt(OspfNetwork *net, OspfNeighbor *nb, OspfPacket *pkt, OspfLsaHeader *lsa, int cnt) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	ASSERT(pkt != NULL);
	ASSERT(lsa != NULL);
	ASSERT(cnt != 0);

	memset(pkt, 0, OSPF_PKT_HDR_LEN);
	memcpy(&pkt->u, lsa, sizeof(OspfLsaHeader) * cnt);

	// add header
	headerBuild(pkt, net, 5, sizeof(OspfHeader) + sizeof(OspfLsaHeader) * cnt );
}
