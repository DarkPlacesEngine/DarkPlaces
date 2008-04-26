/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2002 Mathieu Olivier
Copyright (C) 2003 Forest Hale

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
#include "lhnet.h"

#define QWMASTER_PORT 27000
#define DPMASTER_PORT 27950

// note this defaults on for dedicated servers, off for listen servers
cvar_t sv_public = {0, "sv_public", "0", "1: advertises this server on the master server (so that players can find it in the server browser); 0: allow direct queries only; -1: do not respond to direct queries; -2: do not allow anyone to connect"};
static cvar_t sv_heartbeatperiod = {CVAR_SAVE, "sv_heartbeatperiod", "120", "how often to send heartbeat in seconds (only used if sv_public is 1)"};

static cvar_t sv_masters [] =
{
	{CVAR_SAVE, "sv_master1", "", "user-chosen master server 1"},
	{CVAR_SAVE, "sv_master2", "", "user-chosen master server 2"},
	{CVAR_SAVE, "sv_master3", "", "user-chosen master server 3"},
	{CVAR_SAVE, "sv_master4", "", "user-chosen master server 4"},
	{0, "sv_masterextra1", "ghdigital.com", "default master server 1 (admin: LordHavoc)"}, // admin: LordHavoc
	{0, "sv_masterextra2", "dpmaster.deathmask.net", "default master server 2 (admin: Willis)"}, // admin: Willis
	{0, "sv_masterextra3", "dpmaster.tchr.no", "default master server 3 (admin: tChr)"}, // admin: tChr
	{0, NULL, NULL, NULL}
};

static cvar_t sv_qwmasters [] =
{
	{CVAR_SAVE, "sv_qwmaster1", "", "user-chosen qwmaster server 1"},
	{CVAR_SAVE, "sv_qwmaster2", "", "user-chosen qwmaster server 2"},
	{CVAR_SAVE, "sv_qwmaster3", "", "user-chosen qwmaster server 3"},
	{CVAR_SAVE, "sv_qwmaster4", "", "user-chosen qwmaster server 4"},
	{0, "sv_qwmasterextra1", "master.quakeservers.net:27000", "Global master server. (admin: unknown)"},
	{0, "sv_qwmasterextra2", "asgaard.morphos-team.net:27000", "Global master server. (admin: unknown)"},
	{0, "sv_qwmasterextra3", "qwmaster.ocrana.de:27000", "German master server. (admin: unknown)"},
	{0, "sv_qwmasterextra4", "masterserver.exhale.de:27000", "German master server. (admin: unknown)"},
	{0, "sv_qwmasterextra5", "kubus.rulez.pl:27000", "Poland master server. (admin: unknown)"},
	{0, NULL, NULL, NULL}
};

static double nextheartbeattime = 0;

sizebuf_t net_message;
static unsigned char net_message_buf[NET_MAXMESSAGE];

cvar_t net_messagetimeout = {0, "net_messagetimeout","300", "drops players who have not sent any packets for this many seconds"};
cvar_t net_connecttimeout = {0, "net_connecttimeout","10", "after requesting a connection, the client must reply within this many seconds or be dropped (cuts down on connect floods)"};
cvar_t net_connectfloodblockingtimeout = {0, "net_connectfloodblockingtimeout", "5", "when a connection packet is received, it will block all future connect packets from that IP address for this many seconds (cuts down on connect floods)"};
cvar_t hostname = {CVAR_SAVE, "hostname", "UNNAMED", "server message to show in server browser"};
cvar_t developer_networking = {0, "developer_networking", "0", "prints all received and sent packets (recommended only for debugging)"};

cvar_t cl_netlocalping = {0, "cl_netlocalping","0", "lags local loopback connection by this much ping time (useful to play more fairly on your own server with people with higher pings)"};
static cvar_t cl_netpacketloss_send = {0, "cl_netpacketloss_send","0", "drops this percentage of outgoing packets, useful for testing network protocol robustness (jerky movement, prediction errors, etc)"};
static cvar_t cl_netpacketloss_receive = {0, "cl_netpacketloss_receive","0", "drops this percentage of incoming packets, useful for testing network protocol robustness (jerky movement, effects failing to start, sounds failing to play, etc)"};
static cvar_t net_slist_queriespersecond = {0, "net_slist_queriespersecond", "20", "how many server information requests to send per second"};
static cvar_t net_slist_queriesperframe = {0, "net_slist_queriesperframe", "4", "maximum number of server information requests to send each rendered frame (guards against low framerates causing problems)"};
static cvar_t net_slist_timeout = {0, "net_slist_timeout", "4", "how long to listen for a server information response before giving up"};
static cvar_t net_slist_pause = {0, "net_slist_pause", "0", "when set to 1, the server list won't update until it is set back to 0"};
static cvar_t net_slist_maxtries = {0, "net_slist_maxtries", "3", "how many times to ask the same server for information (more times gives better ping reports but takes longer)"};

static cvar_t gameversion = {0, "gameversion", "0", "version of game data (mod-specific), when client and server gameversion mismatch in the server browser the server is shown as incompatible"};
static cvar_t rcon_restricted_password = {CVAR_PRIVATE, "rcon_restricted_password", "", "password to authenticate rcon commands in restricted mode"};
static cvar_t rcon_restricted_commands = {0, "rcon_restricted_commands", "", "allowed commands for rcon when the restricted mode password was used"};

/* statistic counters */
static int packetsSent = 0;
static int packetsReSent = 0;
static int packetsReceived = 0;
static int receivedDuplicateCount = 0;
static int droppedDatagrams = 0;

static int unreliableMessagesSent = 0;
static int unreliableMessagesReceived = 0;
static int reliableMessagesSent = 0;
static int reliableMessagesReceived = 0;

double masterquerytime = -1000;
int masterquerycount = 0;
int masterreplycount = 0;
int serverquerycount = 0;
int serverreplycount = 0;

// this is only false if there are still servers left to query
static qboolean serverlist_querysleep = true;
static qboolean serverlist_paused = false;
// this is pushed a second or two ahead of realtime whenever a master server
// reply is received, to avoid issuing queries while master replies are still
// flooding in (which would make a mess of the ping times)
static double serverlist_querywaittime = 0;

static unsigned char sendbuffer[NET_HEADERSIZE+NET_MAXMESSAGE];
static unsigned char readbuffer[NET_HEADERSIZE+NET_MAXMESSAGE];

static int cl_numsockets;
static lhnetsocket_t *cl_sockets[16];
static int sv_numsockets;
static lhnetsocket_t *sv_sockets[16];

netconn_t *netconn_list = NULL;
mempool_t *netconn_mempool = NULL;

cvar_t cl_netport = {0, "cl_port", "0", "forces client to use chosen port number if not 0"};
cvar_t sv_netport = {0, "port", "26000", "server port for players to connect to"};
cvar_t net_address = {0, "net_address", "0.0.0.0", "network address to open ports on"};
//cvar_t net_netaddress_ipv6 = {0, "net_address_ipv6", "[0:0:0:0:0:0:0:0]", "network address to open ipv6 ports on"};

char net_extresponse[NET_EXTRESPONSE_MAX][1400];
int net_extresponse_count = 0;
int net_extresponse_last = 0;

// ServerList interface
serverlist_mask_t serverlist_andmasks[SERVERLIST_ANDMASKCOUNT];
serverlist_mask_t serverlist_ormasks[SERVERLIST_ORMASKCOUNT];

serverlist_infofield_t serverlist_sortbyfield;
qboolean serverlist_sortdescending;

int serverlist_viewcount = 0;
serverlist_entry_t *serverlist_viewlist[SERVERLIST_VIEWLISTSIZE];

int serverlist_cachecount;
serverlist_entry_t serverlist_cache[SERVERLIST_TOTALSIZE];

qboolean serverlist_consoleoutput;

// helper function to insert a value into the viewset
// spare entries will be removed
static void _ServerList_ViewList_Helper_InsertBefore( int index, serverlist_entry_t *entry )
{
    int i;
	if( serverlist_viewcount < SERVERLIST_VIEWLISTSIZE ) {
		i = serverlist_viewcount++;
	} else {
		i = SERVERLIST_VIEWLISTSIZE - 1;
	}

	for( ; i > index ; i-- )
		serverlist_viewlist[ i ] = serverlist_viewlist[ i - 1 ];

	serverlist_viewlist[index] = entry;
}

// we suppose serverlist_viewcount to be valid, ie > 0
static void _ServerList_ViewList_Helper_Remove( int index )
{
	serverlist_viewcount--;
	for( ; index < serverlist_viewcount ; index++ )
		serverlist_viewlist[index] = serverlist_viewlist[index + 1];
}

// returns true if A should be inserted before B
static qboolean _ServerList_Entry_Compare( serverlist_entry_t *A, serverlist_entry_t *B )
{
	int result = 0; // > 0 if for numbers A > B and for text if A < B

	switch( serverlist_sortbyfield ) {
		case SLIF_PING:
			result = A->info.ping - B->info.ping;
			break;
		case SLIF_MAXPLAYERS:
			result = A->info.maxplayers - B->info.maxplayers;
			break;
		case SLIF_NUMPLAYERS:
			result = A->info.numplayers - B->info.numplayers;
			break;
		case SLIF_NUMBOTS:
			result = A->info.numbots - B->info.numbots;
			break;
		case SLIF_NUMHUMANS:
			result = A->info.numhumans - B->info.numhumans;
			break;
		case SLIF_FREESLOTS:
			result = A->info.freeslots - B->info.freeslots;
			break;
		case SLIF_PROTOCOL:
			result = A->info.protocol - B->info.protocol;
			break;
		case SLIF_CNAME:
			result = strcmp( B->info.cname, A->info.cname );
			break;
		case SLIF_GAME:
			result = strcasecmp( B->info.game, A->info.game );
			break;
		case SLIF_MAP:
			result = strcasecmp( B->info.map, A->info.map );
			break;
		case SLIF_MOD:
			result = strcasecmp( B->info.mod, A->info.mod );
			break;
		case SLIF_NAME:
			result = strcasecmp( B->info.name, A->info.name );
			break;
		default:
			Con_DPrint( "_ServerList_Entry_Compare: Bad serverlist_sortbyfield!\n" );
			break;
	}

	if( serverlist_sortdescending )
		return result > 0;
	if (result != 0)
		return result < 0;
	// if the chosen sort key is identical, sort by index
	// (makes this a stable sort, so that later replies from servers won't
	//  shuffle the servers around when they have the same ping)
	return A < B;
}

static qboolean _ServerList_CompareInt( int A, serverlist_maskop_t op, int B )
{
	// This should actually be done with some intermediate and end-of-function return
	switch( op ) {
		case SLMO_LESS:
			return A < B;
		case SLMO_LESSEQUAL:
			return A <= B;
		case SLMO_EQUAL:
			return A == B;
		case SLMO_GREATER:
			return A > B;
		case SLMO_NOTEQUAL:
			return A != B;
		case SLMO_GREATEREQUAL:
		case SLMO_CONTAINS:
		case SLMO_NOTCONTAIN:
			return A >= B;
		default:
			Con_DPrint( "_ServerList_CompareInt: Bad op!\n" );
			return false;
	}
}

static qboolean _ServerList_CompareStr( const char *A, serverlist_maskop_t op, const char *B )
{
	int i;
	char bufferA[ 256 ], bufferB[ 256 ]; // should be more than enough
	for (i = 0;i < (int)sizeof(bufferA)-1 && A[i];i++)
		bufferA[i] = (A[i] >= 'A' && A[i] <= 'Z') ? (A[i] + 'a' - 'A') : A[i];
	bufferA[i] = 0;
	for (i = 0;i < (int)sizeof(bufferB)-1 && B[i];i++)
		bufferB[i] = (B[i] >= 'A' && B[i] <= 'Z') ? (B[i] + 'a' - 'A') : B[i];
	bufferB[i] = 0;

	// Same here, also using an intermediate & final return would be more appropriate
	// A info B mask
	switch( op ) {
		case SLMO_CONTAINS:
			return *bufferB && !!strstr( bufferA, bufferB ); // we want a real bool
		case SLMO_NOTCONTAIN:
			return !*bufferB || !strstr( bufferA, bufferB );
		case SLMO_LESS:
			return strcmp( bufferA, bufferB ) < 0;
		case SLMO_LESSEQUAL:
			return strcmp( bufferA, bufferB ) <= 0;
		case SLMO_EQUAL:
			return strcmp( bufferA, bufferB ) == 0;
		case SLMO_GREATER:
			return strcmp( bufferA, bufferB ) > 0;
		case SLMO_NOTEQUAL:
			return strcmp( bufferA, bufferB ) != 0;
		case SLMO_GREATEREQUAL:
			return strcmp( bufferA, bufferB ) >= 0;
		default:
			Con_DPrint( "_ServerList_CompareStr: Bad op!\n" );
			return false;
	}
}

