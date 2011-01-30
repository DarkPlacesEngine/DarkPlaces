/*
Copyright (C) 2004 Andreas Kirsch (used cd_null.c as template)
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

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2
// SDL 1.2 has CD audio

#include <SDL.h>
#include <time.h>

// If one of the functions fails, it returns -1, if not 0

// SDL supports multiple cd devices - so we are going to support this, too.
static void CDAudio_SDL_CDDrive_f( void );

// we only support playing one CD at a time
static SDL_CD *cd;

static int ValidateDrive( void )
{
	if( cd && SDL_CDStatus( cd ) > 0 )
		return cdValid = true;

	return cdValid = false;
}

void CDAudio_SysEject (void)
{
	SDL_CDEject( cd );
}


void CDAudio_SysCloseDoor (void)
{
	//NO SDL FUNCTION
}

int CDAudio_SysGetAudioDiskInfo (void)
{
	if( ValidateDrive() ) // everything > 0 is ok, 0 is trayempty and -1 is error
		return cd->numtracks;
	return -1;
}

float CDAudio_SysGetVolume (void)
{
	return -1.0f;
}

void CDAudio_SysSetVolume (float volume)
{
	//NO SDL FUNCTION
}

int CDAudio_SysPlay (int track)
{
	return SDL_CDPlayTracks(cd, track-1, 0, 1, 0);
}

int CDAudio_SysStop (void)
{
	return SDL_CDStop( cd );
}

int CDAudio_SysPause (void)
{
	return SDL_CDPause( cd );
}

int CDAudio_SysResume (void)
{
	return SDL_CDResume( cd );
}

int CDAudio_SysUpdate (void)
{
	static time_t lastchk = 0;

	if (cdPlaying && lastchk < time(NULL))
	{
		lastchk = time(NULL) + 2; //two seconds between chks
		if( !cd || cd->status <= 0 ) {
			cdValid = false;
			return -1;
		}
		if (SDL_CDStatus( cd ) == CD_STOPPED)
		{
			if( cdPlayLooping )
				CDAudio_SysPlay( cdPlayTrack );
			else
				cdPlaying = false;
		}
	}
	return 0;
}

void CDAudio_SysInit (void)
{
	if( SDL_InitSubSystem( SDL_INIT_CDROM ) == -1 )
		Con_Print( "Failed to init the CDROM SDL subsystem!\n" );

	Cmd_AddCommand( "cddrive", CDAudio_SDL_CDDrive_f, "select an SDL-detected CD drive by number" );
}

static int IsAudioCD( void )
{
	int i;
	for( i = 0 ; i < cd->numtracks ; i++ )
		if( cd->track[ i ].type == SDL_AUDIO_TRACK )
			return true;
	return false;
}

int CDAudio_SysStartup (void)
{
	int i;
	int numdrives;

	numdrives = SDL_CDNumDrives();
	if( numdrives == -1 ) // was the CDROM system initialized correctly?
		return -1;

	Con_Printf( "Found %i cdrom drives.\n", numdrives );

	for( i = 0 ; i < numdrives ; i++, cd = NULL ) {
		cd = SDL_CDOpen( i );
		if( !cd ) {
			Con_Printf( "CD drive %i is invalid.\n", i );
			continue;
		}

		if( CD_INDRIVE( SDL_CDStatus( cd ) ) )
			if( IsAudioCD() )
				break;
			else
				Con_Printf( "The CD in drive %i is not an audio cd.\n", i );
		else
			Con_Printf( "No CD in drive %i.\n", i );

		SDL_CDClose( cd );
	}

	if( i == numdrives && !cd )
		return -1;

	return 0;
}

void CDAudio_SysShutdown (void)
{
	if( cd )
		SDL_CDClose( cd );
}

void CDAudio_SDL_CDDrive_f( void )
{
	int i;
	int numdrives = SDL_CDNumDrives();

	if( Cmd_Argc() != 2 ) {
		Con_Print( "cddrive <drivenr>\n" );
		return;
	}

	i = atoi( Cmd_Argv( 1 ) );
	if( i >= numdrives ) {
		Con_Printf("Only %i drives!\n", numdrives );
		return;
	}

	if( cd )
		SDL_CDClose( cd );

	cd = SDL_CDOpen( i );
	if( !cd ) {
		Con_Printf( "Couldn't open drive %i.\n", i );
		return;
	}

	if( !CD_INDRIVE( SDL_CDStatus( cd ) ) )
		Con_Printf( "No cd in drive %i.\n", i );
	else if( !IsAudioCD() )
		Con_Printf( "The CD in drive %i is not an audio CD.\n", i );

	ValidateDrive();
}





#else
// SDL 1.3 does not have CD audio

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
#endif

