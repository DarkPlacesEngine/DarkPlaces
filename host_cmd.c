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

#include "quakedef.h"

int current_skill;
cvar_t sv_cheats = {0, "sv_cheats", "0", "enables cheat commands in any game, and cheat impulses in dpmod"};
cvar_t rcon_password = {0, "rcon_password", "", "password to authenticate rcon commands"};
cvar_t rcon_address = {0, "rcon_address", "", "server address to send rcon commands to (when not connected to a server)"};
qboolean allowcheats = false;

/*
==================
Host_Quit_f
==================
*/

void Host_Quit_f (void)
{
	Sys_Quit ();
}


/*
==================
Host_Status_f
==================
*/
void Host_Status_f (void)
{
	client_t *client;
	int seconds, minutes, hours = 0, j, players;
	void (*print) (const char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
		print = SV_ClientPrintf;

	for (players = 0, j = 0;j < svs.maxclients;j++)
		if (svs.clients[j].active)
			players++;
	print ("host:     %s\n", Cvar_VariableString ("hostname"));
	print ("version:  %s build %s\n", gamename, buildstring);
	print ("protocol: %i (%s)\n", Protocol_NumberForEnum(sv.protocol), Protocol_NameForEnum(sv.protocol));
	print ("map:      %s\n", sv.name);
	print ("players:  %i active (%i max)\n\n", players, svs.maxclients);
	for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int)(realtime - client->connecttime);
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
			hours = 0;
		print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->fields.server->frags, hours, minutes, seconds);
		print ("   %s\n", client->netconnection ? client->netconnection->address : "botclient");
	}
}


/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	host_client->edict->fields.server->flags = (int)host_client->edict->fields.server->flags ^ FL_GODMODE;
	if (!((int)host_client->edict->fields.server->flags & FL_GODMODE) )
		SV_ClientPrint("godmode OFF\n");
	else
		SV_ClientPrint("godmode ON\n");
}

void Host_Notarget_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	host_client->edict->fields.server->flags = (int)host_client->edict->fields.server->flags ^ FL_NOTARGET;
	if (!((int)host_client->edict->fields.server->flags & FL_NOTARGET) )
		SV_ClientPrint("notarget OFF\n");
	else
		SV_ClientPrint("notarget ON\n");
}

qboolean noclip_anglehack;

void Host_Noclip_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	if (host_client->edict->fields.server->movetype != MOVETYPE_NOCLIP)
	{
		noclip_anglehack = true;
		host_client->edict->fields.server->movetype = MOVETYPE_NOCLIP;
		SV_ClientPrint("noclip ON\n");
	}
	else
	{
		noclip_anglehack = false;
		host_client->edict->fields.server->movetype = MOVETYPE_WALK;
		SV_ClientPrint("noclip OFF\n");
	}
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	if (host_client->edict->fields.server->movetype != MOVETYPE_FLY)
	{
		host_client->edict->fields.server->movetype = MOVETYPE_FLY;
		SV_ClientPrint("flymode ON\n");
	}
	else
	{
		host_client->edict->fields.server->movetype = MOVETYPE_WALK;
		SV_ClientPrint("flymode OFF\n");
	}
}


/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f (void)
{
	int i;
	client_t *client;
	void (*print) (const char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
		print = SV_ClientPrintf;

	print("Client ping times:\n");
	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->active)
			continue;
		print("%4i %s\n", (int)floor(client->ping*1000+0.5), client->name);
	}
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/