static qboolean _ServerList_Entry_Mask( serverlist_mask_t *mask, serverlist_info_t *info )
{
	if( !_ServerList_CompareInt( info->ping, mask->tests[SLIF_PING], mask->info.ping ) )
		return false;
	if( !_ServerList_CompareInt( info->maxplayers, mask->tests[SLIF_MAXPLAYERS], mask->info.maxplayers ) )
		return false;
	if( !_ServerList_CompareInt( info->numplayers, mask->tests[SLIF_NUMPLAYERS], mask->info.numplayers ) )
		return false;
	if( !_ServerList_CompareInt( info->numbots, mask->tests[SLIF_NUMBOTS], mask->info.numbots ) )
		return false;
	if( !_ServerList_CompareInt( info->numhumans, mask->tests[SLIF_NUMHUMANS], mask->info.numhumans ) )
		return false;
	if( !_ServerList_CompareInt( info->freeslots, mask->tests[SLIF_FREESLOTS], mask->info.freeslots ) )
		return false;
	if( !_ServerList_CompareInt( info->protocol, mask->tests[SLIF_PROTOCOL], mask->info.protocol ))
		return false;
	if( *mask->info.cname
		&& !_ServerList_CompareStr( info->cname, mask->tests[SLIF_CNAME], mask->info.cname ) )
		return false;
	if( *mask->info.game
		&& !_ServerList_CompareStr( info->game, mask->tests[SLIF_GAME], mask->info.game ) )
		return false;
	if( *mask->info.mod
		&& !_ServerList_CompareStr( info->mod, mask->tests[SLIF_MOD], mask->info.mod ) )
		return false;
	if( *mask->info.map
		&& !_ServerList_CompareStr( info->map, mask->tests[SLIF_MAP], mask->info.map ) )
		return false;
	if( *mask->info.name
		&& !_ServerList_CompareStr( info->name, mask->tests[SLIF_NAME], mask->info.name ) )
		return false;
	return true;
}

static void ServerList_ViewList_Insert( serverlist_entry_t *entry )
{
	int start, end, mid;

	// reject incompatible servers
	if (entry->info.gameversion != gameversion.integer)
		return;

	// FIXME: change this to be more readable (...)
	// now check whether it passes through the masks
	for( start = 0 ; serverlist_andmasks[start].active && start < SERVERLIST_ANDMASKCOUNT ; start++ )
		if( !_ServerList_Entry_Mask( &serverlist_andmasks[start], &entry->info ) )
			return;

	for( start = 0 ; serverlist_ormasks[start].active && start < SERVERLIST_ORMASKCOUNT ; start++ )
		if( _ServerList_Entry_Mask( &serverlist_ormasks[start], &entry->info ) )
			break;
	if( start == SERVERLIST_ORMASKCOUNT || (start > 0 && !serverlist_ormasks[start].active) )
		return;

	if( !serverlist_viewcount ) {
		_ServerList_ViewList_Helper_InsertBefore( 0, entry );
		return;
	}
	// ok, insert it, we just need to find out where exactly:

	// two special cases
	// check whether to insert it as new first item
	if( _ServerList_Entry_Compare( entry, serverlist_viewlist[0] ) ) {
		_ServerList_ViewList_Helper_InsertBefore( 0, entry );
		return;
	} // check whether to insert it as new last item
	else if( !_ServerList_Entry_Compare( entry, serverlist_viewlist[serverlist_viewcount - 1] ) ) {
		_ServerList_ViewList_Helper_InsertBefore( serverlist_viewcount, entry );
		return;
	}
	start = 0;
	end = serverlist_viewcount - 1;
	while( end > start + 1 )
	{
		mid = (start + end) / 2;
		// test the item that lies in the middle between start and end
		if( _ServerList_Entry_Compare( entry, serverlist_viewlist[mid] ) )
			// the item has to be in the upper half
			end = mid;
		else
			// the item has to be in the lower half
			start = mid;
	}
	_ServerList_ViewList_Helper_InsertBefore( start + 1, entry );
}

static void ServerList_ViewList_Remove( serverlist_entry_t *entry )
{
	int i;
	for( i = 0; i < serverlist_viewcount; i++ )
	{
		if (serverlist_viewlist[i] == entry)
		{
			_ServerList_ViewList_Helper_Remove(i);
			break;
		}
	}
}

void ServerList_RebuildViewList(void)
{
	int i;

	serverlist_viewcount = 0;
	for( i = 0 ; i < serverlist_cachecount ; i++ ) {
		serverlist_entry_t *entry = &serverlist_cache[i];
		// also display entries that are currently being refreshed [11/8/2007 Black]
		if( entry->query == SQS_QUERIED || entry->query == SQS_REFRESHING )
			ServerList_ViewList_Insert( entry );
	}
}

void ServerList_ResetMasks(void)
{
	int i;

	memset( &serverlist_andmasks, 0, sizeof( serverlist_andmasks ) );
	memset( &serverlist_ormasks, 0, sizeof( serverlist_ormasks ) );
	// numbots needs to be compared to -1 to always succeed
	for(i = 0; i < SERVERLIST_ANDMASKCOUNT; ++i)
		serverlist_andmasks[i].info.numbots = -1;
	for(i = 0; i < SERVERLIST_ORMASKCOUNT; ++i)
		serverlist_ormasks[i].info.numbots = -1;
}

void ServerList_GetPlayerStatistics(int *numplayerspointer, int *maxplayerspointer)
{
	int i;
	int numplayers = 0, maxplayers = 0;
	for (i = 0;i < serverlist_cachecount;i++)
	{
		if (serverlist_cache[i].query == SQS_QUERIED)
		{
			numplayers += serverlist_cache[i].info.numhumans;
			maxplayers += serverlist_cache[i].info.maxplayers;
		}
	}
	*numplayerspointer = numplayers;
	*maxplayerspointer = maxplayers;
}

#if 0
static void _ServerList_Test(void)
{
	int i;
	for( i = 0 ; i < 1024 ; i++ ) {
		memset( &serverlist_cache[serverlist_cachecount], 0, sizeof( serverlist_entry_t ) );
		serverlist_cache[serverlist_cachecount].info.ping = 1000 + 1024 - i;
		dpsnprintf( serverlist_cache[serverlist_cachecount].info.name, sizeof(serverlist_cache[serverlist_cachecount].info.name), "Black's ServerList Test %i", i );
		serverlist_cache[serverlist_cachecount].finished = true;
		sprintf( serverlist_cache[serverlist_cachecount].line1, "%i %s", serverlist_cache[serverlist_cachecount].info.ping, serverlist_cache[serverlist_cachecount].info.name );
		ServerList_ViewList_Insert( &serverlist_cache[serverlist_cachecount] );
		serverlist_cachecount++;
	}
}
#endif

void ServerList_QueryList(qboolean resetcache, qboolean querydp, qboolean queryqw, qboolean consoleoutput)
{
	masterquerytime = realtime;
	masterquerycount = 0;
	masterreplycount = 0;
	if( resetcache ) {
		serverquerycount = 0;
		serverreplycount = 0;
		serverlist_cachecount = 0;
		serverlist_viewcount = 0;
	} else {
		// refresh all entries
		int n;
		for( n = 0 ; n < serverlist_cachecount ; n++ ) {
			serverlist_entry_t *entry = &serverlist_cache[ n ];
			entry->query = SQS_REFRESHING;
			entry->querycounter = 0;
		}
	}
	serverlist_consoleoutput = consoleoutput;

	//_ServerList_Test();

	NetConn_QueryMasters(querydp, queryqw);
}

// rest

int NetConn_Read(lhnetsocket_t *mysocket, void *data, int maxlength, lhnetaddress_t *peeraddress)
{
	int length = LHNET_Read(mysocket, data, maxlength, peeraddress);
	int i;
	if (length == 0)
		return 0;
	if (cl_netpacketloss_receive.integer)
		for (i = 0;i < cl_numsockets;i++)
			if (cl_sockets[i] == mysocket && (rand() % 100) < cl_netpacketloss_receive.integer)
				return 0;
	if (developer_networking.integer)
	{
		char addressstring[128], addressstring2[128];
		LHNETADDRESS_ToString(LHNET_AddressFromSocket(mysocket), addressstring, sizeof(addressstring), true);
		if (length > 0)
		{
			LHNETADDRESS_ToString(peeraddress, addressstring2, sizeof(addressstring2), true);
			Con_Printf("LHNET_Read(%p (%s), %p, %i, %p) = %i from %s:\n", mysocket, addressstring, data, maxlength, peeraddress, length, addressstring2);
			Com_HexDumpToConsole((unsigned char *)data, length);
		}
		else
			Con_Printf("LHNET_Read(%p (%s), %p, %i, %p) = %i\n", mysocket, addressstring, data, maxlength, peeraddress, length);
	}
	return length;
}

int NetConn_Write(lhnetsocket_t *mysocket, const void *data, int length, const lhnetaddress_t *peeraddress)
{
	int ret;
	int i;
	if (cl_netpacketloss_send.integer)
		for (i = 0;i < cl_numsockets;i++)
			if (cl_sockets[i] == mysocket && (rand() % 100) < cl_netpacketloss_send.integer)
				return length;
	ret = LHNET_Write(mysocket, data, length, peeraddress);
	if (developer_networking.integer)
	{
		char addressstring[128], addressstring2[128];
		LHNETADDRESS_ToString(LHNET_AddressFromSocket(mysocket), addressstring, sizeof(addressstring), true);
		LHNETADDRESS_ToString(peeraddress, addressstring2, sizeof(addressstring2), true);
		Con_Printf("LHNET_Write(%p (%s), %p, %i, %p (%s)) = %i%s\n", mysocket, addressstring, data, length, peeraddress, addressstring2, length, ret == length ? "" : " (ERROR)");
		Com_HexDumpToConsole((unsigned char *)data, length);
	}
	return ret;
}

int NetConn_WriteString(lhnetsocket_t *mysocket, const char *string, const lhnetaddress_t *peeraddress)
{
	// note this does not include the trailing NULL because we add that in the parser
	return NetConn_Write(mysocket, string, (int)strlen(string), peeraddress);
}

qboolean NetConn_CanSend(netconn_t *conn)
{
	conn->outgoing_packetcounter = (conn->outgoing_packetcounter + 1) % NETGRAPH_PACKETS;
	conn->outgoing_unreliablesize[conn->outgoing_packetcounter] = NETGRAPH_NOPACKET;
	conn->outgoing_reliablesize[conn->outgoing_packetcounter] = NETGRAPH_NOPACKET;
	conn->outgoing_acksize[conn->outgoing_packetcounter] = NETGRAPH_NOPACKET;
	if (realtime > conn->cleartime)
		return true;
	else
	{
		conn->outgoing_unreliablesize[conn->outgoing_packetcounter] = NETGRAPH_CHOKEDPACKET;
		return false;
	}
}

