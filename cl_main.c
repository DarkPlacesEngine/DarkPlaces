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
#include "cl_gecko.h"
#include "cl_video.h"
#include "image.h"
#include "csprogs.h"
#include "r_shadow.h"
#include "libcurl.h"
#include "snd_main.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

cvar_t csqc_progname = {0, "csqc_progname","csprogs.dat","name of csprogs.dat file to load"};
cvar_t csqc_progcrc = {CVAR_READONLY, "csqc_progcrc","-1","CRC of csprogs.dat file to load (-1 is none), only used during level changes and then reset to -1"};
cvar_t csqc_progsize = {CVAR_READONLY, "csqc_progsize","-1","file size of csprogs.dat file to load (-1 is none), only used during level changes and then reset to -1"};

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

cvar_t cl_autodemo = {CVAR_SAVE, "cl_autodemo", "0", "records every game played, using the date/time and map name to name the demo file" };
cvar_t cl_autodemo_nameformat = {CVAR_SAVE, "cl_autodemo_nameformat", "autodemos/%Y-%m-%d_%H-%M", "The format of the cl_autodemo filename, followed by the map name" };

cvar_t r_draweffects = {0, "r_draweffects", "1","renders temporary sprite effects"};

cvar_t cl_explosions_alpha_start = {CVAR_SAVE, "cl_explosions_alpha_start", "1.5","starting alpha of an explosion shell"};
cvar_t cl_explosions_alpha_end = {CVAR_SAVE, "cl_explosions_alpha_end", "0","end alpha of an explosion shell (just before it disappears)"};
cvar_t cl_explosions_size_start = {CVAR_SAVE, "cl_explosions_size_start", "16","starting size of an explosion shell"};
cvar_t cl_explosions_size_end = {CVAR_SAVE, "cl_explosions_size_end", "128","ending alpha of an explosion shell (just before it disappears)"};
cvar_t cl_explosions_lifetime = {CVAR_SAVE, "cl_explosions_lifetime", "0.5","how long an explosion shell lasts"};

cvar_t cl_stainmaps = {CVAR_SAVE, "cl_stainmaps", "0","stains lightmaps, much faster than decals but blurred"};
cvar_t cl_stainmaps_clearonload = {CVAR_SAVE, "cl_stainmaps_clearonload", "1","clear stainmaps on map restart"};

cvar_t cl_beams_polygons = {CVAR_SAVE, "cl_beams_polygons", "1","use beam polygons instead of models"};
cvar_t cl_beams_quakepositionhack = {CVAR_SAVE, "cl_beams_quakepositionhack", "1", "makes your lightning gun appear to fire from your waist (as in Quake and QuakeWorld)"};
cvar_t cl_beams_instantaimhack = {CVAR_SAVE, "cl_beams_instantaimhack", "0", "makes your lightning gun aiming update instantly"};
cvar_t cl_beams_lightatend = {CVAR_SAVE, "cl_beams_lightatend", "0", "make a light at the end of the beam"};

cvar_t cl_noplayershadow = {CVAR_SAVE, "cl_noplayershadow", "0","hide player shadow"};

cvar_t cl_dlights_decayradius = {CVAR_SAVE, "cl_dlights_decayradius", "1", "reduces size of light flashes over time"};
cvar_t cl_dlights_decaybrightness = {CVAR_SAVE, "cl_dlights_decaybrightness", "1", "reduces brightness of light flashes over time"};

cvar_t qport = {0, "qport", "0", "identification key for playing on qw servers (allows you to maintain a connection to a quakeworld server even if your port changes)"};

cvar_t cl_prydoncursor = {0, "cl_prydoncursor", "0", "enables a mouse pointer which is able to click on entities in the world, useful for point and click mods, see PRYDON_CLIENTCURSOR extension in dpextensions.qc"};

cvar_t cl_deathnoviewmodel = {0, "cl_deathnoviewmodel", "1", "hides gun model when dead"};

cvar_t cl_locs_enable = {CVAR_SAVE, "locs_enable", "1", "enables replacement of certain % codes in chat messages: %l (location), %d (last death location), %h (health), %a (armor), %x (rockets), %c (cells), %r (rocket launcher status), %p (powerup status), %w (weapon status), %t (current time in level)"};
cvar_t cl_locs_show = {0, "locs_show", "0", "shows defined locations for editing purposes"};

client_static_t	cls;
client_state_t	cl;

#define MAX_PARTICLES			32768	// default max # of particles at one time
#define MAX_DECALS				32768	// default max # of decals at one time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's on the command line

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState(void)
{
	int i;
	entity_t *ent;

// wipe the entire cl structure
	Mem_EmptyPool(cls.levelmempool);
	memset (&cl, 0, sizeof(cl));

	S_StopAllSounds();

	// reset the view zoom interpolation
	cl.mviewzoom[0] = cl.mviewzoom[1] = 1;
	cl.sensitivityscale = 1.0f;

	// enable rendering of the world and such
	cl.csqc_vidvars.drawworld = true;
	cl.csqc_vidvars.drawenginesbar = true;
	cl.csqc_vidvars.drawcrosshair = true;

	// set up the float version of the stats array for easier access to float stats
	cl.statsf = (float *)cl.stats;

	cl.num_entities = 0;
	cl.num_static_entities = 0;
	cl.num_temp_entities = 0;
	cl.num_brushmodel_entities = 0;

	// tweak these if the game runs out
	cl.max_entities = 256;
	cl.max_static_entities = 256;
	cl.max_temp_entities = 512;
	cl.max_effects = 256;
	cl.max_beams = 256;
	cl.max_dlights = MAX_DLIGHTS;
	cl.max_lightstyle = MAX_LIGHTSTYLES;
	cl.max_brushmodel_entities = MAX_EDICTS;
	cl.max_particles = MAX_PARTICLES;
	cl.max_decals = MAX_DECALS;
	cl.max_showlmps = 0;

// COMMANDLINEOPTION: Client: -particles <number> changes maximum number of particles at once, default 32768
	i = COM_CheckParm ("-particles");
	if (i && i < com_argc - 1)
	{
		cl.max_particles = (int)(atoi(com_argv[i+1]));
		if (cl.max_particles < ABSOLUTE_MIN_PARTICLES)
			cl.max_particles = ABSOLUTE_MIN_PARTICLES;
	}

	cl.num_dlights = 0;
	cl.num_effects = 0;
	cl.num_beams = 0;

	cl.entities = (entity_t *)Mem_Alloc(cls.levelmempool, cl.max_entities * sizeof(entity_t));
	cl.entities_active = (unsigned char *)Mem_Alloc(cls.levelmempool, cl.max_brushmodel_entities * sizeof(unsigned char));
	cl.static_entities = (entity_t *)Mem_Alloc(cls.levelmempool, cl.max_static_entities * sizeof(entity_t));
	cl.temp_entities = (entity_t *)Mem_Alloc(cls.levelmempool, cl.max_temp_entities * sizeof(entity_t));
	cl.effects = (cl_effect_t *)Mem_Alloc(cls.levelmempool, cl.max_effects * sizeof(cl_effect_t));
	cl.beams = (beam_t *)Mem_Alloc(cls.levelmempool, cl.max_beams * sizeof(beam_t));
	cl.dlights = (dlight_t *)Mem_Alloc(cls.levelmempool, cl.max_dlights * sizeof(dlight_t));
	cl.lightstyle = (lightstyle_t *)Mem_Alloc(cls.levelmempool, cl.max_lightstyle * sizeof(lightstyle_t));
	cl.brushmodel_entities = (int *)Mem_Alloc(cls.levelmempool, cl.max_brushmodel_entities * sizeof(int));
	cl.particles = (particle_t *) Mem_Alloc(cls.levelmempool, cl.max_particles * sizeof(particle_t));
	cl.decals = (decal_t *) Mem_Alloc(cls.levelmempool, cl.max_decals * sizeof(decal_t));
	cl.showlmps = NULL;

	// LordHavoc: have to set up the baseline info for alpha and other stuff
	for (i = 0;i < cl.max_entities;i++)
	{
		cl.entities[i].state_baseline = defaultstate;
		cl.entities[i].state_previous = defaultstate;
		cl.entities[i].state_current = defaultstate;
	}

	if (gamemode == GAME_NEXUIZ)
	{
		VectorSet(cl.playerstandmins, -16, -16, -24);
		VectorSet(cl.playerstandmaxs, 16, 16, 45);
		VectorSet(cl.playercrouchmins, -16, -16, -24);
		VectorSet(cl.playercrouchmaxs, 16, 16, 25);
	}
	else
	{
		VectorSet(cl.playerstandmins, -16, -16, -24);
		VectorSet(cl.playerstandmaxs, 16, 16, 24);
		VectorSet(cl.playercrouchmins, -16, -16, -24);
		VectorSet(cl.playercrouchmaxs, 16, 16, 24);
	}

	// disable until we get textures for it
	R_ResetSkyBox();

	ent = &cl.entities[0];
	// entire entity array was cleared, so just fill in a few fields
	ent->state_current.active = true;
	ent->render.model = cl.worldmodel = NULL; // no world model yet
	ent->render.alpha = 1;
	ent->render.flags = RENDER_SHADOW | RENDER_LIGHT;
	Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, 0, 0, 0, 0, 0, 0, 1);
	CL_UpdateRenderEntity(&ent->render);

	// noclip is turned off at start
	noclip_anglehack = false;

	// mark all frames invalid for delta
	memset(cl.qw_deltasequence, -1, sizeof(cl.qw_deltasequence));

	// set bestweapon data back to Quake data
	IN_BestWeapon_ResetData();

	CL_Screen_NewMap();
}

