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

#define OUTPUT_CHUNK 4096	// allocate memory in 4k chunks

static char *reallocate_chunk(char *mem) {
	if (mem == NULL) {
		char *newmem = malloc(OUTPUT_CHUNK + 1);
		if (newmem == NULL)
			return NULL;
		memset(newmem, 0, OUTPUT_CHUNK + 1);
		return newmem;
	}

	int newsize = strlen(mem) + OUTPUT_CHUNK + 1;
	char *newmem = (char *) malloc(newsize);
	if (newmem == NULL) {
		free(mem);
		return NULL;
	}
	memset(newmem, 0, newsize);
	strcpy(newmem, mem);
	free(mem);
	return newmem;
}

char *cliRunProgram(const char *prog) {
	FILE *fp;
	// open the program trough a pipe
	fp = popen(prog, "r");
	if (fp == NULL) {
		ASSERT(0);
		return NULL;
	}

	// allocate the first chunk
	char *rv = reallocate_chunk(NULL);
	if (rv == NULL) {
		pclose(fp);
		ASSERT(0);
		return NULL;
	}

	// read pipe
	int len;
	size_t size = OUTPUT_CHUNK;
	char *ptr = rv;
	while ((len = fread(ptr, 1, size, fp)) > 0) {
		size -= len;
		if (size < 100) { // an arbitrary limit of left space to reallocate the output memory
			rv = reallocate_chunk(rv);
			if (rv == NULL) {
				pclose(fp);
				return NULL;
			}

			size = OUTPUT_CHUNK;
			ptr = rv + strlen(rv);
		}
	}

	pclose(fp);
	return rv;
}
