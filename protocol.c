
#include "quakedef.h"

entity_state_t defaultstate =
{
	0,//double time; // time this state was built
	{0,0,0},//vec3_t origin;
	{0,0,0},//vec3_t angles;
	0,//int number; // entity number this state is for
	0,//unsigned short active; // true if a valid state
	0,//unsigned short modelindex;
	0,//unsigned short frame;
	0,//unsigned short effects;
	0,//unsigned short tagentity;
	0,//unsigned short specialvisibilityradius;
	0,//unsigned short viewmodelforclient;
	0,//unsigned short exteriormodelforclient;
	0,//unsigned short nodrawtoclient;
	0,//unsigned short drawonlytoclient;
	0,//qbyte colormap;
	0,//qbyte skin;
	255,//qbyte alpha;
	16,//qbyte scale;
	0,//qbyte glowsize;
	254,//qbyte glowcolor;
	0,//qbyte flags;
	0,//qbyte tagindex;
};

void ClearStateToDefault(entity_state_t *s)
{
	*s = defaultstate;
	/*
	memset(s, 0, sizeof(*s));
	s->alpha = 255;
	s->scale = 16;
	s->glowcolor = 254;
	*/
}

void EntityState_Write(entity_state_t *ent, sizebuf_t *msg, entity_state_t *delta)
{
	int bits;
	vec3_t org, deltaorg;
	if (ent->active)
	{
		// if not active last frame, delta from defaults
		if (!delta->active)
			delta = &defaultstate;
		bits = 0;
		VectorCopy(ent->origin, org);
		VectorCopy(delta->origin, deltaorg);
		if (ent->flags & RENDER_LOWPRECISION)
		{
			if (org[0] > 0)
				org[0] = (int) (org[0] + 0.5f);
			else
				org[0] = (int) (org[0] - 0.5f);
			if (org[1] > 0)
				org[1] = (int) (org[1] + 0.5f);
			else
				org[1] = (int) (org[1] - 0.5f);
			if (org[2] > 0)
				org[2] = (int) (org[2] + 0.5f);
			else
				org[2] = (int) (org[2] - 0.5f);
		}
		if (delta->flags & RENDER_LOWPRECISION)
		{
			if (deltaorg[0] > 0)
				deltaorg[0] = (int) (deltaorg[0] + 0.5f);
			else
				deltaorg[0] = (int) (deltaorg[0] - 0.5f);
			if (deltaorg[1] > 0)
				deltaorg[1] = (int) (deltaorg[1] + 0.5f);
			else
				deltaorg[1] = (int) (deltaorg[1] - 0.5f);
			if (deltaorg[2] > 0)
				deltaorg[2] = (int) (deltaorg[2] + 0.5f);
			else
				deltaorg[2] = (int) (deltaorg[2] - 0.5f);
		}
		if (fabs(org[0] - deltaorg[0]) > 0.01f)
			bits |= E_ORIGIN1;
		if (fabs(org[1] - deltaorg[1]) > 0.01f)
			bits |= E_ORIGIN2;
		if (fabs(org[2] - deltaorg[2]) > 0.01f)
			bits |= E_ORIGIN3;
		if ((qbyte) (ent->angles[0] * (256.0f / 360.0f)) != (qbyte) (delta->angles[0] * (256.0f / 360.0f)))
			bits |= E_ANGLE1;
		if ((qbyte) (ent->angles[1] * (256.0f / 360.0f)) != (qbyte) (delta->angles[1] * (256.0f / 360.0f)))
			bits |= E_ANGLE2;
		if ((qbyte) (ent->angles[2] * (256.0f / 360.0f)) != (qbyte) (delta->angles[2] * (256.0f / 360.0f)))
			bits |= E_ANGLE3;
		if ((ent->modelindex ^ delta->modelindex) & 0x00FF)
			bits |= E_MODEL1;
		if ((ent->modelindex ^ delta->modelindex) & 0xFF00)
			bits |= E_MODEL2;
		if ((ent->frame ^ delta->frame) & 0x00FF)
			bits |= E_FRAME1;
		if ((ent->frame ^ delta->frame) & 0xFF00)
			bits |= E_FRAME2;
		if ((ent->effects ^ delta->effects) & 0x00FF)
			bits |= E_EFFECTS1;
		if ((ent->effects ^ delta->effects) & 0xFF00)
			bits |= E_EFFECTS2;
		if (ent->colormap != delta->colormap)
			bits |= E_COLORMAP;
		if (ent->skin != delta->skin)
			bits |= E_SKIN;
		if (ent->alpha != delta->alpha)
			bits |= E_ALPHA;
		if (ent->scale != delta->scale)
			bits |= E_SCALE;
		if (ent->glowsize != delta->glowsize)
			bits |= E_GLOWSIZE;
		if (ent->glowcolor != delta->glowcolor)
			bits |= E_GLOWCOLOR;
		if (ent->flags != delta->flags)
			bits |= E_FLAGS;
		if (ent->tagindex != delta->tagindex || ent->tagentity != delta->tagentity)
			bits |= E_TAGATTACHMENT;

		if (bits) // don't send anything if it hasn't changed
		{
			if (bits & 0xFF000000)
				bits |= E_EXTEND3;
			if (bits & 0x00FF0000)
				bits |= E_EXTEND2;
			if (bits & 0x0000FF00)
				bits |= E_EXTEND1;

			MSG_WriteShort(msg, ent->number);
			MSG_WriteByte(msg, bits & 0xFF);
			if (bits & E_EXTEND1)
			{
				MSG_WriteByte(msg, (bits >> 8) & 0xFF);
				if (bits & E_EXTEND2)
				{
					MSG_WriteByte(msg, (bits >> 16) & 0xFF);
					if (bits & E_EXTEND3)
						MSG_WriteByte(msg, (bits >> 24) & 0xFF);
				}
			}
			// LordHavoc: have to write flags first, as they can modify protocol
			if (bits & E_FLAGS)
				MSG_WriteByte(msg, ent->flags);
			if (ent->flags & RENDER_LOWPRECISION)
			{
				if (bits & E_ORIGIN1)
					MSG_WriteShort(msg, org[0]);
				if (bits & E_ORIGIN2)
					MSG_WriteShort(msg, org[1]);
				if (bits & E_ORIGIN3)
					MSG_WriteShort(msg, org[2]);
			}
			else
			{
				if (bits & E_ORIGIN1)
					MSG_WriteFloat(msg, org[0]);
				if (bits & E_ORIGIN2)
					MSG_WriteFloat(msg, org[1]);
				if (bits & E_ORIGIN3)
					MSG_WriteFloat(msg, org[2]);
			}
			if (bits & E_ANGLE1)
				MSG_WriteAngle(msg, ent->angles[0]);
			if (bits & E_ANGLE2)
				MSG_WriteAngle(msg, ent->angles[1]);
			if (bits & E_ANGLE3)
				MSG_WriteAngle(msg, ent->angles[2]);
			if (bits & E_MODEL1)
				MSG_WriteByte(msg, ent->modelindex & 0xFF);
			if (bits & E_MODEL2)
				MSG_WriteByte(msg, (ent->modelindex >> 8) & 0xFF);
			if (bits & E_FRAME1)
				MSG_WriteByte(msg, ent->frame & 0xFF);
			if (bits & E_FRAME2)
				MSG_WriteByte(msg, (ent->frame >> 8) & 0xFF);
			if (bits & E_EFFECTS1)
				MSG_WriteByte(msg, ent->effects & 0xFF);
			if (bits & E_EFFECTS2)
				MSG_WriteByte(msg, (ent->effects >> 8) & 0xFF);
			if (bits & E_COLORMAP)
				MSG_WriteByte(msg, ent->colormap);
			if (bits & E_SKIN)
				MSG_WriteByte(msg, ent->skin);
			if (bits & E_ALPHA)
				MSG_WriteByte(msg, ent->alpha);
			if (bits & E_SCALE)
				MSG_WriteByte(msg, ent->scale);
			if (bits & E_GLOWSIZE)
				MSG_WriteByte(msg, ent->glowsize);
			if (bits & E_GLOWCOLOR)
				MSG_WriteByte(msg, ent->glowcolor);
			if (bits & E_TAGATTACHMENT)
			{
				MSG_WriteShort(msg, ent->tagentity);
				MSG_WriteByte(msg, ent->tagindex);
			}
		}
	}
	else if (delta->active)
		MSG_WriteShort(msg, ent->number | 0x8000);
}

