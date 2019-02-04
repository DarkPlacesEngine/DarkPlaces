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

typedef enum glsl_attrib_e
{
	GLSLATTRIB_POSITION = 0,
	GLSLATTRIB_COLOR = 1,
	GLSLATTRIB_TEXCOORD0 = 2,
	GLSLATTRIB_TEXCOORD1 = 3,
	GLSLATTRIB_TEXCOORD2 = 4,
	GLSLATTRIB_TEXCOORD3 = 5,
	GLSLATTRIB_TEXCOORD4 = 6,
	GLSLATTRIB_TEXCOORD5 = 7,
	GLSLATTRIB_TEXCOORD6 = 8,
	GLSLATTRIB_TEXCOORD7 = 9,
}
glsl_attrib;

typedef enum shaderlanguage_e
{
	SHADERLANGUAGE_GLSL,
	SHADERLANGUAGE_COUNT
}
shaderlanguage_t;

// this enum selects which of the glslshadermodeinfo entries should be used
typedef enum shadermode_e
{
	SHADERMODE_GENERIC, ///< (particles/HUD/etc) vertex color, optionally multiplied by one texture
	SHADERMODE_POSTPROCESS, ///< postprocessing shader (r_glsl_postprocess)
	SHADERMODE_DEPTH_OR_SHADOW, ///< (depthfirst/shadows) vertex shader only
	SHADERMODE_FLATCOLOR, ///< (lightmap) modulate texture by uniform color (q1bsp, q3bsp)
	SHADERMODE_VERTEXCOLOR, ///< (lightmap) modulate texture by vertex colors (q3bsp)
	SHADERMODE_LIGHTMAP, ///< (lightmap) modulate texture by lightmap texture (q1bsp, q3bsp)
	SHADERMODE_LIGHTDIRECTIONMAP_MODELSPACE, ///< (lightmap) use directional pixel shading from texture containing modelspace light directions (q3bsp deluxemap)
	SHADERMODE_LIGHTDIRECTIONMAP_TANGENTSPACE, ///< (lightmap) use directional pixel shading from texture containing tangentspace light directions (q1bsp deluxemap)
	SHADERMODE_LIGHTDIRECTIONMAP_FORCED_LIGHTMAP, // forced deluxemapping for lightmapped surfaces
	SHADERMODE_LIGHTDIRECTIONMAP_FORCED_VERTEXCOLOR, // forced deluxemapping for vertexlit surfaces
	SHADERMODE_LIGHTDIRECTION, ///< (lightmap) use directional pixel shading from fixed light direction (q3bsp)
	SHADERMODE_LIGHTSOURCE, ///< (lightsource) use directional pixel shading from light source (rtlight)
	SHADERMODE_REFRACTION, ///< refract background (the material is rendered normally after this pass)
	SHADERMODE_WATER, ///< refract background and reflection (the material is rendered normally after this pass)
	SHADERMODE_DEFERREDGEOMETRY, ///< (deferred) render material properties to screenspace geometry buffers
	SHADERMODE_DEFERREDLIGHTSOURCE, ///< (deferred) use directional pixel shading from light source (rtlight) on screenspace geometry buffers
	SHADERMODE_COUNT
}
shadermode_t;

