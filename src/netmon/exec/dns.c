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
#include "mgmt.h"

#if 0
no monitor host 192.168.254.254
no monitor host google.com
no monitor tcp www.google.com
no monitor http www.google.com
no monitor http www.slashdot.org
no monitor http www.lxer.com
no monitor ssh 10.0.0.10
no monitor smtp alt2.aspmx.l.google.com
no monitor smtp alt1.aspmx.l.google.com
monitor dns 8.8.8.8
#endif


// packet asking for google.com, transaction id 0x4567
static unsigned char dns_pkt[28] = {
	0x45, 0x67, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f,
	0x6d, 0x00, 0x00, 0x01, 0x00, 0x01
};


void reset_dns_pkt(void) {
	uint8_t r0 = ((unsigned) rand()) & 0xff;
	uint8_t r1 = ((unsigned) rand()) & 0xff;
	dns_pkt[0] = r0;
	dns_pkt[1] = r1;
}


void send_dns(int sock, RcpNetmonHost *ptr) {
	ASSERT(sock);

	// build outgoing server address structure
	static struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = htonl(ptr->ip_resolved);
	addr.sin_port = htons(53);
	addr.sin_family = AF_INET;

	int txlen = sendto(sock, dns_pkt, sizeof(dns_pkt), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (txlen == -1) {
		ptr->ip_sent = 0;
		ptr->wait_response = 0;
		return;
	}
	gettimeofday(&ptr->start_tv, NULL);
	ptr->ip_sent = ptr->ip_resolved;
	ptr->pkt_sent++;
	ptr->interval_pkt_sent++;
	ptr->wait_response = 1;
}

void get_dns(uint32_t port, RcpNetmonHost *ptr, unsigned char *pkt, int len) {
	if (len < 6)
		return;

	// check the port
	if (port != 53)
		return;
	
	// check transaction id
	if (pkt[0] != dns_pkt[0] || pkt[1] != dns_pkt[1])
		return;
	
	// check the flags: this is a response to a standard query
	if ((pkt[2] & 0xf0) != 0x80)
		return;

	// it was one question
	if (pkt[4] != 0 || pkt[5] != 1)
		return;
	
	// we assume a valid response in this moment
	ptr->pkt_received++;
	ptr->interval_pkt_received++;
	ptr->wait_response = 0;

	// calculate the response time
	struct timeval end_tv;
	gettimeofday (&end_tv, NULL);
	unsigned resp_time = delta_tv(&ptr->start_tv, &end_tv);
//printf("received ping reply from %d.%d.%d.%d, response time %fs\n",
//	RCP_PRINT_IP(ip), (float) resp_time / (float) 1000000);
	ptr->resptime_acc += resp_time;
	ptr->resptime_samples++;
}

void monitor_dns(RcpNetmonHost *ptr) {
	ASSERT(ptr);
	
	if (ptr->pkt_sent) {
		if (ptr->wait_response == 0) {
			if (ptr->status == 0) {
				ptr->status = 1;
				rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
					"DNS server %s UP", ptr->hostname);
			}
		}
		else {
			if (ptr->status == 1) {
				ptr->status = 0;
				rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
					"DNS server %s DOWN", ptr->hostname);
			}
			ptr->wait_response = 0;
		}
	}
}

