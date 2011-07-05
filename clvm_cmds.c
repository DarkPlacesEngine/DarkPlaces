#include "quakedef.h"

#include "prvm_cmds.h"
#include "csprogs.h"
#include "cl_collision.h"
#include "r_shadow.h"
#include "jpeg.h"
#include "image.h"

//============================================================================
// Client
//[515]: unsolved PROBLEMS
//- finish player physics code (cs_runplayerphysics)
//- EntWasFreed ?
//- RF_DEPTHHACK is not like it should be
//- add builtin that sets cl.viewangles instead of reading "input_angles" global
//- finish lines support for R_Polygon***
//- insert selecttraceline into traceline somehow

//4 feature darkplaces csqc: add builtin to clientside qc for reading triangles of model meshes (useful to orient a ui along a triangle of a model mesh)
//4 feature darkplaces csqc: add builtins to clientside qc for gl calls

extern cvar_t v_flipped;
extern cvar_t r_equalize_entities_fullbright;

sfx_t *S_FindName(const char *name);
int Sbar_GetSortedPlayerIndex (int index);
void Sbar_SortFrags (void);
void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius);
void CSQC_RelinkAllEntities (int drawmask);
void CSQC_RelinkCSQCEntities (void);

// #1 void(vector ang) makevectors
static void VM_CL_makevectors (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_makevectors);
	AngleVectors (PRVM_G_VECTOR(OFS_PARM0), PRVM_clientglobalvector(v_forward), PRVM_clientglobalvector(v_right), PRVM_clientglobalvector(v_up));
}

// #2 void(entity e, vector o) setorigin
void VM_CL_setorigin (void)
{
	prvm_edict_t	*e;
	float	*org;
	VM_SAFEPARMCOUNT(2, VM_CL_setorigin);

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
	{
		VM_Warning("setorigin: can not modify world entity\n");
		return;
	}
	if (e->priv.required->free)
	{
		VM_Warning("setorigin: can not modify free entity\n");
		return;
	}
	org = PRVM_G_VECTOR(OFS_PARM1);
	VectorCopy (org, PRVM_clientedictvector(e, origin));
	CL_LinkEdict(e);
}

static void SetMinMaxSize (prvm_edict_t *e, float *min, float *max)
{
	int		i;

	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PRVM_ERROR("SetMinMaxSize: backwards mins/maxs");

	// set derived values
	VectorCopy (min, PRVM_clientedictvector(e, mins));
	VectorCopy (max, PRVM_clientedictvector(e, maxs));
	VectorSubtract (max, min, PRVM_clientedictvector(e, size));

	CL_LinkEdict (e);
}

// #3 void(entity e, string m) setmodel
void VM_CL_setmodel (void)
{
	prvm_edict_t	*e;
	const char		*m;
	dp_model_t *mod;
	int				i;

	VM_SAFEPARMCOUNT(2, VM_CL_setmodel);

	e = PRVM_G_EDICT(OFS_PARM0);
	PRVM_clientedictfloat(e, modelindex) = 0;
	PRVM_clientedictstring(e, model) = 0;

	m = PRVM_G_STRING(OFS_PARM1);
	mod = NULL;
	for (i = 0;i < MAX_MODELS && cl.csqc_model_precache[i];i++)
	{
		if (!strcmp(cl.csqc_model_precache[i]->name, m))
		{
			mod = cl.csqc_model_precache[i];
			PRVM_clientedictstring(e, model) = PRVM_SetEngineString(mod->name);
			PRVM_clientedictfloat(e, modelindex) = -(i+1);
			break;
		}
	}

	if( !mod ) {
		for (i = 0;i < MAX_MODELS;i++)
		{
			mod = cl.model_precache[i];
			if (mod && !strcmp(mod->name, m))
			{
				PRVM_clientedictstring(e, model) = PRVM_SetEngineString(mod->name);
				PRVM_clientedictfloat(e, modelindex) = i;
				break;
			}
		}
	}

	if( mod ) {
		// TODO: check if this breaks needed consistency and maybe add a cvar for it too?? [1/10/2008 Black]
		//SetMinMaxSize (e, mod->normalmins, mod->normalmaxs);
	}
	else
	{
		SetMinMaxSize (e, vec3_origin, vec3_origin);
		VM_Warning ("setmodel: model '%s' not precached\n", m);
	}
}

// #4 void(entity e, vector min, vector max) setsize
static void VM_CL_setsize (void)
{
	prvm_edict_t	*e;
	float			*min, *max;
	VM_SAFEPARMCOUNT(3, VM_CL_setsize);

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
	{
		VM_Warning("setsize: can not modify world entity\n");
		return;
	}
	if (e->priv.server->free)
	{
		VM_Warning("setsize: can not modify free entity\n");
		return;
	}
	min = PRVM_G_VECTOR(OFS_PARM1);
	max = PRVM_G_VECTOR(OFS_PARM2);

	SetMinMaxSize( e, min, max );

	CL_LinkEdict(e);
}

// #8 void(entity e, float chan, string samp, float volume, float atten) sound
static void VM_CL_sound (void)
{
	const char			*sample;
	int					channel;
	prvm_edict_t		*entity;
	float 				volume;
	float				attenuation;
	float pitchchange;
	int flags;
	vec3_t				org;

	VM_SAFEPARMCOUNTRANGE(5, 7, VM_CL_sound);

	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = (int)PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = PRVM_G_FLOAT(OFS_PARM3);
	attenuation = PRVM_G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 1)
	{
		VM_Warning("VM_CL_sound: volume must be in range 0-1\n");
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		VM_Warning("VM_CL_sound: attenuation must be in range 0-4\n");
		return;
	}

	if (prog->argc < 6)
		pitchchange = 0;
	else
		pitchchange = PRVM_G_FLOAT(OFS_PARM5);
	// ignoring prog->argc < 7 for now (no flags supported yet)

	if (prog->argc < 7)
		flags = 0;
	else
		flags = PRVM_G_FLOAT(OFS_PARM6);

	channel = CHAN_USER2ENGINE(channel);

	if (!IS_CHAN(channel))
	{
		VM_Warning("VM_CL_sound: channel must be in range 0-127\n");
		return;
	}

	CL_VM_GetEntitySoundOrigin(MAX_EDICTS + PRVM_NUM_FOR_EDICT(entity), org);
	S_StartSound(MAX_EDICTS + PRVM_NUM_FOR_EDICT(entity), channel, S_FindName(sample), org, volume, attenuation);
}

// #483 void(vector origin, string sample, float volume, float attenuation) pointsound
static void VM_CL_pointsound(void)
{
	const char			*sample;
	float 				volume;
	float				attenuation;
	vec3_t				org;

	VM_SAFEPARMCOUNT(4, VM_CL_pointsound);

	VectorCopy( PRVM_G_VECTOR(OFS_PARM0), org);
	sample = PRVM_G_STRING(OFS_PARM1);
	volume = PRVM_G_FLOAT(OFS_PARM2);
	attenuation = PRVM_G_FLOAT(OFS_PARM3);

	if (volume < 0 || volume > 1)
	{
		VM_Warning("VM_CL_pointsound: volume must be in range 0-1\n");
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		VM_Warning("VM_CL_pointsound: attenuation must be in range 0-4\n");
		return;
	}

	// Send World Entity as Entity to Play Sound (for CSQC, that is MAX_EDICTS)
	S_StartSound(MAX_EDICTS, 0, S_FindName(sample), org, volume, attenuation);
}

// #14 entity() spawn
static void VM_CL_spawn (void)
{
	prvm_edict_t *ed;
	ed = PRVM_ED_Alloc();
	VM_RETURN_EDICT(ed);
}

void CL_VM_SetTraceGlobals(const trace_t *trace, int svent)
{
	VM_SetTraceGlobals(trace);
	PRVM_clientglobalfloat(trace_networkentity) = svent;
}

#define CL_HitNetworkBrushModels(move) !((move) == MOVE_WORLDONLY)
#define CL_HitNetworkPlayers(move)     !((move) == MOVE_WORLDONLY || (move) == MOVE_NOMONSTERS)

// #16 void(vector v1, vector v2, float movetype, entity ignore) traceline
static void VM_CL_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		move, svent;
	prvm_edict_t	*ent;

//	R_TimeReport("pretraceline");

	VM_SAFEPARMCOUNTRANGE(4, 4, VM_CL_traceline);

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);
	move = (int)PRVM_G_FLOAT(OFS_PARM2);
	ent = PRVM_G_EDICT(OFS_PARM3);

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v2[1]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in traceline('%f %f %f', '%f %f %f', %i, entity %i)\n", PRVM_NAME, v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], move, PRVM_EDICT_TO_PROG(ent));

	trace = CL_TraceLine(v1, v2, move, ent, CL_GenericHitSuperContentsMask(ent), CL_HitNetworkBrushModels(move), CL_HitNetworkPlayers(move), &svent, true, false);

	CL_VM_SetTraceGlobals(&trace, svent);
//	R_TimeReport("traceline");
}

/*
=================
VM_CL_tracebox

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

tracebox (vector1, vector mins, vector maxs, vector2, tryents)
=================
*/
// LordHavoc: added this for my own use, VERY useful, similar to traceline
static void VM_CL_tracebox (void)
{
	float	*v1, *v2, *m1, *m2;
	trace_t	trace;
	int		move, svent;
	prvm_edict_t	*ent;

//	R_TimeReport("pretracebox");
	VM_SAFEPARMCOUNTRANGE(6, 8, VM_CL_tracebox); // allow more parameters for future expansion

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	m1 = PRVM_G_VECTOR(OFS_PARM1);
	m2 = PRVM_G_VECTOR(OFS_PARM2);
	v2 = PRVM_G_VECTOR(OFS_PARM3);
	move = (int)PRVM_G_FLOAT(OFS_PARM4);
	ent = PRVM_G_EDICT(OFS_PARM5);

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v2[1]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in tracebox('%f %f %f', '%f %f %f', '%f %f %f', '%f %f %f', %i, entity %i)\n", PRVM_NAME, v1[0], v1[1], v1[2], m1[0], m1[1], m1[2], m2[0], m2[1], m2[2], v2[0], v2[1], v2[2], move, PRVM_EDICT_TO_PROG(ent));

	trace = CL_TraceBox(v1, m1, m2, v2, move, ent, CL_GenericHitSuperContentsMask(ent), CL_HitNetworkBrushModels(move), CL_HitNetworkPlayers(move), &svent, true);

	CL_VM_SetTraceGlobals(&trace, svent);
//	R_TimeReport("tracebox");
}

trace_t CL_Trace_Toss (prvm_edict_t *tossent, prvm_edict_t *ignore, int *svent)
{
	int i;
	float gravity;
	vec3_t move, end;
	vec3_t original_origin;
	vec3_t original_velocity;
	vec3_t original_angles;
	vec3_t original_avelocity;
	trace_t trace;

	VectorCopy(PRVM_clientedictvector(tossent, origin)   , original_origin   );
	VectorCopy(PRVM_clientedictvector(tossent, velocity) , original_velocity );
	VectorCopy(PRVM_clientedictvector(tossent, angles)   , original_angles   );
	VectorCopy(PRVM_clientedictvector(tossent, avelocity), original_avelocity);

	gravity = PRVM_clientedictfloat(tossent, gravity);
	if (!gravity)
		gravity = 1.0f;
	gravity *= cl.movevars_gravity * 0.05;

	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		PRVM_clientedictvector(tossent, velocity)[2] -= gravity;
		VectorMA (PRVM_clientedictvector(tossent, angles), 0.05, PRVM_clientedictvector(tossent, avelocity), PRVM_clientedictvector(tossent, angles));
		VectorScale (PRVM_clientedictvector(tossent, velocity), 0.05, move);
		VectorAdd (PRVM_clientedictvector(tossent, origin), move, end);
		trace = CL_TraceBox(PRVM_clientedictvector(tossent, origin), PRVM_clientedictvector(tossent, mins), PRVM_clientedictvector(tossent, maxs), end, MOVE_NORMAL, tossent, CL_GenericHitSuperContentsMask(tossent), true, true, NULL, true);
		VectorCopy (trace.endpos, PRVM_clientedictvector(tossent, origin));

		if (trace.fraction < 1)
			break;
	}

	VectorCopy(original_origin   , PRVM_clientedictvector(tossent, origin)   );
	VectorCopy(original_velocity , PRVM_clientedictvector(tossent, velocity) );
	VectorCopy(original_angles   , PRVM_clientedictvector(tossent, angles)   );
	VectorCopy(original_avelocity, PRVM_clientedictvector(tossent, avelocity));

	return trace;
}

static void VM_CL_tracetoss (void)
{
	trace_t	trace;
	prvm_edict_t	*ent;
	prvm_edict_t	*ignore;
	int svent = 0;

	prog->xfunction->builtinsprofile += 600;

	VM_SAFEPARMCOUNT(2, VM_CL_tracetoss);

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("tracetoss: can not use world entity\n");
		return;
	}
	ignore = PRVM_G_EDICT(OFS_PARM1);

	trace = CL_Trace_Toss (ent, ignore, &svent);

	CL_VM_SetTraceGlobals(&trace, svent);
}


// #20 void(string s) precache_model
void VM_CL_precache_model (void)
{
	const char	*name;
	int			i;
	dp_model_t		*m;

	VM_SAFEPARMCOUNT(1, VM_CL_precache_model);

	name = PRVM_G_STRING(OFS_PARM0);
	for (i = 0;i < MAX_MODELS && cl.csqc_model_precache[i];i++)
	{
		if(!strcmp(cl.csqc_model_precache[i]->name, name))
		{
			PRVM_G_FLOAT(OFS_RETURN) = -(i+1);
			return;
		}
	}
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	m = Mod_ForName(name, false, false, name[0] == '*' ? cl.model_name[1] : NULL);
	if(m && m->loaded)
	{
		for (i = 0;i < MAX_MODELS;i++)
		{
			if (!cl.csqc_model_precache[i])
			{
				cl.csqc_model_precache[i] = (dp_model_t*)m;
				PRVM_G_FLOAT(OFS_RETURN) = -(i+1);
				return;
			}
		}
		VM_Warning("VM_CL_precache_model: no free models\n");
		return;
	}
	VM_Warning("VM_CL_precache_model: model \"%s\" not found\n", name);
}

int CSQC_EntitiesInBox (vec3_t mins, vec3_t maxs, int maxlist, prvm_edict_t **list)
{
	prvm_edict_t	*ent;
	int				i, k;

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for(k=0,i=1; i<prog->num_edicts ;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (ent->priv.required->free)
			continue;
		if(BoxesOverlap(mins, maxs, PRVM_clientedictvector(ent, absmin), PRVM_clientedictvector(ent, absmax)))
			list[k++] = ent;
	}
	return k;
}

// #22 entity(vector org, float rad) findradius
static void VM_CL_findradius (void)
{
	prvm_edict_t	*ent, *chain;
	vec_t			radius, radius2;
	vec3_t			org, eorg, mins, maxs;
	int				i, numtouchedicts;
	static prvm_edict_t	*touchedicts[MAX_EDICTS];
	int             chainfield;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_CL_findradius);

	if(prog->argc == 3)
		chainfield = PRVM_G_INT(OFS_PARM2);
	else
		chainfield = prog->fieldoffsets.chain;
	if(chainfield < 0)
		PRVM_ERROR("VM_findchain: %s doesnt have the specified chain field !", PRVM_NAME);

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
	numtouchedicts = CSQC_EntitiesInBox(mins, maxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens	//[515]: for what then ?
		Con_Printf("CSQC_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	for (i = 0;i < numtouchedicts;i++)
	{
		ent = touchedicts[i];
		// Quake did not return non-solid entities but darkplaces does
		// (note: this is the reason you can't blow up fallen zombies)
		if (PRVM_clientedictfloat(ent, solid) == SOLID_NOT && !sv_gameplayfix_blowupfallenzombies.integer)
			continue;
		// LordHavoc: compare against bounding box rather than center so it
		// doesn't miss large objects, and use DotProduct instead of Length
		// for a major speedup
		VectorSubtract(org, PRVM_clientedictvector(ent, origin), eorg);
		if (sv_gameplayfix_findradiusdistancetobox.integer)
		{
			eorg[0] -= bound(PRVM_clientedictvector(ent, mins)[0], eorg[0], PRVM_clientedictvector(ent, maxs)[0]);
			eorg[1] -= bound(PRVM_clientedictvector(ent, mins)[1], eorg[1], PRVM_clientedictvector(ent, maxs)[1]);
			eorg[2] -= bound(PRVM_clientedictvector(ent, mins)[2], eorg[2], PRVM_clientedictvector(ent, maxs)[2]);
		}
		else
			VectorMAMAM(1, eorg, -0.5f, PRVM_clientedictvector(ent, mins), -0.5f, PRVM_clientedictvector(ent, maxs), eorg);
		if (DotProduct(eorg, eorg) < radius2)
		{
			PRVM_EDICTFIELDEDICT(ent, chainfield) = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	VM_RETURN_EDICT(chain);
}

// #34 float() droptofloor
static void VM_CL_droptofloor (void)
{
	prvm_edict_t		*ent;
	vec3_t				end;
	trace_t				trace;

	VM_SAFEPARMCOUNTRANGE(0, 2, VM_CL_droptofloor); // allow 2 parameters because the id1 defs.qc had an incorrect prototype

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(PRVM_clientglobaledict(self));
	if (ent == prog->edicts)
	{
		VM_Warning("droptofloor: can not modify world entity\n");
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("droptofloor: can not modify free entity\n");
		return;
	}

	VectorCopy (PRVM_clientedictvector(ent, origin), end);
	end[2] -= 256;

	trace = CL_TraceBox(PRVM_clientedictvector(ent, origin), PRVM_clientedictvector(ent, mins), PRVM_clientedictvector(ent, maxs), end, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

	if (trace.fraction != 1)
	{
		VectorCopy (trace.endpos, PRVM_clientedictvector(ent, origin));
		PRVM_clientedictfloat(ent, flags) = (int)PRVM_clientedictfloat(ent, flags) | FL_ONGROUND;
		PRVM_clientedictedict(ent, groundentity) = PRVM_EDICT_TO_PROG(trace.ent);
		PRVM_G_FLOAT(OFS_RETURN) = 1;
		// if support is destroyed, keep suspended (gross hack for floating items in various maps)
//		ent->priv.server->suspendedinairflag = true;
	}
}

// #35 void(float style, string value) lightstyle
static void VM_CL_lightstyle (void)
{
	int			i;
	const char	*c;

	VM_SAFEPARMCOUNT(2, VM_CL_lightstyle);

	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	c = PRVM_G_STRING(OFS_PARM1);
	if (i >= cl.max_lightstyle)
	{
		VM_Warning("VM_CL_lightstyle >= MAX_LIGHTSTYLES\n");
		return;
	}
	strlcpy (cl.lightstyle[i].map, c, sizeof (cl.lightstyle[i].map));
	cl.lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
	cl.lightstyle[i].length = (int)strlen(cl.lightstyle[i].map);
}

// #40 float(entity e) checkbottom
static void VM_CL_checkbottom (void)
{
	static int		cs_yes, cs_no;
	prvm_edict_t	*ent;
	vec3_t			mins, maxs, start, stop;
	trace_t			trace;
	int				x, y;
	float			mid, bottom;

	VM_SAFEPARMCOUNT(1, VM_CL_checkbottom);
	ent = PRVM_G_EDICT(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	VectorAdd (PRVM_clientedictvector(ent, origin), PRVM_clientedictvector(ent, mins), mins);
	VectorAdd (PRVM_clientedictvector(ent, origin), PRVM_clientedictvector(ent, maxs), maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (!(CL_PointSuperContents(start) & (SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY)))
				goto realcheck;
		}

	cs_yes++;
	PRVM_G_FLOAT(OFS_RETURN) = true;
	return;		// we got out easy

realcheck:
	cs_no++;
//
// check it for real...
//
	start[2] = mins[2];

// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*sv_stepheight.value;
	trace = CL_TraceLine(start, stop, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true, false);

	if (trace.fraction == 1.0)
		return;

	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = CL_TraceLine(start, stop, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true, false);

			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > sv_stepheight.value)
				return;
		}

	cs_yes++;
	PRVM_G_FLOAT(OFS_RETURN) = true;
}

// #41 float(vector v) pointcontents
static void VM_CL_pointcontents (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_pointcontents);
	PRVM_G_FLOAT(OFS_RETURN) = Mod_Q1BSP_NativeContentsFromSuperContents(NULL, CL_PointSuperContents(PRVM_G_VECTOR(OFS_PARM0)));
}

// #48 void(vector o, vector d, float color, float count) particle
static void VM_CL_particle (void)
{
	float	*org, *dir;
	int		count;
	unsigned char	color;
	VM_SAFEPARMCOUNT(4, VM_CL_particle);

	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	color = (int)PRVM_G_FLOAT(OFS_PARM2);
	count = (int)PRVM_G_FLOAT(OFS_PARM3);
	CL_ParticleEffect(EFFECT_SVC_PARTICLE, count, org, org, dir, dir, NULL, color);
}

// #74 void(vector pos, string samp, float vol, float atten) ambientsound
static void VM_CL_ambientsound (void)
{
	float	*f;
	sfx_t	*s;
	VM_SAFEPARMCOUNT(4, VM_CL_ambientsound);
	s = S_FindName(PRVM_G_STRING(OFS_PARM0));
	f = PRVM_G_VECTOR(OFS_PARM1);
	S_StaticSound (s, f, PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3)*64);
}

// #92 vector(vector org[, float lpflag]) getlight (DP_QC_GETLIGHT)
static void VM_CL_getlight (void)
{
	vec3_t ambientcolor, diffusecolor, diffusenormal;
	vec_t *p;

	VM_SAFEPARMCOUNTRANGE(1, 2, VM_CL_getlight);

	p = PRVM_G_VECTOR(OFS_PARM0);
	VectorClear(ambientcolor);
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);
	if (prog->argc >= 2)
		R_CompleteLightPoint(ambientcolor, diffusecolor, diffusenormal, p, PRVM_G_FLOAT(OFS_PARM1));
	else if (cl.worldmodel && cl.worldmodel->brush.LightPoint)
		cl.worldmodel->brush.LightPoint(cl.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	VectorMA(ambientcolor, 0.5, diffusecolor, PRVM_G_VECTOR(OFS_RETURN));
}

//============================================================================
//[515]: SCENE MANAGER builtins
extern qboolean CSQC_AddRenderEdict (prvm_edict_t *ed, int edictnum);//csprogs.c

void CSQC_R_RecalcView (void)
{
	extern matrix4x4_t viewmodelmatrix_nobob;
	extern matrix4x4_t viewmodelmatrix_withbob;
	Matrix4x4_CreateFromQuakeEntity(&r_refdef.view.matrix, cl.csqc_vieworigin[0], cl.csqc_vieworigin[1], cl.csqc_vieworigin[2], cl.csqc_viewangles[0], cl.csqc_viewangles[1], cl.csqc_viewangles[2], 1);
	Matrix4x4_Copy(&viewmodelmatrix_nobob, &r_refdef.view.matrix);
	Matrix4x4_ConcatScale(&viewmodelmatrix_nobob, cl_viewmodel_scale.value);
	Matrix4x4_Concat(&viewmodelmatrix_withbob, &r_refdef.view.matrix, &cl.csqc_viewmodelmatrixfromengine);
}

void CL_RelinkLightFlashes(void);
//#300 void() clearscene (EXT_CSQC)
void VM_CL_R_ClearScene (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_R_ClearScene);
	// clear renderable entity and light lists
	r_refdef.scene.numentities = 0;
	r_refdef.scene.numlights = 0;
	// FIXME: restore these to the values from VM_CL_UpdateView
	r_refdef.view.x = 0;
	r_refdef.view.y = 0;
	r_refdef.view.z = 0;
	r_refdef.view.width = vid.width;
	r_refdef.view.height = vid.height;
	r_refdef.view.depth = 1;
	// FIXME: restore frustum_x/frustum_y
	r_refdef.view.useperspective = true;
	r_refdef.view.frustum_y = tan(scr_fov.value * M_PI / 360.0) * (3.0/4.0) * cl.viewzoom;
	r_refdef.view.frustum_x = r_refdef.view.frustum_y * (float)r_refdef.view.width / (float)r_refdef.view.height / vid_pixelheight.value;
	r_refdef.view.frustum_x *= r_refdef.frustumscale_x;
	r_refdef.view.frustum_y *= r_refdef.frustumscale_y;
	r_refdef.view.ortho_x = scr_fov.value * (3.0 / 4.0) * (float)r_refdef.view.width / (float)r_refdef.view.height / vid_pixelheight.value;
	r_refdef.view.ortho_y = scr_fov.value * (3.0 / 4.0);
	r_refdef.view.clear = true;
	r_refdef.view.isoverlay = false;
	VectorCopy(cl.csqc_vieworiginfromengine, cl.csqc_vieworigin);
	VectorCopy(cl.csqc_viewanglesfromengine, cl.csqc_viewangles);
	cl.csqc_vidvars.drawworld = r_drawworld.integer != 0;
	cl.csqc_vidvars.drawenginesbar = false;
	cl.csqc_vidvars.drawcrosshair = false;
	CSQC_R_RecalcView();
}

