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
#include "cl_collision.h"
#include "cl_video.h"
#include "image.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t cl_name = {CVAR_SAVE, "_cl_name", "player"};
cvar_t cl_color = {CVAR_SAVE, "_cl_color", "0"};
cvar_t cl_pmodel = {CVAR_SAVE, "_cl_pmodel", "0"};

cvar_t cl_shownet = {0, "cl_shownet","0"};
cvar_t cl_nolerp = {0, "cl_nolerp", "0"};

cvar_t cl_itembobheight = {0, "cl_itembobheight", "8"};
cvar_t cl_itembobspeed = {0, "cl_itembobspeed", "0.5"};

cvar_t lookspring = {CVAR_SAVE, "lookspring","0"};
cvar_t lookstrafe = {CVAR_SAVE, "lookstrafe","0"};
cvar_t sensitivity = {CVAR_SAVE, "sensitivity","3", 1, 30};

cvar_t m_pitch = {CVAR_SAVE, "m_pitch","0.022"};
cvar_t m_yaw = {CVAR_SAVE, "m_yaw","0.022"};
cvar_t m_forward = {CVAR_SAVE, "m_forward","1"};
cvar_t m_side = {CVAR_SAVE, "m_side","0.8"};

cvar_t freelook = {CVAR_SAVE, "freelook", "1"};

cvar_t r_draweffects = {0, "r_draweffects", "1"};

cvar_t cl_explosions = {CVAR_SAVE, "cl_explosions", "1"};
cvar_t cl_stainmaps = {CVAR_SAVE, "cl_stainmaps", "1"};

cvar_t cl_beams_polygons = {CVAR_SAVE, "cl_beams_polygons", "1"};
cvar_t cl_beams_relative = {CVAR_SAVE, "cl_beams_relative", "1"};
cvar_t cl_beams_lightatend = {CVAR_SAVE, "cl_beams_lightatend", "0"};

cvar_t cl_noplayershadow = {CVAR_SAVE, "cl_noplayershadow", "0"};

mempool_t *cl_scores_mempool;
mempool_t *cl_refdef_mempool;
mempool_t *cl_entities_mempool;

client_static_t	cls;
client_state_t	cl;

int cl_max_entities;
int cl_max_static_entities;
int cl_max_temp_entities;
int cl_max_effects;
int cl_max_beams;
int cl_max_dlights;
int cl_max_lightstyle;
int cl_max_brushmodel_entities;

entity_t *cl_entities;
qbyte *cl_entities_active;
entity_t *cl_static_entities;
entity_t *cl_temp_entities;
cl_effect_t *cl_effects;
beam_t *cl_beams;
dlight_t *cl_dlights;
lightstyle_t *cl_lightstyle;
entity_render_t **cl_brushmodel_entities;

int cl_num_entities;
int cl_num_static_entities;
int cl_num_temp_entities;
int cl_num_brushmodel_entities;

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	int i;

	if (!sv.active)
		Host_ClearMemory ();

	Mem_EmptyPool(cl_scores_mempool);
	Mem_EmptyPool(cl_entities_mempool);

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.message);

	cl_num_entities = 0;
	cl_num_static_entities = 0;
	cl_num_temp_entities = 0;
	cl_num_brushmodel_entities = 0;

	// tweak these if the game runs out
	cl_max_entities = MAX_EDICTS;
	cl_max_static_entities = 256;
	cl_max_temp_entities = 512;
	cl_max_effects = 256;
	cl_max_beams = 24;
	cl_max_dlights = MAX_DLIGHTS;
	cl_max_lightstyle = MAX_LIGHTSTYLES;
	cl_max_brushmodel_entities = MAX_EDICTS;

	cl_entities = Mem_Alloc(cl_entities_mempool, cl_max_entities * sizeof(entity_t));
	cl_entities_active = Mem_Alloc(cl_entities_mempool, cl_max_entities * sizeof(qbyte));
	cl_static_entities = Mem_Alloc(cl_entities_mempool, cl_max_static_entities * sizeof(entity_t));
	cl_temp_entities = Mem_Alloc(cl_entities_mempool, cl_max_temp_entities * sizeof(entity_t));
	cl_effects = Mem_Alloc(cl_entities_mempool, cl_max_effects * sizeof(cl_effect_t));
	cl_beams = Mem_Alloc(cl_entities_mempool, cl_max_beams * sizeof(beam_t));
	cl_dlights = Mem_Alloc(cl_entities_mempool, cl_max_dlights * sizeof(dlight_t));
	cl_lightstyle = Mem_Alloc(cl_entities_mempool, cl_max_lightstyle * sizeof(lightstyle_t));
	cl_brushmodel_entities = Mem_Alloc(cl_entities_mempool, cl_max_brushmodel_entities * sizeof(entity_render_t *));

	CL_Screen_NewMap();

	CL_Particles_Clear();

	// LordHavoc: have to set up the baseline info for alpha and other stuff
	for (i = 0;i < cl_max_entities;i++)
	{
		ClearStateToDefault(&cl_entities[i].state_baseline);
		ClearStateToDefault(&cl_entities[i].state_previous);
		ClearStateToDefault(&cl_entities[i].state_current);
	}

	CL_CGVM_Clear();
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
	if (cls.state == ca_dedicated)
		return;

// stop sounds (especially looping!)
	S_StopAllSounds (true);

	// clear contents blends
	cl.cshifts[0].percent = 0;
	cl.cshifts[1].percent = 0;
	cl.cshifts[2].percent = 0;
	cl.cshifts[3].percent = 0;

	cl.worldmodel = NULL;

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
		cls.state = ca_disconnected; // prevent this code from executing again during Host_ShutdownServer
		// if running a local server, shut it down
		if (sv.active)
			Host_ShutdownServer(false);
	}
	cls.state = ca_disconnected;

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

	CL_ClearState ();
}

/*
==============
CL_PrintEntities_f
==============
*/
static void CL_PrintEntities_f (void)
{
	entity_t *ent;
	int i, j;
	char name[32];

	for (i = 0, ent = cl_entities;i < cl_num_entities;i++, ent++)
	{
		if (!ent->state_current.active)
			continue;

		if (ent->render.model)
			strncpy(name, ent->render.model->name, 25);
		else
			strcpy(name, "--no model--");
		name[25] = 0;
		for (j = strlen(name);j < 25;j++)
			name[j] = ' ';
		Con_Printf ("%3i: %s:%04i (%5i %5i %5i) [%3i %3i %3i] %4.2f %5.3f\n", i, name, ent->render.frame, (int) ent->render.origin[0], (int) ent->render.origin[1], (int) ent->render.origin[2], (int) ent->render.angles[0] % 360, (int) ent->render.angles[1] % 360, (int) ent->render.angles[2] % 360, ent->render.scale, ent->render.alpha);
	}
}