void EntityState_ReadUpdate(entity_state_t *e, int number)
{
	int bits;
	cl_entities_active[number] = true;
	e->active = true;
	e->time = cl.mtime[0];
	e->number = number;

	bits = MSG_ReadByte();
	if (bits & E_EXTEND1)
	{
		bits |= MSG_ReadByte() << 8;
		if (bits & E_EXTEND2)
		{
			bits |= MSG_ReadByte() << 16;
			if (bits & E_EXTEND3)
				bits |= MSG_ReadByte() << 24;
		}
	}

	if (cl.protocol == PROTOCOL_DARKPLACES2)
	{
		if (bits & E_ORIGIN1)
			e->origin[0] = (signed short) MSG_ReadShort();
		if (bits & E_ORIGIN2)
			e->origin[1] = (signed short) MSG_ReadShort();
		if (bits & E_ORIGIN3)
			e->origin[2] = (signed short) MSG_ReadShort();
	}
	else
	{
		if (bits & E_FLAGS)
			e->flags = MSG_ReadByte();
		if (e->flags & RENDER_LOWPRECISION || cl.protocol == PROTOCOL_DARKPLACES2)
		{
			if (bits & E_ORIGIN1)
				e->origin[0] = (signed short) MSG_ReadShort();
			if (bits & E_ORIGIN2)
				e->origin[1] = (signed short) MSG_ReadShort();
			if (bits & E_ORIGIN3)
				e->origin[2] = (signed short) MSG_ReadShort();
		}
		else
		{
			if (bits & E_ORIGIN1)
				e->origin[0] = MSG_ReadFloat();
			if (bits & E_ORIGIN2)
				e->origin[1] = MSG_ReadFloat();
			if (bits & E_ORIGIN3)
				e->origin[2] = MSG_ReadFloat();
		}
	}
	if (bits & E_ANGLE1)
		e->angles[0] = MSG_ReadAngle();
	if (bits & E_ANGLE2)
		e->angles[1] = MSG_ReadAngle();
	if (bits & E_ANGLE3)
		e->angles[2] = MSG_ReadAngle();
	if (bits & E_MODEL1)
		e->modelindex = (e->modelindex & 0xFF00) | (unsigned int) MSG_ReadByte();
	if (bits & E_MODEL2)
		e->modelindex = (e->modelindex & 0x00FF) | ((unsigned int) MSG_ReadByte() << 8);
	if (bits & E_FRAME1)
		e->frame = (e->frame & 0xFF00) | (unsigned int) MSG_ReadByte();
	if (bits & E_FRAME2)
		e->frame = (e->frame & 0x00FF) | ((unsigned int) MSG_ReadByte() << 8);
	if (bits & E_EFFECTS1)
		e->effects = (e->effects & 0xFF00) | (unsigned int) MSG_ReadByte();
	if (bits & E_EFFECTS2)
		e->effects = (e->effects & 0x00FF) | ((unsigned int) MSG_ReadByte() << 8);
	if (bits & E_COLORMAP)
		e->colormap = MSG_ReadByte();
	if (bits & E_SKIN)
		e->skin = MSG_ReadByte();
	if (bits & E_ALPHA)
		e->alpha = MSG_ReadByte();
	if (bits & E_SCALE)
		e->scale = MSG_ReadByte();
	if (bits & E_GLOWSIZE)
		e->glowsize = MSG_ReadByte();
	if (bits & E_GLOWCOLOR)
		e->glowcolor = MSG_ReadByte();
	if (cl.protocol == PROTOCOL_DARKPLACES2)
		if (bits & E_FLAGS)
			e->flags = MSG_ReadByte();
	if (bits & E_TAGATTACHMENT)
	{
		e->tagentity = MSG_ReadShort();
		e->tagindex = MSG_ReadByte();
	}

	if (developer_networkentities.integer)
	{
		Con_Printf("ReadUpdate e%i", number);

		if (bits & E_ORIGIN1)
			Con_Printf(" E_ORIGIN1 %f", e->origin[0]);
		if (bits & E_ORIGIN2)
			Con_Printf(" E_ORIGIN2 %f", e->origin[1]);
		if (bits & E_ORIGIN3)
			Con_Printf(" E_ORIGIN3 %f", e->origin[2]);
		if (bits & E_ANGLE1)
			Con_Printf(" E_ANGLE1 %f", e->angles[0]);
		if (bits & E_ANGLE2)
			Con_Printf(" E_ANGLE2 %f", e->angles[1]);
		if (bits & E_ANGLE3)
			Con_Printf(" E_ANGLE3 %f", e->angles[2]);
		if (bits & (E_MODEL1 | E_MODEL2))
			Con_Printf(" E_MODEL %i", e->modelindex);

		if (bits & (E_FRAME1 | E_FRAME2))
			Con_Printf(" E_FRAME %i", e->frame);
		if (bits & (E_EFFECTS1 | E_EFFECTS2))
			Con_Printf(" E_EFFECTS %i", e->effects);
		if (bits & E_ALPHA)
			Con_Printf(" E_ALPHA %f", e->alpha / 255.0f);
		if (bits & E_SCALE)
			Con_Printf(" E_SCALE %f", e->scale / 16.0f);
		if (bits & E_COLORMAP)
			Con_Printf(" E_COLORMAP %i", e->colormap);
		if (bits & E_SKIN)
			Con_Printf(" E_SKIN %i", e->skin);

		if (bits & E_GLOWSIZE)
			Con_Printf(" E_GLOWSIZE %i", e->glowsize * 8);
		if (bits & E_GLOWCOLOR)
			Con_Printf(" E_GLOWCOLOR %i", e->glowcolor);

		if (bits & E_TAGATTACHMENT)
			Con_Printf(" E_TAGATTACHMENT e%i:%i", e->tagentity, e->tagindex);
		Con_Printf("\n");
	}
}

