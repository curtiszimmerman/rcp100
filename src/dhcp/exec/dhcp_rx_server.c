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
#include "lease.h"

static uint8_t cookie[4] = {0x63, 0x82, 0x53, 0x63};

static inline unsigned add_msgtype_offer(uint8_t *ptr) {
	*ptr++ = 0x35;
	*ptr++ = 0x01;
	*ptr++ = 0x02;
	return 3;
}

static inline unsigned add_msgtype_ack(uint8_t *ptr) {
	*ptr++ = 0x35;
	*ptr++ = 0x01;
	*ptr++ = 0x05;
	return 3;
}

static inline unsigned add_msgtype_nack(uint8_t *ptr) {
	*ptr++ = 0x35;
	*ptr++ = 0x01;
	*ptr++ = 0x06;
	return 3;
}

static inline unsigned add_serverid(uint8_t *ptr, uint32_t ip) {
	*ptr++ = 0x36;
	*ptr++ = 0x04;
	ip = htonl(ip);
	memcpy(ptr, &ip, 4);
	return 6;
}

static inline unsigned add_lease(uint8_t *ptr, uint32_t seconds) {
	*ptr++ = 0x33;
	*ptr++ = 0x04;
	seconds = htonl(seconds);
	memcpy(ptr, &seconds, 4);
	return 6;
}

static inline unsigned add_subnetmask(uint8_t *ptr, uint32_t mask) {
	*ptr++ = 0x01;
	*ptr++ = 0x04;
	mask = htonl(mask);
	memcpy(ptr, &mask, 4);
	return 6;
}

static unsigned add_dns(uint8_t *ptr) {
	uint32_t dns1 = 0;
	uint32_t dns2 = 0;
	uint8_t len = 0;
	if (shm->config.dhcps_dns1 != 0) {
		len += 4;
		dns1 = shm->config.dhcps_dns1;
	}
	else
		return 0;
		
	if (shm->config.dhcps_dns2 != 0) {
		len += 4;
		dns2 = shm->config.dhcps_dns2;
	}
	
	*ptr++ = 0x06;
	*ptr++ = len;
	dns1 = htonl(dns1);
	memcpy(ptr, &dns1, 4);
	ptr+= 4;
	if (dns2) {
		dns2 = htonl(dns2);
		memcpy(ptr, &dns2, 4);
	}

	return len + 2;
}

static unsigned add_ntp(uint8_t *ptr) {
	uint32_t ntp1 = 0;
	uint32_t ntp2 = 0;
	uint8_t len = 0;
	if (shm->config.dhcps_ntp1 != 0) {
		len += 4;
		ntp1 = shm->config.dhcps_ntp1;
	}
	else
		return 0;
		
	if (shm->config.dhcps_ntp2 != 0) {
		len += 4;
		ntp2 = shm->config.dhcps_ntp2;
	}
	
	*ptr++ = 42;
	*ptr++ = len;
	ntp1 = htonl(ntp1);
	memcpy(ptr, &ntp1, 4);
	ptr+= 4;
	if (ntp2) {
		ntp2 = htonl(ntp2);
		memcpy(ptr, &ntp2, 4);
	}

	return len + 2;
}

static unsigned add_router(uint8_t *ptr, RcpDhcpsNetwork *net) {
	uint32_t r1 = 0;
	uint32_t r2 = 0;
	uint8_t len = 0;
	if (net->gw1) {
		len += 4;
		r1 = net->gw1;
	}
	else
		return 0;
		
	if (net->gw2) {
		len += 4;
		r2 = net->gw2;
	}
	
	*ptr++ = 0x03;
	*ptr++ = len;
	r1 = htonl(r1);
	memcpy(ptr, &r1, 4);
	ptr+= 4;
	if (r2) {
		r2 = htonl(r2);
		memcpy(ptr, &r2, 4);
	}

	return len + 2;
}

