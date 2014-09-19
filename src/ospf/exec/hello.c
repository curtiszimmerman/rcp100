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

//A.3.2 The Hello	packet
//
//    Hello packets are OSPF packet type 1.  These packets are sent
//    periodically on all	interfaces (including virtual links) in	order to
//    establish and maintain neighbor relationships.  In addition, Hello
//    Packets are	multicast on those physical networks having a multicast
//    or broadcast capability, enabling dynamic discovery	of neighboring
//    routers.
//
//    All	routers	connected to a common network must agree on certain
//    parameters (Network	mask, HelloInterval and	RouterDeadInterval).
//    These parameters are included in Hello packets, so that differences
//    can	inhibit	the forming of neighbor	relationships.	A detailed
//    explanation	of the receive processing for Hello packets is presented
//    in Section 10.5.  The sending of Hello packets is covered in Section
//    9.5.
//
//
//	0		    1			2		    3
//	0 1 2 3	4 5 6 7	8 9 0 1	2 3 4 5	6 7 8 9	0 1 2 3	4 5 6 7	8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |   Version #   |       1       |	 Packet	length	       |
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
//       |			Network	Mask			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |	 HelloInterval	       |    Options    |    Rtr	Pri    |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		     RouterDeadInterval			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		      Designated Router			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |		   Backup Designated Router		       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			  Neighbor			       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |			      ...			       |
//
//
//    Network mask
//	The network mask associated with this interface.  For example,
//	if the interface is to a class B network whose third byte is
//	used for subnetting, the network mask is 0xffffff00.
//
//    Options
//	The optional capabilities supported by the router, as documented
//	in Section A.2.
//
//    HelloInterval
//	The number of seconds between this router's Hello packets.
//
//    Rtr	Pri
//	This router's Router Priority.	Used in	(Backup) Designated
//	Router election.  If set to 0, the router will be ineligible to
//	become (Backup)	Designated Router.
//
//    RouterDeadInterval
//	The number of seconds before declaring a silent	router down.
//
//    Designated Router
//	The identity of	the Designated Router for this network,	in the
//	view of	the sending router.  The Designated Router is identified
//	here by	its IP interface address on the	network.  Set to 0.0.0.0
//	if there is no Designated Router.
//
//    Backup Designated Router
//	The identity of	the Backup Designated Router for this network,
//	in the view of the sending router.  The	Backup Designated Router
//	is identified here by its IP interface address on the network.
//	Set to 0.0.0.0 if there	is no Backup Designated	Router.
//
//    Neighbor
//	The Router IDs of each router from whom	valid Hello packets have
//	been seen recently on the network.  Recently means in the last
//	RouterDeadInterval seconds.

// process a hello packet; return 1 if error, 0 if ok
int helloRx(OspfPacket *pkt, OspfPacketDesc *desc) {
	TRACE_FUNCTION();
	ASSERT(pkt != NULL);

	OspfArea *area = areaFind(desc->area_id);
	if (area == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	
//	However,
//	there is one exception to the above rule: on point-to-point
//	networks and on	virtual	links, the Network Mask	in the received
//	Hello Packet should be ignored.

	// check the mask in hello packet
	OspfNetwork *net = networkFindIp(desc->area_id, desc->ip_source);	// using the source of the packet
	if (net == NULL) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 2 /*areaMismatch*/, pkt->header.type);
		return 1;
	}
	if (net->type == N_TYPE_BROADCAST) {
		if (net->mask != ntohl(pkt->u.hello.mask)) {
			trap_IfConfigError(desc->if_index, desc->ip_source, 7 /*netMaskMismatch*/, pkt->header.type);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"mask not matching");
			return 1;
		}
	}

//              Next, the values of the Network Mask, HelloInterval,
//	and RouterDeadInterval fields in the received Hello packet must
//	be checked against the values configured for the receiving
//	interface.  Any	mismatch causes	processing to stop and the
//	packet to be dropped.  In other	words, the above fields	are
//	really describing the attached network's configuration.
	// check hello and dead intervals against area configuration
	if (ntohs(pkt->u.hello.hello_interval) != net->hello_interval) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 8 /*helloIntervalMismatch*/, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"invalid hello interval");
		return 1;
	}
	if (ntohl(pkt->u.hello.dead_interval) != net->dead_interval) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 9 /*deadIntervalMismatch*/, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"invalid dead interval");
		return 1;
	}


