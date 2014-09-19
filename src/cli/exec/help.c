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

#define MAX_SORT 1024 // it will assert above that
typedef struct {
	const char *token;
	const char *help;
} HelpSort;
static int hcnt = 0; // count the number of valid structures
static HelpSort hstruct[MAX_SORT];

// these commands are shown in help only in enable mode 
static char *restricted[] = {
	"copy",
	"dir",
	"delete",
	"logout",
	"debug",
	"ssh-client",
	"telnet-client",
	"traceroute",
	"ping",
	"terminal",
	"clear",
	"exec-timeout",
	"restart",
	NULL
};

void cliStoreHelp(CliNode *node) {
	ASSERT(node != NULL);
	ASSERT(node->token != NULL);
	rcpDebug("cli: cliStoreHelp node token %s\n", node->token);
	
	// some commands are restricted to enable mode
	if (csession.cmode != CLIMODE_ENABLE && csession.cmode != CLIMODE_LOGIN) {
		int i = 0;
		while (restricted[i] != NULL) {
			if (strcmp(restricted[i], node->token) == 0)
				return;
			i++;
		}
	}
	
	const char *hs = node->token;
	
	// special token processing
	if (*node->token == '<') {
		ASSERT(node->help_special != NULL);
		if (node->help_special)
			hs = node->help_special;
	}
	
	hstruct[hcnt].token = hs;
	hstruct[hcnt].help = node->help;
	hcnt++;
	if (hcnt >= MAX_SORT) {
		ASSERT(0);
		hcnt--;
	}
}

static int help_cmp(const void *h1, const void *h2) {
	HelpSort *hs1 = (HelpSort *) h1;
	HelpSort *hs2 = (HelpSort *) h2;
	
	return strcmp(hs1->token, hs2->token);
}

void cliPrintHelp(void) {
	if (hcnt == 0) // nothing to print
		return;
		
	// sort the array of structures using the token member
	qsort(hstruct, hcnt, sizeof(HelpSort), help_cmp);

	int i;
	for (i = 0; i < hcnt; i++) {
		cliPrintLine("  %-25.25s%-3s%s", hstruct[i].token, "", (hstruct[i].help)? hstruct[i].help: "");
	}
	// reset the array of structures
	hcnt = 0;
}

