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
#include "dhcp.h"

//*********************************************************************************
// misc dhcp processing
//*********************************************************************************
// return 1 if bad packet
static int chk_options(Packet *pkt) {
	unsigned char *ptr = pkt->dhcp.options;
	int len;

	pkt->pkt_type = NULL;
	pkt->pkt_82 = NULL;
	pkt->pkt_end = NULL;
	pkt->pkt_len = 0;
	pkt->server_id = 0;
	pkt->lease_time = 0;
	pkt->subnet_mask = 0;

	// walk the packet byte by byte and extract option data
	while (*ptr != 0xff && *ptr != '\0') {
		if (*ptr == 53)	// DHCP message type
			pkt->pkt_type = ptr + 2;
		else if (*ptr == 82) // Relay Agent Information Option
			pkt->pkt_82 = ptr;
		else if (*ptr == 54) { // Server ID
			memcpy(&pkt->server_id, ptr + 2, 4);
			pkt->server_id = ntohl(pkt->server_id);
		}			
		else if (*ptr == 51) { // Lease Time
			memcpy(&pkt->lease_time, ptr + 2, 4);
			pkt->lease_time = ntohl(pkt->lease_time);
		}			
		else if (*ptr == 1) { // Subnet Mask
			memcpy(&pkt->subnet_mask, ptr + 2, 4);
			pkt->subnet_mask = ntohl(pkt->subnet_mask);
		}			
		else if (*ptr == 50) { // Requested IP Address
			memcpy(&pkt->requested_ip, ptr + 2, 4);
			pkt->requested_ip = ntohl(pkt->requested_ip);
		}			

		len = *(++ptr);
		ptr += len + 1;
	}

	// the packet should end in 0xff
	if (*ptr != 0xff)
		return 1;
	
	// mark the end of the packet
	pkt->pkt_end = ptr;
	ptr++;
	
	// calculate the packet length
	pkt->pkt_len = (unsigned) ((unsigned char *) ptr - (unsigned char *) &pkt->dhcp);

	return 0;
}


static void print_options(Packet *pkt, char *buf) {
	*buf = '\0';
	if (pkt->subnet_mask != 0) {
		sprintf(buf,
		                "subnet mask %d.%d.%d.%d, ", RCP_PRINT_IP(pkt->subnet_mask));
		buf += strlen(buf);
	}
	if (pkt->server_id != 0) {
		sprintf(buf,
		                "server ID %d.%d.%d.%d, ", RCP_PRINT_IP(pkt->server_id));
		buf += strlen(buf);
	}
	if (pkt->lease_time != 0) {
		sprintf(buf,
			"lease time %u", pkt->lease_time);
		buf += strlen(buf);
	}
}

//*********************************************************************************
// rx socket - a udp socket bound to dhcp server port number
//   - the socket is used for rx and tx
//   - dhcp packet coming in, redirected to rxpacket()
//   - the code will extract the interface where the packet was received
//*********************************************************************************
// return socket number or 0 if error
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

	// allow reuse of local addresses in the next bind call
	on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,(char*)&on,sizeof(on)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	//pick up any UDP packet for port 67, regardless on what interface is coming
	// IP_PKTINFO above allows us to extract interface information
	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons(DHCP_SERVER_PORT);
	s.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *)&s, sizeof(s)) < 0) {
		ASSERT(0);
		close(sock);
		return 0;
	}

	// set the socket to non-blocking
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
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
	return sock;
}



