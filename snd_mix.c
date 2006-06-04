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
#include "snd_main.h"


typedef struct portable_samplepair_s
{
	int sample[SND_LISTENERS];
} portable_sampleframe_t;

// LordHavoc: was 512, expanded to 2048
#define	PAINTBUFFER_SIZE 2048
portable_sampleframe_t paintbuffer[PAINTBUFFER_SIZE];


// FIXME: this desyncs with the video too easily
extern void SCR_CaptureVideo_SoundFrame(unsigned char *bufstereo16le, size_t length, int rate);
static void S_CaptureAVISound(size_t length)
{
	size_t i;
	unsigned char out[PAINTBUFFER_SIZE * 4];
	unsigned char* out_ptr;

	if (!cls.capturevideo_active)
		return;

	// write the sound buffer as little endian 16bit interleaved stereo
	for(i = 0, out_ptr = out; i < length; i++, out_ptr += 4)
	{
		int n0, n1;

		n0 = paintbuffer[i].sample[0];
		n0 = bound(-32768, n0, 32767);
		out_ptr[0] = (unsigned char)n0;
		out_ptr[1] = (unsigned char)(n0 >> 8);

		n1 = paintbuffer[i].sample[1];
		n1 = bound(-32768, n1, 32767);
		out_ptr[2] = (unsigned char)n1;
		out_ptr[3] = (unsigned char)(n1 >> 8);
	}
	SCR_CaptureVideo_SoundFrame(out, length, snd_renderbuffer->format.speed);
}

static unsigned int S_TransferPaintBuffer(snd_ringbuffer_t* rb, unsigned int starttime, unsigned int endtime)
{
	unsigned int partialend;

	// Lock submitbuffer
	if (!simsound && !SndSys_LockRenderBuffer())
		return 0;
	
	partialend = starttime;
	while (partialend < endtime)  // handle recirculating buffer issues
	{
		unsigned int startoffset, maxframes, nbframes, i;
		void *rb_ptr;
		portable_sampleframe_t *painted_ptr;
		int val;

		startoffset = partialend % rb->maxframes;
		maxframes = rb->maxframes - startoffset;
		nbframes = endtime - partialend;
		if (nbframes > maxframes)
			nbframes = maxframes;

		rb_ptr = &rb->ring[startoffset * rb->format.width * rb->format.channels];
		painted_ptr = &paintbuffer[partialend - starttime];

		if (rb->format.width == 2)  // 16bit
		{
			short *snd_out = (short*)rb_ptr;
			if (rb->format.channels == 8)  // 7.1 surround
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[4], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[5], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[6], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[7], 32767);
					}
				}
				else
				{
					for (i = 0;i < nbframes;i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[4], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[5], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[6], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[7], 32767);
					}
				}
			}
			else if (rb->format.channels == 6)  // 5.1 surround
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[4], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[5], 32767);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[4], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[5], 32767);
					}
				}
			}
			else if (rb->format.channels == 4)  // 4.0 surround
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
					}
				}
			}
			else if (rb->format.channels == 2)  // 2.0 stereo
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
						*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
					}
				}
			}
			else if (rb->format.channels == 1)  // 1.0 mono
			{
				for (i = 0; i < nbframes; i++, painted_ptr++)
				{
					val = (painted_ptr->sample[0] + painted_ptr->sample[1]) >> 1;
					*snd_out++ = bound(-32768, val, 32767);
				}
			}
		}
		else  // 8bit
		{
			unsigned char *snd_out = (unsigned char*)rb_ptr;
			if (rb->format.channels == 8)  // 7.1 surround
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[4] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[5] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[6] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[7] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[4] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[5] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[6] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[7] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
			}
			else if (rb->format.channels == 6)  // 5.1 surround
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[4] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[5] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[4] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[5] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
			}
			else if (rb->format.channels == 4)  // 4.0 surround
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
			}
			else if (rb->format.channels == 2)  // 2.0 stereo
			{
				if (snd_swapstereo.value)
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
				else
				{
					for (i = 0; i < nbframes; i++, painted_ptr++)
					{
						val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
						val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
					}
				}
			}
			else if (rb->format.channels == 1)  // 1.0 mono
			{
				for (i = 0;i < nbframes;i++, painted_ptr++)
				{
					val = ((painted_ptr->sample[0] + painted_ptr->sample[1]) >> 9) + 128;
					*snd_out++ = bound(0, val, 255);
				}
			}
		}

		partialend += nbframes;
	}

	rb->endframe = endtime;

	// Remove outdated samples from the ring buffer, if any
	if (rb->startframe < soundtime)
		rb->startframe = soundtime;

	if (!simsound)
		SndSys_UnlockRenderBuffer();

	return endtime - starttime;
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