void CL_SetInfo(const char *key, const char *value, qboolean send, qboolean allowstarkey, qboolean allowmodel, qboolean quiet)
{
	int i;
	qboolean fail = false;
	if (!allowstarkey && key[0] == '*')
		fail = true;
	if (!allowmodel && (!strcasecmp(key, "pmodel") || !strcasecmp(key, "emodel")))
		fail = true;
	for (i = 0;key[i];i++)
		if (key[i] <= ' ' || key[i] == '\"')
			fail = true;
	for (i = 0;value[i];i++)
		if (value[i] == '\r' || value[i] == '\n' || value[i] == '\"')
			fail = true;
	if (fail)
	{
		if (!quiet)
			Con_Printf("Can't setinfo \"%s\" \"%s\"\n", key, value);
		return;
	}
	InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), key, value);
	if (cls.state == ca_connected && cls.netcon)
	{
		if (cls.protocol == PROTOCOL_QUAKEWORLD)
		{
			MSG_WriteByte(&cls.netcon->message, qw_clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, va("setinfo \"%s\" \"%s\"", key, value));
		}
		else if (!strcasecmp(key, "name"))
		{
			MSG_WriteByte(&cls.netcon->message, clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, va("name \"%s\"", value));
		}
		else if (!strcasecmp(key, "playermodel"))
		{
			MSG_WriteByte(&cls.netcon->message, clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, va("playermodel \"%s\"", value));
		}
		else if (!strcasecmp(key, "playerskin"))
		{
			MSG_WriteByte(&cls.netcon->message, clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, va("playerskin \"%s\"", value));
		}
		else if (!strcasecmp(key, "topcolor"))
		{
			// don't send anything, the combined color code will be updated manually
		}
		else if (!strcasecmp(key, "bottomcolor"))
		{
			// don't send anything, the combined color code will be updated manually
		}
		else if (!strcasecmp(key, "rate"))
		{
			MSG_WriteByte(&cls.netcon->message, clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, va("rate \"%s\"", value));
		}
	}
}

void CL_ExpandEntities(int num)
{
	int i, oldmaxentities;
	entity_t *oldentities;
	if (num >= cl.max_entities)
	{
		if (!cl.entities)
			Sys_Error("CL_ExpandEntities: cl.entities not initialized");
		if (num >= MAX_EDICTS)
			Host_Error("CL_ExpandEntities: num %i >= %i", num, MAX_EDICTS);
		oldmaxentities = cl.max_entities;
		oldentities = cl.entities;
		cl.max_entities = (num & ~255) + 256;
		cl.entities = (entity_t *)Mem_Alloc(cls.levelmempool, cl.max_entities * sizeof(entity_t));
		memcpy(cl.entities, oldentities, oldmaxentities * sizeof(entity_t));
		Mem_Free(oldentities);
		for (i = oldmaxentities;i < cl.max_entities;i++)
		{
			cl.entities[i].state_baseline = defaultstate;
			cl.entities[i].state_previous = defaultstate;
			cl.entities[i].state_current = defaultstate;
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

	Curl_Clear_forthismap();

	Con_DPrintf("CL_Disconnect\n");

	CL_VM_ShutDown();
// stop sounds (especially looping!)
	S_StopAllSounds ();

	cl.parsingtextexpectingpingforscores = 0; // just in case no reply has come yet

	// clear contents blends
	cl.cshifts[0].percent = 0;
	cl.cshifts[1].percent = 0;
	cl.cshifts[2].percent = 0;
	cl.cshifts[3].percent = 0;

	cl.worldmodel = NULL;

	CL_Parse_ErrorCleanUp();

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
		NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol, 10000, false);
		NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol, 10000, false);
		NetConn_SendUnreliableMessage(cls.netcon, &buf, cls.protocol, 10000, false);
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
	if (cls.demoplayback)
		CL_StopPlayback();

	// if downloads are running, cancel their finishing action
	Curl_Clear_forthismap();

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
	int i;

	for (i = 0, ent = cl.entities;i < cl.num_entities;i++, ent++)
	{
		const char* modelname;

		if (!ent->state_current.active)
			continue;

		if (ent->render.model)
			modelname = ent->render.model->name;
		else
			modelname = "--no model--";
		Con_Printf("%3i: %-25s:%4i (%5i %5i %5i) [%3i %3i %3i] %4.2f %5.3f\n", i, modelname, ent->render.frame2, (int) ent->state_current.origin[0], (int) ent->state_current.origin[1], (int) ent->state_current.origin[2], (int) ent->state_current.angles[0] % 360, (int) ent->state_current.angles[1] % 360, (int) ent->state_current.angles[2] % 360, ent->render.scale, ent->render.alpha);
	}
}

/*
===============
CL_ModelIndexList_f

List information on all models in the client modelindex
===============
*/
static void CL_ModelIndexList_f(void)
{
	int i = 1;

	// Print Header
	Con_Printf("%3s: %-30s %-8s %-8s\n", "ID", "Name", "Type", "Triangles");

	while(cl.model_precache[i] && i != MAX_MODELS)
	{ // Valid Model
		if(cl.model_precache[i]->loaded || cl.model_precache[i]->isworldmodel)
			Con_Printf("%3i: %-30s %-8s %-10i\n", i, cl.model_precache[i]->name, cl.model_precache[i]->modeldatatypestring, cl.model_precache[i]->surfmesh.num_triangles);
		else
			Con_Printf("%3i: %-30s %-30s\n", i, cl.model_precache[i]->name, "--no local model found--");
		i++;
	}
}

/*
===============
CL_SoundIndexList_f

List all sounds in the client soundindex
===============
*/
static void CL_SoundIndexList_f(void)
{
	int i = 1;

	while(cl.sound_precache[i] && i != MAX_SOUNDS)
	{ // Valid Sound
		Con_Printf("%i : %s\n", i, cl.sound_precache[i]->name);
		i++;
	}
}

static void CL_UpdateRenderEntity_Lighting(entity_render_t *ent)
{
	vec3_t tempdiffusenormal;

	// fetch the lighting from the worldmodel data
	VectorSet(ent->modellight_ambient, r_ambient.value * (2.0f / 128.0f), r_ambient.value * (2.0f / 128.0f), r_ambient.value * (2.0f / 128.0f));
	VectorClear(ent->modellight_diffuse);
	VectorClear(tempdiffusenormal);
	if ((ent->flags & RENDER_LIGHT) && cl.worldmodel && cl.worldmodel->brush.LightPoint)
	{
		vec3_t org;
		Matrix4x4_OriginFromMatrix(&ent->matrix, org);
		cl.worldmodel->brush.LightPoint(cl.worldmodel, org, ent->modellight_ambient, ent->modellight_diffuse, tempdiffusenormal);
	}
	else // highly rare
		VectorSet(ent->modellight_ambient, 1, 1, 1);

	// move the light direction into modelspace coordinates for lighting code
	Matrix4x4_Transform3x3(&ent->inversematrix, tempdiffusenormal, ent->modellight_lightdir);
	if(VectorLength2(ent->modellight_lightdir) <= 0)
		VectorSet(ent->modellight_lightdir, 0, 0, 1); // have to set SOME valid vector here
	VectorNormalize(ent->modellight_lightdir);
}

//static const vec3_t nomodelmins = {-16, -16, -16};
//static const vec3_t nomodelmaxs = {16, 16, 16};
void CL_UpdateRenderEntity(entity_render_t *ent)
{
	vec3_t org;
	vec_t scale;
	model_t *model = ent->model;
	// update the inverse matrix for the renderer
	Matrix4x4_Invert_Simple(&ent->inversematrix, &ent->matrix);
	// update the animation blend state
	R_LerpAnimation(ent);
	// we need the matrix origin to center the box
	Matrix4x4_OriginFromMatrix(&ent->matrix, org);
	// update entity->render.scale because the renderer needs it
	ent->scale = scale = Matrix4x4_ScaleFromMatrix(&ent->matrix);
	if (model)
	{
		// NOTE: this directly extracts vector components from the matrix, which relies on the matrix orientation!
#ifdef MATRIX4x4_OPENGLORIENTATION
		if (ent->matrix.m[0][2] != 0 || ent->matrix.m[1][2] != 0)
#else
		if (ent->matrix.m[2][0] != 0 || ent->matrix.m[2][1] != 0)
#endif
		{
			// pitch or roll
			VectorMA(org, scale, model->rotatedmins, ent->mins);
			VectorMA(org, scale, model->rotatedmaxs, ent->maxs);
		}
#ifdef MATRIX4x4_OPENGLORIENTATION
		else if (ent->matrix.m[1][0] != 0 || ent->matrix.m[0][1] != 0)
#else
		else if (ent->matrix.m[0][1] != 0 || ent->matrix.m[1][0] != 0)
#endif
		{
			// yaw
			VectorMA(org, scale, model->yawmins, ent->mins);
			VectorMA(org, scale, model->yawmaxs, ent->maxs);
		}
		else
		{
			VectorMA(org, scale, model->normalmins, ent->mins);
			VectorMA(org, scale, model->normalmaxs, ent->maxs);
		}
	}
	else
	{
		ent->mins[0] = org[0] - 16;
		ent->mins[1] = org[1] - 16;
		ent->mins[2] = org[2] - 16;
		ent->maxs[0] = org[0] + 16;
		ent->maxs[1] = org[1] + 16;
		ent->maxs[2] = org[2] + 16;
	}
	CL_UpdateRenderEntity_Lighting(ent);
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

	if (cl_nettimesyncboundmode.integer == 1)
		cl.time = bound(cl.mtime[1], cl.time, cl.mtime[0]);

	// LordHavoc: lerp in listen games as the server is being capped below the client (usually)
	if (cl.mtime[0] <= cl.mtime[1])
	{
		cl.time = cl.mtime[0];
		return 1;
	}

	f = (cl.time - cl.mtime[1]) / (cl.mtime[0] - cl.mtime[1]);
	return bound(0, f, 1);
}