int NetConn_SendUnreliableMessage(netconn_t *conn, sizebuf_t *data, protocolversion_t protocol, int rate, qboolean quakesignon_suppressreliables)
{
	int totallen = 0;

	// if this packet was supposedly choked, but we find ourselves sending one
	// anyway, make sure the size counting starts at zero
	// (this mostly happens on level changes and disconnects and such)
	if (conn->outgoing_unreliablesize[conn->outgoing_packetcounter] == NETGRAPH_CHOKEDPACKET)
		conn->outgoing_unreliablesize[conn->outgoing_packetcounter] = NETGRAPH_NOPACKET;

	if (protocol == PROTOCOL_QUAKEWORLD)
	{
		int packetLen;
		qboolean sendreliable;

		// note that it is ok to send empty messages to the qw server,
		// otherwise it won't respond to us at all

		sendreliable = false;
		// if the remote side dropped the last reliable message, resend it
		if (conn->qw.incoming_acknowledged > conn->qw.last_reliable_sequence && conn->qw.incoming_reliable_acknowledged != conn->qw.reliable_sequence)
			sendreliable = true;
		// if the reliable transmit buffer is empty, copy the current message out
		if (!conn->sendMessageLength && conn->message.cursize)
		{
			memcpy (conn->sendMessage, conn->message.data, conn->message.cursize);
			conn->sendMessageLength = conn->message.cursize;
			SZ_Clear(&conn->message); // clear the message buffer
			conn->qw.reliable_sequence ^= 1;
			sendreliable = true;
		}
		// outgoing unreliable packet number, and outgoing reliable packet number (0 or 1)
		*((int *)(sendbuffer + 0)) = LittleLong((unsigned int)conn->outgoing_unreliable_sequence | ((unsigned int)sendreliable<<31));
		// last received unreliable packet number, and last received reliable packet number (0 or 1)
		*((int *)(sendbuffer + 4)) = LittleLong((unsigned int)conn->qw.incoming_sequence | ((unsigned int)conn->qw.incoming_reliable_sequence<<31));
		packetLen = 8;
		conn->outgoing_unreliable_sequence++;
		// client sends qport in every packet
		if (conn == cls.netcon)
		{
			*((short *)(sendbuffer + 8)) = LittleShort(cls.qw_qport);
			packetLen += 2;
			// also update cls.qw_outgoing_sequence
			cls.qw_outgoing_sequence = conn->outgoing_unreliable_sequence;
		}
		if (packetLen + (sendreliable ? conn->sendMessageLength : 0) > 1400)
		{
			Con_Printf ("NetConn_SendUnreliableMessage: reliable message too big %u\n", data->cursize);
			return -1;
		}

		conn->outgoing_unreliablesize[conn->outgoing_packetcounter] += packetLen;

		// add the reliable message if there is one
		if (sendreliable)
		{
			conn->outgoing_reliablesize[conn->outgoing_packetcounter] += conn->sendMessageLength;
			memcpy(sendbuffer + packetLen, conn->sendMessage, conn->sendMessageLength);
			packetLen += conn->sendMessageLength;
			conn->qw.last_reliable_sequence = conn->outgoing_unreliable_sequence;
		}

		// add the unreliable message if possible
		if (packetLen + data->cursize <= 1400)
		{
			conn->outgoing_unreliablesize[conn->outgoing_packetcounter] += data->cursize;
			memcpy(sendbuffer + packetLen, data->data, data->cursize);
			packetLen += data->cursize;
		}

		NetConn_Write(conn->mysocket, (void *)&sendbuffer, packetLen, &conn->peeraddress);

		packetsSent++;
		unreliableMessagesSent++;

		totallen += packetLen + 28;
	}
	else
	{
		unsigned int packetLen;
		unsigned int dataLen;
		unsigned int eom;
		unsigned int *header;

		// if a reliable message fragment has been lost, send it again
		if (conn->sendMessageLength && (realtime - conn->lastSendTime) > 1.0)
		{
			if (conn->sendMessageLength <= MAX_PACKETFRAGMENT)
			{
				dataLen = conn->sendMessageLength;
				eom = NETFLAG_EOM;
			}
			else
			{
				dataLen = MAX_PACKETFRAGMENT;
				eom = 0;
			}

			packetLen = NET_HEADERSIZE + dataLen;

			header = (unsigned int *)sendbuffer;
			header[0] = BigLong(packetLen | (NETFLAG_DATA | eom));
			header[1] = BigLong(conn->nq.sendSequence - 1);
			memcpy(sendbuffer + NET_HEADERSIZE, conn->sendMessage, dataLen);

			conn->outgoing_reliablesize[conn->outgoing_packetcounter] += packetLen;

			if (NetConn_Write(conn->mysocket, (void *)&sendbuffer, packetLen, &conn->peeraddress) == (int)packetLen)
			{
				conn->lastSendTime = realtime;
				packetsReSent++;
			}

			totallen += packetLen + 28;
		}

		// if we have a new reliable message to send, do so
		if (!conn->sendMessageLength && conn->message.cursize && !quakesignon_suppressreliables)
		{
			if (conn->message.cursize > (int)sizeof(conn->sendMessage))
			{
				Con_Printf("NetConn_SendUnreliableMessage: reliable message too big (%u > %u)\n", conn->message.cursize, (int)sizeof(conn->sendMessage));
				conn->message.overflowed = true;
				return -1;
			}

			if (developer_networking.integer && conn == cls.netcon)
			{
				Con_Print("client sending reliable message to server:\n");
				SZ_HexDumpToConsole(&conn->message);
			}

			memcpy(conn->sendMessage, conn->message.data, conn->message.cursize);
			conn->sendMessageLength = conn->message.cursize;
			SZ_Clear(&conn->message);

			if (conn->sendMessageLength <= MAX_PACKETFRAGMENT)
			{
				dataLen = conn->sendMessageLength;
				eom = NETFLAG_EOM;
			}
			else
			{
				dataLen = MAX_PACKETFRAGMENT;
				eom = 0;
			}

			packetLen = NET_HEADERSIZE + dataLen;

			header = (unsigned int *)sendbuffer;
			header[0] = BigLong(packetLen | (NETFLAG_DATA | eom));
			header[1] = BigLong(conn->nq.sendSequence);
			memcpy(sendbuffer + NET_HEADERSIZE, conn->sendMessage, dataLen);

			conn->nq.sendSequence++;

			conn->outgoing_reliablesize[conn->outgoing_packetcounter] += packetLen;

			NetConn_Write(conn->mysocket, (void *)&sendbuffer, packetLen, &conn->peeraddress);

			conn->lastSendTime = realtime;
			packetsSent++;
			reliableMessagesSent++;

			totallen += packetLen + 28;
		}

		// if we have an unreliable message to send, do so
		if (data->cursize)
		{
			packetLen = NET_HEADERSIZE + data->cursize;

			if (packetLen > (int)sizeof(sendbuffer))
			{
				Con_Printf("NetConn_SendUnreliableMessage: message too big %u\n", data->cursize);
				return -1;
			}

			header = (unsigned int *)sendbuffer;
			header[0] = BigLong(packetLen | NETFLAG_UNRELIABLE);
			header[1] = BigLong(conn->outgoing_unreliable_sequence);
			memcpy(sendbuffer + NET_HEADERSIZE, data->data, data->cursize);

			conn->outgoing_unreliable_sequence++;

			conn->outgoing_unreliablesize[conn->outgoing_packetcounter] += packetLen;

			NetConn_Write(conn->mysocket, (void *)&sendbuffer, packetLen, &conn->peeraddress);

			packetsSent++;
			unreliableMessagesSent++;

			totallen += packetLen + 28;
		}
	}

	// delay later packets to obey rate limit
	if (conn->cleartime < realtime - 0.1)
		conn->cleartime = realtime - 0.1;
	conn->cleartime = conn->cleartime + (double)totallen / (double)rate;
	if (conn->cleartime < realtime)
		conn->cleartime = realtime;

	return 0;
}

qboolean NetConn_HaveClientPorts(void)
{
	return !!cl_numsockets;
}

qboolean NetConn_HaveServerPorts(void)
{
	return !!sv_numsockets;
}

void NetConn_CloseClientPorts(void)
{
	for (;cl_numsockets > 0;cl_numsockets--)
		if (cl_sockets[cl_numsockets - 1])
			LHNET_CloseSocket(cl_sockets[cl_numsockets - 1]);
}

void NetConn_OpenClientPort(const char *addressstring, int defaultport)
{
	lhnetaddress_t address;
	lhnetsocket_t *s;
	char addressstring2[1024];
	if (LHNETADDRESS_FromString(&address, addressstring, defaultport))
	{
		if ((s = LHNET_OpenSocket_Connectionless(&address)))
		{
			cl_sockets[cl_numsockets++] = s;
			LHNETADDRESS_ToString(LHNET_AddressFromSocket(s), addressstring2, sizeof(addressstring2), true);
			Con_Printf("Client opened a socket on address %s\n", addressstring2);
		}
		else
		{
			LHNETADDRESS_ToString(&address, addressstring2, sizeof(addressstring2), true);
			Con_Printf("Client failed to open a socket on address %s\n", addressstring2);
		}
	}
	else
		Con_Printf("Client unable to parse address %s\n", addressstring);
}

void NetConn_OpenClientPorts(void)
{
	int port;
	NetConn_CloseClientPorts();
	port = bound(0, cl_netport.integer, 65535);
	if (cl_netport.integer != port)
		Cvar_SetValueQuick(&cl_netport, port);
	if(port == 0)
		Con_Printf("Client using an automatically assigned port\n");
	else
		Con_Printf("Client using port %i\n", port);
	NetConn_OpenClientPort("local:2", 0);
	NetConn_OpenClientPort(net_address.string, port);
	//NetConn_OpenClientPort(net_address_ipv6.string, port);
}

void NetConn_CloseServerPorts(void)
{
	for (;sv_numsockets > 0;sv_numsockets--)
		if (sv_sockets[sv_numsockets - 1])
			LHNET_CloseSocket(sv_sockets[sv_numsockets - 1]);
}

void NetConn_OpenServerPort(const char *addressstring, int defaultport)
{
	lhnetaddress_t address;
	lhnetsocket_t *s;
	int port;
	char addressstring2[1024];

	for (port = defaultport; port <= defaultport + 100; port++)
	{
		if (LHNETADDRESS_FromString(&address, addressstring, port))
		{
			if ((s = LHNET_OpenSocket_Connectionless(&address)))
			{
				sv_sockets[sv_numsockets++] = s;
				LHNETADDRESS_ToString(LHNET_AddressFromSocket(s), addressstring2, sizeof(addressstring2), true);
				Con_Printf("Server listening on address %s\n", addressstring2);
				break;
			}
			else
			{
				LHNETADDRESS_ToString(&address, addressstring2, sizeof(addressstring2), true);
				Con_Printf("Server failed to open socket on address %s\n", addressstring2);
			}
		}
		else
		{
			Con_Printf("Server unable to parse address %s\n", addressstring);
			// if it cant parse one address, it wont be able to parse another for sure
			break;
		}
	}
}

void NetConn_OpenServerPorts(int opennetports)
{
	int port;
	NetConn_CloseServerPorts();
	NetConn_UpdateSockets();
	port = bound(0, sv_netport.integer, 65535);
	if (port == 0)
		port = 26000;
	Con_Printf("Server using port %i\n", port);
	if (sv_netport.integer != port)
		Cvar_SetValueQuick(&sv_netport, port);
	if (cls.state != ca_dedicated)
		NetConn_OpenServerPort("local:1", 0);
	if (opennetports)
	{
		NetConn_OpenServerPort(net_address.string, port);
		//NetConn_OpenServerPort(net_address_ipv6.string, port);
	}
	if (sv_numsockets == 0)
		Host_Error("NetConn_OpenServerPorts: unable to open any ports!");
}

lhnetsocket_t *NetConn_ChooseClientSocketForAddress(lhnetaddress_t *address)
{
	int i, a = LHNETADDRESS_GetAddressType(address);
	for (i = 0;i < cl_numsockets;i++)
		if (cl_sockets[i] && LHNETADDRESS_GetAddressType(LHNET_AddressFromSocket(cl_sockets[i])) == a)
			return cl_sockets[i];
	return NULL;
}

lhnetsocket_t *NetConn_ChooseServerSocketForAddress(lhnetaddress_t *address)
{
	int i, a = LHNETADDRESS_GetAddressType(address);
	for (i = 0;i < sv_numsockets;i++)
		if (sv_sockets[i] && LHNETADDRESS_GetAddressType(LHNET_AddressFromSocket(sv_sockets[i])) == a)
			return sv_sockets[i];
	return NULL;
}

netconn_t *NetConn_Open(lhnetsocket_t *mysocket, lhnetaddress_t *peeraddress)
{
	netconn_t *conn;
	conn = (netconn_t *)Mem_Alloc(netconn_mempool, sizeof(*conn));
	conn->mysocket = mysocket;
	conn->peeraddress = *peeraddress;
	conn->lastMessageTime = realtime;
	conn->message.data = conn->messagedata;
	conn->message.maxsize = sizeof(conn->messagedata);
	conn->message.cursize = 0;
	// LordHavoc: (inspired by ProQuake) use a short connect timeout to
	// reduce effectiveness of connection request floods
	conn->timeout = realtime + net_connecttimeout.value;
	LHNETADDRESS_ToString(&conn->peeraddress, conn->address, sizeof(conn->address), true);
	conn->next = netconn_list;
	netconn_list = conn;
	return conn;
}

void NetConn_ClearConnectFlood(lhnetaddress_t *peeraddress);
void NetConn_Close(netconn_t *conn)
{
	netconn_t *c;
	// remove connection from list

	// allow the client to reconnect immediately
	NetConn_ClearConnectFlood(&(conn->peeraddress));

	if (conn == netconn_list)
		netconn_list = conn->next;
	else
	{
		for (c = netconn_list;c;c = c->next)
		{
			if (c->next == conn)
			{
				c->next = conn->next;
				break;
			}
		}
		// not found in list, we'll avoid crashing here...
		if (!c)
			return;
	}
	// free connection
	Mem_Free(conn);
}

static int clientport = -1;
static int clientport2 = -1;
static int hostport = -1;
void NetConn_UpdateSockets(void)
{
	if (cls.state != ca_dedicated)
	{
		if (clientport2 != cl_netport.integer)
		{
			clientport2 = cl_netport.integer;
			if (cls.state == ca_connected)
				Con_Print("Changing \"cl_port\" will not take effect until you reconnect.\n");
		}
		if (cls.state == ca_disconnected && clientport != clientport2)
		{
			clientport = clientport2;
			NetConn_CloseClientPorts();
		}
		if (cl_numsockets == 0)
			NetConn_OpenClientPorts();
	}

	if (hostport != sv_netport.integer)
	{
		hostport = sv_netport.integer;
		if (sv.active)
			Con_Print("Changing \"port\" will not take effect until \"map\" command is executed.\n");
	}
}

