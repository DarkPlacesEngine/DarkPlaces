
// Written by Forest Hale 2003-06-15 and placed into public domain.

#ifndef STANDALONETEST
#include "quakedef.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#ifdef WIN32
#include <winsock.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef __MORPHOS__
#include <proto/socket.h>
#endif

// for Z_Malloc/Z_Free in quake
#ifndef STANDALONETEST
#include "zone.h"
#include "sys.h"
#include "netconn.h"
#else
#define Con_Print printf
#define Con_Printf printf
#define Z_Malloc malloc
#define Z_Free free
#endif

#include "lhnet.h"

#if defined(WIN32)
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNREFUSED WSAECONNREFUSED

#define SOCKETERRNO WSAGetLastError()

#define IOC_VENDOR 0x18000000
#define _WSAIOW(x,y) (IOC_IN|(x)|(y))
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)

#define SOCKLEN_T int
#elif defined(__MORPHOS__)
#define ioctlsocket IoctlSocket
#define closesocket CloseSocket
#define SOCKETERRNO Errno()

#define SOCKLEN_T int
#else
#define ioctlsocket ioctl
#define closesocket close
#define SOCKETERRNO errno

#define SOCKLEN_T socklen_t
#endif

// to make LHNETADDRESS_FromString resolve repeated hostnames faster, cache them
#define MAX_NAMECACHE 64
static struct namecache_s
{
	lhnetaddress_t address;
	double expirationtime;
	char name[64];
}
namecache[MAX_NAMECACHE];
static int namecacheposition = 0;

int LHNETADDRESS_FromPort(lhnetaddress_t *address, int addresstype, int port)
{
	if (!address)
		return 0;
	switch(addresstype)
	{
	case LHNETADDRESSTYPE_LOOP:
		// local:port  (loopback)
		memset(address, 0, sizeof(*address));
		address->addresstype = LHNETADDRESSTYPE_LOOP;
		address->addressdata.loop.port = port;
		return 1;
	case LHNETADDRESSTYPE_INET4:
		// 0.0.0.0:port  (INADDR_ANY, binds to all interfaces)
		memset(address, 0, sizeof(*address));
		address->addresstype = LHNETADDRESSTYPE_INET4;
		address->addressdata.inet4.family = LHNETADDRESSTYPE_INET4_FAMILY;
		address->addressdata.inet4.port = htons((unsigned short)port);
		return 1;
	case LHNETADDRESSTYPE_INET6:
		// [0:0:0:0:0:0:0:0]:port  (IN6ADDR_ANY, binds to all interfaces)
		memset(address, 0, sizeof(*address));
		address->addresstype = LHNETADDRESSTYPE_INET6;
		address->addressdata.inet6.family = LHNETADDRESSTYPE_INET6_FAMILY;
		address->addressdata.inet6.port = htons((unsigned short)port);
		return 1;
	}
	return 0;
}

