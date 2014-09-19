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
#include "cli.h"

#define HOST_MAX_PRINT 10	// print only the first 10 characters of the host name

int cliPrintPrompt(void) {
	int len = strlen(shm->config.hostname);
	char buf[CLI_MAX_LINE];
	buf[0] = '\0';

	switch (csession.cmode) {
		case CLIMODE_LOGIN:
			sprintf(buf, "%.*s%s>", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;

		case CLIMODE_ENABLE:
			sprintf(buf, "%.*s%s#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;
			
		case CLIMODE_CONFIG:
			sprintf(buf, "%.*s%s(config)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;
			
		case CLIMODE_INTERFACE:
		case CLIMODE_INTERFACE_LOOPBACK:
		case CLIMODE_INTERFACE_BRIDGE:
		case CLIMODE_INTERFACE_VLAN:
			sprintf(buf, "%.*s%s(config-if %s)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "", csession.mode_data);
			fflush(0);
			break;
		case CLIMODE_RIP:
			sprintf(buf, "%.*s%s(rip)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;

		case CLIMODE_DHCP_RELAY:
			sprintf(buf, "%.*s%s(dhcp relay)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;

		case CLIMODE_DHCP_SERVER:
			sprintf(buf, "%.*s%s(dhcp server)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;

		case CLIMODE_DHCP_NETWORK:
			sprintf(buf, "%.*s%s(dhcp %s)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "", csession.mode_data);
			fflush(0);
			break;

		case CLIMODE_TUNNEL_IPSEC: {
			const char *name = ((RcpIpsecTun *) csession.mode_data)->name;
			sprintf(buf, "%.*s%s(tunnel %s)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "", name);
			fflush(0);
		}
		break;

		case CLIMODE_OSPF:
			sprintf(buf, "%.*s%s(ospf)#", 
				HOST_MAX_PRINT, shm->config.hostname, (len > HOST_MAX_PRINT)? "..": "");
			fflush(0);
			break;

		default:
			ASSERT(0);
	}
	printf("%s", buf);
	return strlen(buf);
}