//#301 void(float mask) addentities (EXT_CSQC)
extern void CSQC_Predraw (prvm_edict_t *ed);//csprogs.c
extern void CSQC_Think (prvm_edict_t *ed);//csprogs.c
void VM_CL_R_AddEntities (void)
{
	double t = Sys_DoubleTime();
	int			i, drawmask;
	prvm_edict_t *ed;
	VM_SAFEPARMCOUNT(1, VM_CL_R_AddEntities);
	drawmask = (int)PRVM_G_FLOAT(OFS_PARM0);
	CSQC_RelinkAllEntities(drawmask);
	CL_RelinkLightFlashes();

	PRVM_clientglobalfloat(time) = cl.time;
	for(i=1;i<prog->num_edicts;i++)
	{
		// so we can easily check if CSQC entity #edictnum is currently drawn
		cl.csqcrenderentities[i].entitynumber = 0;
		ed = &prog->edicts[i];
		if(ed->priv.required->free)
			continue;
		CSQC_Think(ed);
		if(ed->priv.required->free)
			continue;
		// note that for RF_USEAXIS entities, Predraw sets v_forward/v_right/v_up globals that are read by CSQC_AddRenderEdict
		CSQC_Predraw(ed);
		if(ed->priv.required->free)
			continue;
		if(!((int)PRVM_clientedictfloat(ed, drawmask) & drawmask))
			continue;
		CSQC_AddRenderEdict(ed, i);
	}

	// callprofile fixing hack: do not include this time in what is counted for CSQC_UpdateView
	prog->functions[PRVM_clientfunction(CSQC_UpdateView)].totaltime -= Sys_DoubleTime() - t;
}

//#302 void(entity ent) addentity (EXT_CSQC)
void VM_CL_R_AddEntity (void)
{
	double t = Sys_DoubleTime();
	VM_SAFEPARMCOUNT(1, VM_CL_R_AddEntity);
	CSQC_AddRenderEdict(PRVM_G_EDICT(OFS_PARM0), 0);
	prog->functions[PRVM_clientfunction(CSQC_UpdateView)].totaltime -= Sys_DoubleTime() - t;
}

//#303 float(float property, ...) setproperty (EXT_CSQC)
//#303 float(float property) getproperty
//#303 vector(float property) getpropertyvec
// VorteX: make this function be able to return previously set property if new value is not given
void VM_CL_R_SetView (void)
{
	int		c;
	float	*f;
	float	k;

	VM_SAFEPARMCOUNTRANGE(1, 3, VM_CL_R_SetView);

	c = (int)PRVM_G_FLOAT(OFS_PARM0);

	// return value?
	if (prog->argc < 2)
	{
		switch(c)
		{
		case VF_MIN:
			VectorSet(PRVM_G_VECTOR(OFS_RETURN), r_refdef.view.x, r_refdef.view.y, 0);
			break;
		case VF_MIN_X:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.x;
			break;
		case VF_MIN_Y:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.y;
			break;
		case VF_SIZE:
			VectorSet(PRVM_G_VECTOR(OFS_RETURN), r_refdef.view.width, r_refdef.view.height, 0);
			break;
		case VF_SIZE_X:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.width;
			break;
		case VF_SIZE_Y:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.height;
			break;
		case VF_VIEWPORT:
			VM_Warning("VM_CL_R_GetView : VF_VIEWPORT can't be retrieved, use VF_MIN/VF_SIZE instead\n");
			break;
		case VF_FOV:
			VectorSet(PRVM_G_VECTOR(OFS_RETURN), r_refdef.view.ortho_x, r_refdef.view.ortho_y, 0);
			break;
		case VF_FOVX:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.ortho_x;
			break;
		case VF_FOVY:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.ortho_y;
			break;
		case VF_ORIGIN:
			VectorCopy(cl.csqc_vieworigin, PRVM_G_VECTOR(OFS_RETURN));
			break;
		case VF_ORIGIN_X:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_vieworigin[0];
			break;
		case VF_ORIGIN_Y:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_vieworigin[1];
			break;
		case VF_ORIGIN_Z:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_vieworigin[2];
			break;
		case VF_ANGLES:
			VectorCopy(cl.csqc_viewangles, PRVM_G_VECTOR(OFS_RETURN));
			break;
		case VF_ANGLES_X:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_viewangles[0];
			break;
		case VF_ANGLES_Y:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_viewangles[1];
			break;
		case VF_ANGLES_Z:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_viewangles[2];
			break;
		case VF_DRAWWORLD:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_vidvars.drawworld;
			break;
		case VF_DRAWENGINESBAR:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_vidvars.drawenginesbar;
			break;
		case VF_DRAWCROSSHAIR:
			PRVM_G_FLOAT(OFS_RETURN) = cl.csqc_vidvars.drawcrosshair;
			break;
		case VF_CL_VIEWANGLES:
			VectorCopy(cl.viewangles, PRVM_G_VECTOR(OFS_RETURN));;
			break;
		case VF_CL_VIEWANGLES_X:
			PRVM_G_FLOAT(OFS_RETURN) = cl.viewangles[0];
			break;
		case VF_CL_VIEWANGLES_Y:
			PRVM_G_FLOAT(OFS_RETURN) = cl.viewangles[1];
			break;
		case VF_CL_VIEWANGLES_Z:
			PRVM_G_FLOAT(OFS_RETURN) = cl.viewangles[2];
			break;
		case VF_PERSPECTIVE:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.useperspective;
			break;
		case VF_CLEARSCREEN:
			PRVM_G_FLOAT(OFS_RETURN) = r_refdef.view.isoverlay;
			break;
		default:
			PRVM_G_FLOAT(OFS_RETURN) = 0;
			VM_Warning("VM_CL_R_GetView : unknown parm %i\n", c);
			return;
		}
		return;
	}

	f = PRVM_G_VECTOR(OFS_PARM1);
	k = PRVM_G_FLOAT(OFS_PARM1);
	switch(c)
	{
	case VF_MIN:
		r_refdef.view.x = (int)(f[0]);
		r_refdef.view.y = (int)(f[1]);
		DrawQ_RecalcView();
		break;
	case VF_MIN_X:
		r_refdef.view.x = (int)(k);
		DrawQ_RecalcView();
		break;
	case VF_MIN_Y:
		r_refdef.view.y = (int)(k);
		DrawQ_RecalcView();
		break;
	case VF_SIZE:
		r_refdef.view.width = (int)(f[0]);
		r_refdef.view.height = (int)(f[1]);
		DrawQ_RecalcView();
		break;
	case VF_SIZE_X:
		r_refdef.view.width = (int)(k);
		DrawQ_RecalcView();
		break;
	case VF_SIZE_Y:
		r_refdef.view.height = (int)(k);
		DrawQ_RecalcView();
		break;
	case VF_VIEWPORT:
		r_refdef.view.x = (int)(f[0]);
		r_refdef.view.y = (int)(f[1]);
		f = PRVM_G_VECTOR(OFS_PARM2);
		r_refdef.view.width = (int)(f[0]);
		r_refdef.view.height = (int)(f[1]);
		DrawQ_RecalcView();
		break;
	case VF_FOV:
		r_refdef.view.frustum_x = tan(f[0] * M_PI / 360.0);r_refdef.view.ortho_x = f[0];
		r_refdef.view.frustum_y = tan(f[1] * M_PI / 360.0);r_refdef.view.ortho_y = f[1];
		break;
	case VF_FOVX:
		r_refdef.view.frustum_x = tan(k * M_PI / 360.0);r_refdef.view.ortho_x = k;
		break;
	case VF_FOVY:
		r_refdef.view.frustum_y = tan(k * M_PI / 360.0);r_refdef.view.ortho_y = k;
		break;
	case VF_ORIGIN:
		VectorCopy(f, cl.csqc_vieworigin);
		CSQC_R_RecalcView();
		break;
	case VF_ORIGIN_X:
		cl.csqc_vieworigin[0] = k;
		CSQC_R_RecalcView();
		break;
	case VF_ORIGIN_Y:
		cl.csqc_vieworigin[1] = k;
		CSQC_R_RecalcView();
		break;
	case VF_ORIGIN_Z:
		cl.csqc_vieworigin[2] = k;
		CSQC_R_RecalcView();
		break;
	case VF_ANGLES:
		VectorCopy(f, cl.csqc_viewangles);
		CSQC_R_RecalcView();
		break;
	case VF_ANGLES_X:
		cl.csqc_viewangles[0] = k;
		CSQC_R_RecalcView();
		break;
	case VF_ANGLES_Y:
		cl.csqc_viewangles[1] = k;
		CSQC_R_RecalcView();
		break;
	case VF_ANGLES_Z:
		cl.csqc_viewangles[2] = k;
		CSQC_R_RecalcView();
		break;
	case VF_DRAWWORLD:
		cl.csqc_vidvars.drawworld = ((k != 0) && r_drawworld.integer);
		break;
	case VF_DRAWENGINESBAR:
		cl.csqc_vidvars.drawenginesbar = k != 0;
		break;
	case VF_DRAWCROSSHAIR:
		cl.csqc_vidvars.drawcrosshair = k != 0;
		break;
	case VF_CL_VIEWANGLES:
		VectorCopy(f, cl.viewangles);
		break;
	case VF_CL_VIEWANGLES_X:
		cl.viewangles[0] = k;
		break;
	case VF_CL_VIEWANGLES_Y:
		cl.viewangles[1] = k;
		break;
	case VF_CL_VIEWANGLES_Z:
		cl.viewangles[2] = k;
		break;
	case VF_PERSPECTIVE:
		r_refdef.view.useperspective = k != 0;
		break;
	case VF_CLEARSCREEN:
		r_refdef.view.isoverlay = !k;
		break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning("VM_CL_R_SetView : unknown parm %i\n", c);
		return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

//#305 void(vector org, float radius, vector lightcolours[, float style, string cubemapname, float pflags]) adddynamiclight (EXT_CSQC)
void VM_CL_R_AddDynamicLight (void)
{
	double t = Sys_DoubleTime();
	vec_t *org;
	float radius = 300;
	vec_t *col;
	int style = -1;
	const char *cubemapname = NULL;
	int pflags = PFLAGS_CORONA | PFLAGS_FULLDYNAMIC;
	float coronaintensity = 1;
	float coronasizescale = 0.25;
	qboolean castshadow = true;
	float ambientscale = 0;
	float diffusescale = 1;
	float specularscale = 1;
	matrix4x4_t matrix;
	vec3_t forward, left, up;
	VM_SAFEPARMCOUNTRANGE(3, 8, VM_CL_R_AddDynamicLight);

	// if we've run out of dlights, just return
	if (r_refdef.scene.numlights >= MAX_DLIGHTS)
		return;

	org = PRVM_G_VECTOR(OFS_PARM0);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	col = PRVM_G_VECTOR(OFS_PARM2);
	if (prog->argc >= 4)
	{
		style = (int)PRVM_G_FLOAT(OFS_PARM3);
		if (style >= MAX_LIGHTSTYLES)
		{
			Con_DPrintf("VM_CL_R_AddDynamicLight: out of bounds lightstyle index %i\n", style);
			style = -1;
		}
	}
	if (prog->argc >= 5)
		cubemapname = PRVM_G_STRING(OFS_PARM4);
	if (prog->argc >= 6)
		pflags = (int)PRVM_G_FLOAT(OFS_PARM5);
	coronaintensity = (pflags & PFLAGS_CORONA) != 0;
	castshadow = (pflags & PFLAGS_NOSHADOW) == 0;

	VectorScale(PRVM_clientglobalvector(v_forward), radius, forward);
	VectorScale(PRVM_clientglobalvector(v_right), -radius, left);
	VectorScale(PRVM_clientglobalvector(v_up), radius, up);
	Matrix4x4_FromVectors(&matrix, forward, left, up, org);

	R_RTLight_Update(&r_refdef.scene.templights[r_refdef.scene.numlights], false, &matrix, col, style, cubemapname, castshadow, coronaintensity, coronasizescale, ambientscale, diffusescale, specularscale, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	r_refdef.scene.lights[r_refdef.scene.numlights] = &r_refdef.scene.templights[r_refdef.scene.numlights];r_refdef.scene.numlights++;
	prog->functions[PRVM_clientfunction(CSQC_UpdateView)].totaltime -= Sys_DoubleTime() - t;
}

//============================================================================

//#310 vector (vector v) cs_unproject (EXT_CSQC)
static void VM_CL_unproject (void)
{
	float	*f;
	vec3_t	temp;

	VM_SAFEPARMCOUNT(1, VM_CL_unproject);
	f = PRVM_G_VECTOR(OFS_PARM0);
	VectorSet(temp,
		f[2],
		(-1.0 + 2.0 * (f[0] / vid_conwidth.integer)) * f[2] * -r_refdef.view.frustum_x,
		(-1.0 + 2.0 * (f[1] / vid_conheight.integer)) * f[2] * -r_refdef.view.frustum_y);
	if(v_flipped.integer)
		temp[1] = -temp[1];
	Matrix4x4_Transform(&r_refdef.view.matrix, temp, PRVM_G_VECTOR(OFS_RETURN));
}

//#311 vector (vector v) cs_project (EXT_CSQC)
static void VM_CL_project (void)
{
	float	*f;
	vec3_t	v;
	matrix4x4_t m;

	VM_SAFEPARMCOUNT(1, VM_CL_project);
	f = PRVM_G_VECTOR(OFS_PARM0);
	Matrix4x4_Invert_Simple(&m, &r_refdef.view.matrix);
	Matrix4x4_Transform(&m, f, v);
	if(v_flipped.integer)
		v[1] = -v[1];
	VectorSet(PRVM_G_VECTOR(OFS_RETURN),
		vid_conwidth.integer * (0.5*(1.0+v[1]/v[0]/-r_refdef.view.frustum_x)),
		vid_conheight.integer * (0.5*(1.0+v[2]/v[0]/-r_refdef.view.frustum_y)),
		v[0]);
	// explanation:
	// after transforming, relative position to viewport (0..1) = 0.5 * (1 + v[2]/v[0]/-frustum_{x \or y})
	// as 2D drawing honors the viewport too, to get the same pixel, we simply multiply this by conwidth/height
}

//#330 float(float stnum) getstatf (EXT_CSQC)
static void VM_CL_getstatf (void)
{
	int i;
	union
	{
		float f;
		int l;
	}dat;
	VM_SAFEPARMCOUNT(1, VM_CL_getstatf);
	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	if(i < 0 || i >= MAX_CL_STATS)
	{
		VM_Warning("VM_CL_getstatf: index>=MAX_CL_STATS or index<0\n");
		return;
	}
	dat.l = cl.stats[i];
	PRVM_G_FLOAT(OFS_RETURN) =  dat.f;
}

//#331 float(float stnum) getstati (EXT_CSQC)
static void VM_CL_getstati (void)
{
	int i, index;
	int firstbit, bitcount;

	VM_SAFEPARMCOUNTRANGE(1, 3, VM_CL_getstati);

	index = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (prog->argc > 1)
	{
		firstbit = (int)PRVM_G_FLOAT(OFS_PARM1);
		if (prog->argc > 2)
			bitcount = (int)PRVM_G_FLOAT(OFS_PARM2);
		else
			bitcount = 1;
	}
	else
	{
		firstbit = 0;
		bitcount = 32;
	}

	if(index < 0 || index >= MAX_CL_STATS)
	{
		VM_Warning("VM_CL_getstati: index>=MAX_CL_STATS or index<0\n");
		return;
	}
	i = cl.stats[index];
	if (bitcount != 32)	//32 causes the mask to overflow, so there's nothing to subtract from.
		i = (((unsigned int)i)&(((1<<bitcount)-1)<<firstbit))>>firstbit;
	PRVM_G_FLOAT(OFS_RETURN) = i;
}

//#332 string(float firststnum) getstats (EXT_CSQC)
static void VM_CL_getstats (void)
{
	int i;
	char t[17];
	VM_SAFEPARMCOUNT(1, VM_CL_getstats);
	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	if(i < 0 || i > MAX_CL_STATS-4)
	{
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
		VM_Warning("VM_CL_getstats: index>MAX_CL_STATS-4 or index<0\n");
		return;
	}
	strlcpy(t, (char*)&cl.stats[i], sizeof(t));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(t);
}

//#333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
static void VM_CL_setmodelindex (void)
{
	int				i;
	prvm_edict_t	*t;
	struct model_s	*model;

	VM_SAFEPARMCOUNT(2, VM_CL_setmodelindex);

	t = PRVM_G_EDICT(OFS_PARM0);

	i = (int)PRVM_G_FLOAT(OFS_PARM1);

	PRVM_clientedictstring(t, model) = 0;
	PRVM_clientedictfloat(t, modelindex) = 0;

	if (!i)
		return;

	model = CL_GetModelByIndex(i);
	if (!model)
	{
		VM_Warning("VM_CL_setmodelindex: null model\n");
		return;
	}
	PRVM_clientedictstring(t, model) = PRVM_SetEngineString(model->name);
	PRVM_clientedictfloat(t, modelindex) = i;

	// TODO: check if this breaks needed consistency and maybe add a cvar for it too?? [1/10/2008 Black]
	if (model)
	{
		SetMinMaxSize (t, model->normalmins, model->normalmaxs);
	}
	else
		SetMinMaxSize (t, vec3_origin, vec3_origin);
}

//#334 string(float mdlindex) modelnameforindex (EXT_CSQC)
static void VM_CL_modelnameforindex (void)
{
	dp_model_t *model;

	VM_SAFEPARMCOUNT(1, VM_CL_modelnameforindex);

	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	model = CL_GetModelByIndex((int)PRVM_G_FLOAT(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = model ? PRVM_SetEngineString(model->name) : 0;
}

//#335 float(string effectname) particleeffectnum (EXT_CSQC)
static void VM_CL_particleeffectnum (void)
{
	int			i;
	VM_SAFEPARMCOUNT(1, VM_CL_particleeffectnum);
	i = CL_ParticleEffectIndexForName(PRVM_G_STRING(OFS_PARM0));
	if (i == 0)
		i = -1;
	PRVM_G_FLOAT(OFS_RETURN) = i;
}

// #336 void(entity ent, float effectnum, vector start, vector end[, float color]) trailparticles (EXT_CSQC)
static void VM_CL_trailparticles (void)
{
	int				i;
	float			*start, *end;
	prvm_edict_t	*t;
	VM_SAFEPARMCOUNTRANGE(4, 5, VM_CL_trailparticles);

	t = PRVM_G_EDICT(OFS_PARM0);
	i		= (int)PRVM_G_FLOAT(OFS_PARM1);
	start	= PRVM_G_VECTOR(OFS_PARM2);
	end		= PRVM_G_VECTOR(OFS_PARM3);

	if (i < 0)
		return;
	CL_ParticleEffect(i, 1, start, end, PRVM_clientedictvector(t, velocity), PRVM_clientedictvector(t, velocity), NULL, prog->argc >= 5 ? (int)PRVM_G_FLOAT(OFS_PARM4) : 0);
}

//#337 void(float effectnum, vector origin, vector dir, float count[, float color]) pointparticles (EXT_CSQC)
static void VM_CL_pointparticles (void)
{
	int			i;
	float n;
	float		*f, *v;
	VM_SAFEPARMCOUNTRANGE(4, 5, VM_CL_pointparticles);
	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	f = PRVM_G_VECTOR(OFS_PARM1);
	v = PRVM_G_VECTOR(OFS_PARM2);
	n = PRVM_G_FLOAT(OFS_PARM3);
	if (i < 0)
		return;
	CL_ParticleEffect(i, n, f, f, v, v, NULL, prog->argc >= 5 ? (int)PRVM_G_FLOAT(OFS_PARM4) : 0);
}

//#502 void(float effectnum, entity own, vector origin_from, vector origin_to, vector dir_from, vector dir_to, float count, float extflags) boxparticles (DP_CSQC_BOXPARTICLES)
static void VM_CL_boxparticles (void)
{
	int effectnum;
	// prvm_edict_t *own;
	float *origin_from, *origin_to, *dir_from, *dir_to;
	float count;
	int flags;
	float tintmins[4], tintmaxs[4];
	VM_SAFEPARMCOUNTRANGE(7, 8, VM_CL_boxparticles);

	effectnum = (int)PRVM_G_FLOAT(OFS_PARM0);
	// own = PRVM_G_EDICT(OFS_PARM1); // TODO find use for this
	origin_from = PRVM_G_VECTOR(OFS_PARM2);
	origin_to = PRVM_G_VECTOR(OFS_PARM3);
	dir_from = PRVM_G_VECTOR(OFS_PARM4);
	dir_to = PRVM_G_VECTOR(OFS_PARM5);
	count = PRVM_G_FLOAT(OFS_PARM6);
	if(prog->argc >= 8)
		flags = PRVM_G_FLOAT(OFS_PARM7);
	else
		flags = 0;
	Vector4Set(tintmins, 1, 1, 1, 1);
	Vector4Set(tintmaxs, 1, 1, 1, 1);
	if(flags & 1) // read alpha
	{
		tintmins[3] = PRVM_clientglobalfloat(particles_alphamin);
		tintmaxs[3] = PRVM_clientglobalfloat(particles_alphamax);
	}
	if(flags & 2) // read color
	{
		VectorCopy(PRVM_clientglobalvector(particles_colormin), tintmins);
		VectorCopy(PRVM_clientglobalvector(particles_colormax), tintmaxs);
	}
	if (effectnum < 0)
		return;
	CL_ParticleTrail(effectnum, count, origin_from, origin_to, dir_from, dir_to, NULL, 0, true, true, tintmins, tintmaxs);
}

//#531 void(float pause) setpause
static void VM_CL_setpause(void) 
{
	VM_SAFEPARMCOUNT(1, VM_CL_setpause);
	if ((int)PRVM_G_FLOAT(OFS_PARM0) != 0)
		cl.csqc_paused = true;
	else
		cl.csqc_paused = false;
}

//#343 void(float usecursor) setcursormode (EXT_CSQC)
static void VM_CL_setcursormode (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_setcursormode);
	cl.csqc_wantsmousemove = PRVM_G_FLOAT(OFS_PARM0) != 0;
	cl_ignoremousemoves = 2;
}

//#344 vector() getmousepos (EXT_CSQC)
static void VM_CL_getmousepos(void)
{
	VM_SAFEPARMCOUNT(0,VM_CL_getmousepos);

	if (key_consoleactive || key_dest != key_game)
		VectorSet(PRVM_G_VECTOR(OFS_RETURN), 0, 0, 0);
	else if (cl.csqc_wantsmousemove)
		VectorSet(PRVM_G_VECTOR(OFS_RETURN), in_windowmouse_x * vid_conwidth.integer / vid.width, in_windowmouse_y * vid_conheight.integer / vid.height, 0);
	else
		VectorSet(PRVM_G_VECTOR(OFS_RETURN), in_mouse_x * vid_conwidth.integer / vid.width, in_mouse_y * vid_conheight.integer / vid.height, 0);
}

//#345 float(float framenum) getinputstate (EXT_CSQC)
static void VM_CL_getinputstate (void)
{
	int i, frame;
	VM_SAFEPARMCOUNT(1, VM_CL_getinputstate);
	frame = (int)PRVM_G_FLOAT(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = false;
	for (i = 0;i < CL_MAX_USERCMDS;i++)
	{
		if (cl.movecmd[i].sequence == frame)
		{
			VectorCopy(cl.movecmd[i].viewangles, PRVM_clientglobalvector(input_angles));
			PRVM_clientglobalfloat(input_buttons) = cl.movecmd[i].buttons; // FIXME: this should not be directly exposed to csqc (translation layer needed?)
			PRVM_clientglobalvector(input_movevalues)[0] = cl.movecmd[i].forwardmove;
			PRVM_clientglobalvector(input_movevalues)[1] = cl.movecmd[i].sidemove;
			PRVM_clientglobalvector(input_movevalues)[2] = cl.movecmd[i].upmove;
			PRVM_clientglobalfloat(input_timelength) = cl.movecmd[i].frametime;
			if(cl.movecmd[i].crouch)
			{
				VectorCopy(cl.playercrouchmins, PRVM_clientglobalvector(pmove_mins));
				VectorCopy(cl.playercrouchmaxs, PRVM_clientglobalvector(pmove_maxs));
			}
			else
			{
				VectorCopy(cl.playerstandmins, PRVM_clientglobalvector(pmove_mins));
				VectorCopy(cl.playerstandmaxs, PRVM_clientglobalvector(pmove_maxs));
			}
			PRVM_G_FLOAT(OFS_RETURN) = true;
		}
	}
}

//#346 void(float sens) setsensitivityscaler (EXT_CSQC)
static void VM_CL_setsensitivityscale (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_setsensitivityscale);
	cl.sensitivityscale = PRVM_G_FLOAT(OFS_PARM0);
}

//#347 void() runstandardplayerphysics (EXT_CSQC)
static void VM_CL_runplayerphysics (void)
{
}

//#348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
static void VM_CL_getplayerkey (void)
{
	int			i;
	char		t[128];
	const char	*c;

	VM_SAFEPARMCOUNT(2, VM_CL_getplayerkey);

	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	c = PRVM_G_STRING(OFS_PARM1);
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	Sbar_SortFrags();

	if (i < 0)
		i = Sbar_GetSortedPlayerIndex(-1-i);
	if(i < 0 || i >= cl.maxclients)
		return;

	t[0] = 0;

	if(!strcasecmp(c, "name"))
		strlcpy(t, cl.scores[i].name, sizeof(t));
	else
		if(!strcasecmp(c, "frags"))
			dpsnprintf(t, sizeof(t), "%i", cl.scores[i].frags);
	else
		if(!strcasecmp(c, "ping"))
			dpsnprintf(t, sizeof(t), "%i", cl.scores[i].qw_ping);
	else
		if(!strcasecmp(c, "pl"))
			dpsnprintf(t, sizeof(t), "%i", cl.scores[i].qw_packetloss);
	else
		if(!strcasecmp(c, "movementloss"))
			dpsnprintf(t, sizeof(t), "%i", cl.scores[i].qw_movementloss);
	else
		if(!strcasecmp(c, "entertime"))
			dpsnprintf(t, sizeof(t), "%f", cl.scores[i].qw_entertime);
	else
		if(!strcasecmp(c, "colors"))
			dpsnprintf(t, sizeof(t), "%i", cl.scores[i].colors);
	else
		if(!strcasecmp(c, "topcolor"))
			dpsnprintf(t, sizeof(t), "%i", cl.scores[i].colors & 0xf0);
	else
		if(!strcasecmp(c, "bottomcolor"))
			dpsnprintf(t, sizeof(t), "%i", (cl.scores[i].colors &15)<<4);
	else
		if(!strcasecmp(c, "viewentity"))
			dpsnprintf(t, sizeof(t), "%i", i+1);
	else
		if(gamemode == GAME_XONOTIC && !strcasecmp(c, "TEMPHACK_origin"))
		{
			// PLEASE REMOVE THIS once deltalisten() of EXT_CSQC_1
			// is implemented, or Xonotic uses CSQC-networked
			// players, whichever comes first
			entity_t *e = cl.entities + (i+1);
			if(e->state_current.active)
			{
				vec3_t origin;
				Matrix4x4_OriginFromMatrix(&e->render.matrix, origin);
				dpsnprintf(t, sizeof(t), "%.9g %.9g %.9g", origin[0], origin[1], origin[2]);
			}
		}
	if(!t[0])
		return;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(t);
}

//#351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
static void VM_CL_setlistener (void)
{
	VM_SAFEPARMCOUNT(4, VM_CL_setlistener);
	Matrix4x4_FromVectors(&cl.csqc_listenermatrix, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), PRVM_G_VECTOR(OFS_PARM3), PRVM_G_VECTOR(OFS_PARM0));
	cl.csqc_usecsqclistener = true;	//use csqc listener at this frame
}

//#352 void(string cmdname) registercommand (EXT_CSQC)
static void VM_CL_registercmd (void)
{
	char *t;
	VM_SAFEPARMCOUNT(1, VM_CL_registercmd);
	if(!Cmd_Exists(PRVM_G_STRING(OFS_PARM0)))
	{
		size_t alloclen;

		alloclen = strlen(PRVM_G_STRING(OFS_PARM0)) + 1;
		t = (char *)Z_Malloc(alloclen);
		memcpy(t, PRVM_G_STRING(OFS_PARM0), alloclen);
		Cmd_AddCommand(t, NULL, "console command created by QuakeC");
	}
	else
		Cmd_AddCommand(PRVM_G_STRING(OFS_PARM0), NULL, "console command created by QuakeC");

}

//#360 float() readbyte (EXT_CSQC)
static void VM_CL_ReadByte (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadByte);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadByte();
}

//#361 float() readchar (EXT_CSQC)
static void VM_CL_ReadChar (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadChar);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadChar();
}

//#362 float() readshort (EXT_CSQC)
static void VM_CL_ReadShort (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadShort);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadShort();
}

