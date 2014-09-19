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
#include "../../cli/exec/cli.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void configIpsecTun(FILE *fp, int no_passwords) {
	// handle rcpsec configuration
	struct stat s;
	if (stat("/opt/rcp/bin/plugins/librcpsec.so", &s) == 0) {
		void *lib_handle;
		int (*fn)(RcpShm*, FILE*, int);
		lib_handle = dlopen("/opt/rcp/bin/plugins/librcpsec.so", RTLD_LAZY);
		if (lib_handle) {
			fn = dlsym(lib_handle, "rcpsec_cfg2");
			if (fn)
				(*fn)(shm, fp, no_passwords);
			else
				ASSERT(0);
			dlclose(lib_handle);
		}
		else
			ASSERT(0);
	}
}



