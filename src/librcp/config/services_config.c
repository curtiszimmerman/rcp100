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
#include "../../cli/exec/cli.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void configLogger(FILE *fp, int no_passwords) {
	// print logger
	int printed = 0;
	if (shm->config.log_level != RLOG_LEVEL_DEFAULT) {
		printed = 1;
		fprintf(fp, "logging %d\n", shm->config.log_level);
	}
	if (shm->config.facility != RLOG_FC_DEFAULT) {
		printed = 1;
		if (shm->config.facility & RLOG_FC_CONFIG)
			fprintf(fp, "logging debugging config\n");
		if (shm->config.facility & RLOG_FC_INTERFACE)
			fprintf(fp, "logging debugging interface\n");
		if (shm->config.facility & RLOG_FC_ROUTER)
			fprintf(fp, "logging debugging router\n");
		if (shm->config.facility & RLOG_FC_ADMIN)
			fprintf(fp, "logging debugging admin\n");
		if (shm->config.facility & RLOG_FC_SYSCFG)
			fprintf(fp, "logging debugging syscfg\n");
		if (shm->config.facility & RLOG_FC_IPC)
			fprintf(fp, "logging debugging ipc\n");
		if (shm->config.facility & RLOG_FC_DHCP)
			fprintf(fp, "logging debugging dhcp\n");
		if (shm->config.facility & RLOG_FC_DNS)
			fprintf(fp, "logging debugging dns\n");
		if (shm->config.facility & RLOG_FC_RIP)
			fprintf(fp, "logging debugging rip\n");
		if (shm->config.facility & RLOG_FC_NTPD)
			fprintf(fp, "logging debugging ntp\n");
		if (shm->config.facility & RLOG_FC_NETMON)
			fprintf(fp, "logging debugging monitor\n");
		if (shm->config.facility & RLOG_FC_HTTP)
			fprintf(fp, "logging debugging http\n");
		if (shm->config.facility & RLOG_FC_OSPF_PKT)
			fprintf(fp, "logging debugging ospf packets\n");
		if (shm->config.facility & RLOG_FC_OSPF_DRBDR)
			fprintf(fp, "logging debugging ospf drbdr\n");
		if (shm->config.facility & RLOG_FC_OSPF_LSA)
			fprintf(fp, "logging debugging ospf lsa\n");
		if (shm->config.facility & RLOG_FC_OSPF_SPF)
			fprintf(fp, "logging debugging ospf spf\n");
	}
	if (shm->config.snmp_facility != 0) {
		printed = 1;
		if (shm->config.snmp_facility & RLOG_FC_CONFIG)
			fprintf(fp, "logging snmp config\n");
		if (shm->config.snmp_facility & RLOG_FC_INTERFACE)
			fprintf(fp, "logging snmp interface\n");
		if (shm->config.snmp_facility & RLOG_FC_ROUTER)
			fprintf(fp, "logging snmp router\n");
		if (shm->config.snmp_facility & RLOG_FC_ADMIN)
			fprintf(fp, "logging snmp admin\n");
		if (shm->config.snmp_facility & RLOG_FC_SYSCFG)
			fprintf(fp, "logging snmp syscfg\n");
		if (shm->config.snmp_facility & RLOG_FC_IPC)
			fprintf(fp, "logging snmp ipc\n");
		if (shm->config.snmp_facility & RLOG_FC_DHCP)
			fprintf(fp, "logging snmp dhcp\n");
		if (shm->config.snmp_facility & RLOG_FC_DNS)
			fprintf(fp, "logging snmp dns\n");
		if (shm->config.snmp_facility & RLOG_FC_RIP)
			fprintf(fp, "logging snmp rip\n");
		if (shm->config.snmp_facility & RLOG_FC_NTPD)
			fprintf(fp, "logging snmp ntp\n");
		if (shm->config.snmp_facility & RLOG_FC_NETMON)
			fprintf(fp, "logging snmp monitor\n");
		if (shm->config.snmp_facility & RLOG_FC_HTTP)
			fprintf(fp, "logging snmp http\n");
		if (shm->config.snmp_facility & RLOG_FC_OSPF_PKT)
			fprintf(fp, "logging snmp ospf packets\n");
		if (shm->config.snmp_facility & RLOG_FC_OSPF_DRBDR)
			fprintf(fp, "logging snmp ospf drbdr\n");
		if (shm->config.snmp_facility & RLOG_FC_OSPF_LSA)
			fprintf(fp, "logging snmp ospf lsa\n");
		if (shm->config.snmp_facility & RLOG_FC_OSPF_SPF)
			fprintf(fp, "logging snmp ospf spf\n");
	}
	
	if (shm->config.rate_limit != RLOG_RATE_DEFAULT) {
		printed = 1;
		fprintf(fp, "logging rate-limit %d\n", shm->config.rate_limit);
	}

	int i;
	RcpSyslogHost *ptr;
	for (i = 0, ptr = shm->config.syslog_host; i < RCP_SYSLOG_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		printed = 1;
		if (ptr->port == 0)
			fprintf(fp, "logging host %d.%d.%d.%d\n",
				RCP_PRINT_IP(ptr->ip));
		else
			fprintf(fp, "logging host %d.%d.%d.%d port %u\n",
				RCP_PRINT_IP(ptr->ip), ptr->port);
	}

	if (printed)
		fprintf(fp, "!\n");
}

