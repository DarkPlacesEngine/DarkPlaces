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
// net_main.c

#include "quakedef.h"
#include "net_master.h"

qsocket_t *net_activeSockets = NULL;
mempool_t *net_mempool;

qboolean	ipxAvailable = false;
qboolean	tcpipAvailable = false;

int			net_hostport;
int			DEFAULTnet_hostport = 26000;

char		my_ipx_address[NET_NAMELEN];
char		my_tcpip_address[NET_NAMELEN];

static qboolean	listening = false;

qboolean	slistInProgress = false;
qboolean	slistSilent = false;
qboolean	slistLocal = true;
static double	slistStartTime;
static int		slistLastShown;

static void Slist_Send(void);
static void Slist_Poll(void);
PollProcedure	slistSendProcedure = {NULL, 0.0, Slist_Send};
PollProcedure	slistPollProcedure = {NULL, 0.0, Slist_Poll};

static void InetSlist_Send(void);
static void InetSlist_Poll(void);
PollProcedure	inetSlistSendProcedure = {NULL, 0.0, InetSlist_Send};
PollProcedure	inetSlistPollProcedure = {NULL, 0.0, InetSlist_Poll};


sizebuf_t		net_message;
int				net_activeconnections = 0;

int messagesSent = 0;
int messagesReceived = 0;
int unreliableMessagesSent = 0;
int unreliableMessagesReceived = 0;

cvar_t	net_messagetimeout = {0, "net_messagetimeout","300"};
cvar_t	hostname = {CVAR_SAVE, "hostname", "UNNAMED"};

qboolean	configRestored = false;

// these two macros are to make the code more readable
#define sfunc	net_drivers[sock->driver]
#define dfunc	net_drivers[net_driverlevel]

int	net_driverlevel;

/*
#define SLSERVERS 1024
#define SLNAME 40
#define SLMAPNAME 16
#define SLMODNAME 16
typedef struct slserver_s
{
	unsigned int ipaddr;
	unsigned short port;
	unsigned short ping;
	char name[SLNAME];
	char mapname[SLMAPNAME];
	char modname[SLMODNAME];
}
slserver_t;

slserver_t sl_server[SLSERVERS];
int sl_numservers = 0;

void SL_ClearServers(void)
{
	sl_numservers = 0;
}

slserver_t *SL_FindServer(unsigned int ipaddr, unsigned short port)
{
	int i;
	slserver_t *sl;
	for (i = 0, sl = sl_server;i < sl_numservers;i++, sl++)
		if (sl->ipaddr == ipaddr && sl->port == port)
			return;
}

void SL_AddServer(unsigned int ipaddr, unsigned short port)
{
	if (SL_FindServer(ipaddr, port))
		return;
	memset(sl_server + sl_numservers, 0, sizeof(slserver_t));
	sl_server[sl_numservers].ipaddr = ipaddr;
	sl_server[sl_numservers].port = port;
	sl_server[sl_numservers].ping = 0xFFFF;
	sl_numservers++;
}

void SL_UpdateServerName(unsigned int ipaddr, unsigned short port, const char *name);
{
	int namelen;
	slserver_t *sl;
	sl = SL_FindServer(ipaddr, port);
	if (sl == NULL)
		return;
	memset(sl->name, 0, sizeof(sl->name));
	namelen = strlen(name);
	if (namelen > sizeof(sl->name) - 1)
		namelen = sizeof(sl->name) - 1;
	if (namelen)
		memcpy(sl->name, name, namelen);
}

void SL_UpdateServerModName(unsigned int ipaddr, unsigned short port, const char *name);
{
	int namelen;
	slserver_t *sl;
	sl = SL_FindServer(ipaddr, port);
	if (sl == NULL)
		return;
	memset(sl->modname, 0, sizeof(sl->modname));
	namelen = strlen(name);
	if (namelen > sizeof(sl->modname) - 1)
		namelen = sizeof(sl->modname) - 1;
	if (namelen)
		memcpy(sl->modname, name, namelen);
}

void SL_UpdateServerMapName(unsigned int ipaddr, unsigned short port, const char *name);
{
	int namelen;
	slserver_t *sl;
	sl = SL_FindServer(ipaddr, port);
	if (sl == NULL)
		return;
	memset(sl->mapname, 0, sizeof(sl->mapname));
	namelen = strlen(name);
	if (namelen > sizeof(sl->mapname) - 1)
		namelen = sizeof(sl->mapname) - 1;
	if (namelen)
		memcpy(sl->mapname, name, namelen);
}

void SL_UpdateServerPing(unsigned int ipaddr, unsigned short port, float ping);
{
	int i;
	slserver_t *sl;
	sl = SL_FindServer(ipaddr, port);
	if (sl == NULL)
		return;
	i = ping * 1000.0;
	sl->ping = bound(0, i, 9999);
}
*/