int LHNETADDRESS_FromString(lhnetaddress_t *address, const char *string, int defaultport)
{
	int i, port, namelen, d1, d2, d3, d4;
	struct hostent *hostentry;
	const char *colon;
	char name[128];
	if (!address || !string || !*string)
		return 0;
	memset(address, 0, sizeof(*address));
	address->addresstype = LHNETADDRESSTYPE_NONE;
	port = 0;
	colon = strrchr(string, ':');
	if (colon)
		port = atoi(colon + 1);
	else
		colon = string + strlen(string);
	if (port == 0)
		port = defaultport;
	namelen = colon - string;
	if (namelen > 127)
		namelen = 127;
	if (string[0] == '[' && namelen > 0 && string[namelen-1] == ']') // ipv6
	{
		string++;
		namelen -= 2;
	}
	memcpy(name, string, namelen);
	name[namelen] = 0;
	// handle loopback
	if (!strcmp(name, "local"))
	{
		address->addresstype = LHNETADDRESSTYPE_LOOP;
		address->addressdata.loop.port = port;
		return 1;
	}
	// try to parse as dotted decimal ipv4 address first
	// note this supports partial ip addresses
	d1 = d2 = d3 = d4 = 0;
	if (sscanf(name, "%d.%d.%d.%d", &d1, &d2, &d3, &d4) >= 1 && (unsigned int)d1 < 256 && (unsigned int)d2 < 256 && (unsigned int)d3 < 256 && (unsigned int)d4 < 256)
	{
		// parsed a valid ipv4 address
		address->addresstype = LHNETADDRESSTYPE_INET4;
		address->addressdata.inet4.family = LHNETADDRESSTYPE_INET4_FAMILY;
		address->addressdata.inet4.port = htons((unsigned short)port);
		address->addressdata.inet4.address[0] = (unsigned char)d1;
		address->addressdata.inet4.address[1] = (unsigned char)d2;
		address->addressdata.inet4.address[2] = (unsigned char)d3;
		address->addressdata.inet4.address[3] = (unsigned char)d4;
#ifdef STANDALONETEST
		printf("manual parsing of ipv4 dotted decimal address \"%s\" successful: %d.%d.%d.%d:%d\n", string, (int)address->addressdata.inet4.address[0], (int)address->addressdata.inet4.address[1], (int)address->addressdata.inet4.address[2], (int)address->addressdata.inet4.address[3], (int)ntohs(address->addressdata.inet4.port));
#endif
		return 1;
	}
	for (i = 0;i < MAX_NAMECACHE;i++)
		if (!strcmp(namecache[i].name, name))
			break;
#ifdef STANDALONETEST
	if (i < MAX_NAMECACHE)
#else
	if (i < MAX_NAMECACHE && realtime < namecache[i].expirationtime)
#endif
	{
		*address = namecache[i].address;
		if (address->addresstype == LHNETADDRESSTYPE_INET6)
		{
			address->addressdata.inet6.port = htons((unsigned short)port);
			return 1;
		}
		else if (address->addresstype == LHNETADDRESSTYPE_INET4)
		{
			address->addressdata.inet4.port = htons((unsigned short)port);
			return 1;
		}
		return 0;
	}
	// try gethostbyname (handles dns and other ip formats)
	hostentry = gethostbyname(name);
	if (hostentry)
	{
		if (hostentry->h_addrtype == LHNETADDRESSTYPE_INET6_FAMILY)
		{
			// great it worked
			address->addresstype = LHNETADDRESSTYPE_INET6;
			address->addressdata.inet6.family = hostentry->h_addrtype;
			address->addressdata.inet6.port = htons((unsigned short)port);
			memcpy(address->addressdata.inet6.address, hostentry->h_addr_list[0], sizeof(address->addressdata.inet6.address));
			for (i = 0;i < (int)sizeof(namecache[namecacheposition].name)-1 && name[i];i++)
				namecache[namecacheposition].name[i] = name[i];
			namecache[namecacheposition].name[i] = 0;
#ifndef STANDALONETEST
			namecache[namecacheposition].expirationtime = realtime + 12 * 3600; // 12 hours
#endif
			namecache[namecacheposition].address = *address;
			namecacheposition = (namecacheposition + 1) % MAX_NAMECACHE;
#ifdef STANDALONETEST
			printf("gethostbyname(\"%s\") returned ipv6 address [%x:%x:%x:%x:%x:%x:%x:%x]:%d\n", name, (int)address->addressdata.inet6.address[0], (int)address->addressdata.inet6.address[1], (int)address->addressdata.inet6.address[2], (int)address->addressdata.inet6.address[3], (int)address->addressdata.inet6.address[4], (int)address->addressdata.inet6.address[5], (int)address->addressdata.inet6.address[6], (int)address->addressdata.inet6.address[7], (int)ntohs(address->addressdata.inet6.port));
#endif
			return 1;
		}
		else if (hostentry->h_addrtype == LHNETADDRESSTYPE_INET4_FAMILY)
		{
			// great it worked
			address->addresstype = LHNETADDRESSTYPE_INET4;
			address->addressdata.inet4.family = hostentry->h_addrtype;
			address->addressdata.inet4.port = htons((unsigned short)port);
			memcpy(address->addressdata.inet4.address, hostentry->h_addr_list[0], sizeof(address->addressdata.inet4.address));
			for (i = 0;i < (int)sizeof(namecache[namecacheposition].name)-1 && name[i];i++)
				namecache[namecacheposition].name[i] = name[i];
			namecache[namecacheposition].name[i] = 0;
#ifndef STANDALONETEST
			namecache[namecacheposition].expirationtime = realtime + 12 * 3600; // 12 hours
#endif
			namecache[namecacheposition].address = *address;
			namecacheposition = (namecacheposition + 1) % MAX_NAMECACHE;
#ifdef STANDALONETEST
			printf("gethostbyname(\"%s\") returned ipv4 address %d.%d.%d.%d:%d\n", name, (int)address->addressdata.inet4.address[0], (int)address->addressdata.inet4.address[1], (int)address->addressdata.inet4.address[2], (int)address->addressdata.inet4.address[3], (int)ntohs(address->addressdata.inet4.port));
#endif
			return 1;
		}
	}
#ifdef STANDALONETEST
	printf("gethostbyname failed on address \"%s\"\n", name);
#endif
	for (i = 0;i < (int)sizeof(namecache[namecacheposition].name)-1 && name[i];i++)
		namecache[namecacheposition].name[i] = name[i];
	namecache[namecacheposition].name[i] = 0;
#ifndef STANDALONETEST
	namecache[namecacheposition].expirationtime = realtime + 12 * 3600; // 12 hours
#endif
	namecache[namecacheposition].address.addresstype = LHNETADDRESSTYPE_NONE;
	namecacheposition = (namecacheposition + 1) % MAX_NAMECACHE;
	return 0;
}

