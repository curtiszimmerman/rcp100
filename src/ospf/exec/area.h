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
#ifndef OSPF_AREA_H
#define OSPF_AREA_H

#include "lsa.h"
#include "packet.h"

//**************************************************************************
// Neighbor
//**************************************************************************
typedef struct ospf_neighbor_t {
	struct ospf_neighbor_t *next;

//	The IP address of the neighboring router's interface to	the
//	attached network.  Used	as the Destination IP address when
//	protocol packets are sent as unicasts along this adjacency.
//	Also used in router-LSAs as the	Link ID	for the	attached network
//	if the neighboring router is selected to be Designated Router
//	(see Section 12.4.1).  The Neighbor IP address is learned when
//	Hello packets are received from	the neighbor.  For virtual
//	links, the Neighbor IP address is learned during the routing
//	table build process (see Section 15).
	uint32_t ip;	// neighbor ip address from ip header - search using neighborFind()

//	The OSPF Router	ID of the neighboring router.  The Neighbor ID
//	is learned when	Hello packets are received from	the neighbor, or
//	is configured if this is a virtual adjacency (see Section C.4).
	uint32_t router_id;	// router id from OSPF header
	
//	The functional level of	the neighbor conversation.  This is
//	described in more detail in Section 10.1.
//    10.1.  Neighbor states
#define NSTATE_DOWN 0
#define NSTATE_ATTEMPT 1
#define NSTATE_INIT 2
#define NSTATE_2WAY 3
#define NSTATE_EXSTART 4
#define NSTATE_EXCHANGE 5
#define NSTATE_LOADING 6
#define NSTATE_FULL 7
#define NSTATE_MAX 8
	unsigned state;

//	A single shot timer whose firing indicates that	no Hello Packet
//	has been seen from this	neighbor recently.  The	length of the
//	timer is RouterDeadInterval seconds.
	int inactivity_timer;	// 0 - trigger, >0 active, <0 inactive
	
//	When the two neighbors are exchanging databases, they form a
//	master/slave relationship.  The	master sends the first Database
//	Description Packet, and	is the only part that is allowed to
//	retransmit.  The slave can only	respond	to the master's	Database
//	Description Packets.  The master/slave relationship is
//	negotiated in state ExStart.
	int master;	// 1 our router is master, 0 our router is slave
	
//	The DD Sequence	number of the Database Description packet that
//	is currently being sent	to the neighbor.
	uint32_t dd_seq_number;	

//	The initialize(I), more	(M) and	master(MS) bits, Options field,
//	and DD sequence	number contained in the	last Database
//	Description packet received from the neighbor. Used to determine
//	whether	the next Database Description packet received from the
//	neighbor is a duplicate. - todo
	uint32_t lastdbd_seq;
	uint8_t lastdbd_options;
	uint8_t lastdbd_fields;

//	The Router Priority of the neighboring router.	Contained in the
//	neighbor's Hello packets, this item is used when selecting the
//	Designated Router for the attached network.
	uint8_t priority;	

//	The optional OSPF capabilities supported by the	neighbor.
//	Learned	during the Database Exchange process (see Section 10.6).
//	The neighbor's optional	OSPF capabilities are also listed in its
//	Hello packets.	This enables received Hello Packets to be
//	rejected (i.e.,	neighbor relationships will not	even start to
//	form) if there is a mismatch in	certain	crucial	OSPF
//	capabilities (see Section 10.5).  The optional OSPF capabilities
//	are documented in Section 4.5.
	uint8_t options;

//	The neighbor's idea of the Designated Router.  If this is the
//	neighbor itself, this is important in the local	calculation of
//	the Designated Router.	Defined	only on	broadcast and NBMA
//	networks.
	uint32_t designated_router;
	
//	The neighbor's idea of the Backup Designated Router.  If this is
//	the neighbor itself, this is important in the local calculation
//	of the Backup Designated Router.  Defined only on broadcast and
//	NBMA networks.
	uint32_t backup_router;

//    The	next set of variables are lists	of LSAs.  These	lists describe
//    subsets of the area	link-state database.  This memo	defines	five
//    distinct types of LSAs, all	of which may be	present	in an area
//    link-state database: router-LSAs, network-LSAs, and	Type 3 and 4
//    summary-LSAs (all stored in	the area data structure), and AS-
//    external-LSAs (stored in the global	data structure).
//
//
//    Link state retransmission list
//	The list of LSAs that have been	flooded	but not	acknowledged on
//	this adjacency.	 These will be retransmitted at	intervals until
//	they are acknowledged, or until	the adjacency is destroyed.
	OspfLsa *rxmt_list;
	int rxmt_list_timer;
//
//    Database summary list
//	The complete list of LSAs that make up the area	link-state
//	database, at the moment	the neighbor goes into Database	Exchange
//	state.	This list is sent to the neighbor in Database
//	Description packets.
	int more0sent;
	OspfLsaHeaderList *ddsum;	// stored in network order
	
//
//    Link state request list
//	The list of LSAs that need to be received from this neighbor in
//	order to synchronize the two neighbors'	link-state databases.
//	This list is created as	Database Description packets are
//	received, and is then sent to the neighbor in Link State Request
//	packets.  The list is depleted as appropriate Link State Update
//	packets	are received.
//
	OspfLsaHeaderList *lsreq;	// stored in network order
	// last LsReq packet sent
	OspfPacket *last_lsreq_pkt;
	int lsreq_rxmt_timer;


	// DR/BDR election variables
	uint8_t bdr_autodeclared;
	uint64_t dr_election_sum;
	uint64_t bdr_election_sum;
	
	// last DD packet sent
	OspfPacket *last_dd_pkt;
	int dd_rxmt_timer;
	
	int adj_posponed;

	// last seq number from neighbor
	uint32_t ospf_md5_seq;
	
} OspfNeighbor;



