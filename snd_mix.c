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

typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

// LordHavoc: was 512, expanded to 2048
#define	PAINTBUFFER_SIZE 2048
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];

// FIXME: this desyncs with the video too easily
extern qboolean cl_capturevideo_active;
extern void SCR_CaptureVideo_SoundFrame(qbyte *bufstereo16le, size_t length, int rate);
void S_CaptureAVISound(portable_samplepair_t *buf, size_t length)
{
	int n;
	size_t i;
	qbyte out[PAINTBUFFER_SIZE * 4];
	if (!cl_capturevideo_active)
		return;
	// write the sound buffer as little endian 16bit interleaved stereo
	for(i = 0;i < length;i++)
	{
		n = buf[i].left;
		n = bound(-32768, n, 32767);
		out[i*4+0] = n & 0xFF;
		out[i*4+1] = (n >> 8) & 0xFF;
		n = buf[i].right;
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
	if ((pbuf = S_LockBuffer()))
	{
		int i;
		int *snd_p;
		int lpaintedtime;
		int snd_linear_count;
		int val;
		snd_p = (int *) paintbuffer;
		lpaintedtime = paintedtime;
		if (shm->format.width == 2)
		{
			// 16bit
			short *snd_out;
			if (shm->format.channels == 2)
			{
				// 16bit 2 channels (stereo)
				while (lpaintedtime < endtime)
				{
					// handle recirculating buffer issues
					i = lpaintedtime & ((shm->samples >> 1) - 1);
					snd_out = (short *) pbuf + (i << 1);
					snd_linear_count = (shm->samples >> 1) - i;
					if (snd_linear_count > endtime - lpaintedtime)
						snd_linear_count = endtime - lpaintedtime;
					snd_linear_count <<= 1;
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_linear_count;i += 2)
						{
							snd_out[i    ] = bound(-32768, snd_p[i + 1], 32767);
							snd_out[i + 1] = bound(-32768, snd_p[i    ], 32767);
						}
					}
					else
					{
						for (i = 0;i < snd_linear_count;i += 2)
						{
							snd_out[i    ] = bound(-32768, snd_p[i    ], 32767);
							snd_out[i + 1] = bound(-32768, snd_p[i + 1], 32767);
						}
					}
					snd_p += snd_linear_count;
					lpaintedtime += (snd_linear_count >> 1);
				}
			}
			else
			{
				// 16bit 1 channel (mono)
				while (lpaintedtime < endtime)
				{
					// handle recirculating buffer issues
					i = lpaintedtime & (shm->samples - 1);
					snd_out = (short *) pbuf + i;
					snd_linear_count = shm->samples - i;
					if (snd_linear_count > endtime - lpaintedtime)
						snd_linear_count = endtime - lpaintedtime;
					for (i = 0;i < snd_linear_count;i++)
					{
						val = (snd_p[i * 2 + 0] + snd_p[i * 2 + 1]) >> 1;
						snd_out[i] = bound(-32768, val, 32767);
					}
					snd_p += snd_linear_count << 1;
					lpaintedtime += snd_linear_count;
				}
			}
		}
		else
		{
			// 8bit
			unsigned char *snd_out;
			if (shm->format.channels == 2)
			{
				// 8bit 2 channels (stereo)
				while (lpaintedtime < endtime)
				{
					// handle recirculating buffer issues
					i = lpaintedtime & ((shm->samples >> 1) - 1);
					snd_out = (unsigned char *) pbuf + (i << 1);
					snd_linear_count = (shm->samples >> 1) - i;
					if (snd_linear_count > endtime - lpaintedtime)
						snd_linear_count = endtime - lpaintedtime;
					snd_linear_count <<= 1;
					if (snd_swapstereo.value)
					{
						for (i = 0;i < snd_linear_count;i += 2)
						{
							val = (snd_p[i + 1] >> 8) + 128;
							snd_out[i    ] = bound(0, val, 255);
							val = (snd_p[i    ] >> 8) + 128;
							snd_out[i + 1] = bound(0, val, 255);
						}
					}
					else
					{
						for (i = 0;i < snd_linear_count;i += 2)
						{
							val = (snd_p[i    ] >> 8) + 128;
							snd_out[i    ] = bound(0, val, 255);
							val = (snd_p[i + 1] >> 8) + 128;
							snd_out[i + 1] = bound(0, val, 255);
						}
					}
					snd_p += snd_linear_count;
					lpaintedtime += (snd_linear_count >> 1);
				}
			}
			else
			{
				// 8bit 1 channel (mono)
				while (lpaintedtime < endtime)
				{
					// handle recirculating buffer issues
					i = lpaintedtime & (shm->samples - 1);
					snd_out = (unsigned char *) pbuf + i;
					snd_linear_count = shm->samples - i;
					if (snd_linear_count > endtime - lpaintedtime)
						snd_linear_count = endtime - lpaintedtime;
					for (i = 0;i < snd_linear_count;i++)
					{
						val = ((snd_p[i * 2] + snd_p[i * 2 + 1]) >> 9) + 128;
						snd_out[i    ] = bound(0, val, 255);
					}
					snd_p += snd_linear_count << 1;
					lpaintedtime += snd_linear_count;
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

qboolean SND_PaintChannelFrom8 (channel_t *ch, int endtime);
qboolean SND_PaintChannelFrom16 (channel_t *ch, int endtime);

void S_PaintChannels(int endtime)
{
	unsigned int i;
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
			if (!ch->leftvol && !ch->rightvol)
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
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{
					if (ch->leftvol > 255)
						ch->leftvol = 255;
					if (ch->rightvol > 255)
						ch->rightvol = 255;

					if (sfx->format.width == 1)
						stop_paint = !SND_PaintChannelFrom8 (ch, count);
					else
						stop_paint = !SND_PaintChannelFrom16 (ch, count);

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


// TODO: Try to merge SND_PaintChannelFrom8 and SND_PaintChannelFrom16
qboolean SND_PaintChannelFrom8 (channel_t *ch, int count)
{
	int snd_vol, leftvol, rightvol;
	const signed char *sfx;
	const sfxbuffer_t *sb;
	int i;

	// If this channel manages its own volume
	if (ch->flags & CHANNELFLAG_FULLVOLUME)
		snd_vol = 256;
	else
		snd_vol = volume.value * 256;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;

	sb = ch->sfx->fetcher->getsb (ch, ch->pos, count);
	if (sb == NULL)
		return false;

	// Stereo sound support
	if (ch->sfx->format.channels == 2)
	{
		sfx = sb->data + (ch->pos - sb->offset) * 2;
		for (i = 0;i < count;i++)
		{
			paintbuffer[i].left += (*sfx++ * leftvol) >> 8;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 8;
		}
	}
	else
	{
		sfx = sb->data + ch->pos - sb->offset;
		for (i = 0;i < count;i++)
		{
			paintbuffer[i].left += (*sfx * leftvol) >> 8;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 8;
		}

	}
	ch->pos += count;
	return true;
}

qboolean SND_PaintChannelFrom16 (channel_t *ch, int count)
{
	int snd_vol, leftvol, rightvol;
	signed short *sfx;
	const sfxbuffer_t *sb;
	int i;

	// If this channel manages its own volume
	if (ch->flags & CHANNELFLAG_FULLVOLUME)
		snd_vol = 256;
	else
		snd_vol = volume.value * 256;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;

	sb = ch->sfx->fetcher->getsb (ch, ch->pos, count);
	if (sb == NULL)
		return false;

	// Stereo sound support
	if (ch->sfx->format.channels == 2)
	{
		sfx = (signed short *)sb->data + (ch->pos - sb->offset) * 2;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += (*sfx++ * leftvol) >> 16;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 16;
		}
	}
	else
	{
		sfx = (signed short *)sb->data + ch->pos - sb->offset;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += (*sfx * leftvol) >> 16;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 16;
		}
	}

	ch->pos += count;
	return true;
}

