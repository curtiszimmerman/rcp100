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

void configDns(FILE *fp, int no_passwords) {
	// hostname etc.
	fprintf(fp, "hostname %s\n", shm->config.hostname);
	
	// domain name
	if (*shm->config.domain_name != '\0')
		fprintf(fp, "ip domain-name %s\n", shm->config.domain_name);

	// name server
	if (shm->config.name_server1 != 0)
		fprintf(fp, "ip name-server %d.%d.%d.%d\n",
			RCP_PRINT_IP(shm->config.name_server1));
	if (shm->config.name_server2 != 0)
		fprintf(fp, "ip name-server %d.%d.%d.%d\n",
			RCP_PRINT_IP(shm->config.name_server2));

	// dns cache
	if (shm->config.dns_server && shm->config.dns_rate_limit)
		fprintf(fp, "ip dns server rate-limit %u\n", shm->config.dns_rate_limit);
	else if (shm->config.dns_server && shm->config.dns_rate_limit == 0)
		fprintf(fp, "ip dns server\n");
		
	// print ip hosts
	int i;
	RcpIpHost *ptr;
	
	for (i = 0, ptr = shm->config.ip_host; i < RCP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		fprintf(fp, "ip host %s %d.%d.%d.%d\n",
			ptr->name, RCP_PRINT_IP(ptr->ip));
	}
	

	fprintf(fp, "!\n");
}
