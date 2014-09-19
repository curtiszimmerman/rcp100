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
#include "http.h"
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static void print_link_newtab(FILE *fp, int x, int y, const char *text, const char *link) {
	if (fp == NULL)
		return;
		
	fprintf(fp, "<foreignObject class=\"node\" x=\"%d\" y=\"%d\" width=\"300\" height=\"20\">\n",
		x, y);
	fprintf(fp, "<body xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	fprintf(fp, "<font style=\"font-family:sans-serif;font-size:12\"><a href=\"%s\" target=\"_blank\">%s</a></font>\n",
		link, text);
	fprintf(fp, "</body></foreignObject>\n");
}

static void print_link(FILE *fp, int x, int y, const char *text1, const char *text2, const char *link) {
	if (fp == NULL)
		return;

	fprintf(fp, "<foreignObject class=\"node\" x=\"%d\" y=\"%d\" width=\"300\" height=\"20\">\n",
		x, y);
	fprintf(fp, "<body xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	fprintf(fp, "<font  style=\"font-family:sans-serif;font-size:12\"><a href=\"%s;%u\">%s</a> %s</font>\n",
		link, html_session(), text1, text2);
	fprintf(fp, "</body></foreignObject>\n");
}

static int find_if_index(uint32_t ip, uint32_t *dr, uint32_t *bdr) {
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;
		if (intf->ip == ip) {
			*dr = intf->stats.ospf_dr;
			*bdr = intf->stats.ospf_bdr;
			return i;
		}
	}
	
	return -1;
}


static const char *find_hostname(uint32_t ip) {
	int i;
	for (i = 0; i < RCP_HOST_LIMIT; i++) {
		RcpIpHost *host = &shm->config.ip_host[i];
		if (!host->valid)
			continue;
		if (host->ip == ip)
			return host->name;
	}
	return NULL;
}

