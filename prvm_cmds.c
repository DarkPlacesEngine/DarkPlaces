// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin and extension lists can be found here
// cause large (I think they will) parts are from pr_cmds the same copyright like in pr_cmds
// also applies here

#include "prvm_cmds.h"

//============================================================================
// Common

// temp string handling
// LordHavoc: added this to semi-fix the problem of using many ftos calls in a print
static char vm_string_temp[VM_STRINGTEMP_BUFFERS][VM_STRINGTEMP_LENGTH];
static int vm_string_tempindex = 0;

// qc file handling
#define MAX_VMFILES		256
#define MAX_PRVMFILES	MAX_VMFILES * PRVM_MAXPROGS
#define VM_FILES ((qfile_t**)(vm_files + PRVM_GetProgNr() * MAX_VMFILES))

qfile_t *vm_files[MAX_PRVMFILES];

// qc fs search handling
#define MAX_VMSEARCHES 128
#define TOTAL_VMSEARCHES MAX_VMSEARCHES * PRVM_MAXPROGS
#define VM_SEARCHLIST ((fssearch_t**)(vm_fssearchlist + PRVM_GetProgNr() * MAX_VMSEARCHES))

fssearch_t *vm_fssearchlist[TOTAL_VMSEARCHES];

char *VM_GetTempString(void)
{
	char *s;
	s = vm_string_temp[vm_string_tempindex];
	vm_string_tempindex = (vm_string_tempindex + 1) % VM_STRINGTEMP_BUFFERS;
	return s;
}

