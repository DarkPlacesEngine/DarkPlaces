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
// snd_main.c -- main control for any streaming sound output device

#include "quakedef.h"

#include "snd_main.h"
#include "snd_ogg.h"


void S_Play(void);
void S_PlayVol(void);
void S_Play2(void);
void S_SoundList(void);
void S_Update_();


// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t channels[MAX_CHANNELS];
unsigned int total_channels;

int snd_blocked = 0;
cvar_t snd_initialized = { CVAR_READONLY, "snd_initialized", "0"};
cvar_t snd_streaming = { CVAR_SAVE, "snd_streaming", "1"};

volatile dma_t *shm = 0;
volatile dma_t sn;

vec3_t listener_origin;
matrix4x4_t listener_matrix;
vec_t sound_nominal_clip_dist=1000.0;
mempool_t *snd_mempool;

int soundtime;
int paintedtime;

// Linked list of known sfx
sfx_t *known_sfx = NULL;

qboolean sound_started = false;
qboolean sound_spatialized = false;

// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.
qboolean fakedma = false;

cvar_t bgmvolume = {CVAR_SAVE, "bgmvolume", "1"};
cvar_t volume = {CVAR_SAVE, "volume", "0.7"};
cvar_t snd_staticvolume = {CVAR_SAVE, "snd_staticvolume", "1"};

cvar_t nosound = {0, "nosound", "0"};
cvar_t snd_precache = {0, "snd_precache", "1"};
cvar_t bgmbuffer = {0, "bgmbuffer", "4096"};
cvar_t ambient_level = {0, "ambient_level", "0.3"};
cvar_t ambient_fade = {0, "ambient_fade", "100"};
cvar_t snd_noextraupdate = {0, "snd_noextraupdate", "0"};
cvar_t snd_show = {0, "snd_show", "0"};
cvar_t _snd_mixahead = {CVAR_SAVE, "_snd_mixahead", "0.1"};
cvar_t snd_swapstereo = {CVAR_SAVE, "snd_swapstereo", "0"};

// Ambient sounds
sfx_t* ambient_sfxs [2] = { NULL, NULL };
const char* ambient_names [2] = { "sound/ambience/water1.wav", "sound/ambience/wind2.wav" };


// ====================================================================
// Functions
// ====================================================================

void S_FreeSfx (sfx_t *sfx, qboolean force);


void S_SoundInfo_f(void)
{
	if (!sound_started)
	{
		Con_Print("sound system not started\n");
		return;
	}

	Con_Printf("%5d stereo\n", shm->format.channels - 1);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d samplebits\n", shm->format.width * 8);
	Con_Printf("%5d speed\n", shm->format.speed);
	Con_Printf("%p dma buffer\n", shm->buffer);
	Con_Printf("%5u total_channels\n", total_channels);
}


void S_Startup(void)
{
	if (!snd_initialized.integer)
		return;

	shm = &sn;
	memset((void *)shm, 0, sizeof(*shm));

	// create a piece of DMA memory
	if (fakedma)
	{
		shm->format.width = 2;
		shm->format.speed = 22050;
		shm->format.channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->buffer = Mem_Alloc(snd_mempool, shm->format.channels * shm->samples * shm->format.width);
	}
	else
	{
		if (!SNDDMA_Init())
		{
			Con_Print("S_Startup: SNDDMA_Init failed.\n");
			shm = NULL;
			sound_started = false;
			sound_spatialized = false;
			return;
		}
	}

	sound_started = true;

	Con_DPrintf("Sound sampling rate: %i\n", shm->format.speed);
}

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	if (fakedma)
		Mem_Free(shm->buffer);
	else
		SNDDMA_Shutdown();

	shm = NULL;
	sound_started = false;
	sound_spatialized = false;
}

void S_Restart_f(void)
{
	S_Shutdown();
	S_Startup();
}

