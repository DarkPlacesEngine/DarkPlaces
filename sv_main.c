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

server_t		sv;
server_static_t	svs;

char	localmodels[MAX_MODELS][5];			// inline model names for precache

//============================================================================

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	int		i;
	extern	cvar_t	sv_maxvelocity;
	extern	cvar_t	sv_gravity;
	extern	cvar_t	sv_nostep;
	extern	cvar_t	sv_friction;
	extern	cvar_t	sv_edgefriction;
	extern	cvar_t	sv_stopspeed;
	extern	cvar_t	sv_maxspeed;
	extern	cvar_t	sv_accelerate;
	extern	cvar_t	sv_idealpitchscale;
	extern	cvar_t	sv_aim;

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

	for (i=0 ; i<MAX_MODELS ; i++)
		sprintf (localmodels[i], "*%i", i);
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
	MSG_WriteCoord (&sv.datagram, org[0]);
	MSG_WriteCoord (&sv.datagram, org[1]);
	MSG_WriteCoord (&sv.datagram, org[2]);
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
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

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
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %i", channel);

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

	channel = (ent<<3) | channel;

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;

// directed messages go only to the entity the are targeted on
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, attenuation*64);
	MSG_WriteShort (&sv.datagram, channel);
	MSG_WriteByte (&sv.datagram, sound_num);
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord (&sv.datagram, entity->v.origin[i]+0.5*(entity->v.mins[i]+entity->v.maxs[i]));
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

	MSG_WriteByte (&client->message, svc_print);
#ifdef NEHAHRA
	sprintf (message, "%c\nDPNEHAHRA VERSION %4.2f SERVER (%i CRC)", 2, DP_VERSION, pr_crc);
#else
	sprintf (message, "%c\nDARKPLACES VERSION %4.2f SERVER (%i CRC)", 2, DP_VERSION, pr_crc);
#endif
	MSG_WriteString (&client->message,message);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, PROTOCOL_VERSION);
	MSG_WriteByte (&client->message, svs.maxclients);

	if (!coop.value && deathmatch.value)
		MSG_WriteByte (&client->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->message, GAME_COOP);

	sprintf (message, pr_strings+sv.edicts->v.message);

	MSG_WriteString (&client->message,message);

	for (s = sv.model_precache+1 ; *s ; s++)
		MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);

	for (s = sv.sound_precache+1 ; *s ; s++)
		MSG_WriteString (&client->message, *s);
	MSG_WriteByte (&client->message, 0);

// send music
	MSG_WriteByte (&client->message, svc_cdtrack);
	MSG_WriteByte (&client->message, sv.edicts->v.sounds);
	MSG_WriteByte (&client->message, sv.edicts->v.sounds);

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

#ifdef IDGODS
	client->privileged = IsID(&client->netconnection->addr);
#else	
	client->privileged = false;				
#endif

	if (sv.loadgame)
		memcpy (client->spawn_parms, spawn_parms, sizeof(spawn_parms));
	else
	{
	// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram (pr_global_struct->SetNewParms);
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
	}

	SV_SendServerinfo (client);
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
byte	fatpvs[MAX_MAP_LEAFS/8];

