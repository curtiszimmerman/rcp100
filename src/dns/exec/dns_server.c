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
#include "dns.h"

void process_server(uint32_t ip, uint16_t port, DnsPkt *pkt, int len) {
	// find the request in the queue
	uint16_t new_tid = ntohs(pkt->tid);
	DnsReq *req = rq_find(new_tid);
	if (req == NULL) {
		// we might get here if the response came to late
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"unexpected DNS response - type %u, %s, from %d.%d.%d.%d, packet dropped",
			pkt->dns_query_type, dnspkt_get_real_name(pkt->dns_query_name),
			RCP_PRINT_IP(ip));
		shm->stats.dns_err_unknown++;
		return;
	}
	if (ip != req->server_ip) {
		// wrong server	
		shm->stats.dns_err_unknown++;
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"DNS server not matching - type %u, %s, from %d.%d.%d.%d, packet dropped",
			pkt->dns_query_type, dnspkt_get_real_name(pkt->dns_query_name),
			RCP_PRINT_IP(ip));
		return;
	}
	// check the query
	if (req->query_type != pkt->dns_query_type ||
	     strcmp(req->query_name, dnspkt_get_real_name(pkt->dns_query_name)) != 0) {
		// wrong query	
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"DNS data not matching - type %u, %s, from %d.%d.%d.%d, packet dropped",
			pkt->dns_query_type, dnspkt_get_real_name(pkt->dns_query_name),
			RCP_PRINT_IP(ip));
		shm->stats.dns_err_unknown++;
		return;
	}



	rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
		"DNS Response - type %u, %s, from %d.%d.%d.%d",
		pkt->dns_query_type, dnspkt_get_real_name(pkt->dns_query_name),
		RCP_PRINT_IP(ip));

	// replace transaction id
	pkt->tid = htons(req->client_tid);
	
	// forward message to client
	static struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = htonl(req->client_ip);
	addr.sin_port = htons(req->client_port);
	addr.sin_family = AF_INET;
	int txlen = sendto(client_sock, pkt, len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (txlen == -1) {
		shm->stats.dns_err_tx++;
		ASSERT(0);
		return;
	}
	shm->stats.dns_answers++;

	rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
		"server message forwarded to client %d.%d.%d.%d", RCP_PRINT_IP(req->client_ip));

	// remove the request from active list
	rq_del(req);
	// put the request in the inactive list
	rq_free(req);
	
	// if dns error do nothing
	if (pkt->dns_error_code != 0)
		return;
	
	// don't bother to add entries with very short values
	if (pkt->dns_ttl < DNS_MIN_TTL)
		return;
	
	// set the maximum ttl
	if (pkt->dns_ttl > DNS_MAX_TTL)
		pkt->dns_ttl = DNS_MAX_TTL;
	
	// caching only type 1, 5, and 28
	if (pkt->dns_type != 1 && pkt->dns_type != 28 && pkt->dns_type != 5)
		return;
		
	// if it is not there already, cache the data
	DnsCache *dc = cache_find((char *) pkt->dns_query_name, pkt->dns_query_type);
	if (dc == NULL) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"adding to cache - query type %u, response type %u, ttl %d, name %s",
			pkt->dns_query_type, pkt->dns_type, pkt->dns_ttl, dnspkt_get_real_name(pkt->dns_query_name));
	
		// allocate a new cache entry
		dc = malloc(sizeof(DnsCache));
		if (dc == NULL) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"cannot allocate memory to store the cache entry");
			return;
		}
		memset(dc, 0, sizeof(DnsCache));

		// fill in the data
		strcpy(dc->dns_name, (char *) pkt->dns_query_name);
		dc->dns_flags = pkt->dns_flags;
		dc->dns_ttl = (int) pkt->dns_ttl;
		dc->type = pkt->dns_query_type;
		if (pkt->dns_type == 1)
			dc->dns_ip = pkt->dns_address;
		else if (pkt->dns_type == 28)
			memcpy(dc->dns_ipv6, pkt->dns_address_v6, 16);
		else if (pkt->dns_type == 5) {
			DnsPkt *newpkt = malloc(sizeof(DnsPkt));
			if (newpkt == NULL) {
				rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
					"cannot allocate memory to store the cache entry");
				free(dc);
				return;
			}
			memcpy(newpkt, pkt, sizeof(DnsPkt));
			dc->dns_cname = newpkt;
			dc->dns_cname_len = pkt->len;
		}
		else
			ASSERT(0);
		// put entry in cache
		cache_add(dc);
	}
}
