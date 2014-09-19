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

#if 0
static void print_election_sum(OspfNeighbor *nb) {
	uint32_t ip1 = (uint32_t) (nb->dr_election_sum & 0xffffffffULL);
	uint32_t priority1 = (uint32_t) (nb->dr_election_sum >> 32);
	uint32_t ip2 = (uint32_t) (nb->bdr_election_sum & 0xffffffffULL);
	uint32_t priority2 = (uint32_t) (nb->dr_election_sum >> 32);
	
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
		"election sum dr: priority %u, %d.%d.%d.%d",
		priority1, 	RCP_PRINT_IP(ip1));
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
		"election sum bdr: priority %u, %d.%d.%d.%d",
		priority2, 	RCP_PRINT_IP(ip2));
}
#endif

//
//	The reason behind the election algorithm's complexity is the
//	desire for an orderly transition from Backup Designated	Router
//	to Designated Router, when the current Designated Router fails.
//	This orderly transition	is ensured through the introduction of
//	hysteresis: no new Backup Designated Router can	be chosen until
//	the old	Backup accepts its new Designated Router
//	responsibilities.
//
//	The above procedure may	elect the same router to be both
//	Designated Router and Backup Designated	Router,	although that
//	router will never be the calculating router (Router X) itself.
//	The elected Designated Router may not be the router having the
//	highest	Router Priority, nor will the Backup Designated	Router
//	necessarily have the second highest Router Priority.  If Router
//	X is not itself	eligible to become Designated Router, it is
//	possible that neither a	Backup Designated Router nor a
//	Designated Router will be selected in the above	procedure.  Note
//	also that if Router X is the only attached router that is
//	eligible to become Designated Router, it will select itself as
//	Designated Router and there will be no Backup Designated Router
//	for the	network.


static void step2and3(OspfNetwork *net) {
	TRACE_FUNCTION();
	net->dr_nb = NULL;
	net->bdr_nb = NULL;
	
//	(2) Calculate the new Backup Designated	Router for the network
//	    as follows.	 Only those routers on the list	that have not
//	    declared themselves	to be Designated Router	are eligible to
//	    become Backup Designated Router. 
	OspfNeighbor *nb = net->neighbor;
	while (nb != NULL) {
		if (nb->bdr_election_sum) {
			if (nb->designated_router == nb->ip)
				nb->bdr_election_sum = 0;
 		}
 		nb = nb->next;
	}

	nb = net->neighbor;
	while (nb != NULL) {
		if (nb->bdr_election_sum) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
				"step 2.1 list: %d.%d.%d.%d (dr %d.%d.%d.%d, backup %d.%d.%d.%d)",
				RCP_PRINT_IP(nb->ip),
				RCP_PRINT_IP(nb->designated_router),
				RCP_PRINT_IP(nb->backup_router));
		}
 		nb = nb->next;
	}



//		                                                 If one or	more of	these
//	    routers have declared themselves Backup Designated Router
//	    (i.e., they	are currently listing themselves as Backup
//	    Designated Router, but not as Designated Router, in	their
//	    Hello Packets) the one having highest Router Priority is
//	    declared to	be Backup Designated Router. In case of a tie,
//	    the	one having the highest Router ID is chosen.
	nb = net->neighbor;
	OspfNeighbor *bdr_nb = NULL;
	while (nb != NULL) {
		if (nb->bdr_election_sum) {
			if (nb->backup_router == nb->ip) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
					"step 2: declaring himself backup %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
				if (bdr_nb == NULL) {
					bdr_nb = nb;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
						"step 2: backup set to %d.%d.%d.%d", RCP_PRINT_IP(bdr_nb->ip));
				}
 				else if (nb->bdr_election_sum > bdr_nb->bdr_election_sum) {
					bdr_nb = nb;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
						"step 2: backup break tie %d.%d.%d.%d", RCP_PRINT_IP(bdr_nb->ip));
				}
			}
 		}
 		nb = nb->next;
	}
	
	if (bdr_nb == NULL) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"step 2: backup NULL");
//	      If	no
//	    routers have declared themselves Backup Designated Router,
//	    choose the router having highest Router Priority, (again
//	    excluding those routers who	have declared themselves
//	    Designated Router),	and again use the Router ID to break
//	    ties.

		nb = net->neighbor;
		while (nb != NULL) {
			if (nb->bdr_election_sum) {
				if (bdr_nb == NULL) {
					bdr_nb = nb;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
						"step 2: backup set to %d.%d.%d.%d", RCP_PRINT_IP(bdr_nb->ip));
				}
				else if (nb->bdr_election_sum > bdr_nb->bdr_election_sum) {
					bdr_nb = nb;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
						"step 2: backup break tie %d.%d.%d.%d", RCP_PRINT_IP(bdr_nb->ip));
				}
	 		}
	 		nb = nb->next;
		}
	}
	
	// bdr_nb can still be null in this moment
	if (bdr_nb != NULL) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"step 2: new backup router %d.%d.%d.%d", RCP_PRINT_IP(bdr_nb->ip));
	}
	
