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
// client.h

#ifndef CLIENT_H
#define CLIENT_H

#include "matrixlib.h"

// LordHavoc: 256 dynamic lights
#define MAX_DLIGHTS 256
// LordHavoc: this affects the lighting scale of the whole game
#define LIGHTOFFSET 1024.0f
// max lights shining on one entity
#define MAXENTLIGHTS 128

extern int cl_max_entities;
extern int cl_max_static_entities;
extern int cl_max_temp_entities;
extern int cl_max_effects;
extern int cl_max_beams;

typedef struct effect_s
{
	int active;
	vec3_t origin;
	float starttime;
	float framerate;
	int modelindex;
	int startframe;
	int endframe;
	// these are for interpolation
	int frame;
	double frame1time;
	double frame2time;
}
cl_effect_t;

typedef struct
{
	int		entity;
	struct model_s	*model;
	float	endtime;
	vec3_t	start, end;
}
beam_t;

typedef struct
{
	// location
	vec3_t	origin;
	// stop lighting after this time
	float	die;
	// color of light
	vec3_t	color;
	// brightness (not really radius anymore)
	float	radius;
	// drop this each second
	float	decay;
	// the entity that spawned this light (can be NULL if it will never be replaced)
	//entity_render_t *ent;
}
dlight_t;

typedef struct frameblend_s
{
	int frame;
	float lerp;
}
frameblend_t;

// LordHavoc: disregard the following warning, entlights stuff is semi-persistent...
// LordHavoc: nothing in this structure is persistent, it may be overwritten by the client every frame, for persistent data use entity_lerp_t.
typedef struct entity_render_s
{
	// location
	vec3_t origin;
	// orientation
	vec3_t angles;
	// transform matrix for model to world
	matrix4x4_t matrix;
	// transform matrix for world to model
	matrix4x4_t inversematrix;
	// opacity (alpha) of the model
	float alpha;
	// size the model is shown
	float scale;

	// NULL = no model
	model_t *model;
	// current uninterpolated animation frame (for things which do not use interpolation)
	int frame;
	// entity shirt and pants colors
	int colormap;
	// light, particles, etc
	int effects;
	// for Alias models
	int skinnum;
	// render flags
	int flags;

	// these are copied from the persistent data

	// frame that the model is interpolating from
	int frame1;
	// frame that the model is interpolating to
	int frame2;
	// interpolation factor, usually computed from frame2time
	double framelerp;
	// time frame1 began playing (for framegroup animations)
	double frame1time;
	// time frame2 began playing (for framegroup animations)
	double frame2time;

	// calculated by the renderer (but not persistent)

	// if visframe == r_framecount, it is visible
	int visframe;
	// calculated during R_AddModelEntities
	vec3_t mins, maxs;
	// 4 frame numbers (-1 if not used) and their blending scalers (0-1), if interpolation is not desired, use frame instead
	frameblend_t frameblend[4];

	// caching results of static light traces (this is semi-persistent)
	double entlightstime;
	vec3_t entlightsorigin;
	int entlightsframe;
	int numentlights;
	unsigned short entlights[MAXENTLIGHTS];
}
entity_render_t;

typedef struct entity_persistent_s
{
	// particles

	// trail rendering
	vec3_t trail_origin;
	float trail_time;

	// effects

	// muzzleflash fading
	float muzzleflash;

	// interpolated movement

	// start time of move
	float lerpstarttime;
	// time difference from start to end of move
	float lerpdeltatime;
	// the move itself, start and end
	float oldorigin[3];
	float oldangles[3];
	float neworigin[3];
	float newangles[3];

	// interpolated animation

	// lerp resets when model changes
	int modelindex;
	// frame that the model is interpolating from
	int frame1;
	// frame that the model is interpolating to
	int frame2;
	// interpolation factor, usually computed from frame2time
	double framelerp;
	// time frame1 began playing (for framegroup animations)
	double frame1time;
	// time frame2 began playing (for framegroup animations)
	double frame2time;
}
entity_persistent_t;

typedef struct entity_s
{
	// baseline state (default values)
	entity_state_t state_baseline;
	// previous state (interpolating from this)
	entity_state_t state_previous;
	// current state (interpolating to this)
	entity_state_t state_current;

	// used for regenerating parts of render
	entity_persistent_t persistent;

	// the only data the renderer should know about
	entity_render_t render;
}
entity_t;

typedef struct
{
	vec3_t	viewangles;

// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;
} usercmd_t;

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		colors; // two 4 bit fields
} scoreboard_t;

typedef struct
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

#define	NAME_LENGTH	64


//
// client_state_t should hold all pieces of the client state
//

#define	SIGNONS		4			// signon messages to receive before connected

