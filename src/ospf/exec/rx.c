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
extern RcpPkt *pktout;

uint8_t last_pkt_type = 0;
uint32_t last_pkt_size = 0;

#define LOST_MAX_DD 125
#define LOST_MAX_LSRQ 25
#define LOST_MAX_LSU 75
#define LOST_MAX_LSACK 175
uint8_t lost_dd = 0;
uint8_t lost_rq = 0;
uint8_t lost_lsu = 0;
uint8_t lost_lsack = 0;

int cliDebugLostDD(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		lost_dd = 0;
	else
		lost_dd = 1;

	return 0;
}

int cliDebugLostLSAUpdate(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		lost_lsu = 0;
	else
		lost_lsu = 1;

	return 0;
}

int cliDebugLostLSAReq(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		lost_rq = 0;
	else
		lost_rq = 1;

	return 0;
}

int cliDebugLostLSAAck(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0)
		lost_lsack = 0;
	else
		lost_lsack = 1;

	return 0;
}



// connect OSPF socket; returns socket number or 0 if error
int rxConnect(void) {
	TRACE_FUNCTION();
	int sock;
	if ((sock = socket(AF_INET, SOCK_RAW, IP_PROT_OSPF)) < 0) {
		ASSERT(0);
		return 0;
	}

	//From ip(7) Linux man page
	//IP_PKTINFO
	//    Pass an IP_PKTINFO ancillary message that contains a pktinfo structure that supplies some
	//    information about the incoming packet. This only works for datagram oriented sockets. The argument
	//    is a flag that tells the socket whether the IP_PKTINFO message should be passed or not. The message itself
	//    can only be sent/retrieved as control message with a packet using recvmsg(2) or sendmsg(2).
	//
	//    struct in_pktinfo {
	//        unsigned int   ipi_ifindex;  /* Interface index */
	//        struct in_addr ipi_spec_dst; /* Local address */
	//        struct in_addr ipi_addr;     /* Header Destination
	//                                        address */
	//    };
	//
	//    ipi_ifindex is the unique index of the interface the packet was received on. ipi_spec_dst is the local address of
	//    the packet and ipi_addr is the destination address in the packet header. If IP_PKTINFO is passed to sendmsg(2)
	//    and ipi_spec_dst is not zero, then it is used as the local source address for the routing table lookup and for
	//    setting up IP source route options. When ipi_ifindex is not zero the primary local address of the interface specified
	//    by the index overwrites ipi_spec_dst for the routing table lookup.
	//
	//    Some other BSD sockets implementations provide IP_RCVDSTADDR and IP_RECVIF
	//    socket options to get the destination address and the interface of received datagrams.
	//    Linux has the more general IP_PKTINFO for the same task.
	int on = 1;
	if (setsockopt(sock, SOL_IP, IP_PKTINFO, &on, sizeof(on)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(char*)&on,sizeof(on)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}


	// set multicast TTL to 1
	char val = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&val, sizeof(val)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}
	
	// type of service
	int tos = IPTOS_PREC_INTERNETCONTROL;
	if (setsockopt(sock, IPPROTO_IP, IP_TOS, (char*)&tos, sizeof(tos)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	// increase rx and tx buffer sizes to 1MB
	int sock_buf_size = 1024 * 1024;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUFFORCE, (char *)&sock_buf_size, sizeof(sock_buf_size))) {
		ASSERT(0);
		close(sock);
		return 0;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUFFORCE, (char *)&sock_buf_size, sizeof(sock_buf_size))) {
		ASSERT(0);
		close(sock);
		return 0;
	}

#if 0
	// just checking	
	socklen_t opt_len = sizeof(sock_buf_size);
	sock_buf_size = 0;
	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&sock_buf_size, &opt_len);
	printf("rcvbuf size %d\n", sock_buf_size);

	sock_buf_size = 0;
	getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sock_buf_size, &opt_len);
	printf("sndbuf size %d\n", sock_buf_size);
#endif

	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	return sock;
}


