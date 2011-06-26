#include "quakedef.h"

#define ENTITYSIZEPROFILING_START(msg, num) \
	int entityprofiling_startsize = msg->cursize

#define ENTITYSIZEPROFILING_END(msg, num) \
	if(developer_networkentities.integer >= 2) \
	{ \
		prvm_edict_t *ed = prog->edicts + num; \
		Con_Printf("sent entity update of size %d for a %s\n", (msg->cursize - entityprofiling_startsize), PRVM_serveredictstring(ed, classname) ? PRVM_GetString(PRVM_serveredictstring(ed, classname)) : "(no classname)"); \
	}

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

// LordHavoc: I own protocol ranges 96, 97, 3500-3599

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

void EntityFrameQuake_ReadEntity(int bits)
{
	int num;
	entity_t *ent;
	entity_state_t s;

	if (bits & U_MOREBITS)
		bits |= (MSG_ReadByte()<<8);
	if ((bits & U_EXTEND1) && cls.protocol != PROTOCOL_NEHAHRAMOVIE)
	{
		bits |= MSG_ReadByte() << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte() << 24;
	}

	if (bits & U_LONGENTITY)
		num = (unsigned short) MSG_ReadShort ();
	else
		num = MSG_ReadByte ();

	if (num >= MAX_EDICTS)
		Host_Error("EntityFrameQuake_ReadEntity: entity number (%i) >= MAX_EDICTS (%i)", num, MAX_EDICTS);
	if (num < 1)
		Host_Error("EntityFrameQuake_ReadEntity: invalid entity number (%i)", num);

	if (cl.num_entities <= num)
	{
		cl.num_entities = num + 1;
		if (num >= cl.max_entities)
			CL_ExpandEntities(num);
	}

	ent = cl.entities + num;

	// note: this inherits the 'active' state of the baseline chosen
	// (state_baseline is always active, state_current may not be active if
	// the entity was missing in the last frame)
	if (bits & U_DELTA)
		s = ent->state_current;
	else
	{
		s = ent->state_baseline;
		s.active = ACTIVE_NETWORK;
	}

	cl.isquakeentity[num] = true;
	if (cl.lastquakeentity < num)
		cl.lastquakeentity = num;
	s.number = num;
	s.time = cl.mtime[0];
	s.flags = 0;
	if (bits & U_MODEL)
	{
		if (cls.protocol == PROTOCOL_NEHAHRABJP || cls.protocol == PROTOCOL_NEHAHRABJP2 || cls.protocol == PROTOCOL_NEHAHRABJP3)
							s.modelindex = (unsigned short) MSG_ReadShort();
		else
							s.modelindex = (s.modelindex & 0xFF00) | MSG_ReadByte();
	}
	if (bits & U_FRAME)		s.frame = (s.frame & 0xFF00) | MSG_ReadByte();
	if (bits & U_COLORMAP)	s.colormap = MSG_ReadByte();
	if (bits & U_SKIN)		s.skin = MSG_ReadByte();
	if (bits & U_EFFECTS)	s.effects = (s.effects & 0xFF00) | MSG_ReadByte();
	if (bits & U_ORIGIN1)	s.origin[0] = MSG_ReadCoord(cls.protocol);
	if (bits & U_ANGLE1)	s.angles[0] = MSG_ReadAngle(cls.protocol);
	if (bits & U_ORIGIN2)	s.origin[1] = MSG_ReadCoord(cls.protocol);
	if (bits & U_ANGLE2)	s.angles[1] = MSG_ReadAngle(cls.protocol);
	if (bits & U_ORIGIN3)	s.origin[2] = MSG_ReadCoord(cls.protocol);
	if (bits & U_ANGLE3)	s.angles[2] = MSG_ReadAngle(cls.protocol);
	if (bits & U_STEP)		s.flags |= RENDER_STEP;
	if (bits & U_ALPHA)		s.alpha = MSG_ReadByte();
	if (bits & U_SCALE)		s.scale = MSG_ReadByte();
	if (bits & U_EFFECTS2)	s.effects = (s.effects & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_GLOWSIZE)	s.glowsize = MSG_ReadByte();
	if (bits & U_GLOWCOLOR)	s.glowcolor = MSG_ReadByte();
	if (bits & U_COLORMOD)	{int c = MSG_ReadByte();s.colormod[0] = (unsigned char)(((c >> 5) & 7) * (32.0f / 7.0f));s.colormod[1] = (unsigned char)(((c >> 2) & 7) * (32.0f / 7.0f));s.colormod[2] = (unsigned char)((c & 3) * (32.0f / 3.0f));}
	if (bits & U_GLOWTRAIL) s.flags |= RENDER_GLOWTRAIL;
	if (bits & U_FRAME2)	s.frame = (s.frame & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_MODEL2)	s.modelindex = (s.modelindex & 0x00FF) | (MSG_ReadByte() << 8);
	if (bits & U_VIEWMODEL)	s.flags |= RENDER_VIEWMODEL;
	if (bits & U_EXTERIORMODEL)	s.flags |= RENDER_EXTERIORMODEL;

	// LordHavoc: to allow playback of the Nehahra movie
	if (cls.protocol == PROTOCOL_NEHAHRAMOVIE && (bits & U_EXTEND1))
	{
		// LordHavoc: evil format
		int i = (int)MSG_ReadFloat();
		int j = (int)(MSG_ReadFloat() * 255.0f);
		if (i == 2)
		{
			i = (int)MSG_ReadFloat();
			if (i)
				s.effects |= EF_FULLBRIGHT;
		}
		if (j < 0)
			s.alpha = 0;
		else if (j == 0 || j >= 255)
			s.alpha = 255;
		else
			s.alpha = j;
	}

	ent->state_previous = ent->state_current;
	ent->state_current = s;
	if (ent->state_current.active == ACTIVE_NETWORK)
	{
		CL_MoveLerpEntityStates(ent);
		cl.entities_active[ent->state_current.number] = true;
	}

	if (msg_badread)
		Host_Error("EntityFrameQuake_ReadEntity: read error");
}

void EntityFrameQuake_ISeeDeadEntities(void)
{
	int num, lastentity;
	if (cl.lastquakeentity == 0)
		return;
	lastentity = cl.lastquakeentity;
	cl.lastquakeentity = 0;
	for (num = 0;num <= lastentity;num++)
	{
		if (cl.isquakeentity[num])
		{
			if (cl.entities_active[num] && cl.entities[num].state_current.time == cl.mtime[0])
			{
				cl.isquakeentity[num] = true;
				cl.lastquakeentity = num;
			}
			else
			{
				cl.isquakeentity[num] = false;
				cl.entities_active[num] = ACTIVE_NOT;
				cl.entities[num].state_current = defaultstate;
				cl.entities[num].state_current.number = num;
			}
		}
	}
}

// NOTE: this only works with DP5 protocol and upwards. For lower protocols
// (including QUAKE), no packet loss handling for CSQC is done, which makes
// CSQC basically useless.
// Always use the DP5 protocol, or a higher one, when using CSQC entities.
static void EntityFrameCSQC_LostAllFrames(client_t *client)
{
	// mark ALL csqc entities as requiring a FULL resend!
	// I know this is a bad workaround, but better than nothing.
	int i, n;
	prvm_edict_t *ed;

	n = client->csqcnumedicts;
	for(i = 0; i < n; ++i)
	{
		if(client->csqcentityglobalhistory[i])
		{
			ed = prog->edicts + i;
			if (PRVM_serveredictfunction(ed, SendEntity))
				client->csqcentitysendflags[i] |= 0xFFFFFF; // FULL RESEND
			else // if it was ever sent to that client as a CSQC entity
			{
				client->csqcentityscope[i] = 1; // REMOVE
				client->csqcentitysendflags[i] |= 0xFFFFFF;
			}
		}
	}
}
void EntityFrameCSQC_LostFrame(client_t *client, int framenum)
{
	// marks a frame as lost
	int i, j;
	qboolean valid;
	int ringfirst, ringlast;
	static int recoversendflags[MAX_EDICTS];
	csqcentityframedb_t *d;

	if(client->csqcentityframe_lastreset < 0)
		return;
	if(framenum < client->csqcentityframe_lastreset)
		return; // no action required, as we resent that data anyway

	// is our frame out of history?
	ringfirst = client->csqcentityframehistory_next; // oldest entry
	ringlast = (ringfirst + NUM_CSQCENTITYDB_FRAMES - 1) % NUM_CSQCENTITYDB_FRAMES; // most recently added entry

	valid = false;
	
	for(j = 0; j < NUM_CSQCENTITYDB_FRAMES; ++j)
	{
		d = &client->csqcentityframehistory[(ringfirst + j) % NUM_CSQCENTITYDB_FRAMES];
		if(d->framenum < 0)
			continue;
		if(d->framenum == framenum)
			break;
		else if(d->framenum < framenum)
			valid = true;
	}
	if(j == NUM_CSQCENTITYDB_FRAMES)
	{
		if(valid) // got beaten, i.e. there is a frame < framenum
		{
			// a non-csqc frame got lost... great
			return;
		}
		else
		{
			// a too old frame got lost... sorry, cannot handle this
			Con_DPrintf("CSQC entity DB: lost a frame too early to do any handling (resending ALL)...\n");
			Con_DPrintf("Lost frame = %d\n", framenum);
			Con_DPrintf("Entity DB = %d to %d\n", client->csqcentityframehistory[ringfirst].framenum, client->csqcentityframehistory[ringlast].framenum);
			EntityFrameCSQC_LostAllFrames(client);
			client->csqcentityframe_lastreset = -1;
		}
		return;
	}

	// so j is the frame that got lost
	// ringlast is the frame that we have to go to
	ringfirst = (ringfirst + j) % NUM_CSQCENTITYDB_FRAMES;
	if(ringlast < ringfirst)
		ringlast += NUM_CSQCENTITYDB_FRAMES;
	
	memset(recoversendflags, 0, sizeof(recoversendflags));

	for(j = ringfirst; j <= ringlast; ++j)
	{
		d = &client->csqcentityframehistory[j % NUM_CSQCENTITYDB_FRAMES];
		if(d->framenum < 0)
		{
			// deleted frame
		}
		else if(d->framenum < framenum)
		{
			// a frame in the past... should never happen
			Con_Printf("CSQC entity DB encountered a frame from the past when recovering from PL...?\n");
		}
		else if(d->framenum == framenum)
		{
			// handling the actually lost frame now
			for(i = 0; i < d->num; ++i)
			{
				int sf = d->sendflags[i];
				int ent = d->entno[i];
				if(sf < 0) // remove
					recoversendflags[ent] |= -1; // all bits, including sign
				else if(sf > 0)
					recoversendflags[ent] |= sf;
			}
		}
		else
		{
			// handling the frames that followed it now
			for(i = 0; i < d->num; ++i)
			{
				int sf = d->sendflags[i];
				int ent = d->entno[i];
				if(sf < 0) // remove
				{
					recoversendflags[ent] = 0; // no need to update, we got a more recent remove (and will fix it THEN)
					break; // no flags left to remove...
				}
				else if(sf > 0)
					recoversendflags[ent] &= ~sf; // no need to update these bits, we already got them later
			}
		}
	}

	for(i = 0; i < client->csqcnumedicts; ++i)
	{
		if(recoversendflags[i] < 0)
		{
			// a remove got lost, then either send a remove or - if it was
			// recreated later - a FULL update to make totally sure
			client->csqcentityscope[i] = 1;
			client->csqcentitysendflags[i] = 0xFFFFFF;
		}
		else
			client->csqcentitysendflags[i] |= recoversendflags[i];
	}
}
static int EntityFrameCSQC_AllocFrame(client_t *client, int framenum)
{
	int ringfirst = client->csqcentityframehistory_next; // oldest entry
	client->csqcentityframehistory_next += 1;
	client->csqcentityframehistory_next %= NUM_CSQCENTITYDB_FRAMES;
	client->csqcentityframehistory[ringfirst].framenum = framenum;
	client->csqcentityframehistory[ringfirst].num = 0;
	return ringfirst;
}
static void EntityFrameCSQC_DeallocFrame(client_t *client, int framenum)
{
	int ringfirst = client->csqcentityframehistory_next; // oldest entry
	int ringlast = (ringfirst + NUM_CSQCENTITYDB_FRAMES - 1) % NUM_CSQCENTITYDB_FRAMES; // most recently added entry
	if(framenum == client->csqcentityframehistory[ringlast].framenum)
	{
		client->csqcentityframehistory[ringlast].framenum = -1;
		client->csqcentityframehistory[ringlast].num = 0;
		client->csqcentityframehistory_next = ringlast;
	}
	else
		Con_Printf("Trying to dealloc the wrong entity frame\n");
}

