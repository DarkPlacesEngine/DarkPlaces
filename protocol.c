
#include "quakedef.h"

void ClearStateToDefault(entity_state_t *s)
{
	s->active = 0;
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
void EntityFrame_Clear(entity_frame_t *f, vec3_t eye)
{
	f->time = 0;
	f->framenum = 0;
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

// (server and client) reads a frame from the database
void EntityFrame_FetchFrame(entity_database_t *d, int framenum, entity_frame_t *f)
{
	int i, n;
	EntityFrame_Clear(f, NULL);
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
	else
		f->framenum = -1;
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
void EntityFrame_Write(entity_database_t *d, entity_frame_t *f, sizebuf_t *msg)
{
	int i, onum, bits, number;
	entity_frame_t deltaframe, *o = &deltaframe;
	entity_state_t *ent, *delta, baseline;

	EntityFrame_AddFrame(d, f);

	ClearStateToDefault(&baseline);
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
			// delta from baseline
			delta = &baseline;
		}
		bits = 0;
		if (ent->flags & RENDER_LOWPRECISION)
		{
			if ((int) ent->origin[0] != (int) delta->origin[0])
				bits |= E_ORIGIN1;
			if ((int) ent->origin[1] != (int) delta->origin[1])
				bits |= E_ORIGIN2;
			if ((int) ent->origin[2] != (int) delta->origin[2])
				bits |= E_ORIGIN3;
		}
		else
		{
			if (fabs(ent->origin[0] - delta->origin[0]) > 0.01f)
				bits |= E_ORIGIN1;
			if (fabs(ent->origin[1] - delta->origin[1]) > 0.01f)
				bits |= E_ORIGIN2;
			if (fabs(ent->origin[2] - delta->origin[2]) > 0.01f)
				bits |= E_ORIGIN3;
		}
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

		if (bits) // don't send anything if it hasn't changed
		{
			if (bits & 0xFF000000)
				bits |= E_EXTEND3;
			if (bits & 0x00FF0000)
				bits |= E_EXTEND2;
			if (bits & 0x0000FF00)
				bits |= E_EXTEND1;

			MSG_WriteShort(msg, number);
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
					MSG_WriteShort(msg, ent->origin[0]);
				if (bits & E_ORIGIN2)
					MSG_WriteShort(msg, ent->origin[1]);
				if (bits & E_ORIGIN3)
					MSG_WriteShort(msg, ent->origin[2]);
			}
			else
			{
				if (bits & E_ORIGIN1)
					MSG_WriteFloat(msg, ent->origin[0]);
				if (bits & E_ORIGIN2)
					MSG_WriteFloat(msg, ent->origin[1]);
				if (bits & E_ORIGIN3)
					MSG_WriteFloat(msg, ent->origin[2]);
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
		}
	}
	for (;onum < o->numentities;onum++)
	{
		// write remove message
		MSG_WriteShort(msg, o->entitydata[onum].number | 0x8000);
	}
	MSG_WriteShort(msg, 0xFFFF);
}

// (client) reads a frame from network stream
void EntityFrame_Read(entity_database_t *d)
{
	int number, removed, bits;
	entity_frame_t framedata, *f = &framedata, deltaframedata, *delta = &deltaframedata;
	entity_state_t *e, baseline, *old, *oldend;

	ClearStateToDefault(&baseline);

	EntityFrame_Clear(f, NULL);

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
			memcpy(f->entitydata + f->numentities, old, sizeof(entity_state_t));
			f->entitydata[f->numentities].time = cl.mtime[0];
			old++;
			f->numentities++;
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
				memcpy(e, old++, sizeof(*e));
			}
			else
			{
				// delta from baseline
				memcpy(e, &baseline, sizeof(*e));
			}

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

			if (dpprotocol == DPPROTOCOL_VERSION2)
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
				if (e->flags & RENDER_LOWPRECISION || dpprotocol == DPPROTOCOL_VERSION2)
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
			if (dpprotocol == DPPROTOCOL_VERSION2)
				if (bits & E_FLAGS)
					e->flags = MSG_ReadByte();
		}
	}
	while (old < oldend)
	{
		if (f->numentities >= MAX_ENTITY_DATABASE)
			Host_Error("EntityFrame_Read: entity list too big\n");
		memcpy(f->entitydata + f->numentities, old, sizeof(entity_state_t));
		f->entitydata[f->numentities].time = cl.mtime[0];
		old++;
		f->numentities++;
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

