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
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#endif

void S_Play(void);
void S_PlayVol(void);
void S_Play2(void);
void S_SoundList(void);
void S_Update_();
void S_StopAllSounds(qboolean clear);
void S_StopAllSoundsC(void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t channels[MAX_CHANNELS];
int total_channels;

int snd_blocked = 0;
static qboolean snd_ambient = 1;
qboolean snd_initialized = false;

// pointer should go away
volatile dma_t *shm = 0;
volatile dma_t sn;

vec3_t listener_origin;
vec3_t listener_forward;
vec3_t listener_right;
vec3_t listener_up;
vec_t sound_nominal_clip_dist=1000.0;
mempool_t *snd_mempool;

// sample PAIRS
int soundtime;
int paintedtime;


//LordHavoc: increased the client sound limit from 512 to 4096 for the Nehahra movie
#define MAX_SFX 4096
sfx_t *known_sfx; // allocated [MAX_SFX]
int num_sfx;

sfx_t *ambient_sfx[NUM_AMBIENTS];

int sound_started = 0;

cvar_t bgmvolume = {CVAR_SAVE, "bgmvolume", "1"};
cvar_t volume = {CVAR_SAVE, "volume", "0.7"};

cvar_t nosound = {0, "nosound", "0"};
cvar_t precache = {0, "precache", "1"};
cvar_t bgmbuffer = {0, "bgmbuffer", "4096"};
cvar_t ambient_level = {0, "ambient_level", "0.3"};
cvar_t ambient_fade = {0, "ambient_fade", "100"};
cvar_t snd_noextraupdate = {0, "snd_noextraupdate", "0"};
cvar_t snd_show = {0, "snd_show", "0"};
cvar_t _snd_mixahead = {CVAR_SAVE, "_snd_mixahead", "0.1"};
cvar_t snd_swapstereo = {CVAR_SAVE, "snd_swapstereo", "0"};


// ====================================================================
// User-setable variables
// ====================================================================


//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

qboolean fakedma = false;
int fakedma_updates = 15;


void S_AmbientOff (void)
{
	snd_ambient = false;
}


void S_AmbientOn (void)
{
	snd_ambient = true;
}


void S_SoundInfo_f(void)
{
	if (!sound_started || !shm)
	{
		Con_Printf ("sound system not started\n");
		return;
	}

	Con_Printf("%5d stereo\n", shm->channels - 1);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d samplebits\n", shm->samplebits);
	Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf("%5d speed\n", shm->speed);
	Con_Printf("0x%x dma buffer\n", shm->buffer);
	Con_Printf("%5d total_channels\n", total_channels);
}


/*
================
S_Startup
================
*/
void S_Startup (void)
{
	int rc;

	if (!snd_initialized)
		return;

	if (!fakedma)
	{
		rc = SNDDMA_Init();

		if (!rc)
		{
			Con_Printf("S_Startup: SNDDMA_Init failed.\n");
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}

/*
================
S_Init
================
*/
void S_Init (void)
{
	Con_Printf("\nSound Initialization\n");

	S_RawSamples_ClearQueue();

	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&bgmvolume);

	if (COM_CheckParm("-nosound"))
		return;

	snd_mempool = Mem_AllocPool("sound");

	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("play2", S_Play2);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSoundsC);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&bgmbuffer);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);
	Cvar_RegisterVariable(&snd_swapstereo); // LordHavoc: for people with backwards sound wiring

	snd_initialized = true;

	known_sfx = Mem_Alloc(snd_mempool, MAX_SFX*sizeof(sfx_t));
	num_sfx = 0;

	S_Startup ();

	SND_InitScaletable ();

