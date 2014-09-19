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
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "router.h"
#include <linux/sockios.h>
#include <linux/if_vlan.h>

//*************************************************************
// RCP messages
//*************************************************************

static void send_trap(RcpInterface *intf, int link_up, int admin_up) {
	// build snmp traps
	char msg[300];
	if (link_up)
		strcpy(msg, ".1.3.6.1.6.3.1.1.5.4 ");	// linkUp
	else
		strcpy(msg, ".1.3.6.1.6.3.1.1.5.3 ");	// linkDown

	// add interface index and name
	char *ptr = msg + strlen(msg);
	int admin = (admin_up)? 1: 2;
	int link = (link_up)? 1: 2;
	sprintf(ptr, ".1.3.6.1.2.1.2.2.1.1.%d i %d .1.3.6.1.2.1.2.2.1.2.%d s %s .1.3.6.1.2.1.2.2.1.7.%d i %d .1.3.6.1.2.1.2.2.1.8.%d i %d ",
		intf->kindex, intf->kindex,	// interface index
		intf->kindex, intf->name,	// interface name
		intf->kindex, admin, // admin status
		intf->kindex, link);	// operational status
	
	// send trap
	rcpSnmpSendTrap(msg);
}



static void send_if_message_up_down(RcpInterface *intf) {
	// for loopback interfaces do nothing
	if (intf->type == IF_LOOPBACK)
		return;

	// send rcp message to clients
	if (muxsock) {
//printf("send message interface %s %s\n", intf->name, (intf->link_up)? "UP": "Down");		
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.destination = RCP_PROC_INTERFACE_NOTIFICATION_GROUP;
		pkt.pkt.interface.interface = intf;
		
		if (intf->link_up)
			pkt.type = RCP_PKT_TYPE_IFUP;
		else
			pkt.type = RCP_PKT_TYPE_IFDOWN;
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}
	
	// send snmp traps
	send_trap(intf, intf->link_up, intf->admin_up);
}

static void send_loopback_message(uint32_t ip, uint32_t mask, uint8_t state) {
	// send rcp message to clients
	if (muxsock) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.type = RCP_PKT_TYPE_LOOPBACK;
		pkt.destination = RCP_PROC_INTERFACE_NOTIFICATION_GROUP;
		pkt.pkt.loopback.ip = ip;
		pkt.pkt.loopback.mask = mask;
		pkt.pkt.loopback.state = state;
//printf("send message interface loopback %d.%d.%d.%d/%d, state %d\n", RCP_PRINT_IP(ip), mask2bits(mask), state);		
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}	
}

static void send_vlan_deleted_message(RcpInterface *intf) {
	// send rcp message to clients
	if (muxsock) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.type = RCP_PKT_TYPE_VLAN_DELETED;
		pkt.destination = RCP_PROC_INTERFACE_NOTIFICATION_GROUP | RCP_PROC_SERVICES;
		pkt.pkt.vlan.interface = intf;
//printf("send message interface vlan deleted %d.%d.%d.%d/%d\n", RCP_PRINT_IP(ip), mask2bits(mask));		
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}
	
}

static void send_if_message_changed(RcpInterface *intf, uint32_t old_ip, uint32_t old_mask) {
	// for loopback interfaces do nothing
	if (intf->type == IF_LOOPBACK)
		return;
		
	// send rcp message to clients
	if (muxsock) {
		RcpPkt pkt;
		memset(&pkt, 0, sizeof(pkt));
		if (old_ip != 0 && old_mask != 0)
// RIP handled differently
			pkt.destination = RCP_PROC_OSPF | RCP_PROC_SERVICES;
		else
			pkt.destination = RCP_PROC_SERVICES;
		pkt.pkt.interface.interface = intf;
		pkt.pkt.interface.old_ip = old_ip;
		pkt.pkt.interface.old_mask = old_mask;
		pkt.type = RCP_PKT_TYPE_IFCHANGED;
//printf("send message interface %s changed\n", intf->name);		
		
		errno = 0;
		int n = send(muxsock, &pkt, sizeof(pkt), 0);
		if (n < 1) {
			close(muxsock);
			muxsock = 0;
//			printf("Error: process %s, muxsocket send %d, errno %d, disconnecting...\n",
//				rcpGetProcName(), n, errno);
		}
	}	
}


