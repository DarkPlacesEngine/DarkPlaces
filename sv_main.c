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
// sv_main.c -- server main program

#include "quakedef.h"

// select which protocol to host, by name
// this is named the same as PROTOCOL_DARKPLACES6 for example, minus the PROTOCOL_ prefix
cvar_t sv_protocolname = {0, "sv_protocolname", "DARKPLACES6"};
cvar_t sv_ratelimitlocalplayer = {0, "sv_ratelimitlocalplayer", "0"};
cvar_t sv_maxrate = {CVAR_SAVE | CVAR_NOTIFY, "sv_maxrate", "10000"};

static cvar_t sv_cullentities_pvs = {0, "sv_cullentities_pvs", "1"}; // fast but loose
static cvar_t sv_cullentities_trace = {0, "sv_cullentities_trace", "0"}; // tends to get false negatives, uses a timeout to keep entities visible a short time after becoming hidden
static cvar_t sv_cullentities_stats = {0, "sv_cullentities_stats", "0"};
static cvar_t sv_entpatch = {0, "sv_entpatch", "1"};

extern cvar_t sys_ticrate;

cvar_t sv_gameplayfix_grenadebouncedownslopes = {0, "sv_gameplayfix_grenadebouncedownslopes", "1"};
cvar_t sv_gameplayfix_noairborncorpse = {0, "sv_gameplayfix_noairborncorpse", "1"};
cvar_t sv_gameplayfix_stepdown = {0, "sv_gameplayfix_stepdown", "1"};
cvar_t sv_gameplayfix_stepwhilejumping = {0, "sv_gameplayfix_stepwhilejumping", "1"};
cvar_t sv_gameplayfix_swiminbmodels = {0, "sv_gameplayfix_swiminbmodels", "1"};
cvar_t sv_gameplayfix_setmodelrealbox = {0, "sv_gameplayfix_setmodelrealbox", "1"};

cvar_t sv_progs = {0, "sv_progs", "progs.dat" };

server_t sv;
server_static_t svs;

mempool_t *sv_mempool = NULL;

//============================================================================

extern void SV_Phys_Init (void);
extern void SV_World_Init (void);
static void SV_SaveEntFile_f(void);

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	Cmd_AddCommand("sv_saveentfile", SV_SaveEntFile_f);
	Cvar_RegisterVariable (&sv_maxvelocity);
	Cvar_RegisterVariable (&sv_gravity);
	Cvar_RegisterVariable (&sv_friction);
	Cvar_RegisterVariable (&sv_edgefriction);
	Cvar_RegisterVariable (&sv_stopspeed);
	Cvar_RegisterVariable (&sv_maxspeed);
	Cvar_RegisterVariable (&sv_accelerate);
	Cvar_RegisterVariable (&sv_idealpitchscale);
	Cvar_RegisterVariable (&sv_aim);
	Cvar_RegisterVariable (&sv_nostep);
	Cvar_RegisterVariable (&sv_deltacompress);
	Cvar_RegisterVariable (&sv_cullentities_pvs);
	Cvar_RegisterVariable (&sv_cullentities_trace);
	Cvar_RegisterVariable (&sv_cullentities_stats);
	Cvar_RegisterVariable (&sv_entpatch);
	Cvar_RegisterVariable (&sv_gameplayfix_grenadebouncedownslopes);
	Cvar_RegisterVariable (&sv_gameplayfix_noairborncorpse);
	Cvar_RegisterVariable (&sv_gameplayfix_stepdown);
	Cvar_RegisterVariable (&sv_gameplayfix_stepwhilejumping);
	Cvar_RegisterVariable (&sv_gameplayfix_swiminbmodels);
	Cvar_RegisterVariable (&sv_gameplayfix_setmodelrealbox);
	Cvar_RegisterVariable (&sv_protocolname);
	Cvar_RegisterVariable (&sv_ratelimitlocalplayer);
	Cvar_RegisterVariable (&sv_maxrate);
	Cvar_RegisterVariable (&sv_progs);

	SV_Phys_Init();
	SV_World_Init();

	sv_mempool = Mem_AllocPool("server", 0, NULL);
}

static void SV_SaveEntFile_f(void)
{
	char basename[MAX_QPATH];
	if (!sv.active || !sv.worldmodel)
	{
		Con_Print("Not running a server\n");
		return;
	}
	FS_StripExtension(sv.worldmodel->name, basename, sizeof(basename));
	FS_WriteFile(va("%s.ent", basename), sv.worldmodel->brush.entities, strlen(sv.worldmodel->brush.entities));
}


/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	int		i, v;

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-18)
		return;
	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
	MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
	MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.datagram, v);
	}
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
}

/*
==================
SV_StartEffect

Make sure the event gets sent to all clients
==================
*/
void SV_StartEffect (vec3_t org, int modelindex, int startframe, int framecount, int framerate)
{
	if (modelindex >= 256 || startframe >= 256)
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-19)
			return;
		MSG_WriteByte (&sv.datagram, svc_effect2);
		MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
		MSG_WriteShort (&sv.datagram, modelindex);
		MSG_WriteShort (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
	else
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-17)
			return;
		MSG_WriteByte (&sv.datagram, svc_effect);
		MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
		MSG_WriteByte (&sv.datagram, modelindex);
		MSG_WriteByte (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume, float attenuation)
{
	int sound_num, field_mask, i, ent;

	if (volume < 0 || volume > 255)
		Host_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Host_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Host_Error ("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-21)
		return;

// find precache number for sound
	sound_num = SV_SoundIndex(sample, 1);
	if (!sound_num)
		return;

	ent = NUM_FOR_EDICT(entity);

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;
	if (ent >= 8192)
		field_mask |= SND_LARGEENTITY;
	if (sound_num >= 256 || channel >= 8)
		field_mask |= SND_LARGESOUND;

// directed messages go only to the entity they are targeted on
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, attenuation*64);
	if (field_mask & SND_LARGEENTITY)
	{
		MSG_WriteShort (&sv.datagram, ent);
		MSG_WriteByte (&sv.datagram, channel);
	}
	else
		MSG_WriteShort (&sv.datagram, (ent<<3) | channel);
	if (field_mask & SND_LARGESOUND)
		MSG_WriteShort (&sv.datagram, sound_num);
	else
		MSG_WriteByte (&sv.datagram, sound_num);
	for (i = 0;i < 3;i++)
		MSG_WriteCoord (&sv.datagram, entity->v->origin[i]+0.5*(entity->v->mins[i]+entity->v->maxs[i]), sv.protocol);
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
	int i;
	char message[128];

	// edicts get reallocated on level changes, so we need to update it here
	client->edict = EDICT_NUM((client - svs.clients) + 1);

	// if client is a botclient coming from a level change, we need to set up
	// client info that normally requires networking
	if (!client->netconnection)
	{
		// set up the edict
		ED_ClearEdict(client->edict);

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(client->edict);
		PR_ExecuteProgram (pr_global_struct->ClientConnect, "QC function ClientConnect is missing");
		PR_ExecuteProgram (pr_global_struct->PutClientInServer, "QC function PutClientInServer is missing");
		host_client->spawned = true;
		return;
	}

	// LordHavoc: clear entityframe tracking
	client->latestframenum = 0;

	if (client->entitydatabase)
		EntityFrame_FreeDatabase(client->entitydatabase);
	if (client->entitydatabase4)
		EntityFrame4_FreeDatabase(client->entitydatabase4);
	if (client->entitydatabase5)
		EntityFrame5_FreeDatabase(client->entitydatabase5);

	if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3)
		client->entitydatabase = EntityFrame_AllocDatabase(sv_mempool);
	if (sv.protocol == PROTOCOL_DARKPLACES4)
		client->entitydatabase4 = EntityFrame4_AllocDatabase(sv_mempool);
	if (sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
		client->entitydatabase5 = EntityFrame5_AllocDatabase(sv_mempool);

	MSG_WriteByte (&client->message, svc_print);
	dpsnprintf (message, sizeof (message), "\002\nServer: %s build %s (progs %i crc)", gamename, buildstring, pr_crc);
	MSG_WriteString (&client->message,message);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, sv.protocol);
	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop.integer && deathmatch.integer)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	MSG_WriteString (&client->message,PR_GetString(sv.edicts->v->message));

	for (i = 1;i < MAX_MODELS && sv.model_precache[i][0];i++)
		MSG_WriteString (&client->message, sv.model_precache[i]);
	MSG_WriteByte (&client->message, 0);

	for (i = 1;i < MAX_SOUNDS && sv.sound_precache[i][0];i++)
		MSG_WriteString (&client->message, sv.sound_precache[i]);
	MSG_WriteByte (&client->message, 0);

