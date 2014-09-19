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

int rxsock = 0;
int force_shutdown = 0;
uint32_t systic_delta_pkt = 0;
uint32_t systic_delta_integrity = 0;
int request_rip = 0;

extern void route_purge(void);
extern void lsa_integrity(void);


int main(int argc, char **argv) {
	
//rcpDebugEnable();
	// initialize shared memory
	rcp_init(RCP_PROC_OSPF);

	// initialize snmp traps
	trap_init();
	
	// initialize lsa hash table
	lsaInitHashTable();
	
	// initialize the receive packet
	RcpPkt *pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: process %s, cannot allocate memory,  exiting...\n", rcpGetProcName());
		exit(1);
	}

	// connect ospf protocol socket
	rxsock = rxConnect();
	if (rxsock == 0) {
		fprintf(stderr, "Error: process %s, cannot connect OSPF receive socket, exiting...\n", rcpGetProcName());
		return 1;
	}

	// receive loop
	int reconnect_timer = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	// use this timer to speed up rx loop when the system is busy
	uint32_t rcptic = rcpTic();

	// restart
	restart();

	// initialize md5 seq
	if (shm->config.ospf_md5_seq == 0)
		shm->config.ospf_md5_seq = (uint32_t) time(NULL);

	// forever
	while (1) {
		// reconnect service socket if necessary
		if (reconnect_timer >= 5) {
			ASSERT(muxsock == 0);
			muxsock = rcpConnectMux(RCP_PROC_OSPF, NULL, NULL, NULL);
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
		uint32_t current_tic = rcpTic(); 
		if (nready < 0) {
			fprintf(stderr, "Error: process %s, select nready %d, errno %d\n", rcpGetProcName(), nready, errno);
		}

		// select timeout
		else if (nready == 0 || current_tic > (rcptic + 2)) {
			shm->config.ospf_md5_seq++;
			
			// watchdog
			pstats->wproc++;

			// if reconnect timer is running, increment it
			if (reconnect_timer > 0)
				reconnect_timer++;

			// monitor interfaces
			ospf_update_interfaces();
			
			// run networks
			networkTimeout(current_tic > (rcptic + 2));
			
			// check redistribution counts
			redistTimeout();
			
			// if any network became DR, generate a network lsa
			if (network_lsa_update) {
				lsa_originate_net_update();
				network_lsa_update = 0;
			}
			
			
			// age lsa
			lsadbTimeout();
			
			// check spf
			spfTimeout();
			
			// integrity check
			if ((current_tic % CheckAge) == 0) {
				struct tms tm;
				uint32_t tic1 = times(&tm);
				
				lsa_integrity();
				route_purge();
				
				uint32_t tic2 = times(&tm);
				uint32_t delta;
				if (tic2 > tic1)
					delta = tic2 - tic1;
				else
					delta = tic1 - tic2;
				
				if (delta > systic_delta_pkt) {
					systic_delta_integrity = delta;
					rcpDebug("integrity delta %u\n", systic_delta_integrity);
				}


			}
			
			if (request_rip) {
				if (--request_rip == 0 && shm->config.ospf_redist_rip)
					rip_route_request();
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
			if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_OSPF) {
				processCli(pkt);
				// forward the packet back to rcp
				send(muxsock, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
			}
			
			// process rcp packets
			else {
				// interface address changed
				if (pkt->type == RCP_PKT_TYPE_IFCHANGED) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "interface %s address changed message received",
					            pkt->pkt.interface.interface->name);
					            
					redistConnectedDel(
						pkt->pkt.interface.old_ip & pkt->pkt.interface.old_mask,
						pkt->pkt.interface.old_mask,
						0);
				}
				
				// interface up packet
				else if (pkt->type == RCP_PKT_TYPE_IFUP) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "interface %s up message received",
					            pkt->pkt.interface.interface->name);

					// update area/network interfaces
					ospf_update_interfaces();
					
					// update connected redistribution	
					redistConnectedUpdate();		
					redistConnectedSubnetsUpdate();		
					
					// update static redistribution	
					redistStaticUpdate();		
				}
				
				// interface down packet
				else if (pkt->type == RCP_PKT_TYPE_IFDOWN) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "interface %s down message received",
					            pkt->pkt.interface.interface->name);					

					// update area/network interfaces
					ospf_update_interfaces();

					// update connected redistribution	
					redistConnectedUpdate();		
					redistConnectedSubnetsUpdate();		
					
					// update static redistribution	
					redistStaticUpdate();		
				}
				
				// loopback message
				else if (pkt->type == RCP_PKT_TYPE_LOOPBACK) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "loopback interface %d.%d.%d.%d/%d %s message received",
					            RCP_PRINT_IP(pkt->pkt.loopback.ip),
					            mask2bits(pkt->pkt.loopback.mask),
					            (pkt->pkt.loopback.state)? "up": "down");
					
					if (pkt->pkt.loopback.state)
						redistConnectedSubnetsUpdate();		
					else
						redistConnectedDel(pkt->pkt.loopback.ip & pkt->pkt.loopback.mask, pkt->pkt.loopback.mask, 0);
				}

				// add route packet
				else if (pkt->type == RCP_PKT_TYPE_ADDROUTE) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "add route %d.%d.%d.%d message received",
					            RCP_PRINT_IP(pkt->pkt.route.ip));
					RcpInterface *interface = rcpFindInterfaceByLPM(shm, pkt->pkt.route.gw);
					if (interface == NULL || interface->link_up == 0)
						continue;
					
					// adding a static route
					if (pkt->pkt.route.type == RCP_ROUTE_STATIC &&
					     shm->config.ospf_redist_static) {
					     	if (pkt->pkt.route.ip == 0 && pkt->pkt.route.mask == 0 && shm->config.ospf_redist_information == 0) {
							rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER, "default route not redistributed");
						}
						else {
							redistStaticAdd(pkt->pkt.route.ip,
								    pkt->pkt.route.mask,
								    pkt->pkt.route.gw,
								    interface);
						}
					}
					
					// adding a rip route
					else if (pkt->pkt.route.type == RCP_ROUTE_RIP &&
					     shm->config.ospf_redist_rip) {
					     	if (pkt->pkt.route.ip == 0 && pkt->pkt.route.mask == 0 && shm->config.ospf_redist_information == 0) {
							rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_ROUTER, "default route not redistributed");
						}
						else {
							redistRipAdd(pkt->pkt.route.ip,
								    pkt->pkt.route.mask,
								    pkt->pkt.route.gw,
								    interface);
						}
					}
				}
				
				// process route delete packets
				else if (pkt->type == RCP_PKT_TYPE_DELROUTE) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "delete route %d.%d.%d.%d message received",
					            RCP_PRINT_IP(pkt->pkt.route.ip));
					if (pkt->pkt.route.type == RCP_ROUTE_STATIC &&
					     shm->config.ospf_redist_static)
						redistStaticDel(pkt->pkt.route.ip,
							    pkt->pkt.route.mask,
							    pkt->pkt.route.gw);
					else if (pkt->pkt.route.type == RCP_ROUTE_RIP &&
					     shm->config.ospf_redist_rip)
						redistRipDel(pkt->pkt.route.ip,
							    pkt->pkt.route.mask,
							    pkt->pkt.route.gw);
				}
				
				// process route request packets
				else if (pkt->type == RCP_PKT_TYPE_ROUTE_RQ) {
					rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_IPC,
					            "route request message received");
				
					route_request();
				}
				else if (pkt->type == RCP_PKT_TYPE_VLAN_DELETED) {
					rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_IPC,
					            "vlan interface %d.%d.%d.%d/%d deleted",
					            RCP_PRINT_IP(pkt->pkt.vlan.interface->ip),
					            mask2bits(pkt->pkt.vlan.interface->mask));
					uint32_t ip = pkt->pkt.vlan.interface->ip;
					uint32_t mask = pkt->pkt.vlan.interface->mask;
					if (ip && mask) {
						// process network
						delete_vlan_network(ip & mask, mask);
						// update connected redistribution
						redistConnectedDel(ip & mask, mask, 0);
						// update static redistribution	
						redistStaticUpdate();
					}
				}
				else
					ASSERT(0);
			}
		}
		
		// process ospf packets
		else if (rxsock != 0 && FD_ISSET(rxsock, &fds)) {
			struct tms tm;
			uint32_t tic1 = times(&tm);
			
			rxpacket(rxsock);
			
			uint32_t tic2 = times(&tm);
			uint32_t delta;
			if (tic2 > tic1)
				delta = tic2 - tic1;
			else
				delta = tic1 - tic2;
			
			if (delta > systic_delta_pkt) {
				systic_delta_pkt = delta;
				rcpDebug("pkt delta %u, packet type %u, packet size %u\n",
					systic_delta_pkt, last_pkt_type, last_pkt_size);
			}
		}
				
		if (force_shutdown)
			break;
	} // while (1)
	
	// free local memory
	free(pkt);
	
	// free librcp memory
	cliRemoveFunctions();
	
	// close all sockets
	if (muxsock)
		close(muxsock);
	if (rxsock)
		close(rxsock);
	
	return 0;
}
