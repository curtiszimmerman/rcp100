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
#include "xmlstats.h"
extern int debug;

static void print_unsigned_array(FILE *fp, int interval, unsigned *ptr) {
	int i;
	// print the stats in reverse order and wrap
	for (i = interval; i >= 0; i--)
		fprintf(fp, "%u, ", ptr[i]);
	for (i = RCP_MAX_STATS - 1; i > interval; i--)
		fprintf(fp, "%u, ", ptr[i]);
}

void save(void) {
	// open a temorary file
	char fname[50];
	FILE *fp = rcpTransportFile(fname);
	if (fp == NULL) {
		ASSERT(0);
		return;
	}

	// print header
	fprintf(fp, "<?xml version=\"1.0\"?>\n");
//<!DOCTYPE xmlstats SYSTEM "xmlstats.dtd">
	fprintf(fp, "\n");
	
	// print timestamp
	time_t ts = time(NULL);
	unsigned adjusted = (unsigned) ts;
	adjusted -= (adjusted % 900);	// ajusting the time to the start of the current interval
	if (debug)
		printf("current timestamp %u, adjusted timestamp %u\n", (unsigned) ts, adjusted);
	fprintf(fp, "<xmlstats timestamp=\"%u\">", adjusted);
	fprintf(fp, "<!-- %s -->\n", ctime((time_t *) &adjusted));
	int interval = rcpGetStatsInterval();
	
	// cpu load and free memory
	fprintf(fp, "<cpu_load>");
	print_unsigned_array(fp, interval, shm->stats.cpu);
	fprintf(fp, "</cpu_load>\n");
	fprintf(fp, "<free_mem>");
	print_unsigned_array(fp, interval, shm->stats.freemem);
	fprintf(fp, "</free_mem>\n");
	
	// dns query statistics
	fprintf(fp,"<dns_query>");
	print_unsigned_array(fp, interval, shm->stats.dns_q);
	fprintf(fp,"</dns_query>\n");
	
	// dhcp relay rx statistics
	fprintf(fp,"<dhcp_relay_rx>");
	print_unsigned_array(fp, interval, shm->stats.dhcp_rx);
	fprintf(fp,"</dhcp_relay_rx>\n");

	// interface stats
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;

		fprintf(fp, "<interface name=\"%s\">\n", intf->name);
		fprintf(fp, "   <rx_kbps>");
		print_unsigned_array(fp, interval, intf->stats.rx_kbps);
		fprintf(fp, "</rx_kbps>\n");
		fprintf(fp, "   <tx_kbps>");
		print_unsigned_array(fp, interval, intf->stats.tx_kbps);
		fprintf(fp, "</tx_kbps>\n");
		fprintf(fp, "</interface>\n");
	}
	
	// network monitoring
	for (i = 0; i < RCP_NETMON_LIMIT; i++) {
		RcpNetmonHost *nm = &shm->config.netmon_host[i];
		if (!nm->valid)
			continue;
		fprintf(fp, "<monitor hostname=\"%s\" port=\"%u\" type=\"%u\">\n",
			nm->hostname, nm->port, nm->type);
		fprintf(fp, "   <rx_packets>");
		print_unsigned_array(fp, interval, nm->rx);
		fprintf(fp, "</rx_packets>\n");
		fprintf(fp, "   <tx_packets>");
		print_unsigned_array(fp, interval, nm->tx);
		fprintf(fp, "</tx_packets>\n");
		fprintf(fp, "   <response_time>");
		print_unsigned_array(fp, interval, nm->resptime);
		fprintf(fp, "</response_time>\n");
		fprintf(fp, "</monitor>\n");
	}
	
	// end file
	fprintf(fp, "</xmlstats>\n");
	fclose(fp);
	
	// flip the file
	char cmd[500];
	sprintf(cmd, "mv %s /home/rcp/xmlstats.xml", fname);
	int v = system(cmd);
	(void) v;
}
