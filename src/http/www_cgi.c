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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "http.h"

// local ntp server enabled
static void cgi_ntps_enabled(FILE *fp) {
	if (shm->config.ntp_server_enabled)
		fprintf(fp, "selected");
}

// local ntp server disabled
static void cgi_ntps_disabled(FILE *fp) {
	if (!shm->config.ntp_server_enabled)
		fprintf(fp, "selected");
}

static int ntphost = -1;
static void cgi_ntph_get(FILE *fp) {
	ntphost++;
	if (ntphost >= RCP_NTP_LIMIT)
		return;
	
	if (shm->config.ntp_server[ntphost].valid == 0)
		return;
	if (shm->config.ntp_server[ntphost].ip != 0) {
		fprintf(fp, "%d.%d.%d.%d",
			RCP_PRINT_IP(shm->config.ntp_server[ntphost].ip));
		return;
	}	
	else if (shm->config.ntp_server[ntphost].name[0] != '\0') {
		fprintf(fp, "%s", shm->config.ntp_server[ntphost].name);
		return;
	}	
}

static void cgi_telnet_enabled(FILE *fp) {
	if (shm->config.services & RCP_SERVICE_TELNET)
		fprintf(fp, "selected");
}		

static void cgi_telnet_disabled(FILE *fp) {
	if (!(shm->config.services & RCP_SERVICE_TELNET))
		fprintf(fp, "selected");
}
		
static void cgi_telnet_port(FILE *fp) {
	fprintf(fp, "%d", shm->config.telnet_port);
}

static void cgi_ftp_enabled(FILE *fp) {
	if (shm->config.services & RCP_SERVICE_FTP)
		fprintf(fp, "selected");
}		

static void cgi_ftp_disabled(FILE *fp) {
	if (!(shm->config.services & RCP_SERVICE_FTP))
		fprintf(fp, "selected");
}		

static void cgi_tftp_enabled(FILE *fp) {
	if (shm->config.services & RCP_SERVICE_TFTP)
		fprintf(fp, "selected");
}		

static void cgi_tftp_disabled(FILE *fp) {
	if (!(shm->config.services & RCP_SERVICE_TFTP))
		fprintf(fp, "selected");
}
		
static void cgi_dns_hostname(FILE *fp) {
	fprintf(fp, "%s", shm->config.hostname);
}		

static void cgi_dns_domain(FILE *fp) {
	fprintf(fp, "%s", shm->config.domain_name);
}		

static void cgi_dns_ns1(FILE *fp) {
	if (shm->config.name_server1)
		fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.name_server1));
}		

static void cgi_dns_ns2(FILE *fp) {
	if (shm->config.name_server2)
		fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.name_server2));
}		

static void cgi_dnss_enabled(FILE *fp) {
	if (shm->config.dns_server)
		fprintf(fp, "selected");
}		

static void cgi_dnss_disabled(FILE *fp) {
	if (!shm->config.dns_server)
		fprintf(fp, "selected");
}		

static int logs_id = -1;
static void cgi_logs_ip(FILE *fp) {
	logs_id++;
	if (logs_id >= RCP_SYSLOG_LIMIT)
		return;
	
	if (shm->config.syslog_host[logs_id].valid == 0)
		return;
	fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.syslog_host[logs_id].ip));
}
static void cgi_logs_port(FILE *fp) {
	if (logs_id >= RCP_SYSLOG_LIMIT)
		return;
	
	if (shm->config.syslog_host[logs_id].valid == 0)
		return;

	if (shm->config.syslog_host[logs_id].port == 0)
		return;
	fprintf(fp, "%d", shm->config.syslog_host[logs_id].port);
}
static void cgi_logs_emergencies(FILE *fp) {
	if (shm->config.log_level == RLOG_EMERG)
		fprintf(fp, "selected");
}
static void cgi_logs_alerts(FILE *fp) {
	if (shm->config.log_level == RLOG_ALERT)
		fprintf(fp, "selected");
}
static void cgi_logs_critical(FILE *fp) {
	if (shm->config.log_level == RLOG_CRIT)
		fprintf(fp, "selected");
}
static void cgi_logs_errors(FILE *fp) {
	if (shm->config.log_level == RLOG_ERR)
		fprintf(fp, "selected");
}
static void cgi_logs_warnings(FILE *fp) {
	if (shm->config.log_level == RLOG_WARNING)
		fprintf(fp, "selected");
}
static void cgi_logs_notifications(FILE *fp) {
	if (shm->config.log_level == RLOG_NOTICE)
		fprintf(fp, "selected");
}
static void cgi_logs_informational(FILE *fp) {
	if (shm->config.log_level == RLOG_INFO)
		fprintf(fp, "selected");
}
static void cgi_logs_debugging(FILE *fp) {
	if (shm->config.log_level == RLOG_DEBUG)
		fprintf(fp, "selected");
}
static void cgi_logsnmp_enabled(FILE *fp) {
	if (shm->config.snmp_facility)
		fprintf(fp, "selected");
}
static void cgi_logsnmp_disabled(FILE *fp) {
	if (!shm->config.snmp_facility)
		fprintf(fp, "selected");
}

