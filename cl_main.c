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
#include "csprogs.h"
#include "r_shadow.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

cvar_t csqc_progname = {0, "csqc_progname","csprogs.dat","name of csprogs.dat file to load"};	//[515]: csqc crc check and right csprogs name according to progs.dat
cvar_t csqc_progcrc = {CVAR_READONLY, "csqc_progcrc","0","CRC of csprogs.dat file to load"};

cvar_t cl_shownet = {0, "cl_shownet","0","1 = print packet size, 2 = print packet message list"};
cvar_t cl_nolerp = {0, "cl_nolerp", "0","network update smoothing"};

cvar_t cl_itembobheight = {0, "cl_itembobheight", "0","how much items bob up and down (try 8)"};
cvar_t cl_itembobspeed = {0, "cl_itembobspeed", "0.5","how frequently items bob up and down"};

cvar_t lookspring = {CVAR_SAVE, "lookspring","0","returns pitch to level with the floor when no longer holding a pitch key"};
cvar_t lookstrafe = {CVAR_SAVE, "lookstrafe","0","move instead of turning"};
cvar_t sensitivity = {CVAR_SAVE, "sensitivity","3","mouse speed multiplier"};

cvar_t m_pitch = {CVAR_SAVE, "m_pitch","0.022","mouse pitch speed multiplier"};
cvar_t m_yaw = {CVAR_SAVE, "m_yaw","0.022","mouse yaw speed multiplier"};
cvar_t m_forward = {CVAR_SAVE, "m_forward","1","mouse forward speed multiplier"};
cvar_t m_side = {CVAR_SAVE, "m_side","0.8","mouse side speed multiplier"};

cvar_t freelook = {CVAR_SAVE, "freelook", "1","mouse controls pitch instead of forward/back"};

cvar_t r_draweffects = {0, "r_draweffects", "1","renders temporary sprite effects"};

cvar_t cl_explosions_alpha_start = {CVAR_SAVE, "cl_explosions_alpha_start", "1.5","starting alpha of an explosion shell"};
cvar_t cl_explosions_alpha_end = {CVAR_SAVE, "cl_explosions_alpha_end", "0","end alpha of an explosion shell (just before it disappears)"};
cvar_t cl_explosions_size_start = {CVAR_SAVE, "cl_explosions_size_start", "16","starting size of an explosion shell"};
cvar_t cl_explosions_size_end = {CVAR_SAVE, "cl_explosions_size_end", "128","ending alpha of an explosion shell (just before it disappears)"};
cvar_t cl_explosions_lifetime = {CVAR_SAVE, "cl_explosions_lifetime", "0.5","how long an explosion shell lasts"};

cvar_t cl_stainmaps = {CVAR_SAVE, "cl_stainmaps", "1","stains lightmaps, much faster than decals but blurred"};
cvar_t cl_stainmaps_clearonload = {CVAR_SAVE, "cl_stainmaps_clearonload", "1","clear stainmaps on map restart"};

cvar_t cl_beams_polygons = {CVAR_SAVE, "cl_beams_polygons", "1","use beam polygons instead of models"};
cvar_t cl_beams_relative = {CVAR_SAVE, "cl_beams_relative", "1","beams are relative to owner (smooth sweeps)"};
cvar_t cl_beams_lightatend = {CVAR_SAVE, "cl_beams_lightatend", "0","make a light at the end of the beam"};

cvar_t cl_noplayershadow = {CVAR_SAVE, "cl_noplayershadow", "0","hide player shadow"};

cvar_t qport = {0, "qport", "0", "identification key for playing on qw servers (allows you to maintain a connection to a quakeworld server even if your port changes)"};

cvar_t cl_prydoncursor = {0, "cl_prydoncursor", "0", "enables a mouse pointer which is able to click on entities in the world, useful for point and click mods, see PRYDON_CLIENTCURSOR extension in dpextensions.qc"};

cvar_t cl_deathnoviewmodel = {0, "cl_deathnoviewmodel", "1", "hides gun model when dead"};

vec3_t cl_playerstandmins;
vec3_t cl_playerstandmaxs;
vec3_t cl_playercrouchmins;
vec3_t cl_playercrouchmaxs;

mempool_t *cl_mempool;

client_static_t	cls;
client_state_t	cl;

int cl_max_entities;
int cl_max_csqcentities;
int cl_max_static_entities;
int cl_max_temp_entities;
int cl_max_effects;
int cl_max_beams;
int cl_max_dlights;
int cl_max_lightstyle;
int cl_max_brushmodel_entities;
int cl_activedlights;
int cl_activeeffects;
int cl_activebeams;

entity_t *cl_entities;
entity_t *cl_csqcentities;	//[515]: csqc
unsigned char *cl_entities_active;
unsigned char *cl_csqcentities_active;	//[515]: csqc
entity_t *cl_static_entities;
entity_t *cl_temp_entities;
cl_effect_t *cl_effects;
beam_t *cl_beams;
dlight_t *cl_dlights;
lightstyle_t *cl_lightstyle;
int *cl_brushmodel_entities;

int cl_num_entities;
int cl_num_csqcentities;	//[515]: csqc
int cl_num_static_entities;
int cl_num_temp_entities;
int cl_num_brushmodel_entities;