//*************************************************************
// Shared memory handling
//*************************************************************
// return -1 if error
// return interface index, from 0 to RCP_INTERFACE_LIMITS - 1
static int shm_find_or_add_intf(const char *name) {
	int i;
	int found = 0;

	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0' &&
		     strcmp(shm->config.intf[i].name, name) == 0) {
		     	found = 1;
		     	break;
		}
	}     

	if (found)
		return i;

	// add the new interface in shared memory
	int empty = -1;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] == '\0') {
			empty = i;
			break;
		}
	}
				
	if (empty == -1)
		return -1;
	memset(&shm->config.intf[empty], 0, sizeof(RcpInterface));
	rcpInterfaceSetDefaults(&shm->config.intf[empty]);
	strncpy(shm->config.intf[empty].name, name, RCP_MAX_IF_NAME);
	return empty;
}


static void intf_set_flags(int index, struct ifaddrs *ifa) {
	ASSERT(ifa);
	ASSERT(index < RCP_INTERFACE_LIMITS);
	
	if (ifa->ifa_flags & IFF_UP)
		shm->config.intf[index].admin_up = 1;
	else
		shm->config.intf[index].admin_up = 0;

	if (ifa->ifa_flags & IFF_RUNNING)
		shm->config.intf[index].link_up = 1;
	else
		shm->config.intf[index].link_up = 0;

	if (ifa->ifa_flags & IFF_LOOPBACK)
		shm->config.intf[index].type = IF_LOOPBACK;
	else
		shm->config.intf[index].type = IF_ETHERNET;
}

void intf_set_data(int index, FILE *fp) {
	ASSERT(index < RCP_INTERFACE_LIMITS);
	const char *name = shm->config.intf[index].name;
	
	int fd;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Error: process %s, cannot open AF_INET socket, exiting...\n", rcpGetProcName());
		exit(1);
	}
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name,  name, RCP_MAX_IF_NAME);

	// interface hardware address
	if (-1 < ioctl (fd, SIOCGIFHWADDR, &ifr))
		memcpy(shm->config.intf[index].mac, (unsigned char *) &ifr.ifr_hwaddr.sa_data, 6);

	// mtu
	if (-1 < ioctl(fd, SIOCGIFMTU, &ifr))
		shm->config.intf[index].mtu =  ifr.ifr_mtu;
		
	// kernel interface index
	if (-1 < ioctl (fd, SIOCGIFINDEX, &ifr))
		shm->config.intf[index].kindex =  ifr.ifr_ifindex;

	if (fp != NULL)
		fprintf(fp, "ROUTER: Detecting interface %s, kernel index %d, rcp index %d, ip %d.%d.%d.%d, mask %d.%d.%d.%d\n",
			name,
			shm->config.intf[index].kindex,
			index,
			RCP_PRINT_IP(shm->config.intf[index].ip),
			RCP_PRINT_IP(shm->config.intf[index].mask));

	// check vlan
	struct vlan_ioctl_args vlanreq;		
	memset(&vlanreq, 0, sizeof(vlanreq));
	strncpy(vlanreq.device1, name, RCP_MAX_IF_NAME);
	vlanreq.cmd = GET_VLAN_REALDEV_NAME_CMD;
	if (ioctl(fd, SIOCGIFVLAN, &vlanreq) >=  0) {
		// find parent index
		int parent_index = shm_find_or_add_intf(vlanreq.u.device2);
		
		// get vlan id
		struct vlan_ioctl_args vlanreq2;
		memset(&vlanreq2, 0, sizeof(vlanreq2));
		strncpy(vlanreq2.device1, name, RCP_MAX_IF_NAME);
		vlanreq2.cmd = GET_VLAN_VID_CMD;
		if (ioctl(fd, SIOCGIFVLAN, &vlanreq2) >= 0) {
			uint16_t vlan_id = vlanreq2.u.VID;
			if (fp)
				fprintf(fp, "ROUTER: %s is a vlan, parent %s (%d), vlan id %u\n", name, vlanreq.u.device2, parent_index, vlan_id);
			shm->config.intf[index].type = IF_VLAN;
			shm->config.intf[index].vlan_id = vlan_id;
			shm->config.intf[index].vlan_parent = parent_index;
		}
	}
	
	close(fd);
}

