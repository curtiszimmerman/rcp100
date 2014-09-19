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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "http.h"

#define BUFSIZE 1024
static unsigned char buf[BUFSIZE + 1];


static void send_file(const char *fname, const char *type) {
	// check if the file is installed and get the length
 	struct stat s;
	if (stat(fname, &s) == 0) {
		int size = (int) s.st_size;
		if (size == 0)
			return;
		
		// open the file
		int fd = open(fname, O_RDONLY);
		if (fd == -1) {
			return;
		}
		
		// send the file
		send_headers( 200, "Ok", (char*) 0, (char *) type, size, -1);
		fflush(0);
		int len;
		while ((len = read(fd, buf, BUFSIZE)) > 0) {
			int v = write(STDOUT_FILENO, buf, len);
			(void) v;
		}
		close(fd);
		fflush(0);
	}
}

static void rcps_file(const char *fname, const char *type) {
	FILE *fp = fopen(fname, "r");
	if (!fp)
		return;

	// create a temporary html file
	char *fname_tmp = rcpGetTempFile();
	if (!fname_tmp)
		return;
	FILE *fptmp = fopen(fname_tmp, "w");
	if (!fptmp)
		return;

	char *line = (char *) buf;
	int linecnt = 0;
	while (fgets(line, BUFSIZE, fp)) {
		linecnt++;
		char *ptr = line;
		int interpreted = 0;
		
		char *start = ptr;
		while ((ptr = strstr(ptr, "<?rcps")) != NULL) {
			interpreted = 1;
			
			// print up to here
			*ptr = '\0';
			fprintf(fptmp, "%s", start);
			
			// advance
			ptr += 6;
			while (*ptr != '\n' && (*ptr == ' ' || *ptr == '\t')) {
				ptr++;
			}
			if (*ptr == '\n') {
				printf("Error: line %d, missing function name\n", linecnt);
				break;
			}

			// advance to the end of the function name
			start = ptr;
			while (*ptr != '\n' && *ptr != '>')
				ptr++;
			if (*ptr == '\n') {
				printf("Error: line %d, missing rcps terminator\n", linecnt);
				break;
			}
			
			// call the function name
			*ptr = '\0';
			ptr++;
			
			int i = 0;
			int found = 0;
			
			while (httpcgibind[i].fname != NULL) {
				if (strcmp(httpcgibind[i].fname, start) == 0) {
					found = 1;
					httpcgibind[i].f(fptmp);
					break;
				}
				i++;
			}
			if (!found) {
				printf("Error: line %d, function #%s# not found\n", linecnt, start);
				break;
			}
			start = ptr;
		}
		if (interpreted)
			fprintf(fptmp, "%s", start);
		else	
			fprintf(fptmp, "%s", line);
	}
	fclose(fptmp);
	fclose(fp);
	
	send_file(fname_tmp, type);
	unlink(fname_tmp);
	free(fname_tmp);
}



void www_file(const char *fname, const char *type) {
	// look for rcps extension
	if (strstr(fname, ".rcps"))
		return rcps_file(fname, type);

	send_file(fname, type);		
}
