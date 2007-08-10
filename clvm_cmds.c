#include "prvm_cmds.h"
#include "csprogs.h"
#include "cl_collision.h"
#include "r_shadow.h"

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

sfx_t *S_FindName(const char *name);
int Sbar_GetPlayer (int index);
void Sbar_SortFrags (void);
void CL_FindNonSolidLocation(const vec3_t in, vec3_t out, vec_t radius);
void CSQC_RelinkAllEntities (int drawmask);
void CSQC_RelinkCSQCEntities (void);
char *Key_GetBind (int key);






// #1 void(vector ang) makevectors
static void VM_CL_makevectors (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_makevectors);
	AngleVectors (PRVM_G_VECTOR(OFS_PARM0), prog->globals.client->v_forward, prog->globals.client->v_right, prog->globals.client->v_up);
}

// #2 void(entity e, vector o) setorigin
static void VM_CL_setorigin (void)
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
	VectorCopy (org, e->fields.client->origin);
	CL_LinkEdict(e);
}

// #3 void(entity e, string m) setmodel
static void VM_CL_setmodel (void)
{
	prvm_edict_t	*e;
	const char		*m;
	struct model_s	*mod;
	int				i;

	VM_SAFEPARMCOUNT(2, VM_CL_setmodel);

	e = PRVM_G_EDICT(OFS_PARM0);
	m = PRVM_G_STRING(OFS_PARM1);
	for (i = 0;i < MAX_MODELS && cl.csqc_model_precache[i];i++)
	{
		if (!strcmp(cl.csqc_model_precache[i]->name, m))
		{
			e->fields.client->model = PRVM_SetEngineString(cl.csqc_model_precache[i]->name);
			e->fields.client->modelindex = -(i+1);
			return;
		}
	}

	for (i = 0;i < MAX_MODELS;i++)
	{
		mod = cl.model_precache[i];
		if (mod && !strcmp(mod->name, m))
		{
			e->fields.client->model = PRVM_SetEngineString(mod->name);
			e->fields.client->modelindex = i;
			return;
		}
	}

	e->fields.client->modelindex = 0;
	e->fields.client->model = 0;
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

	VectorCopy (min, e->fields.client->mins);
	VectorCopy (max, e->fields.client->maxs);
	VectorSubtract (max, min, e->fields.client->size);

	CL_LinkEdict(e);
}

// #8 void(entity e, float chan, string samp, float volume, float atten) sound
static void VM_CL_sound (void)
{
	const char			*sample;
	int					channel;
	prvm_edict_t		*entity;
	int 				volume;
	float				attenuation;

	VM_SAFEPARMCOUNT(5, VM_CL_sound);

	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = (int)PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = (int)(PRVM_G_FLOAT(OFS_PARM3)*255.0f);
	attenuation = PRVM_G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 255)
	{
		VM_Warning("VM_CL_sound: volume must be in range 0-1\n");
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		VM_Warning("VM_CL_sound: attenuation must be in range 0-4\n");
		return;
	}

	if (channel < 0 || channel > 7)
	{
		VM_Warning("VM_CL_sound: channel must be in range 0-7\n");
		return;
	}

	S_StartSound(32768 + PRVM_NUM_FOR_EDICT(entity), channel, S_FindName(sample), entity->fields.client->origin, volume, attenuation);
}

// #14 entity() spawn
static void VM_CL_spawn (void)
{
	prvm_edict_t *ed;
	ed = PRVM_ED_Alloc();
	ed->fields.client->entnum = PRVM_NUM_FOR_EDICT(ed);	//[515]: not needed any more ?
	VM_RETURN_EDICT(ed);
}

// #16 float(vector v1, vector v2, float movetype, entity ignore) traceline
static void VM_CL_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		move;
	prvm_edict_t	*ent;

	VM_SAFEPARMCOUNTRANGE(4, 8, VM_CL_traceline); // allow more parameters for future expansion

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);
	move = (int)PRVM_G_FLOAT(OFS_PARM2);
	ent = PRVM_G_EDICT(OFS_PARM3);

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in traceline('%f %f %f', '%f %f %f', %i, entity %i)\n", PRVM_NAME, v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], move, PRVM_EDICT_TO_PROG(ent));

	trace = CL_Move(v1, vec3_origin, vec3_origin, v2, move, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

	VM_SetTraceGlobals(&trace);
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
	int		move;
	prvm_edict_t	*ent;

	VM_SAFEPARMCOUNTRANGE(6, 8, VM_CL_tracebox); // allow more parameters for future expansion

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	m1 = PRVM_G_VECTOR(OFS_PARM1);
	m2 = PRVM_G_VECTOR(OFS_PARM2);
	v2 = PRVM_G_VECTOR(OFS_PARM3);
	move = (int)PRVM_G_FLOAT(OFS_PARM4);
	ent = PRVM_G_EDICT(OFS_PARM5);

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in tracebox('%f %f %f', '%f %f %f', '%f %f %f', '%f %f %f', %i, entity %i)\n", PRVM_NAME, v1[0], v1[1], v1[2], m1[0], m1[1], m1[2], m2[0], m2[1], m2[2], v2[0], v2[1], v2[2], move, PRVM_EDICT_TO_PROG(ent));

	trace = CL_Move(v1, m1, m2, v2, move, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

	VM_SetTraceGlobals(&trace);
}

trace_t CL_Trace_Toss (prvm_edict_t *tossent, prvm_edict_t *ignore)
{
	int i;
	float gravity;
	vec3_t move, end;
	vec3_t original_origin;
	vec3_t original_velocity;
	vec3_t original_angles;
	vec3_t original_avelocity;
	prvm_eval_t *val;
	trace_t trace;

	VectorCopy(tossent->fields.client->origin   , original_origin   );
	VectorCopy(tossent->fields.client->velocity , original_velocity );
	VectorCopy(tossent->fields.client->angles   , original_angles   );
	VectorCopy(tossent->fields.client->avelocity, original_avelocity);

	val = PRVM_EDICTFIELDVALUE(tossent, prog->fieldoffsets.gravity);
	if (val != NULL && val->_float != 0)
		gravity = val->_float;
	else
		gravity = 1.0;
	gravity *= cl.movevars_gravity * 0.05;

	for (i = 0;i < 200;i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		tossent->fields.client->velocity[2] -= gravity;
		VectorMA (tossent->fields.client->angles, 0.05, tossent->fields.client->avelocity, tossent->fields.client->angles);
		VectorScale (tossent->fields.client->velocity, 0.05, move);
		VectorAdd (tossent->fields.client->origin, move, end);
		trace = CL_Move (tossent->fields.client->origin, tossent->fields.client->mins, tossent->fields.client->maxs, end, MOVE_NORMAL, tossent, CL_GenericHitSuperContentsMask(tossent), true, true, NULL, true);
		VectorCopy (trace.endpos, tossent->fields.client->origin);

		if (trace.fraction < 1)
			break;
	}

	VectorCopy(original_origin   , tossent->fields.client->origin   );
	VectorCopy(original_velocity , tossent->fields.client->velocity );
	VectorCopy(original_angles   , tossent->fields.client->angles   );
	VectorCopy(original_avelocity, tossent->fields.client->avelocity);

	return trace;
}

static void VM_CL_tracetoss (void)
{
	trace_t	trace;
	prvm_edict_t	*ent;
	prvm_edict_t	*ignore;

	prog->xfunction->builtinsprofile += 600;

	VM_SAFEPARMCOUNT(2, VM_CL_tracetoss);

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("tracetoss: can not use world entity\n");
		return;
	}
	ignore = PRVM_G_EDICT(OFS_PARM1);

	trace = CL_Trace_Toss (ent, ignore);

	VM_SetTraceGlobals(&trace);
}


