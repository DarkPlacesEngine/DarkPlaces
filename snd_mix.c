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

#ifdef _WIN32
#include "winquake.h"
#endif

// LordHavoc: was 512, expanded to 2048
#define	PAINTBUFFER_SIZE	2048
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int		snd_scaletable[32][256];

/*
// LordHavoc: disabled this because it desyncs with the video too easily
extern cvar_t cl_avidemo;
static FILE *cl_avidemo_soundfile = NULL;
void S_CaptureAVISound(portable_samplepair_t *buf, int length)
{
	int i, n;
	qbyte out[PAINTBUFFER_SIZE * 4];
	char filename[MAX_OSPATH];

	if (cl_avidemo.value >= 0.1f)
	{
		if (cl_avidemo_soundfile == NULL)
		{
			sprintf (filename, "%s/dpavi.wav", com_gamedir);
			cl_avidemo_soundfile = fopen(filename, "wb");
			memset(out, 0, 44);
			fwrite(out, 1, 44, cl_avidemo_soundfile);
			// header will be filled out when file is closed
		}
		fseek(cl_avidemo_soundfile, 0, SEEK_END);
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
		if (fwrite(out, 4, length, cl_avidemo_soundfile) < length)
		{
			Cvar_SetValueQuick(&cl_avidemo, 0);
			Con_Printf("avi saving sound failed, out of disk space?  stopping avi demo capture.\n");
		}
	}
	else if (cl_avidemo_soundfile)
	{
		// file has not been closed yet, close it
		fseek(cl_avidemo_soundfile, 0, SEEK_END);
		i = ftell(cl_avidemo_soundfile);

		//"RIFF", (int) unknown (chunk size), "WAVE",
		//"fmt ", (int) 16 (chunk size), (short) format 1 (uncompressed PCM), (short) 2 channels, (int) unknown rate, (int) unknown bytes per second, (short) 4 bytes per sample (channels * bytes per channel), (short) 16 bits per channel
		//"data", (int) unknown (chunk size)
		memcpy(out, "RIFF****WAVEfmt \x10\x00\x00\x00\x01\x00\x02\x00********\x04\x00\x10\x00data****", 44);
		// the length of the whole RIFF chunk
		n = i - 8;
		out[4] = (n) & 0xFF;
		out[5] = (n >> 8) & 0xFF;
		out[6] = (n >> 16) & 0xFF;
		out[7] = (n >> 24) & 0xFF;
		// rate
		n = shm->speed;
		out[24] = (n) & 0xFF;
		out[25] = (n >> 8) & 0xFF;
		out[26] = (n >> 16) & 0xFF;
		out[27] = (n >> 24) & 0xFF;
		// bytes per second (rate * channels * bytes per channel)
		n = shm->speed * 4;
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

		fseek(cl_avidemo_soundfile, 0, SEEK_SET);
		fwrite(out, 1, 44, cl_avidemo_soundfile);
		fclose(cl_avidemo_soundfile);
		cl_avidemo_soundfile = NULL;
	}
}
*/

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
		if (shm->samplebits == 16)
		{
			// 16bit
			short *snd_out;
			if (shm->channels == 2)
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
			if (shm->channels == 2)
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

void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime);
void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int endtime);

void S_PaintChannels(int endtime)
{
	int i;
	int end;
	channel_t *ch;
	sfxcache_t *sc;
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
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sc = S_LoadSound (ch->sfx, true);
			if (!sc)
				continue;

			ltime = paintedtime;

			while (ltime < end)
			{
				// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{
					if (sc->width == 1)
						SND_PaintChannelFrom8(ch, sc, count);
					else
						SND_PaintChannelFrom16(ch, sc, count);

					ltime += count;
				}

				// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else
					{
						// channel just stopped
						ch->sfx = NULL;
						break;
					}
				}
			}
		}

		// transfer out according to DMA format
		//S_CaptureAVISound(paintbuffer, end - paintedtime);
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void SND_InitScaletable (void)
{
	int		i, j;

	for (i=0 ; i<32 ; i++)
		for (j=0 ; j<256 ; j++)
			snd_scaletable[i][j] = ((signed char)j) * i * 8;
}


void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int		*lscale, *rscale;
	unsigned char *sfx;
	int		i;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	if (sc->stereo)
	{
		// LordHavoc: stereo sound support, and optimizations
		sfx = (unsigned char *)sc->data + ch->pos * 2;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += lscale[*sfx++];
			paintbuffer[i].right += rscale[*sfx++];
		}
		
	}
	else
	{
		sfx = (unsigned char *)sc->data + ch->pos;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += lscale[*sfx];
			paintbuffer[i].right += rscale[*sfx++];
		}

	}
	ch->pos += count;
}

void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count)
{
	int leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	if (sc->stereo)
	{
		// LordHavoc: stereo sound support, and optimizations
		sfx = (signed short *)sc->data + ch->pos * 2;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += (*sfx++ * leftvol) >> 8;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 8;
		}
	}
	else
	{
		sfx = (signed short *)sc->data + ch->pos;

		for (i=0 ; i<count ; i++)
		{
			paintbuffer[i].left += (*sfx * leftvol) >> 8;
			paintbuffer[i].right += (*sfx++ * rightvol) >> 8;
		}
	}

	ch->pos += count;
}

