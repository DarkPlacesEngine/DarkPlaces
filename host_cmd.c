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
cvar_t sv_cheats = {0, "sv_cheats", "0"};
qboolean allowcheats = false;

mfunction_t *ED_FindFunction (char *name);

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
	const char *protocolname;
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
	switch(sv.protocol)
	{
		case PROTOCOL_QUAKE: protocolname = sv.netquakecompatible ? "QUAKE" : "QUAKEDP";break;
		case PROTOCOL_DARKPLACES1: protocolname = "PROTOCOL_DARKPLACES1";break;
		case PROTOCOL_DARKPLACES2: protocolname = "PROTOCOL_DARKPLACES2";break;
		case PROTOCOL_DARKPLACES3: protocolname = "PROTOCOL_DARKPLACES3";break;
		case PROTOCOL_DARKPLACES4: protocolname = "PROTOCOL_DARKPLACES4";break;
		case PROTOCOL_DARKPLACES5: protocolname = "PROTOCOL_DARKPLACES5";break;
		case PROTOCOL_DARKPLACES6: protocolname = "PROTOCOL_DARKPLACES6";break;
		default: protocolname = "PROTOCOL_UNKNOWN";break;
	}
	print ("protocol: %i (%s)\n", sv.protocol, protocolname);
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
		print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->v->frags, hours, minutes, seconds);
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

	host_client->edict->v->flags = (int)host_client->edict->v->flags ^ FL_GODMODE;
	if (!((int)host_client->edict->v->flags & FL_GODMODE) )
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

	host_client->edict->v->flags = (int)host_client->edict->v->flags ^ FL_NOTARGET;
	if (!((int)host_client->edict->v->flags & FL_NOTARGET) )
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

	if (host_client->edict->v->movetype != MOVETYPE_NOCLIP)
	{
		noclip_anglehack = true;
		host_client->edict->v->movetype = MOVETYPE_NOCLIP;
		SV_ClientPrint("noclip ON\n");
	}
	else
	{
		noclip_anglehack = false;
		host_client->edict->v->movetype = MOVETYPE_WALK;
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

	if (host_client->edict->v->movetype != MOVETYPE_FLY)
	{
		host_client->edict->v->movetype = MOVETYPE_FLY;
		SV_ClientPrint("flymode ON\n");
	}
	else
	{
		host_client->edict->v->movetype = MOVETYPE_WALK;
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
	int		i, j;
	float	total;
	client_t	*client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrint("Client ping times:\n");
	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->active)
			continue;
		total = 0;
		for (j=0 ; j<NUM_PING_TIMES ; j++)
			total+=client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf("%4i %s\n", (int)(total*1000), client->name);
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
	Host_ShutdownServer(false);

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
	if (!sv.active || cls.demoplayback)
	{
		Con_Print("Only the server may changelevel\n");
		return;
	}
	if (cmd_source != src_command)
		return;

	// remove console or menu
	key_dest = key_game;
	key_consoleactive = 0;

	SV_SaveSpawnparms ();
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
	if (!sv.active || cls.demoplayback)
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
	if (Cmd_Argc() != 1)
	{
		Con_Print("reconnect : wait for signon messages again\n");
		return;
	}
	if (!cls.signon)
	{
		//Con_Print("reconnect: no signon, ignoring reconnect\n");
		return;
	}
	cls.signon = 0;		// need new connection messages
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
	memcpy (text, cl.levelname, strlen(cl.levelname));
	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, strlen(kills));
// convert space to _ to make stdio happy
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		if (text[i] == ' ')
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
	char	name[256];
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
			if (svs.clients[i].edict->v->deadflag)
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
		if (sv.lightstyles[i])
			FS_Printf(f, "%s\n", sv.lightstyles[i]);
		else
			FS_Print(f,"m\n");
	}

	ED_WriteGlobals (f);
	for (i=0 ; i<sv.num_edicts ; i++)
		ED_Write (f, EDICT_NUM(i));
	FS_Close (f);
	Con_Print("done.\n");
}


