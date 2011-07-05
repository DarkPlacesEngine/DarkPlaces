// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin and extension lists can be found here
// cause large (I think they will) parts are from pr_cmds the same copyright like in pr_cmds
// also applies here

#include "quakedef.h"

#include "prvm_cmds.h"
#include "libcurl.h"
#include <time.h>

#include "cl_collision.h"
#include "clvm_cmds.h"
#include "ft2.h"

extern cvar_t prvm_backtraceforwarnings;

// LordHavoc: changed this to NOT use a return statement, so that it can be used in functions that must return a value
void VM_Warning(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];
	static double recursive = -1;

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	Con_DPrint(msg);

	// TODO: either add a cvar/cmd to control the state dumping or replace some of the calls with Con_Printf [9/13/2006 Black]
	if(prvm_backtraceforwarnings.integer && recursive != realtime) // NOTE: this compares to the time, just in case if PRVM_PrintState causes a Host_Error and keeps recursive set
	{
		recursive = realtime;
		PRVM_PrintState();
		recursive = -1;
	}
}


//============================================================================
// Common

// TODO DONE: move vm_files and vm_fssearchlist to prvm_prog_t struct
// TODO: move vm_files and vm_fssearchlist back [9/13/2006 Black]
// TODO: (move vm_files and vm_fssearchlist to prvm_prog_t struct again) [2007-01-23 LordHavoc]
// TODO: will this war ever end? [2007-01-23 LordHavoc]

void VM_CheckEmptyString (const char *s)
{
	if (ISWHITESPACE(s[0]))
		PRVM_ERROR ("%s: Bad string", PRVM_NAME);
}

void VM_GenerateFrameGroupBlend(framegroupblend_t *framegroupblend, const prvm_edict_t *ed)
{
	// self.frame is the interpolation target (new frame)
	// self.frame1time is the animation base time for the interpolation target
	// self.frame2 is the interpolation start (previous frame)
	// self.frame2time is the animation base time for the interpolation start
	// self.lerpfrac is the interpolation strength for self.frame2
	// self.lerpfrac3 is the interpolation strength for self.frame3
	// self.lerpfrac4 is the interpolation strength for self.frame4
	// pitch angle on a player model where the animator set up 5 sets of
	// animations and the csqc simply lerps between sets)
	framegroupblend[0].frame = (int) PRVM_gameedictfloat(ed, frame     );
	framegroupblend[1].frame = (int) PRVM_gameedictfloat(ed, frame2    );
	framegroupblend[2].frame = (int) PRVM_gameedictfloat(ed, frame3    );
	framegroupblend[3].frame = (int) PRVM_gameedictfloat(ed, frame4    );
	framegroupblend[0].start =       PRVM_gameedictfloat(ed, frame1time);
	framegroupblend[1].start =       PRVM_gameedictfloat(ed, frame2time);
	framegroupblend[2].start =       PRVM_gameedictfloat(ed, frame3time);
	framegroupblend[3].start =       PRVM_gameedictfloat(ed, frame4time);
	framegroupblend[1].lerp  =       PRVM_gameedictfloat(ed, lerpfrac  );
	framegroupblend[2].lerp  =       PRVM_gameedictfloat(ed, lerpfrac3 );
	framegroupblend[3].lerp  =       PRVM_gameedictfloat(ed, lerpfrac4 );
	// assume that the (missing) lerpfrac1 is whatever remains after lerpfrac2+lerpfrac3+lerpfrac4 are summed
	framegroupblend[0].lerp = 1 - framegroupblend[1].lerp - framegroupblend[2].lerp - framegroupblend[3].lerp;
}

// LordHavoc: quite tempting to break apart this function to reuse the
//            duplicated code, but I suspect it is better for performance
//            this way
void VM_FrameBlendFromFrameGroupBlend(frameblend_t *frameblend, const framegroupblend_t *framegroupblend, const dp_model_t *model)
{
	int sub2, numframes, f, i, k;
	int isfirstframegroup = true;
	int nolerp;
	double sublerp, lerp, d;
	const animscene_t *scene;
	const framegroupblend_t *g;
	frameblend_t *blend = frameblend;

	memset(blend, 0, MAX_FRAMEBLENDS * sizeof(*blend));

	if (!model || !model->surfmesh.isanimated)
	{
		blend[0].lerp = 1;
		return;
	}

	nolerp = (model->type == mod_sprite) ? !r_lerpsprites.integer : !r_lerpmodels.integer;
	numframes = model->numframes;
	for (k = 0, g = framegroupblend;k < MAX_FRAMEGROUPBLENDS;k++, g++)
	{
		f = g->frame;
		if ((unsigned int)f >= (unsigned int)numframes)
		{
			Con_DPrintf("VM_FrameBlendFromFrameGroupBlend: no such frame %d in model %s\n", f, model->name);
			f = 0;
		}
		d = lerp = g->lerp;
		if (lerp <= 0)
			continue;
		if (nolerp)
		{
			if (isfirstframegroup)
			{
				d = lerp = 1;
				isfirstframegroup = false;
			}
			else
				continue;
		}
		if (model->animscenes)
		{
			scene = model->animscenes + f;
			f = scene->firstframe;
			if (scene->framecount > 1)
			{
				// this code path is only used on .zym models and torches
				sublerp = scene->framerate * (cl.time - g->start);
				f = (int) floor(sublerp);
				sublerp -= f;
				sub2 = f + 1;
				if (sublerp < (1.0 / 65536.0f))
					sublerp = 0;
				if (sublerp > (65535.0f / 65536.0f))
					sublerp = 1;
				if (nolerp)
					sublerp = 0;
				if (scene->loop)
				{
					f = (f % scene->framecount);
					sub2 = (sub2 % scene->framecount);
				}
				f = bound(0, f, (scene->framecount - 1)) + scene->firstframe;
				sub2 = bound(0, sub2, (scene->framecount - 1)) + scene->firstframe;
				d = sublerp * lerp;
				// two framelerps produced from one animation
				if (d > 0)
				{
					for (i = 0;i < MAX_FRAMEBLENDS;i++)
					{
						if (blend[i].lerp <= 0 || blend[i].subframe == sub2)
						{
							blend[i].subframe = sub2;
							blend[i].lerp += d;
							break;
						}
					}
				}
				d = (1 - sublerp) * lerp;
			}
		}
		if (d > 0)
		{
			for (i = 0;i < MAX_FRAMEBLENDS;i++)
			{
				if (blend[i].lerp <= 0 || blend[i].subframe == f)
				{
					blend[i].subframe = f;
					blend[i].lerp += d;
					break;
				}
			}
		}
	}
}

void VM_UpdateEdictSkeleton(prvm_edict_t *ed, const dp_model_t *edmodel, const frameblend_t *frameblend)
{
	if (ed->priv.server->skeleton.model != edmodel)
	{
		VM_RemoveEdictSkeleton(ed);
		ed->priv.server->skeleton.model = edmodel;
	}
	if (!ed->priv.server->skeleton.model || !ed->priv.server->skeleton.model->num_bones)
	{
		if(ed->priv.server->skeleton.relativetransforms)
			Mem_Free(ed->priv.server->skeleton.relativetransforms);
		ed->priv.server->skeleton.relativetransforms = NULL;
		return;
	}

	{
		int skeletonindex = -1;
		skeleton_t *skeleton;
		skeletonindex = (int)PRVM_gameedictfloat(ed, skeletonindex) - 1;
		if (skeletonindex >= 0 && skeletonindex < MAX_EDICTS && (skeleton = prog->skeletons[skeletonindex]) && skeleton->model->num_bones == ed->priv.server->skeleton.model->num_bones)
		{
			// custom skeleton controlled by the game (FTE_CSQC_SKELETONOBJECTS)
			if (!ed->priv.server->skeleton.relativetransforms)
				ed->priv.server->skeleton.relativetransforms = (matrix4x4_t *)Mem_Alloc(prog->progs_mempool, ed->priv.server->skeleton.model->num_bones * sizeof(matrix4x4_t));
			memcpy(ed->priv.server->skeleton.relativetransforms, skeleton->relativetransforms, ed->priv.server->skeleton.model->num_bones * sizeof(matrix4x4_t));
		}
		else
		{
			if(ed->priv.server->skeleton.relativetransforms)
				Mem_Free(ed->priv.server->skeleton.relativetransforms);
			ed->priv.server->skeleton.relativetransforms = NULL;
		}
	}
}

void VM_RemoveEdictSkeleton(prvm_edict_t *ed)
{
	if (ed->priv.server->skeleton.relativetransforms)
		Mem_Free(ed->priv.server->skeleton.relativetransforms);
	memset(&ed->priv.server->skeleton, 0, sizeof(ed->priv.server->skeleton));
}




//============================================================================
//BUILT-IN FUNCTIONS

void VM_VarString(int first, char *out, int outlength)
{
	int i;
	const char *s;
	char *outend;

	outend = out + outlength - 1;
	for (i = first;i < prog->argc && out < outend;i++)
	{
		s = PRVM_G_STRING((OFS_PARM0+i*3));
		while (out < outend && *s)
			*out++ = *s++;
	}
	*out++ = 0;
}

/*
=================
VM_checkextension

returns true if the extension is supported by the server

checkextension(extensionname)
=================
*/

// kind of helper function
static qboolean checkextension(const char *name)
{
	int len;
	const char *e, *start;
	len = (int)strlen(name);

	for (e = prog->extensionstring;*e;e++)
	{
		while (*e == ' ')
			e++;
		if (!*e)
			break;
		start = e;
		while (*e && *e != ' ')
			e++;
		if ((e - start) == len && !strncasecmp(start, name, len))
		{
			// special sheck for ODE
			if (!strncasecmp("DP_PHYSICS_ODE", name, 14))
			{
#ifdef USEODE
				return ode_dll ? true : false;
#else
				return false;
#endif
			}
			return true;
		}
	}
	return false;
}

void VM_checkextension (void)
{
	VM_SAFEPARMCOUNT(1,VM_checkextension);

	PRVM_G_FLOAT(OFS_RETURN) = checkextension(PRVM_G_STRING(OFS_PARM0));
}

/*
=================
VM_error

This is a TERMINAL error, which will kill off the entire prog.
Dumps self.

error(value)
=================
*/
void VM_error (void)
{
	prvm_edict_t	*ed;
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	Con_Printf("======%s ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
	ed = PRVM_PROG_TO_EDICT(PRVM_allglobaledict(self));
	PRVM_ED_Print(ed, NULL);

	PRVM_ERROR ("%s: Program error in function %s:\n%s\nTip: read above for entity information\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
}

/*
=================
VM_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void VM_objerror (void)
{
	prvm_edict_t	*ed;
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	Con_Printf("======OBJECT ERROR======\n"); // , PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string); // or include them? FIXME
	ed = PRVM_PROG_TO_EDICT(PRVM_allglobaledict(self));
	PRVM_ED_Print(ed, NULL);
	PRVM_ED_Free (ed);
	Con_Printf("%s OBJECT ERROR in %s:\n%s\nTip: read above for entity information\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
}

/*
=================
VM_print

print to console

print(...[string])
=================
*/
void VM_print (void)
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	Con_Print(string);
}

/*
=================
VM_bprint

broadcast print to everyone on server

bprint(...[string])
=================
*/
void VM_bprint (void)
{
	char string[VM_STRINGTEMP_LENGTH];

	if(!sv.active)
	{
		VM_Warning("VM_bprint: game is not server(%s) !\n", PRVM_NAME);
		return;
	}

	VM_VarString(0, string, sizeof(string));
	SV_BroadcastPrint(string);
}

/*
=================
VM_sprint (menu & client but only if server.active == true)

single print to a specific client

sprint(float clientnum,...[string])
=================
*/
void VM_sprint (void)
{
	client_t	*client;
	int			clientnum;
	char string[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNTRANGE(1, 8, VM_sprint);

	//find client for this entity
	clientnum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (!sv.active  || clientnum < 0 || clientnum >= svs.maxclients || !svs.clients[clientnum].active)
	{
		VM_Warning("VM_sprint: %s: invalid client or server is not active !\n", PRVM_NAME);
		return;
	}

	client = svs.clients + clientnum;
	if (!client->netconnection)
		return;

	VM_VarString(1, string, sizeof(string));
	MSG_WriteChar(&client->netconnection->message,svc_print);
	MSG_WriteString(&client->netconnection->message, string);
}

/*
=================
VM_centerprint

single print to the screen

centerprint(value)
=================
*/
void VM_centerprint (void)
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNTRANGE(1, 8, VM_centerprint);
	VM_VarString(0, string, sizeof(string));
	SCR_CenterPrint(string);
}

/*
=================
VM_normalize

vector normalize(vector)
=================
*/
void VM_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	double	f;

	VM_SAFEPARMCOUNT(1,VM_normalize);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	f = VectorLength2(value1);
	if (f)
	{
		f = 1.0 / sqrt(f);
		VectorScale(value1, f, newvalue);
	}
	else
		VectorClear(newvalue);

	VectorCopy (newvalue, PRVM_G_VECTOR(OFS_RETURN));
}

/*
=================
VM_vlen

scalar vlen(vector)
=================
*/
void VM_vlen (void)
{
	VM_SAFEPARMCOUNT(1,VM_vlen);
	PRVM_G_FLOAT(OFS_RETURN) = VectorLength(PRVM_G_VECTOR(OFS_PARM0));
}

/*
=================
VM_vectoyaw

float vectoyaw(vector)
=================
*/
void VM_vectoyaw (void)
{
	float	*value1;
	float	yaw;

	VM_SAFEPARMCOUNT(1,VM_vectoyaw);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	PRVM_G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
VM_vectoangles

vector vectoangles(vector[, vector])
=================
*/
void VM_vectoangles (void)
{
	VM_SAFEPARMCOUNTRANGE(1, 2,VM_vectoangles);

	AnglesFromVectors(PRVM_G_VECTOR(OFS_RETURN), PRVM_G_VECTOR(OFS_PARM0), prog->argc >= 2 ? PRVM_G_VECTOR(OFS_PARM1) : NULL, true);
}

/*
=================
VM_random

Returns a number from 0<= num < 1

float random()
=================
*/
void VM_random (void)
{
	VM_SAFEPARMCOUNT(0,VM_random);

	PRVM_G_FLOAT(OFS_RETURN) = lhrandom(0, 1);
}

/*
=========
VM_localsound

localsound(string sample)
=========
*/
void VM_localsound(void)
{
	const char *s;

	VM_SAFEPARMCOUNT(1,VM_localsound);

	s = PRVM_G_STRING(OFS_PARM0);

	if(!S_LocalSound (s))
	{
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		VM_Warning("VM_localsound: Failed to play %s for %s !\n", s, PRVM_NAME);
		return;
	}

	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=================
VM_break

break()
=================
*/
void VM_break (void)
{
	PRVM_ERROR ("%s: break statement", PRVM_NAME);
}

//============================================================================

/*
=================
VM_localcmd

Sends text over to the client's execution buffer

[localcmd (string, ...) or]
cmd (string, ...)
=================
*/
void VM_localcmd (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1, 8, VM_localcmd);
	VM_VarString(0, string, sizeof(string));
	Cbuf_AddText(string);
}

static qboolean PRVM_Cvar_ReadOk(const char *string)
{
	cvar_t *cvar;
	cvar = Cvar_FindVar(string);
	return ((cvar) && ((cvar->flags & CVAR_PRIVATE) == 0));
}

/*
=================
VM_cvar

float cvar (string)
=================
*/
void VM_cvar (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1,8,VM_cvar);
	VM_VarString(0, string, sizeof(string));
	VM_CheckEmptyString(string);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_Cvar_ReadOk(string) ? Cvar_VariableValue(string) : 0;
}

/*
=================
VM_cvar

float cvar_type (string)
float CVAR_TYPEFLAG_EXISTS = 1;
float CVAR_TYPEFLAG_SAVED = 2;
float CVAR_TYPEFLAG_PRIVATE = 4;
float CVAR_TYPEFLAG_ENGINE = 8;
float CVAR_TYPEFLAG_HASDESCRIPTION = 16;
float CVAR_TYPEFLAG_READONLY = 32;
=================
*/
void VM_cvar_type (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	cvar_t *cvar;
	int ret;

	VM_SAFEPARMCOUNTRANGE(1,8,VM_cvar);
	VM_VarString(0, string, sizeof(string));
	VM_CheckEmptyString(string);
	cvar = Cvar_FindVar(string);


	if(!cvar)
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return; // CVAR_TYPE_NONE
	}

	ret = 1; // CVAR_EXISTS
	if(cvar->flags & CVAR_SAVE)
		ret |= 2; // CVAR_TYPE_SAVED
	if(cvar->flags & CVAR_PRIVATE)
		ret |= 4; // CVAR_TYPE_PRIVATE
	if(!(cvar->flags & CVAR_ALLOCATED))
		ret |= 8; // CVAR_TYPE_ENGINE
	if(cvar->description != cvar_dummy_description)
		ret |= 16; // CVAR_TYPE_HASDESCRIPTION
	if(cvar->flags & CVAR_READONLY)
		ret |= 32; // CVAR_TYPE_READONLY
	
	PRVM_G_FLOAT(OFS_RETURN) = ret;
}

/*
=================
VM_cvar_string

const string	VM_cvar_string (string, ...)
=================
*/
void VM_cvar_string(void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1,8,VM_cvar_string);
	VM_VarString(0, string, sizeof(string));
	VM_CheckEmptyString(string);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(PRVM_Cvar_ReadOk(string) ? Cvar_VariableString(string) : "");
}


/*
========================
VM_cvar_defstring

const string	VM_cvar_defstring (string, ...)
========================
*/
void VM_cvar_defstring (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1,8,VM_cvar_defstring);
	VM_VarString(0, string, sizeof(string));
	VM_CheckEmptyString(string);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(Cvar_VariableDefString(string));
}

/*
========================
VM_cvar_defstring

const string	VM_cvar_description (string, ...)
========================
*/
void VM_cvar_description (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1,8,VM_cvar_description);
	VM_VarString(0, string, sizeof(string));
	VM_CheckEmptyString(string);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(Cvar_VariableDescription(string));
}
/*
=================
VM_cvar_set

void cvar_set (string,string, ...)
=================
*/
void VM_cvar_set (void)
{
	const char *name;
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(2,8,VM_cvar_set);
	VM_VarString(1, string, sizeof(string));
	name = PRVM_G_STRING(OFS_PARM0);
	VM_CheckEmptyString(name);
	Cvar_Set(name, string);
}

/*
=========
VM_dprint

dprint(...[string])
=========
*/
void VM_dprint (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1, 8, VM_dprint);
	VM_VarString(0, string, sizeof(string));
#if 1
	Con_DPrintf("%s", string);
#else
	Con_DPrintf("%s: %s", PRVM_NAME, string);
#endif
}

/*
=========
VM_ftos

string	ftos(float)
=========
*/

void VM_ftos (void)
{
	float v;
	char s[128];

	VM_SAFEPARMCOUNT(1, VM_ftos);

	v = PRVM_G_FLOAT(OFS_PARM0);

	if ((float)((int)v) == v)
		dpsnprintf(s, sizeof(s), "%i", (int)v);
	else
		dpsnprintf(s, sizeof(s), "%f", v);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(s);
}

/*
=========
VM_fabs

float	fabs(float)
=========
*/

