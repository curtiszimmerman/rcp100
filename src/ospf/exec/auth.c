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
#include "packet.h"

// return 1 if error, 0 if ok
int auth_rx(OspfPacket *pkt, OspfPacketDesc *desc) {
	RcpInterface *intf = rcpFindInterfaceByKIndex(shm, desc->if_index);
	if (!intf)
		return 1;

	uint16_t type = ntohs(pkt->header.auth_type);
	if (type != intf->ospf_auth_type) {
		trap_IfAuthFailure(desc->if_index, desc->ip_source, 1, pkt->header.type);
		return 1;
	}
	
	// simple authentication
	if (type == 1) {
		if (memcmp(pkt->header.auth_data, intf->ospf_passwd, 8)) {
			trap_IfAuthFailure(desc->if_index, desc->ip_source, 0, pkt->header.type);
			return 1;
		}
		// clear the password field before checksum
		memset(pkt->header.auth_data, 0, 8);
	}
	// md5 authentication
	else if (type == 2) {
		// copy md5 header
		OspfMd5	md5;
		memcpy(&md5, pkt->header.auth_data, 8);
		md5.seq = ntohl(md5.seq);

		// check key id
		if (md5.key_id != intf->ospf_md5_key) {
			trap_IfAuthFailure(desc->if_index, desc->ip_source, 0, pkt->header.type);
			return 1;
		}

		// check sequence number
		OspfNeighbor *nb = neighborFind(desc->area_id, desc->ip_source);
		if (nb != NULL) {
			if (md5.seq < nb->ospf_md5_seq) {
				trap_IfAuthFailure(desc->if_index, desc->ip_source, 0, pkt->header.type);
				return 1;
			}
		}
			
		// check digest present
		if (desc->rx_size != (ntohs(pkt->header.length) + 20 + md5.auth_data_len)) {
			trap_IfAuthFailure(desc->if_index, desc->ip_source, 0, pkt->header.type);
			return 1;
		}
				
		// store packet digest
		uint8_t pkt_digest[16];
		uint8_t *end = (uint8_t *) &pkt->header;
		end += ntohs(pkt->header.length);
		memcpy(pkt_digest, end, 16);

		// copy our password at the end of the packet
		memcpy(end, intf->ospf_md5_passwd, 16);

		MD5_CTX context;
		uint8_t digest[16];
		MD5Init(&context);
		MD5Update(&context, (uint8_t *) &pkt->header, ntohs(pkt->header.length) + 16);
		MD5Final(digest, &context);

		// compare
		if (memcmp(digest, pkt_digest, 16) != 0) {
			trap_IfAuthFailure(desc->if_index, desc->ip_source, 0, pkt->header.type);
			return 1;
		}
			
		// update seq in neighbor structure
		if (nb != NULL)
			 nb->ospf_md5_seq = md5.seq;
			
	}
	
	return 0;
}

		
void auth_tx(OspfPacket *pkt, OspfNetwork *net) {
	if (net->auth_type == 1)
		memcpy(pkt->header.auth_data, net->ospf_passwd, 8);
		
	else if (net->auth_type == 2) {
		OspfMd5	md5;
		memset(&md5, 0, sizeof(OspfMd5));
		md5.key_id = net->ospf_md5_key;
		md5.auth_data_len = 16;
		md5.seq = ntohl(shm->config.ospf_md5_seq);
		memcpy(pkt->header.auth_data, &md5, 8);

		uint8_t *end = (uint8_t *) &pkt->header;
		end += ntohs(pkt->header.length);
		memcpy(end, net->ospf_md5_passwd, 16);
		
		MD5_CTX context;
		uint8_t digest[16];
		MD5Init(&context);
		MD5Update(&context, (uint8_t *) &pkt->header, ntohs(pkt->header.length) + 16);
		MD5Final(digest, &context);
		memcpy(end, digest, 16);
	}
		
}