// send music
	MSG_WriteByte (&client->message, svc_cdtrack);
	MSG_WriteByte (&client->message, sv.edicts->v->sounds);
	MSG_WriteByte (&client->message, sv.edicts->v->sounds);

// set view
	MSG_WriteByte (&client->message, svc_setview);
	MSG_WriteShort (&client->message, NUM_FOR_EDICT(client->edict));

	MSG_WriteByte (&client->message, svc_signonnum);
	MSG_WriteByte (&client->message, 1);

	client->sendsignon = true;
	client->spawned = false;		// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum, netconn_t *netconnection)
{
	client_t		*client;
	int				i;
	float			spawn_parms[NUM_SPAWN_PARMS];

	client = svs.clients + clientnum;

// set up the client_t
	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
	memset (client, 0, sizeof(*client));
	client->active = true;
	client->netconnection = netconnection;

	Con_DPrintf("Client %s connected\n", client->netconnection ? client->netconnection->address : "botclient");

	strcpy(client->name, "unconnected");
	strcpy(client->old_name, "unconnected");
	client->spawned = false;
	client->edict = EDICT_NUM(clientnum+1);
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof(client->msgbuf);
	client->message.allowoverflow = true;		// we can catch it
	// updated by receiving "rate" command from client
	client->rate = NET_MINRATE;
	// no limits for local player
	if (client->netconnection && LHNETADDRESS_GetAddressType(&client->netconnection->peeraddress) == LHNETADDRESSTYPE_LOOP)
		client->rate = 1000000000;
	client->connecttime = realtime;

	if (sv.loadgame)
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	else
	{
		// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (pr_global_struct->SetNewParms, "QC function SetNewParms is missing");
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
	}

	// don't call SendServerinfo for a fresh botclient because its fields have
	// not been set up by the qc yet
	if (client->netconnection)
		SV_SendServerinfo (client);
	else
		client->spawned = true;
}


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram

==================
*/
void SV_ClearDatagram (void)
{
	SZ_Clear (&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int sv_writeentitiestoclient_pvsbytes;
qbyte sv_writeentitiestoclient_pvs[MAX_MAP_LEAFS/8];

static int numsendentities;
static entity_state_t sendentities[MAX_EDICTS];
static entity_state_t *sendentitiesindex[MAX_EDICTS];

void SV_PrepareEntitiesForSending(void)
{
	int e, i;
	float f;
	edict_t *ent;
	eval_t *val;
	entity_state_t cs;
	// send all entities that touch the pvs
	numsendentities = 0;
	sendentitiesindex[0] = NULL;
	for (e = 1, ent = NEXT_EDICT(sv.edicts);e < sv.num_edicts;e++, ent = NEXT_EDICT(ent))
	{
		sendentitiesindex[e] = NULL;
		if (ent->e->free)
			continue;

		cs = defaultstate;
		cs.active = true;
		cs.number = e;
		VectorCopy(ent->v->origin, cs.origin);
		VectorCopy(ent->v->angles, cs.angles);
		cs.flags = 0;
		cs.effects = (int)ent->v->effects;
		cs.colormap = (qbyte)ent->v->colormap;
		cs.skin = (qbyte)ent->v->skin;
		cs.frame = (qbyte)ent->v->frame;
		cs.viewmodelforclient = GETEDICTFIELDVALUE(ent, eval_viewmodelforclient)->edict;
		cs.exteriormodelforclient = GETEDICTFIELDVALUE(ent, eval_exteriormodeltoclient)->edict;
		cs.nodrawtoclient = GETEDICTFIELDVALUE(ent, eval_nodrawtoclient)->edict;
		cs.drawonlytoclient = GETEDICTFIELDVALUE(ent, eval_drawonlytoclient)->edict;
		cs.tagentity = GETEDICTFIELDVALUE(ent, eval_tag_entity)->edict;
		cs.tagindex = (qbyte)GETEDICTFIELDVALUE(ent, eval_tag_index)->_float;
		i = (int)(GETEDICTFIELDVALUE(ent, eval_glow_size)->_float * 0.25f);
		cs.glowsize = (qbyte)bound(0, i, 255);
		if (GETEDICTFIELDVALUE(ent, eval_glow_trail)->_float)
			cs.flags |= RENDER_GLOWTRAIL;

		// don't need to init cs.colormod because the defaultstate did that for us
		//cs.colormod[0] = cs.colormod[1] = cs.colormod[2] = 32;
		val = GETEDICTFIELDVALUE(ent, eval_colormod);
		if (val->vector[0] || val->vector[1] || val->vector[2])
		{
			i = val->vector[0] * 32.0f;cs.colormod[0] = bound(0, i, 255);
			i = val->vector[1] * 32.0f;cs.colormod[1] = bound(0, i, 255);
			i = val->vector[2] * 32.0f;cs.colormod[2] = bound(0, i, 255);
		}

		cs.modelindex = 0;
		i = (int)ent->v->modelindex;
		if (i >= 1 && i < MAX_MODELS && *PR_GetString(ent->v->model))
			cs.modelindex = i;

		cs.alpha = 255;
		f = (GETEDICTFIELDVALUE(ent, eval_alpha)->_float * 255.0f);
		if (f)
		{
			i = (int)f;
			cs.alpha = (qbyte)bound(0, i, 255);
		}
		// halflife
		f = (GETEDICTFIELDVALUE(ent, eval_renderamt)->_float);
		if (f)
		{
			i = (int)f;
			cs.alpha = (qbyte)bound(0, i, 255);
		}

		cs.scale = 16;
		f = (GETEDICTFIELDVALUE(ent, eval_scale)->_float * 16.0f);
		if (f)
		{
			i = (int)f;
			cs.scale = (qbyte)bound(0, i, 255);
		}

		cs.glowcolor = 254;
		f = (GETEDICTFIELDVALUE(ent, eval_glow_color)->_float);
		if (f)
			cs.glowcolor = (int)f;

		if (GETEDICTFIELDVALUE(ent, eval_fullbright)->_float)
			cs.effects |= EF_FULLBRIGHT;

		if (ent->v->movetype == MOVETYPE_STEP)
			cs.flags |= RENDER_STEP;
		if ((cs.effects & EF_LOWPRECISION) && cs.origin[0] >= -32768 && cs.origin[1] >= -32768 && cs.origin[2] >= -32768 && cs.origin[0] <= 32767 && cs.origin[1] <= 32767 && cs.origin[2] <= 32767)
			cs.flags |= RENDER_LOWPRECISION;
		if (ent->v->colormap >= 1024)
			cs.flags |= RENDER_COLORMAPPED;
		if (cs.viewmodelforclient)
			cs.flags |= RENDER_VIEWMODEL; // show relative to the view

		f = GETEDICTFIELDVALUE(ent, eval_color)->vector[0]*256;
		cs.light[0] = (unsigned short)bound(0, f, 65535);
		f = GETEDICTFIELDVALUE(ent, eval_color)->vector[1]*256;
		cs.light[1] = (unsigned short)bound(0, f, 65535);
		f = GETEDICTFIELDVALUE(ent, eval_color)->vector[2]*256;
		cs.light[2] = (unsigned short)bound(0, f, 65535);
		f = GETEDICTFIELDVALUE(ent, eval_light_lev)->_float;
		cs.light[3] = (unsigned short)bound(0, f, 65535);
		cs.lightstyle = (qbyte)GETEDICTFIELDVALUE(ent, eval_style)->_float;
		cs.lightpflags = (qbyte)GETEDICTFIELDVALUE(ent, eval_pflags)->_float;

		if (gamemode == GAME_TENEBRAE)
		{
			// tenebrae's EF_FULLDYNAMIC conflicts with Q2's EF_NODRAW
			if (cs.effects & 16)
			{
				cs.effects &= ~16;
				cs.lightpflags |= PFLAGS_FULLDYNAMIC;
			}
			// tenebrae's EF_GREEN conflicts with DP's EF_ADDITIVE
			if (cs.effects & 32)
			{
				cs.effects &= ~32;
				cs.light[0] = 0.2;
				cs.light[1] = 1;
				cs.light[2] = 0.2;
				cs.light[3] = 200;
				cs.lightpflags |= PFLAGS_FULLDYNAMIC;
			}
		}

		cs.specialvisibilityradius = 0;
		if (cs.lightpflags & PFLAGS_FULLDYNAMIC)
			cs.specialvisibilityradius = max(cs.specialvisibilityradius, cs.light[3]);
		if (cs.glowsize)
			cs.specialvisibilityradius = max(cs.specialvisibilityradius, cs.glowsize * 4);
		if (cs.flags & RENDER_GLOWTRAIL)
			cs.specialvisibilityradius = max(cs.specialvisibilityradius, 100);
		if (cs.effects & (EF_BRIGHTFIELD | EF_MUZZLEFLASH | EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_RED | EF_BLUE | EF_FLAME | EF_STARDUST))
		{
			if (cs.effects & EF_BRIGHTFIELD)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 80);
			if (cs.effects & EF_MUZZLEFLASH)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 100);
			if (cs.effects & EF_BRIGHTLIGHT)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 400);
			if (cs.effects & EF_DIMLIGHT)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 200);
			if (cs.effects & EF_RED)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 200);
			if (cs.effects & EF_BLUE)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 200);
			if (cs.effects & EF_FLAME)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 250);
			if (cs.effects & EF_STARDUST)
				cs.specialvisibilityradius = max(cs.specialvisibilityradius, 100);
		}

		if (numsendentities >= MAX_EDICTS)
			continue;
		// we can omit invisible entities with no effects that are not clients
		// LordHavoc: this could kill tags attached to an invisible entity, I
		// just hope we never have to support that case
		if (cs.number > svs.maxclients && ((cs.effects & EF_NODRAW) || (!cs.modelindex && !cs.specialvisibilityradius)))
			continue;
		sendentitiesindex[e] = sendentities + numsendentities;
		sendentities[numsendentities++] = cs;
	}
}

