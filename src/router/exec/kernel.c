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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include "router.h"

 //****************************************************************
// Kernel handling
//****************************************************************
// return 1 if error, 0 if OK
int kernel_set_arp(uint32_t ip, unsigned char *mac) {
//	printf("ROUTER: set kernel ARP\n");
	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if (sock < 0) {
		return 1;
	}
			
	struct arpreq req;
	memset(&req, 0, sizeof(req));
	req.arp_ha.sa_family = ARPHRD_ETHER;
	memcpy(&req.arp_ha.sa_data, mac, 6) ;
	req.arp_flags = ATF_PERM | ATF_COM;

	struct sockaddr_in *in = (struct sockaddr_in *) &req.arp_pa;
	in->sin_addr.s_addr = htonl(ip);
	in->sin_family = AF_INET;

	if (ioctl(sock, SIOCSARP, &req) < 0) {
		close(sock);
		return 1;
	}
	
	close(sock);

	rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_DEBUG, RLOG_FC_ROUTER,
			"%d.%d.%d.%d ARP set",
			RCP_PRINT_IP(ip));
	
	return 0;
}

int kernel_del_arp(uint32_t ip) {
//	printf("ROUTER: delete kernel ARP\n");
	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if (sock < 0) {
		return 1;
	}
	
	struct arpreq req;
	memset(&req, 0, sizeof(req));
	req.arp_ha.sa_family = ARPHRD_ETHER;

	struct sockaddr_in *in = (struct sockaddr_in *) &req.arp_pa;
	in->sin_addr.s_addr = htonl(ip);
	in->sin_family = AF_INET;

	if (ioctl(sock, SIOCDARP, &req) < 0) {
		close(sock);
		return 1;
	}
	
	close(sock);

	rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_DEBUG, RLOG_FC_ROUTER,
			"%d.%d.%d.%d ARP deleted",
			RCP_PRINT_IP(ip));
	return 0;
}


int kernel_set_if_flags(int up, const char *ifname) {
//	printf("ROUTER: set kernel interface flags\n");
	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if (sock < 0) {
		return 1;
	}
			
	struct ifreq ifr;
	strcpy(ifr.ifr_name, ifname);
	// read the existing flags
	if (ioctl( sock, SIOCGIFFLAGS, &ifr ) < 0) {
		close(sock);
		return 1;
	}
	
	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;
	
	// set the new flags
	if (ioctl( sock, SIOCSIFFLAGS, &ifr ) < 0) {
		close(sock);
		return 1;
	}
	close(sock);
	
	if (up)
		kernel_update_interface(ifname);
		
	return 0;
}

// return 1 if error, 0 if OK
int kernel_set_if_ip(uint32_t ip, uint32_t mask, const char *ifname) {
//	printf("ROUTER: set kernel interface address\n");
	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if (sock < 0) {
		return 1;
	}
			
	struct ifreq ifr;
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_addr.sa_family = AF_INET;

	((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr = htonl(ip);
	if (ioctl( sock, SIOCSIFADDR, &ifr ) < 0) {
		close(sock);
		return 1;
	}

	if (ip != 0) {
		((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr =  htonl(mask);
		if (ioctl( sock, SIOCSIFNETMASK, &ifr ) < 0) {
			close(sock);
			return 1;
		}
	}
	
	close(sock);

	rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_INFO, RLOG_FC_INTERFACE,
			"interface %s configured, IP address %d.%d.%d.%d/%d",
			ifname, RCP_PRINT_IP(ip), mask2bits(mask));
	return 0;
}

int kernel_set_if_mtu(int mtu, const char *ifname) {
//	printf("ROUTER: set kernel interface MTU\n");
	int sock = socket(AF_INET,SOCK_DGRAM,0);
	if (sock < 0) {
		return 1;
	}
			
	struct ifreq ifr;
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_mtu = mtu;
	if (ioctl( sock, SIOCSIFMTU, &ifr ) < 0) {
		close(sock);
		return 1;
	}
	
	close(sock);
	return 0;
}

int kernel_update_interface(const char *ifname) {
//	printf("ROUTER: update kernel interface\n");
	int fd;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		ASSERT(0);
		return RCPERR;
	}


	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		ASSERT(0);
		return RCPERR;
	}


	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name,  ifname);

	// interface hardware address
	if (-1 < ioctl (fd, SIOCGIFHWADDR, &ifr))
		memcpy(pif->mac, (unsigned char *) &ifr.ifr_hwaddr.sa_data, 6);

	// mtu
	if (-1 < ioctl(fd, SIOCGIFMTU, &ifr))
		pif->mtu =  ifr.ifr_mtu;
	close(fd);
	return 0;
}			