void VM_CheckEmptyString (const char *s)
{
	if (s[0] <= ' ')
		PRVM_ERROR ("%s: Bad string", PRVM_NAME);
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
	char *e, *start;
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
			return true;
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
	Con_Printf("======%S ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
	if(prog->self)
	{
		ed = PRVM_G_EDICT(prog->self->ofs);
		PRVM_ED_Print(ed);
	}

	PRVM_ERROR ("%s: Program error", PRVM_NAME);
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
	Con_Printf("======%s OBJECT ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
	if(prog->self)
	{
		ed = PRVM_G_EDICT (prog->self->ofs);
		PRVM_ED_Print(ed);

		PRVM_ED_Free (ed);
	}
	else
		// objerror has to display the object fields -> else call
		PRVM_ERROR ("VM_objecterror: self not defined !");
}

/*
=================
VM_print (actually used only by client and menu)

print to console

print(string)
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
		Con_Printf("VM_bprint: game is not server(%s) !\n", PRVM_NAME);
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

	//find client for this entity
	clientnum = PRVM_G_FLOAT(OFS_PARM0);
	if (!sv.active  || clientnum < 0 || clientnum >= svs.maxclients || !svs.clients[clientnum].active)
	{
		Con_Printf("VM_sprint: %s: invalid client or server is not active !\n", PRVM_NAME);
		return;
	}

	client = svs.clients + clientnum;
	VM_VarString(1, string, sizeof(string));
	MSG_WriteChar(&client->message,svc_print);
	MSG_WriteString(&client->message, string);
}

/*
=================
VM_centerprint

single print to the screen

centerprint(clientent, value)
=================
*/
void VM_centerprint (void)
{
	char string[VM_STRINGTEMP_LENGTH];

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

vector vectoangles(vector)
=================
*/
void VM_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;

	VM_SAFEPARMCOUNT(1,VM_vectoangles);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

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

	PRVM_G_FLOAT(OFS_RETURN+0) = pitch;
	PRVM_G_FLOAT(OFS_RETURN+1) = yaw;
	PRVM_G_FLOAT(OFS_RETURN+2) = 0;
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
/*
void PF_sound (void)
{
	char		*sample;
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
		Host_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Host_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Host_Error ("SV_StartSound: channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}
*/

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
		Con_Printf("VM_localsound: Failed to play %s for %s !\n", s, PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = -4;
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
	VM_SAFEPARMCOUNT(1,VM_localcmd);
	VM_VarString(0, string, sizeof(string));
	Cbuf_AddText(string);
}

/*
=================
VM_cvar

float cvar (string)
=================
*/
void VM_cvar (void)
{
	VM_SAFEPARMCOUNT(1,VM_cvar);

	PRVM_G_FLOAT(OFS_RETURN) = Cvar_VariableValue(PRVM_G_STRING(OFS_PARM0));
}

/*
=================
VM_cvar_string

const string	VM_cvar_string (string)
=================
*/
void VM_cvar_string(void)
{
	char *out;
	const char *name;
	const char *cvar_string;
	VM_SAFEPARMCOUNT(1,VM_cvar_string);

	name = PRVM_G_STRING(OFS_PARM0);

	if(!name)
		PRVM_ERROR("VM_cvar_string: %s: null string", PRVM_NAME);

	VM_CheckEmptyString(name);

	out = VM_GetTempString();

	cvar_string = Cvar_VariableString(name);

	strcpy(out, cvar_string);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(out);
}


/*
========================
VM_cvar_defstring

const string	VM_cvar_defstring (string)
========================
*/
void VM_cvar_defstring (void)
{
	char *out;
	const char *name;
	const char *cvar_string;
	VM_SAFEPARMCOUNT(1,VM_cvar_string);

	name = PRVM_G_STRING(OFS_PARM0);

	if(!name)
		PRVM_ERROR("VM_cvar_defstring: %s: null string", PRVM_NAME);

	VM_CheckEmptyString(name);

	out = VM_GetTempString();

	cvar_string = Cvar_VariableDefString(name);

	strcpy(out, cvar_string);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(out);
}
/*
=================
VM_cvar_set

void cvar_set (string,string)
=================
*/
void VM_cvar_set (void)
{
	VM_SAFEPARMCOUNT(2,VM_cvar_set);

	Cvar_Set(PRVM_G_STRING(OFS_PARM0), PRVM_G_STRING(OFS_PARM1));
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
	if (developer.integer)
	{
		VM_VarString(0, string, sizeof(string));
#if 1
		Con_Printf("%s", string);
#else
		Con_Printf("%s: %s", PRVM_NAME, string);
#endif
	}
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
	char *s;

	VM_SAFEPARMCOUNT(1, VM_ftos);

	v = PRVM_G_FLOAT(OFS_PARM0);

	s = VM_GetTempString();
	if ((float)((int)v) == v)
		sprintf(s, "%i", (int)v);
	else
		sprintf(s, "%f", v);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
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
	char *s;

	VM_SAFEPARMCOUNT(1,VM_vtos);

	s = VM_GetTempString();
	sprintf (s, "'%5.1f %5.1f %5.1f'", PRVM_G_VECTOR(OFS_PARM0)[0], PRVM_G_VECTOR(OFS_PARM0)[1], PRVM_G_VECTOR(OFS_PARM0)[2]);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
}

/*
=========
VM_etos

string	etos(entity)
=========
*/

void VM_etos (void)
{
	char *s;

	VM_SAFEPARMCOUNT(1, VM_etos);

	s = VM_GetTempString();
	sprintf (s, "entity %i", PRVM_G_EDICTNUM(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
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
VM_itoe

intt ftoi(float num)
========================
*/
void VM_ftoi(void)
{
	int ent;
	VM_SAFEPARMCOUNT(1, VM_ftoi);

	ent = PRVM_G_FLOAT(OFS_PARM0);
	if(PRVM_PROG_TO_EDICT(ent)->priv.required->free)
		PRVM_ERROR ("VM_ftoe: %s tried to access a freed entity (entity %i)!", PRVM_NAME, ent);

	PRVM_G_INT(OFS_RETURN) = ent;
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
	if( PRVM_NUM_FOR_EDICT(ed) <= prog->reserved_edicts ) {
		Con_DPrint( "VM_remove: tried to remove the null entity or a reserved entity!\n" );
	} else if( ed->priv.required->free ) {
		Con_DPrint( "VM_remove: tried to remove an already freed entity!\n" );
	} else {
		PRVM_ED_Free (ed);
	}
//	if (ed == prog->edicts)
//		PRVM_ERROR ("remove: tried to remove world");
//	if (PRVM_NUM_FOR_EDICT(ed) <= sv.maxclients)
//		Host_Error("remove: tried to remove a client");
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

	if (!s || !s[0])
	{
		// return reserved edict 0 (could be used for whatever the prog wants)
		VM_RETURN_EDICT(prog->edicts);
		return;
	}

	for (e++ ; e < prog->num_edicts ; e++)
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if (ed->priv.required->free)
			continue;
		t = PRVM_E_STRING(ed,f);
		if (!t)
			continue;
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
	int		chain_of;
	const char	*s, *t;
	prvm_edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2,VM_findchain);

	// is the same like !(prog->flag & PRVM_FE_CHAIN) - even if the operator precedence is another
	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchain: %s doesnt have a chain field !", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;

	chain = prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = PRVM_G_STRING(OFS_PARM1);
	if (!s || !s[0])
	{
		VM_RETURN_EDICT(prog->edicts);
		return;
	}

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.required->free)
			continue;
		t = PRVM_E_STRING(ent,f);
		if (!t)
			continue;
		if (strcmp(t,s))
			continue;

		PRVM_E_INT(ent,chain_of) = PRVM_NUM_FOR_EDICT(chain);
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
	int		chain_of;
	float	s;
	prvm_edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2, VM_findchainfloat);

	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchainfloat: %s doesnt have a chain field !", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;

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

		PRVM_E_INT(ent,chain_of) = PRVM_EDICT_TO_PROG(chain);
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
	int		chain_of;
	prvm_edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2, VM_findchainflags);

	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchainflags: %s doesnt have a chain field !", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;

	chain = (prvm_edict_t *)prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = (int)PRVM_G_FLOAT(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.required->free)
			continue;
		if (!((int)PRVM_E_FLOAT(ent,f) & s))
			continue;

		PRVM_E_INT(ent,chain_of) = PRVM_EDICT_TO_PROG(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
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

	PRVM_ED_PrintNum (PRVM_G_EDICTNUM(OFS_PARM0));
}

/*
=========
VM_rint

float	rint(float)
=========
*/
void VM_rint (void)
{
	float	f;

	VM_SAFEPARMCOUNT(1,VM_rint);

	f = PRVM_G_FLOAT(OFS_PARM0);
	if (f > 0)
		PRVM_G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		PRVM_G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
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
	const char	*s;

	VM_SAFEPARMCOUNT(1, VM_changelevel);

	if(!sv.active)
	{
		Con_Printf("VM_changelevel: game is not server (%s)\n", PRVM_NAME);
		return;
	}

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = PRVM_G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
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

float	registercvar (string name, string value, float flags)
=========
*/
void VM_registercvar (void)
{
	const char *name, *value;
	int	flags;

	VM_SAFEPARMCOUNT(3,VM_registercvar);

	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_STRING(OFS_PARM1);
	flags = PRVM_G_FLOAT(OFS_PARM2);
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	if(flags > CVAR_MAXFLAGSVAL)
		return;

// first check to see if it has already been defined
	if (Cvar_FindVar (name))
		return;

// check for overlap with a command
	if (Cmd_Exists (name))
	{
		Con_Printf("VM_registercvar: %s is a command\n", name);
		return;
	}

	Cvar_Get(name, value, flags);

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
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (prog->argc == 2)
		PRVM_G_FLOAT(OFS_RETURN) = min(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
	else if (prog->argc >= 3)
	{
		int i;
		float f = PRVM_G_FLOAT(OFS_PARM0);
		for (i = 1;i < prog->argc;i++)
			if (PRVM_G_FLOAT((OFS_PARM0+i*3)) < f)
				f = PRVM_G_FLOAT((OFS_PARM0+i*3));
		PRVM_G_FLOAT(OFS_RETURN) = f;
	}
	else
		PRVM_ERROR("VM_min: %s must supply at least 2 floats", PRVM_NAME);
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
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (prog->argc == 2)
		PRVM_G_FLOAT(OFS_RETURN) = max(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
	else if (prog->argc >= 3)
	{
		int i;
		float f = PRVM_G_FLOAT(OFS_PARM0);
		for (i = 1;i < prog->argc;i++)
			if (PRVM_G_FLOAT((OFS_PARM0+i*3)) > f)
				f = PRVM_G_FLOAT((OFS_PARM0+i*3));
		PRVM_G_FLOAT(OFS_RETURN) = f;
	}
	else
		PRVM_ERROR("VM_max: %s must supply at least 2 floats", PRVM_NAME);
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

/*
=================
VM_copyentity

copies data from one entity to another

copyentity(entity src, entity dst)
=================
*/
void VM_copyentity (void)
{
	prvm_edict_t *in, *out;
	VM_SAFEPARMCOUNT(2,VM_copyentity);
	in = PRVM_G_EDICT(OFS_PARM0);
	out = PRVM_G_EDICT(OFS_PARM1);
	memcpy(out->fields.vp, in->fields.vp, prog->progs->entityfields * 4);
}

/*
=================
VM_setcolor

sets the color of a client and broadcasts the update to all connected clients

setcolor(clientent, value)
=================
*/
/*void PF_setcolor (void)
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
	if ((val = PRVM_GETEDICTFIELDVALUE(client->edict, eval_clientcolors)))
		val->_float = i;
	client->colors = i;
	client->old_colors = i;
	client->edict->fields.server->team = (i & 15) + 1;

	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, entnum - 1);
	MSG_WriteByte (&sv.reliable_datagram, i);
}*/

void VM_Files_Init(void)
{
	memset(VM_FILES, 0, sizeof(qfile_t*[MAX_VMFILES]));
}

void VM_Files_CloseAll(void)
{
	int i;
	for (i = 0;i < MAX_VMFILES;i++)
	{
		if (VM_FILES[i])
			FS_Close(VM_FILES[i]);
		//VM_FILES[i] = NULL;
	}
	memset(VM_FILES,0,sizeof(qfile_t*[MAX_VMFILES])); // this should be faster (is it ?)
}

qfile_t *VM_GetFileHandle( int index )
{
	if (index < 0 || index >= MAX_VMFILES)
	{
		Con_Printf("VM_GetFileHandle: invalid file handle %i used in %s\n", index, PRVM_NAME);
		return NULL;
	}
	if (VM_FILES[index] == NULL)
	{
		Con_Printf("VM_GetFileHandle: no such file handle %i (or file has been closed) in %s\n", index, PRVM_NAME);
		return NULL;
	}
	return VM_FILES[index];
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

	for (filenum = 0;filenum < MAX_VMFILES;filenum++)
		if (VM_FILES[filenum] == NULL)
			break;
	if (filenum >= MAX_VMFILES)
	{
		Con_Printf("VM_fopen: %s ran out of file handles (%i)\n", PRVM_NAME, MAX_VMFILES);
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		return;
	}
	mode = PRVM_G_FLOAT(OFS_PARM1);
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
		Con_Printf("VM_fopen: %s: no such mode %i (valid: 0 = read, 1 = append, 2 = write)\n", PRVM_NAME, mode);
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		return;
	}
	filename = PRVM_G_STRING(OFS_PARM0);

	VM_FILES[filenum] = FS_Open(va("data/%s", filename), modestring, false, false);
	if (VM_FILES[filenum] == NULL && mode == 0)
		VM_FILES[filenum] = FS_Open(va("%s", filename), modestring, false, false);

	if (VM_FILES[filenum] == NULL)
	{
		if (developer.integer)
			Con_Printf("VM_fopen: %s: %s mode %s failed\n", PRVM_NAME, filename, modestring);
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	}
	else
	{
		if (developer.integer)
			Con_Printf("VM_fopen: %s: %s mode %s opened as #%i\n", PRVM_NAME, filename, modestring, filenum);
		PRVM_G_FLOAT(OFS_RETURN) = filenum;
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

	filenum = PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= MAX_VMFILES)
	{
		Con_Printf("VM_fclose: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (VM_FILES[filenum] == NULL)
	{
		Con_Printf("VM_fclose: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (developer.integer)
		Con_Printf("VM_fclose: %s: #%i closed\n", PRVM_NAME, filenum);
	FS_Close(VM_FILES[filenum]);
	VM_FILES[filenum] = NULL;
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
	static char string[VM_STRINGTEMP_LENGTH];
	int filenum;

	VM_SAFEPARMCOUNT(1,VM_fgets);

	filenum = PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= MAX_VMFILES)
	{
		Con_Printf("VM_fgets: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (VM_FILES[filenum] == NULL)
	{
		Con_Printf("VM_fgets: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	end = 0;
	for (;;)
	{
		c = FS_Getc(VM_FILES[filenum]);
		if (c == '\r' || c == '\n' || c < 0)
			break;
		if (end < VM_STRINGTEMP_LENGTH - 1)
			string[end++] = c;
	}
	string[end] = 0;
	// remove \n following \r
	if (c == '\r')
	{
		c = FS_Getc(VM_FILES[filenum]);
		if (c != '\n')
			FS_UnGetc(VM_FILES[filenum], (unsigned char)c);
	}
	if (developer.integer >= 3)
		Con_Printf("fgets: %s: %s\n", PRVM_NAME, string);
	if (c >= 0 || end)
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(string);
	else
		PRVM_G_INT(OFS_RETURN) = 0;
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

	filenum = PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= MAX_VMFILES)
	{
		Con_Printf("VM_fputs: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (VM_FILES[filenum] == NULL)
	{
		Con_Printf("VM_fputs: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	VM_VarString(1, string, sizeof(string));
	if ((stringlength = (int)strlen(string)))
		FS_Write(VM_FILES[filenum], string, stringlength);
	if (developer.integer)
		Con_Printf("fputs: %s: %s\n", PRVM_NAME, string);
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
	const char *s;

	VM_SAFEPARMCOUNT(1,VM_strlen);

	s = PRVM_G_STRING(OFS_PARM0);
	if (s)
		PRVM_G_FLOAT(OFS_RETURN) = strlen(s);
	else
		PRVM_G_FLOAT(OFS_RETURN) = 0;
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
	char *s;

	if(prog->argc < 1)
		PRVM_ERROR("VM_strcat wrong parameter count (min. 1 expected ) !");

	s = VM_GetTempString();
	VM_VarString(0, s, VM_STRINGTEMP_LENGTH);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
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
	int i, start, length;
	const char *s;
	char *string;

	VM_SAFEPARMCOUNT(3,VM_substring);

	string = VM_GetTempString();
	s = PRVM_G_STRING(OFS_PARM0);
	start = PRVM_G_FLOAT(OFS_PARM1);
	length = PRVM_G_FLOAT(OFS_PARM2);
	if (!s)
		s = "";
	for (i = 0;i < start && *s;i++, s++);
	for (i = 0;i < VM_STRINGTEMP_LENGTH - 1 && *s && i < length;i++, s++)
		string[i] = *s;
	string[i] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(string);
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

	VM_SAFEPARMCOUNT(1,VM_strzone);

	VM_VarString(0, string, sizeof(string));
	PRVM_G_INT(OFS_RETURN) = PRVM_AllocString(strlen(string) + 1, &out);
	strcpy(out, string);
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

	i = PRVM_G_FLOAT(OFS_PARM0);
	if (!sv.active  || i < 0 || i >= svs.maxclients || !svs.clients[i].active)
	{
		Con_Printf("VM_clientcommand: %s: invalid client/server is not active !\n", PRVM_NAME);
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
int num_tokens = 0;
char *tokens[256], tokenbuf[MAX_INPUTLINE];
void VM_tokenize (void)
{
	size_t pos;
	const char *p;

	VM_SAFEPARMCOUNT(1,VM_tokenize);

	p = PRVM_G_STRING(OFS_PARM0);

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

	PRVM_G_FLOAT(OFS_RETURN) = num_tokens;
}

//string(float n) argv = #442; // returns a word from the tokenized string (returns nothing for an invalid index)
//this function originally written by KrimZon, made shorter by LordHavoc
void VM_argv (void)
{
	int token_num;

	VM_SAFEPARMCOUNT(1,VM_argv);

	token_num = PRVM_G_FLOAT(OFS_PARM0);

	if (token_num >= 0 && token_num < num_tokens)
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(tokens[token_num]);
	else
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(NULL);
}

/*
//void(entity e, entity tagentity, string tagname) setattachment = #443; // attachs e to a tag on tagentity (note: use "" to attach to entity origin/angles instead of a tag)
void PF_setattachment (void)
{
	prvm_edict_t *e = PRVM_G_EDICT(OFS_PARM0);
	prvm_edict_t *tagentity = PRVM_G_EDICT(OFS_PARM1);
	char *tagname = PRVM_G_STRING(OFS_PARM2);
	prvm_eval_t *v;
	int i, modelindex;
	model_t *model;

	if (tagentity == NULL)
		tagentity = prog->edicts;

	v = PRVM_GETEDICTFIELDVALUE(e, eval_tag_entity);
	if (v)
		fields.server->edict = PRVM_EDICT_TO_PROG(tagentity);

	v = PRVM_GETEDICTFIELDVALUE(e, eval_tag_index);
	if (v)
		fields.server->_float = 0;
	if (tagentity != NULL && tagentity != prog->edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->fields.server->modelindex;
		if (modelindex >= 0 && modelindex < MAX_MODELS)
		{
			model = sv.models[modelindex];
			if (model->data_overridetagnamesforskin && (unsigned int)tagentity->fields.server->skin < (unsigned int)model->numskins && model->data_overridetagnamesforskin[(unsigned int)tagentity->fields.server->skin].num_overridetagnames)
				for (i = 0;i < model->data_overridetagnamesforskin[(unsigned int)tagentity->fields.server->skin].num_overridetagnames;i++)
					if (!strcmp(tagname, model->data_overridetagnamesforskin[(unsigned int)tagentity->fields.server->skin].data_overridetagnames[i].name))
						fields.server->_float = i + 1;
			// FIXME: use a model function to get tag info (need to handle skeletal)
			if (fields.server->_float == 0 && model->num_tags)
				for (i = 0;i < model->num_tags;i++)
					if (!strcmp(tagname, model->data_tags[i].name))
						fields.server->_float = i + 1;
			if (fields.server->_float == 0)
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity));
	}
}*/

/*
=========
VM_isserver

float	isserver()
=========
*/
void VM_isserver(void)
{
	VM_SAFEPARMCOUNT(0,VM_serverstate);

	PRVM_G_FLOAT(OFS_RETURN) = sv.active;
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

	PRVM_G_FLOAT(OFS_RETURN) = cls.state;
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

#ifdef _WIN32
	PRVM_G_FLOAT(OFS_RETURN) = 0;
#elif defined _MAC
	PRVM_G_FLOAT(OFS_RETURN) = 2;
#else
	PRVM_G_FLOAT(OFS_RETURN) = 1;
#endif
}

/*
=========
VM_getmousepos

vector	getmousepos()
=========
*/
void VM_getmousepos(void)
{

	VM_SAFEPARMCOUNT(0,VM_getmousepos);

	PRVM_G_VECTOR(OFS_RETURN)[0] = in_mouse_x * vid_conwidth.integer / vid.width;
	PRVM_G_VECTOR(OFS_RETURN)[1] = in_mouse_y * vid_conheight.integer / vid.height;
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
VM_gettime

float	gettime(void)
=========
*/
void VM_gettime(void)
{
	VM_SAFEPARMCOUNT(0,VM_gettime);

	PRVM_G_FLOAT(OFS_RETURN) = (float) *prog->time;
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
	if (!COM_ParseToken(&data, false) || com_token[0] != '{' )
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
	// .. is parent directory on many platforms
	// / is parent directory on Amiga
	// : is root of drive on Amiga (also used as a directory separator on Mac, but / works there too, so that's a bad idea)
	// \ is a windows-ism (so it's naughty to use it, / works on all platforms)
	if ((filename[0] == '.' && filename[1] == '.') || filename[0] == '/' || strrchr(filename, ':') || strrchr(filename, '\\'))
	{
		Con_Printf("VM_loadfromfile: %s dangerous or non-portable filename \"%s\" not allowed. (contains : or \\ or begins with .. or /)\n", PRVM_NAME, filename);
		PRVM_G_FLOAT(OFS_RETURN) = -4;
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
	memset(VM_SEARCHLIST,0,sizeof(fssearch_t*[MAX_VMSEARCHES]));
}

void VM_Search_Reset(void)
{
	int i;
	// reset the fssearch list
	for(i = 0; i < MAX_VMSEARCHES; i++)
		if(VM_SEARCHLIST[i])
			FS_FreeSearch(VM_SEARCHLIST[i]);
	memset(VM_SEARCHLIST,0,sizeof(fssearch_t*[MAX_VMSEARCHES]));
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

	caseinsens = PRVM_G_FLOAT(OFS_PARM1);
	quiet = PRVM_G_FLOAT(OFS_PARM2);

	for(handle = 0; handle < MAX_VMSEARCHES; handle++)
		if(!VM_SEARCHLIST[handle])
			break;

	if(handle >= MAX_VMSEARCHES)
	{
		Con_Printf("VM_search_begin: %s ran out of search handles (%i)\n", PRVM_NAME, MAX_VMSEARCHES);
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		return;
	}

	if(!(VM_SEARCHLIST[handle] = FS_Search(pattern,caseinsens, quiet)))
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = handle;
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

	handle = PRVM_G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= MAX_VMSEARCHES)
	{
		Con_Printf("VM_search_end: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(VM_SEARCHLIST[handle] == NULL)
	{
		Con_Printf("VM_search_end: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}

	FS_FreeSearch(VM_SEARCHLIST[handle]);
	VM_SEARCHLIST[handle] = NULL;
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

	handle = PRVM_G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= MAX_VMSEARCHES)
	{
		Con_Printf("VM_search_getsize: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(VM_SEARCHLIST[handle] == NULL)
	{
		Con_Printf("VM_search_getsize: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}

	PRVM_G_FLOAT(OFS_RETURN) = VM_SEARCHLIST[handle]->numfilenames;
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
	char *tmp;
	VM_SAFEPARMCOUNT(2, VM_search_getfilename);

	handle = PRVM_G_FLOAT(OFS_PARM0);
	filenum = PRVM_G_FLOAT(OFS_PARM1);

	if(handle < 0 || handle >= MAX_VMSEARCHES)
	{
		Con_Printf("VM_search_getfilename: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(VM_SEARCHLIST[handle] == NULL)
	{
		Con_Printf("VM_search_getfilename: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}
	if(filenum < 0 || filenum >= VM_SEARCHLIST[handle]->numfilenames)
	{
		Con_Printf("VM_search_getfilename: invalid filenum %i in %s\n", filenum, PRVM_NAME);
		return;
	}

	tmp = VM_GetTempString();
	strcpy(tmp, VM_SEARCHLIST[handle]->filenames[filenum]);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(tmp);
}

/*
=========
VM_chr

string	chr(float ascii)
=========
*/
void VM_chr(void)
{
	char *tmp;
	VM_SAFEPARMCOUNT(1, VM_chr);

	tmp = VM_GetTempString();
	tmp[0] = (unsigned char) PRVM_G_FLOAT(OFS_PARM0);
	tmp[1] = 0;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(tmp);
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

	if(!s)
		PRVM_ERROR ("VM_precache_pic: %s: NULL", PRVM_NAME);

	VM_CheckEmptyString (s);

	// AK Draw_CachePic is supposed to always return a valid pointer
	if( Draw_CachePic(s, false)->tex == r_texture_notexture )
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(NULL);
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

	if(!s)
		PRVM_ERROR ("VM_freepic: %s: NULL");

	VM_CheckEmptyString (s);

	Draw_FreePic(s);
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
	VM_SAFEPARMCOUNT(6,VM_drawcharacter);

	character = (char) PRVM_G_FLOAT(OFS_PARM1);
	if(character == 0)
	{
		Con_Printf("VM_drawcharacter: %s passed null character !\n",PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	flag = (int)PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		Con_Printf("VM_drawcharacter: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		return;
	}

	if(pos[2] || scale[2])
		Con_Printf("VM_drawcharacter: z value%c from %s discarded\n",(pos[2] && scale[2]) ? 's' : 0,((pos[2] && scale[2]) ? "pos and scale" : (pos[2] ? "pos" : "scale")));

	if(!scale[0] || !scale[1])
	{
		Con_Printf("VM_drawcharacter: scale %s is null !\n", (scale[0] == 0) ? ((scale[1] == 0) ? "x and y" : "x") : "y");
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		return;
	}

	DrawQ_String (pos[0], pos[1], &character, 1, scale[0], scale[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag);
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
	VM_SAFEPARMCOUNT(6,VM_drawstring);

	string = PRVM_G_STRING(OFS_PARM1);
	if(!string)
	{
		Con_Printf("VM_drawstring: %s passed null string !\n",PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	//VM_CheckEmptyString(string); Why should it be checked - perhaps the menu wants to support the precolored letters, too?

	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	flag = (int)PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		Con_Printf("VM_drawstring: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		return;
	}

	if(!scale[0] || !scale[1])
	{
		Con_Printf("VM_drawstring: scale %s is null !\n", (scale[0] == 0) ? ((scale[1] == 0) ? "x and y" : "x") : "y");
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		return;
	}

	if(pos[2] || scale[2])
		Con_Printf("VM_drawstring: z value%c from %s discarded\n",(pos[2] && scale[2]) ? 's' : 0,((pos[2] && scale[2]) ? "pos and scale" : (pos[2] ? "pos" : "scale")));

	DrawQ_String (pos[0], pos[1], string, 0, scale[0], scale[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag);
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}
/*
=========
VM_drawpic

float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag)
=========
*/
void VM_drawpic(void)
{
	const char *pic;
	float *size, *pos, *rgb;
	int flag;

	VM_SAFEPARMCOUNT(6,VM_drawpic);

	pic = PRVM_G_STRING(OFS_PARM1);

	if(!pic)
	{
		Con_Printf("VM_drawpic: %s passed null picture name !\n", PRVM_NAME);
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	VM_CheckEmptyString (pic);

	// is pic cached ? no function yet for that
	if(!1)
	{
		Con_Printf("VM_drawpic: %s: %s not cached !\n", PRVM_NAME, pic);
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	flag = (int) PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
	{
		Con_Printf("VM_drawstring: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		return;
	}

	if(pos[2] || size[2])
		Con_Printf("VM_drawstring: z value%c from %s discarded\n",(pos[2] && size[2]) ? 's' : 0,((pos[2] && size[2]) ? "pos and size" : (pos[2] ? "pos" : "size")));

	DrawQ_Pic(pos[0], pos[1], pic, size[0], size[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag);
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
		Con_Printf("VM_drawstring: %s: wrong DRAWFLAG %i !\n",PRVM_NAME,flag);
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		return;
	}

	if(pos[2] || size[2])
		Con_Printf("VM_drawstring: z value%c from %s discarded\n",(pos[2] && size[2]) ? 's' : 0,((pos[2] && size[2]) ? "pos and size" : (pos[2] ? "pos" : "size")));

	DrawQ_Pic(pos[0], pos[1], 0, size[0], size[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM3), flag);
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

	if(!p)
		PRVM_ERROR("VM_getimagepos: %s passed null picture name !", PRVM_NAME);

	VM_CheckEmptyString (p);

	pic = Draw_CachePic (p, false);

	PRVM_G_VECTOR(OFS_RETURN)[0] = pic->width;
	PRVM_G_VECTOR(OFS_RETURN)[1] = pic->height;
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
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

	if( CL_OpenVideo( file, name, MENUOWNER ) )
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

	CL_CloseVideo( CL_GetVideo( name ) );
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

	video = CL_GetVideo( name );
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

	video = CL_GetVideo( name );
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

	video = CL_GetVideo( name );
	if( video )
		CL_RestartVideo( video );
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
	char *outstr, *out;
	const char *instr, *in;
	int size;

	VM_SAFEPARMCOUNT( 1, VM_altstr_prepare );

	instr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( instr );
	outstr = VM_GetTempString();

	for( out = outstr, in = instr, size = VM_STRINGTEMP_LENGTH - 1 ; size && *in ; size--, in++, out++ )
		if( *in == '\'' ) {
			*out++ = '\\';
			*out = '\'';
			size--;
		} else
			*out = *in;
	*out = 0;

	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
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
	char *outstr, *out;
	int count, size;

	VM_SAFEPARMCOUNT( 2, VM_altstr_get );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	count = PRVM_G_FLOAT( OFS_PARM1 );
	count = count * 2 + 1;

	for( pos = altstr ; *pos && count ; pos++ )
		if( *pos == '\\' ) {
			if( !*++pos )
				break;
		} else if( *pos == '\'' )
			count--;

	if( !*pos ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( NULL );
		return;
	}

    outstr = VM_GetTempString();
	for( out = outstr, size = VM_STRINGTEMP_LENGTH - 1 ; size && *pos ; size--, pos++, out++ )
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
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
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
	char *outstr, *out;

	VM_SAFEPARMCOUNT( 3, VM_altstr_set );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	num = PRVM_G_FLOAT( OFS_PARM1 );

	str = PRVM_G_STRING( OFS_PARM2 );
	//VM_CheckEmptyString( str );

	outstr = out = VM_GetTempString();
	for( num = num * 2 + 1, in = altstr; *in && num; *out++ = *in++ )
		if( *in == '\\' ) {
			if( !*++in ) {
				break;
			}
		} else if( *in == '\'' ) {
			num--;
		}

	if( !in ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( altstr );
		return;
	}
	// copy set in
	for( ; *str; *out++ = *str++ );
	// now jump over the old content
	for( ; *in ; in++ )
		if( *in == '\'' || (*in == '\\' && !*++in) )
			break;

	if( !in ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( NULL );
		return;
	}

	strcpy( out, in );
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
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
	const char *setstr;
	const char *set;
	const char *instr;
	const char *in;
	char *outstr;
	char *out;

	in = instr = PRVM_G_STRING( OFS_PARM0 );
	num = PRVM_G_FLOAT( OFS_PARM1 );
	set = setstr = PRVM_G_STRING( OFS_PARM2 );

	out = outstr = VM_GetTempString();
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

	strcpy( out, in );
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
}

void VM_Cmd_Init(void)
{
	// only init the stuff for the current prog
	VM_Files_Init();
	VM_Search_Init();
}

void VM_Cmd_Reset(void)
{
	CL_PurgeOwner( MENUOWNER );
	VM_Search_Reset();
	VM_Files_CloseAll();
}