void SV_AddToFatPVS (vec3_t org, mnode_t *node)
{
	int		i;
	byte	*pvs;
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
byte *SV_FatPVS (vec3_t org)
{
	fatbytes = (sv.worldmodel->numleafs+31)>>3;
	memset (fatpvs, 0, fatbytes);
	SV_AddToFatPVS (org, sv.worldmodel->nodes);
	return fatpvs;
}

//=============================================================================


/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient (edict_t	*clent, sizebuf_t *msg)
{
	int		e, i, clentnum, bits, alpha, glowcolor, glowsize, scale, colormod, modred, modgreen, modblue, dodelta, effects;
	byte	*pvs;
	vec3_t	org, origin, angles;
	float	movelerp, moveilerp;
	edict_t	*ent;
	eval_t  *val;
	entity_state_t *baseline; // LordHavoc: delta or startup baseline

// find the client's PVS
	VectorAdd (clent->v.origin, clent->v.view_ofs, org);
	pvs = SV_FatPVS (org);

	clentnum = NUM_FOR_EDICT(clent); // LordHavoc: for comparison purposes
// send over all entities (except the client) that touch the pvs
	ent = NEXT_EDICT(sv.edicts);
	for (e=1 ; e<sv.num_edicts ; e++, ent = NEXT_EDICT(ent))
	{
		bits = 0;
		if (ent != clent) // LordHavoc: always send player
		{
			if ((val = GETEDICTFIELDVALUE(ent, eval_viewmodelforclient)) && val->edict)
			{
				if (val->edict != clentnum)
					continue; // don't show to anyone else
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
				// ignore if not touching a PV leaf
				for (i=0 ; i < ent->num_leafs ; i++)
					if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i]&7) ))
						break;
					
				if (i == ent->num_leafs)
					continue;		// not visible
			}
		}

		// don't send if flagged for NODRAW and there are no effects
		alpha = 255;
		scale = 16;
		glowsize = 0;
		glowcolor = 254;
		colormod = 255;
		effects = ent->v.effects;

		if (val = GETEDICTFIELDVALUE(ent, eval_alpha))
		if ((alpha = (int) (val->_float * 255.0)) == 0)
			alpha = 255;
		if (val = GETEDICTFIELDVALUE(ent, eval_renderamt)) // HalfLife support
			alpha -= (int) val->_float;
		if (alpha < 0) alpha = 0;
		if (alpha > 255) alpha = 255;

		if (val = GETEDICTFIELDVALUE(ent, eval_glow_size))
			glowsize = (int) val->_float >> 3;
		if (glowsize > 127) glowsize = 127;
		if (glowsize < -128) glowsize = -128;

		if (val = GETEDICTFIELDVALUE(ent, eval_scale))
		if ((scale = (int) (val->_float * 16.0)) == 0) scale = 16;
		if (scale < 0) scale = 0;
		if (scale > 255) scale = 255;

		if (val = GETEDICTFIELDVALUE(ent, eval_glow_trail))
		if (val->_float != 0)
			bits |= U_GLOWTRAIL;

		if (val = GETEDICTFIELDVALUE(ent, eval_glow_color))
		if (val->_float != 0)
			glowcolor = (int) val->_float;

		if (val = GETEDICTFIELDVALUE(ent, eval_fullbright))
		if (val->_float != 0)
			effects |= EF_FULLBRIGHT;

		if (val = GETEDICTFIELDVALUE(ent, eval_colormod))
		if (val->vector[0] != 0 || val->vector[1] != 0 || val->vector[2] != 0)
		{
			modred = val->vector[0] * 8.0;if (modred < 0) modred = 0;if (modred > 7) modred = 7;
			modgreen = val->vector[1] * 8.0;if (modgreen < 0) modgreen = 0;if (modgreen > 7) modgreen = 7;
			modblue = val->vector[2] * 4.0;if (modblue < 0) modblue = 0;if (modblue > 3) modblue = 3;
			colormod = (modred << 5) | (modgreen << 2) | modblue;
		}

		if (ent != clent)
		{
			if (glowsize == 0 && bits == 0) // no effects
			{
				if (ent->v.modelindex && pr_strings[ent->v.model]) // model
				{
					if (sv.models[ (int)ent->v.modelindex ]->flags == 0 && (ent->v.effects == EF_NODRAW || scale <= 0 || alpha <= 0))
						continue;
				}
				else // no model and no effects
					continue;
			}
		}

		if (msg->maxsize - msg->cursize < 32) // LordHavoc: increased check from 16 to 32
		{
			Con_Printf ("packet overflow\n");
			return;
		}