//#363 float() readlong (EXT_CSQC)
static void VM_CL_ReadLong (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadLong);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadLong();
}

//#364 float() readcoord (EXT_CSQC)
static void VM_CL_ReadCoord (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadCoord);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadCoord(cls.protocol);
}

//#365 float() readangle (EXT_CSQC)
static void VM_CL_ReadAngle (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadAngle);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadAngle(cls.protocol);
}

//#366 string() readstring (EXT_CSQC)
static void VM_CL_ReadString (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadString);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(MSG_ReadString());
}

//#367 float() readfloat (EXT_CSQC)
static void VM_CL_ReadFloat (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ReadFloat);
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadFloat();
}

//#501 string() readpicture (DP_CSQC_READWRITEPICTURE)
extern cvar_t cl_readpicture_force;
static void VM_CL_ReadPicture (void)
{
	const char *name;
	unsigned char *data;
	unsigned char *buf;
	int size;
	int i;
	cachepic_t *pic;

	VM_SAFEPARMCOUNT(0, VM_CL_ReadPicture);

	name = MSG_ReadString();
	size = MSG_ReadShort();

	// check if a texture of that name exists
	// if yes, it is used and the data is discarded
	// if not, the (low quality) data is used to build a new texture, whose name will get returned

	pic = Draw_CachePic_Flags (name, CACHEPICFLAG_NOTPERSISTENT);

	if(size)
	{
		if(pic->tex == r_texture_notexture)
			pic->tex = NULL; // don't overwrite the notexture by Draw_NewPic
		if(pic->tex && !cl_readpicture_force.integer)
		{
			// texture found and loaded
			// skip over the jpeg as we don't need it
			for(i = 0; i < size; ++i)
				(void) MSG_ReadByte();
		}
		else
		{
			// texture not found
			// use the attached jpeg as texture
			buf = (unsigned char *) Mem_Alloc(tempmempool, size);
			MSG_ReadBytes(size, buf);
			data = JPEG_LoadImage_BGRA(buf, size, NULL);
			Mem_Free(buf);
			Draw_NewPic(name, image_width, image_height, false, data);
			Mem_Free(data);
		}
	}

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(name);
}

//////////////////////////////////////////////////////////

static void VM_CL_makestatic (void)
{
	prvm_edict_t *ent;

	VM_SAFEPARMCOUNT(1, VM_CL_makestatic);

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("makestatic: can not modify world entity\n");
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("makestatic: can not modify free entity\n");
		return;
	}

	if (cl.num_static_entities < cl.max_static_entities)
	{
		int renderflags;
		entity_t *staticent = &cl.static_entities[cl.num_static_entities++];

		// copy it to the current state
		memset(staticent, 0, sizeof(*staticent));
		staticent->render.model = CL_GetModelByIndex((int)PRVM_clientedictfloat(ent, modelindex));
		staticent->render.framegroupblend[0].frame = (int)PRVM_clientedictfloat(ent, frame);
		staticent->render.framegroupblend[0].lerp = 1;
		// make torchs play out of sync
		staticent->render.framegroupblend[0].start = lhrandom(-10, -1);
		staticent->render.skinnum = (int)PRVM_clientedictfloat(ent, skin);
		staticent->render.effects = (int)PRVM_clientedictfloat(ent, effects);
		staticent->render.alpha = PRVM_clientedictfloat(ent, alpha);
		staticent->render.scale = PRVM_clientedictfloat(ent, scale);
		VectorCopy(PRVM_clientedictvector(ent, colormod), staticent->render.colormod);
		VectorCopy(PRVM_clientedictvector(ent, glowmod), staticent->render.glowmod);

		// sanitize values
		if (!staticent->render.alpha)
			staticent->render.alpha = 1.0f;
		if (!staticent->render.scale)
			staticent->render.scale = 1.0f;
		if (!VectorLength2(staticent->render.colormod))
			VectorSet(staticent->render.colormod, 1, 1, 1);
		if (!VectorLength2(staticent->render.glowmod))
			VectorSet(staticent->render.glowmod, 1, 1, 1);

		renderflags = (int)PRVM_clientedictfloat(ent, renderflags);
		if (renderflags & RF_USEAXIS)
		{
			vec3_t left;
			VectorNegate(PRVM_clientglobalvector(v_right), left);
			Matrix4x4_FromVectors(&staticent->render.matrix, PRVM_clientglobalvector(v_forward), left, PRVM_clientglobalvector(v_up), PRVM_clientedictvector(ent, origin));
			Matrix4x4_Scale(&staticent->render.matrix, staticent->render.scale, 1);
		}
		else
			Matrix4x4_CreateFromQuakeEntity(&staticent->render.matrix, PRVM_clientedictvector(ent, origin)[0], PRVM_clientedictvector(ent, origin)[1], PRVM_clientedictvector(ent, origin)[2], PRVM_clientedictvector(ent, angles)[0], PRVM_clientedictvector(ent, angles)[1], PRVM_clientedictvector(ent, angles)[2], staticent->render.scale);

		// either fullbright or lit
		if(!r_fullbright.integer)
		{
			if (!(staticent->render.effects & EF_FULLBRIGHT))
				staticent->render.flags |= RENDER_LIGHT;
			else if(r_equalize_entities_fullbright.integer)
				staticent->render.flags |= RENDER_LIGHT | RENDER_EQUALIZE;
		}
		// turn off shadows from transparent objects
		if (!(staticent->render.effects & (EF_NOSHADOW | EF_ADDITIVE | EF_NODEPTHTEST)) && (staticent->render.alpha >= 1))
			staticent->render.flags |= RENDER_SHADOW;
		if (staticent->render.effects & EF_NODEPTHTEST)
			staticent->render.flags |= RENDER_NODEPTHTEST;
		if (staticent->render.effects & EF_ADDITIVE)
			staticent->render.flags |= RENDER_ADDITIVE;
		if (staticent->render.effects & EF_DOUBLESIDED)
			staticent->render.flags |= RENDER_DOUBLESIDED;

		staticent->render.allowdecals = true;
		CL_UpdateRenderEntity(&staticent->render);
	}
	else
		Con_Printf("Too many static entities");

// throw the entity away now
	PRVM_ED_Free (ent);
}

//=================================================================//

/*
=================
VM_CL_copyentity

copies data from one entity to another

copyentity(src, dst)
=================
*/
static void VM_CL_copyentity (void)
{
	prvm_edict_t *in, *out;
	VM_SAFEPARMCOUNT(2, VM_CL_copyentity);
	in = PRVM_G_EDICT(OFS_PARM0);
	if (in == prog->edicts)
	{
		VM_Warning("copyentity: can not read world entity\n");
		return;
	}
	if (in->priv.server->free)
	{
		VM_Warning("copyentity: can not read free entity\n");
		return;
	}
	out = PRVM_G_EDICT(OFS_PARM1);
	if (out == prog->edicts)
	{
		VM_Warning("copyentity: can not modify world entity\n");
		return;
	}
	if (out->priv.server->free)
	{
		VM_Warning("copyentity: can not modify free entity\n");
		return;
	}
	memcpy(out->fields.vp, in->fields.vp, prog->entityfields * 4);
	CL_LinkEdict(out);
}

//=================================================================//

// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
static void VM_CL_effect (void)
{
	VM_SAFEPARMCOUNT(5, VM_CL_effect);
	CL_Effect(PRVM_G_VECTOR(OFS_PARM0), (int)PRVM_G_FLOAT(OFS_PARM1), (int)PRVM_G_FLOAT(OFS_PARM2), (int)PRVM_G_FLOAT(OFS_PARM3), PRVM_G_FLOAT(OFS_PARM4));
}

// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
static void VM_CL_te_blood (void)
{
	float	*pos;
	vec3_t	pos2;
	VM_SAFEPARMCOUNT(3, VM_CL_te_blood);
	if (PRVM_G_FLOAT(OFS_PARM2) < 1)
		return;
	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_BLOOD, PRVM_G_FLOAT(OFS_PARM2), pos2, pos2, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM1), NULL, 0);
}

// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
static void VM_CL_te_bloodshower (void)
{
	vec_t speed;
	vec3_t vel1, vel2;
	VM_SAFEPARMCOUNT(4, VM_CL_te_bloodshower);
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	speed = PRVM_G_FLOAT(OFS_PARM2);
	vel1[0] = -speed;
	vel1[1] = -speed;
	vel1[2] = -speed;
	vel2[0] = speed;
	vel2[1] = speed;
	vel2[2] = speed;
	CL_ParticleEffect(EFFECT_TE_BLOOD, PRVM_G_FLOAT(OFS_PARM3), PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), vel1, vel2, NULL, 0);
}

// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
static void VM_CL_te_explosionrgb (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(2, VM_CL_te_explosionrgb);
	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleExplosion(pos2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocLightFlash(NULL, &tempmatrix, 350, PRVM_G_VECTOR(OFS_PARM1)[0], PRVM_G_VECTOR(OFS_PARM1)[1], PRVM_G_VECTOR(OFS_PARM1)[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
static void VM_CL_te_particlecube (void)
{
	VM_SAFEPARMCOUNT(7, VM_CL_te_particlecube);
	CL_ParticleCube(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), (int)PRVM_G_FLOAT(OFS_PARM3), (int)PRVM_G_FLOAT(OFS_PARM4), PRVM_G_FLOAT(OFS_PARM5), PRVM_G_FLOAT(OFS_PARM6));
}

// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
static void VM_CL_te_particlerain (void)
{
	VM_SAFEPARMCOUNT(5, VM_CL_te_particlerain);
	CL_ParticleRain(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), (int)PRVM_G_FLOAT(OFS_PARM3), (int)PRVM_G_FLOAT(OFS_PARM4), 0);
}

// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
static void VM_CL_te_particlesnow (void)
{
	VM_SAFEPARMCOUNT(5, VM_CL_te_particlesnow);
	CL_ParticleRain(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), (int)PRVM_G_FLOAT(OFS_PARM3), (int)PRVM_G_FLOAT(OFS_PARM4), 1);
}

// #411 void(vector org, vector vel, float howmany) te_spark
static void VM_CL_te_spark (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(3, VM_CL_te_spark);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_SPARK, PRVM_G_FLOAT(OFS_PARM2), pos2, pos2, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM1), NULL, 0);
}

extern cvar_t cl_sound_ric_gunshot;
// #412 void(vector org) te_gunshotquad (DP_QUADEFFECTS1)
static void VM_CL_te_gunshotquad (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_gunshotquad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_GUNSHOTQUAD, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	if(cl_sound_ric_gunshot.integer >= 2)
	{
		if (rand() % 5)			S_StartSound(-1, 0, cl.sfx_tink1, pos2, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)		S_StartSound(-1, 0, cl.sfx_ric1, pos2, 1, 1);
			else if (rnd == 2)	S_StartSound(-1, 0, cl.sfx_ric2, pos2, 1, 1);
			else				S_StartSound(-1, 0, cl.sfx_ric3, pos2, 1, 1);
		}
	}
}

// #413 void(vector org) te_spikequad (DP_QUADEFFECTS1)
static void VM_CL_te_spikequad (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_spikequad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_SPIKEQUAD, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	if (rand() % 5)			S_StartSound(-1, 0, cl.sfx_tink1, pos2, 1, 1);
	else
	{
		rnd = rand() & 3;
		if (rnd == 1)		S_StartSound(-1, 0, cl.sfx_ric1, pos2, 1, 1);
		else if (rnd == 2)	S_StartSound(-1, 0, cl.sfx_ric2, pos2, 1, 1);
		else				S_StartSound(-1, 0, cl.sfx_ric3, pos2, 1, 1);
	}
}

// #414 void(vector org) te_superspikequad (DP_QUADEFFECTS1)
static void VM_CL_te_superspikequad (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_superspikequad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_SUPERSPIKEQUAD, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	if (rand() % 5)			S_StartSound(-1, 0, cl.sfx_tink1, pos, 1, 1);
	else
	{
		rnd = rand() & 3;
		if (rnd == 1)		S_StartSound(-1, 0, cl.sfx_ric1, pos2, 1, 1);
		else if (rnd == 2)	S_StartSound(-1, 0, cl.sfx_ric2, pos2, 1, 1);
		else				S_StartSound(-1, 0, cl.sfx_ric3, pos2, 1, 1);
	}
}

// #415 void(vector org) te_explosionquad (DP_QUADEFFECTS1)
static void VM_CL_te_explosionquad (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_explosionquad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleEffect(EFFECT_TE_EXPLOSIONQUAD, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}

// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
static void VM_CL_te_smallflash (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_smallflash);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleEffect(EFFECT_TE_SMALLFLASH, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
}

// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
static void VM_CL_te_customflash (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(4, VM_CL_te_customflash);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocLightFlash(NULL, &tempmatrix, PRVM_G_FLOAT(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM3)[0], PRVM_G_VECTOR(OFS_PARM3)[1], PRVM_G_VECTOR(OFS_PARM3)[2], PRVM_G_FLOAT(OFS_PARM1) / PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM2), 0, -1, true, 1, 0.25, 1, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_gunshot (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_gunshot);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_GUNSHOT, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	if(cl_sound_ric_gunshot.integer == 1 || cl_sound_ric_gunshot.integer == 3)
	{
		if (rand() % 5)			S_StartSound(-1, 0, cl.sfx_tink1, pos2, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)		S_StartSound(-1, 0, cl.sfx_ric1, pos2, 1, 1);
			else if (rnd == 2)	S_StartSound(-1, 0, cl.sfx_ric2, pos2, 1, 1);
			else				S_StartSound(-1, 0, cl.sfx_ric3, pos2, 1, 1);
		}
	}
}

// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_spike (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_spike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_SPIKE, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	if (rand() % 5)			S_StartSound(-1, 0, cl.sfx_tink1, pos2, 1, 1);
	else
	{
		rnd = rand() & 3;
		if (rnd == 1)		S_StartSound(-1, 0, cl.sfx_ric1, pos2, 1, 1);
		else if (rnd == 2)	S_StartSound(-1, 0, cl.sfx_ric2, pos2, 1, 1);
		else				S_StartSound(-1, 0, cl.sfx_ric3, pos2, 1, 1);
	}
}

// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_superspike (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_superspike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_SUPERSPIKE, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	if (rand() % 5)			S_StartSound(-1, 0, cl.sfx_tink1, pos2, 1, 1);
	else
	{
		rnd = rand() & 3;
		if (rnd == 1)		S_StartSound(-1, 0, cl.sfx_ric1, pos2, 1, 1);
		else if (rnd == 2)	S_StartSound(-1, 0, cl.sfx_ric2, pos2, 1, 1);
		else				S_StartSound(-1, 0, cl.sfx_ric3, pos2, 1, 1);
	}
}

// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_explosion (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_explosion);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleEffect(EFFECT_TE_EXPLOSION, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}

// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_tarexplosion (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_tarexplosion);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleEffect(EFFECT_TE_TAREXPLOSION, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}

// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_wizspike (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_wizspike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_WIZSPIKE, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	S_StartSound(-1, 0, cl.sfx_wizhit, pos2, 1, 1);
}

// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_knightspike (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_knightspike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_KNIGHTSPIKE, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
	S_StartSound(-1, 0, cl.sfx_knighthit, pos2, 1, 1);
}

// #425 void(vector org) te_lavasplash (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_lavasplash (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_te_lavasplash);
	CL_ParticleEffect(EFFECT_TE_LAVASPLASH, 1, PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM0), vec3_origin, vec3_origin, NULL, 0);
}

// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_teleport (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_te_teleport);
	CL_ParticleEffect(EFFECT_TE_TELEPORT, 1, PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM0), vec3_origin, vec3_origin, NULL, 0);
}

// #427 void(vector org, float colorstart, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_explosion2 (void)
{
	float		*pos;
	vec3_t		pos2, color;
	matrix4x4_t	tempmatrix;
	int			colorStart, colorLength;
	unsigned char		*tempcolor;
	VM_SAFEPARMCOUNT(3, VM_CL_te_explosion2);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	colorStart = (int)PRVM_G_FLOAT(OFS_PARM1);
	colorLength = (int)PRVM_G_FLOAT(OFS_PARM2);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleExplosion2(pos2, colorStart, colorLength);
	tempcolor = palette_rgb[(rand()%colorLength) + colorStart];
	color[0] = tempcolor[0] * (2.0f / 255.0f);
	color[1] = tempcolor[1] * (2.0f / 255.0f);
	color[2] = tempcolor[2] * (2.0f / 255.0f);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocLightFlash(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}


// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_lightning1 (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_lightning1);
	CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_bolt, true);
}

// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_lightning2 (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_lightning2);
	CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_bolt2, true);
}

// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_lightning3 (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_lightning3);
	CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_bolt3, false);
}

// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
static void VM_CL_te_beam (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_beam);
	CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_beam, false);
}

// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
static void VM_CL_te_plasmaburn (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_plasmaburn);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_PLASMABURN, 1, pos2, pos2, vec3_origin, vec3_origin, NULL, 0);
}

// #457 void(vector org, vector velocity, float howmany) te_flamejet (DP_TE_FLAMEJET)
static void VM_CL_te_flamejet (void)
{
	float *pos;
	vec3_t pos2;
	VM_SAFEPARMCOUNT(3, VM_CL_te_flamejet);
	if (PRVM_G_FLOAT(OFS_PARM2) < 1)
		return;
	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_ParticleEffect(EFFECT_TE_FLAMEJET, PRVM_G_FLOAT(OFS_PARM2), pos2, pos2, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM1), NULL, 0);
}


// #443 void(entity e, entity tagentity, string tagname) setattachment
void VM_CL_setattachment (void)
{
	prvm_edict_t *e;
	prvm_edict_t *tagentity;
	const char *tagname;
	int modelindex;
	int tagindex;
	dp_model_t *model;
	VM_SAFEPARMCOUNT(3, VM_CL_setattachment);

	e = PRVM_G_EDICT(OFS_PARM0);
	tagentity = PRVM_G_EDICT(OFS_PARM1);
	tagname = PRVM_G_STRING(OFS_PARM2);

	if (e == prog->edicts)
	{
		VM_Warning("setattachment: can not modify world entity\n");
		return;
	}
	if (e->priv.server->free)
	{
		VM_Warning("setattachment: can not modify free entity\n");
		return;
	}

	if (tagentity == NULL)
		tagentity = prog->edicts;

	tagindex = 0;
	if (tagentity != NULL && tagentity != prog->edicts && tagname && tagname[0])
	{
		modelindex = (int)PRVM_clientedictfloat(tagentity, modelindex);
		model = CL_GetModelByIndex(modelindex);
		if (model)
		{
			tagindex = Mod_Alias_GetTagIndexForName(model, (int)PRVM_clientedictfloat(tagentity, skin), tagname);
			if (tagindex == 0)
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity));
	}

	PRVM_clientedictedict(e, tag_entity) = PRVM_EDICT_TO_PROG(tagentity);
	PRVM_clientedictfloat(e, tag_index) = tagindex;
}