/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f (void)
{
	char level[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Print("map <levelname> : start a new game (kicks off all players)\n");
		return;
	}

	if (cmd_source != src_command)
		return;

	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer();

	// remove console or menu
	key_dest = key_game;
	key_consoleactive = 0;

	svs.serverflags = 0;			// haven't completed an episode yet
	allowcheats = sv_cheats.integer != 0;
	strcpy(level, Cmd_Argv(1));
	SV_SpawnServer(level);
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1");
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f (void)
{
	char level[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Print("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (cls.demoplayback)
	{
		Con_Print("Only the server may changelevel\n");
		return;
	}
	// HACKHACKHACK
	if (!sv.active) {
		Host_Map_f();
		return;
	}
	if (cmd_source != src_command)
		return;

	// remove console or menu
	key_dest = key_game;
	key_consoleactive = 0;

	SV_VM_Begin();
	SV_SaveSpawnparms ();
	SV_VM_End();
	allowcheats = sv_cheats.integer != 0;
	strcpy(level, Cmd_Argv(1));
	SV_SpawnServer(level);
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1");
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f (void)
{
	char mapname[MAX_QPATH];

	if (Cmd_Argc() != 1)
	{
		Con_Print("restart : restart current level\n");
		return;
	}
	if (!sv.active)
	{
		Con_Print("Only the server may restart\n");
		return;
	}
	if (cmd_source != src_command)
		return;

	// remove console or menu
	key_dest = key_game;
	key_consoleactive = 0;

	allowcheats = sv_cheats.integer != 0;
	strcpy(mapname, sv.name);
	SV_SpawnServer(mapname);
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1");
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
void Host_Reconnect_f (void)
{
	if (cls.protocol == PROTOCOL_QUAKEWORLD)
	{
		if (cls.qw_downloadmemory)  // don't change when downloading
			return;

		S_StopAllSounds();

		if (cls.netcon)
		{
			if (cls.state == ca_connected && cls.signon < SIGNONS)
			{
				Con_Printf("reconnecting...\n");
				MSG_WriteChar(&cls.netcon->message, qw_clc_stringcmd);
				MSG_WriteString(&cls.netcon->message, "new");
			}
			else
				Con_Printf("Please use connect instead (reconnect not implemented)\n");
		}
	}
	else
	{
		if (Cmd_Argc() != 1)
		{
			Con_Print("reconnect : wait for signon messages again\n");
			return;
		}
		if (!cls.signon)
		{
			Con_Print("reconnect: no signon, ignoring reconnect\n");
			return;
		}
		cls.signon = 0;		// need new connection messages
	}
}

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
void Host_Connect_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Print("connect <serveraddress> : connect to a multiplayer game\n");
		return;
	}
	CL_EstablishConnection(Cmd_Argv(1));
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
===============
*/
void Host_SavegameComment (char *text)
{
	int		i;
	char	kills[20];

	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		text[i] = ' ';
	// LordHavoc: added min() to prevent overflow
	memcpy (text, cl.levelname, min(strlen(cl.levelname), SAVEGAME_COMMENT_LENGTH));
	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, strlen(kills));
	// convert space to _ to make stdio happy
	// LordHavoc: convert control characters to _ as well
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		if (text[i] <= ' ')
			text[i] = '_';
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}


/*
===============
Host_Savegame_f
===============
*/
void Host_Savegame_f (void)
{
	char	name[MAX_QPATH];
	qfile_t	*f;
	int		i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (cmd_source != src_command)
		return;

	if (cls.state != ca_connected || !sv.active)
	{
		Con_Print("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Print("Can't save in intermission.\n");
		return;
	}

	for (i = 0;i < svs.maxclients;i++)
	{
		if (svs.clients[i].active)
		{
			if (i > 0)
			{
				Con_Print("Can't save multiplayer games.\n");
				return;
			}
			if (svs.clients[i].edict->fields.server->deadflag)
			{
				Con_Print("Can't savegame with a dead player\n");
				return;
			}
		}
	}

	if (Cmd_Argc() != 2)
	{
		Con_Print("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Print("Relative pathnames are not allowed.\n");
		return;
	}

	strlcpy (name, Cmd_Argv(1), sizeof (name));
	FS_DefaultExtension (name, ".sav", sizeof (name));

	Con_Printf("Saving game to %s...\n", name);
	f = FS_Open (name, "wb", false, false);
	if (!f)
	{
		Con_Print("ERROR: couldn't open.\n");
		return;
	}

	FS_Printf(f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	FS_Printf(f, "%s\n", comment);
	for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
		FS_Printf(f, "%f\n", svs.clients[0].spawn_parms[i]);
	FS_Printf(f, "%d\n", current_skill);
	FS_Printf(f, "%s\n", sv.name);
	FS_Printf(f, "%f\n",sv.time);

	// write the light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.lightstyles[i][0])
			FS_Printf(f, "%s\n", sv.lightstyles[i]);
		else
			FS_Print(f,"m\n");
	}

	SV_VM_Begin();

	PRVM_ED_WriteGlobals (f);
	for (i=0 ; i<prog->num_edicts ; i++)
		PRVM_ED_Write (f, PRVM_EDICT_NUM(i));

	SV_VM_End();

	FS_Close (f);
	Con_Print("done.\n");
}


/*
===============
Host_Loadgame_f
===============
*/
void Host_Loadgame_f (void)
{
	char filename[MAX_QPATH];
	char mapname[MAX_QPATH];
	float time;
	const char *start;
	const char *t;
	const char *oldt;
	char *text;
	prvm_edict_t *ent;
	int i;
	int entnum;
	int version;
	float spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Print("load <savename> : load a game\n");
		return;
	}

	strcpy (filename, Cmd_Argv(1));
	FS_DefaultExtension (filename, ".sav", sizeof (filename));

	Con_Printf("Loading game from %s...\n", filename);

	cls.demonum = -1;		// stop demo loop in case this fails

	t = text = (char *)FS_LoadFile (filename, tempmempool, false, NULL);
	if (!text)
	{
		Con_Print("ERROR: couldn't open.\n");
		return;
	}

	// version
	COM_ParseToken(&t, false);
	version = atoi(com_token);
	if (version != SAVEGAME_VERSION)
	{
		Mem_Free(text);
		Con_Printf("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	// description
	// this is a little hard to parse, as : is a separator in COM_ParseToken,
	// so use the console parser instead
	COM_ParseTokenConsole(&t);

	for (i = 0;i < NUM_SPAWN_PARMS;i++)
	{
		COM_ParseToken(&t, false);
		spawn_parms[i] = atof(com_token);
	}
	// skill
	COM_ParseToken(&t, false);
// this silliness is so we can load 1.06 save files, which have float skill values
	current_skill = (int)(atof(com_token) + 0.5);
	Cvar_SetValue ("skill", (float)current_skill);

	// mapname
	COM_ParseToken(&t, false);
	strcpy (mapname, com_token);

	// time
	COM_ParseToken(&t, false);
	time = atof(com_token);

	allowcheats = sv_cheats.integer != 0;

	SV_SpawnServer (mapname);
	if (!sv.active)
	{
		Mem_Free(text);
		Con_Print("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

// load the light styles

	for (i = 0;i < MAX_LIGHTSTYLES;i++)
	{
		// light style
		oldt = t;
		COM_ParseToken(&t, false);
		// if this is a 64 lightstyle savegame produced by Quake, stop now
		// we have to check this because darkplaces saves 256 lightstyle savegames
		if (com_token[0] == '{')
		{
			t = oldt;
			break;
		}
		strlcpy(sv.lightstyles[i], com_token, sizeof(sv.lightstyles[i]));
	}

	// now skip everything before the first opening brace
	// (this is for forward compatibility, so that older versions (at
	// least ones with this fix) can load savegames with extra data before the
	// first brace, as might be produced by a later engine version)
	for(;;)
	{
		oldt = t;
		COM_ParseToken(&t, false);
		if (com_token[0] == '{')
		{
			t = oldt;
			break;
		}
	}

// load the edicts out of the savegame file
	SV_VM_Begin();
	// -1 is the globals
	entnum = -1;
	for (;;)
	{
		start = t;
		while (COM_ParseToken(&t, false))
			if (!strcmp(com_token, "}"))
				break;
		if (!COM_ParseToken(&start, false))
		{
			// end of file
			break;
		}
		if (strcmp(com_token,"{"))
		{
			Mem_Free(text);
			Host_Error ("First token isn't a brace");
		}

		if (entnum == -1)
		{
			// parse the global vars
			PRVM_ED_ParseGlobals (start);
		}
		else
		{
			// parse an edict
			if (entnum >= MAX_EDICTS)
			{
				Mem_Free(text);
				Host_Error("Host_PerformLoadGame: too many edicts in save file (reached MAX_EDICTS %i)", MAX_EDICTS);
			}
			while (entnum >= prog->max_edicts)
				//SV_IncreaseEdicts();
				PRVM_MEM_IncreaseEdicts();
			ent = PRVM_EDICT_NUM(entnum);
			memset (ent->fields.server, 0, prog->progs->entityfields * 4);
			ent->priv.server->free = false;
			PRVM_ED_ParseEdict (start, ent);

			// link it into the bsp tree
			if (!ent->priv.server->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	prog->num_edicts = entnum;
	sv.time = time;

	for (i = 0;i < NUM_SPAWN_PARMS;i++)
		svs.clients[0].spawn_parms[i] = spawn_parms[i];

	SV_VM_End();

	// make sure we're connected to loopback
	if (cls.state == ca_disconnected || !(cls.state == ca_connected && cls.netcon != NULL && LHNETADDRESS_GetAddressType(&cls.netcon->peeraddress) == LHNETADDRESSTYPE_LOOP))
		CL_EstablishConnection("local:1");
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
cvar_t cl_name = {CVAR_SAVE, "_cl_name", "player", "internal storage cvar for current player name (changed by name command)"};
void Host_Name_f (void)
{
	int i, j;
	char newName[sizeof(host_client->name)];

	if (Cmd_Argc () == 1)
	{
		Con_Printf("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}

	if (Cmd_Argc () == 2)
		strlcpy (newName, Cmd_Argv(1), sizeof (newName));
	else
		strlcpy (newName, Cmd_Args(), sizeof (newName));

	for (i = 0, j = 0;newName[i];i++)
		if (newName[i] != '\r' && newName[i] != '\n')
			newName[j++] = newName[i];
	newName[j] = 0;

	if (cmd_source == src_command)
	{
		Cvar_Set ("_cl_name", newName);
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "name", newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (sv.time < host_client->nametime)
	{
		SV_ClientPrintf("You can't change name more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = sv.time + 5;

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->name, newName, sizeof (host_client->name));
	host_client->edict->fields.server->netname = PRVM_SetEngineString(host_client->name);
	if (strcmp(host_client->old_name, host_client->name))
	{
		if (host_client->spawned)
			SV_BroadcastPrintf("%s changed name to %s\n", host_client->old_name, host_client->name);
		strcpy(host_client->old_name, host_client->name);
		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->name);
	}
}

/*
======================
Host_Playermodel_f
======================
*/
cvar_t cl_playermodel = {CVAR_SAVE, "_cl_playermodel", "", "internal storage cvar for current player model in Nexuiz (changed by playermodel command)"};
// the old cl_playermodel in cl_main has been renamed to __cl_playermodel
void Host_Playermodel_f (void)
{
	int i, j;
	char newPath[sizeof(host_client->playermodel)];

	if (Cmd_Argc () == 1)
	{
		Con_Printf("\"playermodel\" is \"%s\"\n", cl_playermodel.string);
		return;
	}

	if (Cmd_Argc () == 2)
		strlcpy (newPath, Cmd_Argv(1), sizeof (newPath));
	else
		strlcpy (newPath, Cmd_Args(), sizeof (newPath));

	for (i = 0, j = 0;newPath[i];i++)
		if (newPath[i] != '\r' && newPath[i] != '\n')
			newPath[j++] = newPath[i];
	newPath[j] = 0;

	if (cmd_source == src_command)
	{
		Cvar_Set ("_cl_playermodel", newPath);
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "playermodel", newPath);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	/*
	if (sv.time < host_client->nametime)
	{
		SV_ClientPrintf("You can't change playermodel more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = sv.time + 5;
	*/

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->playermodel, newPath, sizeof (host_client->playermodel));
	if( eval_playermodel )
		PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_playermodel)->string = PRVM_SetEngineString(host_client->playermodel);
	if (strcmp(host_client->old_model, host_client->playermodel))
	{
		strcpy(host_client->old_model, host_client->playermodel);
		/*// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatepmodel);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->playermodel);*/
	}
}

/*
======================
Host_Playerskin_f
======================
*/
cvar_t cl_playerskin = {CVAR_SAVE, "_cl_playerskin", "", "internal storage cvar for current player skin in Nexuiz (changed by playerskin command)"};
void Host_Playerskin_f (void)
{
	int i, j;
	char newPath[sizeof(host_client->playerskin)];

	if (Cmd_Argc () == 1)
	{
		Con_Printf("\"playerskin\" is \"%s\"\n", cl_playerskin.string);
		return;
	}

	if (Cmd_Argc () == 2)
		strlcpy (newPath, Cmd_Argv(1), sizeof (newPath));
	else
		strlcpy (newPath, Cmd_Args(), sizeof (newPath));

	for (i = 0, j = 0;newPath[i];i++)
		if (newPath[i] != '\r' && newPath[i] != '\n')
			newPath[j++] = newPath[i];
	newPath[j] = 0;

	if (cmd_source == src_command)
	{
		Cvar_Set ("_cl_playerskin", newPath);
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "playerskin", newPath);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	/*
	if (sv.time < host_client->nametime)
	{
		SV_ClientPrintf("You can't change playermodel more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = sv.time + 5;
	*/

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->playerskin, newPath, sizeof (host_client->playerskin));
	if( eval_playerskin )
		PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_playerskin)->string = PRVM_SetEngineString(host_client->playerskin);
	if (strcmp(host_client->old_skin, host_client->playerskin))
	{
		if (host_client->spawned)
			SV_BroadcastPrintf("%s changed skin to %s\n", host_client->name, host_client->playerskin);
		strcpy(host_client->old_skin, host_client->playerskin);
		/*// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatepskin);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->playerskin);*/
	}
}

void Host_Version_f (void)
{
	Con_Printf("Version: %s build %s\n", gamename, buildstring);
}

void Host_Say(qboolean teamonly)
{
	client_t *save;
	int j, quoted;
	const char *p1;
	char *p2;
	// LordHavoc: long say messages
	char text[1024];
	qboolean fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
		{
			fromServer = true;
			teamonly = false;
		}
		else
		{
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 2)
		return;

	if (!teamplay.integer)
		teamonly = false;

// turn on color set 1
	p1 = Cmd_Args();
	quoted = false;
	if (*p1 == '\"')
	{
		quoted = true;
		p1++;
	}
	if (!fromServer)
		dpsnprintf (text, sizeof(text), "%c%s" STRING_COLOR_DEFAULT_STR ": %s", 1, host_client->name, p1);
	else
		dpsnprintf (text, sizeof(text), "%c<%s" STRING_COLOR_DEFAULT_STR "> %s", 1, hostname.string, p1);
	p2 = text + strlen(text);
	while ((const char *)p2 > (const char *)text && (p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)))
	{
		if (p2[-1] == '\"' && quoted)
			quoted = false;
		p2[-1] = 0;
		p2--;
	}
	strlcat(text, "\n", sizeof(text));

	// note: save is not a valid edict if fromServer is true
	save = host_client;
	for (j = 0, host_client = svs.clients;j < svs.maxclients;j++, host_client++)
		if (host_client->spawned && (!teamonly || host_client->edict->fields.server->team == save->edict->fields.server->team))
			SV_ClientPrint(text);
	host_client = save;

	if (cls.state == ca_dedicated)
		Con_Print(&text[1]);
}


void Host_Say_f(void)
{
	Host_Say(false);
}


void Host_Say_Team_f(void)
{
	Host_Say(true);
}


void Host_Tell_f(void)
{
	client_t *save;
	int j;
	const char *p1, *p2;
	char text[MAX_INPUTLINE]; // LordHavoc: FIXME: temporary buffer overflow fix (was 64)
	qboolean fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
			fromServer = true;
		else
		{
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 3)
		return;

	if (!fromServer)
		sprintf (text, "%s: ", host_client->name);
	else
		sprintf (text, "<%s> ", hostname.string);

	p1 = Cmd_Args();
	p2 = p1 + strlen(p1);
	// remove the target name
	while (p1 < p2 && *p1 != ' ')
		p1++;
	while (p1 < p2 && *p1 == ' ')
		p1++;
	// remove trailing newlines
	while (p2 > p1 && (p2[-1] == '\n' || p2[-1] == '\r'))
		p2--;
	// remove quotes if present
	if (*p1 == '"')
	{
		p1++;
		if (p2[-1] == '"')
			p2--;
		else if (fromServer)
			Con_Print("Host_Tell: missing end quote\n");
		else
			SV_ClientPrint("Host_Tell: missing end quote\n");
	}
	while (p2 > p1 && (p2[-1] == '\n' || p2[-1] == '\r'))
		p2--;
	for (j = (int)strlen(text);j < (int)(sizeof(text) - 2) && p1 < p2;)
		text[j++] = *p1++;
	text[j++] = '\n';
	text[j++] = 0;

	save = host_client;
	for (j = 0, host_client = svs.clients;j < svs.maxclients;j++, host_client++)
		if (host_client->spawned && !strcasecmp(host_client->name, Cmd_Argv(1)))
			SV_ClientPrint(text);
	host_client = save;
}


/*
==================
Host_Color_f
==================
*/
cvar_t cl_color = {CVAR_SAVE, "_cl_color", "0", "internal storage cvar for current player colors (changed by color command)"};
void Host_Color_f(void)
{
	int		top, bottom;
	int		playercolor;
	mfunction_t *f;
	func_t	SV_ChangeTeam;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"color\" is \"%i %i\"\n", cl_color.integer >> 4, cl_color.integer & 15);
		Con_Print("color <0-15> [0-15]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	// LordHavoc: allow skin colormaps 14 and 15 (was 13)
	if (top > 15)
		top = 15;
	bottom &= 15;
	// LordHavoc: allow skin colormaps 14 and 15 (was 13)
	if (bottom > 15)
		bottom = 15;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_color", playercolor);
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "topcolor", va("%i", top));
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "bottomcolor", va("%i", bottom));
		if (cls.state == ca_connected && cls.protocol != PROTOCOL_QUAKEWORLD)
			Cmd_ForwardToServer ();
		return;
	}

	if (cls.protocol == PROTOCOL_QUAKEWORLD)
		return;

	if (host_client->edict && (f = PRVM_ED_FindFunction ("SV_ChangeTeam")) && (SV_ChangeTeam = (func_t)(f - prog->functions)))
	{
		Con_DPrint("Calling SV_ChangeTeam\n");
		prog->globals.server->time = sv.time;
		prog->globals.generic[OFS_PARM0] = playercolor;
		prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram (SV_ChangeTeam, "QC function SV_ChangeTeam is missing");
	}
	else
	{
		prvm_eval_t *val;
		if (host_client->edict)
		{
			if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_clientcolors)))
				val->_float = playercolor;
			host_client->edict->fields.server->team = bottom + 1;
		}
		host_client->colors = playercolor;
		if (host_client->old_colors != host_client->colors)
		{
			host_client->old_colors = host_client->colors;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
			MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
			MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
		}
	}
}

cvar_t cl_rate = {CVAR_SAVE, "_cl_rate", "10000", "internal storage cvar for current rate (changed by rate command)"};
void Host_Rate_f(void)
{
	int rate;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"rate\" is \"%i\"\n", cl_rate.integer);
		Con_Print("rate <500-25000>\n");
		return;
	}

	rate = atoi(Cmd_Argv(1));

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_rate", bound(NET_MINRATE, rate, NET_MAXRATE));
		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "rate", va("%i", rate));
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->rate = rate;
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (host_client->edict->fields.server->health <= 0)
	{
		SV_ClientPrint("Can't suicide -- already dead!\n");
		return;
	}

	prog->globals.server->time = sv.time;
	prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
	PRVM_ExecuteProgram (prog->globals.server->ClientKill, "QC function ClientKill is missing");
}


/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f (void)
{

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}
	if (!pausable.integer)
		SV_ClientPrint("Pause not allowed.\n");
	else
	{
		sv.paused ^= 1;
		SV_BroadcastPrintf("%s %spaused the game\n", host_client->name, sv.paused ? "" : "un");
		// send notification to all clients
		MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte(&sv.reliable_datagram, sv.paused);
	}
}