// send an update
		bits = 0;

		dodelta = FALSE;
		if ((int)ent->v.effects & EF_DELTA)
			dodelta = cl.time < ent->nextfullupdate; // every half second a full update is forced

		if (dodelta)
		{
			bits |= U_DELTA;
			baseline = &ent->deltabaseline;
		}
		else
		{
			ent->nextfullupdate = cl.time + 0.5;
			baseline = &ent->baseline;
		}

		if (e >= 256)
			bits |= U_LONGENTITY;
		if (ent->v.movetype == MOVETYPE_STEP)
			bits |= U_STEP;
		
		if (ent->v.movetype == MOVETYPE_STEP && ((int) ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM))) // monsters have smoothed walking/flying/swimming movement
		{
			if (!ent->steplerptime || ent->steplerptime > sv.time) // when the level just started...
			{
				ent->steplerptime = sv.time;
				VectorCopy(ent->v.origin, ent->stepoldorigin);
				VectorCopy(ent->v.angles, ent->stepoldangles);
				VectorCopy(ent->v.origin, ent->steporigin);
				VectorCopy(ent->v.angles, ent->stepangles);
			}
			VectorSubtract(ent->v.origin, ent->steporigin, origin);
			VectorSubtract(ent->v.angles, ent->stepangles, angles);
			if (DotProduct(origin, origin) >= 0.125 || DotProduct(angles, angles) >= 1.4)
			{
				// update lerp positions
				ent->steplerptime = sv.time;
				VectorCopy(ent->steporigin, ent->stepoldorigin);
				VectorCopy(ent->stepangles, ent->stepoldangles);
				VectorCopy(ent->v.origin, ent->steporigin);
				VectorCopy(ent->v.angles, ent->stepangles);
			}
			movelerp = (sv.time - ent->steplerptime) * 10.0;
			if (movelerp > 1) movelerp = 1;
			moveilerp = 1 - movelerp;
			origin[0] = ent->stepoldorigin[0] * moveilerp + ent->steporigin[0] * movelerp;
			origin[1] = ent->stepoldorigin[1] * moveilerp + ent->steporigin[1] * movelerp;
			origin[2] = ent->stepoldorigin[2] * moveilerp + ent->steporigin[2] * movelerp;
			// choose shortest rotate (to avoid 'spin around' situations)
			VectorSubtract(ent->stepangles, ent->stepoldangles, angles);
			if (angles[0] < -180) angles[0] += 360;if (angles[0] >= 180) angles[0] -= 360;
			if (angles[1] < -180) angles[1] += 360;if (angles[1] >= 180) angles[1] -= 360;
			if (angles[2] < -180) angles[2] += 360;if (angles[2] >= 180) angles[2] -= 360;
			angles[0] = angles[0] * movelerp + ent->stepoldangles[0];
			angles[1] = angles[1] * movelerp + ent->stepoldangles[1];
			angles[2] = angles[2] * movelerp + ent->stepoldangles[2];
		}
		else // copy as they are
		{
			VectorCopy(ent->v.origin, origin);
			VectorCopy(ent->v.angles, angles);
			if (ent->v.movetype == MOVETYPE_STEP) // monster, but airborn, update lerp info
			{
				// update lerp positions
				ent->steplerptime = sv.time;
				VectorCopy(ent->v.origin, ent->stepoldorigin);
				VectorCopy(ent->v.angles, ent->stepoldangles);
				VectorCopy(ent->v.origin, ent->steporigin);
				VectorCopy(ent->v.angles, ent->stepangles);
			}
		}

		// LordHavoc: old stuff, but rewritten to have more exact tolerances
		if ((int)(origin[0]*8.0) != (int)(baseline->origin[0]*8.0))						bits |= U_ORIGIN1;
		if ((int)(origin[1]*8.0) != (int)(baseline->origin[1]*8.0))						bits |= U_ORIGIN2;
		if ((int)(origin[2]*8.0) != (int)(baseline->origin[2]*8.0))						bits |= U_ORIGIN3;
		if ((int)(angles[0]*(256.0/360.0)) != (int)(baseline->angles[0]*(256.0/360.0)))	bits |= U_ANGLE1;
		if ((int)(angles[1]*(256.0/360.0)) != (int)(baseline->angles[1]*(256.0/360.0)))	bits |= U_ANGLE2;
		if ((int)(angles[2]*(256.0/360.0)) != (int)(baseline->angles[2]*(256.0/360.0)))	bits |= U_ANGLE3;
		if (baseline->colormap != (int) ent->v.colormap)								bits |= U_COLORMAP;
		if (baseline->skin != (int) ent->v.skin)										bits |= U_SKIN;
		if ((baseline->frame & 0x00FF) != ((int) ent->v.frame & 0x00FF))				bits |= U_FRAME;
		if ((baseline->effects & 0x00FF) != ((int) ent->v.effects & 0x00FF))			bits |= U_EFFECTS;
		if (baseline->modelindex != (int) ent->v.modelindex)							bits |= U_MODEL;

		// LordHavoc: new stuff
		if (baseline->alpha != alpha)													bits |= U_ALPHA;
		if (baseline->scale != scale)													bits |= U_SCALE;
		if (((int) baseline->effects & 0xFF00) != ((int) ent->v.effects & 0xFF00))		bits |= U_EFFECTS2;
		if (baseline->glowsize != glowsize)												bits |= U_GLOWSIZE;
		if (baseline->glowcolor != glowcolor)											bits |= U_GLOWCOLOR;
		if (baseline->colormod != colormod)												bits |= U_COLORMOD;
		if (((int) baseline->frame & 0xFF00) != ((int) ent->v.frame & 0xFF00))			bits |= U_FRAME2;

		// update delta baseline
		VectorCopy(ent->v.origin, ent->deltabaseline.origin);
		VectorCopy(ent->v.angles, ent->deltabaseline.angles);
		ent->deltabaseline.colormap = ent->v.colormap;
		ent->deltabaseline.skin = ent->v.skin;
		ent->deltabaseline.frame = ent->v.frame;
		ent->deltabaseline.effects = ent->v.effects;
		ent->deltabaseline.modelindex = ent->v.modelindex;
		ent->deltabaseline.alpha = alpha;
		ent->deltabaseline.scale = scale;
		ent->deltabaseline.glowsize = glowsize;
		ent->deltabaseline.glowcolor = glowcolor;
		ent->deltabaseline.colormod = colormod;

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
		if (bits & U_MODEL)		MSG_WriteByte (msg,	ent->v.modelindex);
		if (bits & U_FRAME)		MSG_WriteByte (msg, ent->v.frame);
		if (bits & U_COLORMAP)	MSG_WriteByte (msg, ent->v.colormap);
		if (bits & U_SKIN)		MSG_WriteByte (msg, ent->v.skin);
		if (bits & U_EFFECTS)	MSG_WriteByte (msg, ent->v.effects);
		if (bits & U_ORIGIN1)	MSG_WriteCoord (msg, origin[0]);		
		if (bits & U_ANGLE1)	MSG_WriteAngle(msg, angles[0]);
		if (bits & U_ORIGIN2)	MSG_WriteCoord (msg, origin[1]);
		if (bits & U_ANGLE2)	MSG_WriteAngle(msg, angles[1]);
		if (bits & U_ORIGIN3)	MSG_WriteCoord (msg, origin[2]);
		if (bits & U_ANGLE3)	MSG_WriteAngle(msg, angles[2]);

		// LordHavoc: new stuff
		if (bits & U_ALPHA)		MSG_WriteByte(msg, alpha);
		if (bits & U_SCALE)		MSG_WriteByte(msg, scale);
		if (bits & U_EFFECTS2)	MSG_WriteByte(msg, (int)ent->v.effects >> 8);
		if (bits & U_GLOWSIZE)	MSG_WriteByte(msg, glowsize);
		if (bits & U_GLOWCOLOR)	MSG_WriteByte(msg, glowcolor);
		if (bits & U_COLORMOD)	MSG_WriteByte(msg, colormod);
		if (bits & U_FRAME2)	MSG_WriteByte(msg, (int)ent->v.frame >> 8);
	}
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
	{
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}

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

