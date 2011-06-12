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

typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
	qboolean initstate; // indicates this command existed at init
	char *initialvalue; // backup copy of value at init
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

typedef struct cmddeferred_s
{
	struct cmddeferred_s *next;
	char *value;
	double time;
} cmddeferred_t;

static cmddeferred_t *cmd_deferred_list = NULL;

/*
============
Cmd_Defer_f

Cause a command to be executed after a delay.
============
*/
static void Cmd_Defer_f (void)
{
	if(Cmd_Argc() == 1)
	{
		double time = Sys_DoubleTime();
		cmddeferred_t *next = cmd_deferred_list;
		if(!next)
			Con_Printf("No commands are pending.\n");
		while(next)
		{
			Con_Printf("-> In %9.2f: %s\n", next->time-time, next->value);
			next = next->next;
		}
	} else if(Cmd_Argc() == 2 && !strcasecmp("clear", Cmd_Argv(1)))
	{
		while(cmd_deferred_list)
		{
			cmddeferred_t *cmd = cmd_deferred_list;
			cmd_deferred_list = cmd->next;
			Mem_Free(cmd->value);
			Mem_Free(cmd);
		}
	} else if(Cmd_Argc() == 3)
	{
		const char *value = Cmd_Argv(2);
		cmddeferred_t *defcmd = (cmddeferred_t*)Mem_Alloc(tempmempool, sizeof(*defcmd));
		size_t len = strlen(value);

		defcmd->time = Sys_DoubleTime() + atof(Cmd_Argv(1));
		defcmd->value = (char*)Mem_Alloc(tempmempool, len+1);
		memcpy(defcmd->value, value, len+1);
		defcmd->next = NULL;

		if(cmd_deferred_list)
		{
			cmddeferred_t *next = cmd_deferred_list;
			while(next->next)
				next = next->next;
			next->next = defcmd;
		} else
			cmd_deferred_list = defcmd;
		/* Stupid me... this changes the order... so commands with the same delay go blub :S
		  defcmd->next = cmd_deferred_list;
		  cmd_deferred_list = defcmd;*/
	} else {
		Con_Printf("usage: defer <seconds> <command>\n"
			   "       defer clear\n");
		return;
	}
}

