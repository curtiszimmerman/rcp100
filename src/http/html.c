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
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "http.h"
#include "../../version.h"

// svg plot sizes
static int w = 550, h = 200;	
static int xmargin = 20, ymargin = 20;

// current session
static unsigned session;
unsigned html_session(void) {
	return session;
}

void html_set_secret(void) {
	printf("<script language=\"javascript\">\n");
	
	printf("function change()\n");
	printf("{\n");
	printf("		localStorage.secret = idp.value;\n");
	printf("		document.location = \"configuration.html;%u\";\n", html_session());
	printf("}\n");
	printf("</script>\n");
	
	printf("<form action=\"javascript:change()\">\n");
	printf("<br/><br/><center>Please enter HTTP configuration password</center><br/><br/>\n");
	printf("<center><input type=\"password\" name=\"mypassword\" id=\"idp\">\n");
	printf("<button type=\"button\" onClick=\"change()\">Continue</button></center><br/><br/>\n");
	printf("</form>\n");
}

void html_redir(unsigned sid) {
	printf("<!DOCTYPE html><html><head><title>redirect</title>"
		"<meta http-equiv=\"refresh\" content=\"0;url=index.html;%u \" />"
		"<link rel=\"icon\" type=\"image/png\" href=\"favicon.png\" />"
		"</head><body></body></html>\n",
		sid);
}
void html_redir_passwd(unsigned sid) {
	printf("<!DOCTYPE html><html><head><title>redirect</title>"
		"<meta http-equiv=\"refresh\" content=\"0;url=cfgpasswd.html;%u \" />"
		"<link rel=\"icon\" type=\"image/png\" href=\"favicon.png\" />"
		"</head><body></body></html>\n",
		sid);
}

void html_start(const char *title, const char *url, unsigned sid) {
	ASSERT(title);
	
	printf("<!DOCTYPE html><html><head><title>%s</title><meta http-equiv=\"refresh\" content=\"%d;url=%s;%u\" />"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		"<link rel=\"icon\" type=\"image/png\" href=\"favicon.png\"/>"
		"<style>a:link{color:blue} h1 {text-align:center} body {background-color:#f0f0ff}\n"
		"#page{width:700px; background-color:white;border:1px solid;border-radius:10px;}\n"
		"#isle{border:1px solid;border-radius:10px;background-color:#E3E9FA;margin:20px;padding-top:5px}</style>\n"
		" </head><body>\n",
		title, RCP_HTTP_TIMEOUT, url, sid);
	printf("<div id=\"page\">\n");
	printf("<br/><h1>%s</h1>\n", shm->config.hostname);
	printf("<hr color=\"blue\"/>\n");
	printf("<p  style=\"text-align:center;\">");
	
	if (strcmp(url, "index.html") == 0 || strcmp(url, "index-log.html") == 0)
		printf("Home ");
	else
		printf("<a href=\"index.html;%u\">Home</a>&nbsp;&nbsp;&nbsp;", sid);
	
	if (shm->config.http_passwd[0] != '\0') {
		if (strcmp(url, "configuration.html") == 0)
			printf("Configuration ");
		else
			printf("<a href=\"setsecret.html;%u\">Configuration</a>&nbsp;&nbsp;&nbsp;", sid);
	}

	if (strncmp(url, "system", 6) == 0)
		printf("System ");
	else
		printf("<a href=\"system.html;%u\">System</a>&nbsp;&nbsp;&nbsp;", sid);
		
	if (strncmp(url, "interfaces", 10) == 0)
		printf("Interfaces ");
	else
		printf("<a href=\"interfaces.html;%u\">Interfaces</a>&nbsp;&nbsp;&nbsp;", sid);

	if (strncmp(url, "routing", 7) == 0)
		printf("Routing ");
	else
		printf("<a href=\"routing.html;%u\">Routing</a>&nbsp;&nbsp;&nbsp;", sid);

	if (strncmp(url, "services", 8) == 0)
		printf("Services ");
	else
		printf("<a href=\"services.html;%u\">Services</a>&nbsp;&nbsp;&nbsp;", sid);

	if (strcmp(url, "netmon.html") == 0)
		printf("Monitor ");
	else
		printf("<a href=\"netmon.html;%u\">Monitor</a>", sid);

	printf("</p><hr color=\"blue\"/>\n");
	session = sid;
}


void html_end(void) {
	printf("<br/></div>\n");
	printf("</body></html>\n");
}

static void html_index_top(void) {
	int i;
	FILE *fp;
	printf("<div id=\"isle\">\n");
	printf("<table align=\"center\" width=\"98%%\"><tr><td style=\"vertical-align:top\">\n");
	printf("<blockquote><pre>\n");

	char *product = "RCP100";
	
	// print software version
	printf("%s software version %s\n", product, RCP_VERSION);
	
	// print installed modules
	for (i = 0; i < RCP_MODULE_LIMIT; i++) {
		if (shm->config.module[i].name[0] != '\0')
			printf("%s Module version %s\n",
				shm->config.module[i].name, 
				shm->config.module[i].version);
	} 
		
	
	// print kernel version
	char *rv = cliRunProgram("uname -mrs");
	if (rv != NULL) {
		printf("Kernel version: %s", rv);
		free(rv);
	}

	// print disk stats
	struct statvfs fiData;
	if ((statvfs("/",&fiData)) == 0 ) {
		double kb = fiData.f_bsize / 1024;
		double gtotal = (((double) fiData.f_blocks * kb) / 1024) / 1024;
		double gfree = (((double) fiData.f_bfree * kb) / 1024) / 1024;
		printf("%.02fG total disk storage, free %.02fG\n", gtotal, gfree);
	}

	// get RAM data
	if ((fp = fopen("/proc/meminfo", "r")) != NULL) {
		char line[100 + 1];
		int memtotal = 0;
		int memfree = 0;
		int memcached = 0;
		int swaptotal = 0;
		int swapfree = 0;
		
		while (fgets(line, 100, fp) != NULL) {
			if (strncmp(line, "MemTotal:", 9) == 0)
				sscanf(line + 9, "%d", &memtotal);
			else if (strncmp(line, "MemFree:", 8) == 0)
				sscanf(line + 8, "%d", &memfree);
			else if (strncmp(line, "Cached:", 7) == 0)
				sscanf(line + 7, "%d", &memcached);
			else if (strncmp(line, "SwapTotal:", 10) == 0)
				sscanf(line + 10, "%d", &swaptotal);
			else if (strncmp(line, "SwapFree:", 9) == 0)
				sscanf(line + 9, "%d", &swapfree);
			
		}
		// add free and cached memory
		memfree += memcached;
		printf("%dM total memory, free %dM (%.2f%%)\n",
			memtotal / 1024, memfree / 1024, ((double) memfree / (double) memtotal) * 100);
		printf("%dM swap total memory, free %dM (%.2f%%)\n",
			swaptotal / 1024, swapfree / 1024, ((double) swapfree / (double) swaptotal) * 100);
		fclose(fp);
	}

	// print uptime
	if ((fp = fopen("/proc/uptime", "r")) != NULL) {
		// first number is the number of seconds the system has been up and running
		int total, days, hours, seconds, minutes;
		int rv = fscanf(fp, "%d.", &total);
		if (rv == 1) {
			seconds = total % 60;
			total /= 60; minutes = total % 60;
			total /= 60; hours = total % 24;
			days = total / 24;
			printf("System uptime %d %s, %d %s, %d %s, %d %s\n", 
				days, (days == 1)? "day": "days",
				hours, (hours == 1)? "hour": "hours",
				minutes, (minutes == 1)? "minute": "minutes",
				seconds, (seconds == 1)? "second": "seconds");
		}
		fclose(fp);
	}
	printf("</pre></blockquote>\n");
	printf("</td></tr></table></div>\n");
//	printf("<br/><br/>\n");

}

