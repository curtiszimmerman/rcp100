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

static unsigned last_state = 0;


static inline void TRACE_NSTATE(OspfNeighbor *nb) {
	if (last_state != nb->state) {
		// logging
		if (shm->config.ospf_log_adjacency_detail)
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_WARNING, RLOG_FC_OSPF_PKT,
				"OSPF neighbor %d.%d.%d.%d state changed to %s",
				RCP_PRINT_IP((nb)->ip), nfsmState2String((nb)->state));
		else if (shm->config.ospf_log_adjacency &&
		            (nb->state == NSTATE_DOWN || nb->state == NSTATE_FULL))
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_WARNING, RLOG_FC_OSPF_PKT,
				"OSPF neighbor %d.%d.%d.%d state changed to %s",
				RCP_PRINT_IP((nb)->ip), nfsmState2String((nb)->state));
		else
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"OSPF neighbor %d.%d.%d.%d state changed to %s",
				RCP_PRINT_IP((nb)->ip), nfsmState2String((nb)->state));
		
		// snmp trap send when:
		// - the state regresses
		// - 2way or full
		if (nb->state < last_state ||
		    nb->state == NSTATE_2WAY ||
		    nb->state == NSTATE_FULL)
		trap_NbrStateChange(nb);
	}
}

static inline void restart_inactivity_timer(OspfNetwork *net, OspfNeighbor *nb) {
	nb->inactivity_timer = net->dead_interval;
}


static void update_lsadb(OspfNetwork *net) {
	if (net->state == NETSTATE_DR) {
		OspfLsa *lsa = lsa_originate_net(net);
		if (lsa != NULL)
			lsadbAdd(net->area_id, lsa);
	}

	OspfLsa *lsa = lsa_originate_rtr(net->area_id);
	if (lsa != NULL)
		lsadbAdd(net->area_id, lsa);

}

static void clear_lsa(OspfNeighbor *nb, OspfNetwork *net) {
	TRACE_FUNCTION();
	// clearing Database summary list
	lsahListRemoveAll(&nb->ddsum);
	// clearing LS request list
	lsahListRemoveAll(&nb->lsreq);
	
	// clearing LS update list
	nb->rxmt_list_timer = 0;
	OspfLsa *lsa = nb->rxmt_list;
	while (lsa != NULL) {
		OspfLsa *next = lsa->h.next;
		lsaFree(lsa);
		lsa = next;
	}
	nb->rxmt_list = NULL;

	// force a max age update on neighbors router lsa
	lsadbAgeRemoveNeighbor(nb->router_id, net->area_id);
	lsadbAgeRemoveNeighborNetwork(nb->router_id, net);
}

static  void clean_neighbor(OspfNeighbor *nb, OspfNetwork *net) {
	TRACE_FUNCTION();
	nb->state = NSTATE_DOWN;
	nb->inactivity_timer = 0;
	nb->master = 0;
	nb->dd_seq_number = 0;
	nb->priority = 0;
	nb->designated_router = 0;
	nb->backup_router = 0;
	nb->adj_posponed = 0;
	if (nb->last_dd_pkt) {
		free(nb->last_dd_pkt);
		nb->last_dd_pkt = NULL;
	}	
	if (nb->last_lsreq_pkt) {
		free(nb->last_lsreq_pkt);
		nb->last_lsreq_pkt = NULL;
	}	
	clear_lsa(nb, net);
}

static int become_adjacent(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);

//    10.4.  Whether to become adjacent
//
//	Adjacencies are	established with some subset of	the router's
//	neighbors.  Routers connected by point-to-point	networks,
//	Point-to-MultiPoint networks and virtual links always become
//	adjacent.  
	if (net->type == N_TYPE_VLINK)
		return 1;
//                                 On broadcast	and NBMA networks, all routers become
//	adjacent to both the Designated	Router and the Backup Designated
//	Router.
	else if (nb->ip == net->designated_router ||
	            nb->ip == net->backup_router)
	            	return 1;