static inline unsigned add_domain_name(uint8_t *ptr) {
	if (*shm->config.dhcps_domain_name == '\0')
		return 0;

	unsigned len = strlen((char *) shm->config.dhcps_domain_name);
	*ptr++ = 15;
	*ptr++ = len;
	memcpy(ptr, shm->config.dhcps_domain_name, len);
	return len + 2;
}

static inline unsigned add_option82(uint8_t *ptr, Packet *pkt) {
	if (!pkt->pkt_82)
		return 0;
	
	unsigned len = *(pkt->pkt_82 + 1) + 2;
	memcpy(ptr, pkt->pkt_82, len);
	return len;
}

static uint32_t assign_ip(RcpDhcpsNetwork *ptr, const uint8_t *mac) {
	ASSERT(ptr);
	uint32_t retval = 0;
	
	// look for a static lease
	RcpDhcpsHost *hptr;
	int i;
	for (i = 0, hptr = shm->config.dhcps_host; i < RCP_DHCP_HOST_LIMIT; i++, hptr++) {
		if (!hptr->valid)
			continue;
		// compare mac
		if (memcmp(hptr->mac, mac, 6) == 0) {
			// compare network
			if ((hptr->ip & ptr->mask) == (ptr->ip & ptr->mask))
				return hptr->ip;
		}
	}
	
	// check last ip assigned
	uint32_t start = ptr->last_ip;
	if (start == 0)
		start = ptr->range_start;
	else {
		start++;
		if (start < ptr->range_start || start > ptr->range_end)
			start = ptr->range_start;
	}
			
	uint32_t ip;
	int found = 0;
	for (ip = start; ip <= ptr->range_end; ip++) {
		Lease *lease = lease_find_ip(ip);
		if (lease == NULL) {
			found = 1;
			retval = ip;
			break;
		}
	}

	if (!found) {
		for (ip = ptr->range_start; ip <= start; ip++) {
			Lease *lease = lease_find_ip(ip);
			if (lease == NULL) {
				found = 1;
				retval = ip;
				break;
			}
		}
	}
	
	ptr->last_ip = retval;
	return retval;
}

static Lease *assign_lease(RcpInterface *pif, const uint8_t *mac) {
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if ((pif->ip & pif->mask) != ptr->ip || pif->mask != ptr->mask)
			continue;
		
		// we have a dhcp network configured, look trough ranges
		if (ptr->range_start == 0 || ptr->range_end == 0)
			continue;
			
		// assign an address from the range
		uint32_t ip = assign_ip(ptr, mac);
		if (ip == 0)
			return NULL;
		
		// in ip we have the new ip address
		uint32_t lease_seconds = ptr->lease_minutes * 60 + ptr->lease_hours * 3600 + ptr->lease_days * 24 * 3600;
		time_t now = time(NULL);
		Lease *lease = lease_add(mac, ip, now +  DHCP_OFFER_LEASE_TIME); // this is a temporary lease
		if (!lease) {
			ASSERT(0);
			return NULL;
		}
		lease->lease = lease_seconds;
		lease->mask = ptr->mask;
		return lease;
	}
	
	return NULL;
}

static Lease *assign_lease_relay(const uint32_t giaddr, const uint8_t *mac) {
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if ((giaddr & ptr->mask) != ptr->ip)
			continue;
		
		// we have a dhcp network configured, look trough ranges
		if (ptr->range_start == 0 || ptr->range_end == 0)
			continue;

		// assign an address from the range
		uint32_t ip = assign_ip(ptr, mac);
		if (ip == 0)
			return NULL;
		
		// in ip we have the new ip address
		uint32_t lease_seconds = ptr->lease_minutes * 60 + ptr->lease_hours * 3600 + ptr->lease_days * 24 * 3600;
		time_t now = time(NULL);
		Lease *lease = lease_add(mac, ip, now +  DHCP_OFFER_LEASE_TIME); // this is a temporary lease
		if (!lease) {
			ASSERT(0);
			return NULL;
		}
		lease->lease = lease_seconds;
		lease->mask = ptr->mask;
		return lease;
	}
	
	return NULL;
}


