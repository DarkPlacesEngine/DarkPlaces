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
#include "net_udp.h"
#ifdef WIN32
#include "winquake.h"
#define MAXHOSTNAMELEN		256
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifdef __sun__
#include <sys/filio.h>
#endif

#ifdef NeXT
#include <libc.h>
#endif
#endif

static int net_acceptsocket = -1;		// socket for fielding new connections
static int net_controlsocket;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;

static union {int i;unsigned char d[4];} myAddr;

//=============================================================================

#ifdef WIN32
WSADATA		winsockdata;
#endif

int UDP_Init (void)
{
	int i;
	struct hostent *local = NULL;
	char buff[MAXHOSTNAMELEN];

	if (COM_CheckParm ("-noudp"))
		return -1;

#ifdef WIN32
	if (WSAStartup (MAKEWORD(1, 1), &winsockdata))
	{
		Con_SafePrintf ("Winsock initialization failed.\n");
		return -1;
	}
#endif

	// loopback as a worst case fallback
	myAddr.d[0] = 127;myAddr.d[1] = 0;myAddr.d[2] = 0;myAddr.d[3] = 1;

	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc)
	{
		myAddr.i = inet_addr(com_argv[i+1]);
		Con_Printf("Binding to IP Interface Address of %i.%i.%i.%i\n", myAddr.d[0], myAddr.d[1], myAddr.d[2], myAddr.d[3]);
	}
	else if (gethostname(buff, MAXHOSTNAMELEN) != -1)
	{
		buff[MAXHOSTNAMELEN - 1] = 0;
		local = gethostbyname(buff);
		if (local != NULL)
			myAddr.i = *((int *)local->h_addr_list[0]);
	}

	sprintf(my_tcpip_address, "%d.%d.%d.%d", myAddr.d[0], myAddr.d[1], myAddr.d[2], myAddr.d[3]);

	if ((net_controlsocket = UDP_OpenSocket (0)) == -1)
	{
		Con_Printf("UDP_Init: Unable to open control socket\n");
#ifdef WIN32
		WSACleanup ();
#endif
		return -1;
	}

	((struct sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
	((struct sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = htonl(INADDR_BROADCAST);
	((struct sockaddr_in *)&broadcastaddr)->sin_port = htons((unsigned short)net_hostport);

	Con_Printf("UDP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void UDP_Shutdown (void)
{
	UDP_Listen (false);
	UDP_CloseSocket (net_controlsocket);
#ifdef WIN32
	WSACleanup ();
#endif
}

//=============================================================================

void UDP_Listen (qboolean state)
{
	// enable listening
	if (state)
	{
		if (net_acceptsocket != -1)
			return;
		if ((net_acceptsocket = UDP_OpenSocket (net_hostport)) == -1)
			Sys_Error ("UDP_Listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (net_acceptsocket == -1)
		return;
	UDP_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

int UDP_OpenSocket (int port)
{
	int newsocket;
	struct sockaddr_in address;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	{
#ifdef WIN32
		u_long _true = 1;
		if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		{
			closesocket (newsocket);
#else
		char _true = 1;
		if (ioctl (newsocket, FIONBIO, &_true) == -1)
		{
			close (newsocket);
#endif
			Sys_Error("UDP_OpenSocket: unable to do a ioctl FIONBIO on the socket\n");
		}
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = myAddr.i;
	address.sin_port = htons((unsigned short)port);
	if (bind(newsocket, (void *)&address, sizeof(address)) == -1)
	{
#ifdef WIN32
		closesocket(newsocket);
#else
		close(newsocket);
#endif
		Sys_Error ("UDP_OpenSocket: Unable to bind to %s", UDP_AddrToString((struct qsockaddr *)&address));
	}

	return newsocket;
}

//=============================================================================

int UDP_CloseSocket (int socket)
{
	if (net_broadcastsocket == socket)
		net_broadcastsocket = 0;
#ifdef WIN32
	return closesocket (socket);
#else
	return close (socket);
#endif
}

//=============================================================================

int UDP_Connect (int socket, struct qsockaddr *addr)
{
	return 0;
}

//=============================================================================

int UDP_CheckNewConnections (void)
{
	char buf[4096];
#ifndef WIN32
	unsigned long	available;
	struct sockaddr_in	from;
	socklen_t			fromlen;
#endif

	if (net_acceptsocket == -1)
		return -1;

#ifdef WIN32
	if (recvfrom (net_acceptsocket, buf, sizeof(buf), MSG_PEEK, NULL, NULL) >= 0)
		return net_acceptsocket;
#else
	if (ioctl (net_acceptsocket, FIONREAD, &available) == -1)
		Sys_Error ("UDP: ioctlsocket (FIONREAD) failed\n");
	if (available)
		return net_acceptsocket;
	recvfrom (net_acceptsocket, buf, 0, 0, (struct sockaddr *) &from, &fromlen);
#endif
	return -1;
}

//=============================================================================

int UDP_Recv (qbyte *buf, int len, struct qsockaddr *addr)
{
	return UDP_Read (net_acceptsocket, buf, len, addr);
}

//=============================================================================

int UDP_Send (qbyte *buf, int len, struct qsockaddr *addr)
{
	return UDP_Write (net_acceptsocket, buf, len, addr);
}

//=============================================================================

int UDP_Read (int socket, qbyte *buf, int len, struct qsockaddr *addr)
{
	int addrlen = sizeof (struct qsockaddr);
	int ret;

	ret = recvfrom (socket, buf, len, 0, (struct sockaddr *)addr, &addrlen);
	if (ret == -1)
	{
#ifdef WIN32
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK || e == WSAECONNREFUSED)
			return 0;
		Con_Printf("UDP_Read: WASGetLastError == %i\n", e);
#else
		if (errno == EWOULDBLOCK || errno == ECONNREFUSED)
			return 0;
		Con_Printf("UDP_Read: errno == %i (%s)\n", errno, strerror(errno));
#endif
	}
	return ret;
}

//=============================================================================

int UDP_MakeSocketBroadcastCapable (int socket)
{
	int i = 1;

	// make this socket broadcast capable
	if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int UDP_Broadcast (int socket, qbyte *buf, int len)
{
	int ret;

	if (socket != net_broadcastsocket)
	{
		if (net_broadcastsocket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets\n");
		ret = UDP_MakeSocketBroadcastCapable (socket);
		if (ret == -1)
		{
			Con_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return UDP_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int UDP_Write (int socket, qbyte *buf, int len, struct qsockaddr *addr)
{
	int ret;

	ret = sendto (socket, buf, len, 0, (struct sockaddr *)addr, sizeof(struct qsockaddr));
	if (ret == -1)
	{
#ifdef WIN32
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK)
			return 0;
		Con_Printf("UDP_Write: WASGetLastError == %i\n", e);
#else
		if (errno == EWOULDBLOCK)
			return 0;
		Con_Printf("UDP_Write: errno == %i (%s)\n", errno, strerror(errno));
#endif
	}
	return ret;
}

//=============================================================================

char *UDP_AddrToString (const struct qsockaddr *addr)
{
	static char buffer[22]; // only 22 needed (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 5 + null)
	unsigned char *ip = (char *)(&((struct sockaddr_in *)addr)->sin_addr.s_addr);
	sprintf(buffer, "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], ntohs(((struct sockaddr_in *)addr)->sin_port));
	return buffer;
}

//=============================================================================

int UDP_StringToAddr (const char *string, struct qsockaddr *addr)
{
	int ha1, ha2, ha3, ha4, hp;
	int ipaddr;

	sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
	ipaddr = (ha1 << 24) | (ha2 << 16) | (ha3 << 8) | ha4;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)hp);
	return 0;
}

//=============================================================================

int UDP_GetSocketAddr (int socket, struct qsockaddr *addr)
{
	int ret;
	int addrlen = sizeof(struct qsockaddr);
	memset(addr, 0, sizeof(struct qsockaddr));
	ret = getsockname(socket, (struct sockaddr *)addr, &addrlen);
	if (ret == -1)
	{
#ifdef WIN32
		int e = WSAGetLastError();
		Con_Printf("UDP_GetSocketAddr: WASGetLastError == %i\n", e);
#else
		Con_Printf("UDP_GetSocketAddr: errno == %i (%s)\n", errno, strerror(errno));
#endif
	}
	return ret;
}

//=============================================================================

int UDP_GetNameFromAddr (const struct qsockaddr *addr, char *name)
{
	struct hostent *hostentry;

	hostentry = gethostbyaddr ((char *)&((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET);
	if (hostentry)
	{
		strncpy (name, (char *)hostentry->h_name, NET_NAMELEN - 1);
		return 0;
	}

	strcpy (name, UDP_AddrToString (addr));
	return 0;
}

//=============================================================================

int UDP_GetAddrFromName(const char *name, struct qsockaddr *addr)
{
	struct hostent *hostentry;

	if (name[0] >= '0' && name[0] <= '9')
		return UDP_StringToAddr (name, addr);

	hostentry = gethostbyname (name);
	if (!hostentry)
		return -1;

	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)net_hostport);
	((struct sockaddr_in *)addr)->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];

	return 0;
}

//=============================================================================

int UDP_AddrCompare (const struct qsockaddr *addr1, const struct qsockaddr *addr2)
{
	if (addr1->sa_family != addr2->sa_family)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_addr.s_addr != ((struct sockaddr_in *)addr2)->sin_addr.s_addr)
		return -1;

	if (((struct sockaddr_in *)addr1)->sin_port != ((struct sockaddr_in *)addr2)->sin_port)
		return 1;

	return 0;
}

//=============================================================================

int UDP_GetSocketPort (struct qsockaddr *addr)
{
	return ntohs(((struct sockaddr_in *)addr)->sin_port);
}


int UDP_SetSocketPort (struct qsockaddr *addr, int port)
{
	((struct sockaddr_in *)addr)->sin_port = htons((unsigned short)port);
	return 0;
}

