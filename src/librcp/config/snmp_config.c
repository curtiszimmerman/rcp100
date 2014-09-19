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

void configSnmp(FILE *fp, int no_passwords) {
	// contact and location
	if (shm->config.snmp.contact[0] != '\0')
		fprintf(fp, "snmp-server contact %s\n", shm->config.snmp.contact);
	if (shm->config.snmp.location[0] != '\0')
		fprintf(fp, "snmp-server location %s\n", shm->config.snmp.location);
	
	// SNMP v1 and v2 
	ASSERT(!(shm->config.snmp.com_public && shm->config.snmp.com_passwd[0] != '\0'));
	if (shm->config.snmp.com_public)
		fprintf(fp, "snmp-server community public ro\n");
	else if (shm->config.snmp.com_passwd[0] != '\0')
		fprintf(fp, "snmp-server community %s ro\n", (no_passwords)? "<removed>": shm->config.snmp.com_passwd);

	// SNMP v3
	int i;
	for (i = 0; i < RCP_SNMP_USER_LIMIT; i++) {
		if (shm->config.snmp.user[i].name[0] != '\0')
			fprintf(fp, "snmp-server user %s password %s ro\n",
				shm->config.snmp.user[i].name,
				(no_passwords)? "<removed>": shm->config.snmp.user[i].passwd);
	}
	
	// traps
	if (shm->config.snmp.traps_enabled)
		fprintf(fp, "snmp-server enable traps\n");
	RcpSnmpHost *ptr;
	for (i = 0, ptr = shm->config.snmp.host; i <  RCP_SNMP_HOST_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		if (ptr->inform)
			fprintf(fp, "snmp-server host %d.%d.%d.%d informs version 2c %s\n",
				RCP_PRINT_IP(ptr->ip),
				(no_passwords)? "<removed>": ptr->com_passwd);
		else
			fprintf(fp, "snmp-server host %d.%d.%d.%d traps version 2c %s\n",
				RCP_PRINT_IP(ptr->ip),
				(no_passwords)? "<removed>": ptr->com_passwd);
	}
}
