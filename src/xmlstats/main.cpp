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
#include "xmlstats.h"
int debug = 0;
RcpShm *shm = NULL;

static void usage(char **argv) {
	printf("Usage: %s option\n", argv[0]);
	printf("where option is:\n");
	printf("\t--help - this help text\n");
	printf("\t--load - load /home/rcp/xmlstats file\n");
	printf("\t--save - save /home/rcp/xmlstats file\n");
	printf("\t--debug - debug mode\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv);
		return 1;
	}
	
	int i;
	int do_load = 0;
	int do_save = 0;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--load") == 0)
			do_load = 1;
		else if (strcmp(argv[i], "--save") == 0)
			do_save = 1;
		else if (strcmp(argv[i], "--debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-?") == 0) {
			usage(argv);
			return 0;
		}
	}

	// open shared memory
	
	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}
	
	if (do_load)
		load();
	else if (do_save)
		save();
	else
		ASSERT(0);
		
	return 0;
}
