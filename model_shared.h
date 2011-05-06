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

#ifndef MODEL_SHARED_H
#define MODEL_SHARED_H

typedef enum synctype_e {ST_SYNC=0, ST_RAND } synctype_t;

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

typedef enum modtype_e {mod_invalid, mod_brushq1, mod_sprite, mod_alias, mod_brushq2, mod_brushq3, mod_obj, mod_null} modtype_t;

typedef struct animscene_s
{
	char name[32]; // for viewthing support
	int firstframe;
	int framecount;
	int loop; // true or false
	float framerate;
}
animscene_t;

typedef struct skinframe_s
{
	rtexture_t *stain; // inverse modulate with background (used for decals and such)
	rtexture_t *merged; // original texture without glow
	rtexture_t *base; // original texture without pants/shirt/glow
	rtexture_t *pants; // pants only (in greyscale)
	rtexture_t *shirt; // shirt only (in greyscale)
	rtexture_t *nmap; // normalmap (bumpmap for dot3)
	rtexture_t *gloss; // glossmap (for dot3)
	rtexture_t *glow; // glow only (fullbrights)
	rtexture_t *fog; // alpha of the base texture (if not opaque)
	rtexture_t *reflect; // colored mask for cubemap reflections
	// accounting data for hash searches:
	// the compare variables are used to identify internal skins from certain
	// model formats
	// (so that two q1bsp maps with the same texture name for different
	//  textures do not have any conflicts)
	struct skinframe_s *next; // next on hash chain
	char basename[MAX_QPATH]; // name of this
	int textureflags; // texture flags to use
	int comparewidth;
	int compareheight;
	int comparecrc;
	// mark and sweep garbage collection, this value is updated to a new value
	// on each level change for the used skinframes, if some are not used they
	// are freed
	int loadsequence;
	// indicates whether this texture has transparent pixels
	qboolean hasalpha;
	// average texture color, if applicable
	float avgcolor[4];
	// for mdl skins, we actually only upload on first use (many are never used, and they are almost never used in both base+pants+shirt and merged modes)
	unsigned char *qpixels;
	int qwidth;
	int qheight;
	qboolean qhascolormapping;
	qboolean qgeneratebase;
	qboolean qgeneratemerged;
	qboolean qgeneratenmap;
	qboolean qgenerateglow;
}
skinframe_t;

struct md3vertex_s;
struct trivertx_s;
typedef struct texvecvertex_s
{
	signed char svec[3];
	signed char tvec[3];
}
texvecvertex_t;

typedef struct blendweights_s
{
	unsigned char index[4];
	unsigned char influence[4];
}
blendweights_t;

typedef struct r_vertexgeneric_s
{
	// 36 bytes
	float vertex3f[3];
	float color4f[4];
	float texcoord2f[2];
}
r_vertexgeneric_t;

typedef struct r_vertexmesh_s
{
	// 80 bytes
	float vertex3f[3];
	float color4f[4];
	float texcoordtexture2f[2];
	float texcoordlightmap2f[2];
	float svector3f[3];
	float tvector3f[3];
	float normal3f[3];
}
r_vertexmesh_t;

typedef struct r_meshbuffer_s
{
	int bufferobject; // OpenGL
	void *devicebuffer; // Direct3D
	size_t size;
	qboolean isindexbuffer;
	qboolean isdynamic;
	qboolean isindex16;
	char name[MAX_QPATH];
}
r_meshbuffer_t;

// used for mesh lists in q1bsp/q3bsp map models
// (the surfaces reference portions of these meshes)
typedef struct surfmesh_s
{
	// triangle data in system memory
	int num_triangles; // number of triangles in the mesh
	int *data_element3i; // int[tris*3] triangles of the mesh, 3 indices into vertex arrays for each
	r_meshbuffer_t *data_element3i_indexbuffer;
	size_t data_element3i_bufferoffset;
	unsigned short *data_element3s; // unsigned short[tris*3] triangles of the mesh in unsigned short format (NULL if num_vertices > 65536)
	r_meshbuffer_t *data_element3s_indexbuffer;
	size_t data_element3s_bufferoffset;
	int *data_neighbor3i; // int[tris*3] neighboring triangle on each edge (-1 if none)
	// vertex data in system memory
	int num_vertices; // number of vertices in the mesh
	float *data_vertex3f; // float[verts*3] vertex locations
	float *data_svector3f; // float[verts*3] direction of 'S' (right) texture axis for each vertex
	float *data_tvector3f; // float[verts*3] direction of 'T' (down) texture axis for each vertex
	float *data_normal3f; // float[verts*3] direction of 'R' (out) texture axis for each vertex
	float *data_texcoordtexture2f; // float[verts*2] texcoords for surface texture
	float *data_texcoordlightmap2f; // float[verts*2] texcoords for lightmap texture
	float *data_lightmapcolor4f;
	int *data_lightmapoffsets; // index into surface's lightmap samples for vertex lighting
	// vertex buffer object (stores geometry in video memory)
	r_meshbuffer_t *vbo_vertexbuffer;
	size_t vbooffset_vertex3f;
	size_t vbooffset_svector3f;
	size_t vbooffset_tvector3f;
	size_t vbooffset_normal3f;
	size_t vbooffset_texcoordtexture2f;
	size_t vbooffset_texcoordlightmap2f;
	size_t vbooffset_lightmapcolor4f;
	// morph blending, these are zero if model is skeletal or static
	int num_morphframes;
	struct md3vertex_s *data_morphmd3vertex;
	struct trivertx_s *data_morphmdlvertex;
	struct texvecvertex_s *data_morphtexvecvertex;
	float *data_morphmd2framesize6f;
	float num_morphmdlframescale[3];
	float num_morphmdlframetranslate[3];
	// skeletal blending, these are NULL if model is morph or static
	struct blendweights_s *data_blendweights;
	int num_blends;
	unsigned short *blends;
	// set if there is some kind of animation on this model
	qboolean isanimated;

	// vertex and index buffers for rendering
	r_vertexmesh_t *vertexmesh;
	r_meshbuffer_t *vertex3fbuffer;
	r_meshbuffer_t *vertexmeshbuffer;
}
surfmesh_t;