static qboolean SND_PaintChannel (channel_t *ch, unsigned int count)
{
	int snd_vol, vol[SND_LISTENERS];
	const snd_buffer_t *sb;
	unsigned int i, sb_offset;

	// If this channel manages its own volume
	if (ch->flags & CHANNELFLAG_FULLVOLUME)
		snd_vol = 256;
	else
		snd_vol = (int)(volume.value * 256);

	for (i = 0;i < SND_LISTENERS;i++)
		vol[i] = ch->listener_volume[i] * snd_vol;

	sb_offset = ch->pos;
	sb = ch->sfx->fetcher->getsb (ch, &sb_offset, count);
	if (sb == NULL)
	{
		Con_DPrintf("SND_PaintChannel: ERROR: can't get sound buffer from sfx \"%s\"\n",
					ch->sfx->name, count);
	}
	else
	{
#if SND_LISTENERS != 8
#		error the following code only supports up to 8 channels, update it
#endif
		if (sb->format.width == 1)
		{
			const signed char *samples = (signed char*)sb->samples + (ch->pos - sb_offset) * sb->format.channels;

			// Stereo sound support
			if (sb->format.channels == 2)
			{
				if (vol[6] + vol[7] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 8;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paintbuffer[i].sample[3] += (samples[1] * vol[3]) >> 8;
						paintbuffer[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 9;
						paintbuffer[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 9;
						paintbuffer[i].sample[6] += (samples[0] * vol[6]) >> 8;
						paintbuffer[i].sample[7] += (samples[1] * vol[7]) >> 8;
						samples += 2;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 8;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paintbuffer[i].sample[3] += (samples[1] * vol[3]) >> 8;
						paintbuffer[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 9;
						paintbuffer[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 9;
						samples += 2;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 8;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paintbuffer[i].sample[3] += (samples[1] * vol[3]) >> 8;
						samples += 2;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 8;
						samples += 2;
					}
				}
			}
			else if (sb->format.channels == 1)
			{
				if (vol[6] + vol[7] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 8;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paintbuffer[i].sample[3] += (samples[0] * vol[3]) >> 8;
						paintbuffer[i].sample[4] += (samples[0] * vol[4]) >> 8;
						paintbuffer[i].sample[5] += (samples[0] * vol[5]) >> 8;
						paintbuffer[i].sample[6] += (samples[0] * vol[6]) >> 8;
						paintbuffer[i].sample[7] += (samples[0] * vol[7]) >> 8;
						samples += 1;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 8;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paintbuffer[i].sample[3] += (samples[0] * vol[3]) >> 8;
						paintbuffer[i].sample[4] += (samples[0] * vol[4]) >> 8;
						paintbuffer[i].sample[5] += (samples[0] * vol[5]) >> 8;
						samples += 1;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 8;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paintbuffer[i].sample[3] += (samples[0] * vol[3]) >> 8;
						samples += 1;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 8;
						samples += 1;
					}
				}
			}
			else
				return false; // unsupported number of channels in sound
		}
		else if (sb->format.width == 2)
		{
			const signed short *samples = (signed short*)sb->samples + (ch->pos - sb_offset) * sb->format.channels;

			// Stereo sound support
			if (sb->format.channels == 2)
			{
				if (vol[6] + vol[7] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 16;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paintbuffer[i].sample[3] += (samples[1] * vol[3]) >> 16;
						paintbuffer[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 17;
						paintbuffer[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 17;
						paintbuffer[i].sample[6] += (samples[0] * vol[6]) >> 16;
						paintbuffer[i].sample[7] += (samples[1] * vol[7]) >> 16;
						samples += 2;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 16;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paintbuffer[i].sample[3] += (samples[1] * vol[3]) >> 16;
						paintbuffer[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 17;
						paintbuffer[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 17;
						samples += 2;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 16;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paintbuffer[i].sample[3] += (samples[1] * vol[3]) >> 16;
						samples += 2;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[1] * vol[1]) >> 16;
						samples += 2;
					}
				}
			}
			else if (sb->format.channels == 1)
			{
				if (vol[6] + vol[7] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 16;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paintbuffer[i].sample[3] += (samples[0] * vol[3]) >> 16;
						paintbuffer[i].sample[4] += (samples[0] * vol[4]) >> 16;
						paintbuffer[i].sample[5] += (samples[0] * vol[5]) >> 16;
						paintbuffer[i].sample[6] += (samples[0] * vol[6]) >> 16;
						paintbuffer[i].sample[7] += (samples[0] * vol[7]) >> 16;
						samples += 1;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 16;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paintbuffer[i].sample[3] += (samples[0] * vol[3]) >> 16;
						paintbuffer[i].sample[4] += (samples[0] * vol[4]) >> 16;
						paintbuffer[i].sample[5] += (samples[0] * vol[5]) >> 16;
						samples += 1;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 16;
						paintbuffer[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paintbuffer[i].sample[3] += (samples[0] * vol[3]) >> 16;
						samples += 1;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paintbuffer[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paintbuffer[i].sample[1] += (samples[0] * vol[1]) >> 16;
						samples += 1;
					}
				}
			}
			else
				return false; // unsupported number of channels in sound
		}
	}

	ch->pos += count;
	return true;
}

void S_PaintChannels (snd_ringbuffer_t* rb, unsigned int starttime, unsigned int endtime)
{
	unsigned int paintedtime;

	paintedtime = starttime;
	while (paintedtime < endtime)
	{
		unsigned int partialend, i, framecount;
		channel_t *ch;

		// if paintbuffer is too small
		if (endtime > paintedtime + PAINTBUFFER_SIZE)
			partialend = paintedtime + PAINTBUFFER_SIZE;
		else
			partialend = endtime;

		// clear the paint buffer
		memset (paintbuffer, 0, (partialend - paintedtime) * sizeof (paintbuffer[0]));

		// paint in the channels.
		ch = channels;
		for (i = 0; i < total_channels ; i++, ch++)
		{
			sfx_t *sfx;
			unsigned int ltime, j;

			sfx = ch->sfx;
			if (sfx == NULL)
				continue;
			for (j = 0;j < SND_LISTENERS;j++)
				if (ch->listener_volume[j])
					break;
			if (j == SND_LISTENERS)
				continue;
			if (!S_LoadSound (sfx, true))
				continue;

			// if the channel is paused
			if (ch->flags & CHANNELFLAG_PAUSED)
			{
				int pausedtime = partialend - paintedtime;
				ch->lastptime += pausedtime;
				ch->end += pausedtime;
				continue;
			}

			// if the sound hasn't been painted last time, update his position
			if (ch->lastptime < paintedtime)
			{
				ch->pos += paintedtime - ch->lastptime;

				// If the sound should have ended by then
				if ((unsigned int)ch->pos > sfx->total_length)
				{
					int loopstart;

					if (sfx->loopstart >= 0)
						loopstart = bound(0, sfx->loopstart, (int)sfx->total_length - 1);
					else
					{
						if (ch->flags & CHANNELFLAG_FORCELOOP)
							loopstart = 0;
						else
							loopstart = -1;
					}

					// If the sound is looped
					if (loopstart >= 0)
						ch->pos = (ch->pos - sfx->total_length) % (sfx->total_length - loopstart) + loopstart;
					else
						ch->pos = sfx->total_length;
					ch->end = paintedtime + sfx->total_length - ch->pos;
				}
			}

			ltime = paintedtime;
			while (ltime < partialend)
			{
				int count;
				qboolean stop_paint;

				// paint up to end
				if (ch->end < partialend)
					count = ch->end - ltime;
				else
					count = partialend - ltime;

				if (count > 0)
				{
					for (j = 0; j < SND_LISTENERS; j++)
						ch->listener_volume[j] = bound(0, ch->listener_volume[j], 255);

					stop_paint = !SND_PaintChannel (ch, (unsigned int)count);
					if (!stop_paint)
					{
						ltime += count;
						ch->lastptime = ltime;
					}
				}
				else
					stop_paint = false;

				if (ltime >= ch->end)
				{
					// if at end of loop, restart
					if ((sfx->loopstart >= 0 || (ch->flags & CHANNELFLAG_FORCELOOP)) && !stop_paint)
					{
						ch->pos = bound(0, sfx->loopstart, (int)sfx->total_length - 1);
						ch->end = ltime + sfx->total_length - ch->pos;
					}
					// channel just stopped
					else
						stop_paint = true;
				}

				if (stop_paint)
				{
					S_StopChannel (ch - channels);
					break;
				}
			}
		}

		S_CaptureAVISound (partialend - paintedtime);
		framecount = S_TransferPaintBuffer (rb, paintedtime, partialend);
		paintedtime += framecount;

		// If there was not enough free space in the sound buffer, stop here
		if (paintedtime != partialend)
		{
			Con_DPrintf(">> S_PaintChannels: Not enough free space in the sound buffer ( %u != %u)\n", paintedtime, partialend);
			break;
		}
	}
}
