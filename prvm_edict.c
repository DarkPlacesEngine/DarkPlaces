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
// AK new vm

#include "quakedef.h"
#include "progsvm.h"
#include "csprogs.h"

prvm_prog_t *prog;

static prvm_prog_t prog_list[PRVM_MAXPROGS];

int		prvm_type_size[8] = {1,sizeof(string_t)/4,1,3,1,1,sizeof(func_t)/4,sizeof(void *)/4};

prvm_eval_t prvm_badvalue; // used only for error returns

ddef_t *PRVM_ED_FieldAtOfs(int ofs);
qboolean PRVM_ED_ParseEpair(prvm_edict_t *ent, ddef_t *key, const char *s, qboolean parsebackslash);

cvar_t prvm_language = {CVAR_SAVE, "prvm_language", "", "when set, loads progs.dat.LANGUAGENAME.po for string translations; when set to dump, progs.dat.pot is written from the strings in the progs"};
// LordHavoc: prints every opcode as it executes - warning: this is significant spew
cvar_t prvm_traceqc = {0, "prvm_traceqc", "0", "prints every QuakeC statement as it is executed (only for really thorough debugging!)"};
// LordHavoc: counts usage of each QuakeC statement
cvar_t prvm_statementprofiling = {0, "prvm_statementprofiling", "0", "counts how many times each QuakeC statement has been executed, these counts are displayed in prvm_printfunction output (if enabled)"};
cvar_t prvm_timeprofiling = {0, "prvm_timeprofiling", "0", "counts how long each function has been executed, these counts are displayed in prvm_profile output (if enabled)"};
cvar_t prvm_backtraceforwarnings = {0, "prvm_backtraceforwarnings", "0", "print a backtrace for warnings too"};
cvar_t prvm_leaktest = {0, "prvm_leaktest", "0", "try to detect memory leaks in strings or entities"};
cvar_t prvm_leaktest_ignore_classnames = {0, "prvm_leaktest_ignore_classnames", "", "classnames of entities to NOT leak check because they are found by find(world, classname, ...) but are actually spawned by QC code (NOT map entities)"};
cvar_t prvm_errordump = {0, "prvm_errordump", "0", "write a savegame on crash to crash-server.dmp"};
cvar_t prvm_reuseedicts_startuptime = {0, "prvm_reuseedicts_startuptime", "2", "allows immediate re-use of freed entity slots during start of new level (value in seconds)"};
cvar_t prvm_reuseedicts_neverinsameframe = {0, "prvm_reuseedicts_neverinsameframe", "1", "never allows re-use of freed entity slots during same frame"};

static double prvm_reuseedicts_always_allow = 0;
qboolean prvm_runawaycheck = true;

extern sizebuf_t vm_tempstringsbuf;

//============================================================================
// mempool handling

/*
===============
PRVM_MEM_Alloc
===============
*/
void PRVM_MEM_Alloc(void)
{
	int i;

	// reserve space for the null entity aka world
	// check bound of max_edicts
	prog->max_edicts = bound(1 + prog->reserved_edicts, prog->max_edicts, prog->limit_edicts);
	prog->num_edicts = bound(1 + prog->reserved_edicts, prog->num_edicts, prog->max_edicts);

	// edictprivate_size has to be min as big prvm_edict_private_t
	prog->edictprivate_size = max(prog->edictprivate_size,(int)sizeof(prvm_edict_private_t));

	// alloc edicts
	prog->edicts = (prvm_edict_t *)Mem_Alloc(prog->progs_mempool,prog->limit_edicts * sizeof(prvm_edict_t));

	// alloc edict private space
	prog->edictprivate = Mem_Alloc(prog->progs_mempool, prog->max_edicts * prog->edictprivate_size);

	// alloc edict fields
	prog->entityfieldsarea = prog->entityfields * prog->max_edicts;
	prog->edictsfields = (vec_t *)Mem_Alloc(prog->progs_mempool, prog->entityfieldsarea * sizeof(vec_t));

	// set edict pointers
	for(i = 0; i < prog->max_edicts; i++)
	{
		prog->edicts[i].priv.required = (prvm_edict_private_t *)((unsigned char  *)prog->edictprivate + i * prog->edictprivate_size);
		prog->edicts[i].fields.vp = prog->edictsfields + i * prog->entityfields;
	}
}

/*
===============
PRVM_MEM_IncreaseEdicts
===============
*/
void PRVM_MEM_IncreaseEdicts(void)
{
	int		i;

	if(prog->max_edicts >= prog->limit_edicts)
		return;

	PRVM_GCALL(begin_increase_edicts)();

	// increase edicts
	prog->max_edicts = min(prog->max_edicts + 256, prog->limit_edicts);

	prog->entityfieldsarea = prog->entityfields * prog->max_edicts;
	prog->edictsfields = (vec_t*)Mem_Realloc(prog->progs_mempool, (void *)prog->edictsfields, prog->entityfieldsarea * sizeof(vec_t));
	prog->edictprivate = (void *)Mem_Realloc(prog->progs_mempool, (void *)prog->edictprivate, prog->max_edicts * prog->edictprivate_size);

	//set e and v pointers
	for(i = 0; i < prog->max_edicts; i++)
	{
		prog->edicts[i].priv.required  = (prvm_edict_private_t *)((unsigned char  *)prog->edictprivate + i * prog->edictprivate_size);
		prog->edicts[i].fields.vp = prog->edictsfields + i * prog->entityfields;
	}

	PRVM_GCALL(end_increase_edicts)();
}

//============================================================================
// normal prvm

int PRVM_ED_FindFieldOffset(const char *field)
{
	ddef_t *d;
	d = PRVM_ED_FindField(field);
	if (!d)
		return -1;
	return d->ofs;
}

int PRVM_ED_FindGlobalOffset(const char *global)
{
	ddef_t *d;
	d = PRVM_ED_FindGlobal(global);
	if (!d)
		return -1;
	return d->ofs;
}

func_t PRVM_ED_FindFunctionOffset(const char *function)
{
	mfunction_t *f;
	f = PRVM_ED_FindFunction(function);
	if (!f)
		return 0;
	return (func_t)(f - prog->functions);
}

qboolean PRVM_ProgLoaded(int prognr)
{
	if(prognr < 0 || prognr >= PRVM_MAXPROGS)
		return FALSE;

	return (prog_list[prognr].loaded ? TRUE : FALSE);
}

/*
=================
PRVM_SetProgFromString
=================
*/
// perhaps add a return value when the str doesnt exist
qboolean PRVM_SetProgFromString(const char *str)
{
	int i = 0;
	for(; i < PRVM_MAXPROGS ; i++)
		if(prog_list[i].name && !strcmp(prog_list[i].name,str))
		{
			if(prog_list[i].loaded)
			{
				prog = &prog_list[i];
				return TRUE;
			}
			else
			{
				Con_Printf("%s not loaded !\n",PRVM_NAME);
				return FALSE;
			}
		}

	Con_Printf("Invalid program name %s !\n", str);
	return FALSE;
}

/*
=================
PRVM_SetProg
=================
*/
void PRVM_SetProg(int prognr)
{
	if(0 <= prognr && prognr < PRVM_MAXPROGS)
	{
		if(prog_list[prognr].loaded)
			prog = &prog_list[prognr];
		else
			PRVM_ERROR("%i not loaded !", prognr);
		return;
	}
	PRVM_ERROR("Invalid program number %i", prognr);
}

/*
=================
PRVM_ED_ClearEdict

Sets everything to NULL
=================
*/
void PRVM_ED_ClearEdict (prvm_edict_t *e)
{
	memset (e->fields.vp, 0, prog->entityfields * 4);
	e->priv.required->free = false;

	// AK: Let the init_edict function determine if something needs to be initialized
	PRVM_GCALL(init_edict)(e);
}

const char *PRVM_AllocationOrigin(void)
{
	char *buf = NULL;
	if(prog->leaktest_active)
	if(prog->depth > 0) // actually in QC code and not just parsing the entities block of a map/savegame
	{
		buf = (char *)PRVM_Alloc(128);
		PRVM_ShortStackTrace(buf, 128);
	}
	return buf;
}

/*
=================
PRVM_ED_CanAlloc

Returns if this particular edict could get allocated by PRVM_ED_Alloc
=================
*/
qboolean PRVM_ED_CanAlloc(prvm_edict_t *e)
{
	if(!e->priv.required->free)
		return false;
	if(prvm_reuseedicts_always_allow == realtime)
		return true;
	if(realtime <= e->priv.required->freetime && prvm_reuseedicts_neverinsameframe.integer)
		return false; // never allow reuse in same frame (causes networking trouble)
	if(e->priv.required->freetime < prog->starttime + prvm_reuseedicts_startuptime.value)
		return true;
	if(realtime > e->priv.required->freetime + 1)
		return true;
	return false; // entity slot still blocked because the entity was freed less than one second ago
}