//static const vec3_t nomodelmins = {-16, -16, -16};
//static const vec3_t nomodelmaxs = {16, 16, 16};
void CL_BoundingBoxForEntity(entity_render_t *ent)
{
	if (ent->model)
	{
		//if (ent->angles[0] || ent->angles[2])
		if (ent->matrix.m[2][0] != 0 || ent->matrix.m[2][1] != 0)
		{
			// pitch or roll
			ent->mins[0] = ent->matrix.m[0][3] + ent->model->rotatedmins[0];
			ent->mins[1] = ent->matrix.m[1][3] + ent->model->rotatedmins[1];
			ent->mins[2] = ent->matrix.m[2][3] + ent->model->rotatedmins[2];
			ent->maxs[0] = ent->matrix.m[0][3] + ent->model->rotatedmaxs[0];
			ent->maxs[1] = ent->matrix.m[1][3] + ent->model->rotatedmaxs[1];
			ent->maxs[2] = ent->matrix.m[2][3] + ent->model->rotatedmaxs[2];
			//VectorAdd(ent->origin, ent->model->rotatedmins, ent->mins);
			//VectorAdd(ent->origin, ent->model->rotatedmaxs, ent->maxs);
		}
		//else if (ent->angles[1])
		else if (ent->matrix.m[0][1] != 0 || ent->matrix.m[1][0] != 0)
		{
			// yaw
			ent->mins[0] = ent->matrix.m[0][3] + ent->model->yawmins[0];
			ent->mins[1] = ent->matrix.m[1][3] + ent->model->yawmins[1];
			ent->mins[2] = ent->matrix.m[2][3] + ent->model->yawmins[2];
			ent->maxs[0] = ent->matrix.m[0][3] + ent->model->yawmaxs[0];
			ent->maxs[1] = ent->matrix.m[1][3] + ent->model->yawmaxs[1];
			ent->maxs[2] = ent->matrix.m[2][3] + ent->model->yawmaxs[2];
			//VectorAdd(ent->origin, ent->model->yawmins, ent->mins);
			//VectorAdd(ent->origin, ent->model->yawmaxs, ent->maxs);
		}
		else
		{
			ent->mins[0] = ent->matrix.m[0][3] + ent->model->normalmins[0];
			ent->mins[1] = ent->matrix.m[1][3] + ent->model->normalmins[1];
			ent->mins[2] = ent->matrix.m[2][3] + ent->model->normalmins[2];
			ent->maxs[0] = ent->matrix.m[0][3] + ent->model->normalmaxs[0];
			ent->maxs[1] = ent->matrix.m[1][3] + ent->model->normalmaxs[1];
			ent->maxs[2] = ent->matrix.m[2][3] + ent->model->normalmaxs[2];
			//VectorAdd(ent->origin, ent->model->normalmins, ent->mins);
			//VectorAdd(ent->origin, ent->model->normalmaxs, ent->maxs);
		}
	}
	else
	{
		ent->mins[0] = ent->matrix.m[0][3] - 16;
		ent->mins[1] = ent->matrix.m[1][3] - 16;
		ent->mins[2] = ent->matrix.m[2][3] - 16;
		ent->maxs[0] = ent->matrix.m[0][3] + 16;
		ent->maxs[1] = ent->matrix.m[1][3] + 16;
		ent->maxs[2] = ent->matrix.m[2][3] + 16;
		//VectorAdd(ent->origin, nomodelmins, ent->mins);
		//VectorAdd(ent->origin, nomodelmaxs, ent->maxs);
	}
}