// (server) clears the database to contain no frames (thus delta compression compresses against nothing)
void EntityFrame_ClearDatabase(entity_database_t *d)
{
	memset(d, 0, sizeof(*d));
}

// (server and client) removes frames older than 'frame' from database
void EntityFrame_AckFrame(entity_database_t *d, int frame)
{
	int i;
	if (d->ackframe < frame)
		d->ackframe = frame;
	for (i = 0;i < d->numframes && d->frames[i].framenum < frame;i++);
	// ignore outdated frame acks (out of order packets)
	if (i == 0)
		return;
	d->numframes -= i;
	// if some queue is left, slide it down to beginning of array
	if (d->numframes)
		memmove(&d->frames[0], &d->frames[i], sizeof(d->frames[0]) * d->numframes);
}

// (server) clears frame, to prepare for adding entities
void EntityFrame_Clear(entity_frame_t *f, vec3_t eye, int framenum)
{
	f->time = 0;
	f->framenum = framenum;
	f->numentities = 0;
	if (eye == NULL)
	{
		VectorClear(f->eye);
	}
	else
	{
		VectorCopy(eye, f->eye);
	}
}

// (server) adds an entity to frame
void EntityFrame_AddEntity(entity_frame_t *f, entity_state_t *s)
{
	if (f->numentities < MAX_ENTITY_DATABASE)
	{
		f->entitydata[f->numentities] = *s;
		f->entitydata[f->numentities++].active = true;
	}
}

