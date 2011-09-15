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
} modplug_stream_persfx_t;

// Per-channel data structure
typedef struct
{
	ModPlugFile 	*mf;
	int				bs;
	int				buffer_firstframe;
	int				buffer_numframes;
	unsigned char	buffer[STREAM_BUFFERSIZE*4];
} modplug_stream_perchannel_t;


/*
====================
ModPlug_GetSamplesFloat
====================
*/
static void ModPlug_GetSamplesFloat(channel_t *ch, sfx_t *sfx, int firstsampleframe, int numsampleframes, float *outsamplesfloat)
{
	modplug_stream_perchannel_t* per_ch = (modplug_stream_perchannel_t *)ch->fetcher_data;
	modplug_stream_persfx_t* per_sfx = (modplug_stream_persfx_t *)sfx->fetcher_data;
	int newlength, done, ret;
	int f = sfx->format.width * sfx->format.channels; // bytes per frame
	short *buf;
	int i, len;

	// If there's no fetcher structure attached to the channel yet
	if (per_ch == NULL)
	{
		per_ch = (modplug_stream_perchannel_t *)Mem_Alloc(snd_mempool, sizeof(*per_ch));

		// Open it with the modplugFile API
		per_ch->mf = qModPlug_Load(per_sfx->file, per_sfx->filesize);
		if (!per_ch->mf)
		{
			// we can't call Con_Printf here, not thread safe
//			Con_Printf("error while reading ModPlug stream \"%s\"\n", per_sfx->name);
			Mem_Free(per_ch);
			return;
		}

#ifndef SND_MODPLUG_STATIC
		if(qModPlug_SetMasterVolume)
#endif
			qModPlug_SetMasterVolume(per_ch->mf, 512); // max volume, DP scales down!

		per_ch->bs = 0;

		per_ch->buffer_firstframe = 0;
		per_ch->buffer_numframes = 0;
		ch->fetcher_data = per_ch;
	}

	// if the request is too large for our buffer, loop...
	while (numsampleframes * f > (int)sizeof(per_ch->buffer))
	{
		done = sizeof(per_ch->buffer) / f;
		ModPlug_GetSamplesFloat(ch, sfx, firstsampleframe, done, outsamplesfloat);
		firstsampleframe += done;
		numsampleframes -= done;
		outsamplesfloat += done * sfx->format.channels;
	}

	// seek if the request is before the current buffer (loop back)
	// seek if the request starts beyond the current buffer by at least one frame (channel was zero volume for a while)
	// do not seek if the request overlaps the buffer end at all (expected behavior)
	if (per_ch->buffer_firstframe > firstsampleframe || per_ch->buffer_firstframe + per_ch->buffer_numframes < firstsampleframe)
	{
		// we expect to decode forward from here so this will be our new buffer start
		per_ch->buffer_firstframe = firstsampleframe;
		per_ch->buffer_numframes = 0;
		// we don't actually seek - we don't care much about timing on silent mod music streams and looping never happens
		//qModPlug_Seek(per_ch->mf, firstsampleframe * 1000.0 / sfx->format.speed);
	}

	// decompress the file as needed
	if (firstsampleframe + numsampleframes > per_ch->buffer_firstframe + per_ch->buffer_numframes)
	{
		// first slide the buffer back, discarding any data preceding the range we care about
		int offset = firstsampleframe - per_ch->buffer_firstframe;
		int keeplength = per_ch->buffer_numframes - offset;
		if (keeplength > 0)
			memmove(per_ch->buffer, per_ch->buffer + offset * sfx->format.width * sfx->format.channels, keeplength * sfx->format.width * sfx->format.channels);
		per_ch->buffer_firstframe = firstsampleframe;
		per_ch->buffer_numframes -= offset;
		// decompress as much as we can fit in the buffer
		newlength = sizeof(per_ch->buffer) - per_ch->buffer_numframes * f;
		done = 0;
		while (newlength > done && (ret = qModPlug_Read(per_ch->mf, (void *)((unsigned char *)per_ch->buffer + done), (int)(newlength - done))) > 0)
			done += ret;
		// clear the missing space if any
		if (done < newlength)
		{
			memset(per_ch->buffer + done, 0, newlength - done);
			// Argh. We didn't get as many samples as we wanted. Probably
			// libmodplug forgot what mLoopCount==-1 means... basically, this means
			// we can't loop like this. Try to let DP fix it later...
			sfx->total_length = firstsampleframe + done / f;
			sfx->loopstart = 0;
			// can't Con_Printf from this thread
			//if (newlength != done)
			//	Con_DPrintf("ModPlug_Fetch: wanted: %d, got: %d\n", newlength, done);
		}
		// we now have more data in the buffer
		per_ch->buffer_numframes += done / f;
	}

	// convert the sample format for the caller
	buf = (short *)(per_ch->buffer + (firstsampleframe - per_ch->buffer_firstframe) * f);
	len = numsampleframes * sfx->format.channels;
	for (i = 0;i < len;i++)
		outsamplesfloat[i] = buf[i] * (1.0f / 32768.0f);
}


/*
====================
ModPlug_StopChannel
====================
*/
static void ModPlug_StopChannel(channel_t *ch)
{
	modplug_stream_perchannel_t *per_ch = (modplug_stream_perchannel_t *)ch->fetcher_data;

	if (per_ch != NULL)
	{
		// Free the modplug decoder
		qModPlug_Unload(per_ch->mf);

		Mem_Free(per_ch);
	}
}


/*
====================
ModPlug_FreeSfx
====================
*/
static void ModPlug_FreeSfx (sfx_t *sfx)
{
	modplug_stream_persfx_t* per_sfx = (modplug_stream_persfx_t *)sfx->fetcher_data;

	// Free the modplug file
	Mem_Free(per_sfx->file);

	// Free the stream structure
	Mem_Free(per_sfx);
}


static const snd_fetcher_t modplug_fetcher = { ModPlug_GetSamplesFloat, ModPlug_StopChannel, ModPlug_FreeSfx };


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
	per_sfx->file = data;
	per_sfx->filesize = filesize;
	sfx->memsize += sizeof(*per_sfx);
	sfx->memsize += filesize;
	sfx->format.speed = 44100; // modplug always works at that rate
	sfx->format.width = 2;  // We always work with 16 bits samples
	sfx->format.channels = 2; // stereo rulez ;) (MAYBE default to mono because Amiga MODs sound better then?)
	sfx->fetcher_data = per_sfx;
	sfx->fetcher = &modplug_fetcher;
	sfx->flags |= SFXFLAG_STREAMED;
	sfx->total_length = 1<<30; // 2147384647; // they always loop (FIXME this breaks after 6 hours, we need support for a real "infinite" value!)
	sfx->loopstart = sfx->total_length; // modplug does it

	return true;
}
