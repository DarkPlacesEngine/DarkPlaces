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
// cmd.c -- Quake script command processing module

#include "quakedef.h"

#define	MAX_ALIAS_NAME	32
// this is the largest script file that can be executed in one step
// LordHavoc: inreased this from 8192 to 32768
#define CMDBUFSIZE 32768
// maximum number of parameters to a command
#define	MAX_ARGS 80
// maximum tokenizable commandline length (counting NUL terminations)
#define CMD_TOKENIZELENGTH (MAX_INPUTLINE + 80)

typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

static cmdalias_t *cmd_alias;

static qboolean cmd_wait;

static mempool_t *cmd_mempool;

static char cmd_tokenizebuffer[CMD_TOKENIZELENGTH];
static int cmd_tokenizebufferpos = 0;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
static void Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

static sizebuf_t	cmd_text;
static unsigned char		cmd_text_buf[CMDBUFSIZE];

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (const char *text)
{
	int		l;

	l = (int)strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Print("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, (const unsigned char *)text, (int)strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (const char *text)
{
	char	*temp;
	int		templen;

	// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen)
	{
		temp = (char *)Mem_Alloc (tempmempool, templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
		temp = NULL;

	// add the entire text of the file
	Cbuf_AddText (text);

	// add the copied off data
	if (temp != NULL)
	{
		SZ_Write (&cmd_text, (const unsigned char *)temp, templen);
		Mem_Free (temp);
	}
}

/*
============
Cbuf_Execute
============
*/
static void Cmd_PreprocessString( const char *intext, char *outtext, unsigned maxoutlen, cmdalias_t *alias );
void Cbuf_Execute (void)
{
	int i;
	char *text;
	char line[MAX_INPUTLINE];
	char preprocessed[MAX_INPUTLINE];
	int quotes;

	// LordHavoc: making sure the tokenizebuffer doesn't get filled up by repeated crashes
	cmd_tokenizebufferpos = 0;

	while (cmd_text.cursize)
	{
// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes ^= 1;
			if ( !quotes &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\r' || text[i] == '\n')
				break;
		}

		memcpy (line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memcpy (cmd_text.data, text+i, cmd_text.cursize);
		}

// execute the command line
		Cmd_PreprocessString( line, preprocessed, sizeof(preprocessed), NULL );
		Cmd_ExecuteString (preprocessed, src_command);

		if (cmd_wait)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
qboolean host_stuffcmdsrun = false;
void Cmd_StuffCmds_f (void)
{
	int		i, j, l;
	// this is per command, and bounds checked (no buffer overflows)
	char	build[MAX_INPUTLINE];

	if (Cmd_Argc () != 1)
	{
		Con_Print("stuffcmds : execute command line parameters\n");
		return;
	}

	host_stuffcmdsrun = true;
	for (i = 0;i < com_argc;i++)
	{
		if (com_argv[i] && com_argv[i][0] == '+' && (com_argv[i][1] < '0' || com_argv[i][1] > '9'))
		{
			l = 0;
			j = 1;
			while (com_argv[i][j])
				build[l++] = com_argv[i][j++];
			i++;
			for (;i < com_argc;i++)
			{
				if (!com_argv[i])
					continue;
				if ((com_argv[i][0] == '+' || com_argv[i][0] == '-') && (com_argv[i][1] < '0' || com_argv[i][1] > '9'))
					break;
				if (l + strlen(com_argv[i]) + 5 > sizeof(build))
					break;
				build[l++] = ' ';
				build[l++] = '\"';
				for (j = 0;com_argv[i][j];j++)
					build[l++] = com_argv[i][j];
				build[l++] = '\"';
			}
			build[l++] = '\n';
			build[l++] = 0;
			Cbuf_InsertText (build);
			i--;
		}
	}
}


/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f (void)
{
	char *f;

	if (Cmd_Argc () != 2)
	{
		Con_Print("exec <filename> : execute a script file\n");
		return;
	}

	f = (char *)FS_LoadFile (Cmd_Argv(1), tempmempool, false, NULL);
	if (!f)
	{
		Con_Printf("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}
	Con_DPrintf("execing %s\n",Cmd_Argv(1));

	// if executing default.cfg for the first time, lock the cvar defaults
	// it may seem backwards to insert this text BEFORE the default.cfg
	// but Cbuf_InsertText inserts before, so this actually ends up after it.
	if (!strcmp(Cmd_Argv(1), "default.cfg"))
		Cbuf_InsertText("\ncvar_lockdefaults\n");

	Cbuf_InsertText (f);
	Mem_Free(f);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
static void Cmd_Echo_f (void)
{
	int		i;

	for (i=1 ; i<Cmd_Argc() ; i++)
		Con_Printf("%s ",Cmd_Argv(i));
	Con_Print("\n");
}

// DRESK - 5/14/06
// Support Doom3-style Toggle Console Command
/*
===============
Cmd_Toggle_f

Toggles a specified console variable amongst the values specified (default is 0 and 1)
===============
*/
static void Cmd_Toggle_f(void)
{
	// Acquire Number of Arguments
	int nNumArgs = Cmd_Argc();

	if(nNumArgs == 1)
		// No Arguments Specified; Print Usage
		Con_Print("Toggle Console Variable - Usage\n  toggle <variable> - toggles between 0 and 1\n  toggle <variable> <value> - toggles between 0 and <value>\n  toggle <variable> [string 1] [string 2]...[string n] - cycles through all strings\n");
	else
	{ // Correct Arguments Specified
		// Acquire Potential CVar
		cvar_t* cvCVar = Cvar_FindVar( Cmd_Argv(1) );

		if(cvCVar != NULL)
		{ // Valid CVar
			if(nNumArgs == 2)
			{ // Default Usage
				if(cvCVar->integer)
					Cvar_SetValueQuick(cvCVar, 0);
				else
					Cvar_SetValueQuick(cvCVar, 1);
			}
			else
			if(nNumArgs == 3)
			{ // 0 and Specified Usage
				if(cvCVar->integer == atoi(Cmd_Argv(2) ) )
					// CVar is Specified Value; // Reset to 0
					Cvar_SetValueQuick(cvCVar, 0);
				else
				if(cvCVar->integer == 0)
					// CVar is 0; Specify Value
					Cvar_SetQuick(cvCVar, Cmd_Argv(2) );
				else
					// CVar does not match; Reset to 0
					Cvar_SetValueQuick(cvCVar, 0);
			}
			else
			{ // Variable Values Specified
				int nCnt;
				int bFound = 0;

				for(nCnt = 2; nCnt < nNumArgs; nCnt++)
				{ // Cycle through Values
					if( strcmp(cvCVar->string, Cmd_Argv(nCnt) ) == 0)
					{ // Current Value Located; Increment to Next
						if( (nCnt + 1) == nNumArgs)
							// Max Value Reached; Reset
							Cvar_SetQuick(cvCVar, Cmd_Argv(2) );
						else
							// Next Value
							Cvar_SetQuick(cvCVar, Cmd_Argv(nCnt + 1) );

						// End Loop
						nCnt = nNumArgs;
						// Assign Found
						bFound = 1;
					}
				}
				if(!bFound)
					// Value not Found; Reset to Original
					Cvar_SetQuick(cvCVar, Cmd_Argv(2) );
			}

		}
		else
		{ // Invalid CVar
			Con_Printf("ERROR : CVar '%s' not found\n", Cmd_Argv(2) );
		}
	}
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
static void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[MAX_INPUTLINE];
	int			i, c;
	const char		*s;
	size_t		alloclen;

	if (Cmd_Argc() == 1)
	{
		Con_Print("Current alias commands:\n");
		for (a = cmd_alias ; a ; a=a->next)
			Con_Printf("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Con_Print("Alias name is too long\n");
		return;
	}

	// if the alias already exists, reuse it
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(s, a->name))
		{
			Z_Free (a->value);
			break;
		}
	}

	if (!a)
	{
		cmdalias_t *prev, *current;

		a = (cmdalias_t *)Z_Malloc (sizeof(cmdalias_t));
		strlcpy (a->name, s, sizeof (a->name));
		// insert it at the right alphanumeric position
		for( prev = NULL, current = cmd_alias ; current && strcmp( current->name, a->name ) < 0 ; prev = current, current = current->next )
			;
		if( prev ) {
			prev->next = a;
		} else {
			cmd_alias = a;
		}
		a->next = current;
	}


// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for (i=2 ; i< c ; i++)
	{
		strlcat (cmd, Cmd_Argv(i), sizeof (cmd));
		if (i != c)
			strlcat (cmd, " ", sizeof (cmd));
	}
	strlcat (cmd, "\n", sizeof (cmd));

	alloclen = strlen (cmd) + 1;
	a->value = (char *)Z_Malloc (alloclen);
	memcpy (a->value, cmd, alloclen);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s *next;
	const char *name;
	const char *description;
	xcommand_t function;
	qboolean csqcfunc;
} cmd_function_t;

static int cmd_argc;
static const char *cmd_argv[MAX_ARGS];
static const char *cmd_null_string = "";
static const char *cmd_args;
cmd_source_t cmd_source;


static cmd_function_t *cmd_functions;		// possible commands to execute

/*
Cmd_PreprocessString

Preprocesses strings and replaces $*, $param#, $cvar accordingly
*/
static void Cmd_PreprocessString( const char *intext, char *outtext, unsigned maxoutlen, cmdalias_t *alias ) {
	const char *in;
	unsigned outlen;
	int inquote;

	// don't crash if there's no room in the outtext buffer
	if( maxoutlen == 0 ) {
		return;
	}
	maxoutlen--; // because of \0

	in = intext;
	outlen = 0;
	inquote = 0;

	while( *in && outlen < maxoutlen ) {
		if( *in == '$' && !inquote ) {
			// this is some kind of expansion, see what comes after the $
			in++;
			// replacements that can always be used:
			// $$ is replaced with $, to allow escaping $
			// $<cvarname> is replaced with the contents of the cvar
			//
			// the following can be used in aliases only:
			// $* is replaced with all formal parameters (including name of the alias - this probably is not desirable)
			// $0 is replaced with the name of this alias
			// $<number> is replaced with an argument to this alias (or copied as-is if no such parameter exists), can be multiple digits
			if( *in == '$' ) {
				outtext[outlen++] = *in++;
			} else if( *in == '*' && alias ) {
				const char *linein = Cmd_Args();

				// include all parameters
				if (linein) {
					while( *linein && outlen < maxoutlen ) {
						outtext[outlen++] = *linein++;
					}
				}

				in++;
			} else if( '0' <= *in && *in <= '9' && alias ) {
				char *nexttoken;
				int argnum;

				argnum = strtol( in, &nexttoken, 10 );

				if( 0 <= argnum && argnum < Cmd_Argc() ) {
					const char *param = Cmd_Argv( argnum );
					while( *param && outlen < maxoutlen ) {
						outtext[outlen++] = *param++;
					}
					in = nexttoken;
				} else if( argnum >= Cmd_Argc() ) {
					Con_Printf( "Warning: Not enough parameters passed to alias '%s', at least %i expected:\n    %s\n", alias->name, argnum, alias->value );
					outtext[outlen++] = '$';
				}
			} else {
				cvar_t *cvar;
				const char *tempin = in;

				COM_ParseTokenConsole( &tempin );
				// don't expand rcon_password or similar cvars (CVAR_PRIVATE flag)
				if ((cvar = Cvar_FindVar(&com_token[0])) && !(cvar->flags & CVAR_PRIVATE)) {
					const char *cvarcontent = cvar->string;
					while( *cvarcontent && outlen < maxoutlen ) {
						outtext[outlen++] = *cvarcontent++;
					}
					in = tempin;
				} else {
					if( alias ) {
						Con_Printf( "Warning: could not find cvar %s when expanding alias %s\n    %s\n", com_token, alias->name, alias->value );
					} else {
						Con_Printf( "Warning: could not find cvar %s\n", com_token );
					}
					outtext[outlen++] = '$';
				}
			}
		} else {
			if( *in == '"' ) {
				inquote ^= 1;
			}
			outtext[outlen++] = *in++;
		}
	}
	outtext[outlen] = 0;
}

/*
============
Cmd_ExecuteAlias

Called for aliases and fills in the alias into the cbuffer
============
*/
static void Cmd_ExecuteAlias (cmdalias_t *alias)
{
	static char buffer[ MAX_INPUTLINE + 2 ];
	Cmd_PreprocessString( alias->value, buffer, sizeof(buffer) - 2, alias );
	// insert at start of command buffer, so that aliases execute in order
	// (fixes bug introduced by Black on 20050705)
	Cbuf_InsertText( buffer );
}

/*
========
Cmd_List

	CmdList Added by EvilTypeGuy eviltypeguy@qeradiant.com
	Thanks to Matthias "Maddes" Buecher, http://www.inside3d.com/qip/

========
*/
static void Cmd_List_f (void)
{
	cmd_function_t *cmd;
	const char *partial;
	int len, count;

	if (Cmd_Argc() > 1)
	{
		partial = Cmd_Argv (1);
		len = (int)strlen(partial);
	}
	else
	{
		partial = NULL;
		len = 0;
	}

	count = 0;
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (partial && strncmp(partial, cmd->name, len))
			continue;
		Con_Printf("%s : %s\n", cmd->name, cmd->description);
		count++;
	}

	if (partial)
		Con_Printf("%i Command%s beginning with \"%s\"\n\n", count, (count > 1) ? "s" : "", partial);
	else
		Con_Printf("%i Command%s\n\n", count, (count > 1) ? "s" : "");
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
	cmd_mempool = Mem_AllocPool("commands", 0, NULL);
	// space for commands and script files
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = sizeof(cmd_text_buf);
	cmd_text.cursize = 0;
}

void Cmd_Init_Commands (void)
{
//
// register our commands
//
	Cmd_AddCommand ("stuffcmds",Cmd_StuffCmds_f, "execute commandline parameters (must be present in quake.rc script)");
	Cmd_AddCommand ("exec",Cmd_Exec_f, "execute a script file");
	Cmd_AddCommand ("echo",Cmd_Echo_f, "print a message to the console (useful in scripts)");
	Cmd_AddCommand ("alias",Cmd_Alias_f, "create a script function (parameters are passed in as $1 through $9, and $* for all parameters)");
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer, "send a console commandline to the server (used by some mods)");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "make script execution wait for next rendered frame");
	Cmd_AddCommand ("set", Cvar_Set_f, "create or change the value of a console variable");
	Cmd_AddCommand ("seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to config.cfg");

	// 2000-01-09 CmdList, CvarList commands By Matthias "Maddes" Buecher
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	Cmd_AddCommand ("cmdlist", Cmd_List_f, "lists all console commands beginning with the specified prefix");
	Cmd_AddCommand ("cvarlist", Cvar_List_f, "lists all console variables beginning with the specified prefix");

	Cmd_AddCommand ("cvar_lockdefaults", Cvar_LockDefaults_f, "stores the current values of all cvars into their default values, only used once during startup after parsing default.cfg");
	Cmd_AddCommand ("cvar_resettodefaults_all", Cvar_ResetToDefaults_All_f, "sets all cvars to their locked default values");
	Cmd_AddCommand ("cvar_resettodefaults_nosaveonly", Cvar_ResetToDefaults_NoSaveOnly_f, "sets all non-saved cvars to their locked default values (variables that will not be saved to config.cfg)");
	Cmd_AddCommand ("cvar_resettodefaults_saveonly", Cvar_ResetToDefaults_SaveOnly_f, "sets all saved cvars to their locked default values (variables that will be saved to config.cfg)");

	// DRESK - 5/14/06
	// Support Doom3-style Toggle Command
	Cmd_AddCommand( "toggle", Cmd_Toggle_f, "toggles a console variable's values (use for more info)");
}

/*
============
Cmd_Shutdown
============
*/
void Cmd_Shutdown(void)
{
	Mem_FreePool(&cmd_mempool);
}

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
const char *Cmd_Argv (int arg)
{
	if (arg >= cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args
============
*/
const char *Cmd_Args (void)
{
	return cmd_args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
// AK: This function should only be called from ExcuteString because the current design is a bit of an hack
static void Cmd_TokenizeString (const char *text)
{
	int l;

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
		// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\r' && *text != '\n')
			text++;

		// line endings:
		// UNIX: \n
		// Mac: \r
		// Windows: \r\n
		if (*text == '\n' || *text == '\r')
		{
			// a newline separates commands in the buffer
			if (*text == '\r' && text[1] == '\n')
				text++;
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			cmd_args = text;

		if (!COM_ParseTokenConsole(&text))
			return;

		if (cmd_argc < MAX_ARGS)
		{
			l = (int)strlen(com_token) + 1;
			if (cmd_tokenizebufferpos + l > CMD_TOKENIZELENGTH)
			{
				Con_Printf("Cmd_TokenizeString: ran out of %i character buffer space for command arguements\n", CMD_TOKENIZELENGTH);
				break;
			}
			memcpy (cmd_tokenizebuffer + cmd_tokenizebufferpos, com_token, l);
			cmd_argv[cmd_argc] = cmd_tokenizebuffer + cmd_tokenizebufferpos;
			cmd_tokenizebufferpos += l;
			cmd_argc++;
		}
	}
}


/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *description)
{
	cmd_function_t *cmd;
	cmd_function_t *prev, *current;

// fail if the command is a variable name
	if (Cvar_FindVar( cmd_name ))
	{
		Con_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

// fail if the command already exists
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcmp (cmd_name, cmd->name))
		{
			if (function)
			{
				Con_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
				return;
			}
			else	//[515]: csqc
			{
				cmd->csqcfunc = true;
				return;
			}
		}
	}

	cmd = (cmd_function_t *)Mem_Alloc(cmd_mempool, sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->description = description;
	if(!function)			//[515]: csqc
		cmd->csqcfunc = true;
	cmd->next = cmd_functions;

// insert it at the right alphanumeric position
	for( prev = NULL, current = cmd_functions ; current && strcmp( current->name, cmd->name ) < 0 ; prev = current, current = current->next )
		;
	if( prev ) {
		prev->next = cmd;
	} else {
		cmd_functions = cmd;
	}
	cmd->next = current;
}

/*
============
Cmd_Exists
============
*/
qboolean Cmd_Exists (const char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!strcmp (cmd_name,cmd->name))
			return true;

	return false;
}


/*
============
Cmd_CompleteCommand
============
*/
const char *Cmd_CompleteCommand (const char *partial)
{
	cmd_function_t *cmd;
	size_t len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			return cmd->name;

	return NULL;
}

/*
	Cmd_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
int Cmd_CompleteCountPossible (const char *partial)
{
	cmd_function_t *cmd;
	size_t len;
	int h;

	h = 0;
	len = strlen(partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			h++;

	return h;
}

/*
	Cmd_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
const char **Cmd_CompleteBuildList (const char *partial)
{
	cmd_function_t *cmd;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
}

// written by LordHavoc
void Cmd_CompleteCommandPrint (const char *partial)
{
	cmd_function_t *cmd;
	size_t len = strlen(partial);
	// Loop through the command list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			Con_Printf("%s : %s\n", cmd->name, cmd->description);
}

/*
	Cmd_CompleteAlias

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
const char *Cmd_CompleteAlias (const char *partial)
{
	cmdalias_t *alias;
	size_t len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// Check functions
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			return alias->name;

	return NULL;
}

// written by LordHavoc
void Cmd_CompleteAliasPrint (const char *partial)
{
	cmdalias_t *alias;
	size_t len = strlen(partial);
	// Loop through the alias list and print all matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			Con_Printf("%s : %s\n", alias->name, alias->value);
}


/*
	Cmd_CompleteAliasCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
int Cmd_CompleteAliasCountPossible (const char *partial)
{
	cmdalias_t	*alias;
	size_t		len;
	int			h;

	h = 0;

	len = strlen(partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			h++;

	return h;
}

/*
	Cmd_CompleteAliasBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
const char **Cmd_CompleteAliasBuildList (const char *partial)
{
	cmdalias_t *alias;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cmd_CompleteAliasCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			buf[bpos++] = alias->name;

	buf[bpos] = NULL;
	return buf;
}

void Cmd_ClearCsqcFuncs (void)
{
	cmd_function_t *cmd;
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		cmd->csqcfunc = false;
}

qboolean CL_VM_ConsoleCommand (const char *cmd);
/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString (const char *text, cmd_source_t src)
{
	int oldpos;
	cmd_function_t *cmd;
	cmdalias_t *a;

	oldpos = cmd_tokenizebufferpos;
	cmd_source = src;

	Cmd_TokenizeString (text);

// execute the command line
	if (!Cmd_Argc())
	{
		cmd_tokenizebufferpos = oldpos;
		return;		// no tokens
	}

// check functions
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcasecmp (cmd_argv[0],cmd->name))
		{
			if(cmd->function && !cmd->csqcfunc)
				cmd->function ();
			else
				if(CL_VM_ConsoleCommand (text))	//[515]: csqc
					return;
				else
					if(cmd->function)
						cmd->function ();
			cmd_tokenizebufferpos = oldpos;
			return;
		}
	}

// check alias
	for (a=cmd_alias ; a ; a=a->next)
	{
		if (!strcasecmp (cmd_argv[0], a->name))
		{
			Cmd_ExecuteAlias(a);
			cmd_tokenizebufferpos = oldpos;
			return;
		}
	}

// check cvars
	if (!Cvar_Command () && host_framecount > 0)
		Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));

	cmd_tokenizebufferpos = oldpos;
}


/*
===================
Cmd_ForwardStringToServer

Sends an entire command string over to the server, unprocessed
===================
*/
void Cmd_ForwardStringToServer (const char *s)
{
	if (cls.state != ca_connected)
	{
		Con_Printf("Can't \"%s\", not connected\n", s);
		return;
	}

	if (!cls.netcon)
		return;

	// LordHavoc: thanks to Fuh for bringing the pure evil of SZ_Print to my
	// attention, it has been eradicated from here, its only (former) use in
	// all of darkplaces.
	if (cls.protocol == PROTOCOL_QUAKEWORLD)
		MSG_WriteByte(&cls.netcon->message, qw_clc_stringcmd);
	else
		MSG_WriteByte(&cls.netcon->message, clc_stringcmd);
	SZ_Write(&cls.netcon->message, (const unsigned char *)s, (int)strlen(s) + 1);
}

/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void Cmd_ForwardToServer (void)
{
	const char *s;
	if (!strcasecmp(Cmd_Argv(0), "cmd"))
	{
		// we want to strip off "cmd", so just send the args
		s = Cmd_Argc() > 1 ? Cmd_Args() : "";
	}
	else
	{
		// we need to keep the command name, so send Cmd_Argv(0), a space and then Cmd_Args()
		s = va("%s %s", Cmd_Argv(0), Cmd_Argc() > 1 ? Cmd_Args() : "");
	}
	// don't send an empty forward message if the user tries "cmd" by itself
	if (!s || !*s)
		return;
	Cmd_ForwardStringToServer(s);
}


/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/

int Cmd_CheckParm (const char *parm)
{
	int i;

	if (!parm)
	{
		Con_Printf ("Cmd_CheckParm: NULL");
		return 0;
	}

	for (i = 1; i < Cmd_Argc (); i++)
		if (!strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

