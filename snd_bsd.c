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

#include <sys/param.h>
#include <sys/audioio.h>
#ifndef SUNOS
#	include <sys/endian.h>
#endif
#include <sys/ioctl.h>

#include <fcntl.h>
#ifndef SUNOS
#	include <paths.h>
#endif
#include <unistd.h>

#include "snd_main.h"


static int audio_fd = -1;


/*
====================
SndSys_Init

Create "snd_renderbuffer" with the proper sound format if the call is successful
May return a suggested format if the requested format isn't available
====================
*/
qboolean SndSys_Init (const snd_format_t* requested, snd_format_t* suggested)
{
	unsigned int i;
	const char *snddev;
	audio_info_t info;

	// Open the audio device
#ifdef _PATH_SOUND
	snddev = _PATH_SOUND;
#elif defined(SUNOS)
	snddev = "/dev/audio";
#else
	snddev = "/dev/sound";
#endif
	audio_fd = open (snddev, O_WRONLY | O_NDELAY | O_NONBLOCK);
	if (audio_fd < 0)
	{
		Con_Printf("Can't open the sound device (%s)\n", snddev);
		return false;
	}

	AUDIO_INITINFO (&info);
#ifdef AUMODE_PLAY	// NetBSD / OpenBSD
	info.mode = AUMODE_PLAY;
#endif
	info.play.sample_rate = requested->speed;
	info.play.channels = requested->channels;
	info.play.precision = requested->width * 8;
	if (requested->width == 1)
#ifdef SUNOS
		info.play.encoding = AUDIO_ENCODING_LINEAR8;
#else
		info.play.encoding = AUDIO_ENCODING_ULINEAR;
#endif
	else
#ifdef SUNOS
		info.play.encoding = AUDIO_ENCODING_LINEAR;
#else
	if (mem_bigendian)
		info.play.encoding = AUDIO_ENCODING_SLINEAR_BE;
	else
		info.play.encoding = AUDIO_ENCODING_SLINEAR_LE;
#endif

	if (ioctl (audio_fd, AUDIO_SETINFO, &info) != 0)
	{
		Con_Printf("Can't set up the sound device (%s)\n", snddev);
		return false;
	}

	// TODO: check the parameters with AUDIO_GETINFO
	// TODO: check AUDIO_ENCODINGFLAG_EMULATED with AUDIO_GETENC

	snd_renderbuffer = Snd_CreateRingBuffer(requested, 0, NULL);
	return true;
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown (void)
{
	if (audio_fd >= 0)
	{
		close(audio_fd);
		audio_fd = -1;
	}

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
	unsigned int startoffset, factor, limit, nbframes;
	int written;

	if (audio_fd < 0 ||
		snd_renderbuffer->startframe == snd_renderbuffer->endframe)
		return;

	startoffset = snd_renderbuffer->startframe % snd_renderbuffer->maxframes;
	factor = snd_renderbuffer->format.width * snd_renderbuffer->format.channels;
	limit = snd_renderbuffer->maxframes - startoffset;
	nbframes = snd_renderbuffer->endframe - snd_renderbuffer->startframe;
	if (nbframes > limit)
	{
		written = write (audio_fd, &snd_renderbuffer->ring[startoffset * factor], limit * factor);
		if (written < 0)
		{
			Con_Printf("SndSys_Submit: audio write returned %d!\n", written);
			return;
		}

		if (written % factor != 0)
			Sys_Error("SndSys_Submit: nb of bytes written (%d) isn't aligned to a frame sample!\n", written);

		snd_renderbuffer->startframe += written / factor;

		if ((unsigned int)written < limit * factor)
		{
			Con_Printf("SndSys_Submit: audio can't keep up! (%u < %u)\n", written, limit * factor);
			return;
		}

		nbframes -= limit;
		startoffset = 0;
	}

	written = write (audio_fd, &snd_renderbuffer->ring[startoffset * factor], nbframes * factor);
	if (written < 0)
	{
		Con_Printf("SndSys_Submit: audio write returned %d!\n", written);
		return;
	}
	snd_renderbuffer->startframe += written / factor;
}


/*
====================
SndSys_GetSoundTime

Returns the number of sample frames consumed since the sound started
====================
*/
unsigned int SndSys_GetSoundTime (void)
{
	audio_info_t info;

	if (ioctl (audio_fd, AUDIO_GETINFO, &info) < 0)
	{
		Con_Print("Error: can't get audio info\n");
		SndSys_Shutdown ();
		return 0;
	}

	return info.play.samples;
}


/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
	// Nothing to do
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
	// Nothing to do
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
