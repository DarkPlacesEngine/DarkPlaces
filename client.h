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
	// draw this as lightning polygons, or a model?
	int		lightning;
	struct model_s	*model;
	float	endtime;
	vec3_t	start, end;
	// if this beam is owned by an entity, this is the beam start relative to
	// that entity's matrix for per frame start updates
	vec3_t	relativestart;
	vec3_t	relativeend;
	// indicates whether relativestart is valid
	int	relativestartvalid;
}
beam_t;

typedef struct rtlight_s
{
	// shadow volumes are done entirely in model space, so there are no matrices for dealing with them...  they just use the origin 

	// note that the world to light matrices are inversely scaled (divided) by lightradius

	// core properties
	// matrix for transforming world coordinates to light filter coordinates
	matrix4x4_t matrix_worldtolight;
	// based on worldtolight this transforms -1 to +1 to 0 to 1 for purposes
	// of attenuation texturing in full 3D (Z result often ignored)
	matrix4x4_t matrix_worldtoattenuationxyz;
	// this transforms only the Z to S, and T is always 0.5
	matrix4x4_t matrix_worldtoattenuationz;
	// typically 1 1 1, can be lower (dim) or higher (overbright)
	vec3_t color;
	// size of the light (remove?)
	vec_t radius;
	// light filter
	char cubemapname[64];
	// whether light should render shadows
	int shadow;
	// intensity of corona to render
	vec_t corona;
	// light style to monitor for brightness
	int style;
	
	// generated properties
	// used only for shadow volumes
	vec3_t shadoworigin;
	// culling
	vec3_t cullmins;
	vec3_t cullmaxs;
	// culling
	//vec_t cullradius;
	// squared cullradius
	//vec_t cullradius2;

	// lightmap renderer stuff (remove someday!)
	// the size of the light
	vec_t lightmap_cullradius;
	// the size of the light, squared
	vec_t lightmap_cullradius2;
	// the brightness of the light
	vec3_t lightmap_light;
	// to avoid sudden brightness change at cullradius, subtract this
	vec_t lightmap_subtract;

	// static light info
	// true if this light should be compiled as a static light
	int isstatic;
	// true if this is a compiled world light, cleared if the light changes
	int compiled;
	// premade shadow volumes and lit surfaces to render for world entity
	shadowmesh_t *static_meshchain_shadow;
	shadowmesh_t *static_meshchain_light;
	// used for visibility testing (more exact than bbox)
	int static_numclusters;
	int static_numclusterpvsbytes;
	int *static_clusterlist;
	qbyte *static_clusterpvs;
}
rtlight_t;

typedef struct dlight_s
{
	// destroy light after this time
	// (dlight only)
	vec_t die;
	// the entity that owns this light (can be NULL)
	// (dlight only)
	struct entity_render_s *ent;
	// location
	// (worldlight: saved to .rtlights file)
	vec3_t origin;
	// worldlight orientation
	// (worldlight only)
	// (worldlight: saved to .rtlights file)
	vec3_t angles;
	// dlight orientation/scaling/location
	// (dlight only)
	matrix4x4_t matrix;
	// color of light
	// (worldlight: saved to .rtlights file)
	vec3_t color;
	// cubemap number to use on this light
	// (dlight only)
	int cubemapnum;
	// cubemap name to use on this light
	// (worldlight only)
	// (worldlight: saved to .rtlights file)
	char cubemapname[64];
	// make light flash while selected
	// (worldlight only)
	int selected;
	// brightness (not really radius anymore)
	// (worldlight: saved to .rtlights file)
	vec_t radius;
	// drop radius this much each second
	// (dlight only)
	vec_t decay;
	// light style which controls intensity of this light
	// (worldlight: saved to .rtlights file)
	int style;
	// cast shadows
	// (worldlight: saved to .rtlights file)
	int shadow;
	// corona intensity
	// (worldlight: saved to .rtlights file)
	vec_t corona;
	// linked list of world lights
	// (worldlight only)
	struct dlight_s *next;
	// embedded rtlight struct for renderer
	// (renderer only)	
	rtlight_t rtlight;
}
dlight_t;

typedef struct frameblend_s
{
	int frame;
	float lerp;
}
frameblend_t;

// LordHavoc: this struct is intended for the renderer but some fields are
// used by the client.
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

	// interpolated animation

	// frame that the model is interpolating from
	int frame1;
	// frame that the model is interpolating to
	int frame2;
	// interpolation factor, usually computed from frame2time
	float framelerp;
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
	int linkframe;

	vec3_t trail_origin;

	// particle trail
	float trail_time;

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
#define	CSHIFT_VCSHIFT	4
#define	NUM_CSHIFTS		5

