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

//    Database Description packets are OSPF packet type 2.  These	packets
//    are	exchanged when an adjacency is being initialized.  They	describe
//    the	contents of the	link-state database.  Multiple packets may be
//    used to describe the database.  For	this purpose a poll-response
//    procedure is used.	One of the routers is designated to be the
//    master, the	other the slave.  The master sends Database Description
//    packets (polls) which are acknowledged by Database Description
//    packets sent by the	slave (responses).  The	responses are linked to
//    the	polls via the packets' DD sequence numbers.
//
//	0		    1			2		    3
//	0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |   Version #   |       2       |	 Packet	length	       |
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
//       |	 Interface MTU	       |    Options    |0|0|0|0|0|I|M|MS
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		     DD	sequence number			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |							       |
//       +-							      -+
//       |							       |
//       +-		       An LSA Header			      -+
//       |							       |
//       +-							      -+
//       |							       |
//       +-							      -+
//       |							       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			      ...			       |
//
//
//    The	format of the Database Description packet is very similar to
//    both the Link State	Request	and Link State Acknowledgment packets.
//    The	main part of all three is a list of items, each	item describing
//    a piece of the link-state database.	 The sending of	Database
//    Description	Packets	is documented in Section 10.8.	The reception of
//    Database Description packets is documented in Section 10.6.
//
//    Interface MTU
//	The size in bytes of the largest IP datagram that can be sent
//	out the	associated interface, without fragmentation.  The MTUs
//	of common Internet link	types can be found in Table 7-1	of
//	[Ref22]. Interface MTU should be set to	0 in Database
//	Description packets sent over virtual links.
//
//    Options
//	The optional capabilities supported by the router, as documented
//	in Section A.2.
//
//    I-bit
//	The Init bit.  When set	to 1, this packet is the first in the
//	sequence of Database Description Packets.
//
//    M-bit
//	The More bit.  When set	to 1, it indicates that	more Database
//	Description Packets are	to follow.
//
//    MS-bit
//	The Master/Slave bit.  When set	to 1, it indicates that	the
//	router is the master during the	Database Exchange process.
//	Otherwise, the router is the slave.
//
//    DD sequence	number
//	Used to	sequence the collection	of Database Description	Packets.
//	The initial value (indicated by	the Init bit being set)	should
//	be unique.  The	DD sequence number then	increments until the
//	complete database description has been sent.
//
//    The	rest of	the packet consists of a (possibly partial) list of the
//    link-state database's pieces.  Each	LSA in the database is described
//    by its LSA header.	The LSA	header is documented in	Section	A.4.1.
//    It contains	all the	information required to	uniquely identify both
//    the	LSA and	the LSA's current instance.
//

int dbdescRx(OspfPacket *pkt, OspfPacketDesc *desc) {
	TRACE_FUNCTION();
	ASSERT(pkt != NULL);
	ASSERT(desc != NULL);

	// check packet size
	int len = htons(pkt->header.length);
	if (len < (sizeof(OspfHeader) + sizeof(OspfDbd))) {
		trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"invalid packet length - too small\n");
		return 1;
	}
	// set empty flag
	int empty = (len == (sizeof(OspfHeader) + sizeof(OspfDbd)))? 1: 0;
	// check lsa length
	if (!empty) {
		int lsa_len = len - sizeof(OspfHeader) - sizeof(OspfDbd);
		if (lsa_len % sizeof(OspfLsaHeader)) {
			trap_IfRxBadPacket(desc->if_index, desc->ip_source, pkt->header.type);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"invalid packet length %d, lsa length %d\n", len, lsa_len);
			return 1;
		}
	}
	
