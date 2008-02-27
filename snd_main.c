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
#include "snd_modplug.h"


#define SND_MIN_SPEED 8000
#define SND_MAX_SPEED 96000
#define SND_MIN_WIDTH 1
#define SND_MAX_WIDTH 2
#define SND_MIN_CHANNELS 1
#define SND_MAX_CHANNELS 8

#if SND_LISTENERS != 8
#	error this data only supports up to 8 channel, update it!
#endif
typedef struct listener_s
{
	float yawangle;
	float dotscale;
	float dotbias;
	float ambientvolume;
}
listener_t;
typedef struct speakerlayout_s
{
	const char *name;
	unsigned int channels;
	listener_t listeners[SND_LISTENERS];
}
speakerlayout_t;

static speakerlayout_t snd_speakerlayout;

// Our speaker layouts are based on ALSA. They differ from those
// Win32 and Mac OS X APIs use when there's more than 4 channels.
// (rear left + rear right, and front center + LFE are swapped).
#define SND_SPEAKERLAYOUTS (sizeof(snd_speakerlayouts) / sizeof(snd_speakerlayouts[0]))
static const speakerlayout_t snd_speakerlayouts[] =
{
	{
		"surround71", 8,
		{
			{45, 0.2, 0.2, 0.5}, // front left
			{315, 0.2, 0.2, 0.5}, // front right
			{135, 0.2, 0.2, 0.5}, // rear left
			{225, 0.2, 0.2, 0.5}, // rear right
			{0, 0.2, 0.2, 0.5}, // front center
			{0, 0, 0, 0}, // lfe (we don't have any good lfe sound sources and it would take some filtering work to generate them (and they'd probably still be wrong), so...  no lfe)
			{90, 0.2, 0.2, 0.5}, // side left
			{180, 0.2, 0.2, 0.5}, // side right
		}
	},
	{
		"surround51", 6,
		{
			{45, 0.2, 0.2, 0.5}, // front left
			{315, 0.2, 0.2, 0.5}, // front right
			{135, 0.2, 0.2, 0.5}, // rear left
			{225, 0.2, 0.2, 0.5}, // rear right
			{0, 0.2, 0.2, 0.5}, // front center
			{0, 0, 0, 0}, // lfe (we don't have any good lfe sound sources and it would take some filtering work to generate them (and they'd probably still be wrong), so...  no lfe)
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		}
	},
	{
		// these systems sometimes have a subwoofer as well, but it has no
		// channel of its own
		"surround40", 4,
		{
			{45, 0.3, 0.3, 0.8}, // front left
			{315, 0.3, 0.3, 0.8}, // front right
			{135, 0.3, 0.3, 0.8}, // rear left
			{225, 0.3, 0.3, 0.8}, // rear right
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		}
	},
	{
		// these systems sometimes have a subwoofer as well, but it has no
		// channel of its own
		"stereo", 2,
		{
			{90, 0.5, 0.5, 1}, // side left
			{270, 0.5, 0.5, 1}, // side right
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		}
	},
	{
		"mono", 1,
		{
			{0, 0, 1, 1}, // center
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
			{0, 0, 0, 0},
		}
	}
};


// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t channels[MAX_CHANNELS];
unsigned int total_channels;

snd_ringbuffer_t *snd_renderbuffer = NULL;
unsigned int soundtime = 0;
static unsigned int oldpaintedtime = 0;
static unsigned int extrasoundtime = 0;
static double snd_starttime = 0.0;

vec3_t listener_origin;
matrix4x4_t listener_matrix[SND_LISTENERS];
mempool_t *snd_mempool;

// Linked list of known sfx
static sfx_t *known_sfx = NULL;

static qboolean sound_spatialized = false;

qboolean simsound = false;

static qboolean recording_sound = false;

int snd_blocked = 0;
static int current_swapstereo = false;
static int current_channellayout = SND_CHANNELLAYOUT_AUTO;
static int current_channellayout_used = SND_CHANNELLAYOUT_AUTO;

// Cvars declared in sound.h (part of the sound API)
cvar_t bgmvolume = {CVAR_SAVE, "bgmvolume", "1", "volume of background music (such as CD music or replacement files such as sound/cdtracks/track002.ogg)"};
cvar_t volume = {CVAR_SAVE, "volume", "0.7", "volume of sound effects"};
cvar_t snd_initialized = { CVAR_READONLY, "snd_initialized", "0", "indicates the sound subsystem is active"};
cvar_t snd_staticvolume = {CVAR_SAVE, "snd_staticvolume", "1", "volume of ambient sound effects (such as swampy sounds at the start of e1m2)"};
cvar_t snd_soundradius = {0, "snd_soundradius", "2000", "radius of weapon sounds and other standard sound effects (monster idle noises are half this radius and flickering light noises are one third of this radius)"};

// Cvars declared in snd_main.h (shared with other snd_*.c files)
cvar_t _snd_mixahead = {CVAR_SAVE, "_snd_mixahead", "0.1", "how much sound to mix ahead of time"};
cvar_t snd_streaming = { CVAR_SAVE, "snd_streaming", "1", "enables keeping compressed ogg sound files compressed, decompressing them only as needed, otherwise they will be decompressed completely at load (may use a lot of memory)"};
cvar_t snd_swapstereo = {CVAR_SAVE, "snd_swapstereo", "0", "swaps left/right speakers for old ISA soundblaster cards"};
extern cvar_t v_flipped;
cvar_t snd_channellayout = {0, "snd_channellayout", "0", "channel layout. Can be 0 (auto - snd_restart needed), 1 (standard layout), or 2 (ALSA layout)"};
cvar_t snd_mutewhenidle = {CVAR_SAVE, "snd_mutewhenidle", "1", "whether to disable sound output when game window is inactive"};