typedef enum shaderpermutation_e
{
	SHADERPERMUTATION_DIFFUSE = 1<<0, ///< (lightsource) whether to use directional shading
	SHADERPERMUTATION_VERTEXTEXTUREBLEND = 1<<1, ///< indicates this is a two-layer material blend based on vertex alpha (q3bsp)
	SHADERPERMUTATION_VIEWTINT = 1<<2, ///< view tint (postprocessing only), use vertex colors (generic only)
	SHADERPERMUTATION_COLORMAPPING = 1<<3, ///< indicates this is a colormapped skin
	SHADERPERMUTATION_SATURATION = 1<<4, ///< saturation (postprocessing only)
	SHADERPERMUTATION_FOGINSIDE = 1<<5, ///< tint the color by fog color or black if using additive blend mode
	SHADERPERMUTATION_FOGOUTSIDE = 1<<6, ///< tint the color by fog color or black if using additive blend mode
	SHADERPERMUTATION_FOGHEIGHTTEXTURE = 1<<7, ///< fog color and density determined by texture mapped on vertical axis
	SHADERPERMUTATION_FOGALPHAHACK = 1<<8, ///< fog color and density determined by texture mapped on vertical axis
	SHADERPERMUTATION_GAMMARAMPS = 1<<9, ///< gamma (postprocessing only)
	SHADERPERMUTATION_CUBEFILTER = 1<<10, ///< (lightsource) use cubemap light filter
	SHADERPERMUTATION_GLOW = 1<<11, ///< (lightmap) blend in an additive glow texture
	SHADERPERMUTATION_BLOOM = 1<<12, ///< bloom (postprocessing only)
	SHADERPERMUTATION_SPECULAR = 1<<13, ///< (lightsource or deluxemapping) render specular effects
	SHADERPERMUTATION_POSTPROCESSING = 1<<14, ///< user defined postprocessing (postprocessing only)
	SHADERPERMUTATION_REFLECTION = 1<<15, ///< normalmap-perturbed reflection of the scene infront of the surface, preformed as an overlay on the surface
	SHADERPERMUTATION_OFFSETMAPPING = 1<<16, ///< adjust texcoords to roughly simulate a displacement mapped surface
	SHADERPERMUTATION_OFFSETMAPPING_RELIEFMAPPING = 1<<17, ///< adjust texcoords to accurately simulate a displacement mapped surface (requires OFFSETMAPPING to also be set!)
	SHADERPERMUTATION_SHADOWMAP2D = 1<<18, ///< (lightsource) use shadowmap texture as light filter
	SHADERPERMUTATION_SHADOWMAPVSDCT = 1<<19, ///< (lightsource) use virtual shadow depth cube texture for shadowmap indexing
	SHADERPERMUTATION_SHADOWMAPORTHO = 1<<20, ///< (lightsource) use orthographic shadowmap projection
	SHADERPERMUTATION_DEFERREDLIGHTMAP = 1<<21, ///< (lightmap) read Texture_ScreenDiffuse/Specular textures and add them on top of lightmapping
	SHADERPERMUTATION_ALPHAKILL = 1<<22, ///< (deferredgeometry) discard pixel if diffuse texture alpha below 0.5, (generic) apply global alpha
	SHADERPERMUTATION_REFLECTCUBE = 1<<23, ///< fake reflections using global cubemap (not HDRI light probe)
	SHADERPERMUTATION_NORMALMAPSCROLLBLEND = 1<<24, ///< (water) counter-direction normalmaps scrolling
	SHADERPERMUTATION_BOUNCEGRID = 1<<25, ///< (lightmap) use Texture_BounceGrid as an additional source of ambient light
	SHADERPERMUTATION_BOUNCEGRIDDIRECTIONAL = 1<<26, ///< (lightmap) use 16-component pixels in bouncegrid texture for directional lighting rather than standard 4-component
	SHADERPERMUTATION_TRIPPY = 1<<27, ///< use trippy vertex shader effect
	SHADERPERMUTATION_DEPTHRGB = 1<<28, ///< read/write depth values in RGB color coded format for older hardware without depth samplers
	SHADERPERMUTATION_ALPHAGEN_VERTEX = 1<<29, ///< alphaGen vertex
	SHADERPERMUTATION_SKELETAL = 1<<30, ///< (skeletal models) use skeletal matrices to deform vertices (gpu-skinning)
	SHADERPERMUTATION_OCCLUDE = 1<<31, ///< use occlusion buffer for corona
	SHADERPERMUTATION_COUNT = 32 ///< size of shaderpermutationinfo array
}
shaderpermutation_t;

// 1.0f / N table
extern float ixtable[4096];

// fog stuff
void FOG_clear(void);

// sky stuff
extern cvar_t r_sky;
extern cvar_t r_skyscroll1;
extern cvar_t r_skyscroll2;
extern cvar_t r_sky_scissor;
extern cvar_t r_q3bsp_renderskydepth;
extern int skyrenderlater, skyrendermasked;
extern int skyscissor[4];
int R_SetSkyBox(const char *sky);
void R_SkyStartFrame(void);
void R_Sky(void);
void R_ResetSkyBox(void);

// SHOWLMP stuff (Nehahra)
void SHOWLMP_decodehide(void);
void SHOWLMP_decodeshow(void);
void SHOWLMP_drawall(void);

// render profiling stuff
extern int r_timereport_active;

// lighting stuff
extern cvar_t r_ambient;
extern cvar_t gl_flashblend;

// vis stuff
extern cvar_t r_novis;

extern cvar_t r_trippy;
extern cvar_t r_fxaa;

extern cvar_t r_lerpsprites;
extern cvar_t r_lerpmodels;
extern cvar_t r_lerplightstyles;
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
	// snapping epsilon
	float epsilon2;
}
rmesh_t;

// useful functions for rendering
void R_ModulateColors(float *in, float *out, int verts, float r, float g, float b);
void R_FillColors(float *out, int verts, float r, float g, float b, float a);
void R_Mesh_AddPolygon3f(rmesh_t *mesh, int numvertices, float *vertex3f);
void R_Mesh_AddBrushMeshFromPlanes(rmesh_t *mesh, int numplanes, mplane_t *planes);

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

extern cvar_t r_nearclip;

// forces all rendering to draw triangle outlines
extern cvar_t r_showoverdraw;
extern cvar_t r_showtris;
extern cvar_t r_shownormals;
extern cvar_t r_showlighting;
extern cvar_t r_showcollisionbrushes;
extern cvar_t r_showcollisionbrushes_polygonfactor;
extern cvar_t r_showcollisionbrushes_polygonoffset;
extern cvar_t r_showdisabledepthtest;

