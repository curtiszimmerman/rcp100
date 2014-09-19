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
/* micro_httpd - really small HTTP server
 **
 ** Copyright (C) 1999,2005 by Jef Poskanzer <jef@mail.acme.com>.
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 ** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 ** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 ** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 ** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 ** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 ** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 ** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 ** SUCH DAMAGE.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include "http.h"

#define SERVER_NAME "rcp_httpd"
#define SERVER_URL "http://rcp100.sourceforge.net/"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

RcpShm *shm = NULL;

void http_debug(uint8_t level, const char *str1, const char *str2) {
	if (rcpLogAccept(shm, level, RLOG_FC_HTTP)) {
		int muxsock =  rcpConnectMux(RCP_PROC_TESTPROC, NULL, NULL, NULL);	
		if (muxsock == 0)
			return;
	
		rcpLog(muxsock, RCP_PROC_TESTPROC, level, RLOG_FC_HTTP, "%s %s", str1, str2);
		close(muxsock);
	}
}


/* Forwards. */
static void strdecode( char* to, char* from );
static int hexit( char c );


// dispatch table
typedef struct {
	const char *name;
	void (*f)(void);
} Dispatch;
static Dispatch dtable[] = {
	{"index.html", html_index},
	{"index-log.html", html_index_log},
	{"services-leases.html", html_services_leases},
	{"configuration.html", html_configuration},
	{"system.html", html_system},
	{"routing.html", html_routing},
	{"routing-acl.html", html_routing_acl},
	{"routing-nat.html", html_routing_nat},
	{"routing-arp.html", html_routing_arp},
	{"routing-ospf.html", html_routing_ospf},
	{"routing-ospf-database.html", html_routing_ospf_database},
	{"routing-ospf-neighbors.html", html_routing_ospf_neighbors},
	{"routing-rip.html", html_routing_rip},
	{"routing-rip-database.html", html_routing_rip_database},
	{"services.html", html_services},
	{"services-dhcp.html", html_services_dhcp},
	{"services-ntp.html", html_services_ntp},
	{"services-snmp.html", html_services_snmp},
	{"netmon-log.html", html_netmon_log},
	{"setsecret.html", html_set_secret},
	{NULL, NULL}
};

// cfg table
typedef struct {
	const char *name;
} CfgDispatch;
static CfgDispatch cdtable[] = {
	{"cfgpasswd.html"},
	{"cfgstartup.html"},
	{"cfgrunning.html"},
	{"cfgcommands.html"},
	{"cfginterfaces.rcps"},
	{"cfgroutes.html"},
	{"cfgarps.html"},
	{"cfgnetmon.html"},
	{"cfgservices.rcps"},
	{"cfgntp.rcps"},
	{"cfgdns.rcps"},
	{"cfglogger.rcps"},
	{"cfgdhcpr.rcps"},
	{"cfgdhcps.rcps"},
	{"cfgsnmp.rcps"},
	{"cfgsnmpn.rcps"},
	{"cfgrip.rcps"},
	{"cfgospf.rcps"},
	{NULL}
};
	
static int valid_session(unsigned id) {
	int i;
	for (i = 0; i < RCP_HTTP_AUTH_TTL; i++) {
		if (shm->stats.http_session[i] == id)
			return 1;	
	}
	
	return 0;
}