/*
============
Cmd_Centerprint_f

Print something to the center of the screen using SCR_Centerprint
============
*/
static void Cmd_Centerprint_f (void)
{
	char msg[MAX_INPUTLINE];
	unsigned int i, c, p;
	c = Cmd_Argc();
	if(c >= 2)
	{
		strlcpy(msg, Cmd_Argv(1), sizeof(msg));
		for(i = 2; i < c; ++i)
		{
			strlcat(msg, " ", sizeof(msg));
			strlcat(msg, Cmd_Argv(i), sizeof(msg));
		}
		c = strlen(msg);
		for(p = 0, i = 0; i < c; ++i)
		{
			if(msg[i] == '\\')
			{
				if(msg[i+1] == 'n')
					msg[p++] = '\n';
				else if(msg[i+1] == '\\')
					msg[p++] = '\\';
				else {
					msg[p++] = '\\';
					msg[p++] = msg[i+1];
				}
				++i;
			} else {
				msg[p++] = msg[i];
			}
		}
		msg[p] = '\0';
		SCR_CenterPrint(msg);
	}
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
Cbuf_Execute_Deferred --blub
============
*/
void Cbuf_Execute_Deferred (void)
{
	cmddeferred_t *cmd, *prev;
	double time = Sys_DoubleTime();
	prev = NULL;
	cmd = cmd_deferred_list;
	while(cmd)
	{
		if(cmd->time <= time)
		{
			Cbuf_AddText(cmd->value);
			Cbuf_AddText(";\n");
			Mem_Free(cmd->value);

			if(prev) {
				prev->next = cmd->next;
				Mem_Free(cmd);
				cmd = prev->next;
			} else {
				cmd_deferred_list = cmd->next;
				Mem_Free(cmd);
				cmd = cmd_deferred_list;
			}
			continue;
		}
		prev = cmd;
		cmd = cmd->next;
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
	char *firstchar;
	qboolean quotes;
	char *comment;

	// LordHavoc: making sure the tokenizebuffer doesn't get filled up by repeated crashes
	cmd_tokenizebufferpos = 0;

	Cbuf_Execute_Deferred();
	while (cmd_text.cursize)
	{
// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = false;
		comment = NULL;
		for (i=0 ; i < cmd_text.cursize ; i++)
		{
			if(!comment)
			{
				if (text[i] == '"')
					quotes = !quotes;

				if(quotes)
				{
					// make sure i doesn't get > cursize which causes a negative
					// size in memmove, which is fatal --blub
					if (i < (cmd_text.cursize-1) && (text[i] == '\\' && (text[i+1] == '"' || text[i+1] == '\\')))
						i++;
				}
				else
				{
					if(text[i] == '/' && text[i + 1] == '/' && (i == 0 || ISWHITESPACE(text[i-1])))
						comment = &text[i];
					if(text[i] == ';')
						break;	// don't break if inside a quoted string or comment
				}
			}

			if (text[i] == '\r' || text[i] == '\n')
				break;
		}

		// better than CRASHING on overlong input lines that may SOMEHOW enter the buffer
		if(i >= MAX_INPUTLINE)
		{
			Con_Printf("Warning: console input buffer had an overlong line. Ignored.\n");
			line[0] = 0;
		}
		else
		{
			memcpy (line, text, comment ? (comment - text) : i);
			line[comment ? (comment - text) : i] = 0;
		}

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec, alias) can insert data at the
// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (cmd_text.data, text+i, cmd_text.cursize);
		}

// execute the command line
		firstchar = line;
		while(*firstchar && ISWHITESPACE(*firstchar))
			++firstchar;
		if(
			(strncmp(firstchar, "alias", 5) || !ISWHITESPACE(firstchar[5]))
			&&
			(strncmp(firstchar, "bind", 4) || !ISWHITESPACE(firstchar[4]))
			&&
			(strncmp(firstchar, "in_bind", 7) || !ISWHITESPACE(firstchar[7]))
		)
		{
			Cmd_PreprocessString( line, preprocessed, sizeof(preprocessed), NULL );
			Cmd_ExecuteString (preprocessed, src_command);
		}
		else
		{
			Cmd_ExecuteString (line, src_command);
		}

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
	// this is for all commandline options combined (and is bounds checked)
	char	build[MAX_INPUTLINE];

	if (Cmd_Argc () != 1)
	{
		Con_Print("stuffcmds : execute command line parameters\n");
		return;
	}

	// no reason to run the commandline arguments twice
	if (host_stuffcmdsrun)
		return;

	host_stuffcmdsrun = true;
	build[0] = 0;
	l = 0;
	for (i = 0;i < com_argc;i++)
	{
		if (com_argv[i] && com_argv[i][0] == '+' && (com_argv[i][1] < '0' || com_argv[i][1] > '9') && l + strlen(com_argv[i]) - 1 <= sizeof(build) - 1)
		{
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
				if (l + strlen(com_argv[i]) + 4 > sizeof(build) - 1)
					break;
				build[l++] = ' ';
				if (strchr(com_argv[i], ' '))
					build[l++] = '\"';
				for (j = 0;com_argv[i][j];j++)
					build[l++] = com_argv[i][j];
				if (strchr(com_argv[i], ' '))
					build[l++] = '\"';
			}
			build[l++] = '\n';
			i--;
		}
	}
	// now terminate the combined string and prepend it to the command buffer
	// we already reserved space for the terminator
	build[l++] = 0;
	Cbuf_InsertText (build);
}