/*
=================
PRVM_ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
prvm_edict_t *PRVM_ED_Alloc (void)
{
	int			i;
	prvm_edict_t		*e;

	// the client qc dont need maxclients
	// thus it doesnt need to use svs.maxclients
	// AK:	changed i=svs.maxclients+1
	// AK:	changed so the edict 0 wont spawn -> used as reserved/world entity
	//		although the menu/client has no world
	for (i = prog->reserved_edicts + 1;i < prog->num_edicts;i++)
	{
		e = PRVM_EDICT_NUM(i);
		if(PRVM_ED_CanAlloc(e))
		{
			PRVM_ED_ClearEdict (e);
			e->priv.required->allocation_origin = PRVM_AllocationOrigin();
			return e;
		}
	}

	if (i == prog->limit_edicts)
		PRVM_ERROR ("%s: PRVM_ED_Alloc: no free edicts",PRVM_NAME);

	prog->num_edicts++;
	if (prog->num_edicts >= prog->max_edicts)
		PRVM_MEM_IncreaseEdicts();

	e = PRVM_EDICT_NUM(i);
	PRVM_ED_ClearEdict (e);

	e->priv.required->allocation_origin = PRVM_AllocationOrigin();

	return e;
}

/*
=================
PRVM_ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void PRVM_ED_Free (prvm_edict_t *ed)
{
	// dont delete the null entity (world) or reserved edicts
	if(PRVM_NUM_FOR_EDICT(ed) <= prog->reserved_edicts )
		return;

	PRVM_GCALL(free_edict)(ed);

	ed->priv.required->free = true;
	ed->priv.required->freetime = realtime;
	if(ed->priv.required->allocation_origin)
	{
		PRVM_Free((char *)ed->priv.required->allocation_origin);
		ed->priv.required->allocation_origin = NULL;
	}
}

//===========================================================================

/*
============
PRVM_ED_GlobalAtOfs
============
*/
ddef_t *PRVM_ED_GlobalAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i = 0;i < prog->numglobaldefs;i++)
	{
		def = &prog->globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
PRVM_ED_FieldAtOfs
============
*/
ddef_t *PRVM_ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i = 0;i < prog->numfielddefs;i++)
	{
		def = &prog->fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
PRVM_ED_FindField
============
*/
ddef_t *PRVM_ED_FindField (const char *name)
{
	ddef_t *def;
	int i;

	for (i = 0;i < prog->numfielddefs;i++)
	{
		def = &prog->fielddefs[i];
		if (!strcmp(PRVM_GetString(def->s_name), name))
			return def;
	}
	return NULL;
}

/*
============
PRVM_ED_FindGlobal
============
*/
ddef_t *PRVM_ED_FindGlobal (const char *name)
{
	ddef_t *def;
	int i;

	for (i = 0;i < prog->numglobaldefs;i++)
	{
		def = &prog->globaldefs[i];
		if (!strcmp(PRVM_GetString(def->s_name), name))
			return def;
	}
	return NULL;
}


/*
============
PRVM_ED_FindFunction
============
*/
mfunction_t *PRVM_ED_FindFunction (const char *name)
{
	mfunction_t		*func;
	int				i;

	for (i = 0;i < prog->numfunctions;i++)
	{
		func = &prog->functions[i];
		if (!strcmp(PRVM_GetString(func->s_name), name))
			return func;
	}
	return NULL;
}


/*
============
PRVM_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PRVM_ValueString (etype_t type, prvm_eval_t *val)
{
	static char line[MAX_INPUTLINE];
	ddef_t *def;
	mfunction_t *f;
	int n;

	type = (etype_t)((int) type & ~DEF_SAVEGLOBAL);

	switch (type)
	{
	case ev_string:
		strlcpy (line, PRVM_GetString (val->string), sizeof (line));
		break;
	case ev_entity:
		n = val->edict;
		if (n < 0 || n >= prog->max_edicts)
			dpsnprintf (line, sizeof(line), "entity %i (invalid!)", n);
		else
			dpsnprintf (line, sizeof(line), "entity %i", n);
		break;
	case ev_function:
		f = prog->functions + val->function;
		dpsnprintf (line, sizeof(line), "%s()", PRVM_GetString(f->s_name));
		break;
	case ev_field:
		def = PRVM_ED_FieldAtOfs ( val->_int );
		dpsnprintf (line, sizeof(line), ".%s", PRVM_GetString(def->s_name));
		break;
	case ev_void:
		dpsnprintf (line, sizeof(line), "void");
		break;
	case ev_float:
		// LordHavoc: changed from %5.1f to %10.4f
		dpsnprintf (line, sizeof(line), "%10.4f", val->_float);
		break;
	case ev_vector:
		// LordHavoc: changed from %5.1f to %10.4f
		dpsnprintf (line, sizeof(line), "'%10.4f %10.4f %10.4f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		dpsnprintf (line, sizeof(line), "pointer");
		break;
	default:
		dpsnprintf (line, sizeof(line), "bad type %i", (int) type);
		break;
	}

	return line;
}

/*
============
PRVM_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PRVM_UglyValueString (etype_t type, prvm_eval_t *val)
{
	static char line[MAX_INPUTLINE];
	int i;
	const char *s;
	ddef_t *def;
	mfunction_t *f;

	type = (etype_t)((int)type & ~DEF_SAVEGLOBAL);

	switch (type)
	{
	case ev_string:
		// Parse the string a bit to turn special characters
		// (like newline, specifically) into escape codes,
		// this fixes saving games from various mods
		s = PRVM_GetString (val->string);
		for (i = 0;i < (int)sizeof(line) - 2 && *s;)
		{
			if (*s == '\n')
			{
				line[i++] = '\\';
				line[i++] = 'n';
			}
			else if (*s == '\r')
			{
				line[i++] = '\\';
				line[i++] = 'r';
			}
			else if (*s == '\\')
			{
				line[i++] = '\\';
				line[i++] = '\\';
			}
			else if (*s == '"')
			{
				line[i++] = '\\';
				line[i++] = '"';
			}
			else
				line[i++] = *s;
			s++;
		}
		line[i] = '\0';
		break;
	case ev_entity:
		dpsnprintf (line, sizeof (line), "%i", PRVM_NUM_FOR_EDICT(PRVM_PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = prog->functions + val->function;
		strlcpy (line, PRVM_GetString (f->s_name), sizeof (line));
		break;
	case ev_field:
		def = PRVM_ED_FieldAtOfs ( val->_int );
		dpsnprintf (line, sizeof (line), ".%s", PRVM_GetString(def->s_name));
		break;
	case ev_void:
		dpsnprintf (line, sizeof (line), "void");
		break;
	case ev_float:
		dpsnprintf (line, sizeof (line), "%.9g", val->_float);
		break;
	case ev_vector:
		dpsnprintf (line, sizeof (line), "%.9g %.9g %.9g", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		dpsnprintf (line, sizeof (line), "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PRVM_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PRVM_GlobalString (int ofs)
{
	char	*s;
	//size_t	i;
	ddef_t	*def;
	void	*val;
	static char	line[128];

	val = (void *)&prog->globals.generic[ofs];
	def = PRVM_ED_GlobalAtOfs(ofs);
	if (!def)
		dpsnprintf (line, sizeof(line), "GLOBAL%i", ofs);
	else
	{
		s = PRVM_ValueString ((etype_t)def->type, (prvm_eval_t *)val);
		dpsnprintf (line, sizeof(line), "%s (=%s)", PRVM_GetString(def->s_name), s);
	}

	//i = strlen(line);
	//for ( ; i<20 ; i++)
	//	strcat (line," ");
	//strcat (line," ");

	return line;
}

char *PRVM_GlobalStringNoContents (int ofs)
{
	//size_t	i;
	ddef_t	*def;
	static char	line[128];

	def = PRVM_ED_GlobalAtOfs(ofs);
	if (!def)
		dpsnprintf (line, sizeof(line), "GLOBAL%i", ofs);
	else
		dpsnprintf (line, sizeof(line), "%s", PRVM_GetString(def->s_name));

	//i = strlen(line);
	//for ( ; i<20 ; i++)
	//	strcat (line," ");
	//strcat (line," ");

	return line;
}


/*
=============
PRVM_ED_Print

For debugging
=============
*/
// LordHavoc: optimized this to print out much more quickly (tempstring)
// LordHavoc: changed to print out every 4096 characters (incase there are a lot of fields to print)
void PRVM_ED_Print(prvm_edict_t *ed, const char *wildcard_fieldname)
{
	size_t	l;
	ddef_t	*d;
	int		*v;
	int		i, j;
	const char	*name;
	int		type;
	char	tempstring[MAX_INPUTLINE], tempstring2[260]; // temporary string buffers

	if (ed->priv.required->free)
	{
		Con_Printf("%s: FREE\n",PRVM_NAME);
		return;
	}

	tempstring[0] = 0;
	dpsnprintf(tempstring, sizeof(tempstring), "\n%s EDICT %i:\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ed));
	for (i = 1;i < prog->numfielddefs;i++)
	{
		d = &prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);
		if(strlen(name) > 1 && name[strlen(name)-2] == '_' && (name[strlen(name)-1] == 'x' || name[strlen(name)-1] == 'y' || name[strlen(name)-1] == 'z'))
			continue;	// skip _x, _y, _z vars

		// Check Field Name Wildcard
		if(wildcard_fieldname)
			if( !matchpattern(name, wildcard_fieldname, 1) )
				// Didn't match; skip
				continue;

		v = (int *)(ed->fields.vp + d->ofs);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j=0 ; j<prvm_type_size[type] ; j++)
			if (v[j])
				break;
		if (j == prvm_type_size[type])
			continue;

		if (strlen(name) > sizeof(tempstring2)-4)
		{
			memcpy (tempstring2, name, sizeof(tempstring2)-4);
			tempstring2[sizeof(tempstring2)-4] = tempstring2[sizeof(tempstring2)-3] = tempstring2[sizeof(tempstring2)-2] = '.';
			tempstring2[sizeof(tempstring2)-1] = 0;
			name = tempstring2;
		}
		strlcat(tempstring, name, sizeof(tempstring));
		for (l = strlen(name);l < 14;l++)
			strlcat(tempstring, " ", sizeof(tempstring));
		strlcat(tempstring, " ", sizeof(tempstring));

		name = PRVM_ValueString((etype_t)d->type, (prvm_eval_t *)v);
		if (strlen(name) > sizeof(tempstring2)-4)
		{
			memcpy (tempstring2, name, sizeof(tempstring2)-4);
			tempstring2[sizeof(tempstring2)-4] = tempstring2[sizeof(tempstring2)-3] = tempstring2[sizeof(tempstring2)-2] = '.';
			tempstring2[sizeof(tempstring2)-1] = 0;
			name = tempstring2;
		}
		strlcat(tempstring, name, sizeof(tempstring));
		strlcat(tempstring, "\n", sizeof(tempstring));
		if (strlen(tempstring) >= sizeof(tempstring)/2)
		{
			Con_Print(tempstring);
			tempstring[0] = 0;
		}
	}
	if (tempstring[0])
		Con_Print(tempstring);
}

/*
=============
PRVM_ED_Write

For savegames
=============
*/
extern cvar_t developer_entityparsing;
void PRVM_ED_Write (qfile_t *f, prvm_edict_t *ed)
{
	ddef_t	*d;
	int		*v;
	int		i, j;
	const char	*name;
	int		type;

	FS_Print(f, "{\n");

	if (ed->priv.required->free)
	{
		FS_Print(f, "}\n");
		return;
	}

	for (i = 1;i < prog->numfielddefs;i++)
	{
		d = &prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);

		if(developer_entityparsing.integer)
			Con_Printf("PRVM_ED_Write: at entity %d field %s\n", PRVM_NUM_FOR_EDICT(ed), name);

		//if(strlen(name) > 1 && name[strlen(name)-2] == '_' && (name[strlen(name)-1] == 'x' || name[strlen(name)-1] == 'y' || name[strlen(name)-1] == 'z'))
		if(strlen(name) > 1 && name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars, and ALSO other _? vars as some mods expect them to be never saved (TODO: a gameplayfix for using the "more precise" condition above?)

		v = (int *)(ed->fields.vp + d->ofs);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<prvm_type_size[type] ; j++)
			if (v[j])
				break;
		if (j == prvm_type_size[type])
			continue;

		FS_Printf(f,"\"%s\" ",name);
		prog->statestring = va("PRVM_ED_Write, ent=%d, name=%s", i, name);
		FS_Printf(f,"\"%s\"\n", PRVM_UglyValueString((etype_t)d->type, (prvm_eval_t *)v));
		prog->statestring = NULL;
	}

	FS_Print(f, "}\n");
}

void PRVM_ED_PrintNum (int ent, const char *wildcard_fieldname)
{
	PRVM_ED_Print(PRVM_EDICT_NUM(ent), wildcard_fieldname);
}

