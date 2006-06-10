/*
	Copyright (C) 2006  Mathieu Olivier

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

// ALSA module, used by Linux


#include <alsa/asoundlib.h>

#include "quakedef.h"
#include "snd_main.h"


#define NB_PERIODS 2

static snd_pcm_t* pcm_handle = NULL;
static snd_pcm_sframes_t expected_delay = 0;
static unsigned int alsasoundtime;


/*
====================
SndSys_Init

Create "snd_renderbuffer" with the proper sound format if the call is successful
May return a suggested format if the requested format isn't available
====================
*/
qboolean SndSys_Init (const snd_format_t* requested, snd_format_t* suggested)
{
	const char* pcm_name;
	int i, err;
	snd_pcm_hw_params_t* hw_params = NULL;
	snd_pcm_format_t snd_pcm_format;
	snd_pcm_uframes_t buffer_size;

	Con_Print ("SndSys_Init: using the ALSA module\n");

	// Check the requested sound format
	if (requested->width < 1 || requested->width > 2)
	{
		Con_Printf ("SndSys_Init: invalid sound width (%hu)\n",
					requested->width);

		if (suggested != NULL)
		{
			memcpy (suggested, requested, sizeof (suggested));

			if (requested->width < 1)
				suggested->width = 1;
			else
				suggested->width = 2;

			Con_Printf ("SndSys_Init: suggesting sound width = %hu\n",
						suggested->width);
		}
		
		return false;
    }
	
	if (pcm_handle != NULL)
	{
		Con_Print ("SndSys_Init: WARNING: Init called before Shutdown!\n");
		SndSys_Shutdown ();
	}
	
	// Determine the name of the PCM handle we'll use
	switch (requested->channels)
	{
		case 4:
			pcm_name = "surround40";
			break;
		case 6:
			pcm_name = "surround51";
			break;
		case 8:
			pcm_name = "surround71";
			break;
		default:
			pcm_name = "default";
			break;
	}
// COMMANDLINEOPTION: Linux ALSA Sound: -sndpcm <devicename> selects which pcm device to us, default is "default"
	i = COM_CheckParm ("-sndpcm");
	if (i != 0 && i < com_argc - 1)
		pcm_name = com_argv[i + 1];

	// Open the audio device
	Con_DPrintf ("SndSys_Init: PCM device is \"%s\"\n", pcm_name);
	err = snd_pcm_open (&pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't open audio device \"%s\" (%s)\n",
					pcm_name, snd_strerror (err));
		return false;
	}
	
	// Allocate the hardware parameters
	err = snd_pcm_hw_params_malloc (&hw_params);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't allocate hardware parameters (%s)\n",
					snd_strerror (err));
		goto init_error;
	}
	err = snd_pcm_hw_params_any (pcm_handle, hw_params);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't initialize hardware parameters (%s)\n",
					snd_strerror (err));
		goto init_error;
	}

	// Set the access type
	err = snd_pcm_hw_params_set_access (pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set access type (%s)\n",
					snd_strerror (err));
		goto init_error;
	}

	// Set the sound width
	if (requested->width == 1)
		snd_pcm_format = SND_PCM_FORMAT_U8;
	else
		snd_pcm_format = SND_PCM_FORMAT_S16;
	err = snd_pcm_hw_params_set_format (pcm_handle, hw_params, snd_pcm_format);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set sound width to %hu (%s)\n",
					requested->width, snd_strerror (err));
		goto init_error;
	}

	// Set the sound channels
	err = snd_pcm_hw_params_set_channels (pcm_handle, hw_params, requested->channels);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set sound channels to %hu (%s)\n",
					requested->channels, snd_strerror (err));
		goto init_error;
	}

	// Set the sound speed
	err = snd_pcm_hw_params_set_rate (pcm_handle, hw_params, requested->speed, 0);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set sound speed to %u (%s)\n",
					requested->speed, snd_strerror (err));
		goto init_error;
	}

	buffer_size = requested->speed / 5;
	err = snd_pcm_hw_params_set_buffer_size_near (pcm_handle, hw_params, &buffer_size);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set sound buffer size to %lu (%s)\n",
					buffer_size, snd_strerror (err));
		goto init_error;
	}

	buffer_size /= NB_PERIODS;
	err = snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &buffer_size, 0);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set sound period size to %lu (%s)\n",
					buffer_size, snd_strerror (err));
		goto init_error;
	}

	err = snd_pcm_hw_params (pcm_handle, hw_params);
	if (err != 0)
	{
		Con_Printf ("SndSys_Init: can't set hardware parameters (%s)\n",
					snd_strerror (err));
		goto init_error;
	}

	snd_pcm_hw_params_free (hw_params);

	snd_renderbuffer = Snd_CreateRingBuffer(requested, 0, NULL);
	expected_delay = 0;
	alsasoundtime = 0;
	if (snd_channellayout.integer == SND_CHANNELLAYOUT_AUTO)
		Cvar_SetValueQuick (&snd_channellayout, SND_CHANNELLAYOUT_ALSA);
	
	return true;