// (server and client) reads a frame from the database
void EntityFrame_FetchFrame(entity_database_t *d, int framenum, entity_frame_t *f)
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

// (server and client) adds a entity_frame to the database, for future reference
void EntityFrame_AddFrame(entity_database_t *d, entity_frame_t *f)
{
	int n, e;
	entity_frameinfo_t *info;

	VectorCopy(f->eye, d->eye);

	// figure out how many entity slots are used already
	if (d->numframes)
	{
		n = d->frames[d->numframes - 1].endentity - d->frames[0].firstentity;
		if (n + f->numentities > MAX_ENTITY_DATABASE || d->numframes >= MAX_ENTITY_HISTORY)
		{
			// ran out of room, dump database
			EntityFrame_ClearDatabase(d);
		}
	}

	info = &d->frames[d->numframes];
	info->framenum = f->framenum;
	e = -1000;
	// make sure we check the newly added frame as well, but we haven't incremented numframes yet
	for (n = 0;n <= d->numframes;n++)
	{
		if (e >= d->frames[n].framenum)
		{
			if (e == f->framenum)
				Con_Printf("EntityFrame_AddFrame: tried to add out of sequence frame to database\n");
			else
				Con_Printf("EntityFrame_AddFrame: out of sequence frames in database\n");
			return;
		}
		e = d->frames[n].framenum;
	}
	// if database still has frames after that...
	if (d->numframes)
		info->firstentity = d->frames[d->numframes - 1].endentity;
	else
		info->firstentity = 0;
	info->endentity = info->firstentity + f->numentities;
	d->numframes++;

	n = info->firstentity % MAX_ENTITY_DATABASE;
	e = MAX_ENTITY_DATABASE - n;
	if (e > f->numentities)
		e = f->numentities;
	memcpy(d->entitydata + n, f->entitydata, sizeof(entity_state_t) * e);
	if (f->numentities > e)
		memcpy(d->entitydata, f->entitydata + e, sizeof(entity_state_t) * (f->numentities - e));
}

