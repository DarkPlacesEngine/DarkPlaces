/*
	Copyright (C) 2003-2005  Mathieu Olivier

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/


#include "quakedef.h"
#include "snd_main.h"
#include "snd_modplug.h"

#ifdef SND_MODPLUG_STATIC

#include <libmodplug/modplug.h>
qboolean ModPlug_OpenLibrary (void)
{
	return true; // statically linked
}
void ModPlug_CloseLibrary (void)
{
}
#define modplug_dll 1
#define qModPlug_Load ModPlug_Load
#define qModPlug_Unload ModPlug_Unload
#define qModPlug_Read ModPlug_Read
#define qModPlug_Seek ModPlug_Seek
#define qModPlug_GetSettings ModPlug_GetSettings
#define qModPlug_SetSettings ModPlug_SetSettings
#define qModPlug_SetMasterVolume ModPlug_SetMasterVolume

#else
// BEGIN SECTION FROM modplug.h

	/*
	 * This source code is public domain.
	 *
	 * Authors: Kenton Varda <temporal@gauge3d.org> (C interface wrapper)
	 */

	enum _ModPlug_Flags
	{
			MODPLUG_ENABLE_OVERSAMPLING     = 1 << 0,  /* Enable oversampling (*highly* recommended) */
			MODPLUG_ENABLE_NOISE_REDUCTION  = 1 << 1,  /* Enable noise reduction */
			MODPLUG_ENABLE_REVERB           = 1 << 2,  /* Enable reverb */
			MODPLUG_ENABLE_MEGABASS         = 1 << 3,  /* Enable megabass */
			MODPLUG_ENABLE_SURROUND         = 1 << 4   /* Enable surround sound. */
	};

	enum _ModPlug_ResamplingMode
	{
			MODPLUG_RESAMPLE_NEAREST = 0,  /* No interpolation (very fast, extremely bad sound quality) */
			MODPLUG_RESAMPLE_LINEAR  = 1,  /* Linear interpolation (fast, good quality) */
			MODPLUG_RESAMPLE_SPLINE  = 2,  /* Cubic spline interpolation (high quality) */
			MODPLUG_RESAMPLE_FIR     = 3   /* 8-tap fir filter (extremely high quality) */
	};

	typedef struct _ModPlug_Settings
	{
			int mFlags;  /* One or more of the MODPLUG_ENABLE_* flags above, bitwise-OR'ed */
			
			/* Note that ModPlug always decodes sound at 44100kHz, 32 bit, stereo and then
			 * down-mixes to the settings you choose. */
			int mChannels;       /* Number of channels - 1 for mono or 2 for stereo */
			int mBits;           /* Bits per sample - 8, 16, or 32 */
			int mFrequency;      /* Sampling rate - 11025, 22050, or 44100 */
			int mResamplingMode; /* One of MODPLUG_RESAMPLE_*, above */

			int mStereoSeparation; /* Stereo separation, 1 - 256 */
			int mMaxMixChannels; /* Maximum number of mixing channels (polyphony), 32 - 256 */
			
			int mReverbDepth;    /* Reverb level 0(quiet)-100(loud)      */
			int mReverbDelay;    /* Reverb delay in ms, usually 40-200ms */
			int mBassAmount;     /* XBass level 0(quiet)-100(loud)       */
			int mBassRange;      /* XBass cutoff in Hz 10-100            */
			int mSurroundDepth;  /* Surround level 0(quiet)-100(heavy)   */
			int mSurroundDelay;  /* Surround delay in ms, usually 5-40ms */
			int mLoopCount;      /* Number of times to loop.  Zero prevents looping.
									-1 loops forever. */
	} ModPlug_Settings;

	struct _ModPlugFile;
	typedef struct _ModPlugFile ModPlugFile;

// END SECTION FROM modplug.h

static ModPlugFile* (*qModPlug_Load) (const void* data, int size);
static void (*qModPlug_Unload) (ModPlugFile* file);
static int (*qModPlug_Read) (ModPlugFile* file, void* buffer, int size);
static void (*qModPlug_Seek) (ModPlugFile* file, int millisecond);
static void (*qModPlug_GetSettings) (ModPlug_Settings* settings);
static void (*qModPlug_SetSettings) (const ModPlug_Settings* settings);
typedef void (ModPlug_SetMasterVolume_t) (ModPlugFile* file,unsigned int cvol) ;
ModPlug_SetMasterVolume_t *qModPlug_SetMasterVolume;