// Local cvars
static cvar_t nosound = {0, "nosound", "0", "disables sound"};
static cvar_t snd_precache = {0, "snd_precache", "1", "loads sounds before they are used"};
static cvar_t ambient_level = {0, "ambient_level", "0.3", "volume of environment noises (water and wind)"};
static cvar_t ambient_fade = {0, "ambient_fade", "100", "rate of volume fading when moving from one environment to another"};
static cvar_t snd_noextraupdate = {0, "snd_noextraupdate", "0", "disables extra sound mixer calls that are meant to reduce the chance of sound breakup at very low framerates"};
static cvar_t snd_show = {0, "snd_show", "0", "shows some statistics about sound mixing"};

// Default sound format is 48KHz, 16-bit, stereo
// (48KHz because a lot of onboard sound cards sucks at any other speed)
static cvar_t snd_speed = {CVAR_SAVE, "snd_speed", "48000", "sound output frequency, in hertz"};
static cvar_t snd_width = {CVAR_SAVE, "snd_width", "2", "sound output precision, in bytes (1 and 2 supported)"};
static cvar_t snd_channels = {CVAR_SAVE, "snd_channels", "2", "number of channels for the sound ouput (2 for stereo; up to 8 supported for 3D sound)"};

// Ambient sounds
static sfx_t* ambient_sfxs [2] = { NULL, NULL };
static const char* ambient_names [2] = { "sound/ambience/water1.wav", "sound/ambience/wind2.wav" };


// ====================================================================
// Functions
// ====================================================================

void S_FreeSfx (sfx_t *sfx, qboolean force);

static void S_Play_Common (float fvol, float attenuation)
{
	int i, ch_ind;
	char name [MAX_QPATH];
	sfx_t *sfx;

	i = 1;
	while (i < Cmd_Argc ())
	{
		// Get the name, and appends ".wav" as an extension if there's none
		strlcpy (name, Cmd_Argv (i), sizeof (name));
		if (!strrchr (name, '.'))
			strlcat (name, ".wav", sizeof (name));
		i++;

		// If we need to get the volume from the command line
		if (fvol == -1.0f)
		{
			fvol = atof (Cmd_Argv (i));
			i++;
		}

		sfx = S_PrecacheSound (name, true, false);
		if (sfx)
		{
			ch_ind = S_StartSound (-1, 0, sfx, listener_origin, fvol, attenuation);

			// Free the sfx if the file didn't exist
			if (ch_ind < 0)
				S_FreeSfx (sfx, false);
			else
				channels[ch_ind].flags |= CHANNELFLAG_LOCALSOUND;
		}
	}
}

static void S_Play_f(void)
{
	S_Play_Common (1.0f, 1.0f);
}

static void S_Play2_f(void)
{
	S_Play_Common (1.0f, 0.0f);
}

static void S_PlayVol_f(void)
{
	S_Play_Common (-1.0f, 0.0f);
}

static void S_SoundList_f (void)
{
	unsigned int i;
	sfx_t *sfx;
	unsigned int total;

	total = 0;
	for (sfx = known_sfx, i = 0; sfx != NULL; sfx = sfx->next, i++)
	{
		if (sfx->fetcher != NULL)
		{
			unsigned int size;
			const snd_format_t* format;

			size = sfx->memsize;
			format = sfx->fetcher->getfmt(sfx);
			Con_Printf ("%c%c%c%c(%2db, %6s) %8i : %s\n",
						(sfx->loopstart < sfx->total_length) ? 'L' : ' ',
						(sfx->flags & SFXFLAG_STREAMED) ? 'S' : ' ',
						(sfx->locks > 0) ? 'K' : ' ',
						(sfx->flags & SFXFLAG_PERMANENTLOCK) ? 'P' : ' ',
						format->width * 8,
						(format->channels == 1) ? "mono" : "stereo",
						size,
						sfx->name);
			total += size;
		}
		else
			Con_Printf ("    (  unknown  ) unloaded : %s\n", sfx->name);
	}
	Con_Printf("Total resident: %i\n", total);
}


void S_SoundInfo_f(void)
{
	if (snd_renderbuffer == NULL)
	{
		Con_Print("sound system not started\n");
		return;
	}

	Con_Printf("%5d speakers\n", snd_renderbuffer->format.channels);
	Con_Printf("%5d frames\n", snd_renderbuffer->maxframes);
	Con_Printf("%5d samplebits\n", snd_renderbuffer->format.width * 8);
	Con_Printf("%5d speed\n", snd_renderbuffer->format.speed);
	Con_Printf("%5u total_channels\n", total_channels);
}


int S_GetSoundRate(void)
{
	return snd_renderbuffer ? snd_renderbuffer->format.speed : 0;
}


