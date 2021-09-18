/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2000-2021 DarkPlaces contributors

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

const char *cvar_dummy_description = "custom cvar";
static const char *cvar_null_string = "";

cvar_state_t cvars_all;
cvar_state_t cvars_null;

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar(cvar_state_t *cvars, const char *var_name, int neededflags)
{
	int hashindex;
	cvar_hash_t *hash;

	// use hash lookup to minimize search time
	hashindex = CRC_Block((const unsigned char *)var_name, strlen(var_name)) % CVAR_HASHSIZE;
	for (hash = cvars->hashtable[hashindex];hash;hash = hash->next)
		if (!strcmp (var_name, hash->cvar->name) && (hash->cvar->flags & neededflags))
			return hash->cvar;
		else
			for (char **alias = hash->cvar->aliases; alias && *alias; alias++)
				if (!strcmp (var_name, *alias) && (hash->cvar->flags & neededflags))
					return hash->cvar;
	return NULL;
}

cvar_t *Cvar_FindVarAfter(cvar_state_t *cvars, const char *prev_var_name, int neededflags)
{
	cvar_t *var;

	if (*prev_var_name)
	{
		var = Cvar_FindVar(cvars, prev_var_name, neededflags);
		if (!var)
			return NULL;
		var = var->next;
	}
	else
		var = cvars->vars;

	// search for the next cvar matching the needed flags
	while (var)
	{
		if (var->flags & neededflags)
			break;
		var = var->next;
	}
	return var;
}

static cvar_t *Cvar_FindVarLink(cvar_state_t *cvars, const char *var_name, cvar_t **parent, cvar_t ***link, cvar_t **prev_alpha, int neededflags)
{
	int hashindex;
	cvar_hash_t *hash;

	// use hash lookup to minimize search time
	hashindex = CRC_Block((const unsigned char *)var_name, strlen(var_name)) % CVAR_HASHSIZE;
	if(parent) *parent = NULL;
	if(prev_alpha) *prev_alpha = NULL;
	if(link) *link = &cvars->hashtable[hashindex]->cvar;
	for (hash = cvars->hashtable[hashindex];hash;hash = hash->next)
	{
		if (!strcmp (var_name, hash->cvar->name) && (hash->cvar->flags & neededflags))
			goto match;
		else
			for (char **alias = hash->cvar->aliases; alias && *alias; alias++)
				if (!strcmp (var_name, *alias) && (hash->cvar->flags & neededflags))
					goto match;
		if(parent) *parent = hash->cvar;
	}
	return NULL;
match:
	if(!prev_alpha || hash->cvar == cvars->vars)
		return hash->cvar;

	*prev_alpha = cvars->vars;
	// if prev_alpha happens to become NULL then there has been some inconsistency elsewhere
	// already - should I still insert '*prev_alpha &&' in the loop?
	while((*prev_alpha)->next != hash->cvar)
		*prev_alpha = (*prev_alpha)->next;
	return hash->cvar;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValueOr(cvar_state_t *cvars, const char *var_name, float def, int neededflags)
{
	cvar_t *var;

	var = Cvar_FindVar(cvars, var_name, neededflags);
	if (!var)
		return def;
	return atof (var->string);
}

float Cvar_VariableValue(cvar_state_t *cvars, const char *var_name, int neededflags)
{
	return Cvar_VariableValueOr(cvars, var_name, 0, neededflags);
}

/*
============
Cvar_VariableString
============
*/
const char *Cvar_VariableStringOr(cvar_state_t *cvars, const char *var_name, const char *def, int neededflags)
{
	cvar_t *var;

	var = Cvar_FindVar(cvars, var_name, neededflags);
	if (!var)
		return def;
	return var->string;
}

const char *Cvar_VariableString(cvar_state_t *cvars, const char *var_name, int neededflags)
{
	return Cvar_VariableStringOr(cvars, var_name, cvar_null_string, neededflags);
}

/*
============
Cvar_VariableDefString
============
*/
const char *Cvar_VariableDefString(cvar_state_t *cvars, const char *var_name, int neededflags)
{
	cvar_t *var;

	var = Cvar_FindVar(cvars, var_name, neededflags);
	if (!var)
		return cvar_null_string;
	return var->defstring;
}

/*
============
Cvar_VariableDescription
============
*/
const char *Cvar_VariableDescription(cvar_state_t *cvars, const char *var_name, int neededflags)
{
	cvar_t *var;

	var = Cvar_FindVar(cvars, var_name, neededflags);
	if (!var)
		return cvar_null_string;
	return var->description;
}


/*
============
Cvar_CompleteVariable
============
*/
const char *Cvar_CompleteVariable(cvar_state_t *cvars, const char *partial, int neededflags)
{
	cvar_t		*cvar;
	size_t		len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (cvar=cvars->vars ; cvar ; cvar=cvar->next)
		if (!strncasecmp (partial,cvar->name, len) && (cvar->flags & neededflags))
			return cvar->name;

	return NULL;
}


/*
	CVar_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com

*/
int Cvar_CompleteCountPossible(cvar_state_t *cvars, const char *partial, int neededflags)
{
	cvar_t	*cvar;
	size_t	len;
	int		h;

	h = 0;
	len = strlen(partial);

	if (!len)
		return	0;

	// Loop through the cvars and count all possible matches
	for (cvar = cvars->vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len) && (cvar->flags & neededflags))
			h++;
		else
			for (char **alias = cvar->aliases; alias && *alias; alias++)
				if (!strncasecmp(partial, *alias, len) && (cvar->flags & neededflags))
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
const char **Cvar_CompleteBuildList(cvar_state_t *cvars, const char *partial, int neededflags)
{
	const cvar_t *cvar;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cvar_CompleteCountPossible(cvars, partial, neededflags) + 1) * sizeof(const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)Mem_Alloc(tempmempool, sizeofbuf + sizeof(const char *));
	// Loop through the alias list and print all matches
	for (cvar = cvars->vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len) && (cvar->flags & neededflags))
			buf[bpos++] = cvar->name;
		else
			for (char **alias = cvar->aliases; alias && *alias; alias++)
				if (!strncasecmp(partial, *alias, len) && (cvar->flags & neededflags))
					buf[bpos++] = *alias;
		

	buf[bpos] = NULL;
	return buf;
}