void CL_LerpUpdate(entity_t *e)
{
	entity_persistent_t *p;
	entity_render_t *r;
	p = &e->persistent;
	r = &e->render;

	if (p->modelindex != e->state_current.modelindex)
	{
		// reset all interpolation information
		p->modelindex = e->state_current.modelindex;
		p->frame1 = p->frame2 = e->state_current.frame;
		p->frame1time = p->frame2time = cl.time;
		p->framelerp = 1;
	}
	else if (p->frame2 != e->state_current.frame)
	{
		// transition to new frame
		p->frame1 = p->frame2;
		p->frame1time = p->frame2time;
		p->frame2 = e->state_current.frame;
		p->frame2time = cl.time;
		p->framelerp = 0;
	}
	else
	{
		// update transition
		p->framelerp = (cl.time - p->frame2time) * 10;
		p->framelerp = bound(0, p->framelerp, 1);
	}

	r->model = cl.model_precache[e->state_current.modelindex];
	Mod_CheckLoaded(r->model);
	r->frame = e->state_current.frame;
	r->frame1 = p->frame1;
	r->frame2 = p->frame2;
	r->framelerp = p->framelerp;
	r->frame1time = p->frame1time;
	r->frame2time = p->frame2time;
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float CL_LerpPoint (void)
{
	float f;

	// dropped packet, or start of demo
	if (cl.mtime[1] < cl.mtime[0] - 0.1)
		cl.mtime[1] = cl.mtime[0] - 0.1;

	cl.time = bound(cl.mtime[1], cl.time, cl.mtime[0]);

	// LordHavoc: lerp in listen games as the server is being capped below the client (usually)
	f = cl.mtime[0] - cl.mtime[1];
	if (!f || cl_nolerp.integer || cls.timedemo || (sv.active && svs.maxclients == 1))
	{
		cl.time = cl.mtime[0];
		return 1;
	}

	f = (cl.time - cl.mtime[1]) / f;
	return bound(0, f, 1);
}

void CL_ClearTempEntities (void)
{
	cl_num_temp_entities = 0;
}

entity_t *CL_NewTempEntity (void)
{
	entity_t *ent;

	if (r_refdef.numentities >= r_refdef.maxentities)
		return NULL;
	if (cl_num_temp_entities >= cl_max_temp_entities)
		return NULL;
	ent = &cl_temp_entities[cl_num_temp_entities++];
	memset (ent, 0, sizeof(*ent));
	r_refdef.entities[r_refdef.numentities++] = &ent->render;

	ent->render.colormap = -1; // no special coloring
	ent->render.scale = 1;
	ent->render.alpha = 1;
	return ent;
}

void CL_AllocDlight (entity_render_t *ent, vec3_t org, float radius, float red, float green, float blue, float decay, float lifetime)
{
	int i;
	dlight_t *dl;

	/*
// first look for an exact key match
	if (ent)
	{
		dl = cl_dlights;
		for (i = 0;i < MAX_DLIGHTS;i++, dl++)
			if (dl->ent == ent)
				goto dlightsetup;
	}
	*/

// then look for anything else
	dl = cl_dlights;
	for (i = 0;i < MAX_DLIGHTS;i++, dl++)
		if (!dl->radius)
			goto dlightsetup;

	// unable to find one
	return;

dlightsetup:
	//Con_Printf("dlight %i : %f %f %f : %f %f %f\n", i, org[0], org[1], org[2], red * radius, green * radius, blue * radius);
	memset (dl, 0, sizeof(*dl));
	dl->ent = ent;
	VectorCopy(org, dl->origin);
	dl->radius = radius;
	dl->color[0] = red;
	dl->color[1] = green;
	dl->color[2] = blue;
	dl->decay = decay;
	if (lifetime)
		dl->die = cl.time + lifetime;
	else
		dl->die = 0;
}

void CL_DecayLights (void)
{
	int i;
	dlight_t *dl;
	float time;

	time = cl.time - cl.oldtime;

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

		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

void CL_RelinkWorld (void)
{
	entity_t *ent = &cl_entities[0];
	if (cl_num_entities < 1)
		cl_num_entities = 1;
	cl_brushmodel_entities[cl_num_brushmodel_entities++] = &ent->render;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, ent->render.origin[0], ent->render.origin[1], ent->render.origin[2], ent->render.angles[0], ent->render.angles[1], ent->render.angles[2], ent->render.scale);
	Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
	CL_BoundingBoxForEntity(&ent->render);
}

static void CL_RelinkStaticEntities(void)
{
	int i;
	for (i = 0;i < cl_num_static_entities && r_refdef.numentities < r_refdef.maxentities;i++)
	{
		Mod_CheckLoaded(cl_static_entities[i].render.model);
		r_refdef.entities[r_refdef.numentities++] = &cl_static_entities[i].render;
	}
}

/*
===============
CL_RelinkEntities
===============
*/
extern qboolean Nehahrademcompatibility;
#define MAXVIEWMODELS 32
entity_t *viewmodels[MAXVIEWMODELS];
int numviewmodels;
static void CL_RelinkNetworkEntities(void)
{
	entity_t *ent;
	int i, effects, temp;
	float d, bobjrotate, bobjoffset, lerp;
	vec3_t oldorg, neworg, delta, dlightcolor, v, v2, mins, maxs;

	numviewmodels = 0;

	bobjrotate = ANGLEMOD(100*cl.time);
	if (cl_itembobheight.value)
		bobjoffset = (cos(cl.time * cl_itembobspeed.value * (2.0 * M_PI)) + 1.0) * 0.5 * cl_itembobheight.value;
	else
		bobjoffset = 0;

	// start on the entity after the world
	for (i = 1, ent = cl_entities + 1;i < MAX_EDICTS;i++, ent++)
	{
		// if the object isn't active in the current network frame, skip it
		if (!cl_entities_active[i])
			continue;
		if (!ent->state_current.active)
		{
			cl_entities_active[i] = false;
			continue;
		}

		VectorCopy(ent->persistent.trail_origin, oldorg);

		if (!ent->state_previous.active)
		{
			// only one state available
			VectorCopy (ent->persistent.neworigin, neworg);
			VectorCopy (ent->persistent.newangles, ent->render.angles);
			VectorCopy (neworg, oldorg);
		}
		else
		{
			// if the delta is large, assume a teleport and don't lerp
			VectorSubtract(ent->persistent.neworigin, ent->persistent.oldorigin, delta);
			if (ent->persistent.lerpdeltatime > 0)
			{
				lerp = (cl.time - ent->persistent.lerpstarttime) / ent->persistent.lerpdeltatime;
				if (lerp < 1)
				{
					// interpolate the origin and angles
					VectorMA(ent->persistent.oldorigin, lerp, delta, neworg);
					VectorSubtract(ent->persistent.newangles, ent->persistent.oldangles, delta);
					if (delta[0] < -180) delta[0] += 360;else if (delta[0] >= 180) delta[0] -= 360;
					if (delta[1] < -180) delta[1] += 360;else if (delta[1] >= 180) delta[1] -= 360;
					if (delta[2] < -180) delta[2] += 360;else if (delta[2] >= 180) delta[2] -= 360;
					VectorMA(ent->persistent.oldangles, lerp, delta, ent->render.angles);
				}
				else
				{
					// no interpolation
					VectorCopy (ent->persistent.neworigin, neworg);
					VectorCopy (ent->persistent.newangles, ent->render.angles);
				}
			}
			else
			{
				// no interpolation
				VectorCopy (ent->persistent.neworigin, neworg);
				VectorCopy (ent->persistent.newangles, ent->render.angles);
			}
		}

		if (!ent->render.model || ent->render.model->type != mod_brush)
			ent->render.angles[0] = -ent->render.angles[0];

		VectorCopy (neworg, ent->persistent.trail_origin);
		// persistent.modelindex will be updated by CL_LerpUpdate
		if (ent->state_current.modelindex != ent->persistent.modelindex || !ent->state_previous.active)
			VectorCopy(neworg, oldorg);

		VectorCopy (neworg, ent->render.origin);
		ent->render.flags = ent->state_current.flags;
		if (i == cl.viewentity)
			ent->render.flags |= RENDER_EXTERIORMODEL;
		ent->render.effects = effects = ent->state_current.effects;
		if (ent->state_current.flags & RENDER_COLORMAPPED)
			ent->render.colormap = ent->state_current.colormap;
		else if (cl.scores == NULL || !ent->state_current.colormap)
			ent->render.colormap = -1; // no special coloring
		else
			ent->render.colormap = cl.scores[ent->state_current.colormap - 1].colors; // color it
		ent->render.skinnum = ent->state_current.skin;
		ent->render.alpha = ent->state_current.alpha * (1.0f / 255.0f); // FIXME: interpolate?
		ent->render.scale = ent->state_current.scale * (1.0f / 16.0f); // FIXME: interpolate?

		if (ent->render.model && ent->render.model->flags & EF_ROTATE)
		{
			ent->render.angles[1] = bobjrotate;
			ent->render.origin[2] += bobjoffset;
		}

		Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, ent->render.origin[0], ent->render.origin[1], ent->render.origin[2], ent->render.angles[0], ent->render.angles[1], ent->render.angles[2], ent->render.scale);

		// update interpolation info
		CL_LerpUpdate(ent);

		// handle effects now...
		dlightcolor[0] = 0;
		dlightcolor[1] = 0;
		dlightcolor[2] = 0;

		// LordHavoc: if the entity has no effects, don't check each
		if (effects)
		{
			if (effects & EF_BRIGHTFIELD)
			{
				if (gamemode == GAME_NEXIUZ)
				{
					dlightcolor[0] += 100.0f;
					dlightcolor[1] += 200.0f;
					dlightcolor[2] += 400.0f;
					// don't do trail if we have no previous location
					if (ent->state_previous.active)
						CL_RocketTrail (oldorg, neworg, 8, ent);
				}
				else
					CL_EntityParticles (ent);
			}
			if (effects & EF_MUZZLEFLASH)
				ent->persistent.muzzleflash = 100.0f;
			if (effects & EF_DIMLIGHT)
			{
				dlightcolor[0] += 200.0f;
				dlightcolor[1] += 200.0f;
				dlightcolor[2] += 200.0f;
			}
			if (effects & EF_BRIGHTLIGHT)
			{
				dlightcolor[0] += 400.0f;
				dlightcolor[1] += 400.0f;
				dlightcolor[2] += 400.0f;
			}
			// LordHavoc: added EF_RED and EF_BLUE
			if (effects & EF_RED) // red
			{
				dlightcolor[0] += 200.0f;
				dlightcolor[1] +=  20.0f;
				dlightcolor[2] +=  20.0f;
			}
			if (effects & EF_BLUE) // blue
			{
				dlightcolor[0] +=  20.0f;
				dlightcolor[1] +=  20.0f;
				dlightcolor[2] += 200.0f;
			}
			if (effects & EF_FLAME)
			{
				if (ent->render.model)
				{
					mins[0] = neworg[0] - 16.0f;
					mins[1] = neworg[1] - 16.0f;
					mins[2] = neworg[2] - 16.0f;
					maxs[0] = neworg[0] + 16.0f;
					maxs[1] = neworg[1] + 16.0f;
					maxs[2] = neworg[2] + 16.0f;
					// how many flames to make
					temp = (int) (cl.time * 300) - (int) (cl.oldtime * 300);
					CL_FlameCube(mins, maxs, temp);
				}
				d = lhrandom(200, 250);
				dlightcolor[0] += d * 1.0f;
				dlightcolor[1] += d * 0.7f;
				dlightcolor[2] += d * 0.3f;
			}
			if (effects & EF_STARDUST)
			{
				if (ent->render.model)
				{
					mins[0] = neworg[0] - 16.0f;
					mins[1] = neworg[1] - 16.0f;
					mins[2] = neworg[2] - 16.0f;
					maxs[0] = neworg[0] + 16.0f;
					maxs[1] = neworg[1] + 16.0f;
					maxs[2] = neworg[2] + 16.0f;
					// how many particles to make
					temp = (int) (cl.time * 200) - (int) (cl.oldtime * 200);
					CL_Stardust(mins, maxs, temp);
				}
				d = 100;
				dlightcolor[0] += d * 1.0f;
				dlightcolor[1] += d * 0.7f;
				dlightcolor[2] += d * 0.3f;
			}
		}

		if (ent->persistent.muzzleflash > 0)
		{
			v2[0] = ent->render.matrix.m[0][0] * 18 + neworg[0];
			v2[1] = ent->render.matrix.m[0][1] * 18 + neworg[1];
			v2[2] = ent->render.matrix.m[0][2] * 18 + neworg[2] + 16;
			CL_TraceLine(neworg, v2, v, NULL, 0, true, NULL);

			CL_AllocDlight (NULL, v, ent->persistent.muzzleflash, 1, 1, 1, 0, 0);
			ent->persistent.muzzleflash -= cl.frametime * 1000;
		}

		// LordHavoc: if the model has no flags, don't check each
		if (ent->render.model && ent->render.model->flags)
		{
			// note: EF_ROTATE handled above, above matrix calculation
			// only do trails if present in the previous frame as well
			if (ent->state_previous.active)
			{
				if (ent->render.model->flags & EF_GIB)
					CL_RocketTrail (oldorg, neworg, 2, ent);
				else if (ent->render.model->flags & EF_ZOMGIB)
					CL_RocketTrail (oldorg, neworg, 4, ent);
				else if (ent->render.model->flags & EF_TRACER)
				{
					CL_RocketTrail (oldorg, neworg, 3, ent);
					dlightcolor[0] += 0x10;
					dlightcolor[1] += 0x40;
					dlightcolor[2] += 0x10;
				}
				else if (ent->render.model->flags & EF_TRACER2)
				{
					CL_RocketTrail (oldorg, neworg, 5, ent);
					dlightcolor[0] += 0x50;
					dlightcolor[1] += 0x30;
					dlightcolor[2] += 0x10;
				}
				else if (ent->render.model->flags & EF_ROCKET)
				{
					CL_RocketTrail (oldorg, ent->render.origin, 0, ent);
					dlightcolor[0] += 200.0f;
					dlightcolor[1] += 160.0f;
					dlightcolor[2] +=  80.0f;
				}
				else if (ent->render.model->flags & EF_GRENADE)
				{
					if (ent->render.alpha == -1) // LordHavoc: Nehahra dem compatibility (cigar smoke)
						CL_RocketTrail (oldorg, neworg, 7, ent);
					else
						CL_RocketTrail (oldorg, neworg, 1, ent);
				}
				else if (ent->render.model->flags & EF_TRACER3)
				{
					CL_RocketTrail (oldorg, neworg, 6, ent);
					dlightcolor[0] += 0x50;
					dlightcolor[1] += 0x20;
					dlightcolor[2] += 0x40;
				}
			}
		}
		// LordHavoc: customizable glow
		if (ent->state_current.glowsize)
		{
			// * 4 for the expansion from 0-255 to 0-1023 range,
			// / 255 to scale down byte colors
			VectorMA(dlightcolor, ent->state_current.glowsize * (4.0f / 255.0f), (qbyte *)&palette_complete[ent->state_current.glowcolor], dlightcolor);
		}
		// LordHavoc: customizable trail
		if (ent->render.flags & RENDER_GLOWTRAIL)
			CL_RocketTrail2 (oldorg, neworg, ent->state_current.glowcolor, ent);

		if (dlightcolor[0] || dlightcolor[1] || dlightcolor[2])
		{
			VectorCopy(neworg, v);
			// hack to make glowing player light shine on their gun
			if (i == cl.viewentity/* && !chase_active.integer*/)
				v[2] += 30;
			CL_AllocDlight (&ent->render, v, 1, dlightcolor[0], dlightcolor[1], dlightcolor[2], 0, 0);
		}

		if (chase_active.integer && (ent->render.flags & RENDER_VIEWMODEL))
			continue;

		// don't show entities with no modelindex (note: this still shows
		// entities which have a modelindex that resolved to a NULL model)
		if (!ent->state_current.modelindex)
			continue;
		if (effects & EF_NODRAW)
			continue;

		// store a list of view-relative entities for later adjustment in view code
		if (ent->render.flags & RENDER_VIEWMODEL)
		{
			if (numviewmodels < MAXVIEWMODELS)
				viewmodels[numviewmodels++] = ent;
			continue;
		}

		Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);

		CL_BoundingBoxForEntity(&ent->render);
		if (ent->render.model && ent->render.model->name[0] == '*' && ent->render.model->type == mod_brush)
			cl_brushmodel_entities[cl_num_brushmodel_entities++] = &ent->render;

		// note: the cl.viewentity and intermission check is to hide player
		// shadow during intermission and during the Nehahra movie and
		// Nehahra cinematics
		if (!(ent->state_current.effects & EF_NOSHADOW)
		 && !(ent->state_current.effects & EF_ADDITIVE)
		 && (ent->state_current.alpha == 255)
		 && !(ent->render.flags & RENDER_VIEWMODEL)
		 && (i != cl.viewentity || (!cl.intermission && !Nehahrademcompatibility && !cl_noplayershadow.integer)))
			ent->render.flags |= RENDER_SHADOW;

		if (r_refdef.numentities < r_refdef.maxentities)
			r_refdef.entities[r_refdef.numentities++] = &ent->render;

		if (cl_num_entities < i + 1)
			cl_num_entities = i + 1;
	}
}