#define SHADOWMESHVERTEXHASH 1024
typedef struct shadowmeshvertexhash_s
{
	struct shadowmeshvertexhash_s *next;
}
shadowmeshvertexhash_t;

typedef struct shadowmesh_s
{
	// next mesh in chain
	struct shadowmesh_s *next;
	// used for light mesh (NULL on shadow mesh)
	rtexture_t *map_diffuse;
	rtexture_t *map_specular;
	rtexture_t *map_normal;
	// buffer sizes
	int numverts, maxverts;
	int numtriangles, maxtriangles;
	// used always
	float *vertex3f;
	// used for light mesh (NULL on shadow mesh)
	float *svector3f;
	float *tvector3f;
	float *normal3f;
	float *texcoord2f;
	// used always
	int *element3i;
	r_meshbuffer_t *element3i_indexbuffer;
	size_t element3i_bufferoffset;
	unsigned short *element3s;
	r_meshbuffer_t *element3s_indexbuffer;
	size_t element3s_bufferoffset;
	// used for shadow mapping cubemap side partitioning
	int sideoffsets[6], sidetotals[6];
	// used for shadow mesh (NULL on light mesh)
	int *neighbor3i;
	// these are NULL after Mod_ShadowMesh_Finish is performed, only used
	// while building meshes
	shadowmeshvertexhash_t **vertexhashtable, *vertexhashentries;
	r_meshbuffer_t *vbo_vertexbuffer;
	size_t vbooffset_vertex3f;
	size_t vbooffset_svector3f;
	size_t vbooffset_tvector3f;
	size_t vbooffset_normal3f;
	size_t vbooffset_texcoord2f;
	// vertex/index buffers for rendering
	// (created by Mod_ShadowMesh_Finish if possible)
	r_vertexmesh_t *vertexmesh; // usually NULL
	r_meshbuffer_t *vertex3fbuffer;
	r_meshbuffer_t *vertexmeshbuffer; // usually NULL
}
shadowmesh_t;

// various flags from shaders, used for special effects not otherwise classified
// TODO: support these features more directly
#define Q3TEXTUREFLAG_TWOSIDED 1
#define Q3TEXTUREFLAG_NOPICMIP 16
#define Q3TEXTUREFLAG_POLYGONOFFSET 32
#define Q3TEXTUREFLAG_REFRACTION 256
#define Q3TEXTUREFLAG_REFLECTION 512
#define Q3TEXTUREFLAG_WATERSHADER 1024
#define Q3TEXTUREFLAG_CAMERA 2048

#define Q3PATHLENGTH 64
#define TEXTURE_MAXFRAMES 64
#define Q3WAVEPARMS 4
#define Q3DEFORM_MAXPARMS 3
#define Q3SHADER_MAXLAYERS 2 // FIXME support more than that (currently only two are used, so why keep more in RAM?)
#define Q3RGBGEN_MAXPARMS 3
#define Q3ALPHAGEN_MAXPARMS 1
#define Q3TCGEN_MAXPARMS 6
#define Q3TCMOD_MAXPARMS 6
#define Q3MAXTCMODS 8
#define Q3MAXDEFORMS 4

typedef enum q3wavefunc_e
{
	Q3WAVEFUNC_NONE,
	Q3WAVEFUNC_INVERSESAWTOOTH,
	Q3WAVEFUNC_NOISE,
	Q3WAVEFUNC_SAWTOOTH,
	Q3WAVEFUNC_SIN,
	Q3WAVEFUNC_SQUARE,
	Q3WAVEFUNC_TRIANGLE,
	Q3WAVEFUNC_COUNT
}
q3wavefunc_e;
typedef int q3wavefunc_t;
#define Q3WAVEFUNC_USER_COUNT 4
#define Q3WAVEFUNC_USER_SHIFT 8 // use 8 bits for wave func type

typedef enum q3deform_e
{
	Q3DEFORM_NONE,
	Q3DEFORM_PROJECTIONSHADOW,
	Q3DEFORM_AUTOSPRITE,
	Q3DEFORM_AUTOSPRITE2,
	Q3DEFORM_TEXT0,
	Q3DEFORM_TEXT1,
	Q3DEFORM_TEXT2,
	Q3DEFORM_TEXT3,
	Q3DEFORM_TEXT4,
	Q3DEFORM_TEXT5,
	Q3DEFORM_TEXT6,
	Q3DEFORM_TEXT7,
	Q3DEFORM_BULGE,
	Q3DEFORM_WAVE,
	Q3DEFORM_NORMAL,
	Q3DEFORM_MOVE,
	Q3DEFORM_COUNT
}
q3deform_t;

typedef enum q3rgbgen_e
{
	Q3RGBGEN_IDENTITY,
	Q3RGBGEN_CONST,
	Q3RGBGEN_ENTITY,
	Q3RGBGEN_EXACTVERTEX,
	Q3RGBGEN_IDENTITYLIGHTING,
	Q3RGBGEN_LIGHTINGDIFFUSE,
	Q3RGBGEN_ONEMINUSENTITY,
	Q3RGBGEN_ONEMINUSVERTEX,
	Q3RGBGEN_VERTEX,
	Q3RGBGEN_WAVE,
	Q3RGBGEN_COUNT
}
q3rgbgen_t;

typedef enum q3alphagen_e
{
	Q3ALPHAGEN_IDENTITY,
	Q3ALPHAGEN_CONST,
	Q3ALPHAGEN_ENTITY,
	Q3ALPHAGEN_LIGHTINGSPECULAR,
	Q3ALPHAGEN_ONEMINUSENTITY,
	Q3ALPHAGEN_ONEMINUSVERTEX,
	Q3ALPHAGEN_PORTAL,
	Q3ALPHAGEN_VERTEX,
	Q3ALPHAGEN_WAVE,
	Q3ALPHAGEN_COUNT
}
q3alphagen_t;

typedef enum q3tcgen_e
{
	Q3TCGEN_NONE,
	Q3TCGEN_TEXTURE, // very common
	Q3TCGEN_ENVIRONMENT, // common
	Q3TCGEN_LIGHTMAP,
	Q3TCGEN_VECTOR,
	Q3TCGEN_COUNT
}
q3tcgen_t;

