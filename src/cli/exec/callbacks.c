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
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cli.h"
#include "../../../version.h"

extern char *smem_print_stats(void *handle);

// callbacks
int cliLogoutCmd(CliMode mode, int argc, char **argv) {
	exit(0);
}


int cliExitCmd(CliMode mode, int argc, char **argv) {
	if (csession.cmode == CLIMODE_LOGIN || csession.cmode == CLIMODE_ENABLE)
		exit(0);
	csession.cmode = cliPreviousMode(csession.cmode);
	csession.mode_data = NULL;
	return 0;
}


int cliDebugSessionCmd(CliMode mode, int argc, char **argv) {
	if (strcmp(argv[0], "no") == 0)
		rcpDebugDisable();
	else
		rcpDebugEnable();
	return 0;
}


int cliDebugPrintTreeCmd(CliMode mode, int argc, char **argv) {
	cliParserPrintNodeTree(0, head);
	return 0;
}



int cliCopyCmd(CliMode mode, int argc, char **argv) {
	char buf[strlen(argv[1]) + strlen(argv[2]) + 20];

	sprintf(buf, "cp %s %s", argv[1], argv[2]);
	char *rv = cliRunProgram(buf);
	if (rv != NULL)
		free(rv);
	
	return 0;
}

int cliCopyFtpCmd(CliMode mode, int argc, char **argv) {
	char cmd[strlen(argv[1]) + strlen(argv[2]) + 500];

	struct stat s;
	char *curl = "curl";
	if (stat("/opt/rcpcurl/bin/curl", &s) == 0)
		curl = "/opt/rcpcurl/bin/curl";
	
	if (strncmp(argv[1], "ftp://", 6) == 0)
		// download
		sprintf(cmd, "%s --connect-timeout 10 -o %s %s\n", curl, argv[2], argv[1]);
	else
		// upload
		sprintf(cmd, "%s --connect-timeout 10 -T %s %s\n", curl, argv[1], argv[2]);

	rcpDebug("executing command #%s#\n", cmd);
	char *rv = cliRunProgram(cmd);
	if (rv != NULL)
		free(rv);
	
	return 0;
}


int cliCopyTftpCmd(CliMode mode, int argc, char **argv) {
	char cmd[strlen(argv[1]) + strlen(argv[2]) + 500];

	struct stat s;
	char *curl = "curl";
	if (stat("/opt/rcpcurl/bin/curl", &s) == 0)
		curl = "/opt/rcpcurl/bin/curl";

	if (strncmp(argv[1], "tftp://", 7) == 0)
		// download
		sprintf(cmd, "%s --connect-timeout 10 -o %s %s\n", curl, argv[2], argv[1]);
	else
		// upload
		sprintf(cmd, "%s --connect-timeout 10 -T %s %s\n", curl, argv[1], argv[2]);

	rcpDebug("executing command #%s#\n", cmd);
	char *rv = cliRunProgram(cmd);
	if (rv != NULL)
		free(rv);
	
	return 0;
}

int cliDeleteCmd(CliMode mode, int argc, char **argv) {
	unlink(argv[1]);
	return 0;
}