static int sententitiesmark = 0;
static int sententities[MAX_EDICTS];
static int sententitiesconsideration[MAX_EDICTS];
static int sv_writeentitiestoclient_culled_pvs;
static int sv_writeentitiestoclient_culled_trace;
static int sv_writeentitiestoclient_visibleentities;
static int sv_writeentitiestoclient_totalentities;
//static entity_frame_t sv_writeentitiestoclient_entityframe;
static int sv_writeentitiestoclient_clentnum;
static vec3_t sv_writeentitiestoclient_testeye;
static client_t *sv_writeentitiestoclient_client;

void SV_MarkWriteEntityStateToClient(entity_state_t *s)
{
	int isbmodel;
	vec3_t entmins, entmaxs, lightmins, lightmaxs, testorigin;
	model_t *model;
	trace_t trace;
	if (sententitiesconsideration[s->number] == sententitiesmark)
		return;
	sententitiesconsideration[s->number] = sententitiesmark;
	// viewmodels don't have visibility checking
	if (s->viewmodelforclient)
	{
		if (s->viewmodelforclient != sv_writeentitiestoclient_clentnum)
			return;
	}
	// never reject player
	else if (s->number != sv_writeentitiestoclient_clentnum)
	{
		// check various rejection conditions
		if (s->nodrawtoclient == sv_writeentitiestoclient_clentnum)
			return;
		if (s->drawonlytoclient && s->drawonlytoclient != sv_writeentitiestoclient_clentnum)
			return;
		if (s->effects & EF_NODRAW)
			return;
		// LordHavoc: only send entities with a model or important effects
		if (!s->modelindex && s->specialvisibilityradius == 0)
			return;
		if (s->tagentity)
		{
			// tag attached entities simply check their parent
			if (!sendentitiesindex[s->tagentity])
				return;
			SV_MarkWriteEntityStateToClient(sendentitiesindex[s->tagentity]);
			if (sententities[s->tagentity] != sententitiesmark)
				return;
		}
		// skip invalid modelindexes to avoid crashes
		else if (s->modelindex >= MAX_MODELS)
			return;
		// always send world submodels, they don't generate much traffic
		// except in PROTOCOL_QUAKE where they hog bandwidth like crazy
		else if (!(s->effects & EF_NODEPTHTEST) && (!(isbmodel = (model = sv.models[s->modelindex]) != NULL && model->name[0] == '*') || sv.protocol == PROTOCOL_QUAKE))
		{
			Mod_CheckLoaded(model);
			// entity has survived every check so far, check if visible
			// enlarged box to account for prediction (not that there is
			// any currently, but still helps the 'run into a room and
			// watch items pop up' problem)
			entmins[0] = s->origin[0] - 32.0f;
			entmins[1] = s->origin[1] - 32.0f;
			entmins[2] = s->origin[2] - 32.0f;
			entmaxs[0] = s->origin[0] + 32.0f;
			entmaxs[1] = s->origin[1] + 32.0f;
			entmaxs[2] = s->origin[2] + 32.0f;
			// using the model's bounding box to ensure things are visible regardless of their physics box
			if (model)
			{
				if (s->angles[0] || s->angles[2]) // pitch and roll
				{
					VectorAdd(entmins, model->rotatedmins, entmins);
					VectorAdd(entmaxs, model->rotatedmaxs, entmaxs);
				}
				else if (s->angles[1])
				{
					VectorAdd(entmins, model->yawmins, entmins);
					VectorAdd(entmaxs, model->yawmaxs, entmaxs);
				}
				else
				{
					VectorAdd(entmins, model->normalmins, entmins);
					VectorAdd(entmaxs, model->normalmaxs, entmaxs);
				}
			}
			lightmins[0] = min(entmins[0], s->origin[0] - s->specialvisibilityradius);
			lightmins[1] = min(entmins[1], s->origin[1] - s->specialvisibilityradius);
			lightmins[2] = min(entmins[2], s->origin[2] - s->specialvisibilityradius);
			lightmaxs[0] = max(entmaxs[0], s->origin[0] + s->specialvisibilityradius);
			lightmaxs[1] = max(entmaxs[1], s->origin[1] + s->specialvisibilityradius);
			lightmaxs[2] = max(entmaxs[2], s->origin[2] + s->specialvisibilityradius);
			sv_writeentitiestoclient_totalentities++;
			// if not touching a visible leaf
			if (sv_cullentities_pvs.integer && sv_writeentitiestoclient_pvsbytes && sv.worldmodel && sv.worldmodel->brush.BoxTouchingPVS && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, sv_writeentitiestoclient_pvs, lightmins, lightmaxs))
			{
				sv_writeentitiestoclient_culled_pvs++;
				return;
			}
			// or not seen by random tracelines
			if (sv_cullentities_trace.integer && !isbmodel)
			{
				// LordHavoc: test center first
				testorigin[0] = (entmins[0] + entmaxs[0]) * 0.5f;
				testorigin[1] = (entmins[1] + entmaxs[1]) * 0.5f;
				testorigin[2] = (entmins[2] + entmaxs[2]) * 0.5f;
				sv.worldmodel->TraceBox(sv.worldmodel, 0, &trace, sv_writeentitiestoclient_testeye, sv_writeentitiestoclient_testeye, testorigin, testorigin, SUPERCONTENTS_SOLID);
				if (trace.fraction == 1 || BoxesOverlap(trace.endpos, trace.endpos, entmins, entmaxs))
					sv_writeentitiestoclient_client->visibletime[s->number] = realtime + 1;
				else
				{
					// LordHavoc: test random offsets, to maximize chance of detection
					testorigin[0] = lhrandom(entmins[0], entmaxs[0]);
					testorigin[1] = lhrandom(entmins[1], entmaxs[1]);
					testorigin[2] = lhrandom(entmins[2], entmaxs[2]);
					sv.worldmodel->TraceBox(sv.worldmodel, 0, &trace, sv_writeentitiestoclient_testeye, sv_writeentitiestoclient_testeye, testorigin, testorigin, SUPERCONTENTS_SOLID);
					if (trace.fraction == 1 || BoxesOverlap(trace.endpos, trace.endpos, entmins, entmaxs))
						sv_writeentitiestoclient_client->visibletime[s->number] = realtime + 1;
					else
					{
						if (s->specialvisibilityradius)
						{
							// LordHavoc: test random offsets, to maximize chance of detection
							testorigin[0] = lhrandom(lightmins[0], lightmaxs[0]);
							testorigin[1] = lhrandom(lightmins[1], lightmaxs[1]);
							testorigin[2] = lhrandom(lightmins[2], lightmaxs[2]);
							sv.worldmodel->TraceBox(sv.worldmodel, 0, &trace, sv_writeentitiestoclient_testeye, sv_writeentitiestoclient_testeye, testorigin, testorigin, SUPERCONTENTS_SOLID);
							if (trace.fraction == 1 || BoxesOverlap(trace.endpos, trace.endpos, entmins, entmaxs))
								sv_writeentitiestoclient_client->visibletime[s->number] = realtime + 1;
						}
					}
				}
				if (realtime > sv_writeentitiestoclient_client->visibletime[s->number])
				{
					sv_writeentitiestoclient_culled_trace++;
					return;
				}
			}
			sv_writeentitiestoclient_visibleentities++;
		}
	}
	// this just marks it for sending
	// FIXME: it would be more efficient to send here, but the entity
	// compressor isn't that flexible
	sententities[s->number] = sententitiesmark;
}