const char *strif1 = 
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>%s<input name=\"r%d\" type=\"hidden\" id=\"idr%d\" class=\"ifname\" value=\"%s\"/>\n"
"<input name=\"r%dt\" type=\"hidden\" id=\"idr%dtype\" class=\"iftype\" value=\"%s\"/></td>\n"
"<td><input name=\"r%da\" type=\"text\" id=\"idr%da\" class=\"addr\" size=\"15\" maxlength=\"15\" value=\"%s\"/>\n"
"/ <input name=\"r%dm\" type=\"text\" id=\"idr%dm\" class=\"mask\" size=\"2\" maxlength=\"2\" value=\"%s\"/>\n"
"&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td><input name=\"r%dmtu\" type=\"text\" id=\"idr%dmtu\" class=\"mtu\" size=\"5\" maxlength=\"5\" value=\"%s\"/>\n";

const char *strif2 = 
"<input name=\"r%dshut\" type=\"checkbox\"  id=\"idr%dshut\" class=\"shutdown\" %s>Shutdown<br></td>\n";

const char *strif3 =
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";

static int ifcnt = -1;
static void print_eth(FILE *fp, RcpInterface *intf) {
	ifcnt++;
	char ip[16];
	*ip = '\0';
	char mask[3];
	*mask = '\0';
	if (intf->ip) {
		sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));
		sprintf(mask, "%d", mask2bits(intf->mask));
	}
	char mtu[10];
	*mtu = '\0';
	if (intf->mtu == 0)
		strcpy(mtu, "1500");
	else
		sprintf(mtu, "%d", intf->mtu);

	fprintf(fp, strif1,
		intf->name, ifcnt, ifcnt, intf->name,
		ifcnt, ifcnt, "ethernet", 
		ifcnt, ifcnt, ip,
		ifcnt, ifcnt, mask,
		ifcnt, ifcnt, mtu);
			
	char *cb = "checked";
	if (intf->admin_up)
		cb = "";
	fprintf(fp, strif2,
		ifcnt, ifcnt, cb);

	fprintf(fp, "%s", strif3);
}
static void print_vlan(FILE *fp, RcpInterface *intf) {
	ifcnt++;
	char ip[16];
	*ip = '\0';
	char mask[3];
	*mask = '\0';
	if (intf->ip) {
		sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));
		sprintf(mask, "%d", mask2bits(intf->mask));
	}
	char mtu[10];
	*mtu = '\0';
	if (intf->mtu == 0)
		strcpy(mtu, "1500");
	else
		sprintf(mtu, "%d", intf->mtu);

	fprintf(fp, strif1,
		intf->name, ifcnt, ifcnt, intf->name,
		ifcnt, ifcnt, "vlan", 
		ifcnt, ifcnt, ip,
		ifcnt, ifcnt, mask,
		ifcnt, ifcnt, mtu);
			
	char *cb = "checked";
	if (intf->admin_up)
		cb = "";
	fprintf(fp, strif2,
		ifcnt, ifcnt, cb);

	fprintf(fp, "%s", strif3);
}
static void print_br(FILE *fp, RcpInterface *intf) {
	ifcnt++;
	char ip[16];
	*ip = '\0';
	char mask[3];
	*mask = '\0';
	if (intf->ip) {
		sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));
		sprintf(mask, "%d", mask2bits(intf->mask));
	}
	char mtu[10];
	*mtu = '\0';
	if (intf->mtu == 0)
		strcpy(mtu, "1500");
	else
		sprintf(mtu, "%d", intf->mtu);

	fprintf(fp, strif1,
		intf->name, ifcnt, ifcnt, intf->name,
		ifcnt, ifcnt, "bridge", 
		ifcnt, ifcnt, ip,
		ifcnt, ifcnt, mask,
		ifcnt, ifcnt, mtu);

	char *cb = "checked";
	if (intf->admin_up)
		cb = "";
	fprintf(fp, strif2,
		ifcnt, ifcnt, cb);

	fprintf(fp, "%s", strif3);
}
static void print_lo(FILE *fp, RcpInterface *intf) {
	ifcnt++;
	char ip[16];
	*ip = '\0';
	char mask[3];
	*mask = '\0';
	if (intf->ip) {
		sprintf(ip, "%d.%d.%d.%d", RCP_PRINT_IP(intf->ip));
		sprintf(mask, "%d", mask2bits(intf->mask));
	}
	char mtu[10];
	*mtu = '\0';
	if (intf->mtu == 0)
		strcpy(mtu, "16436");
	else
		sprintf(mtu, "%d", intf->mtu);

	char *vname = intf->name;
	if (rcpInterfaceVirtual(intf->name)) {
		vname = strchr(intf->name, ':');
		vname++;
	}
		
	fprintf(fp, strif1,
		intf->name, ifcnt, ifcnt, vname,
		ifcnt, ifcnt, "loopback", 
		ifcnt, ifcnt, ip,
		ifcnt, ifcnt, mask,
		ifcnt, ifcnt, mtu);
	fprintf(fp, "%s", strif3);
}