static qboolean S_ChooseCheaperFormat (snd_format_t* format, qboolean fixed_speed, qboolean fixed_width, qboolean fixed_channels)
{
	static const snd_format_t thresholds [] =
	{
		// speed			width			channels
		{ SND_MIN_SPEED,	SND_MIN_WIDTH,	SND_MIN_CHANNELS },
		{ 11025,			1,				2 },
		{ 22050,			2,				2 },
		{ 44100,			2,				2 },
		{ 48000,			2,				6 },
		{ 96000,			2,				6 },
		{ SND_MAX_SPEED,	SND_MAX_WIDTH,	SND_MAX_CHANNELS },
	};
	const unsigned int nb_thresholds = sizeof(thresholds) / sizeof(thresholds[0]);
	unsigned int speed_level, width_level, channels_level;

	// If we have reached the minimum values, there's nothing more we can do
	if ((format->speed == thresholds[0].speed || fixed_speed) &&
		(format->width == thresholds[0].width || fixed_width) &&
		(format->channels == thresholds[0].channels || fixed_channels))
		return false;

	// Check the min and max values
	#define CHECK_BOUNDARIES(param)								\
	if (format->param < thresholds[0].param)					\
	{															\
		format->param = thresholds[0].param;					\
		return true;											\
	}															\
	if (format->param > thresholds[nb_thresholds - 1].param)	\
	{															\
		format->param = thresholds[nb_thresholds - 1].param;	\
		return true;											\
	}
	CHECK_BOUNDARIES(speed);
	CHECK_BOUNDARIES(width);
	CHECK_BOUNDARIES(channels);
	#undef CHECK_BOUNDARIES

	// Find the level of each parameter
	#define FIND_LEVEL(param)									\
	param##_level = 0;											\
	while (param##_level < nb_thresholds - 1)					\
	{															\
		if (format->param <= thresholds[param##_level].param)	\
			break;												\
																\
		param##_level++;										\
	}
	FIND_LEVEL(speed);
	FIND_LEVEL(width);
	FIND_LEVEL(channels);
	#undef FIND_LEVEL

	// Decrease the parameter with the highest level to the previous level
	if (channels_level >= speed_level && channels_level >= width_level && !fixed_channels)
	{
		format->channels = thresholds[channels_level - 1].channels;
		return true;
	}
	if (speed_level >= width_level && !fixed_speed)
	{
		format->speed = thresholds[speed_level - 1].speed;
		return true;
	}

	format->width = thresholds[width_level - 1].width;
	return true;
}


#define SWAP_LISTENERS(l1, l2, tmpl) { tmpl = (l1); (l1) = (l2); (l2) = tmpl; }

static void S_SetChannelLayout (void)
{
	unsigned int i;
	listener_t swaplistener;
	listener_t *listeners;
	int layout;

	for (i = 0; i < SND_SPEAKERLAYOUTS; i++)
		if (snd_speakerlayouts[i].channels == snd_renderbuffer->format.channels)
			break;
	if (i >= SND_SPEAKERLAYOUTS)
	{
		Con_Printf("S_SetChannelLayout: can't find the speaker layout for %hu channels. Defaulting to mono output\n",
				   snd_renderbuffer->format.channels);
		i = SND_SPEAKERLAYOUTS - 1;
	}

	snd_speakerlayout = snd_speakerlayouts[i];
	listeners = snd_speakerlayout.listeners;

	// Swap the left and right channels if snd_swapstereo is set
	if (boolxor(snd_swapstereo.integer, v_flipped.integer))
	{
		switch (snd_speakerlayout.channels)
		{
			case 8:
				SWAP_LISTENERS(listeners[6], listeners[7], swaplistener);
				// no break
			case 4:
			case 6:
				SWAP_LISTENERS(listeners[2], listeners[3], swaplistener);
				// no break
			case 2:
				SWAP_LISTENERS(listeners[0], listeners[1], swaplistener);
				break;

			default:
			case 1:
				// Nothing to do
				break;
		}
	}

	// Sanity check
	if (snd_channellayout.integer < SND_CHANNELLAYOUT_AUTO ||
		snd_channellayout.integer > SND_CHANNELLAYOUT_ALSA)
		Cvar_SetValueQuick (&snd_channellayout, SND_CHANNELLAYOUT_STANDARD);

	if (snd_channellayout.integer == SND_CHANNELLAYOUT_AUTO)
	{
		// If we're in the sound engine initialization
		if (current_channellayout_used == SND_CHANNELLAYOUT_AUTO)
		{
			layout = SND_CHANNELLAYOUT_STANDARD;
			Cvar_SetValueQuick (&snd_channellayout, layout);
		}
		else
			layout = current_channellayout_used;
	}
	else
		layout = snd_channellayout.integer;

	// Convert our layout (= ALSA) to the standard layout if necessary
	if (snd_speakerlayout.channels == 6 || snd_speakerlayout.channels == 8)
	{
		if (layout == SND_CHANNELLAYOUT_STANDARD)
		{
			SWAP_LISTENERS(listeners[2], listeners[4], swaplistener);
			SWAP_LISTENERS(listeners[3], listeners[5], swaplistener);
		}

		Con_Printf("S_SetChannelLayout: using %s speaker layout for 3D sound\n",
				   (layout == SND_CHANNELLAYOUT_ALSA) ? "ALSA" : "standard");
	}

	current_swapstereo = boolxor(snd_swapstereo.integer, v_flipped.integer);
	current_channellayout = snd_channellayout.integer;
	current_channellayout_used = layout;
}


void S_Startup (void)
{
	qboolean fixed_speed, fixed_width, fixed_channels;
	snd_format_t chosen_fmt;
	static snd_format_t prev_render_format = {0, 0, 0};
	const char* env;
	int i;

	if (!snd_initialized.integer)
		return;

	fixed_speed = false;
	fixed_width = false;
	fixed_channels = false;

	// Get the starting sound format from the cvars
	chosen_fmt.speed = snd_speed.integer;
	chosen_fmt.width = snd_width.integer;
	chosen_fmt.channels = snd_channels.integer;

	// Check the environment variables to see if the player wants a particular sound format
	env = getenv("QUAKE_SOUND_CHANNELS");
	if (env != NULL)
	{
		chosen_fmt.channels = atoi (env);
		fixed_channels = true;
	}
	env = getenv("QUAKE_SOUND_SPEED");
	if (env != NULL)
	{
		chosen_fmt.speed = atoi (env);
		fixed_speed = true;
	}
	env = getenv("QUAKE_SOUND_SAMPLEBITS");
	if (env != NULL)
	{
		chosen_fmt.width = atoi (env) / 8;
		fixed_width = true;
	}

	// Parse the command line to see if the player wants a particular sound format
// COMMANDLINEOPTION: Sound: -sndquad sets sound output to 4 channel surround
	if (COM_CheckParm ("-sndquad") != 0)
	{
		chosen_fmt.channels = 4;
		fixed_channels = true;
	}
// COMMANDLINEOPTION: Sound: -sndstereo sets sound output to stereo
	else if (COM_CheckParm ("-sndstereo") != 0)
	{
		chosen_fmt.channels = 2;
		fixed_channels = true;
	}
// COMMANDLINEOPTION: Sound: -sndmono sets sound output to mono
	else if (COM_CheckParm ("-sndmono") != 0)
	{
		chosen_fmt.channels = 1;
		fixed_channels = true;
	}
// COMMANDLINEOPTION: Sound: -sndspeed <hz> chooses sound output rate (supported values are 48000, 44100, 32000, 24000, 22050, 16000, 11025 (quake), 8000)
	i = COM_CheckParm ("-sndspeed");
	if (0 < i && i < com_argc - 1)
	{
		chosen_fmt.speed = atoi (com_argv[i + 1]);
		fixed_speed = true;
	}
// COMMANDLINEOPTION: Sound: -sndbits <bits> chooses 8 bit or 16 bit sound output
	i = COM_CheckParm ("-sndbits");
	if (0 < i && i < com_argc - 1)
	{
		chosen_fmt.width = atoi (com_argv[i + 1]) / 8;
		fixed_width = true;
	}

	// You can't change sound speed after start time (not yet supported)
	if (prev_render_format.speed != 0)
	{
		fixed_speed = true;
		if (chosen_fmt.speed != prev_render_format.speed)
		{
			Con_Printf("S_Startup: sound speed has changed! This is NOT supported yet. Falling back to previous speed (%u Hz)\n",
					   prev_render_format.speed);
			chosen_fmt.speed = prev_render_format.speed;
		}
	}

	// Sanity checks
	if (chosen_fmt.speed < SND_MIN_SPEED)
	{
		chosen_fmt.speed = SND_MIN_SPEED;
		fixed_speed = false;
	}
	else if (chosen_fmt.speed > SND_MAX_SPEED)
	{
		chosen_fmt.speed = SND_MAX_SPEED;
		fixed_speed = false;
	}

	if (chosen_fmt.width < SND_MIN_WIDTH)
	{
		chosen_fmt.width = SND_MIN_WIDTH;
		fixed_width = false;
	}
	else if (chosen_fmt.width > SND_MAX_WIDTH)
	{
		chosen_fmt.width = SND_MAX_WIDTH;
		fixed_width = false;
	}

	if (chosen_fmt.channels < SND_MIN_CHANNELS)
	{
		chosen_fmt.channels = SND_MIN_CHANNELS;
		fixed_channels = false;
	}
	else if (chosen_fmt.channels > SND_MAX_CHANNELS)
	{
		chosen_fmt.channels = SND_MAX_CHANNELS;
		fixed_channels = false;
	}

	// create the sound buffer used for sumitting the samples to the plaform-dependent module
	if (!simsound)
	{
		snd_format_t suggest_fmt;
		qboolean accepted;

		accepted = false;
		do
		{
			Con_Printf("S_Startup: initializing sound output format: %dHz, %d bit, %d channels...\n",
						chosen_fmt.speed, chosen_fmt.width * 8,
						chosen_fmt.channels);

			memset(&suggest_fmt, 0, sizeof(suggest_fmt));
			accepted = SndSys_Init(&chosen_fmt, &suggest_fmt);

			if (!accepted)
			{
				Con_Printf("S_Startup: sound output initialization FAILED\n");

				// If the module is suggesting another one
				if (suggest_fmt.speed != 0)
				{
					memcpy(&chosen_fmt, &suggest_fmt, sizeof(chosen_fmt));
					Con_Printf ("           Driver has suggested %dHz, %d bit, %d channels. Retrying...\n",
								suggest_fmt.speed, suggest_fmt.width * 8,
								suggest_fmt.channels);
				}
				// Else, try to find a less resource-demanding format
				else if (!S_ChooseCheaperFormat (&chosen_fmt, fixed_speed, fixed_width, fixed_channels))
					break;
			}
		} while (!accepted);

		// If we haven't found a suitable format
		if (!accepted)
		{
			Con_Print("S_Startup: SndSys_Init failed.\n");
			sound_spatialized = false;
			return;
		}
	}
	else
	{
		snd_renderbuffer = Snd_CreateRingBuffer(&chosen_fmt, 0, NULL);
		Con_Print ("S_Startup: simulating sound output\n");
	}

	memcpy(&prev_render_format, &snd_renderbuffer->format, sizeof(prev_render_format));
	Con_Printf("Sound format: %dHz, %d channels, %d bits per sample\n",
			   chosen_fmt.speed, chosen_fmt.channels, chosen_fmt.width * 8);

	// Update the cvars
	if (snd_speed.integer != (int)chosen_fmt.speed)
		Cvar_SetValueQuick(&snd_speed, chosen_fmt.speed);
	if (snd_width.integer != chosen_fmt.width)
		Cvar_SetValueQuick(&snd_width, chosen_fmt.width);
	if (snd_channels.integer != chosen_fmt.channels)
		Cvar_SetValueQuick(&snd_channels, chosen_fmt.channels);

	current_channellayout_used = SND_CHANNELLAYOUT_AUTO;
	S_SetChannelLayout();

	snd_starttime = realtime;

	// If the sound module has already run, add an extra time to make sure
	// the sound time doesn't decrease, to not confuse playing SFXs
	if (oldpaintedtime != 0)
	{
		// The extra time must be a multiple of the render buffer size
		// to avoid modifying the current position in the buffer,
		// some modules write directly to a shared (DMA) buffer
		extrasoundtime = oldpaintedtime + snd_renderbuffer->maxframes - 1;
		extrasoundtime -= extrasoundtime % snd_renderbuffer->maxframes;
		Con_Printf("S_Startup: extra sound time = %u\n", extrasoundtime);

		soundtime = extrasoundtime;
	}
	else
		extrasoundtime = 0;
	snd_renderbuffer->startframe = soundtime;
	snd_renderbuffer->endframe = soundtime;
	recording_sound = false;
}

void S_Shutdown(void)
{
	if (snd_renderbuffer == NULL)
		return;

	oldpaintedtime = snd_renderbuffer->endframe;

	if (simsound)
	{
		Mem_Free(snd_renderbuffer->ring);
		Mem_Free(snd_renderbuffer);
		snd_renderbuffer = NULL;
	}
	else
		SndSys_Shutdown();

	sound_spatialized = false;
}

void S_Restart_f(void)
{
	// NOTE: we can't free all sounds if we are running a map (this frees sfx_t that are still referenced by precaches)
	// So, refuse to do this if we are connected.
	if(cls.state == ca_connected)
	{
		Con_Printf("snd_restart would wreak havoc if you do that while connected!\n");
		return;
	}

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
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&snd_staticvolume);

	Cvar_RegisterVariable(&snd_speed);
	Cvar_RegisterVariable(&snd_width);
	Cvar_RegisterVariable(&snd_channels);
	Cvar_RegisterVariable(&snd_mutewhenidle);

// COMMANDLINEOPTION: Sound: -nosound disables sound (including CD audio)
	if (COM_CheckParm("-nosound"))
	{
		// dummy out Play and Play2 because mods stuffcmd that
		Cmd_AddCommand("play", Host_NoOperation_f, "does nothing because -nosound was specified");
		Cmd_AddCommand("play2", Host_NoOperation_f, "does nothing because -nosound was specified");
		return;
	}

	snd_mempool = Mem_AllocPool("sound", 0, NULL);

// COMMANDLINEOPTION: Sound: -simsound runs sound mixing but with no output
	if (COM_CheckParm("-simsound"))
		simsound = true;

	Cmd_AddCommand("play", S_Play_f, "play a sound at your current location (not heard by anyone else)");
	Cmd_AddCommand("play2", S_Play2_f, "play a sound globally throughout the level (not heard by anyone else)");
	Cmd_AddCommand("playvol", S_PlayVol_f, "play a sound at the specified volume level at your current location (not heard by anyone else)");
	Cmd_AddCommand("stopsound", S_StopAllSounds, "silence");
	Cmd_AddCommand("soundlist", S_SoundList_f, "list loaded sounds");
	Cmd_AddCommand("soundinfo", S_SoundInfo_f, "print sound system information (such as channels and speed)");
	Cmd_AddCommand("snd_restart", S_Restart_f, "restart sound system");
	Cmd_AddCommand("snd_unloadallsounds", S_UnloadAllSounds_f, "unload all sound files");

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&snd_precache);
	Cvar_RegisterVariable(&snd_initialized);
	Cvar_RegisterVariable(&snd_streaming);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);
	Cvar_RegisterVariable(&snd_swapstereo); // for people with backwards sound wiring
	Cvar_RegisterVariable(&snd_channellayout);
	Cvar_RegisterVariable(&snd_soundradius);

	Cvar_SetValueQuick(&snd_initialized, true);

	known_sfx = NULL;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics
	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	OGG_OpenLibrary ();
	ModPlug_OpenLibrary ();
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
	ModPlug_CloseLibrary ();
	OGG_CloseLibrary ();

	// Free all SFXs
	while (known_sfx != NULL)
		S_FreeSfx (known_sfx, true);

	Cvar_SetValueQuick (&snd_initialized, false);
	Mem_FreePool (&snd_mempool);
}


