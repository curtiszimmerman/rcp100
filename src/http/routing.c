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
#include "http.h"
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <errno.h>

uint32_t seq;
typedef int (*nlrq_filter)(struct nlmsghdr *n, void *);


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

static void print_route(char *type, uint32_t *gw, int gw_cnt,
			uint32_t ip, int mask_len, 
			uint32_t distance, uint32_t metric, char **interface) {
			
	ASSERT(gw_cnt != 0);
	printf("%s%d.%d.%d.%d/%d[%u/%u] via %d.%d.%d.%d, %s\n",
		type, RCP_PRINT_IP(ip), mask_len,
		distance, metric,
		RCP_PRINT_IP(gw[0]),  interface[0]);

	char prefix[100];
	sprintf(prefix, "%s%d.%d.%d.%d/%d[%u/%u]",
		type, RCP_PRINT_IP(ip), mask_len,
		distance, metric);
	int prefix_len = strlen(prefix);

	int i;
	for (i = 1; i < gw_cnt; i++) {
		printf("%*s via %d.%d.%d.%d, %s\n",
			prefix_len, "", RCP_PRINT_IP(gw[i]),  interface[i]);
	}
	
}
// route counters
static int cnt_static = 0;
static int cnt_connected = 0;
static int cnt_rip = 0;
static int cnt_blackhole = 0;
static int cnt_ospf = 0;

// return 1 if error, 0 if ok
static int decode_msg(struct nlmsghdr *n, int cnt, int rconnected, int rstatic, int blackhole, int rip, int ospf) {
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
#define MAX_ECMP 64
	uint32_t gw[MAX_ECMP];
	int gw_cnt = 0;
	uint32_t input_metric = 0;

	uint32_t source_ip;
	int ifindex[MAX_ECMP];
	ifindex[0] = 1;

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
				(void) source_ip;
				break;
			case RTA_GATEWAY:
				a = (struct in_addr *)RTA_DATA(attr);
				gw[0] = ntohl(a->s_addr);
				gw_cnt = 1;
				break;
			case RTA_OIF:
				ifindex[0] = *((int *) RTA_DATA(attr));
				gw_cnt = 1;
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
			case RTA_MULTIPATH:
			{
				int len = RTA_PAYLOAD(attr);
				struct rtnexthop *rtnh = (struct rtnexthop *) RTA_DATA(attr);
				while (len > 0) {
					if (RTNH_OK(rtnh,len)) {
						// expecting an rtattr of length 8 and type RTA_GATEWAY
						struct rtattr *mattr = (struct rtattr *) RTNH_DATA(rtnh);
						if (mattr->rta_len == 8 && mattr->rta_type) {
							a = (struct in_addr *)RTA_DATA(mattr);
							gw[gw_cnt] = ntohl(a->s_addr);
							ifindex[gw_cnt] = rtnh->rtnh_ifindex;
							if (++gw_cnt >= MAX_ECMP)
								break;
						}						
				
						len -= rtnh->rtnh_len;
						rtnh = RTNH_NEXT(rtnh);
					}
					else
						break;
				}
			} break;
			default:
				break;
		}
	}
