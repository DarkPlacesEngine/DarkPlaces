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
// host.c -- coordinates spawning and killing of local servers

#include <time.h>
#include "quakedef.h"
#include "cdaudio.h"
#include "cl_video.h"
#include "progsvm.h"

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

// true if into command execution
qboolean host_initialized;
// LordHavoc: used to turn Host_Error into Sys_Error if starting up or shutting down
qboolean host_loopactive = false;
// LordHavoc: set when quit is executed
qboolean host_shuttingdown = false;

double host_frametime;
// LordHavoc: the real frametime, before slowmo and clamping are applied (used for console scrolling)
double host_realframetime;
// the real time, without any slowmo or clamping
double realtime;
// realtime from previous frame
double oldrealtime;
// how many frames have occurred
int host_framecount;

// used for -developer commandline parameter, hacky hacky
int forcedeveloper;

// current client
client_t *host_client;

jmp_buf host_abortserver;

// pretend frames take this amount of time (in seconds), 0 = realtime
cvar_t host_framerate = {0, "host_framerate","0"};
// shows time used by certain subsystems
cvar_t host_speeds = {0, "host_speeds","0"};
// LordHavoc: framerate independent slowmo
cvar_t slowmo = {0, "slowmo", "1.0"};
// LordHavoc: framerate upper cap
cvar_t cl_maxfps = {CVAR_SAVE, "cl_maxfps", "1000"};

// print broadcast messages in dedicated mode
cvar_t sv_echobprint = {CVAR_SAVE, "sv_echobprint", "1"};

cvar_t sys_ticrate = {CVAR_SAVE, "sys_ticrate","0.05"};
cvar_t serverprofile = {0, "serverprofile","0"};

cvar_t fraglimit = {CVAR_NOTIFY, "fraglimit","0"};
cvar_t timelimit = {CVAR_NOTIFY, "timelimit","0"};
cvar_t teamplay = {CVAR_NOTIFY, "teamplay","0"};

cvar_t samelevel = {0, "samelevel","0"};
cvar_t noexit = {CVAR_NOTIFY, "noexit","0"};

cvar_t developer = {0, "developer","0"};

cvar_t skill = {0, "skill","1"};
cvar_t deathmatch = {0, "deathmatch","0"};
cvar_t coop = {0, "coop","0"};

cvar_t pausable = {0, "pausable","1"};

cvar_t temp1 = {0, "temp1","0"};

cvar_t timestamps = {CVAR_SAVE, "timestamps", "0"};
cvar_t timeformat = {CVAR_SAVE, "timeformat", "[%b %e %X] "};

/*
================
Host_Error

This shuts down both the client and server
================
*/
void PRVM_ProcessError(void);
static char hosterrorstring1[4096];
static char hosterrorstring2[4096];
static qboolean hosterror = false;
void Host_Error (const char *error, ...)
{
	va_list argptr;

	va_start (argptr,error);
	dpvsnprintf (hosterrorstring1,sizeof(hosterrorstring1),error,argptr);
	va_end (argptr);

	Con_Printf("Host_Error: %s\n", hosterrorstring1);

	// LordHavoc: if first frame has not been shown, or currently shutting
	// down, do Sys_Error instead
	if (!host_loopactive || host_shuttingdown)
		Sys_Error ("Host_Error: %s", hosterrorstring1);

	if (hosterror)
		Sys_Error ("Host_Error: recursively entered (original error was: %s    new error is: %s)", hosterrorstring2, hosterrorstring1);
	hosterror = true;

	strcpy(hosterrorstring2, hosterrorstring1);

	CL_Parse_DumpPacket();

	PR_Crash();

	//PRVM_Crash(); // crash current prog

	// crash all prvm progs
	PRVM_CrashAll();

	PRVM_ProcessError();

	Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s\n",hosterrorstring2);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;

	hosterror = false;

	longjmp (host_abortserver, 1);
}

mempool_t *sv_clients_mempool = NULL;