// keep track of quake entities because they need to be killed if they get stale
extern int cl_lastquakeentity;
extern unsigned char cl_isquakeentity[MAX_EDICTS];

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState(void)
{
	int i;
	entity_t *ent;

	if (cl_entities) Mem_Free(cl_entities);cl_entities = NULL;
	if (cl_csqcentities) Mem_Free(cl_csqcentities);cl_csqcentities = NULL;	//[515]: csqc
	if (cl_entities_active) Mem_Free(cl_entities_active);cl_entities_active = NULL;
	if (cl_csqcentities_active) Mem_Free(cl_csqcentities_active);cl_csqcentities_active = NULL;	//[515]: csqc
	if (cl_static_entities) Mem_Free(cl_static_entities);cl_static_entities = NULL;
	if (cl_temp_entities) Mem_Free(cl_temp_entities);cl_temp_entities = NULL;
	if (cl_effects) Mem_Free(cl_effects);cl_effects = NULL;
	if (cl_beams) Mem_Free(cl_beams);cl_beams = NULL;
	if (cl_dlights) Mem_Free(cl_dlights);cl_dlights = NULL;
	if (cl_lightstyle) Mem_Free(cl_lightstyle);cl_lightstyle = NULL;
	if (cl_brushmodel_entities) Mem_Free(cl_brushmodel_entities);cl_brushmodel_entities = NULL;
	if (cl.entitydatabase) EntityFrame_FreeDatabase(cl.entitydatabase);cl.entitydatabase = NULL;
	if (cl.entitydatabase4) EntityFrame4_FreeDatabase(cl.entitydatabase4);cl.entitydatabase4 = NULL;
	if (cl.entitydatabaseqw) EntityFrameQW_FreeDatabase(cl.entitydatabaseqw);cl.entitydatabaseqw = NULL;
	if (cl.scores) Mem_Free(cl.scores);cl.scores = NULL;

	if (!sv.active)
		Host_ClearMemory ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	S_StopAllSounds();

	// reset the view zoom interpolation
	cl.mviewzoom[0] = cl.mviewzoom[1] = 1;

	cl_num_entities = 0;
	cl_num_csqcentities = 0;	//[515]: csqc
	cl_num_static_entities = 0;
	cl_num_temp_entities = 0;
	cl_num_brushmodel_entities = 0;

	// tweak these if the game runs out
	cl_max_entities = 256;
	cl_max_csqcentities = 256;	//[515]: csqc
	cl_max_static_entities = 256;
	cl_max_temp_entities = 512;
	cl_max_effects = 256;
	cl_max_beams = 256;
	cl_max_dlights = MAX_DLIGHTS;
	cl_max_lightstyle = MAX_LIGHTSTYLES;
	cl_max_brushmodel_entities = MAX_EDICTS;
	cl_activedlights = 0;
	cl_activeeffects = 0;
	cl_activebeams = 0;

	cl_entities = (entity_t *)Mem_Alloc(cl_mempool, cl_max_entities * sizeof(entity_t));
	cl_csqcentities = (entity_t *)Mem_Alloc(cl_mempool, cl_max_csqcentities * sizeof(entity_t));	//[515]: csqc
	cl_entities_active = (unsigned char *)Mem_Alloc(cl_mempool, cl_max_brushmodel_entities * sizeof(unsigned char));
	cl_csqcentities_active = (unsigned char *)Mem_Alloc(cl_mempool, cl_max_brushmodel_entities * sizeof(unsigned char));	//[515]: csqc
	cl_static_entities = (entity_t *)Mem_Alloc(cl_mempool, cl_max_static_entities * sizeof(entity_t));
	cl_temp_entities = (entity_t *)Mem_Alloc(cl_mempool, cl_max_temp_entities * sizeof(entity_t));
	cl_effects = (cl_effect_t *)Mem_Alloc(cl_mempool, cl_max_effects * sizeof(cl_effect_t));
	cl_beams = (beam_t *)Mem_Alloc(cl_mempool, cl_max_beams * sizeof(beam_t));
	cl_dlights = (dlight_t *)Mem_Alloc(cl_mempool, cl_max_dlights * sizeof(dlight_t));
	cl_lightstyle = (lightstyle_t *)Mem_Alloc(cl_mempool, cl_max_lightstyle * sizeof(lightstyle_t));
	cl_brushmodel_entities = (int *)Mem_Alloc(cl_mempool, cl_max_brushmodel_entities * sizeof(int));

	cl_lastquakeentity = 0;
	memset(cl_isquakeentity, 0, sizeof(cl_isquakeentity));

	// LordHavoc: have to set up the baseline info for alpha and other stuff
	for (i = 0;i < cl_max_entities;i++)
	{
		cl_entities[i].state_baseline = defaultstate;
		cl_entities[i].state_previous = defaultstate;
		cl_entities[i].state_current = defaultstate;
	}

	for (i = 0;i < cl_max_csqcentities;i++)
	{
		cl_csqcentities[i].state_baseline = defaultstate;	//[515]: csqc
		cl_csqcentities[i].state_previous = defaultstate;	//[515]: csqc
		cl_csqcentities[i].state_current = defaultstate;	//[515]: csqc
		cl_csqcentities[i].csqc = true;
		cl_csqcentities[i].state_current.number = -i;
	}

	if (gamemode == GAME_NEXUIZ)
	{
		VectorSet(cl_playerstandmins, -16, -16, -24);
		VectorSet(cl_playerstandmaxs, 16, 16, 45);
		VectorSet(cl_playercrouchmins, -16, -16, -24);
		VectorSet(cl_playercrouchmaxs, 16, 16, 25);
	}
	else
	{
		VectorSet(cl_playerstandmins, -16, -16, -24);
		VectorSet(cl_playerstandmaxs, 16, 16, 24);
		VectorSet(cl_playercrouchmins, -16, -16, -24);
		VectorSet(cl_playercrouchmaxs, 16, 16, 24);
	}

	// disable until we get textures for it
	R_ResetSkyBox();

	ent = &cl_entities[0];
	// entire entity array was cleared, so just fill in a few fields
	ent->state_current.active = true;
	ent->render.model = cl.worldmodel = NULL; // no world model yet
	ent->render.scale = 1; // some of the renderer still relies on scale
	ent->render.alpha = 1;
	ent->render.colormap = -1; // no special coloring
	ent->render.flags = RENDER_SHADOW | RENDER_LIGHT;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, 0, 0, 0, 0, 0, 0, 1);
	Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
	CL_BoundingBoxForEntity(&ent->render);

	// noclip is turned off at start
	noclip_anglehack = false;

	// mark all frames invalid for delta
	memset(cl.qw_deltasequence, -1, sizeof(cl.qw_deltasequence));

	CL_Screen_NewMap();
	CL_Particles_Clear();
}

void CL_ExpandEntities(int num)
{
	int i, oldmaxentities;
	entity_t *oldentities;
	if (num >= cl_max_entities)
	{
		if (!cl_entities)
			Sys_Error("CL_ExpandEntities: cl_entities not initialized");
		if (num >= MAX_EDICTS)
			Host_Error("CL_ExpandEntities: num %i >= %i", num, MAX_EDICTS);
		oldmaxentities = cl_max_entities;
		oldentities = cl_entities;
		cl_max_entities = (num & ~255) + 256;
		cl_entities = (entity_t *)Mem_Alloc(cl_mempool, cl_max_entities * sizeof(entity_t));
		memcpy(cl_entities, oldentities, oldmaxentities * sizeof(entity_t));
		Mem_Free(oldentities);
		for (i = oldmaxentities;i < cl_max_entities;i++)
		{
			cl_entities[i].state_baseline = defaultstate;
			cl_entities[i].state_previous = defaultstate;
			cl_entities[i].state_current = defaultstate;
		}
	}
}

void CL_ExpandCSQCEntities(int num)
{
	int i, oldmaxentities;
	entity_t *oldentities;
	if (num >= cl_max_csqcentities)
	{
		if (!cl_csqcentities)
			Sys_Error("CL_ExpandCSQCEntities: cl_csqcentities not initialized\n");
		if (num >= MAX_EDICTS)
			Host_Error("CL_ExpandCSQCEntities: num %i >= %i\n", num, MAX_EDICTS);
		oldmaxentities = cl_max_csqcentities;
		oldentities = cl_csqcentities;
		cl_max_csqcentities = (num & ~255) + 256;
		cl_csqcentities = Mem_Alloc(cl_mempool, cl_max_csqcentities * sizeof(entity_t));
		memcpy(cl_csqcentities, oldentities, oldmaxentities * sizeof(entity_t));
		Mem_Free(oldentities);
		for (i = oldmaxentities;i < cl_max_csqcentities;i++)
		{
			cl_csqcentities[i].state_baseline = defaultstate;
			cl_csqcentities[i].state_previous = defaultstate;
			cl_csqcentities[i].state_current = defaultstate;
			cl_csqcentities[i].csqc = true;
			cl_csqcentities[i].state_current.number = -i;
		}
	}
}

void CL_VM_ShutDown (void);
/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect(void)
{
	if (cls.state == ca_dedicated)
		return;

	Con_DPrintf("CL_Disconnect\n");

	CL_VM_ShutDown();
// stop sounds (especially looping!)
	S_StopAllSounds ();

	// clear contents blends
	cl.cshifts[0].percent = 0;
	cl.cshifts[1].percent = 0;
	cl.cshifts[2].percent = 0;
	cl.cshifts[3].percent = 0;

	cl.worldmodel = NULL;

	if (cls.demoplayback)
		CL_StopPlayback();
	else if (cls.netcon)
	{
		sizebuf_t buf;
		unsigned char bufdata[8];
		if (cls.demorecording)
			CL_Stop_f();

		// send disconnect message 3 times to improve chances of server
		// receiving it (but it still fails sometimes)
		memset(&buf, 0, sizeof(buf));
		buf.data = bufdata;
		buf.maxsize = sizeof(bufdata);
		if (cls.protocol == PROTOCOL_QUAKEWORLD)
		{
			Con_DPrint("Sending drop command\n");
			MSG_WriteByte(&buf, qw_clc_stringcmd);
			MSG_WriteString(&buf, "drop");
		}
		else
		{
			Con_DPrint("Sending clc_disconnect\n");
			MSG_WriteByte(&buf, clc_disconnect);
		}
		NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol);
		NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol);
		NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol);
		NetConn_Close(cls.netcon);
		cls.netcon = NULL;
	}
	cls.state = ca_disconnected;

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}

void CL_Disconnect_f(void)
{
	CL_Disconnect ();
	if (sv.active)
		Host_ShutdownServer ();
}