extern cvar_t r_drawentities;
extern cvar_t r_draw2d;
extern qboolean r_draw2d_force;
extern cvar_t r_drawviewmodel;
extern cvar_t r_drawworld;
extern cvar_t r_speeds;
extern cvar_t r_fullbright;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;

void R_UpdateVariables(void); // must call after setting up most of r_refdef, but before calling R_RenderView
void R_RenderView(int fbo, rtexture_t *depthtexture, rtexture_t *colortexture, int x, int y, int width, int height); // must set r_refdef and call R_UpdateVariables and CL_UpdateEntityShading first
void R_RenderView_UpdateViewVectors(void); // just updates r_refdef.view.{forward,left,up,origin,right,inverse_matrix}

typedef enum r_refdef_scene_type_s {
	RST_CLIENT,
	RST_MENU,
	RST_COUNT
} r_refdef_scene_type_t;

void R_SelectScene( r_refdef_scene_type_t scenetype );
r_refdef_scene_t * R_GetScenePointer( r_refdef_scene_type_t scenetype );

void R_SkinFrame_PrepareForPurge(void);
void R_SkinFrame_MarkUsed(skinframe_t *skinframe);
void R_SkinFrame_PurgeSkinFrame(skinframe_t *skinframe);
void R_SkinFrame_Purge(void);
// set last to NULL to start from the beginning
skinframe_t *R_SkinFrame_FindNextByName( skinframe_t *last, const char *name );
skinframe_t *R_SkinFrame_Find(const char *name, int textureflags, int comparewidth, int compareheight, int comparecrc, qboolean add);
skinframe_t *R_SkinFrame_LoadExternal(const char *name, int textureflags, qboolean complain, qboolean fallbacknotexture);
skinframe_t *R_SkinFrame_LoadExternal_SkinFrame(skinframe_t *skinframe, const char *name, int textureflags, qboolean complain, qboolean fallbacknotexture);
skinframe_t *R_SkinFrame_LoadInternalBGRA(const char *name, int textureflags, const unsigned char *skindata, int width, int height, int comparewidth, int compareheight, int comparecrc, qboolean sRGB);
skinframe_t *R_SkinFrame_LoadInternalQuake(const char *name, int textureflags, int loadpantsandshirt, int loadglowtexture, const unsigned char *skindata, int width, int height);
skinframe_t *R_SkinFrame_LoadInternal8bit(const char *name, int textureflags, const unsigned char *skindata, int width, int height, const unsigned int *palette, const unsigned int *alphapalette);
skinframe_t *R_SkinFrame_LoadMissing(void);
skinframe_t *R_SkinFrame_LoadNoTexture(void);
skinframe_t *R_SkinFrame_LoadInternalUsingTexture(const char *name, int textureflags, rtexture_t *tex, int width, int height, qboolean sRGB);

rtexture_t *R_GetCubemap(const char *basename);

void R_View_WorldVisibility(qboolean forcenovis);
void R_DrawDecals(void);
void R_DrawParticles(void);
void R_DrawExplosions(void);

#define gl_solid_format 3
#define gl_alpha_format 4

int R_CullBox(const vec3_t mins, const vec3_t maxs);
int R_CullBoxCustomPlanes(const vec3_t mins, const vec3_t maxs, int numplanes, const mplane_t *planes);
qboolean R_CanSeeBox(int numsamples, vec_t eyejitter, vec_t entboxenlarge, vec_t entboxexpand, vec_t pad, vec3_t eye, vec3_t entboxmins, vec3_t entboxmaxs);

#include "r_modules.h"

#include "meshqueue.h"

/// free all R_FrameData memory
void R_FrameData_Reset(void);
/// prepare for a new frame, recycles old buffers if a resize occurred previously
void R_FrameData_NewFrame(void);
/// allocate some temporary memory for your purposes
void *R_FrameData_Alloc(size_t size);
/// allocate some temporary memory and copy this data into it
void *R_FrameData_Store(size_t size, void *data);
/// set a marker that allows you to discard the following temporary memory allocations
void R_FrameData_SetMark(void);
/// discard recent memory allocations (rewind to marker)
void R_FrameData_ReturnToMark(void);

/// enum of the various types of hardware buffer object used in rendering
/// note that the r_buffermegs[] array must be maintained to match this
typedef enum r_bufferdata_type_e
{
	R_BUFFERDATA_VERTEX, /// vertex buffer
	R_BUFFERDATA_INDEX16, /// index buffer - 16bit (because D3D cares)
	R_BUFFERDATA_INDEX32, /// index buffer - 32bit (because D3D cares)
	R_BUFFERDATA_UNIFORM, /// uniform buffer
	R_BUFFERDATA_COUNT /// how many kinds of buffer we have
}
r_bufferdata_type_t;