/*
======================
Host_PModel_f
LordHavoc: only supported for Nehahra, I personally think this is dumb, but Mindcrime won't listen.
LordHavoc: correction, Mindcrime will be removing pmodel in the future, but it's still stuck here for compatibility.
======================
*/
cvar_t cl_pmodel = {CVAR_SAVE, "_cl_pmodel", "0", "internal storage cvar for current player model number in nehahra (changed by pmodel command)"};
static void Host_PModel_f (void)
{
	int i;
	prvm_eval_t *val;

	if (Cmd_Argc () == 1)
	{
		Con_Printf("\"pmodel\" is \"%s\"\n", cl_pmodel.string);
		return;
	}
	i = atoi(Cmd_Argv(1));

	if (cmd_source == src_command)
	{
		if (cl_pmodel.integer == i)
			return;
		Cvar_SetValue ("_cl_pmodel", i);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->edict && (val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_pmodel)))
		val->_float = i;
}

//===========================================================================


/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Print("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Print("prespawn not valid -- already spawned\n");
		return;
	}

	if (host_client->netconnection)
	{
		SZ_Write (&host_client->netconnection->message, sv.signon.data, sv.signon.cursize);
		MSG_WriteByte (&host_client->netconnection->message, svc_signonnum);
		MSG_WriteByte (&host_client->netconnection->message, 2);
	}

	// reset the name change timer because the client will send name soon
	host_client->nametime = 0;
}

