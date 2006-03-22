#include "prvm_cmds.h"
#include "csprogs.h"
#include "cl_collision.h"

//============================================================================
// Client
//[515]: unsolved PROBLEMS
//- finish player physics code (cs_runplayerphysics)
//- fix R_AddDynamicLight
//- EntWasFreed ?
//- RF_DEPTHHACK is not like it should be
//- add builtin that sets cl.viewangles instead of reading "input_angles" global
//- finish lines support for R_Polygon***
//- insert selecttraceline into traceline somehow

//4 feature darkplaces csqc: add builtin to clientside qc for reading triangles of model meshes (useful to orient a ui along a triangle of a model mesh)
//4 feature darkplaces csqc: add builtins to clientside qc for gl calls

#ifndef PF_WARNING
#define PF_WARNING(s) do{Con_Printf(s);PRVM_PrintState();return;}while(0)
#endif

//[515]: really need new list ?
char *vm_cl_extensions =
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
"DP_ENT_GLOW "
"DP_ENT_SCALE "
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
//"DP_QC_STRINGBUFFERS "	//[515]: not needed ?
"DP_QC_TRACEBOX "
//"DP_QC_TRACETOSS "
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
"DP_SV_EFFECT "
"DP_SV_ROTATINGBMODEL "
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
"EXT_BITSHIFT "
"EXT_CSQC "
"FRIK_FILE "
"KRIMZON_SV_PARSECLIENTCOMMAND "
"NEH_CMD_PLAY2 "
"NXQ_GFX_LETTERBOX "
"PRYDON_CLIENTCURSOR "
"TENEBRAE_GFX_DLIGHTS "
"TW_SV_STEPCONTROL "
"NEXUIZ_PLAYERMODEL "
"NEXUIZ_PLAYERSKIN "
;

sfx_t *S_FindName(const char *name);
int CL_PointQ1Contents(const vec3_t p);
void PF_registercvar (void);
int Sbar_GetPlayer (int index);
void Sbar_SortFrags (void);
void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius);
void CL_ExpandCSQCEntities(int num);
void CSQC_RelinkAllEntities (int drawmask);
void CSQC_RelinkCSQCEntities (void);
char *Key_GetBind (int key);





// #1 void(vector ang) makevectors
void VM_CL_makevectors (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_makevectors);
	AngleVectors (PRVM_G_VECTOR(OFS_PARM0), prog->globals.client->v_forward, prog->globals.client->v_right, prog->globals.client->v_up);
}

// #2 void(entity e, vector o) setorigin
void VM_CL_setorigin (void)
{
	prvm_edict_t	*e;
	float	*org;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
		PF_WARNING("setorigin: can not modify world entity\n");
	if (e->priv.required->free)
		PF_WARNING("setorigin: can not modify free entity\n");
	org = PRVM_G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->fields.client->origin);
}

// #3 void(entity e, string m) setmodel
void VM_CL_setmodel (void)
{
	prvm_edict_t	*e;
	const char		*m;
	struct model_s	*mod;
	int				i;

	VM_SAFEPARMCOUNT(2, VM_CL_setmodel);

	e = PRVM_G_EDICT(OFS_PARM0);
	m = PRVM_G_STRING(OFS_PARM1);
	for(i=0;i<MAX_MODELS;i++)
		if(!cl.csqc_model_precache[i])
			break;
		else
		if(!strcmp(cl.csqc_model_precache[i]->name, m))
		{
			e->fields.client->model = PRVM_SetEngineString(cl.csqc_model_precache[i]->name);
			e->fields.client->modelindex = -(i+1);
			return;
		}

	for (i=0, mod = cl.model_precache[0] ; i < MAX_MODELS ; i++, mod = cl.model_precache[i])
		if(mod)
		if(!strcmp(mod->name, m))
		{
			e->fields.client->model = PRVM_SetEngineString(mod->name);
			e->fields.client->modelindex = i;
			return;
		}
	e->fields.client->modelindex = 0;
	e->fields.client->model = 0;
}

// #4 void(entity e, vector min, vector max) setsize
void VM_CL_setsize (void)
{
	prvm_edict_t	*e;
	float			*min, *max;
	VM_SAFEPARMCOUNT(3, VM_CL_setsize);

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
		PF_WARNING("setsize: can not modify world entity\n");
	if (e->priv.server->free)
		PF_WARNING("setsize: can not modify free entity\n");
	min = PRVM_G_VECTOR(OFS_PARM1);
	max = PRVM_G_VECTOR(OFS_PARM2);

	VectorCopy (min, e->fields.client->mins);
	VectorCopy (max, e->fields.client->maxs);
	VectorSubtract (max, min, e->fields.client->size);
}

// #8 void(entity e, float chan, string samp) sound
void VM_CL_sound (void)
{
	const char			*sample;
	int					channel;
	prvm_edict_t		*entity;
	int 				volume;
	float				attenuation;

	VM_SAFEPARMCOUNT(5, VM_CL_sound);

	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = PRVM_G_FLOAT(OFS_PARM3)*255;
	attenuation = PRVM_G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 255)
		PF_WARNING("VM_CL_sound: volume must be in range 0-1\n");

	if (attenuation < 0 || attenuation > 4)
		PF_WARNING("VM_CL_sound: attenuation must be in range 0-4\n");

	if (channel < 0 || channel > 7)
		PF_WARNING("VM_CL_sound: channel must be in range 0-7\n");

	S_StartSound(32768 + PRVM_NUM_FOR_EDICT(entity), channel, S_FindName(sample), entity->fields.client->origin, volume, attenuation);
}

// #14 entity() spawn
void VM_CL_spawn (void)
{
	prvm_edict_t *ed;
	ed = PRVM_ED_Alloc();
	ed->fields.client->entnum = PRVM_NUM_FOR_EDICT(ed);	//[515]: not needed any more ?
	VM_RETURN_EDICT(ed);
}

// #16 float(vector v1, vector v2, float tryents) traceline
void VM_CL_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		ent;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);

	trace = CL_TraceBox(v1, vec3_origin, vec3_origin, v2, 1, &ent, 1, false);

	prog->globals.client->trace_allsolid = trace.allsolid;
	prog->globals.client->trace_startsolid = trace.startsolid;
	prog->globals.client->trace_fraction = trace.fraction;
	prog->globals.client->trace_inwater = trace.inwater;
	prog->globals.client->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, prog->globals.client->trace_endpos);
	VectorCopy (trace.plane.normal, prog->globals.client->trace_plane_normal);
	prog->globals.client->trace_plane_dist =  trace.plane.dist;
	if (ent)
		prog->globals.client->trace_ent = ent;
	else
		prog->globals.client->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
}

// #19 void(string s) precache_sound
void VM_CL_precache_sound (void)
{
	const char *n;
	VM_SAFEPARMCOUNT(1, VM_CL_precache_sound);
	n = PRVM_G_STRING(OFS_PARM0);
	S_PrecacheSound(n, true, false);
}

// #20 void(string s) precache_model
void VM_CL_precache_model (void)
{
	const char	*name;
	int			i;
	model_t		*m;

	VM_SAFEPARMCOUNT(1, VM_CL_precache_model);

	name = PRVM_G_STRING(OFS_PARM0);
	for(i=1;i<MAX_MODELS;i++)
		if(!cl.csqc_model_precache[i])
		{
			i = 0;
			break;
		}
		else
		if(!strcmp(cl.csqc_model_precache[i]->name, name))
		{
			i = -(i+1);
			break;
		}
	if(i)
	{
		PRVM_G_FLOAT(OFS_RETURN) = i;
		return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	m = Mod_ForName(name, false, false, false);
	if(m && m->loaded)
	{
		for(i=1;i<MAX_MODELS;i++)
			if(!cl.csqc_model_precache[i])
				break;
		if(i == MAX_MODELS)
			PF_WARNING("VM_CL_precache_model: no free models\n");
		cl.csqc_model_precache[i] = (model_t*)m;
		PRVM_G_FLOAT(OFS_RETURN) = -(i+1);
		return;
	}
	Con_Printf("VM_CL_precache_model: model \"%s\" not found\n", name);
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
//		VectorAdd(ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->absmin);
//		VectorAdd(ent->fields.client->origin, ent->fields.client->maxs, ent->fields.client->absmax);
		if(BoxesOverlap(mins, maxs, ent->fields.client->absmin, ent->fields.client->absmax))
			list[k++] = ent;
	}
	return k;
}

// #22 entity(vector org, float rad) findradius
void VM_CL_findradius (void)
{
	prvm_edict_t	*ent, *chain;
	vec_t			radius, radius2;
	vec3_t			org, eorg, mins, maxs;
	int				i, numtouchedicts;
	prvm_edict_t	*touchedicts[MAX_EDICTS];

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
		if (ent->fields.client->solid == SOLID_NOT && !sv_gameplayfix_blowupfallenzombies.integer)
			continue;
		// LordHavoc: compare against bounding box rather than center so it
		// doesn't miss large objects, and use DotProduct instead of Length
		// for a major speedup
		VectorSubtract(org, ent->fields.client->origin, eorg);
		if (sv_gameplayfix_findradiusdistancetobox.integer)
		{
			eorg[0] -= bound(ent->fields.client->mins[0], eorg[0], ent->fields.client->maxs[0]);
			eorg[1] -= bound(ent->fields.client->mins[1], eorg[1], ent->fields.client->maxs[1]);
			eorg[2] -= bound(ent->fields.client->mins[2], eorg[2], ent->fields.client->maxs[2]);
		}
		else
			VectorMAMAM(1, eorg, 0.5f, ent->fields.client->mins, 0.5f, ent->fields.client->maxs, eorg);
		if (DotProduct(eorg, eorg) < radius2)
		{
			ent->fields.client->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	VM_RETURN_EDICT(chain);
}

// #34 float() droptofloor
void VM_CL_droptofloor (void)
{
	prvm_edict_t		*ent;
	vec3_t				end;
	trace_t				trace;
	int					i;

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(prog->globals.client->self);
	if (ent == prog->edicts)
		PF_WARNING("droptofloor: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("droptofloor: can not modify free entity\n");

	VectorCopy (ent->fields.client->origin, end);
	end[2] -= 256;

	trace = CL_TraceBox(ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->maxs, end, 1, &i, 1, false);

	if (trace.fraction != 1)
	{
		VectorCopy (trace.endpos, ent->fields.client->origin);
		ent->fields.client->flags = (int)ent->fields.client->flags | FL_ONGROUND;
//		ent->fields.client->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
		PRVM_G_FLOAT(OFS_RETURN) = 1;
		// if support is destroyed, keep suspended (gross hack for floating items in various maps)
//		ent->priv.server->suspendedinairflag = true;
	}
}

// #35 void(float style, string value) lightstyle
void VM_CL_lightstyle (void)
{
	int			i;
	const char	*c;

	VM_SAFEPARMCOUNT(2, VM_CL_lightstyle);

	i = PRVM_G_FLOAT(OFS_PARM0);
	c = PRVM_G_STRING(OFS_PARM1);
	if (i >= cl.max_lightstyle)
		PF_WARNING("VM_CL_lightstyle >= MAX_LIGHTSTYLES\n");
	strlcpy (cl.lightstyle[i].map,  MSG_ReadString(), sizeof (cl.lightstyle[i].map));
	cl.lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
	cl.lightstyle[i].length = (int)strlen(cl.lightstyle[i].map);
}

// #40 float(entity e) checkbottom
void VM_CL_checkbottom (void)
{
	static int		cs_yes, cs_no;
	prvm_edict_t	*ent;
	vec3_t			mins, maxs, start, stop;
	trace_t			trace;
	int				x, y, hit;
	float			mid, bottom;

	VM_SAFEPARMCOUNT(1, VM_CL_checkbottom);
	ent = PRVM_G_EDICT(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	VectorAdd (ent->fields.client->origin, ent->fields.client->mins, mins);
	VectorAdd (ent->fields.client->origin, ent->fields.client->maxs, maxs);

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
	trace = CL_TraceBox (start, vec3_origin, vec3_origin, stop, 1, &hit, 1, true);

	if (trace.fraction == 1.0)
		return;

	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = CL_TraceBox (start, vec3_origin, vec3_origin, stop, 1, &hit, 1, true);

			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > sv_stepheight.value)
				return;
		}

	cs_yes++;
	PRVM_G_FLOAT(OFS_RETURN) = true;
}

// #41 float(vector v) pointcontents
void VM_CL_pointcontents (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_pointcontents);
	PRVM_G_FLOAT(OFS_RETURN) = CL_PointQ1Contents(PRVM_G_VECTOR(OFS_PARM0));
}

