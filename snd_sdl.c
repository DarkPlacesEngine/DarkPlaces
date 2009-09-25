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

#include <math.h>
#include <SDL.h>

#include "snd_main.h"


static unsigned int sdlaudiotime = 0;


// Note: SDL calls SDL_LockAudio() right before this function, so no need to lock the audio data here
static void Buffer_Callback (void *userdata, Uint8 *stream, int len)
{
	unsigned int factor, RequestedFrames, MaxFrames, FrameCount;
	unsigned int StartOffset, EndOffset;

	factor = snd_renderbuffer->format.channels * snd_renderbuffer->format.width;
	if ((unsigned int)len % factor != 0)
		Sys_Error("SDL sound: invalid buffer length passed to Buffer_Callback (%d bytes)\n", len);

	RequestedFrames = (unsigned int)len / factor;

	if (SndSys_LockRenderBuffer())
	{
		if (snd_usethreadedmixing)
		{
			S_MixToBuffer(stream, RequestedFrames);
			if (snd_blocked)
				memset(stream, snd_renderbuffer->format.width == 1 ? 0x80 : 0, len);
			SndSys_UnlockRenderBuffer();
			return;
		}

		// Transfert up to a chunk of samples from snd_renderbuffer to stream
		MaxFrames = snd_renderbuffer->endframe - snd_renderbuffer->startframe;
		if (MaxFrames > RequestedFrames)
			FrameCount = RequestedFrames;
		else
			FrameCount = MaxFrames;
		StartOffset = snd_renderbuffer->startframe % snd_renderbuffer->maxframes;
		EndOffset = (snd_renderbuffer->startframe + FrameCount) % snd_renderbuffer->maxframes;
		if (StartOffset > EndOffset)  // if the buffer wraps
		{
			unsigned int PartialLength1, PartialLength2;

			PartialLength1 = (snd_renderbuffer->maxframes - StartOffset) * factor;
			memcpy(stream, &snd_renderbuffer->ring[StartOffset * factor], PartialLength1);

			PartialLength2 = FrameCount * factor - PartialLength1;
			memcpy(&stream[PartialLength1], &snd_renderbuffer->ring[0], PartialLength2);
		}
		else
			memcpy(stream, &snd_renderbuffer->ring[StartOffset * factor], FrameCount * factor);

		snd_renderbuffer->startframe += FrameCount;

		if (FrameCount < RequestedFrames && developer.integer >= 1000 && vid_activewindow)
			Con_Printf("SDL sound: %u sample frames missing\n", RequestedFrames - FrameCount);

		sdlaudiotime += RequestedFrames;

		SndSys_UnlockRenderBuffer();
	}
}


/*
====================
SndSys_Init

Create "snd_renderbuffer" with the proper sound format if the call is successful
May return a suggested format if the requested format isn't available
====================
*/
qboolean SndSys_Init (const snd_format_t* requested, snd_format_t* suggested)
{
	unsigned int buffersize;
	SDL_AudioSpec wantspec;
	SDL_AudioSpec obtainspec;

	snd_threaded = false;

	Con_DPrint ("SndSys_Init: using the SDL module\n");

	// Init the SDL Audio subsystem
	if( SDL_InitSubSystem( SDL_INIT_AUDIO ) ) {
		Con_Print( "Initializing the SDL Audio subsystem failed!\n" );
		return false;
	}

	buffersize = (unsigned int)ceil((double)requested->speed / 25.0); // 2048 bytes on 24kHz to 48kHz

	// Init the SDL Audio subsystem
	wantspec.callback = Buffer_Callback;
	wantspec.userdata = NULL;
	wantspec.freq = requested->speed;
	wantspec.format = ((requested->width == 1) ? AUDIO_U8 : AUDIO_S16SYS);
	wantspec.channels = requested->channels;
	wantspec.samples = CeilPowerOf2(buffersize);  // needs to be a power of 2 on some platforms.

	Con_Printf("Wanted audio Specification:\n"
				"\tChannels  : %i\n"
				"\tFormat    : 0x%X\n"
				"\tFrequency : %i\n"
				"\tSamples   : %i\n",
				wantspec.channels, wantspec.format, wantspec.freq, wantspec.samples);

	if( SDL_OpenAudio( &wantspec, &obtainspec ) )
	{
		Con_Printf( "Failed to open the audio device! (%s)\n", SDL_GetError() );
		return false;
	}

	Con_Printf("Obtained audio specification:\n"
				"\tChannels  : %i\n"
				"\tFormat    : 0x%X\n"
				"\tFrequency : %i\n"
				"\tSamples   : %i\n",
				obtainspec.channels, obtainspec.format, obtainspec.freq, obtainspec.samples);

	// If we haven't obtained what we wanted
	if (wantspec.freq != obtainspec.freq ||
		wantspec.format != obtainspec.format ||
		wantspec.channels != obtainspec.channels)
	{
		SDL_CloseAudio();

		// Pass the obtained format as a suggested format
		if (suggested != NULL)
		{
			suggested->speed = obtainspec.freq;
			// FIXME: check the format more carefully. There are plenty of unsupported cases
			suggested->width = ((obtainspec.format == AUDIO_U8) ? 1 : 2);
			suggested->channels = obtainspec.channels;
		}

		return false;
	}

	snd_threaded = true;

	snd_renderbuffer = Snd_CreateRingBuffer(requested, 0, NULL);
	if (snd_channellayout.integer == SND_CHANNELLAYOUT_AUTO)
	{
		int newlayout;

#ifdef __linux__
		newlayout = SND_CHANNELLAYOUT_ALSA;
#else
		newlayout = SND_CHANNELLAYOUT_STANDARD;
#endif
		Cvar_SetValueQuick (&snd_channellayout, newlayout);
	}

	sdlaudiotime = 0;
	SDL_PauseAudio( false );

	return true;
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown(void)
{
	SDL_CloseAudio();

	if (snd_renderbuffer != NULL)
	{
		Mem_Free(snd_renderbuffer->ring);
		Mem_Free(snd_renderbuffer);
		snd_renderbuffer = NULL;
	}
}


/*
====================
SndSys_Submit

Submit the contents of "snd_renderbuffer" to the sound card
====================
*/
void SndSys_Submit (void)
{
	// Nothing to do here (this sound module is callback-based)
}


/*
====================
SndSys_GetSoundTime

Returns the number of sample frames consumed since the sound started
====================
*/
unsigned int SndSys_GetSoundTime (void)
{
	return sdlaudiotime;
}


/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
	SDL_LockAudio();
	return true;
}


/*
====================
SndSys_UnlockRenderBuffer

Release the exclusive lock on "snd_renderbuffer"
====================
*/
void SndSys_UnlockRenderBuffer (void)
{
	SDL_UnlockAudio();
}

/*
====================
SndSys_SendKeyEvents

Send keyboard events originating from the sound system (e.g. MIDI)
====================
*/
void SndSys_SendKeyEvents(void)
{
	// not supported
}