//*********************************************************************************
// Kernel handling
//*********************************************************************************
void router_detect_interfaces(void) {
	struct ifaddrs *ifaddr;
	ASSERT(shm != NULL);

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(1);
	}

	
	// log all actions in /var/log
	FILE *fp = fopen("/opt/rcp/var/log/router", "w");
	int i;
	for (i = 0; i < intremoved_cnt; i++)
		if (fp != NULL)
			fprintf(fp, "ROUTER: disregarding interface %s\n", intremoved[i]);
	

	struct ifaddrs *ifa = ifaddr;
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != NULL) {
			// implementing interface remove option -r
			int i;
			int found = 0;
			for (i = 0; i < intremoved_cnt; i++) {
				if (strncmp(ifa->ifa_name, intremoved[i], strlen(intremoved[i])) == 0) {
					found = 1;
					break;
				}
			}
			if (found)
				continue;
			
			int family = ifa->ifa_addr->sa_family;
			if (family == AF_PACKET) {
				// this is an unconfigured interface - we just need to add the interface if it is not in shared memory
				if (fp != NULL)
					fprintf(fp, "ROUTER: stats entry for %s, flags %x\n", ifa->ifa_name, ifa->ifa_flags);
				// no L3 tunnels
				if (ifa->ifa_flags & IFF_NOARP) {
					continue;
				}
					
				int idx = shm_find_or_add_intf(ifa->ifa_name);
				intf_set_flags(idx, ifa);
				if (fp != NULL)
					fprintf(fp, "ROUTER: added interface %s at index %d\n", ifa->ifa_name, idx);
					
				continue;
			}
			else if (family != AF_INET /* || family == AF_INET6 */) {
				if (fp != NULL)
					fprintf(fp, "ROUTER: unprocessed entry for %s\n", ifa->ifa_name);
				continue;
			}

			int index = shm_find_or_add_intf(ifa->ifa_name);
			if (index == -1)
				continue;
				
			struct sockaddr_in *si = (struct sockaddr_in *) ifa->ifa_netmask;
			shm->config.intf[index].mask = ntohl(si->sin_addr.s_addr);
			si = (struct sockaddr_in *) ifa->ifa_addr;
			shm->config.intf[index].ip = ntohl(si->sin_addr.s_addr);

			// set  interface
			intf_set_flags(index, ifa);
			intf_set_data(index, fp);
		}
		else {
//			printf("ROUTER: unprocessed entry, no interface name\n");
		}
	}
	freeifaddrs(ifaddr);
	
	// get current interface counts	
	RcpIfCounts *ifcounts = rcpGetInterfaceCounts();

	// make sure we have the kernel index for all interfaces
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (*shm->config.intf[i].name == '\0')
			continue;

		// mark the bridge interfaces
		if (strcmp(shm->config.intf[i].name, "br0") == 0 ||
		    strcmp(shm->config.intf[i].name, "br1") == 0) {
			shm->config.intf[i].type = IF_BRIDGE;
			if (fp != NULL)
				fprintf(fp, "ROUTER: %s is a bridge interface\n", shm->config.intf[i].name);
		}

		// initialize rx/tx counters
		RcpInterface *intf = &shm->config.intf[i];
		RcpIfCounts *ptr = ifcounts;
		RcpIfCounts *found = NULL;
		while (ptr) {
			if (strcmp(intf->name, ptr->name) == 0) {
				found = ptr;
				break;
			}
			ptr = ptr->next;
		}
		if (found) {
			intf->stats.current_tx_bytes = found->tx_bytes;
			intf->stats.current_rx_bytes = found->rx_bytes;
			if (fp)
				fprintf(fp, "ROUTER: Interface %s stats initialized to %llu/%llu (rx/tx bytes)\n",
					intf->name,
					intf->stats.current_rx_bytes,
					intf->stats.current_tx_bytes);
		}

		// configure kindex if necessary
		if (shm->config.intf[i].kindex == 0)
			intf_set_data(i, fp);
	}
	
	// free memory
	while (ifcounts) {
		RcpIfCounts *next = ifcounts->next;
		free(ifcounts);
		ifcounts = next;
	}
	
	if (fp != NULL)
		fclose(fp);
}



