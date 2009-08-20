/*
Copyright (C) 1996-1997 Id Software, Inc.
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

#ifndef NET_H
#define NET_H

#include "lhnet.h"

#define NET_HEADERSIZE		(2 * sizeof(unsigned int))

// NetHeader flags
#define NETFLAG_LENGTH_MASK	0x0000ffff
#define NETFLAG_DATA		0x00010000
#define NETFLAG_ACK			0x00020000
#define NETFLAG_NAK			0x00040000
#define NETFLAG_EOM			0x00080000
#define NETFLAG_UNRELIABLE	0x00100000
#define NETFLAG_CTL			0x80000000


#define NET_PROTOCOL_VERSION	3
#define NET_EXTRESPONSE_MAX 16

/// \page netconn The network info/connection protocol.
/// It is used to find Quake
/// servers, get info about them, and connect to them.  Once connected, the
/// Quake game protocol (documented elsewhere) is used.
///
///
/// General notes:\code
///	game_name is currently always "QUAKE", but is there so this same protocol
///		can be used for future games as well; can you say Quake2?
///
/// CCREQ_CONNECT
///		string	game_name				"QUAKE"
///		byte	net_protocol_version	NET_PROTOCOL_VERSION
///
/// CCREQ_SERVER_INFO
///		string	game_name				"QUAKE"
///		byte	net_protocol_version	NET_PROTOCOL_VERSION
///
/// CCREQ_PLAYER_INFO
///		byte	player_number
///
/// CCREQ_RULE_INFO
///		string	rule
///
/// CCREQ_RCON
///		string	password
///		string	command
///
///
///
/// CCREP_ACCEPT
///		long	port
///
/// CCREP_REJECT
///		string	reason
///
/// CCREP_SERVER_INFO
///		string	server_address
///		string	host_name
///		string	level_name
///		byte	current_players
///		byte	max_players
///		byte	protocol_version	NET_PROTOCOL_VERSION
///
/// CCREP_PLAYER_INFO
///		byte	player_number
///		string	name
///		long	colors
///		long	frags
///		long	connect_time
///		string	address
///
/// CCREP_RULE_INFO
///		string	rule
///		string	value
///
/// CCREP_RCON
///		string	reply
/// \endcode
///	\note
///		There are two address forms used above.  The short form is just a
///		port number.  The address that goes along with the port is defined as
///		"whatever address you receive this reponse from".  This lets us use
///		the host OS to solve the problem of multiple host addresses (possibly
///		with no routing between them); the host will use the right address
///		when we reply to the inbound connection request.  The long from is
///		a full address and port in a string.  It is used for returning the
///		address of a server that is not running locally.

#define CCREQ_CONNECT		0x01
#define CCREQ_SERVER_INFO	0x02
#define CCREQ_PLAYER_INFO	0x03
#define CCREQ_RULE_INFO		0x04
#define CCREQ_RCON		0x05 // RocketGuy: ProQuake rcon support

#define CCREP_ACCEPT		0x81
#define CCREP_REJECT		0x82
#define CCREP_SERVER_INFO	0x83
#define CCREP_PLAYER_INFO	0x84
#define CCREP_RULE_INFO		0x85
#define CCREP_RCON		0x86 // RocketGuy: ProQuake rcon support

typedef struct netconn_s
{
	struct netconn_s *next;

	lhnetsocket_t *mysocket;
	lhnetaddress_t peeraddress;

	// this is mostly identical to qsocket_t from quake

	/// if this time is reached, kick off peer
	double connecttime;
	double timeout;
	double lastMessageTime;
	double lastSendTime;

	/// writing buffer to send to peer as the next reliable message
	/// can be added to at any time, copied into sendMessage buffer when it is
	/// possible to send a reliable message and then cleared
	/// @{
	sizebuf_t message;
	unsigned char messagedata[NET_MAXMESSAGE];
	/// @}

	/// reliable message that is currently sending
	/// (for building fragments)
	int sendMessageLength;
	unsigned char sendMessage[NET_MAXMESSAGE];

	/// reliable message that is currently being received
	/// (for putting together fragments)
	int receiveMessageLength;
	unsigned char receiveMessage[NET_MAXMESSAGE];

	/// used by both NQ and QW protocols
	unsigned int outgoing_unreliable_sequence;

	struct netconn_nq_s
	{
		unsigned int ackSequence;
		unsigned int sendSequence;

		unsigned int receiveSequence;
		unsigned int unreliableReceiveSequence;
	}
	nq;
	struct netconn_qw_s
	{
		// QW protocol
		qboolean	fatal_error;

		float		last_received;		// for timeouts

	// the statistics are cleared at each client begin, because
	// the server connecting process gives a bogus picture of the data
		float		frame_latency;		// rolling average
		float		frame_rate;

		int			drop_count;			///< dropped packets, cleared each level
		int			good_count;			///< cleared each level

		int			qport;

	// sequencing variables
		int			incoming_sequence;
		int			incoming_acknowledged;
		int			incoming_reliable_acknowledged;	///< single bit

		int			incoming_reliable_sequence;		///< single bit, maintained local

		int			reliable_sequence;			///< single bit
		int			last_reliable_sequence;		///< sequence number of last send
	}
	qw;

	// bandwidth estimator
	double		cleartime;			// if realtime > nc->cleartime, free to go

	// this tracks packet loss and packet sizes on the most recent packets
	// used by shownetgraph feature
#define NETGRAPH_PACKETS 100
#define NETGRAPH_NOPACKET 0
#define NETGRAPH_LOSTPACKET -1
#define NETGRAPH_CHOKEDPACKET -2
	int incoming_packetcounter;
	int incoming_reliablesize[NETGRAPH_PACKETS];
	int incoming_unreliablesize[NETGRAPH_PACKETS];
	int incoming_acksize[NETGRAPH_PACKETS];
	int outgoing_packetcounter;
	int outgoing_reliablesize[NETGRAPH_PACKETS];
	int outgoing_unreliablesize[NETGRAPH_PACKETS];
	int outgoing_acksize[NETGRAPH_PACKETS];

	char address[128];
} netconn_t;

extern netconn_t *netconn_list;
extern mempool_t *netconn_mempool;

extern cvar_t hostname;
extern cvar_t developer_networking;

#define SERVERLIST_TOTALSIZE		2048
#define SERVERLIST_VIEWLISTSIZE		SERVERLIST_TOTALSIZE
#define SERVERLIST_ANDMASKCOUNT		5
#define SERVERLIST_ORMASKCOUNT		5

typedef enum serverlist_maskop_e
{
	// SLMO_CONTAINS is the default for strings
	// SLMO_GREATEREQUAL is the default for numbers (also used when OP == CONTAINS or NOTCONTAINS
	SLMO_CONTAINS,
	SLMO_NOTCONTAIN,

	SLMO_LESSEQUAL,
	SLMO_LESS,
	SLMO_EQUAL,
	SLMO_GREATER,
	SLMO_GREATEREQUAL,
	SLMO_NOTEQUAL,
	SLMO_STARTSWITH,
	SLMO_NOTSTARTSWITH
} serverlist_maskop_t;

/// struct with all fields that you can search for or sort by
typedef struct serverlist_info_s
{
	/// address for connecting
	char cname[128];
	/// ping time for sorting servers
	int ping;
	/// name of the game
	char game[32];
	/// name of the mod
	char mod[32];
	/// name of the map
	char map[32];
	/// name of the session
	char name[128];
	/// qc-defined short status string
	char qcstatus[128];
	/// frags/ping/name list (if they fit in the packet)
	char players[1400];
	/// max client number
	int maxplayers;
	/// number of currently connected players (including bots)
	int numplayers;
	/// number of currently connected players that are bots
	int numbots;
	/// number of currently connected players that are not bots
	int numhumans;
	/// number of free slots
	int freeslots;
	/// protocol version
	int protocol;
	/// game data version
	/// (an integer that is used for filtering incompatible servers,
	///  not filterable by QC)
	int gameversion;
	/// favorite server flag
	qboolean isfavorite;
} serverlist_info_t;

typedef enum
{
	SLIF_CNAME,
	SLIF_PING,
	SLIF_GAME,
	SLIF_MOD,
	SLIF_MAP,
	SLIF_NAME,
	SLIF_MAXPLAYERS,
	SLIF_NUMPLAYERS,
	SLIF_PROTOCOL,
	SLIF_NUMBOTS,
	SLIF_NUMHUMANS,
	SLIF_FREESLOTS,
	SLIF_QCSTATUS,
	SLIF_PLAYERS,
	SLIF_ISFAVORITE,
	SLIF_COUNT
} serverlist_infofield_t;

typedef enum
{
	SLSF_DESCENDING = 1,
	SLSF_FAVORITESFIRST = 2
} serverlist_sortflags_t;

typedef enum
{
	SQS_NONE = 0,
	SQS_QUERYING,
	SQS_QUERIED,
	SQS_TIMEDOUT,
	SQS_REFRESHING
} serverlist_query_state;

typedef struct serverlist_entry_s
{
	/// used to determine whether this entry should be included into the final view
	serverlist_query_state query;
	/// used to count the number of times the host has tried to query this server already
	unsigned querycounter;
	/// used to calculate ping when update comes in
	double querytime;
	/// query protocol to use on this server, may be PROTOCOL_QUAKEWORLD or PROTOCOL_DARKPLACES7
	int protocol;

	serverlist_info_t info;

	// legacy stuff
	char line1[128];
	char line2[128];
} serverlist_entry_t;

typedef struct serverlist_mask_s
{
	qboolean			active;
	serverlist_maskop_t  tests[SLIF_COUNT];
	serverlist_info_t info;
} serverlist_mask_t;

extern serverlist_mask_t serverlist_andmasks[SERVERLIST_ANDMASKCOUNT];
extern serverlist_mask_t serverlist_ormasks[SERVERLIST_ORMASKCOUNT];

extern serverlist_infofield_t serverlist_sortbyfield;
extern int serverlist_sortflags; // not using the enum, as it is a bitmask

extern int serverlist_viewcount;
extern serverlist_entry_t *serverlist_viewlist[SERVERLIST_VIEWLISTSIZE];

extern int serverlist_cachecount;

extern qboolean serverlist_consoleoutput;

void ServerList_GetPlayerStatistics(int *numplayerspointer, int *maxplayerspointer);

//============================================================================
//
// public network functions
//
//============================================================================

extern char net_extresponse[NET_EXTRESPONSE_MAX][1400];
extern int net_extresponse_count;
extern int net_extresponse_last;

extern double masterquerytime;
extern int masterquerycount;
extern int masterreplycount;
extern int serverquerycount;
extern int serverreplycount;

extern sizebuf_t net_message;

extern cvar_t sv_public;

extern cvar_t cl_netlocalping;

extern cvar_t cl_netport;
extern cvar_t sv_netport;
extern cvar_t net_address;
extern cvar_t net_address_ipv6;

qboolean NetConn_CanSend(netconn_t *conn);
int NetConn_SendUnreliableMessage(netconn_t *conn, sizebuf_t *data, protocolversion_t protocol, int rate, qboolean quakesignon_suppressreliables);
qboolean NetConn_HaveClientPorts(void);
qboolean NetConn_HaveServerPorts(void);
void NetConn_CloseClientPorts(void);
void NetConn_OpenClientPorts(void);
void NetConn_CloseServerPorts(void);
void NetConn_OpenServerPorts(int opennetports);
void NetConn_UpdateSockets(void);
lhnetsocket_t *NetConn_ChooseClientSocketForAddress(lhnetaddress_t *address);
lhnetsocket_t *NetConn_ChooseServerSocketForAddress(lhnetaddress_t *address);
void NetConn_Init(void);
void NetConn_Shutdown(void);
netconn_t *NetConn_Open(lhnetsocket_t *mysocket, lhnetaddress_t *peeraddress);
void NetConn_Close(netconn_t *conn);
void NetConn_Listen(qboolean state);
int NetConn_Read(lhnetsocket_t *mysocket, void *data, int maxlength, lhnetaddress_t *peeraddress);
int NetConn_Write(lhnetsocket_t *mysocket, const void *data, int length, const lhnetaddress_t *peeraddress);
int NetConn_WriteString(lhnetsocket_t *mysocket, const char *string, const lhnetaddress_t *peeraddress);
int NetConn_IsLocalGame(void);
void NetConn_ClientFrame(void);
void NetConn_ServerFrame(void);
void NetConn_SleepMicroseconds(int microseconds);
void NetConn_QueryMasters(qboolean querydp, qboolean queryqw);
void NetConn_Heartbeat(int priority);
void NetConn_QueryQueueFrame(void);
void Net_Stats_f(void);
void Net_Slist_f(void);
void Net_SlistQW_f(void);
void Net_Refresh_f(void);

/// ServerList interface (public)
/// manually refresh the view set, do this after having changed the mask or any other flag
void ServerList_RebuildViewList(void);
void ServerList_ResetMasks(void);
void ServerList_QueryList(qboolean resetcache, qboolean querydp, qboolean queryqw, qboolean consoleoutput);

/// called whenever net_slist_favorites changes
void NetConn_UpdateFavorites(void);

#endif

