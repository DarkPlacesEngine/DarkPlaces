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

// cmd.h -- Command buffer and command execution

//===========================================================================

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

#ifndef CMD_H
#define CMD_H

#include "thread.h"

struct cmd_state_s;

// Command flags
#define CMD_CLIENT				(1<<0)
#define CMD_SERVER				(1<<1)
#define CMD_CLIENT_FROM_SERVER	(1<<2)
#define CMD_SERVER_FROM_CLIENT	(1<<3)
#define CMD_USERINFO			(1<<4)
#define CMD_CHEAT				(1<<5)


#define CMD_SHARED 3

typedef void(*xcommand_t) (struct cmd_state_s *cmd);

typedef enum
{
	src_client,		///< came in over a net connection as a clc_stringcmd
					///< host_client will be valid during this state.
	src_command		///< from the command buffer
} cmd_source_t;

typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
	qboolean initstate; // indicates this command existed at init
	char *initialvalue; // backup copy of value at init
} cmdalias_t;

typedef struct cmd_function_s
{
	int flags;
	struct cmd_function_s *next;
	const char *name;
	const char *description;
	xcommand_t function;
	qboolean csqcfunc;
	qboolean autofunc;
	qboolean initstate; // indicates this command existed at init
} cmd_function_t;

typedef struct cmddeferred_s
{
	struct cmddeferred_s *next;
	char *value;
	double delay;
} cmddeferred_t;

/// container for user-defined QC functions and aliases, shared between different command interpreters
typedef struct cmd_userdefined_s
{
	// csqc functions - this is a mess
	cmd_function_t *csqc_functions;

	// aliases
	cmdalias_t *alias;
}
cmd_userdefined_t;

/// command interpreter state - the tokenizing and execution of commands, as well as pointers to which cvars and aliases they can access
typedef struct cmd_state_s
{
	mempool_t *mempool;

	int argc;
	const char *argv[MAX_ARGS];
	const char *null_string;
	const char *args;
	cmd_source_t source;

	struct cbuf_s *cbuf;

	cmd_userdefined_t *userdefined; // possible csqc functions and aliases to execute

	cmd_function_t *engine_functions;

	cvar_state_t *cvars; // which cvar system is this cmd state able to access? (&cvars_all or &cvars_null)
	int cvars_flagsmask; // which CVAR_* flags should be visible to this interpreter? (CVAR_CLIENT | CVAR_SERVER, or just CVAR_SERVER)

	int cmd_flags; // cmd flags that identify this interpreter

	/*
	 * If a requested flag matches auto_flags, a command will be
	 * added to a given interpreter with auto_function. For example,
	 * a CMD_SERVER_FROM_CLIENT command should be automatically added
	 * to the client interpreter as CL_ForwardToServer_f. It can be
	 * overridden at any time.
	 */
	int auto_flags;
	xcommand_t auto_function;
}
cmd_state_t;

typedef struct cbuf_cmd_s
{
	struct cbuf_cmd_s *prev, *next;
	cmd_state_t *source;
	double delay;
	size_t size;
	char text[MAX_INPUTLINE];
	qboolean pending;
} cbuf_cmd_t;

typedef struct cbuf_s
{
	cbuf_cmd_t *start;
	cbuf_cmd_t *deferred;
	cbuf_cmd_t *free;
	qboolean wait;
	size_t maxsize;
	size_t size;
	char tokenizebuffer[CMD_TOKENIZELENGTH];
	int tokenizebufferpos;
	double deferred_oldtime;
	void *lock;
} cbuf_t;

extern cmd_userdefined_t cmd_userdefined_all; // aliases and csqc functions
extern cmd_userdefined_t cmd_userdefined_null; // intentionally empty