//[515]: we use only one array per-client for SendEntity feature
// TODO: add some handling for entity send priorities, to better deal with huge
// amounts of csqc networked entities
qboolean EntityFrameCSQC_WriteFrame (sizebuf_t *msg, int maxsize, int numnumbers, const unsigned short *numbers, int framenum)
{
	int num, number, end, sendflags;
	qboolean sectionstarted = false;
	const unsigned short *n;
	prvm_edict_t *ed;
	client_t *client = svs.clients + sv.writeentitiestoclient_clientnumber;
	int dbframe = EntityFrameCSQC_AllocFrame(client, framenum);
	csqcentityframedb_t *db = &client->csqcentityframehistory[dbframe];

	if(client->csqcentityframe_lastreset < 0)
		client->csqcentityframe_lastreset = framenum;

	maxsize -= 24; // always fit in an empty svc_entities message (for packet loss detection!)

	// make sure there is enough room to store the svc_csqcentities byte,
	// the terminator (0x0000) and at least one entity update
	if (msg->cursize + 32 >= maxsize)
		return false;

	if (client->csqcnumedicts < prog->num_edicts)
		client->csqcnumedicts = prog->num_edicts;

	number = 1;
	for (num = 0, n = numbers;num < numnumbers;num++, n++)
	{
		end = *n;
		for (;number < end;number++)
		{
			if (client->csqcentityscope[number])
			{
				client->csqcentityscope[number] = 1;
				client->csqcentitysendflags[number] = 0xFFFFFF;
			}
		}
		ed = prog->edicts + number;
		if (PRVM_serveredictfunction(ed, SendEntity))
			client->csqcentityscope[number] = 2;
		else if (client->csqcentityscope[number])
		{
			client->csqcentityscope[number] = 1;
			client->csqcentitysendflags[number] = 0xFFFFFF;
		}
		number++;
	}
	end = client->csqcnumedicts;
	for (;number < end;number++)
	{
		if (client->csqcentityscope[number])
		{
			client->csqcentityscope[number] = 1;
			client->csqcentitysendflags[number] = 0xFFFFFF;
		}
	}

	/*
	// mark all scope entities as remove
	for (number = 1;number < client->csqcnumedicts;number++)
		if (client->csqcentityscope[number])
			client->csqcentityscope[number] = 1;
	// keep visible entities
	for (i = 0, n = numbers;i < numnumbers;i++, n++)
	{
		number = *n;
		ed = prog->edicts + number;
		if (PRVM_serveredictfunction(ed, SendEntity))
			client->csqcentityscope[number] = 2;
	}
	*/

	// now try to emit the entity updates
	// (FIXME: prioritize by distance?)
	end = client->csqcnumedicts;
	for (number = 1;number < end;number++)
	{
		if (!client->csqcentityscope[number])
			continue;
		sendflags = client->csqcentitysendflags[number];
		if (!sendflags)
			continue;
		if(db->num >= NUM_CSQCENTITIES_PER_FRAME)
			break;
		ed = prog->edicts + number;
		// entity scope is either update (2) or remove (1)
		if (client->csqcentityscope[number] == 1)
		{
			// write a remove message
			// first write the message identifier if needed
			if(!sectionstarted)
			{
				sectionstarted = 1;
				MSG_WriteByte(msg, svc_csqcentities);
			}
			// write the remove message
			{
				ENTITYSIZEPROFILING_START(msg, number);
				MSG_WriteShort(msg, (unsigned short)number | 0x8000);
				client->csqcentityscope[number] = 0;
				client->csqcentitysendflags[number] = 0xFFFFFF; // resend completely if it becomes active again
				db->entno[db->num] = number;
				db->sendflags[db->num] = -1;
				db->num += 1;
				client->csqcentityglobalhistory[number] = 1;
				ENTITYSIZEPROFILING_END(msg, number);
			}
			if (msg->cursize + 17 >= maxsize)
				break;
		}
		else
		{
			// write an update
			// save the cursize value in case we overflow and have to rollback
			int oldcursize = msg->cursize;
			client->csqcentityscope[number] = 1;
			if (PRVM_serveredictfunction(ed, SendEntity))
			{
				if(!sectionstarted)
					MSG_WriteByte(msg, svc_csqcentities);
				{
					ENTITYSIZEPROFILING_START(msg, number);
					MSG_WriteShort(msg, number);
					msg->allowoverflow = true;
					PRVM_G_INT(OFS_PARM0) = sv.writeentitiestoclient_cliententitynumber;
					PRVM_G_FLOAT(OFS_PARM1) = sendflags;
					PRVM_serverglobaledict(self) = number;
					PRVM_ExecuteProgram(PRVM_serveredictfunction(ed, SendEntity), "Null SendEntity\n");
					msg->allowoverflow = false;
					if(PRVM_G_FLOAT(OFS_RETURN) && msg->cursize + 2 <= maxsize)
					{
						// an update has been successfully written
						client->csqcentitysendflags[number] = 0;
						db->entno[db->num] = number;
						db->sendflags[db->num] = sendflags;
						db->num += 1;
						client->csqcentityglobalhistory[number] = 1;
						// and take note that we have begun the svc_csqcentities
						// section of the packet
						sectionstarted = 1;
						ENTITYSIZEPROFILING_END(msg, number);
						if (msg->cursize + 17 >= maxsize)
							break;
						continue;
					}
				}
			}
			// self.SendEntity returned false (or does not exist) or the
			// update was too big for this packet - rollback the buffer to its
			// state before the writes occurred, we'll try again next frame
			msg->cursize = oldcursize;
			msg->overflowed = false;
		}
	}
	if (sectionstarted)
	{
		// write index 0 to end the update (0 is never used by real entities)
		MSG_WriteShort(msg, 0);
	}

	if(db->num == 0)
		// if no single ent got added, remove the frame from the DB again, to allow
		// for a larger history
		EntityFrameCSQC_DeallocFrame(client, framenum);
	
	return sectionstarted;
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


qboolean EntityFrameQuake_WriteFrame(sizebuf_t *msg, int maxsize, int numstates, const entity_state_t **states)
{
	const entity_state_t *s;
	entity_state_t baseline;
	int i, bits;
	sizebuf_t buf;
	unsigned char data[128];
	qboolean success = false;

	// prepare the buffer
	memset(&buf, 0, sizeof(buf));
	buf.data = data;
	buf.maxsize = sizeof(data);

	for (i = 0;i < numstates;i++)
	{
		ENTITYSIZEPROFILING_START(msg, states[i]->number);
		s = states[i];
		if(PRVM_serveredictfunction((&prog->edicts[s->number]), SendEntity))
			continue;

		// prepare the buffer
		SZ_Clear(&buf);

// send an update
		bits = 0;
		if (s->number >= 256)
			bits |= U_LONGENTITY;
		if (s->flags & RENDER_STEP)
			bits |= U_STEP;
		if (s->flags & RENDER_VIEWMODEL)
			bits |= U_VIEWMODEL;
		if (s->flags & RENDER_GLOWTRAIL)
			bits |= U_GLOWTRAIL;
		if (s->flags & RENDER_EXTERIORMODEL)
			bits |= U_EXTERIORMODEL;

		// LordHavoc: old stuff, but rewritten to have more exact tolerances
		baseline = prog->edicts[s->number].priv.server->baseline;
		if (baseline.origin[0] != s->origin[0])
			bits |= U_ORIGIN1;
		if (baseline.origin[1] != s->origin[1])
			bits |= U_ORIGIN2;
		if (baseline.origin[2] != s->origin[2])
			bits |= U_ORIGIN3;
		if (baseline.angles[0] != s->angles[0])
			bits |= U_ANGLE1;
		if (baseline.angles[1] != s->angles[1])
			bits |= U_ANGLE2;
		if (baseline.angles[2] != s->angles[2])
			bits |= U_ANGLE3;
		if (baseline.colormap != s->colormap)
			bits |= U_COLORMAP;
		if (baseline.skin != s->skin)
			bits |= U_SKIN;
		if (baseline.frame != s->frame)
		{
			bits |= U_FRAME;
			if (s->frame & 0xFF00)
				bits |= U_FRAME2;
		}
		if (baseline.effects != s->effects)
		{
			bits |= U_EFFECTS;
			if (s->effects & 0xFF00)
				bits |= U_EFFECTS2;
		}
		if (baseline.modelindex != s->modelindex)
		{
			bits |= U_MODEL;
			if ((s->modelindex & 0xFF00) && sv.protocol != PROTOCOL_NEHAHRABJP && sv.protocol != PROTOCOL_NEHAHRABJP2 && sv.protocol != PROTOCOL_NEHAHRABJP3)
				bits |= U_MODEL2;
		}
		if (baseline.alpha != s->alpha)
			bits |= U_ALPHA;
		if (baseline.scale != s->scale)
			bits |= U_SCALE;
		if (baseline.glowsize != s->glowsize)
			bits |= U_GLOWSIZE;
		if (baseline.glowcolor != s->glowcolor)
			bits |= U_GLOWCOLOR;
		if (!VectorCompare(baseline.colormod, s->colormod))
			bits |= U_COLORMOD;

		// if extensions are disabled, clear the relevant update flags
		if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_NEHAHRAMOVIE)
			bits &= 0x7FFF;
		if (sv.protocol == PROTOCOL_NEHAHRAMOVIE)
			if (s->alpha != 255 || s->effects & EF_FULLBRIGHT)
				bits |= U_EXTEND1;

		// write the message
		if (bits >= 16777216)
			bits |= U_EXTEND2;
		if (bits >= 65536)
			bits |= U_EXTEND1;
		if (bits >= 256)
			bits |= U_MOREBITS;
		bits |= U_SIGNAL;

		MSG_WriteByte (&buf, bits);
		if (bits & U_MOREBITS)		MSG_WriteByte(&buf, bits>>8);
		if (sv.protocol != PROTOCOL_NEHAHRAMOVIE)
		{
			if (bits & U_EXTEND1)	MSG_WriteByte(&buf, bits>>16);
			if (bits & U_EXTEND2)	MSG_WriteByte(&buf, bits>>24);
		}
		if (bits & U_LONGENTITY)	MSG_WriteShort(&buf, s->number);
		else						MSG_WriteByte(&buf, s->number);

		if (bits & U_MODEL)
		{
			if (sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
				MSG_WriteShort(&buf, s->modelindex);
			else
				MSG_WriteByte(&buf, s->modelindex);
		}
		if (bits & U_FRAME)			MSG_WriteByte(&buf, s->frame);
		if (bits & U_COLORMAP)		MSG_WriteByte(&buf, s->colormap);
		if (bits & U_SKIN)			MSG_WriteByte(&buf, s->skin);
		if (bits & U_EFFECTS)		MSG_WriteByte(&buf, s->effects);
		if (bits & U_ORIGIN1)		MSG_WriteCoord(&buf, s->origin[0], sv.protocol);
		if (bits & U_ANGLE1)		MSG_WriteAngle(&buf, s->angles[0], sv.protocol);
		if (bits & U_ORIGIN2)		MSG_WriteCoord(&buf, s->origin[1], sv.protocol);
		if (bits & U_ANGLE2)		MSG_WriteAngle(&buf, s->angles[1], sv.protocol);
		if (bits & U_ORIGIN3)		MSG_WriteCoord(&buf, s->origin[2], sv.protocol);
		if (bits & U_ANGLE3)		MSG_WriteAngle(&buf, s->angles[2], sv.protocol);
		if (bits & U_ALPHA)			MSG_WriteByte(&buf, s->alpha);
		if (bits & U_SCALE)			MSG_WriteByte(&buf, s->scale);
		if (bits & U_EFFECTS2)		MSG_WriteByte(&buf, s->effects >> 8);
		if (bits & U_GLOWSIZE)		MSG_WriteByte(&buf, s->glowsize);
		if (bits & U_GLOWCOLOR)		MSG_WriteByte(&buf, s->glowcolor);
		if (bits & U_COLORMOD)		{int c = ((int)bound(0, s->colormod[0] * (7.0f / 32.0f), 7) << 5) | ((int)bound(0, s->colormod[1] * (7.0f / 32.0f), 7) << 2) | ((int)bound(0, s->colormod[2] * (3.0f / 32.0f), 3) << 0);MSG_WriteByte(&buf, c);}
		if (bits & U_FRAME2)		MSG_WriteByte(&buf, s->frame >> 8);
		if (bits & U_MODEL2)		MSG_WriteByte(&buf, s->modelindex >> 8);

		// the nasty protocol
		if ((bits & U_EXTEND1) && sv.protocol == PROTOCOL_NEHAHRAMOVIE)
		{
			if (s->effects & EF_FULLBRIGHT)
			{
				MSG_WriteFloat(&buf, 2); // QSG protocol version
				MSG_WriteFloat(&buf, s->alpha <= 0 ? 0 : (s->alpha >= 255 ? 1 : s->alpha * (1.0f / 255.0f))); // alpha
				MSG_WriteFloat(&buf, 1); // fullbright
			}
			else
			{
				MSG_WriteFloat(&buf, 1); // QSG protocol version
				MSG_WriteFloat(&buf, s->alpha <= 0 ? 0 : (s->alpha >= 255 ? 1 : s->alpha * (1.0f / 255.0f))); // alpha
			}
		}

		// if the commit is full, we're done this frame
		if (msg->cursize + buf.cursize > maxsize)
		{
			// next frame we will continue where we left off
			break;
		}
		// write the message to the packet
		SZ_Write(msg, buf.data, buf.cursize);
		success = true;
		ENTITYSIZEPROFILING_END(msg, s->number);
	}
	return success;
}

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
		// LordHavoc: have to write flags first, as they can modify protocol
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
	unsigned int bits;
	ENTITYSIZEPROFILING_START(msg, ent->number);
	if (ent->active == ACTIVE_NETWORK)
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
		if (delta->active == ACTIVE_NETWORK)
		{
			// write the remove number
			MSG_WriteShort(msg, ent->number | 0x8000);
		}
	}
	ENTITYSIZEPROFILING_END(msg, ent->number);
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
	if (cls.protocol == PROTOCOL_DARKPLACES2)
	{
		if (bits & E_ORIGIN1)
			e->origin[0] = MSG_ReadCoord16i();
		if (bits & E_ORIGIN2)
			e->origin[1] = MSG_ReadCoord16i();
		if (bits & E_ORIGIN3)
			e->origin[2] = MSG_ReadCoord16i();
	}
	else
	{
		if (bits & E_FLAGS)
			e->flags = MSG_ReadByte();
		if (e->flags & RENDER_LOWPRECISION)
		{
			if (bits & E_ORIGIN1)
				e->origin[0] = MSG_ReadCoord16i();
			if (bits & E_ORIGIN2)
				e->origin[1] = MSG_ReadCoord16i();
			if (bits & E_ORIGIN3)
				e->origin[2] = MSG_ReadCoord16i();
		}
		else
		{
			if (bits & E_ORIGIN1)
				e->origin[0] = MSG_ReadCoord32f();
			if (bits & E_ORIGIN2)
				e->origin[1] = MSG_ReadCoord32f();
			if (bits & E_ORIGIN3)
				e->origin[2] = MSG_ReadCoord32f();
		}
	}
	if ((cls.protocol == PROTOCOL_DARKPLACES5 || cls.protocol == PROTOCOL_DARKPLACES6) && !(e->flags & RENDER_LOWPRECISION))
	{
		if (bits & E_ANGLE1)
			e->angles[0] = MSG_ReadAngle16i();
		if (bits & E_ANGLE2)
			e->angles[1] = MSG_ReadAngle16i();
		if (bits & E_ANGLE3)
			e->angles[2] = MSG_ReadAngle16i();
	}
	else
	{
		if (bits & E_ANGLE1)
			e->angles[0] = MSG_ReadAngle8i();
		if (bits & E_ANGLE2)
			e->angles[1] = MSG_ReadAngle8i();
		if (bits & E_ANGLE3)
			e->angles[2] = MSG_ReadAngle8i();
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
	if (cls.protocol == PROTOCOL_DARKPLACES2)
		if (bits & E_FLAGS)
			e->flags = MSG_ReadByte();
	if (bits & E_TAGATTACHMENT)
	{
		e->tagentity = (unsigned short) MSG_ReadShort();
		e->tagindex = MSG_ReadByte();
	}
	if (bits & E_LIGHT)
	{
		e->light[0] = (unsigned short) MSG_ReadShort();
		e->light[1] = (unsigned short) MSG_ReadShort();
		e->light[2] = (unsigned short) MSG_ReadShort();
		e->light[3] = (unsigned short) MSG_ReadShort();
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

extern void CL_NewFrameReceived(int num);

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

// (server) clears the database to contain no frames (thus delta compression compresses against nothing)
void EntityFrame_ClearDatabase(entityframe_database_t *d)
{
	memset(d, 0, sizeof(*d));
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

// (server) clears frame, to prepare for adding entities
void EntityFrame_Clear(entity_frame_t *f, vec3_t eye, int framenum)
{
	f->time = 0;
	f->framenum = framenum;
	f->numentities = 0;
	if (eye == NULL)
		VectorClear(f->eye);
	else
		VectorCopy(eye, f->eye);
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
	number = MSG_ReadLong();
	f->framenum = MSG_ReadLong();
	CL_NewFrameReceived(f->framenum);
	f->eye[0] = MSG_ReadFloat();
	f->eye[1] = MSG_ReadFloat();
	f->eye[2] = MSG_ReadFloat();
	EntityFrame_AckFrame(d, number);
	EntityFrame_FetchFrame(d, number, delta);
	old = delta->entitydata;
	oldend = old + delta->numentities;
	// read entities until we hit the magic 0xFFFF end tag
	while ((number = (unsigned short) MSG_ReadShort()) != 0xFFFF && !msg_badread)
	{
		if (msg_badread)
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
			d->referenceentity[i] = defaultstate;
		// if this is the server, remove commits
			for (i = 0, commit = d->commit;i < MAX_ENTITY_HISTORY;i++, commit++)
				commit->numentities = 0;
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
	referenceframenum = MSG_ReadLong();
	// read the number of this frame
	framenum = MSG_ReadLong();
	CL_NewFrameReceived(framenum);
	// read the start number
	enumber = (unsigned short) MSG_ReadShort();
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




entityframe5_database_t *EntityFrame5_AllocDatabase(mempool_t *pool)
{
	int i;
	entityframe5_database_t *d;
	d = (entityframe5_database_t *)Mem_Alloc(pool, sizeof(*d));
	d->latestframenum = 0;
	for (i = 0;i < d->maxedicts;i++)
		d->states[i] = defaultstate;
	return d;
}

void EntityFrame5_FreeDatabase(entityframe5_database_t *d)
{
	// all the [maxedicts] memory is allocated at once, so there's only one
	// thing to free
	if (d->maxedicts)
		Mem_Free(d->deltabits);
	Mem_Free(d);
}

static void EntityFrame5_ExpandEdicts(entityframe5_database_t *d, int newmax)
{
	if (d->maxedicts < newmax)
	{
		unsigned char *data;
		int oldmaxedicts = d->maxedicts;
		int *olddeltabits = d->deltabits;
		unsigned char *oldpriorities = d->priorities;
		int *oldupdateframenum = d->updateframenum;
		entity_state_t *oldstates = d->states;
		unsigned char *oldvisiblebits = d->visiblebits;
		d->maxedicts = newmax;
		data = (unsigned char *)Mem_Alloc(sv_mempool, d->maxedicts * sizeof(int) + d->maxedicts * sizeof(unsigned char) + d->maxedicts * sizeof(int) + d->maxedicts * sizeof(entity_state_t) + (d->maxedicts+7)/8 * sizeof(unsigned char));
		d->deltabits = (int *)data;data += d->maxedicts * sizeof(int);
		d->priorities = (unsigned char *)data;data += d->maxedicts * sizeof(unsigned char);
		d->updateframenum = (int *)data;data += d->maxedicts * sizeof(int);
		d->states = (entity_state_t *)data;data += d->maxedicts * sizeof(entity_state_t);
		d->visiblebits = (unsigned char *)data;data += (d->maxedicts+7)/8 * sizeof(unsigned char);
		if (oldmaxedicts)
		{
			memcpy(d->deltabits, olddeltabits, oldmaxedicts * sizeof(int));
			memcpy(d->priorities, oldpriorities, oldmaxedicts * sizeof(unsigned char));
			memcpy(d->updateframenum, oldupdateframenum, oldmaxedicts * sizeof(int));
			memcpy(d->states, oldstates, oldmaxedicts * sizeof(entity_state_t));
			memcpy(d->visiblebits, oldvisiblebits, (oldmaxedicts+7)/8 * sizeof(unsigned char));
			// the previous buffers were a single allocation, so just one free
			Mem_Free(olddeltabits);
		}
	}
}

static int EntityState5_Priority(entityframe5_database_t *d, int stateindex)
{
	int limit, priority;
	entity_state_t *s = NULL; // hush compiler warning by initializing this
	// if it is the player, update urgently
	if (stateindex == d->viewentnum)
		return ENTITYFRAME5_PRIORITYLEVELS - 1;
	// priority increases each frame no matter what happens
	priority = d->priorities[stateindex] + 1;
	// players get an extra priority boost
	if (stateindex <= svs.maxclients)
		priority++;
	// remove dead entities very quickly because they are just 2 bytes
	if (d->states[stateindex].active != ACTIVE_NETWORK)
	{
		priority++;
		return bound(1, priority, ENTITYFRAME5_PRIORITYLEVELS - 1);
	}
	// certain changes are more noticable than others
	if (d->deltabits[stateindex] & (E5_FULLUPDATE | E5_ATTACHMENT | E5_MODEL | E5_FLAGS | E5_COLORMAP))
		priority++;
	// find the root entity this one is attached to, and judge relevance by it
	for (limit = 0;limit < 256;limit++)
	{
		s = d->states + stateindex;
		if (s->flags & RENDER_VIEWMODEL)
			stateindex = d->viewentnum;
		else if (s->tagentity)
			stateindex = s->tagentity;
		else
			break;
		if (d->maxedicts < stateindex)
			EntityFrame5_ExpandEdicts(d, (stateindex+256)&~255);
	}
	if (limit >= 256)
		Con_DPrintf("Protocol: Runaway loop recursing tagentity links on entity %i\n", stateindex);
	// now that we have the parent entity we can make some decisions based on
	// distance from the player
	if (VectorDistance(d->states[d->viewentnum].netcenter, s->netcenter) < 1024.0f)
		priority++;
	return bound(1, priority, ENTITYFRAME5_PRIORITYLEVELS - 1);
}

void EntityState5_WriteUpdate(int number, const entity_state_t *s, int changedbits, sizebuf_t *msg)
{
	unsigned int bits = 0;
	//dp_model_t *model;
	ENTITYSIZEPROFILING_START(msg, s->number);

	if (s->active != ACTIVE_NETWORK)
		MSG_WriteShort(msg, number | 0x8000);
	else
	{
		if (PRVM_serveredictfunction((&prog->edicts[s->number]), SendEntity))
			return;

		bits = changedbits;
		if ((bits & E5_ORIGIN) && (!(s->flags & RENDER_LOWPRECISION) || s->exteriormodelforclient || s->tagentity || s->viewmodelforclient || (s->number >= 1 && s->number <= svs.maxclients) || s->origin[0] <= -4096.0625 || s->origin[0] >= 4095.9375 || s->origin[1] <= -4096.0625 || s->origin[1] >= 4095.9375 || s->origin[2] <= -4096.0625 || s->origin[2] >= 4095.9375))
		// maybe also add: ((model = SV_GetModelByIndex(s->modelindex)) != NULL && model->name[0] == '*')
			bits |= E5_ORIGIN32;
			// possible values:
			//   negative origin:
			//     (int)(f * 8 - 0.5) >= -32768
			//          (f * 8 - 0.5) >  -32769
			//           f            >  -4096.0625
			//   positive origin:
			//     (int)(f * 8 + 0.5) <=  32767
			//          (f * 8 + 0.5) <   32768
			//           f * 8 + 0.5) <   4095.9375
		if ((bits & E5_ANGLES) && !(s->flags & RENDER_LOWPRECISION))
			bits |= E5_ANGLES16;
		if ((bits & E5_MODEL) && s->modelindex >= 256)
			bits |= E5_MODEL16;
		if ((bits & E5_FRAME) && s->frame >= 256)
			bits |= E5_FRAME16;
		if (bits & E5_EFFECTS)
		{
			if (s->effects & 0xFFFF0000)
				bits |= E5_EFFECTS32;
			else if (s->effects & 0xFFFFFF00)
				bits |= E5_EFFECTS16;
		}
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
				MSG_WriteCoord32f(msg, s->origin[0]);
				MSG_WriteCoord32f(msg, s->origin[1]);
				MSG_WriteCoord32f(msg, s->origin[2]);
			}
			else
			{
				MSG_WriteCoord13i(msg, s->origin[0]);
				MSG_WriteCoord13i(msg, s->origin[1]);
				MSG_WriteCoord13i(msg, s->origin[2]);
			}
		}
		if (bits & E5_ANGLES)
		{
			if (bits & E5_ANGLES16)
			{
				MSG_WriteAngle16i(msg, s->angles[0]);
				MSG_WriteAngle16i(msg, s->angles[1]);
				MSG_WriteAngle16i(msg, s->angles[2]);
			}
			else
			{
				MSG_WriteAngle8i(msg, s->angles[0]);
				MSG_WriteAngle8i(msg, s->angles[1]);
				MSG_WriteAngle8i(msg, s->angles[2]);
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
			MSG_WriteByte(msg, s->skin);
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
			MSG_WriteByte(msg, s->alpha);
		if (bits & E5_SCALE)
			MSG_WriteByte(msg, s->scale);
		if (bits & E5_COLORMAP)
			MSG_WriteByte(msg, s->colormap);
		if (bits & E5_ATTACHMENT)
		{
			MSG_WriteShort(msg, s->tagentity);
			MSG_WriteByte(msg, s->tagindex);
		}
		if (bits & E5_LIGHT)
		{
			MSG_WriteShort(msg, s->light[0]);
			MSG_WriteShort(msg, s->light[1]);
			MSG_WriteShort(msg, s->light[2]);
			MSG_WriteShort(msg, s->light[3]);
			MSG_WriteByte(msg, s->lightstyle);
			MSG_WriteByte(msg, s->lightpflags);
		}
		if (bits & E5_GLOW)
		{
			MSG_WriteByte(msg, s->glowsize);
			MSG_WriteByte(msg, s->glowcolor);
		}
		if (bits & E5_COLORMOD)
		{
			MSG_WriteByte(msg, s->colormod[0]);
			MSG_WriteByte(msg, s->colormod[1]);
			MSG_WriteByte(msg, s->colormod[2]);
		}
		if (bits & E5_GLOWMOD)
		{
			MSG_WriteByte(msg, s->glowmod[0]);
			MSG_WriteByte(msg, s->glowmod[1]);
			MSG_WriteByte(msg, s->glowmod[2]);
		}
		if (bits & E5_COMPLEXANIMATION)
		{
			if (s->skeletonobject.model && s->skeletonobject.relativetransforms)
			{
				int numbones = s->skeletonobject.model->num_bones;
				int bonenum;
				short pose6s[6];
				MSG_WriteByte(msg, 4);
				MSG_WriteShort(msg, s->modelindex);
				MSG_WriteByte(msg, numbones);
				for (bonenum = 0;bonenum < numbones;bonenum++)
				{
					Matrix4x4_ToBonePose6s(s->skeletonobject.relativetransforms + bonenum, 64, pose6s);
					MSG_WriteShort(msg, pose6s[0]);
					MSG_WriteShort(msg, pose6s[1]);
					MSG_WriteShort(msg, pose6s[2]);
					MSG_WriteShort(msg, pose6s[3]);
					MSG_WriteShort(msg, pose6s[4]);
					MSG_WriteShort(msg, pose6s[5]);
				}
			}
			else if (s->framegroupblend[3].lerp > 0)
			{
				MSG_WriteByte(msg, 3);
				MSG_WriteShort(msg, s->framegroupblend[0].frame);
				MSG_WriteShort(msg, s->framegroupblend[1].frame);
				MSG_WriteShort(msg, s->framegroupblend[2].frame);
				MSG_WriteShort(msg, s->framegroupblend[3].frame);
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[0].start) * 1000.0));
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[1].start) * 1000.0));
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[2].start) * 1000.0));
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[3].start) * 1000.0));
				MSG_WriteByte(msg, s->framegroupblend[0].lerp * 255.0f);
				MSG_WriteByte(msg, s->framegroupblend[1].lerp * 255.0f);
				MSG_WriteByte(msg, s->framegroupblend[2].lerp * 255.0f);
				MSG_WriteByte(msg, s->framegroupblend[3].lerp * 255.0f);
			}
			else if (s->framegroupblend[2].lerp > 0)
			{
				MSG_WriteByte(msg, 2);
				MSG_WriteShort(msg, s->framegroupblend[0].frame);
				MSG_WriteShort(msg, s->framegroupblend[1].frame);
				MSG_WriteShort(msg, s->framegroupblend[2].frame);
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[0].start) * 1000.0));
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[1].start) * 1000.0));
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[2].start) * 1000.0));
				MSG_WriteByte(msg, s->framegroupblend[0].lerp * 255.0f);
				MSG_WriteByte(msg, s->framegroupblend[1].lerp * 255.0f);
				MSG_WriteByte(msg, s->framegroupblend[2].lerp * 255.0f);
			}
			else if (s->framegroupblend[1].lerp > 0)
			{
				MSG_WriteByte(msg, 1);
				MSG_WriteShort(msg, s->framegroupblend[0].frame);
				MSG_WriteShort(msg, s->framegroupblend[1].frame);
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[0].start) * 1000.0));
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[1].start) * 1000.0));
				MSG_WriteByte(msg, s->framegroupblend[0].lerp * 255.0f);
				MSG_WriteByte(msg, s->framegroupblend[1].lerp * 255.0f);
			}
			else
			{
				MSG_WriteByte(msg, 0);
				MSG_WriteShort(msg, s->framegroupblend[0].frame);
				MSG_WriteShort(msg, (int)((sv.time - s->framegroupblend[0].start) * 1000.0));
			}
		}
		if (bits & E5_TRAILEFFECTNUM)
			MSG_WriteShort(msg, s->traileffectnum);
	}

	ENTITYSIZEPROFILING_END(msg, s->number);
}

