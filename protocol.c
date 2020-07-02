#include "quakedef.h"

// this is 88 bytes (must match entity_state_t in protocol.h)
entity_state_t defaultstate =
{
	// ! means this is not sent to client
	0,//double time; // ! time this state was built (used on client for interpolation)
	{0,0,0},//float netcenter[3]; // ! for network prioritization, this is the center of the bounding box (which may differ from the origin)
	{0,0,0},//float origin[3];
	{0,0,0},//float angles[3];
	0,//int effects;
	0,//unsigned int customizeentityforclient; // !
	0,//unsigned short number; // entity number this state is for
	0,//unsigned short modelindex;
	0,//unsigned short frame;
	0,//unsigned short tagentity;
	0,//unsigned short specialvisibilityradius; // ! larger if it has effects/light
	0,//unsigned short viewmodelforclient; // !
	0,//unsigned short exteriormodelforclient; // ! not shown if first person viewing from this entity, shown in all other cases
	0,//unsigned short nodrawtoclient; // !
	0,//unsigned short drawonlytoclient; // !
	0,//unsigned short traileffectnum;
	{0,0,0,0},//unsigned short light[4]; // color*256 (0.00 to 255.996), and radius*1
	ACTIVE_NOT,//unsigned char active; // true if a valid state
	0,//unsigned char lightstyle;
	0,//unsigned char lightpflags;
	0,//unsigned char colormap;
	0,//unsigned char skin; // also chooses cubemap for rtlights if lightpflags & LIGHTPFLAGS_FULLDYNAMIC
	255,//unsigned char alpha;
	16,//unsigned char scale;
	0,//unsigned char glowsize;
	254,//unsigned char glowcolor;
	0,//unsigned char flags;
	0,//unsigned char internaleffects; // INTEF_FLAG1QW and so on
	0,//unsigned char tagindex;
	{32, 32, 32},//unsigned char colormod[3];
	{32, 32, 32},//unsigned char glowmod[3];
};

// LadyHavoc: I own protocol ranges 96, 97, 3500-3599

struct protocolversioninfo_s
{
	int number;
	protocolversion_t version;
	const char *name;
}
protocolversioninfo[] =
{
	{ 3504, PROTOCOL_DARKPLACES7 , "DP7"},
	{ 3503, PROTOCOL_DARKPLACES6 , "DP6"},
	{ 3502, PROTOCOL_DARKPLACES5 , "DP5"},
	{ 3501, PROTOCOL_DARKPLACES4 , "DP4"},
	{ 3500, PROTOCOL_DARKPLACES3 , "DP3"},
	{   97, PROTOCOL_DARKPLACES2 , "DP2"},
	{   96, PROTOCOL_DARKPLACES1 , "DP1"},
	{   15, PROTOCOL_QUAKEDP     , "QUAKEDP"},
	{   15, PROTOCOL_QUAKE       , "QUAKE"},
	{   28, PROTOCOL_QUAKEWORLD  , "QW"},
	{  250, PROTOCOL_NEHAHRAMOVIE, "NEHAHRAMOVIE"},
	{10000, PROTOCOL_NEHAHRABJP  , "NEHAHRABJP"},
	{10001, PROTOCOL_NEHAHRABJP2 , "NEHAHRABJP2"},
	{10002, PROTOCOL_NEHAHRABJP3 , "NEHAHRABJP3"},
	{    0, PROTOCOL_UNKNOWN     , NULL}
};

protocolversion_t Protocol_EnumForName(const char *s)
{
	int i;
	for (i = 0;protocolversioninfo[i].name;i++)
		if (!strcasecmp(s, protocolversioninfo[i].name))
			return protocolversioninfo[i].version;
	return PROTOCOL_UNKNOWN;
}

const char *Protocol_NameForEnum(protocolversion_t p)
{
	int i;
	for (i = 0;protocolversioninfo[i].name;i++)
		if (protocolversioninfo[i].version == p)
			return protocolversioninfo[i].name;
	return "UNKNOWN";
}

protocolversion_t Protocol_EnumForNumber(int n)
{
	int i;
	for (i = 0;protocolversioninfo[i].name;i++)
		if (protocolversioninfo[i].number == n)
			return protocolversioninfo[i].version;
	return PROTOCOL_UNKNOWN;
}

int Protocol_NumberForEnum(protocolversion_t p)
{
	int i;
	for (i = 0;protocolversioninfo[i].name;i++)
		if (protocolversioninfo[i].version == p)
			return protocolversioninfo[i].number;
	return 0;
}

void Protocol_Names(char *buffer, size_t buffersize)
{
	int i;
	if (buffersize < 1)
		return;
	buffer[0] = 0;
	for (i = 0;protocolversioninfo[i].name;i++)
	{
		if (i > 1)
			strlcat(buffer, " ", buffersize);
		strlcat(buffer, protocolversioninfo[i].name, buffersize);
	}
}

