/*
Copyright (C) 2002 Mathieu Olivier

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
// net_master.c

#include "quakedef.h"


cvar_t sv_public = {0, "sv_public", "0"};
cvar_t sv_heartbeatperiod = {CVAR_SAVE, "sv_heartbeatperiod", "180"};

cvar_t sv_masters [] =
{
	{0, "sv_masterextra1", "68.102.242.12"},
	{CVAR_SAVE, "sv_master1", ""},
	{CVAR_SAVE, "sv_master2", ""},
	{CVAR_SAVE, "sv_master3", ""},
	{CVAR_SAVE, "sv_master4", ""}
};

static double nextheartbeattime = 0;


/*
====================
Master_AllowHeartbeat

Allow (or not) NET_Heartbeat to proceed depending on various factors
====================
*/
qboolean Master_AllowHeartbeat (int priority)
{
	// LordHavoc: make advertising optional
	if (!sv_public.integer)
		return false;
	// LordHavoc: don't advertise singleplayer games
	if (svs.maxclients < 2)
		return false;
	// if it's a state change (client connected), limit next heartbeat to no
	// more than 30 sec in the future
	if (priority == 1 && nextheartbeattime > realtime + 30.0)
		nextheartbeattime = realtime + 30.0;
	if (priority <= 1 && realtime < nextheartbeattime)
		return false;
	// limit heartbeatperiod to 30 to 270 second range,
	// lower limit is to avoid abusing master servers with excess traffic,
	// upper limit is to avoid timing out on the master server (which uses
	// 300 sec timeout)
	if (sv_heartbeatperiod.value < 30)
		Cvar_SetValueQuick(&sv_heartbeatperiod, 30);
	if (sv_heartbeatperiod.value > 270)
		Cvar_SetValueQuick(&sv_heartbeatperiod, 270);
	// send a heartbeat as often as the admin wants
	nextheartbeattime = realtime + sv_heartbeatperiod.value;
	return true;
}


/*
====================
Master_BuildGetServers

Build a getservers request for a master server
====================
*/
const char* Master_BuildGetServers (void)
{
	static int nextmaster = 0;
	cvar_t* sv_master;
	char request [256];

	if (nextmaster >= (int)(sizeof (sv_masters) / sizeof (sv_masters[0])))
	{
		nextmaster = 0;
		return NULL;
	}

	sv_master = &sv_masters[nextmaster++];

	// No master, no heartbeat
	if (sv_master->string[0] == '\0')
	{
		nextmaster = 0;
		return NULL;
	}

	// Build the heartbeat
	snprintf (request, sizeof (request), "getservers %s %u empty full\x0A", gamename, NET_PROTOCOL_VERSION);
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);
	MSG_WriteString (&net_message, request);

	net_message.cursize--;  // we don't send the trailing '\0'

	return sv_master->string;
}


/*
====================
Master_BuildHeartbeat

Build an heartbeat for a master server
====================
*/
const char* Master_BuildHeartbeat (void)
{
	static int nextmaster = 0;
	cvar_t* sv_master;

	if (nextmaster >= (int)(sizeof (sv_masters) / sizeof (sv_masters[0])))
	{
		nextmaster = 0;
		return NULL;
	}

	sv_master = &sv_masters[nextmaster++];

	// No master, no heartbeat
	if (sv_master->string[0] == '\0')
	{
		nextmaster = 0;
		return NULL;
	}

	// Build the heartbeat
	SZ_Clear (&net_message);
	MSG_WriteLong (&net_message, -1);
	MSG_WriteString (&net_message, "heartbeat DarkPlaces\x0A");

	net_message.cursize--;  // we don't send the trailing '\0'

	return sv_master->string;
}


/*
====================
Master_HandleMessage

Handle the master server messages
====================
*/
int Master_HandleMessage (void)
{
	const char* string = MSG_ReadString ();

	// If it's a "getinfo" request
	if (!strncmp (string, "getinfo", 7))
	{
		char response [512];
		size_t length;

		length = snprintf (response, sizeof (response), "infoResponse\x0A"
					"\\gamename\\%s\\modname\\%s\\sv_maxclients\\%d"
					"\\clients\\%d\\mapname\\%s\\hostname\\%s\\protocol\\%d",
					gamename, com_modname, svs.maxclients, net_activeconnections,
					sv.name, hostname.string, NET_PROTOCOL_VERSION);

		// Too long to fit into the buffer?
		if (length >= sizeof (response))
			return -1;

		// If there was a challenge in the getinfo message
		if (string[7] == ' ')
		{
			string += 8;  // skip the header and the space

			// If the challenge wouldn't fit into the buffer
			if (length + 11 + strlen (string) >= sizeof (response))
				return -1;

			sprintf (response + length, "\\challenge\\%s", string);
		}

		SZ_Clear (&net_message);
		MSG_WriteLong (&net_message, -1);
		MSG_WriteString (&net_message, response);

		return net_message.cursize - 1;
	}

	return 0;
}


/*
====================
Master_Init

Initialize the code that handles master server requests and reponses
====================
*/
void Master_Init (void)
{
	unsigned int ind;
	Cvar_RegisterVariable (&sv_public);
	Cvar_RegisterVariable (&sv_heartbeatperiod);
	for (ind = 0; ind < sizeof (sv_masters) / sizeof (sv_masters[0]); ind++)
		Cvar_RegisterVariable (&sv_masters[ind]);
}


/*
====================
Master_ParseServerList

Parse getserverResponse messages
====================
*/
void Master_ParseServerList (net_landriver_t* dfunc)
{
	int control;
	qbyte* servers;
	qbyte* crtserver;
	int ipaddr;
	struct qsockaddr svaddr;
	char ipstring [32];

	if (net_message.cursize < (int)sizeof(int))
		return;

	// is the cache full?
	if (hostCacheCount == HOSTCACHESIZE)
		return;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control != -1)
		return;

	if (strncmp (net_message.data + 4, "getserversResponse\\", 19))	
		return;

	// Skip the next 19 bytes
	MSG_ReadLong(); MSG_ReadLong(); MSG_ReadLong(); MSG_ReadLong();
	MSG_ReadShort(); MSG_ReadByte();

	crtserver = servers = Z_Malloc (net_message.cursize - 23);
	memcpy (servers , net_message.data + 23, net_message.cursize - 23);

	// Extract the IP addresses
	while ((ipaddr = (crtserver[3] << 24) | (crtserver[2] << 16) | (crtserver[1] << 8) | crtserver[0]) != -1)
	{
		int port = (crtserver[5] << 8) | crtserver[4];

		if (port == -1 || port == 0)
			break;

		port = ((port >> 8) & 0xFF) + ((port & 0xFF) << 8);
		sprintf (ipstring, "%u.%u.%u.%u:%hu",
					ipaddr & 0xFF, (ipaddr >> 8) & 0xFF,
					(ipaddr >> 16) & 0xFF, ipaddr >> 24,
					port);
		dfunc->GetAddrFromName (ipstring, &svaddr);

		Con_DPrintf("Requesting info from server %s\n", ipstring);
		// Send a request at this address
		SZ_Clear(&net_message);
		MSG_WriteLong(&net_message, 0);  // save space for the header, filled in later
		MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		dfunc->Write(dfunc->controlSock, net_message.data, net_message.cursize, &svaddr);
		SZ_Clear(&net_message);

		if (crtserver[6] != '\\')
			break;
		crtserver += 7;
	}

	Z_Free (servers);
}
