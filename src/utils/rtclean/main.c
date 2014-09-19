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
#include "librcp.h"
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <time.h>

uint32_t seq;
typedef int (*nlrq_filter)(struct nlmsghdr *n, void *);
static int add_del_route(uint32_t dest, uint8_t netmask, uint32_t gw, uint32_t metric, int8_t protocol, int8_t del);

static int store_msg(struct nlmsghdr *n, void *arg) {
	switch (n->nlmsg_type) {
		case RTM_NEWROUTE:
		case RTM_DELROUTE:
		case RTM_GETROUTE: {
			struct rtmsg *info = NLMSG_DATA(n);
			if (info->rtm_family != AF_INET)
				return 0;
			if (info->rtm_type != RTN_UNICAST &&
			     info->rtm_type != RTN_BLACKHOLE &&
			     info->rtm_type != RTN_PROHIBIT)	// todo: get rid of prohibit
				return 0; // we don't care about this route
		} break;

		default:
			return 0;
	}

	FILE *fp = (FILE*)arg;
	fwrite(n, 1, NLMSG_ALIGN(n->nlmsg_len), fp);
	fflush(fp);
	return 0;
}


static int nlrq_open(unsigned groups) {
	// open socket
	int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock < 0) 
		return 0;

	// socket buffers
	int sbuf = 32768;
	int rbuf = 1024 * 1024;
	if (setsockopt(sock,SOL_SOCKET,SO_SNDBUF,&sbuf,sizeof(int)) < 0)
		return 0;
	if (setsockopt(sock,SOL_SOCKET,SO_RCVBUF,&rbuf,sizeof(int)) < 0)
		return 0;
	
	// bind
	struct sockaddr_nl local;
	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = groups;
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0)
		return 0;
	
	// init seq
	seq = time(NULL);

	return sock;
}

static int nlrq_getroutes(int sock) {
	// message
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} r;
	memset(&r, 0, sizeof(r));

	// build message
	r.nlh.nlmsg_len = sizeof(r);
	r.nlh.nlmsg_type = RTM_GETROUTE;
	r.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	r.nlh.nlmsg_pid = getpid();
	r.nlh.nlmsg_seq = ++seq;
	r.g.rtgen_family =  AF_UNSPEC;

	// send
	int rv = send(sock, (void*)&r, sizeof(r), 0);
	if (rv < 0)
		return 1;
	return 0;
}

   
static int nlrq_receive(int sock, nlrq_filter filter, void *arg) {
	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	addr.nl_groups = 0;

	struct iovec iov;
	char   buffer[8192];
	iov.iov_base = buffer;

	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	while (1) {
		iov.iov_len = sizeof(buffer);
		int size = recvmsg(sock, &msg, 0);
		if (size <= 0) {
			if (errno == EINTR || errno == EAGAIN || errno == ENOBUFS)
				continue;
			else
				return 1;
		}

		struct nlmsghdr *h = (struct nlmsghdr*) buffer;
		while (size >= sizeof(struct nlmsghdr)) {
			int len = h->nlmsg_len;
			if ((len - sizeof(struct nlmsghdr)) < 0 ||
			     len > size)
				return 1;

			if (h->nlmsg_type == NLMSG_DONE)
				return 0;	// all done, this is the normal exit
			
			if (filter(h, arg))	// any error parsing the message will get us out of the function
				return 1;

			size -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
		}

		if (msg.msg_flags & MSG_TRUNC)
			continue;

		if (size)
			return 1;
	}
	
	return 1;
}