//**************************************************************************
// Network
//**************************************************************************
// Not implementing Appendix F: Multiple interfaces to the same network/subnet
// for us, a network is an interface
typedef struct ospf_network_t {
	struct ospf_network_t *next;
	
//   List of neighboring	routers
//	The other routers attached to this network.  This list is formed
//	by the Hello Protocol.	Adjacencies will be formed to some of
//	these neighbors.  The set of adjacent neighbors	can be
//	determined by an examination of	all of the neighbors' states.
	OspfNeighbor *neighbor;

//	The OSPF interface type	is either point-to-point, broadcast,
//	NBMA, Point-to-MultiPoint or virtual link.
#define N_TYPE_BROADCAST 0
#define N_TYPE_VLINK 1
#define N_TYPE_MAX 2
// Not implementing p2p, NBMA, p2mp
	uint8_t type;
	
//	The functional level of	an interface.  State determines	whether
//	or not full adjacencies	are allowed to form over the interface.
//	State is also reflected	in the router's	LSAs.
#define NETSTATE_DOWN 0
#define NETSTATE_WAITING 1
#define NETSTATE_P2P 2
#define NETSTATE_DROTHER 3
#define NETSTATE_BACKUP 4
#define NETSTATE_DR 5
#define NETSTATE_MAX 6
// Not implemented loopback
	uint8_t state;
	
//	The length of time, in seconds,	between	the Hello packets that
//	the router sends on the	interface.  Advertised in Hello	packets
//	sent out this interface.
	uint16_t hello_interval;
//	An interval timer that causes the interface to send a Hello
//	packet.	 This timer fires every	HelloInterval seconds.	Note
//	that on	non-broadcast networks a separate Hello	packet is sent
//	to each	qualified neighbor.
	int hello_timer;	

//	The IP address associated with the interface.  This appears as
//	the IP source address in all routing protocol packets originated
//	over this interface.  Interfaces to unnumbered point-to-point
//	networks do not	have an	associated IP address.
	uint32_t ip;

//	Also referred to as the	subnet mask, this indicates the	portion
//	of the IP interface address that identifies the	attached
//	network.  Masking the IP interface address with	the IP interface
//	mask yields the	IP network number of the attached network.  On
//	point-to-point networks	and virtual links, the IP interface mask
//	is not defined.	On these networks, the link itself is not
//	assigned an IP network number, and so the addresses of each side
//	of the link are	assigned independently,	if they	are assigned at
//	all.
	uint32_t mask;

//	The Area ID of the area	to which the attached network belongs.
//	All routing protocol packets originating from the interface are
//	labeled with this Area	ID.
	uint32_t area_id;
	uint32_t router_id;	// router_id

	
	
//	The number of seconds before the router's neighbors will declare
//	it down, when they stop	hearing	the router's Hello Packets.
//	Advertised in Hello packets sent out this interface.
	uint32_t dead_interval;

//	The estimated number of	seconds	it takes to transmit a Link
//	State Update Packet over this interface.  LSAs contained in the
//	Link State Update packet will have their age incremented by this
//	amount before transmission.  This value	should take into account
//	transmission and propagation delays; it	must be	greater	than
//	zero.
//	uint32_t intf_trans_delay;

//	An 8-bit unsigned integer.  When two routers attached to a
//	network	both attempt to	become Designated Router, the one with
//	the highest Router Priority takes precedence.  A router	whose
//	Router Priority	is set to 0 is ineligible to become Designated
//	Router on the attached network.	 Advertised in Hello packets
//	sent out this interface.
	uint8_t router_priority;
	
//	A single shot timer that causes	the interface to exit the
//	Waiting	state, and as a	consequence select a Designated	Router
//	on the network.	 The length of the timer is RouterDeadInterval
//	seconds.
	int wait_timer;
	
//	The Designated Router selected for the attached	network.  The
//	Designated Router is selected on all broadcast and NBMA	networks
//	by the Hello Protocol.	Two pieces of identification are kept
//	for the	Designated Router: its Router ID and its IP interface
//	address	on the network.	 The Designated	Router advertises link
//	state for the network; this network-LSA	is labelled with the
//	Designated Router's IP address.	 The Designated	Router is
//	initialized to 0.0.0.0,	which indicates	the lack of a Designated
//	Router.
	uint32_t designated_router;
	
//	The Backup Designated Router is	also selected on all broadcast
//	and NBMA networks by the Hello Protocol.  All routers on the
//	attached network become	adjacent to both the Designated	Router
//	and the	Backup Designated Router.  The Backup Designated Router
//	becomes	Designated Router when the current Designated Router
//	fails.	The Backup Designated Router is	initialized to 0.0.0.0,
	uint32_t backup_router;
	
//    Interface output cost(s)
//	The cost of sending a data packet on the interface, expressed in
//	the link state metric.	This is	advertised as the link cost for
//	this interface in the router-LSA. The cost of an interface must
//	be greater than	zero.
	uint32_t cost;
 
//    RxmtInterval
//	The number of seconds between LSA retransmissions, for
//	adjacencies belonging to this interface.  Also used when
//	retransmitting Database	Description and	Link State Request
//	Packets.
	uint32_t rxtm_interval;

//    AuType
//	The type of authentication used	on the attached	network/subnet.
//	Authentication types are defined in Appendix D.	 All OSPF packet
//	exchanges are authenticated.  Different	authentication schemes
//	may be used on different networks/subnets.
	uint16_t auth_type;
	
//    Authentication key
//	This configured	data allows the	authentication procedure to
//	generate and/or	verify OSPF protocol packets.  The
//	Authentication key can be configured on	a per-interface	basis.
//	For example, if	the AuType indicates simple password, the
//	Authentication key would be a 64-bit clear password which is
//	inserted into the OSPF packet header. If instead Autype
//	indicates Cryptographic	authentication,	then the Authentication
//	key is a shared	secret which enables the generation/verification
//	of message digests which are appended to the OSPF protocol
//	packets. When Cryptographic authentication is used, multiple
//	simultaneous keys are supported	in order to achieve smooth key
//	transition (see	Section	D.3).
	char ospf_passwd[CLI_OSPF_PASSWD + 1]; // simple authentication password
	char ospf_md5_passwd[CLI_OSPF_MD5_PASSWD + 1]; // md5 authentication password
	uint8_t ospf_md5_key;	// key id
	
	// dr/brd election
	OspfNeighbor *dr_nb;
	OspfNeighbor *bdr_nb;
	
	// flooding
	OspfLsa *flooding_alldrouters;
	OspfLsa *flooding_allspfrouters;
	int flooding_cnt;
} OspfNetwork;