//
//	The adjacency-forming decision occurs in two places in the
//	neighbor state machine.	 First,	when bidirectional communication
//	is initially established with the neighbor, and	secondly, when
//	the identity of	the attached network's (Backup)	Designated
//	Router changes.	 If the	decision is made to not	attempt	an
//	adjacency, the state of	the neighbor communication stops at 2-
//	Way.
//
//	An adjacency should be established with	a bidirectional	neighbor
//	when at	least one of the following conditions holds:
//
//	o   The	underlying network type	is point-to-point
//	o   The	underlying network type	is Point-to-MultiPoint
//	o   The	underlying network type	is virtual link
//	o   The	router itself is the Designated	Router
//	o   The	router itself is the Backup Designated Router
//	o   The	neighboring router is the Designated Router
//	o   The	neighboring router is the Backup Designated Router
//
	else if (net->ip == net->designated_router ||
	            net->ip == net->backup_router)
	            	return 1;
	
	return 0;
}

void start_adj(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(nb != NULL);
	ASSERT(net != NULL);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
		"establishing adjacency with neighbor %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
//		Upon
//		    entering this state, the router increments the DD
//		    sequence number in the neighbor data structure.  If
//		    this is the	first time that	an adjacency has been
//		    attempted, the DD sequence number should be	assigned
//		    some unique	value (like the	time of	day clock).  It
//		    then declares itself master	(sets the master/slave
//		    bit	to master), and	starts sending Database
//		    Description	Packets, with the initialize (I), more
//		    (M)	and master (MS)	bits set.  This	Database
//		    Description	Packet should be otherwise empty.  This
//		    Database Description Packet	should be retransmitted
//		    at intervals of RxmtInterval until the next	state is
//		    entered (see Section 10.8).	


	OspfArea *area = areaFind(net->area_id);
	ASSERT(area);
		
	if (nb->dd_seq_number == 0)
		nb->dd_seq_number = time(NULL);
	nb->dd_seq_number++;
	nb->master = 1;
	if (area && area->stub)
		nb->options = 0x40;
	else
		nb->options = 0x42;

	// build the packet	
	OspfPacket pkt;
	dbdescBuildPkt(net, nb, &pkt, 7);
	
	// send the packet
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
		"sending DD packet as master to neighbor %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
	txpacket_unicast(&pkt, net, nb->ip);
}

static char *strstate[] = {
	"Down",
	"Attempt",
	"Init",
	"2-Way",
	"ExStart",
	"Exchange",
	"Loading",
	"Full"
};

const char *nfsmState2String(unsigned state) {
	if (state >= NSTATE_MAX) {
		ASSERT(0);
		return NULL;
	}
	
	return strstate[state];
}



//    10.2.  Events causing neighbor state changes

void nfsmHelloReceived(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
	
//	    An Hello packet has	been received from the neighbor.

//	 State(s):  Attempt
//	    Event:  HelloReceived
//	New state:  Init
//	   Action:  Restart the	Inactivity Timer for the neighbor, since
//		    the	neighbor has now been heard from.
	if (nb->state == NSTATE_ATTEMPT) {
		nb->state = NSTATE_INIT;
		restart_inactivity_timer(net, nb);
	}		

//	 State(s):  Down
//	    Event:  HelloReceived
//	New state:  Init
//	   Action:  Start the Inactivity Timer for the neighbor.  The
//		    timer's later firing would indicate	that the
//		    neighbor is	dead.
	else if (nb->state == NSTATE_DOWN) {
		nb->state = NSTATE_INIT;
		restart_inactivity_timer(net, nb);
	}		

//	 State(s):  Init or greater
//	    Event:  HelloReceived
//	New state:  No state change.
//	   Action:  Restart the	Inactivity Timer for the neighbor, since
//		    the	neighbor has again been	heard from.

	else if (nb->state >= NSTATE_INIT) {
		 restart_inactivity_timer(net, nb);
	}
	
	else
		ASSERT(0);

	TRACE_NSTATE(nb);
}

