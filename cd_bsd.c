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

#include <sys/types.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <util.h>

#include "quakedef.h"


static int cdfile = -1;
static char cd_dev[64] = _PATH_DEV "cd0";


void CDAudio_SysEject (void)
{
	if (cdfile == -1)
		return;

	ioctl(cdfile, CDIOCALLOW);
	if (ioctl(cdfile, CDIOCEJECT) == -1)
		Con_DPrint("ioctl CDIOCEJECT failed\n");
}


void CDAudio_SysCloseDoor (void)
{
	if (cdfile == -1)
		return;

	ioctl(cdfile, CDIOCALLOW);
	if (ioctl(cdfile, CDIOCCLOSE) == -1)
		Con_DPrint("ioctl CDIOCCLOSE failed\n");
}

int CDAudio_SysGetAudioDiskInfo (void)
{
	struct ioc_toc_header tochdr;

	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDIOREADTOCHEADER, &tochdr) == -1)
	{
		Con_DPrint("ioctl CDIOREADTOCHEADER failed\n");
		return -1;
	}

	if (tochdr.starting_track < 1)
	{
		Con_DPrint("CDAudio: no music tracks\n");
		return -1;
	}

	return tochdr.ending_track;
}


float CDAudio_SysGetVolume (void)
{
	// IMPLEMENTME
	return -1.0f;
}


void CDAudio_SysSetVolume (float volume)
{
	// IMPLEMENTME
}


int CDAudio_SysPlay (qbyte track)
{
	struct ioc_read_toc_entry rte;
	struct cd_toc_entry entry;
	struct ioc_play_track ti;

	if (cdfile == -1)
		return -1;

	// don't try to play a non-audio track
	rte.address_format = CD_MSF_FORMAT;
	rte.starting_track = track;
	rte.data_len = sizeof(entry);
	rte.data = &entry;
	if (ioctl(cdfile, CDIOREADTOCENTRYS, &rte) == -1)
	{
		Con_DPrint("ioctl CDIOREADTOCENTRYS failed\n");
		return -1;
	}
	if (entry.control & 4)  // if it's a data track
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return -1;
	}

	if (cdPlaying)
		CDAudio_Stop();

	ti.start_track = track;
	ti.end_track = track;
	ti.start_index = 1;
	ti.end_index = 99;

	if (ioctl(cdfile, CDIOCPLAYTRACKS, &ti) == -1)
	{
		Con_DPrint("ioctl CDIOCPLAYTRACKS failed\n");
		return -1;
	}

	if (ioctl(cdfile, CDIOCRESUME) == -1)
	{
		Con_DPrint("ioctl CDIOCRESUME failed\n");
		return -1;
	}

	return 0;
}


int CDAudio_SysStop (void)
{
	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDIOCSTOP) == -1)
	{
		Con_DPrintf("ioctl CDIOCSTOP failed (%d)\n", errno);
		return -1;
	}
	ioctl(cdfile, CDIOCALLOW);

	return 0;
}

int CDAudio_SysPause (void)
{
	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDIOCPAUSE) == -1)
	{
		Con_DPrint("ioctl CDIOCPAUSE failed\n");
		return -1;
	}

	return 0;
}


int CDAudio_SysResume (void)
{
	if (cdfile == -1)
		return -1;

	if (ioctl(cdfile, CDIOCRESUME) == -1)
		Con_DPrint("ioctl CDIOCRESUME failed\n");

	return 0;
}

int CDAudio_SysUpdate (void)
{
	static time_t lastchk = 0;
	struct ioc_read_subchannel subchnl;
	struct cd_sub_channel_info data;

	if (cdPlaying && lastchk < time(NULL))
	{
		lastchk = time(NULL) + 2; //two seconds between chks

		bzero(&subchnl, sizeof(subchnl));
		subchnl.data = &data;
		subchnl.data_len = sizeof(data);
		subchnl.address_format = CD_MSF_FORMAT;
		subchnl.data_format = CD_CURRENT_POSITION;

		if (ioctl(cdfile, CDIOCREADSUBCHANNEL, &subchnl) == -1)
		{
			Con_DPrint("ioctl CDIOCREADSUBCHANNEL failed\n");
			cdPlaying = false;
			return -1;
		}
		if (data.header.audio_status != CD_AS_PLAY_IN_PROGRESS &&
			data.header.audio_status != CD_AS_PLAY_PAUSED)
		{
			cdPlaying = false;
			if (cdPlayLooping)
				CDAudio_Play(cdPlayTrack, true);
		}
		else
			cdPlayTrack = data.what.position.track_number;
	}

	return 0;
}

void CDAudio_SysInit (void)
{
	int i;

	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1)
		strlcpy(cd_dev, com_argv[i + 1], sizeof(cd_dev));
}

int CDAudio_SysStartup (void)
{
	char buff [80];

	if ((cdfile = opendisk(cd_dev, O_RDONLY, buff, sizeof(buff), 0)) == -1)
	{
		Con_DPrintf("CDAudio_SysStartup: open of \"%s\" failed (%i)\n",
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
