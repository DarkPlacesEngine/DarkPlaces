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
// snd_mem.c: sound caching

#include "quakedef.h"

#include "snd_ogg.h"
#include "snd_wav.h"


/*
================
ResampleSfx
================
*/
size_t ResampleSfx (const qbyte *in_data, size_t in_length, const snd_format_t* in_format, qbyte *out_data, const char* sfxname)
{
	int samplefrac, fracstep;
	size_t i, srcsample, srclength, outcount;

	// this is usually 0.5 (128), 1 (256), or 2 (512)
	fracstep = ((double) in_format->speed / (double) shm->format.speed) * 256.0;

	srclength = in_length * in_format->channels;

	outcount = (double) in_length * (double) shm->format.speed / (double) in_format->speed;
	Con_DPrintf("ResampleSfx: resampling sound \"%s\" from %dHz to %dHz (%d samples to %d samples)\n",
				sfxname, in_format->speed, shm->format.speed, in_length, outcount);

// resample / decimate to the current source rate

	if (fracstep == 256)
	{
		// fast case for direct transfer
		if (in_format->width == 1) // 8bit
			for (i = 0;i < srclength;i++)
				((signed char *)out_data)[i] = ((unsigned char *)in_data)[i] - 128;
		else //if (sb->width == 2) // 16bit
			for (i = 0;i < srclength;i++)
				((short *)out_data)[i] = ((short *)in_data)[i];
	}
	else
	{
		// general case
		samplefrac = 0;
		if ((fracstep & 255) == 0) // skipping points on perfect multiple
		{
			srcsample = 0;
			fracstep >>= 8;
			if (in_format->width == 2)
			{
				short *out = (short*)out_data;
				const short *in = (const short*)in_data;
				if (in_format->channels == 2) // LordHavoc: stereo sound support
				{
					fracstep <<= 1;
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = in[srcsample  ];
						*out++ = in[srcsample+1];
						srcsample += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = in[srcsample];
						srcsample += fracstep;
					}
				}
			}
			else
			{
				signed char *out = out_data;
				const unsigned char *in = in_data;
				if (in_format->channels == 2)
				{
					fracstep <<= 1;
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = in[srcsample  ] - 128;
						*out++ = in[srcsample+1] - 128;
						srcsample += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						*out++ = in[srcsample  ] - 128;
						srcsample += fracstep;
					}
				}
			}
		}
		else
		{
			int sample;
			int a, b;
			if (in_format->width == 2)
			{
				short *out = (short*)out_data;
				const short *in = (const short*)in_data;
				if (in_format->channels == 2)
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = (samplefrac >> 8) << 1;
						a = in[srcsample  ];
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = in[srcsample+2];
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (short) sample;
						a = in[srcsample+1];
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = in[srcsample+3];
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (short) sample;
						samplefrac += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = samplefrac >> 8;
						a = in[srcsample  ];
						if (srcsample+1 >= srclength)
							b = 0;
						else
							b = in[srcsample+1];
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (short) sample;
						samplefrac += fracstep;
					}
				}
			}
			else
			{
				signed char *out = out_data;
				const unsigned char *in = in_data;
				if (in_format->channels == 2)
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = (samplefrac >> 8) << 1;
						a = (int) in[srcsample  ] - 128;
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = (int) in[srcsample+2] - 128;
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (signed char) sample;
						a = (int) in[srcsample+1] - 128;
						if (srcsample+2 >= srclength)
							b = 0;
						else
							b = (int) in[srcsample+3] - 128;
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (signed char) sample;
						samplefrac += fracstep;
					}
				}
				else
				{
					for (i=0 ; i<outcount ; i++)
					{
						srcsample = samplefrac >> 8;
						a = (int) in[srcsample  ] - 128;
						if (srcsample+1 >= srclength)
							b = 0;
						else
							b = (int) in[srcsample+1] - 128;
						sample = (((b - a) * (samplefrac & 255)) >> 8) + a;
						*out++ = (signed char) sample;
						samplefrac += fracstep;
					}
				}
			}
		}
	}

	return outcount;
}

//=============================================================================

/*
==============
S_LoadSound
==============
*/
qboolean S_LoadSound (sfx_t *s, int complain)
{
	char namebuffer[MAX_QPATH];
	size_t len;
	qboolean modified_name = false;

	// see if still in memory
	if (!shm || !shm->format.speed)
		return false;
	if (s->fetcher != NULL)
	{
		if (s->format.speed != shm->format.speed)
			Sys_Error ("S_LoadSound: sound %s hasn't been resampled (%uHz instead of %uHz)", s->name);
		return true;
	}

	len = snprintf (namebuffer, sizeof (namebuffer), "sound/%s", s->name);
	if (len >= sizeof (namebuffer))
		return false;

	// Try to load it as a WAV file
	if (S_LoadWavFile (namebuffer, s))
		return true;

	// Else, try to load it as an Ogg Vorbis file
	if (!strcasecmp (namebuffer + len - 4, ".wav"))
	{
		strcpy (namebuffer + len - 3, "ogg");
		modified_name = true;
	}
	if (OGG_LoadVorbisFile (namebuffer, s))
		return true;

	// Can't load the sound!
	if (!complain)
		s->flags |= SFXFLAG_SILENTLYMISSING;
	else
		s->flags &= ~SFXFLAG_SILENTLYMISSING;
	if (complain)
	{
		if (modified_name)
			strcpy (namebuffer + len - 3, "wav");
		Con_Printf("Couldn't load %s\n", namebuffer);
	}
	return false;
}

void S_UnloadSound(sfx_t *s)
{
	if (s->fetcher != NULL)
	{
		unsigned int i;

		s->fetcher = NULL;
		s->fetcher_data = NULL;
		Mem_FreePool(&s->mempool);

		// At this point, some per-channel data pointers may point to freed zones.
		// Practically, it shouldn't be a problem; but it's wrong, so we fix that
		for (i = 0; i < total_channels ; i++)
			if (channels[i].sfx == s)
				channels[i].fetcher_data = NULL;
	}
}
