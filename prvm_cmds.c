// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin and extension lists can be found here
// cause large (I think they will) parts are from pr_cmds the same copyright like in pr_cmds
// also applies here


/*
============================================================================
common cmd list:
=================

		checkextension(string)
		error(...[string])
		objerror(...[string)
		print(...[strings])
		bprint(...[string])
		sprint(float clientnum,...[string])
		centerprint(...[string])
vector	normalize(vector)
float	vlen(vector)
float	vectoyaw(vector)
vector	vectoangles(vector)
float	random()
		cmd(string)
		float cvar (string)
		cvar_set (string,string)
		dprint(...[string])
string	ftos(float)
float	fabs(float)
string	vtos(vector)
string	etos(entity)
float	stof(...[string])
entity	spawn()
		remove(entity e)
entity	find(entity start, .string field, string match)

entity	findfloat(entity start, .float field, float match)
entity	findentity(entity start, .entity field, entity match)

entity	findchain(.string field, string match)

entity	findchainfloat(.string field, float match)
entity	findchainentity(.string field, entity match)

string	precache_file(string)
string	precache_sound (string sample)
		coredump()
		traceon()
		traceoff()
		eprint(entity e)
float	rint(float)
float	floor(float)
float	ceil(float)
entity	nextent(entity)
float	sin(float)
float	cos(float)
float	sqrt(float)
vector	randomvec()
float	registercvar (string name, string value, float flags)
float	min(float a, float b, ...[float])
float	max(float a, float b, ...[float])
float	bound(float min, float value, float max)
float	pow(float a, float b)
		copyentity(entity src, entity dst)
float	fopen(string filename, float mode)
		fclose(float fhandle)
string	fgets(float fhandle)
		fputs(float fhandle, string s)
float	strlen(string s)
string	strcat(string,string,...[string])
string	substring(string s, float start, float length)
vector	stov(string s)
string	strzone(string s)
		strunzone(string s)
float	tokenize(string s)
string	argv(float n)
float	isserver()
float	clientcount()
float	clientstate()
		clientcommand(float client, string s) (for client and menu)
		changelevel(string map)
		localsound(string sample)
vector	getmousepos()
float	gettime()
		loadfromdata(string data)
		loadfromfile(string file)
float	mod(float val, float m)
const string	str_cvar (string)
		crash()
		stackdump()

float	search_begin(string pattern, float caseinsensitive, float quiet)
void	search_end(float handle)
float	search_getsize(float handle)
string	search_getfilename(float handle, float num)

string	chr(float ascii)

float	itof(intt ent)
intt	ftoi(float num)

float	altstr_count(string)
string	altstr_prepare(string)
string	altstr_get(string,float)
string	altstr_set(string altstr, float num, string set)
string	altstr_ins(string altstr, float num, string set)

perhaps only : Menu : WriteMsg
===============================

		WriteByte(float data, float dest, float desto)
		WriteChar(float data, float dest, float desto)
		WriteShort(float data, float dest, float desto)
		WriteLong(float data, float dest, float desto)
		WriteAngle(float data, float dest, float desto)
		WriteCoord(float data, float dest, float desto)
		WriteString(string data, float dest, float desto)
		WriteEntity(entity data, float dest, float desto)

Client & Menu : draw functions & video functions
===================================================

float	iscachedpic(string pic)
string	precache_pic(string pic)
		freepic(string s)
float	drawcharacter(vector position, float character, vector scale, vector rgb, float alpha, float flag)
float	drawstring(vector position, string text, vector scale, vector rgb, float alpha, float flag)
float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag)
float	drawfill(vector position, vector size, vector rgb, float alpha, float flag)
		drawsetcliparea(float x, float y, float width, float height)
		drawresetcliparea()
vector	getimagesize(string pic)

float	cin_open(string file, string name)
void	cin_close(string name)
void	cin_setstate(string name, float type)
float	cin_getstate(string name)
void	cin_restart(string name)

==============================================================================
menu cmd list:
===============

		setkeydest(float dest)
float	getkeydest()
		setmousetarget(float target)
float	getmousetarget()

		callfunction(...,string function_name)
		writetofile(float fhandle, entity ent)
float	isfunction(string function_name)
vector	getresolution(float number)
string	keynumtostring(float keynum)
string	findkeysforcommand(string command)
float	getserverliststat(float type)
string	getserverliststring(float fld, float hostnr)

		parseentitydata(entity ent, string data)

float	stringtokeynum(string key)

		resetserverlistmasks()
		setserverlistmaskstring(float mask, float fld, string str)
		setserverlistmasknumber(float mask, float fld, float num, float op)
		resortserverlist()
		setserverlistsort(float field, float descending)
		refreshserverlist()
float	getserverlistnumber(float fld, float hostnr)
float	getserverlistindexforkey(string key)
		addwantedserverlistkey(string key)
*/

#include "quakedef.h"
#include "progdefs.h"
#include "progsvm.h"
#include "clprogdefs.h"
#include "mprogdefs.h"

#include "cl_video.h"

//============================================================================
// nice helper macros

#ifndef VM_NOPARMCHECK
#define VM_SAFEPARMCOUNT(p,f)	if(prog->argc != p) PRVM_ERROR(#f " wrong parameter count (" #p " expected ) !\n")
#else
#define VM_SAFEPARMCOUNT(p,f)
#endif

#define	VM_RETURN_EDICT(e)		(((int *)prog->globals)[OFS_RETURN] = PRVM_EDICT_TO_PROG(e))

#define VM_STRINGS_MEMPOOL		vm_strings_mempool[PRVM_GetProgNr()]

#define e10 0,0,0,0,0,0,0,0,0,0
#define e100 e10,e10,e10,e10,e10,e10,e10,e10,e10,e10
#define e1000 e100,e100,e100,e100,e100,e100,e100,e100,e100,e100

//============================================================================
// Common

// string zone mempool
mempool_t *vm_strings_mempool[PRVM_MAXPROGS];

