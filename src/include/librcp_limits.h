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
#ifndef LIBRCP_LIMITS_H
#define LIBRCP_LIMITS_H

#ifdef __cplusplus
extern "C" {
#endif

#define RCP_ADMIN_LIMIT 16	// maximum number of administrators
#define RCP_PUBKEY_LIMIT 16	// maximum number of public keys
#define RCP_HOST_LIMIT 1024	// maximum number of hosts accepted by "ip host" command
#define RCP_SYSLOG_LIMIT 8	// maximum number of syslog hosts
#define RCP_INTERFACE_LIMITS 64	// maximum number of interfaces
#define RCP_ARP_LIMIT 500		// maximum number of static arps
#define RCP_ROUTE_LIMIT 4000	// maximum number of static routes
#define RCP_DHCP_SERVER_GROUPS_LIMIT 4	// maximum number of server groups for dhcp relay
#define RCP_DHCP_SERVER_LIMIT 4	// maximum number of serves in a group for dhcp relay
#define RCP_DHCP_NETWORK_LIMIT 4	// maximum number of networks for dhcp server
#define RCP_DHCP_HOST_LIMIT 32	// maximum number of static leases for dhcp server
#define RCP_RIP_NETWORK_LIMIT 32	// maximum number of rip networks
#define RCP_RIP_NEIGHBOR_LIMIT 32	// maximum number of rip neighbors
#define RCP_ACL_LIMIT 1024		// maximum number of ACL entries
#define RCP_MASQ_LIMIT 32		// maximum number of masquerade NAT entries
#define RCP_FORWARD_LIMIT 32		// maximum number of  port forwarding entries
#define RCP_NTP_LIMIT 8		// maximum number of NTP servers
#define RCP_IPSEC_TUN_LIMIT 16	// maximum number of IPsec tunnels
#define RCP_IPSEC_FLOW_LIMIT 8	// maximum number of flows for an IPsec tunnel
#define RCP_OSPF_AREA_LIMIT 8	// maximum number of OSPF areas
#define RCP_OSPF_NETWORK_LIMIT 8	// maximum number of networks in an OSPF area
#define RCP_OSPF_ECMP_LIMIT 4	// maximum number of equal cost multipath routes in OSPF
#define RCP_OSPF_SUMMARY_ADDR_LIMIT 32	// maximum number of summary-address prefixes
#define RCP_OSPF_RANGE_LIMIT 32	// maximum number of summary ranges in OSPF
#define RCP_NETMON_LIMIT 64	// maximum number of hosts monitored
#define RCP_SNMP_USER_LIMIT 8	// maximum number of SNMP v3 users
#define RCP_SNMP_HOST_LIMIT 4	// maximum number of SNMP hosts configured to receive traps
#define RCP_MODULE_LIMIT 8	// maximum number of RCP modules supported
#define RCP_HTTP_AUTH_LIMIT 32	// maximum number of authenticated HTTP sessions
#define RCP_ACTIVE_NEIGHBOR_LIMIT 64	// maximum number of routing neigbors stored for http stats purposes

#ifdef __cplusplus
}
#endif

#endif