void Protocol_UpdateClientStats(const int *stats)
{
	int i;
	// update the stats array and set deltabits for any changed stats
	for (i = 0;i < MAX_CL_STATS;i++)
	{
		if (host_client->stats[i] != stats[i])
		{
			host_client->statsdeltabits[i >> 3] |= 1 << (i & 7);
			host_client->stats[i] = stats[i];
		}
	}
}

// only a few stats are within the 32 stat limit of Quake, and most of them
// are sent every frame in svc_clientdata messages, so we only send the
// remaining ones here
static const int sendquakestats[] =
{
// quake did not send these secrets/monsters stats in this way, but doing so
// allows a mod to increase STAT_TOTALMONSTERS during the game, and ensures
// that STAT_SECRETS and STAT_MONSTERS are always correct (even if a client
// didn't receive an svc_foundsecret or svc_killedmonster), which may be most
// valuable if randomly seeking around in a demo
STAT_TOTALSECRETS, // never changes during game
STAT_TOTALMONSTERS, // changes in some mods
STAT_SECRETS, // this makes svc_foundsecret unnecessary
STAT_MONSTERS, // this makes svc_killedmonster unnecessary
STAT_VIEWHEIGHT, // sent just for FTEQW clients
STAT_VIEWZOOM, // this rarely changes
-1,
};

void Protocol_WriteStatsReliable(void)
{
	int i, j;
	if (!host_client->netconnection)
		return;
	// detect changes in stats and write reliable messages
	// this only deals with 32 stats because the older protocols which use
	// this function can only cope with 32 stats,
	// they also do not support svc_updatestatubyte which was introduced in
	// DP6 protocol (except for QW)
	for (j = 0;sendquakestats[j] >= 0;j++)
	{
		i = sendquakestats[j];
		// check if this bit is set
		if (host_client->statsdeltabits[i >> 3] & (1 << (i & 7)))
		{
			host_client->statsdeltabits[i >> 3] -= (1 << (i & 7));
			// send the stat as a byte if possible
			if (sv.protocol == PROTOCOL_QUAKEWORLD)
			{
				if (host_client->stats[i] >= 0 && host_client->stats[i] < 256)
				{
					MSG_WriteByte(&host_client->netconnection->message, qw_svc_updatestat);
					MSG_WriteByte(&host_client->netconnection->message, i);
					MSG_WriteByte(&host_client->netconnection->message, host_client->stats[i]);
				}
				else
				{
					MSG_WriteByte(&host_client->netconnection->message, qw_svc_updatestatlong);
					MSG_WriteByte(&host_client->netconnection->message, i);
					MSG_WriteLong(&host_client->netconnection->message, host_client->stats[i]);
				}
			}
			else
			{
				// this could make use of svc_updatestatubyte in DP6 and later
				// protocols but those protocols do not use this function
				MSG_WriteByte(&host_client->netconnection->message, svc_updatestat);
				MSG_WriteByte(&host_client->netconnection->message, i);
				MSG_WriteLong(&host_client->netconnection->message, host_client->stats[i]);
			}
		}
	}
}