double			net_time;

double SetNetTime(void)
{
	net_time = Sys_DoubleTime();
	return net_time;
}


/*
===================
NET_NewQSocket

Called by drivers when a new communications endpoint is required
The sequence and buffer fields will be filled in properly
===================
*/
qsocket_t *NET_NewQSocket (void)
{
	qsocket_t	*sock;

	if (net_activeconnections >= svs.maxclients)
		return NULL;

	sock = Mem_Alloc(net_mempool, sizeof(qsocket_t));

	// add it to active list
	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = false;
	sock->connecttime = net_time;
	strcpy (sock->address,"UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->driverdata = NULL;
	sock->canSend = true;
	sock->sendNext = false;
	sock->lastMessageTime = net_time;
	sock->ackSequence = 0;
	sock->sendSequence = 0;
	sock->unreliableSendSequence = 0;
	sock->sendMessageLength = 0;
	sock->receiveSequence = 0;
	sock->unreliableReceiveSequence = 0;
	sock->receiveMessageLength = 0;

	return sock;
}


void NET_FreeQSocket(qsocket_t *sock)
{
	qsocket_t	*s;

	// remove it from active list
	if (sock == net_activeSockets)
		net_activeSockets = net_activeSockets->next;
	else
	{
		for (s = net_activeSockets; s; s = s->next)
			if (s->next == sock)
			{
				s->next = sock->next;
				break;
			}
		if (!s)
			Sys_Error ("NET_FreeQSocket: not active\n");
	}

	Mem_Free(sock);
}


static void NET_Listen_f (void)
{
	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"listen\" is \"%u\"\n", listening ? 1 : 0);
		return;
	}

	listening = atoi(Cmd_Argv(1)) ? true : false;

	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.Listen (listening);
	}
}


static void MaxPlayers_f (void)
{
	int n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"maxplayers\" is \"%u\"\n", svs.maxclients);
		return;
	}

	if (sv.active)
	{
		Con_Printf ("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = atoi(Cmd_Argv(1));
	n = bound(1, n, MAX_SCOREBOARD);
	if (svs.maxclients != n)
		Con_Printf ("\"maxplayers\" set to \"%u\"\n", n);

	if ((n == 1) && listening)
		Cbuf_AddText ("listen 0\n");

	if ((n > 1) && (!listening))
		Cbuf_AddText ("listen 1\n");

	SV_SetMaxClients(n);
}


static void NET_Port_f (void)
{
	int 	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"port\" is \"%u\"\n", net_hostport);
		return;
	}

	n = atoi(Cmd_Argv(1));
	if (n < 1 || n > 65534)
	{
		Con_Printf ("Bad value, must be between 1 and 65534\n");
		return;
	}

	DEFAULTnet_hostport = n;
	net_hostport = n;

	if (listening)
	{
		// force a change to the new port
		Cbuf_AddText ("listen 0\n");
		Cbuf_AddText ("listen 1\n");
	}
}


static void NET_Heartbeat_f (void)
{
	NET_Heartbeat (2);
}


static void PrintSlistHeader(void)
{
	Con_Printf("Server          Map             Users\n");
	Con_Printf("--------------- --------------- -----\n");
	slistLastShown = 0;
}