// temp string handling
// LordHavoc: added this to semi-fix the problem of using many ftos calls in a print
#define VM_STRINGTEMP_BUFFERS 16
#define VM_STRINGTEMP_LENGTH 4096
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

static char *VM_GetTempString(void)
{
	char *s;
	s = vm_string_temp[vm_string_tempindex];
	vm_string_tempindex = (vm_string_tempindex + 1) % VM_STRINGTEMP_BUFFERS;
	return s;
}

void VM_CheckEmptyString (char *s)
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
static qboolean checkextension(char *name)
{
	int len;
	char *e, *start;
	len = strlen(name);

	for (e = prog->extensionstring;*e;e++)
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
			{
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
		PRVM_ERROR ("VM_objecterror: self not defined !\n");
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
	float	new;

	VM_SAFEPARMCOUNT(1,VM_normalize);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

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
	float	*value1;
	float	new;

	VM_SAFEPARMCOUNT(1,VM_vlen);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrt(new);

	PRVM_G_FLOAT(OFS_RETURN) = new;
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
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
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
	edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);

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
	char *s;

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

[localcmd (string) or]
cmd (string)
=================
*/
void VM_localcmd (void)
{
	VM_SAFEPARMCOUNT(1,VM_localcmd);

	Cbuf_AddText(PRVM_G_STRING(OFS_PARM0));
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
VM_str_cvar

const string	str_cvar (string)
=================
*/
void VM_str_cvar(void)
{
	char *out, *name;
	const char *cvar_string;
	VM_SAFEPARMCOUNT(1,VM_str_cvar);

	name = PRVM_G_STRING(OFS_PARM0);

	if(!name)
		PRVM_ERROR("VM_str_cvar: %s: null string\n", PRVM_NAME);

	VM_CheckEmptyString(name);

	out = VM_GetTempString();

	cvar_string = Cvar_VariableString(name);

	strcpy(out, cvar_string);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(out);
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
		Con_Printf("%s: %s", PRVM_NAME, string);
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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(s);
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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(s);
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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(s);
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
	if(PRVM_PROG_TO_EDICT(ent)->p.e->free)
		PRVM_ERROR ("VM_ftoe: %s tried to access a freed entity (entity %i)!\n", PRVM_NAME, ent);

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
//	if (ed == prog->edicts)
//		PRVM_ERROR ("remove: tried to remove world\n");
//	if (PRVM_NUM_FOR_EDICT(ed) <= sv.maxclients)
//		Host_Error("remove: tried to remove a client\n");
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
	char	*s, *t;
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
		if (ed->p.e->free)
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
		if (ed->p.e->free)
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
int PRVM_ED_FindFieldOffset(const char *field);
// chained search for strings in entity fields
// entity(.string field, string match) findchain = #402;
void VM_findchain (void)
{
	int		i;
	int		f;
	int		chain_of;
	char	*s, *t;
	prvm_edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2,VM_findchain);

	// is the same like !(prog->flag & PRVM_FE_CHAIN) - even if the operator precedence is another
	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchain: %s doesnt have a chain field !\n", PRVM_NAME);

	chain_of = PRVM_ED_FindFieldOffset ("chain");

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
		if (ent->p.e->free)
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
		PRVM_ERROR("VM_findchainfloat: %s doesnt have a chain field !\n", PRVM_NAME);

	chain_of = PRVM_ED_FindFieldOffset ("chain");

	chain = (prvm_edict_t *)prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = PRVM_G_FLOAT(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->p.e->free)
			continue;
		if (PRVM_E_FLOAT(ent,f) != s)
			continue;

		PRVM_E_INT(ent,chain_of) = PRVM_EDICT_TO_PROG(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
=========
VM_precache_file

string	precache_file(string)
=========
*/
void VM_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	VM_SAFEPARMCOUNT(1,VM_precache_file);

	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
=========
VM_preache_error

used instead of the other VM_precache_* functions in the builtin list
=========
*/

void VM_precache_error (void)
{
	PRVM_ERROR ("PF_Precache_*: Precache can only be done in spawn functions");
}

/*
=========
VM_precache_sound

string	precache_sound (string sample)
=========
*/
void VM_precache_sound (void)
{
	char	*s;

	VM_SAFEPARMCOUNT(1, VM_precache_sound);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
	VM_CheckEmptyString (s);

	if(snd_initialized.integer && !S_PrecacheSound (s,true, true))
		Con_Printf("VM_precache_sound: Failed to load %s for %s\n", s, PRVM_NAME);
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

	PRVM_ERROR("Crash called by %s\n",PRVM_NAME);
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
		if (!ent->p.e->free)
		{
			VM_RETURN_EDICT(ent);
			return;
		}
	}
}

/*
===============================================================================
MESSAGE WRITING

used only for client and menu
severs uses VM_SV_...

Write*(* data, float type, float to)

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *VM_WriteDest (void)
{
	int		dest;
	int		destclient;

	if(!sv.active)
		PRVM_ERROR("VM_WriteDest: game is not server (%s)\n", PRVM_NAME);

	dest = G_FLOAT(OFS_PARM1);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		destclient = (int) PRVM_G_FLOAT(OFS_PARM2);
		if (destclient < 0 || destclient >= svs.maxclients || !svs.clients[destclient].active)
			PRVM_ERROR("VM_clientcommand: %s: invalid client !\n", PRVM_NAME);

		return &svs.clients[destclient].message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return &sv.signon;

	default:
		PRVM_ERROR ("WriteDest: bad destination");
		break;
	}

	return NULL;
}

void VM_WriteByte (void)
{
	MSG_WriteByte (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0));
}

void VM_WriteChar (void)
{
	MSG_WriteChar (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0));
}

void VM_WriteShort (void)
{
	MSG_WriteShort (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0));
}

void VM_WriteLong (void)
{
	MSG_WriteLong (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0));
}

void VM_WriteAngle (void)
{
	MSG_WriteAngle (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0), sv.protocol);
}

void VM_WriteCoord (void)
{
	MSG_WriteCoord (VM_WriteDest(), PRVM_G_FLOAT(OFS_PARM0), sv.protocol);
}

void VM_WriteString (void)
{
	MSG_WriteString (VM_WriteDest(), PRVM_G_STRING(OFS_PARM0));
}

void VM_WriteEntity (void)
{
	MSG_WriteShort (VM_WriteDest(), PRVM_G_EDICTNUM(OFS_PARM0));
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
	char	*s;

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

	s = G_STRING(OFS_PARM0);
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
	char *name, *value;
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
		PRVM_ERROR("VM_min: %s must supply at least 2 floats\n", PRVM_NAME);
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
		PRVM_ERROR("VM_max: %s must supply at least 2 floats\n", PRVM_NAME);
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
	memcpy(out->v, in->v, prog->progs->entityfields * 4);
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
	eval_t *val;

	entnum = G_EDICTNUM(OFS_PARM0);
	i = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients || !svs.clients[entnum-1].active)
	{
		Con_Print("tried to setcolor a non-client\n");
		return;
	}

	client = svs.clients + entnum-1;
	if ((val = GETEDICTFIELDVALUE(client->edict, eval_clientcolors)))
		val->_float = i;
	client->colors = i;
	client->old_colors = i;
	client->edict->v->team = (i & 15) + 1;

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
	char *modestring, *filename;

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
		Con_Printf("VM_fopen: %s no such mode %i (valid: 0 = read, 1 = append, 2 = write)\n", PRVM_NAME, mode);
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		return;
	}
	filename = PRVM_G_STRING(OFS_PARM0);
	// .. is parent directory on many platforms
	// / is parent directory on Amiga
	// : is root of drive on Amiga (also used as a directory separator on Mac, but / works there too, so that's a bad idea)
	// \ is a windows-ism (so it's naughty to use it, / works on all platforms)
	if ((filename[0] == '.' && filename[1] == '.') || filename[0] == '/' || strrchr(filename, ':') || strrchr(filename, '\\'))
	{
		Con_Printf("VM_fopen: %s dangerous or non-portable filename \"%s\" not allowed. (contains : or \\ or begins with .. or /)\n", PRVM_NAME, filename);
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		return;
	}
	VM_FILES[filenum] = FS_Open(va("data/%s", filename), modestring, false, false);
	if (VM_FILES[filenum] == NULL && mode == 0)
		VM_FILES[filenum] = FS_Open(va("%s", filename), modestring, false, false);

	if (VM_FILES[filenum] == NULL)
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = filenum;
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
		PRVM_G_INT(OFS_RETURN) = PRVM_SetString(string);
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
	if ((stringlength = strlen(string)))
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
	char *s;

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
		PRVM_ERROR("VM_strcat wrong parameter count (min. 1 expected ) !\n");

	s = VM_GetTempString();
	VM_VarString(0, s, VM_STRINGTEMP_LENGTH);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(s);
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
	char *s, *string;

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
	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(string);
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
//string(string s) strzone = #118; // makes a copy of a string into the string zone and returns it, this is often used to keep around a tempstring for longer periods of time (tempstrings are replaced often)
void VM_strzone(void)
{
	char *in, *out;

	VM_SAFEPARMCOUNT(1,VM_strzone);

	in = PRVM_G_STRING(OFS_PARM0);
	out = Mem_Alloc(VM_STRINGS_MEMPOOL, strlen(in) + 1);
	strcpy(out, in);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(out);
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
	char *str;
	VM_SAFEPARMCOUNT(1,VM_strunzone);

	str = PRVM_G_STRING(OFS_PARM0);
	if( !str )
		PRVM_ERROR( "VM_strunzone: s%: Null string passed!", PRVM_NAME );
	if( developer.integer && !Mem_IsAllocated( VM_STRINGS_MEMPOOL, str ) )
		PRVM_ERROR( "VM_strunzone: Zone string already freed in %s!", PRVM_NAME );
	else
		Mem_Free( str );
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
//float(string s) tokenize = #441;
// takes apart a string into individal words (access them with argv), returns how many
// this function originally written by KrimZon, made shorter by LordHavoc
static char **tokens = NULL;
static int    max_tokens, num_tokens = 0;
void VM_tokenize (void)
{
	const char *p;
	char *str;

	VM_SAFEPARMCOUNT(1,VM_tokenize);

	str = PRVM_G_STRING(OFS_PARM0);

	if (tokens != NULL)
	{
		int i;
		for (i=0;i<num_tokens;i++)
			Z_Free(tokens[i]);
		Z_Free(tokens);
		num_tokens = 0;
	}

	tokens = Z_Malloc(strlen(str) * sizeof(char *));
	max_tokens = strlen(str);

	for (p = str;COM_ParseToken(&p, false) && num_tokens < max_tokens;num_tokens++)
	{
		tokens[num_tokens] = Z_Malloc(strlen(com_token) + 1);
		strcpy(tokens[num_tokens], com_token);
	}

	PRVM_G_FLOAT(OFS_RETURN) = num_tokens;
}

/*
=========
VM_argv

string argv(float n)
=========
*/
//string(float n) argv = #442;
// returns a word from the tokenized string (returns nothing for an invalid index)
// this function originally written by KrimZon, made shorter by LordHavoc
void VM_argv (void)
{
	int token_num;

	VM_SAFEPARMCOUNT(1,VM_argv);

	token_num = PRVM_G_FLOAT(OFS_PARM0);
	if (token_num >= 0 && token_num < num_tokens)
		PRVM_G_INT(OFS_RETURN) = PRVM_SetString(tokens[token_num]);
	else
		PRVM_G_INT(OFS_RETURN) = PRVM_SetString("");
}

/*
//void(entity e, entity tagentity, string tagname) setattachment = #443; // attachs e to a tag on tagentity (note: use "" to attach to entity origin/angles instead of a tag)
void PF_setattachment (void)
{
	edict_t *e = G_EDICT(OFS_PARM0);
	edict_t *tagentity = G_EDICT(OFS_PARM1);
	char *tagname = G_STRING(OFS_PARM2);
	eval_t *v;
	int i, modelindex;
	model_t *model;

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
		if (modelindex >= 0 && modelindex < MAX_MODELS)
		{
			model = sv.models[modelindex];
			if (model->data_overridetagnamesforskin && (unsigned int)tagentity->v->skin < (unsigned int)model->numskins && model->data_overridetagnamesforskin[(unsigned int)tagentity->v->skin].num_overridetagnames)
				for (i = 0;i < model->data_overridetagnamesforskin[(unsigned int)tagentity->v->skin].num_overridetagnames;i++)
					if (!strcmp(tagname, model->data_overridetagnamesforskin[(unsigned int)tagentity->v->skin].data_overridetagnames[i].name))
						v->_float = i + 1;
			// FIXME: use a model function to get tag info (need to handle skeletal)
			if (v->_float == 0 && model->num_tags)
				for (i = 0;i < model->num_tags;i++)
					if (!strcmp(tagname, model->data_tags[i].name))
						v->_float = i + 1;
			if (v->_float == 0)
				Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", NUM_FOR_EDICT(e), NUM_FOR_EDICT(tagentity), tagname, tagname, NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			Con_DPrintf("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", NUM_FOR_EDICT(e), NUM_FOR_EDICT(tagentity), tagname, tagname, NUM_FOR_EDICT(tagentity));
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

	PRVM_G_VECTOR(OFS_RETURN)[0] = in_mouse_x * vid.conwidth / vid.realwidth;
	PRVM_G_VECTOR(OFS_RETURN)[1] = in_mouse_y * vid.conheight / vid.realheight;
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
VM_M_parseentitydata

parseentitydata(entity ent, string data)
========================
*/
void VM_M_parseentitydata(void)
{
	prvm_edict_t *ent;
	const char *data;

	VM_SAFEPARMCOUNT(2, VM_parseentitydata);

    // get edict and test it
	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent->p.e->free)
		PRVM_ERROR ("VM_parseentitydata: %s: Can only set already spawned entities (entity %i is free)!\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ent));

	data = PRVM_G_STRING(OFS_PARM1);

    // parse the opening brace
	if (!COM_ParseToken(&data, false) || com_token[0] != '{' )
		PRVM_ERROR ("VM_parseentitydata: %s: Couldn't parse entity data:\n%s\n", PRVM_NAME, data );

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
	char *filename;
	qbyte *data;

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
	data = FS_LoadFile(filename, tempmempool, false);
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
	char *pattern;
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

	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(tmp);
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

	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(tmp);
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
	char	*s;

	VM_SAFEPARMCOUNT(1, VM_precache_pic);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);

	if(!s)
		PRVM_ERROR ("VM_precache_pic: %s: NULL\n", PRVM_NAME);

	VM_CheckEmptyString (s);

	if(!Draw_CachePic(s, false))
		PRVM_G_INT(OFS_RETURN) = PRVM_SetString("");
}

/*
=========
VM_freepic

freepic(string s)
=========
*/
void VM_freepic(void)
{
	char *s;

	VM_SAFEPARMCOUNT(1,VM_freepic);

	s = PRVM_G_STRING(OFS_PARM0);

	if(!s)
		PRVM_ERROR ("VM_freepic: %s: NULL\n");

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
	char  *string;
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
	char *pic;
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

	x = bound(0, PRVM_G_FLOAT(OFS_PARM0), vid.conwidth);
	y = bound(0, PRVM_G_FLOAT(OFS_PARM1), vid.conheight);
	w = bound(0, PRVM_G_FLOAT(OFS_PARM2) + PRVM_G_FLOAT(OFS_PARM0) - x, (vid.conwidth  - x));
	h = bound(0, PRVM_G_FLOAT(OFS_PARM3) + PRVM_G_FLOAT(OFS_PARM1) - y, (vid.conheight - y));

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
	char *p;
	cachepic_t *pic;

	VM_SAFEPARMCOUNT(1,VM_getimagesize);

	p = PRVM_G_STRING(OFS_PARM0);

	if(!p)
		PRVM_ERROR("VM_getimagepos: %s passed null picture name !\n", PRVM_NAME);

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
	char *file;
	char *name;

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
	char *name;

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
	char *name;
	clvideostate_t 	state;
	clvideo_t		*video;

	VM_SAFEPARMCOUNT( 2, VM_cin_netstate );

	name = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( name );

	state = PRVM_G_FLOAT( OFS_PARM1 );

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
	char *name;
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
	char *name;
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
	char *altstr, *pos;
	int	count;

	VM_SAFEPARMCOUNT( 1, VM_altstr_count );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	for( count = 0, pos = altstr ; *pos ; pos++ )
		if( *pos == '\\' && !*++pos )
				break;
		else if( *pos == '\'' )
			count++;

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
	char *instr, *in;
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

	PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( outstr );
}

/*
========================
VM_altstr_get

string altstr_get(string, float)
========================
*/
void VM_altstr_get( void )
{
	char *altstr, *pos, *outstr, *out;
	int count, size;

	VM_SAFEPARMCOUNT( 2, VM_altstr_get );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	count = PRVM_G_FLOAT( OFS_PARM1 );
	count = count * 2 + 1;

	for( pos = altstr ; *pos && count ; pos++ )
		if( *pos == '\\' && !*++pos )
			break;
		else if( *pos == '\'' )
			count--;

	if( !*pos ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( "" );
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
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( outstr );
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
	char *altstr, *str;
	char *in;
	char *outstr, *out;

	VM_SAFEPARMCOUNT( 3, VM_altstr_set );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	num = PRVM_G_FLOAT( OFS_PARM1 );

	str = PRVM_G_STRING( OFS_PARM2 );
	//VM_CheckEmptyString( str );

	outstr = out = VM_GetTempString();
	for( num = num * 2 + 1, in = altstr; *in && num; *out++ = *in++ )
		if( *in == '\\' && !*++in )
			break;
		else if( *in == '\'' )
			num--;

	if( !in ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( "" );
		return;
	}
	// copy set in
	for( ; *str; *out++ = *str++ );
	// now jump over the old contents
	for( ; *in ; in++ )
		if( *in == '\'' || (*in == '\\' && !*++in) )
			break;

	if( !in ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( "" );
		return;
	}

	strcpy( out, in );
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( outstr );
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
	char *setstr;
	char *set;
	char *instr;
	char *in;
	char *outstr;
	char *out;

	in = instr = PRVM_G_STRING( OFS_PARM0 );
	num = PRVM_G_FLOAT( OFS_PARM1 );
	set = setstr = PRVM_G_STRING( OFS_PARM2 );

	out = outstr = VM_GetTempString();
	for( num = num * 2 + 2 ; *in && num > 0 ; *out++ = *in++ )
		if( *in == '\\' && !*++in )
			break;
		else if( *in == '\'' )
			num--;

	for( ; *set ; *out++ = *set++ );

	strcpy( out, in );
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( outstr );
}

void VM_Cmd_Init(void)
{
	// only init the stuff for the current prog
	VM_STRINGS_MEMPOOL = Mem_AllocPool(va("vm_stringsmempool[%s]",PRVM_NAME), 0, NULL);
	VM_Files_Init();
	VM_Search_Init();
}

void VM_Cmd_Reset(void)
{
	//Mem_EmptyPool(VM_STRINGS_MEMPOOL);
	if( developer.integer >= 2 && VM_STRINGS_MEMPOOL ) {
		memheader_t *header;
		int	i;

		for( i = 0, header = VM_STRINGS_MEMPOOL->chain ; header ; header = header->next, i++ )
			Con_DPrintf( "Leaked string %i (size: %i): %.*s\n", i, header->size, header->size, ((char*)header) + sizeof( memheader_t ) );
	}

	Mem_FreePool(&VM_STRINGS_MEMPOOL);
	CL_PurgeOwner( MENUOWNER );
	VM_Search_Reset();
	VM_Files_CloseAll();
}

//============================================================================
// Server

char *vm_sv_extensions =
"";

prvm_builtin_t vm_sv_builtins[] = {
0  // to be consistent with the old vm
};

const int vm_sv_numbuiltins = sizeof(vm_sv_builtins) / sizeof(prvm_builtin_t);

void VM_SV_Cmd_Init(void)
{
}

void VM_SV_Cmd_Reset(void)
{
}

//============================================================================
// Client

char *vm_cl_extensions =
"";

prvm_builtin_t vm_cl_builtins[] = {
0  // to be consistent with the old vm
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t);

void VM_CL_Cmd_Init(void)
{
}

void VM_CL_Cmd_Reset(void)
{
}

//============================================================================
// Menu

char *vm_m_extensions =
"DP_CINEMATIC_DPV";

/*
=========
VM_M_setmousetarget

setmousetarget(float target)
=========
*/
void VM_M_setmousetarget(void)
{
	VM_SAFEPARMCOUNT(1, VM_M_setmousetarget);

	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case 1:
		in_client_mouse = false;
		break;
	case 2:
		in_client_mouse = true;
		break;
	default:
		PRVM_ERROR("VM_M_setmousetarget: wrong destination %i !\n",PRVM_G_FLOAT(OFS_PARM0));
	}
}

/*
=========
VM_M_getmousetarget

float	getmousetarget
=========
*/
void VM_M_getmousetarget(void)
{
	VM_SAFEPARMCOUNT(0,VM_M_getmousetarget);

	if(in_client_mouse)
		PRVM_G_FLOAT(OFS_RETURN) = 2;
	else
		PRVM_G_FLOAT(OFS_RETURN) = 1;
}



/*
=========
VM_M_setkeydest

setkeydest(float dest)
=========
*/
void VM_M_setkeydest(void)
{
	VM_SAFEPARMCOUNT(1,VM_M_setkeydest);

	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case 0:
		// key_game
		key_dest = key_game;
		break;
	case 2:
		// key_menu
		key_dest = key_menu;
		break;
	case 1:
		// key_message
		// key_dest = key_message
		// break;
	default:
		PRVM_ERROR("VM_M_setkeydest: wrong destination %i !\n",prog->globals[OFS_PARM0]);
	}
}

/*
=========
VM_M_getkeydest

float	getkeydest
=========
*/
void VM_M_getkeydest(void)
{
	VM_SAFEPARMCOUNT(0,VM_M_getkeydest);

	// key_game = 0, key_message = 1, key_menu = 2, unknown = 3
	switch(key_dest)
	{
	case key_game:
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		break;
	case key_menu:
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		break;
	case key_message:
		// not supported
		// PRVM_G_FLOAT(OFS_RETURN) = 1;
		// break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = 3;
	}
}

/*
=========
VM_M_callfunction

	callfunction(...,string function_name)
Extension: pass
=========
*/
mfunction_t *PRVM_ED_FindFunction (const char *name);
void VM_M_callfunction(void)
{
	mfunction_t *func;
	char *s;

	if(prog->argc == 0)
		PRVM_ERROR("VM_M_callfunction: 1 parameter is required !\n");

	s = PRVM_G_STRING(OFS_PARM0 + (prog->argc - 1));

	if(!s)
		PRVM_ERROR("VM_M_callfunction: null string !\n");

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_ERROR("VM_M_callfunciton: function %s not found !\n", s);
	else if (func->first_statement < 0)
	{
		// negative statements are built in functions
		int builtinnumber = -func->first_statement;
		prog->xfunction->builtinsprofile++;
		if (builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber])
			prog->builtins[builtinnumber]();
		else
			PRVM_ERROR("No such builtin #%i in %s", builtinnumber, PRVM_NAME);
	}
	else if(func > 0)
	{
		prog->argc--;
		PRVM_ExecuteProgram(func - prog->functions,"");
		prog->argc++;
	}
}