extern dp_model_t *CL_GetModelByIndex(int modelindex);

static void EntityState5_ReadUpdate(entity_state_t *s, int number)
{
	int bits;
	bits = MSG_ReadByte();
	if (bits & E5_EXTEND1)
	{
		bits |= MSG_ReadByte() << 8;
		if (bits & E5_EXTEND2)
		{
			bits |= MSG_ReadByte() << 16;
			if (bits & E5_EXTEND3)
				bits |= MSG_ReadByte() << 24;
		}
	}
	if (bits & E5_FULLUPDATE)
	{
		*s = defaultstate;
		s->active = ACTIVE_NETWORK;
	}
	if (bits & E5_FLAGS)
		s->flags = MSG_ReadByte();
	if (bits & E5_ORIGIN)
	{
		if (bits & E5_ORIGIN32)
		{
			s->origin[0] = MSG_ReadCoord32f();
			s->origin[1] = MSG_ReadCoord32f();
			s->origin[2] = MSG_ReadCoord32f();
		}
		else
		{
			s->origin[0] = MSG_ReadCoord13i();
			s->origin[1] = MSG_ReadCoord13i();
			s->origin[2] = MSG_ReadCoord13i();
		}
	}
	if (bits & E5_ANGLES)
	{
		if (bits & E5_ANGLES16)
		{
			s->angles[0] = MSG_ReadAngle16i();
			s->angles[1] = MSG_ReadAngle16i();
			s->angles[2] = MSG_ReadAngle16i();
		}
		else
		{
			s->angles[0] = MSG_ReadAngle8i();
			s->angles[1] = MSG_ReadAngle8i();
			s->angles[2] = MSG_ReadAngle8i();
		}
	}
	if (bits & E5_MODEL)
	{
		if (bits & E5_MODEL16)
			s->modelindex = (unsigned short) MSG_ReadShort();
		else
			s->modelindex = MSG_ReadByte();
	}
	if (bits & E5_FRAME)
	{
		if (bits & E5_FRAME16)
			s->frame = (unsigned short) MSG_ReadShort();
		else
			s->frame = MSG_ReadByte();
	}
	if (bits & E5_SKIN)
		s->skin = MSG_ReadByte();
	if (bits & E5_EFFECTS)
	{
		if (bits & E5_EFFECTS32)
			s->effects = (unsigned int) MSG_ReadLong();
		else if (bits & E5_EFFECTS16)
			s->effects = (unsigned short) MSG_ReadShort();
		else
			s->effects = MSG_ReadByte();
	}
	if (bits & E5_ALPHA)
		s->alpha = MSG_ReadByte();
	if (bits & E5_SCALE)
		s->scale = MSG_ReadByte();
	if (bits & E5_COLORMAP)
		s->colormap = MSG_ReadByte();
	if (bits & E5_ATTACHMENT)
	{
		s->tagentity = (unsigned short) MSG_ReadShort();
		s->tagindex = MSG_ReadByte();
	}
	if (bits & E5_LIGHT)
	{
		s->light[0] = (unsigned short) MSG_ReadShort();
		s->light[1] = (unsigned short) MSG_ReadShort();
		s->light[2] = (unsigned short) MSG_ReadShort();
		s->light[3] = (unsigned short) MSG_ReadShort();
		s->lightstyle = MSG_ReadByte();
		s->lightpflags = MSG_ReadByte();
	}
	if (bits & E5_GLOW)
	{
		s->glowsize = MSG_ReadByte();
		s->glowcolor = MSG_ReadByte();
	}
	if (bits & E5_COLORMOD)
	{
		s->colormod[0] = MSG_ReadByte();
		s->colormod[1] = MSG_ReadByte();
		s->colormod[2] = MSG_ReadByte();
	}
	if (bits & E5_GLOWMOD)
	{
		s->glowmod[0] = MSG_ReadByte();
		s->glowmod[1] = MSG_ReadByte();
		s->glowmod[2] = MSG_ReadByte();
	}
	if (bits & E5_COMPLEXANIMATION)
	{
		skeleton_t *skeleton;
		const dp_model_t *model;
		int modelindex;
		int type;
		int bonenum;
		int numbones;
		short pose6s[6];
		type = MSG_ReadByte();
		switch(type)
		{
		case 0:
			s->framegroupblend[0].frame = MSG_ReadShort();
			s->framegroupblend[1].frame = 0;
			s->framegroupblend[2].frame = 0;
			s->framegroupblend[3].frame = 0;
			s->framegroupblend[0].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[1].start = 0;
			s->framegroupblend[2].start = 0;
			s->framegroupblend[3].start = 0;
			s->framegroupblend[0].lerp = 1;
			s->framegroupblend[1].lerp = 0;
			s->framegroupblend[2].lerp = 0;
			s->framegroupblend[3].lerp = 0;
			break;
		case 1:
			s->framegroupblend[0].frame = MSG_ReadShort();
			s->framegroupblend[1].frame = MSG_ReadShort();
			s->framegroupblend[2].frame = 0;
			s->framegroupblend[3].frame = 0;
			s->framegroupblend[0].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[1].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[2].start = 0;
			s->framegroupblend[3].start = 0;
			s->framegroupblend[0].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[1].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[2].lerp = 0;
			s->framegroupblend[3].lerp = 0;
			break;
		case 2:
			s->framegroupblend[0].frame = MSG_ReadShort();
			s->framegroupblend[1].frame = MSG_ReadShort();
			s->framegroupblend[2].frame = MSG_ReadShort();
			s->framegroupblend[3].frame = 0;
			s->framegroupblend[0].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[1].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[2].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[3].start = 0;
			s->framegroupblend[0].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[1].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[2].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[3].lerp = 0;
			break;
		case 3:
			s->framegroupblend[0].frame = MSG_ReadShort();
			s->framegroupblend[1].frame = MSG_ReadShort();
			s->framegroupblend[2].frame = MSG_ReadShort();
			s->framegroupblend[3].frame = MSG_ReadShort();
			s->framegroupblend[0].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[1].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[2].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[3].start = cl.time - (short)MSG_ReadShort() * (1.0f / 1000.0f);
			s->framegroupblend[0].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[1].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[2].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			s->framegroupblend[3].lerp = MSG_ReadByte() * (1.0f / 255.0f);
			break;
		case 4:
			if (!cl.engineskeletonobjects)
				cl.engineskeletonobjects = (skeleton_t *) Mem_Alloc(cls.levelmempool, sizeof(*cl.engineskeletonobjects) * MAX_EDICTS);
			skeleton = &cl.engineskeletonobjects[number];
			modelindex = MSG_ReadShort();
			model = CL_GetModelByIndex(modelindex);
			numbones = MSG_ReadByte();
			if (model && numbones != model->num_bones)
				Host_Error("E5_COMPLEXANIMATION: model has different number of bones than network packet describes\n");
			if (!skeleton->relativetransforms || skeleton->model != model)
			{
				skeleton->model = model;
				skeleton->relativetransforms = (matrix4x4_t *) Mem_Realloc(cls.levelmempool, skeleton->relativetransforms, sizeof(*skeleton->relativetransforms) * skeleton->model->num_bones);
				for (bonenum = 0;bonenum < model->num_bones;bonenum++)
					skeleton->relativetransforms[bonenum] = identitymatrix;
			}
			for (bonenum = 0;bonenum < numbones;bonenum++)
			{
				pose6s[0] = (short)MSG_ReadShort();
				pose6s[1] = (short)MSG_ReadShort();
				pose6s[2] = (short)MSG_ReadShort();
				pose6s[3] = (short)MSG_ReadShort();
				pose6s[4] = (short)MSG_ReadShort();
				pose6s[5] = (short)MSG_ReadShort();
				Matrix4x4_FromBonePose6s(skeleton->relativetransforms + bonenum, 1.0f / 64.0f, pose6s);
			}
			s->skeletonobject = *skeleton;
			break;
		default:
			Host_Error("E5_COMPLEXANIMATION: Parse error - unknown type %i\n", type);
			break;
		}
	}
	if (bits & E5_TRAILEFFECTNUM)
		s->traileffectnum = (unsigned short) MSG_ReadShort();


	if (developer_networkentities.integer >= 2)
	{
		Con_Printf("ReadFields e%i", number);

		if (bits & E5_ORIGIN)
			Con_Printf(" E5_ORIGIN %f %f %f", s->origin[0], s->origin[1], s->origin[2]);
		if (bits & E5_ANGLES)
			Con_Printf(" E5_ANGLES %f %f %f", s->angles[0], s->angles[1], s->angles[2]);
		if (bits & E5_MODEL)
			Con_Printf(" E5_MODEL %i", s->modelindex);
		if (bits & E5_FRAME)
			Con_Printf(" E5_FRAME %i", s->frame);
		if (bits & E5_SKIN)
			Con_Printf(" E5_SKIN %i", s->skin);
		if (bits & E5_EFFECTS)
			Con_Printf(" E5_EFFECTS %i", s->effects);
		if (bits & E5_FLAGS)
		{
			Con_Printf(" E5_FLAGS %i (", s->flags);
			if (s->flags & RENDER_STEP)
				Con_Print(" STEP");
			if (s->flags & RENDER_GLOWTRAIL)
				Con_Print(" GLOWTRAIL");
			if (s->flags & RENDER_VIEWMODEL)
				Con_Print(" VIEWMODEL");
			if (s->flags & RENDER_EXTERIORMODEL)
				Con_Print(" EXTERIORMODEL");
			if (s->flags & RENDER_LOWPRECISION)
				Con_Print(" LOWPRECISION");
			if (s->flags & RENDER_COLORMAPPED)
				Con_Print(" COLORMAPPED");
			if (s->flags & RENDER_SHADOW)
				Con_Print(" SHADOW");
			if (s->flags & RENDER_LIGHT)
				Con_Print(" LIGHT");
			if (s->flags & RENDER_NOSELFSHADOW)
				Con_Print(" NOSELFSHADOW");
			Con_Print(")");
		}
		if (bits & E5_ALPHA)
			Con_Printf(" E5_ALPHA %f", s->alpha / 255.0f);
		if (bits & E5_SCALE)
			Con_Printf(" E5_SCALE %f", s->scale / 16.0f);
		if (bits & E5_COLORMAP)
			Con_Printf(" E5_COLORMAP %i", s->colormap);
		if (bits & E5_ATTACHMENT)
			Con_Printf(" E5_ATTACHMENT e%i:%i", s->tagentity, s->tagindex);
		if (bits & E5_LIGHT)
			Con_Printf(" E5_LIGHT %i:%i:%i:%i %i:%i", s->light[0], s->light[1], s->light[2], s->light[3], s->lightstyle, s->lightpflags);
		if (bits & E5_GLOW)
			Con_Printf(" E5_GLOW %i:%i", s->glowsize * 4, s->glowcolor);
		if (bits & E5_COLORMOD)
			Con_Printf(" E5_COLORMOD %f:%f:%f", s->colormod[0] / 32.0f, s->colormod[1] / 32.0f, s->colormod[2] / 32.0f);
		if (bits & E5_GLOWMOD)
			Con_Printf(" E5_GLOWMOD %f:%f:%f", s->glowmod[0] / 32.0f, s->glowmod[1] / 32.0f, s->glowmod[2] / 32.0f);
		Con_Print("\n");
	}
}