static void PrintSlist(void)
{
	int n;

	for (n = slistLastShown; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			Con_Printf("%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			Con_Printf("%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
	}
	slistLastShown = n;
}


static void PrintSlistTrailer(void)
{
	if (hostCacheCount)
		Con_Printf("== end list ==\n\n");
	else
	{
		if (gamemode == GAME_TRANSFUSION)
			Con_Printf("No Transfusion servers found.\n\n");
		else
			Con_Printf("No Quake servers found.\n\n");
	}
}


void NET_SlistCommon (PollProcedure *sendProcedure, PollProcedure *pollProcedure)
{
	if (slistInProgress)
		return;

	if (! slistSilent)
	{
		if (gamemode == GAME_TRANSFUSION)
			Con_Printf("Looking for Transfusion servers...\n");
		else
			Con_Printf("Looking for Quake servers...\n");
		PrintSlistHeader();
	}

	slistInProgress = true;
	slistStartTime = Sys_DoubleTime();

	SchedulePollProcedure(sendProcedure, 0.0);
	SchedulePollProcedure(pollProcedure, 0.1);

	hostCacheCount = 0;
}


void NET_Slist_f (void)
{
	NET_SlistCommon (&slistSendProcedure, &slistPollProcedure);
}


void NET_InetSlist_f (void)
{
	NET_SlistCommon (&inetSlistSendProcedure, &inetSlistPollProcedure);
}


static void Slist_Send(void)
{
	for (net_driverlevel=0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (true);
	}

	if ((Sys_DoubleTime() - slistStartTime) < 0.5)
		SchedulePollProcedure(&slistSendProcedure, 0.75);
}


static void Slist_Poll(void)
{
	for (net_driverlevel=0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (false);
	}

	if (! slistSilent)
		PrintSlist();

	if ((Sys_DoubleTime() - slistStartTime) < 1.5)
	{
		SchedulePollProcedure(&slistPollProcedure, 0.1);
		return;
	}

	if (! slistSilent)
		PrintSlistTrailer();
	slistInProgress = false;
	slistSilent = false;
	slistLocal = true;
}


static void InetSlist_Send(void)
{
	const char* host;

	if (!slistInProgress)
		return;

	while ((host = Master_BuildGetServers ()) != NULL)
	{
		for (net_driverlevel=0; net_driverlevel < net_numdrivers; net_driverlevel++)
		{
			if (!slistLocal && net_driverlevel == 0)
				continue;
			if (net_drivers[net_driverlevel].initialized == false)
				continue;
			dfunc.SearchForInetHosts (host);
		}
	}

	if ((Sys_DoubleTime() - slistStartTime) < 3.5)
		SchedulePollProcedure(&inetSlistSendProcedure, 1.0);
}


static void InetSlist_Poll(void)
{
	for (net_driverlevel=0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		// We stop as soon as we have one answer (FIXME: bad...)
		if (dfunc.SearchForInetHosts (NULL))
			slistInProgress = false;
	}

	if (! slistSilent)
		PrintSlist();

	if (slistInProgress && (Sys_DoubleTime() - slistStartTime) < 4.0)
	{
		SchedulePollProcedure(&inetSlistPollProcedure, 0.1);
		return;
	}

	if (! slistSilent)
		PrintSlistTrailer();
	slistInProgress = false;
	slistSilent = false;
	slistLocal = true;
}


/*
===================
NET_Connect
===================
*/

int hostCacheCount = 0;
hostcache_t hostcache[HOSTCACHESIZE];

qsocket_t *NET_Connect (char *host)
{
	qsocket_t		*ret;
	int				n;

	SetNetTime();

	if (host && *host == 0)
		host = NULL;

	if (host)
	{
		if (Q_strcasecmp (host, "local") == 0)
		{
			net_driverlevel = 0;
			return dfunc.Connect (host);
		}

		if (hostCacheCount)
		{
			for (n = 0; n < hostCacheCount; n++)
				if (Q_strcasecmp (host, hostcache[n].name) == 0)
				{
					host = hostcache[n].cname;
					break;
				}
			if (n < hostCacheCount)
				goto JustDoIt;
		}
	}

	slistSilent = host ? true : false;
	NET_Slist_f ();

	while(slistInProgress)
		NET_Poll();

	if (host == NULL)
	{
		if (hostCacheCount != 1)
			return NULL;
		host = hostcache[0].cname;
		Con_Printf("Connecting to...\n%s @ %s\n\n", hostcache[0].name, host);
	}

	if (hostCacheCount)
		for (n = 0; n < hostCacheCount; n++)
			if (Q_strcasecmp (host, hostcache[n].name) == 0)
			{
				host = hostcache[n].cname;
				break;
			}

JustDoIt:
	for (net_driverlevel = 0;net_driverlevel < net_numdrivers;net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		ret = dfunc.Connect (host);
		if (ret)
			return ret;
	}

	if (host)
	{
		Con_Printf("\n");
		PrintSlistHeader();
		PrintSlist();
		PrintSlistTrailer();
	}

	return NULL;
}


/*
===================
NET_CheckNewConnections
===================
*/

qsocket_t *NET_CheckNewConnections (void)
{
	qsocket_t	*ret;

	SetNetTime();

	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		if (net_driverlevel && listening == false)
			continue;
		ret = dfunc.CheckNewConnections ();
		if (ret)
			return ret;
	}

	return NULL;
}