//	This section explains the detailed processing of a received
//	Database Description Packet.  The incoming Database Description
//	Packet has already been	associated with	a neighbor and receiving
//	interface by the generic input packet processing (Section 8.2).
//	Whether	the Database Description packet	should be accepted, and
//	if so, how it should be	further	processed depends upon the
//	neighbor state.

	// associate the packet with a network and a neighbor
	OspfNetwork *net = networkFindIp(desc->area_id, desc->ip_source);
	if (net == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	OspfNeighbor *nb = neighborFind(desc->area_id, desc->ip_source);
	if (nb == NULL) {
		return 1;
	}
	OspfArea *area = areaFind(desc->area_id);
	if (area == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	
	
	
//	If a Database Description packet is accepted, the following
//	packet fields should be	saved in the corresponding neighbor data
//	structure under	"last received Database	Description packet":
//	the packet's initialize(I), more (M) and master(MS) bits,
//	Options	field, and DD sequence number. If these	fields are set
//	identically in two consecutive Database	Description packets
//	received from the neighbor, the	second Database	Description
//	packet is considered to	be a "duplicate" in the	processing
//	described below.
	nb->lastdbd_options = pkt->u.dbd.options;
	nb->lastdbd_fields = pkt->u.dbd.fields;
	nb->lastdbd_seq = ntohl(pkt->u.dbd.seq);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
		"DD packet, MTU %u, options %u, fields %u, seq 0x%x",
		ntohs(pkt->u.dbd.mtu), nb->lastdbd_options, 
		nb->lastdbd_fields, nb->lastdbd_seq);
		
//	If the Interface MTU field in the Database Description packet
//	indicates an IP	datagram size that is larger than the router can
//	accept on the receiving	interface without fragmentation, the
//	Database Description packet is rejected.  
	RcpInterface *intf = rcpFindInterface(shm, net->ip);
	if (intf == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	
	if (intf->ospf_mtu_ignore == 0) {
		uint16_t mtu = ntohs(pkt->u.dbd.mtu);
		if (mtu > intf->mtu) {
			trap_IfConfigError(desc->if_index, desc->ip_source, 11 /*mtuMismatch*/, pkt->header.type);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"MTU missmatch, packet ingored");
			
			return 0;
		}
	}

//	Otherwise, if the neighbor state is:
//
//	Down
//	    The	packet should be rejected.
//
//	Attempt
//	    The	packet should be rejected.
	if (nb->state == NSTATE_DOWN || nb->state == NSTATE_ATTEMPT) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			" packet ignored in %s state\n", nfsmState2String(nb->state));
		return 0;
	}

//	Init
//	    The	neighbor state machine should be executed with the event
//	    2-WayReceived.  This causes	an immediate state change to
//	    either state 2-Way or state	ExStart. If the	new state is
//	    ExStart, the processing of the current packet should then
//	    continue in	this new state by falling through to case
//	    ExStart below.
	if (nb->state ==  NSTATE_INIT)
		nfsm2WayReceived(net, nb);

//	2-Way
//	    The	packet should be ignored.  Database Description	Packets
//	    are	used only for the purpose of bringing up adjacencies.[7]
	if (nb->state == NSTATE_2WAY) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"packet ignored in 2WAY state");
		rcpDebug("%d ignored\n", __LINE__);
		return 0;
	}

	if (nb->state == NSTATE_EXSTART) {
//	ExStart
//	    If the received packet matches one of the following	cases,
//	    then the neighbor state machine should be executed with the
//	    event NegotiationDone (causing the state to	transition to
//	    Exchange), the packet's Options field should be recorded in
//	    the	neighbor structure's Neighbor Options field and	the
//	    packet should be accepted as next in sequence and processed
//	    further (see below).  Otherwise, the packet	should be
//	    ignored.
//
//	    o	The initialize(I), more	(M) and	master(MS) bits	are set,
//		the contents of	the packet are empty, and the neighbor's
//		Router ID is larger than the router's own.  In this case
//		the router is now Slave.  Set the master/slave bit to
//		slave, and set the neighbor data structure's DD	sequence
//		number to that specified by the	master.
		if ((nb->lastdbd_fields & 7) == 7 && empty &&
		      nb->router_id > net->router_id) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "we are slave");
			// our router is slave
			nb->master = 0;
			nb->options = nb->lastdbd_options;
			nb->dd_seq_number = nb->lastdbd_seq;
			nfsmNegotiationDone(net, nb);
		}