static int EntityState5_DeltaBits(const entity_state_t *o, const entity_state_t *n)
{
	unsigned int bits = 0;
	if (n->active == ACTIVE_NETWORK)
	{
		if (o->active != ACTIVE_NETWORK)
			bits |= E5_FULLUPDATE;
		if (!VectorCompare(o->origin, n->origin))
			bits |= E5_ORIGIN;
		if (!VectorCompare(o->angles, n->angles))
			bits |= E5_ANGLES;
		if (o->modelindex != n->modelindex)
			bits |= E5_MODEL;
		if (o->frame != n->frame)
			bits |= E5_FRAME;
		if (o->skin != n->skin)
			bits |= E5_SKIN;
		if (o->effects != n->effects)
			bits |= E5_EFFECTS;
		if (o->flags != n->flags)
			bits |= E5_FLAGS;
		if (o->alpha != n->alpha)
			bits |= E5_ALPHA;
		if (o->scale != n->scale)
			bits |= E5_SCALE;
		if (o->colormap != n->colormap)
			bits |= E5_COLORMAP;
		if (o->tagentity != n->tagentity || o->tagindex != n->tagindex)
			bits |= E5_ATTACHMENT;
		if (o->light[0] != n->light[0] || o->light[1] != n->light[1] || o->light[2] != n->light[2] || o->light[3] != n->light[3] || o->lightstyle != n->lightstyle || o->lightpflags != n->lightpflags)
			bits |= E5_LIGHT;
		if (o->glowsize != n->glowsize || o->glowcolor != n->glowcolor)
			bits |= E5_GLOW;
		if (o->colormod[0] != n->colormod[0] || o->colormod[1] != n->colormod[1] || o->colormod[2] != n->colormod[2])
			bits |= E5_COLORMOD;
		if (o->glowmod[0] != n->glowmod[0] || o->glowmod[1] != n->glowmod[1] || o->glowmod[2] != n->glowmod[2])
			bits |= E5_GLOWMOD;
		if (n->flags & RENDER_COMPLEXANIMATION)
			bits |= E5_COMPLEXANIMATION;
		if (o->traileffectnum != n->traileffectnum)
			bits |= E5_TRAILEFFECTNUM;
	}
	else
		if (o->active == ACTIVE_NETWORK)
			bits |= E5_FULLUPDATE;
	return bits;
}