/*
==================
S_UnloadAllSounds_f
==================
*/
void S_UnloadAllSounds_f (void)
{
	int i;

	// NOTE: we can't free all sounds if we are running a map (this frees sfx_t that are still referenced by precaches)
	// So, refuse to do this if we are connected.
	if(cls.state == ca_connected)
	{
		Con_Printf("snd_unloadallsounds would wreak havoc if you do that while connected!\n");
		return;
	}

	// stop any active sounds
	S_StopAllSounds();

	// because the ambient sounds will be freed, clear the pointers
	for (i = 0;i < (int)sizeof (ambient_sfxs) / (int)sizeof (ambient_sfxs[0]);i++)
		ambient_sfxs[i] = NULL;

	// now free all sounds
	while (known_sfx != NULL)
		S_FreeSfx (known_sfx, true);
}


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
	{
		Con_Printf ("S_FindName: sound name too long (%s)\n", name);
		return NULL;
	}

	// Look for this sound in the list of known sfx
	// TODO: hash table search?
	for (sfx = known_sfx; sfx != NULL; sfx = sfx->next)
		if(!strcmp (sfx->name, name))
			return sfx;

	// Add a sfx_t struct for this sound
	sfx = (sfx_t *)Mem_Alloc (snd_mempool, sizeof (*sfx));
	memset (sfx, 0, sizeof(*sfx));
	strlcpy (sfx->name, name, sizeof (sfx->name));
	sfx->memsize = sizeof(*sfx);
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
	unsigned int i;

	// Never free a locked sfx unless forced
	if (!force && (sfx->locks > 0 || (sfx->flags & SFXFLAG_PERMANENTLOCK)))
		return;

	if (developer_loading.integer)
		Con_Printf ("unloading sound %s\n", sfx->name);

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
		{
			Con_Printf ("S_FreeSfx: Can't find SFX %s in the list!\n", sfx->name);
			return;
		}
	}

	// Stop all channels using this sfx
	for (i = 0; i < total_channels; i++)
		if (channels[i].sfx == sfx)
			S_StopChannel (i);

	// Free it
	if (sfx->fetcher != NULL && sfx->fetcher->free != NULL)
		sfx->fetcher->free (sfx->fetcher_data);
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
			// clear the FILEMISSING flag so that S_LoadSound will try again on a
			// previously missing file
			sfx->flags &= ~ SFXFLAG_FILEMISSING;
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

	if (name == NULL || name[0] == 0)
		return NULL;

	sfx = S_FindName (name);

	if (sfx == NULL)
		return NULL;

	// clear the FILEMISSING flag so that S_LoadSound will try again on a
	// previously missing file
	sfx->flags &= ~ SFXFLAG_FILEMISSING;

	if (lock)
		S_LockSfx (sfx);

	if (!nosound.integer && snd_precache.integer)
		S_LoadSound(sfx, complain);

	return sfx;
}