entity_state_t sendstates[MAX_EDICTS];

void SV_WriteEntitiesToClient(client_t *client, edict_t *clent, sizebuf_t *msg, int *stats)
{
	int i, numsendstates;
	entity_state_t *s;

	// if there isn't enough space to accomplish anything, skip it
	if (msg->cursize + 25 > msg->maxsize)
		return;

	sv_writeentitiestoclient_client = client;

	sv_writeentitiestoclient_culled_pvs = 0;
	sv_writeentitiestoclient_culled_trace = 0;
	sv_writeentitiestoclient_visibleentities = 0;
	sv_writeentitiestoclient_totalentities = 0;

	Mod_CheckLoaded(sv.worldmodel);

// find the client's PVS
	// the real place being tested from
	VectorAdd(clent->v->origin, clent->v->view_ofs, sv_writeentitiestoclient_testeye);
	sv_writeentitiestoclient_pvsbytes = 0;
	if (sv.worldmodel && sv.worldmodel->brush.FatPVS)
		sv_writeentitiestoclient_pvsbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, sv_writeentitiestoclient_testeye, 8, sv_writeentitiestoclient_pvs, sizeof(sv_writeentitiestoclient_pvs));

	sv_writeentitiestoclient_clentnum = EDICT_TO_PROG(clent); // LordHavoc: for comparison purposes

	sententitiesmark++;

	for (i = 0;i < numsendentities;i++)
		SV_MarkWriteEntityStateToClient(sendentities + i);

	numsendstates = 0;
	for (i = 0;i < numsendentities;i++)
	{
		if (sententities[sendentities[i].number] == sententitiesmark)
		{
			s = &sendstates[numsendstates++];
			*s = sendentities[i];
			if (s->exteriormodelforclient && s->exteriormodelforclient == sv_writeentitiestoclient_clentnum)
				s->flags |= RENDER_EXTERIORMODEL;
		}
	}

	if (sv_cullentities_stats.integer)
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d trace\n", client->name, sv_writeentitiestoclient_totalentities, sv_writeentitiestoclient_visibleentities, sv_writeentitiestoclient_culled_pvs + sv_writeentitiestoclient_culled_trace, sv_writeentitiestoclient_culled_pvs, sv_writeentitiestoclient_culled_trace);

	if (client->entitydatabase5)
		EntityFrame5_WriteFrame(msg, client->entitydatabase5, numsendstates, sendstates, client - svs.clients + 1, stats);
	else if (client->entitydatabase4)
		EntityFrame4_WriteFrame(msg, client->entitydatabase4, numsendstates, sendstates);
	else if (client->entitydatabase)
		EntityFrame_WriteFrame(msg, client->entitydatabase, numsendstates, sendstates, client - svs.clients + 1);
	else
		EntityFrameQuake_WriteFrame(msg, numsendstates, sendstates);
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts (void)
{
	int		e;
	edict_t	*ent;

	ent = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
		ent->v->effects = (int)ent->v->effects & ~EF_MUZZLEFLASH;
}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (client_t *client, edict_t *ent, sizebuf_t *msg, int *stats)
{
	int		bits;
	int		i;
	edict_t	*other;
	int		items;
	eval_t	*val;
	vec3_t	punchvector;
	qbyte	viewzoom;
	int		weaponmodelindex;

//
// send a damage message
//
	if (ent->v->dmg_take || ent->v->dmg_save)
	{
		other = PROG_TO_EDICT(ent->v->dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v->dmg_save);
		MSG_WriteByte (msg, ent->v->dmg_take);
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, other->v->origin[i] + 0.5*(other->v->mins[i] + other->v->maxs[i]), sv.protocol);

		ent->v->dmg_take = 0;
		ent->v->dmg_save = 0;
	}