void EntityFrame5_CL_ReadFrame(void)
{
	int n, enumber, framenum;
	entity_t *ent;
	entity_state_t *s;
	// read the number of this frame to echo back in next input packet
	framenum = MSG_ReadLong();
	CL_NewFrameReceived(framenum);
	if (cls.protocol != PROTOCOL_QUAKE && cls.protocol != PROTOCOL_QUAKEDP && cls.protocol != PROTOCOL_NEHAHRAMOVIE && cls.protocol != PROTOCOL_DARKPLACES1 && cls.protocol != PROTOCOL_DARKPLACES2 && cls.protocol != PROTOCOL_DARKPLACES3 && cls.protocol != PROTOCOL_DARKPLACES4 && cls.protocol != PROTOCOL_DARKPLACES5 && cls.protocol != PROTOCOL_DARKPLACES6)
		cls.servermovesequence = MSG_ReadLong();
	// read entity numbers until we find a 0x8000
	// (which would be remove world entity, but is actually a terminator)
	while ((n = (unsigned short)MSG_ReadShort()) != 0x8000 && !msg_badread)
	{
		// get the entity number
		enumber = n & 0x7FFF;
		// we may need to expand the array
		if (cl.num_entities <= enumber)
		{
			cl.num_entities = enumber + 1;
			if (enumber >= cl.max_entities)
				CL_ExpandEntities(enumber);
		}
		// look up the entity
		ent = cl.entities + enumber;
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
			EntityState5_ReadUpdate(s, enumber);
		}
		// set the cl.entities_active flag
		cl.entities_active[enumber] = (s->active == ACTIVE_NETWORK);
		// set the update time
		s->time = cl.mtime[0];
		// fix the number (it gets wiped occasionally by copying from defaultstate)
		s->number = enumber;
		// check if we need to update the lerp stuff
		if (s->active == ACTIVE_NETWORK)
			CL_MoveLerpEntityStates(&cl.entities[enumber]);
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

static int packetlog5cmp(const void *a_, const void *b_)
{
	const entityframe5_packetlog_t *a = (const entityframe5_packetlog_t *) a_;
	const entityframe5_packetlog_t *b = (const entityframe5_packetlog_t *) b_;
	return a->packetnumber - b->packetnumber;
}

void EntityFrame5_LostFrame(entityframe5_database_t *d, int framenum)
{
	int i, j, l, bits;
	entityframe5_changestate_t *s;
	entityframe5_packetlog_t *p;
	static unsigned char statsdeltabits[(MAX_CL_STATS+7)/8];
	static int deltabits[MAX_EDICTS];
	entityframe5_packetlog_t *packetlogs[ENTITYFRAME5_MAXPACKETLOGS];

	for (i = 0, p = d->packetlog;i < ENTITYFRAME5_MAXPACKETLOGS;i++, p++)
		packetlogs[i] = p;
	qsort(packetlogs, sizeof(*packetlogs), ENTITYFRAME5_MAXPACKETLOGS, packetlog5cmp);

	memset(deltabits, 0, sizeof(deltabits));
	memset(statsdeltabits, 0, sizeof(statsdeltabits));
	for (i = 0; i < ENTITYFRAME5_MAXPACKETLOGS; i++)
	{
		p = packetlogs[i];

		if (!p->packetnumber)
			continue;

		if (p->packetnumber <= framenum)
		{
			for (j = 0, s = p->states;j < p->numstates;j++, s++)
				deltabits[s->number] |= s->bits;

			for (l = 0;l < (MAX_CL_STATS+7)/8;l++)
				statsdeltabits[l] |= p->statsdeltabits[l];

			p->packetnumber = 0;
		}
		else
		{
			for (j = 0, s = p->states;j < p->numstates;j++, s++)
				deltabits[s->number] &= ~s->bits;
			for (l = 0;l < (MAX_CL_STATS+7)/8;l++)
				statsdeltabits[l] &= ~p->statsdeltabits[l];
		}
	}

	for(i = 0; i < d->maxedicts; ++i)
	{
		bits = deltabits[i] & ~d->deltabits[i];
		if(bits)
		{
			d->deltabits[i] |= bits;
			// if it was a very important update, set priority higher
			if (bits & (E5_FULLUPDATE | E5_ATTACHMENT | E5_MODEL | E5_COLORMAP))
				d->priorities[i] = max(d->priorities[i], 4);
			else
				d->priorities[i] = max(d->priorities[i], 1);
		}
	}

	for (l = 0;l < (MAX_CL_STATS+7)/8;l++)
		host_client->statsdeltabits[l] |= statsdeltabits[l];
		// no need to mask out the already-set bits here, as we do not
		// do that priorities stuff
}

void EntityFrame5_AckFrame(entityframe5_database_t *d, int framenum)
{
	int i;
	// scan for packets made obsolete by this ack and delete them
	for (i = 0;i < ENTITYFRAME5_MAXPACKETLOGS;i++)
		if (d->packetlog[i].packetnumber <= framenum)
			d->packetlog[i].packetnumber = 0;
}

qboolean EntityFrame5_WriteFrame(sizebuf_t *msg, int maxsize, entityframe5_database_t *d, int numstates, const entity_state_t **states, int viewentnum, int movesequence, qboolean need_empty)
{
	const entity_state_t *n;
	int i, num, l, framenum, packetlognumber, priority;
	sizebuf_t buf;
	unsigned char data[128];
	entityframe5_packetlog_t *packetlog;

	if (prog->max_edicts > d->maxedicts)
		EntityFrame5_ExpandEdicts(d, prog->max_edicts);

	framenum = d->latestframenum + 1;
	d->viewentnum = viewentnum;

	// if packet log is full, mark all frames as lost, this will cause
	// it to send the lost data again
	for (packetlognumber = 0;packetlognumber < ENTITYFRAME5_MAXPACKETLOGS;packetlognumber++)
		if (d->packetlog[packetlognumber].packetnumber == 0)
			break;
	if (packetlognumber == ENTITYFRAME5_MAXPACKETLOGS)
	{
		Con_DPrintf("EntityFrame5_WriteFrame: packetlog overflow for a client, resetting\n");
		EntityFrame5_LostFrame(d, framenum);
		packetlognumber = 0;
	}

	// prepare the buffer
	memset(&buf, 0, sizeof(buf));
	buf.data = data;
	buf.maxsize = sizeof(data);

	// detect changes in states
	num = 1;
	for (i = 0;i < numstates;i++)
	{
		n = states[i];
		// mark gaps in entity numbering as removed entities
		for (;num < n->number;num++)
		{
			// if the entity used to exist, clear it
			if (CHECKPVSBIT(d->visiblebits, num))
			{
				CLEARPVSBIT(d->visiblebits, num);
				d->deltabits[num] = E5_FULLUPDATE;
				d->priorities[num] = max(d->priorities[num], 8); // removal is cheap
				d->states[num] = defaultstate;
				d->states[num].number = num;
			}
		}
		// update the entity state data
		if (!CHECKPVSBIT(d->visiblebits, num))
		{
			// entity just spawned in, don't let it completely hog priority
			// because of being ancient on the first frame
			d->updateframenum[num] = framenum;
			// initial priority is a bit high to make projectiles send on the
			// first frame, among other things
			d->priorities[num] = max(d->priorities[num], 4);
		}
		SETPVSBIT(d->visiblebits, num);
		d->deltabits[num] |= EntityState5_DeltaBits(d->states + num, n);
		d->priorities[num] = max(d->priorities[num], 1);
		d->states[num] = *n;
		d->states[num].number = num;
		// advance to next entity so the next iteration doesn't immediately remove it
		num++;
	}
	// all remaining entities are dead
	for (;num < d->maxedicts;num++)
	{
		if (CHECKPVSBIT(d->visiblebits, num))
		{
			CLEARPVSBIT(d->visiblebits, num);
			d->deltabits[num] = E5_FULLUPDATE;
			d->priorities[num] = max(d->priorities[num], 8); // removal is cheap
			d->states[num] = defaultstate;
			d->states[num].number = num;
		}
	}

	// if there isn't at least enough room for an empty svc_entities,
	// don't bother trying...
	if (buf.cursize + 11 > buf.maxsize)
		return false;

	// build lists of entities by priority level
	memset(d->prioritychaincounts, 0, sizeof(d->prioritychaincounts));
	l = 0;
	for (num = 0;num < d->maxedicts;num++)
	{
		if (d->priorities[num])
		{
			if (d->deltabits[num])
			{
				if (d->priorities[num] < (ENTITYFRAME5_PRIORITYLEVELS - 1))
					d->priorities[num] = EntityState5_Priority(d, num);
				l = num;
				priority = d->priorities[num];
				if (d->prioritychaincounts[priority] < ENTITYFRAME5_MAXSTATES)
					d->prioritychains[priority][d->prioritychaincounts[priority]++] = num;
			}
			else
				d->priorities[num] = 0;
		}
	}

	packetlog = NULL;
	// write stat updates
	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_NEHAHRABJP && sv.protocol != PROTOCOL_NEHAHRABJP2 && sv.protocol != PROTOCOL_NEHAHRABJP3 && sv.protocol != PROTOCOL_DARKPLACES1 && sv.protocol != PROTOCOL_DARKPLACES2 && sv.protocol != PROTOCOL_DARKPLACES3 && sv.protocol != PROTOCOL_DARKPLACES4 && sv.protocol != PROTOCOL_DARKPLACES5)
	{
		for (i = 0;i < MAX_CL_STATS && msg->cursize + 6 + 11 <= maxsize;i++)
		{
			if (host_client->statsdeltabits[i>>3] & (1<<(i&7)))
			{
				host_client->statsdeltabits[i>>3] &= ~(1<<(i&7));
				// add packetlog entry now that we have something for it
				if (!packetlog)
				{
					packetlog = d->packetlog + packetlognumber;
					packetlog->packetnumber = framenum;
					packetlog->numstates = 0;
					memset(packetlog->statsdeltabits, 0, sizeof(packetlog->statsdeltabits));
				}
				packetlog->statsdeltabits[i>>3] |= (1<<(i&7));
				if (host_client->stats[i] >= 0 && host_client->stats[i] < 256)
				{
					MSG_WriteByte(msg, svc_updatestatubyte);
					MSG_WriteByte(msg, i);
					MSG_WriteByte(msg, host_client->stats[i]);
					l = 1;
				}
				else
				{
					MSG_WriteByte(msg, svc_updatestat);
					MSG_WriteByte(msg, i);
					MSG_WriteLong(msg, host_client->stats[i]);
					l = 1;
				}
			}
		}
	}

	// only send empty svc_entities frame if needed
	if(!l && !need_empty)
		return false;

	// add packetlog entry now that we have something for it
	if (!packetlog)
	{
		packetlog = d->packetlog + packetlognumber;
		packetlog->packetnumber = framenum;
		packetlog->numstates = 0;
		memset(packetlog->statsdeltabits, 0, sizeof(packetlog->statsdeltabits));
	}

	// write state updates
	if (developer_networkentities.integer >= 10)
		Con_Printf("send: svc_entities %i\n", framenum);
	d->latestframenum = framenum;
	MSG_WriteByte(msg, svc_entities);
	MSG_WriteLong(msg, framenum);
	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_DARKPLACES1 && sv.protocol != PROTOCOL_DARKPLACES2 && sv.protocol != PROTOCOL_DARKPLACES3 && sv.protocol != PROTOCOL_DARKPLACES4 && sv.protocol != PROTOCOL_DARKPLACES5 && sv.protocol != PROTOCOL_DARKPLACES6)
		MSG_WriteLong(msg, movesequence);
	for (priority = ENTITYFRAME5_PRIORITYLEVELS - 1;priority >= 0 && packetlog->numstates < ENTITYFRAME5_MAXSTATES;priority--)
	{
		for (i = 0;i < d->prioritychaincounts[priority] && packetlog->numstates < ENTITYFRAME5_MAXSTATES;i++)
		{
			num = d->prioritychains[priority][i];
			n = d->states + num;
			if (d->deltabits[num] & E5_FULLUPDATE)
				d->deltabits[num] = E5_FULLUPDATE | EntityState5_DeltaBits(&defaultstate, n);
			buf.cursize = 0;
			EntityState5_WriteUpdate(num, n, d->deltabits[num], &buf);
			// if the entity won't fit, try the next one
			if (msg->cursize + buf.cursize + 2 > maxsize)
				continue;
			// write entity to the packet
			SZ_Write(msg, buf.data, buf.cursize);
			// mark age on entity for prioritization
			d->updateframenum[num] = framenum;
			// log entity so deltabits can be restored later if lost
			packetlog->states[packetlog->numstates].number = num;
			packetlog->states[packetlog->numstates].bits = d->deltabits[num];
			packetlog->numstates++;
			// clear deltabits and priority so it won't be sent again
			d->deltabits[num] = 0;
			d->priorities[num] = 0;
		}
	}
	MSG_WriteShort(msg, 0x8000);

	return true;
}