//
//	    o	The initialize(I) and master(MS) bits are off, the
//		packet's DD sequence number equals the neighbor	data
//		structure's DD sequence	number (indicating
//		acknowledgment)	and the	neighbor's Router ID is	smaller
//		than the router's own.	In this	case the router	is
//		Master.
		else if ((nb->lastdbd_fields & 5) == 0 &&
		            nb->lastdbd_seq == nb->dd_seq_number &&
		            nb->router_id < net->router_id) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "we are master");
			// our router is master
			nb->master = 1;
			nb->options = nb->lastdbd_options;
			nfsmNegotiationDone(net, nb);
		}
		else {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "packet ingored in EXSTART state");
			rcpDebug("   packet ingored in EXSTART state\n");
			rcpDebug("        received fields/seq/options %x/%x/%x, neighbor structure seq %x\n",
				nb->lastdbd_fields,	 nb->lastdbd_seq, nb->lastdbd_options, nb->dd_seq_number);
			rcpDebug("        neghbor router id %d.%d.%d.%d, ours %d.%d.%d.%d\n",
				RCP_PRINT_IP(nb->router_id), RCP_PRINT_IP(net->router_id));
			return 0;
		}
	}

	else if (nb->state == NSTATE_EXCHANGE) {
//	Exchange
//	    Duplicate Database Description packets are discarded by the
//	    master, and	cause the slave	to retransmit the last Database
//	    Description	packet that it had sent.  


		// duplicate packet
		if (nb->master && nb->dd_seq_number > nb->lastdbd_seq) {
			rcpDebug("duplicate\n");			
			return 0;
		}
		else if (nb->master == 0 && (nb->dd_seq_number + 1) > nb->lastdbd_seq) {
			rcpDebug("duplicate\n");			
			return 0;
		}



// Otherwise (the	packet
//	    is not a duplicate):
//
//	    o	If the state of	the MS-bit is inconsistent with	the
//		master/slave state of the connection, generate the
//		neighbor event SeqNumberMismatch and stop processing the
//		packet.
		if (nb->lastdbd_fields & 1 && nb->master) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "master/slave mismatch (received master, we are master)");
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}
		else if ((nb->lastdbd_fields & 1) == 0 && nb->master == 0)  {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "master/slave mismatch (received slave, we are slave)");
			rcpDebug("%d\n", __LINE__);
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}
//
//	    o	If the initialize(I) bit is set, generate the neighbor
//		event SeqNumberMismatch	and stop processing the	packet.
		if (nb->lastdbd_fields & 4) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "not accepting I bit set");
			rcpDebug("%d\n", __LINE__);
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}


//	    o	If the packets	Options	field indicates	a different set
//		of optional OSPF capabilities than were	previously
//		received from the neighbor (recorded in	the Neighbor
//		Options	field of the neighbor structure), generate the
//		neighbor event SeqNumberMismatch and stop processing the
//		packet.
		if (nb->lastdbd_options != nb->options) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "options mismatch");
			rcpDebug("%d\n", __LINE__);
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}

//	    o	Database Description packets must be processed in
//		sequence, as indicated by the packets DD sequence
//		numbers. If the	router is master, the next packet
//		received should	have DD	sequence number	equal to the DD
//		sequence number	in the neighbor	data structure.	If the
//		router is slave, the next packet received should have DD
//		sequence number	equal to one more than the DD sequence
//		number stored in the neighbor data structure. In either
//		case, if the packet is the next	in sequence it should be
//		accepted and its contents processed as specified below.
		int inseq = 0;
		if (nb->master && nb->dd_seq_number == nb->lastdbd_seq)
			inseq = 1;
		if (nb->master == 0 && (nb->dd_seq_number + 1) == nb->lastdbd_seq)
			inseq = 1;
		
		if (inseq == 0) {
//	    o	Else, generate the neighbor event SeqNumberMismatch and
//		stop processing	the packet.
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "sequence number mismatch");
			rcpDebug("%d\n", __LINE__);
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}


		// handle the outgoing LSA headers from nb->ddsum
		// - headers sent have sent flag set
		// - set the ack flag if sent flag is set
		OspfLsaHeaderList *lsahl = nb->ddsum;
		while (lsahl != NULL) {
			if (lsahl->sent && !lsahl->ack) {
				lsahl->ack = 1;
				OspfLsaHeader *lsah = &lsahl->header;
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
					"LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x acknowledged in dd packet",
					RCP_PRINT_IP(ntohl(lsah->link_state_id)),
					lsah->type,
					RCP_PRINT_IP(ntohl(lsah->adv_router)),
					ntohl(lsah->seq));
			}
			lsahl = lsahl->next;
		}
		// attempt to remove ddsum list
		lsahListPurge(&nb->ddsum);