// (server) writes a frame to network stream
static entity_frame_t deltaframe; // FIXME?
void EntityFrame_Write(entity_database_t *d, entity_frame_t *f, sizebuf_t *msg)
{
	int i, onum, number;
	entity_frame_t *o = &deltaframe;
	entity_state_t *ent, *delta;

	EntityFrame_AddFrame(d, f);

	EntityFrame_FetchFrame(d, d->ackframe > 0 ? d->ackframe : -1, o);
	MSG_WriteByte (msg, svc_entities);
	MSG_WriteLong (msg, o->framenum);
	MSG_WriteLong (msg, f->framenum);
	MSG_WriteFloat (msg, f->eye[0]);
	MSG_WriteFloat (msg, f->eye[1]);
	MSG_WriteFloat (msg, f->eye[2]);

	onum = 0;
	for (i = 0;i < f->numentities;i++)
	{
		ent = f->entitydata + i;
		number = ent->number;
		for (;onum < o->numentities && o->entitydata[onum].number < number;onum++)
		{
			// write remove message
			MSG_WriteShort(msg, o->entitydata[onum].number | 0x8000);
		}
		if (onum < o->numentities && (o->entitydata[onum].number == number))
		{
			// delta from previous frame
			delta = o->entitydata + onum;
			// advance to next entity in delta frame
			onum++;
		}
		else
		{
			// delta from defaults
			delta = &defaultstate;
		}
		EntityState_Write(ent, msg, delta);
	}
	for (;onum < o->numentities;onum++)
	{
		// write remove message
		MSG_WriteShort(msg, o->entitydata[onum].number | 0x8000);
	}
	MSG_WriteShort(msg, 0xFFFF);
}