static void QW_TranslateEffects(entity_state_t *s, int qweffects)
{
	s->effects = 0;
	s->internaleffects = 0;
	if (qweffects & QW_EF_BRIGHTFIELD)
		s->effects |= EF_BRIGHTFIELD;
	if (qweffects & QW_EF_MUZZLEFLASH)
		s->effects |= EF_MUZZLEFLASH;
	if (qweffects & QW_EF_FLAG1)
	{
		// mimic FTEQW's interpretation of EF_FLAG1 as EF_NODRAW on non-player entities
		if (s->number > cl.maxclients)
			s->effects |= EF_NODRAW;
		else
			s->internaleffects |= INTEF_FLAG1QW;
	}
	if (qweffects & QW_EF_FLAG2)
	{
		// mimic FTEQW's interpretation of EF_FLAG2 as EF_ADDITIVE on non-player entities
		if (s->number > cl.maxclients)
			s->effects |= EF_ADDITIVE;
		else
			s->internaleffects |= INTEF_FLAG2QW;
	}
	if (qweffects & QW_EF_RED)
	{
		if (qweffects & QW_EF_BLUE)
			s->effects |= EF_RED | EF_BLUE;
		else
			s->effects |= EF_RED;
	}
	else if (qweffects & QW_EF_BLUE)
		s->effects |= EF_BLUE;
	else if (qweffects & QW_EF_BRIGHTLIGHT)
		s->effects |= EF_BRIGHTLIGHT;
	else if (qweffects & QW_EF_DIMLIGHT)
		s->effects |= EF_DIMLIGHT;
}

