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

extern void configInterfaces(FILE *fp, int no_passwords);
extern void configLogger(FILE *fp, int no_passwords);
extern void configAdministrators(FILE *fp, int no_passwords);
extern void configServices(FILE *fp, int no_passwords);
extern void configArp(FILE *fp, int no_passwords);
extern void configRoute(FILE *fp, int no_passwords);
extern void configRip(FILE *fp, int no_passwords);
extern void configOspf(FILE *fp, int no_passwords);
extern void configOspfIf(FILE *fp, int no_passwords);
extern void configDhcp(FILE *fp, int no_passwords);
extern void configDhcpIf(FILE *fp, int no_passwords);
extern void configAcl(FILE *fp, int no_passwords);
extern void configNat(FILE *fp, int no_passwords);
extern void configDns(FILE *fp, int no_passwords);
extern void configNtp(FILE *fp, int no_passwords);
extern void configIpsecTun(FILE *fp, int no_passwords);
extern void configNetmon(FILE *fp, int no_passwords);
extern void configSnmp(FILE *fp, int no_passwords);

int cliConfigureCmd(CliMode mode, int argc, char **argv) {
	ASSERT(csession.cmode == CLIMODE_ENABLE);
	csession.cmode = CLIMODE_CONFIG;

	return 0;
}


int cliShowConfigurationCmd(CliMode mode, int argc, char **argv) {
	int no_passwords = 0;
	ASSERT(shm != NULL);

	// generate a temporary file
	char file_name[50] = "/opt/rcp/var/transport/cXXXXXX";
	int fd = mkstemp(file_name);
	if (fd == -1) {
		ASSERT(0)
		printf("Error: cannot create temporary file\n");
		return RCPERR;
	}
	close(fd);

	// open the file
	FILE *fp = fopen(file_name, "w");
	if (fp == NULL) {
		printf("Error: cannot open temporary file\n");
		return RCPERR;
	}

	if (argc == 3 && strcmp(argv[2], "no-passwords") == 0)
		no_passwords = 1;
		
	if (argc == 2 || (argc == 3 && no_passwords)) {
		// general configuration from services/config directory
		configLogger(fp, no_passwords);
		configDns(fp, no_passwords);
		configServices(fp, no_passwords);
		configAdministrators(fp, no_passwords);
		
		// ntp configuration
		configNtp(fp, no_passwords);
		
		// network monitoring and SNMP configuration
		configNetmon(fp, no_passwords);
		configSnmp(fp, no_passwords);

		// router configuration
		configArp(fp, no_passwords);
		configRoute(fp, no_passwords);
		configAcl(fp, no_passwords);
		configNat(fp, no_passwords);
		configIpsecTun(fp, no_passwords);

		// dhcp configuration
		configDhcp(fp, no_passwords);
		
		
		// rip configuration
		configRip(fp, no_passwords);
		
		// ospf configuration
		configOspf(fp, no_passwords);
	
		// print interfaces
		configInterfaces(fp, no_passwords);
	}
	else if (argc == 3) {
		if (strcmp(argv[2], "dns") == 0)
			configDns(fp, no_passwords);
		else if (strcmp(argv[2], "ntp") == 0)
			configNtp(fp, no_passwords);
		else if (strcmp(argv[2], "arp") == 0)
			configArp(fp, no_passwords);
		else if (strcmp(argv[2], "logger") == 0)
			configLogger(fp, no_passwords);
		else if (strcmp(argv[2], "administrators") == 0)
			configAdministrators(fp, no_passwords);
		else if (strcmp(argv[2], "services") == 0)
			configServices(fp, no_passwords);
		else if (strcmp(argv[2], "interfaces") == 0)
			configInterfaces(fp, no_passwords);
		else if (strcmp(argv[2], "acl") == 0)
			configAcl(fp, no_passwords);
		else if (strcmp(argv[2], "nat") == 0)
			configNat(fp, no_passwords);

		else if (strcmp(argv[2], "tunnel") == 0)
			configIpsecTun(fp, no_passwords);

		else if (strcmp(argv[2], "monitor") == 0)
			configNetmon(fp, no_passwords);
		else if (strcmp(argv[2], "snmp") == 0)
			configSnmp(fp, no_passwords);
		else if (strcmp(argv[2], "route") == 0)
			configRoute(fp, no_passwords);
		else if (strcmp(argv[2], "dhcp") == 0) {
			configDhcp(fp, no_passwords);
			configDhcpIf(fp, no_passwords);
		}
		else if (strcmp(argv[2], "rip") == 0)
			configRip(fp, no_passwords);
		else if (strcmp(argv[2], "ospf") == 0) {
			configOspf(fp, no_passwords);
			configOspfIf(fp, no_passwords);
		}
		else if (strcmp(argv[2], "no-passwords") == 0)
			;
		else
			ASSERT(0);
	}


	// close the file, print and delete it
	fclose(fp);
	cliPrintFile(file_name);
	unlink(file_name);
	
	return 0;
}

int cliCopyRunStartCmd(CliMode mode, int argc, char **argv) {
	char outmods[50];
	strcpy(outmods, "| file /home/rcp/startup-config");
	cliSetOutMods(outmods);

	char *newargv[] = {"show", "configuration"};

	return cliShowConfigurationCmd(mode, 2, newargv);
}

int cliShowStartupConfigurationCmd(CliMode mode, int argc, char **argv) {
	cliPrintFile("/home/rcp/startup-config");
	return 0;
}

int checkEnablePassword(CliMode mode, const char *passwd) {
	if (csession.cmode != CLIMODE_LOGIN)
		return RCPERR;
		
	{ // check password
		char solt[RCP_CRYPT_SALT_LEN + 1];
		int i;
		
		for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
			solt[i] = shm->config.enable_passwd[i];
		solt[RCP_CRYPT_SALT_LEN] = '\0';

		char *cp = rcpCrypt(passwd, solt);
		if (strcmp(shm->config.enable_passwd + RCP_CRYPT_SALT_LEN + 1, cp + RCP_CRYPT_SALT_LEN + 4) != 0) {
			printf("Error: invalid password\n");
			return RCPERR;
		}
	}
	
	csession.cmode = CLIMODE_ENABLE;
	return 0;
}

int cliEnableCmd(CliMode mode, int argc, char **argv) {
	ASSERT(csession.cmode == CLIMODE_LOGIN);

	if (shm->config.enable_passwd_configured) {
		printf("\nPassword: ");
		fflush(0);

		// get user password
		char passwd[CLI_MAX_PASSWD + 1];
		if (NULL == fgets(passwd, CLI_MAX_PASSWD, stdin)) {
			printf("Error: cannot read password\n");
			return RCPERR;
		}
		
		// remove '\n'
		char *ptr = strchr(passwd, '\n');
		if (ptr == NULL) {
			printf("Error: invalid password\n");
			return RCPERR;
		}
		*ptr = '\0';
		
		{ // check password
			char solt[RCP_CRYPT_SALT_LEN + 1];
			int i;
			
			for (i = 0; i < RCP_CRYPT_SALT_LEN; i++)
				solt[i] = shm->config.enable_passwd[i];
			solt[RCP_CRYPT_SALT_LEN] = '\0';
	
			char *cp = rcpCrypt(passwd, solt);
			if (strcmp(shm->config.enable_passwd + RCP_CRYPT_SALT_LEN + 1, cp + RCP_CRYPT_SALT_LEN + 4) != 0) {
				printf("Error: invalid password\n");
				return RCPERR;
			}
		}
		printf("\n");
	}
	
	csession.cmode = CLIMODE_ENABLE;
	return 0;
}