static void Cmd_Exec(const char *filename)
{
	char *f;

	if (!strcmp(filename, "config.cfg"))
	{
		filename = CONFIGFILENAME;
		if (COM_CheckParm("-noconfig"))
			return; // don't execute config.cfg
	}

	f = (char *)FS_LoadFile (filename, tempmempool, false, NULL);
	if (!f)
	{
		Con_Printf("couldn't exec %s\n",filename);
		return;
	}
	Con_Printf("execing %s\n",filename);

	// if executing default.cfg for the first time, lock the cvar defaults
	// it may seem backwards to insert this text BEFORE the default.cfg
	// but Cbuf_InsertText inserts before, so this actually ends up after it.
	if (strlen(filename) >= 11 && !strcmp(filename + strlen(filename) - 11, "default.cfg"))
		Cbuf_InsertText("\ncvar_lockdefaults\n");

	// insert newline after the text to make sure the last line is terminated (some text editors omit the trailing newline)
	// (note: insertion order here is backwards from execution order, so this adds it after the text, by calling it before...)
	Cbuf_InsertText ("\n");
	Cbuf_InsertText (f);
	Mem_Free(f);

	// special defaults for specific games go here, these execute before default.cfg
	// Nehahra pushable crates malfunction in some levels if this is on
	// Nehahra NPC AI is confused by blowupfallenzombies
	if (gamemode == GAME_NEHAHRA)
		Cbuf_InsertText("\nsv_gameplayfix_upwardvelocityclearsongroundflag 0\nsv_gameplayfix_blowupfallenzombies 0\n\n");
	// hipnotic mission pack has issues in their 'friendly monster' ai, which seem to attempt to attack themselves for some reason when findradius() returns non-solid entities.
	// hipnotic mission pack has issues with bobbing water entities 'jittering' between different heights on alternate frames at the default 0.0138889 ticrate, 0.02 avoids this issue
	// hipnotic mission pack has issues in their proximity mine sticking code, which causes them to bounce off.
	if (gamemode == GAME_HIPNOTIC)
		Cbuf_InsertText("\nsv_gameplayfix_blowupfallenzombies 0\nsys_ticrate 0.02\nsv_gameplayfix_slidemoveprojectiles 0\n\n");
	// rogue mission pack has a guardian boss that does not wake up if findradius returns one of the entities around its spawn area
	if (gamemode == GAME_ROGUE)
		Cbuf_InsertText("\nsv_gameplayfix_findradiusdistancetobox 0\n\n");
	if (gamemode == GAME_NEXUIZ)
		Cbuf_InsertText("\nsv_gameplayfix_q2airaccelerate 1\nsv_gameplayfix_stepmultipletimes 1\n\n");
	if (gamemode == GAME_TENEBRAE)
		Cbuf_InsertText("\nr_shadow_gloss 2\nr_shadow_bumpscale_basetexture 4\n\n");
}