static void cgi_interfaces(FILE *fp) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->type == IF_ETHERNET)
				print_eth(fp, intf);
			else if (intf->type == IF_BRIDGE)
				print_br(fp, intf);
			else if (intf->type == IF_LOOPBACK)
				print_lo(fp, intf);
			else if (intf->type == IF_VLAN)
				print_vlan(fp, intf);
		}
	}	
}

static char *dhcprg = 
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>%d.</td><td><input name=\"sg%d1\" type=\"text\" id=\"idsg%d1\" class=\"sg1\" size=\"15\" maxlength=\"15\" value=\"%s\"/>&nbsp;&nbsp;\n"
" <input name=\"sg%d2\" type=\"text\" id=\"idsg%d2\" class=\"sg2\" size=\"15\" maxlength=\"15\" value=\"%s\"/>&nbsp;&nbsp;</td>\n"
"<td><input name=\"sg%d3\" type=\"text\" id=\"idsg%d3\" class=\"sg3\" size=\"15\" maxlength=\"15\" value=\"%s\"/>&nbsp;&nbsp;\n"
" <input name=\"sg%d4\" type=\"text\" id=\"idsg%d4\" class=\"sg4\" size=\"15\" maxlength=\"15\" value=\"%s\"/>&nbsp;&nbsp;</td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";

static void cgi_dhcpr_g(FILE *fp) {
	int group;
	for (group = 0; group < 4; group++) {
		char ip0[16];
		if (shm->config.dhcpr_server[group].valid && shm->config.dhcpr_server[group].server_ip[0])
			sprintf(ip0, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcpr_server[group].server_ip[0]));
		else
			*ip0 = '\0';

		char ip1[16];
		if (shm->config.dhcpr_server[group].valid && shm->config.dhcpr_server[group].server_ip[1])
			sprintf(ip1, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcpr_server[group].server_ip[1]));
		else
			*ip1 = '\0';

		char ip2[16];
		if (shm->config.dhcpr_server[group].valid && shm->config.dhcpr_server[group].server_ip[2])
			sprintf(ip2, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcpr_server[group].server_ip[2]));
		else
			*ip2 = '\0';

		char ip3[16];
		if (shm->config.dhcpr_server[group].valid && shm->config.dhcpr_server[group].server_ip[3])
			sprintf(ip3, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcpr_server[group].server_ip[3]));
		else
			*ip3 = '\0';
			
		fprintf(fp, dhcprg, 
			group + 1, group, group, ip0,	
			group, group, ip1,	
			group, group, ip2,	
			group, group, ip3);
	}
}

static char *dhcpi = 
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>%s<input name=\"r%d\" type=\"hidden\" id=\"idr%d\" class=\"ifname\" value=\"%s\"/>\n"
"<input name=\"r%dt\" type=\"hidden\" id=\"idr%dtype\" class=\"iftype\" value=\"%s\"/></td>\n"
"<td><input name=\"r%de\" type=\"checkbox\"  id=\"idr%de\" class=\"ifenable\" %s>Enable</td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";

static int dhcpcnt = -1;
static void print_ifrelay(FILE *fp, RcpInterface *intf) {
	dhcpcnt++;
	fprintf(fp, dhcpi,
		intf->name, dhcpcnt, dhcpcnt, intf->name,
		dhcpcnt, dhcpcnt, (intf->type == IF_ETHERNET)? "ethernet": "bridge",
		dhcpcnt, dhcpcnt, (intf->dhcp_relay)? "checked": "");
}

static void cgi_dhcpr_i(FILE *fp) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->type != IF_ETHERNET && intf->type != IF_BRIDGE && intf->type != IF_VLAN)
				continue;
			print_ifrelay(fp, intf);
		}
	}
}
static char *dhcpo =
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td colspan=\"3\" align=\"center\">\n"
"Max hops&nbsp;<input name=\"rh\" type=\"text\"  id=\"idrh\" class=\"maxhop\" size=\"2\" maxlength=\"2\" value=\"%d\">&nbsp;&nbsp;&nbsp;&nbsp;\n"
"Option 82&nbsp;<input name=\"ro\" type=\"checkbox\"  id=\"idro\" class=\"o82\" %s>&nbsp;&nbsp;&nbsp;&nbsp;\n"
"Service delay&nbsp;<input name=\"rsd\" type=\"text\"  id=\"idrsd\" class=\"sd\" size=\"3\" maxlength=\"3\" value=\"%d\">\n"
"</td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td></tr>\n";

static void cgi_dhcpr_o(FILE *fp) {
	fprintf(fp, dhcpo,
		(shm->config.dhcpr_max_hops != 0)? shm->config.dhcpr_max_hops: RCP_DHCPR_DEFAULT_MAX_HOPS,
		(shm->config.dhcpr_option82 != 0)? "checked": "",
		(shm->config.dhcpr_service_delay != 0)? shm->config.dhcpr_service_delay: RCP_DHCPR_DEFAULT_SERVICE_DELAY);
}

static void cgi_snmp_contact(FILE *fp) {
	fprintf(fp, "%s", shm->config.snmp.contact);
}
static void cgi_snmp_location(FILE *fp) {
	fprintf(fp, "%s", shm->config.snmp.location);
}
static void cgi_snmp_cname(FILE *fp) {
	if (shm->config.snmp.com_public)
		fprintf(fp, "public");
	else
		fprintf(fp, "%s", shm->config.snmp.com_passwd);
}

