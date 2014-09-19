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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include "services.h"

CliNode *head = NULL;
RcpShm *shm = NULL;
MuxSession mux[MAX_SESSIONS];
static RcpProcStats *pstats = NULL;	// process statistics

static void rcp_print_mux(void) {
#if 0
	int i;
	MuxSession *ptr;
	for (i = 0, ptr =&mux[0]; i < MAX_SESSIONS; i++, ptr++) {
		if (ptr->socket == 0)
			continue;
		
		char buf[200];
		sprintf(buf, "mux: %d/%s/%d", i,  rcpProcName(ptr->connected_process), ptr->socket);
		logDistribute(buf, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
	}
#endif
}

static void rcp_debug_mux(void) {
#if 0
	int i;
	MuxSession *ptr;
	printf("mux: ");
	for (i = 0, ptr =&mux[0]; i < MAX_SESSIONS; i++, ptr++) {
		if (ptr->socket == 0)
			continue;
		
		printf("%d/%s/%d, ", i,  rcpProcName(ptr->connected_process), ptr->socket);
	}
	printf("\nproc: ");
	
		// watchdog timers
	{
		RcpProcStats *ptr;
	
		for (i = 0, ptr = &shm->config.pstats[i]; i < (RCP_PROC_MAX + 2); i++, ptr++) {
			// all running processes
			if (!ptr->proc_type)
				continue;
			
			// but not us or cli processes or test processes
			if (ptr->proc_type == RCP_PROC_CLI ||
			     ptr->proc_type == RCP_PROC_SERVICES ||
			     ptr->proc_type == RCP_PROC_TESTPROC)
				continue;
			
			// started processes
			if (ptr->wproc == 0)
				continue;
	
			printf("%s/%d/%d, ",
				rcpProcName(ptr->proc_type), ptr->wproc, ptr->wmonitor);
	
		}
	}
	printf("\n");
	
#endif
}

static void init_shm(uint32_t process) {
	// initialize parser
	if ((shm = rcpShmemInit(process)) == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize memory, exiting...\n", rcpGetProcName());
		exit(1);
	}
	head = shm->cli_head;
	if (head == NULL) {
		fprintf(stderr, "Error: process %s, memory not initialized, exiting...\n", rcpGetProcName());
		exit(1);
	}

	//***********************************************
	// add CLI commands and grab process stats pointer
	//***********************************************
	// grab semaphore
	sem_t *mutex = rcpSemOpen(RCP_SHMEM_NAME);
	if (mutex == NULL) {
		fprintf(stderr, "Error: process %s, failed to open semaphore, exiting...\n", rcpGetProcName());
		exit(1);
	}
	sem_wait(mutex);
	// init cli commands
	if (module_initialize_commands(head) != 0) {
		fprintf(stderr, "Error: process %s, cannot initialize command line interface\n", rcpGetProcName());
		exit(1);
	}
	// grab proc stats pointer
	pstats = rcpProcStats(shm, process);
	if (pstats == NULL) {
		fprintf(stderr, "Error: process %s, cannot initialize statistics\n", rcpGetProcName());
		exit(1);
	}
	if (pstats->proc_type == 0)
		pstats->proc_type = process;
	pstats->pid = getpid();
	pstats->start_cnt++;
	// close semaphores
	sem_post(mutex);
	sem_close(mutex);
}


int pipecnt = 0;
// sending on the socket might rise SIGPIPE signal
static void sigpipe() {
	// empty
	printf("sigpipe received\n");	
	pipecnt++;
}

static void usage(void) {
	printf("Usage: services <arguments>\n");
	printf("where arguments is:\n");
	printf("\t--help - this help screen\n");
	printf("\t--defaults - set system defaults and exit\n");
	printf("\n");
}

int main(int argc, char **argv) {
	int s;	// socket
	socklen_t len;		
	int maxfd;
	int nready;
	struct sockaddr_un name;
	fd_set fds;


	if (argc != 1) {
		// process arguments
		
		if (strcmp(argv[1], "--help") == 0) {
			usage();
			return 0;
		}
		else if (strcmp(argv[1], "--defaults") == 0) {
			setSystemDefaults();
			return 0;
		}
		
		usage();
		return 1;
	}

//	rcpDebugEnable();

	
	// allocate rcp packet memory
	RcpPkt *pkt;
	pkt = malloc(sizeof(RcpPkt) + RCP_PKT_DATA_LEN);
	if (pkt == NULL) {
		fprintf(stderr, "Error: cannot allocate memory, exiting...\n");
		return 1;
	}

	// initialize shared memory
	init_shm(RCP_PROC_SERVICES);

	// init http authentication
	if (shm->stats.http_auth_initialized == 0) {
		shm->stats.http_auth_initialized = 1;
		http_auth_init();
	}
	

	// init local mux array
	memset(mux, 0, sizeof(mux));
	signal(SIGPIPE, sigpipe);
	
	// Remove previous socket if any
	unlink(RCP_MUX_SOCKET);

	// Create the socket
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error: cannot create communication socket\n");
		exit(1);
	}

	// Create the address of the server
	memset(&name, 0, sizeof(struct sockaddr_un));
	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, RCP_MUX_SOCKET);
	len = sizeof(name.sun_family) + strlen(name.sun_path);

	// Bind the socket to the address
	if (bind(s, (struct sockaddr *) &name, len) < 0) {
		fprintf(stderr, "Error: cannot bind communication socket\n");
		exit(1);
	}
	int v = system("chmod 777 /var/tmp/rcpsocket");
	if (v == -1) {
		fprintf(stderr, "Error: cannot change communication socket attributes\n");
		exit(1);
	}

	// Listen for connections
	if (listen(s, MAX_SESSIONS) < 0) {
		fprintf(stderr, "Error: cannot listen on communication socket\n");
		exit(1);
	}

	// set select data
	maxfd = 0;
	struct timeval ts;
	ts.tv_sec = 1; // 1 second
	ts.tv_usec = 0;

	int fcnt = 0;
	uint32_t rcptic = rcpTic();
	
	// init syslog socket
	syslog_fd = logInitSyslogSocket();
	if (syslog_fd == 0) {
		logStore("cannot initialize syslog socket", RLOG_EMERG, RLOG_FC_IPC, RCP_PROC_SERVICES);
	}
	
	while(1) {
		// debug only
		if ((fcnt % 10) == 0)
			rcp_debug_mux();

		/* Set up polling using select. */
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		maxfd = s;
		int i;
		int connected = 0;
		for (i = 0; i < MAX_SESSIONS; i++) {
			if (mux[i].socket != 0) {
				FD_SET(mux[i].socket, &fds);
				maxfd = (maxfd < mux[i].socket)? mux[i].socket: maxfd;
				connected++;
			}
		}

		// wait for data
		errno = 0;
		nready = select(maxfd + 1, &fds, (fd_set *) 0, (fd_set *) 0, &ts);
//printf("nready %d, errno %d\n", nready, errno);

		// every 50 frames check if we are slipping the clock
		if (++fcnt > 50) {
			fcnt = 0;
			if (rcptic < rcpTic())
				// force a timeout
				nready = 0;
		}
			
				
		if (nready < 0) {
			char tmp[200];
			sprintf(tmp, "select call error %d", errno);
			logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
		}
		else if (nready == 0) {
			// monitor processes every 10 seconds
			if ((rcptic % 10) == 0) {
				rcp_monitor_procs();
				check_canary();
			}
				
			// reset log rate, record max log rate
			if (shm->config.current_rate > shm->config.max_rate)
				shm->config.max_rate = shm->config.current_rate;
			shm->config.current_rate = 0;
			
			// system xinetd update every two seconds at most
			if (update_xinetd_files && (rcptic % 2) == 0)
				updateXinetdFiles();				

			// system ntpd.conf update every two seconds at most
			if (update_ntpd_conf && (rcptic % 2) == 0)
				updateNtpdFile();				

			// system hosts update every two seconds at most
			if (update_hosts_file && (rcptic % 2) == 0)
				updateHostsFile();				

			// system resolvconf update every two seconds at most
			if (update_resolvconf_file && (rcptic % 2) == 0)
				updateResolvconfFile();				

			// manage snmpd process
			if (restart_snmpd > 0) {
				restart_snmpd--;
				if (restart_snmpd == SNMPD_TIMER_KILL)
					kill_snmpd();
				else if (restart_snmpd == SNMPD_TIMER_START)
					start_snmpd();
			}

			// process http authentication ttl
			http_auth_timeout();

			// reload ts
			rcptic++;
			if (rcpTic() > rcptic) {
				// speed up the clock
				ts.tv_sec = 0;
				ts.tv_usec = 800000;	// 0.8 seconds
				pstats->select_speedup++;
			}
			else {
				ts.tv_sec = 1; // 1 second
				ts.tv_usec = 0;
			}
		}
		else if (s != 0 && FD_ISSET(s, &fds)) {
			// Accept another connection
			int n;
			if ((n = accept(s, (struct sockaddr *) &name, &len)) < 0) {
				char tmp[200];
				sprintf(tmp, "accept call error %d", errno);
				logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
				continue;
			}

			int found = 0;
			for (i = 0; i < MAX_SESSIONS; i++) {
				if (mux[i].socket == 0) {
					clean_mux(&mux[i]);
					mux[i].socket = n;
					found = 1;
					char tmp[200];
					sprintf(tmp, "socket %d connected", n);
					logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
					break;
				}
			}
			if (!found) {
				logDistribute("maximum service capacity reached, connection dropped",
					RLOG_WARNING, RLOG_FC_IPC, RCP_PROC_SERVICES);
				close(n);
			}
		}
		else {
			for (i = 0; i < MAX_SESSIONS; i++) {
				if (mux[i].socket != 0 && FD_ISSET(mux[i].socket, &fds)) {
					errno = 0;
					// read the packet header
					int nread = recv(mux[i].socket, pkt, sizeof(RcpPkt), 0);
//printf("sock %d, nread %d, errno %d\n", mux[i].socket, nread, errno); fflush(0);					
					if(nread < sizeof(RcpPkt) || errno != 0) {
						char tmp[500];
						
						// sometimes we get a nread > 0, and errno 104 (connection reseted)
						if (nread != 0) {
							sprintf(tmp, "recv call error %d, closing socket %d, session %d, process %s",
								errno, mux[i].socket, i, rcpProcName(mux[i].connected_process));
							logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						}
						else {
							sprintf(tmp, "socket %d closed by remote end, session %d, process %s", 
								mux[i].socket, i, rcpProcName(mux[i].connected_process));
							logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						}

						if (mux[i].connected_process == RCP_PROC_CLI && strcmp(mux[i].admin, "html")) {
							sprintf(tmp, "administrator %s logged out", mux[i].admin);
							logDistribute(tmp, RLOG_NOTICE, RLOG_FC_ADMIN, RCP_PROC_SERVICES);
						}
							
						close(mux[i].socket);
						clean_mux(&mux[i]);
						rcp_print_mux();
						continue;
					}
					// read the packet data
					if (pkt->data_len != 0) {
						nread += recv(mux[i].socket, (unsigned char *) pkt + sizeof(RcpPkt), pkt->data_len, 0);
					}
					
					ASSERT (nread == (sizeof(RcpPkt) + pkt->data_len));
					if (nread != (sizeof(RcpPkt) + pkt->data_len)) {
						ASSERT(0);
						// todo: error recovery - maybe drop the socket here
					}

					// debug logging for the incoming packet
//					if (pkt->type != RCP_PKT_TYPE_LOG) {
//						char tmp[200];
//						sprintf(tmp, "received %s packet", rcpPktType(pkt->type));
//						logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
//					}
					
					// system updates packet
					if (pkt->type == RCP_PKT_TYPE_UPDATESYS) {
						logDistribute("processing system updates packet", RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						update_xinetd_files = 1;
					}
					// dns updates packet
					else if (pkt->type == RCP_PKT_TYPE_UPDATEDNS) {
						logDistribute("processing dns updates packet", RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						update_resolvconf_file = 1;
					}
					else if (pkt->type == RCP_PKT_TYPE_HTTP_AUTH && pkt->destination == RCP_PROC_SERVICES) {
						http_auth(pkt->pkt.http_auth.ip);
					}
					
					// cli packet for us
					else if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_SERVICES) {
						logDistribute("processing local CLI packet", RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						int rv = processCli(pkt, i);
						
						// respond to the same cli session
						unsigned tmp = pkt->destination;
						pkt->destination = pkt->sender;
						pkt->sender = tmp;
						pkt->pkt.cli.return_value = rv;
						send(mux[i].socket, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
					}
					// registration packet					
					else if (pkt->type == RCP_PKT_TYPE_REGISTRATION && pkt->destination == RCP_PROC_SERVICES) {
						if (pkt->sender == RCP_PROC_CLI) {
							uint8_t *data = (uint8_t *) pkt + sizeof(RcpPkt);
							if (pkt->pkt.registration.offset_tty != 0)
								strncpy((char *) mux[i].ttyname, (char *) (data + pkt->pkt.registration.offset_tty), 								CLI_MAX_TTY_NAME);
							if (pkt->pkt.registration.offset_admin != 0)
								strncpy((char *) mux[i].admin, (char *) (data + pkt->pkt.registration.offset_admin), CLI_MAX_ADMIN_NAME);
							if (pkt->pkt.registration.offset_ip != 0)
								strncpy((char *) mux[i].ip, (char *) (data + pkt->pkt.registration.offset_ip), CLI_MAX_IP_NAME);
							mux[i].start_time = time(NULL);
							
							char tmp[200];
							sprintf(tmp, "CLI session registered on socket %d, tty %s, administrator %s, address %s",
								mux[i].socket, mux[i].ttyname, mux[i].admin, mux[i].ip);
							logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
							if (strcmp(mux[i].admin, "html")) {
								sprintf(tmp, "administrator %s logged in from %s",
									mux[i].admin, mux[i].ip);
								logDistribute(tmp, RLOG_NOTICE, RLOG_FC_ADMIN, RCP_PROC_SERVICES);
							}
						}
						else {
							char tmp[200];
							sprintf(tmp, "%s process registered on socket %d, session %d\n",
								rcpProcName(pkt->sender), mux[i].socket, i);
							logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						}
							
						mux[i].connected_process = pkt->sender;
					}
					// cli packet going to a client
					else if (pkt->type == RCP_PKT_TYPE_CLI && pkt->sender == RCP_PROC_CLI) {
						char buf[200];

						// set the return path
						pkt->pkt.cli.return_path = i;
												
						// look for outgoing socket
						int j;
						for (j = 0; j < MAX_SESSIONS; j++) {
							if (mux[j].connected_process != pkt->destination)
								continue;
							sprintf(buf, "forwarding CLI packet on socket %d, dest %s, session %d",
								mux[j].socket,rcpProcName(pkt->destination), j);
							logDistribute(buf, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
						    	send(mux[j].socket, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
						    	break;
						 }
					}
					// cli packet going to cli
					else if (pkt->type == RCP_PKT_TYPE_CLI && pkt->destination == RCP_PROC_CLI) {
						logDistribute("forwarding CLI response packet", RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);
					    	send(mux[pkt->pkt.cli.return_path].socket, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
					 }			
					// log packet
					else if (pkt->type == RCP_PKT_TYPE_LOG && pkt->destination == RCP_PROC_SERVICES) {
						uint8_t old_level = shm->config.log_level;
						uint64_t old_facility = shm->config.facility;
						shm->config.log_messages++;
						const char *data = (char *) pkt + sizeof(RcpPkt);

						// check facility, level and rate
						shm->config.current_rate++;
						if (rcpLogAccept(shm, pkt->pkt.log.level, pkt->pkt.log.facility)) {
							// send log to cli sessions
							logSendCli(pkt->sender, pkt->pkt.log.facility, data, pkt->pkt.log.level);
						
							// store log in shared memory
							logStore(data, pkt->pkt.log.level, pkt->pkt.log.facility, pkt->sender);
						}
						ASSERT(old_level == shm->config.log_level);
						ASSERT(old_facility == shm->config.facility);
					}
					else {
						char tmp[200];
						sprintf(tmp, "forwarding %s packet to %x", rcpPktType(pkt->type), pkt->destination);
						logDistribute(tmp, RLOG_DEBUG, RLOG_FC_IPC, RCP_PROC_SERVICES);

						// is this packet for us?
						if (pkt->destination & RCP_PROC_SERVICES) {
							if (pkt->type == RCP_PKT_TYPE_IFCHANGED ||
							    pkt->type == RCP_PKT_TYPE_VLAN_DELETED)
								update_ntpd_conf = 2;
							else
								ASSERT(0);
						}						


						// this is a packet going from one client to other clients
						unsigned mask = RCP_PROC_ROUTER;
						int p;
						for (p = 0; p < RCP_PROC_MAX; p++, mask <<= 1) {
							if (mask & pkt->destination) {
								MuxSession *ses = get_mux_process(mask);
								if (ses == NULL)
									continue;

								// send the packet
							    	send(ses->socket, pkt, sizeof(RcpPkt) + pkt->data_len, 0);
							}
								
						}
					}
				} // if
			} // for
		} // else
	}
	
	return 1;
}