void CL_ClearTempEntities (void)
{
	cl.num_temp_entities = 0;
}

entity_t *CL_NewTempEntity(void)
{
	entity_t *ent;

	if (r_refdef.numentities >= r_refdef.maxentities)
		return NULL;
	if (cl.num_temp_entities >= cl.max_temp_entities)
		return NULL;
	ent = &cl.temp_entities[cl.num_temp_entities++];
	memset (ent, 0, sizeof(*ent));
	r_refdef.entities[r_refdef.numentities++] = &ent->render;

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
	if (framerate < 1)
	{
		Con_Printf("CL_Effect: framerate %f is < 1\n", framerate);
		return;
	}
	if (framecount < 1)
	{
		Con_Printf("CL_Effect: framecount %i is < 1\n", framecount);
		return;
	}
	for (i = 0, e = cl.effects;i < cl.max_effects;i++, e++)
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
		cl.num_effects = max(cl.num_effects, i + 1);
		break;
	}
}

void CL_AllocLightFlash(entity_render_t *ent, matrix4x4_t *matrix, float radius, float red, float green, float blue, float decay, float lifetime, int cubemapnum, int style, int shadowenable, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags)
{
	int i;
	dlight_t *dl;

	/*
// first look for an exact key match
	if (ent)
	{
		dl = cl.dlights;
		for (i = 0;i < cl.num_dlights;i++, dl++)
			if (dl->ent == ent)
				goto dlightsetup;
	}
	*/

// then look for anything else
	dl = cl.dlights;
	for (i = 0;i < cl.num_dlights;i++, dl++)
		if (!dl->radius)
			goto dlightsetup;
	// if we hit the end of the active dlights and found no gaps, add a new one
	if (i < MAX_DLIGHTS)
	{
		cl.num_dlights = i + 1;
		goto dlightsetup;
	}

	// unable to find one
	return;

dlightsetup:
	//Con_Printf("dlight %i : %f %f %f : %f %f %f\n", i, org[0], org[1], org[2], red * radius, green * radius, blue * radius);
	memset (dl, 0, sizeof(*dl));
	Matrix4x4_Normalize(&dl->matrix, matrix);
	dl->ent = ent;
	Matrix4x4_OriginFromMatrix(&dl->matrix, dl->origin);
	CL_FindNonSolidLocation(dl->origin, dl->origin, 6);
	Matrix4x4_SetOrigin(&dl->matrix, dl->origin[0], dl->origin[1], dl->origin[2]);
	dl->radius = radius;
	dl->color[0] = red;
	dl->color[1] = green;
	dl->color[2] = blue;
	dl->initialradius = radius;
	dl->initialcolor[0] = red;
	dl->initialcolor[1] = green;
	dl->initialcolor[2] = blue;
	dl->decay = decay / radius; // changed decay to be a percentage decrease
	dl->intensity = 1; // this is what gets decayed
	if (lifetime)
		dl->die = cl.time + lifetime;
	else
		dl->die = 0;
	if (cubemapnum > 0)
		dpsnprintf(dl->cubemapname, sizeof(dl->cubemapname), "cubemaps/%i", cubemapnum);
	else
		dl->cubemapname[0] = 0;
	dl->style = style;
	dl->shadow = shadowenable;
	dl->corona = corona;
	dl->flags = flags;
	dl->coronasizescale = coronasizescale;
	dl->ambientscale = ambientscale;
	dl->diffusescale = diffusescale;
	dl->specularscale = specularscale;
}

void CL_DecayLightFlashes(void)
{
	int i, oldmax;
	dlight_t *dl;
	float time;

	time = bound(0, cl.time - cl.oldtime, 0.1);
	oldmax = cl.num_dlights;
	cl.num_dlights = 0;
	for (i = 0, dl = cl.dlights;i < oldmax;i++, dl++)
	{
		if (dl->radius)
		{
			dl->intensity -= time * dl->decay;
			if (cl.time < dl->die && dl->intensity > 0)
			{
				if (cl_dlights_decayradius.integer)
					dl->radius = dl->initialradius * dl->intensity;
				else
					dl->radius = dl->initialradius;
				if (cl_dlights_decaybrightness.integer)
					VectorScale(dl->initialcolor, dl->intensity, dl->color);
				else
					VectorCopy(dl->initialcolor, dl->color);
				cl.num_dlights = i + 1;
			}
			else
				dl->radius = 0;
		}
	}
}

// called before entity relinking
void CL_RelinkLightFlashes(void)
{
	int i, j, k, l;
	dlight_t *dl;
	float frac, f;
	matrix4x4_t tempmatrix;

	if (r_dynamic.integer)
	{
		for (i = 0, dl = cl.dlights;i < cl.num_dlights && r_refdef.numlights < MAX_DLIGHTS;i++, dl++)
		{
			if (dl->radius)
			{
				tempmatrix = dl->matrix;
				Matrix4x4_Scale(&tempmatrix, dl->radius, 1);
				R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &tempmatrix, dl->color, dl->style, dl->cubemapname, dl->shadow, dl->corona, dl->coronasizescale, dl->ambientscale, dl->diffusescale, dl->specularscale, dl->flags);
			}
		}
	}

// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	f = cl.time * 10;
	i = (int)floor(f);
	frac = f - i;
	for (j = 0;j < cl.max_lightstyle;j++)
	{
		if (!cl.lightstyle || !cl.lightstyle[j].length)
		{
			r_refdef.rtlightstylevalue[j] = 1;
			r_refdef.lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl.lightstyle[j].length;
		l = (i-1) % cl.lightstyle[j].length;
		k = cl.lightstyle[j].map[k] - 'a';
		l = cl.lightstyle[j].map[l] - 'a';
		// rtlightstylevalue is always interpolated because it has no bad
		// consequences for performance
		// lightstylevalue is subject to a cvar for performance reasons;
		// skipping lightmap updates on most rendered frames substantially
		// improves framerates (but makes light fades look bad)
		r_refdef.rtlightstylevalue[j] = ((k*frac)+(l*(1-frac)))*(22/256.0f);
		r_refdef.lightstylevalue[j] = r_lerplightstyles.integer ? (unsigned short)(((k*frac)+(l*(1-frac)))*22) : k*22;
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
	flag->render.alpha = 1;
	VectorSet(flag->render.colormod, 1, 1, 1);
	// attach the flag to the player matrix
	Matrix4x4_CreateFromQuakeEntity(&flagmatrix, -f, -22, 0, 0, 0, -45, 1);
	Matrix4x4_Concat(&flag->render.matrix, &player->render.matrix, &flagmatrix);
	CL_UpdateRenderEntity(&flag->render);
}

matrix4x4_t viewmodelmatrix;

static const vec3_t muzzleflashorigin = {18, 0, 0};

extern void V_DriftPitch(void);
extern void V_FadeViewFlashs(void);
extern void V_CalcViewBlend(void);
extern void V_CalcRefdef(void);

void CL_SetEntityColormapColors(entity_render_t *ent, int colormap)
{
	const unsigned char *cbcolor;
	if (colormap >= 0)
	{
		cbcolor = palette_rgb_pantscolormap[colormap & 0xF];
		VectorScale(cbcolor, (1.0f / 255.0f), ent->colormap_pantscolor);
		cbcolor = palette_rgb_shirtcolormap[(colormap & 0xF0) >> 4];
		VectorScale(cbcolor, (1.0f / 255.0f), ent->colormap_shirtcolor);
	}
	else
	{
		VectorClear(ent->colormap_pantscolor);
		VectorClear(ent->colormap_shirtcolor);
	}
}

// note this is a recursive function, recursionlimit should be 32 or so on the initial call
void CL_UpdateNetworkEntity(entity_t *e, int recursionlimit, qboolean interpolate)
{
	const matrix4x4_t *matrix;
	matrix4x4_t blendmatrix, tempmatrix, matrix2;
	int j, k, l, frame;
	float origin[3], angles[3], delta[3], lerp, d;
	entity_t *t;
	model_t *model;
	//entity_persistent_t *p = &e->persistent;
	//entity_render_t *r = &e->render;
	// skip inactive entities and world
	if (!e->state_current.active || e == cl.entities)
		return;
	if (recursionlimit < 1)
		return;
	e->render.alpha = e->state_current.alpha * (1.0f / 255.0f); // FIXME: interpolate?
	e->render.scale = e->state_current.scale * (1.0f / 16.0f); // FIXME: interpolate?
	e->render.flags = e->state_current.flags;
	e->render.effects = e->state_current.effects;
	VectorScale(e->state_current.colormod, (1.0f / 32.0f), e->render.colormod);
	e->render.entitynumber = e - cl.entities;
	if (e->state_current.flags & RENDER_COLORMAPPED)
		CL_SetEntityColormapColors(&e->render, e->state_current.colormap);
	else if (e->state_current.colormap > 0 && e->state_current.colormap <= cl.maxclients && cl.scores != NULL)
		CL_SetEntityColormapColors(&e->render, cl.scores[e->state_current.colormap-1].colors);
	else
		CL_SetEntityColormapColors(&e->render, -1);
	e->render.skinnum = e->state_current.skin;
	if (e->state_current.tagentity)
	{
		// attached entity (gun held in player model's hand, etc)
		// if the tag entity is currently impossible, skip it
		if (e->state_current.tagentity >= cl.num_entities)
			return;
		t = cl.entities + e->state_current.tagentity;
		// if the tag entity is inactive, skip it
		if (!t->state_current.active)
			return;
		// update the parent first
		CL_UpdateNetworkEntity(t, recursionlimit - 1, interpolate);
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
	else if (e->render.flags & RENDER_VIEWMODEL)
	{
		// view-relative entity (guns and such)
		if (e->render.effects & EF_NOGUNBOB)
			matrix = &r_refdef.view.matrix; // really attached to view
		else
			matrix = &viewmodelmatrix; // attached to gun bob matrix
	}
	else
	{
		// world-relative entity (the normal kind)
		matrix = &identitymatrix;
	}

	// movement lerp
	// if it's the predicted player entity, update according to client movement
	// but don't lerp if going through a teleporter as it causes a bad lerp
	// also don't use the predicted location if fixangle was set on both of
	// the most recent server messages, as that cause means you are spectating
	// someone or watching a cutscene of some sort
	if (cl_nolerp.integer || cls.timedemo)
		interpolate = false;
	if (e == cl.entities + cl.playerentity && cl.movement_predicted && (!cl.fixangle[1] || !cl.fixangle[0]))
	{
		lerp = (cl.time - cl.movement_time[2]) / (cl.movement_time[0] - cl.movement_time[1]);
		lerp = bound(0, lerp, 1);
		if (!interpolate)
			lerp = 1;
		VectorLerp(cl.movement_oldorigin, lerp, cl.movement_origin, origin);
		VectorSet(angles, 0, cl.viewangles[1], 0);
	}
	else if (interpolate && e->persistent.lerpdeltatime > 0 && (lerp = (cl.time - e->persistent.lerpstarttime) / e->persistent.lerpdeltatime) < 1)
	{
		// interpolate the origin and angles
		lerp = max(0, lerp);
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
	frame = e->state_current.frame;
	if (e->state_current.modelindex < MAX_MODELS)
		e->render.model = cl.model_precache[e->state_current.modelindex];
	else
		e->render.model = NULL;
	if (e->render.model)
	{
		if (e->render.skinnum >= e->render.model->numskins)
			e->render.skinnum = 0;
		if (frame >= e->render.model->numframes)
			frame = 0;
		// models can set flags such as EF_ROCKET
		// this 0xFF800000 mask is EF_NOMODELFLAGS plus all the higher EF_ flags such as EF_ROCKET
		if (!(e->render.effects & 0xFF800000))
			e->render.effects |= e->render.model->effects;
		// if model is alias or this is a tenebrae-like dlight, reverse pitch direction
		if (e->render.model->type == mod_alias)
			angles[0] = -angles[0];
		if ((e->render.effects & EF_SELECTABLE) && cl.cmd.cursor_entitynumber == e->state_current.number)
			VectorScale(e->render.colormod, 2, e->render.colormod);
	}
	// if model is alias or this is a tenebrae-like dlight, reverse pitch direction
	else if (e->state_current.lightpflags & PFLAGS_FULLDYNAMIC)
		angles[0] = -angles[0];

	if ((e->render.effects & EF_ROTATE) && !(e->render.flags & RENDER_VIEWMODEL))
	{
		angles[1] = ANGLEMOD(100*cl.time);
		if (cl_itembobheight.value)
			origin[2] += (cos(cl.time * cl_itembobspeed.value * (2.0 * M_PI)) + 1.0) * 0.5 * cl_itembobheight.value;
	}

	// animation lerp
	if (e->render.frame2 == frame)
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
		e->render.frame2 = frame;
		e->render.frame2time = cl.time;
		e->render.framelerp = 0;
	}

	// set up the render matrix
	if (matrix)
	{
		// attached entity, this requires a matrix multiply (concat)
		// FIXME: e->render.scale should go away
		Matrix4x4_CreateFromQuakeEntity(&matrix2, origin[0], origin[1], origin[2], angles[0], angles[1], angles[2], e->render.scale);
		// concat the matrices to make the entity relative to its tag
		Matrix4x4_Concat(&e->render.matrix, matrix, &matrix2);
		// get the origin from the new matrix
		Matrix4x4_OriginFromMatrix(&e->render.matrix, origin);
	}
	else
	{
		// unattached entities are faster to process
		Matrix4x4_CreateFromQuakeEntity(&e->render.matrix, origin[0], origin[1], origin[2], angles[0], angles[1], angles[2], e->render.scale);
	}

	// tenebrae's sprites are all additive mode (weird)
	if (gamemode == GAME_TENEBRAE && e->render.model && e->render.model->type == mod_sprite)
		e->render.effects |= EF_ADDITIVE;
	// player model is only shown with chase_active on
	if (e->state_current.number == cl.viewentity)
		e->render.flags |= RENDER_EXTERIORMODEL;
	// either fullbright or lit
	if (!(e->render.effects & EF_FULLBRIGHT) && !r_fullbright.integer)
		e->render.flags |= RENDER_LIGHT;
	// hide player shadow during intermission or nehahra movie
	if (!(e->render.effects & (EF_NOSHADOW | EF_ADDITIVE | EF_NODEPTHTEST))
	 && (e->render.alpha >= 1)
	 && !(e->render.flags & RENDER_VIEWMODEL)
	 && (!(e->render.flags & RENDER_EXTERIORMODEL) || (!cl.intermission && cls.protocol != PROTOCOL_NEHAHRAMOVIE && !cl_noplayershadow.integer)))
		e->render.flags |= RENDER_SHADOW;
	if (e->render.flags & RENDER_VIEWMODEL)
		e->render.flags |= RENDER_NOSELFSHADOW;

	// make the other useful stuff
	CL_UpdateRenderEntity(&e->render);
}

// creates light and trails from an entity
void CL_UpdateNetworkEntityTrail(entity_t *e)
{
	effectnameindex_t trailtype;
	vec3_t origin;

	// bmodels are treated specially since their origin is usually '0 0 0' and
	// their actual geometry is far from '0 0 0'
	if (e->render.model && e->render.model->soundfromcenter)
	{
		vec3_t o;
		VectorMAM(0.5f, e->render.model->normalmins, 0.5f, e->render.model->normalmaxs, o);
		Matrix4x4_Transform(&e->render.matrix, o, origin);
	}
	else
		Matrix4x4_OriginFromMatrix(&e->render.matrix, origin);

	// handle particle trails and such effects now that we know where this
	// entity is in the world...
	trailtype = EFFECT_NONE;
	// LordHavoc: if the entity has no effects, don't check each
	if (e->render.effects & (EF_BRIGHTFIELD | EF_FLAME | EF_STARDUST))
	{
		if (e->render.effects & EF_BRIGHTFIELD)
		{
			if (gamemode == GAME_NEXUIZ)
				trailtype = EFFECT_TR_NEXUIZPLASMA;
			else
				CL_EntityParticles(e);
		}
		if (e->render.effects & EF_FLAME)
			CL_ParticleTrail(EFFECT_EF_FLAME, bound(0, cl.time - cl.oldtime, 0.1), origin, origin, vec3_origin, vec3_origin, NULL, 0, false, true);
		if (e->render.effects & EF_STARDUST)
			CL_ParticleTrail(EFFECT_EF_STARDUST, bound(0, cl.time - cl.oldtime, 0.1), origin, origin, vec3_origin, vec3_origin, NULL, 0, false, true);
	}
	if (e->render.internaleffects & (INTEF_FLAG1QW | INTEF_FLAG2QW))
	{
		// these are only set on player entities
		CL_AddQWCTFFlagModel(e, (e->render.internaleffects & INTEF_FLAG2QW) != 0);
	}
	// muzzleflash fades over time
	if (e->persistent.muzzleflash > 0)
		e->persistent.muzzleflash -= bound(0, cl.time - cl.oldtime, 0.1) * 20;
	// LordHavoc: if the entity has no effects, don't check each
	if (e->render.effects && !(e->render.flags & RENDER_VIEWMODEL))
	{
		if (e->render.effects & EF_GIB)
			trailtype = EFFECT_TR_BLOOD;
		else if (e->render.effects & EF_ZOMGIB)
			trailtype = EFFECT_TR_SLIGHTBLOOD;
		else if (e->render.effects & EF_TRACER)
			trailtype = EFFECT_TR_WIZSPIKE;
		else if (e->render.effects & EF_TRACER2)
			trailtype = EFFECT_TR_KNIGHTSPIKE;
		else if (e->render.effects & EF_ROCKET)
			trailtype = EFFECT_TR_ROCKET;
		else if (e->render.effects & EF_GRENADE)
		{
			// LordHavoc: e->render.alpha == -1 is for Nehahra dem compatibility (cigar smoke)
			trailtype = e->render.alpha == -1 ? EFFECT_TR_NEHAHRASMOKE : EFFECT_TR_GRENADE;
		}
		else if (e->render.effects & EF_TRACER3)
			trailtype = EFFECT_TR_VORESPIKE;
	}
	// do trails
	if (e->render.flags & RENDER_GLOWTRAIL)
		trailtype = EFFECT_TR_GLOWTRAIL;
	// check if a trail is allowed (it is not after a teleport for example)
	if (trailtype && e->persistent.trail_allowed)
	{
		float len;
		vec3_t vel;
		VectorSubtract(e->state_current.origin, e->state_previous.origin, vel);
		len = e->state_current.time - e->state_previous.time;
		if (len > 0)
			len = 1.0f / len;
		VectorScale(vel, len, vel);
		CL_ParticleTrail(trailtype, 1, e->persistent.trail_origin, origin, vel, vel, e, e->state_current.glowcolor, false, true);
	}
	// now that the entity has survived one trail update it is allowed to
	// leave a real trail on later frames
	e->persistent.trail_allowed = true;
	VectorCopy(origin, e->persistent.trail_origin);
}


/*
===============
CL_UpdateViewEntities
===============
*/
void CL_UpdateViewEntities(void)
{
	int i;
	// update any RENDER_VIEWMODEL entities to use the new view matrix
	for (i = 1;i < cl.num_entities;i++)
	{
		if (cl.entities_active[i])
		{
			entity_t *ent = cl.entities + i;
			if ((ent->render.flags & RENDER_VIEWMODEL) || ent->state_current.tagentity)
				CL_UpdateNetworkEntity(ent, 32, true);
		}
	}
	// and of course the engine viewmodel needs updating as well
	CL_UpdateNetworkEntity(&cl.viewent, 32, true);
}

/*
===============
CL_UpdateNetworkCollisionEntities
===============
*/
void CL_UpdateNetworkCollisionEntities(void)
{
	entity_t *ent;
	int i;

	// start on the entity after the world
	cl.num_brushmodel_entities = 0;
	for (i = cl.maxclients + 1;i < cl.num_entities;i++)
	{
		if (cl.entities_active[i])
		{
			ent = cl.entities + i;
			if (ent->state_current.active && ent->render.model && ent->render.model->name[0] == '*' && ent->render.model->TraceBox)
			{
				// do not interpolate the bmodels for this
				CL_UpdateNetworkEntity(ent, 32, false);
				cl.brushmodel_entities[cl.num_brushmodel_entities++] = i;
			}
		}
	}
}

/*
===============
CL_UpdateNetworkEntities
===============
*/
void CL_UpdateNetworkEntities(void)
{
	entity_t *ent;
	int i;

	// start on the entity after the world
	for (i = 1;i < cl.num_entities;i++)
	{
		if (cl.entities_active[i])
		{
			ent = cl.entities + i;
			if (ent->state_current.active)
			{
				CL_UpdateNetworkEntity(ent, 32, true);
				// view models should never create light/trails
				if (!(ent->render.flags & RENDER_VIEWMODEL))
					CL_UpdateNetworkEntityTrail(ent);
			}
			else
				cl.entities_active[i] = false;
		}
	}
}

void CL_UpdateViewModel(void)
{
	entity_t *ent;
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
	ent->state_current.alpha = cl.entities[cl.viewentity].state_current.alpha;
	ent->state_current.effects = EF_NOSHADOW | (cl.entities[cl.viewentity].state_current.effects & (EF_ADDITIVE | EF_FULLBRIGHT | EF_NODEPTHTEST | EF_NOGUNBOB));

	// reset animation interpolation on weaponmodel if model changed
	if (ent->state_previous.modelindex != ent->state_current.modelindex)
	{
		ent->render.frame1 = ent->render.frame2 = ent->state_current.frame;
		ent->render.frame1time = ent->render.frame2time = cl.time;
		ent->render.framelerp = 1;
	}
	CL_UpdateNetworkEntity(ent, 32, true);
}

// note this is a recursive function, but it can never get in a runaway loop (because of the delayedlink flags)
void CL_LinkNetworkEntity(entity_t *e)
{
	effectnameindex_t trailtype;
	vec3_t origin;
	vec3_t dlightcolor;
	vec_t dlightradius;

	// skip inactive entities and world
	if (!e->state_current.active || e == cl.entities)
		return;
	if (e->state_current.tagentity)
	{
		// if the tag entity is currently impossible, skip it
		if (e->state_current.tagentity >= cl.num_entities)
			return;
		// if the tag entity is inactive, skip it
		if (!cl.entities[e->state_current.tagentity].state_current.active)
			return;
	}

	// create entity dlights associated with this entity
	if (e->render.model && e->render.model->soundfromcenter)
	{
		// bmodels are treated specially since their origin is usually '0 0 0'
		vec3_t o;
		VectorMAM(0.5f, e->render.model->normalmins, 0.5f, e->render.model->normalmaxs, o);
		Matrix4x4_Transform(&e->render.matrix, o, origin);
	}
	else
		Matrix4x4_OriginFromMatrix(&e->render.matrix, origin);
	trailtype = EFFECT_NONE;
	dlightradius = 0;
	dlightcolor[0] = 0;
	dlightcolor[1] = 0;
	dlightcolor[2] = 0;
	// LordHavoc: if the entity has no effects, don't check each
	if (e->render.effects & (EF_BRIGHTFIELD | EF_DIMLIGHT | EF_BRIGHTLIGHT | EF_RED | EF_BLUE | EF_FLAME | EF_STARDUST))
	{
		if (e->render.effects & EF_BRIGHTFIELD)
		{
			if (gamemode == GAME_NEXUIZ)
				trailtype = EFFECT_TR_NEXUIZPLASMA;
		}
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
			CL_ParticleTrail(EFFECT_EF_FLAME, 0, origin, origin, vec3_origin, vec3_origin, NULL, 0, true, false);
		if (e->render.effects & EF_STARDUST)
			CL_ParticleTrail(EFFECT_EF_STARDUST, 0, origin, origin, vec3_origin, vec3_origin, NULL, 0, true, false);
	}
	// muzzleflash fades over time, and is offset a bit
	if (e->persistent.muzzleflash > 0 && r_refdef.numlights < MAX_DLIGHTS)
	{
		vec3_t v2;
		vec3_t color;
		trace_t trace;
		matrix4x4_t tempmatrix;
		Matrix4x4_Transform(&e->render.matrix, muzzleflashorigin, v2);
		trace = CL_Move(origin, vec3_origin, vec3_origin, v2, MOVE_NOMONSTERS, NULL, SUPERCONTENTS_SOLID | SUPERCONTENTS_SKY, true, false, NULL, false);
		Matrix4x4_Normalize(&tempmatrix, &e->render.matrix);
		Matrix4x4_SetOrigin(&tempmatrix, trace.endpos[0], trace.endpos[1], trace.endpos[2]);
		Matrix4x4_Scale(&tempmatrix, 150, 1);
		VectorSet(color, e->persistent.muzzleflash * 4.0f, e->persistent.muzzleflash * 4.0f, e->persistent.muzzleflash * 4.0f);
		R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &tempmatrix, color, -1, NULL, true, 0, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	// LordHavoc: if the model has no flags, don't check each
	if (e->render.model && e->render.effects && !(e->render.flags & RENDER_VIEWMODEL))
	{
		if (e->render.effects & EF_GIB)
			trailtype = EFFECT_TR_BLOOD;
		else if (e->render.effects & EF_ZOMGIB)
			trailtype = EFFECT_TR_SLIGHTBLOOD;
		else if (e->render.effects & EF_TRACER)
			trailtype = EFFECT_TR_WIZSPIKE;
		else if (e->render.effects & EF_TRACER2)
			trailtype = EFFECT_TR_KNIGHTSPIKE;
		else if (e->render.effects & EF_ROCKET)
			trailtype = EFFECT_TR_ROCKET;
		else if (e->render.effects & EF_GRENADE)
		{
			// LordHavoc: e->render.alpha == -1 is for Nehahra dem compatibility (cigar smoke)
			trailtype = e->render.alpha == -1 ? EFFECT_TR_NEHAHRASMOKE : EFFECT_TR_GRENADE;
		}
		else if (e->render.effects & EF_TRACER3)
			trailtype = EFFECT_TR_VORESPIKE;
	}
	// LordHavoc: customizable glow
	if (e->state_current.glowsize)
	{
		// * 4 for the expansion from 0-255 to 0-1023 range,
		// / 255 to scale down byte colors
		dlightradius = max(dlightradius, e->state_current.glowsize * 4);
		VectorMA(dlightcolor, (1.0f / 255.0f), palette_rgb[e->state_current.glowcolor], dlightcolor);
	}
	// make the glow dlight
	if (dlightradius > 0 && (dlightcolor[0] || dlightcolor[1] || dlightcolor[2]) && !(e->render.flags & RENDER_VIEWMODEL) && r_refdef.numlights < MAX_DLIGHTS)
	{
		matrix4x4_t dlightmatrix;
		Matrix4x4_Normalize(&dlightmatrix, &e->render.matrix);
		// hack to make glowing player light shine on their gun
		//if (e->state_current.number == cl.viewentity/* && !chase_active.integer*/)
		//	Matrix4x4_AdjustOrigin(&dlightmatrix, 0, 0, 30);
		Matrix4x4_Scale(&dlightmatrix, dlightradius, 1);
		R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &dlightmatrix, dlightcolor, -1, NULL, true, 1, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	// custom rtlight
	if ((e->state_current.lightpflags & PFLAGS_FULLDYNAMIC) && r_refdef.numlights < MAX_DLIGHTS)
	{
		matrix4x4_t dlightmatrix;
		float light[4];
		VectorScale(e->state_current.light, (1.0f / 256.0f), light);
		light[3] = e->state_current.light[3];
		if (light[0] == 0 && light[1] == 0 && light[2] == 0)
			VectorSet(light, 1, 1, 1);
		if (light[3] == 0)
			light[3] = 350;
		// FIXME: add ambient/diffuse/specular scales as an extension ontop of TENEBRAE_GFX_DLIGHTS?
		Matrix4x4_Normalize(&dlightmatrix, &e->render.matrix);
		Matrix4x4_Scale(&dlightmatrix, light[3], 1);
		R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &dlightmatrix, light, e->state_current.lightstyle, e->state_current.skin > 0 ? va("cubemaps/%i", e->state_current.skin) : NULL, !(e->state_current.lightpflags & PFLAGS_NOSHADOW), (e->state_current.lightpflags & PFLAGS_CORONA) != 0, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	}
	// do trail light
	if (e->render.flags & RENDER_GLOWTRAIL)
		trailtype = EFFECT_TR_GLOWTRAIL;
	if (trailtype)
		CL_ParticleTrail(trailtype, 0, origin, origin, vec3_origin, vec3_origin, NULL, e->state_current.glowcolor, true, false);

	// don't show viewmodels in certain situations
	if (e->render.flags & RENDER_VIEWMODEL)
		if (!r_drawviewmodel.integer || chase_active.integer || r_refdef.envmap)
			return;
	// don't show entities with no modelindex (note: this still shows
	// entities which have a modelindex that resolved to a NULL model)
	if (e->render.model && !(e->render.effects & EF_NODRAW) && r_refdef.numentities < r_refdef.maxentities)
		r_refdef.entities[r_refdef.numentities++] = &e->render;
	//if (cl.viewentity && e->state_current.number == cl.viewentity)
	//	Matrix4x4_Print(&e->render.matrix);
}

void CL_RelinkWorld(void)
{
	entity_t *ent = &cl.entities[0];
	// FIXME: this should be done at load
	ent->render.matrix = identitymatrix;
	ent->render.flags = RENDER_SHADOW;
	if (!r_fullbright.integer)
		ent->render.flags |= RENDER_LIGHT;
	VectorSet(ent->render.colormod, 1, 1, 1);
	CL_UpdateRenderEntity(&ent->render);
	r_refdef.worldentity = &ent->render;
	r_refdef.worldmodel = cl.worldmodel;
}

static void CL_RelinkStaticEntities(void)
{
	int i;
	entity_t *e;
	for (i = 0, e = cl.static_entities;i < cl.num_static_entities && r_refdef.numentities < r_refdef.maxentities;i++, e++)
	{
		e->render.flags = 0;
		// if the model was not loaded when the static entity was created we
		// need to re-fetch the model pointer
		e->render.model = cl.model_precache[e->state_baseline.modelindex];
		// either fullbright or lit
		if (!(e->render.effects & EF_FULLBRIGHT) && !r_fullbright.integer)
			e->render.flags |= RENDER_LIGHT;
		// hide player shadow during intermission or nehahra movie
		if (!(e->render.effects & (EF_NOSHADOW | EF_ADDITIVE | EF_NODEPTHTEST)) && (e->render.alpha >= 1))
			e->render.flags |= RENDER_SHADOW;
		VectorSet(e->render.colormod, 1, 1, 1);
		R_LerpAnimation(&e->render);
		CL_UpdateRenderEntity(&e->render);
		r_refdef.entities[r_refdef.numentities++] = &e->render;
	}
}

/*
===============
CL_RelinkEntities
===============
*/
static void CL_RelinkNetworkEntities(void)
{
	entity_t *ent;
	int i;

	// start on the entity after the world
	for (i = 1;i < cl.num_entities;i++)
	{
		if (cl.entities_active[i])
		{
			ent = cl.entities + i;
			if (ent->state_current.active)
				CL_LinkNetworkEntity(ent);
			else
				cl.entities_active[i] = false;
		}
	}
}

static void CL_RelinkEffects(void)
{
	int i, intframe;
	cl_effect_t *e;
	entity_t *ent;
	float frame;

	for (i = 0, e = cl.effects;i < cl.num_effects;i++, e++)
	{
		if (e->active)
		{
			frame = (cl.time - e->starttime) * e->framerate + e->startframe;
			intframe = (int)frame;
			if (intframe < 0 || intframe >= e->endframe)
			{
				memset(e, 0, sizeof(*e));
				while (cl.num_effects > 0 && !cl.effects[cl.num_effects - 1].active)
					cl.num_effects--;
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
					ent->render.model = cl.csqc_model_precache[-(e->modelindex+1)];
				ent->render.alpha = 1;
				VectorSet(ent->render.colormod, 1, 1, 1);

				Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, e->origin[0], e->origin[1], e->origin[2], 0, 0, 0, 1);
				CL_UpdateRenderEntity(&ent->render);
			}
		}
	}
}

void CL_Beam_CalculatePositions(const beam_t *b, vec3_t start, vec3_t end)
{
	VectorCopy(b->start, start);
	VectorCopy(b->end, end);

	// if coming from the player, update the start position
	if (b->entity == cl.viewentity)
	{
		if (cl_beams_quakepositionhack.integer && !chase_active.integer)
		{
			// LordHavoc: this is a stupid hack from Quake that makes your
			// lightning appear to come from your waist and cover less of your
			// view
			// in Quake this hack was applied to all players (causing the
			// infamous crotch-lightning), but in darkplaces and QuakeWorld it
			// only applies to your own lightning, and only in first person
			Matrix4x4_OriginFromMatrix(&cl.entities[cl.viewentity].render.matrix, start);
		}
		if (cl_beams_instantaimhack.integer)
		{
			vec3_t dir, localend;
			vec_t len;
			// LordHavoc: this updates the beam direction to match your
			// viewangles
			VectorSubtract(end, start, dir);
			len = VectorLength(dir);
			VectorNormalize(dir);
			VectorSet(localend, len, 0, 0);
			Matrix4x4_Transform(&r_refdef.view.matrix, localend, end);
		}
	}
}

void CL_RelinkBeams(void)
{
	int i;
	beam_t *b;
	vec3_t dist, org, start, end;
	float d;
	entity_t *ent;
	double yaw, pitch;
	float forward;
	matrix4x4_t tempmatrix;

	for (i = 0, b = cl.beams;i < cl.num_beams;i++, b++)
	{
		if (!b->model)
			continue;
		if (b->endtime < cl.time)
		{
			b->model = NULL;
			continue;
		}

		CL_Beam_CalculatePositions(b, start, end);

		if (b->lightning)
		{
			if (cl_beams_lightatend.integer && r_refdef.numlights < MAX_DLIGHTS)
			{
				// FIXME: create a matrix from the beam start/end orientation
				vec3_t dlightcolor;
				VectorSet(dlightcolor, 0.3, 0.7, 1);
				Matrix4x4_CreateFromQuakeEntity(&tempmatrix, end[0], end[1], end[2], 0, 0, 0, 200);
				R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &tempmatrix, dlightcolor, -1, NULL, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
			}
			if (cl_beams_polygons.integer)
				continue;
		}

		// calculate pitch and yaw
		// (this is similar to the QuakeC builtin function vectoangles)
		VectorSubtract(end, start, dist);
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
			yaw = atan2(dist[1], dist[0]) * 180 / M_PI;
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = atan2(dist[2], forward) * 180 / M_PI;
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy (start, org);
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
			CL_UpdateRenderEntity(&ent->render);
			VectorMA(org, 30, dist, org);
			d -= 30;
		}
	}

	while (cl.num_beams > 0 && !cl.beams[cl.num_beams - 1].model)
		cl.num_beams--;
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
		ent->render.alpha = 1;
		VectorSet(ent->render.colormod, 1, 1, 1);

		Matrix4x4_CreateFromQuakeEntity(&ent->render.matrix, v[0], v[1], v[2], v[3], v[4], v[5], 1);
		CL_UpdateRenderEntity(&ent->render);
	}
}

void CL_LerpPlayer(float frac)
{
	int i;

	cl.viewzoom = cl.mviewzoom[1] + frac * (cl.mviewzoom[0] - cl.mviewzoom[1]);
	for (i = 0;i < 3;i++)
	{
		cl.punchangle[i] = cl.mpunchangle[1][i] + frac * (cl.mpunchangle[0][i] - cl.mpunchangle[1][i]);
		cl.punchvector[i] = cl.mpunchvector[1][i] + frac * (cl.mpunchvector[0][i] - cl.mpunchvector[1][i]);
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);
	}

	// interpolate the angles if playing a demo or spectating someone
	if (cls.demoplayback || cl.fixangle[0])
	{
		for (i = 0;i < 3;i++)
		{
			float d = cl.mviewangles[0][i] - cl.mviewangles[1][i];
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
	// link stuff
	CL_RelinkWorld();
	CL_RelinkStaticEntities();
	CL_RelinkBeams();
	CL_RelinkEffects();

	// link stuff
	if (drawmask & ENTMASK_ENGINE)
	{
		CL_RelinkNetworkEntities();
		if (drawmask & ENTMASK_ENGINEVIEWMODELS)
			CL_LinkNetworkEntity(&cl.viewent); // link gun model
		CL_RelinkQWNails();
	}

	// update view blend
	V_CalcViewBlend();
}

/*
===============
CL_UpdateWorld

Update client game world for a new frame
===============
*/
void CL_UpdateWorld(void)
{
	r_refdef.extraupdate = !r_speeds.integer;
	r_refdef.numentities = 0;
	r_refdef.numlights = 0;
	r_refdef.view.matrix = identitymatrix;

	cl.num_brushmodel_entities = 0;

	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		// prepare for a new frame
		CL_LerpPlayer(CL_LerpPoint());
		CL_DecayLightFlashes();
		CL_ClearTempEntities();
		V_DriftPitch();
		V_FadeViewFlashs();

		// if prediction is enabled we have to update all the collidable
		// network entities before the prediction code can be run
		CL_UpdateNetworkCollisionEntities();

		// now update the player prediction
		CL_ClientMovement_Replay();

		// update the player entity (which may be predicted)
		CL_UpdateNetworkEntity(cl.entities + cl.viewentity, 32, true);

		// now update the view (which depends on that player entity)
		V_CalcRefdef();

		// now update all the network entities and create particle trails
		// (some entities may depend on the view)
		CL_UpdateNetworkEntities();

		// update the engine-based viewmodel
		CL_UpdateViewModel();

		CL_RelinkLightFlashes();
		CSQC_RelinkAllEntities(ENTMASK_ENGINE | ENTMASK_ENGINEVIEWMODELS);

		// move decals, particles, and any other effects
		CL_MoveDecals();
		CL_MoveParticles();
		R_MoveExplosions();
	}

	r_refdef.time = cl.time;
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
		Con_Printf("\"fog\" is \"%f %f %f %f %f %f %f\"\n", r_refdef.fog_density, r_refdef.fog_red, r_refdef.fog_green, r_refdef.fog_blue, r_refdef.fog_alpha, r_refdef.fog_start, r_refdef.fog_end);
		return;
	}
	r_refdef.fog_start = 0;
	r_refdef.fog_end = 16384;
	r_refdef.fog_alpha = 1;
	if(Cmd_Argc() > 1)
		r_refdef.fog_density = atof(Cmd_Argv(1));
	if(Cmd_Argc() > 2)
		r_refdef.fog_red = atof(Cmd_Argv(2));
	if(Cmd_Argc() > 3)
		r_refdef.fog_green = atof(Cmd_Argv(3));
	if(Cmd_Argc() > 4)
		r_refdef.fog_blue = atof(Cmd_Argv(4));
	if(Cmd_Argc() > 5)
		r_refdef.fog_alpha = atof(Cmd_Argv(5));
	if(Cmd_Argc() > 6)
		r_refdef.fog_start = atof(Cmd_Argv(6));
	if(Cmd_Argc() > 7)
		r_refdef.fog_end = atof(Cmd_Argv(7));
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
	float timestart, timedelta;

	r_refdef.extraupdate = false;

	timestart = Sys_DoubleTime();
	for (i = 0;i < 128;i++)
	{
		Matrix4x4_CreateFromQuakeEntity(&r_refdef.view.matrix, r_refdef.view.origin[0], r_refdef.view.origin[1], r_refdef.view.origin[2], 0, i / 128.0 * 360.0, 0, 1);
		CL_UpdateScreen();
	}
	timedelta = Sys_DoubleTime() - timestart;

	Con_Printf("%f seconds (%f fps)\n", timedelta, 128/timedelta);
}