void CL_Effect(vec3_t org, int modelindex, int startframe, int framecount, float framerate)
{
	int i;
	cl_effect_t *e;
	if (!modelindex) // sanity check
		return;
	for (i = 0, e = cl_effects;i < cl_max_effects;i++, e++)
	{
		if (e->active)
			continue;
		e->active = true;
		VectorCopy(org, e->origin);
		e->modelindex = modelindex;
		e->starttime = cl.time;
		e->startframe = startframe;
		e->endframe = startframe + framecount;
		e->framerate = framerate;

		e->frame = 0;
		e->frame1time = cl.time;
		e->frame2time = cl.time;
		break;
	}
}

static void CL_RelinkEffects(void)
{
	int i, intframe;
	cl_effect_t *e;
	entity_t *ent;
	float frame;

	for (i = 0, e = cl_effects;i < cl_max_effects;i++, e++)
	{
		if (e->active)
		{
			frame = (cl.time - e->starttime) * e->framerate + e->startframe;
			intframe = frame;
			if (intframe < 0 || intframe >= e->endframe)
			{
				e->active = false;
				memset(e, 0, sizeof(*e));
				continue;
			}

			if (intframe != e->frame)
			{
				e->frame = intframe;
				e->frame1time = e->frame2time;
				e->frame2time = cl.time;
			}

			// if we're drawing effects, get a new temp entity
			// (NewTempEntity adds it to the render entities list for us)
			if (r_draweffects.integer && (ent = CL_NewTempEntity()))
			{
				// interpolation stuff
				ent->render.frame1 = intframe;
				ent->render.frame2 = intframe + 1;
				if (ent->render.frame2 >= e->endframe)
					ent->render.frame2 = -1; // disappear
				ent->render.framelerp = frame - intframe;
				ent->render.frame1time = e->frame1time;
				ent->render.frame2time = e->frame2time;

				// normal stuff
				//VectorCopy(e->origin, ent->render.origin);
				ent->render.model = cl.model_precache[e->modelindex];
				ent->render.frame = ent->render.frame2;
				ent->render.colormap = -1; // no special coloring
				//ent->render.scale = 1;
				ent->render.alpha = 1;

				Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, e->origin[0], e->origin[1], e->origin[2], 0, 0, 0, 1);
				Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
				CL_BoundingBoxForEntity(&ent->render);
			}
		}
	}
}

