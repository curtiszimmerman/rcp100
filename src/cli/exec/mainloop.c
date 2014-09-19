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

static int cli_dbg = 0;

// return 0 if the user accepts, 1 if not
static int ask_question(const char *question) {
	// don't ask anyting for html session
	if (csession.html)
		return 0;

	printf("\n%s\n\n", question);
	printf("Do you want to continue (yes/no)? ");
	fflush(0);
	
	cliRestoreTerminal();
	char input[10];
	if (fgets(input, 10, stdin) == NULL)
		return 1;
	cliSetTerminal();
	
	// skip white spaces
	char *ptr = input;
	while (*ptr != '\0') {
		if (*ptr == ' ' || *ptr == '\t')
			ptr++;
		else
			break;
	}
	
	if (*ptr == '\0')
		return 1;
	
	if (strncasecmp(ptr, "yes", 3) == 0)
		return 0;
	return 1;
}

// returns preserve input flag
static int cli_process_tab(char *cli_line_buffer) {
	rcpDebug("cli: processing tab\n");
	CliNode *n = cliParserFindCmd(csession.cmode, head, cli_line_buffer);
	rcpDebug("cli: node %p, multi #%s#\n", n, cliParserGetLastOp()->word_multimatch);
	rcpDebug("cli: completed #%s#\n", cliParserGetLastOp()->word_completed);

	// check debug mode
	if (strncmp(cliParserGetLastOp()->word_completed, "debug", 5) == 0 && cli_dbg == 0) {
		printf("Error: the feature is available only in debug mode\n");
		return 0;
	}

	if (cliParserGetLastOp()->last_node_match != NULL)
		rcpDebug("cli: last match %s\n", cliParserGetLastOp()->last_node_match->token);

	char *ptr;
	if (n != NULL)
		strcpy(cli_line_buffer, cliParserGetLastOp()->word_completed);
	else if (cli_dbg == 0 && (ptr = strstr(cliParserGetLastOp()->word_multimatch, "debug ")) != NULL) {
		// eliminate debug keyword
		int i;
		for (i = 0; i < strlen("debug"); i++, ptr++)
			*ptr = ' ';
		printf("%s\n", cliParserGetLastOp()->word_multimatch);
	}
	else
		printf("%s\n", cliParserGetLastOp()->word_multimatch);

	return 1;
}


// returns preserve input flag
static int cli_process_question(char *cli_line_buffer) {
	rcpDebug("cli: processing question, #%s#\n", cli_line_buffer);
	CliNode *n = cliParserFindCmd(csession.cmode, head, cli_line_buffer);
	rcpDebug("cli: node %p, multi #%s#\n", n, cliParserGetLastOp()->word_multimatch);
	rcpDebug("cli: completed #%s#\n", cliParserGetLastOp()->word_completed);

	// check debug mode
	if (strncmp(cliParserGetLastOp()->word_completed, "debug", 5) == 0 && cli_dbg == 0) {
		printf("Error: the feature is available only in debug mode\n");
		return 0;
	}

	if (cliParserGetLastOp()->last_node_match != NULL)
		rcpDebug("cli: last match %s\n", cliParserGetLastOp()->last_node_match->token);

	CliNode *n2 = cliParserGetLastOp()->last_node_match;
	rcpDebug("cli: node n2 %p\n", n2);
	
	char *mm = cliParserGetLastOp()->word_multimatch;
	if (n2 == NULL) {
		n2 = head;
		while (n2 != NULL) {
			rcpDebug("cli: n2 token %s\n", n2->token);
			// walk the next list
			if (csession.cmode & n2->mode) {
				if (strcmp(n2->token, "debug") == 0 && cli_dbg == 0)
					;
				else {
					if (mm == NULL || strlen(mm) == 0)
						cliStoreHelp(n2);
					else if (strstr(mm, n2->token) != NULL)
						cliStoreHelp(n2);
				}
			}
						
			n2 = n2->next;
		}
	}
	else {
		n2 = n2->rule;
		rcpDebug("cli: advance to node %p\n", n2);
		while (n2 != NULL) {
			// walk the next list
			if (csession.cmode & n2->mode) {
				if (strcmp(n2->token, "debug") == 0 && cli_dbg == 0)
					;
				else {
					if (mm == NULL || strlen(mm) == 0)
						cliStoreHelp(n2);
					else if (strstr(mm, n2->token) != NULL)
						cliStoreHelp(n2);
				}
			}
			n2 = n2->next;
		}
	}

	cliPrintHelp();

	if (n != NULL && n->exe && (n->exe_mode & csession.cmode))
		cliPrintLine("  <CR>\n");

	// reset the output modifiers
	cliResetOutMods();

	strcpy(cli_line_buffer, cliParserGetLastOp()->word_completed);
	return 1;
}

