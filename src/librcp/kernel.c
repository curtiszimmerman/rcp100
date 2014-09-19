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
#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>

#include "librcp.h"

static int add_del_route(uint32_t dest, uint8_t netmask, uint32_t gw, uint32_t metric, int8_t protocol, int8_t del);

int rcpAddRoute(int muxsock, RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t metric) {
	int protocol = 0;
	
	if (type == RCP_ROUTE_STATIC) {
		// metric is in fact a distance
		metric &= 0xff;
		metric <<= 24;
	}
	else if (type == RCP_ROUTE_BLACKHOLE)
		gw = 0;
	else if (type == RCP_ROUTE_RIP) {
		protocol = RCP_RIP_KERNEL_PROTO;
		metric &= 0x0f;
		metric |= (RCP_ROUTE_DISTANCE_RIP << 24);
	}
	else if (type == RCP_ROUTE_OSPF) {
		protocol = RCP_OSPF_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}
	else if (type == RCP_ROUTE_OSPF_IA) {
		protocol = RCP_OSPF_IA_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}
	else if (type == RCP_ROUTE_OSPF_E1) {
		protocol = RCP_OSPF_E1_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}
	else if (type == RCP_ROUTE_OSPF_E2) {
		protocol = RCP_OSPF_E2_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}

	rcpLog(muxsock, rcpGetProcId(), RLOG_DEBUG, RLOG_FC_ROUTER,
		"add kernel route %d.%d.%d.%d/%d [%u/%u] via %d.%d.%d.%d",
		RCP_PRINT_IP(ip), mask2bits(mask),
		(metric & 0xff000000) >> 24, metric & 0x00ffffff,
		RCP_PRINT_IP(gw));

	return add_del_route(ip, mask2bits(mask), gw, metric, protocol, 0);
}

int rcpDelRoute(int muxsock, uint32_t ip, uint32_t mask, uint32_t gw) {
	rcpLog(muxsock, rcpGetProcId(), RLOG_DEBUG, RLOG_FC_ROUTER,
		"delete kernel route %d.%d.%d.%d/%d via %d.%d.%d.%d",
		RCP_PRINT_IP(ip), mask2bits(mask),
		RCP_PRINT_IP(gw));
	return add_del_route(ip, mask2bits(mask), gw, 0, 0, 1);
}



int rcpAddECMPRoute(int muxsock, RcpRouteType type, uint32_t ip, uint32_t mask, uint32_t metric, int gw_cnt, uint32_t *gw_ptr) {
	int protocol = 0;
	ASSERT(gw_cnt >= 1);
	if (gw_cnt == 0)
		return 1;
	
	if (type == RCP_ROUTE_STATIC) {
		// metric is in fact a distance
		metric &= 0xff;
		metric <<= 24;
	}
	else if (type == RCP_ROUTE_BLACKHOLE)
		;
	else if (type == RCP_ROUTE_RIP) {
		protocol = RCP_RIP_KERNEL_PROTO;
		metric &= 0x0f;
		metric |= (RCP_ROUTE_DISTANCE_RIP << 24);
	}
	else if (type == RCP_ROUTE_OSPF) {
		protocol = RCP_OSPF_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}
	else if (type == RCP_ROUTE_OSPF_IA) {
		protocol = RCP_OSPF_IA_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}
	else if (type == RCP_ROUTE_OSPF_E1) {
		protocol = RCP_OSPF_E1_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}
	else if (type == RCP_ROUTE_OSPF_E2) {
		protocol = RCP_OSPF_E2_KERNEL_PROTO;
		metric &= 0x00ffffff;
		metric |= (RCP_ROUTE_DISTANCE_OSPF << 24);
	}

	// allocate enough memory to store the command
	// ip route add metric + nexthop via * gw_cnt	
	int size = 100 + 35 * gw_cnt;
	char *cmd = malloc(size);
	if (cmd == NULL) {
		ASSERT(0);
		return 1;
	}
	
	// build the command
	sprintf(cmd, "ip route add %d.%d.%d.%d/%d protocol %d metric %u ",
		RCP_PRINT_IP(ip), mask2bits(mask), protocol, metric);
	char *ptr = cmd + strlen(cmd);
	
	int i;
	for (i = 0; i < gw_cnt; i++) {
		sprintf(ptr, "nexthop via %d.%d.%d.%d ", RCP_PRINT_IP(gw_ptr[i]));
		ptr += strlen(ptr);
	}
//printf("%s\n", cmd);

	// execute the command
	rcpLog(muxsock, rcpGetProcId(), RLOG_DEBUG, RLOG_FC_ROUTER, "%s", cmd + 3);
	int v = system(cmd);
	free(cmd);
	if (v == -1) {
		ASSERT(0);
		return 1;
	}
		
	return 0;
}