/*
================
S_Init
================
*/
void S_Init(void)
{
	Con_DPrint("\nSound Initialization\n");

	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&snd_staticvolume);

// COMMANDLINEOPTION: Sound: -nosound disables sound (including CD audio)
	if (COM_CheckParm("-nosound") || COM_CheckParm("-safe"))
		return;

	snd_mempool = Mem_AllocPool("sound", 0, NULL);

// COMMANDLINEOPTION: Sound: -simsound runs sound mixing but with no output
	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("play2", S_Play2);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSounds);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);
	Cmd_AddCommand("snd_restart", S_Restart_f);

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&snd_precache);
	Cvar_RegisterVariable(&snd_initialized);
	Cvar_RegisterVariable(&snd_streaming);
	Cvar_RegisterVariable(&bgmbuffer);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);
	Cvar_RegisterVariable(&snd_swapstereo); // for people with backwards sound wiring

	Cvar_SetValueQuick(&snd_initialized, true);

	known_sfx = NULL;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics
	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	OGG_OpenLibrary ();
}


/*
================
S_Terminate

Shutdown and free all resources
================
*/
void S_Terminate (void)
{
	S_Shutdown ();
	OGG_CloseLibrary ();

	// Free all SFXs
	while (known_sfx != NULL)
		S_FreeSfx (known_sfx, true);

	Cvar_SetValueQuick (&snd_initialized, false);
	Mem_FreePool (&snd_mempool);
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (const char *name)
{
	sfx_t *sfx;

	if (!snd_initialized.integer)
		return NULL;

	if (strlen (name) >= sizeof (sfx->name))
		Host_Error ("S_FindName: sound name too long (%s)", name);

	// Look for this sound in the list of known sfx
	for (sfx = known_sfx; sfx != NULL; sfx = sfx->next)
		if(!strcmp (sfx->name, name))
			return sfx;

	// Add a sfx_t struct for this sound
	sfx = Mem_Alloc (snd_mempool, sizeof (*sfx));
	memset (sfx, 0, sizeof(*sfx));
	strlcpy (sfx->name, name, sizeof (sfx->name));
	sfx->next = known_sfx;
	known_sfx = sfx;

	return sfx;
}


/*
==================
S_FreeSfx

==================
*/
void S_FreeSfx (sfx_t *sfx, qboolean force)
{
	// Never free a locked sfx unless forced
	if (!force && (sfx->locks > 0 || (sfx->flags & SFXFLAG_PERMANENTLOCK)))
		return;

	Con_DPrintf ("S_FreeSfx: freeing %s\n", sfx->name);

	// Remove it from the list of known sfx
	if (sfx == known_sfx)
		known_sfx = known_sfx->next;
	else
	{
		sfx_t *prev_sfx;

		for (prev_sfx = known_sfx; prev_sfx != NULL; prev_sfx = prev_sfx->next)
			if (prev_sfx->next == sfx)
			{
				prev_sfx->next = sfx->next;
				break;
			}
		if (prev_sfx == NULL)
			Sys_Error ("S_FreeSfx: Can't find SFX %s in the list!\n", sfx->name);
	}

	// Free it
	Mem_FreePool (&sfx->mempool);
	Mem_Free (sfx);
}


/*
==================
S_ServerSounds

==================
*/
void S_ServerSounds (char serversound [][MAX_QPATH], unsigned int numsounds)
{
	sfx_t *sfx;
	sfx_t *sfxnext;
	unsigned int i;

	// Start the ambient sounds and make them loop
	for (i = 0; i < sizeof (ambient_sfxs) / sizeof (ambient_sfxs[0]); i++)
	{
		// Precache it if it's not done (request a lock to make sure it will never be freed)
		if (ambient_sfxs[i] == NULL)
			ambient_sfxs[i] = S_PrecacheSound (ambient_names[i], false, true);
		if (ambient_sfxs[i] != NULL)
		{
			// Add a lock to the SFX while playing. It will be
			// removed by S_StopAllSounds at the end of the level
			S_LockSfx (ambient_sfxs[i]);

			channels[i].sfx = ambient_sfxs[i];
			channels[i].flags |= CHANNELFLAG_FORCELOOP;
			channels[i].master_vol = 0;
		}
	}

	// Remove 1 lock from all sfx with the SFXFLAG_SERVERSOUND flag, and remove the flag
	for (sfx = known_sfx; sfx != NULL; sfx = sfx->next)
		if (sfx->flags & SFXFLAG_SERVERSOUND)
		{
			S_UnlockSfx (sfx);
			sfx->flags &= ~SFXFLAG_SERVERSOUND;
		}

	// Add 1 lock and the SFXFLAG_SERVERSOUND flag to each sfx in "serversound"
	for (i = 1; i < numsounds; i++)
	{
		sfx = S_FindName (serversound[i]);
		if (sfx != NULL)
		{
			S_LockSfx (sfx);
			sfx->flags |= SFXFLAG_SERVERSOUND;
		}
	}

	// Free all unlocked sfx
	for (sfx = known_sfx;sfx;sfx = sfxnext)
	{
		sfxnext = sfx->next;
		S_FreeSfx (sfx, false);
	}
}


/*
==================
S_PrecacheSound

==================
*/
sfx_t *S_PrecacheSound (const char *name, qboolean complain, qboolean lock)
{
	sfx_t *sfx;

	if (!snd_initialized.integer)
		return NULL;

	sfx = S_FindName (name);
	if (sfx == NULL)
		return NULL;

	if (lock)
		S_LockSfx (sfx);

	if (!nosound.integer && snd_precache.integer)
		S_LoadSound(sfx, complain);

	return sfx;
}

/*
==================
S_LockSfx

Add a lock to a SFX
==================
*/
void S_LockSfx (sfx_t *sfx)
{
	sfx->locks++;
}

/*
==================
S_UnlockSfx

Remove a lock from a SFX
==================
*/
void S_UnlockSfx (sfx_t *sfx)
{
	sfx->locks--;
}


//=============================================================================

/*
=================
SND_PickChannel

Picks a channel based on priorities, empty slots, number of channels
=================
*/
channel_t *SND_PickChannel(int entnum, int entchannel)
{
	int ch_idx;
	int first_to_die;
	int life_left;
	channel_t* ch;

// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
	{
		ch = &channels[ch_idx];
		if (entchannel != 0		// channel 0 never overrides
		&& ch->entnum == entnum
		&& (ch->entchannel == entchannel || entchannel == -1) )
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (ch->entnum == cl.viewentity && entnum != cl.viewentity && ch->sfx)
			continue;

		// don't override looped sounds
		if ((ch->flags & CHANNELFLAG_FORCELOOP) != 0 ||
			(ch->sfx != NULL && ch->sfx->loopstart >= 0))
			continue;

		if (ch->end - paintedtime < life_left)
		{
			life_left = ch->end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	S_StopChannel (first_to_die);

	return &channels[first_to_die];
}

/*
=================
SND_Spatialize

Spatializes a channel
=================
*/
void SND_Spatialize(channel_t *ch, qboolean isstatic)
{
	vec_t dist, scale, pan;
	vec3_t source_vec;

	// anything coming from the view entity will always be full volume
	// LordHavoc: make sounds with ATTN_NONE have no spatialization
	if (ch->entnum == cl.viewentity || ch->dist_mult == 0)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
	}
	else
	{
		// update sound origin if we know about the entity
		if (ch->entnum > 0 && cls.state == ca_connected && cl_entities[ch->entnum].state_current.active)
		{
			//Con_Printf("-- entnum %i origin %f %f %f neworigin %f %f %f\n", ch->entnum, ch->origin[0], ch->origin[1], ch->origin[2], cl_entities[ch->entnum].state_current.origin[0], cl_entities[ch->entnum].state_current.origin[1], cl_entities[ch->entnum].state_current.origin[2]);
			VectorCopy(cl_entities[ch->entnum].state_current.origin, ch->origin);
			if (cl_entities[ch->entnum].state_current.modelindex && cl.model_precache[cl_entities[ch->entnum].state_current.modelindex] && cl.model_precache[cl_entities[ch->entnum].state_current.modelindex]->soundfromcenter)
				VectorMAMAM(1.0f, ch->origin, 0.5f, cl.model_precache[cl_entities[ch->entnum].state_current.modelindex]->normalmins, 0.5f, cl.model_precache[cl_entities[ch->entnum].state_current.modelindex]->normalmaxs, ch->origin);
		}

		// calculate stereo seperation and distance attenuation
		Matrix4x4_Transform(&listener_matrix, ch->origin, source_vec);
		dist = VectorNormalizeLength(source_vec);
		// distance
		scale = ch->master_vol * (1.0 - (dist * ch->dist_mult));
		// panning
		pan = scale * source_vec[1];
		// calculate the volumes
		ch->leftvol = (int) (scale + pan);
		ch->rightvol = (int) (scale - pan);
	}

	// Adjust volume of static sounds
	if (isstatic)
	{
		ch->leftvol *= snd_staticvolume.value;
		ch->rightvol *= snd_staticvolume.value;
	}

	// clamp volumes
	ch->leftvol = bound(0, ch->leftvol, 255);
	ch->rightvol = bound(0, ch->rightvol, 255);
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_PlaySfxOnChannel (sfx_t *sfx, channel_t *target_chan, unsigned int flags, vec3_t origin, float fvol, float attenuation, qboolean isstatic)
{
	// Initialize the channel
	memset (target_chan, 0, sizeof (*target_chan));
	VectorCopy (origin, target_chan->origin);
	target_chan->master_vol = fvol * 255;
	target_chan->sfx = sfx;
	target_chan->end = paintedtime + sfx->total_length;
	target_chan->lastptime = paintedtime;
	target_chan->flags = flags;

	// If it's a static sound
	if (isstatic)
	{
		if (sfx->loopstart == -1)
			Con_DPrintf("Quake compatibility warning: Static sound \"%s\" is not looped\n", sfx->name);
		target_chan->dist_mult = attenuation / (64.0f * sound_nominal_clip_dist);
	}
	else
		target_chan->dist_mult = attenuation / sound_nominal_clip_dist;

	// Lock the SFX during play
	S_LockSfx (sfx);
}


int S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	int		ch_idx;
	size_t	skip;

	if (!sound_started || !sfx || nosound.integer)
		return -1;
	if (!sfx->fetcher)
	{
		Con_DPrintf ("S_StartSound: \"%s\" hasn't been precached\n", sfx->name);
		return -1;
	}

	if (entnum && entnum >= cl_max_entities)
		CL_ExpandEntities(entnum);

	// Pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return -1;

	S_PlaySfxOnChannel (sfx, target_chan, CHANNELFLAG_NONE, origin, fvol, attenuation, false);
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;

	SND_Spatialize(target_chan, false);

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = 0.1 * sfx->format.speed;
			if (skip > sfx->total_length)
				skip = sfx->total_length;
			if (skip > 0)
				skip = rand() % skip;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}

	return (target_chan - channels);
}

void S_StopChannel (unsigned int channel_ind)
{
	channel_t *ch;

	if (channel_ind >= total_channels)
		return;

	ch = &channels[channel_ind];
	if (ch->sfx != NULL)
	{
		sfx_t *sfx = ch->sfx;

		if (sfx->fetcher != NULL)
		{
			snd_fetcher_end_t fetcher_end = sfx->fetcher->end;
			if (fetcher_end != NULL)
				fetcher_end (ch);
		}

		// Remove the lock it holds
		S_UnlockSfx (sfx);

		ch->sfx = NULL;
	}
	ch->end = 0;
}


qboolean S_SetChannelFlag (unsigned int ch_ind, unsigned int flag, qboolean value)
{
	if (ch_ind >= total_channels)
		return false;

	if (flag != CHANNELFLAG_FORCELOOP &&
		flag != CHANNELFLAG_PAUSED &&
		flag != CHANNELFLAG_FULLVOLUME)
		return false;

	if (value)
		channels[ch_ind].flags |= flag;
	else
		channels[ch_ind].flags &= ~flag;

	return true;
}

void S_StopSound(int entnum, int entchannel)
{
	unsigned int i;

	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel)
		{
			S_StopChannel (i);
			return;
		}
}

