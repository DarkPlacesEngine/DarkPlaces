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

char *cvar_dummy_description = "custom cvar";

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
Cvar_VariableDescription
============
*/
const char *Cvar_VariableDescription (const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->description;
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
			Con_Printf ("%c3%s%s : \"%s\" (\"%s\") : %s\n", STRING_COLOR_TAG, cvar->name, STRING_COLOR_DEFAULT_STR, cvar->string, cvar->defstring, cvar->description);
}


/*
============
Cvar_Set
============
*/
void Cvar_SetQuick_Internal (cvar_t *var, const char *value)
{
	qboolean changed;
	size_t valuelen;

	changed = strcmp(var->string, value);
	// LordHavoc: don't reallocate when there is no change
	if (!changed)
		return;

	// LordHavoc: don't reallocate when the buffer is the same size
	valuelen = strlen(value);
	if (!var->string || strlen(var->string) != valuelen)
	{
		Z_Free (var->string);	// free the old value string

		var->string = (char *)Z_Malloc (valuelen + 1);
	}
	memcpy (var->string, value, valuelen + 1);
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
	if ((var->flags & CVAR_USERINFO) && cls.state != ca_dedicated)
		CL_SetInfo(var->name, var->string, true, false, false, false);
	else if ((var->flags & CVAR_NQUSERINFOHACK) && cls.state != ca_dedicated)
	{
		// update the cls.userinfo to have proper values for the
		// silly nq config variables.
		//
		// this is done when these variables are changed rather than at
		// connect time because if the user or code checks the userinfo and it
		// holds weird values it may cause confusion...
		if (!strcmp(var->name, "_cl_color"))
		{
			int top = (var->integer >> 4) & 15, bottom = var->integer & 15;
			CL_SetInfo("topcolor", va("%i", top), true, false, false, false);
			CL_SetInfo("bottomcolor", va("%i", bottom), true, false, false, false);
			if (cls.protocol != PROTOCOL_QUAKEWORLD && cls.netcon)
			{
				MSG_WriteByte(&cls.netcon->message, clc_stringcmd);
				MSG_WriteString(&cls.netcon->message, va("color %i %i", top, bottom));
			}
		}
		else if (!strcmp(var->name, "_cl_rate"))
			CL_SetInfo("rate", va("%i", var->integer), true, false, false, false);
		else if (!strcmp(var->name, "_cl_playerskin"))
			CL_SetInfo("playerskin", var->string, true, false, false, false);
		else if (!strcmp(var->name, "_cl_playermodel"))
			CL_SetInfo("playermodel", var->string, true, false, false, false);
		else if (!strcmp(var->name, "_cl_name"))
			CL_SetInfo("name", var->string, true, false, false, false);
		else if (!strcmp(var->name, "rcon_secure"))
		{
			// whenever rcon_secure is changed, clear rcon_password for
			// security reasons (prevents a send-rcon-password-as-plaintext
			// attack based on NQ protocol session takeover and svc_stufftext)
			Cvar_Set("rcon_password", "");
		}
	}
}

void Cvar_SetQuick (cvar_t *var, const char *value)
{
	if (var == NULL)
	{
		Con_Print("Cvar_SetQuick: var == NULL\n");
		return;
	}

	if (developer.integer >= 100)
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
		dpsnprintf(val, sizeof(val), "%i", (int)value);
	else
		dpsnprintf(val, sizeof(val), "%f", value);
	Cvar_SetQuick(var, val);
}