// #48 void(vector o, vector d, float color, float count) particle
void VM_CL_particle (void)
{
	float	*org, *dir;
	int		count;
	unsigned char	color;
	VM_SAFEPARMCOUNT(4, VM_CL_particle);

	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	color = PRVM_G_FLOAT(OFS_PARM2);
	count = PRVM_G_FLOAT(OFS_PARM3);
	if (cl_particles_blood_bloodhack.integer)
	{
		if (color == 73)
		{
			// regular blood
			CL_BloodPuff(org, dir, count / 2);
			return;
		}
		if (color == 225)
		{
			// lightning blood
			CL_BloodPuff(org, dir, count / 2);
			return;
		}
	}
	CL_RunParticleEffect (org, dir, color, count);
}

// #49 void(entity ent, float ideal_yaw, float speed_yaw) ChangeYaw
void VM_CL_changeyaw (void)
{
	prvm_edict_t	*ent;
	float			ideal, current, move, speed;
	VM_SAFEPARMCOUNT(3, VM_CL_changeyaw);

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("changeyaw: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("changeyaw: can not modify free entity\n");
	current = ANGLEMOD(ent->fields.client->angles[1]);
	ideal = PRVM_G_FLOAT(OFS_PARM1);
	speed = PRVM_G_FLOAT(OFS_PARM2);

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

	ent->fields.client->angles[1] = ANGLEMOD (current + move);
}

// #63 void(entity ent, float ideal_pitch, float speed_pitch) changepitch (DP_QC_CHANGEPITCH)
void VM_CL_changepitch (void)
{
	prvm_edict_t		*ent;
	float				ideal, current, move, speed;
	VM_SAFEPARMCOUNT(3, VM_CL_changepitch);

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("changepitch: can not modify world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("changepitch: can not modify free entity\n");
	current = ANGLEMOD( ent->fields.client->angles[0] );
	ideal = PRVM_G_FLOAT(OFS_PARM1);
	speed = PRVM_G_FLOAT(OFS_PARM2);

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

	ent->fields.client->angles[0] = ANGLEMOD (current + move);
}

// #64 void(entity e, entity ignore) tracetoss (DP_QC_TRACETOSS)
void VM_CL_tracetoss (void)
{
/*	trace_t	trace;
	prvm_edict_t	*ent;
	prvm_edict_t	*ignore;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
		PF_WARNING("tracetoss: can not use world entity\n");
	ignore = PRVM_G_EDICT(OFS_PARM1);

//FIXME
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
*/
}

// #74 void(vector pos, string samp, float vol, float atten) ambientsound
void VM_CL_ambientsound (void)
{
	float	*f;
	sfx_t	*s;
	VM_SAFEPARMCOUNT(4, VM_CL_ambientsound);
	s = S_FindName(PRVM_G_STRING(OFS_PARM0));
	f = PRVM_G_VECTOR(OFS_PARM1);
	S_StaticSound (s, f, PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3)*64);
}

// #90 void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox (DP_QC_TRACEBOX)
void VM_CL_tracebox (void)
{
	float	*v1, *v2, *m1, *m2;
	trace_t	trace;
	int		ent;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	m1 = PRVM_G_VECTOR(OFS_PARM1);
	m2 = PRVM_G_VECTOR(OFS_PARM2);
	v2 = PRVM_G_VECTOR(OFS_PARM3);

	trace = CL_TraceBox(v1, m1, m2, v2, 1, &ent, 1, false);

	prog->globals.client->trace_allsolid = trace.allsolid;
	prog->globals.client->trace_startsolid = trace.startsolid;
	prog->globals.client->trace_fraction = trace.fraction;
	prog->globals.client->trace_inwater = trace.inwater;
	prog->globals.client->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, prog->globals.client->trace_endpos);
	VectorCopy (trace.plane.normal, prog->globals.client->trace_plane_normal);
	prog->globals.client->trace_plane_dist =  trace.plane.dist;
	if (ent)
		prog->globals.client->trace_ent = ent;
	else
		prog->globals.client->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
}

// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
void VM_CL_getlight (void)
{
	vec3_t ambientcolor, diffusecolor, diffusenormal;
	vec_t *p;

	VM_SAFEPARMCOUNT(1, VM_CL_getlight);

	p = PRVM_G_VECTOR(OFS_PARM0);
	VectorClear(ambientcolor);
	VectorClear(diffusecolor);
	VectorClear(diffusenormal);
	if (cl.worldmodel && cl.worldmodel->brush.LightPoint)
		cl.worldmodel->brush.LightPoint(cl.worldmodel, p, ambientcolor, diffusecolor, diffusenormal);
	VectorMA(ambientcolor, 0.5, diffusecolor, PRVM_G_VECTOR(OFS_RETURN));
}


//============================================================================
//[515]: SCENE MANAGER builtins
void V_CalcRefdef (void);//view.c
void CSQC_R_ClearScreen (void);//gl_rmain.c
void CSQC_R_RenderScene (void);//gl_rmain.c
void CSQC_AddEntity (int n);//csprogs.c
void CSQC_ClearCSQCEntities (void);

matrix4x4_t csqc_listenermatrix;
qboolean csqc_usecsqclistener = false, csqc_frame = false;//[515]: per-frame
qboolean csqc_onground;

static char *particleeffect_names[] =
{
	"TE_SPIKE",
	"TE_SUPERSPIKE",
	"TE_GUNSHOT",
	"TE_EXPLOSION",
	"TE_TAREXPLOSION",
	"TE_LIGHTNING1",//trail
	"TE_LIGHTNING2",//trail
	"TE_WIZSPIKE",
	"TE_KNIGHTSPIKE",
	"TE_LIGHTNING3",//trail
	"TE_LAVASPLASH",
	"TE_TELEPORT",
	"TE_EXPLOSION2",
	"TE_BEAM",//trail
	"TE_EXPLOSION3",
	"",//TE_LIGHTNING4NEH
	"TE_BLOOD",
	"TE_SPARK",
	"",//TE_BLOODSHOWER
	"TE_EXPLOSIONRGB",
	"",//unused
	"",
	"",
	"TE_GUNSHOTQUAD",
	"TE_SPIKEQUAD",
	"TE_SUPERSPIKEQUAD",
	"TE_EXPLOSIONQUAD",
	"",
	"",
	"",
	"TE_FLAMEJET",
	"TE_PLASMABURN",
	"TE_TEI_G3",
	"TE_TEI_SMOKE",
	"TE_TEI_BIGEXPLOSION",
	"TE_TEI_PLASMAHIT",


	//trail effects (as modelflags)
	"EF_ROCKET",
	"EF_GRENADE",
	"EF_GIB",
	"EF_TRACER",
	"EF_ZOMGIB",
	"EF_TRACER2",
	"EF_TRACER3",
	"EF_NEH_CIGAR",
	"EF_NEXUIZ_PLASMA",
	"EF_GLOWTRAIL",
};

#define CSQC_TRAILSTART 36
static const int particleeffects_num = sizeof(particleeffect_names)/sizeof(char*);

static void CSQC_R_RecalcView (void)
{
	extern matrix4x4_t viewmodelmatrix;
	viewmodelmatrix = identitymatrix;
	r_refdef.viewentitymatrix = identitymatrix;
	Matrix4x4_CreateFromQuakeEntity(&r_refdef.viewentitymatrix, csqc_origin[0], csqc_origin[1], csqc_origin[2], csqc_angles[0], csqc_angles[1], csqc_angles[2], 1);
	Matrix4x4_CreateFromQuakeEntity(&viewmodelmatrix, csqc_origin[0], csqc_origin[1], csqc_origin[2], csqc_angles[0], csqc_angles[1], csqc_angles[2], 0.3);
}

//#300 void() clearscene (EXT_CSQC)
void VM_R_ClearScene (void)
{
	VM_SAFEPARMCOUNT(0, VM_R_ClearScene);
//	CSQC_R_RecalcView();
	if(csqc_frame)
		CSQC_ClearCSQCEntities();
	CSQC_R_ClearScreen();
}

//#301 void(float mask) addentities (EXT_CSQC)
void VM_R_AddEntities (void)
{
	VM_SAFEPARMCOUNT(1, VM_R_AddEntities);
	csqc_drawmask = PRVM_G_FLOAT(OFS_PARM0);
}

//#302 void(entity ent) addentity (EXT_CSQC)
void VM_R_AddEntity (void)
{
	VM_SAFEPARMCOUNT(1, VM_R_AddEntity);
	CSQC_AddEntity(PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(OFS_PARM0)));
}

