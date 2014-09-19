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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rip.h"

uint32_t trace_prefix = 0;

static RipNeighbor *neighbors = NULL;

RipNeighbor *rxneighbors(void) {
	return neighbors;
}

static void remove_neighbor(RipNeighbor *nb) {
	ASSERT(nb);

	if (nb == neighbors) {
		neighbors = nb->next;
		free(nb);
		return;
	}
	
	RipNeighbor *ptr = neighbors;
	while (ptr) {
		if (nb == ptr->next) {
			ptr->next = nb->next;
			free(nb);
			return;
		}
		ptr = ptr->next;
	}
}


int rxconnect(void) {
	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
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

	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons(RIP_PORT);
	s.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *)&s, sizeof(s)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	return sock;
}


// return 1 if error
int rxmultijoin(int sock, uint32_t ifaddress) {
//printf("rxmultijoin %d.%d.%d.%d\n", RCP_PRINT_IP(ifaddress));
	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(RIP_GROUP);
	mreq.imr_interface.s_addr = htonl(ifaddress);
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		ASSERT(0);
		return 1;
	}
	rcpLog(muxsock, RCP_PROC_RIP, RLOG_NOTICE, RLOG_FC_INTERFACE,
		"Interface %d.%d.%d.%d joined RIP multicast group",
		RCP_PRINT_IP(ifaddress));

	return 0;
}


int rxmultidrop(int sock, uint32_t ifaddress) {
//printf("rxmultidrop %d.%d.%d.%d\n", RCP_PRINT_IP(ifaddress));
	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(mreq));
	mreq.imr_multiaddr.s_addr = inet_addr(RIP_GROUP);
	mreq.imr_interface.s_addr = htonl(ifaddress);
	if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
		ASSERT(0);
		return 1;
	}
	rcpLog(muxsock, RCP_PROC_RIP, RLOG_NOTICE, RLOG_FC_INTERFACE,
		"Interface %d.%d.%d.%d dropped RIP multicast group",
		RCP_PRINT_IP(ifaddress));
	return 0;
}