//	The receiving interface	attaches to a single OSPF area (this
//	could be the backbone).	 The setting of	the E-bit found	in the
//	Hello Packet's Options field must match	this area's
//	ExternalRoutingCapability.  If AS-external-LSAs	are not	flooded
//	into/throughout	the area (i.e, the area	is a "stub"(done)) the E-bit
//	must be	clear in received Hello	Packets, otherwise the E-bit
//	must be	set.  A	mismatch causes	processing to stop and the
//	packet to be dropped.  The setting of the rest of the bits in
//	the Hello Packet's Options field should	be ignored. - todo
	// E bit check
	int ebit = (pkt->u.hello.options & 0x02)? 1: 0;
	if ((ebit && area->stub) || (!ebit && !area->stub)) {
		trap_IfConfigError(desc->if_index, desc->ip_source, 10 /*optionMismatch*/, pkt->header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"E-bit mismatch");
		return 1;
	}

//	At this	point, an attempt is made to match the source of the
//	Hello Packet to	one of the receiving interface's neighbors.  If
//	the receiving interface	connects to a broadcast, Point-to-
//	MultiPoint or NBMA network the source is identified by the IP
//	source address found in	the Hello's IP header.	If the receiving
//	interface connects to a	point-to-point link or a virtual link,
//	the source is identified by the	Router ID found	in the Hello's
//	OSPF packet header.  The interface's current list of neighbors
//	is contained in	the interface's	data structure.	 If a matching
//	neighbor structure cannot be found, (i.e., this	is the first
//	time the neighbor has been detected), one is created.  The
//	initial	state of a newly created neighbor is set to Down. - todo - partial implementation
	
	// find the neighbor
	OspfNeighbor *nb = neighborFind(desc->area_id, desc->ip_source);
	if (nb == NULL) {
		// create a new neighbor
		nb = malloc(sizeof(OspfNeighbor));
		if (nb == NULL) {
			printf("   cannot allocate memory\n");
			exit(1);
		}
		memset(nb, 0, sizeof(OspfNeighbor));
		
//	When receiving an Hello	Packet from a neighbor on a broadcast,
//	Point-to-MultiPoint or NBMA network, set the neighbor
//	structure's Neighbor ID	equal to the Router ID found in	the
//	packet's OSPF header.  For these network types,	the neighbor
//	structure's Router Priority field, Neighbor's Designated Router
//	field, and Neighbor's Backup Designated	Router field are also
//	set equal to the corresponding fields found in the received
//	Hello Packet; changes in these fields should be	noted for
//	possible use in	the steps below.  When receiving an Hello on a
//	point-to-point network (but not	on a virtual link) set the
//	neighbor structure's Neighbor IP address to the	packet's IP
//	source address. - todo - partial implementation
		nb->ip = desc->ip_source;
		nb->router_id = desc->router_id;
		if (neighborAdd(desc->area_id, nb)) {
			return 1;
		}
		else {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"new neighbor added");
		}
	}

	// set new priority value
	int priority_changed = 0;
	if (nb->priority != pkt->u.hello.router_priority) {
		priority_changed = 1;
		nb->priority = pkt->u.hello.router_priority;
	}

	// set the new designated router value
	int prev_drouter = (nb->designated_router == desc->ip_source)? 1: 0;
	int drouter_changed = 0;
	if (nb->designated_router != ntohl(pkt->u.hello.designated_router)) {
		drouter_changed = 1;
		nb->designated_router = ntohl(pkt->u.hello.designated_router);
		rcpDebug("   drouter_changed, prev_drouter %d\n", prev_drouter);

		if (net->state == NETSTATE_DR) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_DRBDR,
				"neighbor %d.%d.%d.%d thinks it has a different DR on network %d.%d.%d.%d, network state %d",
				RCP_PRINT_IP(nb->ip), RCP_PRINT_IP(nb->ip), net->state);
		}
		
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_DRBDR,
			"neighbor %d.%d.%d.%d designated router changed to %d.%d.%d.%d",
			RCP_PRINT_IP(nb->ip), RCP_PRINT_IP(nb->designated_router));
	}
	// set the new backup router value
	int prev_brouter = (nb->backup_router == desc->ip_source)? 1: 0;
	int brouter_changed = 0;
	if (nb->backup_router != ntohl(pkt->u.hello.backup_router)) {
		brouter_changed = 1;
		nb->backup_router = ntohl(pkt->u.hello.backup_router);
		rcpDebug("   drouter_changed, prev_brouter %d\n", prev_brouter);	
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_DRBDR,
			"neighbor %d.%d.%d.%d backup router changed to %d.%d.%d.%d",
			RCP_PRINT_IP(nb->ip), RCP_PRINT_IP(nb->backup_router));
	}
	