void html_index(void) {
	html_index_top();
	
	// print last 10 log entries
	printf("<p style=\"color:blue;margin-left:20px;\">System Log</p>\n");
	printf("<blockquote><pre>\n");
	RcpLogEntry *ptr = shm->log_head;
	char *month[] = {
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec"
	};
	
	int i = 0;
	while (ptr != NULL && i < 10) {
		ASSERT(ptr->ts != 0);
		if (ptr->ts != 0) {
			struct tm *tm_log = localtime(&ptr->ts);
			const char *level = rcpLogLevel(ptr->level);
			const char *fc = rcpLogFacility(ptr->facility);
			const char *proc = rcpProcName(ptr->proc);
			
			int len = strlen((char *) ptr->data);
			if (len != 0) {
				printf("%s %d %d:%02d:%02d %s-%s (%s):\n    %s\n",
					month[tm_log->tm_mon], 
					tm_log->tm_mday,
					tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec,
					proc, fc, level, ptr->data);					
			}
		}

		ptr = ptr->next;
		i++;
	}
	printf("<a href=\"index-log.html;%u\">more...</a>\n", html_session());
	printf("</pre></blockquote>\n");
	printf("<br/><br/>\n");
}

void html_index_log(void) {
	html_index_top();

	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("System Log</p>\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre>\n");
	fflush(0);
	int rv= system("/opt/rcp/bin/cli --html --command \"show logging buffer | no\"");
	(void) rv;
	fflush(0);
	printf("<a href=\"index.html;%u\">less...</a>\n", html_session());
	printf("</pre></blockquote>\n");
}

void html_system(void) {
	int i;
	FILE *fp;

	printf("<div id=\"isle\">\n");
	printf("<table align=\"center\" width=\"98%%\"><tr><td style=\"vertical-align:top\">\n");

	printf("<blockquote><pre>\n");
	printf("<b>%-15.15s</b> <b>Status</b>\n",
		"Process");
	
	for (i = 0; i < (RCP_PROC_MAX + 2); i++) {
		if (shm->config.pstats[i].proc_type != 0) {
			// extract process stats
			const char *proc_name = rcpProcExecutable(shm->config.pstats[i].proc_type);
			
			// check process status
			int status = 0;
			char fname[100];
			sprintf(fname, "/proc/%u/status", shm->config.pstats[i].pid);
			struct stat st;
			if (stat(fname, &st) == 0)
				status = 1;
			
			// print it on cli screen
			printf("%-15.15s ", proc_name);
			if (status)
				svg_checkmark(15);	
			else
				svg_cross(15);	
			printf("\n");
		}
	}
	printf("</pre></blockquote>\n");
	
	printf("</td><td style=\"vertical-align:top\">\n");
	printf("<blockquote><pre>\n");
	
	int memtotal = 0;
	int memfree = 0;
	int memcached = 0;
	int swaptotal = 0;
	int swapfree = 0;

	// get RAM data
	if ((fp = fopen("/proc/meminfo", "r")) != NULL) {
		char line[100 + 1];
		
		while (fgets(line, 100, fp) != NULL) {
			if (strncmp(line, "MemTotal:", 9) == 0)
				sscanf(line + 9, "%d", &memtotal);
			else if (strncmp(line, "MemFree:", 8) == 0)
				sscanf(line + 8, "%d", &memfree);
			else if (strncmp(line, "Cached:", 7) == 0)
				sscanf(line + 7, "%d", &memcached);
			else if (strncmp(line, "SwapTotal:", 10) == 0)
				sscanf(line + 10, "%d", &swaptotal);
			else if (strncmp(line, "SwapFree:", 9) == 0)
				sscanf(line + 9, "%d", &swapfree);
		}
	}
	printf("<b>Memory</b>\n");
	printf("%-10.10s %.02f MB\n", "Total", (float) memtotal / (float) 1024);
	printf("%-10.10s %.02f MB\n", "Free", (float) memfree / (float) 1024 +  (float) memcached / (float) 1024);
	printf("%-10.10s %.02f MB\n", "Swap", (float) swaptotal / (float) 1024);
	printf("%-10.10s %.02f MB\n", "Swap Free", (float) swapfree / (float) 1024);
	printf("\n");

	// extract cpu load by reading /proc/loadavg
	printf("<b>CPU Load</b>\n");
	int nprocs = (int) sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs == -1) {
		nprocs = 1;
	}
//printf("nprocs %d\n", nprocs);	
	fp = fopen("/proc/loadavg", "r");
	float load1;
	float load5;
	float load15;
	char buf[100 + 1];
	if (fp && fgets(buf, 100, fp)) {
		sscanf(buf, "%f %f %f", &load1, &load5, &load15);
		fclose(fp);
	}
	unsigned l1 = (unsigned) (load1 * 100 / nprocs);
	unsigned l5 = (unsigned) (load5 * 100 / nprocs);
	unsigned l15 = (unsigned) (load15 * 100 / nprocs);
	sprintf(buf, "%u%%", l1);
	printf("%-10.10s %-5.5s ", "1-min", buf);
	svg_hbar(l1, 10);
	printf("\n");
	sprintf(buf, "%u%%", l5);
	printf("%-10.10s %-5.5s ", "5-min", buf);
	svg_hbar(l5, 10);
	printf("\n");
	sprintf(buf, "%u%%", l15);
	printf("%-10.10s %-5.5s ", "15-min", buf);
	svg_hbar(l15, 10);
	printf("\n");
	printf("</pre></blockquote>\n");

	printf("</td></tr></table></div>\n");
	
//	printf("<br/>\n");
//	printf("<br/>\n");
	
	// print cpu utilization
	svg("CPU Load (%)", w, h, xmargin, ymargin, shm->stats.cpu,  RCP_MAX_STATS);
	printf("<br/>\n");
	
	// print free memory
	svg("Free Memory (%)", w, h, xmargin, ymargin, shm->stats.freemem,  RCP_MAX_STATS);
	printf("<br/>\n");
	
}

static int is_mac_allzero(unsigned char *mac) {
	int i;
	for (i = 0; i < 6; i++)
		if (mac[i] != 0)
			return 0;
	
	return 1;
}

static void interfaces_header(void) {
	int i;

	// list the interfaces
	printf("<div id=\"isle\">\n");
	printf("<table align=\"center\" width=\"98%%\"><tr><td style=\"vertical-align:top\">\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre><b>\n");
	printf("%-16.16s %-8.8s     %-20.20s    %s</b>\n",
		"Interface", "Type", "IP", "Status");
	
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;

		char buf[1000];
		char *type = "";
		if (intf->type == IF_ETHERNET)
			type = "ethernet";
		else if (intf->type == IF_LOOPBACK)
			type = "loopback";
		if (intf->type == IF_BRIDGE)
			type = "bridge";
		if (intf->type == IF_VLAN)
			type = "VLAN";
		
		char ip[30];
		sprintf(ip, "%d.%d.%d.%d/%d",
			RCP_PRINT_IP(intf->ip), mask2bits(intf->mask));
		
		int status = 0;
		if (intf->type == IF_LOOPBACK) {			
			sprintf(buf, "%-16.16s %-8.8s     %-20.20s    ",
				intf->name,
				type,
				ip);
			status = 1;
		}
		else {
			sprintf(buf, "%-16.16s %-8.8s     %-20.20s    ",
				intf->name,
				type,
				ip);
			if (!intf->admin_up || !intf->link_up)
				status = 0;
			else
				status = 1;
		}
		char *ptr = buf;
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		printf("<a href=\"interfaces-%d.html;%u\">%s</a>%s",
			i, session, intf->name, ptr);
		if (status)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}
	printf("</pre></blockquote>\n");
	printf("</td></tr></table></div>\n");
//	printf("<br/><br/>\n");
}