/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address
=====================
*/
void CL_EstablishConnection(const char *host)
{
	if (cls.state == ca_dedicated)
		return;

	// clear menu's connect error message
	M_Update_Return_Reason("");
	cls.demonum = -1;

	// stop demo loop in case this fails
	CL_Disconnect();

	// make sure the client ports are open before attempting to connect
	NetConn_UpdateSockets();

	// run a network frame
	//NetConn_ClientFrame();SV_VM_Begin();NetConn_ServerFrame();SV_VM_End();

	if (LHNETADDRESS_FromString(&cls.connect_address, host, 26000) && (cls.connect_mysocket = NetConn_ChooseClientSocketForAddress(&cls.connect_address)))
	{
		cls.connect_trying = true;
		cls.connect_remainingtries = 3;
		cls.connect_nextsendtime = 0;
		M_Update_Return_Reason("Trying to connect...");
		// run several network frames to jump into the game quickly
		//if (sv.active)
		//{
		//	NetConn_ClientFrame();SV_VM_Begin();NetConn_ServerFrame();SV_VM_End();
		//	NetConn_ClientFrame();SV_VM_Begin();NetConn_ServerFrame();SV_VM_End();
		//	NetConn_ClientFrame();SV_VM_Begin();NetConn_ServerFrame();SV_VM_End();
		//	NetConn_ClientFrame();SV_VM_Begin();NetConn_ServerFrame();SV_VM_End();
		//}
	}
	else
	{
		Con_Print("Unable to find a suitable network socket to connect to server.\n");
		M_Update_Return_Reason("No network");
	}
}