static dllfunction_t modplugfuncs[] =
{
	{"ModPlug_Load",			(void **) &qModPlug_Load},
	{"ModPlug_Unload",			(void **) &qModPlug_Unload},
	{"ModPlug_Read",			(void **) &qModPlug_Read},
	{"ModPlug_Seek",			(void **) &qModPlug_Seek},
	{"ModPlug_GetSettings",		(void **) &qModPlug_GetSettings},
	{"ModPlug_SetSettings",		(void **) &qModPlug_SetSettings},
	{NULL, NULL}
};

// Handles for the modplug and modplugfile DLLs
static dllhandle_t modplug_dll = NULL;

/*
=================================================================

  DLL load & unload

=================================================================
*/

/*
====================
ModPlug_OpenLibrary

Try to load the modplugFile DLL
====================
*/
qboolean ModPlug_OpenLibrary (void)
{
	const char* dllnames_modplug [] =
	{
#if defined(WIN32)
		"libmodplug-1.dll",
		"modplug.dll",
#elif defined(MACOSX)
		"libmodplug.dylib",
#else
		"libmodplug.so.1",
		"libmodplug.so",
#endif
		NULL
	};

	// Already loaded?
	if (modplug_dll)
		return true;

// COMMANDLINEOPTION: Sound: -nomodplug disables modplug sound support
	if (COM_CheckParm("-nomodplug"))
		return false;

	// Load the DLLs
	// We need to load both by hand because some OSes seem to not load
	// the modplug DLL automatically when loading the modplugFile DLL
	if(Sys_LoadLibrary (dllnames_modplug, &modplug_dll, modplugfuncs))
	{
		qModPlug_SetMasterVolume = (ModPlug_SetMasterVolume_t *) Sys_GetProcAddress(modplug_dll, "ModPlug_SetMasterVolume");
		if(!qModPlug_SetMasterVolume)
			Con_Print("Warning: modplug volume control not supported. Try getting a newer version of libmodplug.\n");
		return true;
	}
	else
		return false;
}


/*
====================
ModPlug_CloseLibrary

Unload the modplugFile DLL
====================
*/
void ModPlug_CloseLibrary (void)
{
	Sys_UnloadLibrary (&modplug_dll);
}
#endif


/*
=================================================================

	modplug decoding

=================================================================
*/

// Per-sfx data structure
typedef struct
{
	unsigned char	*file;
	size_t			filesize;
	snd_format_t	format;
	unsigned int	total_length;
	char			name[128];
	sfx_t           *sfx;
} modplug_stream_persfx_t;

// Per-channel data structure
typedef struct
{
	ModPlugFile 	*mf;
	unsigned int	sb_offset;
	int				bs;
	snd_buffer_t	sb;		// must be at the end due to its dynamically allocated size
} modplug_stream_perchannel_t;