//		if (lsahListIsAck(&nb->ddsum))
//			lsahListRemoveAll(&nb->ddsum);
	}

//	Loading	or Full
//	    In this state, the router has sent and received an entire
//	    sequence of	Database Description Packets.  The only	packets
//	    received should be duplicates (see above).	In particular,
//	    the	packets Options field should match the	set of optional
//	    OSPF capabilities previously indicated by the neighbor
//	    (stored in the neighbor structures	Neighbor Options field).
//	    Any	other packets received,	including the reception	of a
//	    packet with	the Initialize(I) bit set, should generate the
//	    neighbor event SeqNumberMismatch.[8] Duplicates should be
//	    discarded by the master.  The slave	must respond to
//	    duplicates by repeating the	last Database Description packet
//	    that it had	sent.
	else if (nb->state == NSTATE_LOADING || nb->state == NSTATE_FULL) {
		// I bit set go to SeqNumberMismatch
		if (nb->lastdbd_fields & 4) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "not accepting I bit set in loading or full state");
			rcpDebug("%d\n", __LINE__);
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}
		
		// options mismatch
		if (nb->lastdbd_options != nb->options) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "options mismatch in loading or full state");
			rcpDebug("%d\n", __LINE__);
			nfsmSeqNumberMismatch(net, nb);
			return 0;
		}

		// duplicate packet
		if (nb->master && nb->dd_seq_number > nb->lastdbd_seq)
			return 0;
		else if (nb->master == 0 && (nb->dd_seq_number + 1) > nb->lastdbd_seq)
			return 0;

		// anything else
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "sequence mismatch in loading or full state");
		rcpDebug("%d\n", __LINE__);
		nfsmSeqNumberMismatch(net, nb);
		return 0;
	}


//	When the router	accepts	a received Database Description	Packet
//	as the next in sequence	the packet contents are	processed as
//	follows.  For each LSA listed, the LSAs LS type is checked for
//	validity.  If the LS type is unknown (e.g., not	one of the LS
//	types 1-5 defined by this specification), or if	this is	an AS-
//	external-LSA (LS type =	5) and the neighbor is associated with a
//	stub(done) area, generate the	neighbor event SeqNumberMismatch and
//	stop processing	the packet.  Otherwise,	the router looks up the
//	LSA in its database to see whether it also has an instance of
//	the LSA.  If it	does not, or if	the database copy is less recent
//	(see Section 13.1), the	LSA is put on the Link state request
//	list so	that it	can be requested (immediately or at some later
//	time) in Link State Request Packets.


	// check lsa type validity
	if (!empty) {
		OspfLsaHeader *lsah = (OspfLsaHeader *) ((uint8_t *) &pkt->header + sizeof(OspfHeader) + sizeof(OspfDbd));
		int cnt = (len - sizeof(OspfHeader) - sizeof(OspfDbd)) / sizeof(OspfLsaHeader);
		int i;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
			"%d LSAs present in the packet", cnt);
		for (i = 0; i < cnt; i++, lsah++) {
			if (lsah->type > LSA_TYPE_EXTERNAL) {
				rcpDebug("%d\n", __LINE__);
				nfsmSeqNumberMismatch(net, nb);
				return 0;
			}
			else if (area->stub && lsah->type > LSA_TYPE_EXTERNAL) {
				rcpDebug("%d\n", __LINE__);
				nfsmSeqNumberMismatch(net, nb);
				return 0;
			}

			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x",
				RCP_PRINT_IP(ntohl(lsah->link_state_id)),
				lsah->type,
				RCP_PRINT_IP(ntohl(lsah->adv_router)),
				ntohl(lsah->seq));
			// look for lsa in the database
			OspfLsa *lsa = lsadbFindHeader(net->area_id, lsah);
			int age1 = (lsa)? ntohs(lsa->header.age): 0;
			int age2 = ntohs(lsah->age);
			int delta = age1 - age2;
			if (delta < 0)
				delta *= -1;
			
			// if not in the database, add it
			if ((lsa = lsadbFindHeader(net->area_id, lsah)) == NULL) {
				lsahListAdd(&nb->lsreq, lsah);
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA ,
					"added to request list");
			}
			// if different, add it
			else if (lsa->header.seq != lsah->seq ||
			            lsa->header.checksum != lsah->checksum ||
			            delta > MaxAgeDiff) {

				lsahListAdd(&nb->lsreq, lsah);
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
					"found different LSA in database, added to request list");
			}
			else {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
					"not added to request list");
			}
		}
				
	}


