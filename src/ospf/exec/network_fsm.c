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
int network_lsa_update = 0;
uint32_t systic_delta_network = 0;

static inline void TRACE_NSTATE(OspfNetwork *net) {
	if (net->state != last_state) {
		trap_IfStateChange(net);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
			"network %d.%d.%d.%d state changed to %s",
			RCP_PRINT_IP((net)->ip), netfsmState2String((net)->state));

		lsa_originate_net_update(); // just in case...
	}
}


static char *strstate[] = {
	"Down",
	"Waiting",
	"P2P",
	"DROther",
	"Backup",
	"DR",
};

const char *netfsmState2String(unsigned state) {
	if (state >= NETSTATE_MAX) {
		ASSERT(0);
		return NULL;
	}
	
	return strstate[state];
}

//    9.2.  Events causing interface state changes
//	InterfaceUp
//	    Lower-level	protocols have indicated that the network
//	    interface is operational.  This enables the	interface to
//	    transition out of Down state.  On virtual links, the
//	    interface operational indication is	actually a result of the
//	    shortest path calculation (see Section 16.7).

void netfsmInterfaceUp(OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);

//	 State(s):  Down
//	    Event:  InterfaceUp
//	New state:  Depends upon action	routine
//	   Action:  Start the interval Hello Timer, enabling the
//		    periodic sending of	Hello packets out the interface.
	if (net->state == NETSTATE_DOWN) {
//		    If the attached network is a physical point-to-point
//		    network, Point-to-MultiPoint network or virtual
//		    link, the interface	state transitions to Point-to-
//		    Point.  Else, if the router	is not eligible	to
//		    become Designated Router the interface state
//		    transitions	to DR Other.

		if (net->type == N_TYPE_VLINK) {
			last_state = net->state;
			net->state =  NETSTATE_P2P;
		}
		else
//		    Otherwise, the attached network is a broadcast or
//		    NBMA network and the router	is eligible to become
//		    Designated Router.	In this	case, in an attempt to
//		    discover the attached network's Designated Router
//		    the	interface state	is set to Waiting and the single
//		    shot Wait Timer is started.	
#if 0
		                                                 Additionally, if the
		    network is an NBMA network examine the configured
		    list of neighbors for this interface and generate
		    the	neighbor event Start for each neighbor that is
		    also eligible to become Designated Router.
#endif
			last_state = net->state;
			net->state = NETSTATE_WAITING;
			net->wait_timer = net->dead_interval; // wait timer started
	}
	TRACE_NSTATE(net);
}

//	WaitTimer
//	    The	Wait Timer has fired, indicating the end of the	waiting
//	    period that	is required before electing a (Backup)
//	    Designated Router.
void netfsmWaitTimer(OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);

//	 State(s):  Waiting
//	    Event:  WaitTimer
//	New state:  Depends upon action	routine.
//	   Action:  Calculate the attached network's Backup Designated
//		    Router and Designated Router, as shown in Section
//		    9.4.  As a result of this calculation, the new state
//		    of the interface will be either DR Other, Backup or
//		    DR.
	if (net->state == NETSTATE_WAITING) {
		// election time!
		drbdrElection(net);

		// originate a router lsa
		OspfLsa *lsa = lsa_originate_rtr(net->area_id);
		if (lsa != NULL)
			lsadbAdd(net->area_id, lsa);

		last_state = net->state;
		if (net->ip == net->designated_router) {
			net->state = NETSTATE_DR;
			network_lsa_update = 1;
		}
		else if (net->ip == net->backup_router)
			net->state = NETSTATE_BACKUP;
		else
			net->state = NETSTATE_DROTHER;
		
	}

	TRACE_NSTATE(net);
}