// (client) reads a frame from network stream
static entity_frame_t framedata; // FIXME?
void EntityFrame_Read(entity_database_t *d)
{
	int number, removed;
	entity_frame_t *f = &framedata, *delta = &deltaframe;
	entity_state_t *e, *old, *oldend;

	EntityFrame_Clear(f, NULL, -1);

	// read the frame header info
	f->time = cl.mtime[0];
	number = MSG_ReadLong();
	f->framenum = MSG_ReadLong();
	f->eye[0] = MSG_ReadFloat();
	f->eye[1] = MSG_ReadFloat();
	f->eye[2] = MSG_ReadFloat();
	EntityFrame_AckFrame(d, number);
	EntityFrame_FetchFrame(d, number, delta);
	old = delta->entitydata;
	oldend = old + delta->numentities;
	// read entities until we hit the magic 0xFFFF end tag
	while ((number = (unsigned short) MSG_ReadShort()) != 0xFFFF)
	{
		if (msg_badread)
			Host_Error("EntityFrame_Read: read error\n");
		removed = number & 0x8000;
		number &= 0x7FFF;
		if (number >= MAX_EDICTS)
			Host_Error("EntityFrame_Read: number (%i) >= MAX_EDICTS (%i)\n", number, MAX_EDICTS);

		// seek to entity, while copying any skipped entities (assume unchanged)
		while (old < oldend && old->number < number)
		{
			if (f->numentities >= MAX_ENTITY_DATABASE)
				Host_Error("EntityFrame_Read: entity list too big\n");
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
				Host_Error("EntityFrame_Read: entity list too big\n");

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

			EntityState_ReadUpdate(e, number);
		}
	}
	while (old < oldend)
	{
		if (f->numentities >= MAX_ENTITY_DATABASE)
			Host_Error("EntityFrame_Read: entity list too big\n");
		f->entitydata[f->numentities] = *old++;
		f->entitydata[f->numentities++].time = cl.mtime[0];
	}
	EntityFrame_AddFrame(d, f);
}


// (client) returns the frame number of the most recent frame recieved
int EntityFrame_MostRecentlyRecievedFrameNum(entity_database_t *d)
{
	if (d->numframes)
		return d->frames[d->numframes - 1].framenum;
	else
		return -1;
}






int EntityFrame4_SV_ChooseCommitToReplace(entity_database4_t *d)
{
	int i, best, bestframenum;
	best = 0;
	bestframenum = d->commit[0].framenum;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
	{
		if (!d->commit[i].numentities)
			return i;
		if (bestframenum > d->commit[i].framenum)
		{
			bestframenum = d->commit[i].framenum;
			best = i;
		}
	}
	return best;
}

entity_state_t *EntityFrame4_GetReferenceEntity(entity_database4_t *d, int number)
{
	if (d->maxreferenceentities <= number)
	{
		int oldmax = d->maxreferenceentities;
		entity_state_t *oldentity = d->referenceentity;
		d->maxreferenceentities = (number + 15) & ~7;
		d->referenceentity = Mem_Alloc(d->mempool, d->maxreferenceentities * sizeof(*d->referenceentity));
		if (oldentity)
		{
			memcpy(d->referenceentity, oldentity, oldmax * sizeof(*d->referenceentity));
			Mem_Free(oldentity);
		}
		// clear the newly created entities
		for (;oldmax < d->maxreferenceentities;oldmax++)
		{
			d->referenceentity[oldmax] = defaultstate;
			d->referenceentity[oldmax].number = oldmax;
		}
	}
	return d->referenceentity + number;
}

void EntityFrame4_AddCommitEntity(entity_database4_t *d, entity_state_t *s)
{
	// resize commit's entity list if full
	if (d->currentcommit->maxentities <= d->currentcommit->numentities)
	{
		entity_state_t *oldentity = d->currentcommit->entity;
		d->currentcommit->maxentities += 8;
		d->currentcommit->entity = Mem_Alloc(d->mempool, d->currentcommit->maxentities * sizeof(*d->currentcommit->entity));
		if (oldentity)
		{
			memcpy(d->currentcommit->entity, oldentity, d->currentcommit->numentities * sizeof(*d->currentcommit->entity));
			Mem_Free(oldentity);
		}
	}
	d->currentcommit->entity[d->currentcommit->numentities++] = *s;
}

entity_database4_t *EntityFrame4_AllocDatabase(mempool_t *pool)
{
	entity_database4_t *d;
	d = Mem_Alloc(pool, sizeof(*d));
	d->mempool = pool;
	EntityFrame4_ResetDatabase(d);
	d->ackframenum = -1;
	return d;
}

