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

static cvar_t sv_cullentities_pvs = {0, "sv_cullentities_pvs", "1"}; // fast but loose
static cvar_t sv_cullentities_trace = {0, "sv_cullentities_trace", "0"}; // tends to get false negatives, uses a timeout to keep entities visible a short time after becoming hidden
static cvar_t sv_cullentities_stats = {0, "sv_cullentities_stats", "0"};
static cvar_t sv_entpatch = {0, "sv_entpatch", "1"};

server_t sv;
server_static_t svs;

static char localmodels[MAX_MODELS][5];			// inline model names for precache

mempool_t *sv_edicts_mempool = NULL;

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
	int i;

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

	SV_Phys_Init();
	SV_World_Init();

	for (i = 0;i < MAX_MODELS;i++)
		sprintf (localmodels[i], "*%i", i);

	sv_edicts_mempool = Mem_AllocPool("server edicts");
}

static void SV_SaveEntFile_f(void)
{
	char basename[MAX_QPATH];
	if (!sv.active || !sv.worldmodel)
	{
		Con_Printf("Not running a server\n");
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
	if (modelindex >= 256 || startframe >= 256)
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-19)
			return;
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
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-17)
			return;
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
	for (sound_num=1 ; sound_num<MAX_SOUNDS && sv.sound_precache[sound_num] ; sound_num++)
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
	char			message[128];

	// edicts get reallocated on level changes, so we need to update it here
	client->edict = EDICT_NUM(client->number + 1);

	// LordHavoc: clear entityframe tracking
	client->entityframenumber = 0;
	if (client->entitydatabase4)
		EntityFrame4_FreeDatabase(client->entitydatabase4);
	client->entitydatabase4 = EntityFrame4_AllocDatabase(sv_clients_mempool);

	MSG_WriteByte (&client->message, svc_print);
	snprintf (message, sizeof (message), "\002\nServer: %s build %s (progs %i crc)", gamename, buildstring, pr_crc);
	MSG_WriteString (&client->message,message);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, PROTOCOL_DARKPLACES4);
	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop.integer && deathmatch.integer)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	MSG_WriteString (&client->message,PR_GetString(sv.edicts->v->message));

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

	Con_DPrintf("Client %s connected\n", client->netconnection->address);

	strcpy(client->name, "unconnected");
	strcpy(client->old_name, "unconnected");
	client->number = clientnum;
	client->spawned = false;
	client->edict = EDICT_NUM(clientnum+1);
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

	SV_SendServerinfo (client);
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