void Cvar_PrintHelp(cvar_t *cvar, const char *name, qbool full)
{
	// Aliases are purple, cvars are yellow
	if (strcmp(cvar->name, name))
		Con_Printf("^6");
	else
		Con_Printf("^3");
	Con_Printf("%s^7 is \"%s\" [\"%s\"]", name, ((cvar->flags & CF_PRIVATE) ? "********"/*hunter2*/ : cvar->string), cvar->defstring);
	if (strcmp(cvar->name, name))
		Con_Printf(" (also ^3%s^7)", cvar->name);
	if (full)
		Con_Printf(" %s", cvar->description);
	Con_Printf("\n");
}

// written by LadyHavoc
void Cvar_CompleteCvarPrint(cvar_state_t *cvars, const char *partial, int neededflags)
{
	cvar_t *cvar;
	size_t len = strlen(partial);
	// Loop through the command list and print all matches
	for (cvar = cvars->vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len) && (cvar->flags & neededflags))
			Cvar_PrintHelp(cvar, cvar->name, true);
		else
			for (char **alias = cvar->aliases; alias && *alias; alias++)
				if (!strncasecmp (partial, *alias, len) && (cvar->flags & neededflags))
					Cvar_PrintHelp(cvar, *alias, true);

		
}

// check if a cvar is held by some progs
static qbool Cvar_IsAutoCvar(cvar_t *var)
{
	int i;
	prvm_prog_t *prog;
	for (i = 0;i < PRVM_PROG_MAX;i++)
	{
		prog = &prvm_prog_list[i];
		if (prog->loaded && var->globaldefindex[i] >= 0)
			return true;
	}
	return false;
}