void S_StopAllSounds (void)
{
	unsigned int i;
	unsigned char *pbuf;

	if (!sound_started)
		return;

	for (i = 0; i < total_channels; i++)
		S_StopChannel (i);

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics
	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	// Clear sound buffer
	pbuf = S_LockBuffer();
	if (pbuf != NULL)
	{
		int setsize = shm->samples * shm->format.width;
		int	clear = (shm->format.width == 1) ? 0x80 : 0;

		// FIXME: is it (still) true? (check with OSS and ALSA)
		// on i586/i686 optimized versions of glibc, glibc *wrongly* IMO,
		// reads the memory area before writing to it causing a seg fault
		// since the memory is PROT_WRITE only and not PROT_READ|PROT_WRITE
		//memset(shm->buffer, clear, shm->samples * shm->format.width);
		while (setsize--)
			*pbuf++ = clear;

		S_UnlockBuffer ();
	}
}

void S_PauseGameSounds (qboolean toggle)
{
	unsigned int i;

	for (i = 0; i < total_channels; i++)
	{
		channel_t *ch;

		ch = &channels[i];
		if (ch->sfx != NULL && ! (ch->flags & CHANNELFLAG_LOCALSOUND))
			S_SetChannelFlag (i, CHANNELFLAG_PAUSED, toggle);
	}
}

