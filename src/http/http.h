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
#ifndef HTTP_H
#define HTTP_H
#include <stdio.h>
#include <stdlib.h>
#include "librcp.h"

// main.c
extern RcpShm *shm;
void send_error( int status, char* title, char* extra_header, char* text );
void send_headers( int status, char* title, char* extra_header, char* mime_type, off_t length, time_t mod );
static inline unsigned get_session(void) {
	unsigned t = (unsigned) time(NULL);
	int index = t % RCP_HTTP_AUTH_TTL;
	return shm->stats.http_session[index];
}
void http_debug(uint8_t level, const char *str1, const char *str2);


// html.c
void html_redir(unsigned sid);
void html_start(const char *title, const char *url, unsigned sid);
void html_end(void);
void html_index(void);
void html_index_log(void);
void html_system(void);
void html_interfaces(int index);
void html_services(void);
void html_netmon(int index);
void html_netmon_log(void);
int count_active_neighbors(int type);
void html_routing(void);
void html_routing_acl(void);
void html_routing_nat(void);
void html_routing_arp(void);
void html_routing_ospf(void);
void html_routing_ospf_database(void);
void html_routing_ospf_neighbors(void);
void html_routing_rip(void);
void html_routing_rip_database(void);
void html_services_dhcp(void);
void html_services_ntp(void);
void html_services_snmp(void);
void html_send_auth(void);
unsigned html_session(void);
void html_configuration(void);
void html_redir_passwd(unsigned sid);
void html_set_secret(void);
void html_services_leases(void);

// svg.c
void svg(const char *label, int w, int h, int xmargin, int ymargin, unsigned *data, unsigned data_cnt);
void svg_hbar(int w, int h);
void svg_checkmark(int size);
void svg_cross(int size);

// svg2.c
void svg2(const char *label1, const char *label2, int w, int h, int xmargin, int ymargin, unsigned *data1, unsigned *data2, unsigned data_cnt);

// svg_router.c
void svg_router(void);

// svg_diagram.c
void svg_net_diag(int acnt, RcpActiveNeighbor *nb, const char *hostname, int ospf, uint32_t ospf_router_id);

// routing.c
void routing_table_summary(void);
void routing_table_full(void);
void routing_arp_summary(void);
void routing_arp_full(void);
void routing_ospf_summary(void);
void routing_rip_summary(void);
void routing_ospf(void);
void routing_rip(void);

// favicon.c
void favicon(void);

// auth.c
void auth_user(uint32_t ip, char *line);
void auth_rest_token(uint32_t ip);
void auth_rest_command(uint32_t ip, char *line);

// www_file.c
void www_file(const char *fname, const char *type);

typedef struct {
	char *fname;
	void (*f)(FILE *);
} HttpCgiBind;
extern HttpCgiBind httpcgibind[];


#endif
