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

void R_SkinFrame_PrepareForPurge(void);
void R_SkinFrame_MarkUsed(skinframe_t *skinframe);
void R_SkinFrame_Purge(void);
skinframe_t *R_SkinFrame_Find(const char *name, int textureflags, int comparewidth, int compareheight, int comparecrc, qboolean add);
skinframe_t *R_SkinFrame_LoadExternal(const char *name, int textureflags, qboolean complain);
skinframe_t *R_SkinFrame_LoadInternal(const char *name, int textureflags, int loadpantsandshirt, int loadglowtexture, const unsigned char *skindata, int width, int height, int bitsperpixel, const unsigned int *palette, const unsigned int *alphapalette);
skinframe_t *R_SkinFrame_LoadMissing(void);

void R_View_WorldVisibility(qboolean forcenovis);
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
extern rtexture_t *r_texture_grey128;
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
void R_DrawSprite(int blendfunc1, int blendfunc2, rtexture_t *texture, rtexture_t *fogtexture, qboolean depthdisable, qboolean depthshort, const vec3_t origin, const vec3_t left, const vec3_t up, float scalex1, float scalex2, float scaley1, float scaley2, float cr, float cg, float cb, float ca);

extern mempool_t *r_main_mempool;

typedef enum rsurfmode_e
{
	RSURFMODE_NONE,
	RSURFMODE_SHOWSURFACES,
	RSURFMODE_SKY,
	RSURFMODE_MULTIPASS,
	RSURFMODE_GLSL
}
rsurfmode_t;

typedef struct rsurfacestate_s
{
	// processing buffers
	int array_size;
	float *array_modelvertex3f;
	float *array_modelsvector3f;
	float *array_modeltvector3f;
	float *array_modelnormal3f;
	float *array_deformedvertex3f;
	float *array_deformedsvector3f;
	float *array_deformedtvector3f;
	float *array_deformednormal3f;
	float *array_generatedtexcoordtexture2f;
	float *array_color4f;
	float *array_texcoord3f;

	// current model array pointers
	// these may point to processing buffers if model is animated,
	// otherwise they point to static data.
	// these are not directly used for rendering, they are just another level
	// of processing
	//
	// these either point at array_model* buffers (if the model is animated)
	// or the model->surfmesh.data_* buffers (if the model is not animated)
	//
	// these are only set when an entity render begins, they do not change on
	// a per surface basis.
	//
	// this indicates the model* arrays are pointed at array_model* buffers
	// (in other words, the model has been animated in software)
	qboolean generatedvertex;
	float *modelvertex3f;
	int modelvertex3f_bufferobject;
	size_t modelvertex3f_bufferoffset;
	float *modelsvector3f;
	int modelsvector3f_bufferobject;
	size_t modelsvector3f_bufferoffset;
	float *modeltvector3f;
	int modeltvector3f_bufferobject;
	size_t modeltvector3f_bufferoffset;
	float *modelnormal3f;
	int modelnormal3f_bufferobject;
	size_t modelnormal3f_bufferoffset;
	float *modellightmapcolor4f;
	int modellightmapcolor4f_bufferobject;
	size_t modellightmapcolor4f_bufferoffset;
	float *modeltexcoordtexture2f;
	int modeltexcoordtexture2f_bufferobject;
	size_t modeltexcoordtexture2f_bufferoffset;
	float *modeltexcoordlightmap2f;
	int modeltexcoordlightmap2f_bufferobject;
	size_t modeltexcoordlightmap2f_bufferoffset;
	int *modelelement3i;
	int modelelement3i_bufferobject;
	int *modellightmapoffsets;
	int modelnum_vertices;
	int modelnum_triangles;
	msurface_t *modelsurfaces;
	// current rendering array pointers
	// these may point to any of several different buffers depending on how
	// much processing was needed to prepare this model for rendering
	// these usually equal the model* pointers, they only differ if
	// deformvertexes is used in a q3 shader, and consequently these can
	// change on a per-surface basis (according to rsurface.texture)
	//
	// the exception is the color array which is often generated based on
	// colormod, alpha fading, and fogging, it may also come from q3bsp vertex
	// lighting of certain surfaces
	float *vertex3f;
	int vertex3f_bufferobject;
	size_t vertex3f_bufferoffset;
	float *svector3f;
	int svector3f_bufferobject;
	size_t svector3f_bufferoffset;
	float *tvector3f;
	int tvector3f_bufferobject;
	size_t tvector3f_bufferoffset;
	float *normal3f;
	int normal3f_bufferobject;
	size_t normal3f_bufferoffset;
	float *lightmapcolor4f;
	int lightmapcolor4f_bufferobject;
	size_t lightmapcolor4f_bufferoffset;
	float *texcoordtexture2f;
	int texcoordtexture2f_bufferobject;
	size_t texcoordtexture2f_bufferoffset;
	float *texcoordlightmap2f;
	int texcoordlightmap2f_bufferobject;
	size_t texcoordlightmap2f_bufferoffset;
	// transform matrices to render this entity and effects on this entity
	matrix4x4_t matrix;
	matrix4x4_t inversematrix;
	// animation blending state from entity
	frameblend_t frameblend[4];
	// directional model shading state from entity
	vec3_t modellight_ambient;
	vec3_t modellight_diffuse;
	vec3_t modellight_lightdir;
	// colormapping state from entity (these are black if colormapping is off)
	vec3_t colormap_pantscolor;
	vec3_t colormap_shirtcolor;
	// view location in model space
	vec3_t modelorg; // TODO: rename this
	// current texture in batching code
	texture_t *texture;
	// whether lightmapping is active on this batch
	// (otherwise vertex colored)
	qboolean uselightmaptexture;
	// one of the RSURFMODE_ values
	rsurfmode_t mode;
	// type of vertex lighting being used on this batch
	int lightmode; // 0 = lightmap or fullbright, 1 = color array from q3bsp, 2 = vertex shaded model

	// rtlight rendering
	// light currently being rendered
	rtlight_t *rtlight;
	// current light's cull box (copied out of an rtlight or calculated by GetLightInfo)
	vec3_t rtlight_cullmins;
	vec3_t rtlight_cullmaxs;
	// current light's culling planes
	int rtlight_numfrustumplanes;
	mplane_t rtlight_frustumplanes[12+6+6]; // see R_Shadow_ComputeShadowCasterCullingPlanes

	// this is the location of the light in entity space
	vec3_t entitylightorigin;
	// this transforms entity coordinates to light filter cubemap coordinates
	// (also often used for other purposes)
	matrix4x4_t entitytolight;
	// based on entitytolight this transforms -1 to +1 to 0 to 1 for purposes
	// of attenuation texturing in full 3D (Z result often ignored)
	matrix4x4_t entitytoattenuationxyz;
	// this transforms only the Z to S, and T is always 0.5
	matrix4x4_t entitytoattenuationz;
}
rsurfacestate_t;