/// free all dynamic vertex/index/uniform buffers
void R_BufferData_Reset(void);
/// begin a new frame (recycle old buffers)
void R_BufferData_NewFrame(void);
/// request space in a vertex/index/uniform buffer for the chosen data, returns the buffer pointer and offset, always successful
r_meshbuffer_t *R_BufferData_Store(size_t size, const void *data, r_bufferdata_type_t type, int *returnbufferoffset);

/// free all R_AnimCache memory
void R_AnimCache_Free(void);
/// clear the animcache pointers on all known render entities
void R_AnimCache_ClearCache(void);
/// get the skeletal data or cached animated mesh data for an entity (optionally with normals and tangents)
qboolean R_AnimCache_GetEntity(entity_render_t *ent, qboolean wantnormals, qboolean wanttangents);
/// generate animcache data for all entities marked visible
void R_AnimCache_CacheVisibleEntities(void);

#include "r_lerpanim.h"

extern cvar_t r_render;
extern cvar_t r_renderview;
extern cvar_t r_waterwarp;

extern cvar_t r_textureunits;

extern cvar_t r_glsl_offsetmapping;
extern cvar_t r_glsl_offsetmapping_reliefmapping;
extern cvar_t r_glsl_offsetmapping_scale;
extern cvar_t r_glsl_offsetmapping_lod;
extern cvar_t r_glsl_offsetmapping_lod_distance;
extern cvar_t r_glsl_deluxemapping;

extern cvar_t gl_polyblend;

extern cvar_t cl_deathfade;

extern cvar_t r_smoothnormals_areaweighting;

extern cvar_t r_test;

#include "gl_backend.h"

extern rtexture_t *r_texture_blanknormalmap;
extern rtexture_t *r_texture_white;
extern rtexture_t *r_texture_grey128;
extern rtexture_t *r_texture_black;
extern rtexture_t *r_texture_notexture;
extern rtexture_t *r_texture_whitecube;
extern rtexture_t *r_texture_normalizationcube;
extern rtexture_t *r_texture_fogattenuation;
extern rtexture_t *r_texture_fogheighttexture;

extern unsigned int r_queries[MAX_OCCLUSION_QUERIES];
extern unsigned int r_numqueries;
extern unsigned int r_maxqueries;

void R_TimeReport(const char *name);

// r_stain
void R_Stain(const vec3_t origin, float radius, int cr1, int cg1, int cb1, int ca1, int cr2, int cg2, int cb2, int ca2);

void R_CalcBeam_Vertex3f(float *vert, const float *org1, const float *org2, float width);
void R_CalcSprite_Vertex3f(float *vertex3f, const float *origin, const float *left, const float *up, float scalex1, float scalex2, float scaley1, float scaley2);

extern mempool_t *r_main_mempool;

