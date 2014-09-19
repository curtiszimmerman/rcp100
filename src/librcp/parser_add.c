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
#include "smem.h"

static unsigned owner = 0;
static RcpShm *shm = NULL;
static CliNode *create_node(CliMode mode);
extern const char* cliHelpSpecial(const char*token);

unsigned rcpGetProcId(void) {
	return owner;
}


// return NULL if error
RcpShm *rcpShmemInit(uint32_t process) {
	// set the process name
	rcpGetProcName();
	
	int already = 0;
	owner = process;

	// allocate 1MB of memory
	unsigned char *ptr = rcpShmOpen(RCP_SHMEM_NAME, RCP_SHMEM_SIZE, RCP_SHMEM_START, &already);
	if (ptr == NULL) {
		ASSERT(0);
		return NULL;
	}
	
	shm = (RcpShm *) ptr;
	if (already) {
		rcpDebug("shm: file already present\n");
	}
	else {
		memset(shm, 0, sizeof(RcpShm));
		
		// shm version
		// version 1 - up to 0.98.3
		// versoin 2 - from 0.98.4
		shm->config.version = 2;
		
		// interface defaults
		int i;
		for (i = 0; i < RCP_INTERFACE_LIMITS; i++)
			rcpInterfaceSetDefaults(&shm->config.intf[i]);
		
		// hostname default
		strcpy(shm->config.hostname, "rcp");

		// exec timeout default
		shm->config.timeout_minutes = CLI_DEFAULT_TIMEOUT;
		// services
		shm->config.sec_port = RCP_DEFAULT_SSH_PORT;
		shm->config.telnet_port = RCP_DEFAULT_TELNET_PORT;
		shm->config.http_port = RCP_DEFAULT_HTTP_PORT;
		
		// network monitor
		shm->config.monitor_interval =  RCP_DEFAULT_MON_INTERVAL;
		
		// logger defaults
		shm->config.log_level = RLOG_LEVEL_DEFAULT;
		shm->config.facility = RLOG_FC_DEFAULT;
		shm->config.rate_limit = RLOG_RATE_DEFAULT;
		
		// rip defaults
		shm->config.rip_update_timer = RCP_RIP_DEFAULT_TIMER;
		
		// dhcp defaults
		shm->config.dhcpr_max_hops = RCP_DHCPR_DEFAULT_MAX_HOPS;
		shm->config.dhcpr_service_delay =  RCP_DHCPR_DEFAULT_SERVICE_DELAY;
		
		// ospf defaults
		shm->config.ospf_spf_delay = RCP_OSPF_SPF_DELAY_DEFAULT;
		shm->config.ospf_spf_hold = RCP_OSPF_SPF_HOLD_DEFAULT;
		shm->config.ospf_connected_metric_type = RCP_OSPF_CONNECTED_METRIC_TYPE_DEFAULT;
		shm->config.ospf_static_metric_type = RCP_OSPF_STATIC_METRIC_TYPE_DEFAULT;
		shm->config.ospf_rip_metric_type = RCP_OSPF_RIP_METRIC_TYPE_DEFAULT;
		shm->config.ospf_discard_external = 1;
		shm->config.ospf_discard_internal = 1;
		
		// memory monitoring using canary
		shm->stats.can1 =  RCP_CANARY;
		shm->stats.can2 =  RCP_CANARY;
		shm->stats.can3 =  RCP_CANARY;
		shm->stats.can4 =  RCP_CANARY;
		shm->stats.can5 =  RCP_CANARY;
		shm->stats.can6 =  RCP_CANARY;
				
		// initialize rand, used for salting passwords
		srand(time(NULL));
		shm->config.configured = 1;
		
		// initialize memory allocators
		unsigned char *startmem = ptr + sizeof(RcpShm);

		// init cli	
		shm->cli_smem_handle = smem_initialize(startmem, RCP_SHMEM_SIZE_CLI);
		rcpDebug("shm: cli_shmem_handle 0x%x\n", shm->cli_smem_handle);
		shm->cli_head = create_node(CLIMODE_ALL);
		startmem += RCP_SHMEM_SIZE_CLI;
		
		// init logger
		shm->log_smem_handle = smem_initialize(startmem, RCP_SHMEM_SIZE_LOG);
		rcpDebug("shm: log_shmem_handle 0x%x\n", shm->log_smem_handle);
		startmem += RCP_SHMEM_SIZE_LOG;
	}
	
	return shm;
}

