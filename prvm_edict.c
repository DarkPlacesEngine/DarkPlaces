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
cvar_t	prvm_boundscheck = {0, "prvm_boundscheck", "1"};
// LordHavoc: prints every opcode as it executes - warning: this is significant spew
cvar_t	prvm_traceqc = {0, "prvm_traceqc", "0"};

ddef_t *PRVM_ED_FindField (const char *name);
mfunction_t *PRVM_ED_FindFunction (const char *name);

//============================================================================
// mempool handling

/*
===============
PRVM_MEM_Alloc
===============
*/
void PRVM_MEM_Alloc()
{
	int i;

	// reserve space for the null entity aka world
	// check bound of max_edicts
	prog->max_edicts = bound(1, prog->max_edicts, prog->limit_edicts);
	prog->num_edicts = bound(1, prog->num_edicts, prog->max_edicts);	

	// edictprivate_size has to be min as big prvm_edict_private_t
	prog->edictprivate_size = max(prog->edictprivate_size,(int)sizeof(prvm_edict_private_t)); 

	// alloc edicts
	prog->edicts = Mem_Alloc(prog->edicts_mempool,prog->limit_edicts * sizeof(prvm_edict_t));
	
	// alloc edict private space
	prog->edictprivate = Mem_Alloc(prog->edicts_mempool, prog->max_edicts * prog->edictprivate_size);
	
	// alloc edict fields
	prog->edictsfields = Mem_Alloc(prog->edicts_mempool, prog->max_edicts * prog->edict_size);

	// set edict pointers
	for(i = 0; i < prog->max_edicts; i++)
	{
		prog->edicts[i].p.e = (prvm_edict_private_t *)((qbyte  *)prog->edictprivate + i * prog->edictprivate_size);
		prog->edicts[i].v = (void*)((qbyte *)prog->edictsfields + i * prog->edict_size);
	}
}

/*
===============
PRVM_MEM_IncreaseEdicts
===============
*/
void PRVM_MEM_IncreaseEdicts()
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

	prog->edictsfields = Mem_Alloc(prog->edicts_mempool, prog->max_edicts * prog->edict_size);
	prog->edictprivate = Mem_Alloc(prog->edicts_mempool, prog->max_edicts * prog->edictprivate_size);

	memcpy(prog->edictsfields, oldedictsfields, oldmaxedicts * prog->edict_size);
	memcpy(prog->edictprivate, oldedictprivate, oldmaxedicts * prog->edictprivate_size);

	//set e and v pointers
	for(i = 0; i < prog->max_edicts; i++)
	{
		prog->edicts[i].p.e = (prvm_edict_private_t *)((qbyte  *)prog->edictprivate + i * prog->edictprivate_size);
		prog->edicts[i].v = (void*)((qbyte *)prog->edictsfields + i * prog->edict_size);
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
		return 0;
	return d->ofs*4;
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
	if(prognr && prognr < PRVM_MAXPROGS)
	{	
		if(prog_list[prognr].loaded)
			prog = &prog_list[prognr];
		else
			PRVM_ERROR("%i(%s) not loaded !\n", prognr, PRVM_NAME);
		return;
	}
	PRVM_ERROR("Invalid program number %i\n", prognr);
}

