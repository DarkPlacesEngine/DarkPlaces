/*
	snd_alsa_0_6.c

	(description)

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

#include "qtypes.h"
#include "sound.h"
#include "qargs.h"
#include "console.h"

static int snd_inited;

static snd_pcm_t *pcm_handle;
static snd_pcm_params_info_t cpinfo;
static snd_pcm_params_t params;
static snd_pcm_setup_t setup;
static snd_pcm_channel_area_t mmap_stopped_areas[2];
static snd_pcm_channel_area_t mmap_running_areas[2];
static int card=-1,dev=-1,subdev=-1;;

int check_card(int card)
{
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;
	int rc;

	if ((rc = snd_ctl_hw_open(&handle, 0, card)) < 0) {
		Con_Printf("Error: control open (%i): %s\n", card, snd_strerror(rc));
		return rc;
	}
	if ((rc = snd_ctl_hw_info(handle, &info)) < 0) {
		Con_Printf("Error: control hardware info (%i): %s\n", card,
				   snd_strerror(rc));
		snd_ctl_close(handle);
		return rc;
	}
	snd_ctl_close(handle);
	if (dev==-1) {
		for (dev = 0; dev < info.pcmdevs; dev++) {
			if ((rc=snd_pcm_hw_open_subdevice(&pcm_handle,card,dev,subdev,
								 SND_PCM_STREAM_PLAYBACK,
								 SND_PCM_NONBLOCK))==0) {
				return 0;
			}
		}
	} else {
		if (dev>=0 && dev <info.pcmdevs) {
			if ((rc=snd_pcm_hw_open_subdevice(&pcm_handle,card,dev,subdev,
								 SND_PCM_STREAM_PLAYBACK,
								 SND_PCM_NONBLOCK))==0) {
				return 0;
			}
		}
	}
	return 1;
}

qboolean SNDDMA_Init(void)
{
	int rc=0,i;
	char *err_msg="";
	int rate=-1,format=-1,bps,stereo=-1,frag_size,frame_size;
	unsigned int mask;

	mask = snd_cards_mask();
	if (!mask) {
		Con_Printf("No sound cards detected\n");
		return 0;
	}
	if ((i=COM_CheckParm("-sndcard"))!=0) {
		card=atoi(com_argv[i+1]);
	}
	if ((i=COM_CheckParm("-snddev"))!=0) {
		dev=atoi(com_argv[i+1]);
	}
	if ((i=COM_CheckParm("-sndbits")) != 0) {
		i = atoi(com_argv[i+1]);
		if (i==16) {
			format = SND_PCM_SFMT_S16_LE;
		} else if (i==8) {
			format = SND_PCM_SFMT_U8;
		} else {
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
	if (card==-1) {
		for (card=0; card<SND_CARDS; card++) {
			if (!(mask & (1<<card)))
				continue;
			rc=check_card(card);
			if (rc<0)
				return 0;
			if (!rc)
				goto dev_openned;
		}
	} else {
		if (dev==-1) {
			rc=check_card(card);
			if (rc<0)
				return 0;
			if (!rc)
				goto dev_openned;
		} else {
			if ((rc=snd_pcm_hw_open_subdevice(&pcm_handle,card,dev,subdev,
								 SND_PCM_STREAM_PLAYBACK,
								 SND_PCM_NONBLOCK))<0) {
				Con_Printf("Error: audio open error: %s\n", snd_strerror(rc));
				return 0;
			}
			goto dev_openned;
		}
	}
	Con_Printf("Error: audio open error: %s\n", snd_strerror(rc));
	return 0;

 dev_openned:
	Con_Printf("Using card %d, device %d.\n", card, dev);
	memset(&cpinfo, 0, sizeof(cpinfo));
	snd_pcm_params_info(pcm_handle, &cpinfo);
	Con_Printf("%08x %08x %08x\n",cpinfo.flags,cpinfo.formats,cpinfo.rates);
	if ((rate==-1 || rate==44100) && cpinfo.rates & SND_PCM_RATE_44100) {
		rate=44100;
		frag_size=256;	/* assuming stereo 8 bit */
	} else if ((rate==-1 || rate==22050) && cpinfo.rates & SND_PCM_RATE_22050) {
		rate=22050;
		frag_size=128;	/* assuming stereo 8 bit */
	} else if ((rate==-1 || rate==11025) && cpinfo.rates & SND_PCM_RATE_11025) {
		rate=11025;
		frag_size=64;	/* assuming stereo 8 bit */
	} else {
		Con_Printf("ALSA: desired rates not supported\n");
		goto error_2;
	}
	if ((format==-1 || format==SND_PCM_SFMT_S16_LE) && cpinfo.formats & SND_PCM_FMT_S16_LE) {
		format=SND_PCM_SFMT_S16_LE;
		bps=16;
		frame_size=2;
	} else if ((format==-1 || format==SND_PCM_SFMT_U8) && cpinfo.formats & SND_PCM_FMT_U8) {
		format=SND_PCM_SFMT_U8;
		bps=8;
		frame_size=1;
	} else {
		Con_Printf("ALSA: desired formats not supported\n");
		goto error_2;
	}
	//XXX can't support non-interleaved stereo
	if (stereo && (cpinfo.flags & SND_PCM_INFO_INTERLEAVED) && cpinfo.max_channels>=2) {
		stereo=1;
		frame_size*=2;
	} else {
		stereo=0;
	}

	memset(&params, 0, sizeof(params));
	//XXX can't support non-interleaved stereo
	params.xfer_mode = stereo ? SND_PCM_XFER_INTERLEAVED
							  : SND_PCM_XFER_NONINTERLEAVED;
	params.mmap_shape = stereo ? SND_PCM_XFER_INTERLEAVED
								: SND_PCM_XFER_NONINTERLEAVED;
	params.format.sfmt=format;
	params.format.rate=rate;
	params.format.channels=stereo+1;
	params.start_mode = SND_PCM_START_EXPLICIT;
	params.xrun_mode = SND_PCM_XRUN_NONE;

	params.buffer_size = (2<<16) / frame_size;
	params.frag_size=frag_size;
	params.avail_min = frag_size;

	params.boundary = params.buffer_size;

	while (1) {
		err_msg="audio params";
		if ((rc=snd_pcm_params(pcm_handle, &params))<0)
			goto error;

		memset(&setup, 0, sizeof(setup));
		err_msg="audio setup";
		if ((rc=snd_pcm_setup(pcm_handle, &setup))<0)
			goto error;
		if (setup.buffer_size == params.buffer_size)
			break;
		params.boundary = params.buffer_size = setup.buffer_size;
	}

	err_msg="audio mmap";
	if ((rc=snd_pcm_mmap(pcm_handle))<0)
		goto error;
	if ((rc=snd_pcm_mmap_get_areas(pcm_handle, mmap_stopped_areas, mmap_running_areas)))
		goto error;
	err_msg="audio prepare";
	if ((rc=snd_pcm_prepare(pcm_handle))<0)
		goto error;

	shm=&sn;
	memset((dma_t*)shm,0,sizeof(*shm));
    shm->splitbuffer = 0;
	shm->channels=setup.format.channels;
	shm->submission_chunk=frag_size;			// don't mix less than this #
	shm->samplepos=0;							// in mono samples
	shm->samplebits=setup.format.sfmt==SND_PCM_SFMT_S16_LE?16:8;
	shm->samples=setup.buffer_size*shm->channels;	// mono samples in buffer
	shm->speed=setup.format.rate;
	shm->buffer=(unsigned char*)mmap_running_areas->addr;
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
	Con_Printf("Error: %s: %s\n", err_msg, snd_strerror(rc));
 error_2:
	snd_pcm_close(pcm_handle);
	return 0;
}

