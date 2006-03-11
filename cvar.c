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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t *cvar_vars = NULL;
cvar_t *cvar_hashtable[65536];
char *cvar_null_string = "";

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (const char *var_name)
{
	int hashindex;
	cvar_t *var;

	// use hash lookup to minimize search time
	hashindex = CRC_Block((const unsigned char *)var_name, strlen(var_name));
	for (var = cvar_hashtable[hashindex];var;var = var->nextonhashchain)
		if (!strcasecmp (var_name, var->name))
			return var;

	return NULL;
}

cvar_t *Cvar_FindVarAfter (const char *prev_var_name, int neededflags)
{
	cvar_t *var;

	if (*prev_var_name)
	{
		var = Cvar_FindVar (prev_var_name);
		if (!var)
			return NULL;
		var = var->next;
	}
	else
		var = cvar_vars;

	// search for the next cvar matching the needed flags
	while (var)
	{
		if ((var->flags & neededflags) || !neededflags)
			break;
		var = var->next;
	}
	return var;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
const char *Cvar_VariableString (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}

/*
============
Cvar_VariableDefString
============
*/
const char *Cvar_VariableDefString (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->defstring;
}


/*
============
Cvar_CompleteVariable
============
*/
const char *Cvar_CompleteVariable (const char *partial)
{
	cvar_t		*cvar;
	size_t		len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strncasecmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}


/*
	CVar_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com

*/
int Cvar_CompleteCountPossible (const char *partial)
{
	cvar_t	*cvar;
	size_t	len;
	int		h;

	h = 0;
	len = strlen(partial);

	if (!len)
		return	0;

	// Loop through the cvars and count all possible matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			h++;

	return h;
}

/*
	CVar_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
const char **Cvar_CompleteBuildList (const char *partial)
{
	const cvar_t *cvar;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			buf[bpos++] = cvar->name;

	buf[bpos] = NULL;
	return buf;
}

// written by LordHavoc
void Cvar_CompleteCvarPrint (const char *partial)
{
	cvar_t *cvar;
	size_t len = strlen(partial);
	// Loop through the command list and print all matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			Con_Printf("%s : \"%s\" (\"%s\") : %s\n", cvar->name, cvar->string, cvar->defstring, cvar->description);
}


/*
============
Cvar_Set
============
*/
void Cvar_SetQuick_Internal (cvar_t *var, const char *value)
{
	qboolean changed;

	changed = strcmp(var->string, value);
	// LordHavoc: don't reallocate when there is no change
	if (!changed)
		return;

	// LordHavoc: don't reallocate when the buffer is the same size
	if (!var->string || strlen(var->string) != strlen(value))
	{
		Z_Free (var->string);	// free the old value string

		var->string = (char *)Z_Malloc (strlen(value)+1);
	}
	strcpy (var->string, value);
	var->value = atof (var->string);
	var->integer = (int) var->value;
	if ((var->flags & CVAR_NOTIFY) && changed && sv.active)
		SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", var->name, var->string);
#if 0
	// TODO: add infostring support to the server?
	if ((var->flags & CVAR_SERVERINFO) && changed && sv.active)
	{
		InfoString_SetValue(svs.serverinfo, sizeof(svs.serverinfo), var->name, var->string);
		if (sv.active)
		{
			MSG_WriteByte (&sv.reliable_datagram, svc_serverinfostring);
			MSG_WriteString (&sv.reliable_datagram, var->name);
			MSG_WriteString (&sv.reliable_datagram, var->string);
		}
	}
#endif
	if ((var->flags & CVAR_USERINFO) && changed && cls.state != ca_dedicated)
	{
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), var->name, var->string);
		if (cls.state == ca_connected)
			Cmd_ForwardStringToServer(va("setinfo \"%s\" \"%s\"\n", var->name, var->string));
	}
}

void Cvar_SetQuick (cvar_t *var, const char *value)
{
	if (var == NULL)
	{
		Con_Print("Cvar_SetQuick: var == NULL\n");
		return;
	}

	if (developer.integer)
		Con_Printf("Cvar_SetQuick({\"%s\", \"%s\", %i, \"%s\"}, \"%s\");\n", var->name, var->string, var->flags, var->defstring, value);

	Cvar_SetQuick_Internal(var, value);
}

