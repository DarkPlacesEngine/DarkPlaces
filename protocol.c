
#include "quakedef.h"

// this is 80 bytes
entity_state_t defaultstate =
{
	// ! means this is sent to client
	0,//double time; // time this state was built (used on client for interpolation)
	{0,0,0},//float origin[3]; // !
	{0,0,0},//float angles[3]; // !
	0,//int number; // ! entity number this state is for
	0,//int effects; // !
	0,//unsigned short modelindex; // !
	0,//unsigned short frame; // !
	0,//unsigned short tagentity; // !
	0,//unsigned short specialvisibilityradius; // larger if it has effects/light
	0,//unsigned short viewmodelforclient;
	0,//unsigned short exteriormodelforclient; // not shown if first person viewing from this entity, shown in all other cases
	0,//unsigned short nodrawtoclient;
	0,//unsigned short drawonlytoclient;
	{0,0,0,0},//unsigned short light[4]; // ! color*256 (0.00 to 255.996), and radius*1
	0,//unsigned char active; // ! true if a valid state
	0,//unsigned char lightstyle; // !
	0,//unsigned char lightpflags; // !
	0,//unsigned char colormap; // !
	0,//unsigned char skin; // ! also chooses cubemap for rtlights if lightpflags & LIGHTPFLAGS_FULLDYNAMIC
	255,//unsigned char alpha; // !
	16,//unsigned char scale; // !
	0,//unsigned char glowsize; // !
	254,//unsigned char glowcolor; // !
	0,//unsigned char flags; // !
	0,//unsigned char tagindex; // !
	// padding to a multiple of 8 bytes (to align the double time)
	{0,0,0,0,0}//unsigned char unused[5];
};

void ClearStateToDefault(entity_state_t *s)
{
	*s = defaultstate;
}

int EntityState_DeltaBits(const entity_state_t *o, const entity_state_t *n)
{
	unsigned int bits;
	// if o is not active, delta from default
	if (!o->active)
		o = &defaultstate;
	bits = 0;
	if (fabs(n->origin[0] - o->origin[0]) > (1.0f / 256.0f))
		bits |= E_ORIGIN1;
	if (fabs(n->origin[1] - o->origin[1]) > (1.0f / 256.0f))
		bits |= E_ORIGIN2;
	if (fabs(n->origin[2] - o->origin[2]) > (1.0f / 256.0f))
		bits |= E_ORIGIN3;
	if ((qbyte) (n->angles[0] * (256.0f / 360.0f)) != (qbyte) (o->angles[0] * (256.0f / 360.0f)))
		bits |= E_ANGLE1;
	if ((qbyte) (n->angles[1] * (256.0f / 360.0f)) != (qbyte) (o->angles[1] * (256.0f / 360.0f)))
		bits |= E_ANGLE2;
	if ((qbyte) (n->angles[2] * (256.0f / 360.0f)) != (qbyte) (o->angles[2] * (256.0f / 360.0f)))
		bits |= E_ANGLE3;
	if ((n->modelindex ^ o->modelindex) & 0x00FF)
		bits |= E_MODEL1;
	if ((n->modelindex ^ o->modelindex) & 0xFF00)
		bits |= E_MODEL2;
	if ((n->frame ^ o->frame) & 0x00FF)
		bits |= E_FRAME1;
	if ((n->frame ^ o->frame) & 0xFF00)
		bits |= E_FRAME2;
	if ((n->effects ^ o->effects) & 0x00FF)
		bits |= E_EFFECTS1;
	if ((n->effects ^ o->effects) & 0xFF00)
		bits |= E_EFFECTS2;
	if (n->colormap != o->colormap)
		bits |= E_COLORMAP;
	if (n->skin != o->skin)
		bits |= E_SKIN;
	if (n->alpha != o->alpha)
		bits |= E_ALPHA;
	if (n->scale != o->scale)
		bits |= E_SCALE;
	if (n->glowsize != o->glowsize)
		bits |= E_GLOWSIZE;
	if (n->glowcolor != o->glowcolor)
		bits |= E_GLOWCOLOR;
	if (n->flags != o->flags)
		bits |= E_FLAGS;
	if (n->tagindex != o->tagindex || n->tagentity != o->tagentity)
		bits |= E_TAGATTACHMENT;
	if (n->light[0] != o->light[0] || n->light[1] != o->light[1] || n->light[2] != o->light[2] || n->light[3] != o->light[3])
		bits |= E_LIGHT;
	if (n->lightstyle != o->lightstyle)
		bits |= E_LIGHTSTYLE;
	if (n->lightpflags != o->lightpflags)
		bits |= E_LIGHTPFLAGS;

	if (bits)
	{
		if (bits &  0xFF000000)
			bits |= 0x00800000;
		if (bits &  0x00FF0000)
			bits |= 0x00008000;
		if (bits &  0x0000FF00)
			bits |= 0x00000080;
	}
	return bits;
}

