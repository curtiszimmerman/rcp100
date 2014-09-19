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
#include "librcp.h"

typedef struct exec_list_t
{
	int (*function)(CliMode mode, int argc, char **argv);
	CliNode *node;
	CliMode mode;
	struct exec_list_t *next;
} ExecList;

static ExecList *head = NULL;

static ExecList *find_node(CliMode mode, CliNode *node) {
	ExecList *ptr = head;
	while (ptr) {
		if (ptr->node == node && ptr->mode & mode)
			return ptr;
		ptr = ptr->next;
	}

	return NULL;
}


static void add2end(ExecList *e) {
	if (head == NULL) {
		head = e;
		return;
	}

	ExecList *ptr = head;
	while (ptr->next != NULL)
		ptr = ptr->next;

	ptr->next = e;
}


// return 0 if ok
int cliAddFunction(CliMode mode, CliNode *node,  int (*f)(CliMode mode, int argc, char **argv)) {
	ASSERT(node != NULL);
	ASSERT(f != NULL);
	if (node == NULL || f == NULL)
		return RCPERR;;

	// allocate a new list entry
	ExecList *e = malloc(sizeof(ExecList));
	if (e == NULL) {
		ASSERT(0);
		return RCPERR_MALLOC;
	}
	memset(e, 0, sizeof(ExecList));

	e->function = f;
	e->node = node;
	e->mode = mode;
	add2end(e);
	return 0;
}

void cliRemoveFunctions(void) {
	ExecList *ptr = head;
	while (ptr) {
		ExecList *next = ptr->next;
		free(ptr);
		ptr = next;
	}
	head = NULL;
}

static char cmdline[CLI_MAX_LINE + 1];
static char *cmdargv[CLI_MAX_ARGS + 1];		  // the last pointer is always NULL
static int cmdargc;

// execute a command
// arguments: the node and the command itself
// returns 0 if ok, RCPERR if the node is not found
//	- response_string will be populated with a pointer to allocated memory
//	or left NULL if error or no return text is necessary
int cliExecNode(CliMode mode, CliNode *node, const char *cmd) {
	// look for the node in the list
	ExecList *lst = find_node(mode, node);
	if (lst == NULL)
		return RCPERR_FUNCTION_NOT_FOUND;	// the node might not be in the local exec list, send the command to the multiplexer

	if (lst->function == NULL) // this command should go somewhere else
		return RCPERR_FUNCTION_NOT_FOUND;

	// make a local copy of the command and split it into arguments
	strcpy(cmdline, cmd);
	char *ptr = cmdline;
	for (cmdargc = 0; cmdargc < CLI_MAX_ARGS; cmdargc++) {
		// find the start of the word
		while (*ptr == ' ')
			ptr++;

		if (*ptr == '\0') {
			cmdargv[cmdargc] = NULL;
			break;
		}
		cmdargv[cmdargc] = ptr;

		// find the end of the word
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		if (*ptr == '\0') {
			*ptr++ = '\0';
			cmdargc++;
			cmdargv[cmdargc] = NULL;
			break;
		}
		*ptr++ = '\0';
	}
	cmdargv[CLI_MAX_ARGS] = NULL;

	if (lst->function == NULL) {
		ASSERT(0);
		return RCPERR_FUNCTION_NOT_FOUND;
	}
	lst->function(mode, cmdargc, cmdargv);

	return 0;
}
