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

extern qbyte r_pvsbits[(MAX_MAP_LEAFS+7)>>3];

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

extern cvar_t r_lerpsprites;
extern cvar_t r_lerpmodels;
extern cvar_t r_waterscroll;
extern cvar_t r_watershader;

// useful functions for rendering
void R_ModulateColors(float *in, float *out, int verts, float r, float g, float b);
void R_FillColors(float *out, int verts, float r, float g, float b, float a);

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int		c_alias_polys, c_light_polys, c_faces, c_nodes, c_leafs, c_models, c_bmodels, c_sprites, c_particles, c_dlights;

// forces all rendering to draw triangle outlines
extern cvar_t r_showtris;
extern int r_showtrispass;

//
// view origin
//
extern vec3_t r_vieworigin;
extern vec3_t r_viewforward;
extern vec3_t r_viewleft;
extern vec3_t r_viewright;
extern vec3_t r_viewup;
extern int r_view_x;
extern int r_view_y;
extern int r_view_width;
extern int r_view_height;
extern float r_view_fov_x;
extern float r_view_fov_y;
extern matrix4x4_t r_view_matrix;

extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern	unsigned short	d_lightstylevalue[256];	// 8.8 fraction of base light value

extern	qboolean	envmap;

extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_speeds;
extern cvar_t r_fullbright;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;
extern cvar_t r_drawcollisionbrushes;

void R_Init(void);
void R_UpdateWorld(void); // needs no r_refdef
void R_RenderView(void); // must call R_UpdateWorld and set r_refdef first


void R_InitSky (qbyte *src, int bytesperpixel); // called at level load

void R_WorldVisibility(entity_render_t *ent);
void R_DrawWorld(entity_render_t *ent);
void R_DrawParticles(void);
void R_DrawExplosions(void);

#define gl_solid_format 3
#define gl_alpha_format 4

//#define PARANOID 1

int R_CullBox(const vec3_t mins, const vec3_t maxs);
#define VIS_CullBox(mins,maxs) (R_CullBox((mins), (maxs)) || (cl.worldmodel && cl.worldmodel->brush.BoxTouchingPVS && !cl.worldmodel->brush.BoxTouchingPVS(cl.worldmodel, r_pvsbits, (mins), (maxs))))

extern qboolean fogenabled;
extern vec3_t fogcolor;
extern vec_t fogdensity;
#define calcfog(v) (exp(-(fogdensity*fogdensity*(((v)[0] - r_vieworigin[0])*((v)[0] - r_vieworigin[0])+((v)[1] - r_vieworigin[1])*((v)[1] - r_vieworigin[1])+((v)[2] - r_vieworigin[2])*((v)[2] - r_vieworigin[2])))))
#define calcfogbyte(v) ((qbyte) (bound(0, ((int) ((float) (calcfog((v)) * 255.0f))), 255)))

#include "r_modules.h"

#include "meshqueue.h"

#include "r_lerpanim.h"

extern cvar_t r_render;
extern cvar_t r_waterwarp;

extern cvar_t r_textureunits;
extern cvar_t gl_dither;

#include "gl_backend.h"

#include "r_light.h"

void R_TimeReport(char *name);
void R_TimeReport_Start(void);
void R_TimeReport_End(void);

// r_stain
void R_Stain(const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2);

void R_DrawWorldCrosshair(void);
void R_Draw2DCrosshair(void);

void R_CalcBeam_Vertex3f(float *vert, const vec3_t org1, const vec3_t org2, float width);
void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, int depthdisable, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca);

#endif