//**************************************************************************
// area
//**************************************************************************
typedef struct ospf_area_t {
	struct ospf_area_t *next;

	uint32_t area_id;
	uint32_t router_id;
	OspfNetwork *network;
	uint8_t stub;	// stab area flag; set trough configuration
	uint8_t no_summary;	// no summary lsa for stub area
	
	// LSA lists
	OspfLsa *list[LSA_TYPE_MAX];	// a linked list for each type
	OspfLsa *mylsa;	// this router's lsa
} OspfArea;

// area.c
OspfArea *areaGetList(void);
int areaCount(void);
void neighborFree(OspfNeighbor *nb);
OspfArea *areaFind(uint32_t area_id);
OspfArea *areaFindAnyIp(uint32_t ip);
int areaAdd(OspfArea *area);
int areaRemove(OspfArea *area);
int networkCount(uint32_t area_id);
OspfNetwork *networkFind(uint32_t area_id, uint32_t ip, uint32_t mask);
OspfNetwork *networkFindIp(uint32_t area_id, uint32_t ip);
OspfNetwork *networkFindAnyIp(uint32_t ip);
int networkAdd(uint32_t area_id, OspfNetwork *net);
int networkRemove(uint32_t area_id, OspfNetwork *net);
int neighborCount(OspfNetwork *net);
OspfNeighbor *neighborFind(uint32_t area_id, uint32_t ip);
int neighborAdd(uint32_t area_id, OspfNeighbor *nb);
int neighborRemove(uint32_t area_id, OspfNeighbor *nb);
int neighborUnlink(uint32_t area_id, OspfNeighbor *nb);
uint32_t neighborRid2Ip(uint32_t area_id, uint32_t rid);
void areaPrint(void);
void areaTest(void);
void  active_neighbor_update(void);

