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

#ifndef SND_MAIN_H
#define SND_MAIN_H

#include "sound.h"


typedef struct snd_format_s
{
	unsigned int	speed;
	unsigned short	width;
	unsigned short	channels;
} snd_format_t;

typedef struct snd_buffer_s
{
	snd_format_t		format;
	unsigned int		nbframes;	// current size, in sample frames
	unsigned int		maxframes;	// max size (buffer size), in sample frames
	unsigned char		samples[4];	// variable sized
} snd_buffer_t;

typedef struct snd_ringbuffer_s
{
	snd_format_t		format;
	unsigned char*		ring;
	unsigned int		maxframes;	// max size (buffer size), in sample frames
	unsigned int		startframe;	// index of the first frame in the buffer
									// if startframe == endframe, the bufffer is empty
	unsigned int		endframe;	// index of the first EMPTY frame in the "ring" buffer
									// may be smaller than startframe if the "ring" buffer has wrapped
} snd_ringbuffer_t;

// sfx_t flags
#define SFXFLAG_NONE			0
#define SFXFLAG_FILEMISSING		(1 << 0) // wasn't able to load the associated sound file
#define SFXFLAG_SERVERSOUND		(1 << 1) // the sfx is part of the server precache list
#define SFXFLAG_STREAMED		(1 << 2) // informative only. You shouldn't need to know that
#define SFXFLAG_PERMANENTLOCK	(1 << 3) // can never be freed (ex: used by the client code)

typedef struct snd_fetcher_s snd_fetcher_t;
struct sfx_s
{
	char				name[MAX_QPATH];
	sfx_t				*next;
	size_t				memsize;		// total memory used (including sfx_t and fetcher data)

										// One lock is automatically granted while the sfx is
										// playing (and removed when stopped). Locks can also be
	int					locks;			// added by S_PrecacheSound and S_ServerSounds.
										// A SFX with no lock and no SFXFLAG_PERMANENTLOCK is
										// freed at level change by S_ServerSounds.

	unsigned int		flags;			// cf SFXFLAG_* defines
	unsigned int		loopstart;		// in sample frames. equals total_length if not looped
	unsigned int		total_length;	// in sample frames
	const snd_fetcher_t	*fetcher;
	void				*fetcher_data;	// Per-sfx data for the sound fetching functions
};

// maximum supported speakers constant
#define SND_LISTENERS 8

typedef struct channel_s
{
	short			listener_volume [SND_LISTENERS];	// 0-255 volume per speaker
	int				master_vol;		// 0-255 master volume
	sfx_t			*sfx;			// sfx number
	unsigned int	flags;			// cf CHANNELFLAG_* defines
	int				pos;			// sample position in sfx, negative values delay the start of the sound playback
	int				entnum;			// to allow overriding a specific sound
	int				entchannel;
	vec3_t			origin;			// origin of sound effect
	vec_t			dist_mult;		// distance multiplier (attenuation/clipK)
	void			*fetcher_data;	// Per-channel data for the sound fetching function
} channel_t;

// Sound fetching functions
// "start" is both an input and output parameter: it returns the actual start time of the sound buffer
typedef const snd_buffer_t* (*snd_fetcher_getsb_t) (channel_t* ch, unsigned int *start, unsigned int nbsampleframes);
typedef void (*snd_fetcher_endsb_t) (channel_t* ch);
typedef void (*snd_fetcher_free_t) (sfx_t* sfx);
typedef const snd_format_t* (*snd_fetcher_getfmt_t) (sfx_t* sfx);
struct snd_fetcher_s
{
	snd_fetcher_getsb_t		getsb;
	snd_fetcher_endsb_t		endsb;
	snd_fetcher_free_t		free;
	snd_fetcher_getfmt_t	getfmt;
};

// 0 to NUM_AMBIENTS - 1 = water, etc
// NUM_AMBIENTS to NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS - 1 = normal entity sounds
// NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS to total_channels = static sounds
#define	MAX_DYNAMIC_CHANNELS	512
#define	MAX_CHANNELS			1028

extern unsigned int total_channels;
extern channel_t channels[MAX_CHANNELS];

extern snd_ringbuffer_t *snd_renderbuffer;
extern unsigned int soundtime;	// WARNING: sound modules must NOT use it

extern cvar_t _snd_mixahead;
extern cvar_t snd_swapstereo;
extern cvar_t snd_streaming;

#define SND_CHANNELLAYOUT_AUTO		0
#define SND_CHANNELLAYOUT_STANDARD	1
#define SND_CHANNELLAYOUT_ALSA		2
extern cvar_t snd_channellayout;


extern int snd_blocked;		// counter. When > 0, we stop submitting sound to the audio device

extern mempool_t *snd_mempool;

// If simsound is true, the sound card is not initialized and no sound is submitted to it.
// More generally, all arch-dependent operations are skipped or emulated.
// Used for isolating performance in the renderer.
extern qboolean simsound;


// ====================================================================
//         Architecture-independent functions
// ====================================================================

void S_PaintChannels (snd_ringbuffer_t* rb, unsigned int starttime, unsigned int endtime);

qboolean S_LoadSound (sfx_t *sfx, qboolean complain);

void S_LockSfx (sfx_t *sfx);
void S_UnlockSfx (sfx_t *sfx);

snd_buffer_t *Snd_CreateSndBuffer (const unsigned char *samples, unsigned int sampleframes, const snd_format_t* in_format, unsigned int sb_speed);
qboolean Snd_AppendToSndBuffer (snd_buffer_t* sb, const unsigned char *samples, unsigned int sampleframes, const snd_format_t* format);

// If "buffer" is NULL, the function allocates one buffer of "sampleframes" sample frames itself
// (if "sampleframes" is 0, the function chooses the size).
snd_ringbuffer_t *Snd_CreateRingBuffer (const snd_format_t* format, unsigned int sampleframes, void* buffer);


// ====================================================================
//         Architecture-dependent functions
// ====================================================================

// Create "snd_renderbuffer" with the proper sound format if the call is successful
// May return a suggested format if the requested format isn't available
qboolean SndSys_Init (const snd_format_t* requested, snd_format_t* suggested);

// Stop the sound card, delete "snd_renderbuffer" and free its other resources
void SndSys_Shutdown (void);

// Submit the contents of "snd_renderbuffer" to the sound card
void SndSys_Submit (void);

// Returns the number of sample frames consumed since the sound started
unsigned int SndSys_GetSoundTime (void);

// Get the exclusive lock on "snd_renderbuffer"
qboolean SndSys_LockRenderBuffer (void);

// Release the exclusive lock on "snd_renderbuffer"
void SndSys_UnlockRenderBuffer (void);


#endif
