/*
Copyright (C) 2004 Andreas Kirsch

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
#include "snd_main.h"
#include <SDL.h>

/*
Info:
One SDL sample consists of x channel samples
The mixer supposes that the driver has one channel entry/sample though it has x channels/sample
like the SDL
*/

#define AUDIO_SDL_SAMPLES		4096
#define AUDIO_LOCALFACTOR		4

typedef struct AudioState_s
{
	int		width;
    int		size;
	int		pos;
	void	*buffer;
} AudioState;


static AudioState	 as;

static void Buffer_Callback(void *userdata, Uint8 *stream, int len);

/*
==================
S_BlockSound
==================
*/
void S_BlockSound( void )
{
	snd_blocked++;

	if( snd_blocked == 1 )
		SDL_PauseAudio( true );
}


/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound( void )
{
	snd_blocked--;
	if( snd_blocked == 0 )
		SDL_PauseAudio( false );
}


/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

qboolean SNDDMA_Init(void)
{
	SDL_AudioSpec spec;
	int i;

	// Init the SDL Audio subsystem
	if( SDL_InitSubSystem( SDL_INIT_AUDIO ) ) {
		Con_Print( "Initializing the SDL Audio subsystem failed!\n" );
		return false;
	}

	// Init the shm structure
	memset( (void*) shm, 0, sizeof(*shm) );

	shm->format.channels = 2; //stereo
	shm->format.width = 2;

// COMMANDLINEOPTION: SDL Sound: -sndspeed <hz> chooses 44100 hz, 22100 hz, or 11025 hz sound output rate
	i = COM_CheckParm( "-sndspeed" );
	if( i && i != ( com_argc - 1 ) )
		shm->format.speed = atoi( com_argv[ i+1 ] );
	else
		shm->format.speed = 44100;

	shm->samplepos = 0;
	shm->samples = AUDIO_SDL_SAMPLES * AUDIO_LOCALFACTOR;
	shm->bufferlength = shm->samples * shm->format.width;
	shm->buffer = (qbyte *)Mem_Alloc( snd_mempool, shm->bufferlength );

	// Init the as structure
	as.buffer = shm->buffer;
	as.width = shm->format.width;
	as.pos = 0;
	as.size = shm->bufferlength;

	// Init the SDL Audio subsystem
	spec.callback = Buffer_Callback;
	spec.channels = shm->format.channels;
	spec.format = AUDIO_S16SYS;
	spec.freq = shm->format.speed;
	spec.userdata = NULL;
	spec.samples = AUDIO_SDL_SAMPLES;

	if( SDL_OpenAudio( &spec, NULL ) ) {
		Con_Print( "Failed to open the audio device!\n" );
		Con_DPrintf(
			"Audio Specification:\n"
			"\tChannels  : %i\n"
			"\tFormat    : %x\n"
			"\tFrequency : %i\n"
			"\tBuffersize: %i Bytes(%i Samples)\n",
			spec.channels, spec.format, spec.freq, shm->bufferlength , spec.samples );
		Mem_Free( shm->buffer );
		return false;
	}

	SDL_PauseAudio( false );

	return true;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	shm->samplepos = (as.pos / as.width) % shm->samples;
	return shm->samplepos;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{

}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	SDL_CloseAudio();
	Mem_Free( as.buffer );
}

void *S_LockBuffer(void)
{
	SDL_LockAudio();
	return shm->buffer;
}

void S_UnlockBuffer(void)
{
	SDL_UnlockAudio();
}

static void Buffer_Callback(void *userdata, Uint8 *stream, int len)
{
	if( len > as.size )
		len = as.size;
	if( len > as.size - as.pos ) {
		memcpy( stream, (Uint8*) as.buffer + as.pos, as.size - as.pos );
		len -= as.size - as.pos;
		as.pos = 0;
	}
	memcpy( stream, (Uint8*) as.buffer + as.pos, len );
	as.pos = (as.pos + len) % as.size;
}