//#303 float(float property, ...) setproperty (EXT_CSQC)
void VM_R_SetView (void)
{
	int		c;
	float	*f;
	float	k;

	if(prog->argc < 2)
		VM_SAFEPARMCOUNT(2, VM_R_SetView);

	c = PRVM_G_FLOAT(OFS_PARM0);
	f = PRVM_G_VECTOR(OFS_PARM1);
	k = PRVM_G_FLOAT(OFS_PARM1);

	switch(c)
	{
	case VF_MIN:			r_refdef.x = f[0];
							r_refdef.y = f[1];
							break;
	case VF_MIN_X:			r_refdef.x = k;
							break;
	case VF_MIN_Y:			r_refdef.y = k;
							break;
	case VF_SIZE:			r_refdef.width = f[0];
							r_refdef.height = f[1];
							break;
	case VF_SIZE_Y:			r_refdef.width = k;
							break;
	case VF_SIZE_X:			r_refdef.height = k;
							break;
	case VF_VIEWPORT:		r_refdef.x = f[0];
							r_refdef.y = f[1];
							f = PRVM_G_VECTOR(OFS_PARM2);
							r_refdef.width = f[0];
							r_refdef.height = f[1];
							break;
	case VF_FOV:			//r_refdef.fov_x = f[0]; // FIXME!
							//r_refdef.fov_y = f[1]; // FIXME!
							break;
	case VF_FOVX:			//r_refdef.fov_x = k; // FIXME!
							break;
	case VF_FOVY:			//r_refdef.fov_y = k; // FIXME!
							break;
	case VF_ORIGIN:			VectorCopy(f, csqc_origin);
							CSQC_R_RecalcView();
							break;
	case VF_ORIGIN_X:		csqc_origin[0] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ORIGIN_Y:		csqc_origin[1] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ORIGIN_Z:		csqc_origin[2] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES:			VectorCopy(f, csqc_angles);
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES_X:		csqc_angles[0] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES_Y:		csqc_angles[1] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES_Z:		csqc_angles[2] = k;
							CSQC_R_RecalcView();
							break;
	case VF_DRAWWORLD:		cl.csqc_vidvars.drawworld = k;
							break;
	case VF_DRAWENGINESBAR:	cl.csqc_vidvars.drawenginesbar = k;
							break;
	case VF_DRAWCROSSHAIR:	cl.csqc_vidvars.drawcrosshair = k;
							break;

	case VF_CL_VIEWANGLES:	VectorCopy(f, cl.viewangles);
							break;
	case VF_CL_VIEWANGLES_X:cl.viewangles[0] = k;
							break;
	case VF_CL_VIEWANGLES_Y:cl.viewangles[1] = k;
							break;
	case VF_CL_VIEWANGLES_Z:cl.viewangles[2] = k;
							break;

	default:				Con_Printf("VM_R_SetView : unknown parm %i\n", c);
							PRVM_G_FLOAT(OFS_RETURN) = 0;
							return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

//#304 void() renderscene (EXT_CSQC)
void VM_R_RenderScene (void) //#134
{
	VM_SAFEPARMCOUNT(0, VM_R_RenderScene);

	if(csqc_frame)
	{
		CSQC_RelinkCSQCEntities();
		CSQC_RelinkAllEntities(csqc_drawmask);
	}

	CSQC_R_RenderScene();
}

//#305 void(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)
void VM_R_AddDynamicLight (void)
{
	float		*pos, *col;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(3, VM_R_AddDynamicLight);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	col = PRVM_G_VECTOR(OFS_PARM2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
	CL_AllocDlight(NULL, &tempmatrix, PRVM_G_FLOAT(OFS_PARM1), col[0], col[1], col[2], 500, 0, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	//CL_AllocDlight(NULL, &tempmatrix, PRVM_G_FLOAT(OFS_PARM1), col[0], col[1], col[2], 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

//============================================================================

//#310 vector (vector v) cs_unproject (EXT_CSQC)
void VM_CL_unproject (void)
{
	float	*f;
	vec3_t	temp;

	VM_SAFEPARMCOUNT(1, VM_CL_unproject);
	f = PRVM_G_VECTOR(OFS_PARM0);
	VectorSet(temp, f[2], f[0] * f[2] * -r_refdef.frustum_x * 2.0 / r_refdef.width, f[1] * f[2] * -r_refdef.frustum_y * 2.0 / r_refdef.height);
	Matrix4x4_Transform(&r_refdef.viewentitymatrix, temp, PRVM_G_VECTOR(OFS_RETURN));
}

//#311 vector (vector v) cs_project (EXT_CSQC)
void VM_CL_project (void)
{
	float	*f;
	vec3_t	v;
	matrix4x4_t m;

	VM_SAFEPARMCOUNT(1, VM_CL_project);
	f = PRVM_G_VECTOR(OFS_PARM0);
	Matrix4x4_Invert_Simple(&m, &r_refdef.viewentitymatrix);
	Matrix4x4_Transform(&m, f, v);
	VectorSet(PRVM_G_VECTOR(OFS_RETURN), v[1]/v[0]/-r_refdef.frustum_x*0.5*r_refdef.width, v[2]/v[0]/-r_refdef.frustum_y*r_refdef.height*0.5, v[0]);
}

//#330 float(float stnum) getstatf (EXT_CSQC)
void VM_CL_getstatf (void)
{
	int i;
	union
	{
		float f;
		int l;
	}dat;
	VM_SAFEPARMCOUNT(1, VM_CL_getstatf);
	i = PRVM_G_FLOAT(OFS_PARM0);
	if(i < 0 || i >= MAX_CL_STATS)
	{
		Con_Printf("VM_CL_getstatf: index>=MAX_CL_STATS or index<0\n");
		return;
	}
	dat.l = cl.stats[i];
	PRVM_G_FLOAT(OFS_RETURN) =  dat.f;
}

//#331 float(float stnum) getstati (EXT_CSQC)
void VM_CL_getstati (void)
{
	int i, index;
	VM_SAFEPARMCOUNT(1, VM_CL_getstati);
	index = PRVM_G_FLOAT(OFS_PARM0);

	if(index < 0 || index >= MAX_CL_STATS)
	{
		Con_Printf("VM_CL_getstati: index>=MAX_CL_STATS or index<0\n");
		return;
	}
	i = cl.stats[index];
	PRVM_G_FLOAT(OFS_RETURN) = i;
}

//#332 string(float firststnum) getstats (EXT_CSQC)
void VM_CL_getstats (void)
{
	int i;
	char *t;
	VM_SAFEPARMCOUNT(1, VM_CL_getstats);
	i = PRVM_G_FLOAT(OFS_PARM0);
	if(i < 0 || i > MAX_CL_STATS-4)
	{
		Con_Printf("VM_CL_getstats: index>MAX_CL_STATS-4 or index<0\n");
		return;
	}
	t = VM_GetTempString();
	strlcpy(t, (char*)&cl.stats[i], 16);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(t);
}

//#333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
void VM_CL_setmodelindex (void)
{
	int				i;
	prvm_edict_t	*t;
	struct model_s	*m;

	VM_SAFEPARMCOUNT(2, VM_CL_setmodelindex);

	t = PRVM_G_EDICT(OFS_PARM0);
	i = (int)PRVM_G_FLOAT(OFS_PARM1);

	t->fields.client->model = 0;
	t->fields.client->modelindex = 0;

	if(!i)
		return;
	if(i<0)
	{
		i = -(i+1);
		if(i >= MAX_MODELS)
			PF_WARNING("VM_CL_setmodelindex >= MAX_MODELS\n");
		m = cl.csqc_model_precache[i];
	}
	else
		if(i >= MAX_MODELS)
			PF_WARNING("VM_CL_setmodelindex >= MAX_MODELS\n");
		else
			m = cl.model_precache[i];
	if(!m)
		PF_WARNING("VM_CL_setmodelindex: null model\n");
	t->fields.client->model = PRVM_SetEngineString(m->name);
	t->fields.client->modelindex = i;
}

//#334 string(float mdlindex) modelnameforindex (EXT_CSQC)
void VM_CL_modelnameforindex (void)
{
	int i;

	VM_SAFEPARMCOUNT(1, VM_CL_modelnameforindex);

	PRVM_G_INT(OFS_RETURN) = 0;
	i = PRVM_G_FLOAT(OFS_PARM0);
	if(i<0)
	{
		i = -(i+1);
		if(i >= MAX_MODELS)
			PF_WARNING("VM_CL_modelnameforindex >= MAX_MODELS\n");
		if(cl.csqc_model_precache[i])
			PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(cl.csqc_model_precache[i]->name);
		return;
	}
	if(i >= MAX_MODELS)
		PF_WARNING("VM_CL_modelnameforindex >= MAX_MODELS\n");
	if(cl.model_precache[i])
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(cl.model_precache[i]->name);
}

//#335 float(string effectname) particleeffectnum (EXT_CSQC)
void VM_CL_particleeffectnum (void)
{
	const char	*n;
	int			i;
	VM_SAFEPARMCOUNT(1, VM_CL_particleeffectnum);
	n = PRVM_G_STRING(OFS_PARM0);
	for(i=0;i<particleeffects_num;i++)
		if(!strcasecmp(particleeffect_names[i], n))
		{
			PRVM_G_FLOAT(OFS_RETURN) = i;
			return;
		}
	PRVM_G_FLOAT(OFS_RETURN) = -1;
}

void CSQC_ParseBeam (int ent, vec3_t start, vec3_t end, model_t *m, int lightning)
{
	int		i;
	beam_t	*b;

	// override any beam with the same entity
	for (i = 0, b = cl.beams;i < cl.max_beams;i++, b++)
	{
		if (b->entity == ent && ent)
		{
			//b->entity = ent;
			b->lightning = lightning;
			b->relativestartvalid = (ent && cl.csqcentities[ent].state_current.active) ? 2 : 0;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}

	// find a free beam
	for (i = 0, b = cl.beams;i < cl.max_beams;i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->lightning = lightning;
			b->relativestartvalid = (ent && cl.csqcentities[ent].state_current.active) ? 2 : 0;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Print("beam list overflow!\n");
}

// #336 void(entity ent, float effectnum, vector start, vector end[, float color]) trailparticles (EXT_CSQC)
void VM_CL_trailparticles (void)
{
	int				i, entnum, col;
	float			*start, *end;
	entity_t		*ent;
	VM_SAFEPARMCOUNT(4, VM_CL_trailparticles);

	entnum	= PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(OFS_PARM0));
	i		= PRVM_G_FLOAT(OFS_PARM1);
	start	= PRVM_G_VECTOR(OFS_PARM2);
	end		= PRVM_G_VECTOR(OFS_PARM3);

	if(i >= particleeffects_num)
		return;
	if (entnum >= MAX_EDICTS)
	{
		Con_Printf("CSQC_ParseBeam: invalid entity number %i\n", entnum);
		return;
	}
	if (entnum >= cl.max_csqcentities)
		CL_ExpandCSQCEntities(entnum);

	ent = &cl.csqcentities[entnum];

	if(prog->argc > 4)
		col = PRVM_G_FLOAT(OFS_PARM4);
	else
		col = ent->state_current.glowcolor;

	switch(i)
	{
	case TE_LIGHTNING1:
		CSQC_ParseBeam(entnum, start, end, cl.model_bolt, true);
		break;
	case TE_LIGHTNING2:
		CSQC_ParseBeam(entnum, start, end, cl.model_bolt2, true);
		break;
	case TE_LIGHTNING3:
		CSQC_ParseBeam(entnum, start, end, cl.model_bolt3, false);
		break;
	case TE_BEAM:
		CSQC_ParseBeam(entnum, start, end, cl.model_beam, false);
		break;
	default:
		CL_RocketTrail(start, end, i-CSQC_TRAILSTART, col, ent);
		break;
	}
}

//#337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
void VM_CL_pointparticles (void)
{
	int			i, n;
	float		*f, *v;
	if(prog->argc < 2)
		VM_SAFEPARMCOUNT(2, VM_CL_pointparticles);
	i = PRVM_G_FLOAT(OFS_PARM0);
	f = PRVM_G_VECTOR(OFS_PARM1);
	if(prog->argc >= 4)
	{
		v = PRVM_G_VECTOR(OFS_PARM2);
		n = PRVM_G_FLOAT(OFS_PARM3);
	}
	else
	{
		v = vec3_origin;
		n = 15;
	}

	if(i >= particleeffects_num)
		return;

	switch(i)
	{
	case TE_SPIKE:
	case TE_SPIKEQUAD:
	case TE_GUNSHOT:
	case TE_GUNSHOTQUAD:
		CL_SparkShower(f, v, 15, 1, 0);
		CL_Smoke(f, v, 15, 0);
		if (cl_particles_bulletimpacts.integer)
			CL_BulletMark(f);
		break;
	case TE_SUPERSPIKE:
	case TE_SUPERSPIKEQUAD:
		CL_SparkShower(f, v, 30, 1, 0);
		CL_Smoke(f, v, 30, 0);
		if (cl_particles_bulletimpacts.integer)
			CL_BulletMark(f);
		break;
	case TE_EXPLOSION:
	case TE_EXPLOSIONQUAD:
	case TE_TEI_BIGEXPLOSION:
		CL_ParticleExplosion(f);
		break;
	case TE_TAREXPLOSION:
		CL_BlobExplosion(f);
		break;
	case TE_WIZSPIKE:
		CL_RunParticleEffect(f, v, 20, 30);
		break;
	case TE_KNIGHTSPIKE:
		CL_RunParticleEffect(f, v, 226, 20);
		break;
	case TE_LAVASPLASH:
		CL_LavaSplash(f);
		break;
	case TE_TELEPORT:
		CL_TeleportSplash(f);
		break;
	case TE_EXPLOSION2:
	case TE_EXPLOSION3:
	case TE_EXPLOSIONRGB:
		CL_ParticleExplosion2(f, v[0], v[1]);
		break;
	case TE_BLOOD:
		CL_BloodPuff(f, v, n);
		break;
	case TE_SPARK:
		CL_SparkShower(f, v, n, 1, 0);
		break;
	case TE_FLAMEJET:
		CL_Flames(f, v, n);
		break;
	case TE_PLASMABURN:
		CL_PlasmaBurn(f);
		break;
	case TE_TEI_G3:
		CL_BeamParticle(f, v, 8, 1, 1, 1, 1, 1);
		break;
	case TE_TEI_SMOKE:
		CL_Tei_Smoke(f, v, n);
		break;
	case TE_TEI_PLASMAHIT:
		CL_Tei_PlasmaHit(f, v, n);
		break;
	default:break;
	}
}

//#338 void(string s) cprint (EXT_CSQC)
void VM_CL_centerprint (void)
{
	char s[VM_STRINGTEMP_LENGTH];
	if(prog->argc < 1)
		VM_SAFEPARMCOUNT(1, VM_CL_centerprint);
	VM_VarString(0, s, sizeof(s));
	SCR_CenterPrint(s);
}

//#342 string(float keynum) getkeybind (EXT_CSQC)
void VM_CL_getkeybind (void)
{
	int i;

	VM_SAFEPARMCOUNT(1, VM_CL_getkeybind);
	i = PRVM_G_FLOAT(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(Key_GetBind(i));
}

//#343 void(float usecursor) setcursormode (EXT_CSQC)
void VM_CL_setcursormode (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_setcursormode);
	cl.csqc_wantsmousemove = PRVM_G_FLOAT(OFS_PARM0);
	cl_ignoremousemove = true;
}

//#345 float(float framenum) getinputstate (EXT_CSQC)
void VM_CL_getinputstate (void)
{
	int i, frame;
	VM_SAFEPARMCOUNT(1, VM_CL_getinputstate);
	frame = PRVM_G_FLOAT(OFS_PARM0);
	for (i = 0;i < cl.movement_numqueue;i++)
		if (cl.movement_queue[i].sequence == frame)
		{
			VectorCopy(cl.movement_queue[i].viewangles, prog->globals.client->input_angles);
			//prog->globals.client->input_buttons = cl.movement_queue[i].//FIXME
			VectorCopy(cl.movement_queue[i].move, prog->globals.client->input_movevalues);
			prog->globals.client->input_timelength = cl.movement_queue[i].frametime;
			if(cl.movement_queue[i].crouch)
			{
				VectorCopy(cl.playercrouchmins, prog->globals.client->pmove_mins);
				VectorCopy(cl.playercrouchmaxs, prog->globals.client->pmove_maxs);
			}
			else
			{
				VectorCopy(cl.playerstandmins, prog->globals.client->pmove_mins);
				VectorCopy(cl.playerstandmaxs, prog->globals.client->pmove_maxs);
			}
		}
}

//#346 void(float sens) setsensitivityscaler (EXT_CSQC)
void VM_CL_setsensitivityscale (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_setsensitivityscale);
	cl.sensitivityscale = PRVM_G_FLOAT(OFS_PARM0);
}

//#347 void() runstandardplayerphysics (EXT_CSQC)
void VM_CL_runplayerphysics (void)
{
}

//#348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
void VM_CL_getplayerkey (void)
{
	int			i;
	char		t[128];
	const char	*c;
	char		*temp;

	VM_SAFEPARMCOUNT(2, VM_CL_getplayerkey);

	i = PRVM_G_FLOAT(OFS_PARM0);
	c = PRVM_G_STRING(OFS_PARM1);
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	Sbar_SortFrags();

	i = Sbar_GetPlayer(i);
	if(i < 0)
		return;

	t[0] = 0;

	if(!strcasecmp(c, "name"))
		strcpy(t, cl.scores[i].name);
	else
		if(!strcasecmp(c, "frags"))
			sprintf(t, "%i", cl.scores[i].frags);
//	else
//		if(!strcasecmp(c, "ping"))
//			sprintf(t, "%i", cl.scores[i].ping);
//	else
//		if(!strcasecmp(c, "entertime"))
//			sprintf(t, "%f", cl.scores[i].entertime);
	else
		if(!strcasecmp(c, "colors"))
			sprintf(t, "%i", cl.scores[i].colors);
	else
		if(!strcasecmp(c, "topcolor"))
			sprintf(t, "%i", cl.scores[i].colors & 0xf0);
	else
		if(!strcasecmp(c, "bottomcolor"))
			sprintf(t, "%i", (cl.scores[i].colors &15)<<4);
	else
		if(!strcasecmp(c, "viewentity"))
			sprintf(t, "%i", i+1);
	if(!t[0])
		return;
	temp = VM_GetTempString();
	strcpy(temp, t);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(temp);
}

//#349 float() isdemo (EXT_CSQC)
void VM_CL_isdemo (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = cls.demoplayback;
}

//#351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
void VM_CL_setlistener (void)
{
	VM_SAFEPARMCOUNT(4, VM_CL_setlistener);
	Matrix4x4_FromVectors(&csqc_listenermatrix, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), PRVM_G_VECTOR(OFS_PARM3), PRVM_G_VECTOR(OFS_PARM0));
	csqc_usecsqclistener = true;	//use csqc listener at this frame
}

//#352 void(string cmdname) registercommand (EXT_CSQC)
void VM_CL_registercmd (void)
{
	char *t;
	VM_SAFEPARMCOUNT(1, VM_CL_registercmd);
	if(!Cmd_Exists(PRVM_G_STRING(OFS_PARM0)))
	{
		t = Z_Malloc(strlen(PRVM_G_STRING(OFS_PARM0))+1);
		strcpy(t, PRVM_G_STRING(OFS_PARM0));
		Cmd_AddCommand(t, NULL, "console command created by QuakeC");
	}
	else
		Cmd_AddCommand(PRVM_G_STRING(OFS_PARM0), NULL, "console command created by QuakeC");

}

//#354 float() playernum (EXT_CSQC)
void VM_CL_playernum (void)
{
	int i, k;

	VM_SAFEPARMCOUNT(0, VM_CL_playernum);

	for(i=k=0 ; i<cl.maxclients ; i++)
		if(cl.scores[i].name[0])
			k++;
	PRVM_G_FLOAT(OFS_RETURN) = k;
}

//#355 float() cl_onground (EXT_CSQC)
void VM_CL_onground (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = csqc_onground;
}

//#360 float() readbyte (EXT_CSQC)
void VM_CL_ReadByte (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadByte();
}

//#361 float() readchar (EXT_CSQC)
void VM_CL_ReadChar (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadChar();
}

//#362 float() readshort (EXT_CSQC)
void VM_CL_ReadShort (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadShort();
}

//#363 float() readlong (EXT_CSQC)
void VM_CL_ReadLong (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadLong();
}

//#364 float() readcoord (EXT_CSQC)
void VM_CL_ReadCoord (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadCoord(cls.protocol);
}

//#365 float() readangle (EXT_CSQC)
void VM_CL_ReadAngle (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadAngle(cls.protocol);
}

//#366 string() readstring (EXT_CSQC)
void VM_CL_ReadString (void)
{
	char *t, *s;
	t = VM_GetTempString();
	s = MSG_ReadString();
	PRVM_G_INT(OFS_RETURN) = 0;
	if(s)
	{
		strcpy(t, s);
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(t);
	}
}

//#367 float() readfloat (EXT_CSQC)
void VM_CL_ReadFloat (void)
{
	PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadFloat();
}

//=================================================================//

// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
void VM_CL_effect (void)
{
	VM_SAFEPARMCOUNT(5, VM_CL_effect);
	CL_Effect(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1), PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3), PRVM_G_FLOAT(OFS_PARM4));
}

// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
void VM_CL_te_blood (void)
{
	float	*pos;
	vec3_t	pos2;
	VM_SAFEPARMCOUNT(3, VM_CL_te_blood);
	if (PRVM_G_FLOAT(OFS_PARM2) < 1)
		return;
	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_BloodPuff(pos2, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_FLOAT(OFS_PARM2));
}

// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
void VM_CL_te_bloodshower (void)
{
	VM_SAFEPARMCOUNT(4, VM_CL_te_bloodshower);
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	CL_BloodShower(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3));
}

// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
void VM_CL_te_explosionrgb (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(2, VM_CL_te_explosionrgb);
	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleExplosion(pos2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 350, PRVM_G_VECTOR(OFS_PARM1)[0], PRVM_G_VECTOR(OFS_PARM1)[1], PRVM_G_VECTOR(OFS_PARM1)[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
void VM_CL_te_particlecube (void)
{
	VM_SAFEPARMCOUNT(7, VM_CL_te_particlecube);
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	CL_ParticleCube(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3), PRVM_G_FLOAT(OFS_PARM4), PRVM_G_FLOAT(OFS_PARM5), PRVM_G_FLOAT(OFS_PARM6));
}

// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
void VM_CL_te_particlerain (void)
{
	VM_SAFEPARMCOUNT(5, VM_CL_te_particlerain);
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	CL_ParticleRain(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3), PRVM_G_FLOAT(OFS_PARM4), 0);
}

// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
void VM_CL_te_particlesnow (void)
{
	VM_SAFEPARMCOUNT(5, VM_CL_te_particlesnow);
	if (PRVM_G_FLOAT(OFS_PARM3) < 1)
		return;
	CL_ParticleRain(PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM3), PRVM_G_FLOAT(OFS_PARM4), 1);
}

// #411 void(vector org, vector vel, float howmany) te_spark
void VM_CL_te_spark (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(3, VM_CL_te_spark);

	if (PRVM_G_FLOAT(OFS_PARM2) < 1)
		return;
	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_SparkShower(pos2, PRVM_G_VECTOR(OFS_PARM1), PRVM_G_FLOAT(OFS_PARM2), 1, 0);
}

// #412 void(vector org) te_gunshotquad (DP_QUADEFFECTS1)
void VM_CL_te_gunshotquad (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_gunshotquad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_SparkShower(pos2, vec3_origin, 15, 1, 0);
	CL_Smoke(pos2, vec3_origin, 15, 0);
	CL_BulletMark(pos2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

// #413 void(vector org) te_spikequad (DP_QUADEFFECTS1)
void VM_CL_te_spikequad (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_spikequad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	if (cl_particles_bulletimpacts.integer)
	{
		CL_SparkShower(pos2, vec3_origin, 15, 1, 0);
		CL_Smoke(pos2, vec3_origin, 15, 0);
		CL_BulletMark(pos2);
	}
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
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
void VM_CL_te_superspikequad (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_superspikequad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	if (cl_particles_bulletimpacts.integer)
	{
		CL_SparkShower(pos2, vec3_origin, 30, 1, 0);
		CL_Smoke(pos2, vec3_origin, 30, 0);
		CL_BulletMark(pos2);
	}
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 100, 0.15f, 0.15f, 1.5f, 500, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
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
void VM_CL_te_explosionquad (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_explosionquad);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleExplosion(pos2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 350, 2.5f, 2.0f, 4.0f, 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}

// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
void VM_CL_te_smallflash (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_smallflash);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 200, 2, 2, 2, 1000, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
void VM_CL_te_customflash (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(4, VM_CL_te_customflash);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, PRVM_G_FLOAT(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM3)[0], PRVM_G_VECTOR(OFS_PARM3)[1], PRVM_G_VECTOR(OFS_PARM3)[2], PRVM_G_FLOAT(OFS_PARM1) / PRVM_G_FLOAT(OFS_PARM2), PRVM_G_FLOAT(OFS_PARM2), 0, -1, true, 1, 0.25, 1, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_gunshot (void)
{
	float		*pos;
	vec3_t		pos2;
	VM_SAFEPARMCOUNT(1, VM_CL_te_gunshot);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	CL_SparkShower(pos2, vec3_origin, 15, 1, 0);
	CL_Smoke(pos2, vec3_origin, 15, 0);
	CL_BulletMark(pos2);
}

// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_spike (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_spike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	if (cl_particles_bulletimpacts.integer)
	{
		CL_SparkShower(pos2, vec3_origin, 15, 1, 0);
		CL_Smoke(pos2, vec3_origin, 15, 0);
		CL_BulletMark(pos2);
	}
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
void VM_CL_te_superspike (void)
{
	float		*pos;
	vec3_t		pos2;
	int			rnd;
	VM_SAFEPARMCOUNT(1, VM_CL_te_superspike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	if (cl_particles_bulletimpacts.integer)
	{
		CL_SparkShower(pos2, vec3_origin, 30, 1, 0);
		CL_Smoke(pos2, vec3_origin, 30, 0);
		CL_BulletMark(pos2);
	}
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
void VM_CL_te_explosion (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_explosion);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleExplosion(pos2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 350, 4.0f, 2.0f, 0.50f, 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}

// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_tarexplosion (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_tarexplosion);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_BlobExplosion(pos2);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 600, 1.6f, 0.8f, 2.0f, 1200, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}

// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_wizspike (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_wizspike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 100, 0.12f, 0.50f, 0.12f, 500, 0.2, 0, -1, false, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	CL_RunParticleEffect(pos2, vec3_origin, 20, 30);
	S_StartSound(-1, 0, cl.sfx_wizhit, pos2, 1, 1);
}

// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_knightspike (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_knightspike);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 100, 0.50f, 0.30f, 0.10f, 500, 0.2, 0, -1, false, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	CL_RunParticleEffect(pos2, vec3_origin, 226, 20);
	S_StartSound(-1, 0, cl.sfx_knighthit, pos2, 1, 1);
}

// #425 void(vector org) te_lavasplash (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_lavasplash (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_te_lavasplash);
	CL_LavaSplash(PRVM_G_VECTOR(OFS_PARM0));
}

// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_teleport (void)
{
	float		*pos;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_teleport);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	Matrix4x4_CreateTranslate(&tempmatrix, pos[0], pos[1], pos[2]);
	CL_AllocDlight(NULL, &tempmatrix, 200, 1.0f, 1.0f, 1.0f, 600, 99.0f, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	CL_TeleportSplash(pos);
}

// #427 void(vector org, float colorstart, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_explosion2 (void)
{
	float		*pos;
	vec3_t		pos2, color;
	matrix4x4_t	tempmatrix;
	int			colorStart, colorLength;
	unsigned char		*tempcolor;
	VM_SAFEPARMCOUNT(3, VM_CL_te_explosion2);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	colorStart = PRVM_G_FLOAT(OFS_PARM1);
	colorLength = PRVM_G_FLOAT(OFS_PARM2);
	CL_FindNonSolidLocation(pos, pos2, 10);
	CL_ParticleExplosion2(pos2, colorStart, colorLength);
	tempcolor = (unsigned char *)&palette_complete[(rand()%colorLength) + colorStart];
	color[0] = tempcolor[0] * (2.0f / 255.0f);
	color[1] = tempcolor[1] * (2.0f / 255.0f);
	color[2] = tempcolor[2] * (2.0f / 255.0f);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 350, color[0], color[1], color[2], 700, 0.5, 0, -1, true, 1, 0.25, 0.25, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	S_StartSound(-1, 0, cl.sfx_r_exp3, pos2, 1, 1);
}


static void VM_CL_NewBeam (int ent, float *start, float *end, model_t *m, qboolean lightning)
{
	beam_t	*b;
	int		i;

	if (ent >= cl.max_csqcentities)
		CL_ExpandCSQCEntities(ent);

	// override any beam with the same entity
	for (i = 0, b = cl.beams;i < cl.max_beams;i++, b++)
	{
		if (b->entity == ent && ent)
		{
			//b->entity = ent;
			b->lightning = lightning;
			b->relativestartvalid = (ent && cl.csqcentities[ent].state_current.active) ? 2 : 0;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}

	// find a free beam
	for (i = 0, b = cl.beams;i < cl.max_beams;i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->lightning = lightning;
			b->relativestartvalid = (ent && cl.csqcentities[ent].state_current.active) ? 2 : 0;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Print("beam list overflow!\n");
}

// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_lightning1 (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_lightning1);
	VM_CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_bolt, true);
}

// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_lightning2 (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_lightning2);
	VM_CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_bolt2, true);
}

// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_lightning3 (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_lightning3);
	VM_CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_bolt3, false);
}

// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
void VM_CL_te_beam (void)
{
	VM_SAFEPARMCOUNT(3, VM_CL_te_beam);
	VM_CL_NewBeam(PRVM_G_EDICTNUM(OFS_PARM0), PRVM_G_VECTOR(OFS_PARM1), PRVM_G_VECTOR(OFS_PARM2), cl.model_beam, false);
}

// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
void VM_CL_te_plasmaburn (void)
{
	float		*pos;
	vec3_t		pos2;
	matrix4x4_t	tempmatrix;
	VM_SAFEPARMCOUNT(1, VM_CL_te_plasmaburn);

	pos = PRVM_G_VECTOR(OFS_PARM0);
	CL_FindNonSolidLocation(pos, pos2, 4);
	Matrix4x4_CreateTranslate(&tempmatrix, pos2[0], pos2[1], pos2[2]);
	CL_AllocDlight(NULL, &tempmatrix, 200, 1, 1, 1, 1000, 0.2, 0, -1, true, 1, 0.25, 1, 0, 0, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
	CL_PlasmaBurn(pos2);
}


//====================================================================
//DP_QC_GETSURFACE

void clippointtosurface(msurface_t *surface, vec3_t p, vec3_t out);
static msurface_t *cl_getsurface(prvm_edict_t *ed, int surfacenum)
{
	int modelindex;
	model_t *model = NULL;
	if (!ed || ed->priv.server->free)
		return NULL;
	modelindex = ed->fields.client->modelindex;
	if(!modelindex)
		return NULL;
	if(modelindex<0)
	{
		modelindex = -(modelindex+1);
		if(modelindex < MAX_MODELS)
			model = cl.csqc_model_precache[modelindex];
	}
	else
	{
		if(modelindex < MAX_MODELS)
			model = cl.model_precache[modelindex];
	}
	if(!model)
		return NULL;
	if (surfacenum < 0 || surfacenum >= model->nummodelsurfaces)
		return NULL;
	return model->data_surfaces + surfacenum + model->firstmodelsurface;
}

// #434 float(entity e, float s) getsurfacenumpoints
void VM_CL_getsurfacenumpoints(void)
{
	msurface_t *surface;
	// return 0 if no such surface
	if (!(surface = cl_getsurface(PRVM_G_EDICT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1))))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// note: this (incorrectly) assumes it is a simple polygon
	PRVM_G_FLOAT(OFS_RETURN) = surface->num_vertices;
}

// #435 vector(entity e, float s, float n) getsurfacepoint
void VM_CL_getsurfacepoint(void)
{
	prvm_edict_t *ed;
	msurface_t *surface;
	int pointnum;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed || ed->priv.server->free)
		return;
	if (!(surface = cl_getsurface(ed, PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// note: this (incorrectly) assumes it is a simple polygon
	pointnum = PRVM_G_FLOAT(OFS_PARM2);
	if (pointnum < 0 || pointnum >= surface->num_vertices)
		return;
	// FIXME: implement rotation/scaling
	VectorAdd(&(surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex)[pointnum * 3], ed->fields.client->origin, PRVM_G_VECTOR(OFS_RETURN));
}

// #436 vector(entity e, float s) getsurfacenormal
void VM_CL_getsurfacenormal(void)
{
	msurface_t *surface;
	vec3_t normal;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	if (!(surface = cl_getsurface(PRVM_G_EDICT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	// note: this (incorrectly) assumes it is a simple polygon
	// note: this only returns the first triangle, so it doesn't work very
	// well for curved surfaces or arbitrary meshes
	TriangleNormal((surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex), (surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex) + 3, (surface->groupmesh->data_vertex3f + 3 * surface->num_firstvertex) + 6, normal);
	VectorNormalize(normal);
	VectorCopy(normal, PRVM_G_VECTOR(OFS_RETURN));
}

// #437 string(entity e, float s) getsurfacetexture
void VM_CL_getsurfacetexture(void)
{
	msurface_t *surface;
	PRVM_G_INT(OFS_RETURN) = 0;
	if (!(surface = cl_getsurface(PRVM_G_EDICT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1))))
		return;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(surface->texture->name);
}

// #438 float(entity e, vector p) getsurfacenearpoint
void VM_CL_getsurfacenearpoint(void)
{
	int surfacenum, best, modelindex;
	vec3_t clipped, p;
	vec_t dist, bestdist;
	prvm_edict_t *ed;
	model_t *model = NULL;
	msurface_t *surface;
	vec_t *point;
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	ed = PRVM_G_EDICT(OFS_PARM0);
	point = PRVM_G_VECTOR(OFS_PARM1);

	if (!ed || ed->priv.server->free)
		return;
	modelindex = ed->fields.client->modelindex;
	if(!modelindex)
		return;
	if(modelindex<0)
	{
		modelindex = -(modelindex+1);
		if(modelindex < MAX_MODELS)
			model = cl.csqc_model_precache[modelindex];
	}
	else
		if(modelindex < MAX_MODELS)
			model = cl.model_precache[modelindex];
	if(!model)
		return;
	if (!model->num_surfaces)
		return;

	// FIXME: implement rotation/scaling
	VectorSubtract(point, ed->fields.client->origin, p);
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

// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint
void VM_CL_getsurfaceclippedpoint(void)
{
	prvm_edict_t *ed;
	msurface_t *surface;
	vec3_t p, out;
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed || ed->priv.server->free)
		return;
	if (!(surface = cl_getsurface(ed, PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	VectorSubtract(PRVM_G_VECTOR(OFS_PARM2), ed->fields.client->origin, p);
	clippointtosurface(surface, p, out);
	// FIXME: implement rotation/scaling
	VectorAdd(out, ed->fields.client->origin, PRVM_G_VECTOR(OFS_RETURN));
}

// #443 void(entity e, entity tagentity, string tagname) setattachment
void VM_CL_setattachment (void)
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

	v = PRVM_GETEDICTFIELDVALUE(e, csqc_fieldoff_tag_entity);
	if (v)
		v->edict = PRVM_EDICT_TO_PROG(tagentity);

	v = PRVM_GETEDICTFIELDVALUE(e, csqc_fieldoff_tag_index);
	if (v)
		v->_float = 0;
	if (tagentity != NULL && tagentity != prog->edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->fields.client->modelindex;
		model = NULL;

		if(modelindex)
		{
			if(modelindex<0)
			{
				modelindex = -(modelindex+1);
				if(modelindex < MAX_MODELS)
					model = cl.csqc_model_precache[modelindex];
			}
			else
				if(modelindex < MAX_MODELS)
					model = cl.model_precache[modelindex];
		}

		if (model)
		{
			v->_float = Mod_Alias_GetTagIndexForName(model, tagentity->fields.client->skin, tagname);
			if (v->_float == 0)
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity));
	}
}

/////////////////////////////////////////
// DP_MD3_TAGINFO extension coded by VorteX

int CL_GetTagIndex (prvm_edict_t *e, const char *tagname)
{
	int i;
	model_t *m;

	i = e->fields.client->modelindex;

	if(!i)
		return -1;
	if(i<0)
	{
		i = -(i+1);
		if(i >= MAX_MODELS)
			return -1;
		m = cl.csqc_model_precache[i];
	}
	else
		if(i >= MAX_MODELS)
			return -1;
		else
			m = cl.model_precache[i];

	return Mod_Alias_GetTagIndexForName(m, e->fields.client->skin, tagname);
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
int CL_GetTagMatrix (matrix4x4_t *out, prvm_edict_t *ent, int tagindex)
{
	prvm_eval_t *val;
	int modelindex, reqframe, attachloop, i;
	matrix4x4_t entitymatrix, tagmatrix, attachmatrix;
	prvm_edict_t *attachent;
	model_t *model;

	*out = identitymatrix; // warnings and errors return identical matrix

	if (ent == prog->edicts)
		return 1;
	if (ent->priv.server->free)
		return 2;

	modelindex = (int)ent->fields.client->modelindex;

	if(!modelindex)
		return 3;
	if(modelindex<0)
	{
		modelindex = -(modelindex+1);
		if(modelindex >= MAX_MODELS)
			return 3;
		model = cl.csqc_model_precache[modelindex];
	}
	else
		if(modelindex >= MAX_MODELS)
			return 3;
		else
			model = cl.model_precache[modelindex];

	if (ent->fields.client->frame >= 0 && ent->fields.client->frame < model->numframes && model->animscenes)
		reqframe = model->animscenes[(int)ent->fields.client->frame].firstframe;
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
		tagmatrix = identitymatrix;

	if ((val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_tag_entity)) && val->edict)
	{ // DP_GFX_QUAKE3MODELTAGS, scan all chain and stop on unattached entity
		attachloop = 0;
		do
		{
			attachent = PRVM_EDICT_NUM(val->edict); // to this it entity our entity is attached
			val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_tag_index);

			model = NULL;
			i = attachent->fields.client->modelindex;
			if(i<0)
			{
				i = -(i+1);
				if(i < MAX_MODELS)
					model = cl.csqc_model_precache[i];
			}
			else
				if(i < MAX_MODELS)
					model = cl.model_precache[i];

			if (model && val->_float >= 1 && model->animscenes && attachent->fields.client->frame >= 0 && attachent->fields.client->frame < model->numframes)
				Mod_Alias_GetTagMatrix(model, model->animscenes[(int)attachent->fields.client->frame].firstframe, val->_float - 1, &attachmatrix);
			else
				attachmatrix = identitymatrix;

			// apply transformation by child entity matrix
			val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_scale);
			if (val->_float == 0)
				val->_float = 1;
			Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2], -ent->fields.client->angles[0], ent->fields.client->angles[1], ent->fields.client->angles[2], val->_float);
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
		while ((val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_tag_entity)) && val->edict);
	}

	// normal or RENDER_VIEWMODEL entity (or main parent entity on attach chain)
	val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_scale);
	if (val->_float == 0)
		val->_float = 1;
	// Alias models have inverse pitch, bmodels can't have tags, so don't check for modeltype...
	Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2], -ent->fields.client->angles[0], ent->fields.client->angles[1], ent->fields.client->angles[2], val->_float);
	Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
	out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
	out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
	out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);

	if ((val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_renderflags)) && (RF_VIEWMODEL & (int)val->_float))
	{// RENDER_VIEWMODEL magic
		Matrix4x4_Copy(&tagmatrix, out);

		val = PRVM_GETEDICTFIELDVALUE(ent, csqc_fieldoff_scale);
		if (val->_float == 0)
			val->_float = 1;

		Matrix4x4_CreateFromQuakeEntity(&entitymatrix, csqc_origin[0], csqc_origin[1], csqc_origin[2], csqc_angles[0], csqc_angles[1], csqc_angles[2], val->_float);
		Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
		out->m[0][3] = entitymatrix.m[0][3] + val->_float*(entitymatrix.m[0][0]*tagmatrix.m[0][3] + entitymatrix.m[0][1]*tagmatrix.m[1][3] + entitymatrix.m[0][2]*tagmatrix.m[2][3]);
		out->m[1][3] = entitymatrix.m[1][3] + val->_float*(entitymatrix.m[1][0]*tagmatrix.m[0][3] + entitymatrix.m[1][1]*tagmatrix.m[1][3] + entitymatrix.m[1][2]*tagmatrix.m[2][3]);
		out->m[2][3] = entitymatrix.m[2][3] + val->_float*(entitymatrix.m[2][0]*tagmatrix.m[0][3] + entitymatrix.m[2][1]*tagmatrix.m[1][3] + entitymatrix.m[2][2]*tagmatrix.m[2][3]);

		/*
		// Cl_bob, ported from rendering code
		if (ent->fields.client->health > 0 && cl_bob.value && cl_bobcycle.value)
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
			bob = sqrt(ent->fields.client->velocity[0]*ent->fields.client->velocity[0] + ent->fields.client->velocity[1]*ent->fields.client->velocity[1])*cl_bob.value;
			bob = bob*0.3 + bob*0.7*cycle;
			out->m[2][3] += bound(-7, bob, 4);
		}
		*/
	}
	return 0;
}

// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
void VM_CL_gettagindex (void)
{
	prvm_edict_t *ent = PRVM_G_EDICT(OFS_PARM0);
	const char *tag_name = PRVM_G_STRING(OFS_PARM1);
	int modelindex, tag_index;

	if (ent == prog->edicts)
		PF_WARNING("gettagindex: can't affect world entity\n");
	if (ent->priv.server->free)
		PF_WARNING("gettagindex: can't affect free entity\n");

	modelindex = (int)ent->fields.client->modelindex;
	if(modelindex < 0)
		modelindex = -(modelindex+1);
	tag_index = 0;
	if (modelindex <= 0 || modelindex >= MAX_MODELS)
		Con_DPrintf("gettagindex(entity #%i): null or non-precached model\n", PRVM_NUM_FOR_EDICT(ent));
	else
	{
		tag_index = CL_GetTagIndex(ent, tag_name);
		if (tag_index == 0)
			Con_DPrintf("gettagindex(entity #%i): tag \"%s\" not found\n", PRVM_NUM_FOR_EDICT(ent), tag_name);
	}
	PRVM_G_FLOAT(OFS_RETURN) = tag_index;
}

// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
void VM_CL_gettaginfo (void)
{
	prvm_edict_t *e = PRVM_G_EDICT(OFS_PARM0);
	int tagindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	matrix4x4_t tag_matrix;
	int returncode;

	returncode = CL_GetTagMatrix(&tag_matrix, e, tagindex);
	Matrix4x4_ToVectors(&tag_matrix, prog->globals.client->v_forward, prog->globals.client->v_right, prog->globals.client->v_up, PRVM_G_VECTOR(OFS_RETURN));

	switch(returncode)
	{
		case 1:
			PF_WARNING("gettagindex: can't affect world entity\n");
			break;
		case 2:
			PF_WARNING("gettagindex: can't affect free entity\n");
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

//=================================================
//[515]: here goes test/unfinished/etc.

//[515]: check if it is what it should be
void VM_WasFreed (void)
{
	prvm_edict_t	*e;
	VM_SAFEPARMCOUNT(1, VM_WasFreed);

	e = PRVM_G_EDICT(OFS_PARM0);
	if (!e->priv.required->free || (e->priv.required->free && (e->priv.required->freetime < 2 || (*prog->time - e->priv.required->freetime) > 0.5 )))
		PRVM_G_FLOAT(OFS_RETURN) = false;
	else
		PRVM_G_FLOAT(OFS_RETURN) = true;
}

void VM_CL_select_cube (void)
{
	int		i;
	int		chain_of;
	float	*mins2, *maxs2;
	prvm_edict_t	*ent, *chain;
	vec3_t	mins1, maxs1;

	VM_SAFEPARMCOUNT(2, VM_CL_select_cube);

	// is the same like !(prog->flag & PRVM_FE_CHAIN) - even if the operator precedence is another
	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchain: %s doesnt have a chain field !\n", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;
	chain = prog->edicts;

	mins2 = PRVM_G_VECTOR(OFS_PARM0);
	maxs2 = PRVM_G_VECTOR(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (ent->priv.required->free)
			continue;
		VectorCopy(ent->fields.client->origin, mins1);
		VectorAdd(mins1, ent->fields.client->maxs, maxs1);
		VectorAdd(mins1, ent->fields.client->mins, mins1);
		if (mins1[0] > maxs2[0] || mins1[1] > maxs2[1] || mins1[2] > maxs2[2])
			continue;
		if (maxs1[0] < mins2[0] || maxs1[1] < mins2[1] || maxs1[2] < mins2[2])
			continue;
		PRVM_E_INT(ent,chain_of) = PRVM_NUM_FOR_EDICT(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

void VM_CL_select_super (void)
{
/*	int		i;
	int		chain_of;
	float	*v[8];
	prvm_edict_t	*ent, *chain;
	vec3_t	mins1, maxs1;

	VM_SAFEPARMCOUNT(8, VM_findchain);
	for(i=0;i<8;i++)
		v[i] = PRVM_G_VECTOR(OFS_PARM0+i*3);

	// is the same like !(prog->flag & PRVM_FE_CHAIN) - even if the operator precedence is another
	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchain: %s doesnt have a chain field !\n", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;
	chain = prog->edicts;

	mins2 = PRVM_G_VECTOR(OFS_PARM0);
	maxs2 = PRVM_G_VECTOR(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (ent->priv.required->free)
			continue;
		VectorCopy(ent->fields.client->origin, mins1);
		VectorAdd(mins1, ent->fields.client->maxs, maxs1);
		VectorAdd(mins1, ent->fields.client->mins, mins1);
		if (mins1[0] > maxs2[0] || mins1[1] > maxs2[1] || mins1[2] > maxs2[2])
			continue;
		if (maxs1[0] < mins2[0] || maxs1[1] < mins2[1] || maxs1[2] < mins2[2])
			continue;
		PRVM_E_INT(ent,chain_of) = PRVM_NUM_FOR_EDICT(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);*/
}

static int Is_Text_Color (char c, char t)
{
	int a = 0;
	char c2 = c - (c & 128);
	char t2 = t - (t & 128);

	if(c != '^' && c2 != '^')		return 0;
	if(t >= '0' && t <= '9')		a = 1;
	if(t2 >= '0' && t2 <= '9')		a = 1;
/*	if(t >= 'A' && t <= 'Z')		a = 2;
	if(t2 >= 'A' && t2 <= 'Z')		a = 2;

	if(a == 1 && scr_colortext.integer > 0)
		return 1;
	if(a == 2 && scr_multifonts.integer > 0)
		return 2;
*/
	return a;
}

void VM_uncolorstring (void) //#170
{
	const char	*in;
	char		*out;
	int			k = 0, i = 0;

	VM_SAFEPARMCOUNT(1, VM_uncolorstring);
	in = PRVM_G_STRING(OFS_PARM0);
	if(!in)
		PRVM_ERROR ("VM_uncolorstring: %s: NULL\n", PRVM_NAME);
	VM_CheckEmptyString (in);
	out = VM_GetTempString();

	while (in[k])
	{
		if(in[k+1])
		if(Is_Text_Color(in[k], in[k+1]) == 1/* || (in[k] == '&' && in[k+1] == 'r')*/)
		{
			k += 2;
			continue;
		}
		out[i] = in[k];
		++k;
		++i;
	}
}

void VM_CL_selecttraceline (void)
{
	float	*v1, *v2;
	int		ent, ignore, csqcents;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);
	ignore = PRVM_G_FLOAT(OFS_PARM2);
	csqcents = PRVM_G_FLOAT(OFS_PARM3);
	ent = 0;

	if((csqcents && ignore > cl.num_csqcentities) || (!csqcents && ignore > cl.num_entities))
	{
		Con_Printf("VM_CL_selecttraceline: out of entities\n");
		return;
	}
	else
		if(csqcents)
			prog->globals.client->trace_fraction = CL_SelectTraceLine(v1, v2, prog->globals.client->trace_endpos, prog->globals.client->trace_plane_normal, &prog->globals.client->trace_ent, &cl.csqcentities[ignore].render, csqcents);
		else
			prog->globals.client->trace_fraction = CL_SelectTraceLine(v1, v2, prog->globals.client->trace_endpos, prog->globals.client->trace_plane_normal, &ent, &cl.entities[ignore].render, csqcents);
	PRVM_G_FLOAT(OFS_RETURN) = ent;
}

void VM_charindex (void)
{
	const char *s;
	s = PRVM_G_STRING(OFS_PARM0);
	if(!s)
		return;
	if((unsigned)PRVM_G_FLOAT(OFS_PARM1) > strlen(s))
		return;
	PRVM_G_FLOAT(OFS_RETURN) = (unsigned char)s[(int)PRVM_G_FLOAT(OFS_PARM1)];
}

//#223 string(float c, ...) chr2str (FTE_STRINGS)
void VM_chr2str (void)
{
	char	*t;
	int		i;
	t = VM_GetTempString();
	for(i=0;i<prog->argc;i++)
		t[i] = (unsigned char)PRVM_G_FLOAT(OFS_PARM0+i*3);
	t[i] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(t);
}

//#228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
void VM_strncmp (void)
{
	const char *s1, *s2;
	VM_SAFEPARMCOUNT(1, VM_strncmp);
	s1 = PRVM_G_STRING(OFS_PARM0);
	s2 = PRVM_G_STRING(OFS_PARM1);
	PRVM_G_FLOAT(OFS_RETURN) = strncmp(s1, s2, (size_t)PRVM_G_FLOAT(OFS_PARM2));
}

//============================================================================
//============================================================================

prvm_builtin_t vm_cl_builtins[] = {
0,  // to be consistent with the old vm
VM_CL_makevectors,			// #1 void(vector ang) makevectors
VM_CL_setorigin,			// #2 void(entity e, vector o) setorigin
VM_CL_setmodel,				// #3 void(entity e, string m) setmodel
VM_CL_setsize,				// #4 void(entity e, vector min, vector max) setsize
0,
VM_break,					// #6 void() break
VM_random,					// #7 float() random
VM_CL_sound,				// #8 void(entity e, float chan, string samp) sound
VM_normalize,				// #9 vector(vector v) normalize
VM_error,					// #10 void(string e) error
VM_objerror,				// #11 void(string e) objerror
VM_vlen,					// #12 float(vector v) vlen
VM_vectoyaw,				// #13 float(vector v) vectoyaw
VM_CL_spawn,				// #14 entity() spawn
VM_remove,					// #15 void(entity e) remove
VM_CL_traceline,			// #16 float(vector v1, vector v2, float tryents) traceline
0,
VM_find,					// #18 entity(entity start, .string fld, string match) find
VM_CL_precache_sound,		// #19 void(string s) precache_sound
VM_CL_precache_model,		// #20 void(string s) precache_model
0,
VM_CL_findradius,			// #22 entity(vector org, float rad) findradius
0,
0,
VM_dprint,					// #25 void(string s) dprint
VM_ftos,					// #26 void(string s) ftos
VM_vtos,					// #27 void(string s) vtos
VM_coredump,				// #28 void() coredump
VM_traceon,					// #29 void() traceon
VM_traceoff,				// #30 void() traceoff
VM_eprint,					// #31 void(entity e) eprint
0,
NULL,						// #33
VM_CL_droptofloor,			// #34 float() droptofloor
VM_CL_lightstyle,			// #35 void(float style, string value) lightstyle
VM_rint,					// #36 float(float v) rint
VM_floor,					// #37 float(float v) floor
VM_ceil,					// #38 float(float v) ceil
NULL,						// #39
VM_CL_checkbottom,			// #40 float(entity e) checkbottom
VM_CL_pointcontents,		// #41 float(vector v) pointcontents
NULL,						// #42
VM_fabs,					// #43 float(float f) fabs
0,
VM_cvar,					// #45 float(string s) cvar
VM_localcmd,				// #46 void(string s) localcmd
VM_nextent,					// #47 entity(entity e) nextent
VM_CL_particle,				// #48 void(vector o, vector d, float color, float count) particle
VM_CL_changeyaw,			// #49 void(entity ent, float ideal_yaw, float speed_yaw) ChangeYaw
NULL,						// #50
VM_vectoangles,				// #51 vector(vector v) vectoangles
0,			// #52 void(float to, float f) WriteByte
0,			// #53 void(float to, float f) WriteChar
0,			// #54 void(float to, float f) WriteShort
0,			// #55 void(float to, float f) WriteLong
0,			// #56 void(float to, float f) WriteCoord
0,			// #57 void(float to, float f) WriteAngle
0,			// #58 void(float to, string s) WriteString
0,
VM_sin,						// #60 float(float f) sin (DP_QC_SINCOSSQRTPOW)
VM_cos,						// #61 float(float f) cos (DP_QC_SINCOSSQRTPOW)
VM_sqrt,					// #62 float(float f) sqrt (DP_QC_SINCOSSQRTPOW)
VM_CL_changepitch,			// #63 void(entity ent, float ideal_pitch, float speed_pitch) changepitch (DP_QC_CHANGEPITCH)
VM_CL_tracetoss,			// #64 void(entity e, entity ignore) tracetoss (DP_QC_TRACETOSS)
VM_etos,					// #65 string(entity ent) etos (DP_QC_ETOS)
NULL,						// #66
0,								// #67
0,								// #68
0,								// #69
0,								// #70
NULL,						// #71
VM_cvar_set,				// #72 void(string var, string val) cvar_set
0,								// #73
VM_CL_ambientsound,			// #74 void(vector pos, string samp, float vol, float atten) ambientsound
VM_CL_precache_model,		// #75 string(string s) precache_model2
VM_CL_precache_sound,		// #76 string(string s) precache_sound2
0,								// #77
VM_chr,						// #78
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
VM_CL_tracebox,				// #90 void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox (DP_QC_TRACEBOX)
VM_randomvec,				// #91 vector() randomvec (DP_QC_RANDOMVEC)
VM_CL_getlight,				// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
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
e10,	//#200-209
0,	//#210
0,	//#211
0,	//#212
0,	//#213
0,	//#214
0,	//#215
0,	//#216
0,	//#217
VM_bitshift,				//#218 float(float number, float quantity) bitshift (EXT_BITSHIFT)
0,	//#219
0,	//#220
0,	//#221
VM_charindex,				//#222 float(string str, float ofs) str2chr (FTE_STRINGS)
VM_chr2str,					//#223 string(float c, ...) chr2str (FTE_STRINGS)
0,	//#224
0,	//#225
0,	//#226
0,	//#227
VM_strncmp,					//#228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
0,
e10, e10, e10, e10, e10, e10, e10,	// #230-299

//======CSQC start=======//
//3d world (buffer/buffering) operations
VM_R_ClearScene,			//#300 void() clearscene (EXT_CSQC)
VM_R_AddEntities,			//#301 void(float mask) addentities (EXT_CSQC)
VM_R_AddEntity,				//#302 void(entity ent) addentity (EXT_CSQC)
VM_R_SetView,				//#303 float(float property, ...) setproperty (EXT_CSQC)
VM_R_RenderScene,			//#304 void() renderscene (EXT_CSQC)
VM_R_AddDynamicLight,		//#305 void(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)
VM_R_PolygonBegin,			//#306 void(string texturename, float flag[, float is2d, float lines]) R_BeginPolygon
VM_R_PolygonVertex,			//#307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex
VM_R_PolygonEnd,			//#308 void() R_EndPolygon
0,			//#309

//maths stuff that uses the current view settings
VM_CL_unproject,			//#310 vector (vector v) cs_unproject (EXT_CSQC)
VM_CL_project,				//#311 vector (vector v) cs_project (EXT_CSQC)
0,			//#312
0,			//#313
0,			//#314

//2d (immediate) operations
VM_drawline,				//#315 void(float width, vector pos1, vector pos2, float flag) drawline (EXT_CSQC)
VM_iscachedpic,				//#316 float(string name) iscachedpic (EXT_CSQC)
VM_precache_pic,			//#317 string(string name, float trywad) precache_pic (EXT_CSQC)
VM_getimagesize,			//#318 vector(string picname) draw_getimagesize (EXT_CSQC)
VM_freepic,					//#319 void(string name) freepic (EXT_CSQC)
VM_drawcharacter,			//#320 float(vector position, float character, vector scale, vector rgb, float alpha, float flag) drawcharacter (EXT_CSQC)
VM_drawstring,				//#321 float(vector position, string text, vector scale, vector rgb, float alpha, float flag) drawstring (EXT_CSQC)
VM_drawpic,					//#322 float(vector position, string pic, vector size, vector rgb, float alpha, float flag) drawpic (EXT_CSQC)
VM_drawfill,				//#323 float(vector position, vector size, vector rgb, float alpha, float flag) drawfill (EXT_CSQC)
VM_drawsetcliparea,			//#324 void(float x, float y, float width, float height) drawsetcliparea
VM_drawresetcliparea,		//#325 void(void) drawresetcliparea
0,			//#326
0,			//#327
0,			//#328
0,			//#329

VM_CL_getstatf,				//#330 float(float stnum) getstatf (EXT_CSQC)
VM_CL_getstati,				//#331 float(float stnum) getstati (EXT_CSQC)
VM_CL_getstats,				//#332 string(float firststnum) getstats (EXT_CSQC)
VM_CL_setmodelindex,		//#333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
VM_CL_modelnameforindex,	//#334 string(float mdlindex) modelnameforindex (EXT_CSQC)
VM_CL_particleeffectnum,	//#335 float(string effectname) particleeffectnum (EXT_CSQC)
VM_CL_trailparticles,		//#336 void(entity ent, float effectnum, vector start, vector end) trailparticles (EXT_CSQC)
VM_CL_pointparticles,		//#337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
VM_CL_centerprint,			//#338 void(string s) cprint (EXT_CSQC)
VM_print,					//#339 void(string s) print (EXT_CSQC)
VM_keynumtostring,			//#340 string(float keynum) keynumtostring (EXT_CSQC)
VM_stringtokeynum,			//#341 float(string keyname) stringtokeynum (EXT_CSQC)
VM_CL_getkeybind,			//#342 string(float keynum) getkeybind (EXT_CSQC)
VM_CL_setcursormode,		//#343 void(float usecursor) setcursormode (EXT_CSQC)
VM_getmousepos,				//#344 vector() getmousepos (EXT_CSQC)
VM_CL_getinputstate,		//#345 float(float framenum) getinputstate (EXT_CSQC)
VM_CL_setsensitivityscale,	//#346 void(float sens) setsensitivityscaler (EXT_CSQC)
VM_CL_runplayerphysics,		//#347 void() runstandardplayerphysics (EXT_CSQC)
VM_CL_getplayerkey,			//#348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
VM_CL_isdemo,				//#349 float() isdemo (EXT_CSQC)
VM_isserver,				//#350 float() isserver (EXT_CSQC)
VM_CL_setlistener,			//#351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
VM_CL_registercmd,			//#352 void(string cmdname) registercommand (EXT_CSQC)
VM_WasFreed,				//#353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)
VM_CL_playernum,			//#354 float() playernum
VM_CL_onground,				//#355 float() cl_onground (EXT_CSQC)
VM_charindex,				//#356 float(string s, float num) charindex
VM_CL_selecttraceline,		//#357 float(vector start, vector end, float ignore, float csqcents) selecttraceline
0,			//#358
0,			//#359
VM_CL_ReadByte,				//#360 float() readbyte (EXT_CSQC)
VM_CL_ReadChar,				//#361 float() readchar (EXT_CSQC)
VM_CL_ReadShort,			//#362 float() readshort (EXT_CSQC)
VM_CL_ReadLong,				//#363 float() readlong (EXT_CSQC)
VM_CL_ReadCoord,			//#364 float() readcoord (EXT_CSQC)
VM_CL_ReadAngle,			//#365 float() readangle (EXT_CSQC)
VM_CL_ReadString,			//#366 string() readstring (EXT_CSQC)
VM_CL_ReadFloat,			//#367 float() readfloat (EXT_CSQC)
0,			//#368
0,			//#369
0,			//#370
0,			//#371
0,			//#372
0,			//#373
0,			//#374
0,			//#375
0,			//#376
0,			//#377
0,			//#378
0,			//#379
0,			//#380
0,			//#381
0,			//#382
0,			//#383
0,			//#384
0,			//#385
0,			//#386
0,			//#387
0,			//#388
0,			//#389
0,			//#390
0,			//#391
0,			//#392
0,			//#393
0,			//#394
0,			//#395
0,			//#396
0,			//#397
0,			//#398
0,			//#399
//=========CSQC end========//

VM_copyentity,					// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
0,
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
VM_CL_getsurfacenumpoints,		// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
VM_CL_getsurfacepoint,			// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
VM_CL_getsurfacenormal,			// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
VM_CL_getsurfacetexture,		// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
VM_CL_getsurfacenearpoint,		// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
VM_CL_getsurfaceclippedpoint,	// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
0,									// #440
VM_tokenize,				// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_argv,					// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_CL_setattachment,		// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
VM_search_begin,			// #444 float(string pattern, float caseinsensitive, float quiet) search_begin (DP_FS_SEARCH)
VM_search_end,				// #445 void(float handle) search_end (DP_FS_SEARCH)
VM_search_getsize,			// #446 float(float handle) search_getsize (DP_FS_SEARCH)
VM_search_getfilename,		// #447 string(float handle, float num) search_getfilename (DP_FS_SEARCH)
VM_cvar_string,				// #448 string(string s) cvar_string (DP_QC_CVAR_STRING)
VM_findflags,				// #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
VM_findchainflags,			// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
VM_CL_gettagindex,			// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
VM_CL_gettaginfo,			// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
0,								// #453
0,								// #454
0,								// #455
NULL,						// #456
NULL,						// #457
NULL,						// #458
NULL,						// #459
VM_buf_create,				// #460 float() buf_create (DP_QC_STRINGBUFFERS)
VM_buf_del,					// #461 void(float bufhandle) buf_del (DP_QC_STRINGBUFFERS)
VM_buf_getsize,				// #462 float(float bufhandle) buf_getsize (DP_QC_STRINGBUFFERS)
VM_buf_copy,				// #463 void(float bufhandle_from, float bufhandle_to) buf_copy (DP_QC_STRINGBUFFERS)
VM_buf_sort,				// #464 void(float bufhandle, float sortpower, float backward) buf_sort (DP_QC_STRINGBUFFERS)
VM_buf_implode,				// #465 string(float bufhandle, string glue) buf_implode (DP_QC_STRINGBUFFERS)
VM_bufstr_get,				// #466 string(float bufhandle, float string_index) bufstr_get (DP_QC_STRINGBUFFERS)
VM_bufstr_set,				// #467 void(float bufhandle, float string_index, string str) bufstr_set (DP_QC_STRINGBUFFERS)
VM_bufstr_add,				// #468 float(float bufhandle, string str, float order) bufstr_add (DP_QC_STRINGBUFFERS)
VM_bufstr_free,				// #469 void(float bufhandle, float string_index) bufstr_free (DP_QC_STRINGBUFFERS)
e10, e10, e10			// #470-499 (LordHavoc)
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t);

void VM_CL_Cmd_Init(void)
{
}

void VM_CL_Cmd_Reset(void)
{
}