//	BackupSeen
//	    The	router has detected the	existence or non-existence of a
//	    Backup Designated Router for the network.  This is done in
//	    one	of two ways.  First, an	Hello Packet may be received
//	    from a neighbor claiming to	be itself the Backup Designated
//	    Router.  Alternatively, an Hello Packet may	be received from
//	    a neighbor claiming	to be itself the Designated Router, and
//	    indicating that there is no	Backup Designated Router.  In
//	    either case	there must be bidirectional communication with
//	    the	neighbor, i.e.,	the router must	also appear in the
//	    neighbor's Hello Packet.  This event signals an end	to the
//	    Waiting state.
void netfsmBackupSeen(OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	last_state = net->state;

//	 State(s):  Waiting
//	    Event:  BackupSeen
//	New state:  Depends upon action	routine.
//	   Action:  Calculate the attached network's Backup Designated
//		    Router and Designated Router, as shown in Section
//		    9.4.  As a result of this calculation, the new state
//		    of the interface will be either DR Other, Backup or
//		    DR.
	if (net->state == NETSTATE_WAITING) {
		// wait at least a hello-interval before doing anything,
		// otherwise the election might chose a wrong DR, just
		// to change its mind when the new routers hellos are received
//wait fix
//		if (net->wait_timer > (net->dead_interval / 2)) {
//			TRACE_NSTATE(net);
//			return;
//		}

		// election time!
		drbdrElection(net);

		// originate a router lsa
		OspfLsa *lsa = lsa_originate_rtr(net->area_id);
		if (lsa != NULL)
			lsadbAdd(net->area_id, lsa);

		if (net->ip == net->designated_router) {
			net->state = NETSTATE_DR;
			network_lsa_update = 1;
		}
		else if (net->ip == net->backup_router)
			net->state = NETSTATE_BACKUP;
		else
			net->state = NETSTATE_DROTHER;
	}

	TRACE_NSTATE(net);
}

//	NeighborChange
//	    There has been a change in the set of bidirectional
//	    neighbors associated with the interface.  The (Backup)
//	    Designated Router needs to be recalculated.	 The following
//	    neighbor changes lead to the NeighborChange	event.	For an
//	    explanation	of neighbor states, see	Section	10.1.
//
//	    o	Bidirectional communication has	been established to a
//		neighbor.  In other words, the state of	the neighbor has
//		transitioned to	2-Way or higher.
//
//	    o	There is no longer bidirectional communication with a
//		neighbor.  In other words, the state of	the neighbor has
//		transitioned to	Init or	lower.
//
//	    o	One of the bidirectional neighbors is newly declaring
//		itself as either Designated Router or Backup Designated
//		Router.	 This is detected through examination of that
//		neighbor's Hello Packets.
//
//	    o	One of the bidirectional neighbors is no longer
//		declaring itself as Designated Router, or is no	longer
//		declaring itself as Backup Designated Router.  This is
//		again detected through examination of that neighbor's
//		Hello Packets.
//
//	    o	The advertised Router Priority for a bidirectional
//		neighbor has changed.  This is again detected through
//		examination of that neighbor's Hello Packets.
void netfsmNeighborChange(OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	last_state = net->state;

//	 State(s):  DR Other, Backup or	DR
//	    Event:  NeighborChange
//	New state:  Depends upon action	routine.
//	   Action:  Recalculate	the attached network's Backup Designated
//		    Router and Designated Router, as shown in Section
//		    9.4.  As a result of this calculation, the new state
//		    of the interface will be either DR Other, Backup or
//		    DR.


	if (net->state == NETSTATE_DR ||
	    net->state == NETSTATE_BACKUP ||
	    net->state == NETSTATE_DROTHER) {
		drbdrElection(net);
		
		// originate a router lsa
		OspfLsa *lsa = lsa_originate_rtr(net->area_id);
		if (lsa != NULL)
			lsadbAdd(net->area_id, lsa);

		if (net->ip == net->designated_router) {
			net->state = NETSTATE_DR;
			network_lsa_update = 1;
		}
		else if (net->ip == net->backup_router)
			net->state = NETSTATE_BACKUP;
		else
			net->state = NETSTATE_DROTHER;
	}
	
	TRACE_NSTATE(net);
}

#if 0 // Not implemented
	LoopInd
	    An indication has been received that the interface is now
	    looped back	to itself.  This indication can	be received
	    either from	network	management or from the lower level
	    protocols.

	UnloopInd
	    An indication has been received that the interface is no
	    longer looped back.	 As with the LoopInd event, this
	    indication can be received either from network management or
	    from the lower level protocols.
