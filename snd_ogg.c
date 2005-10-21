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
#include "snd_ogg.h"
#include "snd_wav.h"


/*
=================================================================

  Minimal set of definitions from the Ogg Vorbis lib
  (C) COPYRIGHT 1994-2001 by the XIPHOPHORUS Company
  http://www.xiph.org/

  WARNING: for a matter of simplicity, several pointer types are
  casted to "void*", and most enumerated values are not included

=================================================================
*/

#ifdef _MSC_VER
typedef __int64 ogg_int64_t;
#else
typedef long long ogg_int64_t;
#endif

typedef struct
{
	size_t	(*read_func)	(void *ptr, size_t size, size_t nmemb, void *datasource);
	int		(*seek_func)	(void *datasource, ogg_int64_t offset, int whence);
	int		(*close_func)	(void *datasource);
	long	(*tell_func)	(void *datasource);
} ov_callbacks;

typedef struct
{
	unsigned char	*data;
	int				storage;
	int				fill;
	int				returned;
	int				unsynced;
	int				headerbytes;
	int				bodybytes;
} ogg_sync_state;

typedef struct
{
	int		version;
	int		channels;
	long	rate;
	long	bitrate_upper;
	long	bitrate_nominal;
	long	bitrate_lower;
	long	bitrate_window;
	void	*codec_setup;
} vorbis_info;

typedef struct
{
	unsigned char	*body_data;
	long			body_storage;
	long			body_fill;
	long			body_returned;
	int				*lacing_vals;
	ogg_int64_t		*granule_vals;
	long			lacing_storage;
	long			lacing_fill;
	long			lacing_packet;
	long			lacing_returned;
	unsigned char	header[282];
	int				header_fill;
	int				e_o_s;
	int				b_o_s;
	long			serialno;
	long			pageno;
	ogg_int64_t		packetno;
	ogg_int64_t		granulepos;
} ogg_stream_state;

typedef struct
{
	int			analysisp;
	vorbis_info	*vi;
	float		**pcm;
	float		**pcmret;
	int			pcm_storage;
	int			pcm_current;
	int			pcm_returned;
	int			preextrapolate;
	int			eofflag;
	long		lW;
	long		W;
	long		nW;
	long		centerW;
	ogg_int64_t	granulepos;
	ogg_int64_t	sequence;
	ogg_int64_t	glue_bits;
	ogg_int64_t	time_bits;
	ogg_int64_t	floor_bits;
	ogg_int64_t	res_bits;
	void		*backend_state;
} vorbis_dsp_state;

typedef struct
{
	long			endbyte;
	int				endbit;
	unsigned char	*buffer;
	unsigned char	*ptr;
	long			storage;
} oggpack_buffer;

typedef struct
{
	float				**pcm;
	oggpack_buffer		opb;
	long				lW;
	long				W;
	long				nW;
	int					pcmend;
	int					mode;
	int					eofflag;
	ogg_int64_t			granulepos;
	ogg_int64_t			sequence;
	vorbis_dsp_state	*vd;
	void				*localstore;
	long				localtop;
	long				localalloc;
	long				totaluse;
	void				*reap;  // VOIDED POINTER
	long				glue_bits;
	long				time_bits;
	long				floor_bits;
	long				res_bits;
	void				*internal;
} vorbis_block;

typedef struct
{
	void				*datasource;
	int					seekable;
	ogg_int64_t			offset;
	ogg_int64_t			end;
	ogg_sync_state		oy;
	int					links;
	ogg_int64_t			*offsets;
	ogg_int64_t			*dataoffsets;
	long				*serialnos;
	ogg_int64_t			*pcmlengths;
	vorbis_info			*vi;
	void				*vc;  // VOIDED POINTER
	ogg_int64_t			pcm_offset;
	int					ready_state;
	long				current_serialno;
	int					current_link;
	double				bittrack;
	double				samptrack;
	ogg_stream_state	os;
	vorbis_dsp_state	vd;
	vorbis_block		vb;
	ov_callbacks		callbacks;
} OggVorbis_File;