// create a piece of DMA memory

	if (fakedma)
	{
		shm = (void *) Mem_Alloc(snd_mempool, sizeof(*shm));
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = Mem_Alloc(snd_mempool, shm->channels * shm->samples * (shm->samplebits / 8));
	}

	if (!sound_started)
		return;

	Con_Printf ("Sound sampling rate: %i\n", shm->speed);

	// provides a tick sound until washed clean

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound ("ambience/water1.wav", false);
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound ("ambience/wind2.wav", false);

	S_StopAllSounds (true);
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = 0;

	shm = 0;
	sound_started = 0;

	if (!fakedma)
		SNDDMA_Shutdown();
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name)
{
	int i;
	sfx_t *sfx;

	if (!name)
		Host_Error ("S_FindName: NULL\n");

	if (strlen(name) >= MAX_QPATH)
		Host_Error ("Sound name too long: %s", name);

// see if already loaded
	for (i = 0;i < num_sfx;i++)
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;

	return sfx;
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound (char *name)
{
	sfx_t	*sfx;
	
	if (!sound_started)
		return;

	sfx = S_FindName (name);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (char *name, int complain)
{
	sfx_t *sfx;

	if (!sound_started || nosound.integer)
		return NULL;

	sfx = S_FindName (name);

// cache it in
	if (precache.integer)
		S_LoadSound (sfx, complain);

	return sfx;
}


//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t *SND_PickChannel(int entnum, int entchannel)
{
    int ch_idx;
    int first_to_die;
    int life_left;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& (channels[ch_idx].entchannel == entchannel || entchannel == -1) )
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

    return &channels[first_to_die];
}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(channel_t *ch)
{
    vec_t dot;
    vec_t dist;
    vec_t lscale, rscale, scale;
    vec3_t source_vec;
	sfx_t *snd;

// anything coming from the view entity will always be full volume
// LordHavoc: make sounds with ATTN_NONE have no spatialization
	if (ch->entnum == cl.viewentity || ch->dist_mult == 0)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

// calculate stereo seperation and distance attenuation

	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalizeLength(source_vec) * ch->dist_mult;

	dot = DotProduct(listener_right, source_vec);

	if (shm->channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	sfxcache_t	*sc;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.integer)
		return;

	vol = fvol*255;

// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

// new channel
	sc = S_LoadSound (sfx, true);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
    target_chan->end = paintedtime + sc->length;

// if an identical sound has also been started this frame, offset the pos
// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
    for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++, check++)
    {
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			// LordHavoc: fixed skip calculations
			skip = 0.1 * shm->speed;
			if (skip > sc->length)
				skip = sc->length;
			if (skip > 0)
				skip = rand() % skip;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}

	}
}

