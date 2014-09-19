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

//rfc2328
//    o	OSPF is	IP protocol number 89.	This number has	been registered
//	with the Network Information Center.  IP protocol number
//	assignments are	documented in [Ref11].
//
//    o	All OSPF routing protocol packets are sent using the normal
//	service	TOS value of binary 0000 defined in [Ref12].
//
//    o	Routing	protocol packets are sent with IP precedence set to
//	Internetwork Control.  OSPF protocol packets should be given
//	precedence over	regular	IP data	traffic, in both sending and
//	receiving.  Setting the	IP precedence field in the IP header to
//	Internetwork Control [Ref5] may	help implement this objective.

//A.2 The	Options	field
//
//    The	OSPF Options field is present in OSPF Hello packets, Database
//    Description	packets	and all	LSAs.  The Options field enables OSPF
//    routers to support (or not support)	optional capabilities, and to
//    communicate	their capability level to other	OSPF routers.  Through
//    this mechanism routers of differing	capabilities can be mixed within
//    an OSPF routing domain.
//
//    When used in Hello packets,	the Options field allows a router to
//    reject a neighbor because of a capability mismatch.	 Alternatively,
//    when capabilities are exchanged in Database	Description packets a
//    router can choose not to forward certain LSAs to a neighbor	because
//    of its reduced functionality.  Lastly, listing capabilities	in LSAs
//    allows routers to forward traffic around reduced functionality
//    routers, by	excluding them from parts of the routing table
//    calculation.
//
//    Five bits of the OSPF Options field	have been assigned, although
//    only one (the E-bit) is described completely by this memo. Each bit
//    is described briefly below.	Routers	should reset (i.e.  clear)
//    unrecognized bits in the Options field when	sending	Hello packets or
//    Database Description packets and when originating LSAs. Conversely,
//    routers encountering unrecognized Option bits in received Hello
//    Packets, Database Description packets or LSAs should ignore	the
//    capability and process the packet/LSA normally.
//
//		       +------------------------------------+
//		       | * | * | DC | EA | N/P | MC | E	| * |
//		       +------------------------------------+
//
//			     The Options field
//
//
//    E-bit
//	This bit describes the way AS-external-LSAs are	flooded, as
//	described in Sections 3.6, 9.5,	10.8 and 12.1.2	of this	memo.
//
//    MC-bit
//	This bit describes whether IP multicast	datagrams are forwarded
//	according to the specifications	in [Ref18].
//
//    N/P-bit
//	This bit describes the handling	of Type-7 LSAs,	as specified in
//	[Ref19].
//
//    EA-bit
//	This bit describes the router's	willingness to receive and
//	forward	External-Attributes-LSAs, as specified in [Ref20].
//
//    DC-bit
//	This bit describes the router's	handling of demand circuits, as
//	specified in [Ref21].
//
//

static int txconnect(void) {
	TRACE_FUNCTION();
	int sock;
	if ((sock = socket(AF_INET, SOCK_RAW, IP_PROT_OSPF)) < 0) {
		ASSERT(0);
		return 0;
	}

	// default multicast ttl is 1
	char tval = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,(char *)&tval, 1) < 0) {
		ASSERT(0);
		return 0;
	}

	int tos = IPTOS_PREC_INTERNETCONTROL;
	if (setsockopt(sock, IPPROTO_IP, IP_TOS, (char*)&tos, sizeof(tos)) < 0) {
		ASSERT(0);
		return 0;
	}

	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(char*)&on,sizeof(on)) < 0) {
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

	return sock;
}

// count tx packets
static void count_tx_pkts(OspfPacket *pkt, OspfNetwork *net) {
	RcpInterface *intf = rcpFindInterface(shm, net->ip);
	if (intf) {
		switch (pkt->header.type) {
			case 1: intf->stats.ospf_tx_hello++; break;
			case 2: intf->stats.ospf_tx_db++; break;
			case 3: intf->stats.ospf_tx_lsrq++; break;
			case 4: intf->stats.ospf_tx_lsup++; break;
			case 5: intf->stats.ospf_tx_lsack++; break;
			default:
				ASSERT(0);
		}
	}
}

static int txsock = 0;
void txpacket(OspfPacket *pkt, OspfNetwork *net) {
	TRACE_FUNCTION();
	ASSERT(pkt);
	ASSERT(net);
	
	// if this is a passive interface, do not send the packet
	RcpInterface *intf = rcpFindInterface(shm, net->ip);
	if (intf == NULL) {
//printf("network ip %d.%d.%d.%d\n", RCP_PRINT_IP(net->ip));
//		ASSERT(0);
		return;
	}
	if (intf->ospf_passive_interface)
		return;
	
	int sock = txsock;
	if (txsock == 0) {
		txsock = txconnect();
		sock = txsock;
	}
	
	if (sock == 0) {
		ASSERT(0);
		return;
	}

	struct sockaddr_in to;
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	struct sockaddr_in from;
	memset(&from, 0, sizeof(from));
	from.sin_family = AF_INET;

	// bind the socket to the outgoing interface ip address
	from.sin_addr.s_addr = htonl(net->ip);
	if (bind (sock, (struct sockaddr *) &from, sizeof(from)) != 0) {
		ASSERT(0);
		return;
	}

	// set destination address
	to.sin_addr.s_addr = htonl(AllSPFRouters_GROUP_IP);

	char *msg = (char *) &pkt->header;
	int len = ntohs(pkt->header.length);
	if (net->auth_type == 2)
		len += 16;
	if (sendto(sock, msg, len, 0, (struct sockaddr *) &to, sizeof(to)) < 0) {
		perror("sendto");
		ASSERT(0);
	}
	count_tx_pkts(pkt, net);
}

void txpacket_unicast(OspfPacket *pkt, OspfNetwork *net, uint32_t ip) {
	TRACE_FUNCTION();
	ASSERT(pkt);
	ASSERT(net);

	// if this is a passive interface, do not send the packet
	RcpInterface *intf = rcpFindInterface(shm, net->ip);
	if (intf == NULL) {
		ASSERT(0);
		return;
	}
	if (intf->ospf_passive_interface)
		return;

	int sock = txsock;
	if (txsock == 0) {
		txsock = txconnect();
		sock = txsock;
	}
	if (sock == 0) {
		ASSERT(0);
		return;
	}

	struct sockaddr_in to;
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	struct sockaddr_in from;
	memset(&from, 0, sizeof(from));
	from.sin_family = AF_INET;

	// bind the socket to the outgoing interface ip address
	from.sin_addr.s_addr = htonl(net->ip);
	if (bind (sock, (struct sockaddr *) &from, sizeof(from)) != 0) {
		// if somebody changes the ip address ospf running this code, bind will fail
		// the system will recover in a few seconds
		return;
	}

	// set destination address
	to.sin_addr.s_addr = htonl(ip);
	
	char *msg = (char *) &pkt->header;
	int len = ntohs(pkt->header.length);
	if (net->auth_type == 2)
		len += 16;
	if (sendto(sock, msg, len, 0, (struct sockaddr *) &to, sizeof(to)) < 0) {
		perror("sendto");
		ASSERT(0);
	}
	count_tx_pkts(pkt, net);
}
