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

#include <sys/param.h>
#include <sys/audioio.h>
#ifndef SUNOS
#include <sys/endian.h>
#endif
#include <sys/ioctl.h>

#include <fcntl.h>
#ifndef SUNOS
#include <paths.h>
#endif
#include <unistd.h>

#include "quakedef.h"
#include "snd_main.h"


static const int tryrates[] = {44100, 22050, 11025, 8000};

static int audio_fd = -1;
static qboolean snd_inited = false;

// TODO: allocate them in SNDDMA_Init, with a size depending on
// the sound format (enough for 0.5 sec of sound for instance)
#define SND_BUFF_SIZE 65536
static qbyte dma_buffer [SND_BUFF_SIZE];
static qbyte writebuf [SND_BUFF_SIZE];


qboolean SNDDMA_Init (void)
{
	unsigned int i;
	const char *snddev;
	audio_info_t info;

	memset ((void*)shm, 0, sizeof (*shm));

	// Open the audio device
#ifdef _PATH_SOUND
	snddev = _PATH_SOUND;
#else
#ifndef SUNOS
	snddev = "/dev/sound";
#else
	snddev = "/dev/audio";
#endif
#endif
	audio_fd = open (snddev, O_WRONLY | O_NDELAY | O_NONBLOCK);
	if (audio_fd < 0)
	{
		Con_Printf("Can't open the sound device (%s)\n", snddev);
		return false;
	}

	// Look for an appropriate sound format
	// TODO: we should also test mono/stereo and bits
	// TODO: support "-sndspeed", "-sndbits", "-sndmono" and "-sndstereo"
	shm->format.channels = 2;
	shm->format.width = 2;
	for (i = 0; i < sizeof (tryrates) / sizeof (tryrates[0]); i++)
	{
		shm->format.speed = tryrates[i];

		AUDIO_INITINFO (&info);
		info.play.sample_rate = shm->format.speed;
		info.play.channels = shm->format.channels;
		info.play.precision = shm->format.width * 8;
// We only handle sound cards of the same endianess than the CPU
#if BYTE_ORDER == BIG_ENDIAN
		info.play.encoding = AUDIO_ENCODING_SLINEAR_BE;
#else
#ifndef SUNOS
		info.play.encoding = AUDIO_ENCODING_SLINEAR_LE;
#else
		info.play.encoding = AUDIO_ENCODING_LINEAR;
#endif
#endif
		if (ioctl (audio_fd, AUDIO_SETINFO, &info) == 0)
			break;
	}
	if (i == sizeof (tryrates) / sizeof (tryrates[0]))
	{
		Con_Print("Can't select an appropriate sound output format\n");
		close (audio_fd);
		return false;
	}

	// Print some information
	Con_Printf("%d bit %s sound initialized (rate: %dHz)\n",
				info.play.precision,
				(info.play.channels == 2) ? "stereo" : "mono",
				info.play.sample_rate);

	shm->samples = sizeof (dma_buffer) / shm->format.width;
	shm->samplepos = 0;
	shm->buffer = dma_buffer;

	snd_inited = true;
	return true;
}

int SNDDMA_GetDMAPos (void)
{
	audio_info_t info;

	if (!snd_inited)
		return 0;

	if (ioctl (audio_fd, AUDIO_GETINFO, &info) < 0)
	{
		Con_Print("Error: can't get audio info\n");
		SNDDMA_Shutdown ();
		return 0;
	}

	return ((info.play.samples * shm->format.channels) % shm->samples);
}

void SNDDMA_Shutdown (void)
{
	if (snd_inited)
	{
		close (audio_fd);
		audio_fd = -1;
		snd_inited = false;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit (void)
{
	int bsize;
	int bytes, b;
	static int wbufp = 0;
	unsigned char *p;
	int idx;
	int stop = paintedtime;

	if (!snd_inited)
		return;

	if (paintedtime < wbufp)
		wbufp = 0; // reset

	bsize = shm->format.channels * shm->format.width;
	bytes = (paintedtime - wbufp) * bsize;

	if (!bytes)
		return;

	if (bytes > sizeof (writebuf))
	{
		bytes = sizeof (writebuf);
		stop = wbufp + bytes / bsize;
	}

	// Transfert the sound data from the circular dma_buffer to writebuf
	// TODO: using 2 memcpys instead of this loop should be faster
	p = writebuf;
	idx = (wbufp*bsize) & (sizeof (dma_buffer) - 1);
	for (b = bytes; b; b--)
	{
		*p++ = dma_buffer[idx];
		idx = (idx + 1) & (sizeof (dma_buffer) - 1);
	}

	if (write (audio_fd, writebuf, bytes) < bytes)
		Con_Print("audio can't keep up!\n");

	wbufp = stop;
}

void *S_LockBuffer (void)
{
	return shm->buffer;
}

void S_UnlockBuffer (void)
{
}
