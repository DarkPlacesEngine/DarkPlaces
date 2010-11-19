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
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/


#include "quakedef.h"
#include "snd_main.h"
#include "snd_wav.h"


typedef struct wavinfo_s
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;


static unsigned char *data_p;
static unsigned char *iff_end;
static unsigned char *last_chunk;
static unsigned char *iff_data;
static int iff_chunk_len;


static short GetLittleShort(void)
{
	short val;

	val = BuffLittleShort (data_p);
	data_p += 2;

	return val;
}

static int GetLittleLong(void)
{
	int val = 0;

	val = BuffLittleLong (data_p);
	data_p += 4;

	return val;
}

static void FindNextChunk(const char *name)
{
	while (1)
	{
		data_p=last_chunk;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}
		if (data_p + iff_chunk_len > iff_end)
		{
			// truncated chunk!
			data_p = NULL;
			return;
		}
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((const char *)data_p, name, 4))
			return;
	}
}

static void FindChunk(const char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


/*
static void DumpChunks(void)
{
	char str[5];

	str[4] = 0;
	data_p=iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}
*/


/*
============
GetWavinfo
============
*/
static wavinfo_t GetWavinfo (char *name, unsigned char *wav, int wavlength)
{
	wavinfo_t info;
	int i;
	int format;
	int samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((const char *)data_p+8, "WAVE", 4)))
	{
		Con_Print("Missing RIFF/WAVE chunks\n");
		return info;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;
	//DumpChunks ();

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Print("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Con_Print("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

	// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp ((const char *)data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
			}
		}
	}
	else
		info.loopstart = -1;

	// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Print("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width / info.channels;

	if (info.samples)
	{
		if (samples < info.samples)
		{
			Con_Printf ("Sound %s has a bad loop length\n", name);
			info.samples = samples;
		}
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}


/*
====================
WAV_FetchSound
====================
*/
static const snd_buffer_t* WAV_FetchSound (void *sfxfetcher, void **chfetcherpointer, unsigned int *start, unsigned int nbsampleframes)
{
	*start = 0;
	return (snd_buffer_t *)sfxfetcher;
}

/*
====================
WAV_FreeSfx
====================
*/
static void WAV_FreeSfx (void *sfxfetcherdata)
{
	snd_buffer_t* sb = (snd_buffer_t *)sfxfetcherdata;
	// Free the sound buffer
	Mem_Free(sb);
}

/*
====================
WAV_GetFormat
====================
*/
static const snd_format_t* WAV_GetFormat (sfx_t* sfx)
{
	snd_buffer_t* sb = (snd_buffer_t *)sfx->fetcher_data;
	return &sb->format;
}

const snd_fetcher_t wav_fetcher = { WAV_FetchSound, NULL, WAV_FreeSfx, WAV_GetFormat };


/*
==============
S_LoadWavFile
==============
*/
qboolean S_LoadWavFile (const char *filename, sfx_t *sfx)
{
	fs_offset_t filesize;
	unsigned char *data;
	wavinfo_t info;
	snd_format_t wav_format;
	snd_buffer_t* sb;

	// Already loaded?
	if (sfx->fetcher != NULL)
		return true;

	// Load the file
	data = FS_LoadFile(filename, snd_mempool, false, &filesize);
	if (!data)
		return false;

	// Don't try to load it if it's not a WAV file
	if (memcmp (data, "RIFF", 4) || memcmp (data + 8, "WAVE", 4))
	{
		Mem_Free(data);
		return false;
	}

	if (developer_loading.integer >= 2)
		Con_Printf ("Loading WAV file \"%s\"\n", filename);

	info = GetWavinfo (sfx->name, data, (int)filesize);
	if (info.channels < 1 || info.channels > 2)  // Stereo sounds are allowed (intended for music)
	{
		Con_Printf("%s has an unsupported number of channels (%i)\n",sfx->name, info.channels);
		Mem_Free(data);
		return false;
	}
	//if (info.channels == 2)
	//	Log_Printf("stereosounds.log", "%s\n", sfx->name);

	// We must convert the WAV data from little endian
	// to the machine endianess before resampling it
	if (info.width == 2 && mem_bigendian)
	{
		unsigned int len, i;
		short* ptr;

		len = info.samples * info.channels;
		ptr = (short*)(data + info.dataofs);
		for (i = 0; i < len; i++)
			ptr[i] = LittleShort (ptr[i]);
	}

	wav_format.speed = info.rate;
	wav_format.width = info.width;
	wav_format.channels = info.channels;
	sb = Snd_CreateSndBuffer (data + info.dataofs, info.samples, &wav_format, snd_renderbuffer->format.speed);
	if (sb == NULL)
	{
		Mem_Free(data);
		return false;
	}
	sfx->fetcher = &wav_fetcher;
	sfx->fetcher_data = sb;

	sfx->total_length = sb->nbframes;
	sfx->memsize += sb->maxframes * sb->format.channels * sb->format.width + sizeof (*sb) - sizeof (sb->samples);

	if (info.loopstart < 0)
		sfx->loopstart = sfx->total_length;
	else
		sfx->loopstart = (unsigned int) ((double)info.loopstart * (double)sb->format.speed / (double)info.rate);
	sfx->loopstart = min(sfx->loopstart, sfx->total_length);
	sfx->flags &= ~SFXFLAG_STREAMED;

	Mem_Free (data);
	return true;
}