typedef enum q3tcmod_e
{
	Q3TCMOD_NONE,
	Q3TCMOD_ENTITYTRANSLATE,
	Q3TCMOD_ROTATE,
	Q3TCMOD_SCALE,
	Q3TCMOD_SCROLL,
	Q3TCMOD_STRETCH,
	Q3TCMOD_TRANSFORM,
	Q3TCMOD_TURBULENT,
	Q3TCMOD_PAGE,
	Q3TCMOD_COUNT
}
q3tcmod_t;

typedef struct q3shaderinfo_layer_rgbgen_s
{
	q3rgbgen_t rgbgen;
	float parms[Q3RGBGEN_MAXPARMS];
	q3wavefunc_t wavefunc;
	float waveparms[Q3WAVEPARMS];
}
q3shaderinfo_layer_rgbgen_t;

typedef struct q3shaderinfo_layer_alphagen_s
{
	q3alphagen_t alphagen;
	float parms[Q3ALPHAGEN_MAXPARMS];
	q3wavefunc_t wavefunc;
	float waveparms[Q3WAVEPARMS];
}
q3shaderinfo_layer_alphagen_t;

typedef struct q3shaderinfo_layer_tcgen_s
{
	q3tcgen_t tcgen;
	float parms[Q3TCGEN_MAXPARMS];
}
q3shaderinfo_layer_tcgen_t;

typedef struct q3shaderinfo_layer_tcmod_s
{
	q3tcmod_t tcmod;
	float parms[Q3TCMOD_MAXPARMS];
	q3wavefunc_t wavefunc;
	float waveparms[Q3WAVEPARMS];
}
q3shaderinfo_layer_tcmod_t;

typedef struct q3shaderinfo_layer_s
{
	int alphatest;
	int clampmap;
	float framerate;
	int numframes;
	int texflags;
	char** texturename;
	int blendfunc[2];
	q3shaderinfo_layer_rgbgen_t rgbgen;
	q3shaderinfo_layer_alphagen_t alphagen;
	q3shaderinfo_layer_tcgen_t tcgen;
	q3shaderinfo_layer_tcmod_t tcmods[Q3MAXTCMODS];
}
q3shaderinfo_layer_t;

typedef struct q3shaderinfo_deform_s
{
	q3deform_t deform;
	float parms[Q3DEFORM_MAXPARMS];
	q3wavefunc_t wavefunc;
	float waveparms[Q3WAVEPARMS];
}
q3shaderinfo_deform_t;

typedef enum dpoffsetmapping_technique_s
{
	OFFSETMAPPING_OFF,			// none
	OFFSETMAPPING_DEFAULT,		// cvar-set
	OFFSETMAPPING_LINEAR,		// linear
	OFFSETMAPPING_RELIEF		// relief
}dpoffsetmapping_technique_t;


typedef struct q3shaderinfo_s
{
	char name[Q3PATHLENGTH];
#define Q3SHADERINFO_COMPARE_START surfaceparms
	int surfaceparms;
	int textureflags;
	int numlayers;
	qboolean lighting;
	qboolean vertexalpha;
	qboolean textureblendalpha;
	int primarylayer, backgroundlayer;
	q3shaderinfo_layer_t layers[Q3SHADER_MAXLAYERS];
	char skyboxname[Q3PATHLENGTH];
	q3shaderinfo_deform_t deforms[Q3MAXDEFORMS];

	// dp-specific additions:

	// shadow control
	qboolean dpnortlight;
	qboolean dpshadow;
	qboolean dpnoshadow;

	// add collisions to all triangles of the surface
	qboolean dpmeshcollisions;

	// fake reflection
	char dpreflectcube[Q3PATHLENGTH];

	// reflection
	float reflectmin; // when refraction is used, minimum amount of reflection (when looking straight down)
	float reflectmax; // when refraction is used, maximum amount of reflection (when looking parallel to water)
	float refractfactor; // amount of refraction distort (1.0 = like the cvar specifies)
	vec4_t refractcolor4f; // color tint of refraction (including alpha factor)
	float reflectfactor; // amount of reflection distort (1.0 = like the cvar specifies)
	vec4_t reflectcolor4f; // color tint of reflection (including alpha factor)
	float r_water_wateralpha; // additional wateralpha to apply when r_water is active
	float r_water_waterscroll[2]; // water normalmapscrollblend - scale and speed

	// offsetmapping
	dpoffsetmapping_technique_t offsetmapping;
	float offsetscale;

	// polygonoffset (only used if Q3TEXTUREFLAG_POLYGONOFFSET)
	float biaspolygonoffset, biaspolygonfactor;

	// gloss
	float specularscalemod;
	float specularpowermod;
#define Q3SHADERINFO_COMPARE_END specularpowermod
}
q3shaderinfo_t;

typedef enum texturelayertype_e
{
	TEXTURELAYERTYPE_INVALID,
	TEXTURELAYERTYPE_LITTEXTURE,
	TEXTURELAYERTYPE_TEXTURE,
	TEXTURELAYERTYPE_FOG
}
texturelayertype_t;

typedef struct texturelayer_s
{
	texturelayertype_t type;
	qboolean depthmask;
	int blendfunc1;
	int blendfunc2;
	rtexture_t *texture;
	matrix4x4_t texmatrix;
	vec4_t color;
}
texturelayer_t;

