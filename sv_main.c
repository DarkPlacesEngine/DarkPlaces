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
#include "portals.h"

static cvar_t sv_cullentities_pvs = {0, "sv_cullentities_pvs", "0"}; // fast but loose
static cvar_t sv_cullentities_portal = {0, "sv_cullentities_portal", "0"}; // extremely accurate visibility checking, but too slow
static cvar_t sv_cullentities_trace = {0, "sv_cullentities_trace", "1"}; // tends to get false negatives, uses a timeout to keep entities visible a short time after becoming hidden
static cvar_t sv_cullentities_stats = {0, "sv_cullentities_stats", "0"};
static cvar_t sv_entpatch = {0, "sv_entpatch", "1"};

server_t		sv;
server_static_t	svs;

static char localmodels[MAX_MODELS][5];			// inline model names for precache

static mempool_t *sv_edicts_mempool = NULL;

//============================================================================

extern void SV_Phys_Init (void);
extern void SV_World_Init (void);

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	int i;

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
	Cvar_RegisterVariable (&sv_predict);
	Cvar_RegisterVariable (&sv_deltacompress);
	Cvar_RegisterVariable (&sv_cullentities_pvs);
	Cvar_RegisterVariable (&sv_cullentities_portal);
	Cvar_RegisterVariable (&sv_cullentities_trace);
	Cvar_RegisterVariable (&sv_cullentities_stats);
	Cvar_RegisterVariable (&sv_entpatch);

	SV_Phys_Init();
	SV_World_Init();

	for (i = 0;i < MAX_MODELS;i++)
		sprintf (localmodels[i], "*%i", i);

	sv_edicts_mempool = Mem_AllocPool("edicts");
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

	if (sv.datagram.cursize > MAX_DATAGRAM-16)
		return;
	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteDPCoord (&sv.datagram, org[0]);
	MSG_WriteDPCoord (&sv.datagram, org[1]);
	MSG_WriteDPCoord (&sv.datagram, org[2]);
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
	if (sv.datagram.cursize > MAX_DATAGRAM-18)
		return;
	if (modelindex >= 256 || startframe >= 256)
	{
		MSG_WriteByte (&sv.datagram, svc_effect2);
		MSG_WriteDPCoord (&sv.datagram, org[0]);
		MSG_WriteDPCoord (&sv.datagram, org[1]);
		MSG_WriteDPCoord (&sv.datagram, org[2]);
		MSG_WriteShort (&sv.datagram, modelindex);
		MSG_WriteShort (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
	else
	{
		MSG_WriteByte (&sv.datagram, svc_effect);
		MSG_WriteDPCoord (&sv.datagram, org[0]);
		MSG_WriteDPCoord (&sv.datagram, org[1]);
		MSG_WriteDPCoord (&sv.datagram, org[2]);
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
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume,
    float attenuation)
{
    int         sound_num;
    int field_mask;
    int			i;
	int			ent;

	if (volume < 0 || volume > 255)
		Host_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Host_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Host_Error ("SV_StartSound: channel = %i", channel);

	if (sv.datagram.cursize > MAX_DATAGRAM-16)
		return;

// find precache number for sound
    for (sound_num=1 ; sound_num<MAX_SOUNDS
        && sv.sound_precache[sound_num] ; sound_num++)
        if (!strcmp(sample, sv.sound_precache[sound_num]))
            break;

    if ( sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num] )
    {
        Con_Printf ("SV_StartSound: %s not precached\n", sample);
        return;
    }

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
		MSG_WriteDPCoord (&sv.datagram, entity->v->origin[i]+0.5*(entity->v->mins[i]+entity->v->maxs[i]));
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
	char			**s;
	char			message[2048];

	// LordHavoc: clear entityframe tracking
	client->entityframenumber = 0;
	EntityFrame_ClearDatabase(&client->entitydatabase);

	MSG_WriteByte (&client->message, svc_print);
	sprintf (message, "\002\nServer: %s build %s (progs %i crc)", gamename, buildstring, pr_crc);
	MSG_WriteString (&client->message,message);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, DPPROTOCOL_VERSION3);
	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop.integer && deathmatch.integer)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	sprintf (message, PR_GetString(sv.edicts->v->message));

	MSG_WriteString (&client->message,message);

	for (s = sv.model_precache+1 ; *s ; s++)
		MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);

	for (s = sv.sound_precache+1 ; *s ; s++)
		MSG_WriteString (&client->message, *s);
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
void SV_ConnectClient (int clientnum)
{
	edict_t			*ent;
	client_t		*client;
	int				edictnum;
	struct qsocket_s *netconnection;
	int				i;
	float			spawn_parms[NUM_SPAWN_PARMS];

	client = svs.clients + clientnum;

	Con_DPrintf ("Client %s connected\n", client->netconnection->address);

	edictnum = clientnum+1;

	ent = EDICT_NUM(edictnum);

// set up the client_t
	netconnection = client->netconnection;

	if (sv.loadgame)
		memcpy (spawn_parms, client->spawn_parms, sizeof(spawn_parms));
	memset (client, 0, sizeof(*client));
	client->netconnection = netconnection;

	strcpy (client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof(client->msgbuf);
	client->message.allowoverflow = true;		// we can catch it

	if (sv.loadgame)
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	else
	{
	// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (pr_global_struct->SetNewParms, "QC function SetNewParms is missing");
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
	}

#if NOROUTINGFIX
	SV_SendServerinfo (client);
#else
	// send serverinfo on first nop
	client->waitingforconnect = true;
	client->sendsignon = true;
	client->spawned = false;		// need prespawn, spawn, etc
#endif
}


/*
===================
SV_CheckForNewClients

===================
*/
void SV_CheckForNewClients (void)
{
	struct qsocket_s	*ret;
	int				i;

//
// check for new connections
//
	while (1)
	{
		ret = NET_CheckNewConnections ();
		if (!ret)
			break;

	//
	// init a new client structure
	//
		for (i=0 ; i<svs.maxclients ; i++)
			if (!svs.clients[i].active)
				break;
		if (i == svs.maxclients)
			Sys_Error ("Host_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient (i);

		net_activeconnections++;
		NET_Heartbeat (1);
	}
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

int		fatbytes;
qbyte	fatpvs[MAX_MAP_LEAFS/8];

void SV_AddToFatPVS (vec3_t org, mnode_t *node)
{
	int		i;
	qbyte	*pvs;
	mplane_t	*plane;
	float	d;

	while (1)
	{
	// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = Mod_LeafPVS ( (mleaf_t *)node, sv.worldmodel);
				for (i=0 ; i<fatbytes ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPVS (org, node->children[0]);
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
qbyte *SV_FatPVS (vec3_t org)
{
	fatbytes = (sv.worldmodel->numleafs+31)>>3;
	memset (fatpvs, 0, fatbytes);
	SV_AddToFatPVS (org, sv.worldmodel->nodes);
	return fatpvs;
}

//=============================================================================


int SV_BoxTouchingPVS (qbyte *pvs, vec3_t mins, vec3_t maxs, mnode_t *node)
{
	int leafnum;
loc0:
	if (node->contents < 0)
	{
		// leaf
		if (node->contents == CONTENTS_SOLID)
			return false;
		leafnum = (mleaf_t *)node - sv.worldmodel->leafs - 1;
		return pvs[leafnum >> 3] & (1 << (leafnum & 7));
	}

	// node - recurse down the BSP tree
	switch (BoxOnPlaneSide(mins, maxs, node->plane))
	{
	case 1: // front
		node = node->children[0];
		goto loc0;
	case 2: // back
		node = node->children[1];
		goto loc0;
	default: // crossing
		if (node->children[0]->contents != CONTENTS_SOLID)
			if (SV_BoxTouchingPVS (pvs, mins, maxs, node->children[0]))
				return true;
		node = node->children[1];
		goto loc0;
	}
	// never reached
	return false;
}


/*
=============
SV_WriteEntitiesToClient

=============
*/
#ifdef QUAKEENTITIES
void SV_WriteEntitiesToClient (client_t *client, edict_t *clent, sizebuf_t *msg)
{
	int e, clentnum, bits, alpha, glowcolor, glowsize, scale, effects, lightsize;
	int culled_pvs, culled_portal, culled_trace, visibleentities, totalentities;
	qbyte *pvs;
	vec3_t origin, angles, entmins, entmaxs, testorigin, testeye;
	float nextfullupdate, alphaf;
	edict_t *ent;
	eval_t *val;
	entity_state_t *baseline; // LordHavoc: delta or startup baseline
	trace_t trace;
	model_t *model;

	Mod_CheckLoaded(sv.worldmodel);

// find the client's PVS
	VectorAdd (clent->v->origin, clent->v->view_ofs, testeye);
	pvs = SV_FatPVS (testeye);

	culled_pvs = 0;
	culled_portal = 0;
	culled_trace = 0;
	visibleentities = 0;
	totalentities = 0;

	clentnum = EDICT_TO_PROG(clent); // LordHavoc: for comparison purposes
	// send all entities that touch the pvs
	ent = NEXT_EDICT(sv.edicts);
	for (e = 1;e < sv.num_edicts;e++, ent = NEXT_EDICT(ent))
	{
		bits = 0;

		// prevent delta compression against this frame (unless actually sent, which will restore this later)
		nextfullupdate = client->nextfullupdate[e];
		client->nextfullupdate[e] = -1;

		if (ent != clent) // LordHavoc: always send player
		{
			if ((val = GETEDICTFIELDVALUE(ent, eval_viewmodelforclient)) && val->edict)
			{
				if (val->edict != clentnum)
				{
					// don't show to anyone else
					continue;
				}
				else
					bits |= U_VIEWMODEL; // show relative to the view
			}
			else
			{
				// LordHavoc: never draw something told not to display to this client
				if ((val = GETEDICTFIELDVALUE(ent, eval_nodrawtoclient)) && val->edict == clentnum)
					continue;
				if ((val = GETEDICTFIELDVALUE(ent, eval_drawonlytoclient)) && val->edict && val->edict != clentnum)
					continue;
			}
		}

		glowsize = 0;

		if ((val = GETEDICTFIELDVALUE(ent, eval_glow_size)))
			glowsize = (int) val->_float >> 2;
		if (glowsize > 255) glowsize = 255;
		if (glowsize < 0) glowsize = 0;

		if ((val = GETEDICTFIELDVALUE(ent, eval_glow_trail)))
		if (val->_float != 0)
			bits |= U_GLOWTRAIL;

		if (ent->v->modelindex >= 0 && ent->v->modelindex < MAX_MODELS && *PR_GetString(ent->v->model))
		{
			model = sv.models[(int)ent->v->modelindex];
			Mod_CheckLoaded(model);
		}
		else
		{
			model = NULL;
			if (ent != clent) // LordHavoc: always send player
				if (glowsize == 0 && (bits & U_GLOWTRAIL) == 0) // no effects
					continue;
		}

		VectorCopy(ent->v->angles, angles);
		if (DotProduct(ent->v->velocity, ent->v->velocity) >= 1.0f)
		{
			VectorMA(ent->v->origin, host_client->latency, ent->v->velocity, origin);
			// LordHavoc: trace predicted movement to avoid putting things in walls
			trace = SV_Move (ent->v->origin, ent->v->mins, ent->v->maxs, origin, MOVE_NORMAL, ent);
			VectorCopy(trace.endpos, origin);
		}
		else
		{
			VectorCopy(ent->v->origin, origin);
		}

		// ent has survived every check so far, check if it is visible
		if (ent != clent && ((bits & U_VIEWMODEL) == 0))
		{
			// use the predicted origin
			entmins[0] = origin[0] - 1.0f;
			entmins[1] = origin[1] - 1.0f;
			entmins[2] = origin[2] - 1.0f;
			entmaxs[0] = origin[0] + 1.0f;
			entmaxs[1] = origin[1] + 1.0f;
			entmaxs[2] = origin[2] + 1.0f;
			// using the model's bounding box to ensure things are visible regardless of their physics box
			if (model)
			{
				if (ent->v->angles[0] || ent->v->angles[2]) // pitch and roll
				{
					VectorAdd(entmins, model->rotatedmins, entmins);
					VectorAdd(entmaxs, model->rotatedmaxs, entmaxs);
				}
				else if (ent->v->angles[1])
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

			totalentities++;

			// if not touching a visible leaf
			if (sv_cullentities_pvs.integer && !SV_BoxTouchingPVS(pvs, entmins, entmaxs, sv.worldmodel->nodes))
			{
				culled_pvs++;
				continue;
			}

			// or not visible through the portals
			if (sv_cullentities_portal.integer && !Portal_CheckBox(sv.worldmodel, testeye, entmins, entmaxs))
			{
				culled_portal++;
				continue;
			}

			// don't try to cull embedded brush models with this, they're sometimes huge (spanning several rooms)
			if (sv_cullentities_trace.integer && (model == NULL || model->type != mod_brush || model->name[0] != '*'))
			{
				// LordHavoc: test random offsets, to maximize chance of detection
				testorigin[0] = lhrandom(entmins[0], entmaxs[0]);
				testorigin[1] = lhrandom(entmins[1], entmaxs[1]);
				testorigin[2] = lhrandom(entmins[2], entmaxs[2]);

				Collision_ClipTrace(&trace, NULL, sv.worldmodel, vec3_origin, vec3_origin, vec3_origin, testeye, vec3_origin, vec3_origin, testorigin);

				if (trace.fraction == 1)
					client->visibletime[e] = realtime + 1;
				else
				{
					//test nearest point on bbox
					testorigin[0] = bound(entmins[0], testeye[0], entmaxs[0]);
					testorigin[1] = bound(entmins[1], testeye[1], entmaxs[1]);
					testorigin[2] = bound(entmins[2], testeye[2], entmaxs[2]);

					Collision_ClipTrace(&trace, NULL, sv.worldmodel, vec3_origin, vec3_origin, vec3_origin, testeye, vec3_origin, vec3_origin, testorigin);

					if (trace.fraction == 1)
						client->visibletime[e] = realtime + 1;
					else if (realtime > client->visibletime[e])
					{
						culled_trace++;
						continue;
					}
				}
			}
			visibleentities++;
		}

		alphaf = 255.0f;
		scale = 16;
		glowcolor = 254;
		effects = ent->v->effects;

		if ((val = GETEDICTFIELDVALUE(ent, eval_alpha)))
		if (val->_float != 0)
			alphaf = val->_float * 255.0f;

		// HalfLife support
		if ((val = GETEDICTFIELDVALUE(ent, eval_renderamt)))
		if (val->_float != 0)
			alphaf = val->_float;

		if (alphaf == 0.0f)
			alphaf = 255.0f;
		alpha = bound(0, alphaf, 255);

		if ((val = GETEDICTFIELDVALUE(ent, eval_scale)))
		if ((scale = (int) (val->_float * 16.0)) == 0) scale = 16;
		if (scale < 0) scale = 0;
		if (scale > 255) scale = 255;

		if ((val = GETEDICTFIELDVALUE(ent, eval_glow_color)))
		if (val->_float != 0)
			glowcolor = (int) val->_float;

		if ((val = GETEDICTFIELDVALUE(ent, eval_fullbright)))
		if (val->_float != 0)
			effects |= EF_FULLBRIGHT;

		if (ent != clent)
		{
			if (glowsize == 0 && (bits & U_GLOWTRAIL) == 0) // no effects
			{
				if (model) // model
				{
					// don't send if flagged for NODRAW and there are no effects
					if (model->flags == 0 && ((effects & EF_NODRAW) || scale <= 0 || alpha <= 0))
						continue;
				}
				else // no model and no effects
					continue;
			}
		}

		if (msg->maxsize - msg->cursize < 32) // LordHavoc: increased check from 16 to 32
		{
			Con_Printf ("packet overflow\n");
			// mark the rest of the entities so they can't be delta compressed against this frame
			for (;e < sv.num_edicts;e++)
			{
				client->nextfullupdate[e] = -1;
				client->visibletime[e] = -1;
			}
			return;
		}

		if ((val = GETEDICTFIELDVALUE(ent, eval_exteriormodeltoclient)) && val->edict == clentnum)
			bits = bits | U_EXTERIORMODEL;

// send an update
		baseline = &ent->baseline;

		if (((int)ent->v->effects & EF_DELTA) && sv_deltacompress.integer)
		{
			// every half second a full update is forced
			if (realtime < client->nextfullupdate[e])
			{
				bits |= U_DELTA;
				baseline = &ent->deltabaseline;
			}
			else
				nextfullupdate = realtime + 0.5f;
		}
		else
			nextfullupdate = realtime + 0.5f;

		// restore nextfullupdate since this is being sent for real
		client->nextfullupdate[e] = nextfullupdate;

		if (e >= 256)
			bits |= U_LONGENTITY;

		if (ent->v->movetype == MOVETYPE_STEP)
			bits |= U_STEP;

		// LordHavoc: old stuff, but rewritten to have more exact tolerances
		if (origin[0] != baseline->origin[0])											bits |= U_ORIGIN1;
		if (origin[1] != baseline->origin[1])											bits |= U_ORIGIN2;
		if (origin[2] != baseline->origin[2])											bits |= U_ORIGIN3;
		if (((int)(angles[0]*(256.0/360.0)) & 255) != ((int)(baseline->angles[0]*(256.0/360.0)) & 255))	bits |= U_ANGLE1;
		if (((int)(angles[1]*(256.0/360.0)) & 255) != ((int)(baseline->angles[1]*(256.0/360.0)) & 255))	bits |= U_ANGLE2;
		if (((int)(angles[2]*(256.0/360.0)) & 255) != ((int)(baseline->angles[2]*(256.0/360.0)) & 255))	bits |= U_ANGLE3;
		if (baseline->colormap != (qbyte) ent->v->colormap)								bits |= U_COLORMAP;
		if (baseline->skin != (qbyte) ent->v->skin)										bits |= U_SKIN;
		if ((baseline->frame & 0x00FF) != ((int) ent->v->frame & 0x00FF))				bits |= U_FRAME;
		if ((baseline->effects & 0x00FF) != ((int) ent->v->effects & 0x00FF))			bits |= U_EFFECTS;
		if ((baseline->modelindex & 0x00FF) != ((int) ent->v->modelindex & 0x00FF))		bits |= U_MODEL;

		// LordHavoc: new stuff
		if (baseline->alpha != alpha)													bits |= U_ALPHA;
		if (baseline->scale != scale)													bits |= U_SCALE;
		if (((int) baseline->effects & 0xFF00) != ((int) ent->v->effects & 0xFF00))		bits |= U_EFFECTS2;
		if (baseline->glowsize != glowsize)												bits |= U_GLOWSIZE;
		if (baseline->glowcolor != glowcolor)											bits |= U_GLOWCOLOR;
		if (((int) baseline->frame & 0xFF00) != ((int) ent->v->frame & 0xFF00))			bits |= U_FRAME2;
		if (((int) baseline->frame & 0xFF00) != ((int) ent->v->modelindex & 0xFF00))		bits |= U_MODEL2;

		// update delta baseline
		VectorCopy(ent->v->origin, ent->deltabaseline.origin);
		VectorCopy(ent->v->angles, ent->deltabaseline.angles);
		ent->deltabaseline.colormap = ent->v->colormap;
		ent->deltabaseline.skin = ent->v->skin;
		ent->deltabaseline.frame = ent->v->frame;
		ent->deltabaseline.effects = ent->v->effects;
		ent->deltabaseline.modelindex = ent->v->modelindex;
		ent->deltabaseline.alpha = alpha;
		ent->deltabaseline.scale = scale;
		ent->deltabaseline.glowsize = glowsize;
		ent->deltabaseline.glowcolor = glowcolor;

		// write the message
		if (bits >= 16777216)
			bits |= U_EXTEND2;
		if (bits >= 65536)
			bits |= U_EXTEND1;
		if (bits >= 256)
			bits |= U_MOREBITS;
		bits |= U_SIGNAL;

		MSG_WriteByte (msg, bits);
		if (bits & U_MOREBITS)
			MSG_WriteByte (msg, bits>>8);
		// LordHavoc: extend bytes have to be written here due to delta compression
		if (bits & U_EXTEND1)
			MSG_WriteByte (msg, bits>>16);
		if (bits & U_EXTEND2)
			MSG_WriteByte (msg, bits>>24);

		// LordHavoc: old stuff
		if (bits & U_LONGENTITY)
			MSG_WriteShort (msg,e);
		else
			MSG_WriteByte (msg,e);
		if (bits & U_MODEL)		MSG_WriteByte (msg,	ent->v->modelindex);
		if (bits & U_FRAME)		MSG_WriteByte (msg, ent->v->frame);
		if (bits & U_COLORMAP)	MSG_WriteByte (msg, ent->v->colormap);
		if (bits & U_SKIN)		MSG_WriteByte (msg, ent->v->skin);
		if (bits & U_EFFECTS)	MSG_WriteByte (msg, ent->v->effects);
		if (bits & U_ORIGIN1)	MSG_WriteDPCoord (msg, origin[0]);
		if (bits & U_ANGLE1)	MSG_WriteAngle(msg, angles[0]);
		if (bits & U_ORIGIN2)	MSG_WriteDPCoord (msg, origin[1]);
		if (bits & U_ANGLE2)	MSG_WriteAngle(msg, angles[1]);
		if (bits & U_ORIGIN3)	MSG_WriteDPCoord (msg, origin[2]);
		if (bits & U_ANGLE3)	MSG_WriteAngle(msg, angles[2]);

		// LordHavoc: new stuff
		if (bits & U_ALPHA)		MSG_WriteByte(msg, alpha);
		if (bits & U_SCALE)		MSG_WriteByte(msg, scale);
		if (bits & U_EFFECTS2)	MSG_WriteByte(msg, (int)ent->v->effects >> 8);
		if (bits & U_GLOWSIZE)	MSG_WriteByte(msg, glowsize);
		if (bits & U_GLOWCOLOR)	MSG_WriteByte(msg, glowcolor);
		if (bits & U_FRAME2)	MSG_WriteByte(msg, (int)ent->v->frame >> 8);
		if (bits & U_MODEL2)	MSG_WriteByte(msg, (int)ent->v->modelindex >> 8);
	}

	if (sv_cullentities_stats.integer)
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d portal %d trace\n", client->name, totalentities, visibleentities, culled_pvs + culled_portal + culled_trace, culled_pvs, culled_portal, culled_trace);
}
#else
static entity_frame_t sv_writeentitiestoclient_entityframe;
void SV_WriteEntitiesToClient (client_t *client, edict_t *clent, sizebuf_t *msg)
{
	int e, clentnum, flags, alpha, glowcolor, glowsize, scale, effects, modelindex;
	int culled_pvs, culled_portal, culled_trace, visibleentities, totalentities;
	float alphaf, lightsize;
	qbyte *pvs;
	vec3_t origin, angles, entmins, entmaxs, lightmins, lightmaxs, testorigin, testeye;
	edict_t *ent;
	eval_t *val;
	trace_t trace;
	model_t *model;
	entity_state_t *s;

	if (client->sendsignon)
		return;

	Mod_CheckLoaded(sv.worldmodel);

// find the client's PVS
	// the real place being tested from
	VectorAdd (clent->v->origin, clent->v->view_ofs, testeye);
	pvs = SV_FatPVS (testeye);

	// the place being reported (to consider the fact the client still
	// applies the view_ofs[2], so we have to only send the fractional part
	// of view_ofs[2], undoing what the client will redo)
	VectorCopy (testeye, testorigin);
	e = (int) clent->v->view_ofs[2] & 255;
	if (e >= 128)
		e -= 256;
	testorigin[2] -= (float) e;
	EntityFrame_Clear(&sv_writeentitiestoclient_entityframe, testorigin);

	culled_pvs = 0;
	culled_portal = 0;
	culled_trace = 0;
	visibleentities = 0;
	totalentities = 0;

	clentnum = EDICT_TO_PROG(clent); // LordHavoc: for comparison purposes
	// send all entities that touch the pvs
	ent = NEXT_EDICT(sv.edicts);
	for (e = 1;e < sv.num_edicts;e++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;
		flags = 0;

		if (ent != clent) // LordHavoc: always send player
		{
			if ((val = GETEDICTFIELDVALUE(ent, eval_viewmodelforclient)) && val->edict)
			{
				if (val->edict == clentnum)
					flags |= RENDER_VIEWMODEL; // show relative to the view
				else
				{
					// don't show to anyone else
					continue;
				}
			}
			else
			{
				// LordHavoc: never draw something told not to display to this client
				if ((val = GETEDICTFIELDVALUE(ent, eval_nodrawtoclient)) && val->edict == clentnum)
					continue;
				if ((val = GETEDICTFIELDVALUE(ent, eval_drawonlytoclient)) && val->edict && val->edict != clentnum)
					continue;
			}
		}

		glowsize = 0;
		effects = ent->v->effects;
		if ((val = GETEDICTFIELDVALUE(ent, eval_glow_size)))
			glowsize = (int) val->_float >> 2;
		glowsize = bound(0, glowsize, 255);

		lightsize = 0;
		if (effects & (EF_BRIGHTFIELD | EF_MUZZLEFLASH | EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_RED | EF_BLUE | EF_FLAME | EF_STARDUST))
		{
			if (effects & EF_BRIGHTFIELD)
				lightsize = max(lightsize, 80);
			if (effects & EF_MUZZLEFLASH)
				lightsize = max(lightsize, 100);
			if (effects & EF_BRIGHTLIGHT)
				lightsize = max(lightsize, 400);
			if (effects & EF_DIMLIGHT)
				lightsize = max(lightsize, 200);
			if (effects & EF_RED)
				lightsize = max(lightsize, 200);
			if (effects & EF_BLUE)
				lightsize = max(lightsize, 200);
			if (effects & EF_FLAME)
				lightsize = max(lightsize, 250);
			if (effects & EF_STARDUST)
				lightsize = max(lightsize, 100);
		}
		if (glowsize)
			lightsize = max(lightsize, glowsize << 2);

		if ((val = GETEDICTFIELDVALUE(ent, eval_glow_trail)))
		if (val->_float != 0)
		{
			flags |= RENDER_GLOWTRAIL;
			lightsize = max(lightsize, 100);
		}

		modelindex = 0;
		if (ent->v->modelindex >= 0 && ent->v->modelindex < MAX_MODELS && *PR_GetString(ent->v->model))
		{
			modelindex = ent->v->modelindex;
			model = sv.models[(int)ent->v->modelindex];
			Mod_CheckLoaded(model);
		}
		else
		{
			model = NULL;
			if (ent != clent) // LordHavoc: always send player
				if (lightsize == 0) // no effects
					continue;
		}

		VectorCopy(ent->v->angles, angles);
		if (DotProduct(ent->v->velocity, ent->v->velocity) >= 1.0f && host_client->latency >= 0.01f)
		{
			VectorMA(ent->v->origin, host_client->latency, ent->v->velocity, origin);
			// LordHavoc: trace predicted movement to avoid putting things in walls
			trace = SV_Move (ent->v->origin, ent->v->mins, ent->v->maxs, origin, MOVE_NORMAL, ent);
			VectorCopy(trace.endpos, origin);
		}
		else
		{
			VectorCopy(ent->v->origin, origin);
		}

		// ent has survived every check so far, check if it is visible
		// always send embedded brush models, they don't generate much traffic
		if (ent != clent && ((flags & RENDER_VIEWMODEL) == 0) && (model == NULL || model->type != mod_brush || model->name[0] != '*'))
		{
			// use the predicted origin
			entmins[0] = origin[0] - 1.0f;
			entmins[1] = origin[1] - 1.0f;
			entmins[2] = origin[2] - 1.0f;
			entmaxs[0] = origin[0] + 1.0f;
			entmaxs[1] = origin[1] + 1.0f;
			entmaxs[2] = origin[2] + 1.0f;
			// using the model's bounding box to ensure things are visible regardless of their physics box
			if (model)
			{
				if (ent->v->angles[0] || ent->v->angles[2]) // pitch and roll
				{
					VectorAdd(entmins, model->rotatedmins, entmins);
					VectorAdd(entmaxs, model->rotatedmaxs, entmaxs);
				}
				else if (ent->v->angles[1])
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
			lightmins[0] = min(entmins[0], origin[0] - lightsize);
			lightmins[1] = min(entmins[1], origin[1] - lightsize);
			lightmins[2] = min(entmins[2], origin[2] - lightsize);
			lightmaxs[0] = min(entmaxs[0], origin[0] + lightsize);
			lightmaxs[1] = min(entmaxs[1], origin[1] + lightsize);
			lightmaxs[2] = min(entmaxs[2], origin[2] + lightsize);

			totalentities++;

			// if not touching a visible leaf
			if (sv_cullentities_pvs.integer && !SV_BoxTouchingPVS(pvs, lightmins, lightmaxs, sv.worldmodel->nodes))
			{
				culled_pvs++;
				continue;
			}

			// or not visible through the portals
			if (sv_cullentities_portal.integer && !Portal_CheckBox(sv.worldmodel, testeye, lightmins, lightmaxs))
			{
				culled_portal++;
				continue;
			}

			if (sv_cullentities_trace.integer)
			{
				// LordHavoc: test center first
				testorigin[0] = (entmins[0] + entmaxs[0]) * 0.5f;
				testorigin[1] = (entmins[1] + entmaxs[1]) * 0.5f;
				testorigin[2] = (entmins[2] + entmaxs[2]) * 0.5f;
				Collision_ClipTrace(&trace, NULL, sv.worldmodel, vec3_origin, vec3_origin, vec3_origin, vec3_origin, testeye, vec3_origin, vec3_origin, testorigin);
				if (trace.fraction == 1)
					client->visibletime[e] = realtime + 1;
				else
				{
					// LordHavoc: test random offsets, to maximize chance of detection
					testorigin[0] = lhrandom(entmins[0], entmaxs[0]);
					testorigin[1] = lhrandom(entmins[1], entmaxs[1]);
					testorigin[2] = lhrandom(entmins[2], entmaxs[2]);
					Collision_ClipTrace(&trace, NULL, sv.worldmodel, vec3_origin, vec3_origin, vec3_origin, vec3_origin, testeye, vec3_origin, vec3_origin, testorigin);
					if (trace.fraction == 1)
						client->visibletime[e] = realtime + 1;
					else
					{
						if (lightsize)
						{
							// LordHavoc: test random offsets, to maximize chance of detection
							testorigin[0] = lhrandom(lightmins[0], lightmaxs[0]);
							testorigin[1] = lhrandom(lightmins[1], lightmaxs[1]);
							testorigin[2] = lhrandom(lightmins[2], lightmaxs[2]);
							Collision_ClipTrace(&trace, NULL, sv.worldmodel, vec3_origin, vec3_origin, vec3_origin, vec3_origin, testeye, vec3_origin, vec3_origin, testorigin);
							if (trace.fraction == 1)
								client->visibletime[e] = realtime + 1;
							else
							{
								if (realtime > client->visibletime[e])
								{
									culled_trace++;
									continue;
								}
							}
						}
						else
						{
							if (realtime > client->visibletime[e])
							{
								culled_trace++;
								continue;
							}
						}
					}
				}
			}
			visibleentities++;
		}

		alphaf = 255.0f;
		scale = 16;
		glowcolor = 254;
		effects = ent->v->effects;

		if ((val = GETEDICTFIELDVALUE(ent, eval_alpha)))
		if (val->_float != 0)
			alphaf = val->_float * 255.0;

		// HalfLife support
		if ((val = GETEDICTFIELDVALUE(ent, eval_renderamt)))
		if (val->_float != 0)
			alphaf = val->_float;

		if (alphaf == 0.0f)
			alphaf = 255.0f;
		alpha = bound(0, alphaf, 255);

		if ((val = GETEDICTFIELDVALUE(ent, eval_scale)))
		if ((scale = (int) (val->_float * 16.0)) == 0) scale = 16;
		if (scale < 0) scale = 0;
		if (scale > 255) scale = 255;

		if ((val = GETEDICTFIELDVALUE(ent, eval_glow_color)))
		if (val->_float != 0)
			glowcolor = (int) val->_float;

		if ((val = GETEDICTFIELDVALUE(ent, eval_fullbright)))
		if (val->_float != 0)
			effects |= EF_FULLBRIGHT;

		if (ent != clent)
		{
			if (lightsize == 0) // no effects
			{
				if (model) // model
				{
					// don't send if flagged for NODRAW and there are no effects
					if (model->flags == 0 && ((effects & EF_NODRAW) || scale <= 0 || alpha <= 0))
						continue;
				}
				else // no model and no effects
					continue;
			}
		}

		if ((val = GETEDICTFIELDVALUE(ent, eval_exteriormodeltoclient)) && val->edict == clentnum)
			flags |= RENDER_EXTERIORMODEL;

		if (ent->v->movetype == MOVETYPE_STEP)
			flags |= RENDER_STEP;
		// don't send an entity if it's coordinates would wrap around
		if ((effects & EF_LOWPRECISION) && origin[0] >= -32768 && origin[1] >= -32768 && origin[2] >= -32768 && origin[0] <= 32767 && origin[1] <= 32767 && origin[2] <= 32767)
			flags |= RENDER_LOWPRECISION;

		s = EntityFrame_NewEntity(&sv_writeentitiestoclient_entityframe, e);
		// if we run out of space, abort
		if (!s)
			break;
		VectorCopy(origin, s->origin);
		VectorCopy(angles, s->angles);
		if (ent->v->colormap >= 1024)
			flags |= RENDER_COLORMAPPED;
		s->colormap = ent->v->colormap;
		s->skin = ent->v->skin;
		s->frame = ent->v->frame;
		s->modelindex = modelindex;
		s->effects = effects;
		s->alpha = alpha;
		s->scale = scale;
		s->glowsize = glowsize;
		s->glowcolor = glowcolor;
		s->flags = flags;
	}
	sv_writeentitiestoclient_entityframe.framenum = ++client->entityframenumber;
	EntityFrame_Write(&client->entitydatabase, &sv_writeentitiestoclient_entityframe, msg);

	if (sv_cullentities_stats.integer)
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d portal %d trace\n", client->name, totalentities, visibleentities, culled_pvs + culled_portal + culled_trace, culled_pvs, culled_portal, culled_trace);
}
#endif

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
void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
	int		bits;
	int		i;
	edict_t	*other;
	int		items;
	eval_t	*val;
	vec3_t	punchvector;
	qbyte	viewzoom;

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
			MSG_WriteDPCoord (msg, other->v->origin[i] + 0.5*(other->v->mins[i] + other->v->maxs[i]));

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
			MSG_WriteAngle (msg, ent->v->angles[i] );
		ent->v->fixangle = 0;
	}

	bits = 0;

	if (ent->v->view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;

	if (ent->v->idealpitch)
		bits |= SU_IDEALPITCH;

// stuff the sigil bits into the high bits of items for sbar, or else
// mix in items2
	val = GETEDICTFIELDVALUE(ent, eval_items2);

	if (val)
		items = (int)ent->v->items | ((int)val->_float << 23);
	else
		items = (int)ent->v->items | ((int)pr_global_struct->serverflags << 28);

	bits |= SU_ITEMS;

	if ( (int)ent->v->flags & FL_ONGROUND)
		bits |= SU_ONGROUND;

	if ( ent->v->waterlevel >= 2)
		bits |= SU_INWATER;

	// dpprotocol
	VectorClear(punchvector);
	if ((val = GETEDICTFIELDVALUE(ent, eval_punchvector)))
		VectorCopy(val->vector, punchvector);

	i = 255;
	if ((val = GETEDICTFIELDVALUE(ent, eval_viewzoom)))
	{
		i = val->_float * 255.0f;
		if (i == 0)
			i = 255;
		else
			i = bound(0, i, 255);
	}
	viewzoom = i;

	if (viewzoom != 255)
		bits |= SU_VIEWZOOM;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->v->punchangle[i])
			bits |= (SU_PUNCH1<<i);
		if (punchvector[i]) // dpprotocol
			bits |= (SU_PUNCHVEC1<<i); // dpprotocol
		if (ent->v->velocity[i])
			bits |= (SU_VELOCITY1<<i);
	}

	if (ent->v->weaponframe)
		bits |= SU_WEAPONFRAME;

	if (ent->v->armorvalue)
		bits |= SU_ARMOR;

	bits |= SU_WEAPON;

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
		MSG_WriteChar (msg, ent->v->view_ofs[2]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, ent->v->idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
			MSG_WritePreciseAngle(msg, ent->v->punchangle[i]); // dpprotocol
		if (bits & (SU_PUNCHVEC1<<i)) // dpprotocol
			MSG_WriteDPCoord(msg, punchvector[i]); // dpprotocol
		if (bits & (SU_VELOCITY1<<i))
			MSG_WriteChar (msg, ent->v->velocity[i]/16);
	}

// [always sent]	if (bits & SU_ITEMS)
	MSG_WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte (msg, ent->v->weaponframe);
	if (bits & SU_ARMOR)
		MSG_WriteByte (msg, ent->v->armorvalue);
	if (bits & SU_WEAPON)
		MSG_WriteByte (msg, SV_ModelIndex(PR_GetString(ent->v->weaponmodel)));

	MSG_WriteShort (msg, ent->v->health);
	MSG_WriteByte (msg, ent->v->currentammo);
	MSG_WriteByte (msg, ent->v->ammo_shells);
	MSG_WriteByte (msg, ent->v->ammo_nails);
	MSG_WriteByte (msg, ent->v->ammo_rockets);
	MSG_WriteByte (msg, ent->v->ammo_cells);

	if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE)
	{
		for(i=0;i<32;i++)
		{
			if ( ((int)ent->v->weapon) & (1<<i) )
			{
				MSG_WriteByte (msg, i);
				break;
			}
		}
	}
	else
	{
		MSG_WriteByte (msg, ent->v->weapon);
	}

	if (bits & SU_VIEWZOOM)
		MSG_WriteByte (msg, viewzoom);
}

/*
=======================
SV_SendClientDatagram
=======================
*/
static qbyte sv_sendclientdatagram_buf[MAX_DATAGRAM]; // FIXME?
qboolean SV_SendClientDatagram (client_t *client)
{
	sizebuf_t	msg;

	msg.data = sv_sendclientdatagram_buf;
	msg.maxsize = sizeof(sv_sendclientdatagram_buf);
	msg.cursize = 0;

	MSG_WriteByte (&msg, svc_time);
	MSG_WriteFloat (&msg, sv.time);

	if (client->spawned)
	{
		// add the client specific data to the datagram
		SV_WriteClientdataToMessage (client->edict, &msg);

		SV_WriteEntitiesToClient (client, client->edict, &msg);

		// copy the server datagram if there is space
		if (msg.cursize + sv.datagram.cursize < msg.maxsize)
			SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);
	}

// send the datagram
	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
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
	int			i, j;
	client_t *client;

// check for changes to be sent over the reliable streams
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (host_client->old_frags != host_client->edict->v->frags)
		{
			for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
			{
				if (!client->active || !client->spawned)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message, host_client->edict->v->frags);
			}

			host_client->old_frags = host_client->edict->v->frags;
		}
	}

	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

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

	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
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
	int			i;

// update frags, names, etc
	SV_UpdateToReliableMessages ();

// build individual updates
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

#ifndef NOROUTINGFIX
		if (host_client->sendserverinfo)
		{
			host_client->sendserverinfo = false;
			SV_SendServerinfo (host_client);
		}
#endif

		if (host_client->spawned)
		{
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

		// check for an overflowed message.  Should only happen
		// on a very fucked up connection that backs up a lot, then
		// changes level
		if (host_client->message.overflowed)
		{
			SV_DropClient (true); // overflowed
			host_client->message.overflowed = false;
			continue;
		}

		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NET_CanSendMessage (host_client->netconnection))
				continue;

			if (host_client->dropasap)
				SV_DropClient (false);	// went to another level
			else
			{
				if (NET_SendMessage (host_client->netconnection, &host_client->message) == -1)
					SV_DropClient (true);	// if the message couldn't send, kick off
				SZ_Clear (&host_client->message);
				host_client->last_message = realtime;
				host_client->sendsignon = false;
			}
		}
	}


