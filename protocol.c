
#include "quakedef.h"

void ClearStateToDefault(entity_state_t *s)
{
	s->time = 0;
	VectorClear(s->origin);
	VectorClear(s->angles);
	s->effects = 0;
	s->modelindex = 0;
	s->frame = 0;
	s->colormap = 0;
	s->skin = 0;
	s->alpha = 255;
	s->scale = 16;
	s->glowsize = 0;
	s->glowcolor = 254;
	s->flags = 0;
	s->active = 0;
}

// (server) clears the database to contain no frames (thus delta compression compresses against nothing)
void EntityFrame_ClearDatabase(entity_database_t *d)
{
	memset(d, 0, sizeof(*d));
	d->ackframe = -1;
}

// (server) acknowledge a frame as recieved by client (removes old frames from database, will use this new frame for delta compression)
void EntityFrame_AckFrame(entity_database_t *d, int frame)
{
}

// (server) clears frame, to prepare for adding entities
void EntityFrame_Clear(entity_frame_t *f)
{
	memset(f, 0, sizeof(*f));
}

// (server) allocates an entity slot in frame, returns NULL if full
entity_state_t *EntityFrame_NewEntity(entity_frame_t *f, int number)
{
	entity_state_t *e;
	if (f->numentities >= MAX_ENTITY_DATABASE)
		return NULL;
	e = &f->entitydata[f->numentities++];
	e->active = true;
	e->number = number;
	return e;
}

// (server) writes a frame to network stream
void EntityFrame_Write(entity_database_t *d, entity_frame_t *f, int deltaframe, int newframe, sizebuf_t *msg)
{
}

// (client) reads a frame from network stream
void EntityFrame_Read(entity_database_t *d, entity_frame_t *f)
{
}

// (client) fetchs an entity from the database, read with _Read, fills in structs for current and previous state
void EntityFrame_FetchEntity(entity_database_t *d, entity_state_t *previous, entity_state_t *current)
{
}