void svg_net_diag(int acnt, RcpActiveNeighbor *nb, const char *hostname, int ospf, uint32_t ospf_router_id) {
	int i;
	
	// store the links in a temporary transport file and print them at the end of svg
	FILE *fplink = NULL;
	char linkname[50] = "/opt/rcp/var/transport/hXXXXXX";
	int fd = mkstemp(linkname);
	if (fd != -1) {
		close(fd);
		fplink = fopen(linkname, "w+");
	}

	uint32_t if_ip = 0;
	uint32_t bdr = 0;
	uint32_t dr = 0;
	
	// diagram size
	int w = 600;
	int ystep = 70;	// vertical distance between active routers
	int h = ystep * (acnt + 1);
	
	// router.svg width 48, height 24
	int wrouter = 48;
	int hrouter = 24;
	
	// horizontal grid
	int grouter = 150; // my router
	int gnetstart = 250;	// network start
	int gnetend = 350; // network end
	int arouter = 450; // position of active router


	printf("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"");
//	printf(" width=\"%dpx\" height=\"%dpx\" style=\"border: 1px solid black;\">\n", w, h);	
	printf(" width=\"%dpx\" height=\"%dpx\">\n", w, h);	
	
	// print networks
	uint32_t last_net = 0;
	int last_x = 0;
	int last_y = 0;
	RcpActiveNeighbor *ptr = nb;
	for (i = 0; i < acnt; i++, ptr++) {
		int j;
		RcpActiveNeighbor *ptr2;

		if (last_net != ptr->network) {
			// find the number of neighbors on this network
			int cnt = 0;
			for (j = 0, ptr2 = ptr; j < acnt; j++, ptr2++) {
				if (ptr->network == ptr2->network)
					cnt++;
			}
			
			printf("<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" style=\"stroke:black; stroke-width: 1;\"/>\n",
				gnetstart, (i + 1) * ystep, gnetend, (i + 1) * ystep);
			printf("<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" style=\"stroke:black; stroke-width: 1;\"/>\n",
				grouter, h/2, gnetstart, (i + 1) * ystep);
			last_net = ptr->network;
			last_x = gnetend;
			last_y = (i + 1) * ystep;
			
			// network address
			if (ospf)
				printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
					"style=\"font-family:sans-serif;font-size:12\">%d.%d.%d.%d/%d (area %u)</text>\n",
					gnetstart, (i + 1) * ystep - 10,
					RCP_PRINT_IP(ptr->network),
					ptr->netmask_cnt,
					ptr->area);
			else
				printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
					"style=\"font-family:sans-serif;font-size:12\">%d.%d.%d.%d/%d</text>\n",
					gnetstart, (i + 1) * ystep - 10,
					RCP_PRINT_IP(ptr->network),
					ptr->netmask_cnt);

			// interface address
			if_ip = ptr->if_ip;
			float offset_y = (i + 1) * ystep - h/2;
			offset_y /= (float) 3;
			char address[30];
			char link[30];
			sprintf(address, ".%d", ptr->if_ip & 0xff);
			
			int ifindex = find_if_index(ptr->if_ip, &dr, &bdr);
			char *type = " ";
			if (if_ip && if_ip == dr)
				type = "(DR)";
			else if (if_ip && if_ip == bdr)
				type = "(BDR)";
			
			if (ifindex == -1)
				sprintf(link, "interfaces.html");
			else
				sprintf(link, "interfaces-%d.html", ifindex);

			print_link(fplink, grouter + (gnetstart - grouter) / 3 + 2, (float)  (h / 2) + offset_y, address, type, link);
		}
		else {

 			printf("<circle cx=\"%d\" cy=\"%d\" r=\"2\" stroke=\"black\" stroke-width=\"1\" fill=\"black\"/>\n",
				last_x, last_y);
		}
		
		// dealing with multipath - matching router id
		int found = -1;
		ptr2 = nb;
		for (j = 0; j < i; j++, ptr2++) {
			if (ospf) {
				if (ptr2->router_id == ptr->router_id) {
					found = j;
					break;
				}
			}
			else {
				// for rip there is no way to do it - just assume all routers are different
				if (ptr2->ip == ptr->ip) {
					found = j;
					break;
				}
			}
		}
			
		if (found == -1) {
			printf("<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" style=\"stroke:black; stroke-width: 1;\"/>\n",
				last_x, last_y, arouter, (i + 1) * ystep);
			printf("<image x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" xlink:href=\"router.svg;%u\"/>\n",
				arouter - wrouter / 2, ystep * (i + 1) - hrouter / 2, wrouter, hrouter, html_session());
			// neighbor ip address
			char address[30];
			char link[30];
			sprintf(address, ".%d", ptr->ip & 0xff);
			sprintf(link, "http://%d.%d.%d.%d", RCP_PRINT_IP(ptr->ip));
			print_link_newtab(fplink, arouter - 2 * wrouter/3, ystep * (i + 1) - hrouter, address, link);
			// neighbor router id
			if (ospf) {
				printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
					"style=\"font-family:sans-serif;font-size:12\">Router ID</text>\n",
					arouter, ystep * (i + 1) + hrouter);
				if (if_ip && ptr->ip == dr)
					printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
						"style=\"font-family:sans-serif;font-size:12\">%d.%d.%d.%d (DR)</text>\n",
						arouter, ystep * (i + 1) + hrouter + hrouter /2,
						RCP_PRINT_IP(ptr->router_id));
				else if (if_ip && ptr->ip == bdr)
					printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
						"style=\"font-family:sans-serif;font-size:12\">%d.%d.%d.%d (BDR)</text>\n",
						arouter, ystep * (i + 1) + hrouter + hrouter /2,
						RCP_PRINT_IP(ptr->router_id));
				else
					printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
						"style=\"font-family:sans-serif;font-size:12\">%d.%d.%d.%d</text>\n",
						arouter, ystep * (i + 1) + hrouter + hrouter /2,
						RCP_PRINT_IP(ptr->router_id));

				// find the hostname
				const char *name = find_hostname(ptr->router_id);
				if (name) {
					printf("<text x=\"%d\" y=\"%d\" fill=\"black\" font-weight = \"bold\" \n"
						"style=\"font-family:sans-serif;font-size:12\">%s</text>\n",
						arouter + wrouter / 2, ystep * (i + 1), name);
				}
			}
		}
		else {
			printf("<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" style=\"stroke:black; stroke-width: 1;\"/>\n",
				last_x, last_y, arouter, (found + 1) * ystep);
			printf("<image x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" xlink:href=\"router.svg;%u\"/>\n",
				arouter - wrouter / 2, ystep * (found + 1) - hrouter / 2, wrouter, hrouter, html_session());
		}
		

	}

	// print myrouter
	printf("<image x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" xlink:href=\"router.svg;%u\"/>\n",
		grouter - wrouter / 2, h / 2 - hrouter / 2, wrouter, hrouter, html_session());

	printf("<text x=\"%d\" y=\"%d\" fill=\"black\" font-weight = \"bold\" \n"
		"style=\"font-family:sans-serif;font-size:12\">%s</text>\n",
		grouter - ((int) strlen(hostname) * 9), h/2 - hrouter, hostname);
		
	if (ospf) {
		printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
			"style=\"font-family:sans-serif;font-size:12\">Router ID</text>\n",
			25, h/2 + hrouter / 2);
		printf("<text x=\"%d\" y=\"%d\" fill=\"black\" \n"
			"style=\"font-family:sans-serif;font-size:12\">%d.%d.%d.%d</text>\n",
			25, h/2 + hrouter,
			RCP_PRINT_IP(ospf_router_id));
	}
		
	// print links
	if (fplink) {	
		rewind(fplink);
		#define BUFSIZE 500
		char buf[500];
		while (fgets(buf, BUFSIZE, fplink)) {
			printf("%s", buf);
		}
		fclose(fplink);
		unlink(linkname);
	}
		

	// end svg
	printf("</svg>\n");
//printf("ifip %d.%d.%d.%d, dr %d.%d.%d.%d, bdr %d.%d.%d.%d\n",
//RCP_PRINT_IP(if_ip), RCP_PRINT_IP(dr), RCP_PRINT_IP(bdr));
	
}
