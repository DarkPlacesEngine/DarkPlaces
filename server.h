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
// server.h

#ifndef SERVER_H
#define SERVER_H

typedef struct
{
	int maxclients;
	// [maxclients]
	struct client_s *clients;
	// episode completion information
	int serverflags;
	// cleared when at SV_SpawnServer
	qboolean changelevel_issued;
} server_static_t;

//=============================================================================

typedef enum {ss_loading, ss_active} server_state_t;

typedef struct
{
	// false if only a net client
	qboolean active;

	qboolean paused;
	// handle connections specially
	qboolean loadgame;

	double time;

	double frametime;

	// used by PF_checkclient
	int lastcheck;
	double lastchecktime;

	// map name
	char name[64];
	// maps/<name>.bsp, for model_precache[0]
	char modelname[64];
	struct model_s *worldmodel;
	// NULL terminated
	char *model_precache[MAX_MODELS];
	struct model_s *models[MAX_MODELS];
	// NULL terminated
	char *sound_precache[MAX_SOUNDS];
	char *lightstyles[MAX_LIGHTSTYLES];
	int num_edicts;
	int max_edicts;
	// can NOT be array indexed, because edict_t is variable sized, but can be used to reference the world ent
	edict_t *edicts;
	// can be array indexed
	edict_t **edictstable;
	// array of QC edict field variables
	void *edictsfields;
	// PushMove sometimes has to move entities back from a failed move
	edict_t **moved_edicts;
	// some actions are only valid during load
	server_state_t state;

	sizebuf_t datagram;
	qbyte datagram_buf[MAX_DATAGRAM];

	// copied to all clients at end of frame
	sizebuf_t reliable_datagram;
	qbyte reliable_datagram_buf[MAX_DATAGRAM];

	sizebuf_t signon;
	// LordHavoc: increased signon message buffer from 8192 to 32768
	qbyte signon_buf[32768];
} server_t;


#define NUM_PING_TIMES 16
#define NUM_SPAWN_PARMS 16

typedef struct client_s
{
	// false = client is free
	qboolean active;
	// false = don't send datagrams
	qboolean spawned;
	// has been told to go to another level
	qboolean dropasap;
	// only valid before spawned
	qboolean sendsignon;

#ifndef NOROUTINGFIX
	// LordHavoc: to make netquake protocol get through NAT routers, have to wait for client to ack
	// waiting for connect from client (stage 1)
	qboolean waitingforconnect;
	// send server info in next datagram (stage 2)
	qboolean sendserverinfo;
#endif

	// reliable messages must be sent periodically
	double last_message;

	// communications handle
	struct qsocket_s *netconnection;

	// movement
	usercmd_t cmd;
	// intended motion calced from cmd
	vec3_t wishdir;

	// can be added to at any time, copied and clear once per frame
	sizebuf_t message;
	qbyte msgbuf[MAX_DATAGRAM];
	// EDICT_NUM(clientnum+1)
	edict_t *edict;
	// for printing to other people
	char name[32];
	int colors;

	float ping_times[NUM_PING_TIMES];
	// ping_times[num_pings%NUM_PING_TIMES]
	int num_pings;
	// LordHavoc: can be used for prediction or whatever...
	float ping;
	// LordHavoc: specifically used for prediction, accounts for sys_ticrate too
	float latency;

// spawn parms are carried from level to level
	float spawn_parms[NUM_SPAWN_PARMS];

// client known data for deltas
	int old_frags;
	int pmodel;

#ifdef QUAKEENTITIES
	// delta compression state
	float nextfullupdate[MAX_EDICTS];
#endif
	// visibility state
	float visibletime[MAX_EDICTS];

#ifndef QUAKEENTITIES
	entity_database_t entitydatabase;
	int entityframenumber; // incremented each time an entity frame is sent
#endif
} client_t;


//=============================================================================

