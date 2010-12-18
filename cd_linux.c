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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

// suggested by Zero_Dogg to fix a compile problem on Mandriva Linux
#include "quakedef.h"

#include <linux/cdrom.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "cdaudio.h"


static int cdfile = -1;
static char cd_dev[64] = "/dev/cdrom";


void CDAudio_SysEject (void)
{
	if (cdfile == -1)
		return;

	if (ioctl(cdfile, CDROMEJECT) == -1)
		Con_Print("ioctl CDROMEJECT failed\n");
}


void CDAudio_SysCloseDoor (void)
{
	if (cdfile == -1)
		return;

	if (ioctl(cdfile, CDROMCLOSETRAY) == -1)
		Con_Print("ioctl CDROMCLOSETRAY failed\n");
}

int CDAudio_SysGetAudioDiskInfo (void)
{
	struct cdrom_tochdr tochdr;

	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDROMREADTOCHDR, &tochdr) == -1)
	{
		Con_Print("ioctl CDROMREADTOCHDR failed\n");
		return -1;
	}

	if (tochdr.cdth_trk0 < 1)
	{
		Con_Print("CDAudio: no music tracks\n");
		return -1;
	}

	return tochdr.cdth_trk1;
}


float CDAudio_SysGetVolume (void)
{
	struct cdrom_volctrl vol;

	if (cdfile == -1)
		return -1.0f;

	if (ioctl (cdfile, CDROMVOLREAD, &vol) == -1)
	{
		Con_Print("ioctl CDROMVOLREAD failed\n");
		return -1.0f;
	}

	return (vol.channel0 + vol.channel1) / 2.0f / 255.0f;
}


void CDAudio_SysSetVolume (float volume)
{
	struct cdrom_volctrl vol;

	if (cdfile == -1)
		return;

	vol.channel0 = vol.channel1 = (__u8)(volume * 255);
	vol.channel2 = vol.channel3 = 0;

	if (ioctl (cdfile, CDROMVOLCTRL, &vol) == -1)
		Con_Print("ioctl CDROMVOLCTRL failed\n");
}


int CDAudio_SysPlay (int track)
{
	struct cdrom_tocentry entry;
	struct cdrom_ti ti;

	if (cdfile == -1)
		return -1;

	// don't try to play a non-audio track
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;
	if (ioctl(cdfile, CDROMREADTOCENTRY, &entry) == -1)
	{
		Con_Print("ioctl CDROMREADTOCENTRY failed\n");
		return -1;
	}
	if (entry.cdte_ctrl == CDROM_DATA_TRACK)
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return -1;
	}

	if (cdPlaying)
		CDAudio_Stop();

	ti.cdti_trk0 = track;
	ti.cdti_trk1 = track;
	ti.cdti_ind0 = 1;
	ti.cdti_ind1 = 99;

	if (ioctl(cdfile, CDROMPLAYTRKIND, &ti) == -1)
	{
		Con_Print("ioctl CDROMPLAYTRKIND failed\n");
		return -1;
	}

	if (ioctl(cdfile, CDROMRESUME) == -1)
	{
		Con_Print("ioctl CDROMRESUME failed\n");
		return -1;
	}

	return 0;
}


int CDAudio_SysStop (void)
{
	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDROMSTOP) == -1)
	{
		Con_Printf("ioctl CDROMSTOP failed (%d)\n", errno);
		return -1;
	}

	return 0;
}

int CDAudio_SysPause (void)
{
	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDROMPAUSE) == -1)
	{
		Con_Print("ioctl CDROMPAUSE failed\n");
		return -1;
	}

	return 0;
}


int CDAudio_SysResume (void)
{
	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDROMRESUME) == -1)
		Con_Print("ioctl CDROMRESUME failed\n");

	return 0;
}

int CDAudio_SysUpdate (void)
{
	struct cdrom_subchnl subchnl;
	static time_t lastchk = 0;

	if (cdPlaying && lastchk < time(NULL) && cdfile != -1)
	{
		lastchk = time(NULL) + 2; //two seconds between chks
		subchnl.cdsc_format = CDROM_MSF;
		if (ioctl(cdfile, CDROMSUBCHNL, &subchnl) == -1)
		{
			Con_Print("ioctl CDROMSUBCHNL failed\n");
			cdPlaying = false;
			return -1;
		}
		if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY &&
			subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED)
		{
			cdPlaying = false;
			if (cdPlayLooping)
				CDAudio_Play(cdPlayTrack, true);
		}
		else
			cdPlayTrack = subchnl.cdsc_trk;
	}

	return 0;
}

void CDAudio_SysInit (void)
{
	int i;

// COMMANDLINEOPTION: Linux Sound: -cddev <devicepath> chooses which CD drive to use
	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1)
		strlcpy(cd_dev, com_argv[i + 1], sizeof(cd_dev));
}

int CDAudio_SysStartup (void)
{
	if ((cdfile = open(cd_dev, O_RDONLY | O_NONBLOCK)) == -1)
	{
		Con_Printf("CDAudio_SysStartup: open of \"%s\" failed (%i)\n",
					cd_dev, errno);
		cdfile = -1;
		return -1;
	}

	return 0;
}

void CDAudio_SysShutdown (void)
{
	close(cdfile);
	cdfile = -1;
}
