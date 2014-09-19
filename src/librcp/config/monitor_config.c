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

void configNetmon(FILE *fp, int no_passwords) {
	int printed = 0;
	RcpNetmonHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;

		printed = 1;
		if (ptr->type == RCP_NETMON_TYPE_ICMP)
			fprintf(fp, "monitor host %s\n", ptr->hostname);

		else if (ptr->type == RCP_NETMON_TYPE_TCP)
			fprintf(fp, "monitor tcp %s %u\n", ptr->hostname, ptr->port);

		else if (ptr->type == RCP_NETMON_TYPE_HTTP && ptr->port == 0)
			fprintf(fp, "monitor http %s\n", ptr->hostname);
		else if (ptr->type == RCP_NETMON_TYPE_HTTP && ptr->port)
			fprintf(fp, "monitor http %s %u\n", ptr->hostname, ptr->port);

		else if (ptr->type == RCP_NETMON_TYPE_SSH && ptr->port == 0)
			fprintf(fp, "monitor ssh %s\n", ptr->hostname);
		else if (ptr->type == RCP_NETMON_TYPE_SSH && ptr->port)
			fprintf(fp, "monitor ssh %s %u\n", ptr->hostname, ptr->port);

		else if (ptr->type == RCP_NETMON_TYPE_SMTP && ptr->port == 0)
			fprintf(fp, "monitor smtp %s\n", ptr->hostname);
		else if (ptr->type == RCP_NETMON_TYPE_SMTP && ptr->port)
			fprintf(fp, "monitor smtp %s %u\n", ptr->hostname, ptr->port);
		else if (ptr->type == RCP_NETMON_TYPE_NTP)
			fprintf(fp, "monitor ntp %s\n", ptr->hostname);
		else if (ptr->type == RCP_NETMON_TYPE_DNS)
			fprintf(fp, "monitor dns %s\n", ptr->hostname);

		else
			ASSERT(0);
	}
	
//	if (shm->config.monitor_interval != RCP_DEFAULT_MON_INTERVAL) {
//		printed = 1;
//		fprintf(fp, "monitor interval %d\n", shm->config.monitor_interval);
//	}

	if (printed)
		fprintf(fp, "!\n");
}