#define MAXBUF 10000
int main( int argc, char** argv ) {
	char line[MAXBUF], method[MAXBUF], path[MAXBUF], protocol[MAXBUF];
	char* file;
	size_t len;
	struct dirent;
	int i;

	// check home directory
	if ( argc != 2 )
		send_error( 500, "Internal Error", (char*) 0, "Config error - no dir specified." );
	if ( chdir( argv[1] ) < 0 )
		send_error( 500, "Internal Error", (char*) 0, "Config error - couldn't chdir()." );
	
//system("ls -l");
//return 0;

	// parse header
	if ( fgets( line, sizeof(line), stdin ) == (char*) 0 )
		send_error( 400, "Bad Request", (char*) 0, "No request found." );
	if ( sscanf( line, "%[^ ] %[^ ] %[^ ]", method, path, protocol ) != 3 )
		send_error( 400, "Bad Request", (char*) 0, "Can't parse request." );

	int content_len = 0;
	while ( fgets( line, sizeof(line), stdin ) != (char*) 0 ) {
		if ( strcmp( line, "\n" ) == 0 || strcmp( line, "\r\n" ) == 0 )
			break;

		char type[MAXBUF], value[MAXBUF];
		if (sscanf(line, "%[^ ] %[^ ]", type, value) == 2) {
			if (strcasecmp(type, "Content-Length:") == 0)
				content_len = atoi(value);
		}
	}

	int post = 0;
	int get = 0;
	if (strcasecmp( method, "post") == 0 )
		post = 1;
	else if (strcasecmp( method, "get") == 0 )
		get = 1;
	else
		send_error( 501, "Not Implemented", (char*) 0, "That method is not implemented." );

	// parse request
	if ( path[0] != '/' )
		send_error( 400, "Bad Request", (char*) 0, "Bad filename." );
	file = &(path[1]);
	strdecode( file, file );
	if ( file[0] == '\0' )
		file = "./";
	len = strlen( file );
	if ( file[0] == '/' || strcmp( file, ".." ) == 0 || strncmp( file, "../", 3 ) == 0 || strstr( file, "/../" ) != (char*) 0 || strcmp( &(file[len-3]), "/.." ) == 0 )
		send_error( 400, "Bad Request", (char*) 0, "Illegal filename." );

	// extract sender ip address
	struct sockaddr_in from;
	int fromlen = sizeof (from);
	if (getpeername (0, (struct sockaddr *)&from, (socklen_t *) &fromlen) < 0)
		send_error( 404, "Not Found", (char*) 0, "Internal error." );
	if (from.sin_family != AF_INET)
		send_error( 404, "Not Found", (char*) 0, "Internal error." );
	uint32_t ip = ntohl(from.sin_addr.s_addr);

	// load shared memory
	if ((shm = rcpShmemInit(RCP_PROC_TESTPROC)) == NULL)
		send_error( 404, "Not Found", (char*) 0, "Internal error." );
	
//printf("%d file #%s#, method #%s#, path #%s#, content_len %d\n", __LINE__, file, method, path, content_len);
	char tmpstr[100];
	sprintf(tmpstr, "%d.%d.%d.%d, file", RCP_PRINT_IP(ip));
	http_debug(RLOG_DEBUG, tmpstr, file);
	
	// authentication
	if (post && strcmp(file, "login") == 0) {
		// read the content into line
		if ((content_len + 1) > MAXBUF) // protect line buffer
			send_error( 400, "Bad Request", (char*) 0, "Invalid login." );
		if (fgets( line, content_len + 1, stdin)) {
			auth_user(ip, line);
		}

		fflush(0);
		return 0;
	}
	else if (post && strcmp(file, "rest/command") == 0) {
		// read the content into line
		if ((content_len + 1) > MAXBUF) // protect line buffer
			send_error( 400, "Bad Request", (char*) 0, "Invalid rest command." );
		if (fgets( line, content_len + 1, stdin)) {
			sprintf(tmpstr, "%d.%d.%d.%d, rest command:", RCP_PRINT_IP(ip));
			http_debug(RLOG_INFO, tmpstr, line);
			auth_rest_command(ip, line);
		}
		fflush(0);
		return 0;
	}
	else if (post)
		send_error( 501, "Not Implemented", (char*) 0, "Post request not implemented." );
 	else if (get && strcmp(file, "rest/token.json") == 0) {
		auth_rest_token(ip);
		fflush(0);
		return 0;
	}
//	else if (get && strncmp(file, "setsecret.html", 14) == 0) {
//		html_set_secret();
// 		fflush(0);
// 		return 0;
// 	}
	else if (get && strcmp(file, "md5.js") == 0) {
		www_file(file, "application/x-javascript");
		fflush(0);
		return 0;
	}
	else if (strstr(file, "keepalive.json")) {
		char json_buf[100];
		sprintf(json_buf, "{\"result\":true,\"keepalive\":\"%u\"}", get_session());
		send_headers( 200, "Ok", (char*) 0, "application/json",  strlen(json_buf), -1);
		fflush(0);
		printf("%s", json_buf);
		fflush(0);
		return 0;
	}

	// favicon support
	else if (get && strstr(file, "favicon")) {
		send_headers( 200, "Ok", (char*) 0, "image/png", 1661, -1);
		fflush(0);
		favicon();
		fflush(0);
		return 0;
	}
	

	// validate ip address and session id
	unsigned sid;
	int found = 0;
	for (i = 0; i < RCP_HTTP_AUTH_LIMIT; i++) {
		if (shm->stats.http_auth[i].ip == ip) {
			found++;
			break;
		}
	}
	
	char *ptr = strstr(file, ";");
	if (ptr != NULL) {
		*ptr = '\0';
		ptr++;
		sscanf(ptr, "%u", &sid);
		if (valid_session(sid))
			found++;
	}

	if (found != 2) {
		send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
		printf("\n");
		html_send_auth();
		fflush(0);
		return 0;
	}
	else
		shm->stats.http_auth[i].ttl = RCP_HTTP_AUTH_TTL;
	sid = get_session();
		
	// build title
	int tlen = strlen(shm->config.hostname) + 50;
	char title[tlen];
	if (shm->config.ospf_router_id != 0)
		sprintf(title, "%s (%d.%d.%d.%d)",
			shm->config.hostname,
			RCP_PRINT_IP(shm->config.ospf_router_id));
	else
		sprintf(title, "%s", shm->config.hostname);
		
	// look for cfg files
	i = 0;
	if (get && strncmp(file, "cfg", 3) == 0) {
		while (cdtable[i].name != NULL) {
			if (strcmp(file, cdtable[i].name) == 0) {
				www_file(file, "text/html");
				fflush(0);
				return 0;
			}
			i++;
		}
		send_error( 404, "Not Found", (char*) 0, "Bad filename." );
	}

	// look for special files
	i = 0;
	while (dtable[i].name != NULL) {
		if (strcmp(file, dtable[i].name) == 0) {	
			send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
			printf("\n");
			html_start(title, file, sid);
			dtable[i].f();
			html_end();
			fflush(0);
			return 0;
		}
		i++;
	}

	if (strcmp(file, "interfaces.html") == 0) {
		send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
		printf("\n");
		html_start(title, file, sid);
		html_interfaces(0);
		html_end();
		fflush(0);
		return 0;
	}
	else if (strncmp(file, "interfaces-", 11) == 0) {
		send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
		printf("\n");
		html_start(title, file, sid);
		// extract interface index
		int index = 0;
		sscanf(file, "interfaces-%d", &index);
		html_interfaces(index);
		html_end();
		fflush(0);
		return 0;
	}
	else if (strcmp(file, "netmon.html") == 0) {
		send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
		printf("\n");
		html_start(title, file, sid);
		html_netmon(-1);
		html_end();
		fflush(0);
		return 0;
	}
	else if (strncmp(file, "netmon-", 7) == 0) {
		send_headers( 200, "Ok", (char*) 0, "text/html", -1, -1);
		printf("\n");
		html_start(title, file, sid);
		// extract index
		int index = 0;
		sscanf(file, "netmon-%d", &index);
		html_netmon(index);
		html_end();
		fflush(0);
		return 0;
	}
	else if (strcmp(file, "router.svg") == 0) {
		send_headers( 200, "Ok", (char*) 0, "image/svg+xml", -1, -1);
		printf("\n");
		svg_router();
		fflush(0);
		return 0;
	}

	send_error( 400, "Bad Request", (char*) 0, "Illegal filename." );
	exit( 0 );
}