void Cvar_Set (const char *var_name, const char *value)
{
	cvar_t *var;
	var = Cvar_FindVar (var_name);
	if (var == NULL)
	{
		Con_Printf("Cvar_Set: variable %s not found\n", var_name);
		return;
	}
	Cvar_SetQuick(var, value);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValueQuick(cvar_t *var, float value)
{
	char val[MAX_INPUTLINE];

	if ((float)((int)value) == value)
		sprintf(val, "%i", (int)value);
	else
		sprintf(val, "%f", value);
	Cvar_SetQuick(var, val);
}

void Cvar_SetValue(const char *var_name, float value)
{
	char val[MAX_INPUTLINE];

	if ((float)((int)value) == value)
		sprintf(val, "%i", (int)value);
	else
		sprintf(val, "%f", value);
	Cvar_Set(var_name, val);
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *variable)
{
	int hashindex;
	cvar_t *current, *next, *cvar;
	char *oldstr;

	if (developer.integer)
		Con_Printf("Cvar_RegisterVariable({\"%s\", \"%s\", %i});\n", variable->name, variable->string, variable->flags);

// first check to see if it has already been defined
	cvar = Cvar_FindVar (variable->name);
	if (cvar)
	{
		if (cvar->flags & CVAR_ALLOCATED)
		{
			if (developer.integer)
				Con_Printf("...  replacing existing allocated cvar {\"%s\", \"%s\", %i}\n", cvar->name, cvar->string, cvar->flags);
			// fixed variables replace allocated ones
			// (because the engine directly accesses fixed variables)
			// NOTE: this isn't actually used currently
			// (all cvars are registered before config parsing)
			variable->flags |= (cvar->flags & ~CVAR_ALLOCATED);
			// cvar->string is now owned by variable instead
			variable->string = cvar->string;
			variable->defstring = cvar->defstring;
			variable->value = atof (variable->string);
			variable->integer = (int) variable->value;
			// replace cvar with this one...
			variable->next = cvar->next;
			if (cvar_vars == cvar)
			{
				// head of the list is easy to change
				cvar_vars = variable;
			}
			else
			{
				// otherwise find it somewhere in the list
				for (current = cvar_vars;current->next != cvar;current = current->next)
					;
				current->next = variable;
			}

			// get rid of old allocated cvar
			// (but not cvar->string and cvar->defstring, because we kept those)
			Z_Free(cvar->name);
			Z_Free(cvar);
		}
		else
			Con_Printf("Can't register variable %s, already defined\n", variable->name);
		return;
	}

// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = (char *)Z_Malloc (strlen(variable->string)+1);
	strcpy (variable->string, oldstr);
	variable->defstring = (char *)Z_Malloc (strlen(variable->string)+1);
	strcpy (variable->defstring, oldstr);
	variable->value = atof (variable->string);
	variable->integer = (int) variable->value;

// link the variable in
// alphanumerical order
	for( current = NULL, next = cvar_vars ; next && strcmp( next->name, variable->name ) < 0 ; current = next, next = next->next )
		;
	if( current ) {
		current->next = variable;
	} else {
		cvar_vars = variable;
	}
	variable->next = next;

	// link to head of list in this hash table index
	hashindex = CRC_Block((const unsigned char *)variable->name, strlen(variable->name));
	variable->nextonhashchain = cvar_hashtable[hashindex];
	cvar_hashtable[hashindex] = variable;
}

/*
============
Cvar_Get

Adds a newly allocated variable to the variable list or sets its value.
============
*/
cvar_t *Cvar_Get (const char *name, const char *value, int flags)
{
	int hashindex;
	cvar_t *current, *next, *cvar;

	if (developer.integer)
		Con_Printf("Cvar_Get(\"%s\", \"%s\", %i);\n", name, value, flags);

// first check to see if it has already been defined
	cvar = Cvar_FindVar (name);
	if (cvar)
	{
		cvar->flags |= flags;
		Cvar_SetQuick_Internal (cvar, value);
		// also set the default value (but only once)
		if (~cvar->flags & CVAR_DEFAULTSET)
		{
			cvar->flags |= CVAR_DEFAULTSET;

			Z_Free(cvar->defstring);
			cvar->defstring = (char *)Z_Malloc(strlen(value) + 1);
			strcpy(cvar->defstring, value);
		}
		return cvar;
	}

// check for overlap with a command
	if (Cmd_Exists (name))
	{
		Con_Printf("Cvar_Get: %s is a command\n", name);
		return NULL;
	}

// allocate a new cvar, cvar name, and cvar string
// FIXME: these never get Z_Free'd
	cvar = (cvar_t *)Z_Malloc(sizeof(cvar_t));
	cvar->flags = flags | CVAR_ALLOCATED | CVAR_DEFAULTSET;
	cvar->name = (char *)Z_Malloc(strlen(name)+1);
	strcpy(cvar->name, name);
	cvar->string = (char *)Z_Malloc(strlen(value)+1);
	strcpy(cvar->string, value);
	cvar->defstring = (char *)Z_Malloc(strlen(value)+1);
	strcpy(cvar->defstring, value);
	cvar->value = atof (cvar->string);
	cvar->integer = (int) cvar->value;
	cvar->description = "custom cvar";

// link the variable in
// alphanumerical order
	for( current = NULL, next = cvar_vars ; next && strcmp( next->name, cvar->name ) < 0 ; current = next, next = next->next )
		;
	if( current )
		current->next = cvar;
	else
		cvar_vars = cvar;
	cvar->next = next;

	// link to head of list in this hash table index
	hashindex = CRC_Block((const unsigned char *)cvar->name, strlen(cvar->name));
	cvar->nextonhashchain = cvar_hashtable[hashindex];
	cvar_hashtable[hashindex] = cvar;

	return cvar;
}


/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean	Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;

// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"%s\" is \"%s\" [\"%s\"]\n", v->name, v->string, v->defstring);
		return true;
	}

	Con_DPrint("Cvar_Command: ");

	if (v->flags & CVAR_READONLY)
	{
		Con_Printf("%s is read-only\n", v->name);
		return true;
	}
	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (qfile_t *f)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
		if (var->flags & CVAR_SAVE)
			FS_Printf(f, "%s%s \"%s\"\n", var->flags & CVAR_ALLOCATED ? "seta " : "", var->name, var->string);
}