extern mempool_t *edictstring_mempool;

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
	char *text;
	edict_t *ent;
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

	t = text = FS_LoadFile (filename, tempmempool, false);
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
		COM_ParseToken(&t, false);
		sv.lightstyles[i] = Mem_Alloc(edictstring_mempool, strlen(com_token)+1);
		strcpy (sv.lightstyles[i], com_token);
	}

// load the edicts out of the savegame file
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
			ED_ParseGlobals (start);
		}
		else
		{
			// parse an edict
			if (entnum >= MAX_EDICTS)
			{
				Mem_Free(text);
				Host_Error("Host_PerformLoadGame: too many edicts in save file (reached MAX_EDICTS %i)\n", MAX_EDICTS);
			}
			while (entnum >= sv.max_edicts)
				SV_IncreaseEdicts();
			ent = EDICT_NUM(entnum);
			memset (ent->v, 0, progs->entityfields * 4);
			ent->e->free = false;
			ED_ParseEdict (start, ent);

			// link it into the bsp tree
			if (!ent->e->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	for (i = 0;i < NUM_SPAWN_PARMS;i++)
		svs.clients[0].spawn_parms[i] = spawn_parms[i];

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
cvar_t cl_name = {CVAR_SAVE, "_cl_name", "player"};
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
	host_client->edict->v->netname = PR_SetString(host_client->name);
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
cvar_t cl_playermodel = {CVAR_SAVE, "_cl_playermodel", ""};
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
		GETEDICTFIELDVALUE(host_client->edict, eval_playermodel)->string = PR_SetString(host_client->playermodel);
	if (strcmp(host_client->old_model, host_client->playermodel))
	{
		if (host_client->spawned)
			SV_BroadcastPrintf("%s changed model to %s\n", host_client->old_model, host_client->playermodel);
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
cvar_t cl_playerskin = {CVAR_SAVE, "_cl_playerskin", ""};
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
		GETEDICTFIELDVALUE(host_client->edict, eval_playerskin)->string = PR_SetString(host_client->playerskin);
	if (strcmp(host_client->old_skin, host_client->playerskin))
	{
		if (host_client->spawned)
			SV_BroadcastPrintf("%s changed skin to %s\n", host_client->old_skin, host_client->playerskin);
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
	// LordHavoc: 256 char say messages
	unsigned char text[256];
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
		dpsnprintf (text, sizeof(text), "%c%s: %s", 1, host_client->name, p1);
	else
		dpsnprintf (text, sizeof(text), "%c<%s> %s", 1, hostname.string, p1);
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
		if (host_client->spawned && (!teamonly || host_client->edict->v->team == save->edict->v->team))
			SV_ClientPrint(text);
	host_client = save;

	//Con_Print(&text[1]);
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
	char text[1024]; // LordHavoc: FIXME: temporary buffer overflow fix (was 64)
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
	for (j = strlen(text);j < (int)(sizeof(text) - 2) && p1 < p2;)
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
cvar_t cl_color = {CVAR_SAVE, "_cl_color", "0"};
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
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->edict && (f = ED_FindFunction ("SV_ChangeTeam")) && (SV_ChangeTeam = (func_t)(f - pr_functions)))
	{
		Con_DPrint("Calling SV_ChangeTeam\n");
		pr_global_struct->time = sv.time;
		pr_globals[OFS_PARM0] = playercolor;
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram (SV_ChangeTeam, "QC function SV_ChangeTeam is missing");
	}
	else
	{
		eval_t *val;
		if (host_client->edict)
		{
			if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_clientcolors)))
				val->_float = playercolor;
			host_client->edict->v->team = bottom + 1;
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

cvar_t cl_rate = {CVAR_SAVE, "_cl_rate", "10000"};
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

	if (host_client->edict->v->health <= 0)
	{
		SV_ClientPrint("Can't suicide -- already dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
	PR_ExecuteProgram (pr_global_struct->ClientKill, "QC function ClientKill is missing");
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
======================
*/
cvar_t cl_pmodel = {CVAR_SAVE, "_cl_pmodel", "0"};
static void Host_PModel_f (void)
{
	int i;
	eval_t *val;

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

	if (host_client->edict && (val = GETEDICTFIELDVALUE(host_client->edict, eval_pmodel)))
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

	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	host_client->sendsignon = true;

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
	// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

	// run the entrance script
	if (sv.loadgame)
	{
		// loaded games are fully initialized already
		// if this is the last client to be connected, unpause
		sv.paused = false;

		if ((f = ED_FindFunction ("RestoreGame")))
		if ((RestoreGame = (func_t)(f - pr_functions)))
		{
			Con_DPrint("Calling RestoreGame\n");
			pr_global_struct->time = sv.time;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (RestoreGame, "QC function RestoreGame is missing");
		}
	}
	else
	{
		// set up the edict
		ED_ClearEdict(host_client->edict);

		//Con_Printf("Host_Spawn_f: host_client->edict->netname = %s, host_client->edict->netname = %s, host_client->name = %s\n", PR_GetString(host_client->edict->v->netname), PR_GetString(host_client->edict->v->netname), host_client->name);

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram (pr_global_struct->ClientConnect, "QC function ClientConnect is missing");

		if ((Sys_DoubleTime() - host_client->connecttime) <= sv.time)
			Con_Printf("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer, "QC function PutClientInServer is missing");
	}


	// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

	// send all current light styles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char)i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

	// send some stats
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);

	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	MSG_WriteByte (&host_client->message, svc_setangle);
	MSG_WriteAngle (&host_client->message, host_client->edict->v->angles[0], sv.protocol);
	MSG_WriteAngle (&host_client->message, host_client->edict->v->angles[1], sv.protocol);
	MSG_WriteAngle (&host_client->message, 0, sv.protocol);

	SV_WriteClientdataToMessage (host_client, host_client->edict, &host_client->message, stats);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
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
	eval_t *val;

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
					host_client->edict->v->items = (int)host_client->edict->v->items | HIT_PROXIMITY_GUN;
				else
					host_client->edict->v->items = (int)host_client->edict->v->items | IT_GRENADE_LAUNCHER;
			}
			else if (t[0] == '9')
				host_client->edict->v->items = (int)host_client->edict->v->items | HIT_LASER_CANNON;
			else if (t[0] == '0')
				host_client->edict->v->items = (int)host_client->edict->v->items | HIT_MJOLNIR;
			else if (t[0] >= '2')
				host_client->edict->v->items = (int)host_client->edict->v->items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
			if (t[0] >= '2')
				host_client->edict->v->items = (int)host_client->edict->v->items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 's':
		if (gamemode == GAME_ROGUE && (val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_shells1)))
			val->_float = v;

		host_client->edict->v->ammo_shells = v;
		break;
	case 'n':
		if (gamemode == GAME_ROGUE)
		{
			if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_nails1)))
			{
				val->_float = v;
				if (host_client->edict->v->weapon <= IT_LIGHTNING)
					host_client->edict->v->ammo_nails = v;
			}
		}
		else
		{
			host_client->edict->v->ammo_nails = v;
		}
		break;
	case 'l':
		if (gamemode == GAME_ROGUE)
		{
			val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_lava_nails);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->v->weapon > IT_LIGHTNING)
					host_client->edict->v->ammo_nails = v;
			}
		}
		break;
	case 'r':
		if (gamemode == GAME_ROGUE)
		{
			val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_rockets1);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->v->weapon <= IT_LIGHTNING)
					host_client->edict->v->ammo_rockets = v;
			}
		}
		else
		{
			host_client->edict->v->ammo_rockets = v;
		}
		break;
	case 'm':
		if (gamemode == GAME_ROGUE)
		{
			val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_multi_rockets);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->v->weapon > IT_LIGHTNING)
					host_client->edict->v->ammo_rockets = v;
			}
		}
		break;
	case 'h':
		host_client->edict->v->health = v;
		break;
	case 'c':
		if (gamemode == GAME_ROGUE)
		{
			val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_cells1);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->v->weapon <= IT_LIGHTNING)
					host_client->edict->v->ammo_cells = v;
			}
		}
		else
		{
			host_client->edict->v->ammo_cells = v;
		}
		break;
	case 'p':
		if (gamemode == GAME_ROGUE)
		{
			val = GETEDICTFIELDVALUE(host_client->edict, eval_ammo_plasma);
			if (val)
			{
				val->_float = v;
				if (host_client->edict->v->weapon > IT_LIGHTNING)
					host_client->edict->v->ammo_cells = v;
			}
		}
		break;
	}
}