void Host_ServerOptions (void)
{
	int i, numplayers;

	// general default
	numplayers = 8;

// COMMANDLINEOPTION: Server: -dedicated [playerlimit] starts a dedicated server (with a command console), default playerlimit is 8
// COMMANDLINEOPTION: Server: -listen [playerlimit] starts a multiplayer server with graphical client, like singleplayer but other players can connect, default playerlimit is 8
	if (cl_available)
	{
		// client exists, check what mode the user wants
		i = COM_CheckParm ("-dedicated");
		if (i)
		{
			cls.state = ca_dedicated;
			// default players unless specified
			if (i != (com_argc - 1))
				numplayers = atoi (com_argv[i+1]);
			if (COM_CheckParm ("-listen"))
				Sys_Error ("Only one of -dedicated or -listen can be specified");
		}
		else
		{
			cls.state = ca_disconnected;
			i = COM_CheckParm ("-listen");
			if (i)
			{
				// default players unless specified
				if (i != (com_argc - 1))
					numplayers = atoi (com_argv[i+1]);
			}
			else
			{
				// default players in some games, singleplayer in most
				if (gamemode != GAME_TRANSFUSION && gamemode != GAME_GOODVSBAD2 && gamemode != GAME_NEXUIZ && gamemode != GAME_BATTLEMECH)
					numplayers = 1;
			}
		}
	}
	else
	{
		// no client in the executable, always start dedicated server
		if (COM_CheckParm ("-listen"))
			Sys_Error ("-listen not available in a dedicated server executable");
		cls.state = ca_dedicated;
		// check for -dedicated specifying how many players
		i = COM_CheckParm ("-dedicated");
		// default players unless specified
		if (i && i != (com_argc - 1))
			numplayers = atoi (com_argv[i+1]);
	}

	if (numplayers < 1)
		numplayers = 8;

	numplayers = bound(1, numplayers, MAX_SCOREBOARD);

	if (numplayers > 1 && !deathmatch.integer)
		Cvar_SetValueQuick(&deathmatch, 1);

	svs.maxclients = numplayers;
	sv_clients_mempool = Mem_AllocPool("server clients", 0, NULL);
	svs.clients = Mem_Alloc(sv_clients_mempool, sizeof(client_t) * svs.maxclients);
}

/*
=======================
Host_InitLocal
======================
*/
void Host_SaveConfig_f(void);
void Host_InitLocal (void)
{
	Host_InitCommands ();

	Cmd_AddCommand("saveconfig", Host_SaveConfig_f);

	Cvar_RegisterVariable (&host_framerate);
	Cvar_RegisterVariable (&host_speeds);
	Cvar_RegisterVariable (&slowmo);
	Cvar_RegisterVariable (&cl_maxfps);

	Cvar_RegisterVariable (&sv_echobprint);

	Cvar_RegisterVariable (&sys_ticrate);
	Cvar_RegisterVariable (&serverprofile);

	Cvar_RegisterVariable (&fraglimit);
	Cvar_RegisterVariable (&timelimit);
	Cvar_RegisterVariable (&teamplay);
	Cvar_RegisterVariable (&samelevel);
	Cvar_RegisterVariable (&noexit);
	Cvar_RegisterVariable (&skill);
	Cvar_RegisterVariable (&developer);
	if (forcedeveloper) // make it real now that the cvar is registered
		Cvar_SetValue("developer", 1);
	Cvar_RegisterVariable (&deathmatch);
	Cvar_RegisterVariable (&coop);

	Cvar_RegisterVariable (&pausable);

	Cvar_RegisterVariable (&temp1);

	Cvar_RegisterVariable (&timestamps);
	Cvar_RegisterVariable (&timeformat);

	Host_ServerOptions ();
}