typedef struct texture_s
{
	// q1bsp
	// name
	//char name[16];
	// size
	unsigned int width, height;
	// SURF_ flags
	//unsigned int flags;

	// base material flags
	int basematerialflags;
	// current material flags (updated each bmodel render)
	int currentmaterialflags;

	// PolygonOffset values for rendering this material
	// (these are added to the r_refdef values and submodel values)
	float biaspolygonfactor;
	float biaspolygonoffset;

	// textures to use when rendering this material
	skinframe_t *currentskinframe;
	int numskinframes;
	float skinframerate;
	skinframe_t *skinframes[TEXTURE_MAXFRAMES];
	// background layer (for terrain texture blending)
	skinframe_t *backgroundcurrentskinframe;
	int backgroundnumskinframes;
	float backgroundskinframerate;
	skinframe_t *backgroundskinframes[TEXTURE_MAXFRAMES];

	// total frames in sequence and alternate sequence
	int anim_total[2];
	// direct pointers to each of the frames in the sequences
	// (indexed as [alternate][frame])
	struct texture_s *anim_frames[2][10];
	// set if animated or there is an alternate frame set
	// (this is an optimization in the renderer)
	int animated;

	// renderer checks if this texture needs updating...
	int update_lastrenderframe;
	void *update_lastrenderentity;
	// the current alpha of this texture (may be affected by r_wateralpha)
	float currentalpha;
	// the current texture frame in animation
	struct texture_s *currentframe;
	// current texture transform matrix (used for water scrolling)
	matrix4x4_t currenttexmatrix;
	matrix4x4_t currentbackgroundtexmatrix;

	// various q3 shader features
	q3shaderinfo_layer_rgbgen_t rgbgen;
	q3shaderinfo_layer_alphagen_t alphagen;
	q3shaderinfo_layer_tcgen_t tcgen;
	q3shaderinfo_layer_tcmod_t tcmods[Q3MAXTCMODS];
	q3shaderinfo_layer_tcmod_t backgroundtcmods[Q3MAXTCMODS];
	q3shaderinfo_deform_t deforms[Q3MAXDEFORMS];

	qboolean colormapping;
	rtexture_t *basetexture; // original texture without pants/shirt/glow
	rtexture_t *pantstexture; // pants only (in greyscale)
	rtexture_t *shirttexture; // shirt only (in greyscale)
	rtexture_t *nmaptexture; // normalmap (bumpmap for dot3)
	rtexture_t *glosstexture; // glossmap (for dot3)
	rtexture_t *glowtexture; // glow only (fullbrights)
	rtexture_t *fogtexture; // alpha of the base texture (if not opaque)
	rtexture_t *reflectmasktexture; // mask for fake reflections
	rtexture_t *reflectcubetexture; // fake reflections cubemap
	rtexture_t *backgroundbasetexture; // original texture without pants/shirt/glow
	rtexture_t *backgroundnmaptexture; // normalmap (bumpmap for dot3)
	rtexture_t *backgroundglosstexture; // glossmap (for dot3)
	rtexture_t *backgroundglowtexture; // glow only (fullbrights)
	float specularscale;
	float specularpower;
	// color tint (colormod * currentalpha) used for rtlighting this material
	float dlightcolor[3];
	// color tint (colormod * 2) used for lightmapped lighting on this material
	// includes alpha as 4th component
	// replaces role of gl_Color in GLSL shader
	float lightmapcolor[4];

	// from q3 shaders
	int customblendfunc[2];

	int currentnumlayers;
	texturelayer_t currentlayers[16];

	// q3bsp
	char name[64];
	int surfaceflags;
	int supercontents;
	int surfaceparms;
	int textureflags;

	// reflection
	float reflectmin; // when refraction is used, minimum amount of reflection (when looking straight down)
	float reflectmax; // when refraction is used, maximum amount of reflection (when looking parallel to water)
	float refractfactor; // amount of refraction distort (1.0 = like the cvar specifies)
	vec4_t refractcolor4f; // color tint of refraction (including alpha factor)
	float reflectfactor; // amount of reflection distort (1.0 = like the cvar specifies)
	vec4_t reflectcolor4f; // color tint of reflection (including alpha factor)
	float r_water_wateralpha; // additional wateralpha to apply when r_water is active
	float r_water_waterscroll[2]; // scale and speed
	int camera_entity; // entity number for use by cameras

	// offsetmapping
	dpoffsetmapping_technique_t offsetmapping;
	float offsetscale;

	// gloss
	float specularscalemod;
	float specularpowermod;
}
 texture_t;

typedef struct mtexinfo_s
{
	float vecs[2][4];
	texture_t *texture;
	int flags;
}
mtexinfo_t;

typedef struct msurface_lightmapinfo_s
{
	// texture mapping properties used by this surface
	mtexinfo_t *texinfo; // q1bsp
	// index into r_refdef.scene.lightstylevalue array, 255 means not used (black)
	unsigned char styles[MAXLIGHTMAPS]; // q1bsp
	// RGB lighting data [numstyles][height][width][3]
	unsigned char *samples; // q1bsp
	// RGB normalmap data [numstyles][height][width][3]
	unsigned char *nmapsamples; // q1bsp
	// stain to apply on lightmap (soot/dirt/blood/whatever)
	unsigned char *stainsamples; // q1bsp
	int texturemins[2]; // q1bsp
	int extents[2]; // q1bsp
	int lightmaporigin[2]; // q1bsp
}
msurface_lightmapinfo_t;

struct q3deffect_s;
typedef struct msurface_s
{
	// bounding box for onscreen checks
	vec3_t mins;
	vec3_t maxs;
	// the texture to use on the surface
	texture_t *texture;
	// the lightmap texture fragment to use on the rendering mesh
	rtexture_t *lightmaptexture;
	// the lighting direction texture fragment to use on the rendering mesh
	rtexture_t *deluxemaptexture;
	// lightmaptexture rebuild information not used in q3bsp
	msurface_lightmapinfo_t *lightmapinfo; // q1bsp
	// fog volume info in q3bsp
	struct q3deffect_s *effect; // q3bsp
	// mesh information for collisions (only used by q3bsp curves)
	int num_firstcollisiontriangle;
	int *deprecatedq3data_collisionelement3i; // q3bsp
	float *deprecatedq3data_collisionvertex3f; // q3bsp
	float *deprecatedq3data_collisionbbox6f; // collision optimization - contains combined bboxes of every data_collisionstride triangles
	float *deprecatedq3data_bbox6f; // collision optimization - contains combined bboxes of every data_collisionstride triangles

	// surfaces own ranges of vertices and triangles in the model->surfmesh
	int num_triangles; // number of triangles
	int num_firsttriangle; // first triangle
	int num_vertices; // number of vertices
	int num_firstvertex; // first vertex

	// shadow volume building information
	int num_firstshadowmeshtriangle; // index into model->brush.shadowmesh

	// mesh information for collisions (only used by q3bsp curves)
	int num_collisiontriangles; // q3bsp
	int num_collisionvertices; // q3bsp
	int deprecatedq3num_collisionbboxstride;
	int deprecatedq3num_bboxstride;
	// FIXME: collisionmarkframe should be kept in a separate array
	int deprecatedq3collisionmarkframe; // q3bsp // don't collide twice in one trace
}
msurface_t;