static int chk_intf_ip(RcpInterface *pif, uint32_t ip) {
	ASSERT(pif);
	int i;

	// make sure we have the kernel index for all interfaces
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		
		if (*intf->name == '\0')
			continue;
		if (intf == pif)
			continue;
		
		if (intf->ip == ip)
			return 1;
	}
	
	return 0;
}

//*************************************************************
// CLI commands
//*************************************************************


// interface ip address
int cliIpAddressCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	uint32_t ip;
	if (atoip(argv[2], &ip)) {
		strcpy(data, "Error: invalid address\n");
		return RCPERR;
	}
	
	uint32_t mask;
	if (atoip(argv[3], &mask)) {
		strcpy(data, "Error: invalid mask\n");
		return RCPERR;
	}
	
	// we don't allow 1.2.3.0 netmask 255.255.255.0
	if ((ip & ~mask) == 0 ||
	// we don't allow 1.2.3.255 netmask 255.255.255.0
	     (ip & ~mask) == ~mask) {
		strcpy(data, "Error: invalid address mask\n");
		return RCPERR;
	}
	
	     	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	// don't allow the same ip address on different interfaces
	if (chk_intf_ip(pif, ip)) {
		strcpy(data, "Error: the IP address is already in use\n");
		return RCPERR;
	}

	
	if (kernel_set_if_ip(ip, mask, ifname)) {
		strcpy(data, "Error: cannot set interface address\n");
		// put back the good addresses
		kernel_set_if_ip(pif->ip, pif->mask, ifname);
		return RCPERR;
	}

	// send loopback down message
	if (rcpInterfaceVirtual(pif->name) && pif->ip != 0)
		send_loopback_message(pif->ip, pif->mask, 0);

	// store the new configuration in shared memory
	uint32_t old_ip = pif->ip;
	uint32_t old_mask = pif->mask;
	pif->ip = ip;
	pif->mask = mask;
	
	// send rcp messages
	pif->link_up = 0;
	send_if_message_up_down(pif);
	send_if_message_changed(pif, old_ip, old_mask);
	if (rcpInterfaceVirtual(pif->name) && pif->ip != 0)
		send_loopback_message(pif->ip, pif->mask, 1);

	// update arps and routes
	router_update_if_arp(pif);
	router_update_if_route(pif);
	return 0;
}

int cliNoIpAddressCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	if (kernel_set_if_ip(0, 0, ifname)) {
		strcpy(data, "Error: cannot set interface address\n");
		// put back the good addresses
		kernel_set_if_ip(pif->ip, pif->mask, ifname);
		return RCPERR;
	}

	// send loopback down message
	if (rcpInterfaceVirtual(pif->name))
		send_loopback_message(pif->ip, pif->mask, 0);

	// store the new configuration in shared memory
	uint32_t old_ip = pif->ip;
	uint32_t old_mask = pif->mask;
	pif->ip = 0;
	pif->mask = 0;


	// send rcp message
	pif->link_up = 0;
	send_if_message_up_down(pif);
	send_if_message_changed(pif, old_ip, old_mask);

	return 0;
}

int cliIpAddressCidrCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
		
	uint32_t ip;
	uint32_t mask;
	if (atocidr(argv[2], &ip, &mask)) {
		strcpy(data, "Error: invalid address\n");
		return RCPERR;
	}
	
	if (mode != CLIMODE_INTERFACE_LOOPBACK) {
		// we don't allow 1.2.3.0 netmask 255.255.255.0
		if ((ip & ~mask) == 0 ||
		// we don't allow 1.2.3.255 netmask 255.255.255.0
		     (ip & ~mask) == ~mask) {
			strcpy(data, "Error: invalid address\n");
			return RCPERR;
		}
	}
	     	
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}

	// don't allow the same ip address on different interfaces
	if (chk_intf_ip(pif, ip)) {
		strcpy(data, "Error: the IP address is already in use\n");
		return RCPERR;
	}
	
	if (kernel_set_if_ip(ip, mask, ifname)) {
		strcpy(data, "Error: cannot set interface address\n");
		// put back the good addresses
		kernel_set_if_ip(pif->ip, pif->mask, ifname);
		return RCPERR;
	}

	// send loopback down message
	if (rcpInterfaceVirtual(pif->name) && pif->ip != 0)
		send_loopback_message(pif->ip, pif->mask, 0);

	// store the new configuration in shared memory
	uint32_t old_ip = pif->ip;
	uint32_t old_mask = pif->mask;
	pif->ip = ip;
	pif->mask = mask;

	// send rcp message
	pif->link_up = 0;
	send_if_message_up_down(pif);
	send_if_message_changed(pif, old_ip, old_mask);
	if (rcpInterfaceVirtual(pif->name) && pif->ip != 0)
		send_loopback_message(pif->ip, pif->mask, 1);
	
	// update arps and routes
	router_update_if_arp(pif);
	router_update_if_route(pif);
	return 0;
}

int cliMtuCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	int mtu = atoi(argv[2]);

	// extract the interface name
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}
	
	if (kernel_set_if_mtu(mtu, ifname)) {
		strcpy(data, "Error: cannot set MTU\n");
		return RCPERR;
	}
	
	pif->mtu = mtu;
	return 0;
}

int cliNoMtuCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// extract the interface name
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}
	
	if (kernel_set_if_mtu(1500, ifname)) {
		strcpy(data, "Error: cannot reset MTU\n");
		return RCPERR;
	}
	
	if (pif->type == IF_LOOPBACK)
		pif->mtu = 16436;
	else
		pif->mtu = 1500;
		
	return 0;
}

int cliShutdownCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract the interface name
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}
	
	if (kernel_set_if_flags(0, ifname)) {
		strcpy(data, "Error: cannot shutdown the interface\n");
		return RCPERR;
	}
	
	pif->admin_up = 0;
	return 0;
}

int cliNoShutdownCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	// extract the interface name
	ASSERT(pktout->pkt.cli.mode_data != NULL);
	char *ifname = pktout->pkt.cli.mode_data;
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: internal memory problem, attempting recovery...\n");
		return RCPERR;
	}
	
	if (kernel_set_if_flags(1, ifname)) {
		strcpy(data, "Error: cannot activate the interface\n");
		return RCPERR;
	}
	
	pif->admin_up = 1;
	return 0;
}


int cliNoInterfaceCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	char *ifname = argv[3];
	
	// find rpc interface
	RcpInterface *pif = rcpFindInterfaceByName(shm, ifname);
	if (pif == NULL) {
		strcpy(data, "Error: invalid interface name\n");
		return RCPERR;
	}

	// put the interface in shutdown mode
	if (pif->admin_up == 1) {
		pif->admin_up = 0;
		kernel_set_if_flags(0, ifname);
	}
	
	// default mtu configuration
	if (pif->mtu != 1500) {
		pif->mtu = 1500;
		kernel_set_if_mtu(1500, ifname);
	}
	
	// no ip address configuration
	uint32_t old_ip = pif->ip;
	uint32_t old_mask = pif->mask;
	if (pif->ip != 0) {
		pif->ip = 0;
		pif->mask = 0;
		kernel_set_if_ip(0, 0, ifname);
	}
	
	// rip configuration and dhcp configuration is left as is; rip and dhcp stats are not cleared
	//pif->rip_passive_interface = 0;
	//pif->dhcp_relay = 0;

	// send interface message	
	pif->link_up = 0;
	send_if_message_up_down(pif);
	send_if_message_changed(pif, old_ip, old_mask);
		
	return 0;		
}

//*********************************************************************************
// Interface up/down notification
//*********************************************************************************
// detecting an interface up/donw change
// this function is called periodically from the select loop
static int sock_if = 0;
void router_if_timeout(void) {
//http://www.mjmwired.net/kernel/Documentation/networking/operstates.txt
	if (sock_if <= 0) {
		sock_if = socket(AF_INET,SOCK_DGRAM,0);
		if (sock_if < 0) {
			ASSERT(0);
			return;
		}
	}
				
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));

	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0]  == '\0')
			continue;
		strcpy(ifr.ifr_name, shm->config.intf[i].name);
			
		// read the existing flags
		if (ioctl( sock_if, SIOCGIFFLAGS, &ifr ) < 0) {
			close(sock_if);
			sock_if = 0;
			return;
		}