// return 1 if error, 0 if ok
static int decode_msg(struct nlmsghdr *n, int rip, int ospf, int rstatic, int rdefault) {
	if (n->nlmsg_type != RTM_NEWROUTE)
		return 0;
	struct rtmsg *info = NLMSG_DATA(n);
	if (info->rtm_family != AF_INET)
		return 0;
	if (info->rtm_type != RTN_UNICAST &&
	     info->rtm_type != RTN_BLACKHOLE &&
	     info->rtm_type != RTN_PROHIBIT)	// todo: get rid of prohibit
		return 0; // we don't care about this route
		
	uint32_t ip = 0;
	uint32_t mask_len = info->rtm_dst_len;
	uint32_t gw = 0;
	uint32_t input_metric = 0;

	uint32_t source_ip;
	int ifindex = 1;

	struct rtattr *attr = (struct rtattr *) RTM_RTA(info);
	int payload = RTM_PAYLOAD(n);
	struct in_addr *a;

	for (;RTA_OK(attr, payload); attr = RTA_NEXT(attr, payload)) {
		switch(attr->rta_type) {
			case RTA_DST:
				a = (struct in_addr *)RTA_DATA(attr);
				ip = ntohl(a->s_addr);
				break;
			case RTA_SRC:
				a = (struct in_addr *)RTA_DATA(attr);
				source_ip = ntohl(a->s_addr);
				break;
			case RTA_GATEWAY:
				a = (struct in_addr *)RTA_DATA(attr);
				gw = ntohl(a->s_addr);
				break;
			case RTA_OIF:
				ifindex = *((int *) RTA_DATA(attr));
				break;
//			case RTA_PREFSRC:
//				a = (struct in_addr *)RTA_DATA(attr);
//				printf("prefix src %d.%d.%d.%d\n", RCP_PRINT_IP(ntohl(a->s_addr)));
//				break;
			case RTA_PRIORITY:
				input_metric = *((unsigned *) RTA_DATA(attr));
				break;
//			case RTA_TABLE:
//				printf("table  %u\n", *((unsigned*) RTA_DATA(attr)));
//				break;
			default:
				break;
		}
	}

//printf("%d.%d.%d.%d/%d %d.%d.%d.%d metric %d\n",
//	RCP_PRINT_IP(ip), mask_len, RCP_PRINT_IP(gw), input_metric);
//printf("protocol %u, scop %u\n", info->rtm_protocol, info->rtm_scope);

	(void) ifindex;
	(void) source_ip;
	
	uint32_t distance;
	uint32_t metric;
	distance = (input_metric & 0xff000000) >> 24;
	metric = input_metric & 0x00ffffff;
	
	if ((info->rtm_protocol == 3 || info->rtm_protocol == 4) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// static route
		if (rstatic && ip != 0 && mask_len != 0) {
			printf("deleting static route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
		if (rdefault && ip == 0 && mask_len == 0) {
			printf("deleting default route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
	}
	else if ((info->rtm_protocol == RCP_RIP_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		if (rip ) {
			printf("deleting RIP route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		if (ospf) {
			printf("deleting OSPF route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_IA_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		if (ospf) {
			printf("deleting OSPF route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_E1_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		if (ospf) {
			printf("deleting OSPF route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_E2_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		if (ospf) {
			printf("deleting OSPF route %d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d\n",
				RCP_PRINT_IP(ip), mask_len,
				distance, metric,
				RCP_PRINT_IP(gw));
			add_del_route(ip, mask_len, gw, 0, 0, 1);				
		}
	}
	
	return 0;
}

static int process_table(int rip, int ospf, int rstatic, int rdefault) {
	FILE *fp;
	unsigned groups = RTMGRP_IPV4_ROUTE;	  //nl_mgrp(RTNLGRP_IPV4_ROUTE);

	// generate a temporary file
	char fname[50] = "/opt/rcp/var/transport/rXXXXXX";
	int fd = mkstemp(fname);
	if (fd == -1) {
		ASSERT(0)
		return 1;
	}
	close(fd);

	// open the temporary file
	fp = fopen(fname, "w");
	if (fp == NULL)
		return 1;

	// store kernel routes in the temporary file
	int sock = nlrq_open(groups);
	if (sock == 0)
		return 1;
	if (nlrq_getroutes(sock))
		return 1;
	if (nlrq_receive(sock, store_msg, (void*)fp) < 0)
		return 1;
	fclose(fp);
	close(sock);


	// open temporary file
	fp = fopen(fname, "r");
	if (fp == NULL)
		return 1;
	
	while (1) {
		// read the routes one by one from the temporary file
#define READ_BUFSIZE 1024
		uint32_t len;
		union {
			struct nlmsghdr n;
			uint8_t buf[READ_BUFSIZE];
		} u;

		int rv = fread(&len, 1, 4, fp);
		if (rv != 4)
			break;
		if (len < sizeof(struct nlmsghdr) || len > READ_BUFSIZE)
			return 1;
		
		memcpy(u.buf, &len, 4);
		rv = fread(u.buf + 4, 1, len - 4, fp);
		if (rv != (len -4))
			return 1;
		
		//print route
		if (decode_msg(&u.n, rip, ospf, rstatic, rdefault))
			return 1;
	}
	
	fclose(fp);
	unlink(fname);
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

static void usage(void) {
	printf("rtclean - clean protocol routes\n");
	printf("Usage: rtclean ospf | rip | static | default\n");
}

int main(int argc, char **argv) {
	if (argc != 2) {
		usage();
		return 1;
	}
	
	if (strcmp(argv[1], "ospf") == 0)
		process_table(0, 1, 0, 0);
	else if (strcmp(argv[1], "rip") == 0)
		process_table(1, 0, 0, 0);
	else if (strcmp(argv[1], "static") == 0)
		process_table(0, 0, 1, 0);
	else if (strcmp(argv[1], "default") == 0)
		process_table(0, 0, 0, 1);
	else {
		usage();
		return 1;
	}
		
	return 0;
}
