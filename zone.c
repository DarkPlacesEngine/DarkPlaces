/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// Z_zone.c

#include "quakedef.h"

#if MEMPARANOIA
#ifdef _MSC_VER
#include <vadefs.h>
#else
#include <stdint.h>
#endif
#define MEMHEADER_SENTINEL_FOR_ADDRESS(p) ((sentinel_seed ^ (unsigned int) (uintptr_t) (p)) + sentinel_seed)
unsigned int sentinel_seed;
#else
#define MEMHEADER_SENTINEL1 0xDEADF00D
#define MEMHEADER_SENTINEL2 0xDF
#endif

#if MEMCLUMPING
#define MEMCLUMP_SENTINEL 0xABADCAFE
#endif

cvar_t developer_memory = {0, "developer_memory", "0", "prints debugging information about memory allocations"};
cvar_t developer_memorydebug = {0, "developer_memorydebug", "0", "enables memory corruption checks (very slow)"};

mempool_t *poolchain = NULL;

void *_Mem_Alloc(mempool_t *pool, size_t size, const char *filename, int fileline)
{
#if MEMCLUMPING
	int i, j, k, needed, endbit, largest;
	memclump_t *clump, **clumpchainpointer;
#endif
#if MEMPARANOIA
	unsigned int sentinel2;
#endif
	memheader_t *mem;
	if (size <= 0)
		return NULL;
	if (pool == NULL)
		Sys_Error("Mem_Alloc: pool == NULL (alloc at %s:%i)", filename, fileline);
	if (developer.integer && developer_memory.integer)
		Con_Printf("Mem_Alloc: pool %s, file %s:%i, size %i bytes\n", pool->name, filename, fileline, (int)size);
	if (developer.integer && developer_memorydebug.integer)
		_Mem_CheckSentinelsGlobal(filename, fileline);
	pool->totalsize += size;
#if MEMCLUMPING
	if (size < 4096)
	{
		// clumping
#if MEMPARANOIA
		needed = (sizeof(memheader_t) + size + sizeof(unsigned int) + (MEMUNIT - 1)) / MEMUNIT;
#else
		needed = (sizeof(memheader_t) + size + sizeof(unsigned char) + (MEMUNIT - 1)) / MEMUNIT;
#endif
		endbit = MEMBITS - needed;
		for (clumpchainpointer = &pool->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
		{
			clump = *clumpchainpointer;
			if (clump->sentinel1 != MEMCLUMP_SENTINEL)
				Sys_Error("Mem_Alloc: trashed clump sentinel 1 (alloc at %s:%d)", filename, fileline);
			if (clump->sentinel2 != MEMCLUMP_SENTINEL)
				Sys_Error("Mem_Alloc: trashed clump sentinel 2 (alloc at %s:%d)", filename, fileline);
			if (clump->largestavailable >= needed)
			{
				largest = 0;
				for (i = 0;i < endbit;i++)
				{
					if (clump->bits[i >> 5] & (1 << (i & 31)))
						continue;
					k = i + needed;
					for (j = i;i < k;i++)
						if (clump->bits[i >> 5] & (1 << (i & 31)))
							goto loopcontinue;
					goto choseclump;
	loopcontinue:;
					if (largest < j - i)
						largest = j - i;
				}
				// since clump falsely advertised enough space (nothing wrong
				// with that), update largest count to avoid wasting time in
				// later allocations
				clump->largestavailable = largest;
			}
		}
		pool->realsize += sizeof(memclump_t);
		clump = malloc(sizeof(memclump_t));
		if (clump == NULL)
			Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline);
		memset(clump, 0, sizeof(memclump_t));
		*clumpchainpointer = clump;
		clump->sentinel1 = MEMCLUMP_SENTINEL;
		clump->sentinel2 = MEMCLUMP_SENTINEL;
		clump->chain = NULL;
		clump->blocksinuse = 0;
		clump->largestavailable = MEMBITS - needed;
		j = 0;
choseclump:
		mem = (memheader_t *)((unsigned char *) clump->block + j * MEMUNIT);
		mem->clump = clump;
		clump->blocksinuse += needed;
		for (i = j + needed;j < i;j++)
			clump->bits[j >> 5] |= (1 << (j & 31));
	}
	else
	{
		// big allocations are not clumped
#endif
#if MEMPARANOIA
		pool->realsize += sizeof(memheader_t) + size + sizeof(sentinel2);
		mem = (memheader_t *)malloc(sizeof(memheader_t) + size + sizeof(sentinel2));
#else
		pool->realsize += sizeof(memheader_t) + size + 1;
		mem = (memheader_t *)malloc(sizeof(memheader_t) + size + 1);
#endif
		if (mem == NULL)
			Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline);
#if MEMCLUMPING
		mem->clump = NULL;
	}