// It's not very clean, but it avoids a lot of duplicated code.
init_error:
	
	if (hw_params != NULL)
		snd_pcm_hw_params_free (hw_params);
	SndSys_Shutdown ();
	return false;
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown (void)
{
	if (pcm_handle != NULL)
	{
		snd_pcm_close(pcm_handle);
		pcm_handle = NULL;
	}

	if (snd_renderbuffer != NULL)
	{
		Mem_Free(snd_renderbuffer->ring);
		Mem_Free(snd_renderbuffer);
		snd_renderbuffer = NULL;
	}
}


/*
====================
SndSys_Recover

Try to recover from errors
====================
*/
static qboolean SndSys_Recover (int err_num)
{
	int err;

	// We can only do something on underrun ("broken pipe") errors
	if (err_num != -EPIPE)
		return false;
			
	err = snd_pcm_prepare (pcm_handle);
	if (err != 0)
	{
		Con_DPrintf ("SndSys_Recover: unable to recover (%s)\n",
					 snd_strerror (err));

		// TOCHECK: should we stop the playback ?

		return false;
	}

	return true;
}


/*
====================
SndSys_Write
====================
*/
static snd_pcm_sframes_t SndSys_Write (const unsigned char* buffer, unsigned int nbframes)
{
	snd_pcm_sframes_t written;

	written = snd_pcm_writei (pcm_handle, buffer, nbframes);
	if (written < 0)
	{
		if (developer.integer >= 100)
			Con_Printf ("SndSys_Write: audio write returned %ld (%s)!\n",
						 written, snd_strerror (written));

		if (SndSys_Recover (written))
		{
			written = snd_pcm_writei (pcm_handle, buffer, nbframes);
			if (written < 0)
				Con_DPrintf ("SndSys_Write: audio write failed again (error %ld: %s)!\n",
							 written, snd_strerror (written));
		}
	}
	
	return written;
}


/*
====================
SndSys_Submit

Submit the contents of "snd_renderbuffer" to the sound card
====================
*/
void SndSys_Submit (void)
{
	unsigned int startoffset, factor;
 	snd_pcm_uframes_t limit, nbframes;
	snd_pcm_sframes_t written;
	
	if (pcm_handle == NULL ||
		snd_renderbuffer->startframe == snd_renderbuffer->endframe)
		return;

	startoffset = snd_renderbuffer->startframe % snd_renderbuffer->maxframes;
	factor = snd_renderbuffer->format.width * snd_renderbuffer->format.channels;
	limit = snd_renderbuffer->maxframes - startoffset;
	nbframes = snd_renderbuffer->endframe - snd_renderbuffer->startframe;

	if (nbframes > limit)
	{
		written = SndSys_Write (&snd_renderbuffer->ring[startoffset * factor], limit);
		if (written < 0)
			return;
		snd_renderbuffer->startframe += written;
		expected_delay += written;

		if ((snd_pcm_uframes_t)written != limit)
			return;
		
		nbframes -= limit;
		startoffset = 0;
	}

	written = SndSys_Write (&snd_renderbuffer->ring[startoffset * factor], nbframes);
	if (written < 0)
		return;

	snd_renderbuffer->startframe += written;
	expected_delay += written;
}


/*
====================
SndSys_GetSoundTime

Returns the number of sample frames consumed since the sound started
====================
*/
unsigned int SndSys_GetSoundTime (void)
{
	snd_pcm_sframes_t delay, timediff;
	int err;

	if (pcm_handle == NULL)
		return 0;

	err = snd_pcm_delay (pcm_handle, &delay);
	if (err != 0)
	{
		if (developer.integer >= 100)
			Con_DPrintf ("SndSys_GetSoundTime: can't get playback delay (%s)\n",
					 	 snd_strerror (err));

		if (! SndSys_Recover (err))
			return 0;

		err = snd_pcm_delay (pcm_handle, &delay);
		if (err != 0)
		{
			Con_DPrintf ("SndSys_GetSoundTime: can't get playback delay, again (%s)\n",
						 snd_strerror (err));
			return 0;
		}
	}

	if (expected_delay < delay)
	{
		Con_DPrintf ("SndSys_GetSoundTime: expected_delay(%ld) < delay(%ld)\n",
					 expected_delay, delay);
		timediff = 0;
	}
	else
		timediff = expected_delay - delay;
	expected_delay = delay;
	
	alsasoundtime += (unsigned int)timediff;
	
	return alsasoundtime;
}


/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
	// Nothing to do
	return true;
}


/*
====================
SndSys_UnlockRenderBuffer

Release the exclusive lock on "snd_renderbuffer"
====================
*/
void SndSys_UnlockRenderBuffer (void)
{
	// Nothing to do
}