void print_interface(int index) {
	int i;

	if (index >= RCP_INTERFACE_LIMITS) {
		printf("<blockquote><pre>\n");
		printf("Interface data not available\n");
		printf("</pre></blockquote>\n");
		return;
	}
		
	// get current interface counts	
	RcpIfCounts *ifcounts = rcpGetInterfaceCounts();

	RcpInterface *intf = &shm->config.intf[index];
	if (intf->name[0] == '\0') {
		printf("<blockquote><pre>\n");
		printf("Interface data not available\n");
		printf("</pre></blockquote>\n");
		return;
	}
	
	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	printf("Interface %s</p>\n", intf->name);

	printf("<blockquote><pre>\n");
	printf("Interface %s admin %s, link %s\n",
		intf->name,
		(intf->admin_up)? "UP": "DOWN",
		(intf->link_up)? "UP": "DOWN");

	if (!is_mac_allzero(intf->mac))
		printf("MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
			RCP_PRINT_MAC(intf->mac));

	if (intf->type == IF_VLAN) 
		printf("VLAN ID %u, parent interface %s\n",
			intf->vlan_id,  shm->config.intf[intf->vlan_parent].name);

	if (intf->ip != 0 && intf->mask != 0)
		printf("IP address %d.%d.%d.%d, mask %d.%d.%d.%d\n",
			RCP_PRINT_IP(intf->ip),
			RCP_PRINT_IP(intf->mask));
	
	if (intf->mtu)
		printf("MTU %d\n", intf->mtu);

	if (!rcpInterfaceVirtual(intf->name)) {
		// find current counts
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
			printf("TX: %llu packets, %llu bytes, %llu errors, %llu dropped\n",
				found->tx_packets, found->tx_bytes, found->tx_errors, found->tx_dropped);
			printf("RX: %llu packets, %llu bytes, %llu errors, %llu dropped\n",
				found->rx_packets, found->rx_bytes, found->rx_errors, found->rx_dropped);
		}
	}
	
	printf("</pre></blockquote>\n");

	// print rx bps
	char name1[100];
	char name2[100];
	sprintf(name1, "%s RX (Kbps)", intf->name);
	sprintf(name2, "%s TX (Kbps)", intf->name);
	svg2(name1, name2, w, h, xmargin, ymargin, intf->stats.rx_kbps, intf->stats.tx_kbps, RCP_MAX_STATS);
	printf("<br/><br/>\n");
	
	if (intf->type == IF_LOOPBACK)
		return;

	// ospf
	int ospf_configured = 0;
	RcpOspfArea *area = shm->config.ospf_area;
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++, area++) {
		if (area->valid) {
			// look if the interface is on any network inside this area
			int j;
			for (j = 0; j < RCP_OSPF_NETWORK_LIMIT; j++) {
				RcpOspfNetwork *net = &area->network[j];
				if (!net->valid)
					continue;
				if (net->mask > intf->mask)
					continue;
				if ((net->ip & net->mask) == (intf->ip & net->mask)) {
					ospf_configured = 1;
					break;
				}
			}
			
			
			if (ospf_configured)
				break;
		}
	}
	
	if (ospf_configured) {
		// wait for ospf to store the stats
		if (shm->stats.ospf_active_locked)
			sleep(1);
			
		printf("<p style=\"color:blue;margin-left:20px;\">\n");
		printf("OSPF</p>\n");
		printf("<blockquote><pre>\n");
		printf("Router ID %d.%d.%d.%d, network type BROADCAST, cost %u\n",
			RCP_PRINT_IP(shm->config.ospf_router_id),
			intf->ospf_cost);

		if (intf->stats.ospf_state[0] == '\0')
			printf("Transmit delay is 0, priority %u\n",
				intf->ospf_priority);
		else
			printf("Transmit delay is 0, network state %s, priority %u\n",
				intf->stats.ospf_state,
				intf->ospf_priority);

		printf("Hello interval %u, dead interval %u, wait time %u, retransmit interval %u\n",
			intf->ospf_hello,
			intf->ospf_dead,
			intf->ospf_dead,
			intf->ospf_rxmt);

		if (intf->stats.ospf_dr && intf->stats.ospf_bdr)
			printf("Designated router %d.%d.%d.%d, backup designated router %d.%d.%d.%d\n",
				RCP_PRINT_IP(intf->stats.ospf_dr),	
				RCP_PRINT_IP(intf->stats.ospf_bdr));
	
		// packet counts
		printf("Hello packets received %u, sent %u, errors %u\n",
			intf->stats.ospf_rx_hello, intf->stats.ospf_tx_hello, intf->stats.ospf_rx_hello_errors);
		printf("Database Description packets received %u, sent %u, rxmt %u, errors %u\n",
			intf->stats.ospf_rx_db, intf->stats.ospf_tx_db, intf->stats.ospf_tx_db_rxmt, intf->stats.ospf_rx_db_errors);
		printf("LS Request packets received %u, sent %u, rxmt %u, errors %u\n",
			intf->stats.ospf_rx_lsrq, intf->stats.ospf_tx_lsrq, intf->stats.ospf_tx_lsrq_rxmt, intf->stats.ospf_rx_lsrq_errors);
		printf("LS Update packets received %u, sent %u, rxmt %u, errors %u\n",
			intf->stats.ospf_rx_lsup, intf->stats.ospf_tx_lsup, intf->stats.ospf_tx_lsup_rxmt, intf->stats.ospf_rx_lsup_errors);
		printf("LS Acknowledgment packets received %u, sent %u, errors %u\n",
			intf->stats.ospf_rx_lsack, intf->stats.ospf_tx_lsack, intf->stats.ospf_rx_lsack_errors);
		printf("</pre></blockquote>\n");
	}


	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	printf("DHCP</p>\n");
	printf("<blockquote><pre>\n");
	uint32_t err = intf->stats.dhcp_err_rx + intf->stats.dhcp_err_hops + intf->stats.dhcp_err_pkt +
		intf->stats.dhcp_err_not_configured + intf->stats.dhcp_err_server_notconf +
		intf->stats.dhcp_err_outgoing_intf + intf->stats.dhcp_err_giaddr;
	if (intf->dhcp_relay == 0)
		printf("DHCP Relay is not configured for this interface.\n");
		
	else
 		printf("DHCP Relay configured is configured for this interface.\n");
	
	printf("%u packets received, %u packets transmited, %u packet errors\n\n",
			 intf->stats.dhcp_rx, intf->stats.dhcp_tx, err);
			 
	if (intf->stats.dhcp_rx) {
		printf("RX stats:\n\n");
		printf("     DHCPDISCOVER: %u\n", intf->stats.dhcp_rx_discover);
		printf("     DHCPREQUEST: %u\n", intf->stats.dhcp_rx_request);
		printf("     DHCPDECLINE: %u\n", intf->stats.dhcp_rx_decline);
		printf("     DHCPRELEASE: %u\n", intf->stats.dhcp_rx_release);
		printf("     DHCPINFORM: %u\n", intf->stats.dhcp_rx_inform);
		printf("     DHCPOFFER: %u\n", intf->stats.dhcp_rx_offer);
		printf("     DHCPACK: %u\n", intf->stats.dhcp_rx_ack);
		printf("     DHCPNACK: %u\n\n", intf->stats.dhcp_rx_nack);
		printf("\n");
	}
	if (intf->stats.dhcp_tx) {
		printf("TX stats:\n\n");
		printf("     DHCPDISCOVER: %u\n", intf->stats.dhcp_tx_discover);
		printf("     DHCPREQUEST: %u\n", intf->stats.dhcp_tx_request);
		printf("     DHCPDECLINE: %u\n", intf->stats.dhcp_tx_decline);
		printf("     DHCPRELEASE: %u\n", intf->stats.dhcp_tx_release);
		printf("     DHCPINFORM: %u\n", intf->stats.dhcp_tx_inform);
		printf("     DHCPOFFER: %u\n", intf->stats.dhcp_tx_offer);
		printf("     DHCPACK: %u\n", intf->stats.dhcp_tx_ack);
		printf("     DHCPNACK: %u\n\n", intf->stats.dhcp_tx_nack);
		printf("\n");
	}
	printf("</pre></blockquote>\n");
}