void CL_RelinkBeams (void)
{
	int i;
	beam_t *b;
	vec3_t dist, org;
	float d;
	entity_t *ent;
	float yaw, pitch;
	float forward;

	for (i = 0, b = cl_beams;i < cl_max_beams;i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		//if (b->entity == cl.viewentity)
		//	VectorCopy (cl_entities[cl.viewentity].render.origin, b->start);
		if (cl_beams_relative.integer && b->entity && cl_entities[b->entity].state_current.active && b->relativestartvalid)
		{
			entity_state_t *p = &cl_entities[b->entity].state_previous;
			//entity_state_t *c = &cl_entities[b->entity].state_current;
			entity_render_t *r = &cl_entities[b->entity].render;
			matrix4x4_t matrix, imatrix;
			if (b->relativestartvalid == 2)
			{
				// not really valid yet, we need to get the orientation now
				// (ParseBeam flagged this because it is received before
				//  entities are received, by now they have been received)
				// note: because players create lightning in their think
				// function (which occurs before movement), they actually
				// have some lag in it's location, so compare to the
				// previous player state, not the latest
				if (b->entity == cl.viewentity)
					Matrix4x4_CreateFromQuakeEntity(&matrix, cl.viewentoriginold[0], cl.viewentoriginold[1], cl.viewentoriginold[2] + 16, cl.viewangles[0], cl.viewangles[1], cl.viewangles[2], 1);
				else
					Matrix4x4_CreateFromQuakeEntity(&matrix, p->origin[0], p->origin[1], p->origin[2] + 16, p->angles[0], p->angles[1], p->angles[2], 1);
				Matrix4x4_Invert_Simple(&imatrix, &matrix);
				Matrix4x4_Transform(&imatrix, b->start, b->relativestart);
				Matrix4x4_Transform(&imatrix, b->end, b->relativeend);
				b->relativestartvalid = 1;
			}
			else
			{
				if (b->entity == cl.viewentity)
					Matrix4x4_CreateFromQuakeEntity(&matrix, cl.viewentorigin[0], cl.viewentorigin[1], cl.viewentorigin[2] + 16, cl.viewangles[0], cl.viewangles[1], cl.viewangles[2], 1);
				else
					Matrix4x4_CreateFromQuakeEntity(&matrix, r->origin[0], r->origin[1], r->origin[2] + 16, r->angles[0], r->angles[1], r->angles[2], 1);
				Matrix4x4_Transform(&matrix, b->relativestart, b->start);
				Matrix4x4_Transform(&matrix, b->relativeend, b->end);
			}
		}

		if (b->lightning)
		{
			if (cl_beams_lightatend.integer)
				CL_AllocDlight (NULL, b->end, 200, 0.3, 0.7, 1, 0, 0);
			if (cl_beams_polygons.integer)
				continue;
		}

		// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalizeLength(dist);
		while (d > 0)
		{
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			//VectorCopy (org, ent->render.origin);
			ent->render.model = b->model;
			ent->render.effects = EF_FULLBRIGHT;
			//ent->render.angles[0] = pitch;
			//ent->render.angles[1] = yaw;
			//ent->render.angles[2] = rand()%360;
			Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, org[0], org[1], org[2], pitch, yaw, lhrandom(0, 360), 1);
			Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
			CL_BoundingBoxForEntity(&ent->render);
			VectorMA(org, 30, dist, org);
			d -= 30;
		}
	}
}

cvar_t r_lightningbeam_thickness = {CVAR_SAVE, "r_lightningbeam_thickness", "4"};
cvar_t r_lightningbeam_scroll = {CVAR_SAVE, "r_lightningbeam_scroll", "5"};
cvar_t r_lightningbeam_repeatdistance = {CVAR_SAVE, "r_lightningbeam_repeatdistance", "1024"};
cvar_t r_lightningbeam_color_red = {CVAR_SAVE, "r_lightningbeam_color_red", "1"};
cvar_t r_lightningbeam_color_green = {CVAR_SAVE, "r_lightningbeam_color_green", "1"};
cvar_t r_lightningbeam_color_blue = {CVAR_SAVE, "r_lightningbeam_color_blue", "1"};
cvar_t r_lightningbeam_qmbtexture = {CVAR_SAVE, "r_lightningbeam_qmbtexture", "0"};

rtexture_t *r_lightningbeamtexture;
rtexture_t *r_lightningbeamqmbtexture;
rtexturepool_t *r_lightningbeamtexturepool;