int LHNETADDRESS_ToString(const lhnetaddress_t *address, char *string, int stringbuffersize, int includeport)
{
	*string = 0;
	if (!address || !string || stringbuffersize < 1)
		return 0;
	switch(address->addresstype)
	{
	default:
		break;
	case LHNETADDRESSTYPE_LOOP:
		if (includeport)
		{
			if (stringbuffersize >= 12)
			{
				sprintf(string, "local:%d", (int)address->addressdata.loop.port);
				return 1;
			}
		}
		else
		{
			if (stringbuffersize >= 6)
			{
				memcpy(string, "local", 6);
				return 1;
			}
		}
		break;
	case LHNETADDRESSTYPE_INET4:
		if (includeport)
		{
			if (stringbuffersize >= 22)
			{
				sprintf(string, "%d.%d.%d.%d:%d", (int)address->addressdata.inet4.address[0], (int)address->addressdata.inet4.address[1], (int)address->addressdata.inet4.address[2], (int)address->addressdata.inet4.address[3], (int)ntohs(address->addressdata.inet4.port));
				return 1;
			}
		}
		else
		{
			if (stringbuffersize >= 16)
			{
				sprintf(string, "%d.%d.%d.%d", (int)address->addressdata.inet4.address[0], (int)address->addressdata.inet4.address[1], (int)address->addressdata.inet4.address[2], (int)address->addressdata.inet4.address[3]);
				return 1;
			}
		}
		break;
	case LHNETADDRESSTYPE_INET6:
		if (includeport)
		{
			if (stringbuffersize >= 88)
			{
				sprintf(string, "[%x:%x:%x:%x:%x:%x:%x:%x]:%d", (int)address->addressdata.inet6.address[0], (int)address->addressdata.inet6.address[1], (int)address->addressdata.inet6.address[2], (int)address->addressdata.inet6.address[3], (int)address->addressdata.inet6.address[4], (int)address->addressdata.inet6.address[5], (int)address->addressdata.inet6.address[6], (int)address->addressdata.inet6.address[7], (int)ntohs(address->addressdata.inet6.port));
				return 1;
			}
		}
		else
		{
			if (stringbuffersize >= 80)
			{
				sprintf(string, "%x:%x:%x:%x:%x:%x:%x:%x", (int)address->addressdata.inet6.address[0], (int)address->addressdata.inet6.address[1], (int)address->addressdata.inet6.address[2], (int)address->addressdata.inet6.address[3], (int)address->addressdata.inet6.address[4], (int)address->addressdata.inet6.address[5], (int)address->addressdata.inet6.address[6], (int)address->addressdata.inet6.address[7]);
				return 1;
			}
		}
		break;
	}
	return 0;
}

int LHNETADDRESS_GetAddressType(const lhnetaddress_t *address)
{
	if (address)
		return address->addresstype;
	else
		return LHNETADDRESSTYPE_NONE;
}

int LHNETADDRESS_GetPort(const lhnetaddress_t *address)
{
	if (!address)
		return -1;
	switch(address->addresstype)
	{
	case LHNETADDRESSTYPE_LOOP:
		return address->addressdata.loop.port;
	case LHNETADDRESSTYPE_INET4:
		return ntohs(address->addressdata.inet4.port);
	case LHNETADDRESSTYPE_INET6:
		return ntohs(address->addressdata.inet6.port);
	default:
		return -1;
	}
}

int LHNETADDRESS_SetPort(lhnetaddress_t *address, int port)
{
	if (!address)
		return 0;
	switch(address->addresstype)
	{
	case LHNETADDRESSTYPE_LOOP:
		address->addressdata.loop.port = port;
		return 1;
	case LHNETADDRESSTYPE_INET4:
		address->addressdata.inet4.port = htons((unsigned short)port);
		return 1;
	case LHNETADDRESSTYPE_INET6:
		address->addressdata.inet6.port = htons((unsigned short)port);
		return 1;
	default:
		return 0;
	}
}

int LHNETADDRESS_Compare(const lhnetaddress_t *address1, const lhnetaddress_t *address2)
{
	if (!address1 || !address2)
		return 1;
	if (address1->addresstype != address2->addresstype)
		return 1;
	switch(address1->addresstype)
	{
	case LHNETADDRESSTYPE_LOOP:
		if (address1->addressdata.loop.port != address2->addressdata.loop.port)
			return -1;
		return 0;
	case LHNETADDRESSTYPE_INET4:
		if (address1->addressdata.inet4.family != address2->addressdata.inet4.family)
			return 1;
		if (memcmp(address1->addressdata.inet4.address, address2->addressdata.inet4.address, sizeof(address1->addressdata.inet4.address)))
			return 1;
		if (address1->addressdata.inet4.port != address2->addressdata.inet4.port)
			return -1;
		return 0;
	case LHNETADDRESSTYPE_INET6:
		if (address1->addressdata.inet6.family != address2->addressdata.inet6.family)
			return 1;
		if (memcmp(address1->addressdata.inet6.address, address2->addressdata.inet6.address, sizeof(address1->addressdata.inet6.address)))
			return 1;
		if (address1->addressdata.inet6.port != address2->addressdata.inet6.port)
			return -1;
		return 0;
	default:
		return 1;
	}
}