char *snmpu = 
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td><input name=\"r4.%d1\" type=\"text\" id=\"idr4.%d1\" class=\"uname\" size=\"31\" maxlength=\"31\" value=\"%s\"/></td>\n"
"<td><input name=\"r4.%d2\" type=\"password\" id=\"idr4.%d2\" class=\"upasswd\" size=\"31\" maxlength=\"64\" value=\"%s\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
static void cgi_snmp_users(FILE *fp) {
	int i;
	for (i = 0; i < RCP_SNMP_USER_LIMIT; i++) {
		RcpSnmpUser *u = &shm->config.snmp.user[i];
		if (u->name[0] == '\0')
			u->passwd[0] = '\0';

		fprintf(fp, snmpu,
			i, i, u->name,
			i, i, u->passwd);
	}

}

char *snmph =
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td><input name=\"r2h%d1\" type=\"text\" id=\"idr2h%d1\" class=\"ip\" size=\"15\" maxlength=\"15\" value=\"%s\"/>&nbsp;&nbsp&nbsp;&nbsp;&nbsp;</td>\n"
"<td><select name=\"status\" id=\"idr2h%d2\" class=\"ntype\">\n"
"<option value=\"traps\" %s>traps</option>\n"
"<option value=\"informs\" %s>informs</option>\n"
"</select>&nbsp;&nbsp&nbsp;&nbsp</td>\n"
"<td>v2c</td>\n"
"<td><input name=\"r2h%d3\" type=\"text\" id=\"idr2h%d3\" class=\"comstr\" size=\"16\" maxlength=\"16\" value=\"%s\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
static void cgi_snmp_host(FILE *fp) {
	int i;
	for (i = 0; i < RCP_SNMP_HOST_LIMIT; i++) {
		RcpSnmpHost *u = &shm->config.snmp.host[i];
		if (u->valid == 0)
			fprintf(fp, snmph,
				i, i, "",
				i,
				"selected",
				"",
				i, i, "");
		else {
			char addr[16];
			sprintf(addr, "%d.%d.%d.%d", RCP_PRINT_IP(u->ip));
			fprintf(fp, snmph,
				i, i, addr,
				i,
				(u->inform == 0)? "selected": "",
				(u->inform == 1)? "selected": "",
				i, i, u->com_passwd);
		}
	}
}

char *snmpnotif =
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td colspan=\"2\">SNMP Notifications</td>\n"
"<td colspan=\"2\">\n"
"<select name=\"status\" id=\"idr1\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
static void cgi_snmp_notif(FILE *fp) {
	if (shm->config.snmp.traps_enabled)
		fprintf(fp, snmpnotif, "selected", "");
	else
		fprintf(fp, snmpnotif, "", "selected");
}

static char *ripp =
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Update timer</td>\n"
"<td><input name=\"r2\" type=\"text\" id=\"idr2\" size=\"2\" maxlength=\"2\" value=\"%d\"/></td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Insert default route</td>\n"
"<td><select name=\"status\" id=\"idr1\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select>&nbsp;&nbsp;&nbsp</td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"


"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute loopback routes</td>\n"
"<td><select name=\"status\" id=\"idr5\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"


"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute connected routes</td>\n"
"<td><select name=\"status\" id=\"idr3\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>Metric&nbsp;&nbsp;<input name=\"r4\" type=\"text\" id=\"idr4\" size=\"2\" maxlength=\"2\" value=\"%d\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"

"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute static routes</td>\n"
"<td><select name=\"status\" id=\"idr7\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>Metric&nbsp;&nbsp;<input name=\"r8\" type=\"text\" id=\"idr8\" size=\"2\" maxlength=\"2\" value=\"%d\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"

"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute OSPF routes</td>\n"
"<td><select name=\"status\" id=\"idr9\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>Metric&nbsp;&nbsp;<input name=\"r10\" type=\"text\" id=\"idr10\" size=\"2\" maxlength=\"2\" value=\"%d\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";

static void cgi_rip_protocol(FILE *fp) {
	fprintf(fp, ripp,
		shm->config.rip_update_timer,
		(shm->config.rip_default_information)? "selected": "",
		(shm->config.rip_default_information == 0)? "selected": "",
		
		(shm->config.rip_redist_connected_subnets)? "selected": "",
		(shm->config.rip_redist_connected_subnets == 0)? "selected": "",

		(shm->config.rip_redist_connected)? "selected": "",
		(shm->config.rip_redist_connected == 0)? "selected": "",
		(shm->config.rip_redist_connected)? shm->config.rip_redist_connected: 1,
		

		(shm->config.rip_redist_static)? "selected": "",
		(shm->config.rip_redist_static == 0)? "selected": "",
		(shm->config.rip_redist_static)? shm->config.rip_redist_static: 1,

		(shm->config.rip_redist_ospf)? "selected": "",
		(shm->config.rip_redist_ospf == 0)? "selected": "",
		(shm->config.rip_redist_ospf)? shm->config.rip_redist_ospf: 1);
}

