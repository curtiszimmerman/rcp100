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

//A.3.5 The Link State Update packet
//
//    Link State Update packets are OSPF packet type 4.  These packets
//    implement the flooding of LSAs.  Each Link State Update packet
//    carries a collection of LSAs one hop further from their origin.
//    Several LSAs may be	included in a single packet.
//
//    Link State Update packets are multicast on those physical networks
//    that support multicast/broadcast.  In order	to make	the flooding
//    procedure reliable,	flooded	LSAs are acknowledged in Link State
//    Acknowledgment packets.  If	retransmission of certain LSAs is
//    necessary, the retransmitted LSAs are always sent directly to the
//    neighbor.  For more	information on the reliable flooding of	LSAs,
//    consult Section 13.
//
//	0		    1			2		    3
//	0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |   Version #   |       4       |	 Packet	length	       |
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
//       |			    # LSAs			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |							       |
//       +-							     +-+
//       |			     LSAs			       |
//       +-							     +-+
//       |			      ...			       |
//
//    # LSAs
//	The number of LSAs included in this update.
//
//
//    The	body of	the Link State Update packet consists of a list	of LSAs.
//    Each LSA begins with a common 20 byte header, described in Section
//    A.4.1. Detailed formats of the different types of LSAs are described
//    in Section A.4.
//


OspfLsa *duplicate_lsa(OspfLsaHeader *lsah) {
	TRACE_FUNCTION();
	ASSERT(lsah != NULL);
	
	int size = ntohs(lsah->length) + sizeof(struct lsa_h_t);
	OspfLsa *newlsa = malloc(size);
	if (newlsa == NULL) {
		printf("   lsadb: cannot allocate memory\n");
		exit(1);
	}
	memset(newlsa, 0, size);
	memcpy(&newlsa->header, lsah, ntohs(lsah->length));
	newlsa->h.size = size;
	return newlsa;
}

int lsupdateRx(OspfPacket *pkt, OspfPacketDesc *desc) {
	TRACE_FUNCTION();
	ASSERT(pkt != NULL);
	int i;
	
	int cnt = ntohl(pkt->u.update.count);
	if (cnt == 0) {
		trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
		return 1;
	}
	int size = ntohs(pkt->header.length);
	size -= sizeof(OspfHeader) + sizeof(OspfLsUpdate);
	
	// verify length
	uint8_t *ptr = ((uint8_t *) &pkt->u.update) + sizeof(OspfLsUpdate);
	for (i = 0; i < cnt; i++) {
		OspfLsaHeader *lsa = (OspfLsaHeader *) ptr;
		int len = ntohs(lsa->length);
		size -= len;
		if (size < 0) {
			return 1;
		}
		ptr += len;
	}
	if (size != 0) {
		trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
		return 1;
	}
	
	// ack list
	OspfLsaHeader ack[cnt];
	int ackcnt = 0;
	memset(ack, 0, sizeof(ack));
	
//13.  The Flooding Procedure
//
//    Link State Update packets provide the mechanism for	flooding LSAs.
//    A Link State Update	packet may contain several distinct LSAs, and
//    floods each	LSA one	hop further from its point of origination.  To
//    make the flooding procedure	reliable, each LSA must	be acknowledged
//    separately.	 Acknowledgments are transmitted in Link State
//    Acknowledgment packets.  Many separate acknowledgments can also be
//    grouped together into a single packet.
//
//    The	flooding procedure starts when a Link State Update packet has
//    been received.  Many consistency checks have been made on the
//    received packet before being handed	to the flooding	procedure (see
//    Section 8.2).  In particular, the Link State Update	packet has been
//    associated with a particular neighbor, and a particular area.  If
//    the	neighbor is in a lesser	state than Exchange, the packet	should
//    be dropped without further processing.
	// associate the packet with a network and a neighbor
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
			"neighbor state %s, packet refused\n",
			nfsmState2String(nb->state));
		return 0;
	}
	
//
//    All	types of LSAs, other than AS-external-LSAs, are	associated with
//    a specific area.  However, LSAs do not contain an area field.  An
//    LSA's area must be deduced from the	Link State Update packet header.
//    For	each LSA contained in a	Link State Update packet, the following
//    steps are taken:
	ptr = ((uint8_t *) &pkt->u.update) + sizeof(OspfLsUpdate);
	for (i = 0; i < cnt; i++) {
		OspfLsaHeader *lsah = (OspfLsaHeader *) ptr;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
			"LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x, age %u",
			RCP_PRINT_IP(ntohl(lsah->link_state_id)),
			lsah->type,
			RCP_PRINT_IP(ntohl(lsah->adv_router)),
			ntohl(lsah->seq),
			ntohs(lsah->age));

		// if lsa is in the request list, remove it from there
		lsahListRemove(&nb->lsreq, lsah->type, lsah->link_state_id, lsah->adv_router);

