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

void configNtp(FILE *fp, int no_passwords) {
		
	// print ntp servers
	int i;
	RcpNtpServer *ptr;
	
	for (i = 0, ptr = shm->config.ntp_server; i < RCP_NTP_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->ip != 0) {
			if (ptr->preferred)
				fprintf(fp, "ntp server %d.%d.%d.%d prefered\n", RCP_PRINT_IP(ptr->ip));
			else
				fprintf(fp, "ntp server %d.%d.%d.%d\n", RCP_PRINT_IP(ptr->ip));
		}
		else if (*ptr->name != '\0') {
			if (ptr->preferred)
				fprintf(fp, "ntp server %s prefered\n", ptr->name);
			else
				fprintf(fp, "ntp server %s\n", ptr->name);
		}
	}
	
	// ntp server
	if (shm->config.ntp_server_enabled)
		fprintf(fp, "ip ntp server\n");

	fprintf(fp, "!\n");
}