// join AllSPFRouters router group; returns 1 if error, 0 if ok
int rxMultiJoin(int sock, uint32_t ifaddress, uint32_t group) {
	TRACE_FUNCTION();
	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = htonl(group); //inet_addr(AllSPFRouters_GROUP);
	mreq.imr_interface.s_addr = htonl(ifaddress);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		ASSERT(0);
		return 1;
	}
	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_NOTICE, RLOG_FC_INTERFACE,
		"Interface %d.%d.%d.%d joined OSPF multicast group %d.%d.%d.%d",
		RCP_PRINT_IP(ifaddress),
		RCP_PRINT_IP(group));

	return 0;
}


// leaves AllSPFRouters router group; returns 1 if error, 0 if ok
int rxMultiDrop(int sock, uint32_t ifaddress, uint32_t group) {
	TRACE_FUNCTION();
	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = htonl(group); //inet_addr(AllSPFRouters_GROUP);
	mreq.imr_interface.s_addr = htonl(ifaddress);
	if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		ASSERT(0);
		return 1;
	}

	rcpLog(muxsock, RCP_PROC_OSPF, RLOG_NOTICE, RLOG_FC_INTERFACE,
		"Interface %d.%d.%d.%d dropped OSPF multicast group %d.%d.%d.%d",
		RCP_PRINT_IP(ifaddress),
		RCP_PRINT_IP(group));
	return 0;
}

void rxMultiJoinNetwork(int sock, OspfNetwork *net) {
	int rv = rxMultiJoin(sock, net->ip, AllSPFRouters_GROUP_IP);
	if (rv) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_INTERFACE,
			"cannot join AllSPFRouters group for interface %d.%d.%d.%d\n",
			RCP_PRINT_IP(net->ip));
	}
	rv = rxMultiJoin(sock, net->ip, AllDRouters_GROUP_IP);
	if (rv) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_INTERFACE,
			"cannot join AllDRouters group for interface %d.%d.%d.%d",
			RCP_PRINT_IP(net->ip));
	}
}

void rxMultiDropNetwork(int sock, OspfNetwork *net) {
	int rv = rxMultiDrop(sock, net->ip, AllSPFRouters_GROUP_IP);
	if (rv) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_INTERFACE,
			"cannot drop AllSPFRouters group for neighbor %d.%d.%d.%d",
			RCP_PRINT_IP(net->ip));
	}
	rv = rxMultiDrop(sock, net->ip, AllDRouters_GROUP_IP);
	if (rv) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_INTERFACE,
			"cannot drop AllDRouters group for neighbor %d.%d.%d.%d\n",
			RCP_PRINT_IP(net->ip));
	}
}

void rxMultiJoinAll(int sock) {
	OspfArea *area = areaGetList();
	if (area == NULL) {
		return;
	}
	while (area != NULL) {
		OspfNetwork *net = area->network;
		if (net == NULL) {
			return;
		}
		while (net != NULL) {
			rxMultiJoinNetwork(sock, net);
			
			net = net->next;
		}
		
		area = area->next;
	}
}


void rxpacket(int sock) {
	TRACE_FUNCTION();
	OspfPacket pkt;
	OspfPacketDesc desc;
	memset(&pkt, 0, OSPF_PKT_HDR_LEN);
	memset(&desc, 0, sizeof(desc));

	// using recvmsg
	int size;				  // size of the received data
	struct msghdr msg;
	memset(&msg,   0, sizeof(msg));
	struct sockaddr_in from;
	int fromlen=sizeof(from);
	msg.msg_name = &from;
	msg.msg_namelen = fromlen;

	char anciliary[2048];
	msg.msg_control = anciliary;
	msg.msg_controllen = sizeof(anciliary);

	struct iovec   iov[1];
	memset(iov,    0, sizeof(iov));
	iov[0].iov_base = &pkt;
	iov[0].iov_len  = sizeof(pkt);
	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;

	struct cmsghdr *cmsg;
	unsigned int ifindex;			  // interface index
	struct in_addr hdraddr;			  // destination IP address in IP header
	size = recvmsg(sock, &msg, 0);
	if (size == -1) {
		ASSERT(0);
//		rcpLog(muxsock, RCP_PROC_RIP, RLOG_ERR, RLOG_FC_RIP,
//			"cannot read data on socket, attempting recovery...");
		exit(1);
	}
	desc.rx_size = size;
	
	int found = 0;
	for(cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
			//    struct in_pktinfo {
			//        unsigned int   ipi_ifindex;  /* Interface index */
			//        struct in_addr ipi_spec_dst; /* Local address */
			//        struct in_addr ipi_addr;     /* Header Destination
			//                                        address */
			//    };
			hdraddr = ((struct in_pktinfo*)CMSG_DATA(cmsg))->ipi_addr;
			ifindex = ((struct in_pktinfo*)CMSG_DATA(cmsg))->ipi_ifindex;
			found = 1;
		}
	}
	if (!found)
		return;
		
	desc.ip_source = ntohl(from.sin_addr.s_addr);
	desc.ip_dest = ntohl(hdraddr.s_addr);
	desc.if_index = ifindex;