//
// send a damage message
//
	if (ent->v.dmg_take || ent->v.dmg_save)
	{
		other = PROG_TO_EDICT(ent->v.dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v.dmg_save);
		MSG_WriteByte (msg, ent->v.dmg_take);
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, other->v.origin[i] + 0.5*(other->v.mins[i] + other->v.maxs[i]));
	
		ent->v.dmg_take = 0;
		ent->v.dmg_save = 0;
	}

//
// send the current viewpos offset from the view entity
//
	SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet.  Oh well.
	if ( ent->v.fixangle )
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			MSG_WriteAngle (msg, ent->v.angles[i] );
		ent->v.fixangle = 0;
	}

	bits = 0;
	
	if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;
		
	if (ent->v.idealpitch)
		bits |= SU_IDEALPITCH;

// stuff the sigil bits into the high bits of items for sbar, or else
// mix in items2
	val = GETEDICTFIELDVALUE(ent, eval_items2);

	if (val)
		items = (int)ent->v.items | ((int)val->_float << 23);
	else
		items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);

	bits |= SU_ITEMS;
	
	if ( (int)ent->v.flags & FL_ONGROUND)
		bits |= SU_ONGROUND;
	
	if ( ent->v.waterlevel >= 2)
		bits |= SU_INWATER;
	
	for (i=0 ; i<3 ; i++)
	{
		if (ent->v.punchangle[i])
			bits |= (SU_PUNCH1<<i);
		if (ent->v.velocity[i])
			bits |= (SU_VELOCITY1<<i);
	}
	
	if (ent->v.weaponframe)
		bits |= SU_WEAPONFRAME;

	if (ent->v.armorvalue)
		bits |= SU_ARMOR;

