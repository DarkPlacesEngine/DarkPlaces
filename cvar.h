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

typedef struct
{
	int value;
	char *name;
}
cvaroption_t;

typedef struct
{
	int				type;
	float			valuemin, valuemax, valuestep;
	int				numoptions;
	cvaroption_t	optionlist[MAX_CVAROPTIONS];
}
menucvar_t;

typedef struct cvar_s
{
	int				flags;
	char			*name;
	char			*string;
	int				integer;
	float			value;
	float			vector[3];
	menucvar_t		menuinfo;
	struct cvar_s	*next;
} cvar_t;

void	Cvar_MenuSlider(cvar_t *variable, int menu, float slider_min, float slider_max, float slider_step);
void	Cvar_MenuBool(cvar_t *variable, int menu, char *name_false, char *name_true);
void	Cvar_MenuFloat(cvar_t *variable, int menu, float range_min, float range_max);
void	Cvar_MenuInteger(cvar_t *variable, int menu, int range_min, int range_max);
void	Cvar_MenuString(cvar_t *variable, int menu);
void	Cvar_MenuOption(cvar_t *variable, int menu, int value[16], char *name[16]);

void	Cvar_RegisterVariable (cvar_t *variable);
// registers a cvar that already has the name, string, and optionally the
// archive elements set.

void	Cvar_Set (char *var_name, char *value);
// equivelant to "<name> <variable>" typed at the console

void	Cvar_SetValue (char *var_name, float value);
// expands value to a string and calls Cvar_Set

void	Cvar_SetQuick (cvar_t *var, char *value);
void	Cvar_SetValueQuick (cvar_t *var, float value);

float	Cvar_VariableValue (char *var_name);
// returns 0 if not defined or non numeric

char	*Cvar_VariableString (char *var_name);
// returns an empty string if not defined

char 	*Cvar_CompleteVariable (char *partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

qboolean Cvar_Command (void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void 	Cvar_WriteVariables (QFile *f);
// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.

cvar_t *Cvar_FindVar (char *var_name);

extern cvar_t	*cvar_vars;

int		Cvar_CompleteCountPossible (char *partial);
char	**Cvar_CompleteBuildList (char *partial);
// Added by EvilTypeGuy - functions for tab completion system
// Thanks to Fett erich@heintz.com
// Thanks to taniwha

void	Cvar_List_f (void);
// Prints a list of Cvars including a count of them to the user console
// Referenced in cmd.c in Cmd_Init hence it's inclusion here
// Added by EvilTypeGuy eviltypeguy@qeradiant.com
// Thanks to Matthias "Maddes" Buecher, http://www.inside3d.com/qip/

#endif