// we assume that prog is already set to the target progs
static void Cvar_UpdateAutoCvar(cvar_t *var)
{
	int i;
	int j;
	const char *s;
	vec3_t v;
	prvm_prog_t *prog;
	for (i = 0;i < PRVM_PROG_MAX;i++)
	{
		prog = &prvm_prog_list[i];
		if (prog->loaded && var->globaldefindex[i] >= 0)
		{
			// MUST BE SYNCED WITH prvm_edict.c PRVM_LoadProgs
			switch(prog->globaldefs[var->globaldefindex[i]].type & ~DEF_SAVEGLOBAL)
			{
			case ev_float:
				PRVM_GLOBALFIELDFLOAT(prog->globaldefs[var->globaldefindex[i]].ofs) = var->value;
				break;
			case ev_vector:
				s = var->string;
				VectorClear(v);
				for (j = 0;j < 3;j++)
				{
					while (*s && ISWHITESPACE(*s))
						s++;
					if (!*s)
						break;
					v[j] = atof(s);
					while (!ISWHITESPACE(*s))
						s++;
					if (!*s)
						break;
				}
				VectorCopy(v, PRVM_GLOBALFIELDVECTOR(prog->globaldefs[var->globaldefindex[i]].ofs));
				break;
			case ev_string:
				PRVM_ChangeEngineString(prog, var->globaldefindex_stringno[i], var->string);
				PRVM_GLOBALFIELDSTRING(prog->globaldefs[var->globaldefindex[i]].ofs) = var->globaldefindex_stringno[i];
				break;
			}
		}
	}
}

// called after loading a savegame
void Cvar_UpdateAllAutoCvars(cvar_state_t *cvars)
{
	cvar_t *var;
	for (var = cvars->vars ; var ; var = var->next)
		Cvar_UpdateAutoCvar(var);
}

void Cvar_Callback(cvar_t *var)
{
	if (var == NULL)
	{
		Con_Print("Cvar_Callback: var == NULL\n");
		return;
	}

	if(var->callback)
		var->callback(var);
}

/*
============
Cvar_Set
============
*/
extern cvar_t sv_disablenotify;
static void Cvar_SetQuick_Internal (cvar_t *var, const char *value)
{
	qbool changed;
	size_t valuelen;

	changed = strcmp(var->string, value) != 0;
	// LadyHavoc: don't reallocate when there is no change
	if (!changed)
		return;

	// LadyHavoc: don't reallocate when the buffer is the same size
	valuelen = strlen(value);
	if (!var->string || strlen(var->string) != valuelen)
	{
		Z_Free ((char *)var->string);	// free the old value string

		var->string = (char *)Z_Malloc (valuelen + 1);
	}
	memcpy ((char *)var->string, value, valuelen + 1);
	var->value = atof (var->string);
	var->integer = (int) var->value;
	if ((var->flags & CF_NOTIFY) && sv.active && !sv_disablenotify.integer)
		SV_BroadcastPrintf("\003^3Server cvar \"%s\" changed to \"%s\"\n", var->name, var->string);
#if 0
	// TODO: add infostring support to the server?
	if ((var->flags & CF_SERVERINFO) && changed && sv.active)
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
	if (var->flags & CF_USERINFO)
		CL_SetInfo(var->name, var->string, true, false, false, false);

	Cvar_UpdateAutoCvar(var);

	// Call the function stored in the cvar for bounds checking, cleanup, etc
	Cvar_Callback(var);
}

void Cvar_SetQuick (cvar_t *var, const char *value)
{
	if (var == NULL)
	{
		Con_Print("Cvar_SetQuick: var == NULL\n");
		return;
	}

	if (developer_extra.integer)
		Con_DPrintf("Cvar_SetQuick({\"%s\", \"%s\", %i, \"%s\"}, \"%s\");\n", var->name, var->string, var->flags, var->defstring, value);

	Cvar_SetQuick_Internal(var, value);
}