int r_lightningbeamelements[18] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11};

void r_lightningbeams_start(void)
{
	r_lightningbeamtexturepool = R_AllocTexturePool();
	r_lightningbeamtexture = NULL;
	r_lightningbeamqmbtexture = NULL;
}

void r_lightningbeams_setupqmbtexture(void)
{
	r_lightningbeamqmbtexture = loadtextureimage(r_lightningbeamtexturepool, "textures/particles/lightning.pcx", 0, 0, false, TEXF_ALPHA | TEXF_PRECACHE);
	if (r_lightningbeamqmbtexture == NULL)
		Cvar_SetValueQuick(&r_lightningbeam_qmbtexture, false);
}

void r_lightningbeams_setuptexture(void)
{
#if 0
#define BEAMWIDTH 128
#define BEAMHEIGHT 64
#define PATHPOINTS 8
	int i, j, px, py, nearestpathindex, imagenumber;
	float particlex, particley, particlexv, particleyv, dx, dy, s, maxpathstrength;
	qbyte *pixels;
	int *image;
	struct {float x, y, strength;} path[PATHPOINTS], temppath;

	image = Mem_Alloc(tempmempool, BEAMWIDTH * BEAMHEIGHT * sizeof(int));
	pixels = Mem_Alloc(tempmempool, BEAMWIDTH * BEAMHEIGHT * sizeof(qbyte[4]));

	for (imagenumber = 0, maxpathstrength = 0.0339476;maxpathstrength < 0.5;imagenumber++, maxpathstrength += 0.01)
	{
	for (i = 0;i < PATHPOINTS;i++)
	{
		path[i].x = lhrandom(0, 1);
		path[i].y = lhrandom(0.2, 0.8);
		path[i].strength = lhrandom(0, 1);
	}
	for (i = 0;i < PATHPOINTS;i++)
	{
		for (j = i + 1;j < PATHPOINTS;j++)
		{
			if (path[j].x < path[i].x)
			{
				temppath = path[j];
				path[j] = path[i];
				path[i] = temppath;
			}
		}
	}
	particlex = path[0].x;
	particley = path[0].y;
	particlexv = lhrandom(0, 0.02);
	particlexv = lhrandom(-0.02, 0.02);
	memset(image, 0, BEAMWIDTH * BEAMHEIGHT * sizeof(int));
	for (i = 0;i < 65536;i++)
	{
		for (nearestpathindex = 0;nearestpathindex < PATHPOINTS;nearestpathindex++)
			if (path[nearestpathindex].x > particlex)
				break;
		nearestpathindex %= PATHPOINTS;
		dx = path[nearestpathindex].x + lhrandom(-0.01, 0.01);dx = bound(0, dx, 1) - particlex;if (dx < 0) dx += 1;
		dy = path[nearestpathindex].y + lhrandom(-0.01, 0.01);dy = bound(0, dy, 1) - particley;
		s = path[nearestpathindex].strength / sqrt(dx*dx+dy*dy);
		particlexv = particlexv /* (1 - lhrandom(0.08, 0.12))*/ + dx * s;
		particleyv = particleyv /* (1 - lhrandom(0.08, 0.12))*/ + dy * s;
		particlex += particlexv * maxpathstrength;particlex -= (int) particlex;
		particley += particleyv * maxpathstrength;particley = bound(0, particley, 1);
		px = particlex * BEAMWIDTH;
		py = particley * BEAMHEIGHT;
		if (px >= 0 && py >= 0 && px < BEAMWIDTH && py < BEAMHEIGHT)
			image[py*BEAMWIDTH+px] += 16;
	}

	for (py = 0;py < BEAMHEIGHT;py++)
	{
		for (px = 0;px < BEAMWIDTH;px++)
		{
			pixels[(py*BEAMWIDTH+px)*4+0] = bound(0, image[py*BEAMWIDTH+px] * 1.0f, 255.0f);
			pixels[(py*BEAMWIDTH+px)*4+1] = bound(0, image[py*BEAMWIDTH+px] * 1.0f, 255.0f);
			pixels[(py*BEAMWIDTH+px)*4+2] = bound(0, image[py*BEAMWIDTH+px] * 1.0f, 255.0f);
			pixels[(py*BEAMWIDTH+px)*4+3] = 255;
		}
	}

	Image_WriteTGARGBA(va("lightningbeam%i.tga", imagenumber), BEAMWIDTH, BEAMHEIGHT, pixels);
	}

	r_lightningbeamtexture = R_LoadTexture2D(r_lightningbeamtexturepool, "lightningbeam", BEAMWIDTH, BEAMHEIGHT, pixels, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);

	Mem_Free(pixels);
	Mem_Free(image);
#else
#define BEAMWIDTH 64
#define BEAMHEIGHT 128
	float r, g, b, intensity, fx, width, center;
	int x, y;
	qbyte *data, *noise1, *noise2;

	data = Mem_Alloc(tempmempool, BEAMWIDTH * BEAMHEIGHT * 4);
	noise1 = Mem_Alloc(tempmempool, BEAMHEIGHT * BEAMHEIGHT);
	noise2 = Mem_Alloc(tempmempool, BEAMHEIGHT * BEAMHEIGHT);
	fractalnoise(noise1, BEAMHEIGHT, BEAMHEIGHT / 8);
	fractalnoise(noise2, BEAMHEIGHT, BEAMHEIGHT / 16);

	for (y = 0;y < BEAMHEIGHT;y++)
	{
		width = 0.15;//((noise1[y * BEAMHEIGHT] * (1.0f / 256.0f)) * 0.1f + 0.1f);
		center = (noise1[y * BEAMHEIGHT + (BEAMHEIGHT / 2)] / 256.0f) * (1.0f - width * 2.0f) + width;
		for (x = 0;x < BEAMWIDTH;x++, fx++)
		{
			fx = (((float) x / BEAMWIDTH) - center) / width;
			intensity = 1.0f - sqrt(fx * fx);
			if (intensity > 0)
				intensity = pow(intensity, 2) * ((noise2[y * BEAMHEIGHT + x] * (1.0f / 256.0f)) * 0.33f + 0.66f);
			intensity = bound(0, intensity, 1);
			r = intensity * 1.0f;
			g = intensity * 1.0f;
			b = intensity * 1.0f;
			data[(y * BEAMWIDTH + x) * 4 + 0] = (qbyte)(bound(0, r, 1) * 255.0f);
			data[(y * BEAMWIDTH + x) * 4 + 1] = (qbyte)(bound(0, g, 1) * 255.0f);
			data[(y * BEAMWIDTH + x) * 4 + 2] = (qbyte)(bound(0, b, 1) * 255.0f);
			data[(y * BEAMWIDTH + x) * 4 + 3] = (qbyte)255;
		}
	}

	r_lightningbeamtexture = R_LoadTexture2D(r_lightningbeamtexturepool, "lightningbeam", BEAMWIDTH, BEAMHEIGHT, data, TEXTYPE_RGBA, TEXF_PRECACHE, NULL);
	Mem_Free(noise1);
	Mem_Free(noise2);
	Mem_Free(data);
#endif
}