//printf("%s %x\n", ifr.ifr_name, ifr.ifr_flags);
		int changed = 0;
		int up = (ifr.ifr_flags & IFF_UP)? 1: 0;
		if (shm->config.intf[i].admin_up != up) {
			changed = 1;
//			printf("Interface %s changed administrative state to %s\n",
//				shm->config.intf[i].name, (up)? "UP": "DOWN");
			rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_WARNING, RLOG_FC_INTERFACE,
				"interface %s changed administrative state to %s",
				shm->config.intf[i].name, (up)? "UP": "DOWN");
		

			shm->config.intf[i].admin_up = up;
		}
		
		up = (ifr.ifr_flags & IFF_RUNNING)? 1: 0;
		if (shm->config.intf[i].link_up != up) {
			changed = 1;
			
		rcpLog(muxsock, RCP_PROC_ROUTER, RLOG_WARNING, RLOG_FC_INTERFACE,
			"interface %s link changed state to %s",
			shm->config.intf[i].name, (up)? "UP": "DOWN");

//			printf("Interface %s link changed state to %s\n",
//				shm->config.intf[i].name, (up)? "UP": "DOWN");
			shm->config.intf[i].link_up = up;
		}
		
		
		if (changed) {
			// update kernel data for interface, routes, and arp
			RcpInterface *ptr = &shm->config.intf[i];
			kernel_set_if_ip(ptr->ip, ptr->mask, ptr->name);
			kernel_set_if_mtu(ptr->mtu, ptr->name);
			
			// send rcp message to clients
			send_if_message_up_down(ptr);

			// send static routes and arps to kernel if necessary
			if (ptr->link_up && ptr->admin_up) {
				router_update_if_arp(ptr);
				router_update_if_route(ptr);
			}
		}
	}
}


int cliInterfaceEthernetCmd(CliMode mode, int argc, char **argv) {
	ASSERT(mode == CLIMODE_CONFIG);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// find the interface
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, argv[2]) == 0 && shm->config.intf[i].type == IF_ETHERNET) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		sprintf(data, "Error: invalid interface name\n");
		return RCPERR;
	}

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_INTERFACE;
	pktout->pkt.cli.mode_data  = shm->config.intf[i].name;

	return 0;
}

int cliInterfaceLoopbackCmd(CliMode mode, int argc, char **argv) {
	ASSERT(mode == CLIMODE_CONFIG);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// find the port
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, argv[2]) == 0 && shm->config.intf[i].type == IF_LOOPBACK) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		sprintf(data, "Error: invalid interface name\n");
		return RCPERR;
	}

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_INTERFACE_LOOPBACK;
	pktout->pkt.cli.mode_data  = shm->config.intf[i].name;

	return 0;
}

int cliInterfaceUserLoopbackCmd(CliMode mode, int argc, char **argv) {
	ASSERT(mode == CLIMODE_CONFIG);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// find the interface
	char ifname[RCP_MAX_IF_NAME + 1];
	int loindex = atoi(argv[2]);
	sprintf(ifname, "lo:%d", loindex);

	// find/create the interface structure in shm
	int index = shm_find_or_add_intf(ifname);
	if (index == -1) {
		sprintf(data, "Error: cannot create loopback interface, limit reached\n");
		return RCPERR;
	}

	// configure interface
	if (shm->config.intf[index].ip == 0) {
		RcpInterface *intf = &shm->config.intf[index];
		intf->type = IF_LOOPBACK;
		intf->mtu = 16436;
		intf->admin_up = 1;
		intf->link_up = 1;
	
		// create the interface
		char cmd[100];
		sprintf(cmd, "ifconfig %s", ifname);
		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
	}
	
	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_INTERFACE_LOOPBACK;
	pktout->pkt.cli.mode_data  = shm->config.intf[index].name;

	return 0;
}

int cliNoInterfaceUserLoopbackCmd(CliMode mode, int argc, char **argv) {
	ASSERT(mode == CLIMODE_CONFIG);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// delete the interface
	char ifname[RCP_MAX_IF_NAME + 1];
	int loindex = atoi(argv[3]);
	sprintf(ifname, "lo:%d", loindex);

	// find the interface
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, ifname) == 0 && shm->config.intf[i].type == IF_LOOPBACK) {
			found = 1;
			break;
		}
	}
	
	// clear interface structure
	if (found) {
		send_loopback_message(shm->config.intf[i].ip, shm->config.intf[i].mask, 0);
		shm->config.intf[i].name[0] = '\0';
	
		char cmd[100];
		sprintf(cmd, "ifconfig %s down", ifname);
		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
	}

	return 0;
}

