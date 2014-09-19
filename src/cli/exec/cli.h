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
#ifndef CLI_H
#define CLI_H
#include "librcp.h"

// cli session data
typedef struct {
	// mode
	CliMode cmode;
	char *mode_data;
	
	// login user
	char *cli_user;
	char *cli_user_ip;
	
	// terminal
	int term_row;	// default 24
	int term_col;	// default 80
	
	uint8_t html;		// the session is in use by http server
} CliSession;

// main.c
extern CliSession csession;
extern CliNode *head;
extern RcpShm *shm;
extern int cli_no_more;

// mux.c
extern int muxsocket;
void cliReceiveMux(void);
void cliSendMux(CliMode mode, const char *cmd, unsigned owner, CliNode *node);
int cliGetMuxSocket(void);


// history.h
const char *cliGetHistoryArrow(int up);
void cliSetHistory(const char *cmd);
void cliCommitHistory(void);

// prompt.c
int cliPrintPrompt(void);

// outmods.c
void cliSetOutMods(char *str);
void cliResetOutMods(void);
void cliPrintLine(const char *fmt, ...);
void cliPrintBuf(char *str);
void cliPrintFile(const char *fname);

// help.c
void cliStoreHelp(CliNode *node);
void cliPrintHelp(void);

// terminal.c
void cliSetTerminal(void);
void cliRestoreTerminal(void);
void cliUpdateTerminalSize(void);
extern int term_row;
extern int term_col;

// gets.c
#define GETS_OK 0
#define GETS_ARROW_UP 1
#define GETS_ARROW_DOWN 2
#define GETS_QUESTION 3
#define GETS_TAB 4
int cliGets(char *buf, int size);

// main_loop.c
void cliMainLoop(void);
int cli_process_cmd(char *cli_line_buffer, int prompt_len, int noerr);

// runscript.c
int cliRunScript(const char *fname);

// cmds.c - generated at build time
extern int module_initialize_commands(CliNode *head);

// timeout.c
void cliTimeout(void);
int cliTimeoutGetc(void);

// cmds.c
int module_initialize_commands(CliNode *head);

#endif