/*
====================
ModPlug_FetchSound
====================
*/
static const snd_buffer_t* ModPlug_FetchSound (void *sfxfetcher, void **chfetcherpointer, unsigned int *start, unsigned int nbsampleframes)
{
	modplug_stream_perchannel_t* per_ch = (modplug_stream_perchannel_t *)*chfetcherpointer;
	modplug_stream_persfx_t* per_sfx = (modplug_stream_persfx_t *)sfxfetcher;
	snd_buffer_t* sb;
	int newlength, done, ret;
	unsigned int real_start;
	unsigned int factor;

	// If there's no fetcher structure attached to the channel yet
	if (per_ch == NULL)
	{
		size_t buff_len, memsize;
		snd_format_t sb_format;

		sb_format.speed = snd_renderbuffer->format.speed;
		sb_format.width = per_sfx->format.width;
		sb_format.channels = per_sfx->format.channels;

		buff_len = STREAM_BUFFER_SIZE(&sb_format);
		memsize = sizeof (*per_ch) - sizeof (per_ch->sb.samples) + buff_len;
		per_ch = (modplug_stream_perchannel_t *)Mem_Alloc (snd_mempool, memsize);

		// Open it with the modplugFile API
		per_ch->mf = qModPlug_Load(per_sfx->file, per_sfx->filesize);
		if (!per_ch->mf)
		{
			Con_Printf("error while reading ModPlug stream \"%s\"\n", per_sfx->name);
			Mem_Free (per_ch);
			return NULL;
		}

#ifndef SND_MODPLUG_STATIC
		if(qModPlug_SetMasterVolume)
#endif
			qModPlug_SetMasterVolume(per_ch->mf, 512); // max volume, DP scales down!

		per_ch->bs = 0;

		per_ch->sb_offset = 0;
		per_ch->sb.format = sb_format;
		per_ch->sb.nbframes = 0;
		per_ch->sb.maxframes = buff_len / (per_ch->sb.format.channels * per_ch->sb.format.width);

		*chfetcherpointer = per_ch;
	}

	real_start = *start;

	sb = &per_ch->sb;
	factor = per_sfx->format.width * per_sfx->format.channels;

	// If the stream buffer can't contain that much samples anyway
	if (nbsampleframes > sb->maxframes)
	{
		Con_Printf ("ModPlug_FetchSound: stream buffer too small (%u sample frames required)\n", nbsampleframes);
		return NULL;
	}

	// If the data we need has already been decompressed in the sfxbuffer, just return it
	if (per_ch->sb_offset <= real_start && per_ch->sb_offset + sb->nbframes >= real_start + nbsampleframes)
	{
		*start = per_ch->sb_offset;
		return sb;
	}

	newlength = (int)(per_ch->sb_offset + sb->nbframes) - real_start;

	// If we need to skip some data before decompressing the rest, or if the stream has looped
	if (newlength < 0 || per_ch->sb_offset > real_start)
	{
		unsigned int time_start;
		unsigned int modplug_start;

		/*
		MODs loop on their own, so any position is valid!
		if (real_start > (unsigned int)per_sfx->total_length)
		{
			Con_Printf ("ModPlug_FetchSound: asked for a start position after the end of the sfx! (%u > %u)\n",
						real_start, per_sfx->total_length);
			return NULL;
		}
		*/

		// We work with 200ms (1/5 sec) steps to avoid rounding errors
		time_start = real_start * 5 / snd_renderbuffer->format.speed;
		modplug_start = time_start * (1000 / 5);

		Con_DPrintf("warning: mod file needed to seek (to %d)\n", modplug_start);

		qModPlug_Seek(per_ch->mf, modplug_start);
		sb->nbframes = 0;

		real_start = (unsigned int) ((float)modplug_start / 1000 * snd_renderbuffer->format.speed);
		if (*start - real_start + nbsampleframes > sb->maxframes)
		{
			Con_Printf ("ModPlug_FetchSound: stream buffer too small after seek (%u sample frames required)\n",
						*start - real_start + nbsampleframes);
			per_ch->sb_offset = real_start;
			return NULL;
		}
	}
	// Else, move forward the samples we need to keep in the sound buffer
	else
	{
		memmove (sb->samples, sb->samples + (real_start - per_ch->sb_offset) * factor, newlength * factor);
		sb->nbframes = newlength;
	}

	per_ch->sb_offset = real_start;

	// We add more than one frame of sound to the buffer:
	// 1- to ensure we won't lose many samples during the resampling process
	// 2- to reduce calls to ModPlug_FetchSound to regulate workload
	newlength = (int)(per_sfx->format.speed*STREAM_BUFFER_FILL);
	if ((size_t) ((double) newlength * (double)sb->format.speed / (double)per_sfx->format.speed) + sb->nbframes > sb->maxframes)
	{
		Con_Printf ("ModPlug_FetchSound: stream buffer overflow (%u + %u = %u sample frames / %u)\n",
					(unsigned int) ((double) newlength * (double)sb->format.speed / (double)per_sfx->format.speed), sb->nbframes, (unsigned int) ((double) newlength * (double)sb->format.speed / (double)per_sfx->format.speed) + sb->nbframes, sb->maxframes);
		return NULL;
	}
	newlength *= factor; // convert from sample frames to bytes
	if(newlength > (int)sizeof(resampling_buffer))
		newlength = sizeof(resampling_buffer);

	// Decompress in the resampling_buffer
	done = 0;
	while ((ret = qModPlug_Read (per_ch->mf, (char *)&resampling_buffer[done], (int)(newlength - done))) > 0)
		done += ret;
	if(done < newlength)
	{
		// Argh. We didn't get as many samples as we wanted. Probably
		// libmodplug forgot what mLoopCount==-1 means... basically, this means
		// we can't loop like this. Try to let DP fix it later...
		per_sfx->sfx->total_length = (real_start + ((size_t)done / (size_t)factor));
		per_sfx->sfx->loopstart = 0;

		if(newlength != done)
			Con_DPrintf("ModPlug_Fetch: wanted: %d, got: %d\n", newlength, done);
	}

	Snd_AppendToSndBuffer (sb, resampling_buffer, (size_t)done / (size_t)factor, &per_sfx->format);

	*start = per_ch->sb_offset;
	return sb;
}