static int NetConn_ReceivedMessage(netconn_t *conn, unsigned char *data, int length, protocolversion_t protocol, double newtimeout)
{
	int originallength = length;
	if (length < 8)
		return 0;

	if (protocol == PROTOCOL_QUAKEWORLD)
	{
		int sequence, sequence_ack;
		int reliable_ack, reliable_message;
		int count;
		int qport;

		sequence = LittleLong(*((int *)(data + 0)));
		sequence_ack = LittleLong(*((int *)(data + 4)));
		data += 8;
		length -= 8;

		if (conn != cls.netcon)
		{
			// server only
			if (length < 2)
				return 0;
			// TODO: use qport to identify that this client really is who they say they are?  (and elsewhere in the code to identify the connection without a port match?)
			qport = LittleShort(*((int *)(data + 8)));
			data += 2;
			length -= 2;
		}

		packetsReceived++;
		reliable_message = (sequence >> 31) & 1;
		reliable_ack = (sequence_ack >> 31) & 1;
		sequence &= ~(1<<31);
		sequence_ack &= ~(1<<31);
		if (sequence <= conn->qw.incoming_sequence)
		{
			//Con_DPrint("Got a stale datagram\n");
			return 0;
		}
		count = sequence - (conn->qw.incoming_sequence + 1);
		if (count > 0)
		{
			droppedDatagrams += count;
			//Con_DPrintf("Dropped %u datagram(s)\n", count);
			while (count--)
			{
				conn->incoming_packetcounter = (conn->incoming_packetcounter + 1) % NETGRAPH_PACKETS;
				conn->incoming_unreliablesize[conn->incoming_packetcounter] = NETGRAPH_LOSTPACKET;
				conn->incoming_reliablesize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
				conn->incoming_acksize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
			}
		}
		conn->incoming_packetcounter = (conn->incoming_packetcounter + 1) % NETGRAPH_PACKETS;
		conn->incoming_unreliablesize[conn->incoming_packetcounter] = originallength;
		conn->incoming_reliablesize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
		conn->incoming_acksize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
		if (reliable_ack == conn->qw.reliable_sequence)
		{
			// received, now we will be able to send another reliable message
			conn->sendMessageLength = 0;
			reliableMessagesReceived++;
		}
		conn->qw.incoming_sequence = sequence;
		if (conn == cls.netcon)
			cls.qw_incoming_sequence = conn->qw.incoming_sequence;
		conn->qw.incoming_acknowledged = sequence_ack;
		conn->qw.incoming_reliable_acknowledged = reliable_ack;
		if (reliable_message)
			conn->qw.incoming_reliable_sequence ^= 1;
		conn->lastMessageTime = realtime;
		conn->timeout = realtime + newtimeout;
		unreliableMessagesReceived++;
		SZ_Clear(&net_message);
		SZ_Write(&net_message, data, length);
		MSG_BeginReading();
		return 2;
	}
	else
	{
		unsigned int count;
		unsigned int flags;
		unsigned int sequence;
		int qlength;

		qlength = (unsigned int)BigLong(((int *)data)[0]);
		flags = qlength & ~NETFLAG_LENGTH_MASK;
		qlength &= NETFLAG_LENGTH_MASK;
		// control packets were already handled
		if (!(flags & NETFLAG_CTL) && qlength == length)
		{
			sequence = BigLong(((int *)data)[1]);
			packetsReceived++;
			data += 8;
			length -= 8;
			if (flags & NETFLAG_UNRELIABLE)
			{
				if (sequence >= conn->nq.unreliableReceiveSequence)
				{
					if (sequence > conn->nq.unreliableReceiveSequence)
					{
						count = sequence - conn->nq.unreliableReceiveSequence;
						droppedDatagrams += count;
						//Con_DPrintf("Dropped %u datagram(s)\n", count);
						while (count--)
						{
							conn->incoming_packetcounter = (conn->incoming_packetcounter + 1) % NETGRAPH_PACKETS;
							conn->incoming_unreliablesize[conn->incoming_packetcounter] = NETGRAPH_LOSTPACKET;
							conn->incoming_reliablesize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
							conn->incoming_acksize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
						}
					}
					conn->incoming_packetcounter = (conn->incoming_packetcounter + 1) % NETGRAPH_PACKETS;
					conn->incoming_unreliablesize[conn->incoming_packetcounter] = originallength;
					conn->incoming_reliablesize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
					conn->incoming_acksize[conn->incoming_packetcounter] = NETGRAPH_NOPACKET;
					conn->nq.unreliableReceiveSequence = sequence + 1;
					conn->lastMessageTime = realtime;
					conn->timeout = realtime + newtimeout;
					unreliableMessagesReceived++;
					if (length > 0)
					{
						SZ_Clear(&net_message);
						SZ_Write(&net_message, data, length);
						MSG_BeginReading();
						return 2;
					}
				}
				//else
				//	Con_DPrint("Got a stale datagram\n");
				return 1;
			}
			else if (flags & NETFLAG_ACK)
			{
				conn->incoming_acksize[conn->incoming_packetcounter] += originallength;
				if (sequence == (conn->nq.sendSequence - 1))
				{
					if (sequence == conn->nq.ackSequence)
					{
						conn->nq.ackSequence++;
						if (conn->nq.ackSequence != conn->nq.sendSequence)
							Con_DPrint("ack sequencing error\n");
						conn->lastMessageTime = realtime;
						conn->timeout = realtime + newtimeout;
						if (conn->sendMessageLength > MAX_PACKETFRAGMENT)
						{
							unsigned int packetLen;
							unsigned int dataLen;
							unsigned int eom;
							unsigned int *header;

							conn->sendMessageLength -= MAX_PACKETFRAGMENT;
							memmove(conn->sendMessage, conn->sendMessage+MAX_PACKETFRAGMENT, conn->sendMessageLength);

							if (conn->sendMessageLength <= MAX_PACKETFRAGMENT)
							{
								dataLen = conn->sendMessageLength;
								eom = NETFLAG_EOM;
							}
							else
							{
								dataLen = MAX_PACKETFRAGMENT;
								eom = 0;
							}

							packetLen = NET_HEADERSIZE + dataLen;

							header = (unsigned int *)sendbuffer;
							header[0] = BigLong(packetLen | (NETFLAG_DATA | eom));
							header[1] = BigLong(conn->nq.sendSequence);
							memcpy(sendbuffer + NET_HEADERSIZE, conn->sendMessage, dataLen);

							conn->nq.sendSequence++;

							if (NetConn_Write(conn->mysocket, (void *)&sendbuffer, packetLen, &conn->peeraddress) == (int)packetLen)
							{
								conn->lastSendTime = realtime;
								packetsSent++;
							}
						}
						else
							conn->sendMessageLength = 0;
					}
					//else
					//	Con_DPrint("Duplicate ACK received\n");
				}
				//else
				//	Con_DPrint("Stale ACK received\n");
				return 1;
			}
			else if (flags & NETFLAG_DATA)
			{
				unsigned int temppacket[2];
				conn->incoming_reliablesize[conn->incoming_packetcounter] += originallength;
				conn->outgoing_acksize[conn->outgoing_packetcounter] += 8;
				temppacket[0] = BigLong(8 | NETFLAG_ACK);
				temppacket[1] = BigLong(sequence);
				NetConn_Write(conn->mysocket, (unsigned char *)temppacket, 8, &conn->peeraddress);
				if (sequence == conn->nq.receiveSequence)
				{
					conn->lastMessageTime = realtime;
					conn->timeout = realtime + newtimeout;
					conn->nq.receiveSequence++;
					if( conn->receiveMessageLength + length <= (int)sizeof( conn->receiveMessage ) ) {
						memcpy(conn->receiveMessage + conn->receiveMessageLength, data, length);
						conn->receiveMessageLength += length;
					} else {
						Con_Printf( "Reliable message (seq: %i) too big for message buffer!\n"
									"Dropping the message!\n", sequence );
						conn->receiveMessageLength = 0;
						return 1;
					}
					if (flags & NETFLAG_EOM)
					{
						reliableMessagesReceived++;
						length = conn->receiveMessageLength;
						conn->receiveMessageLength = 0;
						if (length > 0)
						{
							SZ_Clear(&net_message);
							SZ_Write(&net_message, conn->receiveMessage, length);
							MSG_BeginReading();
							return 2;
						}
					}
				}
				else
					receivedDuplicateCount++;
				return 1;
			}
		}
	}
	return 0;
}

void NetConn_ConnectionEstablished(lhnetsocket_t *mysocket, lhnetaddress_t *peeraddress, protocolversion_t initialprotocol)
{
	cls.connect_trying = false;
	M_Update_Return_Reason("");
	// the connection request succeeded, stop current connection and set up a new connection
	CL_Disconnect();
	// if we're connecting to a remote server, shut down any local server
	if (LHNETADDRESS_GetAddressType(peeraddress) != LHNETADDRESSTYPE_LOOP && sv.active)
		Host_ShutdownServer ();
	// allocate a net connection to keep track of things
	cls.netcon = NetConn_Open(mysocket, peeraddress);
	Con_Printf("Connection accepted to %s\n", cls.netcon->address);
	key_dest = key_game;
	m_state = m_none;
	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing
	cls.protocol = initialprotocol;
	// reset move sequence numbering on this new connection
	cls.servermovesequence = 0;
	if (cls.protocol == PROTOCOL_QUAKEWORLD)
		Cmd_ForwardStringToServer("new");
	if (cls.protocol == PROTOCOL_QUAKE)
	{
		// write a keepalive (clc_nop) as it seems to greatly improve the
		// chances of connecting to a netquake server
		sizebuf_t msg;
		unsigned char buf[4];
		memset(&msg, 0, sizeof(msg));
		msg.data = buf;
		msg.maxsize = sizeof(buf);
		MSG_WriteChar(&msg, clc_nop);
		NetConn_SendUnreliableMessage(cls.netcon, &msg, cls.protocol, 10000, false);
	}
}

int NetConn_IsLocalGame(void)
{
	if (cls.state == ca_connected && sv.active && cl.maxclients == 1)
		return true;
	return false;
}

static int NetConn_ClientParsePacket_ServerList_ProcessReply(const char *addressstring)
{
	int n;
	int pingtime;
	serverlist_entry_t *entry = NULL;

	// search the cache for this server and update it
	for (n = 0;n < serverlist_cachecount;n++) {
		entry = &serverlist_cache[ n ];
		if (!strcmp(addressstring, entry->info.cname))
			break;
	}

	if (n == serverlist_cachecount)
	{
		// LAN search doesnt require an answer from the master server so we wont
		// know the ping nor will it be initialized already...

		// find a slot
		if (serverlist_cachecount == SERVERLIST_TOTALSIZE)
			return -1;

		entry = &serverlist_cache[n];

		memset(entry, 0, sizeof(*entry));
		// store the data the engine cares about (address and ping)
		strlcpy(entry->info.cname, addressstring, sizeof(entry->info.cname));
		entry->info.ping = 100000;
		entry->querytime = realtime;
		// if not in the slist menu we should print the server to console
		if (serverlist_consoleoutput)
			Con_Printf("querying %s\n", addressstring);
		++serverlist_cachecount;
	}
	// if this is the first reply from this server, count it as having replied
	pingtime = (int)((realtime - entry->querytime) * 1000.0 + 0.5);
	pingtime = bound(0, pingtime, 9999);
	if (entry->query == SQS_REFRESHING) {
		entry->info.ping = pingtime;
		entry->query = SQS_QUERIED;
	} else {
		// convert to unsigned to catch the -1
		// I still dont like this but its better than the old 10000 magic ping number - as in easier to type and read :( [11/8/2007 Black]
		entry->info.ping = min((unsigned) entry->info.ping, (unsigned) pingtime);
		serverreplycount++;
	}
	
	// other server info is updated by the caller
	return n;
}

static void NetConn_ClientParsePacket_ServerList_UpdateCache(int n)
{
	serverlist_entry_t *entry = &serverlist_cache[n];
	serverlist_info_t *info = &entry->info;
	// update description strings for engine menu and console output
	dpsnprintf(entry->line1, sizeof(serverlist_cache[n].line1), "^%c%5d^7 ^%c%3u^7/%3u %-65.65s", info->ping >= 300 ? '1' : (info->ping >= 200 ? '3' : '7'), (int)info->ping, ((info->numhumans > 0 && info->numhumans < info->maxplayers) ? (info->numhumans >= 4 ? '7' : '3') : '1'), info->numplayers, info->maxplayers, info->name);
	dpsnprintf(entry->line2, sizeof(serverlist_cache[n].line2), "^4%-21.21s %-19.19s ^%c%-17.17s^4 %-20.20s", info->cname, info->game, (info->gameversion != gameversion.integer) ? '1' : '4', info->mod, info->map);
	if (entry->query == SQS_QUERIED)
	{
		if(!serverlist_paused)
			ServerList_ViewList_Remove(entry);
	}
	// if not in the slist menu we should print the server to console (if wanted)
	else if( serverlist_consoleoutput )
		Con_Printf("%s\n%s\n", serverlist_cache[n].line1, serverlist_cache[n].line2);
	// and finally, update the view set
	if(!serverlist_paused)
		ServerList_ViewList_Insert( entry );
	//	update the entry's state
	serverlist_cache[n].query = SQS_QUERIED;
}

// returns true, if it's sensible to continue the processing
static qboolean NetConn_ClientParsePacket_ServerList_PrepareQuery( int protocol, const char *ipstring ) {
	int n;
	serverlist_entry_t *entry;

	//	ignore the rest of the message if the serverlist is full
	if( serverlist_cachecount == SERVERLIST_TOTALSIZE )
		return false;
	//	also ignore	it	if	we	have already queried	it	(other master server	response)
	for( n =	0 ; n	< serverlist_cachecount	; n++	)
		if( !strcmp( ipstring, serverlist_cache[ n ].info.cname ) )
			break;

	entry = &serverlist_cache[n];

	if( n < serverlist_cachecount ) {
		// the entry has already been queried once or 
		return true;
	}

	memset(entry, 0, sizeof(entry));
	entry->protocol =	protocol;
	//	store	the data	the engine cares about (address and	ping)
	strlcpy (entry->info.cname, ipstring, sizeof(entry->info.cname));
	
	// no, then reset the ping right away
	entry->info.ping = -1;
	// we also want to increase the serverlist_cachecount then
	serverlist_cachecount++;
	serverquerycount++;

	entry->query =	SQS_QUERYING;

	return true;
}

