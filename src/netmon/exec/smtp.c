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
#include "mgmt.h"
int smtp_enabled = 0;

void monitor_smtp(void) {
	RcpNetmonHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid || ptr->type != RCP_NETMON_TYPE_SMTP)
			continue;

		if (ptr->pkt_sent) {
			int port = (ptr->port)? ptr->port: 25;
			if (ptr->wait_response == 0) {
				if (ptr->status == 0) {
					ptr->status = 1;
					rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
						"SMTP server %s %u UP", ptr->hostname, port);
				}
			}
			else {
				if (ptr->status == 1) {
					ptr->status = 0;
					rcpLog(muxsock, RCP_PROC_NETMON, RLOG_NOTICE, RLOG_FC_NETMON,
						"SMTP server %s %u DOWN", ptr->hostname, port);
				}
				ptr->wait_response = 0;
				ptr->sock = 0;
			}
		}
	}
}

void smtp_flag_update(void) {
	RcpNetmonHost *ptr;
	int i;

	// update netmon status
	int newflag = 0;

	// clean hosts
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (ptr->type == RCP_NETMON_TYPE_SMTP) {
			if (ptr->valid) {
				newflag = 1;
				break;
			}
		}
	}
	
	// update run flag and start time
	if (newflag != smtp_enabled)
		smtp_enabled = newflag;
}
