/*
	snd_alsa_0_9.c

	Support for ALSA 0.9 sound driver (cvs development version)

	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <sys/asoundlib.h>

#include "quakedef.h"
#include "sound.h"
#include "console.h"

static int snd_inited;

static snd_pcm_t *pcm;
static const snd_pcm_channel_area_t *mmap_areas;
static char *pcmname = NULL;
size_t buffer_size;

qboolean SNDDMA_Init(void)
{
	int err,i;
	int rate=-1,bps=-1,stereo=-1,frag_size;
	snd_pcm_hw_params_t *hw;
	snd_pcm_sw_params_t *sw;
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_sw_params_alloca(&sw);

	if ((i=COM_CheckParm("-sndpcm"))!=0) {
		pcmname=com_argv[i+1];
	}
	if ((i=COM_CheckParm("-sndbits")) != 0) {
		bps = atoi(com_argv[i+1]);
		if (bps != 16 && bps != 8) {
			Con_Printf("Error: invalid sample bits: %d\n", i);
			return 0;
		}
	}
	if ((i=COM_CheckParm("-sndspeed")) != 0) {
		rate = atoi(com_argv[i+1]);
		if (rate!=44100 && rate!=22050 && rate!=11025) {
			Con_Printf("Error: invalid sample rate: %d\n", rate);
			return 0;
		}
	}
	if ((i=COM_CheckParm("-sndmono")) != 0) {
		stereo=0;
	}
	if ((i=COM_CheckParm("-sndstereo")) != 0) {
		stereo=1;
	}
	if (!pcmname)
		pcmname = "plug:0,0";
	if ((err=snd_pcm_open(&pcm, pcmname,
			     SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK))<0) {
		Con_Printf("Error: audio open error: %s\n", snd_strerror(err));
		return 0;
	}

	Con_Printf("Using PCM %s.\n", pcmname);
	snd_pcm_hw_params_any(pcm, hw);


	switch (rate) {
	case -1:
		if (snd_pcm_hw_params_set_rate_near(pcm, hw, 44100, 0) >= 0) {
			frag_size = 256; /* assuming stereo 8 bit */
			rate = 44100;
		} else if (snd_pcm_hw_params_set_rate_near(pcm, hw, 22050, 0) >= 0) {
			frag_size = 128; /* assuming stereo 8 bit */
			rate = 22050;
		} else if (snd_pcm_hw_params_set_rate_near(pcm, hw, 11025, 0) >= 0) {
			frag_size = 64;	/* assuming stereo 8 bit */
			rate = 11025;
		} else {
			Con_Printf("ALSA: no useable rates\n");
			goto error;
		}
		break;
	case 11025:
	case 22050:
	case 44100:
		if (snd_pcm_hw_params_set_rate_near(pcm, hw, rate, 0) >= 0) {
			frag_size = 64 * rate / 11025; /* assuming stereo 8 bit */
			break;
		}
		/* Fall through */
	default:
		Con_Printf("ALSA: desired rate not supported\n");
		goto error;
	}

	switch (bps) {
	case -1:
		if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE) >= 0) {
			bps = 16;
		} else if (snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_U8) >= 0) {
			bps = 8;
		} else {
			Con_Printf("ALSA: no useable formats\n");
			goto error;
		}
		break;
	case 8:
	case 16:
		 if (snd_pcm_hw_params_set_format(pcm, hw, 
						  bps == 8 ? SND_PCM_FORMAT_U8 :
						  SND_PCM_FORMAT_S16) >= 0) {
			 break;
		 }
		/* Fall through */
	default:
		Con_Printf("ALSA: desired format not supported\n");
		goto error;
	}

	if (snd_pcm_hw_params_set_access(pcm, hw, 
					 SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0) {
		Con_Printf("ALSA: interleaved is not supported\n");
		goto error;
	}

	switch (stereo) {
	case -1:
		if (snd_pcm_hw_params_set_channels(pcm, hw, 2) >= 0) {
			stereo = 1;
		} else if (snd_pcm_hw_params_set_channels(pcm, hw, 1) >= 0) {
			stereo = 0;
		} else {
			Con_Printf("ALSA: no useable channels\n");
			goto error;
		}
		break;
	case 0:
	case 1:
		 if (snd_pcm_hw_params_set_channels(pcm, hw, stereo ? 2 : 1) >= 0)
			 break;
		 /* Fall through */
	default:
		Con_Printf("ALSA: desired channels not supported\n");
		goto error;
	}

	snd_pcm_hw_params_set_period_size_near(pcm, hw, frag_size, 0);
	
	err = snd_pcm_hw_params(pcm, hw);
	if (err < 0) {
		Con_Printf("ALSA: unable to install hw params\n");
		goto error;
	}

	snd_pcm_sw_params_current(pcm, sw);
	snd_pcm_sw_params_set_start_mode(pcm, sw, SND_PCM_START_EXPLICIT);
	snd_pcm_sw_params_set_xrun_mode(pcm, sw, SND_PCM_XRUN_NONE);

	err = snd_pcm_sw_params(pcm, sw);
	if (err < 0) {
		Con_Printf("ALSA: unable to install sw params\n");
		goto error;
	}

	mmap_areas = snd_pcm_mmap_running_areas(pcm);

	shm=&sn;
	memset((dma_t*)shm,0,sizeof(*shm));
	shm->splitbuffer = 0;
	shm->channels=stereo+1;
	shm->submission_chunk=snd_pcm_hw_params_get_period_size(hw, 0); // don't mix less than this #
	shm->samplepos=0;			// in mono samples
	shm->samplebits=bps;
	buffer_size = snd_pcm_hw_params_get_buffer_size(hw); 
	shm->samples=buffer_size*shm->channels;	// mono samples in buffer
	shm->speed=rate;
	shm->buffer=(unsigned char*)mmap_areas->addr;
	Con_Printf("%5d stereo\n", shm->channels - 1);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d samplebits\n", shm->samplebits);
	Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf("%5d speed\n", shm->speed);
	Con_Printf("0x%x dma buffer\n", (int)shm->buffer);
	Con_Printf("%5d total_channels\n", total_channels);

	snd_inited=1;
	return 1;
 error:
	snd_pcm_close(pcm);
	return 0;
}