// receive data and process dhcp packet
void rxpacket(int sock, int icmp_sock) {
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
	iov[0].iov_base = &pkt.dhcp;
	iov[0].iov_len  = sizeof(pkt.dhcp);
	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;

	struct cmsghdr *cmsg;
	int ifindex;	// interface index
	struct in_addr hdraddr;	// destination IP address in IP header
	size = recvmsg(sock, &msg, 0);
	if (size == -1) {
		ASSERT(0);
		rcpLog(muxsock, RCP_PROC_DHCP, RLOG_ERR, RLOG_FC_DHCP,
		               "cannot read socket");
		// shutdown
		exit(1);
	}

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

	// filter by interface
	RcpInterface *pif = rcpFindInterfaceByKIndex(shm, pkt.if_index);
	if (pif == NULL || pif->link_up == 0 || pif->admin_up == 0) {
		// dropping the packet
		rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
		               "DHCP packet dropped, not a valid interface");
		return;
	}
	pif->stats.dhcp_rx++;

	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
	               "Receiving DHCP paket of size %d on interface %s, source %d.%d.%d.%d, destination %d.%d.%d.%d",
		size,
		pif->name,
		RCP_PRINT_IP(pkt.ip_source),
		RCP_PRINT_IP(pkt.ip_dest));

	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
	               "  op %d, hops %d, tid %x, secs %d, ciaddr %d.%d.%d.%d, yiaddr %d.%d.%d.%d, siaddr %d.%d.%d.%d, giaddr %d.%d.%d.%d, chaddr: %02X:%02X:%02X:%02X:%02X:%02X",
		pkt.dhcp.op,
		pkt.dhcp.hops,
		ntohl(pkt.dhcp.tid),
		ntohs(pkt.dhcp.secs),
		RCP_PRINT_IP(ntohl(pkt.dhcp.ciaddr)),
		RCP_PRINT_IP(ntohl(pkt.dhcp.yiaddr)),
		RCP_PRINT_IP(ntohl(pkt.dhcp.siaddr)),
		RCP_PRINT_IP(ntohl(pkt.dhcp.giaddr)),
		RCP_PRINT_MAC(pkt.dhcp.chaddr));

	// maxhopos checking for realy only
	if (pif->dhcp_relay) {
		if (pkt.dhcp.hops > shm->config.dhcpr_max_hops) {
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			              "maximum number of hops exceeded, packet dropped");
			pif->stats.dhcp_err_rx++;
			pif->stats.dhcp_err_hops++;
			return;
		}
	}

	// check packet integrity
	if (chk_options(&pkt) || pkt.pkt_type == NULL) {
		rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
		              "malformed DHCP packet, packet dropped");
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_pkt++;
		return;
	}

	// print options
	char buf[1024 * 10];
	print_options(&pkt, buf);
	switch (*pkt.pkt_type) {
		//RFC2153:
		case 1:
			pif->stats.dhcp_rx_discover++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			               "DHCPDISCOVER, %s", buf);
			goto forward_server;
			break;
		case 2:
			pif->stats.dhcp_rx_offer++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			               "  DHCPOFFER, %s", buf);
			goto forward_client;
			break;
		case 3:
			pif->stats.dhcp_rx_request++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			                "  DHCPREQUEST, %s", buf);
			goto forward_server;
			break;
		case 4:
			pif->stats.dhcp_rx_decline++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			               "  DHCPDECLINE, %s", buf);
			goto forward_server;
			break;
		case 5:
			pif->stats.dhcp_rx_ack++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			               "  DHCPACK, %s", buf);
			goto forward_client;
			break;
		case 6:
			pif->stats.dhcp_rx_nack++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			                "  DHCPNACK, %s", buf);
			goto forward_client;
			break;
		case 7:
			pif->stats.dhcp_rx_release++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			                "  DHCPRELEASE, %s", buf);
			goto forward_server;
			break;
		case 8:
			pif->stats.dhcp_rx_inform++;
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			               "  DHCPINFORM, %s", buf);
			goto forward_server;
			break;
		default:
			ASSERT(0);
			return;
	}
	return;

// forwarding packets to server
forward_server:
	{
		if (pif->dhcp_relay == 0) {
			if (shm->config.dhcps_enable)
				return dhcp_rx_server(sock, icmp_sock, &pkt, pif);
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			                "DHCP relay feature not configured for interface %s, packet dropped", pif->name);
			pif->stats.dhcp_err_rx++;
			pif->stats.dhcp_err_not_configured++;
			return;
		}
		
		{ // drop the packet if no server is configured
			int cnt = 0;
			int i;
			RcpDhcprServer *ptr;

			for (i = 0, ptr = shm->config.dhcpr_server; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++, ptr++) {
				if (!ptr->valid)
					continue;
				cnt++;
			}
			
			if (cnt == 0) {
				rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
				                "no DHCP server configured, packet dropped");
				pif->stats.dhcp_err_rx++;
				pif->stats.dhcp_err_server_notconf++;
				return;
			}
		}