typedef struct rsurfacestate_s
{
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
	qboolean                    forcecurrenttextureupdate; // set for RSurf_ActiveCustomEntity to force R_GetCurrentTexture to recalculate the texture parameters (such as entity alpha)
	qboolean                    modelgeneratedvertex;
	// skeletal animation can be done by entity (animcache) or per batch,
	// batch may be non-skeletal even if entity is skeletal, indicating that
	// the dynamicvertex code path had to apply skeletal manually for a case
	// where gpu-skinning is not possible, for this reason batch has its own
	// variables
	int                         entityskeletalnumtransforms; // how many transforms are used for this mesh
	float                      *entityskeletaltransform3x4; // use gpu-skinning shader on this mesh
	const r_meshbuffer_t       *entityskeletaltransform3x4buffer; // uniform buffer
	int                         entityskeletaltransform3x4offset;
	int                         entityskeletaltransform3x4size;
	float                      *modelvertex3f;
	const r_meshbuffer_t       *modelvertex3f_vertexbuffer;
	int                         modelvertex3f_bufferoffset;
	float                      *modelsvector3f;
	const r_meshbuffer_t       *modelsvector3f_vertexbuffer;
	int                         modelsvector3f_bufferoffset;
	float                      *modeltvector3f;
	const r_meshbuffer_t       *modeltvector3f_vertexbuffer;
	int                         modeltvector3f_bufferoffset;
	float                      *modelnormal3f;
	const r_meshbuffer_t       *modelnormal3f_vertexbuffer;
	int                         modelnormal3f_bufferoffset;
	float                      *modellightmapcolor4f;
	const r_meshbuffer_t       *modellightmapcolor4f_vertexbuffer;
	int                         modellightmapcolor4f_bufferoffset;
	float                      *modeltexcoordtexture2f;
	const r_meshbuffer_t       *modeltexcoordtexture2f_vertexbuffer;
	int                         modeltexcoordtexture2f_bufferoffset;
	float                      *modeltexcoordlightmap2f;
	const r_meshbuffer_t       *modeltexcoordlightmap2f_vertexbuffer;
	int                         modeltexcoordlightmap2f_bufferoffset;
	unsigned char              *modelskeletalindex4ub;
	const r_meshbuffer_t       *modelskeletalindex4ub_vertexbuffer;
	int                         modelskeletalindex4ub_bufferoffset;
	unsigned char              *modelskeletalweight4ub;
	const r_meshbuffer_t       *modelskeletalweight4ub_vertexbuffer;
	int                         modelskeletalweight4ub_bufferoffset;
	int                        *modelelement3i;
	const r_meshbuffer_t       *modelelement3i_indexbuffer;
	int                         modelelement3i_bufferoffset;
	unsigned short             *modelelement3s;
	const r_meshbuffer_t       *modelelement3s_indexbuffer;
	int                         modelelement3s_bufferoffset;
	int                        *modellightmapoffsets;
	int                         modelnumvertices;
	int                         modelnumtriangles;
	const msurface_t           *modelsurfaces;
	// current rendering array pointers
	// these may point to any of several different buffers depending on how
	// much processing was needed to prepare this model for rendering
	// these usually equal the model* pointers, they only differ if
	// deformvertexes is used in a q3 shader, and consequently these can
	// change on a per-surface basis (according to rsurface.texture)
	qboolean                    batchgeneratedvertex;
	qboolean                    batchmultidraw;
	int                         batchmultidrawnumsurfaces;
	const msurface_t          **batchmultidrawsurfacelist;
	int                         batchfirstvertex;
	int                         batchnumvertices;
	int                         batchfirsttriangle;
	int                         batchnumtriangles;
	float                      *batchvertex3f;
	const r_meshbuffer_t       *batchvertex3f_vertexbuffer;
	int                         batchvertex3f_bufferoffset;
	float                      *batchsvector3f;
	const r_meshbuffer_t       *batchsvector3f_vertexbuffer;
	int                         batchsvector3f_bufferoffset;
	float                      *batchtvector3f;
	const r_meshbuffer_t       *batchtvector3f_vertexbuffer;
	int                         batchtvector3f_bufferoffset;
	float                      *batchnormal3f;
	const r_meshbuffer_t       *batchnormal3f_vertexbuffer;
	int                         batchnormal3f_bufferoffset;
	float                      *batchlightmapcolor4f;
	const r_meshbuffer_t       *batchlightmapcolor4f_vertexbuffer;
	int                         batchlightmapcolor4f_bufferoffset;
	float                      *batchtexcoordtexture2f;
	const r_meshbuffer_t       *batchtexcoordtexture2f_vertexbuffer;
	int                         batchtexcoordtexture2f_bufferoffset;
	float                      *batchtexcoordlightmap2f;
	const r_meshbuffer_t       *batchtexcoordlightmap2f_vertexbuffer;
	int                         batchtexcoordlightmap2f_bufferoffset;
	unsigned char              *batchskeletalindex4ub;
	const r_meshbuffer_t       *batchskeletalindex4ub_vertexbuffer;
	int                         batchskeletalindex4ub_bufferoffset;
	unsigned char              *batchskeletalweight4ub;
	const r_meshbuffer_t       *batchskeletalweight4ub_vertexbuffer;
	int                         batchskeletalweight4ub_bufferoffset;
	int                        *batchelement3i;
	const r_meshbuffer_t       *batchelement3i_indexbuffer;
	int                         batchelement3i_bufferoffset;
	unsigned short             *batchelement3s;
	const r_meshbuffer_t       *batchelement3s_indexbuffer;
	int                         batchelement3s_bufferoffset;
	int                         batchskeletalnumtransforms;
	float                      *batchskeletaltransform3x4;
	const r_meshbuffer_t       *batchskeletaltransform3x4buffer; // uniform buffer
	int                         batchskeletaltransform3x4offset;
	int                         batchskeletaltransform3x4size;

	// some important fields from the entity
	int ent_skinnum;
	int ent_qwskin;
	int ent_flags;
	int ent_alttextures; // used by q1bsp animated textures (pressed buttons)
	double shadertime; // r_refdef.scene.time - ent->shadertime
	// transform matrices to render this entity and effects on this entity
	matrix4x4_t matrix;
	matrix4x4_t inversematrix;
	// scale factors for transforming lengths into/out of entity space
	float matrixscale;
	float inversematrixscale;
	// animation blending state from entity
	frameblend_t frameblend[MAX_FRAMEBLENDS];
	skeleton_t *skeleton;
	// view location in model space
	vec3_t localvieworigin;
	// polygon offset data for submodels
	float basepolygonfactor;
	float basepolygonoffset;
	// current textures in batching code
	texture_t *texture;
	rtexture_t *lightmaptexture;
	rtexture_t *deluxemaptexture;
	// whether lightmapping is active on this batch
	// (otherwise vertex colored)
	qboolean uselightmaptexture;
	// fog plane in model space for direct application to vertices
	float fograngerecip;
	float fogmasktabledistmultiplier;
	float fogplane[4];
	float fogheightfade;
	float fogplaneviewdist;

	// rtlight rendering
	// light currently being rendered
	const rtlight_t *rtlight;

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

	// user wavefunc parameters (from csqc)
	float userwavefunc_param[Q3WAVEFUNC_USER_COUNT];

	// pointer to an entity_render_t used only by R_GetCurrentTexture and
	// RSurf_ActiveModelEntity as a unique id within each frame (see r_frame
	// also)
	entity_render_t *entity;
}
rsurfacestate_t;