#define	MAX_MAPSTRING	2048
#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum
{
	ca_dedicated, 		// a dedicated server with no ability to start a client
	ca_disconnected, 	// full screen console with no connection
	ca_connected		// valid netcon, talking to a server
}
cactive_t;

//
// the client_static_t structure is persistent through an arbitrary number
// of server connections
//
typedef struct
{
	cactive_t state;

// personalization data sent to server
	char mapstring[MAX_QPATH];
	// to restart a level
	//char spawnparms[MAX_MAPSTRING];

// demo loop control
	// -1 = don't play demos
	int demonum;
	// list of demos in loop
	char demos[MAX_DEMOS][MAX_DEMONAME];

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean demorecording;
	qboolean demoplayback;
	qboolean timedemo;
	// -1 = use normal cd track
	int forcetrack;
	QFile *demofile;
	// to meter out one message a frame
	int td_lastframe;
	// host_framecount at start
	int td_startframe;
	// realtime at second frame of timedemo (LordHavoc: changed to double)
	double td_starttime;
	// LordHavoc: pausedemo
	qboolean demopaused;


// connection information
	// 0 to SIGNONS
	int signon;
	// network socket
	struct qsocket_s *netcon;
	// writing buffer to send to server
	sizebuf_t message;
}
client_static_t;

extern client_static_t	cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
	// when connecting to the server throw out the first couple move messages
	// so the player doesn't accidentally do something the first frame
	int movemessages;

	// send a clc_nop periodically until connected
	float sendnoptime;

	// last command sent to the server
	usercmd_t cmd;

// information for local display
	// health, etc
	int stats[MAX_CL_STATS];
	// inventory bit flags
	int items;
	// cl.time of acquiring item, for blinking
	float item_gettime[32];
	// use pain anim frame if cl.time < this
	float faceanimtime;

	// color shifts for damage, powerups
	cshift_t cshifts[NUM_CSHIFTS];
	// and content types
	cshift_t prev_cshifts[NUM_CSHIFTS];

// the client maintains its own idea of view angles, which are
// sent to the server each frame.  The server sets punchangle when
// the view is temporarily offset, and an angle reset commands at the start
// of each level and after teleporting.

	// during demo playback viewangles is lerped between these
	vec3_t mviewangles[2];
	// either client controlled, or lerped from demo mviewangles
	vec3_t viewangles;

	// update by server, used for lean+bob (0 is newest)
	vec3_t mvelocity[2];
	// lerped between mvelocity[0] and [1]
	vec3_t velocity;

	// temporary offset
	vec3_t punchangle;
	// LordHavoc: origin view kick
	vec3_t punchvector;

// pitch drifting vars
	float idealpitch;
	float pitchvel;
	qboolean nodrift;
	float driftmove;
	double laststop;

	float viewheight;
	// local amount for smoothing stepups
	//float crouch;

	// sent by server
	qboolean paused;
	qboolean onground;
	qboolean inwater;

	// don't change view angle, full screen, etc
	int intermission;
	// latched at intermission start
	int completed_time;

	// the timestamp of the last two messages
	double mtime[2];

	// clients view of time, time should be between mtime[0] and mtime[1] to
	// generate a lerp point for other data, oldtime is the previous frame's
	// value of time, frametime is the difference between time and oldtime
	double time, oldtime, frametime;

	// copy of realtime from last recieved message, for net trouble icon
	float last_received_message;

// information that is static for the entire time connected to a server
	struct model_s *model_precache[MAX_MODELS];
	struct sfx_s *sound_precache[MAX_SOUNDS];

	// for display on solo scoreboard
	char levelname[40];
	// cl_entitites[cl.viewentity] = player
	int viewentity;
	// the real player entity (normally same as viewentity,
	// different than viewentity if mod uses chasecam or other tricks)
	int playerentity;
	// max players that can be in this game
	int maxclients;
	// type of game (deathmatch, coop, singleplayer)
	int gametype;

// refresh related state

	// cl_entitites[0].model
	struct model_s *worldmodel;

	// the gun model
	entity_t viewent;

	// cd audio
	int cdtrack, looptrack;

// frag scoreboard

	// [cl.maxclients]
	scoreboard_t *scores;

	// used by view code for setting up eye position
	vec3_t viewentorigin;
	// LordHavoc: sniping zoom, QC controlled
	float viewzoom;
	// for interpolation
	float viewzoomold, viewzoomnew;

	// entity database stuff
	vec3_t viewentoriginold, viewentoriginnew;
	entity_database_t entitydatabase;
}
client_state_t;

extern mempool_t *cl_scores_mempool;

//
// cvars
//
extern cvar_t cl_name;
extern cvar_t cl_color;
extern cvar_t cl_pmodel;

extern cvar_t cl_upspeed;
extern cvar_t cl_forwardspeed;
extern cvar_t cl_backspeed;
extern cvar_t cl_sidespeed;

