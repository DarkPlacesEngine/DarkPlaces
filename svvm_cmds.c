#include "prvm_cmds.h"

//============================================================================
// Server

#define PF_WARNING(s) do{Con_Printf(s);PRVM_PrintState();return;}while(0)
cvar_t sv_aim = {CVAR_SAVE, "sv_aim", "2"}; //"0.93"}; // LordHavoc: disabled autoaim by default


char *vm_sv_extensions =
"DP_CON_EXPANDCVAR "
"DP_CON_ALIASPARAMETERS "
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
"DP_ENT_COLORMOD "
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
"DP_QUAKE3_MAP "
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
"DP_SV_PRECACHEANYTIME "
"DP_SV_PUNCHVECTOR "
"DP_SV_ROTATINGBMODEL "
"DP_SV_SETCOLOR "
"DP_SV_SLOWMO "
"DP_SV_WRITEUNTERMINATEDSTRING "
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
;

/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
	AngleVectors (PRVM_G_VECTOR(OFS_PARM0), prog->globals.server->v_forward, prog->globals.server->v_right, prog->globals.server->v_up);
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
	VectorNormalize2(PRVM_G_VECTOR(OFS_PARM0), prog->globals.server->v_forward);
	VectorVectors(prog->globals.server->v_forward, prog->globals.server->v_right, prog->globals.server->v_up);
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
	prvm_edict_t	*e;
	float	*org;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
		PF_WARNING("setorigin: can not modify world entity\n");
	if (e->priv.server->free)
		PF_WARNING("setorigin: can not modify free entity\n");
	org = PRVM_G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->fields.server->origin);
	SV_LinkEdict (e, false);
}


void SetMinMaxSize (prvm_edict_t *e, float *min, float *max, qboolean rotate)
{
	int		i;

	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PRVM_ERROR("SetMinMaxSize: backwards mins/maxs\n");

// set derived values
	VectorCopy (min, e->fields.server->mins);
	VectorCopy (max, e->fields.server->maxs);
	VectorSubtract (max, min, e->fields.server->size);

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
	prvm_edict_t	*e;
	float	*min, *max;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
		PF_WARNING("setsize: can not modify world entity\n");
	if (e->priv.server->free)
		PF_WARNING("setsize: can not modify free entity\n");
	min = PRVM_G_VECTOR(OFS_PARM1);
	max = PRVM_G_VECTOR(OFS_PARM2);
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
	prvm_edict_t	*e;
	model_t	*mod;
	int		i;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
		PF_WARNING("setmodel: can not modify world entity\n");
	if (e->priv.server->free)
		PF_WARNING("setmodel: can not modify free entity\n");
	i = SV_ModelIndex(PRVM_G_STRING(OFS_PARM1), 1);
	e->fields.server->model = PRVM_SetEngineString(sv.model_precache[i]);
	e->fields.server->modelindex = i;

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
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
	client_t	*client;
	int			entnum;
	char string[VM_STRINGTEMP_LENGTH];

	entnum = PRVM_G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to sprint to a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	VM_VarString(1, string, sizeof(string));
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
	char string[VM_STRINGTEMP_LENGTH];

	entnum = PRVM_G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to sprint to a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	VM_VarString(1, string, sizeof(string));
	MSG_WriteChar(&client->message,svc_centerprint);
	MSG_WriteString(&client->message, string);
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

	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	color = PRVM_G_FLOAT(OFS_PARM2);
	count = PRVM_G_FLOAT(OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (void)
{
	const char	*samp;
	float		*pos;
	float 		vol, attenuation;
	int			soundnum, large;

	pos = PRVM_G_VECTOR (OFS_PARM0);
	samp = PRVM_G_STRING(OFS_PARM1);
	vol = PRVM_G_FLOAT(OFS_PARM2);
	attenuation = PRVM_G_FLOAT(OFS_PARM3);

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
	const char	*sample;
	int			channel;
	prvm_edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = PRVM_G_FLOAT(OFS_PARM3) * 255;
	attenuation = PRVM_G_FLOAT(OFS_PARM4);

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
	prvm_edict_t	*ent;

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);
	move = PRVM_G_FLOAT(OFS_PARM2);
	ent = PRVM_G_EDICT(OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, move, ent);

	prog->globals.server->trace_allsolid = trace.allsolid;
	prog->globals.server->trace_startsolid = trace.startsolid;
	prog->globals.server->trace_fraction = trace.fraction;
	prog->globals.server->trace_inwater = trace.inwater;
	prog->globals.server->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, prog->globals.server->trace_endpos);
	VectorCopy (trace.plane.normal, prog->globals.server->trace_plane_normal);
	prog->globals.server->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(trace.ent);
	else
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
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
	prvm_edict_t	*ent;

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	m1 = PRVM_G_VECTOR(OFS_PARM1);
	m2 = PRVM_G_VECTOR(OFS_PARM2);
	v2 = PRVM_G_VECTOR(OFS_PARM3);
	move = PRVM_G_FLOAT(OFS_PARM4);
	ent = PRVM_G_EDICT(OFS_PARM5);

	trace = SV_Move (v1, m1, m2, v2, move, ent);

	prog->globals.server->trace_allsolid = trace.allsolid;
	prog->globals.server->trace_startsolid = trace.startsolid;
	prog->globals.server->trace_fraction = trace.fraction;
	prog->globals.server->trace_inwater = trace.inwater;
	prog->globals.server->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, prog->globals.server->trace_endpos);
	VectorCopy (trace.plane.normal, prog->globals.server->trace_plane_normal);
	prog->globals.server->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(trace.ent);
	else
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
}