void CL_AreaStats_f(void)
{
	World_PrintAreaStats(&cl.world, "client");
}

cl_locnode_t *CL_Locs_FindNearest(const vec3_t point)
{
	int i;
	cl_locnode_t *loc;
	cl_locnode_t *best;
	vec3_t nearestpoint;
	vec_t dist, bestdist;
	best = NULL;
	bestdist = 0;
	for (loc = cl.locnodes;loc;loc = loc->next)
	{
		for (i = 0;i < 3;i++)
			nearestpoint[i] = bound(loc->mins[i], point[i], loc->maxs[i]);
		dist = VectorDistance2(nearestpoint, point);
		if (bestdist > dist || !best)
		{
			bestdist = dist;
			best = loc;
			if (bestdist < 1)
				break;
		}
	}
	return best;
}

void CL_Locs_FindLocationName(char *buffer, size_t buffersize, vec3_t point)
{
	cl_locnode_t *loc;
	loc = CL_Locs_FindNearest(point);
	if (loc)
		strlcpy(buffer, loc->name, buffersize);
	else
		dpsnprintf(buffer, buffersize, "LOC=%.0f:%.0f:%.0f", point[0], point[1], point[2]);
}

void CL_Locs_FreeNode(cl_locnode_t *node)
{
	cl_locnode_t **pointer, **next;
	for (pointer = &cl.locnodes;*pointer;pointer = next)
	{
		next = &(*pointer)->next;
		if (*pointer == node)
		{
			*pointer = node->next;
			Mem_Free(node);
		}
	}
	Con_Printf("CL_Locs_FreeNode: no such node! (%p)\n", node);
}

