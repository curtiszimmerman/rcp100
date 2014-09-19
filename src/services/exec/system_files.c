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
#include "services.h"
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

static char *tftp_header = 
"service tftp\n"
"{\n"
"        socket_type = dgram\n"
"        protocol = udp\n"
"        wait = yes\n"
"        user = rcp\n"
"        server = /opt/rcp/sbin/tftpd\n"
"        server_args = /home/rcp/tftpboot\n";

static char *telnet_header = 
"service rcptelnet\n"
"{\n"
"        type = UNLISTED\n"
"	flags		= REUSE\n"
"	socket_type	= stream\n"
"	instances	= 32\n"
"	wait		= no\n"
"	user		= root\n"
"	server		= /opt/rcp/sbin/in.telnetd\n"
"	server_args     = -h -n -L /opt/rcp/bin/rcplogin\n"
"	log_on_failure	+= USERID\n";
//        port = 23
//        disable = yes
//}

static char *http_header = 
"service rcphttp\n"
"{\n"
"        type = UNLISTED\n"
"	flags		= REUSE\n"
"	socket_type	= stream\n"
"	instances	= 8\n"
"	wait		= no\n"
"	user		= rcp\n"
"	server		= /opt/rcp/bin/rcphttpd\n"
"	server_args     = /opt/rcp/var/transport\n"
"	log_on_failure	+= USERID\n";
//        port = 80
//        disable = yes
//}

static char *ftp_header = 
"service rcpftp\n"
"{\n"
"        type = UNLISTED\n"
"        socket_type     = stream\n"
"        protocol        = tcp\n"
"        instances       = 32\n"
"        wait            = no\n"
"        user            = root\n"
"        server          = /opt/rcp/sbin/pure-ftpd\n"
"        server_args     = -H -A -E -p 40000:50000 -c 32 -C 1 -u 500 -I 5\n"
"        groups          = yes\n"
"        flags           = REUSE\n"
"         port = 21\n";
//        disable = yes
//}

void setSystemDefaults(void) {
	FILE *fp;

	//******************
	// tftp file
	//******************
	fp = fopen("/opt/rcp/etc/xinetd.d/tftp", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/tftp file\n");
		exit(1);
	}
	fprintf(fp, "%s\n}\n", tftp_header);
	fclose(fp);

	//******************
	// telnet file
	//******************
	fp = fopen("/opt/rcp/etc/xinetd.d/rcptelnet", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/rcptelnet file\n");
		exit(1);
	}
	fprintf(fp, "%s\nport = 23\ndisable = yes\n}\n", telnet_header);
	fclose(fp);

	//******************
	// http file
	//******************
	fp = fopen("/opt/rcp/etc/xinetd.d/rcphttp", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/rcphttp file\n");
		exit(1);
	}
	fprintf(fp, "%s\nport = 80\ndisable = yes\n}\n", http_header);
	fclose(fp);

	//******************
	// ftp file
	//******************
	fp = fopen("/opt/rcp/etc/xinetd.d/rcpftp", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/rcpftp file\n");
		exit(1);
	}
	fprintf(fp, "%s\ndisable = yes\n}\n", ftp_header);
	fclose(fp);

	//******************
	// ntpd file
	//******************
	fp = fopen("/opt/rcp/etc/ntpd.conf", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create ntpd.conf\n");
		exit(1);
	}
	fprintf(fp, "\n");
	fclose(fp);
}
	