/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f (void)
{
	fssearch_t *s;
	int i;

	if (Cmd_Argc () != 2)
	{
		Con_Print("exec <filename> : execute a script file\n");
		return;
	}

	s = FS_Search(Cmd_Argv(1), true, true);
	if(!s || !s->numfilenames)
	{
		Con_Printf("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}

	for(i = 0; i < s->numfilenames; ++i)
		Cmd_Exec(s->filenames[i]);

	FS_FreeSearch(s);
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
			Con_Printf("ERROR : CVar '%s' not found\n", Cmd_Argv(1) );
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
			Con_Printf("%s : %s", a->name, a->value);
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
	for (i=2 ; i < c ; i++)
	{
		if (i != 2)
			strlcat (cmd, " ", sizeof (cmd));
		strlcat (cmd, Cmd_Argv(i), sizeof (cmd));
	}
	strlcat (cmd, "\n", sizeof (cmd));

	alloclen = strlen (cmd) + 1;
	if(alloclen >= 2)
		cmd[alloclen - 2] = '\n'; // to make sure a newline is appended even if too long
	a->value = (char *)Z_Malloc (alloclen);
	memcpy (a->value, cmd, alloclen);
}

/*
===============
Cmd_UnAlias_f

Remove existing aliases.
===============
*/
static void Cmd_UnAlias_f (void)
{
	cmdalias_t	*a, *p;
	int i;
	const char *s;

	if(Cmd_Argc() == 1)
	{
		Con_Print("unalias: Usage: unalias alias1 [alias2 ...]\n");
		return;
	}

	for(i = 1; i < Cmd_Argc(); ++i)
	{
		s = Cmd_Argv(i);
		p = NULL;
		for(a = cmd_alias; a; p = a, a = a->next)
		{
			if(!strcmp(s, a->name))
			{
				if (a->initstate) // we can not remove init aliases
					continue;
				if(a == cmd_alias)
					cmd_alias = a->next;
				if(p)
					p->next = a->next;
				Z_Free(a->value);
				Z_Free(a);
				break;
			}
		}
		if(!a)
			Con_Printf("unalias: %s alias not found\n", s);
	}
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
	xcommand_t consolefunction;
	xcommand_t clientfunction;
	qboolean csqcfunc;
	qboolean initstate; // indicates this command existed at init
} cmd_function_t;

static int cmd_argc;
static const char *cmd_argv[MAX_ARGS];
static const char *cmd_null_string = "";
static const char *cmd_args;
cmd_source_t cmd_source;


static cmd_function_t *cmd_functions;		// possible commands to execute

static const char *Cmd_GetDirectCvarValue(const char *varname, cmdalias_t *alias, qboolean *is_multiple)
{
	cvar_t *cvar;
	long argno;
	char *endptr;

	if(is_multiple)
		*is_multiple = false;

	if(!varname || !*varname)
		return NULL;

	if(alias)
	{
		if(!strcmp(varname, "*"))
		{
			if(is_multiple)
				*is_multiple = true;
			return Cmd_Args();
		}
		else if(!strcmp(varname, "#"))
		{
			return va("%d", Cmd_Argc());
		}
		else if(varname[strlen(varname) - 1] == '-')
		{
			argno = strtol(varname, &endptr, 10);
			if(endptr == varname + strlen(varname) - 1)
			{
				// whole string is a number, apart from the -
				const char *p = Cmd_Args();
				for(; argno > 1; --argno)
					if(!COM_ParseToken_Console(&p))
						break;
				if(p)
				{
					if(is_multiple)
						*is_multiple = true;

					// kill pre-argument whitespace
					for (;*p && ISWHITESPACE(*p);p++)
						;

					return p;
				}
			}
		}
		else
		{
			argno = strtol(varname, &endptr, 10);
			if(*endptr == 0)
			{
				// whole string is a number
				// NOTE: we already made sure we don't have an empty cvar name!
				if(argno >= 0 && argno < Cmd_Argc())
					return Cmd_Argv(argno);
			}
		}
	}

	if((cvar = Cvar_FindVar(varname)) && !(cvar->flags & CVAR_PRIVATE))
		return cvar->string;

	return NULL;
}

qboolean Cmd_QuoteString(char *out, size_t outlen, const char *in, const char *quoteset, qboolean putquotes)
{
	qboolean quote_quot = !!strchr(quoteset, '"');
	qboolean quote_backslash = !!strchr(quoteset, '\\');
	qboolean quote_dollar = !!strchr(quoteset, '$');

	if(putquotes)
	{
		if(outlen <= 2)
		{
			*out++ = 0;
			return false;
		}
		*out++ = '"'; --outlen;
		--outlen;
	}

	while(*in)
	{
		if(*in == '"' && quote_quot)
		{
			if(outlen <= 2)
				goto fail;
			*out++ = '\\'; --outlen;
			*out++ = '"'; --outlen;
		}
		else if(*in == '\\' && quote_backslash)
		{
			if(outlen <= 2)
				goto fail;
			*out++ = '\\'; --outlen;
			*out++ = '\\'; --outlen;
		}
		else if(*in == '$' && quote_dollar)
		{
			if(outlen <= 2)
				goto fail;
			*out++ = '$'; --outlen;
			*out++ = '$'; --outlen;
		}
		else
		{
			if(outlen <= 1)
				goto fail;
			*out++ = *in; --outlen;
		}
		++in;
	}
	if(putquotes)
		*out++ = '"';
	*out++ = 0;
	return true;
fail:
	if(putquotes)
		*out++ = '"';
	*out++ = 0;
	return false;
}

static const char *Cmd_GetCvarValue(const char *var, size_t varlen, cmdalias_t *alias)
{
	static char varname[MAX_INPUTLINE];
	static char varval[MAX_INPUTLINE];
	const char *varstr;
	char *varfunc;
static char asis[] = "asis"; // just to suppress const char warnings

	if(varlen >= MAX_INPUTLINE)
		varlen = MAX_INPUTLINE - 1;
	memcpy(varname, var, varlen);
	varname[varlen] = 0;
	varfunc = strchr(varname, ' ');

	if(varfunc)
	{
		*varfunc = 0;
		++varfunc;
	}

	if(*var == 0)
	{
		// empty cvar name?
		return NULL;
	}

	varstr = NULL;

	if(varname[0] == '$')
		varstr = Cmd_GetDirectCvarValue(Cmd_GetDirectCvarValue(varname + 1, alias, NULL), alias, NULL);
	else
	{
		qboolean is_multiple = false;
		// Exception: $* and $n- don't use the quoted form by default
		varstr = Cmd_GetDirectCvarValue(varname, alias, &is_multiple);
		if(is_multiple)
			if(!varfunc)
				varfunc = asis;
	}

	if(!varstr)
	{
		if(alias)
			Con_Printf("Warning: Could not expand $%s in alias %s\n", varname, alias->name);
		else
			Con_Printf("Warning: Could not expand $%s\n", varname);
		return NULL;
	}

	if(!varfunc || !strcmp(varfunc, "q")) // note: quoted form is default, use "asis" to override!
	{
		// quote it so it can be used inside double quotes
		// we just need to replace " by \", and of course, double backslashes
		Cmd_QuoteString(varval, sizeof(varval), varstr, "\"\\", false);
		return varval;
	}
	else if(!strcmp(varfunc, "asis"))
	{
		return varstr;
	}
	else
		Con_Printf("Unknown variable function %s\n", varfunc);

	return varstr;
}

/*
Cmd_PreprocessString

Preprocesses strings and replaces $*, $param#, $cvar accordingly. Also strips comments.
*/
static void Cmd_PreprocessString( const char *intext, char *outtext, unsigned maxoutlen, cmdalias_t *alias ) {
	const char *in;
	size_t eat, varlen;
	unsigned outlen;
	const char *val;

	// don't crash if there's no room in the outtext buffer
	if( maxoutlen == 0 ) {
		return;
	}
	maxoutlen--; // because of \0

	in = intext;
	outlen = 0;

	while( *in && outlen < maxoutlen ) {
		if( *in == '$' ) {
			// this is some kind of expansion, see what comes after the $
			in++;

			// The console does the following preprocessing:
			//
			// - $$ is transformed to a single dollar sign.
			// - $var or ${var} are expanded to the contents of the named cvar,
			//   with quotation marks and backslashes quoted so it can safely
			//   be used inside quotation marks (and it should always be used
			//   that way)
			// - ${var asis} inserts the cvar value as is, without doing this
			//   quoting
			// - prefix the cvar name with a dollar sign to do indirection;
			//   for example, if $x has the value timelimit, ${$x} will return
			//   the value of $timelimit
			// - when expanding an alias, the special variable name $* refers
			//   to all alias parameters, and a number refers to that numbered
			//   alias parameter, where the name of the alias is $0, the first
			//   parameter is $1 and so on; as a special case, $* inserts all
			//   parameters, without extra quoting, so one can use $* to just
			//   pass all parameters around. All parameters starting from $n
			//   can be referred to as $n- (so $* is equivalent to $1-).
			//
			// Note: when expanding an alias, cvar expansion is done in the SAME step
			// as alias expansion so that alias parameters or cvar values containing
			// dollar signs have no unwanted bad side effects. However, this needs to
			// be accounted for when writing complex aliases. For example,
			//   alias foo "set x NEW; echo $x"
			// actually expands to
			//   "set x NEW; echo OLD"
			// and will print OLD! To work around this, use a second alias:
			//   alias foo "set x NEW; foo2"
			//   alias foo2 "echo $x"
			//
			// Also note: lines starting with alias are exempt from cvar expansion.
			// If you want cvar expansion, write "alias" instead:
			//
			//   set x 1
			//   alias foo "echo $x"
			//   "alias" bar "echo $x"
			//   set x 2
			//
			// foo will print 2, because the variable $x will be expanded when the alias
			// gets expanded. bar will print 1, because the variable $x was expanded
			// at definition time. foo can be equivalently defined as
			//
			//   "alias" foo "echo $$x"
			//
			// because at definition time, $$ will get replaced to a single $.

			if( *in == '$' ) {
				val = "$";
				eat = 1;
			} else if(*in == '{') {
				varlen = strcspn(in + 1, "}");
				if(in[varlen + 1] == '}')
				{
					val = Cmd_GetCvarValue(in + 1, varlen, alias);
					eat = varlen + 2;
				}
				else
				{
					// ran out of data?
					val = NULL;
					eat = varlen + 1;
				}
			} else {
				varlen = strspn(in, "#*0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-");
				val = Cmd_GetCvarValue(in, varlen, alias);
				eat = varlen;
			}
			if(val)
			{
				// insert the cvar value
				while(*val && outlen < maxoutlen)
					outtext[outlen++] = *val++;
				in += eat;
			}
			else
			{
				// copy the unexpanded text
				outtext[outlen++] = '$';
				while(eat && outlen < maxoutlen)
				{
					outtext[outlen++] = *in++;
					--eat;
				}
			}
		}
		else 
			outtext[outlen++] = *in++;
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
	static char buffer[ MAX_INPUTLINE ];
	static char buffer2[ MAX_INPUTLINE ];
	Cmd_PreprocessString( alias->value, buffer, sizeof(buffer) - 2, alias );
	// insert at start of command buffer, so that aliases execute in order
	// (fixes bug introduced by Black on 20050705)

	// Note: Cbuf_PreprocessString will be called on this string AGAIN! So we
	// have to make sure that no second variable expansion takes place, otherwise
	// alias parameters containing dollar signs can have bad effects.
	Cmd_QuoteString(buffer2, sizeof(buffer2), buffer, "$", false);
	Cbuf_InsertText( buffer2 );
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
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (partial && (ispattern ? !matchpattern_with_separator(cmd->name, partial, false, "", false) : strncmp(partial, cmd->name, len)))
			continue;
		Con_Printf("%s : %s\n", cmd->name, cmd->description);
		count++;
	}

	if (len)
	{
		if(ispattern)
			Con_Printf("%i Command%s matching \"%s\"\n\n", count, (count > 1) ? "s" : "", partial);
		else
			Con_Printf("%i Command%s beginning with \"%s\"\n\n", count, (count > 1) ? "s" : "", partial);
	}
	else
		Con_Printf("%i Command%s\n\n", count, (count > 1) ? "s" : "");
}

static void Cmd_Apropos_f(void)
{
	cmd_function_t *cmd;
	cvar_t *cvar;
	cmdalias_t *alias;
	const char *partial;
	int count;
	qboolean ispattern;

	if (Cmd_Argc() > 1)
		partial = Cmd_Args();
	else
	{
		Con_Printf("usage: apropos <string>\n");
		return;
	}

	ispattern = partial && (strchr(partial, '*') || strchr(partial, '?'));
	if(!ispattern)
		partial = va("*%s*", partial);

	count = 0;
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (!matchpattern_with_separator(cvar->name, partial, true, "", false))
		if (!matchpattern_with_separator(cvar->description, partial, true, "", false))
			continue;
		Con_Printf ("cvar ^3%s^7 is \"%s\" [\"%s\"] %s\n", cvar->name, cvar->string, cvar->defstring, cvar->description);
		count++;
	}
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!matchpattern_with_separator(cmd->name, partial, true, "", false))
		if (!matchpattern_with_separator(cmd->description, partial, true, "", false))
			continue;
		Con_Printf("command ^2%s^7: %s\n", cmd->name, cmd->description);
		count++;
	}
	for (alias = cmd_alias; alias; alias = alias->next)
	{
		// procede here a bit differently as an alias value always got a final \n
		if (!matchpattern_with_separator(alias->name, partial, true, "", false))
		if (!matchpattern_with_separator(alias->value, partial, true, "\n", false)) // when \n is as separator wildcards don't match it
			continue;
		Con_Printf("alias ^5%s^7: %s", alias->name, alias->value); // do not print an extra \n
		count++;
	}
	Con_Printf("%i result%s\n\n", count, (count > 1) ? "s" : "");
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
	Cmd_AddCommand ("alias",Cmd_Alias_f, "create a script function (parameters are passed in as $X (being X a number), $* for all parameters, $X- for all parameters starting from $X). Without arguments show the list of all alias");
	Cmd_AddCommand ("unalias",Cmd_UnAlias_f, "remove an alias");
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer, "send a console commandline to the server (used by some mods)");
	Cmd_AddCommand ("wait", Cmd_Wait_f, "make script execution wait for next rendered frame");
	Cmd_AddCommand ("set", Cvar_Set_f, "create or change the value of a console variable");
	Cmd_AddCommand ("seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to config.cfg");
	Cmd_AddCommand ("unset", Cvar_Del_f, "delete a cvar (does not work for static ones like _cl_name, or read-only ones)");