/*
==================
Host_Spawn_f
==================
*/
void Host_Spawn_f (void)
{
	int i;
	client_t *client;
	func_t RestoreGame;
	mfunction_t *f;
	int stats[MAX_CL_STATS];

	if (cmd_source == src_command)
	{
		Con_Print("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Print("Spawn not valid -- already spawned\n");
		return;
	}

	// reset name change timer again because they might want to change name
	// again in the first 5 seconds after connecting
	host_client->nametime = 0;

	// LordHavoc: moved this above the QC calls at FrikaC's request
	// LordHavoc: commented this out
	//if (host_client->netconnection)
	//	SZ_Clear (&host_client->netconnection->message);

	// run the entrance script
	if (sv.loadgame)
	{
		// loaded games are fully initialized already
		// if this is the last client to be connected, unpause
		sv.paused = false;

		if ((f = PRVM_ED_FindFunction ("RestoreGame")))
		if ((RestoreGame = (func_t)(f - prog->functions)))
		{
			Con_DPrint("Calling RestoreGame\n");
			prog->globals.server->time = sv.time;
			prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
			PRVM_ExecuteProgram (RestoreGame, "QC function RestoreGame is missing");
		}
	}
	else
	{
		//Con_Printf("Host_Spawn_f: host_client->edict->netname = %s, host_client->edict->netname = %s, host_client->name = %s\n", PRVM_GetString(host_client->edict->fields.server->netname), PRVM_GetString(host_client->edict->fields.server->netname), host_client->name);

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&prog->globals.server->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		host_client->clientconnectcalled = true;
		prog->globals.server->time = sv.time;
		prog->globals.server->self = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram (prog->globals.server->ClientConnect, "QC function ClientConnect is missing");

		if ((Sys_DoubleTime() - host_client->connecttime) <= sv.time)
			Con_Printf("%s entered the game\n", host_client->name);

		PRVM_ExecuteProgram (prog->globals.server->PutClientInServer, "QC function PutClientInServer is missing");
	}

	if (!host_client->netconnection)
		return;

	// send time of update
	MSG_WriteByte (&host_client->netconnection->message, svc_time);
	MSG_WriteFloat (&host_client->netconnection->message, sv.time);

	// send all current names, colors, and frag counts
	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&host_client->netconnection->message, svc_updatename);
		MSG_WriteByte (&host_client->netconnection->message, i);
		MSG_WriteString (&host_client->netconnection->message, client->name);
		MSG_WriteByte (&host_client->netconnection->message, svc_updatefrags);
		MSG_WriteByte (&host_client->netconnection->message, i);
		MSG_WriteShort (&host_client->netconnection->message, client->frags);
		MSG_WriteByte (&host_client->netconnection->message, svc_updatecolors);
		MSG_WriteByte (&host_client->netconnection->message, i);
		MSG_WriteByte (&host_client->netconnection->message, client->colors);
	}

	// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		if (sv.lightstyles[i][0])
		{
			MSG_WriteByte (&host_client->netconnection->message, svc_lightstyle);
			MSG_WriteByte (&host_client->netconnection->message, (char)i);
			MSG_WriteString (&host_client->netconnection->message, sv.lightstyles[i]);
		}
	}

	// send some stats
	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->netconnection->message, prog->globals.server->total_secrets);

	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->netconnection->message, prog->globals.server->total_monsters);

	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->netconnection->message, prog->globals.server->found_secrets);

	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->netconnection->message, prog->globals.server->killed_monsters);

	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	if (sv.loadgame)
	{
		MSG_WriteByte (&host_client->netconnection->message, svc_setangle);
		MSG_WriteAngle (&host_client->netconnection->message, host_client->edict->fields.server->v_angle[0], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, host_client->edict->fields.server->v_angle[1], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, 0, sv.protocol);
		sv.loadgame = false; // we're basically done with loading now
	}
	else
	{
		MSG_WriteByte (&host_client->netconnection->message, svc_setangle);
		MSG_WriteAngle (&host_client->netconnection->message, host_client->edict->fields.server->angles[0], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, host_client->edict->fields.server->angles[1], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, 0, sv.protocol);
	}

	SV_WriteClientdataToMessage (host_client, host_client->edict, &host_client->netconnection->message, stats);

	MSG_WriteByte (&host_client->netconnection->message, svc_signonnum);
	MSG_WriteByte (&host_client->netconnection->message, 3);
}