void nfsmStart(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
#if 0
	    This is an indication that Hello Packets should now	be sent
	    to the neighbor at intervals of HelloInterval seconds.  This
	    event is generated only for	neighbors associated with NBMA
	    networks.


	 State(s):  Down
	    Event:  Start
	New state:  Attempt
	   Action:  Send an Hello Packet to the	neighbor (this neighbor
		    is always associated with an NBMA network) and start
		    the	Inactivity Timer for the neighbor.  The	timer
		    later firing would indicate	that communication with
		    the	neighbor was not attained.
#endif
	TRACE_NSTATE(nb);
}

void nfsm2WayReceived(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    Bidirectional communication	has been realized between the
//	    two	neighboring routers.  This is indicated	by the router
//	    seeing itself in the neighbor's Hello packet.


//	 State(s):  Init
//	    Event:  2-WayReceived
//	New state:  Depends upon action	routine.
//	   Action:  Determine whether an adjacency should be established
//		    with the neighbor (see Section 10.4).  If not, the
//		    new	neighbor state is 2-Way.
	int doadj = become_adjacent(net, nb);
	if (nb->state == NSTATE_INIT && doadj == 0)
		nb->state = NSTATE_2WAY;

//		    Otherwise (an adjacency should be established) the
//		    neighbor state transitions to ExStart.  Upon
//		    entering this state, the router increments the DD
//		    sequence number in the neighbor data structure.  If
//		    this is the	first time that	an adjacency has been
//		    attempted, the DD sequence number should be	assigned
//		    some unique	value (like the	time of	day clock).  It
//		    then declares itself master	(sets the master/slave
//		    bit	to master), and	starts sending Database
//		    Description	Packets, with the initialize (I), more
//		    (M)	and master (MS)	bits set.  This	Database
//		    Description	Packet should be otherwise empty.  This
//		    Database Description Packet	should be retransmitted
//		    at intervals of RxmtInterval until the next	state is
//		    entered (see Section 10.8).
	else if (nb->state == NSTATE_INIT && doadj == 1) {
		nb->state = NSTATE_EXSTART;
		start_adj(net, nb);	
	}

//	 State(s):  2-Way or greater
//	    Event:  2-WayReceived
//	New state:  No state change.
//	   Action:  No action required.
	if (nb->state >= NSTATE_2WAY)
		;

	TRACE_NSTATE(nb);
}

void nfsmNegotiationDone(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    The	Master/Slave relationship has been negotiated, and DD
//	    sequence numbers have been exchanged.  This	signals	the
//	    start of the sending/receiving of Database Description
//	    packets.  For more information on the generation of	this
//	    event, consult Section 10.8.


//	 State(s):  ExStart
//	    Event:  NegotiationDone
//	New state:  Exchange
//	   Action:  The	router must list the contents of its entire area
//		    link state database	in the neighbor	Database summary
//		    list.  The area link state database	consists of the
//		    router-LSAs, network-LSAs and summary-LSAs contained
//		    in the area	structure, along with the AS-external-
//		    LSAs contained in the global structure.  AS-
//		    external-LSAs are omitted from a virtual neighbor's
//		    Database summary list.  AS-external-LSAs are omitted
//		    from the Database summary list if the area has been
//		    configured as a stub (done) (see Section 3.6).  LSAs whose
//		    age	is equal to MaxAge are instead added to	the
//		    neighbor's Link state retransmission list.	A
//		    summary of the Database summary list will be sent to
//		    the	neighbor in Database Description packets.  Each
//		    Database Description Packet	has a DD sequence
//		    number, and	is explicitly acknowledged.  Only one
//		    Database Description Packet	is allowed outstanding
//		    at any one time.  For more detail on the sending and
//		    receiving of Database Description packets, see
//		    Sections 10.8 and 10.6.
	if (nb->state == NSTATE_EXSTART) {
		ASSERT(nb->ddsum == NULL);
		nb->more0sent = 0;
		nb->dd_rxmt_timer = 0;
		if (nb->last_dd_pkt) {
			free(nb->last_dd_pkt);
		}
		nb->last_dd_pkt = NULL;
			
		OspfArea *area = areaFind(net->area_id);
		ASSERT(area);
		
		// walk lsadb and add all lsa's to database summary list
		int i;
		int cnt = 0;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT, 
			"building database description summary list for neighbor %d.%d.%d.%d",
			RCP_PRINT_IP(nb->ip));
		for (i = 0; i < LSA_TYPE_MAX; i++) {
			if (i == LSA_TYPE_EXTERNAL && area && area->stub)
				continue;

			OspfLsa *lsa = lsadbGetList(net->area_id, i);
			while (lsa != NULL) {
				lsahListAdd(&nb->ddsum, &lsa->header);
//				OspfLsaHeader *lsah = &lsa->header;
//				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
//					"LSA %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x added to ddseq",
//					RCP_PRINT_IP(ntohl(lsah->link_state_id)),
//					lsah->type,
//					RCP_PRINT_IP(ntohl(lsah->adv_router)),
//					ntohl(lsah->seq));

				cnt ++;
				lsa = lsa->h.next;
			}
		}
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
			"%d LSAs to be sent to neighbor %d.%d.%d.%d", cnt, RCP_PRINT_IP(nb->ip));		
		nb->state = NSTATE_EXCHANGE;
	}

	TRACE_NSTATE(nb);
}