//	Now the	rest of	the Hello Packet is examined, generating events
//	to be given to the neighbor and	interface state	machines.  These
//	state machines are specified either to be executed or scheduled
//	(see Section 4.4).  For	example, by specifying below that the
//	neighbor state machine be executed in line, several neighbor
//	state transitions may be effected by a single received Hello:
//
//	o   Each Hello Packet causes the neighbor state	machine	to be
//	    executed with the event HelloReceived.
	nfsmHelloReceived(net, nb);


//	o   Then the list of neighbors contained in the	Hello Packet is
//	    examined.  If the router itself appears in this list, the
//	    neighbor state machine should be executed with the event 2-
//	    WayReceived.  Otherwise, the neighbor state	machine	should
//	    be executed	with the event 1-WayReceived, and the processing
//	    of the packet stops.
	// parse neighbor list


	int nl = (ntohs(pkt->header.length) - 44) / 4; 
	int i;
	int found = 0;
	uint32_t *nptr = (uint32_t *) ((uint8_t *) (&pkt->u.hello) + sizeof(OspfHello));
	for (i = 0; i < nl; i++, nptr++) {
		if (ntohl(*nptr) == net->router_id) {
			found = 1;
			break;
		}
	}
	if (found)
		nfsm2WayReceived(net, nb);	
	else {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_DRBDR,
			"neighbor %d.%d.%d.%d 1WAY, prev_drouter %d, prev_brouter %d",
			RCP_PRINT_IP(nb->ip), prev_drouter, prev_brouter);
		nfsm1Way(net, nb);
		if (!prev_drouter && !prev_brouter) // force a new election
			return 0;
	}

	// scheduled events
	void (*netfsm)(OspfNetwork *net) = NULL;
	
//	o   Next, if a change in the neighbor's	Router Priority	field
//	    was	noted, the receiving interface's state machine is
//	    scheduled with the event NeighborChange.
	if (priority_changed)
		netfsm = netfsmNeighborChange;
		
//	o   If the neighbor is both declaring itself to	be Designated
//	    Router (Hello Packet's Designated Router field = Neighbor IP
//	    address) and the Backup Designated Router field in the
//	    packet is equal to 0.0.0.0 and the receiving interface is in
//	    state Waiting, the receiving interface's state machine is
//	    scheduled with the event BackupSeen. 
	if (pkt->u.hello.backup_router == 0 &&
	     net->state == NETSTATE_WAITING &&
	     ntohl(pkt->u.hello.designated_router) == desc->ip_source) {
		netfsm = netfsmBackupSeen;
	}
//	                                                                     Otherwise, if	the
//	    neighbor is	declaring itself to be Designated Router and it
//	    had	not previously,
	else if (ntohl(pkt->u.hello.designated_router) == desc->ip_source && drouter_changed) {
		netfsm = netfsmNeighborChange;
	}
//	                                             or the neighbor	is not declaring itself
//	    Designated Router where it had previously, the receiving
//	    interface's	state machine is scheduled with	the event
//	    NeighborChange.
	else if (ntohl(pkt->u.hello.designated_router) != desc->ip_source && prev_drouter) {
		netfsm = netfsmNeighborChange;
	}

//	o   If the neighbor is declaring itself	to be Backup Designated
//	    Router (Hello Packet's Backup Designated Router field =
//	    Neighbor IP	address) and the receiving interface is	in state
//	    Waiting, the receiving interface's state machine is
//	    scheduled with the event BackupSeen.
	if ( net->state == NETSTATE_WAITING &&
	     ntohl(pkt->u.hello.backup_router) == desc->ip_source) {
		netfsm = netfsmBackupSeen;
	}