#include "matrixlib.h"
#include "bih.h"

#include "model_brush.h"
#include "model_sprite.h"
#include "model_alias.h"

typedef struct model_sprite_s
{
	int				sprnum_type;
	mspriteframe_t	*sprdata_frames;
}
model_sprite_t;

struct trace_s;

typedef struct model_brush_lightstyleinfo_s
{
	int style;
	int value;
	int numsurfaces;
	int *surfacelist;
}
model_brush_lightstyleinfo_t;

typedef struct model_brush_s
{
	// true if this model is a HalfLife .bsp file
	qboolean ishlbsp;
	// string of entity definitions (.map format)
	char *entities;

	// if not NULL this is a submodel
	struct model_s *parentmodel;
	// (this is the number of the submodel, an index into submodels)
	int submodel;

	// number of submodels in this map (just used by server to know how many
	// submodels to load)
	int numsubmodels;
	// pointers to each of the submodels
	struct model_s **submodels;

	int num_planes;
	mplane_t *data_planes;

	int num_nodes;
	mnode_t *data_nodes;

	// visible leafs, not counting 0 (solid)
	int num_visleafs;
	// number of actual leafs (including 0 which is solid)
	int num_leafs;
	mleaf_t *data_leafs;

	int num_leafbrushes;
	int *data_leafbrushes;

	int num_leafsurfaces;
	int *data_leafsurfaces;

	int num_portals;
	mportal_t *data_portals;

	int num_portalpoints;
	mvertex_t *data_portalpoints;

	int num_brushes;
	q3mbrush_t *data_brushes;

	int num_brushsides;
	q3mbrushside_t *data_brushsides;

	// pvs
	int num_pvsclusters;
	int num_pvsclusterbytes;
	unsigned char *data_pvsclusters;
	// example
	//pvschain = model->brush.data_pvsclusters + mycluster * model->brush.num_pvsclusterbytes;
	//if (pvschain[thatcluster >> 3] & (1 << (thatcluster & 7)))

	// collision geometry for q3 curves
	int num_collisionvertices;
	int num_collisiontriangles;
	float *data_collisionvertex3f;
	int *data_collisionelement3i;

	// a mesh containing all shadow casting geometry for the whole model (including submodels), portions of this are referenced by each surface's num_firstshadowmeshtriangle
	shadowmesh_t *shadowmesh;

	// a mesh containing all SUPERCONTENTS_SOLID surfaces for this model or submodel, for physics engines to use
	shadowmesh_t *collisionmesh;

	// common functions
	int (*SuperContentsFromNativeContents)(struct model_s *model, int nativecontents);
	int (*NativeContentsFromSuperContents)(struct model_s *model, int supercontents);
	unsigned char *(*GetPVS)(struct model_s *model, const vec3_t p);
	int (*FatPVS)(struct model_s *model, const vec3_t org, vec_t radius, unsigned char *pvsbuffer, int pvsbufferlength, qboolean merge);
	int (*BoxTouchingPVS)(struct model_s *model, const unsigned char *pvs, const vec3_t mins, const vec3_t maxs);
	int (*BoxTouchingLeafPVS)(struct model_s *model, const unsigned char *pvs, const vec3_t mins, const vec3_t maxs);
	int (*BoxTouchingVisibleLeafs)(struct model_s *model, const unsigned char *visibleleafs, const vec3_t mins, const vec3_t maxs);
	int (*FindBoxClusters)(struct model_s *model, const vec3_t mins, const vec3_t maxs, int maxclusters, int *clusterlist);
	void (*LightPoint)(struct model_s *model, const vec3_t p, vec3_t ambientcolor, vec3_t diffusecolor, vec3_t diffusenormal);
	void (*FindNonSolidLocation)(struct model_s *model, const vec3_t in, vec3_t out, vec_t radius);
	mleaf_t *(*PointInLeaf)(struct model_s *model, const float *p);
	// these are actually only found on brushq1, but NULL is handled gracefully
	void (*AmbientSoundLevelsForPoint)(struct model_s *model, const vec3_t p, unsigned char *out, int outsize);
	void (*RoundUpToHullSize)(struct model_s *cmodel, const vec3_t inmins, const vec3_t inmaxs, vec3_t outmins, vec3_t outmaxs);
	// trace a line of sight through this model (returns false if the line if sight is definitely blocked)
	qboolean (*TraceLineOfSight)(struct model_s *model, const vec3_t start, const vec3_t end);

	char skybox[MAX_QPATH];

	skinframe_t *solidskyskinframe;
	skinframe_t *alphaskyskinframe;

	qboolean supportwateralpha;

	// QuakeWorld
	int qw_md4sum;
	int qw_md4sum2;
}
model_brush_t;

typedef struct model_brushq1_s
{
	dmodel_t		*submodels;

	int				numvertexes;
	mvertex_t		*vertexes;

	int				numedges;
	medge_t			*edges;

	int				numtexinfo;
	mtexinfo_t		*texinfo;

	int				numsurfedges;
	int				*surfedges;

	int				numclipnodes;
	mclipnode_t		*clipnodes;

	hull_t			hulls[MAX_MAP_HULLS];

	int				num_compressedpvs;
	unsigned char			*data_compressedpvs;

	int				num_lightdata;
	unsigned char			*lightdata;
	unsigned char			*nmaplightdata; // deluxemap file

	// lightmap update chains for light styles
	int				num_lightstyles;
	model_brush_lightstyleinfo_t *data_lightstyleinfo;

	// this contains bytes that are 1 if a surface needs its lightmap rebuilt
	unsigned char *lightmapupdateflags;
	qboolean firstrender; // causes all surface lightmaps to be loaded in first frame
}
model_brushq1_t;

/* MSVC can't compile empty structs, so this is commented out for now
typedef struct model_brushq2_s
{
}
model_brushq2_t;
*/