//	if (ent->v.weapon)
		bits |= SU_WEAPON;

// send the data

	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, ent->v.view_ofs[2]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, ent->v.idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
			MSG_WriteChar (msg, ent->v.punchangle[i]);
		if (bits & (SU_VELOCITY1<<i))
			MSG_WriteChar (msg, ent->v.velocity[i]/16);
	}

// [always sent]	if (bits & SU_ITEMS)
	MSG_WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte (msg, ent->v.weaponframe);
	if (bits & SU_ARMOR)
		MSG_WriteByte (msg, ent->v.armorvalue);
	if (bits & SU_WEAPON)
		MSG_WriteByte (msg, SV_ModelIndex(pr_strings+ent->v.weaponmodel));
	
	MSG_WriteShort (msg, ent->v.health);
	MSG_WriteByte (msg, ent->v.currentammo);
	MSG_WriteByte (msg, ent->v.ammo_shells);
	MSG_WriteByte (msg, ent->v.ammo_nails);
	MSG_WriteByte (msg, ent->v.ammo_rockets);
	MSG_WriteByte (msg, ent->v.ammo_cells);

	if (standard_quake)
	{
		MSG_WriteByte (msg, ent->v.weapon);
	}
	else
	{
		for(i=0;i<32;i++)
		{
			if ( ((int)ent->v.weapon) & (1<<i) )
			{
				MSG_WriteByte (msg, i);
				break;
			}
		}
	}
}

/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
	byte		buf[MAX_DATAGRAM];
	sizebuf_t	msg;
	
	msg.data = buf;
	msg.maxsize = sizeof(buf);
	msg.cursize = 0;

	MSG_WriteByte (&msg, svc_time);
	MSG_WriteFloat (&msg, sv.time);

// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client->edict, &msg);

	SV_WriteEntitiesToClient (client->edict, &msg);

// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

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
		if (host_client->old_frags != host_client->edict->v.frags)
		{
			for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
			{
				if (!client->active)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message, host_client->edict->v.frags);
			}

			host_client->old_frags = host_client->edict->v.frags;
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
	byte		buf[4];
	
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
			SV_DropClient (true);
			host_client->message.overflowed = false;
			continue;
		}
			
		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NET_CanSendMessage (host_client->netconnection))
			{
//				I_Printf ("can't write\n");
				continue;
			}

			if (host_client->dropasap)
				SV_DropClient (false);	// went to another level
			else
			{
				if (NET_SendMessage (host_client->netconnection
				, &host_client->message) == -1)
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
int SV_ModelIndex (char *name)
{
	int		i;
	
	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
		Sys_Error ("SV_ModelIndex: model %s not precached", name);
	return i;
}

/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
	int			i;
	edict_t			*svent;
	int				entnum;	
		
	for (entnum = 0; entnum < sv.num_edicts ; entnum++)
	{
	// get the current server version
		svent = EDICT_NUM(entnum);
		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !svent->v.modelindex)
			continue;

	//
	// create entity baseline
	//
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skin = svent->v.skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex =
				SV_ModelIndex(pr_strings + svent->v.model);
		}
		svent->baseline.alpha = 255;
		svent->baseline.scale = 16;
		svent->baseline.glowsize = 0;
		svent->baseline.glowcolor = 254;
		svent->baseline.colormod = 255;
		
	//
	// add to the message
	//
		MSG_WriteByte (&sv.signon,svc_spawnbaseline);		
		MSG_WriteShort (&sv.signon,entnum);

		MSG_WriteByte (&sv.signon, svent->baseline.modelindex);
		MSG_WriteByte (&sv.signon, svent->baseline.frame);
		MSG_WriteByte (&sv.signon, svent->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->baseline.origin[i]);
			MSG_WriteAngle(&sv.signon, svent->baseline.angles[i]);
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
		PR_ExecuteProgram (pr_global_struct->SetChangeParms);
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
	}
}

qboolean isworldmodel;

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float		scr_centertime_off;

void SV_SpawnServer (char *server)
{
	edict_t		*ent;
	int			i;

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
	{
		SV_SendReconnect ();
	}

//
// make cvars consistant
//
	if (coop.value)
		Cvar_SetValue ("deathmatch", 0);
	current_skill = (int)(skill.value + 0.5);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;

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
	
	sv.edicts = Hunk_AllocName (sv.max_edicts*pr_edict_size, "edicts");

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
	
	strcpy (sv.name, server);
	sprintf (sv.modelname,"maps/%s.bsp", server);
	isworldmodel = true; // LordHavoc: only load submodels on the world model
	sv.worldmodel = Mod_ForName (sv.modelname, false);
	isworldmodel = false;
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
	
	sv.sound_precache[0] = pr_strings;

	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
	{
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false);
	}

//
// load the rest of the entities
//	
	ent = EDICT_NUM(0);
	memset (&ent->v, 0, progs->entityfields * 4);
	ent->free = false;
	ent->v.model = sv.worldmodel->name - pr_strings;
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;
	ent->v.angles[0] = ent->v.angles[1] = ent->v.angles[2] = 0;

	if (coop.value)
		pr_global_struct->coop = coop.value;
	else
		pr_global_struct->deathmatch = deathmatch.value;

	pr_global_struct->mapname = sv.name - pr_strings;

// serverflags are for cross level information (sigils)
	pr_global_struct->serverflags = svs.serverflags;
	
	ED_LoadFromFile (sv.worldmodel->entities);
	// LordHavoc: clear world angles (to fix e3m3.bsp)
	sv.edicts->v.angles[0] = sv.edicts->v.angles[1] = sv.edicts->v.angles[2] = 0;

	sv.active = true;

// all setup is completed, any further precache statements are errors
	sv.state = ss_active;
	
// run two frames to allow everything to settle
	host_frametime = 0.1;
	SV_Physics ();
	SV_Physics ();

// create a baseline for more efficient communications
	SV_CreateBaseline ();

// send serverinfo to all connected clients
	for (i=0,host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo (host_client);
	
	Con_DPrintf ("Server spawned.\n");
}

// LordHavoc: added light checking to the server
int RecursiveLightPoint (vec3_t color, mnode_t *node, vec3_t start, vec3_t end);
void SV_LightPoint (vec3_t color, vec3_t p)
{
	vec3_t		end;
	
	if (!sv.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		return;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	color[0] = color[1] = color[2] = 0;
	RecursiveLightPoint (color, sv.worldmodel->nodes, p, end);
}
