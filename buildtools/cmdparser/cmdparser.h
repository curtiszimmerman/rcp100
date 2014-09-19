#ifndef CMDPARSER_H
#define CMDPARSER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// maximum number of tokens on a line
#define MAX_TOKENS_PER_LINE 512

// maximum length of a generated line
#define MAX_LINE 2048

extern int targc;
extern char *targv[MAX_TOKENS_PER_LINE + 1];
extern void tokenize(char *msg);

extern void do_mode(char *start, int lineno);
extern void do_command(char *start, int lineno);
extern void do_function(char *start, int lineno);
extern void do_help(char *start, int lineno);

#endif