void nfsmExchangeDone(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    Both routers have successfully transmitted a full sequence
//	    of Database	Description packets.  Each router now knows what
//	    parts of its link state database are out of	date.  For more
//	    information	on the generation of this event, consult Section
//	    10.8.

//	 State(s):  Exchange
//	    Event:  ExchangeDone
//	New state:  Depends upon action	routine.
//	   Action:  If the neighbor Link state request list is empty,
//		    the	new neighbor state is Full.  No	other action is
//		    required.  This is an adjacency's final state.
//		    Otherwise, the new neighbor	state is Loading.  Start
//		    (or	continue) sending Link State Request packets to
//		    the	neighbor (see Section 10.9).  These are	requests
//		    for	the neighbor's more recent LSAs	(which were
//		    discovered but not yet received in the Exchange
//		    state).  These LSAs	are listed in the Link state
//		    request list associated with the neighbor.
	if (nb->state == NSTATE_EXCHANGE) {
		// clear dbrxmt
		if (nb->last_dd_pkt) {
			free(nb->last_dd_pkt);
			nb->last_dd_pkt = NULL;
		}
		
		if (nb->lsreq == NULL || lsahListIsAck(&nb->lsreq)) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
				"neighbor %d.%d.%d.%d synchronized", RCP_PRINT_IP(nb->ip));
			nb->state = NSTATE_FULL;
			// update network lsa if we are DR
			if (net->designated_router == net->ip) {
				OspfLsa *lsa = lsa_originate_net(net);
				if (lsa != NULL)
					lsadbAdd(net->area_id, lsa);
			}
		}
		else {
			nb->state = NSTATE_LOADING;

			// sending a link state request packet to neighbor
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"sending LS request to neighbor %d.%d.%d.%d",
				RCP_PRINT_IP(nb->ip));
				
			// build the packet	
			OspfPacket pkt;
			lsrequestBuildPkt(net, nb, &pkt);
			// send the packet
			txpacket_unicast(&pkt, net, nb->ip);
		}
	}

	TRACE_NSTATE(nb);
}

void nfsmBadLSReq(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    A Link State Request has been received for an LSA not
//	    contained in the database.	This indicates an error	in the
//	    Database Exchange process.

//	 State(s):  Exchange or	greater
//	    Event:  BadLSReq
//	New state:  ExStart
//	   Action:  The	action for event BadLSReq is exactly the same as
//		    for	the neighbor event SeqNumberMismatch.  The
//		    (possibly partially	formed)	adjacency is torn down,
//		    and	then an	attempt	is made	at reestablishment.  For
//		    more information, see the neighbor state machine
//		    entry that is invoked when event SeqNumberMismatch
//		    is generated in state Exchange or greater.
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
		"bad ls request from neighbor %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
	nfsmSeqNumberMismatch(net, nb);
//	TRACE_NSTATE(nb);
}

