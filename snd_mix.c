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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "quakedef.h"

typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

// LordHavoc: was 512, expanded to 2048
#define	PAINTBUFFER_SIZE 2048
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int snd_scaletable[32][256];

// FIXME: it desyncs with the video too easily
extern cvar_t cl_avidemo;
static qfile_t *cl_avidemo_soundfile = NULL;
void S_CaptureAVISound(portable_samplepair_t *buf, size_t length)
{
	int n;
	size_t i;
	qbyte out[PAINTBUFFER_SIZE * 4];

	if (cl_avidemo.value >= 0.1f)
	{
		if (cl_avidemo_soundfile == NULL)
		{
			cl_avidemo_soundfile = FS_Open ("video/dpavi.wav", "wb", false);
			memset(out, 0, 44);
			FS_Write (cl_avidemo_soundfile, out, 44);
			// header will be filled out when file is closed
		}
		FS_Seek (cl_avidemo_soundfile, 0, SEEK_END);
		// write the sound buffer as little endian 16bit interleaved stereo
		for(i = 0;i < length;i++)
		{
			n = buf[i].left >> 2; // quiet enough to prevent clipping most of the time
			n = bound(-32768, n, 32767);
			out[i*4+0] = n & 0xFF;
			out[i*4+1] = (n >> 8) & 0xFF;
			n = buf[i].right >> 2; // quiet enough to prevent clipping most of the time
			n = bound(-32768, n, 32767);
			out[i*4+2] = n & 0xFF;
			out[i*4+3] = (n >> 8) & 0xFF;
		}
		if (FS_Write (cl_avidemo_soundfile, out, 4 * length) < 4 * length)
		{
			Cvar_SetValueQuick(&cl_avidemo, 0);
			Con_Print("avi saving sound failed, out of disk space?  stopping avi demo capture.\n");
		}
	}
	else if (cl_avidemo_soundfile)
	{
		// file has not been closed yet, close it
		FS_Seek (cl_avidemo_soundfile, 0, SEEK_END);
		i = FS_Tell (cl_avidemo_soundfile);

		//"RIFF", (int) unknown (chunk size), "WAVE",
		//"fmt ", (int) 16 (chunk size), (short) format 1 (uncompressed PCM), (short) 2 channels, (int) unknown rate, (int) unknown bytes per second, (short) 4 bytes per sample (channels * bytes per channel), (short) 16 bits per channel
		//"data", (int) unknown (chunk size)
		memcpy (out, "RIFF****WAVEfmt \x10\x00\x00\x00\x01\x00\x02\x00********\x04\x00\x10\0data****", 44);
		// the length of the whole RIFF chunk
		n = i - 8;
		out[4] = (n) & 0xFF;
		out[5] = (n >> 8) & 0xFF;
		out[6] = (n >> 16) & 0xFF;
		out[7] = (n >> 24) & 0xFF;
		// rate
		n = shm->format.speed;
		out[24] = (n) & 0xFF;
		out[25] = (n >> 8) & 0xFF;
		out[26] = (n >> 16) & 0xFF;
		out[27] = (n >> 24) & 0xFF;
		// bytes per second (rate * channels * bytes per channel)
		n = shm->format.speed * 2 * 2;
		out[28] = (n) & 0xFF;
		out[29] = (n >> 8) & 0xFF;
		out[30] = (n >> 16) & 0xFF;
		out[31] = (n >> 24) & 0xFF;
		// the length of the data chunk
		n = i - 44;
		out[40] = (n) & 0xFF;
		out[41] = (n >> 8) & 0xFF;
		out[42] = (n >> 16) & 0xFF;
		out[43] = (n >> 24) & 0xFF;

		FS_Seek (cl_avidemo_soundfile, 0, SEEK_SET);
		FS_Write (cl_avidemo_soundfile, out, 44);
		FS_Close (cl_avidemo_soundfile);
		cl_avidemo_soundfile = NULL;
	}
}