static int last_cmd_ok = 0;
int cli_process_cmd(char *cli_line_buffer, int prompt_len, int noerr) {
	int preserve_input = 0;
	last_cmd_ok = 0;

	// if the line is empty, do not try to process it
	{
		char *ptr = cli_line_buffer;
		int found = 0;
		while (*ptr != '\0') {
			if (*ptr != ' ' && *ptr != '\t') {
				found = 1;
				break;
			}
			ptr++;
		}
		if (found == 0)
			return preserve_input;
	}

	rcpDebug("cli: processing enter\n");
	CliNode *n = cliParserFindCmd(csession.cmode, head, cli_line_buffer);
	rcpDebug("cli: node %p, multi #%s#\n", n, cliParserGetLastOp()->word_multimatch);
	rcpDebug("cli: completed #%s#\n", cliParserGetLastOp()->word_completed);
	
	// check debug mode
	if (strncmp(cliParserGetLastOp()->word_completed, "debug", 5) == 0 && cli_dbg == 0) {
		printf("Error: the feature is available only in debug mode\n");
		return 0;
	}
	
	if (cliParserGetLastOp()->last_node_match != NULL) {
		rcpDebug("cli: last match %s\n", cliParserGetLastOp()->last_node_match->token);
		rcpDebug("cli: depth %d\n", cliParserGetLastOp()->depth);
	}
	else if (csession.cmode != CLIMODE_CONFIG &&
	            csession.cmode != CLIMODE_LOGIN &&
	            csession.cmode != CLIMODE_NONE &&
	            csession.cmode != CLIMODE_ENABLE) {
	            	// retry the command in a different mode
		CliMode ctmp = csession.cmode;
		csession.cmode = cliPreviousMode(csession.cmode); // the hierarchy has one level hardcoded!!!
		rcpDebug("cli: existing mode %llx, try new cmode %llx\n", ctmp, csession.cmode);
		preserve_input =  cli_process_cmd(cli_line_buffer, prompt_len, 0);
		rcpDebug("cli: cli_process_cmd return value %d in new mode, last_cmd_ok %d\n", preserve_input, last_cmd_ok);
		if (last_cmd_ok == 0)
			csession.cmode = ctmp;
		return preserve_input;			
	}
	
	cliSetHistory(cliParserGetLastOp()->word_completed);

	if (n == NULL) {
		char *m = cliParserGetLastOp()->word_multimatch;
		if (*m != '\0')
			printf("  %s\n", m);
		else {
			// jump over cliParserGetLastOp()->depth words
			int cnt = 0;
			int position = 0;
			char *ptr = cli_line_buffer;
			while (*ptr == ' ')
				ptr++;
			while (*ptr != '\0') {
				int incremented = 0;
				if (*ptr == ' ') {
					cnt++;
					while (*ptr == ' ') {
						incremented = 1;
						ptr++;
						position++;
					}
				}
				if (cnt == cliParserGetLastOp()->depth) {
					break;
				}
				if (incremented == 0) {
					ptr++;
					position++;
				}
			}
			*ptr = '\0';
			if (noerr == 0) {
				int i;
				for (i = 0; i < prompt_len + position; i++)
					printf(" ");
				printf("^\n");
				if (cliParserGetLastOp()->error_string == NULL)
					printf("Error: command not found\n");
				else
					printf("Error: %s\n", cliParserGetLastOp()->error_string);
			}
		}
	}
	else {
		char cli_out_mods[CLI_MAX_LINE + 1];
		*cli_out_mods = '\0';
		// try to run the command again, this time without output modifiers (if any)
		char *outstart = strstr(cli_line_buffer, "|");
		if (outstart != NULL) {
			// remove output modifiers from the original string
			strcpy(cli_out_mods, outstart);
			*outstart = '\0';
			n = cliParserFindCmd(csession.cmode, head, cli_line_buffer);
			ASSERT(n != NULL);
			cliSetOutMods(cli_out_mods);
		}
		else
			cliSetOutMods(NULL);

		rcpDebug("n->exe %d\n", n->exe);			
		if (n->exe && (csession.cmode & n->exe_mode)) {
			last_cmd_ok = 1;
			cliCommitHistory();
			
			if (n->question != NULL) {
				if (ask_question(n->question)) {
					cliResetOutMods();
					return 0;
				}
			}

			int rv = cliExecNode(csession.cmode, n, cliParserGetLastOp()->word_completed);
			rcpDebug("rv %d, n->exe %d\n", rv, n->exe);			
			if (rv == RCPERR_FUNCTION_NOT_FOUND) {
				uint32_t owner = n->owner;
				// handle commands with multiple owners (i.e. redistribute commands in rip and ospf)
				if (n->exe > 1 || owner == 0 || owner == (RCP_PROC_OSPF | RCP_PROC_RIP)) {
					rcpDebug("changing owner %llu to something else\n", owner);			
					if (csession.cmode == CLIMODE_OSPF)
						owner = RCP_PROC_OSPF;
					else if (csession.cmode == CLIMODE_RIP)
						owner = RCP_PROC_RIP;
					else if (csession.cmode == CLIMODE_DHCP_SERVER)
						owner = RCP_PROC_DHCP;
				}
				ASSERT(owner != 0);				
				rcpDebug("owner %llu\n", owner);			
				cliSendMux(csession.cmode, cliParserGetLastOp()->word_completed, owner, n);
			}
			else if (rv != 0)
				ASSERT(0);
				
			cliResetOutMods();
		}
		else {
			strcpy(cli_line_buffer, cliParserGetLastOp()->word_completed);
			preserve_input = 1;
		}
	}
	return preserve_input;
}

