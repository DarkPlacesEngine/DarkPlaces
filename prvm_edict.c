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

prvm_prog_t *prog;

static prvm_prog_t prog_list[PRVM_MAXPROGS];

int		prvm_type_size[8] = {1,sizeof(string_t)/4,1,3,1,1,sizeof(func_t)/4,sizeof(void *)/4};

ddef_t *PRVM_ED_FieldAtOfs(int ofs);
qboolean PRVM_ED_ParseEpair(prvm_edict_t *ent, ddef_t *key, const char *s);

// LordHavoc: optional runtime bounds checking (speed drain, but worth it for security, on by default - breaks most QCCX features (used by CRMod and others))
cvar_t prvm_boundscheck = {0, "prvm_boundscheck", "1", "enables detection of out of bounds memory access in the QuakeC code being run (in other words, prevents really exceedingly bad QuakeC code from doing nasty things to your computer)"};
// LordHavoc: prints every opcode as it executes - warning: this is significant spew
cvar_t prvm_traceqc = {0, "prvm_traceqc", "0", "prints every QuakeC statement as it is executed (only for really thorough debugging!)"};
// LordHavoc: counts usage of each QuakeC statement
cvar_t prvm_statementprofiling = {0, "prvm_statementprofiling", "0", "counts how many times each QuakeC statement has been executed, these counts are displayed in prvm_printfunction output (if enabled)"};

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
	prog->edictsfields = Mem_Alloc(prog->progs_mempool, prog->max_edicts * prog->edict_size);

	// set edict pointers
	for(i = 0; i < prog->max_edicts; i++)
	{
		prog->edicts[i].priv.required = (prvm_edict_private_t *)((unsigned char  *)prog->edictprivate + i * prog->edictprivate_size);
		prog->edicts[i].fields.vp = (void*)((unsigned char *)prog->edictsfields + i * prog->edict_size);
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
	int		oldmaxedicts = prog->max_edicts;
	void	*oldedictsfields = prog->edictsfields;
	void	*oldedictprivate = prog->edictprivate;

	if(prog->max_edicts >= prog->limit_edicts)
		return;

	PRVM_GCALL(begin_increase_edicts)();

	// increase edicts
	prog->max_edicts = min(prog->max_edicts + 256, prog->limit_edicts);

	prog->edictsfields = Mem_Alloc(prog->progs_mempool, prog->max_edicts * prog->edict_size);
	prog->edictprivate = Mem_Alloc(prog->progs_mempool, prog->max_edicts * prog->edictprivate_size);

	memcpy(prog->edictsfields, oldedictsfields, oldmaxedicts * prog->edict_size);
	memcpy(prog->edictprivate, oldedictprivate, oldmaxedicts * prog->edictprivate_size);

	//set e and v pointers
	for(i = 0; i < prog->max_edicts; i++)
	{
		prog->edicts[i].priv.required  = (prvm_edict_private_t *)((unsigned char  *)prog->edictprivate + i * prog->edictprivate_size);
		prog->edicts[i].fields.vp = (void*)((unsigned char *)prog->edictsfields + i * prog->edict_size);
	}

	PRVM_GCALL(end_increase_edicts)();

	Mem_Free(oldedictsfields);
	Mem_Free(oldedictprivate);
}

//============================================================================
// normal prvm

int PRVM_ED_FindFieldOffset(const char *field)
{
	ddef_t *d;
	d = PRVM_ED_FindField(field);
	if (!d)
		return -1;
	return d->ofs*4;
}