void Cvar_Set(cvar_state_t *cvars, const char *var_name, const char *value)
{
	cvar_t *var;
	var = Cvar_FindVar(cvars, var_name, ~0);
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

void Cvar_SetValue(cvar_state_t *cvars, const char *var_name, float value)
{
	char val[MAX_INPUTLINE];

	if ((float)((int)value) == value)
		dpsnprintf(val, sizeof(val), "%i", (int)value);
	else
		dpsnprintf(val, sizeof(val), "%f", value);
	Cvar_Set(cvars, var_name, val);
}

void Cvar_RegisterCallback(cvar_t *variable, void (*callback)(cvar_t *))
{
	if (variable == NULL)
	{
		Con_Print("Cvar_RegisterCallback: var == NULL\n");
		return;
	}
	variable->callback = callback;
}

void Cvar_RegisterVirtual(cvar_t *variable, const char *name )
{
	cvar_state_t *cvars = &cvars_all;
	cvar_hash_t *hash;
	int hashindex;

	if(!*name)
	{
		Con_Printf("Cvar_RegisterVirtual: invalid virtual cvar name\n");
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists(cmd_local, name))
	{
		Con_Printf("Cvar_RegisterVirtual: %s is a command\n", name);
		return;
	}

	if(Cvar_FindVar(&cvars_all, name, 0))
	{
		Con_Printf("Cvar_RegisterVirtual: %s is a cvar\n", name);
		return;
	}

	// Resize the variable->aliases list to have room for another entry and a null terminator.
	// This zero-pads when resizing, so we don't need to write the NULL terminator manually here.
	// Also if aliases is NULL this allocates fresh for the correct size, so it's fine to just do this.
	variable->aliases = (char **)Z_Realloc(variable->aliases, sizeof(char *) * (variable->aliases_size + 2));
	// Add the new alias, and increment the number of aliases in the list
	variable->aliases[variable->aliases_size++] = (char *)Z_strdup(name);

	// link to head of list in this hash table index
	hash = (cvar_hash_t *)Z_Malloc(sizeof(cvar_hash_t));
	hashindex = CRC_Block((const unsigned char *)name, strlen(name)) % CVAR_HASHSIZE;
	hash->next = cvars->hashtable[hashindex];
	cvars->hashtable[hashindex] = hash;
	hash->cvar = variable;
}

/*
============
Cvar_Link

Links a variable to the variable list and hashtable
============
*/
static void Cvar_Link(cvar_t *variable, cvar_state_t *cvars)
{
	cvar_t *current, *next;
	cvar_hash_t *hash;
	int hashindex;
	/*
	 * Link the variable in
	 * alphanumerical order
	 */
	for( current = NULL, next = cvars->vars ; next && strcmp( next->name, variable->name ) < 0 ; current = next, next = next->next )
		;
	if(current)
		current->next = variable;
	else
		cvars->vars = variable;
	variable->next = next;

	// link to head of list in this hash table index
	hash = (cvar_hash_t *)Z_Malloc(sizeof(cvar_hash_t));
	hashindex = CRC_Block((const unsigned char *)variable->name, strlen(variable->name)) % CVAR_HASHSIZE;
	hash->next = cvars->hashtable[hashindex];
	hash->cvar = variable;
	cvars->hashtable[hashindex] = hash;
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *variable)
{
	cvar_state_t *cvars = NULL;
	cvar_t *current, *cvar;
	int i;

	switch (variable->flags & (CF_CLIENT | CF_SERVER))
	{
	case CF_CLIENT:
	case CF_SERVER:
	case CF_CLIENT | CF_SERVER:
		cvars = &cvars_all;
		break;
	case 0:
		Sys_Error("Cvar_RegisterVariable({\"%s\", \"%s\", %i}) with no CF_CLIENT | CF_SERVER flags\n", variable->name, variable->string, variable->flags);
		break;
	default:
		Sys_Error("Cvar_RegisterVariable({\"%s\", \"%s\", %i}) with weird CF_CLIENT | CF_SERVER flags\n", variable->name, variable->string, variable->flags);
		break;
	}

	if (developer_extra.integer)
		Con_DPrintf("Cvar_RegisterVariable({\"%s\", \"%s\", %i});\n", variable->name, variable->string, variable->flags);

	// first check to see if it has already been defined
	cvar = Cvar_FindVar(cvars, variable->name, ~0);
	if (cvar)
	{
		if (cvar->flags & CF_ALLOCATED)
		{
			if (developer_extra.integer)
				Con_DPrintf("...  replacing existing allocated cvar {\"%s\", \"%s\", %i}\n", cvar->name, cvar->string, cvar->flags);
			// fixed variables replace allocated ones
			// (because the engine directly accesses fixed variables)
			// NOTE: this isn't actually used currently
			// (all cvars are registered before config parsing)
			variable->flags |= (cvar->flags & ~CF_ALLOCATED);
			// cvar->string is now owned by variable instead
			variable->string = cvar->string;
			variable->defstring = cvar->defstring;
			variable->value = atof (variable->string);
			variable->integer = (int) variable->value;
			// Preserve autocvar status.
			memcpy(variable->globaldefindex, cvar->globaldefindex, sizeof(variable->globaldefindex));
			memcpy(variable->globaldefindex_stringno, cvar->globaldefindex_stringno, sizeof(variable->globaldefindex_stringno));
			// replace cvar with this one...
			variable->next = cvar->next;
			if (cvars->vars == cvar)
			{
				// head of the list is easy to change
				cvars->vars = variable;
			}
			else
			{
				// otherwise find it somewhere in the list
				for (current = cvars->vars;current->next != cvar;current = current->next)
					;
				current->next = variable;
			}

			// get rid of old allocated cvar
			// (but not cvar->string and cvar->defstring, because we kept those)
			Z_Free((char *)cvar->name);
			Z_Free(cvar);
		}
		else
			Con_DPrintf("Can't register variable %s, already defined\n", variable->name);
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists(cmd_local, variable->name))
	{
		Con_Printf("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

	// copy the value off, because future sets will Z_Free it
	variable->name = (char *)Mem_strdup(zonemempool, variable->name);
	variable->string = (char *)Mem_strdup(zonemempool, variable->string);
	variable->defstring = (char *)Mem_strdup(zonemempool, variable->string);
	variable->value = atof (variable->string);
	variable->integer = (int) variable->value;
	variable->aliases = NULL;
	variable->aliases_size = 0;
	variable->initstate = NULL;

	// Mark it as not an autocvar.
	for (i = 0;i < PRVM_PROG_MAX;i++)
		variable->globaldefindex[i] = -1;

	Cvar_Link(variable, cvars);
}

/*
============
Cvar_Get

Adds a newly allocated variable to the variable list or sets its value.
============
*/
cvar_t *Cvar_Get(cvar_state_t *cvars, const char *name, const char *value, int flags, const char *newdescription)
{
	cvar_t *cvar;
	int i;

	if (developer_extra.integer)
		Con_DPrintf("Cvar_Get(\"%s\", \"%s\", %i);\n", name, value, flags);

	// first check to see if it has already been defined
	cvar = Cvar_FindVar(cvars, name, ~0);
	if (cvar)
	{
		cvar->flags |= flags;
		Cvar_SetQuick_Internal (cvar, value);
		if(newdescription && (cvar->flags & CF_ALLOCATED))
		{
			if(cvar->description != cvar_dummy_description)
				Z_Free((char *)cvar->description);

			if(*newdescription)
				cvar->description = (char *)Mem_strdup(zonemempool, newdescription);
			else
				cvar->description = cvar_dummy_description;
		}
		return cvar;
	}

	// check for pure evil
	if (!*name)
	{
		Con_Printf("Cvar_Get: invalid variable name\n");
		return NULL;
	}

	// check for overlap with a command
	if (Cmd_Exists(cmd_local, name))
	{
		Con_Printf("Cvar_Get: %s is a command\n", name);
		return NULL;
	}

	// allocate a new cvar, cvar name, and cvar string
	// TODO: factorize the following code with the one at the end of Cvar_RegisterVariable()
	// FIXME: these never get Z_Free'd
	cvar = (cvar_t *)Z_Malloc(sizeof(cvar_t));
	cvar->flags = flags | CF_ALLOCATED;
	cvar->name = (char *)Mem_strdup(zonemempool, name);
	cvar->string = (char *)Mem_strdup(zonemempool, value);
	cvar->defstring = (char *)Mem_strdup(zonemempool, value);
	cvar->value = atof (cvar->string);
	cvar->integer = (int) cvar->value;
	cvar->aliases = NULL;
	cvar->aliases_size = 0;
	cvar->initstate = NULL;

	if(newdescription && *newdescription)
		cvar->description = (char *)Mem_strdup(zonemempool, newdescription);
	else
		cvar->description = cvar_dummy_description; // actually checked by VM_cvar_type

	// Mark it as not an autocvar.
	for (i = 0;i < PRVM_PROG_MAX;i++)
		cvar->globaldefindex[i] = -1;

	Cvar_Link(cvar, cvars);

	return cvar;
}

qbool Cvar_Readonly (cvar_t *var, const char *cmd_name)
{
	if (var->flags & CF_READONLY)
	{
		if(cmd_name)
			Con_Printf("%s: ",cmd_name);
		Con_Printf("%s", var->name);
		Con_Printf(" is read-only\n");
		return true;
	}
	return false;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qbool	Cvar_Command (cmd_state_t *cmd)
{
	cvar_state_t	*cvars = cmd->cvars;
	cvar_t			*v;

	// check variables
	v = Cvar_FindVar(cvars, Cmd_Argv(cmd, 0), (cmd->cvars_flagsmask));
	if (!v)
		return false;

	// perform a variable print or set
	if (Cmd_Argc(cmd) == 1)
	{
		Cvar_PrintHelp(v, Cmd_Argv(cmd, 0), true);
		return true;
	}

	if (developer_extra.integer)
		Con_DPrint("Cvar_Command: ");

	if(Cvar_Readonly(v, NULL))
		return true;

	Cvar_SetQuick(v, Cmd_Argv(cmd, 1));
	if (developer_extra.integer)
		Con_DPrint("\n");
	return true;
}

void Cvar_UnlockDefaults(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *var;
	// unlock the default values of all cvars
	for (var = cvars->vars ; var ; var = var->next)
		var->flags &= ~CF_DEFAULTSET;
}

void Cvar_LockDefaults_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *var;
	// lock in the default values of all cvars
	for (var = cvars->vars ; var ; var = var->next)
	{
		if (!(var->flags & CF_DEFAULTSET))
		{
			size_t alloclen;

			//Con_Printf("locking cvar %s (%s -> %s)\n", var->name, var->string, var->defstring);
			var->flags |= CF_DEFAULTSET;
			Z_Free((char *)var->defstring);
			alloclen = strlen(var->string) + 1;
			var->defstring = (char *)Z_Malloc(alloclen);
			memcpy((char *)var->defstring, var->string, alloclen);
		}
	}
}

void Cvar_SaveInitState(cvar_state_t *cvars)
{
	cvar_t *c;
	for (c = cvars->vars;c;c = c->next)
	{
		c->initstate = (cvar_t *)Z_Malloc(sizeof(cvar_t));
		memcpy(c->initstate, c, sizeof(cvar_t));
	}
}

void Cvar_RestoreInitState(cvar_state_t *cvars)
{
	int hashindex;
	cvar_t *c, **cp;
	cvar_t *c2, **cp2;
	for (cp = &cvars->vars;(c = *cp);)
	{
		if (c->initstate)
		{
			// restore this cvar, it existed at init
			if (((c->flags ^ c->initstate->flags) & CF_MAXFLAGSVAL)
			 || strcmp(c->defstring ? c->defstring : "", c->initstate->defstring ? c->initstate->defstring : "")
			 || strcmp(c->string ? c->string : "", c->initstate->string ? c->initstate->string : ""))
			{
				Con_DPrintf("Cvar_RestoreInitState: Restoring cvar \"%s\"\n", c->name);
				if (c->defstring)
					Z_Free((char *)c->defstring);
				c->defstring = Mem_strdup(zonemempool, c->initstate->defstring);
				if (c->string)
					Z_Free((char *)c->string);
				c->string = Mem_strdup(zonemempool, c->initstate->string);
			}
			c->flags = c->initstate->flags;
			c->value = c->initstate->value;
			c->integer = c->initstate->integer;
			VectorCopy(c->initstate->vector, c->vector);
			cp = &c->next;
		}
		else
		{
			if (!(c->flags & CF_ALLOCATED))
			{
				Con_DPrintf("Cvar_RestoreInitState: Unable to destroy cvar \"%s\", it was registered after init!\n", c->name);
				// In this case, at least reset it to the default.
				if((c->flags & CF_PERSISTENT) == 0)
					Cvar_SetQuick(c, c->defstring);
				cp = &c->next;
				continue;
			}
			if (Cvar_IsAutoCvar(c))
			{
				Con_DPrintf("Cvar_RestoreInitState: Unable to destroy cvar \"%s\", it is an autocvar used by running progs!\n", c->name);
				// In this case, at least reset it to the default.
				if((c->flags & CF_PERSISTENT) == 0)
					Cvar_SetQuick(c, c->defstring);
				cp = &c->next;
				continue;
			}
			// remove this cvar, it did not exist at init
			Con_DPrintf("Cvar_RestoreInitState: Destroying cvar \"%s\"\n", c->name);
			// unlink struct from hash
			hashindex = CRC_Block((const unsigned char *)c->name, strlen(c->name)) % CVAR_HASHSIZE;
			for (cp2 = &cvars->hashtable[hashindex]->cvar;(c2 = *cp2);)
			{
				if (c2 == c)
				{
					*cp2 = cvars->hashtable[hashindex]->next->cvar;
					break;
				}
				else
					cp2 = &cvars->hashtable[hashindex]->next->cvar;
			}
			// unlink struct from main list
			*cp = c->next;
			// free strings
			if (c->defstring)
				Z_Free((char *)c->defstring);
			if (c->string)
				Z_Free((char *)c->string);
			if (c->description && c->description != cvar_dummy_description)
				Z_Free((char *)c->description);
			// free struct
			Z_Free(c);
		}
	}
}

void Cvar_ResetToDefaults_All_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *var;
	// restore the default values of all cvars
	for (var = cvars->vars ; var ; var = var->next)
	{
		if((var->flags & CF_PERSISTENT) == 0)
			Cvar_SetQuick(var, var->defstring);
	}
}

void Cvar_ResetToDefaults_NoSaveOnly_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *var;
	// restore the default values of all cvars
	for (var = cvars->vars ; var ; var = var->next)
	{
		if ((var->flags & (CF_PERSISTENT | CF_ARCHIVE)) == 0)
			Cvar_SetQuick(var, var->defstring);
	}
}


void Cvar_ResetToDefaults_SaveOnly_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *var;
	// restore the default values of all cvars
	for (var = cvars->vars ; var ; var = var->next)
	{
		if ((var->flags & (CF_PERSISTENT | CF_ARCHIVE)) == CF_ARCHIVE)
			Cvar_SetQuick(var, var->defstring);
	}
}

