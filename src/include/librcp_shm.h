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
#ifndef LIBRCP_SHM_H
#define LIBRCP_SHM_H
#ifndef LIBRCP_H
#include <stdint.h>
#include <string.h>
#endif
#ifndef LIBRCP_LIMITS_H
#include "librcp_limits.h"
#endif
#ifndef LIBRCP_CLI_H
#include "librcp_cli.h"
#endif
#ifndef LIBRCP_LOG_H
#include "librcp_log.h"
#endif
#ifndef LIBRCP_PROC_H
#include "librcp_proc.h"
#endif
#include <sys/time.h>	

#ifdef __cplusplus
extern "C" {
#endif

#define RCP_MAX_IF_NAME 15
#define RCP_MAX_STATS 96
#define RCP_CANARY 0xaa55a5a5
//*************************************************************************************
// shared memory support
//*************************************************************************************
typedef struct rcp_proc_t {
	uint32_t proc_type;	// process type; 0 for not configured
	pid_t pid;		// pid
	uint32_t start_cnt;		// number of times the process was started
	uint32_t mux_reconnect;	// mux socket reconnects
	uint32_t select_speedup;	// speed up select loop
	
	// watchdog
	uint32_t wproc;		// counter incremented by the process
	uint32_t wmonitor;		// counter incremented by the monitor
	int no_logging;		// no logging on restart
} RcpProcStats;

typedef struct rcp_module_t {
#define RCP_MODULE_NAME_LEN 47
	char name[RCP_MODULE_NAME_LEN + 1];
#define RCP_MODULE_VERSION_LEN 15
	char version[RCP_MODULE_VERSION_LEN + 1];
} RcpModule;

// administrator structure
typedef struct rcp_admin_t {
	unsigned char valid;
	char name[CLI_MAX_ADMIN_NAME + 1];
	char password[CLI_MAX_PASSWD + 1];
} RcpAdmin;

typedef struct rcp_pubkey_t {
	unsigned char valid;
	char pubkey[CLI_MAX_PUBKEY + 1];
} RcpPubkey;

// external dhcp server structure used by relay
typedef struct rcp_dhcpr_server_t {
	unsigned char valid;
	uint32_t server_ip[RCP_DHCP_SERVER_LIMIT];
	uint32_t leases[RCP_DHCP_SERVER_LIMIT];
} RcpDhcprServer;

// local dhcpserver structure
typedef struct rcp_dhcps_network_t {
	unsigned char valid;
	uint32_t ip;
	uint32_t mask;
	char name[20];	// holds the address in cidr format for CLI IPC purposes

	uint32_t range_start;
	uint32_t range_end;
	uint32_t gw1;	// default gateway
	uint32_t gw2;

#define RCP_DHCP_LEASE_DAY_DEFAULT 1	
	uint8_t lease_days;
#define RCP_DHCP_LEASE_HOUR_DEFAULT 0
	uint8_t lease_hours;
#define RCP_DHCP_LEASE_MINUTE_DEFAULT 0
	uint8_t lease_minutes;
	
	// stats and operation
	uint32_t last_ip;
} RcpDhcpsNetwork;

// static MAC/IP leases
typedef struct rcp_dhcps_host_t {
	uint32_t ip;
	uint8_t mac[6];
	unsigned char valid;
} RcpDhcpsHost;

// static route structure
typedef struct rcp_static_route_t {
	unsigned char valid;
	unsigned char type;	// route type, RCP_ROUTE_STATIC or RCP_ROUTE_BLACKHOLE
	
	uint32_t ip;
	uint32_t mask;
	uint32_t gw;
	uint32_t metric;
} RcpStaticRoute;

// arp structure
typedef struct rcp_static_arp_t {
	uint32_t ip;
	uint8_t mac[6];
	unsigned char valid;
} RcpStaticArp;



// syslog host ip and port
typedef struct rcp_syslog_host_t {
	unsigned char valid;
	uint32_t ip;
	uint16_t port;
} RcpSyslogHost;

// ip host
typedef struct rcp_ip_host_t {
	unsigned char valid;
	char name[CLI_MAX_DOMAIN_NAME + 1];
	uint32_t ip;
} RcpIpHost;

typedef struct rtp_ntp_servert_t {
	unsigned char valid;
	unsigned char preferred;
	uint32_t ip;
	char name[CLI_MAX_DOMAIN_NAME + 1];
	
	// server stats
	double delay;
	double offset;
	time_t interval;
	uint8_t stratum;
	uint8_t trustlevel;
} RcpNtpServer;


// partner structure used for RIP networks and neighbors
typedef struct rcp_rippartner_t {
	unsigned char valid;
	uint32_t ip;
	uint32_t mask;
	int timeout;	// sending 30 seconds RIP updates if this is a neighbor

	// packet stats
	uint32_t req_rx;
	uint32_t req_tx;
	uint32_t resp_rx;
	uint32_t resp_tx;
} RcpRipPartner;

typedef struct rcp_sacl_t {
	// source network IP address
	uint32_t ip; 	  // ip address - a value of 0 means "any"
	uint8_t mask_bits; // network mask bits (from CIDR format)
} RcpAclSimple;

// shared memory storage structure
typedef struct rcp_acl_data_t {
	// source network IP address
	uint32_t ip; 	  // ip address - a value of 0 means "any"
	uint8_t mask_bits; // network mask bits (from CIDR format)
	uint16_t port;	// TCP or UDP port number - a value of 0 means "any"

#define RCPACL_PROTOCOL_TCP	0x06
#define RCPACL_PROTOCOL_UDP	0x11
#define RCPACL_PROTOCOL_ANY	0xff	
	uint8_t protocol;		// protocol number: 0-254, 255 means "any"
				// 6 - TCP
				// 17 - UDP

	// destination network IP address
	uint32_t dest_ip; 	  // ip address - a value of 0 means "any" if out_intf is not set
	uint8_t dest_mask_bits; // network mask bits (from CIDR format)
	uint16_t dest_port;	// TCP or UDP port number - a value of 0 means "any"
	char out_interface[RCP_MAX_IF_NAME + 1];	// output interface
#define RCP_ACL_CONSTATE_NEW 1
#define RCP_ACL_CONSTATE_ESTABLISHED 2
#define RCP_ACL_CONSTATE_RELATED 4
#define RCP_ACL_CONSTATE_INVALID 8
	uint8_t constate;	// connection state
	uint8_t mac[6];	// mac address
} RcpAclExtended;

typedef struct rcp_acl_icmp_t {
	// source network IP address
	uint32_t ip; 	  // ip address - a value of 0 means "any"
	uint8_t mask_bits; // network mask bits (from CIDR format)

	// destination network IP address
	uint32_t dest_ip; 	  // ip address - a value of 0 means "any" if out_intf is not set
	uint8_t dest_mask_bits; // network mask bits (from CIDR format)

	int icmp_type;
	uint8_t mac[6];	// mac address
} RcpAclIcmp;

// shared memory storage structure
typedef struct rcp_acl_t {
	unsigned char valid;
	
#define RCP_ACL_TYPE_EXTENDED 0	
#define RCP_ACL_TYPE_SIMPLE 1
#define RCP_ACL_TYPE_ICMP 2
#define RCP_ACL_TYPE_MAX 3
	uint8_t type;
	
	// action: permit or deny
#define RCPACL_ACTION_NONE 0
#define RCPACL_ACTION_PERMIT 1
#define RCPACL_ACTION_DENY 2
	uint8_t action;
	
	uint8_t list_number;	// acl number

	union {
		RcpAclExtended extended;
		RcpAclSimple simple;
		RcpAclIcmp icmp;
	} u;
} RcpAcl;

// masquerade nat
typedef struct rcp_masq_t {
	uint32_t ip;
	uint32_t mask;
	uint8_t valid;
	char out_interface[RCP_MAX_IF_NAME + 1];	// output interface
} RcpMasq;

typedef struct rcp_frowarding_t {
	uint16_t port;
	uint16_t dest_port;
	uint32_t dest_ip;
	uint8_t valid;
} RcpForwarding;


typedef struct rcp_interface_stats_t {
	// interface packet stats
	unsigned long long current_tx_bytes;
	unsigned long long current_rx_bytes;
	unsigned tx_kbps[RCP_MAX_STATS];
	unsigned can1;
	unsigned rx_kbps[RCP_MAX_STATS];
	unsigned can2;
	
	
	// dhcp receive stats
	uint32_t dhcp_rx;
	uint32_t dhcp_rx_discover;
	uint32_t dhcp_rx_request;
	uint32_t dhcp_rx_decline;
	uint32_t dhcp_rx_release;
	uint32_t dhcp_rx_inform;
	uint32_t dhcp_rx_offer;
	uint32_t dhcp_rx_ack;
	uint32_t dhcp_rx_nack;
	
	// dhcp errors	
	uint32_t dhcp_err_rx;
	uint32_t dhcp_err_hops;	// max-hops exceeded
	uint32_t dhcp_err_pkt;	// malformed packets
	uint32_t dhcp_err_not_configured; // interface not configured
	uint32_t dhcp_err_server_notconf; // server not configured
	uint32_t dhcp_warn_server_unknown; // unknown server - we might be part of a relay chain
	uint32_t dhcp_err_outgoing_intf;  // unknown outgoing interface
	uint32_t dhcp_err_giaddr;	// somebody is spoofing our interface addresses
	uint32_t dhcp_err_lease;	// lease errors
	uint32_t dhcp_err_icmp;	// a client already has the IP address
	
	// dhcp transmit stats - packets sent to clients
	uint32_t dhcp_tx;
	uint32_t dhcp_tx_discover;
	uint32_t dhcp_tx_request;
	uint32_t dhcp_tx_decline;
	uint32_t dhcp_tx_release;
	uint32_t dhcp_tx_inform;
	uint32_t dhcp_tx_offer;
	uint32_t dhcp_tx_ack;
	uint32_t dhcp_tx_nack;
	
	//ospf stats
	uint32_t ospf_rx_hello;
	uint32_t ospf_rx_hello_errors;
	uint32_t ospf_tx_hello;
	uint32_t ospf_rx_db;
	uint32_t ospf_rx_db_errors;
	uint32_t ospf_tx_db;
	uint32_t ospf_tx_db_rxmt;
	uint32_t ospf_rx_lsrq;
	uint32_t ospf_rx_lsrq_errors;
	uint32_t ospf_tx_lsrq;
	uint32_t ospf_tx_lsrq_rxmt;
	uint32_t ospf_rx_lsup;
	uint32_t ospf_rx_lsup_errors;
	uint32_t ospf_tx_lsup;
	uint32_t ospf_tx_lsup_rxmt;
	uint32_t ospf_rx_lsack;
	uint32_t ospf_rx_lsack_errors;
	uint32_t ospf_tx_lsack;
	
	// ospf network state
#define RCP_OSPF_NETSTATE_SIZE 10
	char ospf_state[RCP_OSPF_NETSTATE_SIZE + 1];
	uint32_t ospf_dr;
	uint32_t ospf_bdr;
} RcpInterfaceStats;

// interface structure
typedef struct rcp_interface_t {
	char name[RCP_MAX_IF_NAME + 1];
	uint8_t admin_up;
	uint8_t link_up;
	uint8_t proxy_arp;
	int kindex;	// kernel interface index
	uint32_t ip;
	uint32_t mask;
	unsigned mtu;
	uint16_t vlan_id;		// vlan id, 0 if not configured
	uint16_t vlan_parent;	// parent interface index
	uint8_t mac[6];
#define IF_ETHERNET 0
#define IF_LOOPBACK 1
#define IF_BRIDGE 2
#define IF_VLAN 3
	uint8_t type;	// interface type
	
	// RIP
	uint8_t rip_passive_interface;	// passive interface flag in use by RIP
	uint32_t rip_multicast_ip;
	uint32_t rip_multicast_mask;
	int rip_timeout;	// sending 30 seconds RIP updates
	char rip_passwd[CLI_RIP_PASSWD + 1]; // MD5 authentication password
		
	// DHCP
	uint8_t dhcp_relay;			// dhcp relay flag for the interface


	// acl
	uint8_t acl_in;	// input acl number - 0 if not configured
	uint8_t acl_out;	// output acl number - 0 if not configured
	uint8_t acl_forward; // forwarding acl number - 0 if not configured
	
	// OSPF
	uint32_t ospf_multicast_ip;
	uint32_t ospf_multicast_mask;
#define RCP_OSPF_IF_COST_DEFAULT 1	
	uint16_t ospf_cost;
#define RCP_OSPF_IF_PRIORITY_DEFAULT 1	
	uint16_t ospf_priority;
#define RCP_OSPF_IF_HELLO_DEFAULT 10	
	uint16_t ospf_hello;
#define RCP_OSPF_IF_DEAD_DEFAULT 40	
	uint16_t ospf_dead;
#define RCP_OSPF_IF_RXMT_DEFAULT 5	
	uint16_t ospf_rxmt;
	uint8_t ospf_auth_type;	// 0 - none, 1 - simple, 2 - md5
	char ospf_passwd[CLI_OSPF_PASSWD + 1]; // simple authentication password
	char ospf_md5_passwd[CLI_OSPF_MD5_PASSWD + 1]; // md5 authentication password
	uint8_t ospf_md5_key;	// key id
	uint8_t ospf_mtu_ignore;	// ignore MTU field in DD packets
	uint8_t ospf_passive_interface;
	

	
	//***************************
	// Statistics
	//***************************
	RcpInterfaceStats stats;
} RcpInterface;

static inline void rcpInterfaceSetDefaults(RcpInterface *ptr) {
	if (ptr == NULL)
		return;
		
	ptr->ospf_cost = RCP_OSPF_IF_COST_DEFAULT;
	ptr->ospf_priority = RCP_OSPF_IF_PRIORITY_DEFAULT;
	ptr->ospf_hello = RCP_OSPF_IF_HELLO_DEFAULT;
	ptr->ospf_dead = RCP_OSPF_IF_DEAD_DEFAULT;
	ptr->ospf_rxmt = RCP_OSPF_IF_RXMT_DEFAULT;
	ptr->stats.can1 = RCP_CANARY;
	ptr->stats.can2 = RCP_CANARY;
}


typedef struct rcp_netmon_host_t {
	unsigned char valid;
#define RCP_NETMON_TYPE_ICMP 0	
#define RCP_NETMON_TYPE_TCP 1
#define RCP_NETMON_TYPE_SSH 2
#define RCP_NETMON_TYPE_HTTP 3
#define RCP_NETMON_TYPE_SMTP 4
#define RCP_NETMON_TYPE_NTP 5
#define RCP_NETMON_TYPE_DNS 6
#define RCP_NETMON_TYPE_MAX 7
	unsigned char type;
	char hostname[CLI_MAX_DOMAIN_NAME + 1]; // storing configured hostnames or ip addresses as text
	uint16_t port;
	
	// runtime data
	int sock;
	uint32_t ip_resolved;	// dns-resolved ip address
	uint32_t ip_sent;	// ip address where the request was sent - ip_resolved might change!
	
	// packets
	unsigned pkt_sent;			// total packets sent
	unsigned pkt_received;		// total packets received
	unsigned interval_pkt_sent;		// packets sent out in the current interval
	unsigned interval_pkt_received;	// packets received in the current interval
	unsigned rx[RCP_MAX_STATS];	// rx stats
	unsigned can1;
	unsigned tx[RCP_MAX_STATS];	// tx stats
	unsigned can2;
	int wait_response;	// counting up, waiting for responses

	// host status: 0 down, 1 up
	uint8_t status;	
	
	// response time calculation
	struct timeval start_tv;
	unsigned resptime_acc;
	unsigned resptime_samples;
	unsigned resptime[RCP_MAX_STATS];	// average response time in microseconds
	unsigned can3;
} RcpNetmonHost;
static inline void rcpNetmonHostClean(RcpNetmonHost *n) {
	memset(n, 0, sizeof(RcpNetmonHost));
	n->can1 = RCP_CANARY;
	n->can2 = RCP_CANARY;
	n->can3 = RCP_CANARY;
}

typedef struct ipsec_flow_t {
	unsigned char valid;
	uint32_t local_ip;
	uint32_t local_mask;
	uint32_t remote_ip;
	uint32_t remote_mask;
} RcpIpsecFlow;

typedef struct ipsec_tun_t {
	unsigned char valid;
	uint32_t peer1;
	uint32_t peer2;
	uint32_t spi;
	RcpIpsecFlow flow[RCP_IPSEC_FLOW_LIMIT];
	char name[CLI_MAX_TUNNEL_NAME + 1];
	char shared_secret[CLI_TUN_PASSWD + 1];
} RcpIpsecTun;

// OSPF configuration
typedef struct rcp_ospf_network_t {
	uint8_t valid;
	uint32_t ip;
	uint32_t mask;
	uint32_t area_id;

} RcpOspfNetwork;

typedef struct rcp_ospf_area_t {
	uint8_t valid;
	uint8_t stub;	// this is a stub area
	uint8_t no_summary;	// no summary routes flooded in stub area
	uint32_t area_id;
#define RCP_OSPF_DEFAULT_COST 1 	
	uint32_t default_cost;	// cost of default route for stub areas
	
	RcpOspfNetwork network[RCP_OSPF_NETWORK_LIMIT];
} RcpOspfArea;
static inline void rcpOspfAreaSetDefaults(RcpOspfArea *ptr) {
	ptr->default_cost = RCP_OSPF_DEFAULT_COST;
}

typedef struct rcp_ospf_summary_addr_t {
	uint8_t valid;
	uint8_t not_advertise;
	uint32_t ip;
	uint32_t mask;
} RcpOspfSummaryAddr;

typedef struct rcp_ospf_range_t {
	uint8_t valid;
	uint8_t not_advertise;
	uint32_t area;
	uint32_t ip;
	uint32_t mask;
	// runtime data
	uint32_t cost;
} RcpOspfRange;

typedef struct rpc_snmp_user_t {
	char name[CLI_MAX_ADMIN_NAME + 1];
	char passwd[CLI_SNMP_MAX_PASSWD + 1];
} RcpSnmpUser;

typedef struct rpc_snmp_host_t {
	uint32_t ip;
	uint8_t valid;
	uint8_t inform;
	char com_passwd[CLI_SNMP_MAX_COMMUNITY + 1];
} RcpSnmpHost;

typedef struct rcp_snmp_t {
#define RCP_SNMP_MAX_CONTACT 63
	char contact[RCP_SNMP_MAX_CONTACT + 1];
#define RCP_SNMP_MAX_LOCATION 63
	char location[RCP_SNMP_MAX_LOCATION + 1];
	uint8_t com_public;
	char com_passwd[CLI_SNMP_MAX_COMMUNITY + 1];
	RcpSnmpUser user[RCP_SNMP_USER_LIMIT];
	
	// traps
	uint8_t traps_enabled;
	RcpSnmpHost host[RCP_SNMP_HOST_LIMIT];
	uint8_t coldstart_trap_sent;
} RcpSnmp;

// general configuration - static, will not be allocated
typedef struct {
	int version;
	int configured;	// default configuration applied
	// hostname
	char hostname[CLI_MAX_HOSTNAME + 1];
	
	// name servers
	uint32_t name_server1;	// ip address, 0 for not configured
	uint32_t name_server2;	// p address, 0 for not configured
	
	// domain name
	char domain_name[CLI_MAX_DOMAIN_NAME + 1];
	
	// host entries
	RcpIpHost ip_host[RCP_HOST_LIMIT];	// host array

	// exec-timeout
	uint16_t timeout_minutes;
	uint8_t timeout_seconds;
	
	// enable password
	char enable_passwd[CLI_MAX_PASSWD + 1];
	int enable_passwd_configured;
	
	// administrators
	RcpAdmin admin[RCP_ADMIN_LIMIT]; // array of administrators
	RcpPubkey pubkey[RCP_PUBKEY_LIMIT];	// array of public keys
	int admin_default_passwd;	// there is an "rcp" admin with an "rcp" password configured
	
	// interfaces
	RcpInterface intf[RCP_INTERFACE_LIMITS]; // array of active interfaces
	
	// static arp & routes
	RcpStaticArp sarp[RCP_ARP_LIMIT];
	RcpStaticRoute sroute[RCP_ROUTE_LIMIT];
	
	// logger
	uint8_t log_level;
	uint64_t facility;
	uint64_t snmp_facility;
	int rate_limit;	// log rate limit as configured by cli
	int current_rate;	// counter incremented for each incoming message, reseted once a second
	int max_rate;	// maximum recorded rate
	uint64_t log_messages; // number of messages logged
	uint64_t log_stored_messages; // number of messages stored in shared memory
	RcpSyslogHost syslog_host[RCP_SYSLOG_LIMIT]; // syslog host array
	
	// services
#define RCP_SERVICE_TFTP 1
#define RCP_SERVICE_TELNET (1 << 1)
#define RCP_SERVICE_FTP (1 << 2)
#define RCP_SERVICE_HTTP (1 << 3)
	uint16_t services;
#define RCP_DEFAULT_TELNET_PORT 23	
	uint16_t telnet_port;
#define RCP_DEFAULT_HTTP_PORT 80
	uint16_t http_port;
	char http_passwd[CLI_MAX_PASSWD + 1];
	int http_default_passwd;	// http password is "rcp"
	
#define RCP_SERVICE_SSH 1
#define RCP_SERVICE_SFTP (1 << 1)
#define RCP_SERVICE_SCP (1 << 2)
	uint16_t sec_services;
#define RCP_DEFAULT_SSH_PORT 22	
	uint16_t sec_port;
	

	// rip
	uint8_t rip_redist_static; // 0 - disabled, >0 - enabled, 1 default when enabled
	uint8_t rip_redist_connected; // 0 - disabled, >0 - enabled, 1 default when enabled
	uint8_t rip_redist_connected_subnets; // 0 - disabled, 1 enabled
	uint8_t rip_redist_ospf; // 0 - disabled, >0 - enabled, 1 default when enabled
	uint8_t rip_default_information; // 0 - disabled, 1 - enabled
#define RCP_RIP_DEFAULT_TIMER 30
	uint8_t rip_update_timer;
	RcpRipPartner rip_network[RCP_RIP_NETWORK_LIMIT]; // network array
	RcpRipPartner rip_neighbor[RCP_RIP_NEIGHBOR_LIMIT]; // neighbor array
	
	// dhcp relay
#define RCP_DHCPR_DEFAULT_MAX_HOPS 4	
	uint8_t dhcpr_max_hops;
#define RCP_DHCPR_DEFAULT_SERVICE_DELAY 10
	uint8_t dhcpr_service_delay;
	uint8_t dhcpr_option82;
	RcpDhcprServer dhcpr_server[RCP_DHCP_SERVER_GROUPS_LIMIT]; 	// server groups
	
	// dhcp server
	uint8_t dhcps_enable;	// enable dhcp server
	RcpDhcpsNetwork dhcps_network[RCP_DHCP_NETWORK_LIMIT];
	RcpDhcpsHost dhcps_host[RCP_DHCP_HOST_LIMIT];
	uint32_t dhcps_dns1;	// DNS
	uint32_t dhcps_dns2;
	uint32_t dhcps_ntp1;	// NTP
	uint32_t dhcps_ntp2;
	uint32_t dhcps_leases; // current number of leases;
				 // updated by "show ip dhcp leases" command
	unsigned char dhcps_domain_name[CLI_MAX_DOMAIN_NAME + 1];
	
		
	// dns cache
	uint8_t dns_server;			// enable dns cache
	uint16_t dns_rate_limit;		// forwarding rate limit
	

	// acl
	RcpAcl ip_acl[RCP_ACL_LIMIT];	// access control lists for interfaces
	
	// masq nat
	RcpMasq ip_masq[RCP_MASQ_LIMIT];	// masquarade nat
	
	// port forwarding
	RcpForwarding port_forwarding[RCP_FORWARD_LIMIT];
	
	// process statistics
	RcpProcStats pstats[RCP_PROC_MAX + 2]; // RCP_PROC_MAX excludes cli and services processes
	// installed modules
	RcpModule module[RCP_MODULE_LIMIT];

	// NTP
	RcpNtpServer ntp_server[RCP_NTP_LIMIT];
	int ntp_server_enabled;
	// NTP status
	uint32_t ntp_refid;
	double ntp_offset;
	double ntp_rootdelay;
	double ntp_rootdispersion;
	double ntp_reftime;
	uint8_t ntp_synced;
	uint8_t ntp_stratum;
		
	// network monitoring
	RcpNetmonHost netmon_host[RCP_NETMON_LIMIT];
#define RCP_DEFAULT_MON_INTERVAL 30	
	int monitor_interval;	// monitoring interval, default 10

	// IPsec tunnels
	RcpIpsecTun ipsec_tunnel[RCP_IPSEC_TUN_LIMIT];
	
	// OSPF
	uint32_t ospf_router_id;
	RcpOspfArea ospf_area[RCP_OSPF_AREA_LIMIT];
	RcpOspfSummaryAddr ospf_sum_addr[RCP_OSPF_SUMMARY_ADDR_LIMIT];
	RcpOspfRange ospf_range[RCP_OSPF_RANGE_LIMIT];
	uint8_t ospf_discard_external;
	uint8_t ospf_discard_internal;
	
#define RCP_OSPF_SPF_DELAY_DEFAULT 1	
	uint16_t ospf_spf_delay;
#define RCP_OSPF_SPF_HOLD_DEFAULT 4	
	uint16_t ospf_spf_hold;
	uint8_t ospf_log_adjacency;
	uint8_t ospf_log_adjacency_detail;
	
#define RCP_OSPF_INFORMATION_METRIC_DEFAULT 20	// default gateway
	uint32_t ospf_redist_information; // 0 disabled, > 0 enabled, the value is the metric
#define RCP_OSPF_INFORMATION_METRIC_TYPE_DEFAULT 2
	uint8_t ospf_information_metric_type;
	
#define RCP_OSPF_STATIC_METRIC_DEFAULT 20
	uint32_t ospf_redist_static; // 0 disabled, > 0 enabled, the value is the metric
	uint32_t ospf_static_tag;	// default 0
#define RCP_OSPF_STATIC_METRIC_TYPE_DEFAULT 2
	uint8_t ospf_static_metric_type;
	
#define RCP_OSPF_CONNECTED_METRIC_DEFAULT 20
	uint32_t ospf_redist_connected; // 0 disabled, > 0 enabled, the value is the metric
	uint32_t ospf_redist_connected_subnets; // 0 disabled, > 0 enabled, the value is the metric
	uint32_t ospf_connected_tag;	// default 0
	uint32_t ospf_connected_subnets_tag;	// default 0
#define RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT 2
	uint8_t ospf_connected_metric_type;
	
#define RCP_OSPF_RIP_METRIC_DEFAULT 20
	uint32_t ospf_redist_rip; // 0 disabled, > 0 enabled, the value is the metric
	uint32_t ospf_rip_tag;	// default 0
#define RCP_OSPF_RIP_METRIC_TYPE_DEFAULT 2
	uint8_t ospf_rip_metric_type;
	
	uint32_t ospf_md5_seq;
		
	// SNMP
	RcpSnmp snmp;
} RcpConfig;

typedef struct {
	uint32_t ip;
	// login session - checking ip and checking session id from	shm->stats.http_session[i]
	// rest session - checking ip and checking token;
	// - token changed after each rest transaction

	// browser reload for login sessions
#define RCP_HTTP_TIMEOUT 30

	// ttl timeout for this structure
#define RCP_HTTP_AUTH_TTL ((RCP_HTTP_TIMEOUT * 2) + 10)
	// ttl and rest token
	int ttl;
	uint32_t token;
} RcpHttpAuth;	
	

typedef struct {
	uint8_t valid;
	uint8_t netmask_cnt;
	uint32_t network;
	uint32_t if_ip;
	uint32_t area;
	uint32_t ip;
	uint32_t router_id;
} RcpActiveNeighbor;

typedef struct {
	// ospf
	volatile int ospf_active_locked;
	RcpActiveNeighbor ospf_active[RCP_ACTIVE_NEIGHBOR_LIMIT];
	uint32_t ospf_spf_calculations;
	time_t ospf_last_spf;
	int ospf_external_lsa;
	int ospf_originated_lsa;
	int ospf_is_abr;
	int ospf_is_asbr;

	// rip
	volatile int rip_active_locked;
	RcpActiveNeighbor rip_active[RCP_ACTIVE_NEIGHBOR_LIMIT];

	// service statistics
	unsigned sec_rx;
	unsigned tftp_rx;
	unsigned http_rx;
	unsigned telnet_rx;
	unsigned ftp_rx;
	unsigned ntp_rx;

	unsigned cpu[RCP_MAX_STATS];	// cpu load statistics (%)
	unsigned can1;
	unsigned freemem[RCP_MAX_STATS];	// free memory statistics (%)
	unsigned can2;
	
	// dns
	unsigned dns_queries;
	unsigned dns_answers;
	unsigned dns_cached_answers;
	unsigned dns_a_answers;
	unsigned dns_aaaa_answers;
	unsigned dns_cname_answers;
	unsigned dns_ptr_answers;
	unsigned dns_mx_answers;
	unsigned dns_err_rate_limit;
	unsigned dns_err_rx;
	unsigned dns_err_tx;
	unsigned dns_err_malformed;
	unsigned dns_err_unknown;
	unsigned dns_err_no_server;
	unsigned dns_err_rq_timeout;
	unsigned dns_last_q;
	unsigned dns_q[RCP_MAX_STATS];	// dns query statistics (%)
	unsigned can3;
	
	// dhcp
	unsigned dhcp_rx[RCP_MAX_STATS];
	unsigned can4;
	uint32_t dhcp_last_rx;
	
	// http authentication
	RcpHttpAuth http_auth[RCP_HTTP_AUTH_LIMIT];
	unsigned can5;
	unsigned http_session[RCP_HTTP_AUTH_TTL];
	unsigned can6;
	int http_auth_initialized;
	
	// snmp stats
	unsigned snmp_traps_sent;
	
} RcpStats;

// RCP shared memory
typedef struct {
	//*******************************
	// configuration and statistics
	//*******************************
	RcpConfig config;
	RcpStats stats;
	
	//*******************************
	// CLI
	//*******************************
	// cli parse tree
	CliNode *cli_head; // the top node in the tree
	CliNode *cli_om; // output modifiers node
#define RCP_SHMEM_SIZE_CLI (80 * 4096) // cli node tree  // 36 not working
	void *cli_smem_handle; // small memory allocator handle
	
	//*******************************
	// Logger
	//*******************************
	RcpLogEntry *log_head;
	RcpLogEntry *log_tail;
#define RCP_SHMEM_SIZE_LOG (256 * 4096) // 1MB for logs
	void *log_smem_handle; // small memory allocator handle	
	
} RcpShm;

// mount memory address for RcpShm structure
#define RCP_SHMEM_START 0x30000000 

// shared memory size
#define RCP_SHMEM_SIZE ( \
	RCP_SHMEM_SIZE_CLI + \
	RCP_SHMEM_SIZE_LOG + \
	((sizeof(RcpShm) / 4096) + 1) * 4096)
	