/////////////////////////////////////////
// DP_MD3_TAGINFO extension coded by VorteX

int CL_GetTagIndex (prvm_edict_t *e, const char *tagname)
{
	dp_model_t *model = CL_GetModelFromEdict(e);
	if (model)
		return Mod_Alias_GetTagIndexForName(model, (int)PRVM_clientedictfloat(e, skin), tagname);
	else
		return -1;
}

int CL_GetExtendedTagInfo (prvm_edict_t *e, int tagindex, int *parentindex, const char **tagname, matrix4x4_t *tag_localmatrix)
{
	int r;
	dp_model_t *model;

	*tagname = NULL;
	*parentindex = 0;
	Matrix4x4_CreateIdentity(tag_localmatrix);

	if (tagindex >= 0
	 && (model = CL_GetModelFromEdict(e))
	 && model->animscenes)
	{
		r = Mod_Alias_GetExtendedTagInfoForIndex(model, (int)PRVM_clientedictfloat(e, skin), e->priv.server->frameblend, &e->priv.server->skeleton, tagindex - 1, parentindex, tagname, tag_localmatrix);

		if(!r) // success?
			*parentindex += 1;

		return r;
	}

	return 1;
}

int CL_GetPitchSign(prvm_edict_t *ent)
{
	dp_model_t *model;
	if ((model = CL_GetModelFromEdict(ent)) && model->type == mod_alias)
		return -1;
	return 1;
}

void CL_GetEntityMatrix (prvm_edict_t *ent, matrix4x4_t *out, qboolean viewmatrix)
{
	float scale;
	float pitchsign = 1;

	scale = PRVM_clientedictfloat(ent, scale);
	if (!scale)
		scale = 1.0f;

	if(viewmatrix)
		*out = r_refdef.view.matrix;
	else if ((int)PRVM_clientedictfloat(ent, renderflags) & RF_USEAXIS)
	{
		vec3_t forward;
		vec3_t left;
		vec3_t up;
		vec3_t origin;
		VectorScale(PRVM_clientglobalvector(v_forward), scale, forward);
		VectorScale(PRVM_clientglobalvector(v_right), -scale, left);
		VectorScale(PRVM_clientglobalvector(v_up), scale, up);
		VectorCopy(PRVM_clientedictvector(ent, origin), origin);
		Matrix4x4_FromVectors(out, forward, left, up, origin);
	}
	else
	{
		pitchsign = CL_GetPitchSign(ent);
		Matrix4x4_CreateFromQuakeEntity(out, PRVM_clientedictvector(ent, origin)[0], PRVM_clientedictvector(ent, origin)[1], PRVM_clientedictvector(ent, origin)[2], pitchsign * PRVM_clientedictvector(ent, angles)[0], PRVM_clientedictvector(ent, angles)[1], PRVM_clientedictvector(ent, angles)[2], scale);
	}
}

int CL_GetEntityLocalTagMatrix(prvm_edict_t *ent, int tagindex, matrix4x4_t *out)
{
	dp_model_t *model;
	if (tagindex >= 0
	 && (model = CL_GetModelFromEdict(ent))
	 && model->animscenes)
	{
		VM_GenerateFrameGroupBlend(ent->priv.server->framegroupblend, ent);
		VM_FrameBlendFromFrameGroupBlend(ent->priv.server->frameblend, ent->priv.server->framegroupblend, model);
		VM_UpdateEdictSkeleton(ent, model, ent->priv.server->frameblend);
		return Mod_Alias_GetTagMatrix(model, ent->priv.server->frameblend, &ent->priv.server->skeleton, tagindex, out);
	}
	*out = identitymatrix;
	return 0;
}

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
int CL_GetTagMatrix (matrix4x4_t *out, prvm_edict_t *ent, int tagindex)
{
	int ret;
	int attachloop;
	matrix4x4_t entitymatrix, tagmatrix, attachmatrix;
	dp_model_t *model;

	*out = identitymatrix; // warnings and errors return identical matrix

	if (ent == prog->edicts)
		return 1;
	if (ent->priv.server->free)
		return 2;

	model = CL_GetModelFromEdict(ent);
	if(!model)
		return 3;

	tagmatrix = identitymatrix;
	attachloop = 0;
	for(;;)
	{
		if(attachloop >= 256)
			return 5;
		// apply transformation by child's tagindex on parent entity and then
		// by parent entity itself
		ret = CL_GetEntityLocalTagMatrix(ent, tagindex - 1, &attachmatrix);
		if(ret && attachloop == 0)
			return ret;
		CL_GetEntityMatrix(ent, &entitymatrix, false);
		Matrix4x4_Concat(&tagmatrix, &attachmatrix, out);
		Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
		// next iteration we process the parent entity
		if (PRVM_clientedictedict(ent, tag_entity))
		{
			tagindex = (int)PRVM_clientedictfloat(ent, tag_index);
			ent = PRVM_EDICT_NUM(PRVM_clientedictedict(ent, tag_entity));
		}
		else
			break;
		attachloop++;
	}

	// RENDER_VIEWMODEL magic
	if ((int)PRVM_clientedictfloat(ent, renderflags) & RF_VIEWMODEL)
	{
		Matrix4x4_Copy(&tagmatrix, out);

		CL_GetEntityMatrix(prog->edicts, &entitymatrix, true);
		Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);

		/*
		// Cl_bob, ported from rendering code
		if (PRVM_clientedictfloat(ent, health) > 0 && cl_bob.value && cl_bobcycle.value)
		{
			double bob, cycle;
			// LordHavoc: this code is *weird*, but not replacable (I think it
			// should be done in QC on the server, but oh well, quake is quake)
			// LordHavoc: figured out bobup: the time at which the sin is at 180
			// degrees (which allows lengthening or squishing the peak or valley)
			cycle = cl.time/cl_bobcycle.value;
			cycle -= (int)cycle;
			if (cycle < cl_bobup.value)
				cycle = sin(M_PI * cycle / cl_bobup.value);
			else
				cycle = sin(M_PI + M_PI * (cycle-cl_bobup.value)/(1.0 - cl_bobup.value));
			// bob is proportional to velocity in the xy plane
			// (don't count Z, or jumping messes it up)
			bob = sqrt(PRVM_clientedictvector(ent, velocity)[0]*PRVM_clientedictvector(ent, velocity)[0] + PRVM_clientedictvector(ent, velocity)[1]*PRVM_clientedictvector(ent, velocity)[1])*cl_bob.value;
			bob = bob*0.3 + bob*0.7*cycle;
			Matrix4x4_AdjustOrigin(out, 0, 0, bound(-7, bob, 4));
		}
		*/
	}
	return 0;
}

// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
void VM_CL_gettagindex (void)
{
	prvm_edict_t *ent;
	const char *tag_name;
	int tag_index;

	VM_SAFEPARMCOUNT(2, VM_CL_gettagindex);

	ent = PRVM_G_EDICT(OFS_PARM0);
	tag_name = PRVM_G_STRING(OFS_PARM1);
	if (ent == prog->edicts)
	{
		VM_Warning("VM_CL_gettagindex(entity #%i): can't affect world entity\n", PRVM_NUM_FOR_EDICT(ent));
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("VM_CL_gettagindex(entity #%i): can't affect free entity\n", PRVM_NUM_FOR_EDICT(ent));
		return;
	}

	tag_index = 0;
	if (!CL_GetModelFromEdict(ent))
		Con_DPrintf("VM_CL_gettagindex(entity #%i): null or non-precached model\n", PRVM_NUM_FOR_EDICT(ent));
	else
	{
		tag_index = CL_GetTagIndex(ent, tag_name);
		if (tag_index == 0)
			Con_DPrintf("VM_CL_gettagindex(entity #%i): tag \"%s\" not found\n", PRVM_NUM_FOR_EDICT(ent), tag_name);
	}
	PRVM_G_FLOAT(OFS_RETURN) = tag_index;
}

// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
void VM_CL_gettaginfo (void)
{
	prvm_edict_t *e;
	int tagindex;
	matrix4x4_t tag_matrix;
	matrix4x4_t tag_localmatrix;
	int parentindex;
	const char *tagname;
	int returncode;
	vec3_t fo, le, up, trans;
	const dp_model_t *model;

	VM_SAFEPARMCOUNT(2, VM_CL_gettaginfo);

	e = PRVM_G_EDICT(OFS_PARM0);
	tagindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	returncode = CL_GetTagMatrix(&tag_matrix, e, tagindex);
	Matrix4x4_ToVectors(&tag_matrix, PRVM_clientglobalvector(v_forward), le, PRVM_clientglobalvector(v_up), PRVM_G_VECTOR(OFS_RETURN));
	VectorScale(le, -1, PRVM_clientglobalvector(v_right));
	model = CL_GetModelFromEdict(e);
	VM_GenerateFrameGroupBlend(e->priv.server->framegroupblend, e);
	VM_FrameBlendFromFrameGroupBlend(e->priv.server->frameblend, e->priv.server->framegroupblend, model);
	VM_UpdateEdictSkeleton(e, model, e->priv.server->frameblend);
	CL_GetExtendedTagInfo(e, tagindex, &parentindex, &tagname, &tag_localmatrix);
	Matrix4x4_ToVectors(&tag_localmatrix, fo, le, up, trans);

	PRVM_clientglobalfloat(gettaginfo_parent) = parentindex;
	PRVM_clientglobalstring(gettaginfo_name) = tagname ? PRVM_SetTempString(tagname) : 0;
	VectorCopy(trans, PRVM_clientglobalvector(gettaginfo_offset));
	VectorCopy(fo, PRVM_clientglobalvector(gettaginfo_forward));
	VectorScale(le, -1, PRVM_clientglobalvector(gettaginfo_right));
	VectorCopy(up, PRVM_clientglobalvector(gettaginfo_up));

	switch(returncode)
	{
		case 1:
			VM_Warning("gettagindex: can't affect world entity\n");
			break;
		case 2:
			VM_Warning("gettagindex: can't affect free entity\n");
			break;
		case 3:
			Con_DPrintf("CL_GetTagMatrix(entity #%i): null or non-precached model\n", PRVM_NUM_FOR_EDICT(e));
			break;
		case 4:
			Con_DPrintf("CL_GetTagMatrix(entity #%i): model has no tag with requested index %i\n", PRVM_NUM_FOR_EDICT(e), tagindex);
			break;
		case 5:
			Con_DPrintf("CL_GetTagMatrix(entity #%i): runaway loop at attachment chain\n", PRVM_NUM_FOR_EDICT(e));
			break;
	}
}

//============================================================================

//====================
// DP_CSQC_SPAWNPARTICLE
// a QC hook to engine's CL_NewParticle
//====================

// particle theme struct
typedef struct vmparticletheme_s
{
	unsigned short typeindex;
	qboolean initialized;
	pblend_t blendmode;
	porientation_t orientation;
	int color1;
	int color2;
	int tex;
	float size;
	float sizeincrease;
	float alpha;
	float alphafade;
	float gravity;
	float bounce;
	float airfriction;
	float liquidfriction;
	float originjitter;
	float velocityjitter;
	qboolean qualityreduction;
	float lifetime;
	float stretch;
	int staincolor1;
	int staincolor2;
	int staintex;
	float stainalpha;
	float stainsize;
	float delayspawn;
	float delaycollision;
	float angle;
	float spin;
}vmparticletheme_t;

// particle spawner
typedef struct vmparticlespawner_s
{
	mempool_t			*pool;
	qboolean			initialized;
	qboolean			verified;
	vmparticletheme_t	*themes;
	int					max_themes;
	// global addresses
	float *particle_type;
	float *particle_blendmode; 
	float *particle_orientation;
	float *particle_color1;
	float *particle_color2;
	float *particle_tex;
	float *particle_size;
	float *particle_sizeincrease;
	float *particle_alpha;
	float *particle_alphafade;
	float *particle_time;
	float *particle_gravity;
	float *particle_bounce;
	float *particle_airfriction;
	float *particle_liquidfriction;
	float *particle_originjitter;
	float *particle_velocityjitter;
	float *particle_qualityreduction;
	float *particle_stretch;
	float *particle_staincolor1;
	float *particle_staincolor2;
	float *particle_stainalpha;
	float *particle_stainsize;
	float *particle_staintex;
	float *particle_delayspawn;
	float *particle_delaycollision;
	float *particle_angle;
	float *particle_spin;
}vmparticlespawner_t;

vmparticlespawner_t vmpartspawner;

// TODO: automatic max_themes grow
static void VM_InitParticleSpawner (int maxthemes)
{
	// bound max themes to not be an insane value
	if (maxthemes < 4)
		maxthemes = 4;
	if (maxthemes > 2048)
		maxthemes = 2048;
	// allocate and set up structure
	if (vmpartspawner.initialized) // reallocate
	{
		Mem_FreePool(&vmpartspawner.pool);
		memset(&vmpartspawner, 0, sizeof(vmparticlespawner_t));
	}
	vmpartspawner.pool = Mem_AllocPool("VMPARTICLESPAWNER", 0, NULL);
	vmpartspawner.themes = (vmparticletheme_t *)Mem_Alloc(vmpartspawner.pool, sizeof(vmparticletheme_t)*maxthemes);
	vmpartspawner.max_themes = maxthemes;
	vmpartspawner.initialized = true;
	vmpartspawner.verified = true;
	// get field addresses for fast querying (we can do 1000 calls of spawnparticle in a frame)
	vmpartspawner.particle_type = &PRVM_clientglobalfloat(particle_type);
	vmpartspawner.particle_blendmode = &PRVM_clientglobalfloat(particle_blendmode);
	vmpartspawner.particle_orientation = &PRVM_clientglobalfloat(particle_orientation);
	vmpartspawner.particle_color1 = PRVM_clientglobalvector(particle_color1);
	vmpartspawner.particle_color2 = PRVM_clientglobalvector(particle_color2);
	vmpartspawner.particle_tex = &PRVM_clientglobalfloat(particle_tex);
	vmpartspawner.particle_size = &PRVM_clientglobalfloat(particle_size);
	vmpartspawner.particle_sizeincrease = &PRVM_clientglobalfloat(particle_sizeincrease);
	vmpartspawner.particle_alpha = &PRVM_clientglobalfloat(particle_alpha);
	vmpartspawner.particle_alphafade = &PRVM_clientglobalfloat(particle_alphafade);
	vmpartspawner.particle_time = &PRVM_clientglobalfloat(particle_time);
	vmpartspawner.particle_gravity = &PRVM_clientglobalfloat(particle_gravity);
	vmpartspawner.particle_bounce = &PRVM_clientglobalfloat(particle_bounce);
	vmpartspawner.particle_airfriction = &PRVM_clientglobalfloat(particle_airfriction);
	vmpartspawner.particle_liquidfriction = &PRVM_clientglobalfloat(particle_liquidfriction);
	vmpartspawner.particle_originjitter = &PRVM_clientglobalfloat(particle_originjitter);
	vmpartspawner.particle_velocityjitter = &PRVM_clientglobalfloat(particle_velocityjitter);
	vmpartspawner.particle_qualityreduction = &PRVM_clientglobalfloat(particle_qualityreduction);
	vmpartspawner.particle_stretch = &PRVM_clientglobalfloat(particle_stretch);
	vmpartspawner.particle_staincolor1 = PRVM_clientglobalvector(particle_staincolor1);
	vmpartspawner.particle_staincolor2 = PRVM_clientglobalvector(particle_staincolor2);
	vmpartspawner.particle_stainalpha = &PRVM_clientglobalfloat(particle_stainalpha);
	vmpartspawner.particle_stainsize = &PRVM_clientglobalfloat(particle_stainsize);
	vmpartspawner.particle_staintex = &PRVM_clientglobalfloat(particle_staintex);
	vmpartspawner.particle_staintex = &PRVM_clientglobalfloat(particle_staintex);
	vmpartspawner.particle_delayspawn = &PRVM_clientglobalfloat(particle_delayspawn);
	vmpartspawner.particle_delaycollision = &PRVM_clientglobalfloat(particle_delaycollision);
	vmpartspawner.particle_angle = &PRVM_clientglobalfloat(particle_angle);
	vmpartspawner.particle_spin = &PRVM_clientglobalfloat(particle_spin);
	#undef getglobal
	#undef getglobalvector
}

// reset particle theme to default values
static void VM_ResetParticleTheme (vmparticletheme_t *theme)
{
	theme->initialized = true;
	theme->typeindex = pt_static;
	theme->blendmode = PBLEND_ADD;
	theme->orientation = PARTICLE_BILLBOARD;
	theme->color1 = 0x808080;
	theme->color2 = 0xFFFFFF;
	theme->tex = 63;
	theme->size = 2;
	theme->sizeincrease = 0;
	theme->alpha = 256;
	theme->alphafade = 512;
	theme->gravity = 0.0f;
	theme->bounce = 0.0f;
	theme->airfriction = 1.0f;
	theme->liquidfriction = 4.0f;
	theme->originjitter = 0.0f;
	theme->velocityjitter = 0.0f;
	theme->qualityreduction = false;
	theme->lifetime = 4;
	theme->stretch = 1;
	theme->staincolor1 = -1;
	theme->staincolor2 = -1;
	theme->staintex = -1;
	theme->delayspawn = 0.0f;
	theme->delaycollision = 0.0f;
	theme->angle = 0.0f;
	theme->spin = 0.0f;
}

// particle theme -> QC globals
void VM_CL_ParticleThemeToGlobals(vmparticletheme_t *theme)
{
	*vmpartspawner.particle_type = theme->typeindex;
	*vmpartspawner.particle_blendmode = theme->blendmode;
	*vmpartspawner.particle_orientation = theme->orientation;
	vmpartspawner.particle_color1[0] = (theme->color1 >> 16) & 0xFF; // VorteX: int only can store 0-255, not 0-256 which means 0 - 0,99609375...
	vmpartspawner.particle_color1[1] = (theme->color1 >> 8) & 0xFF;
	vmpartspawner.particle_color1[2] = (theme->color1 >> 0) & 0xFF;
	vmpartspawner.particle_color2[0] = (theme->color2 >> 16) & 0xFF;
	vmpartspawner.particle_color2[1] = (theme->color2 >> 8) & 0xFF;
	vmpartspawner.particle_color2[2] = (theme->color2 >> 0) & 0xFF;
	*vmpartspawner.particle_tex = (float)theme->tex;
	*vmpartspawner.particle_size = theme->size;
	*vmpartspawner.particle_sizeincrease = theme->sizeincrease;
	*vmpartspawner.particle_alpha = theme->alpha/256;
	*vmpartspawner.particle_alphafade = theme->alphafade/256;
	*vmpartspawner.particle_time = theme->lifetime;
	*vmpartspawner.particle_gravity = theme->gravity;
	*vmpartspawner.particle_bounce = theme->bounce;
	*vmpartspawner.particle_airfriction = theme->airfriction;
	*vmpartspawner.particle_liquidfriction = theme->liquidfriction;
	*vmpartspawner.particle_originjitter = theme->originjitter;
	*vmpartspawner.particle_velocityjitter = theme->velocityjitter;
	*vmpartspawner.particle_qualityreduction = theme->qualityreduction;
	*vmpartspawner.particle_stretch = theme->stretch;
	vmpartspawner.particle_staincolor1[0] = ((int)theme->staincolor1 >> 16) & 0xFF;
	vmpartspawner.particle_staincolor1[1] = ((int)theme->staincolor1 >> 8) & 0xFF;
	vmpartspawner.particle_staincolor1[2] = ((int)theme->staincolor1 >> 0) & 0xFF;
	vmpartspawner.particle_staincolor2[0] = ((int)theme->staincolor2 >> 16) & 0xFF;
	vmpartspawner.particle_staincolor2[1] = ((int)theme->staincolor2 >> 8) & 0xFF;
	vmpartspawner.particle_staincolor2[2] = ((int)theme->staincolor2 >> 0) & 0xFF;
	*vmpartspawner.particle_staintex = (float)theme->staintex;
	*vmpartspawner.particle_stainalpha = (float)theme->stainalpha/256;
	*vmpartspawner.particle_stainsize = (float)theme->stainsize;
	*vmpartspawner.particle_delayspawn = theme->delayspawn;
	*vmpartspawner.particle_delaycollision = theme->delaycollision;
	*vmpartspawner.particle_angle = theme->angle;
	*vmpartspawner.particle_spin = theme->spin;
}

// QC globals ->  particle theme
void VM_CL_ParticleThemeFromGlobals(vmparticletheme_t *theme)
{
	theme->typeindex = (unsigned short)*vmpartspawner.particle_type;
	theme->blendmode = (pblend_t)(int)*vmpartspawner.particle_blendmode;
	theme->orientation = (porientation_t)(int)*vmpartspawner.particle_orientation;
	theme->color1 = ((int)vmpartspawner.particle_color1[0] << 16) + ((int)vmpartspawner.particle_color1[1] << 8) + ((int)vmpartspawner.particle_color1[2]);
	theme->color2 = ((int)vmpartspawner.particle_color2[0] << 16) + ((int)vmpartspawner.particle_color2[1] << 8) + ((int)vmpartspawner.particle_color2[2]);
	theme->tex = (int)*vmpartspawner.particle_tex;
	theme->size = *vmpartspawner.particle_size;
	theme->sizeincrease = *vmpartspawner.particle_sizeincrease;
	theme->alpha = *vmpartspawner.particle_alpha*256;
	theme->alphafade = *vmpartspawner.particle_alphafade*256;
	theme->lifetime = *vmpartspawner.particle_time;
	theme->gravity = *vmpartspawner.particle_gravity;
	theme->bounce = *vmpartspawner.particle_bounce;
	theme->airfriction = *vmpartspawner.particle_airfriction;
	theme->liquidfriction = *vmpartspawner.particle_liquidfriction;
	theme->originjitter = *vmpartspawner.particle_originjitter;
	theme->velocityjitter = *vmpartspawner.particle_velocityjitter;
	theme->qualityreduction = (*vmpartspawner.particle_qualityreduction) ? true : false;
	theme->stretch = *vmpartspawner.particle_stretch;
	theme->staincolor1 = ((int)vmpartspawner.particle_staincolor1[0])*65536 + (int)(vmpartspawner.particle_staincolor1[1])*256 + (int)(vmpartspawner.particle_staincolor1[2]);
	theme->staincolor2 = (int)(vmpartspawner.particle_staincolor2[0])*65536 + (int)(vmpartspawner.particle_staincolor2[1])*256 + (int)(vmpartspawner.particle_staincolor2[2]);
	theme->staintex =(int)*vmpartspawner.particle_staintex;
	theme->stainalpha = *vmpartspawner.particle_stainalpha*256;
	theme->stainsize = *vmpartspawner.particle_stainsize;
	theme->delayspawn = *vmpartspawner.particle_delayspawn;
	theme->delaycollision = *vmpartspawner.particle_delaycollision;
	theme->angle = *vmpartspawner.particle_angle;
	theme->spin = *vmpartspawner.particle_spin;
}

// init particle spawner interface
// # float(float max_themes) initparticlespawner
void VM_CL_InitParticleSpawner (void)
{
	VM_SAFEPARMCOUNTRANGE(0, 1, VM_CL_InitParticleSpawner);
	VM_InitParticleSpawner((int)PRVM_G_FLOAT(OFS_PARM0));
	vmpartspawner.themes[0].initialized = true;
	VM_ResetParticleTheme(&vmpartspawner.themes[0]);
	PRVM_G_FLOAT(OFS_RETURN) = (vmpartspawner.verified == true) ? 1 : 0;
}

// void() resetparticle
void VM_CL_ResetParticle (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_ResetParticle);
	if (vmpartspawner.verified == false)
	{
		VM_Warning("VM_CL_ResetParticle: particle spawner not initialized\n");
		return;
	}
	VM_CL_ParticleThemeToGlobals(&vmpartspawner.themes[0]);
}

// void(float themenum) particletheme
void VM_CL_ParticleTheme (void)
{
	int themenum;

	VM_SAFEPARMCOUNT(1, VM_CL_ParticleTheme);
	if (vmpartspawner.verified == false)
	{
		VM_Warning("VM_CL_ParticleTheme: particle spawner not initialized\n");
		return;
	}
	themenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (themenum < 0 || themenum >= vmpartspawner.max_themes)
	{
		VM_Warning("VM_CL_ParticleTheme: bad theme number %i\n", themenum);
		VM_CL_ParticleThemeToGlobals(&vmpartspawner.themes[0]);
		return;
	}
	if (vmpartspawner.themes[themenum].initialized == false)
	{
		VM_Warning("VM_CL_ParticleTheme: theme #%i not exists\n", themenum);
		VM_CL_ParticleThemeToGlobals(&vmpartspawner.themes[0]);
		return;
	}
	// load particle theme into globals
	VM_CL_ParticleThemeToGlobals(&vmpartspawner.themes[themenum]);
}