void S_StopSound(int entnum, int entchannel)
{
	int i;

	for (i=0 ; i<MAX_DYNAMIC_CHANNELS ; i++)
	{
		if (channels[i].entnum == entnum
			&& channels[i].entchannel == entchannel)
		{
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(qboolean clear)
{
	int		i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i=0 ; i<MAX_CHANNELS ; i++)
		if (channels[i].sfx)
			channels[i].sfx = NULL;

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer ();
}

void S_StopAllSoundsC (void)
{
	S_StopAllSounds (true);
}

void S_ClearBuffer (void)
{
	int		clear;

#ifdef _WIN32
	if (!sound_started || !shm || (!shm->buffer && !pDSBuf))
#else
	if (!sound_started || !shm || !shm->buffer)
#endif
		return;

	if (shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

#ifdef _WIN32
	if (pDSBuf)
	{
		DWORD	dwSize;
		DWORD	*pData;
		int		reps;
		HRESULT	hresult;

		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf ("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf ("S_ClearBuffer: DS: couldn't restore buffer\n");
				S_Shutdown ();
				return;
			}
		}

		memset(pData, clear, shm->samples * shm->samplebits/8);

		pDSBuf->lpVtbl->Unlock(pDSBuf, pData, dwSize, NULL, 0);

	}
	else
#endif
	{
		int		setsize = shm->samples * shm->samplebits / 8;
		char	*buf = shm->buffer;

		while (setsize--)
			*buf++ = 0;

// on i586/i686 optimized versions of glibc, glibc *wrongly* IMO,
// reads the memory area before writing to it causing a seg fault
// since the memory is PROT_WRITE only and not PROT_READ|PROT_WRITE
//		memset(shm->buffer, clear, shm->samples * shm->samplebits/8);
	}
}


/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t	*ss;
	sfxcache_t		*sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Con_Printf ("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound (sfx, true);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
    ss->end = paintedtime + sc->length;

	SND_Spatialize (ss);
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds (void)
{
	mleaf_t		*l;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

	// LordHavoc: kill ambient sounds until proven otherwise
	for (ambient_channel = 0 ; ambient_channel < NUM_AMBIENTS;ambient_channel++)
		channels[ambient_channel].sfx = NULL;

	if (!snd_ambient || !cl.worldmodel || ambient_level.value <= 0)
		return;

	l = Mod_PointInLeaf (listener_origin, cl.worldmodel);
	if (!l)
		return;

// calc ambient sound levels
	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

	// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_realframetime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_realframetime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}


/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

// update spatialization for static and dynamic sounds
	ch = channels+NUM_AMBIENTS;
	for (i=NUM_AMBIENTS ; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

	// try to combine static sounds with a previous channel of the same
	// sound effect so we don't mix five torches every frame

		if (i > MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
		// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
		// search for one
			combine = channels+MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j=MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS ; j<i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

//
// debugging output
//
	if (snd_show.integer)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
				total++;

		Con_Printf ("----(%i)----\n", total);
	}

// mix some sound
	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	fullsamples = shm->samples / shm->channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
#ifdef __sun__
	soundtime = SNDDMA_GetSamples();
#else
	samplepos = SNDDMA_GetDMAPos();


	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/shm->channels;
#endif
}

void IN_Accumulate (void);

void S_ExtraUpdate (void)
{

#ifdef _WIN32
	IN_Accumulate ();
#endif

	if (snd_noextraupdate.integer)
		return;		// don't pollute timings
	S_Update_();
}

void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started || (snd_blocked > 0))
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
		paintedtime = soundtime;

// mix ahead of current position
	endtime = soundtime + _snd_mixahead.value * shm->speed;
	samps = shm->samples >> (shm->channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

#ifdef _WIN32
// if the buffer was lost or stopped, restore it and/or restart it
	{
		DWORD	dwStatus;

		if (pDSBuf)
		{
			if (pDSBuf->lpVtbl->GetStatus (pDSBuf, &dwStatus) != DD_OK)
				Con_Printf ("Couldn't get sound buffer status\n");

			if (dwStatus & DSBSTATUS_BUFFERLOST)
				pDSBuf->lpVtbl->Restore (pDSBuf);

			if (!(dwStatus & DSBSTATUS_PLAYING))
				pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
		}
	}
#endif

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	static int hash=345;
	int 	i;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strcpy(name, Cmd_Argv(i));
			strcat(name, ".wav");
		}
		else
			strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name, true);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

void S_Play2(void)
{
	static int hash=345;
	int 	i;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strcpy(name, Cmd_Argv(i));
			strcat(name, ".wav");
		}
		else
			strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name, true);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 0.0);
		i++;
	}
}

void S_PlayVol(void)
{
	static int hash=543;
	int i;
	float vol;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strcpy(name, Cmd_Argv(i));
			strcat(name, ".wav");
		}
		else
			strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name, true);
		vol = atof(Cmd_Argv(i+1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0);
		i+=2;
	}
}