void rxpacket(int sock) {
	Packet pkt;
	memset(&pkt, 0, sizeof(pkt));

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
	iov[0].iov_base = &pkt.data;
	iov[0].iov_len  = sizeof(pkt.data);
	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;

	struct cmsghdr *cmsg;
	unsigned int ifindex;			  // interface index
	struct in_addr hdraddr;			  // destination IP address in IP header
	size = recvmsg(sock, &msg, 0);
	if (size == -1) {
		ASSERT(0);
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_ERR, RLOG_FC_RIP,
			"cannot read data on socket, attempting recovery...");
		exit(1);
	}
	
	// verify packet size
	int sz = size - 4;
	if (sz<= 0 || (sz % sizeof(RipRoute)) != 0) {
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
			"Invalid RIP packet size");
		return;
	}
	int routes = sz / sizeof(RipRoute);
	
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
		
	pkt.ip_source = ntohl(from.sin_addr.s_addr);
	pkt.ip_dest = ntohl(hdraddr.s_addr);
	pkt.if_index = ifindex;

	// is the source ip address one of our addresses?
	RcpInterface *rxif  = rcpFindInterface(shm, pkt.ip_source);
	if (rxif) {
		// on Linux, packets we send to the multicast group will be received back on the socket
		return;
	}

	char *cmd[] = {"", "request", "response"};
	rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
		"Receiving RIP packet of size %d, from %d.%d.%d.%d, destination %d.%d.%d.%d, RIP %s, protocol version %d",
		size,
		RCP_PRINT_IP(pkt.ip_source),
		RCP_PRINT_IP(pkt.ip_dest),
		cmd[(pkt.data.command <= 2) ? pkt.data.command: 0],
		pkt.data.version);


	// update neighbor list
	RipNeighbor *neigh = neighbors;
	while (neigh != NULL) {
		if (neigh->ip == pkt.ip_source) {
			neigh->rx_time = 0;
			break;
		}
		neigh = neigh->next;
	}
	if (neigh == NULL) {
		RipNeighbor *newneigh = malloc(sizeof(RipNeighbor));
		if (newneigh != NULL) {
			memset(newneigh, 0, sizeof(RipNeighbor));
			newneigh->ip = pkt.ip_source;
			newneigh->next = neighbors;
			neighbors = newneigh;
			neigh = newneigh;
		}
		else {
			ASSERT(0);
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_ERR, RLOG_FC_RIP,
				"cannot allocate memory, attempting recovery...");
			exit(1);
		}
	}

	// do we have a valid interface?
	rxif =rcpFindInterfaceByKIndex(shm, pkt.if_index);
	if (rxif == NULL) {
		neigh->errors++;
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid interface, dropping...");
		return;
	}


	// do we have a configured neighbor?
	RcpRipPartner *rxnetwork = NULL;
	RcpRipPartner *net;
	int i;
	for (i = 0, net = shm->config.rip_neighbor; i < RCP_RIP_NEIGHBOR_LIMIT; i++, net++) {
		if (!net->valid)
			continue;

		// matching both source and destination addresses
		if (net->ip == pkt.ip_source && rxif->ip == pkt.ip_dest) {
			rxnetwork = net;
			break;
		}
	}
	
	// if no configured neighbor was found, try to find a configured network
	if (rxnetwork == NULL)
		rxnetwork = find_network_for_interface(rxif);
			
	// no network or neighbor configured, just drop the packet
	if (rxnetwork == NULL) {
		neigh->errors++;
		// the network can get disabled while receiving packets
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid network or neighbor, dropping...");
		return;
	}

	
	// the source of the datagram must be on a directly-connected network
	if ((pkt.ip_source & rxif->mask) != (rxif->ip & rxif->mask)) {
		neigh->errors++;
		// interface ip addresses are changing dynamically via CLI
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid source IP address, dropping...");
		return;
	}
	
	// drop invalid command packets
	if (pkt.data.command > 2) {
		neigh->errors++;
		rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid RIP command, dropping...");
		return;
	}
	
	if (pkt.data.command == 1) {
		rxnetwork->req_rx++;
		// force a response in one second
		rxif->rip_timeout = 1;
		return;
	}
	else
		rxnetwork->resp_rx++;

	ASSERT(sizeof(RipAuthMd5) == sizeof(RipAuthSimple));
	ASSERT(sizeof(RipAuthSimple) == sizeof(RipRoute));

	RipRoute *ptr = &pkt.data.routes[0];
	int rt = 0;
	
	// if md5 auth configured, and the packet is missing the auth header, drop the packet
	if (rxif->rip_passwd[0] != '\0') {
		if (ptr->family != 0xffff ||  ntohs(ptr->tag) != 3) {
			neigh->md5_errors++;		
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   missing MD5 authentication header");
			return;
		}
	}
	
	// checking auth header and calculate md5
	if (ptr->family == 0xffff) {
		// we don't care about simple auth
		if (ntohs(ptr->tag) == 3 && rxif->rip_passwd[0] != '\0') {
			RipAuthMd5 *md5 = (RipAuthMd5 *) ptr;
			uint16_t offset = ntohs(md5->offset);
			uint32_t seq = ntohl(md5->seq);
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   MD5 auth offset %u, key id %d, auth_len %d, seq %u",
				offset,
				md5->key_id,
				md5->auth_len,
				ntohl(md5->seq));

			// check offset
			if ((offset + sizeof(RipRoute)) != size) {
				neigh->md5_errors++;		
				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid offset");
				return;
			}

			// check seq
			if (seq != 0 && seq < neigh->auth_seq) {
				neigh->md5_errors++;		
				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid sequence number");
				return;
			}
			neigh->auth_seq = seq;
			
			// calculate md5
			uint8_t secret[16];
			memset(secret, 0, 16);
			memcpy(secret, rxif->rip_passwd, strlen(rxif->rip_passwd));
			
			MD5_CTX context;
			uint8_t digest[16];
			MD5Init (&context);
			MD5Update (&context, (uint8_t *) &pkt, size - 16);
			MD5Update (&context, secret, 16);
			MD5Final (digest, &context);
#if 0
{
int i;		
uint8_t *p = digest;
printf("rx digest:\n");
for (i = 0; i < 16; i++,p++)
	printf("%02x ", *p);
printf("\n");
}
#endif
			// compare md5
			if (memcmp((uint8_t *) ptr + offset, digest, 16) != 0) {
				neigh->md5_errors++;		
				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid MD5 digest");
				return;
			}	
		}
		ptr++;
		rt++;
		routes--;	// the last route is the digest
	}		
	
	// parsing routes
	while (rt < routes) {
		uint32_t metric = ntohl(ptr->metric);
		uint32_t mask = ntohl(ptr->mask);
		uint32_t ip = ntohl(ptr->ip);
		uint32_t gw = ntohl(ptr->gw);
		
//		if (trace_prefix == 0 || 
//		      (trace_prefix != 0 && trace_prefix == ip)) {
//		      	if (gw == 0)
//				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
//					"   %d.%d.%d.%d/%d metric %u",
//					RCP_PRINT_IP(ip),
//					mask2bits(mask),
//					metric);
//			else
//				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP,
//					"   %d.%d.%d.%d/%d metric %u next hop %d.%d.%d.%d",
//					RCP_PRINT_IP(ip),
//					mask2bits(mask),
//					metric,
//					RCP_PRINT_IP(gw));
//		}
		
		// only AF_INET family is supported
		if (ntohs(ptr->family) != AF_INET) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid route family");
			goto next_element;
		}			

		// check destination for loopback addresses
		if (isLoopback(ip)) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid loopback route prefix");
			goto next_element;
		}			
		
		// check destination for broadcast addresses
		if (isBroadcast(ip)) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid broadcast route prefix");
			goto next_element;
		}			
		
		// check destination for multicast addresses
		if (isMulticast(ip)) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid multicast route prefix");
			goto next_element;
		}			
		
		

		// validate route metric
		else if (metric > 16 || metric == 0) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid metric");
			goto next_element;
		}		
		
		// validate route entry
		if (ip == 0 && mask == 0) {
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   received default route metric %d",
				metric);
		}
		else if (pkt.data.version == 2 && mask == 0) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid RIP route");
			goto next_element;
		}		
		else if (pkt.data.version == 1 && mask != 0) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid RIP route");
			goto next_element;
		}		
		
		// check if the mask is contiguous
		if (mask != 0 && !maskContiguous(mask)) {
			neigh->route_errors++;
			rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid mask");
			goto next_element;
		}		
		
		// validate next hop
		if (gw) {
			if (isLoopback(gw) || isMulticast(gw) || isBroadcast(gw)) {
				neigh->route_errors++;
				rcpLog(muxsock, RCP_PROC_RIP, RLOG_DEBUG, RLOG_FC_RIP, "   invalid next hop");
				goto next_element;
			}
		}		
		
		// manufacture mask for rip v1
		if (pkt.data.version == 1)
			mask = classMask(ip);
			
		// RFC metric = metric + interface cost
		// we assume a cost of 1 for each interface
		metric++;
		
		// add the route in the database
		if (metric < 16) {
//RFC
//- Setting the destination address to the destination address in the
//     RTE
//
//   - Setting the metric to the newly calculated metric (as described 
//     above)
//
//   - Set the next hop address to be the address of the router from which
//     the datagram came
//
//   - Initialize the timeout for the route.  If the garbage-collection
//     timer is running for this route, stop it (see section 3.6 for a
//     discussion of the timers)
//
//   - Set the route change flag
//
//   - Signal the output process to trigger an update (see section 3.8.1)

			// a next hop of 0 means send the packets to me
			if (gw == 0)
				gw = pkt.ip_source;
			ripdb_add(RCP_ROUTE_RIP, ip, mask, gw, metric, pkt.ip_source, rxif);
		}
		else {
			// a next hop of 0 means send the packets to me
			if (gw == 0)
				gw = pkt.ip_source;
			ripdb_delete(RCP_ROUTE_RIP, ip, mask, gw, pkt.ip_source);
		}
