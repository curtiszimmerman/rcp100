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
#include "mgmt.h"
#include <sys/types.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <sys/ioctl.h>

#if 0
void dbg_print(unsigned *stats) {
	int i;
	printf("\n");
	for (i = 0; i < RCP_MAX_STATS; i++, stats++)
		printf("%u, ", *stats);
	printf("\n");

}
#endif



static void process_interfaces(void) {
	int i;

	// get current interface counts	
	RcpIfCounts *ifcounts = rcpGetInterfaceCounts();
	
	// calculate interval bit/s
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;
			
		// find current counts
		RcpIfCounts *ptr = ifcounts;
		RcpIfCounts *found = NULL;
		while (ptr) {
			if (strcmp(intf->name, ptr->name) == 0) {
				found = ptr;
				break;
			}
			ptr = ptr->next;
		}
		if (!found) {
			intf->stats.current_rx_bytes = 0;
			intf->stats.current_tx_bytes = 0;
		}
		else {
			// tx
			long long int kbytes = found->tx_bytes -  intf->stats.current_tx_bytes;
			kbytes /= 1000;
			float kbps = ((float) kbytes / (15 * 60)) * 8;
			intf->stats.tx_kbps[stats_interval] = (uint32_t) kbps;
			intf->stats.current_tx_bytes = found->tx_bytes;
		
			// rx
			kbytes = found->rx_bytes - intf->stats.current_rx_bytes;
			kbytes /= 1000;
			kbps = ((float) kbytes / (15 * 60)) * 8;
			intf->stats.rx_kbps[stats_interval] = (uint32_t) kbps;
			intf->stats.current_rx_bytes = found->rx_bytes;
		}
	}
	
	while (ifcounts) {
		RcpIfCounts *next = ifcounts->next;
		free(ifcounts);
		ifcounts = next;
	}
}


static void process_dns(void) {
	long long int delta = (long long int) shm->stats.dns_queries - (long long int) shm->stats.dns_last_q;
	if (delta < 0)
		delta *= -1;
	
	shm->stats.dns_q[stats_interval] = (unsigned) delta;
	shm->stats.dns_last_q = shm->stats.dns_queries;
}

static void process_dhcp(void) {
	// calculate current rx packets
	long long current = 0;
	int i;
	for (i = 0; i < RCP_INTERFACE_LIMITS; i++) {
		RcpInterface *intf = &shm->config.intf[i];
		if (intf->name[0] == '\0')
			continue;
		current += intf->stats.dhcp_rx;
	}

	
	long long int delta = (long long int) current - (long long int) shm->stats.dhcp_last_rx;
	if (delta < 0)
		delta *= -1;
	
	shm->stats.dhcp_rx[stats_interval] = (unsigned) delta;
	shm->stats.dhcp_last_rx = (unsigned) current;
}

static void process_cpuload(void) {
	// extract cpu load by reading /proc/loadavg
	FILE *fp = fopen("/proc/loadavg", "r");
	char buf[100 + 1];
	if (fp && fgets(buf, 100, fp)) {
		float tmp1;
		float tmp2;
		float tmp3;
		sscanf(buf, "%f %f %f", &tmp1, &tmp2, &tmp3);
		fclose(fp);
		shm->stats.cpu[stats_interval] = (unsigned) (tmp3 * 100) / nprocs;
	}
}

static void process_freemem(void) {
	// extract free memory
	FILE *fp;
	if ((fp = fopen("/proc/meminfo", "r")) != NULL) {
		char line[100 + 1];
		int memtotal = 0;
		int memfree = 0;
		int memcached = 0;
		int swaptotal = 0;
		int swapfree = 0;
		
		while (fgets(line, 100, fp) != NULL) {
			if (strncmp(line, "MemTotal:", 9) == 0)
				sscanf(line + 9, "%d", &memtotal);
			else if (strncmp(line, "MemFree:", 8) == 0)
				sscanf(line + 8, "%d", &memfree);
			else if (strncmp(line, "Cached:", 7) == 0)
				sscanf(line + 7, "%d", &memcached);
			else if (strncmp(line, "SwapTotal:", 10) == 0)
				sscanf(line + 10, "%d", &swaptotal);
			else if (strncmp(line, "SwapFree:", 9) == 0)
				sscanf(line + 9, "%d", &swapfree);
			
		}
		// add free and cached memory
		memfree += memcached;
		double mf = ((double) memfree / (double) memtotal) * 100;
		fclose(fp);
		shm->stats.freemem[stats_interval] = (unsigned) (mf);
	}
}

static void process_nemon(void) {
	RcpNetmonHost *ptr;
	int i;

	for (i = 0, ptr = shm->config.netmon_host; i < RCP_NETMON_LIMIT; i++, ptr++) {
		if (!ptr->valid)
			continue;
			
		ptr->rx[stats_interval] = ptr->interval_pkt_received;
		ptr->interval_pkt_received = 0;	
		ptr->tx[stats_interval] = ptr->interval_pkt_sent;
		ptr->interval_pkt_sent = 0;
		
		if (ptr->resptime_samples != 0) {
			ptr->resptime[stats_interval] = ptr->resptime_acc / ptr->resptime_samples;
		}
		ptr->resptime_acc = 0;
		ptr->resptime_samples = 0;
	}
}

void process_stats(void) {
	process_cpuload();
	process_freemem();
	process_interfaces();
	process_dns();
	process_dhcp();
	process_nemon();
}