void r_lightningbeams_shutdown(void)
{
	r_lightningbeamtexture = NULL;
	r_lightningbeamqmbtexture = NULL;
	R_FreeTexturePool(&r_lightningbeamtexturepool);
}

void r_lightningbeams_newmap(void)
{
}

void R_LightningBeams_Init(void)
{
	Cvar_RegisterVariable(&r_lightningbeam_thickness);
	Cvar_RegisterVariable(&r_lightningbeam_scroll);
	Cvar_RegisterVariable(&r_lightningbeam_repeatdistance);
	Cvar_RegisterVariable(&r_lightningbeam_color_red);
	Cvar_RegisterVariable(&r_lightningbeam_color_green);
	Cvar_RegisterVariable(&r_lightningbeam_color_blue);
	Cvar_RegisterVariable(&r_lightningbeam_qmbtexture);
	R_RegisterModule("R_LightningBeams", r_lightningbeams_start, r_lightningbeams_shutdown, r_lightningbeams_newmap);
}

void R_CalcLightningBeamPolygonVertex3f(float *v, const float *start, const float *end, const float *offset)
{
	// near right corner
	VectorAdd     (start, offset, (v + 0));
	// near left corner
	VectorSubtract(start, offset, (v + 3));
	// far left corner
	VectorSubtract(end  , offset, (v + 6));
	// far right corner
	VectorAdd     (end  , offset, (v + 9));
}

void R_CalcLightningBeamPolygonTexCoord2f(float *tc, float t1, float t2)
{
	if (r_lightningbeam_qmbtexture.integer)
	{
		// near right corner
		tc[0] = t1;tc[1] = 0;
		// near left corner
		tc[2] = t1;tc[3] = 1;
		// far left corner
		tc[4] = t2;tc[5] = 1;
		// far right corner
		tc[6] = t2;tc[7] = 0;
	}
	else
	{
		// near right corner
		tc[0] = 0;tc[1] = t1;
		// near left corner
		tc[2] = 1;tc[3] = t1;
		// far left corner
		tc[4] = 1;tc[5] = t2;
		// far right corner
		tc[6] = 0;tc[7] = t2;
	}
}

void R_FogLightningBeam_Vertex3f_Color4f(const float *v, float *c, int numverts, float r, float g, float b, float a)
{
	int i;
	vec3_t fogvec;
	float ifog;
	for (i = 0;i < numverts;i++, v += 3, c += 4)
	{
		VectorSubtract(v, r_origin, fogvec);
		ifog = 1 - exp(fogdensity/DotProduct(fogvec,fogvec));
		c[0] = r * ifog;
		c[1] = g * ifog;
		c[2] = b * ifog;
		c[3] = a;
	}
}

float beamrepeatscale;

void R_DrawLightningBeamCallback(const void *calldata1, int calldata2)
{
	const beam_t *b = calldata1;
	rmeshstate_t m;
	vec3_t beamdir, right, up, offset;
	float length, t1, t2;
	memset(&m, 0, sizeof(m));
	m.blendfunc1 = GL_SRC_ALPHA;
	m.blendfunc2 = GL_ONE;
	if (r_lightningbeam_qmbtexture.integer && r_lightningbeamqmbtexture == NULL)
		r_lightningbeams_setupqmbtexture();
	if (!r_lightningbeam_qmbtexture.integer && r_lightningbeamtexture == NULL)
		r_lightningbeams_setuptexture();
	if (r_lightningbeam_qmbtexture.integer)
		m.tex[0] = R_GetTexture(r_lightningbeamqmbtexture);
	else
		m.tex[0] = R_GetTexture(r_lightningbeamtexture);
	R_Mesh_State(&m);
	R_Mesh_Matrix(&r_identitymatrix);

	// calculate beam direction (beamdir) vector and beam length
	// get difference vector
	VectorSubtract(b->end, b->start, beamdir);
	// find length of difference vector
	length = sqrt(DotProduct(beamdir, beamdir));
	// calculate scale to make beamdir a unit vector (normalized)
	t1 = 1.0f / length;
	// scale beamdir so it is now normalized
	VectorScale(beamdir, t1, beamdir);

	// calculate up vector such that it points toward viewer, and rotates around the beamdir
	// get direction from start of beam to viewer
	VectorSubtract(r_origin, b->start, up);
	// remove the portion of the vector that moves along the beam
	// (this leaves only a vector pointing directly away from the beam)
	t1 = -DotProduct(up, beamdir);
	VectorMA(up, t1, beamdir, up);
	// now we have a vector pointing away from the beam, now we need to normalize it
	VectorNormalizeFast(up);
	// generate right vector from forward and up, the result is already normalized
	// (CrossProduct returns a vector of multiplied length of the two inputs)
	CrossProduct(beamdir, up, right);

	// calculate T coordinate scrolling (start and end texcoord along the beam)
	t1 = cl.time * -r_lightningbeam_scroll.value;// + beamrepeatscale * DotProduct(b->start, beamdir);
	t1 = t1 - (int) t1;
	t2 = t1 + beamrepeatscale * length;

	// the beam is 3 polygons in this configuration:
	//  *   2
	//   * *
	// 1******
	//   * *
	//  *   3
	// they are showing different portions of the beam texture, creating an
	// illusion of a beam that appears to curl around in 3D space
	// (and realize that the whole polygon assembly orients itself to face
	//  the viewer)

	R_Mesh_GetSpace(12);

	// polygon 1, verts 0-3
	VectorScale(right, r_lightningbeam_thickness.value, offset);
	R_CalcLightningBeamPolygonVertex3f(varray_vertex3f, b->start, b->end, offset);
	R_CalcLightningBeamPolygonTexCoord2f(varray_texcoord2f[0], t1, t2);

	// polygon 2, verts 4-7
	VectorAdd(right, up, offset);
	VectorScale(offset, r_lightningbeam_thickness.value * 0.70710681f, offset);
	R_CalcLightningBeamPolygonVertex3f(varray_vertex3f + 12, b->start, b->end, offset);
	R_CalcLightningBeamPolygonTexCoord2f(varray_texcoord2f[0] + 8, t1 + 0.33, t2 + 0.33);

	// polygon 3, verts 8-11
	VectorSubtract(right, up, offset);
	VectorScale(offset, r_lightningbeam_thickness.value * 0.70710681f, offset);
	R_CalcLightningBeamPolygonVertex3f(varray_vertex3f + 24, b->start, b->end, offset);
	R_CalcLightningBeamPolygonTexCoord2f(varray_texcoord2f[0] + 16, t1 + 0.66, t2 + 0.66);

	if (fogenabled)
	{
		// per vertex colors if fog is used
		GL_UseColorArray();
		R_FogLightningBeam_Vertex3f_Color4f(varray_vertex3f, varray_color4f, 12, r_lightningbeam_color_red.value, r_lightningbeam_color_green.value, r_lightningbeam_color_blue.value, 1);
	}
	else
	{
		// solid color if fog is not used
		GL_Color(r_lightningbeam_color_red.value, r_lightningbeam_color_green.value, r_lightningbeam_color_blue.value, 1);
	}

	// draw the 3 polygons as one batch of 6 triangles using the 12 vertices
	R_Mesh_Draw(12, 6, r_lightningbeamelements);
}