// float() saveparticletheme
// void(float themenum) updateparticletheme
void VM_CL_ParticleThemeSave (void)
{
	int themenum;

	VM_SAFEPARMCOUNTRANGE(0, 1, VM_CL_ParticleThemeSave);
	if (vmpartspawner.verified == false)
	{
		VM_Warning("VM_CL_ParticleThemeSave: particle spawner not initialized\n");
		return;
	}
	// allocate new theme, save it and return
	if (prog->argc < 1)
	{
		for (themenum = 0; themenum < vmpartspawner.max_themes; themenum++)
			if (vmpartspawner.themes[themenum].initialized == false)
				break;
		if (themenum >= vmpartspawner.max_themes)
		{
			if (vmpartspawner.max_themes == 2048)
				VM_Warning("VM_CL_ParticleThemeSave: no free theme slots\n");
			else
				VM_Warning("VM_CL_ParticleThemeSave: no free theme slots, try initparticlespawner() with highter max_themes\n");
			PRVM_G_FLOAT(OFS_RETURN) = -1;
			return;
		}
		vmpartspawner.themes[themenum].initialized = true;
		VM_CL_ParticleThemeFromGlobals(&vmpartspawner.themes[themenum]);
		PRVM_G_FLOAT(OFS_RETURN) = themenum;
		return;
	}
	// update existing theme
	themenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (themenum < 0 || themenum >= vmpartspawner.max_themes)
	{
		VM_Warning("VM_CL_ParticleThemeSave: bad theme number %i\n", themenum);
		return;
	}
	vmpartspawner.themes[themenum].initialized = true;
	VM_CL_ParticleThemeFromGlobals(&vmpartspawner.themes[themenum]);
}

// void(float themenum) freeparticletheme
void VM_CL_ParticleThemeFree (void)
{
	int themenum;

	VM_SAFEPARMCOUNT(1, VM_CL_ParticleThemeFree);
	if (vmpartspawner.verified == false)
	{
		VM_Warning("VM_CL_ParticleThemeFree: particle spawner not initialized\n");
		return;
	}
	themenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	// check parms
	if (themenum <= 0 || themenum >= vmpartspawner.max_themes)
	{
		VM_Warning("VM_CL_ParticleThemeFree: bad theme number %i\n", themenum);
		return;
	}
	if (vmpartspawner.themes[themenum].initialized == false)
	{
		VM_Warning("VM_CL_ParticleThemeFree: theme #%i already freed\n", themenum);
		VM_CL_ParticleThemeToGlobals(&vmpartspawner.themes[0]);
		return;
	}
	// free theme
	VM_ResetParticleTheme(&vmpartspawner.themes[themenum]);
	vmpartspawner.themes[themenum].initialized = false;
}

// float(vector org, vector dir, [float theme]) particle
// returns 0 if failed, 1 if succesful
void VM_CL_SpawnParticle (void)
{
	float *org, *dir;
	vmparticletheme_t *theme;
	particle_t *part;
	int themenum;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_CL_SpawnParticle2);
	if (vmpartspawner.verified == false)
	{
		VM_Warning("VM_CL_SpawnParticle: particle spawner not initialized\n");
		PRVM_G_FLOAT(OFS_RETURN) = 0; 
		return;
	}
	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	
	if (prog->argc < 3) // global-set particle
	{
		part = CL_NewParticle(org, (unsigned short)*vmpartspawner.particle_type, ((int)(vmpartspawner.particle_color1[0]) << 16) + ((int)(vmpartspawner.particle_color1[1]) << 8) + ((int)(vmpartspawner.particle_color1[2])), ((int)vmpartspawner.particle_color2[0] << 16) + ((int)vmpartspawner.particle_color2[1] << 8) + ((int)vmpartspawner.particle_color2[2]), (int)*vmpartspawner.particle_tex, *vmpartspawner.particle_size, *vmpartspawner.particle_sizeincrease, *vmpartspawner.particle_alpha*256, *vmpartspawner.particle_alphafade*256, *vmpartspawner.particle_gravity, *vmpartspawner.particle_bounce, org[0], org[1], org[2], dir[0], dir[1], dir[2], *vmpartspawner.particle_airfriction, *vmpartspawner.particle_liquidfriction, *vmpartspawner.particle_originjitter, *vmpartspawner.particle_velocityjitter, (*vmpartspawner.particle_qualityreduction) ? true : false, *vmpartspawner.particle_time, *vmpartspawner.particle_stretch, (pblend_t)(int)*vmpartspawner.particle_blendmode, (porientation_t)(int)*vmpartspawner.particle_orientation, (int)(vmpartspawner.particle_staincolor1[0])*65536 + (int)(vmpartspawner.particle_staincolor1[1])*256 + (int)(vmpartspawner.particle_staincolor1[2]), (int)(vmpartspawner.particle_staincolor2[0])*65536 + (int)(vmpartspawner.particle_staincolor2[1])*256 + (int)(vmpartspawner.particle_staincolor2[2]), (int)*vmpartspawner.particle_staintex, *vmpartspawner.particle_stainalpha*256, *vmpartspawner.particle_stainsize, *vmpartspawner.particle_angle, *vmpartspawner.particle_spin, NULL);
		if (!part)
		{
			PRVM_G_FLOAT(OFS_RETURN) = 0; 
			return;
		}
		if (*vmpartspawner.particle_delayspawn)
			part->delayedspawn = cl.time + *vmpartspawner.particle_delayspawn;
		//if (*vmpartspawner.particle_delaycollision)
		//	part->delayedcollisions = cl.time + *vmpartspawner.particle_delaycollision;
	}
	else // quick themed particle
	{
		themenum = (int)PRVM_G_FLOAT(OFS_PARM2);
		if (themenum <= 0 || themenum >= vmpartspawner.max_themes)
		{
			VM_Warning("VM_CL_SpawnParticle: bad theme number %i\n", themenum);
			PRVM_G_FLOAT(OFS_RETURN) = 0; 
			return;
		}
		theme = &vmpartspawner.themes[themenum];
		part = CL_NewParticle(org, theme->typeindex, theme->color1, theme->color2, theme->tex, theme->size, theme->sizeincrease, theme->alpha, theme->alphafade, theme->gravity, theme->bounce, org[0], org[1], org[2], dir[0], dir[1], dir[2], theme->airfriction, theme->liquidfriction, theme->originjitter, theme->velocityjitter, theme->qualityreduction, theme->lifetime, theme->stretch, theme->blendmode, theme->orientation, theme->staincolor1, theme->staincolor2, theme->staintex, theme->stainalpha, theme->stainsize, theme->angle, theme->spin, NULL);
		if (!part)
		{
			PRVM_G_FLOAT(OFS_RETURN) = 0; 
			return;
		}
		if (theme->delayspawn)
			part->delayedspawn = cl.time + theme->delayspawn;
		//if (theme->delaycollision)
		//	part->delayedcollisions = cl.time + theme->delaycollision;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1; 
}

// float(vector org, vector dir, float spawndelay, float collisiondelay, [float theme]) delayedparticle
// returns 0 if failed, 1 if success
void VM_CL_SpawnParticleDelayed (void)
{
	float *org, *dir;
	vmparticletheme_t *theme;
	particle_t *part;
	int themenum;

	VM_SAFEPARMCOUNTRANGE(4, 5, VM_CL_SpawnParticle2);
	if (vmpartspawner.verified == false)
	{
		VM_Warning("VM_CL_SpawnParticle: particle spawner not initialized\n");
		PRVM_G_FLOAT(OFS_RETURN) = 0; 
		return;
	}
	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	if (prog->argc < 5) // global-set particle
		part = CL_NewParticle(org, (unsigned short)*vmpartspawner.particle_type, ((int)vmpartspawner.particle_color1[0] << 16) + ((int)vmpartspawner.particle_color1[1] << 8) + ((int)vmpartspawner.particle_color1[2]), ((int)vmpartspawner.particle_color2[0] << 16) + ((int)vmpartspawner.particle_color2[1] << 8) + ((int)vmpartspawner.particle_color2[2]), (int)*vmpartspawner.particle_tex, *vmpartspawner.particle_size, *vmpartspawner.particle_sizeincrease, *vmpartspawner.particle_alpha*256, *vmpartspawner.particle_alphafade*256, *vmpartspawner.particle_gravity, *vmpartspawner.particle_bounce, org[0], org[1], org[2], dir[0], dir[1], dir[2], *vmpartspawner.particle_airfriction, *vmpartspawner.particle_liquidfriction, *vmpartspawner.particle_originjitter, *vmpartspawner.particle_velocityjitter, (*vmpartspawner.particle_qualityreduction) ? true : false, *vmpartspawner.particle_time, *vmpartspawner.particle_stretch, (pblend_t)(int)*vmpartspawner.particle_blendmode, (porientation_t)(int)*vmpartspawner.particle_orientation, ((int)vmpartspawner.particle_staincolor1[0] << 16) + ((int)vmpartspawner.particle_staincolor1[1] << 8) + ((int)vmpartspawner.particle_staincolor1[2]), ((int)vmpartspawner.particle_staincolor2[0] << 16) + ((int)vmpartspawner.particle_staincolor2[1] << 8) + ((int)vmpartspawner.particle_staincolor2[2]), (int)*vmpartspawner.particle_staintex, *vmpartspawner.particle_stainalpha*256, *vmpartspawner.particle_stainsize, *vmpartspawner.particle_angle, *vmpartspawner.particle_spin, NULL);
	else // themed particle
	{
		themenum = (int)PRVM_G_FLOAT(OFS_PARM4);
		if (themenum <= 0 || themenum >= vmpartspawner.max_themes)
		{
			VM_Warning("VM_CL_SpawnParticle: bad theme number %i\n", themenum);
			PRVM_G_FLOAT(OFS_RETURN) = 0;  
			return;
		}
		theme = &vmpartspawner.themes[themenum];
		part = CL_NewParticle(org, theme->typeindex, theme->color1, theme->color2, theme->tex, theme->size, theme->sizeincrease, theme->alpha, theme->alphafade, theme->gravity, theme->bounce, org[0], org[1], org[2], dir[0], dir[1], dir[2], theme->airfriction, theme->liquidfriction, theme->originjitter, theme->velocityjitter, theme->qualityreduction, theme->lifetime, theme->stretch, theme->blendmode, theme->orientation, theme->staincolor1, theme->staincolor2, theme->staintex, theme->stainalpha, theme->stainsize, theme->angle, theme->spin, NULL);
	}
	if (!part) 
	{ 
		PRVM_G_FLOAT(OFS_RETURN) = 0; 
		return; 
	}
	part->delayedspawn = cl.time + PRVM_G_FLOAT(OFS_PARM2);
	//part->delayedcollisions = cl.time + PRVM_G_FLOAT(OFS_PARM3);
	PRVM_G_FLOAT(OFS_RETURN) = 0;
}

//====================
//CSQC engine entities query
//====================

// float(float entitynum, float whatfld) getentity;
// vector(float entitynum, float whatfld) getentityvec;
// querying engine-drawn entity
// VorteX: currently it's only tested with whatfld = 1..7
void VM_CL_GetEntity (void)
{
	int entnum, fieldnum;
	float org[3], v1[3], v2[3];
	VM_SAFEPARMCOUNT(2, VM_CL_GetEntityVec);

	entnum = PRVM_G_FLOAT(OFS_PARM0);
	if (entnum < 0 || entnum >= cl.num_entities)
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	fieldnum = PRVM_G_FLOAT(OFS_PARM1);
	switch(fieldnum)
	{
		case 0: // active state
			PRVM_G_FLOAT(OFS_RETURN) = cl.entities_active[entnum];
			break;
		case 1: // origin
			Matrix4x4_OriginFromMatrix(&cl.entities[entnum].render.matrix, PRVM_G_VECTOR(OFS_RETURN));
			break; 
		case 2: // forward
			Matrix4x4_ToVectors(&cl.entities[entnum].render.matrix, PRVM_G_VECTOR(OFS_RETURN), v1, v2, org);	
			break;
		case 3: // right
			Matrix4x4_ToVectors(&cl.entities[entnum].render.matrix, v1, PRVM_G_VECTOR(OFS_RETURN), v2, org);	
			break;
		case 4: // up
			Matrix4x4_ToVectors(&cl.entities[entnum].render.matrix, v1, v2, PRVM_G_VECTOR(OFS_RETURN), org);	
			break;
		case 5: // scale
			PRVM_G_FLOAT(OFS_RETURN) = Matrix4x4_ScaleFromMatrix(&cl.entities[entnum].render.matrix);
			break;	
		case 6: // origin + v_forward, v_right, v_up
			Matrix4x4_ToVectors(&cl.entities[entnum].render.matrix, PRVM_clientglobalvector(v_forward), PRVM_clientglobalvector(v_right), PRVM_clientglobalvector(v_up), PRVM_G_VECTOR(OFS_RETURN));	
			break;	
		case 7: // alpha
			PRVM_G_FLOAT(OFS_RETURN) = cl.entities[entnum].render.alpha;
			break;	
		case 8: // colormor
			VectorCopy(cl.entities[entnum].render.colormod, PRVM_G_VECTOR(OFS_RETURN));
			break;
		case 9: // pants colormod
			VectorCopy(cl.entities[entnum].render.colormap_pantscolor, PRVM_G_VECTOR(OFS_RETURN));
			break;
		case 10: // shirt colormod
			VectorCopy(cl.entities[entnum].render.colormap_shirtcolor, PRVM_G_VECTOR(OFS_RETURN));
			break;
		case 11: // skinnum
			PRVM_G_FLOAT(OFS_RETURN) = cl.entities[entnum].render.skinnum;
			break;	
		case 12: // mins
			VectorCopy(cl.entities[entnum].render.mins, PRVM_G_VECTOR(OFS_RETURN));		
			break;	
		case 13: // maxs
			VectorCopy(cl.entities[entnum].render.maxs, PRVM_G_VECTOR(OFS_RETURN));		
			break;	
		case 14: // absmin
			Matrix4x4_OriginFromMatrix(&cl.entities[entnum].render.matrix, org);
			VectorAdd(cl.entities[entnum].render.mins, org, PRVM_G_VECTOR(OFS_RETURN));		
			break;	
		case 15: // absmax
			Matrix4x4_OriginFromMatrix(&cl.entities[entnum].render.matrix, org);
			VectorAdd(cl.entities[entnum].render.maxs, org, PRVM_G_VECTOR(OFS_RETURN));		
			break;
		case 16: // light
			VectorMA(cl.entities[entnum].render.modellight_ambient, 0.5, cl.entities[entnum].render.modellight_diffuse, PRVM_G_VECTOR(OFS_RETURN));
			break;	
		default:
			PRVM_G_FLOAT(OFS_RETURN) = 0;
			break;
	}
}

//====================
//QC POLYGON functions
//====================

#define VMPOLYGONS_MAXPOINTS 64

typedef struct vmpolygons_triangle_s
{
	rtexture_t		*texture;
	int				drawflag;
	qboolean hasalpha;
	unsigned short	elements[3];
}vmpolygons_triangle_t;

typedef struct vmpolygons_s
{
	mempool_t		*pool;
	qboolean		initialized;
	double          progstarttime;

	int				max_vertices;
	int				num_vertices;
	float			*data_vertex3f;
	float			*data_color4f;
	float			*data_texcoord2f;

	int				max_triangles;
	int				num_triangles;
	vmpolygons_triangle_t *data_triangles;
	unsigned short	*data_sortedelement3s;

	qboolean		begin_active;
	int	begin_draw2d;
	rtexture_t		*begin_texture;
	int				begin_drawflag;
	int				begin_vertices;
	float			begin_vertex[VMPOLYGONS_MAXPOINTS][3];
	float			begin_color[VMPOLYGONS_MAXPOINTS][4];
	float			begin_texcoord[VMPOLYGONS_MAXPOINTS][2];
	qboolean		begin_texture_hasalpha;
} vmpolygons_t;

// FIXME: make VM_CL_R_Polygon functions use Debug_Polygon functions?
vmpolygons_t vmpolygons[PRVM_MAXPROGS];

//#304 void() renderscene (EXT_CSQC)
// moved that here to reset the polygons,
// resetting them earlier causes R_Mesh_Draw to be called with numvertices = 0
// --blub
void VM_CL_R_RenderScene (void)
{
	double t = Sys_DoubleTime();
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();
	VM_SAFEPARMCOUNT(0, VM_CL_R_RenderScene);

	// we need to update any RENDER_VIEWMODEL entities at this point because
	// csqc supplies its own view matrix
	CL_UpdateViewEntities();
	// now draw stuff!
	R_RenderView();

	polys->num_vertices = polys->num_triangles = 0;
	polys->progstarttime = prog->starttime;

	// callprofile fixing hack: do not include this time in what is counted for CSQC_UpdateView
	prog->functions[PRVM_clientfunction(CSQC_UpdateView)].totaltime -= Sys_DoubleTime() - t;
}

static void VM_ResizePolygons(vmpolygons_t *polys)
{
	float *oldvertex3f = polys->data_vertex3f;
	float *oldcolor4f = polys->data_color4f;
	float *oldtexcoord2f = polys->data_texcoord2f;
	vmpolygons_triangle_t *oldtriangles = polys->data_triangles;
	unsigned short *oldsortedelement3s = polys->data_sortedelement3s;
	polys->max_vertices = min(polys->max_triangles*3, 65536);
	polys->data_vertex3f = (float *)Mem_Alloc(polys->pool, polys->max_vertices*sizeof(float[3]));
	polys->data_color4f = (float *)Mem_Alloc(polys->pool, polys->max_vertices*sizeof(float[4]));
	polys->data_texcoord2f = (float *)Mem_Alloc(polys->pool, polys->max_vertices*sizeof(float[2]));
	polys->data_triangles = (vmpolygons_triangle_t *)Mem_Alloc(polys->pool, polys->max_triangles*sizeof(vmpolygons_triangle_t));
	polys->data_sortedelement3s = (unsigned short *)Mem_Alloc(polys->pool, polys->max_triangles*sizeof(unsigned short[3]));
	if (polys->num_vertices)
	{
		memcpy(polys->data_vertex3f, oldvertex3f, polys->num_vertices*sizeof(float[3]));
		memcpy(polys->data_color4f, oldcolor4f, polys->num_vertices*sizeof(float[4]));
		memcpy(polys->data_texcoord2f, oldtexcoord2f, polys->num_vertices*sizeof(float[2]));
	}
	if (polys->num_triangles)
	{
		memcpy(polys->data_triangles, oldtriangles, polys->num_triangles*sizeof(vmpolygons_triangle_t));
		memcpy(polys->data_sortedelement3s, oldsortedelement3s, polys->num_triangles*sizeof(unsigned short[3]));
	}
	if (oldvertex3f)
		Mem_Free(oldvertex3f);
	if (oldcolor4f)
		Mem_Free(oldcolor4f);
	if (oldtexcoord2f)
		Mem_Free(oldtexcoord2f);
	if (oldtriangles)
		Mem_Free(oldtriangles);
	if (oldsortedelement3s)
		Mem_Free(oldsortedelement3s);
}

static void VM_InitPolygons (vmpolygons_t* polys)
{
	memset(polys, 0, sizeof(*polys));
	polys->pool = Mem_AllocPool("VMPOLY", 0, NULL);
	polys->max_triangles = 1024;
	VM_ResizePolygons(polys);
	polys->initialized = true;
}

static void VM_DrawPolygonCallback (const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int surfacelistindex;
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();
	if(polys->progstarttime != prog->starttime) // from other progs? won't draw these (this can cause crashes!)
		return;
//	R_Mesh_ResetTextureState();
	R_EntityMatrix(&identitymatrix);
	GL_CullFace(GL_NONE);
	GL_DepthTest(true); // polys in 3D space shall always have depth test
	GL_DepthRange(0, 1);
	R_Mesh_PrepareVertices_Generic_Arrays(polys->num_vertices, polys->data_vertex3f, polys->data_color4f, polys->data_texcoord2f);

	for (surfacelistindex = 0;surfacelistindex < numsurfaces;)
	{
		int numtriangles = 0;
		rtexture_t *tex = polys->data_triangles[surfacelist[surfacelistindex]].texture;
		int drawflag = polys->data_triangles[surfacelist[surfacelistindex]].drawflag;
		DrawQ_ProcessDrawFlag(drawflag, polys->data_triangles[surfacelist[surfacelistindex]].hasalpha);
		R_SetupShader_Generic(tex, NULL, GL_MODULATE, 1, false);
		numtriangles = 0;
		for (;surfacelistindex < numsurfaces;surfacelistindex++)
		{
			if (polys->data_triangles[surfacelist[surfacelistindex]].texture != tex || polys->data_triangles[surfacelist[surfacelistindex]].drawflag != drawflag)
				break;
			VectorCopy(polys->data_triangles[surfacelist[surfacelistindex]].elements, polys->data_sortedelement3s + 3*numtriangles);
			numtriangles++;
		}
		R_Mesh_Draw(0, polys->num_vertices, 0, numtriangles, NULL, NULL, 0, polys->data_sortedelement3s, NULL, 0);
	}
}

void VMPolygons_Store(vmpolygons_t *polys)
{
	qboolean hasalpha;
	int i;

	// detect if we have alpha
	hasalpha = polys->begin_texture_hasalpha;
	for(i = 0; !hasalpha && (i < polys->begin_vertices); ++i)
		if(polys->begin_color[i][3] < 1)
			hasalpha = true;

	if (polys->begin_draw2d)
	{
		// draw the polygon as 2D immediately
		drawqueuemesh_t mesh;
		mesh.texture = polys->begin_texture;
		mesh.num_vertices = polys->begin_vertices;
		mesh.num_triangles = polys->begin_vertices-2;
		mesh.data_element3i = polygonelement3i;
		mesh.data_element3s = polygonelement3s;
		mesh.data_vertex3f = polys->begin_vertex[0];
		mesh.data_color4f = polys->begin_color[0];
		mesh.data_texcoord2f = polys->begin_texcoord[0];
		DrawQ_Mesh(&mesh, polys->begin_drawflag, hasalpha);
	}
	else
	{
		// queue the polygon as 3D for sorted transparent rendering later
		int i;
		if (polys->max_triangles < polys->num_triangles + polys->begin_vertices-2)
		{
			while (polys->max_triangles < polys->num_triangles + polys->begin_vertices-2)
				polys->max_triangles *= 2;
			VM_ResizePolygons(polys);
		}
		if (polys->num_vertices + polys->begin_vertices <= polys->max_vertices)
		{
			// needle in a haystack!
			// polys->num_vertices was used for copying where we actually want to copy begin_vertices
			// that also caused it to not render the first polygon that is added
			// --blub
			memcpy(polys->data_vertex3f + polys->num_vertices * 3, polys->begin_vertex[0], polys->begin_vertices * sizeof(float[3]));
			memcpy(polys->data_color4f + polys->num_vertices * 4, polys->begin_color[0], polys->begin_vertices * sizeof(float[4]));
			memcpy(polys->data_texcoord2f + polys->num_vertices * 2, polys->begin_texcoord[0], polys->begin_vertices * sizeof(float[2]));
			for (i = 0;i < polys->begin_vertices-2;i++)
			{
				polys->data_triangles[polys->num_triangles].texture = polys->begin_texture;
				polys->data_triangles[polys->num_triangles].drawflag = polys->begin_drawflag;
				polys->data_triangles[polys->num_triangles].elements[0] = polys->num_vertices;
				polys->data_triangles[polys->num_triangles].elements[1] = polys->num_vertices + i+1;
				polys->data_triangles[polys->num_triangles].elements[2] = polys->num_vertices + i+2;
				polys->data_triangles[polys->num_triangles].hasalpha = hasalpha;
				polys->num_triangles++;
			}
			polys->num_vertices += polys->begin_vertices;
		}
	}
	polys->begin_active = false;
}

