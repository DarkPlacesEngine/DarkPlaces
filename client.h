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

typedef struct frameblend_s
{
	int frame;
	float lerp;
}
frameblend_t;

// LordHavoc: nothing in this structure is persistant, it may be overwritten by the client every frame, for persistant data use entity_lerp_t.
typedef struct entity_render_s
{
	vec3_t	origin;			// location
	vec3_t	angles;			// orientation
	float	alpha;			// opacity (alpha) of the model
	float	scale;			// size the model is shown

	model_t	*model;			// NULL = no model
	int		frame;			// current uninterpolated animation frame (for things which do not use interpolation)
	int		colormap;		// entity shirt and pants colors
	int		effects;		// light, particles, etc
	int		skinnum;		// for Alias models
	int		flags;			// render flags

	// these are copied from the persistent data
	int		frame1;			// frame that the model is interpolating from
	int		frame2;			// frame that the model is interpolating to
	double	framelerp;		// interpolation factor, usually computed from frame2time
	double	frame1time;		// time frame1 began playing (for framegroup animations)
	double	frame2time;		// time frame2 began playing (for framegroup animations)

	// calculated by the renderer (but not persistent)
	int		visframe;		// if visframe == r_framecount, it is visible
	vec3_t	mins, maxs;		// calculated during R_AddModelEntities
	frameblend_t	frameblend[4]; // 4 frame numbers (-1 if not used) and their blending scalers (0-1), if interpolation is not desired, use frame instead
}
entity_render_t;

typedef struct entity_persistent_s
{
	// particles
	vec3_t	trail_origin;	// trail rendering
	float	trail_time;		// trail rendering

	// interpolated animation
	int		modelindex;		// lerp resets when model changes
	int		frame1;			// frame that the model is interpolating from
	int		frame2;			// frame that the model is interpolating to
	double	framelerp;		// interpolation factor, usually computed from frame2time
	double	frame1time;		// time frame1 began playing (for framegroup animations)
	double	frame2time;		// time frame2 began playing (for framegroup animations)
}
entity_persistent_t;

