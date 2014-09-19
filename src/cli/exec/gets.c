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

// ANSI esc sequences
static char save_cursor[4] = { 0x1b, 0x5b, 's', 0};
static char restore_cursor[4] = { 0x1b, 0x5b, 'u', 0};
static char clear_line[4] = { 0x1b, 0x5b, 'K', 0};
static char cursor_back[4] = { 0x1b, 0x5b, 'D', 0};
static char cursor_forward[4] = { 0x1b, 0x5b, 'C', 0};

//--------------------------
//Keyboard
//--------------------------
// arrow up 1b 5b A
// arrow down 1b 5b B
// arrow right1b  5b C
// arrow left 1b 5b D
// home 1b 4f 48
// end 1b 4f 46
// delete 1b 5b 33
// backspace 7f
//--------------------------
int cliGets(char *buf, int size) {
	char *ptr = buf + strlen(buf);
	int rv = GETS_OK;
	int c;

	// clear the string memory after \0
	int i;
	for (i = strlen(buf); i < size; i++)
		*(buf + i) = '\0';
	

	// process chars as they are coming in
	while ((c = cliTimeoutGetc()) != '\n') {
		if (c == '?') {
			//  ? is never printed
			rv = GETS_QUESTION;	
		}
		else if (c == '\t') {
			// TAB is never printed
			rv = GETS_TAB;
		}
		else if (c == 0x7f) { // backspace
			if (ptr <= buf) {
				ptr = buf;
				continue;
			}
			// move the string
			ptr--;
			memmove(ptr, ptr + 1, strlen(ptr + 1) + 1); // +1 for \0
			// print new string on the screen and position the cursor
			printf("%s%s%s%s%s", cursor_back, save_cursor, ptr, clear_line, restore_cursor );
			fflush(0);
		}
		// ansi esc 
		else if (c == 0x1b) {
			int c2 = cliTimeoutGetc();
			int c3 = cliTimeoutGetc();
//printf("%x %x\n", c2, c3);			
			// arrow up
			if (c2 == 0x5b && c3 == 'A') {
				rv = GETS_ARROW_UP;
			}
			// arrow down
			else if (c2 == 0x5b && c3 == 'B') {
				rv = GETS_ARROW_DOWN;
			}
			// arrow right
			else if (c2 == 0x5b && c3 == 'C') {
				if (*ptr == '\0') // we seem to be at the end of the buffer
					continue;
				ptr++;
				printf("%s",  cursor_forward);
			}
			// arrow left
			else if (c2 == 0x5b && c3 == 'D') {
				if (ptr <= buf) { // we seem to be at the beginning of the buffer
					ptr = buf;
					continue;
				}
				ptr--;
				printf("%s",  cursor_back);
			}
			// home
			else if (c2 == 0x4f && c3 == 0x48) {
				if (ptr <= buf) { // we seem to be at the beginning of the buffer
					ptr = buf;
					continue;
				}
				while (ptr > buf) {
					printf("%s",  cursor_back);
					--ptr;
				}
			}
			// end
			else if (c2 == 0x4f && c3 == 0x46) {
				if (*ptr == '\0') // we seem to be at the end of the buffer
					continue;
				while (*ptr != '\0') {
					printf("%s",  cursor_forward);
					++ptr;
				}
			}
			// delete
			else if (c2 == 0x5b && c3 == 0x33) {
				c2 = cliTimeoutGetc(); // finish the esc sequence
				if (*ptr == '\0') // we don't delete at the end of buffer
					continue;
				
				// move the string
				memmove(ptr, ptr + 1, strlen(ptr + 1) + 1); // +1 for \0
				// print new string on the screen and position the cursor
				printf("%s%s%s%s", save_cursor, ptr, clear_line, restore_cursor );
			}

			fflush(0);
		}
		else if (*ptr == '\0')	{ // adding chars at the end of the buffer
			*ptr++ = (char) c;
			char b[2];
			b[0] = (char) c;
			b[1] = '\0';
			printf("%s", b);
			fflush(0);
		}
		else { // inserting chars in the buffer
			char tmp[CLI_MAX_LINE];
			*tmp = (char) c;
			strcpy(tmp + 1, ptr);
			strcpy(ptr, tmp);
			
			printf("%s",  save_cursor);
			printf("%s", tmp);
			printf("%s",  restore_cursor);
			ptr++;
			printf("%s",  cursor_forward);

			fflush(0);
		}
		
		fflush(0);
		if (rv !=0)
			break;
	}
		
	return rv;
}