/*
=================================================================

  DarkPlaces definitions

=================================================================
*/

// Functions exported from the vorbisfile library
static int (*qov_clear) (OggVorbis_File *vf);
static vorbis_info* (*qov_info) (OggVorbis_File *vf,int link);
static int (*qov_open_callbacks) (void *datasource, OggVorbis_File *vf,
								  char *initial, long ibytes,
								  ov_callbacks callbacks);
static int (*qov_pcm_seek) (OggVorbis_File *vf,ogg_int64_t pos);
static ogg_int64_t (*qov_pcm_total) (OggVorbis_File *vf,int i);
static long (*qov_read) (OggVorbis_File *vf,char *buffer,int length,
						 int bigendianp,int word,int sgned,int *bitstream);

static dllfunction_t oggvorbisfuncs[] =
{
	{"ov_clear",			(void **) &qov_clear},
	{"ov_info",				(void **) &qov_info},
	{"ov_open_callbacks",	(void **) &qov_open_callbacks},
	{"ov_pcm_seek",			(void **) &qov_pcm_seek},
	{"ov_pcm_total",		(void **) &qov_pcm_total},
	{"ov_read",				(void **) &qov_read},
	{NULL, NULL}
};

// Handles for the Vorbis and Vorbisfile DLLs
static dllhandle_t vo_dll = NULL;
static dllhandle_t vf_dll = NULL;

typedef struct
{
	qbyte *buffer;
	ogg_int64_t ind, buffsize;
} ov_decode_t;


static size_t ovcb_read (void *ptr, size_t size, size_t nb, void *datasource)
{
	ov_decode_t *ov_decode = (ov_decode_t*)datasource;
	size_t remain, len;

	remain = ov_decode->buffsize - ov_decode->ind;
	len = size * nb;
	if (remain < len)
		len = remain - remain % size;

	memcpy (ptr, ov_decode->buffer + ov_decode->ind, len);
	ov_decode->ind += len;

	return len / size;
}

static int ovcb_seek (void *datasource, ogg_int64_t offset, int whence)
{
	ov_decode_t *ov_decode = (ov_decode_t*)datasource;

	switch (whence)
	{
		case SEEK_SET:
			break;
		case SEEK_CUR:
			offset += ov_decode->ind;
			break;
		case SEEK_END:
			offset += ov_decode->buffsize;
			break;
		default:
			return -1;
	}
	if (offset < 0 || offset > ov_decode->buffsize)
		return -1;

	ov_decode->ind = offset;
	return 0;
}

static int ovcb_close (void *ov_decode)
{
	return 0;
}

static long ovcb_tell (void *ov_decode)
{
	return ((ov_decode_t*)ov_decode)->ind;
}


/*
=================================================================

  DLL load & unload

=================================================================
*/

/*
====================
OGG_OpenLibrary

Try to load the VorbisFile DLL
====================
*/
qboolean OGG_OpenLibrary (void)
{
	const char* dllnames_vo [] =
	{
#if defined(WIN64)
		"vorbis64.dll",
#elif defined(WIN32)
		"vorbis.dll",
#elif defined(MACOSX)
		"libvorbis.dylib",
#else
		"libvorbis.so.0",
		"libvorbis.so",
#endif
		NULL
	};
	const char* dllnames_vf [] =
	{
#if defined(WIN64)
		"vorbisfile64.dll",
#elif defined(WIN32)
		"vorbisfile.dll",
#elif defined(MACOSX)
		"libvorbisfile.dylib",
#else
		"libvorbisfile.so.3",
		"libvorbisfile.so",
#endif
		NULL
	};

	// Already loaded?
	if (vf_dll)
		return true;

// COMMANDLINEOPTION: Sound: -novorbis disables ogg vorbis sound support
	if (COM_CheckParm("-novorbis"))
		return false;

	// Load the DLLs
	// We need to load both by hand because some OSes seem to not load
	// the vorbis DLL automatically when loading the VorbisFile DLL
	if (! Sys_LoadLibrary (dllnames_vo, &vo_dll, NULL) ||
		! Sys_LoadLibrary (dllnames_vf, &vf_dll, oggvorbisfuncs))
	{
		Sys_UnloadLibrary (&vo_dll);
		Con_Printf ("Ogg Vorbis support disabled\n");
		return false;
	}

	Con_Printf ("Ogg Vorbis support enabled\n");
	return true;
}