#endif
	mem->filename = filename;
	mem->fileline = fileline;
	mem->size = size;
	mem->pool = pool;
#if MEMPARANOIA
	mem->sentinel1 = MEMHEADER_SENTINEL_FOR_ADDRESS(&mem->sentinel1);
	sentinel2 = MEMHEADER_SENTINEL_FOR_ADDRESS((unsigned char *) mem + sizeof(memheader_t) + mem->size);
	memcpy((unsigned char *) mem + sizeof(memheader_t) + mem->size, &sentinel2, sizeof(sentinel2));
#else
	mem->sentinel1 = MEMHEADER_SENTINEL1;
	// we have to use only a single byte for this sentinel, because it may not be aligned, and some platforms can't use unaligned accesses
	*((unsigned char *) mem + sizeof(memheader_t) + mem->size) = MEMHEADER_SENTINEL2;
#endif
	// append to head of list
	mem->next = pool->chain;
	mem->prev = NULL;
	pool->chain = mem;
	if (mem->next)
		mem->next->prev = mem;
	memset((void *)((unsigned char *) mem + sizeof(memheader_t)), 0, mem->size);
	return (void *)((unsigned char *) mem + sizeof(memheader_t));
}

// only used by _Mem_Free and _Mem_FreePool
static void _Mem_FreeBlock(memheader_t *mem, const char *filename, int fileline)
{
#if MEMCLUMPING
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
#endif
	mempool_t *pool;
#if MEMPARANOIA
	unsigned int sentinel2;
	if (mem->sentinel1 != MEMHEADER_SENTINEL_FOR_ADDRESS(&mem->sentinel1))
		Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	sentinel2 = MEMHEADER_SENTINEL_FOR_ADDRESS((unsigned char *) mem + sizeof(memheader_t) + mem->size);
	if (memcmp((unsigned char *) mem + sizeof(memheader_t) + mem->size, &sentinel2, sizeof(sentinel2)))
		Sys_Error("Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
#else
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((unsigned char *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
#endif
	pool = mem->pool;
	if (developer.integer && developer_memory.integer)
		Con_Printf("Mem_Free: pool %s, alloc %s:%i, free %s:%i, size %i bytes\n", pool->name, mem->filename, mem->fileline, filename, fileline, (int)(mem->size));
	// unlink memheader from doubly linked list
	if ((mem->prev ? mem->prev->next != mem : pool->chain != mem) || (mem->next && mem->next->prev != mem))
		Sys_Error("Mem_Free: not allocated or double freed (free at %s:%i)", filename, fileline);
	if (mem->prev)
		mem->prev->next = mem->next;
	else
		pool->chain = mem->next;
	if (mem->next)
		mem->next->prev = mem->prev;
	// memheader has been unlinked, do the actual free now
	pool->totalsize -= mem->size;
#if MEMCLUMPING
	if ((clump = mem->clump))
	{
		if (clump->sentinel1 != MEMCLUMP_SENTINEL)
			Sys_Error("Mem_Free: trashed clump sentinel 1 (free at %s:%i)", filename, fileline);
		if (clump->sentinel2 != MEMCLUMP_SENTINEL)
			Sys_Error("Mem_Free: trashed clump sentinel 2 (free at %s:%i)", filename, fileline);
		firstblock = ((unsigned char *) mem - (unsigned char *) clump->block);
		if (firstblock & (MEMUNIT - 1))
			Sys_Error("Mem_Free: address not valid in clump (free at %s:%i)", filename, fileline);
		firstblock /= MEMUNIT;
		endblock = firstblock + ((sizeof(memheader_t) + mem->size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT);
		clump->blocksinuse -= endblock - firstblock;
		// could use &, but we know the bit is set
		for (i = firstblock;i < endblock;i++)
			clump->bits[i >> 5] -= (1 << (i & 31));
		if (clump->blocksinuse <= 0)
		{
			// unlink from chain
			for (clumpchainpointer = &pool->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
			{
				if (*clumpchainpointer == clump)
				{
					*clumpchainpointer = clump->chain;
					break;
				}
			}
			pool->realsize -= sizeof(memclump_t);
			memset(clump, 0xBF, sizeof(memclump_t));
			free(clump);
		}
		else
		{
			// clump still has some allocations
			// force re-check of largest available space on next alloc
			clump->largestavailable = MEMBITS - clump->blocksinuse;
		}
	}
	else
	{
#endif
		pool->realsize -= sizeof(memheader_t) + mem->size + sizeof(int);
		if (developer_memorydebug.integer)
			memset(mem, 0xBF, sizeof(memheader_t) + mem->size + sizeof(int));
		free(mem);
#if MEMCLUMPING
	}
#endif
}

void _Mem_Free(void *data, const char *filename, int fileline)
{
	if (data == NULL)
	{
		Con_DPrintf("Mem_Free: data == NULL (called at %s:%i)", filename, fileline);
		return;
	}

	if (developer.integer && developer_memorydebug.integer)
	{
		_Mem_CheckSentinelsGlobal(filename, fileline);
		if (!Mem_IsAllocated(NULL, data))
			Sys_Error("Mem_Free: data is not allocated (called at %s:%i)", filename, fileline);
	}

	_Mem_FreeBlock((memheader_t *)((unsigned char *) data - sizeof(memheader_t)), filename, fileline);
}

mempool_t *_Mem_AllocPool(const char *name, int flags, mempool_t *parent, const char *filename, int fileline)
{
	mempool_t *pool;
	if (developer.integer && developer_memorydebug.integer)
		_Mem_CheckSentinelsGlobal(filename, fileline);
	pool = (mempool_t *)malloc(sizeof(mempool_t));
	if (pool == NULL)
		Sys_Error("Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline);
	memset(pool, 0, sizeof(mempool_t));
#if MEMPARANOIA
	pool->sentinel1 = MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel1);
	pool->sentinel2 = MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel2);
#else
	pool->sentinel1 = MEMHEADER_SENTINEL1;
	pool->sentinel2 = MEMHEADER_SENTINEL1;
#endif
	pool->filename = filename;
	pool->fileline = fileline;
	pool->flags = flags;
	pool->chain = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof(mempool_t);
	strlcpy (pool->name, name, sizeof (pool->name));
	pool->parent = parent;
	pool->next = poolchain;
	poolchain = pool;
	return pool;
}

void _Mem_FreePool(mempool_t **poolpointer, const char *filename, int fileline)
{
	mempool_t *pool = *poolpointer;
	mempool_t **chainaddress, *iter, *temp;

	if (developer.integer && developer_memorydebug.integer)
		_Mem_CheckSentinelsGlobal(filename, fileline);
	if (pool)
	{
		// unlink pool from chain
		for (chainaddress = &poolchain;*chainaddress && *chainaddress != pool;chainaddress = &((*chainaddress)->next));
		if (*chainaddress != pool)
			Sys_Error("Mem_FreePool: pool already free (freepool at %s:%i)", filename, fileline);
#if MEMPARANOIA
		if (pool->sentinel1 != MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel1))
			Sys_Error("Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel2))
			Sys_Error("Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
#else
		if (pool->sentinel1 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
#endif
		*chainaddress = pool->next;

		// free memory owned by the pool
		while (pool->chain)
			_Mem_FreeBlock(pool->chain, filename, fileline);

		// free child pools, too
		for(iter = poolchain; iter; temp = iter = iter->next)
			if(iter->parent == pool)
				_Mem_FreePool(&temp, filename, fileline);

		// free the pool itself
		memset(pool, 0xBF, sizeof(mempool_t));
		free(pool);

		*poolpointer = NULL;
	}
}

void _Mem_EmptyPool(mempool_t *pool, const char *filename, int fileline)
{
	mempool_t *chainaddress;

	if (developer.integer && developer_memorydebug.integer)
	{
		_Mem_CheckSentinelsGlobal(filename, fileline);
		// check if this pool is in the poolchain
		for (chainaddress = poolchain;chainaddress;chainaddress = chainaddress->next)
			if (chainaddress == pool)
				break;
		if (!chainaddress)
			Sys_Error("Mem_EmptyPool: pool is already free (emptypool at %s:%i)", filename, fileline);
	}
	if (pool == NULL)
		Sys_Error("Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline);
#if MEMPARANOIA
	if (pool->sentinel1 != MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel1))
		Sys_Error("Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
	if (pool->sentinel2 != MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel2))
		Sys_Error("Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
#else
	if (pool->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
	if (pool->sentinel2 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
#endif

	// free memory owned by the pool
	while (pool->chain)
		_Mem_FreeBlock(pool->chain, filename, fileline);

	// empty child pools, too
	for(chainaddress = poolchain; chainaddress; chainaddress = chainaddress->next)
		if(chainaddress->parent == pool)
			_Mem_EmptyPool(chainaddress, filename, fileline);

}

void _Mem_CheckSentinels(void *data, const char *filename, int fileline)
{
	memheader_t *mem;
#if MEMPARANOIA
	unsigned int sentinel2;
#endif

	if (data == NULL)
		Sys_Error("Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline);

	mem = (memheader_t *)((unsigned char *) data - sizeof(memheader_t));
#if MEMPARANOIA
	if (mem->sentinel1 != MEMHEADER_SENTINEL_FOR_ADDRESS(&mem->sentinel1))
		Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	sentinel2 = MEMHEADER_SENTINEL_FOR_ADDRESS((unsigned char *) mem + sizeof(memheader_t) + mem->size);
	if (memcmp((unsigned char *) mem + sizeof(memheader_t) + mem->size, &sentinel2, sizeof(sentinel2)))
		Sys_Error("Mem_Free: trashed header sentinel 2 (alloc at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
#else
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((unsigned char *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
#endif
}

#if MEMCLUMPING
static void _Mem_CheckClumpSentinels(memclump_t *clump, const char *filename, int fileline)
{
	// this isn't really very useful
	if (clump->sentinel1 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i)", filename, fileline);
	if (clump->sentinel2 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i)", filename, fileline);
}
#endif

void _Mem_CheckSentinelsGlobal(const char *filename, int fileline)
{
	memheader_t *mem;
#if MEMCLUMPING
	memclump_t *clump;
#endif
	mempool_t *pool;
	for (pool = poolchain;pool;pool = pool->next)
	{
#if MEMPARANOIA
		if (pool->sentinel1 != MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel1))
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 1 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL_FOR_ADDRESS(&pool->sentinel2))
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 2 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline);
#else
		if (pool->sentinel1 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 1 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 2 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline);
#endif
	}
	for (pool = poolchain;pool;pool = pool->next)
		for (mem = pool->chain;mem;mem = mem->next)
			_Mem_CheckSentinels((void *)((unsigned char *) mem + sizeof(memheader_t)), filename, fileline);
#if MEMCLUMPING
	for (pool = poolchain;pool;pool = pool->next)
		for (clump = pool->clumpchain;clump;clump = clump->chain)
			_Mem_CheckClumpSentinels(clump, filename, fileline);
#endif
}

qboolean Mem_IsAllocated(mempool_t *pool, void *data)
{
	memheader_t *header;
	memheader_t *target;

	if (pool)
	{
		// search only one pool
		target = (memheader_t *)((unsigned char *) data - sizeof(memheader_t));
		for( header = pool->chain ; header ; header = header->next )
			if( header == target )
				return true;
	}
	else
	{
		// search all pools
		for (pool = poolchain;pool;pool = pool->next)
			if (Mem_IsAllocated(pool, data))
				return true;
	}
	return false;
}

void Mem_ExpandableArray_NewArray(memexpandablearray_t *l, mempool_t *mempool, size_t recordsize, int numrecordsperarray)
{
	memset(l, 0, sizeof(*l));
	l->mempool = mempool;
	l->recordsize = recordsize;
	l->numrecordsperarray = numrecordsperarray;
}

void Mem_ExpandableArray_FreeArray(memexpandablearray_t *l)
{
	size_t i;
	if (l->maxarrays)
	{
		for (i = 0;i != l->numarrays;i++)
			Mem_Free(l->arrays[i].data);
		Mem_Free(l->arrays);
	}
	memset(l, 0, sizeof(*l));
}

void *Mem_ExpandableArray_AllocRecord(memexpandablearray_t *l)
{
	size_t i, j;
	for (i = 0;;i++)
	{
		if (i == l->numarrays)
		{
			if (l->numarrays == l->maxarrays)
			{
				memexpandablearray_array_t *oldarrays = l->arrays;
				l->maxarrays = max(l->maxarrays * 2, 128);
				l->arrays = (memexpandablearray_array_t*) Mem_Alloc(l->mempool, l->maxarrays * sizeof(*l->arrays));
				if (oldarrays)
				{
					memcpy(l->arrays, oldarrays, l->numarrays * sizeof(*l->arrays));
					Mem_Free(oldarrays);
				}
			}
			l->arrays[i].numflaggedrecords = 0;
			l->arrays[i].data = (unsigned char *) Mem_Alloc(l->mempool, (l->recordsize + 1) * l->numrecordsperarray);
			l->arrays[i].allocflags = l->arrays[i].data + l->recordsize * l->numrecordsperarray;
			l->numarrays++;
		}
		if (l->arrays[i].numflaggedrecords < l->numrecordsperarray)
		{
			for (j = 0;j < l->numrecordsperarray;j++)
			{
				if (!l->arrays[i].allocflags[j])
				{
					l->arrays[i].allocflags[j] = true;
					l->arrays[i].numflaggedrecords++;
					memset(l->arrays[i].data + l->recordsize * j, 0, l->recordsize);
					return (void *)(l->arrays[i].data + l->recordsize * j);
				}
			}
		}
	}
}

/*****************************************************************************
 * IF YOU EDIT THIS:
 * If this function was to change the size of the "expandable" array, you have
 * to update r_shadow.c
 * Just do a search for "range =", R_ShadowClearWorldLights would be the first
 * function to look at. (And also seems like the only one?) You  might have to
 * move the  call to Mem_ExpandableArray_IndexRange  back into for(...) loop's
 * condition
 */
void Mem_ExpandableArray_FreeRecord(memexpandablearray_t *l, void *record) // const!
{
	size_t i, j;
	unsigned char *p = (unsigned char *)record;
	for (i = 0;i != l->numarrays;i++)
	{
		if (p >= l->arrays[i].data && p < (l->arrays[i].data + l->recordsize * l->numrecordsperarray))
		{
			j = (p - l->arrays[i].data) / l->recordsize;
			if (p != l->arrays[i].data + j * l->recordsize)
				Sys_Error("Mem_ExpandableArray_FreeRecord: no such record %p\n", p);
			if (!l->arrays[i].allocflags[j])
				Sys_Error("Mem_ExpandableArray_FreeRecord: record %p is already free!\n", p);
			l->arrays[i].allocflags[j] = false;
			l->arrays[i].numflaggedrecords--;
			return;
		}
	}
}

size_t Mem_ExpandableArray_IndexRange(const memexpandablearray_t *l)
{
	size_t i, j, k, end = 0;
	for (i = 0;i < l->numarrays;i++)
	{
		for (j = 0, k = 0;k < l->arrays[i].numflaggedrecords;j++)
		{
			if (l->arrays[i].allocflags[j])
			{
				end = l->numrecordsperarray * i + j + 1;
				k++;
			}
		}
	}
	return end;
}

void *Mem_ExpandableArray_RecordAtIndex(const memexpandablearray_t *l, size_t index)
{
	size_t i, j;
	i = index / l->numrecordsperarray;
	j = index % l->numrecordsperarray;
	if (i >= l->numarrays || !l->arrays[i].allocflags[j])
		return NULL;
	return (void *)(l->arrays[i].data + j * l->recordsize);
}


// used for temporary memory allocations around the engine, not for longterm
// storage, if anything in this pool stays allocated during gameplay, it is
// considered a leak
mempool_t *tempmempool;
// only for zone
mempool_t *zonemempool;

void Mem_PrintStats(void)
{
	size_t count = 0, size = 0, realsize = 0;
	mempool_t *pool;
	memheader_t *mem;
	Mem_CheckSentinelsGlobal();
	for (pool = poolchain;pool;pool = pool->next)
	{
		count++;
		size += pool->totalsize;
		realsize += pool->realsize;
	}
	Con_Printf("%lu memory pools, totalling %lu bytes (%.3fMB)\n", (unsigned long)count, (unsigned long)size, size / 1048576.0);
	Con_Printf("total allocated size: %lu bytes (%.3fMB)\n", (unsigned long)realsize, realsize / 1048576.0);
	for (pool = poolchain;pool;pool = pool->next)
	{
		if ((pool->flags & POOLFLAG_TEMP) && pool->chain)
		{
			Con_Printf("Memory pool %p has sprung a leak totalling %lu bytes (%.3fMB)!  Listing contents...\n", (void *)pool, (unsigned long)pool->totalsize, pool->totalsize / 1048576.0);
			for (mem = pool->chain;mem;mem = mem->next)
				Con_Printf("%10lu bytes allocated at %s:%i\n", (unsigned long)mem->size, mem->filename, mem->fileline);
		}
	}
}

void Mem_PrintList(size_t minallocationsize)
{
	mempool_t *pool;
	memheader_t *mem;
	Mem_CheckSentinelsGlobal();
	Con_Print("memory pool list:\n"
	           "size    name\n");
	for (pool = poolchain;pool;pool = pool->next)
	{
		Con_Printf("%10luk (%10luk actual) %s (%+li byte change) %s\n", (unsigned long) ((pool->totalsize + 1023) / 1024), (unsigned long)((pool->realsize + 1023) / 1024), pool->name, (long)pool->totalsize - pool->lastchecksize, (pool->flags & POOLFLAG_TEMP) ? "TEMP" : "");
		pool->lastchecksize = pool->totalsize;
		for (mem = pool->chain;mem;mem = mem->next)
			if (mem->size >= minallocationsize)
				Con_Printf("%10lu bytes allocated at %s:%i\n", (unsigned long)mem->size, mem->filename, mem->fileline);
	}
}

void MemList_f(void)
{
	switch(Cmd_Argc())
	{
	case 1:
		Mem_PrintList(1<<30);
		Mem_PrintStats();
		break;
	case 2:
		Mem_PrintList(atoi(Cmd_Argv(1)) * 1024);
		Mem_PrintStats();
		break;
	default:
		Con_Print("MemList_f: unrecognized options\nusage: memlist [all]\n");
		break;
	}
}

extern void R_TextureStats_Print(qboolean printeach, qboolean printpool, qboolean printtotal);
void MemStats_f(void)
{
	Mem_CheckSentinelsGlobal();
	R_TextureStats_Print(false, false, true);
	GL_Mesh_ListVBOs(false);
	Mem_PrintStats();
}


char* Mem_strdup (mempool_t *pool, const char* s)
{
	char* p;
	size_t sz = strlen (s) + 1;
	if (s == NULL) return NULL;
	p = (char*)Mem_Alloc (pool, sz);
	strlcpy (p, s, sz);
	return p;
}

/*
========================
Memory_Init
========================
*/
void Memory_Init (void)
{
	sentinel_seed = rand();
	poolchain = NULL;
	tempmempool = Mem_AllocPool("Temporary Memory", POOLFLAG_TEMP, NULL);
	zonemempool = Mem_AllocPool("Zone", 0, NULL);
}

void Memory_Shutdown (void)
{
//	Mem_FreePool (&zonemempool);
//	Mem_FreePool (&tempmempool);
}

void Memory_Init_Commands (void)
{
	Cmd_AddCommand ("memstats", MemStats_f, "prints memory system statistics");
	Cmd_AddCommand ("memlist", MemList_f, "prints memory pool information (or if used as memlist 5 lists individual allocations of 5K or larger, 0 lists all allocations)");
	Cvar_RegisterVariable (&developer_memory);
	Cvar_RegisterVariable (&developer_memorydebug);
}

