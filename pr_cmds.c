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

#include "quakedef.h"

cvar_t sv_aim = {CVAR_SAVE, "sv_aim", "2"}; //"0.93"}; // LordHavoc: disabled autoaim by default
cvar_t pr_zone_min_strings = {0, "pr_zone_min_strings", "64"};

// LordHavoc: added this to semi-fix the problem of using many ftos calls in a print
#define STRINGTEMP_BUFFERS 16
#define STRINGTEMP_LENGTH 4096
static char pr_string_temp[STRINGTEMP_BUFFERS][STRINGTEMP_LENGTH];
static int pr_string_tempindex = 0;

static char *PR_GetTempString(void)
{
	char *s;
	s = pr_string_temp[pr_string_tempindex];
	pr_string_tempindex = (pr_string_tempindex + 1) % STRINGTEMP_BUFFERS;
	return s;
}

#define	RETURN_EDICT(e) (G_INT(OFS_RETURN) = EDICT_TO_PROG(e))
#define PF_WARNING(s) do{Con_Printf(s);PR_PrintState();return;}while(0)
#define PF_ERROR(s) do{Host_Error(s);return;}while(0)


/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/


void PF_VarString(int first, char *out, int outlength)
{
	int i;
	const char *s;
	char *outend;

	outend = out + outlength - 1;
	for (i = first;i < pr_argc && out < outend;i++)
	{
		s = G_STRING((OFS_PARM0+i*3));
		while (out < outend && *s)
			*out++ = *s++;
	}
	*out++ = 0;
}

char *ENGINE_EXTENSIONS =
"DP_BUTTONCHAT "
"DP_BUTTONUSE "
"DP_CL_LOADSKY "
"DP_CON_SET "
"DP_CON_SETA "
"DP_CON_STARTMAP "
"DP_EF_ADDITIVE "
"DP_EF_BLUE "
"DP_EF_FLAME "
"DP_EF_FULLBRIGHT "
"DP_EF_NODEPTHTEST "
"DP_EF_NODRAW "
"DP_EF_NOSHADOW "
"DP_EF_RED "
"DP_EF_STARDUST "
"DP_ENT_ALPHA "
"DP_ENT_CUSTOMCOLORMAP "
"DP_ENT_EXTERIORMODELTOCLIENT "
"DP_ENT_GLOW "
"DP_ENT_LOWPRECISION "
"DP_ENT_SCALE "
"DP_ENT_VIEWMODEL "
"DP_GFX_EXTERNALTEXTURES "
"DP_GFX_FOG "
"DP_GFX_QUAKE3MODELTAGS "
"DP_GFX_SKINFILES "
"DP_GFX_SKYBOX "
"DP_HALFLIFE_MAP "
"DP_HALFLIFE_MAP_CVAR "
"DP_HALFLIFE_SPRITE "
"DP_INPUTBUTTONS "
"DP_LITSPRITES "
"DP_LITSUPPORT "
"DP_MONSTERWALK "
"DP_MOVETYPEBOUNCEMISSILE "
"DP_MOVETYPEFOLLOW "
"DP_QC_CHANGEPITCH "
"DP_QC_COPYENTITY "
"DP_QC_CVAR_STRING "
"DP_QC_ETOS "
"DP_QC_FINDCHAIN "
"DP_QC_FINDCHAINFLAGS "
"DP_QC_FINDCHAINFLOAT "
"DP_QC_FINDFLAGS "
"DP_QC_FINDFLOAT "
"DP_QC_FS_SEARCH " // Black: same as in the menu qc
"DP_QC_GETLIGHT "
"DP_QC_GETSURFACE "
"DP_QC_GETTAGINFO "
"DP_QC_MINMAXBOUND "
"DP_QC_MULTIPLETEMPSTRINGS "
"DP_QC_RANDOMVEC "
"DP_QC_SINCOSSQRTPOW "
"DP_QC_TRACEBOX "
"DP_QC_TRACETOSS "
"DP_QC_TRACE_MOVETYPE_HITMODEL "
"DP_QC_TRACE_MOVETYPE_WORLDONLY "
"DP_QC_VECTORVECTORS "
"DP_QUAKE2_MODEL "
"DP_QUAKE2_SPRITE "
"DP_QUAKE3_MODEL "
"DP_REGISTERCVAR "
"DP_SND_DIRECTIONLESSATTNNONE "
"DP_SND_FAKETRACKS "
"DP_SND_OGGVORBIS "
"DP_SND_STEREOWAV "
"DP_SOLIDCORPSE "
"DP_SPRITE32 "
"DP_SV_BOTCLIENT "
"DP_SV_CLIENTCOLORS "
"DP_SV_CLIENTNAME "
"DP_SV_DRAWONLYTOCLIENT "
"DP_SV_DROPCLIENT "
"DP_SV_EFFECT "
"DP_SV_NODRAWTOCLIENT "
"DP_SV_PING "
"DP_SV_PLAYERPHYSICS "
"DP_SV_PUNCHVECTOR "
"DP_SV_ROTATINGBMODEL "
"DP_SV_SETCOLOR "
"DP_SV_SLOWMO "
"DP_TE_BLOOD "
"DP_TE_BLOODSHOWER "
"DP_TE_CUSTOMFLASH "
"DP_TE_EXPLOSIONRGB "
"DP_TE_FLAMEJET "
"DP_TE_PARTICLECUBE "
"DP_TE_PARTICLERAIN "
"DP_TE_PARTICLESNOW "
"DP_TE_PLASMABURN "
"DP_TE_QUADEFFECTS1 "
"DP_TE_SMALLFLASH "
"DP_TE_SPARK "
"DP_TE_STANDARDEFFECTBUILTINS "
"DP_VIEWZOOM "
"FRIK_FILE "
"KRIMZON_SV_PARSECLIENTCOMMAND "
"NEH_CMD_PLAY2 "
"NEH_RESTOREGAME "
"NXQ_GFX_LETTERBOX "
"PRYDON_CLIENTCURSOR "
"TENEBRAE_GFX_DLIGHTS "
"TW_SV_STEPCONTROL "
"NEXUIZ_PLAYERMODEL "
"NEXUIZ_PLAYERSKIN "
;

qboolean checkextension(char *name)
{
	int len;
	char *e, *start;
	len = strlen(name);
	for (e = ENGINE_EXTENSIONS;*e;e++)
	{
		while (*e == ' ')
			e++;
		if (!*e)
			break;
		start = e;
		while (*e && *e != ' ')
			e++;
		if (e - start == len)
			if (!strncasecmp(start, name, len))
				return true;
	}
	return false;
}

/*
=================
PF_checkextension

returns true if the extension is supported by the server

checkextension(extensionname)
=================
*/
void PF_checkextension (void)
{
	G_FLOAT(OFS_RETURN) = checkextension(G_STRING(OFS_PARM0));
}

