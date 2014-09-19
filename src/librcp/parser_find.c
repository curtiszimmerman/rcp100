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

extern int cliCheckSpecial(const char*token, const char*str, CliParserOp *op);

static CliParserOp op;


CliParserOp *cliParserGetLastOp(void) {
	return &op;
}


void cliParserPrintNodeTree(int level, CliNode *node) {
	if (node->token) {
		// retrieve the owner or the node
		const char *owner = rcpProcName(node->owner);
		
		// ... and print it
		int i;
		for (i = 0; i < level; i++)
			printf("   ");
		printf("%p:%s:%s:%s:%llx:%s\n", node, node->token, (node->exe)? "E": "", owner, node->mode, node->help);
		if (strcmp(node->token, "|") == 0)
			return;
	}

//	if (node->rule == NULL)
//		printf("\n");
	
	if (node->rule)
		cliParserPrintNodeTree(level + 1, node->rule);
	if (node->next) {
		cliParserPrintNodeTree(level, node->next);
	}
}


// get the length of the next word in the string
static int get_word_len(const char *str) {
	ASSERT(str != NULL);
	if (*str == '\0')
		return 0;
	int rv = 0;
		
	const char *ptr = str;
	while (*ptr != ' ' && *ptr != '\0') {
		ptr++;
		rv++;
	}
	
	return rv;
}

// look for a token at the current tree level
// return NULL if word not found
static CliNode *find_token(CliMode mode, CliNode *node, const char *token) {
	ASSERT(token != NULL);
	ASSERT(node != NULL);
	CliNode *store = node;

	// try an exact match first in regular tokens		
	while (node != NULL) {
		ASSERT(node->token != NULL);
		
		if (*node->token != '<' && (node->mode & mode) && strcmp(token, node->token) == 0) {
			strcat(op.word_completed, node->token);
			strcat(op.word_completed, " ");
			rcpDebug("parser: exact match, completed #%s#\n", op.word_completed);
			return node;
		}
		node = node->next;
	}
	rcpDebug("parser: couldn't find an exact match, trying a special token match...\n");
	
	// try to match a special token
	node = store;
	while (node != NULL) {
		ASSERT(node->token != NULL);
		
		if (*node->token == '<' && (node->mode & mode) && cliCheckSpecial(node->token, token, &op) == 0 ) {
			strcat(op.word_completed, token);
			strcat(op.word_completed, " ");
			rcpDebug("parser: exact special token match, completed #%s#\n", op.word_completed);
			return node;
		}

		node = node->next;
	}
	rcpDebug("parser: couldn't find a special token match, trying a multimatch...\n");
	
	// count matches limited to the token length
	CliNode *lastfound = NULL;
	int matches = 0;
	int toklen = strlen(token);
	*op.word_multimatch = '\0';
	node = store;
	while (node != NULL) {
		ASSERT(node->token != NULL);
		if (*node->token != '<' && (node->mode & mode) && strncmp(token, node->token, toklen) == 0) {
			lastfound = node;
			matches++;
			strcat(op.word_multimatch, node->token);
			strcat(op.word_multimatch, " ");
			rcpDebug("parser: exact word multimatch #%s#\n", op.word_multimatch);
		}
		node = node->next;
	}
	// if there is a single length-limited match, return it
	if (matches == 1) {
		strcat(op.word_completed, lastfound->token);
		strcat(op.word_completed, " ");
		rcpDebug("parser: length-limited, completed #%s#\n", op.word_completed);
		
		return lastfound;
	}	
	rcpDebug("parser: cannot find token #%s#\n", token);
	
	return NULL;
}

CliNode *cliParserFindCmd_internal(CliMode mode, CliNode *current, const char *str) {
	ASSERT(str != NULL);
	ASSERT(current != NULL);

	*op.word_multimatch = '\0';
	rcpDebug("parser: reset multimatch\n");
	
	// extract the word and set the new value of the string
	const char *ptr = str;
	while (*ptr == ' ')
		ptr++;
	int len = get_word_len(ptr);
	if (len == 0)
		return 0; // all fine, we reached the end of the command
	memcpy(op.word, ptr, len);
	op.word[len] = '\0';

	// walk trough the next list and see if the word is already there
	CliNode *found = find_token(mode, current, op.word);
	if (found == NULL)
		return NULL;
	op.last_node_match = found;
	op.depth++;

	// the node was found, check if this was the last token
	ptr += len;
	while (*ptr == ' ')
		ptr++;
	if (*ptr == '\0')
		return found; // we are at the end of the token list
	
	// test next token
	if (found->rule == NULL)
		return NULL;
	return cliParserFindCmd_internal(mode, found->rule, ptr);
}

CliNode *cliParserFindCmd(CliMode mode, CliNode *current, const char *str) {
	ASSERT(str != NULL);
	ASSERT(current != NULL);

	rcpDebug("parser: reset completed, last match null\n");
	*op.word_completed = '\0';
	op.last_node_match = NULL;
	*op.word_multimatch = '\0';
	op.depth = 0;
	op.error_string = NULL;
	
	return cliParserFindCmd_internal(mode, current, str);
}

