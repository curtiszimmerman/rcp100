#include "cmdparser.h"
#include <assert.h>


// output buffer
char outbuf[MAX_LINE + 1];

// main parser function
static void parse(int start, const char *function, int lineno);


static void print_cmd(char *cmd, const char *function) {
	char outprint[MAX_LINE + 1];
	memset(outprint, 0, MAX_LINE + 1);
	
	// first copy the command in outprint
	strncpy(outprint, cmd, MAX_LINE);
	// remove blanks at the end of the string
	char *ptr = outprint + strlen(outprint);
	ptr--;
	while (*ptr == ' ') {
		*ptr = '\0';
		ptr--;
	}
	
	printf("\tif (cliParserAddCmd(mode, head, \"%s\")) return RCPERR;\n", cmd);
	printf("\tif ((node = cliParserFindCmd(mode, head, \"%s\")) == NULL) return RCPERR;\n", cmd);
	printf("\tif (cliAddFunction(mode, node, %s)) return RCPERR;\n\n", function);
}

static int parse_block(int start, const char *function, int lineno) {
	char *o = outbuf + strlen(outbuf);
	int n = start;
	while (*targv[n] != ']') {
		if (*targv[n] == '\0') {
			printf("Error: unmatched [ on line  %d\n", lineno);
			exit(1);
		}
		n++;
	}
	n++;
	parse(n, function, lineno);
	*o = '\0';
	start++;
	if (targv[start] == NULL) {
		printf("Error: unmatched [ on line %d\n", lineno);
		exit(1);
	}
	return start;
}


static void parse(int start, const char *function, int lineno) {
	// recursivity in progress!
	if (start == 0)
		outbuf[0] = '\0';
	if (targv[start] == NULL)
		return print_cmd(outbuf, function);
	
	// process [
	if (*targv[start] == '[')
		start = parse_block(start, function, lineno);

	// skeep ]
	if (*targv[start] == ']')
		start++;

	if (targv[start] == NULL)
		return print_cmd(outbuf, function);
	
	if (*targv[start] == '[')
		return parse(start, function, lineno);

	
	strcat(outbuf, targv[start]);
	strcat(outbuf, " ");
	parse(start + 1, function, lineno);
}

static void expand_cmd(char *cmd, char *function, int lineno) {
	// add spaces around [ and ]
	char *ptr = cmd;
	int cnt1 = 0;
	int cnt2 = 0;
	int added = 0;
	
	while (*ptr != '\0') {
		if (*ptr == '[') {
			cnt1++;
			added += 2;
		}
		else if (*ptr == ']') {
			cnt2++;
			added += 2;
		}
		ptr++;
	}
	
	if (cnt1 != cnt2) {
		fprintf(stderr, "Error: invalid command on line %d, mismatched parenthesis - %s\n", lineno, cmd);
		exit(1);
	}
	if (cnt1 == 0 && cnt2 == 0)
		return print_cmd(cmd, function);
		
	// allocate a new command string
	char *newcmd = malloc(strlen(cmd) + 1 + added);
	if (newcmd == NULL) {
		fprintf(stderr, "Error: cannot allocate memory\n");
		exit(1);
	}
	memset(newcmd, 0, strlen(cmd) + 1 + added);
	
	ptr = cmd;
	char *ptr2 = newcmd;
	while (*ptr != '\0') {
		if (*ptr == '[' || *ptr == ']') {
			*ptr2++ = ' ';
			*ptr2++ = *ptr++;
			*ptr2++ = ' ';
		}
		else
			*ptr2++ = *ptr++;
	}
	
	// split the command into tokens
	tokenize(newcmd);
	
	// process the arguments
	parse(0, function, lineno);
	
	// free the memory
	free(newcmd);
	
}


//************************************************************
// line type
//************************************************************
void do_mode(char *start, int lineno) {
	if (*start =='\0') {
		printf("Error: invalid line %d\n", lineno);
		exit(1);
	}
	printf("\n\tmode = %s;\n", start);
}

void do_script(char *start, int lineno) {
	if (*start =='\0') {
		printf("Error: invalid line %d\n", lineno);
		exit(1);
	}
	printf("\n\t%s\n", start);
}


void do_command(char *start, int lineno) {
	if (*start =='\0') {
		printf("Error: invalid line %d\n", lineno);
		exit(1);
	}
	char *ptr = strchr(start, ':');
	if (ptr == NULL || *(ptr + 1) == '\0') {
		printf("Error: invalid line %d - %s\n", lineno, start);
		exit(1);
	}
	*ptr = '\0';
	ptr++;

	printf("\t//Command: %s\n", start);
	printf("\textern int %s(CliMode mode, int argc, char **argv);\n", ptr);
	expand_cmd(start, ptr, lineno);
	printf("\n");
}

void do_question(char *start, int lineno) {
	if (*start =='\0') {
		printf("Error: invalid line %d\n", lineno);
		exit(1);
	}
	
	printf("\tif (cliAddQuestion(node, \"%s\")) return RCPERR;\n", start);
	printf("\n");
}

void do_help(char *start, int lineno) {
	char *ptr = strchr(start, ':');
	if (ptr == NULL || *(ptr + 1) == '\0') {
		printf("Error: invalid line %d - %s\n", lineno, start);
		exit(1);
	}
	*ptr = '\0';
	ptr++;
	printf("\tif (cliParserAddHelp(head, \"%s\",  \"%s\")) return RCPERR;\n", start, ptr);
}
