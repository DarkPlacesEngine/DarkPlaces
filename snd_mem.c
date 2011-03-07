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
#include "snd_modplug.h"

unsigned char resampling_buffer [48000 * 2 * 2];


/*
====================
Snd_CreateRingBuffer

If "buffer" is NULL, the function allocates one buffer of "sampleframes" sample frames itself
(if "sampleframes" is 0, the function chooses the size).
====================
*/
snd_ringbuffer_t *Snd_CreateRingBuffer (const snd_format_t* format, unsigned int sampleframes, void* buffer)
{
	snd_ringbuffer_t *ringbuffer;

	// If the caller provides a buffer, it must give us its size
	if (sampleframes == 0 && buffer != NULL)
		return NULL;

	ringbuffer = (snd_ringbuffer_t*)Mem_Alloc(snd_mempool, sizeof (*ringbuffer));
	memset(ringbuffer, 0, sizeof(*ringbuffer));
	memcpy(&ringbuffer->format, format, sizeof(ringbuffer->format));

	// If we haven't been given a buffer
	if (buffer == NULL)
	{
		unsigned int maxframes;
		size_t memsize;

		if (sampleframes == 0)
			maxframes = (format->speed + 1) / 2;  // Make the sound buffer large enough for containing 0.5 sec of sound
		else
			maxframes = sampleframes;

		memsize = maxframes * format->width * format->channels;
		ringbuffer->ring = (unsigned char *) Mem_Alloc(snd_mempool, memsize);
		ringbuffer->maxframes = maxframes;
	}
	else
	{
		ringbuffer->ring = (unsigned char *) buffer;
		ringbuffer->maxframes = sampleframes;
	}

	return ringbuffer;
}


/*
====================
Snd_CreateSndBuffer
====================
*/
snd_buffer_t *Snd_CreateSndBuffer (const unsigned char *samples, unsigned int sampleframes, const snd_format_t* in_format, unsigned int sb_speed)
{
	size_t newsampleframes, memsize;
	snd_buffer_t* sb;

	newsampleframes = (size_t) ceil((double)sampleframes * (double)sb_speed / (double)in_format->speed);

	memsize = newsampleframes * in_format->channels * in_format->width;
	memsize += sizeof (*sb) - sizeof (sb->samples);

	sb = (snd_buffer_t*)Mem_Alloc (snd_mempool, memsize);
	sb->format.channels = in_format->channels;
	sb->format.width = in_format->width;
	sb->format.speed = sb_speed;
	sb->maxframes = newsampleframes;
	sb->nbframes = 0;

	if (!Snd_AppendToSndBuffer (sb, samples, sampleframes, in_format))
	{
		Mem_Free (sb);
		return NULL;
	}

	return sb;
}


/*
====================
Snd_AppendToSndBuffer
====================
*/
qboolean Snd_AppendToSndBuffer (snd_buffer_t* sb, const unsigned char *samples, unsigned int sampleframes, const snd_format_t* format)
{
	size_t srclength, outcount;
	unsigned char *out_data;

	//Con_DPrintf("ResampleSfx: %d samples @ %dHz -> %d samples @ %dHz\n",
	//			sampleframes, format->speed, outcount, sb->format.speed);

	// If the formats are incompatible
	if (sb->format.channels != format->channels || sb->format.width != format->width)
	{
		Con_Print("AppendToSndBuffer: incompatible sound formats!\n");
		return false;
	}

	outcount = (size_t) ((double)sampleframes * (double)sb->format.speed / (double)format->speed);

	// If the sound buffer is too short
	if (outcount > sb->maxframes - sb->nbframes)
	{
		Con_Print("AppendToSndBuffer: sound buffer too short!\n");
		return false;
	}

	out_data = &sb->samples[sb->nbframes * sb->format.width * sb->format.channels];
	srclength = sampleframes * format->channels;

	// Trivial case (direct transfer)
	if (format->speed == sb->format.speed)
	{
		if (format->width == 1)
		{
			size_t i;

			for (i = 0; i < srclength; i++)
				((signed char*)out_data)[i] = samples[i] - 128;
		}
		else  // if (format->width == 2)
			memcpy (out_data, samples, srclength * format->width);
	}

	// General case (linear interpolation with a fixed-point fractional
	// step, 18-bit integer part and 14-bit fractional part)
	// Can handle up to 2^18 (262144) samples per second (> 96KHz stereo)
#	define FRACTIONAL_BITS 14
#	define FRACTIONAL_MASK ((1 << FRACTIONAL_BITS) - 1)
#	define INTEGER_BITS (sizeof(samplefrac)*8 - FRACTIONAL_BITS)
	else
	{
		const unsigned int fracstep = (unsigned int)((double)format->speed / sb->format.speed * (1 << FRACTIONAL_BITS));
		size_t remain_in = srclength, total_out = 0;
		unsigned int samplefrac;
		const unsigned char *in_ptr = samples;
		unsigned char *out_ptr = out_data;

		// Check that we can handle one second of that sound
		if (format->speed * format->channels > (1 << INTEGER_BITS))
		{
			Con_Printf ("ResampleSfx: sound quality too high for resampling (%uHz, %u channel(s))\n",
					   format->speed, format->channels);
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
			if (outcount - total_out > sb->format.speed)
			{
				tmpcount = sb->format.speed;
				interpolation_limit = tmpcount;  // all samples can be interpolated
			}
			else
			{
				tmpcount = outcount - total_out;
				interpolation_limit = (int)ceil((double)(((remain_in / format->channels) - 1) << FRACTIONAL_BITS) / fracstep);
				if (interpolation_limit > tmpcount)
					interpolation_limit = tmpcount;
			}

			// 16 bit samples
			if (format->width == 2)
			{
				const short* in_ptr_short;

				// Interpolated part
				for (i = 0; i < interpolation_limit; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * format->channels;
					in_ptr_short = &((const short*)in_ptr)[srcsample];

					for (j = 0; j < format->channels; j++)
					{
						int a, b;

						a = *in_ptr_short;
						b = *(in_ptr_short + format->channels);
						*((short*)out_ptr) = (((b - a) * (samplefrac & FRACTIONAL_MASK)) >> FRACTIONAL_BITS) + a;

						in_ptr_short++;
						out_ptr += sizeof (short);
					}

					samplefrac += fracstep;
				}

				// Non-interpolated part
				for (/* nothing */; i < tmpcount; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * format->channels;
					in_ptr_short = &((const short*)in_ptr)[srcsample];

					for (j = 0; j < format->channels; j++)
					{
						*((short*)out_ptr) = *in_ptr_short;

						in_ptr_short++;
						out_ptr += sizeof (short);
					}

					samplefrac += fracstep;
				}
			}
			// 8 bit samples
			else  // if (format->width == 1)
			{
				const unsigned char* in_ptr_byte;

				// Convert up to 1 sec of sound
				for (i = 0; i < interpolation_limit; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * format->channels;
					in_ptr_byte = &((const unsigned char*)in_ptr)[srcsample];

					for (j = 0; j < format->channels; j++)
					{
						int a, b;

						a = *in_ptr_byte - 128;
						b = *(in_ptr_byte + format->channels) - 128;
						*((signed char*)out_ptr) = (((b - a) * (samplefrac & FRACTIONAL_MASK)) >> FRACTIONAL_BITS) + a;

						in_ptr_byte++;
						out_ptr += sizeof (signed char);
					}

					samplefrac += fracstep;
				}

				// Non-interpolated part
				for (/* nothing */; i < tmpcount; i++)
				{
					srcsample = (samplefrac >> FRACTIONAL_BITS) * format->channels;
					in_ptr_byte = &((const unsigned char*)in_ptr)[srcsample];

					for (j = 0; j < format->channels; j++)
					{
						*((signed char*)out_ptr) = *in_ptr_byte - 128;

						in_ptr_byte++;
						out_ptr += sizeof (signed char);
					}

					samplefrac += fracstep;
				}
			}

			// Update the counters and the buffer position
			remain_in -= format->speed * format->channels;
			in_ptr += format->speed * format->channels * format->width;
			total_out += tmpcount;
		}
	}

	sb->nbframes += outcount;
	return true;
}