typedef struct model_brushq3_s
{
	int num_models;
	q3dmodel_t *data_models;

	// used only during loading - freed after loading!
	int num_vertices;
	float *data_vertex3f;
	float *data_normal3f;
	float *data_texcoordtexture2f;
	float *data_texcoordlightmap2f;
	float *data_color4f;

	// freed after loading!
	int num_triangles;
	int *data_element3i;

	int num_effects;
	q3deffect_t *data_effects;

	// lightmap textures
	int num_originallightmaps;
	int num_mergedlightmaps;
	int num_lightmapmergedwidthpower;
	int num_lightmapmergedheightpower;
	int num_lightmapmergedwidthheightdeluxepower;
	int num_lightmapmerge;
	rtexture_t **data_lightmaps;
	rtexture_t **data_deluxemaps;

	// voxel light data with directional shading
	int num_lightgrid;
	q3dlightgrid_t *data_lightgrid;
	// size of each cell (may vary by map, typically 64 64 128)
	float num_lightgrid_cellsize[3];
	// 1.0 / num_lightgrid_cellsize
	float num_lightgrid_scale[3];
	// dimensions of the world model in lightgrid cells
	int num_lightgrid_imins[3];
	int num_lightgrid_imaxs[3];
	int num_lightgrid_isize[3];
	// transform modelspace coordinates to lightgrid index
	matrix4x4_t num_lightgrid_indexfromworld;

	// true if this q3bsp file has been detected as using deluxemapping
	// (lightmap texture pairs, every odd one is never directly refernced,
	//  and contains lighting normals, not colors)
	qboolean deluxemapping;
	// true if the detected deluxemaps are the modelspace kind, rather than
	// the faster tangentspace kind
	qboolean deluxemapping_modelspace;
	// size of lightmaps (128 by default, but may be another poweroftwo if
	// external lightmaps are used (q3map2 -lightmapsize)
	int lightmapsize;
}
model_brushq3_t;

struct frameblend_s;
struct skeleton_s;

typedef struct model_s
{
	// name and path of model, for example "progs/player.mdl"
	char			name[MAX_QPATH];
	// model needs to be loaded if this is false
	qboolean		loaded;
	// set if the model is used in current map, models which are not, are purged
	qboolean		used;
	// CRC of the file this model was loaded from, to reload if changed
	unsigned int	crc;
	// mod_brush, mod_alias, mod_sprite
	modtype_t		type;
	// memory pool for allocations
	mempool_t		*mempool;
	// all models use textures...
	rtexturepool_t	*texturepool;
	// EF_* flags (translated from the model file's different flags layout)
	int				effects;
	// number of QC accessible frame(group)s in the model
	int				numframes;
	// number of QC accessible skin(group)s in the model
	int				numskins;
	// whether to randomize animated framegroups
	synctype_t		synctype;
	// bounding box at angles '0 0 0'
	vec3_t			normalmins, normalmaxs;
	// bounding box if yaw angle is not 0, but pitch and roll are
	vec3_t			yawmins, yawmaxs;
	// bounding box if pitch or roll are used
	vec3_t			rotatedmins, rotatedmaxs;
	// sphere radius, usable at any angles
	float			radius;
	// squared sphere radius for easier comparisons
	float			radius2;
	// skin animation info
	animscene_t		*skinscenes; // [numskins]
	// skin animation info
	animscene_t		*animscenes; // [numframes]
	// range of surface numbers in this (sub)model
	int				firstmodelsurface;
	int				nummodelsurfaces;
	int				*sortedmodelsurfaces;
	// range of collision brush numbers in this (sub)model
	int				firstmodelbrush;
	int				nummodelbrushes;
	// BIH (Bounding Interval Hierarchy) for this (sub)model
	bih_t			collision_bih;
	bih_t			render_bih; // if not set, use collision_bih instead for rendering purposes too
	// for md3 models
	int				num_tags;
	int				num_tagframes;
	aliastag_t		*data_tags;
	// for skeletal models
	int				num_bones;
	aliasbone_t		*data_bones;
	float			num_posescale; // scaling factor from origin in poses6s format (includes divide by 32767)
	float			num_poseinvscale; // scaling factor to origin in poses6s format (includes multiply by 32767)
	int				num_poses;
	short			*data_poses6s; // origin xyz, quat xyz, w implied negative, unit length, values normalized to +/-32767 range
	float			*data_baseboneposeinverse;
	// textures of this model
	int				num_textures;
	int				num_texturesperskin;
	texture_t		*data_textures;
	qboolean		wantnormals;
	qboolean		wanttangents;
	// surfaces of this model
	int				num_surfaces;
	msurface_t		*data_surfaces;
	// optional lightmapinfo data for surface lightmap updates
	msurface_lightmapinfo_t *data_surfaces_lightmapinfo;
	// all surfaces belong to this mesh
	surfmesh_t		surfmesh;
	// data type of model
	const char		*modeldatatypestring;
	// generates vertex data for a given frameblend
	void(*AnimateVertices)(const struct model_s * RESTRICT model, const struct frameblend_s * RESTRICT frameblend, const struct skeleton_s *skeleton, float * RESTRICT vertex3f, float * RESTRICT normal3f, float * RESTRICT svector3f, float * RESTRICT tvector3f);
	// draw the model's sky polygons (only used by brush models)
	void(*DrawSky)(struct entity_render_s *ent);
	// draw refraction/reflection textures for the model's water polygons (only used by brush models)
	void(*DrawAddWaterPlanes)(struct entity_render_s *ent);
	// draw the model using lightmap/dlight shading
	void(*Draw)(struct entity_render_s *ent);
	// draw the model to the depth buffer (no color rendering at all)
	void(*DrawDepth)(struct entity_render_s *ent);
	// draw any enabled debugging effects on this model (such as showing triangles, normals, collision brushes...)
	void(*DrawDebug)(struct entity_render_s *ent);
	// draw geometry textures for deferred rendering
	void(*DrawPrepass)(struct entity_render_s *ent);
    // compile an optimized shadowmap mesh for the model based on light source
	void(*CompileShadowMap)(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist);
	// draw depth into a shadowmap
	void(*DrawShadowMap)(int side, struct entity_render_s *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist, const unsigned char *surfacesides, const vec3_t lightmins, const vec3_t lightmaxs);
	// gathers info on which clusters and surfaces are lit by light, as well as calculating a bounding box
	void(*GetLightInfo)(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outleaflist, unsigned char *outleafpvs, int *outnumleafspointer, int *outsurfacelist, unsigned char *outsurfacepvs, int *outnumsurfacespointer, unsigned char *outshadowtrispvs, unsigned char *outlighttrispvs, unsigned char *visitingleafpvs, int numfrustumplanes, const mplane_t *frustumplanes);
	// compile a shadow volume for the model based on light source
	void(*CompileShadowVolume)(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist);
	// draw a shadow volume for the model based on light source
	void(*DrawShadowVolume)(struct entity_render_s *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs);
	// draw the lighting on a model (through stencil)
	void(*DrawLight)(struct entity_render_s *ent, int numsurfaces, const int *surfacelist, const unsigned char *trispvs);
	// trace a box against this model
	void (*TraceBox)(struct model_s *model, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, struct trace_s *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask);
	void (*TraceBrush)(struct model_s *model, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, struct trace_s *trace, struct colbrushf_s *start, struct colbrushf_s *end, int hitsupercontentsmask);
	// trace a box against this model
	void (*TraceLine)(struct model_s *model, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, struct trace_s *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask);
	// trace a point against this model (like PointSuperContents)
	void (*TracePoint)(struct model_s *model, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, struct trace_s *trace, const vec3_t start, int hitsupercontentsmask);
	// find the supercontents value at a point in this model
	int (*PointSuperContents)(struct model_s *model, int frame, const vec3_t point);
	// trace a line against geometry in this model and report correct texture (used by r_shadow_bouncegrid)
	void (*TraceLineAgainstSurfaces)(struct model_s *model, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, struct trace_s *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask);
	// fields belonging to some types of model
	model_sprite_t	sprite;
	model_brush_t	brush;
	model_brushq1_t	brushq1;
	/* MSVC can't handle an empty struct, so this is commented out for now
	model_brushq2_t	brushq2;
	*/
	model_brushq3_t	brushq3;
	// flags this model for offseting sounds to the model center (used by brush models)
	int soundfromcenter;

	// if set, the model contains light information (lightmap, or vertexlight)
	qboolean lit;
}
dp_model_t;