/*
=========
VM_M_isfunction

float	isfunction(string function_name)
=========
*/
mfunction_t *PRVM_ED_FindFunction (const char *name);
void VM_M_isfunction(void)
{
	mfunction_t *func;
	char *s;

	VM_SAFEPARMCOUNT(1, VM_M_isfunction);

	s = PRVM_G_STRING(OFS_PARM0);

	if(!s)
		PRVM_ERROR("VM_M_isfunction: null string !\n");

	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func)
		PRVM_G_FLOAT(OFS_RETURN) = false;
	else
		PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
VM_M_writetofile

	writetofile(float fhandle, entity ent)
=========
*/
void VM_M_writetofile(void)
{
	prvm_edict_t * ent;
	int filenum;

	VM_SAFEPARMCOUNT(2, VM_M_writetofile);

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

	ent = PRVM_G_EDICT(OFS_PARM1);
	if(ent->p.e->free)
	{
		Con_Printf("VM_M_writetofile: %s: entity %i is free !\n", PRVM_NAME, PRVM_EDICT_NUM(OFS_PARM1));
		return;
	}

	PRVM_ED_Write (VM_FILES[filenum], ent);
}

/*
=========
VM_M_getresolution

vector	getresolution(float number)
=========
*/
extern unsigned short video_resolutions[][2];
void VM_M_getresolution(void)
{
	int nr;
	VM_SAFEPARMCOUNT(1, VM_getresolution);

	nr = PRVM_G_FLOAT(OFS_PARM0);


	PRVM_G_VECTOR(OFS_RETURN)[0] = video_resolutions[nr][0];
	PRVM_G_VECTOR(OFS_RETURN)[1] = video_resolutions[nr][1];
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
VM_M_keynumtostring

string keynumtostring(float keynum)
=========
*/
void VM_M_keynumtostring(void)
{
	int keynum;
	char *tmp;
	VM_SAFEPARMCOUNT(1, VM_M_keynumtostring);

	keynum = PRVM_G_FLOAT(OFS_PARM0);

	tmp = VM_GetTempString();

	strcpy(tmp, Key_KeynumToString(keynum));

	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(tmp);
}

/*
=========
VM_M_stringtokeynum

float stringtokeynum(string key)
=========
*/
void VM_M_stringtokeynum( void )
{
	char *str;
	VM_SAFEPARMCOUNT( 1, VM_M_keynumtostring );

	str = PRVM_G_STRING( OFS_PARM0 );

	PRVM_G_INT(OFS_RETURN) = Key_StringToKeynum( str );
}

/*
=========
VM_M_findkeysforcommand

string	findkeysforcommand(string command)

the returned string is an altstring
=========
*/
#define NUMKEYS 5 // TODO: merge the constant in keys.c with this one somewhen

void M_FindKeysForCommand(char *command, int *keys);
void VM_M_findkeysforcommand(void)
{
	char *cmd, *ret;
	int keys[NUMKEYS];
	int i;

	VM_SAFEPARMCOUNT(1, VM_M_findkeysforcommand);

	cmd = PRVM_G_STRING(OFS_PARM0);

	VM_CheckEmptyString(cmd);

	(ret = VM_GetTempString())[0] = 0;

	M_FindKeysForCommand(cmd, keys);

	for(i = 0; i < NUMKEYS; i++)
		ret = strcat(ret, va(" \'%i\'", keys[i]));

	PRVM_G_INT(OFS_RETURN) = PRVM_SetString(ret);
}

/*
=========
VM_M_getserverliststat

float	getserverliststat(float type)
=========
*/
/*
	type:
0	serverlist_viewcount
1   serverlist_totalcount
2	masterquerycount
3	masterreplycount
4	serverquerycount
5	serverreplycount
6	sortfield
7	sortdescending
*/
void VM_M_getserverliststat( void )
{
	int type;
	VM_SAFEPARMCOUNT ( 1, VM_M_getserverliststat );

	PRVM_G_FLOAT( OFS_RETURN ) = 0;

	type = PRVM_G_FLOAT( OFS_PARM0 );
	switch(type)
	{
	case 0:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_viewcount;
		return;
	case 1:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_cachecount;
	case 2:
		PRVM_G_FLOAT ( OFS_RETURN ) = masterquerycount;
		return;
	case 3:
		PRVM_G_FLOAT ( OFS_RETURN ) = masterreplycount;
		return;
	case 4:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverquerycount;
		return;
	case 5:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverreplycount;
		return;
	case 6:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_sortbyfield;
		return;
	case 7:
		PRVM_G_FLOAT ( OFS_RETURN ) = serverlist_sortdescending;
		return;
	default:
		Con_Printf( "VM_M_getserverliststat: bad type %i!\n", type );
	}
}

/*
========================
VM_M_resetserverlistmasks

resetserverlistmasks()
========================
*/
void VM_M_resetserverlistmasks( void )
{
	ServerList_ResetMasks();
}


/*
========================
VM_M_setserverlistmaskstring

setserverlistmaskstring(float mask, float fld, string str, float op)
0-511		and
512 - 1024	or
========================
*/
void VM_M_setserverlistmaskstring( void )
{
	char *str;
	int masknr;
	serverlist_mask_t *mask;
	int field;

	VM_SAFEPARMCOUNT( 4, VM_M_setserverlistmaskstring );
	str = PRVM_G_STRING( OFS_PARM1 );
	if( !str )
		PRVM_ERROR( "VM_M_setserverlistmaskstring: null string passed!" );

	masknr = PRVM_G_FLOAT( OFS_PARM0 );
	if( masknr >= 0 && masknr <= SERVERLIST_ANDMASKCOUNT )
		mask = &serverlist_andmasks[masknr];
	else if( masknr >= 512 && masknr - 512 <= SERVERLIST_ORMASKCOUNT )
		mask = &serverlist_ormasks[masknr - 512 ];
	else {
		Con_Printf( "VM_M_setserverlistmaskstring: invalid mask number %i\n", masknr );
		return;
	}

	field = (int) PRVM_G_FLOAT( OFS_PARM1 );

	switch( field ) {
		case SLIF_CNAME:
			strncpy( mask->info.cname, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.cname) );
			break;
		case SLIF_NAME:
			strncpy( mask->info.name, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.name)  );
			break;
		case SLIF_MAP:
			strncpy( mask->info.map, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.map)  );
			break;
		case SLIF_MOD:
			strncpy( mask->info.mod, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.mod)  );
			break;
		case SLIF_GAME:
			strncpy( mask->info.game, PRVM_G_STRING( OFS_PARM2 ), sizeof(mask->info.game)  );
			break;
		default:
			Con_Printf( "VM_M_setserverlistmaskstring: Bad field number %i passed!\n", field );
			return;
	}

	mask->active = true;
	mask->tests[field] = (int) PRVM_G_FLOAT( OFS_PARM3 );
}

