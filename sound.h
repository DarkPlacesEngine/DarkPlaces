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
// sound.h -- client sound i/o functions

#ifndef SOUND_H
#define SOUND_H

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

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
#define SFXFLAG_SILENTLYMISSING	(1 << 0) // if the sfx is missing and loaded with complain = false
#define SFXFLAG_USED			(1 << 1)

typedef struct snd_fetcher_s snd_fetcher_t;
typedef struct sfx_s
{
	char				name[MAX_QPATH];
	mempool_t			*mempool;
	unsigned int		flags;			// cf SFXFLAG_* defines
	snd_format_t		format;
	int					loopstart;
	size_t				total_length;
	const snd_fetcher_t	*fetcher;
	void				*fetcher_data;	// Per-sfx data for the sound fetching functions
} sfx_t;

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
	sfx_t	*sfx;			// sfx number
	int		forceloop;		// force looping even if the sound is not looped
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int		master_vol;		// 0-255 master volume
	void	*fetcher_data;	// Per-channel data for the sound fetching function
} channel_t;

typedef const sfxbuffer_t* (*snd_fetcher_getsb_t) (channel_t* ch, unsigned int start, unsigned int nbsamples);
typedef void (*snd_fetcher_end_t) (channel_t* ch);
struct snd_fetcher_s
{
	snd_fetcher_getsb_t	getsb;
	snd_fetcher_end_t	end;
};

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

void S_Init (void);
void S_Startup (void);
void S_Shutdown (void);
void S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_StopSound (int entnum, int entchannel);
void S_StopAllSounds(qboolean clear);
void S_ClearBuffer (void);
void S_Update(vec3_t origin, vec3_t forward, vec3_t left, vec3_t up);
void S_ExtraUpdate (void);

sfx_t *S_GetCached(const char *name);
sfx_t *S_PrecacheSound (char *sample, int complain);
void S_TouchSound (char *sample);
void S_ClearUsed (void);
void S_PurgeUnused (void);
void S_PaintChannels(int endtime);
void S_InitPaintChannels (void);

// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel(int entnum, int entchannel);

// spatializes a channel
void SND_Spatialize(channel_t *ch, int isstatic);

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void SNDDMA_Shutdown(void);

extern size_t ResampleSfx (const qbyte *in_data, size_t in_length, const snd_format_t* in_format, qbyte *out_data, const char* sfxname);

// ====================================================================
// User-setable variables
// ====================================================================

// LordHavoc: increased from 128 to 516 (4 for NUM_AMBIENTS)
#define	MAX_CHANNELS			516
// LordHavoc: increased maximum sound channels from 8 to 128
#define	MAX_DYNAMIC_CHANNELS	128


extern channel_t channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern unsigned int total_channels;

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

extern qboolean fakedma;
extern int fakedma_updates;
extern int paintedtime;
extern int soundtime;
extern vec3_t listener_vieworigin;
extern vec3_t listener_viewforward;
extern vec3_t listener_viewleft;
extern vec3_t listener_viewup;
extern volatile dma_t *shm;
extern volatile dma_t sn;
extern vec_t sound_nominal_clip_dist;

extern cvar_t loadas8bit;
extern cvar_t bgmvolume;
extern cvar_t volume;
extern cvar_t snd_swapstereo;

extern cvar_t cdaudioinitialized;
extern cvar_t snd_initialized;
extern cvar_t snd_streaming;

extern int snd_blocked;

void S_LocalSound (char *s);
qboolean S_LoadSound (sfx_t *s, int complain);
void S_UnloadSound(sfx_t *s);

void SND_InitScaletable (void);
void SNDDMA_Submit(void);

void S_AmbientOff (void);
void S_AmbientOn (void);

void *S_LockBuffer(void);
void S_UnlockBuffer(void);

// add some data to the tail of the rawsamples queue
void S_RawSamples_Enqueue(short *samples, unsigned int length);
// read and remove some data from the head of the rawsamples queue
void S_RawSamples_Dequeue(int *samples, unsigned int length);
// empty the rawsamples queue
void S_RawSamples_ClearQueue(void);
// returns how much more data the queue wants, or 0 if it is already full enough
int S_RawSamples_QueueWantsMore(void);

// resamples one sound buffer into another, while changing the length
void S_ResampleBuffer16Stereo(short *input, int inputlength, short *output, int outputlength);

// returns the rate that the rawsamples system is running at
int S_RawSamples_SampleRate(void);

#endif

