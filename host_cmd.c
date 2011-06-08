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
#include "sv_demo.h"
#include "image.h"

#include "utf8lib.h"

// for secure rcon authentication
#include "hmac.h"
#include "mdfour.h"
#include <time.h>

int current_skill;
cvar_t sv_cheats = {0, "sv_cheats", "0", "enables cheat commands in any game, and cheat impulses in dpmod"};
cvar_t sv_adminnick = {CVAR_SAVE, "sv_adminnick", "", "nick name to use for admin messages instead of host name"};
cvar_t sv_status_privacy = {CVAR_SAVE, "sv_status_privacy", "0", "do not show IP addresses in 'status' replies to clients"};
cvar_t sv_status_show_qcstatus = {CVAR_SAVE, "sv_status_show_qcstatus", "0", "show the 'qcstatus' field in status replies, not the 'frags' field. Turn this on if your mod uses this field, and the 'frags' field on the other hand has no meaningful value."};
cvar_t rcon_password = {CVAR_PRIVATE, "rcon_password", "", "password to authenticate rcon commands; NOTE: changing rcon_secure clears rcon_password, so set rcon_secure always before rcon_password; may be set to a string of the form user1:pass1 user2:pass2 user3:pass3 to allow multiple user accounts - the client then has to specify ONE of these combinations"};
cvar_t rcon_secure = {CVAR_NQUSERINFOHACK, "rcon_secure", "0", "force secure rcon authentication (1 = time based, 2 = challenge based); NOTE: changing rcon_secure clears rcon_password, so set rcon_secure always before rcon_password"};
cvar_t rcon_secure_challengetimeout = {0, "rcon_secure_challengetimeout", "5", "challenge-based secure rcon: time out requests if no challenge came within this time interval"};
cvar_t rcon_address = {0, "rcon_address", "", "server address to send rcon commands to (when not connected to a server)"};
cvar_t team = {CVAR_USERINFO | CVAR_SAVE, "team", "none", "QW team (4 character limit, example: blue)"};
cvar_t skin = {CVAR_USERINFO | CVAR_SAVE, "skin", "", "QW player skin name (example: base)"};
cvar_t noaim = {CVAR_USERINFO | CVAR_SAVE, "noaim", "1", "QW option to disable vertical autoaim"};
cvar_t r_fixtrans_auto = {0, "r_fixtrans_auto", "0", "automatically fixtrans textures (when set to 2, it also saves the fixed versions to a fixtrans directory)"};
qboolean allowcheats = false;

extern qboolean host_shuttingdown;
extern cvar_t developer_entityparsing;

/*
==================
Host_Quit_f
==================
*/

void Host_Quit_f (void)
{
	if(host_shuttingdown)
		Con_Printf("shutting down already!\n");
	else
		Sys_Quit (0);
}

/*
==================
Host_Status_f
==================
*/
void Host_Status_f (void)
{
	char qcstatus[256];
	client_t *client;
	int seconds = 0, minutes = 0, hours = 0, i, j, k, in, players, ping = 0, packetloss = 0;
	void (*print) (const char *fmt, ...);
	char ip[48]; // can contain a full length v6 address with [] and a port
	int frags;

	if (cmd_source == src_command)
	{
		// if running a client, try to send over network so the client's status report parser will see the report
		if (cls.state == ca_connected)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
		print = SV_ClientPrintf;

	if (!sv.active)
		return;
	
	if(cmd_source == src_command)
		SV_VM_Begin();
	
	in = 0;
	if (Cmd_Argc() == 2)
	{
		if (strcmp(Cmd_Argv(1), "1") == 0)
			in = 1;
		else if (strcmp(Cmd_Argv(1), "2") == 0)
			in = 2;
	}

	for (players = 0, i = 0;i < svs.maxclients;i++)
		if (svs.clients[i].active)
			players++;
	print ("host:     %s\n", Cvar_VariableString ("hostname"));
	print ("version:  %s build %s\n", gamename, buildstring);
	print ("protocol: %i (%s)\n", Protocol_NumberForEnum(sv.protocol), Protocol_NameForEnum(sv.protocol));
	print ("map:      %s\n", sv.name);
	print ("timing:   %s\n", Host_TimingReport());
	print ("players:  %i active (%i max)\n\n", players, svs.maxclients);

	if (in == 1)
		print ("^2IP                                             %%pl ping  time   frags  no   name\n");
	else if (in == 2)
		print ("^5IP                                              no   name\n");

	for (i = 0, k = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->active)
			continue;

		++k;

		if (in == 0 || in == 1)
		{
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
			
			packetloss = 0;
			if (client->netconnection)
				for (j = 0;j < NETGRAPH_PACKETS;j++)
					if (client->netconnection->incoming_netgraph[j].unreliablebytes == NETGRAPH_LOSTPACKET)
						packetloss++;
			packetloss = (packetloss * 100 + NETGRAPH_PACKETS - 1) / NETGRAPH_PACKETS;
			ping = bound(0, (int)floor(client->ping*1000+0.5), 9999);
		}

		if(sv_status_privacy.integer && cmd_source != src_command)
			strlcpy(ip, client->netconnection ? "hidden" : "botclient", 48);
		else
			strlcpy(ip, (client->netconnection && client->netconnection->address) ? client->netconnection->address : "botclient", 48);

		frags = client->frags;

		if(sv_status_show_qcstatus.integer)
		{
			prvm_edict_t *ed = PRVM_EDICT_NUM(i + 1);
			const char *str = PRVM_GetString(PRVM_serveredictstring(ed, clientstatus));
			if(str && *str)
			{
				char *p;
				const char *q;
				p = qcstatus;
				for(q = str; *q && p != qcstatus + sizeof(qcstatus) - 1; ++q)
					if(*q != '\\' && *q != '"' && !ISWHITESPACE(*q))
						*p++ = *q;
				*p = 0;
				if(*qcstatus)
					frags = atoi(qcstatus);
			}
		}
		
		if (in == 0) // default layout
		{
			if (sv.protocol == PROTOCOL_QUAKE && svs.maxclients <= 99)
			{
				// LordHavoc: this is very touchy because we must maintain ProQuake compatible status output
				print ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", i+1, client->name, frags, hours, minutes, seconds);
				print ("   %s\n", ip);
			}
			else
			{
				// LordHavoc: no real restrictions here, not a ProQuake-compatible protocol anyway...
				print ("#%-3u %-16.16s %4i  %2i:%02i:%02i\n", i+1, client->name, frags, hours, minutes, seconds);
				print ("   %s\n", ip);
			}
		}
		else if (in == 1) // extended layout
		{
			print ("%s%-47s %2i %4i %2i:%02i:%02i %4i  #%-3u ^7%s\n", k%2 ? "^3" : "^7", ip, packetloss, ping, hours, minutes, seconds, frags, i+1, client->name);
		}
		else if (in == 2) // reduced layout
		{
			print ("%s%-47s #%-3u ^7%s\n", k%2 ? "^3" : "^7", ip, i+1, client->name);
		}
	}

	if(cmd_source == src_command)
		SV_VM_End();
}


/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f (void)
{
	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	PRVM_serveredictfloat(host_client->edict, flags) = (int)PRVM_serveredictfloat(host_client->edict, flags) ^ FL_GODMODE;
	if (!((int)PRVM_serveredictfloat(host_client->edict, flags) & FL_GODMODE) )
		SV_ClientPrint("godmode OFF\n");
	else
		SV_ClientPrint("godmode ON\n");
}

void Host_Notarget_f (void)
{
	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	PRVM_serveredictfloat(host_client->edict, flags) = (int)PRVM_serveredictfloat(host_client->edict, flags) ^ FL_NOTARGET;
	if (!((int)PRVM_serveredictfloat(host_client->edict, flags) & FL_NOTARGET) )
		SV_ClientPrint("notarget OFF\n");
	else
		SV_ClientPrint("notarget ON\n");
}

qboolean noclip_anglehack;