void EntityState_WriteExtendBits(sizebuf_t *msg, unsigned int bits)
{
	MSG_WriteByte(msg, bits & 0xFF);
	if (bits & 0x00000080)
	{
		MSG_WriteByte(msg, (bits >> 8) & 0xFF);
		if (bits & 0x00008000)
		{
			MSG_WriteByte(msg, (bits >> 16) & 0xFF);
			if (bits & 0x00800000)
				MSG_WriteByte(msg, (bits >> 24) & 0xFF);
		}
	}
}

void EntityState_WriteFields(entity_state_t *ent, sizebuf_t *msg, unsigned int bits)
{
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
		if (bits & E_ANGLE1)
			MSG_WriteAngle(msg, ent->angles[0]);
		if (bits & E_ANGLE2)
			MSG_WriteAngle(msg, ent->angles[1]);
		if (bits & E_ANGLE3)
			MSG_WriteAngle(msg, ent->angles[2]);
	}
	else
	{
		if (bits & E_ORIGIN1)
			MSG_WriteFloat(msg, ent->origin[0]);
		if (bits & E_ORIGIN2)
			MSG_WriteFloat(msg, ent->origin[1]);
		if (bits & E_ORIGIN3)
			MSG_WriteFloat(msg, ent->origin[2]);
		if (bits & E_ANGLE1)
			MSG_WritePreciseAngle(msg, ent->angles[0]);
		if (bits & E_ANGLE2)
			MSG_WritePreciseAngle(msg, ent->angles[1]);
		if (bits & E_ANGLE3)
			MSG_WritePreciseAngle(msg, ent->angles[2]);
	}
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
	if (bits & E_LIGHT)
	{
		MSG_WriteShort(msg, ent->light[0]);
		MSG_WriteShort(msg, ent->light[1]);
		MSG_WriteShort(msg, ent->light[2]);
		MSG_WriteShort(msg, ent->light[3]);
	}
	if (bits & E_LIGHTSTYLE)
		MSG_WriteByte(msg, ent->lightstyle);
	if (bits & E_LIGHTPFLAGS)
		MSG_WriteByte(msg, ent->lightpflags);
}

void EntityState_WriteUpdate(entity_state_t *ent, sizebuf_t *msg, entity_state_t *delta)
{
	unsigned int bits;
	if (ent->active)
	{
		// entity is active, check for changes from the delta
		if ((bits = EntityState_DeltaBits(delta, ent)))
		{
			// write the update number, bits, and fields
			MSG_WriteShort(msg, ent->number);
			EntityState_WriteExtendBits(msg, bits);
			EntityState_WriteFields(ent, msg, bits);
		}
	}
	else
	{
		// entity is inactive, check if the delta was active
		if (delta->active)
		{
			// write the remove number
			MSG_WriteShort(msg, ent->number | 0x8000);
		}
	}
}

int EntityState_ReadExtendBits(void)
{
	unsigned int bits;
	bits = MSG_ReadByte();
	if (bits & 0x00000080)
	{
		bits |= MSG_ReadByte() << 8;
		if (bits & 0x00008000)
		{
			bits |= MSG_ReadByte() << 16;
			if (bits & 0x00800000)
				bits |= MSG_ReadByte() << 24;
		}
	}
	return bits;
}