// find the corresponding network for a given interface, NULL if not found
static RcpRipPartner *find_network_for_interface(RcpInterface *intf) {
	ASSERT(intf != NULL);
	if (intf->ip == 0)
		return NULL;

	// walk through configured network
	RcpRipPartner *net;
	int i;
	
	for (i = 0, net = shm->config.rip_network; i < RCP_RIP_NETWORK_LIMIT; i++, net++) {
		if (!net->valid)
			continue;

		// first check the length of the two mask
		if (intf->mask >= net->mask) {
			// if the prefix matches, we've found it
			if ((intf->ip & net->mask) == (net->ip & net->mask))
				return net;
		}
	}

	// we have no rip network for this interface
	return NULL;
}
static char *ripn =
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>%s - %s<input name=\"rn%d\" type=\"hidden\" id=\"idrn%d\" class=\"ifnetwork\" value=\"%s\"/>\n"
"<input name=\"rn%dname\" type=\"hidden\" id=\"idrn%dname\" class=\"ifname\" value=\"%s\"/>\n"
"<input name=\"rn%dtype\" type=\"hidden\" id=\"idr%dtype\" class=\"iftype\" value=\"%s\"/></td>\n"
"<td><select name=\"status\" id=\"rn%de\" class=\"ifenable\" >\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>\n"
"<input name=\"rn%dp\" type=\"checkbox\"  id=\"idrn%dp\" class=\"ifpassive\" %s></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
#if 0
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td></td>\n"
"<td colspan=\"2\">MD5 password&nbsp;&nbsp;<input name=\"rn%dmd5\"  type=\"text\" id=\"rn%dmd5\" class=\"ifmd5\" size=\"16\" maxlength=\"16\" value=\"%s\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
#endif


static int ripcnt = -1;
static void print_ifrip(FILE *fp, RcpInterface *intf) {
	ripcnt++;
	char network[20];
	network[0] = '\0';
	
	if (intf->ip == 0)
		goto noconfig;
	sprintf(network, "%d.%d.%d.%d/%d", RCP_PRINT_IP(intf->ip & intf->mask), mask2bits(intf->mask));
	
	// find the network
	RcpRipPartner *p = find_network_for_interface(intf);
	if (!p)
		goto noconfig;
			
	fprintf(fp, ripn,
		intf->name, network, ripcnt, ripcnt, network,
		ripcnt, ripcnt, intf->name,
		ripcnt, ripcnt, (intf->type == IF_ETHERNET)? "ethernet": "bridge",
		ripcnt, "selected", "",
		ripcnt, ripcnt, (intf->rip_passive_interface)? "checked": "");
//		ripcnt, ripcnt, intf->rip_passwd);
	return;
		
noconfig:
	// interface not configured
	fprintf(fp, ripn,
		intf->name, network, ripcnt, ripcnt, network,
		ripcnt, ripcnt, intf->name,
		ripcnt, ripcnt, (intf->type == IF_ETHERNET)? "ethernet": "bridge",
		ripcnt, "", "selected",
		ripcnt, ripcnt, (intf->rip_passive_interface)? "checked": "");
//		ripcnt, ripcnt, intf->rip_passwd);
}

static void cgi_rip_net(FILE *fp) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->type != IF_ETHERNET && intf->type != IF_BRIDGE && intf->type != IF_VLAN)
				continue;
			print_ifrip(fp, intf);
		}
	}
}


static char *ospfp =
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Router ID</td>\n"
"<td colspan=\"2\"><input name=\"r20\" type=\"text\" id=\"idr20\" size=\"15\" maxlength=\"15\" value=\"%s\"/>&nbsp;&nbsp;</td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>SPF timer</td>\n"
"<td><input name=\"r2\" type=\"text\" id=\"idr2\" size=\"3\" maxlength=\"3\" value=\"%d\"/>&nbsp;&nbsp;</td>\n"
"<td>Hold time</td>\n"
"<td><input name=\"r2h\" type=\"text\" id=\"idr2h\" size=\"3\" maxlength=\"3\" value=\"%d\"/></td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"
"<tr>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Insert default route</td>\n"
"<td><select name=\"status\" id=\"idr1\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select>&nbsp;&nbsp;&nbsp</td>\n"
"<td></td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"

"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute loopback routes</td>\n"
"<td><select name=\"status\" id=\"idr5\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td></td>\n"
"<td></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"


"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute connected routes</td>\n"
"<td><select name=\"status\" id=\"idr3\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>E2 Metric&nbsp;&nbsp;<input name=\"r4\" type=\"text\" id=\"idr4\" size=\"8\" maxlength=\"8\" value=\"%d\"/>&nbsp;&nbsp;</td>\n"
"<td>Tag&nbsp;&nbsp;<input name=\"r4t\" type=\"text\" id=\"idr4t\" size=\"10\" maxlength=\"10\" value=\"%u\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"


