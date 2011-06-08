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

#include "quakedef.h"

#include <time.h>
#include "libcurl.h"
#include "cdaudio.h"
#include "cl_gecko.h"
#include "cl_video.h"
#include "progsvm.h"
#include "csprogs.h"
#include "sv_demo.h"
#include "snd_main.h"
#include "thread.h"

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

// how many frames have occurred
// (checked by Host_Error and Host_SaveConfig_f)
int host_framecount = 0;
// LordHavoc: set when quit is executed
qboolean host_shuttingdown = false;

// the real time since application started, without any slowmo or clamping
double realtime;

// current client
client_t *host_client;

jmp_buf host_abortframe;
double host_starttime = 0;

// pretend frames take this amount of time (in seconds), 0 = realtime
cvar_t host_framerate = {0, "host_framerate","0", "locks frame timing to this value in seconds, 0.05 is 20fps for example, note that this can easily run too fast, use cl_maxfps if you want to limit your framerate instead, or sys_ticrate to limit server speed"};
// shows time used by certain subsystems
cvar_t host_speeds = {0, "host_speeds","0", "reports how much time is used in server/graphics/sound"};
cvar_t host_maxwait = {0, "host_maxwait","1000", "maximum sleep time requested from the operating system in millisecond. Larger sleeps will be done using multiple host_maxwait length sleeps. Lowering this value will increase CPU load, but may help working around problems with accuracy of sleep times."};
cvar_t cl_minfps = {CVAR_SAVE, "cl_minfps", "40", "minimum fps target - while the rendering performance is below this, it will drift toward lower quality"};
cvar_t cl_minfps_fade = {CVAR_SAVE, "cl_minfps_fade", "0.2", "how fast the quality adapts to varying framerate"};
cvar_t cl_minfps_qualitymax = {CVAR_SAVE, "cl_minfps_qualitymax", "1", "highest allowed drawdistance multiplier"};
cvar_t cl_minfps_qualitymin = {CVAR_SAVE, "cl_minfps_qualitymin", "0.25", "lowest allowed drawdistance multiplier"};
cvar_t cl_minfps_qualitypower = {CVAR_SAVE, "cl_minfps_qualitypower", "4", "raises quality value to a power of itself, higher values make quality drop more sharply in relation to framerate"};
cvar_t cl_minfps_qualityscale = {CVAR_SAVE, "cl_minfps_qualityscale", "0.5", "multiplier for quality"};
cvar_t cl_maxfps = {CVAR_SAVE, "cl_maxfps", "0", "maximum fps cap, 0 = unlimited, if game is running faster than this it will wait before running another frame (useful to make cpu time available to other programs)"};
cvar_t cl_maxfps_alwayssleep = {0, "cl_maxfps_alwayssleep","1", "gives up some processing time to other applications each frame, value in milliseconds, disabled if cl_maxfps is 0"};
cvar_t cl_maxidlefps = {CVAR_SAVE, "cl_maxidlefps", "20", "maximum fps cap when the game is not the active window (makes cpu time available to other programs"};

cvar_t developer = {CVAR_SAVE, "developer","0", "shows debugging messages and information (recommended for all developers and level designers); the value -1 also suppresses buffering and logging these messages"};
cvar_t developer_extra = {0, "developer_extra", "0", "prints additional debugging messages, often very verbose!"};
cvar_t developer_insane = {0, "developer_insane", "0", "prints huge streams of information about internal workings, entire contents of files being read/written, etc.  Not recommended!"};
cvar_t developer_loadfile = {0, "developer_loadfile","0", "prints name and size of every file loaded via the FS_LoadFile function (which is almost everything)"};
cvar_t developer_loading = {0, "developer_loading","0", "prints information about files as they are loaded or unloaded successfully"};
cvar_t developer_entityparsing = {0, "developer_entityparsing", "0", "prints detailed network entities information each time a packet is received"};

cvar_t timestamps = {CVAR_SAVE, "timestamps", "0", "prints timestamps on console messages"};
cvar_t timeformat = {CVAR_SAVE, "timeformat", "[%Y-%m-%d %H:%M:%S] ", "time format to use on timestamped console messages"};