/*
==============
CL_PrintEntities_f
==============
*/
static void CL_PrintEntities_f(void)
{
	entity_t *ent;
	int i, j;
	char name[32];

	for (i = 0, ent = cl_entities;i < cl_num_entities;i++, ent++)
	{
		if (!ent->state_current.active)
			continue;

		if (ent->render.model)
			strlcpy (name, ent->render.model->name, 25);
		else
			strcpy(name, "--no model--");
		for (j = (int)strlen(name);j < 25;j++)
			name[j] = ' ';
		Con_Printf("%3i: %s:%4i (%5i %5i %5i) [%3i %3i %3i] %4.2f %5.3f\n", i, name, ent->render.frame, (int) ent->render.matrix.m[0][3], (int) ent->render.matrix.m[1][3], (int) ent->render.matrix.m[2][3], (int) ent->render.angles[0] % 360, (int) ent->render.angles[1] % 360, (int) ent->render.angles[2] % 360, ent->render.scale, ent->render.alpha);
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

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float CL_LerpPoint(void)
{
	float f;

	// dropped packet, or start of demo
	if (cl.mtime[1] < cl.mtime[0] - 0.1)
		cl.mtime[1] = cl.mtime[0] - 0.1;

	cl.time = bound(cl.mtime[1], cl.time, cl.mtime[0]);

	// LordHavoc: lerp in listen games as the server is being capped below the client (usually)
	f = cl.mtime[0] - cl.mtime[1];
	if (!f || cl_nolerp.integer || cls.timedemo || (cl.islocalgame && !sv_fixedframeratesingleplayer.integer))
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

entity_t *CL_NewTempEntity(void)
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
	VectorSet(ent->render.colormod, 1, 1, 1);
	return ent;
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
		cl_activeeffects = max(cl_activeeffects, i + 1);
		break;
	}
}

void CL_AllocDlight(entity_render_t *ent, matrix4x4_t *matrix, float radius, float red, float green, float blue, float decay, float lifetime, int cubemapnum, int style, int shadowenable, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags)
{
	int i;
	dlight_t *dl;

	/*
// first look for an exact key match
	if (ent)
	{
		dl = cl_dlights;
		for (i = 0;i < cl_activedlights;i++, dl++)
			if (dl->ent == ent)
				goto dlightsetup;
	}
	*/

// then look for anything else
	dl = cl_dlights;
	for (i = 0;i < cl_activedlights;i++, dl++)
		if (!dl->radius)
			goto dlightsetup;
	// if we hit the end of the active dlights and found no gaps, add a new one
	if (i < MAX_DLIGHTS)
	{
		cl_activedlights = i + 1;
		goto dlightsetup;
	}

	// unable to find one
	return;

dlightsetup:
	//Con_Printf("dlight %i : %f %f %f : %f %f %f\n", i, org[0], org[1], org[2], red * radius, green * radius, blue * radius);
	memset (dl, 0, sizeof(*dl));
	Matrix4x4_Normalize(&dl->matrix, matrix);
	dl->ent = ent;
	dl->origin[0] = dl->matrix.m[0][3];
	dl->origin[1] = dl->matrix.m[1][3];
	dl->origin[2] = dl->matrix.m[2][3];
	CL_FindNonSolidLocation(dl->origin, dl->origin, 6);
	dl->matrix.m[0][3] = dl->origin[0];
	dl->matrix.m[1][3] = dl->origin[1];
	dl->matrix.m[2][3] = dl->origin[2];
	dl->radius = radius;
	dl->color[0] = red;
	dl->color[1] = green;
	dl->color[2] = blue;
	dl->decay = decay;
	if (lifetime)
		dl->die = cl.time + lifetime;
	else
		dl->die = 0;
	dl->cubemapnum = cubemapnum;
	dl->style = style;
	dl->shadow = shadowenable;
	dl->corona = corona;
	dl->flags = flags;
	dl->coronasizescale = coronasizescale;
	dl->ambientscale = ambientscale;
	dl->diffusescale = diffusescale;
	dl->specularscale = specularscale;
}

// called before entity relinking
void CL_DecayLights(void)
{
	int i, oldmax;
	dlight_t *dl;
	float time, f;

	time = cl.time - cl.oldtime;
	oldmax = cl_activedlights;
	cl_activedlights = 0;
	for (i = 0, dl = cl_dlights;i < oldmax;i++, dl++)
	{
		if (dl->radius)
		{
			f = dl->radius - time * dl->decay;
			if (cl.time < dl->die && f > 0)
			{
				dl->radius = dl->radius - time * dl->decay;
				cl_activedlights = i + 1;
			}
			else
				dl->radius = 0;
		}
	}
}

// called after entity relinking
void CL_UpdateLights(void)
{
	int i, j, k, l;
	dlight_t *dl;
	float frac, f;

	r_refdef.numlights = 0;
	if (r_dynamic.integer)
	{
		for (i = 0, dl = cl_dlights;i < cl_activedlights;i++, dl++)
		{
			if (dl->radius)
			{
				R_RTLight_Update(dl, false);
				r_refdef.lights[r_refdef.numlights++] = dl;
			}
		}
	}

// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	f = cl.time * 10;
	i = (int)floor(f);
	frac = f - i;
	for (j = 0;j < cl_max_lightstyle;j++)
	{
		if (!cl_lightstyle || !cl_lightstyle[j].length)
		{
			r_refdef.lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl_lightstyle[j].length;
		l = (i-1) % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		l = cl_lightstyle[j].map[l] - 'a';
		r_refdef.lightstylevalue[j] = ((k*frac)+(l*(1-frac)))*22;
	}
}

void CL_AddQWCTFFlagModel(entity_t *player, int skin)
{
	float f;
	entity_t *flag;
	matrix4x4_t flagmatrix;

	// this code taken from QuakeWorld
	f = 14;
	if (player->render.frame2 >= 29 && player->render.frame2 <= 40)
	{
		if (player->render.frame2 >= 29 && player->render.frame2 <= 34)
		{ //axpain
			if      (player->render.frame2 == 29) f = f + 2;
			else if (player->render.frame2 == 30) f = f + 8;
			else if (player->render.frame2 == 31) f = f + 12;
			else if (player->render.frame2 == 32) f = f + 11;
			else if (player->render.frame2 == 33) f = f + 10;
			else if (player->render.frame2 == 34) f = f + 4;
		}
		else if (player->render.frame2 >= 35 && player->render.frame2 <= 40)
		{ // pain
			if      (player->render.frame2 == 35) f = f + 2;
			else if (player->render.frame2 == 36) f = f + 10;
			else if (player->render.frame2 == 37) f = f + 10;
			else if (player->render.frame2 == 38) f = f + 8;
			else if (player->render.frame2 == 39) f = f + 4;
			else if (player->render.frame2 == 40) f = f + 2;
		}
	}
	else if (player->render.frame2 >= 103 && player->render.frame2 <= 118)
	{
		if      (player->render.frame2 >= 103 && player->render.frame2 <= 104) f = f + 6;  //nailattack
		else if (player->render.frame2 >= 105 && player->render.frame2 <= 106) f = f + 6;  //light
		else if (player->render.frame2 >= 107 && player->render.frame2 <= 112) f = f + 7;  //rocketattack
		else if (player->render.frame2 >= 112 && player->render.frame2 <= 118) f = f + 7;  //shotattack
	}
	// end of code taken from QuakeWorld

	flag = CL_NewTempEntity();
	if (!flag)
		return;

	flag->render.model = cl.model_precache[cl.qw_modelindex_flag];
	flag->render.skinnum = skin;
	flag->render.colormap = -1; // no special coloring
	flag->render.alpha = 1;
	VectorSet(flag->render.colormod, 1, 1, 1);
	// attach the flag to the player matrix
	Matrix4x4_CreateFromQuakeEntity(&flagmatrix, -f, -22, 0, 0, 0, -45, 1);
	Matrix4x4_Concat(&flag->render.matrix, &player->render.matrix, &flagmatrix);
	Matrix4x4_Invert_Simple(&flag->render.inversematrix, &flag->render.matrix);
	R_LerpAnimation(&flag->render);
	CL_BoundingBoxForEntity(&flag->render);
}

#define MAXVIEWMODELS 32
entity_t *viewmodels[MAXVIEWMODELS];
int numviewmodels;

matrix4x4_t viewmodelmatrix;

static int entitylinkframenumber;

static const vec3_t muzzleflashorigin = {18, 0, 0};

extern void V_DriftPitch(void);
extern void V_FadeViewFlashs(void);
extern void V_CalcViewBlend(void);

extern void V_CalcRefdef(void);
// note this is a recursive function, but it can never get in a runaway loop (because of the delayedlink flags)
void CL_LinkNetworkEntity(entity_t *e)
{
	matrix4x4_t *matrix, blendmatrix, tempmatrix, matrix2;
	//matrix4x4_t dlightmatrix;
	int j, k, l, trailtype, temp;
	float origin[3], angles[3], delta[3], lerp, dlightcolor[3], dlightradius, mins[3], maxs[3], v2[3], d;
	entity_t *t;
	model_t *model;
	trace_t trace;
	//entity_persistent_t *p = &e->persistent;
	//entity_render_t *r = &e->render;
	if (e->persistent.linkframe != entitylinkframenumber)
	{
		e->persistent.linkframe = entitylinkframenumber;
		// skip inactive entities and world
		if (!e->state_current.active || e == cl_entities || e == cl_csqcentities)
			return;
		e->render.alpha = e->state_current.alpha * (1.0f / 255.0f); // FIXME: interpolate?
		e->render.scale = e->state_current.scale * (1.0f / 16.0f); // FIXME: interpolate?
		e->render.flags = e->state_current.flags;
		e->render.effects = e->state_current.effects;
		VectorScale(e->state_current.colormod, (1.0f / 32.0f), e->render.colormod);
		if (e->state_current.flags & RENDER_COLORMAPPED)
		{
			int cb;
			unsigned char *cbcolor;
			e->render.colormap = e->state_current.colormap;
			cb = (e->render.colormap & 0xF) << 4;cb += (cb >= 128 && cb < 224) ? 4 : 12;
			cbcolor = (unsigned char *) (&palette_complete[cb]);
			e->render.colormap_pantscolor[0] = cbcolor[0] * (1.0f / 255.0f);
			e->render.colormap_pantscolor[1] = cbcolor[1] * (1.0f / 255.0f);
			e->render.colormap_pantscolor[2] = cbcolor[2] * (1.0f / 255.0f);
			cb = (e->render.colormap & 0xF0);cb += (cb >= 128 && cb < 224) ? 4 : 12;
			cbcolor = (unsigned char *) (&palette_complete[cb]);
			e->render.colormap_shirtcolor[0] = cbcolor[0] * (1.0f / 255.0f);
			e->render.colormap_shirtcolor[1] = cbcolor[1] * (1.0f / 255.0f);
			e->render.colormap_shirtcolor[2] = cbcolor[2] * (1.0f / 255.0f);
		}
		else if (e->state_current.colormap && cl.scores != NULL)
		{
			int cb;
			unsigned char *cbcolor;
			e->render.colormap = cl.scores[e->state_current.colormap - 1].colors; // color it
			cb = (e->render.colormap & 0xF) << 4;cb += (cb >= 128 && cb < 224) ? 4 : 12;
			cbcolor = (unsigned char *) (&palette_complete[cb]);
			e->render.colormap_pantscolor[0] = cbcolor[0] * (1.0f / 255.0f);
			e->render.colormap_pantscolor[1] = cbcolor[1] * (1.0f / 255.0f);
			e->render.colormap_pantscolor[2] = cbcolor[2] * (1.0f / 255.0f);
			cb = (e->render.colormap & 0xF0);cb += (cb >= 128 && cb < 224) ? 4 : 12;
			cbcolor = (unsigned char *) (&palette_complete[cb]);
			e->render.colormap_shirtcolor[0] = cbcolor[0] * (1.0f / 255.0f);
			e->render.colormap_shirtcolor[1] = cbcolor[1] * (1.0f / 255.0f);
			e->render.colormap_shirtcolor[2] = cbcolor[2] * (1.0f / 255.0f);
		}
		else
		{
			e->render.colormap = -1; // no special coloring
			VectorClear(e->render.colormap_pantscolor);
			VectorClear(e->render.colormap_shirtcolor);
		}
		e->render.skinnum = e->state_current.skin;
		if (e->render.flags & RENDER_VIEWMODEL && !e->state_current.tagentity)
		{
			if (!r_drawviewmodel.integer || chase_active.integer || envmap)// || csqc_loaded)
				return;
			if (!e->csqc)
			{
				if (cl.viewentity)
					CL_LinkNetworkEntity(cl_entities + cl.viewentity);
				if (e == &cl.viewent && cl_entities[cl.viewentity].state_current.active)
				{
					e->state_current.alpha = cl_entities[cl.viewentity].state_current.alpha;
					e->state_current.effects = EF_NOSHADOW | (cl_entities[cl.viewentity].state_current.effects & (EF_ADDITIVE | EF_REFLECTIVE | EF_FULLBRIGHT | EF_NODEPTHTEST));
				}
			}
			matrix = &viewmodelmatrix;
		}
		else
		{
			// if the tag entity is currently impossible, skip it
			if (!e->csqc)
			{
				if (e->state_current.tagentity >= cl_num_entities)
					return;
				t = cl_entities + e->state_current.tagentity;
			}
			else
			{
				if (e->state_current.tagentity >= cl_num_csqcentities)
					return;
				t = cl_csqcentities + e->state_current.tagentity;
			}
			// if the tag entity is inactive, skip it
			if (!t->state_current.active)
				return;
			// note: this can link to world
			CL_LinkNetworkEntity(t);
			// make relative to the entity
			matrix = &t->render.matrix;
			// some properties of the tag entity carry over
			e->render.flags |= t->render.flags & (RENDER_EXTERIORMODEL | RENDER_VIEWMODEL);
			// if a valid tagindex is used, make it relative to that tag instead
			// FIXME: use a model function to get tag info (need to handle skeletal)
			if (e->state_current.tagentity && e->state_current.tagindex >= 1 && (model = t->render.model))
			{
				// blend the matrices
				memset(&blendmatrix, 0, sizeof(blendmatrix));
				for (j = 0;j < 4 && t->render.frameblend[j].lerp > 0;j++)
				{
					matrix4x4_t tagmatrix;
					Mod_Alias_GetTagMatrix(model, t->render.frameblend[j].frame, e->state_current.tagindex - 1, &tagmatrix);
					d = t->render.frameblend[j].lerp;
					for (l = 0;l < 4;l++)
						for (k = 0;k < 4;k++)
							blendmatrix.m[l][k] += d * tagmatrix.m[l][k];
				}
				// concat the tag matrices onto the entity matrix
				Matrix4x4_Concat(&tempmatrix, &t->render.matrix, &blendmatrix);
				// use the constructed tag matrix
				matrix = &tempmatrix;
			}
		}

		// movement lerp
		// if it's the player entity, update according to client movement
		if (e == cl_entities + cl.playerentity && cl.movement)// && !e->csqc)
		{
			lerp = (cl.time - cl.mtime[1]) / (cl.mtime[0] - cl.mtime[1]);
			lerp = bound(0, lerp, 1);
			VectorLerp(cl.movement_oldorigin, lerp, cl.movement_origin, origin);
			VectorSet(angles, 0, cl.viewangles[1], 0);
		}
		else if (e->persistent.lerpdeltatime > 0 && (lerp = (cl.time - e->persistent.lerpstarttime) / e->persistent.lerpdeltatime) < 1)
		{
			// interpolate the origin and angles
			VectorLerp(e->persistent.oldorigin, lerp, e->persistent.neworigin, origin);
			VectorSubtract(e->persistent.newangles, e->persistent.oldangles, delta);
			if (delta[0] < -180) delta[0] += 360;else if (delta[0] >= 180) delta[0] -= 360;
			if (delta[1] < -180) delta[1] += 360;else if (delta[1] >= 180) delta[1] -= 360;
			if (delta[2] < -180) delta[2] += 360;else if (delta[2] >= 180) delta[2] -= 360;
			VectorMA(e->persistent.oldangles, lerp, delta, angles);
		}
		else
		{
			// no interpolation
			VectorCopy(e->persistent.neworigin, origin);
			VectorCopy(e->persistent.newangles, angles);
		}

		// model setup and some modelflags
		if(e->state_current.modelindex < MAX_MODELS)
			e->render.model = cl.model_precache[e->state_current.modelindex];
		else
			e->render.model = cl.csqc_model_precache[65536-e->state_current.modelindex];
		if (e->render.model)
		{
			// if model is alias or this is a tenebrae-like dlight, reverse pitch direction
			if (e->render.model->type == mod_alias || (e->state_current.lightpflags & PFLAGS_FULLDYNAMIC))
				angles[0] = -angles[0];
			if ((e->render.model->flags & EF_ROTATE) && (!e->state_current.tagentity && !(e->render.flags & RENDER_VIEWMODEL)))
			{
				angles[1] = ANGLEMOD(100*cl.time);
				if (cl_itembobheight.value)
					origin[2] += (cos(cl.time * cl_itembobspeed.value * (2.0 * M_PI)) + 1.0) * 0.5 * cl_itembobheight.value;
			}
			// transfer certain model flags to effects
			e->render.effects |= e->render.model->flags2 & (EF_FULLBRIGHT | EF_ADDITIVE);
			if (cl_prydoncursor.integer && (e->render.effects & EF_SELECTABLE) && cl.cmd.cursor_entitynumber == e->state_current.number)
				VectorScale(e->render.colormod, 2, e->render.colormod);
		}

		// animation lerp
		if (e->render.frame2 == e->state_current.frame)
		{
			// update frame lerp fraction
			e->render.framelerp = 1;
			if (e->render.frame2time > e->render.frame1time)
			{
				// make sure frame lerp won't last longer than 100ms
				// (this mainly helps with models that use framegroups and
				// switch between them infrequently)
				e->render.framelerp = (cl.time - e->render.frame2time) / min(e->render.frame2time - e->render.frame1time, 0.1);
				e->render.framelerp = bound(0, e->render.framelerp, 1);
			}
		}
		else
		{
			// begin a new frame lerp
			e->render.frame1 = e->render.frame2;
			e->render.frame1time = e->render.frame2time;
			e->render.frame = e->render.frame2 = e->state_current.frame;
			e->render.frame2time = cl.time;
			e->render.framelerp = 0;
		}
		R_LerpAnimation(&e->render);

		// set up the render matrix
		// FIXME: e->render.scale should go away
		Matrix4x4_CreateFromQuakeEntity(&matrix2, origin[0], origin[1], origin[2], angles[0], angles[1], angles[2], e->render.scale);
		// concat the matrices to make the entity relative to its tag
		Matrix4x4_Concat(&e->render.matrix, matrix, &matrix2);
		// make the other useful stuff
		Matrix4x4_Invert_Simple(&e->render.inversematrix, &e->render.matrix);
		CL_BoundingBoxForEntity(&e->render);

		// handle effects now that we know where this entity is in the world...
		if (e->render.model && e->render.model->soundfromcenter)
		{
			// bmodels are treated specially since their origin is usually '0 0 0'
			vec3_t o;
			VectorMAM(0.5f, e->render.model->normalmins, 0.5f, e->render.model->normalmaxs, o);
			Matrix4x4_Transform(&e->render.matrix, o, origin);
		}
		else
		{
			origin[0] = e->render.matrix.m[0][3];
			origin[1] = e->render.matrix.m[1][3];
			origin[2] = e->render.matrix.m[2][3];
		}
		trailtype = -1;
		dlightradius = 0;
		dlightcolor[0] = 0;
		dlightcolor[1] = 0;
		dlightcolor[2] = 0;
		// LordHavoc: if the entity has no effects, don't check each
		if (e->render.effects)
		{
			if (e->render.effects & EF_BRIGHTFIELD)
			{
				if (gamemode == GAME_NEXUIZ)
				{
					dlightradius = max(dlightradius, 200);
					dlightcolor[0] += 0.75f;
					dlightcolor[1] += 1.50f;
					dlightcolor[2] += 3.00f;
					trailtype = 8;
				}
				else
					CL_EntityParticles(e);
			}
			if (e->render.effects & EF_MUZZLEFLASH)
				e->persistent.muzzleflash = 1.0f;
			if (e->render.effects & EF_DIMLIGHT)
			{
				dlightradius = max(dlightradius, 200);
				dlightcolor[0] += 1.50f;
				dlightcolor[1] += 1.50f;
				dlightcolor[2] += 1.50f;
			}
			if (e->render.effects & EF_BRIGHTLIGHT)
			{
				dlightradius = max(dlightradius, 400);
				dlightcolor[0] += 3.00f;
				dlightcolor[1] += 3.00f;
				dlightcolor[2] += 3.00f;
			}
			// LordHavoc: more effects
			if (e->render.effects & EF_RED) // red
			{
				dlightradius = max(dlightradius, 200);
				dlightcolor[0] += 1.50f;
				dlightcolor[1] += 0.15f;
				dlightcolor[2] += 0.15f;
			}
			if (e->render.effects & EF_BLUE) // blue
			{
				dlightradius = max(dlightradius, 200);
				dlightcolor[0] += 0.15f;
				dlightcolor[1] += 0.15f;
				dlightcolor[2] += 1.50f;
			}
			if (e->render.effects & EF_FLAME)
			{
				mins[0] = origin[0] - 16.0f;
				mins[1] = origin[1] - 16.0f;
				mins[2] = origin[2] - 16.0f;
				maxs[0] = origin[0] + 16.0f;
				maxs[1] = origin[1] + 16.0f;
				maxs[2] = origin[2] + 16.0f;
				// how many flames to make
				temp = (int) (cl.time * 300) - (int) (cl.oldtime * 300);
				CL_FlameCube(mins, maxs, temp);
				d = lhrandom(0.75f, 1);
				dlightradius = max(dlightradius, 200);
				dlightcolor[0] += d * 2.0f;
				dlightcolor[1] += d * 1.5f;
				dlightcolor[2] += d * 0.5f;
			}
			if (e->render.effects & EF_STARDUST)
			{
				mins[0] = origin[0] - 16.0f;
				mins[1] = origin[1] - 16.0f;
				mins[2] = origin[2] - 16.0f;
				maxs[0] = origin[0] + 16.0f;
				maxs[1] = origin[1] + 16.0f;
				maxs[2] = origin[2] + 16.0f;
				// how many particles to make
				temp = (int) (cl.time * 200) - (int) (cl.oldtime * 200);
				CL_Stardust(mins, maxs, temp);
				dlightradius = max(dlightradius, 200);
				dlightcolor[0] += 1.0f;
				dlightcolor[1] += 0.7f;
				dlightcolor[2] += 0.3f;
			}
			if (e->render.effects & (EF_FLAG1QW | EF_FLAG2QW))
			{
				// these are only set on player entities
				CL_AddQWCTFFlagModel(e, (e->render.effects & EF_FLAG2QW) != 0);
			}
		}
		// muzzleflash fades over time, and is offset a bit
		if (e->persistent.muzzleflash > 0)
		{
			Matrix4x4_Transform(&e->render.matrix, muzzleflashorigin, v2);
			trace = CL_TraceBox(origin, vec3_origin, vec3_origin, v2, true, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_SKY, false);
			tempmatrix = e->render.matrix;
			tempmatrix.m[0][3] = trace.endpos[0];
			tempmatrix.m[1][3] = trace.endpos[1];
			tempmatrix.m[2][3] = trace.endpos[2];
			CL_AllocDlight(NULL, &tempmatrix, 100, e->persistent.muzzleflash, e->persistent.muzzleflash, e->persistent.muzzleflash, 0, 0, 0, -1, true, 0, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
			e->persistent.muzzleflash -= cl.frametime * 10;
		}
		// LordHavoc: if the model has no flags, don't check each
		if (e->render.model && e->render.model->flags && (!e->state_current.tagentity && !(e->render.flags & RENDER_VIEWMODEL)))
		{
			if (e->render.model->flags & EF_GIB)
				trailtype = 2;
			else if (e->render.model->flags & EF_ZOMGIB)
				trailtype = 4;
			else if (e->render.model->flags & EF_TRACER)
			{
				trailtype = 3;
				//dlightradius = max(dlightradius, 100);
				//dlightcolor[0] += 0.25f;
				//dlightcolor[1] += 1.00f;
				//dlightcolor[2] += 0.25f;
			}
			else if (e->render.model->flags & EF_TRACER2)
			{
				trailtype = 5;
				//dlightradius = max(dlightradius, 100);
				//dlightcolor[0] += 1.00f;
				//dlightcolor[1] += 0.60f;
				//dlightcolor[2] += 0.20f;
			}
			else if (e->render.model->flags & EF_ROCKET)
			{
				trailtype = 0;
				dlightradius = max(dlightradius, 200);
				dlightcolor[0] += 3.00f;
				dlightcolor[1] += 1.50f;
				dlightcolor[2] += 0.50f;
			}
			else if (e->render.model->flags & EF_GRENADE)
			{
				// LordHavoc: e->render.alpha == -1 is for Nehahra dem compatibility (cigar smoke)
				trailtype = e->render.alpha == -1 ? 7 : 1;
			}
			else if (e->render.model->flags & EF_TRACER3)
			{
				trailtype = 6;
				if (gamemode == GAME_PRYDON)
				{
					dlightradius = max(dlightradius, 100);
					dlightcolor[0] += 0.30f;
					dlightcolor[1] += 0.60f;
					dlightcolor[2] += 1.20f;
				}
				else
				{
					dlightradius = max(dlightradius, 200);
					dlightcolor[0] += 1.20f;
					dlightcolor[1] += 0.50f;
					dlightcolor[2] += 1.00f;
				}
			}
		}
		// LordHavoc: customizable glow
		if (e->state_current.glowsize)
		{
			// * 4 for the expansion from 0-255 to 0-1023 range,
			// / 255 to scale down byte colors
			dlightradius = max(dlightradius, e->state_current.glowsize * 4);
			VectorMA(dlightcolor, (1.0f / 255.0f), (unsigned char *)&palette_complete[e->state_current.glowcolor], dlightcolor);
		}
		// make the glow dlight
		if (dlightradius > 0 && (dlightcolor[0] || dlightcolor[1] || dlightcolor[2]) && !(e->render.flags & RENDER_VIEWMODEL))
		{
			//dlightmatrix = e->render.matrix;
			// hack to make glowing player light shine on their gun
			//if (e->state_current.number == cl.viewentity/* && !chase_active.integer*/)
			//	dlightmatrix.m[2][3] += 30;
			CL_AllocDlight(&e->render, &e->render.matrix, dlightradius, dlightcolor[0], dlightcolor[1], dlightcolor[2], 0, 0, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		}
		// custom rtlight
		if (e->state_current.lightpflags & PFLAGS_FULLDYNAMIC)
		{
			float light[4];
			VectorScale(e->state_current.light, (1.0f / 256.0f), light);
			light[3] = e->state_current.light[3];
			if (light[0] == 0 && light[1] == 0 && light[2] == 0)
				VectorSet(light, 1, 1, 1);
			if (light[3] == 0)
				light[3] = 350;
			// FIXME: add ambient/diffuse/specular scales as an extension ontop of TENEBRAE_GFX_DLIGHTS?
			CL_AllocDlight(&e->render, &e->render.matrix, light[3], light[0], light[1], light[2], 0, 0, e->state_current.skin, e->state_current.lightstyle, !(e->state_current.lightpflags & PFLAGS_NOSHADOW), (e->state_current.lightpflags & PFLAGS_CORONA) != 0, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
		}
		// do trails
		if (e->render.flags & RENDER_GLOWTRAIL)
			trailtype = 9;
		if (trailtype >= 0)
			CL_RocketTrail(e->persistent.trail_origin, origin, trailtype, e->state_current.glowcolor, e);
		VectorCopy(origin, e->persistent.trail_origin);
		// tenebrae's sprites are all additive mode (weird)
		if (gamemode == GAME_TENEBRAE && e->render.model && e->render.model->type == mod_sprite)
			e->render.effects |= EF_ADDITIVE;
		// player model is only shown with chase_active on
		if (!e->csqc)
		if (e->state_current.number == cl.viewentity)
			e->render.flags |= RENDER_EXTERIORMODEL;
		// transparent stuff can't be lit during the opaque stage
		if (e->render.effects & (EF_ADDITIVE | EF_NODEPTHTEST) || e->render.alpha < 1)
			e->render.flags |= RENDER_TRANSPARENT;
		// double sided rendering mode causes backfaces to be visible
		// (mostly useful on transparent stuff)
		if (e->render.effects & EF_DOUBLESIDED)
			e->render.flags |= RENDER_NOCULLFACE;
		// either fullbright or lit
		if (!(e->render.effects & EF_FULLBRIGHT) && !r_fullbright.integer)
			e->render.flags |= RENDER_LIGHT;
		// hide player shadow during intermission or nehahra movie
		if (!(e->render.effects & EF_NOSHADOW)
		 && !(e->render.flags & (RENDER_VIEWMODEL | RENDER_TRANSPARENT))
		 && (!(e->render.flags & RENDER_EXTERIORMODEL) || (!cl.intermission && cls.protocol != PROTOCOL_NEHAHRAMOVIE && !cl_noplayershadow.integer)))
			e->render.flags |= RENDER_SHADOW;
		// as soon as player is known we can call V_CalcRefDef
		if (!csqc_loaded)
		if (e->state_current.number == cl.viewentity)
			V_CalcRefdef();
		if (e->render.model && e->render.model->name[0] == '*' && e->render.model->TraceBox)
			cl_brushmodel_entities[cl_num_brushmodel_entities++] = e->state_current.number;
		// don't show entities with no modelindex (note: this still shows
		// entities which have a modelindex that resolved to a NULL model)
		if (e->render.model && !(e->render.effects & EF_NODRAW) && r_refdef.numentities < r_refdef.maxentities)
			r_refdef.entities[r_refdef.numentities++] = &e->render;
		//if (cl.viewentity && e->state_current.number == cl.viewentity)
		//	Matrix4x4_Print(&e->render.matrix);
	}
}

void CL_RelinkWorld(void)
{
	entity_t *ent = &cl_entities[0];
	cl_brushmodel_entities[cl_num_brushmodel_entities++] = 0;
	// FIXME: this should be done at load
	ent->render.matrix = identitymatrix;
	ent->render.inversematrix = identitymatrix;
	R_LerpAnimation(&ent->render);
	CL_BoundingBoxForEntity(&ent->render);
	ent->render.flags = RENDER_SHADOW;
	if (!r_fullbright.integer)
		ent->render.flags |= RENDER_LIGHT;
	VectorSet(ent->render.colormod, 1, 1, 1);
	r_refdef.worldentity = &ent->render;
	r_refdef.worldmodel = cl.worldmodel;
}

void CL_RelinkCSQCWorld(void)	//[515]: csqc
{
	entity_t *ent = &cl_csqcentities[0];
	if(!csqc_loaded)
		return;
//	cl_brushmodel_entities[cl_num_brushmodel_entities++] = 0;
	// FIXME: this should be done at load
	ent->render.matrix = identitymatrix;
	ent->render.inversematrix = identitymatrix;
	R_LerpAnimation(&ent->render);
	CL_BoundingBoxForEntity(&ent->render);
	ent->render.flags = RENDER_SHADOW;
	if (!r_fullbright.integer)
		ent->render.flags |= RENDER_LIGHT;
	VectorSet(ent->render.colormod, 1, 1, 1);
//	r_refdef.worldentity = &ent->render;
//	r_refdef.worldmodel = cl.worldmodel;
}

static void CL_RelinkStaticEntities(void)
{
	int i;
	entity_t *e;
	for (i = 0, e = cl_static_entities;i < cl_num_static_entities && r_refdef.numentities < r_refdef.maxentities;i++, e++)
	{
		e->render.flags = 0;
		// transparent stuff can't be lit during the opaque stage
		if (e->render.effects & (EF_ADDITIVE | EF_NODEPTHTEST) || e->render.alpha < 1)
			e->render.flags |= RENDER_TRANSPARENT;
		// either fullbright or lit
		if (!(e->render.effects & EF_FULLBRIGHT) && !r_fullbright.integer)
			e->render.flags |= RENDER_LIGHT;
		// hide player shadow during intermission or nehahra movie
		if (!(e->render.effects & EF_NOSHADOW) && !(e->render.flags & RENDER_TRANSPARENT))
			e->render.flags |= RENDER_SHADOW;
		VectorSet(e->render.colormod, 1, 1, 1);
		R_LerpAnimation(&e->render);
		r_refdef.entities[r_refdef.numentities++] = &e->render;
	}
}

/*
===============
CL_RelinkEntities
===============
*/
static void CL_RelinkNetworkEntities(int drawmask)
{
	entity_t *ent;
	int i, k;

	if(!csqc_loaded)
	{
		ent = &cl.viewent;
		ent->state_previous = ent->state_current;
		ent->state_current = defaultstate;
		ent->state_current.time = cl.time;
		ent->state_current.number = -1;
		ent->state_current.active = true;
		ent->state_current.modelindex = cl.stats[STAT_WEAPON];
		ent->state_current.frame = cl.stats[STAT_WEAPONFRAME];
		ent->state_current.flags = RENDER_VIEWMODEL;
		if ((cl.stats[STAT_HEALTH] <= 0 && cl_deathnoviewmodel.integer) || cl.intermission)
			ent->state_current.modelindex = 0;
		else if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
		{
			if (gamemode == GAME_TRANSFUSION)
				ent->state_current.alpha = 128;
			else
				ent->state_current.modelindex = 0;
		}

		// reset animation interpolation on weaponmodel if model changed
		if (ent->state_previous.modelindex != ent->state_current.modelindex)
		{
			ent->render.frame = ent->render.frame1 = ent->render.frame2 = ent->state_current.frame;
			ent->render.frame1time = ent->render.frame2time = cl.time;
			ent->render.framelerp = 1;
		}
	}

	// start on the entity after the world
	entitylinkframenumber++;
	if(drawmask & ENTMASK_ENGINE || !csqc_loaded)
	{
		for (i = 1;i < cl_num_entities;i++)
		{
			if (cl_entities_active[i])
			{
				ent = cl_entities + i;
				if (!(drawmask & ENTMASK_ENGINEVIEWMODELS))
				if (ent->state_current.flags & RENDER_VIEWMODEL)	//[515]: csqc drawmask
				{
					cl_entities_active[i] = false;
					continue;
				}
				if (ent->state_current.active)
					CL_LinkNetworkEntity(ent);
				else
					cl_entities_active[i] = false;
			}
		}
	}

	//[515]: csqc
	if(csqc_loaded)
	{
		for (i=1,k=cl_num_csqcentities;k;i++)
		{
			if (cl_csqcentities_active[i])
			{
				--k;
				ent = cl_csqcentities + i;
				if (ent->state_current.active)
					CL_LinkNetworkEntity(ent);
				else
					cl_csqcentities_active[i] = false;
			}
		}
	}
	else
		CL_LinkNetworkEntity(&cl.viewent);
}

static void CL_RelinkEffects(void)
{
	int i, intframe;
	cl_effect_t *e;
	entity_t *ent;
	float frame;

	for (i = 0, e = cl_effects;i < cl_activeeffects;i++, e++)
	{
		if (e->active)
		{
			frame = (cl.time - e->starttime) * e->framerate + e->startframe;
			intframe = frame;
			if (intframe < 0 || intframe >= e->endframe)
			{
				memset(e, 0, sizeof(*e));
				while (cl_activeeffects > 0 && !cl_effects[cl_activeeffects - 1].active)
					cl_activeeffects--;
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
				if(e->modelindex < MAX_MODELS)
					ent->render.model = cl.model_precache[e->modelindex];
				else
					ent->render.model = cl.csqc_model_precache[65536-e->modelindex];
				ent->render.frame = ent->render.frame2;
				ent->render.colormap = -1; // no special coloring
				ent->render.alpha = 1;
				VectorSet(ent->render.colormod, 1, 1, 1);

				Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, e->origin[0], e->origin[1], e->origin[2], 0, 0, 0, 1);
				Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
				R_LerpAnimation(&ent->render);
				CL_BoundingBoxForEntity(&ent->render);
			}
		}
	}
}

void CL_RelinkBeams(void)
{
	int i;
	beam_t *b;
	vec3_t dist, org;
	float d;
	entity_t *ent;
	float yaw, pitch;
	float forward;
	matrix4x4_t tempmatrix;

	for (i = 0, b = cl_beams;i < cl_activebeams;i++, b++)
	{
		if (!b->model)
			continue;
		if (b->endtime < cl.time)
		{
			b->model = NULL;
			continue;
		}

		// if coming from the player, update the start position
		//if (b->entity == cl.viewentity)
		//	Matrix4x4_OriginFromMatrix(&cl_entities[cl.viewentity].render.matrix, b->start);
		if (cl_beams_relative.integer && b->entity && cl_entities[b->entity].state_current.active && b->relativestartvalid)
		{
			entity_render_t *r = &cl_entities[b->entity].render;
			//Matrix4x4_OriginFromMatrix(&r->matrix, origin);
			//Matrix4x4_CreateFromQuakeEntity(&matrix, r->origin[0], r->origin[1], r->origin[2] + 16, r->angles[0], r->angles[1], r->angles[2], 1);
			Matrix4x4_Transform(&r->matrix, b->relativestart, b->start);
			Matrix4x4_Transform(&r->matrix, b->relativeend, b->end);
		}

		if (b->lightning)
		{
			if (cl_beams_lightatend.integer)
			{
				// FIXME: create a matrix from the beam start/end orientation
				Matrix4x4_CreateTranslate(&tempmatrix, b->end[0], b->end[1], b->end[2]);
				CL_AllocDlight (NULL, &tempmatrix, 200, 0.3, 0.7, 1, 0, 0, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
			}
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
			//ent->render.effects = EF_FULLBRIGHT;
			//ent->render.angles[0] = pitch;
			//ent->render.angles[1] = yaw;
			//ent->render.angles[2] = rand()%360;
			Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, org[0], org[1], org[2], -pitch, yaw, lhrandom(0, 360), 1);
			Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
			R_LerpAnimation(&ent->render);
			CL_BoundingBoxForEntity(&ent->render);
			VectorMA(org, 30, dist, org);
			d -= 30;
		}
	}

	while (cl_activebeams > 0 && !cl_beams[cl_activebeams - 1].model)
		cl_activebeams--;
}

static void CL_RelinkQWNails(void)
{
	int i;
	vec_t *v;
	entity_t *ent;

	for (i = 0;i < cl.qw_num_nails;i++)
	{
		v = cl.qw_nails[i];

		// if we're drawing effects, get a new temp entity
		// (NewTempEntity adds it to the render entities list for us)
		if (!(ent = CL_NewTempEntity()))
			continue;

		// normal stuff
		ent->render.model = cl.model_precache[cl.qw_modelindex_spike];
		ent->render.colormap = -1; // no special coloring
		ent->render.alpha = 1;
		VectorSet(ent->render.colormod, 1, 1, 1);

		Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, v[0], v[1], v[2], v[3], v[4], v[5], 1);
		Matrix4x4_Invert_Simple(&ent->render.inversematrix, &ent->render.matrix);
		R_LerpAnimation(&ent->render);
		CL_BoundingBoxForEntity(&ent->render);
	}
}

void CL_LerpPlayer(float frac)
{
	int i;
	float d;

	cl.viewzoom = cl.mviewzoom[1] + frac * (cl.mviewzoom[0] - cl.mviewzoom[1]);
	for (i = 0;i < 3;i++)
	{
		cl.punchangle[i] = cl.mpunchangle[1][i] + frac * (cl.mpunchangle[0][i] - cl.mpunchangle[1][i]);
		cl.punchvector[i] = cl.mpunchvector[1][i] + frac * (cl.mpunchvector[0][i] - cl.mpunchvector[1][i]);
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);
	}

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

void CSQC_RelinkAllEntities (int drawmask)
{
	CL_RelinkNetworkEntities(drawmask);
	if(drawmask & ENTMASK_ENGINE)
	{
		// move particles
		CL_MoveParticles();
		R_MoveExplosions();
	}

	// link stuff
	CL_RelinkWorld();
	CL_RelinkCSQCWorld();	//[515]: csqc
	if(drawmask & ENTMASK_ENGINE)
	{
		CL_RelinkStaticEntities();
		CL_RelinkBeams();
		CL_RelinkEffects();
		CL_RelinkQWNails();
	}
}

/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
extern void CL_ClientMovement_Replay();

int CL_ReadFromServer(void)
{
	CL_ReadDemoMessage();
	CL_SendMove();

	r_refdef.time = cl.time;
	r_refdef.extraupdate = !r_speeds.integer;
	r_refdef.numentities = 0;
	r_refdef.viewentitymatrix = identitymatrix;
	cl_num_brushmodel_entities = 0;

	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		// prepare for a new frame
		CL_LerpPlayer(CL_LerpPoint());
		CL_DecayLights();
		CL_ClearTempEntities();
		V_DriftPitch();
		V_FadeViewFlashs();

		// relink network entities (note: this sets up the view!)
		CL_ClientMovement_Replay();
		if(!csqc_loaded)	//[515]: csqc
		{
			CL_RelinkNetworkEntities(65535);

			// move particles
			CL_MoveParticles();
			R_MoveExplosions();

			// link stuff
			CL_RelinkWorld();
			CL_RelinkCSQCWorld();	//[515]: csqc
			CL_RelinkStaticEntities();
			CL_RelinkBeams();
			CL_RelinkEffects();
			CL_RelinkQWNails();
		}
		else
			csqc_frame = true;

		CL_UpdateLights();

		// update view blend
		V_CalcViewBlend();
	}

	return 0;
}

// LordHavoc: pausedemo command
static void CL_PauseDemo_f (void)
{
	cls.demopaused = !cls.demopaused;
	if (cls.demopaused)
		Con_Print("Demo paused\n");
	else
		Con_Print("Demo unpaused\n");
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
		Con_Printf("\"fog\" is \"%f %f %f %f\"\n", fog_density, fog_red, fog_green, fog_blue);
		return;
	}
	fog_density = atof(Cmd_Argv(1));
	fog_red = atof(Cmd_Argv(2));
	fog_green = atof(Cmd_Argv(3));
	fog_blue = atof(Cmd_Argv(4));
}

/*
====================
CL_TimeRefresh_f

For program optimization
====================
*/
static void CL_TimeRefresh_f (void)
{
	int i;
	float timestart, timedelta, oldangles[3];

	r_refdef.extraupdate = false;
	VectorCopy(cl.viewangles, oldangles);
	VectorClear(cl.viewangles);

	timestart = Sys_DoubleTime();
	for (i = 0;i < 128;i++)
	{
		Matrix4x4_CreateFromQuakeEntity(&r_refdef.viewentitymatrix, r_vieworigin[0], r_vieworigin[1], r_vieworigin[2], 0, i / 128.0 * 360.0, 0, 1);
		CL_UpdateScreen();
	}
	timedelta = Sys_DoubleTime() - timestart;

	VectorCopy(oldangles, cl.viewangles);
	Con_Printf("%f seconds (%f fps)\n", timedelta, 128/timedelta);
}

/*
===========
CL_Shutdown
===========
*/
void CL_Shutdown (void)
{
	CL_Particles_Shutdown();
	CL_Parse_Shutdown();

	Mem_FreePool (&cl_mempool);
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
	cl_mempool = Mem_AllocPool("client", 0, NULL);

	memset(&r_refdef, 0, sizeof(r_refdef));
	// max entities sent to renderer per frame
	r_refdef.maxentities = MAX_EDICTS + 256 + 512;
	r_refdef.entities = (entity_render_t **)Mem_Alloc(cl_mempool, sizeof(entity_render_t *) * r_refdef.maxentities);
	// 256k drawqueue buffer
	// TODO: make dynamic
	r_refdef.maxdrawqueuesize = 256 * 1024;
	r_refdef.drawqueue = (unsigned char *)Mem_Alloc(cl_mempool, r_refdef.maxdrawqueuesize);

	CL_InitInput ();

//
// register our commands
//
	Cvar_RegisterVariable (&csqc_progname);
	Cvar_RegisterVariable (&csqc_progcrc);

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

	Cmd_AddCommand ("entities", CL_PrintEntities_f, "print information on network entities known to client");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "disconnect from server (or disconnect all clients if running a server)");
	Cmd_AddCommand ("record", CL_Record_f, "record a demo");
	Cmd_AddCommand ("stop", CL_Stop_f, "stop recording or playing a demo");
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "watch a demo file");
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f, "play back a demo as fast as possible and save statistics to benchmark.log");

	Cmd_AddCommand ("fog", CL_Fog_f, "set global fog parameters (density red green blue)");

	// LordHavoc: added pausedemo
	Cmd_AddCommand ("pausedemo", CL_PauseDemo_f, "pause demo playback (can also safely pause demo recording if using QUAKE, QUAKEDP or NEHAHRAMOVIE protocol, useful for making movies)");

	Cvar_RegisterVariable(&r_draweffects);
	Cvar_RegisterVariable(&cl_explosions_alpha_start);
	Cvar_RegisterVariable(&cl_explosions_alpha_end);
	Cvar_RegisterVariable(&cl_explosions_size_start);
	Cvar_RegisterVariable(&cl_explosions_size_end);
	Cvar_RegisterVariable(&cl_explosions_lifetime);
	Cvar_RegisterVariable(&cl_stainmaps);
	Cvar_RegisterVariable(&cl_stainmaps_clearonload);
	Cvar_RegisterVariable(&cl_beams_polygons);
	Cvar_RegisterVariable(&cl_beams_relative);
	Cvar_RegisterVariable(&cl_beams_lightatend);
	Cvar_RegisterVariable(&cl_noplayershadow);

	Cvar_RegisterVariable(&cl_prydoncursor);

	Cvar_RegisterVariable(&cl_deathnoviewmodel);

	// for QW connections
	Cvar_RegisterVariable(&qport);
	Cvar_SetValueQuick(&qport, (rand() * RAND_MAX + rand()) & 0xffff);

	Cmd_AddCommand("timerefresh", CL_TimeRefresh_f, "turn quickly and print rendering statistcs");

	CL_Parse_Init();
	CL_Particles_Init();
	CL_Screen_Init();

	CL_Video_Init();
}