/*
==================
Host_Begin_f
==================
*/
void Host_Begin_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Print("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================


/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f (void)
{
	char *who;
	const char *message = NULL;
	client_t *save;
	int i;
	qboolean byNumber = false;

	if (cmd_source != src_command || !sv.active)
		return;

	SV_VM_Begin();
	save = host_client;

	if (Cmd_Argc() > 2 && strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients || !(host_client = svs.clients + i)->active)
			return;
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
		{
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		}
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = Cmd_Args();
			COM_ParseToken(&message, false);
			if (byNumber)
			{
				message++;							// skip the #
				while (*message == ' ')				// skip white space
					message++;
				message += strlen(Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf("Kicked by %s\n", who);
		SV_DropClient (false); // kicked
	}

	host_client = save;
	SV_VM_End();
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
void Host_Give_f (void)
{
	const char *t;
	int v;
	prvm_eval_t *val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	switch (t[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// MED 01/04/97 added hipnotic give stuff
		if (gamemode == GAME_HIPNOTIC)
		{
			if (t[0] == '6')
			{
				if (t[1] == 'a')
					host_client->edict->fields.server->items = (int)host_client->edict->fields.server->items | HIT_PROXIMITY_GUN;
				else
					host_client->edict->fields.server->items = (int)host_client->edict->fields.server->items | IT_GRENADE_LAUNCHER;
			}
			else if (t[0] == '9')
				host_client->edict->fields.server->items = (int)host_client->edict->fields.server->items | HIT_LASER_CANNON;
			else if (t[0] == '0')
				host_client->edict->fields.server->items = (int)host_client->edict->fields.server->items | HIT_MJOLNIR;
			else if (t[0] >= '2')
				host_client->edict->fields.server->items = (int)host_client->edict->fields.server->items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
			if (t[0] >= '2')
				host_client->edict->fields.server->items = (int)host_client->edict->fields.server->items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 's':
		if (gamemode == GAME_ROGUE && (val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_shells1)))
			val->_float = v;

		host_client->edict->fields.server->ammo_shells = v;
		break;
	case 'n':
		if (gamemode == GAME_ROGUE)
		{
			if ((val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_nails1)))
			{
				val->_float = v;
				if (host_client->edict->fields.server->weapon <= IT_LIGHTNING)
					host_client->edict->fields.server->ammo_nails = v;
			}
		}
		else
		{
			host_client->edict->fields.server->ammo_nails = v;
		}
		break;
	case 'l':
		if (gamemode == GAME_ROGUE)
		{
			val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_lava_nails);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->fields.server->weapon > IT_LIGHTNING)
					host_client->edict->fields.server->ammo_nails = v;
			}
		}
		break;
	case 'r':
		if (gamemode == GAME_ROGUE)
		{
			val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_rockets1);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->fields.server->weapon <= IT_LIGHTNING)
					host_client->edict->fields.server->ammo_rockets = v;
			}
		}
		else
		{
			host_client->edict->fields.server->ammo_rockets = v;
		}
		break;
	case 'm':
		if (gamemode == GAME_ROGUE)
		{
			val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_multi_rockets);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->fields.server->weapon > IT_LIGHTNING)
					host_client->edict->fields.server->ammo_rockets = v;
			}
		}
		break;
	case 'h':
		host_client->edict->fields.server->health = v;
		break;
	case 'c':
		if (gamemode == GAME_ROGUE)
		{
			val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_cells1);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->fields.server->weapon <= IT_LIGHTNING)
					host_client->edict->fields.server->ammo_cells = v;
			}
		}
		else
		{
			host_client->edict->fields.server->ammo_cells = v;
		}
		break;
	case 'p':
		if (gamemode == GAME_ROGUE)
		{
			val = PRVM_GETEDICTFIELDVALUE(host_client->edict, eval_ammo_plasma);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->fields.server->weapon > IT_LIGHTNING)
					host_client->edict->fields.server->ammo_cells = v;
			}
		}
		break;
	}
}