//
//
//    (1)	Validate the LSA's LS checksum.	 If the	checksum turns out to be
//	invalid, discard the LSA and get the next one from the Link
//	State Update packet.
		uint8_t *msg = (uint8_t *) lsah + 2;
		int sz = ntohs(lsah->length) - 2;
		uint16_t fl = fletcher16(msg, sz, 0);
		if (fl != 0) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"invalid LSA checksum");
			ptr += ntohs(lsah->length);
			continue;
		}
//
//    (2)	Examine	the LSA's LS type.  If the LS type is unknown, discard
//	the LSA	and get	the next one from the Link State Update	Packet.
//	This specification defines LS types 1-5	(see Section 4.3).
		if (lsah->type == 0 || lsah->type > 5) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"invalid LSA type");
			ptr += ntohs(lsah->length);
			continue;
		}

//    (3)	Else if	this is	an AS-external-LSA (LS type = 5), and the area
//	has been configured as a stub (done) area, discard the	LSA and	get the
//	next one from the Link State Update Packet.  AS-external-LSAs
//	are not	flooded	into/throughout	stub (done) areas (see	Section	3.6).
		if (lsah->type == 5 && area->stub) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"AS-external-LSA discarded, this is a stub area");
			ptr += ntohs(lsah->length);
			continue;
		}

#if 0 // not necessary
//
//    (4)	Else if	the LSA's LS age is equal to MaxAge, and there is
//	currently no instance of the LSA in the	router's link state
//	database, and none of router's neighbors are in	states Exchange
//	or Loading, then take the following actions: a)	Acknowledge the
//	receipt	of the LSA by sending a	Link State Acknowledgment packet
//	back to	the sending neighbor (see Section 13.5), and b)	Discard
//	the LSA	and examine the	next LSA (if any) listed in the	Link
//	State Update packet.
		if (ntohs(lsah->age) == MaxAge) {
			OspfLsa *found = lsadbFindHeader(area->area_id, lsah);
			if (found == NULL) {
				OspfNeighbor *n = net->neighbor;
				int none = 1;
				while (n != NULL) {
					if (n->state == NSTATE_EXCHANGE || n->state == NSTATE_LOADING)
						none = 0;
					n = n->next;
				}
				
				if (none) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
						"LSA reached MaxAge");
					ptr += ntohs(lsah->length);
					continue;
				}
			}
		}
#endif
			