/*
=================
PF_error

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_error (void)
{
	edict_t	*ed;
	char string[STRINGTEMP_LENGTH];

	PF_VarString(0, string, sizeof(string));
	Con_Printf("======SERVER ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name), string);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print(ed);

	PF_ERROR("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror (void)
{
	edict_t	*ed;
	char string[STRINGTEMP_LENGTH];

	PF_VarString(0, string, sizeof(string));
	Con_Printf("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name), string);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print(ed);
	ED_Free (ed);
}


/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
	AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
==============
PF_vectorvectors

Writes new values for v_forward, v_up, and v_right based on the given forward vector
vectorvectors(vector, vector)
==============
*/
void PF_vectorvectors (void)
{
	VectorNormalize2(G_VECTOR(OFS_PARM0), pr_global_struct->v_forward);
	VectorVectors(pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin (void)
{
	edict_t	*e;
	float	*org;

	e = G_EDICT(OFS_PARM0);
	if (e == sv.edicts)
		PF_WARNING("setorigin: can not modify world entity\n");
	if (e->e->free)
		PF_WARNING("setorigin: can not modify free entity\n");
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v->origin);
	SV_LinkEdict (e, false);
}


void SetMinMaxSize (edict_t *e, float *min, float *max, qboolean rotate)
{
	int		i;

	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PF_ERROR("SetMinMaxSize: backwards mins/maxs\n");

// set derived values
	VectorCopy (min, e->v->mins);
	VectorCopy (max, e->v->maxs);
	VectorSubtract (max, min, e->v->size);

	SV_LinkEdict (e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle
LordHavoc: no it isn't...

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (void)
{
	edict_t	*e;
	float	*min, *max;

	e = G_EDICT(OFS_PARM0);
	if (e == sv.edicts)
		PF_WARNING("setsize: can not modify world entity\n");
	if (e->e->free)
		PF_WARNING("setsize: can not modify free entity\n");
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	SetMinMaxSize (e, min, max, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
static vec3_t quakemins = {-16, -16, -16}, quakemaxs = {16, 16, 16};
void PF_setmodel (void)
{
	edict_t	*e;
	model_t	*mod;
	int		i;

	e = G_EDICT(OFS_PARM0);
	if (e == sv.edicts)
		PF_WARNING("setmodel: can not modify world entity\n");
	if (e->e->free)
		PF_WARNING("setmodel: can not modify free entity\n");
	i = SV_ModelIndex(G_STRING(OFS_PARM1), 1);
	e->v->model = PR_SetString(sv.model_precache[i]);
	e->v->modelindex = i;

	mod = sv.models[i];

	if (mod)
	{
		if (mod->type != mod_alias || sv_gameplayfix_setmodelrealbox.integer)
			SetMinMaxSize (e, mod->normalmins, mod->normalmaxs, true);
		else
			SetMinMaxSize (e, quakemins, quakemaxs, true);
	}
	else
		SetMinMaxSize (e, vec3_origin, vec3_origin, true);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
void PF_bprint (void)
{
	char string[STRINGTEMP_LENGTH];
	PF_VarString(0, string, sizeof(string));
	SV_BroadcastPrint(string);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
	client_t	*client;
	int			entnum;
	char string[STRINGTEMP_LENGTH];

	entnum = G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to sprint to a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	PF_VarString(1, string, sizeof(string));
	MSG_WriteChar(&client->message,svc_print);
	MSG_WriteString(&client->message, string);
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint (void)
{
	client_t	*client;
	int			entnum;
	char string[STRINGTEMP_LENGTH];

	entnum = G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to sprint to a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	PF_VarString(1, string, sizeof(string));
	MSG_WriteChar(&client->message,svc_centerprint);
	MSG_WriteString(&client->message, string);
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	float	new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);

	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new = 1/new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}

	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen (void)
{
	float	*value1;
	float	new;

	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);

	G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw (void)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles (void)
{
	double value1[3], forward, yaw, pitch;

	VectorCopy(G_VECTOR(OFS_PARM0), value1);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		// LordHavoc: optimized a bit
		if (value1[0])
		{
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;
		}
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		forward = sqrt(value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	VectorSet(G_VECTOR(OFS_RETURN), pitch, yaw, 0);
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random (void)
{
	G_FLOAT(OFS_RETURN) = lhrandom(0, 1);
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle (void)
{
	float		*org, *dir;
	float		color;
	float		count;

	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (void)
{
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			soundnum, large;

	pos = G_VECTOR (OFS_PARM0);
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

// check to see if samp was properly precached
	soundnum = SV_SoundIndex(samp, 1);
	if (!soundnum)
		return;

	large = false;
	if (soundnum >= 256)
		large = true;

	// add an svc_spawnambient command to the level signon packet

	if (large)
		MSG_WriteByte (&sv.signon, svc_spawnstaticsound2);
	else
		MSG_WriteByte (&sv.signon, svc_spawnstaticsound);

	MSG_WriteVector(&sv.signon, pos, sv.protocol);

	if (large)
		MSG_WriteShort (&sv.signon, soundnum);
	else
		MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (void)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 255)
		PF_WARNING("SV_StartSound: volume must be in range 0-1\n");

	if (attenuation < 0 || attenuation > 4)
		PF_WARNING("SV_StartSound: attenuation must be in range 0-4\n");

	if (channel < 0 || channel > 7)
		PF_WARNING("SV_StartSound: channel must be in range 0-7\n");

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
void PF_break (void)
{
	PF_ERROR("break: break statement\n");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		move;
	edict_t	*ent;

	pr_xfunction->builtinsprofile += 30;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	move = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, move, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
	// FIXME: add trace_endcontents
}


/*
=================
PF_tracebox

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

tracebox (vector1, vector mins, vector maxs, vector2, tryents)
=================
*/
// LordHavoc: added this for my own use, VERY useful, similar to traceline
void PF_tracebox (void)
{
	float	*v1, *v2, *m1, *m2;
	trace_t	trace;
	int		move;
	edict_t	*ent;

	pr_xfunction->builtinsprofile += 30;

	v1 = G_VECTOR(OFS_PARM0);
	m1 = G_VECTOR(OFS_PARM1);
	m2 = G_VECTOR(OFS_PARM2);
	v2 = G_VECTOR(OFS_PARM3);
	move = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(OFS_PARM5);

	trace = SV_Move (v1, m1, m2, v2, move, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);
void PF_TraceToss (void)
{
	trace_t	trace;
	edict_t	*ent;
	edict_t	*ignore;

	pr_xfunction->builtinsprofile += 600;

	ent = G_EDICT(OFS_PARM0);
	if (ent == sv.edicts)
		PF_WARNING("tracetoss: can not use world entity\n");
	ignore = G_EDICT(OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}


/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos (void)
{
}

//============================================================================

int checkpvsbytes;
qbyte checkpvs[MAX_MAP_LEAFS/8];

int PF_newcheckclient (int check)
{
	int		i;
	edict_t	*ent;
	vec3_t	org;

// cycle to the next one

	check = bound(1, check, svs.maxclients);
	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		// count the cost
		pr_xfunction->builtinsprofile++;
		// wrap around
		if (i == svs.maxclients+1)
			i = 1;
		// look up the client's edict
		ent = EDICT_NUM(i);
		// check if it is to be ignored, but never ignore the one we started on (prevent infinite loop)
		if (i != check && (ent->e->free || ent->v->health <= 0 || ((int)ent->v->flags & FL_NOTARGET)))
			continue;
		// found a valid client (possibly the same one again)
		break;
	}

// get the PVS for the entity
	VectorAdd(ent->v->origin, ent->v->view_ofs, org);
	checkpvsbytes = 0;
	if (sv.worldmodel && sv.worldmodel->brush.FatPVS)
		checkpvsbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, org, 0, checkpvs, sizeof(checkpvs));

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there is more than one valid option, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
int c_invis, c_notvis;
void PF_checkclient (void)
{
	edict_t	*ent, *self;
	vec3_t	view;

	// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	// return check if it might be visible
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->e->free || ent->v->health <= 0)
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

	// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd(self->v->origin, self->v->view_ofs, view);
	if (sv.worldmodel && checkpvsbytes && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, checkpvs, view, view))
	{
		c_notvis++;
		RETURN_EDICT(sv.edicts);
		return;
	}

	// might be able to see it
	c_invis++;
	RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (void)
{
	int		entnum;
	char	*str;
	client_t	*old;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("Can't stuffcmd to a non-client\n");
		return;
	}
	str = G_STRING(OFS_PARM1);

	old = host_client;
	host_client = svs.clients + entnum-1;
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
=================
PF_localcmd

Sends text to server console

localcmd (string)
=================
*/
void PF_localcmd (void)
{
	Cbuf_AddText(G_STRING(OFS_PARM0));
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (void)
{
	G_FLOAT(OFS_RETURN) = Cvar_VariableValue(G_STRING(OFS_PARM0));
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set (void)
{
	Cvar_Set(G_STRING(OFS_PARM0), G_STRING(OFS_PARM1));
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius (void)
{
	edict_t *ent, *chain;
	vec_t radius, radius2;
	vec3_t org, eorg, mins, maxs;
	int i;
	int numtouchedicts;
	edict_t *touchedicts[MAX_EDICTS];

	chain = (edict_t *)sv.edicts;

	VectorCopy(G_VECTOR(OFS_PARM0), org);
	radius = G_FLOAT(OFS_PARM1);
	radius2 = radius * radius;

	mins[0] = org[0] - radius;
	mins[1] = org[1] - radius;
	mins[2] = org[2] - radius;
	maxs[0] = org[0] + radius;
	maxs[1] = org[1] + radius;
	maxs[2] = org[2] + radius;
	numtouchedicts = SV_EntitiesInBox(mins, maxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		ent = touchedicts[i];
		pr_xfunction->builtinsprofile++;
		// LordHavoc: compare against bounding box rather than center so it
		// doesn't miss large objects, and use DotProduct instead of Length
		// for a major speedup
		eorg[0] = (org[0] - ent->v->origin[0]) - bound(ent->v->mins[0], (org[0] - ent->v->origin[0]), ent->v->maxs[0]);
		eorg[1] = (org[1] - ent->v->origin[1]) - bound(ent->v->mins[1], (org[1] - ent->v->origin[1]), ent->v->maxs[1]);
		eorg[2] = (org[2] - ent->v->origin[2]) - bound(ent->v->mins[2], (org[2] - ent->v->origin[2]), ent->v->maxs[2]);
		if (DotProduct(eorg, eorg) < radius2)
		{
			ent->v->chain = EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	RETURN_EDICT(chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (void)
{
	char string[STRINGTEMP_LENGTH];
	if (developer.integer)
	{
		PF_VarString(0, string, sizeof(string));
		Con_Print(string);
	}
}

void PF_ftos (void)
{
	float v;
	char *s;
	v = G_FLOAT(OFS_PARM0);

	s = PR_GetTempString();
	if ((float)((int)v) == v)
		sprintf(s, "%i", (int)v);
	else
		sprintf(s, "%f", v);
	G_INT(OFS_RETURN) = PR_SetString(s);
}

void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

void PF_vtos (void)
{
	char *s;
	s = PR_GetTempString();
	sprintf (s, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = PR_SetString(s);
}

void PF_etos (void)
{
	char *s;
	s = PR_GetTempString();
	sprintf (s, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = PR_SetString(s);
}

void PF_Spawn (void)
{
	edict_t	*ed;
	pr_xfunction->builtinsprofile += 20;
	ed = ED_Alloc();
	RETURN_EDICT(ed);
}

void PF_Remove (void)
{
	edict_t	*ed;
	pr_xfunction->builtinsprofile += 20;

	ed = G_EDICT(OFS_PARM0);
	if (ed == sv.edicts)
		PF_WARNING("remove: tried to remove world\n");
	if (NUM_FOR_EDICT(ed) <= svs.maxclients)
		PF_WARNING("remove: tried to remove a client\n");
	// LordHavoc: not an error because id1 progs did this in some cases (killtarget removes entities, even if they are already removed in some cases...)
	if (ed->e->free && developer.integer)
		PF_WARNING("remove: tried to remove an entity that was already removed\n");
	ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
void PF_Find (void)
{
	int		e;
	int		f;
	char	*s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s || !s[0])
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

	for (e++ ; e < sv.num_edicts ; e++)
	{
		pr_xfunction->builtinsprofile++;
		ed = EDICT_NUM(e);
		if (ed->e->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

// LordHavoc: added this for searching float, int, and entity reference fields
void PF_FindFloat (void)
{
	int		e;
	int		f;
	float	s;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_FLOAT(OFS_PARM2);

	for (e++ ; e < sv.num_edicts ; e++)
	{
		pr_xfunction->builtinsprofile++;
		ed = EDICT_NUM(e);
		if (ed->e->free)
			continue;
		if (E_FLOAT(ed,f) == s)
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

// chained search for strings in entity fields
// entity(.string field, string match) findchain = #402;
void PF_findchain (void)
{
	int		i;
	int		f;
	char	*s, *t;
	edict_t	*ent, *chain;

	chain = (edict_t *)sv.edicts;

	f = G_INT(OFS_PARM0);
	s = G_STRING(OFS_PARM1);
	if (!s || !s[0])
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

	ent = NEXT_EDICT(sv.edicts);
	for (i = 1;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
	{
		pr_xfunction->builtinsprofile++;
		if (ent->e->free)
			continue;
		t = E_STRING(ent,f);
		if (!t)
			continue;
		if (strcmp(t,s))
			continue;

		ent->v->chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

// LordHavoc: chained search for float, int, and entity reference fields
// entity(.string field, float match) findchainfloat = #403;
void PF_findchainfloat (void)
{
	int		i;
	int		f;
	float	s;
	edict_t	*ent, *chain;

	chain = (edict_t *)sv.edicts;

	f = G_INT(OFS_PARM0);
	s = G_FLOAT(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i = 1;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
	{
		pr_xfunction->builtinsprofile++;
		if (ent->e->free)
			continue;
		if (E_FLOAT(ent,f) != s)
			continue;

		ent->v->chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

// LordHavoc: search for flags in float fields
void PF_findflags (void)
{
	int		e;
	int		f;
	int		s;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = (int)G_FLOAT(OFS_PARM2);

	for (e++ ; e < sv.num_edicts ; e++)
	{
		pr_xfunction->builtinsprofile++;
		ed = EDICT_NUM(e);
		if (ed->e->free)
			continue;
		if ((int)E_FLOAT(ed,f) & s)
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

// LordHavoc: chained search for flags in float fields
void PF_findchainflags (void)
{
	int		i;
	int		f;
	int		s;
	edict_t	*ent, *chain;

	chain = (edict_t *)sv.edicts;

	f = G_INT(OFS_PARM0);
	s = (int)G_FLOAT(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i = 1;i < sv.num_edicts;i++, ent = NEXT_EDICT(ent))
	{
		pr_xfunction->builtinsprofile++;
		if (ent->e->free)
			continue;
		if (!((int)E_FLOAT(ent,f) & s))
			continue;

		ent->v->chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

void PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PF_ERROR("Bad string");
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}


void PF_precache_sound (void)
{
	SV_SoundIndex(G_STRING(OFS_PARM0), 2);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_model (void)
{
	SV_ModelIndex(G_STRING(OFS_PARM0), 2);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}


void PF_coredump (void)
{
	ED_PrintEdicts ();
}

void PF_traceon (void)
{
	pr_trace = true;
}

void PF_traceoff (void)
{
	pr_trace = false;
}

void PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	mfunction_t	*oldf;
	int 	oldself;

	// assume failure if it returns early
	G_FLOAT(OFS_RETURN) = 0;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	if (ent == sv.edicts)
		PF_WARNING("walkmove: can not modify world entity\n");
	if (ent->e->free)
		PF_WARNING("walkmove: can not modify free entity\n");
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
		return;

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);


// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (void)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	// assume failure if it returns early
	G_FLOAT(OFS_RETURN) = 0;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	if (ent == sv.edicts)
		PF_WARNING("droptofloor: can not modify world entity\n");
	if (ent->e->free)
		PF_WARNING("droptofloor: can not modify free entity\n");

	VectorCopy (ent->v->origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->v->origin, ent->v->mins, ent->v->maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction != 1)
	{
		VectorCopy (trace.endpos, ent->v->origin);
		SV_LinkEdict (ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
		// if support is destroyed, keep suspended (gross hack for floating items in various maps)
		ent->e->suspendedinairflag = true;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (void)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;

	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

// change the string in sv
	sv.lightstyles[style] = val;

// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
	{
		if (client->active)
		{
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message,style);
			MSG_WriteString (&client->message, val);
		}
	}
}

void PF_rint (void)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}
void PF_floor (void)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}
void PF_ceil (void)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
	G_FLOAT(OFS_RETURN) = SV_CheckBottom (G_EDICT(OFS_PARM0));
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
	G_FLOAT(OFS_RETURN) = SV_PointQ1Contents(G_VECTOR(OFS_PARM0));
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent (void)
{
	int		i;
	edict_t	*ent;

	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		pr_xfunction->builtinsprofile++;
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->e->free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
void PF_aim (void)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;

	// assume failure if it returns early
	VectorCopy(pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
	// if sv_aim is so high it can't possibly accept anything, skip out early
	if (sv_aim.value >= 1)
		return;

	ent = G_EDICT(OFS_PARM0);
	if (ent == sv.edicts)
		PF_WARNING("aim: can not use world entity\n");
	if (ent->e->free)
		PF_WARNING("aim: can not use free entity\n");
	speed = G_FLOAT(OFS_PARM1);

	VectorCopy (ent->v->origin, start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
	if (tr.ent && ((edict_t *)tr.ent)->v->takedamage == DAMAGE_AIM
	&& (!teamplay.integer || ent->v->team <=0 || ent->v->team != ((edict_t *)tr.ent)->v->team) )
	{
		VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
		return;
	}


// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;

	check = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, check = NEXT_EDICT(check) )
	{
		pr_xfunction->builtinsprofile++;
		if (check->v->takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.integer && ent->v->team > 0 && ent->v->team == check->v->team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v->origin[j]
			+ 0.5*(check->v->mins[j] + check->v->maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract (bestent->v->origin, ent->v->origin, dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		VectorScale (pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	if (ent == sv.edicts)
		PF_WARNING("changeyaw: can not modify world entity\n");
	if (ent->e->free)
		PF_WARNING("changeyaw: can not modify free entity\n");
	current = ANGLEMOD(ent->v->angles[1]);
	ideal = ent->v->ideal_yaw;
	speed = ent->v->yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v->angles[1] = ANGLEMOD (current + move);
}

/*
==============
PF_changepitch
==============
*/
void PF_changepitch (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	eval_t		*val;

	ent = G_EDICT(OFS_PARM0);
	if (ent == sv.edicts)
		PF_WARNING("changepitch: can not modify world entity\n");
	if (ent->e->free)
		PF_WARNING("changepitch: can not modify free entity\n");
	current = ANGLEMOD( ent->v->angles[0] );
	if ((val = GETEDICTFIELDVALUE(ent, eval_idealpitch)))
		ideal = val->_float;
	else
	{
		PF_WARNING("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch\n");
		return;
	}
	if ((val = GETEDICTFIELDVALUE(ent, eval_pitch_speed)))
		speed = val->_float;
	else
	{
		PF_WARNING("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch\n");
		return;
	}

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v->angles[0] = ANGLEMOD (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *WriteDest (void)
{
	int		entnum;
	int		dest;
	edict_t	*ent;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
			Host_Error("WriteDest: tried to write to non-client\n");
		return &svs.clients[entnum-1].message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return &sv.signon;

	default:
		Host_Error("WriteDest: bad destination");
		break;
	}

	return NULL;
}

void PF_WriteByte (void)
{
	MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
	MSG_WriteChar (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
	MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
	MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
	MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1), sv.protocol);
}

void PF_WriteCoord (void)
{
	MSG_WriteCoord (WriteDest(), G_FLOAT(OFS_PARM1), sv.protocol);
}

void PF_WriteString (void)
{
	MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
	MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
}

//=============================================================================

void PF_makestatic (void)
{
	edict_t *ent;
	int i, large;

	ent = G_EDICT(OFS_PARM0);
	if (ent == sv.edicts)
		PF_WARNING("makestatic: can not modify world entity\n");
	if (ent->e->free)
		PF_WARNING("makestatic: can not modify free entity\n");

	large = false;
	if (ent->v->modelindex >= 256 || ent->v->frame >= 256)
		large = true;

	if (large)
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic2);
		MSG_WriteShort (&sv.signon, ent->v->modelindex);
		MSG_WriteShort (&sv.signon, ent->v->frame);
	}
	else
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic);
		MSG_WriteByte (&sv.signon, ent->v->modelindex);
		MSG_WriteByte (&sv.signon, ent->v->frame);
	}

	MSG_WriteByte (&sv.signon, ent->v->colormap);
	MSG_WriteByte (&sv.signon, ent->v->skin);
	for (i=0 ; i<3 ; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->v->origin[i], sv.protocol);
		MSG_WriteAngle(&sv.signon, ent->v->angles[i], sv.protocol);
	}

// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients || !svs.clients[i-1].active)
	{
		Con_Print("tried to setspawnparms on a non-client\n");
		return;
	}

	// copy spawn parms out of the client_t
	client = svs.clients + i-1;
	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (void)
{
	char	*s;

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
}

void PF_sin (void)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}

void PF_cos (void)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}

void PF_sqrt (void)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}

/*
=================
PF_RandomVec

Returns a vector of length < 1

randomvec()
=================
*/
void PF_randomvec (void)
{
	vec3_t		temp;
	do
	{
		temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	}
	while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_GetLight

Returns a color vector indicating the lighting at the requested point.

(Internal Operation note: actually measures the light beneath the point, just like
                          the model lighting on the client)

getlight(vector)
=================
*/
void PF_GetLight (void)
{
	vec3_t ambientcolor, diffusecolor, diffusenormal;
	vec_t *p;
	p = G_VECTOR(OFS_PARM0);
	VectorClear(ambientcolor);
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);
	if (sv.worldmodel && sv.worldmodel->brush.LightPoint)
		sv.worldmodel->brush.LightPoint(sv.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	VectorMA(ambientcolor, 0.5, diffusecolor, G_VECTOR(OFS_RETURN));
}

void PF_registercvar (void)
{
	char *name, *value;
	name = G_STRING(OFS_PARM0);
	value = G_STRING(OFS_PARM1);
	G_FLOAT(OFS_RETURN) = 0;

// first check to see if it has already been defined
	if (Cvar_FindVar (name))
		return;

// check for overlap with a command
	if (Cmd_Exists (name))
	{
		Con_Printf("PF_registercvar: %s is a command\n", name);
		return;
	}

	Cvar_Get(name, value, 0);

	G_FLOAT(OFS_RETURN) = 1; // success
}

/*
=================
PF_min

returns the minimum of two supplied floats

min(a, b)
=================
*/
void PF_min (void)
{
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		int i;
		float f = G_FLOAT(OFS_PARM0);
		for (i = 1;i < pr_argc;i++)
			if (G_FLOAT((OFS_PARM0+i*3)) < f)
				f = G_FLOAT((OFS_PARM0+i*3));
		G_FLOAT(OFS_RETURN) = f;
	}
	else
	{
		G_FLOAT(OFS_RETURN) = 0;
		PF_WARNING("min: must supply at least 2 floats\n");
	}
}

/*
=================
PF_max

returns the maximum of two supplied floats

max(a, b)
=================
*/
void PF_max (void)
{
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		int i;
		float f = G_FLOAT(OFS_PARM0);
		for (i = 1;i < pr_argc;i++)
			if (G_FLOAT((OFS_PARM0+i*3)) > f)
				f = G_FLOAT((OFS_PARM0+i*3));
		G_FLOAT(OFS_RETURN) = f;
	}
	else
	{
		G_FLOAT(OFS_RETURN) = 0;
		PF_WARNING("max: must supply at least 2 floats\n");
	}
}

/*
=================
PF_bound

returns number bounded by supplied range

min(min, value, max)
=================
*/
void PF_bound (void)
{
	G_FLOAT(OFS_RETURN) = bound(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

/*
=================
PF_pow

returns a raised to power b

pow(a, b)
=================
*/
void PF_pow (void)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

/*
=================
PF_copyentity

copies data from one entity to another

copyentity(src, dst)
=================
*/
void PF_copyentity (void)
{
	edict_t *in, *out;
	in = G_EDICT(OFS_PARM0);
	if (in == sv.edicts)
		PF_WARNING("copyentity: can not read world entity\n");
	if (in->e->free)
		PF_WARNING("copyentity: can not read free entity\n");
	out = G_EDICT(OFS_PARM1);
	if (out == sv.edicts)
		PF_WARNING("copyentity: can not modify world entity\n");
	if (out->e->free)
		PF_WARNING("copyentity: can not modify free entity\n");
	memcpy(out->v, in->v, progs->entityfields * 4);
}

/*
=================
PF_setcolor

sets the color of a client and broadcasts the update to all connected clients

setcolor(clientent, value)
=================
*/
void PF_setcolor (void)
{
	client_t *client;
	int entnum, i;
	eval_t *val;

	entnum = G_EDICTNUM(OFS_PARM0);
	i = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to setcolor a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	if (client->edict)
	{
		if ((val = GETEDICTFIELDVALUE(client->edict, eval_clientcolors)))
			val->_float = i;
		client->edict->v->team = (i & 15) + 1;
	}
	client->colors = i;
	if (client->old_colors != client->colors)
	{
		client->old_colors = client->colors;
		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
		MSG_WriteByte (&sv.reliable_datagram, client - svs.clients);
		MSG_WriteByte (&sv.reliable_datagram, client->colors);
	}
}

/*
=================
PF_effect

effect(origin, modelname, startframe, framecount, framerate)
=================
*/
void PF_effect (void)
{
	int i;
	char *s;
	s = G_STRING(OFS_PARM1);
	if (!s || !s[0])
		PF_WARNING("effect: no model specified\n");

	i = SV_ModelIndex(s, 1);
	if (!i)
		PF_WARNING("effect: model not precached\n");
	SV_StartEffect(G_VECTOR(OFS_PARM0), i, G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4));
}

void PF_te_blood (void)
{
	if (G_FLOAT(OFS_PARM2) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BLOOD);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// velocity
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.datagram, bound(0, (int) G_FLOAT(OFS_PARM2), 255));
}

void PF_te_bloodshower (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BLOODSHOWER);
	// min
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// speed
	MSG_WriteCoord(&sv.datagram, G_FLOAT(OFS_PARM2), sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
}

void PF_te_explosionrgb (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSIONRGB);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// color
	MSG_WriteByte(&sv.datagram, bound(0, (int) (G_VECTOR(OFS_PARM1)[0] * 255), 255));
	MSG_WriteByte(&sv.datagram, bound(0, (int) (G_VECTOR(OFS_PARM1)[1] * 255), 255));
	MSG_WriteByte(&sv.datagram, bound(0, (int) (G_VECTOR(OFS_PARM1)[2] * 255), 255));
}

void PF_te_particlecube (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLECUBE);
	// min
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// velocity
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM4));
	// gravity true/false
	MSG_WriteByte(&sv.datagram, ((int) G_FLOAT(OFS_PARM5)) != 0);
	// randomvel
	MSG_WriteCoord(&sv.datagram, G_FLOAT(OFS_PARM6), sv.protocol);
}

void PF_te_particlerain (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLERAIN);
	// min
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// velocity
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM4));
}

void PF_te_particlesnow (void)
{
	if (G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLESNOW);
	// min
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// velocity
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM4));
}

void PF_te_spark (void)
{
	if (G_FLOAT(OFS_PARM2) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPARK);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// velocity
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.datagram, bound(0, (int) G_FLOAT(OFS_PARM2), 255));
}

void PF_te_gunshotquad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_GUNSHOTQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_spikequad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPIKEQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_superspikequad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SUPERSPIKEQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_explosionquad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSIONQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_smallflash (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SMALLFLASH);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_customflash (void)
{
	if (G_FLOAT(OFS_PARM1) < 8 || G_FLOAT(OFS_PARM2) < (1.0 / 256.0))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_CUSTOMFLASH);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// radius
	MSG_WriteByte(&sv.datagram, bound(0, G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.datagram, bound(0, G_FLOAT(OFS_PARM2) * 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.datagram, bound(0, G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.datagram, bound(0, G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.datagram, bound(0, G_VECTOR(OFS_PARM3)[2] * 255, 255));
}

void PF_te_gunshot (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_GUNSHOT);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_spike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_superspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SUPERSPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_explosion (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_tarexplosion (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_TAREXPLOSION);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_wizspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_WIZSPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_knightspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_KNIGHTSPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_lavasplash (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LAVASPLASH);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_teleport (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_TELEPORT);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_explosion2 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION2);
	// origin
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// color
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM1));
	MSG_WriteByte(&sv.datagram, G_FLOAT(OFS_PARM2));
}

void PF_te_lightning1 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING1);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_lightning2 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING2);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_lightning3 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING3);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_beam (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BEAM);
	// owner entity
	MSG_WriteShort(&sv.datagram, G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_plasmaburn (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PLASMABURN);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

static void clippointtosurface(msurface_t *surface, vec3_t p, vec3_t out)
{
	int i, j, k;
	float *v[3], facenormal[3], edgenormal[3], sidenormal[3], temp[3], offsetdist, dist, bestdist;
	const int *e;
	bestdist = 1000000000;
	VectorCopy(p, out);
	for (i = 0, e = (surface->groupmesh->data_element3i + 3 * surface->num_firsttriangle);i < surface->num_triangles;i++, e += 3)
	{
		// clip original point to each triangle of the surface and find the
		// triangle that is closest
		v[0] = surface->groupmesh->data_vertex3f + e[0] * 3;
		v[1] = surface->groupmesh->data_vertex3f + e[1] * 3;
		v[2] = surface->groupmesh->data_vertex3f + e[2] * 3;
		TriangleNormal(v[0], v[1], v[2], facenormal);
		VectorNormalize(facenormal);
		offsetdist = DotProduct(v[0], facenormal) - DotProduct(p, facenormal);
		VectorMA(p, offsetdist, facenormal, temp);
		for (j = 0, k = 2;j < 3;k = j, j++)
		{
			VectorSubtract(v[k], v[j], edgenormal);
			CrossProduct(edgenormal, facenormal, sidenormal);
			VectorNormalize(sidenormal);
			offsetdist = DotProduct(v[k], sidenormal) - DotProduct(temp, sidenormal);
			if (offsetdist < 0)
				VectorMA(temp, offsetdist, sidenormal, temp);
		}
		dist = VectorDistance2(temp, p);
		if (bestdist > dist)
		{
			bestdist = dist;
			VectorCopy(temp, out);
		}
	}
}

static msurface_t *getsurface(edict_t *ed, int surfacenum)
{
	int modelindex;
	model_t *model;
	if (!ed || ed->e->free)
		return NULL;
	modelindex = ed->v->modelindex;
	if (modelindex < 1 || modelindex >= MAX_MODELS)
		return NULL;
	model = sv.models[modelindex];
	if (surfacenum < 0 || surfacenum >= model->nummodelsurfaces)
		return NULL;
	return model->brush.data_surfaces + surfacenum + model->firstmodelsurface;
}


//PF_getsurfacenumpoints, // #434 float(entity e, float s) getsurfacenumpoints = #434;
void PF_getsurfacenumpoints(void)
{
	msurface_t *surface;
	// return 0 if no such surface
	if (!(surface = getsurface(G_EDICT(OFS_PARM0), G_FLOAT(OFS_PARM1))))
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// note: this (incorrectly) assumes it is a simple polygon
	G_FLOAT(OFS_RETURN) = surface->num_vertices;
}
//PF_getsurfacepoint,     // #435 vector(entity e, float s, float n) getsurfacepoint = #435;
void PF_getsurfacepoint(void)
{
	edict_t *ed;
	msurface_t *surface;
	int pointnum;
	VectorClear(G_VECTOR(OFS_RETURN));
	ed = G_EDICT(OFS_PARM0);
	if (!ed || ed->e->free)
		return;
	if (!(surface = getsurface(ed, G_FLOAT(OFS_PARM1))))
		return;
	// note: this (incorrectly) assumes it is a simple polygon
	pointnum = G_FLOAT(OFS_PARM2);
	if (pointnum < 0 || pointnum >= surface->num_vertices)
		return;
	// FIXME: implement rotation/scaling
	VectorAdd(&(surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex)[pointnum * 3], ed->v->origin, G_VECTOR(OFS_RETURN));
}
//PF_getsurfacenormal,    // #436 vector(entity e, float s) getsurfacenormal = #436;
void PF_getsurfacenormal(void)
{
	msurface_t *surface;
	vec3_t normal;
	VectorClear(G_VECTOR(OFS_RETURN));
	if (!(surface = getsurface(G_EDICT(OFS_PARM0), G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	// note: this (incorrectly) assumes it is a simple polygon
	// note: this only returns the first triangle, so it doesn't work very
	// well for curved surfaces or arbitrary meshes
	TriangleNormal((surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex), (surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex) + 3, (surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex) + 6, normal);
	VectorNormalize(normal);
	VectorCopy(normal, G_VECTOR(OFS_RETURN));
}
//PF_getsurfacetexture,   // #437 string(entity e, float s) getsurfacetexture = #437;
void PF_getsurfacetexture(void)
{
	msurface_t *surface;
	G_INT(OFS_RETURN) = 0;
	if (!(surface = getsurface(G_EDICT(OFS_PARM0), G_FLOAT(OFS_PARM1))))
		return;
	G_INT(OFS_RETURN) = PR_SetString(surface->texture->name);
}
//PF_getsurfacenearpoint, // #438 float(entity e, vector p) getsurfacenearpoint = #438;
void PF_getsurfacenearpoint(void)
{
	int surfacenum, best, modelindex;
	vec3_t clipped, p;
	vec_t dist, bestdist;
	edict_t *ed;
	model_t *model;
	msurface_t *surface;
	vec_t *point;
	G_FLOAT(OFS_RETURN) = -1;
	ed = G_EDICT(OFS_PARM0);
	point = G_VECTOR(OFS_PARM1);

	if (!ed || ed->e->free)
		return;
	modelindex = ed->v->modelindex;
	if (modelindex < 1 || modelindex >= MAX_MODELS)
		return;
	model = sv.models[modelindex];
	if (!model->brush.num_surfaces)
		return;

	// FIXME: implement rotation/scaling
	VectorSubtract(point, ed->v->origin, p);
	best = -1;
	bestdist = 1000000000;
	for (surfacenum = 0;surfacenum < model->nummodelsurfaces;surfacenum++)
	{
		surface = model->brush.data_surfaces + surfacenum + model->firstmodelsurface;
		// first see if the nearest point on the surface's box is closer than the previous match
		clipped[0] = bound(surface->mins[0], p[0], surface->maxs[0]) - p[0];
		clipped[1] = bound(surface->mins[1], p[1], surface->maxs[1]) - p[1];
		clipped[2] = bound(surface->mins[2], p[2], surface->maxs[2]) - p[2];
		dist = VectorLength2(clipped);
		if (dist < bestdist)
		{
			// it is, check the nearest point on the actual geometry
			clippointtosurface(surface, p, clipped);
			VectorSubtract(clipped, p, clipped);
			dist += VectorLength2(clipped);
			if (dist < bestdist)
			{
				// that's closer too, store it as the best match
				best = surfacenum;
				bestdist = dist;
			}
		}
	}
	G_FLOAT(OFS_RETURN) = best;
}
//PF_getsurfaceclippedpoint, // #439 vector(entity e, float s, vector p) getsurfaceclippedpoint = #439;
void PF_getsurfaceclippedpoint(void)
{
	edict_t *ed;
	msurface_t *surface;
	vec3_t p, out;
	VectorClear(G_VECTOR(OFS_RETURN));
	ed = G_EDICT(OFS_PARM0);
	if (!ed || ed->e->free)
		return;
	if (!(surface = getsurface(ed, G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	VectorSubtract(G_VECTOR(OFS_PARM2), ed->v->origin, p);
	clippointtosurface(surface, p, out);
	// FIXME: implement rotation/scaling
	VectorAdd(out, ed->v->origin, G_VECTOR(OFS_RETURN));
}

#define MAX_PRFILES 256

qfile_t *pr_files[MAX_PRFILES];

void PR_Files_Init(void)
{
	memset(pr_files, 0, sizeof(pr_files));
}

void PR_Files_CloseAll(void)
{
	int i;
	for (i = 0;i < MAX_PRFILES;i++)
	{
		if (pr_files[i])
			FS_Close(pr_files[i]);
		pr_files[i] = NULL;
	}
}

//float(string s) stof = #81; // get numerical value from a string
void PF_stof(void)
{
	char string[STRINGTEMP_LENGTH];
	PF_VarString(0, string, sizeof(string));
	G_FLOAT(OFS_RETURN) = atof(string);
}

//float(string filename, float mode) fopen = #110; // opens a file inside quake/gamedir/data/ (mode is FILE_READ, FILE_APPEND, or FILE_WRITE), returns fhandle >= 0 if successful, or fhandle < 0 if unable to open file for any reason
void PF_fopen(void)
{
	int filenum, mode;
	char *modestring, *filename;
	for (filenum = 0;filenum < MAX_PRFILES;filenum++)
		if (pr_files[filenum] == NULL)
			break;
	if (filenum >= MAX_PRFILES)
	{
		Con_Printf("PF_fopen: ran out of file handles (%i)\n", MAX_PRFILES);
		G_FLOAT(OFS_RETURN) = -2;
		return;
	}
	mode = G_FLOAT(OFS_PARM1);
	switch(mode)
	{
	case 0: // FILE_READ
		modestring = "rb";
		break;
	case 1: // FILE_APPEND
		modestring = "ab";
		break;
	case 2: // FILE_WRITE
		modestring = "wb";
		break;
	default:
		Con_Printf("PF_fopen: no such mode %i (valid: 0 = read, 1 = append, 2 = write)\n", mode);
		G_FLOAT(OFS_RETURN) = -3;
		return;
	}
	filename = G_STRING(OFS_PARM0);
	// -4 failure (dangerous/non-portable filename) removed, FS_Open checks
	pr_files[filenum] = FS_Open(va("data/%s", filename), modestring, false, false);

	if (pr_files[filenum] == NULL && modestring == "rb")
		pr_files[filenum] = FS_Open(filename, modestring, false, false);

	if (pr_files[filenum] == NULL)
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = filenum;
}

//void(float fhandle) fclose = #111; // closes a file
void PF_fclose(void)
{
	int filenum = G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= MAX_PRFILES)
	{
		Con_Printf("PF_fclose: invalid file handle %i\n", filenum);
		return;
	}
	if (pr_files[filenum] == NULL)
	{
		Con_Printf("PF_fclose: no such file handle %i (or file has been closed)\n", filenum);
		return;
	}
	FS_Close(pr_files[filenum]);
	pr_files[filenum] = NULL;
}

//string(float fhandle) fgets = #112; // reads a line of text from the file and returns as a tempstring
void PF_fgets(void)
{
	int c, end;
	static char string[STRINGTEMP_LENGTH];
	int filenum = G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= MAX_PRFILES)
	{
		Con_Printf("PF_fgets: invalid file handle %i\n", filenum);
		return;
	}
	if (pr_files[filenum] == NULL)
	{
		Con_Printf("PF_fgets: no such file handle %i (or file has been closed)\n", filenum);
		return;
	}
	end = 0;
	for (;;)
	{
		c = FS_Getc(pr_files[filenum]);
		if (c == '\r' || c == '\n' || c < 0)
			break;
		if (end < STRINGTEMP_LENGTH - 1)
			string[end++] = c;
	}
	string[end] = 0;
	// remove \n following \r
	if (c == '\r')
	{
		c = FS_Getc(pr_files[filenum]);
		if (c != '\n')
			FS_UnGetc(pr_files[filenum], (unsigned char)c);
	}
	if (developer.integer)
		Con_Printf("fgets: %s\n", string);
	if (c >= 0 || end)
		G_INT(OFS_RETURN) = PR_SetString(string);
	else
		G_INT(OFS_RETURN) = 0;
}

//void(float fhandle, string s) fputs = #113; // writes a line of text to the end of the file
void PF_fputs(void)
{
	int stringlength;
	char string[STRINGTEMP_LENGTH];
	int filenum = G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= MAX_PRFILES)
	{
		Con_Printf("PF_fputs: invalid file handle %i\n", filenum);
		return;
	}
	if (pr_files[filenum] == NULL)
	{
		Con_Printf("PF_fputs: no such file handle %i (or file has been closed)\n", filenum);
		return;
	}
	PF_VarString(1, string, sizeof(string));
	if ((stringlength = strlen(string)))
		FS_Write(pr_files[filenum], string, stringlength);
	if (developer.integer)
		Con_Printf("fputs: %s\n", string);
}

//float(string s) strlen = #114; // returns how many characters are in a string
void PF_strlen(void)
{
	char *s;
	s = G_STRING(OFS_PARM0);
	if (s)
		G_FLOAT(OFS_RETURN) = strlen(s);
	else
		G_FLOAT(OFS_RETURN) = 0;
}

//string(string s1, string s2) strcat = #115; // concatenates two strings (for example "abc", "def" would return "abcdef") and returns as a tempstring
void PF_strcat(void)
{
	char *s = PR_GetTempString();
	PF_VarString(0, s, STRINGTEMP_LENGTH);
	G_INT(OFS_RETURN) = PR_SetString(s);
}

//string(string s, float start, float length) substring = #116; // returns a section of a string as a tempstring
void PF_substring(void)
{
	int i, start, length;
	char *s, *string = PR_GetTempString();
	s = G_STRING(OFS_PARM0);
	start = G_FLOAT(OFS_PARM1);
	length = G_FLOAT(OFS_PARM2);
	if (!s)
		s = "";
	for (i = 0;i < start && *s;i++, s++);
	for (i = 0;i < STRINGTEMP_LENGTH - 1 && *s && i < length;i++, s++)
		string[i] = *s;
	string[i] = 0;
	G_INT(OFS_RETURN) = PR_SetString(string);
}

//vector(string s) stov = #117; // returns vector value from a string
void PF_stov(void)
{
	char string[STRINGTEMP_LENGTH];
	PF_VarString(0, string, sizeof(string));
	Math_atov(string, G_VECTOR(OFS_RETURN));
}

//string(string s) strzone = #118; // makes a copy of a string into the string zone and returns it, this is often used to keep around a tempstring for longer periods of time (tempstrings are replaced often)
void PF_strzone(void)
{
	char *in, *out;
	in = G_STRING(OFS_PARM0);
	out = PR_Alloc(strlen(in) + 1);
	strcpy(out, in);
	G_INT(OFS_RETURN) = PR_SetString(out);
}

//void(string s) strunzone = #119; // removes a copy of a string from the string zone (you can not use that string again or it may crash!!!)
void PF_strunzone(void)
{
	PR_Free(G_STRING(OFS_PARM0));
}

//void(entity e, string s) clientcommand = #440; // executes a command string as if it came from the specified client
//this function originally written by KrimZon, made shorter by LordHavoc
void PF_clientcommand (void)
{
	client_t *temp_client;
	int i;

	//find client for this entity
	i = (NUM_FOR_EDICT(G_EDICT(OFS_PARM0)) - 1);
	if (i < 0 || i >= svs.maxclients || !svs.clients[i].active)
	{
		Con_Print("PF_clientcommand: entity is not a client\n");
		return;
	}

	temp_client = host_client;
	host_client = svs.clients + i;
	Cmd_ExecuteString (G_STRING(OFS_PARM1), src_client);
	host_client = temp_client;
}

//float(string s) tokenize = #441; // takes apart a string into individal words (access them with argv), returns how many
//this function originally written by KrimZon, made shorter by LordHavoc
//20040203: rewritten by LordHavoc (no longer uses allocations)
int num_tokens = 0;
char *tokens[256], tokenbuf[4096];
void PF_tokenize (void)
{
	int pos;
	const char *p;
	p = G_STRING(OFS_PARM0);

	num_tokens = 0;
	pos = 0;
	while(COM_ParseToken(&p, false))
	{
		if (num_tokens >= (int)(sizeof(tokens)/sizeof(tokens[0])))
			break;
		if (pos + strlen(com_token) + 1 > sizeof(tokenbuf))
			break;
		tokens[num_tokens++] = tokenbuf + pos;
		strcpy(tokenbuf + pos, com_token);
		pos += strlen(com_token) + 1;
	}

	G_FLOAT(OFS_RETURN) = num_tokens;
}

//string(float n) argv = #442; // returns a word from the tokenized string (returns nothing for an invalid index)
//this function originally written by KrimZon, made shorter by LordHavoc
void PF_argv (void)
{
	int token_num = G_FLOAT(OFS_PARM0);
	if (token_num >= 0 && token_num < num_tokens)
		G_INT(OFS_RETURN) = PR_SetString(tokens[token_num]);
	else
		G_INT(OFS_RETURN) = PR_SetString("");
}

//void(entity e, entity tagentity, string tagname) setattachment = #443; // attachs e to a tag on tagentity (note: use "" to attach to entity origin/angles instead of a tag)
void PF_setattachment (void)
{
	edict_t *e = G_EDICT(OFS_PARM0);
	edict_t *tagentity = G_EDICT(OFS_PARM1);
	char *tagname = G_STRING(OFS_PARM2);
	eval_t *v;
	int modelindex;
	model_t *model;

	if (e == sv.edicts)
		PF_WARNING("setattachment: can not modify world entity\n");
	if (e->e->free)
		PF_WARNING("setattachment: can not modify free entity\n");

	if (tagentity == NULL)
		tagentity = sv.edicts;

	v = GETEDICTFIELDVALUE(e, eval_tag_entity);
	if (v)
		v->edict = EDICT_TO_PROG(tagentity);

	v = GETEDICTFIELDVALUE(e, eval_tag_index);
	if (v)
		v->_float = 0;
	if (tagentity != NULL && tagentity != sv.edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->v->modelindex;
		if (modelindex >= 0 && modelindex < MAX_MODELS && (model = sv.models[modelindex]))
		{
			v->_float = Mod_Alias_GetTagIndexForName(model, tagentity->v->skin, tagname);
			if (v->_float == 0)
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", NUM_FOR_EDICT(e), NUM_FOR_EDICT(tagentity), tagname, tagname, NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", NUM_FOR_EDICT(e), NUM_FOR_EDICT(tagentity), tagname, tagname, NUM_FOR_EDICT(tagentity));
	}
}

/////////////////////////////////////////
// DP_MD3_TAGINFO extension coded by VorteX

int SV_GetTagIndex (edict_t *e, char *tagname)
{
	int i;
	model_t *model;

	i = e->v->modelindex;
	if (i < 1 || i >= MAX_MODELS)
		return -1;
	model = sv.models[i];

	return Mod_Alias_GetTagIndexForName(model, e->v->skin, tagname);
};

// Warnings/errors code:
// 0 - normal (everything all-right)
// 1 - world entity
// 2 - free entity
// 3 - null or non-precached model
// 4 - no tags with requested index
// 5 - runaway loop at attachment chain
extern cvar_t cl_bob;
extern cvar_t cl_bobcycle;
extern cvar_t cl_bobup;
int SV_GetTagMatrix (matrix4x4_t *out, edict_t *ent, int tagindex)
{
	eval_t *val;
	int modelindex, reqframe, attachloop;
	matrix4x4_t entitymatrix, tagmatrix, attachmatrix;
	edict_t *attachent;
	model_t *model;

	Matrix4x4_CreateIdentity(out); // warnings and errors return identical matrix

	if (ent == sv.edicts)
		return 1;
	if (ent->e->free)
		return 2;

	modelindex = (int)ent->v->modelindex;
	if (modelindex <= 0 || modelindex > MAX_MODELS)
		return 3;

	model = sv.models[modelindex];

	if (ent->v->frame >= 0 && ent->v->frame < model->numframes && model->animscenes)
		reqframe = model->animscenes[(int)ent->v->frame].firstframe;
	else
		reqframe = 0; // if model has wrong frame, engine automatically switches to model first frame

	// get initial tag matrix
	if (tagindex)
	{
		int ret = Mod_Alias_GetTagMatrix(model, reqframe, tagindex - 1, &tagmatrix);
		if (ret)
			return ret;
	}
	else
		Matrix4x4_CreateIdentity(&tagmatrix);

	if ((val = GETEDICTFIELDVALUE(ent, eval_tag_entity)) && val->edict)
	{ // DP_GFX_QUAKE3MODELTAGS, scan all chain and stop on unattached entity
		attachloop = 0;
		do
		{
			attachent = EDICT_NUM(val->edict); // to this it entity our entity is attached
			val = GETEDICTFIELDVALUE(ent, eval_tag_index);
			if (val->_float >= 1 && attachent->v->modelindex >= 1 && attachent->v->modelindex < MAX_MODELS && (model = sv.models[(int)attachent->v->modelindex]) && model->animscenes && attachent->v->frame >= 0 && attachent->v->frame < model->numframes)
				Mod_Alias_GetTagMatrix(model, model->animscenes[(int)attachent->v->frame].firstframe, val->_float - 1, &attachmatrix);
			else
				Matrix4x4_CreateIdentity(&attachmatrix);

			// apply transformation by child entity matrix
			val = GETEDICTFIELDVALUE(ent, eval_scale);
			if (val->_float == 0)
				val->_float = 1;
			Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->v->origin[0], ent->v->origin[1], ent->v->origin[2], -ent->v->angles[0], ent->v->angles[1], ent->v->angles[2], val->_float);
			Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
			out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
			out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
			out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);
			Matrix4x4_Copy(&tagmatrix, out);

			// finally transformate by matrix of tag on parent entity
			Matrix4x4_Concat(out, &attachmatrix, &tagmatrix);
			out->m[0][3] = attachmatrix.m[0][3] + attachmatrix.m[0][0]*tagmatrix.m[0][3] + attachmatrix.m[0][1]*tagmatrix.m[1][3] + attachmatrix.m[0][2]*tagmatrix.m[2][3];
			out->m[1][3] = attachmatrix.m[1][3] + attachmatrix.m[1][0]*tagmatrix.m[0][3] + attachmatrix.m[1][1]*tagmatrix.m[1][3] + attachmatrix.m[1][2]*tagmatrix.m[2][3];
			out->m[2][3] = attachmatrix.m[2][3] + attachmatrix.m[2][0]*tagmatrix.m[0][3] + attachmatrix.m[2][1]*tagmatrix.m[1][3] + attachmatrix.m[2][2]*tagmatrix.m[2][3];
			Matrix4x4_Copy(&tagmatrix, out);

			ent = attachent;
			attachloop += 1;
			if (attachloop > 255) // prevent runaway looping
				return 5;
		}
		while ((val = GETEDICTFIELDVALUE(ent, eval_tag_entity)) && val->edict);
	}

	// normal or RENDER_VIEWMODEL entity (or main parent entity on attach chain)
	val = GETEDICTFIELDVALUE(ent, eval_scale);
	if (val->_float == 0)
		val->_float = 1;
	// Alias models have inverse pitch, bmodels can't have tags, so don't check for modeltype...
	Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->v->origin[0], ent->v->origin[1], ent->v->origin[2], -ent->v->angles[0], ent->v->angles[1], ent->v->angles[2], val->_float);
	Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
	out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
	out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
	out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);

	if ((val = GETEDICTFIELDVALUE(ent, eval_viewmodelforclient)) && val->edict)
	{// RENDER_VIEWMODEL magic
		Matrix4x4_Copy(&tagmatrix, out);
		ent = EDICT_NUM(val->edict);

		val = GETEDICTFIELDVALUE(ent, eval_scale);
		if (val->_float == 0)
			val->_float = 1;

		Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->v->origin[0], ent->v->origin[1], ent->v->origin[2] + ent->v->view_ofs[2], ent->v->v_angle[0], ent->v->v_angle[1], ent->v->v_angle[2], val->_float);
		Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
		out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
		out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
		out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);

		/*
		// Cl_bob, ported from rendering code
		if (ent->v->health > 0 && cl_bob.value && cl_bobcycle.value)
		{
			double bob, cycle;
			// LordHavoc: this code is *weird*, but not replacable (I think it
			// should be done in QC on the server, but oh well, quake is quake)
			// LordHavoc: figured out bobup: the time at which the sin is at 180
			// degrees (which allows lengthening or squishing the peak or valley)
			cycle = sv.time/cl_bobcycle.value;
			cycle -= (int)cycle;
			if (cycle < cl_bobup.value)
				cycle = sin(M_PI * cycle / cl_bobup.value);
			else
				cycle = sin(M_PI + M_PI * (cycle-cl_bobup.value)/(1.0 - cl_bobup.value));
			// bob is proportional to velocity in the xy plane
			// (don't count Z, or jumping messes it up)
			bob = sqrt(ent->v->velocity[0]*ent->v->velocity[0] + ent->v->velocity[1]*ent->v->velocity[1])*cl_bob.value;
			bob = bob*0.3 + bob*0.7*cycle;
			out->m[2][3] += bound(-7, bob, 4);
		}
		*/
	}
	return 0;
}

//float(entity ent, string tagname) gettagindex;

void PF_gettagindex (void)
{
	edict_t *ent = G_EDICT(OFS_PARM0);
	char *tag_name = G_STRING(OFS_PARM1);
	int modelindex, tag_index;

	if (ent == sv.edicts)
		PF_WARNING("gettagindex: can't affect world entity\n");
	if (ent->e->free)
		PF_WARNING("gettagindex: can't affect free entity\n");

	modelindex = (int)ent->v->modelindex;
	tag_index = 0;
	if (modelindex <= 0 || modelindex > MAX_MODELS)
		Con_DPrintf("gettagindex(entity #%i): null or non-precached model\n", NUM_FOR_EDICT(ent));
	else
	{
		tag_index = SV_GetTagIndex(ent, tag_name);
		if (tag_index == 0)
			Con_DPrintf("gettagindex(entity #%i): tag \"%s\" not found\n", NUM_FOR_EDICT(ent), tag_name);
	}
	G_FLOAT(OFS_RETURN) = tag_index;
};

//vector(entity ent, float tagindex) gettaginfo;
void PF_gettaginfo (void)
{
	edict_t *e = G_EDICT(OFS_PARM0);
	int tagindex = (int)G_FLOAT(OFS_PARM1);
	matrix4x4_t tag_matrix;
	int returncode;

	returncode = SV_GetTagMatrix(&tag_matrix, e, tagindex);
	Matrix4x4_ToVectors(&tag_matrix, pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up, G_VECTOR(OFS_RETURN));

	switch(returncode)
	{
		case 1:
			PF_WARNING("gettagindex: can't affect world entity\n");
			break;
		case 2:
			PF_WARNING("gettagindex: can't affect free entity\n");
			break;
		case 3:
			Con_DPrintf("SV_GetTagMatrix(entity #%i): null or non-precached model\n", NUM_FOR_EDICT(e));
			break;
		case 4:
			Con_DPrintf("SV_GetTagMatrix(entity #%i): model has no tag with requested index %i\n", NUM_FOR_EDICT(e), tagindex);
			break;
		case 5:
			Con_DPrintf("SV_GetTagMatrix(entity #%i): runaway loop at attachment chain\n", NUM_FOR_EDICT(e));
			break;
	}
}


/////////////////////////////////////////
// DP_QC_FS_SEARCH extension

// qc fs search handling
#define MAX_SEARCHES 128

fssearch_t *pr_fssearchlist[MAX_SEARCHES];

void PR_Search_Init(void)
{
	memset(pr_fssearchlist,0,sizeof(pr_fssearchlist));
}

void PR_Search_Reset(void)
{
	int i;
	// reset the fssearch list
	for(i = 0; i < MAX_SEARCHES; i++)
		if(pr_fssearchlist[i])
			FS_FreeSearch(pr_fssearchlist[i]);
	memset(pr_fssearchlist,0,sizeof(pr_fssearchlist));
}

/*
=========
PF_search_begin

float search_begin(string pattern, float caseinsensitive, float quiet)
=========
*/
void PF_search_begin(void)
{
	int handle;
	char *pattern;
	int caseinsens, quiet;

	pattern = G_STRING(OFS_PARM0);

	PR_CheckEmptyString(pattern);

	caseinsens = G_FLOAT(OFS_PARM1);
	quiet = G_FLOAT(OFS_PARM2);

	for(handle = 0; handle < MAX_SEARCHES; handle++)
		if(!pr_fssearchlist[handle])
			break;

	if(handle >= MAX_SEARCHES)
	{
		Con_Printf("PR_search_begin: ran out of search handles (%i)\n", MAX_SEARCHES);
		G_FLOAT(OFS_RETURN) = -2;
		return;
	}

	if(!(pr_fssearchlist[handle] = FS_Search(pattern,caseinsens, quiet)))
		G_FLOAT(OFS_RETURN) = -1;
	else
		G_FLOAT(OFS_RETURN) = handle;
}

/*
=========
VM_search_end

void	search_end(float handle)
=========
*/
void PF_search_end(void)
{
	int handle;

	handle = G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= MAX_SEARCHES)
	{
		Con_Printf("PF_search_end: invalid handle %i\n", handle);
		return;
	}
	if(pr_fssearchlist[handle] == NULL)
	{
		Con_Printf("PF_search_end: no such handle %i\n", handle);
		return;
	}

	FS_FreeSearch(pr_fssearchlist[handle]);
	pr_fssearchlist[handle] = NULL;
}

/*
=========
VM_search_getsize

float	search_getsize(float handle)
=========
*/
void PF_search_getsize(void)
{
	int handle;

	handle = G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= MAX_SEARCHES)
	{
		Con_Printf("PF_search_getsize: invalid handle %i\n", handle);
		return;
	}
	if(pr_fssearchlist[handle] == NULL)
	{
		Con_Printf("PF_search_getsize: no such handle %i\n", handle);
		return;
	}

	G_FLOAT(OFS_RETURN) = pr_fssearchlist[handle]->numfilenames;
}

/*
=========
VM_search_getfilename

string	search_getfilename(float handle, float num)
=========
*/
void PF_search_getfilename(void)
{
	int handle, filenum;
	char *tmp;

	handle = G_FLOAT(OFS_PARM0);
	filenum = G_FLOAT(OFS_PARM1);

	if(handle < 0 || handle >= MAX_SEARCHES)
	{
		Con_Printf("PF_search_getfilename: invalid handle %i\n", handle);
		return;
	}
	if(pr_fssearchlist[handle] == NULL)
	{
		Con_Printf("PF_search_getfilename: no such handle %i\n", handle);
		return;
	}
	if(filenum < 0 || filenum >= pr_fssearchlist[handle]->numfilenames)
	{
		Con_Printf("PF_search_getfilename: invalid filenum %i\n", filenum);
		return;
	}

	tmp = PR_GetTempString();
	strcpy(tmp, pr_fssearchlist[handle]->filenames[filenum]);

	G_INT(OFS_RETURN) = PR_SetString(tmp);
}

void PF_cvar_string (void)
{
	char *str;
	cvar_t *var;
	char *tmp;

	str = G_STRING(OFS_PARM0);
	var = Cvar_FindVar (str);
	if (var)
	{
		tmp = PR_GetTempString();
		strcpy(tmp, var->string);
	}
	else
		tmp = "";
	G_INT(OFS_RETURN) = PR_SetString(tmp);
}

//void(entity clent) dropclient (DP_SV_DROPCLIENT)
void PF_dropclient (void)
{
	int clientnum;
	client_t *oldhostclient;
	clientnum = G_EDICTNUM(OFS_PARM0) - 1;
	if (clientnum < 0 || clientnum >= svs.maxclients)
		PF_WARNING("dropclient: not a client\n");
	if (!svs.clients[clientnum].active)
		PF_WARNING("dropclient: that client slot is not connected\n");
	oldhostclient = host_client;
	host_client = svs.clients + clientnum;
	SV_DropClient(false);
	host_client = oldhostclient;
}

//entity() spawnclient (DP_SV_BOTCLIENT)
void PF_spawnclient (void)
{
	int i;
	edict_t	*ed;
	pr_xfunction->builtinsprofile += 2;
	ed = sv.edicts;
	for (i = 0;i < svs.maxclients;i++)
	{
		if (!svs.clients[i].active)
		{
			pr_xfunction->builtinsprofile += 100;
			SV_ConnectClient (i, NULL);
			ed = EDICT_NUM(i + 1);
			break;
		}
	}
	RETURN_EDICT(ed);
}

//float(entity clent) clienttype (DP_SV_BOTCLIENT)
void PF_clienttype (void)
{
	int clientnum;
	clientnum = G_EDICTNUM(OFS_PARM0) - 1;
	if (clientnum < 0 || clientnum >= svs.maxclients)
		G_FLOAT(OFS_RETURN) = 3;
	else if (!svs.clients[clientnum].active)
		G_FLOAT(OFS_RETURN) = 0;
	else if (svs.clients[clientnum].netconnection)
		G_FLOAT(OFS_RETURN) = 1;
	else
		G_FLOAT(OFS_RETURN) = 2;
}

builtin_t pr_builtin[] =
{
NULL,						// #0
PF_makevectors,				// #1 void(entity e) makevectors
PF_setorigin,				// #2 void(entity e, vector o) setorigin
PF_setmodel,				// #3 void(entity e, string m) setmodel
PF_setsize,					// #4 void(entity e, vector min, vector max) setsize
NULL,						// #5 void(entity e, vector min, vector max) setabssize
PF_break,					// #6 void() break
PF_random,					// #7 float() random
PF_sound,					// #8 void(entity e, float chan, string samp) sound
PF_normalize,				// #9 vector(vector v) normalize
PF_error,					// #10 void(string e) error
PF_objerror,				// #11 void(string e) objerror
PF_vlen,					// #12 float(vector v) vlen
PF_vectoyaw,				// #13 float(vector v) vectoyaw
PF_Spawn,					// #14 entity() spawn
PF_Remove,					// #15 void(entity e) remove
PF_traceline,				// #16 float(vector v1, vector v2, float tryents) traceline
PF_checkclient,				// #17 entity() clientlist
PF_Find,					// #18 entity(entity start, .string fld, string match) find
PF_precache_sound,			// #19 void(string s) precache_sound
PF_precache_model,			// #20 void(string s) precache_model
PF_stuffcmd,				// #21 void(entity client, string s)stuffcmd
PF_findradius,				// #22 entity(vector org, float rad) findradius
PF_bprint,					// #23 void(string s) bprint
PF_sprint,					// #24 void(entity client, string s) sprint
PF_dprint,					// #25 void(string s) dprint
PF_ftos,					// #26 void(string s) ftos
PF_vtos,					// #27 void(string s) vtos
PF_coredump,				// #28 void() coredump
PF_traceon,					// #29 void() traceon
PF_traceoff,				// #30 void() traceoff
PF_eprint,					// #31 void(entity e) eprint
PF_walkmove,				// #32 float(float yaw, float dist) walkmove
NULL,						// #33
PF_droptofloor,				// #34 float() droptofloor
PF_lightstyle,				// #35 void(float style, string value) lightstyle
PF_rint,					// #36 float(float v) rint
PF_floor,					// #37 float(float v) floor
PF_ceil,					// #38 float(float v) ceil
NULL,						// #39
PF_checkbottom,				// #40 float(entity e) checkbottom
PF_pointcontents,			// #41 float(vector v) pointcontents
NULL,						// #42
PF_fabs,					// #43 float(float f) fabs
PF_aim,						// #44 vector(entity e, float speed) aim
PF_cvar,					// #45 float(string s) cvar
PF_localcmd,				// #46 void(string s) localcmd
PF_nextent,					// #47 entity(entity e) nextent
PF_particle,				// #48 void(vector o, vector d, float color, float count) particle
PF_changeyaw,				// #49 void() ChangeYaw
NULL,						// #50
PF_vectoangles,				// #51 vector(vector v) vectoangles
PF_WriteByte,				// #52 void(float to, float f) WriteByte
PF_WriteChar,				// #53 void(float to, float f) WriteChar
PF_WriteShort,				// #54 void(float to, float f) WriteShort
PF_WriteLong,				// #55 void(float to, float f) WriteLong
PF_WriteCoord,				// #56 void(float to, float f) WriteCoord
PF_WriteAngle,				// #57 void(float to, float f) WriteAngle
PF_WriteString,				// #58 void(float to, string s) WriteString
PF_WriteEntity,				// #59 void(float to, entity e) WriteEntity
PF_sin,						// #60 float(float f) sin (DP_QC_SINCOSSQRTPOW)
PF_cos,						// #61 float(float f) cos (DP_QC_SINCOSSQRTPOW)
PF_sqrt,					// #62 float(float f) sqrt (DP_QC_SINCOSSQRTPOW)
PF_changepitch,				// #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
PF_TraceToss,				// #64 void(entity e, entity ignore) tracetoss (DP_QC_TRACETOSS)
PF_etos,					// #65 string(entity ent) etos (DP_QC_ETOS)
NULL,						// #66
SV_MoveToGoal,				// #67 void(float step) movetogoal
PF_precache_file,			// #68 string(string s) precache_file
PF_makestatic,				// #69 void(entity e) makestatic
PF_changelevel,				// #70 void(string s) changelevel
NULL,						// #71
PF_cvar_set,				// #72 void(string var, string val) cvar_set
PF_centerprint,				// #73 void(entity client, strings) centerprint
PF_ambientsound,			// #74 void(vector pos, string samp, float vol, float atten) ambientsound
PF_precache_model,			// #75 string(string s) precache_model2
PF_precache_sound,			// #76 string(string s) precache_sound2
PF_precache_file,			// #77 string(string s) precache_file2
PF_setspawnparms,			// #78 void(entity e) setspawnparms
NULL,						// #79
NULL,						// #80
PF_stof,					// #81 float(string s) stof (FRIK_FILE)
NULL,						// #82
NULL,						// #83
NULL,						// #84
NULL,						// #85
NULL,						// #86
NULL,						// #87
NULL,						// #88
NULL,						// #89
PF_tracebox,				// #90 void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox (DP_QC_TRACEBOX)
PF_randomvec,				// #91 vector() randomvec (DP_QC_RANDOMVEC)
PF_GetLight,				// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
PF_registercvar,			// #93 float(string name, string value) registercvar (DP_REGISTERCVAR)
PF_min,						// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
PF_max,						// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
PF_bound,					// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
PF_pow,						// #97 float(float f, float f) pow (DP_QC_SINCOSSQRTPOW)
PF_FindFloat,				// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
PF_checkextension,			// #99 float(string s) checkextension (the basis of the extension system)
NULL,						// #100
NULL,						// #101
NULL,						// #102
NULL,						// #103
NULL,						// #104
NULL,						// #105
NULL,						// #106
NULL,						// #107
NULL,						// #108
NULL,						// #109
PF_fopen,					// #110 float(string filename, float mode) fopen (FRIK_FILE)
PF_fclose,					// #111 void(float fhandle) fclose (FRIK_FILE)
PF_fgets,					// #112 string(float fhandle) fgets (FRIK_FILE)
PF_fputs,					// #113 void(float fhandle, string s) fputs (FRIK_FILE)
PF_strlen,					// #114 float(string s) strlen (FRIK_FILE)
PF_strcat,					// #115 string(string s1, string s2) strcat (FRIK_FILE)
PF_substring,				// #116 string(string s, float start, float length) substring (FRIK_FILE)
PF_stov,					// #117 vector(string) stov (FRIK_FILE)
PF_strzone,					// #118 string(string s) strzone (FRIK_FILE)
PF_strunzone,				// #119 void(string s) strunzone (FRIK_FILE)
#define a NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
a a a a a a a a				// #120-199
a a a a a a a a a a			// #200-299
a a a a a a a a a a			// #300-399
PF_copyentity,				// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
PF_setcolor,				// #401 void(entity ent, float colors) setcolor (DP_QC_SETCOLOR)
PF_findchain,				// #402 entity(.string fld, string match) findchain (DP_QC_FINDCHAIN)
PF_findchainfloat,			// #403 entity(.float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
PF_effect,					// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
PF_te_blood,				// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
PF_te_bloodshower,			// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
PF_te_explosionrgb,			// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
PF_te_particlecube,			// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
PF_te_particlerain,			// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
PF_te_particlesnow,			// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
PF_te_spark,				// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
PF_te_gunshotquad,			// #412 void(vector org) te_gunshotquad (DP_QUADEFFECTS1)
PF_te_spikequad,			// #413 void(vector org) te_spikequad (DP_QUADEFFECTS1)
PF_te_superspikequad,		// #414 void(vector org) te_superspikequad (DP_QUADEFFECTS1)
PF_te_explosionquad,		// #415 void(vector org) te_explosionquad (DP_QUADEFFECTS1)
PF_te_smallflash,			// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
PF_te_customflash,			// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
PF_te_gunshot,				// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_spike,				// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_superspike,			// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_explosion,			// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_tarexplosion,			// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_wizspike,				// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_knightspike,			// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_lavasplash,			// #425 void(vector org) te_lavasplash (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_teleport,				// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_explosion2,			// #427 void(vector org, float colorstart, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_lightning1,			// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_lightning2,			// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_lightning3,			// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
PF_te_beam,					// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
PF_vectorvectors,			// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
PF_te_plasmaburn,			// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
PF_getsurfacenumpoints,		// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
PF_getsurfacepoint,			// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
PF_getsurfacenormal,		// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
PF_getsurfacetexture,		// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
PF_getsurfacenearpoint,		// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
PF_getsurfaceclippedpoint,	// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
PF_clientcommand,			// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_tokenize,				// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_argv,					// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_setattachment,			// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
PF_search_begin,			// #444 float(string pattern, float caseinsensitive, float quiet) search_begin (DP_FS_SEARCH)
PF_search_end,				// #445 void(float handle) search_end (DP_FS_SEARCH)
PF_search_getsize,			// #446 float(float handle) search_getsize (DP_FS_SEARCH)
PF_search_getfilename,		// #447 string(float handle, float num) search_getfilename (DP_FS_SEARCH)
PF_cvar_string,				// #448 string(string s) cvar_string (DP_QC_CVAR_STRING)
PF_findflags,				// #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
PF_findchainflags,			// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
PF_gettagindex,				// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
PF_gettaginfo,				// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
PF_dropclient,				// #453 void(entity clent) dropclient (DP_SV_DROPCLIENT)
PF_spawnclient,				// #454 entity() spawnclient (DP_SV_BOTCLIENT)
PF_clienttype,				// #455 float(entity clent) clienttype (DP_SV_BOTCLIENT)
NULL,						// #456
NULL,						// #457
NULL,						// #458
NULL,						// #459
a a a a						// #460-499 (LordHavoc)
};

builtin_t *pr_builtins = pr_builtin;
int pr_numbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);

void PR_Cmd_Init(void)
{
	PR_Files_Init();
	PR_Search_Init();
}

void PR_Cmd_Shutdown(void)
{
}

void PR_Cmd_Reset(void)
{
	PR_Search_Reset();
	PR_Files_CloseAll();
}

