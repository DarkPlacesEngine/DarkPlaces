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

#ifndef ZONE_H
#define ZONE_H

// LordHavoc: this is pointless with a good C library
//#define MEMCLUMPING

#define POOLNAMESIZE 128
// if set this pool will be printed in memlist reports
#define POOLFLAG_TEMP 1
#if MEMCLUMPING
// give malloc padding so we can't waste most of a page at the end
#define MEMCLUMPSIZE (65536 - 1536)
// smallest unit we care about is this many bytes
#define MEMUNIT 8
#define MEMBITS (MEMCLUMPSIZE / MEMUNIT)
#define MEMBITINTS (MEMBITS / 32)
#define MEMCLUMP_SENTINEL 0xABADCAFE
#endif

#define MEMHEADER_SENTINEL1 0xDEADF00D
#define MEMHEADER_SENTINEL2 0xDF

typedef struct memheader_s
{
	// next and previous memheaders in chain belonging to pool
	struct memheader_s *next;
	struct memheader_s *prev;
	// pool this memheader belongs to
	struct mempool_s *pool;
#if MEMCLUMPING
	// clump this memheader lives in, NULL if not in a clump
	struct memclump_s *clump;
#endif
	// size of the memory after the header (excluding header and sentinel2)
	size_t size;
	// file name and line where Mem_Alloc was called
	const char *filename;
	int fileline;
	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel1;
	// immediately followed by data, which is followed by a MEMHEADER_SENTINEL2 byte
}
memheader_t;

#if MEMCLUMPING
typedef struct memclump_s
{
	// contents of the clump
	qbyte block[MEMCLUMPSIZE];
	// should always be MEMCLUMP_SENTINEL
	unsigned int sentinel1;
	// if a bit is on, it means that the MEMUNIT bytes it represents are
	// allocated, otherwise free
	int bits[MEMBITINTS];
	// should always be MEMCLUMP_SENTINEL
	unsigned int sentinel2;
	// if this drops to 0, the clump is freed
	size_t blocksinuse;
	// largest block of memory available (this is reset to an optimistic
	// number when anything is freed, and updated when alloc fails the clump)
	size_t largestavailable;
	// next clump in the chain
	struct memclump_s *chain;
}
memclump_t;
#endif

typedef struct mempool_s
{
	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel1;
	// chain of individual memory allocations
	struct memheader_s *chain;
#if MEMCLUMPING
	// chain of clumps (if any)
	struct memclump_s *clumpchain;
#endif
	// POOLFLAG_*
	int flags;
	// total memory allocated in this pool (inside memheaders)
	size_t totalsize;
	// total memory allocated in this pool (actual malloc total)
	size_t realsize;
	// updated each time the pool is displayed by memlist, shows change from previous time (unless pool was freed)
	size_t lastchecksize;
	// name of the pool
	char name[POOLNAMESIZE];
	// linked into global mempool list
	struct mempool_s *next;
	// parent object (used for nested memory pools)
	struct mempool_s *parent;
	// file name and line where Mem_AllocPool was called
	const char *filename;
	int fileline;
	// should always be MEMHEADER_SENTINEL1
	unsigned int sentinel2;
}
mempool_t;

#define Mem_Alloc(pool,size) _Mem_Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Free(mem) _Mem_Free(mem, __FILE__, __LINE__)
#define Mem_CheckSentinels(data) _Mem_CheckSentinels(data, __FILE__, __LINE__)
#define Mem_CheckSentinelsGlobal() _Mem_CheckSentinelsGlobal(__FILE__, __LINE__)
#define Mem_AllocPool(name, flags, parent) _Mem_AllocPool(name, flags, parent, __FILE__, __LINE__)
#define Mem_FreePool(pool) _Mem_FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) _Mem_EmptyPool(pool, __FILE__, __LINE__)

void *_Mem_Alloc(mempool_t *pool, size_t size, const char *filename, int fileline);
void _Mem_Free(void *data, const char *filename, int fileline);
mempool_t *_Mem_AllocPool(const char *name, int flags, mempool_t *parent, const char *filename, int fileline);
void _Mem_FreePool(mempool_t **pool, const char *filename, int fileline);
void _Mem_EmptyPool(mempool_t *pool, const char *filename, int fileline);
void _Mem_CheckSentinels(void *data, const char *filename, int fileline);
void _Mem_CheckSentinelsGlobal(const char *filename, int fileline);
qboolean Mem_IsAllocated(mempool_t *pool, void *data);

// used for temporary allocations
mempool_t *tempmempool;

void Memory_Init (void);
void Memory_Shutdown (void);
void Memory_Init_Commands (void);

extern mempool_t *zonemempool;
#define Z_Malloc(size) Mem_Alloc(zonemempool,size)
#define Z_Free(data) Mem_Free(data)

extern struct cvar_s developer_memory;
extern struct cvar_s developer_memorydebug;

#endif

