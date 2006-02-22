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

// flag arrays used for visibility checking on world model
// (all other entities have no per-surface/per-leaf visibility checks)
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_clusters
extern unsigned char r_pvsbits[(32768+7)>>3];
// TODO: dynamic resize according to r_refdef.worldmodel->brush.num_leafs
extern unsigned char r_worldleafvisible[32768];
// TODO: dynamic resize according to r_refdef.worldmodel->num_surfaces
extern unsigned char r_worldsurfacevisible[262144];

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
extern cvar_t r_skyscroll1;
extern cvar_t r_skyscroll2;
extern int skyrendernow, skyrendermasked;
extern int R_SetSkyBox(const char *sky);
extern void R_SkyStartFrame(void);
extern void R_Sky(void);
extern void R_ResetSkyBox(void);

// SHOWLMP stuff (Nehahra)
extern void SHOWLMP_decodehide(void);
extern void SHOWLMP_decodeshow(void);
extern void SHOWLMP_drawall(void);
extern void SHOWLMP_clear(void);

// render profiling stuff
extern char r_speeds_string[1024];

// lighting stuff
extern cvar_t r_ambient;
extern cvar_t gl_flashblend;

// vis stuff
extern cvar_t r_novis;

extern cvar_t r_lerpsprites;
extern cvar_t r_lerpmodels;
extern cvar_t r_waterscroll;

extern cvar_t developer_texturelogging;

typedef struct rmesh_s
{
	// vertices of this mesh
	int maxvertices;
	int numvertices;
	float *vertex3f;
	float *svector3f;
	float *tvector3f;
	float *normal3f;
	float *texcoord2f;
	float *texcoordlightmap2f;
	float *color4f;
	// triangles of this mesh
	int maxtriangles;
	int numtriangles;
	int *element3i;
	int *neighbor3i;
	// snapping epsilon
	float epsilon2;
}
rmesh_t;

// useful functions for rendering
void R_ModulateColors(float *in, float *out, int verts, float r, float g, float b);
void R_FillColors(float *out, int verts, float r, float g, float b, float a);
int R_Mesh_AddVertex3f(rmesh_t *mesh, const float *v);
void R_Mesh_AddPolygon3f(rmesh_t *mesh, int numvertices, float *vertex3f);
void R_Mesh_AddBrushMeshFromPlanes(rmesh_t *mesh, int numplanes, mplane_t *planes);

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

extern int r_framecount;
extern mplane_t frustum[5];

typedef struct renderstats_s
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
renderstats_t;

extern renderstats_t renderstats;

// brightness of world lightmaps and related lighting
// (often reduced when world rtlights are enabled)
extern float r_lightmapintensity;
// whether to draw world lights realtime, dlights realtime, and their shadows
extern qboolean r_rtworld;
extern qboolean r_rtworldshadows;
extern qboolean r_rtdlight;
extern qboolean r_rtdlightshadows;

// forces all rendering to draw triangle outlines
extern cvar_t r_showtris;
extern cvar_t r_showtris_polygonoffset;
extern cvar_t r_shownormals;
extern cvar_t r_showlighting;
extern cvar_t r_showshadowvolumes;
extern cvar_t r_showcollisionbrushes;
extern cvar_t r_showcollisionbrushes_polygonfactor;
extern cvar_t r_showcollisionbrushes_polygonoffset;
extern cvar_t r_showdisabledepthtest;
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
extern int r_view_z;
extern int r_view_width;
extern int r_view_height;
extern int r_view_depth;
extern matrix4x4_t r_view_matrix;

extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;

extern	qboolean	envmap;

extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_speeds;
extern cvar_t r_fullbright;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;

void R_Init(void);
void R_UpdateWorld(void); // needs no r_refdef
void R_RenderView(void); // must call R_UpdateWorld and set r_refdef first


void R_InitSky (unsigned char *src, int bytesperpixel); // called at level load

void R_WorldVisibility();
void R_DrawParticles(void);
void R_DrawExplosions(void);

#define gl_solid_format 3
#define gl_alpha_format 4

int R_CullBox(const vec3_t mins, const vec3_t maxs);

#define FOGTABLEWIDTH 1024
extern vec3_t fogcolor;
extern vec_t fogdensity;
extern vec_t fogrange;
extern vec_t fograngerecip;
extern int fogtableindex;
extern vec_t fogtabledistmultiplier;
extern float fogtable[FOGTABLEWIDTH];
extern qboolean fogenabled;
#define VERTEXFOGTABLE(dist) (fogtableindex = (int)((dist) * fogtabledistmultiplier), fogtable[bound(0, fogtableindex, FOGTABLEWIDTH - 1)])

#include "r_modules.h"

#include "meshqueue.h"

#include "r_lerpanim.h"

extern cvar_t r_render;
extern cvar_t r_waterwarp;

extern cvar_t r_textureunits;
extern cvar_t gl_polyblend;
extern cvar_t gl_dither;

extern cvar_t r_smoothnormals_areaweighting;

#include "gl_backend.h"

#include "r_light.h"

extern rtexture_t *r_texture_blanknormalmap;
extern rtexture_t *r_texture_white;
extern rtexture_t *r_texture_black;
extern rtexture_t *r_texture_notexture;
extern rtexture_t *r_texture_whitecube;
extern rtexture_t *r_texture_normalizationcube;
extern rtexture_t *r_texture_fogattenuation;
extern rtexture_t *r_texture_fogintensity;

void R_TimeReport(char *name);
void R_TimeReport_Start(void);
void R_TimeReport_End(void);

// r_stain
void R_Stain(const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2);

void R_DrawWorldCrosshair(void);
void R_Draw2DCrosshair(void);

void R_CalcBeam_Vertex3f(float *vert, const vec3_t org1, const vec3_t org2, float width);
void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, rtexture_t *fogtexture, int depthdisable, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca);

struct entity_render_s;
struct texture_s;
struct msurface_s;
void R_UpdateTextureInfo(const entity_render_t *ent, texture_t *t);
void R_UpdateAllTextureInfo(entity_render_t *ent);
void R_QueueTextureSurfaceList(entity_render_t *ent, struct texture_s *texture, int texturenumsurfaces, const struct msurface_s **texturesurfacelist, const vec3_t modelorg);
void R_DrawSurfaces(entity_render_t *ent, qboolean skysurfaces);

#endif