void S_SetChannelVolume (unsigned int ch_ind, float fvol)
{
	channels[ch_ind].master_vol = fvol * 255;
}


/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t	*target_chan;

	if (!sound_started || !sfx || nosound.integer)
		return;
	if (!sfx->fetcher)
	{
		Con_DPrintf ("S_StaticSound: \"%s\" hasn't been precached\n", sfx->name);
		return;
	}

	if (total_channels == MAX_CHANNELS)
	{
		Con_Print("S_StaticSound: total_channels == MAX_CHANNELS\n");
		return;
	}

	target_chan = &channels[total_channels++];
	S_PlaySfxOnChannel (sfx, target_chan, CHANNELFLAG_FORCELOOP, origin, fvol, attenuation, true);

	SND_Spatialize (target_chan, true);
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds

===================
*/
void S_UpdateAmbientSounds (void)
{
	float		vol;
	int			ambient_channel;
	channel_t	*chan;
	qbyte		ambientlevels[NUM_AMBIENTS];

	if (ambient_level.value <= 0 || !cl.worldmodel || !cl.worldmodel->brush.AmbientSoundLevelsForPoint)
		return;

	cl.worldmodel->brush.AmbientSoundLevelsForPoint(cl.worldmodel, listener_origin, ambientlevels, sizeof(ambientlevels));

	// Calc ambient sound levels
	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		if (chan->sfx == NULL || chan->sfx->fetcher == NULL)
			continue;

		vol = ambient_level.value * ambientlevels[ambient_channel];
		if (vol < 8)
			vol = 0;

		// Don't adjust volume too fast
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
void S_Update(const matrix4x4_t *listenermatrix)
{
	unsigned int i, j, total;
	channel_t *ch, *combine;

	if (!snd_initialized.integer || (snd_blocked > 0))
		return;

	Matrix4x4_Invert_Simple(&listener_matrix, listenermatrix);
	Matrix4x4_OriginFromMatrix(listenermatrix, listener_origin);

	// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds
	ch = channels+NUM_AMBIENTS;
	for (i=NUM_AMBIENTS ; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch, i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS);         // respatialize channel
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
				combine = NULL;
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

	sound_spatialized = true;

	// debugging output
	if (snd_show.integer)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
				total++;

		Con_Printf("----(%u)----\n", total);
	}

	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	fullsamples = shm->samples / shm->format.channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / shm->format.channels;
}