void R_DrawLightningBeams (void)
{
	int i;
	beam_t *b;
	vec3_t org;

	if (!cl_beams_polygons.integer)
		return;

	beamrepeatscale = 1.0f / r_lightningbeam_repeatdistance.value;
	for (i = 0, b = cl_beams;i < cl_max_beams;i++, b++)
	{
		if (b->model && b->endtime >= cl.time && b->lightning)
		{
			VectorAdd(b->start, b->end, org);
			VectorScale(org, 0.5f, org);
			R_MeshQueue_AddTransparent(org, R_DrawLightningBeamCallback, b, 0);
		}
	}
}


void CL_LerpPlayer(float frac)
{
	int i;
	float d;

	if (cl.entitydatabase.numframes && cl.viewentity == cl.playerentity)
	{
		cl.viewentorigin[0] = cl.viewentoriginold[0] + frac * (cl.viewentoriginnew[0] - cl.viewentoriginold[0]);
		cl.viewentorigin[1] = cl.viewentoriginold[1] + frac * (cl.viewentoriginnew[1] - cl.viewentoriginold[1]);
		cl.viewentorigin[2] = cl.viewentoriginold[2] + frac * (cl.viewentoriginnew[2] - cl.viewentoriginold[2]);
	}
	else
	{
		VectorCopy (cl_entities[cl.viewentity].state_previous.origin, cl.viewentoriginold);
		VectorCopy (cl_entities[cl.viewentity].state_current.origin, cl.viewentoriginnew);
		VectorCopy (cl_entities[cl.viewentity].render.origin, cl.viewentorigin);
	}

	cl.viewzoom = cl.viewzoomold + frac * (cl.viewzoomnew - cl.viewzoomold);

	for (i = 0;i < 3;i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback)
	{
		// interpolate the angles
		for (i = 0;i < 3;i++)
		{
			d = cl.mviewangles[0][i] - cl.mviewangles[1][i];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[i] = cl.mviewangles[1][i] + frac * d;
		}
	}
}

void CL_RelinkEntities (void)
{
	float frac;

	// fraction from previous network update to current
	frac = CL_LerpPoint();

	CL_ClearTempEntities();
	CL_DecayLights();
	CL_RelinkWorld();
	CL_RelinkStaticEntities();
	CL_RelinkNetworkEntities();
	CL_RelinkEffects();
	CL_MoveParticles();

	CL_LerpPlayer(frac);

	CL_RelinkBeams();
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

		if (cl_shownet.integer)
			netshown = true;

		CL_ParseServerMessage ();
	}
	while (ret && cls.state == ca_connected);

	if (netshown)
		Con_Printf ("\n");

	r_refdef.numentities = 0;
	cl_num_entities = 0;
	cl_num_brushmodel_entities = 0;

	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		CL_RelinkEntities ();

		// run cgame code (which can add more entities)
		CL_CGVM_Frame();
	}

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

		IN_PreMove(); // OS independent code

	// allow mice or other external controllers to add to the move
		IN_Move (&cmd);

		IN_PostMove(); // OS independent code

	// send the unreliable message
		CL_SendMove (&cmd);
	}
#ifndef NOROUTINGFIX
	else if (cls.signon == 0 && !cls.demoplayback)
	{
		// LordHavoc: fix for NAT routing of netquake:
		// bounce back a clc_nop message to the newly allocated server port,
		// to establish a routing connection for incoming frames,
		// the server waits for this before sending anything
		if (realtime > cl.sendnoptime)
		{
			cl.sendnoptime = realtime + 3;
			Con_DPrintf("sending clc_nop to get server's attention\n");
			{
				sizebuf_t buf;
				qbyte data[128];
				buf.maxsize = 128;
				buf.cursize = 0;
				buf.data = data;
				MSG_WriteByte(&buf, clc_nop);
				if (NET_SendUnreliableMessage (cls.netcon, &buf) == -1)
				{
					Con_Printf ("CL_SendCmd: lost server connection\n");
					CL_Disconnect ();
				}
			}
		}
	}
#endif

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
		if (developer.integer)
			SZ_HexDumpToConsole(&cls.message);
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

// LordHavoc: pausedemo command
static void CL_PauseDemo_f (void)
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
static void CL_PModel_f (void)
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
		if (cl_pmodel.integer == i)
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
static void CL_Fog_f (void)
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
	cl_scores_mempool = Mem_AllocPool("client player info");
	cl_entities_mempool = Mem_AllocPool("client entities");
	cl_refdef_mempool = Mem_AllocPool("refdef");

	memset(&r_refdef, 0, sizeof(r_refdef));
	// max entities sent to renderer per frame
	r_refdef.maxentities = MAX_EDICTS + 256 + 512;
	r_refdef.entities = Mem_Alloc(cl_refdef_mempool, sizeof(entity_render_t *) * r_refdef.maxentities);
	// 256k drawqueue buffer
	r_refdef.maxdrawqueuesize = 256 * 1024;
	r_refdef.drawqueue = Mem_Alloc(cl_refdef_mempool, r_refdef.maxdrawqueuesize);

	SZ_Alloc (&cls.message, 1024, "cls.message");

	CL_InitInput ();
	CL_InitTEnts ();

//
// register our commands
//
	Cvar_RegisterVariable (&cl_name);
	Cvar_RegisterVariable (&cl_color);
	if (gamemode == GAME_NEHAHRA)
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

	Cvar_RegisterVariable (&cl_itembobspeed);
	Cvar_RegisterVariable (&cl_itembobheight);

	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand ("fog", CL_Fog_f);

	// LordHavoc: added pausedemo
	Cmd_AddCommand ("pausedemo", CL_PauseDemo_f);
	if (gamemode == GAME_NEHAHRA)
		Cmd_AddCommand ("pmodel", CL_PModel_f);

	Cvar_RegisterVariable(&r_draweffects);
	Cvar_RegisterVariable(&cl_explosions);
	Cvar_RegisterVariable(&cl_stainmaps);
	Cvar_RegisterVariable(&cl_beams_polygons);
	Cvar_RegisterVariable(&cl_beams_relative);
	Cvar_RegisterVariable(&cl_beams_lightatend);
	Cvar_RegisterVariable(&cl_noplayershadow);

	R_LightningBeams_Init();

	CL_Parse_Init();
	CL_Particles_Init();
	CL_Screen_Init();
	CL_CGVM_Init();

	CL_Video_Init();
}

