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
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/times.h>
#include <fcntl.h>
#include "cli.h"
#include "librcp_mux.h"

int muxsocket = 0;

void cliReceiveMux(void) {
	int nread;
	char buf[1024];
	errno = 0;

	nread = recv(muxsocket, buf, sizeof(buf), 0);
	if (errno == EAGAIN)
		return;

	if(nread < 1) {
		close(muxsocket);
		printf("Info: RCP temporarily disconnected\n");
		muxsocket = 0;
	}
#ifdef DBGCLI
	printf("RCP: cleaning incoming RCP data\n");
#endif
}


void run_cmd_from_file(const char *fname) {
	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		cliPrintLine("Error: no information available\n");
		return;
	}
	
#define FILE_LINE_BUF 1024
	char line[FILE_LINE_BUF + 1];
	if (fgets(line, FILE_LINE_BUF, fp) != NULL) {
		rcpDebug("executing command #%s#\n", line);
		char *rv = cliRunProgram(line);
		if (rv != NULL)
			free(rv);
	}
	fclose(fp);
}

void run_terminal_cmd_from_file(const char *fname) {
	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		cliPrintLine("Error: no information available\n");
		return;
	}
	
#define FILE_LINE_BUF 1024
	char line[FILE_LINE_BUF + 1];
	if (fgets(line, FILE_LINE_BUF, fp) != NULL) {
		rcpDebug("executing command #%s#\n", line);
		cliRestoreTerminal();
		int v = system(line);
		if (v == -1)
			ASSERT(0);
		cliSetTerminal();
	}
	fclose(fp);
}

void cliSendMux(CliMode mode, const char *cmd, unsigned destination, CliNode *node) {
	if (muxsocket == 0) {
		// try to reconnect the multiplexer socket
		if (isatty(0))
			muxsocket = rcpConnectMux(RCP_PROC_CLI, ttyname(0), csession.cli_user, csession.cli_user_ip);
		else
			muxsocket = rcpConnectMux(RCP_PROC_CLI, NULL, csession.cli_user, csession.cli_user_ip);

		if (muxsocket == 0) {
			printf("Error: cannot send command to RCP, attempting recovery\n");
			return;
		}
	}
	int n;

	RcpPkt *pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		ASSERT(0);
		printf("Error: cannot allocate memory, attempting recovery...\n");
		return;
	}
	memset(pkt, 0, sizeof(RcpPkt) + RCP_PKT_DATA_LEN);

	// set packet data
	pkt->type = RCP_PKT_TYPE_CLI;
	pkt->destination = destination;
	pkt->sender = RCP_PROC_CLI;
	pkt->pkt.cli.mode = mode;
	pkt->pkt.cli.clinode = node;
	pkt->pkt.cli.html = csession.html;
	char *data = (char *) pkt + sizeof(RcpPkt);
	strcpy(data, cmd);
	pkt->data_len = strlen(data) + 1;

	// add mode data
	pkt->pkt.cli.mode_data = csession.mode_data;

	rcpDebug("sending request to %s\n", rcpProcName(destination));
	// send packet
	errno = 0;
	n = send(muxsocket, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
	if(n < 1 || muxsocket == 0) {
		if (muxsocket) {
			close(muxsocket);
			muxsocket = 0;
		}
		printf("Info: RCP temporarily disconnected, attempting recovery...\n");
		free(pkt);
		return;
	}
	ASSERT(n == sizeof(RcpPkt) + pkt->data_len);

	// wait for no more than 10 seconds for the command to complete
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(muxsocket, &fds);
	int maxfd = muxsocket + 1;
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0, &timeout);

	if( FD_ISSET(muxsocket, &fds)) {
		errno = 0;
		n = recv(muxsocket, pkt, sizeof(RcpPkt), 0);;
		if(n < sizeof(RcpPkt) || errno != 0) {
			close(muxsocket);
			printf("Error: RCP socket disconnected, attempting recovery...\n");
			muxsocket = 0;
			free(pkt);
			return;
		}

		// read the packet data
		if (pkt->data_len != 0) {
			n += recv(muxsocket, (unsigned char *) pkt + sizeof(RcpPkt), pkt->data_len, 0);
		}

		// verify length
		ASSERT(n == sizeof(RcpPkt) + pkt->data_len);
		data = (char *) pkt + sizeof(RcpPkt);
		if (pkt->data_len != 0 && data[0] != '\0' && pkt->pkt.cli.file_transport == 0) {
			cliPrintBuf(data);
		}
		else if (pkt->data_len != 0 && data[0] != '\0' && pkt->pkt.cli.file_transport == 1 &&
		             pkt->pkt.cli.run_cmd == 0 && pkt->pkt.cli.run_cmd_term == 0) {
			cliPrintFile(data);
			unlink(data);
		}
		else if (pkt->data_len != 0 && data[0] != '\0' && pkt->pkt.cli.file_transport == 1 &&
		             pkt->pkt.cli.run_cmd == 1) {
			run_cmd_from_file(data);
			unlink(data);
		}
		else if (pkt->data_len != 0 && data[0] != '\0' && pkt->pkt.cli.file_transport == 1 &&
		             pkt->pkt.cli.run_cmd_term == 1) {
			run_terminal_cmd_from_file(data);
			unlink(data);
		}
		// if the returning packet has a different mode, change the cli mode
		// and store the mode data
		if (pkt->pkt.cli.mode != mode) {
			csession.cmode = pkt->pkt.cli.mode;
			csession.mode_data = pkt->pkt.cli.mode_data;
		}
	}
	else {
		printf("Error: subsytem not reponding, attempting recovery\n");
	}

	free(pkt);
}
