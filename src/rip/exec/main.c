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
#include "rip.h"

int rxsock = 0;
int static_update_trigger = 0;
int update_redist = 60;


int main(int argc, char **argv) {
//rcpDebugEnable();
	// initialize shared memory
	rcp_init(RCP_PROC_RIP);

	// initialize the receive packet
	RcpPkt *pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory,  exiting...\n", rcpGetProcName());
		exit(1);
	}

	// cleanup - should be run before creating RIP socket
	restart();

	// connect rip protocol socket
	rxsock = rxconnect();
	if (rxsock == 0) {
		fprintf(stderr, "Error: process %s, cannot connect RIP receive socket, exiting...\n", rcpGetProcName());
		return 1;
	}


	// receive loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	// forever
	while (1) {
		// reconnect service socket if necessary
		if (reconnect_timer >= 5) {
			ASSERT(muxsock == 0);
			muxsock = rcpConnectMux(RCP_PROC_RIP, NULL, NULL, NULL);
			if (muxsock == 0)
				reconnect_timer = 1; // start reconnect timer
			else {
				reconnect_timer = 0;
				pstats->mux_reconnect++;
			}
		}

		// select setup
		fd_set fds;
		FD_ZERO(&fds);
		if (muxsock != 0)
			FD_SET(muxsock, &fds);
		FD_SET(rxsock, &fds);
		int maxfd = (muxsock > rxsock)? muxsock: rxsock;

		// wait for data
		errno = 0;
		int nready = select(maxfd + 1, &fds, (fd_set *) 0, (fd_set *) 0, &ts);
		if (nready < 0) {
			fprintf(stderr, "Error: process %s, select nready %d, errno %d\n", rcpGetProcName(), nready, errno);
		}

		// select timeout
		else if (nready == 0) {
			// watchdog
			pstats->wproc++;

			// if reconnect timer is running, increment it
			if (reconnect_timer > 0)
				reconnect_timer++;
			
			// process interface/configured network changes
			rip_update_interfaces();				
			
			// route timeout
			ripdb_timeout();

			// check for route updates and send updates to neighbors
			 if (ripdb_update_available) {
			 	rip_send_delta_updates();
			 	ripdb_clean_updates();
			 }
			 
			// 30 seconds protocol hello sent to neighbors
			rip_send_timeout_updates();
				
			// update neighbor list
			rxneigh_update();

			// triggers static updates
			if (static_update_trigger) {
				if (--static_update_trigger == 0)
					rip_update_static();
			}
			
			// request ospf routes
			if (update_redist) {
				if (--update_redist == 0) {
					if (shm->config.rip_redist_ospf)
						route_request();
					rip_update_connected();
					rip_update_connected_subnets();
					rip_update_static();
					
					// force another update in 3 minutes
					update_redist = 180;
				}
			}
				
			// reload ts
			rcptic++;
			if (rcpTic() > rcptic) {
				// speed up the clock
				ts.tv_sec = 0;
				ts.tv_usec = 800000;	// 0.8 seconds
				pstats->select_speedup++;
			}
			else {
				ts.tv_sec = 1; // 1 second
				ts.tv_usec = 0;
			}
		}

		// process data coming in on service socket
		else if (muxsock != 0 && FD_ISSET(muxsock, &fds)) {
			errno = 0;
			int nread = recv(muxsock, pkt, sizeof(RcpPkt), 0);
			if(nread < sizeof(RcpPkt) || errno != 0) {
//				fprintf(stderr, "Error: process %s, muxsocket nread %d, errno %d, disconnecting...\n",
//					rcpGetProcName(), nread, errno);
				close(muxsock);
				reconnect_timer = 1;
				muxsock = 0;
				continue;
			}

			// read the packet data
			if (pkt->data_len != 0) {
				nread += recv(muxsock, (unsigned char *) pkt + sizeof(RcpPkt), pkt->data_len, 0);
			}
			ASSERT(nread == sizeof(RcpPkt) + pkt->data_len);

			// process cli packets
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_RIP) {
				processCli(pkt);
				// forward the packet back to rcp
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			
			// process rcp packets
			else {
				// interface up packet
				if (pkt->type == RCP_PKT_TYPE_IFUP) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "interface %s up message received",
					            pkt->pkt.interface.interface->name);					
					rip_update_interfaces();
					rip_update_connected();
					rip_update_connected_subnets();
					rip_update_static();
				}
				
				// interface down packet
				else if (pkt->type == RCP_PKT_TYPE_IFDOWN) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "interface %s down message received",
					            pkt->pkt.interface.interface->name);					
					ripdb_delete_interface(pkt->pkt.interface.interface);
					rip_update_interfaces();
					rip_update_connected();
					rip_update_connected_subnets();
					rip_update_static();
				}
				
				// loopback message
				else if (pkt->type == RCP_PKT_TYPE_LOOPBACK) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "loopback interface %d.%d.%d.%d/%d %s message received",
					            RCP_PRINT_IP(pkt->pkt.loopback.ip),
					            mask2bits(pkt->pkt.loopback.mask),
					            (pkt->pkt.loopback.state)? "up": "down");
					            
					if (pkt->pkt.loopback.state)
					            rip_update_connected_subnets();
					else {
						rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER,
						            "redistributing connected route delete %d.%d.%d.%d/%d",
						            RCP_PRINT_IP(pkt->pkt.loopback.ip & pkt->pkt.loopback.mask),
						            mask2bits(pkt->pkt.loopback.mask));
						ripdb_delete(RCP_ROUTE_CONNECTED_SUBNETS,
							    pkt->pkt.loopback.ip & pkt->pkt.loopback.mask,
							    pkt->pkt.loopback.mask,
							    0,
							    0);
					}
				}
				
				// add route packet
				else if (pkt->type == RCP_PKT_TYPE_ADDROUTE) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "add route %d.%d.%d.%d message received",
					            RCP_PRINT_IP(pkt->pkt.route.ip));
					RcpInterface *interface = rcpFindInterfaceByLPM(shm, pkt->pkt.route.gw);
					if (interface == NULL || interface->link_up == 0)
						continue;
					
					// adding a static route
					if ((pkt->pkt.route.type == RCP_ROUTE_STATIC && shm->config.rip_redist_static) ||
					    (pkt->pkt.route.type == RCP_ROUTE_OSPF && shm->config.rip_redist_ospf)) {
					     	if (pkt->pkt.route.ip == 0 && pkt->pkt.route.mask == 0 && shm->config.rip_default_information == 0) {
							rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
								"default route not redistributed");
						}
						else {
							uint8_t metric = 1;
							if (pkt->pkt.route.type == RCP_ROUTE_STATIC)
								metric = shm->config.rip_redist_static;
							else if (pkt->pkt.route.type == RCP_ROUTE_OSPF)
								metric = shm->config.rip_redist_ospf;
							ripdb_add(pkt->pkt.route.type,
								    pkt->pkt.route.ip,
								    pkt->pkt.route.mask,
								    pkt->pkt.route.gw,
								    metric,
								    0,
								    interface);
							rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER,
								"redistributing route add %d.%d.%d.%d/%d via %d.%d.%d.%d",
								RCP_PRINT_IP(pkt->pkt.route.ip),
								mask2bits(pkt->pkt.route.mask),
								RCP_PRINT_IP(pkt->pkt.route.gw));
						}
					}
				}
				
				// process route delete packets
				else if (pkt->type == RCP_PKT_TYPE_DELROUTE) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "delete route %d.%d.%d.%d message received",
					            RCP_PRINT_IP(pkt->pkt.route.ip));
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_ROUTER,
					            "redistributing route delete %d.%d.%d.%d via %d.%d.%d.%d",
					            RCP_PRINT_IP(pkt->pkt.route.ip),
					            RCP_PRINT_IP(pkt->pkt.route.gw));
					ripdb_delete(pkt->pkt.route.type,
						    pkt->pkt.route.ip,
						    pkt->pkt.route.mask,
						    pkt->pkt.route.gw,
						    0);
				}
				
				// process route request packets
				else if (pkt->type == RCP_PKT_TYPE_ROUTE_RQ) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "route request message received");
				
					ripdb_route_request();
				}
				else if (pkt->type == RCP_PKT_TYPE_VLAN_DELETED) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "vlan interface %d.%d.%d.%d/%d deleted",
					            RCP_PRINT_IP(pkt->pkt.vlan.interface->ip),
					            mask2bits(pkt->pkt.vlan.interface->mask));
					ripdb_delete_interface(pkt->pkt.vlan.interface);
					rip_update_interfaces();
					rip_update_connected();
					rip_update_connected_subnets();
					rip_update_static();
				}
				else
					ASSERT(0);
			}
		}
		
		// process rip packets
		else if (rxsock != 0 && FD_ISSET(rxsock, &fds))
			rxpacket(rxsock);
	} // while (1)
	
	// it should never get here
	return 0;
}