//
// send the current viewpos offset from the view entity
//
	SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet.  Oh well.
	if ( ent->v->fixangle )
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			MSG_WriteAngle (msg, ent->v->angles[i], sv.protocol);
		ent->v->fixangle = 0;
	}

	// stuff the sigil bits into the high bits of items for sbar, or else
	// mix in items2
	val = GETEDICTFIELDVALUE(ent, eval_items2);
	if (val)
		items = (int)ent->v->items | ((int)val->_float << 23);
	else
		items = (int)ent->v->items | ((int)pr_global_struct->serverflags << 28);

	VectorClear(punchvector);
	if ((val = GETEDICTFIELDVALUE(ent, eval_punchvector)))
		VectorCopy(val->vector, punchvector);

	weaponmodelindex = SV_ModelIndex(PR_GetString(ent->v->weaponmodel), 1);

	viewzoom = 255;
	if ((val = GETEDICTFIELDVALUE(ent, eval_viewzoom)))
		viewzoom = val->_float * 255.0f;
	if (viewzoom == 0)
		viewzoom = 255;

	bits = 0;

	if ((int)ent->v->flags & FL_ONGROUND)
		bits |= SU_ONGROUND;
	if (ent->v->waterlevel >= 2)
		bits |= SU_INWATER;
	if (ent->v->idealpitch)
		bits |= SU_IDEALPITCH;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->v->punchangle[i])
			bits |= (SU_PUNCH1<<i);
		if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
			if (punchvector[i])
				bits |= (SU_PUNCHVEC1<<i);
		if (ent->v->velocity[i])
			bits |= (SU_VELOCITY1<<i);
	}

	memset(stats, 0, sizeof(int[MAX_CL_STATS]));
	stats[STAT_VIEWHEIGHT] = ent->v->view_ofs[2];
	stats[STAT_ITEMS] = items;
	stats[STAT_WEAPONFRAME] = ent->v->weaponframe;
	stats[STAT_ARMOR] = ent->v->armorvalue;
	stats[STAT_WEAPON] = weaponmodelindex;
	stats[STAT_HEALTH] = ent->v->health;
	stats[STAT_AMMO] = ent->v->currentammo;
	stats[STAT_SHELLS] = ent->v->ammo_shells;
	stats[STAT_NAILS] = ent->v->ammo_nails;
	stats[STAT_ROCKETS] = ent->v->ammo_rockets;
	stats[STAT_CELLS] = ent->v->ammo_cells;
	stats[STAT_ACTIVEWEAPON] = ent->v->weapon;
	stats[STAT_VIEWZOOM] = viewzoom;
	// the QC bumps these itself by sending svc_'s, so we have to keep them
	// zero or they'll be corrected by the engine
	//stats[STAT_TOTALSECRETS] = pr_global_struct->total_secrets;
	//stats[STAT_TOTALMONSTERS] = pr_global_struct->total_monsters;
	//stats[STAT_SECRETS] = pr_global_struct->found_secrets;
	//stats[STAT_MONSTERS] = pr_global_struct->killed_monsters;

	if (sv.protocol != PROTOCOL_DARKPLACES6)
	{
		if (stats[STAT_VIEWHEIGHT] != DEFAULT_VIEWHEIGHT) bits |= SU_VIEWHEIGHT;
		bits |= SU_ITEMS;
		if (stats[STAT_WEAPONFRAME]) bits |= SU_WEAPONFRAME;
		if (stats[STAT_ARMOR]) bits |= SU_ARMOR;
		bits |= SU_WEAPON;
		// FIXME: which protocols support this?  does PROTOCOL_DARKPLACES3 support viewzoom?
		if (sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
			if (viewzoom != 255)
				bits |= SU_VIEWZOOM;
	}

	if (bits >= 65536)
		bits |= SU_EXTEND1;
	if (bits >= 16777216)
		bits |= SU_EXTEND2;

	// send the data
	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);
	if (bits & SU_EXTEND1)
		MSG_WriteByte(msg, bits >> 16);
	if (bits & SU_EXTEND2)
		MSG_WriteByte(msg, bits >> 24);

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, stats[STAT_VIEWHEIGHT]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, ent->v->idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
		{
			if (sv.protocol == PROTOCOL_QUAKE)
				MSG_WriteChar(msg, ent->v->punchangle[i]);
			else if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
				MSG_WriteAngle16i(msg, ent->v->punchangle[i]);
		}
		if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
		{
			if (bits & (SU_PUNCHVEC1<<i))
			{
				if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
					MSG_WriteCoord16i(msg, punchvector[i]);
				else if (sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
					MSG_WriteCoord32f(msg, punchvector[i]);
			}
		}
		if (bits & (SU_VELOCITY1<<i))
		{
			if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteChar(msg, ent->v->velocity[i] * (1.0f / 16.0f));
			else if (sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
				MSG_WriteCoord32f(msg, ent->v->velocity[i]);
		}
	}

	if (bits & SU_ITEMS)
		MSG_WriteLong (msg, stats[STAT_ITEMS]);

	if (sv.protocol == PROTOCOL_DARKPLACES5)
	{
		if (bits & SU_WEAPONFRAME)
			MSG_WriteShort (msg, stats[STAT_WEAPONFRAME]);
		if (bits & SU_ARMOR)
			MSG_WriteShort (msg, stats[STAT_ARMOR]);
		if (bits & SU_WEAPON)
			MSG_WriteShort (msg, stats[STAT_WEAPON]);
		MSG_WriteShort (msg, stats[STAT_HEALTH]);
		MSG_WriteShort (msg, stats[STAT_AMMO]);
		MSG_WriteShort (msg, stats[STAT_SHELLS]);
		MSG_WriteShort (msg, stats[STAT_NAILS]);
		MSG_WriteShort (msg, stats[STAT_ROCKETS]);
		MSG_WriteShort (msg, stats[STAT_CELLS]);
		MSG_WriteShort (msg, stats[STAT_ACTIVEWEAPON]);
		if (bits & SU_VIEWZOOM)
			MSG_WriteShort (msg, min(stats[STAT_VIEWZOOM], 65535));
	}
	else if (sv.protocol != PROTOCOL_DARKPLACES6)
	{
		if (bits & SU_WEAPONFRAME)
			MSG_WriteByte (msg, stats[STAT_WEAPONFRAME]);
		if (bits & SU_ARMOR)
			MSG_WriteByte (msg, stats[STAT_ARMOR]);
		if (bits & SU_WEAPON)
			MSG_WriteByte (msg, stats[STAT_WEAPON]);
		MSG_WriteShort (msg, stats[STAT_HEALTH]);
		MSG_WriteByte (msg, stats[STAT_AMMO]);
		MSG_WriteByte (msg, stats[STAT_SHELLS]);
		MSG_WriteByte (msg, stats[STAT_NAILS]);
		MSG_WriteByte (msg, stats[STAT_ROCKETS]);
		MSG_WriteByte (msg, stats[STAT_CELLS]);
		if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE || gamemode == GAME_NEXUIZ)
		{
			for (i = 0;i < 32;i++)
				if (stats[STAT_WEAPON] & (1<<i))
					break;
			MSG_WriteByte (msg, i);
		}
		else
			MSG_WriteByte (msg, stats[STAT_WEAPON]);
		if (bits & SU_VIEWZOOM)
		{
			if (sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteByte (msg, min(stats[STAT_VIEWZOOM], 255));
			else if (sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
				MSG_WriteShort (msg, min(stats[STAT_VIEWZOOM], 65535));
		}
	}
}

/*
=======================
SV_SendClientDatagram
=======================
*/
static qbyte sv_sendclientdatagram_buf[NET_MAXMESSAGE]; // FIXME?
qboolean SV_SendClientDatagram (client_t *client)
{
	int rate, maxrate, maxsize, maxsize2;
	sizebuf_t msg;
	int stats[MAX_CL_STATS];

	if (LHNETADDRESS_GetAddressType(&host_client->netconnection->peeraddress) == LHNETADDRESSTYPE_LOOP && !sv_ratelimitlocalplayer.integer)
	{
		// for good singleplayer, send huge packets
		maxsize = sizeof(sv_sendclientdatagram_buf);
		maxsize2 = sizeof(sv_sendclientdatagram_buf);
	}
	else if (sv.protocol == PROTOCOL_DARKPLACES5 || sv.protocol == PROTOCOL_DARKPLACES6)
	{
		// PROTOCOL_DARKPLACES5 supports packet size limiting of updates
		maxrate = bound(NET_MINRATE, sv_maxrate.integer, NET_MAXRATE);
		if (sv_maxrate.integer != maxrate)
			Cvar_SetValueQuick(&sv_maxrate, maxrate);

		rate = bound(NET_MINRATE, client->rate, maxrate);
		rate = (int)(client->rate * sys_ticrate.value);
		maxsize = bound(100, rate, 1400);
		maxsize2 = 1400;
	}
	else
	{
		// no rate limiting support on older protocols because dp protocols
		// 1-4 kick the client off if they overflow, and quake protocol shows
		// less than the full entity set if rate limited
		maxsize = 1400;
		maxsize2 = 1400;
	}

	msg.data = sv_sendclientdatagram_buf;
	msg.maxsize = maxsize;
	msg.cursize = 0;

	MSG_WriteByte (&msg, svc_time);
	MSG_WriteFloat (&msg, sv.time);

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client, client->edict, &msg, stats);
	SV_WriteEntitiesToClient (client, client->edict, &msg, stats);

	// expand packet size to allow effects to go over the rate limit
	// (dropping them is FAR too ugly)
	msg.maxsize = maxsize2;

	// copy the server datagram if there is space
	// FIXME: put in delayed queue of effects to send
	if (sv.datagram.cursize > 0 && msg.cursize + sv.datagram.cursize <= msg.maxsize)
		SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

// send the datagram
	if (NetConn_SendUnreliableMessage (client->netconnection, &msg) == -1)
	{
		SV_DropClient (true);// if the message couldn't send, kick off
		return false;
	}

	return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
	int i, j;
	client_t *client;
	eval_t *val;
	char *name;
	char *model;
	char *skin;

// check for changes to be sent over the reliable streams
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		// update the host_client fields we care about according to the entity fields
		host_client->edict = EDICT_NUM(i+1);

		// DP_SV_CLIENTNAME
		name = PR_GetString(host_client->edict->v->netname);
		if (name == NULL)
			name = "";
		// always point the string back at host_client->name to keep it safe
		strlcpy (host_client->name, name, sizeof (host_client->name));
		host_client->edict->v->netname = PR_SetString(host_client->name);
		if (strcmp(host_client->old_name, host_client->name))
		{
			if (host_client->spawned)
				SV_BroadcastPrintf("%s changed name to %s\n", host_client->old_name, host_client->name);
			strcpy(host_client->old_name, host_client->name);
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteString (&sv.reliable_datagram, host_client->name);
		}

		// DP_SV_CLIENTCOLORS
		// this is always found (since it's added by the progs loader)
		if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_clientcolors)))
			host_client->colors = (int)val->_float;
		if (host_client->old_colors != host_client->colors)
		{
			host_client->old_colors = host_client->colors;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
		}

		// NEXUIZ_PLAYERMODEL
		if( eval_playermodel ) {
			model = PR_GetString(GETEDICTFIELDVALUE(host_client->edict, eval_playermodel)->string);
			if (model == NULL)
				model = "";
			// always point the string back at host_client->name to keep it safe
			strlcpy (host_client->playermodel, model, sizeof (host_client->playermodel));
			GETEDICTFIELDVALUE(host_client->edict, eval_playermodel)->string = PR_SetString(host_client->playermodel);
		}

		// NEXUIZ_PLAYERSKIN
		if( eval_playerskin ) {
			skin = PR_GetString(GETEDICTFIELDVALUE(host_client->edict, eval_playerskin)->string);
			if (skin == NULL)
				skin = "";
			// always point the string back at host_client->name to keep it safe
			strlcpy (host_client->playerskin, skin, sizeof (host_client->playerskin));
			GETEDICTFIELDVALUE(host_client->edict, eval_playerskin)->string = PR_SetString(host_client->playerskin);
		}

		// frags
		host_client->frags = (int)host_client->edict->v->frags;
		if (host_client->old_frags != host_client->frags)
		{
			host_client->old_frags = host_client->frags;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatefrags);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteShort (&sv.reliable_datagram, host_client->frags);
		}
	}

	for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
		if (client->netconnection)
			SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