//=============================================================================

/*
==============
S_LoadSound
==============
*/
qboolean S_LoadSound (sfx_t *sfx, qboolean complain)
{
	char namebuffer[MAX_QPATH + 16];
	size_t len;

	// See if already loaded
	if (sfx->fetcher != NULL)
		return true;

	// If we weren't able to load it previously, no need to retry
	// Note: S_PrecacheSound clears this flag to cause a retry
	if (sfx->flags & SFXFLAG_FILEMISSING)
		return false;

	// No sound?
	if (snd_renderbuffer == NULL)
		return false;

	// Initialize volume peak to 0; if ReplayGain is supported, the loader will change this away
	sfx->volume_peak = 0.0;

	if (developer_loading.integer)
		Con_Printf("loading sound %s\n", sfx->name);

	SCR_PushLoadingScreen(true, sfx->name, 1);

	// LordHavoc: if the sound filename does not begin with sound/, try adding it
	if (strncasecmp(sfx->name, "sound/", 6))
	{
		dpsnprintf (namebuffer, sizeof(namebuffer), "sound/%s", sfx->name);
		len = strlen(namebuffer);
		if (len >= 4 && !strcasecmp (namebuffer + len - 4, ".wav"))
		{
			if (S_LoadWavFile (namebuffer, sfx))
				goto loaded;
			memcpy (namebuffer + len - 3, "ogg", 4);
		}
		if (len >= 4 && !strcasecmp (namebuffer + len - 4, ".ogg"))
		{
			if (OGG_LoadVorbisFile (namebuffer, sfx))
				goto loaded;
		}
		else
		{
			if (ModPlug_LoadModPlugFile (namebuffer, sfx))
				goto loaded;
		}
	}

	// LordHavoc: then try without the added sound/ as wav and ogg
	dpsnprintf (namebuffer, sizeof(namebuffer), "%s", sfx->name);
	len = strlen(namebuffer);
	// request foo.wav: tries foo.wav, then foo.ogg
	// request foo.ogg: tries foo.ogg only
	// request foo.mod: tries foo.mod only
	if (len >= 4 && !strcasecmp (namebuffer + len - 4, ".wav"))
	{
		if (S_LoadWavFile (namebuffer, sfx))
			goto loaded;
		memcpy (namebuffer + len - 3, "ogg", 4);
	}
	if (len >= 4 && !strcasecmp (namebuffer + len - 4, ".ogg"))
	{
		if (OGG_LoadVorbisFile (namebuffer, sfx))
			goto loaded;
	}
	else
	{
		if (ModPlug_LoadModPlugFile (namebuffer, sfx))
			goto loaded;
	}

	// Can't load the sound!
	sfx->flags |= SFXFLAG_FILEMISSING;
	if (complain)
		Con_DPrintf("failed to load sound \"%s\"\n", sfx->name);

	SCR_PopLoadingScreen(false);
	return false;

loaded:
	SCR_PopLoadingScreen(false);
	return true;
}
