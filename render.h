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

// refresh.h -- public interface to refresh functions

// far clip distance for scene
extern cvar_t r_farclip;

// fog stuff
extern void FOG_clear(void);
extern float fog_density, fog_red, fog_green, fog_blue;

// SHOWLMP stuff (Nehahra)
extern void SHOWLMP_decodehide(void);
extern void SHOWLMP_decodeshow(void);
extern void SHOWLMP_drawall(void);
extern void SHOWLMP_clear(void);

// render profiling stuff
extern qboolean intimerefresh;
extern cvar_t r_speeds2;
extern char r_speeds2_string[1024];

// lighting stuff
extern vec3_t lightspot;
extern cvar_t r_ambient;
extern int lightscalebit;
extern float lightscale;

// model rendering stuff
extern float *aliasvert;
extern float *aliasvertnorm;
extern byte *aliasvertcolor;

// vis stuff
extern cvar_t r_novis;

// model transform stuff
extern cvar_t gl_transform;

// LordHavoc: 1.0f / N table
extern float ixtable[4096];

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

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
	float	colormod[3];	// color tint for model
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
	// area to render in
	int		x, y, width, height;
	float	fov_x, fov_y;

	// view point
	vec3_t	vieworg;
	vec3_t	viewangles;
}
refdef_t;

extern qboolean hlbsp;
//extern	qboolean	r_cache_thrash;		// compatability
extern	vec3_t		modelorg;
extern	entity_render_t	*currentrenderentity;
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_brush_polys, c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights;


//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_waterripple;

//extern	float	r_world_matrix[16];

void R_Init (void);
void R_RenderView (void); // must set r_refdef first

// LordHavoc: changed this for sake of GLQuake
void R_InitSky (byte *src, int bytesperpixel); // called at level load

//int R_VisibleCullBox (vec3_t mins, vec3_t maxs);

void R_NewMap (void);

#include "r_decals.h"

void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end, int type, entity_t *ent);
void R_RocketTrail2 (vec3_t start, vec3_t end, int type, entity_t *ent);
void R_SparkShower (vec3_t org, vec3_t dir, int count);
void R_BloodPuff (vec3_t org, vec3_t vel, int count);
void R_FlameCube (vec3_t mins, vec3_t maxs, int count);
void R_Flames (vec3_t org, vec3_t vel, int count);

void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org, int smoke);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

void R_NewExplosion(vec3_t org);

void R_PushDlights (void);
void R_DrawWorld (void);
//void R_RenderDlights (void);
void R_DrawParticles (void);
void R_MoveParticles (void);
void R_DrawExplosions (void);
void R_MoveExplosions (void);

#include "r_clip.h"

// LordHavoc: vertex transform
#include "transform.h"

// LordHavoc: transparent polygon system
#include "gl_poly.h"

#define gl_solid_format 3
#define gl_alpha_format 4

//#define PARANOID

// LordHavoc: was a major time waster
#define R_CullBox(mins,maxs) (frustum[0].BoxOnPlaneSideFunc(mins, maxs, &frustum[0]) == 2 || frustum[1].BoxOnPlaneSideFunc(mins, maxs, &frustum[1]) == 2 || frustum[2].BoxOnPlaneSideFunc(mins, maxs, &frustum[2]) == 2 || frustum[3].BoxOnPlaneSideFunc(mins, maxs, &frustum[3]) == 2)
#define R_NotCulledBox(mins,maxs) (frustum[0].BoxOnPlaneSideFunc(mins, maxs, &frustum[0]) != 2 && frustum[1].BoxOnPlaneSideFunc(mins, maxs, &frustum[1]) != 2 && frustum[2].BoxOnPlaneSideFunc(mins, maxs, &frustum[2]) != 2 && frustum[3].BoxOnPlaneSideFunc(mins, maxs, &frustum[3]) != 2)

extern qboolean fogenabled;
extern vec3_t fogcolor;
extern vec_t fogdensity;
//#define calcfog(v) (exp(-(fogdensity*fogdensity*(((v)[0] - r_origin[0]) * vpn[0] + ((v)[1] - r_origin[1]) * vpn[1] + ((v)[2] - r_origin[2]) * vpn[2])*(((v)[0] - r_origin[0]) * vpn[0] + ((v)[1] - r_origin[1]) * vpn[1] + ((v)[2] - r_origin[2]) * vpn[2]))))
#define calcfog(v) (exp(-(fogdensity*fogdensity*(((v)[0] - r_origin[0])*((v)[0] - r_origin[0])+((v)[1] - r_origin[1])*((v)[1] - r_origin[1])+((v)[2] - r_origin[2])*((v)[2] - r_origin[2])))))
#define calcfogbyte(v) ((byte) (bound(0, ((int) ((float) (calcfog((v)) * 255.0f))), 255)))

#include "r_modules.h"

extern qboolean lighthalf;

#include "r_lerpanim.h"

void GL_LockArray(int first, int count);
void GL_UnlockArray(void);

void R_DrawBrushModel (void);
void R_DrawAliasModel (void);
void R_DrawSpriteModel (void);

void R_ClipSprite (void);
void R_Entity_Callback(void *data, void *junk);

extern cvar_t r_render;
extern cvar_t r_upload;
extern cvar_t r_ser;
#include "image.h"

// if contents is not zero, it will impact on content changes
// (leafs matching contents are considered empty, others are solid)
extern int traceline_endcontents; // set by TraceLine
float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal, int contents);