extern rsurfacestate_t rsurface;

void R_HDR_UpdateIrisAdaptation(const vec3_t point);

void RSurf_ActiveModelEntity(const entity_render_t *ent, qboolean wantnormals, qboolean wanttangents, qboolean prepass);
void RSurf_ActiveCustomEntity(const matrix4x4_t *matrix, const matrix4x4_t *inversematrix, int entflags, double shadertime, float r, float g, float b, float a, int numvertices, const float *vertex3f, const float *texcoord2f, const float *normal3f, const float *svector3f, const float *tvector3f, const float *color4f, int numtriangles, const int *element3i, const unsigned short *element3s, qboolean wantnormals, qboolean wanttangents);
void RSurf_SetupDepthAndCulling(void);

extern int r_textureframe; ///< used only by R_GetCurrentTexture, incremented per view and per UI render
texture_t *R_GetCurrentTexture(texture_t *t);
void R_DrawModelSurfaces(entity_render_t *ent, qboolean skysurfaces, qboolean writedepth, qboolean depthonly, qboolean debug, qboolean prepass, qboolean ui);
void R_DrawCustomSurface(skinframe_t *skinframe, const matrix4x4_t *texmatrix, int materialflags, int firstvertex, int numvertices, int firsttriangle, int numtriangles, qboolean writedepth, qboolean prepass);
void R_DrawCustomSurface_Texture(texture_t *texture, const matrix4x4_t *texmatrix, int materialflags, int firstvertex, int numvertices, int firsttriangle, int numtriangles, qboolean writedepth, qboolean prepass);

#define BATCHNEED_ARRAY_VERTEX                (1<< 0) // set up rsurface.batchvertex3f
#define BATCHNEED_ARRAY_NORMAL                (1<< 1) // set up rsurface.batchnormal3f
#define BATCHNEED_ARRAY_VECTOR                (1<< 2) // set up rsurface.batchsvector3f and rsurface.batchtvector3f
#define BATCHNEED_ARRAY_VERTEXTINTCOLOR       (1<< 3) // set up rsurface.batchvertexcolor4f
#define BATCHNEED_ARRAY_VERTEXCOLOR           (1<< 4) // set up rsurface.batchlightmapcolor4f
#define BATCHNEED_ARRAY_TEXCOORD              (1<< 5) // set up rsurface.batchtexcoordtexture2f
#define BATCHNEED_ARRAY_LIGHTMAP              (1<< 6) // set up rsurface.batchtexcoordlightmap2f
#define BATCHNEED_ARRAY_SKELETAL              (1<< 7) // set up skeletal index and weight data for vertex shader
#define BATCHNEED_NOGAPS                      (1<< 8) // force vertex copying if firstvertex is not zero or there are gaps
#define BATCHNEED_ALWAYSCOPY                  (1<< 9) // force vertex copying unconditionally - useful if you want to modify colors
#define BATCHNEED_ALLOWMULTIDRAW              (1<<10) // allow multiple draws
void RSurf_PrepareVerticesForBatch(int batchneed, int texturenumsurfaces, const msurface_t **texturesurfacelist);
void RSurf_UploadBuffersForBatch(void);
void RSurf_DrawBatch(void);

void R_DecalSystem_SplatEntities(const vec3_t org, const vec3_t normal, float r, float g, float b, float a, float s1, float t1, float s2, float t2, float size);

typedef enum rsurfacepass_e
{
	RSURFPASS_BASE,
	RSURFPASS_BACKGROUND,
	RSURFPASS_RTLIGHT,
	RSURFPASS_DEFERREDGEOMETRY
}
rsurfacepass_t;

void R_SetupShader_Generic(rtexture_t *t, qboolean usegamma, qboolean notrippy, qboolean suppresstexalpha);
void R_SetupShader_Generic_NoTexture(qboolean usegamma, qboolean notrippy);
void R_SetupShader_DepthOrShadow(qboolean notrippy, qboolean depthrgb, qboolean skeletal);
void R_SetupShader_Surface(const float ambientcolor[3], const float diffusecolor[3], const float specularcolor[3], rsurfacepass_t rsurfacepass, int texturenumsurfaces, const msurface_t **texturesurfacelist, void *waterplane, qboolean notrippy);
void R_SetupShader_DeferredLight(const rtlight_t *rtlight);

