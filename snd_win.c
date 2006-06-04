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
#include "quakedef.h"
#include "snd_main.h"

#ifndef DIRECTSOUND_VERSION
#	define DIRECTSOUND_VERSION 0x0500  /* Version 5.0 */
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

extern HWND mainwindow;

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);

// Wave output: 64KB in 64 buffers of 1KB
// (64KB is > 1 sec at 16-bit 22050 Hz mono, and is 1/3 sec at 16-bit 44100 Hz stereo)
#define	WAV_BUFFERS				64
#define	WAV_MASK				(WAV_BUFFERS - 1)
#define	WAV_BUFFER_SIZE			1024

// DirectSound output: 64KB in 1 buffer
#define SECONDARY_BUFFER_SIZE	(64 * 1024)

typedef enum sndinitstat_e {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

static qboolean	dsound_init;
static qboolean	wav_init;
static qboolean	primary_format_set;

static int	snd_sent, snd_completed;

static int prev_painted;
static unsigned int paintpot;

static unsigned int dsound_time;


/*
 * Global variables. Must be visible to window-procedure function
 *  so it can unlock and free the data block after it has been played.
 */

HANDLE		hData;
HPSTR		lpData, lpData2;

HGLOBAL		hWaveHdr;
LPWAVEHDR	lpWaveHdr;

HWAVEOUT    hWaveOut;

WAVEOUTCAPS	wavecaps;

DWORD	gSndBufSize;

DWORD	dwStartTime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

qboolean SNDDMA_InitWav (void);
sndinitstat SNDDMA_InitDirect (void);


/*
==================
SndSys_InitDirectSound

DirectSound 5 support
==================
*/
static sndinitstat SndSys_InitDirectSound (const snd_format_t* requested, snd_format_t* suggested)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize;
	DSCAPS			dscaps;
	WAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = requested->channels;
    format.wBitsPerSample = requested->width * 8;
    format.nSamplesPerSec = requested->speed;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0;

	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_Print("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (void *)GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate)
		{
			Con_Print("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}
	}

	while ((hresult = pDirectSoundCreate(NULL, &pDS, NULL)) != DS_OK)
	{
		if (hresult != DSERR_ALLOCATED)
		{
			Con_Print("DirectSound create failed\n");
			return SIS_FAILURE;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
					    "Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_Print("DirectSoundCreate failure\n  hardware already in use\n");
			return SIS_NOTAVAIL;
		}
	}

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != IDirectSound_GetCaps (pDS, &dscaps))
	{
		Con_Print("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_Print("No DirectSound driver installed\n");
		SndSys_Shutdown ();
		return SIS_FAILURE;
	}

	if (DS_OK != IDirectSound_SetCooperativeLevel (pDS, mainwindow, DSSCL_EXCLUSIVE))
	{
		Con_Print("Set coop level failed\n");
		SndSys_Shutdown ();
		return SIS_FAILURE;
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

// COMMANDLINEOPTION: Windows DirectSound: -snoforceformat uses the format that DirectSound returns, rather than forcing it
	if (!COM_CheckParm ("-snoforceformat"))
	{
		if (DS_OK == IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != IDirectSoundBuffer_SetFormat (pDSPBuf, &pformat))
			{
				Con_Print("Set primary sound buffer format: no\n");
			}
			else
			{
				Con_Print("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

// COMMANDLINEOPTION: Windows DirectSound: -primarysound locks the sound hardware for exclusive use
	if (!primary_format_set || !COM_CheckParm ("-primarysound"))
	{
		HRESULT result;

		// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		result = IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL);
		if (result != DS_OK ||
			requested->channels != format.nChannels ||
			requested->width != format.wBitsPerSample / 8 ||
			requested->speed != format.nSamplesPerSec)
		{
			Con_Printf("DS:CreateSoundBuffer Failed (%d): channels=%u, width=%u, speed=%u\n",
					   result, format.nChannels, format.wBitsPerSample / 8, format.nSamplesPerSec);
			SndSys_Shutdown ();
			return SIS_FAILURE;
		}

		if (DS_OK != IDirectSoundBuffer_GetCaps (pDSBuf, &dsbcaps))
		{
			Con_Print("DS:GetCaps failed\n");
			SndSys_Shutdown ();
			return SIS_FAILURE;
		}

		Con_Print("Using secondary sound buffer\n");
	}
	else
	{
		if (DS_OK != IDirectSound_SetCooperativeLevel (pDS, mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_Print("Set coop level failed\n");
			SndSys_Shutdown ();
			return SIS_FAILURE;
		}

		if (DS_OK != IDirectSoundBuffer_GetCaps (pDSPBuf, &dsbcaps))
		{
			Con_Print("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_Print("Using primary sound buffer\n");
	}

	// Make sure mixer is active
	IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	Con_DPrintf("   %d channel(s)\n"
				"   %d bits/sample\n"
				"   %d samples/sec\n",
				requested->channels, requested->width * 8, requested->speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

	// initialize the buffer
	reps = 0;

	while ((hresult = IDirectSoundBuffer_Lock(pDSBuf, 0, gSndBufSize, (LPVOID*)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_Print("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			SndSys_Shutdown ();
			return SIS_FAILURE;
		}

		if (++reps > 10000)
		{
			Con_Print("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			SndSys_Shutdown ();
			return SIS_FAILURE;
		}

	}

	memset(lpData, 0, dwSize);
	IDirectSoundBuffer_Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	IDirectSoundBuffer_Stop(pDSBuf);
	IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	dwStartTime = 0;
	dsound_time = 0;
	snd_renderbuffer = Snd_CreateRingBuffer(requested, gSndBufSize / (requested->width * requested->channels), lpData);

	dsound_init = true;

	return SIS_SUCCESS;
}


/*
==================
SndSys_InitMmsystem

Crappy windows multimedia base
==================
*/
static qboolean SndSys_InitMmsystem (const snd_format_t* requested, snd_format_t* suggested)
{
	WAVEFORMATEX  format;
	int				i;
	HRESULT			hr;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = requested->channels;
    format.wBitsPerSample = requested->width * 8;
    format.nSamplesPerSec = requested->speed;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize = 0;

	// Open a waveform device for output using window callback
	while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER,
					&format,
					0, 0L, CALLBACK_NULL)) != MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED)
		{
			Con_Print("waveOutOpen failed\n");
			return false;
		}

		if (MessageBox (NULL,
						"The sound hardware is in use by another app.\n\n"
					    "Select Retry to try to start sound again or Cancel to run Quake with no sound.",
						"Sound not available",
						MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{
			Con_Print("waveOutOpen failure;\n  hardware already in use\n");
			return false;
		}
	}

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.
	 */
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_Print("Sound: Out of memory.\n");
		SndSys_Shutdown ();
		return false;
	}
	lpData = GlobalLock(hData);
	if (!lpData)
	{
		Con_Print("Sound: Failed to lock.\n");
		SndSys_Shutdown ();
		return false;
	}
	memset (lpData, 0, gSndBufSize);

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
		(DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);

	if (hWaveHdr == NULL)
	{
		Con_Print("Sound: Failed to Alloc header.\n");
		SndSys_Shutdown ();
		return false;
	}

	lpWaveHdr = (LPWAVEHDR) GlobalLock(hWaveHdr);

	if (lpWaveHdr == NULL)
	{
		Con_Print("Sound: Failed to lock header.\n");
		SndSys_Shutdown ();
		return false;
	}

	memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	// After allocation, set up and prepare headers
	for (i=0 ; i<WAV_BUFFERS ; i++)
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) !=
				MMSYSERR_NOERROR)
		{
			Con_Print("Sound: failed to prepare wave headers\n");
			SndSys_Shutdown ();
			return false;
		}
	}

	snd_renderbuffer = Snd_CreateRingBuffer(requested, gSndBufSize / (requested->width * requested->channels), lpData);

	prev_painted = 0;
	paintpot = 0;

	snd_sent = 0;
	snd_completed = 0;

	wav_init = true;

	return true;
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
	qboolean wavonly;
	sndinitstat	stat;

	Con_Print ("SndSys_Init: using the Win32 module\n");

// COMMANDLINEOPTION: Windows Sound: -wavonly uses wave sound instead of DirectSound
	wavonly = (COM_CheckParm ("-wavonly") != 0);
	dsound_init = false;
	wav_init = false;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	// Init DirectSound
	if (!wavonly)
	{
		stat = SndSys_InitDirectSound (requested, suggested);

		if (stat == SIS_SUCCESS)
			Con_Print("DirectSound initialized\n");
		else
			Con_Print("DirectSound failed to init\n");
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
	if (!dsound_init && (stat != SIS_NOTAVAIL))
	{
		if (SndSys_InitMmsystem (requested, suggested))
			Con_Print("Wave sound (MMSYSTEM) initialized\n");
		else
			Con_Print("Wave sound failed to init\n");
	}

	return (dsound_init || wav_init);
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown (void)
{
	if (pDSBuf)
	{
		IDirectSoundBuffer_Stop(pDSBuf);
		IDirectSoundBuffer_Release(pDSBuf);
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		IDirectSoundBuffer_Release(pDSPBuf);
	}

	if (pDS)
	{
		IDirectSound_SetCooperativeLevel (pDS, mainwindow, DSSCL_NORMAL);
		IDirectSound_Release(pDS);
	}

	if (hWaveOut)
	{
		waveOutReset (hWaveOut);

		if (lpWaveHdr)
		{
			unsigned int i;

			for (i=0 ; i< WAV_BUFFERS ; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR));
		}

		waveOutClose (hWaveOut);

		if (hWaveHdr)
		{
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData)
		{
			GlobalUnlock(hData);
			GlobalFree(hData);
		}
	}

	if (snd_renderbuffer != NULL)
	{
		Mem_Free(snd_renderbuffer);
		snd_renderbuffer = NULL;
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}


/*
====================
SndSys_Submit

Submit the contents of "snd_renderbuffer" to the sound card
====================
*/
void SndSys_Submit (void)
{
	LPWAVEHDR	h;
	int			wResult;

	// DirectSound doesn't need this
	if (!wav_init)
		return;

	paintpot += (snd_renderbuffer->endframe - prev_painted) * snd_renderbuffer->format.channels * snd_renderbuffer->format.width;
	if (paintpot > WAV_BUFFERS * WAV_BUFFER_SIZE)
		paintpot = WAV_BUFFERS * WAV_BUFFER_SIZE;
	prev_painted = snd_renderbuffer->endframe;

	// submit new sound blocks
	while (paintpot > WAV_BUFFER_SIZE)
	{
		h = lpWaveHdr + (snd_sent & WAV_MASK);

		snd_sent++;
		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Con_Print("Failed to write block to device\n");
			SndSys_Shutdown ();
			return;
		}

		paintpot -= WAV_BUFFER_SIZE;
	}

}


/*
====================
SndSys_GetSoundTime

Returns the number of sample frames consumed since the sound started
====================
*/
unsigned int SndSys_GetSoundTime (void)
{
	unsigned int s;

	if (dsound_init)
	{
		DWORD dwTime;

		IDirectSoundBuffer_GetCurrentPosition(pDSBuf, &dwTime, NULL);
		s = (dwTime - dwStartTime) & (gSndBufSize - 1);
		dwStartTime = dwTime;

		dsound_time += s / (snd_renderbuffer->format.width * snd_renderbuffer->format.channels);
		return dsound_time;
	}

	if (wav_init)
	{
		// Find which sound blocks have completed
		for (;;)
		{
			if (snd_completed == snd_sent)
			{
				Con_DPrint("Sound overrun\n");
				break;
			}

			if (!(lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
				break;

			snd_completed++;	// this buffer has been played
		}

		s = snd_completed * WAV_BUFFER_SIZE;

		return s / (snd_renderbuffer->format.width * snd_renderbuffer->format.channels);
	}

	return 0;
}


static DWORD dsound_dwSize;
static DWORD dsound_dwSize2;
static DWORD *dsound_pbuf;
static DWORD *dsound_pbuf2;

/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
	int reps;
	HRESULT hresult;
	DWORD	dwStatus;

	if (pDSBuf)
	{
		// if the buffer was lost or stopped, restore it and/or restart it
		if (IDirectSoundBuffer_GetStatus (pDSBuf, &dwStatus) != DS_OK)
			Con_Print("Couldn't get sound buffer status\n");

		if (dwStatus & DSBSTATUS_BUFFERLOST)
		{
			Con_Print("DSound buffer is lost!!\n");
			IDirectSoundBuffer_Restore (pDSBuf);
		}

		if (!(dwStatus & DSBSTATUS_PLAYING))
			IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

		reps = 0;

		while ((hresult = IDirectSoundBuffer_Lock(pDSBuf, 0, gSndBufSize, (LPVOID*)&dsound_pbuf, &dsound_dwSize, (LPVOID*)&dsound_pbuf2, &dsound_dwSize2, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Print("S_LockBuffer: DS: Lock Sound Buffer Failed\n");
				S_Shutdown ();
				S_Startup ();
				return false;
			}

			if (++reps > 10000)
			{
				Con_Print("S_LockBuffer: DS: couldn't restore buffer\n");
				S_Shutdown ();
				S_Startup ();
				return false;
			}
		}

		if ((void*)dsound_pbuf != snd_renderbuffer->ring)
			Sys_Error("SndSys_LockRenderBuffer: the ring address has changed!!!\n");
		return true;
	}

	return wav_init;
}


/*
====================
SndSys_UnlockRenderBuffer

Release the exclusive lock on "snd_renderbuffer"
====================
*/
void SndSys_UnlockRenderBuffer (void)
{
	if (pDSBuf)
		IDirectSoundBuffer_Unlock(pDSBuf, dsound_pbuf, dsound_dwSize, dsound_pbuf2, dsound_dwSize2);
}