void EntityFrame4_FreeDatabase(entity_database4_t *d)
{
	int i;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		if (d->commit[i].entity)
			Mem_Free(d->commit[i].entity);
	if (d->referenceentity)
		Mem_Free(d->referenceentity);
	Mem_Free(d);
}

void EntityFrame4_ResetDatabase(entity_database4_t *d)
{
	int i;
	d->referenceframenum = -1;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		d->commit[i].numentities = 0;
	for (i = 0;i < d->maxreferenceentities;i++)
		d->referenceentity[i] = defaultstate;
}

void EntityFrame4_AckFrame(entity_database4_t *d, int framenum)
{
	int i, j;
	entity_database4_commit_t *commit;
	// check if the peer is requesting no delta compression
	if (framenum == -1)
	{
		EntityFrame4_ResetDatabase(d);
		return;
	}
	// return early if this is already the reference frame
	if (d->referenceframenum == framenum)
		return;
	// find the frame in the database
	for (i = 0, commit = d->commit;i < MAX_ENTITY_HISTORY;i++, commit++)
		if (commit->numentities && commit->framenum == framenum)
			break;
	// check if the frame was found
	if (i == MAX_ENTITY_HISTORY)
	{
		Con_Printf("EntityFrame4_AckFrame: frame %i not found in database, expect glitches!  (VERY BAD ERROR)\n", framenum);
		d->ackframenum = -1;
		EntityFrame4_ResetDatabase(d);
		return;
	}
	// apply commit to database
	d->referenceframenum = framenum;
	for (j = 0;j < commit->numentities;j++)
	{
		//*EntityFrame4_GetReferenceEntity(d, commit->entity[j].number) = commit->entity[j];
		entity_state_t *s = EntityFrame4_GetReferenceEntity(d, commit->entity[j].number);
		if (commit->entity[j].active != s->active)
		{
			if (commit->entity[j].active)
				Con_Printf("commit entity %i has become active (modelindex %i)\n", commit->entity[j].number, commit->entity[j].modelindex);
			else
				Con_Printf("commit entity %i has become inactive (modelindex %i)\n", commit->entity[j].number, commit->entity[j].modelindex);
		}
		*s = commit->entity[j];
	}
	// purge the now-obsolete updates
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		if (d->commit[i].framenum <= framenum)
			d->commit[i].numentities = 0;
}

void EntityFrame4_SV_WriteFrame_Begin(entity_database4_t *d, sizebuf_t *msg, int framenum)
{
	d->currentcommit = d->commit + EntityFrame4_SV_ChooseCommitToReplace(d);
	d->currentcommit->numentities = 0;
	d->currentcommit->framenum = framenum;
	MSG_WriteByte(msg, svc_entities);
	MSG_WriteLong(msg, d->referenceframenum);
	MSG_WriteLong(msg, d->currentcommit->framenum);
}

int EntityFrame4_SV_WriteFrame_Entity(entity_database4_t *d, sizebuf_t *msg, int maxbytes, entity_state_t *s)
{
	qbyte data[128];
	sizebuf_t buf;
	entity_state_t *e;
	// prepare the buffer
	memset(&buf, 0, sizeof(buf));
	buf.data = data;
	buf.maxsize = sizeof(data);
	// make the message
	e = EntityFrame4_GetReferenceEntity(d, s->number);
	// send an update (may update or remove the entity)
	EntityState_Write(s, &buf, e);
	// if the message is empty, skip out now
	if (!buf.cursize)
		return true;
	// if the commit is full, we're done
	if (msg->cursize + buf.cursize + 2 >= min(msg->maxsize, maxbytes))
		return false;
	// add the entity to the commit
	EntityFrame4_AddCommitEntity(d, s);
	// write the message to the packet
	SZ_Write(msg, buf.data, buf.cursize);
	// carry on
	return true;
}