extern trace_t SV_Trace_Toss (prvm_edict_t *ent, prvm_edict_t *ignore);
void PF_tracetoss (void)
{
	trace_t	trace;
	prvm_edict_t	*ent;
	prvm_edict_t	*ignore;

	prog->xfunction->builtinsprofile += 600;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("tracetoss: can not use world entity\n");
	ignore = PRVM_G_EDICT(OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	prog->globals.server->trace_allsolid = trace.allsolid;
	prog->globals.server->trace_startsolid = trace.startsolid;
	prog->globals.server->trace_fraction = trace.fraction;
	prog->globals.server->trace_inwater = trace.inwater;
	prog->globals.server->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, prog->globals.server->trace_endpos);
	VectorCopy (trace.plane.normal, prog->globals.server->trace_plane_normal);
	prog->globals.server->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(trace.ent);
	else
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
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
	prvm_edict_t	*ent;
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
		prog->xfunction->builtinsprofile++;
		// wrap around
		if (i == svs.maxclients+1)
			i = 1;
		// look up the client's edict
		ent = PRVM_EDICT_NUM(i);
		// check if it is to be ignored, but never ignore the one we started on (prevent infinite loop)
		if (i != check && (ent->priv.server->free || ent->fields.server->health <= 0 || ((int)ent->fields.server->flags & FL_NOTARGET)))
			continue;
		// found a valid client (possibly the same one again)
		break;
	}

// get the PVS for the entity
	VectorAdd(ent->fields.server->origin, ent->fields.server->view_ofs, org);
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
	prvm_edict_t	*ent, *self;
	vec3_t	view;

	// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	// return check if it might be visible
	ent = PRVM_EDICT_NUM(sv.lastcheck);
	if (ent->priv.server->free || ent->fields.server->health <= 0)
	{
		VM_RETURN_EDICT(prog->edicts);
		return;
	}

	// if current entity can't possibly see the check entity, return 0
	self = PRVM_PROG_TO_EDICT(prog->globals.server->self);
	VectorAdd(self->fields.server->origin, self->fields.server->view_ofs, view);
	if (sv.worldmodel && checkpvsbytes && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, checkpvs, view, view))
	{
		c_notvis++;
		VM_RETURN_EDICT(prog->edicts);
		return;
	}

	// might be able to see it
	c_invis++;
	VM_RETURN_EDICT(ent);
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
	const char	*str;
	client_t	*old;

	entnum = PRVM_G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("Can't stuffcmd to a non-client\n");
		return;
	}
	str = PRVM_G_STRING(OFS_PARM1);

	old = host_client;
	host_client = svs.clients + entnum-1;
	Host_ClientCommands ("%s", str);
	host_client = old;
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
	prvm_edict_t *ent, *chain;
	vec_t radius, radius2;
	vec3_t org, eorg, mins, maxs;
	int i;
	int numtouchedicts;
	prvm_edict_t *touchedicts[MAX_EDICTS];

	chain = (prvm_edict_t *)prog->edicts;

	VectorCopy(PRVM_G_VECTOR(OFS_PARM0), org);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	radius2 = radius * radius;

	mins[0] = org[0] - (radius + 1);
	mins[1] = org[1] - (radius + 1);
	mins[2] = org[2] - (radius + 1);
	maxs[0] = org[0] + (radius + 1);
	maxs[1] = org[1] + (radius + 1);
	maxs[2] = org[2] + (radius + 1);
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
		prog->xfunction->builtinsprofile++;
		// Quake did not return non-solid entities but darkplaces does
		// (note: this is the reason you can't blow up fallen zombies)
		if (ent->fields.server->solid == SOLID_NOT && !sv_gameplayfix_blowupfallenzombies.integer)
			continue;
		// LordHavoc: compare against bounding box rather than center so it
		// doesn't miss large objects, and use DotProduct instead of Length
		// for a major speedup
		VectorSubtract(org, ent->fields.server->origin, eorg);
		if (sv_gameplayfix_findradiusdistancetobox.integer)
		{
			eorg[0] -= bound(ent->fields.server->mins[0], eorg[0], ent->fields.server->maxs[0]);
			eorg[1] -= bound(ent->fields.server->mins[1], eorg[1], ent->fields.server->maxs[1]);
			eorg[2] -= bound(ent->fields.server->mins[2], eorg[2], ent->fields.server->maxs[2]);
		}
		else
			VectorMAMAM(1, eorg, 0.5f, ent->fields.server->mins, 0.5f, ent->fields.server->maxs, eorg);
		if (DotProduct(eorg, eorg) < radius2)
		{
			ent->fields.server->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	VM_RETURN_EDICT(chain);
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}