//	(3) Calculate the new Designated Router	for the	network	as
//	    follows.  If one or	more of	the routers have declared
//	    themselves Designated Router (i.e.,	they are currently
//	    listing themselves as Designated Router in their Hello
//	    Packets) the one having highest Router Priority is declared
//	    to be Designated Router.  In case of a tie,	the one	having
//	    the	highest	Router ID is chosen. 

	nb = net->neighbor;
	while (nb != NULL) {
		if (nb->dr_election_sum) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
				"step 3.1 list: %d.%d.%d.%d (dr %d.%d.%d.%d, backup %d.%d.%d.%d)",
				RCP_PRINT_IP(nb->ip),
				RCP_PRINT_IP(nb->designated_router),
				RCP_PRINT_IP(nb->backup_router));
 		}
 		nb = nb->next;
	}
	
	nb = net->neighbor;
	OspfNeighbor *dr_nb = NULL;
	while (nb != NULL) {
		if (nb->dr_election_sum) {
			if (nb->designated_router == nb->ip) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
					"step 3: declaring himself dr %d.%d.%d.%d", RCP_PRINT_IP(nb->ip));
				if (dr_nb == NULL) {
					dr_nb = nb;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
						"step 3: dr set to %d.%d.%d.%d", RCP_PRINT_IP(dr_nb->ip));
				}
				else if (nb->dr_election_sum > dr_nb->dr_election_sum) {
					dr_nb = nb;
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
						"step 3: dr break tie %d.%d.%d.%d", RCP_PRINT_IP(dr_nb->ip));
				}
			}
 		}
 		nb = nb->next;
	}
	
	
	
	if (dr_nb == NULL) {
//	                                                                    If no routers have
//	    declared themselves	Designated Router, assign the Designated
//	    Router to be the same as the newly elected Backup Designated
//	    Router.
		dr_nb = bdr_nb;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"step 3: dr assigned to backup");
	}

	// dr_nb can still be null in this moment
	if (dr_nb != NULL) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"step 3: new designated router %d.%d.%d.%d", RCP_PRINT_IP(dr_nb->ip));
	}
	
	net->dr_nb = dr_nb;
	net->bdr_nb = bdr_nb;
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
		"step 2&3 ended, dr %p, bdr %p", net->dr_nb, net->bdr_nb);
}

void drbdrElection(OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(net != 0);
	
//	The Designated Router election algorithm proceeds as follows:
//	Call the router	doing the calculation Router X.	 The list of
//	neighbors attached to the network and having established
//	bidirectional communication with Router	X is examined.	This
//	list is	precisely the collection of Router X's neighbors (on
//	this network) whose state is greater than or equal to 2-Way (see
//	Section	10.1).	Router X itself	is also	considered to be on the
//	list.  Discard all routers from	the list that are ineligible to
//	become Designated Router.  (Routers having Router Priority of 0
//	are ineligible to become Designated Router.)  The following
//	steps are then executed, considering only those	routers	that
//	remain on the list:


	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF_PKT,
		"DR/BDR election for network %d.%d.%d.%d, network state %d", RCP_PRINT_IP(net->ip), net->state);

	// if this router has a priority > 0 add it temporarily to the list
	OspfNeighbor *this_nb = NULL;
	if (net->router_priority > 0) {
		this_nb = malloc(sizeof(OspfNeighbor));
		if (this_nb != NULL) {
			memset(this_nb, 0, sizeof(OspfNeighbor));
			this_nb->state = NSTATE_2WAY;
			this_nb->priority = net->router_priority;
			this_nb->designated_router = net->designated_router;
			this_nb->backup_router = net->backup_router;
			this_nb->ip = net->ip;
			this_nb->router_id = shm->config.ospf_router_id;
			neighborAdd(net->area_id, this_nb);
		}
		else {
			printf("cannot allocate memory, restarting...\n");
			exit(1);
		}
	}

	// initialize list
	OspfNeighbor *nb = net->neighbor;
	while (nb != NULL) {
		// for each neighbor
		if (nb->state >= NSTATE_2WAY && nb->priority > 0) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
				"step 0 list: %d.%d.%d.%d (dr %d.%d.%d.%d, backup %d.%d.%d.%d)",
				RCP_PRINT_IP(nb->ip),
				RCP_PRINT_IP(nb->designated_router),
				RCP_PRINT_IP(nb->backup_router));
			nb->dr_election_sum = (((uint64_t) nb->priority) << 32) | ((uint64_t) nb->router_id);
			nb->bdr_election_sum = nb->dr_election_sum;
		}
		else {
			nb->dr_election_sum = 0;
			nb->bdr_election_sum = 0;
		}

		nb = nb->next;
	}

//
//	(1) Note the current values for	the network's Designated Router
//	    and	Backup Designated Router.  This	is used	later for
//	    comparison purposes.
//
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
		"original DR %d.%d.%d.%d", RCP_PRINT_IP(net->designated_router));
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
		"original BDR %d.%d.%d.%d", RCP_PRINT_IP(net->backup_router));

	// run steps 2 and 3
	step2and3(net);
rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
"here 1: dr %p, bdr %p", net->dr_nb, net->bdr_nb);
	
//	(4) If Router X	is now newly the Designated Router or newly the
//	    Backup Designated Router, or is now	no longer the Designated
//	    Router or no longer	the Backup Designated Router, repeat
//	    steps 2 and	3, and then proceed to step 5.	For example, if
//	    Router X is	now the	Designated Router, when	step 2 is
//	    repeated X will no longer be eligible for Backup Designated
//	    Router election.  Among other things, this will ensure that
//	    no router will declare itself both Backup Designated Router
//	    and	Designated Router.[5]
	if (this_nb != NULL && net->dr_nb != NULL && net->bdr_nb != NULL) {
rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
"here 2: dr %p, bdr %p", net->dr_nb, net->bdr_nb);
		// update X
		this_nb->designated_router = net->dr_nb->ip;
		this_nb->backup_router = net->bdr_nb->ip;
		
		if (net->dr_nb->ip == net->ip && net->designated_router != net->ip) // X is new dr router
			step2and3(net);
		else if (net->dr_nb->ip != net->ip && net->designated_router == net->ip) // X is not dr router anymore
			step2and3(net);
		else if (net->bdr_nb->ip == net->ip && net->backup_router != net->ip) // X is new bdr router
			step2and3(net);
		else if (net->bdr_nb->ip != net->ip && net->backup_router == net->ip) // X is not bdr router anymore
			step2and3(net);

//
//	(5) As a result	of these calculations, the router itself may now
//	    be Designated Router or Backup Designated Router.  See
//	    Sections 7.3 and 7.4 for the additional duties this	would
//	    entail.  The router's interface state should be set
//	    accordingly.  If the router	itself is now Designated Router,
//	    the	new interface state is DR.  If the router itself is now
//	    Backup Designated Router, the new interface	state is Backup.
//	    Otherwise, the new interface state is DR Other. - implemented at the end of this function
rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
"here 3: dr %p, bdr %p", net->dr_nb, net->bdr_nb);
	}
	

#if 0 // Not implementing NBMA
//	(6) If the attached network is an NBMA network,	and the	router
//	    itself has just become either Designated Router or Backup
//	    Designated Router, it must start sending Hello Packets to
//	    those neighbors that are not eligible to become Designated
//	    Router (see	Section	9.5.1).	 This is done by invoking the
//	    neighbor event Start for each neighbor having a Router
//	    Priority of	0.
#endif


//	(7) If the above calculations have caused the identity of either
//	    the	Designated Router or Backup Designated Router to change,
//	    the	set of adjacencies associated with this	interface will
//	    need to be modified.  Some adjacencies may need to be
//	    formed, and	others may need	to be broken.  To accomplish
//	    this, invoke the event AdjOK?  on all neighbors whose state
//	    is at least	2-Way.	This will cause	their eligibility for
//	    adjacency to be reexamined (see Sections 10.3 and 10.4).
	
	// unlink temporary neighbor from the neighbor list;
	// the neighbor's memory is still in use, however some of the functions
	//         below use the area/network/neighbors lists
	// the neighbor's memory is released at the end of the function
	if (this_nb)
		neighborUnlink(net->area_id, this_nb);
		




	int changed = 0;
	ASSERT(net->dr_nb);
	if (net->dr_nb != NULL && net->dr_nb->ip != net->designated_router) {
		net->designated_router = net->dr_nb->ip;
		
		changed = 1;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"network %d.%d.%d.%d designated router %d.%d.%d.%d",
			RCP_PRINT_IP(net->ip), RCP_PRINT_IP(net->designated_router));
	}
	if (net->bdr_nb != NULL && net->bdr_nb->ip != net->backup_router) {
		net->backup_router = net->bdr_nb->ip;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"network %d.%d.%d.%d backup router %d.%d.%d.%d",
			RCP_PRINT_IP(net->ip), RCP_PRINT_IP(net->backup_router));
		changed = 1;
	}
	else if (net->bdr_nb == NULL && net->backup_router != 0) {
		net->backup_router = 0;
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
			"network %d.%d.%d.%d backup router set to 0", RCP_PRINT_IP(net->ip));
		changed = 1;
	}

	// update network lsa if we are just about to become DR
	if (net->designated_router == net->ip) {
		OspfLsa *lsa = lsa_originate_net(net);
		if (lsa != NULL)
			lsadbAdd(net->area_id, lsa);
	}

	if (changed) {
		nb = net->neighbor;
		while (nb != NULL) {
			if (nb->state >= NSTATE_2WAY /* && nb->priority > 0*/) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_DRBDR,
					"sending AdjOK to neighbor %d.%d.%d.%d, neighbor state %d",
						RCP_PRINT_IP(nb->ip), nb->state);			
				nfsmAdjOK(net, nb);
			}
			nb = nb->next;
		}
	}
	
	// remove local neighbor
	if (this_nb)
		neighborFree(this_nb);
}

