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
char *cvar_null_string = "";

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (const char *var_name)
{
	cvar_t *var;

	for (var = cvar_vars;var;var = var->next)
		if (!strcmp (var_name, var->name))
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
Cvar_CompleteVariable
============
*/
const char *Cvar_CompleteVariable (const char *partial)
{
	cvar_t		*cvar;
	int			len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strncmp (partial,cvar->name, len))
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
	int		len;
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
	int len = 0;
	int bpos = 0;
	int sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			buf[bpos++] = cvar->name;

	buf[bpos] = NULL;
	return buf;
}

/*
============
Cvar_Set
============
*/
void Cvar_SetQuick (cvar_t *var, const char *value)
{
	qboolean changed;

	if (var == NULL)
	{
		Con_Printf("Cvar_SetQuick: var == NULL\n");
		return;
	}

	changed = strcmp(var->string, value);
	// LordHavoc: don't reallocate when there is no change
	if (!changed)
		return;

	// LordHavoc: don't reallocate when the buffer is the same size
	if (!var->string || strlen(var->string) != strlen(value))
	{
		Z_Free (var->string);	// free the old value string

		var->string = Z_Malloc (strlen(value)+1);
	}
	strcpy (var->string, value);
	var->value = atof (var->string);
	var->integer = (int) var->value;
	if ((var->flags & CVAR_NOTIFY) && changed)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}
}

void Cvar_Set (const char *var_name, const char *value)
{
	cvar_t *var;
	var = Cvar_FindVar (var_name);
	if (var == NULL)
	{
		// there is an error in C code if this happens
		Con_Printf ("Cvar_Set: variable %s not found\n", var_name);
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
	char val[256];

	if ((float)((int)value) == value)
		sprintf(val, "%i", (int)value);
	else
		sprintf(val, "%f", value);
	Cvar_SetQuick(var, val);
}

void Cvar_SetValue(const char *var_name, float value)
{
	char val[256];

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
	char	*oldstr;

// first check to see if it has already been defined
	if (Cvar_FindVar (variable->name))
	{
		Con_Printf ("Can't register variable %s, already defined\n", variable->name);
		return;
	}

// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = Z_Malloc (strlen(variable->string)+1);
	strcpy (variable->string, oldstr);
	variable->value = atof (variable->string);
	variable->integer = (int) variable->value;

// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
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
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
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
			FS_Printf (f, "%s \"%s\"\n", var->name, var->string);
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
	int len, count;

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
		if (partial && strncmp (partial,cvar->name,len))
			continue;

		Con_Printf ("%s is \"%s\"\n", cvar->name, cvar->string);
		count++;
	}

	Con_Printf ("%i cvar(s)", count);
	if (partial)
		Con_Printf (" beginning with \"%s\"", partial);
	Con_Printf ("\n");
}
// 2000-01-09 CvarList command by Maddes

