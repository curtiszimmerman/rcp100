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


void configNat(FILE *fp, int no_passwords) {
	int i;
	
	RcpMasq *ptr;
	for (i = 0, ptr = shm->config.ip_masq; i < RCP_MASQ_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
		fprintf(fp, "ip nat masquerade %d.%d.%d.%d/%d %s\n",
			RCP_PRINT_IP(ptr->ip), mask2bits(ptr->mask),
			ptr->out_interface);
	}
	
	RcpForwarding *ptr2;
	for (i = 0, ptr2 = shm->config.port_forwarding; i < RCP_FORWARD_LIMIT; i++, ptr2++) {
		if (!ptr2->valid)
			continue;
		fprintf(fp, "ip nat forwarding tcp %u destination %d.%d.%d.%d %u\n",
			ptr2->port, RCP_PRINT_IP(ptr2->dest_ip), ptr2->dest_port);
	}
	
	fprintf(fp, "!\n");
}
