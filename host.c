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

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

quakeparms_t host_parms;

qboolean	host_initialized;		// true if into command execution
qboolean	hostloopactive = 0;		// LordHavoc: used to turn Host_Error into Sys_Error if Host_Frame has not yet run

double		host_frametime;
double		host_realframetime;		// LordHavoc: the real frametime, before slowmo and clamping are applied (used for console scrolling)
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

double		sv_frametime;

int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

cvar_t	host_framerate = {0, "host_framerate","0"};	// set for slow motion
cvar_t	host_speeds = {0, "host_speeds","0"};			// set for running times
cvar_t	slowmo = {0, "slowmo", "1.0"};					// LordHavoc: framerate independent slowmo
cvar_t	host_minfps = {CVAR_SAVE, "host_minfps", "10"};		// LordHavoc: game logic lower cap on framerate (if framerate is below this is, it pretends it is this, so game logic will run normally)
cvar_t	host_maxfps = {CVAR_SAVE, "host_maxfps", "1000"};		// LordHavoc: framerate upper cap

cvar_t	sys_ticrate = {CVAR_SAVE, "sys_ticrate","0.05"};
cvar_t	serverprofile = {0, "serverprofile","0"};

cvar_t	fraglimit = {CVAR_NOTIFY, "fraglimit","0"};
cvar_t	timelimit = {CVAR_NOTIFY, "timelimit","0"};
cvar_t	teamplay = {CVAR_NOTIFY, "teamplay","0"};

cvar_t	samelevel = {0, "samelevel","0"};
cvar_t	noexit = {CVAR_NOTIFY, "noexit","0"};

cvar_t	developer = {0, "developer","0"};

cvar_t	skill = {0, "skill","1"};						// 0 - 3
cvar_t	deathmatch = {0, "deathmatch","0"};			// 0, 1, or 2
cvar_t	coop = {0, "coop","0"};			// 0 or 1

cvar_t	pausable = {0, "pausable","1"};

cvar_t	temp1 = {0, "temp1","0"};

cvar_t	timestamps = {CVAR_SAVE, "timestamps", "0"};
cvar_t	timeformat = {CVAR_SAVE, "timeformat", "[%b %e %X] "};

