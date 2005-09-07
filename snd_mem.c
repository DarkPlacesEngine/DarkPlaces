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
size_t ResampleSfx (const qbyte *in_data, size_t in_length, const snd_format_t* in_format, qbyte *out_data, const char* sfxname)
{
	size_t srclength, outcount, i;

	srclength = in_length * in_format->channels;
	outcount = (double)in_length * shm->format.speed / in_format->speed;

	//Con_DPrintf("ResampleSfx(%s): %d samples @ %dHz -> %d samples @ %dHz\n",
	//			sfxname, in_length, in_format->speed, outcount, shm->format.speed);

	// Trivial case (direct transfer)
	if (in_format->speed == shm->format.speed)
	{
		if (in_format->width == 1)
		{
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
		const qbyte *in_ptr = in_data;
		qbyte *out_ptr = out_data;

		// Check that we can handle one second of that sound
		if (in_format->speed * in_format->channels > (1 << INTEGER_BITS))
		{
			Con_Printf ("ResampleSfx: sound quality too high for resampling (%uHz, %u channel(s))",
					   in_format->speed, in_format->channels);
			return 0;
		}

		// We work 1 sec at a time to make sure we don't accumulate any
		// significant error when adding "fracstep" over several seconds, and
		// also to be able to handle very long sounds.
		while (total_out < outcount)
		{
			size_t tmpcount;

			samplefrac = 0;

			// If more than 1 sec of sound remains to be converted
			if (outcount - total_out > shm->format.speed)
				tmpcount = shm->format.speed;
			else
				tmpcount = outcount - total_out;

			// Convert up to 1 sec of sound
			for (i = 0; i < tmpcount; i++)
			{
				unsigned int j = 0;
				unsigned int srcsample = (samplefrac >> FRACTIONAL_BITS) * in_format->channels;
				int a, b;

				// 16 bit samples
				if (in_format->width == 2)
				{
					for (j = 0; j < in_format->channels; j++, srcsample++)
					{
						// No value to interpolate with?
						if (srcsample + in_format->channels < remain_in)
						{
							a = ((const short*)in_ptr)[srcsample];
							b = ((const short*)in_ptr)[srcsample + in_format->channels];
							*((short*)out_ptr) = (((b - a) * (samplefrac & FRACTIONAL_MASK)) >> FRACTIONAL_BITS) + a;
						}
						else
							*((short*)out_ptr) = ((const short*)in_ptr)[srcsample];

						out_ptr += sizeof (short);
					}
				}
				// 8 bit samples
				else  // if (in_format->width == 1)
				{
					for (j = 0; j < in_format->channels; j++, srcsample++)
					{
						// No more value to interpolate with?
						if (srcsample + in_format->channels < remain_in)
						{
							a = ((const qbyte*)in_ptr)[srcsample] - 128;
							b = ((const qbyte*)in_ptr)[srcsample + in_format->channels] - 128;
							*((signed char*)out_ptr) = (((b - a) * (samplefrac & FRACTIONAL_MASK)) >> FRACTIONAL_BITS) + a;
						}
						else
							*((signed char*)out_ptr) = ((const qbyte*)in_ptr)[srcsample] - 128;

						out_ptr += sizeof (signed char);
					}
				}

				samplefrac += fracstep;
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
			Con_Printf ("S_LoadSound: sound %s hasn't been resampled (%uHz instead of %uHz)", s->name);
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

void S_UnloadSound (sfx_t *s)
{
	if (s->fetcher != NULL)
	{
		unsigned int i;

		// Stop all channels that use this sound
		for (i = 0; i < total_channels ; i++)
			if (channels[i].sfx == s)
				S_StopChannel (i);

		s->fetcher = NULL;
		s->fetcher_data = NULL;
		Mem_FreePool(&s->mempool);
	}
}