// #20 void(string s) precache_model
static void VM_CL_precache_model (void)
{
	const char	*name;
	int			i;
	model_t		*m;

	VM_SAFEPARMCOUNT(1, VM_CL_precache_model);

	name = PRVM_G_STRING(OFS_PARM0);
	for (i = 1;i < MAX_MODELS && cl.csqc_model_precache[i];i++)
	{
		if(!strcmp(cl.csqc_model_precache[i]->name, name))
		{
			PRVM_G_FLOAT(OFS_RETURN) = -(i+1);
			return;
		}
	}
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	m = Mod_ForName(name, false, false, false);
	if(m && m->loaded)
	{
		for (i = 1;i < MAX_MODELS;i++)
		{
			if (!cl.csqc_model_precache[i])
			{
				cl.csqc_model_precache[i] = (model_t*)m;
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
		if(BoxesOverlap(mins, maxs, ent->fields.client->absmin, ent->fields.client->absmax))
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
	prvm_edict_t	*touchedicts[MAX_EDICTS];

	VM_SAFEPARMCOUNT(2, VM_CL_findradius);

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
			VectorMAMAM(1, eorg, -0.5f, ent->fields.client->mins, -0.5f, ent->fields.client->maxs, eorg);
		if (DotProduct(eorg, eorg) < radius2)
		{
			ent->fields.client->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}

	VM_RETURN_EDICT(chain);
}

// #34 float() droptofloor
static void VM_CL_droptofloor (void)
{
	prvm_edict_t		*ent;
	prvm_eval_t			*val;
	vec3_t				end;
	trace_t				trace;

	VM_SAFEPARMCOUNTRANGE(0, 2, VM_CL_droptofloor); // allow 2 parameters because the id1 defs.qc had an incorrect prototype

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(prog->globals.client->self);
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

	VectorCopy (ent->fields.client->origin, end);
	end[2] -= 256;

	trace = CL_Move(ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->maxs, end, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

	if (trace.fraction != 1)
	{
		VectorCopy (trace.endpos, ent->fields.client->origin);
		ent->fields.client->flags = (int)ent->fields.client->flags | FL_ONGROUND;
		if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.groundentity)))
			val->edict = PRVM_EDICT_TO_PROG(trace.ent);
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
	strlcpy (cl.lightstyle[i].map,  MSG_ReadString(), sizeof (cl.lightstyle[i].map));
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
	trace = CL_Move (start, vec3_origin, vec3_origin, stop, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

	if (trace.fraction == 1.0)
		return;

	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = CL_Move (start, vec3_origin, vec3_origin, stop, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

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

// #92 vector(vector org) getlight (DP_QC_GETLIGHT)
static void VM_CL_getlight (void)
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
extern qboolean CSQC_AddRenderEdict (prvm_edict_t *ed);//csprogs.c

static void CSQC_R_RecalcView (void)
{
	extern matrix4x4_t viewmodelmatrix;
	Matrix4x4_CreateFromQuakeEntity(&r_view.matrix, cl.csqc_origin[0], cl.csqc_origin[1], cl.csqc_origin[2], cl.csqc_angles[0], cl.csqc_angles[1], cl.csqc_angles[2], 1);
	Matrix4x4_CreateFromQuakeEntity(&viewmodelmatrix, cl.csqc_origin[0], cl.csqc_origin[1], cl.csqc_origin[2], cl.csqc_angles[0], cl.csqc_angles[1], cl.csqc_angles[2], cl_viewmodel_scale.value);
}

void CL_RelinkLightFlashes(void);
//#300 void() clearscene (EXT_CSQC)
static void VM_CL_R_ClearScene (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_R_ClearScene);
	// clear renderable entity and light lists
	r_refdef.numentities = 0;
	r_refdef.numlights = 0;
	// FIXME: restore these to the values from VM_CL_UpdateView
	r_view.x = 0;
	r_view.y = 0;
	r_view.z = 0;
	r_view.width = vid.width;
	r_view.height = vid.height;
	r_view.depth = 1;
	// FIXME: restore frustum_x/frustum_y
	r_view.useperspective = true;
	r_view.frustum_y = tan(scr_fov.value * M_PI / 360.0) * (3.0/4.0) * cl.viewzoom;
	r_view.frustum_x = r_view.frustum_y * (float)r_view.width / (float)r_view.height / vid_pixelheight.value;
	r_view.frustum_x *= r_refdef.frustumscale_x;
	r_view.frustum_y *= r_refdef.frustumscale_y;
	r_view.ortho_x = scr_fov.value * (3.0 / 4.0) * (float)r_view.width / (float)r_view.height / vid_pixelheight.value;
	r_view.ortho_y = scr_fov.value * (3.0 / 4.0);
	// FIXME: restore cl.csqc_origin
	// FIXME: restore cl.csqc_angles
	cl.csqc_vidvars.drawworld = true;
	cl.csqc_vidvars.drawenginesbar = false;
	cl.csqc_vidvars.drawcrosshair = false;
}

//#301 void(float mask) addentities (EXT_CSQC)
extern void CSQC_Predraw (prvm_edict_t *ed);//csprogs.c
extern void CSQC_Think (prvm_edict_t *ed);//csprogs.c
static void VM_CL_R_AddEntities (void)
{
	int			i, drawmask;
	prvm_edict_t *ed;
	VM_SAFEPARMCOUNT(1, VM_CL_R_AddEntities);
	drawmask = (int)PRVM_G_FLOAT(OFS_PARM0);
	CSQC_RelinkAllEntities(drawmask);
	CL_RelinkLightFlashes();

	prog->globals.client->time = cl.time;
	for(i=1;i<prog->num_edicts;i++)
	{
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
		if(!((int)ed->fields.client->drawmask & drawmask))
			continue;
		CSQC_AddRenderEdict(ed);
	}
}

//#302 void(entity ent) addentity (EXT_CSQC)
static void VM_CL_R_AddEntity (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_R_AddEntity);
	CSQC_AddRenderEdict(PRVM_G_EDICT(OFS_PARM0));
}

//#303 float(float property, ...) setproperty (EXT_CSQC)
static void VM_CL_R_SetView (void)
{
	int		c;
	float	*f;
	float	k;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_CL_R_SetView);

	c = (int)PRVM_G_FLOAT(OFS_PARM0);
	f = PRVM_G_VECTOR(OFS_PARM1);
	k = PRVM_G_FLOAT(OFS_PARM1);

	switch(c)
	{
	case VF_MIN:			r_view.x = (int)(f[0] * vid.width / vid_conwidth.value);
							r_view.y = (int)(f[1] * vid.height / vid_conheight.value);
							break;
	case VF_MIN_X:			r_view.x = (int)(k * vid.width / vid_conwidth.value);
							break;
	case VF_MIN_Y:			r_view.y = (int)(k * vid.height / vid_conheight.value);
							break;
	case VF_SIZE:			r_view.width = (int)(f[0] * vid.width / vid_conwidth.value);
							r_view.height = (int)(f[1] * vid.height / vid_conheight.value);
							break;
	case VF_SIZE_Y:			r_view.width = (int)(k * vid.width / vid_conwidth.value);
							break;
	case VF_SIZE_X:			r_view.height = (int)(k * vid.height / vid_conheight.value);
							break;
	case VF_VIEWPORT:		r_view.x = (int)(f[0] * vid.width / vid_conwidth.value);
							r_view.y = (int)(f[1] * vid.height / vid_conheight.value);
							f = PRVM_G_VECTOR(OFS_PARM2);
							r_view.width = (int)(f[0] * vid.width / vid_conwidth.value);
							r_view.height = (int)(f[1] * vid.height / vid_conheight.value);
							break;
	case VF_FOV:			r_view.frustum_x = tan(f[0] * M_PI / 360.0);r_view.ortho_x = f[0];
							r_view.frustum_y = tan(f[1] * M_PI / 360.0);r_view.ortho_y = f[1];
							break;
	case VF_FOVX:			r_view.frustum_x = tan(k * M_PI / 360.0);r_view.ortho_x = k;
							break;
	case VF_FOVY:			r_view.frustum_y = tan(k * M_PI / 360.0);r_view.ortho_y = k;
							break;
	case VF_ORIGIN:			VectorCopy(f, cl.csqc_origin);
							CSQC_R_RecalcView();
							break;
	case VF_ORIGIN_X:		cl.csqc_origin[0] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ORIGIN_Y:		cl.csqc_origin[1] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ORIGIN_Z:		cl.csqc_origin[2] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES:			VectorCopy(f, cl.csqc_angles);
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES_X:		cl.csqc_angles[0] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES_Y:		cl.csqc_angles[1] = k;
							CSQC_R_RecalcView();
							break;
	case VF_ANGLES_Z:		cl.csqc_angles[2] = k;
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

	case VF_PERSPECTIVE:	r_view.useperspective = k != 0;
							break;

	default:				PRVM_G_FLOAT(OFS_RETURN) = 0;
							VM_Warning("VM_CL_R_SetView : unknown parm %i\n", c);
							return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

//#304 void() renderscene (EXT_CSQC)
static void VM_CL_R_RenderScene (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_R_RenderScene);
	// we need to update any RENDER_VIEWMODEL entities at this point because
	// csqc supplies its own view matrix
	CL_UpdateViewEntities();
	// now draw stuff!
	R_RenderView();
}

//#305 void(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)
static void VM_CL_R_AddDynamicLight (void)
{
	float		*pos, *col;
	matrix4x4_t	matrix;
	VM_SAFEPARMCOUNTRANGE(3, 8, VM_CL_R_AddDynamicLight); // allow more than 3 because we may extend this in the future

	// if we've run out of dlights, just return
	if (r_refdef.numlights >= MAX_DLIGHTS)
		return;

	pos = PRVM_G_VECTOR(OFS_PARM0);
	col = PRVM_G_VECTOR(OFS_PARM2);
	Matrix4x4_CreateFromQuakeEntity(&matrix, pos[0], pos[1], pos[2], 0, 0, 0, PRVM_G_FLOAT(OFS_PARM1));
	R_RTLight_Update(&r_refdef.lights[r_refdef.numlights++], false, &matrix, col, -1, NULL, true, 1, 0.25, 0, 1, 1, LIGHTFLAG_NORMALMODE | LIGHTFLAG_REALTIMEMODE);
}

//============================================================================

//#310 vector (vector v) cs_unproject (EXT_CSQC)
static void VM_CL_unproject (void)
{
	float	*f;
	vec3_t	temp;

	VM_SAFEPARMCOUNT(1, VM_CL_unproject);
	f = PRVM_G_VECTOR(OFS_PARM0);
	VectorSet(temp, f[2], f[0] * f[2] * -r_view.frustum_x * 2.0 / r_view.width, f[1] * f[2] * -r_view.frustum_y * 2.0 / r_view.height);
	Matrix4x4_Transform(&r_view.matrix, temp, PRVM_G_VECTOR(OFS_RETURN));
}

//#311 vector (vector v) cs_project (EXT_CSQC)
static void VM_CL_project (void)
{
	float	*f;
	vec3_t	v;
	matrix4x4_t m;

	VM_SAFEPARMCOUNT(1, VM_CL_project);
	f = PRVM_G_VECTOR(OFS_PARM0);
	Matrix4x4_Invert_Simple(&m, &r_view.matrix);
	Matrix4x4_Transform(&m, f, v);
	VectorSet(PRVM_G_VECTOR(OFS_RETURN), v[1]/v[0]/-r_view.frustum_x*0.5*r_view.width, v[2]/v[0]/-r_view.frustum_y*r_view.height*0.5, v[0]);
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

	t->fields.client->model = 0;
	t->fields.client->modelindex = 0;

	if (!i)
		return;

	model = CL_GetModelByIndex(i);
	if (!model)
	{
		VM_Warning("VM_CL_setmodelindex: null model\n");
		return;
	}
	t->fields.client->model = PRVM_SetEngineString(model->name);
	t->fields.client->modelindex = i;
}

//#334 string(float mdlindex) modelnameforindex (EXT_CSQC)
static void VM_CL_modelnameforindex (void)
{
	model_t *model;

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

	CL_ParticleEffect(i, VectorDistance(start, end), start, end, t->fields.client->velocity, t->fields.client->velocity, NULL, prog->argc >= 5 ? (int)PRVM_G_FLOAT(OFS_PARM4) : 0);
}

//#337 void(float effectnum, vector origin, vector dir, float count[, float color]) pointparticles (EXT_CSQC)
static void VM_CL_pointparticles (void)
{
	int			i, n;
	float		*f, *v;
	VM_SAFEPARMCOUNTRANGE(4, 5, VM_CL_pointparticles);
	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	f = PRVM_G_VECTOR(OFS_PARM1);
	v = PRVM_G_VECTOR(OFS_PARM2);
	n = (int)PRVM_G_FLOAT(OFS_PARM3);
	CL_ParticleEffect(i, n, f, f, v, v, NULL, prog->argc >= 5 ? (int)PRVM_G_FLOAT(OFS_PARM4) : 0);
}

//#342 string(float keynum) getkeybind (EXT_CSQC)
static void VM_CL_getkeybind (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_getkeybind);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(Key_GetBind((int)PRVM_G_FLOAT(OFS_PARM0)));
}

//#343 void(float usecursor) setcursormode (EXT_CSQC)
static void VM_CL_setcursormode (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_setcursormode);
	cl.csqc_wantsmousemove = PRVM_G_FLOAT(OFS_PARM0);
	cl_ignoremousemove = true;
}

//#345 float(float framenum) getinputstate (EXT_CSQC)
static void VM_CL_getinputstate (void)
{
	int i, frame;
	VM_SAFEPARMCOUNT(1, VM_CL_getinputstate);
	frame = (int)PRVM_G_FLOAT(OFS_PARM0);
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

	i = Sbar_GetPlayer(i);
	if(i < 0)
		return;

	t[0] = 0;

	if(!strcasecmp(c, "name"))
		strlcpy(t, cl.scores[i].name, sizeof(t));
	else
		if(!strcasecmp(c, "frags"))
			sprintf(t, "%i", cl.scores[i].frags);
	else
		if(!strcasecmp(c, "ping"))
			sprintf(t, "%i", cl.scores[i].qw_ping);
	else
		if(!strcasecmp(c, "pl"))
			sprintf(t, "%i", cl.scores[i].qw_packetloss);
	else
		if(!strcasecmp(c, "entertime"))
			sprintf(t, "%f", cl.scores[i].qw_entertime);
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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(t);
}

//#349 float() isdemo (EXT_CSQC)
static void VM_CL_isdemo (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_isdemo);
	PRVM_G_FLOAT(OFS_RETURN) = cls.demoplayback;
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
		prvm_eval_t *val;
		entity_t *staticent = &cl.static_entities[cl.num_static_entities++];

		// copy it to the current state
		staticent->render.model = CL_GetModelByIndex((int)ent->fields.client->modelindex);
		staticent->render.frame1 = staticent->render.frame2 = (int)ent->fields.client->frame;
		staticent->render.framelerp = 0;
		// make torchs play out of sync
		staticent->render.frame1time = staticent->render.frame2time = lhrandom(-10, -1);
		staticent->render.colormap = (int)ent->fields.client->colormap; // no special coloring
		staticent->render.skinnum = (int)ent->fields.client->skin;
		staticent->render.effects = (int)ent->fields.client->effects;
		staticent->render.alpha = 1;
		if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.alpha)) && val->_float) staticent->render.alpha = val->_float;
		staticent->render.scale = 1;
		if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.scale)) && val->_float) staticent->render.scale = val->_float;
		if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.colormod)) && VectorLength2(val->vector)) VectorCopy(val->vector, staticent->render.colormod);

		renderflags = 0;
		if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.renderflags)) && val->_float) renderflags = (int)val->_float;
		if (renderflags & RF_USEAXIS)
		{
			vec3_t left;
			VectorNegate(prog->globals.client->v_right, left);
			Matrix4x4_FromVectors(&staticent->render.matrix, prog->globals.client->v_forward, left, prog->globals.client->v_up, ent->fields.client->origin);
			Matrix4x4_Scale(&staticent->render.matrix, staticent->render.scale, 1);
		}
		else
			Matrix4x4_CreateFromQuakeEntity(&staticent->render.matrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2], ent->fields.client->angles[0], ent->fields.client->angles[1], ent->fields.client->angles[2], staticent->render.scale);
		CL_UpdateRenderEntity(&staticent->render);

		// either fullbright or lit
		if (!(staticent->render.effects & EF_FULLBRIGHT) && !r_fullbright.integer)
			staticent->render.flags |= RENDER_LIGHT;
		// turn off shadows from transparent objects
		if (!(staticent->render.effects & (EF_NOSHADOW | EF_ADDITIVE | EF_NODEPTHTEST)) && (staticent->render.alpha >= 1))
			staticent->render.flags |= RENDER_SHADOW;
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
	memcpy(out->fields.vp, in->fields.vp, prog->progs->entityfields * 4);
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
	tempcolor = (unsigned char *)&palette_complete[(rand()%colorLength) + colorStart];
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


