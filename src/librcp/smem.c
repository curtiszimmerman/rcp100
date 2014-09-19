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
#include "smem.h"

#ifdef SMEMUTEST
#define rcpDebug printf
#endif

// allocation strategy
#define ALLOC_CIRCULAR
//#define LAST_FREE

// the maximum size of memory that can be allocated is 256 * 16 = 4096 bytes
// minimum amount of memory allocated is 16 bytes
#define ALIGN_SIZE 16	// 16 bits alignment

// allocator bucket - so far, it is using a single bucket split into segments of ALIGN_SIZE
typedef struct {
	int initialized;
	uint8_t *pmem; // cell memory; empty cell = 0; cell in use: number of chunks
	uint32_t *cmem; // chunk memory
	int max_chunks;
	
	int last_allocation;
	int last_free;
	
	// statistics
	int alloc_calls;
	int alloc_calls_failed;
	int free_calls;
	int wrap;
} Bucket;

static inline void print_bucket(Bucket *bkt) {
	printf("Bucket at %p\n", bkt);
	printf("\tpmem %p\n", bkt->pmem);
	printf("\tcmem %p\n", bkt->cmem);
	printf("\tmax_chunks %d\n", bkt->max_chunks);
	printf("\tlast_allocation %d\n", bkt->last_allocation);
	printf("\talloc_calls %d\n", bkt->alloc_calls);
	printf("\talloc_calls_failed %d\n", bkt->alloc_calls_failed);
	printf("\tfree_calls %d\n", bkt->free_calls);
	printf("\twrap %d\n", bkt->wrap);
}
// return the pointer to aligned memory, modify the size accordingly
static inline void *align(void *mem, size_t *size) {
	uint64_t m = (uint64_t) mem;
	if ((m % ALIGN_SIZE) == 0)
		return (void *) m;
		
	*size -= ALIGN_SIZE;
	return (void *) (((m / ALIGN_SIZE) * ALIGN_SIZE) + ALIGN_SIZE);
}

static inline int get_chunks_from_size(size_t size) {
	int chunks;
	if ((size % ALIGN_SIZE) == 0)
		chunks = size / ALIGN_SIZE;
	else
		chunks = (size / ALIGN_SIZE) + 1;

	if (chunks == 0)
		chunks = 1;		

	return chunks;
}

// return 1 if error, 0 if OK
void *smem_initialize(void *mem, size_t size) {
	// clear all memory
	memset(mem, 0, size);

	// align memory
	size_t size1 = size;
	mem = align(mem, &size);
	if (size1 < size) { // wrapping!
		ASSERT(0);
		return 0;
	}
	
	// set the bucket at the top of memory
	Bucket *bkt = (Bucket *) mem;
	
	// second alignment
	mem += sizeof(Bucket);
	size -= sizeof(Bucket);
	mem = align(mem, &size);
	rcpDebug("shm: memory at %p, size 0x%x\n", mem, size);
	
	// calculate the number of chunks
	bkt->max_chunks = size / (sizeof(uint8_t) + ALIGN_SIZE);
	bkt->cmem = (uint32_t *) mem;
	bkt->pmem = (uint8_t *) (bkt->cmem)  + (bkt->max_chunks * ALIGN_SIZE );
	
	rcpDebug("smem_initialize: max_chunks %d, pmem %p, cmem %p\n", bkt->max_chunks, bkt->pmem, bkt->cmem);

	// mark all entries as empty
	int i;
	uint8_t *ptr = bkt->pmem;
	for (i = 0; i < bkt->max_chunks; i++, ptr++)
		*ptr = 0; // empty cell
	
	
	bkt->initialized = 1;
//	print_bucket(bkt);
	return bkt;
}


void smem_print_mem(void *handle) {
	Bucket *pbkt = (Bucket *) handle;
	if (pbkt != NULL) {
		printf("all bucket memory:\n");
		int i;
		uint8_t *ptr = pbkt->pmem;
		uint8_t *realmem = (uint8_t *) pbkt->cmem;
		for (i = 0; i < pbkt->max_chunks; i++, ptr++, realmem += ALIGN_SIZE) {
			printf("%d: %d, real mem %p\n", i, *ptr, realmem);
		}
		printf("***\n");
	}
}

void smem_print_allocated(void *handle) {
	Bucket *pbkt = (Bucket *) handle;
	if (pbkt != NULL) {
		printf("bucket allocated memory:\n");
		int i;
		uint8_t *ptr = pbkt->pmem;
		uint8_t *realmem = (uint8_t *) pbkt->cmem;
		for (i = 0; i < pbkt->max_chunks; i++, ptr++, realmem += ALIGN_SIZE) {
			if (*ptr != 0)
				printf("%d: %d, real mem %p\n", i, *ptr, realmem);
		}
		printf("***\n");
	}
}

static char outbuf[1024]; // buffered output used in printfs
char *smem_print_stats(void *handle) {
	Bucket *pbkt = (Bucket *) handle;
	
	outbuf[0] = '\0';
	
	if (pbkt != NULL) {
		sprintf(outbuf, "address %p, size 0x%x bytes / %dKB\n", pbkt, pbkt->max_chunks * 16, (pbkt->max_chunks * 16) / 1024);
		sprintf(outbuf + strlen(outbuf), "alloc calls %d, alloc failed %d, free calls %d, wrapping %d\n",
			pbkt->alloc_calls, pbkt->alloc_calls_failed, pbkt->free_calls, pbkt->wrap);
		
		// calculate current allocations
		int a = 0;
		int i;
		uint8_t *ptr = pbkt->pmem;
		for (i = 0; i < pbkt->max_chunks; i++, ptr++) {
			if (*ptr != 0)
				a++;
		}
		sprintf(outbuf + strlen(outbuf), "%d blocks active, %d blocks free (%f%%)\n",
			a, pbkt->max_chunks - a,
			(((float) (pbkt->max_chunks - a)) / ((float) pbkt->max_chunks)) * 100);
	}
	return outbuf;
}