// command interpreter for client commands injected by CSQC, MQC or client engine code
// uses cmddefs_all
extern cmd_state_t cmd_client;
// command interpreter for server commands injected by MQC, SVQC, menu engine code or server engine code
// uses cmddefs_all
extern cmd_state_t cmd_server;
// command interpreter for server commands received over network from clients
// uses cmddefs_null
extern cmd_state_t cmd_serverfromclient;

extern qboolean host_stuffcmdsrun;

void Cbuf_Lock(cbuf_t *cbuf);
void Cbuf_Unlock(cbuf_t *cbuf);

/*! as new commands are generated from the console or keybindings,
 * the text is added to the end of the command buffer.
 */
void Cbuf_AddText (cmd_state_t *cmd, const char *text);

/*! when a command wants to issue other commands immediately, the text is
 * inserted at the beginning of the buffer, before any remaining unexecuted
 * commands.
 */
void Cbuf_InsertText (cmd_state_t *cmd, const char *text);

/*! Pulls off terminated lines of text from the command buffer and sends
 * them through Cmd_ExecuteString.  Stops when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * \note Do not call inside a command function!
 */
void Cbuf_Execute (cbuf_t *cbuf);
/*! Performs deferred commands and runs Cbuf_Execute, called by Host_Frame */
void Cbuf_Frame (cbuf_t *cbuf);

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

void Cmd_Init(void);
void Cmd_Shutdown(void);

// called by Host_Init, this marks cvars, commands and aliases with their init values
void Cmd_SaveInitState(void);
// called by FS_GameDir_f, this restores cvars, commands and aliases to init values
void Cmd_RestoreInitState(void);

void Cmd_AddCommand(int flags, const char *cmd_name, xcommand_t function, const char *description);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory

/// used by the cvar code to check for cvar / command name overlap
qboolean Cmd_Exists (cmd_state_t *cmd, const char *cmd_name);

/// attempts to match a partial command for automatic command line completion
/// returns NULL if nothing fits
const char *Cmd_CompleteCommand (cmd_state_t *cmd, const char *partial);

int Cmd_CompleteAliasCountPossible (cmd_state_t *cmd, const char *partial);

const char **Cmd_CompleteAliasBuildList (cmd_state_t *cmd, const char *partial);

int Cmd_CompleteCountPossible (cmd_state_t *cmd, const char *partial);

const char **Cmd_CompleteBuildList (cmd_state_t *cmd, const char *partial);

void Cmd_CompleteCommandPrint (cmd_state_t *cmd, const char *partial);

const char *Cmd_CompleteAlias (cmd_state_t *cmd, const char *partial);

void Cmd_CompleteAliasPrint (cmd_state_t *cmd, const char *partial);

// Enhanced console completion by Fett erich@heintz.com

// Added by EvilTypeGuy eviltypeguy@qeradiant.com

int Cmd_Argc (cmd_state_t *cmd);
const char *Cmd_Argv (cmd_state_t *cmd, int arg);
const char *Cmd_Args (cmd_state_t *cmd);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv(cmd, ) will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

/// Returns the position (1 to argc-1) in the command's argument list
/// where the given parameter apears, or 0 if not present
int Cmd_CheckParm (cmd_state_t *cmd, const char *parm);

//void Cmd_TokenizeString (char *text);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

/// Parses a single line of text into arguments and tries to execute it.
/// The text can come from the command buffer, a remote client, or stdin.
void Cmd_ExecuteString (cmd_state_t *cmd, const char *text, cmd_source_t src, qboolean lockmutex);

/// quotes a string so that it can be used as a command argument again;
/// quoteset is a string that contains one or more of ", \, $ and specifies
/// the characters to be quoted (you usually want to either pass "\"\\" or
/// "\"\\$"). Returns true on success, and false on overrun (in which case out
/// will contain a part of the quoted string). If putquotes is set, the
/// enclosing quote marks are also put.
qboolean Cmd_QuoteString(char *out, size_t outlen, const char *in, const char *quoteset, qboolean putquotes);

void Cmd_ClearCSQCCommands (cmd_state_t *cmd);

#endif

