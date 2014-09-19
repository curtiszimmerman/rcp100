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
#include <termios.h>
#include <sys/ioctl.h>
#include "cli.h"

// store original terminal settings here, upon exit restore the terminal
static struct termios tin;

// set terminal when cli is entered
void cliSetTerminal(void) {
	struct termios tlocal;

	tcgetattr(STDIN_FILENO, &tin);
	memcpy(&tlocal, &tin, sizeof(tin));
	tlocal.c_lflag &= ~ICANON;
	tlocal.c_lflag &= ~ECHO;
	tlocal.c_lflag &= ~IXON;
	tcsetattr(STDIN_FILENO,TCSANOW,&tlocal);
}


// restore terminal when cli exits
void cliRestoreTerminal(void) {
	tcsetattr(STDIN_FILENO,TCSANOW,&tin);
}


// update terminal size
void cliUpdateTerminalSize(void) {
	struct winsize sz;
	
	if (!ioctl(0, TIOCGWINSZ, &sz)) {
		csession.term_col = sz.ws_col;
		csession.term_row = sz.ws_row;
	}
}