//	o   The	IP protocol specified must be OSPF (89).
	if (pkt.ip_header.protocol != 89) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"OSPF packet from %d.%d.%d.%d, invalid protocol",
			RCP_PRINT_IP(desc.ip_source));
		return;
	}
	
	
//	o   Locally originated packets should not be passed on to OSPF.
//	    That is, the source	IP address should be examined to make
//	    sure this is not a multicast packet	that the router	itself
//	    generated.
	OspfArea *area = areaGetList();
	while (area != NULL) {
		OspfNetwork *net = area->network;
		while (net != NULL) {
			if (net->ip == desc.ip_source)
				return;
			net = net->next;
		}
		area = area->next;
	}
	// find area_id for this packet
	area = areaFindAnyIp(desc.ip_source);
	if (area)
		desc.area_id = area->area_id;
	else {
		trap_IfConfigError(desc.if_index, desc.ip_source, 2 /*areaMismatch*/, pkt.header.type);
		return;
	}
	
	// doing authentication before checksum
	if (auth_rx(&pkt, &desc)) {	
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"OSPF packet from %d.%d.%d.%d, invalid authentication",
			RCP_PRINT_IP(desc.ip_source));
		return;
	}


//	o   The	IP checksum must be correct.
	if (ntohs(pkt.header.auth_type) != 2) {
		int rv = validate_checksum((uint16_t *) &pkt.header, ntohs(pkt.header.length));
		if (rv == 1)
			;
		else {
			trap_IfRxBadPacket(desc.if_index, desc.ip_source, pkt.header.type);
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"OSPF packet from %d.%d.%d.%d, invalid checksum",
				RCP_PRINT_IP(desc.ip_source));
			return;
		}
	}

#if 0
//	o   The	packet's IP destination	address	must be	the IP address
//	    of the receiving interface,	or one of the IP multicast
//	    addresses AllSPFRouters or AllDRouters.
	if (pkt.ip_dest != ifaddress &&
	     pkt.ip_dest != AllSPFRouters_GROUP_IP &&
	     pkt.ip_dest != AllDRouters_GROUP_IP) {
	     	printf("   invalid destination address\n");
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_INFO, RLOG_FC_OSPF,
			"OSPF packet from %d.%d.%d.%d, invalid destination address",
			RCP_PRINT_IP(desc.ip_source));
	     	return;
	}