typedef struct lhnetpacket_s
{
	void *data;
	int length;
	int sourceport;
	int destinationport;
	time_t timeout;
#ifndef STANDALONETEST
	double sentdoubletime;
#endif
	struct lhnetpacket_s *next, *prev;
}
lhnetpacket_t;

static int lhnet_active;
static lhnetsocket_t lhnet_socketlist;
static lhnetpacket_t lhnet_packetlist;
#ifdef WIN32
static int lhnet_didWSAStartup = 0;
static WSADATA lhnet_winsockdata;
#endif

void LHNET_Init(void)
{
	if (lhnet_active)
		return;
	lhnet_socketlist.next = lhnet_socketlist.prev = &lhnet_socketlist;
	lhnet_packetlist.next = lhnet_packetlist.prev = &lhnet_packetlist;
	lhnet_active = 1;
#ifdef WIN32
	lhnet_didWSAStartup = !WSAStartup(MAKEWORD(1, 1), &lhnet_winsockdata);
	if (!lhnet_didWSAStartup)
		Con_Print("LHNET_Init: WSAStartup failed, networking disabled\n");
#endif
}

void LHNET_Shutdown(void)
{
	lhnetpacket_t *p;
	if (!lhnet_active)
		return;
	while (lhnet_socketlist.next != &lhnet_socketlist)
		LHNET_CloseSocket(lhnet_socketlist.next);
	while (lhnet_packetlist.next != &lhnet_packetlist)
	{
		p = lhnet_packetlist.next;
		p->prev->next = p->next;
		p->next->prev = p->prev;
		Z_Free(p);
	}
#ifdef WIN32
	if (lhnet_didWSAStartup)
	{
		lhnet_didWSAStartup = 0;
		WSACleanup();
	}
#endif
	lhnet_active = 0;
}

static const char *LHNETPRIVATE_StrError(void)
{
#ifdef WIN32
	int i = WSAGetLastError();
	switch (i)
	{
		case WSAEINTR:           return "WSAEINTR";
		case WSAEBADF:           return "WSAEBADF";
		case WSAEACCES:          return "WSAEACCES";
		case WSAEFAULT:          return "WSAEFAULT";
		case WSAEINVAL:          return "WSAEINVAL";
		case WSAEMFILE:          return "WSAEMFILE";
		case WSAEWOULDBLOCK:     return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS:     return "WSAEINPROGRESS";
		case WSAEALREADY:        return "WSAEALREADY";
		case WSAENOTSOCK:        return "WSAENOTSOCK";
		case WSAEDESTADDRREQ:    return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE:        return "WSAEMSGSIZE";
		case WSAEPROTOTYPE:      return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT:     return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP:      return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT:    return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT:    return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE:      return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL:   return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN:        return "WSAENETDOWN";
		case WSAENETUNREACH:     return "WSAENETUNREACH";
		case WSAENETRESET:       return "WSAENETRESET";
		case WSAECONNABORTED:    return "WSAECONNABORTED";
		case WSAECONNRESET:      return "WSAECONNRESET";
		case WSAENOBUFS:         return "WSAENOBUFS";
		case WSAEISCONN:         return "WSAEISCONN";
		case WSAENOTCONN:        return "WSAENOTCONN";
		case WSAESHUTDOWN:       return "WSAESHUTDOWN";
		case WSAETOOMANYREFS:    return "WSAETOOMANYREFS";
		case WSAETIMEDOUT:       return "WSAETIMEDOUT";
		case WSAECONNREFUSED:    return "WSAECONNREFUSED";
		case WSAELOOP:           return "WSAELOOP";
		case WSAENAMETOOLONG:    return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN:       return "WSAEHOSTDOWN";
		case WSAEHOSTUNREACH:    return "WSAEHOSTUNREACH";
		case WSAENOTEMPTY:       return "WSAENOTEMPTY";
		case WSAEPROCLIM:        return "WSAEPROCLIM";
		case WSAEUSERS:          return "WSAEUSERS";
		case WSAEDQUOT:          return "WSAEDQUOT";
		case WSAESTALE:          return "WSAESTALE";
		case WSAEREMOTE:         return "WSAEREMOTE";
		case WSAEDISCON:         return "WSAEDISCON";
		case 0:                  return "no error";
		default:                 return "unknown WSAE error";
	}
#else
	return strerror(errno);
#endif
}