// clear muzzle flashes
	SV_CleanupEnts ();
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
int SV_ModelIndex (const char *name)
{
	int i;

	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
		Host_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

#ifdef SV_QUAKEENTITIES
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
		ClearStateToDefault(&svent->baseline);

		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !svent->v->modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->v->origin, svent->baseline.origin);
		VectorCopy (svent->v->angles, svent->baseline.angles);
		svent->baseline.frame = svent->v->frame;
		svent->baseline.skin = svent->v->skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = svent->v->modelindex;
		}

		large = false;
		if (svent->baseline.modelindex & 0xFF00 || svent->baseline.frame & 0xFF00)
			large = true;

		// add to the message
		if (large)
			MSG_WriteByte (&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		MSG_WriteShort (&sv.signon, entnum);

		if (large)
		{
			MSG_WriteShort (&sv.signon, svent->baseline.modelindex);
			MSG_WriteShort (&sv.signon, svent->baseline.frame);
		}
		else
		{
			MSG_WriteByte (&sv.signon, svent->baseline.modelindex);
			MSG_WriteByte (&sv.signon, svent->baseline.frame);
		}
		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteDPCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
		}
	}
}
#endif


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
	NET_SendToAll (&msg, 5);

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

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
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

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		Cvar_Set ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	Con_DPrintf ("SpawnServer: %s\n",server);
	svs.changelevel_issued = false;		// now safe to issue another

