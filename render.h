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

#ifndef RENDER_H
#define RENDER_H

extern matrix4x4_t r_identitymatrix;

// 1.0f / N table
extern float ixtable[4096];

// far clip distance for scene
extern float r_farclip;

// fog stuff
extern void FOG_clear(void);
extern float fog_density, fog_red, fog_green, fog_blue;

// sky stuff
extern cvar_t r_sky;
extern int skyrendernow, skyrendermasked;
extern int R_SetSkyBox(const char *sky);
extern void R_SkyStartFrame(void);
extern void R_Sky(void);
extern void R_ResetQuakeSky(void);
extern void R_ResetSkyBox(void);

// SHOWLMP stuff (Nehahra)
extern void SHOWLMP_decodehide(void);
extern void SHOWLMP_decodeshow(void);
extern void SHOWLMP_drawall(void);
extern void SHOWLMP_clear(void);

// render profiling stuff
extern qboolean intimerefresh;
extern char r_speeds_string[1024];

// lighting stuff
extern cvar_t r_ambient;

// model rendering stuff
extern float *aliasvert;
extern float *aliasvertnorm;
extern float *aliasvertcolor;

// vis stuff
extern cvar_t r_novis;

// detail texture stuff
extern cvar_t r_detailtextures;

// useful functions for rendering
void R_ModulateColors(float *in, float *out, int verts, float r, float g, float b);
void R_FillColors(float *out, int verts, float r, float g, float b, float a);

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights;


//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;

extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_dynamic;

void R_Init (void);
void R_RenderView (void); // must set r_refdef first


void R_InitSky (qbyte *src, int bytesperpixel); // called at level load

void R_NewMap (void);

void R_DrawWorld(entity_render_t *ent, int baselighting);
void R_DrawParticles(void);
void R_DrawExplosions(void);

// LordHavoc: vertex transform
#include "transform.h"

#define gl_solid_format 3
#define gl_alpha_format 4

//#define PARANOID 1

int R_CullBox(const vec3_t emins, const vec3_t emaxs);
int R_NotCulledBox(const vec3_t emins, const vec3_t emaxs);

extern qboolean fogenabled;
extern vec3_t fogcolor;
extern vec_t fogdensity;
#define calcfog(v) (exp(-(fogdensity*fogdensity*(((v)[0] - r_origin[0])*((v)[0] - r_origin[0])+((v)[1] - r_origin[1])*((v)[1] - r_origin[1])+((v)[2] - r_origin[2])*((v)[2] - r_origin[2])))))
#define calcfogbyte(v) ((qbyte) (bound(0, ((int) ((float) (calcfog((v)) * 255.0f))), 255)))

// start a farclip measuring session
void R_FarClip_Start(vec3_t origin, vec3_t direction, vec_t startfarclip);
// enlarge farclip to accomodate box
void R_FarClip_Box(vec3_t mins, vec3_t maxs);
// return farclip value
float R_FarClip_Finish(void);

// updates farclip distance so it is large enough for the specified box
// (*important*)
void R_Mesh_EnlargeFarClipBBox(vec3_t mins, vec3_t maxs);

#include "r_modules.h"

#include "meshqueue.h"

extern float overbrightscale;

#include "r_lerpanim.h"

extern cvar_t r_render;
#include "image.h"

extern cvar_t r_textureunits;
extern cvar_t gl_dither;

#include "gl_backend.h"

#include "r_light.h"

void R_TimeReport(char *name);
void R_TimeReport_Start(void);
void R_TimeReport_End(void);

// r_stain
void R_Stain (vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2);

void R_DrawWorldCrosshair(void);
void R_Draw2DCrosshair(void);

void R_CalcBeamVerts (float *vert, vec3_t org1, vec3_t org2, float width);

#endif