// shared memory file name
#define RCP_SHMEM_NAME "rcp"	

// get a pointer to RCP shared memory
RcpShm *rcpGetShm(void);

// initialize RCP shared memory access for a specific process
RcpShm *rcpShmemInit(uint32_t process);

// grab process stats pointer
RcpProcStats *rcpProcStats(RcpShm *shm, uint32_t process);

// grab rcp semaphore
void *rcpSemOpen(const char *fname);

// is redistribution configured?
int rcpRipRedistStatic(void);
int rcpRipRedistOspf(void);
int rcpOspfRedistStatic(void);
int rcpOspfRedistRip(void);

// is SNMP agent configured?
int rcpSnmpAgentConfigured(void);

// open a shared memory file of specified size, and mount it in the memory at the specified location
// the file will never be closed
void *rcpShmOpen(const char *fname, size_t size, uint64_t start_point, int *already);
void *rcpAlloc(void *handle, size_t size);
void rcpFree(void *handle, void *mem);

// return 1 if the message is acceptable
static inline int rcpLogAccept(RcpShm *shm, uint8_t level, uint64_t facility) {
	// accept the message if:
	// - the current rate is smaller or equal to the configured rate
	// and
	//     - the level is smaller or equal to the configured level
	//     or
	//     - the facility is enabled
	if (shm->config.current_rate <= shm->config.rate_limit &&
	    (level <= shm->config.log_level || facility & shm->config.facility))
		return 1;
	return 0;
}

// set a module
extern RcpModule *rcpSetModule(RcpShm *shm, const char *name, const char *version);

#ifdef __cplusplus
}
#endif

#endif