void LHNET_SleepUntilPacket_Microseconds(int microseconds)
{
	fd_set fdreadset;
	struct timeval tv;
	int lastfd;
	lhnetsocket_t *s;
	FD_ZERO(&fdreadset);
	lastfd = 0;
	for (s = lhnet_socketlist.next;s != &lhnet_socketlist;s = s->next)
	{
		if (s->address.addresstype == LHNETADDRESSTYPE_INET4 || s->address.addresstype == LHNETADDRESSTYPE_INET6)
		{
			if (lastfd < s->inetsocket)
				lastfd = s->inetsocket;
			FD_SET((unsigned int)s->inetsocket, &fdreadset);
		}
	}
	tv.tv_sec = microseconds / 1000000;
	tv.tv_usec = microseconds % 1000000;
	select(lastfd + 1, &fdreadset, NULL, NULL, &tv);
}

lhnetsocket_t *LHNET_OpenSocket_Connectionless(lhnetaddress_t *address)
{
	lhnetsocket_t *lhnetsocket, *s;
	if (!address)
		return NULL;
	lhnetsocket = (lhnetsocket_t *)Z_Malloc(sizeof(*lhnetsocket));
	if (lhnetsocket)
	{
		memset(lhnetsocket, 0, sizeof(*lhnetsocket));
		lhnetsocket->address = *address;
		switch(lhnetsocket->address.addresstype)
		{
		case LHNETADDRESSTYPE_LOOP:
			if (lhnetsocket->address.addressdata.loop.port == 0)
			{
				// allocate a port dynamically
				// this search will always terminate because there is never
				// an allocated socket with port 0, so if the number wraps it
				// will find the port is unused, and then refuse to use port
				// 0, causing an intentional failure condition
				lhnetsocket->address.addressdata.loop.port = 1024;
				for (;;)
				{
					for (s = lhnet_socketlist.next;s != &lhnet_socketlist;s = s->next)
						if (s->address.addresstype == lhnetsocket->address.addresstype && s->address.addressdata.loop.port == lhnetsocket->address.addressdata.loop.port)
							break;
					if (s == &lhnet_socketlist)
						break;
					lhnetsocket->address.addressdata.loop.port++;
				}
			}
			// check if the port is available
			for (s = lhnet_socketlist.next;s != &lhnet_socketlist;s = s->next)
				if (s->address.addresstype == lhnetsocket->address.addresstype && s->address.addressdata.loop.port == lhnetsocket->address.addressdata.loop.port)
					break;
			if (s == &lhnet_socketlist && lhnetsocket->address.addressdata.loop.port != 0)
			{
				lhnetsocket->next = &lhnet_socketlist;
				lhnetsocket->prev = lhnetsocket->next->prev;
				lhnetsocket->next->prev = lhnetsocket;
				lhnetsocket->prev->next = lhnetsocket;
				return lhnetsocket;
			}
			break;
		case LHNETADDRESSTYPE_INET4:
		case LHNETADDRESSTYPE_INET6:
#ifdef WIN32
			if (lhnet_didWSAStartup)
			{
#endif
				if ((lhnetsocket->inetsocket = socket(address->addresstype == LHNETADDRESSTYPE_INET6 ? LHNETADDRESSTYPE_INET6_FAMILY : LHNETADDRESSTYPE_INET4_FAMILY, SOCK_DGRAM, IPPROTO_UDP)) != -1)
				{
#ifdef WIN32
					u_long _true = 1;
					u_long _false = 0;
#else
					char _true = 1;
#endif
					if (ioctlsocket(lhnetsocket->inetsocket, FIONBIO, &_true) != -1)
					{
						SOCKLEN_T namelen;
						namelen = address->addresstype == LHNETADDRESSTYPE_INET6 ? sizeof(lhnetsocket->address.addressdata.inet6) : sizeof(lhnetsocket->address.addressdata.inet4);
						if (bind(lhnetsocket->inetsocket, (struct sockaddr *)&lhnetsocket->address.addressdata, namelen) != -1)
						{
							int i = 1;
							getsockname(lhnetsocket->inetsocket, (struct sockaddr *)&lhnetsocket->address.addressdata, &namelen);
							// enable broadcast on this socket
							setsockopt(lhnetsocket->inetsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i));
							lhnetsocket->next = &lhnet_socketlist;
							lhnetsocket->prev = lhnetsocket->next->prev;
							lhnetsocket->next->prev = lhnetsocket;
							lhnetsocket->prev->next = lhnetsocket;
#ifdef WIN32
							if (ioctlsocket(lhnetsocket->inetsocket, SIO_UDP_CONNRESET, &_false) == -1)
								Con_DPrintf("LHNET_OpenSocket_Connectionless: ioctlsocket SIO_UDP_CONNRESET returned error: %s\n", LHNETPRIVATE_StrError());
#endif
							return lhnetsocket;
						}
						else
							Con_Printf("LHNET_OpenSocket_Connectionless: bind returned error: %s\n", LHNETPRIVATE_StrError());
					}
					else
						Con_Printf("LHNET_OpenSocket_Connectionless: ioctlsocket returned error: %s\n", LHNETPRIVATE_StrError());
					closesocket(lhnetsocket->inetsocket);
				}
				else
					Con_Printf("LHNET_OpenSocket_Connectionless: socket returned error: %s\n", LHNETPRIVATE_StrError());
#ifdef WIN32
			}
			else
				Con_Print("LHNET_OpenSocket_Connectionless: can't open a socket (WSAStartup failed during LHNET_Init)\n");
#endif
			break;
		default:
			break;
		}
		Z_Free(lhnetsocket);
	}
	return NULL;
}