void Host_Noclip_f (void)
{
	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	if (PRVM_serveredictfloat(host_client->edict, movetype) != MOVETYPE_NOCLIP)
	{
		noclip_anglehack = true;
		PRVM_serveredictfloat(host_client->edict, movetype) = MOVETYPE_NOCLIP;
		SV_ClientPrint("noclip ON\n");
	}
	else
	{
		noclip_anglehack = false;
		PRVM_serveredictfloat(host_client->edict, movetype) = MOVETYPE_WALK;
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
	if (!allowcheats)
	{
		SV_ClientPrint("No cheats allowed, use sv_cheats 1 and restart level to enable.\n");
		return;
	}

	if (PRVM_serveredictfloat(host_client->edict, movetype) != MOVETYPE_FLY)
	{
		PRVM_serveredictfloat(host_client->edict, movetype) = MOVETYPE_FLY;
		SV_ClientPrint("flymode ON\n");
	}
	else
	{
		PRVM_serveredictfloat(host_client->edict, movetype) = MOVETYPE_WALK;
		SV_ClientPrint("flymode OFF\n");
	}
}


/*
==================
Host_Ping_f

==================
*/
void Host_Pings_f (void); // called by Host_Ping_f
void Host_Ping_f (void)
{
	int i;
	client_t *client;
	void (*print) (const char *fmt, ...);

	if (cmd_source == src_command)
	{
		// if running a client, try to send over network so the client's ping report parser will see the report
		if (cls.state == ca_connected)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print = Con_Printf;
	}
	else
		print = SV_ClientPrintf;

	if (!sv.active)
		return;

	print("Client ping times:\n");
	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->active)
			continue;
		print("%4i %s\n", bound(0, (int)floor(client->ping*1000+0.5), 9999), client->name);
	}

	// now call the Pings command also, which will send a report that contains packet loss for the scoreboard (as well as a simpler ping report)
	// actually, don't, it confuses old clients (resulting in "unknown command pingplreport" flooding the console)
	//Host_Pings_f();
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

	// GAME_DELUXEQUAKE - clear warpmark (used by QC)
	if (gamemode == GAME_DELUXEQUAKE)
		Cvar_Set("warpmark", "");

	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer();

	if(svs.maxclients != svs.maxclients_next)
	{
		svs.maxclients = svs.maxclients_next;
		if (svs.clients)
			Mem_Free(svs.clients);
		svs.clients = (client_t *)Mem_Alloc(sv_mempool, sizeof(client_t) * svs.maxclients);
	}

	// remove menu
	if (key_dest == key_menu || key_dest == key_menu_grabbed)
		MR_ToggleMenu(0);
	key_dest = key_game;

	svs.serverflags = 0;			// haven't completed an episode yet
	allowcheats = sv_cheats.integer != 0;
	strlcpy(level, Cmd_Argv(1), sizeof(level));
	SV_SpawnServer(level);
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1", -2);
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
	// HACKHACKHACK
	if (!sv.active) {
		Host_Map_f();
		return;
	}

	// remove menu
	if (key_dest == key_menu || key_dest == key_menu_grabbed)
		MR_ToggleMenu(0);
	key_dest = key_game;

	SV_VM_Begin();
	SV_SaveSpawnparms ();
	SV_VM_End();
	allowcheats = sv_cheats.integer != 0;
	strlcpy(level, Cmd_Argv(1), sizeof(level));
	SV_SpawnServer(level);
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1", -2);
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

	// remove menu
	if (key_dest == key_menu || key_dest == key_menu_grabbed)
		MR_ToggleMenu(0);
	key_dest = key_game;

	allowcheats = sv_cheats.integer != 0;
	strlcpy(mapname, sv.name, sizeof(mapname));
	SV_SpawnServer(mapname);
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1", -2);
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
	char temp[128];
	// if not connected, reconnect to the most recent server
	if (!cls.netcon)
	{
		// if we have connected to a server recently, the userinfo
		// will still contain its IP address, so get the address...
		InfoString_GetValue(cls.userinfo, "*ip", temp, sizeof(temp));
		if (temp[0])
			CL_EstablishConnection(temp, -1);
		else
			Con_Printf("Reconnect to what server?  (you have not connected to a server yet)\n");
		return;
	}
	// if connected, do something based on protocol
	if (cls.protocol == PROTOCOL_QUAKEWORLD)
	{
		// quakeworld can just re-login
		if (cls.qw_downloadmemory)  // don't change when downloading
			return;

		S_StopAllSounds();

		if (cls.state == ca_connected && cls.signon < SIGNONS)
		{
			Con_Printf("reconnecting...\n");
			MSG_WriteChar(&cls.netcon->message, qw_clc_stringcmd);
			MSG_WriteString(&cls.netcon->message, "new");
		}
	}
	else
	{
		// netquake uses reconnect on level changes (silly)
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
	if (Cmd_Argc() < 2)
	{
		Con_Print("connect <serveraddress> [<key> <value> ...]: connect to a multiplayer game\n");
		return;
	}
	// clear the rcon password, to prevent vulnerability by stuffcmd-ing a connect command
	if(rcon_secure.integer <= 0)
		Cvar_SetQuick(&rcon_password, "");
	CL_EstablishConnection(Cmd_Argv(1), 2);
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

void Host_Savegame_to (const char *name)
{
	qfile_t	*f;
	int		i, k, l, lightstyles = 64;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];
	char	line[MAX_INPUTLINE];
	qboolean isserver;
	char	*s;

	// first we have to figure out if this can be saved in 64 lightstyles
	// (for Quake compatibility)
	for (i=64 ; i<MAX_LIGHTSTYLES ; i++)
		if (sv.lightstyles[i][0])
			lightstyles = i+1;

	isserver = !strcmp(PRVM_NAME, "server");

	Con_Printf("Saving game to %s...\n", name);
	f = FS_OpenRealFile(name, "wb", false);
	if (!f)
	{
		Con_Print("ERROR: couldn't open.\n");
		return;
	}

	FS_Printf(f, "%i\n", SAVEGAME_VERSION);

	memset(comment, 0, sizeof(comment));
	if(isserver)
		dpsnprintf(comment, sizeof(comment), "%-21.21s kills:%3i/%3i", PRVM_GetString(PRVM_serveredictstring(prog->edicts, message)), (int)PRVM_serverglobalfloat(killed_monsters), (int)PRVM_serverglobalfloat(total_monsters));
	else
		dpsnprintf(comment, sizeof(comment), "(crash dump of %s progs)", PRVM_NAME);
	// convert space to _ to make stdio happy
	// LordHavoc: convert control characters to _ as well
	for (i=0 ; i<SAVEGAME_COMMENT_LENGTH ; i++)
		if (ISWHITESPACEORCONTROL(comment[i]))
			comment[i] = '_';
	comment[SAVEGAME_COMMENT_LENGTH] = '\0';

	FS_Printf(f, "%s\n", comment);
	if(isserver)
	{
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			FS_Printf(f, "%f\n", svs.clients[0].spawn_parms[i]);
		FS_Printf(f, "%d\n", current_skill);
		FS_Printf(f, "%s\n", sv.name);
		FS_Printf(f, "%f\n",sv.time);
	}
	else
	{
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			FS_Printf(f, "(dummy)\n");
		FS_Printf(f, "%d\n", 0);
		FS_Printf(f, "%s\n", "(dummy)");
		FS_Printf(f, "%f\n", realtime);
	}

	// write the light styles
	for (i=0 ; i<lightstyles ; i++)
	{
		if (isserver && sv.lightstyles[i][0])
			FS_Printf(f, "%s\n", sv.lightstyles[i]);
		else
			FS_Print(f,"m\n");
	}

	PRVM_ED_WriteGlobals (f);
	for (i=0 ; i<prog->num_edicts ; i++)
	{
		FS_Printf(f,"// edict %d\n", i);
		//Con_Printf("edict %d...\n", i);
		PRVM_ED_Write (f, PRVM_EDICT_NUM(i));
	}

#if 1
	FS_Printf(f,"/*\n");
	FS_Printf(f,"// DarkPlaces extended savegame\n");
	// darkplaces extension - extra lightstyles, support for color lightstyles
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
		if (isserver && sv.lightstyles[i][0])
			FS_Printf(f, "sv.lightstyles %i %s\n", i, sv.lightstyles[i]);

	// darkplaces extension - model precaches
	for (i=1 ; i<MAX_MODELS ; i++)
		if (sv.model_precache[i][0])
			FS_Printf(f,"sv.model_precache %i %s\n", i, sv.model_precache[i]);

	// darkplaces extension - sound precaches
	for (i=1 ; i<MAX_SOUNDS ; i++)
		if (sv.sound_precache[i][0])
			FS_Printf(f,"sv.sound_precache %i %s\n", i, sv.sound_precache[i]);

	// darkplaces extension - save buffers
	for (i = 0; i < (int)Mem_ExpandableArray_IndexRange(&prog->stringbuffersarray); i++)
	{
		prvm_stringbuffer_t *stringbuffer = (prvm_stringbuffer_t*) Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, i);
		if(stringbuffer && (stringbuffer->flags & STRINGBUFFER_SAVED))
		{
			for(k = 0; k < stringbuffer->num_strings; k++)
			{
				if (!stringbuffer->strings[k])
					continue;
				// Parse the string a bit to turn special characters
				// (like newline, specifically) into escape codes
				s = stringbuffer->strings[k];
				for (l = 0;l < (int)sizeof(line) - 2 && *s;)
				{	
					if (*s == '\n')
					{
						line[l++] = '\\';
						line[l++] = 'n';
					}
					else if (*s == '\r')
					{
						line[l++] = '\\';
						line[l++] = 'r';
					}
					else if (*s == '\\')
					{
						line[l++] = '\\';
						line[l++] = '\\';
					}
					else if (*s == '"')
					{
						line[l++] = '\\';
						line[l++] = '"';
					}
					else
						line[l++] = *s;
					s++;
				}
				line[l] = '\0';
				FS_Printf(f,"sv.bufstr %i %i \"%s\"\n", i, k, line);
			}
		}
	}
	FS_Printf(f,"*/\n");
