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


static portable_sampleframe_t paintbuffer[PAINTBUFFER_SIZE];
static portable_sampleframe_t paintbuffer_unswapped[PAINTBUFFER_SIZE];

extern speakerlayout_t snd_speakerlayout; // for querying the listeners

extern void SCR_CaptureVideo_SoundFrame(const portable_sampleframe_t *paintbuffer, size_t length);
static void S_CaptureAVISound(size_t length)
{
	size_t i;
	unsigned int j;

	if (!cls.capturevideo.active)
		return;

	// undo whatever swapping the channel layout (swapstereo, ALSA) did
	for(j = 0; j < snd_speakerlayout.channels; ++j)
	{
		unsigned int j0 = snd_speakerlayout.listeners[j].channel_unswapped;
		for(i = 0; i < length; ++i)
			paintbuffer_unswapped[i].sample[j0] = paintbuffer[i].sample[j];
	}

	SCR_CaptureVideo_SoundFrame(paintbuffer_unswapped, length);
}

static void S_ConvertPaintBuffer(const portable_sampleframe_t *painted_ptr, void *rb_ptr, int nbframes, int width, int channels)
{
	int i, val;
	if (width == 2)  // 16bit
	{
		short *snd_out = (short*)rb_ptr;
		if (channels == 8)  // 7.1 surround
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
		else if (channels == 6)  // 5.1 surround
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
		else if (channels == 4)  // 4.0 surround
		{
			for (i = 0; i < nbframes; i++, painted_ptr++)
			{
				*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
				*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
				*snd_out++ = bound(-32768, painted_ptr->sample[2], 32767);
				*snd_out++ = bound(-32768, painted_ptr->sample[3], 32767);
			}
		}
		else if (channels == 2)  // 2.0 stereo
		{
			for (i = 0; i < nbframes; i++, painted_ptr++)
			{
				*snd_out++ = bound(-32768, painted_ptr->sample[0], 32767);
				*snd_out++ = bound(-32768, painted_ptr->sample[1], 32767);
			}
		}
		else if (channels == 1)  // 1.0 mono
		{
			for (i = 0; i < nbframes; i++, painted_ptr++)
			{
				val = (painted_ptr->sample[0] + painted_ptr->sample[1]) >> 1;
				*snd_out++ = bound(-32768, val, 32767);
			}
		}

		// noise is really really annoying
		if (cls.timedemo)
			memset(rb_ptr, 0, nbframes * channels * width);
	}
	else  // 8bit
	{
		unsigned char *snd_out = (unsigned char*)rb_ptr;
		if (channels == 8)  // 7.1 surround
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
		else if (channels == 6)  // 5.1 surround
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
		else if (channels == 4)  // 4.0 surround
		{
			for (i = 0; i < nbframes; i++, painted_ptr++)
			{
				val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
				val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
				val = (painted_ptr->sample[2] >> 8) + 128; *snd_out++ = bound(0, val, 255);
				val = (painted_ptr->sample[3] >> 8) + 128; *snd_out++ = bound(0, val, 255);
			}
		}
		else if (channels == 2)  // 2.0 stereo
		{
			for (i = 0; i < nbframes; i++, painted_ptr++)
			{
				val = (painted_ptr->sample[0] >> 8) + 128; *snd_out++ = bound(0, val, 255);
				val = (painted_ptr->sample[1] >> 8) + 128; *snd_out++ = bound(0, val, 255);
			}
		}
		else if (channels == 1)  // 1.0 mono
		{
			for (i = 0;i < nbframes;i++, painted_ptr++)
			{
				val = ((painted_ptr->sample[0] + painted_ptr->sample[1]) >> 9) + 128;
				*snd_out++ = bound(0, val, 255);
			}
		}

		// noise is really really annoying
		if (cls.timedemo)
			memset(rb_ptr, 128, nbframes * channels);
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

static qboolean SND_PaintChannel (channel_t *ch, portable_sampleframe_t *paint, unsigned int count)
{
	int vol[SND_LISTENERS];
	const snd_buffer_t *sb;
	unsigned int i, sb_offset;
	sfx_t *sfx;

	sfx = ch->sfx; // fetch the volatile variable
	if (!sfx) // given that this is called by the mixer thread, this never happens, but...
		return false;

	// move to the stack (do we need to?)
	for (i = 0;i < SND_LISTENERS;i++)
		vol[i] = ch->listener_volume[i];

	// if volumes are all zero, just return
	for (i = 0;i < SND_LISTENERS;i++)
		if (vol[i])
			break;
	if (i == SND_LISTENERS)
		return false;

	sb_offset = ch->pos;
	sb = sfx->fetcher->getsb (sfx->fetcher_data, &ch->fetcher_data, &sb_offset, count);
	if (sb == NULL)
	{
		Con_DPrintf("SND_PaintChannel: ERROR: can't get sound buffer from sfx \"%s\"\n",
					sfx->name); // , count); // or add this? FIXME
		return false;
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
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 8;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paint[i].sample[3] += (samples[1] * vol[3]) >> 8;
						paint[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 9;
						paint[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 9;
						paint[i].sample[6] += (samples[0] * vol[6]) >> 8;
						paint[i].sample[7] += (samples[1] * vol[7]) >> 8;
						samples += 2;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 8;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paint[i].sample[3] += (samples[1] * vol[3]) >> 8;
						paint[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 9;
						paint[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 9;
						samples += 2;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 8;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paint[i].sample[3] += (samples[1] * vol[3]) >> 8;
						samples += 2;
					}
				}
				else if (vol[0] + vol[1] > 0 && ch->prologic_invert == -1)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] -= (samples[1] * vol[1]) >> 8;
						samples += 2;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 8;
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
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 8;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paint[i].sample[3] += (samples[0] * vol[3]) >> 8;
						paint[i].sample[4] += (samples[0] * vol[4]) >> 8;
						paint[i].sample[5] += (samples[0] * vol[5]) >> 8;
						paint[i].sample[6] += (samples[0] * vol[6]) >> 8;
						paint[i].sample[7] += (samples[0] * vol[7]) >> 8;
						samples += 1;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 8;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paint[i].sample[3] += (samples[0] * vol[3]) >> 8;
						paint[i].sample[4] += (samples[0] * vol[4]) >> 8;
						paint[i].sample[5] += (samples[0] * vol[5]) >> 8;
						samples += 1;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 8;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 8;
						paint[i].sample[3] += (samples[0] * vol[3]) >> 8;
						samples += 1;
					}
				}
				else if (vol[0] + vol[1] > 0 && ch->prologic_invert == -1)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] -= (samples[0] * vol[1]) >> 8;
						samples += 1;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 8;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 8;
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
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 16;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paint[i].sample[3] += (samples[1] * vol[3]) >> 16;
						paint[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 17;
						paint[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 17;
						paint[i].sample[6] += (samples[0] * vol[6]) >> 16;
						paint[i].sample[7] += (samples[1] * vol[7]) >> 16;
						samples += 2;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 16;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paint[i].sample[3] += (samples[1] * vol[3]) >> 16;
						paint[i].sample[4] += ((samples[0] + samples[1]) * vol[4]) >> 17;
						paint[i].sample[5] += ((samples[0] + samples[1]) * vol[5]) >> 17;
						samples += 2;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 16;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paint[i].sample[3] += (samples[1] * vol[3]) >> 16;
						samples += 2;
					}
				}
				else if (vol[0] + vol[1] > 0 && ch->prologic_invert == -1)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] -= (samples[1] * vol[1]) >> 16;
						samples += 2;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[1] * vol[1]) >> 16;
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
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 16;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paint[i].sample[3] += (samples[0] * vol[3]) >> 16;
						paint[i].sample[4] += (samples[0] * vol[4]) >> 16;
						paint[i].sample[5] += (samples[0] * vol[5]) >> 16;
						paint[i].sample[6] += (samples[0] * vol[6]) >> 16;
						paint[i].sample[7] += (samples[0] * vol[7]) >> 16;
						samples += 1;
					}
				}
				else if (vol[4] + vol[5] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 16;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paint[i].sample[3] += (samples[0] * vol[3]) >> 16;
						paint[i].sample[4] += (samples[0] * vol[4]) >> 16;
						paint[i].sample[5] += (samples[0] * vol[5]) >> 16;
						samples += 1;
					}
				}
				else if (vol[2] + vol[3] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 16;
						paint[i].sample[2] += (samples[0] * vol[2]) >> 16;
						paint[i].sample[3] += (samples[0] * vol[3]) >> 16;
						samples += 1;
					}
				}
				else if (vol[0] + vol[1] > 0 && ch->prologic_invert == -1)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] -= (samples[0] * vol[1]) >> 16;
						samples += 1;
					}
				}
				else if (vol[0] + vol[1] > 0)
				{
					for (i = 0;i < count;i++)
					{
						paint[i].sample[0] += (samples[0] * vol[0]) >> 16;
						paint[i].sample[1] += (samples[0] * vol[1]) >> 16;
						samples += 1;
					}
				}
			}
			else
				return false; // unsupported number of channels in sound
		}
	}
	return true;
}