/*
================
Host_EndGame
================
*/
void Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,message);
	vsprintf (string,message,argptr);
	va_end (argptr);
	Con_DPrintf ("Host_EndGame: %s\n",string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo ();
	else
		CL_Disconnect ();

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
char hosterrorstring[4096];
void Host_Error (char *error, ...)
{
	va_list		argptr;
	static	qboolean inerror = false;

	// LordHavoc: if host_frame loop has not been run yet, do a Sys_Error instead
	if (!hostloopactive)
	{
		char string[4096];
		va_start (argptr,error);
		vsprintf (string,error,argptr);
		va_end (argptr);
		Sys_Error ("%s", string);
	}

	if (inerror)
	{
		char string[4096];
		va_start (argptr,error);
		vsprintf (string,error,argptr);
		va_end (argptr);
		Sys_Error ("Host_Error: recursively entered (original error was: %s    new error is: %s)", hosterrorstring, string);
	}
	inerror = true;
	
//	SCR_EndLoadingPlaque ();		// reenable screen updates

	va_start (argptr,error);
	vsprintf (hosterrorstring,error,argptr);
	va_end (argptr);
	Con_Printf ("Host_Error: %s\n",hosterrorstring);
	
	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s\n",hosterrorstring);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;

	inerror = false;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
	int		i;

	svs.maxclients = 1;
		
	i = COM_CheckParm ("-dedicated");
	if (i)
	{
		cls.state = ca_dedicated;
		if (i != (com_argc - 1))
		{
			svs.maxclients = atoi (com_argv[i+1]);
		}
		else
			svs.maxclients = 8;
	}
	else
		cls.state = ca_disconnected;

	i = COM_CheckParm ("-listen");
	if (i)
	{
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = atoi (com_argv[i+1]);
		else
			svs.maxclients = 8;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < MAX_SCOREBOARD) // LordHavoc: upped listen mode limit from 4 to MAX_SCOREBOARD
		svs.maxclientslimit = MAX_SCOREBOARD;
	svs.clients = Hunk_AllocName (svs.maxclientslimit*sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetValue ("deathmatch", 1.0);
	else
		Cvar_SetValue ("deathmatch", 0.0);
}


/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
	Host_InitCommands ();
	
	Cvar_RegisterVariable (&host_framerate);
	Cvar_RegisterVariable (&host_speeds);
	Cvar_RegisterVariable (&slowmo);
	Cvar_RegisterVariable (&host_minfps);
	Cvar_RegisterVariable (&host_maxfps);

	Cvar_RegisterVariable (&sys_ticrate);
	Cvar_RegisterVariable (&serverprofile);

	Cvar_RegisterVariable (&fraglimit);
	Cvar_RegisterVariable (&timelimit);
	Cvar_RegisterVariable (&teamplay);
	Cvar_RegisterVariable (&samelevel);
	Cvar_RegisterVariable (&noexit);
	Cvar_RegisterVariable (&skill);
	Cvar_RegisterVariable (&developer);
	Cvar_RegisterVariable (&deathmatch);
	Cvar_RegisterVariable (&coop);

	Cvar_RegisterVariable (&pausable);

	Cvar_RegisterVariable (&temp1);

	Cvar_RegisterVariable (&timestamps);
	Cvar_RegisterVariable (&timeformat);

	Host_FindMaxClients ();
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (void)
{
	QFile	*f;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	if (host_initialized && cls.state != ca_dedicated)
	{
		f = Qopen (va("%s/config.cfg",com_gamedir), "w");
		if (!f)
		{
			Con_Printf ("Couldn't write config.cfg.\n");
			return;
		}
		
		Key_WriteBindings (f);
		Cvar_WriteVariables (f);

		Qclose (f);
	}
}


/*
=================
SV_ClientPrintf

Sends text across to be displayed 
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	int			i;
	
	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	for (i=0 ; i<svs.maxclients ; i++)
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, string);
		}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	va_start (argptr,fmt);
	vsprintf (string, fmt,argptr);
	va_end (argptr);
	
	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (qboolean crash)
{
	int		saveSelf;
	int		i;
	client_t *client;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}
	
		if (sv.active && host_client->edict && host_client->spawned) // LordHavoc: don't call QC if server is dead (avoids recursive Host_Error in some mods when they run out of edicts)
		{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect, "QC function ClientDisconnect is missing");
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf ("Client %s removed\n",host_client->name);
	}

// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

// send notification to all clients
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(qboolean crash)
{
	int		i;
	int		count;
	sizebuf_t	buf;
	char		message[4];
	double	start;

	if (!sv.active)
		return;

	sv.active = false;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

// flush any pending messages - like the score!!!
	start = Sys_DoubleTime();
	do
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime() - start) > 3.0)
			break;
	}
	while (count);

// make sure all the clients know we're disconnecting
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

//
// clear structures
//
	memset (&sv, 0, sizeof(sv));
	memset (svs.clients, 0, svs.maxclientslimit*sizeof(client_t));
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
	Con_DPrintf ("Clearing memory\n");
	Mod_ClearAll ();
	if (host_hunklevel)
		Hunk_FreeToLowMark (host_hunklevel);

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
qboolean Host_FilterTime (double time)
{
	double timecap;
	realtime += time;

	if (slowmo.value < 0.0f)
		Cvar_SetValue("slowmo", 0.0f);
	if (host_minfps.value < 10.0f)
		Cvar_SetValue("host_minfps", 10.0f);
	if (host_maxfps.value < host_minfps.value)
		Cvar_SetValue("host_maxfps", host_minfps.value);

	 // check if framerate is too high
	if (!cls.timedemo)
	{
		timecap = sys_ticrate.value;
		if (cls.state == ca_connected)
			timecap = 1.0 / host_maxfps.value;

		if ((realtime - oldrealtime) < timecap)
			return false;
	}

	// LordHavoc: copy into host_realframetime as well
	host_realframetime = host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if (cls.timedemo)
	{
		// disable time effects
		cl.frametime = host_frametime;
		return true;
	}

	if (host_framerate.value > 0)
		host_frametime = host_framerate.value;
	else
	{
		// don't allow really short frames
		if (host_frametime > (1.0 / host_minfps.value))
			host_frametime = (1.0 / host_minfps.value);
	}

	cl.frametime = host_frametime = bound(0, host_frametime * slowmo.value, 0.1f); // LordHavoc: the QC code relies on no less than 10fps
	
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
	char	*cmd;

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
	static double frametimetotal = 0, lastservertime = 0;
	frametimetotal += host_frametime;
	// LordHavoc: cap server at sys_ticrate in listen games
	if (cls.state != ca_dedicated && svs.maxclients > 1 && ((realtime - lastservertime) < sys_ticrate.value))
		return;
// run the world state
	sv.frametime = pr_global_struct->frametime = frametimetotal;
	frametimetotal = 0;
	lastservertime = realtime;
//	pr_global_struct->frametime = host_frametime;

// set the time and clear the general datagram
	SV_ClearDatagram ();
	
// check for new clients
	SV_CheckForNewClients ();

// read client messages
	SV_RunClients ();
	
// move things around and think
// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game) )
		SV_Physics ();

// send all messages to the clients
	SV_SendClientMessages ();
}


/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (float time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;

	if (setjmp (host_abortserver) )
		return;			// something bad happened, or the server disconnected
	hostloopactive = 1;

// keep the random time dependent
	rand ();

// decide the simulation time
	if (!Host_FilterTime (time))
	{
		// if time was rejected, don't totally hog the CPU
		Sys_Sleep();
		return;
	}

// get new key events
	Sys_SendKeyEvents ();

// allow mice or other external controllers to add commands
	IN_Commands ();

// process console commands
	Cbuf_Execute ();

	NET_Poll();

// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd ();

//-------------------
//
// server operations
//
//-------------------

// check for commands typed to the host
	Host_GetConsoleCommands ();

	if (sv.active)
		Host_ServerFrame ();

//-------------------
//
// client operations
//
//-------------------

// if running the server remotely, send intentions now after
// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

// fetch results from server
	if (cls.state == ca_connected)
		CL_ReadFromServer ();

	ui_update();

// update video
	if (host_speeds.value)
		time1 = Sys_DoubleTime ();

	SCR_UpdateScreen ();

	if (host_speeds.value)
		time2 = Sys_DoubleTime ();

// update audio
	if (cls.signon == SIGNONS)
	{
		S_Update (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	}
	else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update();

	if (host_speeds.value)
	{
		pass1 = (time1 - time3)*1000000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1)*1000000;
		pass3 = (time3 - time2)*1000000;
		Con_Printf ("%6ius total %6ius server %6ius gfx %6ius snd\n",
					pass1+pass2+pass3, pass1, pass2, pass3);
	}

	host_framecount++;
}

void Host_Frame (float time)
{
	double	time1, time2;
	static double	timetotal;
	static int		timecount;
	int		i, c, m;

	if (!serverprofile.value)
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

	Con_Printf ("serverprofile: %2i clients %2i msec\n",  c,  m);
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
	/*
	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else
		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm ("-minmemory"))
		host_parms.memsize = minimum_memory;

	if (host_parms.memsize < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory available, can't execute game", host_parms.memsize / (float)0x100000);
	*/

	host_parms.memsize = DEFAULTMEM * 1024 * 1024;

	i = COM_CheckParm("-mem");
	if (i)
		host_parms.memsize = (int) (atof(com_argv[i+1]) * 1024 * 1024);

	i = COM_CheckParm("-winmem");
	if (i)
		host_parms.memsize = (int) (atof(com_argv[i+1]) * 1024 * 1024);

	i = COM_CheckParm("-heapsize");
	if (i)
		host_parms.memsize = (int) (atof(com_argv[i+1]) * 1024);

	host_parms.membase = qmalloc(host_parms.memsize);
	if (!host_parms.membase)
		Sys_Error("Not enough memory free, close some programs and try again, or free disk space\n");

	com_argc = host_parms.argc;
	com_argv = host_parms.argv;

	Memory_Init (host_parms.membase, host_parms.memsize);
	Cbuf_Init ();
	Cmd_Init ();	
	V_Init ();
	COM_Init (host_parms.basedir);
	Host_InitLocal ();
	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	Con_Init ();	
	Chase_Init ();
	M_Init ();	
	PR_Init ();
	Mod_Init ();
	NET_Init ();
	SV_Init ();

	Con_Printf ("Exe: "__TIME__" "__DATE__"\n");
	Con_Printf ("%4.1f megabyte heap\n",host_parms.memsize/(1024*1024.0));
	
	if (cls.state != ca_dedicated)
	{
		VID_InitCvars();

		Gamma_Init();

		Palette_Init();

#ifndef _WIN32 // on non win32, mouse comes before video for security reasons
		IN_Init ();
#endif
		VID_Init ();

		Render_Init();
		S_Init ();
		CDAudio_Init ();
		Sbar_Init ();
		CL_Init ();
#ifdef _WIN32 // on non win32, mouse comes before video for security reasons
		IN_Init ();
#endif
	}

	Cbuf_InsertText ("exec quake.rc\n");

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;
	
	Sys_Printf ("========Quake Initialized=========\n");	
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
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration (); 

	CDAudio_Shutdown ();
	NET_Shutdown ();
	S_Shutdown();
	IN_Shutdown ();

	if (cls.state != ca_dedicated)
	{
		R_Modules_Shutdown();
		VID_Shutdown();
	}
}

