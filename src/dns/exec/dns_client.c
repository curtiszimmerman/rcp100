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

static int last_server = 1;
uint16_t rate_limit = 0;

static inline uint16_t get_new_tid(void) {
	uint16_t r = ((unsigned) random()) & 0xffff;
	
	// check if the tid is already requested
	if (rq_find(r))
		return get_new_tid();
	
	return r;
}

// is the name unqualified?
// return a pointer to the unqualified name, or NULL if the name is qualified
static const char *is_unqualified(const char *name) {
	const char *rn = dnspkt_get_real_name((unsigned char *) name);
	if (rn == NULL)
		return NULL;
	
	if (strchr(rn, '.') != NULL)
		return NULL;	// this is a qualified name
	return rn;
}

// process a client packet
void process_client(uint32_t ip, uint16_t port, DnsPkt *pkt, int len) {
	shm->stats.dns_queries++;

	// pick up a server for this message
	uint32_t server_ip = 0;
	if (last_server == 1) {
		// try server 2
		if (shm->config.name_server2 != 0) {
			last_server = 2;
			server_ip = shm->config.name_server2;
		}
		else 
			server_ip = shm->config.name_server1;
	}
	else if (last_server == 2) {
		// try server 1n   
		if (shm->config.name_server1 != 0) {
			last_server = 1;
			server_ip = shm->config.name_server1;
		}
		else 
			server_ip = shm->config.name_server2;
	}
	else
		ASSERT(0);
	
	if (server_ip == 0) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"no DNS server configured, packet dropped");
		shm->stats.dns_err_no_server++;
		return;
	}

	// get tid
	uint16_t client_tid = ntohs(pkt->tid);

	// can we serve the request from cache?
	if (pkt->dns_query_type == 1 || pkt->dns_query_type == 28|| pkt->dns_query_type == 5) {
		// only CNAME, A and AAAA records are served from cache
		const char *name = NULL;
		char qualified[CLI_MAX_DOMAIN_NAME + 1];
		// if this is an unqualified domain name, add the domain name
		if ((name = is_unqualified( (char *) pkt->dns_query_name)) != NULL) {
			// build a fully qualified domain name
			if (strlen(name) + strlen(shm->config.domain_name) > CLI_MAX_DOMAIN_NAME) {
				// use the incoming name if the result seems to big
				name = (char *) pkt->dns_query_name;
			}	
			else {		
				strcpy(qualified, name);
				strcat(qualified, shm->config.domain_name);
				name = qualified;
			}
		}
		else
			name = (char *) pkt->dns_query_name;		
		
		DnsCache *dc = cache_find(name, pkt->dns_query_type);
		if (dc != NULL) {
			rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
				"DSN response from cache - type %u, %s",
				pkt->dns_query_type, dnspkt_get_real_name(pkt->dns_query_name));
			// build a dns packet
			DnsPkt out;
			int out_len = dnspkt_build(&out, dc, client_tid);
			if (out_len <= 12) {
				ASSERT(0);
				shm->stats.dns_err_tx++;
				return;
			}
	
	
			// build outgoing server address structure
			static struct sockaddr_in addr;
			memset(&addr, 0, sizeof(struct sockaddr_in));
			addr.sin_addr.s_addr = htonl(ip);
			addr.sin_port = htons(port);
			addr.sin_family = AF_INET;
	
			int txlen = sendto(client_sock, &out, out_len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
			if (txlen == -1) {
				ASSERT(0);
				shm->stats.dns_err_tx++;
				return;
			}
			
			shm->stats.dns_answers++;
			shm->stats.dns_cached_answers++;
			return;
		}
	}

	// the packet will be forwarded to a server; apply rate limiting
	if (shm->config.dns_rate_limit && ++rate_limit > shm->config.dns_rate_limit) {
		shm->stats.dns_err_rate_limit++;
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
			"forwarding rate limit exceeded, packet dropped");
		return;
	}


	// replace transaction id
	uint16_t new_tid = get_new_tid();
	pkt->tid = htons(new_tid);
	

	// queue the request
	DnsReq *req = rq_malloc();
	if (req == NULL) {
		rcpLog(muxsock, RCP_PROC_DNS, RLOG_WARNING, RLOG_FC_DNS,
			"request limit exceeded, attempting recovery...");
		return;
	}
	memset(req, 0, sizeof(DnsReq));
	req->client_tid = client_tid;
	req->new_tid = new_tid;
	req->client_ip = ip;
	req->client_port = port;
	req->server_ip = server_ip;
	req->query_type = pkt->dns_query_type;
	strncpy(req->query_name, dnspkt_get_real_name(pkt->dns_query_name), CLI_MAX_DOMAIN_NAME);
	req->query_name[CLI_MAX_DOMAIN_NAME] = '\0';
	rq_add(req);

	// build outgoing server address structure
	static struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = htonl(server_ip);
	addr.sin_port = htons(DNS_SERVER_PORT);	// send messages to DNS server port (53)
	addr.sin_family = AF_INET;

	// forward message to server
	int txlen = sendto(req->sock, pkt, len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (txlen == -1) {
		shm->stats.dns_err_tx++;
		return;
	}

	
	rcpLog(muxsock, RCP_PROC_DNS, RLOG_DEBUG, RLOG_FC_DNS,
		"DNS Query - type %u, %s, forwarded to %d.%d.%d.%d",
		pkt->dns_query_type, dnspkt_get_real_name(pkt->dns_query_name),
		RCP_PRINT_IP(server_ip));
}