void Cvar_SetValue(const char *var_name, float value)
{
	char val[MAX_INPUTLINE];

	if ((float)((int)value) == value)
		dpsnprintf(val, sizeof(val), "%i", (int)value);
	else
		dpsnprintf(val, sizeof(val), "%f", value);
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
	size_t alloclen;

	if (developer.integer >= 100)
		Con_Printf("Cvar_RegisterVariable({\"%s\", \"%s\", %i});\n", variable->name, variable->string, variable->flags);

// first check to see if it has already been defined
	cvar = Cvar_FindVar (variable->name);
	if (cvar)
	{
		if (cvar->flags & CVAR_ALLOCATED)
		{
			if (developer.integer >= 100)
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
	alloclen = strlen(variable->string) + 1;
	variable->string = (char *)Z_Malloc (alloclen);
	memcpy (variable->string, oldstr, alloclen);
	variable->defstring = (char *)Z_Malloc (alloclen);
	memcpy (variable->defstring, oldstr, alloclen);
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
cvar_t *Cvar_Get (const char *name, const char *value, int flags, const char *newdescription)
{
	int hashindex;
	cvar_t *current, *next, *cvar;
	size_t alloclen;

	if (developer.integer >= 100)
		Con_Printf("Cvar_Get(\"%s\", \"%s\", %i);\n", name, value, flags);

// first check to see if it has already been defined
	cvar = Cvar_FindVar (name);
	if (cvar)
	{
		cvar->flags |= flags;
		Cvar_SetQuick_Internal (cvar, value);
		if(newdescription && (cvar->flags & CVAR_ALLOCATED))
		{
			if(cvar->description != cvar_dummy_description)
				Z_Free(cvar->description);

			if(*newdescription)
			{
				alloclen = strlen(newdescription) + 1;
				cvar->description = (char *)Z_Malloc(alloclen);
				memcpy(cvar->description, newdescription, alloclen);
			}
			else
				cvar->description = cvar_dummy_description;
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
// TODO: factorize the following code with the one at the end of Cvar_RegisterVariable()
// FIXME: these never get Z_Free'd
	cvar = (cvar_t *)Z_Malloc(sizeof(cvar_t));
	cvar->flags = flags | CVAR_ALLOCATED;
	alloclen = strlen(name) + 1;
	cvar->name = (char *)Z_Malloc(alloclen);
	memcpy(cvar->name, name, alloclen);
	alloclen = strlen(value) + 1;
	cvar->string = (char *)Z_Malloc(alloclen);
	memcpy(cvar->string, value, alloclen);
	cvar->defstring = (char *)Z_Malloc(alloclen);
	memcpy(cvar->defstring, value, alloclen);
	cvar->value = atof (cvar->string);
	cvar->integer = (int) cvar->value;

	if(newdescription && *newdescription)
	{
		alloclen = strlen(newdescription) + 1;
		cvar->description = (char *)Z_Malloc(alloclen);
		memcpy(cvar->description, newdescription, alloclen);
	}
	else
		cvar->description = cvar_dummy_description; // actually checked by VM_cvar_type

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

	if (developer.integer >= 100)
		Con_DPrint("Cvar_Command: ");

	if (v->flags & CVAR_READONLY)
	{
		Con_Printf("%s is read-only\n", v->name);
		return true;
	}
	Cvar_Set (v->name, Cmd_Argv(1));
	if (developer.integer >= 100)
		Con_DPrint("\n");
	return true;
}


void Cvar_UnlockDefaults (void)
{
	cvar_t *var;
	// unlock the default values of all cvars
	for (var = cvar_vars ; var ; var = var->next)
		var->flags &= ~CVAR_DEFAULTSET;
}


void Cvar_LockDefaults_f (void)
{
	cvar_t *var;
	// lock in the default values of all cvars
	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!(var->flags & CVAR_DEFAULTSET))
		{
			size_t alloclen;

			//Con_Printf("locking cvar %s (%s -> %s)\n", var->name, var->string, var->defstring);
			var->flags |= CVAR_DEFAULTSET;
			Z_Free(var->defstring);
			alloclen = strlen(var->string) + 1;
			var->defstring = (char *)Z_Malloc(alloclen);
			memcpy(var->defstring, var->string, alloclen);
		}
	}
}


void Cvar_ResetToDefaults_All_f (void)
{
	cvar_t *var;
	// restore the default values of all cvars
	for (var = cvar_vars ; var ; var = var->next)
		Cvar_SetQuick(var, var->defstring);
}


void Cvar_ResetToDefaults_NoSaveOnly_f (void)
{
	cvar_t *var;
	// restore the default values of all cvars
	for (var = cvar_vars ; var ; var = var->next)
		if (!(var->flags & CVAR_SAVE))
			Cvar_SetQuick(var, var->defstring);
}


void Cvar_ResetToDefaults_SaveOnly_f (void)
{
	cvar_t *var;
	// restore the default values of all cvars
	for (var = cvar_vars ; var ; var = var->next)
		if (var->flags & CVAR_SAVE)
			Cvar_SetQuick(var, var->defstring);
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

	// don't save cvars that match their default value
	for (var = cvar_vars ; var ; var = var->next)
		if ((var->flags & CVAR_SAVE) && (strcmp(var->string, var->defstring) || (var->flags & CVAR_ALLOCATED)))
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
	qboolean ispattern;

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

	ispattern = partial && (strchr(partial, '*') || strchr(partial, '?'));

	count = 0;
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (len && (ispattern ? !matchpattern_with_separator(cvar->name, partial, false, "", false) : strncmp (partial,cvar->name,len)))
			continue;

		Con_Printf("%s is \"%s\" [\"%s\"] %s\n", cvar->name, cvar->string, cvar->defstring, cvar->description);
		count++;
	}

	if (len)
	{
		if(ispattern)
			Con_Printf("%i cvar(s) matching \"%s\"\n", count, partial);
		else
			Con_Printf("%i cvar(s) beginning with \"%s\"\n", count, partial);
	}
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
		Con_Printf("Set: wrong number of parameters, usage: set <variablename> <value> [<description>]\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(Cmd_Argv(1));
	if (cvar && cvar->flags & CVAR_READONLY)
	{
		Con_Printf("Set: %s is read-only\n", cvar->name);
		return;
	}

	if (developer.integer >= 100)
		Con_DPrint("Set: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(Cmd_Argv(1), Cmd_Argv(2), 0, Cmd_Argc() > 3 ? Cmd_Argv(3) : NULL);
}

void Cvar_SetA_f (void)
{
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc() < 3)
	{
		Con_Printf("SetA: wrong number of parameters, usage: seta <variablename> <value> [<description>]\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(Cmd_Argv(1));
	if (cvar && cvar->flags & CVAR_READONLY)
	{
		Con_Printf("SetA: %s is read-only\n", cvar->name);
		return;
	}

	if (developer.integer >= 100)
		Con_DPrint("SetA: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(Cmd_Argv(1), Cmd_Argv(2), CVAR_SAVE, Cmd_Argc() > 3 ? Cmd_Argv(3) : NULL);
}