static RcpDhcpsNetwork *find_network(Lease *lease) {
	int i;
	RcpDhcpsNetwork *ptr;

	for (i = 0, ptr = shm->config.dhcps_network; i < RCP_DHCP_NETWORK_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if ((lease->ip & lease->mask) != ptr->ip || lease->mask != ptr->mask)
			continue;
		
		return ptr;
	}
	
	return NULL;
}


static void process_discover(int sock, int icmp_sock, Packet *pkt, RcpInterface *pif) {
	uint32_t giaddr =  ntohl(pkt->dhcp.giaddr);

	// do we have an existing lease for this mac address???
	Lease *lease = lease_find_mac(pkt->dhcp.chaddr);
	
	// if the client previousely declined this lease, disregard this discover message and let it cool down for 
	// the temporary lease time
	if (lease && lease->declined)
		return;
	
	if (!lease) {
		if (giaddr == 0)
			lease = assign_lease(pif, pkt->dhcp.chaddr);
		else
			lease = assign_lease_relay(giaddr, pkt->dhcp.chaddr);
	}
	if (!lease) {
		rcpLog(muxsock, RCP_PROC_DHCP, RLOG_WARNING, RLOG_FC_DHCP,
			"cannot assign new lease; the pool could be exhausted");
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		return;
	}
	RcpDhcpsNetwork *net = find_network(lease);
	if (!net) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_not_configured++; // interface not configured for dhcp server services
		return;
	}
	
	// store tid in the lease
	memcpy(&lease->tid, &pkt->dhcp.tid, 4);
	
	// build outgoing packet
	Packet *outpkt = malloc(sizeof(Packet));
	if (outpkt == NULL) {
		ASSERT(0);
		return;
	}
	memset(outpkt, 0, sizeof(Packet));

	outpkt->dhcp.op = 2;
	outpkt->dhcp.htype = 1;
	outpkt->dhcp.hlen = 6;
//	outpkt->dhcp.hops = 0;
	outpkt->dhcp.tid = pkt->dhcp.tid;
	outpkt->dhcp.secs = pkt->dhcp.secs;
//	outpkt->dhcp.flags = 0;
//	outpkt->dhcp.ciaddr = 0;	
	outpkt->dhcp.yiaddr = htonl(lease->ip);
	outpkt->dhcp.siaddr = htonl(pif->ip);
	outpkt->dhcp.giaddr = pkt->dhcp.giaddr;
	memcpy(outpkt->dhcp.chaddr, pkt->dhcp.chaddr, 6);
	memcpy(&outpkt->dhcp.cookie, cookie, 4);

	// add options
	unsigned optlen = 0;
	optlen += add_msgtype_offer(outpkt->dhcp.options + optlen);
	optlen += add_serverid(outpkt->dhcp.options + optlen, pif->ip);
	optlen += add_lease(outpkt->dhcp.options + optlen, lease->lease);
	optlen += add_subnetmask(outpkt->dhcp.options + optlen, lease->mask);
	optlen += add_router(outpkt->dhcp.options + optlen, net);
	optlen += add_dns(outpkt->dhcp.options + optlen);
	optlen += add_ntp(outpkt->dhcp.options + optlen);
	optlen += add_domain_name(outpkt->dhcp.options + optlen);
	// add option 82 if present in the incoming packet
	if (giaddr)
		optlen += add_option82(outpkt->dhcp.options + optlen, pkt);
	outpkt->dhcp.options[optlen] = 0xff;
	optlen++;
	outpkt->pkt_len = 0xf0 + optlen;
	outpkt->icmp_giaddr = giaddr;
	outpkt->icmp_udp_sock = sock;
	outpkt->icmp_pif = pif;
	send_icmp_pkt(icmp_sock, lease->ip, outpkt);
}

