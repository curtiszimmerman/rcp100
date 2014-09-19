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
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <dlfcn.h>
 #include <signal.h>
 #include "cli.h"
#include "librcp_proc.h"
#include "librcp_mux.h"

CliNode *head = NULL;
RcpShm *shm = NULL;
CliSession csession;
int cli_no_more = 0;

// run at exit time, restoring the terminal, killing child processes, etc
void myexit(void) {
	cliRestoreTerminal();
}

static void sigpipe(int a) {
	if (muxsocket)
		close(muxsocket);
	muxsocket = 0;
}
	
int main(int argc, char **argv, char **envp) {
	{ // if the system is not started, just exit here!!!
		struct stat s;
		if (stat("/dev/shm/rcp", &s) == -1) {
			fprintf(stderr, "Error:  RCP router not started, exiting...\n");
			return 1;
		}
	}

	int i;
	signal(SIGPIPE, sigpipe);
	rcpInitCrashHandler();

	// clear the session
	memset(&csession, 0, sizeof(csession));
	csession.term_row = 24;
	csession.term_col = 80;
	// set terminal
	cliSetTerminal();
	cliUpdateTerminalSize();
	atexit(myexit);
	
	// initialize parser
	if ((shm = rcpShmemInit(RCP_PROC_CLI)) == NULL) {
		fprintf(stderr, "Error: cannot initialize memory, exiting...\n");
		return 1;
	}

	// extract user name
	char *user = getenv("RCPUSER");
	
	// extract user IP address
	char *connection = getenv("SSH_CONNECTION");

	// check user
	uid_t uid = getuid();
	uid_t euid = geteuid();
	struct passwd *pwd = getpwnam("rcp");
	if (!pwd) {
		printf("Error: cannot authenticate user\n");
		exit(1);
	}


//printf("uid %d, euid %d, pwd %p\n", uid, euid, pwd);
	// verify uid & euid
	// root loging in by running /opt/rcp/bin/cli
	if (uid == 0 && euid == 0) {
		// drop root privileges
		initgroups(pwd->pw_name, pwd->pw_gid);
		setgid(pwd->pw_gid);
		setuid(pwd->pw_uid);
	
		seteuid(0);   /* this should fail */
		if (!getuid() || !geteuid())
			exit(1);
		user = "root";
	}
	// anything else shuld be user rcp by now
	else {
		if (uid != pwd->pw_uid || euid != pwd->pw_uid) {
			sleep(5);
			exit(1);
		}

	}		

	// force a directory change
	if (chdir("/home/rcp") != 0)
		exit(1);

	if (!user)
		user = "root";
	if (!connection)
		connection = "localhost";

	csession.cli_user = malloc(strlen(user) + 1);
	if (csession.cli_user != NULL)
		strcpy(csession.cli_user, user);
		
	csession.cli_user_ip = malloc(strlen(connection) + 1);
	if (csession.cli_user_ip != NULL) {
		strcpy(csession.cli_user_ip, connection);
		
		// clear the port number if present
		char *ptr = strchr(csession.cli_user_ip, ' ');
		if (ptr != NULL)
			*ptr = '\0';
	}

	{ // log the request in debug mode
		int len = 0;

		// log environment
		char *cmd = malloc(strlen(user) + strlen(connection) + 100);
		if (cmd != NULL) {
			// build the log command
			strcpy(cmd, "/opt/rcp/bin/rcplog 7 \" login ");
			strcat(cmd, user);
			strcat(cmd, ", ");
			strcat(cmd, connection);
			strcat(cmd, "\"");
			
			// send the log
			int v = system(cmd);
			if (v == -1)
				ASSERT(0);
			free(cmd);
		}
			

		// log user ids
		cmd = malloc(200);
		if (cmd != NULL) {		
			// build the log command
			char uid[20];
			sprintf(uid, "%d", getuid());
			char euid[20];
			sprintf(euid, "%d", geteuid());
			
			strcpy(cmd, "/opt/rcp/bin/rcplog 7 \"login uid ");
			strcat(cmd, uid);
			strcat(cmd, " euid ");
			strcat(cmd, euid);
			strcat(cmd, "\"");
			
			// send the log
			int v = system(cmd);
			if (v == -1)
				ASSERT(0);
			free(cmd);
		}
		
		
		// log arguments
		for (i = 0; i < argc; i++)
			len += strlen(argv[i]);
		
		cmd = malloc(len + i + 100);
		if (cmd != NULL) {
			// build the log command
			strcpy(cmd, "/opt/rcp/bin/rcplog 7 \"login cli arguments: ");
			for (i = 0; i < argc; i++) {
				strcat(cmd, argv[i]);
				strcat(cmd, " ");
			}
			strcat(cmd, "\"");

			// send command to log
			int v = system(cmd);
			if (v == -1)
				ASSERT(0);
			free(cmd);
		}
		
		// log current directory
		char *current_dir = get_current_dir_name();
		if (current_dir) {
			cmd = malloc(strlen(current_dir) + 100);
			if (cmd != NULL) {
				// build the log command
				sprintf(cmd, "/opt/rcp/bin/rcplog 7 \"login directory: %s\"", current_dir);
				// send command to log
				int v = system(cmd);
				if (v == -1)
					ASSERT(0);
				free(cmd);
			}
		}
	}	


	// parse command line arguments
	int runscript = 0;
	int runcommand = 0;
	char *commandptr = NULL;
	char *runfile = NULL;
	int runfile_remove = 0;
	int run_config = 0;
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			printf("Usage: /opt/rcp/bin/cli [options]\n");
			printf("Options:\n");
			printf("\t--script - run script in config mode\n");
			printf("\t--command - run command and exit\n");
			printf("\t--html - the session is in use by http server\n");
			printf("\n");
			exit(0);
		}
		else if (strcmp(argv[i], "--script") == 0) {
			runscript = 1;
			if (i + 1 >= argc) {
				fprintf(stderr, "Error: file name not specified\n");
				exit(1);
			}
			runfile = argv[i + 1];
			run_config = 1;
			cli_no_more = 1;
			break;
		}
		else if (strcmp(argv[i], "--command") == 0) {
			if (argc <= (i + 1)) {
				fprintf(stderr, "Error: incomplete command\n");
				exit(1);
			}
			runcommand = 1;
			commandptr = argv[i + 1];
			i++;
			cli_no_more = 1;
		}		
		else if (strcmp(argv[i], "--html") == 0) {
			csession.html = 1;
			
			// switch the user to html
			if (csession.cli_user)
				free(csession.cli_user);
			user = "html";
			csession.cli_user = malloc(strlen(user) + 1);
			if (csession.cli_user != NULL)
				strcpy(csession.cli_user, user);
			cli_no_more = 1;
		}
		else if (strcmp(argv[i], "-c") == 0) {
			if (argc <= (i + 1)) {
				fprintf(stderr, "Error: incomplete command\n");
				exit(1);
			}


			// handle rcpsec redirection
			struct stat s;
			if (stat("/opt/rcp/bin/plugins/librcpsec.so", &s) == 0) {
				void *lib_handle;
				int (*fn)(RcpShm *shm, int index, int argc, char **argv, char **envp);
				lib_handle = dlopen("/opt/rcp/bin/plugins/librcpsec.so", RTLD_LAZY);
				if (lib_handle) {
					fn = dlsym(lib_handle, "rcpsec_redirect");
					if (fn)
						(*fn)(shm, i, argc, argv, envp);
					else
						ASSERT(0);
					dlclose(lib_handle);
				}
				else
					ASSERT(0);
			}

			// open a temporary file
			char *fname = rcpGetTempFile();
			if (fname == NULL) {
				fprintf(stderr, "Error: cannot run command\n");
				exit(1);
			}
			
			FILE *fp = fopen(fname, "w");
			if (fp == NULL) {
				fprintf(stderr, "Error: cannot run command\n");
				free(fname);
				exit(1);
			}
			
			// store the cli command in the temporary file
			int j;
			for (j = i + 1; j < argc; j++) {
				// "&&" string is used to separate commands
				char *p = argv[j];
				while ((p = strstr(p, "&&")) != NULL) {
					*p++ = ' ';
					*p++ = '\n';
				}
				fprintf(fp, "%s ", argv[j]);
			}
			fprintf(fp, "\n");
			fclose(fp);
			
			// pass it forward as a run script
			runscript = 1;
			runfile = fname;
			runfile_remove = 1;
			cli_no_more = 1;
			break;
		}
	}
	
	// initialize cli
	head = shm->cli_head;
	if (head == NULL) {
		fprintf(stderr, "Error: memory not initialized, exiting...\n");
		return 1;
	}
	
	// add CLI-specific commands
	if (module_initialize_commands(head) != 0) {
		fprintf(stderr, "Error: cannot initialize command line interface\n");
		exit(1);
	}
	cliParserAddOM();

	// connect multiplexer
	if (isatty(0))
		muxsocket = rcpConnectMux(RCP_PROC_CLI, ttyname(0), csession.cli_user, csession.cli_user_ip);
	else
		muxsocket = rcpConnectMux(RCP_PROC_CLI, NULL, csession.cli_user, csession.cli_user_ip);
	
	csession.cmode = CLIMODE_LOGIN;
	if (runscript) {
		int rv = 0;
		ASSERT(runfile != NULL);
		if (run_config)
			csession.cmode = CLIMODE_CONFIG;

		if (cliRunScript(runfile)) {
			fprintf(stderr, "Error: cannot run script %s\n", runfile);
			rv = 1;
		}

		if (runfile_remove)
			unlink(runfile);
		return rv;
	}
	else if (runcommand) {
		return cli_process_cmd(commandptr, 0, 1);
	}
	
	// going into processing loop...
	csession.cmode = CLIMODE_LOGIN;
	cliMainLoop();
	
	return 0;
}