void EntityStateQW_ReadPlayerUpdate(void)
{
	int slot = MSG_ReadByte();
	int enumber = slot + 1;
	int weaponframe;
	int msec;
	int playerflags;
	int bits;
	entity_state_t *s;
	// look up the entity
	entity_t *ent = cl.entities + enumber;
	vec3_t viewangles;
	vec3_t velocity;

	// slide the current state into the previous
	ent->state_previous = ent->state_current;

	// read the update
	s = &ent->state_current;
	*s = defaultstate;
	s->active = ACTIVE_NETWORK;
	s->number = enumber;
	s->colormap = enumber;
	playerflags = MSG_ReadShort();
	MSG_ReadVector(s->origin, cls.protocol);
	s->frame = MSG_ReadByte();

	VectorClear(viewangles);
	VectorClear(velocity);

	if (playerflags & QW_PF_MSEC)
	{
		// time difference between last update this player sent to the server,
		// and last input we sent to the server (this packet is in response to
		// our input, so msec is how long ago the last update of this player
		// entity occurred, compared to our input being received)
		msec = MSG_ReadByte();
	}
	else
		msec = 0;
	if (playerflags & QW_PF_COMMAND)
	{
		bits = MSG_ReadByte();
		if (bits & QW_CM_ANGLE1)
			viewangles[0] = MSG_ReadAngle16i(); // cmd->angles[0]
		if (bits & QW_CM_ANGLE2)
			viewangles[1] = MSG_ReadAngle16i(); // cmd->angles[1]
		if (bits & QW_CM_ANGLE3)
			viewangles[2] = MSG_ReadAngle16i(); // cmd->angles[2]
		if (bits & QW_CM_FORWARD)
			MSG_ReadShort(); // cmd->forwardmove
		if (bits & QW_CM_SIDE)
			MSG_ReadShort(); // cmd->sidemove
		if (bits & QW_CM_UP)
			MSG_ReadShort(); // cmd->upmove
		if (bits & QW_CM_BUTTONS)
			(void) MSG_ReadByte(); // cmd->buttons
		if (bits & QW_CM_IMPULSE)
			(void) MSG_ReadByte(); // cmd->impulse
		(void) MSG_ReadByte(); // cmd->msec
	}
	if (playerflags & QW_PF_VELOCITY1)
		velocity[0] = MSG_ReadShort();
	if (playerflags & QW_PF_VELOCITY2)
		velocity[1] = MSG_ReadShort();
	if (playerflags & QW_PF_VELOCITY3)
		velocity[2] = MSG_ReadShort();
	if (playerflags & QW_PF_MODEL)
		s->modelindex = MSG_ReadByte();
	else
		s->modelindex = cl.qw_modelindex_player;
	if (playerflags & QW_PF_SKINNUM)
		s->skin = MSG_ReadByte();
	if (playerflags & QW_PF_EFFECTS)
		QW_TranslateEffects(s, MSG_ReadByte());
	if (playerflags & QW_PF_WEAPONFRAME)
		weaponframe = MSG_ReadByte();
	else
		weaponframe = 0;

	if (enumber == cl.playerentity)
	{
		// if this is an update on our player, update the angles
		VectorCopy(cl.viewangles, viewangles);
	}

	// calculate the entity angles from the viewangles
	s->angles[0] = viewangles[0] * -0.0333;
	s->angles[1] = viewangles[1];
	s->angles[2] = 0;
	s->angles[2] = V_CalcRoll(s->angles, velocity)*4;

	// if this is an update on our player, update interpolation state
	if (enumber == cl.playerentity)
	{
		VectorCopy (cl.mpunchangle[0], cl.mpunchangle[1]);
		VectorCopy (cl.mpunchvector[0], cl.mpunchvector[1]);
		VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
		cl.mviewzoom[1] = cl.mviewzoom[0];

		cl.idealpitch = 0;
		cl.mpunchangle[0][0] = 0;
		cl.mpunchangle[0][1] = 0;
		cl.mpunchangle[0][2] = 0;
		cl.mpunchvector[0][0] = 0;
		cl.mpunchvector[0][1] = 0;
		cl.mpunchvector[0][2] = 0;
		cl.mvelocity[0][0] = 0;
		cl.mvelocity[0][1] = 0;
		cl.mvelocity[0][2] = 0;
		cl.mviewzoom[0] = 1;

		VectorCopy(velocity, cl.mvelocity[0]);
		cl.stats[STAT_WEAPONFRAME] = weaponframe;
		if (playerflags & QW_PF_GIB)
			cl.stats[STAT_VIEWHEIGHT] = 8;
		else if (playerflags & QW_PF_DEAD)
			cl.stats[STAT_VIEWHEIGHT] = -16;
		else
			cl.stats[STAT_VIEWHEIGHT] = 22;
	}

	// set the cl.entities_active flag
	cl.entities_active[enumber] = (s->active == ACTIVE_NETWORK);
	// set the update time
	s->time = cl.mtime[0] - msec * 0.001; // qw has no clock
	// check if we need to update the lerp stuff
	if (s->active == ACTIVE_NETWORK)
		CL_MoveLerpEntityStates(&cl.entities[enumber]);
}