prvm_edict_t	*FindViewthing (void)
{
	int		i;
	prvm_edict_t	*e;

	for (i=0 ; i<prog->num_edicts ; i++)
	{
		e = PRVM_EDICT_NUM(i);
		if (!strcmp (PRVM_GetString(e->fields.server->classname), "viewthing"))
			return e;
	}
	Con_Print("No viewthing on map\n");
	return NULL;
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f (void)
{
	prvm_edict_t	*e;
	model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false, true, false);
	if (!m || !m->loaded || !m->Draw)
	{
		Con_Printf("viewmodel: can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->fields.server->frame = 0;
	cl.model_precache[(int)e->fields.server->modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f (void)
{
	prvm_edict_t	*e;
	int		f;
	model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;
	m = cl.model_precache[(int)e->fields.server->modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes-1;

	e->fields.server->frame = f;
}


void PrintFrameName (model_t *m, int frame)
{
	if (m->animscenes)
		Con_Printf("frame %i: %s\n", frame, m->animscenes[frame].name);
	else
		Con_Printf("frame %i\n", frame);
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f (void)
{
	prvm_edict_t	*e;
	model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;
	m = cl.model_precache[(int)e->fields.server->modelindex];

	e->fields.server->frame = e->fields.server->frame + 1;
	if (e->fields.server->frame >= m->numframes)
		e->fields.server->frame = m->numframes - 1;

	PrintFrameName (m, e->fields.server->frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	prvm_edict_t	*e;
	model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;

	m = cl.model_precache[(int)e->fields.server->modelindex];

	e->fields.server->frame = e->fields.server->frame - 1;
	if (e->fields.server->frame < 0)
		e->fields.server->frame = 0;

	PrintFrameName (m, e->fields.server->frame);
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f (void)
{
	int		i, c;

	if (cls.state == ca_dedicated || COM_CheckParm("-listen") || COM_CheckParm("-benchmark") || COM_CheckParm("-demo") || COM_CheckParm("-demolooponly"))
		return;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_Printf("%i demo(s) in loop\n", c);

	for (i=1 ; i<c+1 ; i++)
		strlcpy (cls.demos[i-1], Cmd_Argv(i), sizeof (cls.demos[i-1]));

	// LordHavoc: clear the remaining slots
	for (;i <= MAX_DEMOS;i++)
		cls.demos[i-1][0] = 0;

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		CL_NextDemo ();
	}
	else
		cls.demonum = -1;
}


/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (cls.demonum == -1)
		cls.demonum = 1;
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f (void)
{
	if (!cls.demoplayback)
		return;
	CL_Disconnect ();
	Host_ShutdownServer ();
}

void Host_SendCvar_f (void)
{
	int		i;
	cvar_t	*c;
	client_t *old;

	if(Cmd_Argc() != 2)
		return;
	if(!(c = Cvar_FindVar(Cmd_Argv(1))))
		return;
	if (cls.state != ca_dedicated)
		Cmd_ForwardStringToServer(va("sentcvar %s %s\n", c->name, c->string));
	if(!sv.active)// || !SV_ParseClientCommandQC)
		return;

	old = host_client;
	if (cls.state != ca_dedicated)
		i = 1;
	else
		i = 0;
	for(;i<svs.maxclients;i++)
		if(svs.clients[i].active && svs.clients[i].netconnection)
		{
			host_client = &svs.clients[i];
			Host_ClientCommands(va("sendcvar %s\n", c->name));
		}
	host_client = old;
}

static void MaxPlayers_f(void)
{
	int n;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"maxplayers\" is \"%u\"\n", svs.maxclients);
		return;
	}

	if (sv.active)
	{
		Con_Print("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = atoi(Cmd_Argv(1));
	n = bound(1, n, MAX_SCOREBOARD);
	Con_Printf("\"maxplayers\" set to \"%u\"\n", n);

	if (svs.clients)
		Mem_Free(svs.clients);
	svs.maxclients = n;
	svs.clients = (client_t *)Mem_Alloc(sv_mempool, sizeof(client_t) * svs.maxclients);
	if (n == 1)
		Cvar_Set ("deathmatch", "0");
	else
		Cvar_Set ("deathmatch", "1");
}

//=============================================================================

// QuakeWorld commands

/*
=====================
Host_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void Host_Rcon_f (void) // credit: taken from QuakeWorld
{
	int i;
	lhnetaddress_t to;
	lhnetsocket_t *mysocket;

	if (!rcon_password.string || !rcon_password.string[0])
	{
		Con_Printf ("You must set rcon_password before issuing an rcon command.\n");
		return;
	}

	for (i = 0;rcon_password.string[i];i++)
	{
		if (rcon_password.string[i] <= ' ')
		{
			Con_Printf("rcon_password is not allowed to have any whitespace.\n");
			return;
		}
	}

	if (cls.netcon)
		to = cls.netcon->peeraddress;
	else
	{
		if (!rcon_address.integer || !rcon_address.string[0])
		{
			Con_Printf ("You must either be connected, or set the rcon_address cvar to issue rcon commands\n");
			return;
		}
		LHNETADDRESS_FromString(&to, rcon_address.string, sv_netport.integer);
	}
	mysocket = NetConn_ChooseClientSocketForAddress(&to);
	if (mysocket)
	{
		// simply put together the rcon packet and send it
		NetConn_WriteString(mysocket, va("\377\377\377\377rcon %s %s", rcon_password.string, Cmd_Args()), &to);
	}
}

/*
====================
Host_User_f

user <name or userid>

Dump userdata / masterdata for a user
====================
*/
void Host_User_f (void) // credit: taken from QuakeWorld
{
	int		uid;
	int		i;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("Usage: user <username / userid>\n");
		return;
	}

	uid = atoi(Cmd_Argv(1));

	for (i = 0;i < cl.maxclients;i++)
	{
		if (!cl.scores[i].name[0])
			continue;
		if (cl.scores[i].qw_userid == uid || !strcasecmp(cl.scores[i].name, Cmd_Argv(1)))
		{
			InfoString_Print(cl.scores[i].qw_userinfo);
			return;
		}
	}
	Con_Printf ("User not in server.\n");
}

/*
====================
Host_Users_f

Dump userids for all current players
====================
*/
void Host_Users_f (void) // credit: taken from QuakeWorld
{
	int		i;
	int		c;

	c = 0;
	Con_Printf ("userid frags name\n");
	Con_Printf ("------ ----- ----\n");
	for (i = 0;i < cl.maxclients;i++)
	{
		if (cl.scores[i].name[0])
		{
			Con_Printf ("%6i %4i %s\n", cl.scores[i].qw_userid, cl.scores[i].frags, cl.scores[i].name);
			c++;
		}
	}

	Con_Printf ("%i total users\n", c);
}

/*
==================
Host_FullServerinfo_f

Sent by server when serverinfo changes
==================
*/
// TODO: shouldn't this be a cvar instead?
void Host_FullServerinfo_f (void) // credit: taken from QuakeWorld
{
	char temp[512];
	if (Cmd_Argc() != 2)
	{
		Con_Printf ("usage: fullserverinfo <complete info string>\n");
		return;
	}

	strlcpy (cl.qw_serverinfo, Cmd_Argv(1), sizeof(cl.qw_serverinfo));
	InfoString_GetValue(cl.qw_serverinfo, "teamplay", temp, sizeof(temp));
	cl.qw_teamplay = atoi(temp);
}

/*
==================
Host_FullInfo_f

Allow clients to change userinfo
==================
Casey was here :)
*/
void Host_FullInfo_f (void) // credit: taken from QuakeWorld
{
	char key[512];
	char value[512];
	char *o;
	const char *s;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv(1);
	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s)
		{
			Con_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		if (!strcasecmp(key, "pmodel") || !strcasecmp(key, "emodel"))
			continue;

		if (key[0] == '*')
		{
			Con_Printf("Can't set star-key \"%s\" to \"%s\"\n", key, value);
			continue;
		}

		InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), key, value);
	}
}

/*
==================
CL_SetInfo_f

Allow clients to change userinfo
==================
*/
void Host_SetInfo_f (void) // credit: taken from QuakeWorld
{
	if (Cmd_Argc() == 1)
	{
		InfoString_Print(cls.userinfo);
		return;
	}
	if (Cmd_Argc() != 3)
	{
		Con_Printf ("usage: setinfo [ <key> <value> ]\n");
		return;
	}
	if (!strcasecmp(Cmd_Argv(1), "pmodel") || !strcasecmp(Cmd_Argv(1), "emodel"))
		return;
	if (Cmd_Argv(1)[0] == '*')
	{
		Con_Printf("Can't set star-key \"%s\" to \"%s\"\n", Cmd_Argv(1), Cmd_Argv(2));
		return;
	}
	InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), Cmd_Argv(1), Cmd_Argv(2));
	if (cls.state == ca_connected)
		Cmd_ForwardToServer ();
}

/*
====================
Host_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void Host_Packet_f (void) // credit: taken from QuakeWorld
{
	char send[2048];
	int i, l;
	const char *in;
	char *out;
	lhnetaddress_t address;
	lhnetsocket_t *mysocket;

	if (Cmd_Argc() != 3)
	{
		Con_Printf ("packet <destination> <contents>\n");
		return;
	}

	if (!LHNETADDRESS_FromString (&address, Cmd_Argv(1), sv_netport.integer))
	{
		Con_Printf ("Bad address\n");
		return;
	}

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = 0xff;

	l = (int)strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (out >= send + sizeof(send) - 1)
			break;
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '0')
		{
			*out++ = '\0';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == 't')
		{
			*out++ = '\t';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == 'r')
		{
			*out++ = '\r';
			i++;
		}
		else if (in[i] == '\\' && in[i+1] == '"')
		{
			*out++ = '\"';
			i++;
		}
		else
			*out++ = in[i];
	}

	mysocket = NetConn_ChooseClientSocketForAddress(&address);
	if (mysocket)
		NetConn_Write(mysocket, send, out - send, &address);
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	strcpy(cls.userinfo, "\\name\\player\\team\\none\\topcolor\\0\\bottomcolor\\0\\rate\\10000\\msg\\1\\*ver\\dp");

	Cmd_AddCommand ("status", Host_Status_f, "print server status information");
	Cmd_AddCommand ("quit", Host_Quit_f, "quit the game");
	if (gamemode == GAME_NEHAHRA)
	{
		Cmd_AddCommand ("max", Host_God_f, "god mode (invulnerability)");
		Cmd_AddCommand ("monster", Host_Notarget_f, "notarget mode (monsters do not see you)");
		Cmd_AddCommand ("scrag", Host_Fly_f, "fly mode (flight)");
		Cmd_AddCommand ("wraith", Host_Noclip_f, "noclip mode (flight without collisions, move through walls)");
		Cmd_AddCommand ("gimme", Host_Give_f, "alter inventory");
	}
	else
	{
		Cmd_AddCommand ("god", Host_God_f, "god mode (invulnerability)");
		Cmd_AddCommand ("notarget", Host_Notarget_f, "notarget mode (monsters do not see you)");
		Cmd_AddCommand ("fly", Host_Fly_f, "fly mode (flight)");
		Cmd_AddCommand ("noclip", Host_Noclip_f, "noclip mode (flight without collisions, move through walls)");
		Cmd_AddCommand ("give", Host_Give_f, "alter inventory");
	}
	Cmd_AddCommand ("map", Host_Map_f, "kick everyone off the server and start a new level");
	Cmd_AddCommand ("restart", Host_Restart_f, "restart current level");
	Cmd_AddCommand ("changelevel", Host_Changelevel_f, "change to another level, bringing along all connected clients");
	Cmd_AddCommand ("connect", Host_Connect_f, "connect to a server by IP address or hostname");
	Cmd_AddCommand ("reconnect", Host_Reconnect_f, "reset signon level in preparation for a new level (do not use)");
	Cmd_AddCommand ("version", Host_Version_f, "print engine version");
	Cmd_AddCommand ("say", Host_Say_f, "send a chat message to everyone on the server");
	Cmd_AddCommand ("say_team", Host_Say_Team_f, "send a chat message to your team on the server");
	Cmd_AddCommand ("tell", Host_Tell_f, "send a chat message to only one person on the server");
	Cmd_AddCommand ("kill", Host_Kill_f, "die instantly");
	Cmd_AddCommand ("pause", Host_Pause_f, "pause the game (if the server allows pausing)");
	Cmd_AddCommand ("kick", Host_Kick_f, "kick a player off the server by number or name");
	Cmd_AddCommand ("ping", Host_Ping_f, "print ping times of all players on the server");
	Cmd_AddCommand ("load", Host_Loadgame_f, "load a saved game file");
	Cmd_AddCommand ("save", Host_Savegame_f, "save the game to a file");

	Cmd_AddCommand ("startdemos", Host_Startdemos_f, "start playing back the selected demos sequentially (used at end of startup script)");
	Cmd_AddCommand ("demos", Host_Demos_f, "restart looping demos defined by the last startdemos command");
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f, "stop playing or recording demo (like stop command) and return to looping demos");

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f, "change model of viewthing entity in current level");
	Cmd_AddCommand ("viewframe", Host_Viewframe_f, "change animation frame of viewthing entity in current level");
	Cmd_AddCommand ("viewnext", Host_Viewnext_f, "change to next animation frame of viewthing entity in current level");
	Cmd_AddCommand ("viewprev", Host_Viewprev_f, "change to previous animation frame of viewthing entity in current level");

	Cvar_RegisterVariable (&cl_name);
	Cmd_AddCommand ("name", Host_Name_f, "change your player name");
	Cvar_RegisterVariable (&cl_color);
	Cmd_AddCommand ("color", Host_Color_f, "change your player shirt and pants colors");
	Cvar_RegisterVariable (&cl_rate);
	Cmd_AddCommand ("rate", Host_Rate_f, "change your network connection speed");
	if (gamemode == GAME_NEHAHRA)
	{
		Cvar_RegisterVariable (&cl_pmodel);
		Cmd_AddCommand ("pmodel", Host_PModel_f, "change your player model choice (Nehahra specific)");
	}

	// BLACK: This isnt game specific anymore (it was GAME_NEXUIZ at first)
	Cvar_RegisterVariable (&cl_playermodel);
	Cmd_AddCommand ("playermodel", Host_Playermodel_f, "change your player model");
	Cvar_RegisterVariable (&cl_playerskin);
	Cmd_AddCommand ("playerskin", Host_Playerskin_f, "change your player skin number");

	Cmd_AddCommand ("prespawn", Host_PreSpawn_f, "signon 1 (client acknowledges that server information has been received)");
	Cmd_AddCommand ("spawn", Host_Spawn_f, "signon 2 (client has sent player information, and is asking server to send scoreboard rankings)");
	Cmd_AddCommand ("begin", Host_Begin_f, "signon 3 (client asks server to start sending entities, and will go to signon 4 (playing) when the first entity update is received)");
	Cmd_AddCommand ("maxplayers", MaxPlayers_f, "sets limit on how many players (or bots) may be connected to the server at once");

	Cmd_AddCommand ("sendcvar", Host_SendCvar_f, "sends the value of a cvar to the server as a sentcvar command, for use by QuakeC");	// By [515]

	Cvar_RegisterVariable (&rcon_password);
	Cvar_RegisterVariable (&rcon_address);
	Cmd_AddCommand ("rcon", Host_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's)");
	Cmd_AddCommand ("user", Host_User_f, "prints additional information about a player number or name on the scoreboard");
	Cmd_AddCommand ("users", Host_Users_f, "prints additional information about all players on the scoreboard");
	Cmd_AddCommand ("fullserverinfo", Host_FullServerinfo_f, "internal use only, sent by server to client to update client's local copy of serverinfo string");
	Cmd_AddCommand ("fullinfo", Host_FullInfo_f, "allows client to modify their userinfo");
	Cmd_AddCommand ("setinfo", Host_SetInfo_f, "modifies your userinfo");
	Cmd_AddCommand ("packet", Host_Packet_f, "send a packet to the specified address:port containing a text string");

	Cvar_RegisterVariable(&sv_cheats);
}