entity_state_t *EntityFrame4_GetReferenceEntity(entityframe4_database_t *d, int number)
{
	if (d->maxreferenceentities <= number)
	{
		int oldmax = d->maxreferenceentities;
		entity_state_t *oldentity = d->referenceentity;
		d->maxreferenceentities = (number + 15) & ~7;
		d->referenceentity = (entity_state_t *)Mem_Alloc(d->mempool, d->maxreferenceentities * sizeof(*d->referenceentity));
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

void EntityFrame4_AddCommitEntity(entityframe4_database_t *d, const entity_state_t *s)
{
	// resize commit's entity list if full
	if (d->currentcommit->maxentities <= d->currentcommit->numentities)
	{
		entity_state_t *oldentity = d->currentcommit->entity;
		d->currentcommit->maxentities += 8;
		d->currentcommit->entity = (entity_state_t *)Mem_Alloc(d->mempool, d->currentcommit->maxentities * sizeof(*d->currentcommit->entity));
		if (oldentity)
		{
			memcpy(d->currentcommit->entity, oldentity, d->currentcommit->numentities * sizeof(*d->currentcommit->entity));
			Mem_Free(oldentity);
		}
	}
	d->currentcommit->entity[d->currentcommit->numentities++] = *s;
}

entityframe4_database_t *EntityFrame4_AllocDatabase(mempool_t *pool)
{
	entityframe4_database_t *d;
	d = (entityframe4_database_t *)Mem_Alloc(pool, sizeof(*d));
	d->mempool = pool;
	EntityFrame4_ResetDatabase(d);
	return d;
}

void EntityFrame4_FreeDatabase(entityframe4_database_t *d)
{
	int i;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		if (d->commit[i].entity)
			Mem_Free(d->commit[i].entity);
	if (d->referenceentity)
		Mem_Free(d->referenceentity);
	Mem_Free(d);
}

void EntityFrame4_ResetDatabase(entityframe4_database_t *d)
{
	int i;
	d->referenceframenum = -1;
	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		d->commit[i].numentities = 0;
	for (i = 0;i < d->maxreferenceentities;i++)
		d->referenceentity[i] = defaultstate;
}

int EntityFrame4_AckFrame(entityframe4_database_t *d, int framenum, int servermode)
{
	int i, j, found;
	entity_database4_commit_t *commit;
	if (framenum == -1)
	{
		// reset reference, but leave commits alone
		d->referenceframenum = -1;
		for (i = 0;i < d->maxreferenceentities;i++)
		{
			d->referenceentity[i] = defaultstate;
		// if this is the server, remove commits
			for (i = 0, commit = d->commit;i < MAX_ENTITY_HISTORY;i++, commit++)
				commit->numentities = 0;
		}
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
								if (commit->entity[j].active == ACTIVE_NETWORK)
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

void EntityFrame4_CL_ReadFrame(void)
{
	int i, n, cnumber, referenceframenum, framenum, enumber, done, stopnumber, skip = false;
	entity_state_t *s;
	entityframe4_database_t *d;
	if (!cl.entitydatabase4)
		cl.entitydatabase4 = EntityFrame4_AllocDatabase(cls.levelmempool);
	d = cl.entitydatabase4;
	// read the number of the frame this refers to
	referenceframenum = MSG_ReadLong(&cl_message);
	// read the number of this frame
	framenum = MSG_ReadLong(&cl_message);
	CL_NewFrameReceived(framenum);
	// read the start number
	enumber = (unsigned short) MSG_ReadShort(&cl_message);
	if (developer_networkentities.integer >= 10)
	{
		Con_Printf("recv svc_entities num:%i ref:%i database: ref:%i commits:", framenum, referenceframenum, d->referenceframenum);
		for (i = 0;i < MAX_ENTITY_HISTORY;i++)
			if (d->commit[i].numentities)
				Con_Printf(" %i", d->commit[i].framenum);
		Con_Print("\n");
	}
	if (!EntityFrame4_AckFrame(d, referenceframenum, false))
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
			d->currentcommit->framenum = framenum;
			d->currentcommit->numentities = 0;
		}
	}
	if (d->currentcommit == NULL)
	{
		Con_Printf("EntityFrame4_CL_ReadFrame: error while decoding frame %i: database full, reading but not storing this update\n", framenum);
		skip = true;
	}
	done = false;
	while (!done && !cl_message.badread)
	{
		// read the number of the modified entity
		// (gaps will be copied unmodified)
		n = (unsigned short)MSG_ReadShort(&cl_message);
		if (n == 0x8000)
		{
			// no more entities in this update, but we still need to copy the
			// rest of the reference entities (final gap)
			done = true;
			// read end of range number, then process normally
			n = (unsigned short)MSG_ReadShort(&cl_message);
		}
		// high bit means it's a remove message
		cnumber = n & 0x7FFF;
		// if this is a live entity we may need to expand the array
		if (cl.num_entities <= cnumber && !(n & 0x8000))
		{
			cl.num_entities = cnumber + 1;
			if (cnumber >= cl.max_entities)
				CL_ExpandEntities(cnumber);
		}
		// add one (the changed one) if not done
		stopnumber = cnumber + !done;
		// process entities in range from the last one to the changed one
		for (;enumber < stopnumber;enumber++)
		{
			if (skip || enumber >= cl.num_entities)
			{
				if (enumber == cnumber && (n & 0x8000) == 0)
				{
					entity_state_t tempstate;
					EntityState_ReadFields(&tempstate, EntityState_ReadExtendBits());
				}
				continue;
			}
			// slide the current into the previous slot
			cl.entities[enumber].state_previous = cl.entities[enumber].state_current;
			// copy a new current from reference database
			cl.entities[enumber].state_current = *EntityFrame4_GetReferenceEntity(d, enumber);
			s = &cl.entities[enumber].state_current;
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
					s->active = ACTIVE_NETWORK;
					EntityState_ReadFields(s, EntityState_ReadExtendBits());
				}
			}
			else if (developer_networkentities.integer >= 4)
				Con_Printf("entity %i: copy\n", enumber);
			// set the cl.entities_active flag
			cl.entities_active[enumber] = (s->active == ACTIVE_NETWORK);
			// set the update time
			s->time = cl.mtime[0];
			// fix the number (it gets wiped occasionally by copying from defaultstate)
			s->number = enumber;
			// check if we need to update the lerp stuff
			if (s->active == ACTIVE_NETWORK)
				CL_MoveLerpEntityStates(&cl.entities[enumber]);
			// add this to the commit entry whether it is modified or not
			if (d->currentcommit)
				EntityFrame4_AddCommitEntity(d, &cl.entities[enumber].state_current);
			// print extra messages if desired
			if (developer_networkentities.integer >= 2 && cl.entities[enumber].state_current.active != cl.entities[enumber].state_previous.active)
			{
				if (cl.entities[enumber].state_current.active == ACTIVE_NETWORK)
					Con_Printf("entity #%i has become active\n", enumber);
				else if (cl.entities[enumber].state_previous.active)
					Con_Printf("entity #%i has become inactive\n", enumber);
			}
		}
	}
	d->currentcommit = NULL;
	if (skip)
		EntityFrame4_ResetDatabase(d);
}

