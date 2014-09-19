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
#include <netdb.h>
#include "librcp.h"

#define BUFSIZE 1024
static char buf[BUFSIZE + 1];
int debug = 0;

static void send_message_http(RcpNetmonHost *ptr);
static int read_message_http(RcpNetmonHost *ptr);
static int read_message_ssh(RcpNetmonHost *ptr);
static int read_message_smtp(RcpNetmonHost *ptr);

typedef struct worker_t {
	void (*send)(RcpNetmonHost *ptr);
	int (*read)(RcpNetmonHost *ptr);
} Worker;

static Worker worker[RCP_NETMON_TYPE_MAX] = {
	{NULL, NULL}, // ICMP
	{NULL, NULL}, // TCP
	{NULL, read_message_ssh}, // SSH
	{send_message_http, read_message_http}, // HTTP
	{NULL, read_message_smtp} // SMTP
};

static int check_type(RcpNetmonHost *ptr) {
	if (ptr->type == RCP_NETMON_TYPE_HTTP ||
	    ptr->type == RCP_NETMON_TYPE_SSH ||
	    ptr->type == RCP_NETMON_TYPE_SMTP)
		return 1;

	return 0;
}

static int get_port(RcpNetmonHost *ptr) {
	if (ptr->port)
		return ptr->port;
	if (ptr->type == RCP_NETMON_TYPE_HTTP)
		return 80;
	if (ptr->type == RCP_NETMON_TYPE_SSH)
		return 22;
	if (ptr->type == RCP_NETMON_TYPE_SMTP)
		return 25;
	ASSERT(0);
	return 0;
}


// differenmce in microseconds
static inline unsigned delta_tv(struct timeval *start, struct timeval *end) {
	return (end->tv_sec - start->tv_sec)*1000000 + end->tv_usec - start->tv_usec;
}

static float resptime(RcpNetmonHost *ptr) {
	ASSERT(ptr);
	
	struct timeval end_tv;
	gettimeofday (&end_tv, NULL);
	unsigned resp_time = delta_tv(&ptr->start_tv, &end_tv);
	ptr->resptime_acc += resp_time;
	ptr->resptime_samples++;
	return (float) resp_time / (float) 1000000;
}

static void send_message_http(RcpNetmonHost *ptr) {
	if (debug)
		printf("MONITOR: sending http message\n");

	// send get request
	if (ptr->port)
		sprintf(buf,
			"GET / HTTP/1.1\r\n"
			"User-Agent: rcpagent\r\n"
			"Accept: */*\r\n"
			"Host: %s:%d\r\n"
			"Connection: close\r\n\r\n",
			ptr->hostname, ptr->port);
	else
		sprintf(buf,
			"GET / HTTP/1.1\r\n"
			"User-Agent: rcpagent\r\n"
			"Accept: */*\r\n"
			"Host: %s\r\n"
			"Connection: close\r\n\r\n",
			ptr->hostname);
	send(ptr->sock, buf, strlen(buf), 0);
}

static int read_message_http(RcpNetmonHost *ptr) {
	if (debug)
		printf("MONITOR: read http message\n");

	// read data
	int len = recv(ptr->sock, buf, BUFSIZE, 0);
	int err = -1;
	if (len <= 0) {
		if (debug)
			perror("MONITOR: read");
		err = 1;
	}
	else {
		buf[len] = '\0';

		if (debug)
			printf("MONITOR: received buffer  length %d, #%s#\n", len, buf);

		// parse the data
		char *ptr1 = buf;
		int found = 0;
		while (*ptr1 != '\0') {
			if (*ptr1 == '\n' || *ptr1 == '\r') {
				found = 1;
				break;
			}
			ptr1++;
		}
		
		if (!found) {
			if (debug)
				printf("MONITOR: fragmented buffer, reading again\n");
			usleep(1000);  // sleep 1 ms
			int len1 = recv(ptr->sock, buf + len, BUFSIZE - len, 0);
			if (len1 <= 0) {
				if (debug)
					perror("MONITOR: read");
				err = 1;
			}
			else {
				// parse the data
				ptr1 = buf;
				while (*ptr1 != '\0') {
					if (*ptr1 == '\n' || *ptr1 == '\r') {
						found = 1;
						break;
					}
					ptr1++;
				}
			}
		}		
		
		if (found) {
			*ptr1 = '\0';
			
			char *scode = strchr(buf, ' ') + 1;
			if (scode == NULL)
				err = 1;
			else {
				int code = 0;
				sscanf(scode, "%d", &code);
				if (code >= 200 && code < 400)
					err = 0;
			}
		}
			
		if (debug)
			printf("MONITOR: received from %s: %s\n",
				ptr->hostname, buf);							
	}
	return err;
}

static int read_message_smtp(RcpNetmonHost *ptr) {
	if (debug)
		printf("MONITOR: read smtp message\n");

	// read data
	int len = recv(ptr->sock, buf, BUFSIZE, 0);
	int err = -1;
	if (len <= 0) {
		if (debug)
			perror("MONITOR: read");
		err = 1;
	}
	else {
		buf[len] = '\0';
		// parse the data
		char *ptr1 = buf;
		int found = 0;
		while (*ptr1 != '\0') {
			if (*ptr1 == '\n' || *ptr1 == '\r') {
				found = 1;
				break;
			}
			ptr1++;
		}
		if (found) {
			*ptr1 = '\0';
			if (strstr(buf, "220"))
				err = 0;
		}
			
		if (debug)
			printf("MONITOR: received from %s: %s\n",
				ptr->hostname, buf);							
	}
	
	return err;
}

