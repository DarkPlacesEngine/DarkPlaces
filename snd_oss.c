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

// OSS module, used by Linux and FreeBSD

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/soundcard.h>
#include <stdio.h>
#include "quakedef.h"
#include "snd_main.h"

int audio_fd;

static int tryrates[] = {44100, 48000, 22050, 24000, 11025, 16000, 8000};

qboolean SNDDMA_Init(void)
{
	int rc;
	int fmt;
	int tmp;
	int i;
	char *s;
	struct audio_buf_info info;
	int caps;
	int format16bit;

#if BYTE_ORDER == BIG_ENDIAN
	format16bit = AFMT_S16_BE;
#else
	format16bit = AFMT_S16_LE;
#endif

	// open /dev/dsp, confirm capability to mmap, and get size of dma buffer
    audio_fd = open("/dev/dsp", O_RDWR);  // we have to open it O_RDWR for mmap
	if (audio_fd < 0)
	{
		perror("/dev/dsp");
		Con_Print("Could not open /dev/dsp\n");
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) < 0)
	{
		perror("/dev/dsp");
		Con_Print("Could not reset /dev/dsp\n");
		close(audio_fd);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		perror("/dev/dsp");
		Con_Print("Sound driver too old\n");
		close(audio_fd);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Print("Sorry but your soundcard can't do this\n");
		close(audio_fd);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
	{
		perror("GETOSPACE");
		Con_Print("Um, can't do GETOSPACE?\n");
		close(audio_fd);
		return 0;
	}

	// set sample bits & speed
	s = getenv("QUAKE_SOUND_SAMPLEBITS");
	if (s)
		shm->format.width = atoi(s) / 8;
// COMMANDLINEOPTION: Linux OSS Sound: -sndbits <bits> chooses 8 bit or 16 bit sound output
	else if ((i = COM_CheckParm("-sndbits")) != 0)
		shm->format.width = atoi(com_argv[i+1]) / 8;

	if (shm->format.width != 2 && shm->format.width != 1)
	{
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &fmt);
		if (fmt & format16bit)
			shm->format.width = 2;
		else if (fmt & AFMT_U8)
			shm->format.width = 1;
    }

	s = getenv("QUAKE_SOUND_SPEED");
	if (s)
		shm->format.speed = atoi(s);
// COMMANDLINEOPTION: Linux OSS Sound: -sndspeed <hz> chooses 44100 hz, 22100 hz, or 11025 hz sound output rate
	else if ((i = COM_CheckParm("-sndspeed")) != 0)
		shm->format.speed = atoi(com_argv[i+1]);
	else
	{
		for (i = 0;i < (int) sizeof(tryrates) / (int) sizeof(tryrates[0]);i++)
			if (!ioctl(audio_fd, SNDCTL_DSP_SPEED, &tryrates[i]))
				break;

		shm->format.speed = tryrates[i];
    }

	s = getenv("QUAKE_SOUND_CHANNELS");
	if (s)
		shm->format.channels = atoi(s);
// COMMANDLINEOPTION: Linux OSS Sound: -sndmono sets sound output to mono
	else if ((i = COM_CheckParm("-sndmono")) != 0)
		shm->format.channels = 1;
// COMMANDLINEOPTION: Linux OSS Sound: -sndstereo sets sound output to stereo
	else // if ((i = COM_CheckParm("-sndstereo")) != 0)
		shm->format.channels = 2;

	tmp = (shm->format.channels == 2);
	rc = ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp);
	if (rc < 0)
	{
		perror("/dev/dsp");
		Con_Printf("Could not set /dev/dsp to stereo=%d\n", tmp);
		close(audio_fd);
		return 0;
	}

	rc = ioctl(audio_fd, SNDCTL_DSP_SPEED, &shm->format.speed);
	if (rc < 0)
	{
		perror("/dev/dsp");
		Con_Printf("Could not set /dev/dsp speed to %d\n", shm->format.speed);
		close(audio_fd);
		return 0;
	}

	if (shm->format.width == 2)
	{
		rc = format16bit;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0)
		{
			perror("/dev/dsp");
			Con_Print("Could not support 16-bit data.  Try 8-bit.\n");
			close(audio_fd);
			return 0;
		}
	}
	else if (shm->format.width == 1)
	{
		rc = AFMT_U8;
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);
		if (rc < 0)
		{
			perror("/dev/dsp");
			Con_Print("Could not support 8-bit data.\n");
			close(audio_fd);
			return 0;
		}
	}
	else
	{
		perror("/dev/dsp");
		Con_Printf("%d-bit sound not supported.\n", shm->format.width * 8);
		close(audio_fd);
		return 0;
	}

	shm->sampleframes = info.fragstotal * info.fragsize / shm->format.width / shm->format.channels;
	shm->samples = shm->sampleframes * shm->format.channels;

	// memory map the dma buffer
	shm->bufferlength = info.fragstotal * info.fragsize;
	shm->buffer = (unsigned char *) mmap(NULL, shm->bufferlength, PROT_WRITE, MAP_FILE|MAP_SHARED, audio_fd, 0);
	if (!shm->buffer || shm->buffer == (unsigned char *)-1)
	{
		perror("/dev/dsp");
		Con_Print("Could not mmap /dev/dsp\n");
		close(audio_fd);
		return 0;
	}

	// toggle the trigger & start her up
	tmp = 0;
	rc  = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror("/dev/dsp");
		Con_Print("Could not toggle.\n");
		close(audio_fd);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror("/dev/dsp");
		Con_Print("Could not toggle.\n");
		close(audio_fd);
		return 0;
	}

	shm->samplepos = 0;

	return 1;
}

int SNDDMA_GetDMAPos(void)
{

	struct count_info count;

	if (!shm) return 0;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count)==-1)
	{
		perror("/dev/dsp");
		Con_Print("Uh, sound dead.\n");
		S_Shutdown();
		return 0;
	}
	shm->samplepos = count.ptr / shm->format.width;

	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	int tmp;
	// unmap the memory
	munmap(shm->buffer, shm->bufferlength);
	// stop the sound
	tmp = 0;
	ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
	// close the device
	close(audio_fd);
	audio_fd = -1;
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

void *S_LockBuffer(void)
{
	return shm->buffer;
}

void S_UnlockBuffer(void)
{
}