void nfsmLoadingDone(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    Link State Updates have been received for all out-of-date
//	    portions of	the database.  This is indicated by the	Link
//	    state request list becoming	empty after the	Database
//	    Exchange process has completed.

//	 State(s):  Loading
//	    Event:  Loading Done
//	New state:  Full
//	   Action:  No action required.	 This is an adjacency's	final
//		    state.

	if (nb->state == NSTATE_LOADING) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
			"neighbor %d.%d.%d.%d synchronized", RCP_PRINT_IP(nb->ip));
		nb->state = NSTATE_FULL;
		// update network lsa if we are DR
		if (net->designated_router == net->ip) {
			OspfLsa *lsa = lsa_originate_net(net);
			if (lsa != NULL)
				lsadbAdd(net->area_id, lsa);
		}
	}
	TRACE_NSTATE(nb);
}

void nfsmAdjOK(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    A decision must be made as to whether an adjacency should be
//	    established/maintained with	the neighbor.  This event will
//	    start some adjacencies forming, and	destroy	others.

//	 State(s):  2-Way
//	    Event:  AdjOK?
//	New state:  Depends upon action	routine.
//	   Action:  Determine whether an adjacency should be formed with
//		    the	neighboring router (see	Section	10.4).	If not,
//		    the	neighbor state remains at 2-Way.  Otherwise,
//		    transition the neighbor state to ExStart and perform
//		    the	actions	associated with	the above state	machine
//		    entry for state Init and event 2-WayReceived.

	int doadj = become_adjacent(net, nb);
	if (nb->state == NSTATE_2WAY && doadj == 1) {
		OspfNeighbor *ne = net->neighbor;
		int cnt = 0;
		while (ne) {
			if (ne->state == NSTATE_EXSTART || ne->state == NSTATE_EXCHANGE || ne->state == NSTATE_LOADING)
				cnt++;
			ne = ne->next;
		}
	
		if (cnt >= OSPF_NB_ADJ_MAX) {
			nb->adj_posponed	= 1;
			return;
		}
		else {
			nb->adj_posponed	= 0;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
				"synchronizing database with neighbor %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
		}
		
		nb->state = NSTATE_EXSTART;
		start_adj(net, nb);	
	}

//	 State(s):  ExStart or greater
//	    Event:  AdjOK?
//	New state:  Depends upon action	routine.
//	   Action:  Determine whether the neighboring router should
//		    still be adjacent.	If yes,	there is no state change
//		    and	no further action is necessary.
//		    Otherwise, the (possibly partially formed) adjacency
//		    must be destroyed.	The neighbor state transitions
//		    to 2-Way.  The Link	state retransmission list,
//		    Database summary list and Link state request list
//		    are	cleared	of LSAs.
	else if (nb->state >= NSTATE_EXSTART && doadj == 0) {
		clear_lsa(nb, net);
		nb->state = NSTATE_2WAY;
	}
			
	TRACE_NSTATE(nb);
}

void nfsmSeqNumberMismatch(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    A Database Description packet has been received that either
//	    a) has an unexpected DD sequence number, b)	unexpectedly has
//	    the	Init bit set or	c) has an Options field	differing from
//	    the	last Options field received in a Database Description
//	    packet.  Any of these conditions indicate that some	error
//	    has	occurred during	adjacency establishment.

//	 State(s):  Exchange or	greater
//	    Event:  SeqNumberMismatch
//	New state:  ExStart
//	   Action:  The	(possibly partially formed) adjacency is torn
//		    down, and then an attempt is made at
//		    reestablishment.  The neighbor state first
//		    transitions	to ExStart.  The Link state
//		    retransmission list, Database summary list and Link
//		    state request list are cleared of LSAs.  Then the
//		    router increments the DD sequence number in	the
//		    neighbor data structure, declares itself master
//		    (sets the master/slave bit to master), and starts
//		    sending Database Description Packets, with the
//		    initialize (I), more (M) and master	(MS) bits set.
//		    This Database Description Packet should be otherwise
//		    empty (see Section 10.8).

	if (nb->state >= NSTATE_EXCHANGE) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
			"restarting database exchange with neighbor %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
		nb->state = NSTATE_EXSTART;
		nb->dd_seq_number++;
		if (nb->last_dd_pkt) {
			free(nb->last_dd_pkt);
			nb->last_dd_pkt = NULL;
		}	
		if (nb->last_lsreq_pkt) {
			free(nb->last_lsreq_pkt);
			nb->last_lsreq_pkt = NULL;
		}	
	
		// clearing Database summary list
		lsahListRemoveAll(&nb->ddsum);
		// clearing LS request list
		lsahListRemoveAll(&nb->lsreq);
		
		// clearing LS update list
		nb->rxmt_list_timer = 0;
		OspfLsa *lsa = nb->rxmt_list;
		while (lsa != NULL) {
			OspfLsa *next = lsa->h.next;
			lsaFree(lsa);
			lsa = next;
		}
		nb->rxmt_list = NULL;
	
//		// force a max age update on any lsa from this neighbor
//		lsadbAgeRemoveNeighbor(nb->router_id);
	
		start_adj(net, nb);	
	}
	
	TRACE_NSTATE(nb);
}