void EntityFrame4_SV_WriteFrame_End(entity_database4_t *d, sizebuf_t *msg)
{
	// remove world message (invalid, and thus a good terminator)
	MSG_WriteShort(msg, 0x8000);
	// just to be sure
	d->currentcommit = NULL;
}

extern void CL_MoveLerpEntityStates(entity_t *ent);
void EntityFrame4_CL_ReadFrame(entity_database4_t *d)
{
	int i, n, cnumber, referenceframenum, framenum, enumber, done, stopnumber, skip = false;
	// read the number of the frame this refers to
	referenceframenum = MSG_ReadLong();
	// read the number of this frame
	framenum = MSG_ReadLong();
	// read the start number
	enumber = MSG_ReadShort();
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		if (!d->commit[i].numentities)
			break;
	if (i < MAX_ENTITY_HISTORY)
	{
		d->currentcommit = d->commit + i;
		d->currentcommit->framenum = d->ackframenum = framenum;
		d->currentcommit->numentities = 0;
		EntityFrame4_AckFrame(d, referenceframenum);
		if (d->ackframenum == -1)
		{
			Con_Printf("EntityFrame4_CL_ReadFrame: reference frame invalid, this update will be skipped\n");
			skip = true;
		}
	}
	else
	{
		Con_Printf("EntityFrame4_CL_ReadFrame: error while decoding frame %i: database full, reading but not storing this update\n", framenum);
		skip = true;
	}
	done = false;
	while (!done && !msg_badread)
	{
		// read the number of the modified entity
		// (gaps will be copied unmodified)
		n = (unsigned short)MSG_ReadShort();
		if (n == 0x8000)
		{
			// no more entities in this update, but we still need to copy the
			// rest of the reference entities (final gap)
			done = true;
			// read end of range number, then process normally
			n = (unsigned short)MSG_ReadShort();
		}
		// high bit means it's a remove message
		cnumber = n & 0x7FFF;
		// add one (the changed one) if not done
		stopnumber = cnumber + !done;
		// process entities in range from the last one to the changed one
		for (;enumber < stopnumber;enumber++)
		{
			if (skip)
			{
				if (enumber == cnumber && (n & 0x8000) == 0)
				{
					entity_state_t tempstate;
					EntityState_ReadUpdate(&tempstate, enumber);
				}
				continue;
			}
			// slide the current into the previous slot
			cl_entities[enumber].state_previous = cl_entities[enumber].state_current;
			// copy a new current from reference database
			cl_entities[enumber].state_current = *EntityFrame4_GetReferenceEntity(d, enumber);
			// if this is the one to modify, read more data...
			if (enumber == cnumber)
			{
				if (n & 0x8000)
				{
					// simply removed
					if (developer_networkentities.integer)
						Con_Printf("entity %i: remove\n", enumber);
					cl_entities[enumber].state_current = defaultstate;
				}
				else
				{
					// read the changes
					if (developer_networkentities.integer)
						Con_Printf("entity %i: update\n", enumber);
					EntityState_ReadUpdate(&cl_entities[enumber].state_current, enumber);
				}
			}
			//else if (developer_networkentities.integer)
			//	Con_Printf("entity %i: copy\n", enumber);
			// fix the number (it gets wiped occasionally by copying from defaultstate)
			cl_entities[enumber].state_current.number = enumber;
			// check if we need to update the lerp stuff
			if (cl_entities[enumber].state_current.active)
			{
				CL_MoveLerpEntityStates(&cl_entities[enumber]);
				cl_entities_active[enumber] = true;
			}
			// add this to the commit entry whether it is modified or not
			if (d->currentcommit)
				EntityFrame4_AddCommitEntity(d, &cl_entities[enumber].state_current);
			// print extra messages if desired
			if (developer_networkentities.integer && cl_entities[enumber].state_current.active != cl_entities[enumber].state_previous.active)
			{
				if (cl_entities[enumber].state_current.active)
					Con_Printf("entity #%i has become active\n", enumber);
				else if (cl_entities[enumber].state_previous.active)
					Con_Printf("entity #%i has become inactive\n", enumber);
			}
		}
	}
	d->currentcommit = NULL;
}


