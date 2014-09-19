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
extern int checkEnablePassword(CliMode mode, const char *passwd);

// return 1 if error, 0 if OK
int cliRunScript(const char *fname) {
	ASSERT(fname != NULL);
	FILE *fp = fopen(fname, "r");
	if (fp == NULL)
		return 1;
#define BUFSIZE (CLI_MAX_LINE * 1)	
	char buf[BUFSIZE];
	while (fgets(buf, BUFSIZE, fp)) {
		// remove \n
		char *ptr = strchr(buf, '\n');
		if (ptr == NULL) {
			fclose(fp);
			return 1;
		}
		*ptr = '\0';
		
		// do not run comments
		ptr = strchr(buf, '!');
		if (ptr != NULL)
			*ptr = '\0';
		
		// do not run empty commands
		ptr = buf;
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		if (*ptr == '\0')
			continue;
			
		// run the command
		ptr = buf;
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		
		// print prompt
		int prlen = cliPrintPrompt();

		// interpret enable:password script command
		if (strncmp("enable:", ptr, 7) == 0) {
			printf("enable\n");
			ptr += 7;
			// trim end of the buffer
			char *ptr2 = ptr;
			while (*ptr2 != '\0') {
				if (*ptr2 == ' ')
					*ptr2 = '\0';
				ptr2++;
			}
		
			checkEnablePassword(csession.cmode, ptr);
		}
		else {
			printf("%s\n", ptr);
			cli_process_cmd(ptr, prlen, 0);
		}
	}
	fclose(fp);
	return 0;
}