//
//    (5)	Otherwise, find	the instance of	this LSA that is currently
//	contained in the router's link state database.	If there is no
//	database copy, or the received LSA is more recent than the
//	database copy (see Section 13.1	below for the determination of
//	which LSA is more recent) the following	steps must be performed:
//
//	(a) If there is	already	a database copy, and if	the database
//	    copy was received via flooding and installed less than
//	    MinLSArrival seconds ago, discard the new LSA (without
//	    acknowledging it) and examine the next LSA (if any)	listed
//	    in the Link	State Update packet.
//
//	(b) Otherwise immediately flood	the new	LSA out	some subset of
//	    the	router's interfaces (see Section 13.3).	 In some cases
//	    (e.g., the state of	the receiving interface	is DR and the
//	    LSA	was received from a router other than the Backup DR) the
//	    LSA	will be	flooded	back out the receiving interface.  This
//	    occurrence should be noted for later use by	the
//	    acknowledgment process (Section 13.5).
//
//	(c) Remove the current database	copy from all neighbors' Link
//	    state retransmission lists.
//
//	(d) Install the	new LSA	in the link state database (replacing
//	    the	current	database copy).	 This may cause	the routing
//	    table calculation to be scheduled.	In addition, timestamp
//	    the	new LSA	with the current time (i.e., the time it was
//	    received).	The flooding procedure cannot overwrite	the
//	    newly installed LSA	until MinLSArrival seconds have	elapsed.
//	    The	LSA installation process is discussed further in Section
//	    13.2.
//
//	(e) Possibly acknowledge the receipt of	the LSA	by sending a
//	    Link State Acknowledgment packet back out the receiving
//	    interface.	This is	explained below	in Section 13.5.
//
//	(f) moved down
//
//    (6)	Else, if there is an instance of the LSA on the	sending
//	neighbor's Link	state request list, an error has occurred in the
//	Database Exchange process.  In this case, restart the Database
//	Exchange process by generating the neighbor event BadLSReq for
//	the sending neighbor and stop processing the Link State	Update
//	packet.
//
//    (7)	Else, if the received LSA is the same instance as the database
//	copy (i.e., neither one	is more	recent)	the following two steps
//	should be performed:
//
//	(a) If the LSA is listed in the	Link state retransmission list
//	    for	the receiving adjacency, the router itself is expecting
//	    an acknowledgment for this LSA.  The router	should treat the
//	    received LSA as an acknowledgment by removing the LSA from
//	    the	Link state retransmission list.	 This is termed	an
//	    "implied acknowledgment".  Its occurrence should be	noted
//	    for	later use by the acknowledgment	process	(Section 13.5).
//
//	(b) Possibly acknowledge the receipt of	the LSA	by sending a
//	    Link State Acknowledgment packet back out the receiving
//	    interface.	This is	explained below	in Section 13.5.
//
//    (8)	Else, the database copy	is more	recent.	 If the	database copy
//	has LS age equal to MaxAge and LS sequence number equal	to
//	MaxSequenceNumber, simply discard the received LSA without
//	acknowledging it. (In this case, the LSA's LS sequence number is
//	wrapping, and the MaxSequenceNumber LSA	must be	completely
//	flushed	before any new LSA instance can	be introduced).
//	Otherwise, as long as the database copy	has not	been sent in a
//	Link State Update within the last MinLSArrival seconds,	send the
//	database copy back to the sending neighbor, encapsulated within
//	a Link State Update Packet. The	Link State Update Packet should
//	be sent	directly to the	neighbor. In so	doing, do not put the
//	database copy of the LSA on the	neighbor's link	state
//	retransmission list, and do not	acknowledge the	received (less
//	recent)	LSA instance.
//

		int do_not_ack = 0; // no ack will be sent back
		
		// for now, check if it is there, and add it
		OspfLsa *found = lsadbFindHeader(area->area_id, lsah);
		if (found == NULL) {
			// Implicit ack:
			// DR, receiving a new LAS from a DROther, will not ack the new LSA,
			//       instead it will flood it back to AllSPFRouters.
			if (net->state == NETSTATE_DR && // we are DR
			    desc->ip_dest == AllDRouters_GROUP_IP) {
//printf("TESTING Implicit ack:  req received from %d.%d.%d.%d, will not ack\n", RCP_PRINT_IP(desc->router_id));			
			    	do_not_ack = 1;
			 }
			
			// BDR, recieving a new LSA from DROther, does noting, waiting for DR to react to it
			if (net->state == NETSTATE_BACKUP && // we are BDR
			    desc->ip_dest == AllDRouters_GROUP_IP) { 
//printf("TESTING Implicit ack: we are bdr, doing nothing\n");
				do_not_ack = 1;
			}
			else if (ntohs(lsah->age) != MaxAge) {
				// create a new lsa
				OspfLsa *newlsa = duplicate_lsa(lsah);
				ASSERT(newlsa != NULL);
				newlsa->h.net_ip = net->ip;
	
				// added to lsadb
				newlsa->h.flood = 1;
				lsadbAdd(area->area_id, newlsa);
			}
		}
		else if (found && found->h.self_originated) {
//	(f) If this new	LSA indicates that it was originated by	the
//	    receiving router itself (i.e., is considered a self-
//	    originated LSA), the router	must take special action, either
//	    updating the LSA or	in some	cases flushing it from the
//	    routing domain. For	a description of how self-originated
//	    LSAs are detected and subsequently handled,	see Section
//	    13.4.

			// max age
			if (ntohs(lsah->age) == MaxAge) 
				found->header.age = htons(MaxAge);
			// smaller seq
			else if (ntohl(found->header.seq) < ntohl(lsah->seq)) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
					"reflooding LSA in 3 seconds");
				// delete the old lsa, generate a new one, and reflood it in 3 seconds
				found->h.old_cnt = 3;
				found->h.old_self_originated_seq = lsah->seq;
			}
		}
		else if (found && ntohl(found->header.seq) < ntohl(lsah->seq)) {
			OspfLsa *newlsa = duplicate_lsa(lsah);
			newlsa->h.flood = 1;
			newlsa->h.net_ip = net->ip;
			ASSERT(newlsa != NULL);
			lsadbReplace(area->area_id, found, newlsa);
			lsaFree(found);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"free LSA");
		}
		// set maxage if necessary
		else if (found && ntohs(lsah->age) == MaxAge)
			found->header.age = htons(MaxAge);

		// if there is already one lsa in rxmt list, remove it - this handles the implicit ack scenario
		OspfLsa *found_rxmt = lsaListFind(&nb->rxmt_list, lsah->type, lsah->link_state_id, lsah->adv_router);
		if (found_rxmt) {
//printf("TESTING Implicit ack: removing req from rxmt list\n");			
			lsaListRemove(&nb->rxmt_list, found_rxmt);
			lsaFree(found_rxmt);
			
			// if the router is DROther and originates a new LSA, DR will flood it back to AllSPFRouters,
//			// the router should not ack it
			if (net->state == NETSTATE_DROTHER &&
			     desc->router_id == net->designated_router &&
			     desc->ip_dest == AllSPFRouters_GROUP_IP &&
			     found && found->h.self_originated) {
//printf("Implicit ack: req flooded by DR %d.%d.%d.%d, will not ack\n", RCP_PRINT_IP(desc->router_id));			
				do_not_ack = 1;
			}
		}

		
		// add it to ack list
		if (do_not_ack == 0) {
			memcpy(&ack[ackcnt], lsah, sizeof(OspfLsaHeader));
			ackcnt++;
		}
		
		ptr += ntohs(lsah->length);
	}

	// send out the acknowledgment
	if (ackcnt != 0) {
		OspfPacket pktout;
		// build the packet	
		lsackBuildPkt(net, nb, &pktout, &ack[0], ackcnt);
		// sending out  the packet
		// if the packet was sent as unicast
		//	respond as unicast
		// else if the packet was a broadcast
			// if state is DROther
			//	respond to AllDRouters_GROUP
			// else if state is DR or BDR
			//	respond to AllSPFRouters_GROUP

		
		if (desc->ip_dest == net->ip) {
			txpacket_unicast(&pktout, net, nb->ip);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"LS Acknowledge sent to neighbor %d.%d.%d.%d",
				RCP_PRINT_IP(nb->ip));
		}
		else if (net->state == NETSTATE_DROTHER) { // multicast
			txpacket_unicast(&pktout, net, AllDRouters_GROUP_IP);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"LS Acknowledge sent to AllDRouters");
		}				
		else { // multicast
			txpacket(&pktout, net);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"LS Acknowledge sent to AllSPFRouters");
		}
	}

