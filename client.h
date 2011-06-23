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
#include "snd_main.h"

// flags for rtlight rendering
#define LIGHTFLAG_NORMALMODE 1
#define LIGHTFLAG_REALTIMEMODE 2

typedef struct tridecal_s
{
	// color and initial alpha value
	float			texcoord2f[3][2];
	float			vertex3f[3][3];
	float			color4f[3][4];
	float			plane[4]; // backface culling
	// how long this decal has lived so far (the actual fade begins at cl_decals_time)
	float			lived;
	// if >= 0 this indicates the decal should follow an animated triangle
	int				triangleindex;
	// for visibility culling
	int				surfaceindex;
	// old decals are killed to obey cl_decals_max
	int				decalsequence;
}
tridecal_t;

typedef struct decalsystem_s
{
	dp_model_t *model;
	double lastupdatetime;
	int maxdecals;
	int freedecal;
	int numdecals;
	tridecal_t *decals;
	float *vertex3f;
	float *texcoord2f;
	float *color4f;
	int *element3i;
	unsigned short *element3s;
}
decalsystem_t;

typedef struct effect_s
{
	int active;
	vec3_t origin;
	double starttime;
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

typedef struct rtlight_particle_s
{
	float origin[3];
	float color[3];
}
rtlight_particle_t;

typedef struct rtlight_s
{
	// shadow volumes are done entirely in model space, so there are no matrices for dealing with them...  they just use the origin

	// note that the world to light matrices are inversely scaled (divided) by lightradius

	// core properties
	/// matrix for transforming light filter coordinates to world coordinates
	matrix4x4_t matrix_lighttoworld;
	/// matrix for transforming world coordinates to light filter coordinates
	matrix4x4_t matrix_worldtolight;
	/// typically 1 1 1, can be lower (dim) or higher (overbright)
	vec3_t color;
	/// size of the light (remove?)
	vec_t radius;
	/// light filter
	char cubemapname[64];
	/// light style to monitor for brightness
	int style;
	/// whether light should render shadows
	int shadow;
	/// intensity of corona to render
	vec_t corona;
	/// radius scale of corona to render (1.0 means same as light radius)
	vec_t coronasizescale;
	/// ambient intensity to render
	vec_t ambientscale;
	/// diffuse intensity to render
	vec_t diffusescale;
	/// specular intensity to render
	vec_t specularscale;
	/// LIGHTFLAG_* flags
	int flags;

	// generated properties
	/// used only for shadow volumes
	vec3_t shadoworigin;
	/// culling
	vec3_t cullmins;
	vec3_t cullmaxs;
	// culling
	//vec_t cullradius;
	// squared cullradius
	//vec_t cullradius2;

	// rendering properties, updated each time a light is rendered
	// this is rtlight->color * d_lightstylevalue
	vec3_t currentcolor;
	/// used by corona updates, due to occlusion query
	float corona_visibility;
	unsigned int corona_queryindex_visiblepixels;
	unsigned int corona_queryindex_allpixels;
	/// this is R_GetCubemap(rtlight->cubemapname)
	rtexture_t *currentcubemap;
	/// set by R_Shadow_PrepareLight to decide whether R_Shadow_DrawLight should draw it
	qboolean draw;
	/// these fields are set by R_Shadow_PrepareLight for later drawing
	int cached_numlightentities;
	int cached_numlightentities_noselfshadow;
	int cached_numshadowentities;
	int cached_numshadowentities_noselfshadow;
	int cached_numsurfaces;
	struct entity_render_s **cached_lightentities;
	struct entity_render_s **cached_lightentities_noselfshadow;
	struct entity_render_s **cached_shadowentities;
	struct entity_render_s **cached_shadowentities_noselfshadow;
	unsigned char *cached_shadowtrispvs;
	unsigned char *cached_lighttrispvs;
	int *cached_surfacelist;
	// reduced light cullbox from GetLightInfo
	vec3_t cached_cullmins;
	vec3_t cached_cullmaxs;
	// current shadow-caster culling planes based on view
	// (any geometry outside these planes can not contribute to the visible
	//  shadows in any way, and thus can be culled safely)
	int cached_numfrustumplanes;
	mplane_t cached_frustumplanes[5]; // see R_Shadow_ComputeShadowCasterCullingPlanes

	/// static light info
	/// true if this light should be compiled as a static light
	int isstatic;
	/// true if this is a compiled world light, cleared if the light changes
	int compiled;
	/// the shadowing mode used to compile this light
	int shadowmode;
	/// premade shadow volumes to render for world entity
	shadowmesh_t *static_meshchain_shadow_zpass;
	shadowmesh_t *static_meshchain_shadow_zfail;
	shadowmesh_t *static_meshchain_shadow_shadowmap;
	/// used for visibility testing (more exact than bbox)
	int static_numleafs;
	int static_numleafpvsbytes;
	int *static_leaflist;
	unsigned char *static_leafpvs;
	/// surfaces seen by light
	int static_numsurfaces;
	int *static_surfacelist;
	/// flag bits indicating which triangles of the world model should cast
	/// shadows, and which ones should be lit
	///
	/// this avoids redundantly scanning the triangles in each surface twice
	/// for whether they should cast shadows, once in culling and once in the
	/// actual shadowmarklist production.
	int static_numshadowtrispvsbytes;
	unsigned char *static_shadowtrispvs;
	/// this allows the lighting batch code to skip backfaces andother culled
	/// triangles not relevant for lighting
	/// (important on big surfaces such as terrain)
	int static_numlighttrispvsbytes;
	unsigned char *static_lighttrispvs;
	/// masks of all shadowmap sides that have any potential static receivers or casters
	int static_shadowmap_receivers;
	int static_shadowmap_casters;
	/// particle-tracing cache for global illumination
	int particlecache_numparticles;
	int particlecache_maxparticles;
	int particlecache_updateparticle;
	rtlight_particle_t *particlecache_particles;

	/// bouncegrid light info
	float photoncolor[3];
	float photons;
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
	// cubemap name to use on this light
	// (worldlight: saved to .rtlights file)
	char cubemapname[64];
	// make light flash while selected
	// (worldlight only)
	int selected;
	// brightness (not really radius anymore)
	// (worldlight: saved to .rtlights file)
	vec_t radius;
	// drop intensity this much each second
	// (dlight only)
	vec_t decay;
	// intensity value which is dropped over time
	// (dlight only)
	vec_t intensity;
	// initial values for intensity to modify
	// (dlight only)
	vec_t initialradius;
	vec3_t initialcolor;
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
	// (worldlight only)
	rtlight_t rtlight;
}
dlight_t;

// this is derived from processing of the framegroupblend array
// note: technically each framegroupblend can produce two of these, but that
// never happens in practice because no one blends between more than 2
// framegroups at once
#define MAX_FRAMEBLENDS (MAX_FRAMEGROUPBLENDS * 2)
typedef struct frameblend_s
{
	int subframe;
	float lerp;
}
frameblend_t;

// LordHavoc: this struct is intended for the renderer but some fields are
// used by the client.
//
// The renderer should not rely on any changes to this struct to be persistent
// across multiple frames because temp entities are wiped every frame, but it
// is acceptable to cache things in this struct that are not critical.
//
// For example the r_cullentities_trace code does such caching.
typedef struct entity_render_s
{
	// location
	//vec3_t origin;
	// orientation
	//vec3_t angles;
	// transform matrix for model to world
	matrix4x4_t matrix;
	// transform matrix for world to model
	matrix4x4_t inversematrix;
	// opacity (alpha) of the model
	float alpha;
	// size the model is shown
	float scale;
	// transparent sorting offset
	float transparent_offset;

	// NULL = no model
	dp_model_t *model;
	// number of the entity represents, or 0 for non-network entities
	int entitynumber;
	// literal colormap colors for renderer, if both are 0 0 0 it is not colormapped
	vec3_t colormap_pantscolor;
	vec3_t colormap_shirtcolor;
	// light, particles, etc
	int effects;
	// qw CTF flags and other internal-use-only effect bits
	int internaleffects;
	// for Alias models
	int skinnum;
	// render flags
	int flags;

	// colormod tinting of models
	float colormod[3];
	float glowmod[3];

	// interpolated animation - active framegroups and blend factors
	framegroupblend_t framegroupblend[MAX_FRAMEGROUPBLENDS];

	// time of last model change (for shader animations)
	double shadertime;

	// calculated by the renderer (but not persistent)

	// calculated during R_AddModelEntities
	vec3_t mins, maxs;
	// subframe numbers (-1 if not used) and their blending scalers (0-1), if interpolation is not desired, use subframeblend[0].subframe
	frameblend_t frameblend[MAX_FRAMEBLENDS];
	// skeletal animation data (if skeleton.relativetransforms is not NULL, it overrides frameblend)
	skeleton_t *skeleton;

	// animation cache (pointers allocated using R_FrameData_Alloc)
	// ONLY valid during R_RenderView!  may be NULL (not cached)
	float *animcache_vertex3f;
	float *animcache_normal3f;
	float *animcache_svector3f;
	float *animcache_tvector3f;
	// interleaved arrays for rendering and dynamic vertex buffers for them
	r_meshbuffer_t *animcache_vertex3fbuffer;
	r_vertexmesh_t *animcache_vertexmesh;
	r_meshbuffer_t *animcache_vertexmeshbuffer;

	// current lighting from map (updated ONLY by client code, not renderer)
	vec3_t modellight_ambient;
	vec3_t modellight_diffuse; // q3bsp
	vec3_t modellight_lightdir; // q3bsp

	// storage of decals on this entity
	// (note: if allowdecals is set, be sure to call R_DecalSystem_Reset on removal!)
	int allowdecals;
	decalsystem_t decalsystem;

	// FIELDS UPDATED BY RENDERER:
	// last time visible during trace culling
	double last_trace_visibility;

	// user wavefunc parameters (from csqc)
	float userwavefunc_param[Q3WAVEFUNC_USER_COUNT];
}
entity_render_t;

typedef struct entity_persistent_s
{
	vec3_t trail_origin;

	// particle trail
	float trail_time;
	qboolean trail_allowed; // set to false by teleports, true by update code, prevents bad lerps

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

	double time; // time the move is executed for (cl_movement: clienttime, non-cl_movement: receivetime)
	double receivetime; // time the move was received at
	double clienttime; // time to which server state the move corresponds to
	int msec; // for predicted moves
	int buttons;
	int impulse;
	int sequence;
	qboolean applied; // if false we're still accumulating a move
	qboolean predicted; // if true the sequence should be sent as 0

	// derived properties
	double frametime;
	qboolean canjump;
	qboolean jump;
	qboolean crouch;
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
	int		qw_movementloss;
	int		qw_spectator;
	char	qw_team[8];
	char	qw_skin[MAX_QPATH];
} scoreboard_t;

typedef struct cshift_s
{
	float	destcolor[3];
	float	percent;		// 0-255
	float   alphafade;      // (any speed)
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

typedef enum cactive_e
{
	ca_uninitialized,	// during early startup
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
	CAPTUREVIDEOFORMAT_AVI_I420,
	CAPTUREVIDEOFORMAT_OGG_VORBIS_THEORA
}
capturevideoformat_t;

typedef struct capturevideostate_s
{
	double startrealtime;
	double framerate;
	int framestep;
	int framestepframe;
	qboolean active;
	qboolean realtime;
	qboolean error;
	int soundrate;
	int soundchannels;
	int frame;
	double starttime;
	double lastfpstime;
	int lastfpsframe;
	int soundsampleframe;
	unsigned char *screenbuffer;
	unsigned char *outbuffer;
	char basename[MAX_QPATH];
	int width, height;

	// precomputed RGB to YUV tables
	// converts the RGB values to YUV (see cap_avi.c for how to use them)
	short rgbtoyuvscaletable[3][3][256];
	unsigned char yuvnormalizetable[3][256];

	// precomputed gamma ramp (only needed if the capturevideo module uses RGB output)
	// note: to map from these values to RGB24, you have to multiply by 255.0/65535.0, then add 0.5, then cast to integer
	unsigned short vidramp[256 * 3];

	// stuff to be filled in by the video format module
	capturevideoformat_t format;
	const char *formatextension;
	qfile_t *videofile;
		// always use this:
		//   cls.capturevideo.videofile = FS_OpenRealFile(va("%s.%s", cls.capturevideo.basename, cls.capturevideo.formatextension), "wb", false);
	void (*endvideo) (void);
	void (*videoframes) (int num);
	void (*soundframe) (const portable_sampleframe_t *paintbuffer, size_t length);

	// format specific data
	void *formatspecific;
}
capturevideostate_t;

#define CL_MAX_DOWNLOADACKS 4

typedef struct cl_downloadack_s
{
	int start, size;
}
cl_downloadack_t;

typedef struct cl_soundstats_s
{
	int mixedsounds;
	int totalsounds;
	int latency_milliseconds;
}
cl_soundstats_t;

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
	char demoname[MAX_QPATH];

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean demorecording;
	fs_offset_t demo_lastcsprogssize;
	int demo_lastcsprogscrc;
	qboolean demoplayback;
	qboolean timedemo;
	// -1 = use normal cd track
	int forcetrack;
	qfile_t *demofile;
	// realtime at second frame of timedemo (LordHavoc: changed to double)
	double td_starttime;
	int td_frames; // total frames parsed
	double td_onesecondnexttime;
	double td_onesecondframes;
	double td_onesecondrealtime;
	double td_onesecondminfps;
	double td_onesecondmaxfps;
	double td_onesecondavgfps;
	int td_onesecondavgcount;
	// LordHavoc: pausedemo
	qboolean demopaused;

	// sound mixer statistics for showsound display
	cl_soundstats_t soundstats;

	qboolean connect_trying;
	int connect_remainingtries;
	double connect_nextsendtime;
	lhnetsocket_t *connect_mysocket;
	lhnetaddress_t connect_address;
	// protocol version of the server we're connected to
	// (kept outside client_state_t because it's used between levels)
	protocolversion_t protocol;

#define MAX_RCONS 16
	int rcon_trying;
	lhnetaddress_t rcon_addresses[MAX_RCONS];
	char rcon_commands[MAX_RCONS][MAX_INPUTLINE];
	double rcon_timeout[MAX_RCONS];
	int rcon_ringpos;

// connection information
	// 0 to SIGNONS
	int signon;
	// network connection
	netconn_t *netcon;

	// download information
	// (note: qw_download variables are also used)
	cl_downloadack_t dp_downloadack[CL_MAX_DOWNLOADACKS];

	// input sequence numbers are not reset on level change, only connect
	int movesequence;
	int servermovesequence;

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
	// transfer rate display
	double qw_downloadspeedtime;
	int qw_downloadspeedcount;
	int qw_downloadspeedrate;
	qboolean qw_download_deflate;

	// current file upload buffer (for uploading screenshots to server)
	unsigned char *qw_uploaddata;
	int qw_uploadsize;
	int qw_uploadpos;

	// user infostring
	// this normally contains the following keys in quakeworld:
	// password spectator name team skin topcolor bottomcolor rate noaim msg *ver *ip
	char userinfo[MAX_USERINFO_STRING];

	// extra user info for the "connect" command
	char connect_userinfo[MAX_USERINFO_STRING];

	// video capture stuff
	capturevideostate_t capturevideo;

	// crypto channel
	crypto_t crypto;

	// ProQuake compatibility stuff
	int proquake_servermod; // 0 = not proquake, 1 = proquake
	int proquake_serverversion; // actual proquake server version * 10 (3.40 = 34, etc)
	int proquake_serverflags; // 0 (PQF_CHEATFREE not supported)
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
	qboolean canjump;
}
client_movementqueue_t;

//[515]: csqc
typedef struct
{
	qboolean drawworld;
	qboolean drawenginesbar;
	qboolean drawcrosshair;
}csqc_vidvars_t;

typedef enum
{
	PARTICLE_BILLBOARD = 0,
	PARTICLE_SPARK = 1,
	PARTICLE_ORIENTED_DOUBLESIDED = 2,
	PARTICLE_VBEAM = 3,
	PARTICLE_HBEAM = 4,
	PARTICLE_INVALID = -1
}
porientation_t;

typedef enum
{
	PBLEND_ALPHA = 0,
	PBLEND_ADD = 1,
	PBLEND_INVMOD = 2,
	PBLEND_INVALID = -1
}
pblend_t;

typedef struct particletype_s
{
	pblend_t blendmode;
	porientation_t orientation;
	qboolean lighting;
}
particletype_t;

typedef enum ptype_e
{
	pt_dead, pt_alphastatic, pt_static, pt_spark, pt_beam, pt_rain, pt_raindecal, pt_snow, pt_bubble, pt_blood, pt_smoke, pt_decal, pt_entityparticle, pt_total
}
ptype_t;

typedef struct decal_s
{
	// fields used by rendering:  (44 bytes)
	unsigned short	typeindex;
	unsigned short	texnum;
	int				decalsequence;
	vec3_t			org;
	vec3_t			normal;
	float			size;
	float			alpha; // 0-255
	unsigned char	color[3];
	unsigned char	unused1;
	int				clusterindex; // cheap culling by pvs

	// fields not used by rendering: (36 bytes in 32bit, 40 bytes in 64bit)
	float			time2; // used for decal fade
	unsigned int	owner; // decal stuck to this entity
	dp_model_t			*ownermodel; // model the decal is stuck to (used to make sure the entity is still alive)
	vec3_t			relativeorigin; // decal at this location in entity's coordinate space
	vec3_t			relativenormal; // decal oriented this way relative to entity's coordinate space
}
decal_t;

typedef struct particle_s
{
	// for faster batch rendering, particles are rendered in groups by effect (resulting in less perfect sorting but far less state changes)

	// fields used by rendering: (48 bytes)
	vec3_t          sortorigin; // sort by this group origin, not particle org
	vec3_t          org;
	vec3_t          vel; // velocity of particle, or orientation of decal, or end point of beam
	float           size;
	float           alpha; // 0-255
	float           stretch; // only for sparks

	// fields not used by rendering:  (44 bytes)
	float           stainsize;
	float           stainalpha;
	float           sizeincrease; // rate of size change per second
	float           alphafade; // how much alpha reduces per second
	float           time2; // used for snow fluttering and decal fade
	float           bounce; // how much bounce-back from a surface the particle hits (0 = no physics, 1 = stop and slide, 2 = keep bouncing forever, 1.5 is typical)
	float           gravity; // how much gravity affects this particle (1.0 = normal gravity, 0.0 = none)
	float           airfriction; // how much air friction affects this object (objects with a low mass/size ratio tend to get more air friction)
	float           liquidfriction; // how much liquid friction affects this object (objects with a low mass/size ratio tend to get more liquid friction)
//	float           delayedcollisions; // time that p->bounce becomes active
	float           delayedspawn; // time that particle appears and begins moving
	float           die; // time when this particle should be removed, regardless of alpha

	// short variables grouped to save memory (4 bytes)
	short			angle; // base rotation of particle
	short			spin; // geometry rotation speed around the particle center normal

	// byte variables grouped to save memory (12 bytes)
	unsigned char   color[3];
	unsigned char   qualityreduction; // enables skipping of this particle according to r_refdef.view.qualityreduction
	unsigned char   typeindex;
	unsigned char   blendmode;
	unsigned char   orientation;
	unsigned char   texnum;
	unsigned char   staincolor[3];
	signed char     staintexnum;
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

typedef struct cl_locnode_s
{
	struct cl_locnode_s *next;
	char *name;
	vec3_t mins, maxs;
}
cl_locnode_t;

typedef struct showlmp_s
{
	qboolean	isactive;
	float		x;
	float		y;
	char		label[32];
	char		pic[128];
}
showlmp_t;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct client_state_s
{
	// true if playing in a local game and no one else is connected
	int islocalgame;

	// send a clc_nop periodically until connected
	float sendnoptime;

	// current input being accumulated by mouse/joystick/etc input
	usercmd_t cmd;
	// latest moves sent to the server that have not been confirmed yet
	usercmd_t movecmd[CL_MAX_USERCMDS];

// information for local display
	// health, etc
	int stats[MAX_CL_STATS];
	float *statsf; // points to stats[] array
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
	// for stair smoothing
	float stairsmoothz;
	double stairsmoothtime;

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
	// if true the CL_ClientMovement_Replay function will update origin, etc
	qboolean movement_replay;
	// simulated data (this is valid even if cl.movement is false)
	vec3_t movement_origin;
	vec3_t movement_velocity;
	// whether the replay should allow a jump at the first sequence
	qboolean movement_replay_canjump;

	// previous gun angles (for leaning effects)
	vec3_t gunangles_prev;
	vec3_t gunangles_highpass;
	vec3_t gunangles_adjustment_lowpass;
	vec3_t gunangles_adjustment_highpass;
	// previous gun angles (for leaning effects)
	vec3_t gunorg_prev;
	vec3_t gunorg_highpass;
	vec3_t gunorg_adjustment_lowpass;
	vec3_t gunorg_adjustment_highpass;

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
	qboolean csqc_paused; // vortex: int because could be flags
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
	float bob2_smooth;
	float bobfall_speed;
	float bobfall_swing;

	// don't change view angle, full screen, etc
	int intermission;
	// latched at intermission start
	double completed_time;

	// the timestamp of the last two messages
	double mtime[2];

	// clients view of time, time should be between mtime[0] and mtime[1] to
	// generate a lerp point for other data, oldtime is the previous frame's
	// value of time, frametime is the difference between time and oldtime
	// note: cl.time may be beyond cl.mtime[0] if packet loss is occuring, it
	// is only forcefully limited when a packet is received
	double time, oldtime;
	// how long it has been since the previous client frame in real time
	// (not game time, for that use cl.time - cl.oldtime)
	double realframetime;
	
	// fade var for fading while dead
	float deathfade;

	// motionblur alpha level variable
	float motionbluralpha;

	// copy of realtime from last recieved message, for net trouble icon
	float last_received_message;

// information that is static for the entire time connected to a server
	struct model_s *model_precache[MAX_MODELS];
	struct sfx_s *sound_precache[MAX_SOUNDS];

	// FIXME: this is a lot of memory to be keeping around, this really should be dynamically allocated and freed somehow
	char model_name[MAX_MODELS][MAX_QPATH];
	char sound_name[MAX_SOUNDS][MAX_QPATH];

	// for display on solo scoreboard
	char worldmessage[40]; // map title (not related to filename)
	// variants of map name
	char worldbasename[MAX_QPATH]; // %s
	char worldname[MAX_QPATH]; // maps/%s.bsp
	char worldnamenoextension[MAX_QPATH]; // maps/%s
	// cl_entitites[cl.viewentity] = player
	int viewentity;
	// the real player entity (normally same as viewentity,
	// different than viewentity if mod uses chasecam or other tricks)
	int realplayerentity;
	// this is updated to match cl.viewentity whenever it is in the clients
	// range, basically this is used in preference to cl.realplayerentity for
	// most purposes because when spectating another player it should show
	// their information rather than yours
	int playerentity;
	// max players that can be in this game
	int maxclients;
	// type of game (deathmatch, coop, singleplayer)
	int gametype;

	// models and sounds used by engine code (particularly cl_parse.c)
	dp_model_t *model_bolt;
	dp_model_t *model_bolt2;
	dp_model_t *model_bolt3;
	dp_model_t *model_beam;
	sfx_t *sfx_wizhit;
	sfx_t *sfx_knighthit;
	sfx_t *sfx_tink1;
	sfx_t *sfx_ric1;
	sfx_t *sfx_ric2;
	sfx_t *sfx_ric3;
	sfx_t *sfx_r_exp3;
	// indicates that the file "sound/misc/talk2.wav" was found (for use by team chat messages)
	qboolean foundtalk2wav;

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
	// set by scoreboard code when sending ping command, this causes the next ping results to be hidden
	// (which could eat the wrong ping report if the player issues one
	//  manually, but they would still see a ping report, just a later one
	//  caused by the scoreboard code rather than the one they intentionally
	//  issued)
	int parsingtextexpectingpingforscores;

	// entity database stuff
	// latest received entity frame numbers
#define LATESTFRAMENUMS 32
	int latestframenumsposition;
	int latestframenums[LATESTFRAMENUMS];
	int latestsendnums[LATESTFRAMENUMS];
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

	// old decals are killed based on this
	int decalsequence;

	int max_entities;
	int max_csqcrenderentities;
	int max_static_entities;
	int max_effects;
	int max_beams;
	int max_dlights;
	int max_lightstyle;
	int max_brushmodel_entities;
	int max_particles;
	int max_decals;
	int max_showlmps;

	entity_t *entities;
	entity_render_t *csqcrenderentities;
	unsigned char *entities_active;
	entity_t *static_entities;
	cl_effect_t *effects;
	beam_t *beams;
	dlight_t *dlights;
	lightstyle_t *lightstyle;
	int *brushmodel_entities;
	particle_t *particles;
	decal_t *decals;
	showlmp_t *showlmps;

	int num_entities;
	int num_static_entities;
	int num_brushmodel_entities;
	int num_effects;
	int num_beams;
	int num_dlights;
	int num_particles;
	int num_decals;
	int num_showlmps;

	double particles_updatetime;
	double decals_updatetime;
	int free_particle;
	int free_decal;

	// cl_serverextension_download feature
	int loadmodel_current;
	int downloadmodel_current;
	int loadmodel_total;
	int loadsound_current;
	int downloadsound_current;
	int loadsound_total;
	qboolean downloadcsqc;
	qboolean loadcsqc;
	qboolean loadbegun;
	qboolean loadfinished;

	// quakeworld stuff

	// local copy of the server infostring
	char qw_serverinfo[MAX_SERVERINFO_STRING];

	// time of last qw "pings" command sent to server while showing scores
	double last_ping_request;

	// used during connect
	int qw_servercount;

	// updated from serverinfo
	int qw_teamplay;

	// unused: indicates whether the player is spectating
	// use cl.scores[cl.playerentity-1].qw_spectator instead
	//qboolean qw_spectator;

	// last time an input packet was sent
	double lastpackettime;

	// movement parameters for client prediction
	unsigned int moveflags;
	float movevars_wallfriction;
	float movevars_waterfriction;
	float movevars_friction;
	float movevars_timescale;
	float movevars_gravity;
	float movevars_stopspeed;
	float movevars_maxspeed;
	float movevars_spectatormaxspeed;
	float movevars_accelerate;
	float movevars_airaccelerate;
	float movevars_wateraccelerate;
	float movevars_entgravity;
	float movevars_jumpvelocity;
	float movevars_edgefriction;
	float movevars_maxairspeed;
	float movevars_stepheight;
	float movevars_airaccel_qw;
	float movevars_airaccel_qw_stretchfactor;
	float movevars_airaccel_sideways_friction;
	float movevars_airstopaccelerate;
	float movevars_airstrafeaccelerate;
	float movevars_maxairstrafespeed;
	float movevars_airstrafeaccel_qw;
	float movevars_aircontrol;
	float movevars_aircontrol_power;
	float movevars_aircontrol_penalty;
	float movevars_warsowbunny_airforwardaccel;
	float movevars_warsowbunny_accel;
	float movevars_warsowbunny_topspeed;
	float movevars_warsowbunny_turnaccel;
	float movevars_warsowbunny_backtosideratio;
	float movevars_ticrate;
	float movevars_airspeedlimit_nonqw;

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

	int qw_deltasequence[QW_UPDATE_BACKUP];

	// csqc stuff:
	// server entity number corresponding to a clientside entity
	unsigned short csqc_server2csqcentitynumber[MAX_EDICTS];
	qboolean csqc_loaded;
	vec3_t csqc_vieworigin;
	vec3_t csqc_viewangles;
	vec3_t csqc_vieworiginfromengine;
	vec3_t csqc_viewanglesfromengine;
	matrix4x4_t csqc_viewmodelmatrixfromengine;
	qboolean csqc_usecsqclistener;
	matrix4x4_t csqc_listenermatrix;
	char csqc_printtextbuf[MAX_INPUTLINE];

	// collision culling data
	world_t world;

	// loc file stuff (points and boxes describing locations in the level)
	cl_locnode_t *locnodes;
	// this is updated to cl.movement_origin whenever health is < 1
	// used by %d print in say/say_team messages if cl_locs_enable is on
	vec3_t lastdeathorigin;

	// processing buffer used by R_BuildLightMap, reallocated as needed,
	// freed on each level change
	size_t buildlightmapmemorysize;
	unsigned char *buildlightmapmemory;

	// used by EntityState5_ReadUpdate
	skeleton_t *engineskeletonobjects;
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

extern cvar_t cl_shownet;
extern cvar_t cl_nolerp;
extern cvar_t cl_nettimesyncfactor;
extern cvar_t cl_nettimesyncboundmode;
extern cvar_t cl_nettimesyncboundtolerance;

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
extern cvar_t cl_autodemo_delete;

extern cvar_t r_draweffects;

extern cvar_t cl_explosions_alpha_start;
extern cvar_t cl_explosions_alpha_end;
extern cvar_t cl_explosions_size_start;
extern cvar_t cl_explosions_size_end;
extern cvar_t cl_explosions_lifetime;
extern cvar_t cl_stainmaps;
extern cvar_t cl_stainmaps_clearonload;

extern cvar_t cl_prydoncursor;
extern cvar_t cl_prydoncursor_notrace;

extern cvar_t cl_locs_enable;

extern client_state_t cl;

extern void CL_AllocLightFlash (entity_render_t *ent, matrix4x4_t *matrix, float radius, float red, float green, float blue, float decay, float lifetime, int cubemapnum, int style, int shadowenable, vec_t corona, vec_t coronasizescale, vec_t ambientscale, vec_t diffusescale, vec_t specularscale, int flags);

cl_locnode_t *CL_Locs_FindNearest(const vec3_t point);
void CL_Locs_FindLocationName(char *buffer, size_t buffersize, vec3_t point);

//=============================================================================

//
// cl_main
//

void CL_Shutdown (void);
void CL_Init (void);

void CL_EstablishConnection(const char *host, int firstarg);

void CL_Disconnect (void);
void CL_Disconnect_f (void);

void CL_UpdateRenderEntity(entity_render_t *ent);
void CL_SetEntityColormapColors(entity_render_t *ent, int colormap);
void CL_UpdateViewEntities(void);

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
void CL_NewBeam (int ent, vec3_t start, vec3_t end, dp_model_t *m, int lightning);
void CL_RelinkBeams (void);
void CL_Beam_CalculatePositions (const beam_t *b, vec3_t start, vec3_t end);
void CL_ClientMovement_Replay(void);

void CL_ClearTempEntities (void);
entity_render_t *CL_NewTempEntity (double shadertime);

void CL_Effect(vec3_t org, int modelindex, int startframe, int framecount, float framerate);

void CL_ClearState (void);
void CL_ExpandEntities(int num);
void CL_ExpandCSQCRenderEntities(int num);
void CL_SetInfo(const char *key, const char *value, qboolean send, qboolean allowstarkey, qboolean allowmodel, qboolean quiet);


void CL_UpdateWorld (void);
void CL_WriteToServer (void);
void CL_Input (void);
extern int cl_ignoremousemoves;


float CL_KeyState (kbutton_t *key);
const char *Key_KeynumToString (int keynum);
int Key_StringToKeynum (const char *str);

//
// cl_demo.c
//
void CL_StopPlayback(void);
void CL_ReadDemoMessage(void);
void CL_WriteDemoMessage(sizebuf_t *mesage);

void CL_CutDemo(unsigned char **buf, fs_offset_t *filesize);
void CL_PasteDemo(unsigned char **buf, fs_offset_t *filesize);

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
void CL_KeepaliveMessage(qboolean readmessages); // call this during loading of large content

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
extern cvar_t cl_particles_blood_decal_alpha;
extern cvar_t cl_particles_blood_decal_scalemin;
extern cvar_t cl_particles_blood_decal_scalemax;
extern cvar_t cl_particles_blood_bloodhack;
extern cvar_t cl_particles_bulletimpacts;
extern cvar_t cl_particles_explosions_sparks;
extern cvar_t cl_particles_explosions_shell;
extern cvar_t cl_particles_rain;
extern cvar_t cl_particles_snow;
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
particle_t *CL_NewParticle(const vec3_t sortorigin, unsigned short ptypeindex, int pcolor1, int pcolor2, int ptex, float psize, float psizeincrease, float palpha, float palphafade, float pgravity, float pbounce, float px, float py, float pz, float pvx, float pvy, float pvz, float pairfriction, float pliquidfriction, float originjitter, float velocityjitter, qboolean pqualityreduction, float lifetime, float stretch, pblend_t blendmode, porientation_t orientation, int staincolor1, int staincolor2, int staintex, float stainalpha, float stainsize, float angle, float spin, float tint[4]);

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
void CL_ParticleTrail(int effectindex, float pcount, const vec3_t originmins, const vec3_t originmaxs, const vec3_t velocitymins, const vec3_t velocitymaxs, entity_t *ent, int palettecolor, qboolean spawndlight, qboolean spawnparticles, float tintmins[4], float tintmaxs[4]);
void CL_ParseParticleEffect (void);
void CL_ParticleCube (const vec3_t mins, const vec3_t maxs, const vec3_t dir, int count, int colorbase, vec_t gravity, vec_t randomvel);
void CL_ParticleRain (const vec3_t mins, const vec3_t maxs, const vec3_t dir, int count, int colorbase, int type);
void CL_EntityParticles (const entity_t *ent);
void CL_ParticleExplosion (const vec3_t org);
void CL_ParticleExplosion2 (const vec3_t org, int colorStart, int colorLength);
void R_NewExplosion(const vec3_t org);

void Debug_PolygonBegin(const char *picname, int flags);
void Debug_PolygonVertex(float x, float y, float z, float s, float t, float r, float g, float b, float a);
void Debug_PolygonEnd(void);

#include "cl_screen.h"

extern qboolean sb_showscores;

float RSurf_FogVertex(const vec3_t p);
float RSurf_FogPoint(const vec3_t p);

typedef struct r_refdef_stats_s
{
	int renders;
	int entities;
	int entities_surfaces;
	int entities_triangles;
	int world_leafs;
	int world_portals;
	int world_surfaces;
	int world_triangles;
	int lightmapupdates;
	int lightmapupdatepixels;
	int particles;
	int drawndecals;
	int totaldecals;
	int draws;
	int draws_vertices;
	int draws_elements;
	int lights;
	int lights_clears;
	int lights_scissored;
	int lights_lighttriangles;
	int lights_shadowtriangles;
	int lights_dynamicshadowtriangles;
	int bouncegrid_lights;
	int bouncegrid_particles;
	int bouncegrid_traces;
	int bouncegrid_hits;
	int bouncegrid_splats;
	int bouncegrid_bounces;
	int collisioncache_animated;
	int collisioncache_cached;
	int collisioncache_traced;
	int bloom;
	int bloom_copypixels;
	int bloom_drawpixels;
	int indexbufferuploadcount;
	int indexbufferuploadsize;
	int vertexbufferuploadcount;
	int vertexbufferuploadsize;
	int framedatacurrent;
	int framedatasize;
}
r_refdef_stats_t;

typedef enum r_viewport_type_e
{
	R_VIEWPORTTYPE_ORTHO,
	R_VIEWPORTTYPE_PERSPECTIVE,
	R_VIEWPORTTYPE_PERSPECTIVE_INFINITEFARCLIP,
	R_VIEWPORTTYPE_PERSPECTIVECUBESIDE,
	R_VIEWPORTTYPE_TOTAL
}
r_viewport_type_t;

typedef struct r_viewport_s
{
	matrix4x4_t cameramatrix; // from entity (transforms from camera entity to world)
	matrix4x4_t viewmatrix; // actual matrix for rendering (transforms to viewspace)
	matrix4x4_t projectmatrix; // actual projection matrix (transforms from viewspace to screen)
	int x;
	int y;
	int z;
	int width;
	int height;
	int depth;
	r_viewport_type_t type;
	float screentodepth[2]; // used by deferred renderer to calculate linear depth from device depth coordinates
}
r_viewport_t;

typedef struct r_refdef_view_s
{
	// view information (changes multiple times per frame)
	// if any of these variables change then r_refdef.viewcache must be regenerated
	// by calling R_View_Update
	// (which also updates viewport, scissor, colormask)

	// it is safe and expected to copy this into a structure on the stack and
	// call the renderer recursively, then restore from the stack afterward
	// (as long as R_View_Update is called)

	// eye position information
	matrix4x4_t matrix, inverse_matrix;
	vec3_t origin;
	vec3_t forward;
	vec3_t left;
	vec3_t right;
	vec3_t up;
	int numfrustumplanes;
	mplane_t frustum[6];
	qboolean useclipplane;
	qboolean usecustompvs; // uses r_refdef.viewcache.pvsbits as-is rather than computing it
	mplane_t clipplane;
	float frustum_x, frustum_y;
	vec3_t frustumcorner[4];
	// if turned off it renders an ortho view
	int useperspective;
	float ortho_x, ortho_y;

	// screen area to render in
	int x;
	int y;
	int z;
	int width;
	int height;
	int depth;
	r_viewport_t viewport; // note: if r_viewscale is used, the viewport.width and viewport.height may be less than width and height

	// which color components to allow (for anaglyph glasses)
	int colormask[4];

	// global RGB color multiplier for rendering, this is required by HDR
	float colorscale;

	// whether to call R_ClearScreen before rendering stuff
	qboolean clear;
	// if true, don't clear or do any post process effects (bloom, etc)
	qboolean isoverlay;

	// whether to draw r_showtris and such, this is only true for the main
	// view render, all secondary renders (HDR, mirrors, portals, cameras,
	// distortion effects, etc) omit such debugging information
	qboolean showdebug;

	// these define which values to use in GL_CullFace calls to request frontface or backface culling
	int cullface_front;
	int cullface_back;

	// render quality (0 to 1) - affects r_drawparticles_drawdistance and others
	float quality;
}
r_refdef_view_t;

typedef struct r_refdef_viewcache_s
{
	// updated by gl_main_newmap()
	int maxentities;
	int world_numclusters;
	int world_numclusterbytes;
	int world_numleafs;
	int world_numsurfaces;

	// these properties are generated by R_View_Update()

	// which entities are currently visible for this viewpoint
	// (the used range is 0...r_refdef.scene.numentities)
	unsigned char *entityvisible;

	// flag arrays used for visibility checking on world model
	// (all other entities have no per-surface/per-leaf visibility checks)
	unsigned char *world_pvsbits;
	unsigned char *world_leafvisible;
	unsigned char *world_surfacevisible;
	// if true, the view is currently in a leaf without pvs data
	qboolean world_novis;
}
r_refdef_viewcache_t;

// TODO: really think about which fields should go into scene and which one should stay in refdef [1/7/2008 Black]
// maybe also refactor some of the functions to support different setting sources (ie. fogenabled, etc.) for different scenes
typedef struct r_refdef_scene_s {
	// whether to call S_ExtraUpdate during render to reduce sound chop
	qboolean extraupdate;

	// (client gameworld) time for rendering time based effects
	double time;

	// the world
	entity_render_t *worldentity;

	// same as worldentity->model
	dp_model_t *worldmodel;

	// renderable entities (excluding world)
	entity_render_t **entities;
	int numentities;
	int maxentities;

	// field of temporary entities that is reset each (client) frame
	entity_render_t *tempentities;
	int numtempentities;
	int maxtempentities;
	qboolean expandtempentities;

	// renderable dynamic lights
	rtlight_t *lights[MAX_DLIGHTS];
	rtlight_t templights[MAX_DLIGHTS];
	int numlights;

	// intensities for light styles right now, controls rtlights
	float rtlightstylevalue[MAX_LIGHTSTYLES];	// float fraction of base light value
	// 8.8bit fixed point intensities for light styles
	// controls intensity lightmap layers
	unsigned short lightstylevalue[MAX_LIGHTSTYLES];	// 8.8 fraction of base light value

	float ambient;

	qboolean rtworld;
	qboolean rtworldshadows;
	qboolean rtdlight;
	qboolean rtdlightshadows;
} r_refdef_scene_t;

typedef struct r_refdef_s
{
	// these fields define the basic rendering information for the world
	// but not the view, which could change multiple times in one rendered
	// frame (for example when rendering textures for certain effects)

	// these are set for water warping before
	// frustum_x/frustum_y are calculated
	float frustumscale_x, frustumscale_y;

	// current view settings (these get reset a few times during rendering because of water rendering, reflections, etc)
	r_refdef_view_t view;
	r_refdef_viewcache_t viewcache;

	// minimum visible distance (pixels closer than this disappear)
	double nearclip;
	// maximum visible distance (pixels further than this disappear in 16bpp modes,
	// in 32bpp an infinite-farclip matrix is used instead)
	double farclip;

	// fullscreen color blend
	float viewblend[4];

	r_refdef_scene_t scene;

	float fogplane[4];
	float fogplaneviewdist;
	qboolean fogplaneviewabove;
	float fogheightfade;
	float fogcolor[3];
	float fogrange;
	float fograngerecip;
	float fogmasktabledistmultiplier;
#define FOGMASKTABLEWIDTH 1024
	float fogmasktable[FOGMASKTABLEWIDTH];
	float fogmasktable_start, fogmasktable_alpha, fogmasktable_range, fogmasktable_density;
	float fog_density;
	float fog_red;
	float fog_green;
	float fog_blue;
	float fog_alpha;
	float fog_start;
	float fog_end;
	float fog_height;
	float fog_fadedepth;
	qboolean fogenabled;
	qboolean oldgl_fogenable;

	// new flexible texture height fog (overrides normal fog)
	char fog_height_texturename[64]; // note: must be 64 for the sscanf code
	unsigned char *fog_height_table1d;
	unsigned char *fog_height_table2d;
	int fog_height_tablesize; // enable
	float fog_height_tablescale;
	float fog_height_texcoordscale;
	char fogheighttexturename[64]; // detects changes to active fog height texture

	int draw2dstage; // 0 = no, 1 = yes, other value = needs setting up again

	// true during envmap command capture
	qboolean envmap;

	// brightness of world lightmaps and related lighting
	// (often reduced when world rtlights are enabled)
	float lightmapintensity;
	// whether to draw world lights realtime, dlights realtime, and their shadows
	float polygonfactor;
	float polygonoffset;
	float shadowpolygonfactor;
	float shadowpolygonoffset;

	// how long R_RenderView took on the previous frame
	double lastdrawscreentime;

	// rendering stats for r_speeds display
	// (these are incremented in many places)
	r_refdef_stats_t stats;
}
r_refdef_t;

extern r_refdef_t r_refdef;

// warpzone prediction hack (CSQC builtin)
void CL_RotateMoves(const matrix4x4_t *m);

#endif

