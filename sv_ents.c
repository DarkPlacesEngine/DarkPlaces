#include "quakedef.h"
#include "protocol.h"

int EntityState_DeltaBits(const entity_state_t *o, const entity_state_t *n)
{
	unsigned int bits;
	// if o is not active, delta from default
	if (o->active != ACTIVE_NETWORK)
		o = &defaultstate;
	bits = 0;
	if (fabs(n->origin[0] - o->origin[0]) > (1.0f / 256.0f))
		bits |= E_ORIGIN1;
	if (fabs(n->origin[1] - o->origin[1]) > (1.0f / 256.0f))
		bits |= E_ORIGIN2;
	if (fabs(n->origin[2] - o->origin[2]) > (1.0f / 256.0f))
		bits |= E_ORIGIN3;
	if ((unsigned char) (n->angles[0] * (256.0f / 360.0f)) != (unsigned char) (o->angles[0] * (256.0f / 360.0f)))
		bits |= E_ANGLE1;
	if ((unsigned char) (n->angles[1] * (256.0f / 360.0f)) != (unsigned char) (o->angles[1] * (256.0f / 360.0f)))
		bits |= E_ANGLE2;
	if ((unsigned char) (n->angles[2] * (256.0f / 360.0f)) != (unsigned char) (o->angles[2] * (256.0f / 360.0f)))
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

void EntityState_WriteFields(const entity_state_t *ent, sizebuf_t *msg, unsigned int bits)
{
	if (sv.protocol == PROTOCOL_DARKPLACES2)
	{
		if (bits & E_ORIGIN1)
			MSG_WriteCoord16i(msg, ent->origin[0]);
		if (bits & E_ORIGIN2)
			MSG_WriteCoord16i(msg, ent->origin[1]);
		if (bits & E_ORIGIN3)
			MSG_WriteCoord16i(msg, ent->origin[2]);
	}
	else
	{
		// LadyHavoc: have to write flags first, as they can modify protocol
		if (bits & E_FLAGS)
			MSG_WriteByte(msg, ent->flags);
		if (ent->flags & RENDER_LOWPRECISION)
		{
			if (bits & E_ORIGIN1)
				MSG_WriteCoord16i(msg, ent->origin[0]);
			if (bits & E_ORIGIN2)
				MSG_WriteCoord16i(msg, ent->origin[1]);
			if (bits & E_ORIGIN3)
				MSG_WriteCoord16i(msg, ent->origin[2]);
		}
		else
		{
			if (bits & E_ORIGIN1)
				MSG_WriteCoord32f(msg, ent->origin[0]);
			if (bits & E_ORIGIN2)
				MSG_WriteCoord32f(msg, ent->origin[1]);
			if (bits & E_ORIGIN3)
				MSG_WriteCoord32f(msg, ent->origin[2]);
		}
	}
	if ((sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4) && (ent->flags & RENDER_LOWPRECISION))
	{
		if (bits & E_ANGLE1)
			MSG_WriteAngle8i(msg, ent->angles[0]);
		if (bits & E_ANGLE2)
			MSG_WriteAngle8i(msg, ent->angles[1]);
		if (bits & E_ANGLE3)
			MSG_WriteAngle8i(msg, ent->angles[2]);
	}
	else
	{
		if (bits & E_ANGLE1)
			MSG_WriteAngle16i(msg, ent->angles[0]);
		if (bits & E_ANGLE2)
			MSG_WriteAngle16i(msg, ent->angles[1]);
		if (bits & E_ANGLE3)
			MSG_WriteAngle16i(msg, ent->angles[2]);
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
	if (sv.protocol == PROTOCOL_DARKPLACES2)
		if (bits & E_FLAGS)
			MSG_WriteByte(msg, ent->flags);
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

void EntityState_WriteUpdate(const entity_state_t *ent, sizebuf_t *msg, const entity_state_t *delta)
{
	prvm_prog_t *prog = SVVM_prog;
	unsigned int bits;
	if (ent->active == ACTIVE_NETWORK)
	{
		// entity is active, check for changes from the delta
		if ((bits = EntityState_DeltaBits(delta, ent)))
		{
			// write the update number, bits, and fields
			ENTITYSIZEPROFILING_START(msg, ent->number, bits);
			MSG_WriteShort(msg, ent->number);
			EntityState_WriteExtendBits(msg, bits);
			EntityState_WriteFields(ent, msg, bits);
			ENTITYSIZEPROFILING_END(msg, ent->number, bits);
		}
	}
	else
	{
		// entity is inactive, check if the delta was active
		if (delta->active == ACTIVE_NETWORK)
		{
			// write the remove number
			ENTITYSIZEPROFILING_START(msg, ent->number, 0);
			MSG_WriteShort(msg, ent->number | 0x8000);
			ENTITYSIZEPROFILING_END(msg, ent->number, 0);
		}
	}
}

// (server) adds a entity_frame to the database, for future reference
void EntityFrame_AddFrame_Server(entityframe_database_t *d, vec3_t eye, int framenum, int numentities, const entity_state_t **entitydata)
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

// (server) writes a frame to network stream
qboolean EntityFrame_WriteFrame(sizebuf_t *msg, int maxsize, entityframe_database_t *d, int numstates, const entity_state_t **states, int viewentnum)
{
	prvm_prog_t *prog = SVVM_prog;
	int i, onum, number;
	entity_frame_t *o = &d->deltaframe;
	const entity_state_t *ent, *delta;
	vec3_t eye;

	d->latestframenum++;

	VectorClear(eye);
	for (i = 0;i < numstates;i++)
	{
		ent = states[i];
		if (ent->number == viewentnum)
		{
			VectorSet(eye, ent->origin[0], ent->origin[1], ent->origin[2] + 22);
			break;
		}
	}

	EntityFrame_AddFrame_Server(d, eye, d->latestframenum, numstates, states);

	EntityFrame_FetchFrame(d, d->ackframenum, o);

	MSG_WriteByte (msg, svc_entities);
	MSG_WriteLong (msg, o->framenum);
	MSG_WriteLong (msg, d->latestframenum);
	MSG_WriteFloat (msg, eye[0]);
	MSG_WriteFloat (msg, eye[1]);
	MSG_WriteFloat (msg, eye[2]);

	onum = 0;
	for (i = 0;i < numstates;i++)
	{
		ent = states[i];
		number = ent->number;

		if (PRVM_serveredictfunction((&prog->edicts[number]), SendEntity))
			continue;
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

	return true;
}
