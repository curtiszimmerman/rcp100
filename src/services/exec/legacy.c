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

//******************************************************************
// Telnet
//******************************************************************

int cliServiceTelnet(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.services &= ~RCP_SERVICE_TELNET;		
		shm->config.telnet_port = RCP_DEFAULT_TELNET_PORT;
	}
	else
		shm->config.services |= RCP_SERVICE_TELNET;		
	
	// trigger a system update
	update_xinetd_files = 1;
	return 0;
}


int cliServiceTelnetPort(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';


	if (strcmp(argv[0], "no") == 0)
		shm->config.telnet_port = RCP_DEFAULT_TELNET_PORT;
	else {
		int p = atoi(argv[3]);
		if ((uint16_t) p == shm->config.sec_port) {
			strcpy(data, "Error: port already in use for SSH");
			return RCPERR;
		}
		else if ((uint16_t) p == shm->config.http_port) {
			strcpy(data, "Error: port already in use for HTTP");
			return RCPERR;
		}
		shm->config.telnet_port = (uint16_t) p;
		shm->config.services |= RCP_SERVICE_TELNET;		
	}
	
	// trigger a system update
	update_xinetd_files = 1;
	return 0;
}


//******************************************************************
// HTTP
//******************************************************************

int cliServiceHttp(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.services &= ~RCP_SERVICE_HTTP;		
		shm->config.http_port = RCP_DEFAULT_HTTP_PORT;
		memset(shm->config.http_passwd, 0, sizeof(shm->config.http_passwd));
		shm->config.http_default_passwd = 0;
	}
	else {
		shm->config.services |= RCP_SERVICE_HTTP;		
		shm->config.http_port = RCP_DEFAULT_HTTP_PORT;
		memset(shm->config.http_passwd, 0, sizeof(shm->config.http_passwd));
		shm->config.http_default_passwd = 0;
	}
	
	// trigger a system update
	update_xinetd_files = 1;
	return 0;
}

int cliServiceHttpPassword(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	int index = 2;

	// configure port
	if (strcmp(argv[index], "port") == 0) {
		int p = atoi(argv[index + 1]);
		if ((uint16_t) p == shm->config.sec_port) {
			strcpy(data, "Error: port already in use for SSH");
			return RCPERR;
		}
		else if ((uint16_t) p == shm->config.telnet_port) {
			strcpy(data, "Error: port already in use for telnet");
			return RCPERR;
		}
		shm->config.http_port = (uint16_t) p;
		shm->config.services |= RCP_SERVICE_HTTP;
		index += 2;
	}
	
	// configure encrypted password
	if (argc > index && strcmp(argv[index], "encrypted") == 0) {
		strcpy(shm->config.http_passwd, argv[index + 2]);
		shm->config.services |= RCP_SERVICE_HTTP;
	}
	
	// configure unencrypted password
	if (argc > index && strcmp(argv[index], "password") == 0) {
		if (strcmp(argv[index + 1], "rcp") == 0)
			shm->config.http_default_passwd = 1;
	
		// generate solt
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++) {
			unsigned s = ((unsigned) rand()) % ((unsigned) 'Z' - (unsigned) 'A');
			solt[i] = 'A' + s;
		}
		solt[RCP_CRYPT_SALT_LEN] = '\0';

		// calculate md5 on solt + string
		int len = RCP_CRYPT_SALT_LEN + strlen(argv[index + 1]);
		char str[len + 1];
		strcpy(str, solt);
		strcat(str, argv[index + 1]);

		// calculate md5
		MD5_CTX context;
		unsigned char digest[16];
		
		MD5Init(&context);
		MD5Update(&context, (unsigned char *) str, len); 
		MD5Final (digest, &context);
		
		// store solt and digest in shared memory
		strcpy(shm->config.http_passwd, solt);
		strcat(shm->config.http_passwd, "$");
		for (i = 0; i < 16; i++) {
			sprintf(shm->config.http_passwd + RCP_CRYPT_SALT_LEN + 1 + i * 2, "%02x", ((unsigned char) digest[i]) & 0xff);
		}
		shm->config.services |= RCP_SERVICE_HTTP;
	}
	
	// trigger a system update
	update_xinetd_files = 1;
	return 0;
}

//******************************************************************
// FTP
//******************************************************************
int cliServiceFtp(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0';

	if (strcmp(argv[0], "no") == 0) {
		shm->config.services &= ~RCP_SERVICE_FTP;
	}
	else
		shm->config.services |= RCP_SERVICE_FTP;		
	
	// trigger a system update
	update_xinetd_files = 1;
	return 0;
}

	