/*
===============
Host_SaveConfig_f

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_SaveConfig_f(void)
{
	qfile_t *f;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	if (host_initialized && cls.state != ca_dedicated)
	{
		f = FS_Open ("config.cfg", "wb", false, false);
		if (!f)
		{
			Con_Print("Couldn't write config.cfg.\n");
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVariables (f);

		FS_Close (f);
	}
}


/*
=================
SV_ClientPrint

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrint(const char *msg)
{
	MSG_WriteByte(&host_client->message, svc_print);
	MSG_WriteString(&host_client->message, msg);
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[4096];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	SV_ClientPrint(msg);
}

/*
=================
SV_BroadcastPrint

Sends text to all active clients
=================
*/
void SV_BroadcastPrint(const char *msg)
{
	int i;
	client_t *client;

	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (client->spawned)
		{
			MSG_WriteByte(&client->message, svc_print);
			MSG_WriteString(&client->message, msg);
		}
	}

	if (sv_echobprint.integer && cls.state == ca_dedicated)
		Con_Print(msg);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[4096];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	SV_BroadcastPrint(msg);
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands(const char *fmt, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr,fmt);
	dpvsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->message, svc_stufftext);
	MSG_WriteString(&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient(qboolean crash)
{
	int i;
	Con_Printf("Client \"%s\" dropped\n", host_client->name);

	// make sure edict is not corrupt (from a level change for example)
	host_client->edict = EDICT_NUM(host_client - svs.clients + 1);

	if (host_client->netconnection)
	{
		// free the client (the body stays around)
		if (!crash)
		{
			// LordHavoc: no opportunity for resending, so use unreliable
			MSG_WriteByte(&host_client->message, svc_disconnect);
			NetConn_SendUnreliableMessage(host_client->netconnection, &host_client->message);
		}
		// break the net connection
		NetConn_Close(host_client->netconnection);
		host_client->netconnection = NULL;
	}

	// call qc ClientDisconnect function
	// LordHavoc: don't call QC if server is dead (avoids recursive
	// Host_Error in some mods when they run out of edicts)
	if (host_client->active && sv.active && host_client->edict && host_client->spawned)
	{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
		int saveSelf = pr_global_struct->self;
		pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
		PR_ExecuteProgram(pr_global_struct->ClientDisconnect, "QC function ClientDisconnect is missing");
		pr_global_struct->self = saveSelf;
	}

	// remove leaving player from scoreboard
	//host_client->edict->v->netname = PR_SetString(host_client->name);
	//if ((val = GETEDICTFIELDVALUE(host_client->edict, eval_clientcolors)))
	//	val->_float = 0;
	//host_client->edict->v->frags = 0;
	host_client->name[0] = 0;
	host_client->colors = 0;
	host_client->frags = 0;
	// send notification to all clients
	// get number of client manually just to make sure we get it right...
	i = host_client - svs.clients;
	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
	MSG_WriteByte (&sv.reliable_datagram, svc_updatefrags);
	MSG_WriteByte (&sv.reliable_datagram, i);
	MSG_WriteShort (&sv.reliable_datagram, host_client->frags);

	// free the client now
	if (host_client->entitydatabase)
		EntityFrame_FreeDatabase(host_client->entitydatabase);
	if (host_client->entitydatabase4)
		EntityFrame4_FreeDatabase(host_client->entitydatabase4);
	if (host_client->entitydatabase5)
		EntityFrame5_FreeDatabase(host_client->entitydatabase5);

	if (sv.active)
	{
		// clear a fields that matter to DP_SV_CLIENTNAME and DP_SV_CLIENTCOLORS, and also frags
		ED_ClearEdict(host_client->edict);
	}

	// clear the client struct (this sets active to false)
	memset(host_client, 0, sizeof(*host_client));

	// update server listing on the master because player count changed
	// (which the master uses for filtering empty/full servers)
	NetConn_Heartbeat(1);
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qboolean crash)
{
	int i, count;
	sizebuf_t buf;
	char message[4];

	Con_DPrintf("Host_ShutdownServer\n");

	if (!sv.active)
		return;

	// print out where the crash happened, if it was caused by QC
	PR_Crash();

	NetConn_Heartbeat(2);
	NetConn_Heartbeat(2);

// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NetConn_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NetConn_SendToAll failed for %u clients\n", count);

	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash); // server shutdown

	NetConn_CloseServerPorts();

	sv.active = false;

//
// clear structures
//
	memset(&sv, 0, sizeof(sv));
	memset(svs.clients, 0, svs.maxclients*sizeof(client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrint("Clearing memory\n");
	Mod_ClearAll ();

	cls.signon = 0;
	memset (&sv, 0, sizeof(sv));
	memset (&cl, 0, sizeof(cl));
}


//============================================================================

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
extern qboolean cl_capturevideo_active;
extern double cl_capturevideo_framerate;
qboolean Host_FilterTime (double time)
{
	double timecap, timeleft;
	realtime += time;

	if (sys_ticrate.value < 0.00999 || sys_ticrate.value > 0.10001)
		Cvar_SetValue("sys_ticrate", bound(0.01, sys_ticrate.value, 0.1));
	if (slowmo.value < 0)
		Cvar_SetValue("slowmo", 0);
	if (host_framerate.value < 0.00001 && host_framerate.value != 0)
		Cvar_SetValue("host_framerate", 0);
	if (cl_maxfps.value < 1)
		Cvar_SetValue("cl_maxfps", 1);

	if (cls.timedemo)
	{
		// disable time effects during timedemo
		cl.frametime = host_realframetime = host_frametime = realtime - oldrealtime;
		oldrealtime = realtime;
		return true;
	}

	// check if framerate is too high
	// default to sys_ticrate (server framerate - presumably low) unless we
	// have a good reason to run faster
	timecap = host_framerate.value;
	if (!timecap)
		timecap = sys_ticrate.value;
	if (cls.state != ca_dedicated)
	{
		if (cl_capturevideo_active)
			timecap = 1.0 / cl_capturevideo_framerate;
		else if (vid_activewindow)
			timecap = 1.0 / cl_maxfps.value;
	}

	timeleft = (oldrealtime - realtime) + timecap;
	if (timeleft > 0)
	{
		int msleft;
		// don't totally hog the CPU
		if (cls.state == ca_dedicated)
		{
			// if dedicated, try to use as little cpu as possible by waiting
			// just a little longer than necessary
			// (yes this means it doesn't quite keep up with the framerate)
			msleft = (int)ceil(timeleft * 1000);
		}
		else
		{
			// if not dedicated, try to hit exactly a steady framerate by not
			// sleeping the full amount
			msleft = (int)floor(timeleft * 1000);
		}
		if (msleft > 0)
			Sys_Sleep(msleft);
		return false;
	}

	// LordHavoc: copy into host_realframetime as well
	host_realframetime = host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	// apply slowmo scaling
	host_frametime *= slowmo.value;

	// host_framerate overrides all else
	if (host_framerate.value)
		host_frametime = host_framerate.value;

	// never run a frame longer than 1 second
	if (host_frametime > 1)
		host_frametime = 1;

	cl.frametime = host_frametime;

	return true;
}


/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void Host_GetConsoleCommands (void)
{
	char *cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
}


/*
==================
Host_ServerFrame

==================
*/
void Host_ServerFrame (void)
{
	// never run more than 5 frames at a time as a sanity limit
	int framecount, framelimit = 5;
	double advancetime, newtime;
	if (!sv.active)
		return;
	newtime = Sys_DoubleTime();
	// if this is the first frame of a new server, ignore the huge time difference
	if (!sv.timer)
		sv.timer = newtime;
	// if we're already past the new time, don't run a frame
	// (does not happen if cl.islocalgame)
	if (sv.timer > newtime)
		return;
	// run the world state
	// don't allow simulation to run too fast or too slow or logic glitches can occur
	for (framecount = 0;framecount < framelimit && sv.timer < newtime;framecount++)
	{
		if (cl.islocalgame)
			advancetime = min(newtime - sv.timer, sys_ticrate.value);
		else
			advancetime = sys_ticrate.value;
		sv.timer += advancetime;

		// only advance time if not paused
		// the game also pauses in singleplayer when menu or console is used
		sv.frametime = advancetime * slowmo.value;
		if (sv.paused || (cl.islocalgame && (key_dest != key_game || key_consoleactive)))
			sv.frametime = 0;

		pr_global_struct->frametime = sv.frametime;

		// set the time and clear the general datagram
		SV_ClearDatagram();

		// check for network packets to the server each world step incase they
		// come in midframe (particularly if host is running really slow)
		NetConn_ServerFrame();

		// read client messages
		SV_RunClients();

		// move things around and think unless paused
		if (sv.frametime)
			SV_Physics();

		// send all messages to the clients
		SV_SendClientMessages();

		// send an heartbeat if enough time has passed since the last one
		NetConn_Heartbeat(0);
	}
	// if we fell behind too many frames just don't worry about it
	if (sv.timer < newtime)
		sv.timer = newtime;
}


/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (float time)
{
	static double time1 = 0;
	static double time2 = 0;
	static double time3 = 0;
	int pass1, pass2, pass3;

	if (setjmp(host_abortserver))
		return;			// something bad happened, or the server disconnected

	// decide the simulation time
	if (!Host_FilterTime(time))
		return;

	// keep the random time dependent
	rand();

	cl.islocalgame = NetConn_IsLocalGame();

	// get new key events
	Sys_SendKeyEvents();

	// allow mice or other external controllers to add commands
	IN_Commands();

	// Collect input into cmd
	IN_ProcessMove();

	// process console commands
	Cbuf_Execute();

	// if running the server locally, make intentions now
	if (cls.state == ca_connected && sv.active)
		CL_SendCmd();

//-------------------
//
// server operations
//
//-------------------

	// check for commands typed to the host
	Host_GetConsoleCommands();

	if (sv.active)
		Host_ServerFrame();

//-------------------
//
// client operations
//
//-------------------

	cl.oldtime = cl.time;
	cl.time += cl.frametime;

	NetConn_ClientFrame();

	if (cls.state == ca_connected)
	{
		// if running the server remotely, send intentions now after
		// the incoming messages have been read
		if (!sv.active)
			CL_SendCmd();
		CL_ReadFromServer();
	}

	//ui_update();

	CL_VideoFrame();

	// update video
	if (host_speeds.integer)
		time1 = Sys_DoubleTime();

	CL_UpdateScreen();

	if (host_speeds.integer)
		time2 = Sys_DoubleTime();

	// update audio
	if (cls.signon == SIGNONS && cl.viewentity >= 0 && cl.viewentity < MAX_EDICTS && cl_entities[cl.viewentity].state_current.active)
	{
		// LordHavoc: this used to use renderer variables (eww)
		S_Update(&cl_entities[cl.viewentity].render.matrix);
	}
	else
		S_Update(&identitymatrix);

	CDAudio_Update();

	if (host_speeds.integer)
	{
		pass1 = (time1 - time3)*1000000;
		time3 = Sys_DoubleTime();
		pass2 = (time2 - time1)*1000000;
		pass3 = (time3 - time2)*1000000;
		Con_Printf("%6ius total %6ius server %6ius gfx %6ius snd\n",
					pass1+pass2+pass3, pass1, pass2, pass3);
	}

	host_framecount++;
	host_loopactive = true;

}

void Host_Frame (float time)
{
	double time1, time2;
	static double timetotal;
	static int timecount;
	int i, c, m;

	if (!serverprofile.integer)
	{
		_Host_Frame (time);
		return;
	}

	time1 = Sys_DoubleTime ();
	_Host_Frame (time);
	time2 = Sys_DoubleTime ();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal*1000/timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active)
			c++;
	}

	Con_Printf("serverprofile: %2i clients %2i msec\n",  c,  m);
}