static int NetConn_ClientParsePacket(lhnetsocket_t *mysocket, unsigned char *data, int length, lhnetaddress_t *peeraddress)
{
	qboolean fromserver;
	int ret, c, control;
	const char *s;
	char *string, addressstring2[128], ipstring[32];
	char stringbuf[16384];

	// quakeworld ingame packet
	fromserver = cls.netcon && mysocket == cls.netcon->mysocket && !LHNETADDRESS_Compare(&cls.netcon->peeraddress, peeraddress);

	// convert the address to a string incase we need it
	LHNETADDRESS_ToString(peeraddress, addressstring2, sizeof(addressstring2), true);

	if (length >= 5 && data[0] == 255 && data[1] == 255 && data[2] == 255 && data[3] == 255)
	{
		// received a command string - strip off the packaging and put it
		// into our string buffer with NULL termination
		data += 4;
		length -= 4;
		length = min(length, (int)sizeof(stringbuf) - 1);
		memcpy(stringbuf, data, length);
		stringbuf[length] = 0;
		string = stringbuf;

		if (developer_networking.integer)
		{
			Con_Printf("NetConn_ClientParsePacket: %s sent us a command:\n", addressstring2);
			Com_HexDumpToConsole(data, length);
		}

		if (length > 10 && !memcmp(string, "challenge ", 10) && cls.connect_trying)
		{
			// darkplaces or quake3
			char protocolnames[1400];
			Protocol_Names(protocolnames, sizeof(protocolnames));
			Con_Printf("\"%s\" received, sending connect request back to %s\n", string, addressstring2);
			M_Update_Return_Reason("Got challenge response");
			// update the server IP in the userinfo (QW servers expect this, and it is used by the reconnect command)
			InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "*ip", addressstring2);
			// TODO: add userinfo stuff here instead of using NQ commands?
			NetConn_WriteString(mysocket, va("\377\377\377\377connect\\protocol\\darkplaces 3\\protocols\\%s\\challenge\\%s", protocolnames, string + 10), peeraddress);
			return true;
		}
		if (length == 6 && !memcmp(string, "accept", 6) && cls.connect_trying)
		{
			// darkplaces or quake3
			M_Update_Return_Reason("Accepted");
			NetConn_ConnectionEstablished(mysocket, peeraddress, PROTOCOL_DARKPLACES3);
			return true;
		}
		if (length > 7 && !memcmp(string, "reject ", 7) && cls.connect_trying)
		{
			char rejectreason[32];
			cls.connect_trying = false;
			string += 7;
			length = max(length - 7, (int)sizeof(rejectreason) - 1);
			memcpy(rejectreason, string, length);
			rejectreason[length] = 0;
			M_Update_Return_Reason(rejectreason);
			return true;
		}
		if (length >= 13 && !memcmp(string, "infoResponse\x0A", 13))
		{
			serverlist_info_t *info;
			int n;

			string += 13;
			// search the cache for this server and update it
			n = NetConn_ClientParsePacket_ServerList_ProcessReply(addressstring2);
			if (n < 0)
				return true;

			info = &serverlist_cache[n].info;
			info->game[0] = 0;
			info->mod[0]  = 0;
			info->map[0]  = 0;
			info->name[0] = 0;
			info->protocol = -1;
			info->numplayers = 0;
			info->numbots = -1;
			info->maxplayers  = 0;
			info->gameversion = 0;
			if ((s = SearchInfostring(string, "gamename"     )) != NULL) strlcpy(info->game, s, sizeof (info->game));
			if ((s = SearchInfostring(string, "modname"      )) != NULL) strlcpy(info->mod , s, sizeof (info->mod ));
			if ((s = SearchInfostring(string, "mapname"      )) != NULL) strlcpy(info->map , s, sizeof (info->map ));
			if ((s = SearchInfostring(string, "hostname"     )) != NULL) strlcpy(info->name, s, sizeof (info->name));
			if ((s = SearchInfostring(string, "protocol"     )) != NULL) info->protocol = atoi(s);
			if ((s = SearchInfostring(string, "clients"      )) != NULL) info->numplayers = atoi(s);
			if ((s = SearchInfostring(string, "bots"         )) != NULL) info->numbots = atoi(s);
			if ((s = SearchInfostring(string, "sv_maxclients")) != NULL) info->maxplayers = atoi(s);
			if ((s = SearchInfostring(string, "gameversion"  )) != NULL) info->gameversion = atoi(s);
			info->numhumans = info->numplayers - max(0, info->numbots);
			info->freeslots = info->maxplayers - info->numplayers;

			NetConn_ClientParsePacket_ServerList_UpdateCache(n);

			return true;
		}
		if (!strncmp(string, "getserversResponse\\", 19) && serverlist_cachecount < SERVERLIST_TOTALSIZE)
		{
			// Extract the IP addresses
			data += 18;
			length -= 18;
			masterreplycount++;
			if (serverlist_consoleoutput)
				Con_Print("received DarkPlaces server list...\n");
			while (length >= 7 && data[0] == '\\' && (data[1] != 0xFF || data[2] != 0xFF || data[3] != 0xFF || data[4] != 0xFF) && data[5] * 256 + data[6] != 0)
			{
				dpsnprintf (ipstring, sizeof (ipstring), "%u.%u.%u.%u:%u", data[1], data[2], data[3], data[4], data[5] * 256 + data[6]);
				if (serverlist_consoleoutput && developer_networking.integer)
					Con_Printf("Requesting info from DarkPlaces server %s\n", ipstring);
				
				if( !NetConn_ClientParsePacket_ServerList_PrepareQuery( PROTOCOL_DARKPLACES7, ipstring ) ) {
					break;
				}

				// move on to next address in packet
				data += 7;
				length -= 7;
			}
			// begin or resume serverlist queries
			serverlist_querysleep = false;
			serverlist_querywaittime = realtime + 3;
			return true;
		}
		if (!memcmp(string, "d\n", 2) && serverlist_cachecount < SERVERLIST_TOTALSIZE)
		{
			// Extract the IP addresses
			data += 2;
			length -= 2;
			masterreplycount++;
			if (serverlist_consoleoutput)
				Con_Printf("received QuakeWorld server list from %s...\n", addressstring2);
			while (length >= 6 && (data[0] != 0xFF || data[1] != 0xFF || data[2] != 0xFF || data[3] != 0xFF) && data[4] * 256 + data[5] != 0)
			{
				dpsnprintf (ipstring, sizeof (ipstring), "%u.%u.%u.%u:%u", data[0], data[1], data[2], data[3], data[4] * 256 + data[5]);
				if (serverlist_consoleoutput && developer_networking.integer)
					Con_Printf("Requesting info from QuakeWorld server %s\n", ipstring);
				
				if( !NetConn_ClientParsePacket_ServerList_PrepareQuery( PROTOCOL_QUAKEWORLD, ipstring ) ) {
					break;
				}

				// move on to next address in packet
				data += 6;
				length -= 6;
			}
			// begin or resume serverlist queries
			serverlist_querysleep = false;
			serverlist_querywaittime = realtime + 3;
			return true;
		}
		if (!strncmp(string, "extResponse ", 12))
		{
			++net_extresponse_count;
			if(net_extresponse_count > NET_EXTRESPONSE_MAX)
				net_extresponse_count = NET_EXTRESPONSE_MAX;
			net_extresponse_last = (net_extresponse_last + 1) % NET_EXTRESPONSE_MAX;
			dpsnprintf(net_extresponse[net_extresponse_last], sizeof(net_extresponse[net_extresponse_last]), "'%s' %s", addressstring2, string + 12);
			return true;
		}
		if (!strncmp(string, "ping", 4))
		{
			if (developer.integer >= 10)
				Con_Printf("Received ping from %s, sending ack\n", addressstring2);
			NetConn_WriteString(mysocket, "\377\377\377\377ack", peeraddress);
			return true;
		}
		if (!strncmp(string, "ack", 3))
			return true;
		// QuakeWorld compatibility
		if (length > 1 && string[0] == 'c' && (string[1] == '-' || (string[1] >= '0' && string[1] <= '9')) && cls.connect_trying)
		{
			// challenge message
			Con_Printf("challenge %s received, sending QuakeWorld connect request back to %s\n", string + 1, addressstring2);
			M_Update_Return_Reason("Got QuakeWorld challenge response");
			cls.qw_qport = qport.integer;
			// update the server IP in the userinfo (QW servers expect this, and it is used by the reconnect command)
			InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "*ip", addressstring2);
			NetConn_WriteString(mysocket, va("\377\377\377\377connect %i %i %i \"%s\"\n", 28, cls.qw_qport, atoi(string + 1), cls.userinfo), peeraddress);
			return true;
		}
		if (length >= 1 && string[0] == 'j' && cls.connect_trying)
		{
			// accept message
			M_Update_Return_Reason("QuakeWorld Accepted");
			NetConn_ConnectionEstablished(mysocket, peeraddress, PROTOCOL_QUAKEWORLD);
			return true;
		}
		if (length > 2 && !memcmp(string, "n\\", 2))
		{
			serverlist_info_t *info;
			int n;

			// qw server status
			if (serverlist_consoleoutput && developer_networking.integer >= 2)
				Con_Printf("QW server status from server at %s:\n%s\n", addressstring2, string + 1);

			string += 1;
			// search the cache for this server and update it
			n = NetConn_ClientParsePacket_ServerList_ProcessReply(addressstring2);
			if (n < 0)
				return true;

			info = &serverlist_cache[n].info;
			strlcpy(info->game, "QuakeWorld", sizeof(info->game));;
			if ((s = SearchInfostring(string, "*gamedir"     )) != NULL) strlcpy(info->mod , s, sizeof (info->mod ));else info->mod[0]  = 0;
			if ((s = SearchInfostring(string, "map"          )) != NULL) strlcpy(info->map , s, sizeof (info->map ));else info->map[0]  = 0;
			if ((s = SearchInfostring(string, "hostname"     )) != NULL) strlcpy(info->name, s, sizeof (info->name));else info->name[0] = 0;
			info->protocol = 0;
			info->numplayers = 0; // updated below
			info->numhumans = 0; // updated below
			if ((s = SearchInfostring(string, "maxclients"   )) != NULL) info->maxplayers = atoi(s);else info->maxplayers  = 0;
			if ((s = SearchInfostring(string, "gameversion"  )) != NULL) info->gameversion = atoi(s);else info->gameversion = 0;

			// count active players on server
			// (we could gather more info, but we're just after the number)
			s = strchr(string, '\n');
			if (s)
			{
				s++;
				while (s < string + length)
				{
					for (;s < string + length && *s != '\n';s++)
						;
					if (s >= string + length)
						break;
					info->numplayers++;
					info->numhumans++;
					s++;
				}
			}

			NetConn_ClientParsePacket_ServerList_UpdateCache(n);

			return true;
		}
		if (string[0] == 'n')
		{
			// qw print command
			Con_Printf("QW print command from server at %s:\n%s\n", addressstring2, string + 1);
		}
		// we may not have liked the packet, but it was a command packet, so
		// we're done processing this packet now
		return true;
	}
	// quakeworld ingame packet
	if (fromserver && cls.protocol == PROTOCOL_QUAKEWORLD && length >= 8 && (ret = NetConn_ReceivedMessage(cls.netcon, data, length, cls.protocol, net_messagetimeout.value)) == 2)
	{
		ret = 0;
		CL_ParseServerMessage();
		return ret;
	}
	// netquake control packets, supported for compatibility only
	if (length >= 5 && (control = BigLong(*((int *)data))) && (control & (~NETFLAG_LENGTH_MASK)) == (int)NETFLAG_CTL && (control & NETFLAG_LENGTH_MASK) == length)
	{
		int n;
		serverlist_info_t *info;

		data += 4;
		length -= 4;
		SZ_Clear(&net_message);
		SZ_Write(&net_message, data, length);
		MSG_BeginReading();
		c = MSG_ReadByte();
		switch (c)
		{
		case CCREP_ACCEPT:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREP_ACCEPT from %s.\n", addressstring2);
			if (cls.connect_trying)
			{
				lhnetaddress_t clientportaddress;
				clientportaddress = *peeraddress;
				LHNETADDRESS_SetPort(&clientportaddress, MSG_ReadLong());
				// update the server IP in the userinfo (QW servers expect this, and it is used by the reconnect command)
				InfoString_SetValue(cls.userinfo, sizeof(cls.userinfo), "*ip", addressstring2);
				M_Update_Return_Reason("Accepted");
				NetConn_ConnectionEstablished(mysocket, &clientportaddress, PROTOCOL_QUAKE);
			}
			break;
		case CCREP_REJECT:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREP_REJECT from %s.\n", addressstring2);
			cls.connect_trying = false;
			M_Update_Return_Reason((char *)MSG_ReadString());
			break;
		case CCREP_SERVER_INFO:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREP_SERVER_INFO from %s.\n", addressstring2);
			// LordHavoc: because the quake server may report weird addresses
			// we just ignore it and keep the real address
			MSG_ReadString();
			// search the cache for this server and update it
			n = NetConn_ClientParsePacket_ServerList_ProcessReply(addressstring2);
			if (n < 0)
				break;

			info = &serverlist_cache[n].info;
			strlcpy(info->game, "Quake", sizeof(info->game));
			strlcpy(info->mod , "", sizeof(info->mod)); // mod name is not specified
			strlcpy(info->name, MSG_ReadString(), sizeof(info->name));
			strlcpy(info->map , MSG_ReadString(), sizeof(info->map));
			info->numplayers = MSG_ReadByte();
			info->maxplayers = MSG_ReadByte();
			info->protocol = MSG_ReadByte();

			NetConn_ClientParsePacket_ServerList_UpdateCache(n);

			break;
		case CCREP_PLAYER_INFO:
			// we got a CCREP_PLAYER_INFO??
			//if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREP_PLAYER_INFO from %s.\n", addressstring2);
			break;
		case CCREP_RULE_INFO:
			// we got a CCREP_RULE_INFO??
			//if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREP_RULE_INFO from %s.\n", addressstring2);
			break;
		default:
			break;
		}
		SZ_Clear(&net_message);
		// we may not have liked the packet, but it was a valid control
		// packet, so we're done processing this packet now
		return true;
	}
	ret = 0;
	if (fromserver && length >= (int)NET_HEADERSIZE && (ret = NetConn_ReceivedMessage(cls.netcon, data, length, cls.protocol, net_messagetimeout.value)) == 2)
		CL_ParseServerMessage();
	return ret;
}

