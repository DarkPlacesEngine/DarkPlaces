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

#include "svbsp.h"

// 1.0f / N table
extern float ixtable[4096];

// fog stuff
extern void FOG_clear(void);

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
extern int r_timereport_active;

// lighting stuff
extern cvar_t r_ambient;
extern cvar_t gl_flashblend;

// vis stuff
extern cvar_t r_novis;

extern cvar_t r_lerpsprites;
extern cvar_t r_lerpmodels;
extern cvar_t r_waterscroll;

extern cvar_t developer_texturelogging;

// shadow volume bsp struct with automatically growing nodes buffer
extern svbsp_t r_svbsp;

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

extern cvar_t r_nearclip;

// forces all rendering to draw triangle outlines
extern cvar_t r_showtris;
extern cvar_t r_shownormals;
extern cvar_t r_showlighting;
extern cvar_t r_showshadowvolumes;
extern cvar_t r_showcollisionbrushes;
extern cvar_t r_showcollisionbrushes_polygonfactor;
extern cvar_t r_showcollisionbrushes_polygonoffset;
extern cvar_t r_showdisabledepthtest;

//
// view origin
//
extern cvar_t r_drawentities;
extern cvar_t r_drawviewmodel;
extern cvar_t r_speeds;
extern cvar_t r_fullbright;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;

void R_Init(void);
void R_UpdateVariables(void); // must call after setting up most of r_refdef, but before calling R_RenderView
void R_RenderView(void); // must set r_refdef and call R_UpdateVariables first


void R_InitSky (unsigned char *src, int bytesperpixel); // called at level load

void R_View_WorldVisibility();
void R_DrawParticles(void);
void R_DrawExplosions(void);

#define gl_solid_format 3
#define gl_alpha_format 4

int R_CullBox(const vec3_t mins, const vec3_t maxs);
int R_CullBoxCustomPlanes(const vec3_t mins, const vec3_t maxs, int numplanes, const mplane_t *planes);

#include "r_modules.h"

#include "meshqueue.h"

#include "r_lerpanim.h"

extern cvar_t r_render;
extern cvar_t r_waterwarp;

extern cvar_t r_textureunits;
extern cvar_t r_glsl;
extern cvar_t r_glsl_offsetmapping;
extern cvar_t r_glsl_offsetmapping_reliefmapping;
extern cvar_t r_glsl_offsetmapping_scale;
extern cvar_t r_glsl_deluxemapping;

extern cvar_t gl_polyblend;
extern cvar_t gl_dither;

extern cvar_t r_smoothnormals_areaweighting;

extern cvar_t r_test;

#include "gl_backend.h"

#include "r_light.h"

extern rtexture_t *r_texture_blanknormalmap;
extern rtexture_t *r_texture_white;
extern rtexture_t *r_texture_black;
extern rtexture_t *r_texture_notexture;
extern rtexture_t *r_texture_whitecube;
extern rtexture_t *r_texture_normalizationcube;
extern rtexture_t *r_texture_fogattenuation;
//extern rtexture_t *r_texture_fogintensity;

void R_TimeReport(char *name);

// r_stain
void R_Stain(const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2);

void R_CalcBeam_Vertex3f(float *vert, const vec3_t org1, const vec3_t org2, float width);
void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, rtexture_t *fogtexture, int depthdisable, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca);

extern mempool_t *r_main_mempool;

extern int rsurface_array_size;
extern float *rsurface_array_modelvertex3f;
extern float *rsurface_array_modelsvector3f;
extern float *rsurface_array_modeltvector3f;
extern float *rsurface_array_modelnormal3f;
extern float *rsurface_array_deformedvertex3f;
extern float *rsurface_array_deformedsvector3f;
extern float *rsurface_array_deformedtvector3f;
extern float *rsurface_array_deformednormal3f;
extern float *rsurface_array_color4f;
extern float *rsurface_array_texcoord3f;

typedef enum rsurfmode_e
{
	RSURFMODE_NONE,
	RSURFMODE_SHOWSURFACES,
	RSURFMODE_SKY,
	RSURFMODE_MULTIPASS,
	RSURFMODE_GLSL
}
rsurfmode_t;

extern float *rsurface_modelvertex3f;
extern int rsurface_modelvertex3f_bufferobject;
extern size_t rsurface_modelvertex3f_bufferoffset;
extern float *rsurface_modelsvector3f;
extern int rsurface_modelsvector3f_bufferobject;
extern size_t rsurface_modelsvector3f_bufferoffset;
extern float *rsurface_modeltvector3f;
extern int rsurface_modeltvector3f_bufferobject;
extern size_t rsurface_modeltvector3f_bufferoffset;
extern float *rsurface_modelnormal3f;
extern int rsurface_modelnormal3f_bufferobject;
extern size_t rsurface_modelnormal3f_bufferoffset;
extern float *rsurface_vertex3f;
extern int rsurface_vertex3f_bufferobject;
extern size_t rsurface_vertex3f_bufferoffset;
extern float *rsurface_svector3f;
extern int rsurface_svector3f_bufferobject;
extern size_t rsurface_svector3f_bufferoffset;
extern float *rsurface_tvector3f;
extern int rsurface_tvector3f_bufferobject;
extern size_t rsurface_tvector3f_bufferoffset;
extern float *rsurface_normal3f;
extern int rsurface_normal3f_bufferobject;
extern size_t rsurface_normal3f_bufferoffset;
extern float *rsurface_lightmapcolor4f;
extern int rsurface_lightmapcolor4f_bufferobject;
extern size_t rsurface_lightmapcolor4f_bufferoffset;
extern vec3_t rsurface_modelorg;
extern qboolean rsurface_generatedvertex;
extern const entity_render_t *rsurface_entity;
extern const model_t *rsurface_model;
extern texture_t *rsurface_texture;
extern qboolean rsurface_uselightmaptexture;
extern rsurfmode_t rsurface_mode;