#ifdef FILLALLCVARSWITHRUBBISH
	Cmd_AddCommand ("fillallcvarswithrubbish", Cvar_FillAll_f, "fill all cvars with a specified number of characters to provoke buffer overruns");
#endif /* FILLALLCVARSWITHRUBBISH */

	// 2000-01-09 CmdList, CvarList commands By Matthias "Maddes" Buecher
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	Cmd_AddCommand ("cmdlist", Cmd_List_f, "lists all console commands beginning with the specified prefix or matching the specified wildcard pattern");
	Cmd_AddCommand ("cvarlist", Cvar_List_f, "lists all console variables beginning with the specified prefix or matching the specified wildcard pattern");
	Cmd_AddCommand ("apropos", Cmd_Apropos_f, "lists all console variables/commands/aliases containing the specified string in the name or description");

	Cmd_AddCommand ("cvar_lockdefaults", Cvar_LockDefaults_f, "stores the current values of all cvars into their default values, only used once during startup after parsing default.cfg");
	Cmd_AddCommand ("cvar_resettodefaults_all", Cvar_ResetToDefaults_All_f, "sets all cvars to their locked default values");
	Cmd_AddCommand ("cvar_resettodefaults_nosaveonly", Cvar_ResetToDefaults_NoSaveOnly_f, "sets all non-saved cvars to their locked default values (variables that will not be saved to config.cfg)");
	Cmd_AddCommand ("cvar_resettodefaults_saveonly", Cvar_ResetToDefaults_SaveOnly_f, "sets all saved cvars to their locked default values (variables that will be saved to config.cfg)");

	Cmd_AddCommand ("cprint", Cmd_Centerprint_f, "print something at the screen center");
	Cmd_AddCommand ("defer", Cmd_Defer_f, "execute a command in the future");

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
		while (*text && ISWHITESPACE(*text) && *text != '\r' && *text != '\n')
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

		if (!COM_ParseToken_Console(&text))
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
void Cmd_AddCommand_WithClientCommand (const char *cmd_name, xcommand_t consolefunction, xcommand_t clientfunction, const char *description)
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
			if (consolefunction || clientfunction)
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
	cmd->consolefunction = consolefunction;
	cmd->clientfunction = clientfunction;
	cmd->description = description;
	if(!consolefunction && !clientfunction)			//[515]: csqc
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

void Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *description)
{
	Cmd_AddCommand_WithClientCommand (cmd_name, function, NULL, description);
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
			Con_Printf("^2%s^7: %s\n", cmd->name, cmd->description);
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
			Con_Printf("^5%s^7: %s", alias->name, alias->value);
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
	int found;
	cmd_function_t *cmd;
	cmdalias_t *a;

	oldpos = cmd_tokenizebufferpos;
	cmd_source = src;
	found = false;

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
			if (cmd->csqcfunc && CL_VM_ConsoleCommand (text))	//[515]: csqc
				return;
			switch (src)
			{
			case src_command:
				if (cmd->consolefunction)
					cmd->consolefunction ();
				else if (cmd->clientfunction)
				{
					if (cls.state == ca_connected)
					{
						// forward remote commands to the server for execution
						Cmd_ForwardToServer();
					}
					else
						Con_Printf("Can not send command \"%s\", not connected.\n", Cmd_Argv(0));
				}
				else
					Con_Printf("Command \"%s\" can not be executed\n", Cmd_Argv(0));
				found = true;
				goto command_found;
			case src_client:
				if (cmd->clientfunction)
				{
					cmd->clientfunction ();
					cmd_tokenizebufferpos = oldpos;
					return;
				}
				break;
			}
			break;
		}
	}
