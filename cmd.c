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

typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;

static cmdalias_t *cmd_alias;

static qboolean cmd_wait;

static mempool_t *cmd_mempool;

#define CMD_TOKENIZELENGTH 4096
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

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	// LordHavoc: inreased this from 8192 to 32768
	SZ_Alloc (&cmd_text, 32768, "command buffer"); // space for commands and script files
}


/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (const char *text)
{
	int		l;

	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Print("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, text, strlen (text));
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
		temp = Mem_Alloc (tempmempool, templen);
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
		SZ_Write (&cmd_text, temp, templen);
		Mem_Free (temp);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int i;
	char *text;
	char line[1024];
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
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
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
		Cmd_ExecuteString (line, src_command);

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
void Cmd_StuffCmds_f (void)
{
	int		i, j;
	int		s;
	char	*text, *build, c;

	if (Cmd_Argc () != 1)
	{
		Con_Print("stuffcmds : execute command line parameters\n");
		return;
	}

// build the combined string to parse from
	s = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		s += strlen (com_argv[i]) + 1;
	}
	if (!s)
		return;

	text = Mem_Alloc (tempmempool, s + 1);
	text[0] = 0;
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP nulls out -NXHost
		strcat (text,com_argv[i]);
		if (i != com_argc-1)
			strcat (text, " ");
	}

	// pull out the commands
	build = Mem_Alloc (tempmempool, s + 1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;

			strcat (build, text+i);
			strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	if (build[0])
		Cbuf_InsertText (build);

	Mem_Free (text);
	Mem_Free (build);
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

	f = (char *)FS_LoadFile (Cmd_Argv(1), tempmempool, false);
	if (!f)
	{
		Con_Printf("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}
	Con_DPrintf("execing %s\n",Cmd_Argv(1));

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

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
static void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[1024];
	int			i, c;
	const char		*s;

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
		a = Z_Malloc (sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strlcpy (a->name, s, sizeof (a->name));

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

	a->value = Z_Malloc (strlen (cmd) + 1);
	strcpy (a->value, cmd);
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
	xcommand_t function;
} cmd_function_t;


#define	MAX_ARGS		80

static int cmd_argc;
static const char *cmd_argv[MAX_ARGS];
static const char *cmd_null_string = "";
static const char *cmd_args = NULL;

cmd_source_t cmd_source;


static cmd_function_t *cmd_functions;		// possible commands to execute

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
		len = strlen(partial);
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
		Con_Printf("%s\n", cmd->name);
		count++;
	}

	Con_Printf("%i Command%s", count, (count > 1) ? "s" : "");
	if (partial)
		Con_Printf(" beginning with \"%s\"", partial);

	Con_Print("\n\n");
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
	cmd_mempool = Mem_AllocPool("commands", 0, NULL);

//
// register our commands
//
	Cmd_AddCommand ("stuffcmds",Cmd_StuffCmds_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand ("cmdlist", Cmd_List_f); 	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	Cmd_AddCommand ("cvarlist", Cvar_List_f);	// 2000-01-09 CmdList, CvarList commands
												// By Matthias "Maddes" Buecher
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
static void Cmd_TokenizeString (const char *text)
{
	int l;

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
		// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
			text++;

		if (*text == '\n')
		{
			// a newline seperates commands in the buffer
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
			l = strlen(com_token) + 1;
			if (cmd_tokenizebufferpos + l > CMD_TOKENIZELENGTH)
				Sys_Error("Cmd_TokenizeString: ran out of %i character buffer space for command arguements\n", CMD_TOKENIZELENGTH);
			strcpy (cmd_tokenizebuffer + cmd_tokenizebufferpos, com_token);
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
void Cmd_AddCommand (const char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

// fail if the command already exists
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcmp (cmd_name, cmd->name))
		{
			Con_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Mem_Alloc(cmd_mempool, sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
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
	int len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp(partial, cmd->name, len))
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
	int len, h;

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
	int len = 0;
	int bpos = 0;
	int sizeofbuf = (Cmd_CompleteCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncasecmp(partial, cmd->name, len))
			buf[bpos++] = cmd->name;

	buf[bpos] = NULL;
	return buf;
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
	int len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// Check functions
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			return alias->name;

	return NULL;
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
	int			len;
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
	int len = 0;
	int bpos = 0;
	int sizeofbuf = (Cmd_CompleteAliasCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			buf[bpos++] = alias->name;

	buf[bpos] = NULL;
	return buf;
}

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

// check functions (only after host_initialized)
	if (host_initialized || !strcasecmp(cmd_argv[0], "exec"))
	{
		for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		{
			if (!strcasecmp (cmd_argv[0],cmd->name))
			{
				cmd->function ();
				cmd_tokenizebufferpos = oldpos;
				return;
			}
		}
	}

// check alias (only after host_initialized)
	if (host_initialized)
	{
		for (a=cmd_alias ; a ; a=a->next)
		{
			if (!strcasecmp (cmd_argv[0], a->name))
			{
				Cbuf_InsertText (a->value);
				cmd_tokenizebufferpos = oldpos;
				return;
			}
		}
	}

// check cvars (always)
	if (!Cvar_Command () && host_initialized)
		Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));

	cmd_tokenizebufferpos = oldpos;
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
	if (cls.state != ca_connected)
	{
		Con_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	// LordHavoc: thanks to Fuh for bringing the pure evil of SZ_Print to my
	// attention, it has been eradicated from here, its only (former) use in
	// all of darkplaces.
	if (strcasecmp(Cmd_Argv(0), "cmd") != 0)
		s = va("%s %s", Cmd_Argv(0), Cmd_Argc() > 1 ? Cmd_Args() : "\n");
	else
		s = Cmd_Argc() > 1 ? Cmd_Args() : "\n";
	MSG_WriteByte(&cls.message, clc_stringcmd);
	SZ_Write(&cls.message, s, strlen(s) + 1);
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
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < Cmd_Argc (); i++)
		if (!strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