RcpShm *rcpGetShm(void) {
	return shm;
}

int rcpRipRedistStatic(void) {
	if (shm == NULL)
		return 0;
	if (shm->config.rip_redist_static != 0)
		return 1;
	return 0;
}

int rcpRipRedistOspf(void) {
	if (shm == NULL)
		return 0;
	if (shm->config.rip_redist_ospf != 0)
		return 1;
	return 0;
}

int rcpOspfRedistRip(void) {
	if (shm == NULL)
		return 0;
	if (shm->config.ospf_redist_rip != 0)
		return 1;
	return 0;
}

int rcpOspfRedistStatic(void) {
	if (shm == NULL)
		return 0;
	if (shm->config.ospf_redist_static != 0)
		return 1;
	return 0;
}


static CliNode *create_node(CliMode mode) {
	ASSERT(shm->cli_smem_handle != 0);	
	
	CliNode *rv = rcpAlloc(shm->cli_smem_handle, sizeof(CliNode));
	if (rv == NULL)
		return NULL;
	memset(rv, 0, sizeof(CliNode));
	rv->mode = mode;
	rv->owner = owner;
	return rv;
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


// look for a token at the current tree level - exact match only
// return NULL if word not found
static CliNode *find_token(CliMode mode, CliNode *node, const char *token) {
	ASSERT(token != NULL);
	ASSERT(node != NULL);

	// try an exact match first		
	while (node != NULL) {
		ASSERT(node->token != NULL);
		if (strcmp(token, node->token) == 0 && (node->mode & mode)) {
			rcpDebug("parser: exact match, completed #%s#\n", token);
			return node;
		}
		node = node->next;
	}
	
	return NULL;
}


static char opword[CLI_MAX_LINE];
static int prnt_internal = 0;

// return 0 if all ok or error as follow:
// 1 - memory allocation failed
static int cliParserAddCmd_internal(CliMode mode, CliNode *current, const char *str) {
	ASSERT(str != NULL);
	ASSERT(current != NULL);
//printf("current->mode %llx, current->token %s, mode %llx, str %s\n", current->mode, current->token, mode, str);	

	// extract the word
	const char *ptr = str;
	while (*ptr == ' ')
		ptr++;
	int len = get_word_len(ptr);
	if (len == 0) {
		return 0; // all fine, we reached the end of the command
	}
	memcpy(opword, ptr, len);
	opword[len] = '\0';

	if (current->token == NULL) {
		// allocate token
		current->token = rcpAlloc(shm->cli_smem_handle, len + 1);
		if (current->token == NULL)
			return 1;
		strcpy(current->token, opword);
		
		return cliParserAddCmd_internal(mode, current, ptr);
	}

		

	// walk trough the next list and see if the word is already there
	CliNode *found = find_token(CLIMODE_ALL, current, opword);

	// if found, just follow the node
	if (found != NULL) {
		// is it necessary to  allocate a new node?
		found->mode |= mode;
				
		ptr += len;
		while (*ptr == ' ')
			ptr++;
		if (*ptr == '\0') {
//			if (found->owner != owner && found->exe) {
//				printf("************************************\n");	
//				printf("different owner present for token %s\n", found->token);
//				printf("original process %s, this process %s\n", rcpProcName(found->owner), rcpProcName(owner));
//				printf("removing original process, installing new process\n");
//				prnt_internal = 1;
//			}
			found->owner |= owner;
			found->exe++;
			found->exe_mode |= mode;
			return 0;
		}
		// follow the rule pointer
		if (found->rule == NULL) {
			found->rule = create_node(mode);
			if (found->rule == NULL)
				return 1;
		}
			
		return cliParserAddCmd_internal(mode, found->rule, ptr);
	}
	
	// else create a new node and stick it at the end of next list
	CliNode *newnode = create_node(mode);
	if (newnode == NULL)
		return 1;

	// allocate token
	newnode->token = rcpAlloc(shm->cli_smem_handle, len + 1);
	if (newnode->token == NULL)
		return 1;
	strcpy(newnode->token, opword);
	// advance to the end of the list
	while (current->next != NULL)
		current = current->next;
	current->next = newnode;
	
	// retry	
	return cliParserAddCmd_internal(mode, newnode, ptr);
}

int cli_alloc_modifs(void) {
	CliMode mode = CLIMODE_ALL;
	ASSERT(shm->cli_om == NULL);
	char *otoken = rcpAlloc(shm->cli_smem_handle, 2);
	if (otoken == NULL)
		return RCPERR_MALLOC;
	shm->cli_om = create_node(mode);
	if (shm->cli_om == NULL)
		return RCPERR_MALLOC;
		
	otoken[0] = '|';
	otoken[1] = '\0';
	shm->cli_om->token = otoken;
	int rv = cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	if (rv != 0)
		return rv;
	
	// count
	rv = cliParserAddCmd_internal(mode, shm->cli_om, "| count");
	rv |= cliParserAddHelp(shm->cli_om, "count", "Count the lines");

	// file
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| file <file>");
	rv |= cliParserAddHelp(shm->cli_om, "file", "Save the output to a file");
	rv |= cliParserAddHelp(shm->cli_om, "<file>", "File name");
	
	// no-more
	rv = cliParserAddCmd_internal(mode, shm->cli_om, "| no-more");
	rv |= cliParserAddHelp(shm->cli_om, "no-more", "Do not paginate output");

	// include
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| include <word>");
	rv |= cliParserAddHelp(shm->cli_om, "include", "Include lines that match");
	rv |= cliParserAddHelp(shm->cli_om, "<word>", "Word to match");
	
	// exclude
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| exclude <word>");
	rv |= cliParserAddHelp(shm->cli_om, "exclude", "Exclude lines that match");
	rv |= cliParserAddHelp(shm->cli_om, "<word>", "Word to match");
	
	//************
	// include 1
	//************
	// include | count
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | count");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "count", "Count the lines");

	// include | file
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | file <file>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "file", "Save the output to a file");
	rv |= cliParserAddHelp(shm->cli_om, "<file>", "File name");
	
	// include | no-more
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | no-more");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "no-more", "Do not paginate output");

	// include / exclude
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| include <word> | exclude <word>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "exclude", "Exclude lines that match");
	rv |= cliParserAddHelp(shm->cli_om, "<word>", "Word to match");

	// include / exclude / count
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| include <word> | exclude <word> | count");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "count", "Count the lines");

	// include | exclude / file
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | exclude <word> | file <file>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "file", "Save the output to a file");
	rv |= cliParserAddHelp(shm->cli_om, "<file>", "File name");
	
	// include | exclude / no-more
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | exclude <word> | no-more");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "no-more", "Do not paginate output");

	//************
	// include 2
	//************
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| include <word> | include <word>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "include", "Include lines that match");
	rv |= cliParserAddHelp(shm->cli_om, "<word>", "Word to match");
	
	// include | include | count
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | include <word> | count");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "count", "Count the lines");

	// include | include | file
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | include <word> | file <file>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "file", "Save the output to a file");
	rv |= cliParserAddHelp(shm->cli_om, "<file>", "File name");
	
	// include | include | no-more
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | include <word> | no-more");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "no-more", "Do not paginate output");

	// include / include | exclude
	rv |= cliParserAddCmd_internal(mode, shm->cli_om,  "| include <word> | include <word> | exclude <word>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "exclude", "Exclude lines that match");
	rv |= cliParserAddHelp(shm->cli_om, "<word>", "Word to match");

	// include | include | exclude | count
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | include <word> | exclude <word> | count");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "count", "Count the lines");
	
	// include | include | exclude | file
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | include <word> | exclude <word> | file <file>");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "file", "Save the output to a file");
	rv |= cliParserAddHelp(shm->cli_om, "<file>", "File name");
	
	// include | include | exclude | no-more
	rv |= cliParserAddCmd_internal(mode, shm->cli_om, "| include <word> | include <word> | exclude <word> | no-more");
	rv |= cliParserAddHelp(shm->cli_om, "|", "Output modifiers");
	rv |= cliParserAddHelp(shm->cli_om, "no-more", "Do not paginate output");
	return rv;
}

