#include "quakedef.h"
#include "protocol.h"

// (client) adds a entity_frame to the database, for future reference
void EntityFrame_AddFrame_Client(entityframe_database_t *d, vec3_t eye, int framenum, int numentities, const entity_state_t *entitydata)
{
	int n, e;
	entity_frameinfo_t *info;

	VectorCopy(eye, d->eye);

	// figure out how many entity slots are used already
	if (d->numframes)
	{
		n = d->frames[d->numframes - 1].endentity - d->frames[0].firstentity;
		if (n + numentities > MAX_ENTITY_DATABASE || d->numframes >= MAX_ENTITY_HISTORY)
		{
			// ran out of room, dump database
			EntityFrame_ClearDatabase(d);
		}
	}

	info = &d->frames[d->numframes];
	info->framenum = framenum;
	e = -1000;
	// make sure we check the newly added frame as well, but we haven't incremented numframes yet
	for (n = 0;n <= d->numframes;n++)
	{
		if (e >= d->frames[n].framenum)
		{
			if (e == framenum)
				Con_Print("EntityFrame_AddFrame: tried to add out of sequence frame to database\n");
			else
				Con_Print("EntityFrame_AddFrame: out of sequence frames in database\n");
			return;
		}
		e = d->frames[n].framenum;
	}
	// if database still has frames after that...
	if (d->numframes)
		info->firstentity = d->frames[d->numframes - 1].endentity;
	else
		info->firstentity = 0;
	info->endentity = info->firstentity + numentities;
	d->numframes++;

	n = info->firstentity % MAX_ENTITY_DATABASE;
	e = MAX_ENTITY_DATABASE - n;
	if (e > numentities)
		e = numentities;
	memcpy(d->entitydata + n, entitydata, sizeof(entity_state_t) * e);
	if (numentities > e)
		memcpy(d->entitydata, entitydata + e, sizeof(entity_state_t) * (numentities - e));
}

// (client) reads a frame from network stream
void EntityFrame_CL_ReadFrame(void)
{
	int i, number, removed;
	entity_frame_t *f, *delta;
	entity_state_t *e, *old, *oldend;
	entity_t *ent;
	entityframe_database_t *d;
	if (!cl.entitydatabase)
		cl.entitydatabase = EntityFrame_AllocDatabase(cls.levelmempool);
	d = cl.entitydatabase;
	f = &d->framedata;
	delta = &d->deltaframe;

	EntityFrame_Clear(f, NULL, -1);

	// read the frame header info
	f->time = cl.mtime[0];
	number = MSG_ReadLong(&cl_message);
	f->framenum = MSG_ReadLong(&cl_message);
	CL_NewFrameReceived(f->framenum);
	f->eye[0] = MSG_ReadFloat(&cl_message);
	f->eye[1] = MSG_ReadFloat(&cl_message);
	f->eye[2] = MSG_ReadFloat(&cl_message);
	EntityFrame_AckFrame(d, number);
	EntityFrame_FetchFrame(d, number, delta);
	old = delta->entitydata;
	oldend = old + delta->numentities;
	// read entities until we hit the magic 0xFFFF end tag
	while ((number = (unsigned short) MSG_ReadShort(&cl_message)) != 0xFFFF && !cl_message.badread)
	{
		if (cl_message.badread)
			Host_Error("EntityFrame_Read: read error");
		removed = number & 0x8000;
		number &= 0x7FFF;
		if (number >= MAX_EDICTS)
			Host_Error("EntityFrame_Read: number (%i) >= MAX_EDICTS (%i)", number, MAX_EDICTS);

		// seek to entity, while copying any skipped entities (assume unchanged)
		while (old < oldend && old->number < number)
		{
			if (f->numentities >= MAX_ENTITY_DATABASE)
				Host_Error("EntityFrame_Read: entity list too big");
			f->entitydata[f->numentities] = *old++;
			f->entitydata[f->numentities++].time = cl.mtime[0];
		}
		if (removed)
		{
			if (old < oldend && old->number == number)
				old++;
			else
				Con_Printf("EntityFrame_Read: REMOVE on unused entity %i\n", number);
		}
		else
		{
			if (f->numentities >= MAX_ENTITY_DATABASE)
				Host_Error("EntityFrame_Read: entity list too big");

			// reserve this slot
			e = f->entitydata + f->numentities++;

			if (old < oldend && old->number == number)
			{
				// delta from old entity
				*e = *old++;
			}
			else
			{
				// delta from defaults
				*e = defaultstate;
			}

			if (cl.num_entities <= number)
			{
				cl.num_entities = number + 1;
				if (number >= cl.max_entities)
					CL_ExpandEntities(number);
			}
			cl.entities_active[number] = true;
			e->active = ACTIVE_NETWORK;
			e->time = cl.mtime[0];
			e->number = number;
			EntityState_ReadFields(e, EntityState_ReadExtendBits());
		}
	}
	while (old < oldend)
	{
		if (f->numentities >= MAX_ENTITY_DATABASE)
			Host_Error("EntityFrame_Read: entity list too big");
		f->entitydata[f->numentities] = *old++;
		f->entitydata[f->numentities++].time = cl.mtime[0];
	}
	EntityFrame_AddFrame_Client(d, f->eye, f->framenum, f->numentities, f->entitydata);

	memset(cl.entities_active, 0, cl.num_entities * sizeof(unsigned char));
	number = 1;
	for (i = 0;i < f->numentities;i++)
	{
		for (;number < f->entitydata[i].number && number < cl.num_entities;number++)
		{
			if (cl.entities_active[number])
			{
				cl.entities_active[number] = false;
				cl.entities[number].state_current.active = ACTIVE_NOT;
			}
		}
		if (number >= cl.num_entities)
			break;
		// update the entity
		ent = &cl.entities[number];
		ent->state_previous = ent->state_current;
		ent->state_current = f->entitydata[i];
		CL_MoveLerpEntityStates(ent);
		// the entity lives again...
		cl.entities_active[number] = true;
		number++;
	}
	for (;number < cl.num_entities;number++)
	{
		if (cl.entities_active[number])
		{
			cl.entities_active[number] = false;
			cl.entities[number].state_current.active = ACTIVE_NOT;
		}
	}
}


// (client) returns the frame number of the most recent frame recieved
int EntityFrame_MostRecentlyRecievedFrameNum(entityframe_database_t *d)
{
	if (d->numframes)
		return d->frames[d->numframes - 1].framenum;
	else
		return -1;
}