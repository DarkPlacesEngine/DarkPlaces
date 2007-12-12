// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin and extension lists can be found here
// cause large (I think they will) parts are from pr_cmds the same copyright like in pr_cmds
// also applies here

#include "quakedef.h"

#include "prvm_cmds.h"
#include <time.h>

// LordHavoc: changed this to NOT use a return statement, so that it can be used in functions that must return a value
void VM_Warning(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	Con_Print(msg);
	// TODO: either add a cvar/cmd to control the state dumping or replace some of the calls with Con_Printf [9/13/2006 Black]
	//PRVM_PrintState();
}


//============================================================================
// Common

// TODO DONE: move vm_files and vm_fssearchlist to prvm_prog_t struct
// TODO: move vm_files and vm_fssearchlist back [9/13/2006 Black]
// TODO: (move vm_files and vm_fssearchlist to prvm_prog_t struct again) [2007-01-23 LordHavoc]
// TODO: will this war ever end? [2007-01-23 LordHavoc]

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
	Con_Printf("======%s ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
	if (prog->globaloffsets.self >= 0)
	{
		ed = PRVM_PROG_TO_EDICT(PRVM_GLOBALFIELDVALUE(prog->globaloffsets.self)->edict);
		PRVM_ED_Print(ed, NULL);
	}

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
	if (prog->globaloffsets.self >= 0)
	{
		ed = PRVM_PROG_TO_EDICT(PRVM_GLOBALFIELDVALUE(prog->globaloffsets.self)->edict);
		PRVM_ED_Print(ed, NULL);

		PRVM_ED_Free (ed);
	}
	else
		// objerror has to display the object fields -> else call
		PRVM_ERROR ("VM_objecterror: self not defined !");
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
	PRVM_G_FLOAT(OFS_RETURN) = Cvar_VariableValue(string);
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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(Cvar_VariableString(string));
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
	char s[128];

	VM_SAFEPARMCOUNT(1, VM_ftos);

	v = PRVM_G_FLOAT(OFS_PARM0);

	if ((float)((int)v) == v)
		sprintf(s, "%i", (int)v);
	else
		sprintf(s, "%f", v);
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

	sprintf (s, "'%5.1f %5.1f %5.1f'", PRVM_G_VECTOR(OFS_PARM0)[0], PRVM_G_VECTOR(OFS_PARM0)[1], PRVM_G_VECTOR(OFS_PARM0)[2]);
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

	sprintf (s, "entity %i", PRVM_G_EDICTNUM(OFS_PARM0));
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
	if (ent < 0 || ent >= MAX_EDICTS || PRVM_PROG_TO_EDICT(ent)->priv.required->free)
		ent = 0; // return world instead of a free or invalid entity

	PRVM_G_INT(OFS_RETURN) = ent;
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
	struct tm *tm;
	char fmt[VM_STRINGTEMP_LENGTH];
	char result[VM_STRINGTEMP_LENGTH];
	VM_SAFEPARMCOUNTRANGE(2, 8, VM_strftime);
	VM_VarString(1, fmt, sizeof(fmt));
	t = time(NULL);
	if (PRVM_G_FLOAT(OFS_PARM0))
		tm = localtime(&t);
	else
		tm = gmtime(&t);
	if (!tm)
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	strftime(result, sizeof(result), fmt, tm);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_SetTempString(result);
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
		if (developer.integer >= 1)
			VM_Warning( "VM_remove: tried to remove the null entity or a reserved entity!\n" );
	}
	else if( ed->priv.required->free )
	{
		if (developer.integer >= 1)
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

	VM_SAFEPARMCOUNT(2,VM_findchain);

	if (prog->fieldoffsets.chain < 0)
		PRVM_ERROR("VM_findchain: %s doesnt have a chain field !", PRVM_NAME);

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

		PRVM_EDICTFIELDVALUE(ent,prog->fieldoffsets.chain)->edict = PRVM_NUM_FOR_EDICT(chain);
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

	VM_SAFEPARMCOUNT(2, VM_findchainfloat);

	if (prog->fieldoffsets.chain < 0)
		PRVM_ERROR("VM_findchainfloat: %s doesnt have a chain field !", PRVM_NAME);

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

		PRVM_EDICTFIELDVALUE(ent,prog->fieldoffsets.chain)->edict = PRVM_EDICT_TO_PROG(chain);
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

	VM_SAFEPARMCOUNT(2, VM_findchainflags);

	if (prog->fieldoffsets.chain < 0)
		PRVM_ERROR("VM_findchainflags: %s doesnt have a chain field !", PRVM_NAME);

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

		PRVM_EDICTFIELDVALUE(ent,prog->fieldoffsets.chain)->edict = PRVM_EDICT_TO_PROG(chain);
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
	VM_CheckEmptyString(s);

	if(snd_initialized.integer && !S_PrecacheSound(s, true, false))
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
	mode = (int)PRVM_G_FLOAT(OFS_PARM1);
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
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning("VM_fopen: %s: no such mode %i (valid: 0 = read, 1 = append, 2 = write)\n", PRVM_NAME, mode);
		return;
	}
	filename = PRVM_G_STRING(OFS_PARM0);

	prog->openfiles[filenum] = FS_Open(va("data/%s", filename), modestring, false, false);
	if (prog->openfiles[filenum] == NULL && mode == 0)
		prog->openfiles[filenum] = FS_Open(va("%s", filename), modestring, false, false);

	if (prog->openfiles[filenum] == NULL)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		if (developer.integer >= 100)
			VM_Warning("VM_fopen: %s: %s mode %s failed\n", PRVM_NAME, filename, modestring);
	}
	else
	{
		PRVM_G_FLOAT(OFS_RETURN) = filenum;
		if (developer.integer >= 100)
			Con_Printf("VM_fopen: %s: %s mode %s opened as #%i\n", PRVM_NAME, filename, modestring, filenum);
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
	if (developer.integer >= 100)
		Con_Printf("VM_fclose: %s: #%i closed\n", PRVM_NAME, filenum);
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
	if (developer.integer >= 100)
		Con_Printf("fgets: %s: %s\n", PRVM_NAME, string);
	if (c >= 0 || end)
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(string);
	else
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
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
	if (developer.integer >= 100)
		Con_Printf("fputs: %s: %s\n", PRVM_NAME, string);
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

	PRVM_G_FLOAT(OFS_RETURN) = strlen(PRVM_G_STRING(OFS_PARM0));
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

	nCnt = COM_StringLengthNoColors(szString, 0, NULL);

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
	int i, start, length;
	const char *s;
	char string[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT(3,VM_substring);

	s = PRVM_G_STRING(OFS_PARM0);
	start = (int)PRVM_G_FLOAT(OFS_PARM1);
	length = (int)PRVM_G_FLOAT(OFS_PARM2);
	for (i = 0;i < start && *s;i++, s++);
	for (i = 0;i < (int)sizeof(string) - 1 && *s && i < length;i++, s++)
		string[i] = *s;
	string[i] = 0;
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
	for (i = 0; i < subject_len; i++)
	{
		for (j = 0; j < search_len && i+j < subject_len; j++)
			if (subject[i+j] != search[j])
				break;
		if (j == search_len || i+j == subject_len)
		{
		// found it at offset 'i'
			for (j = 0; j < replace_len && si < (int)sizeof(string) - 1; j++)
				string[si++] = replace[j];
			i += search_len - 1;
		}
		else
		{
		// not found
			if (si < (int)sizeof(string) - 1)
				string[si++] = subject[i];
		}
	}
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
	for (i = 0; i < subject_len; i++)
	{
		for (j = 0; j < search_len && i+j < subject_len; j++)
			if (tolower(subject[i+j]) != tolower(search[j]))
				break;
		if (j == search_len || i+j == subject_len)
		{
		// found it at offset 'i'
			for (j = 0; j < replace_len && si < (int)sizeof(string) - 1; j++)
				string[si++] = replace[j];
			i += search_len - 1;
		}
		else
		{
		// not found
			if (si < (int)sizeof(string) - 1)
				string[si++] = subject[i];
		}
	}
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
int num_tokens = 0;
int tokens[256];
void VM_tokenize (void)
{
	const char *p;
	static char string[VM_STRINGTEMP_LENGTH]; // static, because it's big

	VM_SAFEPARMCOUNT(1,VM_tokenize);

	strlcpy(string, PRVM_G_STRING(OFS_PARM0), sizeof(string));
	p = string;

	num_tokens = 0;
	while(COM_ParseToken_VM_Tokenize(&p, false))
	{
		if (num_tokens >= (int)(sizeof(tokens)/sizeof(tokens[0])))
			break;
		tokens[num_tokens++] = PRVM_SetTempString(com_token);
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
	const char *p;
	const char *token;
	char tokentext[MAX_INPUTLINE];
	static char string[VM_STRINGTEMP_LENGTH]; // static, because it's big

	VM_SAFEPARMCOUNTRANGE(2, 8,VM_tokenizebyseparator);

	strlcpy(string, PRVM_G_STRING(OFS_PARM0), sizeof(string));
	p = string;

	numseparators = 0;;
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
		}
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

	if (token_num >= 0 && token_num < num_tokens)
		PRVM_G_INT(OFS_RETURN) = tokens[token_num];
	else
		PRVM_G_INT(OFS_RETURN) = OFS_NULL;
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

	PRVM_G_FLOAT(OFS_RETURN) = (float) realtime;
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
	char tmp[2];
	VM_SAFEPARMCOUNT(1, VM_chr);

	tmp[0] = (unsigned char) PRVM_G_FLOAT(OFS_PARM0);
	tmp[1] = 0;

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
	if( Draw_CachePic(s, false)->tex == r_texture_notexture )
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

dp_font_t *getdrawfont()
{
	if(prog->globaloffsets.drawfont >= 0)
	{
		int f = PRVM_G_FLOAT(prog->globaloffsets.drawfont);
		if(f < 0 || f >= MAX_FONTS)
			return FONT_DEFAULT;
		return &dp_fonts[f];
	}
	else
		return FONT_DEFAULT;
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

	DrawQ_String_Font(pos[0], pos[1], &character, 1, scale[0], scale[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag, NULL, true, getdrawfont());
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

	DrawQ_String_Font(pos[0], pos[1], string, 0, scale[0], scale[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag, NULL, true, getdrawfont());
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_drawcolorcodedstring

float	drawcolorcodedstring(vector position, string text, vector scale, float alpha, float flag)
=========
*/
void VM_drawcolorcodedstring(void)
{
	float *pos,*scale;
	const char  *string;
	int flag,color;
	VM_SAFEPARMCOUNT(5,VM_drawstring);

	string = PRVM_G_STRING(OFS_PARM1);
	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	flag = (int)PRVM_G_FLOAT(OFS_PARM5);

	if(flag < DRAWFLAG_NORMAL || flag >=DRAWFLAG_NUMFLAGS)
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

	color = -1;
	DrawQ_String_Font(pos[0], pos[1], string, 0, scale[0], scale[1], 1, 1, 1, PRVM_G_FLOAT(OFS_PARM3), flag, NULL, false, getdrawfont());
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}
/*
=========
VM_stringwidth

float	stringwidth(string text, float allowColorCodes)
=========
*/
void VM_stringwidth(void)
{
	const char  *string;
	int colors;
	VM_SAFEPARMCOUNT(2,VM_drawstring);

	string = PRVM_G_STRING(OFS_PARM0);
	colors = (int)PRVM_G_FLOAT(OFS_PARM1);

	PRVM_G_FLOAT(OFS_RETURN) = DrawQ_String_Font(0, 0, string, 0, 1, 1, 0, 0, 0, 0, 0, NULL, !colors, getdrawfont()); // 1x1 characters, don't actually draw
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

	DrawQ_Pic(pos[0], pos[1], Draw_CachePic(picname, true), size[0], size[1], rgb[0], rgb[1], rgb[2], PRVM_G_FLOAT(OFS_PARM4), flag);
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

	DrawQ_SuperPic(pos[0], pos[1], Draw_CachePic(picname, true),
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

	pic = Draw_CachePic (p, false);

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
VM_stringtokeynum

float stringtokeynum(string key)
=========
*/
void VM_stringtokeynum (void)
{
	VM_SAFEPARMCOUNT( 1, VM_keynumtostring );

	PRVM_G_INT(OFS_RETURN) = Key_StringToKeynum(PRVM_G_STRING(OFS_PARM0));
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

#ifdef SUPPORT_GECKO
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

	instance = prog->opengeckoinstances[ i ] = CL_Gecko_CreateBrowser( name );
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

	PRVM_G_FLOAT( OFS_RETURN ) = (CL_Gecko_Event_Key( instance, key, eventtype ) == true);
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
	CL_Gecko_Resize( instance, w, h );
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


#endif

/*
==============
VM_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
void makevectors(vector angle)
==============
*/
void VM_makevectors (void)
{
	prvm_eval_t *valforward, *valright, *valup;
	valforward = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.v_forward);
	valright = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.v_right);
	valup = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.v_up);
	if (!valforward || !valright || !valup)
	{
		VM_Warning("makevectors: could not find v_forward, v_right, or v_up global variables\n");
		return;
	}
	VM_SAFEPARMCOUNT(1, VM_makevectors);
	AngleVectors (PRVM_G_VECTOR(OFS_PARM0), valforward->vector, valright->vector, valup->vector);
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
	prvm_eval_t *valforward, *valright, *valup;
	valforward = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.v_forward);
	valright = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.v_right);
	valup = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.v_up);
	if (!valforward || !valright || !valup)
	{
		VM_Warning("vectorvectors: could not find v_forward, v_right, or v_up global variables\n");
		return;
	}
	VM_SAFEPARMCOUNT(1, VM_vectorvectors);
	VectorNormalize2(PRVM_G_VECTOR(OFS_PARM0), valforward->vector);
	VectorVectors(valforward->vector, valright->vector, valup->vector);
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

	n1 = (int)fabs((int)PRVM_G_FLOAT(OFS_PARM0));
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
	const char *setstr;
	const char *set;
	const char *instr;
	const char *in;
	char *out;
	char outstr[VM_STRINGTEMP_LENGTH];

	VM_SAFEPARMCOUNT(3, VM_altstr_ins);

	in = instr = PRVM_G_STRING( OFS_PARM0 );
	num = (int)PRVM_G_FLOAT( OFS_PARM1 );
	set = setstr = PRVM_G_STRING( OFS_PARM2 );

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
#define MAX_QCSTR_BUFFERS 128
#define MAX_QCSTR_STRINGS 1024

typedef struct
{
	int		num_strings;
	char	*strings[MAX_QCSTR_STRINGS];
}qcstrbuffer_t;

// FIXME: move stringbuffers to prog_t to allow multiple progs!
static qcstrbuffer_t	*qcstringbuffers[MAX_QCSTR_BUFFERS];
static int				num_qcstringbuffers;
static int				buf_sortpower;

#define BUFSTR_BUFFER(a) (a>=MAX_QCSTR_BUFFERS) ? NULL : (qcstringbuffers[a])
#define BUFSTR_ISFREE(a) (a<MAX_QCSTR_BUFFERS&&qcstringbuffers[a]&&qcstringbuffers[a]->num_strings<=0) ? 1 : 0

static int BufStr_FindFreeBuffer (void)
{
	int	i;
	if(num_qcstringbuffers == MAX_QCSTR_BUFFERS)
		return -1;
	for(i=0;i<MAX_QCSTR_BUFFERS;i++)
		if(!qcstringbuffers[i])
		{
			qcstringbuffers[i] = (qcstrbuffer_t *)Z_Malloc(sizeof(qcstrbuffer_t));
			memset(qcstringbuffers[i], 0, sizeof(qcstrbuffer_t));
			return i;
		}
	return -1;
}

static void BufStr_ClearBuffer (int index)
{
	qcstrbuffer_t	*b = qcstringbuffers[index];
	int				i;

	if(b)
	{
		if(b->num_strings > 0)
		{
			for(i=0;i<b->num_strings;i++)
				if(b->strings[i])
					Z_Free(b->strings[i]);
			num_qcstringbuffers--;
		}
		Z_Free(qcstringbuffers[index]);
		qcstringbuffers[index] = NULL;
	}
}

static int BufStr_FindFreeString (qcstrbuffer_t *b)
{
	int				i;
	for(i=0;i<b->num_strings;i++)
		if(!b->strings[i] || !b->strings[i][0])
			return i;
	if(i == MAX_QCSTR_STRINGS)	return -1;
	else						return i;
}

static int BufStr_SortStringsUP (const void *in1, const void *in2)
{
	const char *a, *b;
	a = *((const char **) in1);
	b = *((const char **) in2);
	if(!a[0])	return 1;
	if(!b[0])	return -1;
	return strncmp(a, b, buf_sortpower);
}

static int BufStr_SortStringsDOWN (const void *in1, const void *in2)
{
	const char *a, *b;
	a = *((const char **) in1);
	b = *((const char **) in2);
	if(!a[0])	return 1;
	if(!b[0])	return -1;
	return strncmp(b, a, buf_sortpower);
}

/*
========================
VM_buf_create
creates new buffer, and returns it's index, returns -1 if failed
float buf_create(void) = #460;
========================
*/
void VM_buf_create (void)
{
	int i;
	VM_SAFEPARMCOUNT(0, VM_buf_create);
	i = BufStr_FindFreeBuffer();
	if(i >= 0)
		num_qcstringbuffers++;
	//else
		//Con_Printf("VM_buf_create: buffers overflow in %s\n", PRVM_NAME);
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
	VM_SAFEPARMCOUNT(1, VM_buf_del);
	if(BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0)))
		BufStr_ClearBuffer((int)PRVM_G_FLOAT(OFS_PARM0));
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
	qcstrbuffer_t	*b;
	VM_SAFEPARMCOUNT(1, VM_buf_getsize);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		VM_Warning("VM_buf_getsize: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = b->num_strings;
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
	qcstrbuffer_t	*b1, *b2;
	int				i;
	VM_SAFEPARMCOUNT(2, VM_buf_copy);

	b1 = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b1)
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
	b2 = BUFSTR_BUFFER(i);
	if(!b2)
	{
		VM_Warning("VM_buf_copy: invalid destination buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM1), PRVM_NAME);
		return;
	}

	BufStr_ClearBuffer(i);
	qcstringbuffers[i] = (qcstrbuffer_t *)Z_Malloc(sizeof(qcstrbuffer_t));
	memset(qcstringbuffers[i], 0, sizeof(qcstrbuffer_t));
	b2->num_strings = b1->num_strings;

	for(i=0;i<b1->num_strings;i++)
		if(b1->strings[i] && b1->strings[i][0])
		{
			size_t stringlen;
			stringlen = strlen(b1->strings[i]) + 1;
			b2->strings[i] = (char *)Z_Malloc(stringlen);
			if(!b2->strings[i])
			{
				VM_Warning("VM_buf_copy: not enough memory for buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM1), PRVM_NAME);
				break;
			}
			memcpy(b2->strings[i], b1->strings[i], stringlen);
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
	qcstrbuffer_t	*b;
	int				i;
	VM_SAFEPARMCOUNT(3, VM_buf_sort);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		VM_Warning("VM_buf_sort: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(b->num_strings <= 0)
	{
		VM_Warning("VM_buf_sort: tried to sort empty buffer %i in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	// TODO: please someone rename this to buf_cmplength [12/3/2007 Black]
	buf_sortpower = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(buf_sortpower <= 0)
		buf_sortpower = 99999999;

	if(!PRVM_G_FLOAT(OFS_PARM2))
		qsort(b->strings, b->num_strings, sizeof(char*), BufStr_SortStringsUP);
	else
		qsort(b->strings, b->num_strings, sizeof(char*), BufStr_SortStringsDOWN);

	for(i=b->num_strings-1;i>=0;i--)	//[515]: delete empty lines
		if(b->strings)
		{
			if(b->strings[i][0])
				break;
			else
			{
				Z_Free(b->strings[i]);
				--b->num_strings;
				b->strings[i] = NULL;
			}
		}
		else
			--b->num_strings;
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
	qcstrbuffer_t	*b;
	char			k[VM_STRINGTEMP_LENGTH];
	const char		*sep;
	int				i;
	size_t			l;
	VM_SAFEPARMCOUNT(2, VM_buf_implode);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	if(!b)
	{
		VM_Warning("VM_buf_implode: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(!b->num_strings)
		return;
	sep = PRVM_G_STRING(OFS_PARM1);
	k[0] = 0;
	for(l=i=0;i<b->num_strings;i++)
		if(b->strings[i])
		{
			l += (i > 0 ? strlen(sep) : 0) + strlen(b->strings[i]);
			if (l >= sizeof(k) - 1)
				break;
			strlcat(k, sep, sizeof(k));
			strlcat(k, b->strings[i], sizeof(k));
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
	qcstrbuffer_t	*b;
	int				strindex;
	VM_SAFEPARMCOUNT(2, VM_bufstr_get);

	PRVM_G_INT(OFS_RETURN) = OFS_NULL;
	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		VM_Warning("VM_bufstr_get: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	strindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(strindex < 0 || strindex > MAX_QCSTR_STRINGS)
	{
		VM_Warning("VM_bufstr_get: invalid string index %i used in %s\n", strindex, PRVM_NAME);
		return;
	}
	if(b->num_strings <= strindex)
		return;
	if(b->strings[strindex])
		PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(b->strings[strindex]);
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
	int				bufindex, strindex;
	qcstrbuffer_t	*b;
	const char		*news;
	size_t			alloclen;

	VM_SAFEPARMCOUNT(3, VM_bufstr_set);

	bufindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	b = BUFSTR_BUFFER(bufindex);
	if(!b)
	{
		VM_Warning("VM_bufstr_set: invalid buffer %i used in %s\n", bufindex, PRVM_NAME);
		return;
	}
	strindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(strindex < 0 || strindex > MAX_QCSTR_STRINGS)
	{
		VM_Warning("VM_bufstr_set: invalid string index %i used in %s\n", strindex, PRVM_NAME);
		return;
	}
	news = PRVM_G_STRING(OFS_PARM2);
	if(b->strings[strindex])
		Z_Free(b->strings[strindex]);
	alloclen = strlen(news) + 1;
	b->strings[strindex] = (char *)Z_Malloc(alloclen);
	memcpy(b->strings[strindex], news, alloclen);
}

/*
========================
VM_bufstr_add
adds string to buffer in nearest free slot and returns it
"order == TRUE" means that string will be added after last "full" slot
float bufstr_add(float bufhandle, string str, float order) = #467;
========================
*/
void VM_bufstr_add (void)
{
	int				bufindex, order, strindex;
	qcstrbuffer_t	*b;
	const char		*string;
	size_t			alloclen;

	VM_SAFEPARMCOUNT(3, VM_bufstr_add);

	bufindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	b = BUFSTR_BUFFER(bufindex);
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	if(!b)
	{
		VM_Warning("VM_bufstr_add: invalid buffer %i used in %s\n", bufindex, PRVM_NAME);
		return;
	}
	string = PRVM_G_STRING(OFS_PARM1);
	order = (int)PRVM_G_FLOAT(OFS_PARM2);
	if(order)
		strindex = b->num_strings;
	else
	{
		strindex = BufStr_FindFreeString(b);
		if(strindex < 0)
		{
			VM_Warning("VM_bufstr_add: buffer %i has no free string slots in %s\n", bufindex, PRVM_NAME);
			return;
		}
	}

	while(b->num_strings <= strindex)
	{
		if(b->num_strings == MAX_QCSTR_STRINGS)
		{
			VM_Warning("VM_bufstr_add: buffer %i has no free string slots in %s\n", bufindex, PRVM_NAME);
			return;
		}
		b->strings[b->num_strings] = NULL;
		b->num_strings++;
	}
	if(b->strings[strindex])
		Z_Free(b->strings[strindex]);
	alloclen = strlen(string) + 1;
	b->strings[strindex] = (char *)Z_Malloc(alloclen);
	memcpy(b->strings[strindex], string, alloclen);
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
	qcstrbuffer_t	*b;
	VM_SAFEPARMCOUNT(2, VM_bufstr_free);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		VM_Warning("VM_bufstr_free: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	i = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(i < 0 || i > MAX_QCSTR_STRINGS)
	{
		VM_Warning("VM_bufstr_free: invalid string index %i used in %s\n", i, PRVM_NAME);
		return;
	}
	if(b->strings[i])
		Z_Free(b->strings[i]);
	b->strings[i] = NULL;
	if(i+1 == b->num_strings)
		--b->num_strings;
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

	ent = PRVM_PROG_TO_EDICT(PRVM_GLOBALFIELDVALUE(prog->globaloffsets.self)->edict);
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
	if (prog->fieldoffsets.angles < 0 || prog->fieldoffsets.ideal_yaw < 0 || prog->fieldoffsets.yaw_speed < 0)
	{
		VM_Warning("changeyaw: angles, ideal_yaw, or yaw_speed field(s) not found\n");
		return;
	}
	current = ANGLEMOD(PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.angles)->vector[1]);
	ideal = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.ideal_yaw)->_float;
	speed = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.yaw_speed)->_float;

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

	PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.angles)->vector[1] = ANGLEMOD (current + move);
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
	if (prog->fieldoffsets.angles < 0 || prog->fieldoffsets.idealpitch < 0 || prog->fieldoffsets.pitch_speed < 0)
	{
		VM_Warning("changepitch: angles, idealpitch, or pitch_speed field(s) not found\n");
		return;
	}
	current = ANGLEMOD(PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.angles)->vector[0]);
	ideal = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.idealpitch)->_float;
	speed = PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.pitch_speed)->_float;

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

	PRVM_EDICTFIELDVALUE(ent, prog->fieldoffsets.angles)->vector[0] = ANGLEMOD (current + move);
}

// TODO: adapt all static function names to use a single naming convention... [12/3/2007 Black]
static int Is_Text_Color (char c, char t)
{
	int a = 0;
	char c2 = c - (c & 128);
	char t2 = t - (t & 128);

	if(c != STRING_COLOR_TAG && c2 != STRING_COLOR_TAG)		return 0;
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

void VM_uncolorstring (void)
{
	const char	*in;
	char		out[VM_STRINGTEMP_LENGTH];
	int			k = 0, i = 0;

	VM_SAFEPARMCOUNT(1, VM_uncolorstring);
	in = PRVM_G_STRING(OFS_PARM0);
	VM_CheckEmptyString (in);

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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(out);
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
	firstofs = (prog->argc > 2)?PRVM_G_FLOAT(OFS_PARM2):0;

	if (firstofs && (firstofs < 0 || firstofs > (int)strlen(instr)))
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	match = strstr(instr+firstofs, match);
	if (!match)
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = match - instr;
}

//#222 string(string s, float index) str2chr (FTE_STRINGS)
void VM_str2chr (void)
{
	const char *s;
	VM_SAFEPARMCOUNT(2, VM_str2chr);
	s = PRVM_G_STRING(OFS_PARM0);
	if((unsigned)PRVM_G_FLOAT(OFS_PARM1) < strlen(s))
		PRVM_G_FLOAT(OFS_RETURN) = (unsigned char)s[(unsigned)PRVM_G_FLOAT(OFS_PARM1)];
	else
		PRVM_G_FLOAT(OFS_RETURN) = 0;
}

//#223 string(float c, ...) chr2str (FTE_STRINGS)
void VM_chr2str (void)
{
	char	t[9];
	int		i;
	VM_SAFEPARMCOUNTRANGE(0, 8, VM_chr2str);
	for(i = 0;i < prog->argc && i < (int)sizeof(t) - 1;i++)
		t[i] = (unsigned char)PRVM_G_FLOAT(OFS_PARM0+i*3);
	t[i] = 0;
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

	ccase = PRVM_G_FLOAT(OFS_PARM0);	//0 same, 1 lower, 2 upper
	redalpha = PRVM_G_FLOAT(OFS_PARM1);	//0 same, 1 white, 2 red,  5 alternate, 6 alternate-alternate
	rednum = PRVM_G_FLOAT(OFS_PARM2);	//0 same, 1 white, 2 red, 3 redspecial, 4 whitespecial, 5 alternate, 6 alternate-alternate
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
	pad = PRVM_G_FLOAT(OFS_PARM0);
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

void VM_wasfreed (void)
{
	VM_SAFEPARMCOUNT(1, VM_wasfreed);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_G_EDICT(OFS_PARM0)->priv.required->free;
}

void VM_SetTraceGlobals(const trace_t *trace)
{
	prvm_eval_t *val;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_allsolid)))
		val->_float = trace->allsolid;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_startsolid)))
		val->_float = trace->startsolid;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_fraction)))
		val->_float = trace->fraction;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_inwater)))
		val->_float = trace->inwater;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_inopen)))
		val->_float = trace->inopen;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_endpos)))
		VectorCopy(trace->endpos, val->vector);
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_plane_normal)))
		VectorCopy(trace->plane.normal, val->vector);
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_plane_dist)))
		val->_float = trace->plane.dist;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_ent)))
		val->edict = PRVM_EDICT_TO_PROG(trace->ent ? trace->ent : prog->edicts);
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dpstartcontents)))
		val->_float = trace->startsupercontents;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphitcontents)))
		val->_float = trace->hitsupercontents;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphitq3surfaceflags)))
		val->_float = trace->hitq3surfaceflags;
	if ((val = PRVM_GLOBALFIELDVALUE(prog->globaloffsets.trace_dphittexturename)))
		val->string = trace->hittexture ? PRVM_SetTempString(trace->hittexture->name) : 0;
}

//=============

void VM_Cmd_Init(void)
{
	// only init the stuff for the current prog
	VM_Files_Init();
	VM_Search_Init();
#ifdef SUPPORT_GECKO
	VM_Gecko_Init();
#endif
//	VM_BufStr_Init();
}

void VM_Cmd_Reset(void)
{
	CL_PurgeOwner( MENUOWNER );
	VM_Search_Reset();
	VM_Files_CloseAll();
#ifdef SUPPORT_GECKO
	VM_Gecko_Destroy();
#endif
//	VM_BufStr_ShutDown();
}