void S_TransferPaintBuffer(int endtime)
{
	void *pbuf;
	if ((pbuf = S_LockBuffer()))
	{
		int i;
		int *snd_p;
		int snd_vol;
		int lpaintedtime;
		int snd_linear_count;
		int val;
		snd_p = (int *) paintbuffer;
		snd_vol = volume.value*256;
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
							val = (snd_p[i + 1] * snd_vol) >> 8;
							snd_out[i    ] = bound(-32768, val, 32767);
							val = (snd_p[i    ] * snd_vol) >> 8;
							snd_out[i + 1] = bound(-32768, val, 32767);
						}
					}
					else
					{
						for (i = 0;i < snd_linear_count;i += 2)
						{
							val = (snd_p[i    ] * snd_vol) >> 8;
							snd_out[i    ] = bound(-32768, val, 32767);
							val = (snd_p[i + 1] * snd_vol) >> 8;
							snd_out[i + 1] = bound(-32768, val, 32767);
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
						val = ((snd_p[i * 2 + 0] + snd_p[i * 2 + 1]) * snd_vol) >> 9;
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
							val = ((snd_p[i + 1] * snd_vol) >> 16) + 128;
							snd_out[i    ] = bound(0, val, 255);
							val = ((snd_p[i    ] * snd_vol) >> 16) + 128;
							snd_out[i + 1] = bound(0, val, 255);
						}
					}
					else
					{
						for (i = 0;i < snd_linear_count;i += 2)
						{
							val = ((snd_p[i    ] * snd_vol) >> 16) + 128;
							snd_out[i    ] = bound(0, val, 255);
							val = ((snd_p[i + 1] * snd_vol) >> 16) + 128;
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
						val = (((snd_p[i * 2] + snd_p[i * 2 + 1]) * snd_vol) >> 17) + 128;
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

		// clear the paint buffer, filling it with data from rawsamples (music/video/whatever)
		S_RawSamples_Dequeue(&paintbuffer->left, end - paintedtime);

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
				size_t pausedtime;

				pausedtime = end - paintedtime;
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
					if (sfx->format.width == 1)
						stop_paint = !SND_PaintChannelFrom8(ch, count);
					else
						stop_paint = !SND_PaintChannelFrom16(ch, count);

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

void SND_InitScaletable (void)
{
	int i, j;

	for (i = 0;i < 32;i++)
		for (j = 0;j < 256;j++)
			snd_scaletable[i][j] = ((signed char)j) * i * 8;
}


qboolean SND_PaintChannelFrom8 (channel_t *ch, int count)
{
	int *lscale, *rscale;
	unsigned char *sfx;
	const sfxbuffer_t *sb;
	int i, n;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	sb = ch->sfx->fetcher->getsb (ch, ch->pos, count);
	if (sb == NULL)
		return false;

	if (ch->sfx->format.channels == 2)
	{
		// LordHavoc: stereo sound support, and optimizations
		sfx = (unsigned char *)sb->data + (ch->pos - sb->offset) * 2;
		for (i = 0;i < count;i++)
		{
			paintbuffer[i].left += lscale[*sfx++];
			paintbuffer[i].right += rscale[*sfx++];
		}
	}
	else
	{
		sfx = (unsigned char *)sb->data + ch->pos - sb->offset;
		for (i = 0;i < count;i++)
		{
			n = *sfx++;
			paintbuffer[i].left += lscale[n];
			paintbuffer[i].right += rscale[n];
		}

	}
	ch->pos += count;
	return true;
}

qboolean SND_PaintChannelFrom16 (channel_t *ch, int count)
{
	int leftvol, rightvol;
	signed short *sfx;
	const sfxbuffer_t *sb;
	int i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;

	sb = ch->sfx->fetcher->getsb (ch, ch->pos, count);
	if (sb == NULL)
		return false;

	if (ch->sfx->format.channels == 2)
	{
		// LordHavoc: stereo sound support, and optimizations
		sfx = (signed short *)sb->data + (ch->pos - sb->offset) * 2;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += (*sfx++ * leftvol) >> 8;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 8;
		}
	}
	else
	{
		sfx = (signed short *)sb->data + ch->pos - sb->offset;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += (*sfx * leftvol) >> 8;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 8;
		}
	}

	ch->pos += count;
	return true;
}