// TODO: move this into the client code and clean-up everything else, too! [1/6/2008 Black]
// LordHavoc: agreed, this is a mess
void VM_CL_AddPolygonsToMeshQueue (void)
{
	int i;
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();
	vec3_t center;

	// only add polygons of the currently active prog to the queue - if there is none, we're done
	if( !prog )
		return;

	if (!polys->num_triangles)
		return;

	for (i = 0;i < polys->num_triangles;i++)
	{
		VectorMAMAM(1.0f / 3.0f, polys->data_vertex3f + 3*polys->data_triangles[i].elements[0], 1.0f / 3.0f, polys->data_vertex3f + 3*polys->data_triangles[i].elements[1], 1.0f / 3.0f, polys->data_vertex3f + 3*polys->data_triangles[i].elements[2], center);
		R_MeshQueue_AddTransparent(center, VM_DrawPolygonCallback, NULL, i, NULL);
	}

	/*polys->num_triangles = 0; // now done after rendering the scene,
	  polys->num_vertices = 0;  // otherwise it's not rendered at all and prints an error message --blub */
}

//void(string texturename, float flag[, float is2d]) R_BeginPolygon
void VM_CL_R_PolygonBegin (void)
{
	const char		*picname;
	skinframe_t     *sf;
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();
	int tf;

	// TODO instead of using skinframes here (which provides the benefit of
	// better management of flags, and is more suited for 3D rendering), what
	// about supporting Q3 shaders?

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_CL_R_PolygonBegin);

	if (!polys->initialized)
		VM_InitPolygons(polys);
	if(polys->progstarttime != prog->starttime)
	{
		// from another progs? then reset the polys first (fixes crashes on map change, because that can make skinframe textures invalid)
		polys->num_vertices = polys->num_triangles = 0;
		polys->progstarttime = prog->starttime;
	}
	if (polys->begin_active)
	{
		VM_Warning("VM_CL_R_PolygonBegin: called twice without VM_CL_R_PolygonBegin after first\n");
		return;
	}
	picname = PRVM_G_STRING(OFS_PARM0);

	sf = NULL;
	if(*picname)
	{
		tf = TEXF_ALPHA;
		if((int)PRVM_G_FLOAT(OFS_PARM1) & DRAWFLAG_MIPMAP)
			tf |= TEXF_MIPMAP;

		do
		{
			sf = R_SkinFrame_FindNextByName(sf, picname);
		}
		while(sf && sf->textureflags != tf);

		if(!sf || !sf->base)
			sf = R_SkinFrame_LoadExternal(picname, tf, true);

		if(sf)
			R_SkinFrame_MarkUsed(sf);
	}

	polys->begin_texture = (sf && sf->base) ? sf->base : r_texture_white;
	polys->begin_texture_hasalpha = (sf && sf->base) ? sf->hasalpha : false;
	polys->begin_drawflag = (int)PRVM_G_FLOAT(OFS_PARM1) & DRAWFLAG_MASK;
	polys->begin_vertices = 0;
	polys->begin_active = true;
	polys->begin_draw2d = (prog->argc >= 3 ? (int)PRVM_G_FLOAT(OFS_PARM2) : r_refdef.draw2dstage);
}

//void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex
void VM_CL_R_PolygonVertex (void)
{
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();

	VM_SAFEPARMCOUNT(4, VM_CL_R_PolygonVertex);

	if (!polys->begin_active)
	{
		VM_Warning("VM_CL_R_PolygonVertex: VM_CL_R_PolygonBegin wasn't called\n");
		return;
	}

	if (polys->begin_vertices >= VMPOLYGONS_MAXPOINTS)
	{
		VM_Warning("VM_CL_R_PolygonVertex: may have %i vertices max\n", VMPOLYGONS_MAXPOINTS);
		return;
	}

	polys->begin_vertex[polys->begin_vertices][0] = PRVM_G_VECTOR(OFS_PARM0)[0];
	polys->begin_vertex[polys->begin_vertices][1] = PRVM_G_VECTOR(OFS_PARM0)[1];
	polys->begin_vertex[polys->begin_vertices][2] = PRVM_G_VECTOR(OFS_PARM0)[2];
	polys->begin_texcoord[polys->begin_vertices][0] = PRVM_G_VECTOR(OFS_PARM1)[0];
	polys->begin_texcoord[polys->begin_vertices][1] = PRVM_G_VECTOR(OFS_PARM1)[1];
	polys->begin_color[polys->begin_vertices][0] = PRVM_G_VECTOR(OFS_PARM2)[0];
	polys->begin_color[polys->begin_vertices][1] = PRVM_G_VECTOR(OFS_PARM2)[1];
	polys->begin_color[polys->begin_vertices][2] = PRVM_G_VECTOR(OFS_PARM2)[2];
	polys->begin_color[polys->begin_vertices][3] = PRVM_G_FLOAT(OFS_PARM3);
	polys->begin_vertices++;
}

//void() R_EndPolygon
void VM_CL_R_PolygonEnd (void)
{
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();

	VM_SAFEPARMCOUNT(0, VM_CL_R_PolygonEnd);
	if (!polys->begin_active)
	{
		VM_Warning("VM_CL_R_PolygonEnd: VM_CL_R_PolygonBegin wasn't called\n");
		return;
	}
	polys->begin_active = false;
	if (polys->begin_vertices >= 3)
		VMPolygons_Store(polys);
	else
		VM_Warning("VM_CL_R_PolygonEnd: %i vertices isn't a good choice\n", polys->begin_vertices);
}

static vmpolygons_t debugPolys;

void Debug_PolygonBegin(const char *picname, int drawflag)
{
	if(!debugPolys.initialized)
		VM_InitPolygons(&debugPolys);
	if(debugPolys.begin_active)
	{
		Con_Printf("Debug_PolygonBegin: called twice without Debug_PolygonEnd after first\n");
		return;
	}
	debugPolys.begin_texture = picname[0] ? Draw_CachePic_Flags (picname, CACHEPICFLAG_NOTPERSISTENT)->tex : r_texture_white;
	debugPolys.begin_drawflag = drawflag;
	debugPolys.begin_vertices = 0;
	debugPolys.begin_active = true;
}

void Debug_PolygonVertex(float x, float y, float z, float s, float t, float r, float g, float b, float a)
{
	if(!debugPolys.begin_active)
	{
		Con_Printf("Debug_PolygonVertex: Debug_PolygonBegin wasn't called\n");
		return;
	}

	if(debugPolys.begin_vertices > VMPOLYGONS_MAXPOINTS)
	{
		Con_Printf("Debug_PolygonVertex: may have %i vertices max\n", VMPOLYGONS_MAXPOINTS);
		return;
	}

	debugPolys.begin_vertex[debugPolys.begin_vertices][0] = x;
	debugPolys.begin_vertex[debugPolys.begin_vertices][1] = y;
	debugPolys.begin_vertex[debugPolys.begin_vertices][2] = z;
	debugPolys.begin_texcoord[debugPolys.begin_vertices][0] = s;
	debugPolys.begin_texcoord[debugPolys.begin_vertices][1] = t;
	debugPolys.begin_color[debugPolys.begin_vertices][0] = r;
	debugPolys.begin_color[debugPolys.begin_vertices][1] = g;
	debugPolys.begin_color[debugPolys.begin_vertices][2] = b;
	debugPolys.begin_color[debugPolys.begin_vertices][3] = a;
	debugPolys.begin_vertices++;
}

void Debug_PolygonEnd(void)
{
	if (!debugPolys.begin_active)
	{
		Con_Printf("Debug_PolygonEnd: Debug_PolygonBegin wasn't called\n");
		return;
	}
	debugPolys.begin_active = false;
	if (debugPolys.begin_vertices >= 3)
		VMPolygons_Store(&debugPolys);
	else
		Con_Printf("Debug_PolygonEnd: %i vertices isn't a good choice\n", debugPolys.begin_vertices);
}

/*
=============
CL_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
qboolean CL_CheckBottom (prvm_edict_t *ent)
{
	vec3_t	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;

	VectorAdd (PRVM_clientedictvector(ent, origin), PRVM_clientedictvector(ent, mins), mins);
	VectorAdd (PRVM_clientedictvector(ent, origin), PRVM_clientedictvector(ent, maxs), maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (!(CL_PointSuperContents(start) & (SUPERCONTENTS_SOLID | SUPERCONTENTS_BODY)))
				goto realcheck;
		}

	return true;		// we got out easy

realcheck:
//
// check it for real...
//
	start[2] = mins[2];

// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*sv_stepheight.value;
	trace = CL_TraceLine(start, stop, MOVE_NOMONSTERS, ent, CL_GenericHitSuperContentsMask(ent), true, false, NULL, true, false);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = CL_TraceLine(start, stop, MOVE_NOMONSTERS, ent, CL_GenericHitSuperContentsMask(ent), true, false, NULL, true, false);

			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > sv_stepheight.value)
				return false;
		}

	return true;
}

/*
=============
CL_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done and false is returned
=============
*/
qboolean CL_movestep (prvm_edict_t *ent, vec3_t move, qboolean relink, qboolean noenemy, qboolean settrace)
{
	float		dz;
	vec3_t		oldorg, neworg, end, traceendpos;
	trace_t		trace;
	int			i, svent;
	prvm_edict_t		*enemy;

// try the move
	VectorCopy (PRVM_clientedictvector(ent, origin), oldorg);
	VectorAdd (PRVM_clientedictvector(ent, origin), move, neworg);

// flying monsters don't step up
	if ( (int)PRVM_clientedictfloat(ent, flags) & (FL_SWIM | FL_FLY) )
	{
	// try one move with vertical motion, then one without
		for (i=0 ; i<2 ; i++)
		{
			VectorAdd (PRVM_clientedictvector(ent, origin), move, neworg);
			enemy = PRVM_PROG_TO_EDICT(PRVM_clientedictedict(ent, enemy));
			if (i == 0 && enemy != prog->edicts)
			{
				dz = PRVM_clientedictvector(ent, origin)[2] - PRVM_clientedictvector(PRVM_PROG_TO_EDICT(PRVM_clientedictedict(ent, enemy)), origin)[2];
				if (dz > 40)
					neworg[2] -= 8;
				if (dz < 30)
					neworg[2] += 8;
			}
			trace = CL_TraceBox(PRVM_clientedictvector(ent, origin), PRVM_clientedictvector(ent, mins), PRVM_clientedictvector(ent, maxs), neworg, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, &svent, true);
			if (settrace)
				CL_VM_SetTraceGlobals(&trace, svent);

			if (trace.fraction == 1)
			{
				VectorCopy(trace.endpos, traceendpos);
				if (((int)PRVM_clientedictfloat(ent, flags) & FL_SWIM) && !(CL_PointSuperContents(traceendpos) & SUPERCONTENTS_LIQUIDSMASK))
					return false;	// swim monster left water

				VectorCopy (traceendpos, PRVM_clientedictvector(ent, origin));
				if (relink)
					CL_LinkEdict(ent);
				return true;
			}

			if (enemy == prog->edicts)
				break;
		}

		return false;
	}

// push down from a step height above the wished position
	neworg[2] += sv_stepheight.value;
	VectorCopy (neworg, end);
	end[2] -= sv_stepheight.value*2;

	trace = CL_TraceBox(neworg, PRVM_clientedictvector(ent, mins), PRVM_clientedictvector(ent, maxs), end, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, &svent, true);
	if (settrace)
		CL_VM_SetTraceGlobals(&trace, svent);

	if (trace.startsolid)
	{
		neworg[2] -= sv_stepheight.value;
		trace = CL_TraceBox(neworg, PRVM_clientedictvector(ent, mins), PRVM_clientedictvector(ent, maxs), end, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, &svent, true);
		if (settrace)
			CL_VM_SetTraceGlobals(&trace, svent);
		if (trace.startsolid)
			return false;
	}
	if (trace.fraction == 1)
	{
	// if monster had the ground pulled out, go ahead and fall
		if ( (int)PRVM_clientedictfloat(ent, flags) & FL_PARTIALGROUND )
		{
			VectorAdd (PRVM_clientedictvector(ent, origin), move, PRVM_clientedictvector(ent, origin));
			if (relink)
				CL_LinkEdict(ent);
			PRVM_clientedictfloat(ent, flags) = (int)PRVM_clientedictfloat(ent, flags) & ~FL_ONGROUND;
			return true;
		}

		return false;		// walked off an edge
	}

// check point traces down for dangling corners
	VectorCopy (trace.endpos, PRVM_clientedictvector(ent, origin));

	if (!CL_CheckBottom (ent))
	{
		if ( (int)PRVM_clientedictfloat(ent, flags) & FL_PARTIALGROUND )
		{	// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink)
				CL_LinkEdict(ent);
			return true;
		}
		VectorCopy (oldorg, PRVM_clientedictvector(ent, origin));
		return false;
	}

	if ( (int)PRVM_clientedictfloat(ent, flags) & FL_PARTIALGROUND )
		PRVM_clientedictfloat(ent, flags) = (int)PRVM_clientedictfloat(ent, flags) & ~FL_PARTIALGROUND;

	PRVM_clientedictedict(ent, groundentity) = PRVM_EDICT_TO_PROG(trace.ent);

// the move is ok
	if (relink)
		CL_LinkEdict(ent);
	return true;
}

/*
===============
VM_CL_walkmove

float(float yaw, float dist[, settrace]) walkmove
===============
*/
static void VM_CL_walkmove (void)
{
	prvm_edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	mfunction_t	*oldf;
	int 	oldself;
	qboolean	settrace;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_CL_walkmove);

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(PRVM_clientglobaledict(self));
	if (ent == prog->edicts)
	{
		VM_Warning("walkmove: can not modify world entity\n");
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("walkmove: can not modify free entity\n");
		return;
	}
	yaw = PRVM_G_FLOAT(OFS_PARM0);
	dist = PRVM_G_FLOAT(OFS_PARM1);
	settrace = prog->argc >= 3 && PRVM_G_FLOAT(OFS_PARM2);

	if ( !( (int)PRVM_clientedictfloat(ent, flags) & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
		return;

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because CL_movestep may call other progs
	oldf = prog->xfunction;
	oldself = PRVM_clientglobaledict(self);

	PRVM_G_FLOAT(OFS_RETURN) = CL_movestep(ent, move, true, false, settrace);


// restore program state
	prog->xfunction = oldf;
	PRVM_clientglobaledict(self) = oldself;
}

/*
===============
VM_CL_serverkey

string(string key) serverkey
===============
*/
void VM_CL_serverkey(void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNT(1, VM_CL_serverkey);
	InfoString_GetValue(cl.qw_serverinfo, PRVM_G_STRING(OFS_PARM0), string, sizeof(string));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
}

/*
=================
VM_CL_checkpvs

Checks if an entity is in a point's PVS.
Should be fast but can be inexact.

float checkpvs(vector viewpos, entity viewee) = #240;
=================
*/
static void VM_CL_checkpvs (void)
{
	vec3_t viewpos;
	prvm_edict_t *viewee;
	vec3_t mi, ma;
#if 1
	unsigned char *pvs;
#else
	int fatpvsbytes;
	unsigned char fatpvs[MAX_MAP_LEAFS/8];
#endif

	VM_SAFEPARMCOUNT(2, VM_SV_checkpvs);
	VectorCopy(PRVM_G_VECTOR(OFS_PARM0), viewpos);
	viewee = PRVM_G_EDICT(OFS_PARM1);

	if(viewee->priv.required->free)
	{
		VM_Warning("checkpvs: can not check free entity\n");
		PRVM_G_FLOAT(OFS_RETURN) = 4;
		return;
	}

	VectorAdd(PRVM_serveredictvector(viewee, origin), PRVM_serveredictvector(viewee, mins), mi);
	VectorAdd(PRVM_serveredictvector(viewee, origin), PRVM_serveredictvector(viewee, maxs), ma);

#if 1
	if(!sv.worldmodel->brush.GetPVS || !sv.worldmodel->brush.BoxTouchingPVS)
	{
		// no PVS support on this worldmodel... darn
		PRVM_G_FLOAT(OFS_RETURN) = 3;
		return;
	}
	pvs = sv.worldmodel->brush.GetPVS(sv.worldmodel, viewpos);
	if(!pvs)
	{
		// viewpos isn't in any PVS... darn
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, pvs, mi, ma);
#else
	// using fat PVS like FTEQW does (slow)
	if(!sv.worldmodel->brush.FatPVS || !sv.worldmodel->brush.BoxTouchingPVS)
	{
		// no PVS support on this worldmodel... darn
		PRVM_G_FLOAT(OFS_RETURN) = 3;
		return;
	}
	fatpvsbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, viewpos, 8, fatpvs, sizeof(fatpvs), false);
	if(!fatpvsbytes)
	{
		// viewpos isn't in any PVS... darn
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, fatpvs, mi, ma);
#endif
}

// #263 float(float modlindex) skel_create = #263; // (FTE_CSQC_SKELETONOBJECTS) create a skeleton (be sure to assign this value into .skeletonindex for use), returns skeleton index (1 or higher) on success, returns 0 on failure  (for example if the modelindex is not skeletal), it is recommended that you create a new skeleton if you change modelindex.
static void VM_CL_skel_create(void)
{
	int modelindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	dp_model_t *model = CL_GetModelByIndex(modelindex);
	skeleton_t *skeleton;
	int i;
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if (!model || !model->num_bones)
		return;
	for (i = 0;i < MAX_EDICTS;i++)
		if (!prog->skeletons[i])
			break;
	if (i == MAX_EDICTS)
		return;
	prog->skeletons[i] = skeleton = (skeleton_t *)Mem_Alloc(cls.levelmempool, sizeof(skeleton_t) + model->num_bones * sizeof(matrix4x4_t));
	PRVM_G_FLOAT(OFS_RETURN) = i + 1;
	skeleton->model = model;
	skeleton->relativetransforms = (matrix4x4_t *)(skeleton+1);
	// initialize to identity matrices
	for (i = 0;i < skeleton->model->num_bones;i++)
		skeleton->relativetransforms[i] = identitymatrix;
}

// #264 float(float skel, entity ent, float modlindex, float retainfrac, float firstbone, float lastbone) skel_build = #264; // (FTE_CSQC_SKELETONOBJECTS) blend in a percentage of standard animation, 0 replaces entirely, 1 does nothing, 0.5 blends half, etc, and this only alters the bones in the specified range for which out of bounds values like 0,100000 are safe (uses .frame, .frame2, .frame3, .frame4, .lerpfrac, .lerpfrac3, .lerpfrac4, .frame1time, .frame2time, .frame3time, .frame4time), returns skel on success, 0 on failure
static void VM_CL_skel_build(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	skeleton_t *skeleton;
	prvm_edict_t *ed = PRVM_G_EDICT(OFS_PARM1);
	int modelindex = (int)PRVM_G_FLOAT(OFS_PARM2);
	float retainfrac = PRVM_G_FLOAT(OFS_PARM3);
	int firstbone = PRVM_G_FLOAT(OFS_PARM4) - 1;
	int lastbone = PRVM_G_FLOAT(OFS_PARM5) - 1;
	dp_model_t *model = CL_GetModelByIndex(modelindex);
	float blendfrac;
	int numblends;
	int bonenum;
	int blendindex;
	framegroupblend_t framegroupblend[MAX_FRAMEGROUPBLENDS];
	frameblend_t frameblend[MAX_FRAMEBLENDS];
	matrix4x4_t blendedmatrix;
	matrix4x4_t matrix;
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	firstbone = max(0, firstbone);
	lastbone = min(lastbone, model->num_bones - 1);
	lastbone = min(lastbone, skeleton->model->num_bones - 1);
	VM_GenerateFrameGroupBlend(framegroupblend, ed);
	VM_FrameBlendFromFrameGroupBlend(frameblend, framegroupblend, model);
	blendfrac = 1.0f - retainfrac;
	for (numblends = 0;numblends < MAX_FRAMEBLENDS && frameblend[numblends].lerp;numblends++)
		frameblend[numblends].lerp *= blendfrac;
	for (bonenum = firstbone;bonenum <= lastbone;bonenum++)
	{
		memset(&blendedmatrix, 0, sizeof(blendedmatrix));
		Matrix4x4_Accumulate(&blendedmatrix, &skeleton->relativetransforms[bonenum], retainfrac);
		for (blendindex = 0;blendindex < numblends;blendindex++)
		{
			Matrix4x4_FromBonePose6s(&matrix, model->num_posescale, model->data_poses6s + 6 * (frameblend[blendindex].subframe * model->num_bones + bonenum));
			Matrix4x4_Accumulate(&blendedmatrix, &matrix, frameblend[blendindex].lerp);
		}
		skeleton->relativetransforms[bonenum] = blendedmatrix;
	}
	PRVM_G_FLOAT(OFS_RETURN) = skeletonindex + 1;
}

// #265 float(float skel) skel_get_numbones = #265; // (FTE_CSQC_SKELETONOBJECTS) returns how many bones exist in the created skeleton
static void VM_CL_skel_get_numbones(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	skeleton_t *skeleton;
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	PRVM_G_FLOAT(OFS_RETURN) = skeleton->model->num_bones;
}

// #266 string(float skel, float bonenum) skel_get_bonename = #266; // (FTE_CSQC_SKELETONOBJECTS) returns name of bone (as a tempstring)
static void VM_CL_skel_get_bonename(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int bonenum = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	skeleton_t *skeleton;
	PRVM_G_INT(OFS_RETURN) = 0;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	if (bonenum < 0 || bonenum >= skeleton->model->num_bones)
		return;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(skeleton->model->data_bones[bonenum].name);
}

// #267 float(float skel, float bonenum) skel_get_boneparent = #267; // (FTE_CSQC_SKELETONOBJECTS) returns parent num for supplied bonenum, 0 if bonenum has no parent or bone does not exist (returned value is always less than bonenum, you can loop on this)
static void VM_CL_skel_get_boneparent(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int bonenum = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	skeleton_t *skeleton;
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	if (bonenum < 0 || bonenum >= skeleton->model->num_bones)
		return;
	PRVM_G_FLOAT(OFS_RETURN) = skeleton->model->data_bones[bonenum].parent + 1;
}

// #268 float(float skel, string tagname) skel_find_bone = #268; // (FTE_CSQC_SKELETONOBJECTS) get number of bone with specified name, 0 on failure, tagindex (bonenum+1) on success, same as using gettagindex on the modelindex
static void VM_CL_skel_find_bone(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	const char *tagname = PRVM_G_STRING(OFS_PARM1);
	skeleton_t *skeleton;
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	PRVM_G_FLOAT(OFS_RETURN) = Mod_Alias_GetTagIndexForName(skeleton->model, 0, tagname);
}

// #269 vector(float skel, float bonenum) skel_get_bonerel = #269; // (FTE_CSQC_SKELETONOBJECTS) get matrix of bone in skeleton relative to its parent - sets v_forward, v_right, v_up, returns origin (relative to parent bone)
static void VM_CL_skel_get_bonerel(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int bonenum = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	skeleton_t *skeleton;
	matrix4x4_t matrix;
	vec3_t forward, left, up, origin;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	VectorClear(PRVM_clientglobalvector(v_forward));
	VectorClear(PRVM_clientglobalvector(v_right));
	VectorClear(PRVM_clientglobalvector(v_up));
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	if (bonenum < 0 || bonenum >= skeleton->model->num_bones)
		return;
	matrix = skeleton->relativetransforms[bonenum];
	Matrix4x4_ToVectors(&matrix, forward, left, up, origin);
	VectorCopy(forward, PRVM_clientglobalvector(v_forward));
	VectorNegate(left, PRVM_clientglobalvector(v_right));
	VectorCopy(up, PRVM_clientglobalvector(v_up));
	VectorCopy(origin, PRVM_G_VECTOR(OFS_RETURN));
}

// #270 vector(float skel, float bonenum) skel_get_boneabs = #270; // (FTE_CSQC_SKELETONOBJECTS) get matrix of bone in skeleton in model space - sets v_forward, v_right, v_up, returns origin (relative to entity)
static void VM_CL_skel_get_boneabs(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int bonenum = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	skeleton_t *skeleton;
	matrix4x4_t matrix;
	matrix4x4_t temp;
	vec3_t forward, left, up, origin;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	VectorClear(PRVM_clientglobalvector(v_forward));
	VectorClear(PRVM_clientglobalvector(v_right));
	VectorClear(PRVM_clientglobalvector(v_up));
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	if (bonenum < 0 || bonenum >= skeleton->model->num_bones)
		return;
	matrix = skeleton->relativetransforms[bonenum];
	// convert to absolute
	while ((bonenum = skeleton->model->data_bones[bonenum].parent) >= 0)
	{
		temp = matrix;
		Matrix4x4_Concat(&matrix, &skeleton->relativetransforms[bonenum], &temp);
	}
	Matrix4x4_ToVectors(&matrix, forward, left, up, origin);
	VectorCopy(forward, PRVM_clientglobalvector(v_forward));
	VectorNegate(left, PRVM_clientglobalvector(v_right));
	VectorCopy(up, PRVM_clientglobalvector(v_up));
	VectorCopy(origin, PRVM_G_VECTOR(OFS_RETURN));
}

