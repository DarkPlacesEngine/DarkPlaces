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
// snd_null.c -- include this instead of all the other snd_* files to have
// no sound code whatsoever

#include "quakedef.h"

cvar_t bgmvolume = {CVAR_SAVE, "bgmvolume", "1"};
cvar_t volume = {CVAR_SAVE, "volume", "0.7"};
cvar_t snd_staticvolume = {CVAR_SAVE, "snd_staticvolume", "1"};

cvar_t snd_initialized = { CVAR_READONLY, "snd_initialized", "0"};

void S_Init (void)
{
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&snd_staticvolume);
	Cvar_RegisterVariable(&snd_initialized);
}

void S_Startup (void)
{
}

void S_Shutdown (void)
{
}

void S_TouchSound (const char *sample, qboolean stdpath)
{
}

void S_ClearUsed (void)
{
}

void S_PurgeUnused (void)
{
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
}

int S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation)
{
	return -1;
}

void S_StopChannel (unsigned int channel_ind)
{
}

void S_PauseChannel (unsigned int channel_ind, qboolean toggle)
{
}

void S_LoopChannel (unsigned int channel_ind, qboolean toggle)
{
}

void S_StopSound (int entnum, int entchannel)
{
}

void S_PauseGameSounds (void)
{
}

void S_ResumeGameSounds (void)
{
}

void S_SetChannelVolume (unsigned int ch_ind, float fvol)
{
}

sfx_t *S_GetCached(const char *name, qboolean stdpath)
{
	return NULL;
}

sfx_t *S_PrecacheSound (const char *sample, qboolean complain, qboolean stdpath)
{
	return NULL;
}

void S_Update(const matrix4x4_t *matrix)
{
}

void S_StopAllSounds (qboolean clear)
{
}

void S_ExtraUpdate (void)
{
}

void S_LocalSound (const char *s, qboolean stdpath)
{
}