void CL_Locs_AddNode(vec3_t mins, vec3_t maxs, const char *name)
{
	cl_locnode_t *node, **pointer;
	int namelen;
	if (!name)
		name = "";
	namelen = strlen(name);
	node = Mem_Alloc(cls.levelmempool, sizeof(cl_locnode_t) + namelen + 1);
	VectorSet(node->mins, min(mins[0], maxs[0]), min(mins[1], maxs[1]), min(mins[2], maxs[2]));
	VectorSet(node->maxs, max(mins[0], maxs[0]), max(mins[1], maxs[1]), max(mins[2], maxs[2]));
	node->name = (char *)(node + 1);
	memcpy(node->name, name, namelen);
	node->name[namelen] = 0;
	// link it into the tail of the list to preserve the order
	for (pointer = &cl.locnodes;*pointer;pointer = &(*pointer)->next)
		;
	*pointer = node;
}

void CL_Locs_Add_f(void)
{
	vec3_t mins, maxs;
	if (Cmd_Argc() != 5 && Cmd_Argc() != 8)
	{
		Con_Printf("usage: %s x y z[ x y z] name\n", Cmd_Argv(0));
		return;
	}
	mins[0] = atof(Cmd_Argv(1));
	mins[1] = atof(Cmd_Argv(2));
	mins[2] = atof(Cmd_Argv(3));
	if (Cmd_Argc() == 8)
	{
		maxs[0] = atof(Cmd_Argv(4));
		maxs[1] = atof(Cmd_Argv(5));
		maxs[2] = atof(Cmd_Argv(6));
		CL_Locs_AddNode(mins, maxs, Cmd_Argv(7));
	}
	else
		CL_Locs_AddNode(mins, mins, Cmd_Argv(4));
}

