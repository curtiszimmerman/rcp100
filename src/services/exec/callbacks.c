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

RcpPkt *pktout = NULL;
int muxsession;

// dispatch callback
int processCli(RcpPkt *pkt, int session) {
	muxsession = session;
	pktout = pkt;
	char *data = (char *) pkt + sizeof(RcpPkt);
	int rv = cliExecNode(pkt->pkt.cli.mode, pkt->pkt.cli.clinode, data);
	// set new data length
	int len = strlen(data);
	pkt->data_len = (len == 0)? 0: len + 1;
	return rv;
}

int cliSetEnablePasswordCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	ASSERT(mode == CLIMODE_CONFIG);
	shm->config.enable_passwd_configured = 1;
	
	{
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++) {
			unsigned s = ((unsigned) rand()) % ((unsigned) 'Z' - (unsigned) 'A');
			solt[i] = 'A' + s;
		}
		solt[RCP_CRYPT_SALT_LEN] = '\0';
		
		char *hash = rcpCrypt(argv[2], solt);
		ASSERT(strlen(hash + 3) <= CLI_MAX_PASSWD);
		memset(shm->config.enable_passwd, 0, CLI_MAX_PASSWD + 1);
		strncpy(shm->config.enable_passwd, hash + 3, CLI_MAX_PASSWD);
	}
	
	return 0;
}

int cliSetEnableEncryptedPasswordCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	ASSERT(mode == CLIMODE_CONFIG);
	shm->config.enable_passwd_configured = 1;
	ASSERT(strlen(argv[3]) <= CLI_MAX_PASSWD);
	memset(shm->config.enable_passwd, 0, CLI_MAX_PASSWD + 1);
	strncpy(shm->config.enable_passwd, argv[3], CLI_MAX_PASSWD);
	return 0;
}

int cliNoSetEnablePasswordCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	ASSERT(mode == CLIMODE_CONFIG);
	shm->config.enable_passwd_configured = 0;
	memset(shm->config.enable_passwd, 0, sizeof(shm->config.enable_passwd));
	return 0;
}

int cliShowAdministratorsCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	int i;
	fprintf(fp, "%-20.20s %-17s%-14.14s %s\n", "Administrator", "IP Address", "TTY", "Start Time");
	for (i = 0; i < MAX_SESSIONS; i++) {
		if (mux[i].socket == 0)
			continue;
		if (mux[i].connected_process != RCP_PROC_CLI)
			continue;
		
		fprintf(fp, "%-20.20s %-17s%-14.14s %s", // ctime adds a \n
			(mux[i].admin[0] != '\0')? mux[i].admin: "unknown",
			(mux[i].ip[0] != '\0')? mux[i].ip: "unknown",
			(mux[i].ttyname[0] != '\0')? mux[i].ttyname: "unknown",
			ctime(&mux[i].start_time));
	}

	fclose(fp);
	return 0;
}

static RcpAdmin *admin_find(const char *name) {
	int i;
	RcpAdmin *ptr;
	
	// find the admin in the array
	for (i = 0, ptr = shm->config.admin; i < RCP_ADMIN_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		if (strcmp(ptr->name, name) == 0)
			return ptr;
	}
	
	return NULL;
}

static RcpAdmin *admin_find_empty(const char *name) {
	int i;
	RcpAdmin *ptr;
	
	// find empty entry
	for (i = 0, ptr = shm->config.admin; i < RCP_ADMIN_LIMIT; i++, ptr++) {
		if (ptr->valid)
			continue;
		return ptr;	
	}
	
	return NULL;
}


int cliAdminPasswdCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	ASSERT(mode == CLIMODE_CONFIG);
	
	if (strcmp(argv[1], "rcp") == 0 && strcmp(argv[3], "rcp") == 0)
		shm->config.admin_default_passwd = 1;
	else if (strcmp(argv[1], "rcp") == 0 && strcmp(argv[3], "rcp") != 0)
		shm->config.admin_default_passwd = 0;
		
		
		
	// encrypt the password
	char solt[RCP_CRYPT_SALT_LEN + 1];
	int i;
	for (i = 0; i < RCP_CRYPT_SALT_LEN; i++) {
		unsigned s = ((unsigned) rand()) % ((unsigned) 'Z' - (unsigned) 'A');
		solt[i] = 'A' + s;
	}
	solt[RCP_CRYPT_SALT_LEN] = '\0';
	char *cp = rcpCrypt(argv[3], solt);
	ASSERT(strlen(cp + 3) <= CLI_MAX_PASSWD);
	
	// find an existing admin and change the password	
	RcpAdmin *admin = admin_find(argv[1]);
	if (admin != NULL) {
		memset(admin->password, 0, CLI_MAX_PASSWD + 1);
		strncpy(admin->password, cp + 3, CLI_MAX_PASSWD);
		return 0;
	}
	
	// create a new admin
	admin = admin_find_empty(argv[1]);
	if (admin == NULL) {
		strcpy(data, "Error: cannot add administrator account, limit reached\n");
		return RCPERR;
	}
	memset(admin, 0, sizeof(RcpAdmin));
	strcpy(admin->name, argv[1]);
	strncpy(admin->password, cp + 3, CLI_MAX_PASSWD);
	admin->valid = 1;
		
	return 0;
}

int cliAdminEncPasswdCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	// find an existing admin and change the password
	RcpAdmin *admin = admin_find(argv[1]);
	if (admin != NULL) {
		strncpy(admin->password, argv[4], CLI_MAX_PASSWD);
		return 0;
	}
	


	// create a new admin
	admin = admin_find_empty(argv[1]);
	if (admin == NULL) {
		strcpy(data, "Error: cannot add administrator account, limit reached\n");
		return RCPERR;
	}
	memset(admin, 0, sizeof(RcpAdmin));
	strcpy(admin->name, argv[1]);
	strncpy(admin->password, argv[4], CLI_MAX_PASSWD);
	admin->valid = 1;;

	return 0;
}

int cliNoAdminCmd(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string

	if (strcmp(argv[2], "rcp") == 0)
		shm->config.admin_default_passwd = 0;
	
	// find an existing admin
	RcpAdmin *admin = admin_find(argv[2]);
	if (admin == NULL)
		return 0;
	
	// remove it
	admin->valid = 0;
	return 0;
}

int cliExecTimeout(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	if (strcmp(argv[0], "no") == 0) {
		shm->config.timeout_minutes = CLI_DEFAULT_TIMEOUT;	
		shm->config.timeout_seconds = 0;
		return 0;
	}

	shm->config.timeout_minutes = atoi(argv[1]);
	if (argc == 3)
		shm->config.timeout_seconds = atoi(argv[2]);
	else
		shm->config.timeout_seconds = 0;

	return 0;
}


int cliDebugMux(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	int i;
	
	// mux sessions
	{
		MuxSession *ptr;
		for (i = 0, ptr =&mux[0]; i < MAX_SESSIONS; i++, ptr++) {
			if (ptr->socket == 0)
				continue;
			
			fprintf(fp, "mux: %d/%s/%d\n",
				i,  rcpProcName(ptr->connected_process), ptr->socket);
		}
	}

	// watchdog timers
	{
		RcpProcStats *ptr;
	
		for (i = 0, ptr = &shm->config.pstats[i]; i < (RCP_PROC_MAX + 2); i++, ptr++) {
			// all running processes
			if (!ptr->proc_type)
				continue;
			
			// but not us or cli processes or test processes
			if (ptr->proc_type == RCP_PROC_CLI ||
			     ptr->proc_type == RCP_PROC_SERVICES ||
			     ptr->proc_type == RCP_PROC_TESTPROC)
				continue;
			
			// started processes
			if (ptr->wproc == 0)
				continue;
	
			fprintf(fp, "process %s, pid %d, watchdog %d/%d\n",
				rcpProcName(ptr->proc_type), ptr->pid, ptr->wproc, ptr->wmonitor);
		}
	}

	fclose(fp);
	return 0;
}

#define BUFSIZE 1024
int cliDebugLog(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	// open log file
	FILE *fin = NULL;
	if (strcmp(argv[2], "xinetd") == 0)
		fin = fopen("/opt/rcp/var/log/xinetd", "r");
	else if (strcmp(argv[2], "router") == 0)
		fin = fopen("/opt/rcp/var/log/rcprouter", "r");
	else if (strcmp(argv[2], "restarts") == 0)
		fin = fopen("/opt/rcp/var/log/restart", "r");
	else
		ASSERT(0);

	if (fin == NULL) {
		fprintf(fp, "Error: cannot open debug log file\n");
		fclose(fp);
		return RCPERR;
	}
	
	// transfer data
	char buf[BUFSIZE];
	while (fgets(buf, BUFSIZE, fin))
		fprintf(fp, "%s", buf);
	
	fclose(fin);
	fclose(fp);
	return 0;
}

int cliDebugLogRouter(CliMode mode, int argc, char **argv) {
	char *data = (char *) pktout + sizeof(RcpPkt);
	*data = '\0'; // cleaning the return string
	
	FILE *fp = rcpTransportFile(data);
	if (fp == NULL) {
		ASSERT(0);
		strcpy(data, "Error: cannot create temporary file\n");
		return RCPERR;
	}
	pktout->pkt.cli.file_transport = 1;

	// open log file
	FILE *fin = fopen("/opt/rcp/var/log/rcprouter", "r");
	if (fin == NULL) {
		fprintf(fp, "Error: cannot open rcprouter debug log\n");
		fclose(fp);
		return RCPERR;
	}
	
	// transfer data
	char buf[BUFSIZE];
	while (fgets(buf, BUFSIZE, fin))
		fprintf(fp, "%s", buf);
	
	fclose(fin);
	fclose(fp);
	return 0;
}

