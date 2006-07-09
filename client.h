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

// flags for rtlight rendering
#define LIGHTFLAG_NORMALMODE 1
#define LIGHTFLAG_REALTIMEMODE 2

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

typedef struct beam_s
{
	int		entity;
	// draw this as lightning polygons, or a model?
	int		lightning;
	struct model_s	*model;
	float	endtime;
	vec3_t	start, end;
}
beam_t;

typedef struct rtlight_s
{
	// shadow volumes are done entirely in model space, so there are no matrices for dealing with them...  they just use the origin

	// note that the world to light matrices are inversely scaled (divided) by lightradius

	// core properties
	// matrix for transforming world coordinates to light filter coordinates
	matrix4x4_t matrix_worldtolight;
	// typically 1 1 1, can be lower (dim) or higher (overbright)
	vec3_t color;
	// size of the light (remove?)
	vec_t radius;
	// light filter
	char cubemapname[64];
	// light style to monitor for brightness
	int style;
	// whether light should render shadows
	int shadow;
	// intensity of corona to render
	vec_t corona;
	// radius scale of corona to render (1.0 means same as light radius)
	vec_t coronasizescale;
	// ambient intensity to render
	vec_t ambientscale;
	// diffuse intensity to render
	vec_t diffusescale;
	// specular intensity to render
	vec_t specularscale;
	// LIGHTFLAG_* flags
	int flags;

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

	// rendering properties, updated each time a light is rendered
	// this is rtlight->color * d_lightstylevalue
	vec3_t currentcolor;
	// this is R_Shadow_Cubemap(rtlight->cubemapname)
	rtexture_t *currentcubemap;

	// static light info
	// true if this light should be compiled as a static light
	int isstatic;
	// true if this is a compiled world light, cleared if the light changes
	int compiled;
	// premade shadow volumes to render for world entity
	shadowmesh_t *static_meshchain_shadow;
	// used for visibility testing (more exact than bbox)
	int static_numleafs;
	int static_numleafpvsbytes;
	int *static_leaflist;
	unsigned char *static_leafpvs;
	// surfaces seen by light
	int static_numsurfaces;
	int *static_surfacelist;
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
	// radius scale of corona to render (1.0 means same as light radius)
	// (worldlight: saved to .rtlights file)
	vec_t coronasizescale;
	// ambient intensity to render
	// (worldlight: saved to .rtlights file)
	vec_t ambientscale;
	// diffuse intensity to render
	// (worldlight: saved to .rtlights file)
	vec_t diffusescale;
	// specular intensity to render
	// (worldlight: saved to .rtlights file)
	vec_t specularscale;
	// LIGHTFLAG_* flags
	// (worldlight: saved to .rtlights file)
	int flags;
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
	// entity shirt and pants colors (-1 if not colormapped)
	int colormap;
	// literal colors for renderer
	vec3_t colormap_pantscolor;
	vec3_t colormap_shirtcolor;
	// light, particles, etc
	int effects;
	// for Alias models
	int skinnum;
	// render flags
	int flags;

	// colormod tinting of models
	float colormod[3];

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

	// calculated during R_AddModelEntities
	vec3_t mins, maxs;
	// 4 frame numbers (-1 if not used) and their blending scalers (0-1), if interpolation is not desired, use frame instead
	frameblend_t frameblend[4];

	// current lighting from map
	vec3_t modellight_ambient;
	vec3_t modellight_diffuse; // q3bsp
	vec3_t modellight_lightdir; // q3bsp
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
	qboolean csqc;
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

typedef struct usercmd_s
{
	vec3_t	viewangles;

// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;

	vec3_t	cursor_screen;
	vec3_t	cursor_start;
	vec3_t	cursor_end;
	vec3_t	cursor_impact;
	vec3_t	cursor_normal;
	vec_t	cursor_fraction;
	int		cursor_entitynumber;

	double time;
	double receivetime;
	int buttons;
	int impulse;
	int sequence;
	qboolean applied; // if false we're still accumulating a move
} usercmd_t;

typedef struct lightstyle_s
{
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct scoreboard_s
{
	char	name[MAX_SCOREBOARDNAME];
	int		frags;
	int		colors; // two 4 bit fields
	// QW fields:
	int		qw_userid;
	char	qw_userinfo[MAX_USERINFO_STRING];
	float	qw_entertime;
	int		qw_ping;
	int		qw_packetloss;
	int		qw_spectator;
	char	qw_team[8];
	char	qw_skin[MAX_QPATH];
} scoreboard_t;

typedef struct cshift_s
{
	float	destcolor[3];
	float	percent;		// 0-256
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

typedef enum cactive_e
{
	ca_dedicated, 		// a dedicated server with no ability to start a client
	ca_disconnected, 	// full screen console with no connection
	ca_connected		// valid netcon, talking to a server
}
cactive_t;

typedef enum qw_downloadtype_e
{
	dl_none,
	dl_single,
	dl_skin,
	dl_model,
	dl_sound
}
qw_downloadtype_t;

typedef enum capturevideoformat_e
{
	CAPTUREVIDEOFORMAT_TARGA,
	CAPTUREVIDEOFORMAT_JPEG,
	CAPTUREVIDEOFORMAT_RAWRGB,
	CAPTUREVIDEOFORMAT_RAWYV12
}
capturevideoformat_t;

//
// the client_static_t structure is persistent through an arbitrary number
// of server connections
//
typedef struct client_static_s
{
	cactive_t state;

	// all client memory allocations go in these pools
	mempool_t *levelmempool;
	mempool_t *permanentmempool;

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
	// protocol version of the server we're connected to
	// (kept outside client_state_t because it's used between levels)
	protocolversion_t protocol;

// connection information
	// 0 to SIGNONS
	int signon;
	// network connection
	netconn_t *netcon;

	// quakeworld stuff below

	// value of "qport" cvar at time of connection
	int qw_qport;
	// copied from cls.netcon->qw. variables every time they change, or set by demos (which have no cls.netcon)
	int qw_incoming_sequence;
	int qw_outgoing_sequence;

	// current file download buffer (only saved when file is completed)
	char qw_downloadname[MAX_QPATH];
	unsigned char *qw_downloadmemory;
	int qw_downloadmemorycursize;
	int qw_downloadmemorymaxsize;
	int qw_downloadnumber;
	int qw_downloadpercent;
	qw_downloadtype_t qw_downloadtype;

	// current file upload buffer (for uploading screenshots to server)
	unsigned char *qw_uploaddata;
	int qw_uploadsize;
	int qw_uploadpos;

	// user infostring
	// this normally contains the following keys in quakeworld:
	// password spectator name team skin topcolor bottomcolor rate noaim msg *ver *ip
	char userinfo[MAX_USERINFO_STRING];

	// video capture stuff
	qboolean capturevideo_active;
	capturevideoformat_t capturevideo_format;
	double capturevideo_starttime;
	double capturevideo_framerate;
	int capturevideo_soundrate;
	int capturevideo_frame;
	unsigned char *capturevideo_buffer;
	qfile_t *capturevideo_videofile;
	qfile_t *capturevideo_soundfile;
	short capturevideo_rgbtoyuvscaletable[3][3][256];
	unsigned char capturevideo_yuvnormalizetable[3][256];
	char capturevideo_basename[64];
}
client_static_t;

extern client_static_t	cls;

typedef struct client_movementqueue_s
{
	double time;
	float frametime;
	int sequence;
	float viewangles[3];
	float move[3];
	qboolean jump;
	qboolean crouch;
}
client_movementqueue_t;

//[515]: csqc
typedef struct
{
	qboolean drawworld;
	qboolean drawenginesbar;
	qboolean drawcrosshair;
}csqc_vidvars_t;

typedef struct qw_usercmd_s
{
	vec3_t angles;
	short forwardmove, sidemove, upmove;
	unsigned char padding1[2];
	unsigned char msec;
	unsigned char buttons;
	unsigned char impulse;
	unsigned char padding2;
}
qw_usercmd_t;

typedef enum
{
	PARTICLE_BILLBOARD = 0,
	PARTICLE_SPARK = 1,
	PARTICLE_ORIENTED_DOUBLESIDED = 2,
	PARTICLE_BEAM = 3
}
porientation_t;

typedef enum
{
	PBLEND_ALPHA = 0,
	PBLEND_ADD = 1,
	PBLEND_MOD = 2
}
pblend_t;

typedef struct particletype_s
{
	pblend_t blendmode;
	porientation_t orientation;
	qboolean lighting;
}
particletype_t;

typedef enum
{
	pt_alphastatic, pt_static, pt_spark, pt_beam, pt_rain, pt_raindecal, pt_snow, pt_bubble, pt_blood, pt_smoke, pt_decal, pt_entityparticle, pt_total
}
ptype_t;

typedef struct particle_s
{
	particletype_t *type;
	int			texnum;
	vec3_t		org;
	vec3_t		vel; // velocity of particle, or orientation of decal, or end point of beam
	float		size;
	float		sizeincrease; // rate of size change per second
	float		alpha; // 0-255
	float		alphafade; // how much alpha reduces per second
	float		time2; // used for snow fluttering and decal fade
	float		bounce; // how much bounce-back from a surface the particle hits (0 = no physics, 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
	float		gravity; // how much gravity affects this particle (1.0 = normal gravity, 0.0 = none)
	float		airfriction; // how much air friction affects this object (objects with a low mass/size ratio tend to get more air friction)
	float		liquidfriction; // how much liquid friction affects this object (objects with a low mass/size ratio tend to get more liquid friction)
	unsigned char		color[4];
	unsigned short owner; // decal stuck to this entity
	model_t		*ownermodel; // model the decal is stuck to (used to make sure the entity is still alive)
	vec3_t		relativeorigin; // decal at this location in entity's coordinate space
	vec3_t		relativedirection; // decal oriented this way relative to entity's coordinate space
}
particle_t;

typedef enum cl_parsingtextmode_e
{
	CL_PARSETEXTMODE_NONE,
	CL_PARSETEXTMODE_PING,
	CL_PARSETEXTMODE_STATUS,
	CL_PARSETEXTMODE_STATUS_PLAYERID,
	CL_PARSETEXTMODE_STATUS_PLAYERIP
}
cl_parsingtextmode_t;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct client_state_s
{
	// true if playing in a local game and no one else is connected
	int islocalgame;

	// when connecting to the server throw out the first couple move messages
	// so the player doesn't accidentally do something the first frame
	int movemessages;

	// send a clc_nop periodically until connected
	float sendnoptime;

	// current input to send to the server
	usercmd_t cmd;

// information for local display
	// health, etc
	int stats[MAX_CL_STATS];
	// last known inventory bit flags, for blinking
	int olditems;
	// cl.time of acquiring item, for blinking
	float item_gettime[32];
	// last known STAT_ACTIVEWEAPON
	int activeweapon;
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

	// mviewangles is read from demo
	// viewangles is either client controlled or lerped from mviewangles
	vec3_t mviewangles[2], viewangles;
	// update by server, used by qc to do weapon recoil
	vec3_t mpunchangle[2], punchangle;
	// update by server, can be used by mods to kick view around
	vec3_t mpunchvector[2], punchvector;
	// update by server, used for lean+bob (0 is newest)
	vec3_t mvelocity[2], velocity;
	// update by server, can be used by mods for zooming
	vec_t mviewzoom[2], viewzoom;
	// if true interpolation the mviewangles and other interpolation of the
	// player is disabled until the next network packet
	// this is used primarily by teleporters, and when spectating players
	// special checking of the old fixangle[1] is used to differentiate
	// between teleporting and spectating
	qboolean fixangle[2];

	// client movement simulation
	// these fields are only updated by CL_ClientMovement (called by CL_SendMove after parsing each network packet)
	// set by CL_ClientMovement_Replay functions
	qboolean movement_predicted;
	// this is set true by svc_time parsing and causes a new movement to be
	// queued for prediction purposes
	qboolean movement_needupdate;
	// indicates the queue has been updated and should be replayed
	qboolean movement_replay;
	// timestamps of latest two predicted moves for interpolation
	double movement_time[2];
	// simulated data (this is valid even if cl.movement is false)
	vec3_t movement_origin;
	vec3_t movement_oldorigin;
	vec3_t movement_velocity;
	// queue of proposed moves
	int movement_numqueue;
	client_movementqueue_t movement_queue[256];
	int movesequence;
	int servermovesequence;
	// whether the replay should allow a jump at the first sequence
	qboolean movement_replay_canjump;

// pitch drifting vars
	float idealpitch;
	float pitchvel;
	qboolean nodrift;
	float driftmove;
	double laststop;

//[515]: added for csqc purposes
	float sensitivityscale;
	csqc_vidvars_t csqc_vidvars;	//[515]: these parms must be set to true by default
	qboolean csqc_wantsmousemove;
	struct model_s *csqc_model_precache[MAX_MODELS];

	// local amount for smoothing stepups
	//float crouch;

	// sent by server
	qboolean paused;
	qboolean onground;
	qboolean inwater;

	// used by bob
	qboolean oldonground;
	double lastongroundtime;
	double hitgroundtime;

	// don't change view angle, full screen, etc
	int intermission;
	// latched at intermission start
	double completed_time;

	// the timestamp of the last two messages
	double mtime[2];

	// clients view of time, time should be between mtime[0] and mtime[1] to
	// generate a lerp point for other data, oldtime is the previous frame's
	// value of time, frametime is the difference between time and oldtime
	double time, oldtime;
	// how long it has been since the previous client frame in real time
	// (not game time, for that use cl.time - cl.oldtime)
	double realframetime;

	// copy of realtime from last recieved message, for net trouble icon
	float last_received_message;

// information that is static for the entire time connected to a server
	struct model_s *model_precache[MAX_MODELS];
	struct sfx_s *sound_precache[MAX_SOUNDS];

	// FIXME: this is a lot of memory to be keeping around, this really should be dynamically allocated and freed somehow
	char model_name[MAX_MODELS][MAX_QPATH];
	char sound_name[MAX_SOUNDS][MAX_QPATH];

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

	// models and sounds used by engine code (particularly cl_parse.c)
	model_t *model_bolt;
	model_t *model_bolt2;
	model_t *model_bolt3;
	model_t *model_beam;
	sfx_t *sfx_wizhit;
	sfx_t *sfx_knighthit;
	sfx_t *sfx_tink1;
	sfx_t *sfx_ric1;
	sfx_t *sfx_ric2;
	sfx_t *sfx_ric3;
	sfx_t *sfx_r_exp3;

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

	// keep track of svc_print parsing state (analyzes ping reports and status reports)
	cl_parsingtextmode_t parsingtextmode;
	int parsingtextplayerindex;

	// entity database stuff
	// latest received entity frame numbers
#define LATESTFRAMENUMS 3
	int latestframenums[LATESTFRAMENUMS];
	entityframe_database_t *entitydatabase;
	entityframe4_database_t *entitydatabase4;
	entityframeqw_database_t *entitydatabaseqw;

	// keep track of quake entities because they need to be killed if they get stale
	int lastquakeentity;
	unsigned char isquakeentity[MAX_EDICTS];

	// bounding boxes for clientside movement
	vec3_t playerstandmins;
	vec3_t playerstandmaxs;
	vec3_t playercrouchmins;
	vec3_t playercrouchmaxs;

	int max_entities;
	int max_csqcentities;
	int max_static_entities;
	int max_temp_entities;
	int max_effects;
	int max_beams;
	int max_dlights;
	int max_lightstyle;
	int max_brushmodel_entities;
	int max_particles;

	entity_t *entities;
	entity_t *csqcentities;	//[515]: csqc
	unsigned char *entities_active;
	unsigned char *csqcentities_active;	//[515]: csqc
	entity_t *static_entities;
	entity_t *temp_entities;
	cl_effect_t *effects;
	beam_t *beams;
	dlight_t *dlights;
	lightstyle_t *lightstyle;
	int *brushmodel_entities;
	particle_t *particles;

	int num_entities;
	int num_csqcentities;	//[515]: csqc
	int num_static_entities;
	int num_temp_entities;
	int num_brushmodel_entities;
	int num_effects;
	int num_beams;
	int num_dlights;
	int num_particles;

	int free_particle;

	// quakeworld stuff

	// local copy of the server infostring
	char qw_serverinfo[MAX_SERVERINFO_STRING];

	// time of last qw "pings" command sent to server while showing scores
	double last_ping_request;

	// used during connect
	int qw_servercount;

	// updated from serverinfo
	int qw_teamplay;

	// indicates whether the player is spectating
	qboolean qw_spectator;

	// movement parameters for client prediction
	float qw_movevars_gravity;
	float qw_movevars_stopspeed;
	float qw_movevars_maxspeed; // can change during play
	float qw_movevars_spectatormaxspeed;
	float qw_movevars_accelerate;
	float qw_movevars_airaccelerate;
	float qw_movevars_wateraccelerate;
	float qw_movevars_friction;
	float qw_movevars_waterfriction;
	float qw_movevars_entgravity; // can change during play

	// models used by qw protocol
	int qw_modelindex_spike;
	int qw_modelindex_player;
	int qw_modelindex_flag;
	int qw_modelindex_s_explod;

	vec3_t qw_intermission_origin;
	vec3_t qw_intermission_angles;

	// 255 is the most nails the QW protocol could send
	int qw_num_nails;
	vec_t qw_nails[255][6];

	float qw_weaponkick;

	int qw_validsequence;

	qw_usercmd_t qw_moves[QW_UPDATE_BACKUP];

	int qw_deltasequence[QW_UPDATE_BACKUP];
}
client_state_t;

//
// cvars
//
extern cvar_t cl_name;
extern cvar_t cl_color;
extern cvar_t cl_rate;
extern cvar_t cl_pmodel;
extern cvar_t cl_playermodel;
extern cvar_t cl_playerskin;

extern cvar_t rcon_password;
extern cvar_t rcon_address;

extern cvar_t cl_upspeed;
extern cvar_t cl_forwardspeed;
extern cvar_t cl_backspeed;
extern cvar_t cl_sidespeed;

extern cvar_t cl_movespeedkey;

extern cvar_t cl_yawspeed;
extern cvar_t cl_pitchspeed;

extern cvar_t cl_anglespeedkey;

extern cvar_t cl_autofire;

extern cvar_t csqc_progname;	//[515]: csqc crc check and right csprogs name according to progs.dat
extern cvar_t csqc_progcrc;

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

extern cvar_t cl_autodemo;
extern cvar_t cl_autodemo_nameformat;

extern cvar_t r_draweffects;

extern cvar_t cl_explosions_alpha_start;
extern cvar_t cl_explosions_alpha_end;
extern cvar_t cl_explosions_size_start;
extern cvar_t cl_explosions_size_end;
extern cvar_t cl_explosions_lifetime;
extern cvar_t cl_stainmaps;
extern cvar_t cl_stainmaps_clearonload;

extern cvar_t cl_prydoncursor;

extern client_state_t cl;

extern void CL_AllocDlight (entity_render_t *ent, matrix4x4_t *matrix, float radius, float red, float green, float blue, float decay, float lifetime, int cubemapnum, int style, int shadowenable, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags);

//=============================================================================

//
// cl_main
//

void CL_Shutdown (void);
void CL_Init (void);

void CL_EstablishConnection(const char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);

void CL_BoundingBoxForEntity(entity_render_t *ent);

//
// cl_input
//
typedef struct kbutton_s
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
}
kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;

void CL_InitInput (void);
void CL_SendMove (void);

void CL_ValidateState(entity_state_t *s);
void CL_MoveLerpEntityStates(entity_t *ent);
void CL_LerpUpdate(entity_t *e);
void CL_ParseTEnt (void);
void CL_NewBeam (int ent, vec3_t start, vec3_t end, model_t *m, int lightning);
void CL_RelinkBeams (void);
void CL_Beam_CalculatePositions (const beam_t *b, vec3_t start, vec3_t end);

void CL_ClearTempEntities (void);
entity_t *CL_NewTempEntity (void);

void CL_Effect(vec3_t org, int modelindex, int startframe, int framecount, float framerate);

void CL_ClearState (void);
void CL_ExpandEntities(int num);
void CL_SetInfo(const char *key, const char *value, qboolean send, qboolean allowstarkey, qboolean allowmodel, qboolean quiet);


int  CL_ReadFromServer (void);
void CL_WriteToServer (void);
void CL_Move (void);
extern qboolean cl_ignoremousemove;


float CL_KeyState (kbutton_t *key);
const char *Key_KeynumToString (int keynum);
int Key_StringToKeynum (const char *str);

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
void CL_Parse_Shutdown(void);
void CL_ParseServerMessage(void);
void CL_Parse_DumpPacket(void);
void CL_Parse_ErrorCleanUp(void);
void QW_CL_StartUpload(unsigned char *data, int size);
extern cvar_t qport;

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
// cl_part
//

extern cvar_t cl_particles;
extern cvar_t cl_particles_quality;
extern cvar_t cl_particles_size;
extern cvar_t cl_particles_quake;
extern cvar_t cl_particles_blood;
extern cvar_t cl_particles_blood_alpha;
extern cvar_t cl_particles_blood_bloodhack;
extern cvar_t cl_particles_bulletimpacts;
extern cvar_t cl_particles_explosions_bubbles;
extern cvar_t cl_particles_explosions_smoke;
extern cvar_t cl_particles_explosions_sparks;
extern cvar_t cl_particles_explosions_shell;
extern cvar_t cl_particles_smoke;
extern cvar_t cl_particles_smoke_alpha;
extern cvar_t cl_particles_smoke_alphafade;
extern cvar_t cl_particles_sparks;
extern cvar_t cl_particles_bubbles;
extern cvar_t cl_decals;
extern cvar_t cl_decals_time;
extern cvar_t cl_decals_fadetime;

void CL_Particles_Clear(void);
void CL_Particles_Init(void);
void CL_Particles_Shutdown(void);

typedef enum effectnameindex_s
{
	EFFECT_NONE,
	EFFECT_TE_GUNSHOT,
	EFFECT_TE_GUNSHOTQUAD,
	EFFECT_TE_SPIKE,
	EFFECT_TE_SPIKEQUAD,
	EFFECT_TE_SUPERSPIKE,
	EFFECT_TE_SUPERSPIKEQUAD,
	EFFECT_TE_WIZSPIKE,
	EFFECT_TE_KNIGHTSPIKE,
	EFFECT_TE_EXPLOSION,
	EFFECT_TE_EXPLOSIONQUAD,
	EFFECT_TE_TAREXPLOSION,
	EFFECT_TE_TELEPORT,
	EFFECT_TE_LAVASPLASH,
	EFFECT_TE_SMALLFLASH,
	EFFECT_TE_FLAMEJET,
	EFFECT_EF_FLAME,
	EFFECT_TE_BLOOD,
	EFFECT_TE_SPARK,
	EFFECT_TE_PLASMABURN,
	EFFECT_TE_TEI_G3,
	EFFECT_TE_TEI_SMOKE,
	EFFECT_TE_TEI_BIGEXPLOSION,
	EFFECT_TE_TEI_PLASMAHIT,
	EFFECT_EF_STARDUST,
	EFFECT_TR_ROCKET,
	EFFECT_TR_GRENADE,
	EFFECT_TR_BLOOD,
	EFFECT_TR_WIZSPIKE,
	EFFECT_TR_SLIGHTBLOOD,
	EFFECT_TR_KNIGHTSPIKE,
	EFFECT_TR_VORESPIKE,
	EFFECT_TR_NEHAHRASMOKE,
	EFFECT_TR_NEXUIZPLASMA,
	EFFECT_TR_GLOWTRAIL,
	EFFECT_SVC_PARTICLE,
	EFFECT_TOTAL
}
effectnameindex_t;

int CL_ParticleEffectIndexForName(const char *name);
const char *CL_ParticleEffectNameForIndex(int i);
void CL_ParticleEffect(int effectindex, float pcount, const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, entity_t *ent, int palettecolor);
void CL_ParseParticleEffect (void);
void CL_ParticleCube (const vec3_t mins, const vec3_t maxs, const vec3_t dir, int count, int colorbase, vec_t gravity, vec_t randomvel);
void CL_ParticleRain (const vec3_t mins, const vec3_t maxs, const vec3_t dir, int count, int colorbase, int type);
void CL_EntityParticles (const entity_t *ent);
void CL_ParticleExplosion (const vec3_t org);
void CL_ParticleExplosion2 (const vec3_t org, int colorStart, int colorLength);
void CL_MoveParticles(void);
void R_MoveExplosions(void);
void R_NewExplosion(const vec3_t org);

#include "cl_screen.h"

extern qboolean sb_showscores;

#define NUMCROSSHAIRS 32
extern cachepic_t *r_crosshairs[NUMCROSSHAIRS+1];

#define FOGTABLEWIDTH 1024
extern int fogtableindex;
#define VERTEXFOGTABLE(dist) (fogtableindex = (int)((dist) * r_refdef.fogtabledistmultiplier), r_refdef.fogtable[bound(0, fogtableindex, FOGTABLEWIDTH - 1)])

typedef struct r_refdef_stats_s
{
	int entities;
	int entities_surfaces;
	int entities_triangles;
	int world_leafs;
	int world_portals;
	int particles;
	int meshes;
	int meshes_elements;
	int lights;
	int lights_clears;
	int lights_scissored;
	int lights_lighttriangles;
	int lights_shadowtriangles;
	int lights_dynamicshadowtriangles;
	int bloom;
	int bloom_copypixels;
	int bloom_drawpixels;
}
r_refdef_stats_t;

typedef struct r_refdef_s
{
	// these fields define the basic rendering information for the world
	// but not the view, which could change multiple times in one rendered
	// frame (for example when rendering textures for certain effects)

	// these are set for water warping before
	// frustum_x/frustum_y are calculated
	float frustumscale_x, frustumscale_y;

	// minimum visible distance (pixels closer than this disappear)
	double nearclip;
	// maximum visible distance (pixels further than this disappear in 16bpp modes,
	// in 32bpp an infinite-farclip matrix is used instead)
	double farclip;

	// fullscreen color blend
	float viewblend[4];

	// whether to call S_ExtraUpdate during render to reduce sound chop
	qboolean extraupdate;

	// client gameworld time for rendering time based effects
	double time;

	// the world
	entity_render_t *worldentity;

	// same as worldentity->model
	model_t *worldmodel;

	// renderable entities (excluding world)
	entity_render_t **entities;
	int numentities;
	int maxentities;

	// renderable dynamic lights
	dlight_t *lights[MAX_DLIGHTS];
	int numlights;

	// 8.8bit fixed point intensities for light styles
	// controls intensity of dynamic lights and lightmap layers
	unsigned short	lightstylevalue[256];	// 8.8 fraction of base light value

	vec3_t fogcolor;
	vec_t fogrange;
	vec_t fograngerecip;
	vec_t fogtabledistmultiplier;
	float fogtable[FOGTABLEWIDTH];
	float fog_density;
	float fog_red;
	float fog_green;
	float fog_blue;
	qboolean fogenabled;
	qboolean oldgl_fogenable;

	qboolean draw2dstage;

	// true during envmap command capture
	qboolean envmap;

	// brightness of world lightmaps and related lighting
	// (often reduced when world rtlights are enabled)
	float lightmapintensity;
	// whether to draw world lights realtime, dlights realtime, and their shadows
	qboolean rtworld;
	qboolean rtworldshadows;
	qboolean rtdlight;
	qboolean rtdlightshadows;
	float polygonfactor;
	float polygonoffset;
	float shadowpolygonfactor;
	float shadowpolygonoffset;

	// rendering stats for r_speeds display
	// (these are incremented in many places)
	r_refdef_stats_t stats;
}
r_refdef_t;

typedef struct r_view_s
{
	// view information (changes multiple times per frame)
	// if any of these variables change then r_viewcache must be regenerated
	// by calling R_View_Update
	// (which also updates viewport, scissor, colormask)

	// it is safe and expected to copy this into a structure on the stack and
	// call the renderer recursively, then restore from the stack afterward
	// (as long as R_View_Update is called)

	// eye position information
	matrix4x4_t matrix;
	vec3_t origin;
	vec3_t forward;
	vec3_t left;
	vec3_t right;
	vec3_t up;
	mplane_t frustum[5];
	float frustum_x, frustum_y;

	// screen area to render in
	int x;
	int y;
	int z;
	int width;
	int height;
	int depth;

	// which color components to allow (for anaglyph glasses)
	int colormask[4];

	// global RGB color multiplier for rendering, this is required by HDR
	float colorscale;
}
r_view_t;

typedef struct r_viewcache_s
{
	// these properties are generated by R_View_Update()

	// which entities are currently visible for this viewpoint
	// (the used range is 0...r_refdef.numentities)
	unsigned char entityvisible[MAX_EDICTS];
	// flag arrays used for visibility checking on world model
	// (all other entities have no per-surface/per-leaf visibility checks)
	// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_clusters
	unsigned char world_pvsbits[(32768+7)>>3];
	// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_leafs
	unsigned char world_leafvisible[32768];
	// TODO: dynamic resize according to r_refdef.worldmodel->num_surfaces
	unsigned char world_surfacevisible[262144];
	// if true, the view is currently in a leaf without pvs data
	qboolean world_novis;
}
r_viewcache_t;

extern r_refdef_t r_refdef;
extern r_view_t r_view;
extern r_viewcache_t r_viewcache;

#endif