//====================================================================
//DP_QC_GETSURFACE

extern void clippointtosurface(model_t *model, msurface_t *surface, vec3_t p, vec3_t out);

static msurface_t *cl_getsurface(model_t *model, int surfacenum)
{
	if (surfacenum < 0 || surfacenum >= model->nummodelsurfaces)
		return NULL;
	return model->data_surfaces + surfacenum + model->firstmodelsurface;
}

// #434 float(entity e, float s) getsurfacenumpoints
static void VM_CL_getsurfacenumpoints(void)
{
	model_t *model;
	msurface_t *surface;
	VM_SAFEPARMCOUNT(2, VM_CL_getsurfacenumpoints);
	// return 0 if no such surface
	if (!(model = CL_GetModelFromEdict(PRVM_G_EDICT(OFS_PARM0))) || !(surface = cl_getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// note: this (incorrectly) assumes it is a simple polygon
	PRVM_G_FLOAT(OFS_RETURN) = surface->num_vertices;
}

// #435 vector(entity e, float s, float n) getsurfacepoint
static void VM_CL_getsurfacepoint(void)
{
	prvm_edict_t *ed;
	model_t *model;
	msurface_t *surface;
	int pointnum;
	VM_SAFEPARMCOUNT(3, VM_CL_getsurfacenumpoints);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!(model = CL_GetModelFromEdict(ed)) || !(surface = cl_getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// note: this (incorrectly) assumes it is a simple polygon
	pointnum = (int)PRVM_G_FLOAT(OFS_PARM2);
	if (pointnum < 0 || pointnum >= surface->num_vertices)
		return;
	// FIXME: implement rotation/scaling
	VectorAdd(&(model->surfmesh.data_vertex3f + 3 * surface->num_firstvertex)[pointnum * 3], ed->fields.client->origin, PRVM_G_VECTOR(OFS_RETURN));
}

// #436 vector(entity e, float s) getsurfacenormal
static void VM_CL_getsurfacenormal(void)
{
	model_t *model;
	msurface_t *surface;
	vec3_t normal;
	VM_SAFEPARMCOUNT(2, VM_CL_getsurfacenormal);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	if (!(model = CL_GetModelFromEdict(PRVM_G_EDICT(OFS_PARM0))) || !(surface = cl_getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	// note: this (incorrectly) assumes it is a simple polygon
	// note: this only returns the first triangle, so it doesn't work very
	// well for curved surfaces or arbitrary meshes
	TriangleNormal((model->surfmesh.data_vertex3f + 3 * surface->num_firstvertex), (model->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + 3, (model->surfmesh.data_vertex3f + 3 * surface->num_firstvertex) + 6, normal);
	VectorNormalize(normal);
	VectorCopy(normal, PRVM_G_VECTOR(OFS_RETURN));
}

// #437 string(entity e, float s) getsurfacetexture
static void VM_CL_getsurfacetexture(void)
{
	model_t *model;
	msurface_t *surface;
	VM_SAFEPARMCOUNT(2, VM_CL_getsurfacetexture);
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	if (!(model = CL_GetModelFromEdict(PRVM_G_EDICT(OFS_PARM0))) || !(surface = cl_getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(surface->texture->name);
}

// #438 float(entity e, vector p) getsurfacenearpoint
static void VM_CL_getsurfacenearpoint(void)
{
	int surfacenum, best;
	vec3_t clipped, p;
	vec_t dist, bestdist;
	prvm_edict_t *ed;
	model_t *model = NULL;
	msurface_t *surface;
	vec_t *point;
	VM_SAFEPARMCOUNT(2, VM_CL_getsurfacenearpoint);
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	ed = PRVM_G_EDICT(OFS_PARM0);
	if(!(model = CL_GetModelFromEdict(ed)) || !model->num_surfaces)
		return;

	// FIXME: implement rotation/scaling
	point = PRVM_G_VECTOR(OFS_PARM1);
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
			clippointtosurface(model, surface, p, clipped);
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
static void VM_CL_getsurfaceclippedpoint(void)
{
	prvm_edict_t *ed;
	model_t *model;
	msurface_t *surface;
	vec3_t p, out;
	VM_SAFEPARMCOUNT(3, VM_CL_getsurfaceclippedpoint);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!(model = CL_GetModelFromEdict(ed)) || !(surface = cl_getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// FIXME: implement rotation/scaling
	VectorSubtract(PRVM_G_VECTOR(OFS_PARM2), ed->fields.client->origin, p);
	clippointtosurface(model, surface, p, out);
	// FIXME: implement rotation/scaling
	VectorAdd(out, ed->fields.client->origin, PRVM_G_VECTOR(OFS_RETURN));
}

// #443 void(entity e, entity tagentity, string tagname) setattachment
static void VM_CL_setattachment (void)
{
	prvm_edict_t *e;
	prvm_edict_t *tagentity;
	const char *tagname;
	prvm_eval_t *v;
	int modelindex;
	model_t *model;
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

	v = PRVM_EDICTFIELDVALUE(e, prog->fieldoffsets.tag_entity);
	if (v)
		v->edict = PRVM_EDICT_TO_PROG(tagentity);

	v = PRVM_EDICTFIELDVALUE(e, prog->fieldoffsets.tag_index);
	if (v)
		v->_float = 0;
	if (tagentity != NULL && tagentity != prog->edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->fields.client->modelindex;
		model = CL_GetModelByIndex(modelindex);
		if (model)
		{
			v->_float = Mod_Alias_GetTagIndexForName(model, (int)tagentity->fields.client->skin, tagname);
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
	model_t *model = CL_GetModelFromEdict(e);
	if (model)
		return Mod_Alias_GetTagIndexForName(model, (int)e->fields.client->skin, tagname);
	else
		return -1;
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
	int reqframe, attachloop;
	matrix4x4_t entitymatrix, tagmatrix, attachmatrix;
	prvm_edict_t *attachent;
	model_t *model;

	*out = identitymatrix; // warnings and errors return identical matrix

	if (ent == prog->edicts)
		return 1;
	if (ent->priv.server->free)
		return 2;

	model = CL_GetModelFromEdict(ent);

	if(!model)
		return 3;

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

	if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.tag_entity)) && val->edict)
	{ // DP_GFX_QUAKE3MODELTAGS, scan all chain and stop on unattached entity
		attachloop = 0;
		do
		{
			attachent = PRVM_EDICT_NUM(val->edict); // to this it entity our entity is attached
			val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.tag_index);

			model = CL_GetModelFromEdict(attachent);

			if (model && val->_float >= 1 && model->animscenes && attachent->fields.client->frame >= 0 && attachent->fields.client->frame < model->numframes)
				Mod_Alias_GetTagMatrix(model, model->animscenes[(int)attachent->fields.client->frame].firstframe, (int)val->_float - 1, &attachmatrix);
			else
				attachmatrix = identitymatrix;

			// apply transformation by child entity matrix
			val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.scale);
			if (val->_float == 0)
				val->_float = 1;
			Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2], -ent->fields.client->angles[0], ent->fields.client->angles[1], ent->fields.client->angles[2], val->_float);
			Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);
			Matrix4x4_Copy(&tagmatrix, out);

			// finally transformate by matrix of tag on parent entity
			Matrix4x4_Concat(out, &attachmatrix, &tagmatrix);
			Matrix4x4_Copy(&tagmatrix, out);

			ent = attachent;
			attachloop += 1;
			if (attachloop > 255) // prevent runaway looping
				return 5;
		}
		while ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.tag_entity)) && val->edict);
	}

	// normal or RENDER_VIEWMODEL entity (or main parent entity on attach chain)
	val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.scale);
	if (val->_float == 0)
		val->_float = 1;
	// Alias models have inverse pitch, bmodels can't have tags, so don't check for modeltype...
	Matrix4x4_CreateFromQuakeEntity(&entitymatrix, ent->fields.client->origin[0], ent->fields.client->origin[1], ent->fields.client->origin[2], -ent->fields.client->angles[0], ent->fields.client->angles[1], ent->fields.client->angles[2], val->_float);
	Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);

	if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.renderflags)) && (RF_VIEWMODEL & (int)val->_float))
	{// RENDER_VIEWMODEL magic
		Matrix4x4_Copy(&tagmatrix, out);

		val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.scale);
		if (val->_float == 0)
			val->_float = 1;

		Matrix4x4_CreateFromQuakeEntity(&entitymatrix, cl.csqc_origin[0], cl.csqc_origin[1], cl.csqc_origin[2], cl.csqc_angles[0], cl.csqc_angles[1], cl.csqc_angles[2], val->_float);
		Matrix4x4_Concat(out, &entitymatrix, &tagmatrix);

		/*
		// Cl_bob, ported from rendering code
		if (ent->fields.client->health > 0 && cl_bob.value && cl_bobcycle.value)
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
			bob = sqrt(ent->fields.client->velocity[0]*ent->fields.client->velocity[0] + ent->fields.client->velocity[1]*ent->fields.client->velocity[1])*cl_bob.value;
			bob = bob*0.3 + bob*0.7*cycle;
			Matrix4x4_AdjustOrigin(out, 0, 0, bound(-7, bob, 4));
		}
		*/
	}
	return 0;
}

// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
static void VM_CL_gettagindex (void)
{
	prvm_edict_t *ent;
	const char *tag_name;
	int modelindex, tag_index;

	VM_SAFEPARMCOUNT(2, VM_CL_gettagindex);

	ent = PRVM_G_EDICT(OFS_PARM0);
	tag_name = PRVM_G_STRING(OFS_PARM1);
	if (ent == prog->edicts)
	{
		VM_Warning("gettagindex: can't affect world entity\n");
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("gettagindex: can't affect free entity\n");
		return;
	}

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
static void VM_CL_gettaginfo (void)
{
	prvm_edict_t *e;
	int tagindex;
	matrix4x4_t tag_matrix;
	int returncode;

	VM_SAFEPARMCOUNT(2, VM_CL_gettaginfo);

	e = PRVM_G_EDICT(OFS_PARM0);
	tagindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	returncode = CL_GetTagMatrix(&tag_matrix, e, tagindex);
	Matrix4x4_ToVectors(&tag_matrix, prog->globals.client->v_forward, prog->globals.client->v_right, prog->globals.client->v_up, PRVM_G_VECTOR(OFS_RETURN));

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
//QC POLYGON functions
//====================

typedef struct
{
	rtexture_t		*tex;
	float			data[36];	//[515]: enough for polygons
	unsigned char			flags;	//[515]: + VM_POLYGON_2D and VM_POLYGON_FL4V flags
}vm_polygon_t;

//static float			vm_polygon_linewidth = 1;
static mempool_t		*vm_polygons_pool = NULL;
static unsigned char			vm_current_vertices = 0;
static qboolean			vm_polygons_initialized = false;
static vm_polygon_t		*vm_polygons = NULL;
static unsigned long	vm_polygons_num = 0, vm_drawpolygons_num = 0;	//[515]: ok long on 64bit ?
static qboolean			vm_polygonbegin = false;	//[515]: for "no-crap-on-the-screen" check
#define VM_DEFPOLYNUM 64	//[515]: enough for default ?

#define VM_POLYGON_FL3V		16	//more than 2 vertices (used only for lines)
#define VM_POLYGON_FLLINES	32
#define VM_POLYGON_FL2D		64
#define VM_POLYGON_FL4V		128	//4 vertices

static void VM_InitPolygons (void)
{
	vm_polygons_pool = Mem_AllocPool("VMPOLY", 0, NULL);
	vm_polygons = (vm_polygon_t *)Mem_Alloc(vm_polygons_pool, VM_DEFPOLYNUM*sizeof(vm_polygon_t));
	memset(vm_polygons, 0, VM_DEFPOLYNUM*sizeof(vm_polygon_t));
	vm_polygons_num = VM_DEFPOLYNUM;
	vm_drawpolygons_num = 0;
	vm_polygonbegin = false;
	vm_polygons_initialized = true;
}

static void VM_DrawPolygonCallback (const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist)
{
	int surfacelistindex;
	// LordHavoc: FIXME: this is stupid code
	for (surfacelistindex = 0;surfacelistindex < numsurfaces;surfacelistindex++)
	{
		const vm_polygon_t	*p = &vm_polygons[surfacelist[surfacelistindex]];
		int					flags = p->flags & 0x0f;

		if(flags == DRAWFLAG_ADDITIVE)
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		else if(flags == DRAWFLAG_MODULATE)
			GL_BlendFunc(GL_DST_COLOR, GL_ZERO);
		else if(flags == DRAWFLAG_2XMODULATE)
			GL_BlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
		else
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		R_Mesh_TexBind(0, R_GetTexture(p->tex));

		CHECKGLERROR
		//[515]: is speed is max ?
		if(p->flags & VM_POLYGON_FLLINES)	//[515]: lines
		{
			qglLineWidth(p->data[13]);CHECKGLERROR
			qglBegin(GL_LINE_LOOP);
				qglTexCoord1f	(p->data[12]);
				qglColor4f		(p->data[20], p->data[21], p->data[22], p->data[23]);
				qglVertex3f		(p->data[0] , p->data[1],  p->data[2]);

				qglTexCoord1f	(p->data[14]);
				qglColor4f		(p->data[24], p->data[25], p->data[26], p->data[27]);
				qglVertex3f		(p->data[3] , p->data[4],  p->data[5]);

				if(p->flags & VM_POLYGON_FL3V)
				{
					qglTexCoord1f	(p->data[16]);
					qglColor4f		(p->data[28], p->data[29], p->data[30], p->data[31]);
					qglVertex3f		(p->data[6] , p->data[7],  p->data[8]);

					if(p->flags & VM_POLYGON_FL4V)
					{
						qglTexCoord1f	(p->data[18]);
						qglColor4f		(p->data[32], p->data[33], p->data[34], p->data[35]);
						qglVertex3f		(p->data[9] , p->data[10],  p->data[11]);
					}
				}
			qglEnd();
			CHECKGLERROR
		}
		else
		{
			qglBegin(GL_POLYGON);
				qglTexCoord2f	(p->data[12], p->data[13]);
				qglColor4f		(p->data[20], p->data[21], p->data[22], p->data[23]);
				qglVertex3f		(p->data[0] , p->data[1],  p->data[2]);

				qglTexCoord2f	(p->data[14], p->data[15]);
				qglColor4f		(p->data[24], p->data[25], p->data[26], p->data[27]);
				qglVertex3f		(p->data[3] , p->data[4],  p->data[5]);

				qglTexCoord2f	(p->data[16], p->data[17]);
				qglColor4f		(p->data[28], p->data[29], p->data[30], p->data[31]);
				qglVertex3f		(p->data[6] , p->data[7],  p->data[8]);

				if(p->flags & VM_POLYGON_FL4V)
				{
					qglTexCoord2f	(p->data[18], p->data[19]);
					qglColor4f		(p->data[32], p->data[33], p->data[34], p->data[35]);
					qglVertex3f		(p->data[9] , p->data[10],  p->data[11]);
				}
			qglEnd();
			CHECKGLERROR
		}
	}
}

static void VM_CL_AddPolygonTo2DScene (vm_polygon_t *p)
{
	drawqueuemesh_t	mesh;
	static int		picelements[6] = {0, 1, 2, 0, 2, 3};

	mesh.texture = p->tex;
	mesh.data_element3i = picelements;
	mesh.data_vertex3f = p->data;
	mesh.data_texcoord2f = p->data + 12;
	mesh.data_color4f = p->data + 20;
	if(p->flags & VM_POLYGON_FL4V)
	{
		mesh.num_vertices = 4;
		mesh.num_triangles = 2;
	}
	else
	{
		mesh.num_vertices = 3;
		mesh.num_triangles = 1;
	}
	if(p->flags & VM_POLYGON_FLLINES)	//[515]: lines
		DrawQ_LineLoop (&mesh, (p->flags&0x0f));
	else
		DrawQ_Mesh (&mesh, (p->flags&0x0f));
}

void VM_CL_AddPolygonsToMeshQueue (void)
{
	int i;
	if(!vm_drawpolygons_num)
		return;
	R_Mesh_Matrix(&identitymatrix);
	GL_CullFace(GL_NONE);
	for(i = 0;i < (int)vm_drawpolygons_num;i++)
		VM_DrawPolygonCallback(NULL, NULL, 1, &i);
	vm_drawpolygons_num = 0;
}

//void(string texturename, float flag[, float 2d[, float lines]]) R_BeginPolygon
static void VM_CL_R_PolygonBegin (void)
{
	vm_polygon_t	*p;
	const char		*picname;
	VM_SAFEPARMCOUNTRANGE(2, 4, VM_CL_R_PolygonBegin);

	if(!vm_polygons_initialized)
		VM_InitPolygons();
	if(vm_polygonbegin)
	{
		VM_Warning("VM_CL_R_PolygonBegin: called twice without VM_CL_R_PolygonEnd after first\n");
		return;
	}
	if(vm_drawpolygons_num >= vm_polygons_num)
	{
		p = (vm_polygon_t *)Mem_Alloc(vm_polygons_pool, 2 * vm_polygons_num * sizeof(vm_polygon_t));
		memset(p, 0, 2 * vm_polygons_num * sizeof(vm_polygon_t));
		memcpy(p, vm_polygons, vm_polygons_num * sizeof(vm_polygon_t));
		Mem_Free(vm_polygons);
		vm_polygons = p;
		vm_polygons_num *= 2;
	}
	p = &vm_polygons[vm_drawpolygons_num];
	picname = PRVM_G_STRING(OFS_PARM0);
	if(picname[0])
		p->tex = Draw_CachePic(picname, true)->tex;
	else
		p->tex = r_texture_white;
	p->flags = (unsigned char)PRVM_G_FLOAT(OFS_PARM1);
	vm_current_vertices = 0;
	vm_polygonbegin = true;
	if(prog->argc >= 3)
	{
		if(PRVM_G_FLOAT(OFS_PARM2))
			p->flags |= VM_POLYGON_FL2D;
		if(prog->argc >= 4 && PRVM_G_FLOAT(OFS_PARM3))
		{
			p->data[13] = PRVM_G_FLOAT(OFS_PARM3);	//[515]: linewidth
			p->flags |= VM_POLYGON_FLLINES;
		}
	}
}

//void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex
static void VM_CL_R_PolygonVertex (void)
{
	float			*coords, *tx, *rgb, alpha;
	vm_polygon_t	*p;
	VM_SAFEPARMCOUNT(4, VM_CL_R_PolygonVertex);

	if(!vm_polygonbegin)
	{
		VM_Warning("VM_CL_R_PolygonVertex: VM_CL_R_PolygonBegin wasn't called\n");
		return;
	}
	coords	= PRVM_G_VECTOR(OFS_PARM0);
	tx		= PRVM_G_VECTOR(OFS_PARM1);
	rgb		= PRVM_G_VECTOR(OFS_PARM2);
	alpha = PRVM_G_FLOAT(OFS_PARM3);

	p = &vm_polygons[vm_drawpolygons_num];
	if(vm_current_vertices > 4)
	{
		VM_Warning("VM_CL_R_PolygonVertex: may have 4 vertices max\n");
		return;
	}

	p->data[vm_current_vertices*3]		= coords[0];
	p->data[1+vm_current_vertices*3]	= coords[1];
	p->data[2+vm_current_vertices*3]	= coords[2];

	p->data[12+vm_current_vertices*2]	= tx[0];
	if(!(p->flags & VM_POLYGON_FLLINES))
		p->data[13+vm_current_vertices*2]	= tx[1];

	p->data[20+vm_current_vertices*4]	= rgb[0];
	p->data[21+vm_current_vertices*4]	= rgb[1];
	p->data[22+vm_current_vertices*4]	= rgb[2];
	p->data[23+vm_current_vertices*4]	= alpha;

	vm_current_vertices++;
	if(vm_current_vertices == 4)
		p->flags |= VM_POLYGON_FL4V;
	else
		if(vm_current_vertices == 3)
			p->flags |= VM_POLYGON_FL3V;
}

//void() R_EndPolygon
static void VM_CL_R_PolygonEnd (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_R_PolygonEnd);
	if(!vm_polygonbegin)
	{
		VM_Warning("VM_CL_R_PolygonEnd: VM_CL_R_PolygonBegin wasn't called\n");
		return;
	}
	vm_polygonbegin = false;
	if(vm_current_vertices > 2 || (vm_current_vertices >= 2 && vm_polygons[vm_drawpolygons_num].flags & VM_POLYGON_FLLINES))
	{
		if(vm_polygons[vm_drawpolygons_num].flags & VM_POLYGON_FL2D)	//[515]: don't use qcpolygons memory if 2D
			VM_CL_AddPolygonTo2DScene(&vm_polygons[vm_drawpolygons_num]);
		else
			vm_drawpolygons_num++;
	}
	else
		VM_Warning("VM_CL_R_PolygonEnd: %i vertices isn't a good choice\n", vm_current_vertices);
}

void Debug_PolygonBegin(const char *picname, int flags, qboolean draw2d, float linewidth)
{
	vm_polygon_t	*p;

	if(!vm_polygons_initialized)
		VM_InitPolygons();
	if(vm_polygonbegin)
	{
		Con_Printf("Debug_PolygonBegin: called twice without Debug_PolygonEnd after first\n");
		return;
	}
	// limit polygons to a vaguely sane amount, beyond this each one just
	// replaces the last one
	vm_drawpolygons_num = min(vm_drawpolygons_num, (1<<20)-1);
	if(vm_drawpolygons_num >= vm_polygons_num)
	{
		p = (vm_polygon_t *)Mem_Alloc(vm_polygons_pool, 2 * vm_polygons_num * sizeof(vm_polygon_t));
		memset(p, 0, 2 * vm_polygons_num * sizeof(vm_polygon_t));
		memcpy(p, vm_polygons, vm_polygons_num * sizeof(vm_polygon_t));
		Mem_Free(vm_polygons);
		vm_polygons = p;
		vm_polygons_num *= 2;
	}
	p = &vm_polygons[vm_drawpolygons_num];
	if(picname && picname[0])
		p->tex = Draw_CachePic(picname, true)->tex;
	else
		p->tex = r_texture_white;
	p->flags = flags;
	vm_current_vertices = 0;
	vm_polygonbegin = true;
	if(draw2d)
		p->flags |= VM_POLYGON_FL2D;
	if(linewidth)
	{
		p->data[13] = linewidth;	//[515]: linewidth
		p->flags |= VM_POLYGON_FLLINES;
	}
}

void Debug_PolygonVertex(float x, float y, float z, float s, float t, float r, float g, float b, float a)
{
	vm_polygon_t	*p;

	if(!vm_polygonbegin)
	{
		Con_Printf("Debug_PolygonVertex: Debug_PolygonBegin wasn't called\n");
		return;
	}

	p = &vm_polygons[vm_drawpolygons_num];
	if(vm_current_vertices > 4)
	{
		Con_Printf("Debug_PolygonVertex: may have 4 vertices max\n");
		return;
	}

	p->data[vm_current_vertices*3]		= x;
	p->data[1+vm_current_vertices*3]	= y;
	p->data[2+vm_current_vertices*3]	= z;

	p->data[12+vm_current_vertices*2]	= s;
	if(!(p->flags & VM_POLYGON_FLLINES))
		p->data[13+vm_current_vertices*2]	= t;

	p->data[20+vm_current_vertices*4]	= r;
	p->data[21+vm_current_vertices*4]	= g;
	p->data[22+vm_current_vertices*4]	= b;
	p->data[23+vm_current_vertices*4]	= a;

	vm_current_vertices++;
	if(vm_current_vertices == 4)
		p->flags |= VM_POLYGON_FL4V;
	else
		if(vm_current_vertices == 3)
			p->flags |= VM_POLYGON_FL3V;
}

void Debug_PolygonEnd(void)
{
	if(!vm_polygonbegin)
	{
		Con_Printf("Debug_PolygonEnd: Debug_PolygonBegin wasn't called\n");
		return;
	}
	vm_polygonbegin = false;
	if(vm_current_vertices > 2 || (vm_current_vertices >= 2 && vm_polygons[vm_drawpolygons_num].flags & VM_POLYGON_FLLINES))
	{
		if(vm_polygons[vm_drawpolygons_num].flags & VM_POLYGON_FL2D)	//[515]: don't use qcpolygons memory if 2D
			VM_CL_AddPolygonTo2DScene(&vm_polygons[vm_drawpolygons_num]);
		else
			vm_drawpolygons_num++;
	}
	else
		Con_Printf("Debug_PolygonEnd: %i vertices isn't a good choice\n", vm_current_vertices);
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
	trace = CL_Move (start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
	for	(x=0 ; x<=1 ; x++)
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			trace = CL_Move (start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);

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
	int			i;
	prvm_edict_t		*enemy;
	prvm_eval_t	*val;

// try the move
	VectorCopy (ent->fields.client->origin, oldorg);
	VectorAdd (ent->fields.client->origin, move, neworg);

// flying monsters don't step up
	if ( (int)ent->fields.client->flags & (FL_SWIM | FL_FLY) )
	{
	// try one move with vertical motion, then one without
		for (i=0 ; i<2 ; i++)
		{
			VectorAdd (ent->fields.client->origin, move, neworg);
			enemy = PRVM_PROG_TO_EDICT(ent->fields.client->enemy);
			if (i == 0 && enemy != prog->edicts)
			{
				dz = ent->fields.client->origin[2] - PRVM_PROG_TO_EDICT(ent->fields.client->enemy)->fields.client->origin[2];
				if (dz > 40)
					neworg[2] -= 8;
				if (dz < 30)
					neworg[2] += 8;
			}
			trace = CL_Move (ent->fields.client->origin, ent->fields.client->mins, ent->fields.client->maxs, neworg, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);
			if (settrace)
				VM_SetTraceGlobals(&trace);

			if (trace.fraction == 1)
			{
				VectorCopy(trace.endpos, traceendpos);
				if (((int)ent->fields.client->flags & FL_SWIM) && !(CL_PointSuperContents(traceendpos) & SUPERCONTENTS_LIQUIDSMASK))
					return false;	// swim monster left water

				VectorCopy (traceendpos, ent->fields.client->origin);
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

	trace = CL_Move (neworg, ent->fields.client->mins, ent->fields.client->maxs, end, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);
	if (settrace)
		VM_SetTraceGlobals(&trace);

	if (trace.startsolid)
	{
		neworg[2] -= sv_stepheight.value;
		trace = CL_Move (neworg, ent->fields.client->mins, ent->fields.client->maxs, end, MOVE_NORMAL, ent, CL_GenericHitSuperContentsMask(ent), true, true, NULL, true);
		if (settrace)
			VM_SetTraceGlobals(&trace);
		if (trace.startsolid)
			return false;
	}
	if (trace.fraction == 1)
	{
	// if monster had the ground pulled out, go ahead and fall
		if ( (int)ent->fields.client->flags & FL_PARTIALGROUND )
		{
			VectorAdd (ent->fields.client->origin, move, ent->fields.client->origin);
			if (relink)
				CL_LinkEdict(ent);
			ent->fields.client->flags = (int)ent->fields.client->flags & ~FL_ONGROUND;
			return true;
		}

		return false;		// walked off an edge
	}

// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->fields.client->origin);

	if (!CL_CheckBottom (ent))
	{
		if ( (int)ent->fields.client->flags & FL_PARTIALGROUND )
		{	// entity had floor mostly pulled out from underneath it
			// and is trying to correct
			if (relink)
				CL_LinkEdict(ent);
			return true;
		}
		VectorCopy (oldorg, ent->fields.client->origin);
		return false;
	}

	if ( (int)ent->fields.client->flags & FL_PARTIALGROUND )
		ent->fields.client->flags = (int)ent->fields.client->flags & ~FL_PARTIALGROUND;

	if ((val = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.groundentity)))
		val->edict = PRVM_EDICT_TO_PROG(trace.ent);

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

	ent = PRVM_PROG_TO_EDICT(prog->globals.client->self);
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

	if ( !( (int)ent->fields.client->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
		return;

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

// save program state, because CL_movestep may call other progs
	oldf = prog->xfunction;
	oldself = prog->globals.client->self;

	PRVM_G_FLOAT(OFS_RETURN) = CL_movestep(ent, move, true, false, settrace);


// restore program state
	prog->xfunction = oldf;
	prog->globals.client->self = oldself;
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

//============================================================================

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
VM_CL_traceline,				// #16 float(vector v1, vector v2, float tryents) traceline (QUAKE)
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
VM_CL_walkmove,					// #32 float(float yaw, float dist) walkmove (QUAKE)
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
NULL,							// #240
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
NULL,							// #263
NULL,							// #264
NULL,							// #265
NULL,							// #266
NULL,							// #267
NULL,							// #268
NULL,							// #269
NULL,							// #270
NULL,							// #271
NULL,							// #272
NULL,							// #273
NULL,							// #274
NULL,							// #275
NULL,							// #276
NULL,							// #277
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
VM_CL_R_PolygonBegin,			// #306 void(string texturename, float flag[, float is2d, float lines]) R_BeginPolygon
VM_CL_R_PolygonVertex,			// #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex
VM_CL_R_PolygonEnd,				// #308 void() R_EndPolygon
NULL,							// #309
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
NULL,							// #326
NULL,							// #327
NULL,							// #328
NULL,							// #329
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
VM_CL_getkeybind,				// #342 string(float keynum) getkeybind (EXT_CSQC)
VM_CL_setcursormode,			// #343 void(float usecursor) setcursormode (EXT_CSQC)
VM_getmousepos,					// #344 vector() getmousepos (EXT_CSQC)
VM_CL_getinputstate,			// #345 float(float framenum) getinputstate (EXT_CSQC)
VM_CL_setsensitivityscale,		// #346 void(float sens) setsensitivityscaler (EXT_CSQC)
VM_CL_runplayerphysics,			// #347 void() runstandardplayerphysics (EXT_CSQC)
VM_CL_getplayerkey,				// #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
VM_CL_isdemo,					// #349 float() isdemo (EXT_CSQC)
VM_isserver,					// #350 float() isserver (EXT_CSQC)
VM_CL_setlistener,				// #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
VM_CL_registercmd,				// #352 void(string cmdname) registercommand (EXT_CSQC)
VM_wasfreed,					// #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)
VM_CL_serverkey,				// #354 string(string key) serverkey (EXT_CSQC)
NULL,							// #355
NULL,							// #356
NULL,							// #357
NULL,							// #358
NULL,							// #359
VM_CL_ReadByte,					// #360 float() readbyte (EXT_CSQC)
VM_CL_ReadChar,					// #361 float() readchar (EXT_CSQC)
VM_CL_ReadShort,				// #362 float() readshort (EXT_CSQC)
VM_CL_ReadLong,					// #363 float() readlong (EXT_CSQC)
VM_CL_ReadCoord,				// #364 float() readcoord (EXT_CSQC)
VM_CL_ReadAngle,				// #365 float() readangle (EXT_CSQC)
VM_CL_ReadString,				// #366 string() readstring (EXT_CSQC)
VM_CL_ReadFloat,				// #367 float() readfloat (EXT_CSQC)
NULL,							// #368
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
VM_CL_getsurfacenumpoints,		// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
VM_CL_getsurfacepoint,			// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
VM_CL_getsurfacenormal,			// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
VM_CL_getsurfacetexture,		// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
VM_CL_getsurfacenearpoint,		// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
VM_CL_getsurfaceclippedpoint,	// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
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
VM_CL_te_flamejet,				// #457 void(vector org, vector vel, float howmany) te_flamejet = #457 (DP_TE_FLAMEJET)
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
NULL,							// #483
NULL,							// #484
NULL,							// #485
NULL,							// #486
NULL,							// #487
NULL,							// #488
NULL,							// #489
NULL,							// #490
NULL,							// #491
NULL,							// #492
NULL,							// #493
NULL,							// #494
NULL,							// #495
NULL,							// #496
NULL,							// #497
NULL,							// #498
NULL,							// #499
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t);

void VM_CL_Cmd_Init(void)
{
	// TODO: replace vm_polygons stuff with a more general debugging polygon system, and make vm_polygons functions use that system
	if(vm_polygons_initialized)
	{
		Mem_FreePool(&vm_polygons_pool);
		vm_polygons_initialized = false;
	}
}

void VM_CL_Cmd_Reset(void)
{
	if(vm_polygons_initialized)
	{
		Mem_FreePool(&vm_polygons_pool);
		vm_polygons_initialized = false;
	}
}