/*
====================
OGG_CloseLibrary

Unload the VorbisFile DLL
====================
*/
void OGG_CloseLibrary (void)
{
	Sys_UnloadLibrary (&vf_dll);
	Sys_UnloadLibrary (&vo_dll);
}


/*
=================================================================

	Ogg Vorbis decoding

=================================================================
*/

#define STREAM_BUFFER_DURATION 1.5f	// 1.5 sec
#define STREAM_BUFFER_SIZE(format_ptr) (ceil (STREAM_BUFFER_DURATION * ((format_ptr)->speed * (format_ptr)->width * (format_ptr)->channels)))

// We work with 1 sec sequences, so this buffer must be able to contain
// 1 sec of sound of the highest quality (48 KHz, 16 bit samples, stereo)
static qbyte resampling_buffer [48000 * 2 * 2];


// Per-sfx data structure
typedef struct
{
	qbyte			*file;
	size_t			filesize;
	snd_format_t	format;
} ogg_stream_persfx_t;

// Per-channel data structure
typedef struct
{
	OggVorbis_File	vf;
	ov_decode_t		ov_decode;
	int				bs;
	sfxbuffer_t		sb;		// must be at the end due to its dynamically allocated size
} ogg_stream_perchannel_t;


static const ov_callbacks callbacks = {ovcb_read, ovcb_seek, ovcb_close, ovcb_tell};

/*
====================
OGG_FetchSound
====================
*/
static const sfxbuffer_t* OGG_FetchSound (channel_t* ch, unsigned int start, unsigned int nbsamples)
{
	ogg_stream_perchannel_t* per_ch;
	sfxbuffer_t* sb;
	sfx_t* sfx;
	snd_format_t* format;
	ogg_stream_persfx_t* per_sfx;
	int newlength, done, ret, bigendian;
	unsigned int factor;
	size_t buff_len;

	per_ch = (ogg_stream_perchannel_t *)ch->fetcher_data;
	sfx = ch->sfx;
	per_sfx = (ogg_stream_persfx_t *)sfx->fetcher_data;
	format = &sfx->format;
	buff_len = STREAM_BUFFER_SIZE(format);

	// If there's no fetcher structure attached to the channel yet
	if (per_ch == NULL)
	{
		size_t memsize;
		ogg_stream_persfx_t* per_sfx;

		memsize = sizeof (*per_ch) - sizeof (per_ch->sb.data) + buff_len;
		per_ch = (ogg_stream_perchannel_t *)Mem_Alloc (snd_mempool, memsize);
		sfx->memsize += memsize;
		per_sfx = (ogg_stream_persfx_t *)sfx->fetcher_data;

		// Open it with the VorbisFile API
		per_ch->ov_decode.buffer = per_sfx->file;
		per_ch->ov_decode.ind = 0;
		per_ch->ov_decode.buffsize = per_sfx->filesize;
		if (qov_open_callbacks (&per_ch->ov_decode, &per_ch->vf, NULL, 0, callbacks) < 0)
		{
			Con_Printf("error while reading Ogg Vorbis stream \"%s\"\n", sfx->name);
			Mem_Free (per_ch);
			return NULL;
		}

		per_ch->sb.offset = 0;
		per_ch->sb.length = 0;
		per_ch->bs = 0;

		ch->fetcher_data = per_ch;
	}

	sb = &per_ch->sb;
	factor = per_sfx->format.width * per_sfx->format.channels;

	// If the stream buffer can't contain that much samples anyway
	if (nbsamples * factor > buff_len)
	{
		Con_Printf ("OGG_FetchSound: stream buffer too small (%u bytes required)\n", nbsamples * factor);
		return NULL;
	}

	// If the data we need has already been decompressed in the sfxbuffer, just return it
	if (sb->offset <= start && sb->offset + sb->length >= start + nbsamples)
		return sb;

	newlength = (int)(sb->offset + sb->length) - start;

	// If we need to skip some data before decompressing the rest, or if the stream has looped
	if (newlength < 0 || sb->offset > start)
	{
		if (qov_pcm_seek (&per_ch->vf, (ogg_int64_t)start) != 0)
			return NULL;
		sb->length = 0;
	}
	// Else, move forward the samples we need to keep in the sfxbuffer
	else
	{
		memmove (sb->data, sb->data + (start - sb->offset) * factor, newlength * factor);
		sb->length = newlength;
	}

	sb->offset = start;

	// We add exactly 1 sec of sound to the buffer:
	// 1- to ensure we won't lose any sample during the resampling process
	// 2- to force one call to OGG_FetchSound per second to regulate the workload
	if ((sfx->format.speed + sb->length) * factor > buff_len)
	{
		Con_Printf ("OGG_FetchSound: stream buffer overflow (%u bytes / %u)\n",
					(sfx->format.speed + sb->length) * factor, buff_len);
		return NULL;
	}
	newlength = per_sfx->format.speed * factor;  // -> 1 sec of sound before resampling

	// Decompress in the resampling_buffer
#if BYTE_ORDER == BIG_ENDIAN
	bigendian = 1;
#else
	bigendian = 0;
#endif
	done = 0;
	while ((ret = qov_read (&per_ch->vf, (char *)&resampling_buffer[done], (int)(newlength - done), bigendian, 2, 1, &per_ch->bs)) > 0)
		done += ret;

	// Resample in the sfxbuffer
	newlength = (int)ResampleSfx (resampling_buffer, (size_t)done / (size_t)factor, &per_sfx->format, sb->data + sb->length * (size_t)factor, sfx->name);
	sb->length += newlength;

	return sb;
}