void html_interfaces(int index) {
	interfaces_header();
	print_interface(index);
}

int count_active_neighbors(int type) {
	RcpActiveNeighbor *an;
	int rv = 0;
	if (type == 0) {
		an = &shm->stats.rip_active[0];
		if (shm->stats.rip_active_locked)
			sleep(1);
	}
	else {
		an = &shm->stats.ospf_active[0];
		if (shm->stats.ospf_active_locked)
			sleep(1);
	}
		
	int i;
	for (i = 0; i < RCP_ACTIVE_NEIGHBOR_LIMIT; i++, an++) {
		if (an->valid == 0)
			break;
#if 0
		printf("%d.%d.%d.%d, %d.%d.%d.%d/%d, %d.%d.%d.%d<br/>\n",
			RCP_PRINT_IP(an->ip),
			RCP_PRINT_IP(an->network),
			an->netmask_cnt,
			RCP_PRINT_IP(an->if_ip));
#endif
		rv++;
	}
	return rv;
}

static void routing_header() {
	printf("<div id=\"isle\">\n");
	printf("<table align=\"center\" width=\"98%%\"><tr><td style=\"vertical-align:top\">\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre>\n");
	routing_table_summary();
	printf("</pre></blockquote>\n");
	printf("</td><td style=\"vertical-align:top\">\n");

	printf("<blockquote><pre>\n");
	printf("<a href=\"routing-acl.html;%u\">ACL</a>&nbsp;&nbsp;"
		"<a href=\"routing-nat.html;%u\">NAT</a>\n\n",
		html_session(), html_session());
	routing_arp_summary();
	printf("\n");
	routing_ospf_summary();
	printf("\n");
	routing_rip_summary();
	printf("</pre></blockquote>\n");
	printf("</td></tr></table></div>\n");
//	printf("<br/><br/>\n");
}

void html_routing(void) {
	routing_header();
	
	// routing table
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("Routing Table</p>\n");
	printf("<blockquote><pre>\n");
	routing_table_full();
	printf("</pre></blockquote>\n");
	printf("<br/><br/>\n");
}