void RSurf_ActiveWorldEntity(void);
void RSurf_ActiveModelEntity(const entity_render_t *ent, qboolean wantnormals, qboolean wanttangents);
void RSurf_CleanUp(void);

void R_Mesh_ResizeArrays(int newvertices);

struct entity_render_s;
struct texture_s;
struct msurface_s;
void R_UpdateTextureInfo(const entity_render_t *ent, texture_t *t);
void R_UpdateAllTextureInfo(entity_render_t *ent);
void R_QueueTextureSurfaceList(int texturenumsurfaces, msurface_t **texturesurfacelist);
void R_DrawWorldSurfaces(qboolean skysurfaces);
void R_DrawModelSurfaces(entity_render_t *ent, qboolean skysurfaces);

void RSurf_PrepareVerticesForBatch(qboolean generatenormals, qboolean generatetangents, int texturenumsurfaces, msurface_t **texturesurfacelist);
void RSurf_DrawBatch_Simple(int texturenumsurfaces, msurface_t **texturesurfacelist);

#define SHADERPERMUTATION_MODE_LIGHTSOURCE (1<<0) // (lightsource) use directional pixel shading from light source (rtlight)
#define SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_MODELSPACE (1<<1) // (lightmap) use directional pixel shading from texture containing modelspace light directions (deluxemap)
#define SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_TANGENTSPACE (1<<2) // (lightmap) use directional pixel shading from texture containing tangentspace light directions (deluxemap)
#define SHADERPERMUTATION_MODE_LIGHTDIRECTION (1<<3) // (lightmap) use directional pixel shading from fixed light direction (q3bsp)
#define SHADERPERMUTATION_GLOW (1<<4) // (lightmap) blend in an additive glow texture
#define SHADERPERMUTATION_FOG (1<<5) // tint the color by fog color or black if using additive blend mode
#define SHADERPERMUTATION_COLORMAPPING (1<<6) // indicates this is a colormapped skin
#define SHADERPERMUTATION_DIFFUSE (1<<7) // (lightsource) whether to use directional shading
#define SHADERPERMUTATION_SPECULAR (1<<8) // (lightsource or deluxemapping) render specular effects
#define SHADERPERMUTATION_CUBEFILTER (1<<9) // (lightsource) use cubemap light filter
#define SHADERPERMUTATION_OFFSETMAPPING (1<<10) // adjust texcoords to roughly simulate a displacement mapped surface
#define SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING (1<<11) // adjust texcoords to accurately simulate a displacement mapped surface (requires OFFSETMAPPING to also be set!)

#define SHADERPERMUTATION_MAX (1<<12) // how many permutations are possible
#define SHADERPERMUTATION_MASK (SHADERPERMUTATION_MAX - 1) // mask of valid indexing bits for r_glsl_permutations[] array

// these are additional flags used only by R_GLSL_CompilePermutation
#define SHADERPERMUTATION_USES_VERTEXSHADER (1<<29)
#define SHADERPERMUTATION_USES_GEOMETRYSHADER (1<<30)
#define SHADERPERMUTATION_USES_FRAGMENTSHADER (1<<31)

typedef struct r_glsl_permutation_s
{
	// indicates if we have tried compiling this permutation already
	qboolean compiled;
	// 0 if compilation failed
	int program;
	int loc_Texture_Normal;
	int loc_Texture_Color;
	int loc_Texture_Gloss;
	int loc_Texture_Cube;
	int loc_Texture_Attenuation;
	int loc_Texture_FogMask;
	int loc_Texture_Pants;
	int loc_Texture_Shirt;
	int loc_Texture_Lightmap;
	int loc_Texture_Deluxemap;
	int loc_Texture_Glow;
	int loc_FogColor;
	int loc_LightPosition;
	int loc_EyePosition;
	int loc_LightColor;
	int loc_Color_Pants;
	int loc_Color_Shirt;
	int loc_FogRangeRecip;
	int loc_AmbientScale;
	int loc_DiffuseScale;
	int loc_SpecularScale;
	int loc_SpecularPower;
	int loc_GlowScale;
	int loc_SceneBrightness;
	int loc_OffsetMapping_Scale;
	int loc_AmbientColor;
	int loc_DiffuseColor;
	int loc_SpecularColor;
	int loc_LightDir;
}
r_glsl_permutation_t;

// information about each possible shader permutation
extern r_glsl_permutation_t r_glsl_permutations[SHADERPERMUTATION_MAX];
// currently selected permutation
extern r_glsl_permutation_t *r_glsl_permutation;

void R_GLSL_CompilePermutation(const char *shaderfilename, int permutation);
int R_SetupSurfaceShader(const vec3_t lightcolorbase, qboolean modellighting, float ambientscale, float diffusescale, float specularscale);
void R_SwitchSurfaceShader(int permutation);

#endif