/*
========================
VM_M_setserverlistmasknumber

setserverlistmasknumber(float mask, float fld, float num, float op)

0-511		and
512 - 1024	or
========================
*/
void VM_M_setserverlistmasknumber( void )
{
	int number;
	serverlist_mask_t *mask;
	int	masknr;
	int field;
	VM_SAFEPARMCOUNT( 4, VM_M_setserverlistmasknumber );

	masknr = PRVM_G_FLOAT( OFS_PARM0 );
	if( masknr >= 0 && masknr <= SERVERLIST_ANDMASKCOUNT )
		mask = &serverlist_andmasks[masknr];
	else if( masknr >= 512 && masknr - 512 <= SERVERLIST_ORMASKCOUNT )
		mask = &serverlist_ormasks[masknr - 512 ];
	else {
		Con_Printf( "VM_M_setserverlistmasknumber: invalid mask number %i\n", masknr );
		return;
	}

	number = PRVM_G_FLOAT( OFS_PARM2 );
	field = (int) PRVM_G_FLOAT( OFS_PARM1 );

	switch( field ) {
		case SLIF_MAXPLAYERS:
			mask->info.maxplayers = number;
			break;
		case SLIF_NUMPLAYERS:
			mask->info.numplayers = number;
			break;
		case SLIF_PING:
			mask->info.ping = number;
			break;
		case SLIF_PROTOCOL:
			mask->info.protocol = number;
			break;
		default:
			Con_Printf( "VM_M_setserverlistmasknumber: Bad field number %i passed!\n", field );
			return;
	}

	mask->active = true;
	mask->tests[field] = (int) PRVM_G_FLOAT( OFS_PARM3 );
}