/*
==================
S_IsSoundPrecached
==================
*/
qboolean S_IsSoundPrecached (const sfx_t *sfx)
{
	return (sfx != NULL && sfx->fetcher != NULL);
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


/*
==================
S_BlockSound
==================
*/
void S_BlockSound (void)
{
	snd_blocked++;
}


/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound (void)
{
	snd_blocked--;
}


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
	int first_life_left, life_left;
	channel_t* ch;

// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	first_life_left = 0x7fffffff;

	// entity channels try to replace the existing sound on the channel
	if (entchannel != 0)
	{
		for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
		{
			ch = &channels[ch_idx];
			if (ch->entnum == entnum && (ch->entchannel == entchannel || entchannel == -1) )
			{
				// always override sound from same entity
				S_StopChannel (ch_idx);
				return &channels[ch_idx];
			}
		}
	}

	// there was no channel to override, so look for the first empty one
	for (ch_idx=NUM_AMBIENTS ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS ; ch_idx++)
	{
		ch = &channels[ch_idx];
		if (!ch->sfx)
		{
			// no sound on this channel
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (ch->entnum == cl.viewentity && entnum != cl.viewentity)
			continue;

		// don't override looped sounds
		if ((ch->flags & CHANNELFLAG_FORCELOOP) || ch->sfx->loopstart < ch->sfx->total_length)
			continue;
		life_left = ch->sfx->total_length - ch->pos;

		if (life_left < first_life_left)
		{
			first_life_left = life_left;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	return &channels[first_to_die];
}

/*
=================
SND_Spatialize

Spatializes a channel
=================
*/
extern cvar_t cl_gameplayfix_soundsmovewithentities;
void SND_Spatialize(channel_t *ch, qboolean isstatic)
{
	int i;
	vec_t dist, mastervol, intensity, vol;
	vec3_t source_vec;

	// update sound origin if we know about the entity
	if (ch->entnum > 0 && cls.state == ca_connected && cl_gameplayfix_soundsmovewithentities.integer)
	{
		if (ch->entnum >= 32768)
		{
			// TODO: sounds that follow CSQC entities?
		}
		else if (cl.entities[ch->entnum].state_current.active)
		{
			//Con_Printf("-- entnum %i origin %f %f %f neworigin %f %f %f\n", ch->entnum, ch->origin[0], ch->origin[1], ch->origin[2], cl.entities[ch->entnum].state_current.origin[0], cl.entities[ch->entnum].state_current.origin[1], cl.entities[ch->entnum].state_current.origin[2]);
			VectorCopy(cl.entities[ch->entnum].state_current.origin, ch->origin);
			if (cl.entities[ch->entnum].state_current.modelindex && cl.model_precache[cl.entities[ch->entnum].state_current.modelindex] && cl.model_precache[cl.entities[ch->entnum].state_current.modelindex]->soundfromcenter)
				VectorMAMAM(1.0f, ch->origin, 0.5f, cl.model_precache[cl.entities[ch->entnum].state_current.modelindex]->normalmins, 0.5f, cl.model_precache[cl.entities[ch->entnum].state_current.modelindex]->normalmaxs, ch->origin);
		}
	}

	mastervol = ch->master_vol;
	// Adjust volume of static sounds
	if (isstatic)
		mastervol *= snd_staticvolume.value;

	// anything coming from the view entity will always be full volume
	// LordHavoc: make sounds with ATTN_NONE have no spatialization
	if (ch->entnum == cl.viewentity || ch->dist_mult == 0)
	{
		for (i = 0;i < SND_LISTENERS;i++)
		{
			vol = mastervol * snd_speakerlayout.listeners[i].ambientvolume;
			ch->listener_volume[i] = (int)bound(0, vol, 255);
		}
	}
	else
	{
		// calculate stereo seperation and distance attenuation
		VectorSubtract(listener_origin, ch->origin, source_vec);
		dist = VectorLength(source_vec);
		intensity = mastervol * (1.0 - dist * ch->dist_mult);
		if (intensity > 0)
		{
			for (i = 0;i < SND_LISTENERS;i++)
			{
				Matrix4x4_Transform(&listener_matrix[i], ch->origin, source_vec);
				VectorNormalize(source_vec);
				vol = intensity * max(0, source_vec[0] * snd_speakerlayout.listeners[i].dotscale + snd_speakerlayout.listeners[i].dotbias);
				ch->listener_volume[i] = (int)bound(0, vol, 255);
			}
		}
		else
			for (i = 0;i < SND_LISTENERS;i++)
				ch->listener_volume[i] = 0;
	}
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_PlaySfxOnChannel (sfx_t *sfx, channel_t *target_chan, unsigned int flags, vec3_t origin, float fvol, float attenuation, qboolean isstatic)
{
	// Initialize the channel
	memset (target_chan, 0, sizeof (*target_chan));
	VectorCopy (origin, target_chan->origin);
	target_chan->sfx = sfx;
	target_chan->flags = flags;
	target_chan->pos = 0; // start of the sound

	// If it's a static sound
	if (isstatic)
	{
		if (sfx->loopstart >= sfx->total_length)
			Con_DPrintf("Quake compatibility warning: Static sound \"%s\" is not looped\n", sfx->name);
		target_chan->dist_mult = attenuation / (64.0f * snd_soundradius.value);
	}
	else
		target_chan->dist_mult = attenuation / snd_soundradius.value;

	// Lock the SFX during play
	S_LockSfx (sfx);

	// and finally, apply the volume
	S_SetChannelVolume(target_chan - channels, fvol);
}


int S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	int		ch_idx;

	if (snd_renderbuffer == NULL || sfx == NULL || nosound.integer)
		return -1;

	if (sfx->fetcher == NULL)
		return -1;

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
			// use negative pos offset to delay this sound effect
			target_chan->pos += (int)lhrandom(0, -0.1 * snd_renderbuffer->format.speed);
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
			snd_fetcher_endsb_t fetcher_endsb = sfx->fetcher->endsb;
			if (fetcher_endsb != NULL)
				fetcher_endsb (ch->fetcher_data);
		}

		// Remove the lock it holds
		S_UnlockSfx (sfx);

		ch->fetcher_data = NULL;
		ch->sfx = NULL;
	}
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

extern void CDAudio_Stop(void);
void S_StopAllSounds (void)
{
	unsigned int i;

	// TOCHECK: is this test necessary?
	if (snd_renderbuffer == NULL)
		return;

	// stop CD audio because it may be using a faketrack
	CDAudio_Stop();

	for (i = 0; i < total_channels; i++)
		S_StopChannel (i);

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics
	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	// Mute the contents of the submittion buffer
	if (simsound || SndSys_LockRenderBuffer ())
	{
		int clear;
		size_t memsize;

		clear = (snd_renderbuffer->format.width == 1) ? 0x80 : 0;
		memsize = snd_renderbuffer->maxframes * snd_renderbuffer->format.width * snd_renderbuffer->format.channels;
		memset(snd_renderbuffer->ring, clear, memsize);

		if (!simsound)
			SndSys_UnlockRenderBuffer ();
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
	sfx_t *sfx = channels[ch_ind].sfx;
	if(sfx->volume_peak > 0)
	{
		// Replaygain support
		// Con_DPrintf("Setting volume on ReplayGain-enabled track... %f -> ", fvol);
		fvol *= sfx->volume_mult;
		if(fvol * sfx->volume_peak > 1)
			fvol = 1 / sfx->volume_peak;
		// Con_DPrintf("%f\n", fvol);
	}
	channels[ch_ind].master_vol = (int)(fvol * 255.0f);
}


/*
=================
S_StaticSound
=================
*/
void S_StaticSound (sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t	*target_chan;

	if (snd_renderbuffer == NULL || sfx == NULL || nosound.integer)
		return;
	if (!sfx->fetcher)
	{
		Con_Printf ("S_StaticSound: \"%s\" hasn't been precached\n", sfx->name);
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


/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds (void)
{
	int			i;
	int			vol;
	int			ambient_channel;
	channel_t	*chan;
	unsigned char		ambientlevels[NUM_AMBIENTS];

	memset(ambientlevels, 0, sizeof(ambientlevels));
	if (cl.worldmodel && cl.worldmodel->brush.AmbientSoundLevelsForPoint)
		cl.worldmodel->brush.AmbientSoundLevelsForPoint(cl.worldmodel, listener_origin, ambientlevels, sizeof(ambientlevels));

	// Calc ambient sound levels
	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		if (chan->sfx == NULL || chan->sfx->fetcher == NULL)
			continue;

		vol = (int)ambientlevels[ambient_channel];
		if (vol < 8)
			vol = 0;

		// Don't adjust volume too fast
		// FIXME: this rounds off to an int each frame, meaning there is little to no fade at extremely high framerates!
		if (cl.time > cl.oldtime)
		{
			if (chan->master_vol < vol)
			{
				chan->master_vol += (int)((cl.time - cl.oldtime) * ambient_fade.value);
				if (chan->master_vol > vol)
					chan->master_vol = vol;
			}
			else if (chan->master_vol > vol)
			{
				chan->master_vol -= (int)((cl.time - cl.oldtime) * ambient_fade.value);
				if (chan->master_vol < vol)
					chan->master_vol = vol;
			}
		}

		for (i = 0;i < SND_LISTENERS;i++)
			chan->listener_volume[i] = (int)(chan->master_vol * ambient_level.value * snd_speakerlayout.listeners[i].ambientvolume);
	}
}

static void S_PaintAndSubmit (void)
{
	unsigned int newsoundtime, paintedtime, endtime, maxtime, usedframes;
	int usesoundtimehack;
	static int soundtimehack = -1;
	static int oldsoundtime = 0;

	if (snd_renderbuffer == NULL || nosound.integer)
		return;

	// Update sound time
	usesoundtimehack = true;
	if (cls.timedemo) // SUPER NASTY HACK to mix non-realtime sound for more reliable benchmarking
	{
		usesoundtimehack = 1;
		newsoundtime = (unsigned int)((double)cl.mtime[0] * (double)snd_renderbuffer->format.speed);
	}
	else if (cls.capturevideo.soundrate && !cls.capturevideo.realtime) // SUPER NASTY HACK to record non-realtime sound
	{
		usesoundtimehack = 2;
		newsoundtime = (unsigned int)((double)cls.capturevideo.frame * (double)snd_renderbuffer->format.speed / (double)cls.capturevideo.framerate);
	}
	else if (simsound)
	{
		usesoundtimehack = 3;
		newsoundtime = (unsigned int)((realtime - snd_starttime) * (double)snd_renderbuffer->format.speed);
	}
	else
	{
		usesoundtimehack = 0;
		newsoundtime = SndSys_GetSoundTime();
	}
	// if the soundtimehack state changes we need to reset the soundtime
	if (soundtimehack != usesoundtimehack)
	{
		snd_renderbuffer->startframe = snd_renderbuffer->endframe = soundtime = newsoundtime;

		// Mute the contents of the submission buffer
		if (simsound || SndSys_LockRenderBuffer ())
		{
			int clear;
			size_t memsize;

			clear = (snd_renderbuffer->format.width == 1) ? 0x80 : 0;
			memsize = snd_renderbuffer->maxframes * snd_renderbuffer->format.width * snd_renderbuffer->format.channels;
			memset(snd_renderbuffer->ring, clear, memsize);

			if (!simsound)
				SndSys_UnlockRenderBuffer ();
		}
	}
	soundtimehack = usesoundtimehack;

	if (!soundtimehack && snd_blocked > 0)
		return;

	newsoundtime += extrasoundtime;
	if (newsoundtime < soundtime)
	{
		if ((cls.capturevideo.soundrate != 0) != recording_sound)
		{
			unsigned int additionaltime;

			// add some time to extrasoundtime make newsoundtime higher

			// The extra time must be a multiple of the render buffer size
			// to avoid modifying the current position in the buffer,
			// some modules write directly to a shared (DMA) buffer
			additionaltime = (soundtime - newsoundtime) + snd_renderbuffer->maxframes - 1;
			additionaltime -= additionaltime % snd_renderbuffer->maxframes;

			extrasoundtime += additionaltime;
			newsoundtime += additionaltime;
			Con_DPrintf("S_PaintAndSubmit: new extra sound time = %u\n",
						extrasoundtime);
		}
		else if (!soundtimehack)
			Con_Printf("S_PaintAndSubmit: WARNING: newsoundtime < soundtime (%u < %u)\n",
					   newsoundtime, soundtime);
	}
	soundtime = newsoundtime;
	recording_sound = (cls.capturevideo.soundrate != 0);

	// Check to make sure that we haven't overshot
	paintedtime = snd_renderbuffer->endframe;
	if (paintedtime < soundtime)
		paintedtime = soundtime;

	// mix ahead of current position
	if (soundtimehack)
		endtime = soundtime + (unsigned int)(_snd_mixahead.value * (float)snd_renderbuffer->format.speed);
	else
		endtime = soundtime + (unsigned int)(max(_snd_mixahead.value * (float)snd_renderbuffer->format.speed, min(3 * (soundtime - oldsoundtime), 0.3 * (float)snd_renderbuffer->format.speed)));
	usedframes = snd_renderbuffer->endframe - snd_renderbuffer->startframe;
	maxtime = paintedtime + snd_renderbuffer->maxframes - usedframes;
	endtime = min(endtime, maxtime);

	S_PaintChannels(snd_renderbuffer, paintedtime, endtime);

	if (simsound)
		snd_renderbuffer->startframe = snd_renderbuffer->endframe;
	else
		SndSys_Submit();

	oldsoundtime = soundtime;
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
	matrix4x4_t basematrix, rotatematrix;

	if (snd_renderbuffer == NULL || nosound.integer)
		return;

	// If snd_swapstereo or snd_channellayout has changed, recompute the channel layout
	if (current_swapstereo != boolxor(snd_swapstereo.integer, v_flipped.integer) ||
		current_channellayout != snd_channellayout.integer)
		S_SetChannelLayout();

	Matrix4x4_Invert_Simple(&basematrix, listenermatrix);
	Matrix4x4_OriginFromMatrix(listenermatrix, listener_origin);

	// calculate the current matrices
	for (j = 0;j < SND_LISTENERS;j++)
	{
		Matrix4x4_CreateFromQuakeEntity(&rotatematrix, 0, 0, 0, 0, -snd_speakerlayout.listeners[j].yawangle, 0, 1);
		Matrix4x4_Concat(&listener_matrix[j], &rotatematrix, &basematrix);
		// I think this should now do this:
		//   1. create a rotation matrix for rotating by e.g. -90 degrees CCW
		//      (note: the matrix will rotate the OBJECT, not the VIEWER, so its
		//       angle has to be taken negative)
		//   2. create a transform which first rotates and moves its argument
		//      into the player's view coordinates (using basematrix which is
		//      an inverted "absolute" listener matrix), then applies the
		//      rotation matrix for the ear
		// Isn't Matrix4x4_CreateFromQuakeEntity a bit misleading because this
		// does not actually refer to an entity?
	}

	// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds
	ch = channels+NUM_AMBIENTS;
	for (i=NUM_AMBIENTS ; i<total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;

		// respatialize channel
		SND_Spatialize(ch, i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS);

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame
		if (i > MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
			// no need to merge silent channels
			for (j = 0;j < SND_LISTENERS;j++)
				if (ch->listener_volume[j])
					break;
			if (j == SND_LISTENERS)
				continue;
			// if the last combine chosen isn't suitable, find a new one
			if (!(combine && combine != ch && combine->sfx == ch->sfx))
			{
				// search for one
				combine = NULL;
				for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;j < i;j++)
				{
					if (channels[j].sfx == ch->sfx)
					{
						combine = channels + j;
						break;
					}
				}
			}
			if (combine && combine != ch && combine->sfx == ch->sfx)
			{
				for (j = 0;j < SND_LISTENERS;j++)
				{
					combine->listener_volume[j] += ch->listener_volume[j];
					ch->listener_volume[j] = 0;
				}
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
		{
			if (ch->sfx)
			{
				for (j = 0;j < SND_LISTENERS;j++)
					if (ch->listener_volume[j])
						break;
				if (j < SND_LISTENERS)
					total++;
			}
		}

		Con_Printf("----(%u)----\n", total);
	}

	S_PaintAndSubmit();
}

void S_ExtraUpdate (void)
{
	if (snd_noextraupdate.integer || !sound_spatialized)
		return;

	S_PaintAndSubmit();
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

	ch_ind = S_StartSound (cl.viewentity, 0, sfx, vec3_origin, 1, 0);
	if (ch_ind < 0)
		return false;

	channels[ch_ind].flags |= CHANNELFLAG_LOCALSOUND;
	return true;
}
