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
// cvar.h

struct cmd_state_s;
typedef struct cmd_state_s cmd_state_t;

/*

cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
in C code.

it is sufficient to initialize a cvar_t with just the first two fields, or
you can add a ,true flag for variables that you want saved to the configuration
file when the game is quit:

cvar_t	r_draworder = {"r_draworder","1"};
cvar_t	scr_screensize = {"screensize","1",true};

Cvars must be registered before use, or they will have a 0 value instead of the float interpretation of the string.  Generally, all cvar_t declarations should be registered in the apropriate init function before any console commands are executed:
Cvar_RegisterVariable (&host_framerate);


C code usually just references a cvar in place:
if ( r_draworder.value )

It could optionally ask for the value to be looked up for a string name:
if (Cvar_VariableValue ("r_draworder"))

Interpreted prog code can access cvars with the cvar(name) or
cvar_set (name, value) internal functions:
teamplay = cvar("teamplay");
cvar_set ("registered", "1");

The user can access cvars from the console in two ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

#ifndef CVAR_H
#define CVAR_H

// cvar flags

#define CVAR_SAVE 1
#define CVAR_NOTIFY 2
#define CVAR_READONLY 4
#define CVAR_SERVERINFO 8
#define CVAR_USERINFO 16
// CVAR_PRIVATE means do not $ expand or sendcvar this cvar under any circumstances (rcon_password uses this)
#define CVAR_PRIVATE 32
// for engine-owned cvars that must not be reset on gametype switch (e.g. scr_screenshot_name, which otherwise isn't set to the mod name properly)
#define CVAR_NORESETTODEFAULTS 64
// cvar is accessible in client
#define CVAR_CLIENT 128
// cvar is accessible in dedicated server
#define CVAR_SERVER 256
// used to determine if flags is valid
#define CVAR_MAXFLAGSVAL 511
// for internal use only!
#define CVAR_DEFAULTSET (1<<30)
#define CVAR_ALLOCATED (1<<31)

/*
// type of a cvar for menu purposes
#define CVARMENUTYPE_FLOAT 1
#define CVARMENUTYPE_INTEGER 2
#define CVARMENUTYPE_SLIDER 3
#define CVARMENUTYPE_BOOL 4
#define CVARMENUTYPE_STRING 5
#define CVARMENUTYPE_OPTION 6

// which menu to put a cvar in
#define CVARMENU_GRAPHICS 1
#define CVARMENU_SOUND 2
#define CVARMENU_INPUT 3
#define CVARMENU_NETWORK 4
#define CVARMENU_SERVER 5

#define MAX_CVAROPTIONS 16

typedef struct cvaroption_s
{
	int value;
	const char *name;
}
cvaroption_t;

typedef struct menucvar_s
{
	int type;
	float valuemin, valuemax, valuestep;
	int numoptions;
	cvaroption_t optionlist[MAX_CVAROPTIONS];
}
menucvar_t;
*/

typedef struct cvar_s
{
	int flags;

	const char *name;

	const char *string;
	const char *description;
	int integer;
	float value;
	float vector[3];

	const char *defstring;

	void (*callback)(char *value);
	qboolean ignore_callback;

	char **aliases;
	int aliasindex;

	// values at init (for Cvar_RestoreInitState)
	qboolean initstate; // indicates this existed at init
	int initflags;
	const char *initstring;
	const char *initdescription;
	int initinteger;
	float initvalue;
	float initvector[3];
	const char *initdefstring;

	int globaldefindex[3];
	int globaldefindex_stringno[3];

	//menucvar_t menuinfo;
	struct cvar_s *next;
} cvar_t;

typedef struct cvar_hash_s
{
	cvar_t *cvar;
	struct cvar_hash_s *next;
} cvar_hash_t;

typedef struct cvar_state_s
{
	cvar_t *vars;
	cvar_hash_t *hashtable[CVAR_HASHSIZE];
}
cvar_state_t;

extern cvar_state_t cvars_all;
extern cvar_state_t cvars_null; // used by cmd_serverfromclient which intentionally has no cvars available

/*
void Cvar_MenuSlider(cvar_t *variable, int menu, float slider_min, float slider_max, float slider_step);
void Cvar_MenuBool(cvar_t *variable, int menu, const char *name_false, const char *name_true);
void Cvar_MenuFloat(cvar_t *variable, int menu, float range_min, float range_max);
void Cvar_MenuInteger(cvar_t *variable, int menu, int range_min, int range_max);
void Cvar_MenuString(cvar_t *variable, int menu);
void Cvar_MenuOption(cvar_t *variable, int menu, int value[16], const char *name[16]);
*/