/*
====================
ModPlug_FetchEnd
====================
*/
static void ModPlug_FetchEnd (void *chfetcherdata)
{
	modplug_stream_perchannel_t* per_ch = (modplug_stream_perchannel_t *)chfetcherdata;

	if (per_ch != NULL)
	{
		// Free the modplug decoder
		qModPlug_Unload (per_ch->mf);

		Mem_Free (per_ch);
	}
}


/*
====================
ModPlug_FreeSfx
====================
*/
static void ModPlug_FreeSfx (void *sfxfetcherdata)
{
	modplug_stream_persfx_t* per_sfx = (modplug_stream_persfx_t *)sfxfetcherdata;

	// Free the modplug file
	Mem_Free(per_sfx->file);

	// Free the stream structure
	Mem_Free(per_sfx);
}


/*
====================
ModPlug_GetFormat
====================
*/
static const snd_format_t* qModPlug_GetFormat (sfx_t* sfx)
{
	modplug_stream_persfx_t* per_sfx = (modplug_stream_persfx_t *)sfx->fetcher_data;
	return &per_sfx->format;
}

static const snd_fetcher_t modplug_fetcher = { ModPlug_FetchSound, ModPlug_FetchEnd, ModPlug_FreeSfx, qModPlug_GetFormat };


/*
====================
ModPlug_LoadmodplugFile

Load an modplug file into memory
====================
*/
qboolean ModPlug_LoadModPlugFile (const char *filename, sfx_t *sfx)
{
	unsigned char *data;
	fs_offset_t filesize;
	ModPlugFile *mf;
	modplug_stream_persfx_t* per_sfx;
	ModPlug_Settings s;

	if (!modplug_dll)
		return false;

	// Already loaded?
	if (sfx->fetcher != NULL)
		return true;

	// Load the file
	data = FS_LoadFile (filename, snd_mempool, false, &filesize);
	if (data == NULL)
		return false;

	if (developer_loading.integer >= 2)
		Con_Printf ("Loading ModPlug file \"%s\"\n", filename);

	qModPlug_GetSettings(&s);
	s.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION | MODPLUG_ENABLE_REVERB;
	s.mChannels = 2;
	s.mBits = 16;
	s.mFrequency = 44100;
	s.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
	s.mLoopCount = -1;
	qModPlug_SetSettings(&s);

	// Open it with the modplugFile API
	if (!(mf = qModPlug_Load (data, filesize)))
	{
		Con_Printf ("error while opening ModPlug file \"%s\"\n", filename);
		Mem_Free(data);
		return false;
	}

#ifndef SND_MODPLUG_STATIC
	if(qModPlug_SetMasterVolume)
#endif
		qModPlug_SetMasterVolume(mf, 512); // max volume, DP scales down!

	if (developer_loading.integer >= 2)
		Con_Printf ("\"%s\" will be streamed\n", filename);
	per_sfx = (modplug_stream_persfx_t *)Mem_Alloc (snd_mempool, sizeof (*per_sfx));
	strlcpy(per_sfx->name, sfx->name, sizeof(per_sfx->name));
	sfx->memsize += sizeof (*per_sfx);
	per_sfx->file = data;
	per_sfx->filesize = filesize;
	sfx->memsize += filesize;

	per_sfx->format.speed = 44100; // modplug always works at that rate
	per_sfx->format.width = 2;  // We always work with 16 bits samples
	per_sfx->format.channels = 2; // stereo rulez ;) (MAYBE default to mono because Amiga MODs sound better then?)
	per_sfx->sfx = sfx;

	sfx->fetcher_data = per_sfx;
	sfx->fetcher = &modplug_fetcher;
	sfx->flags |= SFXFLAG_STREAMED;
	sfx->total_length = 2147384647; // they always loop
	sfx->loopstart = sfx->total_length; // modplug does it

	return true;
}