void LHNET_CloseSocket(lhnetsocket_t *lhnetsocket)
{
	if (lhnetsocket)
	{
		// unlink from socket list
		if (lhnetsocket->next == NULL)
			return; // invalid!
		lhnetsocket->next->prev = lhnetsocket->prev;
		lhnetsocket->prev->next = lhnetsocket->next;
		lhnetsocket->next = NULL;
		lhnetsocket->prev = NULL;

		// no special close code for loopback, just inet
		if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_INET4 || lhnetsocket->address.addresstype == LHNETADDRESSTYPE_INET6)
		{
			closesocket(lhnetsocket->inetsocket);
		}
		Z_Free(lhnetsocket);
	}
}

lhnetaddress_t *LHNET_AddressFromSocket(lhnetsocket_t *sock)
{
	if (sock)
		return &sock->address;
	else
		return NULL;
}

int LHNET_Read(lhnetsocket_t *lhnetsocket, void *content, int maxcontentlength, lhnetaddress_t *address)
{
	int value = 0;
	if (!lhnetsocket || !address || !content || maxcontentlength < 1)
		return -1;
	if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_LOOP)
	{
		time_t currenttime;
		lhnetpacket_t *p, *pnext;
		// scan for any old packets to timeout while searching for a packet
		// that is waiting to be delivered to this socket
		currenttime = time(NULL);
		for (p = lhnet_packetlist.next;p != &lhnet_packetlist;p = pnext)
		{
			pnext = p->next;
			if (p->timeout < currenttime)
			{
				// unlink and free
				p->next->prev = p->prev;
				p->prev->next = p->next;
				Z_Free(p);
				continue;
			}
#ifndef STANDALONETEST
			if (cl_netlocalping.value && (realtime - cl_netlocalping.value * (1.0 / 2000.0)) < p->sentdoubletime)
				continue;
#endif
			if (value == 0 && p->destinationport == lhnetsocket->address.addressdata.loop.port)
			{
				if (p->length <= maxcontentlength)
				{
					*address = lhnetsocket->address;
					address->addressdata.loop.port = p->sourceport;
					memcpy(content, p->data, p->length);
					value = p->length;
				}
				else
					value = -1;
				// unlink and free
				p->next->prev = p->prev;
				p->prev->next = p->next;
				Z_Free(p);
			}
		}
	}
	else if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_INET4)
	{
		unsigned int inetaddresslength;
		address->addresstype = LHNETADDRESSTYPE_NONE;
		inetaddresslength = sizeof(address->addressdata.inet4);
		value = recvfrom(lhnetsocket->inetsocket, content, maxcontentlength, 0, (struct sockaddr *)&address->addressdata.inet4, &inetaddresslength);
		if (value > 0)
		{
			address->addresstype = LHNETADDRESSTYPE_INET4;
			return value;
		}
		else if (value == -1)
		{
			int e = SOCKETERRNO;
			if (e == EWOULDBLOCK)
				return 0;
			switch (e)
			{
				case ECONNREFUSED:
					Con_Print("Connection refused\n");
					return 0;
			}
			Con_Printf("LHNET_Read: recvfrom returned error: %s\n", LHNETPRIVATE_StrError());
		}
	}
	else if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_INET6)
	{
		unsigned int inetaddresslength;
		address->addresstype = LHNETADDRESSTYPE_NONE;
		inetaddresslength = sizeof(address->addressdata.inet6);
		value = recvfrom(lhnetsocket->inetsocket, content, maxcontentlength, 0, (struct sockaddr *)&address->addressdata.inet6, &inetaddresslength);
		if (value > 0)
		{
			address->addresstype = LHNETADDRESSTYPE_INET6;
			return value;
		}
		else if (value == -1)
		{
			int e = SOCKETERRNO;
			if (e == EWOULDBLOCK)
				return 0;
			switch (e)
			{
				case ECONNREFUSED:
					Con_Print("Connection refused\n");
					return 0;
			}
			Con_Printf("LHNET_Read: recvfrom returned error: %s\n", LHNETPRIVATE_StrError());
		}
	}
	return value;
}