void Cvar_RegisterAlias(cvar_t *variable, const char *alias );

void Cvar_RegisterCallback(cvar_t *variable, void (*callback)(char *));

/// registers a cvar that already has the name, string, and optionally the
/// archive elements set.
void Cvar_RegisterVariable(cvar_t *variable);

qboolean Cvar_Readonly (cvar_t *var, const char *cmd_name);

/// equivelant to "<name> <variable>" typed at the console
void Cvar_Set (cvar_state_t *cvars, const char *var_name, const char *value);
void Cvar_Set_NoCallback (cvar_t *var, const char *value);

/// expands value to a string and calls Cvar_Set
void Cvar_SetValue (cvar_state_t *cvars, const char *var_name, float value);

void Cvar_SetQuick (cvar_t *var, const char *value);
void Cvar_SetValueQuick (cvar_t *var, float value);

float Cvar_VariableValueOr (cvar_state_t *cvars, const char *var_name, float def, int neededflags);
// returns def if not defined

float Cvar_VariableValue (cvar_state_t *cvars, const char *var_name, int neededflags);
// returns 0 if not defined or non numeric

const char *Cvar_VariableStringOr (cvar_state_t *cvars, const char *var_name, const char *def, int neededflags);
// returns def if not defined

const char *Cvar_VariableString (cvar_state_t *cvars, const char *var_name, int neededflags);
// returns an empty string if not defined

const char *Cvar_VariableDefString (cvar_state_t *cvars, const char *var_name, int neededflags);
// returns an empty string if not defined

const char *Cvar_VariableDescription (cvar_state_t *cvars, const char *var_name, int neededflags);
// returns an empty string if not defined

const char *Cvar_CompleteVariable (cvar_state_t *cvars, const char *partial, int neededflags);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

void Cvar_PrintHelp(cvar_t *cvar, const char *name, qboolean full);

void Cvar_CompleteCvarPrint (cvar_state_t *cvars, const char *partial, int neededflags);

qboolean Cvar_Command (cmd_state_t *cmd);
// called by Cmd_ExecuteString when Cmd_Argv(cmd, 0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void Cvar_SaveInitState(cvar_state_t *cvars);
void Cvar_RestoreInitState(cvar_state_t *cvars);

void Cvar_UnlockDefaults(cmd_state_t *cmd);
void Cvar_LockDefaults_f(cmd_state_t *cmd);
void Cvar_ResetToDefaults_All_f(cmd_state_t *cmd);
void Cvar_ResetToDefaults_NoSaveOnly_f(cmd_state_t *cmd);
void Cvar_ResetToDefaults_SaveOnly_f(cmd_state_t *cmd);

void Cvar_WriteVariables (cvar_state_t *cvars, qfile_t *f);
// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.

cvar_t *Cvar_FindVar(cvar_state_t *cvars, const char *var_name, int neededflags);
cvar_t *Cvar_FindVarAfter(cvar_state_t *cvars, const char *prev_var_name, int neededflags);

int Cvar_CompleteCountPossible(cvar_state_t *cvars, const char *partial, int neededflags);
const char **Cvar_CompleteBuildList(cvar_state_t *cvars, const char *partial, int neededflags);
// Added by EvilTypeGuy - functions for tab completion system
// Thanks to Fett erich@heintz.com
// Thanks to taniwha

/// Prints a list of Cvars including a count of them to the user console
/// Referenced in cmd.c in Cmd_Init hence it's inclusion here.
/// Added by EvilTypeGuy eviltypeguy@qeradiant.com
/// Thanks to Matthias "Maddes" Buecher, http://www.inside3d.com/qip/
void Cvar_List_f(cmd_state_t *cmd);

void Cvar_Set_f(cmd_state_t *cmd);
void Cvar_SetA_f(cmd_state_t *cmd);
void Cvar_Del_f(cmd_state_t *cmd);
// commands to create new cvars (or set existing ones)
// seta creates an archived cvar (saved to config)

/// allocates a cvar by name and returns its address,
/// or merely sets its value if it already exists.
cvar_t *Cvar_Get(cvar_state_t *cvars, const char *name, const char *value, int flags, const char *newdescription);

extern const char *cvar_dummy_description; // ALWAYS the same pointer

void Cvar_UpdateAllAutoCvars(cvar_state_t *cvars); // updates ALL autocvars of the active prog to the cvar values (savegame loading)

#ifdef FILLALLCVARSWITHRUBBISH
void Cvar_FillAll_f(cmd_state_t *cmd);
#endif /* FILLALLCVARSWITHRUBBISH */

#endif

