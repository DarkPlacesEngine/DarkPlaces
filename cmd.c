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
#include "thread.h"

cmd_state_t cmd_client;
cmd_state_t cmd_clientfromserver;
cmd_state_t cmd_server;
cmd_state_t cmd_serverfromclient;

cmd_userdefined_t cmd_userdefined_all;
cmd_userdefined_t cmd_userdefined_null;

typedef struct cmd_iter_s {
	cmd_state_t *cmd;
}
cmd_iter_t;

static cmd_iter_t cmd_iter_all[] = {
	{&cmd_client},
	{&cmd_clientfromserver},
	{&cmd_server},
	{&cmd_serverfromclient},
	{NULL},
};


// we only run the +whatever commandline arguments once
qboolean host_stuffcmdsrun = false;

//=============================================================================

void Cbuf_Lock(cmd_state_t *cmd)
{
	Thread_AtomicLock(&cmd->text_lock);
}

void Cbuf_Unlock(cmd_state_t *cmd)
{
	Thread_AtomicUnlock(&cmd->text_lock);
}


/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
static void Cmd_Wait_f (cmd_state_t *cmd)
{
	cmd->wait = true;
}

/*
============
Cmd_Defer_f

Cause a command to be executed after a delay.
============
*/
static void Cmd_Defer_f (cmd_state_t *cmd)
{
	if(Cmd_Argc(cmd) == 1)
	{
		cmddeferred_t *next = cmd->deferred_list;
		if(!next)
			Con_Printf("No commands are pending.\n");
		while(next)
		{
			Con_Printf("-> In %9.2f: %s\n", next->delay, next->value);
			next = next->next;
		}
	} else if(Cmd_Argc(cmd) == 2 && !strcasecmp("clear", Cmd_Argv(cmd, 1)))
	{
		while(cmd->deferred_list)
		{
			cmddeferred_t *defcmd = cmd->deferred_list;
			cmd->deferred_list = defcmd->next;
			Mem_Free(defcmd->value);
			Mem_Free(defcmd);
		}
	} else if(Cmd_Argc(cmd) == 3)
	{
		const char *value = Cmd_Argv(cmd, 2);
		cmddeferred_t *defcmd = (cmddeferred_t*)Mem_Alloc(tempmempool, sizeof(*defcmd));
		size_t len = strlen(value);

		defcmd->delay = atof(Cmd_Argv(cmd, 1));
		defcmd->value = (char*)Mem_Alloc(tempmempool, len+1);
		memcpy(defcmd->value, value, len+1);
		defcmd->next = NULL;

		if(cmd->deferred_list)
		{
			cmddeferred_t *next = cmd->deferred_list;
			while(next->next)
				next = next->next;
			next->next = defcmd;
		} else
			cmd->deferred_list = defcmd;
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
static void Cmd_Centerprint_f (cmd_state_t *cmd)
{
	char msg[MAX_INPUTLINE];
	unsigned int i, c, p;
	c = Cmd_Argc(cmd);
	if(c >= 2)
	{
		strlcpy(msg, Cmd_Argv(cmd,1), sizeof(msg));
		for(i = 2; i < c; ++i)
		{
			strlcat(msg, " ", sizeof(msg));
			strlcat(msg, Cmd_Argv(cmd, i), sizeof(msg));
		}
		c = (unsigned int)strlen(msg);
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

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (cmd_state_t *cmd, const char *text)
{
	int		l;

	l = (int)strlen(text);

	Cbuf_Lock(cmd);
	if (cmd->text.maxsize - cmd->text.cursize <= l)
		Con_Print("Cbuf_AddText: overflow\n");
	else
		SZ_Write(&cmd->text, (const unsigned char *)text, l);
	Cbuf_Unlock(cmd);
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (cmd_state_t *cmd, const char *text)
{
	size_t l = strlen(text);
	Cbuf_Lock(cmd);
	// we need to memmove the existing text and stuff this in before it...
	if (cmd->text.cursize + l >= (size_t)cmd->text.maxsize)
		Con_Print("Cbuf_InsertText: overflow\n");
	else
	{
		// we don't have a SZ_Prepend, so...
		memmove(cmd->text.data + l, cmd->text.data, cmd->text.cursize);
		cmd->text.cursize += (int)l;
		memcpy(cmd->text.data, text, l);
	}
	Cbuf_Unlock(cmd);
}

/*
============
Cbuf_Execute_Deferred --blub
============
*/
static void Cbuf_Execute_Deferred (cmd_state_t *cmd)
{
	cmddeferred_t *defcmd, *prev;
	double eat;
	if (realtime - cmd->deferred_oldrealtime < 0 || realtime - cmd->deferred_oldrealtime > 1800) cmd->deferred_oldrealtime = realtime;
	eat = realtime - cmd->deferred_oldrealtime;
	if (eat < (1.0 / 120.0))
		return;
	cmd->deferred_oldrealtime = realtime;
	prev = NULL;
	defcmd = cmd->deferred_list;
	while(defcmd)
	{
		defcmd->delay -= eat;
		if(defcmd->delay <= 0)
		{
			Cbuf_AddText(cmd, defcmd->value);
			Cbuf_AddText(cmd, ";\n");
			Mem_Free(defcmd->value);

			if(prev) {
				prev->next = defcmd->next;
				Mem_Free(defcmd);
				defcmd = prev->next;
			} else {
				cmd->deferred_list = defcmd->next;
				Mem_Free(defcmd);
				defcmd = cmd->deferred_list;
			}
			continue;
		}
		prev = defcmd;
		defcmd = defcmd->next;
	}
}

/*
============
Cbuf_Execute
============
*/
static qboolean Cmd_PreprocessString(cmd_state_t *cmd, const char *intext, char *outtext, unsigned maxoutlen, cmdalias_t *alias );
void Cbuf_Execute (cmd_state_t *cmd)
{
	int i;
	char *text;
	char line[MAX_INPUTLINE];
	char preprocessed[MAX_INPUTLINE];
	char *firstchar;
	qboolean quotes;
	char *comment;

	// LadyHavoc: making sure the tokenizebuffer doesn't get filled up by repeated crashes
	cmd->tokenizebufferpos = 0;

	while (cmd->text.cursize)
	{
// find a \n or ; line break
		text = (char *)cmd->text.data;

		quotes = false;
		comment = NULL;
		for (i=0 ; i < cmd->text.cursize ; i++)
		{
			if(!comment)
			{
				if (text[i] == '"')
					quotes = !quotes;

				if(quotes)
				{
					// make sure i doesn't get > cursize which causes a negative
					// size in memmove, which is fatal --blub
					if (i < (cmd->text.cursize-1) && (text[i] == '\\' && (text[i+1] == '"' || text[i+1] == '\\')))
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
			Con_Warnf("Warning: console input buffer had an overlong line. Ignored.\n");
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

		if (i == cmd->text.cursize)
			cmd->text.cursize = 0;
		else
		{
			i++;
			cmd->text.cursize -= i;
			memmove (cmd->text.data, text+i, cmd->text.cursize);
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
			if(Cmd_PreprocessString( cmd, line, preprocessed, sizeof(preprocessed), NULL ))
				Cmd_ExecuteString (cmd, preprocessed, src_command, false);
		}
		else
		{
			Cmd_ExecuteString (cmd, line, src_command, false);
		}

		if (cmd->wait)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd->wait = false;
			break;
		}
	}
}

void Cbuf_Frame(cmd_state_t *cmd)
{
	Cbuf_Execute_Deferred(cmd);
	if (cmd->text.cursize)
	{
		SV_LockThreadMutex();
		Cbuf_Execute(cmd);
		SV_UnlockThreadMutex();
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

extern qboolean host_init;

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
static void Cmd_StuffCmds_f (cmd_state_t *cmd)
{
	int		i, j, l;
	// this is for all commandline options combined (and is bounds checked)
	char	build[MAX_INPUTLINE];

	// come back later so we don't crash
	if(host_init)
		return;

	if (Cmd_Argc (cmd) != 1)
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
	Cbuf_InsertText (cmd, build);
}

static void Cmd_Exec(cmd_state_t *cmd, const char *filename)
{
	char *f;
	size_t filenameLen = strlen(filename);
	qboolean isdefaultcfg =
		!strcmp(filename, "default.cfg") ||
		(filenameLen >= 12 && !strcmp(filename + filenameLen - 12, "/default.cfg"));

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
	if (isdefaultcfg)
		Cbuf_InsertText(cmd, "\ncvar_lockdefaults\n");

	// insert newline after the text to make sure the last line is terminated (some text editors omit the trailing newline)
	// (note: insertion order here is backwards from execution order, so this adds it after the text, by calling it before...)
	Cbuf_InsertText (cmd, "\n");
	Cbuf_InsertText (cmd, f);
	Mem_Free(f);

	if (isdefaultcfg)
	{
		// special defaults for specific games go here, these execute before default.cfg
		// Nehahra pushable crates malfunction in some levels if this is on
		// Nehahra NPC AI is confused by blowupfallenzombies
		switch(gamemode)
		{
		case GAME_NORMAL:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 0\n"
"sv_gameplayfix_findradiusdistancetobox 0\n"
"sv_gameplayfix_grenadebouncedownslopes 0\n"
"sv_gameplayfix_slidemoveprojectiles 0\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 0\n"
"sv_gameplayfix_setmodelrealbox 0\n"
"sv_gameplayfix_droptofloorstartsolid 0\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 0\n"
"sv_gameplayfix_noairborncorpse 0\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 0\n"
"sv_gameplayfix_easierwaterjump 0\n"
"sv_gameplayfix_delayprojectiles 0\n"
"sv_gameplayfix_multiplethinksperframe 0\n"
"sv_gameplayfix_fixedcheckwatertransition 0\n"
"sv_gameplayfix_q1bsptracelinereportstexture 0\n"
"sv_gameplayfix_swiminbmodels 0\n"
"sv_gameplayfix_downtracesupportsongroundflag 0\n"
"sys_ticrate 0.01388889\n"
"r_shadow_gloss 1\n"
"r_shadow_bumpscale_basetexture 0\n"
"csqc_polygons_defaultmaterial_nocullface 0\n"
				);
			break;
		case GAME_NEHAHRA:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 0\n"
"sv_gameplayfix_findradiusdistancetobox 0\n"
"sv_gameplayfix_grenadebouncedownslopes 0\n"
"sv_gameplayfix_slidemoveprojectiles 0\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 0\n"
"sv_gameplayfix_setmodelrealbox 0\n"
"sv_gameplayfix_droptofloorstartsolid 0\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 0\n"
"sv_gameplayfix_noairborncorpse 0\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 0\n"
"sv_gameplayfix_easierwaterjump 0\n"
"sv_gameplayfix_delayprojectiles 0\n"
"sv_gameplayfix_multiplethinksperframe 0\n"
"sv_gameplayfix_fixedcheckwatertransition 0\n"
"sv_gameplayfix_q1bsptracelinereportstexture 0\n"
"sv_gameplayfix_swiminbmodels 0\n"
"sv_gameplayfix_downtracesupportsongroundflag 0\n"
"sys_ticrate 0.01388889\n"
"r_shadow_gloss 1\n"
"r_shadow_bumpscale_basetexture 0\n"
"csqc_polygons_defaultmaterial_nocullface 0\n"
				);
			break;
		// hipnotic mission pack has issues in their 'friendly monster' ai, which seem to attempt to attack themselves for some reason when findradius() returns non-solid entities.
		// hipnotic mission pack has issues with bobbing water entities 'jittering' between different heights on alternate frames at the default 0.0138889 ticrate, 0.02 avoids this issue
		// hipnotic mission pack has issues in their proximity mine sticking code, which causes them to bounce off.
		case GAME_HIPNOTIC:
		case GAME_QUOTH:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 0\n"
"sv_gameplayfix_findradiusdistancetobox 0\n"
"sv_gameplayfix_grenadebouncedownslopes 0\n"
"sv_gameplayfix_slidemoveprojectiles 0\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 0\n"
"sv_gameplayfix_setmodelrealbox 0\n"
"sv_gameplayfix_droptofloorstartsolid 0\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 0\n"
"sv_gameplayfix_noairborncorpse 0\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 0\n"
"sv_gameplayfix_easierwaterjump 0\n"
"sv_gameplayfix_delayprojectiles 0\n"
"sv_gameplayfix_multiplethinksperframe 0\n"
"sv_gameplayfix_fixedcheckwatertransition 0\n"
"sv_gameplayfix_q1bsptracelinereportstexture 0\n"
"sv_gameplayfix_swiminbmodels 0\n"
"sv_gameplayfix_downtracesupportsongroundflag 0\n"
"sys_ticrate 0.02\n"
"r_shadow_gloss 1\n"
"r_shadow_bumpscale_basetexture 0\n"
"csqc_polygons_defaultmaterial_nocullface 0\n"
				);
			break;
		// rogue mission pack has a guardian boss that does not wake up if findradius returns one of the entities around its spawn area
		case GAME_ROGUE:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 0\n"
"sv_gameplayfix_findradiusdistancetobox 0\n"
"sv_gameplayfix_grenadebouncedownslopes 0\n"
"sv_gameplayfix_slidemoveprojectiles 0\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 0\n"
"sv_gameplayfix_setmodelrealbox 0\n"
"sv_gameplayfix_droptofloorstartsolid 0\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 0\n"
"sv_gameplayfix_noairborncorpse 0\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 0\n"
"sv_gameplayfix_easierwaterjump 0\n"
"sv_gameplayfix_delayprojectiles 0\n"
"sv_gameplayfix_multiplethinksperframe 0\n"
"sv_gameplayfix_fixedcheckwatertransition 0\n"
"sv_gameplayfix_q1bsptracelinereportstexture 0\n"
"sv_gameplayfix_swiminbmodels 0\n"
"sv_gameplayfix_downtracesupportsongroundflag 0\n"
"sys_ticrate 0.01388889\n"
"r_shadow_gloss 1\n"
"r_shadow_bumpscale_basetexture 0\n"
"csqc_polygons_defaultmaterial_nocullface 0\n"
				);
			break;
		case GAME_TENEBRAE:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 0\n"
"sv_gameplayfix_findradiusdistancetobox 0\n"
"sv_gameplayfix_grenadebouncedownslopes 0\n"
"sv_gameplayfix_slidemoveprojectiles 0\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 0\n"
"sv_gameplayfix_setmodelrealbox 0\n"
"sv_gameplayfix_droptofloorstartsolid 0\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 0\n"
"sv_gameplayfix_noairborncorpse 0\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 0\n"
"sv_gameplayfix_easierwaterjump 0\n"
"sv_gameplayfix_delayprojectiles 0\n"
"sv_gameplayfix_multiplethinksperframe 0\n"
"sv_gameplayfix_fixedcheckwatertransition 0\n"
"sv_gameplayfix_q1bsptracelinereportstexture 0\n"
"sv_gameplayfix_swiminbmodels 0\n"
"sv_gameplayfix_downtracesupportsongroundflag 0\n"
"sys_ticrate 0.01388889\n"
"r_shadow_gloss 2\n"
"r_shadow_bumpscale_basetexture 4\n"
"csqc_polygons_defaultmaterial_nocullface 0\n"
				);
			break;
		case GAME_NEXUIZ:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 1\n"
"sv_gameplayfix_findradiusdistancetobox 1\n"
"sv_gameplayfix_grenadebouncedownslopes 1\n"
"sv_gameplayfix_slidemoveprojectiles 1\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 1\n"
"sv_gameplayfix_setmodelrealbox 1\n"
"sv_gameplayfix_droptofloorstartsolid 1\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 1\n"
"sv_gameplayfix_noairborncorpse 1\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 1\n"
"sv_gameplayfix_easierwaterjump 1\n"
"sv_gameplayfix_delayprojectiles 1\n"
"sv_gameplayfix_multiplethinksperframe 1\n"
"sv_gameplayfix_fixedcheckwatertransition 1\n"
"sv_gameplayfix_q1bsptracelinereportstexture 1\n"
"sv_gameplayfix_swiminbmodels 1\n"
"sv_gameplayfix_downtracesupportsongroundflag 1\n"
"sys_ticrate 0.01388889\n"
"sv_gameplayfix_q2airaccelerate 1\n"
"sv_gameplayfix_stepmultipletimes 1\n"
"csqc_polygons_defaultmaterial_nocullface 1\n"
				);
			break;
		case GAME_XONOTIC:
		case GAME_VORETOURNAMENT:
			// compatibility for versions prior to 2020-05-25, this can be overridden in newer versions to get the default behavior and be consistent with FTEQW engine
			Cbuf_InsertText(cmd, "\n"
"csqc_polygons_defaultmaterial_nocullface 1\n"
				);
			break;
		// Steel Storm: Burning Retribution csqc misinterprets CSQC_InputEvent if type is a value other than 0 or 1
		case GAME_STEELSTORM:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 1\n"
"sv_gameplayfix_findradiusdistancetobox 1\n"
"sv_gameplayfix_grenadebouncedownslopes 1\n"
"sv_gameplayfix_slidemoveprojectiles 1\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 1\n"
"sv_gameplayfix_setmodelrealbox 1\n"
"sv_gameplayfix_droptofloorstartsolid 1\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 1\n"
"sv_gameplayfix_noairborncorpse 1\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 1\n"
"sv_gameplayfix_easierwaterjump 1\n"
"sv_gameplayfix_delayprojectiles 1\n"
"sv_gameplayfix_multiplethinksperframe 1\n"
"sv_gameplayfix_fixedcheckwatertransition 1\n"
"sv_gameplayfix_q1bsptracelinereportstexture 1\n"
"sv_gameplayfix_swiminbmodels 1\n"
"sv_gameplayfix_downtracesupportsongroundflag 1\n"
"sys_ticrate 0.01388889\n"
"cl_csqc_generatemousemoveevents 0\n"
"csqc_polygons_defaultmaterial_nocullface 1\n"
				);
			break;
		default:
			Cbuf_InsertText(cmd, "\n"
"sv_gameplayfix_blowupfallenzombies 1\n"
"sv_gameplayfix_findradiusdistancetobox 1\n"
"sv_gameplayfix_grenadebouncedownslopes 1\n"
"sv_gameplayfix_slidemoveprojectiles 1\n"
"sv_gameplayfix_upwardvelocityclearsongroundflag 1\n"
"sv_gameplayfix_setmodelrealbox 1\n"
"sv_gameplayfix_droptofloorstartsolid 1\n"
"sv_gameplayfix_droptofloorstartsolid_nudgetocorrect 1\n"
"sv_gameplayfix_noairborncorpse 1\n"
"sv_gameplayfix_noairborncorpse_allowsuspendeditems 1\n"
"sv_gameplayfix_easierwaterjump 1\n"
"sv_gameplayfix_delayprojectiles 1\n"
"sv_gameplayfix_multiplethinksperframe 1\n"
"sv_gameplayfix_fixedcheckwatertransition 1\n"
"sv_gameplayfix_q1bsptracelinereportstexture 1\n"
"sv_gameplayfix_swiminbmodels 1\n"
"sv_gameplayfix_downtracesupportsongroundflag 1\n"
"sys_ticrate 0.01388889\n"
"csqc_polygons_defaultmaterial_nocullface 0\n"
				);
			break;
		}
	}
}

/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f (cmd_state_t *cmd)
{
	fssearch_t *s;
	int i;

	if (Cmd_Argc(cmd) != 2)
	{
		Con_Print("exec <filename> : execute a script file\n");
		return;
	}

	s = FS_Search(Cmd_Argv(cmd, 1), true, true);
	if(!s || !s->numfilenames)
	{
		Con_Printf("couldn't exec %s\n",Cmd_Argv(cmd, 1));
		return;
	}

	for(i = 0; i < s->numfilenames; ++i)
		Cmd_Exec(cmd, s->filenames[i]);

	FS_FreeSearch(s);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
static void Cmd_Echo_f (cmd_state_t *cmd)
{
	int		i;

	for (i=1 ; i<Cmd_Argc(cmd) ; i++)
		Con_Printf("%s ",Cmd_Argv(cmd, i));
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
static void Cmd_Toggle_f(cmd_state_t *cmd)
{
	// Acquire Number of Arguments
	int nNumArgs = Cmd_Argc(cmd);

	if(nNumArgs == 1)
		// No Arguments Specified; Print Usage
		Con_Print("Toggle Console Variable - Usage\n  toggle <variable> - toggles between 0 and 1\n  toggle <variable> <value> - toggles between 0 and <value>\n  toggle <variable> [string 1] [string 2]...[string n] - cycles through all strings\n");
	else
	{ // Correct Arguments Specified
		// Acquire Potential CVar
		cvar_t* cvCVar = Cvar_FindVar(cmd->cvars, Cmd_Argv(cmd, 1), cmd->cvars_flagsmask);

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
				if(cvCVar->integer == atoi(Cmd_Argv(cmd, 2) ) )
					// CVar is Specified Value; // Reset to 0
					Cvar_SetValueQuick(cvCVar, 0);
				else
				if(cvCVar->integer == 0)
					// CVar is 0; Specify Value
					Cvar_SetQuick(cvCVar, Cmd_Argv(cmd, 2) );
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
					if( strcmp(cvCVar->string, Cmd_Argv(cmd, nCnt) ) == 0)
					{ // Current Value Located; Increment to Next
						if( (nCnt + 1) == nNumArgs)
							// Max Value Reached; Reset
							Cvar_SetQuick(cvCVar, Cmd_Argv(cmd, 2) );
						else
							// Next Value
							Cvar_SetQuick(cvCVar, Cmd_Argv(cmd, nCnt + 1) );

						// End Loop
						nCnt = nNumArgs;
						// Assign Found
						bFound = 1;
					}
				}
				if(!bFound)
					// Value not Found; Reset to Original
					Cvar_SetQuick(cvCVar, Cmd_Argv(cmd, 2) );
			}

		}
		else
		{ // Invalid CVar
			Con_Printf("ERROR : CVar '%s' not found\n", Cmd_Argv(cmd, 1) );
		}
	}
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
static void Cmd_Alias_f (cmd_state_t *cmd)
{
	cmdalias_t	*a;
	char		line[MAX_INPUTLINE];
	int			i, c;
	const char		*s;
	size_t		alloclen;

	if (Cmd_Argc(cmd) == 1)
	{
		Con_Print("Current alias commands:\n");
		for (a = cmd->userdefined->alias ; a ; a=a->next)
			Con_Printf("%s : %s", a->name, a->value);
		return;
	}

	s = Cmd_Argv(cmd, 1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Con_Print("Alias name is too long\n");
		return;
	}

	// if the alias already exists, reuse it
	for (a = cmd->userdefined->alias ; a ; a=a->next)
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
		for( prev = NULL, current = cmd->userdefined->alias ; current && strcmp( current->name, a->name ) < 0 ; prev = current, current = current->next )
			;
		if( prev ) {
			prev->next = a;
		} else {
			cmd->userdefined->alias = a;
		}
		a->next = current;
	}


// copy the rest of the command line
	line[0] = 0;		// start out with a null string
	c = Cmd_Argc(cmd);
	for (i=2 ; i < c ; i++)
	{
		if (i != 2)
			strlcat (line, " ", sizeof (line));
		strlcat (line, Cmd_Argv(cmd, i), sizeof (line));
	}
	strlcat (line, "\n", sizeof (line));

	alloclen = strlen (line) + 1;
	if(alloclen >= 2)
		line[alloclen - 2] = '\n'; // to make sure a newline is appended even if too long
	a->value = (char *)Z_Malloc (alloclen);
	memcpy (a->value, line, alloclen);
}

/*
===============
Cmd_UnAlias_f

Remove existing aliases.
===============
*/
static void Cmd_UnAlias_f (cmd_state_t *cmd)
{
	cmdalias_t	*a, *p;
	int i;
	const char *s;

	if(Cmd_Argc(cmd) == 1)
	{
		Con_Print("unalias: Usage: unalias alias1 [alias2 ...]\n");
		return;
	}

	for(i = 1; i < Cmd_Argc(cmd); ++i)
	{
		s = Cmd_Argv(cmd, i);
		p = NULL;
		for(a = cmd->userdefined->alias; a; p = a, a = a->next)
		{
			if(!strcmp(s, a->name))
			{
				if (a->initstate) // we can not remove init aliases
					continue;
				if(a == cmd->userdefined->alias)
					cmd->userdefined->alias = a->next;
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

static const char *Cmd_GetDirectCvarValue(cmd_state_t *cmd, const char *varname, cmdalias_t *alias, qboolean *is_multiple)
{
	cvar_t *cvar;
	long argno;
	char *endptr;
	static char vabuf[1024]; // cmd_mutex

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
			return Cmd_Args(cmd);
		}
		else if(!strcmp(varname, "#"))
		{
			return va(vabuf, sizeof(vabuf), "%d", Cmd_Argc(cmd));
		}
		else if(varname[strlen(varname) - 1] == '-')
		{
			argno = strtol(varname, &endptr, 10);
			if(endptr == varname + strlen(varname) - 1)
			{
				// whole string is a number, apart from the -
				const char *p = Cmd_Args(cmd);
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
				if(argno >= 0 && argno < Cmd_Argc(cmd))
					return Cmd_Argv(cmd, argno);
			}
		}
	}

	if((cvar = Cvar_FindVar(cmd->cvars, varname, cmd->cvars_flagsmask)) && !(cvar->flags & CVAR_PRIVATE))
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

static const char *Cmd_GetCvarValue(cmd_state_t *cmd, const char *var, size_t varlen, cmdalias_t *alias)
{
	static char varname[MAX_INPUTLINE]; // cmd_mutex
	static char varval[MAX_INPUTLINE]; // cmd_mutex
	const char *varstr = NULL;
	char *varfunc;
	qboolean required = false;
	qboolean optional = false;
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
		if(alias)
			Con_Warnf("Warning: Could not expand $ in alias %s\n", alias->name);
		else
			Con_Warnf("Warning: Could not expand $\n");
		return "$";
	}

	if(varfunc)
	{
		char *p;
		// ? means optional
		while((p = strchr(varfunc, '?')))
		{
			optional = true;
			memmove(p, p+1, strlen(p)); // with final NUL
		}
		// ! means required
		while((p = strchr(varfunc, '!')))
		{
			required = true;
			memmove(p, p+1, strlen(p)); // with final NUL
		}
		// kill spaces
		while((p = strchr(varfunc, ' ')))
		{
			memmove(p, p+1, strlen(p)); // with final NUL
		}
		// if no function is left, NULL it
		if(!*varfunc)
			varfunc = NULL;
	}

	if(varname[0] == '$')
		varstr = Cmd_GetDirectCvarValue(cmd, Cmd_GetDirectCvarValue(cmd, varname + 1, alias, NULL), alias, NULL);
	else
	{
		qboolean is_multiple = false;
		// Exception: $* and $n- don't use the quoted form by default
		varstr = Cmd_GetDirectCvarValue(cmd, varname, alias, &is_multiple);
		if(is_multiple)
			if(!varfunc)
				varfunc = asis;
	}

	if(!varstr)
	{
		if(required)
		{
			if(alias)
				Con_Errorf("Error: Could not expand $%s in alias %s\n", varname, alias->name);
			else
				Con_Errorf("Error: Could not expand $%s\n", varname);
			return NULL;
		}
		else if(optional)
		{
			return "";
		}
		else
		{
			if(alias)
				Con_Warnf("Warning: Could not expand $%s in alias %s\n", varname, alias->name);
			else
				Con_Warnf("Warning: Could not expand $%s\n", varname);
			dpsnprintf(varval, sizeof(varval), "$%s", varname);
			return varval;
		}
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
static qboolean Cmd_PreprocessString(cmd_state_t *cmd, const char *intext, char *outtext, unsigned maxoutlen, cmdalias_t *alias ) {
	const char *in;
	size_t eat, varlen;
	unsigned outlen;
	const char *val;

	// don't crash if there's no room in the outtext buffer
	if( maxoutlen == 0 ) {
		return false;
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
			// - ${var ?} silently expands to the empty string if
			//   $var does not exist
			// - ${var !} fails expansion and executes nothing if
			//   $var does not exist
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
			// - ${* q} and ${n- q} force quoting anyway
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
					val = Cmd_GetCvarValue(cmd, in + 1, varlen, alias);
					if(!val)
						return false;
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
				val = Cmd_GetCvarValue(cmd, in, varlen, alias);
				if(!val)
					return false;
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
	return true;
}

/*
============
Cmd_ExecuteAlias

Called for aliases and fills in the alias into the cbuffer
============
*/
static void Cmd_ExecuteAlias (cmd_state_t *cmd, cmdalias_t *alias)
{
	static char buffer[ MAX_INPUTLINE ]; // cmd_mutex
	static char buffer2[ MAX_INPUTLINE ]; // cmd_mutex
	qboolean ret = Cmd_PreprocessString( cmd, alias->value, buffer, sizeof(buffer) - 2, alias );
	if(!ret)
		return;
	// insert at start of command buffer, so that aliases execute in order
	// (fixes bug introduced by Black on 20050705)

	// Note: Cbuf_PreprocessString will be called on this string AGAIN! So we
	// have to make sure that no second variable expansion takes place, otherwise
	// alias parameters containing dollar signs can have bad effects.
	Cmd_QuoteString(buffer2, sizeof(buffer2), buffer, "$", false);
	Cbuf_InsertText(cmd, buffer2);
}

/*
========
Cmd_List

	CmdList Added by EvilTypeGuy eviltypeguy@qeradiant.com
	Thanks to Matthias "Maddes" Buecher, http://www.inside3d.com/qip/

========
*/
static void Cmd_List_f (cmd_state_t *cmd)
{
	cmd_function_t *func;
	const char *partial;
	size_t len;
	int count;
	qboolean ispattern;

	if (Cmd_Argc(cmd) > 1)
	{
		partial = Cmd_Argv(cmd, 1);
		len = strlen(partial);
		ispattern = (strchr(partial, '*') || strchr(partial, '?'));
	}
	else
	{
		partial = NULL;
		len = 0;
		ispattern = false;
	}

	count = 0;
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
	{
		if (partial && (ispattern ? !matchpattern_with_separator(func->name, partial, false, "", false) : strncmp(partial, func->name, len)))
			continue;
		Con_Printf("%s : %s\n", func->name, func->description);
		count++;
	}
	for (func = cmd->engine_functions; func; func = func->next)
	{
		if (partial && (ispattern ? !matchpattern_with_separator(func->name, partial, false, "", false) : strncmp(partial, func->name, len)))
			continue;
		Con_Printf("%s : %s\n", func->name, func->description);
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

static void Cmd_Apropos_f(cmd_state_t *cmd)
{
	cmd_function_t *func;
	cvar_t *cvar;
	cmdalias_t *alias;
	const char *partial;
	int count;
	qboolean ispattern;
	char vabuf[1024];
	char *cvar_name;

	if (Cmd_Argc(cmd) > 1)
		partial = Cmd_Args(cmd);
	else
	{
		Con_Printf("usage: apropos <string>\n");
		return;
	}

	ispattern = partial && (strchr(partial, '*') || strchr(partial, '?'));
	if(!ispattern)
		partial = va(vabuf, sizeof(vabuf), "*%s*", partial);

	count = 0;
	for (cvar = cmd->cvars->vars; cvar; cvar = cvar->next)
	{
		if (!matchpattern_with_separator(cvar->name, partial, true, "", false) &&
		    !matchpattern_with_separator(cvar->description, partial, true, "", false))
		{
			for (int i = 0; i < cvar->aliasindex; i++)
			{
				if (!matchpattern_with_separator(cvar->aliases[i], partial, true, "", false)) {
					continue;
				} else {
					cvar_name = cvar->aliases[i];
					goto print;
				}
			}	
			continue;
		} else {
			cvar_name = (char *)cvar->name;
print:
			Con_Printf ("cvar ");
			Cvar_PrintHelp(cvar, cvar_name, true);
			count++;
		}
	}
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
	{
		if (!matchpattern_with_separator(func->name, partial, true, "", false))
			if (!matchpattern_with_separator(func->description, partial, true, "", false))
				continue;
		Con_Printf("command ^2%s^7: %s\n", func->name, func->description);
		count++;
	}
	for (func = cmd->engine_functions; func; func = func->next)
	{
		if (!matchpattern_with_separator(func->name, partial, true, "", false))
		if (!matchpattern_with_separator(func->description, partial, true, "", false))
			continue;
		Con_Printf("command ^2%s^7: %s\n", func->name, func->description);
		count++;
	}
	for (alias = cmd->userdefined->alias; alias; alias = alias->next)
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
void Cmd_Init(void)
{
	cmd_iter_t *cmd_iter;
	for (cmd_iter = cmd_iter_all; cmd_iter->cmd; cmd_iter++)
	{
		cmd_state_t *cmd = cmd_iter->cmd;
		cmd->mempool = Mem_AllocPool("commands", 0, NULL);
		// space for commands and script files
		cmd->text.data = cmd->text_buf;
		cmd->text.maxsize = sizeof(cmd->text_buf);
		cmd->text.cursize = 0;
		cmd->null_string = "";
	}
	// client console can see server cvars because the user may start a server
	cmd_client.cvars = &cvars_all;
	cmd_client.cvars_flagsmask = CVAR_CLIENT | CVAR_SERVER;
	cmd_client.userdefined = &cmd_userdefined_all;
	// stuffcmd from server has access to the reasonable client things, but it probably doesn't need to access the client's server-only cvars
	cmd_clientfromserver.cvars = &cvars_all;
	cmd_clientfromserver.cvars_flagsmask = CVAR_CLIENT;
	cmd_clientfromserver.userdefined = &cmd_userdefined_all;
	// dedicated server console can only see server cvars, there is no client
	cmd_server.cvars = &cvars_all;
	cmd_server.cvars_flagsmask = CVAR_SERVER;
	cmd_server.userdefined = &cmd_userdefined_all;
	// server commands received from clients have no reason to access cvars, cvar expansion seems perilous.
	cmd_serverfromclient.cvars = &cvars_null;
	cmd_serverfromclient.cvars_flagsmask = 0;
	cmd_serverfromclient.userdefined = &cmd_userdefined_null;
}

void Cmd_Init_Commands(qboolean dedicated_server)
{
//
// register our commands
//
	// client-only commands
	Cmd_AddCommand(&cmd_client, "cmd", Cmd_ForwardToServer_f, "send a console commandline to the server (used by some mods)");
	Cmd_AddCommand(&cmd_clientfromserver, "cmd", Cmd_ForwardToServer_f, "send a console commandline to the server (used by some mods)");
	Cmd_AddCommand(&cmd_client, "wait", Cmd_Wait_f, "make script execution wait for next rendered frame");
	Cmd_AddCommand(&cmd_client, "cprint", Cmd_Centerprint_f, "print something at the screen center");

	// maintenance commands used for upkeep of cvars and saved configs
	Cmd_AddCommand(&cmd_client, "stuffcmds", Cmd_StuffCmds_f, "execute commandline parameters (must be present in quake.rc script)");
	Cmd_AddCommand(&cmd_client, "cvar_lockdefaults", Cvar_LockDefaults_f, "stores the current values of all cvars into their default values, only used once during startup after parsing default.cfg");
	Cmd_AddCommand(&cmd_client, "cvar_resettodefaults_all", Cvar_ResetToDefaults_All_f, "sets all cvars to their locked default values");
	Cmd_AddCommand(&cmd_client, "cvar_resettodefaults_nosaveonly", Cvar_ResetToDefaults_NoSaveOnly_f, "sets all non-saved cvars to their locked default values (variables that will not be saved to config.cfg)");
	Cmd_AddCommand(&cmd_client, "cvar_resettodefaults_saveonly", Cvar_ResetToDefaults_SaveOnly_f, "sets all saved cvars to their locked default values (variables that will be saved to config.cfg)");
	Cmd_AddCommand(&cmd_server, "stuffcmds", Cmd_StuffCmds_f, "execute commandline parameters (must be present in quake.rc script)");
	Cmd_AddCommand(&cmd_server, "cvar_lockdefaults", Cvar_LockDefaults_f, "stores the current values of all cvars into their default values, only used once during startup after parsing default.cfg");
	Cmd_AddCommand(&cmd_server, "cvar_resettodefaults_all", Cvar_ResetToDefaults_All_f, "sets all cvars to their locked default values");
	Cmd_AddCommand(&cmd_server, "cvar_resettodefaults_nosaveonly", Cvar_ResetToDefaults_NoSaveOnly_f, "sets all non-saved cvars to their locked default values (variables that will not be saved to config.cfg)");
	Cmd_AddCommand(&cmd_server, "cvar_resettodefaults_saveonly", Cvar_ResetToDefaults_SaveOnly_f, "sets all saved cvars to their locked default values (variables that will be saved to config.cfg)");

	// general console commands used in multiple environments
	Cmd_AddCommand(&cmd_client, "exec", Cmd_Exec_f, "execute a script file");
	Cmd_AddCommand(&cmd_client, "echo",Cmd_Echo_f, "print a message to the console (useful in scripts)");
	Cmd_AddCommand(&cmd_client, "alias",Cmd_Alias_f, "create a script function (parameters are passed in as $X (being X a number), $* for all parameters, $X- for all parameters starting from $X). Without arguments show the list of all alias");
	Cmd_AddCommand(&cmd_client, "unalias",Cmd_UnAlias_f, "remove an alias");
	Cmd_AddCommand(&cmd_client, "set", Cvar_Set_f, "create or change the value of a console variable");
	Cmd_AddCommand(&cmd_client, "seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to config.cfg");
	Cmd_AddCommand(&cmd_client, "unset", Cvar_Del_f, "delete a cvar (does not work for static ones like _cl_name, or read-only ones)");
	Cmd_AddCommand(&cmd_clientfromserver, "exec", Cmd_Exec_f, "execute a script file");
	Cmd_AddCommand(&cmd_clientfromserver, "echo", Cmd_Echo_f, "print a message to the console (useful in scripts)");
	Cmd_AddCommand(&cmd_clientfromserver, "alias", Cmd_Alias_f, "create a script function (parameters are passed in as $X (being X a number), $* for all parameters, $X- for all parameters starting from $X). Without arguments show the list of all alias");
	Cmd_AddCommand(&cmd_clientfromserver, "unalias", Cmd_UnAlias_f, "remove an alias");
	Cmd_AddCommand(&cmd_clientfromserver, "set", Cvar_Set_f, "create or change the value of a console variable");
	Cmd_AddCommand(&cmd_clientfromserver, "seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to config.cfg");
	Cmd_AddCommand(&cmd_clientfromserver, "unset", Cvar_Del_f, "delete a cvar (does not work for static ones like _cl_name, or read-only ones)");
	Cmd_AddCommand(&cmd_server, "exec", Cmd_Exec_f, "execute a script file");
	Cmd_AddCommand(&cmd_server, "echo", Cmd_Echo_f, "print a message to the console (useful in scripts)");
	Cmd_AddCommand(&cmd_server, "alias", Cmd_Alias_f, "create a script function (parameters are passed in as $X (being X a number), $* for all parameters, $X- for all parameters starting from $X). Without arguments show the list of all alias");
	Cmd_AddCommand(&cmd_server, "unalias", Cmd_UnAlias_f, "remove an alias");
	Cmd_AddCommand(&cmd_server, "set", Cvar_Set_f, "create or change the value of a console variable");
	Cmd_AddCommand(&cmd_server, "seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to config.cfg");
	Cmd_AddCommand(&cmd_server, "unset", Cvar_Del_f, "delete a cvar (does not work for static ones like _cl_name, or read-only ones)");

#ifdef FILLALLCVARSWITHRUBBISH
	Cmd_AddCommand(&cmd_client, "fillallcvarswithrubbish", Cvar_FillAll_f, "fill all cvars with a specified number of characters to provoke buffer overruns");
	Cmd_AddCommand(&cmd_server, "fillallcvarswithrubbish", Cvar_FillAll_f, "fill all cvars with a specified number of characters to provoke buffer overruns");
#endif /* FILLALLCVARSWITHRUBBISH */

	// 2000-01-09 CmdList, CvarList commands By Matthias "Maddes" Buecher
	// Added/Modified by EvilTypeGuy eviltypeguy@qeradiant.com
	Cmd_AddCommand(&cmd_client, "cmdlist", Cmd_List_f, "lists all console commands beginning with the specified prefix or matching the specified wildcard pattern");
	Cmd_AddCommand(&cmd_client, "cvarlist", Cvar_List_f, "lists all console variables beginning with the specified prefix or matching the specified wildcard pattern");
	Cmd_AddCommand(&cmd_client, "apropos", Cmd_Apropos_f, "lists all console variables/commands/aliases containing the specified string in the name or description");
	Cmd_AddCommand(&cmd_server, "cmdlist", Cmd_List_f, "lists all console commands beginning with the specified prefix or matching the specified wildcard pattern");
	Cmd_AddCommand(&cmd_server, "cvarlist", Cvar_List_f, "lists all console variables beginning with the specified prefix or matching the specified wildcard pattern");
	Cmd_AddCommand(&cmd_server, "apropos", Cmd_Apropos_f, "lists all console variables/commands/aliases containing the specified string in the name or description");

	Cmd_AddCommand(&cmd_client, "defer", Cmd_Defer_f, "execute a command in the future");
	Cmd_AddCommand(&cmd_server, "defer", Cmd_Defer_f, "execute a command in the future");

	// DRESK - 5/14/06
	// Support Doom3-style Toggle Command
	Cmd_AddCommand(&cmd_client, "toggle", Cmd_Toggle_f, "toggles a console variable's values (use for more info)");
	Cmd_AddCommand(&cmd_server, "toggle", Cmd_Toggle_f, "toggles a console variable's values (use for more info)");
	Cmd_AddCommand(&cmd_clientfromserver, "toggle", Cmd_Toggle_f, "toggles a console variable's values (use for more info)");
}

/*
============
Cmd_Shutdown
============
*/
void Cmd_Shutdown(void)
{
	cmd_iter_t *cmd_iter;
	for (cmd_iter = cmd_iter_all; cmd_iter->cmd; cmd_iter++)
	{
		cmd_state_t *cmd = cmd_iter->cmd;

		if (cmd->text_lock)
		{
			// we usually have this locked when we get here from Host_Quit_f
			Cbuf_Unlock(cmd);
		}

		Mem_FreePool(&cmd->mempool);
	}
}

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc (cmd_state_t *cmd)
{
	return cmd->argc;
}

/*
============
Cmd_Argv
============
*/
const char *Cmd_Argv(cmd_state_t *cmd, int arg)
{
	if (arg >= cmd->argc )
		return cmd->null_string;
	return cmd->argv[arg];
}

/*
============
Cmd_Args
============
*/
const char *Cmd_Args (cmd_state_t *cmd)
{
	return cmd->args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
// AK: This function should only be called from ExcuteString because the current design is a bit of an hack
static void Cmd_TokenizeString (cmd_state_t *cmd, const char *text)
{
	int l;

	cmd->argc = 0;
	cmd->args = NULL;

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

		if (cmd->argc == 1)
			cmd->args = text;

		if (!COM_ParseToken_Console(&text))
			return;

		if (cmd->argc < MAX_ARGS)
		{
			l = (int)strlen(com_token) + 1;
			if (cmd->tokenizebufferpos + l > CMD_TOKENIZELENGTH)
			{
				Con_Printf("Cmd_TokenizeString: ran out of %i character buffer space for command arguements\n", CMD_TOKENIZELENGTH);
				break;
			}
			memcpy (cmd->tokenizebuffer + cmd->tokenizebufferpos, com_token, l);
			cmd->argv[cmd->argc] = cmd->tokenizebuffer + cmd->tokenizebufferpos;
			cmd->tokenizebufferpos += l;
			cmd->argc++;
		}
	}
}


/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand(cmd_state_t *cmd, const char *cmd_name, xcommand_t function, const char *description)
{
	cmd_function_t *func;
	cmd_function_t *prev, *current;

// fail if the command is a variable name
	if (Cvar_FindVar(cmd->cvars, cmd_name, ~0))
	{
		Con_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	if (function)
	{
		// fail if the command already exists in this interpreter
		for (func = cmd->engine_functions; func; func = func->next)
		{
			if (!strcmp(cmd_name, func->name))
			{
				Con_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
				return;
			}
		}

		func = (cmd_function_t *)Mem_Alloc(cmd->mempool, sizeof(cmd_function_t));
		func->name = cmd_name;
		func->function = function;
		func->description = description;
		func->next = cmd->engine_functions;

		// insert it at the right alphanumeric position
		for (prev = NULL, current = cmd->engine_functions; current && strcmp(current->name, func->name) < 0; prev = current, current = current->next)
			;
		if (prev) {
			prev->next = func;
		}
		else {
			cmd->engine_functions = func;
		}
		func->next = current;
	}
	else
	{
		// mark csqcfunc if the function already exists in the csqc_functions list
		for (func = cmd->userdefined->csqc_functions; func; func = func->next)
		{
			if (!strcmp(cmd_name, func->name))
			{
				func->csqcfunc = true; //[515]: csqc
				return;
			}
		}


		func = (cmd_function_t *)Mem_Alloc(cmd->mempool, sizeof(cmd_function_t));
		func->name = cmd_name;
		func->function = function;
		func->description = description;
		func->csqcfunc = true; //[515]: csqc
		func->next = cmd->userdefined->csqc_functions;

		// insert it at the right alphanumeric position
		for (prev = NULL, current = cmd->userdefined->csqc_functions; current && strcmp(current->name, func->name) < 0; prev = current, current = current->next)
			;
		if (prev) {
			prev->next = func;
		}
		else {
			cmd->userdefined->csqc_functions = func;
		}
		func->next = current;
	}
}

/*
============
Cmd_Exists
============
*/
qboolean Cmd_Exists (cmd_state_t *cmd, const char *cmd_name)
{
	cmd_function_t	*func;

	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
		if (!strcmp(cmd_name, func->name))
			return true;

	for (func=cmd->engine_functions ; func ; func=func->next)
		if (!strcmp (cmd_name,func->name))
			return true;

	return false;
}


/*
============
Cmd_CompleteCommand
============
*/
const char *Cmd_CompleteCommand (cmd_state_t *cmd, const char *partial)
{
	cmd_function_t *func;
	size_t len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			return func->name;

	for (func = cmd->engine_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			return func->name;

	return NULL;
}

/*
	Cmd_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
int Cmd_CompleteCountPossible (cmd_state_t *cmd, const char *partial)
{
	cmd_function_t *func;
	size_t len;
	int h;

	h = 0;
	len = strlen(partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			h++;

	for (func = cmd->engine_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
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
const char **Cmd_CompleteBuildList (cmd_state_t *cmd, const char *partial)
{
	cmd_function_t *func;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cmd_CompleteCountPossible (cmd, partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the functions lists and print all matches
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			buf[bpos++] = func->name;
	for (func = cmd->engine_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			buf[bpos++] = func->name;

	buf[bpos] = NULL;
	return buf;
}

// written by LadyHavoc
void Cmd_CompleteCommandPrint (cmd_state_t *cmd, const char *partial)
{
	cmd_function_t *func;
	size_t len = strlen(partial);
	// Loop through the command list and print all matches
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			Con_Printf("^2%s^7: %s\n", func->name, func->description);
	for (func = cmd->engine_functions; func; func = func->next)
		if (!strncasecmp(partial, func->name, len))
			Con_Printf("^2%s^7: %s\n", func->name, func->description);
}

/*
	Cmd_CompleteAlias

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
const char *Cmd_CompleteAlias (cmd_state_t *cmd, const char *partial)
{
	cmdalias_t *alias;
	size_t len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// Check functions
	for (alias = cmd->userdefined->alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			return alias->name;

	return NULL;
}

// written by LadyHavoc
void Cmd_CompleteAliasPrint (cmd_state_t *cmd, const char *partial)
{
	cmdalias_t *alias;
	size_t len = strlen(partial);
	// Loop through the alias list and print all matches
	for (alias = cmd->userdefined->alias; alias; alias = alias->next)
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
int Cmd_CompleteAliasCountPossible (cmd_state_t *cmd, const char *partial)
{
	cmdalias_t	*alias;
	size_t		len;
	int			h;

	h = 0;

	len = strlen(partial);

	if (!len)
		return 0;

	// Loop through the command list and count all partial matches
	for (alias = cmd->userdefined->alias; alias; alias = alias->next)
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
const char **Cmd_CompleteAliasBuildList (cmd_state_t *cmd, const char *partial)
{
	cmdalias_t *alias;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cmd_CompleteAliasCountPossible (cmd, partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)Mem_Alloc(tempmempool, sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (alias = cmd->userdefined->alias; alias; alias = alias->next)
		if (!strncasecmp(partial, alias->name, len))
			buf[bpos++] = alias->name;

	buf[bpos] = NULL;
	return buf;
}

// TODO: Make this more generic?
void Cmd_ClearCSQCCommands (cmd_state_t *cmd)
{
	cmd_function_t *func;
	cmd_function_t **next = &cmd->userdefined->csqc_functions;
	
	while(*next)
	{
		func = *next;
		*next = func->next;
		Z_Free(func);
	}
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString (cmd_state_t *cmd, const char *text, cmd_source_t src, qboolean lockmutex)
{
	int oldpos;
	cmd_function_t *func;
	cmdalias_t *a;
	if (lockmutex)
		Cbuf_Lock(cmd);
	oldpos = cmd->tokenizebufferpos;
	cmd->source = src;

	Cmd_TokenizeString (cmd, text);

// execute the command line
	if (!Cmd_Argc(cmd))
		goto done; // no tokens

// check functions
	for (func = cmd->userdefined->csqc_functions; func; func = func->next)
	{
		if (!strcasecmp(cmd->argv[0], func->name))
		{
			if (func->csqcfunc && CL_VM_ConsoleCommand(text))	//[515]: csqc
				goto done;
			break;
		}
	}

	for (func = cmd->engine_functions; func; func=func->next)
	{
		if (!strcasecmp (cmd->argv[0], func->name))
		{
			switch (src)
			{
			case src_command:
				if (func->function)
					func->function(cmd);
				else
					Con_Printf("Command \"%s\" can not be executed\n", Cmd_Argv(cmd, 0));
				goto done;
			case src_client:
				if (func->function)
				{
					func->function(cmd);
					goto done;
				}
			}
			break;
		}
	}

	// if it's a client command and no command was found, say so.
	if (cmd->source == src_client)
	{
		Con_Printf("Client \"%s\" tried to execute \"%s\"\n", host_client->name, text);
		goto done;
	}

// check alias
	for (a=cmd->userdefined->alias ; a ; a=a->next)
	{
		if (!strcasecmp (cmd->argv[0], a->name))
		{
			Cmd_ExecuteAlias(cmd, a);
			goto done;
		}
	}

// check cvars
	if (!Cvar_Command(cmd) && host_framecount > 0) {
		if (cmd == &cmd_clientfromserver) {
			Con_Printf("Server tried to execute \"%s\"\n", Cmd_Argv(cmd, 0));
		} else {
			Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(cmd, 0));
		}
	}
done:
	cmd->tokenizebufferpos = oldpos;
	if (lockmutex)
		Cbuf_Unlock(cmd);
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

	// LadyHavoc: thanks to Fuh for bringing the pure evil of SZ_Print to my
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
				SZ_Write(&cls.netcon->message, (unsigned char *)temp, (int)strlen(temp));
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
void Cmd_ForwardToServer_f (cmd_state_t *cmd)
{
	const char *s;
	char vabuf[1024];
	if (!strcasecmp(Cmd_Argv(cmd, 0), "cmd"))
	{
		// we want to strip off "cmd", so just send the args
		s = Cmd_Argc(cmd) > 1 ? Cmd_Args(cmd) : "";
	}
	else
	{
		// we need to keep the command name, so send Cmd_Argv(cmd, 0), a space and then Cmd_Args(cmd)
		s = va(vabuf, sizeof(vabuf), "%s %s", Cmd_Argv(cmd, 0), Cmd_Argc(cmd) > 1 ? Cmd_Args(cmd) : "");
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

int Cmd_CheckParm (cmd_state_t *cmd, const char *parm)
{
	int i;

	if (!parm)
	{
		Con_Printf ("Cmd_CheckParm: NULL");
		return 0;
	}

	for (i = 1; i < Cmd_Argc (cmd); i++)
		if (!strcasecmp (parm, Cmd_Argv(cmd, i)))
			return i;

	return 0;
}



void Cmd_SaveInitState(void)
{
	cmd_iter_t *cmd_iter;
	for (cmd_iter = cmd_iter_all; cmd_iter->cmd; cmd_iter++)
	{
		cmd_state_t *cmd = cmd_iter->cmd;
		cmd_function_t *f;
		cmdalias_t *a;
		for (f = cmd->userdefined->csqc_functions; f; f = f->next)
			f->initstate = true;
		for (f = cmd->engine_functions; f; f = f->next)
			f->initstate = true;
		for (a = cmd->userdefined->alias; a; a = a->next)
		{
			a->initstate = true;
			a->initialvalue = Mem_strdup(zonemempool, a->value);
		}
	}
	Cvar_SaveInitState(&cvars_all);
}

void Cmd_RestoreInitState(void)
{
	cmd_iter_t *cmd_iter;
	for (cmd_iter = cmd_iter_all; cmd_iter->cmd; cmd_iter++)
	{
		cmd_state_t *cmd = cmd_iter->cmd;
		cmd_function_t *f, **fp;
		cmdalias_t *a, **ap;
		for (fp = &cmd->userdefined->csqc_functions; (f = *fp);)
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
		for (fp = &cmd->engine_functions; (f = *fp);)
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
		for (ap = &cmd->userdefined->alias; (a = *ap);)
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
	}
	Cvar_RestoreInitState(&cvars_all);
}
