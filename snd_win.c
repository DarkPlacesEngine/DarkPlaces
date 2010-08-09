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

#ifdef SUPPORTDIRECTX
#ifndef DIRECTSOUND_VERSION
#	define DIRECTSOUND_VERSION 0x0500  /* Version 5.0 */
#endif
#endif
#include <windows.h>
#include <mmsystem.h>
#ifdef SUPPORTDIRECTX
#include <dsound.h>
#endif

#include "qtypes.h"
#include "quakedef.h"
#include "snd_main.h"

// ==============================================================================

#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct
{
	WAVEFORMATEX Format;
	union
	{
		WORD wValidBitsPerSample;	// bits of precision
		WORD wSamplesPerBlock;		// valid if wBitsPerSample==0
		WORD wReserved;				// If neither applies, set to zero
	} Samples;
    DWORD dwChannelMask;			// which channels are present in stream
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#	define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

// Some speaker positions
#ifndef SPEAKER_FRONT_LEFT
#	define SPEAKER_FRONT_LEFT				0x1
#	define SPEAKER_FRONT_RIGHT				0x2
#	define SPEAKER_FRONT_CENTER				0x4
#	define SPEAKER_LOW_FREQUENCY			0x8
#	define SPEAKER_BACK_LEFT				0x10
#	define SPEAKER_BACK_RIGHT				0x20
#	define SPEAKER_FRONT_LEFT_OF_CENTER		0x40
#	define SPEAKER_FRONT_RIGHT_OF_CENTER	0x80
// ... we never use the other values
#endif

// KSDATAFORMAT_SUBTYPE_PCM = GUID "00000001-0000-0010-8000-00aa00389b71"
static const GUID MY_KSDATAFORMAT_SUBTYPE_PCM =
{
	0x00000001,
	0x0000,
	0x0010,
	{
		0x80, 0x00,
		0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
	}
};


// ==============================================================================

extern HWND mainwindow;
static cvar_t snd_wav_partitionsize = {CVAR_SAVE, "snd_wav_partitionsize", "1024", "controls sound delay in samples, values too low will cause crackling, too high will cause delayed sounds"};
static qboolean sndsys_registeredcvars = false;

#ifdef SUPPORTDIRECTX
HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
#endif

// Wave output: queue of this many sound buffers to play, reused cyclically
#define	WAV_BUFFERS		16
#define	WAV_MASK		(WAV_BUFFERS - 1)
static unsigned int wav_buffer_size;

// DirectSound output: 64KB in 1 buffer
//#define SECONDARY_BUFFER_SIZE(fmt_ptr) ((fmt_ptr)->width * (fmt_ptr)->channels * (fmt_ptr)->speed / 2)
// LordHavoc: changed this to be a multiple of 32768
#define SECONDARY_BUFFER_SIZE(fmt_ptr) ((fmt_ptr)->channels * 32768)

typedef enum sndinitstat_e {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

#ifdef SUPPORTDIRECTX
static qboolean	dsound_init;
static unsigned int dsound_time;
static qboolean	primary_format_set;
#endif

static qboolean	wav_init;

static int	snd_sent, snd_completed;

static int prev_painted;
static unsigned int paintpot;



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

#ifdef SUPPORTDIRECTX
LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;
#endif

qboolean SNDDMA_InitWav (void);
#ifdef SUPPORTDIRECTX
sndinitstat SNDDMA_InitDirect (void);
#endif


/*
==================
SndSys_BuildWaveFormat
==================
*/
static qboolean SndSys_BuildWaveFormat (const snd_format_t* requested, WAVEFORMATEXTENSIBLE* fmt_ptr)
{
	WAVEFORMATEX* pfmtex;

	memset (fmt_ptr, 0, sizeof(*fmt_ptr));

	pfmtex = &fmt_ptr->Format;
	pfmtex->nChannels = requested->channels;
    pfmtex->wBitsPerSample = requested->width * 8;
    pfmtex->nSamplesPerSec = requested->speed;
	pfmtex->nBlockAlign = pfmtex->nChannels * pfmtex->wBitsPerSample / 8;
	pfmtex->nAvgBytesPerSec = pfmtex->nSamplesPerSec * pfmtex->nBlockAlign;

	// LordHavoc: disabled this WAVE_FORMAT_EXTENSIBLE support because it does not seem to be working
#if 0
	if (requested->channels <= 2)
	{
#endif
		pfmtex->wFormatTag = WAVE_FORMAT_PCM;
		pfmtex->cbSize = 0;
#if 0
	}
	else
	{
		pfmtex->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		pfmtex->cbSize = sizeof(*fmt_ptr) - sizeof(fmt_ptr->Format);
		fmt_ptr->Samples.wValidBitsPerSample = fmt_ptr->Format.wBitsPerSample;
		fmt_ptr->SubFormat = MY_KSDATAFORMAT_SUBTYPE_PCM;

		// Build the channel mask
		fmt_ptr->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		switch (requested->channels)
		{
			case 8:
				fmt_ptr->dwChannelMask |= SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER;
				// no break
			case 6:
				fmt_ptr->dwChannelMask |= SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY;
				// no break
			case 4:
				fmt_ptr->dwChannelMask |= SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
				break;

			default:
				Con_Printf("SndSys_BuildWaveFormat: invalid number of channels (%hu)\n", requested->channels);
				return false;
		}
	}
#endif

	return true;
}


#ifdef SUPPORTDIRECTX
/*
==================
SndSys_InitDirectSound

DirectSound 5 support
==================
*/
static sndinitstat SndSys_InitDirectSound (const snd_format_t* requested)
{
	DSBUFFERDESC			dsbuf;
	DSBCAPS					dsbcaps;
	DWORD					dwSize;
	DSCAPS					dscaps;
	WAVEFORMATEXTENSIBLE	format, pformat;
	HRESULT					hresult;
	int						reps;

	if (! SndSys_BuildWaveFormat(requested, &format))
		return SIS_FAILURE;

	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_Print("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (HRESULT (__stdcall *)(GUID *, LPDIRECTSOUND *,IUnknown *))GetProcAddress(hInstDS,"DirectSoundCreate");

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

			if (DS_OK != IDirectSoundBuffer_SetFormat (pDSPBuf, (WAVEFORMATEX*)&pformat))
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
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE(requested);
		dsbuf.lpwfxFormat = (WAVEFORMATEX*)&format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		result = IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL);
		if (result != DS_OK ||
			requested->channels != format.Format.nChannels ||
			requested->width != format.Format.wBitsPerSample / 8 ||
			requested->speed != format.Format.nSamplesPerSec)
		{
			Con_Printf("DS:CreateSoundBuffer Failed (%d): channels=%u, width=%u, speed=%u\n",
					   (int)result, (unsigned)format.Format.nChannels, (unsigned)format.Format.wBitsPerSample / 8, (unsigned)format.Format.nSamplesPerSec);
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

	Con_Printf("   %d channel(s)\n"
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
#endif


/*
==================
SndSys_InitMmsystem

Crappy windows multimedia base
==================
*/
static qboolean SndSys_InitMmsystem (const snd_format_t* requested)
{
	WAVEFORMATEXTENSIBLE format;
	int				i;
	HRESULT			hr;

	if (! SndSys_BuildWaveFormat(requested, &format))
		return false;

	// Open a waveform device for output using window callback
	while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, (WAVEFORMATEX*)&format,
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

	wav_buffer_size = bound(128, snd_wav_partitionsize.integer, 8192) * requested->channels * requested->width;

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.
	 */
	gSndBufSize = WAV_BUFFERS * wav_buffer_size;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_Print("Sound: Out of memory.\n");
		SndSys_Shutdown ();
		return false;
	}
	lpData = (HPSTR)GlobalLock(hData);
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
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);

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
		lpWaveHdr[i].dwBufferLength = wav_buffer_size;
		lpWaveHdr[i].lpData = lpData + i * wav_buffer_size;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
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
#ifdef SUPPORTDIRECTX
	qboolean wavonly;
#endif
	sndinitstat	stat;

	if (!sndsys_registeredcvars)
	{
		sndsys_registeredcvars = true;
		Cvar_RegisterVariable(&snd_wav_partitionsize);
	}

	Con_Print ("SndSys_Init: using the Win32 module\n");

#ifdef SUPPORTDIRECTX
// COMMANDLINEOPTION: Windows Sound: -wavonly uses wave sound instead of DirectSound
	wavonly = (COM_CheckParm ("-wavonly") != 0);
	dsound_init = false;
#endif
	wav_init = false;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

#ifdef SUPPORTDIRECTX
	// Init DirectSound
	if (!wavonly)
	{
		stat = SndSys_InitDirectSound (requested);

		if (stat == SIS_SUCCESS)
			Con_Print("DirectSound initialized\n");
		else
			Con_Print("DirectSound failed to init\n");
	}
#endif

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
#ifdef SUPPORTDIRECTX
	if (!dsound_init && (stat != SIS_NOTAVAIL))
#endif
	{
		if (SndSys_InitMmsystem (requested))
			Con_Print("Wave sound (MMSYSTEM) initialized\n");
		else
			Con_Print("Wave sound failed to init\n");
	}

#ifdef SUPPORTDIRECTX
	return (dsound_init || wav_init);
#else
	return wav_init;
#endif
}


/*
====================
SndSys_Shutdown

Stop the sound card, delete "snd_renderbuffer" and free its other resources
====================
*/
void SndSys_Shutdown (void)
{
#ifdef SUPPORTDIRECTX
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
#endif

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

#ifdef SUPPORTDIRECTX
	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	dsound_init = false;
#endif
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
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
	if (paintpot > WAV_BUFFERS * wav_buffer_size)
		paintpot = WAV_BUFFERS * wav_buffer_size;
	prev_painted = snd_renderbuffer->endframe;

	// submit new sound blocks
	while (paintpot > wav_buffer_size)
	{
		h = lpWaveHdr + (snd_sent & WAV_MASK);

		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));
		if (wResult == MMSYSERR_NOERROR)
			snd_sent++;
		else if (wResult == WAVERR_STILLPLAYING)
		{
			if(developer_insane.integer)
				Con_DPrint("waveOutWrite failed (too much sound data)\n");
			//h->dwFlags |= WHDR_DONE;
			//snd_sent++;
		}
		else
		{
			Con_Printf("waveOutWrite failed, error code %d\n", (int) wResult);
			SndSys_Shutdown ();
			return;
		}

		paintpot -= wav_buffer_size;
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
	unsigned int factor;

	factor = snd_renderbuffer->format.width * snd_renderbuffer->format.channels;

#ifdef SUPPORTDIRECTX
	if (dsound_init)
	{
		DWORD dwTime;
		unsigned int diff;

		IDirectSoundBuffer_GetCurrentPosition(pDSBuf, &dwTime, NULL);
		diff = (unsigned int)(dwTime - dwStartTime) % (unsigned int)gSndBufSize;
		dwStartTime = dwTime;

		dsound_time += diff / factor;
		return dsound_time;
	}
#endif

	if (wav_init)
	{
		// Find which sound blocks have completed
		for (;;)
		{
			if (snd_completed == snd_sent)
			{
			//	Con_DPrint("Sound overrun\n");
				break;
			}

			if (!(lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
				break;

			snd_completed++;	// this buffer has been played
		}

		return (snd_completed * wav_buffer_size) / factor;

		/*
		 * S_PaintAndSubmit: WARNING: newsoundtime (soundtime (275 < 134217707)
		 * apparently this sound time wraps quite early?
		{
		MMRESULT res;
		MMTIME mmtime;

		mmtime.wType = TIME_SAMPLES;
		res = waveOutGetPosition(hWaveOut, &mmtime, sizeof(mmtime));
		if(res == MMSYSERR_NOERROR)
			return mmtime.u.sample;
		}
		*/
	}

	return 0;
}


#ifdef SUPPORTDIRECTX
static DWORD dsound_dwSize;
static DWORD dsound_dwSize2;
static DWORD *dsound_pbuf;
static DWORD *dsound_pbuf2;
#endif

/*
====================
SndSys_LockRenderBuffer

Get the exclusive lock on "snd_renderbuffer"
====================
*/
qboolean SndSys_LockRenderBuffer (void)
{
#ifdef SUPPORTDIRECTX
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
#endif

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
#ifdef SUPPORTDIRECTX
	if (pDSBuf)
		IDirectSoundBuffer_Unlock(pDSBuf, dsound_pbuf, dsound_dwSize, dsound_pbuf2, dsound_dwSize2);
#endif
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