int LHNET_Write(lhnetsocket_t *lhnetsocket, const void *content, int contentlength, const lhnetaddress_t *address)
{
	int value = -1;
	if (!lhnetsocket || !address || !content || contentlength < 1)
		return -1;
	if (lhnetsocket->address.addresstype != address->addresstype)
		return -1;
	if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_LOOP)
	{
		lhnetpacket_t *p;
		p = (lhnetpacket_t *)Z_Malloc(sizeof(*p) + contentlength);
		p->data = (void *)(p + 1);
		memcpy(p->data, content, contentlength);
		p->length = contentlength;
		p->sourceport = lhnetsocket->address.addressdata.loop.port;
		p->destinationport = address->addressdata.loop.port;
		p->timeout = time(NULL) + 10;
		p->next = &lhnet_packetlist;
		p->prev = p->next->prev;
		p->next->prev = p;
		p->prev->next = p;
#ifndef STANDALONETEST
		p->sentdoubletime = realtime;
#endif
		value = contentlength;
	}
	else if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_INET4)
	{
		value = sendto(lhnetsocket->inetsocket, content, contentlength, 0, (struct sockaddr *)&address->addressdata.inet4, sizeof(address->addressdata.inet4));
		if (value == -1)
		{
			if (SOCKETERRNO == EWOULDBLOCK)
				return 0;
			Con_Printf("LHNET_Write: sendto returned error: %s\n", LHNETPRIVATE_StrError());
		}
	}
	else if (lhnetsocket->address.addresstype == LHNETADDRESSTYPE_INET6)
	{
		value = sendto(lhnetsocket->inetsocket, content, contentlength, 0, (struct sockaddr *)&address->addressdata.inet6, sizeof(address->addressdata.inet6));
		if (value == -1)
		{
			if (SOCKETERRNO == EWOULDBLOCK)
				return 0;
			Con_Printf("LHNET_Write: sendto returned error: %s\n", LHNETPRIVATE_StrError());
		}
	}
	return value;
}