void CL_Locs_RemoveNearest_f(void)
{
	cl_locnode_t *loc;
	loc = CL_Locs_FindNearest(r_refdef.view.origin);
	if (loc)
		CL_Locs_FreeNode(loc);
	else
		Con_Printf("no loc point or box found for your location\n");
}

void CL_Locs_Clear_f(void)
{
	while (cl.locnodes)
		CL_Locs_FreeNode(cl.locnodes);
}

void CL_Locs_Save_f(void)
{
	cl_locnode_t *loc;
	qfile_t *outfile;
	char locfilename[MAX_QPATH];
	if (!cl.locnodes)
	{
		Con_Printf("No loc points/boxes exist!\n");
		return;
	}
	if (cls.state != ca_connected || !cl.worldmodel)
	{
		Con_Printf("No level loaded!\n");
		return;
	}
	FS_StripExtension(cl.worldmodel->name, locfilename, sizeof(locfilename));
	strlcat(locfilename, ".loc", sizeof(locfilename));

	outfile = FS_Open(locfilename, "w", false, false);
	if (!outfile)
		return;
	// if any boxes are used then this is a proquake-format loc file, which
	// allows comments, so add some relevant information at the start
	for (loc = cl.locnodes;loc;loc = loc->next)
		if (!VectorCompare(loc->mins, loc->maxs))
			break;
	if (loc)
	{
		FS_Printf(outfile, "// %s %s saved by %s\n// x,y,z,x,y,z,\"name\"\n\n", locfilename, Sys_TimeString("%Y-%m-%d"), engineversion);
		for (loc = cl.locnodes;loc;loc = loc->next)
			if (VectorCompare(loc->mins, loc->maxs))
				break;
		if (loc)
			Con_Printf("Warning: writing loc file containing a mixture of qizmo-style points and proquake-style boxes may not work in qizmo or proquake!\n");
	}
	for (loc = cl.locnodes;loc;loc = loc->next)
	{
		if (VectorCompare(loc->mins, loc->maxs))
		{
			int len;
			const char *s;
			const char *in = loc->name;
			char name[MAX_INPUTLINE];
			for (len = 0;len < (int)sizeof(name) - 1 && *in;)
			{
				if (*in == ' ') {s = "$loc_name_separator";in++;}
				else if (!strncmp(in, "SSG", 3)) {s = "$loc_name_ssg";in += 3;}
				else if (!strncmp(in, "NG", 2)) {s = "$loc_name_ng";in += 2;}
				else if (!strncmp(in, "SNG", 3)) {s = "$loc_name_sng";in += 3;}
				else if (!strncmp(in, "GL", 2)) {s = "$loc_name_gl";in += 2;}
				else if (!strncmp(in, "RL", 2)) {s = "$loc_name_rl";in += 2;}
				else if (!strncmp(in, "LG", 2)) {s = "$loc_name_lg";in += 2;}
				else if (!strncmp(in, "GA", 2)) {s = "$loc_name_ga";in += 2;}
				else if (!strncmp(in, "YA", 2)) {s = "$loc_name_ya";in += 2;}
				else if (!strncmp(in, "RA", 2)) {s = "$loc_name_ra";in += 2;}
				else if (!strncmp(in, "MEGA", 4)) {s = "$loc_name_mh";in += 4;}
				else s = NULL;
				if (s)
				{
					while (len < (int)sizeof(name) - 1 && *s)
						name[len++] = *s++;
					continue;
				}
				name[len++] = *in++;
			}
			name[len] = 0;
			FS_Printf(outfile, "%.0f %.0f %.0f %s\n", loc->mins[0]*8, loc->mins[1]*8, loc->mins[2]*8, name);
		}
		else
			FS_Printf(outfile, "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,\"%s\"\n", loc->mins[0], loc->mins[1], loc->mins[2], loc->maxs[0], loc->maxs[1], loc->maxs[2], loc->name);
	}
	FS_Close(outfile);
}