static void process_decline(int sock, Packet *pkt, RcpInterface *pif) {
	// instead of deleting the lease, we just let the temporary lease expire; however, we do record errors

	// if requested ip address option and ciaddr are missing, this is an invalid packet, don't answer
	uint32_t ip = pkt->requested_ip;
	if (ip == 0) {
		// grab the ip from ciaddr field
		memcpy(&ip, &pkt->dhcp.ciaddr, 4);
		ip = ntohl(ip);
		if (ip == 0) {
			pif->stats.dhcp_err_rx++;
			pif->stats.dhcp_err_pkt++;
			return;
		}
	}
	
	// find an existing lease
	Lease *lease1 = lease_find_ip(ip);
	Lease *lease2 = lease_find_mac(pkt->dhcp.chaddr);
	if (!lease1 || !lease2 || lease1 != lease2) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		return;
	}
	// check tid if this was a temporary lease
	if (lease1->tid && memcmp(&lease1->tid, &pkt->dhcp.tid, 4) != 0) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		return;
	}

	
	RcpDhcpsNetwork *net = find_network(lease1);
	if (!net) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_not_configured++; // interface not configured for dhcp server services
	}
	
	// mark the lease as declined
	lease1->declined = 1;

	// notify the admin about a lease declined
	rcpLog(muxsock, RCP_PROC_DHCP, RLOG_NOTICE, RLOG_FC_DHCP,
		"lease %d.%d.%d.%d declined by the client %02x:%02x:%02x:%02x:%02x:%02x",
			RCP_PRINT_IP(lease1->ip), RCP_PRINT_MAC(lease1->mac));

}

static void send_nack(int sock, Packet *pkt, RcpInterface *pif) {
	uint32_t giaddr =  ntohl(pkt->dhcp.giaddr);
	// build outgoing packet
	Packet outpkt;
	memset(&outpkt, 0, sizeof(outpkt));

	outpkt.dhcp.op = 2;
	outpkt.dhcp.htype = 1;
	outpkt.dhcp.hlen = 6;
//	outpkt.dhcp.hops = 0;
	outpkt.dhcp.tid = pkt->dhcp.tid;
	outpkt.dhcp.secs = pkt->dhcp.secs;
//	outpkt.dhcp.flags = 0;
//	outpkt.dhcp.ciaddr = 0;
	outpkt.dhcp.yiaddr = 0;
	outpkt.dhcp.siaddr = htonl(pif->ip);
	outpkt.dhcp.giaddr = pkt->dhcp.giaddr;
	memcpy(outpkt.dhcp.chaddr, pkt->dhcp.chaddr, 6);
	memcpy(&outpkt.dhcp.cookie, cookie, 4);

	// add options
	unsigned optlen = 0;
	optlen += add_msgtype_nack(outpkt.dhcp.options + optlen);
	outpkt.dhcp.options[optlen] = 0xff;
	optlen++;
	outpkt.pkt_len = 0xf0 + optlen;

#if 0	
RFC 2131          Dynamic Host Configuration Protocol         March 1997
      If 'giaddr' is 0x0 in the DHCPREQUEST message, the client is on
      the same subnet as the server.  The server MUST
      broadcast the DHCPNAK message to the 0xffffffff broadcast address
      because the client may not have a correct network address or subnet
      mask, and the client may not be answering ARP requests.
      Otherwise, the server MUST send the DHCPNAK message to the IP
      address of the BOOTP relay agent, as recorded in 'giaddr'.  The
      relay agent will, in turn, forward the message directly to the
      clients hardware address, so that the DHCPNAK can be delivered even
      if the client has moved to a new network.
#endif	
	
	
	// transmit packet
	pif->stats.dhcp_tx++;
	pif->stats.dhcp_tx_nack++;
	if (giaddr == 0) {
		// broadcast always
		txrawpacket(&outpkt, pif->name, pif->ip, pif->kindex);
	}
	else
		txpacket(sock, &outpkt, giaddr);


}