#ifdef STANDALONETEST
int main(int argc, char **argv)
{
#if 1
	char *buffer = "test", buffer2[1024];
	int blen = strlen(buffer);
	int b2len = 1024;
	lhnetsocket_t *sock1;
	lhnetsocket_t *sock2;
	lhnetaddress_t myaddy1;
	lhnetaddress_t myaddy2;
	lhnetaddress_t myaddy3;
	lhnetaddress_t localhostaddy1;
	lhnetaddress_t localhostaddy2;
	int test1;
	int test2;

	printf("calling LHNET_Init\n");
	LHNET_Init();

	printf("calling LHNET_FromPort twice to create two local addresses\n");
	LHNETADDRESS_FromPort(&myaddy1, LHNETADDRESSTYPE_INET4, 4000);
	LHNETADDRESS_FromPort(&myaddy2, LHNETADDRESSTYPE_INET4, 4001);
	LHNETADDRESS_FromString(&localhostaddy1, "127.0.0.1", 4000);
	LHNETADDRESS_FromString(&localhostaddy2, "127.0.0.1", 4001);

	printf("calling LHNET_OpenSocket_Connectionless twice to create two local sockets\n");
	sock1 = LHNET_OpenSocket_Connectionless(&myaddy1);
	sock2 = LHNET_OpenSocket_Connectionless(&myaddy2);

	printf("calling LHNET_Write to send a packet from the first socket to the second socket\n");
	test1 = LHNET_Write(sock1, buffer, blen, &localhostaddy2);
	printf("sleeping briefly\n");
#ifdef WIN32
	Sleep (100);
#else
	usleep (100000);
#endif
	printf("calling LHNET_Read on the second socket to read the packet sent from the first socket\n");
	test2 = LHNET_Read(sock2, buffer2, b2len - 1, &myaddy3);
	if (test2 > 0)
		Con_Printf("socket to socket test succeeded\n");
	else
		Con_Printf("socket to socket test failed\n");

#ifdef WIN32
	printf("press any key to exit\n");
	getchar();
#endif

	printf("calling LHNET_Shutdown\n");
	LHNET_Shutdown();
	printf("exiting\n");
	return 0;
#else
	lhnetsocket_t *sock[16], *sendsock;
	int i;
	int numsockets;
	int count;
	int length;
	int port;
	time_t oldtime;
	time_t newtime;
	char *sendmessage;
	int sendmessagelength;
	lhnetaddress_t destaddress;
	lhnetaddress_t receiveaddress;
	lhnetaddress_t sockaddress[16];
	char buffer[1536], addressstring[128], addressstring2[128];
	if ((argc == 2 || argc == 5) && (port = atoi(argv[1])) >= 1 && port < 65535)
	{
		printf("calling LHNET_Init()\n");
		LHNET_Init();

		numsockets = 0;
		LHNETADDRESS_FromPort(&sockaddress[numsockets++], LHNETADDRESSTYPE_LOOP, port);
		LHNETADDRESS_FromPort(&sockaddress[numsockets++], LHNETADDRESSTYPE_INET4, port);
		LHNETADDRESS_FromPort(&sockaddress[numsockets++], LHNETADDRESSTYPE_INET6, port+1);

		sendsock = NULL;
		sendmessage = NULL;
		sendmessagelength = 0;

		for (i = 0;i < numsockets;i++)
		{
			LHNETADDRESS_ToString(&sockaddress[i], addressstring, sizeof(addressstring), 1);
			printf("calling LHNET_OpenSocket_Connectionless(<%s>)\n", addressstring);
			if ((sock[i] = LHNET_OpenSocket_Connectionless(&sockaddress[i])))
			{
				LHNETADDRESS_ToString(LHNET_AddressFromSocket(sock[i]), addressstring2, sizeof(addressstring2), 1);
				printf("opened socket successfully (address \"%s\")\n", addressstring2);
			}
			else
			{
				printf("failed to open socket\n");
				if (i == 0)
				{
					LHNET_Shutdown();
					return -1;
				}
			}
		}
		count = 0;
		if (argc == 5)
		{
			count = atoi(argv[2]);
			if (LHNETADDRESS_FromString(&destaddress, argv[3], -1))
			{
				sendmessage = argv[4];
				sendmessagelength = strlen(sendmessage);
				sendsock = NULL;
				for (i = 0;i < numsockets;i++)
					if (sock[i] && LHNETADDRESS_GetAddressType(&destaddress) == LHNETADDRESS_GetAddressType(&sockaddress[i]))
						sendsock = sock[i];
				if (sendsock == NULL)
				{
					printf("Could not find an open socket matching the addresstype (%i) of destination address, switching to listen only mode\n", LHNETADDRESS_GetAddressType(&destaddress));
					argc = 2;
				}
			}
			else
			{
				printf("LHNETADDRESS_FromString did not like the address \"%s\", switching to listen only mode\n", argv[3]);
				argc = 2;
			}
		}
		printf("started, now listening for \"exit\" on the opened sockets\n");
		oldtime = time(NULL);
		for(;;)
		{
#ifdef WIN32
			Sleep(1);
#else
			usleep(1);
#endif
			for (i = 0;i < numsockets;i++)
			{
				if (sock[i])
				{
					length = LHNET_Read(sock[i], buffer, sizeof(buffer), &receiveaddress);
					if (length < 0)
						printf("localsock read error: length < 0");
					else if (length > 0 && length < (int)sizeof(buffer))
					{
						buffer[length] = 0;
						LHNETADDRESS_ToString(&receiveaddress, addressstring, sizeof(addressstring), 1);
						LHNETADDRESS_ToString(LHNET_AddressFromSocket(sock[i]), addressstring2, sizeof(addressstring2), 1);
						printf("received message \"%s\" from \"%s\" on socket \"%s\"\n", buffer, addressstring, addressstring2);
						if (!strcmp(buffer, "exit"))
							break;
					}
				}
			}
			if (i < numsockets)
				break;
			if (argc == 5 && count > 0)
			{
				newtime = time(NULL);
				if (newtime != oldtime)
				{
					LHNETADDRESS_ToString(&destaddress, addressstring, sizeof(addressstring), 1);
					LHNETADDRESS_ToString(LHNET_AddressFromSocket(sendsock), addressstring2, sizeof(addressstring2), 1);
					printf("calling LHNET_Write(<%s>, \"%s\", %i, <%s>)\n", addressstring2, sendmessage, sendmessagelength, addressstring);
					length = LHNET_Write(sendsock, sendmessage, sendmessagelength, &destaddress);
					if (length == sendmessagelength)
						printf("sent successfully\n");
					else
						printf("LH_Write failed, returned %i (length of message was %i)\n", length, strlen(argv[4]));
					oldtime = newtime;
					count--;
					if (count <= 0)
						printf("Done sending, still listening for \"exit\"\n");
				}
			}
		}
		for (i = 0;i < numsockets;i++)
		{
			if (sock[i])
			{
				LHNETADDRESS_ToString(LHNET_AddressFromSocket(sock[i]), addressstring2, sizeof(addressstring2), 1);
				printf("calling LHNET_CloseSocket(<%s>)\n", addressstring2);
				LHNET_CloseSocket(sock[i]);
			}
		}
		printf("calling LHNET_Shutdown()\n");
		LHNET_Shutdown();
		return 0;
	}
	printf("Testing code for lhnet.c\nusage: lhnettest <localportnumber> [<sendnumberoftimes> <sendaddress:port> <sendmessage>]\n");
	return -1;
#endif
}
#endif