void NetConn_QueryQueueFrame(void)
{
	int index;
	int queries;
	int maxqueries;
	double timeouttime;
	static double querycounter = 0;

	if(!net_slist_pause.integer && serverlist_paused)
		ServerList_RebuildViewList();
	serverlist_paused = net_slist_pause.integer;

	if (serverlist_querysleep)
		return;

	// apply a cool down time after master server replies,
	// to avoid messing up the ping times on the servers
	if (serverlist_querywaittime > realtime)
		return;

	// each time querycounter reaches 1.0 issue a query
	querycounter += cl.realframetime * net_slist_queriespersecond.value;
	maxqueries = (int)querycounter;
	maxqueries = bound(0, maxqueries, net_slist_queriesperframe.integer);
	querycounter -= maxqueries;

	if( maxqueries == 0 ) {
		return;
	}

	//	scan serverlist and issue queries as needed
	serverlist_querysleep = true;

	timeouttime	= realtime - net_slist_timeout.value;
	for( index = 0, queries	= 0 ;	index	< serverlist_cachecount	&&	queries < maxqueries	; index++ )
	{
		serverlist_entry_t *entry = &serverlist_cache[ index ];
		if( entry->query != SQS_QUERYING && entry->query != SQS_REFRESHING )
		{
			continue;
		}

		serverlist_querysleep	= false;
		if( entry->querycounter	!=	0 && entry->querytime >	timeouttime	)
		{
			continue;
		}

		if( entry->querycounter	!=	(unsigned) net_slist_maxtries.integer )
		{
			lhnetaddress_t	address;
			int socket;

			LHNETADDRESS_FromString(&address, entry->info.cname, 0);
			if	(entry->protocol == PROTOCOL_QUAKEWORLD)
			{
				for (socket	= 0; socket	< cl_numsockets ;	socket++)
					NetConn_WriteString(cl_sockets[socket], "\377\377\377\377status\n", &address);
			}
			else
			{
				for (socket	= 0; socket	< cl_numsockets ;	socket++)
					NetConn_WriteString(cl_sockets[socket], "\377\377\377\377getinfo", &address);
			}

			//	update the entry fields
			entry->querytime = realtime;
			entry->querycounter++;

			// if not in the slist menu we should print the server to console
			if (serverlist_consoleoutput)
				Con_Printf("querying %25s (%i. try)\n", entry->info.cname, entry->querycounter);

			queries++;
		}
		else
		{
			// have we tried to refresh this server?
			if( entry->query == SQS_REFRESHING ) {
				// yes, so update the reply count (since its not responding anymore)
				serverreplycount--;
				if(!serverlist_paused)
					ServerList_ViewList_Remove(entry);
			}
			entry->query = SQS_TIMEDOUT;
		}
	}
}

void NetConn_ClientFrame(void)
{
	int i, length;
	lhnetaddress_t peeraddress;
	NetConn_UpdateSockets();
	if (cls.connect_trying && cls.connect_nextsendtime < realtime)
	{
		if (cls.connect_remainingtries == 0)
			M_Update_Return_Reason("Connect: Waiting 10 seconds for reply");
		cls.connect_nextsendtime = realtime + 1;
		cls.connect_remainingtries--;
		if (cls.connect_remainingtries <= -10)
		{
			cls.connect_trying = false;
			M_Update_Return_Reason("Connect: Failed");
			return;
		}
		// try challenge first (newer DP server or QW)
		NetConn_WriteString(cls.connect_mysocket, "\377\377\377\377getchallenge", &cls.connect_address);
		// then try netquake as a fallback (old server, or netquake)
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CCREQ_CONNECT);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
		*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
		NetConn_Write(cls.connect_mysocket, net_message.data, net_message.cursize, &cls.connect_address);
		SZ_Clear(&net_message);
	}
	for (i = 0;i < cl_numsockets;i++)
		while (cl_sockets[i] && (length = NetConn_Read(cl_sockets[i], readbuffer, sizeof(readbuffer), &peeraddress)) > 0)
			NetConn_ClientParsePacket(cl_sockets[i], readbuffer, length, &peeraddress);
	NetConn_QueryQueueFrame();
	if (cls.netcon && realtime > cls.netcon->timeout && !sv.active)
	{
		Con_Print("Connection timed out\n");
		CL_Disconnect();
		Host_ShutdownServer ();
	}
}

#define MAX_CHALLENGES 128
struct challenge_s
{
	lhnetaddress_t address;
	double time;
	char string[12];
}
challenge[MAX_CHALLENGES];

static void NetConn_BuildChallengeString(char *buffer, int bufferlength)
{
	int i;
	char c;
	for (i = 0;i < bufferlength - 1;i++)
	{
		do
		{
			c = rand () % (127 - 33) + 33;
		} while (c == '\\' || c == ';' || c == '"' || c == '%' || c == '/');
		buffer[i] = c;
	}
	buffer[i] = 0;
}

static qboolean NetConn_BuildStatusResponse(const char* challenge, char* out_msg, size_t out_size, qboolean fullstatus)
{
	unsigned int nb_clients = 0, nb_bots = 0, i;
	int length;

	// How many clients are there?
	for (i = 0;i < (unsigned int)svs.maxclients;i++)
	{
		if (svs.clients[i].active)
		{
			nb_clients++;
			if (!svs.clients[i].netconnection)
				nb_bots++;
		}
	}

	// TODO: we should add more information for the full status string
	length = dpsnprintf(out_msg, out_size,
						"\377\377\377\377%s\x0A"
						"\\gamename\\%s\\modname\\%s\\gameversion\\%d\\sv_maxclients\\%d"
						"\\clients\\%d\\bots\\%d\\mapname\\%s\\hostname\\%s\\protocol\\%d"
						"%s%s"
						"%s",
						fullstatus ? "statusResponse" : "infoResponse",
						gamename, com_modname, gameversion.integer, svs.maxclients,
						nb_clients, nb_bots, sv.name, hostname.string, NET_PROTOCOL_VERSION,
						challenge ? "\\challenge\\" : "", challenge ? challenge : "",
						fullstatus ? "\n" : "");

	// Make sure it fits in the buffer
	if (length < 0)
		return false;

	if (fullstatus)
	{
		char *ptr;
		int left;

		ptr = out_msg + length;
		left = (int)out_size - length;

		for (i = 0;i < (unsigned int)svs.maxclients;i++)
		{
			client_t *cl = &svs.clients[i];
			if (cl->active)
			{
				int nameind, cleanind, pingvalue;
				char curchar;
				char cleanname [sizeof(cl->name)];

				// Remove all characters '"' and '\' in the player name
				nameind = 0;
				cleanind = 0;
				do
				{
					curchar = cl->name[nameind++];
					if (curchar != '"' && curchar != '\\')
					{
						cleanname[cleanind++] = curchar;
						if (cleanind == sizeof(cleanname) - 1)
							break;
					}
				} while (curchar != '\0');

				pingvalue = (int)(cl->ping * 1000.0f);
				if(cl->netconnection)
					pingvalue = bound(1, pingvalue, 9999);
				else
					pingvalue = 0;
				length = dpsnprintf(ptr, left, "%d %d \"%s\"\n",
									cl->frags,
									pingvalue,
									cleanname);
				if(length < 0)
					return false;
				left -= length;
				ptr += length;
			}
		}
	}

	return true;
}

static qboolean NetConn_PreventConnectFlood(lhnetaddress_t *peeraddress)
{
	int floodslotnum, bestfloodslotnum;
	double bestfloodtime;
	lhnetaddress_t noportpeeraddress;
	// see if this is a connect flood
	noportpeeraddress = *peeraddress;
	LHNETADDRESS_SetPort(&noportpeeraddress, 0);
	bestfloodslotnum = 0;
	bestfloodtime = sv.connectfloodaddresses[bestfloodslotnum].lasttime;
	for (floodslotnum = 0;floodslotnum < MAX_CONNECTFLOODADDRESSES;floodslotnum++)
	{
		if (bestfloodtime >= sv.connectfloodaddresses[floodslotnum].lasttime)
		{
			bestfloodtime = sv.connectfloodaddresses[floodslotnum].lasttime;
			bestfloodslotnum = floodslotnum;
		}
		if (sv.connectfloodaddresses[floodslotnum].lasttime && LHNETADDRESS_Compare(&noportpeeraddress, &sv.connectfloodaddresses[floodslotnum].address) == 0)
		{
			// this address matches an ongoing flood address
			if (realtime < sv.connectfloodaddresses[floodslotnum].lasttime + net_connectfloodblockingtimeout.value)
			{
				// renew the ban on this address so it does not expire
				// until the flood has subsided
				sv.connectfloodaddresses[floodslotnum].lasttime = realtime;
				//Con_Printf("Flood detected!\n");
				return true;
			}
			// the flood appears to have subsided, so allow this
			bestfloodslotnum = floodslotnum; // reuse the same slot
			break;
		}
	}
	// begin a new timeout on this address
	sv.connectfloodaddresses[bestfloodslotnum].address = noportpeeraddress;
	sv.connectfloodaddresses[bestfloodslotnum].lasttime = realtime;
	//Con_Printf("Flood detection initiated!\n");
	return false;
}

void NetConn_ClearConnectFlood(lhnetaddress_t *peeraddress)
{
	int floodslotnum;
	lhnetaddress_t noportpeeraddress;
	// see if this is a connect flood
	noportpeeraddress = *peeraddress;
	LHNETADDRESS_SetPort(&noportpeeraddress, 0);
	for (floodslotnum = 0;floodslotnum < MAX_CONNECTFLOODADDRESSES;floodslotnum++)
	{
		if (sv.connectfloodaddresses[floodslotnum].lasttime && LHNETADDRESS_Compare(&noportpeeraddress, &sv.connectfloodaddresses[floodslotnum].address) == 0)
		{
			// this address matches an ongoing flood address
			// remove the ban
			sv.connectfloodaddresses[floodslotnum].address.addresstype = LHNETADDRESSTYPE_NONE;
			sv.connectfloodaddresses[floodslotnum].lasttime = 0;
			//Con_Printf("Flood cleared!\n");
		}
	}
}

// returns a string describing the user level, or NULL for auth failure
const char *RCon_Authenticate(const char *password, const char *s, const char *endpos)
{
	const char *text;
	qboolean hasquotes;

	if(!strcmp(rcon_password.string, password))
		return "rcon";
	
	if(strcmp(rcon_restricted_password.string, password))
		return NULL;

	for(text = s; text != endpos; ++text)
		if(*text > 0 && (*text < ' ' || *text == ';'))
			return NULL; // block possible exploits against the parser/alias expansion

	while(s != endpos)
	{
		size_t l = strlen(s);
		if(l)
		{
			hasquotes = (strchr(s, '"') != NULL);
			// sorry, we can't allow these substrings in wildcard expressions,
			// as they can mess with the argument counts
			text = rcon_restricted_commands.string;
			while(COM_ParseToken_Console(&text))
			{
				// com_token now contains a pattern to check for...
				if(strchr(com_token, '*') || strchr(com_token, '?')) // wildcard expression, * can only match a SINGLE argument
				{
					if(!hasquotes)
						if(matchpattern_with_separator(s, com_token, true, " ", true)) // note how we excluded tab, newline etc. above
							goto match;
				}
				else if(strchr(com_token, ' ')) // multi-arg expression? must match in whole
				{
					if(!strcmp(com_token, s))
						goto match;
				}
				else // single-arg expression? must match the beginning of the command
				{
					if(!strcmp(com_token, s))
						goto match;
					if(!memcmp(va("%s ", com_token), s, strlen(com_token) + 1))
						goto match;
				}
			}
			// if we got here, nothing matched!
			return NULL;
		}
match:
		s += l + 1;
	}

	return "restricted rcon";
}