// From section 10.9. Sending Link State Request Packets
//	When the Link state request list becomes empty,	and the	neighbor
//	state is Loading (i.e.,	a complete sequence of Database
//	Description packets has	been sent to and received from the
//	neighbor), the Loading Done neighbor event is generated.
	if (nb->state == NSTATE_LOADING) {
		if (nb->lsreq == NULL)
			nfsmLoadingDone(net, nb);
		else {
			// send a new ls request
			OspfPacket pkt;
			lsrequestBuildPkt(net, nb, &pkt);
			
			// send the packet
			txpacket_unicast(&pkt, net, nb->ip);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"sending LS request to neighbor %d.%d.%d.%d",
				RCP_PRINT_IP(nb->ip));
				
		}
	}
	
	return 0;
}

// return 0 if packet build, 1 if error
int lsupdateBuildPkt(OspfNetwork *net, OspfPacket *pkt, OspfLsa *lsa) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(pkt != NULL);
	ASSERT(lsa != NULL);

	memset(pkt, 0, OSPF_PKT_HDR_LEN);
	pkt->u.update.count = htonl(1);
	int szlsa = ntohs(lsa->header.length);
	// refuse to send the packet if max payload exceeded - it will overwrite memory!
	if (szlsa > OSPF_MAX_PAYLOAD) {
		ASSERT(0);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_ERR, RLOG_FC_OSPF_LSA,
			"maximum packet payload exceeded");
		return  1;
	}
	memcpy((uint8_t *) &pkt->u.update + sizeof(OspfLsUpdate), &lsa->header, szlsa);

	// add header
	headerBuild(pkt, net, 4, sizeof(OspfHeader) + sizeof(OspfLsUpdate) + szlsa);
	return 0;
}


// return next unprocessed lsa
OspfLsa *lsupdateBuildMultiplePkt(OspfNetwork *net, OspfPacket *pkt, OspfLsa *lsa, int trap) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(pkt != NULL);
	ASSERT(lsa != NULL);
	OspfLsa *rv = NULL;
	memset(pkt, 0, OSPF_PKT_HDR_LEN);
	
	int size = 0;
	int cnt = 0;
	OspfLsa *ptr = lsa;
	while (ptr != NULL) {
		int szlsa = ntohs(ptr->header.length);
		ASSERT(szlsa < sizeof(OspfPacket));
		if ((size + szlsa) > OSPF_MAX_PAYLOAD) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"sending out %d LSAs, payload size %d\n", cnt, size);
			rv = ptr;
			break;
		}
		memcpy((uint8_t *) &pkt->u.update + sizeof(OspfLsUpdate) + size, &ptr->header, szlsa);
		if (trap)
			trap_TxRetransmit(ptr);
		size += szlsa;
		cnt++;
		ptr = ptr->h.util_next;
	}
	pkt->u.update.count = htonl(cnt);

	// add header
	headerBuild(pkt, net, 4, sizeof(OspfHeader) + sizeof(OspfLsUpdate) + size);
	return rv;
}