void configAdministrators(FILE *fp, int no_passwords) {
	int i;
	RcpAdmin *ptr;
	int printed = 0;
	
	// print administrators
	for (i = 0, ptr = shm->config.admin; i < RCP_ADMIN_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		fprintf(fp, "administrator %s encrypted password %s\n",
			ptr->name, (no_passwords)? "<removed>": ptr->password);
		printed = 1;
	}

	if (printed)
		fprintf(fp, "!\n");
}

void configServices(FILE *fp, int no_passwords) {
	if (shm->config.timeout_minutes == CLI_DEFAULT_TIMEOUT && shm->config.timeout_seconds == 0)
		; // this is a default, do not print
	else
		fprintf(fp, "exec-timeout %u %u\n",
			shm->config.timeout_minutes,
			shm->config.timeout_seconds);
	if (shm->config.enable_passwd_configured)
		fprintf(fp, "enable encrypted password %s\n", (no_passwords)? "<removed>": shm->config.enable_passwd);

	int printed = 0;

	// tftp configuration
	if (shm->config.services & RCP_SERVICE_TFTP) {
		printed = 1;
		fprintf(fp, "service tftp\n");
	}

	// handle rcpsec configuration
	struct stat s;
	if (stat("/opt/rcp/bin/plugins/librcpsec.so", &s) == 0) {
		void *lib_handle;
		int (*fn)(RcpShm*, FILE*, int);
		lib_handle = dlopen("/opt/rcp/bin/plugins/librcpsec.so", RTLD_LAZY);
		if (lib_handle) {
			fn = dlsym(lib_handle, "rcpsec_cfg1");
			if (fn)
				printed |= (*fn)(shm, fp, no_passwords);
			else
				ASSERT(0);
			dlclose(lib_handle);
		}
		else
			ASSERT(0);
	}
	
	// telnet configuration
	if (shm->config.services & RCP_SERVICE_TELNET) {
		printed = 1;
		if (shm->config.telnet_port == RCP_DEFAULT_TELNET_PORT)
			fprintf(fp, "service telnet\n");
		else
			fprintf(fp, "service telnet port %u\n", shm->config.telnet_port);
	}

	// http configuration
	if (shm->config.services & RCP_SERVICE_HTTP) {
		printed = 1;
		if (shm->config.http_port == RCP_DEFAULT_HTTP_PORT && shm->config.http_passwd[0] == '\0')
			fprintf(fp, "service http\n");
		else if (shm->config.http_port != RCP_DEFAULT_HTTP_PORT && shm->config.http_passwd[0] == '\0')
			fprintf(fp, "service http port %u\n", shm->config.http_port);
		else if (shm->config.http_port == RCP_DEFAULT_HTTP_PORT && shm->config.http_passwd[0] != '\0')
			fprintf(fp, "service http encrypted password %s\n", shm->config.http_passwd);
		else if (shm->config.http_port != RCP_DEFAULT_HTTP_PORT && shm->config.http_passwd[0] != '\0')
			fprintf(fp, "service http port %u encrypted password %s\n",
				shm->config.http_port, shm->config.http_passwd);
	}


	// ftp configuration
	if (shm->config.services & RCP_SERVICE_FTP) {
		printed = 1;
		fprintf(fp, "service ftp\n");
	}

	if (printed)
		fprintf(fp, "!\n");
}
