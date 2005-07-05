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

int old_vsync = 0;

void CL_FinishTimeDemo (void);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all outgoing network messages are skipped, and
incoming messages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char	str[1024];

	if (cls.demonum == -1)
		return;		// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			Con_Print("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	sprintf (str,"playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
// LordHavoc: now called only by CL_Disconnect
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	FS_Close (cls.demofile);
	cls.demoplayback = false;
	cls.demofile = NULL;

	if (cls.timedemo)
		CL_FinishTimeDemo ();

	if (COM_CheckParm("-demo") || COM_CheckParm("-demolooponly"))
		Host_Quit_f();

}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (void)
{
	int		len;
	int		i;
	float	f;

	if (cls.demopaused) // LordHavoc: pausedemo
		return;

	len = LittleLong (net_message.cursize);
	FS_Write (cls.demofile, &len, 4);
	for (i=0 ; i<3 ; i++)
	{
		f = LittleFloat (cl.viewangles[i]);
		FS_Write (cls.demofile, &f, 4);
	}
	FS_Write (cls.demofile, net_message.data, net_message.cursize);
}

/*
====================
CL_ReadDemoMessage

Handles playback of demos
====================
*/
void CL_ReadDemoMessage(void)
{
	int r, i;
	float f;

	if (!cls.demoplayback)
		return;

	// LordHavoc: pausedemo
	if (cls.demopaused)
		return;

	while (1)
	{
		// decide if it is time to grab the next message
		// always grab until fully connected
		if (cls.signon == SIGNONS)
		{
			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
				{
					// already read this frame's message
					return;
				}
				if (cls.td_lastframe == -1)
				{
					// we start counting on the second frame
					// (after parsing connection stuff)
					cls.td_startframe = host_framecount + 1;
				}
				cls.td_lastframe = host_framecount;
				// if this is the first official frame we can now grab the real
				// td_starttime so the bogus time on the first frame doesn't
				// count against the final report
				if (host_framecount == cls.td_startframe)
					cls.td_starttime = realtime;
				if (host_framecount > cls.td_startframe + 2)
				{
					cls.td_minframetime = min(cls.td_minframetime, host_realframetime);
					cls.td_maxframetime = max(cls.td_maxframetime, host_realframetime);
				}
				else
					cls.td_minframetime = cls.td_maxframetime = host_realframetime;
			}
			else if (cl.time <= cl.mtime[0])
			{
				// don't need another message yet
				return;
			}
		}

		// get the next message
		FS_Read(cls.demofile, &net_message.cursize, 4);
		net_message.cursize = LittleLong(net_message.cursize);
		if (net_message.cursize > net_message.maxsize)
			Host_Error("Demo message (%i) > net_message.maxsize (%i)", net_message.cursize, net_message.maxsize);
		VectorCopy(cl.mviewangles[0], cl.mviewangles[1]);
		for (i = 0;i < 3;i++)
		{
			r = (int)FS_Read(cls.demofile, &f, 4);
			cl.mviewangles[0][i] = LittleFloat(f);
		}

		if (FS_Read(cls.demofile, net_message.data, net_message.cursize) == (size_t)net_message.cursize)
		{
			MSG_BeginReading();
			CL_ParseServerMessage();

			// In case the demo contains a "svc_disconnect" message
			if (!cls.demoplayback)
				return;
		}
		else
		{
			CL_Disconnect();
			return;
		}
	}
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Print("Not recording a demo.\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_disconnect);
	CL_WriteDemoMessage ();

// finish up
	FS_Close (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Print("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
	int c, track;
	char name[MAX_OSPATH];

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc();
	if (c != 2 && c != 3 && c != 4)
	{
		Con_Print("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Print("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected)
	{
		Con_Print("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
		return;
	}

	// write the forced cd track number, or -1
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
		track = -1;

	// get the demo name
	strlcpy (name, Cmd_Argv(1), sizeof (name));
	FS_DefaultExtension (name, ".dem", sizeof (name));

	// start the map up
	if (c > 2)
		Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);

	// open the demo file
	Con_Printf("recording to %s.\n", name);
	cls.demofile = FS_Open (name, "wb", false, false);
	if (!cls.demofile)
	{
		Con_Print("ERROR: couldn't open.\n");
		return;
	}

	cls.forcetrack = track;
	FS_Printf(cls.demofile, "%i\n", cls.forcetrack);

	cls.demorecording = true;
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
	char	name[256];
	int c;
	qboolean neg = false;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Print("play <demoname> : plays a demo\n");
		return;
	}

	// disconnect from server
	CL_Disconnect ();
	Host_ShutdownServer (false);

	// update networking ports (this is mainly just needed at startup)
	NetConn_ClientFrame();

	// open the demo file
	strlcpy (name, Cmd_Argv(1), sizeof (name));
	FS_DefaultExtension (name, ".dem", sizeof (name));

	Con_Printf("Playing demo from %s.\n", name);
	cls.demofile = FS_Open (name, "rb", false, false);
	if (!cls.demofile)
	{
		Con_Print("ERROR: couldn't open.\n");
		cls.demonum = -1;		// stop demo loop
		return;
	}

	strlcpy(cls.demoname, name, sizeof(cls.demoname));
	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = FS_Getc (cls.demofile)) != '\n')
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');

	if (neg)
		cls.forcetrack = -cls.forcetrack;
}

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo (void)
{
	int frames;
	double time; // LordHavoc: changed timedemo accuracy to double
	double fpsmin, fpsavg, fpsmax; // report min/avg/max fps

	cls.timedemo = false;

// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	fpsmin = cls.td_maxframetime > 0 ? 1.0 / cls.td_maxframetime : 0;
	fpsavg = time > 0 ? frames / time : 0;
	fpsmax = cls.td_minframetime > 0 ? 1.0 / cls.td_minframetime : 0;
	// LordHavoc: timedemo now prints out 7 digits of fraction, and min/avg/max
	Con_Printf("%i frames %5.7f seconds %5.7f fps\nmin/avg/max: %5.7f/%5.7f/%5.7f\n", frames, time, fpsavg, fpsmin, fpsavg, fpsmax);
	Log_Printf("benchmark.log", "date %s | enginedate %s | demo %s | commandline %s | result %i frames %5.7f seconds %5.7f fps min/avg/max: %5.7f/%5.7f/%5.7f\n", Sys_TimeString("%Y-%m-%d %H:%M:%S"), buildstring, cls.demoname, cmdline.string, frames, time, fpsavg, fpsmin, fpsavg, fpsmax);
	if (COM_CheckParm("-benchmark"))
		Host_Quit_f();
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Print("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted

	// instantly hide console and deactivate it
	key_dest = key_game;
	key_consoleactive = 0;
	scr_con_current = 0;

	cls.timedemo = true;
	// get first message this frame
	cls.td_lastframe = -1;
}