#endif

//	InterfaceDown
//	    Lower-level	protocols indicate that	this interface is no
//	    longer functional.	No matter what the current interface
//	    state is, the new interface	state will be Down.
void netfsmInterfaceDown(OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != NULL);
	last_state = net->state;

//	 State(s):  Any	State
//	    Event:  InterfaceDown
//	New state:  Down
//	   Action:  All	interface variables are	reset, and interface
//		    timers disabled.  Also, all	neighbor connections
//		    associated with the	interface are destroyed.  This
//		    is done by generating the event KillNbr on all
//		    associated neighbors (see Section 10.2).

	net->state = NETSTATE_DOWN;
	
	// stop timers
	net->hello_timer = 0;
	net->wait_timer = 0;
	net->designated_router = 0;
	net->backup_router = 0;
	
	// kill all neighbors
	OspfNeighbor *nb = net->neighbor;
	while (nb != NULL) {
		OspfNeighbor *next = nb->next;
		nfsmKillNbr(net, nb);
		nb = next;
	}
	TRACE_NSTATE(net);
}

#if 0 // Not implemented
	 State(s):  Any	State
	    Event:  LoopInd
	New state:  Loopback
	   Action:  Since this interface is no longer connected	to the
		    attached network the actions associated with the
		    above InterfaceDown	event are executed.


	 State(s):  Loopback
	    Event:  UnloopInd
	New state:  Down
	   Action:  No actions are necessary.  For example, the
		    interface variables	have already been reset	upon
		    entering the Loopback state.  Note that reception of
		    an InterfaceUp event is necessary before the
		    interface again becomes fully functional.
#endif

#if 0
void debug_print_lsa_list(OspfLsa *lsa) {
	if (lsa == NULL)
		return;
	while (lsa != NULL) {
		OspfLsaHeader *lsah = &lsa->header;
		printf("*LSA print %d.%d.%d.%d, type %d, from %d.%d.%d.%d, seq 0x%x\n",
			RCP_PRINT_IP(ntohl(lsah->link_state_id)),
			lsah->type,
			RCP_PRINT_IP(ntohl(lsah->adv_router)),
			ntohl(lsah->seq));
		
		lsa = lsa->h.next;
	}
	

}
#endif