static void process_request(int sock, Packet *pkt, RcpInterface *pif) {
	uint32_t giaddr =  ntohl(pkt->dhcp.giaddr);

	// if requested ip address option and ciaddr are missing, this is an invalid packet, don't answer
	uint32_t ip = pkt->requested_ip;
	if (ip == 0) {
		// grab the ip from ciaddr field
		memcpy(&ip, &pkt->dhcp.ciaddr, 4);
		ip = ntohl(ip);
		if (ip == 0) {
			pif->stats.dhcp_err_rx++;
			pif->stats.dhcp_err_pkt++;
			send_nack(sock, pkt, pif);
			return;
		}
	}
	
	// find an existing lease
	Lease *lease1 = lease_find_ip(ip);
	Lease *lease2 = lease_find_mac(pkt->dhcp.chaddr);
	if (!lease1 || !lease2 || lease1 != lease2) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		send_nack(sock, pkt, pif);
		return;
	}
	// check tid if this was a temporary lease
	if (lease1->tid && memcmp(&lease1->tid, &pkt->dhcp.tid, 4) != 0) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		send_nack(sock, pkt, pif);
		return;
	}
	lease1->tid = 0;

	
	RcpDhcpsNetwork *net = find_network(lease1);
	if (!net) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_not_configured++; // interface not configured for dhcp server services
		return;
	}

	// renew the lease
	time_t now = time(NULL);
	uint32_t lease_seconds = net->lease_minutes * 60 + net->lease_hours * 3600 + net->lease_days * 24 * 3600;
	lease_renew(lease1, now + lease_seconds);
	lease1->lease = lease_seconds;
	lease_store(lease1);	// store the lease in permanent storage

	// build outgoing packet
	Packet outpkt;
	memset(&outpkt, 0, sizeof(outpkt));

	outpkt.dhcp.op = 2;
	outpkt.dhcp.htype = 1;
	outpkt.dhcp.hlen = 6;
//	outpkt.dhcp.hops = 0;
	outpkt.dhcp.tid = pkt->dhcp.tid;
	outpkt.dhcp.secs = pkt->dhcp.secs;
//	outpkt.dhcp.flags = 0;
//	outpkt.dhcp.ciaddr = 0;
	outpkt.dhcp.yiaddr = htonl(lease1->ip);
	outpkt.dhcp.siaddr = htonl(pif->ip);
	outpkt.dhcp.giaddr = pkt->dhcp.giaddr;
	memcpy(outpkt.dhcp.chaddr, pkt->dhcp.chaddr, 6);
	memcpy(&outpkt.dhcp.cookie, cookie, 4);

	// add options
	unsigned optlen = 0;
	optlen += add_msgtype_ack(outpkt.dhcp.options + optlen);
	optlen += add_serverid(outpkt.dhcp.options + optlen, pif->ip);
	optlen += add_lease(outpkt.dhcp.options + optlen, lease1->lease);
	optlen += add_subnetmask(outpkt.dhcp.options + optlen, lease1->mask);
	optlen += add_router(outpkt.dhcp.options + optlen, net);
	optlen += add_dns(outpkt.dhcp.options + optlen);
	optlen += add_ntp(outpkt.dhcp.options + optlen);
	optlen += add_domain_name(outpkt.dhcp.options + optlen);
	// add option 82 if present in the incoming packet
	if (giaddr)
		optlen += add_option82(outpkt.dhcp.options + optlen, pkt);
	outpkt.dhcp.options[optlen] = 0xff;
	optlen++;
	outpkt.pkt_len = 0xf0 + optlen;
	
	// transmit packet
	pif->stats.dhcp_tx++;
	pif->stats.dhcp_tx_ack++;
	if (giaddr == 0) {
		if (pkt->ip_source == 0)
			txrawpacket(&outpkt, pif->name, pif->ip, pif->kindex);
		else
			txpacket_client(sock, &outpkt, pkt->ip_source);
	}
	else
		txpacket(sock, &outpkt, giaddr);
}