/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (cvar_state_t *cvars, qfile_t *f)
{
	cvar_t	*var;
	char buf1[MAX_INPUTLINE], buf2[MAX_INPUTLINE];

	// don't save cvars that match their default value
	for (var = cvars->vars ; var ; var = var->next) {
		if ((var->flags & CF_ARCHIVE) && (strcmp(var->string, var->defstring) || ((var->flags & CF_ALLOCATED) && !(var->flags & CF_DEFAULTSET))))
		{
			Cmd_QuoteString(buf1, sizeof(buf1), var->name, "\"\\$", false);
			Cmd_QuoteString(buf2, sizeof(buf2), var->string, "\"\\$", false);
			FS_Printf(f, "%s\"%s\" \"%s\"\n", var->flags & CF_ALLOCATED ? "seta " : "", buf1, buf2);
		}
	}
}

// Added by EvilTypeGuy eviltypeguy@qeradiant.com
// 2000-01-09 CvarList command By Matthias "Maddes" Buecher, http://www.inside3d.com/qip/
/*
=========
Cvar_List
=========
*/
void Cvar_List_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *cvar;
	const char *partial;
	int count;
	qbool ispattern;
	char vabuf[1024];

	if (Cmd_Argc(cmd) > 1)
	{
		partial = Cmd_Argv(cmd, 1);
		ispattern = (strchr(partial, '*') || strchr(partial, '?'));
		if(!ispattern)
			partial = va(vabuf, sizeof(vabuf), "%s*", partial);
	}
	else
	{
		partial = va(vabuf, sizeof(vabuf), "*");
		ispattern = false;
	}

	count = 0;
	for (cvar = cvars->vars; cvar; cvar = cvar->next)
	{
		if (matchpattern_with_separator(cvar->name, partial, false, "", false))
		{
			Cvar_PrintHelp(cvar, cvar->name, true);
			count++;
		}
		for (char **alias = cvar->aliases; alias && *alias; alias++)
		{
			if (matchpattern_with_separator(*alias, partial, false, "", false))
			{
				Cvar_PrintHelp(cvar, *alias, true);
				count++;
			}
		}
	}

	if (Cmd_Argc(cmd) > 1)
	{
		if(ispattern)
			Con_Printf("%i cvar%s matching \"%s\"\n", count, (count > 1) ? "s" : "", partial);
		else
			Con_Printf("%i cvar%s beginning with \"%s\"\n", count, (count > 1) ? "s" : "", Cmd_Argv(cmd,1));
	}
	else
		Con_Printf("%i cvar(s)\n", count);
}
// 2000-01-09 CvarList command by Maddes