typedef struct r_rendertarget_s {
	// texcoords for sampling from the viewport (clockwise: 0,0 1,0 1,1 0,1)
	float texcoord2f[8];
	// textures are this size and type
	int texturewidth;
	int textureheight;
	// TEXTYPE for each color target - usually TEXTYPE_COLORBUFFER16F
	textype_t colortextype[4];
	// TEXTYPE for depth target - usually TEXTYPE_DEPTHBUFFER24 or TEXTYPE_SHADOWMAP24_COMP
	textype_t depthtextype;
	// if true the depth target will be a renderbuffer rather than a texture (still rtexture_t though)
	qboolean depthisrenderbuffer;
	// framebuffer object referencing the textures
	int fbo;
	// there can be up to 4 color targets and 1 depth target, the depthtexture
	// may be a real texture (readable) or just a renderbuffer (not readable,
	// but potentially faster)
	rtexture_t *colortexture[4];
	rtexture_t *depthtexture;
	// a rendertarget will not be reused in the same frame (realtime == lastusetime),
	// on a new frame, matching rendertargets will be reused (texturewidth, textureheight, number of color and depth textures and their types),
	// when a new frame arrives the rendertargets can be reused by requests for matching texturewidth,textureheight and fbo configuration (the number of color and depth textures), when a rendertarget is not reused for > 200ms (realtime - lastusetime > 0.2) the rendertarget's resources will be freed (fbo, textures) and it can be reused for any target in future frames
	double lastusetime;
} r_rendertarget_t;

// called each frame after render to delete render targets that have not been used for a while
void R_RenderTarget_FreeUnused(qboolean force);
// returns a rendertarget, creates rendertarget if needed or intelligently reuses targets across frames if they match and have not been used already this frame
r_rendertarget_t *R_RenderTarget_Get(int texturewidth, int textureheight, textype_t depthtextype, qboolean depthisrenderbuffer, textype_t colortextype0, textype_t colortextype1, textype_t colortextype2, textype_t colortextype3);

typedef struct r_waterstate_waterplane_s
{
	r_rendertarget_t *rt_refraction; // MATERIALFLAG_WATERSHADER or MATERIALFLAG_REFRACTION
	r_rendertarget_t *rt_reflection; // MATERIALFLAG_WATERSHADER or MATERIALFLAG_REFLECTION
	r_rendertarget_t *rt_camera; // MATERIALFLAG_CAMERA
	mplane_t plane;
	int materialflags; // combined flags of all water surfaces on this plane
	unsigned char pvsbits[(MAX_MAP_LEAFS+7)>>3]; // FIXME: buffer overflow on huge maps
	qboolean pvsvalid;
	int camera_entity;
	vec3_t mins, maxs;
}
r_waterstate_waterplane_t;

typedef struct r_waterstate_s
{
	int waterwidth, waterheight;
	int texturewidth, textureheight;
	int camerawidth, cameraheight;

	int maxwaterplanes; // same as MAX_WATERPLANES
	int numwaterplanes;
	r_waterstate_waterplane_t waterplanes[MAX_WATERPLANES];

	float screenscale[2];
	float screencenter[2];

	qboolean enabled;

	qboolean renderingscene; // true while rendering a refraction or reflection texture, disables water surfaces
	qboolean hideplayer;
}
r_waterstate_t;

typedef struct r_framebufferstate_s
{
	textype_t textype; // type of color buffer we're using (dependent on r_viewfbo cvar)
	int screentexturewidth, screentextureheight; // dimensions of texture

	// rt_* fields are per-RenderView so we reset them in R_Bloom_StartFrame
	r_rendertarget_t *rt_screen;
	r_rendertarget_t *rt_bloom;

	rtexture_t *ghosttexture; // for r_motionblur (not recommended on multi-GPU hardware!)
	float ghosttexcoord2f[8]; // for r_motionblur

	int bloomwidth, bloomheight;

	// arrays for rendering the screen passes
	float offsettexcoord2f[8]; // temporary use while updating bloomtexture[]

	r_waterstate_t water;

	qboolean ghosttexture_valid; // don't draw garbage on first frame with motionblur
	qboolean usedepthtextures; // use depth texture instead of depth renderbuffer (faster if you need to read it later anyway)

	// rendertargets (fbo and viewport), these can be reused across frames
	memexpandablearray_t rendertargets;
}
r_framebufferstate_t;

extern r_framebufferstate_t r_fb;

extern cvar_t r_viewfbo;