int cliParserAddCmd(CliMode mode, CliNode *current, const char *str) {
	ASSERT(str != NULL);
	ASSERT(current != NULL);
	prnt_internal = 0;

	rcpDebug("parser: adding command \"%s\"\n", str);	
	int rv = cliParserAddCmd_internal(mode, current, str);
	if (rv != 0) {
		printf("Command \"%s\" was NOT added\n", str);	
		return rv;
	}
	rcpDebug("parser: Command \"%s\" was added\n", str);	
	if (prnt_internal) {
		printf("Command \"%s\"\n", str);		
		printf("************************************\n");	
	}
	return rv;
}

static void add_om(CliNode *node) {
	if (node->token) {
		if (node->exe) {
			if (node->rule == NULL)
				node->rule = shm->cli_om;
			else {
				CliNode *current = node->rule;
				while (current->next != NULL)
					current = current->next;
				current->next = shm->cli_om;
			}
		}
		if (strcmp(node->token, "|") == 0)
			return;
	}

	
	if (node->rule)
		add_om(node->rule);
	if (node->next) {
		add_om(node->next);
	}
}



int cliParserAddOM(void) {

	if (shm->cli_om != NULL)
		return 0;
		
	int rv;
	if ((rv = cli_alloc_modifs()) != 0)
		return rv;
		
	CliNode *node;
	CliMode mode = CLIMODE_ALL;
	if ((node = cliParserFindCmd(mode, shm->cli_head, "show")) == NULL) {
		ASSERT(0);
		return RCPERR;
	}
	
	add_om(node->rule);

//	while (node != NULL) {
//		node = node->next;
//	}

	return 0;
}