void CL_Locs_Reload_f(void)
{
	int i, linenumber, limit, len;
	const char *s;
	char *filedata, *text, *textend, *linestart, *linetext, *lineend;
	fs_offset_t filesize;
	vec3_t mins, maxs;
	char locfilename[MAX_QPATH];
	char name[MAX_INPUTLINE];

	if (cls.state != ca_connected || !cl.worldmodel)
	{
		Con_Printf("No level loaded!\n");
		return;
	}

	CL_Locs_Clear_f();

	// try maps/something.loc first (LordHavoc: where I think they should be)
	FS_StripExtension(cl.worldmodel->name, locfilename, sizeof(locfilename));
	strlcat(locfilename, ".loc", sizeof(locfilename));
	filedata = (char *)FS_LoadFile(locfilename, cls.levelmempool, false, &filesize);
	if (!filedata)
	{
		// try proquake name as well (LordHavoc: I hate path mangling)
		FS_StripExtension(va("locs/%s", FS_FileWithoutPath(cl.worldmodel->name)), locfilename, sizeof(locfilename));
		strlcat(locfilename, ".loc", sizeof(locfilename));
		filedata = (char *)FS_LoadFile(locfilename, cls.levelmempool, false, &filesize);
		if (!filedata)
			return;
	}
	text = filedata;
	textend = filedata + filesize;
	for (linenumber = 1;text < textend;linenumber++)
	{
		linestart = text;
		for (;text < textend && *text != '\r' && *text != '\n';text++)
			;
		lineend = text;
		if (text + 1 < textend && *text == '\r' && text[1] == '\n')
			text++;
		if (text < textend)
			text++;
		// trim trailing whitespace
		while (lineend > linestart && lineend[-1] <= ' ')
			lineend--;
		// trim leading whitespace
		while (linestart < lineend && *linestart <= ' ')
			linestart++;
		// check if this is a comment
		if (linestart + 2 <= lineend && !strncmp(linestart, "//", 2))
			continue;
		linetext = linestart;
		limit = 3;
		for (i = 0;i < limit;i++)
		{
			if (linetext >= lineend)
				break;
			// note: a missing number is interpreted as 0
			if (i < 3)
				mins[i] = atof(linetext);
			else
				maxs[i - 3] = atof(linetext);
			// now advance past the number
			while (linetext < lineend && *linetext > ' ' && *linetext != ',')
				linetext++;
			// advance through whitespace
			if (linetext < lineend)
			{
				if (*linetext == ',')
				{
					linetext++;
					limit = 6;
					// note: comma can be followed by whitespace
				}
				if (*linetext <= ' ')
				{
					// skip whitespace
					while (linetext < lineend && *linetext <= ' ')
						linetext++;
				}
			}
		}
		// if this is a quoted name, remove the quotes
		if (i == 6)
		{
			if (linetext >= lineend || *linetext != '"')
				continue; // proquake location names are always quoted
			lineend--;
			linetext++;
			len = min(lineend - linetext, (int)sizeof(name) - 1);
			memcpy(name, linetext, len);
			name[len] = 0;
			// add the box to the list
			CL_Locs_AddNode(mins, maxs, name);
		}
		// if a point was parsed, it needs to be scaled down by 8 (since
		// point-based loc files were invented by a proxy which dealt
		// directly with quake protocol coordinates, which are *8), turn
		// it into a box
		else if (i == 3)
		{
			// interpret silly fuhquake macros
			for (len = 0;len < (int)sizeof(name) - 1 && linetext < lineend;)
			{
				if (*linetext == '$')
				{
					if (linetext + 18 <= lineend && !strncmp(linetext, "$loc_name_separator", 19)) {s = " ";linetext += 19;}
					else if (linetext + 13 <= lineend && !strncmp(linetext, "$loc_name_ssg", 13)) {s = "SSG";linetext += 13;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_ng", 12)) {s = "NG";linetext += 12;}
					else if (linetext + 13 <= lineend && !strncmp(linetext, "$loc_name_sng", 13)) {s = "SNG";linetext += 13;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_gl", 12)) {s = "GL";linetext += 12;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_rl", 12)) {s = "RL";linetext += 12;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_lg", 12)) {s = "LG";linetext += 12;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_ga", 12)) {s = "GA";linetext += 12;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_ya", 12)) {s = "YA";linetext += 12;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_ra", 12)) {s = "RA";linetext += 12;}
					else if (linetext + 12 <= lineend && !strncmp(linetext, "$loc_name_mh", 12)) {s = "MEGA";linetext += 12;}
					else s = NULL;
					if (s)
					{
						while (len < (int)sizeof(name) - 1 && *s)
							name[len++] = *s++;
						continue;
					}
				}
				name[len++] = *linetext++;
			}
			name[len] = 0;
			// add the point to the list
			VectorScale(mins, (1.0 / 8.0), mins);
			CL_Locs_AddNode(mins, mins, name);
		}
		else
			continue;
	}
}

/*
===========
CL_Shutdown
===========
*/
void CL_Shutdown (void)
{
	CL_Screen_Shutdown();
	CL_Particles_Shutdown();
	CL_Parse_Shutdown();

	Mem_FreePool (&cls.permanentmempool);
	Mem_FreePool (&cls.levelmempool);
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
	cls.levelmempool = Mem_AllocPool("client (per-level memory)", 0, NULL);
	cls.permanentmempool = Mem_AllocPool("client (long term memory)", 0, NULL);

	memset(&r_refdef, 0, sizeof(r_refdef));
	// max entities sent to renderer per frame
	r_refdef.maxentities = MAX_EDICTS + 256 + 512;
	r_refdef.entities = (entity_render_t **)Mem_Alloc(cls.permanentmempool, sizeof(entity_render_t *) * r_refdef.maxentities);

	CL_InitInput ();

//
// register our commands
//
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

	// Support Client-side Model Index List
	Cmd_AddCommand ("cl_modelindexlist", CL_ModelIndexList_f, "list information on all models in the client modelindex");
	// Support Client-side Sound Index List
	Cmd_AddCommand ("cl_soundindexlist", CL_SoundIndexList_f, "list all sounds in the client soundindex");

	Cvar_RegisterVariable (&cl_autodemo);
	Cvar_RegisterVariable (&cl_autodemo_nameformat);

	Cmd_AddCommand ("fog", CL_Fog_f, "set global fog parameters (density red green blue [alpha [mindist maxdist]])");

	// LordHavoc: added pausedemo
	Cmd_AddCommand ("pausedemo", CL_PauseDemo_f, "pause demo playback (can also safely pause demo recording if using QUAKE, QUAKEDP or NEHAHRAMOVIE protocol, useful for making movies)");

	Cmd_AddCommand ("cl_areastats", CL_AreaStats_f, "prints statistics on entity culling during collision traces");

	Cvar_RegisterVariable(&r_draweffects);
	Cvar_RegisterVariable(&cl_explosions_alpha_start);
	Cvar_RegisterVariable(&cl_explosions_alpha_end);
	Cvar_RegisterVariable(&cl_explosions_size_start);
	Cvar_RegisterVariable(&cl_explosions_size_end);
	Cvar_RegisterVariable(&cl_explosions_lifetime);
	Cvar_RegisterVariable(&cl_stainmaps);
	Cvar_RegisterVariable(&cl_stainmaps_clearonload);
	Cvar_RegisterVariable(&cl_beams_polygons);
	Cvar_RegisterVariable(&cl_beams_quakepositionhack);
	Cvar_RegisterVariable(&cl_beams_instantaimhack);
	Cvar_RegisterVariable(&cl_beams_lightatend);
	Cvar_RegisterVariable(&cl_noplayershadow);
	Cvar_RegisterVariable(&cl_dlights_decayradius);
	Cvar_RegisterVariable(&cl_dlights_decaybrightness);

	Cvar_RegisterVariable(&cl_prydoncursor);

	Cvar_RegisterVariable(&cl_deathnoviewmodel);

	// for QW connections
	Cvar_RegisterVariable(&qport);
	Cvar_SetValueQuick(&qport, (rand() * RAND_MAX + rand()) & 0xffff);

	Cmd_AddCommand("timerefresh", CL_TimeRefresh_f, "turn quickly and print rendering statistcs");

	Cvar_RegisterVariable(&cl_locs_enable);
	Cvar_RegisterVariable(&cl_locs_show);
	Cmd_AddCommand("locs_add", CL_Locs_Add_f, "add a point or box location (usage: x y z[ x y z] \"name\", if two sets of xyz are supplied it is a box, otherwise point)");
	Cmd_AddCommand("locs_removenearest", CL_Locs_RemoveNearest_f, "remove the nearest point or box (note: you need to be very near a box to remove it)");
	Cmd_AddCommand("locs_clear", CL_Locs_Clear_f, "remove all loc points/boxes");
	Cmd_AddCommand("locs_reload", CL_Locs_Reload_f, "reload .loc file for this map");
	Cmd_AddCommand("locs_save", CL_Locs_Save_f, "save .loc file for this map containing currently defined points and boxes");

	CL_Parse_Init();
	CL_Particles_Init();
	CL_Screen_Init();

	CL_Video_Init();
#ifdef SUPPORT_GECKO
	CL_Gecko_Init();
#endif
}



