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
#define _GNU_SOURCE
#include "librcp.h"
#include <signal.h>
#include <time.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <execinfo.h>

static int usr2_handler(int sig) {
	printf("User requested exit (USR2)\n");
	fflush(0);
	exit(1);
}

static int crash_handler(int sig) {
	// extract process name
	const char *prog = rcpGetProcName();
	
	// extract time
	char current[50];
	time_t now = time(NULL);
	ctime_r(&now, current);
	
	// print error in console
	fprintf(stderr, "\033[31mError: Process %s creashed on %s\033[0m\n", prog, current);

	
	// print process status
	fprintf(stderr, "********** status **********************\n");	
	FILE *fp = fopen("/proc/self/status", "r");
	if (fp != NULL) {
		char buf[1000 + 1];
		buf[1000] = '\0';
		while (fgets(buf, 1000, fp) != NULL)
			fprintf(stderr, "%s", buf);
		fclose(fp);
	}

	// print memory map
	fprintf(stderr, "********** memory map **********************\n");	
	fp = fopen("/proc/self/maps", "r");
	if (fp != NULL) {
		char buf[1000 + 1];
		buf[1000] = '\0';
		while (fgets(buf, 1000, fp) != NULL)
			fprintf(stderr, "%s", buf);
		fclose(fp);
	}

	// try to generate the crushdump using gdb
	fprintf(stderr, "********** stack trace **********************\n");	
	struct stat buf;
	if (stat("/usr/bin/gdb", &buf) == 0) {
		fp = fopen("gdbdump", "w");
		if (fp != NULL) {
			// create a command file
			fprintf(fp, "set height 0\n");
			fprintf(fp, "bt full\n");
			fprintf(fp, "quit\n");
			fclose(fp);
		
			// build gdb command and run it
			char command[200];
			memset(command, 0, 200);
			snprintf(command, 199, "gdb -batch -x ./gdbdump --pid %d  < /dev/null 2>&1", getpid());
			int rv = system(command);
			(void) rv;
		}
	}
	// if not, generate a backtrace
	else {
		void *array[30];
		size_t size;
		
		size = backtrace(array, 30);
		fprintf(stderr, "Error: signal %d received\n", sig);
		backtrace_symbols_fd(array, size, 2);
	}

	abort();				  
	return 0;
}


void rcpInitCrashHandler(void) {
	// kill -8
	signal(SIGFPE, (sighandler_t) crash_handler);
	// kill -4
	signal(SIGILL, (sighandler_t) crash_handler);
	// kill -11
	signal(SIGSEGV, (sighandler_t) crash_handler);
	// kill -7
	signal(SIGBUS, (sighandler_t) crash_handler);

	signal(SIGUSR2, (sighandler_t) usr2_handler);
	signal(SIGUSR1, (sighandler_t) usr2_handler);
}