//	                                                                     Otherwise, if	the
//	    neighbor is	declaring itself to be Backup Designated Router
//	    and	it had not previously,
	else if (ntohl(pkt->u.hello.backup_router) == desc->ip_source && brouter_changed) {
		netfsm = netfsmNeighborChange;
	}
//	                                                   or the neighbor is not declaring
//	    itself Backup Designated Router where it had previously, the
//	    receiving interface's state	machine	is scheduled with the
//	    event NeighborChange.
	else if (ntohl(pkt->u.hello.backup_router) != desc->ip_source && prev_brouter) {
		netfsm = netfsmNeighborChange;
	}

	// run the scheduled event
	if (netfsm != NULL) {
		netfsm(net);
	}

	return 0;	
}



void helloBuildPkt(OspfNetwork *net, OspfPacket *pkt) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(pkt != NULL);
	memset(pkt, 0, OSPF_PKT_HDR_LEN);

	OspfArea *area = areaFind(net->area_id);
	ASSERT(area);

//	Hello packets are sent out each	functioning router interface.
//	They are used to discover and maintain neighbor
//	relationships.[6] On broadcast and NBMA	networks, Hello	Packets
//	are also used to elect the Designated Router and Backup
//	Designated Router.
//
//	The format of an Hello packet is detailed in Section A.3.2.  The
//	Hello Packet contains the router's Router Priority (used in
//	choosing the Designated	Router), and the interval between Hello
//	Packets	sent out the interface (HelloInterval).	 The Hello
//	Packet also indicates how often	a neighbor must	be heard from to
//	remain active (RouterDeadInterval).  Both HelloInterval	and
//	RouterDeadInterval must	be the same for	all routers attached to
//	a common network.  The Hello packet also contains the IP address
//	mask of	the attached network (Network Mask).  On unnumbered
//	point-to-point networks	and on virtual links this field	should
//	be set to 0.0.0.0.
//
//	The Hello packet's Options field describes the router's	optional
//	OSPF capabilities.  One	optional capability is defined in this
//	specification (see Sections 4.5	and A.2).  The E-bit of	the
//	Options	field should be	set if and only	if the attached	area is
//	capable	of processing AS-external-LSAs (i.e., it is not	a stub (done)
//	area).	If the E-bit is	set incorrectly	the neighboring	routers
//	will refuse to accept the Hello	Packet (see Section 10.5).
//	Unrecognized bits in the Hello Packet's	Options	field should be
//	set to zero.
//
//	In order to ensure two-way communication between adjacent
//	routers, the Hello packet contains the list of all routers on
//	the network from which Hello Packets have been seen recently.
//	The Hello packet also contains the router's current choice for
//	Designated Router and Backup Designated	Router.	 A value of
//	0.0.0.0	in these fields	means that one has not yet been
//	selected.
//
//	On broadcast networks and physical point-to-point networks,
//	Hello packets are sent every HelloInterval seconds to the IP
//	multicast address AllSPFRouters.  On virtual links, Hello
//	packets	are sent as unicasts (addressed	directly to the	other
//	end of the virtual link) every HelloInterval seconds. 


	// build hello
	pkt->u.hello.mask = htonl(net->mask);
	pkt->u.hello.hello_interval = htons(net->hello_interval);
	if (area && area->stub)
		pkt->u.hello.options = 0;
	else
		pkt->u.hello.options = 0x02;
	pkt->u.hello.router_priority = net->router_priority;
	pkt->u.hello.dead_interval = htonl(net->dead_interval);
	pkt->u.hello.designated_router = htonl(net->designated_router);
	pkt->u.hello.backup_router = htonl(net->backup_router);
	
	// add known neighbors
	int size = 44;	// size without known neighbors
	OspfNeighbor *nb = net->neighbor;
	uint32_t *nptr = (uint32_t *) ((uint8_t *) (&pkt->u.hello) + sizeof(OspfHello));
	while (nb != NULL) {
		// for each neighbor
		if (nb->state != NSTATE_DOWN) {
			*nptr++ = htonl(nb->router_id);
			size += 4;
		}
		nb = nb->next;
	}

	// add header
	headerBuild(pkt, net, 1, size);
}