void VM_fabs (void)
{
	float	v;

	VM_SAFEPARMCOUNT(1,VM_fabs);

	v = PRVM_G_FLOAT(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = fabs(v);
}

/*
=========
VM_vtos

string	vtos(vector)
=========
*/

void VM_vtos (void)
{
	char s[512];

	VM_SAFEPARMCOUNT(1,VM_vtos);

	dpsnprintf (s, sizeof(s), "'%5.1f %5.1f %5.1f'", PRVM_G_VECTOR(OFS_PARM0)[0], PRVM_G_VECTOR(OFS_PARM0)[1], PRVM_G_VECTOR(OFS_PARM0)[2]);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(s);
}

/*
=========
VM_etos

string	etos(entity)
=========
*/

void VM_etos (void)
{
	char s[128];

	VM_SAFEPARMCOUNT(1, VM_etos);

	dpsnprintf (s, sizeof(s), "entity %i", PRVM_G_EDICTNUM(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(s);
}

/*
=========
VM_stof

float stof(...[string])
=========
*/
void VM_stof(void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1, 8, VM_stof);
	VM_VarString(0, string, sizeof(string));
	PRVM_G_FLOAT(OFS_RETURN) = atof(string);
}

/*
========================
VM_itof

float itof(intt ent)
========================
*/
void VM_itof(void)
{
	VM_SAFEPARMCOUNT(1, VM_itof);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
========================
VM_ftoe

entity ftoe(float num)
========================
*/
void VM_ftoe(void)
{
	int ent;
	VM_SAFEPARMCOUNT(1, VM_ftoe);

	ent = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (ent < 0 || ent >= prog->max_edicts || PRVM_PROG_TO_EDICT(ent)->priv.required->free)
		ent = 0; // return world instead of a free or invalid entity

	PRVM_G_INT(OFS_RETURN) = ent;
}

/*
========================
VM_etof

float etof(entity ent)
========================
*/
void VM_etof(void)
{
	VM_SAFEPARMCOUNT(1, VM_etof);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_G_EDICTNUM(OFS_PARM0);
}

/*
=========
VM_strftime

string strftime(float uselocaltime, string[, string ...])
=========
*/
void VM_strftime(void)
{
	time_t t;
#if _MSC_VER >= 1400
	struct tm tm;
	int tmresult;
#else
	struct tm *tm;
#endif
	char fmt[VM_STRINGTEMP_LENGTH];
	char result[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(2, 8, VM_strftime);
	VM_VarString(1, fmt, sizeof(fmt));
	t = time(NULL);
#if _MSC_VER >= 1400
	if (PRVM_G_FLOAT(OFS_PARM0))
		tmresult = localtime_s(&tm, &t);
	else
		tmresult = gmtime_s(&tm, &t);
	if (!tmresult)
#else
	if (PRVM_G_FLOAT(OFS_PARM0))
		tm = localtime(&t);
	else
		tm = gmtime(&t);
	if (!tm)
#endif
	{
		PRVM_G_INT(OFS_RETURN) = 0;
		return;
	}
#if _MSC_VER >= 1400
	strftime(result, sizeof(result), fmt, &tm);
#else
	strftime(result, sizeof(result), fmt, tm);
#endif
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(result);
}

/*
=========
VM_spawn

entity spawn()
=========
*/

void VM_spawn (void)
{
	prvm_edict_t	*ed;
	VM_SAFEPARMCOUNT(0, VM_spawn);
	prog->xfunction->builtinsprofile += 20;
	ed = PRVM_ED_Alloc();
	VM_RETURN_EDICT(ed);
}

/*
=========
VM_remove

remove(entity e)
=========
*/

void VM_remove (void)
{
	prvm_edict_t	*ed;
	prog->xfunction->builtinsprofile += 20;

	VM_SAFEPARMCOUNT(1, VM_remove);

	ed = PRVM_G_EDICT(OFS_PARM0);
	if( PRVM_NUM_FOR_EDICT(ed) <= prog->reserved_edicts )
	{
		if (developer.integer > 0)
			VM_Warning( "VM_remove: tried to remove the null entity or a reserved entity!\n" );
	}
	else if( ed->priv.required->free )
	{
		if (developer.integer > 0)
			VM_Warning( "VM_remove: tried to remove an already freed entity!\n" );
	}
	else
		PRVM_ED_Free (ed);
}

/*
=========
VM_find

entity	find(entity start, .string field, string match)
=========
*/

void VM_find (void)
{
	int		e;
	int		f;
	const char	*s, *t;
	prvm_edict_t	*ed;

	VM_SAFEPARMCOUNT(3,VM_find);

	e = PRVM_G_EDICTNUM(OFS_PARM0);
	f = PRVM_G_INT(OFS_PARM1);
	s = PRVM_G_STRING(OFS_PARM2);

	// LordHavoc: apparently BloodMage does a find(world, weaponmodel, "") and
	// expects it to find all the monsters, so we must be careful to support
	// searching for ""

	for (e++ ; e < prog->num_edicts ; e++)
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if (ed->priv.required->free)
			continue;
		t = PRVM_E_STRING(ed,f);
		if (!t)
			t = "";
		if (!strcmp(t,s))
		{
			VM_RETURN_EDICT(ed);
			return;
		}
	}

	VM_RETURN_EDICT(prog->edicts);
}

/*
=========
VM_findfloat

  entity	findfloat(entity start, .float field, float match)
  entity	findentity(entity start, .entity field, entity match)
=========
*/
// LordHavoc: added this for searching float, int, and entity reference fields
void VM_findfloat (void)
{
	int		e;
	int		f;
	float	s;
	prvm_edict_t	*ed;

	VM_SAFEPARMCOUNT(3,VM_findfloat);

	e = PRVM_G_EDICTNUM(OFS_PARM0);
	f = PRVM_G_INT(OFS_PARM1);
	s = PRVM_G_FLOAT(OFS_PARM2);

	for (e++ ; e < prog->num_edicts ; e++)
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if (ed->priv.required->free)
			continue;
		if (PRVM_E_FLOAT(ed,f) == s)
		{
			VM_RETURN_EDICT(ed);
			return;
		}
	}

	VM_RETURN_EDICT(prog->edicts);
}

/*
=========
VM_findchain

entity	findchain(.string field, string match)
=========
*/
// chained search for strings in entity fields
// entity(.string field, string match) findchain = #402;
void VM_findchain (void)
{
	int		i;
	int		f;
	const char	*s, *t;
	prvm_edict_t	*ent, *chain;
	int chainfield;

	VM_SAFEPARMCOUNTRANGE(2,3,VM_findchain);

	if(prog->argc == 3)
		chainfield = PRVM_G_INT(OFS_PARM2);
	else
		chainfield = prog->fieldoffsets.chain;
	if (chainfield < 0)
		PRVM_ERROR("VM_findchain: %s doesnt have the specified chain field !", PRVM_NAME);

	chain = prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = PRVM_G_STRING(OFS_PARM1);

	// LordHavoc: apparently BloodMage does a find(world, weaponmodel, "") and
	// expects it to find all the monsters, so we must be careful to support
	// searching for ""

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.required->free)
			continue;
		t = PRVM_E_STRING(ent,f);
		if (!t)
			t = "";
		if (strcmp(t,s))
			continue;

		PRVM_EDICTFIELDEDICT(ent,chainfield) = PRVM_NUM_FOR_EDICT(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
=========
VM_findchainfloat

entity	findchainfloat(.string field, float match)
entity	findchainentity(.string field, entity match)
=========
*/
// LordHavoc: chained search for float, int, and entity reference fields
// entity(.string field, float match) findchainfloat = #403;
void VM_findchainfloat (void)
{
	int		i;
	int		f;
	float	s;
	prvm_edict_t	*ent, *chain;
	int chainfield;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_findchainfloat);

	if(prog->argc == 3)
		chainfield = PRVM_G_INT(OFS_PARM2);
	else
		chainfield = prog->fieldoffsets.chain;
	if (chainfield < 0)
		PRVM_ERROR("VM_findchain: %s doesnt have the specified chain field !", PRVM_NAME);

	chain = (prvm_edict_t *)prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = PRVM_G_FLOAT(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.required->free)
			continue;
		if (PRVM_E_FLOAT(ent,f) != s)
			continue;

		PRVM_EDICTFIELDEDICT(ent,chainfield) = PRVM_EDICT_TO_PROG(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
========================
VM_findflags

entity	findflags(entity start, .float field, float match)
========================
*/
// LordHavoc: search for flags in float fields
void VM_findflags (void)
{
	int		e;
	int		f;
	int		s;
	prvm_edict_t	*ed;

	VM_SAFEPARMCOUNT(3, VM_findflags);


	e = PRVM_G_EDICTNUM(OFS_PARM0);
	f = PRVM_G_INT(OFS_PARM1);
	s = (int)PRVM_G_FLOAT(OFS_PARM2);

	for (e++ ; e < prog->num_edicts ; e++)
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if (ed->priv.required->free)
			continue;
		if (!PRVM_E_FLOAT(ed,f))
			continue;
		if ((int)PRVM_E_FLOAT(ed,f) & s)
		{
			VM_RETURN_EDICT(ed);
			return;
		}
	}

	VM_RETURN_EDICT(prog->edicts);
}

/*
========================
VM_findchainflags

entity	findchainflags(.float field, float match)
========================
*/
// LordHavoc: chained search for flags in float fields
void VM_findchainflags (void)
{
	int		i;
	int		f;
	int		s;
	prvm_edict_t	*ent, *chain;
	int chainfield;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_findchainflags);

	if(prog->argc == 3)
		chainfield = PRVM_G_INT(OFS_PARM2);
	else
		chainfield = prog->fieldoffsets.chain;
	if (chainfield < 0)
		PRVM_ERROR("VM_findchain: %s doesnt have the specified chain field !", PRVM_NAME);

	chain = (prvm_edict_t *)prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = (int)PRVM_G_FLOAT(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.required->free)
			continue;
		if (!PRVM_E_FLOAT(ent,f))
			continue;
		if (!((int)PRVM_E_FLOAT(ent,f) & s))
			continue;

		PRVM_EDICTFIELDEDICT(ent,chainfield) = PRVM_EDICT_TO_PROG(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
=========
VM_precache_sound

string	precache_sound (string sample)
=========
*/
void VM_precache_sound (void)
{
	const char *s;

	VM_SAFEPARMCOUNT(1, VM_precache_sound);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
	//VM_CheckEmptyString(s);

	if(snd_initialized.integer && !S_PrecacheSound(s, true, true))
	{
		VM_Warning("VM_precache_sound: Failed to load %s for %s\n", s, PRVM_NAME);
		return;
	}
}

/*
=================
VM_precache_file

returns the same string as output

does nothing, only used by qcc to build .pak archives
=================
*/
void VM_precache_file (void)
{
	VM_SAFEPARMCOUNT(1,VM_precache_file);
	// precache_file is only used to copy files with qcc, it does nothing
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
=========
VM_coredump

coredump()
=========
*/
void VM_coredump (void)
{
	VM_SAFEPARMCOUNT(0,VM_coredump);

	Cbuf_AddText("prvm_edicts ");
	Cbuf_AddText(PRVM_NAME);
	Cbuf_AddText("\n");
}

/*
=========
VM_stackdump

stackdump()
=========
*/
void PRVM_StackTrace(void);
void VM_stackdump (void)
{
	VM_SAFEPARMCOUNT(0, VM_stackdump);

	PRVM_StackTrace();
}

/*
=========
VM_crash

crash()
=========
*/

void VM_crash(void)
{
	VM_SAFEPARMCOUNT(0, VM_crash);

	PRVM_ERROR("Crash called by %s",PRVM_NAME);
}

/*
=========
VM_traceon

traceon()
=========
*/
void VM_traceon (void)
{
	VM_SAFEPARMCOUNT(0,VM_traceon);

	prog->trace = true;
}

/*
=========
VM_traceoff

traceoff()
=========
*/
void VM_traceoff (void)
{
	VM_SAFEPARMCOUNT(0,VM_traceoff);

	prog->trace = false;
}

/*
=========
VM_eprint

eprint(entity e)
=========
*/
void VM_eprint (void)
{
	VM_SAFEPARMCOUNT(1,VM_eprint);

	PRVM_ED_PrintNum (PRVM_G_EDICTNUM(OFS_PARM0), NULL);
}

/*
=========
VM_rint

float	rint(float)
=========
*/
void VM_rint (void)
{
	float f;
	VM_SAFEPARMCOUNT(1,VM_rint);

	f = PRVM_G_FLOAT(OFS_PARM0);
	if (f > 0)
		PRVM_G_FLOAT(OFS_RETURN) = floor(f + 0.5);
	else
		PRVM_G_FLOAT(OFS_RETURN) = ceil(f - 0.5);
}

/*
=========
VM_floor

float	floor(float)
=========
*/
void VM_floor (void)
{
	VM_SAFEPARMCOUNT(1,VM_floor);

	PRVM_G_FLOAT(OFS_RETURN) = floor(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_ceil

float	ceil(float)
=========
*/
void VM_ceil (void)
{
	VM_SAFEPARMCOUNT(1,VM_ceil);

	PRVM_G_FLOAT(OFS_RETURN) = ceil(PRVM_G_FLOAT(OFS_PARM0));
}


/*
=============
VM_nextent

entity	nextent(entity)
=============
*/
void VM_nextent (void)
{
	int		i;
	prvm_edict_t	*ent;

	VM_SAFEPARMCOUNT(1, VM_nextent);

	i = PRVM_G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		prog->xfunction->builtinsprofile++;
		i++;
		if (i == prog->num_edicts)
		{
			VM_RETURN_EDICT(prog->edicts);
			return;
		}
		ent = PRVM_EDICT_NUM(i);
		if (!ent->priv.required->free)
		{
			VM_RETURN_EDICT(ent);
			return;
		}
	}
}

//=============================================================================

/*
==============
VM_changelevel
server and menu

changelevel(string map)
==============
*/
void VM_changelevel (void)
{
	VM_SAFEPARMCOUNT(1, VM_changelevel);

	if(!sv.active)
	{
		VM_Warning("VM_changelevel: game is not server (%s)\n", PRVM_NAME);
		return;
	}

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	Cbuf_AddText (va("changelevel %s\n",PRVM_G_STRING(OFS_PARM0)));
}

/*
=========
VM_sin

float	sin(float)
=========
*/
void VM_sin (void)
{
	VM_SAFEPARMCOUNT(1,VM_sin);
	PRVM_G_FLOAT(OFS_RETURN) = sin(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_cos
float	cos(float)
=========
*/
void VM_cos (void)
{
	VM_SAFEPARMCOUNT(1,VM_cos);
	PRVM_G_FLOAT(OFS_RETURN) = cos(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_sqrt

float	sqrt(float)
=========
*/
void VM_sqrt (void)
{
	VM_SAFEPARMCOUNT(1,VM_sqrt);
	PRVM_G_FLOAT(OFS_RETURN) = sqrt(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_asin

float	asin(float)
=========
*/
void VM_asin (void)
{
	VM_SAFEPARMCOUNT(1,VM_asin);
	PRVM_G_FLOAT(OFS_RETURN) = asin(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_acos
float	acos(float)
=========
*/
void VM_acos (void)
{
	VM_SAFEPARMCOUNT(1,VM_acos);
	PRVM_G_FLOAT(OFS_RETURN) = acos(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_atan
float	atan(float)
=========
*/
void VM_atan (void)
{
	VM_SAFEPARMCOUNT(1,VM_atan);
	PRVM_G_FLOAT(OFS_RETURN) = atan(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_atan2
float	atan2(float,float)
=========
*/
void VM_atan2 (void)
{
	VM_SAFEPARMCOUNT(2,VM_atan2);
	PRVM_G_FLOAT(OFS_RETURN) = atan2(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=========
VM_tan
float	tan(float)
=========
*/
void VM_tan (void)
{
	VM_SAFEPARMCOUNT(1,VM_tan);
	PRVM_G_FLOAT(OFS_RETURN) = tan(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=================
VM_randomvec

Returns a vector of length < 1 and > 0

vector randomvec()
=================
*/
void VM_randomvec (void)
{
	vec3_t		temp;
	//float		length;

	VM_SAFEPARMCOUNT(0, VM_randomvec);

	//// WTF ??
	do
	{
		temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	}
	while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, PRVM_G_VECTOR(OFS_RETURN));

	/*
	temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	// length returned always > 0
	length = (rand()&32766 + 1) * (1.0 / 32767.0) / VectorLength(temp);
	VectorScale(temp,length, temp);*/
	//VectorCopy(temp, PRVM_G_VECTOR(OFS_RETURN));
}

//=============================================================================

/*
=========
VM_registercvar

float	registercvar (string name, string value[, float flags])
=========
*/
void VM_registercvar (void)
{
	const char *name, *value;
	int	flags;

	VM_SAFEPARMCOUNTRANGE(2, 3, VM_registercvar);

	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_STRING(OFS_PARM1);
	flags = prog->argc >= 3 ? (int)PRVM_G_FLOAT(OFS_PARM2) : 0;
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	if(flags > CVAR_MAXFLAGSVAL)
		return;

// first check to see if it has already been defined
	if (Cvar_FindVar (name))
		return;

// check for overlap with a command
	if (Cmd_Exists (name))
	{
		VM_Warning("VM_registercvar: %s is a command\n", name);
		return;
	}

	Cvar_Get(name, value, flags, NULL);

	PRVM_G_FLOAT(OFS_RETURN) = 1; // success
}


/*
=================
VM_min

returns the minimum of two supplied floats

float min(float a, float b, ...[float])
=================
*/
void VM_min (void)
{
	VM_SAFEPARMCOUNTRANGE(2, 8, VM_min);
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (prog->argc >= 3)
	{
		int i;
		float f = PRVM_G_FLOAT(OFS_PARM0);
		for (i = 1;i < prog->argc;i++)
			if (f > PRVM_G_FLOAT((OFS_PARM0+i*3)))
				f = PRVM_G_FLOAT((OFS_PARM0+i*3));
		PRVM_G_FLOAT(OFS_RETURN) = f;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = min(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=================
VM_max

returns the maximum of two supplied floats

float	max(float a, float b, ...[float])
=================
*/
void VM_max (void)
{
	VM_SAFEPARMCOUNTRANGE(2, 8, VM_max);
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (prog->argc >= 3)
	{
		int i;
		float f = PRVM_G_FLOAT(OFS_PARM0);
		for (i = 1;i < prog->argc;i++)
			if (f < PRVM_G_FLOAT((OFS_PARM0+i*3)))
				f = PRVM_G_FLOAT((OFS_PARM0+i*3));
		PRVM_G_FLOAT(OFS_RETURN) = f;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = max(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=================
VM_bound

returns number bounded by supplied range

float	bound(float min, float value, float max)
=================
*/
void VM_bound (void)
{
	VM_SAFEPARMCOUNT(3,VM_bound);
	PRVM_G_FLOAT(OFS_RETURN) = bound(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1), PRVM_G_FLOAT(OFS_PARM2));
}

/*
=================
VM_pow

returns a raised to power b

float	pow(float a, float b)
=================
*/
void VM_pow (void)
{
	VM_SAFEPARMCOUNT(2,VM_pow);
	PRVM_G_FLOAT(OFS_RETURN) = pow(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

void VM_log (void)
{
	VM_SAFEPARMCOUNT(1,VM_log);
	PRVM_G_FLOAT(OFS_RETURN) = log(PRVM_G_FLOAT(OFS_PARM0));
}

void VM_Files_Init(void)
{
	int i;
	for (i = 0;i < PRVM_MAX_OPENFILES;i++)
		prog->openfiles[i] = NULL;
}

void VM_Files_CloseAll(void)
{
	int i;
	for (i = 0;i < PRVM_MAX_OPENFILES;i++)
	{
		if (prog->openfiles[i])
			FS_Close(prog->openfiles[i]);
		prog->openfiles[i] = NULL;
	}
}

static qfile_t *VM_GetFileHandle( int index )
{
	if (index < 0 || index >= PRVM_MAX_OPENFILES)
	{
		Con_Printf("VM_GetFileHandle: invalid file handle %i used in %s\n", index, PRVM_NAME);
		return NULL;
	}
	if (prog->openfiles[index] == NULL)
	{
		Con_Printf("VM_GetFileHandle: no such file handle %i (or file has been closed) in %s\n", index, PRVM_NAME);
		return NULL;
	}
	return prog->openfiles[index];
}

/*
=========
VM_fopen

float	fopen(string filename, float mode)
=========
*/
// float(string filename, float mode) fopen = #110;
// opens a file inside quake/gamedir/data/ (mode is FILE_READ, FILE_APPEND, or FILE_WRITE),
// returns fhandle >= 0 if successful, or fhandle < 0 if unable to open file for any reason
void VM_fopen(void)
{
	int filenum, mode;
	const char *modestring, *filename;

	VM_SAFEPARMCOUNT(2,VM_fopen);

	for (filenum = 0;filenum < PRVM_MAX_OPENFILES;filenum++)
		if (prog->openfiles[filenum] == NULL)
			break;
	if (filenum >= PRVM_MAX_OPENFILES)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_fopen: %s ran out of file handles (%i)\n", PRVM_NAME, PRVM_MAX_OPENFILES);
		return;
	}
	filename = PRVM_G_STRING(OFS_PARM0);
	mode = (int)PRVM_G_FLOAT(OFS_PARM1);
	switch(mode)
	{
	case 0: // FILE_READ
		modestring = "rb";
		prog->openfiles[filenum] = FS_OpenVirtualFile(va("data/%s", filename), false);
		if (prog->openfiles[filenum] == NULL)
			prog->openfiles[filenum] = FS_OpenVirtualFile(va("%s", filename), false);
		break;
	case 1: // FILE_APPEND
		modestring = "a";
		prog->openfiles[filenum] = FS_OpenRealFile(va("data/%s", filename), modestring, false);
		break;
	case 2: // FILE_WRITE
		modestring = "w";
		prog->openfiles[filenum] = FS_OpenRealFile(va("data/%s", filename), modestring, false);
		break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning("VM_fopen: %s: no such mode %i (valid: 0 = read, 1 = append, 2 = write)\n", PRVM_NAME, mode);
		return;
	}

	if (prog->openfiles[filenum] == NULL)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		if (developer_extra.integer)
			VM_Warning("VM_fopen: %s: %s mode %s failed\n", PRVM_NAME, filename, modestring);
	}
	else
	{
		PRVM_G_FLOAT(OFS_RETURN) = filenum;
		if (developer_extra.integer)
			Con_DPrintf("VM_fopen: %s: %s mode %s opened as #%i\n", PRVM_NAME, filename, modestring, filenum);
		prog->openfiles_origin[filenum] = PRVM_AllocationOrigin();
	}
}

/*
=========
VM_fclose

fclose(float fhandle)
=========
*/
//void(float fhandle) fclose = #111; // closes a file
void VM_fclose(void)
{
	int filenum;

	VM_SAFEPARMCOUNT(1,VM_fclose);

	filenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= PRVM_MAX_OPENFILES)
	{
		VM_Warning("VM_fclose: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (prog->openfiles[filenum] == NULL)
	{
		VM_Warning("VM_fclose: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	FS_Close(prog->openfiles[filenum]);
	prog->openfiles[filenum] = NULL;
	if(prog->openfiles_origin[filenum])
		PRVM_Free((char *)prog->openfiles_origin[filenum]);
	if (developer_extra.integer)
		Con_DPrintf("VM_fclose: %s: #%i closed\n", PRVM_NAME, filenum);
}

/*
=========
VM_fgets

string	fgets(float fhandle)
=========
*/
//string(float fhandle) fgets = #112; // reads a line of text from the file and returns as a tempstring
void VM_fgets(void)
{
	int c, end;
	char string[VM_STRINGTEMP_LENGTH];
	int filenum;

	VM_SAFEPARMCOUNT(1,VM_fgets);

	// set the return value regardless of any possible errors
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;

	filenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= PRVM_MAX_OPENFILES)
	{
		VM_Warning("VM_fgets: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (prog->openfiles[filenum] == NULL)
	{
		VM_Warning("VM_fgets: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	end = 0;
	for (;;)
	{
		c = FS_Getc(prog->openfiles[filenum]);
		if (c == '\r' || c == '\n' || c < 0)
			break;
		if (end < VM_STRINGTEMP_LENGTH - 1)
			string[end++] = c;
	}
	string[end] = 0;
	// remove \n following \r
	if (c == '\r')
	{
		c = FS_Getc(prog->openfiles[filenum]);
		if (c != '\n')
			FS_UnGetc(prog->openfiles[filenum], (unsigned char)c);
	}
	if (developer_extra.integer)
		Con_DPrintf("fgets: %s: %s\n", PRVM_NAME, string);
	if (c >= 0 || end)
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
}

/*
=========
VM_fputs

fputs(float fhandle, string s)
=========
*/
//void(float fhandle, string s) fputs = #113; // writes a line of text to the end of the file
void VM_fputs(void)
{
	int stringlength;
	char string[VM_STRINGTEMP_LENGTH];
	int filenum;

	VM_SAFEPARMCOUNT(2,VM_fputs);

	filenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= PRVM_MAX_OPENFILES)
	{
		VM_Warning("VM_fputs: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (prog->openfiles[filenum] == NULL)
	{
		VM_Warning("VM_fputs: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	VM_VarString(1, string, sizeof(string));
	if ((stringlength = (int)strlen(string)))
		FS_Write(prog->openfiles[filenum], string, stringlength);
	if (developer_extra.integer)
		Con_DPrintf("fputs: %s: %s\n", PRVM_NAME, string);
}

/*
=========
VM_writetofile

	writetofile(float fhandle, entity ent)
=========
*/
void VM_writetofile(void)
{
	prvm_edict_t * ent;
	qfile_t *file;

	VM_SAFEPARMCOUNT(2, VM_writetofile);

	file = VM_GetFileHandle( (int)PRVM_G_FLOAT(OFS_PARM0) );
	if( !file )
	{
		VM_Warning("VM_writetofile: invalid or closed file handle\n");
		return;
	}

	ent = PRVM_G_EDICT(OFS_PARM1);
	if(ent->priv.required->free)
	{
		VM_Warning("VM_writetofile: %s: entity %i is free !\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ent));
		return;
	}

	PRVM_ED_Write (file, ent);
}

// KrimZon - DP_QC_ENTITYDATA
/*
=========
VM_numentityfields

float() numentityfields
Return the number of entity fields - NOT offsets
=========
*/
void VM_numentityfields(void)
{
	PRVM_G_FLOAT(OFS_RETURN) = prog->numfielddefs;
}

// KrimZon - DP_QC_ENTITYDATA
/*
=========
VM_entityfieldname

string(float fieldnum) entityfieldname
Return name of the specified field as a string, or empty if the field is invalid (warning)
=========
*/
void VM_entityfieldname(void)
{
	ddef_t *d;
	int i = (int)PRVM_G_FLOAT(OFS_PARM0);
	
	if (i < 0 || i >= prog->numfielddefs)
	{
        VM_Warning("VM_entityfieldname: %s: field index out of bounds\n", PRVM_NAME);
        PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString("");
		return;
	}
	
	d = &prog->fielddefs[i];
	PRVM_G_INT(OFS_RETURN) = d->s_name; // presuming that s_name points to a string already
}

// KrimZon - DP_QC_ENTITYDATA
/*
=========
VM_entityfieldtype

float(float fieldnum) entityfieldtype
=========
*/
void VM_entityfieldtype(void)
{
	ddef_t *d;
	int i = (int)PRVM_G_FLOAT(OFS_PARM0);
	
	if (i < 0 || i >= prog->numfielddefs)
	{
		VM_Warning("VM_entityfieldtype: %s: field index out of bounds\n", PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = -1.0;
		return;
	}
	
	d = &prog->fielddefs[i];
	PRVM_G_FLOAT(OFS_RETURN) = (float)d->type;
}

// KrimZon - DP_QC_ENTITYDATA
/*
=========
VM_getentityfieldstring

string(float fieldnum, entity ent) getentityfieldstring
=========
*/
void VM_getentityfieldstring(void)
{
	// put the data into a string
	ddef_t *d;
	int type, j;
	int *v;
	prvm_edict_t * ent;
	int i = (int)PRVM_G_FLOAT(OFS_PARM0);
	
	if (i < 0 || i >= prog->numfielddefs)
	{
        VM_Warning("VM_entityfielddata: %s: field index out of bounds\n", PRVM_NAME);
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString("");
		return;
	}
	
	d = &prog->fielddefs[i];
	
	// get the entity
	ent = PRVM_G_EDICT(OFS_PARM1);
	if(ent->priv.required->free)
	{
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString("");
		VM_Warning("VM_entityfielddata: %s: entity %i is free !\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ent));
		return;
	}
	v = (int *)((char *)ent->fields.vp + d->ofs*4);
	
	// if it's 0 or blank, return an empty string
	type = d->type & ~DEF_SAVEGLOBAL;
	for (j=0 ; j<prvm_type_size[type] ; j++)
		if (v[j])
			break;
	if (j == prvm_type_size[type])
	{
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString("");
		return;
	}
		
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(PRVM_UglyValueString((etype_t)d->type, (prvm_eval_t *)v));
}

// KrimZon - DP_QC_ENTITYDATA
/*
=========
VM_putentityfieldstring

float(float fieldnum, entity ent, string s) putentityfieldstring
=========
*/
void VM_putentityfieldstring(void)
{
	ddef_t *d;
	prvm_edict_t * ent;
	int i = (int)PRVM_G_FLOAT(OFS_PARM0);

	if (i < 0 || i >= prog->numfielddefs)
	{
        VM_Warning("VM_entityfielddata: %s: field index out of bounds\n", PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = 0.0f;
		return;
	}

	d = &prog->fielddefs[i];

	// get the entity
	ent = PRVM_G_EDICT(OFS_PARM1);
	if(ent->priv.required->free)
	{
		VM_Warning("VM_entityfielddata: %s: entity %i is free !\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ent));
		PRVM_G_FLOAT(OFS_RETURN) = 0.0f;
		return;
	}

	// parse the string into the value
	PRVM_G_FLOAT(OFS_RETURN) = ( PRVM_ED_ParseEpair(ent, d, PRVM_G_STRING(OFS_PARM2), false) ) ? 1.0f : 0.0f;
}

/*
=========
VM_strlen

float	strlen(string s)
=========
*/
//float(string s) strlen = #114; // returns how many characters are in a string
void VM_strlen(void)
{
	VM_SAFEPARMCOUNT(1,VM_strlen);

	//PRVM_G_FLOAT(OFS_RETURN) = strlen(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = u8_strlen(PRVM_G_STRING(OFS_PARM0));
}

// DRESK - Decolorized String
/*
=========
VM_strdecolorize

string	strdecolorize(string s)
=========
*/
// string (string s) strdecolorize = #472; // returns the passed in string with color codes stripped
void VM_strdecolorize(void)
{
	char szNewString[VM_STRINGTEMP_LENGTH];
	const char *szString;

	// Prepare Strings
	VM_SAFEPARMCOUNT(1,VM_strdecolorize);
	szString = PRVM_G_STRING(OFS_PARM0);
	COM_StringDecolorize(szString, 0, szNewString, sizeof(szNewString), TRUE);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(szNewString);
}

// DRESK - String Length (not counting color codes)
/*
=========
VM_strlennocol

float	strlennocol(string s)
=========
*/
// float(string s) strlennocol = #471; // returns how many characters are in a string not including color codes
// For example, ^2Dresk returns a length of 5
void VM_strlennocol(void)
{
	const char *szString;
	int nCnt;

	VM_SAFEPARMCOUNT(1,VM_strlennocol);

	szString = PRVM_G_STRING(OFS_PARM0);

	//nCnt = COM_StringLengthNoColors(szString, 0, NULL);
	nCnt = u8_COM_StringLengthNoColors(szString, 0, NULL);

	PRVM_G_FLOAT(OFS_RETURN) = nCnt;
}

// DRESK - String to Uppercase and Lowercase
/*
=========
VM_strtolower

string	strtolower(string s)
=========
*/
// string (string s) strtolower = #480; // returns passed in string in lowercase form
void VM_strtolower(void)
{
	char szNewString[VM_STRINGTEMP_LENGTH];
	const char *szString;

	// Prepare Strings
	VM_SAFEPARMCOUNT(1,VM_strtolower);
	szString = PRVM_G_STRING(OFS_PARM0);

	COM_ToLowerString(szString, szNewString, sizeof(szNewString) );

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(szNewString);
}

/*
=========
VM_strtoupper

string	strtoupper(string s)
=========
*/
// string (string s) strtoupper = #481; // returns passed in string in uppercase form
void VM_strtoupper(void)
{
	char szNewString[VM_STRINGTEMP_LENGTH];
	const char *szString;

	// Prepare Strings
	VM_SAFEPARMCOUNT(1,VM_strtoupper);
	szString = PRVM_G_STRING(OFS_PARM0);

	COM_ToUpperString(szString, szNewString, sizeof(szNewString) );

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(szNewString);
}

/*
=========
VM_strcat

string strcat(string,string,...[string])
=========
*/
//string(string s1, string s2) strcat = #115;
// concatenates two strings (for example "abc", "def" would return "abcdef")
// and returns as a tempstring
void VM_strcat(void)
{
	char s[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(1, 8, VM_strcat);

	VM_VarString(0, s, sizeof(s));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(s);
}

/*
=========
VM_substring

string	substring(string s, float start, float length)
=========
*/
// string(string s, float start, float length) substring = #116;
// returns a section of a string as a tempstring
void VM_substring(void)
{
	int start, length;
	int u_slength = 0, u_start;
	size_t u_length;
	const char *s;
	char string[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT(3,VM_substring);

	/*
	s = PRVM_G_STRING(OFS_PARM0);
	start = (int)PRVM_G_FLOAT(OFS_PARM1);
	length = (int)PRVM_G_FLOAT(OFS_PARM2);
	slength = strlen(s);

	if (start < 0) // FTE_STRINGS feature
		start += slength;
	start = bound(0, start, slength);

	if (length < 0) // FTE_STRINGS feature
		length += slength - start + 1;
	maxlen = min((int)sizeof(string) - 1, slength - start);
	length = bound(0, length, maxlen);

	memcpy(string, s + start, length);
	string[length] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
	*/
	
	s = PRVM_G_STRING(OFS_PARM0);
	start = (int)PRVM_G_FLOAT(OFS_PARM1);
	length = (int)PRVM_G_FLOAT(OFS_PARM2);

	if (start < 0) // FTE_STRINGS feature
	{
		u_slength = u8_strlen(s);
		start += u_slength;
		start = bound(0, start, u_slength);
	}

	if (length < 0) // FTE_STRINGS feature
	{
		if (!u_slength) // it's not calculated when it's not needed above
			u_slength = u8_strlen(s);
		length += u_slength - start + 1;
	}
		
	// positive start, positive length
	u_start = u8_byteofs(s, start, NULL);
	if (u_start < 0)
	{
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString("");
		return;
	}
	u_length = u8_bytelen(s + u_start, length);
	if (u_length >= sizeof(string)-1)
		u_length = sizeof(string)-1;
	
	memcpy(string, s + u_start, u_length);
	string[u_length] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
}

/*
=========
VM_strreplace

string(string search, string replace, string subject) strreplace = #484;
=========
*/
// replaces all occurrences of search with replace in the string subject, and returns the result
void VM_strreplace(void)
{
	int i, j, si;
	const char *search, *replace, *subject;
	char string[VM_STRINGTEMP_LENGTH];
	int search_len, replace_len, subject_len;

	VM_SAFEPARMCOUNT(3,VM_strreplace);

	search = PRVM_G_STRING(OFS_PARM0);
	replace = PRVM_G_STRING(OFS_PARM1);
	subject = PRVM_G_STRING(OFS_PARM2);

	search_len = (int)strlen(search);
	replace_len = (int)strlen(replace);
	subject_len = (int)strlen(subject);

	si = 0;
	for (i = 0; i <= subject_len - search_len; i++)
	{
		for (j = 0; j < search_len; j++) // thus, i+j < subject_len
			if (subject[i+j] != search[j])
				break;
		if (j == search_len)
		{
			// NOTE: if search_len == 0, we always hit THIS case, and never the other
			// found it at offset 'i'
			for (j = 0; j < replace_len && si < (int)sizeof(string) - 1; j++)
				string[si++] = replace[j];
			if(search_len > 0)
			{
				i += search_len - 1;
			}
			else
			{
				// the above would subtract 1 from i... so we
				// don't do that, but instead output the next
				// char
				if (si < (int)sizeof(string) - 1)
					string[si++] = subject[i];
			}
		}
		else
		{
			// in THIS case, we know search_len > 0, thus i < subject_len
			// not found
			if (si < (int)sizeof(string) - 1)
				string[si++] = subject[i];
		}
	}
	// remaining chars (these cannot match)
	for (; i < subject_len; i++)
		if (si < (int)sizeof(string) - 1)
			string[si++] = subject[i];
	string[si] = '\0';

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
}

/*
=========
VM_strireplace

string(string search, string replace, string subject) strireplace = #485;
=========
*/
// case-insensitive version of strreplace
void VM_strireplace(void)
{
	int i, j, si;
	const char *search, *replace, *subject;
	char string[VM_STRINGTEMP_LENGTH];
	int search_len, replace_len, subject_len;

	VM_SAFEPARMCOUNT(3,VM_strreplace);

	search = PRVM_G_STRING(OFS_PARM0);
	replace = PRVM_G_STRING(OFS_PARM1);
	subject = PRVM_G_STRING(OFS_PARM2);

	search_len = (int)strlen(search);
	replace_len = (int)strlen(replace);
	subject_len = (int)strlen(subject);

	si = 0;
	for (i = 0; i <= subject_len - search_len; i++)
	{
		for (j = 0; j < search_len; j++) // thus, i+j < subject_len
			if (tolower(subject[i+j]) != tolower(search[j]))
				break;
		if (j == search_len)
		{
			// NOTE: if search_len == 0, we always hit THIS case, and never the other
			// found it at offset 'i'
			for (j = 0; j < replace_len && si < (int)sizeof(string) - 1; j++)
				string[si++] = replace[j];
			if(search_len > 0)
			{
				i += search_len - 1;
			}
			else
			{
				// the above would subtract 1 from i... so we
				// don't do that, but instead output the next
				// char
				if (si < (int)sizeof(string) - 1)
					string[si++] = subject[i];
			}
		}
		else
		{
			// in THIS case, we know search_len > 0, thus i < subject_len
			// not found
			if (si < (int)sizeof(string) - 1)
				string[si++] = subject[i];
		}
	}
	// remaining chars (these cannot match)
	for (; i < subject_len; i++)
		if (si < (int)sizeof(string) - 1)
			string[si++] = subject[i];
	string[si] = '\0';

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
}

/*
=========
VM_stov

vector	stov(string s)
=========
*/
//vector(string s) stov = #117; // returns vector value from a string
void VM_stov(void)
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT(1,VM_stov);

	VM_VarString(0, string, sizeof(string));
	Math_atov(string, PRVM_G_VECTOR(OFS_RETURN));
}

/*
=========
VM_strzone

string	strzone(string s)
=========
*/
//string(string s, ...) strzone = #118; // makes a copy of a string into the string zone and returns it, this is often used to keep around a tempstring for longer periods of time (tempstrings are replaced often)
void VM_strzone(void)
{
	char *out;
	char string[VM_STRINGTEMP_LENGTH];
	size_t alloclen;

	VM_SAFEPARMCOUNT(1,VM_strzone);

	VM_VarString(0, string, sizeof(string));
	alloclen = strlen(string) + 1;
	PRVM_G_INT(OFS_RETURN) = PRVM_AllocString(alloclen, &out);
	memcpy(out, string, alloclen);
}

/*
=========
VM_strunzone

strunzone(string s)
=========
*/
//void(string s) strunzone = #119; // removes a copy of a string from the string zone (you can not use that string again or it may crash!!!)
void VM_strunzone(void)
{
	VM_SAFEPARMCOUNT(1,VM_strunzone);
	PRVM_FreeString(PRVM_G_INT(OFS_PARM0));
}

/*
=========
VM_command (used by client and menu)

clientcommand(float client, string s) (for client and menu)
=========
*/
//void(entity e, string s) clientcommand = #440; // executes a command string as if it came from the specified client
//this function originally written by KrimZon, made shorter by LordHavoc
void VM_clcommand (void)
{
	client_t *temp_client;
	int i;

	VM_SAFEPARMCOUNT(2,VM_clcommand);

	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (!sv.active  || i < 0 || i >= svs.maxclients || !svs.clients[i].active)
	{
		VM_Warning("VM_clientcommand: %s: invalid client/server is not active !\n", PRVM_NAME);
		return;
	}

	temp_client = host_client;
	host_client = svs.clients + i;
	Cmd_ExecuteString (PRVM_G_STRING(OFS_PARM1), src_client);
	host_client = temp_client;
}


/*
=========
VM_tokenize

float tokenize(string s)
=========
*/
//float(string s) tokenize = #441; // takes apart a string into individal words (access them with argv), returns how many
//this function originally written by KrimZon, made shorter by LordHavoc
//20040203: rewritten by LordHavoc (no longer uses allocations)
static int num_tokens = 0;
static int tokens[VM_STRINGTEMP_LENGTH / 2];
static int tokens_startpos[VM_STRINGTEMP_LENGTH / 2];
static int tokens_endpos[VM_STRINGTEMP_LENGTH / 2];
static char tokenize_string[VM_STRINGTEMP_LENGTH];
void VM_tokenize (void)
{
	const char *p;

	VM_SAFEPARMCOUNT(1,VM_tokenize);

	strlcpy(tokenize_string, PRVM_G_STRING(OFS_PARM0), sizeof(tokenize_string));
	p = tokenize_string;

	num_tokens = 0;
	for(;;)
	{
		if (num_tokens >= (int)(sizeof(tokens)/sizeof(tokens[0])))
			break;

		// skip whitespace here to find token start pos
		while(*p && ISWHITESPACE(*p))
			++p;

		tokens_startpos[num_tokens] = p - tokenize_string;
		if(!COM_ParseToken_VM_Tokenize(&p, false))
			break;
		tokens_endpos[num_tokens] = p - tokenize_string;
		tokens[num_tokens] = PRVM_SetTempString(com_token);
		++num_tokens;
	}

	PRVM_G_FLOAT(OFS_RETURN) = num_tokens;
}

//float(string s) tokenize = #514; // takes apart a string into individal words (access them with argv), returns how many
void VM_tokenize_console (void)
{
	const char *p;

	VM_SAFEPARMCOUNT(1,VM_tokenize);

	strlcpy(tokenize_string, PRVM_G_STRING(OFS_PARM0), sizeof(tokenize_string));
	p = tokenize_string;

	num_tokens = 0;
	for(;;)
	{
		if (num_tokens >= (int)(sizeof(tokens)/sizeof(tokens[0])))
			break;

		// skip whitespace here to find token start pos
		while(*p && ISWHITESPACE(*p))
			++p;

		tokens_startpos[num_tokens] = p - tokenize_string;
		if(!COM_ParseToken_Console(&p))
			break;
		tokens_endpos[num_tokens] = p - tokenize_string;
		tokens[num_tokens] = PRVM_SetTempString(com_token);
		++num_tokens;
	}

	PRVM_G_FLOAT(OFS_RETURN) = num_tokens;
}

/*
=========
VM_tokenizebyseparator

float tokenizebyseparator(string s, string separator1, ...)
=========
*/
//float(string s, string separator1, ...) tokenizebyseparator = #479; // takes apart a string into individal words (access them with argv), returns how many
//this function returns the token preceding each instance of a separator (of
//which there can be multiple), and the text following the last separator
//useful for parsing certain kinds of data like IP addresses
//example:
//numnumbers = tokenizebyseparator("10.1.2.3", ".");
//returns 4 and the tokens "10" "1" "2" "3".
void VM_tokenizebyseparator (void)
{
	int j, k;
	int numseparators;
	int separatorlen[7];
	const char *separators[7];
	const char *p, *p0;
	const char *token;
	char tokentext[MAX_INPUTLINE];

	VM_SAFEPARMCOUNTRANGE(2, 8,VM_tokenizebyseparator);

	strlcpy(tokenize_string, PRVM_G_STRING(OFS_PARM0), sizeof(tokenize_string));
	p = tokenize_string;

	numseparators = 0;
	for (j = 1;j < prog->argc;j++)
	{
		// skip any blank separator strings
		const char *s = PRVM_G_STRING(OFS_PARM0+j*3);
		if (!s[0])
			continue;
		separators[numseparators] = s;
		separatorlen[numseparators] = strlen(s);
		numseparators++;
	}

	num_tokens = 0;
	j = 0;

	while (num_tokens < (int)(sizeof(tokens)/sizeof(tokens[0])))
	{
		token = tokentext + j;
		tokens_startpos[num_tokens] = p - tokenize_string;
		p0 = p;
		while (*p)
		{
			for (k = 0;k < numseparators;k++)
			{
				if (!strncmp(p, separators[k], separatorlen[k]))
				{
					p += separatorlen[k];
					break;
				}
			}
			if (k < numseparators)
				break;
			if (j < (int)sizeof(tokentext)-1)
				tokentext[j++] = *p;
			p++;
			p0 = p;
		}
		tokens_endpos[num_tokens] = p0 - tokenize_string;
		if (j >= (int)sizeof(tokentext))
			break;
		tokentext[j++] = 0;
		tokens[num_tokens++] = PRVM_SetTempString(token);
		if (!*p)
			break;
	}

	PRVM_G_FLOAT(OFS_RETURN) = num_tokens;
}

//string(float n) argv = #442; // returns a word from the tokenized string (returns nothing for an invalid index)
//this function originally written by KrimZon, made shorter by LordHavoc
void VM_argv (void)
{
	int token_num;

	VM_SAFEPARMCOUNT(1,VM_argv);

	token_num = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(token_num < 0)
		token_num += num_tokens;

	if (token_num >= 0 && token_num < num_tokens)
		PRVM_G_INT(OFS_RETURN) = tokens[token_num];
	else
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
}

//float(float n) argv_start_index = #515; // returns the start index of a token
void VM_argv_start_index (void)
{
	int token_num;

	VM_SAFEPARMCOUNT(1,VM_argv);

	token_num = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(token_num < 0)
		token_num += num_tokens;

	if (token_num >= 0 && token_num < num_tokens)
		PRVM_G_FLOAT(OFS_RETURN) = tokens_startpos[token_num];
	else
		PRVM_G_FLOAT(OFS_RETURN) = -1;
}

//float(float n) argv_end_index = #516; // returns the end index of a token
void VM_argv_end_index (void)
{
	int token_num;

	VM_SAFEPARMCOUNT(1,VM_argv);

	token_num = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(token_num < 0)
		token_num += num_tokens;

	if (token_num >= 0 && token_num < num_tokens)
		PRVM_G_FLOAT(OFS_RETURN) = tokens_endpos[token_num];
	else
		PRVM_G_FLOAT(OFS_RETURN) = -1;
}

/*
=========
VM_isserver

float	isserver()
=========
*/
void VM_isserver(void)
{
	VM_SAFEPARMCOUNT(0,VM_serverstate);

	PRVM_G_FLOAT(OFS_RETURN) = sv.active && (svs.maxclients > 1 || cls.state == ca_dedicated);
}

/*
=========
VM_clientcount

float	clientcount()
=========
*/
void VM_clientcount(void)
{
	VM_SAFEPARMCOUNT(0,VM_clientcount);

	PRVM_G_FLOAT(OFS_RETURN) = svs.maxclients;
}

/*
=========
VM_clientstate

float	clientstate()
=========
*/
void VM_clientstate(void)
{
	VM_SAFEPARMCOUNT(0,VM_clientstate);


	switch( cls.state ) {
		case ca_uninitialized:
		case ca_dedicated:
			PRVM_G_FLOAT(OFS_RETURN) = 0;
			break;
		case ca_disconnected:
			PRVM_G_FLOAT(OFS_RETURN) = 1;
			break;
		case ca_connected:
			PRVM_G_FLOAT(OFS_RETURN) = 2;
			break;
		default:
			// should never be reached!
			break;
	}
}

/*
=========
VM_getostype

float	getostype(void)
=========
*/ // not used at the moment -> not included in the common list
void VM_getostype(void)
{
	VM_SAFEPARMCOUNT(0,VM_getostype);

	/*
	OS_WINDOWS
	OS_LINUX
	OS_MAC - not supported
	*/

#ifdef WIN32
	PRVM_G_FLOAT(OFS_RETURN) = 0;
#elif defined(MACOSX)
	PRVM_G_FLOAT(OFS_RETURN) = 2;
#else
	PRVM_G_FLOAT(OFS_RETURN) = 1;
#endif
}

/*
=========
VM_gettime

float	gettime(void)
=========
*/
extern double host_starttime;
float CDAudio_GetPosition(void);
void VM_gettime(void)
{
	int timer_index;

	VM_SAFEPARMCOUNTRANGE(0,1,VM_gettime);

	if(prog->argc == 0)
	{
		PRVM_G_FLOAT(OFS_RETURN) = (float) realtime;
	}
	else
	{
		timer_index = (int) PRVM_G_FLOAT(OFS_PARM0);
        switch(timer_index)
        {
            case 0: // GETTIME_FRAMESTART
                PRVM_G_FLOAT(OFS_RETURN) = (float) realtime;
                break;
            case 1: // GETTIME_REALTIME
                PRVM_G_FLOAT(OFS_RETURN) = (float) Sys_DoubleTime();
                break;
            case 2: // GETTIME_HIRES
                PRVM_G_FLOAT(OFS_RETURN) = (float) (Sys_DoubleTime() - realtime);
                break;
            case 3: // GETTIME_UPTIME
                PRVM_G_FLOAT(OFS_RETURN) = (float) (Sys_DoubleTime() - host_starttime);
                break;
            case 4: // GETTIME_CDTRACK
                PRVM_G_FLOAT(OFS_RETURN) = (float) CDAudio_GetPosition();
                break;
			default:
				VM_Warning("VM_gettime: %s: unsupported timer specified, returning realtime\n", PRVM_NAME);
				PRVM_G_FLOAT(OFS_RETURN) = (float) realtime;
				break;
		}
	}
}

/*
=========
VM_getsoundtime

float	getsoundtime(void)
=========
*/

void VM_getsoundtime (void)
{
	int entnum, entchannel, pnum;
	VM_SAFEPARMCOUNT(2,VM_getsoundtime);

	pnum = PRVM_GetProgNr();
	if (pnum == PRVM_MENUPROG)
	{
		VM_Warning("VM_getsoundtime: %s: not supported on this progs\n", PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	entnum = ((pnum == PRVM_CLIENTPROG) ? MAX_EDICTS : 0) + PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(OFS_PARM0));
	entchannel = (int)PRVM_G_FLOAT(OFS_PARM1);
	entchannel = CHAN_USER2ENGINE(entchannel);
	if (!IS_CHAN(entchannel))
		VM_Warning("VM_getsoundtime: %s: bad channel %i\n", PRVM_NAME, entchannel);
	PRVM_G_FLOAT(OFS_RETURN) = (float)S_GetEntChannelPosition(entnum, entchannel);
}

/*
=========
VM_GetSoundLen

string	soundlength (string sample)
=========
*/
void VM_soundlength (void)
{
	const char *s;

	VM_SAFEPARMCOUNT(1, VM_soundlength);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = S_SoundLength(s);
}

/*
=========
VM_loadfromdata

loadfromdata(string data)
=========
*/
void VM_loadfromdata(void)
{
	VM_SAFEPARMCOUNT(1,VM_loadentsfromfile);

	PRVM_ED_LoadFromFile(PRVM_G_STRING(OFS_PARM0));
}

/*
========================
VM_parseentitydata

parseentitydata(entity ent, string data)
========================
*/
void VM_parseentitydata(void)
{
	prvm_edict_t *ent;
	const char *data;

	VM_SAFEPARMCOUNT(2, VM_parseentitydata);

	// get edict and test it
	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent->priv.required->free)
		PRVM_ERROR ("VM_parseentitydata: %s: Can only set already spawned entities (entity %i is free)!", PRVM_NAME, PRVM_NUM_FOR_EDICT(ent));

	data = PRVM_G_STRING(OFS_PARM1);

	// parse the opening brace
	if (!COM_ParseToken_Simple(&data, false, false) || com_token[0] != '{' )
		PRVM_ERROR ("VM_parseentitydata: %s: Couldn't parse entity data:\n%s", PRVM_NAME, data );

	PRVM_ED_ParseEdict (data, ent);
}

/*
=========
VM_loadfromfile

loadfromfile(string file)
=========
*/
void VM_loadfromfile(void)
{
	const char *filename;
	char *data;

	VM_SAFEPARMCOUNT(1,VM_loadfromfile);

	filename = PRVM_G_STRING(OFS_PARM0);
	if (FS_CheckNastyPath(filename, false))
	{
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		VM_Warning("VM_loadfromfile: %s dangerous or non-portable filename \"%s\" not allowed. (contains : or \\ or begins with .. or /)\n", PRVM_NAME, filename);
		return;
	}

	// not conform with VM_fopen
	data = (char *)FS_LoadFile(filename, tempmempool, false, NULL);
	if (data == NULL)
		PRVM_G_FLOAT(OFS_RETURN) = -1;

	PRVM_ED_LoadFromFile(data);

	if(data)
		Mem_Free(data);
}


/*
=========
VM_modulo

float	mod(float val, float m)
=========
*/
void VM_modulo(void)
{
	int val, m;
	VM_SAFEPARMCOUNT(2,VM_module);

	val = (int) PRVM_G_FLOAT(OFS_PARM0);
	m	= (int) PRVM_G_FLOAT(OFS_PARM1);

	PRVM_G_FLOAT(OFS_RETURN) = (float) (val % m);
}

void VM_Search_Init(void)
{
	int i;
	for (i = 0;i < PRVM_MAX_OPENSEARCHES;i++)
		prog->opensearches[i] = NULL;
}

void VM_Search_Reset(void)
{
	int i;
	// reset the fssearch list
	for(i = 0; i < PRVM_MAX_OPENSEARCHES; i++)
	{
		if(prog->opensearches[i])
			FS_FreeSearch(prog->opensearches[i]);
		prog->opensearches[i] = NULL;
	}
}

/*
=========
VM_search_begin

float search_begin(string pattern, float caseinsensitive, float quiet)
=========
*/
void VM_search_begin(void)
{
	int handle;
	const char *pattern;
	int caseinsens, quiet;

	VM_SAFEPARMCOUNT(3, VM_search_begin);

	pattern = PRVM_G_STRING(OFS_PARM0);

	VM_CheckEmptyString(pattern);

	caseinsens = (int)PRVM_G_FLOAT(OFS_PARM1);
	quiet = (int)PRVM_G_FLOAT(OFS_PARM2);

	for(handle = 0; handle < PRVM_MAX_OPENSEARCHES; handle++)
		if(!prog->opensearches[handle])
			break;

	if(handle >= PRVM_MAX_OPENSEARCHES)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_search_begin: %s ran out of search handles (%i)\n", PRVM_NAME, PRVM_MAX_OPENSEARCHES);
		return;
	}

	if(!(prog->opensearches[handle] = FS_Search(pattern,caseinsens, quiet)))
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	else
	{
		prog->opensearches_origin[handle] = PRVM_AllocationOrigin();
		PRVM_G_FLOAT(OFS_RETURN) = handle;
	}
}

/*
=========
VM_search_end

void	search_end(float handle)
=========
*/
void VM_search_end(void)
{
	int handle;
	VM_SAFEPARMCOUNT(1, VM_search_end);

	handle = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= PRVM_MAX_OPENSEARCHES)
	{
		VM_Warning("VM_search_end: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(prog->opensearches[handle] == NULL)
	{
		VM_Warning("VM_search_end: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}

	FS_FreeSearch(prog->opensearches[handle]);
	prog->opensearches[handle] = NULL;
	if(prog->opensearches_origin[handle])
		PRVM_Free((char *)prog->opensearches_origin[handle]);
}

/*
=========
VM_search_getsize

float	search_getsize(float handle)
=========
*/
void VM_search_getsize(void)
{
	int handle;
	VM_SAFEPARMCOUNT(1, VM_M_search_getsize);

	handle = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= PRVM_MAX_OPENSEARCHES)
	{
		VM_Warning("VM_search_getsize: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(prog->opensearches[handle] == NULL)
	{
		VM_Warning("VM_search_getsize: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}

	PRVM_G_FLOAT(OFS_RETURN) = prog->opensearches[handle]->numfilenames;
}

/*
=========
VM_search_getfilename

string	search_getfilename(float handle, float num)
=========
*/
void VM_search_getfilename(void)
{
	int handle, filenum;
	VM_SAFEPARMCOUNT(2, VM_search_getfilename);

	handle = (int)PRVM_G_FLOAT(OFS_PARM0);
	filenum = (int)PRVM_G_FLOAT(OFS_PARM1);

	if(handle < 0 || handle >= PRVM_MAX_OPENSEARCHES)
	{
		VM_Warning("VM_search_getfilename: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(prog->opensearches[handle] == NULL)
	{
		VM_Warning("VM_search_getfilename: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}
	if(filenum < 0 || filenum >= prog->opensearches[handle]->numfilenames)
	{
		VM_Warning("VM_search_getfilename: invalid filenum %i in %s\n", filenum, PRVM_NAME);
		return;
	}

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(prog->opensearches[handle]->filenames[filenum]);
}

/*
=========
VM_chr

string	chr(float ascii)
=========
*/
void VM_chr(void)
{
	/*
	char tmp[2];
	VM_SAFEPARMCOUNT(1, VM_chr);

	tmp[0] = (unsigned char) PRVM_G_FLOAT(OFS_PARM0);
	tmp[1] = 0;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(tmp);
	*/
	
	char tmp[8];
	int len;
	VM_SAFEPARMCOUNT(1, VM_chr);

	len = u8_fromchar((Uchar)PRVM_G_FLOAT(OFS_PARM0), tmp, sizeof(tmp));
	tmp[len] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(tmp);
}

//=============================================================================
// Draw builtins (client & menu)

/*
=========
VM_iscachedpic

float	iscachedpic(string pic)
=========
*/
void VM_iscachedpic(void)
{
	VM_SAFEPARMCOUNT(1,VM_iscachedpic);

	// drawq hasnt such a function, thus always return true
	PRVM_G_FLOAT(OFS_RETURN) = false;
}

/*
=========
VM_precache_pic

string	precache_pic(string pic)
=========
*/
void VM_precache_pic(void)
{
	const char	*s;

	VM_SAFEPARMCOUNT(1, VM_precache_pic);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
	VM_CheckEmptyString (s);

	// AK Draw_CachePic is supposed to always return a valid pointer
	if( Draw_CachePic_Flags(s, 0)->tex == r_texture_notexture )
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
}

/*
=========
VM_freepic

freepic(string s)
=========
*/
void VM_freepic(void)
{
	const char *s;

	VM_SAFEPARMCOUNT(1,VM_freepic);

	s = PRVM_G_STRING(OFS_PARM0);
	VM_CheckEmptyString (s);

	Draw_FreePic(s);
}

void getdrawfontscale(float *sx, float *sy)
{
	vec3_t v;
	*sx = *sy = 1;
	VectorCopy(PRVM_drawglobalvector(drawfontscale), v);
	if(VectorLength2(v) > 0)
	{
		*sx = v[0];
		*sy = v[1];
	}
}

dp_font_t *getdrawfont(void)
{
	int f = (int) PRVM_drawglobalfloat(drawfont);
	if(f < 0 || f >= dp_fonts.maxsize)
		return FONT_DEFAULT;
	return &dp_fonts.f[f];
}

/*
=========
VM_drawcharacter

float	drawcharacter(vector position, float character, vector scale, vector rgb, float alpha, float flag)
=========
*/
void VM_drawcharacter(void)
{
	float *pos,*scale,*rgb;
	char   character;
	int flag;
	float sx, sy;
	VM_SAFEPARMCOUNT(6,VM_drawcharacter);

	character = (char) PRVM_G_FLOAT(OFS_PARM1);
	if(character == 0)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		VM_Warning("VM_drawcharacter: %s passed null character !\n",PRVM_NAME);
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	flag = (int)PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawcharacter: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(pos[2] || scale[2])
		Con_Printf("VM_drawcharacter: z value%c from %s discarded\n",(pos[2] && scale[2]) ? 's' : 0,((pos[2] && scale[2]) ? "pos and scale" : (pos[2] ? "pos" : "scale")));

	if(!scale[0] || !scale[1])
	{
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning("VM_drawcharacter: scale %s is null !\n", (scale[0] == 0) ? ((scale[1] == 0) ? "x and y" : "x") : "y");
		return;
	}

	getdrawfontscale(&sx, &sy);
	DrawQ_String_Scale(pos[0], pos[1], &character, 1, scale[0], scale[1], sx, sy, rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag, NULL, true, getdrawfont());
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_drawstring

float	drawstring(vector position, string text, vector scale, vector rgb, float alpha, float flag)
=========
*/
void VM_drawstring(void)
{
	float *pos,*scale,*rgb;
	const char  *string;
	int flag;
	float sx, sy;
	VM_SAFEPARMCOUNT(6,VM_drawstring);

	string = PRVM_G_STRING(OFS_PARM1);
	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	flag = (int)PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawstring: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(!scale[0] || !scale[1])
	{
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning("VM_drawstring: scale %s is null !\n", (scale[0] == 0) ? ((scale[1] == 0) ? "x and y" : "x") : "y");
		return;
	}

	if(pos[2] || scale[2])
		Con_Printf("VM_drawstring: z value%s from %s discarded\n",(pos[2] && scale[2]) ? "s" : " ",((pos[2] && scale[2]) ? "pos and scale" : (pos[2] ? "pos" : "scale")));

	getdrawfontscale(&sx, &sy);
	DrawQ_String_Scale(pos[0], pos[1], string, 0, scale[0], scale[1], sx, sy, rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag, NULL, true, getdrawfont());
	//Font_DrawString(pos[0], pos[1], string, 0, scale[0], scale[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag, NULL, true);
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_drawcolorcodedstring

float	drawcolorcodedstring(vector position, string text, vector scale, float alpha, float flag)
/
float	drawcolorcodedstring(vector position, string text, vector scale, vector rgb, float alpha, float flag)
=========
*/
void VM_drawcolorcodedstring(void)
{
	float *pos, *scale;
	const char  *string;
	int flag;
	vec3_t rgb;
	float sx, sy, alpha;

	VM_SAFEPARMCOUNTRANGE(5,6,VM_drawcolorcodedstring);

	if (prog->argc == 6) // full 6 parms, like normal drawstring
	{
		pos = PRVM_G_VECTOR(OFS_PARM0);
		string = PRVM_G_STRING(OFS_PARM1);
		scale = PRVM_G_VECTOR(OFS_PARM2);
		VectorCopy(PRVM_G_VECTOR(OFS_PARM3), rgb); 
		alpha = PRVM_G_FLOAT(OFS_PARM4);
		flag = (int)PRVM_G_FLOAT(OFS_PARM5);
	}
	else
	{
		pos = PRVM_G_VECTOR(OFS_PARM0);
		string = PRVM_G_STRING(OFS_PARM1);
		scale = PRVM_G_VECTOR(OFS_PARM2);
		rgb[0] = 1.0;
		rgb[1] = 1.0;
		rgb[2] = 1.0;
		alpha = PRVM_G_FLOAT(OFS_PARM3);
		flag = (int)PRVM_G_FLOAT(OFS_PARM4);
	}

	if(flag < DRAWFLAG_NORMAL || flag >= DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawcolorcodedstring: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(!scale[0] || !scale[1])
	{
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning("VM_drawcolorcodedstring: scale %s is null !\n", (scale[0] == 0) ? ((scale[1] == 0) ? "x and y" : "x") : "y");
		return;
	}

	if(pos[2] || scale[2])
		Con_Printf("VM_drawcolorcodedstring: z value%s from %s discarded\n",(pos[2] && scale[2]) ? "s" : " ",((pos[2] && scale[2]) ? "pos and scale" : (pos[2] ? "pos" : "scale")));

	getdrawfontscale(&sx, &sy);
	DrawQ_String_Scale(pos[0], pos[1], string, 0, scale[0], scale[1], sx, sy, rgb[0], rgb[1], rgb[2], alpha, flag, NULL, false, getdrawfont());
	if (prog->argc == 6) // also return vector of last color
		VectorCopy(DrawQ_Color, PRVM_G_VECTOR(OFS_RETURN));
	else
		PRVM_G_FLOAT(OFS_RETURN) = 1;
}
/*
=========
VM_stringwidth

float	stringwidth(string text, float allowColorCodes, float size)
=========
*/
void VM_stringwidth(void)
{
	const char  *string;
	float *szv;
	float mult; // sz is intended font size so we can later add freetype support, mult is font size multiplier in pixels per character cell
	int colors;
	float sx, sy;
	size_t maxlen = 0;
	VM_SAFEPARMCOUNTRANGE(2,3,VM_drawstring);

	getdrawfontscale(&sx, &sy);
	if(prog->argc == 3)
	{
		szv = PRVM_G_VECTOR(OFS_PARM2);
		mult = 1;
	}
	else
	{
		// we want the width for 8x8 font size, divided by 8
		static float defsize[] = {8, 8};
		szv = defsize;
		mult = 0.125;
		// to make sure snapping is turned off, ALWAYS use a nontrivial scale in this case
		if(sx >= 0.9 && sx <= 1.1)
		{
			mult *= 2;
			sx /= 2;
			sy /= 2;
		}
	}

	string = PRVM_G_STRING(OFS_PARM0);
	colors = (int)PRVM_G_FLOAT(OFS_PARM1);

	PRVM_G_FLOAT(OFS_RETURN) = DrawQ_TextWidth_UntilWidth_TrackColors_Scale(string, &maxlen, szv[0], szv[1], sx, sy, NULL, !colors, getdrawfont(), 1000000000) * mult;
/*
	if(prog->argc == 3)
	{
		mult = sz = PRVM_G_FLOAT(OFS_PARM2);
	}
	else
	{
		sz = 8;
		mult = 1;
	}

	string = PRVM_G_STRING(OFS_PARM0);
	colors = (int)PRVM_G_FLOAT(OFS_PARM1);

	PRVM_G_FLOAT(OFS_RETURN) = DrawQ_TextWidth(string, 0, !colors, getdrawfont()) * mult; // 1x1 characters, don't actually draw
*/
}

/*
=========
VM_findfont

float findfont(string s)
=========
*/

float getdrawfontnum(const char *fontname)
{
	int i;

	for(i = 0; i < dp_fonts.maxsize; ++i)
		if(!strcmp(dp_fonts.f[i].title, fontname))
			return i;
	return -1;
}

void VM_findfont(void)
{
	VM_SAFEPARMCOUNT(1,VM_findfont);
	PRVM_G_FLOAT(OFS_RETURN) = getdrawfontnum(PRVM_G_STRING(OFS_PARM0));
}

/*
=========
VM_loadfont

float loadfont(string fontname, string fontmaps, string sizes, float slot)
=========
*/

dp_font_t *FindFont(const char *title, qboolean allocate_new);
void LoadFont(qboolean override, const char *name, dp_font_t *fnt, float scale, float voffset);
void VM_loadfont(void)
{
	const char *fontname, *filelist, *sizes, *c, *cm;
	char mainfont[MAX_QPATH];
	int i, numsizes;
	float sz, scale, voffset;
	dp_font_t *f;

	VM_SAFEPARMCOUNTRANGE(3,6,VM_loadfont);

	fontname = PRVM_G_STRING(OFS_PARM0);
	if (!fontname[0])
		fontname = "default";

	filelist = PRVM_G_STRING(OFS_PARM1);
	if (!filelist[0])
		filelist = "gfx/conchars";

	sizes = PRVM_G_STRING(OFS_PARM2);
	if (!sizes[0])
		sizes = "10";

	// find a font
	f = NULL;
	if (prog->argc >= 4)
	{
		i = PRVM_G_FLOAT(OFS_PARM3);
		if (i >= 0 && i < dp_fonts.maxsize)
		{
			f = &dp_fonts.f[i];
			strlcpy(f->title, fontname, sizeof(f->title)); // replace name
		}
	}
	if (!f)
		f = FindFont(fontname, true);
	if (!f)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return; // something go wrong
	}

	memset(f->fallbacks, 0, sizeof(f->fallbacks));
	memset(f->fallback_faces, 0, sizeof(f->fallback_faces));

	// first font is handled "normally"
	c = strchr(filelist, ':');
	cm = strchr(filelist, ',');
	if(c && (!cm || c < cm))
		f->req_face = atoi(c+1);
	else
	{
		f->req_face = 0;
		c = cm;
	}
	if(!c || (c - filelist) > MAX_QPATH)
		strlcpy(mainfont, filelist, sizeof(mainfont));
	else
	{
		memcpy(mainfont, filelist, c - filelist);
		mainfont[c - filelist] = 0;
	}

	// handle fallbacks
	for(i = 0; i < MAX_FONT_FALLBACKS; ++i)
	{
		c = strchr(filelist, ',');
		if(!c)
			break;
		filelist = c + 1;
		if(!*filelist)
			break;
		c = strchr(filelist, ':');
		cm = strchr(filelist, ',');
		if(c && (!cm || c < cm))
			f->fallback_faces[i] = atoi(c+1);
		else
		{
			f->fallback_faces[i] = 0; // f->req_face; could make it stick to the default-font's face index
			c = cm;
		}
		if(!c || (c-filelist) > MAX_QPATH)
		{
			strlcpy(f->fallbacks[i], filelist, sizeof(mainfont));
		}
		else
		{
			memcpy(f->fallbacks[i], filelist, c - filelist);
			f->fallbacks[i][c - filelist] = 0;
		}
	}

	// handle sizes
	for(i = 0; i < MAX_FONT_SIZES; ++i)
		f->req_sizes[i] = -1;
	for (numsizes = 0,c = sizes;;)
	{
		if (!COM_ParseToken_VM_Tokenize(&c, 0))
			break;
		sz = atof(com_token);
		// detect crap size
		if (sz < 0.001f || sz > 1000.0f)
		{
			VM_Warning("VM_loadfont: crap size %s", com_token);
			continue;
		}
		// check overflow
		if (numsizes == MAX_FONT_SIZES)
		{
			VM_Warning("VM_loadfont: MAX_FONT_SIZES = %i exceeded", MAX_FONT_SIZES);
			break;
		}
		f->req_sizes[numsizes] = sz;
		numsizes++;
	}

	// additional scale/hoffset parms
	scale = 1;
	voffset = 0;
	if (prog->argc >= 5)
	{
		scale = PRVM_G_FLOAT(OFS_PARM4);
		if (scale <= 0)
			scale = 1;
	}
	if (prog->argc >= 6)
		voffset = PRVM_G_FLOAT(OFS_PARM5);

	// load
	LoadFont(true, mainfont, f, scale, voffset);

	// return index of loaded font
	PRVM_G_FLOAT(OFS_RETURN) = (f - dp_fonts.f);
}

/*
=========
VM_drawpic

float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag)
=========
*/
void VM_drawpic(void)
{
	const char *picname;
	float *size, *pos, *rgb;
	int flag;

	VM_SAFEPARMCOUNT(6,VM_drawpic);

	picname = PRVM_G_STRING(OFS_PARM1);
	VM_CheckEmptyString (picname);

	// is pic cached ? no function yet for that
	if(!1)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		VM_Warning("VM_drawpic: %s: %s not cached !\n", PRVM_NAME, picname);
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	flag = (int) PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawpic: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(pos[2] || size[2])
		Con_Printf("VM_drawpic: z value%s from %s discarded\n",(pos[2] && size[2]) ? "s" : " ",((pos[2] && size[2]) ? "pos and size" : (pos[2] ? "pos" : "size")));

	DrawQ_Pic(pos[0], pos[1], Draw_CachePic_Flags (picname, CACHEPICFLAG_NOTPERSISTENT), size[0], size[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag);
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}
/*
=========
VM_drawrotpic

float	drawrotpic(vector position, string pic, vector size, vector org, float angle, vector rgb, float alpha, float flag)
=========
*/
void VM_drawrotpic(void)
{
	const char *picname;
	float *size, *pos, *org, *rgb;
	int flag;

	VM_SAFEPARMCOUNT(8,VM_drawrotpic);

	picname = PRVM_G_STRING(OFS_PARM1);
	VM_CheckEmptyString (picname);

	// is pic cached ? no function yet for that
	if(!1)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		VM_Warning("VM_drawrotpic: %s: %s not cached !\n", PRVM_NAME, picname);
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM2);
	org = PRVM_G_VECTOR(OFS_PARM3);
	rgb = PRVM_G_VECTOR(OFS_PARM5);
	flag = (int) PRVM_G_FLOAT(OFS_PARM7);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawrotpic: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(pos[2] || size[2] || org[2])
		Con_Printf("VM_drawrotpic: z value from pos/size/org discarded\n");

	DrawQ_RotPic(pos[0], pos[1], Draw_CachePic_Flags(picname, CACHEPICFLAG_NOTPERSISTENT), size[0], size[1], org[0], org[1], PRVM_G_FLOAT(OFS_PARM4), rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM6), flag);
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}
/*
=========
VM_drawsubpic

float	drawsubpic(vector position, vector size, string pic, vector srcPos, vector srcSize, vector rgb, float alpha, float flag)

=========
*/
void VM_drawsubpic(void)
{
	const char *picname;
	float *size, *pos, *rgb, *srcPos, *srcSize, alpha;
	int flag;

	VM_SAFEPARMCOUNT(8,VM_drawsubpic);

	picname = PRVM_G_STRING(OFS_PARM2);
	VM_CheckEmptyString (picname);

	// is pic cached ? no function yet for that
	if(!1)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		VM_Warning("VM_drawsubpic: %s: %s not cached !\n", PRVM_NAME, picname);
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM1);
	srcPos = PRVM_G_VECTOR(OFS_PARM3);
	srcSize = PRVM_G_VECTOR(OFS_PARM4);
	rgb = PRVM_G_VECTOR(OFS_PARM5);
	alpha = PRVM_G_FLOAT(OFS_PARM6);
	flag = (int) PRVM_G_FLOAT(OFS_PARM7);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawsubpic: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(pos[2] || size[2])
		Con_Printf("VM_drawsubpic: z value%s from %s discarded\n",(pos[2] && size[2]) ? "s" : " ",((pos[2] && size[2]) ? "pos and size" : (pos[2] ? "pos" : "size")));

	DrawQ_SuperPic(pos[0], pos[1], Draw_CachePic_Flags (picname, CACHEPICFLAG_NOTPERSISTENT),
		size[0], size[1],
		srcPos[0],              srcPos[1],              rgb[0], rgb[1], rgb[2], alpha,
		srcPos[0] + srcSize[0], srcPos[1],              rgb[0], rgb[1], rgb[2], alpha,
		srcPos[0],              srcPos[1] + srcSize[1], rgb[0], rgb[1], rgb[2], alpha,
		srcPos[0] + srcSize[0], srcPos[1] + srcSize[1], rgb[0], rgb[1], rgb[2], alpha,
		flag);
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_drawfill

float drawfill(vector position, vector size, vector rgb, float alpha, float flag)
=========
*/
void VM_drawfill(void)
{
	float *size, *pos, *rgb;
	int flag;

	VM_SAFEPARMCOUNT(5,VM_drawfill);


	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM1);
	rgb = PRVM_G_VECTOR(OFS_PARM2);
	flag = (int) PRVM_G_FLOAT(OFS_PARM4);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_drawfill: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		return;
	}

	if(pos[2] || size[2])
		Con_Printf("VM_drawfill: z value%s from %s discarded\n",(pos[2] && size[2]) ? "s" : " ",((pos[2] && size[2]) ? "pos and size" : (pos[2] ? "pos" : "size")));

	DrawQ_Fill(pos[0], pos[1], size[0], size[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM3), flag);
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_drawsetcliparea

drawsetcliparea(float x, float y, float width, float height)
=========
*/
void VM_drawsetcliparea(void)
{
	float x,y,w,h;
	VM_SAFEPARMCOUNT(4,VM_drawsetcliparea);

	x = bound(0, PRVM_G_FLOAT(OFS_PARM0), vid_conwidth.integer);
	y = bound(0, PRVM_G_FLOAT(OFS_PARM1), vid_conheight.integer);
	w = bound(0, PRVM_G_FLOAT(OFS_PARM2) + PRVM_G_FLOAT(OFS_PARM0) - x, (vid_conwidth.integer  - x));
	h = bound(0, PRVM_G_FLOAT(OFS_PARM3) + PRVM_G_FLOAT(OFS_PARM1) - y, (vid_conheight.integer - y));

	DrawQ_SetClipArea(x, y, w, h);
}

/*
=========
VM_drawresetcliparea

drawresetcliparea()
=========
*/
void VM_drawresetcliparea(void)
{
	VM_SAFEPARMCOUNT(0,VM_drawresetcliparea);

	DrawQ_ResetClipArea();
}

/*
=========
VM_getimagesize

vector	getimagesize(string pic)
=========
*/
void VM_getimagesize(void)
{
	const char *p;
	cachepic_t *pic;

	VM_SAFEPARMCOUNT(1,VM_getimagesize);

	p = PRVM_G_STRING(OFS_PARM0);
	VM_CheckEmptyString (p);

	pic = Draw_CachePic_Flags (p, CACHEPICFLAG_NOTPERSISTENT);

	PRVM_G_VECTOR(OFS_RETURN)[0] = pic->width;
	PRVM_G_VECTOR(OFS_RETURN)[1] = pic->height;
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
VM_keynumtostring

string keynumtostring(float keynum)
=========
*/
void VM_keynumtostring (void)
{
	VM_SAFEPARMCOUNT(1, VM_keynumtostring);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(Key_KeynumToString((int)PRVM_G_FLOAT(OFS_PARM0)));
}

/*
=========
VM_findkeysforcommand

string	findkeysforcommand(string command, float bindmap)

the returned string is an altstring
=========
*/
#define FKFC_NUMKEYS 5
void M_FindKeysForCommand(const char *command, int *keys);
void VM_findkeysforcommand(void)
{
	const char *cmd;
	char ret[VM_STRINGTEMP_LENGTH];
	int keys[FKFC_NUMKEYS];
	int i;
	int bindmap;

	VM_SAFEPARMCOUNTRANGE(1, 2, VM_findkeysforcommand);

	cmd = PRVM_G_STRING(OFS_PARM0);
	if(prog->argc == 2)
		bindmap = bound(-1, PRVM_G_FLOAT(OFS_PARM1), MAX_BINDMAPS-1);
	else
		bindmap = 0; // consistent to "bind"

	VM_CheckEmptyString(cmd);

	Key_FindKeysForCommand(cmd, keys, FKFC_NUMKEYS, bindmap);

	ret[0] = 0;
	for(i = 0; i < FKFC_NUMKEYS; i++)
		strlcat(ret, va(" \'%i\'", keys[i]), sizeof(ret));

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(ret);
}

/*
=========
VM_stringtokeynum

float stringtokeynum(string key)
=========
*/
void VM_stringtokeynum (void)
{
	VM_SAFEPARMCOUNT( 1, VM_keynumtostring );

	PRVM_G_FLOAT(OFS_RETURN) = Key_StringToKeynum(PRVM_G_STRING(OFS_PARM0));
}

/*
=========
VM_getkeybind

string getkeybind(float key, float bindmap)
=========
*/
void VM_getkeybind (void)
{
	int bindmap;
	VM_SAFEPARMCOUNTRANGE(1, 2, VM_CL_getkeybind);
	if(prog->argc == 2)
		bindmap = bound(-1, PRVM_G_FLOAT(OFS_PARM1), MAX_BINDMAPS-1);
	else
		bindmap = 0; // consistent to "bind"

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(Key_GetBind((int)PRVM_G_FLOAT(OFS_PARM0), bindmap));
}

/*
=========
VM_setkeybind

float setkeybind(float key, string cmd, float bindmap)
=========
*/
void VM_setkeybind (void)
{
	int bindmap;
	VM_SAFEPARMCOUNTRANGE(2, 3, VM_CL_setkeybind);
	if(prog->argc == 3)
		bindmap = bound(-1, PRVM_G_FLOAT(OFS_PARM2), MAX_BINDMAPS-1);
	else
		bindmap = 0; // consistent to "bind"

	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if(Key_SetBinding((int)PRVM_G_FLOAT(OFS_PARM0), bindmap, PRVM_G_STRING(OFS_PARM1)))
		PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_getbindmap

vector getbindmaps()
=========
*/
void VM_getbindmaps (void)
{
	int fg, bg;
	VM_SAFEPARMCOUNT(0, VM_CL_getbindmap);
	Key_GetBindMap(&fg, &bg);
	PRVM_G_VECTOR(OFS_RETURN)[0] = fg;
	PRVM_G_VECTOR(OFS_RETURN)[1] = bg;
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
VM_setbindmap

float setbindmaps(vector bindmap)
=========
*/
void VM_setbindmaps (void)
{
	VM_SAFEPARMCOUNT(1, VM_CL_setbindmap);
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if(PRVM_G_VECTOR(OFS_PARM0)[2] == 0)
		if(Key_SetBindMap((int)PRVM_G_VECTOR(OFS_PARM0)[0], (int)PRVM_G_VECTOR(OFS_PARM0)[1]))
			PRVM_G_FLOAT(OFS_RETURN) = 1;
}

// CL_Video interface functions

/*
========================
VM_cin_open

float cin_open(string file, string name)
========================
*/
void VM_cin_open( void )
{
	const char *file;
	const char *name;

	VM_SAFEPARMCOUNT( 2, VM_cin_open );

	file = PRVM_G_STRING( OFS_PARM0 );
	name = PRVM_G_STRING( OFS_PARM1 );

	VM_CheckEmptyString( file );
    VM_CheckEmptyString( name );

	if( CL_OpenVideo( file, name, MENUOWNER, "" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = 1;
	else
		PRVM_G_FLOAT( OFS_RETURN ) = 0;
}

/*
========================
VM_cin_close

void cin_close(string name)
========================
*/
void VM_cin_close( void )
{
	const char *name;

	VM_SAFEPARMCOUNT( 1, VM_cin_close );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );

	CL_CloseVideo( CL_GetVideoByName( name ) );
}

/*
========================
VM_cin_setstate
void cin_setstate(string name, float type)
========================
*/
void VM_cin_setstate( void )
{
	const char *name;
	clvideostate_t 	state;
	clvideo_t		*video;

	VM_SAFEPARMCOUNT( 2, VM_cin_netstate );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );

	state = (clvideostate_t)((int)PRVM_G_FLOAT( OFS_PARM1 ));

	video = CL_GetVideoByName( name );
	if( video && state > CLVIDEO_UNUSED && state < CLVIDEO_STATECOUNT )
		CL_SetVideoState( video, state );
}

/*
========================
VM_cin_getstate

float cin_getstate(string name)
========================
*/
void VM_cin_getstate( void )
{
	const char *name;
	clvideo_t		*video;

	VM_SAFEPARMCOUNT( 1, VM_cin_getstate );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );

	video = CL_GetVideoByName( name );
	if( video )
		PRVM_G_FLOAT( OFS_RETURN ) = (int)video->state;
	else
		PRVM_G_FLOAT( OFS_RETURN ) = 0;
}

/*
========================
VM_cin_restart

void cin_restart(string name)
========================
*/
void VM_cin_restart( void )
{
	const char *name;
	clvideo_t		*video;

	VM_SAFEPARMCOUNT( 1, VM_cin_restart );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );

	video = CL_GetVideoByName( name );
	if( video )
		CL_RestartVideo( video );
}

/*
========================
VM_Gecko_Init
========================
*/
void VM_Gecko_Init( void ) {
	// the prog struct is memset to 0 by Initprog? [12/6/2007 Black]
	// FIXME: remove the other _Init functions then, too? [12/6/2007 Black]
}

/*
========================
VM_Gecko_Destroy
========================
*/
void VM_Gecko_Destroy( void ) {
	int i;
	for( i = 0 ; i < PRVM_MAX_GECKOINSTANCES ; i++ ) {
		clgecko_t **instance = &prog->opengeckoinstances[ i ];
		if( *instance ) {
			CL_Gecko_DestroyBrowser( *instance );
		}
		*instance = NULL;
	}
}

/*
========================
VM_gecko_create

float[bool] gecko_create( string name )
========================
*/
void VM_gecko_create( void ) {
	const char *name;
	int i;
	clgecko_t *instance;
   	
	VM_SAFEPARMCOUNT( 1, VM_gecko_create );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );

	// find an empty slot for this gecko browser..
	for( i = 0 ; i < PRVM_MAX_GECKOINSTANCES ; i++ ) {
		if( prog->opengeckoinstances[ i ] == NULL ) {
			break;
		}
	}
	if( i == PRVM_MAX_GECKOINSTANCES ) {
			VM_Warning("VM_gecko_create: %s ran out of gecko handles (%i)\n", PRVM_NAME, PRVM_MAX_GECKOINSTANCES);
			PRVM_G_FLOAT( OFS_RETURN ) = 0;
			return;
	}

	instance = prog->opengeckoinstances[ i ] = CL_Gecko_CreateBrowser( name, PRVM_GetProgNr() );
   if( !instance ) {
		// TODO: error handling [12/3/2007 Black]
		PRVM_G_FLOAT( OFS_RETURN ) = 0;
		return;
	}
	PRVM_G_FLOAT( OFS_RETURN ) = 1;
}

/*
========================
VM_gecko_destroy

void gecko_destroy( string name )
========================
*/
void VM_gecko_destroy( void ) {
	const char *name;
	clgecko_t *instance;

	VM_SAFEPARMCOUNT( 1, VM_gecko_destroy );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );
	instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		return;
	}
	CL_Gecko_DestroyBrowser( instance );
}

/*
========================
VM_gecko_navigate

void gecko_navigate( string name, string URI )
========================
*/
void VM_gecko_navigate( void ) {
	const char *name;
	const char *URI;
	clgecko_t *instance;

	VM_SAFEPARMCOUNT( 2, VM_gecko_navigate );

	name = PRVM_G_STRING( OFS_PARM0 );
	URI = PRVM_G_STRING( OFS_PARM1 );
	VM_CheckEmptyString( name );
	VM_CheckEmptyString( URI );

   instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		return;
	}
	CL_Gecko_NavigateToURI( instance, URI );
}

/*
========================
VM_gecko_keyevent

float[bool] gecko_keyevent( string name, float key, float eventtype ) 
========================
*/
void VM_gecko_keyevent( void ) {
	const char *name;
	unsigned int key;
	clgecko_buttoneventtype_t eventtype;
	clgecko_t *instance;

	VM_SAFEPARMCOUNT( 3, VM_gecko_keyevent );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );
	key = (unsigned int) PRVM_G_FLOAT( OFS_PARM1 );
	switch( (unsigned int) PRVM_G_FLOAT( OFS_PARM2 ) ) {
	case 0:
		eventtype = CLG_BET_DOWN;
		break;
	case 1:
		eventtype = CLG_BET_UP;
		break;
	case 2:
		eventtype = CLG_BET_PRESS;
		break;
	case 3:
		eventtype = CLG_BET_DOUBLECLICK;
		break;
	default:
		// TODO: console printf? [12/3/2007 Black]
		PRVM_G_FLOAT( OFS_RETURN ) = 0;
		return;
	}

	instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		PRVM_G_FLOAT( OFS_RETURN ) = 0;
		return;
	}

	PRVM_G_FLOAT( OFS_RETURN ) = (CL_Gecko_Event_Key( instance, (keynum_t) key, eventtype ) == true);
}

/*
========================
VM_gecko_movemouse

void gecko_mousemove( string name, float x, float y )
========================
*/
void VM_gecko_movemouse( void ) {
	const char *name;
	float x, y;
	clgecko_t *instance;

	VM_SAFEPARMCOUNT( 3, VM_gecko_movemouse );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );
	x = PRVM_G_FLOAT( OFS_PARM1 );
	y = PRVM_G_FLOAT( OFS_PARM2 );
	
	instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		return;
	}
	CL_Gecko_Event_CursorMove( instance, x, y );
}


/*
========================
VM_gecko_resize

void gecko_resize( string name, float w, float h )
========================
*/
void VM_gecko_resize( void ) {
	const char *name;
	float w, h;
	clgecko_t *instance;

	VM_SAFEPARMCOUNT( 3, VM_gecko_movemouse );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );
	w = PRVM_G_FLOAT( OFS_PARM1 );
	h = PRVM_G_FLOAT( OFS_PARM2 );
	
	instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		return;
	}
	CL_Gecko_Resize( instance, (int) w, (int) h );
}


/*
========================
VM_gecko_get_texture_extent

vector gecko_get_texture_extent( string name )
========================
*/
void VM_gecko_get_texture_extent( void ) {
	const char *name;
	clgecko_t *instance;

	VM_SAFEPARMCOUNT( 1, VM_gecko_movemouse );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );
	
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
	instance = CL_Gecko_FindBrowser( name );
	if( !instance ) {
		PRVM_G_VECTOR(OFS_RETURN)[0] = 0;
		PRVM_G_VECTOR(OFS_RETURN)[1] = 0;
		return;
	}
	CL_Gecko_GetTextureExtent( instance, 
		PRVM_G_VECTOR(OFS_RETURN), PRVM_G_VECTOR(OFS_RETURN)+1 );
}



/*
==============
VM_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
void makevectors(vector angle)
==============
*/
void VM_makevectors (void)
{
	VM_SAFEPARMCOUNT(1, VM_makevectors);
	AngleVectors(PRVM_G_VECTOR(OFS_PARM0), PRVM_gameglobalvector(v_forward), PRVM_gameglobalvector(v_right), PRVM_gameglobalvector(v_up));
}

/*
==============
VM_vectorvectors

Writes new values for v_forward, v_up, and v_right based on the given forward vector
vectorvectors(vector)
==============
*/
void VM_vectorvectors (void)
{
	VM_SAFEPARMCOUNT(1, VM_vectorvectors);
	VectorNormalize2(PRVM_G_VECTOR(OFS_PARM0), PRVM_gameglobalvector(v_forward));
	VectorVectors(PRVM_gameglobalvector(v_forward), PRVM_gameglobalvector(v_right), PRVM_gameglobalvector(v_up));
}

/*
========================
VM_drawline

void drawline(float width, vector pos1, vector pos2, vector rgb, float alpha, float flags)
========================
*/
void VM_drawline (void)
{
	float	*c1, *c2, *rgb;
	float	alpha, width;
	unsigned char	flags;

	VM_SAFEPARMCOUNT(6, VM_drawline);
	width	= PRVM_G_FLOAT(OFS_PARM0);
	c1		= PRVM_G_VECTOR(OFS_PARM1);
	c2		= PRVM_G_VECTOR(OFS_PARM2);
	rgb		= PRVM_G_VECTOR(OFS_PARM3);
	alpha	= PRVM_G_FLOAT(OFS_PARM4);
	flags	= (int)PRVM_G_FLOAT(OFS_PARM5);
	DrawQ_Line(width, c1[0], c1[1], c2[0], c2[1], rgb[0], rgb[1], rgb[2], alpha, flags);
}

// float(float number, float quantity) bitshift (EXT_BITSHIFT)
void VM_bitshift (void)
{
	int n1, n2;
	VM_SAFEPARMCOUNT(2, VM_bitshift);

	n1 = (int)fabs((float)((int)PRVM_G_FLOAT(OFS_PARM0)));
	n2 = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(!n1)
		PRVM_G_FLOAT(OFS_RETURN) = n1;
	else
	if(n2 < 0)
		PRVM_G_FLOAT(OFS_RETURN) = (n1 >> -n2);
	else
		PRVM_G_FLOAT(OFS_RETURN) = (n1 << n2);
}

////////////////////////////////////////
// AltString functions
////////////////////////////////////////

/*
========================
VM_altstr_count

float altstr_count(string)
========================
*/
void VM_altstr_count( void )
{
	const char *altstr, *pos;
	int	count;

	VM_SAFEPARMCOUNT( 1, VM_altstr_count );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	for( count = 0, pos = altstr ; *pos ; pos++ ) {
		if( *pos == '\\' ) {
			if( !*++pos ) {
				break;
			}
		} else if( *pos == '\'' ) {
			count++;
		}
	}

	PRVM_G_FLOAT( OFS_RETURN ) = (float) (count / 2);
}

/*
========================
VM_altstr_prepare

string altstr_prepare(string)
========================
*/
void VM_altstr_prepare( void )
{
	char *out;
	const char *instr, *in;
	int size;
	char outstr[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT( 1, VM_altstr_prepare );

	instr = PRVM_G_STRING( OFS_PARM0 );

	for( out = outstr, in = instr, size = sizeof(outstr) - 1 ; size && *in ; size--, in++, out++ )
		if( *in == '\'' ) {
			*out++ = '\\';
			*out = '\'';
			size--;
		} else
			*out = *in;
	*out = 0;

	PRVM_G_INT( OFS_RETURN ) = PRVM_SetTempString( outstr );
}

/*
========================
VM_altstr_get

string altstr_get(string, float)
========================
*/
void VM_altstr_get( void )
{
	const char *altstr, *pos;
	char *out;
	int count, size;
	char outstr[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT( 2, VM_altstr_get );

	altstr = PRVM_G_STRING( OFS_PARM0 );

	count = (int)PRVM_G_FLOAT( OFS_PARM1 );
	count = count * 2 + 1;

	for( pos = altstr ; *pos && count ; pos++ )
		if( *pos == '\\' ) {
			if( !*++pos )
				break;
		} else if( *pos == '\'' )
			count--;

	if( !*pos ) {
		PRVM_G_INT( OFS_RETURN ) = 0;
		return;
	}

	for( out = outstr, size = sizeof(outstr) - 1 ; size && *pos ; size--, pos++, out++ )
		if( *pos == '\\' ) {
			if( !*++pos )
				break;
			*out = *pos;
			size--;
		} else if( *pos == '\'' )
			break;
		else
			*out = *pos;

	*out = 0;
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetTempString( outstr );
}

/*
========================
VM_altstr_set

string altstr_set(string altstr, float num, string set)
========================
*/
void VM_altstr_set( void )
{
    int num;
	const char *altstr, *str;
	const char *in;
	char *out;
	char outstr[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT( 3, VM_altstr_set );

	altstr = PRVM_G_STRING( OFS_PARM0 );

	num = (int)PRVM_G_FLOAT( OFS_PARM1 );

	str = PRVM_G_STRING( OFS_PARM2 );

	out = outstr;
	for( num = num * 2 + 1, in = altstr; *in && num; *out++ = *in++ )
		if( *in == '\\' ) {
			if( !*++in ) {
				break;
			}
		} else if( *in == '\'' ) {
			num--;
		}

	// copy set in
	for( ; *str; *out++ = *str++ );
	// now jump over the old content
	for( ; *in ; in++ )
		if( *in == '\'' || (*in == '\\' && !*++in) )
			break;

	strlcpy(out, in, outstr + sizeof(outstr) - out);
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetTempString( outstr );
}

/*
========================
VM_altstr_ins
insert after num
string	altstr_ins(string altstr, float num, string set)
========================
*/
void VM_altstr_ins(void)
{
	int num;
	const char *set;
	const char *in;
	char *out;
	char outstr[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT(3, VM_altstr_ins);

	in = PRVM_G_STRING( OFS_PARM0 );
	num = (int)PRVM_G_FLOAT( OFS_PARM1 );
	set = PRVM_G_STRING( OFS_PARM2 );

	out = outstr;
	for( num = num * 2 + 2 ; *in && num > 0 ; *out++ = *in++ )
		if( *in == '\\' ) {
			if( !*++in ) {
				break;
			}
		} else if( *in == '\'' ) {
			num--;
		}

	*out++ = '\'';
	for( ; *set ; *out++ = *set++ );
	*out++ = '\'';

	strlcpy(out, in, outstr + sizeof(outstr) - out);
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetTempString( outstr );
}


////////////////////////////////////////
// BufString functions
////////////////////////////////////////
//[515]: string buffers support

static size_t stringbuffers_sortlength;

static void BufStr_Expand(prvm_stringbuffer_t *stringbuffer, int strindex)
{
	if (stringbuffer->max_strings <= strindex)
	{
		char **oldstrings = stringbuffer->strings;
		stringbuffer->max_strings = max(stringbuffer->max_strings * 2, 128);
		while (stringbuffer->max_strings <= strindex)
			stringbuffer->max_strings *= 2;
		stringbuffer->strings = (char **) Mem_Alloc(prog->progs_mempool, stringbuffer->max_strings * sizeof(stringbuffer->strings[0]));
		if (stringbuffer->num_strings > 0)
			memcpy(stringbuffer->strings, oldstrings, stringbuffer->num_strings * sizeof(stringbuffer->strings[0]));
		if (oldstrings)
			Mem_Free(oldstrings);
	}
}

static void BufStr_Shrink(prvm_stringbuffer_t *stringbuffer)
{
	// reduce num_strings if there are empty string slots at the end
	while (stringbuffer->num_strings > 0 && stringbuffer->strings[stringbuffer->num_strings - 1] == NULL)
		stringbuffer->num_strings--;

	// if empty, free the string pointer array
	if (stringbuffer->num_strings == 0)
	{
		stringbuffer->max_strings = 0;
		if (stringbuffer->strings)
			Mem_Free(stringbuffer->strings);
		stringbuffer->strings = NULL;
	}
}

static int BufStr_SortStringsUP (const void *in1, const void *in2)
{
	const char *a, *b;
	a = *((const char **) in1);
	b = *((const char **) in2);
	if(!a || !a[0])	return 1;
	if(!b || !b[0])	return -1;
	return strncmp(a, b, stringbuffers_sortlength);
}

static int BufStr_SortStringsDOWN (const void *in1, const void *in2)
{
	const char *a, *b;
	a = *((const char **) in1);
	b = *((const char **) in2);
	if(!a || !a[0])	return 1;
	if(!b || !b[0])	return -1;
	return strncmp(b, a, stringbuffers_sortlength);
}

/*
========================
VM_buf_create
creates new buffer, and returns it's index, returns -1 if failed
float buf_create(void) = #460;
float newbuf(string format, float flags) = #460;
========================
*/

void VM_buf_create (void)
{
	prvm_stringbuffer_t *stringbuffer;
	int i;
	
	VM_SAFEPARMCOUNTRANGE(0, 2, VM_buf_create);

	// VorteX: optional parm1 (buffer format) is unfinished, to keep intact with future databuffers extension must be set to "string"
	if(prog->argc >= 1 && strcmp(PRVM_G_STRING(OFS_PARM0), "string"))
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	stringbuffer = (prvm_stringbuffer_t *) Mem_ExpandableArray_AllocRecord(&prog->stringbuffersarray);
	for (i = 0;stringbuffer != Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, i);i++);
	stringbuffer->origin = PRVM_AllocationOrigin();
	// optional flags parm
	if (prog->argc >= 2)
		stringbuffer->flags = (int)PRVM_G_FLOAT(OFS_PARM1) & 0xFF;
	PRVM_G_FLOAT(OFS_RETURN) = i;
}



/*
========================
VM_buf_del
deletes buffer and all strings in it
void buf_del(float bufhandle) = #461;
========================
*/
void VM_buf_del (void)
{
	prvm_stringbuffer_t *stringbuffer;
	VM_SAFEPARMCOUNT(1, VM_buf_del);
	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if (stringbuffer)
	{
		int i;
		for (i = 0;i < stringbuffer->num_strings;i++)
			if (stringbuffer->strings[i])
				Mem_Free(stringbuffer->strings[i]);
		if (stringbuffer->strings)
			Mem_Free(stringbuffer->strings);
		if(stringbuffer->origin)
			PRVM_Free((char *)stringbuffer->origin);
		Mem_ExpandableArray_FreeRecord(&prog->stringbuffersarray, stringbuffer);
	}
	else
	{
		VM_Warning("VM_buf_del: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
}

/*
========================
VM_buf_getsize
how many strings are stored in buffer
float buf_getsize(float bufhandle) = #462;
========================
*/
void VM_buf_getsize (void)
{
	prvm_stringbuffer_t *stringbuffer;
	VM_SAFEPARMCOUNT(1, VM_buf_getsize);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!stringbuffer)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		VM_Warning("VM_buf_getsize: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = stringbuffer->num_strings;
}

/*
========================
VM_buf_copy
copy all content from one buffer to another, make sure it exists
void buf_copy(float bufhandle_from, float bufhandle_to) = #463;
========================
*/
void VM_buf_copy (void)
{
	prvm_stringbuffer_t *srcstringbuffer, *dststringbuffer;
	int i;
	VM_SAFEPARMCOUNT(2, VM_buf_copy);

	srcstringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!srcstringbuffer)
	{
		VM_Warning("VM_buf_copy: invalid source buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	i = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(i == (int)PRVM_G_FLOAT(OFS_PARM0))
	{
		VM_Warning("VM_buf_copy: source == destination (%i) in %s\n", i, PRVM_NAME);
		return;
	}
	dststringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!dststringbuffer)
	{
		VM_Warning("VM_buf_copy: invalid destination buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM1), PRVM_NAME);
		return;
	}

	for (i = 0;i < dststringbuffer->num_strings;i++)
		if (dststringbuffer->strings[i])
			Mem_Free(dststringbuffer->strings[i]);
	if (dststringbuffer->strings)
		Mem_Free(dststringbuffer->strings);
	*dststringbuffer = *srcstringbuffer;
	if (dststringbuffer->max_strings)
		dststringbuffer->strings = (char **)Mem_Alloc(prog->progs_mempool, sizeof(dststringbuffer->strings[0]) * dststringbuffer->max_strings);

	for (i = 0;i < dststringbuffer->num_strings;i++)
	{
		if (srcstringbuffer->strings[i])
		{
			size_t stringlen;
			stringlen = strlen(srcstringbuffer->strings[i]) + 1;
			dststringbuffer->strings[i] = (char *)Mem_Alloc(prog->progs_mempool, stringlen);
			memcpy(dststringbuffer->strings[i], srcstringbuffer->strings[i], stringlen);
		}
	}
}

/*
========================
VM_buf_sort
sort buffer by beginnings of strings (cmplength defaults it's length)
"backward == TRUE" means that sorting goes upside-down
void buf_sort(float bufhandle, float cmplength, float backward) = #464;
========================
*/
void VM_buf_sort (void)
{
	prvm_stringbuffer_t *stringbuffer;
	VM_SAFEPARMCOUNT(3, VM_buf_sort);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!stringbuffer)
	{
		VM_Warning("VM_buf_sort: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(stringbuffer->num_strings <= 0)
	{
		VM_Warning("VM_buf_sort: tried to sort empty buffer %i in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	stringbuffers_sortlength = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(stringbuffers_sortlength <= 0)
		stringbuffers_sortlength = 0x7FFFFFFF;

	if(!PRVM_G_FLOAT(OFS_PARM2))
		qsort(stringbuffer->strings, stringbuffer->num_strings, sizeof(char*), BufStr_SortStringsUP);
	else
		qsort(stringbuffer->strings, stringbuffer->num_strings, sizeof(char*), BufStr_SortStringsDOWN);

	BufStr_Shrink(stringbuffer);
}

/*
========================
VM_buf_implode
concantenates all buffer string into one with "glue" separator and returns it as tempstring
string buf_implode(float bufhandle, string glue) = #465;
========================
*/
void VM_buf_implode (void)
{
	prvm_stringbuffer_t *stringbuffer;
	char			k[VM_STRINGTEMP_LENGTH];
	const char		*sep;
	int				i;
	size_t			l;
	VM_SAFEPARMCOUNT(2, VM_buf_implode);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	if(!stringbuffer)
	{
		VM_Warning("VM_buf_implode: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(!stringbuffer->num_strings)
		return;
	sep = PRVM_G_STRING(OFS_PARM1);
	k[0] = 0;
	for(l = i = 0;i < stringbuffer->num_strings;i++)
	{
		if(stringbuffer->strings[i])
		{
			l += (i > 0 ? strlen(sep) : 0) + strlen(stringbuffer->strings[i]);
			if (l >= sizeof(k) - 1)
				break;
			strlcat(k, sep, sizeof(k));
			strlcat(k, stringbuffer->strings[i], sizeof(k));
		}
	}
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(k);
}

/*
========================
VM_bufstr_get
get a string from buffer, returns tempstring, dont str_unzone it!
string bufstr_get(float bufhandle, float string_index) = #465;
========================
*/
void VM_bufstr_get (void)
{
	prvm_stringbuffer_t *stringbuffer;
	int				strindex;
	VM_SAFEPARMCOUNT(2, VM_bufstr_get);

	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!stringbuffer)
	{
		VM_Warning("VM_bufstr_get: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	strindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	if (strindex < 0)
	{
		// VM_Warning("VM_bufstr_get: invalid string index %i used in %s\n", strindex, PRVM_NAME);
		return;
	}
	if (strindex < stringbuffer->num_strings && stringbuffer->strings[strindex])
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(stringbuffer->strings[strindex]);
}

/*
========================
VM_bufstr_set
copies a string into selected slot of buffer
void bufstr_set(float bufhandle, float string_index, string str) = #466;
========================
*/
void VM_bufstr_set (void)
{
	size_t alloclen;
	int				strindex;
	prvm_stringbuffer_t *stringbuffer;
	const char		*news;

	VM_SAFEPARMCOUNT(3, VM_bufstr_set);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!stringbuffer)
	{
		VM_Warning("VM_bufstr_set: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	strindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(strindex < 0 || strindex >= 1000000) // huge number of strings
	{
		VM_Warning("VM_bufstr_set: invalid string index %i used in %s\n", strindex, PRVM_NAME);
		return;
	}

	BufStr_Expand(stringbuffer, strindex);
	stringbuffer->num_strings = max(stringbuffer->num_strings, strindex + 1);

	if(stringbuffer->strings[strindex])
		Mem_Free(stringbuffer->strings[strindex]);
	stringbuffer->strings[strindex] = NULL;

	if(PRVM_G_INT(OFS_PARM2))
	{
		// not the NULL string!
		news = PRVM_G_STRING(OFS_PARM2);
		alloclen = strlen(news) + 1;
		stringbuffer->strings[strindex] = (char *)Mem_Alloc(prog->progs_mempool, alloclen);
		memcpy(stringbuffer->strings[strindex], news, alloclen);
	}

	BufStr_Shrink(stringbuffer);
}

/*
========================
VM_bufstr_add
adds string to buffer in first free slot and returns its index
"order == TRUE" means that string will be added after last "full" slot
float bufstr_add(float bufhandle, string str, float order) = #467;
========================
*/
void VM_bufstr_add (void)
{
	int				order, strindex;
	prvm_stringbuffer_t *stringbuffer;
	const char		*string;
	size_t			alloclen;

	VM_SAFEPARMCOUNT(3, VM_bufstr_add);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	if(!stringbuffer)
	{
		VM_Warning("VM_bufstr_add: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(!PRVM_G_INT(OFS_PARM1)) // NULL string
	{
		VM_Warning("VM_bufstr_add: can not add an empty string to buffer %i in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	string = PRVM_G_STRING(OFS_PARM1);
	order = (int)PRVM_G_FLOAT(OFS_PARM2);
	if(order)
		strindex = stringbuffer->num_strings;
	else
		for (strindex = 0;strindex < stringbuffer->num_strings;strindex++)
			if (stringbuffer->strings[strindex] == NULL)
				break;

	BufStr_Expand(stringbuffer, strindex);

	stringbuffer->num_strings = max(stringbuffer->num_strings, strindex + 1);
	alloclen = strlen(string) + 1;
	stringbuffer->strings[strindex] = (char *)Mem_Alloc(prog->progs_mempool, alloclen);
	memcpy(stringbuffer->strings[strindex], string, alloclen);

	PRVM_G_FLOAT(OFS_RETURN) = strindex;
}

/*
========================
VM_bufstr_free
delete string from buffer
void bufstr_free(float bufhandle, float string_index) = #468;
========================
*/
void VM_bufstr_free (void)
{
	int				i;
	prvm_stringbuffer_t	*stringbuffer;
	VM_SAFEPARMCOUNT(2, VM_bufstr_free);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!stringbuffer)
	{
		VM_Warning("VM_bufstr_free: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	i = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(i < 0)
	{
		VM_Warning("VM_bufstr_free: invalid string index %i used in %s\n", i, PRVM_NAME);
		return;
	}

	if (i < stringbuffer->num_strings)
	{
		if(stringbuffer->strings[i])
			Mem_Free(stringbuffer->strings[i]);
		stringbuffer->strings[i] = NULL;
	}

	BufStr_Shrink(stringbuffer);
}







void VM_buf_cvarlist(void)
{
	cvar_t *cvar;
	const char *partial, *antipartial;
	size_t len, antilen;
	size_t alloclen;
	qboolean ispattern, antiispattern;
	int n;
	prvm_stringbuffer_t	*stringbuffer;
	VM_SAFEPARMCOUNTRANGE(2, 3, VM_buf_cvarlist);

	stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, (int)PRVM_G_FLOAT(OFS_PARM0));
	if(!stringbuffer)
	{
		VM_Warning("VM_bufstr_free: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}

	partial = PRVM_G_STRING(OFS_PARM1);
	if(!partial)
		len = 0;
	else
		len = strlen(partial);

	if(prog->argc == 3)
		antipartial = PRVM_G_STRING(OFS_PARM2);
	else
		antipartial = NULL;
	if(!antipartial)
		antilen = 0;
	else
		antilen = strlen(antipartial);
	
	for (n = 0;n < stringbuffer->num_strings;n++)
		if (stringbuffer->strings[n])
			Mem_Free(stringbuffer->strings[n]);
	if (stringbuffer->strings)
		Mem_Free(stringbuffer->strings);
	stringbuffer->strings = NULL;

	ispattern = partial && (strchr(partial, '*') || strchr(partial, '?'));
	antiispattern = antipartial && (strchr(antipartial, '*') || strchr(antipartial, '?'));

	n = 0;
	for(cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if(len && (ispattern ? !matchpattern_with_separator(cvar->name, partial, false, "", false) : strncmp(partial, cvar->name, len)))
			continue;

		if(antilen && (antiispattern ? matchpattern_with_separator(cvar->name, antipartial, false, "", false) : !strncmp(antipartial, cvar->name, antilen)))
			continue;

		++n;
	}

	stringbuffer->max_strings = stringbuffer->num_strings = n;
	if (stringbuffer->max_strings)
		stringbuffer->strings = (char **)Mem_Alloc(prog->progs_mempool, sizeof(stringbuffer->strings[0]) * stringbuffer->max_strings);
	
	n = 0;
	for(cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if(len && (ispattern ? !matchpattern_with_separator(cvar->name, partial, false, "", false) : strncmp(partial, cvar->name, len)))
			continue;

		if(antilen && (antiispattern ? matchpattern_with_separator(cvar->name, antipartial, false, "", false) : !strncmp(antipartial, cvar->name, antilen)))
			continue;

		alloclen = strlen(cvar->name) + 1;
		stringbuffer->strings[n] = (char *)Mem_Alloc(prog->progs_mempool, alloclen);
		memcpy(stringbuffer->strings[n], cvar->name, alloclen);

		++n;
	}
}




//=============

/*
==============
VM_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void VM_changeyaw (void)
{
	prvm_edict_t		*ent;
	float		ideal, current, move, speed;

	// this is called (VERY HACKISHLY) by SV_MoveToGoal, so it can not use any
	// parameters because they are the parameters to SV_MoveToGoal, not this
	//VM_SAFEPARMCOUNT(0, VM_changeyaw);

	ent = PRVM_PROG_TO_EDICT(PRVM_gameglobaledict(self));
	if (ent == prog->edicts)
	{
		VM_Warning("changeyaw: can not modify world entity\n");
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("changeyaw: can not modify free entity\n");
		return;
	}
	current = PRVM_gameedictvector(ent, angles)[1];
	current = ANGLEMOD(current);
	ideal = PRVM_gameedictfloat(ent, ideal_yaw);
	speed = PRVM_gameedictfloat(ent, yaw_speed);

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

	current += move;
	PRVM_gameedictvector(ent, angles)[1] = ANGLEMOD(current);
}

/*
==============
VM_changepitch
==============
*/
void VM_changepitch (void)
{
	prvm_edict_t		*ent;
	float		ideal, current, move, speed;

	VM_SAFEPARMCOUNT(1, VM_changepitch);

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("changepitch: can not modify world entity\n");
		return;
	}
	if (ent->priv.server->free)
	{
		VM_Warning("changepitch: can not modify free entity\n");
		return;
	}
	current = PRVM_gameedictvector(ent, angles)[0];
	current = ANGLEMOD(current);
	ideal = PRVM_gameedictfloat(ent, idealpitch);
	speed = PRVM_gameedictfloat(ent, pitch_speed);

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

	current += move;
	PRVM_gameedictvector(ent, angles)[0] = ANGLEMOD(current);
}


void VM_uncolorstring (void)
{
	char szNewString[VM_STRINGTEMP_LENGTH];
	const char *szString;

	// Prepare Strings
	VM_SAFEPARMCOUNT(1, VM_uncolorstring);
	szString = PRVM_G_STRING(OFS_PARM0);
	COM_StringDecolorize(szString, 0, szNewString, sizeof(szNewString), TRUE);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(szNewString);
	
}

// #221 float(string str, string sub[, float startpos]) strstrofs (FTE_STRINGS)
//strstr, without generating a new string. Use in conjunction with FRIK_FILE's substring for more similar strstr.
void VM_strstrofs (void)
{
	const char *instr, *match;
	int firstofs;
	VM_SAFEPARMCOUNTRANGE(2, 3, VM_strstrofs);
	instr = PRVM_G_STRING(OFS_PARM0);
	match = PRVM_G_STRING(OFS_PARM1);
	firstofs = (prog->argc > 2)?(int)PRVM_G_FLOAT(OFS_PARM2):0;
	firstofs = u8_bytelen(instr, firstofs);

	if (firstofs && (firstofs < 0 || firstofs > (int)strlen(instr)))
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	match = strstr(instr+firstofs, match);
	if (!match)
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = u8_strnlen(instr, match-instr);
}

//#222 string(string s, float index) str2chr (FTE_STRINGS)
void VM_str2chr (void)
{
	const char *s;
	Uchar ch;
	int index;
	VM_SAFEPARMCOUNT(2, VM_str2chr);
	s = PRVM_G_STRING(OFS_PARM0);
	index = u8_bytelen(s, (int)PRVM_G_FLOAT(OFS_PARM1));

	if((unsigned)index < strlen(s))
	{
		if (utf8_enable.integer)
			ch = u8_getchar_noendptr(s + index);
		else
			ch = (unsigned char)s[index];
		PRVM_G_FLOAT(OFS_RETURN) = ch;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = 0;
}

//#223 string(float c, ...) chr2str (FTE_STRINGS)
void VM_chr2str (void)
{
	/*
	char	t[9];
	int		i;
	VM_SAFEPARMCOUNTRANGE(0, 8, VM_chr2str);
	for(i = 0;i < prog->argc && i < (int)sizeof(t) - 1;i++)
		t[i] = (unsigned char)PRVM_G_FLOAT(OFS_PARM0+i*3);
	t[i] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(t);
	*/
	char t[9 * 4 + 1];
	int i;
	size_t len = 0;
	VM_SAFEPARMCOUNTRANGE(0, 8, VM_chr2str);
	for(i = 0; i < prog->argc && len < sizeof(t)-1; ++i)
		len += u8_fromchar((Uchar)PRVM_G_FLOAT(OFS_PARM0+i*3), t + len, sizeof(t)-1);
	t[len] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(t);
}

static int chrconv_number(int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 5:
	case 6:
	case 0:
		break;
	case 1:
		base = '0';
		break;
	case 2:
		base = '0'+128;
		break;
	case 3:
		base = '0'-30;
		break;
	case 4:
		base = '0'+128-30;
		break;
	}
	return i + base;
}
static int chrconv_punct(int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 0:
		break;
	case 1:
		base = 0;
		break;
	case 2:
		base = 128;
		break;
	}
	return i + base;
}

static int chrchar_alpha(int i, int basec, int baset, int convc, int convt, int charnum)
{
	//convert case and colour seperatly...

	i -= baset + basec;
	switch (convt)
	{
	default:
	case 0:
		break;
	case 1:
		baset = 0;
		break;
	case 2:
		baset = 128;
		break;

	case 5:
	case 6:
		baset = 128*((charnum&1) == (convt-5));
		break;
	}

	switch (convc)
	{
	default:
	case 0:
		break;
	case 1:
		basec = 'a';
		break;
	case 2:
		basec = 'A';
		break;
	}
	return i + basec + baset;
}
// #224 string(float ccase, float calpha, float cnum, string s, ...) strconv (FTE_STRINGS)
//bulk convert a string. change case or colouring.
void VM_strconv (void)
{
	int ccase, redalpha, rednum, len, i;
	unsigned char resbuf[VM_STRINGTEMP_LENGTH];
	unsigned char *result = resbuf;

	VM_SAFEPARMCOUNTRANGE(3, 8, VM_strconv);

	ccase = (int) PRVM_G_FLOAT(OFS_PARM0);	//0 same, 1 lower, 2 upper
	redalpha = (int) PRVM_G_FLOAT(OFS_PARM1);	//0 same, 1 white, 2 red,  5 alternate, 6 alternate-alternate
	rednum = (int) PRVM_G_FLOAT(OFS_PARM2);	//0 same, 1 white, 2 red, 3 redspecial, 4 whitespecial, 5 alternate, 6 alternate-alternate
	VM_VarString(3, (char *) resbuf, sizeof(resbuf));
	len = strlen((char *) resbuf);

	for (i = 0; i < len; i++, result++)	//should this be done backwards?
	{
		if (*result >= '0' && *result <= '9')	//normal numbers...
			*result = chrconv_number(*result, '0', rednum);
		else if (*result >= '0'+128 && *result <= '9'+128)
			*result = chrconv_number(*result, '0'+128, rednum);
		else if (*result >= '0'+128-30 && *result <= '9'+128-30)
			*result = chrconv_number(*result, '0'+128-30, rednum);
		else if (*result >= '0'-30 && *result <= '9'-30)
			*result = chrconv_number(*result, '0'-30, rednum);

		else if (*result >= 'a' && *result <= 'z')	//normal numbers...
			*result = chrchar_alpha(*result, 'a', 0, ccase, redalpha, i);
		else if (*result >= 'A' && *result <= 'Z')	//normal numbers...
			*result = chrchar_alpha(*result, 'A', 0, ccase, redalpha, i);
		else if (*result >= 'a'+128 && *result <= 'z'+128)	//normal numbers...
			*result = chrchar_alpha(*result, 'a', 128, ccase, redalpha, i);
		else if (*result >= 'A'+128 && *result <= 'Z'+128)	//normal numbers...
			*result = chrchar_alpha(*result, 'A', 128, ccase, redalpha, i);

		else if ((*result & 127) < 16 || !redalpha)	//special chars..
			*result = *result;
		else if (*result < 128)
			*result = chrconv_punct(*result, 0, redalpha);
		else
			*result = chrconv_punct(*result, 128, redalpha);
	}
	*result = '\0';

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString((char *) resbuf);
}

// #225 string(float chars, string s, ...) strpad (FTE_STRINGS)
void VM_strpad (void)
{
	char src[VM_STRINGTEMP_LENGTH];
	char destbuf[VM_STRINGTEMP_LENGTH];
	int pad;
	VM_SAFEPARMCOUNTRANGE(1, 8, VM_strpad);
	pad = (int) PRVM_G_FLOAT(OFS_PARM0);
	VM_VarString(1, src, sizeof(src));

	// note: < 0 = left padding, > 0 = right padding,
	// this is reverse logic of printf!
	dpsnprintf(destbuf, sizeof(destbuf), "%*s", -pad, src);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(destbuf);
}

// #226 string(string info, string key, string value, ...) infoadd (FTE_STRINGS)
//uses qw style \key\value strings
void VM_infoadd (void)
{
	const char *info, *key;
	char value[VM_STRINGTEMP_LENGTH];
	char temp[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNTRANGE(2, 8, VM_infoadd);
	info = PRVM_G_STRING(OFS_PARM0);
	key = PRVM_G_STRING(OFS_PARM1);
	VM_VarString(2, value, sizeof(value));

	strlcpy(temp, info, VM_STRINGTEMP_LENGTH);

	InfoString_SetValue(temp, VM_STRINGTEMP_LENGTH, key, value);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(temp);
}

// #227 string(string info, string key) infoget (FTE_STRINGS)
//uses qw style \key\value strings
void VM_infoget (void)
{
	const char *info;
	const char *key;
	char value[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT(2, VM_infoget);
	info = PRVM_G_STRING(OFS_PARM0);
	key = PRVM_G_STRING(OFS_PARM1);

	InfoString_GetValue(info, key, value, VM_STRINGTEMP_LENGTH);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(value);
}

//#228 float(string s1, string s2, float len) strncmp (FTE_STRINGS)
// also float(string s1, string s2) strcmp (FRIK_FILE)
void VM_strncmp (void)
{
	const char *s1, *s2;
	VM_SAFEPARMCOUNTRANGE(2, 3, VM_strncmp);
	s1 = PRVM_G_STRING(OFS_PARM0);
	s2 = PRVM_G_STRING(OFS_PARM1);
	if (prog->argc > 2)
	{
		PRVM_G_FLOAT(OFS_RETURN) = strncmp(s1, s2, (size_t)PRVM_G_FLOAT(OFS_PARM2));
	}
	else
	{
		PRVM_G_FLOAT(OFS_RETURN) = strcmp(s1, s2);
	}
}

// #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)
// #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
void VM_strncasecmp (void)
{
	const char *s1, *s2;
	VM_SAFEPARMCOUNTRANGE(2, 3, VM_strncasecmp);
	s1 = PRVM_G_STRING(OFS_PARM0);
	s2 = PRVM_G_STRING(OFS_PARM1);
	if (prog->argc > 2)
	{
		PRVM_G_FLOAT(OFS_RETURN) = strncasecmp(s1, s2, (size_t)PRVM_G_FLOAT(OFS_PARM2));
	}
	else
	{
		PRVM_G_FLOAT(OFS_RETURN) = strcasecmp(s1, s2);
	}
}

// #494 float(float caseinsensitive, string s, ...) crc16
void VM_crc16(void)
{
	float insensitive;
	static char s[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(2, 8, VM_hash);
	insensitive = PRVM_G_FLOAT(OFS_PARM0);
	VM_VarString(1, s, sizeof(s));
	PRVM_G_FLOAT(OFS_RETURN) = (unsigned short) ((insensitive ? CRC_Block_CaseInsensitive : CRC_Block) ((unsigned char *) s, strlen(s)));
}

void VM_wasfreed (void)
{
	VM_SAFEPARMCOUNT(1, VM_wasfreed);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_G_EDICT(OFS_PARM0)->priv.required->free;
}

void VM_SetTraceGlobals(const trace_t *trace)
{
	PRVM_gameglobalfloat(trace_allsolid) = trace->allsolid;
	PRVM_gameglobalfloat(trace_startsolid) = trace->startsolid;
	PRVM_gameglobalfloat(trace_fraction) = trace->fraction;
	PRVM_gameglobalfloat(trace_inwater) = trace->inwater;
	PRVM_gameglobalfloat(trace_inopen) = trace->inopen;
	VectorCopy(trace->endpos, PRVM_gameglobalvector(trace_endpos));
	VectorCopy(trace->plane.normal, PRVM_gameglobalvector(trace_plane_normal));
	PRVM_gameglobalfloat(trace_plane_dist) = trace->plane.dist;
	PRVM_gameglobaledict(trace_ent) = PRVM_EDICT_TO_PROG(trace->ent ? trace->ent : prog->edicts);
	PRVM_gameglobalfloat(trace_dpstartcontents) = trace->startsupercontents;
	PRVM_gameglobalfloat(trace_dphitcontents) = trace->hitsupercontents;
	PRVM_gameglobalfloat(trace_dphitq3surfaceflags) = trace->hitq3surfaceflags;
	PRVM_gameglobalstring(trace_dphittexturename) = trace->hittexture ? PRVM_SetTempString(trace->hittexture->name) : 0;
}

void VM_ClearTraceGlobals(void)
{
	// clean up all trace globals when leaving the VM (anti-triggerbot safeguard)
	PRVM_gameglobalfloat(trace_allsolid) = 0;
	PRVM_gameglobalfloat(trace_startsolid) = 0;
	PRVM_gameglobalfloat(trace_fraction) = 0;
	PRVM_gameglobalfloat(trace_inwater) = 0;
	PRVM_gameglobalfloat(trace_inopen) = 0;
	VectorClear(PRVM_gameglobalvector(trace_endpos));
	VectorClear(PRVM_gameglobalvector(trace_plane_normal));
	PRVM_gameglobalfloat(trace_plane_dist) = 0;
	PRVM_gameglobaledict(trace_ent) = PRVM_EDICT_TO_PROG(prog->edicts);
	PRVM_gameglobalfloat(trace_dpstartcontents) = 0;
	PRVM_gameglobalfloat(trace_dphitcontents) = 0;
	PRVM_gameglobalfloat(trace_dphitq3surfaceflags) = 0;
	PRVM_gameglobalstring(trace_dphittexturename) = 0;
}

//=============

void VM_Cmd_Init(void)
{
	// only init the stuff for the current prog
	VM_Files_Init();
	VM_Search_Init();
	VM_Gecko_Init();
//	VM_BufStr_Init();
}

void VM_Cmd_Reset(void)
{
	CL_PurgeOwner( MENUOWNER );
	VM_Search_Reset();
	VM_Files_CloseAll();
	VM_Gecko_Destroy();
//	VM_BufStr_ShutDown();
}

// #510 string(string input, ...) uri_escape (DP_QC_URI_ESCAPE)
// does URI escaping on a string (replace evil stuff by %AB escapes)
void VM_uri_escape (void)
{
	char src[VM_STRINGTEMP_LENGTH];
	char dest[VM_STRINGTEMP_LENGTH];
	char *p, *q;
	static const char *hex = "0123456789ABCDEF";

	VM_SAFEPARMCOUNTRANGE(1, 8, VM_uri_escape);
	VM_VarString(0, src, sizeof(src));

	for(p = src, q = dest; *p && q < dest + sizeof(dest) - 3; ++p)
	{
		if((*p >= 'A' && *p <= 'Z')
			|| (*p >= 'a' && *p <= 'z')
			|| (*p >= '0' && *p <= '9')
			|| (*p == '-')  || (*p == '_') || (*p == '.')
			|| (*p == '!')  || (*p == '~')
			|| (*p == '\'') || (*p == '(') || (*p == ')'))
			*q++ = *p;
		else
		{
			*q++ = '%';
			*q++ = hex[(*(unsigned char *)p >> 4) & 0xF];
			*q++ = hex[ *(unsigned char *)p       & 0xF];
		}
	}
	*q++ = 0;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(dest);
}

// #510 string(string input, ...) uri_unescape (DP_QC_URI_ESCAPE)
// does URI unescaping on a string (get back the evil stuff)
void VM_uri_unescape (void)
{
	char src[VM_STRINGTEMP_LENGTH];
	char dest[VM_STRINGTEMP_LENGTH];
	char *p, *q;
	int hi, lo;

	VM_SAFEPARMCOUNTRANGE(1, 8, VM_uri_unescape);
	VM_VarString(0, src, sizeof(src));

	for(p = src, q = dest; *p; ) // no need to check size, because unescape can't expand
	{
		if(*p == '%')
		{
			if(p[1] >= '0' && p[1] <= '9')
				hi = p[1] - '0';
			else if(p[1] >= 'a' && p[1] <= 'f')
				hi = p[1] - 'a' + 10;
			else if(p[1] >= 'A' && p[1] <= 'F')
				hi = p[1] - 'A' + 10;
			else
				goto nohex;
			if(p[2] >= '0' && p[2] <= '9')
				lo = p[2] - '0';
			else if(p[2] >= 'a' && p[2] <= 'f')
				lo = p[2] - 'a' + 10;
			else if(p[2] >= 'A' && p[2] <= 'F')
				lo = p[2] - 'A' + 10;
			else
				goto nohex;
			if(hi != 0 || lo != 0) // don't unescape NUL bytes
				*q++ = (char) (hi * 0x10 + lo);
			p += 3;
			continue;
		}

nohex:
		// otherwise:
		*q++ = *p++;
	}
	*q++ = 0;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(dest);
}

// #502 string(string filename) whichpack (DP_QC_WHICHPACK)
// returns the name of the pack containing a file, or "" if it is not in any pack (but local or non-existant)
void VM_whichpack (void)
{
	const char *fn, *pack;

	VM_SAFEPARMCOUNT(1, VM_whichpack);
	fn = PRVM_G_STRING(OFS_PARM0);
	pack = FS_WhichPack(fn);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(pack ? pack : "");
}

typedef struct
{
	int prognr;
	double starttime;
	float id;
	char buffer[MAX_INPUTLINE];
	unsigned char *postdata; // free when uri_to_prog_t is freed
	size_t postlen;
	char *sigdata; // free when uri_to_prog_t is freed
	size_t siglen;
}
uri_to_prog_t;

static void uri_to_string_callback(int status, size_t length_received, unsigned char *buffer, void *cbdata)
{
	uri_to_prog_t *handle = (uri_to_prog_t *) cbdata;

	if(!PRVM_ProgLoaded(handle->prognr))
	{
		// curl reply came too late... so just drop it
		if(handle->postdata)
			Z_Free(handle->postdata);
		if(handle->sigdata)
			Z_Free(handle->sigdata);
		Z_Free(handle);
		return;
	}
		
	PRVM_SetProg(handle->prognr);
	PRVM_Begin;
		if((prog->starttime == handle->starttime) && (PRVM_allfunction(URI_Get_Callback)))
		{
			if(length_received >= sizeof(handle->buffer))
				length_received = sizeof(handle->buffer) - 1;
			handle->buffer[length_received] = 0;
		
			PRVM_G_FLOAT(OFS_PARM0) = handle->id;
			PRVM_G_FLOAT(OFS_PARM1) = status;
			PRVM_G_INT(OFS_PARM2) = PRVM_SetTempString(handle->buffer);
			PRVM_ExecuteProgram(PRVM_allfunction(URI_Get_Callback), "QC function URI_Get_Callback is missing");
		}
	PRVM_End;
	
	if(handle->postdata)
		Z_Free(handle->postdata);
	if(handle->sigdata)
		Z_Free(handle->sigdata);
	Z_Free(handle);
}

// uri_get() gets content from an URL and calls a callback "uri_get_callback" with it set as string; an unique ID of the transfer is returned
// returns 1 on success, and then calls the callback with the ID, 0 or the HTTP status code, and the received data in a string
void VM_uri_get (void)
{
	const char *url;
	float id;
	qboolean ret;
	uri_to_prog_t *handle;
	const char *posttype = NULL;
	const char *postseparator = NULL;
	int poststringbuffer = -1;
	int postkeyid = -1;
	const char *query_string = NULL;
	size_t lq;

	if(!PRVM_allfunction(URI_Get_Callback))
		PRVM_ERROR("uri_get called by %s without URI_Get_Callback defined", PRVM_NAME);

	VM_SAFEPARMCOUNTRANGE(2, 6, VM_uri_get);

	url = PRVM_G_STRING(OFS_PARM0);
	id = PRVM_G_FLOAT(OFS_PARM1);
	if(prog->argc >= 3)
		posttype = PRVM_G_STRING(OFS_PARM2);
	if(prog->argc >= 4)
		postseparator = PRVM_G_STRING(OFS_PARM3);
	if(prog->argc >= 5)
		poststringbuffer = PRVM_G_FLOAT(OFS_PARM4);
	if(prog->argc >= 6)
		postkeyid = PRVM_G_FLOAT(OFS_PARM5);
	handle = (uri_to_prog_t *) Z_Malloc(sizeof(*handle)); // this can't be the prog's mem pool, as curl may call the callback later!

	query_string = strchr(url, '?');
	if(query_string)
		++query_string;
	lq = query_string ? strlen(query_string) : 0;

	handle->prognr = PRVM_GetProgNr();
	handle->starttime = prog->starttime;
	handle->id = id;
	if(postseparator)
	{
		size_t l = strlen(postseparator);
		if(poststringbuffer >= 0)
		{
			size_t ltotal;
			int i;
			// "implode"
			prvm_stringbuffer_t *stringbuffer;
			stringbuffer = (prvm_stringbuffer_t *)Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, poststringbuffer);
			if(!stringbuffer)
			{
				VM_Warning("uri_get: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
				return;
			}
			ltotal = 0;
			for(i = 0;i < stringbuffer->num_strings;i++)
			{
				if(i > 0)
					ltotal += l;
				if(stringbuffer->strings[i])
					ltotal += strlen(stringbuffer->strings[i]);
			}
			handle->postdata = (unsigned char *)Z_Malloc(ltotal + 1 + lq);
			handle->postlen = ltotal;
			ltotal = 0;
			for(i = 0;i < stringbuffer->num_strings;i++)
			{
				if(i > 0)
				{
					memcpy(handle->postdata + ltotal, postseparator, l);
					ltotal += l;
				}
				if(stringbuffer->strings[i])
				{
					memcpy(handle->postdata + ltotal, stringbuffer->strings[i], strlen(stringbuffer->strings[i]));
					ltotal += strlen(stringbuffer->strings[i]);
				}
			}
			if(ltotal != handle->postlen)
				PRVM_ERROR ("%s: string buffer content size mismatch, possible overrun", PRVM_NAME);
		}
		else
		{
			handle->postdata = (unsigned char *)Z_Malloc(l + 1 + lq);
			handle->postlen = l;
			memcpy(handle->postdata, postseparator, l);
		}
		handle->postdata[handle->postlen] = 0;
		if(query_string)
			memcpy(handle->postdata + handle->postlen + 1, query_string, lq);
		if(postkeyid >= 0)
		{
			// POST: we sign postdata \0 query string
			size_t ll;
			handle->sigdata = (char *)Z_Malloc(8192);
			strlcpy(handle->sigdata, "X-D0-Blind-ID-Detached-Signature: ", 8192);
			l = strlen(handle->sigdata);
			handle->siglen = Crypto_SignDataDetached(handle->postdata, handle->postlen + 1 + lq, postkeyid, handle->sigdata + l, 8192 - l);
			if(!handle->siglen)
			{
				Z_Free(handle->sigdata);
				handle->sigdata = NULL;
				goto out1;
			}
			ll = base64_encode((unsigned char *) (handle->sigdata + l), handle->siglen, 8192 - l - 1);
			if(!ll)
			{
				Z_Free(handle->sigdata);
				handle->sigdata = NULL;
				goto out1;
			}
			handle->siglen = l + ll;
			handle->sigdata[handle->siglen] = 0;
		}
out1:
		ret = Curl_Begin_ToMemory_POST(url, handle->sigdata, 0, posttype, handle->postdata, handle->postlen, (unsigned char *) handle->buffer, sizeof(handle->buffer), uri_to_string_callback, handle);
	}
	else
	{
		if(postkeyid >= 0 && query_string)
		{
			// GET: we sign JUST the query string
			size_t l, ll;
			handle->sigdata = (char *)Z_Malloc(8192);
			strlcpy(handle->sigdata, "X-D0-Blind-ID-Detached-Signature: ", 8192);
			l = strlen(handle->sigdata);
			handle->siglen = Crypto_SignDataDetached(query_string, lq, postkeyid, handle->sigdata + l, 8192 - l);
			if(!handle->siglen)
			{
				Z_Free(handle->sigdata);
				handle->sigdata = NULL;
				goto out2;
			}
			ll = base64_encode((unsigned char *) (handle->sigdata + l), handle->siglen, 8192 - l - 1);
			if(!ll)
			{
				Z_Free(handle->sigdata);
				handle->sigdata = NULL;
				goto out2;
			}
			handle->siglen = l + ll;
			handle->sigdata[handle->siglen] = 0;
		}
out2:
		handle->postdata = NULL;
		handle->postlen = 0;
		ret = Curl_Begin_ToMemory(url, 0, (unsigned char *) handle->buffer, sizeof(handle->buffer), uri_to_string_callback, handle);
	}
	if(ret)
	{
		PRVM_G_INT(OFS_RETURN) = 1;
	}
	else
	{
		if(handle->postdata)
			Z_Free(handle->postdata);
		if(handle->sigdata)
			Z_Free(handle->sigdata);
		Z_Free(handle);
		PRVM_G_INT(OFS_RETURN) = 0;
	}
}

void VM_netaddress_resolve (void)
{
	const char *ip;
	char normalized[128];
	int port;
	lhnetaddress_t addr;

	VM_SAFEPARMCOUNTRANGE(1, 2, VM_netaddress_resolve);

	ip = PRVM_G_STRING(OFS_PARM0);
	port = 0;
	if(prog->argc > 1)
		port = (int) PRVM_G_FLOAT(OFS_PARM1);

	if(LHNETADDRESS_FromString(&addr, ip, port) && LHNETADDRESS_ToString(&addr, normalized, sizeof(normalized), prog->argc > 1))
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(normalized);
	else
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString("");
}

//string(void) getextresponse = #624; // returns the next extResponse packet that was sent to this client
void VM_CL_getextresponse (void)
{
	VM_SAFEPARMCOUNT(0,VM_argv);

	if (cl_net_extresponse_count <= 0)
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	else
	{
		int first;
		--cl_net_extresponse_count;
		first = (cl_net_extresponse_last + NET_EXTRESPONSE_MAX - cl_net_extresponse_count) % NET_EXTRESPONSE_MAX;
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(cl_net_extresponse[first]);
	}
}

void VM_SV_getextresponse (void)
{
	VM_SAFEPARMCOUNT(0,VM_argv);

	if (sv_net_extresponse_count <= 0)
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	else
	{
		int first;
		--sv_net_extresponse_count;
		first = (sv_net_extresponse_last + NET_EXTRESPONSE_MAX - sv_net_extresponse_count) % NET_EXTRESPONSE_MAX;
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(sv_net_extresponse[first]);
	}
}

/*
=========
Common functions between menu.dat and clsprogs
=========
*/

//#349 float() isdemo 
void VM_CL_isdemo (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_isdemo);
	PRVM_G_FLOAT(OFS_RETURN) = cls.demoplayback;
}

//#355 float() videoplaying 
void VM_CL_videoplaying (void)
{
	VM_SAFEPARMCOUNT(0, VM_CL_videoplaying);
	PRVM_G_FLOAT(OFS_RETURN) = cl_videoplaying;
}

/*
=========
VM_M_callfunction

	callfunction(...,string function_name)
Extension: pass
=========
*/
mfunction_t *PRVM_ED_FindFunction (const char *name);
void VM_callfunction(void)
{
	mfunction_t *func;
	const char *s;

	VM_SAFEPARMCOUNTRANGE(1, 8, VM_callfunction);

	s = PRVM_G_STRING(OFS_PARM0+(prog->argc - 1)*3);

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_ERROR("VM_callfunciton: function %s not found !", s);
	else if (func->first_statement < 0)
	{
		// negative statements are built in functions
		int builtinnumber = -func->first_statement;
		prog->xfunction->builtinsprofile++;
		if (builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber])
			prog->builtins[builtinnumber]();
		else
			PRVM_ERROR("No such builtin #%i in %s; most likely cause: outdated engine build. Try updating!", builtinnumber, PRVM_NAME);
	}
	else if(func - prog->functions > 0)
	{
		prog->argc--;
		PRVM_ExecuteProgram(func - prog->functions,"");
		prog->argc++;
	}
}

/*
=========
VM_isfunction

float	isfunction(string function_name)
=========
*/
mfunction_t *PRVM_ED_FindFunction (const char *name);
void VM_isfunction(void)
{
	mfunction_t *func;
	const char *s;

	VM_SAFEPARMCOUNT(1, VM_isfunction);

	s = PRVM_G_STRING(OFS_PARM0);

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_G_FLOAT(OFS_RETURN) = false;
	else
		PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
VM_sprintf

string sprintf(string format, ...)
=========
*/

void VM_sprintf(void)
{
	const char *s, *s0;
	char outbuf[MAX_INPUTLINE];
	char *o = outbuf, *end = outbuf + sizeof(outbuf), *err;
	int argpos = 1;
	int width, precision, thisarg, flags;
	char formatbuf[16];
	char *f;
	int isfloat;
	static int dummyivec[3] = {0, 0, 0};
	static float dummyvec[3] = {0, 0, 0};

#define PRINTF_ALTERNATE 1
#define PRINTF_ZEROPAD 2
#define PRINTF_LEFT 4
#define PRINTF_SPACEPOSITIVE 8
#define PRINTF_SIGNPOSITIVE 16

	formatbuf[0] = '%';

	s = PRVM_G_STRING(OFS_PARM0);

#define GETARG_FLOAT(a) (((a)>=1 && (a)<prog->argc) ? (PRVM_G_FLOAT(OFS_PARM0 + 3 * (a))) : 0)
#define GETARG_VECTOR(a) (((a)>=1 && (a)<prog->argc) ? (PRVM_G_VECTOR(OFS_PARM0 + 3 * (a))) : dummyvec)
#define GETARG_INT(a) (((a)>=1 && (a)<prog->argc) ? (PRVM_G_INT(OFS_PARM0 + 3 * (a))) : 0)
#define GETARG_INTVECTOR(a) (((a)>=1 && (a)<prog->argc) ? ((int*) PRVM_G_VECTOR(OFS_PARM0 + 3 * (a))) : dummyivec)
#define GETARG_STRING(a) (((a)>=1 && (a)<prog->argc) ? (PRVM_G_STRING(OFS_PARM0 + 3 * (a))) : "")

	for(;;)
	{
		s0 = s;
		switch(*s)
		{
			case 0:
				goto finished;
			case '%':
				++s;

				if(*s == '%')
					goto verbatim;

				// complete directive format:
				// %3$*1$.*2$ld
				
				width = -1;
				precision = -1;
				thisarg = -1;
				flags = 0;
				isfloat = -1;

				// is number following?
				if(*s >= '0' && *s <= '9')
				{
					width = strtol(s, &err, 10);
					if(!err)
					{
						VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
						goto finished;
					}
					if(*err == '$')
					{
						thisarg = width;
						width = -1;
						s = err + 1;
					}
					else
					{
						if(*s == '0')
						{
							flags |= PRINTF_ZEROPAD;
							if(width == 0)
								width = -1; // it was just a flag
						}
						s = err;
					}
				}

				if(width < 0)
				{
					for(;;)
					{
						switch(*s)
						{
							case '#': flags |= PRINTF_ALTERNATE; break;
							case '0': flags |= PRINTF_ZEROPAD; break;
							case '-': flags |= PRINTF_LEFT; break;
							case ' ': flags |= PRINTF_SPACEPOSITIVE; break;
							case '+': flags |= PRINTF_SIGNPOSITIVE; break;
							default:
								goto noflags;
						}
						++s;
					}
noflags:
					if(*s == '*')
					{
						++s;
						if(*s >= '0' && *s <= '9')
						{
							width = strtol(s, &err, 10);
							if(!err || *err != '$')
							{
								VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
								goto finished;
							}
							s = err + 1;
						}
						else
							width = argpos++;
						width = GETARG_FLOAT(width);
						if(width < 0)
						{
							flags |= PRINTF_LEFT;
							width = -width;
						}
					}
					else if(*s >= '0' && *s <= '9')
					{
						width = strtol(s, &err, 10);
						if(!err)
						{
							VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
							goto finished;
						}
						s = err;
						if(width < 0)
						{
							flags |= PRINTF_LEFT;
							width = -width;
						}
					}
					// otherwise width stays -1
				}

				if(*s == '.')
				{
					++s;
					if(*s == '*')
					{
						++s;
						if(*s >= '0' && *s <= '9')
						{
							precision = strtol(s, &err, 10);
							if(!err || *err != '$')
							{
								VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
								goto finished;
							}
							s = err + 1;
						}
						else
							precision = argpos++;
						precision = GETARG_FLOAT(precision);
					}
					else if(*s >= '0' && *s <= '9')
					{
						precision = strtol(s, &err, 10);
						if(!err)
						{
							VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
							goto finished;
						}
						s = err;
					}
					else
					{
						VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
						goto finished;
					}
				}

				for(;;)
				{
					switch(*s)
					{
						case 'h': isfloat = 1; break;
						case 'l': isfloat = 0; break;
						case 'L': isfloat = 0; break;
						case 'j': break;
						case 'z': break;
						case 't': break;
						default:
							goto nolength;
					}
					++s;
				}
nolength:

				// now s points to the final directive char and is no longer changed
				if(isfloat < 0)
				{
					if(*s == 'i')
						isfloat = 0;
					else
						isfloat = 1;
				}

				if(thisarg < 0)
					thisarg = argpos++;

				if(o < end - 1)
				{
					f = &formatbuf[1];
					if(*s != 's' && *s != 'c')
						if(flags & PRINTF_ALTERNATE) *f++ = '#';
					if(flags & PRINTF_ZEROPAD) *f++ = '0';
					if(flags & PRINTF_LEFT) *f++ = '-';
					if(flags & PRINTF_SPACEPOSITIVE) *f++ = ' ';
					if(flags & PRINTF_SIGNPOSITIVE) *f++ = '+';
					*f++ = '*';
					if(precision >= 0)
					{
						*f++ = '.';
						*f++ = '*';
					}
					*f++ = *s;
					*f++ = 0;

					if(width < 0) // not set
						width = 0;

					switch(*s)
					{
						case 'd': case 'i':
							if(precision < 0) // not set
								o += dpsnprintf(o, end - o, formatbuf, width, (isfloat ? (int) GETARG_FLOAT(thisarg) : (int) GETARG_INT(thisarg)));
							else
								o += dpsnprintf(o, end - o, formatbuf, width, precision, (isfloat ? (int) GETARG_FLOAT(thisarg) : (int) GETARG_INT(thisarg)));
							break;
						case 'o': case 'u': case 'x': case 'X':
							if(precision < 0) // not set
								o += dpsnprintf(o, end - o, formatbuf, width, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							else
								o += dpsnprintf(o, end - o, formatbuf, width, precision, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							break;
						case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
							if(precision < 0) // not set
								o += dpsnprintf(o, end - o, formatbuf, width, (isfloat ? (double) GETARG_FLOAT(thisarg) : (double) GETARG_INT(thisarg)));
							else
								o += dpsnprintf(o, end - o, formatbuf, width, precision, (isfloat ? (double) GETARG_FLOAT(thisarg) : (double) GETARG_INT(thisarg)));
							break;
						case 'v': case 'V':
							f[-2] += 'g' - 'v';
							if(precision < 0) // not set
								o += dpsnprintf(o, end - o, va("%s %s %s", /* NESTED SPRINTF IS NESTED */ formatbuf, formatbuf, formatbuf),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[0] : (double) GETARG_INTVECTOR(thisarg)[0]),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[1] : (double) GETARG_INTVECTOR(thisarg)[1]),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[2] : (double) GETARG_INTVECTOR(thisarg)[2])
								);
							else
								o += dpsnprintf(o, end - o, va("%s %s %s", /* NESTED SPRINTF IS NESTED */ formatbuf, formatbuf, formatbuf),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[0] : (double) GETARG_INTVECTOR(thisarg)[0]),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[1] : (double) GETARG_INTVECTOR(thisarg)[1]),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[2] : (double) GETARG_INTVECTOR(thisarg)[2])
								);
							break;
						case 'c':
							if(flags & PRINTF_ALTERNATE)
							{
								if(precision < 0) // not set
									o += dpsnprintf(o, end - o, formatbuf, width, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
								else
									o += dpsnprintf(o, end - o, formatbuf, width, precision, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							}
							else
							{
								unsigned int c = (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg));
								const char *buf = u8_encodech(c, NULL);
								if(!buf)
									buf = "";
								if(precision < 0) // not set
									precision = end - o - 1;
								o += u8_strpad(o, end - o, buf, (flags & PRINTF_LEFT) != 0, width, precision);
							}
							break;
						case 's':
							if(flags & PRINTF_ALTERNATE)
							{
								if(precision < 0) // not set
									o += dpsnprintf(o, end - o, formatbuf, width, GETARG_STRING(thisarg));
								else
									o += dpsnprintf(o, end - o, formatbuf, width, precision, GETARG_STRING(thisarg));
							}
							else
							{
								if(precision < 0) // not set
									precision = end - o - 1;
								o += u8_strpad(o, end - o, GETARG_STRING(thisarg), (flags & PRINTF_LEFT) != 0, width, precision);
							}
							break;
						default:
							VM_Warning("VM_sprintf: invalid directive in %s: %s\n", PRVM_NAME, s0);
							goto finished;
					}
				}
				++s;
				break;
			default:
verbatim:
				if(o < end - 1)
					*o++ = *s++;
				break;
		}
	}
finished:
	*o = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(outbuf);
}


// surface querying

static dp_model_t *getmodel(prvm_edict_t *ed)
{
	switch(PRVM_GetProgNr())
	{
		case PRVM_SERVERPROG:
			return SV_GetModelFromEdict(ed);
		case PRVM_CLIENTPROG:
			return CL_GetModelFromEdict(ed);
		default:
			return NULL;
	}
}

typedef struct
{
	unsigned int progid;
	dp_model_t *model;
	frameblend_t frameblend[MAX_FRAMEBLENDS];
	skeleton_t *skeleton_p;
	skeleton_t skeleton;
	float *data_vertex3f;
	float *data_svector3f;
	float *data_tvector3f;
	float *data_normal3f;
	int max_vertices;
	float *buf_vertex3f;
	float *buf_svector3f;
	float *buf_tvector3f;
	float *buf_normal3f;
}
animatemodel_cache_t;
static animatemodel_cache_t animatemodel_cache;

void animatemodel(dp_model_t *model, prvm_edict_t *ed)
{
	skeleton_t *skeleton;
	int skeletonindex = -1;
	qboolean need = false;
	if(!(model->surfmesh.isanimated && model->AnimateVertices))
	{
		animatemodel_cache.data_vertex3f = model->surfmesh.data_vertex3f;
		animatemodel_cache.data_svector3f = model->surfmesh.data_svector3f;
		animatemodel_cache.data_tvector3f = model->surfmesh.data_tvector3f;
		animatemodel_cache.data_normal3f = model->surfmesh.data_normal3f;
		return;
	}
	if(animatemodel_cache.progid != prog->id)
		memset(&animatemodel_cache, 0, sizeof(animatemodel_cache));
	need |= (animatemodel_cache.model != model);
	VM_GenerateFrameGroupBlend(ed->priv.server->framegroupblend, ed);
	VM_FrameBlendFromFrameGroupBlend(ed->priv.server->frameblend, ed->priv.server->framegroupblend, model);
	need |= (memcmp(&animatemodel_cache.frameblend, &ed->priv.server->frameblend, sizeof(ed->priv.server->frameblend))) != 0;
	skeletonindex = (int)PRVM_gameedictfloat(ed, skeletonindex) - 1;
	if (!(skeletonindex >= 0 && skeletonindex < MAX_EDICTS && (skeleton = prog->skeletons[skeletonindex]) && skeleton->model->num_bones == ed->priv.server->skeleton.model->num_bones))
		skeleton = NULL;
	need |= (animatemodel_cache.skeleton_p != skeleton);
	if(skeleton)
		need |= (memcmp(&animatemodel_cache.skeleton, skeleton, sizeof(ed->priv.server->skeleton))) != 0;
	if(!need)
		return;
	if(model->surfmesh.num_vertices > animatemodel_cache.max_vertices)
	{
		animatemodel_cache.max_vertices = model->surfmesh.num_vertices * 2;
		if(animatemodel_cache.buf_vertex3f) Mem_Free(animatemodel_cache.buf_vertex3f);
		if(animatemodel_cache.buf_svector3f) Mem_Free(animatemodel_cache.buf_svector3f);
		if(animatemodel_cache.buf_tvector3f) Mem_Free(animatemodel_cache.buf_tvector3f);
		if(animatemodel_cache.buf_normal3f) Mem_Free(animatemodel_cache.buf_normal3f);
		animatemodel_cache.buf_vertex3f = (float *)Mem_Alloc(prog->progs_mempool, sizeof(float[3]) * animatemodel_cache.max_vertices);
		animatemodel_cache.buf_svector3f = (float *)Mem_Alloc(prog->progs_mempool, sizeof(float[3]) * animatemodel_cache.max_vertices);
		animatemodel_cache.buf_tvector3f = (float *)Mem_Alloc(prog->progs_mempool, sizeof(float[3]) * animatemodel_cache.max_vertices);
		animatemodel_cache.buf_normal3f = (float *)Mem_Alloc(prog->progs_mempool, sizeof(float[3]) * animatemodel_cache.max_vertices);
	}
	animatemodel_cache.data_vertex3f = animatemodel_cache.buf_vertex3f;
	animatemodel_cache.data_svector3f = animatemodel_cache.buf_svector3f;
	animatemodel_cache.data_tvector3f = animatemodel_cache.buf_tvector3f;
	animatemodel_cache.data_normal3f = animatemodel_cache.buf_normal3f;
	VM_UpdateEdictSkeleton(ed, model, ed->priv.server->frameblend);
	model->AnimateVertices(model, ed->priv.server->frameblend, &ed->priv.server->skeleton, animatemodel_cache.data_vertex3f, animatemodel_cache.data_normal3f, animatemodel_cache.data_svector3f, animatemodel_cache.data_tvector3f);
	animatemodel_cache.progid = prog->id;
	animatemodel_cache.model = model;
	memcpy(&animatemodel_cache.frameblend, &ed->priv.server->frameblend, sizeof(ed->priv.server->frameblend));
	animatemodel_cache.skeleton_p = skeleton;
	if(skeleton)
		memcpy(&animatemodel_cache.skeleton, skeleton, sizeof(ed->priv.server->skeleton));
}

static void getmatrix(prvm_edict_t *ed, matrix4x4_t *out)
{
	switch(PRVM_GetProgNr())
	{
		case PRVM_SERVERPROG:
			SV_GetEntityMatrix(ed, out, false);
			break;
		case PRVM_CLIENTPROG:
			CL_GetEntityMatrix(ed, out, false);
			break;
		default:
			*out = identitymatrix;
			break;
	}
}

static void applytransform_forward(const vec3_t in, prvm_edict_t *ed, vec3_t out)
{
	matrix4x4_t m;
	getmatrix(ed, &m);
	Matrix4x4_Transform(&m, in, out);
}

static void applytransform_forward_direction(const vec3_t in, prvm_edict_t *ed, vec3_t out)
{
	matrix4x4_t m;
	getmatrix(ed, &m);
	Matrix4x4_Transform3x3(&m, in, out);
}

static void applytransform_inverted(const vec3_t in, prvm_edict_t *ed, vec3_t out)
{
	matrix4x4_t m, n;
	getmatrix(ed, &m);
	Matrix4x4_Invert_Full(&n, &m);
	Matrix4x4_Transform3x3(&n, in, out);
}

static void applytransform_forward_normal(const vec3_t in, prvm_edict_t *ed, vec3_t out)
{
	matrix4x4_t m;
	float p[4];
	getmatrix(ed, &m);
	Matrix4x4_TransformPositivePlane(&m, in[0], in[1], in[2], 0, p);
	VectorCopy(p, out);
}

static void clippointtosurface(prvm_edict_t *ed, dp_model_t *model, msurface_t *surface, vec3_t p, vec3_t out)
{
	int i, j, k;
	float *v[3], facenormal[3], edgenormal[3], sidenormal[3], temp[3], offsetdist, dist, bestdist;
	const int *e;
	animatemodel(model, ed);
	bestdist = 1000000000;
	VectorCopy(p, out);
	for (i = 0, e = (model->surfmesh.data_element3i + 3 * surface->num_firsttriangle);i < surface->num_triangles;i++, e += 3)
	{
		// clip original point to each triangle of the surface and find the
		// triangle that is closest
		v[0] = animatemodel_cache.data_vertex3f + e[0] * 3;
		v[1] = animatemodel_cache.data_vertex3f + e[1] * 3;
		v[2] = animatemodel_cache.data_vertex3f + e[2] * 3;
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

static msurface_t *getsurface(dp_model_t *model, int surfacenum)
{
	if (surfacenum < 0 || surfacenum >= model->nummodelsurfaces)
		return NULL;
	return model->data_surfaces + surfacenum + model->firstmodelsurface;
}


//PF_getsurfacenumpoints, // #434 float(entity e, float s) getsurfacenumpoints = #434;
void VM_getsurfacenumpoints(void)
{
	dp_model_t *model;
	msurface_t *surface;
	VM_SAFEPARMCOUNT(2, VM_getsurfacenumpoints);
	// return 0 if no such surface
	if (!(model = getmodel(PRVM_G_EDICT(OFS_PARM0))) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	// note: this (incorrectly) assumes it is a simple polygon
	PRVM_G_FLOAT(OFS_RETURN) = surface->num_vertices;
}
//PF_getsurfacepoint,     // #435 vector(entity e, float s, float n) getsurfacepoint = #435;
void VM_getsurfacepoint(void)
{
	prvm_edict_t *ed;
	dp_model_t *model;
	msurface_t *surface;
	int pointnum;
	VM_SAFEPARMCOUNT(3, VM_getsurfacepoint);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!(model = getmodel(ed)) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// note: this (incorrectly) assumes it is a simple polygon
	pointnum = (int)PRVM_G_FLOAT(OFS_PARM2);
	if (pointnum < 0 || pointnum >= surface->num_vertices)
		return;
	animatemodel(model, ed);
	applytransform_forward(&(animatemodel_cache.data_vertex3f + 3 * surface->num_firstvertex)[pointnum * 3], ed, PRVM_G_VECTOR(OFS_RETURN));
}
//PF_getsurfacepointattribute,     // #486 vector(entity e, float s, float n, float a) getsurfacepointattribute = #486;
// float SPA_POSITION = 0;
// float SPA_S_AXIS = 1;
// float SPA_T_AXIS = 2;
// float SPA_R_AXIS = 3; // same as SPA_NORMAL
// float SPA_TEXCOORDS0 = 4;
// float SPA_LIGHTMAP0_TEXCOORDS = 5;
// float SPA_LIGHTMAP0_COLOR = 6;
void VM_getsurfacepointattribute(void)
{
	prvm_edict_t *ed;
	dp_model_t *model;
	msurface_t *surface;
	int pointnum;
	int attributetype;

	VM_SAFEPARMCOUNT(4, VM_getsurfacepoint);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!(model = getmodel(ed)) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	pointnum = (int)PRVM_G_FLOAT(OFS_PARM2);
	if (pointnum < 0 || pointnum >= surface->num_vertices)
		return;
	attributetype = (int) PRVM_G_FLOAT(OFS_PARM3);

	animatemodel(model, ed);

	switch( attributetype ) {
		// float SPA_POSITION = 0;
		case 0:
			applytransform_forward(&(animatemodel_cache.data_vertex3f + 3 * surface->num_firstvertex)[pointnum * 3], ed, PRVM_G_VECTOR(OFS_RETURN));
			break;
		// float SPA_S_AXIS = 1;
		case 1:
			applytransform_forward_direction(&(animatemodel_cache.data_svector3f + 3 * surface->num_firstvertex)[pointnum * 3], ed, PRVM_G_VECTOR(OFS_RETURN));
			break;
		// float SPA_T_AXIS = 2;
		case 2:
			applytransform_forward_direction(&(animatemodel_cache.data_tvector3f + 3 * surface->num_firstvertex)[pointnum * 3], ed, PRVM_G_VECTOR(OFS_RETURN));
			break;
		// float SPA_R_AXIS = 3; // same as SPA_NORMAL
		case 3:
			applytransform_forward_direction(&(animatemodel_cache.data_normal3f + 3 * surface->num_firstvertex)[pointnum * 3], ed, PRVM_G_VECTOR(OFS_RETURN));
			break;
		// float SPA_TEXCOORDS0 = 4;
		case 4: {
			float *ret = PRVM_G_VECTOR(OFS_RETURN);
			float *texcoord = &(model->surfmesh.data_texcoordtexture2f + 2 * surface->num_firstvertex)[pointnum * 2];
			ret[0] = texcoord[0];
			ret[1] = texcoord[1];
			ret[2] = 0.0f;
			break;
		}
		// float SPA_LIGHTMAP0_TEXCOORDS = 5;
		case 5: {
			float *ret = PRVM_G_VECTOR(OFS_RETURN);
			float *texcoord = &(model->surfmesh.data_texcoordlightmap2f + 2 * surface->num_firstvertex)[pointnum * 2];
			ret[0] = texcoord[0];
			ret[1] = texcoord[1];
			ret[2] = 0.0f;
			break;
		}
		// float SPA_LIGHTMAP0_COLOR = 6;
		case 6:
			// ignore alpha for now..
			VectorCopy( &(model->surfmesh.data_lightmapcolor4f + 4 * surface->num_firstvertex)[pointnum * 4], PRVM_G_VECTOR(OFS_RETURN));
			break;
		default:
			VectorSet( PRVM_G_VECTOR(OFS_RETURN), 0.0f, 0.0f, 0.0f );
			break;
	}
}
//PF_getsurfacenormal,    // #436 vector(entity e, float s) getsurfacenormal = #436;
void VM_getsurfacenormal(void)
{
	dp_model_t *model;
	msurface_t *surface;
	vec3_t normal;
	VM_SAFEPARMCOUNT(2, VM_getsurfacenormal);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	if (!(model = getmodel(PRVM_G_EDICT(OFS_PARM0))) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	// note: this only returns the first triangle, so it doesn't work very
	// well for curved surfaces or arbitrary meshes
	animatemodel(model, PRVM_G_EDICT(OFS_PARM0));
	TriangleNormal((animatemodel_cache.data_vertex3f + 3 * surface->num_firstvertex), (animatemodel_cache.data_vertex3f + 3 * surface->num_firstvertex) + 3, (animatemodel_cache.data_vertex3f + 3 * surface->num_firstvertex) + 6, normal);
	applytransform_forward_normal(normal, PRVM_G_EDICT(OFS_PARM0), PRVM_G_VECTOR(OFS_RETURN));
	VectorNormalize(PRVM_G_VECTOR(OFS_RETURN));
}
//PF_getsurfacetexture,   // #437 string(entity e, float s) getsurfacetexture = #437;
void VM_getsurfacetexture(void)
{
	dp_model_t *model;
	msurface_t *surface;
	VM_SAFEPARMCOUNT(2, VM_getsurfacetexture);
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	if (!(model = getmodel(PRVM_G_EDICT(OFS_PARM0))) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(surface->texture->name);
}
//PF_getsurfacenearpoint, // #438 float(entity e, vector p) getsurfacenearpoint = #438;
void VM_getsurfacenearpoint(void)
{
	int surfacenum, best;
	vec3_t clipped, p;
	vec_t dist, bestdist;
	prvm_edict_t *ed;
	dp_model_t *model;
	msurface_t *surface;
	vec_t *point;
	VM_SAFEPARMCOUNT(2, VM_getsurfacenearpoint);
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	ed = PRVM_G_EDICT(OFS_PARM0);
	point = PRVM_G_VECTOR(OFS_PARM1);

	if (!ed || ed->priv.server->free)
		return;
	model = getmodel(ed);
	if (!model || !model->num_surfaces)
		return;

	animatemodel(model, ed);

	applytransform_inverted(point, ed, p);
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
			clippointtosurface(ed, model, surface, p, clipped);
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
void VM_getsurfaceclippedpoint(void)
{
	prvm_edict_t *ed;
	dp_model_t *model;
	msurface_t *surface;
	vec3_t p, out;
	VM_SAFEPARMCOUNT(3, VM_te_getsurfaceclippedpoint);
	VectorClear(PRVM_G_VECTOR(OFS_RETURN));
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!(model = getmodel(ed)) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
		return;
	animatemodel(model, ed);
	applytransform_inverted(PRVM_G_VECTOR(OFS_PARM2), ed, p);
	clippointtosurface(ed, model, surface, p, out);
	VectorAdd(out, PRVM_serveredictvector(ed, origin), PRVM_G_VECTOR(OFS_RETURN));
}

//PF_getsurfacenumtriangles, // #??? float(entity e, float s) getsurfacenumtriangles = #???;
void VM_getsurfacenumtriangles(void)
{
       dp_model_t *model;
       msurface_t *surface;
       VM_SAFEPARMCOUNT(2, VM_SV_getsurfacenumtriangles);
       // return 0 if no such surface
       if (!(model = getmodel(PRVM_G_EDICT(OFS_PARM0))) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
       {
               PRVM_G_FLOAT(OFS_RETURN) = 0;
               return;
       }

       // note: this (incorrectly) assumes it is a simple polygon
       PRVM_G_FLOAT(OFS_RETURN) = surface->num_triangles;
}
//PF_getsurfacetriangle,     // #??? vector(entity e, float s, float n) getsurfacetriangle = #???;
void VM_getsurfacetriangle(void)
{
       const vec3_t d = {-1, -1, -1};
       prvm_edict_t *ed;
       dp_model_t *model;
       msurface_t *surface;
       int trinum;
       VM_SAFEPARMCOUNT(3, VM_SV_getsurfacetriangle);
       VectorClear(PRVM_G_VECTOR(OFS_RETURN));
       ed = PRVM_G_EDICT(OFS_PARM0);
       if (!(model = getmodel(ed)) || !(surface = getsurface(model, (int)PRVM_G_FLOAT(OFS_PARM1))))
               return;
       trinum = (int)PRVM_G_FLOAT(OFS_PARM2);
       if (trinum < 0 || trinum >= surface->num_triangles)
               return;
       // FIXME: implement rotation/scaling
       VectorMA(&(model->surfmesh.data_element3i + 3 * surface->num_firsttriangle)[trinum * 3], surface->num_firstvertex, d, PRVM_G_VECTOR(OFS_RETURN));
}

//
// physics builtins
//

void World_Physics_ApplyCmd(prvm_edict_t *ed, edict_odefunc_t *f);

#define VM_physics_ApplyCmd(ed,f) if (!ed->priv.server->ode_body) VM_physics_newstackfunction(ed, f); else World_Physics_ApplyCmd(ed, f)

edict_odefunc_t *VM_physics_newstackfunction(prvm_edict_t *ed, edict_odefunc_t *f)
{
	edict_odefunc_t *newfunc, *func;

	newfunc = (edict_odefunc_t *)Mem_Alloc(prog->progs_mempool, sizeof(edict_odefunc_t));
	memcpy(newfunc, f, sizeof(edict_odefunc_t));
	newfunc->next = NULL;
	if (!ed->priv.server->ode_func)
		ed->priv.server->ode_func = newfunc;
	else
	{
		for (func = ed->priv.server->ode_func; func->next; func = func->next);
		func->next = newfunc;
	}
	return newfunc;
}

// void(entity e, float physics_enabled) physics_enable = #;
void VM_physics_enable(void)
{
	prvm_edict_t *ed;
	edict_odefunc_t f;
	
	VM_SAFEPARMCOUNT(2, VM_physics_enable);
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed)
	{
		if (developer.integer > 0)
			VM_Warning("VM_physics_enable: null entity!\n");
		return;
	}
	// entity should have MOVETYPE_PHYSICS already set, this can damage memory (making leaked allocation) so warn about this even if non-developer
	if (PRVM_serveredictfloat(ed, movetype) != MOVETYPE_PHYSICS)
	{
		VM_Warning("VM_physics_enable: entity is not MOVETYPE_PHYSICS!\n");
		return;
	}
	f.type = PRVM_G_FLOAT(OFS_PARM1) == 0 ? ODEFUNC_DISABLE : ODEFUNC_ENABLE;
	VM_physics_ApplyCmd(ed, &f);
}

// void(entity e, vector force, vector relative_ofs) physics_addforce = #;
void VM_physics_addforce(void)
{
	prvm_edict_t *ed;
	edict_odefunc_t f;
	
	VM_SAFEPARMCOUNT(3, VM_physics_addforce);
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed)
	{
		if (developer.integer > 0)
			VM_Warning("VM_physics_addforce: null entity!\n");
		return;
	}
	// entity should have MOVETYPE_PHYSICS already set, this can damage memory (making leaked allocation) so warn about this even if non-developer
	if (PRVM_serveredictfloat(ed, movetype) != MOVETYPE_PHYSICS)
	{
		VM_Warning("VM_physics_addforce: entity is not MOVETYPE_PHYSICS!\n");
		return;
	}
	f.type = ODEFUNC_RELFORCEATPOS;
	VectorCopy(PRVM_G_VECTOR(OFS_PARM1), f.v1);
	VectorSubtract(PRVM_serveredictvector(ed, origin), PRVM_G_VECTOR(OFS_PARM2), f.v2);
	VM_physics_ApplyCmd(ed, &f);
}

// void(entity e, vector torque) physics_addtorque = #;
void VM_physics_addtorque(void)
{
	prvm_edict_t *ed;
	edict_odefunc_t f;
	
	VM_SAFEPARMCOUNT(2, VM_physics_addtorque);
	ed = PRVM_G_EDICT(OFS_PARM0);
	if (!ed)
	{
		if (developer.integer > 0)
			VM_Warning("VM_physics_addtorque: null entity!\n");
		return;
	}
	// entity should have MOVETYPE_PHYSICS already set, this can damage memory (making leaked allocation) so warn about this even if non-developer
	if (PRVM_serveredictfloat(ed, movetype) != MOVETYPE_PHYSICS)
	{
		VM_Warning("VM_physics_addtorque: entity is not MOVETYPE_PHYSICS!\n");
		return;
	}
	f.type = ODEFUNC_RELTORQUE;
	VectorCopy(PRVM_G_VECTOR(OFS_PARM1), f.v1);
	VM_physics_ApplyCmd(ed, &f);
}