//	When the router	accepts	a received Database Description	Packet
//	as the next in sequence, it also performs the following	actions,
//	depending on whether it	is master or slave:
//
//	Master
//	    Increments the DD sequence number in the neighbor data
//	    structure.	If the router has already sent its entire
//	    sequence of	Database Description Packets, and the just
//	    accepted packet has	the more bit (M) set to	0, the neighbor
//	    event ExchangeDone is generated.  Otherwise, it should send
//	    a new Database Description to the slave.
	if (nb->master == 1) {
		if (nb->more0sent && (nb->lastdbd_fields & 2) == 0) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"DD exchange done");
			nb->more0sent = 0;
			nfsmExchangeDone(net, nb);
		}
		else {
			nb->dd_seq_number++;
			// send the response
			OspfPacket resp;
			uint8_t fields = ((nb->ddsum == NULL)? 0: 2) | 1; //nb->lastdbd_fields & 2 | 1; // preserve the M bit
			dbdescBuildPkt(net, nb, &resp, fields);
			txpacket_unicast(&resp, net, nb->ip);
			if (nb->ddsum == NULL)
				nb->more0sent = 1;
		}
	}
		
//	Slave
//	    Sets the DD	sequence number	in the neighbor	data structure
//	    to the DD sequence number appearing	in the received	packet.
//	    The	slave must send	a Database Description Packet in reply.
//	    If the received packet has the more	bit (M)	set to 0, and
//	    the	packet to be sent by the slave will also have the M-bit
//	    set	to 0, the neighbor event ExchangeDone is generated.
//	    Note that the slave	always generates this event before the
//	    master.
	else {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "responde as slave to seq %u", nb->lastdbd_seq);
		nb->dd_seq_number = nb->lastdbd_seq;
		
		// send the response
		OspfPacket resp;
//		uint8_t fields = nb->lastdbd_fields & 2; // preserve the M bit
		uint8_t fields;
		if (nb->ddsum != NULL)
			fields = 2;
		else
			fields = nb->lastdbd_fields & 2; // preserve the M bit
		dbdescBuildPkt(net, nb, &resp, fields);
		txpacket_unicast(&resp, net, nb->ip);
		
		if ((nb->lastdbd_fields & 2) == 0 && nb->ddsum == NULL) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, "DD exchange done");
			nfsmExchangeDone(net, nb);
		}
	}

	return 0;
}


void dbdescBuildPkt(OspfNetwork *net, OspfNeighbor *nb, OspfPacket *pkt, uint8_t fields) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(pkt != NULL);
	ASSERT(nb != NULL);
	memset(pkt, 0, OSPF_PKT_HDR_LEN);

//	This section describes how Database Description	Packets	are sent
//	to a neighbor. The Database Description	packet's Interface MTU
//	field is set to	the size of the	largest	IP datagram that can be
//	sent out the sending interface,	without	fragmentation.	Common
//	MTUs in	use in the Internet can	be found in Table 7-1 of
//	[Ref22]. Interface MTU should be set to	 0 in Database
//	Description packets sent over virtual links.

	RcpInterface *intf = rcpFindInterface(shm, net->ip);
	if (intf == NULL) {
		ASSERT(0);
		pkt->u.dbd.mtu = htons(1500);
	}
	else
		pkt->u.dbd.mtu = htons((uint16_t) intf->mtu);
	
