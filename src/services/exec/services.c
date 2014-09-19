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

volatile int update_xinetd_files = 0;


//******************************************************************
// callbacks
//******************************************************************
int cliClearServiceStats(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	shm->stats.sec_rx = 0;
	shm->stats.telnet_rx = 0;
	shm->stats.tftp_rx = 0;
	shm->stats.ftp_rx = 0;
	shm->stats.ntp_rx = 0;
	shm->stats.http_rx = 0;
	
	return 0;
}

//**********************************************************
// TFTP
//**********************************************************


int cliServiceTftpCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// trigger a system update
	update_xinetd_files = 1;

	if (strcmp(argv[0], "no") == 0) {
		shm->config.services &= ~RCP_SERVICE_TFTP;
	}
	else {
		shm->config.services |= RCP_SERVICE_TFTP;
	}
	return 0;
}