void SV_SendNop (client_t *client)
{
	sizebuf_t	msg;
	qbyte		buf[4];

	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteChar (&msg, svc_nop);

	if (NetConn_SendUnreliableMessage (client->netconnection, &msg) == -1)
		SV_DropClient (true);	// if the message couldn't send, kick off
	client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int i, prepared = false;

// update frags, names, etc
	SV_UpdateToReliableMessages();

// build individual updates
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;
		if (!host_client->netconnection)
		{
			SZ_Clear(&host_client->message);
			continue;
		}

		if (host_client->deadsocket || host_client->message.overflowed)
		{
			SV_DropClient (true);	// if the message couldn't send, kick off
			continue;
		}

		if (host_client->spawned)
		{
			if (!prepared)
			{
				prepared = true;
				// only prepare entities once per frame
				SV_PrepareEntitiesForSending();
			}
			if (!SV_SendClientDatagram (host_client))
				continue;
		}
		else
		{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		// send a full message when the next signon stage has been requested
		// some other message data (name changes, etc) may accumulate
		// between signon stages
			if (!host_client->sendsignon)
			{
				if (realtime - host_client->last_message > 5)
					SV_SendNop (host_client);
				continue;	// don't send out non-signon messages
			}
		}

		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NetConn_CanSendMessage (host_client->netconnection))
				continue;

			if (host_client->dropasap)
				SV_DropClient (false);	// went to another level
			else
			{
				if (NetConn_SendReliableMessage (host_client->netconnection, &host_client->message) == -1)
					SV_DropClient (true);	// if the message couldn't send, kick off
				SZ_Clear (&host_client->message);
				host_client->last_message = realtime;
				host_client->sendsignon = false;
			}
		}
	}

// clear muzzle flashes
	SV_CleanupEnts();
}