edict_t	*FindViewthing (void)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if (!strcmp (PR_GetString(e->v->classname), "viewthing"))
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
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false, true, false);
	if (!m)
	{
		Con_Printf("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->v->frame = 0;
	cl.model_precache[(int)e->v->modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f (void)
{
	edict_t	*e;
	int		f;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v->modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes-1;

	e->v->frame = f;
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
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v->modelindex];

	e->v->frame = e->v->frame + 1;
	if (e->v->frame >= m->numframes)
		e->v->frame = m->numframes - 1;

	PrintFrameName (m, e->v->frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = cl.model_precache[(int)e->v->modelindex];

	e->v->frame = e->v->frame - 1;
	if (e->v->frame < 0)
		e->v->frame = 0;

	PrintFrameName (m, e->v->frame);
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

	if (cls.state == ca_dedicated || COM_CheckParm("-listen"))
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
	Host_ShutdownServer (false);
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
	svs.clients = Mem_Alloc(sv_clients_mempool, sizeof(client_t) * svs.maxclients);
	if (n == 1)
		Cvar_Set ("deathmatch", "0");
	else
		Cvar_Set ("deathmatch", "1");
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	Cmd_AddCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	if (gamemode == GAME_NEHAHRA)
	{
		Cmd_AddCommand ("max", Host_God_f);
		Cmd_AddCommand ("monster", Host_Notarget_f);
		Cmd_AddCommand ("scrag", Host_Fly_f);
		Cmd_AddCommand ("wraith", Host_Noclip_f);
		Cmd_AddCommand ("gimme", Host_Give_f);
	}
	else
	{
		Cmd_AddCommand ("god", Host_God_f);
		Cmd_AddCommand ("notarget", Host_Notarget_f);
		Cmd_AddCommand ("fly", Host_Fly_f);
		Cmd_AddCommand ("noclip", Host_Noclip_f);
		Cmd_AddCommand ("give", Host_Give_f);
	}
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand ("version", Host_Version_f);
	Cmd_AddCommand ("say", Host_Say_f);
	Cmd_AddCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand ("tell", Host_Tell_f);
	Cmd_AddCommand ("kill", Host_Kill_f);
	Cmd_AddCommand ("pause", Host_Pause_f);
	Cmd_AddCommand ("kick", Host_Kick_f);
	Cmd_AddCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);
	Cmd_AddCommand ("demos", Host_Demos_f);
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand ("viewframe", Host_Viewframe_f);
	Cmd_AddCommand ("viewnext", Host_Viewnext_f);
	Cmd_AddCommand ("viewprev", Host_Viewprev_f);

	Cvar_RegisterVariable (&cl_name);
	Cmd_AddCommand ("name", Host_Name_f);
	Cvar_RegisterVariable (&cl_color);
	Cmd_AddCommand ("color", Host_Color_f);
	Cvar_RegisterVariable (&cl_rate);
	Cmd_AddCommand ("rate", Host_Rate_f);
	if (gamemode == GAME_NEHAHRA)
	{
		Cvar_RegisterVariable (&cl_pmodel);
		Cmd_AddCommand ("pmodel", Host_PModel_f);
	}

	// BLACK: This isnt game specific anymore (it was GAME_NEXUIZ at first)
	Cvar_RegisterVariable (&cl_playermodel);
	Cmd_AddCommand ("playermodel", Host_Playermodel_f);
	Cvar_RegisterVariable (&cl_playerskin);
	Cmd_AddCommand ("playerskin", Host_Playerskin_f);

	Cmd_AddCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand ("begin", Host_Begin_f);
	Cmd_AddCommand ("maxplayers", MaxPlayers_f);

	Cvar_RegisterVariable(&sv_cheats);
}

