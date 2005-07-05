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


typedef struct
{
	size_t	length;
	size_t	offset;
	qbyte	data[4];	// variable sized
} sfxbuffer_t;

typedef struct
{
	unsigned int	speed;
	unsigned int	width;
	unsigned int	channels;
} snd_format_t;

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
	mempool_t			*mempool;
	int					locks;			// One lock is automatically granted while the sfx is
										// playing (and removed when stopped). Locks can also be
										// added by S_PrecacheSound and S_ServerSounds.
										// A SFX with no lock and no SFXFLAG_PERMANENTLOCK is
										// freed at level change by S_ServerSounds.
	unsigned int		flags;			// cf SFXFLAG_* defines
	snd_format_t		format;
	int					loopstart;
	size_t				total_length;
	const snd_fetcher_t	*fetcher;
	void				*fetcher_data;	// Per-sfx data for the sound fetching functions
};

typedef struct
{
	snd_format_t	format;
	int				samples;		// mono samples in buffer
	int				samplepos;		// in mono samples
	unsigned char	*buffer;
	int				bufferlength;	// used only by certain drivers
} dma_t;

typedef struct
{
	sfx_t			*sfx;			// sfx number
	unsigned int	flags;			// cf CHANNELFLAG_* defines
	int				master_vol;		// 0-255 master volume
	int				leftvol;		// 0-255 volume
	int				rightvol;		// 0-255 volume
	int				end;			// end time in global paintsamples
	int				lastptime;		// last time this channel was painted
	int				pos;			// sample position in sfx
	int				entnum;			// to allow overriding a specific sound
	int				entchannel;
	vec3_t			origin;			// origin of sound effect
	vec_t			dist_mult;		// distance multiplier (attenuation/clipK)
	void			*fetcher_data;	// Per-channel data for the sound fetching function
} channel_t;

typedef const sfxbuffer_t* (*snd_fetcher_getsb_t) (channel_t* ch, unsigned int start, unsigned int nbsamples);
typedef void (*snd_fetcher_end_t) (channel_t* ch);
struct snd_fetcher_s
{
	snd_fetcher_getsb_t		getsb;
	snd_fetcher_end_t		end;
};

void S_PaintChannels(int endtime);

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int SNDDMA_GetDMAPos(void);

void SNDDMA_Submit(void);

// shutdown the DMA xfer.
void SNDDMA_Shutdown(void);

qboolean S_LoadSound (sfx_t *s, qboolean complain);
void S_UnloadSound(sfx_t *s);

void S_LockSfx (sfx_t *sfx);
void S_UnlockSfx (sfx_t *sfx);

void *S_LockBuffer(void);
void S_UnlockBuffer(void);

extern size_t ResampleSfx (const qbyte *in_data, size_t in_length, const snd_format_t* in_format, qbyte *out_data, const char* sfxname);

// ====================================================================
// User-setable variables
// ====================================================================

// 0 to NUM_AMBIENTS - 1 = water, etc
// NUM_AMBIENTS to NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS - 1 = normal entity sounds
// NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS to total_channels = static sounds
#define	MAX_CHANNELS			516
#define	MAX_DYNAMIC_CHANNELS	128

extern channel_t channels[MAX_CHANNELS];

extern unsigned int total_channels;

extern int paintedtime;
extern int soundtime;
extern volatile dma_t *shm;

extern cvar_t snd_swapstereo;
extern cvar_t snd_streaming;

extern int snd_blocked;


#endif
