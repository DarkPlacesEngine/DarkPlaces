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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"

// Prototypes of the system dependent functions
extern void CDAudio_SysEject (void);
extern void CDAudio_SysCloseDoor (void);
extern int CDAudio_SysGetAudioDiskInfo (void);
extern int CDAudio_SysPlay (qbyte track);
extern int CDAudio_SysStop (void);
extern int CDAudio_SysPause (void);
extern int CDAudio_SysResume (void);
extern int CDAudio_SysUpdate (void);
extern void CDAudio_SysInit (void);
extern int CDAudio_SysStartup (void);
extern void CDAudio_SysShutdown (void);

// used by menu to ghost CD audio slider
cvar_t	cdaudioinitialized = {CVAR_READONLY,"cdaudioinitialized","0"};

static qboolean wasPlaying = false;
static qboolean initialized = false;
static qboolean enabled = false;
static float cdvolume;
static qbyte remap[100];
static qbyte maxTrack;
static int faketrack = -1;

// exported variables
qboolean cdValid = false;
qboolean cdPlaying = false;
qboolean cdPlayLooping = false;
qbyte cdPlayTrack;


static void CDAudio_Eject (void)
{
	if (!enabled)
		return;

	CDAudio_SysEject();
}


static void CDAudio_CloseDoor (void)
{
	if (!enabled)
		return;

	CDAudio_SysCloseDoor();
}

static int CDAudio_GetAudioDiskInfo (void)
{
	int ret;

	cdValid = false;

	ret = CDAudio_SysGetAudioDiskInfo();
	if (ret < 1)
		return -1;

	cdValid = true;
	maxTrack = ret;

	return 0;
}


void CDAudio_Play (qbyte track, qboolean looping)
{
	sfx_t* sfx;

	if (!enabled)
		return;

	track = remap[track];
	if (track < 1)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	if (cdPlaying && cdPlayTrack == track)
		return;
	CDAudio_Stop ();

	// Try playing a fake track (sound file) first
	sfx = S_PrecacheSound (va ("cdtracks/track%02u.wav", track), false);
	if (sfx != NULL)
	{
		faketrack = S_StartSound (-1, 0, sfx, vec3_origin, cdvolume, 0);
		if (faketrack != -1)
		{
			if (looping)
			S_LoopChannel (faketrack, true);
			Con_DPrintf ("Fake CD track %u playing...\n", track);
		}
	}

	// If we can't play a fake CD track, try the real one
	if (faketrack == -1)
	{
		if (!cdValid)
		{
			CDAudio_GetAudioDiskInfo();
			if (!cdValid)
			{
				Con_Print ("No CD in player.\n");
				return;
			}
		}

		if (track > maxTrack)
		{
			Con_DPrintf("CDAudio: Bad track number %u.\n", track);
			return;
		}

		if (CDAudio_SysPlay(track) == -1)
			return;
	}

	cdPlayLooping = looping;
	cdPlayTrack = track;
	cdPlaying = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
}


void CDAudio_Stop (void)
{
	if (!enabled || !cdPlaying)
		return;

	if (faketrack != -1)
	{
		S_StopChannel (faketrack);
		faketrack = -1;
	}
	else if (CDAudio_SysStop() == -1)
		return;

	wasPlaying = false;
	cdPlaying = false;
}

void CDAudio_Pause (void)
{
	if (!enabled || !cdPlaying)
		return;

	if (faketrack != -1)
		S_PauseChannel (faketrack, true);
	else if (CDAudio_SysPause() == -1)
		return;

	wasPlaying = cdPlaying;
	cdPlaying = false;
}


void CDAudio_Resume (void)
{
	if (!enabled || cdPlaying || !wasPlaying)
		return;

	if (faketrack != -1)
		S_PauseChannel (faketrack, false);
	else if (CDAudio_SysResume() == -1)
		return;
	cdPlaying = true;
}

static void CD_f (void)
{
	const char *command;
	int ret;
	int n;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	if (strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (strcasecmp(command, "off") == 0)
	{
		if (cdPlaying)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (cdPlaying)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = atoi(Cmd_Argv (n+1));
		return;
	}

	if (strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((qbyte)atoi(Cmd_Argv (2)), false);
		return;
	}

	if (strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((qbyte)atoi(Cmd_Argv (2)), true);
		return;
	}

	if (strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (strcasecmp(command, "eject") == 0)
	{
		if (cdPlaying && faketrack == -1)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (strcasecmp(command, "info") == 0)
	{
		CDAudio_GetAudioDiskInfo ();
		if (cdValid)
			Con_Printf("%u tracks on CD.\n", maxTrack);
		else
			Con_Print ("No CD in player.\n");
		if (cdPlaying)
			Con_Printf("Currently %s track %u\n", cdPlayLooping ? "looping" : "playing", cdPlayTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", cdPlayLooping ? "looping" : "playing", cdPlayTrack);
		Con_Printf("Volume is %f\n", cdvolume);
		return;
	}
}

void CDAudio_SetVolume (float newvol)
{
	// If the volume hasn't changed
	if (newvol == cdvolume)
		return;

	// If the CD has been muted
	if (newvol == 0.0f)
		CDAudio_Pause ();
	else
	{
		// If the CD has been unmuted
		if (cdvolume == 0.0f)
			CDAudio_Resume ();

		if (faketrack != -1)
			S_SetChannelVolume (faketrack, newvol);
		else
		{
			// TODO: add support for the "real CD" mixer
		}
	}

	cdvolume = newvol;
}

void CDAudio_Update (void)
{
	if (!enabled)
		return;

	CDAudio_SetVolume (bgmvolume.value);

	if (faketrack == -1)
		CDAudio_SysUpdate();
}

int CDAudio_Init (void)
{
	int i;

	if (cls.state == ca_dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio") || COM_CheckParm("-safe"))
		return -1;

	CDAudio_SysInit();

	for (i = 0; i < 100; i++)
		remap[i] = i;

	Cvar_RegisterVariable(&cdaudioinitialized);
	Cvar_SetValueQuick(&cdaudioinitialized, true);
	enabled = true;

	Cmd_AddCommand("cd", CD_f);

	return 0;
}

int CDAudio_Startup (void)
{
	CDAudio_SysStartup ();

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_DPrint("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	initialized = true;

	Con_DPrint("CD Audio Initialized\n");

	return 0;
}

void CDAudio_Shutdown (void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	CDAudio_SysShutdown();
	initialized = false;
}