void EntityState_ReadFields(entity_state_t *e, unsigned int bits)
{
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
	if (cl.protocol == PROTOCOL_DARKPLACES5 && !(e->flags & RENDER_LOWPRECISION))
	{
		if (bits & E_ANGLE1)
			e->angles[0] = MSG_ReadPreciseAngle();
		if (bits & E_ANGLE2)
			e->angles[1] = MSG_ReadPreciseAngle();
		if (bits & E_ANGLE3)
			e->angles[2] = MSG_ReadPreciseAngle();
	}
	else
	{
		if (bits & E_ANGLE1)
			e->angles[0] = MSG_ReadAngle();
		if (bits & E_ANGLE2)
			e->angles[1] = MSG_ReadAngle();
		if (bits & E_ANGLE3)
			e->angles[2] = MSG_ReadAngle();
	}
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
	if (bits & E_LIGHT)
	{
		e->light[0] = MSG_ReadShort();
		e->light[1] = MSG_ReadShort();
		e->light[2] = MSG_ReadShort();
		e->light[3] = MSG_ReadShort();
	}
	if (bits & E_LIGHTSTYLE)
		e->lightstyle = MSG_ReadByte();
	if (bits & E_LIGHTPFLAGS)
		e->lightpflags = MSG_ReadByte();

	if (developer_networkentities.integer >= 2)
	{
		Con_Printf("ReadFields e%i", e->number);

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
			Con_Printf(" E_GLOWSIZE %i", e->glowsize * 4);
		if (bits & E_GLOWCOLOR)
			Con_Printf(" E_GLOWCOLOR %i", e->glowcolor);
		
		if (bits & E_LIGHT)
			Con_Printf(" E_LIGHT %i:%i:%i:%i", e->light[0], e->light[1], e->light[2], e->light[3]);
		if (bits & E_LIGHTPFLAGS)			
			Con_Printf(" E_LIGHTPFLAGS %i", e->lightpflags);

		if (bits & E_TAGATTACHMENT)
			Con_Printf(" E_TAGATTACHMENT e%i:%i", e->tagentity, e->tagindex);
		if (bits & E_LIGHTSTYLE)
			Con_Printf(" E_LIGHTSTYLE %i", e->lightstyle);
		Con_Print("\n");
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
		EntityState_WriteUpdate(ent, msg, delta);
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

			cl_entities_active[number] = true;
			e->active = true;
			e->time = cl.mtime[0];
			e->number = number;
			EntityState_ReadFields(e, EntityState_ReadExtendBits());
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
	d->ackframenum = -1;
	d->referenceframenum = -1;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		d->commit[i].numentities = 0;
	for (i = 0;i < d->maxreferenceentities;i++)
		d->referenceentity[i] = defaultstate;
}

int EntityFrame4_AckFrame(entity_database4_t *d, int framenum)
{
	int i, j, found;
	entity_database4_commit_t *commit;
	if (framenum == -1)
	{
		// reset reference, but leave commits alone
		d->referenceframenum = -1;
		for (i = 0;i < d->maxreferenceentities;i++)
			d->referenceentity[i] = defaultstate;
		found = true;
	}
	else if (d->referenceframenum == framenum)
		found = true;
	else
	{
		found = false;
		for (i = 0, commit = d->commit;i < MAX_ENTITY_HISTORY;i++, commit++)
		{
			if (commit->numentities && commit->framenum <= framenum)
			{
				if (commit->framenum == framenum)
				{
					found = true;
					d->referenceframenum = framenum;
					if (developer_networkentities.integer >= 3)
					{
						for (j = 0;j < commit->numentities;j++)
						{
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
					}
					else
						for (j = 0;j < commit->numentities;j++)
							*EntityFrame4_GetReferenceEntity(d, commit->entity[j].number) = commit->entity[j];
				}
				commit->numentities = 0;
			}
		}
	}
	if (developer_networkentities.integer >= 1)
	{
		Con_Printf("ack ref:%i database updated to: ref:%i commits:", framenum, d->referenceframenum);
		for (i = 0;i < MAX_ENTITY_HISTORY;i++)
			if (d->commit[i].numentities)
				Con_Printf(" %i", d->commit[i].framenum);
		Con_Print("\n");
	}
	return found;
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
	// make the update message
	e = EntityFrame4_GetReferenceEntity(d, s->number);
	EntityState_WriteUpdate(s, &buf, e);
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

extern void CL_MoveLerpEntityStates(entity_t *ent);
void EntityFrame4_CL_ReadFrame(entity_database4_t *d)
{
	int i, n, cnumber, referenceframenum, framenum, enumber, done, stopnumber, skip = false;
	entity_state_t *s;
	// read the number of the frame this refers to
	referenceframenum = MSG_ReadLong();
	// read the number of this frame
	framenum = MSG_ReadLong();
	// read the start number
	enumber = MSG_ReadShort();
	if (developer_networkentities.integer >= 1)
	{
		Con_Printf("recv svc_entities num:%i ref:%i database: ref:%i commits:", framenum, referenceframenum, d->referenceframenum);
		for (i = 0;i < MAX_ENTITY_HISTORY;i++)
			if (d->commit[i].numentities)
				Con_Printf(" %i", d->commit[i].framenum);
		Con_Print("\n");
	}
	if (!EntityFrame4_AckFrame(d, referenceframenum))
	{
		Con_Print("EntityFrame4_CL_ReadFrame: reference frame invalid (VERY BAD ERROR), this update will be skipped\n");
		skip = true;
	}
	d->currentcommit = NULL;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
	{
		if (!d->commit[i].numentities)
		{
			d->currentcommit = d->commit + i;
			d->currentcommit->framenum = d->ackframenum = framenum;
			d->currentcommit->numentities = 0;
		}
	}
	if (d->currentcommit == NULL)
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
					EntityState_ReadFields(&tempstate, EntityState_ReadExtendBits());
				}
				continue;
			}
			// slide the current into the previous slot
			cl_entities[enumber].state_previous = cl_entities[enumber].state_current;
			// copy a new current from reference database
			cl_entities[enumber].state_current = *EntityFrame4_GetReferenceEntity(d, enumber);
			s = &cl_entities[enumber].state_current;
			// if this is the one to modify, read more data...
			if (enumber == cnumber)
			{
				if (n & 0x8000)
				{
					// simply removed
					if (developer_networkentities.integer >= 2)
						Con_Printf("entity %i: remove\n", enumber);
					*s = defaultstate;
				}
				else
				{
					// read the changes
					if (developer_networkentities.integer >= 2)
						Con_Printf("entity %i: update\n", enumber);
					s->active = true;
					EntityState_ReadFields(s, EntityState_ReadExtendBits());
				}
			}
			else if (developer_networkentities.integer >= 4)
				Con_Printf("entity %i: copy\n", enumber);
			// set the cl_entities_active flag
			cl_entities_active[enumber] = s->active;
			// set the update time
			s->time = cl.mtime[0];
			// fix the number (it gets wiped occasionally by copying from defaultstate)
			s->number = enumber;
			// check if we need to update the lerp stuff
			if (s->active)
				CL_MoveLerpEntityStates(&cl_entities[enumber]);
			// add this to the commit entry whether it is modified or not
			if (d->currentcommit)
				EntityFrame4_AddCommitEntity(d, &cl_entities[enumber].state_current);
			// print extra messages if desired
			if (developer_networkentities.integer >= 2 && cl_entities[enumber].state_current.active != cl_entities[enumber].state_previous.active)
			{
				if (cl_entities[enumber].state_current.active)
					Con_Printf("entity #%i has become active\n", enumber);
				else if (cl_entities[enumber].state_previous.active)
					Con_Printf("entity #%i has become inactive\n", enumber);
			}
		}
	}
	d->currentcommit = NULL;
	if (skip)
		EntityFrame4_ResetDatabase(d);
}