void PF_precache_sound (void)
{
	SV_SoundIndex(PRVM_G_STRING(OFS_PARM0), 2);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

void PF_precache_model (void)
{
	SV_ModelIndex(PRVM_G_STRING(OFS_PARM0), 2);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
	prvm_edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	mfunction_t	*oldf;
	int 	oldself;

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(prog->globals.server->self);
	if (ent == prog->edicts)
		PF_WARNING("walkmove: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("walkmove: can not modify free entity\n");
	yaw = PRVM_G_FLOAT(OFS_PARM0);
	dist = PRVM_G_FLOAT(OFS_PARM1);

	if ( !( (int)ent->fields.server->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
		return;

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = prog->xfunction;
	oldself = prog->globals.server->self;

	PRVM_G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);


// restore program state
	prog->xfunction = oldf;
	prog->globals.server->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (void)
{
	prvm_edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(prog->globals.server->self);
	if (ent == prog->edicts)
		PF_WARNING("droptofloor: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("droptofloor: can not modify free entity\n");

	VectorCopy (ent->fields.server->origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->fields.server->origin, ent->fields.server->mins, ent->fields.server->maxs, end, MOVE_NORMAL, ent);

	if (trace.fraction != 1)
	{
		VectorCopy (trace.endpos, ent->fields.server->origin);
		SV_LinkEdict (ent, false);
		ent->fields.server->flags = (int)ent->fields.server->flags | FL_ONGROUND;
		ent->fields.server->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
		PRVM_G_FLOAT(OFS_RETURN) = 1;
		// if support is destroyed, keep suspended (gross hack for floating items in various maps)
		ent->priv.server->suspendedinairflag = true;
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
	const char	*val;
	client_t	*client;
	int			j;

	style = PRVM_G_FLOAT(OFS_PARM0);
	val = PRVM_G_STRING(OFS_PARM1);

	if( (unsigned) style >= MAX_LIGHTSTYLES ) {
		PRVM_ERROR( "PF_lightstyle: style: %i >= 64", style );
	}

// change the string in sv
	strlcpy(sv.lightstyles[style], val, sizeof(sv.lightstyles[style]));

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

/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = SV_CheckBottom (PRVM_G_EDICT(OFS_PARM0));
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = SV_PointQ1Contents(PRVM_G_VECTOR(OFS_PARM0));
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
	prvm_edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;

	// assume failure if it returns early
	VectorCopy(prog->globals.server->v_forward, PRVM_G_VECTOR(OFS_RETURN));
	// if sv_aim is so high it can't possibly accept anything, skip out early
	if (sv_aim.value >= 1)
		return;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("aim: can not use world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("aim: can not use free entity\n");
	speed = PRVM_G_FLOAT(OFS_PARM1);

	VectorCopy (ent->fields.server->origin, start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (prog->globals.server->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
	if (tr.ent && ((prvm_edict_t *)tr.ent)->fields.server->takedamage == DAMAGE_AIM
	&& (!teamplay.integer || ent->fields.server->team <=0 || ent->fields.server->team != ((prvm_edict_t *)tr.ent)->fields.server->team) )
	{
		VectorCopy (prog->globals.server->v_forward, PRVM_G_VECTOR(OFS_RETURN));
		return;
	}


// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;

	check = PRVM_NEXT_EDICT(prog->edicts);
	for (i=1 ; i<prog->num_edicts ; i++, check = PRVM_NEXT_EDICT(check) )
	{
		prog->xfunction->builtinsprofile++;
		if (check->fields.server->takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.integer && ent->fields.server->team > 0 && ent->fields.server->team == check->fields.server->team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->fields.server->origin[j]
			+ 0.5*(check->fields.server->mins[j] + check->fields.server->maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, prog->globals.server->v_forward);
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
		VectorSubtract (bestent->fields.server->origin, ent->fields.server->origin, dir);
		dist = DotProduct (dir, prog->globals.server->v_forward);
		VectorScale (prog->globals.server->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, PRVM_G_VECTOR(OFS_RETURN));
	}
	else
	{
		VectorCopy (bestdir, PRVM_G_VECTOR(OFS_RETURN));
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
	prvm_edict_t		*ent;
	float		ideal, current, move, speed;

	ent = PRVM_PROG_TO_EDICT(prog->globals.server->self);
	if (ent == prog->edicts)
		PF_WARNING("changeyaw: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("changeyaw: can not modify free entity\n");
	current = ANGLEMOD(ent->fields.server->angles[1]);
	ideal = ent->fields.server->ideal_yaw;
	speed = ent->fields.server->yaw_speed;

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

	ent->fields.server->angles[1] = ANGLEMOD (current + move);
}

/*
==============
PF_changepitch
==============
*/
void PF_changepitch (void)
{
	prvm_edict_t		*ent;
	float		ideal, current, move, speed;
	prvm_eval_t		*val;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("changepitch: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("changepitch: can not modify free entity\n");
	current = ANGLEMOD( ent->fields.server->angles[0] );
	if ((val = PRVM_GETEDICTFIELDVALUE(ent, eval_idealpitch)))
		ideal = val->_float;
	else
	{
		PF_WARNING("PF_changepitch: .float idealpitch and .float pitch_speed must be defined to use changepitch\n");
		return;
	}
	if ((val = PRVM_GETEDICTFIELDVALUE(ent, eval_pitch_speed)))
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

	ent->fields.server->angles[0] = ANGLEMOD (current + move);
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
	prvm_edict_t	*ent;

	dest = PRVM_G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		ent = PRVM_PROG_TO_EDICT(prog->globals.server->msg_entity);
		entnum = PRVM_NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
		{
			Con_Printf ("WriteDest: tried to write to non-client\n");
			return &sv.reliable_datagram;
		}
		else
			return &svs.clients[entnum-1].message;

	default:
		Con_Printf ("WriteDest: bad destination");
	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return &sv.signon;
	}

	return NULL;
}

void PF_WriteByte (void)
{
	MSG_WriteByte (WriteDest(), PRVM_G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
	MSG_WriteChar (WriteDest(), PRVM_G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
	MSG_WriteShort (WriteDest(), PRVM_G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
	MSG_WriteLong (WriteDest(), PRVM_G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
	MSG_WriteAngle (WriteDest(), PRVM_G_FLOAT(OFS_PARM1), sv.protocol);
}

void PF_WriteCoord (void)
{
	MSG_WriteCoord (WriteDest(), PRVM_G_FLOAT(OFS_PARM1), sv.protocol);
}

void PF_WriteString (void)
{
	MSG_WriteString (WriteDest(), PRVM_G_STRING(OFS_PARM1));
}

void PF_WriteUnterminatedString (void)
{
	MSG_WriteUnterminatedString (WriteDest(), PRVM_G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
	MSG_WriteShort (WriteDest(), PRVM_G_EDICTNUM(OFS_PARM1));
}

//////////////////////////////////////////////////////////

void PF_makestatic (void)
{
	prvm_edict_t *ent;
	int i, large;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("makestatic: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("makestatic: can not modify free entity\n");

	large = false;
	if (ent->fields.server->modelindex >= 256 || ent->fields.server->frame >= 256)
		large = true;

	if (large)
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic2);
		MSG_WriteShort (&sv.signon, ent->fields.server->modelindex);
		MSG_WriteShort (&sv.signon, ent->fields.server->frame);
	}
	else
	{
		MSG_WriteByte (&sv.signon,svc_spawnstatic);
		MSG_WriteByte (&sv.signon, ent->fields.server->modelindex);
		MSG_WriteByte (&sv.signon, ent->fields.server->frame);
	}

	MSG_WriteByte (&sv.signon, ent->fields.server->colormap);
	MSG_WriteByte (&sv.signon, ent->fields.server->skin);
	for (i=0 ; i<3 ; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->fields.server->origin[i], sv.protocol);
		MSG_WriteAngle(&sv.signon, ent->fields.server->angles[i], sv.protocol);
	}

// throw the entity away now
	PRVM_ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
	prvm_edict_t	*ent;
	int		i;
	client_t	*client;

	ent = PRVM_G_EDICT(OFS_PARM0);
	i = PRVM_NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients || !svs.clients[i-1].active)
	{
		Con_Print("tried to setspawnparms on a non-client\n");
		return;
	}

	// copy spawn parms out of the client_t
	client = svs.clients + i-1;
	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&prog->globals.server->parm1)[i] = client->spawn_parms[i];
}

/*
=================
PF_getlight

Returns a color vector indicating the lighting at the requested point.

(Internal Operation note: actually measures the light beneath the point, just like
                          the model lighting on the client)

getlight(vector)
=================
*/
void PF_getlight (void)
{
	vec3_t ambientcolor, diffusecolor, diffusenormal;
	vec_t *p;
	p = PRVM_G_VECTOR(OFS_PARM0);
	VectorClear(ambientcolor);
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);
	if (sv.worldmodel && sv.worldmodel->brush.LightPoint)
		sv.worldmodel->brush.LightPoint(sv.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	VectorMA(ambientcolor, 0.5, diffusecolor, PRVM_G_VECTOR(OFS_RETURN));
}

void PF_registercvar (void)
{
	const char *name, *value;
	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_STRING(OFS_PARM1);
	PRVM_G_FLOAT(OFS_RETURN) = 0;

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

	PRVM_G_FLOAT(OFS_RETURN) = 1; // success
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
	prvm_edict_t *in, *out;
	in = PRVM_G_EDICT(OFS_PARM0);
	if (in == prog->edicts)
		PF_WARNING("copyentity: can not read world entity\n");
	if (in->priv.server->free)
		PF_WARNING("copyentity: can not read free entity\n");
	out = PRVM_G_EDICT(OFS_PARM1);
	if (out == prog->edicts)
		PF_WARNING("copyentity: can not modify world entity\n");
	if (out->priv.server->free)
		PF_WARNING("copyentity: can not modify free entity\n");
	memcpy(out->fields.server, in->fields.server, prog->progs->entityfields * 4);
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
	prvm_eval_t *val;

	entnum = PRVM_G_EDICTNUM(OFS_PARM0);
	i = PRVM_G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to setcolor a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	if (client->edict)
	{
		if ((val = PRVM_GETEDICTFIELDVALUE(client->edict, eval_clientcolors)))
			val->_float = i;
		client->edict->fields.server->team = (i & 15) + 1;
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
	const char *s;
	s = PRVM_G_STRING(OFS_PARM1);
	if (!s || !s[0])
		PF_WARNING("effect: no model specified\n");

	i = SV_ModelIndex(s, 1);
	if (!i)
		PF_WARNING("effect: model not precached\n");
	SV_StartEffect(PRVM_G_VECTOR(OFS_PARM0), i, PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3), PRVM_G_FLOAT(OFS_PARM4));
}

void PF_te_blood (void)
{
	if (PRVM_G_FLOAT(OFS_PARM2) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BLOOD);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// velocity
	MSG_WriteByte(&sv.datagram, bound(-128, (int) PRVM_G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) PRVM_G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) PRVM_G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.datagram, bound(0, (int) PRVM_G_FLOAT(OFS_PARM2), 255));
}

void PF_te_bloodshower (void)
{
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BLOODSHOWER);
	// min
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// speed
	MSG_WriteCoord(&sv.datagram, PRVM_G_FLOAT(OFS_PARM2), sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, PRVM_G_FLOAT(OFS_PARM3), 65535));
}

void PF_te_explosionrgb (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSIONRGB);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// color
	MSG_WriteByte(&sv.datagram, bound(0, (int) (PRVM_G_VECTOR(OFS_PARM1)[0] * 255), 255));
	MSG_WriteByte(&sv.datagram, bound(0, (int) (PRVM_G_VECTOR(OFS_PARM1)[1] * 255), 255));
	MSG_WriteByte(&sv.datagram, bound(0, (int) (PRVM_G_VECTOR(OFS_PARM1)[2] * 255), 255));
}

void PF_te_particlecube (void)
{
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLECUBE);
	// min
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// velocity
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, PRVM_G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, PRVM_G_FLOAT(OFS_PARM4));
	// gravity true/false
	MSG_WriteByte(&sv.datagram, ((int) PRVM_G_FLOAT(OFS_PARM5)) != 0);
	// randomvel
	MSG_WriteCoord(&sv.datagram, PRVM_G_FLOAT(OFS_PARM6), sv.protocol);
}

void PF_te_particlerain (void)
{
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLERAIN);
	// min
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// velocity
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, PRVM_G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, PRVM_G_FLOAT(OFS_PARM4));
}

void PF_te_particlesnow (void)
{
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PARTICLESNOW);
	// min
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// max
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// velocity
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
	// count
	MSG_WriteShort(&sv.datagram, bound(0, PRVM_G_FLOAT(OFS_PARM3), 65535));
	// color
	MSG_WriteByte(&sv.datagram, PRVM_G_FLOAT(OFS_PARM4));
}

void PF_te_spark (void)
{
	if (PRVM_G_FLOAT(OFS_PARM2) < 1)
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPARK);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// velocity
	MSG_WriteByte(&sv.datagram, bound(-128, (int) PRVM_G_VECTOR(OFS_PARM1)[0], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) PRVM_G_VECTOR(OFS_PARM1)[1], 127));
	MSG_WriteByte(&sv.datagram, bound(-128, (int) PRVM_G_VECTOR(OFS_PARM1)[2], 127));
	// count
	MSG_WriteByte(&sv.datagram, bound(0, (int) PRVM_G_FLOAT(OFS_PARM2), 255));
}

void PF_te_gunshotquad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_GUNSHOTQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_spikequad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPIKEQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_superspikequad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SUPERSPIKEQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_explosionquad (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSIONQUAD);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_smallflash (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SMALLFLASH);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_customflash (void)
{
	if (PRVM_G_FLOAT(OFS_PARM1) < 8 || PRVM_G_FLOAT(OFS_PARM2) < (1.0 / 256.0))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_CUSTOMFLASH);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// radius
	MSG_WriteByte(&sv.datagram, bound(0, PRVM_G_FLOAT(OFS_PARM1) / 8 - 1, 255));
	// lifetime
	MSG_WriteByte(&sv.datagram, bound(0, PRVM_G_FLOAT(OFS_PARM2) * 256 - 1, 255));
	// color
	MSG_WriteByte(&sv.datagram, bound(0, PRVM_G_VECTOR(OFS_PARM3)[0] * 255, 255));
	MSG_WriteByte(&sv.datagram, bound(0, PRVM_G_VECTOR(OFS_PARM3)[1] * 255, 255));
	MSG_WriteByte(&sv.datagram, bound(0, PRVM_G_VECTOR(OFS_PARM3)[2] * 255, 255));
}

void PF_te_gunshot (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_GUNSHOT);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_spike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_superspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_SUPERSPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_explosion (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_tarexplosion (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_TAREXPLOSION);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_wizspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_WIZSPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_knightspike (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_KNIGHTSPIKE);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_lavasplash (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LAVASPLASH);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_teleport (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_TELEPORT);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_explosion2 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION2);
	// origin
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// color
	MSG_WriteByte(&sv.datagram, PRVM_G_FLOAT(OFS_PARM1));
	MSG_WriteByte(&sv.datagram, PRVM_G_FLOAT(OFS_PARM2));
}