void S_MixToBuffer(void *stream, unsigned int bufferframes)
{
	unsigned int i;
	channel_t *ch;
	unsigned int frames;
	unsigned char *outbytes = (unsigned char *) stream;

	// mix as many times as needed to fill the requested buffer
	while (bufferframes)
	{
		// limit to the size of the paint buffer
		frames = min(bufferframes, PAINTBUFFER_SIZE);

		// clear the paint buffer
		memset (paintbuffer, 0, frames * sizeof (paintbuffer[0]));

		// paint in the channels.
		// channels with zero volumes still advance in time but don't paint.
		ch = channels;
		for (i = 0; i < total_channels ; i++, ch++)
		{
			sfx_t *sfx;
			int ltime;
			int count;

			sfx = ch->sfx;
			if (sfx == NULL)
				continue;
			if (!S_LoadSound (sfx, true))
				continue;
			if (ch->flags & CHANNELFLAG_PAUSED)
				continue;
			if (!sfx->total_length)
				continue;
			if (sfx->total_length > 1<<30)
				Sys_Error("S_MixToBuffer: sfx corrupt\n");

			ltime = 0;
			if (ch->pos < 0)
			{
				count = -ch->pos;
				count = min(count, (int)frames - ltime);
				ch->pos += count;
				ltime += count;
			}

			while (ltime < (int)frames)
			{
				// paint up to end of buffer or of input, whichever is lower
				count = sfx->total_length - ch->pos;
				count = bound(0, count, (int)frames - ltime);
				// mix the remaining samples
				if (count)
				{
					SND_PaintChannel (ch, paintbuffer + ltime, count);
					ch->pos += count;
					ltime += count;
				}
				// if at end of sfx, loop or stop the channel
				else
				{
					if (sfx->loopstart < sfx->total_length)
						ch->pos = sfx->loopstart;
					else if (ch->flags & CHANNELFLAG_FORCELOOP)
						ch->pos = 0;
					else
					{
						S_StopChannel (ch - channels, false, false);
						break;
					}
				}
			}
		}

		if (!snd_usethreadedmixing)
			S_CaptureAVISound(frames);

		S_ConvertPaintBuffer(paintbuffer, outbytes, frames, snd_renderbuffer->format.width, snd_renderbuffer->format.channels);

		// advance the output pointer
		outbytes += frames * snd_renderbuffer->format.width * snd_renderbuffer->format.channels;
		bufferframes -= frames;
	}
}
