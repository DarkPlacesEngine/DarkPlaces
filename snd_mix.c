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
// snd_mix.c -- portable code to mix sounds

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
void S_CaptureAVISound(portable_sampleframe_t *buf, size_t length)
{
	int n;
	size_t i;
	unsigned char out[PAINTBUFFER_SIZE * 4];
	if (!cls.capturevideo_active)
		return;
	// write the sound buffer as little endian 16bit interleaved stereo
	for(i = 0;i < length;i++)
	{
		n = buf[i].sample[0];
		n = bound(-32768, n, 32767);
		out[i*4+0] = n & 0xFF;
		out[i*4+1] = (n >> 8) & 0xFF;
		n = buf[i].sample[1];
		n = bound(-32768, n, 32767);
		out[i*4+2] = n & 0xFF;
		out[i*4+3] = (n >> 8) & 0xFF;
	}
	SCR_CaptureVideo_SoundFrame(out, length, shm->format.speed);
}

// TODO: rewrite this function
void S_TransferPaintBuffer(int endtime)
{
	void *pbuf;
	int i;
	portable_sampleframe_t *snd_p;
	int lpaintedtime;
	int snd_frames;
	int val;
	if ((pbuf = S_LockBuffer()))
	{
		snd_p = paintbuffer;
		lpaintedtime = paintedtime;
		for (lpaintedtime = paintedtime;lpaintedtime < endtime;lpaintedtime += snd_frames)
		{
			// handle recirculating buffer issues
			i = lpaintedtime & (shm->sampleframes - 1);
			snd_frames = shm->sampleframes - i;
			if (snd_frames > endtime - lpaintedtime)
				snd_frames = endtime - lpaintedtime;
			if (shm->format.width == 2)
			{
				// 16bit
				short *snd_out = (short *) pbuf + i * shm->format.channels;
				if (shm->format.channels == 8)
				{
					// 7.1 surround
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[3], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[2], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[4], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[5], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[6], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[7], 32767);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[2], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[3], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[4], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[5], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[6], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[7], 32767);
						}
					}
				}
				else if (shm->format.channels == 6)
				{
					// 5.1 surround
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[3], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[2], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[4], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[5], 32767);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[2], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[3], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[4], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[5], 32767);
						}
					}
				}
				else if (shm->format.channels == 4)
				{
					// 4.0 surround
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[3], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[2], 32767);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[2], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[3], 32767);
						}
					}
				}
				else if (shm->format.channels == 2)
				{
					// 2.0 stereo
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							*snd_out++ = bound(-32768, snd_p->sample[0], 32767);
							*snd_out++ = bound(-32768, snd_p->sample[1], 32767);
						}
					}
				}
				else if (shm->format.channels == 1)
				{
					// 1.0 mono
					for (i = 0;i < snd_frames;i++, snd_p++)
						*snd_out++ = bound(-32768, (snd_p->sample[0] + snd_p->sample[1]) >> 1, 32767);
				}
			}
			else
			{
				// 8bit
				unsigned char *snd_out = (unsigned char *) pbuf + i * shm->format.channels;
				if (shm->format.channels == 8)
				{
					// 7.1 surround
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[3] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[2] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[4] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[5] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[6] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[7] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[2] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[3] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[4] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[5] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[6] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[7] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
				}
				else if (shm->format.channels == 6)
				{
					// 5.1 surround
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[3] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[2] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[4] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[5] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[2] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[3] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[4] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[5] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
				}
				else if (shm->format.channels == 4)
				{
					// 4.0 surround
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[3] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[2] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[2] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[3] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
				}
				else if (shm->format.channels == 2)
				{
					// 2.0 stereo
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
					else
					{
						for (i = 0;i < snd_frames;i++, snd_p++)
						{
							val = (snd_p->sample[0] >> 8) + 128;*snd_out++ = bound(0, val, 255);
							val = (snd_p->sample[1] >> 8) + 128;*snd_out++ = bound(0, val, 255);
						}
					}
				}
				else if (shm->format.channels == 1)
				{
					// 1.0 mono
					for (i = 0;i < snd_frames;i++, snd_p++)
					{
						val = ((snd_p->sample[0]+snd_p->sample[1]) >> 9) + 128;*snd_out++ = bound(0, val, 255);
					}
				}
			}
		}
		S_UnlockBuffer();
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