int smem_get_alloc_free(void *handle) {
	Bucket *pbkt = (Bucket *) handle;
	if (pbkt != NULL)
		return pbkt->alloc_calls + pbkt->free_calls;

	return 0;
}

void *rcpAlloc(void *handle, size_t size) {
	rcpDebug("shm: allocating shm memory size %d\n", size);

	Bucket *pbkt = (Bucket *) handle;
	if (pbkt == NULL)
		return NULL;

	int chunks = get_chunks_from_size(size);


	int start_walk = 0;
#ifdef ALLOC_CIRCULAR
	start_walk = pbkt->last_allocation;
#endif
#ifdef LAST_FREE
	if (*(pbkt->pmem + pbkt->last_free) == 0)
		start_walk = pbkt->last_free;
	else
		start_walk = pbkt->last_allocation;
#endif

	uint8_t *entry = pbkt->pmem;
	entry += start_walk;

	// find the first empty chunk;
	int i;
	for (i = start_walk; i < pbkt->max_chunks - chunks; i++, entry++) {
		if (*entry == 0) {
			// found an empty chunk
			int empty = 1;
			int end = i + chunks;
			for (; i < end; i++, entry++) {
				if (*entry != 0) {
					empty = 0;
					break;		
				}
			}
			
			if (empty == 1) {
				entry -= chunks;
				int index = i - chunks;
				pbkt->last_allocation = index;
				int size = chunks;
				// found enough empty chunks
				for (i = 0; i < chunks; i++, entry++, size--) {
					*entry = size;
				}
				pbkt->alloc_calls++;
				rcpDebug("shm: allocated %p\n", pbkt->cmem + (index * ALIGN_SIZE / 4));				

				return pbkt->cmem + (index * ALIGN_SIZE / 4);
			}
		}
		else {
			i += *entry - 1;
			entry += *entry - 1;
		}
	}
	
	if (start_walk != 0) {
		pbkt->last_allocation = 0;
		pbkt->last_free = 0;
		pbkt->wrap++;
		return rcpAlloc(handle, size);
	}
	
	pbkt->alloc_calls_failed++;	 
	return NULL;
}		
			
void rcpFree(void *handle, void *mem) {
	Bucket *pbkt = (Bucket *) handle;
	if (pbkt == NULL)
		return;

	// find the pointer
	uint64_t sm = (uint64_t) pbkt->cmem;
	uint64_t rm = (uint64_t) mem;
	int delta = (rm - sm) / ALIGN_SIZE;
	if (delta < 0 || delta >= pbkt->max_chunks) {
		ASSERT(0);
		return;
	}

	// first cell should not be 0
	if (*(pbkt->pmem + delta) == 0) {
		ASSERT(0);
		return;
	}
	
	pbkt->last_free = delta;
	int size = *(pbkt->pmem + delta);
	int i;
	for (i = 0; i < size; i++)
		*(pbkt->pmem + delta + i) = 0;
	pbkt->free_calls++;
}

#ifdef SMEMUTEST
#define TEST_SIZE 1024

int main(int argc, char **argv) {
	char *mem = malloc(TEST_SIZE * 1024);
	void *handle = smem_initialize(mem, TEST_SIZE * 1024);
	if (handle == 0) {
		printf("Cannot initialize the small memory allocator\n");
		return 1;
	}
	char *chunks[1024];

	unsigned end_time = 0;
	if (argc > 1)
		end_time = (unsigned) time(NULL) + atoi(argv[1]);

	int i;
	for (i = 0; i < 1024; i++)
		chunks[i] = NULL;

	while (1) {
		if (end_time != 0) {
			unsigned current = (unsigned) time(NULL);
			if (current > end_time)
				break;
		}
			
		// allocate some memory
		for (i = 0; i < 1024; i++) {
			if (chunks[i] != NULL)
				continue;
			int size = rand() % 1024;
			chunks[i] = rcpAlloc(handle, size);
			if (chunks[i] == NULL) {
//				printf("******MEMFULL, i = %d\n", i);
				break;
			}
//			else
//				printf("allocated buf %d at %p\n", i, chunks[i]);
		}

//		print_mem();
		
		// free some slots
		int some = rand() % 64;
		some++;
//printf("*********some %d\n", some);
		for (i = 0; i < 1024; i++) {
			if ((i % some) == 0 && chunks[i] != NULL) {
//				printf("*********free buf %d at %p\n", i, chunks[i]);
				rcpFree(handle, chunks[i]);
				chunks[i] = NULL;
			}
		}


//		print_allocated();
	}
	
	// deallocate all memory
	for (i = 0; i < 1024; i++) {
		if (chunks[i] != NULL) {
			rcpFree(handle, chunks[i]);
			chunks[i] = NULL;
		}
	}


	smem_print_stats(handle);
	int cnt = smem_get_alloc_free(handle);
	cnt /= atoi(argv[1]);
	printf("%d alloc/free calls per second\n", cnt);
	
	free(mem);	
	return 0;
}

#endif