void send_error( int status, char* title, char* extra_header, char* text ) {
	send_headers( status, title, extra_header, "text/html", -1, -1 );
	(void) printf( "<!DOCTYPE html><html><head><title>%d %s</title></head>\n<body bgcolor=\"#cc9999\"><h4>%d %s</h4>\n", status, title, status, title );
	(void) printf( "%s\n<br/></body></html>\n", text );
	(void) fflush( stdout );
	exit( 1 );
}


void send_headers( int status, char* title, char* extra_header, char* mime_type, off_t length, time_t mod ) {
	time_t now;
	char timebuf[100];

	(void) printf( "%s %d %s\015\012", PROTOCOL, status, title );
	(void) printf( "Server: %s\015\012", SERVER_NAME );
	now = time( (time_t*) 0 );
	(void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
	(void) printf( "Date: %s\015\012", timebuf );
	if ( extra_header != (char*) 0 )
		(void) printf( "%s\015\012", extra_header );
	if ( mime_type != (char*) 0 )
		(void) printf( "Content-Type: %s\015\012", mime_type );
	if ( length >= 0 )
		(void) printf( "Content-Length: %lld\015\012", (long long int) length );
	if ( mod != (time_t) -1 ) {
		(void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &mod ) );
		(void) printf( "Last-Modified: %s\015\012", timebuf );
	}
	(void) printf( "Connection: close\015\012" );
	(void) printf( "\015\012" );
}



static void strdecode( char* to, char* from ) {
	for ( ; *from != '\0'; ++to, ++from ) {
		if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ) {
			*to = hexit( from[1] ) * 16 + hexit( from[2] );
			from += 2;
		}
		else
			*to = *from;
	}
	*to = '\0';
}


static int hexit( char c ) {
	if ( c >= '0' && c <= '9' )
		return c - '0';
	if ( c >= 'a' && c <= 'f' )
		return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' )
		return c - 'A' + 10;
	return 0;				  /* shouldn't happen, we're guarded by isxdigit() */
}


