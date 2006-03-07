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
#include "snd_ogg.h"
#include "snd_wav.h"


/*
================
ResampleSfx
================
*/
size_t ResampleSfx (const unsigned char *in_data, size_t in_length, const snd_format_t* in_format, unsigned char *out_data, const char* sfxname)
{
	size_t srclength, outcount;

	srclength = in_length * in_format->channels;
	outcount = (double)in_length * shm->format.speed / in_format->speed;

	//Con_DPrintf("ResampleSfx(%s): %d samples @ %dHz -> %d samples @ %dHz\n",
	//			sfxname, in_length, in_format->speed, outcount, shm->format.speed);

	// Trivial case (direct transfer)
	if (in_format->speed == shm->format.speed)
	{
		if (in_format->width == 1)
		{
			size_t i;

			for (i = 0; i < srclength; i++)
				((signed char*)out_data)[i] = in_data[i] - 128;
		}
		else  // if (in_format->width == 2)
			memcpy (out_data, in_data, srclength * in_format->width);
	}

	// General case (linear interpolation with a fixed-point fractional
	// step, 18-bit integer part and 14-bit fractional part)
	// Can handle up to 2^18 (262144) samples per second (> 96KHz stereo)
#	define FRACTIONAL_BITS 14
#	define FRACTIONAL_MASK ((1 << FRACTIONAL_BITS) - 1)
#	define INTEGER_BITS (sizeof(samplefrac)*8 - FRACTIONAL_BITS)
	else
	{
		const unsigned int fracstep = (double)in_format->speed / shm->format.speed * (1 << FRACTIONAL_BITS);
		size_t remain_in = srclength, total_out = 0;
		unsigned int samplefrac;
		const unsigned char *in_ptr = in_data;
		unsigned char *out_ptr = out_data;

		// Check that we can handle one second of that sound
		if (in_format->speed * in_format->channels > (1 << INTEGER_BITS))
		{
			Con_Printf ("ResampleSfx: sound quality too high for resampling (%uHz, %u channel(s))\n",
					   in_format->speed, in_format->channels);
			return 0;
		}

		// We work 1 sec at a time to make sure we don't accumulate any
		// significant error when adding "fracstep" over several seconds, and
		// also to be able to handle very long sounds.
		while (total_out < outcount)
		{
			size_t tmpcount, interpolation_limit, i, j;
			unsigned int srcsample;

			samplefrac = 0;

			// If more than 1 sec of sound remains to be converted
			if (outcount - total_out > shm->format.speed)
			{
				tmpcount = shm->format.speed;
				interpolation_limit = tmpcount;  // all samples can be interpolated
			}
			else
			{
				tmpcount = outcount - total_out;
				interpolation_limit = ceil((double)(((remain_in / in_format->channels) - 1) << FRACTIONAL_BITS) / fracstep);
				if (interpolation_limit > tmpcount)
					interpolation_limit = tmpcount;
			}

			// 16 bit samples
			if (in_format->width == 2)
			{
				const short* in_ptr_short;

				// Interpolated part
				for (i = 0; i < interpolation_limit; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * in_format->channels;
					in_ptr_short = &((const short*)in_ptr)[srcsample];

					for (j = 0; j < in_format->channels; j++)
					{
						int a, b;

						a = *in_ptr_short;
						b = *(in_ptr_short + in_format->channels);
						*((short*)out_ptr) = (((b - a) * (samplefrac & FRACTIONAL_MASK)) >> FRACTIONAL_BITS) + a;

						in_ptr_short++;
						out_ptr += sizeof (short);
					}

					samplefrac += fracstep;
				}

				// Non-interpolated part
				for (/* nothing */; i < tmpcount; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * in_format->channels;
					in_ptr_short = &((const short*)in_ptr)[srcsample];

					for (j = 0; j < in_format->channels; j++)
					{
						*((short*)out_ptr) = *in_ptr_short;

						in_ptr_short++;
						out_ptr += sizeof (short);
					}

					samplefrac += fracstep;
				}
			}
			// 8 bit samples
			else  // if (in_format->width == 1)
			{
				const unsigned char* in_ptr_byte;

				// Convert up to 1 sec of sound
				for (i = 0; i < interpolation_limit; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * in_format->channels;
					in_ptr_byte = &((const unsigned char*)in_ptr)[srcsample];

					for (j = 0; j < in_format->channels; j++)
					{
						int a, b;

						a = *in_ptr_byte - 128;
						b = *(in_ptr_byte + in_format->channels) - 128;
						*((signed char*)out_ptr) = (((b - a) * (samplefrac & FRACTIONAL_MASK)) >> FRACTIONAL_BITS) + a;

						in_ptr_byte++;
						out_ptr += sizeof (signed char);
					}

					samplefrac += fracstep;
				}

				// Non-interpolated part
				for (/* nothing */; i < tmpcount; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * in_format->channels;
					in_ptr_byte = &((const unsigned char*)in_ptr)[srcsample];

					for (j = 0; j < in_format->channels; j++)
					{
						*((signed char*)out_ptr) = *in_ptr_byte - 128;

						in_ptr_byte++;
						out_ptr += sizeof (signed char);
					}

					samplefrac += fracstep;
				}
			}

			// Update the counters and the buffer position
			remain_in -= in_format->speed * in_format->channels;
			in_ptr += in_format->speed * in_format->channels * in_format->width;
			total_out += tmpcount;
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
qboolean S_LoadSound (sfx_t *s, qboolean complain)
{
	char namebuffer[MAX_QPATH + 16];
	size_t len;

	if (!shm || !shm->format.speed)
		return false;

	// If we weren't able to load it previously, no need to retry
	if (s->flags & SFXFLAG_FILEMISSING)
		return false;

	// See if in memory
	if (s->fetcher != NULL)
	{
		if (s->format.speed != shm->format.speed)
			Con_Printf ("S_LoadSound: sound %s hasn't been resampled (%uHz instead of %uHz)\n", s->name);
		return true;
	}

	// LordHavoc: if the sound filename does not begin with sound/, try adding it
	if (strncasecmp(s->name, "sound/", 6))
	{
		len = dpsnprintf (namebuffer, sizeof(namebuffer), "sound/%s", s->name);
		if (len < 0)
		{
			// name too long
			Con_Printf("S_LoadSound: name \"%s\" is too long\n", s->name);
			return false;
		}
		if (S_LoadWavFile (namebuffer, s))
			return true;
		if (len >= 4 && !strcasecmp (namebuffer + len - 4, ".wav"))
			strcpy (namebuffer + len - 3, "ogg");
		if (OGG_LoadVorbisFile (namebuffer, s))
			return true;
	}

	// LordHavoc: then try without the added sound/ as wav and ogg
	len = dpsnprintf (namebuffer, sizeof(namebuffer), "%s", s->name);
	if (len < 0)
	{
		// name too long
		Con_Printf("S_LoadSound: name \"%s\" is too long\n", s->name);
		return false;
	}
	if (S_LoadWavFile (namebuffer, s))
		return true;
	if (len >= 4 && !strcasecmp (namebuffer + len - 4, ".wav"))
		strcpy (namebuffer + len - 3, "ogg");
	if (OGG_LoadVorbisFile (namebuffer, s))
		return true;

	// Can't load the sound!
	s->flags |= SFXFLAG_FILEMISSING;
	if (complain)
		Con_Printf("S_LoadSound: Couldn't load \"%s\"\n", s->name);
	return false;
}