/*
=================
PRVM_ED_ClearEdict

Sets everything to NULL
=================
*/
void PRVM_ED_ClearEdict (prvm_edict_t *e)
{
	int num;
	memset (e->v, 0, prog->progs->entityfields * 4);
	e->p.e->free = false;
	// LordHavoc: for consistency set these here
	num = PRVM_NUM_FOR_EDICT(e) - 1;

	// AK: Let the init_edict function determine if something needs to be initialized
	PRVM_GCALL(init_edict)(num);	
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
	for (i = 1;i < prog->num_edicts;i++)
	{
		e = PRVM_EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->p.e->free && ( e->p.e->freetime < 2 || (*prog->time - e->p.e->freetime) > 0.5 ) )
		{
			PRVM_ED_ClearEdict (e);
			return e;
		}
	}

	if (i == MAX_EDICTS)
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
	// dont delete the null entity (world)
	if(PRVM_NUM_FOR_EDICT(ed) == 0)
		return;

	PRVM_GCALL(free_edict)(ed);

	ed->p.e->free = true;
	ed->p.e->freetime = *prog->time;
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
	static char line[1024]; // LordHavoc: enlarged a bit (was 256)
	ddef_t *def;
	mfunction_t *f;
	int n;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		strlcpy (line, PRVM_GetString (val->string), sizeof (line));
		break;
	case ev_entity:
		n = val->edict;
		if (n < 0 || n >= MAX_EDICTS)
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
		sprintf (line, "bad type %i", type);
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
	static char line[4096];
	int i;
	char *s;
	ddef_t *def;
	mfunction_t *f;
	
	type &= ~DEF_SAVEGLOBAL;
	
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
		snprintf (line, sizeof (line), "%i", PRVM_NUM_FOR_EDICT(PRVM_PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = pr_functions + val->function;
		strlcpy (line, PRVM_GetString (f->s_name), sizeof (line));
		break;
	case ev_field:
		def = PRVM_ED_FieldAtOfs ( val->_int );
		snprintf (line, sizeof (line), ".%s", PRVM_GetString(def->s_name));
		break;
	case ev_void:
		snprintf (line, sizeof (line), "void");
		break;
	case ev_float:
		snprintf (line, sizeof (line), "%f", val->_float);
		break;
	case ev_vector:
		snprintf (line, sizeof (line), "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		snprintf (line, sizeof (line), "bad type %i", type);
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
	int		i;
	ddef_t	*def;
	void	*val;
	static char	line[128];

	val = (void *)&prog->globals[ofs];
	def = PRVM_ED_GlobalAtOfs(ofs);
	if (!def)
		sprintf (line,"%i(?)", ofs);
	else
	{
		s = PRVM_ValueString (def->type, val);
		sprintf (line,"%i(%s)%s", ofs, PRVM_GetString(def->s_name), s);
	}

	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");

	return line;
}

char *PRVM_GlobalStringNoContents (int ofs)
{
	int		i;
	ddef_t	*def;
	static char	line[128];

	def = PRVM_ED_GlobalAtOfs(ofs);
	if (!def)
		sprintf (line,"%i(?)", ofs);
	else
		sprintf (line,"%i(%s)", ofs, PRVM_GetString(def->s_name));

	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");

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
	int		l;
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;
	char	tempstring[8192], tempstring2[260]; // temporary string buffers

	if (ed->p.e->free)
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

		v = (int *)((char *)ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j=0 ; j<prvm_type_size[type] ; j++)
			if (v[j])
				break;
		if (j == prvm_type_size[type])
			continue;

		if (strlen(name) > 256)
		{
			memcpy (tempstring2, name, 256);
			tempstring2[256] = tempstring2[257] = tempstring2[258] = '.';
			tempstring2[259] = 0;
			name = tempstring2;
		}
		strcat(tempstring, name);
		for (l = strlen(name);l < 14;l++)
			strcat(tempstring, " ");
		strcat(tempstring, " ");

		name = PRVM_ValueString(d->type, (prvm_eval_t *)v);
		if (strlen(name) > 256)
		{
			memcpy (tempstring2, name, 256);
			tempstring2[256] = tempstring2[257] = tempstring2[258] = '.';
			tempstring2[259] = 0;
			name = tempstring2;
		}
		strcat(tempstring, name);
		strcat(tempstring, "\n");
		if (strlen(tempstring) >= 4096)
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
	char	*name;
	int		type;

	FS_Print(f, "{\n");

	if (ed->p.e->free)
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

		v = (int *)((char *)ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<prvm_type_size[type] ; j++)
			if (v[j])
				break;
		if (j == prvm_type_size[type])
			continue;

		FS_Printf(f,"\"%s\" ",name);
		FS_Printf(f,"\"%s\"\n", PRVM_UglyValueString(d->type, (prvm_eval_t *)v));
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
			if (ent->p.e->free)
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
	char		*name;
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
		FS_Printf(f,"\"%s\"\n", PRVM_UglyValueString(type, (prvm_eval_t *)&prog->globals[def->ofs]));
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
	char keyname[1024]; // LordHavoc: good idea? bad idea?  was 64
	ddef_t *key;

	while (1)
	{
		// parse key
		if (!COM_ParseToken(&data, false))
			PRVM_ERROR ("PRVM_ED_ParseEntity: EOF without closing brace");
		if (com_token[0] == '}')
			break;

		strcpy (keyname, com_token);

		// parse value
		if (!COM_ParseToken(&data, false))
			PRVM_ERROR ("PRVM_ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PRVM_ERROR ("PRVM_ED_ParseEntity: closing brace without data");

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
PRVM_ED_NewString
=============
*/
char *PRVM_ED_NewString (const char *string)
{
	char *new, *new_p;
	int i,l;

	l = strlen(string) + 1;
	new = Mem_Alloc(prog->edictstring_mempool, l);
	new_p = new;

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return new;
}

/*
=============
PRVM_ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
qboolean PRVM_ED_ParseEpair(prvm_edict_t *ent, ddef_t *key, const char *s)
{
	int i;
	ddef_t *def;
	prvm_eval_t *val;
	mfunction_t *func;

	if (ent)
		val = (prvm_eval_t *)((int *)ent->v + key->ofs);
	else
		val = (prvm_eval_t *)((int *)pr_globals + key->ofs);
	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		val->string = PRVM_SetString(PRVM_ED_NewString(s));
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
		if (i < 0 || i >= MAX_EDICTS)
			Con_Printf("PRVM_ED_ParseEpair: ev_entity reference too large (edict %i >= MAX_EDICTS %i) on %s\n", i, MAX_EDICTS, PRVM_NAME);
		while (i >= prog->max_edicts)
			PRVM_MEM_IncreaseEdicts();
			//SV_IncreaseEdicts();
		// if SV_IncreaseEdicts was called the base pointer needs to be updated
		if (ent)
			val = (prvm_eval_t *)((int *)ent->v + key->ofs);
		val->edict = PRVM_EDICT_TO_PROG(EDICT_NUM(i));
		break;

	case ev_field:
		def = PRVM_ED_FindField(s);
		if (!def)
		{
			Con_DPrintf("PRVM_ED_ParseEpair: Can't find field %s in %s\n", s, PRVM_NAME);
			return false;
		}
		val->_int = PRVM_G_INT(def->ofs);
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
		Con_Printf("PRVM_ED_ParseEpair: Unknown key->type %i for key \"%s\" on %s\n", key->type, PR_GetString(key->s_name), PRVM_NAME);
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
	{
		Con_Printf("Key %s not found !\n", Cmd_Argv(3));
		return;
	}

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
const char *PRVM_ED_ParseEdict (const char *data, prvm_edict_t *ent)
{
	ddef_t *key;
	qboolean anglehack;
	qboolean init;
	char keyname[256];
	int n;

	init = false;

// go through all the dictionary pairs
	while (1)
	{
	// parse key
		if (!COM_ParseToken(&data, false))
			PRVM_ERROR ("PRVM_ED_ParseEntity: EOF without closing brace");
		if (com_token[0] == '}')
			break;

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(com_token, "angle"))
		{
			strcpy (com_token, "angles");
			anglehack = true;
		}
		else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light"))
			strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix keynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

	// parse value
		if (!COM_ParseToken(&data, false))
			PRVM_ERROR ("PRVM_ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PRVM_ERROR ("PRVM_ED_ParseEntity: closing brace without data");

		init = true;

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
			strcpy (temp, com_token);
			sprintf (com_token, "0 %s 0", temp);
		}

		if (!PRVM_ED_ParseEpair(ent, key, com_token))
			PRVM_ERROR ("PRVM_ED_ParseEdict: parse error");
	}

	if (!init)
		ent->p.e->free = true;

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
		if (!COM_ParseToken(&data, false))
			break;
		if (com_token[0] != '{')
			PRVM_ERROR ("PRVM_ED_LoadFromFile: %s: found %s when expecting {", PRVM_NAME, com_token);

		// CHANGED: this is not conform to ED_LoadFromFile
		if(!prog->num_edicts) 
			ent = PRVM_EDICT_NUM(0);
		else 
			ent = PRVM_ED_Alloc();

		// clear it
		if (ent != prog->edicts)	// hack
			memset (ent->v, 0, prog->progs->entityfields * 4);

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
		if(prog->self && prog->flag & PRVM_FE_CLASSNAME)
		{
			string_t handle =  *(string_t*)&((float*)ent->v)[PRVM_ED_FindFieldOffset("classname")];
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
			PRVM_G_INT(prog->self->ofs) = PRVM_EDICT_TO_PROG(ent);
			PRVM_ExecuteProgram (func - prog->functions, "");
		}
	
		spawned++;
		if (ent->p.e->free)
			died++;
	}

	Con_DPrintf("%s: %i new entities parsed, %i new inhibited, %i (%i new) spawned (whereas %i removed self, %i stayed)\n", PRVM_NAME, parsed, inhibited, prog->num_edicts, spawned, died, spawned - died);
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
	/*mempool_t *t1, *t2, *t3;

	t1 = prog->progs_mempool;
	t2 = prog->edictstring_mempool;
	t3 = prog->edicts_mempool;
	
	Mem_EmptyPool(prog->progs_mempool);
	Mem_EmptyPool(prog->edictstring_mempool);
	Mem_EmptyPool(prog->edicts_mempool);*/
	Mem_FreePool(&prog->progs_mempool);
	Mem_FreePool(&prog->edictstring_mempool);
	Mem_FreePool(&prog->edicts_mempool);
	
	memset(prog,0,sizeof(prvm_prog_t));
	
	/*prog->time = &prog->_time;
	
	prog->progs_mempool = t1;
	prog->edictstring_mempool = t2;
	prog->edicts_mempool = t3;*/

	PRVM_GCALL(reset_cmd)();
}

/*
===============
PRVM_LoadProgs
===============
*/
void PRVM_LoadProgs (const char * filename, int numrequiredfunc, char **required_func)
{
	int i;
	dstatement_t *st;
	ddef_t *infielddefs;
	dfunction_t *dfunctions;

	Mem_EmptyPool(prog->progs_mempool);
	Mem_EmptyPool(prog->edictstring_mempool);

	prog->progs = (dprograms_t *)FS_LoadFile (filename, prog->progs_mempool, false);
	if (prog->progs == NULL)
		PRVM_ERROR ("PRVM_LoadProgs: couldn't load %s for %s", filename, PRVM_NAME);

	Con_DPrintf("%s programs occupy %iK.\n", PRVM_NAME, fs_filesize/1024);

	pr_crc = CRC_Block((qbyte *)prog->progs, fs_filesize);

// byte swap the header
	for (i = 0;i < (int) sizeof(*prog->progs) / 4;i++)
		((int *)prog->progs)[i] = LittleLong ( ((int *)prog->progs)[i] );

	if (prog->progs->version != PROG_VERSION)
		PRVM_ERROR ("%s: %s has wrong version number (%i should be %i)", PRVM_NAME, filename, prog->progs->version, PROG_VERSION);
	if (prog->progs->crc != prog->crc)
		PRVM_ERROR ("%s: %s system vars have been modified, progdefs.h is out of date", PRVM_NAME, filename);

	//pr_functions = (dfunction_t *)((qbyte *)progs + progs->ofs_functions);
	dfunctions = (dfunction_t *)((qbyte *)prog->progs + prog->progs->ofs_functions);
	prog->strings = (char *)prog->progs + prog->progs->ofs_strings;
	prog->globaldefs = (ddef_t *)((qbyte *)prog->progs + prog->progs->ofs_globaldefs);

	// we need to expand the fielddefs list to include all the engine fields,
	// so allocate a new place for it
	infielddefs = (ddef_t *)((qbyte *)prog->progs + prog->progs->ofs_fielddefs);
	//												( + DPFIELDS			   )
	prog->fielddefs = Mem_Alloc(prog->progs_mempool, prog->progs->numfielddefs * sizeof(ddef_t));

	prog->statements = (dstatement_t *)((qbyte *)prog->progs + prog->progs->ofs_statements);

	// moved edict_size calculation down below field adding code

	//pr_global_struct = (globalvars_t *)((qbyte *)progs + progs->ofs_globals);
	prog->globals = (float *)((qbyte *)prog->progs + prog->progs->ofs_globals);

// byte swap the lumps
	for (i=0 ; i<prog->progs->numstatements ; i++)
	{
		prog->statements[i].op = LittleShort(prog->statements[i].op);
		prog->statements[i].a = LittleShort(prog->statements[i].a);
		prog->statements[i].b = LittleShort(prog->statements[i].b);
		prog->statements[i].c = LittleShort(prog->statements[i].c);
	}

	prog->functions = Mem_Alloc(prog->progs_mempool, sizeof(mfunction_t) * prog->progs->numfunctions);
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

/*	// append the darkplaces fields
	for (i = 0;i < (int) DPFIELDS;i++)
	{
		pr_fielddefs[progs->numfielddefs].type = dpfields[i].type;
		pr_fielddefs[progs->numfielddefs].ofs = progs->entityfields;
		pr_fielddefs[progs->numfielddefs].s_name = PR_SetString(dpfields[i].string);
		if (pr_fielddefs[progs->numfielddefs].type == ev_vector)
			progs->entityfields += 3;
		else
			progs->entityfields++;
		progs->numfielddefs++;
	}*/

	// check required functions
	for(i=0 ; i < numrequiredfunc ; i++)
		if(PRVM_ED_FindFunction(required_func[i]) == 0)
			PRVM_ERROR("%s: %s not found in %s\n",PRVM_NAME, required_func[i], filename);

	for (i=0 ; i<prog->progs->numglobals ; i++)
		((int *)prog->globals)[i] = LittleLong (((int *)prog->globals)[i]);

	// moved edict_size calculation down here, below field adding code
	// LordHavoc: this no longer includes the edict_t header
	prog->edict_size = prog->progs->entityfields * 4;
	prog->edictareasize = prog->edict_size * MAX_EDICTS;

	// LordHavoc: bounds check anything static
	for (i = 0,st = prog->statements;i < prog->progs->numstatements;i++,st++)
	{
		switch (st->op)
		{
		case OP_IF:
		case OP_IFNOT:
			if ((unsigned short) st->a >= prog->progs->numglobals || st->b + i < 0 || st->b + i >= prog->progs->numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds IF/IFNOT (statement %d) in %s\n", i, PRVM_NAME);
			break;
		case OP_GOTO:
			if (st->a + i < 0 || st->a + i >= prog->progs->numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds GOTO (statement %d) in %s\n", i, PRVM_NAME);
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
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d)\n", i);
			break;
		// global none global
		case OP_NOT_F:
		case OP_NOT_V:
		case OP_NOT_S:
		case OP_NOT_FNC:
		case OP_NOT_ENT:
			if ((unsigned short) st->a >= prog->progs->numglobals || (unsigned short) st->c >= prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s\n", i, PRVM_NAME);
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
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d)\n in %s", i, PRVM_NAME);
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
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s\n", i, PRVM_NAME);
			break;
		default:
			PRVM_ERROR("PRVM_LoadProgs: unknown opcode %d at statement %d in %s\n", st->op, i, PRVM_NAME);
			break;
		}
	}

	PRVM_Init_Exec();

	prog->loaded = TRUE;
	
	// set flags & ddef_ts in prog
	
	prog->flag = 0;
	
	prog->self = PRVM_ED_FindGlobal("self");

	if(PRVM_ED_FindGlobal("time"))
		prog->time = &PRVM_G_FLOAT(PRVM_ED_FindGlobal("time")->ofs);

	if(PRVM_ED_FindField ("chain"))
		prog->flag |= PRVM_FE_CHAIN;

	if(PRVM_ED_FindField ("classname"))
		prog->flag |= PRVM_FE_CLASSNAME; 

	if(PRVM_ED_FindField ("nextthink") && PRVM_ED_FindField ("frame") && PRVM_ED_FindField ("think") 
		&& prog->flag && prog->self) 
		prog->flag |= PRVM_OP_STATE;
	
	PRVM_GCALL(reset_cmd)();
	PRVM_GCALL(init_cmd)();

	// init mempools
	PRVM_MEM_Alloc();
}


void PRVM_Fields_f (void)
{
	int i, j, ednum, used, usedamount;
	int *counts;
	char tempstring[5000], tempstring2[260], *name;
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

	counts = Mem_Alloc(tempmempool, prog->progs->numfielddefs * sizeof(int));
	for (ednum = 0;ednum < prog->max_edicts;ednum++)
	{
		ed = PRVM_EDICT_NUM(ednum);
		if (ed->p.e->free)
			continue;
		for (i = 1;i < prog->progs->numfielddefs;i++)
		{
			d = &prog->fielddefs[i];
			name = PRVM_GetString(d->s_name);
			if (name[strlen(name)-2] == '_')
				continue;	// skip _x, _y, _z vars
			v = (int *)((char *)ed->v + d->ofs*4);
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
			strcat(tempstring, "string   ");
			break;
		case ev_entity:
			strcat(tempstring, "entity   ");
			break;
		case ev_function:
			strcat(tempstring, "function ");
			break;
		case ev_field:
			strcat(tempstring, "field    ");
			break;
		case ev_void:
			strcat(tempstring, "void     ");
			break;
		case ev_float:
			strcat(tempstring, "float    ");
			break;
		case ev_vector:
			strcat(tempstring, "vector   ");
			break;
		case ev_pointer:
			strcat(tempstring, "pointer  ");
			break;
		default:
			sprintf (tempstring2, "bad type %i ", d->type & ~DEF_SAVEGLOBAL);
			strcat(tempstring, tempstring2);
			break;
		}
		if (strlen(name) > 256)
		{
			memcpy (tempstring2, name, 256);
			tempstring2[256] = tempstring2[257] = tempstring2[258] = '.';
			tempstring2[259] = 0;
			name = tempstring2;
		}
		strcat(tempstring, name);
		for (j = strlen(name);j < 25;j++)
			strcat(tempstring, " ");
		sprintf(tempstring2, "%5d", counts[i]);
		strcat(tempstring, tempstring2);
		strcat(tempstring, "\n");
		if (strlen(tempstring) >= 4096)
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
		Con_Printf( "%s: %s\n", Cmd_Argv(2), PRVM_ValueString( global->type, (prvm_eval_t *) &prog->globals[ global->ofs ] ) ); 
	PRVM_End;
}

/*
===============
PRVM_Init
===============
*/
void PRVM_Init (void)
{
	Cmd_AddCommand ("prvm_edict", PRVM_ED_PrintEdict_f);
	Cmd_AddCommand ("prvm_edicts", PRVM_ED_PrintEdicts_f);
	Cmd_AddCommand ("prvm_edictcount", PRVM_ED_Count_f);
	Cmd_AddCommand ("prvm_profile", PRVM_Profile_f);
	Cmd_AddCommand ("prvm_fields", PRVM_Fields_f);
	Cmd_AddCommand ("prvm_globals", PRVM_Globals_f);
	Cmd_AddCommand ("prvm_global", PRVM_Global_f);
	Cmd_AddCommand ("prvm_edictset", PRVM_ED_EdictSet_f);
	// LordHavoc: optional runtime bounds checking (speed drain, but worth it for security, on by default - breaks most QCCX features (used by CRMod and others))
	Cvar_RegisterVariable (&prvm_boundscheck);
	Cvar_RegisterVariable (&prvm_traceqc);

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
		Sys_Error("PRVM_InitProg: Invalid program number %i\n",prognr);

	prog = &prog_list[prognr];

	if(prog->loaded)
		PRVM_ResetProg();

	memset(prog, 0, sizeof(prvm_prog_t));

	prog->time = &prog->_time;
}

int PRVM_GetProgNr()
{
	return prog - prog_list;
}

// LordHavoc: turned PRVM_EDICT_NUM into a #define for speed reasons
prvm_edict_t *PRVM_EDICT_NUM_ERROR(int n, char *filename, int fileline)
{
	PRVM_ERROR ("PRVM_EDICT_NUM: %s: bad number %i (called at %s:%i)", PRVM_NAME, n, filename, fileline);
	return NULL;
}

void PRVM_ProcessError(void)
{
	if(prog)
		PRVM_GCALL(error_cmd)();
}

/*
int NUM_FOR_EDICT_ERROR(edict_t *e)
{
	Host_Error ("NUM_FOR_EDICT: bad pointer %p (world is %p, entity number would be %i)", e, sv.edicts, e - sv.edicts);
	return 0;
}

int NUM_FOR_EDICT(edict_t *e)
{
	int n;
	n = e - sv.edicts;
	if ((unsigned int)n >= MAX_EDICTS)
		Host_Error ("NUM_FOR_EDICT: bad pointer");
	return n;
}

//int NoCrash_NUM_FOR_EDICT(edict_t *e)
//{
//	return e - sv.edicts;
//}

//#define	EDICT_TO_PROG(e) ((qbyte *)(((edict_t *)e)->v) - (qbyte *)(sv.edictsfields))
//#define PROG_TO_EDICT(e) (sv.edicts + ((e) / (progs->entityfields * 4)))
int EDICT_TO_PROG(edict_t *e)
{
	int n;
	n = e - sv.edicts;
	if ((unsigned int)n >= (unsigned int)sv.max_edicts)
		Host_Error("EDICT_TO_PROG: invalid edict %8p (number %i compared to world at %8p)\n", e, n, sv.edicts);
	return n;// EXPERIMENTAL
	//return (qbyte *)e->v - (qbyte *)sv.edictsfields;
}
edict_t *PROG_TO_EDICT(int n)
{
	if ((unsigned int)n >= (unsigned int)sv.max_edicts)
		Host_Error("PROG_TO_EDICT: invalid edict number %i\n", n);
	return sv.edicts + n; // EXPERIMENTAL
	//return sv.edicts + ((n) / (progs->entityfields * 4));
}
*/

