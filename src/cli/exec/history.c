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

#ifndef UTEST_HIST
#define MAX_HIST_SIZE 64
#else
#define MAX_HIST_SIZE 3
#endif

typedef struct {
	int current;
	char *cmd[MAX_HIST_SIZE];
} CliHist;

static CliHist hist[CLIMODE_MAX];
static int initialized = 0;
static int arrow = 0;


static char last[CLI_MAX_LINE];
static char last_tmp[CLI_MAX_LINE];

static const char *cliGetHistLast(void) {
	int h = hist[cliModeIndex(csession.cmode)].current;
	--h;
	if (h < 0)
		h = MAX_HIST_SIZE - 1;
		
	const char *retval = hist[cliModeIndex(csession.cmode)].cmd[h];
	if (retval == NULL)
		return "";
	return retval;
}

void cliSetHistory(const char *cmd) {
	strcpy(last, cmd);
}

void cliCommitHistory(void) {
	if (initialized == 0) {
		memset(hist, 0, sizeof(hist));
		initialized = 1;
	}
	
	// do not accept show history
	if (strncmp(last, "show history", strlen("show history")) == 0) {
		last[0] = '\0';
		return;
	}

	// do not add the same command twice in a raw
	const char *tmp = cliGetHistLast();
	if (strlen(tmp) != 0 && strcmp(last, tmp) == 0) {
		last[0] = '\0';
		arrow = hist[cliModeIndex(csession.cmode)].current;
		return;
	}
	
	// allocate the new history string and copy the command
	char *h = malloc(strlen(last) + 1);
	if (h == NULL) {
		ASSERT(0);
		last[0] = '\0';
		return;
	}
	strcpy(h, last);
	last[0] = '\0';

	// set the top of the list
	if (hist[cliModeIndex(csession.cmode)].current == 0) {
		hist[cliModeIndex(csession.cmode)].cmd[0] = h;
		hist[cliModeIndex(csession.cmode)].current++;
	}
	// middle of the list
	else if (hist[cliModeIndex(csession.cmode)].current < (MAX_HIST_SIZE -1)) {
		hist[cliModeIndex(csession.cmode)].cmd[hist[cliModeIndex(csession.cmode)].current] = h;
		hist[cliModeIndex(csession.cmode)].current++;
	}
	// end of the list: hist[cliModeIndex(csession.cmode)].current == (MAX_HIST_SIZE -1), no last entry
	else if (hist[cliModeIndex(csession.cmode)].cmd[MAX_HIST_SIZE - 1] == NULL) {
		hist[cliModeIndex(csession.cmode)].cmd[hist[cliModeIndex(csession.cmode)].current] = h;
		hist[cliModeIndex(csession.cmode)].current++; // force it in MAX_HIST_SIZE position
	}
	// outside the list, wrapping around
	else {
		// release the first entry
		free(hist[cliModeIndex(csession.cmode)].cmd[0]);
		
		// move pointers around
		int i;
		for (i = 1; i < MAX_HIST_SIZE; i++) {
			hist[cliModeIndex(csession.cmode)].cmd[i - 1] = hist[cliModeIndex(csession.cmode)].cmd[i];
		}
		hist[cliModeIndex(csession.cmode)].cmd[MAX_HIST_SIZE - 1] = h;
	}


	arrow = hist[cliModeIndex(csession.cmode)].current;
	rcpDebug("cli: history current %d\n", hist[cliModeIndex(csession.cmode)].current);
}

int cliShowHistoryCmd(int argc, char **argv) {
	if (initialized == 0) {
		memset(hist, 0, sizeof(hist));
		initialized = 1;
	}

	int i;
	for (i = 0; i < hist[cliModeIndex(csession.cmode)].current; i++)
		cliPrintLine(hist[cliModeIndex(csession.cmode)].cmd[i]);
	
	return 0;
}

const char *cliGetHistoryArrow(int up) {
	if (last[0] != '\0') {
		strcpy(last_tmp, last);
		last[0] = '\0';
		return last_tmp;
	}

	if (up) {
		if (arrow > 0) 
			--arrow;
	}	
	else {
		if (arrow < hist[cliModeIndex(csession.cmode)].current)
			arrow++;
	}

	rcpDebug("cli: history arrow %d\n", arrow);
	if (hist[cliModeIndex(csession.cmode)].cmd[arrow] == NULL)
		return "";
		
	return hist[cliModeIndex(csession.cmode)].cmd[arrow];
}