/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex(char *s, int precachemode)
{
	int i, limit = (sv.protocol == PROTOCOL_QUAKE ? 256 : MAX_MODELS);
	char filename[MAX_QPATH];
	if (!s || !*s)
		return 0;
	// testing
	//if (precachemode == 2)
	//	return 0;
	strlcpy(filename, s, sizeof(filename));
	for (i = 2;i < limit;i++)
	{
		if (!sv.model_precache[i][0])
		{
			if (precachemode)
			{
				if (sv.state != ss_loading && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5))
				{
					Con_Printf("SV_ModelIndex(\"%s\"): precache_model can only be done in spawn functions\n", filename);
					return 0;
				}
				if (precachemode == 1)
					Con_Printf("SV_ModelIndex(\"%s\"): not precached (fix your code), precaching anyway\n", filename);
				strlcpy(sv.model_precache[i], filename, sizeof(sv.model_precache[i]));
				sv.models[i] = Mod_ForName (sv.model_precache[i], true, false, false);
				if (sv.protocol == PROTOCOL_DARKPLACES6 && sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svc_precache);
					MSG_WriteShort(&sv.reliable_datagram, i);
					MSG_WriteString(&sv.reliable_datagram, filename);
				}
				return i;
			}
			Con_Printf("SV_ModelIndex(\"%s\"): not precached\n", filename);
			return 0;
		}
		if (!strcmp(sv.model_precache[i], filename))
			return i;
	}
	Con_Printf("SV_ModelIndex(\"%s\"): i (%i) == MAX_MODELS (%i)\n", filename, i, MAX_MODELS);
	return 0;
}

/*
================
SV_SoundIndex

================
*/
int SV_SoundIndex(char *s, int precachemode)
{
	int i, limit = (sv.protocol == PROTOCOL_QUAKE ? 256 : MAX_SOUNDS);
	char filename[MAX_QPATH];
	if (!s || !*s)
		return 0;
	// testing
	//if (precachemode == 2)
	//	return 0;
	strlcpy(filename, s, sizeof(filename));
	for (i = 1;i < limit;i++)
	{
		if (!sv.sound_precache[i][0])
		{
			if (precachemode)
			{
				if (sv.state != ss_loading && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5))
				{
					Con_Printf("SV_SoundIndex(\"%s\"): precache_sound can only be done in spawn functions\n", filename);
					return 0;
				}
				if (precachemode == 1)
					Con_Printf("SV_SoundIndex(\"%s\"): not precached (fix your code), precaching anyway\n", filename);
				strlcpy(sv.sound_precache[i], filename, sizeof(sv.sound_precache[i]));
				if (sv.protocol == PROTOCOL_DARKPLACES6 && sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svc_precache);
					MSG_WriteShort(&sv.reliable_datagram, i + 32768);
					MSG_WriteString(&sv.reliable_datagram, filename);
				}
				return i;
			}
			Con_Printf("SV_SoundIndex(\"%s\"): not precached\n", filename);
			return 0;
		}
		if (!strcmp(sv.sound_precache[i], filename))
			return i;
	}
	Con_Printf("SV_SoundIndex(\"%s\"): i (%i) == MAX_SOUNDS (%i)\n", filename, i, MAX_SOUNDS);
	return 0;
}

/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
	int i, entnum, large;
	edict_t *svent;

	// LordHavoc: clear *all* states (note just active ones)
	for (entnum = 0;entnum < sv.max_edicts;entnum++)
	{
		// get the current server version
		svent = EDICT_NUM(entnum);

		// LordHavoc: always clear state values, whether the entity is in use or not
		svent->e->baseline = defaultstate;

		if (svent->e->free)
			continue;
		if (entnum > svs.maxclients && !svent->v->modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->v->origin, svent->e->baseline.origin);
		VectorCopy (svent->v->angles, svent->e->baseline.angles);
		svent->e->baseline.frame = svent->v->frame;
		svent->e->baseline.skin = svent->v->skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->e->baseline.colormap = entnum;
			svent->e->baseline.modelindex = SV_ModelIndex("progs/player.mdl", 1);
		}
		else
		{
			svent->e->baseline.colormap = 0;
			svent->e->baseline.modelindex = svent->v->modelindex;
		}

		large = false;
		if (svent->e->baseline.modelindex & 0xFF00 || svent->e->baseline.frame & 0xFF00)
			large = true;

		// add to the message
		if (large)
			MSG_WriteByte (&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		MSG_WriteShort (&sv.signon, entnum);

		if (large)
		{
			MSG_WriteShort (&sv.signon, svent->e->baseline.modelindex);
			MSG_WriteShort (&sv.signon, svent->e->baseline.frame);
		}
		else
		{
			MSG_WriteByte (&sv.signon, svent->e->baseline.modelindex);
			MSG_WriteByte (&sv.signon, svent->e->baseline.frame);
		}
		MSG_WriteByte (&sv.signon, svent->e->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->e->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->e->baseline.origin[i], sv.protocol);
			MSG_WriteAngle(&sv.signon, svent->e->baseline.angles[i], sv.protocol);
		}
	}
}


/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect (void)
{
	char	data[128];
	sizebuf_t	msg;

	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof(data);

	MSG_WriteChar (&msg, svc_stufftext);
	MSG_WriteString (&msg, "reconnect\n");
	NetConn_SendToAll (&msg, 5);

	if (cls.state != ca_dedicated)
		Cmd_ExecuteString ("reconnect\n", src_command);
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int		i, j;

	svs.serverflags = pr_global_struct->serverflags;

	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;

	// call the progs to get default spawn parms for the new client
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram (pr_global_struct->SetChangeParms, "QC function SetChangeParms is missing");
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}

void SV_IncreaseEdicts(void)
{
	int i;
	edict_t *ent;
	int oldmax_edicts = sv.max_edicts;
	void *oldedictsengineprivate = sv.edictsengineprivate;
	void *oldedictsfields = sv.edictsfields;
	void *oldmoved_edicts = sv.moved_edicts;

	if (sv.max_edicts >= MAX_EDICTS)
		return;

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = sv.edicts;i < sv.max_edicts;i++, ent++)
	{
		if (!ent->e->free)
			SV_UnlinkEdict(sv.edicts + i);
		memset(&ent->e->areagrid, 0, sizeof(ent->e->areagrid));
	}
	SV_ClearWorld();

	sv.max_edicts   = min(sv.max_edicts + 256, MAX_EDICTS);
	sv.edictsengineprivate = PR_Alloc(sv.max_edicts * sizeof(edict_engineprivate_t));
	sv.edictsfields = PR_Alloc(sv.max_edicts * pr_edict_size);
	sv.moved_edicts = PR_Alloc(sv.max_edicts * sizeof(edict_t *));

	memcpy(sv.edictsengineprivate, oldedictsengineprivate, oldmax_edicts * sizeof(edict_engineprivate_t));
	memcpy(sv.edictsfields, oldedictsfields, oldmax_edicts * pr_edict_size);

	for (i = 0, ent = sv.edicts;i < sv.max_edicts;i++, ent++)
	{
		ent->e = sv.edictsengineprivate + i;
		ent->v = (void *)((qbyte *)sv.edictsfields + i * pr_edict_size);
		// link every entity except world
		if (!ent->e->free)
			SV_LinkEdict(ent, false);
	}

	PR_Free(oldedictsengineprivate);
	PR_Free(oldedictsfields);
	PR_Free(oldmoved_edicts);
}

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float		scr_centertime_off;