command_found:

	// if it's a client command and no command was found, say so.
	if (cmd_source == src_client)
	{
		Con_Printf("player \"%s\" tried to %s\n", host_client->name, text);
		cmd_tokenizebufferpos = oldpos;
		return;
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

	if(found) // if the command was hooked and found, all is good
	{
		cmd_tokenizebufferpos = oldpos;
		return;
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
	char temp[128];
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
	if ((!strncmp(s, "say ", 4) || !strncmp(s, "say_team ", 9)) && cl_locs_enable.integer)
	{
		// say/say_team commands can replace % character codes with status info
		while (*s)
		{
			if (*s == '%' && s[1])
			{
				// handle proquake message macros
				temp[0] = 0;
				switch (s[1])
				{
				case 'l': // current location
					CL_Locs_FindLocationName(temp, sizeof(temp), cl.movement_origin);
					break;
				case 'h': // current health
					dpsnprintf(temp, sizeof(temp), "%i", cl.stats[STAT_HEALTH]);
					break;
				case 'a': // current armor
					dpsnprintf(temp, sizeof(temp), "%i", cl.stats[STAT_ARMOR]);
					break;
				case 'x': // current rockets
					dpsnprintf(temp, sizeof(temp), "%i", cl.stats[STAT_ROCKETS]);
					break;
				case 'c': // current cells
					dpsnprintf(temp, sizeof(temp), "%i", cl.stats[STAT_CELLS]);
					break;
				// silly proquake macros
				case 'd': // loc at last death
					CL_Locs_FindLocationName(temp, sizeof(temp), cl.lastdeathorigin);
					break;
				case 't': // current time
					dpsnprintf(temp, sizeof(temp), "%.0f:%.0f", floor(cl.time / 60), cl.time - floor(cl.time / 60) * 60);
					break;
				case 'r': // rocket launcher status ("I have RL", "I need rockets", "I need RL")
					if (!(cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER))
						dpsnprintf(temp, sizeof(temp), "I need RL");
					else if (!cl.stats[STAT_ROCKETS])
						dpsnprintf(temp, sizeof(temp), "I need rockets");
					else
						dpsnprintf(temp, sizeof(temp), "I have RL");
					break;
				case 'p': // powerup status (outputs "quad" "pent" and "eyes" according to status)
					if (cl.stats[STAT_ITEMS] & IT_QUAD)
					{
						if (temp[0])
							strlcat(temp, " ", sizeof(temp));
						strlcat(temp, "quad", sizeof(temp));
					}
					if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
					{
						if (temp[0])
							strlcat(temp, " ", sizeof(temp));
						strlcat(temp, "pent", sizeof(temp));
					}
					if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
					{
						if (temp[0])
							strlcat(temp, " ", sizeof(temp));
						strlcat(temp, "eyes", sizeof(temp));
					}
					break;
				case 'w': // weapon status (outputs "SSG:NG:SNG:GL:RL:LG" with the text between : characters omitted if you lack the weapon)
					if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
						strlcat(temp, "SSG", sizeof(temp));
					strlcat(temp, ":", sizeof(temp));
					if (cl.stats[STAT_ITEMS] & IT_NAILGUN)
						strlcat(temp, "NG", sizeof(temp));
					strlcat(temp, ":", sizeof(temp));
					if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
						strlcat(temp, "SNG", sizeof(temp));
					strlcat(temp, ":", sizeof(temp));
					if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
						strlcat(temp, "GL", sizeof(temp));
					strlcat(temp, ":", sizeof(temp));
					if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
						strlcat(temp, "RL", sizeof(temp));
					strlcat(temp, ":", sizeof(temp));
					if (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
						strlcat(temp, "LG", sizeof(temp));
					break;
				default:
					// not a recognized macro, print it as-is...
					temp[0] = s[0];
					temp[1] = s[1];
					temp[2] = 0;
					break;
				}
				// write the resulting text
				SZ_Write(&cls.netcon->message, (unsigned char *)temp, strlen(temp));
				s += 2;
				continue;
			}
			MSG_WriteByte(&cls.netcon->message, *s);
			s++;
		}
		MSG_WriteByte(&cls.netcon->message, 0);
	}
	else // any other command is passed on as-is
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



void Cmd_SaveInitState(void)
{
	cmd_function_t *f;
	cmdalias_t *a;
	for (f = cmd_functions;f;f = f->next)
		f->initstate = true;
	for (a = cmd_alias;a;a = a->next)
	{
		a->initstate = true;
		a->initialvalue = Mem_strdup(zonemempool, a->value);
	}
	Cvar_SaveInitState();
}

void Cmd_RestoreInitState(void)
{
	cmd_function_t *f, **fp;
	cmdalias_t *a, **ap;
	for (fp = &cmd_functions;(f = *fp);)
	{
		if (f->initstate)
			fp = &f->next;
		else
		{
			// destroy this command, it didn't exist at init
			Con_DPrintf("Cmd_RestoreInitState: Destroying command %s\n", f->name);
			*fp = f->next;
			Z_Free(f);
		}
	}
	for (ap = &cmd_alias;(a = *ap);)
	{
		if (a->initstate)
		{
			// restore this alias, it existed at init
			if (strcmp(a->value ? a->value : "", a->initialvalue ? a->initialvalue : ""))
			{
				Con_DPrintf("Cmd_RestoreInitState: Restoring alias %s\n", a->name);
				if (a->value)
					Z_Free(a->value);
				a->value = Mem_strdup(zonemempool, a->initialvalue);
			}
			ap = &a->next;
		}
		else
		{
			// free this alias, it didn't exist at init...
			Con_DPrintf("Cmd_RestoreInitState: Destroying alias %s\n", a->name);
			*ap = a->next;
			if (a->value)
				Z_Free(a->value);
			Z_Free(a);
		}
	}
	Cvar_RestoreInitState();
}