//
// tell all connected clients that we are going to a new level
//
	if (sv.active)
		SV_SendReconnect ();

//
// make cvars consistant
//
	if (coop.integer)
		Cvar_SetValue ("deathmatch", 0);
	current_skill = bound(0, (int)(skill.value + 0.5), 3);

	Cvar_SetValue ("skill", (float)current_skill);

//
// set up the new server
//
	Host_ClearMemory ();

	memset (&sv, 0, sizeof(sv));

	strcpy (sv.name, server);

// load progs to get entity field count
	PR_LoadProgs ();

// allocate server memory
	sv.max_edicts = MAX_EDICTS;

	// clear the edict memory pool
	Mem_EmptyPool(sv_edicts_mempool);
	// edict_t structures (hidden from progs)
	sv.edicts = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_t));
	// progs fields, often accessed by server
	sv.edictsfields = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * pr_edict_size);
	// table of edict pointers, for quicker lookup of edicts
	sv.edictstable = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_t *));
	// used by PushMove to move back pushed entities
	sv.moved_edicts = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_t *));
	for (i = 0;i < sv.max_edicts;i++)
	{
		ent = sv.edicts + i;
		ent->v = (void *)((qbyte *)sv.edictsfields + i * pr_edict_size);
		sv.edictstable[i] = ent;
	}

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
	for (i=0 ; i<svs.maxclients ; i++)
	{
		ent = EDICT_NUM(i+1);
		svs.clients[i].edict = ent;
	}

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	Mod_ClearUsed();

	strcpy (sv.name, server);
	sprintf (sv.modelname,"maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName(sv.modelname, false, true, true);
	if (!sv.worldmodel)
	{
		Con_Printf ("Couldn't spawn server %s\n", sv.modelname);
		sv.active = false;
		return;
	}
	sv.models[1] = sv.worldmodel;

//
// clear world interaction links
//
	SV_ClearWorld ();

	sv.sound_precache[0] = "";

	sv.model_precache[0] = "";
	sv.model_precache[1] = sv.modelname;
	for (i = 1;i < sv.worldmodel->numsubmodels;i++)
	{
		sv.model_precache[i+1] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false, false, false);
	}

