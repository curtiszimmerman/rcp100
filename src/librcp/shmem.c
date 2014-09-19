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
#include "librcp.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <semaphore.h>

static void create_shm(const char *fname, size_t size) {
	char *semname = malloc(strlen(fname) + 4 + 1);
	if (semname == NULL) {
		perror("cannot allocate memory");
		ASSERT(0);
		exit(1);
	}
	sprintf(semname, "%s.sem", fname);
	// create the semaphore
  	sem_t *mutex = sem_open(semname,O_CREAT,0644,1);
  	if (mutex == SEM_FAILED) {
		perror("cannot create semaphore");
		ASSERT(0);
		sem_unlink(semname);
		unlink(semname);
		exit(1);
	}

	int fd = shm_open(fname, O_CREAT | O_EXCL | O_RDWR, S_IRWXO | S_IRWXU | S_IRWXG);
	if (fd == -1) {
		perror("cannot create shared memory");
		ASSERT(0);
		unlink(semname);
		exit(1);
	}
	free(semname);
	
	void *ptr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0 );
	if (ptr == NULL) {
		ASSERT(0);
		return;
	}

	// set the size, close, change mode
	int v = ftruncate(fd, size);
	if (v == -1)
		ASSERT(0);
	memset(ptr, 0, size);

	rcpDebug("shm: %s shared memory file of size 0x%x created and initialized\n", fname, size);

	char long_file_name[1024];
	sprintf(long_file_name, "/dev/shm/%s", fname);
	if (0 != chmod(long_file_name, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)) {
		ASSERT(0);
		return;
	}

	sem_close(mutex);
}

void *rcpShmOpen(const char *fname, size_t size, uint64_t start_point, int *already) {
	int fd;
	*already = 1;
	rcpDebug("shm: opening %s shared memory file, address 0x%x\n", fname, start_point);
	
	// open the shared mem object
	fd = shm_open(fname, O_RDWR, S_IRWXU );
	if (fd == -1) {
		// create it and try again
		create_shm(fname, size);
		*already = 0;
		fd = shm_open(fname, O_RDWR, S_IRWXU );
		if (fd == -1) {
			ASSERT(0);
			return NULL;
		}
	}

	// grab a pointer to the shared memory object
	void *ptr = mmap((char *) start_point, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0 );
	if (ptr == NULL)
		return NULL;

	// Set the size of the shared memory object
	int v = ftruncate(fd, size);
	if (v == -1)
		ASSERT(0);

	return ptr;
}

void *rcpSemOpen(const char *fname) {
	char *semname = malloc(strlen(fname) + 4 + 1);
	if (semname == NULL) {
		perror("cannot allocate memory");
		ASSERT(0);
		return NULL;
	}
	sprintf(semname, "%s.sem", fname);
	// create the semaphore
  	sem_t *mutex = sem_open(semname, 0, 0644, 0);
  	free(semname);
  	if (mutex == SEM_FAILED) {
		perror("cannot open semaphore");
		ASSERT(0);
		return NULL;
	}

	return mutex;
};