static void EntityStateQW_ReadEntityUpdate(entity_state_t *s, int bits)
{
	int qweffects = 0;
	s->active = ACTIVE_NETWORK;
	s->number = bits & 511;
	bits &= ~511;
	if (bits & QW_U_MOREBITS)
		bits |= MSG_ReadByte();

	// store the QW_U_SOLID bit here?

	if (bits & QW_U_MODEL)
		s->modelindex = MSG_ReadByte();
	if (bits & QW_U_FRAME)
		s->frame = MSG_ReadByte();
	if (bits & QW_U_COLORMAP)
		s->colormap = MSG_ReadByte();
	if (bits & QW_U_SKIN)
		s->skin = MSG_ReadByte();
	if (bits & QW_U_EFFECTS)
		QW_TranslateEffects(s, qweffects = MSG_ReadByte());
	if (bits & QW_U_ORIGIN1)
		s->origin[0] = MSG_ReadCoord13i();
	if (bits & QW_U_ANGLE1)
		s->angles[0] = MSG_ReadAngle8i();
	if (bits & QW_U_ORIGIN2)
		s->origin[1] = MSG_ReadCoord13i();
	if (bits & QW_U_ANGLE2)
		s->angles[1] = MSG_ReadAngle8i();
	if (bits & QW_U_ORIGIN3)
		s->origin[2] = MSG_ReadCoord13i();
	if (bits & QW_U_ANGLE3)
		s->angles[2] = MSG_ReadAngle8i();

	if (developer_networkentities.integer >= 2)
	{
		Con_Printf("ReadFields e%i", s->number);
		if (bits & QW_U_MODEL)
			Con_Printf(" U_MODEL %i", s->modelindex);
		if (bits & QW_U_FRAME)
			Con_Printf(" U_FRAME %i", s->frame);
		if (bits & QW_U_COLORMAP)
			Con_Printf(" U_COLORMAP %i", s->colormap);
		if (bits & QW_U_SKIN)
			Con_Printf(" U_SKIN %i", s->skin);
		if (bits & QW_U_EFFECTS)
			Con_Printf(" U_EFFECTS %i", qweffects);
		if (bits & QW_U_ORIGIN1)
			Con_Printf(" U_ORIGIN1 %f", s->origin[0]);
		if (bits & QW_U_ANGLE1)
			Con_Printf(" U_ANGLE1 %f", s->angles[0]);
		if (bits & QW_U_ORIGIN2)
			Con_Printf(" U_ORIGIN2 %f", s->origin[1]);
		if (bits & QW_U_ANGLE2)
			Con_Printf(" U_ANGLE2 %f", s->angles[1]);
		if (bits & QW_U_ORIGIN3)
			Con_Printf(" U_ORIGIN3 %f", s->origin[2]);
		if (bits & QW_U_ANGLE3)
			Con_Printf(" U_ANGLE3 %f", s->angles[2]);
		if (bits & QW_U_SOLID)
			Con_Printf(" U_SOLID");
		Con_Print("\n");
	}
}

entityframeqw_database_t *EntityFrameQW_AllocDatabase(mempool_t *pool)
{
	entityframeqw_database_t *d;
	d = (entityframeqw_database_t *)Mem_Alloc(pool, sizeof(*d));
	return d;
}

void EntityFrameQW_FreeDatabase(entityframeqw_database_t *d)
{
	Mem_Free(d);
}

void EntityFrameQW_CL_ReadFrame(qboolean delta)
{
	qboolean invalid = false;
	int number, oldsnapindex, newsnapindex, oldindex, newindex, oldnum, newnum;
	entity_t *ent;
	entityframeqw_database_t *d;
	entityframeqw_snapshot_t *oldsnap, *newsnap;

	if (!cl.entitydatabaseqw)
		cl.entitydatabaseqw = EntityFrameQW_AllocDatabase(cls.levelmempool);
	d = cl.entitydatabaseqw;

	// there is no cls.netcon in demos, so this reading code can't access
	// cls.netcon-> at all...  so cls.qw_incoming_sequence and
	// cls.qw_outgoing_sequence are updated every time the corresponding
	// cls.netcon->qw. variables are updated
	// read the number of this frame to echo back in next input packet
	cl.qw_validsequence = cls.qw_incoming_sequence;
	newsnapindex = cl.qw_validsequence & QW_UPDATE_MASK;
	newsnap = d->snapshot + newsnapindex;
	memset(newsnap, 0, sizeof(*newsnap));
	oldsnapindex = -1;
	oldsnap = NULL;
	if (delta)
	{
		number = MSG_ReadByte();
		oldsnapindex = cl.qw_deltasequence[newsnapindex];
		if ((number & QW_UPDATE_MASK) != (oldsnapindex & QW_UPDATE_MASK))
			Con_DPrintf("WARNING: from mismatch\n");
		if (oldsnapindex != -1)
		{
			if (cls.qw_outgoing_sequence - oldsnapindex >= QW_UPDATE_BACKUP-1)
			{
				Con_DPrintf("delta update too old\n");
				newsnap->invalid = invalid = true; // too old
				delta = false;
			}
			oldsnap = d->snapshot + (oldsnapindex & QW_UPDATE_MASK);
		}
		else
			delta = false;
	}

	// if we can't decode this frame properly, report that to the server
	if (invalid)
		cl.qw_validsequence = 0;

	// read entity numbers until we find a 0x0000
	// (which would be an empty update on world entity, but is actually a terminator)
	newsnap->num_entities = 0;
	oldindex = 0;
	for (;;)
	{
		int word = (unsigned short)MSG_ReadShort();
		if (msg_badread)
			return; // just return, the main parser will print an error
		newnum = word == 0 ? 512 : (word & 511);
		oldnum = delta ? (oldindex >= oldsnap->num_entities ? 9999 : oldsnap->entities[oldindex].number) : 9999;

		// copy unmodified oldsnap entities
		while (newnum > oldnum) // delta only
		{
			if (developer_networkentities.integer >= 2)
				Con_Printf("copy %i\n", oldnum);
			// copy one of the old entities
			if (newsnap->num_entities >= QW_MAX_PACKET_ENTITIES)
				Host_Error("EntityFrameQW_CL_ReadFrame: newsnap->num_entities == MAX_PACKETENTITIES");
			newsnap->entities[newsnap->num_entities] = oldsnap->entities[oldindex++];
			newsnap->num_entities++;
			oldnum = oldindex >= oldsnap->num_entities ? 9999 : oldsnap->entities[oldindex].number;
		}

		if (word == 0)
			break;

		if (developer_networkentities.integer >= 2)
		{
			if (word & QW_U_REMOVE)
				Con_Printf("remove %i\n", newnum);
			else if (newnum == oldnum)
				Con_Printf("delta %i\n", newnum);
			else
				Con_Printf("baseline %i\n", newnum);
		}

		if (word & QW_U_REMOVE)
		{
			if (newnum != oldnum && !delta && !invalid)
			{
				cl.qw_validsequence = 0;
				Con_Printf("WARNING: U_REMOVE %i on full update\n", newnum);
			}
		}
		else
		{
			if (newsnap->num_entities >= QW_MAX_PACKET_ENTITIES)
				Host_Error("EntityFrameQW_CL_ReadFrame: newsnap->num_entities == MAX_PACKETENTITIES");
			newsnap->entities[newsnap->num_entities] = (newnum == oldnum) ? oldsnap->entities[oldindex] : cl.entities[newnum].state_baseline;
			EntityStateQW_ReadEntityUpdate(newsnap->entities + newsnap->num_entities, word);
			newsnap->num_entities++;
		}

		if (newnum == oldnum)
			oldindex++;
	}

	// expand cl.num_entities to include every entity we've seen this game
	newnum = newsnap->num_entities ? newsnap->entities[newsnap->num_entities - 1].number : 1;
	if (cl.num_entities <= newnum)
	{
		cl.num_entities = newnum + 1;
		if (cl.max_entities < newnum + 1)
			CL_ExpandEntities(newnum);
	}

	// now update the non-player entities from the snapshot states
	number = cl.maxclients + 1;
	for (newindex = 0;;newindex++)
	{
		newnum = newindex >= newsnap->num_entities ? cl.num_entities : newsnap->entities[newindex].number;
		// kill any missing entities
		for (;number < newnum;number++)
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
		ent->state_current = newsnap->entities[newindex];
		ent->state_current.time = cl.mtime[0];
		CL_MoveLerpEntityStates(ent);
		// the entity lives again...
		cl.entities_active[number] = true;
		number++;
	}
}