void R_ResetViewRendering2D_Common(int viewfbo, rtexture_t *viewdepthtexture, rtexture_t *viewcolortexture, int viewx, int viewy, int viewwidth, int viewheight, float x2, float y2); // this is called by R_ResetViewRendering2D and _DrawQ_Setup and internal
void R_ResetViewRendering2D(int viewfbo, rtexture_t *viewdepthtexture, rtexture_t *viewcolortexture, int viewx, int viewy, int viewwidth, int viewheight);
void R_ResetViewRendering3D(int viewfbo, rtexture_t *viewdepthtexture, rtexture_t *viewcolortexture, int viewx, int viewy, int viewwidth, int viewheight);
void R_SetupView(qboolean allowwaterclippingplane, int viewfbo, rtexture_t *viewdepthtexture, rtexture_t *viewcolortexture, int viewx, int viewy, int viewwidth, int viewheight);
void R_DebugLine(vec3_t start, vec3_t end);
extern const float r_screenvertex3f[12];
extern cvar_t r_showspriteedges;
extern cvar_t r_showparticleedges;
extern cvar_t r_shadows;
extern cvar_t r_shadows_darken;
extern cvar_t r_shadows_drawafterrtlighting;
extern cvar_t r_shadows_castfrombmodels;
extern cvar_t r_shadows_throwdistance;
extern cvar_t r_shadows_throwdirection;
extern cvar_t r_shadows_focus;
extern cvar_t r_shadows_shadowmapscale;
extern cvar_t r_shadows_shadowmapbias;
extern cvar_t r_transparent_alphatocoverage;
extern cvar_t r_transparent_sortsurfacesbynearest;
extern cvar_t r_transparent_useplanardistance;
extern cvar_t r_transparent_sortarraysize;
extern cvar_t r_transparent_sortmindist;
extern cvar_t r_transparent_sortmaxdist;

extern qboolean r_shadow_usingdeferredprepass;
extern rtexture_t *r_shadow_attenuationgradienttexture;
extern rtexture_t *r_shadow_attenuation2dtexture;
extern rtexture_t *r_shadow_attenuation3dtexture;
extern qboolean r_shadow_usingshadowmap2d;
extern qboolean r_shadow_usingshadowmaportho;
extern float r_shadow_modelshadowmap_texturescale[4];
extern float r_shadow_modelshadowmap_parameters[4];
extern float r_shadow_lightshadowmap_texturescale[4];
extern float r_shadow_lightshadowmap_parameters[4];
extern qboolean r_shadow_shadowmapvsdct;
extern rtexture_t *r_shadow_shadowmap2ddepthbuffer;
extern rtexture_t *r_shadow_shadowmap2ddepthtexture;
extern rtexture_t *r_shadow_shadowmapvsdcttexture;
extern matrix4x4_t r_shadow_shadowmapmatrix;
extern int r_shadow_prepass_width;
extern int r_shadow_prepass_height;
extern rtexture_t *r_shadow_prepassgeometrydepthbuffer;
extern rtexture_t *r_shadow_prepassgeometrynormalmaptexture;
extern rtexture_t *r_shadow_prepasslightingdiffusetexture;
extern rtexture_t *r_shadow_prepasslightingspeculartexture;
extern int r_shadow_viewfbo;
extern rtexture_t *r_shadow_viewdepthtexture;
extern rtexture_t *r_shadow_viewcolortexture;
extern int r_shadow_viewx;
extern int r_shadow_viewy;
extern int r_shadow_viewwidth;
extern int r_shadow_viewheight;

void R_RenderScene(int viewfbo, rtexture_t *viewdepthtexture, rtexture_t *viewcolortexture, int viewx, int viewy, int viewwidth, int viewheight);
void R_RenderWaterPlanes(int viewfbo, rtexture_t *viewdepthtexture, rtexture_t *viewcolortexture, int viewx, int viewy, int viewwidth, int viewheight);

void R_Model_Sprite_Draw(entity_render_t *ent);

struct prvm_prog_s;
void R_UpdateFog(void);
qboolean CL_VM_UpdateView(double frametime);
void SCR_DrawConsole(void);
void R_Shadow_EditLights_DrawSelectedLightProperties(void);
void R_DecalSystem_Reset(decalsystem_t *decalsystem);
void R_Shadow_UpdateBounceGridTexture(void);
void R_DrawPortals(void);
void R_BuildLightMap(const entity_render_t *ent, msurface_t *surface);
void R_Water_AddWaterPlane(msurface_t *surface, int entno);
int R_Shadow_GetRTLightInfo(unsigned int lightindex, float *origin, float *radius, float *color);
dp_font_t *FindFont(const char *title, qboolean allocate_new);
void LoadFont(qboolean override, const char *name, dp_font_t *fnt, float scale, float voffset);

void Render_Init(void);

// these are called by Render_Init
void R_Textures_Init(void);
void GL_Draw_Init(void);
void GL_Main_Init(void);
void R_Shadow_Init(void);
void R_Sky_Init(void);
void GL_Surf_Init(void);
void R_Particles_Init(void);
void R_Explosion_Init(void);
void gl_backend_init(void);
void Sbar_Init(void);
void R_LightningBeams_Init(void);
void Mod_RenderInit(void);
void Font_Init(void);

qboolean R_CompileShader_CheckStaticParms(void);
void R_GLSL_Restart_f(void);

#endif