void SV_SpawnServer (const char *server)
{
	edict_t *ent;
	int i;
	qbyte *entities;
	model_t *worldmodel;
	char modelname[sizeof(sv.modelname)];

	Con_DPrintf("SpawnServer: %s\n", server);

	if (cls.state != ca_dedicated)
		SCR_BeginLoadingPlaque();

	dpsnprintf (modelname, sizeof(modelname), "maps/%s.bsp", server);
	worldmodel = Mod_ForName(modelname, false, true, true);
	if (!worldmodel || !worldmodel->TraceBox)
	{
		Con_Printf("Couldn't load map %s\n", modelname);
		return;
	}

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		Cvar_Set ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	svs.changelevel_issued = false;		// now safe to issue another

//
// tell all connected clients that we are going to a new level
//
	if (sv.active)
		SV_SendReconnect();
	else
	{
		// make sure cvars have been checked before opening the ports
		NetConn_ServerFrame();
		NetConn_OpenServerPorts(true);
	}

//
// make cvars consistant
//
	if (coop.integer)
		Cvar_SetValue ("deathmatch", 0);
	// LordHavoc: it can be useful to have skills outside the range 0-3...
	//current_skill = bound(0, (int)(skill.value + 0.5), 3);
	//Cvar_SetValue ("skill", (float)current_skill);
	current_skill = (int)(skill.value + 0.5);

//
// set up the new server
//
	Host_ClearMemory ();

	memset (&sv, 0, sizeof(sv));

	strlcpy (sv.name, server, sizeof (sv.name));

	sv.netquakecompatible = false;
	if (!strcasecmp(sv_protocolname.string, "QUAKE"))
	{
		sv.protocol = PROTOCOL_QUAKE;
		sv.netquakecompatible = true;
	}
	else if (!strcasecmp(sv_protocolname.string, "QUAKEDP"))
		sv.protocol = PROTOCOL_QUAKE;
	else if (!strcasecmp(sv_protocolname.string, "DARKPLACES1"))
		sv.protocol = PROTOCOL_DARKPLACES1;
	else if (!strcasecmp(sv_protocolname.string, "DARKPLACES2"))
		sv.protocol = PROTOCOL_DARKPLACES2;
	else if (!strcasecmp(sv_protocolname.string, "DARKPLACES3"))
		sv.protocol = PROTOCOL_DARKPLACES3;
	else if (!strcasecmp(sv_protocolname.string, "DARKPLACES4"))
		sv.protocol = PROTOCOL_DARKPLACES4;
	else if (!strcasecmp(sv_protocolname.string, "DARKPLACES5"))
		sv.protocol = PROTOCOL_DARKPLACES5;
	else if (!strcasecmp(sv_protocolname.string, "DARKPLACES6"))
		sv.protocol = PROTOCOL_DARKPLACES6;
	else
	{
		sv.protocol = PROTOCOL_DARKPLACES6;
		Con_Printf("Unknown sv_protocolname \"%s\", valid values are QUAKE, QUAKEDP, DARKPLACES1, DARKPLACES2, DARKPLACES3, DARKPLACES4, DARKPLACES5, DARKPLACES6, falling back to DARKPLACES6 protocol\n", sv_protocolname.string);
	}

// load progs to get entity field count
	PR_LoadProgs ( sv_progs.string );

// allocate server memory
	// start out with just enough room for clients and a reasonable estimate of entities
	sv.max_edicts = max(svs.maxclients + 1, 512);
	sv.max_edicts = min(sv.max_edicts, MAX_EDICTS);

	// edict_t structures (hidden from progs)
	sv.edicts = PR_Alloc(MAX_EDICTS * sizeof(edict_t));
	// engine private structures (hidden from progs)
	sv.edictsengineprivate = PR_Alloc(sv.max_edicts * sizeof(edict_engineprivate_t));
	// progs fields, often accessed by server
	sv.edictsfields = PR_Alloc(sv.max_edicts * pr_edict_size);
	// used by PushMove to move back pushed entities
	sv.moved_edicts = PR_Alloc(sv.max_edicts * sizeof(edict_t *));
	for (i = 0;i < sv.max_edicts;i++)
	{
		ent = sv.edicts + i;
		ent->e = sv.edictsengineprivate + i;
		ent->v = (void *)((qbyte *)sv.edictsfields + i * pr_edict_size);
	}

	// fix up client->edict pointers for returning clients right away...
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		host_client->edict = EDICT_NUM(i + 1);

	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.signon.maxsize = sizeof(sv.signon_buf);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

// leave slots at start for clients only
	sv.num_edicts = svs.maxclients+1;

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	Mod_ClearUsed();
	worldmodel->used = true;

	strlcpy (sv.name, server, sizeof (sv.name));
	strcpy(sv.modelname, modelname);
	sv.worldmodel = worldmodel;
	sv.models[1] = sv.worldmodel;

//
// clear world interaction links
//
	SV_ClearWorld ();

	strlcpy(sv.sound_precache[0], "", sizeof(sv.sound_precache[0]));

	strlcpy(sv.model_precache[0], "", sizeof(sv.model_precache[0]));
	strlcpy(sv.model_precache[1], sv.modelname, sizeof(sv.model_precache[1]));
	for (i = 1;i < sv.worldmodel->brush.numsubmodels;i++)
	{
		dpsnprintf(sv.model_precache[i+1], sizeof(sv.model_precache[i+1]), "*%i", i);
		sv.models[i+1] = Mod_ForName (sv.model_precache[i+1], false, false, false);
	}

//
// load the rest of the entities
//
	ent = EDICT_NUM(0);
	memset (ent->v, 0, progs->entityfields * 4);
	ent->e->free = false;
	ent->v->model = PR_SetString(sv.modelname);
	ent->v->modelindex = 1;		// world model
	ent->v->solid = SOLID_BSP;
	ent->v->movetype = MOVETYPE_PUSH;

	if (coop.value)
		pr_global_struct->coop = coop.integer;
	else
		pr_global_struct->deathmatch = deathmatch.integer;

	pr_global_struct->mapname = PR_SetString(sv.name);

// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	// load replacement entity file if found
	entities = NULL;
	if (sv_entpatch.integer)
		entities = FS_LoadFile(va("maps/%s.ent", sv.name), tempmempool, true);
	if (entities)
	{
		Con_Printf("Loaded maps/%s.ent\n", sv.name);
		ED_LoadFromFile (entities);
		Mem_Free(entities);
	}
	else
		ED_LoadFromFile (sv.worldmodel->brush.entities);


	// LordHavoc: clear world angles (to fix e3m3.bsp)
	VectorClear(sv.edicts->v->angles);

	sv.active = true;

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

// run two frames to allow everything to settle
	for (i = 0;i < 2;i++)
	{
		sv.frametime = pr_global_struct->frametime = host_frametime = 0.1;
		SV_Physics ();
	}

	Mod_PurgeUnused();

// create a baseline for more efficient communications
	if (sv.protocol == PROTOCOL_QUAKE)
		SV_CreateBaseline ();

// send serverinfo to all connected clients
	// (note this also handles botclients coming back from a level change)
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo(host_client);

	Con_DPrint("Server spawned.\n");
	NetConn_Heartbeat (2);
}