int cliShowVersionCmd(CliMode mode, int argc, char **argv) {
	FILE *fp;

	char *product = "RCP100";
	
	// print software version
	cliPrintLine("%s software version %s\n", product, RCP_VERSION);
	
	// print installed modules
	int i;
	for (i = 0; i < RCP_MODULE_LIMIT; i++) {
		if (shm->config.module[i].name[0] != '\0')
			cliPrintLine("%s Module version %s\n",
				shm->config.module[i].name, 
				shm->config.module[i].version);
	} 
		
	
	// print kernel version
	char *rv = cliRunProgram("uname -mrs");
	if (rv != NULL) {
		cliPrintLine("Kernel version: %s", rv);
		free(rv);
	}

	// print uptime
	if ((fp = fopen("/proc/uptime", "r")) != NULL) {
		// first number is the number of seconds the system has been up and running
		int total, days, hours, seconds, minutes;
		if (fscanf(fp, "%d.", &total) == 1) {
			seconds = total % 60;
			total /= 60; minutes = total % 60;
			total /= 60; hours = total % 24;
			days = total / 24;
			cliPrintLine("System uptime is %d days, %d hours %d minutes %d seconds\n", days, hours, minutes, seconds);
		}
		fclose(fp);
	}

	// print disk stats
	struct statvfs fiData;
	if ((statvfs("/",&fiData)) == 0 ) {
		double kb = fiData.f_bsize / 1024;
		double gtotal = (((double) fiData.f_blocks * kb) / 1024) / 1024;
		double gfree = (((double) fiData.f_bfree * kb) / 1024) / 1024;
		cliPrintLine("%.02fG total disk storage, free %.02fG\n", gtotal, gfree);
	}

	// get RAM data
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
		cliPrintLine("%dM total memory, free %dM (%.2f\%)\n",
			memtotal / 1024, memfree / 1024, ((double) memfree / (double) memtotal) * 100);
		cliPrintLine("%dM swap total memory, free %dM (%.2f\%)\n",
			swaptotal / 1024, swapfree / 1024, ((double) swapfree / (double) swaptotal) * 100);
		fclose(fp);
	}

	return 0;
}

int cliShowClockCmd(CliMode mode, int argc, char **argv) {
	char *rv = cliRunProgram("date");
	if (rv != NULL) {
		cliPrintLine(rv);
		free(rv);
	}
	
	return 0;
}

int cliDirCmd(CliMode mode, int argc, char **argv) {
	char *rv = cliRunProgram("ls -lR");
	if (rv != NULL) {
		char *start = rv;
		char *ptr = rv;
		while ((ptr = strchr(start, '\n')) != NULL) {
			*ptr = '\0';
			cliPrintLine(start);
			start = ptr + 1;
		}
		cliPrintLine(start);
		free(rv);
	}
		
	return 0;
}

// return 1 if the process was found
int shm_process(pid_t pid) {
	int i;
	for (i = 0; i < (RCP_PROC_MAX + 2); i++) {
		if (shm->config.pstats[i].pid == pid)
			return 1;
	}
	
	return 0;
}

int cliShowProcessCmd(CliMode mode, int argc, char **argv) {
	// show process stats
	if (argc == 3 && strcmp(argv[2], "statistics") == 0) {
		cliPrintLine("%-15.15s %-10.10s %-10.10s %-10.10s %-10.10s",
			"Process", "PID", "Start", "Reconnect", "Speed-up");
		
		int i;
		for (i = 0; i < (RCP_PROC_MAX + 2); i++) {
			if (shm->config.pstats[i].proc_type != 0) {
				// extract process stats
				const char *proc_name = rcpProcExecutable(shm->config.pstats[i].proc_type);
				char pid[20];
				snprintf(pid, 19, "%u", shm->config.pstats[i].pid);
				char start[20];
				snprintf(start, 19, "%u", shm->config.pstats[i].start_cnt);
				char reconnect[20];				
				snprintf(reconnect, 19, "%u", shm->config.pstats[i].mux_reconnect);
				char speedup[20];
				snprintf(speedup, 19, "%u", shm->config.pstats[i].select_speedup);
				
				// print it on cli screen
				cliPrintLine("%-15.15s %-10.10s %-10.10s %-10.10s %-10.10s",
					proc_name, pid, start, reconnect, speedup);
			}
		}
				
		return 0;	
	}

	// show process
	char *rv = cliRunProgram("top -b -n 1");
	if (rv != NULL) {
		char *start = rv;
		char *ptr = rv;
		while ((ptr = strchr(start, '\n')) != NULL) {
			*ptr = '\0';

			// remove % char
			char *ptr2 = start;
			while (*ptr2 != '\0') {
				if (*ptr2 == '%')
					*ptr2 = ' ';
				ptr2++;
			}
			
			// print lines
			if (strstr(start, "PID"))
				// print table header
				cliPrintLine(start);




			else if ( // print process line
			             strstr(start, "Cpu(s)") == NULL &&
			             strstr(start, "top - ") == NULL &&
			             strstr(start, "Tasks:") == NULL &&
			             strstr(start, "Mem:") == NULL &&
			             strstr(start, "Swap:") == NULL) {
			             

				// xinetd process
			    	if (strstr(start, " xinetd"))
					cliPrintLine(start);
				// telnet server
				else if (strstr(start, " in.telnetd"))
					cliPrintLine(start);
				// ftp server
				else if (strstr(start, " pure-ftpd"))
					cliPrintLine(start);
				// ssh server
				else if (strstr(start, " sshd"))
					cliPrintLine(start);
				// tftp server
				else if (strstr(start, " tftpd"))
					cliPrintLine(start);
				// ntpd server
				else if (strstr(start, " ntpd"))
					cliPrintLine(start);
				// snmpd
				else if (strstr(start, " snmpd"))
					cliPrintLine(start);
				// cli session
				else if (strstr(start, " cli"))
					cliPrintLine(start);
			                // processes with a name starting with "rcp"
			    	else if (strstr(start, " rcp") && strstr(start, "top") == NULL) {
			    		// monitoring processes are not printed
			    		pid_t pid;
			    		sscanf(start, "%d", &pid);
			    		if (shm_process(pid))
						cliPrintLine(start);
				}
			}
			start = ptr + 1;
		}
		cliPrintLine(start);
		free(rv);
	}
	return 0;
}