qboolean SND_PaintChannel (channel_t *ch, int endtime);

void S_PaintChannels(int endtime)
{
	unsigned int i, j;
	int end;
	channel_t *ch;
	sfx_t *sfx;
	int ltime, count;

	while (paintedtime < endtime)
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		memset (&paintbuffer, 0, (end - paintedtime) * sizeof (paintbuffer[0]));

		// paint in the channels.
		ch = channels;
		for (i=0; i<total_channels ; i++, ch++)
		{
			sfx = ch->sfx;
			if (!sfx)
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
				int pausedtime = end - paintedtime;
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

					if (ch->flags & CHANNELFLAG_FORCELOOP)
						loopstart = 0;
					else
						loopstart = -1;
					if (sfx->loopstart >= 0)
						loopstart = sfx->loopstart;

					// If the sound is looped
					if (loopstart >= 0)
						ch->pos = (ch->pos - sfx->total_length) % (sfx->total_length - loopstart) + loopstart;
					else
						ch->pos = sfx->total_length;
					ch->end = paintedtime + sfx->total_length - ch->pos;
				}
			}

			ltime = paintedtime;
			while (ltime < end)
			{
				qboolean stop_paint;

				// paint up to end
				if (ch->end < end)
					count = (int)ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{
					for (j = 0;j < SND_LISTENERS;j++)
						ch->listener_volume[j] = bound(0, ch->listener_volume[j], 255);

					stop_paint = !SND_PaintChannel (ch, count);

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

		// transfer out according to DMA format
		S_CaptureAVISound (paintbuffer, end - paintedtime);
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}


qboolean SND_PaintChannel (channel_t *ch, int count)
{
	int snd_vol, vol[SND_LISTENERS];
	const sfxbuffer_t *sb;
	int i;

	// If this channel manages its own volume
	if (ch->flags & CHANNELFLAG_FULLVOLUME)
		snd_vol = 256;
	else
		snd_vol = volume.value * 256;

	for (i = 0;i < SND_LISTENERS;i++)
		vol[i] = ch->listener_volume[i] * snd_vol;

	sb = ch->sfx->fetcher->getsb (ch, ch->pos, count);
	if (sb == NULL)
		return false;

#if SND_LISTENERS != 8
#error this code only supports up to 8 channels, update it
#endif

	if (ch->sfx->format.width == 1)
	{
		const signed char *sfx = (signed char *)sb->data + (ch->pos - sb->offset) * ch->sfx->format.channels;
		// Stereo sound support
		if (ch->sfx->format.channels == 2)
		{
			if (vol[6] + vol[7] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 8;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 8;
					paintbuffer[i].sample[3] += (sfx[1] * vol[3]) >> 8;
					paintbuffer[i].sample[4] += ((sfx[0]+sfx[1]) * vol[4]) >> 9;
					paintbuffer[i].sample[5] += ((sfx[0]+sfx[1]) * vol[5]) >> 9;
					paintbuffer[i].sample[6] += (sfx[0] * vol[6]) >> 8;
					paintbuffer[i].sample[7] += (sfx[1] * vol[7]) >> 8;
					sfx += 2;
				}
			}
			else if (vol[4] + vol[5] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 8;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 8;
					paintbuffer[i].sample[3] += (sfx[1] * vol[3]) >> 8;
					paintbuffer[i].sample[4] += ((sfx[0]+sfx[1]) * vol[4]) >> 9;
					paintbuffer[i].sample[5] += ((sfx[0]+sfx[1]) * vol[5]) >> 9;
					sfx += 2;
				}
			}
			else if (vol[2] + vol[3] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 8;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 8;
					paintbuffer[i].sample[3] += (sfx[1] * vol[3]) >> 8;
					sfx += 2;
				}
			}
			else if (vol[0] + vol[1] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 8;
					sfx += 2;
				}
			}
		}
		else if (ch->sfx->format.channels == 1)
		{
			if (vol[6] + vol[7] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 8;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 8;
					paintbuffer[i].sample[3] += (sfx[0] * vol[3]) >> 8;
					paintbuffer[i].sample[4] += (sfx[0] * vol[4]) >> 8;
					paintbuffer[i].sample[5] += (sfx[0] * vol[5]) >> 8;
					paintbuffer[i].sample[6] += (sfx[0] * vol[6]) >> 8;
					paintbuffer[i].sample[7] += (sfx[0] * vol[7]) >> 8;
					sfx += 1;
				}
			}
			else if (vol[4] + vol[5] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 8;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 8;
					paintbuffer[i].sample[3] += (sfx[0] * vol[3]) >> 8;
					paintbuffer[i].sample[4] += (sfx[0] * vol[4]) >> 8;
					paintbuffer[i].sample[5] += (sfx[0] * vol[5]) >> 8;
					sfx += 1;
				}
			}
			else if (vol[2] + vol[3] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 8;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 8;
					paintbuffer[i].sample[3] += (sfx[0] * vol[3]) >> 8;
					sfx += 1;
				}
			}
			else if (vol[0] + vol[1] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 8;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 8;
					sfx += 1;
				}
			}
		}
		else
			return true; // unsupported number of channels in sound
	}
	else if (ch->sfx->format.width == 2)
	{
		const signed short *sfx = (signed short *)sb->data + (ch->pos - sb->offset) * ch->sfx->format.channels;
		// Stereo sound support
		if (ch->sfx->format.channels == 2)
		{
			if (vol[6] + vol[7] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 16;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 16;
					paintbuffer[i].sample[3] += (sfx[1] * vol[3]) >> 16;
					paintbuffer[i].sample[4] += ((sfx[0]+sfx[1]) * vol[4]) >> 17;
					paintbuffer[i].sample[5] += ((sfx[0]+sfx[1]) * vol[5]) >> 17;
					paintbuffer[i].sample[6] += (sfx[0] * vol[6]) >> 16;
					paintbuffer[i].sample[7] += (sfx[1] * vol[7]) >> 16;
					sfx += 2;
				}
			}
			else if (vol[4] + vol[5] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 16;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 16;
					paintbuffer[i].sample[3] += (sfx[1] * vol[3]) >> 16;
					paintbuffer[i].sample[4] += ((sfx[0]+sfx[1]) * vol[4]) >> 17;
					paintbuffer[i].sample[5] += ((sfx[0]+sfx[1]) * vol[5]) >> 17;
					sfx += 2;
				}
			}
			else if (vol[2] + vol[3] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 16;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 16;
					paintbuffer[i].sample[3] += (sfx[1] * vol[3]) >> 16;
					sfx += 2;
				}
			}
			else if (vol[0] + vol[1] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[1] * vol[1]) >> 16;
					sfx += 2;
				}
			}
		}
		else if (ch->sfx->format.channels == 1)
		{
			if (vol[6] + vol[7] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 16;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 16;
					paintbuffer[i].sample[3] += (sfx[0] * vol[3]) >> 16;
					paintbuffer[i].sample[4] += (sfx[0] * vol[4]) >> 16;
					paintbuffer[i].sample[5] += (sfx[0] * vol[5]) >> 16;
					paintbuffer[i].sample[6] += (sfx[0] * vol[6]) >> 16;
					paintbuffer[i].sample[7] += (sfx[0] * vol[7]) >> 16;
					sfx += 1;
				}
			}
			else if (vol[4] + vol[5] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 16;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 16;
					paintbuffer[i].sample[3] += (sfx[0] * vol[3]) >> 16;
					paintbuffer[i].sample[4] += (sfx[0] * vol[4]) >> 16;
					paintbuffer[i].sample[5] += (sfx[0] * vol[5]) >> 16;
					sfx += 1;
				}
			}
			else if (vol[2] + vol[3] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 16;
					paintbuffer[i].sample[2] += (sfx[0] * vol[2]) >> 16;
					paintbuffer[i].sample[3] += (sfx[0] * vol[3]) >> 16;
					sfx += 1;
				}
			}
			else if (vol[0] + vol[1] > 0)
			{
				for (i = 0;i < count;i++)
				{
					paintbuffer[i].sample[0] += (sfx[0] * vol[0]) >> 16;
					paintbuffer[i].sample[1] += (sfx[0] * vol[1]) >> 16;
					sfx += 1;
				}
			}
		}
		else
			return true; // unsupported number of channels in sound
	}
	ch->pos += count;
	return true;
}

