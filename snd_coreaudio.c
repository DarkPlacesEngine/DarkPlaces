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

// snd_coreaudio.c
// all other sound mixing is portable

#include <limits.h>

#include <CoreAudio/AudioHardware.h>

#include "quakedef.h"
#include "snd_main.h"

// BUFFER_SIZE must be an even multiple of CHUNK_SIZE
#define CHUNK_SIZE 2048
#define BUFFER_SIZE 16384

static unsigned int submissionChunk;
static unsigned int maxMixedSamples;
static short *s_mixedSamples;
static int s_chunkCount;  // number of chunks submitted
static qboolean s_isRunning;

static AudioDeviceID outputDeviceID;
static AudioStreamBasicDescription outputStreamBasicDescription;


/*
===============
audioDeviceIOProc
===============
*/

OSStatus audioDeviceIOProc(AudioDeviceID inDevice,
						   const AudioTimeStamp *inNow,
						   const AudioBufferList *inInputData,
						   const AudioTimeStamp *inInputTime,
						   AudioBufferList *outOutputData,
						   const AudioTimeStamp *inOutputTime,
						   void *inClientData)
{
	int	offset;
	short *samples;
	unsigned int sampleIndex;
	float *outBuffer;
	float scale;

	offset = (s_chunkCount * submissionChunk) % maxMixedSamples;
	samples = s_mixedSamples + offset;

	outBuffer = (float *)outOutputData->mBuffers[0].mData;

	// If we have run out of samples, return silence
	if (s_chunkCount * submissionChunk > shm->format.channels * paintedtime)
	{
		memset(outBuffer, 0, sizeof(*outBuffer) * submissionChunk);
	}
	else
	{
		// Convert the samples from shorts to floats.  Scale the floats to be [-1..1].
		scale = (1.0f / SHRT_MAX);
		for (sampleIndex = 0; sampleIndex < submissionChunk; sampleIndex++)
			outBuffer[sampleIndex] = samples[sampleIndex] * scale;

		s_chunkCount++; // this is the next buffer we will submit
	}

	return 0;
}

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init(void)
{
	OSStatus status;
	UInt32 propertySize, bufferByteCount;

	if (s_isRunning)
		return true;

	Con_Printf("Initializing CoreAudio...\n");

	// Get the output device
	propertySize = sizeof(outputDeviceID);
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propertySize, &outputDeviceID);
	if (status)
	{
		Con_Printf("AudioHardwareGetProperty returned %d\n", status);
		return false;
	}

	if (outputDeviceID == kAudioDeviceUnknown)
	{
		Con_Printf("AudioHardwareGetProperty: outputDeviceID is kAudioDeviceUnknown\n");
		return false;
	}

	// Configure the output device
	// TODO: support "-sndspeed", "-sndmono" and "-sndstereo"
	propertySize = sizeof(bufferByteCount);
	bufferByteCount = CHUNK_SIZE * sizeof(float);
	status = AudioDeviceSetProperty(outputDeviceID, NULL, 0, false, kAudioDevicePropertyBufferSize, propertySize, &bufferByteCount);
	if (status)
	{
		Con_Printf("AudioDeviceSetProperty: returned %d when setting kAudioDevicePropertyBufferSize to %d\n", status, CHUNK_SIZE);
		return false;
	}

	propertySize = sizeof(bufferByteCount);
	status = AudioDeviceGetProperty(outputDeviceID, 0, false, kAudioDevicePropertyBufferSize, &propertySize, &bufferByteCount);
	if (status)
	{
		Con_Printf("AudioDeviceGetProperty: returned %d when setting kAudioDevicePropertyBufferSize\n", status);
		return false;
	}
	submissionChunk = bufferByteCount / sizeof(float);
	Con_DPrintf("   Chunk size = %d samples\n", submissionChunk);

	// Print out the device status
	propertySize = sizeof(outputStreamBasicDescription);
	status = AudioDeviceGetProperty(outputDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &propertySize, &outputStreamBasicDescription);
	if (status)
	{
		Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyStreamFormat\n", status);
		return false;
	}
	Con_DPrintf("   Hardware format:\n");
	Con_DPrintf("	 %5d mSampleRate\n", (unsigned int)outputStreamBasicDescription.mSampleRate);
	Con_DPrintf("	  %c%c%c%c mFormatID\n",
				(outputStreamBasicDescription.mFormatID & 0xff000000) >> 24,
				(outputStreamBasicDescription.mFormatID & 0x00ff0000) >> 16,
				(outputStreamBasicDescription.mFormatID & 0x0000ff00) >>  8,
				(outputStreamBasicDescription.mFormatID & 0x000000ff) >>  0);
	Con_DPrintf("	 %5d mBytesPerPacket\n", outputStreamBasicDescription.mBytesPerPacket);
	Con_DPrintf("	 %5d mFramesPerPacket\n", outputStreamBasicDescription.mFramesPerPacket);
	Con_DPrintf("	 %5d mBytesPerFrame\n", outputStreamBasicDescription.mBytesPerFrame);
	Con_DPrintf("	 %5d mChannelsPerFrame\n", outputStreamBasicDescription.mChannelsPerFrame);
	Con_DPrintf("	 %5d mBitsPerChannel\n", outputStreamBasicDescription.mBitsPerChannel);

	if(outputStreamBasicDescription.mFormatID != kAudioFormatLinearPCM)
	{
		Con_Printf("Default Audio Device doesn't support Linear PCM!\n");
		return false;
	}

	// Start sound running
	status = AudioDeviceAddIOProc(outputDeviceID, audioDeviceIOProc, NULL);
	if (status)
	{
		Con_Printf("AudioDeviceAddIOProc: returned %d\n", status);
		return false;
	}

	maxMixedSamples = BUFFER_SIZE;
	s_mixedSamples = (short *)Mem_Alloc (snd_mempool, sizeof(*s_mixedSamples) * maxMixedSamples);
	Con_DPrintf("   Buffer size = %d samples (%d chunks)\n",
				maxMixedSamples, (maxMixedSamples / submissionChunk));

	// Tell the main app what we expect from it
	memset ((void*)shm, 0, sizeof (*shm));
	shm->format.speed = outputStreamBasicDescription.mSampleRate;
	shm->format.channels = outputStreamBasicDescription.mChannelsPerFrame;
	shm->format.width = 2;
	shm->samples = maxMixedSamples;
	shm->buffer = (unsigned char *)s_mixedSamples;
	shm->samplepos = 0;

	// We haven't enqueued anything yet
	s_chunkCount = 0;

	status = AudioDeviceStart(outputDeviceID, audioDeviceIOProc);
	if (status)
	{
		Con_Printf("AudioDeviceStart: returned %d\n", status);
		return false;
	}

	s_isRunning = true;

	Con_Printf("   Initialization successful\n");

	return true;
}

/*
===============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	return (s_chunkCount * submissionChunk) % shm->samples;
}

/*
===============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	OSStatus status;

	if (!s_isRunning)
		return;

	status = AudioDeviceStop(outputDeviceID, audioDeviceIOProc);
	if (status)
	{
		Con_Printf("AudioDeviceStop: returned %d\n", status);
		return;
	}

	s_isRunning = false;

	status = AudioDeviceRemoveIOProc(outputDeviceID, audioDeviceIOProc);
	if (status)
	{
		Con_Printf("AudioDeviceRemoveIOProc: returned %d\n", status);
		return;
	}

	Mem_Free(s_mixedSamples);
	s_mixedSamples = NULL;
	shm->buffer = NULL;
}

/*
===============
SNDDMA_Submit
===============
*/
void SNDDMA_Submit(void)
{
	// nothing to do (CoreAudio is callback-based)
}

/*
===============
S_LockBuffer
===============
*/
void *S_LockBuffer(void)
{
	// not necessary (just return the buffer)
	return shm->buffer;
}

/*
===============
S_UnlockBuffer
===============
*/
void S_UnlockBuffer(void)
{
	// not necessary
}