//	The router's optional OSPF capabilities	(see Section 4.5) are
//	transmitted to the neighbor in the Options field of the	Database
//	Description packet.  The router	should maintain	the same set of
//	optional capabilities throughout the Database Exchange and
//	flooding procedures.  If for some reason the router's optional
//	capabilities change, the Database Exchange procedure should be
//	restarted by reverting to neighbor state ExStart.  One optional
//	capability is defined in this specification (see Sections 4.5
//	and A.2). The E-bit should be set if and only if the attached
//	network	belongs	to a non-stub(done) area. Unrecognized bits in the
//	Options	field should be	set to zero.

	OspfArea *area = areaFind(net->area_id);
	ASSERT(area);
	if (area && area->stub)
		pkt->u.dbd.options = nb->options & 0x02;
	else
		pkt->u.dbd.options = nb->options | 0x02;
	
	
//	The sending of Database	Description packets depends on the
//	neighbor's state.  In state ExStart the	router sends empty
//	Database Description packets, with the initialize (I), more (M)
//	and master (MS)	bits set.  These packets are retransmitted every
//	RxmtInterval seconds.
	if (nb->state == NSTATE_EXSTART)
		pkt->u.dbd.fields = 7;
	else
		pkt->u.dbd.fields = fields;

//	In state Exchange the Database Description Packets actually
//	contain	summaries of the link state information	contained in the
//	router's database.  Each LSA in	the area's link-state database
//	(at the	time the neighbor transitions into Exchange state) is
//	listed in the neighbor Database	summary	list.  Each new	Database
//	Description Packet copies its DD sequence number from the
//	neighbor data structure	and then describes the current top of
//	the Database summary list.  Items are removed from the Database
//	summary	list when the previous packet is acknowledged.
//
//	In state Exchange, the determination of	when to	send a Database
//	Description packet depends on whether the router is master or
//	slave:
//
//
//	Master
//	    Database Description packets are sent when either a) the
//	    slave acknowledges the previous Database Description packet
//	    by echoing the DD sequence number or b) RxmtInterval seconds
//	    elapse without an acknowledgment, in which case the	previous
//	    Database Description packet	is retransmitted.
//
//	Slave
//	    Database Description packets are sent only in response to
//	    Database Description packets received from the master.  If
//	    the	Database Description packet received from the master is
//	    new, a new Database	Description packet is sent, otherwise
//	    the	previous Database Description packet is	resent.
//
	int cnt = 0;
	if (nb->state == NSTATE_EXCHANGE && nb->ddsum != NULL) {
		OspfLsaHeader *lsah = (OspfLsaHeader *) ((uint8_t *) &pkt->u.dbd + sizeof(OspfDbd));
		OspfLsaHeaderList *lsahl = nb->ddsum;
		while (lsahl != NULL) {
			if (!lsahl->ack) {
				ASSERT(lsahl->sent == 0);
				memcpy(lsah, &lsahl->header, sizeof(OspfLsaHeader));
				cnt++;
				lsahl->sent = 1;

				if (cnt >= OSPF_DD_MAX)
					break;

				lsah++;
			}
			lsahl = lsahl->next;
		}
	}		

	pkt->u.dbd.seq = htonl(nb->dd_seq_number);
	int size = sizeof(OspfHeader) + sizeof(OspfDbd) + cnt * sizeof(OspfLsaHeader);

	// add header
	headerBuild(pkt, net, 2, size);
	
	// store a copy of the packet
	OspfPacket *last = malloc(sizeof(OspfPacket));
	if (last == NULL) {
		printf("cannot allocate memory\n");
		exit(1);
	}
	memcpy(last, pkt, sizeof(OspfPacket));
	if (nb->last_dd_pkt != NULL) {
		free(nb->last_dd_pkt);
	}
	nb->last_dd_pkt = last;

	// start rxmt timer
	nb->dd_rxmt_timer = net->rxtm_interval;	
}