void html_routing_acl(void) {
	routing_header();

	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("Access Control Lists</p>\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre>\n");
	fflush(0);
	int rv = system("/opt/rcp/bin/cli --html --command \"show access-list statistics | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

void html_routing_nat(void) {
	routing_header();

	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("Network Address Translation</p>\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre>\n");
	fflush(0);
	int rv = system("/opt/rcp/bin/cli --html --command \"show ip nat | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

void html_routing_arp(void) {
	routing_header();
	
	// arp table
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("ARP Table</p>\n");
	printf("<blockquote><pre>\n");
	routing_arp_full();
	printf("</pre></blockquote>\n");
//	printf("<br/><br/>\n");
}

void html_routing_ospf(void) {
	routing_header();
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("OSPF Protocol</p>\n");

	int cnt = count_active_neighbors(1);
	if (cnt) {
		svg_net_diag(cnt, shm->stats.ospf_active, shm->config.hostname, 1, shm->config.ospf_router_id);
	}
	routing_ospf();
}

void html_routing_ospf_database(void) {
	routing_header();
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("OSPF Database</p>\n");

	fflush(0);
	printf("<blockquote><pre>\n");
	fflush(0);
	int rv = system("/opt/rcp/bin/cli --html --command \"show ip ospf database | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

void html_routing_ospf_neighbors(void) {
	routing_header();
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("OSPF Neighbors</p>\n");

	fflush(0);
	printf("<blockquote><pre>\n");
	fflush(0);
	int rv = system("/opt/rcp/bin/cli --html --command \"show ip ospf neighbor | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

void html_routing_rip(void) {
	routing_header();
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("RIP Protocol</p>\n");
	int cnt = count_active_neighbors(0);
	if (cnt) {
		svg_net_diag(cnt, shm->stats.rip_active, shm->config.hostname, 0, 0);
	}
	routing_rip();
}

void html_routing_rip_database(void) {
	routing_header();

	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("RIP Database</p>\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre>\n");
	fflush(0);
	int rv = system("/opt/rcp/bin/cli --html --command \"show ip rip database | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

static int ntp_configured;
static int dhcp_configured;
static uint32_t dhcp_rx;
static uint32_t dhcp_rx_discover;
static uint32_t dhcp_rx_request;
static uint32_t dhcp_rx_decline;
static uint32_t dhcp_rx_release;
static uint32_t dhcp_rx_inform;
static uint32_t dhcp_rx_offer;
static uint32_t dhcp_rx_ack;
static uint32_t dhcp_rx_nack;
static uint32_t dhcp_err;
static int snmp_configured;
static RcpSnmpStats rstats;

static void services_header(void) {
	int i;

	ntp_configured = 0;
	dhcp_configured = 0;
	snmp_configured = 0;
	dhcp_rx = 0;
	dhcp_rx_discover = 0;
	dhcp_rx_request = 0;
	dhcp_rx_decline = 0;
	dhcp_rx_release = 0;
	dhcp_rx_inform = 0;
	dhcp_rx_offer = 0;
	dhcp_rx_ack = 0;
	dhcp_rx_nack = 0;
	dhcp_err = 0;

	printf("<div id=\"isle\">\n");
	printf("<table align=\"center\" width=\"98%%\"><tr><td style=\"vertical-align:top\">\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre><b>\n");
	printf("%-20.20s %-20.20s Status</b>\n", "Service", "Connections");
	
		
	// telnet
	{
		char con[30];
		sprintf(con, "%u", shm->stats.telnet_rx);
		printf("%-20.20s %-20.20s ", "Telnet", con);
		if (shm->config.services & RCP_SERVICE_TELNET)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}

	// http
	{
		char con[30];
		sprintf(con, "%u", shm->stats.http_rx);
		printf("%-20.20s %-20.20s ", "HTTP", con);
		if (shm->config.services & RCP_SERVICE_HTTP)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}

	// ftp
	{
		char con[30];
		sprintf(con, "%u", shm->stats.ftp_rx);
		printf("%-20.20s %-20.20s ", "FTP", con);
		if (shm->config.services & RCP_SERVICE_FTP)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}

	// tftp
	{
		char con[30];
		sprintf(con, "%u", shm->stats.tftp_rx);
		printf("%-20.20s %-20.20s ", "TFTP", con);
		if (shm->config.services & RCP_SERVICE_TFTP)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}

	// ssh
	struct stat s;
	if (stat("/opt/rcp/bin/plugins/librcpsec.so", &s) == 0) {
		char con[30];
		sprintf(con, "%u", shm->stats.sec_rx);
		printf("%-20.20s %-20.20s ", "SSH", con);
		if (shm->config.sec_services & RCP_SERVICE_SSH)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");

		printf("%-20.20s %-20.20s ", "SFTP", "");
		if (shm->config.sec_services & RCP_SERVICE_SFTP)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");

		printf("%-20.20s %-20.20s ", "SCP", "");
		if (shm->config.sec_services & RCP_SERVICE_SCP)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}

	// dns proxy
	{
		char con[30];
		sprintf(con, "%u", shm->stats.dns_queries);
		char buf[200];
		sprintf(buf, "%-20.20s %-20.20s ", "DNS_Proxy", con);
		char *ptr = buf;
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		printf("<a href=\"services.html;%u\">DNS Proxy</a>%s", session, ptr);
		if (shm->config.dns_server)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}


	// only if dhcp is configured
	if (shm->config.dhcps_enable)
		dhcp_configured = 1;

	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;
		if (intf->dhcp_relay)
			dhcp_configured = 1;
		
		// accumulate counts
		dhcp_rx += intf->stats.dhcp_rx;
		dhcp_rx_discover += intf->stats.dhcp_rx_discover;
		dhcp_rx_request += intf->stats.dhcp_rx_request;
		dhcp_rx_decline += intf->stats.dhcp_rx_decline;
		dhcp_rx_release += intf->stats.dhcp_rx_release;
		dhcp_rx_inform += intf->stats.dhcp_rx_inform;
		dhcp_rx_offer += intf->stats.dhcp_rx_offer;
		dhcp_rx_ack += intf->stats.dhcp_rx_ack;
		dhcp_rx_nack += intf->stats.dhcp_rx_nack;
		dhcp_err += intf->stats.dhcp_err_rx + intf->stats.dhcp_err_hops + intf->stats.dhcp_err_pkt +
			intf->stats.dhcp_err_not_configured + intf->stats.dhcp_err_server_notconf +
			intf->stats.dhcp_err_outgoing_intf + intf->stats.dhcp_err_giaddr;
	}

	// dhcp
	{
		char con[30];
		sprintf(con, "%u", dhcp_rx);
		char buf[200];
		sprintf(buf, "%-20.20s %-20.20s ", "DHCP", con);
		char *ptr = buf;
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		printf("<a href=\"services-dhcp.html;%u\">DHCP</a>%s", session, ptr);
		if (dhcp_configured)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}
	
	RcpNtpServer *ptr1;
	for (i = 0, ptr1 = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr1++) {
		if (!ptr1->valid)
			continue;
		ntp_configured = 1;
	}
	if (shm->config.ntp_server_enabled)
		ntp_configured = 1;
		
	// ntp
	{
		char con[30];
		sprintf(con, "%u", shm->stats.ntp_rx);
		char buf[200];
		sprintf(buf, "%-20.20s %-20.20s ", "NTP", con);
		char *ptr = buf;
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		printf("<a href=\"services-ntp.html;%u\">NTP</a>%s", session, ptr);
		if (ntp_configured)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}

	// only if snmp configured
	memset(&rstats, 0, sizeof(rstats));
	if (rcpSnmpAgentConfigured()) {
		snmp_configured = 1;
		// read snmp statistics
		int fd = open("snmpstats", O_RDONLY);
		if (fd != -1) {
			int rv = read(fd, &rstats, sizeof(rstats));
			(void) rv;
			close(fd);
		}
	}

	// snmp
	{
		char con[30];
		sprintf(con, "%u", rstats.inpkts);
		char buf[200];
		sprintf(buf, "%-20.20s %-20.20s ", "SNMP", con);
		char *ptr = buf;
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		printf("<a href=\"services-snmp.html;%u\">SNMP</a>%s", session, ptr);
		if (snmp_configured)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}
	
	printf("</pre></blockquote>\n");
	printf("</td></tr></table></div>\n");
//	printf("<br/><br/>\n");
}


void html_services(void) {
	services_header();

	// only if dns server is configured
	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	printf("DNS Proxy</p>\n");
	
	
	struct stat s;
	if (stat("/opt/rcp/var/log/dnsproxy_at_startup", &s) == 0) {		
		printf("<blockquote><pre>\n");
		printf("An external DNS proxy is already running on the system\nRCP DNS proxy has been disabled\n");
		printf("</pre></blockquote>\n");
	}	
	else if (shm->config.dns_server == 0) {
		printf("<blockquote><pre>\n");
		printf("DNS Proxy is not enabled\n");
		printf("</pre></blockquote>\n");
	}
	else {
		printf("<blockquote><pre>\n");
		printf("DNS queries: %u packets\n", shm->stats.dns_queries);
		printf("DNS answers: %u packets\n", shm->stats.dns_answers);
		printf("DNS cached answers: %u packets\n", shm->stats.dns_cached_answers);
		printf("RR types:\n");
		printf("\tA: %u packets\n", shm->stats.dns_a_answers);
		printf("\tCNAME: %u packets\n", shm->stats.dns_cname_answers);
		printf("\tPTR: %u packets\n", shm->stats.dns_ptr_answers);
		printf("\tMX: %u packets\n", shm->stats.dns_mx_answers);
		printf("\tAAAA (IPv6): %u packets\n", shm->stats.dns_aaaa_answers);
		
		if (shm->stats.dns_err_rate_limit ||
		     shm->stats.dns_err_rx ||
		     shm->stats.dns_err_tx ||
		     shm->stats.dns_err_malformed ||
		     shm->stats.dns_err_unknown ||
		     shm->stats.dns_err_no_server ||
		     shm->stats.dns_err_rq_timeout) {
			printf("Errors:\n");
			if (shm->stats.dns_err_rate_limit)
				printf("\tRate limit exceeded: %u packets\n", shm->stats.dns_err_rate_limit);
			if (shm->stats.dns_err_rx)
				printf("\tReceive error: %u packets\n", shm->stats.dns_err_rx);
			if (shm->stats.dns_err_tx)
				printf("\tTransmit error: %u packets\n", shm->stats.dns_err_tx);
			if (shm->stats.dns_err_malformed)
				printf("\tMalformed: %u packets\n", shm->stats.dns_err_malformed);
			if (shm->stats.dns_err_unknown)
				printf("\tUnknown partner: %u packets\n", shm->stats.dns_err_unknown);
			if (shm->stats.dns_err_no_server)
				printf("\tNo server configured: %u packets\n", shm->stats.dns_err_no_server);
			if (shm->stats.dns_err_rq_timeout)
				printf("\tRequest timeout: %u packets\n", shm->stats.dns_err_rq_timeout);
		}
		printf("</pre></blockquote>\n");
		svg("DNS QUERIES per 15 minute interval ", w, h, xmargin, ymargin, shm->stats.dns_q,  RCP_MAX_STATS);
	}
//	printf("<br/><br/>\n");
}	
	

static void print_dhcp(void) {
	int i;
	printf("<table align=\"center\" width=\"90%%\" style=\"background-color:#E3E9FA;\">"
		"<tr><td style=\"vertical-align:top\">\n");

	printf("<blockquote><pre>\n");
	printf("<b>%-18.18s</b><b>Packets</b>\n", "Interface");
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;
		printf("%-18.18s", intf->name);
		if (dhcp_rx != 0) {
			unsigned b = ((float) intf->stats.dhcp_rx / (float) dhcp_rx) * (float) 100;
			svg_hbar(b, 10);
			printf(" %u\n", intf->stats.dhcp_rx);
		}
		else
			printf("0\n");

	}
	printf("</pre></blockquote>\n");

	printf("</td><td style=\"vertical-align:top\">\n");
	printf("<blockquote><pre>\n");
	printf("<b>%-18.18s</b><b><a href=\"services-leases.html;%u\">Leases</a></b>\n", "Servers",  html_session());
	unsigned leases = 0;
	for (i = 0; i < RCP_DHCP_SERVER_GROUPS_LIMIT; i++) {
		RcpDhcprServer *srv = &shm->config.dhcpr_server[i];
		if (srv->valid == 0)
			continue;
			
		int n2;
		for (n2 = 0; n2 < RCP_DHCP_SERVER_LIMIT; n2++) {
			if (srv->server_ip[n2] == 0)
				continue;
			leases += srv->leases[n2];
		}
	}
	// count the server leases
	if (shm->config.dhcps_enable) {
		// run show ip dhcp leases to update the lease count
		
		// store the command in a temporary file and pass it to cli
		char fname1[50] = "/opt/rcp/var/transport/hXXXXXX";
		int fd = mkstemp(fname1);
		if (fd != -1) {
			close(fd);
			FILE *fp = fopen(fname1, "w+");
			if (fp) {
				fprintf(fp, "show ip dhcp leases\n");
				fclose(fp);
				
				char fname2[50] = "/opt/rcp/var/transport/hXXXXXX";
				int fd = mkstemp(fname2);
				if (fd != -1) {
					close(fd);
					char cmd[200];
					sprintf(cmd, "/opt/rcp/bin/cli --html --script %s > %s", fname1, fname2);
					int v = system(cmd);
					(void) v;
					unlink(fname2);
				}
			}
			unlink(fname1);
		}
		leases += shm->config.dhcps_leases;
	}
	
	// print the leases
	for (i = 0; i < RCP_DHCP_SERVER_GROUPS_LIMIT && leases != 0; i++) {
		RcpDhcprServer *srv = &shm->config.dhcpr_server[i];
		if (srv->valid == 0)
			continue;
			
		int n2;
		for (n2 = 0; n2 < RCP_DHCP_SERVER_LIMIT; n2++) {
			if (srv->server_ip[n2] == 0)
				continue;
			char addr[20];
			sprintf(addr, "%d.%d.%d.%d", RCP_PRINT_IP(srv->server_ip[n2]));
			printf("%-18.18s", addr);
			unsigned b = ((float) srv->leases[n2] / (float) leases) * (float) 100;
			svg_hbar(b, 10);
			printf(" %u\n", srv->leases[n2]);
		}
	}
	
	if (shm->config.dhcps_enable) {
		printf("%-18.18s", "local");
		unsigned b = ((float) shm->config.dhcps_leases / (float) leases) * (float) 100;
		svg_hbar(b, 10);
		printf(" %u\n", shm->config.dhcps_leases);
	}
		
	printf("</pre></blockquote>\n");
	printf("</td></tr></table>\n");
}



void html_services_dhcp(void) {
	services_header();

	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	printf("DHCP Server and Relay</p>\n");

	struct stat s;
	if (stat("/opt/rcp/var/log/dhcp_at_startup", &s) == 0) {		
		printf("<blockquote><pre>\n");
		printf("Warning: an external DHCP server or relay is already running on the system.\n");
		printf("It might interfere with RCP DHCP functionality\n");
		printf("</pre></blockquote>\n");
	}	

	if (dhcp_configured == 0) {
		printf("<blockquote><pre>\n");
		printf("DHCP is not enabled\n");
		printf("</pre></blockquote>\n");
	}
	else {
		print_dhcp();
		printf("<blockquote><pre>\n");
		printf("%d DHCP packets received, %d packet errors\n\n", dhcp_rx, dhcp_err);
		printf("     DHCPDISCOVER: %u\n", dhcp_rx_discover);
		printf("     DHCPREQUEST: %u\n", dhcp_rx_request);
		printf("     DHCPDECLINE: %u\n", dhcp_rx_decline);
		printf("     DHCPRELEASE: %u\n", dhcp_rx_release);
		printf("     DHCPINFORM: %u\n", dhcp_rx_inform);
		printf("     DHCPOFFER: %u\n", dhcp_rx_offer);
		printf("     DHCPACK: %u\n", dhcp_rx_ack);
		printf("     DHCPNACK: %u\n\n", dhcp_rx_nack);
		printf("</pre></blockquote>\n");
		svg("DHCP RX packets per 15 minute interval ", w, h, xmargin, ymargin, shm->stats.dhcp_rx,  RCP_MAX_STATS);
	}
//	printf("<br/><br/>\n");
}


void html_services_snmp(void) {
	services_header();

	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	printf("SNMP</p>\n");
	if (snmp_configured == 0) {
		printf("<blockquote><pre>\n");
		printf("SNMP agent is not enabled\n");
		printf("</pre></blockquote>\n");
	}
	else {

		printf("<blockquote><pre>\n");
		printf("%u input packets\n", rstats.inpkts);
		printf("\tGet-Request PDU: %u\n", rstats.ingetrq);
		printf("\tGet-Next PDU: %u\n", rstats.innextrq);
		printf("\tSet-Request PDU: %u\n", rstats.insetrq);
	
		if (rstats.inerrbadver || rstats.inerrbadcomname || rstats.inerrbadcomuses || rstats.inerrasn) {
			printf("\nInput Errors:\n");
			printf("\tBad version: %u\n", rstats.inerrbadver);
			printf("\tBad community name: %u\n", rstats.inerrbadcomname);
			printf("\tBad community uses: %u\n", rstats.inerrbadcomuses);
			printf("\tASN.1 parse errors: %u\n", rstats.inerrasn);
		}
		
		printf("\n%u output packets\n", rstats.outpkts);
		printf("\tGet-Response PDU: %u\n", rstats.outgetresp);
		printf("\tTraps: %u\n", shm->stats.snmp_traps_sent);
		
		if (rstats.outerrtoobig || rstats.outerrnosuchname || rstats.outerrbadvalue || rstats.outerrgenerr) {
			printf("\t'tooBig' errors: %u\n", rstats.outerrtoobig);
			printf("\t'noSuchName' errors: %u\n", rstats.outerrnosuchname);
			printf("\t'badValue' errors: %u\n", rstats.outerrbadvalue);
			printf("\t'getErr' errors: %u\n", rstats.outerrgenerr);
		}
		printf("</pre></blockquote>\n");
	}
//	printf("<br/><br/>\n");
}

	
void html_services_ntp(void) {
	services_header();

	// only if ntp is configured
	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	printf("NTP</p>\n");
	if (ntp_configured == 0) {
		printf("<blockquote><pre>\n");
		printf("NTP is not enabled\n");
		printf("</pre></blockquote>\n");
	}
	else {

		printf("<blockquote><pre>\n");
		printf("\nNTP Associations:\n\n");
		printf("<b>%-25.25s %-11.11s %-7.7s  %-10.10s  %-10.10s  %-8.8s</b>\n",
			"Peer", "Status", "Stratum", "Offset", "Delay", "Interval");
		
		RcpNtpServer *ptr = NULL;
		int i;
		for (i = 0, ptr = &shm->config.ntp_server[0]; i < RCP_NTP_LIMIT; i++, ptr++) {
			if (ptr->valid == 0)
				continue;
			
			// ip or hostname
			char server[26];
			if (ptr->ip != 0)
				sprintf(server, "%d.%d.%d.%d", RCP_PRINT_IP(ptr->ip));
			else
				sprintf(server, "%-25.25s", ptr->name);
			
			// server status
			char status[11];
			if (ptr->trustlevel < 2)
				strcpy(status, "inactive");
			else if (ptr->trustlevel < 5)
				strcpy(status, "pathetic");
			else if (ptr->trustlevel < 8)
				strcpy(status, "agresive");
			else
				strcpy(status, "active");
	
			// stratum
			char stratum[20];
			sprintf(stratum, "%d", ptr->stratum);
			if (ptr->trustlevel < 2)
				*stratum = '\0';
			
			// offset
			char offset[20];
			sprintf(offset, "%fs", ptr->offset);
			if (ptr->trustlevel < 2)
				*offset = '\0';		
			// delay
			char delay[20];
			sprintf(delay, "%fs", ptr->delay);
			if (ptr->trustlevel < 2)
				*delay = '\0';
						
			// interval
			char interval[20];
			if (ptr->interval == 0)
				*interval = '\0';
			else
				sprintf(interval, "%ds", (int) ptr->interval);
	
			printf("%-25.25s %-11.11s %-7.7s  %-10.10s  %-10.10s  %-8.8s\n",
				server, status, stratum, offset, delay, interval);
		}
		printf("\n");
		
		if (shm->config.ntp_synced)
			printf("Clock is synchronized, stratum %d\n",
				shm->config.ntp_stratum);
		else
			printf("Clock is not synchronized\n");
		
		if (shm->config.ntp_refid != 0 && shm->config.ntp_reftime != 0)
			printf("Reference is %d.%d.%d.%d, reference time is %f\n", 
				RCP_PRINT_IP(shm->config.ntp_refid), shm->config.ntp_reftime);
		
		if (shm->config.ntp_offset != 0 && shm->config.ntp_rootdelay != 0)
			printf("Clock offset is %fs, root delay is %fs\n",
				shm->config.ntp_offset, shm->config.ntp_rootdelay);
	
		if (shm->config.ntp_rootdispersion != 0)
			printf("Root dispersion is %f\n", shm->config.ntp_rootdispersion);
		
		if (shm->config.ntp_server_enabled)
			printf("\nLocal NTP server is enabled\n");
		printf("</pre></blockquote>\n");
	}
//	printf("<br/><br/>\n");
}

void html_send_auth(void) {
	printf("<!DOCTYPE html><html><head><title>Authentication</title>"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		"<link rel=\"icon\" type=\"image/png\" href=\"favicon.png\"/>"
		"<style>a:link {color:blue}</style>\n"
		"</head><body OnLoad=\"document.login.username.focus();\">\n");
	printf("<div id=\"page\" style=\"width:700px;background-color:white\"><br/>\n");

	printf("<div style=\"border:1px solid;border-radius:10px;background-color:#f0f0ff;margin:20px;padding-top:5px\">\n");
	printf("<form name=\"login\" method=\"post\" action=\"/login\">\n");
	printf("<br/>\n");
	printf("<table border=\"0\" cellpadding=\"3\" cellspacing=\"2\" align=\"center\"><tr>\n");
	printf("<td colspan=\"4\" align=\"center\" ><br/><br/><h2>%s login</h2><br/><br/></td>\n", shm->config.hostname);
	printf("</tr><tr>\n");

	printf("<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n");
	printf("<td>Username&nbsp;</td>\n");
	printf("<td><input name=\"username\" type=\"text\" id=\"username\" maxlength=\"31\"/></td>\n");
	printf("<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n");
	printf("</tr><tr>\n");

	printf("<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n");
	printf("<td>Password&nbsp;</td>\n");
	printf("<td><input name=\"password\" type=\"password\" id=\"password\" maxlength=\"128\"/></td>\n");
	printf("<td>&nbsp;&nbsp;&nbsp;&nbsp;</td>\n");
	printf("</tr><tr>\n");

	printf("<td colspan=\"4\">&nbsp;</td>\n");
	printf("</tr><tr>\n");

	printf("<td colspan=\"4\" align=\"center\">\n"); 
	printf("<input name=\"Login\" type=\"submit\" value=\"Login\"/>\n");
	printf("</td></tr>\n");
	
	printf("<tr><td colspan=\"4\">&nbsp;</td></tr>\n");
	printf("</table></form></div></div></body></html>\n");
}

static int netmon_configured;
static int netmon_header(void) {
	int i;
	netmon_configured = 0;;
	RcpNetmonHost *ptr;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		netmon_configured = 1;
		break;
	}

	if (netmon_configured == 0) {
		printf("<blockquote><pre >\n");
		printf("Network monitoring is not configured\n");
		printf("</pre></blockquote>\n");
		return 1;
	}

	printf("<div id=\"isle\">\n");
	printf("<table align=\"center\" width=\"98%%\"><tr><td style=\"vertical-align:top\">\n");
	printf("<blockquote style=\"margin-left:10px;\"><pre><b>\n");
	printf("%-24.24s %-15.15s %-10.10s %-15.15s Status</b>\n",
		"Host", "Type", "Uptime", "Response time");
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->pkt_sent == 0)
			continue;
			
		char buf[1000];	
		float uptime = 100 * ((float) ptr->pkt_received / (float) ptr->pkt_sent);
		if (uptime > 100)
			uptime = 100;
		char upstr[20];
		sprintf(upstr, "%.02f%%", uptime);
		char type[20];
		if (ptr->type == RCP_NETMON_TYPE_ICMP)
			strcpy(type, "ICMP");
		else if (ptr->type == RCP_NETMON_TYPE_TCP)
			sprintf(type, "TCP (%u)", ptr->port);
		else if (ptr->type == RCP_NETMON_TYPE_HTTP) {
			int port = (ptr->port)? ptr->port: 80;
			sprintf(type, "HTTP (%u)", port);
		}
		else if (ptr->type == RCP_NETMON_TYPE_SSH) {
			int port = (ptr->port)? ptr->port: 22;
			sprintf(type, "SSH (%u)", port);
		}
		else if (ptr->type == RCP_NETMON_TYPE_SMTP) {
			int port = (ptr->port)? ptr->port: 25;
			sprintf(type, "SMTP (%u)", port);
		}
		else if (ptr->type == RCP_NETMON_TYPE_DNS)
			strcpy(type, "DNS");
		else if (ptr->type == RCP_NETMON_TYPE_NTP)
			strcpy(type, "NTP");
		
		
		char resp[20];
		if (ptr->resptime_samples == 0)
			*resp = '\0';
		else {
			unsigned r = ptr->resptime_acc / ptr->resptime_samples;
			r /= 1000;
			sprintf(resp, "%ums", r);
		}
		
		sprintf(buf, "%-24.24s %-15.15s %-10.10s %-15.15s ",
			ptr->hostname,
			type,
			upstr,
			resp);
		
		char *p = buf;
		while (*p != '\0' &&  *p != ' ')
			p++;
		printf("<a href=\"netmon-%d.html;%u\">%s</a>%s",
			i, session, ptr->hostname, p);

		if (ptr->status)
			svg_checkmark(15);
		else
			svg_cross(15);
		printf("\n");
	}
	printf("</pre></blockquote>\n");
	printf("</td></tr></table></div>\n");
//	printf("<br/><br/>\n");
	
	return 0;
}

void print_monitor(int index) {
	if (index >= RCP_NETMON_LIMIT) {
		printf("</pre></blockquote>\n");
		printf("No monitoring data available\n");
		printf("</pre></blockquote>\n");
		return;
	}

	RcpNetmonHost *ptr = &shm->config.netmon_host[index];
	if (!ptr->valid || ptr->pkt_sent == 0) {
		printf("</pre></blockquote>\n");
		printf("No monitoring data available\n");
		printf("</pre></blockquote>\n");
		return;
	}

	uint32_t ip = 0;
	int print_ip = 1;
	atoip(ptr->hostname, &ip);
	if (ptr->ip_resolved == ip)
		print_ip = 0;

	printf("<p style=\"color:blue;margin-left:20px;\">\n");
	if (ptr->type == RCP_NETMON_TYPE_ICMP) {
		if (print_ip)
			printf("Host %s (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else
			printf("Host %s, status %s</p>\n",
				ptr->hostname,
				(ptr->status)? "UP": "DOWN");
	}
	
	else if (ptr->type == RCP_NETMON_TYPE_TCP) {
		if (print_ip)
			printf("TCP service %s:%u (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				ptr->port,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else
			printf("TCP service %s:%u, status %s</p>\n",
				ptr->hostname,
				ptr->port,
				(ptr->status)? "UP": "DOWN");
	}
	
	else if (ptr->type == RCP_NETMON_TYPE_HTTP) {
		int port = (ptr->port)? ptr->port: 80;
		if (print_ip)
			printf("HTTP server %s:%u (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				port,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else
			printf("HTTP server %s:%u, status %s</p>\n",
				ptr->hostname,
				port,
				(ptr->status)? "UP": "DOWN");
	}
	
	else if (ptr->type == RCP_NETMON_TYPE_SSH) {
		int port = (ptr->port)? ptr->port: 22;
		if (print_ip)
			printf("SSH server %s:%u (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				port,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else
			printf("SSH server %s:%u, status %s</p>\n",
				ptr->hostname,
				port,
				(ptr->status)? "UP": "DOWN");
	}
	
	else if (ptr->type == RCP_NETMON_TYPE_SMTP) {
		int port = (ptr->port)? ptr->port: 25;
		if (print_ip)
			printf("SMTP server %s:%u (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				port,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else
			printf("SMTP server %s:%u, status %s</p>\n",
				ptr->hostname,
				port,
				(ptr->status)? "UP": "DOWN");
	}
	
	else if (ptr->type == RCP_NETMON_TYPE_DNS) {
		if (print_ip)
			printf("DNS server %s (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else
			printf("DNS server %s, status %s</p>\n",
				ptr->hostname,
				(ptr->status)? "UP": "DOWN");
	}
	
	else if (ptr->type == RCP_NETMON_TYPE_DNS) {
		if (print_ip)
			printf("NTP server %s (%d.%d.%d.%d), status %s</p>\n",
				ptr->hostname,
				RCP_PRINT_IP(ptr->ip_resolved), 
				(ptr->status)? "UP": "DOWN");
		else 
			printf("NTP server %s, status %s</p>\n",
				ptr->hostname,
				(ptr->status)? "UP": "DOWN");
	}
	
	else
		ASSERT(0);

	unsigned u[RCP_MAX_STATS];
	int j;
	for (j = 0; j < RCP_MAX_STATS; j++) {
		if (ptr->tx[j] == 0)
			u[j] = 0;
		else {
			u[j] = 100 * ((float) ptr->rx[j] / (float) ptr->tx[j]);
			if (u[j] > 100)
				u[j] = 100;
		}
	}
	svg("Average Availability (%)", w, h / 2 , xmargin, ymargin, u,  RCP_MAX_STATS);
	printf("<br/><br/>\n");

	for (j = 0; j < RCP_MAX_STATS; j++)
		u[j] = ptr->resptime[j] / 1000;
	svg("Response time (ms)", w, h, xmargin, ymargin, u,  RCP_MAX_STATS);
//	printf("<br/><br/>\n");


}

void html_netmon(int index) {
	if (netmon_header())
		return;

	if (index >= 0)
		print_monitor(index);
	else {
		printf("<p style=\"color:blue;margin-left:20px;\">");
		printf("Network Monitoring Log</p>\n");
		printf("<blockquote><pre>\n");
	
		// print last 10 log entries
		RcpLogEntry *ptr = shm->log_head;
		char *month[] = {
			"Jan",
			"Feb",
			"Mar",
			"Apr",
			"May",
			"Jun",
			"Jul",
			"Aug",
			"Sep",
			"Oct",
			"Nov",
			"Dec"
		};
		
		int i = 0;
		while (ptr != NULL && i < 10) {
			ASSERT(ptr->ts != 0);
			if (ptr->ts != 0) {
				if (ptr->facility == RLOG_FC_NETMON) {
					struct tm *tm_log = localtime(&ptr->ts);
					const char *level = rcpLogLevel(ptr->level);
					const char *fc = rcpLogFacility(ptr->facility);
					const char *proc = rcpProcName(ptr->proc);
					
					int len = strlen((char *) ptr->data);
					if (len != 0) {
						printf("%s %d %d:%02d:%02d %s-%s (%s):\n    %s\n",
							month[tm_log->tm_mon], 
							tm_log->tm_mday,
							tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec,
							proc, fc, level, ptr->data);					
					}
					i++;
				}
			}
	
			ptr = ptr->next;
		}
		printf("<a href=\"netmon-log.html;%u\">more...</a>\n", html_session());
		printf("</pre></blockquote>\n");
//		printf("<br/><br/>\n");
	}
}

void html_netmon_log(void) {
	if (netmon_header())
		return;

	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("Network Monitoring Log</p>\n");
	
	fflush(0);
	printf("<blockquote><pre>\n");
	fflush(0);
	int rv = system("/opt/rcp/bin/cli --html --command \"show logging buffer monitor | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

void html_configuration(void) {
	printf("<div id=\"isle\">\n");
	printf("<p>&nbsp;<b>Toolbox</b></p>\n");
	printf("<table><tr><td>\n");
	printf("</td><td>\n");
	printf("<ul>\n");
	printf("<li>Display <a href=\"cfgrunning.html;%u\" target=\"_blank\">running</a> configuration</li>\n", html_session());
	printf("<li>Display <a href=\"cfgstartup.html;%u\" target=\"_blank\">startup</a> configuration</li>\n", html_session());
	printf("<li>Run arbitrary <a href=\"cfgcommands.html;%u\" target=\"_blank\">CLI commands</a></li>\n", html_session());
	printf("</ul>\n");
	printf("</td></tr></table>\n");
	printf("</div>\n");

	printf("<table border=\"0\" cellpadding=\"3\" cellspacing=\"2\" width=\"100%%\"><tr><td width=\"50%%\">\n");
	
	// system
	printf("<p style=\"margin-left:20px;\">System</p>\n");
	printf("<ul style=\"margin-left:20px;\">\n");
	printf("<li><a href=\"cfgdns.rcps;%u\" target=\"_blank\">DNS Configuration</a></li>\n", html_session());
	printf("<li><a href=\"cfglogger.rcps;%u\" target=\"_blank\">System Logger</a></li>\n", html_session());
	printf("</ul>\n");
		

	// router
	printf("<p style=\"margin-left:20px;\">Router</p>\n");
	printf("<ul style=\"margin-left:20px;\">\n");
	printf("<li><a href=\"cfginterfaces.rcps;%u\" target=\"_blank\">Interfaces</a></li>\n", html_session());
	printf("<li><a href=\"cfgroutes.html;%u\" target=\"_blank\">Static Routes</a></li>\n", html_session());
	printf("<li><a href=\"cfgarps.html;%u\" target=\"_blank\">Static ARP</a></li>\n", html_session());
	printf("<li><a href=\"cfgrip.rcps;%u\" target=\"_blank\">RIP</a></li>\n", html_session());
	printf("<li><a href=\"cfgospf.rcps;%u\" target=\"_blank\">OSPF</a></li>\n", html_session());
	printf("</ul>\n");

	printf("</td><td width=\"50%%\"  style=\"vertical-align:top\">\n");
	
	// services
	printf("<p style=\"margin-left:20px;\">Services</p>\n");
	printf("<ul style=\"margin-left:20px;\">\n");
	printf("<li><a href=\"cfgservices.rcps;%u\" target=\"_blank\">Telnet, FTP, TFTP</a></li>\n", html_session());
	printf("<li><a href=\"cfgntp.rcps;%u\" target=\"_blank\">NTP</a></li>\n", html_session());
	printf("<li><a href=\"cfgdhcpr.rcps;%u\" target=\"_blank\">DHCP Relay</a></li>\n", html_session());
	printf("<li><a href=\"cfgdhcps.rcps;%u\" target=\"_blank\">DHCP Server</a></li>\n", html_session());
	printf("<li><a href=\"cfgsnmp.rcps;%u\" target=\"_blank\">SNMP Agent</a></li>\n", html_session());
	printf("<li><a href=\"cfgsnmpn.rcps;%u\" target=\"_blank\">SNMP Notifications</a></li>\n", html_session());
	printf("<li><a href=\"cfgnetmon.html;%u\" target=\"_blank\">Network Monitoring</a></li>\n", html_session());
	printf("</ul>\n");

	printf("</td></tr></table>\n");
}

void html_services_leases(void) {
	services_header();
	printf("<p style=\"color:blue;margin-left:20px;\">");
	printf("DHCP Leases</p>\n");
	printf("<blockquote style=\"margin-left:20px;\"><pre>\n");
	fflush(0);
	int rv= system("/opt/rcp/bin/cli --html --command \"show ip dhcp leases | no\"");
	(void) rv;
	fflush(0);
	printf("</pre></blockquote>\n");
}