static inline int
get_hw_ptr()
{
	size_t app_ptr;
	snd_pcm_sframes_t delay;
	int hw_ptr;

	if (snd_pcm_state (pcm) != SND_PCM_STATE_RUNNING)
		return 0;
	app_ptr = snd_pcm_mmap_offset (pcm);
	snd_pcm_delay (pcm, &delay);
	hw_ptr = app_ptr - delay;
	if (hw_ptr < 0)
		hw_ptr += buffer_size;
	return hw_ptr;
}

int SNDDMA_GetDMAPos(void)
{
	int hw_ptr;

	if (!snd_inited) return 0;

	hw_ptr = get_hw_ptr();
	hw_ptr *= shm->channels;
	shm->samplepos = hw_ptr;
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if (snd_inited)
	{
		snd_pcm_close(pcm);
		snd_inited = 0;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
	int count = paintedtime - soundtime;
	int avail;
	int missed;
	int state;
	int hw_ptr;
	int offset;

	state = snd_pcm_state (pcm);

	switch (state) {
	case SND_PCM_STATE_PREPARED:
		snd_pcm_mmap_forward (pcm, count);
		snd_pcm_start (pcm);
		break;
	case SND_PCM_STATE_RUNNING:
		hw_ptr = get_hw_ptr();
		missed = hw_ptr - shm->samplepos / shm->channels;
		if (missed < 0)
			missed += buffer_size;
		count -= missed;
		offset = snd_pcm_mmap_offset (pcm);
		if (offset > hw_ptr)
			count -= (offset - hw_ptr);
		else
			count -= (buffer_size - hw_ptr + offset);
		if (count < 0) {
			snd_pcm_rewind (pcm, -count);
		} else {
			avail = snd_pcm_avail_update(pcm);
			if (avail < 0)
				avail = buffer_size;
			if (count > avail)
				count = avail;
			snd_pcm_mmap_forward (pcm, count);
		}
		break;
	default:
		break;
	}
}