static int read_message_ssh(RcpNetmonHost *ptr) {
	if (debug)
		printf("MONITOR: read ssh message\n");
		
	// read data
	int len = recv(ptr->sock, buf, BUFSIZE, 0);
	int err = -1;
	if (len <= 0) {
		if (debug)
			perror("MONITOR: read");
		err = 1;
	}
	else {
		buf[len] = '\0';
		// parse the data
		char *ptr1 = buf;
		int found = 0;
		while (*ptr1 != '\0') {
			if (*ptr1 == '\n' || *ptr1 == '\r') {
				found = 1;
				break;
			}
			ptr1++;
		}
		if (found) {
			*ptr1 = '\0';
			if (strncasecmp(buf, "SSH-", 4) == 0)
				err = 0;
		}
			
		if (debug)
			printf("MONITOR: received from %s: %s\n",
				ptr->hostname, buf);							
	}
	
	return err;
}

int main(int argc, char **argv) {
	// test debug mode
	if (getenv("DBGMONHTTP"))
		debug = 1;

	if (debug)
		printf("MONITOR: starting %s\n", argv[0]);
	
	// open rcp shared memory
	RcpShm *shm = NULL;
	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}
	

	// file descriptor set
	fd_set fds_tx;
	fd_set fds_rx;
	FD_ZERO(&fds_tx);
	FD_ZERO(&fds_rx);
	int maxfd = 0;
	
	// open sockets and try to connect
	int i;
	RcpNetmonHost *ptr;
	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid || check_type(ptr) == 0)
			continue;
		if (ptr->ip_resolved == 0)
			continue;	// address not available yet
	
		// update tx counters
		ptr->pkt_sent++;
		ptr->interval_pkt_sent++;
		ptr->wait_response = 1;	// state 1: waiting for connections
		
		// open socket
		int sock;
		struct sockaddr_in server;
		sock = socket(AF_INET , SOCK_STREAM , 0);
		if (sock == -1)
			continue;
		
		// set the socket to non-blocking
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
			close(sock);
			continue;
		}	
		
		// port
		int port = get_port(ptr);
		
		// connect
		server.sin_addr.s_addr = htonl(ptr->ip_resolved);
		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
			if (errno != EINPROGRESS) {
				close(sock);
				continue;
			}
		}	

		// set fd
		FD_SET(sock, &fds_tx);
		maxfd = (sock > maxfd)? sock: maxfd;
		ptr->sock = sock;
		
		// set the socket to non-blocking
		if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
			ASSERT(0);
			perror("cannot make the socket nonblocking");
			exit(1);
		}

		gettimeofday(&ptr->start_tv, NULL);
		if (debug)
			printf("MONITOR: open socket %d.%d.%d.%d:%d, %s, \n",
				RCP_PRINT_IP(ptr->ip_resolved), port, ptr->hostname);		
	}
	
	// select loop
	struct timeval ts;
	ts.tv_sec = 10; // 10 seconds
	ts.tv_usec = 0;
	while (1) {
		int nready = select(maxfd + 1, &fds_rx, &fds_tx, (fd_set *) 0, &ts);
		if (nready < 0) {
			exit(1);
		}
		else if (nready == 0) {
			if (debug)
				printf("MONITOR: %s giving up!\n", argv[0]);
			exit(0);
		}
		else {
			for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
				if (!ptr->valid || check_type(ptr) == 0)
					continue;
				if (ptr->ip_resolved == 0)
					continue;	// address not available yet
				
				if (FD_ISSET(ptr->sock, &fds_tx)) {
					if (ptr->wait_response == 1) {
						if (debug)
							printf("MONITOR: socket %d.%d.%d.%d connected\n", 
								RCP_PRINT_IP(ptr->ip_resolved));						
						FD_CLR(ptr->sock, &fds_tx);
						ptr->wait_response = 2;
						if (worker[ptr->type].send != NULL)
							worker[ptr->type].send(ptr);
					}
					else
						ASSERT(0);
				}
				if (FD_ISSET(ptr->sock, &fds_rx)) {
					if (ptr->wait_response == 2) {
						int err = -1;
						if (worker[ptr->type].read != NULL)
							err = worker[ptr->type].read(ptr);
						
						// close socket
						FD_CLR(ptr->sock, &fds_rx);
						close(ptr->sock);
						ptr->sock = 0;
						
						// update counters
						if (err == 0) {
							ptr->pkt_received++;
							ptr->interval_pkt_received++;
							ptr->wait_response = 0;
							// calculate the response time
							float rsp = resptime(ptr);
							if (debug)
								printf("MONITOR: connection OK to %s, response time %fs\n",
									ptr->hostname, rsp);
						}
						
					}
					else
						ASSERT(0);


				}
			}
			
			FD_ZERO(&fds_tx);
			FD_ZERO(&fds_rx);
			maxfd = 0;
			int found = 0;
			for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
				if (!ptr->valid || check_type(ptr) == 0)
					continue;
				if (ptr->ip_resolved == 0)
					continue;	// address not available yet
				if (ptr->sock) {
					if (ptr->wait_response == 1)
						FD_SET(ptr->sock, &fds_tx);
					if (ptr->wait_response == 2)
						FD_SET(ptr->sock, &fds_rx);
					maxfd = (ptr->sock > maxfd)? ptr->sock: maxfd;
					found = 1;
				}
			}
			if (!found) {
				exit(0);
			}
		}
	}
	
	return 0;
}

