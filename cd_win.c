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
#include <windows.h>
#include <mmsystem.h>

#include "cdaudio.h"

#if defined(_MSC_VER) && (_MSC_VER < 1300)
typedef DWORD DWORD_PTR;
#endif

extern	HWND	mainwindow;

UINT	wDeviceID;

void CDAudio_SysEject(void)
{
	DWORD	dwReturn;

	if ((dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD_PTR)NULL)))
		Con_Printf("MCI_SET_DOOR_OPEN failed (%x)\n", (unsigned)dwReturn);
}


void CDAudio_SysCloseDoor(void)
{
	DWORD	dwReturn;

	if ((dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD_PTR)NULL)))
		Con_Printf("MCI_SET_DOOR_CLOSED failed (%x)\n", (unsigned)dwReturn);
}

int CDAudio_SysGetAudioDiskInfo(void)
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;

	mciStatusParms.dwItem = MCI_STATUS_READY;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_Print("CDAudio_SysGetAudioDiskInfo: drive ready test - get status failed\n");
		return -1;
	}
	if (!mciStatusParms.dwReturn)
	{
		Con_Print("CDAudio_SysGetAudioDiskInfo: drive not ready\n");
		return -1;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_Print("CDAudio_SysGetAudioDiskInfo: get tracks - status failed\n");
		return -1;
	}
	if (mciStatusParms.dwReturn < 1)
	{
		Con_Print("CDAudio_SysGetAudioDiskInfo: no music tracks\n");
		return -1;
	}

	return mciStatusParms.dwReturn;
}


float CDAudio_SysGetVolume (void)
{
	// IMPLEMENTME
	return -1.0f;
}


void CDAudio_SysSetVolume (float volume)
{
	// IMPLEMENTME
}


int CDAudio_SysPlay (int track)
{
	DWORD				dwReturn;
	MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_Printf("CDAudio_SysPlay: MCI_STATUS failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}
	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		Con_Printf("CDAudio_SysPlay: track %i is not audio\n", track);
		return -1;
	}

	if (cdPlaying)
		CDAudio_Stop();

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_Printf("CDAudio_SysPlay: MCI_STATUS failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
	mciPlayParms.dwTo = (mciStatusParms.dwReturn << 8) | track;
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD_PTR)(LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_Printf("CDAudio_SysPlay: MCI_PLAY failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}

	return 0;
}


int CDAudio_SysStop (void)
{
	DWORD	dwReturn;

	if ((dwReturn = mciSendCommand(wDeviceID, MCI_STOP, 0, (DWORD_PTR)NULL)))
	{
		Con_Printf("MCI_STOP failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}
	return 0;
}

int CDAudio_SysPause (void)
{
	DWORD				dwReturn;
	MCI_GENERIC_PARMS	mciGenericParms;

	mciGenericParms.dwCallback = (DWORD_PTR)mainwindow;
	if ((dwReturn = mciSendCommand(wDeviceID, MCI_PAUSE, 0, (DWORD_PTR)(LPVOID) &mciGenericParms)))
	{
		Con_Printf("MCI_PAUSE failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}
	return 0;
}


int CDAudio_SysResume (void)
{
	DWORD			dwReturn;
	MCI_PLAY_PARMS	mciPlayParms;

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(cdPlayTrack, 0, 0, 0);
	mciPlayParms.dwTo = MCI_MAKE_TMSF(cdPlayTrack + 1, 0, 0, 0);
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR)(LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_Printf("CDAudio_SysResume: MCI_PLAY failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}
	return 0;
}

LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != (LPARAM)wDeviceID)
		return 1;

	switch (wParam)
	{
		case MCI_NOTIFY_SUCCESSFUL:
			if (cdPlaying)
			{
				cdPlaying = false;
				if (cdPlayLooping)
					CDAudio_Play(cdPlayTrack, true);
			}
			break;

		case MCI_NOTIFY_ABORTED:
		case MCI_NOTIFY_SUPERSEDED:
			break;

		case MCI_NOTIFY_FAILURE:
			Con_Print("MCI_NOTIFY_FAILURE\n");
			CDAudio_Stop ();
			cdValid = false;
			break;

		default:
			Con_Printf("Unexpected MM_MCINOTIFY type (%i)\n", (int)wParam);
			return 1;
	}

	return 0;
}


int CDAudio_SysUpdate (void)
{
	return 0;
}

void CDAudio_SysInit (void)
{
}

int CDAudio_SysStartup (void)
{
	DWORD	dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
	MCI_SET_PARMS	mciSetParms;

	mciOpenParms.lpstrDeviceType = "cdaudio";
	if ((dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD_PTR) (LPVOID) &mciOpenParms)))
	{
		Con_Printf("CDAudio_SysStartup: MCI_OPEN failed (%x)\n", (unsigned)dwReturn);
		return -1;
	}
	wDeviceID = mciOpenParms.wDeviceID;

	// Set the time format to track/minute/second/frame (TMSF).
	mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	if ((dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)(LPVOID) &mciSetParms)))
	{
		Con_Printf("CDAudio_SysStartup: MCI_SET_TIME_FORMAT failed (%x)\n", (unsigned)dwReturn);
		mciSendCommand(wDeviceID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
		return -1;
	}

	return 0;
}

void CDAudio_SysShutdown (void)
{
	if (mciSendCommand(wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)NULL))
		Con_Print("CDAudio_SysShutdown: MCI_CLOSE failed\n");
}