extern cvar_t cl_movespeedkey;

extern cvar_t cl_yawspeed;
extern cvar_t cl_pitchspeed;

extern cvar_t cl_anglespeedkey;

extern cvar_t cl_autofire;

extern cvar_t cl_shownet;
extern cvar_t cl_nolerp;

extern cvar_t cl_pitchdriftspeed;
extern cvar_t lookspring;
extern cvar_t lookstrafe;
extern cvar_t sensitivity;

extern cvar_t freelook;

extern cvar_t m_pitch;
extern cvar_t m_yaw;
extern cvar_t m_forward;
extern cvar_t m_side;

extern cvar_t r_draweffects;

extern cvar_t cl_explosions;
extern cvar_t cl_stainmaps;

// these are updated by CL_ClearState
extern int cl_num_entities;
extern int cl_num_static_entities;
extern int cl_num_temp_entities;
extern int cl_num_brushmodel_entities;

extern entity_t *cl_entities;
extern qbyte *cl_entities_active;
extern entity_t *cl_static_entities;
extern entity_t *cl_temp_entities;
extern entity_render_t **cl_brushmodel_entities;
extern cl_effect_t *cl_effects;
extern beam_t *cl_beams;
extern dlight_t *cl_dlights;
extern lightstyle_t *cl_lightstyle;


extern client_state_t cl;

extern void CL_AllocDlight (entity_render_t *ent, vec3_t org, float radius, float red, float green, float blue, float decay, float lifetime);
extern void CL_DecayLights (void);

//=============================================================================

//
// cl_main
//

void CL_Init (void);

void CL_EstablishConnection (char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);

void CL_BoundingBoxForEntity(entity_render_t *ent);

//
// cl_input
//
typedef struct
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
}
kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;

void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_LerpUpdate(entity_t *e);
void CL_ParseTEnt (void);
void CL_RelinkBeams (void);

void CL_ClearTempEntities (void);
entity_t *CL_NewTempEntity (void);

void CL_Effect(vec3_t org, int modelindex, int startframe, int framecount, float framerate);

void CL_ClearState (void);


int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);


float CL_KeyState (kbutton_t *key);
char *Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void CL_StopPlayback (void);
int CL_GetMessage (void);

void CL_NextDemo (void);
void CL_Stop_f (void);
void CL_Record_f (void);
void CL_PlayDemo_f (void);
void CL_TimeDemo_f (void);

//
// cl_parse.c
//
void CL_Parse_Init(void);
void CL_ParseServerMessage(void);

//
// view
//
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_Init (void);
float V_CalcRoll (vec3_t angles, vec3_t velocity);
void V_UpdateBlends (void);
void V_ParseDamage (void);


//
// cl_tent
//
void CL_InitTEnts (void);

//
// cl_part
//

void CL_Particles_Clear(void);
void CL_Particles_Init(void);

void CL_ParseParticleEffect (void);
void CL_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void CL_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent);
void CL_RocketTrail2 (vec3_t start, vec3_t end, int color, entity_t *ent);
void CL_SparkShower (vec3_t org, vec3_t dir, int count);
void CL_PlasmaBurn (vec3_t org);
void CL_BloodPuff (vec3_t org, vec3_t vel, int count);
void CL_Stardust (vec3_t mins, vec3_t maxs, int count);
void CL_FlameCube (vec3_t mins, vec3_t maxs, int count);
void CL_Flames (vec3_t org, vec3_t vel, int count);
void CL_BloodShower (vec3_t mins, vec3_t maxs, float velspeed, int count);
void CL_ParticleCube (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int gravity, int randomvel);
void CL_ParticleRain (vec3_t mins, vec3_t maxs, vec3_t dir, int count, int colorbase, int type);
void CL_EntityParticles (entity_t *ent);
void CL_BlobExplosion (vec3_t org);
void CL_ParticleExplosion (vec3_t org, int smoke);
void CL_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void CL_LavaSplash (vec3_t org);
void CL_TeleportSplash (vec3_t org);
void CL_MoveParticles(void);
void R_MoveExplosions(void);
void R_NewExplosion(vec3_t org);

#include "cl_screen.h"

typedef struct
{
	// area to render in
	int x, y, width, height;
	float fov_x, fov_y;

	// view point
	vec3_t vieworg;
	vec3_t viewangles;

	// fullscreen color blend
	float viewblend[4];

	// weapon model
	entity_render_t viewent;

	entity_render_t **entities;
	int numentities;
	int maxentities;

	qbyte *drawqueue;
	int drawqueuesize;
	int maxdrawqueuesize;
}
refdef_t;

refdef_t r_refdef;

extern mempool_t *cl_refdef_mempool;

#include "cgamevm.h"

#endif