void PF_te_lightning1 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING1);
	// owner entity
	MSG_WriteShort(&sv.datagram, PRVM_G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_lightning2 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING2);
	// owner entity
	MSG_WriteShort(&sv.datagram, PRVM_G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_lightning3 (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_LIGHTNING3);
	// owner entity
	MSG_WriteShort(&sv.datagram, PRVM_G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_beam (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_BEAM);
	// owner entity
	MSG_WriteShort(&sv.datagram, PRVM_G_EDICTNUM(OFS_PARM0));
	// start
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// end
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM2)[2], sv.protocol);
}

void PF_te_plasmaburn (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_PLASMABURN);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
}

void PF_te_flamejet (void)
{
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_FLAMEJET);
	// org
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM0)[2], sv.protocol);
	// vel
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[0], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[1], sv.protocol);
	MSG_WriteCoord(&sv.datagram, PRVM_G_VECTOR(OFS_PARM1)[2], sv.protocol);
	// count
	MSG_WriteByte(&sv.datagram, PRVM_G_FLOAT(OFS_PARM2));
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

static msurface_t *getsurface(prvm_edict_t *ed, int surfacenum)
{
	int modelindex;
	model_t *model;
	if (!ed || ed->priv.server->free)
		return NULL;
	modelindex = ed->fields.server->modelindex;
	if (modelindex < 1 || modelindex >= MAX_MODELS)
		return NULL;
	model = sv.models[modelindex];
	if (surfacenum < 0 || surfacenum >= model->nummodelsurfaces)
		return NULL;
	return model->data_surfaces + surfacenum + model->firstmodelsurface;
}