// RFC 3046
// 2.1.1 Reforwarded DHCP requests
// 
// 
//    A DHCP relay agent may receive a client DHCP packet forwarded from a
//    BOOTP/DHCP relay agent closer to the client.  Such a packet will have
//    giaddr as non-zero, and may or may not already have a DHCP Relay
//    Agent option in it.
// 
//    Relay agents configured to add a Relay Agent option which receive a
//    client DHCP packet with a nonzero giaddr SHALL discard the packet if
//    the giaddr spoofs a giaddr address implemented by the local agent
//    itself.
// 
//    Otherwise, the relay agent SHALL forward any received DHCP packet
//    with a valid non-zero giaddr WITHOUT adding any relay agent options.
//    Per RFC 2131, it shall also NOT modify the giaddr value.
// 
		if (pkt.dhcp.giaddr == 0) {
			// if option82 is present drop the packet
			if (pkt.pkt_82 != NULL) {
				rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
				                "unexpected option 82 present, packet dropped");
				pif->stats.dhcp_err_rx++;
				pif->stats.dhcp_err_pkt++;
				return;
			}
			
			// set giaddr field
			pkt.dhcp.giaddr = htonl(pif->ip);
		}
		else { // we have another dhcp relay upstream; we are not allowed to modify giaddr

			// spoofing: check if giaddr is ours and drop the packet 
			uint32_t giaddr = ntohl(pkt.dhcp.giaddr);
			if (rcpFindInterface(shm, giaddr)) {
				rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
				                "invalid interface, packet dropped");
				// somebody is spoofing our addresses
				pif->stats.dhcp_err_rx++;
				pif->stats.dhcp_err_giaddr++;
				return;
			}
		}

		// add option 82 if configured and none already present
		if (shm->config.dhcpr_option82 && pkt.pkt_82 == NULL) {
			// add option 82
			int len_host = strlen(shm->config.hostname);
			int len_if = strlen(pif->name);
			int len_option = len_host + 1 +len_if;
			
			// check if we have enough space to add the option
			if ( (pkt.pkt_len + len_option) < MAX_DHCP_PKT_SIZE) {
				// populate option 82
				*pkt.pkt_end++ = 82;
				*pkt.pkt_end++ = len_option + 2;
				*pkt.pkt_end++ = 1;  // suboption 1
				*pkt.pkt_end++ = len_option;
	
				// add hostname
				memcpy(pkt.pkt_end, shm->config.hostname, len_host);
				pkt.pkt_end += len_host;
	
				// add separator
				*pkt.pkt_end++ = '.';
	
				// add interface name			
				memcpy(pkt.pkt_end, pif->name, len_if);
				pkt.pkt_end += len_if;
	
				// terminate the packet
				*pkt.pkt_end = 0xff;
	
				// set the new length
				pkt.pkt_len += len_option + 4;
			}
		}

		// increment hops field
		pkt.dhcp.hops++;
	
		// figure out where the packet is going
		if (pkt.server_id != 0) {
			if (check_server(pkt.server_id) == 0) {
				// it looks like we have DHCP relay IP addresses in configuration, not real DHCP servers
				// we are part of a chain of DHCP relays!
				
				// log a warning and still froward the packet
				pif->stats.dhcp_warn_server_unknown++;
			}
			txpacket(sock, &pkt, pkt.server_id);
			return;
		}

		int secs = ntohs(pkt.dhcp.secs);
		if (secs >= shm->config.dhcpr_service_delay) {
			// send the packet to all servers
			txpacket_all(sock, &pkt);
			return;
		}

		txpacket_random_balancing(sock, &pkt);
		return;
	}

// forwarding packets to clients	
forward_client:
	{
		// find outgoing interface
		RcpInterface *ifout = rcpFindInterfaceByLPM(shm, ntohl(pkt.dhcp.yiaddr));
		if (ifout == NULL || ifout->dhcp_relay == 0) {
			// It means we are not the first relay in the chain; if we got here,
			// there is a misconfigured relay in the chain, replacing
			// the original giaddr with its own address.
			
			// drop the packet		
			rcpLog(muxsock, RCP_PROC_DHCP, RLOG_DEBUG, RLOG_FC_DHCP,
			                "cannot find outgoing DHCP interface for %d.%d.%d.%d, packet dropped",
			                RCP_PRINT_IP(ntohl(pkt.dhcp.yiaddr)));
			pif->stats.dhcp_err_rx++;
			pif->stats.dhcp_err_outgoing_intf++;
			return;
		}
		// this is a broadcast packet going to a client - we just made sure of it in the code above!
		
		// remove option 82 if any
		if (pkt.pkt_82 != NULL) {
			// no checking, just get rid of it		
			pkt.pkt_len -= *(pkt.pkt_82 + 1);
			*pkt.pkt_82 = 0xff;
			pkt.pkt_end = pkt.pkt_82;
			pkt.pkt_82 = NULL;
		}
		
		// increment hops field
		pkt.dhcp.hops++;
		
		// transmit raw packet
		txrawpacket(&pkt, ifout->name, ifout->ip, ifout->kindex);
	
		// update stats
		ifout->stats.dhcp_tx++;
		switch (*pkt.pkt_type) {
			//RFC2153:
			case 2:
				ifout->stats.dhcp_tx_offer++;
				break;
			case 5:
				ifout->stats.dhcp_tx_ack++;
				update_lease_count(pkt.server_id);
				break;
			case 6:
				ifout->stats.dhcp_tx_nack++;
				break;
			default:
				ASSERT(0);
				return;
		}
		return;
	}
}
