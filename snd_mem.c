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

	Con_DPrintf("ResampleSfx: resampling sound \"%s\" from %dHz to %dHz (%d samples to %d samples)\n",
				sfxname, in_format->speed, shm->format.speed, in_length, outcount);

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
	#define FRACTIONAL_BITS 14
	#define FRACTIONAL_MASK ((1 << FRACTIONAL_BITS) - 1)
	#define INTEGER_BITS (sizeof(samplefrac)*8 - FRACTIONAL_BITS)
	else
	{
		const unsigned int fracstep = (double)in_format->speed / shm->format.speed * (1 << FRACTIONAL_BITS);
		size_t remain_in = srclength, total_out = 0;
		unsigned int samplefrac;
		const qbyte *in_ptr = in_data;
		qbyte *out_ptr = out_data;

		// Check that we can handle one second of that sound
		if (in_format->speed * in_format->channels > (1 << INTEGER_BITS))
			Sys_Error ("ResampleSfx: sound quality too high for resampling (%uHz, %u channel(s))",
					   in_format->speed, in_format->channels);

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

	len = strlcpy (namebuffer, s->name, sizeof (namebuffer));
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
