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

/// allocates an initial text buffer that will grow as needed
void Cbuf_Init (void);

void Cmd_Init_Commands (void);

void Cbuf_Shutdown (void);

/*! as new commands are generated from the console or keybindings,
 * the text is added to the end of the command buffer.
 */
void Cbuf_AddText (const char *text);

/*! when a command wants to issue other commands immediately, the text is
 * inserted at the beginning of the buffer, before any remaining unexecuted
 * commands.
 */
void Cbuf_InsertText (const char *text);

/*! Pulls off terminated lines of text from the command buffer and sends
 * them through Cmd_ExecuteString.  Stops when the buffer is empty.
 * Normally called once per frame, but may be explicitly invoked.
 * \note Do not call inside a command function!
 */
void Cbuf_Execute (void);

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

typedef void (*xcommand_t) (void);

typedef enum
{
	src_client,		///< came in over a net connection as a clc_stringcmd
					///< host_client will be valid during this state.
	src_command		///< from the command buffer
} cmd_source_t;

extern cmd_source_t cmd_source;

void Cmd_Init (void);
void Cmd_Shutdown (void);

// called by Host_Init, this marks cvars, commands and aliases with their init values
void Cmd_SaveInitState (void);
// called by FS_GameDir_f, this restores cvars, commands and aliases to init values
void Cmd_RestoreInitState (void);

void Cmd_AddCommand_WithClientCommand (const char *cmd_name, xcommand_t consolefunction, xcommand_t clientfunction, const char *description);
void Cmd_AddCommand (const char *cmd_name, xcommand_t function, const char *description);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory

/// used by the cvar code to check for cvar / command name overlap
qboolean Cmd_Exists (const char *cmd_name);

/// attempts to match a partial command for automatic command line completion
/// returns NULL if nothing fits
const char *Cmd_CompleteCommand (const char *partial);

int Cmd_CompleteAliasCountPossible (const char *partial);

const char **Cmd_CompleteAliasBuildList (const char *partial);

int Cmd_CompleteCountPossible (const char *partial);

const char **Cmd_CompleteBuildList (const char *partial);

void Cmd_CompleteCommandPrint (const char *partial);

const char *Cmd_CompleteAlias (const char *partial);

void Cmd_CompleteAliasPrint (const char *partial);

// Enhanced console completion by Fett erich@heintz.com

// Added by EvilTypeGuy eviltypeguy@qeradiant.com

int Cmd_Argc (void);
const char *Cmd_Argv (int arg);
const char *Cmd_Args (void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

/// Returns the position (1 to argc-1) in the command's argument list
/// where the given parameter apears, or 0 if not present
int Cmd_CheckParm (const char *parm);

//void Cmd_TokenizeString (char *text);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

/// Parses a single line of text into arguments and tries to execute it.
/// The text can come from the command buffer, a remote client, or stdin.
void Cmd_ExecuteString (const char *text, cmd_source_t src);

/// adds the string as a clc_stringcmd to the client message.
/// (used when there is no reason to generate a local command to do it)
void Cmd_ForwardStringToServer (const char *s);

/// adds the current command line as a clc_stringcmd to the client message.
/// things like godmode, noclip, etc, are commands directed to the server,
/// so when they are typed in at the console, they will need to be forwarded.
void Cmd_ForwardToServer (void);

/// used by command functions to send output to either the graphics console or
/// passed as a print message to the client
void Cmd_Print(const char *text);

/// quotes a string so that it can be used as a command argument again;
/// quoteset is a string that contains one or more of ", \, $ and specifies
/// the characters to be quoted (you usually want to either pass "\"\\" or
/// "\"\\$"). Returns true on success, and false on overrun (in which case out
/// will contain a part of the quoted string). If putquotes is set, the
/// enclosing quote marks are also put.
qboolean Cmd_QuoteString(char *out, size_t outlen, const char *in, const char *quoteset, qboolean putquotes);

#endif