void Cvar_Set_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc(cmd) < 3)
	{
		Con_Printf("Set: wrong number of parameters, usage: set <variablename> <value> [<description>]\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(cvars, Cmd_Argv(cmd, 1), ~0);
	if (cvar)
		if(Cvar_Readonly(cvar,"Set"))
			return;

	if (developer_extra.integer)
		Con_DPrint("Set: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(cvars, Cmd_Argv(cmd, 1), Cmd_Argv(cmd, 2), cmd->cvars_flagsmask, Cmd_Argc(cmd) > 3 ? Cmd_Argv(cmd, 3) : NULL);
}

void Cvar_SetA_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc(cmd) < 3)
	{
		Con_Printf("SetA: wrong number of parameters, usage: seta <variablename> <value> [<description>]\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(cvars, Cmd_Argv(cmd, 1), ~0);
	if (cvar)
		if(Cvar_Readonly(cvar,"SetA"))
			return;

	if (developer_extra.integer)
		Con_DPrint("SetA: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(cvars, Cmd_Argv(cmd, 1), Cmd_Argv(cmd, 2), cmd->cvars_flagsmask | CF_ARCHIVE, Cmd_Argc(cmd) > 3 ? Cmd_Argv(cmd, 3) : NULL);
}

void Cvar_Del_f(cmd_state_t *cmd)
{
	cvar_state_t *cvars = cmd->cvars;
	int neededflags = ~0;
	int i;
	cvar_t *parent, **link;
	cvar_t *cvar, *prev;

	if(Cmd_Argc(cmd) < 2)
	{
		Con_Printf("%s: wrong number of parameters, usage: unset <variablename1> [<variablename2> ...]\n", Cmd_Argv(cmd, 0));
		return;
	}
	for(i = 1; i < Cmd_Argc(cmd); ++i)
	{
		cvar = Cvar_FindVarLink(cvars, Cmd_Argv(cmd, i), &parent, &link, &prev, neededflags);

		if(!cvar)
		{
			Con_Printf("%s: %s is not defined\n", Cmd_Argv(cmd, 0), Cmd_Argv(cmd, i));
			continue;
		}
		if(Cvar_Readonly(cvar, Cmd_Argv(cmd, 0)))
			continue;
		if(!(cvar->flags & CF_ALLOCATED))
		{
			Con_Printf("%s: %s is static and cannot be deleted\n", Cmd_Argv(cmd, 0), cvar->name);
			continue;
		}
		if(cvar == cvars->vars)
		{
			cvars->vars = cvar->next;
		}
		else
		{
			// in this case, prev must be set, otherwise there has been some inconsistensy
			// elsewhere already... should I still check for prev != NULL?
			prev->next = cvar->next;
		}

		if(parent)
			parent->next = cvar->next;
		else if(link)
			*link = cvar->next;
		if(cvar->description != cvar_dummy_description)
			Z_Free((char *)cvar->description);

		Z_Free((char *)cvar->name);
		Z_Free((char *)cvar->string);
		Z_Free((char *)cvar->defstring);
		Z_Free(cvar);
	}
}

#ifdef FILLALLCVARSWITHRUBBISH
void Cvar_FillAll_f(cmd_state_t *cmd)
{
	char *buf, *p, *q;
	int n, i;
	cvar_t *var;
	qbool verify;
	if(Cmd_Argc(cmd) != 2)
	{
		Con_Printf("Usage: %s length to plant rubbish\n", Cmd_Argv(cmd, 0));
		Con_Printf("Usage: %s -length to verify that the rubbish is still there\n", Cmd_Argv(cmd, 0));
		return;
	}
	n = atoi(Cmd_Argv(cmd, 1));
	verify = (n < 0);
	if(verify)
		n = -n;
	buf = Z_Malloc(n + 1);
	buf[n] = 0;
	for(var = cvars->vars; var; var = var->next)
	{
		for(i = 0, p = buf, q = var->name; i < n; ++i)
		{
			*p++ = *q++;
			if(!*q)
				q = var->name;
		}
		if(verify && strcmp(var->string, buf))
		{
			Con_Printf("\n%s does not contain the right rubbish, either this is the first run or a possible overrun was detected, or something changed it intentionally; it DOES contain: %s\n", var->name, var->string);
		}
		Cvar_SetQuick(var, buf);
	}
	Z_Free(buf);
}
#endif /* FILLALLCVARSWITHRUBBISH */