extern void SV_SendServerinfo (client_t *client);
static int NetConn_ServerParsePacket(lhnetsocket_t *mysocket, unsigned char *data, int length, lhnetaddress_t *peeraddress)
{
	int i, ret, clientnum, best;
	double besttime;
	client_t *client;
	char *s, *string, response[1400], addressstring2[128], stringbuf[16384];
	qboolean islocal = (LHNETADDRESS_GetAddressType(peeraddress) == LHNETADDRESSTYPE_LOOP);

	if (!sv.active)
		return false;

	// convert the address to a string incase we need it
	LHNETADDRESS_ToString(peeraddress, addressstring2, sizeof(addressstring2), true);

	// see if we can identify the sender as a local player
	// (this is necessary for rcon to send a reliable reply if the client is
	//  actually on the server, not sending remotely)
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
		if (host_client->netconnection && host_client->netconnection->mysocket == mysocket && !LHNETADDRESS_Compare(&host_client->netconnection->peeraddress, peeraddress))
			break;
	if (i == svs.maxclients)
		host_client = NULL;

	if (length >= 5 && data[0] == 255 && data[1] == 255 && data[2] == 255 && data[3] == 255)
	{
		// received a command string - strip off the packaging and put it
		// into our string buffer with NULL termination
		data += 4;
		length -= 4;
		length = min(length, (int)sizeof(stringbuf) - 1);
		memcpy(stringbuf, data, length);
		stringbuf[length] = 0;
		string = stringbuf;

		if (developer.integer >= 10)
		{
			Con_Printf("NetConn_ServerParsePacket: %s sent us a command:\n", addressstring2);
			Com_HexDumpToConsole(data, length);
		}

		if (length >= 12 && !memcmp(string, "getchallenge", 12) && (islocal || sv_public.integer > -2))
		{
			for (i = 0, best = 0, besttime = realtime;i < MAX_CHALLENGES;i++)
			{
				if (!LHNETADDRESS_Compare(peeraddress, &challenge[i].address))
					break;
				if (besttime > challenge[i].time)
					besttime = challenge[best = i].time;
			}
			// if we did not find an exact match, choose the oldest and
			// update address and string
			if (i == MAX_CHALLENGES)
			{
				i = best;
				challenge[i].address = *peeraddress;
				NetConn_BuildChallengeString(challenge[i].string, sizeof(challenge[i].string));
			}
			challenge[i].time = realtime;
			// send the challenge
			NetConn_WriteString(mysocket, va("\377\377\377\377challenge %s", challenge[i].string), peeraddress);
			return true;
		}
		if (length > 8 && !memcmp(string, "connect\\", 8) && (islocal || sv_public.integer > -2))
		{
			string += 7;
			length -= 7;

			if (!(s = SearchInfostring(string, "challenge")))
				return true;
			// validate the challenge
			for (i = 0;i < MAX_CHALLENGES;i++)
				if (!LHNETADDRESS_Compare(peeraddress, &challenge[i].address) && !strcmp(challenge[i].string, s))
					break;
			// if the challenge is not recognized, drop the packet
			if (i == MAX_CHALLENGES)
				return true;

			// check engine protocol
			if (strcmp(SearchInfostring(string, "protocol"), "darkplaces 3"))
			{
				if (developer.integer >= 10)
					Con_Printf("Datagram_ParseConnectionless: sending \"reject Wrong game protocol.\" to %s.\n", addressstring2);
				NetConn_WriteString(mysocket, "\377\377\377\377reject Wrong game protocol.", peeraddress);
				return true;
			}

			// see if this is a duplicate connection request or a disconnected
			// client who is rejoining to the same client slot
			for (clientnum = 0, client = svs.clients;clientnum < svs.maxclients;clientnum++, client++)
			{
				if (client->netconnection && LHNETADDRESS_Compare(peeraddress, &client->netconnection->peeraddress) == 0)
				{
					// this is a known client...
					if (client->spawned)
					{
						// client crashed and is coming back,
						// keep their stuff intact
						if (developer.integer >= 10)
							Con_Printf("Datagram_ParseConnectionless: sending \"accept\" to %s.\n", addressstring2);
						NetConn_WriteString(mysocket, "\377\377\377\377accept", peeraddress);
						SV_VM_Begin();
						SV_SendServerinfo(client);
						SV_VM_End();
					}
					else
					{
						// client is still trying to connect,
						// so we send a duplicate reply
						if (developer.integer >= 10)
							Con_Printf("Datagram_ParseConnectionless: sending duplicate accept to %s.\n", addressstring2);
						NetConn_WriteString(mysocket, "\377\377\377\377accept", peeraddress);
					}
					return true;
				}
			}

			if (NetConn_PreventConnectFlood(peeraddress))
				return true;

			// find an empty client slot for this new client
			for (clientnum = 0, client = svs.clients;clientnum < svs.maxclients;clientnum++, client++)
			{
				netconn_t *conn;
				if (!client->active && (conn = NetConn_Open(mysocket, peeraddress)))
				{
					// allocated connection
					if (developer.integer >= 10)
						Con_Printf("Datagram_ParseConnectionless: sending \"accept\" to %s.\n", conn->address);
					NetConn_WriteString(mysocket, "\377\377\377\377accept", peeraddress);
					// now set up the client
					SV_VM_Begin();
					SV_ConnectClient(clientnum, conn);
					SV_VM_End();
					NetConn_Heartbeat(1);
					return true;
				}
			}

			// no empty slots found - server is full
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: sending \"reject Server is full.\" to %s.\n", addressstring2);
			NetConn_WriteString(mysocket, "\377\377\377\377reject Server is full.", peeraddress);

			return true;
		}
		if (length >= 7 && !memcmp(string, "getinfo", 7) && (islocal || sv_public.integer > -1))
		{
			const char *challenge = NULL;

			// If there was a challenge in the getinfo message
			if (length > 8 && string[7] == ' ')
				challenge = string + 8;

			if (NetConn_BuildStatusResponse(challenge, response, sizeof(response), false))
			{
				if (developer.integer >= 10)
					Con_Printf("Sending reply to master %s - %s\n", addressstring2, response);
				NetConn_WriteString(mysocket, response, peeraddress);
			}
			return true;
		}
		if (length >= 9 && !memcmp(string, "getstatus", 9) && (islocal || sv_public.integer > -1))
		{
			const char *challenge = NULL;

			// If there was a challenge in the getinfo message
			if (length > 10 && string[9] == ' ')
				challenge = string + 10;

			if (NetConn_BuildStatusResponse(challenge, response, sizeof(response), true))
			{
				if (developer.integer >= 10)
					Con_Printf("Sending reply to client %s - %s\n", addressstring2, response);
				NetConn_WriteString(mysocket, response, peeraddress);
			}
			return true;
		}
		if (length >= 5 && !memcmp(string, "rcon ", 5))
		{
			int i;
			char *s = string + 5;
			char *endpos = string + length + 1; // one behind the NUL, so adding strlen+1 will eventually reach it
			char password[64];
			for (i = 0;*s > ' ';s++)
				if (i < (int)sizeof(password) - 1)
					password[i++] = *s;
			if(*s <= ' ' && s != endpos) // skip leading ugly space
				++s;
			password[i] = 0;
			if (password[0] > ' ')
			{
				const char *userlevel = RCon_Authenticate(password, s, endpos);
				if(userlevel)
				{
					// looks like a legitimate rcon command with the correct password
					char *s_ptr = s;
					Con_Printf("server received %s command from %s: ", userlevel, host_client ? host_client->name : addressstring2);
					while(s_ptr != endpos)
					{
						size_t l = strlen(s_ptr);
						if(l)
							Con_Printf(" %s;", s_ptr);
						s_ptr += l + 1;
					}
					Con_Printf("\n");
					rcon_redirect = true;
					rcon_redirect_bufferpos = 0;
					while(s != endpos)
					{
						size_t l = strlen(s);
						if(l)
							Cmd_ExecuteString(s, src_command);
						s += l + 1;
					}
					rcon_redirect_buffer[rcon_redirect_bufferpos] = 0;
					rcon_redirect = false;
					// print resulting text to client
					// if client is playing, send a reliable reply instead of
					// a command packet
					if (host_client)
					{
						// if the netconnection is loop, then this is the
						// local player on a listen mode server, and it would
						// result in duplicate printing to the console
						// (not that the local player should be using rcon
						//  when they have the console)
						if (host_client->netconnection && LHNETADDRESS_GetAddressType(&host_client->netconnection->peeraddress) != LHNETADDRESSTYPE_LOOP)
							SV_ClientPrintf("%s", rcon_redirect_buffer);
					}
					else
					{
						// qw print command
						dpsnprintf(response, sizeof(response), "\377\377\377\377n%s", rcon_redirect_buffer);
						NetConn_WriteString(mysocket, response, peeraddress);
					}
				}
				else
				{
					Con_Printf("server denied rcon access to %s\n", host_client ? host_client->name : addressstring2);
				}
			}
			return true;
		}
		if (!strncmp(string, "ping", 4))
		{
			if (developer.integer >= 10)
				Con_Printf("Received ping from %s, sending ack\n", addressstring2);
			NetConn_WriteString(mysocket, "\377\377\377\377ack", peeraddress);
			return true;
		}
		if (!strncmp(string, "ack", 3))
			return true;
		// we may not have liked the packet, but it was a command packet, so
		// we're done processing this packet now
		return true;
	}
	// netquake control packets, supported for compatibility only, and only
	// when running game protocols that are normally served via this connection
	// protocol
	// (this protects more modern protocols against being used for
	//  Quake packet flood Denial Of Service attacks)
	if (length >= 5 && (i = BigLong(*((int *)data))) && (i & (~NETFLAG_LENGTH_MASK)) == (int)NETFLAG_CTL && (i & NETFLAG_LENGTH_MASK) == length && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3))
	{
		int c;
		int protocolnumber;
		const char *protocolname;
		data += 4;
		length -= 4;
		SZ_Clear(&net_message);
		SZ_Write(&net_message, data, length);
		MSG_BeginReading();
		c = MSG_ReadByte();
		switch (c)
		{
		case CCREQ_CONNECT:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREQ_CONNECT from %s.\n", addressstring2);
			if(!islocal && sv_public.integer <= -2)
				break;

			protocolname = MSG_ReadString();
			protocolnumber = MSG_ReadByte();
			if (strcmp(protocolname, "QUAKE") || protocolnumber != NET_PROTOCOL_VERSION)
			{
				if (developer.integer >= 10)
					Con_Printf("Datagram_ParseConnectionless: sending CCREP_REJECT \"Incompatible version.\" to %s.\n", addressstring2);
				SZ_Clear(&net_message);
				// save space for the header, filled in later
				MSG_WriteLong(&net_message, 0);
				MSG_WriteByte(&net_message, CCREP_REJECT);
				MSG_WriteString(&net_message, "Incompatible version.\n");
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
				SZ_Clear(&net_message);
				break;
			}

			// see if this connect request comes from a known client
			for (clientnum = 0, client = svs.clients;clientnum < svs.maxclients;clientnum++, client++)
			{
				if (client->netconnection && LHNETADDRESS_Compare(peeraddress, &client->netconnection->peeraddress) == 0)
				{
					// this is either a duplicate connection request
					// or coming back from a timeout
					// (if so, keep their stuff intact)

					// send a reply
					if (developer.integer >= 10)
						Con_Printf("Datagram_ParseConnectionless: sending duplicate CCREP_ACCEPT to %s.\n", addressstring2);
					SZ_Clear(&net_message);
					// save space for the header, filled in later
					MSG_WriteLong(&net_message, 0);
					MSG_WriteByte(&net_message, CCREP_ACCEPT);
					MSG_WriteLong(&net_message, LHNETADDRESS_GetPort(LHNET_AddressFromSocket(client->netconnection->mysocket)));
					*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
					NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
					SZ_Clear(&net_message);

					// if client is already spawned, re-send the
					// serverinfo message as they'll need it to play
					if (client->spawned)
					{
						SV_VM_Begin();
						SV_SendServerinfo(client);
						SV_VM_End();
					}
					return true;
				}
			}

			// this is a new client, check for connection flood
			if (NetConn_PreventConnectFlood(peeraddress))
				break;

			// find a slot for the new client
			for (clientnum = 0, client = svs.clients;clientnum < svs.maxclients;clientnum++, client++)
			{
				netconn_t *conn;
				if (!client->active && (client->netconnection = conn = NetConn_Open(mysocket, peeraddress)) != NULL)
				{
					// connect to the client
					// everything is allocated, just fill in the details
					strlcpy (conn->address, addressstring2, sizeof (conn->address));
					if (developer.integer >= 10)
						Con_Printf("Datagram_ParseConnectionless: sending CCREP_ACCEPT to %s.\n", addressstring2);
					// send back the info about the server connection
					SZ_Clear(&net_message);
					// save space for the header, filled in later
					MSG_WriteLong(&net_message, 0);
					MSG_WriteByte(&net_message, CCREP_ACCEPT);
					MSG_WriteLong(&net_message, LHNETADDRESS_GetPort(LHNET_AddressFromSocket(conn->mysocket)));
					*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
					NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
					SZ_Clear(&net_message);
					// now set up the client struct
					SV_VM_Begin();
					SV_ConnectClient(clientnum, conn);
					SV_VM_End();
					NetConn_Heartbeat(1);
					return true;
				}
			}

			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: sending CCREP_REJECT \"Server is full.\" to %s.\n", addressstring2);
			// no room; try to let player know
			SZ_Clear(&net_message);
			// save space for the header, filled in later
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CCREP_REJECT);
			MSG_WriteString(&net_message, "Server is full.\n");
			*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
			NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
			SZ_Clear(&net_message);
			break;
		case CCREQ_SERVER_INFO:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREQ_SERVER_INFO from %s.\n", addressstring2);
			if(!islocal && sv_public.integer <= -1)
				break;
			if (sv.active && !strcmp(MSG_ReadString(), "QUAKE"))
			{
				int numclients;
				char myaddressstring[128];
				if (developer.integer >= 10)
					Con_Printf("Datagram_ParseConnectionless: sending CCREP_SERVER_INFO to %s.\n", addressstring2);
				SZ_Clear(&net_message);
				// save space for the header, filled in later
				MSG_WriteLong(&net_message, 0);
				MSG_WriteByte(&net_message, CCREP_SERVER_INFO);
				LHNETADDRESS_ToString(LHNET_AddressFromSocket(mysocket), myaddressstring, sizeof(myaddressstring), true);
				MSG_WriteString(&net_message, myaddressstring);
				MSG_WriteString(&net_message, hostname.string);
				MSG_WriteString(&net_message, sv.name);
				// How many clients are there?
				for (i = 0, numclients = 0;i < svs.maxclients;i++)
					if (svs.clients[i].active)
						numclients++;
				MSG_WriteByte(&net_message, numclients);
				MSG_WriteByte(&net_message, svs.maxclients);
				MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
				SZ_Clear(&net_message);
			}
			break;
		case CCREQ_PLAYER_INFO:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREQ_PLAYER_INFO from %s.\n", addressstring2);
			if(!islocal && sv_public.integer <= -1)
				break;
			if (sv.active)
			{
				int playerNumber, activeNumber, clientNumber;
				client_t *client;

				playerNumber = MSG_ReadByte();
				activeNumber = -1;
				for (clientNumber = 0, client = svs.clients; clientNumber < svs.maxclients; clientNumber++, client++)
					if (client->active && ++activeNumber == playerNumber)
						break;
				if (clientNumber != svs.maxclients)
				{
					SZ_Clear(&net_message);
					// save space for the header, filled in later
					MSG_WriteLong(&net_message, 0);
					MSG_WriteByte(&net_message, CCREP_PLAYER_INFO);
					MSG_WriteByte(&net_message, playerNumber);
					MSG_WriteString(&net_message, client->name);
					MSG_WriteLong(&net_message, client->colors);
					MSG_WriteLong(&net_message, client->frags);
					MSG_WriteLong(&net_message, (int)(realtime - client->connecttime));
					MSG_WriteString(&net_message, client->netconnection ? client->netconnection->address : "botclient");
					*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
					NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
					SZ_Clear(&net_message);
				}
			}
			break;
		case CCREQ_RULE_INFO:
			if (developer.integer >= 10)
				Con_Printf("Datagram_ParseConnectionless: received CCREQ_RULE_INFO from %s.\n", addressstring2);
			if(!islocal && sv_public.integer <= -1)
				break;
			if (sv.active)
			{
				char *prevCvarName;
				cvar_t *var;

				// find the search start location
				prevCvarName = MSG_ReadString();
				var = Cvar_FindVarAfter(prevCvarName, CVAR_NOTIFY);

				// send the response
				SZ_Clear(&net_message);
				// save space for the header, filled in later
				MSG_WriteLong(&net_message, 0);
				MSG_WriteByte(&net_message, CCREP_RULE_INFO);
				if (var)
				{
					MSG_WriteString(&net_message, var->name);
					MSG_WriteString(&net_message, var->string);
				}
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				NetConn_Write(mysocket, net_message.data, net_message.cursize, peeraddress);
				SZ_Clear(&net_message);
			}
			break;
		default:
			break;
		}
		SZ_Clear(&net_message);
		// we may not have liked the packet, but it was a valid control
		// packet, so we're done processing this packet now
		return true;
	}
	if (host_client)
	{
		if ((ret = NetConn_ReceivedMessage(host_client->netconnection, data, length, sv.protocol, host_client->spawned ? net_messagetimeout.value : net_connecttimeout.value)) == 2)
		{
			SV_VM_Begin();
			SV_ReadClientMessage();
			SV_VM_End();
			return ret;
		}
	}
	return 0;
}