void nfsm1Way(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    An Hello packet has	been received from the neighbor, in
//	    which the router is	not mentioned.	This indicates that
//	    communication with the neighbor is not bidirectional.

//	 State(s):  Init
//	    Event:  1-WayReceived
//	New state:  No state change.
//	   Action:  No action required.
	if (nb->state == NSTATE_INIT)
		;

//	 State(s):  2-Way or greater
//	    Event:  1-WayReceived
//	New state:  Init
//	   Action:  The	Link state retransmission list,	Database summary
//		    list and Link state	request	list are cleared of
//		    LSAs.
	if (nb->state >= NSTATE_2WAY) {
		nb->state = NSTATE_INIT;
		clear_lsa(nb, net);
	}
	TRACE_NSTATE(nb);
}


void nfsmKillNbr(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
	
	uint32_t tmp = nb->ip;
	
//	    This  is  an  indication that  all	communication  with  the
//	    neighbor  is now  impossible,  forcing  the	 neighbor  to
//	    revert  to	Down  state.

//	 State(s):  Any	state
//	    Event:  KillNbr
//	New state:  Down
//	   Action:  The	Link state retransmission list,	Database summary
//		    list and Link state	request	list are cleared of
//		    LSAs.  Also, the Inactivity	Timer is disabled.

	clean_neighbor(nb, net);
	TRACE_NSTATE(nb);
	
	// remove the neighbor
	neighborRemove(net->area_id, nb);
	update_lsadb(net);
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
		"Neighbor %d.%d.%d.%d removed\n", RCP_PRINT_IP(tmp));
	netfsmNeighborChange(net);
}

void nfsmInactivityTimer(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    The	inactivity Timer has fired.  This means	that no	Hello
//	    packets have been seen recently from the neighbor.	The
//	    neighbor reverts to	Down state.

//	 State(s):  Any	state
//	    Event:  InactivityTimer
//	New state:  Down
//	   Action:  The	Link state retransmission list,	Database summary
//		    list and Link state	request	list are cleared of
//		    LSAs.

	clean_neighbor(nb, net);
	TRACE_NSTATE(nb);
	
	// remove the neighbor if we are on a broadcast network
	if (net->type == N_TYPE_BROADCAST ) {
		neighborRemove(net->area_id, nb);
		update_lsadb(net);
		netfsmNeighborChange(net);
	}
}

void nfsmLLDown(OspfNetwork *net, OspfNeighbor *nb) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	ASSERT(nb != NULL);
	last_state = nb->state;
//	    This is an indication from the lower level protocols that
//	    the	neighbor is now	unreachable.  For example, on an X.25
//	    network this could be indicated by an X.25 clear indication
//	    with appropriate cause and diagnostic fields.  This	event
//	    forces the neighbor	into Down state.

//	 State(s):  Any	state
//	    Event:  LLDown
//	New state:  Down
//	   Action:  The	Link state retransmission list,	Database summary
//		    list and Link state	request	list are cleared of
//		    LSAs.  Also, the Inactivity	Timer is disabled.
	clean_neighbor(nb, net);
	TRACE_NSTATE(nb);
	
}




	