static int is_comment(const char *str) {
	ASSERT(str != NULL);
	const char *ptr = str;

	while (*ptr == ' ' || *ptr == '\t')
		ptr++;
		
	if (*ptr == '!')
		return 1;
	return 0;
}

void cliMainLoop(void) {
	char cli_line_buffer[CLI_MAX_LINE + 1];
	int  preserve_input = 0;

	while (1) {
		if (preserve_input == 0)
			memset(cli_line_buffer, 0 , sizeof( cli_line_buffer));
		preserve_input = 0;

		int prompt_len = cliPrintPrompt();
		printf("%s", cli_line_buffer);
		fflush(0);

		// get user input
		int rv = cliGets(cli_line_buffer,  sizeof( cli_line_buffer));;
		printf("\n");fflush(0);
		rcpDebug("cli: cliGets rv = %d, buf len %d, #%s#\n", rv, strlen(cli_line_buffer), cli_line_buffer);

		// hidden commands for debug mode
		if (strcmp(cli_line_buffer, "dbg") == 0) {
			cli_dbg = 1;
			continue;
		}
		if (strcmp(cli_line_buffer, "no dbg") == 0) {
			cli_dbg = 0;
			continue;
		}

		// process tab
		if (rv == GETS_TAB)
			preserve_input = cli_process_tab(cli_line_buffer);
		
		// process question
		else if (rv == GETS_QUESTION)
			preserve_input = cli_process_question(cli_line_buffer);

		// process enter
		else if (rv == GETS_OK) {
			if (is_comment(cli_line_buffer))
				continue;
			preserve_input = cli_process_cmd(cli_line_buffer, prompt_len, 0);			
			rcpDebug("cli: cli_process_cmd return value %d\n", preserve_input);				
		}

		// process arrow up
		else if (rv == GETS_ARROW_UP) {
			const char *newcmd = cliGetHistoryArrow(1);
			strcpy(cli_line_buffer, newcmd);
			preserve_input = 1;
		}

		// process arrow down
		else if (rv == GETS_ARROW_DOWN) {
			const char *newcmd = cliGetHistoryArrow(0);
			strcpy(cli_line_buffer, newcmd);
			preserve_input = 1;
		}

		else
			ASSERT(0);
	}
}
