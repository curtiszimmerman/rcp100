#include "cmdparser.h"

// line tokenizer
int targc = 0;
char *targv[MAX_TOKENS_PER_LINE + 1];

// split a command into tokens
void tokenize(char *msg) {
	targc = 0;
	memset(targv, 0, sizeof(targv));
	char *ptr = msg;
	int i = 0;

	for (i = 0; i <MAX_TOKENS_PER_LINE; i++) {
		// skeep spaces
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;
		if (*ptr == '\0') {
			i--;
			break;
		}
		
		// set the argument
		targv[i] = ptr;

		// go to the end of the word
		while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t')
			ptr++;

		// exit the loop at the end of the string
		if (*ptr == '\0')
			break;
		*ptr = '\0';
		ptr++;
//printf("argument #%s#\n", targv[i]);		
	}

	// set the number of arguments
	targc = i + 1;
}