//
// load the rest of the entities
//
	ent = EDICT_NUM(0);
	memset (ent->v, 0, progs->entityfields * 4);
	ent->free = false;
	ent->v->model = PR_SetString(sv.modelname);
	ent->v->modelindex = 1;		// world model
	ent->v->solid = SOLID_BSP;
	ent->v->movetype = MOVETYPE_PUSH;

	if (coop.integer)
		pr_global_struct->coop = coop.integer;
	else
		pr_global_struct->deathmatch = deathmatch.integer;

	pr_global_struct->mapname = PR_SetString(sv.name);

// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;

	// load replacement entity file if found
	entities = NULL;
	if (sv_entpatch.integer)
		entities = FS_LoadFile(va("maps/%s.ent", sv.name), true);
	if (entities)
	{
		Con_Printf("Loaded maps/%s.ent\n", sv.name);
		ED_LoadFromFile (entities);
		Mem_Free(entities);
	}
	else
		ED_LoadFromFile (sv.worldmodel->entities);


	// LordHavoc: clear world angles (to fix e3m3.bsp)
	VectorClear(sv.edicts->v->angles);

	sv.active = true;

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

// run two frames to allow everything to settle
	sv.frametime = pr_global_struct->frametime = host_frametime = 0.1;
	SV_Physics ();
	sv.frametime = pr_global_struct->frametime = host_frametime = 0.1;
	SV_Physics ();

	Mod_PurgeUnused();

#ifdef QUAKEENTITIES
// create a baseline for more efficient communications
	SV_CreateBaseline ();
#endif

// send serverinfo to all connected clients
	for (i=0,host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo (host_client);

	Con_DPrintf ("Server spawned.\n");
	NET_Heartbeat (2);
}