#endif

	FS_Close (f);
	Con_Print("done.\n");
}

/*
===============
Host_Savegame_f
===============
*/
void Host_Savegame_f (void)
{
	char	name[MAX_QPATH];
	qboolean deadflag = false;

	if (!sv.active)
	{
		Con_Print("Can't save - no server running.\n");
		return;
	}

	SV_VM_Begin();
	deadflag = cl.islocalgame && svs.clients[0].active && PRVM_serveredictfloat(svs.clients[0].edict, deadflag);
	SV_VM_End();

	if (cl.islocalgame)
	{
		// singleplayer checks
		if (cl.intermission)
		{
			Con_Print("Can't save in intermission.\n");
			return;
		}

		if (deadflag)
		{
			Con_Print("Can't savegame with a dead player\n");
			return;
		}
	}
	else
		Con_Print("Warning: saving a multiplayer game may have strange results when restored (to properly resume, all players must join in the same player slots and then the game can be reloaded).\n");

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

	SV_VM_Begin();
	Host_Savegame_to(name);
	SV_VM_End();
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
	const char *end;
	const char *t;
	char *text;
	prvm_edict_t *ent;
	int i, k;
	int entnum;
	int version;
	float spawn_parms[NUM_SPAWN_PARMS];
	prvm_stringbuffer_t *stringbuffer;
	size_t alloclen;

	if (Cmd_Argc() != 2)
	{
		Con_Print("load <savename> : load a game\n");
		return;
	}

	strlcpy (filename, Cmd_Argv(1), sizeof(filename));
	FS_DefaultExtension (filename, ".sav", sizeof (filename));

	Con_Printf("Loading game from %s...\n", filename);

	// stop playing demos
	if (cls.demoplayback)
		CL_Disconnect ();

	// remove menu
	if (key_dest == key_menu || key_dest == key_menu_grabbed)
		MR_ToggleMenu(0);
	key_dest = key_game;

	cls.demonum = -1;		// stop demo loop in case this fails

	t = text = (char *)FS_LoadFile (filename, tempmempool, false, NULL);
	if (!text)
	{
		Con_Print("ERROR: couldn't open.\n");
		return;
	}

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: loading version\n");

	// version
	COM_ParseToken_Simple(&t, false, false);
	version = atoi(com_token);
	if (version != SAVEGAME_VERSION)
	{
		Mem_Free(text);
		Con_Printf("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: loading description\n");

	// description
	COM_ParseToken_Simple(&t, false, false);

	for (i = 0;i < NUM_SPAWN_PARMS;i++)
	{
		COM_ParseToken_Simple(&t, false, false);
		spawn_parms[i] = atof(com_token);
	}
	// skill
	COM_ParseToken_Simple(&t, false, false);
// this silliness is so we can load 1.06 save files, which have float skill values
	current_skill = (int)(atof(com_token) + 0.5);
	Cvar_SetValue ("skill", (float)current_skill);

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: loading mapname\n");

	// mapname
	COM_ParseToken_Simple(&t, false, false);
	strlcpy (mapname, com_token, sizeof(mapname));

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: loading time\n");

	// time
	COM_ParseToken_Simple(&t, false, false);
	time = atof(com_token);

	allowcheats = sv_cheats.integer != 0;

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: spawning server\n");

	SV_SpawnServer (mapname);
	if (!sv.active)
	{
		Mem_Free(text);
		Con_Print("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: loading light styles\n");

// load the light styles

	SV_VM_Begin();
	// -1 is the globals
	entnum = -1;

	for (i = 0;i < MAX_LIGHTSTYLES;i++)
	{
		// light style
		start = t;
		COM_ParseToken_Simple(&t, false, false);
		// if this is a 64 lightstyle savegame produced by Quake, stop now
		// we have to check this because darkplaces may save more than 64
		if (com_token[0] == '{')
		{
			t = start;
			break;
		}
		strlcpy(sv.lightstyles[i], com_token, sizeof(sv.lightstyles[i]));
	}

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: skipping until globals\n");

	// now skip everything before the first opening brace
	// (this is for forward compatibility, so that older versions (at
	// least ones with this fix) can load savegames with extra data before the
	// first brace, as might be produced by a later engine version)
	for (;;)
	{
		start = t;
		if (!COM_ParseToken_Simple(&t, false, false))
			break;
		if (com_token[0] == '{')
		{
			t = start;
			break;
		}
	}

	// unlink all entities
	World_UnlinkAll(&sv.world);

// load the edicts out of the savegame file
	end = t;
	for (;;)
	{
		start = t;
		while (COM_ParseToken_Simple(&t, false, false))
			if (!strcmp(com_token, "}"))
				break;
		if (!COM_ParseToken_Simple(&start, false, false))
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
			if(developer_entityparsing.integer)
				Con_Printf("Host_Loadgame_f: loading globals\n");

			// parse the global vars
			PRVM_ED_ParseGlobals (start);

			// restore the autocvar globals
			Cvar_UpdateAllAutoCvars();
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
				PRVM_MEM_IncreaseEdicts();
			ent = PRVM_EDICT_NUM(entnum);
			memset(ent->fields.vp, 0, prog->entityfields * 4);
			ent->priv.server->free = false;

			if(developer_entityparsing.integer)
				Con_Printf("Host_Loadgame_f: loading edict %d\n", entnum);

			PRVM_ED_ParseEdict (start, ent);

			// link it into the bsp tree
			if (!ent->priv.server->free)
				SV_LinkEdict(ent);
		}

		end = t;
		entnum++;
	}

	prog->num_edicts = entnum;
	sv.time = time;

	for (i = 0;i < NUM_SPAWN_PARMS;i++)
		svs.clients[0].spawn_parms[i] = spawn_parms[i];

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: skipping until extended data\n");

	// read extended data if present
	// the extended data is stored inside a /* */ comment block, which the
	// parser intentionally skips, so we have to check for it manually here
	if(end)
	{
		while (*end == '\r' || *end == '\n')
			end++;
		if (end[0] == '/' && end[1] == '*' && (end[2] == '\r' || end[2] == '\n'))
		{
			if(developer_entityparsing.integer)
				Con_Printf("Host_Loadgame_f: loading extended data\n");

			Con_Printf("Loading extended DarkPlaces savegame\n");
			t = end + 2;
			memset(sv.lightstyles[0], 0, sizeof(sv.lightstyles));
			memset(sv.model_precache[0], 0, sizeof(sv.model_precache));
			memset(sv.sound_precache[0], 0, sizeof(sv.sound_precache));
			while (COM_ParseToken_Simple(&t, false, false))
			{
				if (!strcmp(com_token, "sv.lightstyles"))
				{
					COM_ParseToken_Simple(&t, false, false);
					i = atoi(com_token);
					COM_ParseToken_Simple(&t, false, false);
					if (i >= 0 && i < MAX_LIGHTSTYLES)
						strlcpy(sv.lightstyles[i], com_token, sizeof(sv.lightstyles[i]));
					else
						Con_Printf("unsupported lightstyle %i \"%s\"\n", i, com_token);
				}
				else if (!strcmp(com_token, "sv.model_precache"))
				{
					COM_ParseToken_Simple(&t, false, false);
					i = atoi(com_token);
					COM_ParseToken_Simple(&t, false, false);
					if (i >= 0 && i < MAX_MODELS)
					{
						strlcpy(sv.model_precache[i], com_token, sizeof(sv.model_precache[i]));
						sv.models[i] = Mod_ForName (sv.model_precache[i], true, false, sv.model_precache[i][0] == '*' ? sv.worldname : NULL);
					}
					else
						Con_Printf("unsupported model %i \"%s\"\n", i, com_token);
				}
				else if (!strcmp(com_token, "sv.sound_precache"))
				{
					COM_ParseToken_Simple(&t, false, false);
					i = atoi(com_token);
					COM_ParseToken_Simple(&t, false, false);
					if (i >= 0 && i < MAX_SOUNDS)
						strlcpy(sv.sound_precache[i], com_token, sizeof(sv.sound_precache[i]));
					else
						Con_Printf("unsupported sound %i \"%s\"\n", i, com_token);
				}
				else if (!strcmp(com_token, "sv.bufstr"))
				{
					COM_ParseToken_Simple(&t, false, false);
					i = atoi(com_token);
					COM_ParseToken_Simple(&t, false, false);
					k = atoi(com_token);
					COM_ParseToken_Simple(&t, false, false);
					stringbuffer = (prvm_stringbuffer_t*) Mem_ExpandableArray_RecordAtIndex(&prog->stringbuffersarray, i);
					// VorteX: nasty code, cleanup required
					// create buffer at this index
					if(!stringbuffer) 
						stringbuffer = (prvm_stringbuffer_t *) Mem_ExpandableArray_AllocRecordAtIndex(&prog->stringbuffersarray, i);
					if (!stringbuffer)
						Con_Printf("cant write string %i into buffer %i\n", k, i);
					else
					{
						// code copied from VM_bufstr_set
						// expand buffer
						if (stringbuffer->max_strings <= i)
						{
							char **oldstrings = stringbuffer->strings;
							stringbuffer->max_strings = max(stringbuffer->max_strings * 2, 128);
							while (stringbuffer->max_strings <= i)
								stringbuffer->max_strings *= 2;
							stringbuffer->strings = (char **) Mem_Alloc(prog->progs_mempool, stringbuffer->max_strings * sizeof(stringbuffer->strings[0]));
							if (stringbuffer->num_strings > 0)
								memcpy(stringbuffer->strings, oldstrings, stringbuffer->num_strings * sizeof(stringbuffer->strings[0]));
							if (oldstrings)
								Mem_Free(oldstrings);
						}
						// allocate string
						stringbuffer->num_strings = max(stringbuffer->num_strings, k + 1);
						if(stringbuffer->strings[k])
							Mem_Free(stringbuffer->strings[k]);
						stringbuffer->strings[k] = NULL;
						alloclen = strlen(com_token) + 1;
						stringbuffer->strings[k] = (char *)Mem_Alloc(prog->progs_mempool, alloclen);
						memcpy(stringbuffer->strings[k], com_token, alloclen);
					}
				}	
				// skip any trailing text or unrecognized commands
				while (COM_ParseToken_Simple(&t, true, false) && strcmp(com_token, "\n"))
					;
			}
		}
	}
	Mem_Free(text);

	if(developer_entityparsing.integer)
		Con_Printf("Host_Loadgame_f: finished\n");

	SV_VM_End();

	// make sure we're connected to loopback
	if (sv.active && cls.state == ca_disconnected)
		CL_EstablishConnection("local:1", -2);
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
cvar_t cl_name = {CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_name", "player", "internal storage cvar for current player name (changed by name command)"};
void Host_Name_f (void)
{
	int i, j;
	qboolean valid_colors;
	const char *newNameSource;
	char newName[sizeof(host_client->name)];

	if (Cmd_Argc () == 1)
	{
		Con_Printf("name: %s\n", cl_name.string);
		return;
	}

	if (Cmd_Argc () == 2)
		newNameSource = Cmd_Argv(1);
	else
		newNameSource = Cmd_Args();

	strlcpy(newName, newNameSource, sizeof(newName));

	if (cmd_source == src_command)
	{
		Cvar_Set ("_cl_name", newName);
		if (strlen(newNameSource) >= sizeof(newName)) // overflowed
		{
			Con_Printf("Your name is longer than %i chars! It has been truncated.\n", (int) (sizeof(newName) - 1));
			Con_Printf("name: %s\n", cl_name.string);
		}
		return;
	}

	if (realtime < host_client->nametime)
	{
		SV_ClientPrintf("You can't change name more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = realtime + 5;

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->name, newName, sizeof (host_client->name));

	for (i = 0, j = 0;host_client->name[i];i++)
		if (host_client->name[i] != '\r' && host_client->name[i] != '\n')
			host_client->name[j++] = host_client->name[i];
	host_client->name[j] = 0;

	if(host_client->name[0] == 1 || host_client->name[0] == 2)
	// may interfere with chat area, and will needlessly beep; so let's add a ^7
	{
		memmove(host_client->name + 2, host_client->name, sizeof(host_client->name) - 2);
		host_client->name[sizeof(host_client->name) - 1] = 0;
		host_client->name[0] = STRING_COLOR_TAG;
		host_client->name[1] = '0' + STRING_COLOR_DEFAULT;
	}

	u8_COM_StringLengthNoColors(host_client->name, 0, &valid_colors);
	if(!valid_colors) // NOTE: this also proves the string is not empty, as "" is a valid colored string
	{
		size_t l;
		l = strlen(host_client->name);
		if(l < sizeof(host_client->name) - 1)
		{
			// duplicate the color tag to escape it
			host_client->name[i] = STRING_COLOR_TAG;
			host_client->name[i+1] = 0;
			//Con_DPrintf("abuse detected, adding another trailing color tag\n");
		}
		else
		{
			// remove the last character to fix the color code
			host_client->name[l-1] = 0;
			//Con_DPrintf("abuse detected, removing a trailing color tag\n");
		}
	}

	// find the last color tag offset and decide if we need to add a reset tag
	for (i = 0, j = -1;host_client->name[i];i++)
	{
		if (host_client->name[i] == STRING_COLOR_TAG)
		{
			if (host_client->name[i+1] >= '0' && host_client->name[i+1] <= '9')
			{
				j = i;
				// if this happens to be a reset  tag then we don't need one
				if (host_client->name[i+1] == '0' + STRING_COLOR_DEFAULT)
					j = -1;
				i++;
				continue;
			}
			if (host_client->name[i+1] == STRING_COLOR_RGB_TAG_CHAR && isxdigit(host_client->name[i+2]) && isxdigit(host_client->name[i+3]) && isxdigit(host_client->name[i+4]))
			{
				j = i;
				i += 4;
				continue;
			}
			if (host_client->name[i+1] == STRING_COLOR_TAG)
			{
				i++;
				continue;
			}
		}
	}
	// does not end in the default color string, so add it
	if (j >= 0 && strlen(host_client->name) < sizeof(host_client->name) - 2)
		memcpy(host_client->name + strlen(host_client->name), STRING_COLOR_DEFAULT_STR, strlen(STRING_COLOR_DEFAULT_STR) + 1);

	PRVM_serveredictstring(host_client->edict, netname) = PRVM_SetEngineString(host_client->name);
	if (strcmp(host_client->old_name, host_client->name))
	{
		if (host_client->spawned)
			SV_BroadcastPrintf("%s ^7changed name to %s\n", host_client->old_name, host_client->name);
		strlcpy(host_client->old_name, host_client->name, sizeof(host_client->old_name));
		// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
		MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
		MSG_WriteString (&sv.reliable_datagram, host_client->name);
		SV_WriteNetnameIntoDemo(host_client);
	}
}

/*
======================
Host_Playermodel_f
======================
*/
cvar_t cl_playermodel = {CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_playermodel", "", "internal storage cvar for current player model in Nexuiz/Xonotic (changed by playermodel command)"};
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
		return;
	}

	/*
	if (realtime < host_client->nametime)
	{
		SV_ClientPrintf("You can't change playermodel more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = realtime + 5;
	*/

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->playermodel, newPath, sizeof (host_client->playermodel));
	PRVM_serveredictstring(host_client->edict, playermodel) = PRVM_SetEngineString(host_client->playermodel);
	if (strcmp(host_client->old_model, host_client->playermodel))
	{
		strlcpy(host_client->old_model, host_client->playermodel, sizeof(host_client->old_model));
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
cvar_t cl_playerskin = {CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_playerskin", "", "internal storage cvar for current player skin in Nexuiz/Xonotic (changed by playerskin command)"};
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
		return;
	}

	/*
	if (realtime < host_client->nametime)
	{
		SV_ClientPrintf("You can't change playermodel more than once every 5 seconds!\n");
		return;
	}

	host_client->nametime = realtime + 5;
	*/

	// point the string back at updateclient->name to keep it safe
	strlcpy (host_client->playerskin, newPath, sizeof (host_client->playerskin));
	PRVM_serveredictstring(host_client->edict, playerskin) = PRVM_SetEngineString(host_client->playerskin);
	if (strcmp(host_client->old_skin, host_client->playerskin))
	{
		//if (host_client->spawned)
		//	SV_BroadcastPrintf("%s changed skin to %s\n", host_client->name, host_client->playerskin);
		strlcpy(host_client->old_skin, host_client->playerskin, sizeof(host_client->old_skin));
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

	p1 = Cmd_Args();
	quoted = false;
	if (*p1 == '\"')
	{
		quoted = true;
		p1++;
	}
	// note this uses the chat prefix \001
	if (!fromServer && !teamonly)
		dpsnprintf (text, sizeof(text), "\001%s: %s", host_client->name, p1);
	else if (!fromServer && teamonly)
		dpsnprintf (text, sizeof(text), "\001(%s): %s", host_client->name, p1);
	else if(*(sv_adminnick.string))
		dpsnprintf (text, sizeof(text), "\001<%s> %s", sv_adminnick.string, p1);
	else
		dpsnprintf (text, sizeof(text), "\001<%s> %s", hostname.string, p1);
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
		if (host_client->active && (!teamonly || PRVM_serveredictfloat(host_client->edict, team) == PRVM_serveredictfloat(save->edict, team)))
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
	const char *playername_start = NULL;
	size_t playername_length = 0;
	int playernumber = 0;
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

	if (Cmd_Argc () < 2)
		return;

	// note this uses the chat prefix \001
	if (!fromServer)
		dpsnprintf (text, sizeof(text), "\001%s tells you: ", host_client->name);
	else if(*(sv_adminnick.string))
		dpsnprintf (text, sizeof(text), "\001<%s tells you> ", sv_adminnick.string);
	else
		dpsnprintf (text, sizeof(text), "\001<%s tells you> ", hostname.string);

	p1 = Cmd_Args();
	p2 = p1 + strlen(p1);
	// remove the target name
	while (p1 < p2 && *p1 == ' ')
		p1++;
	if(*p1 == '#')
	{
		++p1;
		while (p1 < p2 && *p1 == ' ')
			p1++;
		while (p1 < p2 && isdigit(*p1))
		{
			playernumber = playernumber * 10 + (*p1 - '0');
			p1++;
		}
		--playernumber;
	}
	else if(*p1 == '"')
	{
		++p1;
		playername_start = p1;
		while (p1 < p2 && *p1 != '"')
			p1++;
		playername_length = p1 - playername_start;
		if(p1 < p2)
			p1++;
	}
	else
	{
		playername_start = p1;
		while (p1 < p2 && *p1 != ' ')
			p1++;
		playername_length = p1 - playername_start;
	}
	while (p1 < p2 && *p1 == ' ')
		p1++;
	if(playername_start)
	{
		// set playernumber to the right client
		char namebuf[128];
		if(playername_length >= sizeof(namebuf))
		{
			if (fromServer)
				Con_Print("Host_Tell: too long player name/ID\n");
			else
				SV_ClientPrint("Host_Tell: too long player name/ID\n");
			return;
		}
		memcpy(namebuf, playername_start, playername_length);
		namebuf[playername_length] = 0;
		for (playernumber = 0; playernumber < svs.maxclients; playernumber++)
		{
			if (!svs.clients[playernumber].active)
				continue;
			if (strcasecmp(svs.clients[playernumber].name, namebuf) == 0)
				break;
		}
	}
	if(playernumber < 0 || playernumber >= svs.maxclients || !(svs.clients[playernumber].active))
	{
		if (fromServer)
			Con_Print("Host_Tell: invalid player name/ID\n");
		else
			SV_ClientPrint("Host_Tell: invalid player name/ID\n");
		return;
	}
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
	if(p1 == p2)
		return; // empty say
	for (j = (int)strlen(text);j < (int)(sizeof(text) - 2) && p1 < p2;)
		text[j++] = *p1++;
	text[j++] = '\n';
	text[j++] = 0;

	save = host_client;
	host_client = svs.clients + playernumber;
	SV_ClientPrint(text);
	host_client = save;
}


/*
==================
Host_Color_f
==================
*/
cvar_t cl_color = {CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_color", "0", "internal storage cvar for current player colors (changed by color command)"};
void Host_Color(int changetop, int changebottom)
{
	int top, bottom, playercolor;

	// get top and bottom either from the provided values or the current values
	// (allows changing only top or bottom, or both at once)
	top = changetop >= 0 ? changetop : (cl_color.integer >> 4);
	bottom = changebottom >= 0 ? changebottom : cl_color.integer;

	top &= 15;
	bottom &= 15;
	// LordHavoc: allowing skin colormaps 14 and 15 by commenting this out
	//if (top > 13)
	//	top = 13;
	//if (bottom > 13)
	//	bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValueQuick(&cl_color, playercolor);
		return;
	}

	if (cls.protocol == PROTOCOL_QUAKEWORLD)
		return;

	if (host_client->edict && PRVM_clientfunction(SV_ChangeTeam))
	{
		Con_DPrint("Calling SV_ChangeTeam\n");
		PRVM_serverglobalfloat(time) = sv.time;
		prog->globals.generic[OFS_PARM0] = playercolor;
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram(PRVM_clientfunction(SV_ChangeTeam), "QC function SV_ChangeTeam is missing");
	}
	else
	{
		if (host_client->edict)
		{
			PRVM_serveredictfloat(host_client->edict, clientcolors) = playercolor;
			PRVM_serveredictfloat(host_client->edict, team) = bottom + 1;
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

void Host_Color_f(void)
{
	int		top, bottom;

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
	Host_Color(top, bottom);
}

void Host_TopColor_f(void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"topcolor\" is \"%i\"\n", (cl_color.integer >> 4) & 15);
		Con_Print("topcolor <0-15>\n");
		return;
	}

	Host_Color(atoi(Cmd_Argv(1)), -1);
}

void Host_BottomColor_f(void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"bottomcolor\" is \"%i\"\n", cl_color.integer & 15);
		Con_Print("bottomcolor <0-15>\n");
		return;
	}

	Host_Color(-1, atoi(Cmd_Argv(1)));
}

cvar_t cl_rate = {CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_rate", "20000", "internal storage cvar for current rate (changed by rate command)"};
void Host_Rate_f(void)
{
	int rate;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"rate\" is \"%i\"\n", cl_rate.integer);
		Con_Print("rate <bytespersecond>\n");
		return;
	}

	rate = atoi(Cmd_Argv(1));

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_rate", max(NET_MINRATE, rate));
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
	if (PRVM_serveredictfloat(host_client->edict, health) <= 0)
	{
		SV_ClientPrint("Can't suicide -- already dead!\n");
		return;
	}

	PRVM_serverglobalfloat(time) = sv.time;
	PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
	PRVM_ExecuteProgram (PRVM_serverfunction(ClientKill), "QC function ClientKill is missing");
}


/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f (void)
{
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
cvar_t cl_pmodel = {CVAR_SAVE | CVAR_NQUSERINFOHACK, "_cl_pmodel", "0", "internal storage cvar for current player model number in nehahra (changed by pmodel command)"};
static void Host_PModel_f (void)
{
	int i;

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

	PRVM_serveredictfloat(host_client->edict, pmodel) = i;
}

//===========================================================================


/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f (void)
{
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
		host_client->sendsignon = 0;		// enable unlimited sends again
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
	int stats[MAX_CL_STATS];

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
		if (PRVM_serverfunction(RestoreGame))
		{
			Con_DPrint("Calling RestoreGame\n");
			PRVM_serverglobalfloat(time) = sv.time;
			PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
			PRVM_ExecuteProgram(PRVM_serverfunction(RestoreGame), "QC function RestoreGame is missing");
		}
	}
	else
	{
		//Con_Printf("Host_Spawn_f: host_client->edict->netname = %s, host_client->edict->netname = %s, host_client->name = %s\n", PRVM_GetString(PRVM_serveredictstring(host_client->edict, netname)), PRVM_GetString(PRVM_serveredictstring(host_client->edict, netname)), host_client->name);

		// copy spawn parms out of the client_t
		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&PRVM_serverglobalfloat(parm1))[i] = host_client->spawn_parms[i];

		// call the spawn function
		host_client->clientconnectcalled = true;
		PRVM_serverglobalfloat(time) = sv.time;
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram (PRVM_serverfunction(ClientConnect), "QC function ClientConnect is missing");

		if (cls.state == ca_dedicated)
			Con_Printf("%s connected\n", host_client->name);

		PRVM_ExecuteProgram (PRVM_serverfunction(PutClientInServer), "QC function PutClientInServer is missing");
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
	MSG_WriteLong (&host_client->netconnection->message, (int)PRVM_serverglobalfloat(total_secrets));

	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->netconnection->message, (int)PRVM_serverglobalfloat(total_monsters));

	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->netconnection->message, (int)PRVM_serverglobalfloat(found_secrets));

	MSG_WriteByte (&host_client->netconnection->message, svc_updatestat);
	MSG_WriteByte (&host_client->netconnection->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->netconnection->message, (int)PRVM_serverglobalfloat(killed_monsters));

	// send a fixangle
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	if (sv.loadgame)
	{
		MSG_WriteByte (&host_client->netconnection->message, svc_setangle);
		MSG_WriteAngle (&host_client->netconnection->message, PRVM_serveredictvector(host_client->edict, v_angle)[0], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, PRVM_serveredictvector(host_client->edict, v_angle)[1], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, 0, sv.protocol);
	}
	else
	{
		MSG_WriteByte (&host_client->netconnection->message, svc_setangle);
		MSG_WriteAngle (&host_client->netconnection->message, PRVM_serveredictvector(host_client->edict, angles)[0], sv.protocol);
		MSG_WriteAngle (&host_client->netconnection->message, PRVM_serveredictvector(host_client->edict, angles)[1], sv.protocol);
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
	host_client->spawned = true;

	// LordHavoc: note: this code also exists in SV_DropClient
	if (sv.loadgame)
	{
		int i;
		for (i = 0;i < svs.maxclients;i++)
			if (svs.clients[i].active && !svs.clients[i].spawned)
				break;
		if (i == svs.maxclients)
		{
			Con_Printf("Loaded game, everyone rejoined - unpausing\n");
			sv.paused = sv.loadgame = false; // we're basically done with loading now
		}
	}
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
	const char *who;
	const char *message = NULL;
	client_t *save;
	int i;
	qboolean byNumber = false;

	if (!sv.active)
		return;

	SV_VM_Begin();
	save = host_client;

	if (Cmd_Argc() > 2 && strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = (int)(atof(Cmd_Argv(2)) - 1);
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
			COM_ParseToken_Simple(&message, false, false);
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
					PRVM_serveredictfloat(host_client->edict, items) = (int)PRVM_serveredictfloat(host_client->edict, items) | HIT_PROXIMITY_GUN;
				else
					PRVM_serveredictfloat(host_client->edict, items) = (int)PRVM_serveredictfloat(host_client->edict, items) | IT_GRENADE_LAUNCHER;
			}
			else if (t[0] == '9')
				PRVM_serveredictfloat(host_client->edict, items) = (int)PRVM_serveredictfloat(host_client->edict, items) | HIT_LASER_CANNON;
			else if (t[0] == '0')
				PRVM_serveredictfloat(host_client->edict, items) = (int)PRVM_serveredictfloat(host_client->edict, items) | HIT_MJOLNIR;
			else if (t[0] >= '2')
				PRVM_serveredictfloat(host_client->edict, items) = (int)PRVM_serveredictfloat(host_client->edict, items) | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
			if (t[0] >= '2')
				PRVM_serveredictfloat(host_client->edict, items) = (int)PRVM_serveredictfloat(host_client->edict, items) | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 's':
		if (gamemode == GAME_ROGUE)
			PRVM_serveredictfloat(host_client->edict, ammo_shells1) = v;

		PRVM_serveredictfloat(host_client->edict, ammo_shells) = v;
		break;
	case 'n':
		if (gamemode == GAME_ROGUE)
		{
			PRVM_serveredictfloat(host_client->edict, ammo_nails1) = v;
			if (PRVM_serveredictfloat(host_client->edict, weapon) <= IT_LIGHTNING)
				PRVM_serveredictfloat(host_client->edict, ammo_nails) = v;
		}
		else
		{
			PRVM_serveredictfloat(host_client->edict, ammo_nails) = v;
		}
		break;
	case 'l':
		if (gamemode == GAME_ROGUE)
		{
			PRVM_serveredictfloat(host_client->edict, ammo_lava_nails) = v;
			if (PRVM_serveredictfloat(host_client->edict, weapon) > IT_LIGHTNING)
				PRVM_serveredictfloat(host_client->edict, ammo_nails) = v;
		}
		break;
	case 'r':
		if (gamemode == GAME_ROGUE)
		{
			PRVM_serveredictfloat(host_client->edict, ammo_rockets1) = v;
			if (PRVM_serveredictfloat(host_client->edict, weapon) <= IT_LIGHTNING)
				PRVM_serveredictfloat(host_client->edict, ammo_rockets) = v;
		}
		else
		{
			PRVM_serveredictfloat(host_client->edict, ammo_rockets) = v;
		}
		break;
	case 'm':
		if (gamemode == GAME_ROGUE)
		{
			PRVM_serveredictfloat(host_client->edict, ammo_multi_rockets) = v;
			if (PRVM_serveredictfloat(host_client->edict, weapon) > IT_LIGHTNING)
				PRVM_serveredictfloat(host_client->edict, ammo_rockets) = v;
		}
		break;
	case 'h':
		PRVM_serveredictfloat(host_client->edict, health) = v;
		break;
	case 'c':
		if (gamemode == GAME_ROGUE)
		{
			PRVM_serveredictfloat(host_client->edict, ammo_cells1) = v;
			if (PRVM_serveredictfloat(host_client->edict, weapon) <= IT_LIGHTNING)
				PRVM_serveredictfloat(host_client->edict, ammo_cells) = v;
		}
		else
		{
			PRVM_serveredictfloat(host_client->edict, ammo_cells) = v;
		}
		break;
	case 'p':
		if (gamemode == GAME_ROGUE)
		{
			PRVM_serveredictfloat(host_client->edict, ammo_plasma) = v;
			if (PRVM_serveredictfloat(host_client->edict, weapon) > IT_LIGHTNING)
				PRVM_serveredictfloat(host_client->edict, ammo_cells) = v;
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
		if (!strcmp (PRVM_GetString(PRVM_serveredictstring(e, classname)), "viewthing"))
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
	dp_model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false, true, NULL);
	if (!m || !m->loaded || !m->Draw)
	{
		Con_Printf("viewmodel: can't load %s\n", Cmd_Argv(1));
		return;
	}

	PRVM_serveredictfloat(e, frame) = 0;
	cl.model_precache[(int)PRVM_serveredictfloat(e, modelindex)] = m;
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
	dp_model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;
	m = cl.model_precache[(int)PRVM_serveredictfloat(e, modelindex)];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes-1;

	PRVM_serveredictfloat(e, frame) = f;
}


void PrintFrameName (dp_model_t *m, int frame)
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
	dp_model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;
	m = cl.model_precache[(int)PRVM_serveredictfloat(e, modelindex)];

	PRVM_serveredictfloat(e, frame) = PRVM_serveredictfloat(e, frame) + 1;
	if (PRVM_serveredictfloat(e, frame) >= m->numframes)
		PRVM_serveredictfloat(e, frame) = m->numframes - 1;

	PrintFrameName (m, (int)PRVM_serveredictfloat(e, frame));
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	prvm_edict_t	*e;
	dp_model_t	*m;

	if (!sv.active)
		return;

	SV_VM_Begin();
	e = FindViewthing ();
	SV_VM_End();
	if (!e)
		return;

	m = cl.model_precache[(int)PRVM_serveredictfloat(e, modelindex)];

	PRVM_serveredictfloat(e, frame) = PRVM_serveredictfloat(e, frame) - 1;
	if (PRVM_serveredictfloat(e, frame) < 0)
		PRVM_serveredictfloat(e, frame) = 0;

	PrintFrameName (m, (int)PRVM_serveredictfloat(e, frame));
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

	if (cls.state == ca_dedicated || COM_CheckParm("-listen") || COM_CheckParm("-benchmark") || COM_CheckParm("-demo") || COM_CheckParm("-capturedemo"))
		return;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_DPrintf("%i demo(s) in loop\n", c);

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
	const char *cvarname;
	client_t *old;

	if(Cmd_Argc() != 2)
		return;
	cvarname = Cmd_Argv(1);
	if (cls.state == ca_connected)
	{
		c = Cvar_FindVar(cvarname);
		// LordHavoc: if there is no such cvar or if it is private, send a
		// reply indicating that it has no value
		if(!c || (c->flags & CVAR_PRIVATE))
			Cmd_ForwardStringToServer(va("sentcvar %s", cvarname));
		else
			Cmd_ForwardStringToServer(va("sentcvar %s \"%s\"", c->name, c->string));
		return;
	}
	if(!sv.active)// || !PRVM_serverfunction(SV_ParseClientCommand))
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
			Host_ClientCommands("sendcvar %s\n", cvarname);
		}
	host_client = old;
}