int cliDebugPrintCliMemCmd(CliMode mode, int argc, char **argv) {
	printf("Shared memory file: /dev/shm/%s, size %lu bytes\n\n", RCP_SHMEM_NAME, RCP_SHMEM_SIZE);
	
	cliPrintLine("CLI:\n");
	cliPrintBuf(smem_print_stats(shm->cli_smem_handle));
	
	cliPrintLine("Logger:\n");
	cliPrintBuf(smem_print_stats(shm->log_smem_handle));
	
	return 0;
}

int cliPingCmd(CliMode mode, int argc, char **argv) {
	// execute ping command
	char bigbuf[strlen(argv[1]) + 50];
	sprintf(bigbuf, "ping -c 5 %s", argv[1]);
	int v = system(bigbuf);
	if (v == -1)
		ASSERT(0);
	return 0;
}

int cliTelnetCmd(CliMode mode, int argc, char **argv) {
	// execute telnet command
	char bigbuf[strlen(argv[1]) + 50];
	cliRestoreTerminal();
	if (argc == 2)
		sprintf(bigbuf, "/opt/rcp/bin/telnet %s", argv[1]);
	else {
		int port = atoi(argv[3]);
		sprintf(bigbuf, "/opt/rcp/bin/telnet %s %d", argv[1], port);
	}
	int v = system(bigbuf);
	if (v == -1)
		ASSERT(0);
	cliSetTerminal();

	return 0;
}

int cliTracerouteCmd(CliMode mode, int argc, char **argv) {
	// execute traceroute command
	char bigbuf[strlen(argv[1]) + 50];
	sprintf(bigbuf, "traceroute %s", argv[1]);
	int v = system(bigbuf);
	if (v == -1)
		ASSERT(0);
	return 0;
}

int cliShowServicesCmd(CliMode mode, int argc, char **argv) {
	cliPrintLine("%-20s%-15sConnections\n", "Service", "Status");
	
	// tftp
	if (shm->config.services & RCP_SERVICE_TFTP)
		cliPrintLine("%-20s%-15s%d\n", "TFTP", "enabled", shm->stats.tftp_rx);
		
	// telnet
	if (shm->config.services & RCP_SERVICE_TELNET)
		cliPrintLine("%-20s%-15s%d\n", "telnet", "enabled", shm->stats.telnet_rx);

	// http
	if (shm->config.services & RCP_SERVICE_HTTP)
		cliPrintLine("%-20s%-15s%d\n", "http", "enabled", shm->stats.http_rx);

	// ftp
	if (shm->config.services & RCP_SERVICE_FTP)
		cliPrintLine("%-20s%-15s%d\n", "FTP", "enabled", shm->stats.ftp_rx);
	
	return 0;
}

