/*
	Copyright (C) 2003  Mathieu Olivier

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
#include "snd_ogg.h"


extern void ResampleSfx (sfxcache_t *sc, qbyte *data, char *name);


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
static ogg_int64_t (*qov_pcm_total) (OggVorbis_File *vf,int i);
static long (*qov_read) (OggVorbis_File *vf,char *buffer,int length,
						 int bigendianp,int word,int sgned,int *bitstream);

static dllfunction_t oggvorbisfuncs[] =
{
	{"ov_clear",			(void **) &qov_clear},
	{"ov_info",				(void **) &qov_info},
	{"ov_open_callbacks",	(void **) &qov_open_callbacks},
	{"ov_pcm_total",		(void **) &qov_pcm_total},
	{"ov_read",				(void **) &qov_read},
	{NULL, NULL}
};

// Handle for the Vorbisfile DLL
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
	const char* dllname;
	const dllfunction_t *func;

	// Already loaded?
	if (vf_dll)
		return true;

#ifdef WIN32
	dllname = "vorbisfile.dll";
#else
	dllname = "libvorbisfile.so";
#endif

	// Initializations
	for (func = oggvorbisfuncs; func && func->name != NULL; func++)
		*func->funcvariable = NULL;

	// Load the DLL
	if (! (vf_dll = Sys_LoadLibrary (dllname)))
	{
		Con_DPrintf("Can't find %s. Ogg Vorbis support disabled\n", dllname);
		return false;
	}

	// Get the function adresses
	for (func = oggvorbisfuncs; func && func->name != NULL; func++)
		if (!(*func->funcvariable = (void *) Sys_GetProcAddress (vf_dll, func->name)))
		{
			Con_Printf("missing function \"%s\" - broken Ogg Vorbis library!\n", func->name);
			OGG_CloseLibrary ();
			return false;
		}

	Con_DPrintf("%s loaded. Ogg Vorbis support enabled\n", dllname);
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
	if (!vf_dll)
		return;

	Sys_UnloadLibrary (vf_dll);
	vf_dll = NULL;
}


/*
=================================================================

	Ogg Vorbis decoding

=================================================================
*/

/*
====================
OGG_LoadVorbisFile

Load an Ogg Vorbis file into a sfxcache_t
====================
*/
sfxcache_t *OGG_LoadVorbisFile (const char *filename, sfx_t *s)
{
	qbyte *data;
	ov_decode_t ov_decode;
	OggVorbis_File vf;
	ov_callbacks callbacks = {ovcb_read, ovcb_seek, ovcb_close, ovcb_tell};
	vorbis_info *vi;
	ogg_int64_t len;
	char *buff;
	ogg_int64_t done;
	int bs, bigendian;
	long ret;
	sfxcache_t *sc;

	if (!vf_dll)
		return NULL;

	// Load the file
	data = FS_LoadFile (filename, false);
	if (data == NULL)
		return NULL;

	// Open it with the VorbisFile API
	ov_decode.buffer = data;
	ov_decode.ind = 0;
	ov_decode.buffsize = fs_filesize;
	if (qov_open_callbacks (&ov_decode, &vf, NULL, 0, callbacks) < 0)
	{
		Con_Printf("error while opening Ogg Vorbis file \"%s\"\n", filename);
		Mem_Free (data);
		return NULL;
	}

	// Get the stream information
	vi = qov_info (&vf, -1);
	if (vi->channels < 1 || vi->channels > 2)
	{
		Con_Printf("%s has an unsupported number of channels (%i)\n",
					s->name, vi->channels);
		qov_clear (&vf);
		Mem_Free (data);
		return NULL;
	}

	// Decode it
	len = qov_pcm_total (&vf, -1) * vi->channels * 2;  // 16 bits => "* 2"
	buff = Mem_Alloc (tempmempool, (int)len);
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
	len = (double)done * (double)shm->speed / (double)vi->rate;

	// Resample it
	Mem_FreePool (&s->mempool);
	s->mempool = Mem_AllocPool (s->name);
	sc = s->sfxcache = Mem_Alloc (s->mempool, (int)len + sizeof (sfxcache_t));
	if (sc != NULL)
	{
		sc->length = (int)done / (vi->channels * 2);
		sc->loopstart = -1;
		sc->speed = vi->rate;
		sc->width = 2;  // We always work with 16 bits samples
		sc->stereo = (vi->channels == 2);

		ResampleSfx (sc, buff, s->name);
	}
	else
	{
		Con_Printf("failed to allocate memory for sound \"%s\"\n", s->name);
		Mem_FreePool (&s->mempool);
	}

	qov_clear (&vf);
	Mem_Free (buff);
	Mem_Free (data);

	return sc;
}