/*
=============
SV_WriteEntitiesToClient

=============
*/
#ifdef QUAKEENTITIES
void SV_WriteEntitiesToClient (client_t *client, edict_t *clent, sizebuf_t *msg)
{
	int e, clentnum, bits, alpha, glowcolor, glowsize, scale, effects, lightsize;
	int culled_pvs, culled_trace, visibleentities, totalentities;
	qbyte *pvs;
	vec3_t origin, angles, entmins, entmaxs, testorigin, testeye;
	float nextfullupdate, alphaf;
	edict_t *ent;
	eval_t *val;
	entity_state_t *baseline; // LordHavoc: delta or startup baseline
	model_t *model;

	Mod_CheckLoaded(sv.worldmodel);

// find the client's PVS
	VectorAdd (clent->v->origin, clent->v->view_ofs, testeye);
	fatbytes = 0;
	if (sv.worldmodel && sv.worldmodel->brush.FatPVS)
		fatbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, testeye, 8, sv_writeentitiestoclient_pvs, sizeof(sv_writeentitiestoclient_pvs));

	culled_pvs = 0;
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
		VectorCopy(ent->v->origin, origin);

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
			if (sv_cullentities_pvs.integer && fatbytes && sv.worldmodel && sv.worldmodel->brush.BoxTouchingPVS && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, sv_writeentitiestoclient_pvs, entmins, entmaxs))
			{
				culled_pvs++;
				continue;
			}

			// don't try to cull embedded brush models with this, they're sometimes huge (spanning several rooms)
			if (sv_cullentities_trace.integer && (model == NULL || model->name[0] != '*'))
			{
				// LordHavoc: test random offsets, to maximize chance of detection
				testorigin[0] = lhrandom(entmins[0], entmaxs[0]);
				testorigin[1] = lhrandom(entmins[1], entmaxs[1]);
				testorigin[2] = lhrandom(entmins[2], entmaxs[2]);

				sv.worldmodel->TraceBox(sv.worldmodel, 0, &trace, testeye, testeye, testorigin, testorigin, SUPERCONTENTS_SOLID);
				if (trace.fraction == 1)
					client->visibletime[e] = realtime + 1;
				else
				{
					//test nearest point on bbox
					testorigin[0] = bound(entmins[0], testeye[0], entmaxs[0]);
					testorigin[1] = bound(entmins[1], testeye[1], entmaxs[1]);
					testorigin[2] = bound(entmins[2], testeye[2], entmaxs[2]);

					sv.worldmodel->TraceBox(sv.worldmodel, 0, &trace, testeye, testeye, testorigin, testorigin, SUPERCONTENTS_SOLID);
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
		baseline = &ent->e->baseline;

		if (((int)ent->v->effects & EF_DELTA) && sv_deltacompress.integer)
		{
			// every half second a full update is forced
			if (realtime < client->nextfullupdate[e])
			{
				bits |= U_DELTA;
				baseline = &ent->e->deltabaseline;
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
		VectorCopy(ent->v->origin, ent->e->deltabaseline.origin);
		VectorCopy(ent->v->angles, ent->e->deltabaseline.angles);
		ent->e->deltabaseline.colormap = ent->v->colormap;
		ent->e->deltabaseline.skin = ent->v->skin;
		ent->e->deltabaseline.frame = ent->v->frame;
		ent->e->deltabaseline.effects = ent->v->effects;
		ent->e->deltabaseline.modelindex = ent->v->modelindex;
		ent->e->deltabaseline.alpha = alpha;
		ent->e->deltabaseline.scale = scale;
		ent->e->deltabaseline.glowsize = glowsize;
		ent->e->deltabaseline.glowcolor = glowcolor;

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
		if (bits & U_MODEL)		MSG_WriteByte(msg,	ent->v->modelindex);
		if (bits & U_FRAME)		MSG_WriteByte(msg, ent->v->frame);
		if (bits & U_COLORMAP)	MSG_WriteByte(msg, ent->v->colormap);
		if (bits & U_SKIN)		MSG_WriteByte(msg, ent->v->skin);
		if (bits & U_EFFECTS)	MSG_WriteByte(msg, ent->v->effects);
		if (bits & U_ORIGIN1)	MSG_WriteDPCoord(msg, origin[0]);
		if (bits & U_ANGLE1)	MSG_WriteAngle(msg, angles[0]);
		if (bits & U_ORIGIN2)	MSG_WriteDPCoord(msg, origin[1]);
		if (bits & U_ANGLE2)	MSG_WriteAngle(msg, angles[1]);
		if (bits & U_ORIGIN3)	MSG_WriteDPCoord(msg, origin[2]);
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
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d trace\n", client->name, totalentities, visibleentities, culled_pvs + culled_trace, culled_pvs, culled_trace);
}
#else
static int numsendentities;
static entity_state_t sendentities[MAX_EDICTS];
static entity_state_t *sendentitiesindex[MAX_EDICTS];

void SV_PrepareEntitiesForSending(void)
{
	int e, i;
	float f;
	edict_t *ent;
	entity_state_t cs;
	// send all entities that touch the pvs
	numsendentities = 0;
	sendentitiesindex[0] = NULL;
	for (e = 1, ent = NEXT_EDICT(sv.edicts);e < sv.num_edicts;e++, ent = NEXT_EDICT(ent))
	{
		sendentitiesindex[e] = NULL;
		if (ent->e->free)
			continue;

		ClearStateToDefault(&cs);
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

		cs.specialvisibilityradius = 0;
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
		// always send world submodels, they don't generate much traffic
		else if ((model = sv.models[s->modelindex]) == NULL || model->name[0] != '*')
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
			lightmaxs[0] = min(entmaxs[0], s->origin[0] + s->specialvisibilityradius);
			lightmaxs[1] = min(entmaxs[1], s->origin[1] + s->specialvisibilityradius);
			lightmaxs[2] = min(entmaxs[2], s->origin[2] + s->specialvisibilityradius);
			sv_writeentitiestoclient_totalentities++;
			// if not touching a visible leaf
			if (sv_cullentities_pvs.integer && sv_writeentitiestoclient_pvsbytes && sv.worldmodel && sv.worldmodel->brush.BoxTouchingPVS && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, sv_writeentitiestoclient_pvs, lightmins, lightmaxs))
			{
				sv_writeentitiestoclient_culled_pvs++;
				return;
			}
			// or not seen by random tracelines
			if (sv_cullentities_trace.integer)
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

void SV_WriteEntitiesToClient(client_t *client, edict_t *clent, sizebuf_t *msg)
{
	int i;
	vec3_t testorigin;
	entity_state_t *s;
	entity_database4_t *d;
	int maxbytes, n, startnumber;
	entity_state_t *e, inactiveentitystate;
	sizebuf_t buf;
	qbyte data[128];
	// prepare the buffer
	memset(&buf, 0, sizeof(buf));
	buf.data = data;
	buf.maxsize = sizeof(data);

	d = client->entitydatabase4;

	for (i = 0;i < MAX_ENTITY_HISTORY;i++)
		if (!d->commit[i].numentities)
			break;
	// if commit buffer full, just don't bother writing an update this frame
	if (i == MAX_ENTITY_HISTORY)
		return;
	d->currentcommit = d->commit + i;

	// this state's number gets played around with later
	ClearStateToDefault(&inactiveentitystate);
	//inactiveentitystate = defaultstate;

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

	// the place being reported (to consider the fact the client still
	// applies the view_ofs[2], so we have to only send the fractional part
	// of view_ofs[2], undoing what the client will redo)
	VectorCopy(sv_writeentitiestoclient_testeye, testorigin);
	i = (int) clent->v->view_ofs[2] & 255;
	if (i >= 128)
		i -= 256;
	testorigin[2] -= (float) i;

	for (i = 0;i < numsendentities;i++)
		SV_MarkWriteEntityStateToClient(sendentities + i);

	// calculate maximum bytes to allow in this packet
	// deduct 4 to account for the end data
	maxbytes = min(msg->maxsize, MAX_PACKETFRAGMENT) - 4;

	d->currentcommit->numentities = 0;
	d->currentcommit->framenum = ++client->entityframenumber;
	MSG_WriteByte(msg, svc_entities);
	MSG_WriteLong(msg, d->referenceframenum);
	MSG_WriteLong(msg, d->currentcommit->framenum);
	if (developer_networkentities.integer >= 1)
	{
		Con_Printf("send svc_entities ref:%i num:%i (database: ref:%i commits:", d->referenceframenum, d->currentcommit->framenum, d->referenceframenum);
		for (i = 0;i < MAX_ENTITY_HISTORY;i++)
			if (d->commit[i].numentities)
				Con_Printf(" %i", d->commit[i].framenum);
		Con_Printf(")\n");
	}
	if (d->currententitynumber >= sv.max_edicts)
		startnumber = 1;
	else
		startnumber = bound(1, d->currententitynumber, sv.max_edicts - 1);
	MSG_WriteShort(msg, startnumber);
	// reset currententitynumber so if the loop does not break it we will
	// start at beginning next frame (if it does break, it will set it)
	d->currententitynumber = 1;
	for (i = 0, n = startnumber;n < sv.max_edicts;n++)
	{
		// find the old state to delta from
		e = EntityFrame4_GetReferenceEntity(d, n);
		// prepare the buffer
		SZ_Clear(&buf);
		// make the message
		if (sententities[n] == sententitiesmark)
		{
			// entity exists, build an update (if empty there is no change)
			// find the state in the list
			for (;i < numsendentities && sendentities[i].number < n;i++);
			s = sendentities + i;
			if (s->number != n)
				Sys_Error("SV_WriteEntitiesToClient: s->number != n\n");
			// build the update
			if (s->exteriormodelforclient && s->exteriormodelforclient == sv_writeentitiestoclient_clentnum)
			{
				s->flags |= RENDER_EXTERIORMODEL;
				EntityState_Write(s, &buf, e);
				s->flags &= ~RENDER_EXTERIORMODEL;
			}
			else
				EntityState_Write(s, &buf, e);
		}
		else
		{
			s = &inactiveentitystate;
			s->number = n;
			if (e->active)
			{
				// entity used to exist but doesn't anymore, send remove
				MSG_WriteShort(&buf, n | 0x8000);
			}
		}
		// if the commit is full, we're done this frame
		if (msg->cursize + buf.cursize > maxbytes)
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

	if (sv_cullentities_stats.integer)
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d trace\n", client->name, sv_writeentitiestoclient_totalentities, sv_writeentitiestoclient_visibleentities, sv_writeentitiestoclient_culled_pvs + sv_writeentitiestoclient_culled_trace, sv_writeentitiestoclient_culled_pvs, sv_writeentitiestoclient_culled_trace);
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

	// PROTOCOL_DARKPLACES
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
		if (punchvector[i]) // PROTOCOL_DARKPLACES
			bits |= (SU_PUNCHVEC1<<i); // PROTOCOL_DARKPLACES
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
			MSG_WritePreciseAngle(msg, ent->v->punchangle[i]); // PROTOCOL_DARKPLACES
		if (bits & (SU_PUNCHVEC1<<i)) // PROTOCOL_DARKPLACES
			MSG_WriteDPCoord(msg, punchvector[i]); // PROTOCOL_DARKPLACES
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

	if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE || gamemode == GAME_NEXUIZ)
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

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client->edict, &msg);

	SV_WriteEntitiesToClient (client, client->edict, &msg);

	// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
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
	char *s;

// check for changes to be sent over the reliable streams
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		// update the host_client fields we care about according to the entity fields
		sv_player = EDICT_NUM(i+1);
		s = PR_GetString(sv_player->v->netname);
		if (s != host_client->name)
		{
			if (s == NULL)
				s = "";
			// point the string back at host_client->name to keep it safe
			strlcpy (host_client->name, s, sizeof (host_client->name));
			sv_player->v->netname = PR_SetString(host_client->name);
		}
		if ((val = GETEDICTFIELDVALUE(sv_player, eval_clientcolors)) && host_client->colors != val->_float)
			host_client->colors = val->_float;
		host_client->frags = sv_player->v->frags;
		if (gamemode == GAME_NEHAHRA)
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_pmodel)) && host_client->pmodel != val->_float)
				host_client->pmodel = val->_float;

		// if the fields changed, send messages about the changes
		if (strcmp(host_client->old_name, host_client->name))
		{
			strcpy(host_client->old_name, host_client->name);
			for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
			{
				if (!client->spawned || !client->netconnection)
					continue;
				MSG_WriteByte (&client->message, svc_updatename);
				MSG_WriteByte (&client->message, i);
				MSG_WriteString (&client->message, host_client->name);
			}
		}
		if (host_client->old_colors != host_client->colors)
		{
			host_client->old_colors = host_client->colors;
			for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
			{
				if (!client->spawned || !client->netconnection)
					continue;
				MSG_WriteByte (&client->message, svc_updatecolors);
				MSG_WriteByte (&client->message, i);
				MSG_WriteByte (&client->message, host_client->colors);
			}
		}
		if (host_client->old_frags != host_client->frags)
		{
			host_client->old_frags = host_client->frags;
			for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
			{
				if (!client->spawned || !client->netconnection)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message, host_client->frags);
			}
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
		ClearStateToDefault(&svent->e->baseline);

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
			svent->e->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
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
			MSG_WriteDPCoord(&sv.signon, svent->e->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->e->baseline.angles[i]);
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
	sv.edictsengineprivate = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_engineprivate_t));
	sv.edictsfields = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * pr_edict_size);
	sv.moved_edicts = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_t *));

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

	Mem_Free(oldedictsengineprivate);
	Mem_Free(oldedictsfields);
	Mem_Free(oldmoved_edicts);
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
	current_skill = bound(0, (int)(skill.value + 0.5), 3);

	Cvar_SetValue ("skill", (float)current_skill);