// call this function every second to run the various timers
void networkTimeout(int no_inactivity) {
	struct tms tm;
	uint32_t tic1 = times(&tm);

	OspfArea *area = areaGetList();

	// for each area
	while (area != NULL) {
		// for each network
		OspfNetwork *net = area->network;
		while (net != NULL) {
			if (net->state == NETSTATE_DOWN) {
				net = net->next;
				continue;
			}
			else if (net->state == NETSTATE_WAITING) {
				// decrement wait timer and trigger event
				if (--net->wait_timer <= 0) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
						"Wait timer expired for network %d.%d.%d.%d",
						RCP_PRINT_IP(net->ip));
					netfsmWaitTimer(net);
					net->wait_timer = 0;
				}
			}
			RcpInterface *intf = rcpFindInterface(shm, net->ip);
			
			OspfNeighbor *nb = net->neighbor;
			int load_cnt = 0;
			while (nb) {
				if (nb->state == NSTATE_EXSTART || nb->state == NSTATE_EXCHANGE || nb->state == NSTATE_LOADING)
					load_cnt++;
				nb = nb->next;
			}
			
			
			// for each neighbor: inactivity timer, database description retransmit timer, lsa rxmt timer
			nb = net->neighbor;
			while (nb != NULL) {
				OspfNeighbor *next = nb->next;
				
				//inactivity timer
				if (no_inactivity == 0) {
					if (nb->state != NSTATE_DOWN && --nb->inactivity_timer <= 0) {
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
							"neighbor %d.%d.%d.%d inactivity timer", RCP_PRINT_IP(nb->ip));
						nfsmInactivityTimer(net, nb);	 // nb is removed
						nb = next;
						continue;
					}
				}
				
				//database description retransmit timer
				if (nb->last_dd_pkt &&
				    (nb->state == NSTATE_EXCHANGE || nb->state == NSTATE_EXSTART) &&
				     --nb->dd_rxmt_timer <= 0) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
						"Retransmitting the last database description to neighbor %d.%d.%d.%d",
						RCP_PRINT_IP(nb->ip));
					txpacket_unicast(nb->last_dd_pkt, net, nb->ip);
					if (intf)
						intf->stats.ospf_tx_db_rxmt++;
					nb->dd_rxmt_timer = net->rxtm_interval;
				}
				else if (nb->last_dd_pkt && nb->state == NSTATE_FULL) {
					free(nb->last_dd_pkt);
					nb->last_dd_pkt = NULL;
				}
				
				// ls request retransmit timer
				if (nb->last_lsreq_pkt &&
				     nb->state == NSTATE_LOADING &&
				     --nb->lsreq_rxmt_timer <= 0) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
						"Retransmitting the last LS REQUEST to neighbor %d.%d.%d.%d",
						RCP_PRINT_IP(nb->ip));
					txpacket_unicast(nb->last_lsreq_pkt, net, nb->ip);
					if (intf)
						intf->stats.ospf_tx_lsrq_rxmt++;
					nb->lsreq_rxmt_timer = net->rxtm_interval;
				}	
				else if (nb->last_lsreq_pkt && nb->state == NSTATE_FULL) {
					free(nb->last_lsreq_pkt);
					nb->last_lsreq_pkt = NULL;
				}
					
				
				
				// prepare the utility list
				int rxmt_cnt = 0;
				if (nb->rxmt_list != NULL) {
					OspfLsa *lsa = nb->rxmt_list;
					while (lsa != NULL) {
						rxmt_cnt++;
//						uint16_t age = ntohs(lsa->header.age);
//						if (++age >= MaxAge)
//							age = MaxAge;
//						lsa->header.age = htons(age);
						lsa->h.util_next = lsa->h.next;
						lsa = lsa->h.next;
					}
				}
				
				// retransmit from rxmt list
				if (nb->state == NSTATE_FULL) {
					if (nb->rxmt_list != NULL && --nb->rxmt_list_timer <= 0) {
						rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
							"retransmiting %d LSAs to neighbor %d.%d.%d.%d",
							rxmt_cnt, RCP_PRINT_IP(nb->ip));					
						if (intf)
							intf->stats.ospf_tx_lsup_rxmt++;
							
						trap_set_retransmit(net, nb, 4 /*lsUpdate*/);
						OspfLsa *nextlsa = nb->rxmt_list;
						while (nextlsa) {
							OspfPacket out;
							nextlsa = lsupdateBuildMultiplePkt(net, &out, nextlsa, 1);
							txpacket_unicast(&out, net, nb->ip);
						}
						nb->rxmt_list_timer = net->rxtm_interval;
						trap_set_retransmit(NULL, NULL, 0);
					}
				}
				
				// check postponed adjacency
				if (load_cnt < OSPF_NB_ADJ_MAX && nb->adj_posponed) {
					nb->adj_posponed = 0;
					load_cnt++;
					nfsmAdjOK(net, nb);
				}
				
				nb = next;
			}

			// hello timer
			if (--net->hello_timer <= 0) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
					"Sending OSPF hello message on network %d.%d.%d.%d/%d",
					RCP_PRINT_IP(net->ip), mask2bits(net->mask));
				net->hello_timer = net->hello_interval;
				
				// build a hello packet
				OspfPacket pkt;
				helloBuildPkt(net, &pkt);
				
				// send the packet
				txpacket(&pkt, net);
			}
			
			net = net->next;
		}
		
		area = area->next;		
	}
	
	uint32_t tic2 = times(&tm);
	uint32_t delta;
	if (tic2 > tic1)
		delta = tic2 - tic1;
	else
		delta = tic1 - tic2;
	
	if (delta > systic_delta_network) {
		systic_delta_network = delta;
//		printf("network timeout delta %u\n", systic_delta_network);
	}
}
