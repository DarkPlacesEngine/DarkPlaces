#include "quakedef.h"
#include "protocol.h"

// (client and server) allocates a new empty database
entityframe_database_t *EntityFrame_AllocDatabase(mempool_t *mempool)
{
	return (entityframe_database_t *)Mem_Alloc(mempool, sizeof(entityframe_database_t));
}

// (client and server) frees the database
void EntityFrame_FreeDatabase(entityframe_database_t *d)
{
	Mem_Free(d);
}

// (server and client) removes frames older than 'frame' from database
void EntityFrame_AckFrame(entityframe_database_t *d, int frame)
{
	int i;
	d->ackframenum = frame;
	for (i = 0;i < d->numframes && d->frames[i].framenum < frame;i++);
	// ignore outdated frame acks (out of order packets)
	if (i == 0)
		return;
	d->numframes -= i;
	// if some queue is left, slide it down to beginning of array
	if (d->numframes)
		memmove(&d->frames[0], &d->frames[i], sizeof(d->frames[0]) * d->numframes);
}

// (server and client) reads a frame from the database
void EntityFrame_FetchFrame(entityframe_database_t *d, int framenum, entity_frame_t *f)
{
	int i, n;
	EntityFrame_Clear(f, NULL, -1);
	for (i = 0;i < d->numframes && d->frames[i].framenum < framenum;i++);
	if (i < d->numframes && framenum == d->frames[i].framenum)
	{
		f->framenum = framenum;
		f->numentities = d->frames[i].endentity - d->frames[i].firstentity;
		n = MAX_ENTITY_DATABASE - (d->frames[i].firstentity % MAX_ENTITY_DATABASE);
		if (n > f->numentities)
			n = f->numentities;
		memcpy(f->entitydata, d->entitydata + d->frames[i].firstentity % MAX_ENTITY_DATABASE, sizeof(*f->entitydata) * n);
		if (f->numentities > n)
			memcpy(f->entitydata + n, d->entitydata, sizeof(*f->entitydata) * (f->numentities - n));
		VectorCopy(d->eye, f->eye);
	}
}
