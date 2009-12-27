/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "quakedef.h"

#include <limits.h>
#include <pthread.h>

#include <CoreAudio/AudioHardware.h>

#include "snd_main.h"


#define CHUNK_SIZE 1024

static unsigned int submissionChunk = 0;  // in sample frames
static unsigned int coreaudiotime = 0;  // based on the number of chunks submitted so far
static qboolean s_isRunning = false;
static pthread_mutex_t coreaudio_mutex;
static AudioDeviceID outputDeviceID = kAudioDeviceUnknown;
static short *mixbuffer = NULL;


/*
====================
audioDeviceIOProc
====================
*/
static OSStatus audioDeviceIOProc(AudioDeviceID inDevice,
								  const AudioTimeStamp *inNow,
								  const AudioBufferList *inInputData,
								  const AudioTimeStamp *inInputTime,
								  AudioBufferList *outOutputData,
								  const AudioTimeStamp *inOutputTime,
								  void *inClientData)
{
	float *outBuffer;
	unsigned int frameCount, factor, sampleIndex;
	float scale = 1.0f / SHRT_MAX;

	outBuffer = (float*)outOutputData->mBuffers[0].mData;
	factor = snd_renderbuffer->format.channels * snd_renderbuffer->format.width;
	frameCount = 0;
	if (snd_blocked)
		scale = 0;

	// Lock the snd_renderbuffer
	if (SndSys_LockRenderBuffer())
	{
		unsigned int maxFrames, sampleCount;
		unsigned int startOffset, endOffset;
		const short *samples;

		if (snd_usethreadedmixing)
		{
			S_MixToBuffer(mixbuffer, submissionChunk);
			sampleCount = submissionChunk * snd_renderbuffer->format.channels;
			for (sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
				outBuffer[sampleIndex] = mixbuffer[sampleIndex] * scale;
			// unlock the mutex now
			SndSys_UnlockRenderBuffer();
			return 0;
		}

		// Transfert up to a chunk of sample frames from snd_renderbuffer to outBuffer
		maxFrames = snd_renderbuffer->endframe - snd_renderbuffer->startframe;
		if (maxFrames >= submissionChunk)
			frameCount = submissionChunk;
		else
			frameCount = maxFrames;

		// Convert the samples from shorts to floats.  Scale the floats to be [-1..1].
		startOffset = snd_renderbuffer->startframe % snd_renderbuffer->maxframes;
		endOffset = (snd_renderbuffer->startframe + frameCount) % snd_renderbuffer->maxframes;
		if (startOffset > endOffset)  // if the buffer wraps
		{
			sampleCount = (snd_renderbuffer->maxframes - startOffset) * snd_renderbuffer->format.channels;
			samples = (const short*)(&snd_renderbuffer->ring[startOffset * factor]);
			for (sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
				outBuffer[sampleIndex] = samples[sampleIndex] * scale;

			outBuffer = &outBuffer[sampleCount];
			sampleCount = frameCount * snd_renderbuffer->format.channels - sampleCount;
			samples = (const short*)(&snd_renderbuffer->ring[0]);
			for (sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
				outBuffer[sampleIndex] = samples[sampleIndex] * scale;
		}
		else
		{
			sampleCount = frameCount * snd_renderbuffer->format.channels;
			samples = (const short*)(&snd_renderbuffer->ring[startOffset * factor]);
			for (sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
				outBuffer[sampleIndex] = samples[sampleIndex] * scale;
		}

		snd_renderbuffer->startframe += frameCount;

		// unlock the mutex now
		SndSys_UnlockRenderBuffer();
	}

	// If there was not enough samples, complete with silence samples
	if (frameCount < submissionChunk)
	{
		unsigned int missingFrames;

		missingFrames = submissionChunk - frameCount;
		if (developer_insane.integer && vid_activewindow)
			Con_DPrintf("audioDeviceIOProc: %u sample frames missing\n", missingFrames);
		memset(&outBuffer[frameCount * snd_renderbuffer->format.channels], 0, missingFrames * sizeof(outBuffer[0]));
	}

	coreaudiotime += submissionChunk;
	return 0;
}


/*
====================
SndSys_Init

Create "snd_renderbuffer" with the proper sound format if the call is successful
May return a suggested format if the requested format isn't available
====================
*/
qboolean SndSys_Init (const snd_format_t* requested, snd_format_t* suggested)
{
	OSStatus status;
	UInt32 propertySize, bufferByteCount;
	AudioStreamBasicDescription streamDesc;

	if (s_isRunning)
		return true;

	Con_Printf("Initializing CoreAudio...\n");
	snd_threaded = false;

	if(requested->width != 2)
	{
		// we can only do 16bit per sample for now
		if(suggested != NULL)
		{
			memcpy (suggested, requested, sizeof (*suggested));
			suggested->width = 2;
		}
		return false;
	}

	// Get the output device
	propertySize = sizeof(outputDeviceID);
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propertySize, &outputDeviceID);
	if (status)
	{
		Con_Printf("CoreAudio: AudioDeviceGetProperty() returned %d when getting kAudioHardwarePropertyDefaultOutputDevice\n", (int)status);
		return false;
	}
	if (outputDeviceID == kAudioDeviceUnknown)
	{
		Con_Printf("CoreAudio: outputDeviceID is kAudioDeviceUnknown\n");
		return false;
	}

	// Configure the output device
	propertySize = sizeof(bufferByteCount);
	bufferByteCount = CHUNK_SIZE * sizeof(float) * requested->channels;
	status = AudioDeviceSetProperty(outputDeviceID, NULL, 0, false, kAudioDevicePropertyBufferSize, propertySize, &bufferByteCount);
	if (status)
	{
		Con_Printf("CoreAudio: AudioDeviceSetProperty() returned %d when setting kAudioDevicePropertyBufferSize to %d\n", (int)status, CHUNK_SIZE);
		return false;
	}

	propertySize = sizeof(bufferByteCount);
	status = AudioDeviceGetProperty(outputDeviceID, 0, false, kAudioDevicePropertyBufferSize, &propertySize, &bufferByteCount);
	if (status)
	{
		Con_Printf("CoreAudio: AudioDeviceGetProperty() returned %d when setting kAudioDevicePropertyBufferSize\n", (int)status);
		return false;
	}

	submissionChunk = bufferByteCount / sizeof(float);
	if (submissionChunk % requested->channels != 0)
	{
		Con_Print("CoreAudio: chunk size is NOT a multiple of the number of channels\n");
		return false;
	}
	submissionChunk /= requested->channels;
	Con_Printf("   Chunk size = %d sample frames\n", submissionChunk);

	// Print out the device status
	propertySize = sizeof(streamDesc);
	status = AudioDeviceGetProperty(outputDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &propertySize, &streamDesc);
	if (status)
	{
		Con_Printf("CoreAudio: AudioDeviceGetProperty() returned %d when getting kAudioDevicePropertyStreamFormat\n", (int)status);
		return false;
	}

	Con_Print ("   Hardware format:\n");
	Con_Printf("    %5d mSampleRate\n", (unsigned int)streamDesc.mSampleRate);
	Con_Printf("     %c%c%c%c mFormatID\n",
				(char)(streamDesc.mFormatID >> 24),
				(char)(streamDesc.mFormatID >> 16),
				(char)(streamDesc.mFormatID >>  8),
				(char)(streamDesc.mFormatID >>  0));
	Con_Printf("    %5u mBytesPerPacket\n", (unsigned int)streamDesc.mBytesPerPacket);
	Con_Printf("    %5u mFramesPerPacket\n", (unsigned int)streamDesc.mFramesPerPacket);
	Con_Printf("    %5u mBytesPerFrame\n", (unsigned int)streamDesc.mBytesPerFrame);
	Con_Printf("    %5u mChannelsPerFrame\n", (unsigned int)streamDesc.mChannelsPerFrame);
	Con_Printf("    %5u mBitsPerChannel\n", (unsigned int)streamDesc.mBitsPerChannel);

	// Suggest proper settings if they differ
	if (requested->channels != streamDesc.mChannelsPerFrame || requested->speed != streamDesc.mSampleRate)
	{
		if (suggested != NULL)
		{
			memcpy (suggested, requested, sizeof (*suggested));
			suggested->channels = streamDesc.mChannelsPerFrame;
			suggested->speed = streamDesc.mSampleRate;
		}
		return false;
	}

	if(streamDesc.mFormatID == kAudioFormatLinearPCM)
	{
		// Add the callback function
		status = AudioDeviceAddIOProc(outputDeviceID, audioDeviceIOProc, NULL);
		if (!status)
		{
			// We haven't sent any sample frames yet
			coreaudiotime = 0;
			if (pthread_mutex_init(&coreaudio_mutex, NULL) == 0)
			{
				if ((snd_renderbuffer = Snd_CreateRingBuffer(requested, 0, NULL)))
				{
					if ((mixbuffer = Mem_Alloc(snd_mempool, CHUNK_SIZE * sizeof(*mixbuffer) * requested->channels)))
					{
						// Start sound running
						status = AudioDeviceStart(outputDeviceID, audioDeviceIOProc);
						if (!status)
						{
							s_isRunning = true;
							snd_threaded = true;
							Con_Print("   Initialization successful\n");
							return true;
						}
						else
							Con_Printf("CoreAudio: AudioDeviceStart() returned %d\n", (int)status);
						Mem_Free(mixbuffer);
						mixbuffer = NULL;
					}
					else
						Con_Print("CoreAudio: can't allocate memory for mixbuffer\n");
					Mem_Free(snd_renderbuffer->ring);
					Mem_Free(snd_renderbuffer);
					snd_renderbuffer = NULL;
				}
				else
					Con_Print("CoreAudio: can't allocate memory for ringbuffer\n");
				pthread_mutex_destroy(&coreaudio_mutex);
			}
			else
				Con_Print("CoreAudio: can't create pthread mutex\n");
			AudioDeviceRemoveIOProc(outputDeviceID, audioDeviceIOProc);
		}
		else
			Con_Printf("CoreAudio: AudioDeviceAddIOProc() returned %d\n", (int)status);
	}
	else
		Con_Print("CoreAudio: Default audio device doesn't support linear PCM!\n");
	return false;
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown(void)
{
	OSStatus status;

	if (!s_isRunning)
		return;

	status = AudioDeviceStop(outputDeviceID, audioDeviceIOProc);
	if (status)
	{
		Con_Printf("AudioDeviceStop: returned %d\n", (int)status);
		return;
	}
	s_isRunning = false;

	pthread_mutex_destroy(&coreaudio_mutex);

	status = AudioDeviceRemoveIOProc(outputDeviceID, audioDeviceIOProc);
	if (status)
	{
		Con_Printf("AudioDeviceRemoveIOProc: returned %d\n", (int)status);
		return;
	}

	if (snd_renderbuffer != NULL)
	{
		Mem_Free(snd_renderbuffer->ring);
		Mem_Free(snd_renderbuffer);
		snd_renderbuffer = NULL;
	}

	if (mixbuffer != NULL)
		Mem_Free(mixbuffer);
	mixbuffer = NULL;
}


/*
====================
SndSys_Submit

Submit the contents of "snd_renderbuffer" to the sound card
====================
*/
void SndSys_Submit (void)
{
	// Nothing to do here (this sound module is callback-based)
}


/*
====================
SndSys_GetSoundTime

Returns the number of sample frames consumed since the sound started
====================
*/
unsigned int SndSys_GetSoundTime (void)
{
	return coreaudiotime;
}


/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
	return (pthread_mutex_lock(&coreaudio_mutex) == 0);
}


/*
====================
SndSys_UnlockRenderBuffer

Release the exclusive lock on "snd_renderbuffer"
====================
*/
void SndSys_UnlockRenderBuffer (void)
{
	pthread_mutex_unlock(&coreaudio_mutex);
}

/*
====================
SndSys_SendKeyEvents

Send keyboard events originating from the sound system (e.g. MIDI)
====================
*/
void SndSys_SendKeyEvents(void)
{
	// not supported
}