/*
========================
VM_M_resortserverlist

resortserverlist
========================
*/
void VM_M_resortserverlist( void )
{
	ServerList_RebuildViewList();
}

/*
=========
VM_M_getserverliststring

string	getserverliststring(float field, float hostnr)
=========
*/
void VM_M_getserverliststring(void)
{
	serverlist_entry_t *cache;
	int hostnr;

	VM_SAFEPARMCOUNT(2, VM_M_getserverliststring);

	PRVM_G_INT(OFS_RETURN) = 0;

	hostnr = PRVM_G_FLOAT(OFS_PARM1);

	if(hostnr < 0 || hostnr >= serverlist_viewcount)
	{
		Con_Print("VM_M_getserverliststring: bad hostnr passed!\n");
		return;
	}
	cache = serverlist_viewlist[hostnr];
	switch( (int) PRVM_G_FLOAT(OFS_PARM0) ) {
		case SLIF_CNAME:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->info.cname );
			break;
		case SLIF_NAME:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->info.name );
			break;
		case SLIF_GAME:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->info.game );
			break;
		case SLIF_MOD:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->info.mod );
			break;
		case SLIF_MAP:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->info.map );
			break;
		// TODO remove this again
		case 1024:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->line1 );
			break;
		case 1025:
			PRVM_G_INT( OFS_RETURN ) = PRVM_SetString( cache->line2 );
			break;
		default:
			Con_Print("VM_M_getserverliststring: bad field number passed!\n");
	}
}