qboolean EntityFrame4_WriteFrame(sizebuf_t *msg, int maxsize, entityframe4_database_t *d, int numstates, const entity_state_t **states)
{
	prvm_prog_t *prog = SVVM_prog;
	const entity_state_t *e, *s;
	entity_state_t inactiveentitystate;
	int i, n, startnumber;
	sizebuf_t buf;
	unsigned char data[128];

	// if there isn't enough space to accomplish anything, skip it
	if (msg->cursize + 24 > maxsize)
		return false;

	// prepare the buffer
	memset(&buf, 0, sizeof(buf));
	buf.data = data;
	buf.maxsize = sizeof(data);

	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		if (!d->commit[i].numentities)
			break;
	// if commit buffer full, just don't bother writing an update this frame
	if (i == MAX_ENTITY_HISTORY)
		return false;
	d->currentcommit = d->commit + i;

	// this state's number gets played around with later
	inactiveentitystate = defaultstate;

	d->currentcommit->numentities = 0;
	d->currentcommit->framenum = ++d->latestframenumber;
	MSG_WriteByte(msg, svc_entities);
	MSG_WriteLong(msg, d->referenceframenum);
	MSG_WriteLong(msg, d->currentcommit->framenum);
	if (developer_networkentities.integer >= 10)
	{
		Con_Printf("send svc_entities num:%i ref:%i (database: ref:%i commits:", d->currentcommit->framenum, d->referenceframenum, d->referenceframenum);
		for (i = 0;i < MAX_ENTITY_HISTORY;i++)
			if (d->commit[i].numentities)
				Con_Printf(" %i", d->commit[i].framenum);
		Con_Print(")\n");
	}
	if (d->currententitynumber >= prog->max_edicts)
		startnumber = 1;
	else
		startnumber = bound(1, d->currententitynumber, prog->max_edicts - 1);
	MSG_WriteShort(msg, startnumber);
	// reset currententitynumber so if the loop does not break it we will
	// start at beginning next frame (if it does break, it will set it)
	d->currententitynumber = 1;
	for (i = 0, n = startnumber;n < prog->max_edicts;n++)
	{
		if (PRVM_serveredictfunction((&prog->edicts[n]), SendEntity))
			continue;
		// find the old state to delta from
		e = EntityFrame4_GetReferenceEntity(d, n);
		// prepare the buffer
		SZ_Clear(&buf);
		// entity exists, build an update (if empty there is no change)
		// find the state in the list
		for (;i < numstates && states[i]->number < n;i++);
		// make the message
		s = states[i];
		if (s->number == n)
		{
			// build the update
			EntityState_WriteUpdate(s, &buf, e);
		}
		else
		{
			inactiveentitystate.number = n;
			s = &inactiveentitystate;
			if (e->active == ACTIVE_NETWORK)
			{
				// entity used to exist but doesn't anymore, send remove
				MSG_WriteShort(&buf, n | 0x8000);
			}
		}
		// if the commit is full, we're done this frame
		if (msg->cursize + buf.cursize > maxsize - 4)
		{
			// next frame we will continue where we left off
			break;
		}
		// add the entity to the commit
		EntityFrame4_AddCommitEntity(d, s);
		// if the message is empty, skip out now
		if (buf.cursize)
		{
			// write the message to the packet
			SZ_Write(msg, buf.data, buf.cursize);
		}
	}
	d->currententitynumber = n;

	// remove world message (invalid, and thus a good terminator)
	MSG_WriteShort(msg, 0x8000);
	// write the number of the end entity
	MSG_WriteShort(msg, d->currententitynumber);
	// just to be sure
	d->currentcommit = NULL;

	return true;
}