void S_SoundList(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		sc = sfx->sfxcache;
		if (!sc)
			continue;
		size = sc->length*sc->width*(sc->stereo+1);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf ("L");
		else
			Con_Printf (" ");
		Con_Printf("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Con_Printf ("Total resident: %i\n", total);
}


void S_LocalSound (char *sound)
{
	sfx_t	*sfx;

	if (nosound.integer)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound (sound, true);
	if (!sfx)
	{
		Con_Printf ("S_LocalSound: can't precache %s\n", sound);
		return;
	}
	S_StartSound (cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}


void S_ClearPrecache (void)
{
}


void S_BeginPrecaching (void)
{
}


void S_EndPrecaching (void)
{
}


#define RAWSAMPLESBUFFER 32768
short s_rawsamplesbuffer[RAWSAMPLESBUFFER * 2];
int s_rawsamplesbuffer_start;
int s_rawsamplesbuffer_count;

void S_RawSamples_Enqueue(short *samples, unsigned int length)
{
	int b2, b3;
	//Con_Printf("S_RawSamples_Enqueue: %i samples\n", length);
	if (s_rawsamplesbuffer_count + length > RAWSAMPLESBUFFER)
		return;
	b2 = (s_rawsamplesbuffer_start + s_rawsamplesbuffer_count) % RAWSAMPLESBUFFER;
	b3 = (s_rawsamplesbuffer_start + s_rawsamplesbuffer_count + length) % RAWSAMPLESBUFFER;
	if (b3 < b2)
	{
		memcpy(s_rawsamplesbuffer + b2 * 2, samples, (RAWSAMPLESBUFFER - b2) * sizeof(short[2]));
		memcpy(s_rawsamplesbuffer, samples + (RAWSAMPLESBUFFER - b2) * 2, b3 * sizeof(short[2]));
	}
	else
		memcpy(s_rawsamplesbuffer + b2 * 2, samples, length * sizeof(short[2]));
	s_rawsamplesbuffer_count += length;
}

void S_RawSamples_Dequeue(int *samples, unsigned int length)
{
	int b1, b2, l;
	int i;
	short *in;
	int *out;
	int count;
	l = length;
	if (l > s_rawsamplesbuffer_count)
		l = s_rawsamplesbuffer_count;
	b1 = (s_rawsamplesbuffer_start) % RAWSAMPLESBUFFER;
	b2 = (s_rawsamplesbuffer_start + l) % RAWSAMPLESBUFFER;
	if (b2 < b1)
	{
		//memcpy(samples, s_rawsamplesbuffer + b1 * 2, (RAWSAMPLESBUFFER - b1) * sizeof(short[2]));
		//memcpy(samples + (RAWSAMPLESBUFFER - b1) * 2, s_rawsamplesbuffer, b2 * sizeof(short[2]));
		for (out = samples, in = s_rawsamplesbuffer + b1 * 2, count = (RAWSAMPLESBUFFER - b1) * 2, i = 0;i < count;i++)
			out[i] = in[i];
		for (out = samples + (RAWSAMPLESBUFFER - b1) * 2, in = s_rawsamplesbuffer, count = b2 * 2, i = 0;i < count;i++)
			out[i] = in[i];
		//Con_Printf("S_RawSamples_Dequeue: buffer wrap %i %i\n", (RAWSAMPLESBUFFER - b1), b2);
	}
	else
	{
		//memcpy(samples, s_rawsamplesbuffer + b1 * 2, l * sizeof(short[2]));
		for (out = samples, in = s_rawsamplesbuffer + b1 * 2, count = l * 2, i = 0;i < count;i++)
			out[i] = in[i];
		//Con_Printf("S_RawSamples_Dequeue: normal      %i\n", l);
	}
	if (l < length)
	{
		memset(samples + l * 2, 0, (length - l) * sizeof(int[2]));
		//Con_Printf("S_RawSamples_Dequeue: padding with %i samples\n", length - l);
	}
	s_rawsamplesbuffer_start = (s_rawsamplesbuffer_start + l) % RAWSAMPLESBUFFER;
	s_rawsamplesbuffer_count -= l;
}

void S_RawSamples_ClearQueue(void)
{
	s_rawsamplesbuffer_count = 0;
	s_rawsamplesbuffer_start = 0;
}

int S_RawSamples_QueueWantsMore(void)
{
	if (s_rawsamplesbuffer_count < min(shm->speed >> 1, RAWSAMPLESBUFFER >> 1))
		return RAWSAMPLESBUFFER - s_rawsamplesbuffer_count;
	else
		return 0;
}

void S_ResampleBuffer16Stereo(short *input, int inputlength, short *output, int outputlength)
{
	if (inputlength != outputlength)
	{
		int i, position, stopposition, step;
		short *in, *out;
		step = (float) inputlength * 256.0f / (float) outputlength;
		position = 0;
		stopposition = (inputlength - 1) << 8;
		out = output;
		for (i = 0;i < outputlength && position < stopposition;i++, position += step)
		{
			in = input + ((position >> 8) << 1);
			out[0] = (((in[1] - in[0]) * (position & 255)) >> 8) + in[0];
			out[1] = (((in[3] - in[2]) * (position & 255)) >> 8) + in[2];
			out += 2;
		}
		stopposition = inputlength << 8;
		for (i = 0;i < outputlength && position < stopposition;i++, position += step)
		{
			in = input + ((position >> 8) << 1);
			out[0] = in[0];
			out[1] = in[2];
			out += 2;
		}
	}
	else
		memcpy(output, input, inputlength * sizeof(short[2]));
}