// #271 void(float skel, float bonenum, vector org) skel_set_bone = #271; // (FTE_CSQC_SKELETONOBJECTS) set matrix of bone relative to its parent, reads v_forward, v_right, v_up, takes origin as parameter (relative to parent bone)
static void VM_CL_skel_set_bone(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int bonenum = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	vec3_t forward, left, up, origin;
	skeleton_t *skeleton;
	matrix4x4_t matrix;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	if (bonenum < 0 || bonenum >= skeleton->model->num_bones)
		return;
	VectorCopy(PRVM_clientglobalvector(v_forward), forward);
	VectorNegate(PRVM_clientglobalvector(v_right), left);
	VectorCopy(PRVM_clientglobalvector(v_up), up);
	VectorCopy(PRVM_G_VECTOR(OFS_PARM2), origin);
	Matrix4x4_FromVectors(&matrix, forward, left, up, origin);
	skeleton->relativetransforms[bonenum] = matrix;
}

// #272 void(float skel, float bonenum, vector org) skel_mul_bone = #272; // (FTE_CSQC_SKELETONOBJECTS) transform bone matrix (relative to its parent) by the supplied matrix in v_forward, v_right, v_up, takes origin as parameter (relative to parent bone)
static void VM_CL_skel_mul_bone(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int bonenum = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	vec3_t forward, left, up, origin;
	skeleton_t *skeleton;
	matrix4x4_t matrix;
	matrix4x4_t temp;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	if (bonenum < 0 || bonenum >= skeleton->model->num_bones)
		return;
	VectorCopy(PRVM_G_VECTOR(OFS_PARM2), origin);
	VectorCopy(PRVM_clientglobalvector(v_forward), forward);
	VectorNegate(PRVM_clientglobalvector(v_right), left);
	VectorCopy(PRVM_clientglobalvector(v_up), up);
	Matrix4x4_FromVectors(&matrix, forward, left, up, origin);
	temp = skeleton->relativetransforms[bonenum];
	Matrix4x4_Concat(&skeleton->relativetransforms[bonenum], &matrix, &temp);
}

// #273 void(float skel, float startbone, float endbone, vector org) skel_mul_bones = #273; // (FTE_CSQC_SKELETONOBJECTS) transform bone matrices (relative to their parents) by the supplied matrix in v_forward, v_right, v_up, takes origin as parameter (relative to parent bones)
static void VM_CL_skel_mul_bones(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int firstbone = PRVM_G_FLOAT(OFS_PARM1) - 1;
	int lastbone = PRVM_G_FLOAT(OFS_PARM2) - 1;
	int bonenum;
	vec3_t forward, left, up, origin;
	skeleton_t *skeleton;
	matrix4x4_t matrix;
	matrix4x4_t temp;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	VectorCopy(PRVM_G_VECTOR(OFS_PARM3), origin);
	VectorCopy(PRVM_clientglobalvector(v_forward), forward);
	VectorNegate(PRVM_clientglobalvector(v_right), left);
	VectorCopy(PRVM_clientglobalvector(v_up), up);
	Matrix4x4_FromVectors(&matrix, forward, left, up, origin);
	firstbone = max(0, firstbone);
	lastbone = min(lastbone, skeleton->model->num_bones - 1);
	for (bonenum = firstbone;bonenum <= lastbone;bonenum++)
	{
		temp = skeleton->relativetransforms[bonenum];
		Matrix4x4_Concat(&skeleton->relativetransforms[bonenum], &matrix, &temp);
	}
}

// #274 void(float skeldst, float skelsrc, float startbone, float endbone) skel_copybones = #274; // (FTE_CSQC_SKELETONOBJECTS) copy bone matrices (relative to their parents) from one skeleton to another, useful for copying a skeleton to a corpse
static void VM_CL_skel_copybones(void)
{
	int skeletonindexdst = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	int skeletonindexsrc = (int)PRVM_G_FLOAT(OFS_PARM1) - 1;
	int firstbone = PRVM_G_FLOAT(OFS_PARM2) - 1;
	int lastbone = PRVM_G_FLOAT(OFS_PARM3) - 1;
	int bonenum;
	skeleton_t *skeletondst;
	skeleton_t *skeletonsrc;
	if (skeletonindexdst < 0 || skeletonindexdst >= MAX_EDICTS || !(skeletondst = prog->skeletons[skeletonindexdst]))
		return;
	if (skeletonindexsrc < 0 || skeletonindexsrc >= MAX_EDICTS || !(skeletonsrc = prog->skeletons[skeletonindexsrc]))
		return;
	firstbone = max(0, firstbone);
	lastbone = min(lastbone, skeletondst->model->num_bones - 1);
	lastbone = min(lastbone, skeletonsrc->model->num_bones - 1);
	for (bonenum = firstbone;bonenum <= lastbone;bonenum++)
		skeletondst->relativetransforms[bonenum] = skeletonsrc->relativetransforms[bonenum];
}

// #275 void(float skel) skel_delete = #275; // (FTE_CSQC_SKELETONOBJECTS) deletes skeleton at the beginning of the next frame (you can add the entity, delete the skeleton, renderscene, and it will still work)
static void VM_CL_skel_delete(void)
{
	int skeletonindex = (int)PRVM_G_FLOAT(OFS_PARM0) - 1;
	skeleton_t *skeleton;
	if (skeletonindex < 0 || skeletonindex >= MAX_EDICTS || !(skeleton = prog->skeletons[skeletonindex]))
		return;
	Mem_Free(skeleton);
	prog->skeletons[skeletonindex] = NULL;
}

// #276 float(float modlindex, string framename) frameforname = #276; // (FTE_CSQC_SKELETONOBJECTS) finds number of a specified frame in the animation, returns -1 if no match found
static void VM_CL_frameforname(void)
{
	int modelindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	dp_model_t *model = CL_GetModelByIndex(modelindex);
	const char *name = PRVM_G_STRING(OFS_PARM1);
	int i;
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	if (!model || !model->animscenes)
		return;
	for (i = 0;i < model->numframes;i++)
	{
		if (!strcasecmp(model->animscenes[i].name, name))
		{
			PRVM_G_FLOAT(OFS_RETURN) = i;
			break;
		}
	}
}

// #277 float(float modlindex, float framenum) frameduration = #277; // (FTE_CSQC_SKELETONOBJECTS) returns the intended play time (in seconds) of the specified framegroup, if it does not exist the result is 0, if it is a single frame it may be a small value around 0.1 or 0.
static void VM_CL_frameduration(void)
{
	int modelindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	dp_model_t *model = CL_GetModelByIndex(modelindex);
	int framenum = (int)PRVM_G_FLOAT(OFS_PARM1);
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if (!model || !model->animscenes || framenum < 0 || framenum >= model->numframes)
		return;
	if (model->animscenes[framenum].framerate)
		PRVM_G_FLOAT(OFS_RETURN) = model->animscenes[framenum].framecount / model->animscenes[framenum].framerate;
}

void VM_CL_RotateMoves(void)
{
	/*
	 * Obscure builtin used by GAME_XONOTIC.
	 *
	 * Edits the input history of cl_movement by rotating all move commands
	 * currently in the queue using the given transform.
	 *
	 * The vector passed is an "angles transform" as used by warpzonelib, i.e.
	 * v_angle-like (non-inverted) euler angles that perform the rotation
	 * of the space that is to be done.
	 *
	 * This is meant to be used as a fixangle replacement after passing
	 * through a warpzone/portal: the client is told about the warp transform,
	 * and calls this function in the same frame as the one on which the
	 * client's origin got changed by the serverside teleport. Then this code
	 * transforms the pre-warp input (which matches the empty space behind
	 * the warp plane) into post-warp input (which matches the target area
	 * of the warp). Also, at the same time, the client has to use
	 * R_SetView to adjust VF_CL_VIEWANGLES according to the same transform.
	 *
	 * This together allows warpzone motion to be perfectly predicted by
	 * the client!
	 *
	 * Furthermore, for perfect warpzone behaviour, the server side also
	 * has to detect input the client sent before it received the origin
	 * update, but after the warp occurred on the server, and has to adjust
	 * input appropriately.
    */
	matrix4x4_t m;
	vec3_t v = {0, 0, 0};
	vec3_t x, y, z;
	VM_SAFEPARMCOUNT(1, VM_CL_RotateMoves);
	AngleVectorsFLU(PRVM_G_VECTOR(OFS_PARM0), x, y, z);
	Matrix4x4_FromVectors(&m, x, y, z, v);
	CL_RotateMoves(&m);
}

// #358 void(string cubemapname) loadcubemap
static void VM_CL_loadcubemap(void)
{
	const char *name;

	VM_SAFEPARMCOUNT(1, VM_CL_loadcubemap);
	name = PRVM_G_STRING(OFS_PARM0);
	R_GetCubemap(name);
}

//============================================================================

// To create a almost working builtin file from this replace:
// "^NULL.*" with ""
// "^{.*//.*}:Wh\(.*\)" with "\1"
// "\:" with "//"
// "^.*//:Wh{\#:d*}:Wh{.*}" with "\2 = \1;"
// "\n\n+" with "\n\n"