next_element:
		ptr++;
		rt++;
	}
}

#define RIP_UPDATE_SHM_TIMEOUT 10
static int shm_update_timeout = 0;
void rxneigh_update(void) {
	// update local neighbor list
	RipNeighbor *neigh = neighbors;
	while (neigh != NULL) {
		RipNeighbor *next = neigh->next;
		if (++neigh->rx_time > RIP_ROUTE_TIMEOUT)
			remove_neighbor(neigh);
		neigh = next;
	}

	// update shm neighbor list
	if (--shm_update_timeout <= 0) {
		shm_update_timeout = RIP_UPDATE_SHM_TIMEOUT;

		// lock http
		shm->stats.rip_active_locked = 1;
		neigh = neighbors;
		int i = 0;
		while (neigh != NULL) {
			RcpActiveNeighbor *an = &shm->stats.rip_active[i];
			an->valid = 0;
			an->area = 0;
			an->router_id = 0;
			RcpInterface *intf = rcpFindInterfaceByLPM(shm, neigh->ip);
			if (intf) {
				an->network = intf->ip & intf->mask;
				an->netmask_cnt = mask2bits(intf->mask);
				an->if_ip = intf->ip;
				an->ip = neigh->ip;
				an->valid = 1;
				i++;
			}
			neigh = neigh->next;
		}
		
		// clear remaining active neighbors
		for (; i < RCP_ACTIVE_NEIGHBOR_LIMIT; i++)
			shm->stats.rip_active[i].valid = 0;
	}
	shm->stats.rip_active_locked = 0;
}