//	printf("%d.%d.%d.%d/%d %d.%d.%d.%d metric %d\n",
//		RCP_PRINT_IP(ip), mask_len, RCP_PRINT_IP(gw), input_metric);
	
	uint32_t distance;
	uint32_t metric;
	distance = (input_metric & 0xff000000) >> 24;
	metric = input_metric & 0x00ffffff;
	
	// find output interface
	char *interface[MAX_ECMP];
	{
		int i;
		for (i = 0; i < gw_cnt; i++) {
			RcpInterface *intf = rcpFindInterfaceByKIndex(shm, ifindex[i]);
			if (intf != NULL)
				interface[i] = intf->name;
			else
				interface[i] = "";
		}
	}
	
	if (info->rtm_protocol == 2 && info->rtm_scope == RT_SCOPE_LINK) {
		// connected route
		cnt_connected++;
		if (rconnected && !cnt) {
			printf("C    %d.%d.%d.%d/%d is directly connected, %s\n",
				RCP_PRINT_IP(ip), mask_len, interface[0]);
		}
	}
	else if (info->rtm_type == RTN_BLACKHOLE || info->rtm_type == RTN_PROHIBIT) {
		if (info->rtm_protocol == RCP_OSPF_KERNEL_PROTO)
			cnt_ospf++;
		else
			cnt_blackhole++;
			

		if (ospf && !cnt && info->rtm_protocol == RCP_OSPF_KERNEL_PROTO) {
			printf("O B  %d.%d.%d.%d/%d\n",
				RCP_PRINT_IP(ip), mask_len);
		
		}
		else if (blackhole && !cnt) {
			printf("B    %d.%d.%d.%d/%d\n",
				RCP_PRINT_IP(ip), mask_len);
		}
	}
	else if ((info->rtm_protocol == 3 || info->rtm_protocol == 4) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// static route
		cnt_static++;
		if (rstatic && !cnt) {
			print_route("S    ", gw, gw_cnt, ip, mask_len, distance, metric, interface);
		}
	}
	else if ((info->rtm_protocol == RCP_RIP_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// connected route
		cnt_rip++;
		if (rip && !cnt) {
			print_route("R    ", gw, gw_cnt, ip, mask_len, distance, metric, interface);
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// connected route
		cnt_ospf++;
		if (ospf && !cnt) {
			print_route("O    ", gw, gw_cnt, ip, mask_len, distance, metric, interface);
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_IA_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// connected route
		cnt_ospf++;
		if (ospf && !cnt) {
			print_route("O IA ", gw, gw_cnt, ip, mask_len, distance, metric, interface);
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_E1_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// connected route
		cnt_ospf++;
		if (ospf && !cnt) {
			print_route("O E1 ", gw, gw_cnt, ip, mask_len, distance, metric, interface);
		}
	}
	else if ((info->rtm_protocol == RCP_OSPF_E2_KERNEL_PROTO) &&
	              info->rtm_scope == RT_SCOPE_UNIVERSE) {
		// connected route
		cnt_ospf++;
		if (ospf && !cnt) {
			print_route("O E2 ", gw, gw_cnt, ip, mask_len, distance, metric, interface);
		}
	}
	
	return 0;
}


static int print_routes(int cnt, int rconnected, int rstatic, int blackhole, int rip, int ospf) {
	FILE *fp;
	unsigned groups = RTMGRP_IPV4_ROUTE;	  //nl_mgrp(RTNLGRP_IPV4_ROUTE);

	// generate a temporary file
	char fname[50] = "/opt/rcp/var/transport/tXXXXXX";
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

	// print headers
	if (cnt == 0) {
		printf("<b>Codes: C - connected, S - static, R - RIP, B - blackhole, O - OSPF,\n");
		printf("IA - OSPF inter area, E1 - OSPF external type 1,\n");
		printf("E2 - OSPF external type 2</b>\n");
		printf("\n");
	}
	else
		printf("<a href=\"routing.html;%u\">Routing Table</a>\n\n<b>Type\t\tCount</b>\n", html_session());

	// open temporary file
	fp = fopen(fname, "r");
	if (fp == NULL)
		return 1;
	
	// reset route counters
	cnt_static = 0;
	cnt_connected = 0;
	cnt_rip = 0;
	cnt_blackhole = 0;
	cnt_ospf = 0;
	
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
		if (decode_msg(&u.n, cnt, rconnected, rstatic, blackhole, rip, ospf))
			return 1;
	}
	
	if (cnt) {
		printf("Connected\t%d\n", cnt_connected);
		printf("Static\t\t%d\n", cnt_static);
		printf("RIP\t\t%d\n", cnt_rip);
		printf("OSPF\t\t%d\n", cnt_ospf);
		printf("Blackhole\t%d\n", cnt_blackhole);
		int total = cnt_connected + cnt_static + cnt_rip + cnt_blackhole + cnt_ospf;
		if (total == 1)
			printf("%d route total\n", total);
		else
			printf("%d routes total\n", total);
	}

	fclose(fp);
	unlink(fname);
	return 0;
}




#define LINE_BUF 1000
static void print_arp(int cntonly) {
	int cnt = 0;
	
	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp == NULL)
		return;
	
	char line[LINE_BUF + 1];
	if (fgets(line, LINE_BUF, fp) == NULL) {
		fclose(fp);
		return;
	}
	if (!cntonly)
		printf("<b>%s</b>", line);
	
	while (fgets(line, LINE_BUF, fp)) {
		char ipstr[32];
		unsigned type;
		unsigned flags;
		int rv = sscanf(line, "%s 0x%x 0x%x", ipstr, &type, &flags);
		if (rv != 3) {
			break;
		}
		
		if ((flags & ATF_COM) == 0)
			continue;
			
		if (!cntonly)
			printf("%s", line);
		cnt++;
	}
	fclose(fp);
	
	if (cntonly == 1) {
		if (cnt == 1)
			printf("<a href=\"routing-arp.html;%u\">ARP Table</a>: %u entry\n", html_session(), cnt);
		else if (cnt > 1 || cnt <= 0)
		printf("<a href=\"routing-arp.html;%u\">ARP Table</a>: %u entries\n", html_session(), cnt);
	}
}

void routing_table_summary(void) {
	print_routes(1, 1, 1, 1, 1, 1);
}

void routing_table_full(void) {
	print_routes(0, 1, 1, 1, 1, 1);
}

void routing_arp_summary(void) {
	print_arp(1);
}

void routing_arp_full(void) {
	print_arp(0);
}

void routing_ospf_summary(void) {
	int configured = 0;
	RcpOspfArea *area = shm->config.ospf_area;
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++, area++) {
		if (area->valid) {
			configured = 1;
			break;
		}
	}
	
	if (configured) {
		printf("<a href=\"routing-ospf.html;%u\">OSPF</a> protocol enabled\n", html_session());
		int cnt = count_active_neighbors(1);
		if (cnt == 1)
			printf("%d active neighbor\n", cnt);
		else if (cnt > 1)
			printf("%d active neighbors\n", cnt);
		printf("<a href=\"routing-ospf-database.html;%u\">Database</a>&nbsp;&nbsp;"
			"<a href=\"routing-ospf-neighbors.html;%u\">Neighbors</a>\n",
			html_session(), html_session());
	}
	else
		printf("OSPF protocol disabled\n");
}