#define	NAME_LENGTH	64


//
// client_state_t should hold all pieces of the client state
//

#define	SIGNONS		4			// signon messages to receive before connected

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

// demo loop control
	// -1 = don't play demos
	int demonum;
	// list of demos in loop
	char demos[MAX_DEMOS][MAX_DEMONAME];
	// the actively playing demo (set by CL_PlayDemo_f)
	char demoname[64];

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean demorecording;
	qboolean demoplayback;
	qboolean timedemo;
	// -1 = use normal cd track
	int forcetrack;
	qfile_t *demofile;
	// to meter out one message a frame
	int td_lastframe;
	// host_framecount at start
	int td_startframe;
	// realtime at second frame of timedemo (LordHavoc: changed to double)
	double td_starttime;
	// LordHavoc: for measuring maxfps
	double td_minframetime;
	// LordHavoc: for measuring minfps
	double td_maxframetime;
	// LordHavoc: pausedemo
	qboolean demopaused;

	qboolean connect_trying;
	int connect_remainingtries;
	double connect_nextsendtime;
	lhnetsocket_t *connect_mysocket;
	lhnetaddress_t connect_address;

// connection information
	// 0 to SIGNONS
	int signon;
	// network connection
	netconn_t *netcon;
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
	// true if playing in a local game and no one else is connected
	int islocalgame;

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
	// cl.time of changing STAT_ACTIVEWEAPON
	float weapontime;
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

	// LordHavoc: sniping zoom, QC controlled
	float viewzoom;
	// for interpolation
	float viewzoomold, viewzoomnew;

	// protocol version of the server we're connected to
	int protocol;

	// entity database stuff
	entity_database_t entitydatabase;
	entity_database4_t *entitydatabase4;
}
client_state_t;

extern mempool_t *cl_scores_mempool;

//
// cvars
//
extern cvar_t cl_name;
extern cvar_t cl_color;
extern cvar_t cl_rate;
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
extern cvar_t cl_explosions_alpha_start;
extern cvar_t cl_explosions_alpha_end;
extern cvar_t cl_explosions_size_start;
extern cvar_t cl_explosions_size_end;
extern cvar_t cl_explosions_lifetime;
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

extern void CL_AllocDlight (entity_render_t *ent, matrix4x4_t *matrix, float radius, float red, float green, float blue, float decay, float lifetime, int cubemapnum, int style, int shadowenable, vec_t corona);
extern void CL_DecayLights (void);

//=============================================================================

//
// cl_main
//

void CL_Init (void);

void CL_EstablishConnection(const char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);

void CL_BoundingBoxForEntity(entity_render_t *ent);

extern cvar_t cl_beams_polygons;
extern cvar_t cl_beams_relative;
extern cvar_t cl_beams_lightatend;

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
void CL_SendCmd (usercmd_t *cmd);
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
const char *Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void CL_StopPlayback(void);
void CL_ReadDemoMessage(void);
void CL_WriteDemoMessage(void);

void CL_NextDemo(void);
void CL_Stop_f(void);
void CL_Record_f(void);
void CL_PlayDemo_f(void);
void CL_TimeDemo_f(void);

//
// cl_parse.c
//
void CL_Parse_Init(void);
void CL_ParseServerMessage(void);
void CL_Parse_DumpPacket(void);

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
void CL_ParticleExplosion (vec3_t org);
void CL_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void CL_LavaSplash (vec3_t org);
void CL_TeleportSplash (vec3_t org);
void CL_BeamParticle (const vec3_t start, const vec3_t end, vec_t radius, float red, float green, float blue, float alpha, float lifetime);
void CL_Tei_Smoke(const vec3_t pos, const vec3_t dir, int count);
void CL_Tei_PlasmaHit(const vec3_t pos, const vec3_t dir, int count);
void CL_MoveParticles(void);
void R_MoveExplosions(void);
void R_NewExplosion(vec3_t org);

#include "cl_screen.h"

typedef struct
{
	// area to render in
	int x, y, width, height;
	float fov_x, fov_y;

	// these are set for water warping before
	// fov_x/fov_y are calculated
	float fovscale_x, fovscale_y;

	// view transform
	matrix4x4_t viewentitymatrix;

	// which color components to allow (for anaglyph glasses)
	int colormask[4];

	// fullscreen color blend
	float viewblend[4];

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