int cliInterfaceBridgeCmd(CliMode mode, int argc, char **argv) {
	ASSERT(mode == CLIMODE_CONFIG);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// find the interface
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, argv[2]) == 0 && shm->config.intf[i].type == IF_BRIDGE) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		sprintf(data, "Error: invalid interface name\n");
		return RCPERR;
	}

	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_INTERFACE_BRIDGE;
	pktout->pkt.cli.mode_data  = shm->config.intf[i].name;

	return 0;
}

int cliInterfaceVlanCmd(CliMode mode, int argc, char **argv) {
	ASSERT(mode == CLIMODE_CONFIG);
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	// check for a different type of interface with the same name	
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, argv[2]) == 0) {
			found = 1;
			break;
		}
	}
	if (found && shm->config.intf[i].type != IF_VLAN) {
		sprintf(data, "Error: another interface with the same name is already present\n");
		return RCPERR;
	}


	// check for an existing interface
	found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, argv[2]) == 0 && shm->config.intf[i].type == IF_VLAN) {
			found = 1;
			break;
		}
	}
	
	// extract interface data
	uint16_t vlan_id = 0;
	int j;	// parent index
	int pfound = 0;
	if (argc > 3) {
		unsigned tmp;
		sscanf(argv[4], "%u", &tmp);
		vlan_id = tmp;
		// find parent
		for (j = 0; j < RCP_INTERFACE_LIMITS; j++) {
			if (strcmp(shm->config.intf[j].name, argv[5]) == 0 &&
			    (shm->config.intf[j].type == IF_ETHERNET || shm->config.intf[j].type == IF_BRIDGE)) {
				pfound = 1;
				break;
			}
		}
	}
	
	
	if (found && argc == 3) 
		;
	else if (found && argc > 3) {
		// check interface data
		if (!pfound || shm->config.intf[i].vlan_id != vlan_id || shm->config.intf[i].vlan_parent != j) {
			sprintf(data, "Error: invalid vlan interface\n");
			return RCPERR;
		}
	}
	else if (!found && argc == 3) {
		sprintf(data, "Error: invalid vlan interface\n");
		return RCPERR;
	}
	else if (!found && argc > 3) {
		// create the new interface
		int index = shm_find_or_add_intf(argv[2]);
		if (index == -1) {
			sprintf(data, "Error: cannot create vlan interface, limit reached\n");
			return RCPERR;
		}
		
		// set shared memory structure
		shm->config.intf[index].type = IF_VLAN;
		shm->config.intf[index].vlan_id = vlan_id;
		shm->config.intf[index].vlan_parent = j;
		
		// set kernel interface
		char cmd[500];
		sprintf(cmd, "ip link add link %s name %s type vlan id %u",
			shm->config.intf[j].name,
			shm->config.intf[index].name,
			shm->config.intf[index].vlan_id);

		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
		i = index;
		intf_set_data(index, NULL);
	}
			
	// set the new cli mode
	pktout->pkt.cli.mode  = CLIMODE_INTERFACE_VLAN;
	pktout->pkt.cli.mode_data  = shm->config.intf[i].name;

	return 0;
}

int cliNoInterfaceVlanCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	char *ifname = argv[3];

	// find the interface
	int i;
	int found = 0;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (strcmp(shm->config.intf[i].name, ifname) == 0 && shm->config.intf[i].type == IF_VLAN) {
			found = 1;
			break;
		}
	}
	
	// clear interface structure
	if (found) {
		// send snmp traps
		send_trap(&shm->config.intf[i], 0, 0);
		
		shm->config.intf[i].name[0] = '\0'; // scrap the name before sending the message!
		// send message deleted
		send_vlan_deleted_message(&shm->config.intf[i]);
		
		char cmd[100];
		sprintf(cmd, "ip link delete %s", ifname);
		int v = system(cmd);
		if (v == -1)
			ASSERT(0);
		
		sleep(2);	// wait for the system to process the interface deleted message
	}

	return 0;
}	