"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute static routes</td>\n"
"<td><select name=\"status\" id=\"idr7\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>E2 Metric&nbsp;&nbsp;<input name=\"r8\" type=\"text\" id=\"idr8\" size=\"8\" maxlength=\"8\" value=\"%d\"/>&nbsp;&nbsp;</td>\n"
"<td>Tag&nbsp;&nbsp;<input name=\"r8t\" type=\"text\" id=\"idr8t\" size=\"10\" maxlength=\"10\" value=\"%u\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n"
"\n"

"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>Redistribute RIP routes</td>\n"
"<td><select name=\"status\" id=\"idr9\">\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td>E2 Metric&nbsp;&nbsp;<input name=\"r10\" type=\"text\" id=\"idr10\" size=\"8\" maxlength=\"8\" value=\"%d\"/>&nbsp;&nbsp;</td>\n"
"<td>Tag&nbsp;&nbsp;<input name=\"r10t\" type=\"text\" id=\"idr10t\" size=\"10\" maxlength=\"10\" value=\"%u\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";

static void cgi_ospf_protocol(FILE *fp) {
	char rid[16];
	if (shm->config.ospf_router_id)
		sprintf(rid, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.ospf_router_id));
	else
		rid[0] = '\0';
			
	fprintf(fp, ospfp,
		rid,
		shm->config.ospf_spf_delay, 
		shm->config.ospf_spf_hold, 
		(shm->config.ospf_redist_information)? "selected": "",
		(shm->config.ospf_redist_information == 0)? "selected": "",
		
		(shm->config.ospf_redist_connected_subnets)? "selected": "",
		(shm->config.ospf_redist_connected_subnets == 0)? "selected": "",

		(shm->config.ospf_redist_connected)? "selected": "",
		(shm->config.ospf_redist_connected == 0)? "selected": "",
		(shm->config.ospf_redist_connected)? shm->config.ospf_redist_connected: 20,
		shm->config.ospf_connected_tag,
		
		(shm->config.ospf_redist_static)? "selected": "",
		(shm->config.ospf_redist_static == 0)? "selected": "",
		(shm->config.ospf_redist_static)? shm->config.ospf_redist_static: 20,
		shm->config.ospf_static_tag,

		(shm->config.ospf_redist_rip)? "selected": "",
		(shm->config.ospf_redist_rip == 0)? "selected": "",
		(shm->config.ospf_redist_rip)? shm->config.ospf_redist_rip: 20,
		shm->config.ospf_rip_tag);
}




// test if the input subnet (ip,mask) can be matched with any
// network configured in ospf cli mode
static RcpOspfNetwork *shm_match_network(uint32_t ip, uint32_t mask) {
	if (ip == 0)
		return NULL;
		
	int i;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		if (!shm->config.ospf_area[i].valid)
			continue;
			
		int j;
		for (j = 0; j < RCP_OSPF_NETWORK_LIMIT; j++) {
			RcpOspfNetwork *net = &shm->config.ospf_area[i].network[j];

			if (!net->valid)
				continue;
			
			if (net->mask > mask) {
				continue;
			}
					
			if ((net->ip & net->mask) == (ip & net->mask)) {
				return net;
			}
		}
	}

	return NULL;	// no match
}

static char *ospfn =
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td>%s - %s<input name=\"rn%d\" type=\"hidden\" id=\"idrn%d\" class=\"ifnetwork\" value=\"%s\"/>\n"
"<input name=\"rn%dname\" type=\"hidden\" id=\"idrn%dname\" class=\"ifname\" value=\"%s\"/>\n"
"<input name=\"rn%dtype\" type=\"hidden\" id=\"idr%dtype\" class=\"iftype\" value=\"%s\"/></td>\n"
"<td><select name=\"status\" id=\"rn%de\" class=\"ifenable\" >\n"
"<option value=\"enabled\" %s>enabled</option>\n"
"<option value=\"disabled\" %s>disabled</option>\n"
"</select></td>\n"
"<td><input name=\"rn%da\" type=\"text\" id=\"rn%da\" class=\"ifarea\" size=\"10\" maxlength=\"10\" value=\"%u\"/></td>\n"
"<td><input name=\"rn%dh\" type=\"text\" id=\"rn%dh\" class=\"ifhello\" size=\"5\" maxlength=\"5\" value=\"%u\"/>\n"
"/ <input name=\"rn%dd\" type=\"text\" id=\"rn%dd\" class=\"ifdead\" size=\"5\" maxlength=\"5\" value=\"%u\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
#if 0
"<tr><td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"<td></td>\n"
"<td colspan=\"2\">MD5 password&nbsp;&nbsp;<input name=\"rn%dmd5\"  type=\"text\" id=\"rn%dmd5\" class=\"ifmd5\" size=\"16\" maxlength=\"16\" value=\"%s\"/></td>\n"
"<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
"</tr>\n";
#endif