/*
=========
VM_M_getserverlistnumber

float	getserverlistnumber(float field, float hostnr)
=========
*/
void VM_M_getserverlistnumber(void)
{
	serverlist_entry_t *cache;
	int hostnr;

	VM_SAFEPARMCOUNT(2, VM_M_getserverliststring);

	PRVM_G_INT(OFS_RETURN) = 0;

	hostnr = PRVM_G_FLOAT(OFS_PARM1);

	if(hostnr < 0 || hostnr >= serverlist_viewcount)
	{
		Con_Print("VM_M_getserverliststring: bad hostnr passed!\n");
		return;
	}
	cache = serverlist_viewlist[hostnr];
	switch( (int) PRVM_G_FLOAT(OFS_PARM0) ) {
		case SLIF_MAXPLAYERS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.maxplayers;
			break;
		case SLIF_NUMPLAYERS:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.numplayers;
			break;
		case SLIF_PING:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.ping;
			break;
		case SLIF_PROTOCOL:
			PRVM_G_FLOAT( OFS_RETURN ) = cache->info.protocol;
			break;
		default:
			Con_Print("VM_M_getserverlistnumber: bad field number passed!\n");
	}
}

/*
========================
VM_M_setserverlistsort

setserverlistsort(float field, float descending)
========================
*/
void VM_M_setserverlistsort( void )
{
	VM_SAFEPARMCOUNT( 2, VM_M_setserverlistsort );

	serverlist_sortbyfield = (int) PRVM_G_FLOAT( OFS_PARM0 );
	serverlist_sortdescending = (qboolean) PRVM_G_FLOAT( OFS_PARM1 );
}