void S_ExtraUpdate (void)
{
	if (snd_noextraupdate.integer || !sound_spatialized)
		return;

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
	endtime = soundtime + _snd_mixahead.value * shm->format.speed;
	samps = shm->samples >> (shm->format.channels - 1);
	if (endtime > (unsigned int)(soundtime + samps))
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

static void S_Play_Common(float fvol, float attenuation)
{
	int 	i, ch_ind;
	char name[256];
	sfx_t	*sfx;

	i = 1;
	while (i<Cmd_Argc())
	{
		// Get the name
		strlcpy(name, Cmd_Argv(i), sizeof(name));
		if (!strrchr(name, '.'))
			strlcat(name, ".wav", sizeof(name));
		i++;

		// If we need to get the volume from the command line
		if (fvol == -1.0f)
		{
			fvol = atof(Cmd_Argv(i));
			i++;
		}

		sfx = S_PrecacheSound (name, true, false);
		if (sfx)
		{
			ch_ind = S_StartSound(-1, 0, sfx, listener_origin, fvol, attenuation);

			// Free the sfx if the file didn't exist
			if (ch_ind < 0)
				S_FreeSfx (sfx, false);
			else
				channels[ch_ind].flags |= CHANNELFLAG_LOCALSOUND;
		}
	}
}

void S_Play(void)
{
	S_Play_Common (1.0f, 1.0f);
}

void S_Play2(void)
{
	S_Play_Common (1.0f, 0.0f);
}

void S_PlayVol(void)
{
	S_Play_Common (-1.0f, 0.0f);
}

void S_SoundList(void)
{
	unsigned int i;
	sfx_t	*sfx;
	int		size, total;

	total = 0;
	for (sfx = known_sfx, i = 0; sfx != NULL; sfx = sfx->next, i++)
	{
		if (sfx->fetcher != NULL)
		{
			size = sfx->mempool->totalsize;
			total += size;
			Con_Printf ("%c%c%c%c(%2db, %6s) %8i : %s\n",
						(sfx->loopstart >= 0) ? 'L' : ' ',
						(sfx->flags & SFXFLAG_STREAMED) ? 'S' : ' ',
						(sfx->locks > 0) ? 'K' : ' ',
						(sfx->flags & SFXFLAG_PERMANENTLOCK) ? 'P' : ' ',
						sfx->format.width * 8,
						(sfx->format.channels == 1) ? "mono" : "stereo",
						size,
						sfx->name);
		}
		else
			Con_Printf ("    (  unknown  ) unloaded : %s\n", sfx->name);
	}
	Con_Printf("Total resident: %i\n", total);
}


qboolean S_LocalSound (const char *sound)
{
	sfx_t	*sfx;
	int		ch_ind;

	if (!snd_initialized.integer || nosound.integer)
		return true;

	sfx = S_PrecacheSound (sound, true, false);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't precache %s\n", sound);
		return false;
	}

	// Local sounds must not be freed
	sfx->flags |= SFXFLAG_PERMANENTLOCK;

	ch_ind = S_StartSound (cl.viewentity, 0, sfx, vec3_origin, 1, 1);
	if (ch_ind < 0)
		return false;

	channels[ch_ind].flags |= CHANNELFLAG_LOCALSOUND;
	return true;
}
