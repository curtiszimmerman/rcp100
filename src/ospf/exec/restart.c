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
#include "ospf.h"


void restart(void) {
	int i;
	// delete all ospf routes
	int v = system("/opt/rcp/bin/rtclean ospf >> /opt/rcp/var/log/restart");
	if (v == -1)
		ASSERT(0);

	// create configured areas
	for (i = 0; i < RCP_OSPF_AREA_LIMIT; i++) {
		if (shm->config.ospf_area[i].valid) {
			OspfArea *a = createOspfArea(shm->config.ospf_area[i].area_id);
			if (!a) {
				rcpLog(muxsock, RCP_PROC_OSPF, RLOG_ERR, RLOG_FC_OSPF_PKT,
					"Cannot create area %u", shm->config.ospf_area[i].area_id);
				continue;
			}
		}			
	}
	
	// clear multicast membership - this will trigger the creation of ospf networks
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		intf->ospf_multicast_ip = 0;
		intf->ospf_multicast_mask = 0;
	}
	
	// if static route redistribution is enabled, we need to generate external lsas
	if (shm->config.ospf_redist_static)
		redistStaticUpdate();
		
	// trigger spf
	spfTrigger();
}
