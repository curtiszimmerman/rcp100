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
#ifndef LIBRCP_H
#define LIBRCP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//*************
// errors and return values
//*************
// no error is always 0
#define RCPERR  1				  	  // generic error
#define RCPERR_MALLOC 2				  // memory allocation failed
#define RCPERR_MEM_CORRUPTED 3			  // memory corruption
#define RCPERR_EXTERNAL_PROG 4			  // failed to run an external program
#define RCPERR_FUNCTION_NOT_FOUND 5		  // failed to find the function associated with a node

//*************************************************************************************
// assertions
//*************************************************************************************
#define ASSERT(X) \
if (!(X)) \
printf("\033[31mAssertion %s:%s:%d\033[0m\n", \
rcpGetProcName(), __FILE__, __LINE__);

/* Implementation map:

ibrcp_proc.h		proc.c, crash.c, debug.c
librcp_interface.h		interface.c
librcp_route.h		kernel.c
librcp_ip.h		inline implementation
librcp_log.h		log.c
librcp_mux.h		mux.c
librcp_crypt.h		crypt.c, md5.c

cli parser and shared memory: parser_add.c, parser_find.c, parser_special.c, exec_proc.c, smem.c, shmem.c
*/

#ifdef __cplusplus
}
#endif

#include "librcp_snmp.h"
#include "librcp_ip.h"
#include "librcp_crypt.h"
#include "librcp_route.h"
#include "librcp_interface.h"
#include "librcp_mux.h"

#endif						  //LIBRCP_H