// Added by EvilTypeGuy eviltypeguy@qeradiant.com
// 2000-01-09 CvarList command By Matthias "Maddes" Buecher, http://www.inside3d.com/qip/
/*
=========
Cvar_List
=========
*/
void Cvar_List_f (void)
{
	cvar_t *cvar;
	const char *partial;
	size_t len;
	int count;

	if (Cmd_Argc() > 1)
	{
		partial = Cmd_Argv (1);
		len = strlen(partial);
	}
	else
	{
		partial = NULL;
		len = 0;
	}

	count = 0;
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (partial && strncasecmp (partial,cvar->name,len))
			continue;

		Con_Printf("%s is \"%s\" [\"%s\"] %s\n", cvar->name, cvar->string, cvar->defstring, cvar->description);
		count++;
	}

	if (partial)
		Con_Printf("%i cvar(s) beginning with \"%s\"\n", count, partial);
	else
		Con_Printf("%i cvar(s)\n", count);
}
// 2000-01-09 CvarList command by Maddes

void Cvar_Set_f (void)
{
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc() < 3)
	{
		Con_Printf("Set: wrong number of parameters, usage: set <variablename> <value>\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(Cmd_Argv(1));
	if (cvar && cvar->flags & CVAR_READONLY)
	{
		Con_Printf("Set: %s is read-only\n", cvar->name);
		return;
	}

	Con_DPrint("Set: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(Cmd_Argv(1), Cmd_Argv(2), 0);
}

void Cvar_SetA_f (void)
{
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc() < 3)
	{
		Con_Printf("SetA: wrong number of parameters, usage: seta <variablename> <value>\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(Cmd_Argv(1));
	if (cvar && cvar->flags & CVAR_READONLY)
	{
		Con_Printf("SetA: %s is read-only\n", cvar->name);
		return;
	}

	Con_DPrint("SetA: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(Cmd_Argv(1), Cmd_Argv(2), CVAR_SAVE);
}