void updateXinetdFiles(void) {
	// reset update flag
	update_xinetd_files = 0;
	FILE *fp;

	//******************
	// tftp file - tftp is always enabled
	//******************
	logDistribute("write xinetd file /opt/rcp/etc/xinetd.d/tftp", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
	fp = fopen("/opt/rcp/etc/xinetd.d/tftp", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/tftp file\n");
		exit(1);
	}
	fprintf(fp, "%s\n\n", tftp_header);
	
	// close file
	fprintf(fp, "}\n");
	fclose(fp);


	//******************
	// ftp file
	//******************
	logDistribute("write xinetd file /opt/rcp/etc/xinetd.d/rcpftp", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
	fp = fopen("/opt/rcp/etc/xinetd.d/rcpftp", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/rcpftp file\n");
		exit(1);
	}
	
	if (shm->config.services & RCP_SERVICE_FTP) {
		fprintf(fp, "%s\ndisable = no\n", ftp_header);
		fprintf(fp, "}\n");
	}
	else
		fprintf(fp, "%s\ndisable = yes\n}\n", ftp_header);
	
	// close file
	fclose(fp);



	//******************
	// telnet file
	//******************
	logDistribute("write xinetd file /opt/rcp/etc/xinetd.d/rcptelnet", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
	fp = fopen("/opt/rcp/etc/xinetd.d/rcptelnet", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/rcptelnet file\n");
		exit(1);
	}
	fprintf(fp, "%s\n\n", telnet_header);
	
	// add telnet configured port
	fprintf(fp, "   port = %u\n", shm->config.telnet_port);

	if (shm->config.services & RCP_SERVICE_TELNET) {
		fprintf(fp, "disable = no\n");
		fprintf(fp, "}\n");
	}
	else
		fprintf(fp, "disable = yes\n}\n");
	
	fclose(fp);

	//******************
	// http file
	//******************
	logDistribute("write xinetd file /opt/rcp/etc/xinetd.d/rcphttp", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
	fp = fopen("/opt/rcp/etc/xinetd.d/rcphttp", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: cannot create xinet.d/rcphttp file\n");
		exit(1);
	}
	fprintf(fp, "%s\n\n", http_header);
	
	// add telnet configured port
	fprintf(fp, "   port = %u\n", shm->config.http_port);

	if (shm->config.services & RCP_SERVICE_HTTP) {
		fprintf(fp, "disable = no\n");
		fprintf(fp, "}\n");
	}
	else
		fprintf(fp, "disable = yes\n}\n");
	
	fclose(fp);

	// restart xinetd
	// send HUP signal to xinetd for reconfiguration
	logDistribute("reconfiguring xinetd", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
	int v = system("pkill -HUP xinetd");
	if (v == -1)
		ASSERT(0);
}


static char *hosts_header = 
"127.0.0.1		localhost.localdomain localhost\n"
"::1		localhost6.localdomain6 localhost6\n";

void updateHostsFile(void) {
	update_hosts_file = 0;
	
	// open file
	FILE *fp = fopen("/etc/hosts", "w"); // truncate or create new file
	if (fp == NULL) {
		logDistribute("cannot open /etc/hosts file", RLOG_ERR, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
		return;
	}
	
	// add localhost data
	fprintf(fp, "%s", hosts_header);

	// read hostname file
	FILE *hn = fopen("/etc/hostname", "r");
	if (hn) {
		char buf[500];
		if (fgets(buf, 500, hn)) {
			// remove \n
			char *ptr = strchr(buf, '\n');
			if (ptr) {
				*ptr = '\0';
			}
			fprintf(fp, "127.0.0.1   %s\n", buf);
		}
		fclose(hn);
	}
	
	// add rcp configuration
	int i;
	RcpIpHost *ptr;
	
	for (i = 0, ptr = shm->config.ip_host; i < RCP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		fprintf(fp, "%d.%d.%d.%d\t%s\n", RCP_PRINT_IP(ptr->ip), ptr->name);
	}

	// close file and log message
	fclose(fp);
	logDistribute("write /etc/hosts file", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
}

void updateResolvconfFile(void) {
	update_resolvconf_file = 0;
	
	// open file
	FILE *fp = fopen("/etc/resolv.conf", "w"); // truncate or create new file
	if (fp == NULL) {
		logDistribute("cannot open /etc/resolv.conf file", RLOG_ERR, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
		return;
	}

	// domain name
	if (*shm->config.domain_name != '\0')
		fprintf(fp, "domain %s\n", shm->config.domain_name);

	// name server
	// if dns cache enabled, put in the loopback address
	if (shm->config.dns_server)
		fprintf(fp, "nameserver 127.0.0.1\n");
	else {
		if (shm->config.name_server1 != 0)
			fprintf(fp, "nameserver %d.%d.%d.%d\n",
				RCP_PRINT_IP(shm->config.name_server1));
		if (shm->config.name_server2 != 0)
			fprintf(fp, "nameserver %d.%d.%d.%d\n",
				RCP_PRINT_IP(shm->config.name_server2));
	}

	// close file and log message
	fclose(fp);
	logDistribute("write /etc/resolv.conf file", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
}


void updateNtpdFile(void) {
	update_ntpd_conf = 0;
	
	// open file
	FILE *fp = fopen("/opt/rcp/etc/ntpd.conf", "w"); // truncate or create new file
	if (fp == NULL) {
		logDistribute("cannot open /opt/rcp/etc/ntpd.conf file", RLOG_ERR, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
		return;
	}

	// server running?
	if (shm->config.ntp_server_enabled)
		fprintf(fp, "listen on *\n");
	
	// external servers
	RcpNtpServer *ptr;
	int i;

	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		if (*ptr->name != '\0')
			fprintf(fp, "server %s\n", ptr->name);
		else if (ptr->ip != 0)
			fprintf(fp, "server %d.%d.%d.%d\n", RCP_PRINT_IP(ptr->ip));
	}

	// close file and log message
	fclose(fp);
	logDistribute("write /opt/rcp/etc/ntpd.conf file", RLOG_DEBUG, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);

// HUP signal not yet implemented in openntpd
//	system("pkill -HUP ntpd");
	logDistribute("NTP configuration changed, restarting ntpd",
		RLOG_NOTICE, RLOG_FC_SYSCFG, RCP_PROC_SERVICES);
	int v = system("pkill ntpd");
	if (v == -1)
		ASSERT(0);
}