//
// set up the new server
//
	Host_ClearMemory ();

	memset (&sv, 0, sizeof(sv));

	strlcpy (sv.name, server, sizeof (sv.name));

// load progs to get entity field count
	PR_LoadProgs ();

// allocate server memory
	// start out with just enough room for clients and a reasonable estimate of entities
	sv.max_edicts = max(svs.maxclients + 1, 512);
	sv.max_edicts = min(sv.max_edicts, MAX_EDICTS);

	// clear the edict memory pool
	Mem_EmptyPool(sv_edicts_mempool);
	// edict_t structures (hidden from progs)
	sv.edicts = Mem_Alloc(sv_edicts_mempool, MAX_EDICTS * sizeof(edict_t));
	// engine private structures (hidden from progs)
	sv.edictsengineprivate = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_engineprivate_t));
	// progs fields, often accessed by server
	sv.edictsfields = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * pr_edict_size);
	// used by PushMove to move back pushed entities
	sv.moved_edicts = Mem_Alloc(sv_edicts_mempool, sv.max_edicts * sizeof(edict_t *));
	for (i = 0;i < sv.max_edicts;i++)
	{
		ent = sv.edicts + i;
		ent->e = sv.edictsengineprivate + i;
		ent->v = (void *)((qbyte *)sv.edictsfields + i * pr_edict_size);
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

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	Mod_ClearUsed();

	strlcpy (sv.name, server, sizeof (sv.name));
	snprintf (sv.modelname, sizeof (sv.modelname), "maps/%s.bsp", server);
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
	for (i = 1;i < sv.worldmodel->brush.numsubmodels;i++)
	{
		sv.model_precache[i+1] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false, false, false);
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
		entities = FS_LoadFile(va("maps/%s.ent", sv.name), true);
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

#ifdef QUAKEENTITIES
// create a baseline for more efficient communications
	SV_CreateBaseline ();
#endif

// send serverinfo to all connected clients
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		if (host_client->netconnection)
			SV_SendServerinfo(host_client);

	Con_DPrintf ("Server spawned.\n");
	NetConn_Heartbeat (2);
}