int rcpDelECMPRoute(int muxsock, RcpRouteType type, uint32_t ip, uint32_t mask, int gw_cnt, uint32_t *gw_ptr) {
	ASSERT(gw_cnt >= 1);
	if (gw_cnt == 0)
		return 1;


	int protocol = 0;
	if (type == RCP_ROUTE_RIP) {
		protocol = RCP_RIP_KERNEL_PROTO;
	}
	else if (type == RCP_ROUTE_OSPF) {
		protocol = RCP_OSPF_KERNEL_PROTO;
	}
	else if (type == RCP_ROUTE_OSPF_IA) {
		protocol = RCP_OSPF_IA_KERNEL_PROTO;
	}
	else if (type == RCP_ROUTE_OSPF_E1) {
		protocol = RCP_OSPF_E1_KERNEL_PROTO;
	}
	else if (type == RCP_ROUTE_OSPF_E2) {
		protocol = RCP_OSPF_E2_KERNEL_PROTO;
	}

	// allocate enough memory to store the command
	// ip route del + nexthop via * gw_cnt	
	int size = 100 + 35 * gw_cnt;
	char *cmd = malloc(size);
	if (cmd == NULL) {
		ASSERT(0);
		return 1;
	}
	
	// build the command
	sprintf(cmd, "ip route del %d.%d.%d.%d/%d protocol %d ",
		RCP_PRINT_IP(ip), mask2bits(mask), protocol);
	char *ptr = cmd + strlen(cmd);
	
	int i;
	for (i = 0; i < gw_cnt; i++) {
		sprintf(ptr, "nexthop via %d.%d.%d.%d ", RCP_PRINT_IP(gw_ptr[i]));
		ptr += strlen(ptr);
	}
//printf("%s\n", cmd);

	// execute the command
	rcpLog(muxsock, rcpGetProcId(), RLOG_DEBUG, RLOG_FC_ROUTER, "%s", cmd + 3);
	int v = system(cmd);
	free(cmd);
	if (v == -1) {
		ASSERT(0);
		return 1;
	}
		
	return 0;
}


static int add_del_route( uint32_t dest, uint8_t netmask, uint32_t gw, uint32_t metric, int8_t protocol, int8_t del ) {
	dest = htonl(dest);
	gw = htonl(gw);


	// set socket address
	struct sockaddr_nl nladdr;
	memset( &nladdr, 0, sizeof(struct sockaddr_nl) );
	nladdr.nl_family = AF_NETLINK;

	// set request
#define SIZE_RTA (sizeof(struct rtattr) + 4) // header + 4 bytes payload
	struct {
		struct nlmsghdr nh;
		struct rtmsg rtm;
		uint8_t buff[3 * SIZE_RTA];
	} req;

	int len = sizeof(struct rtmsg);
	memset( &req, 0, sizeof(req) );
	req.nh.nlmsg_pid = getpid();
	req.rtm.rtm_family = AF_INET;
	req.rtm.rtm_table = 0;
	req.rtm.rtm_dst_len = netmask;

	if ( del ) {
		req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		req.nh.nlmsg_type = RTM_DELROUTE;
		req.rtm.rtm_scope = RT_SCOPE_NOWHERE;
	}
	else {
		req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
		req.nh.nlmsg_type = RTM_NEWROUTE;
		req.rtm.rtm_scope = RT_SCOPE_UNIVERSE;
		if (protocol == 0)
			req.rtm.rtm_protocol = RTPROT_STATIC;
		else
			req.rtm.rtm_protocol = protocol;

		if (gw == 0) // blackhole
			req.rtm.rtm_type = RTN_BLACKHOLE;
		else
			req.rtm.rtm_type = RTN_UNICAST;
	}

	// set attributes
	// first rta destination
	struct rtattr *rta = (struct rtattr *)req.buff;
	rta->rta_type = RTA_DST;
	rta->rta_len = sizeof(struct rtattr) + 4;
	memcpy((uint8_t *) rta + sizeof(struct rtattr), &dest, 4 );
	len += SIZE_RTA;
	rta = (struct rtattr *)(req.buff + SIZE_RTA);

	if (gw != 0) {
		// second rta gateway
		rta->rta_type = RTA_GATEWAY;
		rta->rta_len = SIZE_RTA;
		memcpy((uint8_t *) rta + sizeof(struct rtattr), &gw, 4 );
		len += SIZE_RTA;
		rta = (struct rtattr *) ((uint8_t *) rta + SIZE_RTA);
	
		if (metric && !del) {
			// third rta metic
			rta->rta_type = RTA_PRIORITY;
			rta->rta_len = SIZE_RTA;
			memcpy((uint8_t *) rta + sizeof(struct rtattr), &metric, 4);
			len += SIZE_RTA;
			rta = (struct rtattr *) ((uint8_t *) rta + SIZE_RTA);
		}
	}

	// set request length
	req.nh.nlmsg_len = NLMSG_LENGTH(len);

	// open socket
	int sock;
	if ((sock = socket( PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0)
		return 1;

	// send request
	if (sendto(sock, &req, req.nh.nlmsg_len, 0, (struct sockaddr *)&nladdr, sizeof(struct sockaddr_nl)) < 0) {
		close( sock );
		return 1;
	}

	// receive response
	char buf[4096];
	struct iovec iov = {
		buf,
		sizeof(buf)
	};
	struct msghdr msg;
	memset( &msg, 0, sizeof(struct msghdr) );
	msg.msg_name = (void *)&nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	len = recvmsg( sock, &msg, 0 );
	struct nlmsghdr *nh = (struct nlmsghdr *)buf;

	while (NLMSG_OK(nh, len)) {
		if ( nh->nlmsg_type == NLMSG_ERROR) {
			close(sock);
			return 1;
		}
		if ( nh->nlmsg_type == NLMSG_DONE )
			break;
		nh = NLMSG_NEXT(nh, len);
	}

	close(sock);
	return 0;
}