/*
====================
OGG_FetchEnd
====================
*/
static void OGG_FetchEnd (channel_t* ch)
{
	ogg_stream_perchannel_t* per_ch;

	per_ch = (ogg_stream_perchannel_t *)ch->fetcher_data;
	if (per_ch != NULL)
	{
		size_t buff_len;
		snd_format_t* format;

		// Free the ogg vorbis decoder
		qov_clear (&per_ch->vf);

		Mem_Free (per_ch);
		ch->fetcher_data = NULL;

		format = &ch->sfx->format;
		buff_len = STREAM_BUFFER_SIZE(format);
		ch->sfx->memsize -= sizeof (*per_ch) - sizeof (per_ch->sb.data) + buff_len;
	}
}


/*
====================
OGG_FreeSfx
====================
*/
static void OGG_FreeSfx (sfx_t* sfx)
{
	ogg_stream_persfx_t* per_sfx = (ogg_stream_persfx_t *)sfx->fetcher_data;

	// Free the Ogg Vorbis file
	Mem_Free(per_sfx->file);
	sfx->memsize -= per_sfx->filesize;

	// Free the stream structure
	Mem_Free(per_sfx);
	sfx->memsize -= sizeof (*per_sfx);

	sfx->fetcher_data = NULL;
	sfx->fetcher = NULL;
}

static const snd_fetcher_t ogg_fetcher = { OGG_FetchSound, OGG_FetchEnd, OGG_FreeSfx };


