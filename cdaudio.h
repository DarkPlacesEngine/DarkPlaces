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

typedef struct cl_cdstate_s
{
	qboolean Valid;
	qboolean Playing;
	qboolean PlayLooping;
	unsigned char PlayTrack;
}
cl_cdstate_t;

//extern cl_cdstate_t cd;

extern qboolean cdValid;
extern qboolean cdPlaying;
extern qboolean cdPlayLooping;
extern unsigned char cdPlayTrack;

extern cvar_t cdaudioinitialized;

int CDAudio_Init(void);
void CDAudio_Open(void);
void CDAudio_Close(void);
void CDAudio_Play(int track, qboolean looping);
void CDAudio_Play_byName (const char *trackname, qboolean looping, qboolean tryreal, float startposition);
void CDAudio_Stop(void);
void CDAudio_Pause(void);
void CDAudio_Resume(void);
int CDAudio_Startup(void);
void CDAudio_Shutdown(void);
void CDAudio_Update(void);
float CDAudio_GetPosition(void);
void CDAudio_StartPlaylist(qboolean resume);
