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
#include <stdarg.h>

// config
static char *file_arg = NULL;
static char *include1_arg = NULL;
static char *include2_arg = NULL;
static char *exclude_arg = NULL;
static int no_more_flag = 0;
static int count_flag = 0;
// status
static int more_count = 0;
static int count_count = 0;
static int print_killed = 0;
static FILE *file_fp = NULL;
static int file_err_printed = 0;

static char *args[CLI_MAX_ARGS]; 

static void reset_locals(void) {
	count_flag = 0;

	// force no more flag if the remote telnet client doesn't negotiated a proper terminal
	if (csession.term_row == 0)
		no_more_flag = 1;
	else
		no_more_flag = 0;

	file_arg = NULL;
	include1_arg = NULL;
	include2_arg = NULL;
	exclude_arg = NULL;
	more_count = 0;
	count_count = 0;
	print_killed = 0;
	
	if (file_fp != NULL) {
		fclose(file_fp);
	}
	file_fp = NULL;
	file_err_printed = 0;
}


static int split_args(char *str) {
	char *ptr = str;
	int argc = 0;
	while (argc < CLI_MAX_ARGS) {
		// skip empty space
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		if (*ptr == '\0')
			return argc;
		args[argc++] = ptr;
		
		// advance to the end of the word
		while (*ptr != ' ' && *ptr != '\t' && *ptr != '\0')
			ptr++;
		if (*ptr == '\0')
			return argc;
		*ptr++ = '\0';
	}
	
	return argc;
}

void cliSetOutMods(char *str) {
	if (str == NULL) {
		reset_locals();
		rcpDebug("cli: No output modifiers\n");
	}
	else {
		// split the string into args
		int state = 0;
		int argc = split_args(str);
		int i = 0;
		for (i = 0; i < argc; i++) {
			switch (state) {
				case 0:
					if (strcmp(args[i], "|") == 0)
						state = 1;
					break;
				case 1:
					if (strncmp("count", args[i], strlen(args[i])) == 0) {
						count_flag = 1;
						state = 0;
					}
					else if (strncmp("no-more", args[i], strlen(args[i])) == 0) {
						no_more_flag = 1;
						state = 0;
					}
					else if (strncmp("include", args[i], strlen(args[i])) == 0) {
						state = 2;
					}							
					else if (strncmp("exclude", args[i], strlen(args[i])) == 0) {
						state = 4;
					}							
					else if (strncmp("file", args[i], strlen(args[i])) == 0) {
						state = 3;
					}
					break;
				case 2:
					if (include1_arg == NULL)
						include1_arg = args[i];
					else
						include2_arg = args[i];
					state = 0;
					break;
				case 3:
					file_arg = args[i];
					state = 0;
					break;
				case 4:
					exclude_arg = args[i];
					state = 0;
					break;
				default:
					ASSERT(0);
			}
														
		}
		rcpDebug("cli: Outut modifiers: count %s, more %s, file %s, include1 %s, include2 %s, exclude %s\n",
			(count_flag)? "yes": "no",
			(no_more_flag)? "no": "yes",
			(file_arg)? file_arg: "none",
			(include1_arg)? include1_arg: "none",
			(include2_arg)? include2_arg: "none",
			(exclude_arg)? exclude_arg: "none");
	}
}


static inline void print_nomore(const char *str) {
	printf("%s\n", str);
	fflush(0);
}

static char clear_line[4] = { 0x1b, 0x5b, 'K', 0};
static char cursor_back[4] = { 0x1b, 0x5b, 'D', 0};
static void print_more(const char *str) {
	if (print_killed)
		return;
	
	if (++more_count >= (csession.term_row - 1)) {
		more_count = 0;
		
		char *msg = "--More--";
		int len = strlen(msg);
		printf("%s", msg);
		fflush(0);
		
		int c = cliTimeoutGetc();
		if (c == 'q') {
			print_killed = 1;
		}
		
		int i;
		for (i = 0; i < len; i++)
			printf("%s", cursor_back);
		printf("%s", clear_line);
		fflush(0);
	}
	print_nomore(str);
}

// print multiple lines
void cliPrintBuf(char *str) {
	if (str == NULL) {
		ASSERT(0);
		return;
	}
	
	if (str[0] != '\0') {
		// split the data into lines and print them
		char *start = str;
		char *ptr = str;

		while ((ptr = strchr(start, '\n')) != NULL) {
			*ptr = '\0';
			cliPrintLine(start);
			ptr++;
			start = ptr;
			if (*start == '\0')
				break;
		}
		
		cliPrintLine(start);
	}
}

// print multiple lines
#define FILE_LINE_BUF 1024
void cliPrintFile(const char *fname) {
	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		cliPrintLine("Error: no information available\n");
		return;
	}
	
	char line[FILE_LINE_BUF + 1];
	while (fgets(line, FILE_LINE_BUF, fp) != NULL)
		cliPrintLine(line);
	
	fclose(fp);
}

// print a line on CLI screen; output modifiers have been removed
// \n is removed if present in the string
// in more mode, the line is trimmed to the size of the terminal
// in no-more mode, the full line is printed; if larger than the terminal, the line will wrap
void cliPrintLine(const char *fmt, ...) {
	char line[CLI_MAX_LINE + 10];
	memset(line, 0, CLI_MAX_LINE + 10);
		
	// prepare the line
	va_list a;
	va_start (a,fmt);
	vsnprintf(line, CLI_MAX_LINE, fmt, a);
	va_end (a);
	
	// remove the \n, cut anything coming after it
	char *ptr = strchr(line, '\n');
	if (ptr != NULL) {
		*ptr = '\0';
	}

	// check include1 command
	if (include1_arg != NULL)  {
		if (strstr(line, include1_arg) == NULL)
			return;
	}
	
	// check include2 command
	if (include2_arg != NULL)  {
		if (strstr(line, include2_arg) == NULL)
			return;
	}
	
	// check exclude command
	if (exclude_arg != NULL)  {
		if (strstr(line, exclude_arg) != NULL)
			return;
	}
	
	// check for count
	if (count_flag) {
		count_count++;
		return;
	}
	
	// check for file
	if (file_arg != NULL) {
		if (file_fp == NULL && file_err_printed == 0) {
			file_fp = fopen(file_arg, "w");
			if (file_fp == NULL && file_err_printed == 0) {
				printf("Error: cannot open file %s\n", file_arg);
				file_err_printed = 1;
			}
		}
		if (file_fp != NULL) {
			fprintf(file_fp, "%s\n", line);
		}
		return;	
	}
	
	// update the terminal sizes
	cliUpdateTerminalSize();

	// printing
	if (no_more_flag || cli_no_more)
		print_nomore(line);
	else {
		// trim the line to the size of the terminal
		line[csession.term_col] = '\0';
		print_more(line);
	}
	fflush(0);
}

// call this function to finish the printing
void cliResetOutMods(void) {
	if (count_count)
		printf("count %d\n", count_count);
		
	reset_locals();
}