static void MaxPlayers_f(void)
{
	int n;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("\"maxplayers\" is \"%u\"\n", svs.maxclients_next);
		return;
	}

	if (sv.active)
	{
		Con_Print("maxplayers can not be changed while a server is running.\n");
		Con_Print("It will be changed on next server startup (\"map\" command).\n");
	}

	n = atoi(Cmd_Argv(1));
	n = bound(1, n, MAX_SCOREBOARD);
	Con_Printf("\"maxplayers\" set to \"%u\"\n", n);

	svs.maxclients_next = n;
	if (n == 1)
		Cvar_Set ("deathmatch", "0");
	else
		Cvar_Set ("deathmatch", "1");
}

/*
=====================
Host_PQRcon_f

ProQuake rcon support
=====================
*/
void Host_PQRcon_f (void)
{
	int n;
	const char *e;
	lhnetaddress_t to;
	lhnetsocket_t *mysocket;
	char peer_address[64];

	if (!rcon_password.string || !rcon_password.string[0] || rcon_secure.integer > 0)
	{
		Con_Printf ("You must set rcon_password before issuing an pqrcon command, and rcon_secure must be 0.\n");
		return;
	}

	e = strchr(rcon_password.string, ' ');
	n = e ? e-rcon_password.string : (int)strlen(rcon_password.string);

	if (cls.netcon)
	{
		InfoString_GetValue(cls.userinfo, "*ip", peer_address, sizeof(peer_address));
	}
	else
	{
		if (!rcon_address.string[0])
		{
			Con_Printf ("You must either be connected, or set the rcon_address cvar to issue rcon commands\n");
			return;
		}
		strlcpy(peer_address, rcon_address.string, strlen(rcon_address.string)+1);
	}
	LHNETADDRESS_FromString(&to, peer_address, sv_netport.integer);
	mysocket = NetConn_ChooseClientSocketForAddress(&to);
	if (mysocket)
	{
		SZ_Clear(&net_message);
		MSG_WriteLong (&net_message, 0);
		MSG_WriteByte (&net_message, CCREQ_RCON);
		SZ_Write(&net_message, (const unsigned char*)rcon_password.string, n);
		MSG_WriteByte (&net_message, 0); // terminate the (possibly partial) string
		MSG_WriteString (&net_message, Cmd_Args());
		StoreBigLong(net_message.data, NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		NetConn_Write(mysocket, net_message.data, net_message.cursize, &to);
		SZ_Clear (&net_message);
	}
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
	int i, n;
	const char *e;
	lhnetaddress_t to;
	lhnetsocket_t *mysocket;

	if (!rcon_password.string || !rcon_password.string[0])
	{
		Con_Printf ("You must set rcon_password before issuing an rcon command.\n");
		return;
	}

	e = strchr(rcon_password.string, ' ');
	n = e ? e-rcon_password.string : (int)strlen(rcon_password.string);

	if (cls.netcon)
		to = cls.netcon->peeraddress;
	else
	{
		if (!rcon_address.string[0])
		{
			Con_Printf ("You must either be connected, or set the rcon_address cvar to issue rcon commands\n");
			return;
		}
		LHNETADDRESS_FromString(&to, rcon_address.string, sv_netport.integer);
	}
	mysocket = NetConn_ChooseClientSocketForAddress(&to);
	if (mysocket && Cmd_Args()[0])
	{
		// simply put together the rcon packet and send it
		if(Cmd_Argv(0)[0] == 's' || rcon_secure.integer > 1)
		{
			if(cls.rcon_commands[cls.rcon_ringpos][0])
			{
				char s[128];
				LHNETADDRESS_ToString(&cls.rcon_addresses[cls.rcon_ringpos], s, sizeof(s), true);
				Con_Printf("rcon to %s (for command %s) failed: too many buffered commands (possibly increase MAX_RCONS)\n", s, cls.rcon_commands[cls.rcon_ringpos]);
				cls.rcon_commands[cls.rcon_ringpos][0] = 0;
				--cls.rcon_trying;
			}
			for (i = 0;i < MAX_RCONS;i++)
				if(cls.rcon_commands[i][0])
					if (!LHNETADDRESS_Compare(&to, &cls.rcon_addresses[i]))
						break;
			++cls.rcon_trying;
			if(i >= MAX_RCONS)
				NetConn_WriteString(mysocket, "\377\377\377\377getchallenge", &to); // otherwise we'll request the challenge later
			strlcpy(cls.rcon_commands[cls.rcon_ringpos], Cmd_Args(), sizeof(cls.rcon_commands[cls.rcon_ringpos]));
			cls.rcon_addresses[cls.rcon_ringpos] = to;
			cls.rcon_timeout[cls.rcon_ringpos] = realtime + rcon_secure_challengetimeout.value;
			cls.rcon_ringpos = (cls.rcon_ringpos + 1) % MAX_RCONS;
		}
		else if(rcon_secure.integer > 0)
		{
			char buf[1500];
			char argbuf[1500];
			dpsnprintf(argbuf, sizeof(argbuf), "%ld.%06d %s", (long) time(NULL), (int) (rand() % 1000000), Cmd_Args());
			memcpy(buf, "\377\377\377\377srcon HMAC-MD4 TIME ", 24);
			if(HMAC_MDFOUR_16BYTES((unsigned char *) (buf + 24), (unsigned char *) argbuf, strlen(argbuf), (unsigned char *) rcon_password.string, n))
			{
				buf[40] = ' ';
				strlcpy(buf + 41, argbuf, sizeof(buf) - 41);
				NetConn_Write(mysocket, buf, 41 + strlen(buf + 41), &to);
			}
		}
		else
		{
			NetConn_WriteString(mysocket, va("\377\377\377\377rcon %.*s %s", n, rcon_password.string, Cmd_Args()), &to);
		}
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

		CL_SetInfo(key, value, false, false, false, false);
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
	CL_SetInfo(Cmd_Argv(1), Cmd_Argv(2), true, false, false, false);
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
	send[0] = send[1] = send[2] = send[3] = -1;

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
	if (!mysocket)
		mysocket = NetConn_ChooseServerSocketForAddress(&address);
	if (mysocket)
		NetConn_Write(mysocket, send, out - send, &address);
}

/*
====================
Host_Pings_f

Send back ping and packet loss update for all current players to this player
====================
*/
void Host_Pings_f (void)
{
	int		i, j, ping, packetloss, movementloss;
	char temp[128];

	if (!host_client->netconnection)
		return;

	if (sv.protocol != PROTOCOL_QUAKEWORLD)
	{
		MSG_WriteByte(&host_client->netconnection->message, svc_stufftext);
		MSG_WriteUnterminatedString(&host_client->netconnection->message, "pingplreport");
	}
	for (i = 0;i < svs.maxclients;i++)
	{
		packetloss = 0;
		movementloss = 0;
		if (svs.clients[i].netconnection)
		{
			for (j = 0;j < NETGRAPH_PACKETS;j++)
				if (svs.clients[i].netconnection->incoming_netgraph[j].unreliablebytes == NETGRAPH_LOSTPACKET)
					packetloss++;
			for (j = 0;j < NETGRAPH_PACKETS;j++)
				if (svs.clients[i].movement_count[j] < 0)
					movementloss++;
		}
		packetloss = (packetloss * 100 + NETGRAPH_PACKETS - 1) / NETGRAPH_PACKETS;
		movementloss = (movementloss * 100 + NETGRAPH_PACKETS - 1) / NETGRAPH_PACKETS;
		ping = (int)floor(svs.clients[i].ping*1000+0.5);
		ping = bound(0, ping, 9999);
		if (sv.protocol == PROTOCOL_QUAKEWORLD)
		{
			// send qw_svc_updateping and qw_svc_updatepl messages
			MSG_WriteByte(&host_client->netconnection->message, qw_svc_updateping);
			MSG_WriteShort(&host_client->netconnection->message, ping);
			MSG_WriteByte(&host_client->netconnection->message, qw_svc_updatepl);
			MSG_WriteByte(&host_client->netconnection->message, packetloss);
		}
		else
		{
			// write the string into the packet as multiple unterminated strings to avoid needing a local buffer
			if(movementloss)
				dpsnprintf(temp, sizeof(temp), " %d %d,%d", ping, packetloss, movementloss);
			else
				dpsnprintf(temp, sizeof(temp), " %d %d", ping, packetloss);
			MSG_WriteUnterminatedString(&host_client->netconnection->message, temp);
		}
	}
	if (sv.protocol != PROTOCOL_QUAKEWORLD)
		MSG_WriteString(&host_client->netconnection->message, "\n");
}

void Host_PingPLReport_f(void)
{
	char *errbyte;
	int i;
	int l = Cmd_Argc();
	if (l > cl.maxclients)
		l = cl.maxclients;
	for (i = 0;i < l;i++)
	{
		cl.scores[i].qw_ping = atoi(Cmd_Argv(1+i*2));
		cl.scores[i].qw_packetloss = strtol(Cmd_Argv(1+i*2+1), &errbyte, 0);
		if(errbyte && *errbyte == ',')
			cl.scores[i].qw_movementloss = atoi(errbyte + 1);
		else
			cl.scores[i].qw_movementloss = 0;
	}
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	dpsnprintf(cls.userinfo, sizeof(cls.userinfo), "\\name\\player\\team\\none\\topcolor\\0\\bottomcolor\\0\\rate\\10000\\msg\\1\\noaim\\1\\*ver\\dp");

	Cmd_AddCommand_WithClientCommand ("status", Host_Status_f, Host_Status_f, "print server status information");
	Cmd_AddCommand ("quit", Host_Quit_f, "quit the game");
	Cmd_AddCommand_WithClientCommand ("god", NULL, Host_God_f, "god mode (invulnerability)");
	Cmd_AddCommand_WithClientCommand ("notarget", NULL, Host_Notarget_f, "notarget mode (monsters do not see you)");
	Cmd_AddCommand_WithClientCommand ("fly", NULL, Host_Fly_f, "fly mode (flight)");
	Cmd_AddCommand_WithClientCommand ("noclip", NULL, Host_Noclip_f, "noclip mode (flight without collisions, move through walls)");
	Cmd_AddCommand_WithClientCommand ("give", NULL, Host_Give_f, "alter inventory");
	Cmd_AddCommand ("map", Host_Map_f, "kick everyone off the server and start a new level");
	Cmd_AddCommand ("restart", Host_Restart_f, "restart current level");
	Cmd_AddCommand ("changelevel", Host_Changelevel_f, "change to another level, bringing along all connected clients");
	Cmd_AddCommand ("connect", Host_Connect_f, "connect to a server by IP address or hostname");
	Cmd_AddCommand ("reconnect", Host_Reconnect_f, "reconnect to the last server you were on, or resets a quakeworld connection (do not use if currently playing on a netquake server)");
	Cmd_AddCommand ("version", Host_Version_f, "print engine version");
	Cmd_AddCommand_WithClientCommand ("say", Host_Say_f, Host_Say_f, "send a chat message to everyone on the server");
	Cmd_AddCommand_WithClientCommand ("say_team", Host_Say_Team_f, Host_Say_Team_f, "send a chat message to your team on the server");
	Cmd_AddCommand_WithClientCommand ("tell", Host_Tell_f, Host_Tell_f, "send a chat message to only one person on the server");
	Cmd_AddCommand_WithClientCommand ("kill", NULL, Host_Kill_f, "die instantly");
	Cmd_AddCommand_WithClientCommand ("pause", NULL, Host_Pause_f, "pause the game (if the server allows pausing)");
	Cmd_AddCommand ("kick", Host_Kick_f, "kick a player off the server by number or name");
	Cmd_AddCommand_WithClientCommand ("ping", Host_Ping_f, Host_Ping_f, "print ping times of all players on the server");
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
	Cmd_AddCommand_WithClientCommand ("name", Host_Name_f, Host_Name_f, "change your player name");
	Cvar_RegisterVariable (&cl_color);
	Cmd_AddCommand_WithClientCommand ("color", Host_Color_f, Host_Color_f, "change your player shirt and pants colors");
	Cvar_RegisterVariable (&cl_rate);
	Cmd_AddCommand_WithClientCommand ("rate", Host_Rate_f, Host_Rate_f, "change your network connection speed");
	Cvar_RegisterVariable (&cl_pmodel);
	Cmd_AddCommand_WithClientCommand ("pmodel", Host_PModel_f, Host_PModel_f, "(Nehahra-only) change your player model choice");

	// BLACK: This isnt game specific anymore (it was GAME_NEXUIZ at first)
	Cvar_RegisterVariable (&cl_playermodel);
	Cmd_AddCommand_WithClientCommand ("playermodel", Host_Playermodel_f, Host_Playermodel_f, "change your player model");
	Cvar_RegisterVariable (&cl_playerskin);
	Cmd_AddCommand_WithClientCommand ("playerskin", Host_Playerskin_f, Host_Playerskin_f, "change your player skin number");

	Cmd_AddCommand_WithClientCommand ("prespawn", NULL, Host_PreSpawn_f, "signon 1 (client acknowledges that server information has been received)");
	Cmd_AddCommand_WithClientCommand ("spawn", NULL, Host_Spawn_f, "signon 2 (client has sent player information, and is asking server to send scoreboard rankings)");
	Cmd_AddCommand_WithClientCommand ("begin", NULL, Host_Begin_f, "signon 3 (client asks server to start sending entities, and will go to signon 4 (playing) when the first entity update is received)");
	Cmd_AddCommand ("maxplayers", MaxPlayers_f, "sets limit on how many players (or bots) may be connected to the server at once");

	Cmd_AddCommand ("sendcvar", Host_SendCvar_f, "sends the value of a cvar to the server as a sentcvar command, for use by QuakeC");

	Cvar_RegisterVariable (&rcon_password);
	Cvar_RegisterVariable (&rcon_address);
	Cvar_RegisterVariable (&rcon_secure);
	Cvar_RegisterVariable (&rcon_secure_challengetimeout);
	Cmd_AddCommand ("rcon", Host_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's); note: if rcon_secure is set, client and server clocks must be synced e.g. via NTP");
	Cmd_AddCommand ("srcon", Host_Rcon_f, "sends a command to the server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's); this always works as if rcon_secure is set; note: client and server clocks must be synced e.g. via NTP");
	Cmd_AddCommand ("pqrcon", Host_PQRcon_f, "sends a command to a proquake server console (if your rcon_password matches the server's rcon_password), or to the address specified by rcon_address when not connected (again rcon_password must match the server's)");
	Cmd_AddCommand ("user", Host_User_f, "prints additional information about a player number or name on the scoreboard");
	Cmd_AddCommand ("users", Host_Users_f, "prints additional information about all players on the scoreboard");
	Cmd_AddCommand ("fullserverinfo", Host_FullServerinfo_f, "internal use only, sent by server to client to update client's local copy of serverinfo string");
	Cmd_AddCommand ("fullinfo", Host_FullInfo_f, "allows client to modify their userinfo");
	Cmd_AddCommand ("setinfo", Host_SetInfo_f, "modifies your userinfo");
	Cmd_AddCommand ("packet", Host_Packet_f, "send a packet to the specified address:port containing a text string");
	Cmd_AddCommand ("topcolor", Host_TopColor_f, "QW command to set top color without changing bottom color");
	Cmd_AddCommand ("bottomcolor", Host_BottomColor_f, "QW command to set bottom color without changing top color");

	Cmd_AddCommand_WithClientCommand ("pings", NULL, Host_Pings_f, "command sent by clients to request updated ping and packetloss of players on scoreboard (originally from QW, but also used on NQ servers)");
	Cmd_AddCommand ("pingplreport", Host_PingPLReport_f, "command sent by server containing client ping and packet loss values for scoreboard, triggered by pings command from client (not used by QW servers)");

	Cmd_AddCommand ("fixtrans", Image_FixTransparentPixels_f, "change alpha-zero pixels in an image file to sensible values, and write out a new TGA (warning: SLOW)");
	Cvar_RegisterVariable (&r_fixtrans_auto);

	Cvar_RegisterVariable (&team);
	Cvar_RegisterVariable (&skin);
	Cvar_RegisterVariable (&noaim);

	Cvar_RegisterVariable(&sv_cheats);
	Cvar_RegisterVariable(&sv_adminnick);
	Cvar_RegisterVariable(&sv_status_privacy);
	Cvar_RegisterVariable(&sv_status_show_qcstatus);
}

void Host_NoOperation_f(void)
{
}