int PRVM_ED_FindGlobalOffset(const char *global)
{
	ddef_t *d;
	d = PRVM_ED_FindGlobal(global);
	if (!d)
		return -1;
	return d->ofs*4;
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
	memset (e->fields.vp, 0, prog->progs->entityfields * 4);
	e->priv.required->free = false;

	// AK: Let the init_edict function determine if something needs to be initialized
	PRVM_GCALL(init_edict)(e);
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
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->priv.required->free && ( e->priv.required->freetime < 2 || prog->globaloffsets.time < 0 || (PRVM_GETGLOBALFIELDVALUE(prog->globaloffsets.time)->_float - e->priv.required->freetime) > 0.5 ) )
		{
			PRVM_ED_ClearEdict (e);
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
	ed->priv.required->freetime = prog->globaloffsets.time >= 0 ? PRVM_GETGLOBALFIELDVALUE(prog->globaloffsets.time)->_float : 0;
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

	for (i=0 ; i<prog->progs->numglobaldefs ; i++)
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

	for (i=0 ; i<prog->progs->numfielddefs ; i++)
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

	for (i=0 ; i<prog->progs->numfielddefs ; i++)
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

	for (i=0 ; i<prog->progs->numglobaldefs ; i++)
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

	for (i=0 ; i<prog->progs->numfunctions ; i++)
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
		if (n < 0 || n >= prog->limit_edicts)
			sprintf (line, "entity %i (invalid!)", n);
		else
			sprintf (line, "entity %i", n);
		break;
	case ev_function:
		f = prog->functions + val->function;
		sprintf (line, "%s()", PRVM_GetString(f->s_name));
		break;
	case ev_field:
		def = PRVM_ED_FieldAtOfs ( val->_int );
		sprintf (line, ".%s", PRVM_GetString(def->s_name));
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		// LordHavoc: changed from %5.1f to %10.4f
		sprintf (line, "%10.4f", val->_float);
		break;
	case ev_vector:
		// LordHavoc: changed from %5.1f to %10.4f
		sprintf (line, "'%10.4f %10.4f %10.4f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		break;
	default:
		sprintf (line, "bad type %i", (int) type);
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
		dpsnprintf (line, sizeof (line), "%f", val->_float);
		break;
	case ev_vector:
		dpsnprintf (line, sizeof (line), "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
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
		sprintf (line,"GLOBAL%i", ofs);
	else
	{
		s = PRVM_ValueString ((etype_t)def->type, (prvm_eval_t *)val);
		sprintf (line,"%s (=%s)", PRVM_GetString(def->s_name), s);
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
		sprintf (line,"GLOBAL%i", ofs);
	else
		sprintf (line,"%s", PRVM_GetString(def->s_name));

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
void PRVM_ED_Print(prvm_edict_t *ed)
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
	sprintf(tempstring, "\n%s EDICT %i:\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ed));
	for (i=1 ; i<prog->progs->numfielddefs ; i++)
	{
		d = &prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)ed->fields.vp + d->ofs*4);

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

	for (i=1 ; i<prog->progs->numfielddefs ; i++)
	{
		d = &prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)ed->fields.vp + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<prvm_type_size[type] ; j++)
			if (v[j])
				break;
		if (j == prvm_type_size[type])
			continue;

		FS_Printf(f,"\"%s\" ",name);
		FS_Printf(f,"\"%s\"\n", PRVM_UglyValueString((etype_t)d->type, (prvm_eval_t *)v));
	}

	FS_Print(f, "}\n");
}

void PRVM_ED_PrintNum (int ent)
{
	PRVM_ED_Print(PRVM_EDICT_NUM(ent));
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

	if(Cmd_Argc() != 2)
	{
		Con_Print("prvm_edicts <program name>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	Con_Printf("%s: %i entities\n", PRVM_NAME, prog->num_edicts);
	for (i=0 ; i<prog->num_edicts ; i++)
		PRVM_ED_PrintNum (i);

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

	if(Cmd_Argc() != 3)
	{
		Con_Print("prvm_edict <program name> <edict number>\n");
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
	PRVM_ED_PrintNum (i);

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
	for (i=0 ; i<prog->progs->numglobaldefs ; i++)
	{
		def = &prog->globaldefs[i];
		type = def->type;
		if ( !(def->type & DEF_SAVEGLOBAL) )
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PRVM_GetString(def->s_name);
		FS_Printf(f,"\"%s\" ", name);
		FS_Printf(f,"\"%s\"\n", PRVM_UglyValueString((etype_t)type, (prvm_eval_t *)&prog->globals.generic[def->ofs]));
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
		if (!COM_ParseTokenConsole(&data))
			PRVM_ERROR ("PRVM_ED_ParseGlobals: EOF without closing brace");
		if (com_token[0] == '}')
			break;

		strlcpy (keyname, com_token, sizeof(keyname));

		// parse value
		if (!COM_ParseTokenConsole(&data))
			PRVM_ERROR ("PRVM_ED_ParseGlobals: EOF without closing brace");

		if (com_token[0] == '}')
			PRVM_ERROR ("PRVM_ED_ParseGlobals: closing brace without data");

		key = PRVM_ED_FindGlobal (keyname);
		if (!key)
		{
			Con_DPrintf("'%s' is not a global on %s\n", keyname, PRVM_NAME);
			continue;
		}

		if (!PRVM_ED_ParseEpair(NULL, key, com_token))
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
qboolean PRVM_ED_ParseEpair(prvm_edict_t *ent, ddef_t *key, const char *s)
{
	int i, l;
	char *new_p;
	ddef_t *def;
	prvm_eval_t *val;
	mfunction_t *func;

	if (ent)
		val = (prvm_eval_t *)((int *)ent->fields.vp + key->ofs);
	else
		val = (prvm_eval_t *)((int *)prog->globals.generic + key->ofs);
	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		l = (int)strlen(s) + 1;
		val->string = PRVM_AllocString(l, &new_p);
		for (i = 0;i < l;i++)
		{
			if (s[i] == '\\' && i < l-1)
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
		while (*s && *s <= ' ')
			s++;
		val->_float = atof(s);
		break;

	case ev_vector:
		for (i = 0;i < 3;i++)
		{
			while (*s && *s <= ' ')
				s++;
			if (!*s)
				break;
			val->vector[i] = atof(s);
			while (*s > ' ')
				s++;
			if (!*s)
				break;
		}
		break;

	case ev_entity:
		while (*s && *s <= ' ')
			s++;
		i = atoi(s);
		if (i >= prog->limit_edicts)
			Con_Printf("PRVM_ED_ParseEpair: ev_entity reference too large (edict %u >= MAX_EDICTS %u) on %s\n", (unsigned int)i, (unsigned int)MAX_EDICTS, PRVM_NAME);
		while (i >= prog->max_edicts)
			PRVM_MEM_IncreaseEdicts();
			//SV_IncreaseEdicts();
		// if SV_IncreaseEdicts was called the base pointer needs to be updated
		if (ent)
			val = (prvm_eval_t *)((int *)ent->fields.vp + key->ofs);
		val->edict = PRVM_EDICT_TO_PROG(PRVM_EDICT_NUM((int)i));
		break;

	case ev_field:
		def = PRVM_ED_FindField(s);
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
		PRVM_ED_ParseEpair(ed, key, Cmd_Argv(4));

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
extern cvar_t developer_entityparsing;
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
		if (!COM_ParseTokenConsole(&data))
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
		if (!COM_ParseTokenConsole(&data))
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
			sprintf (com_token, "0 %s 0", temp);
		}

		if (!PRVM_ED_ParseEpair(ent, key, com_token))
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
	mfunction_t *func;

	parsed = 0;
	inhibited = 0;
	spawned = 0;
	died = 0;


// parse ents
	while (1)
	{
// parse the opening brace
		if (!COM_ParseTokenConsole(&data))
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
			memset (ent->fields.vp, 0, prog->progs->entityfields * 4);

		data = PRVM_ED_ParseEdict (data, ent);
		parsed++;

		// remove the entity ?
		if(prog->load_edict && !prog->load_edict(ent))
		{
			PRVM_ED_Free(ent);
			inhibited++;
			continue;
		}

//
// immediately call spawn function, but only if there is a self global and a classname
//
		if(prog->globaloffsets.self >= 0 && prog->fieldoffsets.classname >= 0)
		{
			string_t handle =  *(string_t*)&((unsigned char*)ent->fields.vp)[prog->fieldoffsets.classname];
			if (!handle)
			{
				Con_Print("No classname for:\n");
				PRVM_ED_Print(ent);
				PRVM_ED_Free (ent);
				continue;
			}

			// look for the spawn function
			func = PRVM_ED_FindFunction (PRVM_GetString(handle));

			if (!func)
			{
				if (developer.integer) // don't confuse non-developers with errors
				{
					Con_Print("No spawn function for:\n");
					PRVM_ED_Print(ent);
				}
				PRVM_ED_Free (ent);
				continue;
			}

			// self = ent
			PRVM_GETGLOBALFIELDVALUE(prog->globaloffsets.self)->edict = PRVM_EDICT_TO_PROG(ent);
			PRVM_ExecuteProgram (func - prog->functions, "");
		}

		spawned++;
		if (ent->priv.required->free)
			died++;
	}

	Con_DPrintf("%s: %i new entities parsed, %i new inhibited, %i (%i new) spawned (whereas %i removed self, %i stayed)\n", PRVM_NAME, parsed, inhibited, prog->num_edicts, spawned, died, spawned - died);
}

void PRVM_FindOffsets(void)
{
	// field and global searches use -1 for NULL
	memset(&prog->fieldoffsets, -1, sizeof(prog->fieldoffsets));
	memset(&prog->globaloffsets, -1, sizeof(prog->globaloffsets));
	// functions use 0 for NULL
	memset(&prog->funcoffsets, 0, sizeof(prog->funcoffsets));

	// common
	prog->fieldoffsets.classname = PRVM_ED_FindFieldOffset("classname");
	prog->fieldoffsets.chain = PRVM_ED_FindFieldOffset("chain");
	prog->fieldoffsets.think = PRVM_ED_FindFieldOffset("think");
	prog->fieldoffsets.nextthink = PRVM_ED_FindFieldOffset("nextthink");
	prog->fieldoffsets.frame = PRVM_ED_FindFieldOffset("frame");
	prog->fieldoffsets.angles = PRVM_ED_FindFieldOffset("angles");
	prog->globaloffsets.self = PRVM_ED_FindGlobalOffset("self");
	prog->globaloffsets.time = PRVM_ED_FindGlobalOffset("time");

	// ssqc
	prog->fieldoffsets.gravity = PRVM_ED_FindFieldOffset("gravity");
	prog->fieldoffsets.button3 = PRVM_ED_FindFieldOffset("button3");
	prog->fieldoffsets.button4 = PRVM_ED_FindFieldOffset("button4");
	prog->fieldoffsets.button5 = PRVM_ED_FindFieldOffset("button5");
	prog->fieldoffsets.button6 = PRVM_ED_FindFieldOffset("button6");
	prog->fieldoffsets.button7 = PRVM_ED_FindFieldOffset("button7");
	prog->fieldoffsets.button8 = PRVM_ED_FindFieldOffset("button8");
	prog->fieldoffsets.button9 = PRVM_ED_FindFieldOffset("button9");
	prog->fieldoffsets.button10 = PRVM_ED_FindFieldOffset("button10");
	prog->fieldoffsets.button11 = PRVM_ED_FindFieldOffset("button11");
	prog->fieldoffsets.button12 = PRVM_ED_FindFieldOffset("button12");
	prog->fieldoffsets.button13 = PRVM_ED_FindFieldOffset("button13");
	prog->fieldoffsets.button14 = PRVM_ED_FindFieldOffset("button14");
	prog->fieldoffsets.button15 = PRVM_ED_FindFieldOffset("button15");
	prog->fieldoffsets.button16 = PRVM_ED_FindFieldOffset("button16");
	prog->fieldoffsets.buttonuse = PRVM_ED_FindFieldOffset("buttonuse");
	prog->fieldoffsets.buttonchat = PRVM_ED_FindFieldOffset("buttonchat");
	prog->fieldoffsets.glow_size = PRVM_ED_FindFieldOffset("glow_size");
	prog->fieldoffsets.glow_trail = PRVM_ED_FindFieldOffset("glow_trail");
	prog->fieldoffsets.glow_color = PRVM_ED_FindFieldOffset("glow_color");
	prog->fieldoffsets.items2 = PRVM_ED_FindFieldOffset("items2");
	prog->fieldoffsets.scale = PRVM_ED_FindFieldOffset("scale");
	prog->fieldoffsets.alpha = PRVM_ED_FindFieldOffset("alpha");
	prog->fieldoffsets.renderamt = PRVM_ED_FindFieldOffset("renderamt"); // HalfLife support
	prog->fieldoffsets.rendermode = PRVM_ED_FindFieldOffset("rendermode"); // HalfLife support
	prog->fieldoffsets.fullbright = PRVM_ED_FindFieldOffset("fullbright");
	prog->fieldoffsets.ammo_shells1 = PRVM_ED_FindFieldOffset("ammo_shells1");
	prog->fieldoffsets.ammo_nails1 = PRVM_ED_FindFieldOffset("ammo_nails1");
	prog->fieldoffsets.ammo_lava_nails = PRVM_ED_FindFieldOffset("ammo_lava_nails");
	prog->fieldoffsets.ammo_rockets1 = PRVM_ED_FindFieldOffset("ammo_rockets1");
	prog->fieldoffsets.ammo_multi_rockets = PRVM_ED_FindFieldOffset("ammo_multi_rockets");
	prog->fieldoffsets.ammo_cells1 = PRVM_ED_FindFieldOffset("ammo_cells1");
	prog->fieldoffsets.ammo_plasma = PRVM_ED_FindFieldOffset("ammo_plasma");
	prog->fieldoffsets.ideal_yaw = PRVM_ED_FindFieldOffset("ideal_yaw");
	prog->fieldoffsets.yaw_speed = PRVM_ED_FindFieldOffset("yaw_speed");
	prog->fieldoffsets.idealpitch = PRVM_ED_FindFieldOffset("idealpitch");
	prog->fieldoffsets.pitch_speed = PRVM_ED_FindFieldOffset("pitch_speed");
	prog->fieldoffsets.viewmodelforclient = PRVM_ED_FindFieldOffset("viewmodelforclient");
	prog->fieldoffsets.nodrawtoclient = PRVM_ED_FindFieldOffset("nodrawtoclient");
	prog->fieldoffsets.exteriormodeltoclient = PRVM_ED_FindFieldOffset("exteriormodeltoclient");
	prog->fieldoffsets.drawonlytoclient = PRVM_ED_FindFieldOffset("drawonlytoclient");
	prog->fieldoffsets.ping = PRVM_ED_FindFieldOffset("ping");
	prog->fieldoffsets.movement = PRVM_ED_FindFieldOffset("movement");
	prog->fieldoffsets.pmodel = PRVM_ED_FindFieldOffset("pmodel");
	prog->fieldoffsets.punchvector = PRVM_ED_FindFieldOffset("punchvector");
	prog->fieldoffsets.viewzoom = PRVM_ED_FindFieldOffset("viewzoom");
	prog->fieldoffsets.clientcolors = PRVM_ED_FindFieldOffset("clientcolors");
	prog->fieldoffsets.tag_entity = PRVM_ED_FindFieldOffset("tag_entity");
	prog->fieldoffsets.tag_index = PRVM_ED_FindFieldOffset("tag_index");
	prog->fieldoffsets.light_lev = PRVM_ED_FindFieldOffset("light_lev");
	prog->fieldoffsets.color = PRVM_ED_FindFieldOffset("color");
	prog->fieldoffsets.style = PRVM_ED_FindFieldOffset("style");
	prog->fieldoffsets.pflags = PRVM_ED_FindFieldOffset("pflags");
	prog->fieldoffsets.cursor_active = PRVM_ED_FindFieldOffset("cursor_active");
	prog->fieldoffsets.cursor_screen = PRVM_ED_FindFieldOffset("cursor_screen");
	prog->fieldoffsets.cursor_trace_start = PRVM_ED_FindFieldOffset("cursor_trace_start");
	prog->fieldoffsets.cursor_trace_endpos = PRVM_ED_FindFieldOffset("cursor_trace_endpos");
	prog->fieldoffsets.cursor_trace_ent = PRVM_ED_FindFieldOffset("cursor_trace_ent");
	prog->fieldoffsets.colormod = PRVM_ED_FindFieldOffset("colormod");
	prog->fieldoffsets.playermodel = PRVM_ED_FindFieldOffset("playermodel");
	prog->fieldoffsets.playerskin = PRVM_ED_FindFieldOffset("playerskin");
	prog->fieldoffsets.SendEntity = PRVM_ED_FindFieldOffset("SendEntity");
	prog->fieldoffsets.Version = PRVM_ED_FindFieldOffset("Version");
	prog->fieldoffsets.customizeentityforclient = PRVM_ED_FindFieldOffset("customizeentityforclient");
	prog->fieldoffsets.dphitcontentsmask = PRVM_ED_FindFieldOffset("dphitcontentsmask");
	prog->fieldoffsets.contentstransition = PRVM_ED_FindFieldOffset("contentstransition");
	prog->globaloffsets.trace_dpstartcontents = PRVM_ED_FindGlobalOffset("trace_dpstartcontents");
	prog->globaloffsets.trace_dphitcontents = PRVM_ED_FindGlobalOffset("trace_dphitcontents");
	prog->globaloffsets.trace_dphitq3surfaceflags = PRVM_ED_FindGlobalOffset("trace_dphitq3surfaceflags");
	prog->globaloffsets.trace_dphittexturename = PRVM_ED_FindGlobalOffset("trace_dphittexturename");
	prog->globaloffsets.SV_InitCmd = PRVM_ED_FindGlobalOffset("SV_InitCmd");
	prog->funcoffsets.SV_ParseClientCommand = PRVM_ED_FindFunctionOffset("SV_ParseClientCommand");
	prog->funcoffsets.SV_PlayerPhysics = PRVM_ED_FindFunctionOffset("SV_PlayerPhysics");
	prog->funcoffsets.SV_ChangeTeam = PRVM_ED_FindFunctionOffset("SV_ChangeTeam");
	prog->funcoffsets.EndFrame = PRVM_ED_FindFunctionOffset("EndFrame");
	prog->funcoffsets.RestoreGame = PRVM_ED_FindFunctionOffset("RestoreGame");

	// csqc
	prog->fieldoffsets.alpha = PRVM_ED_FindFieldOffset("alpha");
	prog->fieldoffsets.scale = PRVM_ED_FindFieldOffset("scale");
	//prog->fieldoffsets.fatness = PRVM_ED_FindFieldOffset("fatness");
	prog->fieldoffsets.frame2 = PRVM_ED_FindFieldOffset("frame2");
	prog->fieldoffsets.frame1time = PRVM_ED_FindFieldOffset("frame1time");
	prog->fieldoffsets.frame2time = PRVM_ED_FindFieldOffset("frame2time");
	prog->fieldoffsets.lerpfrac = PRVM_ED_FindFieldOffset("lerpfrac");
	prog->fieldoffsets.renderflags = PRVM_ED_FindFieldOffset("renderflags");
	//prog->fieldoffsets.forceshader = PRVM_ED_FindFieldOffset("forceshader");
	//prog->fieldoffsets.dimension_hit = PRVM_ED_FindFieldOffset("dimension_hit");
	//prog->fieldoffsets.dimension_solid = PRVM_ED_FindFieldOffset("dimension_solid");
	//prog->fieldoffsets.groundentity = PRVM_ED_FindFieldOffset("groundentity");
	//prog->fieldoffsets.hull = PRVM_ED_FindFieldOffset("hull");
	prog->fieldoffsets.colormod = PRVM_ED_FindFieldOffset("colormod");
	prog->fieldoffsets.effects = PRVM_ED_FindFieldOffset("effects");
	prog->fieldoffsets.tag_entity = PRVM_ED_FindFieldOffset("tag_entity");
	prog->fieldoffsets.tag_index = PRVM_ED_FindFieldOffset("tag_index");
	prog->funcoffsets.CSQC_Init = PRVM_ED_FindFunctionOffset("CSQC_Init");
	prog->funcoffsets.CSQC_InputEvent = PRVM_ED_FindFunctionOffset("CSQC_InputEvent");
	prog->funcoffsets.CSQC_UpdateView = PRVM_ED_FindFunctionOffset("CSQC_UpdateView");
	prog->funcoffsets.CSQC_ConsoleCommand = PRVM_ED_FindFunctionOffset("CSQC_ConsoleCommand");
	prog->funcoffsets.CSQC_Shutdown = PRVM_ED_FindFunctionOffset("CSQC_Shutdown");
	prog->funcoffsets.CSQC_Parse_TempEntity = PRVM_ED_FindFunctionOffset("CSQC_Parse_TempEntity");
	prog->funcoffsets.CSQC_Parse_StuffCmd = PRVM_ED_FindFunctionOffset("CSQC_Parse_StuffCmd");
	prog->funcoffsets.CSQC_Parse_Print = PRVM_ED_FindFunctionOffset("CSQC_Parse_Print");
	prog->funcoffsets.CSQC_Parse_CenterPrint = PRVM_ED_FindFunctionOffset("CSQC_Parse_CenterPrint");
	prog->funcoffsets.CSQC_Ent_Update = PRVM_ED_FindFunctionOffset("CSQC_Ent_Update");
	prog->funcoffsets.CSQC_Ent_Remove = PRVM_ED_FindFunctionOffset("CSQC_Ent_Remove");
	prog->funcoffsets.CSQC_Event = PRVM_ED_FindFunctionOffset("CSQC_Event");

	// mqc
	prog->funcoffsets.m_init = PRVM_ED_FindFunctionOffset("m_init");
#ifdef NG_MENU
	prog->funcoffsets.m_display = PRVM_ED_FindFunctionOffset("m_display");
	prog->funcoffsets.m_hide = PRVM_ED_FindFunctionOffset("m_hide");
#endif
	prog->funcoffsets.m_keydown = PRVM_ED_FindFunctionOffset("m_keydown");
	prog->funcoffsets.m_keyup = PRVM_ED_FindFunctionOffset("m_keyup");
	prog->funcoffsets.m_draw = PRVM_ED_FindFunctionOffset("m_draw");
	prog->funcoffsets.m_toggle = PRVM_ED_FindFunctionOffset("m_toggle");
	prog->funcoffsets.m_shutdown = PRVM_ED_FindFunctionOffset("m_shutdown");
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

void PRVM_ResetProg()
{
	PRVM_GCALL(reset_cmd)();
	Mem_FreePool(&prog->progs_mempool);
	memset(prog,0,sizeof(prvm_prog_t));
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
	if( (unsigned) filesize < (6 + prog->progs->numstatements) * sizeof( int ) ) {
		Mem_Free(lno);
		return;
	}

	header = (unsigned int *) lno;
	if( header[ 0 ] == *(unsigned int *) "LNOF" &&
		LittleLong( header[ 1 ] ) == 1 &&
		(unsigned int)LittleLong( header[ 2 ] ) == (unsigned int)prog->progs->numglobaldefs &&
		(unsigned int)LittleLong( header[ 3 ] ) == (unsigned int)prog->progs->numglobals &&
		(unsigned int)LittleLong( header[ 4 ] ) == (unsigned int)prog->progs->numfielddefs &&
		(unsigned int)LittleLong( header[ 5 ] ) == (unsigned int)prog->progs->numstatements )
	{
		prog->statement_linenums = (int *)Mem_Alloc(prog->progs_mempool, prog->progs->numstatements * sizeof( int ) );
		memcpy( prog->statement_linenums, (int *) lno + 6, prog->progs->numstatements * sizeof( int ) );
	}
	Mem_Free( lno );
}

/*
===============
PRVM_LoadProgs
===============
*/
void PRVM_LoadProgs (const char * filename, int numrequiredfunc, char **required_func, int numrequiredfields, prvm_required_field_t *required_field, int numrequiredglobals, char **required_global)
{
	int i;
	dstatement_t *st;
	ddef_t *infielddefs;
	dfunction_t *dfunctions;
	fs_offset_t filesize;

	if( prog->loaded ) {
		PRVM_ERROR ("PRVM_LoadProgs: there is already a %s program loaded!", PRVM_NAME );
	}

	prog->progs = (dprograms_t *)FS_LoadFile (filename, prog->progs_mempool, false, &filesize);
	if (prog->progs == NULL || filesize < (fs_offset_t)sizeof(dprograms_t))
		PRVM_ERROR ("PRVM_LoadProgs: couldn't load %s for %s", filename, PRVM_NAME);

	Con_DPrintf("%s programs occupy %iK.\n", PRVM_NAME, (int)(filesize/1024));

	prog->filecrc = CRC_Block((unsigned char *)prog->progs, filesize);

// byte swap the header
	for (i = 0;i < (int) sizeof(*prog->progs) / 4;i++)
		((int *)prog->progs)[i] = LittleLong ( ((int *)prog->progs)[i] );

	if (prog->progs->version != PROG_VERSION)
		PRVM_ERROR ("%s: %s has wrong version number (%i should be %i)", PRVM_NAME, filename, prog->progs->version, PROG_VERSION);
	if (prog->progs->crc != prog->headercrc)
		PRVM_ERROR ("%s: %s system vars have been modified, progdefs.h is out of date", PRVM_NAME, filename);

	//prog->functions = (dfunction_t *)((unsigned char *)progs + progs->ofs_functions);
	dfunctions = (dfunction_t *)((unsigned char *)prog->progs + prog->progs->ofs_functions);

	prog->strings = (char *)prog->progs + prog->progs->ofs_strings;
	prog->stringssize = 0;
	for (i = 0;i < prog->progs->numstrings;i++)
	{
		if (prog->progs->ofs_strings + prog->stringssize >= (int)filesize)
			PRVM_ERROR ("%s: %s strings go past end of file", PRVM_NAME, filename);
		prog->stringssize += (int)strlen (prog->strings + prog->stringssize) + 1;
	}
	prog->numknownstrings = 0;
	prog->maxknownstrings = 0;
	prog->knownstrings = NULL;
	prog->knownstrings_freeable = NULL;

	prog->globaldefs = (ddef_t *)((unsigned char *)prog->progs + prog->progs->ofs_globaldefs);

	// we need to expand the fielddefs list to include all the engine fields,
	// so allocate a new place for it
	infielddefs = (ddef_t *)((unsigned char *)prog->progs + prog->progs->ofs_fielddefs);
	//												( + DPFIELDS			   )
	prog->fielddefs = (ddef_t *)Mem_Alloc(prog->progs_mempool, (prog->progs->numfielddefs + numrequiredfields) * sizeof(ddef_t));

	prog->statements = (dstatement_t *)((unsigned char *)prog->progs + prog->progs->ofs_statements);

	prog->statement_profile = (double *)Mem_Alloc(prog->progs_mempool, prog->progs->numstatements * sizeof(*prog->statement_profile));

	// moved edict_size calculation down below field adding code

	//pr_global_struct = (globalvars_t *)((unsigned char *)progs + progs->ofs_globals);
	prog->globals.generic = (float *)((unsigned char *)prog->progs + prog->progs->ofs_globals);

// byte swap the lumps
	for (i=0 ; i<prog->progs->numstatements ; i++)
	{
		prog->statements[i].op = LittleShort(prog->statements[i].op);
		prog->statements[i].a = LittleShort(prog->statements[i].a);
		prog->statements[i].b = LittleShort(prog->statements[i].b);
		prog->statements[i].c = LittleShort(prog->statements[i].c);
	}

	prog->functions = (mfunction_t *)Mem_Alloc(prog->progs_mempool, sizeof(mfunction_t) * prog->progs->numfunctions);
	for (i = 0;i < prog->progs->numfunctions;i++)
	{
		prog->functions[i].first_statement = LittleLong (dfunctions[i].first_statement);
		prog->functions[i].parm_start = LittleLong (dfunctions[i].parm_start);
		prog->functions[i].s_name = LittleLong (dfunctions[i].s_name);
		prog->functions[i].s_file = LittleLong (dfunctions[i].s_file);
		prog->functions[i].numparms = LittleLong (dfunctions[i].numparms);
		prog->functions[i].locals = LittleLong (dfunctions[i].locals);
		memcpy(prog->functions[i].parm_size, dfunctions[i].parm_size, sizeof(dfunctions[i].parm_size));
	}

	for (i=0 ; i<prog->progs->numglobaldefs ; i++)
	{
		prog->globaldefs[i].type = LittleShort (prog->globaldefs[i].type);
		prog->globaldefs[i].ofs = LittleShort (prog->globaldefs[i].ofs);
		prog->globaldefs[i].s_name = LittleLong (prog->globaldefs[i].s_name);
	}

	// copy the progs fields to the new fields list
	for (i = 0;i < prog->progs->numfielddefs;i++)
	{
		prog->fielddefs[i].type = LittleShort (infielddefs[i].type);
		if (prog->fielddefs[i].type & DEF_SAVEGLOBAL)
			PRVM_ERROR ("PRVM_LoadProgs: prog->fielddefs[i].type & DEF_SAVEGLOBAL in %s", PRVM_NAME);
		prog->fielddefs[i].ofs = LittleShort (infielddefs[i].ofs);
		prog->fielddefs[i].s_name = LittleLong (infielddefs[i].s_name);
	}

	// append the required fields
	for (i = 0;i < (int) numrequiredfields;i++)
	{
		prog->fielddefs[prog->progs->numfielddefs].type = required_field[i].type;
		prog->fielddefs[prog->progs->numfielddefs].ofs = prog->progs->entityfields;
		prog->fielddefs[prog->progs->numfielddefs].s_name = PRVM_SetEngineString(required_field[i].name);
		if (prog->fielddefs[prog->progs->numfielddefs].type == ev_vector)
			prog->progs->entityfields += 3;
		else
			prog->progs->entityfields++;
		prog->progs->numfielddefs++;
	}

	// check required functions
	for(i=0 ; i < numrequiredfunc ; i++)
		if(PRVM_ED_FindFunction(required_func[i]) == 0)
			PRVM_ERROR("%s: %s not found in %s",PRVM_NAME, required_func[i], filename);

	// check required globals
	for(i=0 ; i < numrequiredglobals ; i++)
		if(PRVM_ED_FindGlobal(required_global[i]) == 0)
			PRVM_ERROR("%s: %s not found in %s",PRVM_NAME, required_global[i], filename);

	for (i=0 ; i<prog->progs->numglobals ; i++)
		((int *)prog->globals.generic)[i] = LittleLong (((int *)prog->globals.generic)[i]);

	// moved edict_size calculation down here, below field adding code
	// LordHavoc: this no longer includes the prvm_edict_t header
	prog->edict_size = prog->progs->entityfields * 4;
	prog->edictareasize = prog->edict_size * prog->limit_edicts;

	// LordHavoc: bounds check anything static
	for (i = 0,st = prog->statements;i < prog->progs->numstatements;i++,st++)
	{
		switch (st->op)
		{
		case OP_IF:
		case OP_IFNOT:
			if ((unsigned short) st->a >= prog->progs->numglobals || st->b + i < 0 || st->b + i >= prog->progs->numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds IF/IFNOT (statement %d) in %s", i, PRVM_NAME);
			break;
		case OP_GOTO:
			if (st->a + i < 0 || st->a + i >= prog->progs->numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds GOTO (statement %d) in %s", i, PRVM_NAME);
			break;
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
			if ((unsigned short) st->a >= prog->progs->numglobals || (unsigned short) st->b >= prog->progs->numglobals || (unsigned short) st->c >= prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d)", i);
			break;
		// global none global
		case OP_NOT_F:
		case OP_NOT_V:
		case OP_NOT_S:
		case OP_NOT_FNC:
		case OP_NOT_ENT:
			if ((unsigned short) st->a >= prog->progs->numglobals || (unsigned short) st->c >= prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
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
			if ((unsigned short) st->a >= prog->progs->numglobals || (unsigned short) st->b >= prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
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
			if ((unsigned short) st->a >= prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			break;
		default:
			Con_DPrintf("PRVM_LoadProgs: unknown opcode %d at statement %d in %s\n", st->op, i, PRVM_NAME);
			break;
		}
	}

	PRVM_LoadLNO(filename);

	PRVM_Init_Exec();

	prog->loaded = TRUE;

	// set flags & ddef_ts in prog

	prog->flag = 0;

	PRVM_FindOffsets();

	// check if OP_STATE animation is possible in this dat file
	if (prog->fieldoffsets.nextthink >= 0 && prog->fieldoffsets.frame >= 0 && prog->fieldoffsets.think >= 0 && prog->globaloffsets.self >= 0)
		prog->flag |= PRVM_OP_STATE;

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

	counts = (int *)Mem_Alloc(tempmempool, prog->progs->numfielddefs * sizeof(int));
	for (ednum = 0;ednum < prog->max_edicts;ednum++)
	{
		ed = PRVM_EDICT_NUM(ednum);
		if (ed->priv.required->free)
			continue;
		for (i = 1;i < prog->progs->numfielddefs;i++)
		{
			d = &prog->fielddefs[i];
			name = PRVM_GetString(d->s_name);
			if (name[strlen(name)-2] == '_')
				continue;	// skip _x, _y, _z vars
			v = (int *)((char *)ed->fields.vp + d->ofs*4);
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
	for (i = 0;i < prog->progs->numfielddefs;i++)
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
			sprintf (tempstring2, "bad type %i ", d->type & ~DEF_SAVEGLOBAL);
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
		sprintf(tempstring2, "%5d", counts[i]);
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
	Con_Printf("%s: %i entity fields (%i in use), totalling %i bytes per edict (%i in use), %i edicts allocated, %i bytes total spent on edict fields (%i needed)\n", PRVM_NAME, prog->progs->entityfields, used, prog->progs->entityfields * 4, usedamount * 4, prog->max_edicts, prog->progs->entityfields * 4 * prog->max_edicts, usedamount * 4 * prog->max_edicts);

	PRVM_End;
}

void PRVM_Globals_f (void)
{
	int i;
	// TODO
	/*if (!sv.active)
	{
		Con_Print("no progs loaded\n");
		return;
	}*/
	if(Cmd_Argc () != 2)
	{
		Con_Print("prvm_globals <program name>\n");
		return;
	}

	PRVM_Begin;
	if(!PRVM_SetProgFromString (Cmd_Argv (1)))
		return;

	Con_Printf("%s :", PRVM_NAME);

	for (i = 0;i < prog->progs->numglobaldefs;i++)
		Con_Printf("%s\n", PRVM_GetString(prog->globaldefs[i].s_name));
	Con_Printf("%i global variables, totalling %i bytes\n", prog->progs->numglobals, prog->progs->numglobals * 4);

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
		Con_Printf( "%s: %s\n", Cmd_Argv(2), PRVM_ValueString( (etype_t)global->type, (prvm_eval_t *) &prog->globals.generic[ global->ofs ] ) );
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
		PRVM_ED_ParseEpair( NULL, global, Cmd_Argv(3) );
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
	Cmd_AddCommand ("prvm_edicts", PRVM_ED_PrintEdicts_f, "set a property on an entity number in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_edictcount", PRVM_ED_Count_f, "prints number of active entities in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_profile", PRVM_Profile_f, "prints execution statistics about the most used QuakeC functions in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_fields", PRVM_Fields_f, "prints usage statistics on properties (how many entities have non-zero values) in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_globals", PRVM_Globals_f, "prints all global variables in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_global", PRVM_Global_f, "prints value of a specified global variable in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_globalset", PRVM_GlobalSet_f, "sets value of a specified global variable in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_edictset", PRVM_ED_EdictSet_f, "changes value of a specified property of a specified entity in the selected VM (server, client, menu)");
	Cmd_AddCommand ("prvm_printfunction", PRVM_PrintFunction_f, "prints a disassembly (QuakeC instructions) of the specified function in the selected VM (server, client, menu)");
	// LordHavoc: optional runtime bounds checking (speed drain, but worth it for security, on by default - breaks most QCCX features (used by CRMod and others))
	Cvar_RegisterVariable (&prvm_boundscheck);
	Cvar_RegisterVariable (&prvm_traceqc);
	Cvar_RegisterVariable (&prvm_statementprofiling);

	//VM_Cmd_Init();
}

/*
===============
PRVM_InitProg
===============
*/
void PRVM_InitProg(int prognr)
{
	if(prognr < 0 || prognr >= PRVM_MAXPROGS)
		Sys_Error("PRVM_InitProg: Invalid program number %i",prognr);

	prog = &prog_list[prognr];

	if(prog->loaded)
		PRVM_ResetProg();

	memset(prog, 0, sizeof(prvm_prog_t));

	prog->error_cmd = Host_Error;
}

int PRVM_GetProgNr()
{
	return prog - prog_list;
}

void *_PRVM_Alloc(size_t buffersize, const char *filename, int fileline)
{
	return _Mem_Alloc(prog->progs_mempool, buffersize, filename, fileline);
}

void _PRVM_Free(void *buffer, const char *filename, int fileline)
{
	_Mem_Free(buffer, filename, fileline);
}

void _PRVM_FreeAll(const char *filename, int fileline)
{
	prog->progs = NULL;
	prog->fielddefs = NULL;
	prog->functions = NULL;
	_Mem_EmptyPool(prog->progs_mempool, filename, fileline);
}

// LordHavoc: turned PRVM_EDICT_NUM into a #define for speed reasons
prvm_edict_t *PRVM_EDICT_NUM_ERROR(int n, char *filename, int fileline)
{
	PRVM_ERROR ("PRVM_EDICT_NUM: %s: bad number %i (called at %s:%i)", PRVM_NAME, n, filename, fileline);
	return NULL;
}

/*
int NUM_FOR_EDICT_ERROR(prvm_edict_t *e)
{
	PRVM_ERROR ("PRVM_NUM_FOR_EDICT: bad pointer %p (world is %p, entity number would be %i)", e, prog->edicts, e - prog->edicts);
	return 0;
}

int PRVM_NUM_FOR_EDICT(prvm_edict_t *e)
{
	int n;
	n = e - prog->edicts;
	if ((unsigned int)n >= prog->limit_edicts)
		Host_Error ("PRVM_NUM_FOR_EDICT: bad pointer");
	return n;
}

//int NoCrash_NUM_FOR_EDICT(prvm_edict_t *e)
//{
//	return e - prog->edicts;
//}

//#define	PRVM_EDICT_TO_PROG(e) ((unsigned char *)(((prvm_edict_t *)e)->v) - (unsigned char *)(prog->edictsfields))
//#define PRVM_PROG_TO_EDICT(e) (prog->edicts + ((e) / (progs->entityfields * 4)))
int PRVM_EDICT_TO_PROG(prvm_edict_t *e)
{
	int n;
	n = e - prog->edicts;
	if ((unsigned int)n >= (unsigned int)prog->max_edicts)
		Host_Error("PRVM_EDICT_TO_PROG: invalid edict %8p (number %i compared to world at %8p)", e, n, prog->edicts);
	return n;// EXPERIMENTAL
	//return (unsigned char *)e->v - (unsigned char *)prog->edictsfields;
}
prvm_edict_t *PRVM_PROG_TO_EDICT(int n)
{
	if ((unsigned int)n >= (unsigned int)prog->max_edicts)
		Host_Error("PRVM_PROG_TO_EDICT: invalid edict number %i", n);
	return prog->edicts + n; // EXPERIMENTAL
	//return prog->edicts + ((n) / (progs->entityfields * 4));
}
*/


sizebuf_t vm_tempstringsbuf;

const char *PRVM_GetString(int num)
{
	if (num >= 0)
	{
		if (num < prog->stringssize)
			return prog->strings + num;
		else
#if 1
		if (num <= prog->stringssize + vm_tempstringsbuf.maxsize)
		{
			num -= prog->stringssize;
			if (num < vm_tempstringsbuf.cursize)
				return (char *)vm_tempstringsbuf.data + num;
			else
			{
				VM_Warning("PRVM_GetString: Invalid temp-string offset (%i >= %i vm_tempstringsbuf.cursize)", num, vm_tempstringsbuf.cursize);
				return "";
			}
		}
		else
#endif
		{
			VM_Warning("PRVM_GetString: Invalid constant-string offset (%i >= %i prog->stringssize)", num, prog->stringssize);
			return "";
		}
	}
	else
	{
		num = -1 - num;
#if 0
		if (num >= (1<<30))
		{
			// special range reserved for tempstrings
			num -= (1<<30);
			if (num < vm_tempstringsbuf.cursize)
				return (char *)vm_tempstringsbuf.data + num;
			else
			{
				VM_Warning("PRVM_GetString: Invalid temp-string offset (%i >= %i vm_tempstringsbuf.cursize)", num, vm_tempstringsbuf.cursize);
				return "";
			}
		}
		else
#endif
		if (num < prog->numknownstrings)
		{
			if (!prog->knownstrings[num])
				VM_Warning("PRVM_GetString: Invalid zone-string offset (%i has been freed)", num);
			return prog->knownstrings[num];
		}
		else
		{
			VM_Warning("PRVM_GetString: Invalid zone-string offset (%i >= %i)", num, prog->numknownstrings);
			return "";
		}
	}
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
#else
		return -1 - ((1<<30) + (s - (char *)vm_tempstringsbuf.data));
#endif
	// see if it's a known string address
	for (i = 0;i < prog->numknownstrings;i++)
		if (prog->knownstrings[i] == s)
			return -1 - i;
	// new unknown engine string
	if (developer.integer >= 100)
		Con_Printf("new engine string %p\n", s);
	for (i = prog->firstfreeknownstring;i < prog->numknownstrings;i++)
		if (!prog->knownstrings[i])
			break;
	if (i >= prog->numknownstrings)
	{
		if (i >= prog->maxknownstrings)
		{
			const char **oldstrings = prog->knownstrings;
			const unsigned char *oldstrings_freeable = prog->knownstrings_freeable;
			prog->maxknownstrings += 128;
			prog->knownstrings = (const char **)PRVM_Alloc(prog->maxknownstrings * sizeof(char *));
			prog->knownstrings_freeable = (unsigned char *)PRVM_Alloc(prog->maxknownstrings * sizeof(unsigned char));
			if (prog->numknownstrings)
			{
				memcpy((char **)prog->knownstrings, oldstrings, prog->numknownstrings * sizeof(char *));
				memcpy((char **)prog->knownstrings_freeable, oldstrings_freeable, prog->numknownstrings * sizeof(unsigned char));
			}
		}
		prog->numknownstrings++;
	}
	prog->firstfreeknownstring = i + 1;
	prog->knownstrings[i] = s;
	return -1 - i;
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
	if (developer.integer >= 300)
		Con_Printf("PRVM_SetTempString: cursize %i, size %i\n", vm_tempstringsbuf.cursize, size);
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
			if (developer.integer >= 100)
				Con_Printf("PRVM_SetTempString: enlarging tempstrings buffer (%iKB -> %iKB)\n", old.maxsize/1024, vm_tempstringsbuf.maxsize/1024);
			vm_tempstringsbuf.data = Mem_Alloc(sv_mempool, vm_tempstringsbuf.maxsize);
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
			prog->maxknownstrings += 128;
			prog->knownstrings = (const char **)PRVM_Alloc(prog->maxknownstrings * sizeof(char *));
			prog->knownstrings_freeable = (unsigned char *)PRVM_Alloc(prog->maxknownstrings * sizeof(unsigned char));
			if (prog->numknownstrings)
			{
				memcpy((char **)prog->knownstrings, oldstrings, prog->numknownstrings * sizeof(char *));
				memcpy((char **)prog->knownstrings_freeable, oldstrings_freeable, prog->numknownstrings * sizeof(unsigned char));
			}
		}
		prog->numknownstrings++;
	}
	prog->firstfreeknownstring = i + 1;
	prog->knownstrings[i] = (char *)PRVM_Alloc(bufferlength);
	prog->knownstrings_freeable[i] = true;
	if (pointer)
		*pointer = (char *)(prog->knownstrings[i]);
	return -1 - i;
}

void PRVM_FreeString(int num)
{
	if (num == 0)
		PRVM_ERROR("PRVM_FreeString: attempt to free a NULL string");
	else if (num >= 0 && num < prog->stringssize)
		PRVM_ERROR("PRVM_FreeString: attempt to free a constant string");
	else if (num < 0 && num >= -prog->numknownstrings)
	{
		num = -1 - num;
		if (!prog->knownstrings[num])
			PRVM_ERROR("PRVM_FreeString: attempt to free a non-existent or already freed string");
		if (!prog->knownstrings[num])
			PRVM_ERROR("PRVM_FreeString: attempt to free a string owned by the engine");
		PRVM_Free((char *)prog->knownstrings[num]);
		prog->knownstrings[num] = NULL;
		prog->knownstrings_freeable[num] = false;
		prog->firstfreeknownstring = min(prog->firstfreeknownstring, num);
	}
	else
		PRVM_ERROR("PRVM_FreeString: invalid string offset %i", num);
}

