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
// cl_main.c  -- client main loop

#include "quakedef.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t	cl_name = {"_cl_name", "player", true};
cvar_t	cl_color = {"_cl_color", "0", true};
cvar_t	cl_pmodel = {"_cl_pmodel", "0", true};

cvar_t	cl_shownet = {"cl_shownet","0"};	// can be 0, 1, or 2
cvar_t	cl_nolerp = {"cl_nolerp","0"};

cvar_t	lookspring = {"lookspring","0", true};
cvar_t	lookstrafe = {"lookstrafe","0", true};
cvar_t	sensitivity = {"sensitivity","3", true};

cvar_t	m_pitch = {"m_pitch","0.022", true};
cvar_t	m_yaw = {"m_yaw","0.022", true};
cvar_t	m_forward = {"m_forward","1", true};
cvar_t	m_side = {"m_side","0.8", true};

cvar_t freelook = {"freelook", "1", true};

client_static_t	cls;
client_state_t	cl;
// FIXME: put these on hunk?
entity_t		cl_entities[MAX_EDICTS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

int				cl_numvisedicts;
entity_t		*cl_visedicts[MAX_VISEDICTS];

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	int			i;

	if (!sv.active)
		Host_ClearMemory ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.message);

// clear other arrays	
	memset (cl_entities, 0, sizeof(cl_entities));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset (cl_temp_entities, 0, sizeof(cl_temp_entities));
	memset (cl_beams, 0, sizeof(cl_beams));
	// LordHavoc: have to set up the baseline info for alpha and other stuff
	for (i = 0;i < MAX_EDICTS;i++)
	{
		ClearStateToDefault(&cl_entities[i].state_baseline);
		ClearStateToDefault(&cl_entities[i].state_previous);
		ClearStateToDefault(&cl_entities[i].state_current);
	}
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
// stop sounds (especially looping!)
	S_StopAllSounds (true);

	// clear contents blends
	cl.cshifts[0].percent = 0;
	cl.cshifts[1].percent = 0;
	cl.cshifts[2].percent = 0;
	cl.cshifts[3].percent = 0;

// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state == ca_connected)
	{
		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf ("Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer(false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}

void CL_Disconnect_f (void)
{
	CL_Disconnect ();
	if (sv.active)
		Host_ShutdownServer (false);
}




/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Con_DPrintf ("CL_EstablishConnection: connected to %s\n", host);
	
	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply (void)
{
	char 	str[8192];

Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;
		
	case 2:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %i %i\n", ((int)cl_color.value)>>4, ((int)cl_color.value)&15));
	
		if (cl_pmodel.value)
		{
			MSG_WriteByte (&cls.message, clc_stringcmd);
			MSG_WriteString (&cls.message, va("pmodel %f\n", cl_pmodel.value));
		}

		MSG_WriteByte (&cls.message, clc_stringcmd);
		sprintf (str, "spawn %s", cls.spawnparms);
		MSG_WriteString (&cls.message, str);
		break;
		
	case 3:	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();		// print remaining memory
		break;
		
	case 4:
//		SCR_EndLoadingPlaque ();		// allow normal screen updates
		Con_ClearNotify();
		break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char	str[1024];

	if (cls.demonum == -1)
		return;		// don't play demos

//	SCR_BeginLoadingPlaque ();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	sprintf (str,"playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f (void)
{
	entity_t	*ent;
	int			i;
	
	for (i = 0, ent = cl_entities;i < MAX_EDICTS /*cl.num_entities*/;i++, ent++)
	{
		Con_Printf ("%3i:", i);
		if (!ent->render.model)
		{
			Con_Printf ("EMPTY\n");
			continue;
		}
		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n", ent->render.model->name, ent->render.frame, ent->render.origin[0], ent->render.origin[1], ent->render.origin[2], ent->render.angles[0], ent->render.angles[1], ent->render.angles[2]);
	}
}


/*
===============
CL_AllocDlight

===============
*/
void CL_AllocDlight (entity_t *ent, vec3_t org, float radius, float red, float green, float blue, float decay, float lifetime)
{
	int		i;
	dlight_t	*dl;

// first look for an exact key match
	if (ent)
	{
		dl = cl_dlights;
		for (i = 0;i < MAX_DLIGHTS;i++, dl++)
			if (dl->ent == ent)
				goto dlightsetup;
	}

// then look for anything else
	dl = cl_dlights;
	for (i = 0;i < MAX_DLIGHTS;i++, dl++)
		if (!dl->radius)
			goto dlightsetup;

	// unable to find one
	return;

dlightsetup:
	memset (dl, 0, sizeof(*dl));
	dl->ent = ent;
	VectorCopy(org, dl->origin);
	dl->radius = radius;
	dl->color[0] = red;
	dl->color[1] = green;
	dl->color[2] = blue;
	dl->decay = decay;
	dl->die = cl.time + lifetime;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t	*dl;
	float		time;
	
	time = cl.time - cl.oldtime;

	c_dlights = 0;
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			continue;
		}

		c_dlights++; // count every dlight in use

		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float	CL_LerpPoint (void)
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	// LordHavoc: lerp in listen games as the server is being capped below the client (usually)
	if (!f || cl_nolerp.value || cls.timedemo || (sv.active && svs.maxclients == 1))
	{
		cl.time = cl.mtime[0];
		return 1;
	}
		
	if (f > 0.1)
	{	// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;
//	Con_Printf ("frac: %f\n",frac);
	if (frac < 0)
	{
		if (frac < -0.01)
		{
			cl.time = cl.mtime[1];
//			Con_Printf ("low frac\n");
		}
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
		{
			cl.time = cl.mtime[0];
//			Con_Printf ("high frac\n");
		}
		frac = 1;
	}
		
	return frac;
}

float CL_EntityLerpPoint (entity_t *ent)
{
	float	f;

	if (cl_nolerp.value || cls.timedemo || (sv.active && svs.maxclients == 1))
		return 1;

	f = ent->state_current.time - ent->state_previous.time;
//	Con_Printf(" %g-%g=%g", ent->state_current.time, ent->state_previous.time, f);

	if (f <= 0)
		return 1;
	if (f >= 0.1)
		f = 0.1;

//	Con_Printf(" %g-%g/%g=%f", cl.time, ent->state_previous.time, f, (cl.time - ent->state_previous.time) / f);
	f = (cl.time - ent->state_previous.time) / f;
	return bound(0, f, 1);
}

void CL_RelinkStaticEntities(void)
{
	entity_t *ent, *endent;
	if (cl.num_statics > MAX_VISEDICTS)
		Host_Error("CL_RelinkStaticEntities: cl.num_statics > MAX_VISEDICTS??\n");

	ent = cl_static_entities;
	endent = ent + cl.num_statics;
	for (;ent < endent;ent++)
		cl_visedicts[cl_numvisedicts++] = ent;
}

/*
===============
CL_RelinkEntities
===============
*/
void R_RocketTrail2 (vec3_t start, vec3_t end, int color, entity_t *ent);
void CL_RelinkEntities (void)
{
	entity_t	*ent;
	int			i, j;
	float		frac, f, d, bobjrotate/*, bobjoffset*/, dlightradius;
	vec3_t		oldorg, delta, dlightcolor;

// determine partial update time	
	frac = CL_LerpPoint ();

	cl_numvisedicts = 0;

	CL_RelinkStaticEntities();

//
// interpolate player info
//
	for (i = 0;i < 3;i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback)
	{
	// interpolate the angles	
		for (j = 0;j < 3;j++)
		{
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac*d;
		}
	}
	
	bobjrotate = ANGLEMOD(100*cl.time);
//	bobjoffset = cos(180 * cl.time * M_PI / 180) * 4.0f + 4.0f;
	
// start on the entity after the world
	for (i = 1, ent = cl_entities + 1;i < MAX_EDICTS /*cl.num_entities*/;i++, ent++)
	{
		// if the object wasn't included in the latest packet, remove it
		if (!ent->state_current.active)
			continue;

		VectorCopy (ent->render.origin, oldorg);

		if (!ent->state_previous.active)
		{
			// only one state available
			VectorCopy (ent->state_current.origin, ent->render.origin);
			VectorCopy (ent->state_current.angles, ent->render.angles);
//			Con_Printf(" %i", i);
		}
		else
		{
			// if the delta is large, assume a teleport and don't lerp
			f = CL_EntityLerpPoint(ent);
			if (f < 1)
			{
				for (j = 0;j < 3;j++)
				{
					delta[j] = ent->state_current.origin[j] - ent->state_previous.origin[j];
					// LordHavoc: increased lerp tolerance from 100 to 200
					if (delta[j] > 200 || delta[j] < -200)
						f = 1;
				}
			}
			if (f >= 1)
			{
				// no interpolation
				VectorCopy (ent->state_current.origin, ent->render.origin);
				VectorCopy (ent->state_current.angles, ent->render.angles);
			}
			else
			{
				// interpolate the origin and angles
				for (j = 0;j < 3;j++)
				{
					ent->render.origin[j] = ent->state_previous.origin[j] + f*delta[j];

					d = ent->state_current.angles[j] - ent->state_previous.angles[j];
					if (d > 180)
						d -= 360;
					else if (d < -180)
						d += 360;
					ent->render.angles[j] = ent->state_previous.angles[j] + f*d;
				}
			}
		}

		ent->render.flags = ent->state_current.flags;
		ent->render.effects = ent->state_current.effects;
		ent->render.model = cl.model_precache[ent->state_current.modelindex];
		ent->render.frame = ent->state_current.frame;
		if (cl.scores == NULL || !ent->state_current.colormap)
			ent->render.colormap = -1; // no special coloring
		else
			ent->render.colormap = cl.scores[ent->state_current.colormap - 1].colors; // color it
		ent->render.skinnum = ent->state_current.skin;
		ent->render.alpha = ent->state_current.alpha * (1.0f / 255.0f); // FIXME: interpolate?
		ent->render.scale = ent->state_current.scale * (1.0f / 16.0f); // FIXME: interpolate?
		ent->render.glowsize = ent->state_current.glowsize * 4.0f; // FIXME: interpolate?
		ent->render.glowcolor = ent->state_current.glowcolor;
		ent->render.colormod[0] = (float) ((ent->state_current.colormod >> 5) & 7) * (1.0f / 7.0f);
		ent->render.colormod[1] = (float) ((ent->state_current.colormod >> 2) & 7) * (1.0f / 7.0f);
		ent->render.colormod[2] = (float) (ent->state_current.colormod & 3) * (1.0f / 3.0f);

		dlightradius = 0;
		dlightcolor[0] = 0;
		dlightcolor[1] = 0;
		dlightcolor[2] = 0;

		// LordHavoc: if the entity has no effects, don't check each
		if (ent->render.effects)
		{
			if (ent->render.effects & EF_BRIGHTFIELD)
				R_EntityParticles (ent);
			if (ent->render.effects & EF_MUZZLEFLASH)
			{
				vec3_t v;

				AngleVectors (ent->render.angles, v, NULL, NULL);

				v[0] = v[0] * 18 + ent->render.origin[0];
				v[1] = v[1] * 18 + ent->render.origin[1];
				v[2] = v[2] * 18 + ent->render.origin[2] + 16;

				CL_AllocDlight (NULL, v, 100, 1, 1, 1, 0, 0.1);
			}
			if (ent->render.effects & EF_DIMLIGHT)
			{
				dlightcolor[0] += 200.0f;
				dlightcolor[1] += 200.0f;
				dlightcolor[2] += 200.0f;
			}
			if (ent->render.effects & EF_BRIGHTLIGHT)
			{
				dlightcolor[0] += 400.0f;
				dlightcolor[1] += 400.0f;
				dlightcolor[2] += 400.0f;
			}
			// LordHavoc: added EF_RED and EF_BLUE
			if (ent->render.effects & EF_RED) // red
			{
				dlightcolor[0] += 200.0f;
				dlightcolor[1] +=  20.0f;
				dlightcolor[2] +=  20.0f;
			}
			if (ent->render.effects & EF_BLUE) // blue
			{
				dlightcolor[0] +=  20.0f;
				dlightcolor[1] +=  20.0f;
				dlightcolor[2] += 200.0f;
			}
			else if (ent->render.effects & EF_FLAME)
			{
				if (ent->render.model)
				{
					vec3_t mins, maxs;
					int temp;
					VectorAdd(ent->render.origin, ent->render.model->mins, mins);
					VectorAdd(ent->render.origin, ent->render.model->maxs, maxs);
					// how many flames to make
					temp = (int) (cl.time * 300) - (int) (cl.oldtime * 300);
					R_FlameCube(mins, maxs, temp);
				}
				d = lhrandom(200, 250);
				dlightcolor[0] += d * 1.0f;
				dlightcolor[1] += d * 0.7f;
				dlightcolor[2] += d * 0.3f;
			}
		}

		// LordHavoc: if the model has no flags, don't check each
		if (ent->render.model && ent->render.model->flags)
		{
			if (ent->render.model->flags & EF_ROTATE)
			{
				ent->render.angles[1] = bobjrotate;
//				ent->render.origin[2] += bobjoffset;
			}
			// only do trails if present in the previous frame as well
			if (ent->state_previous.active)
			{
				if (ent->render.model->flags & EF_GIB)
					R_RocketTrail (oldorg, ent->render.origin, 2, ent);
				else if (ent->render.model->flags & EF_ZOMGIB)
					R_RocketTrail (oldorg, ent->render.origin, 4, ent);
				else if (ent->render.model->flags & EF_TRACER)
					R_RocketTrail (oldorg, ent->render.origin, 3, ent);
				else if (ent->render.model->flags & EF_TRACER2)
					R_RocketTrail (oldorg, ent->render.origin, 5, ent);
				else if (ent->render.model->flags & EF_ROCKET)
				{
					R_RocketTrail (oldorg, ent->render.origin, 0, ent);
					dlightcolor[0] += 200.0f;
					dlightcolor[1] += 160.0f;
					dlightcolor[2] +=  80.0f;
				}
				else if (ent->render.model->flags & EF_GRENADE)
				{
					if (ent->render.alpha == -1) // LordHavoc: Nehahra dem compatibility
						R_RocketTrail (oldorg, ent->render.origin, 7, ent);
					else
						R_RocketTrail (oldorg, ent->render.origin, 1, ent);
				}
				else if (ent->render.model->flags & EF_TRACER3)
					R_RocketTrail (oldorg, ent->render.origin, 6, ent);
			}
		}
		// LordHavoc: customizable glow
		if (ent->render.glowsize)
		{
			byte *tempcolor = (byte *)&d_8to24table[ent->render.glowcolor];
			dlightcolor[0] += ent->render.glowsize * tempcolor[0] * (1.0f / 255.0f);
			dlightcolor[1] += ent->render.glowsize * tempcolor[1] * (1.0f / 255.0f);
			dlightcolor[2] += ent->render.glowsize * tempcolor[2] * (1.0f / 255.0f);
		}
		// LordHavoc: customizable trail
		if (ent->render.flags & RENDER_GLOWTRAIL)
			R_RocketTrail2 (oldorg, ent->render.origin, ent->render.glowcolor, ent);

		if (dlightcolor[0] || dlightcolor[1] || dlightcolor[2])
		{
			dlightradius = VectorLength(dlightcolor);
			d = 1.0f / dlightradius;
			CL_AllocDlight (ent, ent->render.origin, dlightradius, dlightcolor[0] * d, dlightcolor[1] * d, dlightcolor[2] * d, 0, 0);
		}

		if (!chase_active.value && ((i == cl.viewentity) || (ent->render.flags & RENDER_EXTERIORMODEL)))
			continue;

		if (ent->render.model == NULL)
			continue;
		if (ent->render.effects & EF_NODRAW)
			continue;
		if (cl_numvisedicts < MAX_VISEDICTS)
			cl_visedicts[cl_numvisedicts++] = ent;
	}
//	Con_Printf("\n");
}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer (void)
{
	int ret, netshown;

	cl.oldtime = cl.time;
	cl.time += cl.frametime;
	
	netshown = false;
	do
	{
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;
		
		cl.last_received_message = realtime;

		if (cl_shownet.value)
			netshown = true;

		CL_ParseServerMessage ();
	}
	while (ret && cls.state == ca_connected);
	
	if (netshown)
		Con_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts ();
	CL_DoEffects ();

//
// bring the links up to date
//
	return 0;
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	usercmd_t		cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
	// get basic movement from keyboard
		CL_BaseMove (&cmd);
	
	// allow mice or other external controllers to add to the move
		IN_Move (&cmd);
	
	// send the unreliable message
		CL_SendMove (&cmd);
	}

	if (cls.demoplayback)
	{
		SZ_Clear (&cls.message);
		return;
	}
	
// send the reliable message
	if (!cls.message.cursize)
		return;		// no message at all
	
	if (!NET_CanSendMessage (cls.netcon))
	{
		Con_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

// LordHavoc: pausedemo command
void CL_PauseDemo_f (void)
{
	cls.demopaused = !cls.demopaused;
	if (cls.demopaused)
		Con_Printf("Demo paused\n");
	else
		Con_Printf("Demo unpaused\n");
}

/*
======================
CL_PModel_f
LordHavoc: Intended for Nehahra, I personally think this is dumb, but Mindcrime won't listen.
======================
*/
void CL_PModel_f (void)
{
	int i;
	eval_t *val;

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("\"pmodel\" is \"%s\"\n", cl_pmodel.string);
		return;
	}
	i = atoi(Cmd_Argv(1));

	if (cmd_source == src_command)
	{
		if (cl_pmodel.value == i)
			return;
		Cvar_SetValue ("_cl_pmodel", i);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->pmodel = i;
	if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_pmodel)))
		val->_float = i;
}

/*
======================
CL_Fog_f
======================
*/
void CL_Fog_f (void)
{
	if (Cmd_Argc () == 1)
	{
		Con_Printf ("\"fog\" is \"%f %f %f %f\"\n", fog_density, fog_red, fog_green, fog_blue);
		return;
	}
	fog_density = atof(Cmd_Argv(1));
	fog_red = atof(Cmd_Argv(2));
	fog_green = atof(Cmd_Argv(3));
	fog_blue = atof(Cmd_Argv(4));
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{	
	SZ_Alloc (&cls.message, 1024);

	CL_InitInput ();
	CL_InitTEnts ();
	
//
// register our commands
//
	Cvar_RegisterVariable (&cl_name);
	Cvar_RegisterVariable (&cl_color);
	Cvar_RegisterVariable (&cl_pmodel);
	Cvar_RegisterVariable (&cl_upspeed);
	Cvar_RegisterVariable (&cl_forwardspeed);
	Cvar_RegisterVariable (&cl_backspeed);
	Cvar_RegisterVariable (&cl_sidespeed);
	Cvar_RegisterVariable (&cl_movespeedkey);
	Cvar_RegisterVariable (&cl_yawspeed);
	Cvar_RegisterVariable (&cl_pitchspeed);
	Cvar_RegisterVariable (&cl_anglespeedkey);
	Cvar_RegisterVariable (&cl_shownet);
	Cvar_RegisterVariable (&cl_nolerp);
	Cvar_RegisterVariable (&lookspring);
	Cvar_RegisterVariable (&lookstrafe);
	Cvar_RegisterVariable (&sensitivity);
	Cvar_RegisterVariable (&freelook);

	Cvar_RegisterVariable (&m_pitch);
	Cvar_RegisterVariable (&m_yaw);
	Cvar_RegisterVariable (&m_forward);
	Cvar_RegisterVariable (&m_side);

//	Cvar_RegisterVariable (&cl_autofire);
	
	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("bitprofile", CL_BitProfile_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand ("fog", CL_Fog_f);

	// LordHavoc: added pausedemo
	Cmd_AddCommand ("pausedemo", CL_PauseDemo_f);
	// LordHavoc: added pmodel command (like name, etc, only intended for Nehahra)
	Cmd_AddCommand ("pmodel", CL_PModel_f);

	CL_Parse_Init();
}

