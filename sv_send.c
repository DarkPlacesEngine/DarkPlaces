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

/*
=================
SV_ClientPrint

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrint(const char *msg)
{
	if (host_client->netconnection)
	{
		MSG_WriteByte(&host_client->netconnection->message, svc_print);
		MSG_WriteString(&host_client->netconnection->message, msg);
	}
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	SV_ClientPrint(msg);
}

/*
=================
SV_BroadcastPrint

Sends text to all active clients
=================
*/
void SV_BroadcastPrint(const char *msg)
{
	int i;
	client_t *client;

	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (client->active && client->netconnection)
		{
			MSG_WriteByte(&client->netconnection->message, svc_print);
			MSG_WriteString(&client->netconnection->message, msg);
		}
	}

	if (sv_echobprint.integer && cls.state == ca_dedicated)
		Con_Print(msg);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAX_INPUTLINE];

	va_start(argptr,fmt);
	dpvsnprintf(msg,sizeof(msg),fmt,argptr);
	va_end(argptr);

	SV_BroadcastPrint(msg);
}

/*
=================
SV_ClientCommands

Send text over to the client to be executed
=================
*/
void SV_ClientCommands(const char *fmt, ...)
{
	va_list argptr;
	char string[MAX_INPUTLINE];

	if (!host_client->netconnection)
		return;

	va_start(argptr,fmt);
	dpvsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->netconnection->message, svc_stufftext);
	MSG_WriteString(&host_client->netconnection->message, string);
}