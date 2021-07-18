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

#include "qtypes.h"
#include "qdefs.h"
#include "com_list.h"

struct cmd_state_s;

// Command flags
#define CF_NONE 0
#define CF_CLIENT               (1<<0)  // cvar/command that only the client can change/execute
#define CF_SERVER               (1<<1)  // cvar/command that only the server can change/execute
#define CF_CLIENT_FROM_SERVER   (1<<2)  // command that the server is allowed to execute on the client
#define CF_SERVER_FROM_CLIENT   (1<<3)  // command the client is allowed to execute on the server as a stringcmd
#define CF_CHEAT                (1<<4)  // command or cvar that gives an unfair advantage over other players and is blocked unless sv_cheats is 1
#define CF_ARCHIVE              (1<<5)  // cvar should have its set value saved to config.cfg and persist across sessions
#define CF_READONLY             (1<<6)  // cvar cannot be changed from the console or the command buffer
#define CF_NOTIFY               (1<<7)  // cvar should trigger a chat notification to all connected clients when changed
#define CF_SERVERINFO           (1<<8)  // command or cvar relevant to serverinfo string handling
#define CF_USERINFO             (1<<9)  // command or cvar used to communicate userinfo to the server
#define CF_PERSISTENT           (1<<10) // cvar must not be reset on gametype switch (such as scr_screenshot_name, which otherwise isn't set to the mod name properly)
#define CF_PRIVATE              (1<<11) // cvar should not be $ expanded or sent to the server under any circumstances (rcon_password, etc)
#define CF_MAXFLAGSVAL          ((1<<12) - 1)    // used to determine if flags is valid
// for internal use only!
#define CF_DEFAULTSET (1<<30)
#define CF_ALLOCATED (1<<31)

#define CF_SHARED 3

typedef void(*xcommand_t) (struct cmd_state_s *cmd);

typedef enum cmd_source_s
{
	src_client,		///< came in over a net connection as a clc_stringcmd
					///< host_client will be valid during this state.
	src_local		///< from the command buffer
} cmd_source_t;

typedef struct cmd_alias_s
{
	struct cmd_alias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
	qbool initstate; // indicates this command existed at init
	char *initialvalue; // backup copy of value at init
} cmd_alias_t;

typedef struct cmd_function_s
{
	int flags;
	struct cmd_function_s *next;
	const char *name;
	const char *description;
	xcommand_t function;
	qbool qcfunc;
	qbool autofunc;
	qbool initstate; // indicates this command existed at init
} cmd_function_t;

/// container for user-defined QC functions and aliases, shared between different command interpreters
typedef struct cmd_userdefined_s
{
	// csqc functions - this is a mess
	cmd_function_t *qc_functions;

	// aliases
	cmd_alias_t *alias;
}
cmd_userdefined_t;

typedef struct cmd_buf_s
{
	llist_t start;
	llist_t deferred;
	llist_t free;
	qbool wait;
	size_t maxsize;
	size_t size;
	char tokenizebuffer[CMD_TOKENIZELENGTH];
	int tokenizebufferpos;
	double deferred_oldtime;
	void *lock;
} cmd_buf_t;

/// command interpreter state - the tokenizing and execution of commands, as well as pointers to which cvars and aliases they can access
typedef struct cmd_state_s
{
	struct mempool_s *mempool;

	int argc;
	const char *cmdline;
	const char *argv[MAX_ARGS];
	const char *null_string;
	const char *args;
	cmd_source_t source;

	cmd_buf_t *cbuf;

	cmd_userdefined_t *userdefined; // possible csqc functions and aliases to execute

	cmd_function_t *engine_functions;

	struct cvar_state_s *cvars; // which cvar system is this cmd state able to access? (&cvars_all or &cvars_null)
	int cvars_flagsmask; // which CVAR_* flags should be visible to this interpreter? (CF_CLIENT | CF_SERVER, or just CF_SERVER)

	int cmd_flags; // cmd flags that identify this interpreter

	qbool (*Handle)(struct cmd_state_s *, struct cmd_function_s *, const char *, enum cmd_source_s);
	qbool (*NotFound)(struct cmd_state_s *, struct cmd_function_s *, const char *, enum cmd_source_s);
}
cmd_state_t;

qbool Cmd_Callback(cmd_state_t *cmd, cmd_function_t *func, const char *text, cmd_source_t src);
qbool Cmd_CL_Callback(cmd_state_t *cmd, cmd_function_t *func, const char *text, cmd_source_t src);
qbool Cmd_SV_Callback(cmd_state_t *cmd, cmd_function_t *func, const char *text, cmd_source_t src);
qbool Cmd_SV_NotFound(cmd_state_t *cmd, cmd_function_t *func, const char *text, cmd_source_t src);

typedef struct cmd_input_s
{
	llist_t list;
	cmd_state_t *source;
	double delay;
	size_t size;
	size_t length;
	char *text;
	qbool pending;
} cmd_input_t;

extern cmd_userdefined_t cmd_userdefined_all; // aliases and csqc functions
extern cmd_userdefined_t cmd_userdefined_null; // intentionally empty

// command interpreter for local commands injected by SVQC, CSQC, MQC, server or client engine code
// uses cmddefs_all
extern cmd_state_t *cmd_local;
// command interpreter for server commands received over network from clients
// uses cmddefs_null
extern cmd_state_t *cmd_serverfromclient;

extern qbool host_stuffcmdsrun;

void Cbuf_Lock(cmd_buf_t *cbuf);
void Cbuf_Unlock(cmd_buf_t *cbuf);

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
void Cbuf_Execute (cmd_buf_t *cbuf);
/*! Performs deferred commands and runs Cbuf_Execute, called by Host_Frame */
void Cbuf_Frame (cmd_buf_t *cbuf);

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
qbool Cmd_Exists (cmd_state_t *cmd, const char *cmd_name);

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
void Cmd_ExecuteString (cmd_state_t *cmd, const char *text, cmd_source_t src, qbool lockmutex);

/// quotes a string so that it can be used as a command argument again;
/// quoteset is a string that contains one or more of ", \, $ and specifies
/// the characters to be quoted (you usually want to either pass "\"\\" or
/// "\"\\$"). Returns true on success, and false on overrun (in which case out
/// will contain a part of the quoted string). If putquotes is set, the
/// enclosing quote marks are also put.
qbool Cmd_QuoteString(char *out, size_t outlen, const char *in, const char *quoteset, qbool putquotes);

void Cmd_ClearCSQCCommands (cmd_state_t *cmd);

void Cmd_NoOperation_f(cmd_state_t *cmd);

#endif