// network_fsm.c
extern int network_lsa_update;
const char *netfsmState2String(unsigned state);
void netfsmInterfaceUp(OspfNetwork *net);
void netfsmWaitTimer(OspfNetwork *net);
void netfsmBackupSeen(OspfNetwork *net);
void netfsmNeighborChange(OspfNetwork *net);
void netfsmInterfaceDown(OspfNetwork *net);
void debug_print_lsa_list(OspfLsa *lsa);
void networkTimeout(int no_inactivity);


// neighbor_fsm.c
const char *nfsmState2String(unsigned state);
void nfsmHelloReceived(OspfNetwork *net, OspfNeighbor *nb);
void nfsmStart(OspfNetwork *net, OspfNeighbor *nb);
void nfsm2WayReceived(OspfNetwork *net, OspfNeighbor *nb);
void nfsmNegotiationDone(OspfNetwork *net, OspfNeighbor *nb);
void nfsmExchangeDone(OspfNetwork *net, OspfNeighbor *nb);
void nfsmBadLSReq(OspfNetwork *net, OspfNeighbor *nb);
void nfsmLoadingDone(OspfNetwork *net, OspfNeighbor *nb);
void nfsmAdjOK(OspfNetwork *net, OspfNeighbor *nb);
void nfsmSeqNumberMismatch(OspfNetwork *net, OspfNeighbor *nb);
void nfsm1Way(OspfNetwork *net, OspfNeighbor *nb);
void nfsmKillNbr(OspfNetwork *net, OspfNeighbor *nb);
void nfsmInactivityTimer(OspfNetwork *net, OspfNeighbor *nb);

// auth.c
int auth_rx(OspfPacket *pkt, OspfPacketDesc *desc);
void auth_tx(OspfPacket *pkt, OspfNetwork *net);

#endif