void NetConn_ServerFrame(void)
{
	int i, length;
	lhnetaddress_t peeraddress;
	for (i = 0;i < sv_numsockets;i++)
		while (sv_sockets[i] && (length = NetConn_Read(sv_sockets[i], readbuffer, sizeof(readbuffer), &peeraddress)) > 0)
			NetConn_ServerParsePacket(sv_sockets[i], readbuffer, length, &peeraddress);
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		// never timeout loopback connections
		if (host_client->netconnection && realtime > host_client->netconnection->timeout && LHNETADDRESS_GetAddressType(&host_client->netconnection->peeraddress) != LHNETADDRESSTYPE_LOOP)
		{
			Con_Printf("Client \"%s\" connection timed out\n", host_client->name);
			SV_VM_Begin();
			SV_DropClient(false);
			SV_VM_End();
		}
	}
}

void NetConn_SleepMicroseconds(int microseconds)
{
	LHNET_SleepUntilPacket_Microseconds(microseconds);
}

void NetConn_QueryMasters(qboolean querydp, qboolean queryqw)
{
	int i;
	int masternum;
	lhnetaddress_t masteraddress;
	lhnetaddress_t broadcastaddress;
	char request[256];

	if (serverlist_cachecount >= SERVERLIST_TOTALSIZE)
		return;

	// 26000 is the default quake server port, servers on other ports will not
	// be found
	// note this is IPv4-only, I doubt there are IPv6-only LANs out there
	LHNETADDRESS_FromString(&broadcastaddress, "255.255.255.255", 26000);

	if (querydp)
	{
		for (i = 0;i < cl_numsockets;i++)
		{
			if (cl_sockets[i])
			{
				// search LAN for Quake servers
				SZ_Clear(&net_message);
				// save space for the header, filled in later
				MSG_WriteLong(&net_message, 0);
				MSG_WriteByte(&net_message, CCREQ_SERVER_INFO);
				MSG_WriteString(&net_message, "QUAKE");
				MSG_WriteByte(&net_message, NET_PROTOCOL_VERSION);
				*((int *)net_message.data) = BigLong(NETFLAG_CTL | (net_message.cursize & NETFLAG_LENGTH_MASK));
				NetConn_Write(cl_sockets[i], net_message.data, net_message.cursize, &broadcastaddress);
				SZ_Clear(&net_message);

				// search LAN for DarkPlaces servers
				NetConn_WriteString(cl_sockets[i], "\377\377\377\377getinfo", &broadcastaddress);

				// build the getservers message to send to the dpmaster master servers
				dpsnprintf(request, sizeof(request), "\377\377\377\377getservers %s %u empty full\x0A", gamename, NET_PROTOCOL_VERSION);

				// search internet
				for (masternum = 0;sv_masters[masternum].name;masternum++)
				{
					if (sv_masters[masternum].string && sv_masters[masternum].string[0] && LHNETADDRESS_FromString(&masteraddress, sv_masters[masternum].string, DPMASTER_PORT) && LHNETADDRESS_GetAddressType(&masteraddress) == LHNETADDRESS_GetAddressType(LHNET_AddressFromSocket(cl_sockets[i])))
					{
						masterquerycount++;
						NetConn_WriteString(cl_sockets[i], request, &masteraddress);
					}
				}
			}
		}
	}

	// only query QuakeWorld servers when the user wants to
	if (queryqw)
	{
		for (i = 0;i < cl_numsockets;i++)
		{
			if (cl_sockets[i])
			{
				// search LAN for QuakeWorld servers
				NetConn_WriteString(cl_sockets[i], "\377\377\377\377status\n", &broadcastaddress);

				// build the getservers message to send to the qwmaster master servers
				// note this has no -1 prefix, and the trailing nul byte is sent
				dpsnprintf(request, sizeof(request), "c\n");

				// search internet
				for (masternum = 0;sv_qwmasters[masternum].name;masternum++)
				{
					if (sv_qwmasters[masternum].string && LHNETADDRESS_FromString(&masteraddress, sv_qwmasters[masternum].string, QWMASTER_PORT) && LHNETADDRESS_GetAddressType(&masteraddress) == LHNETADDRESS_GetAddressType(LHNET_AddressFromSocket(cl_sockets[i])))
					{
						if (m_state != m_slist)
						{
							char lookupstring[128];
							LHNETADDRESS_ToString(&masteraddress, lookupstring, sizeof(lookupstring), true);
							Con_Printf("Querying master %s (resolved from %s)\n", lookupstring, sv_qwmasters[masternum].string);
						}
						masterquerycount++;
						NetConn_Write(cl_sockets[i], request, (int)strlen(request) + 1, &masteraddress);
					}
				}
			}
		}
	}
	if (!masterquerycount)
	{
		Con_Print("Unable to query master servers, no suitable network sockets active.\n");
		M_Update_Return_Reason("No network");
	}
}

void NetConn_Heartbeat(int priority)
{
	lhnetaddress_t masteraddress;
	int masternum;
	lhnetsocket_t *mysocket;

	// if it's a state change (client connected), limit next heartbeat to no
	// more than 30 sec in the future
	if (priority == 1 && nextheartbeattime > realtime + 30.0)
		nextheartbeattime = realtime + 30.0;

	// limit heartbeatperiod to 30 to 270 second range,
	// lower limit is to avoid abusing master servers with excess traffic,
	// upper limit is to avoid timing out on the master server (which uses
	// 300 sec timeout)
	if (sv_heartbeatperiod.value < 30)
		Cvar_SetValueQuick(&sv_heartbeatperiod, 30);
	if (sv_heartbeatperiod.value > 270)
		Cvar_SetValueQuick(&sv_heartbeatperiod, 270);

	// make advertising optional and don't advertise singleplayer games, and
	// only send a heartbeat as often as the admin wants
	if (sv.active && sv_public.integer > 0 && svs.maxclients >= 2 && (priority > 1 || realtime > nextheartbeattime))
	{
		nextheartbeattime = realtime + sv_heartbeatperiod.value;
		for (masternum = 0;sv_masters[masternum].name;masternum++)
			if (sv_masters[masternum].string && sv_masters[masternum].string[0] && LHNETADDRESS_FromString(&masteraddress, sv_masters[masternum].string, DPMASTER_PORT) && (mysocket = NetConn_ChooseServerSocketForAddress(&masteraddress)))
				NetConn_WriteString(mysocket, "\377\377\377\377heartbeat DarkPlaces\x0A", &masteraddress);
	}
}

static void Net_Heartbeat_f(void)
{
	if (sv.active)
		NetConn_Heartbeat(2);
	else
		Con_Print("No server running, can not heartbeat to master server.\n");
}

void PrintStats(netconn_t *conn)
{
	if ((cls.state == ca_connected && cls.protocol == PROTOCOL_QUAKEWORLD) || (sv.active && sv.protocol == PROTOCOL_QUAKEWORLD))
		Con_Printf("address=%21s canSend=%u sendSeq=%6u recvSeq=%6u\n", conn->address, !conn->sendMessageLength, conn->outgoing_unreliable_sequence, conn->qw.incoming_sequence);
	else
		Con_Printf("address=%21s canSend=%u sendSeq=%6u recvSeq=%6u\n", conn->address, !conn->sendMessageLength, conn->nq.sendSequence, conn->nq.receiveSequence);
}

void Net_Stats_f(void)
{
	netconn_t *conn;
	Con_Printf("unreliable messages sent   = %i\n", unreliableMessagesSent);
	Con_Printf("unreliable messages recv   = %i\n", unreliableMessagesReceived);
	Con_Printf("reliable messages sent     = %i\n", reliableMessagesSent);
	Con_Printf("reliable messages received = %i\n", reliableMessagesReceived);
	Con_Printf("packetsSent                = %i\n", packetsSent);
	Con_Printf("packetsReSent              = %i\n", packetsReSent);
	Con_Printf("packetsReceived            = %i\n", packetsReceived);
	Con_Printf("receivedDuplicateCount     = %i\n", receivedDuplicateCount);
	Con_Printf("droppedDatagrams           = %i\n", droppedDatagrams);
	Con_Print("connections                =\n");
	for (conn = netconn_list;conn;conn = conn->next)
		PrintStats(conn);
}

void Net_Refresh_f(void)
{
	if (m_state != m_slist) {
		Con_Print("Sending new requests to master servers\n");
		ServerList_QueryList(false, true, false, true);
		Con_Print("Listening for replies...\n");
	} else
		ServerList_QueryList(false, true, false, false);
}

void Net_Slist_f(void)
{
	ServerList_ResetMasks();
	serverlist_sortbyfield = SLIF_PING;
	serverlist_sortdescending = false;
    if (m_state != m_slist) {
		Con_Print("Sending requests to master servers\n");
		ServerList_QueryList(true, true, false, true);
		Con_Print("Listening for replies...\n");
	} else
		ServerList_QueryList(true, true, false, false);
}

void Net_SlistQW_f(void)
{
	ServerList_ResetMasks();
	serverlist_sortbyfield = SLIF_PING;
	serverlist_sortdescending = false;
    if (m_state != m_slist) {
		Con_Print("Sending requests to master servers\n");
		ServerList_QueryList(true, false, true, true);
		serverlist_consoleoutput = true;
		Con_Print("Listening for replies...\n");
	} else
		ServerList_QueryList(true, false, true, false);
}

void NetConn_Init(void)
{
	int i;
	lhnetaddress_t tempaddress;
	netconn_mempool = Mem_AllocPool("network connections", 0, NULL);
	Cmd_AddCommand("net_stats", Net_Stats_f, "print network statistics");
	Cmd_AddCommand("net_slist", Net_Slist_f, "query dp master servers and print all server information");
	Cmd_AddCommand("net_slistqw", Net_SlistQW_f, "query qw master servers and print all server information");
	Cmd_AddCommand("net_refresh", Net_Refresh_f, "query dp master servers and refresh all server information");
	Cmd_AddCommand("heartbeat", Net_Heartbeat_f, "send a heartbeat to the master server (updates your server information)");
	Cvar_RegisterVariable(&rcon_restricted_password);
	Cvar_RegisterVariable(&rcon_restricted_commands);
	Cvar_RegisterVariable(&net_slist_queriespersecond);
	Cvar_RegisterVariable(&net_slist_queriesperframe);
	Cvar_RegisterVariable(&net_slist_timeout);
	Cvar_RegisterVariable(&net_slist_maxtries);
	Cvar_RegisterVariable(&net_slist_pause);
	Cvar_RegisterVariable(&net_messagetimeout);
	Cvar_RegisterVariable(&net_connecttimeout);
	Cvar_RegisterVariable(&net_connectfloodblockingtimeout);
	Cvar_RegisterVariable(&cl_netlocalping);
	Cvar_RegisterVariable(&cl_netpacketloss_send);
	Cvar_RegisterVariable(&cl_netpacketloss_receive);
	Cvar_RegisterVariable(&hostname);
	Cvar_RegisterVariable(&developer_networking);
	Cvar_RegisterVariable(&cl_netport);
	Cvar_RegisterVariable(&sv_netport);
	Cvar_RegisterVariable(&net_address);
	//Cvar_RegisterVariable(&net_address_ipv6);
	Cvar_RegisterVariable(&sv_public);
	Cvar_RegisterVariable(&sv_heartbeatperiod);
	for (i = 0;sv_masters[i].name;i++)
		Cvar_RegisterVariable(&sv_masters[i]);
	Cvar_RegisterVariable(&gameversion);
// COMMANDLINEOPTION: Server: -ip <ipaddress> sets the ip address of this machine for purposes of networking (default 0.0.0.0 also known as INADDR_ANY), use only if you have multiple network adapters and need to choose one specifically.
	if ((i = COM_CheckParm("-ip")) && i + 1 < com_argc)
	{
		if (LHNETADDRESS_FromString(&tempaddress, com_argv[i + 1], 0) == 1)
		{
			Con_Printf("-ip option used, setting net_address to \"%s\"\n", com_argv[i + 1]);
			Cvar_SetQuick(&net_address, com_argv[i + 1]);
		}
		else
			Con_Printf("-ip option used, but unable to parse the address \"%s\"\n", com_argv[i + 1]);
	}
// COMMANDLINEOPTION: Server: -port <portnumber> sets the port to use for a server (default 26000, the same port as QUAKE itself), useful if you host multiple servers on your machine
	if (((i = COM_CheckParm("-port")) || (i = COM_CheckParm("-ipport")) || (i = COM_CheckParm("-udpport"))) && i + 1 < com_argc)
	{
		i = atoi(com_argv[i + 1]);
		if (i >= 0 && i < 65536)
		{
			Con_Printf("-port option used, setting port cvar to %i\n", i);
			Cvar_SetValueQuick(&sv_netport, i);
		}
		else
			Con_Printf("-port option used, but %i is not a valid port number\n", i);
	}
	cl_numsockets = 0;
	sv_numsockets = 0;
	net_message.data = net_message_buf;
	net_message.maxsize = sizeof(net_message_buf);
	net_message.cursize = 0;
	LHNET_Init();
}

void NetConn_Shutdown(void)
{
	NetConn_CloseClientPorts();
	NetConn_CloseServerPorts();
	LHNET_Shutdown();
}