/*
===================
NET_Close
===================
*/
void NET_Close (qsocket_t *sock)
{
	if (!sock)
		return;

	if (sock->disconnected)
		return;

	SetNetTime();

	// call the driver_Close function
	sfunc.Close (sock);

	NET_FreeQSocket(sock);
}


/*
=================
NET_GetMessage

If there is a complete message, return it in net_message

returns 0 if no data is waiting
returns 1 if a message was received
returns -1 if connection is invalid
=================
*/

extern void PrintStats(qsocket_t *s);

int	NET_GetMessage (qsocket_t *sock)
{
	int ret;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = sfunc.QGetMessage(sock);

	// see if this connection has timed out
	if (ret == 0 && sock->driver)
	{
		if (net_time - sock->lastMessageTime > net_messagetimeout.value)
		{
			NET_Close(sock);
			return -1;
		}
	}


	if (ret > 0)
	{
		if (sock->driver)
		{
			sock->lastMessageTime = net_time;
			if (ret == 1)
				messagesReceived++;
			else if (ret == 2)
				unreliableMessagesReceived++;
		}
	}

	return ret;
}


/*
==================
NET_SendMessage

Try to send a complete length+message unit over the reliable stream.
returns 0 if the message cannot be delivered reliably, but the connection
		is still considered valid
returns 1 if the message was sent properly
returns -1 if the connection died
==================
*/
int NET_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;
	
	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.QSendMessage(sock, data);
	if (r == 1 && sock->driver)
		messagesSent++;

	return r;
}


int NET_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;
	
	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.SendUnreliableMessage(sock, data);
	if (r == 1 && sock->driver)
		unreliableMessagesSent++;

	return r;
}


/*
==================
NET_CanSendMessage

Returns true or false if the given qsocket can currently accept a
message to be transmitted.
==================
*/
qboolean NET_CanSendMessage (qsocket_t *sock)
{
	int		r;
	
	if (!sock)
		return false;

	if (sock->disconnected)
		return false;

	SetNetTime();

	r = sfunc.CanSendMessage(sock);
	
	return r;
}


/*
====================
NET_Heartbeat

Send an heartbeat to the master server(s)
====================
*/
void NET_Heartbeat (int priority)
{
	const char* host;

	if (! Master_AllowHeartbeat (priority))
		return;

	while ((host = Master_BuildHeartbeat ()) != NULL)
	{
		for (net_driverlevel=0 ; net_driverlevel<net_numdrivers; net_driverlevel++)
		{
			if (net_drivers[net_driverlevel].initialized == false)
				continue;
			if (net_driverlevel && listening == false)
				continue;
			dfunc.Heartbeat (host);
		}
	}
}