static inline int
get_hw_ptr()
{
	size_t app_ptr;
	ssize_t delay;
	int hw_ptr;

	if (snd_pcm_state (pcm_handle) != SND_PCM_STATE_RUNNING)
		return 0;
	app_ptr = snd_pcm_mmap_offset (pcm_handle);
	snd_pcm_delay (pcm_handle, &delay);
	hw_ptr = app_ptr - delay;
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
		snd_pcm_close(pcm_handle);
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

	state = snd_pcm_state (pcm_handle);

	switch (state) {
	case SND_PCM_STATE_PREPARED:
		snd_pcm_mmap_forward (pcm_handle, count);
		snd_pcm_start (pcm_handle);
		break;
	case SND_PCM_STATE_RUNNING:
		hw_ptr = get_hw_ptr();
		missed = hw_ptr - shm->samplepos / shm->channels;
		count -= missed;
		offset = snd_pcm_mmap_offset (pcm_handle);
		if (offset > hw_ptr)
			count -= (offset - hw_ptr);
		else
			count -= (setup.buffer_size - hw_ptr + offset);
		if (count < 0) {
			snd_pcm_rewind (pcm_handle, -count);
		} else if (count > 0) {
			avail = snd_pcm_avail_update(pcm_handle);
			if (avail > 0 && count > avail) {
				snd_pcm_mmap_forward (pcm_handle, avail);
				count -= avail;
			}
			snd_pcm_mmap_forward (pcm_handle, count);
		}
		break;
	default:
		break;
	}
}

