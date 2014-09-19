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
no monitor dns 8.8.8.8
no monitor ssh web.sourceforge.net
no monitor dns 8.8.4.4
monitor ntp nist1-nj.ustiming.org
#endif

// packet
static unsigned char ntp_pkt[48] = {
	0x23, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xa7, 0xec, 0x2d, 0xf1, 0x10, 0xe1, 0xa2, 0xb0	
};
	

#if 0
void reset_dns_pkt(void) {
	uint8_t r0 = ((unsigned) rand()) & 0xff;
	uint8_t r1 = ((unsigned) rand()) & 0xff;
	dns_pkt[0] = r0;
	dns_pkt[1] = r1;
}
#endif

void send_ntp(int sock, RcpNetmonHost *ptr) {
	ASSERT(sock);

	// build outgoing server address structure
	static struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = htonl(ptr->ip_resolved);
	addr.sin_port = htons(123);
	addr.sin_family = AF_INET;

	int txlen = sendto(sock, ntp_pkt, sizeof(ntp_pkt), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
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

void get_ntp(uint32_t port, RcpNetmonHost *ptr, unsigned char *pkt, int len) {
	if (len != 48)
		return;
	// check the port
	if (port != 123)
		return;

	// check the server flags
	if ((pkt[0] & 0x07) != 0x04)
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

void monitor_ntp(RcpNetmonHost *ptr) {
	ASSERT(ptr);
	
	if (ptr->pkt_sent) {
		if (ptr->wait_response == 0) {
			if (ptr->status == 0) {
				ptr->status = 1;
				rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
					"NTP server %s UP", ptr->hostname);
			}
		}
		else {
			if (ptr->status == 1) {
				ptr->status = 0;
				rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
					"NTP server %s DOWN", ptr->hostname);
			}
			ptr->wait_response = 0;
		}
	}
}

