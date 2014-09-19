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
#ifndef SMEM_H
#define SMEM_H
#include <stddef.h>



// initialize memory, returns a handle to memory, 0 if failed
extern void *smem_initialize(void *mem, size_t size);

// alloc and free
extern void *rcpAlloc(void *handle, size_t size);
extern void rcpFree(void *handle, void *mem);

// debug functions
extern void smem_print_mem(void *handle);
extern void smem_print_allocated(void *handle);
extern char *smem_print_stats(void *handle);
extern int smem_get_alloc_free(void *handle);

#endif
