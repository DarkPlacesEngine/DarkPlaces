/*
	snd_alsa_0_5.c

	Support for ALSA 0.5, the old stable version of ALSA.

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
# include <config.h>
#endif

#include "quakedef.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#if defined HAVE_SYS_SOUNDCARD_H
# include <sys/soundcard.h>
#elif defined HAVE_LINUX_SOUNDCARD_H
# include <linux/soundcard.h>
#elif HAVE_MACHINE_SOUNDCARD_H
# include <machine/soundcard.h>
#endif

#include <sys/asoundlib.h>

#ifndef MAP_FAILED
# define MAP_FAILED ((void*)-1)
#endif

extern int soundtime;
static int snd_inited;

static snd_pcm_t *pcm_handle;
static struct snd_pcm_channel_info cinfo;
static struct snd_pcm_channel_params params;
static struct snd_pcm_channel_setup setup;
static snd_pcm_mmap_control_t *mmap_control = NULL;
static char *mmap_data = NULL;
static int card=-1,dev=-1;

int check_card(int card)
{
	snd_ctl_t *handle;
	snd_ctl_hw_info_t info;
	int rc;

	if ((rc = snd_ctl_open(&handle, card)) < 0) {
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
			if ((rc=snd_pcm_open(&pcm_handle,card,dev,
								 SND_PCM_OPEN_PLAYBACK
								 | SND_PCM_OPEN_NONBLOCK))==0) {
				return 0;
			}
		}
	} else {
		if (dev>=0 && dev <info.pcmdevs) {
			if ((rc=snd_pcm_open(&pcm_handle,card,dev,
								 SND_PCM_OPEN_PLAYBACK
								 | SND_PCM_OPEN_NONBLOCK))==0) {
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
	int rate=-1,format=-1,bps,stereo=-1,frag_size;
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
			if ((rc=snd_pcm_open(&pcm_handle,card,dev,
								 SND_PCM_OPEN_PLAYBACK
								 | SND_PCM_OPEN_NONBLOCK))<0) {
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
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.channel = SND_PCM_CHANNEL_PLAYBACK;
	snd_pcm_channel_info(pcm_handle, &cinfo);
	Con_Printf("%08x %08x %08x\n",cinfo.flags,cinfo.formats,cinfo.rates);
	if ((rate==-1 || rate==44100) && cinfo.rates & SND_PCM_RATE_44100) {
		rate=44100;
		frag_size=512;	/* assuming stereo 8 bit */
	} else if ((rate==-1 || rate==22050) && cinfo.rates & SND_PCM_RATE_22050) {
		rate=22050;
		frag_size=256;	/* assuming stereo 8 bit */
	} else if ((rate==-1 || rate==11025) && cinfo.rates & SND_PCM_RATE_11025) {
		rate=11025;
		frag_size=128;	/* assuming stereo 8 bit */
	} else {
		Con_Printf("ALSA: desired rates not supported\n");
		goto error_2;
	}
	if ((format==-1 || format==SND_PCM_SFMT_S16_LE) && cinfo.formats & SND_PCM_FMT_S16_LE) {
		format=SND_PCM_SFMT_S16_LE;
		bps=16;
		frag_size*=2;
	} else if ((format==-1 || format==SND_PCM_SFMT_U8) && cinfo.formats & SND_PCM_FMT_U8) {
		format=SND_PCM_SFMT_U8;
		bps=8;
	} else {
		Con_Printf("ALSA: desired formats not supported\n");
		goto error_2;
	}
	if (stereo && cinfo.max_voices>=2) {
		stereo=1;
	} else {
		stereo=0;
		frag_size/=2;
	}

	err_msg="audio munmap";
	if ((rc=snd_pcm_munmap(pcm_handle, SND_PCM_CHANNEL_PLAYBACK))<0)
		goto error;

	memset(&params, 0, sizeof(params));
	params.channel = SND_PCM_CHANNEL_PLAYBACK;
	params.mode = SND_PCM_MODE_BLOCK;
	params.format.interleave=1;
	params.format.format=format;
	params.format.rate=rate;
	params.format.voices=stereo+1;
	params.start_mode = SND_PCM_START_GO;
	params.stop_mode = SND_PCM_STOP_ROLLOVER;
	params.buf.block.frag_size=frag_size;
	params.buf.block.frags_min=1;
	params.buf.block.frags_max=-1;
	err_msg="audio params";
	if ((rc=snd_pcm_channel_params(pcm_handle, &params))<0)
		goto error;

	err_msg="audio mmap";
	if ((rc=snd_pcm_mmap(pcm_handle, SND_PCM_CHANNEL_PLAYBACK, &mmap_control, (void **)&mmap_data))<0)
		goto error;
	err_msg="audio prepare";
	if ((rc=snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK))<0)
		goto error;

	memset(&setup, 0, sizeof(setup));
	setup.mode = SND_PCM_MODE_BLOCK;
	setup.channel = SND_PCM_CHANNEL_PLAYBACK;
	err_msg="audio setup";
	if ((rc=snd_pcm_channel_setup(pcm_handle, &setup))<0)
		goto error;

	shm=&sn;
	memset((dma_t*)shm,0,sizeof(*shm));
    shm->splitbuffer = 0;
	shm->channels=setup.format.voices;
	shm->submission_chunk=128;					// don't mix less than this #
	shm->samplepos=0;							// in mono samples
	shm->samplebits=setup.format.format==SND_PCM_SFMT_S16_LE?16:8;
	shm->samples=setup.buf.block.frags*setup.buf.block.frag_size/(shm->samplebits/8);	// mono samples in buffer
	shm->speed=setup.format.rate;
	shm->buffer=(unsigned char*)mmap_data;
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

int SNDDMA_GetDMAPos(void)
{
	if (!snd_inited) return 0;
	shm->samplepos=(mmap_control->status.frag_io+1)*setup.buf.block.frag_size/(shm->samplebits/8);
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
	int count=paintedtime-soundtime;
	int i,s,e;
	int rc;

	count+=setup.buf.block.frag_size-1;
	count/=setup.buf.block.frag_size;
	s=soundtime/setup.buf.block.frag_size;
	e=s+count;
	for (i=s; i<e; i++)
		mmap_control->fragments[i % setup.buf.block.frags].data=1;
	switch (mmap_control->status.status) {
	case SND_PCM_STATUS_PREPARED:
		if ((rc=snd_pcm_channel_go(pcm_handle, SND_PCM_CHANNEL_PLAYBACK))<0) {
			fprintf(stderr, "unable to start playback. %s\n",
					snd_strerror(rc));
			exit(1);
		}
		break;
	case SND_PCM_STATUS_RUNNING:
		break;
	case SND_PCM_STATUS_UNDERRUN:
		if ((rc=snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK))<0) {
			fprintf(stderr, "underrun: playback channel prepare error. %s\n",
					snd_strerror(rc));
			exit(1);
		}
		break;
	default:
		break;
	}
}

void *S_LockBuffer(void)
{
	return shm->buffer;
}

void S_UnlockBuffer(void)
{
}
