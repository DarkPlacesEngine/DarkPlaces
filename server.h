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
	// number of svs.clients slots (updated by maxplayers command)
	int maxclients;
	// client slots
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

	// one of the PROTOCOL_ values
	protocolversion_t protocol;

	// used for running multiple steps in one frame, etc
	double timer;

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
	// LordHavoc: precaches are now MAX_QPATH rather than a pointer
	// updated by SV_ModelIndex
	char model_precache[MAX_MODELS][MAX_QPATH];
	struct model_s *models[MAX_MODELS];
	// NULL terminated
	// LordHavoc: precaches are now MAX_QPATH rather than a pointer
	// updated by SV_SoundIndex
	char sound_precache[MAX_SOUNDS][MAX_QPATH];
	char lightstyles[MAX_LIGHTSTYLES][64];
	// PushMove sometimes has to move entities back from a failed move
	// (dynamically resized)
	prvm_edict_t **moved_edicts;
	// some actions are only valid during load
	server_state_t state;

	sizebuf_t datagram;
	qbyte datagram_buf[NET_MAXMESSAGE];

	// copied to all clients at end of frame
	sizebuf_t reliable_datagram;
	qbyte reliable_datagram_buf[NET_MAXMESSAGE];

	sizebuf_t signon;
	// LordHavoc: increased signon message buffer from 8192
	qbyte signon_buf[NET_MAXMESSAGE];
} server_t;

// if defined this does ping smoothing, otherwise it does not
//#define NUM_PING_TIMES 16

#define NUM_SPAWN_PARMS 16

typedef struct client_s
{
	// false = empty client slot
	qboolean active;
	// false = don't do ClientDisconnect on drop
	qboolean clientconnectcalled;
	// false = don't send datagrams
	qboolean spawned;
	// has been told to go to another level
	qboolean dropasap;
	// only valid before spawned
	qboolean sendsignon;

	// requested rate in bytes per second
	int rate;

	// realtime this client connected
	double connecttime;

	// reliable messages must be sent periodically
	double last_message;

	// communications handle
	netconn_t *netconnection;

	int movesequence;
	// movement
	usercmd_t cmd;
	// intended motion calced from cmd
	vec3_t wishdir;

	// can be added to at any time, copied and clear once per frame
	sizebuf_t message;
	qbyte msgbuf[NET_MAXMESSAGE];
	// PRVM_EDICT_NUM(clientnum+1)
	prvm_edict_t *edict;

#ifdef NUM_PING_TIMES
	float ping_times[NUM_PING_TIMES];
	// ping_times[num_pings%NUM_PING_TIMES]
	int num_pings;
#endif
	// LordHavoc: can be used for prediction or whatever...
	float ping;

// spawn parms are carried from level to level
	float spawn_parms[NUM_SPAWN_PARMS];

	// properties that are sent across the network only when changed
	char name[64], old_name[64];
	int colors, old_colors;
	int frags, old_frags;
	char playermodel[MAX_QPATH], old_model[MAX_QPATH];
	char playerskin[MAX_QPATH], old_skin[MAX_QPATH];

	// visibility state
	float visibletime[MAX_EDICTS];

	// prevent animated names
	float nametime;

	// latest received clc_ackframe (used to detect packet loss)
	int latestframenum;

	entityframe_database_t *entitydatabase;
	entityframe4_database_t *entitydatabase4;
	entityframe5_database_t *entitydatabase5;
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
#define MOVETYPE_FAKEPUSH		13		// tenebrae's push that doesn't push

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
extern cvar_t sv_stepheight;
extern cvar_t sv_jumpstep;
extern cvar_t sv_public;
extern cvar_t sv_maxrate;

extern cvar_t sv_gameplayfix_grenadebouncedownslopes;
extern cvar_t sv_gameplayfix_noairborncorpse;
extern cvar_t sv_gameplayfix_stepdown;
extern cvar_t sv_gameplayfix_stepwhilejumping;
extern cvar_t sv_gameplayfix_swiminbmodels;
extern cvar_t sv_gameplayfix_setmodelrealbox;
extern cvar_t sv_gameplayfix_blowupfallenzombies;
extern cvar_t sv_gameplayfix_findradiusdistancetobox;

extern mempool_t *sv_mempool;

// persistant server info
extern server_static_t svs;
// local server
extern server_t sv;

extern client_t *host_client;

//===========================================================

void SV_Init (void);

void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count);
void SV_StartEffect (vec3_t org, int modelindex, int startframe, int framecount, int framerate);
void SV_StartSound (prvm_edict_t *entity, int channel, const char *sample, int volume, float attenuation);

void SV_ConnectClient (int clientnum, netconn_t *netconnection);
void SV_DropClient (qboolean crash);

void SV_SendClientMessages (void);
void SV_ClearDatagram (void);

void SV_ReadClientMessage(void);

// precachemode values:
// 0 = fail if not precached,
// 1 = warn if not found and precache if possible
// 2 = precache
int SV_ModelIndex(const char *s, int precachemode);
int SV_SoundIndex(const char *s, int precachemode);

void SV_SetIdealPitch (void);

void SV_AddUpdates (void);

void SV_ClientThink (void);

void SV_ClientPrint(const char *msg);
void SV_ClientPrintf(const char *fmt, ...);
void SV_BroadcastPrint(const char *msg);
void SV_BroadcastPrintf(const char *fmt, ...);

void SV_Physics (void);

qboolean SV_PlayerCheckGround (prvm_edict_t *ent);
qboolean SV_CheckBottom (prvm_edict_t *ent);
qboolean SV_movestep (prvm_edict_t *ent, vec3_t move, qboolean relink);

void SV_WriteClientdataToMessage (client_t *client, prvm_edict_t *ent, sizebuf_t *msg, int *stats);

void SV_MoveToGoal (void);

void SV_ApplyClientMove (void);
void SV_SaveSpawnparms (void);
void SV_SpawnServer (const char *server);

void SV_CheckVelocity (prvm_edict_t *ent);

void SV_SetupVM(void);

void SV_VM_Begin(void);
void SV_VM_End(void);

#endif