// edict->movetype values
#define	MOVETYPE_NONE			0		// never moves
#define	MOVETYPE_ANGLENOCLIP	1
#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// gravity
#define	MOVETYPE_STEP			4		// gravity, special edge handling
#define	MOVETYPE_FLY			5
#define	MOVETYPE_TOSS			6		// gravity
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11		// bounce w/o gravity
#define MOVETYPE_FOLLOW			12		// track movement of aiment

// edict->solid values
#define	SOLID_NOT				0		// no interaction with other objects
#define	SOLID_TRIGGER			1		// touch on edge, but not blocking
#define	SOLID_BBOX				2		// touch on edge, block
#define	SOLID_SLIDEBOX			3		// touch on edge, but not an onground
#define	SOLID_BSP				4		// bsp clip, touch on edge, block
// LordHavoc: corpse code
#define	SOLID_CORPSE			5		// same as SOLID_BBOX, except it behaves as SOLID_NOT against SOLID_SLIDEBOX objects (players/monsters)

// edict->deadflag values
#define	DEAD_NO					0
#define	DEAD_DYING				1
#define	DEAD_DEAD				2

#define	DAMAGE_NO				0
#define	DAMAGE_YES				1
#define	DAMAGE_AIM				2

// edict->flags
#define	FL_FLY					1
#define	FL_SWIM					2
#define	FL_CONVEYOR				4
#define	FL_CLIENT				8
#define	FL_INWATER				16
#define	FL_MONSTER				32
#define	FL_GODMODE				64
#define	FL_NOTARGET				128
#define	FL_ITEM					256
#define	FL_ONGROUND				512
#define	FL_PARTIALGROUND		1024	// not all corners are valid
#define	FL_WATERJUMP			2048	// player jumping out of water
#define	FL_JUMPRELEASED			4096	// for jump debouncing

// entity effects

#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8
// added EF_ effects:
#define	EF_NODRAW				16
#define EF_ADDITIVE				32  // LordHavoc: Additive Rendering
#define EF_BLUE					64
#define EF_RED					128

#define	SPAWNFLAG_NOT_EASY			256
#define	SPAWNFLAG_NOT_MEDIUM		512
#define	SPAWNFLAG_NOT_HARD			1024
#define	SPAWNFLAG_NOT_DEATHMATCH	2048

//============================================================================

extern cvar_t teamplay;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t pausable;
extern cvar_t sv_deltacompress;
extern cvar_t sv_maxvelocity;
extern cvar_t sv_gravity;
extern cvar_t sv_nostep;
extern cvar_t sv_friction;
extern cvar_t sv_edgefriction;
extern cvar_t sv_stopspeed;
extern cvar_t sv_maxspeed;
extern cvar_t sv_accelerate;
extern cvar_t sv_idealpitchscale;
extern cvar_t sv_aim;
extern cvar_t sv_predict;
extern cvar_t sv_stepheight;
extern cvar_t sv_jumpstep;

// persistant server info
extern server_static_t svs;
// local server
extern server_t sv;

extern client_t *host_client;

extern jmp_buf host_abortserver;

extern edict_t *sv_player;

//===========================================================

void SV_Init (void);

void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count);
void SV_StartEffect (vec3_t org, int modelindex, int startframe, int framecount, int framerate);
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume, float attenuation);

void SV_DropClient (qboolean crash);

void SV_SendClientMessages (void);
void SV_ClearDatagram (void);

int SV_ModelIndex (const char *name);

void SV_SetIdealPitch (void);

void SV_AddUpdates (void);

void SV_ClientThink (void);
void SV_AddClientToServer (struct qsocket_s	*ret);

void SV_ClientPrintf (const char *fmt, ...);
void SV_BroadcastPrintf (const char *fmt, ...);

void SV_Physics (void);

qboolean SV_CheckBottom (edict_t *ent);
qboolean SV_movestep (edict_t *ent, vec3_t move, qboolean relink);

void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg);

void SV_MoveToGoal (void);

void SV_CheckForNewClients (void);
void SV_RunClients (void);
void SV_SaveSpawnparms (void);
void SV_SpawnServer (const char *server);

void SV_SetMaxClients(int n);

void SV_CheckVelocity (edict_t *ent);

#endif