/*
int EntityState5_PriorityForChangedBits(int changedbits)
{
	if (changedbits & E5_ISACTIVE)
		return 2;
	else if (changedbits & (E5_FLAGS | E5_ATTACHMENT | E5_MODEL | E5_SKIN | E5_EXTERIORFORENTITY | E5_COLORMAP | E5_LIGHT | E5_GLOW | E5_EFFECTS | E5_ORIGIN | E5_ANGLES | E5_FRAME | E5_ALPHA | E5_SCALE))
		return 1;
	else
		return 0;
}

void EntityState5_WriteUpdate(int number, entitystate_t *s, int changedbits, sizebuf_t *msg)
{
	bits = 0;
	if (!s->active)
		MSG_WriteShort(msg, number | 0x8000);
	else
	{
		bits |= E5_ISACTIVE;
		if (changedbits & E5_ORIGIN)
		{
			bits |= E5_ORIGIN;
			if (s->origin[0] < -4096 || s->origin[0] >= 4096 || s->origin[1] < -4096 || s->origin[1] >= 4096 || s->origin[2] < -4096 || s->origin[2] >= 4096)
				bits |= E5_ORIGIN32;
		}
		if (changedbits & E5_ANGLES)
		{
			bits |= E5_ANGLES;
			if (!(s->flags & RENDERFLAGS_LOWPRECISION))
				bits |= E5_ANGLES16;
		}
		if (changedbits & E5_MODEL)
		{
			bits |= E5_MODEL;
			if (s->modelindex >= 256)
				bits |= E5_MODEL16;
		}
		if (changedbits & E5_FRAME)
		{
			bits |= E5_FRAME;
			if (s->frame >= 256)
				bits |= E5_FRAME16;
		}
		if (changedbits & E5_SKIN)
			bits |= E5_SKIN;
		if (changedbits & E5_EFFECTS)
		{
			bits |= E5_EFFECTS;
			if (s->modelindex >= 256)
				bits |= E5_MODEL16;
		}
		if (changedbits & E5_FLAGS)
			bits |= E5_FLAGS;
		if (changedbits & E5_ALPHA)
			bits |= E5_ALPHA;
		if (changedbits & E5_SCALE)
			bits |= E5_SCALE;
		if (changedbits & E5_ATTACHMENT)
			bits |= E5_ATTACHMENT;
		if (changedbits & E5_EXTERIORFORENTITY)
			bits |= E5_EXTERIORFORENTITY;
		if (changedbits & E5_LIGHT)
			bits |= E5_LIGHT;
		if (changedbits & E5_COLORMAP)
			bits |= E5_COLORMAP;
		if (changedbits & E5_GLOW)
			bits |= E5_GLOW;
		if (bits >= 256)
			bits |= E5_EXTEND1;
		if (bits >= 65536)
			bits |= E5_EXTEND2;
		if (bits >= 16777216)
			bits |= E5_EXTEND3;
		MSG_WriteShort(msg, number);
		MSG_WriteByte(msg, bits & 0xFF);
		if (bits & E5_EXTEND1)
			MSG_WriteByte(msg, (bits >> 8) & 0xFF);
		if (bits & E5_EXTEND2)
			MSG_WriteByte(msg, (bits >> 16) & 0xFF);
		if (bits & E5_EXTEND3)
			MSG_WriteByte(msg, (bits >> 24) & 0xFF);
		if (bits & E5_FLAGS)
			MSG_WriteByte(msg, s->flags);
		if (bits & E5_ORIGIN)
		{
			if (bits & E5_ORIGIN32)
			{
				MSG_WriteFloat(msg, s->origin[0]);
				MSG_WriteFloat(msg, s->origin[1]);
				MSG_WriteFloat(msg, s->origin[2]);
			}
			else
			{
				MSG_WriteShort(msg, (int)floor(s->origin[0] * 8 + 0.5f));
				MSG_WriteShort(msg, (int)floor(s->origin[1] * 8 + 0.5f));
				MSG_WriteShort(msg, (int)floor(s->origin[2] * 8 + 0.5f));
			}
		}
		if (bits & E5_ANGLES)
		{
			if (bits & E5_ANGLES16)
			{
				MSG_WriteShort(msg, (int)floor(s->angles[0] * (65536.0f / 360.0f) + 0.5f));
				MSG_WriteShort(msg, (int)floor(s->angles[1] * (65536.0f / 360.0f) + 0.5f));
				MSG_WriteShort(msg, (int)floor(s->angles[2] * (65536.0f / 360.0f) + 0.5f));
			}
			else
			{
				MSG_WriteByte(msg, (int)floor(s->angles[0] * (256.0f / 360.0f) + 0.5f));
				MSG_WriteByte(msg, (int)floor(s->angles[1] * (256.0f / 360.0f) + 0.5f));
				MSG_WriteByte(msg, (int)floor(s->angles[2] * (256.0f / 360.0f) + 0.5f));
			}
		}
		if (bits & E5_MODEL)
		{
			if (bits & E5_MODEL16)
				MSG_WriteShort(msg, s->modelindex);
			else
				MSG_WriteByte(msg, s->modelindex);
		}
		if (bits & E5_FRAME)
		{
			if (bits & E5_FRAME16)
				MSG_WriteShort(msg, s->frame);
			else
				MSG_WriteByte(msg, s->frame);
		}
		if (bits & E5_SKIN)
			MSG_WriteByte(msg, s->flags);
		if (bits & E5_EFFECTS)
		{
			if (bits & E5_EFFECTS32)
				MSG_WriteLong(msg, s->effects);
			else if (bits & E5_EFFECTS16)
				MSG_WriteShort(msg, s->effects);
			else
				MSG_WriteByte(msg, s->effects);
		}
		if (bits & E5_ALPHA)
			MSG_WriteByte(msg, s->flags);
		if (bits & E5_SCALE)
			MSG_WriteByte(msg, s->flags);
		if (bits & E5_ATTACHMENT)
		{
			MSG_WriteShort(msg, s->tagentity);
			MSG_WriteByte(msg, s->tagindex);
		}
		if (bits & E5_EXTERIORFORENTITY)
			MSG_WriteShort(msg, s->tagentity);
		if (bits & E5_LIGHT)
		{
			MSG_WriteShort(msg, s->light[0]);
			MSG_WriteShort(msg, s->light[1]);
			MSG_WriteShort(msg, s->light[2]);
			MSG_WriteShort(msg, s->light[3]);
		}
		if (bits & E5_COLORMAP)
			MSG_WriteByte(msg, s->colormap);
		if (bits & E5_GLOW)
		{
			MSG_WriteByte(msg, s->glowsize);
			MSG_WriteByte(msg, s->glowcolor);
		}
	}
}

int EntityFrame5_ReadUpdate(void)
{
	number = MSG_ReadShort();
	e = cl_entities + (number & 0x7FFF);
	e->state_previous = e->state_current;
	if (number & 0x8000)
	{
		if (number == 0x8000)
		{
			// end of entity list
			return false;
		}
		// remove
		number &= 0x7FFF;
		e->state_current = defaultstate;
		e->state_current.number = number;
		return true;
	}
	else
	{
	}
}

int cl_entityframe5_lastreceivedframenum;

void EntityFrame5_CL_ReadFrame(void)
{
	int n, enumber;
	entity_t *ent;
	entity_state_t *s;
	// read the number of this frame to echo back in next input packet
	cl_entityframe5_lastreceivedframenum = MSG_ReadLong();
	// read entity numbers until we find a 0x8000
	// (which would be remove world entity, but is actually a terminator)
	while ((n = MSG_ReadShort()) != 0x8000)
	{
		// get the entity number and look it up
		enumber = n & 0x7FFF;
		ent = cl_entities + enumber;
		// slide the current into the previous slot
		ent->state_previous = ent->state_current;
		// read the update
		s = &ent->state_current;
		if (n & 0x8000)
		{
			// remove entity
			*s = defaultstate;
		}
		else
		{
			// update entity
			s->active = true;
			EntityState_ReadFields(s, EntityState_ReadExtendBits());
		}
		// set the cl_entities_active flag
		cl_entities_active[enumber] = s->active;
		// set the update time
		s->time = cl.mtime[0];
		// fix the number (it gets wiped occasionally by copying from defaultstate)
		s->number = enumber;
		// check if we need to update the lerp stuff
		if (s->active)
			CL_MoveLerpEntityStates(&cl_entities[enumber]);
		// print extra messages if desired
		if (developer_networkentities.integer >= 2 && cl_entities[enumber].state_current.active != cl_entities[enumber].state_previous.active)
		{
			if (cl_entities[enumber].state_current.active)
				Con_Printf("entity #%i has become active\n", enumber);
			else if (cl_entities[enumber].state_previous.active)
				Con_Printf("entity #%i has become inactive\n", enumber);
		}
	}
}

#define ENTITYFRAME5_MAXPACKETLOGS 64
#define ENTITYFRAME5_MAXSTATES 128

typedef struct entityframe5_state_s
{
	unsigned short entitynumber;
	qbyte active;
	qbyte activedirtybit;
	int dirtybits;
}
entityframe5_state_t;

typedef struct entityframe5_packetlog_s
{
	int packetnumber;
	int numstates;
	entityframe5_state_t states[ENTITYFRAME5_MAXSTATES];
}
entityframe5_packetlog_t;

typedef struct entityframe5_s
{
	int ackedframenum;
	entityframe5_packetlog_t packetlog[ENTITYFRAME5_MAXPACKETLOGS];
	qbyte activedirtybits[(MAX_EDICTS + 7) / 8];
	int dirtybits[MAX_EDICTS];
}
entityframe5_t;

void EntityFrame5_AckFrame(entityframe5_t *d, int framenum)
{
	int i, j, k, l, dirtybits, activedirtybit;
	entityframe5_state_t *s, *s2;
	entityframe5_packetlog_t *p, *p2;
	if (framenum >= d->ackedframenum)
		return;
	d->ackedframenum = framenum;
	// scan for packets made obsolete by this ack
	for (i = 0, p = d->packetlog;i < ENTITYFRAME5_MAXPACKETLOGS;i++, p++)
	{
		// skip packets that are empty or in the future
		if (p->packetnumber == 0 || p->packetnumber > framenum)
			continue;
		// if the packetnumber matches it is deleted without any processing 
		// (since it was received).
		// if the packet number is less than this ack it was lost and its
		// important information will be repeated in this update if it is not
		// already obsolete due to a later update.
		if (p->packetnumber < framenum)
		{
			// packet was lost - merge dirtybits into the main array so they
			// will be re-sent, but only if there is no newer update of that
			// bit in the logs (as those will arrive before this update)
			for (j = 0, s = p->states;j < p->numstates;j++, s++)
			{
				activedirtybit = s->activedirtybit;
				dirtybits = s->dirtybits;
				// check for any newer updates to this entity
				for (k = 0, p2 = d->packetlog;k < ENTITYFRAME5_MAXPACKETLOGS;k++, p2++)
				{
					if (p2->packetnumber > framenum)
					{
						for (l = 0, s2 = p2->states;l < p2->numstates;l++, p2++)
						{
							if (s2->entitynumber == s->entitynumber)
							{
								activedirtybit &= ~s2->activedirtybit;
								dirtybits &= ~s2->dirtybits;
								break;
							}
						}
						if (!activedirtybit && !dirtybits)
							break;
					}
				}
				// if the bits haven't all been cleared, there were some bits
				// lost with this packet, so set them again now
				if (activedirtybit)
					d->activedirtybits[s->entitynumber / 8] |= 1 << (s->entitynumber & 7);
				if (dirtybits)
					d->dirtybits[s->entitynumber] |= dirtybits;
			}
		}
		// delete this packet log as it is now obsolete
		p->packetnumber = 0;
	}
}

void EntityFrame5_WriteFrame(sizebuf_t *msg, int numstates, entity_state_t *states)
{
}
*/