/*
========================
VM_M_refreshserverlist

refreshserverlist()
========================
*/
void VM_M_refreshserverlist( void )
{
	VM_SAFEPARMCOUNT( 0, VM_M_refreshserverlist );
	ServerList_QueryList();
}

/*
========================
VM_M_getserverlistindexforkey

float getserverlistindexforkey(string key)
========================
*/
void VM_M_getserverlistindexforkey( void )
{
	char *key;
	VM_SAFEPARMCOUNT( 1, VM_M_getserverlistindexforkey );

	key = PRVM_G_STRING( OFS_PARM0 );
	VM_CheckEmptyString( key );

	if( !strcmp( key, "cname" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_CNAME;
	else if( !strcmp( key, "ping" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_PING;
	else if( !strcmp( key, "game" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_GAME;
	else if( !strcmp( key, "mod" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MOD;
	else if( !strcmp( key, "map" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MAP;
	else if( !strcmp( key, "name" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NAME;
	else if( !strcmp( key, "maxplayers" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_MAXPLAYERS;
	else if( !strcmp( key, "numplayers" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_NUMPLAYERS;
	else if( !strcmp( key, "protocol" ) )
		PRVM_G_FLOAT( OFS_RETURN ) = SLIF_PROTOCOL;
	else
		PRVM_G_FLOAT( OFS_RETURN ) = -1;
}

/*
========================
VM_M_addwantedserverlistkey

addwantedserverlistkey(string key)
========================
*/
void VM_M_addwantedserverlistkey( void )
{
	VM_SAFEPARMCOUNT( 1, VM_M_addwantedserverlistkey );
}

prvm_builtin_t vm_m_builtins[] = {
	0, // to be consistent with the old vm
	// common builtings (mostly)
	VM_checkextension,
	VM_error,
	VM_objerror,
	VM_print,
	VM_bprint,
	VM_sprint,
	VM_centerprint,
	VM_normalize,
	VM_vlen,
	VM_vectoyaw,	// #10
	VM_vectoangles,
	VM_random,
	VM_localcmd,
	VM_cvar,
	VM_cvar_set,
	VM_dprint,
	VM_ftos,
	VM_fabs,
	VM_vtos,
	VM_etos,		// 20
	VM_stof,
	VM_spawn,
	VM_remove,
	VM_find,
	VM_findfloat,
	VM_findchain,
	VM_findchainfloat,
	VM_precache_file,
	VM_precache_sound,
	VM_coredump,	// 30
	VM_traceon,
	VM_traceoff,
	VM_eprint,
	VM_rint,
	VM_floor,
	VM_ceil,
	VM_nextent,
	VM_sin,
	VM_cos,
	VM_sqrt,		// 40
	VM_randomvec,
	VM_registercvar,
	VM_min,
	VM_max,
	VM_bound,
	VM_pow,
	VM_copyentity,
	VM_fopen,
	VM_fclose,
	VM_fgets,		// 50
	VM_fputs,
	VM_strlen,
	VM_strcat,
	VM_substring,
	VM_stov,
	VM_strzone,
	VM_strunzone,
	VM_tokenize,
	VM_argv,
	VM_isserver,	// 60
	VM_clientcount,
	VM_clientstate,
	VM_clcommand,
	VM_changelevel,
	VM_localsound,
	VM_getmousepos,
	VM_gettime,
	VM_loadfromdata,
	VM_loadfromfile,
	VM_modulo,		// 70
	VM_str_cvar,
	VM_crash,
	VM_stackdump,	// 73
	VM_search_begin,
	VM_search_end,
	VM_search_getsize,
	VM_search_getfilename, // 77
	VM_chr,
	VM_itof,
	VM_ftoi,		// 80
	VM_itof,		// isString
	VM_altstr_count,
	VM_altstr_prepare,
	VM_altstr_get,
	VM_altstr_set,
	VM_altstr_ins,	// 86
	0,0,0,0,	// 90
	e10,			// 100
	e100,			// 200
	e100,			// 300
	e100,			// 400
	// msg functions
	VM_WriteByte,
	VM_WriteChar,
	VM_WriteShort,
	VM_WriteLong,
	VM_WriteAngle,
	VM_WriteCoord,
	VM_WriteString,
	VM_WriteEntity,	// 408
	0,
	0,				// 410
	e10,			// 420
	e10,			// 430
	e10,			// 440
	e10,			// 450
	// draw functions
	VM_iscachedpic,
	VM_precache_pic,
	VM_freepic,
	VM_drawcharacter,
	VM_drawstring,
	VM_drawpic,
	VM_drawfill,
	VM_drawsetcliparea,
	VM_drawresetcliparea,
	VM_getimagesize,// 460
	VM_cin_open,
	VM_cin_close,
	VM_cin_setstate,
	VM_cin_getstate,
	VM_cin_restart, // 465
	0,0,0,0,0,	// 470
	e10,			// 480
	e10,			// 490
	e10,			// 500
	e100,			// 600
	// menu functions
	VM_M_setkeydest,
	VM_M_getkeydest,
	VM_M_setmousetarget,
	VM_M_getmousetarget,
	VM_M_callfunction,
	VM_M_writetofile,
	VM_M_isfunction,
	VM_M_getresolution,
	VM_M_keynumtostring,
	VM_M_findkeysforcommand,// 610
	VM_M_getserverliststat,
	VM_M_getserverliststring,
	VM_M_parseentitydata,
	VM_M_stringtokeynum,
	VM_M_resetserverlistmasks,
	VM_M_setserverlistmaskstring,
	VM_M_setserverlistmasknumber,
	VM_M_resortserverlist,
	VM_M_setserverlistsort,
	VM_M_refreshserverlist,
	VM_M_getserverlistnumber,
	VM_M_getserverlistindexforkey,
	VM_M_addwantedserverlistkey // 623
};

const int vm_m_numbuiltins = sizeof(vm_m_builtins) / sizeof(prvm_builtin_t);

void VM_M_Cmd_Init(void)
{
	VM_Cmd_Init();
}

void VM_M_Cmd_Reset(void)
{
	//VM_Cmd_Init();
	VM_Cmd_Reset();
}
