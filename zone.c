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

mempool_t *poolchain = NULL;

void *_Mem_Alloc(mempool_t *pool, int size, char *filename, int fileline)
{
#if MEMCLUMPING
	int i, j, k, needed, endbit, largest;
	memclump_t *clump, **clumpchainpointer;
#endif
	memheader_t *mem;
	if (size <= 0)
		return NULL;
	if (pool == NULL)
		Sys_Error("Mem_Alloc: pool == NULL (alloc at %s:%i)", filename, fileline);
	Con_DPrintf("Mem_Alloc: pool %s, file %s:%i, size %i bytes\n", pool->name, filename, fileline, size);
	pool->totalsize += size;
#if MEMCLUMPING
	if (size < 4096)
	{
		// clumping
		needed = (sizeof(memheader_t) + size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT;
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
		mem = (memheader_t *)((qbyte *) clump->block + j * MEMUNIT);
		mem->clump = clump;
		clump->blocksinuse += needed;
		for (i = j + needed;j < i;j++)
			clump->bits[j >> 5] |= (1 << (j & 31));
	}
	else
	{
		// big allocations are not clumped
#endif
		pool->realsize += sizeof(memheader_t) + size + sizeof(int);
		mem = malloc(sizeof(memheader_t) + size + sizeof(int));
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
	mem->sentinel1 = MEMHEADER_SENTINEL1;
	// we have to use only a single byte for this sentinel, because it may not be aligned, and some platforms can't use unaligned accesses
	*((qbyte *) mem + sizeof(memheader_t) + mem->size) = MEMHEADER_SENTINEL2;
	// append to head of list
	mem->chain = pool->chain;
	pool->chain = mem;
	memset((void *)((qbyte *) mem + sizeof(memheader_t)), 0, mem->size);
	return (void *)((qbyte *) mem + sizeof(memheader_t));
}

void _Mem_Free(void *data, char *filename, int fileline)
{
#if MEMCLUMPING
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
#endif
	memheader_t *mem, **memchainpointer;
	mempool_t *pool;
	if (data == NULL)
		Sys_Error("Mem_Free: data == NULL (called at %s:%i)", filename, fileline);


	mem = (memheader_t *)((qbyte *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((qbyte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	pool = mem->pool;
	Con_DPrintf("Mem_Free: pool %s, alloc %s:%i, free %s:%i, size %i bytes\n", pool->name, mem->filename, mem->fileline, filename, fileline, mem->size);
	for (memchainpointer = &pool->chain;*memchainpointer;memchainpointer = &(*memchainpointer)->chain)
	{
		if (*memchainpointer == mem)
		{
			*memchainpointer = mem->chain;
			pool->totalsize -= mem->size;
#if MEMCLUMPING
			if ((clump = mem->clump))
			{
				if (clump->sentinel1 != MEMCLUMP_SENTINEL)
					Sys_Error("Mem_Free: trashed clump sentinel 1 (free at %s:%i)", filename, fileline);
				if (clump->sentinel2 != MEMCLUMP_SENTINEL)
					Sys_Error("Mem_Free: trashed clump sentinel 2 (free at %s:%i)", filename, fileline);
				firstblock = ((qbyte *) mem - (qbyte *) clump->block);
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
				memset(mem, 0xBF, sizeof(memheader_t) + mem->size + sizeof(int));
				free(mem);
#if MEMCLUMPING
			}
#endif
			return;
		}
	}
	Sys_Error("Mem_Free: not allocated (free at %s:%i)", filename, fileline);
}

mempool_t *_Mem_AllocPool(char *name, char *filename, int fileline)
{
	mempool_t *pool;
	pool = malloc(sizeof(mempool_t));
	if (pool == NULL)
		Sys_Error("Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline);
	memset(pool, 0, sizeof(mempool_t));
	pool->chain = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof(mempool_t);
	strcpy(pool->name, name);
	pool->next = poolchain;
	poolchain = pool;
	return pool;
}

void _Mem_FreePool(mempool_t **pool, char *filename, int fileline)
{
	mempool_t **chainaddress;
	if (*pool)
	{
		// unlink pool from chain
		for (chainaddress = &poolchain;*chainaddress && *chainaddress != *pool;chainaddress = &((*chainaddress)->next));
		if (*chainaddress != *pool)
			Sys_Error("Mem_FreePool: pool already free (freepool at %s:%i)", filename, fileline);
		*chainaddress = (*pool)->next;

		// free memory owned by the pool
		while ((*pool)->chain)
			Mem_Free((void *)((qbyte *) (*pool)->chain + sizeof(memheader_t)));

		// free the pool itself
		memset(*pool, 0xBF, sizeof(mempool_t));
		free(*pool);
		*pool = NULL;
	}
}

void _Mem_EmptyPool(mempool_t *pool, char *filename, int fileline)
{
	if (pool == NULL)
		Sys_Error("Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline);

	// free memory owned by the pool
	while (pool->chain)
		Mem_Free((void *)((qbyte *) pool->chain + sizeof(memheader_t)));
}

void _Mem_CheckSentinels(void *data, char *filename, int fileline)
{
	memheader_t *mem;

	if (data == NULL)
		Sys_Error("Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline);

	mem = (memheader_t *)((qbyte *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((qbyte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
}

#if MEMCLUMPING
static void _Mem_CheckClumpSentinels(memclump_t *clump, char *filename, int fileline)
{
	// this isn't really very useful
	if (clump->sentinel1 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i)", filename, fileline);
	if (clump->sentinel2 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i)", filename, fileline);
}
#endif

void _Mem_CheckSentinelsGlobal(char *filename, int fileline)
{
	memheader_t *mem;
#if MEMCLUMPING
	memclump_t *clump;
#endif
	mempool_t *pool;
	for (pool = poolchain;pool;pool = pool->next)
#if MEMCLUMPING
	{
#endif
		for (mem = pool->chain;mem;mem = mem->chain)
			_Mem_CheckSentinels((void *)((qbyte *) mem + sizeof(memheader_t)), filename, fileline);
#if MEMCLUMPING
		for (clump = pool->clumpchain;clump;clump = clump->chain)
			_Mem_CheckClumpSentinels(clump, filename, fileline);
	}
#endif
}

// used for temporary memory allocations around the engine, not for longterm
// storage, if anything in this pool stays allocated during gameplay, it is
// considered a leak
mempool_t *tempmempool;
// only for zone
mempool_t *zonemempool;

void Mem_PrintStats(void)
{
	int count = 0, size = 0;
	mempool_t *pool;
	memheader_t *mem;
	Mem_CheckSentinelsGlobal();
	for (pool = poolchain;pool;pool = pool->next)
	{
		count++;
		size += pool->totalsize;
	}
	Con_Printf("%i memory pools, totalling %i bytes (%.3fMB)\n", count, size, size / 1048576.0);
	if (tempmempool == NULL)
		Con_Printf("Error: no tempmempool allocated\n");
	else if (tempmempool->chain)
	{
		Con_Printf("%i bytes (%.3fMB) of temporary memory still allocated (Leak!)\n", tempmempool->totalsize, tempmempool->totalsize / 1048576.0);
		Con_Printf("listing temporary memory allocations:\n");
		for (mem = tempmempool->chain;mem;mem = mem->chain)
			Con_Printf("%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline);
	}
}

void Mem_PrintList(int listallocations)
{
	mempool_t *pool;
	memheader_t *mem;
	Mem_CheckSentinelsGlobal();
	Con_Printf("memory pool list:\n"
	           "size    name\n");
	for (pool = poolchain;pool;pool = pool->next)
	{
		if (pool->lastchecksize != 0 && pool->totalsize != pool->lastchecksize)
			Con_Printf("%6ik (%6ik actual) %s (%i byte change)\n", (pool->totalsize + 1023) / 1024, (pool->realsize + 1023) / 1024, pool->name, pool->totalsize - pool->lastchecksize);
		else
			Con_Printf("%6ik (%6ik actual) %s\n", (pool->totalsize + 1023) / 1024, (pool->realsize + 1023) / 1024, pool->name);
		pool->lastchecksize = pool->totalsize;
		if (listallocations)
			for (mem = pool->chain;mem;mem = mem->chain)
				Con_Printf("%10i bytes allocated at %s:%i\n", mem->size, mem->filename, mem->fileline);
	}
}

void MemList_f(void)
{
	switch(Cmd_Argc())
	{
	case 1:
		Mem_PrintList(false);
		Mem_PrintStats();
		break;
	case 2:
		if (!strcmp(Cmd_Argv(1), "all"))
		{
			Mem_PrintList(true);
			Mem_PrintStats();
			break;
		}
		// drop through
	default:
		Con_Printf("MemList_f: unrecognized options\nusage: memlist [all]\n");
		break;
	}
}

extern void R_TextureStats_PrintTotal(void);
void MemStats_f(void)
{
	Mem_CheckSentinelsGlobal();
	R_TextureStats_PrintTotal();
	Mem_PrintStats();
}


/*
========================
Memory_Init
========================
*/
void Memory_Init (void)
{
	tempmempool = Mem_AllocPool("Temporary Memory");
	zonemempool = Mem_AllocPool("Zone");
}

void Memory_Init_Commands (void)
{
	Cmd_AddCommand ("memstats", MemStats_f);
	Cmd_AddCommand ("memlist", MemList_f);
}

