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
#include "cdaudio.h"


void CDAudio_SysEject (void)
{
}


void CDAudio_SysCloseDoor (void)
{
}


int CDAudio_SysGetAudioDiskInfo (void)
{
	return -1;
}


float CDAudio_SysGetVolume (void)
{
	return -1.0f;
}


void CDAudio_SysSetVolume (float volume)
{
}


int CDAudio_SysPlay (int track)
{
	return -1;
}


int CDAudio_SysStop (void)
{
	return -1;
}


int CDAudio_SysPause (void)
{
	return -1;
}

int CDAudio_SysResume (void)
{
	return -1;
}

int CDAudio_SysUpdate (void)
{
	return -1;
}


void CDAudio_SysInit (void)
{
}

int CDAudio_SysStartup (void)
{
	return -1;
}

void CDAudio_SysShutdown (void)
{
}
