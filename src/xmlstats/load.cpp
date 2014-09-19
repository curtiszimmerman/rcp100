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
#include "vtimestamp.h"
#include "vsingle.h"
#include "vinterface.h"
#include "vhost.h"

void load(void) {
	XMLDocument doc;
	int rv = doc.LoadFile("/home/rcp/xmlstats.xml");
	if (rv) {
		exit(1);
	}
	printf("Loading XML statistics\n");

	int interval = rcpGetStatsInterval();
	unsigned ts_now = (unsigned) time(NULL);

	VTimestamp mytimestamp;
	doc.Accept(&mytimestamp);
	unsigned ts_xml = mytimestamp.getTimestamp();
	if (debug)
		printf("Loading XML statistics timestamped %s\n", ctime((time_t *) &ts_xml));
	unsigned ts_delta = ts_now - ts_xml;
	int interval_delta = ts_delta / (15 * 60); // 15 minutes interval
	
	// if the stats are too old, don't load them
	if (interval_delta < RCP_MAX_STATS) {
		VSingle mycpu(interval, interval_delta, "cpu_load", shm->stats.cpu);
		doc.Accept(&mycpu);
	
		VSingle mymem(interval, interval_delta, "free_mem", shm->stats.freemem);
		doc.Accept(&mymem);
	
		VSingle mydns(interval, interval_delta, "dns_query", shm->stats.dns_q);
		doc.Accept(&mydns);
	
		VSingle mydhcp(interval, interval_delta, "dhcp_relay_rx", shm->stats.dhcp_rx);
		doc.Accept(&mydhcp);
	
		VInterface myinterface(interval, interval_delta);
		doc.Accept(&myinterface);
	
		VHost myhost(interval, interval_delta);
		doc.Accept(&myhost);
	}
}