//============================================================================

void Render_Init(void);

/*
====================
Host_Init
====================
*/
void Host_Init (void)
{
	int i;

	// LordHavoc: quake never seeded the random number generator before... heh
	srand(time(NULL));

	// FIXME: this is evil, but possibly temporary
// COMMANDLINEOPTION: Console: -developer enables warnings and other notices (RECOMMENDED for mod developers)
	if (COM_CheckParm("-developer"))
	{
		forcedeveloper = true;
		developer.integer = 1;
		developer.value = 1;
	}

	Cmd_Init();
	Memory_Init_Commands();
	R_Modules_Init();
	Cbuf_Init();
	V_Init();
	COM_Init();
	Host_InitLocal();
	Key_Init();
	Con_Init();
	PR_Init();
	PRVM_Init();
	Mod_Init();
	NetConn_Init();
	SV_Init();

	Con_Printf("Builddate: %s\n", buildstring);

	if (cls.state != ca_dedicated)
	{
		Palette_Init();
		MR_Init_Commands();
		VID_Shared_Init();
		VID_Init();

		Render_Init();
		S_Init();
		CDAudio_Init();
		CL_Init();
	}

	// only cvars are executed when host_initialized == false
	if (gamemode == GAME_TEU)
		Cbuf_InsertText("exec teu.rc\n");
	else
		Cbuf_InsertText("exec quake.rc\n");
	Cbuf_Execute();

	host_initialized = true;

	Con_DPrint("========Initialized=========\n");

	if (cls.state != ca_dedicated)
	{
		VID_Open();
		CDAudio_Startup();
		CL_InitTEnts ();  // We must wait after sound startup to load tent sounds
		SCR_BeginLoadingPlaque();
		MR_Init();
	}

	// set up the default startmap_sp and startmap_dm aliases, mods can
	// override these
	if (gamemode == GAME_NEHAHRA)
	{
		Cbuf_InsertText ("alias startmap_sp \"map nehstart\"\n");
		Cbuf_InsertText ("alias startmap_dm \"map nehstart\"\n");
	}
	else if (gamemode == GAME_TRANSFUSION)
	{
		Cbuf_InsertText ("alias startmap_sp \"map e1m1\"\n");
		Cbuf_InsertText ("alias startmap_dm \"map bb1\"\n");
	}
	else if (gamemode == GAME_NEXUIZ)
	{
		Cbuf_InsertText ("alias startmap_sp \"map nexdm01\"\n");
		Cbuf_InsertText ("alias startmap_dm \"map nexdm01\"\n");
	}
	else
	{
		Cbuf_InsertText ("alias startmap_sp \"map start\"\n");
		Cbuf_InsertText ("alias startmap_dm \"map start\"\n");
	}

	// stuff it again so the first host frame will execute it again, this time
	// in its entirety
	if (gamemode == GAME_TEU)
		Cbuf_InsertText("exec teu.rc\n");
	else
		Cbuf_InsertText("exec quake.rc\n");

	Cbuf_Execute();
	Cbuf_Execute();
	Cbuf_Execute();

	if (!sv.active && (cls.state == ca_dedicated || COM_CheckParm("-listen")))
		Cbuf_InsertText ("startmap_dm\n");

	// check for special benchmark mode
// COMMANDLINEOPTION: Client: -benchmark <demoname> runs a timedemo and quits, results of any timedemo can be found in gamedir/benchmark.log (for example id1/benchmark.log)
	i = COM_CheckParm("-benchmark");
	if (i && i + 1 < com_argc && !sv.active)
		Cbuf_InsertText(va("timedemo %s\n", com_argv[i + 1]));

	if (!sv.active && !cls.demoplayback && !cls.connect_trying)
		Cbuf_InsertText("togglemenu\n");

	Cbuf_Execute();

	// We must wait for the log_file cvar to be initialized to start the log
	Log_Start ();
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static qboolean isdown = false;

	if (isdown)
	{
		Con_Print("recursive shutdown\n");
		return;
	}
	isdown = true;

	// disconnect client from server if active
	CL_Disconnect();

	// shut down local server if active
	Host_ShutdownServer (false);

	// Shutdown menu
	if(MR_Shutdown)
		MR_Shutdown();

	// AK shutdown PRVM
	// AK hmm, no PRVM_Shutdown(); yet

	CL_Video_Shutdown();

	Host_SaveConfig_f();

	CDAudio_Shutdown ();
	S_Terminate ();
	NetConn_Shutdown ();
	PR_Shutdown ();
	Cbuf_Shutdown ();

	if (cls.state != ca_dedicated)
	{
		R_Modules_Shutdown();
		VID_Shutdown();
	}

	Cmd_Shutdown();
	CL_Shutdown();
	Sys_Shutdown();
	Log_Close ();
	COM_Shutdown ();
	Memory_Shutdown();
}