/*
================
Host_AbortCurrentFrame

aborts the current host frame and goes on with the next one
================
*/
void Host_AbortCurrentFrame(void)
{
	longjmp (host_abortframe, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (const char *error, ...)
{
	static char hosterrorstring1[MAX_INPUTLINE];
	static char hosterrorstring2[MAX_INPUTLINE];
	static qboolean hosterror = false;
	va_list argptr;

	// turn off rcon redirect if it was active when the crash occurred
	// to prevent loops when it is a networking problem
	Con_Rcon_Redirect_Abort();

	va_start (argptr,error);
	dpvsnprintf (hosterrorstring1,sizeof(hosterrorstring1),error,argptr);
	va_end (argptr);

	Con_Printf("Host_Error: %s\n", hosterrorstring1);

	// LordHavoc: if crashing very early, or currently shutting down, do
	// Sys_Error instead
	if (host_framecount < 3 || host_shuttingdown)
		Sys_Error ("Host_Error: %s", hosterrorstring1);

	if (hosterror)
		Sys_Error ("Host_Error: recursively entered (original error was: %s    new error is: %s)", hosterrorstring2, hosterrorstring1);
	hosterror = true;

	strlcpy(hosterrorstring2, hosterrorstring1, sizeof(hosterrorstring2));

	CL_Parse_DumpPacket();

	CL_Parse_ErrorCleanUp();

	//PR_Crash();

	// print out where the crash happened, if it was caused by QC (and do a cleanup)
	PRVM_Crash();


	Host_ShutdownServer ();

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s",hosterrorstring2);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;

	hosterror = false;

	Host_AbortCurrentFrame();
}

void Host_ServerOptions (void)
{
	int i;

	// general default
	svs.maxclients = 8;

// COMMANDLINEOPTION: Server: -dedicated [playerlimit] starts a dedicated server (with a command console), default playerlimit is 8
// COMMANDLINEOPTION: Server: -listen [playerlimit] starts a multiplayer server with graphical client, like singleplayer but other players can connect, default playerlimit is 8
	// if no client is in the executable or -dedicated is specified on
	// commandline, start a dedicated server
	i = COM_CheckParm ("-dedicated");
	if (i || !cl_available)
	{
		cls.state = ca_dedicated;
		// check for -dedicated specifying how many players
		if (i && i + 1 < com_argc && atoi (com_argv[i+1]) >= 1)
			svs.maxclients = atoi (com_argv[i+1]);
		if (COM_CheckParm ("-listen"))
			Con_Printf ("Only one of -dedicated or -listen can be specified\n");
		// default sv_public on for dedicated servers (often hosted by serious administrators), off for listen servers (often hosted by clueless users)
		Cvar_SetValue("sv_public", 1);
	}
	else if (cl_available)
	{
		// client exists and not dedicated, check if -listen is specified
		cls.state = ca_disconnected;
		i = COM_CheckParm ("-listen");
		if (i)
		{
			// default players unless specified
			if (i + 1 < com_argc && atoi (com_argv[i+1]) >= 1)
				svs.maxclients = atoi (com_argv[i+1]);
		}
		else
		{
			// default players in some games, singleplayer in most
			if (gamemode != GAME_GOODVSBAD2 && gamemode != GAME_NEXUIZ && gamemode != GAME_XONOTIC && gamemode != GAME_BATTLEMECH)
				svs.maxclients = 1;
		}
	}

	svs.maxclients = svs.maxclients_next = bound(1, svs.maxclients, MAX_SCOREBOARD);

	svs.clients = (client_t *)Mem_Alloc(sv_mempool, sizeof(client_t) * svs.maxclients);

	if (svs.maxclients > 1 && !deathmatch.integer && !coop.integer)
		Cvar_SetValueQuick(&deathmatch, 1);
}

/*
=======================
Host_InitLocal
======================
*/
void Host_SaveConfig_f(void);
void Host_LoadConfig_f(void);
extern cvar_t sv_writepicture_quality;
extern cvar_t r_texture_jpeg_fastpicmip;
static void Host_InitLocal (void)
{
	Cmd_AddCommand("saveconfig", Host_SaveConfig_f, "save settings to config.cfg (or a specified filename) immediately (also automatic when quitting)");
	Cmd_AddCommand("loadconfig", Host_LoadConfig_f, "reset everything and reload configs");

	Cvar_RegisterVariable (&host_framerate);
	Cvar_RegisterVariable (&host_speeds);
	Cvar_RegisterVariable (&host_maxwait);
	Cvar_RegisterVariable (&cl_minfps);
	Cvar_RegisterVariable (&cl_minfps_fade);
	Cvar_RegisterVariable (&cl_minfps_qualitymax);
	Cvar_RegisterVariable (&cl_minfps_qualitymin);
	Cvar_RegisterVariable (&cl_minfps_qualitypower);
	Cvar_RegisterVariable (&cl_minfps_qualityscale);
	Cvar_RegisterVariable (&cl_maxfps);
	Cvar_RegisterVariable (&cl_maxfps_alwayssleep);
	Cvar_RegisterVariable (&cl_maxidlefps);

	Cvar_RegisterVariable (&developer);
	Cvar_RegisterVariable (&developer_extra);
	Cvar_RegisterVariable (&developer_insane);
	Cvar_RegisterVariable (&developer_loadfile);
	Cvar_RegisterVariable (&developer_loading);
	Cvar_RegisterVariable (&developer_entityparsing);

	Cvar_RegisterVariable (&timestamps);
	Cvar_RegisterVariable (&timeformat);

	Cvar_RegisterVariable (&sv_writepicture_quality);
	Cvar_RegisterVariable (&r_texture_jpeg_fastpicmip);
}


/*
===============
Host_SaveConfig_f

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_SaveConfig_to(const char *file)
{
	qfile_t *f;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	// LordHavoc: don't save a config if it crashed in startup
	if (host_framecount >= 3 && cls.state != ca_dedicated && !COM_CheckParm("-benchmark") && !COM_CheckParm("-capturedemo"))
	{
		f = FS_OpenRealFile(file, "wb", false);
		if (!f)
		{
			Con_Printf("Couldn't write %s.\n", file);
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVariables (f);

		FS_Close (f);
	}
}
void Host_SaveConfig(void)
{
	Host_SaveConfig_to(CONFIGFILENAME);
}
void Host_SaveConfig_f(void)
{
	const char *file = CONFIGFILENAME;

	if(Cmd_Argc() >= 2) {
		file = Cmd_Argv(1);
		Con_Printf("Saving to %s\n", file);
	}

	Host_SaveConfig_to(file);
}

void Host_AddConfigText(void)
{
	// set up the default startmap_sp and startmap_dm aliases (mods can
	// override these) and then execute the quake.rc startup script
	if (gamemode == GAME_NEHAHRA)
		Cbuf_InsertText("alias startmap_sp \"map nehstart\"\nalias startmap_dm \"map nehstart\"\nexec " STARTCONFIGFILENAME "\n");
	else if (gamemode == GAME_TRANSFUSION)
		Cbuf_InsertText("alias startmap_sp \"map e1m1\"\n""alias startmap_dm \"map bb1\"\nexec " STARTCONFIGFILENAME "\n");
	else if (gamemode == GAME_TEU)
		Cbuf_InsertText("alias startmap_sp \"map start\"\nalias startmap_dm \"map start\"\nexec teu.rc\n");
	else
		Cbuf_InsertText("alias startmap_sp \"map start\"\nalias startmap_dm \"map start\"\nexec " STARTCONFIGFILENAME "\n");
}

/*
===============
Host_LoadConfig_f

Resets key bindings and cvars to defaults and then reloads scripts
===============
*/
void Host_LoadConfig_f(void)
{
	// reset all cvars, commands and aliases to init values
	Cmd_RestoreInitState();
	// prepend a menu restart command to execute after the config
	Cbuf_InsertText("\nmenu_restart\n");
	// reset cvars to their defaults, and then exec startup scripts again
	Host_AddConfigText();
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
	if (host_client->netconnection)
	{
		MSG_WriteByte(&host_client->netconnection->message, svc_print);
		MSG_WriteString(&host_client->netconnection->message, msg);
	}
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
	char msg[MAX_INPUTLINE];

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
		if (client->active && client->netconnection)
		{
			MSG_WriteByte(&client->netconnection->message, svc_print);
			MSG_WriteString(&client->netconnection->message, msg);
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
	char msg[MAX_INPUTLINE];

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
	char string[MAX_INPUTLINE];

	if (!host_client->netconnection)
		return;

	va_start(argptr,fmt);
	dpvsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->netconnection->message, svc_stufftext);
	MSG_WriteString(&host_client->netconnection->message, string);
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

	SV_StopDemoRecording(host_client);

	// make sure edict is not corrupt (from a level change for example)
	host_client->edict = PRVM_EDICT_NUM(host_client - svs.clients + 1);

	if (host_client->netconnection)
	{
		// tell the client to be gone
		if (!crash)
		{
			// LordHavoc: no opportunity for resending, so use unreliable 3 times
			unsigned char bufdata[8];
			sizebuf_t buf;
			memset(&buf, 0, sizeof(buf));
			buf.data = bufdata;
			buf.maxsize = sizeof(bufdata);
			MSG_WriteByte(&buf, svc_disconnect);
			NetConn_SendUnreliableMessage(host_client->netconnection, &buf, sv.protocol, 10000, false);
			NetConn_SendUnreliableMessage(host_client->netconnection, &buf, sv.protocol, 10000, false);
			NetConn_SendUnreliableMessage(host_client->netconnection, &buf, sv.protocol, 10000, false);
		}
	}

	// call qc ClientDisconnect function
	// LordHavoc: don't call QC if server is dead (avoids recursive
	// Host_Error in some mods when they run out of edicts)
	if (host_client->clientconnectcalled && sv.active && host_client->edict)
	{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
		int saveSelf = PRVM_serverglobaledict(self);
		host_client->clientconnectcalled = false;
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram(PRVM_serverfunction(ClientDisconnect), "QC function ClientDisconnect is missing");
		PRVM_serverglobaledict(self) = saveSelf;
	}

	if (host_client->netconnection)
	{
		// break the net connection
		NetConn_Close(host_client->netconnection);
		host_client->netconnection = NULL;
	}

	// if a download is active, close it
	if (host_client->download_file)
	{
		Con_DPrintf("Download of %s aborted when %s dropped\n", host_client->download_name, host_client->name);
		FS_Close(host_client->download_file);
		host_client->download_file = NULL;
		host_client->download_name[0] = 0;
		host_client->download_expectedposition = 0;
		host_client->download_started = false;
	}

	// remove leaving player from scoreboard
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
		PRVM_ED_ClearEdict(host_client->edict);
	}

	// clear the client struct (this sets active to false)
	memset(host_client, 0, sizeof(*host_client));

	// update server listing on the master because player count changed
	// (which the master uses for filtering empty/full servers)
	NetConn_Heartbeat(1);

	if (sv.loadgame)
	{
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

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(void)
{
	int i;

	Con_DPrintf("Host_ShutdownServer\n");

	if (!sv.active)
		return;

	NetConn_Heartbeat(2);
	NetConn_Heartbeat(2);

// make sure all the clients know we're disconnecting
	SV_VM_Begin();
	World_End(&sv.world);
	if(prog->loaded)
		if(PRVM_serverfunction(SV_Shutdown))
		{
			func_t s = PRVM_serverfunction(SV_Shutdown);
			PRVM_serverfunction(SV_Shutdown) = 0; // prevent it from getting called again
			PRVM_ExecuteProgram(s,"SV_Shutdown() required");
		}
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		if (host_client->active)
			SV_DropClient(false); // server shutdown
	SV_VM_End();

	NetConn_CloseServerPorts();

	sv.active = false;
//
// clear structures
//
	memset(&sv, 0, sizeof(sv));
	memset(svs.clients, 0, svs.maxclients*sizeof(client_t));
}


//============================================================================

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
Host_TimeReport

Returns a time report string, for example for
==================
*/
const char *Host_TimingReport(void)
{
	return va("%.1f%% CPU, %.2f%% lost, offset avg %.1fms, max %.1fms, sdev %.1fms", svs.perf_cpuload * 100, svs.perf_lost * 100, svs.perf_offset_avg * 1000, svs.perf_offset_max * 1000, svs.perf_offset_sdev * 1000);
}

/*
==================
Host_Frame

Runs all active servers
==================
*/
static void Host_Init(void);
void Host_Main(void)
{
	double time1 = 0;
	double time2 = 0;
	double time3 = 0;
	double cl_timer, sv_timer;
	double clframetime, deltarealtime, oldrealtime;
	double wait;
	int pass1, pass2, pass3, i;

	Host_Init();

	cl_timer = 0;
	sv_timer = 0;

	realtime = host_starttime = Sys_DoubleTime();
	for (;;)
	{
		if (setjmp(host_abortframe))
		{
			SCR_ClearLoadingScreen(false);
			continue;			// something bad happened, or the server disconnected
		}

		oldrealtime = realtime;
		realtime = Sys_DoubleTime();

		deltarealtime = realtime - oldrealtime;
		cl_timer += deltarealtime;
		sv_timer += deltarealtime;

		svs.perf_acc_realtime += deltarealtime;

		// Look for clients who have spawned
		for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
			if(host_client->spawned)
				if(host_client->netconnection)
					break;
		if(i == svs.maxclients)
		{
			// Nobody is looking? Then we won't do timing...
			// Instead, reset it to zero
			svs.perf_acc_realtime = svs.perf_acc_sleeptime = svs.perf_acc_lost = svs.perf_acc_offset = svs.perf_acc_offset_squared = svs.perf_acc_offset_max = svs.perf_acc_offset_samples = 0;
		}
		else if(svs.perf_acc_realtime > 5)
		{
			svs.perf_cpuload = 1 - svs.perf_acc_sleeptime / svs.perf_acc_realtime;
			svs.perf_lost = svs.perf_acc_lost / svs.perf_acc_realtime;
			if(svs.perf_acc_offset_samples > 0)
			{
				svs.perf_offset_max = svs.perf_acc_offset_max;
				svs.perf_offset_avg = svs.perf_acc_offset / svs.perf_acc_offset_samples;
				svs.perf_offset_sdev = sqrt(svs.perf_acc_offset_squared / svs.perf_acc_offset_samples - svs.perf_offset_avg * svs.perf_offset_avg);
			}
			if(svs.perf_lost > 0 && developer_extra.integer)
				Con_DPrintf("Server can't keep up: %s\n", Host_TimingReport());
			svs.perf_acc_realtime = svs.perf_acc_sleeptime = svs.perf_acc_lost = svs.perf_acc_offset = svs.perf_acc_offset_squared = svs.perf_acc_offset_max = svs.perf_acc_offset_samples = 0;
		}

		if (slowmo.value < 0.00001 && slowmo.value != 0)
			Cvar_SetValue("slowmo", 0);
		if (host_framerate.value < 0.00001 && host_framerate.value != 0)
			Cvar_SetValue("host_framerate", 0);

		// keep the random time dependent, but not when playing demos/benchmarking
		if(!*sv_random_seed.string && !cls.demoplayback)
			rand();

		cl.islocalgame = NetConn_IsLocalGame();

		// get new key events
		Key_EventQueue_Unblock();
		SndSys_SendKeyEvents();
		Sys_SendKeyEvents();

		NetConn_UpdateSockets();

		Log_DestBuffer_Flush();

		// receive packets on each main loop iteration, as the main loop may
		// be undersleeping due to select() detecting a new packet
		if (sv.active)
			NetConn_ServerFrame();

		Curl_Run();

		// check for commands typed to the host
		Host_GetConsoleCommands();

		// when a server is running we only execute console commands on server frames
		// (this mainly allows frikbot .way config files to work properly by staying in sync with the server qc)
		// otherwise we execute them on client frames
		if (sv.active ? sv_timer > 0 : cl_timer > 0)
		{
			// process console commands
//			R_TimeReport("preconsole");
			CL_VM_PreventInformationLeaks();
			Cbuf_Execute();
//			R_TimeReport("console");
		}

		//Con_Printf("%6.0f %6.0f\n", cl_timer * 1000000.0, sv_timer * 1000000.0);

		// if the accumulators haven't become positive yet, wait a while
		if (cls.state == ca_dedicated)
			wait = sv_timer * -1000000.0;
		else if (!sv.active)
			wait = cl_timer * -1000000.0;
		else
			wait = max(cl_timer, sv_timer) * -1000000.0;

		if (!cls.timedemo && wait >= 1)
		{
			double time0;

			if(host_maxwait.value <= 0)
				wait = min(wait, 1000000.0);
			else
				wait = min(wait, host_maxwait.value * 1000.0);
			if(wait < 1)
				wait = 1; // because we cast to int

			time0 = Sys_DoubleTime();
			if (sv_checkforpacketsduringsleep.integer && !sys_usenoclockbutbenchmark.integer)
				NetConn_SleepMicroseconds((int)wait);
			else
				Sys_Sleep((int)wait);
			svs.perf_acc_sleeptime += Sys_DoubleTime() - time0;
//			R_TimeReport("sleep");
			continue;
		}

		R_TimeReport("---");

	//-------------------
	//
	// server operations
	//
	//-------------------

		// limit the frametime steps to no more than 100ms each
		if (cl_timer > 0.1)
			cl_timer = 0.1;
		if (sv_timer > 0.1)
		{
			svs.perf_acc_lost += (sv_timer - 0.1);
			sv_timer = 0.1;
		}

		if (sv.active && sv_timer > 0)
		{
			// execute one or more server frames, with an upper limit on how much
			// execution time to spend on server frames to avoid freezing the game if
			// the server is overloaded, this execution time limit means the game will
			// slow down if the server is taking too long.
			int framecount, framelimit = 1;
			double advancetime, aborttime = 0;
			float offset;

			if (cls.state == ca_dedicated)
				Collision_Cache_NewFrame();

			// run the world state
			// don't allow simulation to run too fast or too slow or logic glitches can occur

			// stop running server frames if the wall time reaches this value
			if (sys_ticrate.value <= 0)
				advancetime = sv_timer;
			else if (cl.islocalgame && !sv_fixedframeratesingleplayer.integer)
			{
				// synchronize to the client frametime, but no less than 10ms and no more than sys_ticrate
				advancetime = bound(0.01, cl_timer, sys_ticrate.value);
				framelimit = 10;
				aborttime = realtime + 0.1;
			}
			else
			{
				advancetime = sys_ticrate.value;
				// listen servers can run multiple server frames per client frame
				if (cls.state == ca_connected)
				{
					framelimit = 10;
					aborttime = realtime + 0.1;
				}
			}
			if(slowmo.value > 0 && slowmo.value < 1)
				advancetime = min(advancetime, 0.1 / slowmo.value);
			else
				advancetime = min(advancetime, 0.1);

			if(advancetime > 0)
			{
				offset = sv_timer + (Sys_DoubleTime() - realtime);
				++svs.perf_acc_offset_samples;
				svs.perf_acc_offset += offset;
				svs.perf_acc_offset_squared += offset * offset;
				if(svs.perf_acc_offset_max < offset)
					svs.perf_acc_offset_max = offset;
			}

			// only advance time if not paused
			// the game also pauses in singleplayer when menu or console is used
			sv.frametime = advancetime * slowmo.value;
			if (host_framerate.value)
				sv.frametime = host_framerate.value;
			if (sv.paused || (cl.islocalgame && (key_dest != key_game || key_consoleactive || cl.csqc_paused)))
				sv.frametime = 0;

			// setup the VM frame
			SV_VM_Begin();

			for (framecount = 0;framecount < framelimit && sv_timer > 0;framecount++)
			{
				sv_timer -= advancetime;

				// move things around and think unless paused
				if (sv.frametime)
					SV_Physics();

				// if this server frame took too long, break out of the loop
				if (framelimit > 1 && Sys_DoubleTime() >= aborttime)
					break;
			}
			R_TimeReport("serverphysics");

			// send all messages to the clients
			SV_SendClientMessages();
			
			if (sv.paused == 1 && realtime > sv.pausedstart && sv.pausedstart > 0) {
				prog->globals.generic[OFS_PARM0] = realtime - sv.pausedstart;
				PRVM_ExecuteProgram(PRVM_serverfunction(SV_PausedTic), "QC function SV_PausedTic is missing");
			}

			// end the server VM frame
			SV_VM_End();

			// send an heartbeat if enough time has passed since the last one
			NetConn_Heartbeat(0);
			R_TimeReport("servernetwork");
		}
		else
		{
			// don't let r_speeds display jump around
			R_TimeReport("serverphysics");
			R_TimeReport("servernetwork");
		}

	//-------------------
	//
	// client operations
	//
	//-------------------

		if (cls.state != ca_dedicated && (cl_timer > 0 || cls.timedemo || ((vid_activewindow ? cl_maxfps : cl_maxidlefps).value < 1)))
		{
			R_TimeReport("---");
			Collision_Cache_NewFrame();
			R_TimeReport("collisioncache");
			// decide the simulation time
			if (cls.capturevideo.active)
			{
				//***
				if (cls.capturevideo.realtime)
					clframetime = cl.realframetime = max(cl_timer, 1.0 / cls.capturevideo.framerate);
				else
				{
					clframetime = 1.0 / cls.capturevideo.framerate;
					cl.realframetime = max(cl_timer, clframetime);
				}
			}
			else if (vid_activewindow && cl_maxfps.value >= 1 && !cls.timedemo)
			{
				clframetime = cl.realframetime = max(cl_timer, 1.0 / cl_maxfps.value);
				// when running slow, we need to sleep to keep input responsive
				wait = bound(0, cl_maxfps_alwayssleep.value * 1000, 100000);
				if (wait > 0)
					Sys_Sleep((int)wait);
			}
			else if (!vid_activewindow && cl_maxidlefps.value >= 1 && !cls.timedemo)
				clframetime = cl.realframetime = max(cl_timer, 1.0 / cl_maxidlefps.value);
			else
				clframetime = cl.realframetime = cl_timer;

			// apply slowmo scaling
			clframetime *= cl.movevars_timescale;
			// scale playback speed of demos by slowmo cvar
			if (cls.demoplayback)
			{
				clframetime *= slowmo.value;
				// if demo playback is paused, don't advance time at all
				if (cls.demopaused)
					clframetime = 0;
			}

			// host_framerate overrides all else
			if (host_framerate.value)
				clframetime = host_framerate.value;

			if (cl.paused || (cl.islocalgame && (key_dest != key_game || key_consoleactive || cl.csqc_paused)))
				clframetime = 0;

			if (cls.timedemo)
				clframetime = cl.realframetime = cl_timer;

			// deduct the frame time from the accumulator
			cl_timer -= cl.realframetime;

			cl.oldtime = cl.time;
			cl.time += clframetime;

			// update video
			if (host_speeds.integer)
				time1 = Sys_DoubleTime();
			R_TimeReport("pre-input");

			// Collect input into cmd
			CL_Input();

			R_TimeReport("input");

			// check for new packets
			NetConn_ClientFrame();

			// read a new frame from a demo if needed
			CL_ReadDemoMessage();
			R_TimeReport("clientnetwork");

			// now that packets have been read, send input to server
			CL_SendMove();
			R_TimeReport("sendmove");

			// update client world (interpolate entities, create trails, etc)
			CL_UpdateWorld();
			R_TimeReport("lerpworld");

			CL_Video_Frame();
			CL_Gecko_Frame();

			R_TimeReport("client");

			CL_UpdateScreen();
			R_TimeReport("render");

			if (host_speeds.integer)
				time2 = Sys_DoubleTime();

			// update audio
			if(cl.csqc_usecsqclistener)
			{
				S_Update(&cl.csqc_listenermatrix);
				cl.csqc_usecsqclistener = false;
			}
			else
				S_Update(&r_refdef.view.matrix);

			CDAudio_Update();
			R_TimeReport("audio");

			// reset gathering of mouse input
			in_mouse_x = in_mouse_y = 0;

			if (host_speeds.integer)
			{
				pass1 = (int)((time1 - time3)*1000000);
				time3 = Sys_DoubleTime();
				pass2 = (int)((time2 - time1)*1000000);
				pass3 = (int)((time3 - time2)*1000000);
				Con_Printf("%6ius total %6ius server %6ius gfx %6ius snd\n",
							pass1+pass2+pass3, pass1, pass2, pass3);
			}
		}

#if MEMPARANOIA
		Mem_CheckSentinelsGlobal();
#else
		if (developer_memorydebug.integer)
			Mem_CheckSentinelsGlobal();
#endif

		// if there is some time remaining from this frame, reset the timers
		if (cl_timer >= 0)
			cl_timer = 0;
		if (sv_timer >= 0)
		{
			svs.perf_acc_lost += sv_timer;
			sv_timer = 0;
		}

		host_framecount++;
	}
}

//============================================================================

qboolean vid_opened = false;
void Host_StartVideo(void)
{
	if (!vid_opened && cls.state != ca_dedicated)
	{
		vid_opened = true;
		// make sure we open sockets before opening video because the Windows Firewall "unblock?" dialog can screw up the graphics context on some graphics drivers
		NetConn_UpdateSockets();
		VID_Start();
		CDAudio_Startup();
	}
}

char engineversion[128];

qboolean sys_nostdout = false;

extern void u8_Init(void);
extern void Render_Init(void);
extern void Mathlib_Init(void);
extern void FS_Init_SelfPack(void);
extern void FS_Init(void);
extern void FS_Shutdown(void);
extern void PR_Cmd_Init(void);
extern void COM_Init_Commands(void);
extern void FS_Init_Commands(void);
extern qboolean host_stuffcmdsrun;

/*
====================
Host_Init
====================
*/
static void Host_Init (void)
{
	int i;
	const char* os;

	if (COM_CheckParm("-profilegameonly"))
		Sys_AllowProfiling(false);

	// LordHavoc: quake never seeded the random number generator before... heh
	if (COM_CheckParm("-benchmark"))
		srand(0); // predictable random sequence for -benchmark
	else
		srand((unsigned int)time(NULL));

	// FIXME: this is evil, but possibly temporary
	// LordHavoc: doesn't seem very temporary...
	// LordHavoc: made this a saved cvar
// COMMANDLINEOPTION: Console: -developer enables warnings and other notices (RECOMMENDED for mod developers)
	if (COM_CheckParm("-developer"))
	{
		developer.value = developer.integer = 1;
		developer.string = "1";
	}

	if (COM_CheckParm("-developer2"))
	{
		developer.value = developer.integer = 1;
		developer.string = "1";
		developer_extra.value = developer_extra.integer = 1;
		developer_extra.string = "1";
		developer_insane.value = developer_insane.integer = 1;
		developer_insane.string = "1";
		developer_memory.value = developer_memory.integer = 1;
		developer_memory.string = "1";
		developer_memorydebug.value = developer_memorydebug.integer = 1;
		developer_memorydebug.string = "1";
	}

// COMMANDLINEOPTION: Console: -nostdout disables text output to the terminal the game was launched from
	if (COM_CheckParm("-nostdout"))
		sys_nostdout = 1;

	// used by everything
	Memory_Init();

	// initialize console command/cvar/alias/command execution systems
	Cmd_Init();

	// initialize memory subsystem cvars/commands
	Memory_Init_Commands();

	// initialize console and logging and its cvars/commands
	Con_Init();

	// initialize various cvars that could not be initialized earlier
	u8_Init();
	Curl_Init_Commands();
	Cmd_Init_Commands();
	Sys_Init_Commands();
	COM_Init_Commands();
	FS_Init_Commands();

	// initialize console window (only used by sys_win.c)
	Sys_InitConsole();

	// initialize the self-pack (must be before COM_InitGameType as it may add command line options)
	FS_Init_SelfPack();

	// detect gamemode from commandline options or executable name
	COM_InitGameType();

	// construct a version string for the corner of the console
	os = DP_OS_NAME;
	dpsnprintf (engineversion, sizeof (engineversion), "%s %s %s", gamename, os, buildstring);
	Con_Printf("%s\n", engineversion);

	// initialize ixtable
	Mathlib_Init();

	// initialize filesystem (including fs_basedir, fs_gamedir, -game, scr_screenshot_name)
	FS_Init();

	// must be after FS_Init
	Crypto_Init();
	Crypto_Init_Commands();

	NetConn_Init();
	Curl_Init();
	//PR_Init();
	//PR_Cmd_Init();
	PRVM_Init();
	Mod_Init();
	World_Init();
	SV_Init();
	V_Init(); // some cvars needed by server player physics (cl_rollangle etc)
	Host_InitCommands();
	Host_InitLocal();
	Host_ServerOptions();

	if (cls.state == ca_dedicated)
		Cmd_AddCommand ("disconnect", CL_Disconnect_f, "disconnect from server (or disconnect all clients if running a server)");
	else
	{
		Con_DPrintf("Initializing client\n");

		R_Modules_Init();
		Palette_Init();
		MR_Init_Commands();
		Thread_Init();
		VID_Shared_Init();
		VID_Init();
		Render_Init();
		S_Init();
		CDAudio_Init();
		Key_Init();
		CL_Init();
	}

	// save off current state of aliases, commands and cvars for later restore if FS_GameDir_f is called
	// NOTE: menu commands are freed by Cmd_RestoreInitState
	Cmd_SaveInitState();

	// FIXME: put this into some neat design, but the menu should be allowed to crash
	// without crashing the whole game, so this should just be a short-time solution

	// here comes the not so critical stuff
	if (setjmp(host_abortframe)) {
		return;
	}

	Host_AddConfigText();
	Cbuf_Execute();

	// if stuffcmds wasn't run, then quake.rc is probably missing, use default
	if (!host_stuffcmdsrun)
	{
		Cbuf_AddText("exec default.cfg\nexec " CONFIGFILENAME "\nexec autoexec.cfg\nstuffcmds\n");
		Cbuf_Execute();
	}

	// put up the loading image so the user doesn't stare at a black screen...
	SCR_BeginLoadingPlaque();

	if (cls.state != ca_dedicated)
	{
		MR_Init();
	}

	// check for special benchmark mode
// COMMANDLINEOPTION: Client: -benchmark <demoname> runs a timedemo and quits, results of any timedemo can be found in gamedir/benchmark.log (for example id1/benchmark.log)
	i = COM_CheckParm("-benchmark");
	if (i && i + 1 < com_argc)
	if (!sv.active && !cls.demoplayback && !cls.connect_trying)
	{
		Cbuf_AddText(va("timedemo %s\n", com_argv[i + 1]));
		Cbuf_Execute();
	}

	// check for special demo mode
// COMMANDLINEOPTION: Client: -demo <demoname> runs a playdemo and quits
	i = COM_CheckParm("-demo");
	if (i && i + 1 < com_argc)
	if (!sv.active && !cls.demoplayback && !cls.connect_trying)
	{
		Cbuf_AddText(va("playdemo %s\n", com_argv[i + 1]));
		Cbuf_Execute();
	}

// COMMANDLINEOPTION: Client: -capturedemo <demoname> captures a playdemo and quits
	i = COM_CheckParm("-capturedemo");
	if (i && i + 1 < com_argc)
	if (!sv.active && !cls.demoplayback && !cls.connect_trying)
	{
		Cbuf_AddText(va("playdemo %s\ncl_capturevideo 1\n", com_argv[i + 1]));
		Cbuf_Execute();
	}

	if (cls.state == ca_dedicated || COM_CheckParm("-listen"))
	if (!sv.active && !cls.demoplayback && !cls.connect_trying)
	{
		Cbuf_AddText("startmap_dm\n");
		Cbuf_Execute();
	}

	if (!sv.active && !cls.demoplayback && !cls.connect_trying)
	{
		Cbuf_AddText("togglemenu\n");
		Cbuf_Execute();
	}

	Con_DPrint("========Initialized=========\n");

	//Host_StartVideo();
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
	if (setjmp(host_abortframe))
	{
		Con_Print("aborted the quitting frame?!?\n");
		return;
	}
	isdown = true;

	// be quiet while shutting down
	S_StopAllSounds();

	// disconnect client from server if active
	CL_Disconnect();

	// shut down local server if active
	Host_ShutdownServer ();

	// Shutdown menu
	if(MR_Shutdown)
		MR_Shutdown();

	// AK shutdown PRVM
	// AK hmm, no PRVM_Shutdown(); yet

	CL_Gecko_Shutdown();
	CL_Video_Shutdown();

	Host_SaveConfig();

	CDAudio_Shutdown ();
	S_Terminate ();
	Curl_Shutdown ();
	NetConn_Shutdown ();
	//PR_Shutdown ();

	if (cls.state != ca_dedicated)
	{
		R_Modules_Shutdown();
		VID_Shutdown();
		Thread_Shutdown();
	}

	Cmd_Shutdown();
	Key_Shutdown();
	CL_Shutdown();
	Sys_Shutdown();
	Log_Close();
	Crypto_Shutdown();
	FS_Shutdown();
	Con_Shutdown();
	Memory_Shutdown();
}

