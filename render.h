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
extern char r_speeds2_string1[81], r_speeds2_string2[81], r_speeds2_string3[81], r_speeds2_string4[81], r_speeds2_string5[81], r_speeds2_string6[81], r_speeds2_string7[81];

// lighting stuff
extern vec3_t lightspot;
extern cvar_t r_ambient;

// model rendering stuff
extern float *aliasvert;
extern float *aliasvertnorm;
extern byte *aliasvertcolor;
extern float modelalpha;

// vis stuff
extern cvar_t r_novis;

// model transform stuff
extern cvar_t gl_transform;

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct entity_render_s
{
	vec3_t	origin;
	vec3_t	angles;

	int		visframe;		// last frame this entity was found in an active leaf

	model_t	*model;			// NULL = no model
	int		frame;			// current desired frame (usually identical to frame2, but frame2 is not always used)
	int		colormap;		// entity shirt and pants colors
	int		effects;		// light, particles, etc
	int		skinnum;		// for Alias models
	int		flags;			// render flags

	float	alpha;			// opacity (alpha) of the model
	float	scale;			// size the model is shown
	float	trail_time;		// last time for trail rendering
	float	colormod[3];	// color tint for model

	model_t	*lerp_model;	// lerp resets when model changes
	int		frame1;			// frame that the model is interpolating from
	int		frame2;			// frame that the model is interpolating to
	double	lerp_starttime;	// start of this transition
	double	framelerp;		// interpolation factor, usually computed from lerp_starttime
	double	frame1start;	// time frame1 began playing (for framegroup animations)
	double	frame2start;	// time frame2 began playing (for framegroup animations)
}
entity_render_t;

typedef struct entity_s
{
	entity_state_t state_baseline;	// baseline for entity
	entity_state_t state_previous;	// previous state (interpolating from this)
	entity_state_t state_current;	// current state (interpolating to this)

	entity_render_t render;
} entity_t;

typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh

	vec3_t		vieworg;
	vec3_t		viewangles;

	float		fov_x, fov_y;
} refdef_t;


//
// refresh
//


extern	refdef_t	r_refdef;
extern vec3_t	r_origin, vpn, vright, vup;
extern qboolean hlbsp;

void R_Init (void);
void R_RenderView (void); // must set r_refdef first
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect); // called whenever r_refdef or vid change

// LordHavoc: changed this for sake of GLQuake
void R_InitSky (byte *src, int bytesperpixel); // called at level load

int R_VisibleCullBox (vec3_t mins, vec3_t maxs);

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