static int ospfcnt = -1;
static void print_ifospf(FILE *fp, RcpInterface *intf) {
	ospfcnt++;
	char network[20];
	network[0] = '\0';
	
	if (intf->ip == 0)
		goto noconfig;
	sprintf(network, "%d.%d.%d.%d/%d", RCP_PRINT_IP(intf->ip & intf->mask), mask2bits(intf->mask));
	
	// find the network
	RcpOspfNetwork *n = shm_match_network(intf->ip & intf->mask, intf->mask);
	if (!n)
		goto noconfig;
	// find the area
	fprintf(fp, ospfn,
		intf->name, network, ospfcnt, ospfcnt, network,
		ospfcnt, ospfcnt, intf->name,
		ospfcnt, ospfcnt, (intf->type == IF_ETHERNET)? "ethernet": "bridge",
		ospfcnt, "selected", "",
		ospfcnt, ospfcnt, n->area_id,
		ospfcnt, ospfcnt, intf->ospf_hello,
		ospfcnt, ospfcnt, intf->ospf_dead);
	return;
noconfig:
	// interface not configured
	fprintf(fp, ospfn,
		intf->name, network, ospfcnt, ospfcnt, network,
		ospfcnt, ospfcnt, intf->name,
		ospfcnt, ospfcnt, (intf->type == IF_ETHERNET)? "ethernet": "bridge",
		ospfcnt, "", "selected",
		ospfcnt, ospfcnt, 0,
		ospfcnt, ospfcnt, 10,
		ospfcnt, ospfcnt, 40);
}

static void cgi_ospf_net(FILE *fp) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		if (shm->config.intf[i].name[0] != '\0') {
			RcpInterface *intf = &shm->config.intf[i];
			if (intf->type != IF_ETHERNET && intf->type != IF_BRIDGE && intf->type != IF_VLAN)
				continue;
			print_ifospf(fp, intf);
		}
	}
}

static void cgi_dhcps_enabled(FILE *fp) {
	if (shm->config.dhcps_enable)
		fprintf(fp, "selected");
}
static void cgi_dhcps_disabled(FILE *fp) {
	if (!shm->config.dhcps_enable)
		fprintf(fp, "selected");
}


static void cgi_dhcps_domain(FILE *fp) {
	fprintf(fp, "%s", shm->config.dhcps_domain_name);
}

static void cgi_dhcps_dns1(FILE *fp) {
	if (shm->config.dhcps_dns1)
		fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_dns1));
}
static void cgi_dhcps_dns2(FILE *fp) {
	if (shm->config.dhcps_dns2)
		fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_dns2));
}

static void cgi_dhcps_ntp1(FILE *fp) {
	if (shm->config.dhcps_ntp1)
		fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_ntp1));
}
static void cgi_dhcps_ntp2(FILE *fp) {
	if (shm->config.dhcps_ntp1)
		fprintf(fp, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_ntp2));
}

static int dcnt = 0;
char *dhcpsnet_1 = "<tr><td>Network</td>\n";
char *dhcpsnet_2 = 
"<td><input name=\"r%da\" type=\"text\" id=\"idr%da\" class=\"addr\" size=\"15\" maxlength=\"15\" value=\"%s\"/>\n"
"/ <input name=\"r%dm\" type=\"text\" id=\"idr%dm\" class=\"mask\" size=\"2\" maxlength=\"2\" value=\"%s\"/>\n"
"</td></tr>\n";
char *dhcpsnet_3 = "<tr><td>Range</td>"
"<td><input name=\"r%dr1\" type=\"text\" id=\"idr%dr1\" class=\"addr\" size=\"15\" maxlength=\"15\" value=\"%s\"/></td>\n"
"<td><input name=\"r%dr2\" type=\"text\" id=\"idr%dr2\" class=\"addr\" size=\"15\" maxlength=\"15\" value=\"%s\"/></td>\n"
"</tr>\n";
char *dhcpsnet_4 = "<tr><td>Default router</td>"
"<td><input name=\"r%dgw1\" type=\"text\" id=\"idr%dgw1\" class=\"addr\" size=\"15\" maxlength=\"15\" value=\"%s\"/></td>\n"
"<td><input name=\"r%dgw2\" type=\"text\" id=\"idr%dgw2\" class=\"addr\" size=\"15\" maxlength=\"15\" value=\"%s\"/></td>\n"
"</tr>\n";
char *dhcpsnet_5 = "<tr><td>Lease (d/h/m)</td><td>\n"
"<input name=\"r%dday\" type=\"text\" id=\"idr%dday\" class=\"day\" size=\"2\" maxlength=\"2\" value=\"%s\"/>\n"
"<input name=\"r%dhour\" type=\"text\" id=\"idr%dhour\" class=\"hour\" size=\"2\" maxlength=\"2\" value=\"%s\"/>\n"
"<input name=\"r%dminute\" type=\"text\" id=\"idr%dminute\" class=\"minute\" size=\"2\" maxlength=\"2\" value=\"%s\"/>\n"
"</td></tr>\n";

char *dhcpsnet_end = "<tr><td>&nbsp;</td></tr>";