// return 0 if all ok or error as follow:
// 1 - memory allocation failed
int cliParserAddHelp(CliNode *current, const char *token, const char *str) {
	ASSERT(str != NULL);
	ASSERT(current != NULL);
	ASSERT(token != NULL);
	int rv = 0;

	if (current->help == NULL &&
	      strcmp(current->token, token) == 0) {
		char *help = rcpAlloc(shm->cli_smem_handle, strlen(str) + 1);
		if (help == NULL)
			return RCPERR_MALLOC;
		strcpy(help, str);
		current->help = help;
		rcpDebug("parser: set node %p help %s\n", current, str);

		// if this is a special token, add the specific help
		if (*current->token == '<') {
			const char *newhelp =  cliHelpSpecial(token);
			if (newhelp == NULL)
				newhelp = str;
			help = rcpAlloc(shm->cli_smem_handle, strlen(newhelp) + 1);
			if (help == NULL)
				return RCPERR;

			strcpy(help, newhelp); // str
			current->help_special = help;
		}
	}

	if (current->rule != NULL)
		rv =  cliParserAddHelp(current->rule, token, str);
	if (rv != 0)
		return rv;
	
	if (current->next != NULL)
		rv =  cliParserAddHelp(current->next, token, str);
	return rv;
}
	
int cliAddQuestion(CliNode *node, const char *str) {
	ASSERT(node != NULL);
	ASSERT(str != NULL);
	
	// after a crash, when the process is restarted, a question could be already there
	// free the memory for the question and install the new one
	if (node->question)
		rcpFree(shm->cli_smem_handle, node->question);
	node->question = NULL;
	
	node->question = rcpAlloc(shm->cli_smem_handle, strlen(str) + 1);
	if (node->question == NULL) {
		fprintf(stderr, "Error: cannot allocate shared memory\n");
		return RCPERR;
	}
	strcpy(node->question, str);
	return 0;
}
	