int NET_SendToAll(sizebuf_t *data, int blocktime)
{
	double		start;
	int			i;
	int			count = 0;
	qboolean	state1 [MAX_SCOREBOARD];
	qboolean	state2 [MAX_SCOREBOARD];

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->netconnection)
			continue;
		if (host_client->active)
		{
			if (host_client->netconnection->driver == 0)
			{
				NET_SendMessage(host_client->netconnection, data);
				state1[i] = true;
				state2[i] = true;
				continue;
			}
			count++;
			state1[i] = false;
			state2[i] = false;
		}
		else
		{
			state1[i] = true;
			state2[i] = true;
		}
	}

	start = Sys_DoubleTime();
	while (count)
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (! state1[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					state1[i] = true;
					NET_SendMessage(host_client->netconnection, data);
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}

			if (! state2[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					state2[i] = true;
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}
		}
		if ((Sys_DoubleTime() - start) > blocktime)
			break;
	}
	return count;
}


//=============================================================================

/*
====================
NET_Init
====================
*/

void NET_Init (void)
{
	int			i;
	int			controlSocket;

	i = COM_CheckParm ("-port");
	if (!i)
		i = COM_CheckParm ("-udpport");
	if (!i)
		i = COM_CheckParm ("-ipxport");

	if (i)
	{
		if (i < com_argc-1)
			DEFAULTnet_hostport = atoi (com_argv[i+1]);
		else
			Sys_Error ("NET_Init: you must specify a number after -port");
	}
	net_hostport = DEFAULTnet_hostport;

	if (COM_CheckParm("-listen") || cls.state == ca_dedicated || gamemode == GAME_TRANSFUSION)
		listening = true;

	SetNetTime();

	net_mempool = Mem_AllocPool("qsocket");

	// allocate space for network message buffer
	SZ_Alloc (&net_message, NET_MAXMESSAGE, "net_message");

	Cvar_RegisterVariable (&net_messagetimeout);
	Cvar_RegisterVariable (&hostname);

	Cmd_AddCommand ("net_slist", NET_Slist_f);
	Cmd_AddCommand ("net_inetslist", NET_InetSlist_f);
	Cmd_AddCommand ("listen", NET_Listen_f);
	Cmd_AddCommand ("maxplayers", MaxPlayers_f);
	Cmd_AddCommand ("port", NET_Port_f);
	Cmd_AddCommand ("heartbeat", NET_Heartbeat_f);

	// initialize all the drivers
	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers ; net_driverlevel++)
		{
		controlSocket = net_drivers[net_driverlevel].Init();
		if (controlSocket == -1)
			continue;
		net_drivers[net_driverlevel].initialized = true;
		net_drivers[net_driverlevel].controlSock = controlSocket;
		if (listening)
			net_drivers[net_driverlevel].Listen (true);
		}

	if (*my_ipx_address)
		Con_DPrintf("IPX address %s\n", my_ipx_address);
	if (*my_tcpip_address)
		Con_DPrintf("TCP/IP address %s\n", my_tcpip_address);

	Master_Init ();
}

/*
====================
NET_Shutdown
====================
*/

void NET_Shutdown (void)
{
	SetNetTime();

	while (net_activeSockets)
		NET_Close(net_activeSockets);

//
// shutdown the drivers
//
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (net_drivers[net_driverlevel].initialized == true)
		{
			net_drivers[net_driverlevel].Shutdown ();
			net_drivers[net_driverlevel].initialized = false;
		}
	}

	Mem_FreePool(&net_mempool);
}


static PollProcedure *pollProcedureList = NULL;

void NET_Poll(void)
{
	PollProcedure *pp;

	if (!configRestored)
		configRestored = true;

	SetNetTime();

	for (pp = pollProcedureList; pp; pp = pp->next)
	{
		if (pp->nextTime > net_time)
			break;
		pollProcedureList = pp->next;
		pp->procedure(pp->arg);
	}
}


void SchedulePollProcedure(PollProcedure *proc, double timeOffset)
{
	PollProcedure *pp, *prev;

	proc->nextTime = Sys_DoubleTime() + timeOffset;
	for (pp = pollProcedureList, prev = NULL; pp; pp = pp->next)
	{
		if (pp->nextTime >= proc->nextTime)
			break;
		prev = pp;
	}

	if (prev == NULL)
	{
		proc->next = pollProcedureList;
		pollProcedureList = proc;
		return;
	}

	proc->next = pp;
	prev->next = proc;
}