static void cgi_dhcps_net(FILE *fp) {
	char s1[20];
	char s2[20];
	char s3[20];
	
	fprintf(fp, "%s", dhcpsnet_1);
	
	uint32_t ip;
	uint32_t mask;
	if (atocidr(shm->config.dhcps_network[dcnt].name, &ip, &mask)) {
		s1[0] ='\0';
		s2[0] ='\0';
	}
	else {
		sprintf(s1, "%d.%d.%d.%d", RCP_PRINT_IP(ip));
		sprintf(s2, "%d", mask2bits(mask));
	}
	fprintf(fp, dhcpsnet_2, 
		dcnt, dcnt, s1,
		dcnt, dcnt, s2);

	*s1 = '\0';
	*s2 = '\0';
	if (shm->config.dhcps_network[dcnt].range_start)
		sprintf(s1, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_network[dcnt].range_start));
	if (shm->config.dhcps_network[dcnt].range_end)
		sprintf(s2, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_network[dcnt].range_end));
	fprintf(fp, dhcpsnet_3,
		dcnt, dcnt, s1,
		dcnt, dcnt, s2);

	*s1 = '\0';
	*s2 = '\0';
	if (shm->config.dhcps_network[dcnt].gw1)
		sprintf(s1, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_network[dcnt].gw1));
	if (shm->config.dhcps_network[dcnt].gw2)
		sprintf(s2, "%d.%d.%d.%d", RCP_PRINT_IP(shm->config.dhcps_network[dcnt].gw2));
	fprintf(fp, dhcpsnet_4,
		dcnt, dcnt, s1,
		dcnt, dcnt, s2);

	// uninitialized lease
	if (shm->config.dhcps_network[dcnt].lease_days == 0 &&
	    shm->config.dhcps_network[dcnt].lease_hours == 0 &&
	    shm->config.dhcps_network[dcnt].lease_minutes == 0) {
		sprintf(s1, "%d", RCP_DHCP_LEASE_DAY_DEFAULT);
		sprintf(s2, "%d", RCP_DHCP_LEASE_HOUR_DEFAULT);
		sprintf(s3, "%d", RCP_DHCP_LEASE_MINUTE_DEFAULT);
	}
	else {
		sprintf(s1, "%d", shm->config.dhcps_network[dcnt].lease_days);
		sprintf(s2, "%d", shm->config.dhcps_network[dcnt].lease_hours);
		sprintf(s3, "%d", shm->config.dhcps_network[dcnt].lease_minutes);
	}
	fprintf(fp, dhcpsnet_5,
		dcnt, dcnt, s1,
		dcnt, dcnt, s2,
		dcnt, dcnt, s3);
	
	fprintf(fp, "%s", dhcpsnet_end);
	dcnt++;
}


HttpCgiBind httpcgibind[] = {
	{"dhcps_domain", cgi_dhcps_domain},
	{"dhcps_dns1", cgi_dhcps_dns1},
	{"dhcps_dns2", cgi_dhcps_dns2},
	{"dhcps_ntp1", cgi_dhcps_ntp1},
	{"dhcps_ntp2", cgi_dhcps_ntp2},
	{"dhcps_net", cgi_dhcps_net},
	{"dhcps_enabled", cgi_dhcps_enabled},
	{"dhcps_disabled", cgi_dhcps_disabled},
	{"ospf_net", cgi_ospf_net},
	{"ospf_protocol", cgi_ospf_protocol},
	{"rip_net", cgi_rip_net},
	{"rip_protocol", cgi_rip_protocol},
	{"snmp_notif", cgi_snmp_notif},
	{"snmp_host", cgi_snmp_host},
	{"snmp_users", cgi_snmp_users},
	{"snmp_cname", cgi_snmp_cname},
	{"snmp_location", cgi_snmp_location},
	{"snmp_contact", cgi_snmp_contact},
	{"dhcpr_o", cgi_dhcpr_o},
	{"dhcpr_g", cgi_dhcpr_g},
	{"dhcpr_i", cgi_dhcpr_i},
	{"interfaces", cgi_interfaces},
	{"logsnmp_enabled", cgi_logsnmp_enabled},
	{"logsnmp_disabled", cgi_logsnmp_disabled},
	{"logs_emergencies", cgi_logs_emergencies},
	{"logs_alerts", cgi_logs_alerts},
	{"logs_critical", cgi_logs_critical},
	{"logs_errors", cgi_logs_errors},
	{"logs_warnings", cgi_logs_warnings},
	{"logs_notifications", cgi_logs_notifications},
	{"logs_informational", cgi_logs_informational},
	{"logs_debugging", cgi_logs_debugging},
	{"logs_ip", cgi_logs_ip},
	{"logs_port", cgi_logs_port},
	{"dns_hostname", cgi_dns_hostname},
	{"dns_domain", cgi_dns_domain},
	{"dns_ns1", cgi_dns_ns1},
	{"dns_ns2", cgi_dns_ns2},
	{"dnss_enabled", cgi_dnss_enabled},
	{"dnss_disabled", cgi_dnss_disabled},
	{"ntps_enabled", cgi_ntps_enabled},
	{"ntps_disabled", cgi_ntps_disabled},
	{"ntph_get", cgi_ntph_get},
	{"telnet_enabled", cgi_telnet_enabled},
	{"telnet_disabled", cgi_telnet_disabled},
	{"telnet_port", cgi_telnet_port},
	{"ftp_enabled", cgi_ftp_enabled},
	{"ftp_disabled", cgi_ftp_disabled},
	{"tftp_enabled", cgi_tftp_enabled},
	{"tftp_disabled", cgi_tftp_disabled},
	{NULL, NULL}
};