//============================================================================

// model loading
extern dp_model_t *loadmodel;
extern unsigned char *mod_base;
// sky/water subdivision
//extern cvar_t gl_subdivide_size;
// texture fullbrights
extern cvar_t r_fullbrights;
extern cvar_t r_enableshadowvolumes;

void Mod_Init (void);
void Mod_Reload (void);
dp_model_t *Mod_LoadModel(dp_model_t *mod, qboolean crash, qboolean checkdisk);
dp_model_t *Mod_FindName (const char *name, const char *parentname);
dp_model_t *Mod_ForName (const char *name, qboolean crash, qboolean checkdisk, const char *parentname);
void Mod_UnloadModel (dp_model_t *mod);

void Mod_ClearUsed(void);
void Mod_PurgeUnused(void);
void Mod_RemoveStaleWorldModels(dp_model_t *skip); // only used during loading!

extern dp_model_t *loadmodel;
extern char loadname[32];	// for hunk tags

int Mod_BuildVertexRemapTableFromElements(int numelements, const int *elements, int numvertices, int *remapvertices);
void Mod_BuildTriangleNeighbors(int *neighbors, const int *elements, int numtriangles);
void Mod_ValidateElements(int *elements, int numtriangles, int firstvertex, int numverts, const char *filename, int fileline);
void Mod_BuildNormals(int firstvertex, int numvertices, int numtriangles, const float *vertex3f, const int *elements, float *normal3f, qboolean areaweighting);
void Mod_BuildTextureVectorsFromNormals(int firstvertex, int numvertices, int numtriangles, const float *vertex3f, const float *texcoord2f, const float *normal3f, const int *elements, float *svector3f, float *tvector3f, qboolean areaweighting);

void Mod_AllocSurfMesh(mempool_t *mempool, int numvertices, int numtriangles, qboolean lightmapoffsets, qboolean vertexcolors, qboolean neighbors);
void Mod_MakeSortedSurfaces(dp_model_t *mod);

// called specially by brush model loaders before generating submodels
// automatically called after model loader returns
void Mod_BuildVBOs(void);

shadowmesh_t *Mod_ShadowMesh_Alloc(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable);
shadowmesh_t *Mod_ShadowMesh_ReAlloc(mempool_t *mempool, shadowmesh_t *oldmesh, int light, int neighbors);
int Mod_ShadowMesh_AddVertex(shadowmesh_t *mesh, float *vertex14f);
void Mod_ShadowMesh_AddTriangle(mempool_t *mempool, shadowmesh_t *mesh, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, float *vertex14f);
void Mod_ShadowMesh_AddMesh(mempool_t *mempool, shadowmesh_t *mesh, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, const float *vertex3f, const float *svector3f, const float *tvector3f, const float *normal3f, const float *texcoord2f, int numtris, const int *element3i);
shadowmesh_t *Mod_ShadowMesh_Begin(mempool_t *mempool, int maxverts, int maxtriangles, rtexture_t *map_diffuse, rtexture_t *map_specular, rtexture_t *map_normal, int light, int neighbors, int expandable);
shadowmesh_t *Mod_ShadowMesh_Finish(mempool_t *mempool, shadowmesh_t *firstmesh, qboolean light, qboolean neighbors, qboolean createvbo);
void Mod_ShadowMesh_CalcBBox(shadowmesh_t *firstmesh, vec3_t mins, vec3_t maxs, vec3_t center, float *radius);
void Mod_ShadowMesh_Free(shadowmesh_t *mesh);

void Mod_CreateCollisionMesh(dp_model_t *mod);

void Mod_FreeQ3Shaders(void);
void Mod_LoadQ3Shaders(void);
q3shaderinfo_t *Mod_LookupQ3Shader(const char *name);
qboolean Mod_LoadTextureFromQ3Shader(texture_t *texture, const char *name, qboolean warnmissing, qboolean fallback, int defaulttexflags);

extern cvar_t r_mipskins;
extern cvar_t r_mipnormalmaps;

typedef struct skinfileitem_s
{
	struct skinfileitem_s *next;
	char name[MAX_QPATH];
	char replacement[MAX_QPATH];
}
skinfileitem_t;

typedef struct skinfile_s
{
	struct skinfile_s *next;
	skinfileitem_t *items;
}
skinfile_t;