prvm_builtin_t vm_cl_builtins[] = {
NULL,							// #0 NULL function (not callable) (QUAKE)
VM_CL_makevectors,				// #1 void(vector ang) makevectors (QUAKE)
VM_CL_setorigin,				// #2 void(entity e, vector o) setorigin (QUAKE)
VM_CL_setmodel,					// #3 void(entity e, string m) setmodel (QUAKE)
VM_CL_setsize,					// #4 void(entity e, vector min, vector max) setsize (QUAKE)
NULL,							// #5 void(entity e, vector min, vector max) setabssize (QUAKE)
VM_break,						// #6 void() break (QUAKE)
VM_random,						// #7 float() random (QUAKE)
VM_CL_sound,					// #8 void(entity e, float chan, string samp) sound (QUAKE)
VM_normalize,					// #9 vector(vector v) normalize (QUAKE)
VM_error,						// #10 void(string e) error (QUAKE)
VM_objerror,					// #11 void(string e) objerror (QUAKE)
VM_vlen,						// #12 float(vector v) vlen (QUAKE)
VM_vectoyaw,					// #13 float(vector v) vectoyaw (QUAKE)
VM_CL_spawn,					// #14 entity() spawn (QUAKE)
VM_remove,						// #15 void(entity e) remove (QUAKE)
VM_CL_traceline,				// #16 void(vector v1, vector v2, float tryents, entity ignoreentity) traceline (QUAKE)
NULL,							// #17 entity() checkclient (QUAKE)
VM_find,						// #18 entity(entity start, .string fld, string match) find (QUAKE)
VM_precache_sound,				// #19 void(string s) precache_sound (QUAKE)
VM_CL_precache_model,			// #20 void(string s) precache_model (QUAKE)
NULL,							// #21 void(entity client, string s, ...) stuffcmd (QUAKE)
VM_CL_findradius,				// #22 entity(vector org, float rad) findradius (QUAKE)
NULL,							// #23 void(string s, ...) bprint (QUAKE)
NULL,							// #24 void(entity client, string s, ...) sprint (QUAKE)
VM_dprint,						// #25 void(string s, ...) dprint (QUAKE)
VM_ftos,						// #26 string(float f) ftos (QUAKE)
VM_vtos,						// #27 string(vector v) vtos (QUAKE)
VM_coredump,					// #28 void() coredump (QUAKE)
VM_traceon,						// #29 void() traceon (QUAKE)
VM_traceoff,					// #30 void() traceoff (QUAKE)
VM_eprint,						// #31 void(entity e) eprint (QUAKE)
VM_CL_walkmove,					// #32 float(float yaw, float dist[, float settrace]) walkmove (QUAKE)
NULL,							// #33 (QUAKE)
VM_CL_droptofloor,				// #34 float() droptofloor (QUAKE)
VM_CL_lightstyle,				// #35 void(float style, string value) lightstyle (QUAKE)
VM_rint,						// #36 float(float v) rint (QUAKE)
VM_floor,						// #37 float(float v) floor (QUAKE)
VM_ceil,						// #38 float(float v) ceil (QUAKE)
NULL,							// #39 (QUAKE)
VM_CL_checkbottom,				// #40 float(entity e) checkbottom (QUAKE)
VM_CL_pointcontents,			// #41 float(vector v) pointcontents (QUAKE)
NULL,							// #42 (QUAKE)
VM_fabs,						// #43 float(float f) fabs (QUAKE)
NULL,							// #44 vector(entity e, float speed) aim (QUAKE)
VM_cvar,						// #45 float(string s) cvar (QUAKE)
VM_localcmd,					// #46 void(string s) localcmd (QUAKE)
VM_nextent,						// #47 entity(entity e) nextent (QUAKE)
VM_CL_particle,					// #48 void(vector o, vector d, float color, float count) particle (QUAKE)
VM_changeyaw,					// #49 void() ChangeYaw (QUAKE)
NULL,							// #50 (QUAKE)
VM_vectoangles,					// #51 vector(vector v) vectoangles (QUAKE)
NULL,							// #52 void(float to, float f) WriteByte (QUAKE)
NULL,							// #53 void(float to, float f) WriteChar (QUAKE)
NULL,							// #54 void(float to, float f) WriteShort (QUAKE)
NULL,							// #55 void(float to, float f) WriteLong (QUAKE)
NULL,							// #56 void(float to, float f) WriteCoord (QUAKE)
NULL,							// #57 void(float to, float f) WriteAngle (QUAKE)
NULL,							// #58 void(float to, string s) WriteString (QUAKE)
NULL,							// #59 (QUAKE)
VM_sin,							// #60 float(float f) sin (DP_QC_SINCOSSQRTPOW)
VM_cos,							// #61 float(float f) cos (DP_QC_SINCOSSQRTPOW)
VM_sqrt,						// #62 float(float f) sqrt (DP_QC_SINCOSSQRTPOW)
VM_changepitch,					// #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
VM_CL_tracetoss,				// #64 void(entity e, entity ignore) tracetoss (DP_QC_TRACETOSS)
VM_etos,						// #65 string(entity ent) etos (DP_QC_ETOS)
NULL,							// #66 (QUAKE)
NULL,							// #67 void(float step) movetogoal (QUAKE)
VM_precache_file,				// #68 string(string s) precache_file (QUAKE)
VM_CL_makestatic,				// #69 void(entity e) makestatic (QUAKE)
NULL,							// #70 void(string s) changelevel (QUAKE)
NULL,							// #71 (QUAKE)
VM_cvar_set,					// #72 void(string var, string val) cvar_set (QUAKE)
NULL,							// #73 void(entity client, strings) centerprint (QUAKE)
VM_CL_ambientsound,				// #74 void(vector pos, string samp, float vol, float atten) ambientsound (QUAKE)
VM_CL_precache_model,			// #75 string(string s) precache_model2 (QUAKE)
VM_precache_sound,				// #76 string(string s) precache_sound2 (QUAKE)
VM_precache_file,				// #77 string(string s) precache_file2 (QUAKE)
NULL,							// #78 void(entity e) setspawnparms (QUAKE)
NULL,							// #79 void(entity killer, entity killee) logfrag (QUAKEWORLD)
NULL,							// #80 string(entity e, string keyname) infokey (QUAKEWORLD)
VM_stof,						// #81 float(string s) stof (FRIK_FILE)
NULL,							// #82 void(vector where, float set) multicast (QUAKEWORLD)
NULL,							// #83 (QUAKE)
NULL,							// #84 (QUAKE)
NULL,							// #85 (QUAKE)
NULL,							// #86 (QUAKE)
NULL,							// #87 (QUAKE)
NULL,							// #88 (QUAKE)
NULL,							// #89 (QUAKE)
VM_CL_tracebox,					// #90 void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox (DP_QC_TRACEBOX)
VM_randomvec,					// #91 vector() randomvec (DP_QC_RANDOMVEC)
VM_CL_getlight,					// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
VM_registercvar,				// #93 float(string name, string value) registercvar (DP_REGISTERCVAR)
VM_min,							// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
VM_max,							// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
VM_bound,						// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
VM_pow,							// #97 float(float f, float f) pow (DP_QC_SINCOSSQRTPOW)
VM_findfloat,					// #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
VM_checkextension,				// #99 float(string s) checkextension (the basis of the extension system)
// FrikaC and Telejano range #100-#199
NULL,							// #100
NULL,							// #101
NULL,							// #102
NULL,							// #103
NULL,							// #104
NULL,							// #105
NULL,							// #106
NULL,							// #107
NULL,							// #108
NULL,							// #109
VM_fopen,						// #110 float(string filename, float mode) fopen (FRIK_FILE)
VM_fclose,						// #111 void(float fhandle) fclose (FRIK_FILE)
VM_fgets,						// #112 string(float fhandle) fgets (FRIK_FILE)
VM_fputs,						// #113 void(float fhandle, string s) fputs (FRIK_FILE)
VM_strlen,						// #114 float(string s) strlen (FRIK_FILE)
VM_strcat,						// #115 string(string s1, string s2, ...) strcat (FRIK_FILE)
VM_substring,					// #116 string(string s, float start, float length) substring (FRIK_FILE)
VM_stov,						// #117 vector(string) stov (FRIK_FILE)
VM_strzone,						// #118 string(string s) strzone (FRIK_FILE)
VM_strunzone,					// #119 void(string s) strunzone (FRIK_FILE)
NULL,							// #120
NULL,							// #121
NULL,							// #122
NULL,							// #123
NULL,							// #124
NULL,							// #125
NULL,							// #126
NULL,							// #127
NULL,							// #128
NULL,							// #129
NULL,							// #130
NULL,							// #131
NULL,							// #132
NULL,							// #133
NULL,							// #134
NULL,							// #135
NULL,							// #136
NULL,							// #137
NULL,							// #138
NULL,							// #139
NULL,							// #140
NULL,							// #141
NULL,							// #142
NULL,							// #143
NULL,							// #144
NULL,							// #145
NULL,							// #146
NULL,							// #147
NULL,							// #148
NULL,							// #149
NULL,							// #150
NULL,							// #151
NULL,							// #152
NULL,							// #153
NULL,							// #154
NULL,							// #155
NULL,							// #156
NULL,							// #157
NULL,							// #158
NULL,							// #159
NULL,							// #160
NULL,							// #161
NULL,							// #162
NULL,							// #163
NULL,							// #164
NULL,							// #165
NULL,							// #166
NULL,							// #167
NULL,							// #168
NULL,							// #169
NULL,							// #170
NULL,							// #171
NULL,							// #172
NULL,							// #173
NULL,							// #174
NULL,							// #175
NULL,							// #176
NULL,							// #177
NULL,							// #178
NULL,							// #179
NULL,							// #180
NULL,							// #181
NULL,							// #182
NULL,							// #183
NULL,							// #184
NULL,							// #185
NULL,							// #186
NULL,							// #187
NULL,							// #188
NULL,							// #189
NULL,							// #190
NULL,							// #191
NULL,							// #192
NULL,							// #193
NULL,							// #194
NULL,							// #195
NULL,							// #196
NULL,							// #197
NULL,							// #198
NULL,							// #199
// FTEQW range #200-#299
NULL,							// #200
NULL,							// #201
NULL,							// #202
NULL,							// #203
NULL,							// #204
NULL,							// #205
NULL,							// #206
NULL,							// #207
NULL,							// #208
NULL,							// #209
NULL,							// #210
NULL,							// #211
NULL,							// #212
NULL,							// #213
NULL,							// #214
NULL,							// #215
NULL,							// #216
NULL,							// #217
VM_bitshift,					// #218 float(float number, float quantity) bitshift (EXT_BITSHIFT)
NULL,							// #219
NULL,							// #220
VM_strstrofs,					// #221 float(string str, string sub[, float startpos]) strstrofs (FTE_STRINGS)
VM_str2chr,						// #222 float(string str, float ofs) str2chr (FTE_STRINGS)
VM_chr2str,						// #223 string(float c, ...) chr2str (FTE_STRINGS)
VM_strconv,						// #224 string(float ccase, float calpha, float cnum, string s, ...) strconv (FTE_STRINGS)
VM_strpad,						// #225 string(float chars, string s, ...) strpad (FTE_STRINGS)
VM_infoadd,						// #226 string(string info, string key, string value, ...) infoadd (FTE_STRINGS)
VM_infoget,						// #227 string(string info, string key) infoget (FTE_STRINGS)
VM_strncmp,						// #228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
VM_strncasecmp,					// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)
VM_strncasecmp,					// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
NULL,							// #231
NULL,							// #232 void(float index, float type, .void field) SV_AddStat (EXT_CSQC)
NULL,							// #233
NULL,							// #234
NULL,							// #235
NULL,							// #236
NULL,							// #237
NULL,							// #238
NULL,							// #239
VM_CL_checkpvs,					// #240
NULL,							// #241
NULL,							// #242
NULL,							// #243
NULL,							// #244
NULL,							// #245
NULL,							// #246
NULL,							// #247
NULL,							// #248
NULL,							// #249
NULL,							// #250
NULL,							// #251
NULL,							// #252
NULL,							// #253
NULL,							// #254
NULL,							// #255
NULL,							// #256
NULL,							// #257
NULL,							// #258
NULL,							// #259
NULL,							// #260
NULL,							// #261
NULL,							// #262
VM_CL_skel_create,				// #263 float(float modlindex) skel_create = #263; // (FTE_CSQC_SKELETONOBJECTS) create a skeleton (be sure to assign this value into .skeletonindex for use), returns skeleton index (1 or higher) on success, returns 0 on failure  (for example if the modelindex is not skeletal), it is recommended that you create a new skeleton if you change modelindex.
VM_CL_skel_build,				// #264 float(float skel, entity ent, float modlindex, float retainfrac, float firstbone, float lastbone) skel_build = #264; // (FTE_CSQC_SKELETONOBJECTS) blend in a percentage of standard animation, 0 replaces entirely, 1 does nothing, 0.5 blends half, etc, and this only alters the bones in the specified range for which out of bounds values like 0,100000 are safe (uses .frame, .frame2, .frame3, .frame4, .lerpfrac, .lerpfrac3, .lerpfrac4, .frame1time, .frame2time, .frame3time, .frame4time), returns skel on success, 0 on failure
VM_CL_skel_get_numbones,		// #265 float(float skel) skel_get_numbones = #265; // (FTE_CSQC_SKELETONOBJECTS) returns how many bones exist in the created skeleton
VM_CL_skel_get_bonename,		// #266 string(float skel, float bonenum) skel_get_bonename = #266; // (FTE_CSQC_SKELETONOBJECTS) returns name of bone (as a tempstring)
VM_CL_skel_get_boneparent,		// #267 float(float skel, float bonenum) skel_get_boneparent = #267; // (FTE_CSQC_SKELETONOBJECTS) returns parent num for supplied bonenum, -1 if bonenum has no parent or bone does not exist (returned value is always less than bonenum, you can loop on this)
VM_CL_skel_find_bone,			// #268 float(float skel, string tagname) skel_find_bone = #268; // (FTE_CSQC_SKELETONOBJECTS) get number of bone with specified name, 0 on failure, tagindex (bonenum+1) on success, same as using gettagindex on the modelindex
VM_CL_skel_get_bonerel,			// #269 vector(float skel, float bonenum) skel_get_bonerel = #269; // (FTE_CSQC_SKELETONOBJECTS) get matrix of bone in skeleton relative to its parent - sets v_forward, v_right, v_up, returns origin (relative to parent bone)
VM_CL_skel_get_boneabs,			// #270 vector(float skel, float bonenum) skel_get_boneabs = #270; // (FTE_CSQC_SKELETONOBJECTS) get matrix of bone in skeleton in model space - sets v_forward, v_right, v_up, returns origin (relative to entity)
VM_CL_skel_set_bone,			// #271 void(float skel, float bonenum, vector org) skel_set_bone = #271; // (FTE_CSQC_SKELETONOBJECTS) set matrix of bone relative to its parent, reads v_forward, v_right, v_up, takes origin as parameter (relative to parent bone)
VM_CL_skel_mul_bone,			// #272 void(float skel, float bonenum, vector org) skel_mul_bone = #272; // (FTE_CSQC_SKELETONOBJECTS) transform bone matrix (relative to its parent) by the supplied matrix in v_forward, v_right, v_up, takes origin as parameter (relative to parent bone)
VM_CL_skel_mul_bones,			// #273 void(float skel, float startbone, float endbone, vector org) skel_mul_bones = #273; // (FTE_CSQC_SKELETONOBJECTS) transform bone matrices (relative to their parents) by the supplied matrix in v_forward, v_right, v_up, takes origin as parameter (relative to parent bones)
VM_CL_skel_copybones,			// #274 void(float skeldst, float skelsrc, float startbone, float endbone) skel_copybones = #274; // (FTE_CSQC_SKELETONOBJECTS) copy bone matrices (relative to their parents) from one skeleton to another, useful for copying a skeleton to a corpse
VM_CL_skel_delete,				// #275 void(float skel) skel_delete = #275; // (FTE_CSQC_SKELETONOBJECTS) deletes skeleton at the beginning of the next frame (you can add the entity, delete the skeleton, renderscene, and it will still work)
VM_CL_frameforname,				// #276 float(float modlindex, string framename) frameforname = #276; // (FTE_CSQC_SKELETONOBJECTS) finds number of a specified frame in the animation, returns -1 if no match found
VM_CL_frameduration,			// #277 float(float modlindex, float framenum) frameduration = #277; // (FTE_CSQC_SKELETONOBJECTS) returns the intended play time (in seconds) of the specified framegroup, if it does not exist the result is 0, if it is a single frame it may be a small value around 0.1 or 0.
NULL,							// #278
NULL,							// #279
NULL,							// #280
NULL,							// #281
NULL,							// #282
NULL,							// #283
NULL,							// #284
NULL,							// #285
NULL,							// #286
NULL,							// #287
NULL,							// #288
NULL,							// #289
NULL,							// #290
NULL,							// #291
NULL,							// #292
NULL,							// #293
NULL,							// #294
NULL,							// #295
NULL,							// #296
NULL,							// #297
NULL,							// #298
NULL,							// #299
// CSQC range #300-#399
VM_CL_R_ClearScene,				// #300 void() clearscene (EXT_CSQC)
VM_CL_R_AddEntities,			// #301 void(float mask) addentities (EXT_CSQC)
VM_CL_R_AddEntity,				// #302 void(entity ent) addentity (EXT_CSQC)
VM_CL_R_SetView,				// #303 float(float property, ...) setproperty (EXT_CSQC)
VM_CL_R_RenderScene,			// #304 void() renderscene (EXT_CSQC)
VM_CL_R_AddDynamicLight,		// #305 void(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)
VM_CL_R_PolygonBegin,			// #306 void(string texturename, float flag, float is2d[NYI: , float lines]) R_BeginPolygon
VM_CL_R_PolygonVertex,			// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex
VM_CL_R_PolygonEnd,				// #308 void() R_EndPolygon
NULL /* R_LoadWorldModel in menu VM, should stay unassigned in client*/, // #309
VM_CL_unproject,				// #310 vector (vector v) cs_unproject (EXT_CSQC)
VM_CL_project,					// #311 vector (vector v) cs_project (EXT_CSQC)
NULL,							// #312
NULL,							// #313
NULL,							// #314
VM_drawline,					// #315 void(float width, vector pos1, vector pos2, float flag) drawline (EXT_CSQC)
VM_iscachedpic,					// #316 float(string name) iscachedpic (EXT_CSQC)
VM_precache_pic,				// #317 string(string name, float trywad) precache_pic (EXT_CSQC)
VM_getimagesize,				// #318 vector(string picname) draw_getimagesize (EXT_CSQC)
VM_freepic,						// #319 void(string name) freepic (EXT_CSQC)
VM_drawcharacter,				// #320 float(vector position, float character, vector scale, vector rgb, float alpha, float flag) drawcharacter (EXT_CSQC)
VM_drawstring,					// #321 float(vector position, string text, vector scale, vector rgb, float alpha, float flag) drawstring (EXT_CSQC)
VM_drawpic,						// #322 float(vector position, string pic, vector size, vector rgb, float alpha, float flag) drawpic (EXT_CSQC)
VM_drawfill,					// #323 float(vector position, vector size, vector rgb, float alpha, float flag) drawfill (EXT_CSQC)
VM_drawsetcliparea,				// #324 void(float x, float y, float width, float height) drawsetcliparea
VM_drawresetcliparea,			// #325 void(void) drawresetcliparea
VM_drawcolorcodedstring,		// #326 float drawcolorcodedstring(vector position, string text, vector scale, vector rgb, float alpha, float flag) (EXT_CSQC)
VM_stringwidth,                 // #327 // FIXME is this okay?
VM_drawsubpic,					// #328 // FIXME is this okay?
VM_drawrotpic,					// #329 // FIXME is this okay?
VM_CL_getstatf,					// #330 float(float stnum) getstatf (EXT_CSQC)
VM_CL_getstati,					// #331 float(float stnum) getstati (EXT_CSQC)
VM_CL_getstats,					// #332 string(float firststnum) getstats (EXT_CSQC)
VM_CL_setmodelindex,			// #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
VM_CL_modelnameforindex,		// #334 string(float mdlindex) modelnameforindex (EXT_CSQC)
VM_CL_particleeffectnum,		// #335 float(string effectname) particleeffectnum (EXT_CSQC)
VM_CL_trailparticles,			// #336 void(entity ent, float effectnum, vector start, vector end) trailparticles (EXT_CSQC)
VM_CL_pointparticles,			// #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
VM_centerprint,					// #338 void(string s, ...) centerprint (EXT_CSQC)
VM_print,						// #339 void(string s, ...) print (EXT_CSQC, DP_SV_PRINT)
VM_keynumtostring,				// #340 string(float keynum) keynumtostring (EXT_CSQC)
VM_stringtokeynum,				// #341 float(string keyname) stringtokeynum (EXT_CSQC)
VM_getkeybind,					// #342 string(float keynum[, float bindmap]) getkeybind (EXT_CSQC)
VM_CL_setcursormode,			// #343 void(float usecursor) setcursormode (EXT_CSQC)
VM_CL_getmousepos,				// #344 vector() getmousepos (EXT_CSQC)
VM_CL_getinputstate,			// #345 float(float framenum) getinputstate (EXT_CSQC)
VM_CL_setsensitivityscale,		// #346 void(float sens) setsensitivityscale (EXT_CSQC)
VM_CL_runplayerphysics,			// #347 void() runstandardplayerphysics (EXT_CSQC)
VM_CL_getplayerkey,				// #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
VM_CL_isdemo,					// #349 float() isdemo (EXT_CSQC)
VM_isserver,					// #350 float() isserver (EXT_CSQC)
VM_CL_setlistener,				// #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
VM_CL_registercmd,				// #352 void(string cmdname) registercommand (EXT_CSQC)
VM_wasfreed,					// #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)
VM_CL_serverkey,				// #354 string(string key) serverkey (EXT_CSQC)
VM_CL_videoplaying,				// #355
VM_findfont,					// #356 float(string fontname) loadfont (DP_GFX_FONTS)
VM_loadfont,					// #357 float(string fontname, string fontmaps, string sizes, float slot) loadfont (DP_GFX_FONTS)
VM_CL_loadcubemap,				// #358 void(string cubemapname) loadcubemap (DP_GFX_)
NULL,							// #359
VM_CL_ReadByte,					// #360 float() readbyte (EXT_CSQC)
VM_CL_ReadChar,					// #361 float() readchar (EXT_CSQC)
VM_CL_ReadShort,				// #362 float() readshort (EXT_CSQC)
VM_CL_ReadLong,					// #363 float() readlong (EXT_CSQC)
VM_CL_ReadCoord,				// #364 float() readcoord (EXT_CSQC)
VM_CL_ReadAngle,				// #365 float() readangle (EXT_CSQC)
VM_CL_ReadString,				// #366 string() readstring (EXT_CSQC)
VM_CL_ReadFloat,				// #367 float() readfloat (EXT_CSQC)
NULL,						// #368
NULL,							// #369
NULL,							// #370
NULL,							// #371
NULL,							// #372
NULL,							// #373
NULL,							// #374
NULL,							// #375
NULL,							// #376
NULL,							// #377
NULL,							// #378
NULL,							// #379
NULL,							// #380
NULL,							// #381
NULL,							// #382
NULL,							// #383
NULL,							// #384
NULL,							// #385
NULL,							// #386
NULL,							// #387
NULL,							// #388
NULL,							// #389
NULL,							// #390
NULL,							// #391
NULL,							// #392
NULL,							// #393
NULL,							// #394
NULL,							// #395
NULL,							// #396
NULL,							// #397
NULL,							// #398
NULL,							// #399
// LordHavoc's range #400-#499
VM_CL_copyentity,				// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
NULL,							// #401 void(entity ent, float colors) setcolor (DP_QC_SETCOLOR)
VM_findchain,					// #402 entity(.string fld, string match) findchain (DP_QC_FINDCHAIN)
VM_findchainfloat,				// #403 entity(.float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
VM_CL_effect,					// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
VM_CL_te_blood,					// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
VM_CL_te_bloodshower,			// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
VM_CL_te_explosionrgb,			// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
VM_CL_te_particlecube,			// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
VM_CL_te_particlerain,			// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
VM_CL_te_particlesnow,			// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
VM_CL_te_spark,					// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
VM_CL_te_gunshotquad,			// #412 void(vector org) te_gunshotquad (DP_QUADEFFECTS1)
VM_CL_te_spikequad,				// #413 void(vector org) te_spikequad (DP_QUADEFFECTS1)
VM_CL_te_superspikequad,		// #414 void(vector org) te_superspikequad (DP_QUADEFFECTS1)
VM_CL_te_explosionquad,			// #415 void(vector org) te_explosionquad (DP_QUADEFFECTS1)
VM_CL_te_smallflash,			// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
VM_CL_te_customflash,			// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
VM_CL_te_gunshot,				// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_spike,					// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_superspike,			// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_explosion,				// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_tarexplosion,			// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_wizspike,				// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_knightspike,			// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_lavasplash,			// #425 void(vector org) te_lavasplash (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_teleport,				// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_explosion2,			// #427 void(vector org, float colorstart, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_lightning1,			// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_lightning2,			// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_lightning3,			// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
VM_CL_te_beam,					// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
VM_vectorvectors,				// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
VM_CL_te_plasmaburn,			// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
VM_getsurfacenumpoints,		// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
VM_getsurfacepoint,			// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
VM_getsurfacenormal,			// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
VM_getsurfacetexture,		// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
VM_getsurfacenearpoint,		// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
VM_getsurfaceclippedpoint,	// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
NULL,							// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_tokenize,					// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_argv,						// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_CL_setattachment,			// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
VM_search_begin,				// #444 float(string pattern, float caseinsensitive, float quiet) search_begin (DP_QC_FS_SEARCH)
VM_search_end,					// #445 void(float handle) search_end (DP_QC_FS_SEARCH)
VM_search_getsize,				// #446 float(float handle) search_getsize (DP_QC_FS_SEARCH)
VM_search_getfilename,			// #447 string(float handle, float num) search_getfilename (DP_QC_FS_SEARCH)
VM_cvar_string,					// #448 string(string s) cvar_string (DP_QC_CVAR_STRING)
VM_findflags,					// #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
VM_findchainflags,				// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
VM_CL_gettagindex,				// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
VM_CL_gettaginfo,				// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
NULL,							// #453 void(entity clent) dropclient (DP_SV_DROPCLIENT)
NULL,							// #454 entity() spawnclient (DP_SV_BOTCLIENT)
NULL,							// #455 float(entity clent) clienttype (DP_SV_BOTCLIENT)
NULL,							// #456 void(float to, string s) WriteUnterminatedString (DP_SV_WRITEUNTERMINATEDSTRING)
VM_CL_te_flamejet,				// #457 void(vector org, vector vel, float howmany) te_flamejet (DP_TE_FLAMEJET)
NULL,							// #458
VM_ftoe,						// #459 entity(float num) entitybyindex (DP_QC_EDICT_NUM)
VM_buf_create,					// #460 float() buf_create (DP_QC_STRINGBUFFERS)
VM_buf_del,						// #461 void(float bufhandle) buf_del (DP_QC_STRINGBUFFERS)
VM_buf_getsize,					// #462 float(float bufhandle) buf_getsize (DP_QC_STRINGBUFFERS)
VM_buf_copy,					// #463 void(float bufhandle_from, float bufhandle_to) buf_copy (DP_QC_STRINGBUFFERS)
VM_buf_sort,					// #464 void(float bufhandle, float sortpower, float backward) buf_sort (DP_QC_STRINGBUFFERS)
VM_buf_implode,					// #465 string(float bufhandle, string glue) buf_implode (DP_QC_STRINGBUFFERS)
VM_bufstr_get,					// #466 string(float bufhandle, float string_index) bufstr_get (DP_QC_STRINGBUFFERS)
VM_bufstr_set,					// #467 void(float bufhandle, float string_index, string str) bufstr_set (DP_QC_STRINGBUFFERS)
VM_bufstr_add,					// #468 float(float bufhandle, string str, float order) bufstr_add (DP_QC_STRINGBUFFERS)
VM_bufstr_free,					// #469 void(float bufhandle, float string_index) bufstr_free (DP_QC_STRINGBUFFERS)
NULL,							// #470 void(float index, float type, .void field) SV_AddStat (EXT_CSQC)
VM_asin,						// #471 float(float s) VM_asin (DP_QC_ASINACOSATANATAN2TAN)
VM_acos,						// #472 float(float c) VM_acos (DP_QC_ASINACOSATANATAN2TAN)
VM_atan,						// #473 float(float t) VM_atan (DP_QC_ASINACOSATANATAN2TAN)
VM_atan2,						// #474 float(float c, float s) VM_atan2 (DP_QC_ASINACOSATANATAN2TAN)
VM_tan,							// #475 float(float a) VM_tan (DP_QC_ASINACOSATANATAN2TAN)
VM_strlennocol,					// #476 float(string s) : DRESK - String Length (not counting color codes) (DP_QC_STRINGCOLORFUNCTIONS)
VM_strdecolorize,				// #477 string(string s) : DRESK - Decolorized String (DP_QC_STRINGCOLORFUNCTIONS)
VM_strftime,					// #478 string(float uselocaltime, string format, ...) (DP_QC_STRFTIME)
VM_tokenizebyseparator,			// #479 float(string s) tokenizebyseparator (DP_QC_TOKENIZEBYSEPARATOR)
VM_strtolower,					// #480 string(string s) VM_strtolower (DP_QC_STRING_CASE_FUNCTIONS)
VM_strtoupper,					// #481 string(string s) VM_strtoupper (DP_QC_STRING_CASE_FUNCTIONS)
VM_cvar_defstring,				// #482 string(string s) cvar_defstring (DP_QC_CVAR_DEFSTRING)
VM_CL_pointsound,				// #483 void(vector origin, string sample, float volume, float attenuation) pointsound (DP_SV_POINTSOUND)
VM_strreplace,					// #484 string(string search, string replace, string subject) strreplace (DP_QC_STRREPLACE)
VM_strireplace,					// #485 string(string search, string replace, string subject) strireplace (DP_QC_STRREPLACE)
VM_getsurfacepointattribute,// #486 vector(entity e, float s, float n, float a) getsurfacepointattribute
VM_gecko_create,					// #487 float gecko_create( string name )
VM_gecko_destroy,					// #488 void gecko_destroy( string name )
VM_gecko_navigate,				// #489 void gecko_navigate( string name, string URI )
VM_gecko_keyevent,				// #490 float gecko_keyevent( string name, float key, float eventtype )
VM_gecko_movemouse,				// #491 void gecko_mousemove( string name, float x, float y )
VM_gecko_resize,					// #492 void gecko_resize( string name, float w, float h )
VM_gecko_get_texture_extent,	// #493 vector gecko_get_texture_extent( string name )
VM_crc16,						// #494 float(float caseinsensitive, string s, ...) crc16 = #494 (DP_QC_CRC16)
VM_cvar_type,					// #495 float(string name) cvar_type = #495; (DP_QC_CVAR_TYPE)
VM_numentityfields,				// #496 float() numentityfields = #496; (QP_QC_ENTITYDATA)
VM_entityfieldname,				// #497 string(float fieldnum) entityfieldname = #497; (DP_QC_ENTITYDATA)
VM_entityfieldtype,				// #498 float(float fieldnum) entityfieldtype = #498; (DP_QC_ENTITYDATA)
VM_getentityfieldstring,		// #499 string(float fieldnum, entity ent) getentityfieldstring = #499; (DP_QC_ENTITYDATA)
VM_putentityfieldstring,		// #500 float(float fieldnum, entity ent, string s) putentityfieldstring = #500; (DP_QC_ENTITYDATA)
VM_CL_ReadPicture,				// #501 string() ReadPicture = #501;
VM_CL_boxparticles,				// #502 void(float effectnum, entity own, vector origin_from, vector origin_to, vector dir_from, vector dir_to, float count) boxparticles (DP_CSQC_BOXPARTICLES)
VM_whichpack,					// #503 string(string) whichpack = #503;
VM_CL_GetEntity,				// #504 float(float entitynum, float fldnum) getentity = #504; vector(float entitynum, float fldnum) getentityvec = #504;
NULL,							// #505
NULL,							// #506
NULL,							// #507
NULL,							// #508
NULL,							// #509
VM_uri_escape,					// #510 string(string in) uri_escape = #510;
VM_uri_unescape,				// #511 string(string in) uri_unescape = #511;
VM_etof,					// #512 float(entity ent) num_for_edict = #512 (DP_QC_NUM_FOR_EDICT)
VM_uri_get,						// #513 float(string uri, float id, [string post_contenttype, string post_delim, [float buf]]) uri_get = #513; (DP_QC_URI_GET, DP_QC_URI_POST)
VM_tokenize_console,					// #514 float(string str) tokenize_console = #514; (DP_QC_TOKENIZE_CONSOLE)
VM_argv_start_index,					// #515 float(float idx) argv_start_index = #515; (DP_QC_TOKENIZE_CONSOLE)
VM_argv_end_index,						// #516 float(float idx) argv_end_index = #516; (DP_QC_TOKENIZE_CONSOLE)
VM_buf_cvarlist,						// #517 void(float buf, string prefix, string antiprefix) buf_cvarlist = #517; (DP_QC_STRINGBUFFERS_CVARLIST)
VM_cvar_description,					// #518 float(string name) cvar_description = #518; (DP_QC_CVAR_DESCRIPTION)
VM_gettime,						// #519 float(float timer) gettime = #519; (DP_QC_GETTIME)
VM_keynumtostring,				// #520 string keynumtostring(float keynum)
VM_findkeysforcommand,			// #521 string findkeysforcommand(string command[, float bindmap])
VM_CL_InitParticleSpawner,		// #522 void(float max_themes) initparticlespawner (DP_CSQC_SPAWNPARTICLE)
VM_CL_ResetParticle,			// #523 void() resetparticle (DP_CSQC_SPAWNPARTICLE)
VM_CL_ParticleTheme,			// #524 void(float theme) particletheme (DP_CSQC_SPAWNPARTICLE)
VM_CL_ParticleThemeSave,		// #525 void() particlethemesave, void(float theme) particlethemeupdate (DP_CSQC_SPAWNPARTICLE)
VM_CL_ParticleThemeFree,		// #526 void() particlethemefree (DP_CSQC_SPAWNPARTICLE)
VM_CL_SpawnParticle,			// #527 float(vector org, vector vel, [float theme]) particle (DP_CSQC_SPAWNPARTICLE)
VM_CL_SpawnParticleDelayed,		// #528 float(vector org, vector vel, float delay, float collisiondelay, [float theme]) delayedparticle (DP_CSQC_SPAWNPARTICLE)
VM_loadfromdata,				// #529
VM_loadfromfile,				// #530
VM_CL_setpause,					// #531 float(float ispaused) setpause = #531 (DP_CSQC_SETPAUSE)
VM_log,							// #532
VM_getsoundtime,				// #533 float(entity e, float channel) getsoundtime = #533; (DP_SND_GETSOUNDTIME)
VM_soundlength,					// #534 float(string sample) soundlength = #534; (DP_SND_GETSOUNDTIME)
NULL,							// #535
NULL,							// #536
NULL,							// #537
NULL,							// #538
NULL,							// #539
VM_physics_enable,				// #540 void(entity e, float physics_enabled) physics_enable = #540; (DP_PHYSICS_ODE)
VM_physics_addforce,			// #541 void(entity e, vector force, vector relative_ofs) physics_addforce = #541; (DP_PHYSICS_ODE)
VM_physics_addtorque,			// #542 void(entity e, vector torque) physics_addtorque = #542; (DP_PHYSICS_ODE)
NULL,							// #543
NULL,							// #544
NULL,							// #545
NULL,							// #546
NULL,							// #547
NULL,							// #548
NULL,							// #549
NULL,							// #550
NULL,							// #551
NULL,							// #552
NULL,							// #553
NULL,							// #554
NULL,							// #555
NULL,							// #556
NULL,							// #557
NULL,							// #558
NULL,							// #559
NULL,							// #560
NULL,							// #561
NULL,							// #562
NULL,							// #563
NULL,							// #564
NULL,							// #565
NULL,							// #566
NULL,							// #567
NULL,							// #568
NULL,							// #569
NULL,							// #570
NULL,							// #571
NULL,							// #572
NULL,							// #573
NULL,							// #574
NULL,							// #575
NULL,							// #576
NULL,							// #577
NULL,							// #578
NULL,							// #579
NULL,							// #580
NULL,							// #581
NULL,							// #582
NULL,							// #583
NULL,							// #584
NULL,							// #585
NULL,							// #586
NULL,							// #587
NULL,							// #588
NULL,							// #589
NULL,							// #590
NULL,							// #591
NULL,							// #592
NULL,							// #593
NULL,							// #594
NULL,							// #595
NULL,							// #596
NULL,							// #597
NULL,							// #598
NULL,							// #599
NULL,							// #600
NULL,							// #601
NULL,							// #602
NULL,							// #603
NULL,							// #604
VM_callfunction,				// #605
VM_writetofile,					// #606
VM_isfunction,					// #607
NULL,							// #608
NULL,							// #609
VM_findkeysforcommand,			// #610 string findkeysforcommand(string command[, float bindmap])
NULL,							// #611
NULL,							// #612
VM_parseentitydata,				// #613
NULL,							// #614
NULL,							// #615
NULL,							// #616
NULL,							// #617
NULL,							// #618
NULL,							// #619
NULL,							// #620
NULL,							// #621
NULL,							// #622
NULL,							// #623
VM_CL_getextresponse,			// #624 string getextresponse(void)
NULL,							// #625
NULL,							// #626
VM_sprintf,                     // #627 string sprintf(string format, ...)
VM_getsurfacenumtriangles,		// #628 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACETRIANGLE)
VM_getsurfacetriangle,			// #629 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACETRIANGLE)
VM_setkeybind,						// #630 float(float key, string bind[, float bindmap]) setkeybind
VM_getbindmaps,						// #631 vector(void) getbindmap
VM_setbindmaps,						// #632 float(vector bm) setbindmap
NULL,							// #633
NULL,							// #634
NULL,							// #635
NULL,							// #636
NULL,							// #637
VM_CL_RotateMoves,					// #638
NULL,							// #639
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t);

void VM_Polygons_Reset(void)
{
	vmpolygons_t* polys = vmpolygons + PRVM_GetProgNr();

	// TODO: replace vm_polygons stuff with a more general debugging polygon system, and make vm_polygons functions use that system
	if(polys->initialized)
	{
		Mem_FreePool(&polys->pool);
		polys->initialized = false;
	}
}

void VM_CL_Cmd_Init(void)
{
	VM_Cmd_Init();
	VM_Polygons_Reset();
}

void VM_CL_Cmd_Reset(void)
{
	World_End(&cl.world);
	VM_Cmd_Reset();
	VM_Polygons_Reset();
}