/*
=============
PRVM_ED_PrintEdicts_f

For debugging, prints all the entities in the current server
=============
*/
void PRVM_ED_PrintEdicts_f (void)
{
	int		i;
	const char *wildcard_fieldname;

	if(Cmd_Argc() < 2 || Cmd_Argc() > 3)
	{
		Con_Print("prvm_edicts <program name> <optional field name wildcard>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	if( Cmd_Argc() == 3)
		wildcard_fieldname = Cmd_Argv(2);
	else
		wildcard_fieldname = NULL;

	Con_Printf("%s: %i entities\n", PRVM_NAME, prog->num_edicts);
	for (i=0 ; i<prog->num_edicts ; i++)
		PRVM_ED_PrintNum (i, wildcard_fieldname);

	PRVM_End;
}

/*
=============
PRVM_ED_PrintEdict_f

For debugging, prints a single edict
=============
*/
void PRVM_ED_PrintEdict_f (void)
{
	int		i;
	const char	*wildcard_fieldname;

	if(Cmd_Argc() < 3 || Cmd_Argc() > 4)
	{
		Con_Print("prvm_edict <program name> <edict number> <optional field name wildcard>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	i = atoi (Cmd_Argv(2));
	if (i >= prog->num_edicts)
	{
		Con_Print("Bad edict number\n");
		PRVM_End;
		return;
	}
	if( Cmd_Argc() == 4)
		// Optional Wildcard Provided
		wildcard_fieldname = Cmd_Argv(3);
	else
		// Use All
		wildcard_fieldname = NULL;
	PRVM_ED_PrintNum (i, wildcard_fieldname);

	PRVM_End;
}

/*
=============
PRVM_ED_Count

For debugging
=============
*/
// 2 possibilities : 1. just displaying the active edict count
//					 2. making a function pointer [x]
void PRVM_ED_Count_f (void)
{
	int		i;
	prvm_edict_t	*ent;
	int		active;

	if(Cmd_Argc() != 2)
	{
		Con_Print("prvm_count <program name>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	if(prog->count_edicts)
		prog->count_edicts();
	else
	{
		active = 0;
		for (i=0 ; i<prog->num_edicts ; i++)
		{
			ent = PRVM_EDICT_NUM(i);
			if (ent->priv.required->free)
				continue;
			active++;
		}

		Con_Printf("num_edicts:%3i\n", prog->num_edicts);
		Con_Printf("active    :%3i\n", active);
	}

	PRVM_End;
}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
PRVM_ED_WriteGlobals
=============
*/
void PRVM_ED_WriteGlobals (qfile_t *f)
{
	ddef_t		*def;
	int			i;
	const char		*name;
	int			type;

	FS_Print(f,"{\n");
	for (i = 0;i < prog->numglobaldefs;i++)
	{
		def = &prog->globaldefs[i];
		type = def->type;
		if ( !(def->type & DEF_SAVEGLOBAL) )
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PRVM_GetString(def->s_name);

		if(developer_entityparsing.integer)
			Con_Printf("PRVM_ED_WriteGlobals: at global %s\n", name);

		prog->statestring = va("PRVM_ED_WriteGlobals, name=%s", name);
		FS_Printf(f,"\"%s\" ", name);
		FS_Printf(f,"\"%s\"\n", PRVM_UglyValueString((etype_t)type, (prvm_eval_t *)&prog->globals.generic[def->ofs]));
		prog->statestring = NULL;
	}
	FS_Print(f,"}\n");
}

/*
=============
PRVM_ED_ParseGlobals
=============
*/
void PRVM_ED_ParseGlobals (const char *data)
{
	char keyname[MAX_INPUTLINE];
	ddef_t *key;

	while (1)
	{
		// parse key
		if (!COM_ParseToken_Simple(&data, false, false))
			PRVM_ERROR ("PRVM_ED_ParseGlobals: EOF without closing brace");
		if (com_token[0] == '}')
			break;

		if (developer_entityparsing.integer)
			Con_Printf("Key: \"%s\"", com_token);

		strlcpy (keyname, com_token, sizeof(keyname));

		// parse value
		if (!COM_ParseToken_Simple(&data, false, true))
			PRVM_ERROR ("PRVM_ED_ParseGlobals: EOF without closing brace");

		if (developer_entityparsing.integer)
			Con_Printf(" \"%s\"\n", com_token);

		if (com_token[0] == '}')
			PRVM_ERROR ("PRVM_ED_ParseGlobals: closing brace without data");

		key = PRVM_ED_FindGlobal (keyname);
		if (!key)
		{
			Con_DPrintf("'%s' is not a global on %s\n", keyname, PRVM_NAME);
			continue;
		}

		if (!PRVM_ED_ParseEpair(NULL, key, com_token, true))
			PRVM_ERROR ("PRVM_ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
PRVM_ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
qboolean PRVM_ED_ParseEpair(prvm_edict_t *ent, ddef_t *key, const char *s, qboolean parsebackslash)
{
	int i, l;
	char *new_p;
	ddef_t *def;
	prvm_eval_t *val;
	mfunction_t *func;

	if (ent)
		val = (prvm_eval_t *)(ent->fields.vp + key->ofs);
	else
		val = (prvm_eval_t *)(prog->globals.generic + key->ofs);
	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		l = (int)strlen(s) + 1;
		val->string = PRVM_AllocString(l, &new_p);
		for (i = 0;i < l;i++)
		{
			if (s[i] == '\\' && s[i+1] && parsebackslash)
			{
				i++;
				if (s[i] == 'n')
					*new_p++ = '\n';
				else if (s[i] == 'r')
					*new_p++ = '\r';
				else
					*new_p++ = s[i];
			}
			else
				*new_p++ = s[i];
		}
		break;

	case ev_float:
		while (*s && ISWHITESPACE(*s))
			s++;
		val->_float = atof(s);
		break;

	case ev_vector:
		for (i = 0;i < 3;i++)
		{
			while (*s && ISWHITESPACE(*s))
				s++;
			if (!*s)
				break;
			val->vector[i] = atof(s);
			while (!ISWHITESPACE(*s))
				s++;
			if (!*s)
				break;
		}
		break;

	case ev_entity:
		while (*s && ISWHITESPACE(*s))
			s++;
		i = atoi(s);
		if (i >= prog->limit_edicts)
			Con_Printf("PRVM_ED_ParseEpair: ev_entity reference too large (edict %u >= MAX_EDICTS %u) on %s\n", (unsigned int)i, prog->limit_edicts, PRVM_NAME);
		while (i >= prog->max_edicts)
			PRVM_MEM_IncreaseEdicts();
		// if IncreaseEdicts was called the base pointer needs to be updated
		if (ent)
			val = (prvm_eval_t *)(ent->fields.vp + key->ofs);
		val->edict = PRVM_EDICT_TO_PROG(PRVM_EDICT_NUM((int)i));
		break;

	case ev_field:
		if (*s != '.')
		{
			Con_DPrintf("PRVM_ED_ParseEpair: Bogus field name %s in %s\n", s, PRVM_NAME);
			return false;
		}
		def = PRVM_ED_FindField(s + 1);
		if (!def)
		{
			Con_DPrintf("PRVM_ED_ParseEpair: Can't find field %s in %s\n", s, PRVM_NAME);
			return false;
		}
		val->_int = def->ofs;
		break;

	case ev_function:
		func = PRVM_ED_FindFunction(s);
		if (!func)
		{
			Con_Printf("PRVM_ED_ParseEpair: Can't find function %s in %s\n", s, PRVM_NAME);
			return false;
		}
		val->function = func - prog->functions;
		break;

	default:
		Con_Printf("PRVM_ED_ParseEpair: Unknown key->type %i for key \"%s\" on %s\n", key->type, PRVM_GetString(key->s_name), PRVM_NAME);
		return false;
	}
	return true;
}

/*
=============
PRVM_GameCommand_f

Console command to send a string to QC function GameCommand of the
indicated progs

Usage:
  sv_cmd adminmsg 3 "do not teamkill"
  cl_cmd someclientcommand
  menu_cmd somemenucommand

All progs can support this extension; sg calls it in server QC, cg in client
QC, mg in menu QC.
=============
*/
void PRVM_GameCommand(const char *whichprogs, const char *whichcmd)
{
	if(Cmd_Argc() < 1)
	{
		Con_Printf("%s text...\n", whichcmd);
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(whichprogs))
	// note: this is not PRVM_SetProg because that one aborts "hard" using PRVM_Error
	// also, it makes printing error messages easier!
	{
		Con_Printf("%s program not loaded.\n", whichprogs);
		return;
	}

	if(!PRVM_allfunction(GameCommand))
	{
		Con_Printf("%s program do not support GameCommand!\n", whichprogs);
	}
	else
	{
		int restorevm_tempstringsbuf_cursize;
		const char *s;

		s = Cmd_Args();

		restorevm_tempstringsbuf_cursize = vm_tempstringsbuf.cursize;
		PRVM_G_INT(OFS_PARM0) = PRVM_SetTempString(s ? s : "");
		PRVM_ExecuteProgram (PRVM_allfunction(GameCommand), "QC function GameCommand is missing");
		vm_tempstringsbuf.cursize = restorevm_tempstringsbuf_cursize;
	}

	PRVM_End;
}
void PRVM_GameCommand_Server_f(void)
{
	PRVM_GameCommand("server", "sv_cmd");
}
void PRVM_GameCommand_Client_f(void)
{
	PRVM_GameCommand("client", "cl_cmd");
}
void PRVM_GameCommand_Menu_f(void)
{
	PRVM_GameCommand("menu", "menu_cmd");
}

/*
=============
PRVM_ED_EdictGet_f

Console command to load a field of a specified edict
=============
*/
void PRVM_ED_EdictGet_f(void)
{
	prvm_edict_t *ed;
	ddef_t *key;
	const char *s;
	prvm_eval_t *v;

	if(Cmd_Argc() != 4 && Cmd_Argc() != 5)
	{
		Con_Print("prvm_edictget <program name> <edict number> <field> [<cvar>]\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
	{
		Con_Printf("Wrong program name %s !\n", Cmd_Argv(1));
		return;
	}

	ed = PRVM_EDICT_NUM(atoi(Cmd_Argv(2)));

	if((key = PRVM_ED_FindField(Cmd_Argv(3))) == 0)
	{
		Con_Printf("Key %s not found !\n", Cmd_Argv(3));
		goto fail;
	}

	v = (prvm_eval_t *)(ed->fields.vp + key->ofs);
	s = PRVM_UglyValueString((etype_t)key->type, v);
	if(Cmd_Argc() == 5)
	{
		cvar_t *cvar = Cvar_FindVar(Cmd_Argv(4));
		if (cvar && cvar->flags & CVAR_READONLY)
		{
			Con_Printf("prvm_edictget: %s is read-only\n", cvar->name);
			goto fail;
		}
		Cvar_Get(Cmd_Argv(4), s, 0, NULL);
	}
	else
		Con_Printf("%s\n", s);

fail:
	PRVM_End;
}

void PRVM_ED_GlobalGet_f(void)
{
	ddef_t *key;
	const char *s;
	prvm_eval_t *v;

	if(Cmd_Argc() != 3 && Cmd_Argc() != 4)
	{
		Con_Print("prvm_globalget <program name> <global> [<cvar>]\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
	{
		Con_Printf("Wrong program name %s !\n", Cmd_Argv(1));
		return;
	}

	key = PRVM_ED_FindGlobal(Cmd_Argv(2));
	if(!key)
	{
		Con_Printf( "No global '%s' in %s!\n", Cmd_Argv(2), Cmd_Argv(1) );
		goto fail;
	}

	v = (prvm_eval_t *) &prog->globals.generic[key->ofs];
	s = PRVM_UglyValueString((etype_t)key->type, v);
	if(Cmd_Argc() == 4)
	{
		cvar_t *cvar = Cvar_FindVar(Cmd_Argv(3));
		if (cvar && cvar->flags & CVAR_READONLY)
		{
			Con_Printf("prvm_globalget: %s is read-only\n", cvar->name);
			goto fail;
		}
		Cvar_Get(Cmd_Argv(3), s, 0, NULL);
	}
	else
		Con_Printf("%s\n", s);

fail:
	PRVM_End;
}

/*
=============
PRVM_ED_EdictSet_f

Console command to set a field of a specified edict
=============
*/
void PRVM_ED_EdictSet_f(void)
{
	prvm_edict_t *ed;
	ddef_t *key;

	if(Cmd_Argc() != 5)
	{
		Con_Print("prvm_edictset <program name> <edict number> <field> <value>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
	{
		Con_Printf("Wrong program name %s !\n", Cmd_Argv(1));
		return;
	}

	ed = PRVM_EDICT_NUM(atoi(Cmd_Argv(2)));

	if((key = PRVM_ED_FindField(Cmd_Argv(3))) == 0)
		Con_Printf("Key %s not found !\n", Cmd_Argv(3));
	else
		PRVM_ED_ParseEpair(ed, key, Cmd_Argv(4), true);

	PRVM_End;
}

/*
====================
PRVM_ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
const char *PRVM_ED_ParseEdict (const char *data, prvm_edict_t *ent)
{
	ddef_t *key;
	qboolean anglehack;
	qboolean init;
	char keyname[256];
	size_t n;

	init = false;

// go through all the dictionary pairs
	while (1)
	{
	// parse key
		if (!COM_ParseToken_Simple(&data, false, false))
			PRVM_ERROR ("PRVM_ED_ParseEdict: EOF without closing brace");
		if (developer_entityparsing.integer)
			Con_Printf("Key: \"%s\"", com_token);
		if (com_token[0] == '}')
			break;

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(com_token, "angle"))
		{
			strlcpy (com_token, "angles", sizeof(com_token));
			anglehack = true;
		}
		else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light"))
			strlcpy (com_token, "light_lev", sizeof(com_token));	// hack for single light def

		strlcpy (keyname, com_token, sizeof(keyname));

		// another hack to fix keynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

	// parse value
		if (!COM_ParseToken_Simple(&data, false, false))
			PRVM_ERROR ("PRVM_ED_ParseEdict: EOF without closing brace");
		if (developer_entityparsing.integer)
			Con_Printf(" \"%s\"\n", com_token);

		if (com_token[0] == '}')
			PRVM_ERROR ("PRVM_ED_ParseEdict: closing brace without data");

		init = true;

		// ignore attempts to set key "" (this problem occurs in nehahra neh1m8.bsp)
		if (!keyname[0])
			continue;

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		key = PRVM_ED_FindField (keyname);
		if (!key)
		{
			Con_DPrintf("%s: '%s' is not a field\n", PRVM_NAME, keyname);
			continue;
		}

		if (anglehack)
		{
			char	temp[32];
			strlcpy (temp, com_token, sizeof(temp));
			dpsnprintf (com_token, sizeof(com_token), "0 %s 0", temp);
		}

		if (!PRVM_ED_ParseEpair(ent, key, com_token, strcmp(keyname, "wad") != 0))
			PRVM_ERROR ("PRVM_ED_ParseEdict: parse error");
	}

	if (!init)
		ent->priv.required->free = true;

	return data;
}


/*
================
PRVM_ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
PRVM_ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call PRVM_ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void PRVM_ED_LoadFromFile (const char *data)
{
	prvm_edict_t *ent;
	int parsed, inhibited, spawned, died;
	const char *funcname;
	mfunction_t *func;

	parsed = 0;
	inhibited = 0;
	spawned = 0;
	died = 0;

	prvm_reuseedicts_always_allow = realtime;

// parse ents
	while (1)
	{
// parse the opening brace
		if (!COM_ParseToken_Simple(&data, false, false))
			break;
		if (com_token[0] != '{')
			PRVM_ERROR ("PRVM_ED_LoadFromFile: %s: found %s when expecting {", PRVM_NAME, com_token);

		// CHANGED: this is not conform to PR_LoadFromFile
		if(prog->loadintoworld)
		{
			prog->loadintoworld = false;
			ent = PRVM_EDICT_NUM(0);
		}
		else
			ent = PRVM_ED_Alloc();

		// clear it
		if (ent != prog->edicts)	// hack
			memset (ent->fields.vp, 0, prog->entityfields * 4);

		data = PRVM_ED_ParseEdict (data, ent);
		parsed++;

		// remove the entity ?
		if(prog->load_edict && !prog->load_edict(ent))
		{
			PRVM_ED_Free(ent);
			inhibited++;
			continue;
		}

		if (PRVM_serverfunction(SV_OnEntityPreSpawnFunction))
		{
			// self = ent
			PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(ent);
			PRVM_ExecuteProgram (PRVM_serverfunction(SV_OnEntityPreSpawnFunction), "QC function SV_OnEntityPreSpawnFunction is missing");
		}

		if(ent->priv.required->free)
		{
			inhibited++;
			continue;
		}

//
// immediately call spawn function, but only if there is a self global and a classname
//
		if(!ent->priv.required->free)
		{
			if (!PRVM_alledictstring(ent, classname))
			{
				Con_Print("No classname for:\n");
				PRVM_ED_Print(ent, NULL);
				PRVM_ED_Free (ent);
				continue;
			}

			// look for the spawn function
			funcname = PRVM_GetString(PRVM_alledictstring(ent, classname));
			func = PRVM_ED_FindFunction (va("spawnfunc_%s", funcname));
			if(!func)
				if(!PRVM_allglobalfloat(require_spawnfunc_prefix))
					func = PRVM_ED_FindFunction (funcname);

			if (!func)
			{
				// check for OnEntityNoSpawnFunction
				if (PRVM_serverfunction(SV_OnEntityNoSpawnFunction))
				{
					// self = ent
					PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(ent);
					PRVM_ExecuteProgram (PRVM_serverfunction(SV_OnEntityNoSpawnFunction), "QC function SV_OnEntityNoSpawnFunction is missing");
				}
				else
				{
					if (developer.integer > 0) // don't confuse non-developers with errors
					{
						Con_Print("No spawn function for:\n");
						PRVM_ED_Print(ent, NULL);
					}
					PRVM_ED_Free (ent);
					continue; // not included in "inhibited" count
				}
			}
			else
			{
				// self = ent
				PRVM_allglobaledict(self) = PRVM_EDICT_TO_PROG(ent);
				PRVM_ExecuteProgram (func - prog->functions, "");
			}
		}

		if(!ent->priv.required->free)
		if (PRVM_serverfunction(SV_OnEntityPostSpawnFunction))
		{
			// self = ent
			PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(ent);
			PRVM_ExecuteProgram (PRVM_serverfunction(SV_OnEntityPostSpawnFunction), "QC function SV_OnEntityPostSpawnFunction is missing");
		}

		spawned++;
		if (ent->priv.required->free)
			died++;
	}

	Con_DPrintf("%s: %i new entities parsed, %i new inhibited, %i (%i new) spawned (whereas %i removed self, %i stayed)\n", PRVM_NAME, parsed, inhibited, prog->num_edicts, spawned, died, spawned - died);

	prvm_reuseedicts_always_allow = 0;
}

void PRVM_FindOffsets(void)
{
	// field and global searches use -1 for NULL
	memset(&prog->fieldoffsets, -1, sizeof(prog->fieldoffsets));
	memset(&prog->globaloffsets, -1, sizeof(prog->globaloffsets));
	// function searches use 0 for NULL
	memset(&prog->funcoffsets, 0, sizeof(prog->funcoffsets));
#define PRVM_DECLARE_serverglobalfloat(x)
#define PRVM_DECLARE_serverglobalvector(x)
#define PRVM_DECLARE_serverglobalstring(x)
#define PRVM_DECLARE_serverglobaledict(x)
#define PRVM_DECLARE_serverglobalfunction(x)
#define PRVM_DECLARE_clientglobalfloat(x)
#define PRVM_DECLARE_clientglobalvector(x)
#define PRVM_DECLARE_clientglobalstring(x)
#define PRVM_DECLARE_clientglobaledict(x)
#define PRVM_DECLARE_clientglobalfunction(x)
#define PRVM_DECLARE_menuglobalfloat(x)
#define PRVM_DECLARE_menuglobalvector(x)
#define PRVM_DECLARE_menuglobalstring(x)
#define PRVM_DECLARE_menuglobaledict(x)
#define PRVM_DECLARE_menuglobalfunction(x)
#define PRVM_DECLARE_serverfieldfloat(x)
#define PRVM_DECLARE_serverfieldvector(x)
#define PRVM_DECLARE_serverfieldstring(x)
#define PRVM_DECLARE_serverfieldedict(x)
#define PRVM_DECLARE_serverfieldfunction(x)
#define PRVM_DECLARE_clientfieldfloat(x)
#define PRVM_DECLARE_clientfieldvector(x)
#define PRVM_DECLARE_clientfieldstring(x)
#define PRVM_DECLARE_clientfieldedict(x)
#define PRVM_DECLARE_clientfieldfunction(x)
#define PRVM_DECLARE_menufieldfloat(x)
#define PRVM_DECLARE_menufieldvector(x)
#define PRVM_DECLARE_menufieldstring(x)
#define PRVM_DECLARE_menufieldedict(x)
#define PRVM_DECLARE_menufieldfunction(x)
#define PRVM_DECLARE_serverfunction(x)
#define PRVM_DECLARE_clientfunction(x)
#define PRVM_DECLARE_menufunction(x)
#define PRVM_DECLARE_field(x) prog->fieldoffsets.x = PRVM_ED_FindFieldOffset(#x);
#define PRVM_DECLARE_global(x) prog->globaloffsets.x = PRVM_ED_FindGlobalOffset(#x);
#define PRVM_DECLARE_function(x) prog->funcoffsets.x = PRVM_ED_FindFunctionOffset(#x);
#include "prvm_offsets.h"
#undef PRVM_DECLARE_serverglobalfloat
#undef PRVM_DECLARE_serverglobalvector
#undef PRVM_DECLARE_serverglobalstring
#undef PRVM_DECLARE_serverglobaledict
#undef PRVM_DECLARE_serverglobalfunction
#undef PRVM_DECLARE_clientglobalfloat
#undef PRVM_DECLARE_clientglobalvector
#undef PRVM_DECLARE_clientglobalstring
#undef PRVM_DECLARE_clientglobaledict
#undef PRVM_DECLARE_clientglobalfunction
#undef PRVM_DECLARE_menuglobalfloat
#undef PRVM_DECLARE_menuglobalvector
#undef PRVM_DECLARE_menuglobalstring
#undef PRVM_DECLARE_menuglobaledict
#undef PRVM_DECLARE_menuglobalfunction
#undef PRVM_DECLARE_serverfieldfloat
#undef PRVM_DECLARE_serverfieldvector
#undef PRVM_DECLARE_serverfieldstring
#undef PRVM_DECLARE_serverfieldedict
#undef PRVM_DECLARE_serverfieldfunction
#undef PRVM_DECLARE_clientfieldfloat
#undef PRVM_DECLARE_clientfieldvector
#undef PRVM_DECLARE_clientfieldstring
#undef PRVM_DECLARE_clientfieldedict
#undef PRVM_DECLARE_clientfieldfunction
#undef PRVM_DECLARE_menufieldfloat
#undef PRVM_DECLARE_menufieldvector
#undef PRVM_DECLARE_menufieldstring
#undef PRVM_DECLARE_menufieldedict
#undef PRVM_DECLARE_menufieldfunction
#undef PRVM_DECLARE_serverfunction
#undef PRVM_DECLARE_clientfunction
#undef PRVM_DECLARE_menufunction
#undef PRVM_DECLARE_field
#undef PRVM_DECLARE_global
#undef PRVM_DECLARE_function
}

// not used
/*
typedef struct dpfield_s
{
	int type;
	char *string;
}
dpfield_t;

#define DPFIELDS (sizeof(dpfields) / sizeof(dpfield_t))

dpfield_t dpfields[] =
{
};
*/

/*
===============
PRVM_ResetProg
===============
*/

#define PO_HASHSIZE 16384
typedef struct po_string_s
{
	char *key, *value;
	struct po_string_s *nextonhashchain;
}
po_string_t;
typedef struct po_s
{
	po_string_t *hashtable[PO_HASHSIZE];
}
po_t;
void PRVM_PO_UnparseString(char *out, const char *in, size_t outsize)
{
	for(;;)
	{
		switch(*in)
		{
			case 0:
				*out++ = 0;
				return;
			case '\a': if(outsize >= 2) { *out++ = '\\'; *out++ = 'a'; outsize -= 2; } break;
			case '\b': if(outsize >= 2) { *out++ = '\\'; *out++ = 'b'; outsize -= 2; } break;
			case '\t': if(outsize >= 2) { *out++ = '\\'; *out++ = 't'; outsize -= 2; } break;
			case '\r': if(outsize >= 2) { *out++ = '\\'; *out++ = 'r'; outsize -= 2; } break;
			case '\n': if(outsize >= 2) { *out++ = '\\'; *out++ = 'n'; outsize -= 2; } break;
			case '\\': if(outsize >= 2) { *out++ = '\\'; *out++ = '\\'; outsize -= 2; } break;
			case '"': if(outsize >= 2) { *out++ = '\\'; *out++ = '"'; outsize -= 2; } break;
			default:
				if(*in >= 0 && *in <= 0x1F)
				{
					if(outsize >= 4)
					{
						*out++ = '\\';
						*out++ = '0' + ((*in & 0700) >> 6);
						*out++ = '0' + ((*in & 0070) >> 3);
						*out++ = '0' + ((*in & 0007));
						outsize -= 4;
					}
				}
				else
				{
					if(outsize >= 1)
					{
						*out++ = *in;
						outsize -= 1;
					}
				}
				break;
		}
		++in;
	}
}
void PRVM_PO_ParseString(char *out, const char *in, size_t outsize)
{
	for(;;)
	{
		switch(*in)
		{
			case 0:
				*out++ = 0;
				return;
			case '\\':
				++in;
				switch(*in)
				{
					case 'a': if(outsize > 0) { *out++ = '\a'; --outsize; } break;
					case 'b': if(outsize > 0) { *out++ = '\b'; --outsize; } break;
					case 't': if(outsize > 0) { *out++ = '\t'; --outsize; } break;
					case 'r': if(outsize > 0) { *out++ = '\r'; --outsize; } break;
					case 'n': if(outsize > 0) { *out++ = '\n'; --outsize; } break;
					case '\\': if(outsize > 0) { *out++ = '\\'; --outsize; } break;
					case '"': if(outsize > 0) { *out++ = '"'; --outsize; } break;
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
						if(outsize > 0) 
							*out = *in - '0';
						++in;
						if(*in >= '0' && *in <= '7')
						{
							if(outsize > 0)
								*out = (*out << 3) | (*in - '0');
							++in;
						}
						if(*in >= '0' && *in <= '7')
						{
							if(outsize > 0)
								*out = (*out << 3) | (*in - '0');
							++in;
						}
						--in;
						if(outsize > 0)
						{
							++out;
							--outsize;
						}
						break;
					default:
						if(outsize > 0) { *out++ = *in; --outsize; }
						break;
				}
				break;
			default:
				if(outsize > 0)
				{
					*out++ = *in;
					--outsize;
				}
				break;
		}
		++in;
	}
}
po_t *PRVM_PO_Load(const char *filename, mempool_t *pool)
{
	po_t *po;
	const char *p, *q;
	int mode;
	char inbuf[MAX_INPUTLINE];
	char decodedbuf[MAX_INPUTLINE];
	size_t decodedpos;
	int hashindex;
	po_string_t thisstr;
	const char *buf = (const char *) FS_LoadFile(filename, pool, true, NULL);

	if(!buf)
		return NULL;

	memset(&thisstr, 0, sizeof(thisstr)); // hush compiler warning

	po = (po_t *)Mem_Alloc(pool, sizeof(*po));
	memset(po, 0, sizeof(*po));

	p = buf;
	while(*p)
	{
		if(*p == '#')
		{
			// skip to newline
			p = strchr(p, '\n');
			if(!p)
				break;
			++p;
			continue;
		}
		if(*p == '\r' || *p == '\n')
		{
			++p;
			continue;
		}
		if(!strncmp(p, "msgid \"", 7))
		{
			mode = 0;
			p += 6;
		}
		else if(!strncmp(p, "msgstr \"", 8))
		{
			mode = 1;
			p += 7;
		}
		else
		{
			p = strchr(p, '\n');
			if(!p)
				break;
			++p;
			continue;
		}
		decodedpos = 0;
		while(*p == '"')
		{
			++p;
			q = strchr(p, '\n');
			if(!q)
				break;
			if(*(q-1) == '\r')
				--q;
			if(*(q-1) != '"')
				break;
			if((size_t)(q - p) >= (size_t) sizeof(inbuf))
				break;
			strlcpy(inbuf, p, q - p); // not - 1, because this adds a NUL
			PRVM_PO_ParseString(decodedbuf + decodedpos, inbuf, sizeof(decodedbuf) - decodedpos);
			decodedpos += strlen(decodedbuf + decodedpos);
			if(*q == '\r')
				++q;
			if(*q == '\n')
				++q;
			p = q;
		}
		if(mode == 0)
		{
			if(thisstr.key)
				Mem_Free(thisstr.key);
			thisstr.key = (char *)Mem_Alloc(pool, decodedpos + 1);
			memcpy(thisstr.key, decodedbuf, decodedpos + 1);
		}
		else if(decodedpos > 0 && thisstr.key) // skip empty translation results
		{
			thisstr.value = (char *)Mem_Alloc(pool, decodedpos + 1);
			memcpy(thisstr.value, decodedbuf, decodedpos + 1);
			hashindex = CRC_Block((const unsigned char *) thisstr.key, strlen(thisstr.key)) % PO_HASHSIZE;
			thisstr.nextonhashchain = po->hashtable[hashindex];
			po->hashtable[hashindex] = (po_string_t *)Mem_Alloc(pool, sizeof(thisstr));
			memcpy(po->hashtable[hashindex], &thisstr, sizeof(thisstr));
			memset(&thisstr, 0, sizeof(thisstr));
		}
	}
	
	Mem_Free((char *) buf);
	return po;
}
const char *PRVM_PO_Lookup(po_t *po, const char *str)
{
	int hashindex = CRC_Block((const unsigned char *) str, strlen(str)) % PO_HASHSIZE;
	po_string_t *p = po->hashtable[hashindex];
	while(p)
	{
		if(!strcmp(str, p->key))
			return p->value;
		p = p->nextonhashchain;
	}
	return NULL;
}
void PRVM_PO_Destroy(po_t *po)
{
	int i;
	for(i = 0; i < PO_HASHSIZE; ++i)
	{
		po_string_t *p = po->hashtable[i];
		while(p)
		{
			po_string_t *q = p;
			p = p->nextonhashchain;
			Mem_Free(q->key);
			Mem_Free(q->value);
			Mem_Free(q);
		}
	}
	Mem_Free(po);
}

void PRVM_LeakTest(void);
void PRVM_ResetProg(void)
{
	PRVM_LeakTest();
	PRVM_GCALL(reset_cmd)();
	Mem_FreePool(&prog->progs_mempool);
	if(prog->po)
		PRVM_PO_Destroy((po_t *) prog->po);
	memset(prog,0,sizeof(prvm_prog_t));
	prog->starttime = Sys_DoubleTime();
}

/*
===============
PRVM_LoadLNO
===============
*/
void PRVM_LoadLNO( const char *progname ) {
	fs_offset_t filesize;
	unsigned char *lno;
	unsigned int *header;
	char filename[512];

	FS_StripExtension( progname, filename, sizeof( filename ) );
	strlcat( filename, ".lno", sizeof( filename ) );

	lno = FS_LoadFile( filename, tempmempool, false, &filesize );
	if( !lno ) {
		return;
	}

/*
<Spike>    SafeWrite (h, &lnotype, sizeof(int));
<Spike>    SafeWrite (h, &version, sizeof(int));
<Spike>    SafeWrite (h, &numglobaldefs, sizeof(int));
<Spike>    SafeWrite (h, &numpr_globals, sizeof(int));
<Spike>    SafeWrite (h, &numfielddefs, sizeof(int));
<Spike>    SafeWrite (h, &numstatements, sizeof(int));
<Spike>    SafeWrite (h, statement_linenums, numstatements*sizeof(int));
*/
	if ((unsigned int)filesize < (6 + prog->progs_numstatements) * sizeof(int))
	{
		Mem_Free(lno);
		return;
	}

	header = (unsigned int *) lno;
	if( header[ 0 ] == *(unsigned int *) "LNOF" &&
		LittleLong( header[ 1 ] ) == 1 &&
		(unsigned int)LittleLong( header[ 2 ] ) == (unsigned int)prog->progs_numglobaldefs &&
		(unsigned int)LittleLong( header[ 3 ] ) == (unsigned int)prog->progs_numglobals &&
		(unsigned int)LittleLong( header[ 4 ] ) == (unsigned int)prog->progs_numfielddefs &&
		(unsigned int)LittleLong( header[ 5 ] ) == (unsigned int)prog->progs_numstatements )
	{
		prog->statement_linenums = (int *)Mem_Alloc(prog->progs_mempool, prog->progs_numstatements * sizeof( int ) );
		memcpy( prog->statement_linenums, (int *) lno + 6, prog->progs_numstatements * sizeof( int ) );
	}
	Mem_Free( lno );
}

/*
===============
PRVM_LoadProgs
===============
*/
void PRVM_LoadProgs (const char * filename, int numrequiredfunc, const char **required_func, int numrequiredfields, prvm_required_field_t *required_field, int numrequiredglobals, prvm_required_field_t *required_global)
{
	int i;
	dprograms_t *dprograms;
	dstatement_t *instatements;
	ddef_t *infielddefs;
	ddef_t *inglobaldefs;
	float *inglobals;
	dfunction_t *infunctions;
	char *instrings;
	fs_offset_t filesize;
	int requiredglobalspace;
	opcode_t op;
	int a;
	int b;
	int c;

	if (prog->loaded)
		PRVM_ERROR ("PRVM_LoadProgs: there is already a %s program loaded!", PRVM_NAME );

	dprograms = (dprograms_t *)FS_LoadFile (filename, prog->progs_mempool, false, &filesize);
	if (dprograms == NULL || filesize < (fs_offset_t)sizeof(dprograms_t))
		PRVM_ERROR ("PRVM_LoadProgs: couldn't load %s for %s", filename, PRVM_NAME);
	// TODO bounds check header fields (e.g. numstatements), they must never go behind end of file

	Con_DPrintf("%s programs occupy %iK.\n", PRVM_NAME, (int)(filesize/1024));

	requiredglobalspace = 0;
	for (i = 0;i < numrequiredglobals;i++)
		requiredglobalspace += required_global[i].type == ev_vector ? 3 : 1;

	prog->filecrc = CRC_Block((unsigned char *)dprograms, filesize);

// byte swap the header
	prog->progs_version = LittleLong(dprograms->version);
	prog->progs_crc = LittleLong(dprograms->crc);
	if (prog->progs_version != PROG_VERSION)
		PRVM_ERROR ("%s: %s has wrong version number (%i should be %i)", PRVM_NAME, filename, prog->progs_version, PROG_VERSION);
	instatements = (dstatement_t *)((unsigned char *)dprograms + LittleLong(dprograms->ofs_statements));
	prog->progs_numstatements = LittleLong(dprograms->numstatements);
	inglobaldefs = (ddef_t *)((unsigned char *)dprograms + LittleLong(dprograms->ofs_globaldefs));
	prog->progs_numglobaldefs = LittleLong(dprograms->numglobaldefs);
	infielddefs = (ddef_t *)((unsigned char *)dprograms + LittleLong(dprograms->ofs_fielddefs));
	prog->progs_numfielddefs = LittleLong(dprograms->numfielddefs);
	infunctions = (dfunction_t *)((unsigned char *)dprograms + LittleLong(dprograms->ofs_functions));
	prog->progs_numfunctions = LittleLong(dprograms->numfunctions);
	instrings = (char *)((unsigned char *)dprograms + LittleLong(dprograms->ofs_strings));
	prog->progs_numstrings = LittleLong(dprograms->numstrings);
	inglobals = (float *)((unsigned char *)dprograms + LittleLong(dprograms->ofs_globals));
	prog->progs_numglobals = LittleLong(dprograms->numglobals);
	prog->progs_entityfields = LittleLong(dprograms->entityfields);

	prog->numstatements = prog->progs_numstatements;
	prog->numglobaldefs = prog->progs_numglobaldefs;
	prog->numfielddefs = prog->progs_numfielddefs;
	prog->numfunctions = prog->progs_numfunctions;
	prog->numstrings = prog->progs_numstrings;
	prog->numglobals = prog->progs_numglobals;
	prog->entityfields = prog->progs_entityfields;

	if (LittleLong(dprograms->ofs_strings) + prog->progs_numstrings >= (int)filesize)
		PRVM_ERROR ("%s: %s strings go past end of file", PRVM_NAME, filename);
	prog->strings = (char *)Mem_Alloc(prog->progs_mempool, prog->progs_numstrings);
	memcpy(prog->strings, instrings, prog->progs_numstrings);
	prog->stringssize = prog->progs_numstrings;

	prog->numknownstrings = 0;
	prog->maxknownstrings = 0;
	prog->knownstrings = NULL;
	prog->knownstrings_freeable = NULL;

	Mem_ExpandableArray_NewArray(&prog->stringbuffersarray, prog->progs_mempool, sizeof(prvm_stringbuffer_t), 64);

	// we need to expand the globaldefs and fielddefs to include engine defs
	prog->globaldefs = (ddef_t *)Mem_Alloc(prog->progs_mempool, (prog->progs_numglobaldefs + numrequiredglobals) * sizeof(ddef_t));
	prog->globals.generic = (float *)Mem_Alloc(prog->progs_mempool, (prog->progs_numglobals + requiredglobalspace) * sizeof(float));
	prog->fielddefs = (ddef_t *)Mem_Alloc(prog->progs_mempool, (prog->progs_numfielddefs + numrequiredfields) * sizeof(ddef_t));
	// we need to convert the statements to our memory format
	prog->statements = (mstatement_t *)Mem_Alloc(prog->progs_mempool, prog->progs_numstatements * sizeof(mstatement_t));
	// allocate space for profiling statement usage
	prog->statement_profile = (double *)Mem_Alloc(prog->progs_mempool, prog->progs_numstatements * sizeof(*prog->statement_profile));
	// functions need to be converted to the memory format
	prog->functions = (mfunction_t *)Mem_Alloc(prog->progs_mempool, sizeof(mfunction_t) * prog->progs_numfunctions);

	for (i = 0;i < prog->progs_numfunctions;i++)
	{
		prog->functions[i].first_statement = LittleLong(infunctions[i].first_statement);
		prog->functions[i].parm_start = LittleLong(infunctions[i].parm_start);
		prog->functions[i].s_name = LittleLong(infunctions[i].s_name);
		prog->functions[i].s_file = LittleLong(infunctions[i].s_file);
		prog->functions[i].numparms = LittleLong(infunctions[i].numparms);
		prog->functions[i].locals = LittleLong(infunctions[i].locals);
		memcpy(prog->functions[i].parm_size, infunctions[i].parm_size, sizeof(infunctions[i].parm_size));
		if(prog->functions[i].first_statement >= prog->numstatements)
			PRVM_ERROR("PRVM_LoadProgs: out of bounds function statement (function %d) in %s", i, PRVM_NAME);
		// TODO bounds check parm_start, s_name, s_file, numparms, locals, parm_size
	}

	// copy the globaldefs to the new globaldefs list
	for (i=0 ; i<prog->numglobaldefs ; i++)
	{
		prog->globaldefs[i].type = LittleShort(inglobaldefs[i].type);
		prog->globaldefs[i].ofs = LittleShort(inglobaldefs[i].ofs);
		prog->globaldefs[i].s_name = LittleLong(inglobaldefs[i].s_name);
		// TODO bounds check ofs, s_name
	}

	// append the required globals
	for (i = 0;i < numrequiredglobals;i++)
	{
		prog->globaldefs[prog->numglobaldefs].type = required_global[i].type;
		prog->globaldefs[prog->numglobaldefs].ofs = prog->numglobals;
		prog->globaldefs[prog->numglobaldefs].s_name = PRVM_SetEngineString(required_global[i].name);
		if (prog->globaldefs[prog->numglobaldefs].type == ev_vector)
			prog->numglobals += 3;
		else
			prog->numglobals++;
		prog->numglobaldefs++;
	}

	// copy the progs fields to the new fields list
	for (i = 0;i < prog->numfielddefs;i++)
	{
		prog->fielddefs[i].type = LittleShort(infielddefs[i].type);
		if (prog->fielddefs[i].type & DEF_SAVEGLOBAL)
			PRVM_ERROR ("PRVM_LoadProgs: prog->fielddefs[i].type & DEF_SAVEGLOBAL in %s", PRVM_NAME);
		prog->fielddefs[i].ofs = LittleShort(infielddefs[i].ofs);
		prog->fielddefs[i].s_name = LittleLong(infielddefs[i].s_name);
		// TODO bounds check ofs, s_name
	}

	// append the required fields
	for (i = 0;i < numrequiredfields;i++)
	{
		prog->fielddefs[prog->numfielddefs].type = required_field[i].type;
		prog->fielddefs[prog->numfielddefs].ofs = prog->entityfields;
		prog->fielddefs[prog->numfielddefs].s_name = PRVM_SetEngineString(required_field[i].name);
		if (prog->fielddefs[prog->numfielddefs].type == ev_vector)
			prog->entityfields += 3;
		else
			prog->entityfields++;
		prog->numfielddefs++;
	}

	// LordHavoc: TODO: reorder globals to match engine struct
	// LordHavoc: TODO: reorder fields to match engine struct
#define remapglobal(index) (index)
#define remapfield(index) (index)

	// copy globals
	for (i = 0;i < prog->progs_numglobals;i++)
		((int *)prog->globals.generic)[remapglobal(i)] = LittleLong(((int *)inglobals)[i]);

	// LordHavoc: TODO: support 32bit progs statement formats
	// copy, remap globals in statements, bounds check
	for (i = 0;i < prog->progs_numstatements;i++)
	{
		op = (opcode_t)LittleShort(instatements[i].op);
		a = (unsigned short)LittleShort(instatements[i].a);
		b = (unsigned short)LittleShort(instatements[i].b);
		c = (unsigned short)LittleShort(instatements[i].c);
		switch (op)
		{
		case OP_IF:
		case OP_IFNOT:
			b = (short)b;
			if (a >= prog->progs_numglobals || b + i < 0 || b + i >= prog->progs_numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds IF/IFNOT (statement %d) in %s", i, PRVM_NAME);
			prog->statements[i].op = op;
			prog->statements[i].operand[0] = remapglobal(a);
			prog->statements[i].operand[1] = -1;
			prog->statements[i].operand[2] = -1;
			prog->statements[i].jumpabsolute = i + b;
			break;
		case OP_GOTO:
			a = (short)a;
			if (a + i < 0 || a + i >= prog->progs_numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds GOTO (statement %d) in %s", i, PRVM_NAME);
			prog->statements[i].op = op;
			prog->statements[i].operand[0] = -1;
			prog->statements[i].operand[1] = -1;
			prog->statements[i].operand[2] = -1;
			prog->statements[i].jumpabsolute = i + a;
			break;
		default:
			Con_DPrintf("PRVM_LoadProgs: unknown opcode %d at statement %d in %s\n", (int)op, i, PRVM_NAME);
		// global global global
		case OP_ADD_F:
		case OP_ADD_V:
		case OP_SUB_F:
		case OP_SUB_V:
		case OP_MUL_F:
		case OP_MUL_V:
		case OP_MUL_FV:
		case OP_MUL_VF:
		case OP_DIV_F:
		case OP_BITAND:
		case OP_BITOR:
		case OP_GE:
		case OP_LE:
		case OP_GT:
		case OP_LT:
		case OP_AND:
		case OP_OR:
		case OP_EQ_F:
		case OP_EQ_V:
		case OP_EQ_S:
		case OP_EQ_E:
		case OP_EQ_FNC:
		case OP_NE_F:
		case OP_NE_V:
		case OP_NE_S:
		case OP_NE_E:
		case OP_NE_FNC:
		case OP_ADDRESS:
		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC:
		case OP_LOAD_V:
			if (a >= prog->progs_numglobals || b >= prog->progs_numglobals || c >= prog->progs_numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d)", i);
			prog->statements[i].op = op;
			prog->statements[i].operand[0] = remapglobal(a);
			prog->statements[i].operand[1] = remapglobal(b);
			prog->statements[i].operand[2] = remapglobal(c);
			prog->statements[i].jumpabsolute = -1;
			break;
		// global none global
		case OP_NOT_F:
		case OP_NOT_V:
		case OP_NOT_S:
		case OP_NOT_FNC:
		case OP_NOT_ENT:
			if (a >= prog->progs_numglobals || c >= prog->progs_numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			prog->statements[i].op = op;
			prog->statements[i].operand[0] = remapglobal(a);
			prog->statements[i].operand[1] = -1;
			prog->statements[i].operand[2] = remapglobal(c);
			prog->statements[i].jumpabsolute = -1;
			break;
		// 2 globals
		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:
		case OP_STOREP_S:
		case OP_STOREP_FNC:
		case OP_STORE_F:
		case OP_STORE_ENT:
		case OP_STORE_FLD:
		case OP_STORE_S:
		case OP_STORE_FNC:
		case OP_STATE:
		case OP_STOREP_V:
		case OP_STORE_V:
			if (a >= prog->progs_numglobals || b >= prog->progs_numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			prog->statements[i].op = op;
			prog->statements[i].operand[0] = remapglobal(a);
			prog->statements[i].operand[1] = remapglobal(b);
			prog->statements[i].operand[2] = -1;
			prog->statements[i].jumpabsolute = -1;
			break;
		// 1 global
		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8:
		case OP_DONE:
		case OP_RETURN:
			if ( a >= prog->progs_numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			prog->statements[i].op = op;
			prog->statements[i].operand[0] = remapglobal(a);
			prog->statements[i].operand[1] = -1;
			prog->statements[i].operand[2] = -1;
			prog->statements[i].jumpabsolute = -1;
			break;
		}
	}
	if(prog->numstatements < 1)
	{
		PRVM_ERROR("PRVM_LoadProgs: empty program in %s", PRVM_NAME);
	}
	else switch(prog->statements[prog->numstatements - 1].op)
	{
		case OP_RETURN:
		case OP_GOTO:
		case OP_DONE:
			break;
		default:
			PRVM_ERROR("PRVM_LoadProgs: program may fall off the edge (does not end with RETURN, GOTO or DONE) in %s", PRVM_NAME);
			break;
	}

	// we're done with the file now
	Mem_Free(dprograms);
	dprograms = NULL;

	// check required functions
	for(i=0 ; i < numrequiredfunc ; i++)
		if(PRVM_ED_FindFunction(required_func[i]) == 0)
			PRVM_ERROR("%s: %s not found in %s",PRVM_NAME, required_func[i], filename);

	PRVM_LoadLNO(filename);

	PRVM_Init_Exec();

	if(*prvm_language.string)
	// in CSQC we really shouldn't be able to change how stuff works... sorry for now
	// later idea: include a list of authorized .po file checksums with the csprogs
	{
		qboolean deftrans = !!strcmp(PRVM_NAME, "client");
		const char *realfilename = (strcmp(PRVM_NAME, "client") ? filename : csqc_progname.string);
		if(deftrans) // once we have dotranslate_ strings, ALWAYS use the opt-in method!
		{
			for (i=0 ; i<prog->numglobaldefs ; i++)
			{
				const char *name;
				name = PRVM_GetString(prog->globaldefs[i].s_name);
				if((prog->globaldefs[i].type & ~DEF_SAVEGLOBAL) == ev_string)
				if(name && !strncmp(name, "dotranslate_", 12))
				{
					deftrans = false;
					break;
				}
			}
		}
		if(!strcmp(prvm_language.string, "dump"))
		{
			qfile_t *f = FS_OpenRealFile(va("%s.pot", realfilename), "w", false);
			Con_Printf("Dumping to %s.pot\n", realfilename);
			if(f)
			{
				for (i=0 ; i<prog->numglobaldefs ; i++)
				{
					const char *name;
					name = PRVM_GetString(prog->globaldefs[i].s_name);
					if(deftrans ? (!name || strncmp(name, "notranslate_", 12)) : (name && !strncmp(name, "dotranslate_", 12)))
					if((prog->globaldefs[i].type & ~DEF_SAVEGLOBAL) == ev_string)
					{
						prvm_eval_t *val = PRVM_GLOBALFIELDVALUE(prog->globaldefs[i].ofs);
						const char *value = PRVM_GetString(val->string);
						if(*value)
						{
							char buf[MAX_INPUTLINE];
							PRVM_PO_UnparseString(buf, value, sizeof(buf));
							FS_Printf(f, "msgid \"%s\"\nmsgstr \"\"\n\n", buf);
						}
					}
				}
				FS_Close(f);
			}
		}
		else
		{
			po_t *po = PRVM_PO_Load(va("%s.%s.po", realfilename, prvm_language.string), prog->progs_mempool);
			if(po)
			{
				for (i=0 ; i<prog->numglobaldefs ; i++)
				{
					const char *name;
					name = PRVM_GetString(prog->globaldefs[i].s_name);
					if(deftrans ? (!name || strncmp(name, "notranslate_", 12)) : (name && !strncmp(name, "dotranslate_", 12)))
					if((prog->globaldefs[i].type & ~DEF_SAVEGLOBAL) == ev_string)
					{
						prvm_eval_t *val = PRVM_GLOBALFIELDVALUE(prog->globaldefs[i].ofs);
						const char *value = PRVM_GetString(val->string);
						if(*value)
						{
							value = PRVM_PO_Lookup(po, value);
							if(value)
								val->string = PRVM_SetEngineString(value);
						}
					}
				}
			}
		}
	}

	for (i=0 ; i<prog->numglobaldefs ; i++)
	{
		const char *name;
		name = PRVM_GetString(prog->globaldefs[i].s_name);
		//Con_Printf("found var %s\n", name);
		if(name
			&& !strncmp(name, "autocvar_", 9)
			&& !(strlen(name) > 1 && name[strlen(name)-2] == '_' && (name[strlen(name)-1] == 'x' || name[strlen(name)-1] == 'y' || name[strlen(name)-1] == 'z'))
		)
		{
			prvm_eval_t *val = PRVM_GLOBALFIELDVALUE(prog->globaldefs[i].ofs);
			cvar_t *cvar = Cvar_FindVar(name + 9);
			//Con_Printf("PRVM_LoadProgs: autocvar global %s in %s, processing...\n", name, PRVM_NAME);
			if(!cvar)
			{
				const char *value;
				char buf[64];
				Con_DPrintf("PRVM_LoadProgs: no cvar for autocvar global %s in %s, creating...\n", name, PRVM_NAME);
				switch(prog->globaldefs[i].type & ~DEF_SAVEGLOBAL)
				{
					case ev_float:
						if((float)((int)(val->_float)) == val->_float)
							dpsnprintf(buf, sizeof(buf), "%i", (int)(val->_float));
						else
							dpsnprintf(buf, sizeof(buf), "%.9g", val->_float);
						value = buf;
						break;
					case ev_vector:
						dpsnprintf(buf, sizeof(buf), "%.9g %.9g %.9g", val->vector[0], val->vector[1], val->vector[2]); value = buf;
						break;
					case ev_string:
						value = PRVM_GetString(val->string);
						break;
					default:
						Con_Printf("PRVM_LoadProgs: invalid type of autocvar global %s in %s\n", name, PRVM_NAME);
						goto fail;
				}
				cvar = Cvar_Get(name + 9, value, 0, NULL);
				if((prog->globaldefs[i].type & ~DEF_SAVEGLOBAL) == ev_string)
				{
					val->string = PRVM_SetEngineString(cvar->string);
					cvar->globaldefindex_stringno[prog - prog_list] = val->string;
				}
				if(!cvar)
					PRVM_ERROR("PRVM_LoadProgs: could not create cvar for autocvar global %s in %s", name, PRVM_NAME);
				cvar->globaldefindex_progid[prog - prog_list] = prog->id;
				cvar->globaldefindex[prog - prog_list] = i;
			}
			else if((cvar->flags & CVAR_PRIVATE) == 0)
			{
				// MUST BE SYNCED WITH cvar.c Cvar_Set
				int j;
				const char *s;
				switch(prog->globaldefs[i].type & ~DEF_SAVEGLOBAL)
				{
					case ev_float:
						val->_float = cvar->value;
						break;
					case ev_vector:
						s = cvar->string;
						VectorClear(val->vector);
						for (j = 0;j < 3;j++)
						{
							while (*s && ISWHITESPACE(*s))
								s++;
							if (!*s)
								break;
							val->vector[j] = atof(s);
							while (!ISWHITESPACE(*s))
								s++;
							if (!*s)
								break;
						}
						break;
					case ev_string:
						val->string = PRVM_SetEngineString(cvar->string);
						cvar->globaldefindex_stringno[prog - prog_list] = val->string;
						break;
					default:
						Con_Printf("PRVM_LoadProgs: invalid type of autocvar global %s in %s\n", name, PRVM_NAME);
						goto fail;
				}
				cvar->globaldefindex_progid[prog - prog_list] = prog->id;
				cvar->globaldefindex[prog - prog_list] = i;
			}
			else
				Con_Printf("PRVM_LoadProgs: private cvar for autocvar global %s in %s\n", name, PRVM_NAME);
		}
fail:
		;
	}

	prog->loaded = TRUE;

	// set flags & ddef_ts in prog

	prog->flag = 0;

	PRVM_FindOffsets();

	PRVM_GCALL(init_cmd)();

	// init mempools
	PRVM_MEM_Alloc();
}


void PRVM_Fields_f (void)
{
	int i, j, ednum, used, usedamount;
	int *counts;
	char tempstring[MAX_INPUTLINE], tempstring2[260];
	const char *name;
	prvm_edict_t *ed;
	ddef_t *d;
	int *v;

	// TODO
	/*
	if (!sv.active)
	{
		Con_Print("no progs loaded\n");
		return;
	}
	*/

	if(Cmd_Argc() != 2)
	{
		Con_Print("prvm_fields <program name>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	counts = (int *)Mem_Alloc(tempmempool, prog->numfielddefs * sizeof(int));
	for (ednum = 0;ednum < prog->max_edicts;ednum++)
	{
		ed = PRVM_EDICT_NUM(ednum);
		if (ed->priv.required->free)
			continue;
		for (i = 1;i < prog->numfielddefs;i++)
		{
			d = &prog->fielddefs[i];
			name = PRVM_GetString(d->s_name);
			if (name[strlen(name)-2] == '_')
				continue;	// skip _x, _y, _z vars
			v = (int *)(ed->fields.vp + d->ofs);
			// if the value is still all 0, skip the field
			for (j = 0;j < prvm_type_size[d->type & ~DEF_SAVEGLOBAL];j++)
			{
				if (v[j])
				{
					counts[i]++;
					break;
				}
			}
		}
	}
	used = 0;
	usedamount = 0;
	tempstring[0] = 0;
	for (i = 0;i < prog->numfielddefs;i++)
	{
		d = &prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars
		switch(d->type & ~DEF_SAVEGLOBAL)
		{
		case ev_string:
			strlcat(tempstring, "string   ", sizeof(tempstring));
			break;
		case ev_entity:
			strlcat(tempstring, "entity   ", sizeof(tempstring));
			break;
		case ev_function:
			strlcat(tempstring, "function ", sizeof(tempstring));
			break;
		case ev_field:
			strlcat(tempstring, "field    ", sizeof(tempstring));
			break;
		case ev_void:
			strlcat(tempstring, "void     ", sizeof(tempstring));
			break;
		case ev_float:
			strlcat(tempstring, "float    ", sizeof(tempstring));
			break;
		case ev_vector:
			strlcat(tempstring, "vector   ", sizeof(tempstring));
			break;
		case ev_pointer:
			strlcat(tempstring, "pointer  ", sizeof(tempstring));
			break;
		default:
			dpsnprintf (tempstring2, sizeof(tempstring2), "bad type %i ", d->type & ~DEF_SAVEGLOBAL);
			strlcat(tempstring, tempstring2, sizeof(tempstring));
			break;
		}
		if (strlen(name) > sizeof(tempstring2)-4)
		{
			memcpy (tempstring2, name, sizeof(tempstring2)-4);
			tempstring2[sizeof(tempstring2)-4] = tempstring2[sizeof(tempstring2)-3] = tempstring2[sizeof(tempstring2)-2] = '.';
			tempstring2[sizeof(tempstring2)-1] = 0;
			name = tempstring2;
		}
		strlcat(tempstring, name, sizeof(tempstring));
		for (j = (int)strlen(name);j < 25;j++)
			strlcat(tempstring, " ", sizeof(tempstring));
		dpsnprintf(tempstring2, sizeof(tempstring2), "%5d", counts[i]);
		strlcat(tempstring, tempstring2, sizeof(tempstring));
		strlcat(tempstring, "\n", sizeof(tempstring));
		if (strlen(tempstring) >= sizeof(tempstring)/2)
		{
			Con_Print(tempstring);
			tempstring[0] = 0;
		}
		if (counts[i])
		{
			used++;
			usedamount += prvm_type_size[d->type & ~DEF_SAVEGLOBAL];
		}
	}
	Mem_Free(counts);
	Con_Printf("%s: %i entity fields (%i in use), totalling %i bytes per edict (%i in use), %i edicts allocated, %i bytes total spent on edict fields (%i needed)\n", PRVM_NAME, prog->entityfields, used, prog->entityfields * 4, usedamount * 4, prog->max_edicts, prog->entityfields * 4 * prog->max_edicts, usedamount * 4 * prog->max_edicts);

	PRVM_End;
}

void PRVM_Globals_f (void)
{
	int i;
	const char *wildcard;
	int numculled;
		numculled = 0;
	// TODO
	/*if (!sv.active)
	{
		Con_Print("no progs loaded\n");
		return;
	}*/
	if(Cmd_Argc () < 2 || Cmd_Argc() > 3)
	{
		Con_Print("prvm_globals <program name> <optional name wildcard>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString (Cmd_Argv (1)))
		return;

	if( Cmd_Argc() == 3)
		wildcard = Cmd_Argv(2);
	else
		wildcard = NULL;

	Con_Printf("%s :", PRVM_NAME);

	for (i = 0;i < prog->numglobaldefs;i++)
	{
		if(wildcard)
			if( !matchpattern( PRVM_GetString(prog->globaldefs[i].s_name), wildcard, 1) )
			{
				numculled++;
				continue;
			}
		Con_Printf("%s\n", PRVM_GetString(prog->globaldefs[i].s_name));
	}
	Con_Printf("%i global variables, %i culled, totalling %i bytes\n", prog->numglobals, numculled, prog->numglobals * 4);

	PRVM_End;
}

/*
===============
PRVM_Global
===============
*/
void PRVM_Global_f(void)
{
	ddef_t *global;
	if( Cmd_Argc() != 3 ) {
		Con_Printf( "prvm_global <program name> <global name>\n" );
		return;
	}

	PRVM_Begin;
	if( !PRVM_SetProgFromString( Cmd_Argv(1) ) )
		return;

	global = PRVM_ED_FindGlobal( Cmd_Argv(2) );
	if( !global )
		Con_Printf( "No global '%s' in %s!\n", Cmd_Argv(2), Cmd_Argv(1) );
	else
		Con_Printf( "%s: %s\n", Cmd_Argv(2), PRVM_ValueString( (etype_t)global->type, PRVM_GLOBALFIELDVALUE(global->ofs) ) );
	PRVM_End;
}

/*
===============
PRVM_GlobalSet
===============
*/
void PRVM_GlobalSet_f(void)
{
	ddef_t *global;
	if( Cmd_Argc() != 4 ) {
		Con_Printf( "prvm_globalset <program name> <global name> <value>\n" );
		return;
	}

	PRVM_Begin;
	if( !PRVM_SetProgFromString( Cmd_Argv(1) ) )
		return;

	global = PRVM_ED_FindGlobal( Cmd_Argv(2) );
	if( !global )
		Con_Printf( "No global '%s' in %s!\n", Cmd_Argv(2), Cmd_Argv(1) );
	else
		PRVM_ED_ParseEpair( NULL, global, Cmd_Argv(3), true );
	PRVM_End;
}

/*
===============
PRVM_Init
===============
*/
void PRVM_Init (void)
{
	Cmd_AddCommand ("prvm_edict", PRVM_ED_PrintEdict_f, "print all data about an entity number in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_edicts", PRVM_ED_PrintEdicts_f, "prints all data about all entities in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_edictcount", PRVM_ED_Count_f, "prints number of active entities in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_profile", PRVM_Profile_f, "prints execution statistics about the most used QuakeC functions in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_childprofile", PRVM_ChildProfile_f, "prints execution statistics about the most used QuakeC functions in the selected VM (server, client, menu), sorted by time taken in function with child calls");
	Cmd_AddCommand ("prvm_callprofile", PRVM_CallProfile_f, "prints execution statistics about the most time consuming QuakeC calls from the engine in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_fields", PRVM_Fields_f, "prints usage statistics on properties (how many entities have non-zero values) in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_globals", PRVM_Globals_f, "prints all global variables in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_global", PRVM_Global_f, "prints value of a specified global variable in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_globalset", PRVM_GlobalSet_f, "sets value of a specified global variable in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_edictset", PRVM_ED_EdictSet_f, "changes value of a specified property of a specified entity in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_edictget", PRVM_ED_EdictGet_f, "retrieves the value of a specified property of a specified entity in the selected VM (server, client menu) into a cvar or to the console");
	Cmd_AddCommand ("prvm_globalget", PRVM_ED_GlobalGet_f, "retrieves the value of a specified global variable in the selected VM (server, client menu) into a cvar or to the console");
	Cmd_AddCommand ("prvm_printfunction", PRVM_PrintFunction_f, "prints a disassembly (QuakeC instructions) of the specified function in the selected VM (server, client, menu)");
	Cmd_AddCommand ("cl_cmd", PRVM_GameCommand_Client_f, "calls the client QC function GameCommand with the supplied string as argument");
	Cmd_AddCommand ("menu_cmd", PRVM_GameCommand_Menu_f, "calls the menu QC function GameCommand with the supplied string as argument");
	Cmd_AddCommand ("sv_cmd", PRVM_GameCommand_Server_f, "calls the server QC function GameCommand with the supplied string as argument");

	Cvar_RegisterVariable (&prvm_language);
	Cvar_RegisterVariable (&prvm_traceqc);
	Cvar_RegisterVariable (&prvm_statementprofiling);
	Cvar_RegisterVariable (&prvm_timeprofiling);
	Cvar_RegisterVariable (&prvm_backtraceforwarnings);
	Cvar_RegisterVariable (&prvm_leaktest);
	Cvar_RegisterVariable (&prvm_leaktest_ignore_classnames);
	Cvar_RegisterVariable (&prvm_errordump);
	Cvar_RegisterVariable (&prvm_reuseedicts_startuptime);
	Cvar_RegisterVariable (&prvm_reuseedicts_neverinsameframe);

	// COMMANDLINEOPTION: PRVM: -norunaway disables the runaway loop check (it might be impossible to exit DarkPlaces if used!)
	prvm_runawaycheck = !COM_CheckParm("-norunaway");

	//VM_Cmd_Init();
}

/*
===============
PRVM_InitProg
===============
*/
void PRVM_InitProg(int prognr)
{
	static unsigned int progid = 0;

	if(prognr < 0 || prognr >= PRVM_MAXPROGS)
		Sys_Error("PRVM_InitProg: Invalid program number %i",prognr);

	prog = &prog_list[prognr];

	if(prog->loaded)
		PRVM_ResetProg();

	memset(prog, 0, sizeof(prvm_prog_t));
	prog->starttime = Sys_DoubleTime();
	prog->id = ++progid;

	prog->error_cmd = Host_Error;
	prog->leaktest_active = prvm_leaktest.integer != 0;
}

int PRVM_GetProgNr(void)
{
	return prog - prog_list;
}

void *_PRVM_Alloc(size_t buffersize, const char *filename, int fileline)
{
	return _Mem_Alloc(prog->progs_mempool, NULL, buffersize, 16, filename, fileline);
}

void _PRVM_Free(void *buffer, const char *filename, int fileline)
{
	_Mem_Free(buffer, filename, fileline);
}

void _PRVM_FreeAll(const char *filename, int fileline)
{
	prog->functions = NULL;
	prog->strings = NULL;
	prog->fielddefs = NULL;
	prog->globaldefs = NULL;
	prog->statements = NULL;
	// FIXME: what about knownstrings?
	_Mem_EmptyPool(prog->progs_mempool, filename, fileline);
}

// LordHavoc: turned PRVM_EDICT_NUM into a #define for speed reasons
unsigned int PRVM_EDICT_NUM_ERROR(unsigned int n, const char *filename, int fileline)
{
	PRVM_ERROR ("PRVM_EDICT_NUM: %s: bad number %i (called at %s:%i)", PRVM_NAME, n, filename, fileline);
	return 0;
}

sizebuf_t vm_tempstringsbuf;
#define PRVM_KNOWNSTRINGBASE 0x40000000

const char *PRVM_GetString(int num)
{
	if (num < 0)
	{
		// invalid
		VM_Warning("PRVM_GetString: Invalid string offset (%i < 0)\n", num);
		return "";
	}
	else if (num < prog->stringssize)
	{
		// constant string from progs.dat
		return prog->strings + num;
	}
	else if (num <= prog->stringssize + vm_tempstringsbuf.maxsize)
	{
		// tempstring returned by engine to QC (becomes invalid after returning to engine)
		num -= prog->stringssize;
		if (num < vm_tempstringsbuf.cursize)
			return (char *)vm_tempstringsbuf.data + num;
		else
		{
			VM_Warning("PRVM_GetString: Invalid temp-string offset (%i >= %i vm_tempstringsbuf.cursize)\n", num, vm_tempstringsbuf.cursize);
			return "";
		}
	}
	else if (num & PRVM_KNOWNSTRINGBASE)
	{
		// allocated string
		num = num - PRVM_KNOWNSTRINGBASE;
		if (num >= 0 && num < prog->numknownstrings)
		{
			if (!prog->knownstrings[num])
			{
				VM_Warning("PRVM_GetString: Invalid zone-string offset (%i has been freed)\n", num);
				return "";
			}
			return prog->knownstrings[num];
		}
		else
		{
			VM_Warning("PRVM_GetString: Invalid zone-string offset (%i >= %i)\n", num, prog->numknownstrings);
			return "";
		}
	}
	else
	{
		// invalid string offset
		VM_Warning("PRVM_GetString: Invalid constant-string offset (%i >= %i prog->stringssize)\n", num, prog->stringssize);
		return "";
	}
}

const char *PRVM_ChangeEngineString(int i, const char *s)
{
	const char *old;
	i = i - PRVM_KNOWNSTRINGBASE;
	if(i < 0 || i >= prog->numknownstrings)
		PRVM_ERROR("PRVM_ChangeEngineString: s is not an engine string");
	old = prog->knownstrings[i];
	prog->knownstrings[i] = s;
	return old;
}

int PRVM_SetEngineString(const char *s)
{
	int i;
	if (!s)
		return 0;
	if (s >= prog->strings && s <= prog->strings + prog->stringssize)
		PRVM_ERROR("PRVM_SetEngineString: s in prog->strings area");
	// if it's in the tempstrings area, use a reserved range
	// (otherwise we'd get millions of useless string offsets cluttering the database)
	if (s >= (char *)vm_tempstringsbuf.data && s < (char *)vm_tempstringsbuf.data + vm_tempstringsbuf.maxsize)
#if 1
		return prog->stringssize + (s - (char *)vm_tempstringsbuf.data);
#endif
	// see if it's a known string address
	for (i = 0;i < prog->numknownstrings;i++)
		if (prog->knownstrings[i] == s)
			return PRVM_KNOWNSTRINGBASE + i;
	// new unknown engine string
	if (developer_insane.integer)
		Con_DPrintf("new engine string %p = \"%s\"\n", s, s);
	for (i = prog->firstfreeknownstring;i < prog->numknownstrings;i++)
		if (!prog->knownstrings[i])
			break;
	if (i >= prog->numknownstrings)
	{
		if (i >= prog->maxknownstrings)
		{
			const char **oldstrings = prog->knownstrings;
			const unsigned char *oldstrings_freeable = prog->knownstrings_freeable;
			const char **oldstrings_origin = prog->knownstrings_origin;
			prog->maxknownstrings += 128;
			prog->knownstrings = (const char **)PRVM_Alloc(prog->maxknownstrings * sizeof(char *));
			prog->knownstrings_freeable = (unsigned char *)PRVM_Alloc(prog->maxknownstrings * sizeof(unsigned char));
			if(prog->leaktest_active)
				prog->knownstrings_origin = (const char **)PRVM_Alloc(prog->maxknownstrings * sizeof(char *));
			if (prog->numknownstrings)
			{
				memcpy((char **)prog->knownstrings, oldstrings, prog->numknownstrings * sizeof(char *));
				memcpy((char **)prog->knownstrings_freeable, oldstrings_freeable, prog->numknownstrings * sizeof(unsigned char));
				if(prog->leaktest_active)
					memcpy((char **)prog->knownstrings_origin, oldstrings_origin, prog->numknownstrings * sizeof(char *));
			}
		}
		prog->numknownstrings++;
	}
	prog->firstfreeknownstring = i + 1;
	prog->knownstrings[i] = s;
	prog->knownstrings_freeable[i] = false;
	if(prog->leaktest_active)
		prog->knownstrings_origin[i] = NULL;
	return PRVM_KNOWNSTRINGBASE + i;
}

// temp string handling

// all tempstrings go into this buffer consecutively, and it is reset
// whenever PRVM_ExecuteProgram returns to the engine
// (technically each PRVM_ExecuteProgram call saves the cursize value and
//  restores it on return, so multiple recursive calls can share the same
//  buffer)
// the buffer size is automatically grown as needed

int PRVM_SetTempString(const char *s)
{
	int size;
	char *t;
	if (!s)
		return 0;
	size = (int)strlen(s) + 1;
	if (developer_insane.integer)
		Con_DPrintf("PRVM_SetTempString: cursize %i, size %i\n", vm_tempstringsbuf.cursize, size);
	if (vm_tempstringsbuf.maxsize < vm_tempstringsbuf.cursize + size)
	{
		sizebuf_t old = vm_tempstringsbuf;
		if (vm_tempstringsbuf.cursize + size >= 1<<28)
			PRVM_ERROR("PRVM_SetTempString: ran out of tempstring memory!  (refusing to grow tempstring buffer over 256MB, cursize %i, size %i)\n", vm_tempstringsbuf.cursize, size);
		vm_tempstringsbuf.maxsize = max(vm_tempstringsbuf.maxsize, 65536);
		while (vm_tempstringsbuf.maxsize < vm_tempstringsbuf.cursize + size)
			vm_tempstringsbuf.maxsize *= 2;
		if (vm_tempstringsbuf.maxsize != old.maxsize || vm_tempstringsbuf.data == NULL)
		{
			Con_DPrintf("PRVM_SetTempString: enlarging tempstrings buffer (%iKB -> %iKB)\n", old.maxsize/1024, vm_tempstringsbuf.maxsize/1024);
			vm_tempstringsbuf.data = (unsigned char *) Mem_Alloc(sv_mempool, vm_tempstringsbuf.maxsize);
			if (old.cursize)
				memcpy(vm_tempstringsbuf.data, old.data, old.cursize);
			if (old.data)
				Mem_Free(old.data);
		}
	}
	t = (char *)vm_tempstringsbuf.data + vm_tempstringsbuf.cursize;
	memcpy(t, s, size);
	vm_tempstringsbuf.cursize += size;
	return PRVM_SetEngineString(t);
}

int PRVM_AllocString(size_t bufferlength, char **pointer)
{
	int i;
	if (!bufferlength)
		return 0;
	for (i = prog->firstfreeknownstring;i < prog->numknownstrings;i++)
		if (!prog->knownstrings[i])
			break;
	if (i >= prog->numknownstrings)
	{
		if (i >= prog->maxknownstrings)
		{
			const char **oldstrings = prog->knownstrings;
			const unsigned char *oldstrings_freeable = prog->knownstrings_freeable;
			const char **oldstrings_origin = prog->knownstrings_origin;
			prog->maxknownstrings += 128;
			prog->knownstrings = (const char **)PRVM_Alloc(prog->maxknownstrings * sizeof(char *));
			prog->knownstrings_freeable = (unsigned char *)PRVM_Alloc(prog->maxknownstrings * sizeof(unsigned char));
			if(prog->leaktest_active)
				prog->knownstrings_origin = (const char **)PRVM_Alloc(prog->maxknownstrings * sizeof(char *));
			if (prog->numknownstrings)
			{
				memcpy((char **)prog->knownstrings, oldstrings, prog->numknownstrings * sizeof(char *));
				memcpy((char **)prog->knownstrings_freeable, oldstrings_freeable, prog->numknownstrings * sizeof(unsigned char));
				if(prog->leaktest_active)
					memcpy((char **)prog->knownstrings_origin, oldstrings_origin, prog->numknownstrings * sizeof(char *));
			}
			if (oldstrings)
				Mem_Free((char **)oldstrings);
			if (oldstrings_freeable)
				Mem_Free((unsigned char *)oldstrings_freeable);
			if (oldstrings_origin)
				Mem_Free((char **)oldstrings_origin);
		}
		prog->numknownstrings++;
	}
	prog->firstfreeknownstring = i + 1;
	prog->knownstrings[i] = (char *)PRVM_Alloc(bufferlength);
	prog->knownstrings_freeable[i] = true;
	if(prog->leaktest_active)
		prog->knownstrings_origin[i] = PRVM_AllocationOrigin();
	if (pointer)
		*pointer = (char *)(prog->knownstrings[i]);
	return PRVM_KNOWNSTRINGBASE + i;
}

void PRVM_FreeString(int num)
{
	if (num == 0)
		PRVM_ERROR("PRVM_FreeString: attempt to free a NULL string");
	else if (num >= 0 && num < prog->stringssize)
		PRVM_ERROR("PRVM_FreeString: attempt to free a constant string");
	else if (num >= PRVM_KNOWNSTRINGBASE && num < PRVM_KNOWNSTRINGBASE + prog->numknownstrings)
	{
		num = num - PRVM_KNOWNSTRINGBASE;
		if (!prog->knownstrings[num])
			PRVM_ERROR("PRVM_FreeString: attempt to free a non-existent or already freed string");
		if (!prog->knownstrings_freeable[num])
			PRVM_ERROR("PRVM_FreeString: attempt to free a string owned by the engine");
		PRVM_Free((char *)prog->knownstrings[num]);
		if(prog->leaktest_active)
			if(prog->knownstrings_origin[num])
				PRVM_Free((char *)prog->knownstrings_origin[num]);
		prog->knownstrings[num] = NULL;
		prog->knownstrings_freeable[num] = false;
		prog->firstfreeknownstring = min(prog->firstfreeknownstring, num);
	}
	else
		PRVM_ERROR("PRVM_FreeString: invalid string offset %i", num);
}

static qboolean PRVM_IsStringReferenced(string_t string)
{
	int i, j;

	for (i = 0;i < prog->numglobaldefs;i++)
	{
		ddef_t *d = &prog->globaldefs[i];
		if((etype_t)((int) d->type & ~DEF_SAVEGLOBAL) != ev_string)
			continue;
		if(string == PRVM_GLOBALFIELDSTRING(d->ofs))
			return true;
	}

	for(j = 0; j < prog->num_edicts; ++j)
	{
		prvm_edict_t *ed = PRVM_EDICT_NUM(j);
		if (ed->priv.required->free)
			continue;
		for (i=0; i<prog->numfielddefs; ++i)
		{
			ddef_t *d = &prog->fielddefs[i];
			if((etype_t)((int) d->type & ~DEF_SAVEGLOBAL) != ev_string)
				continue;
			if(string == PRVM_EDICTFIELDSTRING(ed, d->ofs))
				return true;
		}
	}

	return false;
}

static qboolean PRVM_IsEdictRelevant(prvm_edict_t *edict)
{
	if(PRVM_NUM_FOR_EDICT(edict) <= prog->reserved_edicts)
		return true; // world or clients
	switch(prog - prog_list)
	{
		case PRVM_SERVERPROG:
			{
				if(PRVM_serveredictfloat(edict, solid)) // can block other stuff, or is a trigger?
					return true;
				if(PRVM_serveredictfloat(edict, modelindex)) // visible ent?
					return true;
				if(PRVM_serveredictfloat(edict, effects)) // particle effect?
					return true;
				if(PRVM_serveredictfunction(edict, think)) // has a think function?
					if(PRVM_serveredictfloat(edict, nextthink) > 0) // that actually will eventually run?
						return true;
				if(PRVM_serveredictfloat(edict, takedamage))
					return true;
				if(*prvm_leaktest_ignore_classnames.string)
				{
					if(strstr(va(" %s ", prvm_leaktest_ignore_classnames.string), va(" %s ", PRVM_GetString(PRVM_serveredictstring(edict, classname)))))
						return true;
				}
			}
			break;
		case PRVM_CLIENTPROG:
			{
				// TODO someone add more stuff here
				if(PRVM_clientedictfloat(edict, entnum)) // csqc networked
					return true;
				if(PRVM_clientedictfloat(edict, modelindex)) // visible ent?
					return true;
				if(PRVM_clientedictfloat(edict, effects)) // particle effect?
					return true;
				if(PRVM_clientedictfunction(edict, think)) // has a think function?
					if(PRVM_clientedictfloat(edict, nextthink) > 0) // that actually will eventually run?
						return true;
				if(*prvm_leaktest_ignore_classnames.string)
				{
					if(strstr(va(" %s ", prvm_leaktest_ignore_classnames.string), va(" %s ", PRVM_GetString(PRVM_clientedictstring(edict, classname)))))
						return true;
				}
			}
			break;
		case PRVM_MENUPROG:
			// menu prog does not have classnames
			break;
	}
	return false;
}

static qboolean PRVM_IsEdictReferenced(prvm_edict_t *edict, int mark)
{
	int i, j;
	int edictnum = PRVM_NUM_FOR_EDICT(edict);
	const char *targetname = NULL;

	switch(prog - prog_list)
	{
		case PRVM_SERVERPROG:
			targetname = PRVM_GetString(PRVM_serveredictstring(edict, targetname));
			break;
	}

	if(targetname)
		if(!*targetname) // ""
			targetname = NULL;

	for (i = 0;i < prog->numglobaldefs;i++)
	{
		ddef_t *d = &prog->globaldefs[i];
		if((etype_t)((int) d->type & ~DEF_SAVEGLOBAL) != ev_entity)
			continue;
		if(edictnum == PRVM_GLOBALFIELDEDICT(d->ofs))
			return true;
	}

	for(j = 0; j < prog->num_edicts; ++j)
	{
		prvm_edict_t *ed = PRVM_EDICT_NUM(j);
		if (ed->priv.required->mark < mark)
			continue;
		if(ed == edict)
			continue;
		if(targetname)
		{
			const char *target = PRVM_GetString(PRVM_serveredictstring(ed, target));
			if(target)
				if(!strcmp(target, targetname))
					return true;
		}
		for (i=0; i<prog->numfielddefs; ++i)
		{
			ddef_t *d = &prog->fielddefs[i];
			if((etype_t)((int) d->type & ~DEF_SAVEGLOBAL) != ev_entity)
				continue;
			if(edictnum == PRVM_EDICTFIELDEDICT(ed, d->ofs))
				return true;
		}
	}

	return false;
}

static void PRVM_MarkReferencedEdicts(void)
{
	int j;
	qboolean found_new;
	int stage;

	for(j = 0; j < prog->num_edicts; ++j)
	{
		prvm_edict_t *ed = PRVM_EDICT_NUM(j);
		if(ed->priv.required->free)
			continue;
		ed->priv.required->mark = PRVM_IsEdictRelevant(ed) ? 1 : 0;
	}

	stage = 1;
	do
	{
		found_new = false;
		for(j = 0; j < prog->num_edicts; ++j)
		{
			prvm_edict_t *ed = PRVM_EDICT_NUM(j);
			if(ed->priv.required->free)
				continue;
			if(ed->priv.required->mark)
				continue;
			if(PRVM_IsEdictReferenced(ed, stage))
			{
				ed->priv.required->mark = stage + 1;
				found_new = true;
			}
		}
		++stage;
	}
	while(found_new);
	Con_DPrintf("leak check used %d stages to find all references\n", stage);
}

void PRVM_LeakTest(void)
{
	int i, j;
	qboolean leaked = false;

	if(!prog->leaktest_active)
		return;

	// 1. Strings
	for (i = 0; i < prog->numknownstrings; ++i)
	{
		if(prog->knownstrings[i])
		if(prog->knownstrings_freeable[i])
		if(prog->knownstrings_origin[i])
		if(!PRVM_IsStringReferenced(PRVM_KNOWNSTRINGBASE + i))
		{
			Con_Printf("Unreferenced string found!\n  Value: %s\n  Origin: %s\n", prog->knownstrings[i], prog->knownstrings_origin[i]);
			leaked = true;
		}
	}

	// 2. Edicts
	PRVM_MarkReferencedEdicts();
	for(j = 0; j < prog->num_edicts; ++j)
	{
		prvm_edict_t *ed = PRVM_EDICT_NUM(j);
		if(ed->priv.required->free)
			continue;
		if(!ed->priv.required->mark)
		if(ed->priv.required->allocation_origin)
		{
			Con_Printf("Unreferenced edict found!\n  Allocated at: %s\n", ed->priv.required->allocation_origin);
			PRVM_ED_Print(ed, NULL);
			Con_Print("\n");
			leaked = true;
		}
	}

	for (i = 0; i < (int)Mem_ExpandableArray_IndexRange(&prog->stringbuffersarray); ++i)
	{
		prvm_stringbuffer_t *stringbuffer = (prvm_stringbuffer_t*) Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, i);
		if(stringbuffer)
		if(stringbuffer->origin)
		{
			Con_Printf("Open string buffer handle found!\n  Allocated at: %s\n", stringbuffer->origin);
			leaked = true;
		}
	}

	for(i = 0; i < PRVM_MAX_OPENFILES; ++i)
	{
		if(prog->openfiles[i])
		if(prog->openfiles_origin[i])
		{
			Con_Printf("Open file handle found!\n  Allocated at: %s\n", prog->openfiles_origin[i]);
			leaked = true;
		}
	}

	for(i = 0; i < PRVM_MAX_OPENSEARCHES; ++i)
	{
		if(prog->opensearches[i])
		if(prog->opensearches_origin[i])
		{
			Con_Printf("Open search handle found!\n  Allocated at: %s\n", prog->opensearches_origin[i]);
			leaked = true;
		}
	}

	if(!leaked)
		Con_Printf("Congratulations. No leaks found.\n");
}