skinfile_t *Mod_LoadSkinFiles(void);
void Mod_FreeSkinFiles(skinfile_t *skinfile);
int Mod_CountSkinFiles(skinfile_t *skinfile);
void Mod_BuildAliasSkinsFromSkinFiles(texture_t *skin, skinfile_t *skinfile, const char *meshname, const char *shadername);

void Mod_SnapVertices(int numcomponents, int numvertices, float *vertices, float snap);
int Mod_RemoveDegenerateTriangles(int numtriangles, const int *inelement3i, int *outelement3i, const float *vertex3f);
void Mod_VertexRangeFromElements(int numelements, const int *elements, int *firstvertexpointer, int *lastvertexpointer);

typedef struct mod_alloclightmap_row_s
{
	int rowY;
	int currentX;
}
mod_alloclightmap_row_t;

typedef struct mod_alloclightmap_state_s
{
	int width;
	int height;
	int currentY;
	mod_alloclightmap_row_t *rows;
}
mod_alloclightmap_state_t;

void Mod_AllocLightmap_Init(mod_alloclightmap_state_t *state, int width, int height);
void Mod_AllocLightmap_Free(mod_alloclightmap_state_t *state);
void Mod_AllocLightmap_Reset(mod_alloclightmap_state_t *state);
qboolean Mod_AllocLightmap_Block(mod_alloclightmap_state_t *state, int blockwidth, int blockheight, int *outx, int *outy);

// bsp models
void Mod_BrushInit(void);
// used for talking to the QuakeC mainly
int Mod_Q1BSP_NativeContentsFromSuperContents(struct model_s *model, int supercontents);
int Mod_Q1BSP_SuperContentsFromNativeContents(struct model_s *model, int nativecontents);

// a lot of model formats use the Q1BSP code, so here are the prototypes...
struct entity_render_s;
void R_Q1BSP_DrawAddWaterPlanes(struct entity_render_s *ent);
void R_Q1BSP_DrawSky(struct entity_render_s *ent);
void R_Q1BSP_Draw(struct entity_render_s *ent);
void R_Q1BSP_DrawDepth(struct entity_render_s *ent);
void R_Q1BSP_DrawDebug(struct entity_render_s *ent);
void R_Q1BSP_DrawPrepass(struct entity_render_s *ent);
void R_Q1BSP_GetLightInfo(struct entity_render_s *ent, vec3_t relativelightorigin, float lightradius, vec3_t outmins, vec3_t outmaxs, int *outleaflist, unsigned char *outleafpvs, int *outnumleafspointer, int *outsurfacelist, unsigned char *outsurfacepvs, int *outnumsurfacespointer, unsigned char *outshadowtrispvs, unsigned char *outlighttrispvs, unsigned char *visitingleafpvs, int numfrustumplanes, const mplane_t *frustumplanes);
void R_Q1BSP_CompileShadowMap(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist);
void R_Q1BSP_DrawShadowMap(int side, struct entity_render_s *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int modelnumsurfaces, const int *modelsurfacelist, const unsigned char *surfacesides, const vec3_t lightmins, const vec3_t lightmaxs);
void R_Q1BSP_CompileShadowVolume(struct entity_render_s *ent, vec3_t relativelightorigin, vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist);
void R_Q1BSP_DrawShadowVolume(struct entity_render_s *ent, const vec3_t relativelightorigin, const vec3_t relativelightdirection, float lightradius, int numsurfaces, const int *surfacelist, const vec3_t lightmins, const vec3_t lightmaxs);
void R_Q1BSP_DrawLight(struct entity_render_s *ent, int numsurfaces, const int *surfacelist, const unsigned char *trispvs);

// Collision optimization using Bounding Interval Hierarchy
void Mod_CollisionBIH_TracePoint(dp_model_t *model, const struct frameblend_s *frameblend, const skeleton_t *skeleton, struct trace_s *trace, const vec3_t start, int hitsupercontentsmask);
void Mod_CollisionBIH_TraceLine(dp_model_t *model, const struct frameblend_s *frameblend, const skeleton_t *skeleton, struct trace_s *trace, const vec3_t start, const vec3_t end, int hitsupercontentsmask);
void Mod_CollisionBIH_TraceBox(dp_model_t *model, const struct frameblend_s *frameblend, const skeleton_t *skeleton, struct trace_s *trace, const vec3_t start, const vec3_t boxmins, const vec3_t boxmaxs, const vec3_t end, int hitsupercontentsmask);
void Mod_CollisionBIH_TraceBrush(dp_model_t *model, const struct frameblend_s *frameblend, const skeleton_t *skeleton, struct trace_s *trace, struct colbrushf_s *start, struct colbrushf_s *end, int hitsupercontentsmask);
void Mod_CollisionBIH_TracePoint_Mesh(dp_model_t *model, const struct frameblend_s *frameblend, const skeleton_t *skeleton, struct trace_s *trace, const vec3_t start, int hitsupercontentsmask);
int Mod_CollisionBIH_PointSuperContents_Mesh(struct model_s *model, int frame, const vec3_t point);
bih_t *Mod_MakeCollisionBIH(dp_model_t *model, qboolean userendersurfaces, bih_t *out);

// alias models
struct frameblend_s;
struct skeleton_s;
void Mod_AliasInit(void);
int Mod_Alias_GetTagMatrix(const dp_model_t *model, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, int tagindex, matrix4x4_t *outmatrix);
int Mod_Alias_GetTagIndexForName(const dp_model_t *model, unsigned int skin, const char *tagname);
int Mod_Alias_GetExtendedTagInfoForIndex(const dp_model_t *model, unsigned int skin, const struct frameblend_s *frameblend, const struct skeleton_s *skeleton, int tagindex, int *parentindex, const char **tagname, matrix4x4_t *tag_localmatrix);

void Mod_Skeletal_FreeBuffers(void);

// sprite models
void Mod_SpriteInit(void);

// loaders
void Mod_Q1BSP_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_IBSP_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_MAP_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_OBJ_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_IDP0_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_IDP2_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_IDP3_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_ZYMOTICMODEL_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_DARKPLACESMODEL_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_PSKMODEL_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_IDSP_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_IDS2_Load(dp_model_t *mod, void *buffer, void *bufferend);
void Mod_INTERQUAKEMODEL_Load(dp_model_t *mod, void *buffer, void *bufferend);

#endif	// MODEL_SHARED_H