static void process_release(int sock, Packet *pkt, RcpInterface *pif) {
	// grab the ip from ciaddr field
	uint32_t ip;
	memcpy(&ip, &pkt->dhcp.ciaddr, 4);
	ip = ntohl(ip);
	if (ip == 0) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_pkt++;
		return;
	}
	
	// find an existing lease
	Lease *lease1 = lease_find_ip(ip);
	Lease *lease2 = lease_find_mac(pkt->dhcp.chaddr);
	if (!lease1 || !lease2 || lease1 != lease2) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		return;
	}

	// check tid if this was a temporary lease
	if (lease1->tid) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_lease++;
		return;
	}
	
	RcpDhcpsNetwork *net = find_network(lease1);
	if (!net) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_not_configured++; // interface not configured for dhcp server services
		return;
	}

	// renew it with a lease time of DHCP_OFFER_LEASE_TIME (20 seconds, see lease.h)
	// the lease is removed after DHCP_OFFER_LEASE_TIME expires
	// if the client changes its mind, don't give it a new offer in all this time
	time_t now = time(NULL);
	lease_renew(lease1, now + DHCP_OFFER_LEASE_TIME);
	lease1->lease = DHCP_OFFER_LEASE_TIME;
	lease_store(lease1);	// store the lease in permanent storage
	lease1->declined = 1;
}

static void process_inform(int sock, Packet *pkt, RcpInterface *pif) {
	// if requested ip address option and ciaddr are missing, this is an invalid packet, don't answer
	uint32_t ip;
	// grab the ip from ciaddr field
	memcpy(&ip, &pkt->dhcp.ciaddr, 4);
	ip = ntohl(ip);
	if (ip == 0) {
		pif->stats.dhcp_err_rx++;
		pif->stats.dhcp_err_pkt++;
		return;
	}
	

	// build outgoing packet
	Packet outpkt;
	memset(&outpkt, 0, sizeof(outpkt));

	outpkt.dhcp.op = 2;
	outpkt.dhcp.htype = 1;
	outpkt.dhcp.hlen = 6;
//	outpkt.dhcp.hops = 0;
	outpkt.dhcp.tid = pkt->dhcp.tid;
	outpkt.dhcp.secs = pkt->dhcp.secs;
//	outpkt.dhcp.flags = 0;
//	outpkt.dhcp.ciaddr = 0;
	outpkt.dhcp.yiaddr = 0;
	outpkt.dhcp.siaddr = htonl(pif->ip);
	outpkt.dhcp.giaddr = pkt->dhcp.giaddr;
	memcpy(outpkt.dhcp.chaddr, pkt->dhcp.chaddr, 6);
	memcpy(&outpkt.dhcp.cookie, cookie, 4);

	// add options
	unsigned optlen = 0;
	optlen += add_msgtype_ack(outpkt.dhcp.options + optlen);
	optlen += add_serverid(outpkt.dhcp.options + optlen, pif->ip);
	optlen += add_dns(outpkt.dhcp.options + optlen);
	optlen += add_ntp(outpkt.dhcp.options + optlen);
	optlen += add_domain_name(outpkt.dhcp.options + optlen);
	outpkt.dhcp.options[optlen] = 0xff;
	optlen++;
	outpkt.pkt_len = 0xf0 + optlen;
	
	// transmit packet
	pif->stats.dhcp_tx++;
	pif->stats.dhcp_tx_ack++;
	// unicast it back to the sender, do not send it to giaddr
	txpacket_client(sock, &outpkt, ip);
}

void dhcp_rx_server(int sock, int icmp_sock, Packet *pkt, RcpInterface *pif) {
	switch (*pkt->pkt_type) {
		//RFC2153:
		case 1: // discover
			process_discover(sock, icmp_sock, pkt, pif);
			break;
		case 3: // request
			process_request(sock, pkt, pif);
			break;
		case 4: // decline
			process_decline(sock, pkt, pif);
			break;
		case 7: // release
			process_release(sock, pkt, pif);
			break;
		case 8: // inform
			process_inform(sock, pkt, pif);
			break;
		default:
			return;
	}
}