void routing_rip_summary(void) {
	int configured = 0;
	RcpRipPartner *part = shm->config.rip_network;
	int i;
	for (i = 0; i < RCP_RIP_NETWORK_LIMIT; i++, part++) {
		if (part->valid) {
			configured = 1;
			break;
		}
	}
	if (!configured) {
		part = shm->config.rip_neighbor;
		for (i = 0; i < RCP_RIP_NEIGHBOR_LIMIT; i++, part++) {
			if (part->valid) {
				configured = 1;
				break;
			}
		}
	}
	
	if (configured) {
		printf("<a href=\"routing-rip.html;%u\">RIP</a> protocol enabled\n", html_session());
		int cnt = count_active_neighbors(0);
		if (cnt == 1)
			printf("%d active neighbor\n", cnt);
		else if (cnt > 1)
			printf("%d active neighbors\n", cnt);
		printf("<a href=\"routing-rip-database.html;%u\">Database</a>\n",
			html_session());
	}
	else
		printf("RIP protocol disabled\n");
}

void routing_ospf(void) {
	printf("<blockquote><pre>\n");
	// general data
	printf("OSPF router id %d.%d.%d.%d\n", RCP_PRINT_IP(shm->config.ospf_router_id));
	printf("Conforms to RFC2328, RFC1583 Compatibility flag is disabled\n");
	printf("Supports only single TOS(TOS0) routes\n");
	if (shm->stats.ospf_is_abr)
		printf("This is an Area Border Router\n");
	if (shm->stats.ospf_is_asbr)
		printf("This is an AS Border Router\n");

	// spf
	printf("SPF schedule delay %u seconds, hold time %u seconds\n",
		shm->config.ospf_spf_delay, shm->config.ospf_spf_hold);
	printf("Number of SPF calculations %u\n",
		shm->stats.ospf_spf_calculations);
	if (shm->stats.ospf_last_spf)
		printf("Last SPF calculation %s", ctime(&shm->stats.ospf_last_spf));

	// lsa
	printf("Number of external LSA %d\n", shm->stats.ospf_external_lsa);
	printf("Number of LSA originated %d\n", shm->stats.ospf_originated_lsa);
	
	printf("</pre></blockquote>\n");
	printf("<br/><br/>\n");
}

static void print_net(RcpRipPartner *net) {
	char network[20];
	if (net->mask == 0xffffffff)
		sprintf(network, "%d.%d.%d.%d",
			RCP_PRINT_IP(net->ip));
	else
		sprintf(network, "%d.%d.%d.%d/%d",
			RCP_PRINT_IP(net->ip), mask2bits(net->mask));

	char req[24];
	sprintf(req, "%u/%u", net->req_rx, net->req_tx);
	char resp[24];
	sprintf(resp, "%u/%u", net->resp_rx, net->resp_tx);
		
	printf("%-19s%-20.20s%-20.20s\n",
		network, req, resp);
}

void routing_rip(void) {
	printf("<blockquote><pre>\n");
	printf("<b>%-19s%-20.20s%-20.20s</b>\n",
		"Network", "Request (Rx/Tx)", "Response (Rx/Tx)");

	// print configured networks
	RcpRipPartner *net;
	int i;
	for (i = 0, net = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, net++) {
		if (!net->valid)
			continue;
		print_net(net);
	}
	
	printf("</pre></blockquote>\n");
}