//PF_getsurfacenumpoints, // #434 float(entity e, float s) getsurfacenumpoints = #434;
void PF_getsurfacenumpoints(void)
{
	msurface_t *surface;
	// return 0 if no such surface
	if (!(surface = getsurface(PRVM_G_EDICT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1))))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// note: this (incorrectly) assumes it is a simple polygon
	PRVM_G_FLOAT(OFS_RETURN) = surface->num_vertices;
}
//PF_getsurfacepoint,     // #435 vector(entity e, float s, float n) getsurfacepoint = #435;
void PF_getsurfacepoint(void)
{
	prvm_edict_t *ed;
	msurface_t *surface;
	int pointnum;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed || ed->priv.server->free)
		return;
	if (!(surface = getsurface(ed, PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// note: this (incorrectly) assumes it is a simple polygon
	pointnum = PRVM_G_FLOAT(OFS_PARM2);
	if (pointnum < 0 || pointnum >= surface->num_vertices)
		return;
	// FIXME: implement rotation/scaling
	VectorAdd(&(surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex)[pointnum * 3], ed->fields.server->origin, PRVM_G_VECTOR(OFS_RETURN));
}
//PF_getsurfacenormal,    // #436 vector(entity e, float s) getsurfacenormal = #436;
void PF_getsurfacenormal(void)
{
	msurface_t *surface;
	vec3_t normal;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	if (!(surface = getsurface(PRVM_G_EDICT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	// note: this (incorrectly) assumes it is a simple polygon
	// note: this only returns the first triangle, so it doesn't work very
	// well for curved surfaces or arbitrary meshes
	TriangleNormal((surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex), (surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex) + 3, (surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex) + 6, normal);
	VectorNormalize(normal);
	VectorCopy(normal, PRVM_G_VECTOR(OFS_RETURN));
}
//PF_getsurfacetexture,   // #437 string(entity e, float s) getsurfacetexture = #437;
void PF_getsurfacetexture(void)
{
	msurface_t *surface;
	PRVM_G_INT(OFS_RETURN) = 0;
	if (!(surface = getsurface(PRVM_G_EDICT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1))))
		return;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(surface->texture->name);
}
//PF_getsurfacenearpoint, // #438 float(entity e, vector p) getsurfacenearpoint = #438;
void PF_getsurfacenearpoint(void)
{
	int surfacenum, best, modelindex;
	vec3_t clipped, p;
	vec_t dist, bestdist;
	prvm_edict_t *ed;
	model_t *model;
	msurface_t *surface;
	vec_t *point;
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	ed = PRVM_G_EDICT(OFS_PARM0);
	point = PRVM_G_VECTOR(OFS_PARM1);

	if (!ed || ed->priv.server->free)
		return;
	modelindex = ed->fields.server->modelindex;
	if (modelindex < 1 || modelindex >= MAX_MODELS)
		return;
	model = sv.models[modelindex];
	if (!model->num_surfaces)
		return;

	// FIXME: implement rotation/scaling
	VectorSubtract(point, ed->fields.server->origin, p);
	best = -1;
	bestdist = 1000000000;
	for (surfacenum = 0;surfacenum < model->nummodelsurfaces;surfacenum++)
	{
		surface = model->data_surfaces + surfacenum + model->firstmodelsurface;
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
	PRVM_G_FLOAT(OFS_RETURN) = best;
}
//PF_getsurfaceclippedpoint, // #439 vector(entity e, float s, vector p) getsurfaceclippedpoint = #439;
void PF_getsurfaceclippedpoint(void)
{
	prvm_edict_t *ed;
	msurface_t *surface;
	vec3_t p, out;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed || ed->priv.server->free)
		return;
	if (!(surface = getsurface(ed, PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	VectorSubtract(PRVM_G_VECTOR(OFS_PARM2), ed->fields.server->origin, p);
	clippointtosurface(surface, p, out);
	// FIXME: implement rotation/scaling
	VectorAdd(out, ed->fields.server->origin, PRVM_G_VECTOR(OFS_RETURN));
}

//void(entity e, string s) clientcommand = #440; // executes a command string as if it came from the specified client
//this function originally written by KrimZon, made shorter by LordHavoc
void PF_clientcommand (void)
{
	client_t *temp_client;
	int i;

	//find client for this entity
	i = (PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(OFS_PARM0)) - 1);
	if (i < 0 || i >= svs.maxclients || !svs.clients[i].active)
	{
		Con_Print("PF_clientcommand: entity is not a client\n");
		return;
	}

	temp_client = host_client;
	host_client = svs.clients + i;
	Cmd_ExecuteString (PRVM_G_STRING(OFS_PARM1), src_client);
	host_client = temp_client;
}

//void(entity e, entity tagentity, string tagname) setattachment = #443; // attachs e to a tag on tagentity (note: use "" to attach to entity origin/angles instead of a tag)
void PF_setattachment (void)
{
	prvm_edict_t *e = PRVM_G_EDICT(OFS_PARM0);
	prvm_edict_t *tagentity = PRVM_G_EDICT(OFS_PARM1);
	const char *tagname = PRVM_G_STRING(OFS_PARM2);
	prvm_eval_t *v;
	int modelindex;
	model_t *model;

	if (e == prog->edicts)
		PF_WARNING("setattachment: can not modify world entity\n");
	if (e->priv.server->free)
		PF_WARNING("setattachment: can not modify free entity\n");

	if (tagentity == NULL)
		tagentity = prog->edicts;

	v = PRVM_GETEDICTFIELDVALUE(e, eval_tag_entity);
	if (v)
		v->edict = PRVM_EDICT_TO_PROG(tagentity);

	v = PRVM_GETEDICTFIELDVALUE(e, eval_tag_index);
	if (v)
		v->_float = 0;
	if (tagentity != NULL && tagentity != prog->edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->fields.server->modelindex;
		if (modelindex >= 0 && modelindex < MAX_MODELS && (model = sv.models[modelindex]))
		{
			v->_float = Mod_Alias_GetTagIndexForName(model, tagentity->fields.server->skin, tagname);
			if (v->_float == 0)
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity));
	}
}

/////////////////////////////////////////
// DP_MD3_TAGINFO extension coded by VorteX

int SV_GetTagIndex (prvm_edict_t *e, const char *tagname)
{
	int i;
	model_t *model;

	i = e->fields.server->modelindex;
	if (i < 1 || i >= MAX_MODELS)
		return -1;
	model = sv.models[i];

	return Mod_Alias_GetTagIndexForName(model, e->fields.server->skin, tagname);
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
int SV_GetTagMatrix (matrix4x4_t *out, prvm_edict_t *ent, int tagindex)
{
	prvm_eval_t *val;
	int modelindex, reqframe, attachloop;
	matrix4x4_t entitymatrix, tagmatrix, attachmatrix;
	prvm_edict_t *attachent;
	model_t *model;

	Matrix4x4_CreateIdentity(out); // warnings and errors return identical matrix

	if (ent == prog->edicts)
		return 1;
	if (ent->priv.server->free)
		return 2;

	modelindex = (int)ent->fields.server->modelindex;
	if (modelindex <= 0 || modelindex > MAX_MODELS)
		return 3;

	model = sv.models[modelindex];

	if (ent->fields.server->frame >= 0 && ent->fields.server->frame < model->numframes && model->animscenes)
		reqframe = model->animscenes[(int)ent->fields.server->frame].firstframe;
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

	if ((val = PRVM_GETEDICTFIELDVALUE(ent, eval_tag_entity)) && val->edict)
	{ // DP_GFX_QUAKE3MODELTAGS, scan all chain and stop on unattached entity
		attachloop = 0;
		do
		{
			attachent = PRVM_EDICT_NUM(val->edict); // to this it entity our entity is attached
			val = PRVM_GETEDICTFIELDVALUE(ent, eval_tag_index);
			if (val->_float >= 1 && attachent->fields.server->modelindex >= 1 && attachent->fields.server->modelindex < MAX_MODELS && (model = sv.models[(int)attachent->fields.server->modelindex]) && model->animscenes && attachent->fields.server->frame >= 0 && attachent->fields.server->frame < model->numframes)
				Mod_Alias_GetTagMatrix(model, model->animscenes[(int)attachent->fields.server->frame].firstframe, val->_float - 1, &attachmatrix);
			else
				Matrix4x4_CreateIdentity(&attachmatrix);

			// apply transformation by child entity matrix
			val = PRVM_GETEDICTFIELDVALUE(ent, eval_scale);
			if (val->_float == 0)
				val->_float = 1;
			Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.server->origin[0], ent->fields.server->origin[1], ent->fields.server->origin[2], -ent->fields.server->angles[0], ent->fields.server->angles[1], ent->fields.server->angles[2], val->_float);
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
		while ((val = PRVM_GETEDICTFIELDVALUE(ent, eval_tag_entity)) && val->edict);
	}

	// normal or RENDER_VIEWMODEL entity (or main parent entity on attach chain)
	val = PRVM_GETEDICTFIELDVALUE(ent, eval_scale);
	if (val->_float == 0)
		val->_float = 1;
	// Alias models have inverse pitch, bmodels can't have tags, so don't check for modeltype...
	Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.server->origin[0], ent->fields.server->origin[1], ent->fields.server->origin[2], -ent->fields.server->angles[0], ent->fields.server->angles[1], ent->fields.server->angles[2], val->_float);
	Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
	out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
	out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
	out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);

	if ((val = PRVM_GETEDICTFIELDVALUE(ent, eval_viewmodelforclient)) && val->edict)
	{// RENDER_VIEWMODEL magic
		Matrix4x4_Copy(&tagmatrix, out);
		ent = PRVM_EDICT_NUM(val->edict);

		val = PRVM_GETEDICTFIELDVALUE(ent, eval_scale);
		if (val->_float == 0)
			val->_float = 1;

		Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.server->origin[0], ent->fields.server->origin[1], ent->fields.server->origin[2] + ent->fields.server->view_ofs[2], ent->fields.server->v_angle[0], ent->fields.server->v_angle[1], ent->fields.server->v_angle[2], val->_float);
		Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
		out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
		out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
		out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);

		/*
		// Cl_bob, ported from rendering code
		if (ent->fields.server->health > 0 && cl_bob.value && cl_bobcycle.value)
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
			bob = sqrt(ent->fields.server->velocity[0]*ent->fields.server->velocity[0] + ent->fields.server->velocity[1]*ent->fields.server->velocity[1])*cl_bob.value;
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
	prvm_edict_t *ent = PRVM_G_EDICT(OFS_PARM0);
	const char *tag_name = PRVM_G_STRING(OFS_PARM1);
	int modelindex, tag_index;

	if (ent == prog->edicts)
		PF_WARNING("gettagindex: can't affect world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("gettagindex: can't affect free entity\n");

	modelindex = (int)ent->fields.server->modelindex;
	tag_index = 0;
	if (modelindex <= 0 || modelindex > MAX_MODELS)
		Con_DPrintf("gettagindex(entity #%i): null or non-precached model\n", PRVM_NUM_FOR_EDICT(ent));
	else
	{
		tag_index = SV_GetTagIndex(ent, tag_name);
		if (tag_index == 0)
			Con_DPrintf("gettagindex(entity #%i): tag \"%s\" not found\n", PRVM_NUM_FOR_EDICT(ent), tag_name);
	}
	PRVM_G_FLOAT(OFS_RETURN) = tag_index;
};

//vector(entity ent, float tagindex) gettaginfo;
void PF_gettaginfo (void)
{
	prvm_edict_t *e = PRVM_G_EDICT(OFS_PARM0);
	int tagindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	matrix4x4_t tag_matrix;
	int returncode;

	returncode = SV_GetTagMatrix(&tag_matrix, e, tagindex);
	Matrix4x4_ToVectors(&tag_matrix, prog->globals.server->v_forward, prog->globals.server->v_right, prog->globals.server->v_up, PRVM_G_VECTOR(OFS_RETURN));

	switch(returncode)
	{
		case 1:
			PF_WARNING("gettagindex: can't affect world entity\n");
			break;
		case 2:
			PF_WARNING("gettagindex: can't affect free entity\n");
			break;
		case 3:
			Con_DPrintf("SV_GetTagMatrix(entity #%i): null or non-precached model\n", PRVM_NUM_FOR_EDICT(e));
			break;
		case 4:
			Con_DPrintf("SV_GetTagMatrix(entity #%i): model has no tag with requested index %i\n", PRVM_NUM_FOR_EDICT(e), tagindex);
			break;
		case 5:
			Con_DPrintf("SV_GetTagMatrix(entity #%i): runaway loop at attachment chain\n", PRVM_NUM_FOR_EDICT(e));
			break;
	}
}

//void(entity clent) dropclient (DP_SV_DROPCLIENT)
void PF_dropclient (void)
{
	int clientnum;
	client_t *oldhostclient;
	clientnum = PRVM_G_EDICTNUM(OFS_PARM0) - 1;
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
	prvm_edict_t	*ed;
	prog->xfunction->builtinsprofile += 2;
	ed = prog->edicts;
	for (i = 0;i < svs.maxclients;i++)
	{
		if (!svs.clients[i].active)
		{
			prog->xfunction->builtinsprofile += 100;
			SV_ConnectClient (i, NULL);
			ed = PRVM_EDICT_NUM(i + 1);
			break;
		}
	}
	VM_RETURN_EDICT(ed);
}

//float(entity clent) clienttype (DP_SV_BOTCLIENT)
void PF_clienttype (void)
{
	int clientnum;
	clientnum = PRVM_G_EDICTNUM(OFS_PARM0) - 1;
	if (clientnum < 0 || clientnum >= svs.maxclients)
		PRVM_G_FLOAT(OFS_RETURN) = 3;
	else if (!svs.clients[clientnum].active)
		PRVM_G_FLOAT(OFS_RETURN) = 0;
	else if (svs.clients[clientnum].netconnection)
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = 2;
}

prvm_builtin_t vm_sv_builtins[] = {
NULL,						// #0
PF_makevectors,				// #1 void(entity e) makevectors
PF_setorigin,				// #2 void(entity e, vector o) setorigin
PF_setmodel,				// #3 void(entity e, string m) setmodel
PF_setsize,					// #4 void(entity e, vector min, vector max) setsize
NULL,						// #5 void(entity e, vector min, vector max) setabssize
VM_break,					// #6 void() break
VM_random,					// #7 float() random
PF_sound,					// #8 void(entity e, float chan, string samp) sound
VM_normalize,				// #9 vector(vector v) normalize
VM_error,					// #10 void(string e) error
VM_objerror,				// #11 void(string e) objerror
VM_vlen,					// #12 float(vector v) vlen
VM_vectoyaw,				// #13 float(vector v) vectoyaw
VM_spawn,					// #14 entity() spawn
VM_remove,					// #15 void(entity e) remove
PF_traceline,				// #16 float(vector v1, vector v2, float tryents) traceline
PF_checkclient,				// #17 entity() clientlist
VM_find,					// #18 entity(entity start, .string fld, string match) find
PF_precache_sound,			// #19 void(string s) precache_sound
PF_precache_model,			// #20 void(string s) precache_model
PF_stuffcmd,				// #21 void(entity client, string s)stuffcmd
PF_findradius,				// #22 entity(vector org, float rad) findradius
VM_bprint,					// #23 void(string s) bprint
PF_sprint,					// #24 void(entity client, string s) sprint
VM_dprint,					// #25 void(string s) dprint
VM_ftos,					// #26 void(string s) ftos
VM_vtos,					// #27 void(string s) vtos
VM_coredump,				// #28 void() coredump
VM_traceon,					// #29 void() traceon
VM_traceoff,				// #30 void() traceoff
VM_eprint,					// #31 void(entity e) eprint
PF_walkmove,				// #32 float(float yaw, float dist) walkmove
NULL,						// #33
PF_droptofloor,				// #34 float() droptofloor
PF_lightstyle,				// #35 void(float style, string value) lightstyle
VM_rint,					// #36 float(float v) rint
VM_floor,					// #37 float(float v) floor
VM_ceil,					// #38 float(float v) ceil
NULL,						// #39
PF_checkbottom,				// #40 float(entity e) checkbottom
PF_pointcontents,			// #41 float(vector v) pointcontents
NULL,						// #42
VM_fabs,					// #43 float(float f) fabs
PF_aim,						// #44 vector(entity e, float speed) aim
VM_cvar,					// #45 float(string s) cvar
VM_localcmd,				// #46 void(string s) localcmd
VM_nextent,					// #47 entity(entity e) nextent
PF_particle,				// #48 void(vector o, vector d, float color, float count) particle
PF_changeyaw,				// #49 void() ChangeYaw
NULL,						// #50
VM_vectoangles,				// #51 vector(vector v) vectoangles
PF_WriteByte,				// #52 void(float to, float f) WriteByte
PF_WriteChar,				// #53 void(float to, float f) WriteChar
PF_WriteShort,				// #54 void(float to, float f) WriteShort
PF_WriteLong,				// #55 void(float to, float f) WriteLong
PF_WriteCoord,				// #56 void(float to, float f) WriteCoord
PF_WriteAngle,				// #57 void(float to, float f) WriteAngle
PF_WriteString,				// #58 void(float to, string s) WriteString
PF_WriteEntity,				// #59 void(float to, entity e) WriteEntity
VM_sin,						// #60 float(float f) sin (DP_QC_SINCOSSQRTPOW)
VM_cos,						// #61 float(float f) cos (DP_QC_SINCOSSQRTPOW)
VM_sqrt,					// #62 float(float f) sqrt (DP_QC_SINCOSSQRTPOW)
PF_changepitch,				// #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
PF_tracetoss,				// #64 void(entity e, entity ignore) tracetoss (DP_QC_TRACETOSS)
VM_etos,					// #65 string(entity ent) etos (DP_QC_ETOS)
NULL,						// #66
SV_MoveToGoal,				// #67 void(float step) movetogoal
PF_precache_file,			// #68 string(string s) precache_file
PF_makestatic,				// #69 void(entity e) makestatic
VM_changelevel,				// #70 void(string s) changelevel
NULL,						// #71
VM_cvar_set,				// #72 void(string var, string val) cvar_set
PF_centerprint,				// #73 void(entity client, strings) centerprint
PF_ambientsound,			// #74 void(vector pos, string samp, float vol, float atten) ambientsound
PF_precache_model,			// #75 string(string s) precache_model2
PF_precache_sound,			// #76 string(string s) precache_sound2
PF_precache_file,			// #77 string(string s) precache_file2
PF_setspawnparms,			// #78 void(entity e) setspawnparms
NULL,						// #79
NULL,						// #80
VM_stof,					// #81 float(string s) stof (FRIK_FILE)
NULL,						// #82
NULL,						// #83
NULL,						// #84
NULL,						// #85
NULL,						// #86
NULL,						// #87
NULL,						// #88
NULL,						// #89
PF_tracebox,				// #90 void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox (DP_QC_TRACEBOX)
VM_randomvec,				// #91 vector() randomvec (DP_QC_RANDOMVEC)
PF_getlight,				// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
PF_registercvar,			// #93 float(string name, string value) registercvar (DP_REGISTERCVAR)
VM_min,						// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
VM_max,						// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
VM_bound,					// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
VM_pow,						// #97 float(float f, float f) pow (DP_QC_SINCOSSQRTPOW)
VM_findfloat,				// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
VM_checkextension,			// #99 float(string s) checkextension (the basis of the extension system)
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
VM_fopen,					// #110 float(string filename, float mode) fopen (FRIK_FILE)
VM_fclose,					// #111 void(float fhandle) fclose (FRIK_FILE)
VM_fgets,					// #112 string(float fhandle) fgets (FRIK_FILE)
VM_fputs,					// #113 void(float fhandle, string s) fputs (FRIK_FILE)
VM_strlen,					// #114 float(string s) strlen (FRIK_FILE)
VM_strcat,					// #115 string(string s1, string s2) strcat (FRIK_FILE)
VM_substring,				// #116 string(string s, float start, float length) substring (FRIK_FILE)
VM_stov,					// #117 vector(string) stov (FRIK_FILE)
VM_strzone,					// #118 string(string s) strzone (FRIK_FILE)
VM_strunzone,				// #119 void(string s) strunzone (FRIK_FILE)
e10, e10, e10, e10, e10, e10, e10, e10,		// #120-199
e10, e10, e10, e10, e10, e10, e10, e10, e10, e10,	// #200-299
e10, e10, e10, e10, e10, e10, e10, e10, e10, e10,	// #300-399
VM_copyentity,				// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
PF_setcolor,				// #401 void(entity ent, float colors) setcolor (DP_QC_SETCOLOR)
VM_findchain,				// #402 entity(.string fld, string match) findchain (DP_QC_FINDCHAIN)
VM_findchainfloat,			// #403 entity(.float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
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
VM_tokenize,				// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_argv,					// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_setattachment,			// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
VM_search_begin,			// #444 float(string pattern, float caseinsensitive, float quiet) search_begin (DP_FS_SEARCH)
VM_search_end,				// #445 void(float handle) search_end (DP_FS_SEARCH)
VM_search_getsize,			// #446 float(float handle) search_getsize (DP_FS_SEARCH)
VM_search_getfilename,		// #447 string(float handle, float num) search_getfilename (DP_FS_SEARCH)
VM_cvar_string,				// #448 string(string s) cvar_string (DP_QC_CVAR_STRING)
VM_findflags,				// #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
VM_findchainflags,			// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
PF_gettagindex,				// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
PF_gettaginfo,				// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
PF_dropclient,				// #453 void(entity clent) dropclient (DP_SV_DROPCLIENT)
PF_spawnclient,				// #454 entity() spawnclient (DP_SV_BOTCLIENT)
PF_clienttype,				// #455 float(entity clent) clienttype (DP_SV_BOTCLIENT)
PF_WriteUnterminatedString,	// #456 void(float to, string s) WriteUnterminatedString (DP_SV_WRITEUNTERMINATEDSTRING)
PF_te_flamejet,				// #457 void(vector org, vector vel, float howmany) te_flamejet = #457 (DP_TE_FLAMEJET)
NULL,						// #458
NULL,						// #459
e10, e10, e10, e10			// #460-499 (LordHavoc)
};

const int vm_sv_numbuiltins = sizeof(vm_sv_builtins) / sizeof(prvm_builtin_t);

void VM_SV_Cmd_Init(void)
{
	VM_Cmd_Init();
}

void VM_SV_Cmd_Reset(void)
{
	VM_Cmd_Reset();
}