#endif

	// verify header
	if (headerExtract(&pkt, &desc)) {
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"OSPF packet from %d.%d.%d.%d, invalid header",
			RCP_PRINT_IP(desc.ip_source));
		return;
	}

	last_pkt_type = pkt.header.type;
	last_pkt_size = size;
	
	// check for duplicate router id
	if (desc.router_id == shm->config.ospf_router_id) {
		trap_IfConfigError(desc.if_index, desc.ip_source, 12 /*duplicateRouterId*/, pkt.header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"OSPF packet from %d.%d.%d.%d, duplicate router ID",
			RCP_PRINT_IP(desc.ip_source));
		return;
	}

	if (pkt.header.type > 5) {
		trap_IfRxBadPacket(desc.if_index, desc.ip_source, pkt.header.type);
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"OSPF packet from %d.%d.%d.%d, unsupported OSPF packet type %u",
			RCP_PRINT_IP(desc.ip_source), pkt.header.type);
		return;
	}
	char *pkt_type[] = {
		"",
		"Hello",
		"DB Description",
		"LS Request",
		"LS Update",
		"LS Acknowledge"
	};
	char *pkt_type_str = pkt_type[pkt.header.type];


	if (pkt.header.type == 1) 
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
			"%s packet from %d.%d.%d.%d, router id %d.%d.%d.%d, area %d",
			pkt_type_str,
			RCP_PRINT_IP(desc.ip_source),
			RCP_PRINT_IP(ntohl(pkt.header.router_id)),
			ntohl(pkt.header.area));
	else
		rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
			"%s packet from %d.%d.%d.%d, router id %d.%d.%d.%d, area %d",
			pkt_type_str,
			RCP_PRINT_IP(desc.ip_source),
			RCP_PRINT_IP(ntohl(pkt.header.router_id)),
			ntohl(pkt.header.area));

	// find the network in shared memory
	RcpInterface *intf = NULL;
	OspfNetwork *net = networkFindIp(desc.area_id, desc.ip_source);	// using the source of the packet
	if (net) {
		intf = rcpFindInterface(shm, net->ip);
		if (intf && intf->ospf_passive_interface) {
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT | RLOG_FC_OSPF_LSA,
				"packet dropped on passive interface");
			return;
		}
	}
				

	if (pkt.header.type == 1) {
		// hello
		if (intf)
			intf->stats.ospf_rx_hello++;
		
		if (helloRx(&pkt, &desc)) {
			if (intf)
				intf->stats.ospf_rx_hello_errors++;

			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"invalid packet");
			return;
		}
		return;
	}
	else if  (pkt.header.type == 2) {
		if (lost_dd) {
			int r = random();
			if ((r % LOST_MAX_DD) == 0) {
				printf("Lost DD packet\n");
				return;
			}
		}

		// db description
		if (intf)
			intf->stats.ospf_rx_db++;
		
		if (dbdescRx(&pkt, &desc)) {
			if (intf)
				intf->stats.ospf_rx_db_errors++;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"invalid packet");
			return;
		}
		return;
	}
	else if  (pkt.header.type == 3) {
		if (lost_rq) {
			int r = random();
			if ((r % LOST_MAX_LSRQ) == 0) {
				printf("Lost LSRequest packet\n");
				return;
			}
		}

		// ls request
		if (intf)
			intf->stats.ospf_rx_lsrq++;
		
		if (lsrequestRx(&pkt, &desc)) {
			if (intf)
				intf->stats.ospf_rx_lsrq_errors++;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"invalid packet");
			return;
		}
		return;
	}
	else if  (pkt.header.type == 4) {
		if (lost_lsu) {
			int r = random();
			if ((r % LOST_MAX_LSU) == 0) {
				printf("Lost LSUpdate packet\n");
				return;
			}
		}

		// ls update
		if (intf)
			intf->stats.ospf_rx_lsup++;
		
		if (lsupdateRx(&pkt, &desc)) {
			if (intf)
				intf->stats.ospf_rx_lsup_errors++;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"invalid packet");
			return;
		}
		return;
	}
	else if  (pkt.header.type == 5) {
		if (lost_lsack) {
			int r = random();
			if ((r % LOST_MAX_LSACK) == 0) {
				printf("Lost LSAck packet\n");
				return;
			}
		}

		// lsack
		if (intf)
			intf->stats.ospf_rx_lsack++;
		
		if (lsackRx(&pkt, &desc)) {
			if (intf)
				intf->stats.ospf_rx_lsack_errors++;
			rcpLog(muxsock, RCP_PROC_OSPF, RLOG_DEBUG, RLOG_FC_OSPF_PKT,
				"invalid packet");
			return;
		}
		return;
	}
}