/*
====================
OGG_LoadVorbisFile

Load an Ogg Vorbis file into memory
====================
*/
qboolean OGG_LoadVorbisFile (const char *filename, sfx_t *s)
{
	qbyte *data;
	ov_decode_t ov_decode;
	OggVorbis_File vf;
	vorbis_info *vi;
	ogg_int64_t len, buff_len;

	if (!vf_dll)
		return false;

	// Already loaded?
	if (s->fetcher != NULL)
		return true;

	// Load the file
	data = FS_LoadFile (filename, snd_mempool, false);
	if (data == NULL)
		return false;

	Con_DPrintf ("Loading Ogg Vorbis file \"%s\"\n", filename);

	// Open it with the VorbisFile API
	ov_decode.buffer = data;
	ov_decode.ind = 0;
	ov_decode.buffsize = fs_filesize;
	if (qov_open_callbacks (&ov_decode, &vf, NULL, 0, callbacks) < 0)
	{
		Con_Printf ("error while opening Ogg Vorbis file \"%s\"\n", filename);
		Mem_Free(data);
		return false;
	}

	// Get the stream information
	vi = qov_info (&vf, -1);
	if (vi->channels < 1 || vi->channels > 2)
	{
		Con_Printf("%s has an unsupported number of channels (%i)\n",
					s->name, vi->channels);
		qov_clear (&vf);
		Mem_Free(data);
		return false;
	}

	len = qov_pcm_total (&vf, -1) * vi->channels * 2;  // 16 bits => "* 2"

	// Decide if we go for a stream or a simple PCM cache
	buff_len = ceil (STREAM_BUFFER_DURATION * (shm->format.speed * 2 * vi->channels));
	if (snd_streaming.integer && len > (ogg_int64_t)fs_filesize + 3 * buff_len)
	{
		ogg_stream_persfx_t* per_sfx;

		Con_DPrintf ("\"%s\" will be streamed\n", filename);
		per_sfx = (ogg_stream_persfx_t *)Mem_Alloc (snd_mempool, sizeof (*per_sfx));
		s->memsize += sizeof (*per_sfx);
		per_sfx->file = data;
		per_sfx->filesize = fs_filesize;
		s->memsize += fs_filesize;

		per_sfx->format.speed = vi->rate;
		per_sfx->format.width = 2;  // We always work with 16 bits samples
		per_sfx->format.channels = vi->channels;
		s->format.speed = shm->format.speed;
		s->format.width = per_sfx->format.width;
		s->format.channels = per_sfx->format.channels;

		s->fetcher_data = per_sfx;
		s->fetcher = &ogg_fetcher;
		s->loopstart = -1;
		s->flags |= SFXFLAG_STREAMED;
		s->total_length = (size_t)len / per_sfx->format.channels / 2 * ((float)s->format.speed / per_sfx->format.speed);
	}
	else
	{
		char *buff;
		ogg_int64_t done;
		int bs, bigendian;
		long ret;
		sfxbuffer_t *sb;
		size_t memsize;

		Con_DPrintf ("\"%s\" will be cached\n", filename);

		// Decode it
		buff = (char *)Mem_Alloc (snd_mempool, (int)len);
		done = 0;
		bs = 0;
#if BYTE_ORDER == LITTLE_ENDIAN
		bigendian = 0;
#else
		bigendian = 1;
#endif
		while ((ret = qov_read (&vf, &buff[done], (int)(len - done), bigendian, 2, 1, &bs)) > 0)
			done += ret;

		// Calculate resampled length
		len = (double)done * (double)shm->format.speed / (double)vi->rate;

		// Resample it
		memsize = (size_t)len + sizeof (*sb) - sizeof (sb->data);
		sb = (sfxbuffer_t *)Mem_Alloc (snd_mempool, memsize);
		s->memsize += memsize;
		s->fetcher_data = sb;
		s->fetcher = &wav_fetcher;
		s->format.speed = vi->rate;
		s->format.width = 2;  // We always work with 16 bits samples
		s->format.channels = vi->channels;
		s->loopstart = -1;
		s->flags &= ~SFXFLAG_STREAMED;

		sb->length = (unsigned int)ResampleSfx ((qbyte *)buff, (size_t)done / (vi->channels * 2), &s->format, sb->data, s->name);
		s->format.speed = shm->format.speed;
		s->total_length = sb->length;
		sb->offset = 0;

		qov_clear (&vf);
		Mem_Free (data);
		Mem_Free (buff);
	}

	return true;
}