extern rsurfacestate_t rsurface;

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
void R_DrawWorldSurfaces(qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean addwaterplanes, qboolean debug);
void R_DrawModelSurfaces(entity_render_t *ent, qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean addwaterplanes, qboolean debug);

void RSurf_PrepareVerticesForBatch(qboolean generatenormals, qboolean generatetangents, int texturenumsurfaces, msurface_t **texturesurfacelist);
void RSurf_DrawBatch_Simple(int texturenumsurfaces, msurface_t **texturesurfacelist);

#define SHADERPERMUTATION_MODE_LIGHTMAP (1<<0) // (lightmap) use directional pixel shading from fixed light direction (q3bsp)
#define SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_MODELSPACE (1<<1) // (lightmap) use directional pixel shading from texture containing modelspace light directions (deluxemap)
#define SHADERPERMUTATION_MODE_LIGHTDIRECTIONMAP_TANGENTSPACE (1<<2) // (lightmap) use directional pixel shading from texture containing tangentspace light directions (deluxemap)
#define SHADERPERMUTATION_MODE_LIGHTDIRECTION (1<<3) // (lightmap) use directional pixel shading from fixed light direction (q3bsp)
#define SHADERPERMUTATION_MODE_LIGHTSOURCE (1<<4) // (lightsource) use directional pixel shading from light source (rtlight)
#define SHADERPERMUTATION_WATER (1<<5) // normalmap-perturbed refraction of the background, performed behind the surface (the texture or material must be transparent to see it)
#define SHADERPERMUTATION_REFLECTION (1<<6) // normalmap-perturbed reflection of the scene infront of the surface, preformed as an overlay on the surface
#define SHADERPERMUTATION_GLOW (1<<7) // (lightmap) blend in an additive glow texture
#define SHADERPERMUTATION_FOG (1<<8) // tint the color by fog color or black if using additive blend mode
#define SHADERPERMUTATION_COLORMAPPING (1<<9) // indicates this is a colormapped skin
#define SHADERPERMUTATION_DIFFUSE (1<<10) // (lightsource) whether to use directional shading
#define SHADERPERMUTATION_CONTRASTBOOST (1<<11) // r_glsl_contrastboost boosts the contrast at low color levels (similar to gamma)
#define SHADERPERMUTATION_SPECULAR (1<<12) // (lightsource or deluxemapping) render specular effects
#define SHADERPERMUTATION_CUBEFILTER (1<<13) // (lightsource) use cubemap light filter
#define SHADERPERMUTATION_OFFSETMAPPING (1<<14) // adjust texcoords to roughly simulate a displacement mapped surface
#define SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING (1<<15) // adjust texcoords to accurately simulate a displacement mapped surface (requires OFFSETMAPPING to also be set!)

#define SHADERPERMUTATION_MAX (1<<16) // how many permutations are possible
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
	int loc_Texture_Refraction;
	int loc_Texture_Reflection;
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
	int loc_SceneBrightness; // or: Scenebrightness * ContrastBoost
	int loc_OffsetMapping_Scale;
	int loc_AmbientColor;
	int loc_DiffuseColor;
	int loc_SpecularColor;
	int loc_LightDir;
	int loc_ContrastBoostCoeff; // 1 - 1/ContrastBoost
	int loc_DistortScaleRefractReflect;
	int loc_ScreenScaleRefractReflect;
	int loc_ScreenCenterRefractReflect;
	int loc_RefractColor;
	int loc_ReflectColor;
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