typedef struct entity_s
{
	entity_state_t state_baseline;	// baseline state (default values)
	entity_state_t state_previous;	// previous state (interpolating from this)
	entity_state_t state_current;	// current state (interpolating to this)

	entity_persistent_t persistent; // used for regenerating parts of render

	entity_render_t render; // the only data the renderer should know about
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

#define	MAX_BEAMS	24
typedef struct
{
	int		entity;
	struct model_s	*model;
	float	endtime;
	vec3_t	start, end;
}
beam_t;

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
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct
{
	cactive_t	state;

// personalization data sent to server
	char		mapstring[MAX_QPATH];
	char		spawnparms[MAX_MAPSTRING];	// to restart a level

// demo loop control
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	int			forcetrack;			// -1 = use normal cd track
	QFile		*demofile;
	int			td_lastframe;		// to meter out one message a frame
	int			td_startframe;		// host_framecount at start
	double		td_starttime;		// realtime at second frame of timedemo (LordHavoc: changed to double)
	qboolean	demopaused;			// LordHavoc: pausedemo


// connection information
	int			signon;			// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t	message;		// writing buffer to send to server
}
client_static_t;

extern client_static_t	cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
	int			movemessages;	// since connecting to this server
								// throw out the first couple, so the player
								// doesn't accidentally do something the
								// first frame
	usercmd_t	cmd;			// last command sent to the server

// information for local display
	int			stats[MAX_CL_STATS];	// health, etc
	int			items;			// inventory bit flags
	float		item_gettime[32];	// cl.time of acquiring item, for blinking
	float		faceanimtime;	// use anim frame if cl.time < this

	cshift_t	cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t	prev_cshifts[NUM_CSHIFTS];	// and content types

// the client maintains its own idea of view angles, which are
// sent to the server each frame.  The server sets punchangle when
// the view is temporarliy offset, and an angle reset commands at the start
// of each level and after teleporting.
	vec3_t		mviewangles[2];	// during demo playback viewangles is lerped
								// between these
	vec3_t		viewangles;

	vec3_t		mvelocity[2];	// update by server, used for lean+bob
								// (0 is newest)
	vec3_t		velocity;		// lerped between mvelocity[0] and [1]

	vec3_t		punchangle;		// temporary offset
	vec3_t		punchvector;	// LordHavoc: origin view kick

// pitch drifting vars
	float		idealpitch;
	float		pitchvel;
	qboolean	nodrift;
	float		driftmove;
	double		laststop;

	float		viewheight;
	float		crouch;			// local amount for smoothing stepups

	qboolean	paused;			// send over by server
	qboolean	onground;
	qboolean	inwater;

	int			intermission;	// don't change view angle, full screen, etc
	int			completed_time;	// latched at intermission start

	double		mtime[2];		// the timestamp of last two messages
	double		time;			// clients view of time, should be between
								// servertime and oldservertime to generate
								// a lerp point for other data
	double		oldtime;		// previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups

	double		frametime;


	float		last_received_message;	// (realtime) for net trouble icon

//
// information that is static for the entire time connected to a server
//
	struct model_s		*model_precache[MAX_MODELS];
	struct sfx_s		*sound_precache[MAX_SOUNDS];

	char		levelname[40];	// for display on solo scoreboard
	int			viewentity;		// cl_entitites[cl.viewentity] = player
	int			maxclients;
	int			gametype;

// refresh related state
	struct model_s	*worldmodel;	// cl_entitites[0].model
//	int			num_entities;	// held in cl_entities array
	int			num_statics;	// held in cl_staticentities array
	entity_t	viewent;			// the gun model

	int			cdtrack, looptrack;	// cd audio

// frag scoreboard
	scoreboard_t	*scores;		// [cl.maxclients]
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


// LordHavoc: raised these from 64 and 128 to 512 and 256
#define	MAX_TEMP_ENTITIES	512			// lightning bolts, effects, etc
#define	MAX_STATIC_ENTITIES	256			// torches, etc

extern client_state_t cl;

// FIXME, allocate dynamically
extern	entity_t		cl_entities[MAX_EDICTS];
extern	entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
extern	lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
extern	entity_t		cl_temp_entities[MAX_TEMP_ENTITIES];
extern	beam_t			cl_beams[MAX_BEAMS];

#include "cl_light.h"

//=============================================================================

//
// cl_main
//

void CL_Init (void);

void CL_EstablishConnection (char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);

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
void CL_UpdateTEnts (void);

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
void CL_BitProfile_f(void);

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

#define PARTICLE_INVALID 0
#define PARTICLE_BILLBOARD 1
#define PARTICLE_UPRIGHT_FACING 2
#define PARTICLE_ORIENTED_DOUBLESIDED 3

typedef struct renderparticle_s
{
	int tex;
	int orientation;
	int additive;
	int dynlight;
	float scalex;
	float scaley;
	float org[3];
	float dir[3];
	float color[4];
}
renderparticle_t;

void CL_Particles_Clear(void);
void CL_Particles_Init(void);

void CL_ParseParticleEffect (void);
void CL_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void CL_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent);
void CL_RocketTrail2 (vec3_t start, vec3_t end, int color, entity_t *ent);
void CL_SparkShower (vec3_t org, vec3_t dir, int count);
void CL_PlasmaBurn (vec3_t org);
void CL_BloodPuff (vec3_t org, vec3_t vel, int count);
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

// if contents is not zero, it will impact on content changes
// (leafs matching contents are considered empty, others are solid)
extern int traceline_endcontents; // set by TraceLine
// need to call this sometime before using TraceLine with hitbmodels
void TraceLine_ScanForBModels(void);
float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int contents, int hitbmodels);

#include "cl_screen.h"

#define MAX_VISEDICTS (MAX_EDICTS + MAX_STATIC_ENTITIES + MAX_TEMP_ENTITIES)

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

	int numentities;
	entity_render_t **entities;

	int numparticles;
	struct renderparticle_s *particles;

	byte drawqueue[MAX_DRAWQUEUE];
	int drawqueuesize;
}
refdef_t;

refdef_t r_refdef;

extern mempool_t *cl_refdef_mempool;

#include "cgamevm.h"
